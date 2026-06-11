# PowerShell script to set up VS Code
# Usage: .\setup_vscode_settings.ps1

Write-Host "Setting up VS Code..." -ForegroundColor Green

# Backup existing settings
$settingsPath = "$env:APPDATA\Code\User\settings.json"
$backupPath = "$env:APPDATA\Code\User\settings_backup.json"

if (Test-Path $settingsPath) {
    Write-Host "Backing up existing settings..." -ForegroundColor Yellow
    Copy-Item $settingsPath $backupPath -Force
    Write-Host "✓ Settings backed up to: $backupPath" -ForegroundColor Green
}

# Create new settings
$settings = @"
{
    "github.copilot.nextEditSuggestions.enabled": true,
    "git.autofetch": true,
    "editor.minimap.enabled": false,
    "python.defaultInterpreterPath": "c:\\\\ProgramData\\\\anaconda3",
    "python.terminal.activateEnvironment": true,
    "terminal.integrated.shell.windows": "C:\\\\Windows\\\\System32\\\\WindowsPowerShell\\\\v1.0\\\\powershell.exe",
    "terminal.integrated.shellArgs.windows": [],
    "terminal.integrated.automationShell.windows": "C:\\\\Windows\\\\System32\\\\cmd.exe",
    "workbench.editor.enablePreview": false,
    "files.autoSave": "afterDelay",
    "files.autoSaveDelay": 1000,
    "editor.formatOnSave": true,
    "python.formatting.provider": "black",
    "python.linting.enabled": true,
    "python.linting.pylintEnabled": true,
    "jupyter.notebookFileRoot": "`${workspaceFolder}",
    "cmake.configureOnOpen": true
}
"@

Write-Host "Writing new settings..." -ForegroundColor Yellow
$settings | Out-File -FilePath $settingsPath -Encoding UTF8 -Force
Write-Host "✓ VS Code settings updated" -ForegroundColor Green

# Set up workspace settings (if in project directory)
$workspaceSettingsPath = ".vscode\settings.json"
if (-not (Test-Path ".vscode")) {
    New-Item -ItemType Directory -Path ".vscode" -Force | Out-Null
}

$workspaceSettings = @"
{
    "python.pythonPath": "c:\\\\ProgramData\\\\anaconda3\\\\python.exe",
    "python.terminal.activateEnvironment": true,
    "terminal.integrated.cwd": "`${workspaceFolder}"
}
"@

Write-Host "Setting up workspace settings..." -ForegroundColor Yellow
$workspaceSettings | Out-File -FilePath $workspaceSettingsPath -Encoding UTF8 -Force
Write-Host "✓ Workspace settings created" -ForegroundColor Green

Write-Host "VS Code setup complete! Please restart VS Code for the settings to take effect." -ForegroundColor Green