#!/usr/bin/env python3
"""
espROMkit CLI — Flash and backup tool for ESP32 microcontrollers
Version: 2026.02A
Board: M5StickC, M5StickC-Plus, and other ESP32 devices
Author: tommyho510@gmail.com

Uses esptool.py (https://github.com/espressif/esptool) to detect, back up,
restore, and reboot ESP32-based Arduino microcontrollers via the command line.

ESP32 Flash Layout (typical):
  0x1000   — Bootloader (second-stage)
  0x8000   — Partition table
  0xe000   — OTA data (if applicable)
  0x10000  — Application firmware
"""

import sys
import os
import time
from datetime import datetime

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    sys.exit("ERROR: pyserial is required. Install with: pip install pyserial")

try:
    import esptool
except ImportError:
    sys.exit("ERROR: esptool is required. Install with: pip install esptool")


# Default flash parameters (matching command.txt reference)
DEFAULT_BAUD = 1500000
DEFAULT_FLASH_MODE = "dio"
DEFAULT_FLASH_FREQ = "80m"
DEFAULT_FLASH_SIZE = "detect"

# Standard ESP32 flash partition offsets
ESP32_PARTITIONS = {
    "bootloader":      {"offset": 0x1000,  "size": 0x7000,   "label": "Bootloader"},
    "partition_table":  {"offset": 0x8000,  "size": 0x1000,   "label": "Partition Table"},
    "ota_data":        {"offset": 0xe000,  "size": 0x2000,   "label": "OTA Data"},
    "application":     {"offset": 0x10000, "size": None,      "label": "Application Firmware"},
}

# Known ESP32 chip descriptions for M5StickC / M5StickC-Plus
KNOWN_DEVICES = {
    "ESP32-PICO-D4": "M5StickC / M5StickC-Plus (ESP32-PICO-D4)",
    "ESP32-PICO-V3": "M5StickC-Plus2 (ESP32-PICO-V3)",
    "ESP32": "Generic ESP32 module",
    "ESP32-S3": "ESP32-S3 module (e.g. LilyGO T-Display S3)",
    "ESP32-S2": "ESP32-S2 module",
    "ESP32-C3": "ESP32-C3 module",
}

FLASH_SIZE_BYTES = {
    "1MB": 0x100000,
    "2MB": 0x200000,
    "4MB": 0x400000,
    "8MB": 0x800000,
    "16MB": 0x1000000,
}


def print_banner():
    print("=" * 60)
    print("  espROMkit CLI — ESP32 Flash & Backup Tool")
    print("  Version 2026.02A")
    print("=" * 60)
    print()


def detect_ports():
    """Detect serial ports that likely have an ESP32 device attached."""
    ports = serial.tools.list_ports.comports()
    esp_ports = []
    other_ports = []

    for port in sorted(ports, key=lambda p: p.device):
        desc = (port.description or "").lower()
        vid = port.vid
        # Common USB-to-serial chips used by ESP32 dev boards
        # CP210x (Silicon Labs): VID 0x10C4
        # CH340/CH341: VID 0x1A86
        # FTDI: VID 0x0403
        # ESP32-S2/S3 native USB: VID 0x303A
        is_esp = (
            vid in (0x10C4, 0x1A86, 0x0403, 0x303A)
            or "cp210" in desc
            or "ch340" in desc
            or "ch910" in desc
            or "ftdi" in desc
            or "usb serial" in desc
            or "usb-serial" in desc
        )
        entry = {
            "device": port.device,
            "description": port.description or "Unknown",
            "hwid": port.hwid or "",
            "vid": vid,
            "pid": port.pid,
        }
        if is_esp:
            esp_ports.append(entry)
        else:
            other_ports.append(entry)

    return esp_ports, other_ports


