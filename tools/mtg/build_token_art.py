import argparse
import csv
import os
import shutil
import time
import urllib.request
from concurrent.futures import ProcessPoolExecutor, as_completed
from pathlib import Path

from PIL import Image, ImageEnhance, ImageOps


USER_AGENT = (
    "X3MTGTokenBuilder/0.3 "
    "(+https://github.com/coldcutsconsumer/crosspoint-reader)"
)

HEADERS = {
    "User-Agent": USER_AGENT,
    "Accept": "image/*,*/*;q=0.8",
}

EXPECTED_TOKEN_COUNT = 526

TARGET_WIDTH = 250
TARGET_HEIGHT = 250

DEFAULT_WORKERS = min(
    8,
    os.cpu_count() or 4,
)


def request(url):
    req = urllib.request.Request(
        url,
        headers=HEADERS,
    )

    return urllib.request.urlopen(
        req,
        timeout=60,
    )


def get_x3mtg_cache():
    base = Path(
        os.environ.get(
            "LOCALAPPDATA",
            Path.home(),
        )
    )

    root = (
        base
        / "X3MTG"
    )

    root.mkdir(
        parents=True,
        exist_ok=True,
    )

    return root


def get_source_cache():
    path = (
        get_x3mtg_cache()
        / "art-source"
    )

    path.mkdir(
        parents=True,
        exist_ok=True,
    )

    return path


def get_stage_root():
    return (
        get_x3mtg_cache()
        / "art-rev2-250x250-atkinson"
    )


def load_tokens(master_path):
    with open(
        master_path,
        "r",
        encoding="utf-8",
        newline="",
    ) as handle:
        return list(
            csv.DictReader(
                handle,
                delimiter="\t",
            )
        )


def token_bucket(name):
    name = (
        name
        or ""
    ).strip()

    if not name:
        return "#"

    first = name[0].upper()

    if (
        len(first) == 1
        and
        "A" <= first <= "Z"
    ):
        return first

    return "#"


def find_cached_source(token_id):
    cache = get_source_cache()

    candidates = [
        cache / f"{token_id}.jpg",
        cache / f"{token_id}.jpeg",
        cache / f"{token_id}.png",
        cache / f"{token_id}.webp",
        cache / f"{token_id}.source",
    ]

    for path in candidates:
        if not path.exists():
            continue

        try:
            with Image.open(
                path
            ) as image:
                image.verify()

            return path

        except Exception:
            pass

    return None


def download_source(
    token,
    attempt_count=3,
):
    token_id = (
        token.get(
            "token_id",
            "",
        )
        .strip()
    )

    name = (
        token.get(
            "name",
            "",
        )
        .strip()
    )

    art_url = (
        token.get(
            "art_url",
            "",
        )
        .strip()
    )

    if not token_id:
        raise RuntimeError(
            f"{name}: missing token_id"
        )

    if not art_url:
        raise RuntimeError(
            f"{name}: missing art_url"
        )

    cache = get_source_cache()

    destination = (
        cache
        / f"{token_id}.jpg"
    )

    temp = (
        cache
        / f"{token_id}.jpg.part"
    )

    for attempt in range(
        1,
        attempt_count + 1,
    ):
        try:
            if temp.exists():
                temp.unlink()

            with request(
                art_url
            ) as response, open(
                temp,
                "wb",
            ) as output:
                shutil.copyfileobj(
                    response,
                    output,
                )

            with Image.open(
                temp
            ) as image:
                image.verify()

            temp.replace(
                destination
            )

            return destination

        except Exception as exc:
            if temp.exists():
                try:
                    temp.unlink()
                except OSError:
                    pass

            if attempt >= attempt_count:
                raise RuntimeError(
                    f"Failed downloading {name}: {exc}"
                ) from exc

            print(
                f"Retrying {name} "
                f"({attempt}/{attempt_count})..."
            )

            time.sleep(
                float(attempt)
            )

    raise RuntimeError(
        f"Could not download {name}"
    )


