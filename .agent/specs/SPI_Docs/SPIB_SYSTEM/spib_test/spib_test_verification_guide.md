# SPIB 主從通訊整合設計與實體驗證指南 (spib_test_verification_guide.md)

本文件整合了 ASR5K 系統中，主機 (Master, C2000 CPU1 模擬端) 與從機 (Slave, C2000 CPU1 實體端) 之間的**實體硬體接線**、**SysConfig 軟體配置**、**驅動程式 API 參考**、**FIFO 指針工作概念**，以及 **5 大實機測試驗證情境**。

本指南引用了位於同目錄下的示波器與 Expressions 截圖 `1.png` 到 `8.png` 作為物理證據。

---

## 壹、 硬體接線與 SysConfig 設定 (Hardware Setup & SysConfig)

### 1. 物理引腳接線關係
若要建立 Master 與 Slave 的物理連接，必須將兩板的 GPIO 腳位與大地 (GND) 進行直連，接線對照如下：

| 訊號名稱 | Master 針腳 (C2000 模擬端) | Slave 針腳 (C2000 實際端) | 阻抗電阻 | 功能說明 |
| :--- | :--- | :--- | :--- | :--- |
| **SPI CLK** | GPIO65 (CLK) | GPIO65 (CLK) | `R2040` (22 Ω) | 串列時脈。由 Master 提供 10 MHz 時脈。 |
| **SPI MOSI** | GPIO63 (SIMO) | GPIO63 (SIMO) | `R2037` (22 Ω) | 主出從入。連接 Master TX 至 Slave RX。 |
| **SPI MISO** | GPIO64 (SOMI) | GPIO64 (SOMI) | `R2039` (22 Ω) | 主入從出。連接 Slave TX 至 Master RX。 |
| **SPI CS / STE**| GPIO66 (STE) | GPIO66 (STE) | `R2041` (22 Ω) | 從機致能。由 Master 控制之片選訊號 (Active Low)。 |
| **GND** | GND | GND | N/A | **共地線**。高頻通訊下必須共地以防波形失真。 |

> [!CAUTION]
> **共地安全警告：**
> 在進行高頻 10 MHz 測試前，必須使用粗短地線將兩塊開發板的 GND 進行低阻抗共地，否則高頻信號的反射過沖會導致資料位元滑移，甚至可能燒毀 GPIO 針腳。

### 2. SysConfig 設定說明
在主機與從機的 `pinmux.syscfg` 軟體設定中：
* **SPI Mode**：主機設為 `Master`，從機設為 `Slave`。
* **波特率 (Baud Rate)**：實物通訊設為 `10,000,000 Hz` (10 MHz)。
* **資料寬度 (Data Width)**：`16 Bits` (每次傳送一個 16-bit word 以對齊 FIFO)。
* **時脈極性與相位**：Mode 1 (CPOL = 0, CPHA = 1，時脈空閒為低，在下降沿鎖存輸入)。
* **FIFO Mode**：啟用 Rx FIFO 與 Tx FIFO，中斷閾值皆設為 2。
* **STE/CS Mode**：`Active Low`。

> [!WARNING]
> **實例命名混淆警告：**
> 在 `pinmux.syscfg` 軟體配置中，名為 **`SPIA_SYSTEM`** 的實例實際使用的是 **SPIB 硬體週邊**；而名為 **`SPIB_EXTFLASH`** 的實例則使用的是 **SPIA 硬體週邊**。切勿將邏輯實例名與硬體週邊搞混。

---

## 貳、 軟體驅動設計與 API 參考 (Software & API Reference)

本專案在主機端實現了扁平化驅動結構，將核心變數、測試應用變數與診斷變數進行了解耦隔離：

### 1. FIFO 讀寫指針與 Level 運作原理
C2000 的 SPI FIFO 硬體深度固定為 16 層 (16 x 16-bit words)。
* **Write Pointer**：每當實體引腳接收到一個 16-bit word，硬體寫入指針自動加 1。
* **Read Pointer**：每當軟體呼叫 `SPI_readDataNonBlocking()` 時，硬體讀取指針自動加 1。
* **FIFO Level**：可透過 `SPI_getRxFIFOStatus()` 取得，即 `(Write Pointer - Read Pointer)` 的差值，代表目前 FIFO 中有幾筆資料未讀。呼叫 `SPI_readDataNonBlocking()` 會使 Level 減 1。
* **resetRxFIFO() 的重要性**：呼叫 `SPI_resetRxFIFO(base)` 等同於強行將 Read Pointer 拉到與 Write Pointer 相等，將 Level 清零。在發起新的區塊傳輸 (Block Transfer) 前，**必須清空 RX FIFO**，否則上一輪殘留的垃圾數據會導致下一輪的 RxFIFO Status 提前滿足條件，使狀態機讀到錯位舊資料，進而判定校驗失敗。

