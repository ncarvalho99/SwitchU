#!/usr/bin/env python3
"""Build BC1 animated-theme packages for SwitchU.

Drop-in replacement for build_bc7_theme_catalog.py.  The only difference is the
encoder: BC1 (DXT1) at 4 bpp instead of BC7 at 8 bpp.  GPU memory per frame
drops from 456 KB to 232 KB at 912x512, halving the total footprint with no
visible quality difference on the Switch screen (confirmed by user testing on
18-Aug-2026).

Everything else — manifest handling, video extraction, frame blending at loop
boundary, screenshot generation — is identical.
"""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unicodedata
import zipfile

from PIL import Image

from encode_theme_frames import encode_bc1, dds_header

WIDTH, HEIGHT, FPS = 912, 512, 30
VIDEO_EXTENSIONS = {'.mp4', '.mov', '.webm', '.mkv'}
MANUAL_IDS = {
    'abstract-blue-motion-background-loop': 'blue-motion',
    'abstract-festive-looping-background': 'festive-lights',
    'color-neon-gradient-moving-abstract-blurred-background': 'neon-gradient',
    'glowing-blue-neon-lights-on-dark-background': 'blue-neon-glow',
    'golden-light-curtain-sparkle-animation-on-black': 'golden-curtain',
    'minimalist-background-with-an-elegant-flowing-blue-digital': 'digital-flow',
    'purple-themed-particle-form-futuristic-neon-graphic': 'purple-particles',
    'white-particles-strings-fading-over-blue-background': 'white-strings',
    'zelda-ultrahand-tears-of-the-kingdom-moewalls-com': 'zelda-ultrahand',
    'miles-morales-falling-upside-down-spiderman-into-the-spiderverse-moewalls-com':
        'miles-morales-falling-upside-down-spiderman-into-the-spidervers',
}


def video_slug(path):
    name = os.path.splitext(os.path.basename(path))[0]
    name = re.sub(r'^vecteezy[_-]+', '', name, flags=re.I)
    name = unicodedata.normalize('NFKD', name).encode('ascii', 'ignore').decode()
    name = re.sub(r'[^a-zA-Z0-9]+', '-', name).strip('-').lower()
    return re.sub(r'-\d{4,}$', '', name)


def theme_id(path):
    stem = video_slug(path)
    return MANUAL_IDS.get(stem, re.sub(r'-moewalls-com$', '', stem))


def display_name(ident):
    """Turn a safe catalogue id into the initial human-readable theme name."""
    return ' '.join(part.upper() if len(part) <= 3 else part.capitalize()
                    for part in ident.replace('_', '-').split('-'))


