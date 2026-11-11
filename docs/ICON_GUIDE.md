# Making upgrade icons for the Forza Horizon 2 garage

This guide explains how the icons of the NitroSystem II upgrade menu entries are made, how to replace them with your own, and how to use the same technique in another mod. It covers:

1. [What the game expects](#1-what-the-game-expects)
2. [Drawing an icon](#2-drawing-an-icon)
3. [Replacing an icon of this mod](#3-replacing-an-icon-of-this-mod)
4. [Converting single files by hand](#4-converting-single-files-by-hand-png--swatchbin)
5. [How the icons reach the game](#5-how-the-icons-reach-the-game)
6. [The `.swatchbin` format](#6-the-swatchbin-format)
7. [Using this in your own mod](#7-using-this-in-your-own-mod)
8. [Troubleshooting](#8-troubleshooting)

## 1. What the game expects

Every upgrade type and every upgrade option in the garage menu has an `IconPath` (small tile) and an `ImagePath` (large picture) in the game database. The files are `.swatchbin` textures inside `media\ui\textures\data_bound\Upgrade_Parts.zip`, grouped in category folders (`Engine_Power`, `Tire_Rims`, ...). There are two sizes:

| Variant | Picture size | Stored as | File size | Used for |
|---|---|---|---|---|
| small | **200 x 200 px** | 256 x 256 surface | 65 619 bytes | the option tiles in the upgrade menu (the ones you see in the screenshots) |
| large (`_LG`) | **916 x 458 px** | 1024 x 512 surface | 524 371 bytes | the wide picture of a part; **not verified in game** for the custom parts of this mod, see the note below |

The game's own icons are BC7 compressed, use **premultiplied alpha** and use the Xbox One's tiled memory layout, so a plain PNG cannot be dropped in. The tools in `scripts/` convert PNGs to that format (section 6 describes it).

> **Large icons:** if you do not supply `<slot>_LG.png`, the mod points the large picture at a stock icon of a similar part. The small tile is the one that matters in the menu.

## 2. Drawing an icon

- **200 x 200 px, RGBA PNG, transparent background.** The tile in the menu is a light grey / white square, and the selected one is highlighted with an orange bar, so draw for a light background.
- The game's own part icons are monochrome "X-ray" style renderings: light grey, slightly transparent bodies with dark grey edges, shown at an angle. Keep the style consistent with them and the menu looks native.
- Leave some margin (about 10 px) around the picture, the tile is tight.
- Use **straight (non-premultiplied) alpha**, the way every PNG editor saves it. The converter premultiplies it for you.
- A picture of another size is scaled to the icon size by the converter, but you get the best result by drawing at the exact size. Soft, anti-aliased edges are fine; the encoder picks the best of three BC7 modes per 4x4 block, so edges stay clean.

## 3. Replacing an icon of this mod

The icons are PNGs in `icons/`. A script packs them into `src/icon_data.h`, which the DLL carries.

| Slot (file in `icons/`) | Used for |
|---|---|
| `kit.png` | the **Nitrous Kit** type tile (Engine) |
| `stage1.png`, `stage2.png`, `stage3.png` | the *Stage 1 / 2 / 3 Nitrous* option tiles (4 icons in total) |

The *Stock* option (no nitrous) uses the game's own stock icon.

Steps:

```sh
pip install pillow numpy                     # once
# put your 200x200 PNG into icons/ under the slot name, e.g. icons/kit.png
python scripts/gen_icons.py --prefix nitro
```

Then rebuild the DLL with `BUILD_WINDOWS.cmd`. Nothing else changes.

- `icons/<slot>.png` is the small (200 x 200) tile, `icons/<slot>_LG.png` the optional large (916 x 458) picture.
- If a slot has no PNG, the mod falls back to a stock game icon of a similar part.
- `--prefix` is part of the generated file names (`Custom\nitro_<slot>_Icon.swatchbin`); keep it as it is for this mod so the names stay unique among mods.
- `gen_icons.py` prints a warning when a PNG has the wrong size (it is scaled then).

`src/icon_data.h` is generated: do not edit it by hand.

## 4. Converting single files by hand (PNG <-> swatchbin)

```sh
python scripts/png2swatchbin.py my_icon.png                 # -> my_icon.swatchbin (200x200 = small, 916x458 = large)
python scripts/png2swatchbin.py my_icon.png out.swatchbin
python scripts/png2swatchbin.py --extract original.swatchbin original.png   # read one of the game's own icons
```

`--extract` is handy for studying the game's style: open `Upgrade_Parts.zip` from your own game folder with any zip tool and convert a few icons to PNG. (The game's files are not part of this repository.)

The set of tools lets you inspect the game's own icons and check what a conversion produces; the shipping route for a mod is the one in the next section.

## 5. How the icons reach the game

Nothing in the game's files is replaced or edited. Two small pieces make it work:

**a) The file.** At startup the mod unpacks each icon from `icon_data.h` (a run-length packed list of the 16-byte BC7 blocks, about a quarter of the raw size) into a buffer and gives it to the game with the platform's `set_file`:

```c
// path is relative to the game's media folder; a zip counts as a folder
"ui/textures/data_bound/Upgrade_Parts/Custom/nitro_kit_Icon.swatchbin"
api->set_file(mod, path, buffer, size);       // returns 0 if the platform refused it
```

`set_file` writes a copy into `mediaoverride`, which the game reads before its own files, and deletes it again when the mod is removed. Files added **inside** the existing `Upgrade_Parts.zip` folder are found by the game: that is the trick that makes new icons work without touching the zip. The mod logs the result: `icons handed to the game: 04/04`.

**b) The database row.** The upgrade's rows in `UpgradeTypes` / `Upgrades` carry the icon path in the `IconPath` (small) and `ImagePath` (large) columns:

```
GAME:\Media\UI\Textures\Data_Bound\Upgrade_Parts\Custom\nitro_kit_Icon.swatchbin
```

If the PNG slot does not exist, the same column points to a stock icon instead (for example `Tire_Rims\tire_width_Icon.swatchbin`), so the menu never shows a hole. See [RUNTIME_DB_GUIDE.md](RUNTIME_DB_GUIDE.md) for the rest of the database side.

## 6. The `.swatchbin` format

Short version (the complete write-up with the tiling formulas is in [`scripts/icontools/FORMAT.md`](../scripts/icontools/FORMAT.md)):

- 83-byte header (`burG` container with `BCXT` and `HCXT` chunks), identical for every file of one size. Width and height are at offsets `0x40` / `0x44`, the pixel format byte at `0x4e`.
- Pixels: **BC7** (DXGI format 98), 16 bytes per 4 x 4 block, **premultiplied alpha**, no mip levels. The picture sits in the top-left of a 256 x 256 (small) or 1024 x 512 (large) surface; blocks outside the picture are 16 zero bytes.
- Block order: the Xbox One (GCN) **macro-tiled** layout. The storage index of the block at (x, y) is a linear function over GF(2) of the block coordinates, e.g. for the small icon: `i0=x0 i1=y0 i2=x1 i3=y1 i4=x3^x4^y3 i5=x4^y4 i6=x5^y5 i7=y4^y5 i8=y3 i9=x2 i10=y2 i11=y5`. This was verified against all 272 small and 172 large icons of the game: the all-zero blocks are exactly the padding, and every icon decodes cleanly.
- Encoder (`scripts/icontools/bc7enc.py`): a small BC7 encoder for flat art that tries mode 6 (RGBA on one line, 16 levels), mode 5 and mode 4 (colour and alpha separately) for every block and keeps the one with the smallest error, with least-squares endpoint refinement. It is not a general-purpose encoder, but for icon art it is close to the quality of the game's own files.

## 7. Using this in your own mod

You need three files from this repository:

| File | Role |
|---|---|
| `scripts/icontools/` (`swatch.py`, `bc7enc.py`, `bc.py`) and `scripts/gen_icons.py` | PNG to `icon_data.h` (build time, Python with pillow and numpy) |
| `src/icon_pack.h` | unpacks an icon at runtime and builds the file name (C or C++, no libc needed) |
| `src/icon_data.h` | the generated data (rebuild it with your own PNGs and your own `--prefix`) |

Runtime code (this is all there is):

```c
#include "icon_data.h"
#include "icon_pack.h"

static void install_icons(const FH2ModApi *api, FH2Mod *mod) {
    static const char base[] = "ui/textures/data_bound/Upgrade_Parts/";
    for (const IconEntry *e = k_icons; e->slot; e++) {
        char path[200]; int n = 0;
        for (int i = 0; base[i]; i++) path[n++] = base[i];
        icon_name(e, path + n, '/');                 // "Custom/<prefix>_<slot>[_LG]_Icon.swatchbin"
        unsigned size = icon_unpack(e, g_icon_buf);  // header + blocks -> a complete .swatchbin
        api->set_file(mod, path, g_icon_buf, size);  // platform API 2 or newer
    }
}
```

and `icon_has("<slot>", large)` tells the SQL builder whether your mod carries a picture for a slot, so it can fall back to a stock icon path. Call `install_icons` from `fh2mod_init`, early, because the game reads most files once when it first needs them.

Pick a unique `--prefix` so your files cannot collide with another mod's `Custom\...` files.

## 8. Troubleshooting

| Symptom | Check |
|---|---|
| The icon tile is empty / white | `fh2-mods\platform.log`: is there an `icons handed to the game: N/N` line, and is N equal to the total? A smaller number means the platform refused a file (another mod claims the same path, or the platform is too old: `set_file` needs platform 2.1.0 or newer). |
| The old (stock) icon still shows | The path in the database row and the file name must match exactly, including `_LG` and the prefix. Run `python scripts/gen_icons.py --prefix nitro` and rebuild. |
| Icons look pixelated | Draw the PNG at exactly 200 x 200 (a scaled picture gets soft), use anti-aliasing, avoid very fine noise or one-pixel lines. The encoder is a small one, so very detailed photographic icons lose more than flat X-ray style art. |
| The colours look darker than the PNG | The game uses premultiplied alpha; the converter handles it. Semi-transparent areas look darker over a light tile than over a dark one. |
| `gen_icons.py` cannot import PIL / numpy | `pip install pillow numpy`. |
