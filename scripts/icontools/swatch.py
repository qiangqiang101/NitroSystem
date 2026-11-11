"""FH2 (Xbox One / Durango) UI icon .swatchbin <-> PNG.

File = 83-byte header (constant per size) + BC7 blocks in GCN-tiled order.
  small icon: 200x200 px, in a 256x256 surface  (64x64 blocks)
  large icon: 916x458 px, in a 1024x512 surface (256x128 blocks)
The storage index of the 4x4 block at (x, y) is a GF(2)-linear function of the block coordinates.
Surface 256x256 (verified on real files, see TEST in __main__):
  i0=x0 i1=y0 i2=x1 i3=y1 i4=x3^x4^y3 i5=x4^y4 i6=x5^y5 i7=y4^y5 i8=y3 i9=x2 i10=y2 i11=y5
"""
import sys
import numpy as np
from PIL import Image
from bc import decode
from bc7enc import encode

HDR_SMALL = bytes.fromhex(
    '62757247010001005300000053000100424358540200010028000000530000000000010000000100'
    '484358543002080000000000000000000000000000000001c800c8000100c800c80001000101002e0100b8')
HDR_LARGE = bytes.fromhex(
    '627572470100010053000000530008004243585402000100280000005300000000000800000008004843585430'
    '020800000000000000000000000000000000019403ca0101009403ca0101000101002e0100b8')
assert len(HDR_SMALL) == 0x53 and len(HDR_LARGE) == 0x53

# per surface: list of (xor mask over coordinate bits) for each storage-index bit; coordinate bit order x0..,y0..
def _small_index(x, y):
    g = lambda v, k: (v >> k) & 1
    i = [g(x, 0), g(y, 0), g(x, 1), g(y, 1),
         g(x, 3) ^ g(x, 4) ^ g(y, 3), g(x, 4) ^ g(y, 4), g(x, 5) ^ g(y, 5), g(y, 4) ^ g(y, 5),
         g(y, 3), g(x, 2), g(y, 2), g(y, 5)]
    return sum(b << k for k, b in enumerate(i))


def _large_index(x, y):
    g = lambda v, k: (v >> k) & 1
    i = [g(x, 0), g(y, 0), g(x, 1), g(y, 1),
         g(x, 3) ^ g(x, 4) ^ g(y, 3), g(x, 4) ^ g(y, 4), g(x, 5) ^ g(y, 5), g(x, 6) ^ g(y, 4) ^ g(y, 5),
         g(x, 7) ^ g(y, 3), g(x, 2), g(y, 2), g(x, 6), g(x, 7), g(y, 5), g(y, 6)]
    return sum(b << k for k, b in enumerate(i))


def _index_map(kind):
    if kind == 'small':
        bw, bh = 64, 64
        ys, xs = np.mgrid[0:bh, 0:bw]
        return _small_index(xs, ys), bw, bh
    if kind == 'large':
        bw, bh = 256, 128
        ys, xs = np.mgrid[0:bh, 0:bw]
        return _large_index(xs, ys), bw, bh
    raise ValueError(kind)


SIZES = {'small': (200, 200), 'large': (916, 458)}


def _premultiply(arr):
    a = arr[..., 3:4].astype(np.float32) / 255.0
    out = arr.copy()
    out[..., :3] = np.clip(np.round(arr[..., :3].astype(np.float32) * a), 0, 255).astype(np.uint8)
    return out


def _unpremultiply(arr):
    a = arr[..., 3:4].astype(np.float32)
    out = arr.copy()
    with np.errstate(divide='ignore', invalid='ignore'):
        rgb = np.where(a > 0, arr[..., :3].astype(np.float32) * 255.0 / np.maximum(a, 1), 0)
    out[..., :3] = np.clip(np.round(rgb), 0, 255).astype(np.uint8)
    return out


def png_to_swatchbin(img, kind='small'):
    """img: PIL image (straight alpha, as every PNG). The game's icons store premultiplied alpha, so the colour is
    multiplied by alpha here. A picture of another size is scaled (premultiplied) to the icon size."""
    w, h = SIZES[kind]
    img = img.convert('RGBA')
    arr = np.array(img)
    if img.size != (w, h):
        pre = Image.fromarray(_premultiply(arr), 'RGBA').resize((w, h), Image.LANCZOS)
        arr = np.array(pre)
    else:
        arr = _premultiply(arr)
    idx, bw, bh = _index_map(kind)
    surf = np.zeros((bh * 4, bw * 4, 4), np.uint8)
    surf[:h, :w] = arr
    blocks = np.frombuffer(encode(surf), np.uint8).reshape(bh, bw, 16)   # row-major (by, bx)
    # padding blocks (entirely outside the picture) are stored as 16 zero bytes in the original files
    nbx, nby = (w + 3) // 4, (h + 3) // 4
    out = np.zeros((bw * bh, 16), np.uint8)
    for by in range(nby):
        for bx in range(nbx):
            out[idx[by, bx]] = blocks[by, bx]
    return (HDR_SMALL if kind == 'small' else HDR_LARGE) + out.tobytes()


def swatchbin_to_png(data, kind='small'):
    """Returns a straight-alpha RGBA PIL image."""
    idx, bw, bh = _index_map(kind)
    body = np.frombuffer(data[0x53:], np.uint8).reshape(-1, 16)
    lin = body[idx.reshape(-1)]           # row-major block order
    img = decode(lin.tobytes(), bw * 4, bh * 4, 98)
    w, h = SIZES[kind]
    arr = _unpremultiply(np.array(img.crop((0, 0, w, h))))
    return Image.fromarray(arr, 'RGBA')


def kind_of(data):
    return {len(HDR_SMALL) + 65536: 'small', len(HDR_LARGE) + 524288: 'large'}[len(data)]


if __name__ == '__main__':
    print('library module; use png2swatchbin.py')
