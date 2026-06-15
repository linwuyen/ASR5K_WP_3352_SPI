# IPC (Inter-Processor Communication) 模組功能說明文件

本文件詳細說明 F28388x 雙核系統中，CPU1 與 CPU2 之間高效能通訊模組的開發成果。該模組旨在實現低延遲、非阻塞且具備高度診斷能力的數據交換。

## 1. 系統架構 (System Architecture)

IPC 模組遵循「三層解耦」設計原則，確保代碼的高移植性與可維護性：

- **硬體抽象層 (HAL)**: 封裝 TI DriverLib `IPC_init`, `IPC_sync`, `IPC_setFlagLtoR` 等暫存器操作。
- **功能邏輯層 (Functional Layer)**: 實實作循環緩衝區 (Circular Buffer)、狀態機 (FSM) 與指令分發器 (Dispatcher)。
- **應用服務層 (Service Layer)**: 定義具體的 `IPC_Command_t` 指令處理函式 (Handlers) 與共享記憶體數據結構。

### 記憶體佈局 (Memory Layout)
使用 F28388x 特有的 Message RAM (`MSGRAM`) 進行雙向通訊：
- **CPU1 to CPU2**: 定義於 `MSGRAM_CPU1_TO_CPU2` 段。
- **CPU2 to CPU1**: 定義於 `MSGRAM_CPU2_TO_CPU1` 段。

---

## 2. 核心功能特性 (Core Features)

### 2.1 非阻塞非同步傳輸 (Non-blocking Async Tx)
- **傳輸管道**: 使用核心中的 `IPC_MsgQueue_t` (大小為 16 slots) 緩衝發送請求。
- **Pipeline 機制**: 呼叫 `IPC_TxQueue_Push` 將任務推入佇列，由後台任務 `IPC_Tx_ProcessTask` 在硬體信箱空閒時自動發送。
- **優勢**: 避免控制迴路中的 CPU 阻塞，確保高頻控制算法的確定性（Determinism）。

### 2.2 基於指令的分發機制 (Command-based Dispatch Rx)
- **硬體觸發**: 利用 IPC Flag 0 作為通訊鈴音。
- **自動分發**: `IPC_Rx_PollTask` 偵測到旗標後，根據 `c_IPC_Handlers` 分發表自動調用對應的處理函式。
- **指令支持**: 涵蓋核心控制 (`IPC_CMD_STOP`)、數據更新 (`IPC_CMD_SET`)、回調確認 (`IPC_CMD_CALLBACK`) 等。

### 2.3 雙核握手與同步 (Boot Handshake & Sync)
- **Barrier Sync**: 啟動時使用硬體 Flag 31 進行 Barrier 同步，確保雙核皆已初始化。
- **狀態演進**: 遵循 `IDLE -> SYNC -> RUN` 的狀態演進。只有在握手成功 (`IPC_SYNC_PASS`) 後才會進入 `RUN` 模式，避免未定義行為。

### 2.4 心跳監控與安全連鎖 (Heartbeat & Safety Interlock)
- **活體偵測**: 透過 `u32TimestampHW` 在共享記憶體中持續跳變進行心跳監測。
- **超時保護**: 若遠端核心在 `IPC_HEARTBEAT_TIMEOUT` 週期內未更新時間戳，本地將強制進入 `IPC_STATE_ERR`。
- **連鎖機制**: `IPC_CheckSafetyInterlock` 確保一旦有一核發生致命錯誤，另一核會同步進入報警狀態，防止損壞電源硬體。

---

## 3. 診斷與監控工具 (Diagnostics & Toolkit)

### 3.1 流量與錯誤統計 (Traffic Stats)
即時監控通訊健康度，欄位包含：
- `u32Tx / u32Rx`: 成功發送與接收計數。
- `u32TxDrop`: 因佇列滿載而丟棄的封包數。
- `u32RxErr`: 通訊協議錯誤或非法指令計數。

### 3.2 錯誤快照 (Error Snapshot)
當系統切換至 `IPC_STATE_ERR` 時，模組會自動執行「死後剖析」快照：
- 記錄 `u32ErrorCode`、故障時間戳。
- 備份關鍵數據（如 `f32Vin`, `f32Iout`），方便工程師離線追蹤首位故障點。

### 3.3 自動化測試引擎 (Test Engine)
內建 `IPC_Test_Engine` 工具，提供多種開發測試模式：
- `TEST_MODE_STRESS_FLOOD`: 極限頻寬壓力測試。
- `TEST_MODE_TRAFFIC_RAMP`: 鋸齒波模擬遙測數據更新。
- `TEST_MODE_SAFETY_FAULT`: 人為故障注入，驗證安全連鎖邏輯。

---

## 4. 關鍵數據結構 (Data Structures)

### IPC_Context_t (核心上下文)
該結構體被精心「扁平化」設計，旨在方便開發者在 CCS (Code Composer Studio) 的 **Watch Window** 中直觀查看所有運行時變數（State, SyncStatus, Traffic, TestData）。

### IPC_SharedMemory_t (共享記憶體)
定義了傳輸協議的「大腦」，包含：
1. **Header**: 狀態同步訊息。
2. **Diagnostics**: 錯誤碼與快照。
3. **Mailbox**: 指令封包格式 (Cmd / Addr / Data1 / Data2)。
4. **Payload (Streaming)**: 高頻數據流動區域 (f32Vin, f32Iout)，實現無延遲讀取。

---

## 5. 性能設計規範

1. **嚴禁除法**: 內部運算嚴格執行純乘法或移位操作。
2. **零等待自旋**: 任務迴圈中不使用 `while` 等待旗標，確保其符合電源控制系統的非阻塞要求。
3. **最小化中斷**: 採用輪詢分發機制，降低頻繁通訊對控制 ISR 的干擾。
