import argparse
import csv
import os
import time
import urllib.error
import urllib.request
from io import BytesIO
from pathlib import Path

from PIL import Image, ImageOps


USER_AGENT = (
    "X3MTGTokenBuilder/0.1 "
    "(+https://github.com/coldcutsconsumer/crosspoint-reader)"
)

HEADERS = {
    "User-Agent": USER_AGENT,
    "Accept": "image/avif,image/webp,image/apng,image/*,*/*;q=0.8",
}

DEFAULT_SIZE = 224
DOWNLOAD_DELAY = 0.10
DOWNLOAD_RETRIES = 3


def get_cache_dir():
    base = Path(os.environ.get("LOCALAPPDATA", Path.home()))
    path = base / "X3MTG" / "art-source"
    path.mkdir(parents=True, exist_ok=True)
    return path


def load_tokens(tsv_path):
    with open(tsv_path, "r", encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(
            handle,
            delimiter="\t",
        )

        return list(reader)


def download_image(url, destination):
    if destination.exists():
        return destination

    request = urllib.request.Request(
        url,
        headers=HEADERS,
    )

    last_error = None

    for attempt in range(1, DOWNLOAD_RETRIES + 1):
        try:
            with urllib.request.urlopen(
                request,
                timeout=30,
            ) as response:
                data = response.read()

            # Make sure Pillow can actually decode what we received
            # before putting it into the cache.
            with Image.open(BytesIO(data)) as image:
                image.verify()

            temp = destination.with_suffix(
                destination.suffix + ".part"
            )

            with open(temp, "wb") as handle:
                handle.write(data)

            temp.replace(destination)

            time.sleep(DOWNLOAD_DELAY)

            return destination

        except (
            urllib.error.URLError,
            urllib.error.HTTPError,
            TimeoutError,
            OSError,
        ) as exc:
            last_error = exc

            print(
                f"    Download attempt {attempt}/"
                f"{DOWNLOAD_RETRIES} failed: {exc}"
            )

            time.sleep(attempt)

    raise RuntimeError(
        f"Could not download artwork after "
        f"{DOWNLOAD_RETRIES} attempts: {url}"
    ) from last_error


def prepare_image(source_path, size):
    with Image.open(source_path) as source:
        # Normalize source imagery before resizing.
        image = source.convert("RGB")

        # Crop to a square while preserving the center of the artwork.
        #
        # The 2x2 token layout is roughly square, so this avoids storing
        # a bunch of pixels we'll never display.
        image = ImageOps.fit(
            image,
            (size, size),
            method=Image.Resampling.LANCZOS,
            centering=(0.5, 0.5),
        )

        # Convert to grayscale first so we can improve contrast before
        # reducing everything to black/white.
        image = image.convert("L")

        # MTG artwork can be fairly dark. Stretching the grayscale range
        # before dithering tends to preserve much more detail on e-ink.
        image = ImageOps.autocontrast(
            image,
            cutoff=1,
        )

        # Convert to true 1-bit black/white using Floyd-Steinberg
        # dithering. This is intentionally done on the PC rather than
        # making the ESP32 perform image conversion.
        image = image.convert(
            "1",
            dither=Image.Dither.FLOYDSTEINBERG,
        )

        return image.copy()


def build_art(
    token,
    cache_dir,
    output_dir,
    size,
    force=False,
):
    token_id = token["token_id"]
    name = token["name"]
    art_url = token["art_url"]

    if not art_url:
        print(f"SKIP  {name}: no art_url")
        return False

    source_path = cache_dir / f"{token_id}.source"

    destination = output_dir / f"{token_id}.bmp"

    if destination.exists() and not force:
        print(f"HAVE  {name}")
        return True

    print(f"ART   {name}")

    download_image(
        art_url,
        source_path,
    )

    image = prepare_image(
        source_path,
        size,
    )

    # Pillow saves mode "1" BMPs as actual 1-bit BMP files.
    image.save(
        destination,
        format="BMP",
    )

    return True


def validate_output(path, expected_size):
    with Image.open(path) as image:
        if image.size != (
            expected_size,
            expected_size,
        ):
            raise RuntimeError(
                f"Unexpected BMP dimensions for {path}: "
                f"{image.size}"
            )

        if image.mode != "1":
            raise RuntimeError(
                f"Expected 1-bit BMP but got mode "
                f"{image.mode}: {path}"
            )


def main():
    parser = argparse.ArgumentParser(
        description=(
            "Build XTEINK X3-ready MTG token artwork "
            "from tokens.tsv."
        )
    )

    parser.add_argument(
        "sd_root",
        help="SD card root, for example H:\\",
    )

    parser.add_argument(
        "--limit",
        type=int,
        default=None,
        help=(
            "Process only the first N tokens. "
            "Useful for testing."
        ),
    )

    parser.add_argument(
        "--size",
        type=int,
        default=DEFAULT_SIZE,
        help=(
            f"Output image width/height in pixels "
            f"(default: {DEFAULT_SIZE})."
        ),
    )

    parser.add_argument(
        "--force",
        action="store_true",
        help="Rebuild BMP files that already exist.",
    )

    args = parser.parse_args()

    sd_root = Path(args.sd_root)

    mtg_root = sd_root / "MTG"
    tsv_path = mtg_root / "data" / "tokens.tsv"
    output_dir = mtg_root / "art"

    if not sd_root.exists():
        raise RuntimeError(
            f"SD card path does not exist: {sd_root}"
        )

    if not mtg_root.exists():
        raise RuntimeError(
            f"MTG directory does not exist: {mtg_root}"
        )

    if not tsv_path.exists():
        raise RuntimeError(
            f"Token database does not exist: {tsv_path}"
        )

    output_dir.mkdir(
        parents=True,
        exist_ok=True,
    )

    cache_dir = get_cache_dir()

    tokens = load_tokens(tsv_path)

    if args.limit is not None:
        if args.limit < 1:
            raise RuntimeError(
                "--limit must be greater than zero"
            )

        tokens = tokens[: args.limit]

    print()
    print("X3 MTG Token Artwork Builder")
    print("=" * 60)
    print(f"Token database: {tsv_path}")
    print(f"Output folder:  {output_dir}")
    print(f"Source cache:   {cache_dir}")
    print(f"Image size:     {args.size} x {args.size}")
    print(f"Tokens queued:  {len(tokens)}")
    print()

    success_count = 0
    failure_count = 0

    for index, token in enumerate(tokens, start=1):
        print(
            f"[{index:>3}/{len(tokens):>3}] ",
            end="",
        )

        try:
            built = build_art(
                token,
                cache_dir,
                output_dir,
                args.size,
                force=args.force,
            )

            if built:
                destination = (
                    output_dir
                    / f"{token['token_id']}.bmp"
                )

                validate_output(
                    destination,
                    args.size,
                )

                success_count += 1

        except Exception as exc:
            failure_count += 1

            print(
                f"ERROR {token['name']}: {exc}"
            )

    print()
    print("=" * 60)
    print("Artwork generation complete.")
    print(f"Successful: {success_count}")
    print(f"Failed:     {failure_count}")
    print()
    print(f"BMP files are in:")
    print(f"  {output_dir}")

    return 1 if failure_count else 0


if __name__ == "__main__":
    raise SystemExit(main())