# build_firmware.ps1
# Builds the ESP32-P4 AllSky Display project (pure ESP-IDF, esp-idf branch)
# with ESP-IDF 5.5.2. Single binary; panel type is a runtime NVS setting.
#
# idf.py is NOT on PATH and does NOT run under Git Bash/MSYS. This PowerShell
# script is the supported entry point: it activates the IDF environment, sets
# PYTHONIOENCODING (the component-manager NOTICE crashes cp1252 stdout otherwise),
# and drives the build out of build_idf/.
#
# Usage:
#   .\build_firmware.ps1                 # Normal build
#   .\build_firmware.ps1 -FullClean      # Clean rebuild
#   .\build_firmware.ps1 -Flash          # Build + serial flash
#   .\build_firmware.ps1 -Flash -Monitor # Build + serial flash + monitor
#   .\build_firmware.ps1 -Monitor        # Build + serial monitor
#   .\build_firmware.ps1 -Port COM7 -Flash
#   .\build_firmware.ps1 -OTA            # Build + OTA flash over the network
#   .\build_firmware.ps1 -OTA -Device allskyesp3236.lan
#   .\build_firmware.ps1 -Out C:\artifacts  # Build + copy app bin to a directory
#   .\build_firmware.ps1 -Downloads      # Build + copy app bin to ~\Downloads

[Diagnostics.CodeAnalysis.SuppressMessageAttribute('PSAvoidUsingPlainTextForPassword', 'Password',
    Justification='Dev tool: password sourced from env var or interactive prompt; not persisted.')]
param(
    [string]$IdfPath   = "C:\Espressif\frameworks\esp-idf-v5.5.2",
    [string]$ToolsPath = "C:\Espressif",
    [switch]$FullClean,
    [switch]$Flash,
    [switch]$Monitor,
    [switch]$OTA,
    [string]$Port,
    [string]$Device    = "allskyesp3236.lan",
    [string]$Password  = $(if ($env:ALLSKY_PASSWORD) { $env:ALLSKY_PASSWORD } else { "" }),
    [string]$Out,
    [switch]$Downloads
)

$ErrorActionPreference = "Stop"

$ProjectDir = $PSScriptRoot
$BuildDir   = Join-Path $ProjectDir "build_idf"

# Interactive menu when invoked without arguments
if ($PSBoundParameters.Count -eq 0) {
    function Show-Menu {
        param($IdfPath, $FullClean, $Flash, $Monitor, $OTA, $Device)
        Write-Host ""
        Write-Host "  ESP32-P4 AllSky Display - Build Options" -ForegroundColor Cyan
        Write-Host "  =======================================" -ForegroundColor Cyan
        Write-Host ""
        $onOff = { param($b) if ($b) { "Yes" } else { "No" } }
        $onClr = { param($b) if ($b) { "Yellow" } else { "DarkGray" } }
        Write-Host "  [1] Full Clean:   " -NoNewline; Write-Host (& $onOff $FullClean) -ForegroundColor (& $onClr $FullClean)
        Write-Host "  [2] Serial Flash: " -NoNewline; Write-Host (& $onOff $Flash)     -ForegroundColor (& $onClr $Flash)
        Write-Host "  [3] Monitor:      " -NoNewline; Write-Host (& $onOff $Monitor)   -ForegroundColor (& $onClr $Monitor)
        Write-Host "  [4] OTA Flash:    " -NoNewline; Write-Host (& $onOff $OTA)       -ForegroundColor (& $onClr $OTA)
        Write-Host "  [5] Device:       " -NoNewline; Write-Host $Device               -ForegroundColor White
        Write-Host "  [6] IDF Path:     " -NoNewline; Write-Host $IdfPath              -ForegroundColor White
        Write-Host ""
    }

    $menuLoop = $true
    while ($menuLoop) {
        Show-Menu -IdfPath $IdfPath -FullClean $FullClean -Flash $Flash -Monitor $Monitor -OTA $OTA -Device $Device
        $selection = Read-Host "  Enter option numbers to toggle (e.g. 1,3), or press Enter to build"
        if ([string]::IsNullOrWhiteSpace($selection)) { $menuLoop = $false; continue }
        foreach ($choice in ($selection -split '[,\s]+')) {
            switch ($choice.Trim()) {
                '1' { $FullClean = -not $FullClean }
                '2' { $Flash     = -not $Flash }
                '3' { $Monitor   = -not $Monitor }
                '4' { $OTA       = -not $OTA }
                '5' {
                    $newDevice = Read-Host "  Device [$Device]"
                    if (-not [string]::IsNullOrWhiteSpace($newDevice)) { $Device = $newDevice.Trim() }
                }
                '6' {
                    $newPath = Read-Host "  IDF Path [$IdfPath]"
                    if (-not [string]::IsNullOrWhiteSpace($newPath)) { $IdfPath = $newPath }
                }
                default { Write-Host "  Unknown option: $choice" -ForegroundColor Red }
            }
        }
    }

    $summary = @("Build")
    if ($FullClean) { $summary += "FullClean" }
    if ($Flash)     { $summary += "Flash" }
    if ($Monitor)   { $summary += "Monitor" }
    if ($OTA)       { $summary += "OTA -> $Device" }
    Write-Host ""
    Write-Host "  Building: $($summary -join ' + ')" -ForegroundColor Cyan
    Write-Host ""
}

