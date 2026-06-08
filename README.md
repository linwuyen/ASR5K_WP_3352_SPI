# ASR5K SPI Master/Slave 自動化自檢測試框架 (WP_3352_SPI)

本專案實作了 ASR5K 系統中 SPIA Master (模擬端) 與 SPIB Slave (實體端) 之間的非阻塞自動化自檢測試框架 (Self-Test Framework)。

---

## 壹、 系統架構與訊號流 (Architecture & Signal Flow)

本框架遵循以下訊號流設計：
`SCI (UART) 接收` -> `字元過濾與命令匹配` -> `啟動自檢狀態機` -> `循環調度測試案例` -> `全雙工 SPI 傳輸` -> `DMA 與協定狀態機驗證` -> `輸出測試報告`。

### 1. 核心檔案說明
* **[asr5k_spi_selftest.c](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/Emu_3352_SPI/asr5k_spi_selftest.c)** / **[asr5k_spi_selftest.h](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/Emu_3352_SPI/asr5k_spi_selftest.h)**: 測試編排、自檢狀態機（IDLE, START, WAIT_RUNNING, WAIT_DONE）及 8 個測試案例的驗證邏輯。
* **[main.c](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/Emu_3352_SPI/main.c)**: 主迴圈中非阻塞調用 `Asr5kSpiSelfTest_Run()` 以驅動測試狀態機。
* **[ModbusSlave.c](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/Emu_3352_SPI/mb_slave/ModbusSlave.c)**: 於 RX FIFO 接收流程中整合過濾機制，攔截 `spi_test all` 命令以實現自檢啟動。
* **[cdebug.c](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/Emu_3352_SPI/debug_app/cdebug.c)**: 在 Modbus 非繁忙時調用 `Asr5kSpiSelfTest_UartRun()`，將測試狀態與最終結果異步輸出至 UART。
* **[spib_block_ram.cmd](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/Emu_3352_SPI/spib_block_ram.cmd)**: 將測試狀態與配置資料配置於 `RAMGS4` 區段，避免與現有通訊緩衝區衝突。

---

## 貳、 測試案例設計 (Test Cases)

自檢框架定義了 8 個測試案例，涵蓋基本讀寫、GPIO 控制、大數據區塊壓測、正弦波/漸變波比對及協定封包解析：

| ID | 測試項目 | 傳輸類型 | 預期回傳值 / 比對行為 | 驗證指標 |
| :--- | :--- | :--- | :--- | :--- |
| **1** | 暫存器寫入測試 | 32-bit 單次 | `0x04471234` | 驗證 Master Detail |
| **2** | 暫存器讀取測試 | 32-bit 單次 | `0x051A8A90` | 驗證 Master Detail |
| **3** | 輸出控制測試 (GPIO 翻轉) | 32-bit 單次 | 控制 `OUTPUT_ON` 狀態 | 驗證輸出置位與復歸 |
| **4** | 區塊連續寫入測試 (16 字) | Block 寫入 | 寫入 16 words，校驗和比對 | 驗證 Block 傳輸結果 |
| **5** | 漸變波形區塊測試 (4095 字) | Block 寫入 | 寫入漸變波，比對首尾資料 | 驗證 Ramp 數據正確性 |
| **6** | 正弦波形區塊測試 (4095 字) | Block 寫入 | 比對寫入之正弦波與 RAM 數據 | 驗證正弦波無誤 |
| **7** | 暫存器框架壓測 (1000 筆) | 32-bit 連續 | 傳輸 1000 筆暫存器框架 | 驗證傳輸次數與 OK 計數 |
| **8** | 封包命令協議測試 | 混合寫入 | 傳輸封包控制，復歸輸出狀態 | 驗證 Packet 狀態機與計數器 |

---

## 參、 執行與控制介面 (Execution & Control)

### 1. 經由 UART (SCI) 指令觸發
1. 連線至開發板的偵錯串口 (Baud Rate 與 Modbus 配置一致)。
2. 輸入指令：`spi_test all` 並送出 `\r\n`。
3. **終端機輸出預期：**
   * 啟動時：`SPI_TEST RUN`
   * 測試進行中若重複輸入：`SPI_TEST BUSY`
   * 測試通過：`PASS failed_test_id=0 failed_step=0000 fault_code=0000`
   * 測試失敗：`FAIL failed_test_id=<id> failed_step=<step> fault_code=<fault>`

### 2. 經由 CCS 偵錯變數觸發
1. 在 CCS **Expressions** 視窗中加入全域變數 `g_asr5kSpiSelfTest`。
2. 將 `g_asr5kSpiSelfTest.start` 設為 `1`。
3. 觀察 `g_asr5kSpiSelfTest.status` 從 `ASR5K_SPI_SELFTEST_RUNNING` (1) 變更為 `ASR5K_SPI_SELFTEST_PASS` (2) 或 `ASR5K_SPI_SELFTEST_FAIL` (3)。
4. 若測試失敗，可透過 `failed_test_id`、`failed_step` 及 `fault_code` 快速定位問題點。
