#!/usr/bin/env python3
"""
espROMkit CLI — Flash and backup tool for ESP32 microcontrollers
Version: 2026.02A
Board: M5StickC, M5StickC-Plus, and other ESP32 devices
Author: tommyho510@gmail.com

Uses esptool.py (https://github.com/espressif/esptool) to detect, back up,
restore, and reboot ESP32-based Arduino microcontrollers via the command line.
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

# Known ESP32 chip descriptions for M5StickC / M5StickC-Plus
KNOWN_DEVICES = {
    "ESP32-PICO-D4": "M5StickC / M5StickC-Plus (ESP32-PICO-D4)",
    "ESP32-PICO-V3": "M5StickC-Plus2 (ESP32-PICO-V3)",
    "ESP32": "Generic ESP32 module",
    "ESP32-S3": "ESP32-S3 module (e.g. LilyGO T-Display S3)",
    "ESP32-S2": "ESP32-S2 module",
    "ESP32-C3": "ESP32-C3 module",
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

    # Capture esptool output by temporarily redirecting stdout
    import io
    old_stdout = sys.stdout
    sys.stdout = captured = io.StringIO()

    rc = run_esptool([
        "--port", port,
        "--baud", str(DEFAULT_BAUD),
        "--before", "default_reset",
        "--after", "no_reset",
        "chip_id",
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

    # Parse key fields
    info = {
        "chip": "Unknown",
        "features": "",
        "mac": "",
        "crystal": "",
        "chip_id": "",
        "flash_size": "",
    }

    for line in output.splitlines():
        line_stripped = line.strip()
        if line_stripped.startswith("Chip is "):
            info["chip"] = line_stripped[len("Chip is "):]
        elif line_stripped.startswith("Features:"):
            info["features"] = line_stripped[len("Features:"):].strip()
        elif line_stripped.startswith("MAC:"):
            info["mac"] = line_stripped[len("MAC:"):].strip()
        elif line_stripped.startswith("Crystal is"):
            info["crystal"] = line_stripped[len("Crystal is"):].strip()
        elif line_stripped.startswith("Chip ID:"):
            info["chip_id"] = line_stripped[len("Chip ID:"):].strip()
        elif "Auto-detected Flash size" in line_stripped:
            info["flash_size"] = line_stripped.split(":")[-1].strip()

    return info


def confirm_device(info):
    """Display chip info and ask user to confirm."""
    print("\n[3/6] Device identification")

    chip_base = info["chip"].split(" (")[0] if " (" in info["chip"] else info["chip"]
    friendly = KNOWN_DEVICES.get(chip_base, info["chip"])

    print(f"  Chip     : {info['chip']}")
    print(f"  Device   : {friendly}")
    print(f"  Features : {info['features']}")
    print(f"  Crystal  : {info['crystal']}")
    print(f"  MAC      : {info['mac']}")
    if info["chip_id"]:
        print(f"  Chip ID  : {info['chip_id']}")
    if info["flash_size"]:
        print(f"  Flash    : {info['flash_size']}")

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
    print("  [1] Backup  — Read flash ROM to a .bin file")
    print("  [2] Restore — Write a .bin file to flash ROM")

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


def do_backup(port, info):
    """Read the entire flash into a .bin file."""
    print("\n[5/6] Backup — reading flash ROM...")

    # Determine flash size for read_flash
    flash_size = info.get("flash_size", "")
    if not flash_size:
        flash_size = "4MB"
        print(f"  Flash size not detected, defaulting to {flash_size}")

    # Parse size to bytes
    size_map = {
        "1MB": 0x100000,
        "2MB": 0x200000,
        "4MB": 0x400000,
        "8MB": 0x800000,
        "16MB": 0x1000000,
    }
    size_bytes = size_map.get(flash_size, 0x400000)

    # Build output filename: MAC_YYYYMMDD_HHMMSS.bin
    mac_slug = info.get("mac", "unknown").replace(":", "")
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    default_name = f"{mac_slug}_{timestamp}.bin"

    user_name = input(f"  Output filename [{default_name}]: ").strip()
    if not user_name:
        user_name = default_name
    if not user_name.lower().endswith(".bin"):
        user_name += ".bin"

    output_path = os.path.abspath(user_name)
    print(f"  Reading {flash_size} ({size_bytes} bytes) from flash...")
    print(f"  Saving to: {output_path}")
    print(f"  This may take a few minutes.\n")

    rc = run_esptool([
        "--port", port,
        "--baud", str(DEFAULT_BAUD),
        "--before", "default_reset",
        "--after", "no_reset",
        "read_flash",
        "0x0", str(size_bytes),
        output_path,
    ])

    if rc != 0:
        print("\n  ERROR: Backup failed.")
        return False

    if os.path.exists(output_path):
        file_size = os.path.getsize(output_path)
        print(f"\n  Backup complete: {output_path} ({file_size:,} bytes)")
    else:
        print("\n  ERROR: Output file was not created.")
        return False

    return True


def do_restore(port):
    """Write a .bin file to flash."""
    print("\n[5/6] Restore — writing flash ROM...")

    while True:
        bin_path = input("  Path to .bin file: ").strip().strip('"').strip("'")
        if not bin_path:
            print("  No file specified. Aborted.")
            return False
        bin_path = os.path.expanduser(bin_path)
        if os.path.isfile(bin_path):
            break
        print(f"  File not found: {bin_path}")
        print("  Please enter a valid path.")

    file_size = os.path.getsize(bin_path)
    print(f"  File: {bin_path} ({file_size:,} bytes)")

    # Confirm before flashing
    confirm = input("  This will ERASE and overwrite the device flash. Continue? [y/N]: ").strip().lower()
    if confirm not in ("y", "yes"):
        print("  Aborted by user.")
        return False

    # Flash offset — 0x0 for full ROM backup/restore
    flash_offset = "0x0"
    print(f"  Flashing to offset {flash_offset}...")
    print(f"  Baud: {DEFAULT_BAUD}, Mode: {DEFAULT_FLASH_MODE}, Freq: {DEFAULT_FLASH_FREQ}")
    print(f"  This may take a few minutes.\n")

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
        flash_offset, bin_path,
    ])

    if rc != 0:
        print("\n  ERROR: Restore failed.")
        return False

    print("\n  Restore complete.")
    return True


def reboot_device(port):
    """Hard-reset the device via RTS pin."""
    print("\n[6/6] Rebooting device...")

    # Use a brief serial connection to toggle RTS for a hard reset
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
