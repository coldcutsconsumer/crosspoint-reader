## Disclaimer

This is an independent hobby project.

CrossPoint MTG Token Board is not affiliated with or endorsed by Wizards of the Coast, Magic: The Gathering, Scryfall, XTEINK, or the CrossPoint Reader project.

Magic: The Gathering card names, artwork, rules text, and related intellectual property belong to their respective rights holders.

Token data and artwork used by the SD-card generation tools are obtained from Scryfall.

---

## Credits

### CrossPoint Reader

The overwhelming majority of the device firmware, hardware support, renderer infrastructure, reader functionality, and UI foundation comes from the CrossPoint Reader project and its contributors.

CrossPoint MTG Token Board would not exist without that work.

### Scryfall

Scryfall provides the card and token data used to generate the offline token library and artwork set.

### CrossPoint MTG Token Board


!!!!!!!CrossPoint MTG Token Board!!!!!!!

An MTG token and game-state board for the XTEINK X3 e-paper reader, built as a fork of CrossPoint Reader.

CrossPoint MTG Token Board turns the X3 into a compact four-slot Magic: The Gathering token display. Each slot can represent a different token, track quantity and power/toughness, display token artwork, calculate combined power/toughness, and provide quick access to rules text.

The original CrossPoint e-reader functionality remains available alongside the MTG Token Board.

**Current release: 1.2**

---

## Why?

Token-heavy Commander decks can become difficult to manage with physical token cards, counters, dice, and copies of the same creature.

The XTEINK X3 is a small ESP32-C3-based e-paper reader with an SD card slot and physical controls, which makes it a surprisingly good platform for a persistent tabletop token display.

The goal of this project is to provide a token board that is:

- Easy to read across the table
- Fast to update during a game
- Persistent between sessions
- Usable without Wi-Fi
- Capable of storing a large token library on inexpensive microSD storage
- Lightweight enough for the limited RAM available on the ESP32-C3

---

## Features

### Four-token board

The main screen contains four independently configurable token slots.

Each populated slot displays:

- Token name
- Token artwork
- Power/toughness
- Quantity
- Total combined power/toughness
- Rules-text availability indicator

For example, three 3/1 tokens display:

```text
3/1   [T]:9/3
x 3
```

`[T]` represents the combined power and toughness of every token in that slot.

If rules text is available for the token, the quantity row displays:

```text
*?*
```

---

## Persistent State

The token board saves its state to the microSD card.

The following survive leaving the MTG app or restarting the device:

- Selected token
- Quantity
- Edited power
- Edited toughness
- Artwork path
- Rules text

A new board starts with four empty slots.

Individual slots or the entire board can be cleared from the Edit menu.

---

## Token Zoom

Hold the main board's **Right [ZM]** button for approximately **1.3 seconds** to open Token Zoom for the selected slot.

Zoom mode presents the token in a card-like layout with:

- Token name
- Quantity
- Large artwork
- Power/toughness
- Total combined power/toughness
- Rules text

While Zoom is open, the physical side buttons can still adjust quantity.

Changes are reflected immediately in both quantity and total power/toughness.

Press **Back** to return to the four-token board.

---

## Controls

### Main Token Board

| Control | Action |
| --- | --- |
| Back | Return to CrossPoint home |
| Edit | Open Edit menu |
| Bottom-right Left | Previous token slot |
| Bottom-right Right | Next token slot |
| Hold Right ~1.3 sec | Token Zoom |
| Physical left side button | -1 quantity |
| Physical right side button | +1 quantity |

### Zoom

| Control | Action |
| --- | --- |
| Back | Return to token board |
| Physical left side button | -1 quantity |
| Physical right side button | +1 quantity |

---

## Edit Menu

Each token slot has an Edit menu containing:

```text
Change Card
Edit Power
Edit Toughness
Read Card Text
Clear Card
Clear Board
```

### Change Card

The token picker first displays an alphabet screen.

Select the first letter of the token name, then browse only tokens beginning with that letter.

This avoids loading or displaying the entire token database at once.

### Edit Power / Toughness

Power and toughness can be modified independently from the original token definition.

This is useful for effects that permanently or temporarily change a token's effective stats during a game.

### Read Card Text

Displays the stored Oracle/rules text for the selected token in a dedicated text view.

### Clear Card

Returns only the selected slot to Empty.

### Clear Board

Returns all four slots to Empty.

---

## Token Library

The current library contains **526 unique token names**.

The library intentionally contains only one entry for each token name.

For example, Magic may have many different Elemental token printings with different colors, power/toughness values, or artwork. The picker contains one canonical:

```text
Elemental
```

