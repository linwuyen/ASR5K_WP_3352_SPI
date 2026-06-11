# 環境同步指南 - 讓兩台電腦的 VS Code + Python + PowerShell 環境完全一致

## 快速同步 (推薦)

如果您有權限執行 PowerShell 腳本，可以使用自動同步腳本：

```powershell
# 切換到腳本目錄
cd "path\to\flashapi\table_manager\ps1"

# 測試模式 (檢查環境)
.\sync_environment.ps1 -TestOnly

# 完整安裝模式
.\sync_environment.ps1
```

腳本會自動處理所有環境設定。如果遇到權限問題，請以管理員身份運行 PowerShell。

## 當前環境資訊 (參考電腦)

### 系統資訊
- **作業系統**: Microsoft Windows 11 Pro 10.0.26100 (64 位元)
- **PowerShell 版本**: 5.1.26100
- **VS Code 版本**: 1.105.1
- **Python 版本**: 3.11.5
- **Git 版本**: 2.51.2.windows.1

### Python 環境
- **Python 發行版**: Anaconda 3
- **安裝路徑**: C:\ProgramData\anaconda3
- **Conda 環境**:
  - base: C:\ProgramData\anaconda3
  - stock_env_python_3v9: C:\Users\Cody\.conda\envs\stock_env_python_3v9

### VS Code 擴充功能
```
076923.python-image-preview
ajshort.include-autocomplete
alefragnani.bookmarks
efanzh.graphviz-preview
github.copilot
github.copilot-chat
jeff-hykin.better-cpp-syntax
mechatroner.rainbow-csv
ms-ceintl.vscode-language-pack-zh-hant
ms-python.debugpy
ms-python.isort
ms-python.python
ms-python.vscode-pylance
ms-python.vscode-python-envs
ms-toolsai.jupyter
ms-toolsai.jupyter-keymap
ms-toolsai.jupyter-renderers
ms-toolsai.vscode-jupyter-cell-tags
ms-toolsai.vscode-jupyter-slideshow
ms-vscode-remote.remote-ssh
ms-vscode-remote.remote-ssh-edit
ms-vscode-remote.remote-wsl
ms-vscode.cmake-tools
ms-vscode.cpptools
ms-vscode.cpptools-extension-pack
ms-vscode.cpptools-themes
ms-vscode.notepadplusplus-keybindings
ms-vscode.remote-explorer
twxs.cmake
yzhang.markdown-all-in-one
```

### VS Code 設定
**使用者設定** (`%APPDATA%\Code\User\settings.json`):
```json
{
    "github.copilot.nextEditSuggestions.enabled": true,
    "git.autofetch": true,
    "editor.minimap.enabled": false,
    "python.defaultInterpreterPath": "c:\\ProgramData\\anaconda3"
}
```

## 同步步驟

### 步驟 1: 安裝相同的作業系統和版本
- 確保兩台電腦都是 Windows 11 專業版
- 保持系統更新到相同版本

### 步驟 2: 安裝 Python 環境 (Anaconda)
```powershell
# 下載並安裝 Anaconda (與參考電腦相同版本)
# 從 https://www.anaconda.com/products/distribution 下載
# 安裝到 C:\ProgramData\anaconda3 (需要管理員權限)

# 安裝後，初始化 conda
& "C:\ProgramData\anaconda3\Scripts\conda.exe" init powershell

# 創建相同的環境
& "C:\ProgramData\anaconda3\Scripts\conda.exe" create -n stock_env_python_3v9 python=3.9
```

### 步驟 3: 安裝 VS Code
```powershell
# 下載並安裝 VS Code 1.105.1 版本
# 從 https://code.visualstudio.com/download 下載
winget install Microsoft.VisualStudioCode
```

### 步驟 4: 安裝 VS Code 擴充功能
創建一個 PowerShell 腳本 `install_extensions.ps1`:

```powershell
$extensions = @(
    "076923.python-image-preview",
    "ajshort.include-autocomplete",
    "alefragnani.bookmarks",
    "efanzh.graphviz-preview",
    "github.copilot",
    "github.copilot-chat",
    "jeff-hykin.better-cpp-syntax",
    "mechatroner.rainbow-csv",
    "ms-ceintl.vscode-language-pack-zh-hant",
    "ms-python.debugpy",
    "ms-python.isort",
    "ms-python.python",
    "ms-python.vscode-pylance",
    "ms-python.vscode-python-envs",
    "ms-toolsai.jupyter",
    "ms-toolsai.jupyter-keymap",
    "ms-toolsai.jupyter-renderers",
    "ms-toolsai.vscode-jupyter-cell-tags",
    "ms-toolsai.vscode-jupyter-slideshow",
    "ms-vscode-remote.remote-ssh",
    "ms-vscode-remote.remote-ssh-edit",
    "ms-vscode-remote.remote-wsl",
    "ms-vscode.cmake-tools",
    "ms-vscode.cpptools",
    "ms-vscode.cpptools-extension-pack",
    "ms-vscode.cpptools-themes",
    "ms-vscode.notepadplusplus-keybindings",
    "ms-vscode.remote-explorer",
    "twxs.cmake",
    "yzhang.markdown-all-in-one"
)

foreach ($ext in $extensions) {
    code --install-extension $ext
}
```

執行腳本：
```powershell
.\install_extensions.ps1
```

