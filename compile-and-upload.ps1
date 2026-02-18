# ============================================================================
# ESP32-P4 Allsky Display - Build & Upload Script
# ============================================================================
#
# What this does:
# Compiles the ESP32-P4 firmware and uploads it to your device via USB serial.
# Handles Arduino library patching, git metadata injection, and memory reporting.
#
# Prerequisites:
# - Arduino CLI installed (script auto-detects common locations)
# - ESP32 board support (esp32:esp32 package)
# - Git (optional, for build metadata)
#
# Quick Start:
#   .\compile-and-upload.ps1                  # Upload to COM3 at 921600 baud
#   .\compile-and-upload.ps1 -ComPort COM5    # Different port
#   .\compile-and-upload.ps1 -SkipUpload      # Just compile, save .bin to Downloads
#
# Pro-Tip: If you're doing network-only OTA updates, use -SkipUpload and grab
# the .bin file from your Downloads folder. Upload it via http://[device]:8080/update
#
# Parameters
param(
    # Serial port where your ESP32-P4 is connected (Windows: COM3, Linux: /dev/ttyUSB0)
    [string]$ComPort = "COM3",
    
    # Upload speed - 921600 is the fastest stable rate for ESP32-P4
    # Drop to 460800 if you get checksum errors during upload
    [string]$BaudRate = "921600",
    
    # Where to save the .bin file when using -SkipUpload
    # Defaults to your Downloads folder if not specified
    [string]$OutputFolder = "",
    
    # Alternative parameter name for OutputFolder (both work the same way)
    [string]$OutputPath = "",
    
    # Set this to compile only - no upload happens
    # Use when device isn't connected via USB (network OTA scenarios)
    [switch]$SkipUpload
)

# ============================================================================
# Path Detection
# ============================================================================
# We need to find the sketch file and create a build directory that Arduino
# CLI can use. The build path is MD5-hashed because Arduino CLI uses the same
# mechanism - this prevents conflicts with IDE builds.

$SCRIPT_DIR = Split-Path -Parent $MyInvocation.MyCommand.Path
$SKETCH_NAME = "ESP32-P4-Allsky-Display.ino"
$SKETCH_PATH = Join-Path $SCRIPT_DIR $SKETCH_NAME

# Arduino CLI uses MD5 of the sketch path to create unique build folders
# This matches Arduino IDE's behavior and prevents build cache conflicts
$SKETCH_HASH = [System.BitConverter]::ToString([System.Security.Cryptography.MD5]::Create().ComputeHash([System.Text.Encoding]::UTF8.GetBytes($SKETCH_PATH))).Replace("-","")
$BUILD_PATH = Join-Path $env:LOCALAPPDATA "arduino\sketches\$SKETCH_HASH"

# Arduino packages and libraries live here (on Windows)
$ARDUINO15_PATH = "$env:LOCALAPPDATA\Arduino15"

# ============================================================================
# Board Configuration
# ============================================================================
# These settings match the hardware requirements for ESP32-P4 with DSI display.
# Watch out for this: PSRAM *must* be enabled or compilation fails - the firmware
# requires PSRAM for image buffers (we allocate 4MB+ for scaled images).

# FQBN (Fully Qualified Board Name) defines every compile-time setting
# FlashSize=32M: This is the physical flash chip size
# PartitionScheme=app13M_data7M_32MB: Custom OTA layout (see partitions.csv)
#   - 13MB per app partition (A/B for rollback support)
#   - 7MB for data/config storage
# PSRAM=enabled: Required - firmware uses 10MB+ PSRAM for image processing
$FQBN = "esp32:esp32:esp32p4:FlashSize=32M,PartitionScheme=app13M_data7M_32MB,PSRAM=enabled"
$BOARD = "esp32p4"
$CHIP = "esp32p4"

# Where the ESP32 toolchain and compilers are installed
# This gets auto-detected, but we set the base path here
$ESP32_PACKAGE = "$ARDUINO15_PATH\packages\esp32"

