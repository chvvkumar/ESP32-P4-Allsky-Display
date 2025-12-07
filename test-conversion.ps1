Write-Host "Testing Serial conversion..." -ForegroundColor Cyan
$testLine = 'Serial.println("[WiFi] Connection successful!");'
Write-Host "Before: $testLine" -ForegroundColor Red
$converted = $testLine -replace 'Serial\.println\(', 'LOG_INFO('
Write-Host "After:  $converted" -ForegroundColor Green
