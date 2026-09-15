cd %~dp0
@echo Hold BOOT, tap RST, release BOOT (this board has no working auto-download), then
pause
python -m esptool --port COM5 --before no-reset --baud 460800 write-flash 0 ESP32-3248S035_94e686_4MB_backup.bin
