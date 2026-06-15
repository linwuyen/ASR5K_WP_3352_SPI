# U64_PACK 64-bit 浮點數通訊實驗計畫與測試報告 (spib_u64_experimental_report.md)

本文件整合了 ASR5K 專案中，主從機 SPIB 通訊鏈路引入 64-bit 封包 (U64_PACK) 以傳輸 32-bit float 單精度浮點數的**實驗性實作計畫**與**測試報告**。

> [!IMPORTANT]
> **定位說明：**
> 本文件描述的 64-bit 封包傳輸為**實驗性功能**。最終生產環境仍維持 32-bit 協定。
> 所有 64-bit 程式碼均透過編譯巨集 `SPI_ENABLE_U64_TEST` 進行完整隔離，方便一鍵切換與未來安全退場。

---

## 1. 32-bit vs 64-bit 封包結構對照

在 ASR5K 系統中，32-bit 與 64-bit 封包的物理與邏輯定義對照如下：

| 項目 | 32-bit 封包 (U32_PACK) | 64-bit 封包 (U64_PACK) |
| :--- | :--- | :--- |
| **Word 數 / 幀長** | 2 Words (16-bit x 2 = 32 bits) | 4 Words (16-bit x 4 = 64 bits) |
| **Word 1** | `addr[14:0]` (最高位恆為 0，15-bit 位址) | `bit15 = 1` (64-bit 標誌)<br>`bit14 = R/W` (0=Read, 1=Write)<br>`addr[13:0]` (14-bit 位址) |
| **Word 2** | `data` (16-bit 數據) | `payload_low` (低 16-bit 數據) |
| **Word 3** | 無 (N/A) | `payload_high` (高 16-bit 數據，對應 IEEE-754 浮點數) |
| **Word 4** | 無 (N/A) | `checksum` (8-bit 累加和) |
| **CLK 傳送時間** | 3.2 us (@ 10MHz SCLK) | 6.4 us (@ 10MHz SCLK) |
| **適用情境** | 快速控制指令、狀態讀寫、低延遲輪詢 | 大資料量（float、int32 等）、高精度參數校正 |

---

## 2. 實驗功能隔離設計 (Isolation Boundaries)

為確保生產環境程式碼的純淨度，所有 64-bit 實驗性結構體與邏輯均被編譯期巨集包覆：

```text
SPI_master.h (主機端標頭檔)
├── [永遠存在] ST_SPI_MASTER_PACKET (32-bit 封包)
├── [永遠存在] ST_SPI_MASTER
└── #ifdef SPI_ENABLE_U64_TEST
    ├── U64_PACK union (Layout A 匿名結構)
    └── ST_SPI_MASTER.app 新增 u16PacketMode, cmdSendFloat, f32TestValue
    #endif

SPI_master.c (主機端發送源碼)
├── [永遠存在] 所有 32-bit 傳送與 FIFO 排程流程
└── #ifdef SPI_ENABLE_U64_TEST
    ├── SpiMaster_SendFloat() (封裝 U64_PACK 推入佇列)
    └── onU64PackSentComplete() (回呼解析從機回備 4 Words)
    #endif

cmd_parser.h (從機端解析器)
└── #ifdef SPI_ENABLE_U64_TEST
    ├── 若 (u16Address & 0x8000) == 0x8000 → 進入 64-bit 解析分支
    ├── parseGroup0x09_64bit() (接收 RxFIFO 剩餘 2 Words 並組裝為 float)
    └── pushU64PackIntoTxD() (回備 4 Words 給主機)
    #endif
```

---

## 3. 系統實作細節

### 3.1 主機端 (Master) 浮點數拆解與重組
由於 C2000 C28x 核心對於 32-bit 資料的對齊限制，在 Union 結構中宣告 `uint32_t` 可能導致編譯器自動插入無效的 Padding Word。因此本專案採用匿名結構 Layout A，並在 C 語言中以 Union 做物理對齊：

```c
#ifdef SPI_ENABLE_U64_TEST
typedef union {
    uint16_t all[4];
    struct {
        uint16_t u16_header;    /* Word 1: 地址 | 0x8000 */
        uint32_t u32_data;      /* Word 2-3: 32-bit float */
        uint16_t u16_checksum;  /* Word 4: 校驗和 */
    };
} U64_PACK;
#endif
```
主機的傳送狀態機中，若 `u16PacketMode == 1`，發送完 Word 1 & 2 後會非阻塞地繼續壓入 Word 3 & 4。