# Verify ESP-IDF path exists
if (-not (Test-Path (Join-Path $IdfPath "export.ps1"))) {
    Write-Error "ESP-IDF not found at $IdfPath. Check the path and try again."
    exit 1
}

# Required: the ESP-IDF component manager prints a Unicode NOTICE that crashes on
# a cp1252 console. Force UTF-8 stdout before any idf.py call.
$env:PYTHONIOENCODING = 'utf-8'
$env:IDF_TOOLS_PATH   = $ToolsPath

# Activate ESP-IDF environment (idf.py is not otherwise on PATH)
Write-Host "Activating ESP-IDF environment from $IdfPath ..." -ForegroundColor Cyan
& (Join-Path $IdfPath "export.ps1")
if ($LASTEXITCODE -and $LASTEXITCODE -ne 0) {
    Write-Error "Failed to activate ESP-IDF environment."
    exit 1
}

# Full clean if requested, or if the build cache references a different Python
$CmakeCache = Join-Path $BuildDir "CMakeCache.txt"
if (-not $FullClean -and (Test-Path $CmakeCache)) {
    $cached = Select-String -Path $CmakeCache -Pattern "PYTHON.*=(.+)" -AllMatches | ForEach-Object { $_.Matches[0].Groups[1].Value } | Select-Object -First 1
    $activePython = (Get-Command python -ErrorAction SilentlyContinue).Source
    if ($cached -and $activePython -and ($cached -ne $activePython)) {
        Write-Host "Detected Python mismatch (cached: $cached, active: $activePython). Running fullclean ..." -ForegroundColor Yellow
        $FullClean = $true
    }
}

if ($FullClean) {
    Write-Host "`nRunning fullclean ..." -ForegroundColor Yellow
    Push-Location $ProjectDir
    try {
        idf.py -B build_idf fullclean
        # fullclean only removes build_idf/; delete sdkconfig too so it regenerates
        # from sdkconfig.defaults. A stale local sdkconfig would otherwise override
        # the defaults and drift the build from CI.
        if (Test-Path sdkconfig) {
            Remove-Item -Force sdkconfig
            Write-Host "Removed stale sdkconfig (regenerates from sdkconfig.defaults)" -ForegroundColor Yellow
        }
    } finally {
        Pop-Location
    }
}

# Build the project
Write-Host "`nBuilding project ..." -ForegroundColor Cyan
Push-Location $ProjectDir
try {
    idf.py -B build_idf build
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Build failed."
        exit 1
    }
} finally {
    Pop-Location
}

# Resolve the app binary from the CMake project name (expected: allsky_display.bin)
$ProjectDescJson = Join-Path $BuildDir "project_description.json"
if (Test-Path $ProjectDescJson) {
    $projDesc = Get-Content $ProjectDescJson -Raw | ConvertFrom-Json
    $AppBin = Join-Path $BuildDir "$($projDesc.app_bin)"
} else {
    $AppBin = Get-ChildItem -Path $BuildDir -Filter "*.bin" -File |
              Where-Object { $_.Name -notmatch "bootloader|partition" } |
              Sort-Object Length -Descending |
              Select-Object -First 1 -ExpandProperty FullName
}

if (-not $AppBin -or -not (Test-Path $AppBin)) {
    Write-Error "Missing build artifact: $AppBin"
    exit 1
}

$BinSize   = (Get-Item $AppBin).Length
$BinSizeMB = [math]::Round($BinSize / 1MB, 2)

Write-Host "`nBuild successful!" -ForegroundColor Green
Write-Host "  App binary: $AppBin" -ForegroundColor Green
Write-Host "              $BinSizeMB MB ($BinSize bytes)" -ForegroundColor Green

# Copy build output to a directory (only after a successful build). -Out wins
# over -Downloads if both are given. The app bin path is the one resolved above.
$OutDir = if ($Out) { $Out } elseif ($Downloads) { Join-Path $env:USERPROFILE "Downloads" } else { $null }
if ($OutDir) {
    if (-not (Test-Path $OutDir)) {
        New-Item -ItemType Directory -Path $OutDir -Force | Out-Null
    }
    $OutBin = Join-Path $OutDir (Split-Path $AppBin -Leaf)
    Copy-Item -Path $AppBin -Destination $OutBin -Force
    Write-Host "`nCopied app binary to: $OutBin" -ForegroundColor Green

    # Also produce a merged flash-from-scratch image next to the app bin.
    $MergedBin = Join-Path $OutDir "allsky_display_merged.bin"
    Push-Location $ProjectDir
    try {
        idf.py -B build_idf merge-bin -o $MergedBin
        if ($LASTEXITCODE -eq 0 -and (Test-Path $MergedBin)) {
            Write-Host "Copied merged image to:  $MergedBin" -ForegroundColor Green
        } else {
            Write-Warning "merge-bin did not produce $MergedBin; app bin was still copied."
        }
    } finally {
        Pop-Location
    }
}

