# Simple Environment Test Script
# Usage: .\test_environment.ps1

Write-Host "=== Environment Test Started ===" -ForegroundColor Cyan
Write-Host ""

# Test PowerShell
Write-Host "1. Testing PowerShell..." -ForegroundColor Yellow
$psVersion = $PSVersionTable.PSVersion
Write-Host "   OK PowerShell version: $($psVersion.Major).$($psVersion.Minor).$($psVersion.Build)" -ForegroundColor Green

# Test OS
Write-Host "2. Testing Operating System..." -ForegroundColor Yellow
$os = Get-WmiObject -Class Win32_OperatingSystem
Write-Host "   OK OS: $($os.Caption) $($os.Version) ($($os.OSArchitecture))" -ForegroundColor Green

# Test Python
Write-Host "3. Testing Python..." -ForegroundColor Yellow
try {
    $pythonVersion = python --version 2>&1
    Write-Host "   OK Python: $pythonVersion" -ForegroundColor Green
} catch {
    Write-Host "   FAIL Python test failed" -ForegroundColor Red
}

# Test VS Code
Write-Host "4. Testing VS Code..." -ForegroundColor Yellow
try {
    $vscodeVersion = code --version 2>&1 | Select-Object -First 1
    Write-Host "   OK VS Code: $vscodeVersion" -ForegroundColor Green
} catch {
    Write-Host "   FAIL VS Code test failed" -ForegroundColor Red
}

# Test Git
Write-Host "5. Testing Git..." -ForegroundColor Yellow
try {
    $gitVersion = git --version 2>&1
    Write-Host "   OK Git: $gitVersion" -ForegroundColor Green
} catch {
    Write-Host "   FAIL Git test failed" -ForegroundColor Red
}

Write-Host ""
Write-Host "=== Environment Test Completed ===" -ForegroundColor Cyan