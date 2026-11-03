@echo off
setlocal
echo ========================================================
echo     RPMLOOP Navigator - ESP32-S3 Flash Utility
echo ========================================================
set /p PORT="Enter ESP32-S3 COM Port (e.g. COM3): "
if "%PORT%"=="" (
    echo [ERROR] COM port cannot be empty!
    pause
    exit /b 1
)

echo Flashing RPMLOOP Navigator firmware to %PORT%...
python -m esptool --chip esp32s3 -p %PORT% -b 460800 --before default-reset --after hard-reset write-flash --flash-mode dio --flash-size 4MB --flash-freq 80m 0x0 bootloader.bin 0x8000 partition-table.bin 0x10000 rpmloop_navigator.bin

if %ERRORLEVEL% equ 0 (
    echo.
    echo ========================================================
    echo  [SUCCESS] Flashing completed successfully!
    echo ========================================================
) else (
    echo.
    echo [ERROR] Flashing failed! Check cable, drivers, and COM port.
)
pause
