# ASR5K 知識庫整合索引 (Knowledge Base Index)

本目錄存放 ASR5K 專案的核心技術文件、硬體驗證報告與開發指南。所有文件遵循 `[類別]_[名稱].md` 的命名規範。

---

## 🏗️ 系統架構與啟動 (System & Architecture)
核心系統設計與韌體啟動流程。
- **[SYS_Bringup_Guide.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/.agent/specs/knowledge/SYS_Bringup_Guide.md)**
  - ASR5K 控制卡系統啟動與硬體驗證指南。包含 SysConfig 驅動架構、FSI 菊花鏈驗證與架構約束。

---

## 🔌 硬體週邊與驅動 (Hardware & Peripherals)
各項硬體模組的詳細配置、時序分析與驗證。
- **[HW_SDRAM_Troubleshooting.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/.agent/specs/knowledge/HW_SDRAM_Troubleshooting.md)**
  - SDRAM 硬體除錯與時序優化指南。紀錄 Aliasing 問題解決方案與 Timing Audit SOP。
- **[HW_SPI_Flash_Manual.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/.agent/specs/knowledge/HW_SPI_Flash_Manual.md)**
  - W25Q64 SPI Flash 驗證報告。實現 32-bit 連續通訊與硬體 STE 控制。
- **[HW_ADC_Temp_Calculation.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/.agent/specs/knowledge/HW_ADC_Temp_Calculation.md)**
  - ADC 溫度監控技術指南。包含 DriverLib 校準函式使用與 SysConfig 採樣窗口配置。

---

## 📡 通訊協定 (Protocols)
外部介面與通訊模型驅動開發。
- **[PROTO_Modbus_QuickStart.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/.agent/specs/knowledge/PROTO_Modbus_QuickStart.md)**
  - Modbus MDD 變數新增操作指南 (SOP)。包含 Excel 與 CCS 同步流程。
- **[PROTO_Modbus_Workflow.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/.agent/specs/knowledge/PROTO_Modbus_Workflow.md)**
  - Modbus 數據鏈條架構深度解析。

---

## 🔍 研究紀錄與概念 (Research & Concepts)
量測邏輯研究與電力電子術語解析。
- **[RES_Legacy_28377_Logic.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/.agent/specs/knowledge/RES_Legacy_28377_Logic.md)**
  - 28377 舊版專案量測邏輯 (VO/IO) 研究紀錄，作為 28388 遷移參考。
- **[RES_Measurement_Concept_QA.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/.agent/specs/knowledge/RES_Measurement_Concept_QA.md)**
  - 逆變器量測概念與術語解析 (Q&A)。包含 RMS 計算、LM-NM 與 CS 術語說明。

---

## 🛠️ 工具與疑難排解 (Tools & Troubleshooting)
通用開發陷阱與除錯指引。
- **[TOOL_General_Troubleshooting.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/.agent/specs/knowledge/TOOL_General_Troubleshooting.md)**
  - C2000 已知錯誤與排雷手冊。包含 Linker 錯誤、Watchdog Reset 與 IPC 數據衝突處理。

---
*最後更新：2026-05-11*
