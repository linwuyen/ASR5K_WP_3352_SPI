# Skills 目錄

本目錄存放供 AI Agent 呼叫的擴充工具與腳本，用於執行特定的自動化檢查或檢索任務。

**依賴套件（請先安裝）**：
```bash
python -m pip install -r .agent/skills/requirements.txt
```

---

## 工具列表

### `query_trm.py`

用於檢索 TI C2000 Technical Reference Manual (TRM) 中的暫存器定義，確保硬體配置的準確性。

**支援模組**：`ADC`, `DMA`, `EPWM`（含 `PWM`）, `IPC`, `MCBSP`, `SPI`

**使用方式**：
```bash
python query_trm.py <模組名稱> <暫存器名稱>
# 範例
python query_trm.py EPWM AQCTLA
python query_trm.py ADC ADCCTL1
python query_trm.py SPI SPICCR
```

**回傳內容**：TRM PDF 中最相關的 2 頁暫存器定義（含位元欄位說明）。

**可用性檢查**：
```bash
python .agent/skills/query_trm.py --dependency-check
```

依賴檢查失敗時，不得宣稱 TRM 查詢工具可用。

---

### `query_driverlib.py`

用於檢索 TI C2000Ware DriverLib 本地安裝的原始碼，獲取函式原型、參數型別與 Doxygen 說明，防止 API 簽名幻覺（Hallucination）。

**DriverLib 路徑**：優先使用環境變數 `C2000WARE_DRIVERLIB`，否則掃描
`C:\ti\c2000\C2000Ware_*` 並選擇最新版本。

**使用方式**：
```bash
python query_driverlib.py <模組名稱> <函式名稱/關鍵字>
# 範例
python query_driverlib.py SPI SPI_writeDataNonBlocking
python query_driverlib.py SPI SPI_setConfig
python query_driverlib.py EPWM EPWM_setTimeBasePeriod
python query_driverlib.py ADC ADC_setupSOC
```

**回傳內容**：完整函式宣告原型（含 `uint32_t`/`uint16_t` 等 stdint 參數型別）+ 前置 Doxygen `//!` 說明區塊。

---

### `pre_flight_check.py`

提供實際可執行的檢查入口：

```bash
python .agent/skills/pre_flight_check.py --check-division path/to/source.c
python .agent/skills/pre_flight_check.py --safety-scan path/to/source.c
python .agent/skills/pre_flight_check.py --all path/to/source.c
```

任何檢查違規或工具異常都以非零 exit code 結束。

### `agent_hooks.py`

Hooks 只有在真正的 Google Antigravity SDK 可匯入時才視為啟用。若 SDK
缺失、被 Python 標準函式庫同名模組遮蔽，或 hook 執行發生例外，必須
fail closed，不得降級為允許寫入或驗證通過。

---

## 🛠️ AI IDE SKILL 撰寫準則核心結構與標準模板

為防止 AI 產生幻覺（Hallucination）並確保硬體操作的安全性，本目錄下的所有 SKILL 說明文件（如 `skill.md`）必須嚴格遵守以下**四大核心區塊結構**：

1. **`## 1. When to use (觸發時機)`**：
   必須明確定義此技能的啟動情境，讓 AI 能在正確的對話上下文或任務中自律性地觸發（Trigger）此技能。
2. **`## 2. Tool Invocation (前置硬體審核)`**：
   落實「沒有證據就不寫 code」的原則。強制 AI 在生成程式碼前，必須先呼叫外部工具（如 `query_trm.py`）查詢 TI C2000 的 TRM 文件，獲取精準的暫存器定義。
3. **`## 3. How to use (執行步驟與邊界約束)`**：
   將硬體安全規範明確化。例如操作關鍵暫存器需以 `EALLOW`/`EDIS` 包覆切換，或規定「硬體暫存器操作僅限於 HAL 層，嚴禁暴露給 Control Layer」。
4. **`## 4. Response Format (預期輸出與代碼規範)`**：
   提供 Few-shot（少樣本）範例，要求 AI 輸出格式一致。絕對禁止產生 Magic Number 或使用絕對記憶體位址，並強制要求使用 TI 標準位元欄位結構體（Bit-field structs）。

（完整範例可參考 `example/skill.md`）
