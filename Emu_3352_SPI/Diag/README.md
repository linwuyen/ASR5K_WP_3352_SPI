# 通用通訊診斷模組 (Generic Communication Diagnostics)

此診斷模組是一個通用、輕量且高移植性的 C 語言通訊診斷框架。它適用於各種嵌入式通訊協定（如 SPI, CAN, I2C, UART 等），專門用來鎖定通訊崩潰時的**「第一筆錯誤現場」**。

---

## 📂 模組檔案
* **`comm_diag.h`**：定義診斷暫存器結構與 API 介面。
* **`comm_diag.c`**：診斷核心邏輯（錯誤鎖存、計數器更新）。

---

## ⚙️ 診斷暫存器結構 (`ST_COMM_DIAG`)

診斷結構體內含兩類暫存器：
1. **統計計數器 (Statistical Counters)**：持續累加。
   * `u32TxTotal`：累計發送的封包數。
   * `u32RxTotal`：累計接收的封包數。
   * `u32ErrCount`：累計發生錯誤的次數。
   * `u32MaxQueueDepth`：歷史上最大佇列積壓深度。
   * `u32ResetCount`：歷史上通訊模組重置的次數。
2. **首發錯誤鎖存器 (First-Error Latch)**：**只記錄第一筆發生的錯誤**，後續錯誤在解鎖前不會覆蓋它。
   * `u16LastErrType`：首發錯誤類型 (定義於 `E_COMM_ERR_TYPE`)。
   * `u16LastErrStep`：發生首發錯誤時的軟體步驟/狀態機 ID。
   * `u32LastErrTime`：首發錯誤的時間戳。
   * `u16ErrDetail[4]`：首發錯誤時的詳細上下文字節（如：預期值、實際值、錯誤位址、雜訊值等）。

---

## 🛠️ 移植與使用步驟

### 步驟 1：嵌入與初始化
在您的通訊驅動/應用結構體中嵌入 `ST_COMM_DIAG`，並在模組初始化時呼叫 `CommDiag_Init()`。

```c
#include "Diag/comm_diag.h"

typedef struct {
    // 您原有的驅動變數...
    uint32_t spiBaseAddr;
    
    // 嵌入診斷標配
    ST_COMM_DIAG diag;
} ST_SPI_DRIVER;

ST_SPI_DRIVER g_sSpiMaster;

void initMySpi(void) {
    // 初始化驅動...
    CommDiag_Init(&g_sSpiMaster.diag);
}
```

### 步驟 2：記錄發送與接收
在每次成功發送或接收封包時，更新對應的累加計數器。

```c
void onSpiTxComplete(void) {
    g_sSpiMaster.diag.u32TxTotal++;
}

void onSpiRxComplete(void) {
    g_sSpiMaster.diag.u32RxTotal++;
}
```

### 步驟 3：回報與鎖定錯誤
當通訊超時（Timeout）、校驗和錯誤（Checksum Error）或數值異常時，呼叫 `CommDiag_ReportError()`。

```c
// 範例：校驗和錯誤時鎖定現場
if (calculated_chk != received_chk) {
    CommDiag_ReportError(
        &g_sSpiMaster.diag,
        ERR_COMM_CHECKSUM,         // 錯誤類型
        current_step_id,           // 當前步驟 ID
        U32_UPCNTS,                // 當前平台計時器 Ticks (例如 C2000 的 U32_UPCNTS)
        calculated_chk,            // Detail 0: 預期校驗和
        received_chk,              // Detail 1: 實際校驗和
        target_addr,               // Detail 2: 目的位址
        received_data              // Detail 3: 異常資料
    );
}
```

### 步驟 4：監控佇列堆積
若您的系統有背景環形緩衝區（Ring Buffer），在接收中斷或輪詢中，呼叫此 API 記錄最大積壓量。

```c
void pollRxBuffer(void) {
    uint32_t current_depth = getBufferCount();
    CommDiag_UpdateQueueDepth(&g_sSpiMaster.diag, current_depth);
}
```

---

## 🔍 CCS (Code Composer Studio) 除錯指南

1. **Expressions 視窗動態觀測**：
   * 在 CCS 中，將驅動變數的 `diag` 欄位（例如 `g_sSpiMaster.diag`）加入 Expressions 視窗。
   * 開記 **Continuous Refresh** 讓數值動態更新。
2. **定位首發錯誤**：
   * 通訊掛掉時，直接檢視 `u16LastErrType`。如果它是 `1` (ERR_COMM_TIMEOUT_ACK)，並且 `u16LastErrStep` = `3`，表示在 Step 3 時發送資料後，從機超過時間未回傳 ACK。
   * 透過 `u16ErrDetail` 陣列，可直接讀出當時發送的位址與資料，不需要去翻記憶體或對照複雜的變數。
3. **解除鎖定以捕捉下一次錯誤**：
   * 若想清除當前的錯誤鎖存，在 CCS Expressions 視窗中手動將 `u16LastErrType` 修改為 `0`，或者在軟體中呼叫 `CommDiag_ClearLatch(&g_sSpiMaster.diag)`。
