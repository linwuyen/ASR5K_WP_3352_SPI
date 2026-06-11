# Install VS Code extensions PowerShell script
# Usage: .\install_vscode_extensions.ps1

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

Write-Host "Starting to install VS Code extensions..." -ForegroundColor Green

foreach ($ext in $extensions) {
    Write-Host "Installing extension: $ext" -ForegroundColor Yellow
    try {
        $result = code --install-extension $ext 2>&1
        if ($LASTEXITCODE -eq 0) {
            Write-Host "✓ $ext installed successfully" -ForegroundColor Green
        } else {
            Write-Host "✗ $ext installation failed: $result" -ForegroundColor Red
        }
    } catch {
        Write-Host "✗ $ext installation error: $($_.Exception.Message)" -ForegroundColor Red
    }
}

Write-Host "Extensions installation completed!" -ForegroundColor Green