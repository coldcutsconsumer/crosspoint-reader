import csv
import gzip
import hashlib
import json
import os
import shutil
import sys
import urllib.request
from pathlib import Path


SCRYFALL_BULK_API = "https://api.scryfall.com/bulk-data/default_cards"

USER_AGENT = (
    "X3MTGTokenBuilder/0.1 "
    "(+https://github.com/coldcutsconsumer/crosspoint-reader)"
)

HEADERS = {
    "User-Agent": USER_AGENT,
    "Accept": "application/json;q=0.9,*/*;q=0.8",
}

TOKEN_LAYOUTS = {
    "token",
    "double_faced_token",
    "emblem",
}


def request(url):
    req = urllib.request.Request(url, headers=HEADERS)
    return urllib.request.urlopen(req)


def get_cache_dir():
    base = Path(os.environ.get("LOCALAPPDATA", Path.home()))
    path = base / "X3MTG"
    path.mkdir(parents=True, exist_ok=True)
    return path


def get_bulk_url():
    print("Checking Scryfall bulk-data manifest...")

    with request(SCRYFALL_BULK_API) as response:
        manifest = json.load(response)

    url = manifest.get("jsonl_download_uri")

    if not url:
        raise RuntimeError(
            "Scryfall manifest did not contain jsonl_download_uri"
        )

    print(f"Bulk data updated: {manifest.get('updated_at', 'unknown')}")
    return url


def download_bulk(url, destination):
    if destination.exists():
        print("Using cached bulk data:")
        print(f"  {destination}")
        return

    temp = destination.with_suffix(destination.suffix + ".part")

    print("Downloading Scryfall Default Cards bulk data...")
    print("This is a fairly large download and only needs to happen once.")

    with request(url) as response, open(temp, "wb") as output:
        shutil.copyfileobj(response, output)

    temp.replace(destination)

    print("Saved:")
    print(f"  {destination}")


def clean_text(value):
    if value is None:
        return ""

    return (
        str(value)
        .replace("\t", " ")
        .replace("\r", " ")
        .replace("\n", " ")
        .strip()
    )


def make_candidate(card, face=None):
    source = face if face is not None else card

    image_uris = (
        source.get("image_uris")
        or card.get("image_uris")
        or {}
    )

    name = clean_text(
        source.get("name", card.get("name", ""))
    )

    type_line = clean_text(
        source.get("type_line", card.get("type_line", ""))
    )

    oracle_text = clean_text(
        source.get("oracle_text", card.get("oracle_text", ""))
    )

    power = clean_text(
        source.get("power", card.get("power", ""))
    )

    toughness = clean_text(
        source.get("toughness", card.get("toughness", ""))
    )

    colors = source.get("colors")

    if colors is None:
        colors = card.get("colors", [])

    colors_text = "".join(colors or [])

    art_url = image_uris.get("art_crop", "")

    # One canonical library entry per token NAME.
    #
    # Power, toughness, and color will eventually be editable
    # properties of each active tile on the X3, so they do not
    # create separate entries in the token picker.
    normalized_name = name.casefold().strip()

    token_id = hashlib.sha1(
        normalized_name.encode("utf-8")
    ).hexdigest()[:16]

    return {
        "token_id": token_id,
        "name": name,
        "type_line": type_line,
        "power": power,
        "toughness": toughness,
        "colors": colors_text,
        "oracle_text": oracle_text,
        "scryfall_id": card.get("id", ""),
        "released_at": card.get("released_at", ""),
        "art_url": art_url,
        "art_file": f"art/{token_id}.bmp",
        "artist": clean_text(
            source.get("artist", card.get("artist", ""))
        ),
        "set": clean_text(card.get("set", "")),
        "collector_number": clean_text(
            card.get("collector_number", "")
        ),
    }


def is_token_like(candidate):
    type_line = candidate["type_line"]

    return (
        type_line == "Token"
        or type_line.startswith("Token ")
        or type_line == "Emblem"
        or type_line.startswith("Emblem ")
        or type_line.startswith("Emblem —")
    )


