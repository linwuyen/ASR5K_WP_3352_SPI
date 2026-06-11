@echo off

REM 切換到 bat 檔案所在目錄
cd /d %~dp0

REM 執行 Python 腳本
python HwConfig.py
