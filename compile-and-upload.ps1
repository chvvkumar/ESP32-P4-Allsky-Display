# ESP32-P4 Allsky Display - Compile and Upload Script
# This script compiles and uploads the sketch to ESP32-P4

# Parameters
param(
    [string]$ComPort = "COM3",
    [string]$BaudRate = "921600",
    [string]$OutputFolder = "",
    [switch]$SkipUpload
)

# Auto-detect script directory and sketch path
$SCRIPT_DIR = Split-Path -Parent $MyInvocation.MyCommand.Path
$SKETCH_NAME = "ESP32-P4-Allsky-Display.ino"
$SKETCH_PATH = Join-Path $SCRIPT_DIR $SKETCH_NAME

# Generate unique build path based on sketch location
$SKETCH_HASH = [System.BitConverter]::ToString([System.Security.Cryptography.MD5]::Create().ComputeHash([System.Text.Encoding]::UTF8.GetBytes($SKETCH_PATH))).Replace("-","")
$BUILD_PATH = Join-Path $env:LOCALAPPDATA "arduino\sketches\$SKETCH_HASH"

# Standard paths
$ARDUINO15_PATH = "$env:LOCALAPPDATA\Arduino15"

# Board configuration
# Note: Using custom partitions.csv for 13MB app / 7MB data OTA support
$FQBN = "esp32:esp32:esp32p4:FlashSize=32M,PartitionScheme=app13M_data7M_32MB,PSRAM=enabled"
$BOARD = "esp32p4"
$CHIP = "esp32p4"

# Tool paths (will be detected dynamically)
$ESP32_PACKAGE = "$ARDUINO15_PATH\packages\esp32"

Write-Host "`n================================" -ForegroundColor Cyan
Write-Host "ESP32-P4 Allsky Display Builder" -ForegroundColor Cyan
Write-Host "================================`n" -ForegroundColor Cyan

# Check if sketch exists
if (-not (Test-Path $SKETCH_PATH)) {
    Write-Host "ERROR: Sketch file not found at $SKETCH_PATH" -ForegroundColor Red
    Write-Host "       Make sure to run this script from the project directory." -ForegroundColor Yellow
    exit 1
}

Write-Host "[1/6] Updating build information..." -ForegroundColor Yellow

# Update build_info.h with current git information
$BUILD_INFO_FILE = Join-Path $SCRIPT_DIR "build_info.h"
try {
    $gitHash = (git rev-parse --short HEAD 2>$null) -replace "`n|`r"
    $gitHashFull = (git rev-parse HEAD 2>$null) -replace "`n|`r"
    $gitBranch = (git branch --show-current 2>$null) -replace "`n|`r"
    
    if (-not $gitHash) { $gitHash = "unknown" }
    if (-not $gitHashFull) { $gitHashFull = "unknown" }
    if (-not $gitBranch) { $gitBranch = "unknown" }
    
    $buildInfoContent = @"
// Auto-generated build information
// This file is updated at compile time by the build script

#ifndef BUILD_INFO_H
#define BUILD_INFO_H

// Build timestamp
#define BUILD_DATE __DATE__
#define BUILD_TIME __TIME__

// Git information (updated by compile script)
#define GIT_COMMIT_HASH "$gitHash"
#define GIT_COMMIT_FULL "$gitHashFull"
#define GIT_BRANCH "$gitBranch"

#endif // BUILD_INFO_H
"@
    
    Set-Content -Path $BUILD_INFO_FILE -Value $buildInfoContent -NoNewline
    Write-Host "      Build info updated: $gitHash ($gitBranch)" -ForegroundColor Green
} catch {
    Write-Host "      Warning: Could not update git info" -ForegroundColor Yellow
}

Write-Host "`n[2/6] Checking Arduino CLI..." -ForegroundColor Yellow

# Try to use Arduino CLI if available
$ARDUINO_CLI = $null
if (Get-Command "arduino-cli" -ErrorAction SilentlyContinue) {
    $ARDUINO_CLI = "arduino-cli"
    Write-Host "      Arduino CLI found!" -ForegroundColor Green
} elseif (Test-Path "$env:ProgramFiles\Arduino CLI\arduino-cli.exe") {
    $ARDUINO_CLI = "$env:ProgramFiles\Arduino CLI\arduino-cli.exe"
    Write-Host "      Arduino CLI found at: $ARDUINO_CLI" -ForegroundColor Green
} elseif (Test-Path "$env:LOCALAPPDATA\Arduino15\arduino-cli.exe") {
    $ARDUINO_CLI = "$env:LOCALAPPDATA\Arduino15\arduino-cli.exe"
    Write-Host "      Arduino CLI found at: $ARDUINO_CLI" -ForegroundColor Green
} else {
    Write-Host "      Arduino CLI not found - will use direct toolchain" -ForegroundColor Yellow
}