def ensure_source_art(tokens):
    source_paths = {}
    missing = []

    for token in tokens:
        token_id = (
            token.get(
                "token_id",
                "",
            )
            .strip()
        )

        cached = find_cached_source(
            token_id
        )

        if cached is not None:
            source_paths[
                token_id
            ] = cached

        else:
            missing.append(
                token
            )

    print()

    print(
        f"Cached source art:   "
        f"{len(source_paths)}"
    )

    print(
        f"Sources to download: "
        f"{len(missing)}"
    )

    if not missing:
        return source_paths

    print()

    total_missing = len(
        missing
    )

    for index, token in enumerate(
        missing,
        start=1,
    ):
        name = (
            token.get(
                "name",
                "",
            )
            .strip()
        )

        token_id = (
            token.get(
                "token_id",
                "",
            )
            .strip()
        )

        print(
            f"[{index:>3}/{total_missing}] "
            f"Downloading {name}"
        )

        source = download_source(
            token
        )

        source_paths[
            token_id
        ] = source

        time.sleep(
            0.12
        )

    return source_paths


def prepare_grayscale(
    source_path,
):
    with Image.open(
        source_path
    ) as source:
        source = source.convert(
            "RGB"
        )

        # Stretch the COMPLETE art_crop into the final
        # 250x250 bitmap. Nothing is cropped.
        resized = source.resize(
            (
                TARGET_WIDTH,
                TARGET_HEIGHT,
            ),
            Image.Resampling.LANCZOS,
        )

        gray = resized.convert(
            "L"
        )

        gray = ImageOps.autocontrast(
            gray,
            cutoff=1,
        )

        gray = ImageEnhance.Sharpness(
            gray
        ).enhance(
            1.20
        )

        return gray


def atkinson_dither(gray):
    width, height = gray.size

    pixels = [
        float(value)
        for value in gray.getdata()
    ]

    output = bytearray(
        width
        * height
    )

    def add_error(
        x,
        y,
        amount,
    ):
        if (
            x < 0
            or
            x >= width
            or
            y < 0
            or
            y >= height
        ):
            return

        index = (
            y
            * width
            + x
        )

        pixels[index] += amount

    for y in range(
        height
    ):
        row_start = (
            y
            * width
        )

        for x in range(
            width
        ):
            index = (
                row_start
                + x
            )

            old_value = (
                pixels[index]
            )

            new_value = (
                255.0
                if old_value >= 128.0
                else 0.0
            )

            output[index] = (
                255
                if new_value > 0
                else 0
            )

            error = (
                old_value
                - new_value
            )

            distributed = (
                error
                / 8.0
            )

            add_error(
                x + 1,
                y,
                distributed,
            )

            add_error(
                x + 2,
                y,
                distributed,
            )

            add_error(
                x - 1,
                y + 1,
                distributed,
            )

            add_error(
                x,
                y + 1,
                distributed,
            )

            add_error(
                x + 1,
                y + 1,
                distributed,
            )

            add_error(
                x,
                y + 2,
                distributed,
            )

    result = Image.frombytes(
        "L",
        (
            width,
            height,
        ),
        bytes(output),
    )

    return result.convert(
        "1",
        dither=Image.Dither.NONE,
    )


def build_one_token(job):
    (
        token_id,
        name,
        bucket,
        source_path,
        stage_root,
    ) = job

    source_path = Path(
        source_path
    )

    stage_root = Path(
        stage_root
    )

    destination_dir = (
        stage_root
        / bucket
    )

    destination_dir.mkdir(
        parents=True,
        exist_ok=True,
    )

    destination = (
        destination_dir
        / f"{token_id}.bmp"
    )

    gray = prepare_grayscale(
        source_path
    )

    result = atkinson_dither(
        gray
    )

    result.save(
        destination,
        format="BMP",
    )

    return (
        token_id,
        name,
        str(destination),
    )