def select_port():
    """Auto-detect and let the user pick a serial port."""
    print("[1/6] Detecting serial ports...")
    esp_ports, other_ports = detect_ports()

    if not esp_ports and not other_ports:
        print("  ERROR: No serial ports found.")
        print("  - Check that the device is connected via USB.")
        print("  - Install the correct USB driver (CP210x or CH340).")
        sys.exit(1)

    all_ports = esp_ports + other_ports

    if esp_ports:
        print(f"  Found {len(esp_ports)} likely ESP32 port(s):")
        for i, p in enumerate(esp_ports):
            print(f"    [{i + 1}] {p['device']}  —  {p['description']}")
    if other_ports:
        offset = len(esp_ports)
        print(f"  Other ports ({len(other_ports)}):")
        for i, p in enumerate(other_ports):
            print(f"    [{offset + i + 1}] {p['device']}  —  {p['description']}")

    if len(all_ports) == 1:
        chosen = all_ports[0]
        print(f"  Auto-selected: {chosen['device']}")
        return chosen["device"]

    while True:
        try:
            choice = input(f"\n  Select port [1-{len(all_ports)}]: ").strip()
            idx = int(choice) - 1
            if 0 <= idx < len(all_ports):
                chosen = all_ports[idx]
                print(f"  Selected: {chosen['device']}")
                return chosen["device"]
        except (ValueError, EOFError):
            pass
        print("  Invalid selection, try again.")


def run_esptool(args):
    """Run esptool with the given argument list. Returns exit code."""
    try:
        esptool.main(args)
        return 0
    except SystemExit as e:
        return e.code if e.code else 0
    except Exception as e:
        print(f"  esptool error: {e}")
        return 1


def get_chip_info(port):
    """Read chip info and MAC address using esptool."""
    print(f"\n[2/6] Reading chip info on {port}...")
    print(f"  Baud rate: {DEFAULT_BAUD}")

    import io
    old_stdout = sys.stdout
    sys.stdout = captured = io.StringIO()

    rc = run_esptool([
        "--port", port,
        "--baud", str(DEFAULT_BAUD),
        "--before", "default_reset",
        "--after", "no_reset",
        "flash_id",
    ])

    output = captured.getvalue()
    sys.stdout = old_stdout

    if rc != 0:
        print("  ERROR: Could not read chip info.")
        print("  - Is the device connected and powered on?")
        print("  - Try pressing the reset button while connecting.")
        print()
        print("  Raw output:")
        for line in output.strip().splitlines():
            print(f"    {line}")
        sys.exit(1)

    info = {
        "chip": "Unknown",
        "features": "",
        "mac": "",
        "crystal": "",
        "chip_id": "",
        "flash_size": "",
        "flash_id": "",
    }

    for line in output.splitlines():
        s = line.strip()
        if s.startswith("Chip is "):
            info["chip"] = s[len("Chip is "):]
        elif s.startswith("Features:"):
            info["features"] = s[len("Features:"):].strip()
        elif s.startswith("MAC:"):
            info["mac"] = s[len("MAC:"):].strip()
        elif s.startswith("Crystal is"):
            info["crystal"] = s[len("Crystal is"):].strip()
        elif s.startswith("Chip ID:"):
            info["chip_id"] = s[len("Chip ID:"):].strip()
        elif "Auto-detected Flash size" in s:
            info["flash_size"] = s.split(":")[-1].strip()
        elif s.startswith("Detected flash size:"):
            info["flash_size"] = s.split(":")[-1].strip()
        elif s.startswith("Manufacturer:"):
            info["flash_id"] = s

    return info


