---
Category: Hardware & Peripherals
Status: Verified
Related Files: [rules.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/.agent/rules/rules.md)
---

# ASR5K SDRAM 硬體除錯與時序優化指南 (Troubleshooting)


本指南紀錄了 ASR5K 控制卡在開發初期遇到的 SDRAM 穩定性問題，並總結了一套結合「數學推導」與「數據快照」的軟體輔助硬體除錯 (Software-Assisted Hardware Debugging) SOP。

---

## 1. 診斷核心：位址摺疊 (Memory Aliasing)

### 1.1 現象描述
當執行 SDRAM 壓力測試時，寫入位址 `A` 的資料，竟然會覆蓋掉位址 `B` 的資料。例如：寫入位址 `0x80000600` 時，位於 `0x80000000` 的資料被幽靈覆蓋。

### 1.2 根因分析 (Root Cause)
這是由於 **SysConfig 軟體設定** 與 **實體晶片規格** 不符所導致：
* **軟體誤設**：SysConfig 中的 `Column Address Width (CAWIDTH)` 設定過大（例如 11 Bits / 2048 words）。
* **硬體限制**：實體晶片 (如 IS42S16160J) 僅支援 9 Bits Column。
* **物理後果**：當 EMIF 在 Burst 模式下自動增加位址時，它會拉高第 10 號位址線 (A9/A10)，但 SDRAM 晶片根本沒接這幾根線，導致位址訊號被忽略，最終位址就像摺紙一樣「摺疊」回原點。

### 1.3 參數推導 SOP (關鍵國小數學)
下次拿到新板子，請依此公式核對 SysConfig：
1. **看型號**：IS42S16160J (16Meg x16) → 總容量 = 2^24 Words.
2. **減 Bank**：4 個 Bank (2^2) → 每個 Bank 容量 = 2^22.
3. **數 Row**：看電路圖 A0~A12 接了 13 根線 → Row = 13 Bits.
4. **算 Column**：22 (總需求) - 13 (Row) = 9 → **結論：CAWIDTH 必須選 512 Words (9 Bits)**。

---

## 2. 效能優化：時序審計 (Timing Audit)

### 2.1 穩定性陷阱 (Flaky Errors)
即使位址正確，若壓力測試出現隨機、不連續的錯誤（Pass 數十萬次，Error 數萬次），通常是 **EMIF 時序違反了 Datasheet 物理極限**。

### 2.2 關鍵時序校對公式 (Time to Cycles)
在 100MHz (10ns/cycle) 的環境下，**「時間除以 10，遇小數就進位」** 是唯一原則。

| 參數 | 規格書要求 (ns) | 計算式 | SysConfig 設定 (Cycles) |
|---|---|---|---|
| **tRFC** | 60ns | 60 / 10 = 6 | **6~7** |
| **tRP** | 15ns | 15 / 10 = 1.5 | **2** (必進位) |
| **tRAS** | 37ns | 37 / 10 = 3.7 | **4~5** (必進位) |

### 2.3 穩定性進階：溫度與漏電 (Heat vs Leakage)
若系統出現「剛開機正常，跑 10 分鐘變熱後出錯」的現象，通常是 **Refresh Rate** 邊緣不穩：
*   **物理原理**：SDRAM 電容在熱態下漏電速度會加快。
*   **解決對策**：將 Refresh Rate 稍微縮短（例如從 781 降到 750），提高充電頻率，補償高溫帶來的電力流失。

---

## 3. 測試演算法與執行機制 (Test Algorithms & Execution)

### 3.1 基礎資料匯流排測試 (`HwVerification_SDRAM_RunTest`)
本測試針對資料線 (Data Bus) 的物理連通性進行單次掃描。
*   **演算法 (Walking 1s)**：在 SDRAM 首個位址 (`0x80000000`)，依序寫入 `(1 << 0)` 至 `(1 << 31)` 並立即讀回比對。此操作會在 32 根資料線上依序產生高電位，檢測是否有斷線 (Open) 或短路 (Short) 現象。
*   **異常捕捉**：若某根資料線空焊（永遠為 0），或兩根資料線短路在一起（連動改變），讀回的數值就會跟寫入的不同。此時狀態變數會紀錄出錯的位元索引。

