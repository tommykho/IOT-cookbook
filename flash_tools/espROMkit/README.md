# espROMkit — ESP32 Flash & Backup Tool

CLI and GUI tools to back up and restore the flash ROM of ESP32-based Arduino microcontrollers using [esptool.py](https://github.com/espressif/esptool).

**Version**: 2026.02A
**Author**: tommyho510@gmail.com

## Features

1. **Auto-detect** the COM/serial port of the connected controller
2. **Read chip info** and MAC address to confirm the device
3. **Backup** — read the entire flash ROM to a `.bin` file
4. **Restore** — write a `.bin` file back to flash
5. **Reboot** the controller after the operation

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

## Usage

### CLI

```bash
python espromkit_cli.py
```

The CLI walks you through each step interactively:

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
  Chip     : ESP32-PICO-D4 (revision v1.0)
  Device   : M5StickC / M5StickC-Plus (ESP32-PICO-D4)
  Features : WiFi, BT, Dual Core, 240MHz, Embedded Flash
  MAC      : d4:d4:da:98:66:d0

  Is this the correct device? [Y/n]: y

[4/6] Select action
  [1] Backup  — Read flash ROM to a .bin file
  [2] Restore — Write a .bin file to flash ROM

  Select action [1/2]: 1

[5/6] Backup — reading flash ROM...
  Output filename [d4d4da9866d0_20260213_143022.bin]:

[6/6] Rebooting device...
  Device rebooted successfully.

Done. espROMkit finished successfully.
```

### GUI

```bash
python espromkit_gui.py
```

The GUI provides:
- Port selection dropdown with **Refresh** button
- **Detect Device** to read and display chip info
- **Backup ROM** button with a file-save dialog
- **Restore ROM** button with a file-open dialog
- **Reboot Device** button
- Baud rate and flash size selectors
- Scrollable log output showing esptool progress

## Flash Parameters

Default values (matching the project's `command.txt` reference):

| Parameter   | Value   |
|-------------|---------|
| Baud rate   | 1500000 |
| Flash mode  | dio     |
| Flash freq  | 80m     |
| Flash size  | detect  |
| Offset      | 0x0     |

## File Structure

```
espROMkit/
├── espromkit_cli.py     # Command-line interface
├── espromkit_gui.py     # Tkinter graphical interface
├── requirements.txt     # Python dependencies
└── README.md            # This file
```
