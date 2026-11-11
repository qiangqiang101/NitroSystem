# FH2 upgrade icon format (.swatchbin), as far as it was decoded

Source: `media\ui\textures\data_bound\Upgrade_Parts.zip` (Xbox One build of Forza Horizon 2 on PC).

* 83-byte header (`burG` container with `BCXT` and `HCXT` chunks), identical for every file of a size:
  small icon 200x200 (file 65 619 bytes), large icon 916x458 (524 371 bytes). The picture dimensions are at offset 0x40/0x44, the pixel format byte at 0x4e.
* Pixels: BC7 (DXGI 98), 16 bytes per 4x4 block, **premultiplied alpha** (RGB <= A in the game's own icons), no mip levels.
  The picture is padded to 256x256 (small) / 1024x512 (large); blocks outside the picture are 16 zero bytes.
* Block order: Xbox One (GCN, 4 pipes, 4 banks) macro-tiled layout, 256-byte micro tiles of 4x4 blocks. With x, y = block
  coordinates and `i` = block index in the file (bit k = i_k):
  small (64x64 blocks):
    i0=x0 i1=y0 i2=x1 i3=y1 i4=x3^x4^y3 i5=x4^y4 i6=x5^y5 i7=y4^y5 i8=y3 i9=x2 i10=y2 i11=y5
  large (256x128 blocks):
    i0=x0 i1=y0 i2=x1 i3=y1 i4=x3^x4^y3 i5=x4^y4 i6=x5^y5 i7=x6^y4^y5 i8=x7^y3 i9=x2 i10=y2 i11=x6 i12=x7 i13=y5 i14=y6
  Checked on all 272 small and 172 large icons of Upgrade_Parts.zip: the all-zero blocks are exactly the padding, and the pictures decode cleanly.
* `swatch.py` converts both ways; `bc7enc.py` is a small BC7 encoder for flat icon art: per 4x4 block it tries mode 6 (RGBA on one line, 16 levels), mode 5 and mode 4 (colour and alpha on separate lines) with least-squares endpoint refinement, and keeps the mode with the smallest error.
