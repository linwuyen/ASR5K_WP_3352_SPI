# HW_SPI_Flash_Verification_Manual

本文件記錄 ASR5K V2 控制板上 W25Q64JV SPI Flash 的硬體配置參數與測試驗證流程。

## 1. C28x SPIA 硬體詳細設定 (Confirmed Baseline)

為確保通訊穩定，特別是寫入與擦除指令的可靠性，SPIA 必須嚴格遵循下列配置：

| 參數項目 | 設定值 | 說明 |
| :--- | :--- | :--- |
| **Module** | **SPIA** | 專用於外部 Flash。 |
| **Protocol** | **Mode 1 (POL0, PHA1)** | 實測證實比 Mode 0 更穩定 (見後續技術說明)。 |
| **Bitrate** | **1,000,000 (1MHz)** | 初期驗證頻率，穩定後可視需求提升。 |
| **Data Width** | **8-bit** | C28x 要求資料須「左對齊」(Left-justified)。 |
| **FIFO Mode** | **Enabled** | TX FIFO Level: 16, RX FIFO Level: 0。 |
| **PTE/STE Polarity** | **Active Low** | 硬體自動控制 CS 引腳。 |
| **Emulation Mode** | **Stop Midway** | 斷點調試時停止通訊，防止 FIFO 溢位。 |

### 引腳定義 (Pinmux)
*   **PICO (SIMO)**: GPIO 16
*   **POCI (SOMI)**: GPIO 17
*   **CLK (SPICLK)**: GPIO 18
*   **STE (SPISTE)**: GPIO 19 (連至 W25Q64 CS 引腳)

---

## 2. 測試觸發指令表 (Trigger List)

透過修改 `g_hwTest.stFlash.u16Trigger` 執行下列功能：

| Trigger | 功能名稱 | 預期結果 / 觀測變數 |
| :--- | :--- | :--- |
| **1** | **Read ID (0x9F)** | `stFlash.u32ID` 應為 `0x00EF4017` (Winbond W25Q64)。 |
| **2** | **WREN (0x06)** | 寫入致能，執行 3 或 5 之前必須先觸發一次。 |
| **3** | **Sector Erase (0x20)** | 對位址 `0x000000` 執行擦除，讀取結果應變為 `0xFFFFFFFF`。 |
| **4** | **Read Data (0x03)** | 讀取位址 `0x000000` 的資料，結果統一存於 `stFlash.u32Data`。 |
| **5** | **Page Write (0x02)** | 將測試資料 `0x55AA` 寫入位址 `0x000000`。 |

---

## 3. 關鍵實作要點 (Technical Notes)

### A. 左對齊要求 (Alignment)
由於硬體配置為 8-bit 模式，寫入 `SPITXBUF` 時必須左移 8 位。
```c
SPI_writeDataNonBlocking(SPIA_BASE, (uint16_t)(data << 8));
```

### B. 硬體 STE (CS) 連動邏輯與 FIFO 協作
在 ASR5K V2 的 100% 硬體 SPI 實作中，STE (CS) 訊號能穩定維持在低電位，主要歸功於下列機制：

1.  **FIFO 的緩衝作用**：
    當呼叫 `SPI_writeDataNonBlocking` 時，資料會先存入 16 層深的 **TX FIFO**。由於 C28x CPU (200MHz) 寫入速度遠快於 SPI 傳輸速度 (1MHz)，整串指令（如 4~6 Bytes）會在第一個 bit 傳完前就全部進入 FIFO 排隊。
2.  **STE 自動維持機制**：
    C28x SPI 硬體邏輯規定：**「只要 TX FIFO 內還有資料，或是移位暫存器 (Shift Register) 正在傳輸，STE 引腳就會維持在有效電位 (LOW)。」**
3.  **結論**：
    在目前的 1MHz 頻率下，CPU 幾乎是「瞬間」灌滿 FIFO，這確保了 STE 訊號在整個多位元組通訊過程中不會跳變，完全不需要軟體切換 GPIO。

### C. WREN (Write Enable) 的使用時機
WREN 指令 (`0x06`) 僅在涉及「修改內容」時才需要，這是 Flash 的硬體保護機制。

*   **需要 WREN 的指令**：
    *   `Page Program (0x02)`
    *   `Sector Erase (0x20)`
    *   `Write Status Register (0x01)`
*   **不需要 WREN 的指令 (直接執行)**：
    *   `Read ID (0x9F)`
    *   `Read Data (0x03)`
    *   `Read Status Register (0x05)`

### D. 為何選擇 Mode 1 (PHA=1)？
雖然 W25Q64 支援 Mode 0/3，但在 ASR5K V2 硬體平台上選用 **Mode 1** 是基於硬體調優 (Hardware Tuning) 的深層考量：

1.  **補償訊號延遲 (Signal Skew Compensation)**：
    在高頻通訊下，時脈從 C28x 出發到資料從 Flash 回傳會產生數十 ns 的往返延遲。
    *   **Mode 0**：在時脈上升沿立即取樣，若延遲較大可能導致建立時間 (Setup Time) 不足。
    *   **Mode 1**：將取樣點往後推遲了**半個時脈週期**，為訊號穩定提供了極大的緩衝時間。
2.  **取樣窗口優化**：
    TI C28x 在 PHA=1 時，取樣點剛好落在資料位元的**正中央**（而非邊界），能有效降低因訊號抖動 (Jitter) 或波形畸變產生的誤讀風險。
3.  **Flash 相容性**：
    W25Q64JV 對輸入時序較寬容，接收 C28x 的 Mode 1 訊號時仍能穩定運作，但對 C28x 端的接收穩定性有質的提升。

---
**文件狀態**: 已驗證 (Validated on ASR5K V2 Hardware)
**最後更新**: 2026/05/11