def confirm_device(info):
    """Display chip info and ask user to confirm."""
    print("\n[3/6] Device identification")

    chip_base = info["chip"].split(" (")[0] if " (" in info["chip"] else info["chip"]
    friendly = KNOWN_DEVICES.get(chip_base, info["chip"])

    print(f"  Chip      : {info['chip']}")
    print(f"  Device    : {friendly}")
    print(f"  Features  : {info['features']}")
    print(f"  Crystal   : {info['crystal']}")
    print(f"  MAC       : {info['mac']}")
    if info["chip_id"]:
        print(f"  Chip ID   : {info['chip_id']}")
    if info["flash_size"]:
        print(f"  Flash Size: {info['flash_size']}")

    print()
    print("  ESP32 flash layout:")
    print("    0x1000   — Bootloader")
    print("    0x8000   — Partition table")
    print("    0xe000   — OTA data")
    print("    0x10000  — Application firmware")

    while True:
        confirm = input("\n  Is this the correct device? [Y/n]: ").strip().lower()
        if confirm in ("", "y", "yes"):
            return True
        if confirm in ("n", "no"):
            print("  Aborted by user.")
            sys.exit(0)
        print("  Please enter Y or N.")


def choose_action():
    """Ask user whether to backup or restore."""
    print("\n[4/6] Select action")
    print("  [1] Backup  — Read flash ROM to .bin file(s)")
    print("  [2] Restore — Write .bin file(s) to flash ROM")

    while True:
        try:
            choice = input("\n  Select action [1/2]: ").strip()
            if choice == "1":
                return "backup"
            if choice == "2":
                return "restore"
        except EOFError:
            sys.exit(1)
        print("  Invalid selection, enter 1 or 2.")


def choose_backup_mode(info):
    """Ask user what to back up."""
    print("\n  Backup mode:")
    print("  [1] Full ROM       — entire flash as a single .bin (recommended)")
    print("  [2] Partitions     — bootloader + partition table + app as separate files")
    print("  [3] App only       — application firmware only (0x10000+)")

    while True:
        try:
            choice = input("\n  Select backup mode [1/2/3]: ").strip()
            if choice in ("1", "2", "3"):
                return choice
        except EOFError:
            sys.exit(1)
        print("  Invalid selection.")


def _make_filename(info, suffix):
    """Generate a default filename from MAC and timestamp."""
    mac_slug = info.get("mac", "unknown").replace(":", "")
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    return f"{mac_slug}_{timestamp}_{suffix}.bin"


def _ask_filename(default_name):
    """Prompt for output filename."""
    user_name = input(f"  Output filename [{default_name}]: ").strip()
    if not user_name:
        user_name = default_name
    if not user_name.lower().endswith(".bin"):
        user_name += ".bin"
    return os.path.abspath(user_name)


def _read_flash_region(port, offset, size, output_path):
    """Read a region of flash to a file. Returns True on success."""
    print(f"  Reading 0x{offset:X}..0x{offset + size:X} ({size:,} bytes) -> {output_path}")

    rc = run_esptool([
        "--port", port,
        "--baud", str(DEFAULT_BAUD),
        "--before", "default_reset",
        "--after", "no_reset",
        "read_flash",
        hex(offset), str(size),
        output_path,
    ])

    if rc != 0:
        print(f"  ERROR: Failed to read region at 0x{offset:X}.")
        return False

    if os.path.exists(output_path):
        fsize = os.path.getsize(output_path)
        print(f"  OK: {output_path} ({fsize:,} bytes)")
        return True

    print(f"  ERROR: File was not created.")
    return False


