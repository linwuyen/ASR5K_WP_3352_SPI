# [音檔說明]關於 TX（傳送端）採用 Ping-Pong Buffer 進行 Block 傳輸的技術說明

### 1. 為什麼 TX 需要 Ping-Pong Buffer？

在資料傳輸的架構中，通常會面臨不同資料量與硬體 FIFO 限制的衝突：

- **單筆傳輸（Single Word）**：如果 RX 接收端傳來的資料每次只有一筆，且 TX 回傳也只有一筆，此時硬體內建的 **FIFO（First-In, First-Out）** 容量便足以應付，不需要複雜的緩衝機制。
- **區塊傳輸（Block Transfer）**：當 RX 確定會有大筆資料傳入，或是 TX 未來需要回傳一個完整的數據區塊（例如超過 16 個 Word 的校正資料等）時，硬體的 FIFO 空間（通常容量有限）就會裝不下，導致資料溢位或傳輸中斷。
因此，必須導入 **DMA（Direct Memory Access，直接記憶體存取）** 搭配 **Ping-Pong Buffer（雙緩衝區）** 機制來解決這個問題。

### 2. Ping-Pong Buffer 與 DMA 的運作機制

Ping-Pong Buffer 是透過兩組獨立且對等（例如：Buffer A 與 Buffer B）的記憶體區塊進行交替切換。其運作邏輯如下：

[ MCU / CPU 寫入資料 ]
                     │
┌──────┴──────┐
▼                                       ▼
┌─────────┐   ┌─────────┐
│ BufferA            │    │            BufferB │
└─────────┘   └─────────┘
▲                                       ▲
└──────┬──────┘
│ [ DMA 自動搬移至 TX ]
                     ▼
[ 硬體傳送介面 ]

1. **資料填入（Ping 階段）**：
- CPU 或內部邏輯先將需要傳送的資料，放進第一組緩衝區（例如 Buffer A）。
1. **緩衝切換與 DMA 搬移（Pong 階段）**：
- 當 Buffer A 填滿後，系統會自動將核心切換至第二組緩衝區（Buffer B），讓後續的資料繼續填入。
- 與此同時，硬體觸發 **DMA**，將已經填滿的 Buffer A 內的整組資料直接搬移出去進行傳送。
1. **硬體自動交替**：
- 這兩組 Buffer 透過硬體機制自動輪流切換（A -> B -> A -> B），直到整組 Block 的大資料完全傳輸完畢。
- 這種設計能確保**資料填入**與**核心傳送**同時進行，達到不間斷的連續區塊傳輸。

### 3. 未來規劃與架構優化（Memory Map 對應）

雖然目前的舊做法可能是「一筆一筆資料讀取/回傳」，但為了因應未來可能的大資料傳輸需求（如：整組校正資料回傳），建議將雙方的溝通架構定義為 **Memory Map（記憶體映射）** 的 Block 範圍對應：

- **RX 端**：定義一塊固定的 Block Range 作為記憶體映射空間。
- **TX 端**：同樣定義一塊相對應的 Block Range。
- **效益**：未來雙方對接時，可以直接將「一整組記憶體（整組 Block）」直接塞給對方，而不需要像過去一樣，每一次傳輸都要單筆單筆反覆要求與回應，能大幅提升系統的傳輸效率與頻寬利用率。

### 4. 程式設計架構建議 (Doxygen 規範)

為了符合系統開發規範，以下提供針對此傳輸機制的 FSM（有限狀態機）與暫存器結構的 Doxygen 註釋設計範例：
c
/**

- @file tx_ping_pong.h
- @brief TX Ping-Pong Buffer 與 DMA 傳輸控制定義
- @note 適用於大數據區塊 (Block) 傳輸優化
*/

#ifndef TX_PING_PONG_H
#define TX_PING_PONG_H

#include <stdint.h>

/**

- @brief TX 傳輸狀態機列舉
- @details 使用 FSM 控制 Ping-Pong Buffer 的切換與 DMA 狀態
*/
typedef enum {
ST_00_IDLE, /**< 閒置狀態，等待傳輸觸發 */
ST_01_FILL_BUF_A, /**< 正在寫入 Buffer A，同時可用 DMA 傳送 Buffer B */
ST_02_FILL_BUF_B, /**< 正在寫入 Buffer B，同時可用 DMA 傳送 Buffer A */
ST_03_DMA_WAIT /**< 等待最後一組 DMA 區塊傳輸完成 */
} TX_FSM_STATE;

/**

- @brief Ping-Pong Buffer 結構體定義
- @note 命名規範：型態 + 長度 + 名稱
*/
typedef struct {
uint32_t u32_BufferA[32]; /**< 32-bit 長度 32 的 A組緩衝區 (Ping) */
uint32_t u32_BufferB[32]; /**< 32-bit 長度 32 的 B組緩衝區 (Pong) */
uint8_t u8_ActiveBank; /**< 8-bit 目前正在寫入的 Bank 編號 (0:A, 1:B) */
uint16_t u16_BlockSize; /**< 16-bit 預計傳輸的區塊大小 */
} STRUCT_DMA_BUFFER;

#endif /* TX_PING_PONG_H */

### 5. 總結

音訊中提及的核心觀念是：**「用空間換取連續傳輸的時間」**。當資料量大到 FIFO 無法負荷時，透過 DMA 搭配 Ping-Pong Buffer 進行整塊的 Memory Map 搬移，是確保大數據傳輸（如校正資料、連續音訊等）不中斷、不掉封包的最佳標準解法。