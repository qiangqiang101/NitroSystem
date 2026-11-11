#!/usr/bin/env python3
"""FH2 upgrade icon converter (PNG <-> .swatchbin).

  python3 png2swatchbin.py icon.png                 -> icon.swatchbin  (200x200 = small, 916x458 = large icon)
  python3 png2swatchbin.py icon.png out.swatchbin
  python3 png2swatchbin.py --extract original.swatchbin out.png   (read one of the game's own icons)

Needs: python3, numpy, pillow.  The game's icons are straight BC7 data, premultiplied alpha, Xbox One (GCN) tiled.
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from PIL import Image
import swatch


def main(argv):
    if len(argv) >= 3 and argv[0] == '--extract':
        data = open(argv[1], 'rb').read()
        img = swatch.swatchbin_to_png(data, swatch.kind_of(data))
        img.save(argv[2]); print('wrote', argv[2], img.size); return 0
    if not argv:
        print(__doc__); return 1
    src = argv[0]; dst = argv[1] if len(argv) > 1 else os.path.splitext(src)[0] + '.swatchbin'
    img = Image.open(src)
    kind = {v: k for k, v in swatch.SIZES.items()}.get(img.size)
    if kind is None:
        # not one of the two icon sizes: small unless clearly wide
        kind = 'large' if img.size[0] >= img.size[1] * 1.6 else 'small'
        print('note: %s is %dx%d, scaling to %dx%d (%s icon)' % (src, img.size[0], img.size[1], *swatch.SIZES[kind], kind))
    open(dst, 'wb').write(swatch.png_to_swatchbin(img, kind))
    print('wrote', dst, '(%s icon)' % kind); return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