def do_backup(port, info):
    """Back up flash ROM to one or more .bin files."""
    print("\n[5/6] Backup — reading flash ROM...")

    flash_size_str = info.get("flash_size", "4MB")
    total_bytes = FLASH_SIZE_BYTES.get(flash_size_str, 0x400000)

    mode = choose_backup_mode(info)
    print()

    if mode == "1":
        # Full ROM
        default_name = _make_filename(info, "full")
        output_path = _ask_filename(default_name)
        print(f"  Reading full ROM ({flash_size_str}, {total_bytes:,} bytes)...")
        print(f"  This may take a few minutes.\n")
        return _read_flash_region(port, 0x0, total_bytes, output_path)

    elif mode == "2":
        # Partition-aware: bootloader + partition table + app
        app_size = total_bytes - ESP32_PARTITIONS["application"]["offset"]

        parts = [
            ("bootloader",     0x1000, 0x7000,   _make_filename(info, "bootloader")),
            ("partition_table", 0x8000, 0x1000,   _make_filename(info, "partitions")),
            ("application",    0x10000, app_size, _make_filename(info, "app")),
        ]

        print("  Will back up 3 partitions:")
        for label, off, sz, fname in parts:
            print(f"    {label:20s}  0x{off:05X}  {sz:>10,} bytes  -> {fname}")
        print()

        confirm = input("  Proceed? [Y/n]: ").strip().lower()
        if confirm not in ("", "y", "yes"):
            print("  Aborted.")
            return False

        print(f"  This may take a few minutes.\n")
        all_ok = True
        for label, off, sz, fname in parts:
            path = _ask_filename(fname)
            ok = _read_flash_region(port, off, sz, path)
            if not ok:
                all_ok = False
            print()
        return all_ok

    elif mode == "3":
        # App only
        app_offset = ESP32_PARTITIONS["application"]["offset"]
        app_size = total_bytes - app_offset
        default_name = _make_filename(info, "app")
        output_path = _ask_filename(default_name)
        print(f"  Reading application firmware ({app_size:,} bytes from 0x{app_offset:X})...")
        print(f"  This may take a few minutes.\n")
        return _read_flash_region(port, app_offset, app_size, output_path)


def _get_file_path(prompt_text):
    """Ask user for a file path, return (path, size) or None."""
    while True:
        path = input(prompt_text).strip().strip('"').strip("'")
        if not path:
            return None
        path = os.path.expanduser(path)
        if os.path.isfile(path):
            return path
        print(f"  File not found: {path}")


def _write_flash_region(port, offset, bin_path):
    """Write a file to flash at the given offset. Returns True on success."""
    file_size = os.path.getsize(bin_path)
    print(f"  Writing {bin_path} ({file_size:,} bytes) -> 0x{offset:X}")

    rc = run_esptool([
        "--port", port,
        "--baud", str(DEFAULT_BAUD),
        "--before", "default_reset",
        "--after", "no_reset",
        "write_flash",
        "-z",
        "--flash_mode", DEFAULT_FLASH_MODE,
        "--flash_freq", DEFAULT_FLASH_FREQ,
        "--flash_size", DEFAULT_FLASH_SIZE,
        hex(offset), bin_path,
    ])

    if rc != 0:
        print(f"  ERROR: Failed to write at 0x{offset:X}.")
        return False

    print(f"  OK: written to 0x{offset:X}")
    return True