# If Arduino CLI is available, use it (much simpler)
if ($ARDUINO_CLI) {
    Write-Host "`n[3/6] Compiling sketch with Arduino CLI..." -ForegroundColor Yellow
    Write-Host "      (This may take a minute...)" -ForegroundColor Gray
    
    # Capture output and show progress
    $compileOutput = & $ARDUINO_CLI compile --fqbn $FQBN `
        --build-path $BUILD_PATH `
        --build-property "build.partitions=partitions" `
        --build-property "upload.maximum_size=10485760" `
        --warnings "default" `
        $SKETCH_PATH 2>&1 | ForEach-Object {
            $line = $_.ToString()
            # Show important messages
            if ($line -match "Sketch uses|Global variables|Compiling sketch|Compiling libraries|Compiling core|Linking") {
                Write-Host "      $line" -ForegroundColor Gray
            }
            # Show errors and warnings
            if ($line -match "error:|warning:|Error|WARNING") {
                Write-Host "      $line" -ForegroundColor Yellow
            }
            $line
        }
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "`nERROR: Compilation failed!" -ForegroundColor Red
        Write-Host "Full output:" -ForegroundColor Yellow
        $compileOutput | ForEach-Object { Write-Host $_ }
        exit 1
    }
    
    Write-Host "`n[4/6] Compilation successful!" -ForegroundColor Green
    
    # Show size information - auto-detect ESP32 version and compiler path
    $ELF_FILE = "$BUILD_PATH\$SKETCH_NAME.elf"
    if (Test-Path $ELF_FILE) {
        Write-Host "`n[5/6] Memory usage:" -ForegroundColor Yellow
        
        # Try to find the size tool
        $SIZE_TOOL = $null
        $ESP32_VERSIONS = Get-ChildItem "$ESP32_PACKAGE\hardware\esp32" -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending
        foreach ($version in $ESP32_VERSIONS) {
            $COMPILER_SEARCH = Get-ChildItem "$ESP32_PACKAGE\tools\esp-rv32" -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending | Select-Object -First 1
            if ($COMPILER_SEARCH) {
                $SIZE_TOOL = Join-Path $COMPILER_SEARCH.FullName "bin\riscv32-esp-elf-size.exe"
                if (Test-Path $SIZE_TOOL) {
                    break
                }
            }
        }
        
        if ($SIZE_TOOL -and (Test-Path $SIZE_TOOL)) {
            & $SIZE_TOOL -A $ELF_FILE
        } else {
            Write-Host "      (Size tool not found - skipping memory details)" -ForegroundColor Gray
        }
    }
    
    # Check if upload should be skipped
    if ($SkipUpload) {
        Write-Host "`n[6/6] Skipping upload (SkipUpload flag set)" -ForegroundColor Yellow
        
        # Copy binary to specified output folder or Downloads folder
        $BIN_FILE = "$BUILD_PATH\$SKETCH_NAME.bin"
        if (Test-Path $BIN_FILE) {
            # Determine destination path
            if ($OutputFolder -and $OutputFolder -ne "") {
                # Use specified output folder
                if (-not (Test-Path $OutputFolder)) {
                    New-Item -ItemType Directory -Path $OutputFolder -Force | Out-Null
                }
                $destFile = Join-Path $OutputFolder "ESP32-P4-Allsky-Display.bin"
            } else {
                # Default to Downloads folder
                $downloadsPath = [System.IO.Path]::Combine($env:USERPROFILE, "Downloads")
                $destFile = Join-Path $downloadsPath "ESP32-P4-Allsky-Display.bin"
            }
            
            Copy-Item $BIN_FILE -Destination $destFile -Force
            
            $fileSize = [math]::Round((Get-Item $destFile).Length / 1MB, 2)
            Write-Host "`n      Binary copied to:" -ForegroundColor Green
            Write-Host "      $destFile" -ForegroundColor Cyan
            Write-Host "      Size: $fileSize MB" -ForegroundColor Gray
            Write-Host "`n      You can upload via:" -ForegroundColor White
            Write-Host "      - ElegantOTA: http://[device-ip]:8080/update" -ForegroundColor Magenta
            Write-Host "      - ArduinoOTA from IDE" -ForegroundColor Magenta
        } else {
            Write-Host "      WARNING: Binary file not found at $BIN_FILE" -ForegroundColor Yellow
        }
        
        exit 0
    }
    
    Write-Host "`n[6/6] Uploading to $ComPort..." -ForegroundColor Yellow
    
    # Capture upload output and show progress on one line
    $uploadOutput = & $ARDUINO_CLI upload --fqbn $FQBN `
        --port $ComPort `
        --input-dir $BUILD_PATH `
        $SKETCH_PATH 2>&1 | ForEach-Object {
            $line = $_.ToString()
            
            # Show upload progress on a single line
            if ($line -match "Writing at 0x[0-9a-f]+ \[.*\]\s+(\d+\.\d+)%") {
                $percent = $matches[1]
                Write-Host "`r      Upload progress: $percent%  " -NoNewline -ForegroundColor Cyan
            }
            # Show other important messages on new lines
            elseif ($line -match "Connecting|Connected to|Chip type:|Serial port|Uploading stub|Running stub|Changing baud|Configuring flash|Erasing flash|Flash memory erased|Wrote \d+ bytes|Hash of data verified|Hard resetting") {
                if ($line -match "Writing at") { 
                    # Skip individual writing progress lines since we show consolidated progress
                } else {
                    Write-Host "`n      $line" -ForegroundColor Gray
                }
            }
            elseif ($line -match "error:|Error") {
                Write-Host "`n      $line" -ForegroundColor Red
            }
            $line
        }
    
    # Add newline after progress
    Write-Host ""
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "`nERROR: Upload failed!" -ForegroundColor Red
        exit 1
    }
    
    Write-Host "`nSUCCESS: Upload completed!" -ForegroundColor Green
}