# ============================================================================
# Startup Banner
# ============================================================================
Write-Host "`n================================" -ForegroundColor Cyan
Write-Host "ESP32-P4 Allsky Display Builder" -ForegroundColor Cyan
Write-Host "================================`n" -ForegroundColor Cyan

# Basic sanity check - make sure we're running from the right place
# This catches the "I double-clicked the script from Downloads" mistake
if (-not (Test-Path $SKETCH_PATH)) {
    Write-Host "ERROR: Sketch file not found at $SKETCH_PATH" -ForegroundColor Red
    Write-Host "       Make sure to run this script from the project directory." -ForegroundColor Yellow
    exit 1
}

# ============================================================================
# Step 1/6: Build Metadata Injection
# ============================================================================
# We generate build_info.h with git metadata so the firmware knows what version
# it's running. This shows up in the web UI and logs - helps with debugging.
#
# Pro-Tip: If you're working on a feature branch, this will show the branch name
# in the device's /api/info endpoint. Makes it easy to tell which version is deployed.

Write-Host "[1/6] Updating build information..." -ForegroundColor Yellow

$BUILD_INFO_FILE = Join-Path $SCRIPT_DIR "build_info.h"
try {
    # Grab the current git state (short hash, full hash, branch name)
    # The 2>$null suppresses errors if we're not in a git repo
    $gitHash = (git rev-parse --short HEAD 2>$null) -replace "`n|`r"
    $gitHashFull = (git rev-parse HEAD 2>$null) -replace "`n|`r"
    $gitBranch = (git branch --show-current 2>$null) -replace "`n|`r"
    
    # Fallback to "unknown" if git commands failed (not a git repo, git not installed, etc.)
    if (-not $gitHash) { $gitHash = "unknown" }
    if (-not $gitHashFull) { $gitHashFull = "unknown" }
    if (-not $gitBranch) { $gitBranch = "unknown" }
    
    # Generate the C header file with #defines
    # __DATE__ and __TIME__ are compiler macros that get expanded at compile time
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
    
    # Write the file (NoNewline prevents extra blank line at end)
    Set-Content -Path $BUILD_INFO_FILE -Value $buildInfoContent -NoNewline
    Write-Host "      Build info updated: $gitHash ($gitBranch)" -ForegroundColor Green
} catch {
    # Not a fatal error - build can proceed without git metadata
    Write-Host "      Warning: Could not update git info" -ForegroundColor Yellow
}

# ============================================================================
# Step 2/6: Arduino CLI Detection
# ============================================================================
# We need Arduino CLI to compile the sketch. This checks common install locations
# before giving up. If you installed Arduino CLI via the installer, it should be
# in Program Files. If you used the portable ZIP, it might be in Arduino15.
#
# Watch out for this: The script won't work without Arduino CLI. Using esptool
# directly for compilation would require manually tracking every library dependency,
# which is a nightmare. Just install Arduino CLI if it's missing.

Write-Host "`n[2/6] Checking Arduino CLI..." -ForegroundColor Yellow

$ARDUINO_CLI = $null

# Check 1: Is arduino-cli in PATH?
# This catches standard installs via MSI or Chocolatey
if (Get-Command "arduino-cli" -ErrorAction SilentlyContinue) {
    $ARDUINO_CLI = "arduino-cli"
    Write-Host "      Arduino CLI found!" -ForegroundColor Green
} 
# Check 2: Standard Program Files location (default MSI install path)
elseif (Test-Path "$env:ProgramFiles\Arduino CLI\arduino-cli.exe") {
    $ARDUINO_CLI = "$env:ProgramFiles\Arduino CLI\arduino-cli.exe"
    Write-Host "      Arduino CLI found at: $ARDUINO_CLI" -ForegroundColor Green
} 
# Check 3: Arduino15 folder (some users put portable versions here)
elseif (Test-Path "$env:LOCALAPPDATA\Arduino15\arduino-cli.exe") {
    $ARDUINO_CLI = "$env:LOCALAPPDATA\Arduino15\arduino-cli.exe"
    Write-Host "      Arduino CLI found at: $ARDUINO_CLI" -ForegroundColor Green
} 
# Not found - script will exit later with install instructions
else {
    Write-Host "      Arduino CLI not found - will use direct toolchain" -ForegroundColor Yellow
}

