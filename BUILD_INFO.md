# DICE\\VESSEL v0.4.0-beta build

Compiled on 18 July 2026.

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

- RAM: 25,900 / 327,680 bytes — 7.9%
- Application flash: 524,661 / 3,342,336 bytes — 15.7%

## Release images

| File | Size | Flash offset | SHA-256 |
|---|---:|---:|---|
| `release/dicevessel-v0.4-beta-factory.bin` | 590,560 bytes | `0x0` | `D76F455D7DA51EBA2DB1DC53D6E827C118C1F7A50264584038B0A2C0A33005C5` |
| `release/dicevessel-v0.4-beta-firmware.bin` | 525,024 bytes | `0x10000` | `5C1081625056E44509385E2F5E00B763E5A26F3522A902EF6816269AD973EB15` |

The factory image includes the bootloader, partition table, OTA boot selector, and application. It is the recommended image for a fresh installation.
