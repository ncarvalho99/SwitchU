#!/usr/bin/env python3
"""BC7 modo 6: um subconjunto, extremos de 8 bits, indices de 4 bits.

Por que este modo e nao os outros sete: o modo 6 e o unico com indices de 4 bits
em todos os dezesseis pixels e extremos de precisao cheia, e e onde esta quase
todo o ganho sobre o BC1 para o que este projeto guarda -- video, sem alpha, com
gradiente suave. Os modos com dois e tres subconjuntos rendem mais em blocos que
tem duas superficies distintas, que num quadro de video sao a minoria.

Medido por simulacao antes de escrever isto, num quadro de "Miles Morales Purple
Neon", que e o pior caso do catalogo:

    BC1  (4 niveis, extremos 5-6-5)    34.25 dB    51.4% dos blocos com <=2 cores
    BC7 modo 6 (16 niveis, 7 bits)     42.25 dB    38.0%

Custa exatamente o dobro do BC1: 16 bytes por bloco de 4x4 contra 8.

Layout do bloco, do bit menos significativo para o mais (128 bits):

    modo      7 bits    seis zeros e um um
    R0 R1     7+7       extremos, um canal de cada vez
    G0 G1     7+7
    B0 B1     7+7
    A0 A1     7+7
    P0 P1     1+1       um bit por extremo, comum aos quatro canais
    indices   63        o primeiro tem 3 bits, os outros quinze tem 4

O primeiro indice perde o bit alto porque o decodificador o assume zero. E por
isso que os extremos as vezes precisam ser trocados de lugar: se o pixel do
canto ficaria acima da metade da reta, trocar hi com lo o traz para baixo sem
mudar a cor reconstruida.
"""

import numpy as np

# Pesos de interpolacao de 4 bits, em sessenta e quatro avos. Vem da
# especificacao; nao sao dezesseis passos iguais.
WEIGHTS4 = np.array([0, 4, 9, 13, 17, 21, 26, 30,
                     34, 38, 43, 47, 51, 55, 60, 64], dtype=np.float32) / 64.0


def _quantize_endpoints(v):
    """Extremos de 8 bits para sete bits mais um bit de paridade comum.

    O bit de paridade e um so para os quatro canais do extremo, entao ele nao da
    para ser escolhido canal a canal: os dois valores possiveis sao testados e
    fica o que erra menos no conjunto.
    """
    best_q = None
    best_p = None
    best_err = None
    for p in (0, 1):
        q = np.clip(np.rint((v - p) / 2.0), 0, 127)
        rec = q * 2 + p
        err = ((rec - v) ** 2).sum(-1)
        if best_err is None:
            best_q, best_p, best_err = q, np.full(err.shape, p, np.int64), err
        else:
            take = err < best_err
            best_q = np.where(take[..., None], q, best_q)
            best_p = np.where(take, p, best_p)
            best_err = np.where(take, err, best_err)
    return best_q.astype(np.int64), best_p.astype(np.int64)


def encode(img):
    """Uma imagem RGB para blocos BC7 modo 6, opaca."""
    a = np.asarray(img.convert('RGB'), dtype=np.float32)
    h, w, _ = a.shape
    if h % 4 or w % 4:
        raise ValueError('%dx%d nao e multiplo de 4' % (w, h))

    blocks = (a.reshape(h // 4, 4, w // 4, 4, 3)
               .transpose(0, 2, 1, 3, 4)
               .reshape(-1, 16, 3))
    n = len(blocks)

    # Alpha opaco entra como quarto canal para os extremos serem escolhidos
    # junto com ele -- e o bit de paridade e comum aos quatro.
    rgba = np.concatenate([blocks, np.full((n, 16, 1), 255.0, np.float32)], -1)

    mean = rgba.mean(1, keepdims=True)
    centred = rgba - mean
    axis = np.linalg.svd(centred, full_matrices=False)[2][:, 0, :][:, None, :]
    proj = (centred * axis).sum(-1)
    hi = np.clip(mean[:, 0] + axis[:, 0] * proj.max(1, keepdims=True), 0, 255)
    lo = np.clip(mean[:, 0] + axis[:, 0] * proj.min(1, keepdims=True), 0, 255)

    q_hi, p_hi = _quantize_endpoints(hi)
    q_lo, p_lo = _quantize_endpoints(lo)
    d_hi = q_hi * 2 + p_hi[:, None]
    d_lo = q_lo * 2 + p_lo[:, None]

    # Indices: posicao de cada pixel na reta lo->hi, no passo mais proximo.
    diff = (d_hi - d_lo).astype(np.float32)
    denom = (diff * diff).sum(-1, keepdims=True)
    denom[denom == 0] = 1.0
    t = ((rgba - d_lo[:, None].astype(np.float32)) * diff[:, None]).sum(-1) / denom[:, 0][:, None]
    idx = np.abs(t[..., None] - WEIGHTS4).argmin(-1).astype(np.int64)

    # O primeiro indice nao tem bit alto. Trocar os extremos inverte todos os
    # indices e resolve, sem alterar as cores que o bloco reconstroi.
    flip = idx[:, 0] > 7
    idx = np.where(flip[:, None], 15 - idx, idx)
    q_hi, q_lo = (np.where(flip[:, None], q_lo, q_hi),
                  np.where(flip[:, None], q_hi, q_lo))
    p_hi, p_lo = np.where(flip, p_lo, p_hi), np.where(flip, p_hi, p_lo)

    return _pack(q_lo, q_hi, p_lo, p_hi, idx)


def _pack(q0, q1, p0, p1, idx):
    """Os campos acima num fluxo de 128 bits por bloco, do bit 0 para cima."""
    n = len(idx)
    out = np.zeros((n, 16), dtype=np.uint8)
    pos = np.zeros(n, dtype=np.int64)

    def put(value, bits):
        nonlocal pos
        value = np.asarray(value, dtype=np.int64)
        for b in range(bits):
            bit = (value >> b) & 1
            byte = (pos + b) >> 3
            off = (pos + b) & 7
            np.add.at(out, (np.arange(n), byte), (bit << off).astype(np.uint8))
        pos = pos + bits

    put(np.full(n, 1 << 6), 7)                 # modo 6: seis zeros e um um
    for c in range(4):                          # R, G, B, A -- extremo 0 e 1
        put(q0[:, c], 7)
        put(q1[:, c], 7)
    put(p0, 1)
    put(p1, 1)
    put(idx[:, 0], 3)                           # ancora, sem o bit alto
    for i in range(1, 16):
        put(idx[:, i], 4)
    return out.tobytes()


def dds_header(w, h, payload):
    """Cabecalho DDS com a extensao DX10, que e como se declara BC7.

    O DXT1 cabia no cabecalho antigo por ter um fourCC proprio; o BC7 nao tem, e
    precisa do bloco DX10 com o numero do formato DXGI.
    """
    import struct
    DDSD = 0x1 | 0x2 | 0x4 | 0x1000 | 0x80000
    pf = struct.pack('<2I4s5I', 32, 0x4, b'DX10', 0, 0, 0, 0, 0)
    head = (b'DDS ' +
            struct.pack('<7I', 124, DDSD, h, w, len(payload), 0, 0) +
            b'\0' * 44 + pf +
            struct.pack('<5I', 0x1000, 0, 0, 0, 0))
    DXGI_FORMAT_BC7_UNORM = 98
    dx10 = struct.pack('<5I', DXGI_FORMAT_BC7_UNORM, 3, 0, 1, 0)
    return head + dx10
