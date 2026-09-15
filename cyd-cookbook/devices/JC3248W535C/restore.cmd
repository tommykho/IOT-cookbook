cd %~dp0
python -m esptool --port COM3 write-flash 0 JC3248W535C_e80690_16MB_backup.bin