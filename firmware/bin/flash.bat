@echo off
setlocal enabledelayedexpansion

echo ========================================================
echo    RPMLOOP Navigator v3.0 — One-Click Firmware Flasher
echo ========================================================
echo.
echo Please ensure your ESP32-S3 is connected via USB cable.
echo.

where pio >nul 2>&1
if %ERRORLEVEL% equ 0 (
    echo [1/2] PlatformIO detected. Flashing latest build via PlatformIO...
    cd /d "%~dp0\.."
    pio run -t upload
    if !ERRORLEVEL! equ 0 (
        echo.
        echo ========================================================
        echo  SUCCESS: Firmware flashed successfully!
        echo ========================================================
        pause
        exit /b 0
    )
    echo PlatformIO upload returned an error. Falling back to esptool...
    cd /d "%~dp0"
)

echo [2/2] Flashing pre-built binaries via esptool...
set PYTHON_CMD=python
where python >nul 2>&1
if %ERRORLEVEL% neq 0 (
    if exist "%LOCALAPPDATA%\Programs\Python\Python312\python.exe" (
        set PYTHON_CMD="%LOCALAPPDATA%\Programs\Python\Python312\python.exe"
    )
)

%PYTHON_CMD% -m esptool --chip esp32s3 -b 921600 write_flash 0x0 "%~dp0bootloader.bin" 0x8000 "%~dp0partitions.bin" 0x10000 "%~dp0firmware.bin"

if %ERRORLEVEL% equ 0 (
    echo.
    echo ========================================================
    echo  SUCCESS: RPMLOOP Navigator v3.0 Flashed Successfully!
    echo ========================================================
) else (
    echo.
    echo [ERROR] Flashing failed. Please check COM port connection.
)

pause
