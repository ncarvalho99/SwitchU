#!/usr/bin/env python3
"""Turn a video into the frame sequence a SwitchU theme animates.

Frames are held decoded on the GPU for as long as the theme is applied, so what
limits an animation is bytes, not disk. At four bytes a pixel a ten second loop
only affords about 224x126 a frame, and stretched to the screen that reads as
blocks. BC1 keeps a 4x4 block in 8 bytes -- half a byte a pixel -- so the same
allowance holds 640x360 and the stretch drops from 5.7x to 2x.

    python tools/encode_theme_frames.py clip.mp4 "romfs/themes/My Theme" \
        --start 3.25 --duration 10 --fps 12 --width 640

Needs ffmpeg on PATH, plus numpy and Pillow.
"""

import argparse
import glob
import json
import os
import shutil
import struct
import subprocess
import sys
import tempfile

import numpy as np
from PIL import Image

# What the frames together may cost. Mirrors kFrameMemoryBudget in
# WaraWaraBackground.cpp; if that moves, this has to move with it, or the tool
# blesses a sequence the console then samples down.
#
# There is a second ceiling on the console that is not about bytes: each frame
# is its own GPU memory block, and 600 of them crashed the menu mid-load. The
# runtime caps the count at 320 -- a sequence longer than that plays sampled.
MAX_FRAMES = 320
FRAME_BUDGET_MB = 150.0


