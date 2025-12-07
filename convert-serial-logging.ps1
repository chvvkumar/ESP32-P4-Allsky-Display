<#
.SYNOPSIS
    Convert Serial.println/printf calls to WebSocket-enabled LOG macros with severity detection

.DESCRIPTION
    This script automatically replaces Serial.println() and Serial.printf() calls with
    LOG_DEBUG/INFO/WARNING/ERROR/CRITICAL macros that broadcast to both Serial and WebSocket.
    It intelligently detects severity levels based on message content.

.PARAMETER DryRun
    Show what would be changed without actually modifying files

.PARAMETER BackupFolder
    Folder to store backups (default: .\backups\serial-conversion-TIMESTAMP)

.PARAMETER Files
    Specific files to convert (default: all .ino, .cpp, .h files in current directory)

.EXAMPLE
    .\convert-serial-logging.ps1 -DryRun
    
.EXAMPLE
    .\convert-serial-logging.ps1 -Files @("ESP32-P4-Allsky-Display.ino","network_manager.cpp")
#>

param(
    [switch]$DryRun,
    [string]$BackupFolder = "",
    [string[]]$Files = @()
)

# Color output functions
function Write-Info([string]$msg) { Write-Host $msg -ForegroundColor Cyan }
function Write-Success([string]$msg) { Write-Host $msg -ForegroundColor Green }
function Write-Warn([string]$msg) { Write-Host $msg -ForegroundColor Yellow }
function Write-Err([string]$msg) { Write-Host $msg -ForegroundColor Red }

# Severity detection based on message content
function Get-Severity([string]$message) {
    if ($message -imatch '(CRITICAL|FATAL|PANIC|ASSERT)') { return 'CRITICAL' }
    elseif ($message -imatch '(ERROR|FAIL)') { return 'ERROR' }
    elseif ($message -imatch '(WARNING|WARN)') { return 'WARNING' }
    elseif ($message -imatch '(DEBUG|TRACE|DUMP)') { return 'DEBUG' }
    else { return 'INFO' }
}

# Extract the message from Serial.println/printf call
function Get-MessageFromCall([string]$call) {
    if ($call -match 'Serial\.(println|printf)\s*\(\s*"([^"]*)"') {
        return $Matches[2]
    }
    elseif ($call -match "Serial\.(println|printf)\s*\(\s*'([^']*)'") {
        return $Matches[2]
    }
    elseif ($call -match 'Serial\.(println|printf)\s*\(\s*(\w+)') {
        return $Matches[2]
    }
    return ""
}

# Main conversion function
function Convert-File {
    param([string]$FilePath, [switch]$DryRun)
    
    Write-Info "`nProcessing: $FilePath"
    
    $lines = Get-Content $FilePath
    $newLines = @()
    $changeCount = 0
    
    $lineNum = 0
    foreach ($line in $lines) {
        $lineNum++
        $originalLine = $line
        $changed = $false
        $severity = ""
        
        # Convert Serial.println()
        if ($line -match 'Serial\.println\s*\(') {
            $message = Get-MessageFromCall $line
            $severity = Get-Severity $message
            $line = $line -replace 'Serial\.println\s*\(', "LOG_${severity}("
            $changed = $true
        }
        # Convert Serial.printf()
        elseif ($line -match 'Serial\.printf\s*\(') {
            $message = Get-MessageFromCall $line
            $severity = Get-Severity $message
            $line = $line -replace 'Serial\.printf\s*\(', "LOG_${severity}_F("
            $changed = $true
        }
        # Convert Serial.print()
        elseif ($line -match 'Serial\.print\s*\(') {
            $message = Get-MessageFromCall $line
            $severity = Get-Severity $message
            $line = $line -replace 'Serial\.print\s*\(([^)]+)\)', "logPrint(`$1, LOG_${severity})"
            $changed = $true
        }
        
        if ($changed) {
            $changeCount++
            Write-Host "    Line $lineNum [$severity]:" -ForegroundColor Yellow
            Write-Host "      - $($originalLine.Trim())" -ForegroundColor Red
            Write-Host "      + $($line.Trim())" -ForegroundColor Green
        }
        
        $newLines += $line
    }
    
    if ($changeCount -gt 0) {
        Write-Success "  Found $changeCount Serial calls to convert"
        
        if (-not $DryRun) {
            $newContent = $newLines -join "`r`n"
            Set-Content -Path $FilePath -Value $newContent -NoNewline
            Write-Success "  [OK] File updated"
        }
        else {
            Write-Info "  (Dry run - no changes made)"
        }
    }
    else {
        Write-Info "  No Serial calls found"
    }
    
    return $changeCount
}

# Main script execution
Write-Info "======================================================"
Write-Info "  Serial -> WebSocket Logging Conversion Script"
Write-Info "======================================================"

# Create backup if not dry run
if (-not $DryRun) {
    if ($BackupFolder -eq "") {
        $timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
        $BackupFolder = ".\backups\serial-conversion-$timestamp"
    }
    
    if (-not (Test-Path $BackupFolder)) {
        New-Item -ItemType Directory -Path $BackupFolder -Force | Out-Null
        Write-Success "Created backup folder: $BackupFolder"
    }
}

# Determine files to process
if ($Files.Count -eq 0) {
    $Files = @(Get-ChildItem -Path . -Include "*.ino","*.cpp","*.h" -File | 
               Where-Object { $_.Name -notmatch "(displays_config|web_config_html|wifi_qr_code)" } | 
               Select-Object -ExpandProperty Name)
    
    Write-Info "Auto-detected $($Files.Count) files to process"
}

# Backup files
if (-not $DryRun) {
    Write-Info "`nBacking up files..."
    foreach ($file in $Files) {
        if (Test-Path $file) {
            Copy-Item $file -Destination (Join-Path $BackupFolder $file) -Force
        }
    }
    Write-Success "Backup complete"
}

# Process files
$totalChanges = 0
foreach ($file in $Files) {
    if (Test-Path $file) {
        $changeCount = Convert-File -FilePath $file -DryRun:$DryRun
        $totalChanges += $changeCount
    }
    else {
        Write-Warn "File not found: $file"
    }
}

# Summary
Write-Info "`n======================================================"
Write-Info "                Conversion Summary"
Write-Info "======================================================"
Write-Success "Files processed: $($Files.Count)"
Write-Success "Total conversions: $totalChanges"

if ($DryRun) {
    Write-Warn "`n[DRY RUN MODE] No files were modified"
    Write-Info "Run without -DryRun to apply changes"
}
else {
    Write-Success "`n[OK] Conversion complete!"
    Write-Info "Backup location: $BackupFolder"
    Write-Info "`nNext steps:"
    Write-Info "  1. Review the changes above"
    Write-Info "  2. Compile the project: .\compile-and-upload.ps1 -SkipUpload"
    Write-Info "  3. Check for compilation errors"
    Write-Info "  4. If issues occur, restore from: $BackupFolder"
}
