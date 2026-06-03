**【系統環境與開發目標】**
*   **MCU 平台**：TI C2000 系列 (TMS320F2838x) - CPU2。
*   **時脈參數**：SYSCLK = 200 MHz, LSPCLK = 50 MHz。
*   **開發目標**：在**尚無**真實 FSI 模組以及外部 ADC/DAC 硬體的過渡階段，使用 **McBSP-A 模擬標準 4-Wire SPI Master** (Clock Stop Mode)，並建立一條完全零 CPU 負擔 (0% Overhead) 的「全自動 DMA 收發管線」，以 100kHz 的控制週期進行回圈測試。
*   **模擬策略**：
    1. 使用全域記憶體變數 (`Simulated_FSI_RX_Data`, `Simulated_FSI_TX_Data`) 模擬尚未實作的 FSI 收發緩衝區。
    2. 透過外部跳線 (External Short) 直接將 McBSP 的 TX 腳位接回 RX 腳位，藉此模擬 DAC 指令直接被當作 ADC 採樣值收回來的行為。

請根據以下「3 大核心規範」，幫我撰寫並配置對應的 C 語言韌體程式碼 (基於 TI Driverlib)：

#### 規範 1：硬體實體短接與預設 GPIO PinMux 配置
根據官方手冊規定，McBSP 在 SPI 模擬模式 (Clock Stop Mode) 下絕對無法使用內部數位迴圈 (DLB)，必須依靠外部實體連線。**PinMux 的定義與初始化將全權交由 SysConfig 生成的 `board.c` 與 `board.h` 處理，本驅動層不需再自行定義腳位與執行 PinMux 初始化函式。**
請在硬體上確保以下 6 根腳位的實體跳線對接 (具體 GPIO 腳號請以 `board.h` 最新設定為準)：
1.  **時脈短接 (SCK)**：`myMCBSP0_MCLKX_PIN` 實體短接至 `myMCBSP0_MCLKR_PIN`。
2.  **片選短接 (CS)**：`myMCBSP0_MFSX_PIN` 實體短接至 `myMCBSP0_MFSR_PIN`。
3.  **資料短接 (MOSI -> MISO)**：`myMCBSP0_MDX_PIN` 實體短接至 `myMCBSP0_MDR_PIN`。

#### 規範 2：McBSP SPI Master 初始化與防呆啟動程序
C2000 F2838x 的 McBSP **沒有深度 FIFO**，絕對不可呼叫 `McBSP_enableFIFO()`。初始化請嚴格遵守以下流程：
1.  **強制重置**：設定前先將 TX, RX, SRG, Frame Sync 的狀態機強制 Reset。
2.  **SPI 參數**：16-bit 單字組傳輸，時脈無延遲 (CPHA=0)，上升緣發送 (CPOL=0)。
3.  **時脈除頻 (2.5MHz)**：目標產生 2.5MHz SPI 時脈，SRG 除頻器 (`clockSRGDivider`) 必須精準設為 **19** (公式：50MHz / (19+1) = 2.5MHz)。
4.  **作法儀式 (清除 XSYNCERR)**：剛上電時極易產生同步錯誤。啟動 SRG 後 $\rightarrow$ 延遲 8 個 NOP $\rightarrow$ 開啟 TX $\rightarrow$ 延遲 160 個 NOP 讓雜訊釋放 $\rightarrow$ 關閉 TX 以強制清除懸而未決的 `XSYNCERR` $\rightarrow$ 最後再乾淨地同時開啟 TX 與 RX。
5.  **注意**：不需要開啟內部 Frame Sync Logic (`FSGM=0`)，因為 SPI 的 CS 是由「資料寫入 DXR 暫存器」的動作由硬體自動觸發。

#### 規範 3：全硬體 DMA 管線設定 (無 CPU 中斷)
請完全不使用 CPU 輪詢或 ISR，透過以下兩個 DMA 頻道實現全自動化搬運 (皆為 16-bit Word 大小、1 Word/Burst、連續模式 `DMA_CFG_CONTINUOUS_ENABLE`)：
1.  **TX DMA (負責觸發 SPI 發送)**：
    *   **觸發源**：設定為 EPWM1 的 SOCA 事件 (`DMA_TRIGGER_EPWM1SOCA`)。
    *   **動作**：將 `Simulated_FSI_RX_Data` 變數的值搬運到 McBSP 的 `DXR1`。此動作會瞬間喚醒 McBSP 打出 SPI Clock 與 CS。
2.  **RX DMA (負責接收 SPI 資料)**：
    *   **觸發源**：設定為 McBSP-A 的接收就緒事件 (`DMA_TRIGGER_MCBSPAMREVT`)。
    *   **動作**：當 McBSP 接收完 16-bit 資料並產生硬體 Event 時，立刻將 `DRR1` 的資料搬運到 `Simulated_FSI_TX_Data` 變數中。

**【給 AI 的任務】**
請回覆「我已完全理解您目前的 100kHz DMA-McBSP 模擬管線需求」。接著，請直接輸出符合上述 3 大規範的：
1. `Init_McBSP_SPI_Master()` 初始化函式 (須包含清除 XSYNCERR 的防護)。
2. `Init_DMA_CompensationLoop()` DMA 頻道 1 與 2 的設定函式 (使用變數模擬 FSI)。

---

### 💡 後續開發建議：
把這份提示詞交給 AI 後，它生成的程式碼就能直接在您目前的板子上跑起來。您只要透過 CCS 的 Watch Window 動態修改 `Simulated_FSI_RX_Data` 的值，就能立刻看到 `Simulated_FSI_TX_Data` 跟著改變，藉此完美驗證「**PWM $\rightarrow$ TX DMA $\rightarrow$ SPI 實體線路 $\rightarrow$ RX DMA**」的全硬體 0% CPU 負載管線！等未來外接 IC 與 FSI 通訊到位，只需把 DMA 的記憶體位址換掉即可。