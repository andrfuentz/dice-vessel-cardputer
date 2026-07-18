# DICE\\VESSEL

**KEEP ROLLING.**

[Português (Brasil)](README.pt-BR.md)

![PlatformIO](https://img.shields.io/badge/PlatformIO-ESP32--S3-orange)
![Hardware](https://img.shields.io/badge/M5Stack-Cardputer-amber)
![Release](https://img.shields.io/badge/release-v0.4--beta-yellow)
![License](https://img.shields.io/badge/license-MIT-blue)

A pocket campaign companion for fast, flexible dice rolls on the M5Stack Cardputer.

DICE\\VESSEL helps keep the game moving. Build mixed dice expressions through a guided workflow, save recurring combinations, review recent rolls, and roll with a key or—on supported hardware—a shake. Fluid animation and responsive sound add character without slowing down the session.

![DICE\\VESSEL startup screen](docs/screenshots/splash.png)

## Highlights

- Guided roll builder: die type, quantity, final bonus or penalty, and confirmation.
- Direct expressions such as `2D20+1`, `3D6-2`, and `1D20+1D8+5`.
- D2, D4, D6, D8, D10, D12, D20, and D100.
- Click-to-roll on every Cardputer.
- Shake-to-roll when an IMU is available.
- Hardware RNG selected only at release; gesture strength never changes probability.
- Cinematic 2D dice motion, circular collisions, wall impacts, and a full-box representation.
- Procedural wooden-box impact audio, dedicated coin sound, and critical/failure feedback.
- Eight persistent saved-combination slots.
- Ten-roll session history.
- English and Brazilian Portuguese interface.
- Tabbed settings, 0–10 controls, instructions, About screen, and animated charging mode.

## Interface

| English | Português |
|---|---|
| ![English roller](docs/screenshots/roller-en.png) | ![Rolador em português](docs/screenshots/roller-pt.png) |
| ![English roll builder](docs/screenshots/builder-en.png) | ![Assistente em português](docs/screenshots/builder-pt.png) |
| ![English settings](docs/screenshots/options-en.png) | ![Opções em português](docs/screenshots/options-pt.png) |

## Quick controls

| Key | Action |
|---|---|
| `Enter` | Roll the current expression |
| `R` | Open the guided roll builder |
| `C` | Open saved combinations |
| `H` | Open session history |
| Alphanumeric keys | Type an expression directly |
| `Backspace` | Delete the last character |
| `Tab` | Select the next guided field |
| `[` / `]` | Decrease or increase the selected field |
| `` ` `` | Open options or go back |

On the Cardputer keyboard, `Fn+L` / `Fn+M` move up/down and `Fn+N` / `Fn+,` move left/right. Standard HID arrows and Escape are accepted when available.

Change language under **Options → System → Language**.

## Install

The easiest test installation uses the complete image from the GitHub release:

1. Download `dicevessel-v0.4-beta-factory.bin`.
2. Connect the Cardputer by USB.
3. Flash the file at offset `0x0` with an ESP32-S3 compatible tool.
4. Restart the device.

Detailed instructions and component offsets are in [Flashing Guide](docs/FLASHING.md).

## Build from source

Install [PlatformIO](https://platformio.org/) and run:

```bash
pio run -e m5stack-cardputer
pio run -e m5stack-cardputer -t upload
```

The target is M5Stack StampS3 / Cardputer with the Arduino framework. Build dependencies are declared in `platformio.ini`.

## Project structure

```text
include/           Data models and module interfaces
src/               UI, parser, physics, audio, storage, and application flow
docs/screenshots/  Pixel-accurate interface captures used by the README
release/           Ready-to-upload release binaries
```

## Current status

This is a beta build intended for hardware testing. Current limitations include:

- session history is not persistent after power-off;
- saved combinations use numbered slots without custom names;
- D100 still uses a compact percentage presentation;
- the final hand-drawn sprite atlas remains planned;
- Cardputer ADV motion calibration needs more dedicated testing;
- advantage/disadvantage, exploding dice, and success pools are not implemented yet.

See [CHANGELOG.md](CHANGELOG.md) and [ROADMAP.md](docs/ROADMAP.md).

## License

DICE\\VESSEL is released under the [MIT License](LICENSE).

## Credits

- Concept: Andre Fuentes — [@anfuentz](https://github.com/anfuentz)
- Vibecoded by Codex

> “We are the music makers, and we are the dreamers of dreams.”