# Fallback: Use esptool directly for upload only
# (Full compilation without Arduino CLI is too complex)
Write-Host "`nERROR: Arduino CLI is required for compilation." -ForegroundColor Red
Write-Host "Please install Arduino CLI from: https://arduino.github.io/arduino-cli/" -ForegroundColor Yellow
Write-Host "`nAlternatively, you can use this script to upload pre-compiled binaries:" -ForegroundColor Yellow
Write-Host "  .\compile-and-upload.ps1 -UploadOnly`n" -ForegroundColor Cyan

# Check if we're in upload-only mode
if ($args -contains "-UploadOnly") {
    Write-Host "`n[Upload Only Mode]" -ForegroundColor Cyan
    
    # Check if binaries exist
    $BOOTLOADER_BIN = "$BUILD_PATH\$SKETCH_NAME.bootloader.bin"
    $PARTITIONS_BIN = "$BUILD_PATH\$SKETCH_NAME.partitions.bin"
    $APP_BIN = "$BUILD_PATH\$SKETCH_NAME.bin"
    
    # Find boot_app0.bin in ESP32 package
    $BOOT_APP0_BIN = $null
    $ESP32_VERSIONS = Get-ChildItem "$ESP32_PACKAGE\hardware\esp32" -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending
    foreach ($version in $ESP32_VERSIONS) {
        $testPath = Join-Path $version.FullName "tools\partitions\boot_app0.bin"
        if (Test-Path $testPath) {
            $BOOT_APP0_BIN = $testPath
            break
        }
    }
    
    $missing = @()
    if (-not (Test-Path $BOOTLOADER_BIN)) { $missing += "bootloader.bin" }
    if (-not (Test-Path $PARTITIONS_BIN)) { $missing += "partitions.bin" }
    if (-not (Test-Path $APP_BIN)) { $missing += "application.bin" }
    if (-not $BOOT_APP0_BIN -or -not (Test-Path $BOOT_APP0_BIN)) { $missing += "boot_app0.bin" }
    
    if ($missing.Count -gt 0) {
        Write-Host "ERROR: Missing binary files: $($missing -join ', ')" -ForegroundColor Red
        Write-Host "Please compile first using Arduino IDE or install Arduino CLI." -ForegroundColor Yellow
        exit 1
    }
    
    # Find esptool
    $ESPTOOL = $null
    $ESPTOOL_VERSIONS = Get-ChildItem "$ESP32_PACKAGE\tools\esptool_py" -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending | Select-Object -First 1
    if ($ESPTOOL_VERSIONS) {
        $ESPTOOL = Join-Path $ESPTOOL_VERSIONS.FullName "esptool.exe"
    }
    
    if (-not $ESPTOOL -or -not (Test-Path $ESPTOOL)) {
        Write-Host "ERROR: esptool not found in $ESP32_PACKAGE\tools\esptool_py" -ForegroundColor Red
        exit 1
    }
    
    Write-Host "Uploading pre-compiled binaries to $ComPort..." -ForegroundColor Yellow
    
    & $ESPTOOL `
        --chip $CHIP `
        --port $ComPort `
        --baud $BaudRate `
        --before default-reset `
        --after hard-reset `
        write-flash -e -z `
        --flash-mode keep `
        --flash-freq keep `
        --flash-size keep `
        0x2000 $BOOTLOADER_BIN `
        0x8000 $PARTITIONS_BIN `
        0xe000 $BOOT_APP0_BIN `
        0x10000 $APP_BIN
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "`nSUCCESS: Upload completed!" -ForegroundColor Green
    } else {
        Write-Host "`nERROR: Upload failed!" -ForegroundColor Red
        exit 1
    }
}
exit 0
