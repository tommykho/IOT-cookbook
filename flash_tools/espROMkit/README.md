# espROMkit — ESP32 Flash & Backup Tool

CLI and GUI tools to back up and restore the flash ROM of ESP32-based Arduino microcontrollers using [esptool.py](https://github.com/espressif/esptool).

**Version**: 2026.02A
**Author**: tommyho510@gmail.com

## Features

1. **Auto-detect** the COM/serial port of the connected controller
2. **Read chip info** and MAC address to confirm the device
3. **Partition-aware backup** — full ROM, individual partitions, or app-only
4. **Partition-aware restore** — full ROM, bootloader+app, app-only, or custom offset
5. **Reboot** the controller after the operation

## ESP32 Flash Layout

ESP32 microcontrollers use a bootloader and partition table. The standard layout:

```
Address    Region              Size     Description
────────   ──────────────────  ───────  ──────────────────────────────────────
0x1000     Bootloader          28 KB    Second-stage bootloader
0x8000     Partition Table      4 KB    Describes flash partition layout
0xe000     OTA Data             8 KB    Over-the-air update state (if used)
0x10000    Application         ~4 MB*   Your Arduino sketch / firmware
```

\* Application size = total flash minus 0x10000. On a 4MB flash chip, this is ~3.9MB.

Both tools support flashing at correct offsets — you can back up and restore the bootloader and application as separate files, or operate on the full ROM as a single image.

## Tested Devices

- M5Stack M5StickC (ESP32-PICO-D4)
- M5Stack M5StickC-Plus (ESP32-PICO-D4)
- Should work with any ESP32/ESP8266 board supported by esptool.py

## Requirements

- Python 3.8+
- [esptool](https://github.com/espressif/esptool) >= 4.0
- [pyserial](https://pypi.org/project/pyserial/) >= 3.5
- **GUI only**: tkinter (usually included with Python; on Linux install `python3-tk`)

Install dependencies:
```bash
pip install -r requirements.txt
```

On Linux, install tkinter for the GUI:
```bash
sudo apt install python3-tk
```

> **Note on esptool.exe**: On Windows, Espressif provides a standalone `esptool.exe`. This project uses the Python `esptool` package instead (installed via pip), which works cross-platform (Windows, macOS, Linux). The `esptool.exe` standalone and the `pip install esptool` Python package are both official and use the same underlying code.

## Usage

### CLI

```bash
python espromkit_cli.py
```

The CLI walks you through 6 steps interactively:

```
==============================================================
  espROMkit CLI — ESP32 Flash & Backup Tool
  Version 2026.02A
==============================================================

[1/6] Detecting serial ports...
  Found 1 likely ESP32 port(s):
    [1] /dev/ttyUSB0  —  CP2104 USB to UART Bridge Controller
  Auto-selected: /dev/ttyUSB0

[2/6] Reading chip info on /dev/ttyUSB0...

[3/6] Device identification
  Chip      : ESP32-PICO-D4 (revision v1.0)
  Device    : M5StickC / M5StickC-Plus (ESP32-PICO-D4)
  Features  : WiFi, BT, Dual Core, 240MHz, Embedded Flash
  MAC       : d4:d4:da:98:66:d0
  Flash Size: 4MB

  ESP32 flash layout:
    0x1000   — Bootloader
    0x8000   — Partition table
    0xe000   — OTA data
    0x10000  — Application firmware

  Is this the correct device? [Y/n]: y

[4/6] Select action
  [1] Backup  — Read flash ROM to .bin file(s)
  [2] Restore — Write .bin file(s) to flash ROM

  Select action [1/2]: 1

[5/6] Backup — reading flash ROM...

  Backup mode:
  [1] Full ROM       — entire flash as a single .bin (recommended)
  [2] Partitions     — bootloader + partition table + app as separate files
  [3] App only       — application firmware only (0x10000+)

  Select backup mode [1/2/3]: 2

  Will back up 3 partitions:
    bootloader            0x01000      28,672 bytes
    partition_table       0x08000       4,096 bytes
    application           0x10000   4,128,768 bytes

[6/6] Rebooting device...
  Device rebooted successfully.

Done. espROMkit finished successfully.
```

#### Restore modes

When restoring, you can choose:

| Mode | Description | Files needed |
|------|-------------|--------------|
| Full ROM | Single .bin at 0x0 | 1 full-ROM backup file |
| Bootloader+App | Two .bin files at 0x1000 and 0x10000 | Bootloader + Application |
| App only | Single .bin at 0x10000 | Application firmware only |
| Custom offset | Any .bin at any offset | 1 file + manual offset |

### GUI

```bash
python espromkit_gui.py
```

The GUI provides:
- Port selection dropdown with **Refresh** button
- **Detect Device** to read and display chip info and flash size
- Backup/Restore **mode radio buttons** for partition-aware operations
- **Backup ROM** with file-save dialog (full, partitions, or app-only)
- **Restore ROM** with file-open dialog (full, bootloader+app, app-only, or custom offset)
- **Reboot Device** button
- Baud rate and flash size selectors
- ESP32 flash layout reference bar
- Scrollable log output showing esptool progress

## Flash Parameters

Default values (matching the project's `command.txt` reference):

| Parameter   | Value   |
|-------------|---------|
| Baud rate   | 1500000 |
| Flash mode  | dio     |
| Flash freq  | 80m     |
| Flash size  | detect  |

## File Structure

```
espROMkit/
├── espromkit_cli.py     # Command-line interface
├── espromkit_gui.py     # Tkinter graphical interface
├── requirements.txt     # Python dependencies
└── README.md            # This file
```