---

### 2. 主機端關鍵 API 說明
主機代碼位於 `WP_3352_SPI_M/Emu_3352_SPI_M/SPIA_Master/SPI_master.c`，核心 API 如下：

#### A. SPI_Master_Init
* **原型**：`void SPI_Master_Init(ST_SPI_MASTER* inst, uint32_t spiBaseAddr);`
* **說明**：初始化 SPI 主機結構體，綁定實體外設地址，並配置狀態機進入 IDLE。

#### B. SpiMaster_Read
* **原型**：`uint16_t SpiMaster_Read(ST_SPI_MASTER* inst, uint16_t addr, SpiRxCallback_t cb);`
* **說明**：非阻塞發起 32-bit 暫存器讀取請求。註冊 Callback 函數。成功回傳 1，佇列滿回傳 0。

#### C. SpiMaster_Write
* **原型**：`uint16_t SpiMaster_Write(ST_SPI_MASTER* inst, uint16_t addr, uint16_t data, SpiRxCallback_t cb);`
* **說明**：非阻塞發起 32-bit 暫存器寫入請求。成功回傳 1，佇列滿回傳 0。

#### D. SpiMaster_WriteBlock
* **原型**：`uint16_t SpiMaster_WriteBlock(ST_SPI_MASTER* inst, const ST_SPI_MASTER_PACKET* pTxBuf, ST_SPI_MASTER_PACKET* pRxBuf, uint16_t u16Length, SpiBlockCallback_t cb);`
* **說明**：非阻塞批量發送大量封包（最大 4095 筆）。此 API 會在開頭主動調用 `SPI_resetRxFIFO` 清空緩衝區，並透過 FSM 控制 `SPI_CMD_BLOCK_TX` 狀態，每發送一包後進入 `SPI_CMD_BLOCK_TX_WAIT` 狀態等待 20us。

#### E. SPI_Master_Task
* **原型**：`void SPI_Master_Task(ST_SPI_MASTER* inst);`
* **說明**：主機端輪詢任務，必須放入 `main.c` 的主循環中。負責消化軟體發送佇列、控制 FSM 移位、處理超時重試與自癒。

#### F. getSpiMasterHandle / getSpiAppHandle / getSpiDiagHandle
* **原型**：
  * `ST_SPI_MASTER* getSpiMasterHandle(void);`
  * `ST_SPI_APP* getSpiAppHandle(void);`
  * `ST_SPI_DIAG* getSpiDiagHandle(void);`
* **說明**：獲取主機端核心、測試應用與診斷觀測結構體的指標，便於在 CCS Expressions 視窗中進行變數監控。

---

## 參、 5 大實體驗證測試情境

請依序在雙邊 CCS 偵錯環境的 **Expressions** 視窗中加入對應變數（如 `sMaster`, `s_stSpiApp`, `s_stSpiDiag`, `spiB_slave` 等）後執行測試：

### 測試情境一：基礎連線與傳統 32-bit 讀寫測試
* **目的**：驗證物理引腳與基本 32-bit 全雙工協定的正確性。
* **步驟**：
  1. 確認 Master 的 `sMaster.state` 為 `SPI_CMD_IDLE` (0)，Slave 的 `spiB_slave.fsm` 為 `_POP_RXD_FROM_SPI`。
  2. 於 Master 端手動將 `s_stSpiApp.cmdRead` 設為 1。
  3. **預期結果**：`cmdRead` 自動復歸為 0，且 `s_stSpiApp.testData` 成功更新為從機回傳的對應資料。
  4. 手動設定 `s_stSpiApp.testAddr = 0x0900`, `s_stSpiApp.testData = 0x0001`，並將 `s_stSpiApp.cmdWrite` 設為 1。
  5. **預期結果**：Slave 端的全域變數 `OUTPUT_ON` 變為 1，且 Master 端能正確收到資料寫入完成的對齊回應。

