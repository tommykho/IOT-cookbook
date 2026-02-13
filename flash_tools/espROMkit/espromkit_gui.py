#!/usr/bin/env python3
"""
espROMkit GUI — Flash and backup tool for ESP32 microcontrollers
Version: 2026.02A
Board: M5StickC, M5StickC-Plus, and other ESP32 devices
Author: tommyho510@gmail.com

Tkinter-based GUI that uses esptool.py to detect, back up,
restore, and reboot ESP32-based Arduino microcontrollers.
"""

import sys
import os
import io
import threading
import time
from datetime import datetime

try:
    import tkinter as tk
    from tkinter import ttk, filedialog, messagebox, scrolledtext
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

KNOWN_DEVICES = {
    "ESP32-PICO-D4": "M5StickC / M5StickC-Plus",
    "ESP32-PICO-V3": "M5StickC-Plus2",
    "ESP32": "Generic ESP32",
    "ESP32-S3": "ESP32-S3 (e.g. LilyGO T-Display S3)",
    "ESP32-S2": "ESP32-S2",
    "ESP32-C3": "ESP32-C3",
}

FLASH_SIZES = ["detect", "1MB", "2MB", "4MB", "8MB", "16MB"]
BAUD_RATES = ["115200", "230400", "460800", "921600", "1500000"]


class LogRedirector:
    """Redirect stdout/stderr writes into a tkinter text widget (thread-safe)."""

    def __init__(self, widget, tag="stdout"):
        self.widget = widget
        self.tag = tag

    def write(self, text):
        # Schedule the update on the main thread
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
        self.root.geometry("720x680")
        self.root.minsize(640, 580)

        self.port_var = tk.StringVar()
        self.baud_var = tk.StringVar(value=str(DEFAULT_BAUD))
        self.chip_info = {}
        self.working = False

        self._build_ui()
        self.refresh_ports()

    # ------------------------------------------------------------------ UI
    def _build_ui(self):
        # Top frame — Port selection
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
        baud_combo = ttk.Combobox(
            port_frame,
            textvariable=self.baud_var,
            values=BAUD_RATES,
            state="readonly",
            width=10,
        )
        baud_combo.pack(side="left")

        # Device info frame
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

        # Action frame
        action_frame = ttk.LabelFrame(self.root, text="3. Action", padding=8)
        action_frame.pack(fill="x", padx=10, pady=4)

        btn_box = ttk.Frame(action_frame)
        btn_box.pack(fill="x")

        self.backup_btn = ttk.Button(
            btn_box, text="Backup ROM", command=self._on_backup, state="disabled"
        )
        self.backup_btn.pack(side="left", padx=(0, 8))

        self.restore_btn = ttk.Button(
            btn_box, text="Restore ROM", command=self._on_restore, state="disabled"
        )
        self.restore_btn.pack(side="left", padx=(0, 8))

        self.reboot_btn = ttk.Button(
            btn_box, text="Reboot Device", command=self._on_reboot, state="disabled"
        )
        self.reboot_btn.pack(side="left", padx=(0, 8))

        # Flash size selector (for backup)
        ttk.Label(btn_box, text="Flash size:").pack(side="left", padx=(20, 2))
        self.flash_size_var = tk.StringVar(value="detect")
        flash_combo = ttk.Combobox(
            btn_box,
            textvariable=self.flash_size_var,
            values=FLASH_SIZES,
            state="readonly",
            width=8,
        )
        flash_combo.pack(side="left")

        # Progress bar
        self.progress = ttk.Progressbar(
            self.root, mode="indeterminate", length=300
        )
        self.progress.pack(fill="x", padx=10, pady=4)

        # Log output
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
        t = threading.Thread(target=worker, daemon=True)
        t.start()

    # ------------------------------------------------------- Detect device
    def _on_detect(self):
        port = self._selected_port()
        if not port:
            messagebox.showwarning("No port", "Select a serial port first.")
            return

        baud = self.baud_var.get()
        self.log_clear()
        self.log(f"Detecting device on {port} @ {baud} baud...\n\n")

        # We need to capture output to parse chip info
        self.chip_info = {}
        self._detect_output = io.StringIO()

        def worker():
            old_stdout, old_stderr = sys.stdout, sys.stderr
            # Tee to both the log widget and our capture buffer
            class Tee:
                def __init__(self, widget, buf):
                    self.redir = LogRedirector(widget)
                    self.buf = buf
                def write(self, text):
                    self.redir.write(text)
                    self.buf.write(text)
                def flush(self):
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
                    "chip_id",
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

        self.chip_info = info
        chip = info.get("chip", "Unknown")
        mac = info.get("mac", "N/A")
        chip_base = chip.split(" (")[0] if " (" in chip else chip
        friendly = KNOWN_DEVICES.get(chip_base, chip)

        if info.get("flash_size"):
            self.flash_size_var.set(info["flash_size"])

        self.info_label.configure(
            text=f"{friendly}  |  Chip: {chip}  |  MAC: {mac}",
            foreground="black",
        )
        self.log(f"\nDevice confirmed: {friendly}\n")

    # --------------------------------------------------------- Backup ROM
    def _on_backup(self):
        port = self._selected_port()
        if not port:
            return

        flash_size = self.flash_size_var.get()
        size_map = {
            "detect": 0x400000,  # default 4MB
            "1MB": 0x100000,
            "2MB": 0x200000,
            "4MB": 0x400000,
            "8MB": 0x800000,
            "16MB": 0x1000000,
        }
        size_bytes = size_map.get(flash_size, 0x400000)

        mac_slug = self.chip_info.get("mac", "unknown").replace(":", "")
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        default_name = f"{mac_slug}_{timestamp}.bin"

        path = filedialog.asksaveasfilename(
            title="Save ROM backup as",
            initialfile=default_name,
            defaultextension=".bin",
            filetypes=[("Binary files", "*.bin"), ("All files", "*.*")],
        )
        if not path:
            return

        baud = self.baud_var.get()
        self.log(f"\nStarting backup: {flash_size} ({size_bytes} bytes) -> {path}\n\n")

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
                "0x0", str(size_bytes),
                path,
            ],
            on_done=on_done,
        )

    # -------------------------------------------------------- Restore ROM
    def _on_restore(self):
        port = self._selected_port()
        if not port:
            return

        path = filedialog.askopenfilename(
            title="Select ROM .bin file to flash",
            filetypes=[("Binary files", "*.bin"), ("All files", "*.*")],
        )
        if not path:
            return

        file_size = os.path.getsize(path)
        confirm = messagebox.askyesno(
            "Confirm Restore",
            f"This will ERASE the device flash and write:\n\n"
            f"{path}\n({file_size:,} bytes)\n\n"
            f"Continue?",
        )
        if not confirm:
            return

        baud = self.baud_var.get()
        self.log(f"\nStarting restore: {path} ({file_size:,} bytes)\n\n")

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
                "0x0", path,
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
