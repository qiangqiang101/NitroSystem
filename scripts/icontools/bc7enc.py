"""Small BC7 encoder for flat / X-ray style UI icons.
Single-subset modes only: 6 (RGBA on one line, 16 levels), 5 (RGB line and alpha line separately, 4+4 levels) and
4 (separate lines, 4+8 or 8+4 levels, 5-bit colour). Every block takes the mode with the smallest error.
Input: RGBA uint8 array (h, w, 4), h and w multiples of 4 (premultiplied alpha stays as given).
Output: bytes, 16 per 4x4 block, in row-major block order."""
import numpy as np

W2 = np.array([0, 21, 43, 64], np.float32)
W3 = np.array([0, 9, 18, 27, 37, 46, 55, 64], np.float32)
W4 = np.array([0, 4, 9, 13, 17, 21, 26, 30, 34, 38, 43, 47, 51, 55, 60, 64], np.float32)


def _interp(e0, e1, W):
    """e0,e1: (n,C) float -> palette (n,L,C)"""
    return (e0[:, None, :] * (64 - W)[None, :, None] + e1[:, None, :] * W[None, :, None]) / 64.0


def _assign(px, pal):
    """px (n,16,C), pal (n,L,C) -> idx (n,16), err (n)"""
    d = ((px[:, :, None, :] - pal[:, None, :, :]) ** 2).sum(-1)
    idx = d.argmin(axis=2).astype(np.int64)
    err = np.take_along_axis(d, idx[:, :, None], axis=2).sum(axis=(1, 2))
    return idx, err


def _principal_line(px):
    n, _, C = px.shape
    mean = px.mean(axis=1, keepdims=True)
    c = px - mean
    cov = np.einsum('npi,npj->nij', c, c)
    v = np.ones((n, C), np.float32)
    for _ in range(6):
        v = np.einsum('nij,nj->ni', cov, v)
        nv = np.linalg.norm(v, axis=1, keepdims=True)
        v = np.where(nv > 1e-6, v / np.maximum(nv, 1e-6), np.float32(1.0) / np.sqrt(C))
    t = np.einsum('npi,ni->np', c, v)
    lo = np.clip(mean[:, 0, :] + v * t.min(axis=1)[:, None], 0, 255)
    hi = np.clip(mean[:, 0, :] + v * t.max(axis=1)[:, None], 0, 255)
    return lo, hi


def _lsq(px, idx, W, lo, hi):
    w = (W[idx] / 64.0).astype(np.float32)
    a00 = ((1 - w) ** 2).sum(1); a01 = ((1 - w) * w).sum(1); a11 = (w ** 2).sum(1)
    det = a00 * a11 - a01 * a01
    ok = np.abs(det) > 1e-4
    b0 = np.einsum('np,npc->nc', 1 - w, px); b1 = np.einsum('np,npc->nc', w, px)
    safe = np.where(ok, det, 1.0)
    e0 = (a11[:, None] * b0 - a01[:, None] * b1) / safe[:, None]
    e1 = (a00[:, None] * b1 - a01[:, None] * b0) / safe[:, None]
    return np.clip(np.where(ok[:, None], e0, lo), 0, 255), np.clip(np.where(ok[:, None], e1, hi), 0, 255)


def _quant_plain(e, bits):
    """no p-bit: returns (q ints, reconstructed 0..255 floats)"""
    m = (1 << bits) - 1
    q = np.clip(np.round(e * m / 255.0), 0, m).astype(np.int64)
    rec = ((q << (8 - bits)) | (q >> (2 * bits - 8))).astype(np.float32)
    return q, rec


def _quant_pbit(e):
    """mode 6: 7 bit + one p-bit per endpoint (shared by the four channels)"""
    best = None
    for p in (0, 1):
        q = np.clip(np.round((e - p) / 2.0), 0, 127).astype(np.int64)
        rec = (q * 2 + p).astype(np.float32)
        err = ((rec - e) ** 2).sum(axis=1)
        if best is None:
            best = [q, np.full(len(e), p, np.int64), rec, err]
        else:
            m = err < best[3]
            best[0] = np.where(m[:, None], q, best[0]); best[1] = np.where(m, p, best[1])
            best[2] = np.where(m[:, None], rec, best[2]); best[3] = np.where(m, err, best[3])
    return best[0], best[1], best[2]


def _fit(px, W, quant, iters=5, lo=None, hi=None):
    """Fit one line with len(W) levels. quant(e) -> (extra, rec) ; returns dict(extra0, extra1, idx, err, rec0, rec1)."""
    if lo is None:
        lo, hi = _principal_line(px)
    def run(lo, hi):
        x0, r0 = quant(lo); x1, r1 = quant(hi)
        idx, err = _assign(px, _interp(r0, r1, W))
        return dict(x0=x0, x1=x1, idx=idx, err=err, r0=r0, r1=r1, lo=lo, hi=hi)
    cur = run(lo, hi)
    for _ in range(iters):
        e0, e1 = _lsq(px, cur['idx'], W, cur['lo'], cur['hi'])
        nxt = run(e0, e1)
        better = nxt['err'] < cur['err']
        if not better.any(): break
        for k in list(cur):
            a, b = cur[k], nxt[k]
            m = better.reshape((-1,) + (1,) * (a.ndim - 1))
            cur[k] = np.where(m, b, a)
    return cur


