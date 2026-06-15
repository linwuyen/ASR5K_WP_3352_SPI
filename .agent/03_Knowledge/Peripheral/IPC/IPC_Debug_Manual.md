# IPC 偵錯與測試手冊 (Debug & Testing Manual)

本手冊說明如何使用 `IPC_ctl` 結構體與 `IPC_Test_Engine` 在 CCS Watch Window 中進行即時測試。

---

## 1. 偵錯準備 (Setup)
請在 CCS 下載程式後，於 **Expressions** 或 **Watch Window** 中輸入並展開變數：
- `IPC_ctl` (核心控制 context)
- `IPC_ctl.psLocalShm` (本機共享記憶體)
- `IPC_ctl.psRemoteShm` (對端共享記憶體)

---

## 2. A 類模式：指令手動注入 (Command Injection)
用於手動發送 A 類指令給對端。

### 測試步驟：
1. **設定指令 (eInjectCmd)**：修改為您要發送的 `IPC_Command_t` (如 `IPC_CMD_SET`)。
2. **設定參數 (u32InjectAddr / u32InjectD1)**：
   - `u32InjectAddr`：例如 `0x10` (代表電壓 Vin 索引)。
   - `u32InjectD1`：例如您要傳送的 `float` 轉 `uint32_t` 的數值。
3. **觸發發送 (u16InjectTrig)**：
   - 將 `u16InjectTrig` 設定為 **1**。
   - `IPC_Test_Engine` 會自動偵測並調用 `IPC_TxQueue_Push` 將指令壓入硬體暫存器。
   - 發送成功後，`u16InjectTrig` 會自動清零。
4. **驗證**：查看對端的 `u32LastRx` 是否更新，或查看對端的 `stPayload` 是否有變動。

---

## 3. B 類模式：數據流測試 (Streaming Test)
用於模擬 B 類高頻資料更新。

### 測試步驟：
1. **切換模式 (eMode)**：將 `eMode` 修改為 `TEST_MODE_TRAFFIC_RAMP` (值為 2)。
2. **觀察數據**：查看 `IPC_ctl.psLocalShm.stPayload.f32Vin`。
   - 您會看到該值開始自動以鋸齒波方式跳動 (100.0 ~ 200.0)。
3. **驗證**：進入對端的 Watch Window，觀察其 `psRemoteShm.stPayload.f32Vin`。由於 MSGRAM 是實體共享，對端應能即時看到數值變更而不需任何指令。

---

## 4. 壓力測試 (Stress Testing)
驗證 TX Queue 與硬體暫存器的吞吐能力。

1. **Flood 模式**：設定 `eMode` 為 `TEST_MODE_STRESS_FLOOD` (值為 3)。
   - 系統會無延遲地填入 `IPC_CMD_IDLE`。
   - 觀察 `stStats.u32Tx` 計數器會快速跳升。
2. **Burst 模式**：設定 `eMode` 為 `TEST_MODE_STRESS_BURST` (值為 4)。
   - 系統會瞬間塞滿 4 筆 Packet。
   - 如果 Queue 已滿，`stStats.u32TxDrop` 會加 1。

---

## 5. 安全與故障測試 (Safety Injection)
1. **注入錯誤**：設定 `eMode` 為 `TEST_MODE_SAFETY_FAULT` (值為 5)。
   - 系統會進入 `IPC_STATE_ERR` 狀態。
   - `psLocalShm.u32ErrorCode` 會顯示強制停止。
2. **故障恢復**：將 `eMode` 設定為 `TEST_MODE_SAFETY_CLEAR` (值為 6)。
   - 會清除 local 與對端的 ErrorCode 並嘗試回復系統運作。
