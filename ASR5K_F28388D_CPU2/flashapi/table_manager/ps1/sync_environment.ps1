# Environment synchronization main script
# Usage: .\sync_environment.ps1

param(
    [switch]$SkipVSCodeExtensions,
    [switch]$SkipPythonPackages,
    [switch]$TestOnly
)

Write-Host "=== Environment Synchronization Script ===" -ForegroundColor Cyan
Write-Host "Starting to synchronize development environment to reference computer settings..." -ForegroundColor Yellow
Write-Host ""

if ($TestOnly) {
    Write-Host "Test mode: Only run tests, no installations" -ForegroundColor Yellow
}

# Check administrator privileges
$currentPrincipal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
$isAdmin = $currentPrincipal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

if (-not $isAdmin) {
    Write-Host "Warning: Some operations may require administrator privileges" -ForegroundColor Yellow
}

# Step 1: Set PowerShell execution policy
Write-Host "Step 1: Setting PowerShell execution policy..." -ForegroundColor Green
try {
    Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser -Force
    Write-Host "✓ Execution policy set to RemoteSigned" -ForegroundColor Green
}
catch {
    Write-Host "✗ Failed to set execution policy: $($_.Exception.Message)" -ForegroundColor Red
}

# Step 2: Check and set Python PATH
Write-Host "Step 2: Setting Python environment variables..." -ForegroundColor Green
$pythonPaths = @(
    "C:\ProgramData\anaconda3",
    "C:\ProgramData\anaconda3\Scripts",
    "C:\ProgramData\anaconda3\Library\bin"
)

$currentPath = [Environment]::GetEnvironmentVariable("Path", "User")
$pathUpdated = $false

foreach ($path in $pythonPaths) {
    if ($currentPath -notlike "*$path*") {
        $currentPath += ";$path"
        $pathUpdated = $true
    }
}

if ($pathUpdated) {
    [Environment]::SetEnvironmentVariable("Path", $currentPath, "User")
    Write-Host "✓ Python PATH updated, please restart terminal" -ForegroundColor Green
}
else {
    Write-Host "✓ Python PATH is already correctly set" -ForegroundColor Green
}

# Step 2.5: Check and set Git PATH
Write-Host "Step 2.5: Setting Git environment variables..." -ForegroundColor Green
$gitPath = "C:\Program Files\Git\bin"
$currentPath = [Environment]::GetEnvironmentVariable("Path", "User")
if ($currentPath -notlike "*$gitPath*") {
    $currentPath += ";$gitPath"
    [Environment]::SetEnvironmentVariable("Path", $currentPath, "User")
    Write-Host "✓ Git PATH updated, please restart terminal" -ForegroundColor Green
}
else {
    Write-Host "✓ Git PATH is already correctly set" -ForegroundColor Green
}

# Step 3: Check Conda environment
Write-Host "Step 3: Checking Conda environment..." -ForegroundColor Green
$condaPath = "C:\ProgramData\anaconda3\Scripts\conda.exe"
if (Test-Path $condaPath) {
    try {
        $envs = & $condaPath info --envs 2>&1
        if ($envs -like "*stock_env_python_3v9*") {
            Write-Host "✓ Conda environment stock_env_python_3v9 already exists" -ForegroundColor Green
        }
        else {
            if (-not $TestOnly) {
                Write-Host "Creating stock_env_python_3v9 environment..." -ForegroundColor Yellow
                & $condaPath create -n stock_env_python_3v9 python=3.9 -y
                Write-Host "✓ Conda environment created" -ForegroundColor Green
            }
            else {
                Write-Host "⚠ Test mode: Skipping Conda environment creation" -ForegroundColor Yellow
            }
        }
    }
    catch {
        Write-Host "✗ Conda environment check failed: $($_.Exception.Message)" -ForegroundColor Red
    }
}
else {
    Write-Host "✗ Conda not found: $condaPath" -ForegroundColor Red
    Write-Host "Please install Anaconda to C:\ProgramData\anaconda3 first" -ForegroundColor Yellow
}

# Step 4: Install Python packages
if (-not $SkipPythonPackages -and -not $TestOnly) {
    Write-Host "Step 4: Installing Python packages..." -ForegroundColor Green
    $packages = @("pyserial", "requests", "numpy", "pandas", "matplotlib", "jupyter")

    try {
        # Install to base environment
        Write-Host "Installing packages to base environment..." -ForegroundColor Yellow
        & pip install $packages --quiet

        # Install to stock_env environment
        Write-Host "Installing packages to stock_env_python_3v9 environment..." -ForegroundColor Yellow
        & $condaPath run -n stock_env_python_3v9 pip install $packages --quiet

        Write-Host "✓ Python packages installed" -ForegroundColor Green
    }
    catch {
        Write-Host "✗ Python packages installation failed: $($_.Exception.Message)" -ForegroundColor Red
    }
}
elseif ($SkipPythonPackages) {
    Write-Host "Step 4: Skipping Python packages installation" -ForegroundColor Yellow
}
else {
    Write-Host "Step 4: Test mode, skipping Python packages installation" -ForegroundColor Yellow
}

# Step 5: Set up VS Code
if (-not $TestOnly) {
    Write-Host "Step 5: Setting up VS Code..." -ForegroundColor Green
    try {
        & ".\setup_vscode_settings.ps1"
    }
    catch {
        Write-Host "✗ VS Code setup failed: $($_.Exception.Message)" -ForegroundColor Red
    }
}
else {
    Write-Host "Step 5: Test mode, skipping VS Code setup" -ForegroundColor Yellow
}

# Step 6: Install VS Code extensions
if (-not $SkipVSCodeExtensions -and -not $TestOnly) {
    Write-Host "Step 6: Installing VS Code extensions..." -ForegroundColor Green
    try {
        & ".\install_vscode_extensions.ps1"
    }
    catch {
        Write-Host "✗ Extensions installation failed: $($_.Exception.Message)" -ForegroundColor Red
    }
}
elseif ($SkipVSCodeExtensions) {
    Write-Host "Step 6: Skipping VS Code extensions installation" -ForegroundColor Yellow
}
else {
    Write-Host "Step 6: Test mode, skipping VS Code extensions installation" -ForegroundColor Yellow
}

# Step 7: Run environment tests
Write-Host "Step 7: Running environment tests..." -ForegroundColor Green
try {
    & ".\test_environment.ps1"
}
catch {
    Write-Host "✗ Environment tests failed: $($_.Exception.Message)" -ForegroundColor Red
}

Write-Host ""
Write-Host "=== Environment Synchronization Completed ===" -ForegroundColor Cyan
Write-Host ""
Write-Host "Recommended Actions:" -ForegroundColor Yellow
Write-Host "1. Restart VS Code" -ForegroundColor White
Write-Host "2. Restart PowerShell terminal" -ForegroundColor White
Write-Host "3. Test GitHub Copilot functionality" -ForegroundColor White
Write-Host "4. Confirm terminal operations are normal" -ForegroundColor White
Write-Host ""
Write-Host "If issues occur, refer to environment_sync_guide.md" -ForegroundColor Yellow