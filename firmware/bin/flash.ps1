# RPMLOOP Navigator v3.0 — One-Click Firmware Flasher (PowerShell)
Write-Host "========================================================" -ForegroundColor Cyan
Write-Host "   RPMLOOP Navigator v3.0 — One-Click Firmware Flasher   " -ForegroundColor Cyan
Write-Host "========================================================" -ForegroundColor Cyan
Write-Host "`nPlease ensure your ESP32-S3 is connected via USB cable.`n"

$binDir = $PSScriptRoot
$fwDir = Split-Path $binDir -Parent

if (Get-Command pio -ErrorAction SilentlyContinue) {
    Write-Host "[1/2] PlatformIO detected. Flashing latest build via PlatformIO..." -ForegroundColor Yellow
    Push-Location $fwDir
    pio run -t upload
    if ($LASTEXITCODE -eq 0) {
        Write-Host "`nSUCCESS: Firmware flashed successfully!" -ForegroundColor Green
        Pop-Location
        Read-Host "Press Enter to exit..."
        exit 0
    }
    Pop-Location
    Write-Host "PlatformIO failed. Falling back to esptool..." -ForegroundColor Yellow
}

$pythonCmd = "python"
if (-not (Get-Command python -ErrorAction SilentlyContinue)) {
    $fallbackPy = "$env:LOCALAPPDATA\Programs\Python\Python312\python.exe"
    if (Test-Path $fallbackPy) {
        $pythonCmd = $fallbackPy
    }
}

Write-Host "[2/2] Flashing pre-built binaries via esptool..." -ForegroundColor Yellow
& $pythonCmd -m esptool --chip esp32s3 -b 921600 write_flash 0x0 "$binDir\bootloader.bin" 0x8000 "$binDir\partitions.bin" 0x10000 "$binDir\firmware.bin"

if ($LASTEXITCODE -eq 0) {
    Write-Host "`nSUCCESS: RPMLOOP Navigator v3.0 Flashed Successfully!" -ForegroundColor Green
} else {
    Write-Host "`n[ERROR] Flashing failed. Please check USB cable and COM port connection." -ForegroundColor Red
}

Read-Host "Press Enter to exit..."