def gpu_bytes(w, h):
    """What a BC1 frame occupies once the GPU has it -- not w*h/2.

    Images are stored tiled, so a row of blocks is padded out to 64 bytes and
    the rows themselves to 128. At 640x360 that is 163840 bytes against a
    nominal 115200: a 42% surcharge, and budgeting without it is how a build
    shipped a theme whose loop was silently cut from 120 frames to 89, jumping
    mid-motion every time it came round.

    Derived from the device: a console reported exactly 89 frames fitting in
    14 MB, which brackets the true cost between 163112 and 164944 bytes.
    """
    row = -(-(w // 4 * 8) // 64) * 64
    rows = -(-(h // 4) // 128) * 128
    return row * rows


def to_565(c):
    """Both halves of the round trip: the stored 16 bits, and the colour a
    decoder gets back from them.

    The two have to be derived together. Rounding to a multiple of 8 and then
    shifting looks equivalent and is not -- 255 rounds to 256, which overflows
    the five bit field and lands in the next channel. Nor does a decoder pad
    with zeroes: it replicates the high bits, so 31 comes back as 255 and not
    248. Fitting endpoints against the wrong values quietly costs accuracy.
    """
    c = np.clip(c, 0, 255)
    r5 = np.round(c[..., 0] * 31 / 255).astype(np.uint16)
    g6 = np.round(c[..., 1] * 63 / 255).astype(np.uint16)
    b5 = np.round(c[..., 2] * 31 / 255).astype(np.uint16)
    packed = (r5 << 11) | (g6 << 5) | b5
    decoded = np.stack([(r5 << 3) | (r5 >> 2),
                        (g6 << 2) | (g6 >> 4),
                        (b5 << 3) | (b5 >> 2)], -1).astype(np.float32)
    return packed, decoded


# Bayer 4x4, do tamanho exato de um bloco BC1.
BAYER = np.array([[0, 8, 2, 10],
                  [12, 4, 14, 6],
                  [3, 11, 1, 9],
                  [15, 7, 13, 5]], dtype=np.float32)
BAYER = (BAYER + 0.5) / 16.0 - 0.5     # -0.5 .. +0.5


def encode_bc1(img, refine=1, dither=0.5):
    """Encode to BC1 blocks.

    Duas coisas alem de escolher o eixo principal, porque o BC1 da quatro cores
    a cada bloco de 4x4 e um gradiente suave nao cabe em quatro:

    - Os extremos do eixo sao sensiveis a outlier: um pixel claro estica a reta
      e os outros quinze passam a ser quantizados grosso. Com os indices ja
      escolhidos, o par de extremos que minimiza o erro sai de um sistema 2x2, e
      o resultado realimenta a escolha dos indices.
    - O arredondamento do indice recebe um deslocamento em padrao Bayer, o que
      troca degrau por ruido fino. O olho perdoa ruido e nao perdoa degrau.

    Medido num quadro de "Miles Morales Purple Neon", que e o pior caso do
    catalogo -- neon suave sobre preto:

        extremos do eixo        34.46 dB   49.5% dos blocos com <=2 cores
        refit 1x + dither 0.5   35.51 dB   46.0%
        refit 2x + dither 0.5   35.76 dB   46.5%

    Uma iteracao, nao duas: a segunda rende 0.25 dB e custa 35% mais tempo, o
    que num catalogo de 14 mil quadros e quase uma hora por um quarto de dB.

    O dither piora o PSNR de proposito: ele troca erro concentrado por erro
    espalhado, e e o concentrado que se ve como quadrado.
    """
    a = np.asarray(img.convert('RGB'), dtype=np.float32)
    h, w, _ = a.shape
    if h % 4 or w % 4:
        raise ValueError('%dx%d is not a multiple of 4' % (w, h))

    blocks = (a.reshape(h // 4, 4, w // 4, 4, 3)
               .transpose(0, 2, 1, 3, 4)
               .reshape(-1, 16, 3))

    mean = blocks.mean(1, keepdims=True)
    centred = blocks - mean
    axis = np.linalg.svd(centred, full_matrices=False)[2][:, 0, :][:, None, :]
    proj = (centred * axis).sum(-1)
    hi = mean[:, 0] + axis[:, 0] * proj.max(1, keepdims=True)
    lo = mean[:, 0] + axis[:, 0] * proj.min(1, keepdims=True)

    W = np.array([1.0, 0.0, 2 / 3, 1 / 3], dtype=np.float32)   # peso de hi por indice
    for _ in range(max(0, refine)):
        _, dhi = to_565(hi)
        _, dlo = to_565(lo)
        palette = np.stack([dhi, dlo, (2 * dhi + dlo) / 3, (dhi + 2 * dlo) / 3], 1)
        idx = ((blocks[:, None] - palette[:, :, None]) ** 2).sum(-1).argmin(1)

        wgt = W[idx]
        one = 1.0 - wgt
        A = (wgt * wgt).sum(1)
        B = (wgt * one).sum(1)
        C = (one * one).sum(1)
        X = (wgt[..., None] * blocks).sum(1)
        Y = (one[..., None] * blocks).sum(1)
        det = A * C - B * B
        ok = np.abs(det) > 1e-6
        safe = np.where(ok, det, 1.0)[:, None]
        hi = np.clip(np.where(ok[:, None], (C[:, None] * X - B[:, None] * Y) / safe, hi), 0, 255)
        lo = np.clip(np.where(ok[:, None], (A[:, None] * Y - B[:, None] * X) / safe, lo), 0, 255)

    p_hi, dhi = to_565(hi)
    p_lo, dlo = to_565(lo)

    # BC1 reads the four-colour palette only when colour0 sorts above colour1;
    # the other order means "three colours and a transparent slot", which would
    # punch holes in an opaque wallpaper.
    swap = p_hi < p_lo
    dhi, dlo = np.where(swap[:, None], dlo, dhi), np.where(swap[:, None], dhi, dlo)
    p_hi, p_lo = np.where(swap, p_lo, p_hi), np.where(swap, p_hi, p_lo)

    d = dhi - dlo
    denom = (d * d).sum(-1, keepdims=True)
    denom[denom == 0] = 1.0
    t = ((blocks - dlo[:, None]) * d[:, None]).sum(-1) / denom[:, 0][:, None]
    level = np.clip(t, 0.0, 1.0) * 3.0
    if dither > 0:
        offs = np.tile(BAYER.reshape(1, 16), (len(blocks), 1)) * dither
        # Bloco de uma cor so nao tem reta: mexer nele so inventa ruido.
        offs[p_hi == p_lo] = 0.0
        level = level + offs
    q = np.clip(np.rint(level), 0, 3).astype(np.int64)

    # Ordem dos indices do BC1: 0=hi, 1=lo, 2=(2hi+lo)/3, 3=(hi+2lo)/3.
    idx = np.array([1, 3, 2, 0], dtype=np.int64)[q]
    idx[p_hi == p_lo] = 0

    idx = idx.reshape(-1, 4, 4)
    rows = (idx[..., 0] | (idx[..., 1] << 2) | (idx[..., 2] << 4) | (idx[..., 3] << 6))
    packed = (rows[:, 0] | (rows[:, 1] << 8) | (rows[:, 2] << 16) | (rows[:, 3] << 24))

    out = np.empty(len(blocks), dtype=[('c0', '<u2'), ('c1', '<u2'), ('i', '<u4')])
    out['c0'], out['c1'], out['i'] = p_hi, p_lo, packed.astype(np.uint32)
    return out.tobytes()

def dds_header(w, h, payload):
    """The 128 byte DDS header for an uncompressed-mipmap DXT1 surface."""
    DDSD = 0x1 | 0x2 | 0x4 | 0x1000 | 0x80000   # caps|height|width|pixelformat|linearsize
    pf = struct.pack('<2I4s5I', 32, 0x4, b'DXT1', 0, 0, 0, 0, 0)
    return (b'DDS ' +
            struct.pack('<7I', 124, DDSD, h, w, len(payload), 0, 0) +
            b'\0' * 44 + pf +
            struct.pack('<5I', 0x1000, 0, 0, 0, 0))


def extract(video, start, duration, fps, width, height, workdir):
    cmd = ['ffmpeg', '-y', '-loglevel', 'error']
    if start:
        cmd += ['-ss', str(start)]
    if duration:
        cmd += ['-t', str(duration)]
    cmd += ['-i', video,
            '-vf', 'fps=%g,scale=%d:%d:flags=lanczos' % (fps, width, height),
            '-c:v', 'png', os.path.join(workdir, 'f%04d.png')]
    subprocess.run(cmd, check=True)
    return sorted(glob.glob(os.path.join(workdir, '*.png')))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('video')
    ap.add_argument('theme_dir', help='the theme folder holding theme.json')
    ap.add_argument('--start', type=float, default=0.0)
    ap.add_argument('--duration', type=float, default=0.0, help='0 uses the whole clip')
    ap.add_argument('--fps', type=float, default=12.0)
    ap.add_argument('--width', type=int, default=640)
    ap.add_argument('--height', type=int, default=0, help='0 keeps 16:9 from --width')
    args = ap.parse_args()

    height = args.height or (args.width * 9 // 16)
    if args.width % 4 or height % 4:
        sys.exit('BC1 works in 4x4 blocks, so %dx%d has to round to a multiple of 4'
                 % (args.width, height))

    out_dir = os.path.join(args.theme_dir, 'media', 'backgrounds')
    theme_json = os.path.join(args.theme_dir, 'theme.json')
    if not os.path.isfile(theme_json):
        sys.exit('no theme.json in %s' % args.theme_dir)

    work = tempfile.mkdtemp(prefix='switchu-frames-')
    try:
        frames = extract(args.video, args.start, args.duration, args.fps,
                         args.width, height, work)
        if not frames:
            sys.exit('ffmpeg produced no frames')

        total = gpu_bytes(args.width, height) * len(frames) / 1048576.0
        print('%d frames  %dx%d  %.2f MB on the GPU (budget %.2f, %.2f MB on disk before tiling)'
              % (len(frames), args.width, height, total, FRAME_BUDGET_MB,
                 args.width * height / 2 * len(frames) / 1048576.0))
        if total > FRAME_BUDGET_MB:
            # Refused rather than trimmed. A sequence cut short still plays, and
            # loops from wherever it was cut -- which looks like a glitch in the
            # video rather than a theme that did not fit.
            sys.exit('over budget: shorten the loop, drop the fps, or narrow the frame')

        # Rebuilt from scratch: leftovers from an earlier encode would be listed
        # by the glob below and animate as frames from another clip.
        if os.path.isdir(out_dir):
            shutil.rmtree(out_dir)
        os.makedirs(out_dir)

        names = []
        for i, png in enumerate(frames):
            payload = encode_bc1(Image.open(png))
            name = 'f%04d.dds' % i
            with open(os.path.join(out_dir, name), 'wb') as fh:
                fh.write(dds_header(args.width, height, payload))
                fh.write(payload)
            names.append(name)

        with open(theme_json, encoding='utf-8') as fh:
            theme = json.load(fh)
        image = theme['theme']['background'].setdefault('image', {})
        image['frames'] = ['media/backgrounds/' + n for n in names]
        image['fps'] = args.fps
        with open(theme_json, 'w', encoding='utf-8', newline='\n') as fh:
            json.dump(theme, fh, ensure_ascii=False, indent=2)
            fh.write('\n')

        print('wrote %d dds frames and updated %s' % (len(names), theme_json))
    finally:
        shutil.rmtree(work, ignore_errors=True)


if __name__ == '__main__':
    main()
