# Table Manager Project

這個專案提供工具來管理黑盒表（BBox Table）的匯入和匯出操作，支援透過JSON格式更新C語言的TABLE表，並通過Modbus與DSP進行數據通訊。

## 專案結構

```
table_manager
├── src
│   ├── main.py              # 主要FSM介面，用於管理BBox Table的匯入匯出
│   ├── fsm.py               # 有限狀態機實現
│   ├── table_manager.py     # 表管理邏輯
│   ├── ModbusMaster.py      # Modbus Master實現，支持讀寫寄存器
│   ├── ReadMe.md            # ModbusMaster說明
│   ├── README_ModbusMaster.md # ModbusMaster詳細文檔
│   └── __init__.py
├── BBoxManager.py           # 菜單驅動介面，通過Modbus批量匯出/匯入黑盒數據
├── ExportBBox.py            # 從DSP通過Modbus匯出黑盒數據到JSON
├── ImportBBox.py            # 從JSON通過Modbus匯入黑盒數據到DSP
├── configs/                 # 設定檔案目錄
│   └── bbox_config.py       # Modbus地址和命令常量
├── out/                     # 輸出JSON檔案目錄（匯出結果）
├── ps1/                     # PowerShell腳本和環境設置
├── tests/                   # 單元測試
├── requirements.txt         # Python依賴
└── README.md
```

## 主要功能

### 1. BBox Table 管理 (src/main.py)

提供互動式有限狀態機介面來管理 `f021Bbox.c` 中的BBox Table：

- **匯入外部JSON**：從外部JSON檔案匯入並更新C文件的TABLE表
- **匯出到外部JSON**：從C文件分析變數並匯出為JSON格式
- **執行ModbusMaster**：執行Modbus通訊操作

變數大小分析邏輯：
- 包含 `u32`、`s32`、`f32` 的變數：4 BYTE
- 包含 `u16`、`s16` 的變數：2 BYTE
- 其他變數：提示用戶輸入長度（支援使用上次輸入值）

### 2. 黑盒數據管理 (BBoxManager.py)

菜單驅動介面，用於通過Modbus從DSP匯出和匯入黑盒數據：

- **匯出黑盒數據**：從DSP讀取校準參數和系統變數
- **匯入黑盒數據**：將JSON數據寫入DSP黑盒存儲

### 3. Modbus 通訊 (ModbusMaster.py)

使用pymodbus庫實現的Modbus Master，支持：
- 讀保持寄存器 (0x03)
- 寫單個寄存器 (0x06)
- 寫多個寄存器 (0x10)

### 4. 數據匯出/匯入 (ExportBBox.py / ImportBBox.py)

基於有限狀態機的數據傳輸：
- **匯出**：從DSP讀取所有變數，計算CRC32校驗和，保存為JSON
- **匯入**：驗證CRC32，逐個寫入變數到DSP

## 安裝

複製專案並安裝依賴：

```bash
git clone <repository-url>
cd table_manager
pip install -r requirements.txt
```

主要依賴：
- pymodbus：Modbus通訊
- Flask：Web介面（如果使用）
- pytest：測試框架

## 使用方法

### BBox Table 管理

```bash
python src/main.py
```

### 黑盒數據管理

```bash
python BBoxManager.py
```

### 直接匯出數據

```bash
python ExportBBox.py --port COM4 --baudrate 115200 --slave 0x03 --output exported_data.json
```

### 直接匯入數據

```bash
python ImportBBox.py --input data.json --port COM4 --baudrate 115200 --slave 0x03
```

### Modbus 測試

```bash
python src/ModbusMaster.py --port COM1 --slave 1 --function 3 --address 0 --quantity 10
```

## 設定

- `configs/bbox_config.py`：包含Modbus地址和命令定義
- `PRB6K_BUCK_BBOX.json`：變數定義和大小映射
- `f021Bbox.c`：目標C文件，包含sBBoxTable定義

## 測試

執行單元測試：

```bash
pytest
```

測試包括：
- ModbusMaster功能測試
- FSM邏輯測試

## 環境設置

`ps1/` 目錄包含PowerShell腳本用於：
- 安裝VS Code擴展
- 同步環境設定
- 測試環境配置

## 貢獻

歡迎貢獻！請開啟issue或提交pull request來改進或修復bug。

## 授權

本專案採用MIT授權。詳見LICENSE檔案。