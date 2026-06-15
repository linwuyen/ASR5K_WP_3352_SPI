---
Category: Tools & Troubleshooting
Status: Verified
Related Files: [README.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/.agent/specs/knowledge/README.md)
---

# C2000 已知錯誤與排雷手冊 (Troubleshooting & KI)


本文件記錄 C28x 開發過程中常見的編譯錯誤與硬體陷阱，作為 AI 遇到問題時的優先查閱指引 (Knowledge Items)。

## 1. 編譯與 Linker 錯誤

### `error #10099-D: program will not fit into available memory`
- **原因**：宣告了過大的全域陣列、查表，或是把太多的 `.TI.ramfunc` 塞進了容量不足的 RAM 區塊（例如 `RAMLS0`）。
- **解法**：
  1. 打開 `.cmd` (Linker Command File)。
  2. 檢查 `PAGE 0` (Program) 或 `PAGE 1` (Data) 中的區段定義。
  3. 將大陣列宣告至較大的區段 (如 `RAMGS`)，或把相鄰的 RAM 區塊在 `.cmd` 中合併（例如 `RAMLS012 : origin = 0x008000, length = 0x001800`）。

### `warning #16002-D: build attribute vendor section TI missing in "xxx.obj"`
- **原因**：通常是因為編譯 FPU 相關數學運算時，部分檔案沒有正確開啟 `fpu32` 支援，導致軟體浮點與硬體浮點庫衝突。
- **解法**：確認編譯設定中的 `--float_support=fpu32` 是否全局開啟，並檢查是否誤用了非 FPU 版的 `math.h` 或 `rts2800_fpu32.lib`。

## 2. 執行期硬體陷阱

### 進入 `main()` 之前系統不斷 Reset (Watchdog Timeout)
- **原因**：`.cinit` 區段過大。C2000 從 Flash 啟動時，C Runtime 會自動將有初始值的全域變數搬移到 RAM 中。如果搬移時間超過硬體預設的 Watchdog 時間，就會無限重啟。
- **解法**：
  1. 減少在全域變數宣告時給定非零初始值。
  2. 改為在 `main()` 關閉 Watchdog 之後，再透過迴圈或 `memset` 進行軟體初始化。

### IPC 跨核讀取到髒數據 (Dirty Data)
- **原因**：多核同時存取 Shared RAM (如 `MSGRAM`)，且只使用了 `volatile` 修飾字，缺乏硬體層級的記憶體屏障防護。
- **解法**：寫入端寫完資料後，必須觸發 IPC Flag (如 `IpcRegs.IPCSET.bit.IPC0 = 1`)；讀取端必須使用 `IPC_waitForAck()` 確認，或在跨中斷的關鍵讀寫邊界強制加入 `asm(" RPT #3 || NOP");` 清空管線。
