# ASR5K 韌體系統架構深度規格書 (Firmware Architecture Deep Spec)

本文件基於 ASR5K_F28384D 專案的底層配置檔（`.cmd`, `pinmux.syscfg`, `timetask.c` 等）進行深度反向工程，記錄系統的實體資源分配、排程器細節與 IPC 握手機制。這不僅是高階架構，更是除錯與底層開發的「實體地圖」。

---

## 1. 雙核心實體資源與職責 (Physical Resources & Responsibilities)

系統採用非對稱多處理 (AMP) 架構，硬體周邊被嚴格隔離以確保不發生競爭危害。

### 1.1 CPU1 (系統總管 - 實體位址 `0x000000` 啟動)
* **硬體周邊分配 (實體腳位綁定)**：
  * **指示燈**：`STAT_CPU1` (GPIO94), `STAT_CM` (GPIO93)。
  * **通訊網路**：SPIA_EXTFLASH (GPIO16~19), SPIB_SYSTEM (GPIO60,61,65,66), SCIA (Boot Debug, GPIO135/136), FSI (TX/RX), McBSPA。
  * **控制輸出**：ePWM1 (MEAS_CNV), ePWM2 (CVAD_CNV), ePWM5 (PWM5_OFFPWM)。
  * **記憶體擴充**：EMIF1 (綁定 GPIO20~25 等控制線，以及 GPIO55~79 資料線)。
* **軟體職責**：
  * **初始化霸權**：CPU1 擁有 `Device_init()` 的絕對權限，並透過 `Device_bootCPU2()` 喚醒核心 2。
  * **記憶體授權**：透過 `MemCfg_setGSRAMMasterSel` 將 GSRAM 5~11, 15 的讀寫權限下放給 CPU2。

### 1.2 CPU2 (演算法從集核心)
* **硬體周邊分配**：
  * **指示燈**：僅擁有 `STAT_CPU2` (GPIO92) 的控制權 (`GPIO_CORE_CPU2`)。
  * **定時器**：擁有獨立的 `CPUTIMER1_BASE` 用於自身排程。
* **軟體職責**：
  * 當前僅掛載極簡的基礎建設，為未來高頻控制演算法（如電流環、電壓環）保留了極大的算力餘裕。

---

## 2. 記憶體映射與 IPC 機制 (Memory Map & IPC Mechanism)

記憶體管理是雙核穩定運行的基石。我們採用了基於區塊隔離的設計。

### 2.1 實體記憶體對接地圖 (.cmd 解析)
雙核之間的 Shared RAM 被明確切分為「單向寫入區塊」，嚴格避免了寫入衝突：
* **CPU1 寫入 / CPU2 讀取 (2K Words)**：
  * 對應 `.cmd` 區段：`RW_CPU1` (CPU1) -> `RD_CPU1` (CPU2)。
  * 實體位址：`RAMGS12_13` (起始位址 `0x019000`，長度 `0x2000`)。
  * 資料結構：CPU1 端宣告 `sAccessCPU1`，CPU2 端宣告 `sReadCPU1`。
* **CPU2 寫入 / CPU1 讀取 (2K Words)**：
  * 對應 `.cmd` 區段：`RW_CPU2` (CPU2) -> `RD_CPU2` (CPU1)。
  * 實體位址：`RAMGS10_11` (起始位址 `0x017000`，長度 `0x2000`)。
  * 資料結構：CPU2 端宣告 `sAccessCPU2`，CPU1 端宣告 `sReadCPU2`。

### 2.2 IPC 硬體旗標與同步 (IPC Sync)
* **開機握手**：系統使用 `IPC_FLAG31` 進行硬體級別的開機同步。CPU1 與 CPU2 皆會呼叫 `IPC_sync()` 阻塞，確保雙方都完成 `Board_init()` 後才一同進入主迴圈。
* **Msg RAM 通訊**：保留了硬體級的 Message RAM (`0x03A000` CPU1TOCPU2RAM, `0x03B000` CPU2TOCPU1RAM) 供未來快速短訊互動。

---

## 3. 任務排程與時間切片 (Task Scheduling & Time Slicing)

系統捨棄了 RTOS 的 Context Switch 開銷，採用極輕量的「背景軟定時器 (Soft-Timer) 陣列」架構 (`pollTimeTask`)。

### 3.1 頻率切片矩陣 (Frequency Matrix)
排程器位於 `timetask.c`，透過陣列 `time_task[]` 定義任務頻率：
* **CPU1 排程器配置**：
  * `task2D5msec` (**2.5ms**)：執行 `scanWarning()` 掃描系統警告狀態。
  * `task25msec` (**25ms**)：執行狀態機輪詢 (`pollSlowError` <-> `pollUpdateParamCLA`)、參數重算 (`recalParameters`)、以及心跳同步 (`sDrv.u32HeartBeat`)。
  * `asapTask` (**連續背景執行**)：執行無時間約束之任務，如 `runManualFlashApi()`, `runFlashStorage()`, `runDebug()`。
