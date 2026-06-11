# ASR5K_F28388D_CPU1 目錄導覽

## [整個專案架構]

**目標硬體：** TI TMS320F28384D / F28388D（C2000 DSP，雙核心）
**用途：** ASR-5000 系列工業電源（逆變器/轉換器）控制板韌體 + 硬體設計

```
GW_ASR5K_F28384D/
├── Code/          ← 韌體原始碼（TI Code Composer Studio 專案）
├── Doc/           ← IC datasheet、系統架構圖（drawio/pdf）
├── SCH & PCB/     ← Altium 硬體設計（SCH + PCB）
└── README.md
```

### Code/ — 6 個子專案

| 子專案 | 說明 |
| :--- | :--- |
| `ASR5K_F28384D_CPU1/` | F28384D 晶片 CPU1 韌體 |
| `ASR5K_F28384D_CPU2/` | F28384D 晶片 CPU2 韌體 |
| `ASR5K_F28384_LINK_PROJECT/` | F28384D CCS Link Project |
| `ASR5K_F28388D_CPU1/` | **F28388D CPU1 韌體（本目錄）** |
| `ASR5K_F28388D_CPU2/` | F28388D CPU2 韌體 |
| `ASR5K_F28388D_LINK_PROJECT/` | F28388D CCS Link Project |

### 各 CPU 專案共同結構

每個 CPU 專案包含以下子目錄：

| 子目錄 | 說明 |
| :--- | :--- |
| `cla/` | CLA（Control Law Accelerator）即時運算任務 |
| `device/driverlib/` | TI DriverLib 周邊驅動（ADC/SPI/ePWM/CAN 等）含 `inc/hw_*.h` 暫存器定義 |
| `flashapi/` | Flash 操作 API（C 層）+ Python `table_manager` 工具（Modbus/BBOX 參數表管理） |
| `libraries/` | HRPWM 校正庫、Flash API 預編譯庫 |
| `mb_slave/` | Modbus RTU Slave 實作（CRC16、ModbusSlave、linkVariables 等） |
| `targetConfigs/` | CCS debug 目標設定（.ccxml） |

根目錄主要檔案：`main.c`, `isr.c`, `timetask.c`, `HwConfig.c`, `shareram.h`, `*.cmd`（linker script）

### SCH & PCB/ — 主要硬體設計

| 資料夾 | 說明 |
| :--- | :--- |
| `ASR5K_F28384D_CONTROL_BOARD_V2/` | F28384D 控制板 V2（Altium 專案） |
| `ASR5K_MainPowerBoard/` | ASR-5075 主功率板 |
| `ASR-5150_DCAC+SPWM-P-V4/` | ASR-5150 DC/AC SPWM 板 |
| `ASR5K_AM3352_SPI_TO_C2K_LVDS/` | AM3352→F28384D LVDS 介面板 |
| `TI_ControlBoard_F28388D/` | TI 官方 F28388D 控制板參考 |
| `GW_ASR-5000 Control_Board-P-V1/` | 控制板 PCB V1 |
| `TestPlacement/` | 控制板 PCB V2 測試佈局 |
| `Reference/` | 歷史版本與參考電路 |

---

## [本目錄結構預覽]

- **API 與底層驅動**
  - `API_SPI.c` / `.h`
  - `HwConfig.c` / `.h` / `.py`
  - `HwVersionHistory.h`
  - `SPI_SPEC.md`
- **核心邏輯與任務排程**
  - `main.c`
  - `timetask.c` / `.h`
  - `isr.c` / `isr_common.h`
- **DDS 模組**（F28388D CPU1 特有）
  - `dds/dds.c`, `dds_api.h`, `dds_buildTable.h`, `dds_common.h`, `dds_config.h`, `dds_core.h`, `dds_timectrl.h`
- **除錯模組**
  - `debug_app/cdebug.c` / `.h`
- **中介層與數學函式**
  - `c28param.h`
  - `c28protection.h`
  - `cTimeMeas.h`
  - `c28math.h` / `cmath.h`
- **其他周邊模組與資料結構**
  - `common.h`
  - `ctypedef.h`
  - `initHRPWM.h`
  - `shareram.h`
- **腳本與規則配置**
  - `.agents/rules/common.md` / `RULES.md`
  - `CopyProjectTo.py` / `copydest.json`
  - `update_hwconfig.bat`
  - `.gitignore` / `pinmux.syscfg`
  - `2838x_FLASH_lnk_cpu1.cmd`
- **資料夾**
  - `cla/`
  - `dds/`
  - `debug_app/`
  - `device/`
  - `flashapi/`
  - `libraries/`
  - `mb_slave/`
  - `targetConfigs/`

## [檔案簡介]

- `CopyProjectTo.py`：專案複製腳本，會清空目標路徑並排除設定等特定名稱。
- `c28param.h`：核心參數定義檔。
- `common.h`：通用定義標頭檔。
- `implementation.md`：開發設計預審提案報告。
- `analysis.md`：現有代碼或目錄分析報告。
- `task.md`：核心工作任務清單。

## [修改紀錄]

| 修改人   | 日期       | 版本   | 修改內容摘要          |
| :------- | :--------- | :----- | :-------------------- |
| AI Agent | 2026-06-03 | v1.1.0 | 新增整個專案架構總覽（Code/Doc/SCH&PCB 三層說明）。 |
| AI Agent | 2026-02-23 | v1.0.1 | 更新 `CopyProjectTo.py` 以允許複製 `.settings` 與 `.launches` (含 `..launches`) 特定設定目錄。 |
| AI Agent | 2026-02-23 | v1.0.0 | 建立初始版本 README。 |
