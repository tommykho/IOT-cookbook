#!/usr/bin/env python3
"""
espROMkit GUI — Flash and backup tool for ESP32 microcontrollers
Version: 2026.02A
Board: M5StickC, M5StickC-Plus, and other ESP32 devices
Author: tommyho510@gmail.com

Tkinter-based GUI that uses esptool.py to detect, back up,
restore, and reboot ESP32-based Arduino microcontrollers.

ESP32 Flash Layout (typical):
  0x1000   — Bootloader (second-stage)
  0x8000   — Partition table
  0xe000   — OTA data (if applicable)
  0x10000  — Application firmware
"""

import sys
import os
import io
import threading
import time
from datetime import datetime

try:
    import tkinter as tk
    from tkinter import ttk, filedialog, messagebox, scrolledtext, simpledialog
except ImportError:
    sys.exit(
        "ERROR: tkinter is required for the GUI.\n"
        "Install with: apt install python3-tk  (Linux)\n"
        "  or: brew install python-tk  (macOS)"
    )

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    sys.exit("ERROR: pyserial is required. Install with: pip install pyserial")

try:
    import esptool
except ImportError:
    sys.exit("ERROR: esptool is required. Install with: pip install esptool")


# Default flash parameters
DEFAULT_BAUD = 1500000
DEFAULT_FLASH_MODE = "dio"
DEFAULT_FLASH_FREQ = "80m"
DEFAULT_FLASH_SIZE = "detect"

# Standard ESP32 flash partition offsets
ESP32_PARTITIONS = {
    "bootloader":     {"offset": 0x1000,  "size": 0x7000,  "label": "Bootloader"},
    "partition_table": {"offset": 0x8000,  "size": 0x1000,  "label": "Partition Table"},
    "ota_data":       {"offset": 0xe000,  "size": 0x2000,  "label": "OTA Data"},
    "application":    {"offset": 0x10000, "size": None,     "label": "Application Firmware"},
}

KNOWN_DEVICES = {
    "ESP32-PICO-D4": "M5StickC / M5StickC-Plus",
    "ESP32-PICO-V3": "M5StickC-Plus2",
    "ESP32": "Generic ESP32",
    "ESP32-S3": "ESP32-S3 (e.g. LilyGO T-Display S3)",
    "ESP32-S2": "ESP32-S2",
    "ESP32-C3": "ESP32-C3",
}

FLASH_SIZES = ["detect", "1MB", "2MB", "4MB", "8MB", "16MB"]
FLASH_SIZE_BYTES = {
    "detect": 0x400000,
    "1MB": 0x100000,
    "2MB": 0x200000,
    "4MB": 0x400000,
    "8MB": 0x800000,
    "16MB": 0x1000000,
}
BAUD_RATES = ["115200", "230400", "460800", "921600", "1500000"]

# Backup/Restore mode options
BACKUP_MODES = [
    ("Full ROM (0x0)", "full"),
    ("Partitions (bootloader + partition table + app)", "partitions"),
    ("App only (0x10000)", "app"),
]
RESTORE_MODES = [
    ("Full ROM (single .bin at 0x0)", "full"),
    ("Bootloader + App (two .bin files)", "bl_app"),
    ("App only (.bin at 0x10000)", "app"),
    ("Custom offset", "custom"),
]


class LogRedirector:
    """Redirect stdout/stderr writes into a tkinter text widget (thread-safe)."""

    def __init__(self, widget, tag="stdout"):
        self.widget = widget
        self.tag = tag

    def write(self, text):
        self.widget.after(0, self._append, text)

    def _append(self, text):
        self.widget.configure(state="normal")
        self.widget.insert(tk.END, text, self.tag)
        self.widget.see(tk.END)
        self.widget.configure(state="disabled")

    def flush(self):
        pass


class EspROMkitGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("espROMkit — ESP32 Flash & Backup Tool")
        self.root.geometry("760x750")
        self.root.minsize(680, 650)

        self.port_var = tk.StringVar()
        self.baud_var = tk.StringVar(value=str(DEFAULT_BAUD))
        self.chip_info = {}
        self.working = False

        self._build_ui()
        self.refresh_ports()

    # ------------------------------------------------------------------ UI
    def _build_ui(self):
        # === Port selection ===
        port_frame = ttk.LabelFrame(self.root, text="1. Serial Port", padding=8)
        port_frame.pack(fill="x", padx=10, pady=(10, 4))

        self.port_combo = ttk.Combobox(
            port_frame, textvariable=self.port_var, state="readonly", width=40
        )
        self.port_combo.pack(side="left", padx=(0, 6))

        ttk.Button(port_frame, text="Refresh", command=self.refresh_ports).pack(
            side="left", padx=2
        )

        ttk.Label(port_frame, text="Baud:").pack(side="left", padx=(12, 2))
        ttk.Combobox(
            port_frame,
            textvariable=self.baud_var,
            values=BAUD_RATES,
            state="readonly",
            width=10,
        ).pack(side="left")

        # === Device info ===
        info_frame = ttk.LabelFrame(self.root, text="2. Device Info", padding=8)
        info_frame.pack(fill="x", padx=10, pady=4)

        self.detect_btn = ttk.Button(
            info_frame, text="Detect Device", command=self._on_detect
        )
        self.detect_btn.pack(side="left", padx=(0, 10))

        self.info_label = ttk.Label(
            info_frame, text="No device detected yet.", foreground="gray"
        )
        self.info_label.pack(side="left", fill="x", expand=True)

        # === Flash layout reference ===
        layout_frame = ttk.LabelFrame(self.root, text="ESP32 Flash Layout", padding=6)
        layout_frame.pack(fill="x", padx=10, pady=4)
        layout_text = (
            "0x1000 Bootloader   |   0x8000 Partition Table   |   "
            "0xe000 OTA Data   |   0x10000 Application"
        )
        ttk.Label(layout_frame, text=layout_text, font=("Consolas", 9)).pack(anchor="w")

        # === Action frame ===
        action_frame = ttk.LabelFrame(self.root, text="3. Action", padding=8)
        action_frame.pack(fill="x", padx=10, pady=4)

        # Row 1: backup controls
        backup_row = ttk.Frame(action_frame)
        backup_row.pack(fill="x", pady=(0, 6))

        self.backup_btn = ttk.Button(
            backup_row, text="Backup ROM", command=self._on_backup, state="disabled"
        )
        self.backup_btn.pack(side="left", padx=(0, 8))

        ttk.Label(backup_row, text="Mode:").pack(side="left", padx=(4, 2))
        self.backup_mode_var = tk.StringVar(value="full")
        for label, value in BACKUP_MODES:
            ttk.Radiobutton(
                backup_row, text=label, variable=self.backup_mode_var, value=value
            ).pack(side="left", padx=2)

        # Row 2: restore controls
        restore_row = ttk.Frame(action_frame)
        restore_row.pack(fill="x", pady=(0, 6))

        self.restore_btn = ttk.Button(
            restore_row, text="Restore ROM", command=self._on_restore, state="disabled"
        )
        self.restore_btn.pack(side="left", padx=(0, 8))

        ttk.Label(restore_row, text="Mode:").pack(side="left", padx=(4, 2))
        self.restore_mode_var = tk.StringVar(value="full")
        for label, value in RESTORE_MODES:
            ttk.Radiobutton(
                restore_row, text=label, variable=self.restore_mode_var, value=value
            ).pack(side="left", padx=2)

        # Row 3: reboot + flash size
        util_row = ttk.Frame(action_frame)
        util_row.pack(fill="x")

        self.reboot_btn = ttk.Button(
            util_row, text="Reboot Device", command=self._on_reboot, state="disabled"
        )
        self.reboot_btn.pack(side="left", padx=(0, 8))

        ttk.Label(util_row, text="Flash size:").pack(side="left", padx=(20, 2))
        self.flash_size_var = tk.StringVar(value="detect")
        ttk.Combobox(
            util_row,
            textvariable=self.flash_size_var,
            values=FLASH_SIZES,
            state="readonly",
            width=8,
        ).pack(side="left")

        # === Progress bar ===
        self.progress = ttk.Progressbar(self.root, mode="indeterminate", length=300)
        self.progress.pack(fill="x", padx=10, pady=4)

        # === Log output ===
        log_frame = ttk.LabelFrame(self.root, text="Log", padding=4)
        log_frame.pack(fill="both", expand=True, padx=10, pady=(4, 10))

        self.log_text = scrolledtext.ScrolledText(
            log_frame, height=16, state="disabled", font=("Consolas", 9), wrap="word"
        )
        self.log_text.pack(fill="both", expand=True)
        self.log_text.tag_configure("stderr", foreground="red")

    # ----------------------------------------------------------- Port mgmt
    def refresh_ports(self):
        ports = serial.tools.list_ports.comports()
        port_list = []
        for p in sorted(ports, key=lambda x: x.device):
            label = f"{p.device}  —  {p.description or 'Unknown'}"
            port_list.append((p.device, label))

        self._port_map = {label: device for device, label in port_list}
        labels = [label for _, label in port_list]
        self.port_combo["values"] = labels

        if labels:
            self.port_combo.current(0)
        else:
            self.port_var.set("")

        self.log(f"Found {len(labels)} serial port(s).\n")

    def _selected_port(self):
        label = self.port_var.get()
        return self._port_map.get(label, label)

    # -------------------------------------------------------------- Logging
    def log(self, text):
        self.log_text.configure(state="normal")
        self.log_text.insert(tk.END, text)
        self.log_text.see(tk.END)
        self.log_text.configure(state="disabled")

    def log_clear(self):
        self.log_text.configure(state="normal")
        self.log_text.delete("1.0", tk.END)
        self.log_text.configure(state="disabled")

    # -------------------------------------------------------- Button guards
    def _set_busy(self, busy):
        self.working = busy
        state = "disabled" if busy else "normal"
        self.detect_btn.configure(state=state)
        if busy:
            self.backup_btn.configure(state="disabled")
            self.restore_btn.configure(state="disabled")
            self.reboot_btn.configure(state="disabled")
            self.progress.start(10)
        else:
            has_info = bool(self.chip_info)
            self.backup_btn.configure(state="normal" if has_info else "disabled")
            self.restore_btn.configure(state="normal" if has_info else "disabled")
            self.reboot_btn.configure(state="normal" if has_info else "disabled")
            self.progress.stop()

    # -------------------------------------------- esptool wrapper (threaded)
    def _run_esptool_threaded(self, args, on_done=None):
        """Run esptool in a background thread, capturing output to the log."""

        def worker():
            old_stdout, old_stderr = sys.stdout, sys.stderr
            redirector = LogRedirector(self.log_text)
            err_redirector = LogRedirector(self.log_text, tag="stderr")
            sys.stdout = redirector
            sys.stderr = err_redirector

            rc = 0
            try:
                esptool.main(args)
            except SystemExit as e:
                rc = e.code if e.code else 0
            except Exception as e:
                rc = 1
                redirector.write(f"\nesptool error: {e}\n")
            finally:
                sys.stdout = old_stdout
                sys.stderr = old_stderr

            self.root.after(0, _finished, rc)

        def _finished(rc):
            self._set_busy(False)
            if on_done:
                on_done(rc)

        self._set_busy(True)
        threading.Thread(target=worker, daemon=True).start()

    # ------------------------------------------------------- Detect device
    def _on_detect(self):
        port = self._selected_port()
        if not port:
            messagebox.showwarning("No port", "Select a serial port first.")
            return

        baud = self.baud_var.get()
        self.log_clear()
        self.log(f"Detecting device on {port} @ {baud} baud...\n\n")

        self.chip_info = {}
        self._detect_output = io.StringIO()

        def worker():
            old_stdout, old_stderr = sys.stdout, sys.stderr

            class Tee:
                def __init__(tee_self, widget, buf):
                    tee_self.redir = LogRedirector(widget)
                    tee_self.buf = buf
                def write(tee_self, text):
                    tee_self.redir.write(text)
                    tee_self.buf.write(text)
                def flush(tee_self):
                    pass

            tee = Tee(self.log_text, self._detect_output)
            sys.stdout = tee
            sys.stderr = LogRedirector(self.log_text, tag="stderr")

            rc = 0
            try:
                esptool.main([
                    "--port", port,
                    "--baud", baud,
                    "--before", "default_reset",
                    "--after", "no_reset",
                    "flash_id",
                ])
            except SystemExit as e:
                rc = e.code if e.code else 0
            except Exception as e:
                rc = 1
                tee.write(f"\nesptool error: {e}\n")
            finally:
                sys.stdout = old_stdout
                sys.stderr = old_stderr

            self.root.after(0, _done, rc)

        def _done(rc):
            self._set_busy(False)
            if rc != 0:
                self.info_label.configure(
                    text="Detection failed. Check connection.", foreground="red"
                )
                return
            self._parse_chip_info(self._detect_output.getvalue())

        self._set_busy(True)
        threading.Thread(target=worker, daemon=True).start()

    def _parse_chip_info(self, output):
        info = {}
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

        self.chip_info = info
        chip = info.get("chip", "Unknown")
        mac = info.get("mac", "N/A")
        chip_base = chip.split(" (")[0] if " (" in chip else chip
        friendly = KNOWN_DEVICES.get(chip_base, chip)

        if info.get("flash_size"):
            self.flash_size_var.set(info["flash_size"])

        self.info_label.configure(
            text=f"{friendly}  |  Chip: {chip}  |  MAC: {mac}  |  Flash: {info.get('flash_size', 'N/A')}",
            foreground="black",
        )
        self.log(f"\nDevice confirmed: {friendly}\n")
        self.log(
            f"Flash layout: 0x1000=Bootloader | 0x8000=Partitions | "
            f"0xe000=OTA | 0x10000=Application\n"
        )

    # --------------------------------------------------------- Helpers
    def _flash_total_bytes(self):
        return FLASH_SIZE_BYTES.get(self.flash_size_var.get(), 0x400000)

    def _mac_slug(self):
        return self.chip_info.get("mac", "unknown").replace(":", "")

    def _default_filename(self, suffix):
        ts = datetime.now().strftime("%Y%m%d_%H%M%S")
        return f"{self._mac_slug()}_{ts}_{suffix}.bin"

    # --------------------------------------------------------- Backup ROM
    def _on_backup(self):
        port = self._selected_port()
        if not port:
            return

        mode = self.backup_mode_var.get()
        total_bytes = self._flash_total_bytes()

        if mode == "full":
            self._backup_region(port, 0x0, total_bytes, "full")

        elif mode == "partitions":
            self._backup_partitions(port, total_bytes)

        elif mode == "app":
            app_off = ESP32_PARTITIONS["application"]["offset"]
            app_size = total_bytes - app_off
            self._backup_region(port, app_off, app_size, "app")

    def _backup_region(self, port, offset, size, suffix):
        """Back up a single flash region via a save-file dialog."""
        path = filedialog.asksaveasfilename(
            title=f"Save {suffix} backup as",
            initialfile=self._default_filename(suffix),
            defaultextension=".bin",
            filetypes=[("Binary files", "*.bin"), ("All files", "*.*")],
        )
        if not path:
            return

        baud = self.baud_var.get()
        self.log(
            f"\nBackup [{suffix}]: 0x{offset:X}..0x{offset + size:X} "
            f"({size:,} bytes) -> {path}\n\n"
        )

        def on_done(rc):
            if rc == 0 and os.path.exists(path):
                fsize = os.path.getsize(path)
                self.log(f"\nBackup complete: {path} ({fsize:,} bytes)\n")
                messagebox.showinfo("Backup Complete", f"Saved to:\n{path}\n({fsize:,} bytes)")
            else:
                self.log("\nBackup FAILED.\n")
                messagebox.showerror("Backup Failed", "See log for details.")

        self._run_esptool_threaded(
            [
                "--port", port,
                "--baud", baud,
                "--before", "default_reset",
                "--after", "no_reset",
                "read_flash",
                hex(offset), str(size),
                path,
            ],
            on_done=on_done,
        )

    def _backup_partitions(self, port, total_bytes):
        """Back up bootloader, partition table, and app as separate files."""
        app_size = total_bytes - ESP32_PARTITIONS["application"]["offset"]
        parts = [
            ("bootloader",      0x1000, 0x7000),
            ("partition_table",  0x8000, 0x1000),
            ("application",     0x10000, app_size),
        ]

        # Ask user for a directory to save all three files
        save_dir = filedialog.askdirectory(title="Select directory to save partition backups")
        if not save_dir:
            return

        files = []
        for name, offset, size in parts:
            fname = self._default_filename(name)
            full_path = os.path.join(save_dir, fname)
            files.append((name, offset, size, full_path))

        summary = "\n".join(
            f"  {name:20s} 0x{off:05X} ({sz:>10,} bytes) -> {os.path.basename(fp)}"
            for name, off, sz, fp in files
        )
        confirm = messagebox.askyesno(
            "Confirm Partition Backup",
            f"Will back up 3 partitions to:\n{save_dir}\n\n{summary}\n\nProceed?",
        )
        if not confirm:
            return

        # Chain the three reads sequentially
        baud = self.baud_var.get()
        self.log(f"\nPartition backup to {save_dir}\n")

        def run_chain(idx=0):
            if idx >= len(files):
                self._set_busy(False)
                self.log("\nAll partition backups complete.\n")
                messagebox.showinfo("Backup Complete", f"3 partitions saved to:\n{save_dir}")
                return

            name, offset, size, path = files[idx]
            self.log(f"\n[{idx + 1}/3] {name}: 0x{offset:X}..0x{offset + size:X} ({size:,} bytes)\n")

            def on_part_done(rc):
                if rc != 0:
                    self.log(f"\nERROR: Failed to back up {name}.\n")
                    messagebox.showerror("Backup Failed", f"Failed on {name}. See log.")
                    return
                if os.path.exists(path):
                    fsize = os.path.getsize(path)
                    self.log(f"  OK: {os.path.basename(path)} ({fsize:,} bytes)\n")
                self.root.after(100, run_chain, idx + 1)

            self._run_esptool_threaded(
                [
                    "--port", port,
                    "--baud", baud,
                    "--before", "default_reset",
                    "--after", "no_reset",
                    "read_flash",
                    hex(offset), str(size),
                    path,
                ],
                on_done=on_part_done,
            )

        run_chain(0)

    # -------------------------------------------------------- Restore ROM
    def _on_restore(self):
        port = self._selected_port()
        if not port:
            return

        mode = self.restore_mode_var.get()

        if mode == "full":
            self._restore_files(port, [("Full ROM", 0x0)])

        elif mode == "bl_app":
            self._restore_files(
                port,
                [("Bootloader", 0x1000), ("Application", 0x10000)],
            )

        elif mode == "app":
            self._restore_files(port, [("Application", 0x10000)])

        elif mode == "custom":
            self._restore_custom(port)

    def _restore_files(self, port, file_specs):
        """Ask user for .bin file(s) and flash them at the given offsets.

        file_specs: list of (label, offset) pairs. One file dialog per entry.
        """
        pairs = []  # (offset_hex, path)
        details = []

        for label, offset in file_specs:
            path = filedialog.askopenfilename(
                title=f"Select {label} .bin file (0x{offset:X})",
                filetypes=[("Binary files", "*.bin"), ("All files", "*.*")],
            )
            if not path:
                return  # user cancelled
            fsize = os.path.getsize(path)
            pairs.append((hex(offset), path))
            details.append(f"  {label}: {os.path.basename(path)} ({fsize:,} bytes) -> 0x{offset:X}")

        summary = "\n".join(details)
        confirm = messagebox.askyesno(
            "Confirm Restore",
            f"This will ERASE and overwrite flash:\n\n{summary}\n\nContinue?",
        )
        if not confirm:
            return

        baud = self.baud_var.get()
        self.log(f"\nStarting restore...\n{summary}\n\n")

        # Build esptool args with multiple offset+file pairs
        args = [
            "--port", port,
            "--baud", baud,
            "--before", "default_reset",
            "--after", "no_reset",
            "write_flash",
            "-z",
            "--flash_mode", DEFAULT_FLASH_MODE,
            "--flash_freq", DEFAULT_FLASH_FREQ,
            "--flash_size", DEFAULT_FLASH_SIZE,
        ]
        for offset_hex, path in pairs:
            args.extend([offset_hex, path])

        def on_done(rc):
            if rc == 0:
                self.log("\nRestore complete.\n")
                messagebox.showinfo("Restore Complete", "Flash write finished successfully.")
            else:
                self.log("\nRestore FAILED.\n")
                messagebox.showerror("Restore Failed", "See log for details.")

        self._run_esptool_threaded(args, on_done=on_done)

    def _restore_custom(self, port):
        """Restore with a user-specified flash offset."""
        path = filedialog.askopenfilename(
            title="Select .bin file for custom restore",
            filetypes=[("Binary files", "*.bin"), ("All files", "*.*")],
        )
        if not path:
            return

        # Ask for offset via a small dialog
        offset_str = simpledialog.askstring(
            "Flash Offset",
            "Enter flash offset in hex (e.g. 0x10000):",
            initialvalue="0x10000",
        )
        if not offset_str:
            return

        try:
            offset = int(offset_str, 0)
        except ValueError:
            messagebox.showerror("Invalid Offset", f"'{offset_str}' is not a valid hex number.")
            return

        fsize = os.path.getsize(path)
        confirm = messagebox.askyesno(
            "Confirm Custom Restore",
            f"File: {os.path.basename(path)} ({fsize:,} bytes)\n"
            f"Offset: 0x{offset:X}\n\n"
            f"This will overwrite flash at this offset. Continue?",
        )
        if not confirm:
            return

        baud = self.baud_var.get()
        self.log(f"\nCustom restore: {path} ({fsize:,} bytes) -> 0x{offset:X}\n\n")

        def on_done(rc):
            if rc == 0:
                self.log("\nRestore complete.\n")
                messagebox.showinfo("Restore Complete", "Flash write finished successfully.")
            else:
                self.log("\nRestore FAILED.\n")
                messagebox.showerror("Restore Failed", "See log for details.")

        self._run_esptool_threaded(
            [
                "--port", port,
                "--baud", baud,
                "--before", "default_reset",
                "--after", "no_reset",
                "write_flash",
                "-z",
                "--flash_mode", DEFAULT_FLASH_MODE,
                "--flash_freq", DEFAULT_FLASH_FREQ,
                "--flash_size", DEFAULT_FLASH_SIZE,
                hex(offset), path,
            ],
            on_done=on_done,
        )

    # -------------------------------------------------------- Reboot device
    def _on_reboot(self):
        port = self._selected_port()
        if not port:
            return

        self.log("\nRebooting device...\n")
        try:
            with serial.Serial(port, 115200, timeout=1) as ser:
                ser.dtr = False
                ser.rts = True
                time.sleep(0.1)
                ser.rts = False
                time.sleep(0.1)
            self.log("Device rebooted successfully.\n")
        except serial.SerialException as e:
            self.log(f"Reboot failed: {e}\nPlease reset the device manually.\n")


def main():
    root = tk.Tk()
    EspROMkitGUI(root)
    root.mainloop()


if __name__ == "__main__":
    main()
