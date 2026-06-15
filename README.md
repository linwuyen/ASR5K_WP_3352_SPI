# ASR5K SPI Master/Slave 自動化自檢測試框架 (WP_3352_SPI)

本專案在單一 **TMS320F28388D (C28x)** MCU 上,同時模擬 SPIA Master (模擬端) 與 SPIB Slave (實體端),建立一套非阻塞 (non-blocking) 的自動化自檢測試框架 (Self-Test Framework),用以驗證 SPIA↔SPIB 之間的協定、DMA 傳輸與**波形下載管線 (Wave Download Pipeline)**。

> 「Emu」(emulator) 名稱由來:SPIA 當 Master、SPIB 當 Slave,在同一顆 F28388D 上直接對接,屬上板前的整合驗證手段。

---

## 壹、 系統架構與訊號流 (Architecture & Signal Flow)

本框架遵循以下訊號流設計:
`SCI (UART) 接收` -> `字元過濾與命令匹配` -> `啟動自檢狀態機` -> `表驅動逐案例調度` -> `全雙工 SPI 傳輸` -> `DMA 與協定狀態機驗證` -> `輸出測試報告`。

`main.c` 主迴圈以輪詢方式非阻塞調用四個任務:
```c
runSPIBslave();          // SPIB Slave:DMA 收包 + 協定解析
runSPIAmaster();         // SPIA Master:交易佇列 + 區塊/波形傳輸(首次自動初始化)
Asr5kSpiSelfTest_Run();  // 自檢狀態機(表驅動)
runDebug();              // (_FLASH) SCIA Modbus/debug 埠
```

### 1. 核心檔案說明
* **[asr5k_spi_selftest.c](Emu_3352_SPI/asr5k_spi_selftest.c)** / **[asr5k_spi_selftest.h](Emu_3352_SPI/asr5k_spi_selftest.h)**:表驅動自檢引擎。每個測試 = 一張 `ST_SELFTEST_STEP` 步驟表 + 一個最終 validator。自檢狀態機為 `IDLE → START → WAIT_RUNNING → WAIT_DONE → STEP_DONE`。
* **[asr5k_spi_selftest_port.h](Emu_3352_SPI/asr5k_spi_selftest_port.h)**:Port 層。所有對 Master/Slave/WaveDownload 的相依都集中於此(`SelfTestPort_*`),引擎本身與驅動解耦;未來拆分只需改 port。
* **[SPIA_Master/SPI_master.c](Emu_3352_SPI/SPIA_Master/SPI_master.c)**:Master 驅動 — 交易佇列、單筆/區塊/波形傳輸、Driver/Protocol/Service 三層診斷。
* **[SPIB_Slave/spi_b_slave.c](Emu_3352_SPI/SPIB_Slave/spi_b_slave.c)**:Slave 驅動 — DMA (CH3 RX / CH4 TX) + ping-pong 緩衝、RegFrame 與波形 burst 雙模式接收。
* **[SPIB_Slave/wave_download.c](Emu_3352_SPI/SPIB_Slave/wave_download.c)** / **[wave_download.h](Emu_3352_SPI/SPIB_Slave/wave_download.h)**:波形下載服務 — D10 規格分頁狀態機 (`EMPTY→DOWNLOADING→COMPLETE→VALIDATING→VALID/INVALID→LOCKED`),256 頁、每頁 4096 樣本。
* **[main.c](Emu_3352_SPI/main.c)**:主迴圈非阻塞調度上述任務。
* **[mb_slave/ModbusSlave.c](Emu_3352_SPI/mb_slave/ModbusSlave.c)**:於 RX FIFO 接收流程整合 `Asr5kSpiSelfTest_UartRxByte()` 過濾機制,攔截 `spi_test all` 命令以啟動自檢。
* **[debug_app/cdebug.c](Emu_3352_SPI/debug_app/cdebug.c)**:在 Modbus 非繁忙時調用 `Asr5kSpiSelfTest_UartRun()`,將測試狀態與最終結果非同步輸出至 UART。

---

## 貳、 測試案例設計 (Test Cases)

自檢框架定義 **9 個測試案例**。Test1~4 為基礎讀寫/控制(legacy),Test5~9 為 **Phase 2 波形下載管線**。測試以 page 1 為操作頁。

| ID | 測試項目 | 傳輸類型 | 預期值 / 比對行為 | 驗證指標 |
| :--- | :--- | :--- | :--- | :--- |
| **1** | 暫存器寫入測試 | 32-bit 單次 | detail = `0x04471234` | 驗證 Master Detail 回拼 |
| **2** | 暫存器讀取測試 | 32-bit 單次 | detail = `0x051A8A90` | 驗證 Master Detail 回拼 |
| **3** | 輸出控制測試 (relay set/clear) | 32-bit ×2 | `OUTPUT_ON` 置位後復歸 | 每步驟驗證輸出狀態 |
| **4** | 16 字區塊連續寫入 | Block 寫入 | 寫入 16 words,checksum 比對 | Block index/progress/status |
| **5** | 波形分頁選擇 | 寫 `0x0958` | selected page=1、metadata 歸零、狀態進入 `DOWNLOADING` | Page select metadata |
| **6** | 波形樣本部分寫入 (4 筆) | 窗口 `0x3000~` 寫入 | sample_count=4、last_addr 正確、逐筆讀回比對 | Window 解析 page/index |
| **7** | 不完整頁狀態防護 | 讀 `0x095A` | 4 筆 partial page 保持 `DOWNLOADING`，不得出現 `RX_DONE` | Incomplete status guard |
| **8** | Validate 把關 (**負向測試**) | 寫 `0x0960` | 僅 4/4096 樣本 → validator 必須拒絕為 `INVALID`,且 `OUTPUT_ON` 維持 OFF | Validator gatekeeping |
| **9** | 完整 4096 下載管線 | 連續 burst + status poll | select → 4096 download → status(`REG_READY \| RX_DONE`) → validate(`VALID`) → activate(`LOCKED`) | 長傳輸零 DMA 遺失、全樣本比對、DMA delta minimum `>=4107`、最終 `LOCKED` |

