# DICE\\VESSEL v1.0.0 build

Compiled on 20 July 2026.

## Target

- Board: M5Stack Cardputer / StampS3
- Chip: ESP32-S3
- Flash: 8 MB
- Framework: Arduino for ESP32
- PlatformIO environment: `m5stack-cardputer`
- M5Cardputer: 1.1.1
- M5Unified: 0.2.18
- M5GFX: 0.2.25

## Memory

- RAM: 27,932 / 327,680 bytes — 8.5%
- Application flash: 540,041 / 3,342,336 bytes — 16.2%

## Release images

| File | Size | Flash offset | SHA-256 |
|---|---:|---:|---|
| `release/dicevessel-v1.0.0-factory.bin` | 605,936 bytes | `0x0` | `04D8EB9AE89931A74B2EE1D012B029928521E6C514041F2943EC715C338426A9` |
| `release/dicevessel-v1.0.0-firmware.bin` | 540,400 bytes | `0x10000` | `76C3FEF0DA9B26141F94A129435E4E3402283B454FA04330C00D7B69E8C81408` |

The factory image is for a clean installation. The firmware-only image updates an existing installation while preserving settings, named combinations, and history.
