import argparse
import csv
import shutil
import string
from collections import defaultdict
from pathlib import Path


INDEX_FIELDS = [
    "token_id",
    "name",
    "power",
    "toughness",
    "colors",
    "art_file",
    "oracle_text",
]


def bucket_for_name(name):
    name = name.strip()

    if not name:
        return "#"

    first = name[0].upper()

    if (
        first
        in string.ascii_uppercase
    ):
        return first

    return "#"


def load_tokens(
    master_path,
):
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


def write_letter_index(
    destination,
    tokens,
):
    with open(
        destination,
        "w",
        encoding="utf-8",
        newline="",
    ) as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=INDEX_FIELDS,
            delimiter="\t",
            lineterminator="\n",
            quoting=csv.QUOTE_MINIMAL,
        )

        writer.writeheader()

        for token in tokens:
            writer.writerow(
                {
                    field:
                        token.get(
                            field,
                            "",
                        )
                    for field
                    in INDEX_FIELDS
                }
            )


def main():
    parser = argparse.ArgumentParser(
        description=(
            "Reorganize the X3 MTG SD library into "
            "per-letter indexes and art folders."
        )
    )

    parser.add_argument(
        "sd_root",
        help=(
            "SD card root, for example H:\\"
        ),
    )

    args = parser.parse_args()

    sd_root = Path(
        args.sd_root
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

    index_root = (
        mtg_root
        / "index"
    )

    art_root = (
        mtg_root
        / "art"
    )

    if not sd_root.exists():
        raise RuntimeError(
            "SD card does not exist: "
            f"{sd_root}"
        )

    if not master_path.exists():
        raise RuntimeError(
            "Master token database not found: "
            f"{master_path}"
        )

    if not art_root.exists():
        raise RuntimeError(
            "Artwork directory not found: "
            f"{art_root}"
        )

    tokens = load_tokens(
        master_path
    )

    print()

    print(
        "X3 MTG SD Layout Migration"
    )

    print(
        "=" * 60
    )

    print(
        f"Master database: "
        f"{master_path}"
    )

    print(
        f"Tokens found:    "
        f"{len(tokens)}"
    )

    print()

    if not tokens:
        raise RuntimeError(
            "Master database contains no tokens."
        )

    index_root.mkdir(
        parents=True,
        exist_ok=True,
    )

    # Rebuild generated per-letter indexes from scratch.
    for old_index in index_root.glob(
        "*.tsv"
    ):
        old_index.unlink()

    buckets = defaultdict(
        list
    )

    copied_count = 0
    missing_count = 0

    for token in tokens:
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

        if (
            not name
            or
            not token_id
        ):
            print(
                "SKIP malformed record: "
                f"name={name!r} "
                f"id={token_id!r}"
            )

            continue

        bucket = bucket_for_name(
            name
        )

        destination_dir = (
            art_root
            / bucket
        )

        destination_dir.mkdir(
            parents=True,
            exist_ok=True,
        )

        old_art = (
            art_root
            / f"{token_id}.bmp"
        )

        new_art = (
            destination_dir
            / f"{token_id}.bmp"
        )

        if new_art.exists():
            # Already in the new folder layout.
            pass

        elif old_art.exists():
            # Copy before deleting anything.
            shutil.copy2(
                old_art,
                new_art,
            )

            copied_count += 1

        else:
            print(
                "MISSING ART: "
                f"{name} "
                f"({token_id}.bmp)"
            )

            missing_count += 1

        index_record = {
            "token_id":
                token_id,

            "name":
                name,

            "power":
                token.get(
                    "power",
                    "",
                ),

            "toughness":
                token.get(
                    "toughness",
                    "",
                ),

            "colors":
                token.get(
                    "colors",
                    "",
                ),

            "art_file": (
                f"/MTG/art/"
                f"{bucket}/"
                f"{token_id}.bmp"
            ),

            # Preserve the exact multiline Oracle text
            # produced by build_token_library.py.
            "oracle_text":
                token.get(
                    "oracle_text",
                    "",
                ),
        }

        buckets[
            bucket
        ].append(
            index_record
        )

    # Alphabetize every letter bucket.
    for bucket_tokens in (
        buckets.values()
    ):
        bucket_tokens.sort(
            key=lambda token:
                token[
                    "name"
                ].casefold()
        )

    # Always make A-Z indexes, even when a letter has
    # no entries.
    for letter in (
        string.ascii_uppercase
    ):
        letter_tokens = (
            buckets.get(
                letter,
                [],
            )
        )

        destination = (
            index_root
            / f"{letter}.tsv"
        )

        write_letter_index(
            destination,
            letter_tokens,
        )

    # Non A-Z names get their own bucket.
    hash_tokens = (
        buckets.get(
            "#",
            [],
        )
    )

    hash_path = (
        index_root
        / "#.tsv"
    )

    if hash_tokens:
        write_letter_index(
            hash_path,
            hash_tokens,
        )

    elif hash_path.exists():
        hash_path.unlink()

    # Verify all expected images exist in the new layout.
    verify_missing = []

    for (
        bucket,
        bucket_tokens,
    ) in buckets.items():
        for token in (
            bucket_tokens
        ):
            expected = (
                art_root
                / bucket
                / (
                    f"{token['token_id']}"
                    ".bmp"
                )
            )

            if not expected.exists():
                verify_missing.append(
                    expected
                )

    if verify_missing:
        print()

        print(
            "Migration verification FAILED."
        )

        print(
            "Missing files: "
            f"{len(verify_missing)}"
        )

        for path in (
            verify_missing[:20]
        ):
            print(
                f"  {path}"
            )

        print()

        print(
            "Original flat BMP files have NOT "
            "been deleted."
        )

        return 1

    # All images are verified. Any old flat copies can
    # safely be removed.
    deleted_count = 0

    for old_art in art_root.glob(
        "*.bmp"
    ):
        old_art.unlink()

        deleted_count += 1

    print()

    print(
        "=" * 60
    )

    print(
        "Migration successful."
    )

    print()

    print(
        f"Token records:         "
        f"{len(tokens)}"
    )

    print(
        f"Artwork copied:        "
        f"{copied_count}"
    )

    print(
        f"Old flat BMPs removed: "
        f"{deleted_count}"
    )

    print(
        f"Missing artwork:       "
        f"{missing_count}"
    )

    print()

    print(
        "Letter counts:"
    )

    print(
        "-" * 60
    )

    for letter in (
        string.ascii_uppercase
    ):
        count = len(
            buckets.get(
                letter,
                [],
            )
        )

        print(
            f"{letter}: "
            f"{count:>3}"
        )

    if hash_tokens:
        print(
            f"#: "
            f"{len(hash_tokens):>3}"
        )

    print()

    print(
        "New layout:"
    )

    print(
        f"  {index_root}"
    )

    print(
        f"  {art_root}"
    )

    print()

    print(
        "Per-letter indexes now include "
        "multiline oracle_text."
    )

    return 0


if __name__ == "__main__":
    raise SystemExit(
        main()
    )