import os
import shutil
from pathlib import Path

from PIL import Image, ImageOps


TOKEN_ID = "86c5d291db3d103e"
SIZE = 288

cache_root = (
    Path(os.environ["LOCALAPPDATA"])
    / "X3MTG"
    / "art-source"
)

source_path = cache_root / f"{TOKEN_ID}.source"

sd_root = Path("H:/")

elemental_path = (
    sd_root
    / "MTG"
    / "art"
    / "E"
    / f"{TOKEN_ID}.bmp"
)

backup_path = (
    sd_root
    / "MTG"
    / "art"
    / "E"
    / f"{TOKEN_ID}-224-backup.bmp"
)

gray_test_path = (
    sd_root
    / "elemental-gray-test.bmp"
)


if not source_path.exists():
    raise RuntimeError(
        f"Cached source art not found: {source_path}"
    )

if not elemental_path.exists():
    raise RuntimeError(
        f"Elemental BMP not found: {elemental_path}"
    )


# Keep the original 224x224 Elemental BMP.
if not backup_path.exists():
    shutil.copy2(
        elemental_path,
        backup_path,
    )

    print(f"Backed up original:")
    print(f"  {backup_path}")


with Image.open(source_path) as source:
    image = source.convert("RGB")

    image = ImageOps.fit(
        image,
        (SIZE, SIZE),
        method=Image.Resampling.LANCZOS,
        centering=(0.5, 0.5),
    )

    gray = image.convert("L")

    gray = ImageOps.autocontrast(
        gray,
        cutoff=1,
    )

    # Larger version of our existing 1-bit art.
    bw = gray.convert(
        "1",
        dither=Image.Dither.FLOYDSTEINBERG,
    )

    bw.save(
        elemental_path,
        format="BMP",
    )

    # Build a true four-level grayscale test:
    #
    # 0   = black
    # 85  = dark gray
    # 170 = light gray
    # 255 = white
    gray4 = gray.point(
        lambda value:
            min(
                255,
                round(value / 85) * 85,
            )
    )

    # Save as an ordinary uncompressed 24-bit BMP.
    #
    # This is deliberately not optimized for storage yet.
    # We're testing the X3's physical grayscale output first.
    gray4.convert("RGB").save(
        gray_test_path,
        format="BMP",
    )


print()
print("Created larger 1-bit Elemental:")
print(f"  {elemental_path}")
print(f"  {SIZE}x{SIZE}")

print()
print("Created four-level grayscale test:")
print(f"  {gray_test_path}")
print(f"  {SIZE}x{SIZE}")