def gpu_bytes_per_frame():
    # BC1: 8 bytes per 4x4 block, with row tile=64 B, block-row tile=128.
    row = ((WIDTH // 4 * 8 + 63) // 64) * 64
    rows = ((HEIGHT // 4 + 127) // 128) * 128
    return row * rows


def discover_videos(directories):
    found = {}
    for directory in directories:
        if not os.path.isdir(directory):
            continue
        for name in sorted(os.listdir(directory)):
            path = os.path.join(directory, name)
            if os.path.splitext(name)[1].lower() in VIDEO_EXTENSIONS:
                found.setdefault(theme_id(path), path)
    return found


def frame_names(package):
    with zipfile.ZipFile(package) as archive:
        names = sorted(name for name in archive.namelist() if name.lower().endswith('.dds'))
        if not names:
            raise RuntimeError('package has no DDS frames')
        return names


def write_package(source_package, video, output_package, ident=None):
    names = frame_names(source_package)
    count = len(names)
    estimated = count * gpu_bytes_per_frame() / 1048576
    if estimated > 150:
        raise RuntimeError('%.1f MB GPU estimate exceeds the 150 MB safety limit' % estimated)

    workspace = tempfile.mkdtemp(prefix='switchu-bc1-')
    try:
        png_pattern = os.path.join(workspace, 'f%05d.png')
        subprocess.run([
            'ffmpeg', '-y', '-loglevel', 'error', '-i', video,
            '-vf', 'fps=%d,scale=%d:%d:flags=lanczos' % (FPS, WIDTH, HEIGHT),
            '-frames:v', str(count + FPS), '-c:v', 'png', png_pattern,
        ], check=True)
        pngs = sorted(os.path.join(workspace, name) for name in os.listdir(workspace)
                      if name.endswith('.png'))
        if len(pngs) < count:
            raise RuntimeError('video produced %d frames, package needs %d' % (len(pngs), count))

        blend = min(FPS, max(1, len(pngs) // 8))
        os.makedirs(os.path.dirname(output_package), exist_ok=True)
        temp_output = output_package + '.part'
        with zipfile.ZipFile(source_package) as source, \
             zipfile.ZipFile(temp_output, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as output:
            frame_set = set(names)
            manifest_names = {name for name in source.namelist()
                              if name.lower().endswith('/theme.json') or name == 'theme.json'}
            screenshot_names = {name for name in source.namelist()
                                if name.lower() == 'media/screenshots/00.jpg'}
            for name in source.namelist():
                if (name not in frame_set and name not in manifest_names and
                        name not in screenshot_names and not name.endswith('/')):
                    output.writestr(name, source.read(name))

            if ident:
                manifest_name = next(iter(manifest_names), 'theme.json')
                manifest = json.loads(source.read(manifest_name).decode('utf-8'))
                manifest.update({
                    'id': ident,
                    'name': display_name(ident),
                    'author': 'ncarvalho99',
                    'version': '1.0.0',
                    'license': 'origem nao declarada -- distribuicao restrita',
                    'source': 'arquivo fornecido pela usuaria: ' + os.path.basename(video),
                    'preview': {'screenshots': ['media/screenshots/00.jpg']},
                })
                output.writestr(manifest_name, json.dumps(manifest, ensure_ascii=False,
                                                          indent=2) + '\n')
                Image.open(pngs[0]).convert('RGB').save(
                    os.path.join(workspace, 'preview.jpg'), quality=88, optimize=True)
                output.write(os.path.join(workspace, 'preview.jpg'), 'media/screenshots/00.jpg')
            else:
                for name in manifest_names | screenshot_names:
                    output.writestr(name, source.read(name))
            for index, name in enumerate(names):
                image = Image.open(pngs[index]).convert('RGB')
                if index < blend and count + index < len(pngs):
                    future = Image.open(pngs[count + index]).convert('RGB')
                    image = Image.blend(image, future, (blend - index) / float(blend))
                payload = encode_bc1(image)
                output.writestr(name, dds_header(WIDTH, HEIGHT, payload) + payload)
                if (index + 1) % 25 == 0 or index + 1 == count:
                    print('    %d/%d' % (index + 1, count), flush=True)
        os.replace(temp_output, output_package)
    finally:
        shutil.rmtree(workspace, ignore_errors=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--packages', nargs='+', required=True, help='directories containing current theme ZIPs')
    parser.add_argument('--videos', nargs='+', required=True, help='directories containing source videos')
    parser.add_argument('--template', help='existing package used for videos without a matching package id')
    parser.add_argument('--output', default='artifacts/themes-bc1')
    parser.add_argument('--only', help='build one theme id')
    parser.add_argument('--overwrite', action='store_true')
    args = parser.parse_args()

    packages = {}
    for directory in args.packages:
        if os.path.isdir(directory):
            for name in os.listdir(directory):
                if name.endswith('.zip'):
                    packages.setdefault(name[:-4], os.path.join(directory, name))
    videos = discover_videos(args.videos)
    if args.template:
        if not os.path.isfile(args.template):
            raise SystemExit('template package does not exist: ' + args.template)
        ids = sorted(videos)
    else:
        ids = sorted(set(packages) & set(videos))
    if args.only:
        ids = [args.only] if args.only in videos and (args.template or args.only in packages) else []
    print('%d packages, %d videos; BC1 %dx%d, %.0f KiB/frame' %
          (len(packages), len(ids), WIDTH, HEIGHT, gpu_bytes_per_frame() / 1024), flush=True)
    for number, ident in enumerate(ids, 1):
        destination = os.path.join(args.output, ident + '.zip')
        if os.path.isfile(destination) and not args.overwrite:
            print('%3d/%d %-46s exists' % (number, len(ids), ident[:44]), flush=True)
            continue
        print('%3d/%d %s' % (number, len(ids), ident), flush=True)
        write_package(packages.get(ident, args.template), videos[ident], destination,
                      ident if ident not in packages else None)
        print('    %.1f MB zip' % (os.path.getsize(destination) / 1048576), flush=True)


if __name__ == '__main__':
    main()
