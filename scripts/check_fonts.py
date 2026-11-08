# Checks that every translation in src/lang.cpp only uses characters the game's fonts for that language have, so
# none of them shows as a box on the Mods page.
#   python scripts/check_fonts.py "<Mount>/media/ui/Fonts.zip"
#
# The game picks fonts by language (fontsettings.xml in Fonts.zip): the Anthem fonts for Latin text, with Verdana
# behind them for Russian, and a Japanese, Korean or Chinese font that holds only the characters the game's own
# text uses. A .vfont has its glyph count at 0x80 and, from 0xC0, a 28-byte record per glyph starting with its code
# point (see ANNA Drone Mode's gamefont.cpp).
import re
import struct
import sys
import zipfile
from pathlib import Path

FALLBACK = {"JP": "DFGHSGothic-W5", "KO": "MalgunBdGm", "CHT": "DFYuanMedium-B5", "RU": "verdana"}


def code_points(data):
    count = struct.unpack_from("<I", data, 0x80)[0]
    return {struct.unpack_from("<I", data, 0xC0 + 28 * i)[0] for i in range(count)}


def c_string(literal):   # the table only escapes quotes and backslashes
    return re.sub(r'\\(["\\])', r"\1", literal)


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__ if __doc__ else "usage: check_fonts.py <Fonts.zip>")
    fonts = zipfile.ZipFile(sys.argv[1])
    glyphs = {Path(n).stem: code_points(fonts.read(n)) for n in fonts.namelist() if n.endswith(".vfont")}
    latin = glyphs["Anthem_A"]

    src = (Path(__file__).resolve().parent.parent / "src" / "lang.cpp").read_text(encoding="utf-8")
    codes = re.findall(r'"([A-Z]+)"', re.search(r"kCodes\[\] = \{([^}]*)\}", src).group(1))
    table = src[src.index("const Text kTexts[] = {"):src.index("// clang-format on")]
    literal = r'"((?:[^"\\]|\\.)*)"'
    rows = re.findall(r"\{" + literal + r",\s*\{((?:\s*" + literal + r",?)+)\s*\}\}", table)

    problems = 0
    for row in rows:
        english = c_string(row[0])
        texts = [c_string(t) for t in re.findall(literal, row[1])]
        if len(texts) != len(codes):
            print(f"{english[:50]!r}: {len(texts)} translations for {len(codes)} languages")
            problems += 1
            continue
        for code, text in [("EN", english)] + list(zip(codes, texts)):
            have = latin | glyphs.get(FALLBACK.get(code, ""), set())
            missing = sorted({c for c in text if ord(c) not in have})
            if missing:
                print(f"{code}: {''.join(missing)} not in its font, in {text[:60]!r}")
                problems += 1
    print(f"{len(rows)} texts, {len(codes)} languages: {problems} problem(s)")
    sys.exit(1 if problems else 0)


if __name__ == "__main__":
    main()