### 3.2 壓力與位址摺疊測試 (`HwVerification_SDRAM_RunStressTest`)
本測試針對位址線 (Address Bus) 與高速時序 (Timing) 進行大區塊連續測試。
*   **演算法 (`~Address`)**：寫入時，將「實體記憶體位址取反 (`~Address`)」作為資料存入對應位址；讀取時再進行比對。
*   **異常偵測能力**：
    *   **位址摺疊 (Aliasing)**：若硬體位址線短路，或 `CAWIDTH` 設定過大，會導致寫入高位址時覆蓋低位址的資料。由於寫入資料具有位址唯一性，讀回時會立刻發現數值殘留。
    *   **時序不穩 (Timing Violation)**：若 100MHz 運作下 `tRFC` 或 `tRP` 等參數過於緊湊，會在連續讀寫中發生隨機錯誤 (Bit Flip)。

### 3.3 如何手動/背景觸發測試
目前上述函式已被升級為多模式狀態機 (Multi-mode FSM)，完全內聚於 `HwVerification.c` 內部管理：
1.  **單次觸發模式 (`u16Ctrl = 1`)**：透過上位機寫入 `1`，系統會立刻全速執行一次連線與壓力測試。通過後自動歸零 (`0`)，失敗則鎖定錯誤碼。
2.  **連續燒機模式 (`u16Ctrl = 2`)**：寫入 `2` 後，韌體內部排程器會自動每隔 `100ms` 在背景發動一次壓力測試，讓 `StressPass` 持續飆升。若中途發生任何錯誤，`u16Ctrl` 會立刻改寫為 `0x8100` 並中斷燒機，完美保護現場供工程師查驗。

---

## 4. 診斷工具與變數解析 (Diagnostics & Variables)

### 4.1 失敗快照 (Failure Snapshot) 機制
當測試失敗時，禁止僅顯示 `Pass/Fail`。必須在韌體中實作「第一案發現場凍結」機制：

```c
// 在讀取比較失敗時，立即紀錄這兩個變數並跳出迴圈
g_hwTest.stSdram.u32FailAddr = 錯誤發生的實體位址;
g_hwTest.stSdram.u32FailRead = 實際上讀出的髒資料;
g_hwTest.stSdram.u16Ctrl     = 0x8100; // 紀錄錯誤代碼
```

**數據鑑識範例：**
* 若 `Read = 0x7FFF7FFF`：代表 D15 位元恆為 0，高度懷疑 BGA 空銲或資料線斷路。
* 若 `Read = ~FailAddr` (但地址偏移了)：代表位址線設定錯誤，發生了 Aliasing。

### 4.2 診斷變數意義與用途 (Modbus 監控項目)
在除錯介面上（或 `g_hwTest` 結構體中）看到的這些變數，分別代表不同的硬體驗證意義：

| 變數名稱 | 意義 | 除錯觀點 |
|:---|:---|:---|
| **u16Ctrl** | 觸發與狀態機 | 單一暫存器多模式控制：<br>`0`：Idle / 測試成功<br>`1`：單次健檢觸發<br>`2`：進入連續 100ms 定時燒機模式<br>`0x80xx`：基礎資料線測試失敗<br>`0x8100`：壓力測試失敗 |
| **u32StressPass** | 壓力測試成功次數 | 代表 SDRAM 在高速連發下表現穩定的次數。這個值應該要一直跳動增加。 |
| **u32StressErr** | 壓力測試失敗次數 | **理想必須為 0**。如果不為 0 但 Pass 持續增加，代表硬體有 **Timing (時序)** 或 **SI (雜訊)** 問題。 |
| **u32FailAddr** | 案發現場：出錯的位址 | 用來判斷哪一根 **位址線 (Address Line)** 出問題。例如 `0x600` 代表 A9, A10 可能異常。 |
| **u32FailRead** | 案發現場：實際讀出值 | 用來判斷哪一根 **資料線 (Data Line)** 出問題。 |

---

## 5. 物理量測注意事項

若軟體反推與電表量測不符，請檢查以下「防呆」：
1. **Via 絕緣**：PCB 的導通孔孔環通常有透明綠漆，二極體檔 (Diode Mode) 量測前必須確保刺穿氧化層。
2. **基準點對照**：量測「懷疑腳位」前，先量測一根「已知正常」的腳位 (如 D0 或 LED 腳)。若連正常腳位都量出 `OL`，代表是量測手法錯誤。
3. **斷電量測**：所有二極體檔阻值量測，必須在 **板子完全斷電** 的狀態下執行。

---

## 6. 總結：ASR5K SDRAM 成功配置 (IS42S16160J)
* **CAWIDTH**: 9 Bits (512 Words)
* **Wait Cycles**: 全數放寬至 Datasheet 的 1.2 倍（以空間換取絕對穩定性）。
* **結果**: 100MHz 壓力測試達 1,000,000 次 0 Error。
