# Flashing DICE\\VESSEL

## English

### Complete factory image

The recommended file is `release/dicevessel-v1.0.0-factory.bin`.

- Chip: ESP32-S3
- Flash size: 8 MB
- Flash mode: DIO
- Flash frequency: 80 MHz
- Offset: `0x0`

Example with esptool:

```bash
esptool.py --chip esp32s3 --port COM3 --baud 460800 write_flash \
  --flash_mode dio --flash_freq 80m --flash_size 8MB \
  0x0 release/dicevessel-v1.0.0-factory.bin
```

Replace `COM3` with the serial port used by your Cardputer.

### PlatformIO upload

```bash
pio run -e m5stack-cardputer -t upload
```

## Português (Brasil)

### Imagem completa

O arquivo recomendado é `release/dicevessel-v1.0.0-factory.bin`.

- Chip: ESP32-S3
- Flash: 8 MB
- Modo: DIO
- Frequência: 80 MHz
- Offset: `0x0`

Exemplo com esptool:

```bash
esptool.py --chip esp32s3 --port COM3 --baud 460800 write_flash \
  --flash_mode dio --flash_freq 80m --flash_size 8MB \
  0x0 release/dicevessel-v1.0.0-factory.bin
```

Substitua `COM3` pela porta serial usada pelo seu Cardputer.

### Envio pelo PlatformIO

```bash
pio run -e m5stack-cardputer -t upload
```