# Serial flash and/or monitor over USB (default path)
if ($Flash -or $Monitor) {
    $idfArgs = @("-B", "build_idf")
    if ($Port) { $idfArgs += @("-p", $Port) }
    if ($Flash)   { $idfArgs += "flash" }
    if ($Monitor) { $idfArgs += "monitor" }

    Write-Host "`nRunning: idf.py $($idfArgs -join ' ') ..." -ForegroundColor Cyan
    Push-Location $ProjectDir
    try {
        idf.py @idfArgs
        if ($LASTEXITCODE -ne 0) {
            Write-Error "Serial flash/monitor failed."
            exit 1
        }
    } finally {
        Pop-Location
    }
}

# OTA flash over the network if requested.
# The device requires a session cookie: POST /api/login (JSON password) then a
# multipart/form-data upload to /update.
if ($OTA) {
    if ([string]::IsNullOrEmpty($Password)) {
        $sec = Read-Host -Prompt "Admin password for $Device" -AsSecureString
        $bstr = [System.Runtime.InteropServices.Marshal]::SecureStringToBSTR($sec)
        try { $Password = [System.Runtime.InteropServices.Marshal]::PtrToStringBSTR($bstr) }
        finally { [System.Runtime.InteropServices.Marshal]::ZeroFreeBSTR($bstr) }
    }

    # The web server listens on :8080. Append it when the caller gave a bare host.
    $OtaHost = if ($Device -match ':\d+$') { $Device } else { "${Device}:8080" }

    Write-Host "`nUploading OTA firmware to $OtaHost ..." -ForegroundColor Cyan

    # 1. Login for a session cookie
    $LoginUrl  = "http://${OtaHost}/api/login"
    $loginBody = @{ password = $Password } | ConvertTo-Json -Compress
    $loginResp = Invoke-WebRequest -Uri $LoginUrl -Method Post `
        -Body $loginBody -ContentType "application/json" -TimeoutSec 15 -UseBasicParsing
    if ($loginResp.StatusCode -ne 200) {
        Write-Error "OTA login failed: HTTP $($loginResp.StatusCode)"
        exit 1
    }
    $setCookie = $loginResp.Headers['Set-Cookie']
    if ($setCookie -is [array]) { $setCookie = $setCookie[0] }
    if (-not $setCookie -or $setCookie -notmatch 'session=([^;]+)') {
        Write-Error "OTA login succeeded but no session cookie was returned."
        exit 1
    }
    $sessionCookie = "session=$($Matches[1])"

    # 2. Build a multipart/form-data body (the /update handler streams past the
    #    part header to the boundary; the field name is not checked).
    $boundary  = "----AllskyOta$([guid]::NewGuid().ToString('N'))"
    $fileBytes = [System.IO.File]::ReadAllBytes($AppBin)
    $enc       = [System.Text.Encoding]::ASCII
    $preamble  = "--$boundary`r`n" +
                 "Content-Disposition: form-data; name=`"firmware`"; filename=`"$(Split-Path $AppBin -Leaf)`"`r`n" +
                 "Content-Type: application/octet-stream`r`n`r`n"
    $epilogue  = "`r`n--$boundary--`r`n"

    $ms = New-Object System.IO.MemoryStream
    $preBytes = $enc.GetBytes($preamble)
    $epiBytes = $enc.GetBytes($epilogue)
    $ms.Write($preBytes, 0, $preBytes.Length)
    $ms.Write($fileBytes, 0, $fileBytes.Length)
    $ms.Write($epiBytes, 0, $epiBytes.Length)
    $body = $ms.ToArray()
    $ms.Dispose()

    $OtaUrl  = "http://${OtaHost}/update"
    $otaResp = Invoke-WebRequest -Uri $OtaUrl -Method Post `
        -Body $body `
        -ContentType "multipart/form-data; boundary=$boundary" `
        -Headers @{ Cookie = $sessionCookie } `
        -TimeoutSec 600 -UseBasicParsing
    if ($otaResp.StatusCode -lt 200 -or $otaResp.StatusCode -ge 300) {
        Write-Error "OTA upload failed: HTTP $($otaResp.StatusCode)"
        exit 1
    }

    Write-Host "Device $Device updated successfully!" -ForegroundColor Green
}