### 步驟 5: 設定 VS Code
```powershell
# 複製設定文件
Copy-Item "$env:APPDATA\Code\User\settings.json" "$env:APPDATA\Code\User\settings_backup.json"

# 創建新的設定文件
@"
{
    `"github.copilot.nextEditSuggestions.enabled`": true,
    `"git.autofetch`": true,
    `"editor.minimap.enabled`": false,
    `"python.defaultInterpreterPath`": `"c:\\\\ProgramData\\\\anaconda3`"
}
"@ | Out-File -FilePath "$env:APPDATA\Code\User\settings.json" -Encoding UTF8
```

### 步驟 6: 設定 PowerShell 執行政策
```powershell
# 設定執行政策
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser

# 驗證設定
Get-ExecutionPolicy
```

### 步驟 7: 設定系統環境變數
```powershell
# 確保 Python 在 PATH 中
$pythonPath = "C:\ProgramData\anaconda3;C:\ProgramData\anaconda3\Scripts;C:\ProgramData\anaconda3\Library\bin"
$currentPath = [Environment]::GetEnvironmentVariable("Path", "User")
if ($currentPath -notlike "*$pythonPath*") {
    [Environment]::SetEnvironmentVariable("Path", "$currentPath;$pythonPath", "User")
}

# 確保 Git 在 PATH 中
$gitPath = "C:\Program Files\Git\bin"
$currentPath = [Environment]::GetEnvironmentVariable("Path", "User")
if ($currentPath -notlike "*$gitPath*") {
    [Environment]::SetEnvironmentVariable("Path", "$currentPath;$gitPath", "User")
}

# 重新載入環境變數
$env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path","User")
```

### 步驟 8: 安裝 Python 套件
```powershell
# 激活 base 環境並安裝常用套件
& "C:\ProgramData\anaconda3\Scripts\activate.bat"
pip install pyserial requests numpy pandas matplotlib jupyter

# 安裝到特定環境
& "C:\ProgramData\anaconda3\Scripts\activate.bat" stock_env_python_3v9
pip install pyserial requests numpy pandas matplotlib jupyter
```

### 步驟 9: 測試環境
創建測試腳本 `test_environment.ps1`:

```powershell
Write-Host "=== 環境測試 ==="

# 測試 PowerShell
Write-Host "PowerShell 版本: $($PSVersionTable.PSVersion)"

# 測試 Python
try {
    $pythonVersion = python --version 2>&1
    Write-Host "Python 版本: $pythonVersion"
} catch {
    Write-Host "Python 測試失敗" -ForegroundColor Red
}

# 測試 conda
try {
    $condaInfo = & "C:\ProgramData\anaconda3\Scripts\conda.exe" --version 2>&1
    Write-Host "Conda 版本: $condaInfo"
} catch {
    Write-Host "Conda 測試失敗" -ForegroundColor Red
}

# 測試 VS Code
try {
    $vscodeVersion = code --version 2>&1 | Select-Object -First 1
    Write-Host "VS Code 版本: $vscodeVersion"
} catch {
    Write-Host "VS Code 測試失敗" -ForegroundColor Red
}

# 測試 Git
try {
    $gitVersion = git --version 2>&1
    Write-Host "Git 版本: $gitVersion"
} catch {
    Write-Host "Git 測試失敗" -ForegroundColor Red
}

# 測試 pip
try {
    $pipVersion = pip --version 2>&1
    Write-Host "Pip 版本: $pipVersion"
} catch {
    Write-Host "Pip 測試失敗" -ForegroundColor Red
}

Write-Host "=== 測試完成 ==="
```

執行測試：
```powershell
.\test_environment.ps1
```

### 步驟 10: 設定 Git 和專案同步
```powershell
# 安裝 Git (如果尚未安裝)
winget install Git.Git

# 設定 Git
git config --global user.name "Your Name"
git config --global user.email "your.email@example.com"

# 複製專案
git clone https://github.com/yanbai7/GW_PRB.git
cd GW_PRB
git checkout PY_MBUS_MASTER
```

### 步驟 11: 驗證 GitHub Copilot 功能
1. 開啟 VS Code
2. 登入 GitHub 帳號
3. 確保 GitHub Copilot 擴充功能正常工作
4. 測試終端機整合功能

## 疑難排解

### 如果 Git 命令無法執行：
```powershell
# 檢查 Git 安裝
Get-Command git -All

# 手動添加 Git 到 PATH
$gitPath = "C:\Program Files\Git\bin"
$currentPath = [Environment]::GetEnvironmentVariable("Path", "User")
if ($currentPath -notlike "*$gitPath*") {
    [Environment]::SetEnvironmentVariable("Path", "$currentPath;$gitPath", "User")
}

# 重新載入環境變數
$env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path","User")
git --version
```

### 如果 Python 命令無法執行：
```powershell
# 檢查 Python 安裝
Get-Command python -All

# 重新設定 PATH
$env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path","User")
python --version
```

### 如果 VS Code 無法操作終端機：
1. 檢查 VS Code 設定中的終端機設定
2. 重新安裝 VS Code
3. 檢查防火牆設定

### 如果擴充功能安裝失敗：
```powershell
# 手動安裝
code --install-extension ms-python.python
code --install-extension github.copilot
```

## 最終驗證
確保以下功能正常工作：
- [ ] PowerShell 終端機可以執行命令
- [ ] Python 可以正常執行
- [ ] Git 可以正常執行
- [ ] VS Code 可以操作終端機
- [ ] GitHub Copilot 可以提供建議
- [ ] 所有擴充功能正常載入

完成這些步驟後，兩台電腦的環境應該完全一致。