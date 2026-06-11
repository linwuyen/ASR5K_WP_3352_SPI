# R02_2_FSI_OPT_PACKAGE

# R02_2 - FSI 封包優化評估 (Optimized Packet Evaluation)

本文件評估新的封包結構，旨在檢討是否能在此架構下支援 **1 Master + 3 Slaves** (M+3S)。

## 1. 提案封包結構 (Proposed Packet Structure)

### 1.1 Master Payload (Downstream)

Master 發出的指令與全域廣播資料。

| 內容 | 位元數 (Bits) | Word 數 (16-bit) | 說明 |
| --- | --- | --- | --- |
| **L1_Ref** | 16 | 1 | Inductor Current Ref 1 |
| **L2_Ref** | 16 | 1 | Inductor Current Ref 2 |
| **L3_Ref** | 16 | 1 | Inductor Current Ref 3 |
| **CV_AD** | 16 | 1 | Voltage Control Output (AD units?) |
| **ID_Func** | 4 + 12 = 16 | 1 | 指令 ID (4b) + 目標節點 (12b) |
| **Data_H** | 16 | 1 | 32-bit Data High Word |
| **Data_L** | 16 | 1 | 32-bit Data Low Word |
| **總計 (Master)** | **112 bits** | **7 Words** |  |

### 1.2 Slave Payload (Upstream)

每個 Slave 回傳的狀態資料。

| 內容 | 位元數 (Bits) | Word 數 (16-bit) | 說明 |
| --- | --- | --- | --- |
| **Vout** | 16 | 1 | Output Voltage |
| **Iout** | 16 | 1 | Output Current |
| **Status** | 16 | 1 | System Status / Flags |
| **總計 (Per Slave)** | **48 bits** | **3 Words** |  |

---

## 2. 容量與擴充性分析 (Capacity Analysis)

FSI 單一 Data Frame 最大容量為 **16 Words** (User Data)。

### 2.1 M + 2S 架構 (現行)

- **Master**: 7 Words
- **Slave 1**: 3 Words
- **Slave 2**: 3 Words
- **總需求**: 7 + 3 + 3 = **13 Words**
- **剩餘空間**: 16 - 13 = **3 Words**
- **判定**: **可行 (Pass)**，且還有餘裕。

### 2.2 M + 3S 架構 (擴充目標)

- **Master**: 7 Words
- **Slave 1**: 3 Words
- **Slave 2**: 3 Words
- **Slave 3**: 3 Words
- **總需求**: 7 + 3 + 3 + 3 = **16 Words**
- **剩餘空間**: 16 - 16 = **0 Words**
- **判定**: **可行 (Pass)**，剛好滿載。

---

## 3. 欄位映射表 (Word Mapping) - M+3S Full Load

為了實現 M+3S，封包將被塞滿至極限 (16 Words)。

| Word Index | Content | Writer | Reader | Description |
| --- | --- | --- | --- | --- |
| **0** | `L1_Ref` | Master | All Slaves |  |
| **1** | `L2_Ref` | Master | All Slaves |  |
| **2** | `L3_Ref` | Master | All Slaves |  |
| **3** | `CV_AD` | Master | All Slaves |  |
| **4** | `ID_Func` | Master | All Slaves | NodeID(4) + FuncID(12) |
| **5** | `Data_H` | Master | All Slaves | 32-bit Data High Word |
| **6** | `Data_L` | Master | All Slaves | 32-bit Data Low Word |
| **7** | `S1_Vout` | **Slave 1** | Master |  |
| **8** | `S1_Iout` | **Slave 1** | Master |  |
| **9** | `S1_Stat` | **Slave 1** | Master |  |
| **10** | `S2_Vout` | **Slave 2** | Master |  |
| **11** | `S2_Iout` | **Slave 2** | Master |  |
| **12** | `S2_Stat` | **Slave 2** | Master |  |
| **13** | `S3_Vout` | **Slave 3** | Master |  |
| **14** | `S3_Iout` | **Slave 3** | Master |  |
| **15** | `S3_Stat` | **Slave 3** | Master |  |

---

## 4. 潛在風險與注意事項

1. **無任何擴充空間**: 封包已達硬體上限 (16 words)。無法再增加任何 Status bit 或 Master command。
2. **DMA 設定複雜度增加**:
    - Slave 1 DMA 必須覆寫 `TX_BUF[7..9]`。
    - Slave 2 DMA 必須覆寫 `TX_BUF[10..12]`。
    - Slave 3 DMA 必須覆寫 `TX_BUF[13..15]`。
    這意味著每個 Slave 的 DMA 記憶體映射 (Destination Address Offset) **不同**，需依據其 ID 動態配置。

## 5. 結論

採用新的精簡封包結構 (Master 7 Words + Slave 3 Words) 是 **完全可行** 的，並且成功在單一 Frame 中實現了 **1 Master + 3 Slaves** 的擴充目標。
建議採納此設計作為 R02_2 版的標準。