### 3.2 從機端 (Slave) 0us 阻塞 Yield-Polling 狀態機
在從機接收端，為了避免 64-bit 的 4-word 傳輸對原本 32-bit (2-word) 的軟體佇列造成拆包污染，且避免在輪詢中長達數十微秒的 CPU 阻塞等待，我們實現了 Yield-Polling 非阻塞接收狀態機：

* **RX_STATE_IDLE**：
  從機讀取 RxFIFO 的第一個 Word，檢查 `bit15`。
  * 若 `bit15 == 0`，判定為標準 32-bit 封包，走原接收佇列路徑。
  * 若 `bit15 == 1` (且地址符合測試暫存器 `0x09F0U`)，判定為 64-bit 封包。狀態機切換至 `RX_STATE_WAIT_U64_BODY`，並**立刻退出 (return) 讓渡 CPU 執行權**，達成 0us CPU 阻塞。
* **RX_STATE_WAIT_U64_BODY**：
  在後續的每次主迴圈輪詢中，檢查 RxFIFO 狀態。
  * 若 `RxFIFO >= 3` 字組，代表剩餘的 3 個 Word 已全部到達。一次讀出並調用 `parseU64PackDirect` 將浮點數原子性還原存入 `g_f32SlaveRxTestVal`，回傳 4-word echo 封包，最後歸還至 `RX_STATE_IDLE`。
  * 若 `RxFIFO < 3` 且未超時，**立刻退出 (return)**，繼續等待下一次主迴圈輪詢。
  * **超時防呆自癒**：如果發生斷線或干擾丟包，超時計數器 `s_u32RxTimeoutCnt` 累加超過 `SLAVE_RX_TIMEOUT_LIMIT` (100000) 時，從機主動執行 SPI 硬體模組重置並回歸 IDLE，防止死鎖。

---

## 4. 自動壓力測試與對比分析

### 4.1 壓力測試方法 (Stress Test)
1. 於主機端 CCS Expressions 中設定 `sMaster.app.u16PacketMode = 1`。
2. 設定 `sMaster.app.u16StressEnable = 1`。
3. 主機自動從 `f32TestValue = 1.0f` 開始高速發送。每次傳輸完成回呼時，自動比對接收回傳的值是否與送出值相等。若相等則 `u32StressPassCnt++`，否則 `u32StressFailCnt++`，並自動累加 `1.0f` 循環測試。

### 4.2 U32_PACK vs U64_PACK 效能對比

| 比較項目 | 32-bit 封包 (U32_PACK) | 64-bit 封包 (U64_PACK) | 說明與優劣分析 |
| :--- | :--- | :--- | :--- |
| **傳輸資料量** | 16-bit 數據 + 16-bit 校驗 | 32-bit 數據 + 16-bit 標誌/校驗 | 64-bit 才能一次原子性容納完整的 32-bit float。 |
| **等待間隔** | 30 us (CS High 閒置) | 30 us (CS High 閒置) | 兩者在傳輸中皆需等待從機處理的 30 us 固定開銷。 |
| **單次通訊開銷** | 約 35.2 us | 約 38.4 us | **64-bit 通訊效率極高**。若以 32-bit 傳輸一個 float，需拆成兩次通訊，總開銷為 70.4 us。64-bit 僅需 38.4 us，**速度提升約 45%**。 |
| **代碼複雜度** | 低，符合常規佇列 | 中，需手動重組且有 Yield 狀態機 | 64-bit 需處理 C28x 對齊與非阻塞狀態機分流。 |
| **對即時系統影響**| 無額外 CPU 佔用 | 從機 poll 需 Yield-Polling 輪詢 | 每次 64-bit 接收會佔用從機極微幅的輪詢時間。 |

* **結論**：
  若系統需要大量傳輸 32-bit 浮點數（如高精度溫度、電壓回授、校正參數），使用 64-bit 封包在頻寬與原子安全性上具備絕對優勢。若僅是簡單控制指令與整數狀態，則應維持 32-bit 以降低代碼複雜度。

---

## 5. 測試完成後的代碼清理 (退場機制)

當本實驗測試結束後，可根據決定執行以下退場或保留操作：

1. **僅關閉實驗功能（保留代碼做診斷）**：
   在主機與從機的 CCS Project Properties -> Build -> Compiler -> Predefined Symbols 中，**移除 `SPI_ENABLE_U64_TEST` 定義**。重新編譯後，所有 64-bit 邏輯區塊將被編譯器完全忽略，Flash 佔用與純 32-bit 版本完全相同。
2. **完全清除程式碼**：
   若確定生產環境不再需要，可利用 Git 回滾，或搜尋專案中所有出現 `SPI_ENABLE_U64_TEST` 的段落，手動刪除巨集包覆的代碼區塊。