def clear_stage(stage_root):
    if stage_root.exists():
        shutil.rmtree(
            stage_root
        )

    stage_root.mkdir(
        parents=True,
        exist_ok=True,
    )


def build_all_art(
    tokens,
    source_paths,
    stage_root,
    workers,
):
    jobs = []

    for token in tokens:
        token_id = (
            token.get(
                "token_id",
                "",
            )
            .strip()
        )

        name = (
            token.get(
                "name",
                "",
            )
            .strip()
        )

        bucket = token_bucket(
            name
        )

        jobs.append(
            (
                token_id,
                name,
                bucket,
                str(
                    source_paths[
                        token_id
                    ]
                ),
                str(
                    stage_root
                ),
            )
        )

    print()

    print(
        "=" * 60
    )

    print(
        "Building MTG Art Rev2"
    )

    print(
        "=" * 60
    )

    print(
        f"Tokens:      {len(jobs)}"
    )

    print(
        f"Resolution:  "
        f"{TARGET_WIDTH}x{TARGET_HEIGHT}"
    )

    print(
        "Dithering:   Atkinson"
    )

    print(
        "Cropping:    None"
    )

    print(
        "Scaling:     Stretch to fit"
    )

    print(
        f"CPU workers: {workers}"
    )

    print()

    completed = 0
    failures = []

    with ProcessPoolExecutor(
        max_workers=workers
    ) as executor:
        futures = {
            executor.submit(
                build_one_token,
                job,
            ): job
            for job in jobs
        }

        for future in as_completed(
            futures
        ):
            job = futures[
                future
            ]

            (
                token_id,
                name,
                _bucket,
                _source,
                _stage,
            ) = job

            try:
                future.result()

                completed += 1

                print(
                    f"[{completed:>3}/{len(jobs)}] "
                    f"{name}"
                )

            except Exception as exc:
                failures.append(
                    (
                        token_id,
                        name,
                        str(exc),
                    )
                )

                print(
                    f"FAILED: {name}"
                )

                print(
                    f"        {exc}"
                )

    if failures:
        print()

        print(
            "Artwork build FAILED."
        )

        for (
            token_id,
            name,
            error,
        ) in failures:
            print(
                f"  {name} "
                f"({token_id})"
            )

            print(
                f"    {error}"
            )

        raise RuntimeError(
            f"{len(failures)} token images failed."
        )


def verify_art_tree(
    tokens,
    root,
):
    failures = []

    for token in tokens:
        token_id = (
            token.get(
                "token_id",
                "",
            )
            .strip()
        )

        name = (
            token.get(
                "name",
                "",
            )
            .strip()
        )

        bucket = token_bucket(
            name
        )

        path = (
            root
            / bucket
            / f"{token_id}.bmp"
        )

        if not path.exists():
            failures.append(
                (
                    name,
                    "missing file",
                )
            )

            continue

        try:
            with Image.open(
                path
            ) as image:
                if image.size != (
                    TARGET_WIDTH,
                    TARGET_HEIGHT,
                ):
                    failures.append(
                        (
                            name,
                            (
                                "wrong size "
                                f"{image.size}"
                            ),
                        )
                    )

                if image.mode != "1":
                    failures.append(
                        (
                            name,
                            (
                                "wrong image mode "
                                f"{image.mode}"
                            ),
                        )
                    )

        except Exception as exc:
            failures.append(
                (
                    name,
                    str(exc),
                )
            )

    if failures:
        for (
            name,
            error,
        ) in failures[
            :25
        ]:
            print(
                f"  {name}: {error}"
            )

        raise RuntimeError(
            f"{len(failures)} images failed verification."
        )

    bmp_count = len(
        list(
            root.rglob(
                "*.bmp"
            )
        )
    )

    if bmp_count != len(
        tokens
    ):
        raise RuntimeError(
            "Artwork count mismatch: "
            f"expected {len(tokens)}, "
            f"found {bmp_count}"
        )

    return bmp_count