---

### 測試情境二：Block 批量寫入壓力測試 (Block Write)
* **目的**：驗證一次性發送大批資料時，Block 發送機制、位址校驗碼與資料 Echo 的正確性與穩定性。
* **步驟**：
  1. 設定 Master 的 `s_stSpiApp.u16StressLength` 為測試長度（例如 `4095`U，支持 1 ~ 4095）。
  2. 設定 `s_stSpiApp.u16ContinuousMode = 3` (Block Write 模式)。
  3. 將 `s_stSpiApp.u16StressEnable` 設為 1，啟動連續批量寫入測試。
* **預期結果**：
  * `s_stSpiApp.u32StressPassCnt` 快速以大步長（每次加 `Length`）累加。
  * `s_stSpiApp.u32StressFailCnt` 恆定保持為 0。表示在 20us 包間延遲保護與 FIFO 快速排空機制下，即使是最大 4095 筆的極限寫入也 100% 穩定且資料完全對齊。

---

### 測試情境三：Block 批量讀取與混合讀寫測試 (Block Read & Mixed WR/RD)
* **目的**：驗證大區塊批量讀取以及複合讀寫模式下的資料流穩定性。
* **步驟**：
  1. 設定 Master 的 `s_stSpiApp.u16StressLength = 4095`。
  2. 將 `s_stSpiApp.u16ContinuousMode` 設為 `4` (Block Read) 或 `5` (Mixed WR/RD) 模式。
  3. 將 `s_stSpiApp.u16StressEnable` 設為 1 啟動壓測。
* **預期結果**：
  * 批量讀取或複合讀寫的 FSM 能自動連續流轉，整組傳輸完成後觸發對應的 Block 回呼。
  * `s_stSpiApp.u32StressPassCnt` 持續高速累加，`s_stSpiApp.u32StressFailCnt` 保持為 0，無任何校驗失敗。

---

### 測試情境四：實體斷線、超時與防呆自恢復力測試
* **目的**：模擬高頻強干擾、丟包、甚至實體杜邦線全數拔除時，系統的容錯自癒能力。
* **步驟**：
  1. 啟動任一 Block 連續壓力測試 (`u16ContinuousMode` = 3, 4 或 5，且 `u16StressEnable = 1`)。
  2. 在運行中，直接將連接主從機的杜邦線**全數拔除**。
  3. **預期結果**：
     * Master 端偵測到 Block 傳輸超時（超時次數超過 50,000 次），自動觸發 `triggerMasterRecovery` 清空 FSM 與 FIFO 並進入 5ms 冷卻恢復期。`u32StressFailCnt` 開始慢速累加，但不發生死鎖。
     * Slave 端由於線路靜默超過 2ms，會自動執行一次 SPI 硬體模組重置，清空 FIFO，預載 Null 回應，為熱插拔做好準備。
  4. 重新將線路插回。
  5. **預期結果**：主從機自動恢復同步，`s_stSpiApp.u32StressPassCnt` 重新快速累加，`s_stSpiApp.u32StressFailCnt` 停止遞增。

---

### 測試情境五：背景 Flash 寫入與自癒力驗證
* **目的**：驗證當從機因為背景 Flash 寫入（如參數保存）引發約 1ms 的短暫 CPU 輪詢阻塞時，通訊的自癒能力。
* **步驟**：
  1. 啟動 Block 連續寫入壓力測試 (`u16ContinuousMode = 3`，`u16StressEnable = 1`)。
  2. 透過寫入指令觸發從機的 Flash 儲存寫入程序。
* **預期結果**：
  * 在 Flash 寫入的 1ms 期間，Master 的 `u32StressFailCnt` 僅微幅增加 1 或 2（由於從機被卡住未發送 Response 導致主機超時）。
  * 寫入一結束，從機的有界 `for` 迴圈排空緩衝區，配合主機的 **最低 20us 包間延遲保護**，通訊瞬間恢復 normal 狀態，無相位錯位且 Fail 停止遞增。
  * *調試經驗*：引進 RxFIFO 有界排空與自癒機制後，本系統成功將 `WAIT_IDLE_TICKS` 與 `WAIT_SLAVE_TICKS` 收斂並鎖定在 **20us (1000U Ticks)** 的極致安全時間，在此時序下實機壓力測試極度穩定，完全不累積任何 Fail 次數。