def do_restore(port):
    """Write one or more .bin files to flash."""
    print("\n[5/6] Restore — writing flash ROM...")

    print("\n  Restore mode:")
    print("  [1] Full ROM        — single .bin file written at 0x0")
    print("  [2] Bootloader+App  — bootloader (.bin at 0x1000) + app (.bin at 0x10000)")
    print("  [3] App only        — application firmware only (.bin at 0x10000)")
    print("  [4] Custom offset   — specify a .bin file and flash offset manually")

    while True:
        try:
            mode = input("\n  Select restore mode [1/2/3/4]: ").strip()
            if mode in ("1", "2", "3", "4"):
                break
        except EOFError:
            sys.exit(1)
        print("  Invalid selection.")

    if mode == "1":
        # Full ROM restore
        path = _get_file_path("  Path to full ROM .bin file: ")
        if not path:
            print("  Aborted.")
            return False

        file_size = os.path.getsize(path)
        print(f"\n  File: {path} ({file_size:,} bytes)")
        print(f"  This will ERASE the entire flash and write from 0x0.")
        confirm = input("  Continue? [y/N]: ").strip().lower()
        if confirm not in ("y", "yes"):
            print("  Aborted.")
            return False

        print(f"\n  Flashing full ROM... this may take a few minutes.\n")
        return _write_flash_region(port, 0x0, path)

    elif mode == "2":
        # Bootloader + App
        print("\n  Provide bootloader and application firmware .bin files.")

        bl_path = _get_file_path("  Path to bootloader .bin (0x1000): ")
        if not bl_path:
            print("  Aborted.")
            return False

        app_path = _get_file_path("  Path to application .bin (0x10000): ")
        if not app_path:
            print("  Aborted.")
            return False

        bl_size = os.path.getsize(bl_path)
        app_size = os.path.getsize(app_path)
        print(f"\n  Bootloader : {bl_path} ({bl_size:,} bytes) -> 0x1000")
        print(f"  Application: {app_path} ({app_size:,} bytes) -> 0x10000")
        print(f"\n  This will overwrite the bootloader and application partitions.")
        confirm = input("  Continue? [y/N]: ").strip().lower()
        if confirm not in ("y", "yes"):
            print("  Aborted.")
            return False

        # Flash both in a single esptool call (multiple address+file pairs)
        print(f"\n  Flashing... this may take a few minutes.\n")
        rc = run_esptool([
            "--port", port,
            "--baud", str(DEFAULT_BAUD),
            "--before", "default_reset",
            "--after", "no_reset",
            "write_flash",
            "-z",
            "--flash_mode", DEFAULT_FLASH_MODE,
            "--flash_freq", DEFAULT_FLASH_FREQ,
            "--flash_size", DEFAULT_FLASH_SIZE,
            "0x1000", bl_path,
            "0x10000", app_path,
        ])
        if rc != 0:
            print("\n  ERROR: Restore failed.")
            return False
        print("\n  Restore complete.")
        return True

    elif mode == "3":
        # App only
        path = _get_file_path("  Path to application .bin (0x10000): ")
        if not path:
            print("  Aborted.")
            return False

        file_size = os.path.getsize(path)
        print(f"\n  File: {path} ({file_size:,} bytes)")
        print(f"  This will overwrite the application partition at 0x10000.")
        confirm = input("  Continue? [y/N]: ").strip().lower()
        if confirm not in ("y", "yes"):
            print("  Aborted.")
            return False

        print(f"\n  Flashing app... this may take a few minutes.\n")
        return _write_flash_region(port, 0x10000, path)

    elif mode == "4":
        # Custom offset
        path = _get_file_path("  Path to .bin file: ")
        if not path:
            print("  Aborted.")
            return False

        while True:
            offset_str = input("  Flash offset (hex, e.g. 0x10000): ").strip()
            try:
                offset = int(offset_str, 0)
                break
            except ValueError:
                print("  Invalid hex value, try again.")

        file_size = os.path.getsize(path)
        print(f"\n  File: {path} ({file_size:,} bytes) -> 0x{offset:X}")
        confirm = input("  Continue? [y/N]: ").strip().lower()
        if confirm not in ("y", "yes"):
            print("  Aborted.")
            return False

        print(f"\n  Flashing... this may take a few minutes.\n")
        return _write_flash_region(port, offset, path)


def reboot_device(port):
    """Hard-reset the device via RTS pin."""
    print("\n[6/6] Rebooting device...")

    try:
        with serial.Serial(port, 115200, timeout=1) as ser:
            ser.dtr = False
            ser.rts = True
            time.sleep(0.1)
            ser.rts = False
            time.sleep(0.1)
        print("  Device rebooted successfully.")
    except serial.SerialException as e:
        print(f"  Could not reboot via serial: {e}")
        print("  Please manually reset the device (press the power/reset button).")


def main():
    print_banner()

    port = select_port()
    info = get_chip_info(port)
    confirm_device(info)
    action = choose_action()

    if action == "backup":
        success = do_backup(port, info)
    else:
        success = do_restore(port)

    if success:
        reboot_device(port)

    print()
    if success:
        print("Done. espROMkit finished successfully.")
    else:
        print("espROMkit finished with errors.")
        sys.exit(1)


if __name__ == "__main__":
    main()
