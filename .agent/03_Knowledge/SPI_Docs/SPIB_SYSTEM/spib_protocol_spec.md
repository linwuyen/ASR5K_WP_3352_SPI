# ASR5K 主從機 SPI 通訊協定與封包規範 (spib_protocol_spec.md)

本文件定義主機 (AM3352 Master / 模擬端) 與從機 (C2000 Slave) 之間的 32-bit 標準全雙工流水線通訊協定，以及 64-bit 實驗性浮點數封包格式。本文件已收斂所有歷史衝突，作為本專案之單一真相來源 (SSOT)。

---

## 1. 32-bit 流水線全雙工協定 (U32_PACK)

由於 SPI 為全雙工 (Full-Duplex) 匯流排，Master 在發送第 N 筆 Command 的同時，收到的 MISO 資料為 Slave 對第 N-1 筆 Command 的 Response。

### 1.1 訊框結構 (Frame Structure)
每個傳輸訊框為 32-bit（拆分為兩個 16-bit 傳輸字組）：

*   **主機發送 (MOSI)**：
    *   **Word 1 (16-bit)**：`0xxx xxxx xxxx xxxx`。最高位 (Bit 15) 恆為 0 (代表 32-bit 封包標誌)，剩餘 15 位元為暫存器位址 (Address)。
    *   **Word 2 (16-bit)**：寫入數據 (Write Data) 或 讀取填充 (0x0000)。
*   **從機回傳 (MISO)**：
    *   **Word 1 (16-bit)**：對應前一筆指令的位址帶校驗碼 (Last Command Address + Checksum)。
    *   **Word 2 (16-bit)**：前一筆指令的響應數據 (Response Data)。

在 C 語言中，通訊聯集結構定義如下：
```c
typedef union {
    uint32_t u32All;
    struct {
        uint16_t u16Data;       /* Word 2: 資料欄位 (低 16 位元) */
        uint16_t u16Address;    /* Word 1: 位址欄位 (高 16 位元，最高位元為 0) */
    } stPack;
} HAL_U32PACK;
```

### 1.2 校驗碼計算 (Checksum Algorithm)
MISO 回傳位址中的校驗碼，是基於該次響應數據 (Response Data) 的高低位元組直接相加：

Checksum = (Response Data >> 8) + (Response Data & 0x00FFU)

響應位址計算公式：

Response Address = Last Command Address + Checksum

*   **實例對齊驗證**：
    *   **讀取版本號 (位址 0x0400，資料 0x0100)**：
        Checksum = 0x01 + 0x00 = 0x01。
        Response Address = 0x0400 + 0x01 = 0x0401。
        *物理證據*：CCS Expression 觀測值精確為 0x0401，完全吻合。
    *   **寫入控制暫存器 (位址 0x0900，資料 0x0001，Output ON)**：
        Checksum = 0x00 + 0x01 = 0x01。
        Response Address = 0x0900 + 0x01 = 0x0901。

---

## 2. 32-bit 連續通訊範例解析

### 2.1 連續寫入範例 (Continuous Write)
最後必須以一個 Null 訊框收尾以沖刷 (Flush) 流水線。

| 訊框順序 | MOSI (Master 發送) | MISO (Slave 回傳) | 說明與校驗計算 |
| :--- | :--- | :--- | :--- |
| **Write 1** | `0x09020011` | `0xFFFF0000` | 寫入 `0x0902` 暫存器 (Data = 0x0011)。此時 Slave 剛就緒，回傳預載 Null 資料。 |
| **Write 2** | `0x09112710` | `0x09130011` | 寫入 `0x0911` 暫存器 (Data = 0x2710)。<br>回傳 Write 1 的結果：Data = 0x0011。<br>Checksum = 0x00 + 0x11 = 0x11。<br>Response Address = 0x0902 + 0x11 = 0x0913。 |
| **Write 3** | `0x09100000` | `0x09482710` | 寫入 `0x0910` 暫存器 (Data = 0x0000)。<br>回傳 Write 2 的結果：Data = 0x2710。<br>Checksum = 0x27 + 0x10 = 0x37。<br>Response Address = 0x0911 + 0x37 = 0x0948。 |
| **Null** | `0xFFFF0000` | `0x09100000` | 發送結束 Null 訊框。<br>回傳 Write 3 的結果：Data = 0x0000。<br>Checksum = 0x00 + 0x00 = 0x00。<br>Response Address = 0x0910 + 0x00 = 0x0910。 |