def _fix_anchor(f, nlev):
    """pixel 0 may not use the upper half of the index range: swap endpoints and mirror the indices where it does"""
    sw = f['idx'][:, 0] >= nlev // 2
    if sw.any():
        f['x0'], f['x1'] = np.where(sw[:, None], f['x1'], f['x0']), np.where(sw[:, None], f['x0'], f['x1'])
        f['idx'] = np.where(sw[:, None], (nlev - 1) - f['idx'], f['idx'])
        if 'p0' in f:
            f['p0'], f['p1'] = np.where(sw, f['p1'], f['p0']), np.where(sw, f['p0'], f['p1'])
    return f


def _pack(fields):
    """fields: list of (value, nbits), LSB first"""
    bits = 0; pos = 0
    for v, nb in fields:
        bits |= (int(v) & ((1 << nb) - 1)) << pos
        pos += nb
    assert pos == 128, pos
    return bits.to_bytes(16, 'little')


def _idx_fields(idx, nb):
    out = [(idx[0], nb - 1)]
    out += [(idx[k], nb) for k in range(1, 16)]
    return out


def _mode6(px):
    lo, hi = _principal_line(px)
    state = {}
    def quant(e):
        q, p, rec = _quant_pbit(e)
        return (q, p), rec
    # p-bits need to be carried: wrap
    def run(lo, hi):
        (q0, p0), r0 = quant(lo); (q1, p1), r1 = quant(hi)
        idx, err = _assign(px, _interp(r0, r1, W4))
        return dict(q0=q0, q1=q1, p0=p0, p1=p1, idx=idx, err=err, r0=r0, r1=r1, lo=lo, hi=hi)
    cur = run(lo, hi)
    for _ in range(5):
        e0, e1 = _lsq(px, cur['idx'], W4, cur['lo'], cur['hi'])
        nxt = run(e0, e1)
        better = nxt['err'] < cur['err']
        if not better.any(): break
        for k in list(cur):
            a, b = cur[k], nxt[k]
            m = better.reshape((-1,) + (1,) * (a.ndim - 1))
            cur[k] = np.where(m, b, a)
    sw = cur['idx'][:, 0] >= 8
    q0 = np.where(sw[:, None], cur['q1'], cur['q0']); q1 = np.where(sw[:, None], cur['q0'], cur['q1'])
    p0 = np.where(sw, cur['p1'], cur['p0']); p1 = np.where(sw, cur['p0'], cur['p1'])
    idx = np.where(sw[:, None], 15 - cur['idx'], cur['idx'])
    blocks = []
    for i in range(len(px)):
        f = [(0b1000000, 7)]
        for ch in range(4):
            f += [(q0[i, ch], 7), (q1[i, ch], 7)]
        f += [(p0[i], 1), (p1[i], 1)] + _idx_fields(idx[i], 4)
        blocks.append(f)
    return cur['err'], blocks


def _mode45(px, mode):
    rgb, a = px[:, :, :3], px[:, :, 3:4]
    if mode == 5:
        cb, ab, variants = 7, 8, [(W2, W2, 0)]
    else:
        cb, ab, variants = 5, 6, [(W2, W3, 0), (W3, W2, 1)]
    best = None
    lo_c, hi_c = _principal_line(rgb)
    lo_a, hi_a = _principal_line(a)
    for Wc, Wa, isel in variants:
        fc = _fit(rgb, Wc, lambda e: _quant_plain(e, cb), lo=lo_c, hi=hi_c)
        fa = _fit(a, Wa, lambda e: _quant_plain(e, ab), lo=lo_a, hi=hi_a)
        err = fc['err'] + fa['err']
        fc = _fix_anchor(fc, len(Wc)); fa = _fix_anchor(fa, len(Wa))
        blocks = []
        for i in range(len(px)):
            if mode == 5:
                f = [(1 << 5, 6), (0, 2)]
            else:
                f = [(1 << 4, 5), (0, 2), (isel, 1)]
            for ch in range(3):
                f += [(fc['x0'][i, ch], cb), (fc['x1'][i, ch], cb)]
            f += [(fa['x0'][i, 0], ab), (fa['x1'][i, 0], ab)]
            ci = _idx_fields(fc['idx'][i], int(np.log2(len(Wc))))
            ai = _idx_fields(fa['idx'][i], int(np.log2(len(Wa))))
            if mode == 5 or isel == 0:
                f += ci + ai            # 2-bit set first (colour when idx mode 0), then the 3-bit set
            else:
                f += ai + ci
            blocks.append(f)
        if best is None:
            best = (err, blocks)
        else:
            m = err < best[0]
            best = (np.where(m, err, best[0]), [b if mm else o for b, o, mm in zip(blocks, best[1], m)])
    return best


def encode(rgba):
    h, w, _ = rgba.shape
    bh, bw = h // 4, w // 4
    px = rgba.astype(np.float32).reshape(bh, 4, bw, 4, 4).transpose(0, 2, 1, 3, 4).reshape(-1, 16, 4)
    cands = [_mode6(px), _mode45(px, 5), _mode45(px, 4)]
    errs = np.stack([c[0] for c in cands])
    pick = errs.argmin(axis=0)
    out = bytearray()
    for i in range(len(px)):
        out += _pack(cands[pick[i]][1][i])
    return bytes(out)