# ============================================================================
# Step 3/6: Compilation
# ============================================================================
# This is where the magic happens. Arduino CLI:
# 1. Resolves all library dependencies
# 2. Compiles core ESP32 framework
# 3. Compiles project .cpp files
# 4. Links everything into a .elf binary
# 5. Converts .elf to .bin for flashing
#
# The first compile takes 2-3 minutes because it builds the entire ESP32 core.
# Subsequent compiles are faster (15-30 seconds) thanks to caching.
#
# Pro-Tip: If you see linker errors about "section will not fit in region",
# you've exceeded the 13MB app partition size. This usually happens if you
# enable debug logging in a production build.

if ($ARDUINO_CLI) {
    Write-Host "`n[3/6] Compiling sketch with Arduino CLI..." -ForegroundColor Yellow
    Write-Host "      (This may take a minute...)" -ForegroundColor Gray
    
    # The compile command explained:
    # --fqbn: Board definition (includes flash size, partition scheme, PSRAM config)
    # --build-path: Where to put compiled objects (.o files, .elf, .bin)
    # --build-property "build.partitions=partitions": Use our custom partitions.csv
    # --build-property "upload.maximum_size=10485760": Enforce 10MB app size limit
    # --warnings "default": Show warnings (change to "all" if debugging compile issues)
    
    $compileOutput = & $ARDUINO_CLI compile --fqbn $FQBN `
        --build-path $BUILD_PATH `
        --build-property "build.partitions=partitions" `
        --build-property "upload.maximum_size=10485760" `
        --warnings "default" `
        $SKETCH_PATH 2>&1 | ForEach-Object {
            $line = $_.ToString()
            
            # Filter output to show only important progress messages
            # Full output is captured in $compileOutput if something goes wrong
            if ($line -match "Sketch uses|Global variables|Compiling sketch|Compiling libraries|Compiling core|Linking") {
                Write-Host "      $line" -ForegroundColor Gray
            }
            # Always show errors and warnings immediately
            if ($line -match "error:|warning:|Error|WARNING") {
                Write-Host "      $line" -ForegroundColor Yellow
            }
            $line
        }
    
    # Check if compilation succeeded
    # LASTEXITCODE is PowerShell's equivalent of $? in bash
    if ($LASTEXITCODE -ne 0) {
        Write-Host "`nERROR: Compilation failed!" -ForegroundColor Red
        Write-Host "Full output:" -ForegroundColor Yellow
        $compileOutput | ForEach-Object { Write-Host $_ }
        exit 1
    }
    
    Write-Host "`n[4/6] Compilation successful!" -ForegroundColor Green
    
    # ========================================================================
    # Step 4/6: Memory Usage Report
    # ========================================================================
    # The size tool shows how much flash and RAM your firmware uses. This is
    # critical for ESP32-P4 because you need to stay under the partition limits.
    #
    # Watch out for this: If you see .text section over 10MB, you've exceeded
    # the app partition. You'll need to disable features or optimize code.
    
    $ELF_FILE = "$BUILD_PATH\$SKETCH_NAME.elf"
    if (Test-Path $ELF_FILE) {
        Write-Host "`n[5/6] Memory usage:" -ForegroundColor Yellow
        
        # Find the size tool in the ESP32 toolchain
        # ESP32-P4 uses RISC-V architecture, so we need riscv32-esp-elf-size
        # (older ESP32 boards use Xtensa and have xtensa-esp32-elf-size instead)
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
        
        # Run the size tool with -A flag (shows all sections)
        # Output looks like:
        #   .text    = flash code (must fit in 13MB)
        #   .data    = initialized RAM
        #   .bss     = uninitialized RAM
        if ($SIZE_TOOL -and (Test-Path $SIZE_TOOL)) {
            & $SIZE_TOOL -A $ELF_FILE
        } else {
            Write-Host "      (Size tool not found - skipping memory details)" -ForegroundColor Gray
        }
    }
    
    # ========================================================================
    # Step 6/6 (Alternative Path): Skip Upload Mode
    # ========================================================================
    # When -SkipUpload is set, we just save the .bin file instead of uploading.
    # Use this when:
    # - Device isn't connected via USB
    # - You want to do OTA updates over WiFi
    # - You're compiling on one machine, uploading from another
    #
    # Pro-Tip: The .bin file can be uploaded via the web UI at http://[device]:8080/update
    # You can also use ArduinoOTA from the IDE if you prefer that workflow.
    
    if ($SkipUpload) {
        Write-Host "`n[6/6] Skipping upload (SkipUpload flag set)" -ForegroundColor Yellow
        
        $BIN_FILE = "$BUILD_PATH\$SKETCH_NAME.bin"
        if (Test-Path $BIN_FILE) {
            # Figure out where to save the binary
            # Support both -OutputPath and -OutputFolder (they're aliases)
            $targetPath = if ($OutputPath -and $OutputPath -ne "") { $OutputPath } else { $OutputFolder }
            
            if ($targetPath -and $targetPath -ne "") {
                # User specified a folder - create it if it doesn't exist
                if (-not (Test-Path $targetPath)) {
                    New-Item -ItemType Directory -Path $targetPath -Force | Out-Null
                }
                $destFile = Join-Path $targetPath "ESP32-P4-Allsky-Display.bin"
            } else {
                # Default behavior: save to Downloads folder
                # This is where most users expect to find compiled binaries
                $downloadsPath = [System.IO.Path]::Combine($env:USERPROFILE, "Downloads")
                $destFile = Join-Path $downloadsPath "ESP32-P4-Allsky-Display.bin"
            }
            
            Copy-Item $BIN_FILE -Destination $destFile -Force
            
            # Show the file size so you can verify it's reasonable
            # Should be 2-4MB for this firmware (depends on enabled features)
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
    
    # ========================================================================
    # Step 6/6: Serial Upload
    # ========================================================================
    # Upload the compiled binary to the ESP32 via USB serial. This uses esptool
    # under the hood (Arduino CLI wraps it for you).
    #
    # Watch out for this: If upload fails with "Failed to connect", try:
    # 1. Hold the BOOT button on the board while plugging in USB
    # 2. Check Device Manager - COM port might be different
    # 3. Try a different USB cable (some are charge-only, no data)
    # 4. Lower baud rate to 460800 if you get checksum errors
    #
    # The upload process:
    # 1. Connect to ESP32 bootloader
    # 2. Erase flash sectors
    # 3. Write bootloader (0x2000)
    # 4. Write partition table (0x8000)
    # 5. Write app binary (0x10000)
    # 6. Verify checksums
    # 7. Hard reset the chip
    
    Write-Host "`n[6/6] Uploading to $ComPort..." -ForegroundColor Yellow
    
    # The upload command filters output for readability
    # We show a percentage progress bar instead of per-block status
    $uploadOutput = & $ARDUINO_CLI upload --fqbn $FQBN `
        --port $ComPort `
        --input-dir $BUILD_PATH `
        $SKETCH_PATH 2>&1 | ForEach-Object {
            $line = $_.ToString()
            
            # Extract and display upload percentage on a single line
            # This prevents console spam - you just see "Upload progress: 75%"
            if ($line -match "Writing at 0x[0-9a-f]+ \[.*\]\s+(\d+\.\d+)%") {
                $percent = $matches[1]
                Write-Host "`r      Upload progress: $percent%  " -NoNewline -ForegroundColor Cyan
            }
            # Show milestone messages on new lines (connection, flash erase, etc.)
            elseif ($line -match "Connecting|Connected to|Chip type:|Serial port|Uploading stub|Running stub|Changing baud|Configuring flash|Erasing flash|Flash memory erased|Wrote \d+ bytes|Hash of data verified|Hard resetting") {
                if ($line -match "Writing at") { 
                    # Skip individual block writes - we already show consolidated progress above
                } else {
                    Write-Host "`n      $line" -ForegroundColor Gray
                }
            }
            # Always show errors immediately
            elseif ($line -match "error:|Error") {
                Write-Host "`n      $line" -ForegroundColor Red
            }
            $line
        }
    
    # Add newline after progress bar to prevent next prompt from overwriting it
    Write-Host ""
    
    # Check upload result
    if ($LASTEXITCODE -ne 0) {
        Write-Host "`nERROR: Upload failed!" -ForegroundColor Red
        exit 1
    }
    
    Write-Host "`nSUCCESS: Upload completed!" -ForegroundColor Green
}