def create_backup(
    mtg_root,
    tokens,
):
    live_root = (
        mtg_root
        / "art"
    )

    backup_root = (
        mtg_root
        / "art-rev1-backup"
    )

    if backup_root.exists():
        print()

        print(
            "Existing Art Rev1 backup found:"
        )

        print(
            f"  {backup_root}"
        )

        print(
            "Leaving it untouched."
        )

        return backup_root

    if not live_root.exists():
        raise RuntimeError(
            f"Live art directory not found: {live_root}"
        )

    print()

    print(
        "Backing up current live artwork..."
    )

    shutil.copytree(
        live_root,
        backup_root,
    )

    # During our Atkinson test, Elemental's live BMP was
    # temporarily replaced. If its dedicated Rev1 backup is
    # still present, put that original version into the full
    # backup tree so art-rev1-backup is a true old-art snapshot.
    elemental = next(
        (
            token
            for token in tokens
            if token.get(
                "name",
                "",
            ).casefold().strip()
            == "elemental"
        ),
        None,
    )

    elemental_backup = (
        mtg_root
        / "art-test"
        / "elemental-art-rev1-backup.bmp"
    )

    if (
        elemental is not None
        and
        elemental_backup.exists()
    ):
        elemental_bucket = (
            token_bucket(
                elemental[
                    "name"
                ]
            )
        )

        elemental_destination = (
            backup_root
            / elemental_bucket
            / (
                f"{elemental['token_id']}"
                ".bmp"
            )
        )

        elemental_destination.parent.mkdir(
            parents=True,
            exist_ok=True,
        )

        shutil.copy2(
            elemental_backup,
            elemental_destination,
        )

        print(
            "Restored original Elemental BMP "
            "inside Rev1 backup."
        )

    print(
        f"Backup complete:"
    )

    print(
        f"  {backup_root}"
    )

    return backup_root


def install_rev2(
    tokens,
    stage_root,
    mtg_root,
):
    live_root = (
        mtg_root
        / "art"
    )

    live_root.mkdir(
        parents=True,
        exist_ok=True,
    )

    print()

    print(
        "=" * 60
    )

    print(
        "Installing Art Rev2 to SD card..."
    )

    installed = 0

    for token in tokens:
        token_id = (
            token.get(
                "token_id",
                "",
            )
            .strip()
        )

        name = (
            token.get(
                "name",
                "",
            )
            .strip()
        )

        bucket = token_bucket(
            name
        )

        source = (
            stage_root
            / bucket
            / f"{token_id}.bmp"
        )

        destination_dir = (
            live_root
            / bucket
        )

        destination_dir.mkdir(
            parents=True,
            exist_ok=True,
        )

        destination = (
            destination_dir
            / f"{token_id}.bmp"
        )

        shutil.copy2(
            source,
            destination,
        )

        installed += 1

        if (
            installed % 50 == 0
            or
            installed == len(tokens)
        ):
            print(
                f"Installed "
                f"{installed}/{len(tokens)}"
            )

    print()

    print(
        "Verifying live SD artwork..."
    )

    verified = verify_art_tree(
        tokens,
        live_root,
    )

    print(
        f"Verified {verified} live BMPs."
    )


def restore_rev1(
    mtg_root,
):
    live_root = (
        mtg_root
        / "art"
    )

    backup_root = (
        mtg_root
        / "art-rev1-backup"
    )

    if not backup_root.exists():
        raise RuntimeError(
            "No Art Rev1 backup exists:"
            f" {backup_root}"
        )

    print()

    print(
        "Restoring Art Rev1..."
    )

    if live_root.exists():
        shutil.rmtree(
            live_root
        )

    shutil.copytree(
        backup_root,
        live_root,
    )

    print()

    print(
        "Art Rev1 restored."
    )

    print(
        f"  {live_root}"
    )