> Test8 之所以為負向測試:Test6 只寫入 4 筆樣本即停,Test8 故意對這個不完整的頁下 validate,確認 validator 會以「樣本數不足」拒絕,並且**驗證流程絕不會啟動功率級 (Output 維持 OFF)**。

---

## 參、 共用驗證不變式 (Counter Invariants)

每個測試在啟動前先取 Slave 計數器 baseline,結束時計算 delta,套用同一組不變式 (`validateCounters()`),再交給 test-specific validator。任一不符即判 FAIL:

* `delta.dma_done` 等於(或對長傳輸 ≥)該測試的預期 frame 數
* `delta.parse_ok == delta.dma_done` — 每個 frame 都成功解析
* `delta.parse_fail == 0`
* `delta.dma_restart >= delta.dma_done` — 每個 frame 後 DMA 都重新武裝
* `error_flags == 0` 且 Master/Slave 無 latched fault

Test9 額外以 `u32WriteCount >= 4096` 與全 4096 筆 ramp 樣本掃描,二次證明 DMA CH3 在長傳輸中零遺失。

---

## 肆、 執行與控制介面 (Execution & Control)

### 1. 經由 UART (SCI) 指令觸發
1. 連線至開發板偵錯串口 (Baud Rate 與 Modbus 配置一致)。
2. 輸入指令 `spi_test all` 並送出 `\r\n`。
3. **終端機輸出預期:**
   * 啟動時:`SPI_TEST RUN`
   * 測試進行中重複輸入:`SPI_TEST BUSY`
   * 命令格式錯誤:`SPI_TEST ERROR`
   * 測試通過:`PASS failed_test_id=0 failed_step=0000 fault_code=0000`
   * 測試失敗:`FAIL failed_test_id=<id> failed_step=<step> fault_code=<fault>`(皆為 16 進位)

### 2. 經由 CCS 偵錯變數觸發
1. 在 CCS **Expressions** 視窗加入全域變數 `g_asr5kSpiSelfTest`。
2. 將 `g_asr5kSpiSelfTest.start` 設為 `1`。
3. 觀察 `status` 由 `ASR5K_SPI_SELFTEST_RUNNING` (1) 變為 `PASS` (2) 或 `FAIL` (3);`completed_test_count` 反映已通過案例數。
4. 失敗時透過 `failed_test_id`、`failed_step`、`fault_code` 快速定位;每案例的細節保存在 `test[id-1]`(含 `expected/actual`、counter `baseline/delta`、`spiA_fault/spiB_fault`)。

---

## 伍、 失敗診斷編碼 (Fault Decoding)

* **`failed_step`**:`ASR5K_SPI_FAIL_STEP_e`(定義於 [asr5k_spi_selftest.h](Emu_3352_SPI/asr5k_spi_selftest.h))。採「只追加不修改」原則以維持主機端解碼相容。
* **`fault_code`**:
  * `0x30xx` — 自檢引擎層級故障(如 `0x3002` wait timeout、`0x3015` Test8 validator 把關失敗、`0x3016` Test9 最終狀態錯誤)。
  * `0x1xyy` — Master 硬體診斷:`x` = fault source、`yy` = fault code。
  * `0x2xyy` — Slave 硬體診斷 / RX error flags。

---

## 陸、 記憶體配置 (Memory Placement)

記憶體區段由 **SysConfig** 驅動([main.syscfg](Emu_3352_SPI/main.syscfg)),於建置時產生 `device_cmd.cmd`(位於 git-ignored 的 `CPU1_FLASH/` 與 `CPU1_RAM/` 輸出目錄,不納入版控)。重要區段對應:

| Section | 記憶體 | 用途 |
| :--- | :--- | :--- |
| `spib_block_ram` | RAMGS2 | 區塊接收 RAM |
| `spib_slave_state` | RAMGS3 | Slave 狀態 / 診斷 / `g_waveDownload` |
| `asr5k_spi_selftest_state` / `_config` | RAMGS3 | 自檢狀態機與步驟表 |
| `spib_rx_wave_raw_1` / `_2` | RAMGS10 / RAMGS11 | 波形 burst 原始 DMA 緩衝 |
| `fake_sdram_page0/1/2` | RAMGS4 / RAMGS5 / RAMGS6 | 沙箱假 SDRAM(僅前 3 頁有 backing) |

> 沙箱階段以「假 SDRAM (3 頁)」做 write→storage→readback 的儲存一致性 checksum 驗證;正式板才接 EMIF1 SDRAM(由 `ASR5K_HAS_EMIF1_SDRAM` 切換)。