---

## 3. 64-bit 實驗性封包規格 (U64_PACK)

為了支援單精度浮點數 (float32) 的原子性 (Atomicity) 傳輸，避免拆分傳輸造成的拼接錯位，設計了 64-bit 實驗性封包（4 個 16-bit Words）：

```text
Word 1 (16-bit) : 1 R/W addr[13:0]
    bit15 = 1  → 64-bit 標誌 (用於 Slave 接收分流)
    bit14 = R/W (0=Read, 1=Write)
    bits13-0 = 暫存器地址 (0x0000-0x3FFF)
Word 2 (16-bit) : payload low 16-bit (低 16 位元數據)
Word 3 (16-bit) : payload high 16-bit (高 16 位元數據)
Word 4 (16-bit) : checksum (四個 Byte 相加和)
```

在 C 語言中以多型 union 實作：
```c
#ifdef SPI_ENABLE_U64_TEST
typedef union {
    uint16_t all[4];            /* 物理傳輸陣列 */
    struct {
        uint16_t u16_header;    /* Word 1: 地址 (最高位 1 代表 64-bit) */
        uint32_t u32_data;      /* Word 2-3: 32-bit 數據 (可重組為 float) */
        uint16_t u16_checksum;  /* Word 4: 校驗和 */
    } stFloat;
} U64_PACK;
#endif
```

---

## 4. 32-bit (U32) 與 64-bit (U64) 對比分析

| 比較項目 | 32-bit 封包 (U32_PACK) | 64-bit 封包 (U64_PACK) | 說明與優劣分析 |
| :--- | :--- | :--- | :--- |
| **Word 數 / 幀長** | 2 Words (32 bits) | 4 Words (64 bits) | 64-bit 才能一次容納完整的 32-bit 浮點數 (float)。 |
| **Baud Rate 傳送時間**| 3.2 us (@ 10MHz) | 6.4 us (@ 10MHz) | 64-bit 的純硬體傳送時間僅增加 3.2 us。 |
| **等待間隔** | 20 us (CS High 閒置) | 20 us (CS High 閒置) | 兩者在傳輸中皆需等待從機背景處理的固定開銷。 |
| **單次通訊開銷** | 約 23.2 us | 約 26.4 us | **64-bit 通訊效率極高**。若以 32-bit 傳輸一個 float，需拆成兩次通訊，總開銷為 46.4 us。64-bit 僅需 26.4 us，**速度提升約 43%**。 |
| **適用情境** | 控制開關、狀態讀寫、快速輪詢 | 大資料量（float、int32、複合結構）傳輸 | 32-bit 適合高頻低延遲控制；64-bit 適合高精度資料與校正參數。 |

---

## 5. 設計指導原則 (Guidelines)

1.  **Pipeline 沖刷 (Flush)**：
    每次對暫存器的寫入或讀取指令，**強烈建議在序列最後再傳送一次 Null 包 (0xFFFF0000) 作為結尾**。這可確保最後一筆指令的 Response 能夠被完全移位出來，不殘留在從機的 FIFO 中。
2.  **異常自癒 (Self-Healing)**：
    若發生物理雜訊丟包，主從機將透過超時檢測（主機 50,000 次循環超時，從機 2ms 靜默超時）執行模組重置與 FIFO 清空，重新建立對齊。