* **CPU2 排程器配置**：
  * 除了上述任務外，額外保留了 `task1msec` (**1ms**) 的掛載點，供未來的快速控制邏輯使用。

### 3.2 效能剖析 (Performance Profiling)
* **精準測時**：主迴圈外圍包覆了 `startTimerMeasure(&sDrv.tpMainCost)` 與 `stopTimerMeasure()`，這能利用硬體定時器精準算出主迴圈跑完一趟的 CPU Cycle。
* **延遲分析**：這項數據對於評估「是否能在背景處理 Modbus 封包而不影響 1ms 任務」具備決定性價值。

---

## 4. 模組化硬體驗證庫 (Hardware Verification Library)

為了在量產前保證硬體完好，系統整合了 `HwVerification` 模組，並將結果與 Modbus 完全解耦對接：
* **執行點**：於 CPU1 主迴圈最高頻率層持續執行。
* **驗證項目解析**：
  * **ADC/DAC 環回**：以 `DAC_setShadowValue` 輸出 12-bit Raw Data 至 DACA_BASE，隨即透過 `ADC_forceSOC` 強制採樣 ADCA_SOC0，驗證 0~3V AFE 鏈路。
  * **SPI Flash**：藉由拉低 GPIO19 (CS)，傳送 `0x9F` 命令，讀回三個位元組拼湊出 JEDEC ID (`0xEF4017`)，確保 SPIA 鏈路。
  * **SDRAM**：直接對位址 `0x80000000` 進行 Walking 1s (0x01, 0x02, 0x04...) 寫入與讀回，確保 16/32-bit 資料線 (GPIO55~79) 無短路。

---

## 5. C28x 核心硬體功能模組運用 (C28x Hardware Modules Usage)

專案高度依賴 TI C28x 專屬硬體加速器與控制周邊，以下為目前已啟用並配置之模組總覽（基於 `pinmux.syscfg` 與 `.cmd` 分析）：

### 5.1 類比與控制模組 (Analog & Control)
* **ADC (A/B/C/D)**：啟用 4 組獨立類比數位轉換器，配置 12-bit 解析度。目前 ADC A (`ADCIN0`) 已對接至硬體驗證模組進行採樣測試。
* **DAC (A/B/C)**：啟用 3 組數位類比轉換器，參考電壓設定為內部 3.0V (`DAC_REF_ADC_VREFHI`)。DAC A 目前負責輸出設定電壓以驗證 AFE (類比前端) 鏈路。
* **ePWM (1/2/5)**：增強型脈寬調變器，配置為自訂模式。ePWM1 分配給量測轉換 (`MEAS_CNV`)，ePWM2 分配給 `CVAD_CNV`，ePWM5 負責 `PWM5_OFFPWM`。
* **eCAP1**：增強型捕獲模組，配置為交流電壓異常偵測 (`ECAP1_ACFAIL`)，綁定至 `GPIO8`。

### 5.2 記憶體與通訊模組 (Memory & Communication)
* **EMIF1**：外部記憶體介面，配置為非同步 16/32-bit 資料匯流排，綁定實體位址 `0x80000000` 用以驅動 SDRAM。
* **SPI (A/B/C)**：
  * **SPIA**：作為控制器讀寫外部 SPI Flash (`10Mbps`)，目前用於週期性驗證 JEDEC ID。
  * **SPIB**：系統內部高速通訊匯流排。
  * **SPIC**：量測與 DDS (直接數位合成) 控制介面。
* **FSI (Fast Serial Interface)**：配置為 Daisy Chain (菊花鏈) 模式，用於系統間高速串列數據鏈路 (`FSITXA` / `FSIRXA`)，具備硬體級的封包檢錯與隔離能力。
* **I2CA / SCIA / McBSPA**：分別負責基礎感測器通訊、Boot Loader 與除錯介面、以及音頻/特定訊號介面。

### 5.3 協同處理器與加速模組 (Co-processors & Accelerators)
* **CLA (Control Law Accelerator)**：
  * **硬體定位**：獨立於主 C28x 核心的浮點運算加速器，能與 CPU 平行處理任務。
  * **記憶體配置**：擁有專屬的 `RAMLS45` 作為程式記憶體 (`Cla1Prog`)，`RAMLS67` 作為資料記憶體 (`CLAscratch`, `cla_shared`)。
  * **現行任務**：透過 `timetask.c` 中的 `pollUpdateParamCLA` 進行參數同步與狀態更新，未來將接手高頻核心演算法（如電流環、電壓環），以徹底解放 CPU 負擔。
* **Flash API (FMC)**：透過 `#pragma CODE_SECTION(..., ".TI.ramfunc")` 將 Flash 寫入演算法動態載入至 `RAMLS01` / `RAMGS14` 執行。這保證了在進行參數儲存 (`runFlashStorage`) 時，程式碼是從 RAM 中執行，避免發生取步錯誤 (Fetch Error)。

*文件建立日期：2026-05-05*
*維護者：Antigravity AI*