# ============================================================================
# Fallback Error Handler
# ============================================================================
# If we reach this point, Arduino CLI wasn't found. We can't compile without it
# because tracking all the library dependencies manually is impractical.
#
# We do support upload-only mode (see below) if you have pre-compiled binaries,
# but that's rare - most users should just install Arduino CLI.

Write-Host "`nERROR: Arduino CLI is required for compilation." -ForegroundColor Red
Write-Host "Please install Arduino CLI from: https://arduino.github.io/arduino-cli/" -ForegroundColor Yellow
Write-Host "`nAlternatively, you can use this script to upload pre-compiled binaries:" -ForegroundColor Yellow
Write-Host "  .\compile-and-upload.ps1 -UploadOnly`n" -ForegroundColor Cyan

# ============================================================================
# Upload-Only Mode (Advanced)
# ============================================================================
# This mode uploads pre-compiled .bin files directly via esptool, bypassing
# Arduino CLI entirely. Only use this if you:
# - Have binaries from a previous compile
# - Are flashing multiple boards with the same firmware
# - Compiled on a different machine
#
# Watch out for this: The binaries must be from the *same* board config (FQBN).
# Mixing binaries from different partition schemes will brick your flash layout.

if ($args -contains "-UploadOnly") {
    Write-Host "`n[Upload Only Mode]" -ForegroundColor Cyan
    
    # Verify all required binaries exist before trying to upload
    # ESP32 needs four separate binary files written to specific flash addresses:
    # 1. bootloader.bin   @ 0x2000  - First stage bootloader (CPU startup)
    # 2. partitions.bin   @ 0x8000  - Partition table (where things live in flash)
    # 3. boot_app0.bin    @ 0xe000  - OTA data (which app partition to boot)
    # 4. app.bin          @ 0x10000 - Your actual firmware
    
    $BOOTLOADER_BIN = "$BUILD_PATH\$SKETCH_NAME.bootloader.bin"
    $PARTITIONS_BIN = "$BUILD_PATH\$SKETCH_NAME.partitions.bin"
    $APP_BIN = "$BUILD_PATH\$SKETCH_NAME.bin"
    
    # boot_app0.bin is in the ESP32 package (not in build output)
    # This binary tells the bootloader "boot from app partition 0" initially
    $BOOT_APP0_BIN = $null
    $ESP32_VERSIONS = Get-ChildItem "$ESP32_PACKAGE\hardware\esp32" -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending
    foreach ($version in $ESP32_VERSIONS) {
        $testPath = Join-Path $version.FullName "tools\partitions\boot_app0.bin"
        if (Test-Path $testPath) {
            $BOOT_APP0_BIN = $testPath
            break
        }
    }
    
    # Check what's missing and bail out if we can't find everything
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
    
    # Find esptool in the ESP32 package
    # We auto-detect the latest version to avoid hardcoding version numbers
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
    
    # esptool parameters explained:
    # --chip esp32p4: Target chip type
    # --before default-reset: Reset ESP32 before upload
    # --after hard-reset: Hard reset after upload (boots new firmware)
    # write-flash: Main command
    #   -e: Erase flash before writing
    #   -z: Compress data during transfer (faster upload)
    #   --flash-mode keep: Don't change SPI flash mode (use existing config)
    #   --flash-freq keep: Don't change SPI flash frequency
    #   --flash-size keep: Don't change flash size setting
    # The address/file pairs tell esptool where to write each binary
    
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

# End of script - clean exit
exit 0