def main():
    parser = argparse.ArgumentParser(
        description=(
            "Build the full XTEINK X3 MTG token "
            "Art Rev2 library."
        )
    )

    parser.add_argument(
        "sd_root",
        help=(
            "SD card root, for example H:\\"
        ),
    )

    parser.add_argument(
        "--workers",
        type=int,
        default=DEFAULT_WORKERS,
        help=(
            "Number of parallel Atkinson conversion "
            f"workers. Default: {DEFAULT_WORKERS}"
        ),
    )

    parser.add_argument(
        "--restore",
        action="store_true",
        help=(
            "Restore the backed-up Art Rev1 library "
            "instead of building Rev2."
        ),
    )

    args = parser.parse_args()

    sd_root = Path(
        args.sd_root
    )

    if not sd_root.exists():
        raise RuntimeError(
            f"SD card does not exist: {sd_root}"
        )

    mtg_root = (
        sd_root
        / "MTG"
    )

    master_path = (
        mtg_root
        / "data"
        / "tokens.tsv"
    )

    if args.restore:
        restore_rev1(
            mtg_root
        )

        return 0

    if not master_path.exists():
        raise RuntimeError(
            "Master token database not found: "
            f"{master_path}"
        )

    tokens = load_tokens(
        master_path
    )

    print()

    print(
        "XTEINK X3 MTG Token Art Rev2"
    )

    print(
        "=" * 60
    )

    print(
        f"Database:    {master_path}"
    )

    print(
        f"Tokens:      {len(tokens)}"
    )

    print(
        f"Resolution:  "
        f"{TARGET_WIDTH}x{TARGET_HEIGHT}"
    )

    print(
        "Dithering:   Atkinson"
    )

    print(
        "Crop:        None"
    )

    print(
        "Fit:         Stretch entire image"
    )

    if len(
        tokens
    ) != EXPECTED_TOKEN_COUNT:
        raise RuntimeError(
            "Token database count changed. "
            f"Expected {EXPECTED_TOKEN_COUNT}, "
            f"found {len(tokens)}. "
            "Refusing to replace the artwork until "
            "the database is checked."
        )

    if args.workers < 1:
        raise RuntimeError(
            "--workers must be at least 1"
        )

    source_paths = ensure_source_art(
        tokens
    )

    if len(
        source_paths
    ) != len(
        tokens
    ):
        raise RuntimeError(
            "Not every token has source artwork."
        )

    stage_root = get_stage_root()

    clear_stage(
        stage_root
    )

    build_all_art(
        tokens,
        source_paths,
        stage_root,
        args.workers,
    )

    print()

    print(
        "Verifying complete Art Rev2 build..."
    )

    verified = verify_art_tree(
        tokens,
        stage_root,
    )

    print(
        f"Verified {verified} staged BMPs."
    )

    # Do not touch live artwork until the complete Rev2
    # library has successfully built and verified.
    create_backup(
        mtg_root,
        tokens,
    )

    install_rev2(
        tokens,
        stage_root,
        mtg_root,
    )

    print()

    print(
        "=" * 60
    )

    print(
        "ART REV2 COMPLETE"
    )

    print(
        "=" * 60
    )

    print()

    print(
        f"{len(tokens)} token images installed."
    )

    print()

    print(
        "Format:"
    )

    print(
        f"  {TARGET_WIDTH}x{TARGET_HEIGHT}"
    )

    print(
        "  1-bit BMP"
    )

    print(
        "  Atkinson dithering"
    )

    print(
        "  Full art stretched to fit"
    )

    print(
        "  No cropping"
    )

    print()

    print(
        "Live artwork:"
    )

    print(
        f"  {mtg_root / 'art'}"
    )

    print()

    print(
        "Art Rev1 backup:"
    )

    print(
        f"  {mtg_root / 'art-rev1-backup'}"
    )

    print()

    print(
        "To restore Art Rev1 later:"
    )

    print(
        f'  py "{Path(__file__)}" '
        f'"{sd_root}" --restore'
    )

    return 0


if __name__ == "__main__":
    raise SystemExit(
        main()
    )