def iter_candidates(bulk_path):
    with gzip.open(bulk_path, "rt", encoding="utf-8") as handle:
        for line_number, line in enumerate(handle, start=1):
            if not line.strip():
                continue

            try:
                card = json.loads(line)
            except json.JSONDecodeError as exc:
                raise RuntimeError(
                    f"Invalid JSON on bulk-data line {line_number}"
                ) from exc

            layout = card.get("layout", "")

            if layout not in TOKEN_LAYOUTS:
                continue

            if layout == "double_faced_token":
                for face in card.get("card_faces", []):
                    candidate = make_candidate(card, face)

                    if is_token_like(candidate):
                        yield candidate
            else:
                candidate = make_candidate(card)

                if is_token_like(candidate):
                    yield candidate


def candidate_score(candidate):
    """
    Score same-name candidates so we choose a useful
    representative record/artwork for the library.

    This does NOT create separate token entries.
    """

    score = 0

    # Prefer proper typed tokens such as:
    # "Token Creature — Beast"
    # over generic objects whose type line is just "Token".
    if candidate["type_line"] not in ("Token", "Emblem"):
        score += 100

    # Creature tokens with printed/default stats make
    # convenient representatives.
    if candidate["power"]:
        score += 10

    if candidate["toughness"]:
        score += 10

    # Prefer a record containing useful rules text.
    if candidate["oracle_text"]:
        score += 5

    # Prefer records with artwork.
    if candidate["art_url"]:
        score += 1

    return score


def choose_tokens(bulk_path):
    tokens = {}
    raw_count = 0

    for candidate in iter_candidates(bulk_path):
        raw_count += 1

        # THIS is the important dedupe rule:
        # one entry for each human-visible token name.
        key = candidate["name"].casefold().strip()

        existing = tokens.get(key)

        if existing is None:
            tokens[key] = candidate
            continue

        candidate_rank = (
            candidate_score(candidate),
            candidate["released_at"],
        )

        existing_rank = (
            candidate_score(existing),
            existing["released_at"],
        )

        # If multiple Scryfall objects share the same name,
        # keep whichever makes the better representative.
        #
        # If they're otherwise tied, the newer printing wins.
        if candidate_rank > existing_rank:
            tokens[key] = candidate

    print()
    print(f"Raw token/emblem faces found: {raw_count}")
    print(f"Unique token names:           {len(tokens)}")

    return sorted(
        tokens.values(),
        key=lambda token: token["name"].casefold(),
    )


def write_tsv(tokens, destination):
    destination.parent.mkdir(parents=True, exist_ok=True)

    fields = [
        "token_id",
        "name",
        "type_line",
        "power",
        "toughness",
        "colors",
        "oracle_text",
        "art_file",
        "scryfall_id",
        "art_url",
        "artist",
        "set",
        "collector_number",
    ]

    with open(
        destination,
        "w",
        encoding="utf-8",
        newline="",
    ) as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=fields,
            delimiter="\t",
            lineterminator="\n",
        )

        writer.writeheader()

        for token in tokens:
            writer.writerow(
                {
                    field: token.get(field, "")
                    for field in fields
                }
            )

    print()
    print("Wrote token database:")
    print(f"  {destination}")


def print_examples(tokens, count=25):
    print()
    print("First entries:")
    print("-" * 60)

    for token in tokens[:count]:
        stats = ""

        if token["power"] or token["toughness"]:
            stats = (
                f" {token['power']}/{token['toughness']}"
            )

        colors = ""

        if token["colors"]:
            colors = f" [{token['colors']}]"

        print(
            f"{token['name']}"
            f"{stats}"
            f"{colors}"
            f"  ({token['type_line']})"
        )


def main():
    if len(sys.argv) != 2:
        print(
            "Usage: py build_token_library.py "
            "<SD-card drive letter>"
        )
        print()
        print("Example:")
        print("  py build_token_library.py H:")
        return 2

    sd_root = Path(sys.argv[1])

    if not sd_root.exists():
        raise RuntimeError(
            f"SD card path does not exist: {sd_root}"
        )

    mtg_root = sd_root / "MTG"

    if not mtg_root.exists():
        raise RuntimeError(
            f"Expected MTG directory was not found: {mtg_root}"
        )

    cache_dir = get_cache_dir()

    bulk_path = (
        cache_dir
        / "scryfall-default-cards.jsonl.gz"
    )

    bulk_url = get_bulk_url()

    download_bulk(
        bulk_url,
        bulk_path,
    )

    tokens = choose_tokens(bulk_path)

    output = (
        mtg_root
        / "data"
        / "tokens.tsv"
    )

    write_tsv(
        tokens,
        output,
    )

    print_examples(tokens)

    print()
    print("Artwork has NOT been downloaded yet.")
    print("Database generation complete.")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())