rather than dozens of Elemental variants.

Power and toughness are editable on the device, so duplicate token-name entries are unnecessary.

This keeps Change Card significantly faster and easier to navigate.

---

## Artwork

Token artwork is stored on the microSD card rather than compiled into firmware.

Current Art Rev2 format:

```text
250 x 250 pixels
1-bit BMP
Atkinson dithering
One artwork per unique token name
```

Artwork is generated from Scryfall art crops.

The source artwork is resized before being converted to 1-bit, avoiding additional dithering passes.

Atkinson dithering was selected after testing multiple conversion methods on the physical XTEINK X3 display.

The firmware includes a custom nearest-neighbor 1-bit renderer that can stretch the same 250x250 artwork to fit both:

- The smaller four-token board frames
- The much larger Token Zoom artwork frame

This allows one SD-card image to serve both interfaces.

---

## SD Card Layout

The MTG data is stored separately from normal CrossPoint files.

```text
/MTG/
├── data/
│   └── tokens.tsv
│
├── index/
│   ├── A.tsv
│   ├── B.tsv
│   ├── C.tsv
│   ├── ...
│   └── Z.tsv
│
├── art/
│   ├── A/
│   │   └── *.bmp
│   ├── B/
│   │   └── *.bmp
│   ├── ...
│   └── Z/
│       └── *.bmp
│
└── session/
    └── state.tsv
```

### Why the alphabet folders?

The ESP32-C3 has very limited RAM.

Rather than loading metadata for hundreds of tokens simultaneously, Change Card loads only the small index corresponding to the selected first letter.

Artwork itself is not loaded while browsing the picker.

Only after a token is selected does the firmware open its BMP from the SD card.

---

## Token Data

The per-letter token indexes contain information such as:

```text
token_id
name
power
toughness
colors
art_file
oracle_text
```

Rules text preserves line breaks so it can be displayed naturally by the Read Card Text and Token Zoom views.

---

## MTG Build Tools

Utility scripts for creating the SD-card library live under:

```text
tools/mtg/
```

These scripts are used to:

- Retrieve Scryfall bulk data
- Find token and emblem entries
- Deduplicate entries by token name
- Generate the master token database
- Generate alphabet indexes
- Download token artwork
- Resize artwork
- Apply Atkinson dithering
- Generate X3-compatible 1-bit BMP files

The resulting SD-card database and artwork are intentionally kept outside the firmware image.

---

## Building the Firmware

The project uses the same PlatformIO-based build system as CrossPoint Reader.

Typical build:

```powershell
pio run -e default
```

Build and upload to a connected device using the appropriate PlatformIO upload target for your XTEINK X3 development setup.

The MTG token database and artwork must be installed separately on the microSD card.

---

## Based on CrossPoint Reader

This project is a fork of **CrossPoint Reader**.

CrossPoint Reader is community-developed, open-source firmware for e-paper reading devices including the XTEINK X3 and X4.

It provides the underlying platform used by this project, including:

- XTEINK X3 hardware support
- E-paper rendering
- Physical button handling
- SD-card access
- UI framework and navigation
- Power management
- EPUB and document reading
- File browsing
- Settings and device infrastructure

CrossPoint MTG Token Board adds the MTG application while retaining the underlying CrossPoint reader experience.

Upstream project:

```text
crosspoint-reader/crosspoint-reader
```

If you are interested primarily in using the XTEINK device as an e-reader rather than an MTG token board, the upstream CrossPoint Reader project is the place to start.

---

## Project History

### 1.2

- Added Token Zoom
- Added 1.3-second Right-button Zoom shortcut
- Added large token artwork to Zoom
- Added quantity controls while Zoom is open
- Added calculated total P/T to Zoom
- Added rules text to Zoom
- Added rules-text availability indicator to token tiles
- Added `Right [ZM]` control hint
- Added true stretched 1-bit artwork rendering
- Standardized artwork on 250x250 Atkinson-dithered BMPs
- Expanded artwork to use more of the token tile
- Removed the old long-hold Edit text shortcut
- Simplified Zoom exit to Back

See `CHANGELOG.md` for the full release history.

---

## Upstream Compatibility

Because this project is a specialized CrossPoint Reader fork, upstream CrossPoint development may continue independently.

Where practical, the MTG functionality is kept isolated under:

```text
src/activities/mtg/
tools/mtg/
```

This should make future upstream merges easier and reduce unnecessary modifications to the rest of CrossPoint.

---



The MTG-specific application adds the token board, persistent token state, SD-backed token database, token picker, artwork processing pipeline, rules-text views, quantity/stat tracking, and Token Zoom interface.
