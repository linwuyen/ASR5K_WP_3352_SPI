# SPIC_module 與 F28384D 整合硬體測試計畫書

本計畫書旨在引導工程師針對 ASR5K 控制板上的 **SPIC_module**（包含外部精密雙通道 ADC **LTC2353-16** 與精密 DAC **AD5543**）與 TI **F28384D (CPU1)** 微控制器進行完整的硬體驗證與整合測試。

本計畫書特別針對傳輸時序對齊、1-bit 位元偏移偏斜 (Bit Shift)、自環 (Loopback) 測試、以及中斷即時控制收斂，建立了一套「自底向上 (Bottom-Up)」的標準化硬體調試與驗證脈絡。

---

## 一、 測試背景與硬體映射說明

在 ASR5K 逆變器控制底層中，高精密反饋信號的同步讀取與動態環路控制指令的寫入，高度依賴於 SPIC 的串列 Daisychain 高速通訊鏈路。為保證信號完整性與系統即時性，相關硬體引腳與外設模組的分配如下表所示：

### 1. 物理引腳與外設映射表

| 信號名稱 | 功能說明 | C2000 GPIO 引腳 | 主控核心 / 歸屬關係 | 物理電氣特性與備註 |
| :--- | :--- | :--- | :--- | :--- |
| **SPIC_SIMO** | SPI 主出從入數據線 | GPIO 100 | CPU1 (SPIC) | 輸出給 DAC (AD5543) 與 ADC (LTC2353) 的配置數據 |
| **SPIC_SOMI** | SPI 主入從出數據線 | GPIO 101 | CPU1 (SPIC) | 接收來自 ADC (LTC2353) 通道 0/1 的 24-bit 數據鏈 |
| **SPIC_CLK** | SPI 物理時脈信號線 | GPIO 102 | CPU1 (SPIC) | 10 MHz 高速通訊時脈 |
| **SPIC_STEN** | SPI 片選使能信號 (CS) | GPIO 103 | CPU1 (SPIC) | 主控端拉低以啟動 3-word (48-bit) 數據訊號鏈傳輸 |
| **MEAS_CNV** | ADC 啟動轉換信號 | GPIO 145 | CPU1 (HRPWM1A) | 由 HRPWM1A 高精度輸出窄脈衝，啟動外部 ADC 物理轉換 |
| **EN_SDI** | 門極致能信號 (Gate Enable) | -- | 由 M0 控制 | C2000 CPU1 無此 GPIO 控制權，完全由外部 M0 控制 |

### 2. 外部目標元件規格
*   **ADC**: LTC2353-16（16-bit 數據 + 8-bit 狀態位，2通道同步 SAR 取樣，最大物理轉換時間 tCONV = 1100 ns）
*   **DAC**: AD5543（16-bit 精密電壓輸出 DAC，與 ADC 串接於同一 SPI Daisychain 鏈路上）

---

## 二、 物理時序設計原理（核心中心對齊硬體掩護機制）

本系統在硬體時序設計上，巧妙地利用了 **「HRPWM 中心對齊（Up-Down）對稱計數差」** 來為外部 ADC (LTC2353-16) 提供 100% 零 CPU 開銷的物理轉換等待時間，這是確保 SPI 讀出資料絕無髒數據的底層基礎：

```
[HRPWM 計數器狀態]
       Down-CMPA (CNV上升沿)
         |
         v
       /---\
      /     \
     /       \
----+---------+-----> [時間軸]
             ^
             |
            Zero (SOCA發送 -> 內部ADC採樣與轉換 -> 觸發EOC中斷 -> 進入ISR打出SPI)
```

1.  **CNV 上升沿提前觸發（外部轉換開始）**：
    *   HRPWM1A 通道 A 配置為 `Down-CMPA` 時輸出拉高，`Up-CMPA` 時輸出拉低。
    *   這意指，`MEAS_CNV` (GPIO 145) 的**上升沿**（啟動外部 ADC 轉換）發生在計數器遞減至 CMPA 的時刻。
    *   在典型 50% 佔空比（CMPA = 250 ticks）下，該上升沿**比計數器中點 Zero 提前了約 2.5 microseconds**！外部 ADC 在此上升沿瞬間已啟動轉換。
2.  **SOCA 中點觸發（內部採樣與中斷啟動）**：
    *   內部的 `EPWM1_SOCA` 則是在計數器等於中點 **Zero** 時才觸發內部 ADCA 開始採樣。
    *   外部 LTC2353-16 的實質轉換時間 (tCONV) 需滿足 **`> 1.1 us (1100ns)`**。
    *   C2000 內部 ADC 設定其 **Acquisition Window (ACQPS = 200，約 1.2 us)**，使其總時間 **大於 1.1 us**，實施完美的硬體掩護。
3.  **中斷執行與 SPI 讀取**：
    *   內部 ADCA 轉換完成（**End of Conversion - EOC**）後觸發 CPU 中斷，CPU 進入 `handleMeasDdsIsr()` 並打出 SPI。此時外部 ADC 轉換早就 100%完成，數據已穩穩鎖存在輸出暫存器中。
4.  **安全底線限制（為什麼限制最小佔空比 10%）**：
    *   當佔空比縮小時，`Down-CMPA` 點會逼近 `Zero` 點。
    *   為了防止在極端小佔空比下，CNV 上升沿到中點 Zero 的時間過短（小於 1.1 us），系統在底層強制限制了 **最小脈寬為 500 ns（實質最小佔空比 10%）**！
    *   這樣即使在最極端的工作情況下，CNV 上升沿至中斷 SPI 讀取之間，依舊被硬體強制保留了大於 1.1 us 的物理轉換安全間隔，徹底根絕讀出髒數據的可能。

---

## 三、 測試項目一：GPIO 145 (MEAS_CNV) 物理觸發時序與 ADC 轉換延遲對齊驗證
*(硬體驗證的第一步：必須先確保 ADC 的物理轉換觸發源工作正常)*

### 1. 測試目的
驗證 HRPWM1A / EPWM1A (GPIO 145) 所產生的 ADC 高精度啟動轉換 (CNV) 訊號之頻率與脈寬是否符合設計，且確認 CNV 脈衝與 SPIC_STEN (CS) 拉低之間保留了充足的轉換時間（LTC2353-16 的物理轉換時間 tCONV 需滿足 **`> 1.1 us / 1100ns`**），以防止在轉換未完成時即啟動 SPI 傳輸而讀出髒數據。

### 2. 測試步驟
1.  **引腳接線**：將示波器 Ch1 探針掛在 **GPIO 145 (MEAS_CNV)**，Ch2 探針掛在 **GPIO 103 (SPIC_STEN / CS)**。
2.  **時序觀測設定**：觸發源設為 Ch1 (MEAS_CNV) 的上升沿，水平時基設為 200 ns/div。
3.  **脈寬與頻率測量**：
    *   量測 GPIO 145 脈衝頻率，確認是否符合設計的控制環路頻率（如 100 kHz）。
    *   量測 GPIO 145 高電平脈寬，應大於 LTC2353 規格要求的最小寬度（如大於 20 ns）。
4.  **轉換延遲測量**：
    *   量測從 GPIO 145 的下降沿（代表 ADC 啟動轉換）至 GPIO 103 (CS) 拉低之間的物理時間差。
    *   確認該時間間隔必須大於 **1100 ns (1.1 us)**，確保 ADC 已完全完成 SAR 轉換並將資料鎖存至輸出暫存器。

### 3. 判定標準
*   **通過**：CNV 觸發頻率正確，脈寬滿足要求，且 CNV 與 CS 下降沿的間隔時間大於 1100 ns，沒有時序重疊。
*   **失敗**：時序存在重疊，或 CNV 與 CS 間隔小於 1100 ns，導致 ADC 在轉換期間被強制讀取，引起 Expressions 中通道資料異常抖動。

---

## 四、 測試項目二：SPI 物理層 Clock 模式與 SDO 邊緣採樣驗證
*(針對讀回之 Channel ID 皆為 1 的 1-bit 位元左移偏斜 Bug 進行調試)*

### 1. 測試目的
驗證 SPIC 在高速 10 MHz 運作下，CLK 與 SOMI (SDO) 訊號線的建立/保持時間 (Setup/Hold Time) 是否充足，排除採樣點過於貼近訊號跳變沿所導致 of 1-bit 偏移。

### 2. 測試步驟
1.  **引腳觀測**：將示波器 Channel 1 探針掛在 **GPIO 102 (SPIC_CLK)**，Channel 2 探針掛在 **GPIO 101 (SPIC_SOMI)**。
2.  **時序觸發設定**：示波器觸發源設為 Channel 1 (CLK)，觸發電位 1.65 V，水平時基設為 50 ns/div。
3.  **模式 A (預設模式) 觀測**：
    *   在 `SPI_PROT_POL0PHA0` (Mode 0，時脈空閒為低，在上升沿鎖存輸入) 下，觀測在 CLK 的上升沿時，SOMI (SDO) 訊號線 of 電位是否已經完全穩定。
    *   檢查 Status Byte 的解析結果，觀察 `g_sMeasDds.u16AdcCh0Id` 與 `g_sMeasDds.u16AdcCh1Id` 是否皆被誤讀為 1。
4.  **模式 B (優化模式) 調整**：
    *   在 `board.c` 中將 SPIC 的配置更改為 **`SPI_PROT_POL0PHA1` (Mode 1)** 或 **`SPI_PROT_POL1PHA1` (Mode 3)**。
    *   *時序對齊目的*：將 Master 採樣邊緣移至 SDO 的穩定區（下降沿），確保避開 LTC2353 SDO 移出時的過渡狀態。
    *   重新觀察 `g_sMeasDds.u16AdcCh0Id` (應為 0) 與 `g_sMeasDds.u16AdcCh1Id` (應為 1)。

### 3. 判定標準
*   **通過**：在調整為適宜模式後，Ch0 ID 讀出為 `0`，Ch1 ID 讀出為 `1`，且讀出的模擬電壓與輸入信號成線性對應。
*   **失敗**：通道 ID 讀出仍皆為 1，或資料出現隨機跳變，代表位元偏移未被消除或線路反射嚴重。

---

## 五、 測試項目三：LTC2353 配置字寫入與 SoftSpan 切換驗證
*(驗證 SDI 下行鏈路與外部元件解碼狀態)*

### 1. 測試目的
驗證 F28384D 每週期發送的配置字 `0xFC00U` (LTC2353_CONFIG_TX) 是否被 ADC 成功解碼，並正確切換模擬通道的 SoftSpan 量程 (±10.24 V)。

### 2. 測試步驟
1.  **模擬信號輸入**：使用精密信號源 (Signal Generator) 對外部 ADC Channel 0 輸入固定直流電壓（例如 +5.000 V）。
2.  **變數監控**：在 CCS Expressions 視窗中，監測 `g_sMeasDds.f32AdcCh0V` 與 `g_sMeasDds.u16AdcCh0Ss` 的數值。
3.  **配置字切換測試**：
    *   **測試點 A**：發送配置字 `0xFC00U` (SoftSpan = `111`，量程 ±10.24 V)。
        *   預期 `g_sMeasDds.u16AdcCh0Ss` 應讀回 `7` (二進制 `111`)。
        *   預期 `g_sMeasDds.f32AdcCh0V` 應顯示近乎精確的 `5.00V`，誤差小於 ±5 mV。
    *   **測試點 B**：嘗試暫時修改配置字為 `0xF400U` (SoftSpan = `101`，量程 ±5.12 V)。
        *   預期 `g_sMeasDds.u16AdcCh0Ss` 應讀回 `5` (二進制 `101`)。

### 3. 判定標準
*   **通過**：`u16AdcCh0Ss` 能隨發送配置字的改變而準確跳變，且轉換電壓的換算縮放係數完全符合量程定義。
*   **失敗**：量程代碼維持全 1 (`7`) 或全 0 且無法更改，表示外部 ADC 無法接收配置字（可能 SDI 線路不通，或 `EN_SDI` 邏輯狀態不對）。

---

## 六、 測試項目四：Loopback (自環) 測試與 Production (實體) 模式切換驗證
*(確保代碼具備優秀的測試隔離與硬體無縫切換能力)*

### 1. 測試目的
驗證代碼中的測試隔離設計，確保在自環測試（Loopback）下算法迴路能 safe 閉環，且在轉為實體運作時能無縫對接外部硬體。

### 2. 測試步驟
1.  **啟用 Loopback 測試**：
    *   在 `meas_dds_module.h` 中開啟自環巨集定義：
        ```c
        #define SPI_LOOPBACK_TEST
        ```
    *   重新編譯並燒錄代碼。此時，中斷發送的測試字為 `0x1234U`, `0x00AAU`, `0xBB00U`。
    *   **預期行為**：`g_u16SpiRxBuf[0]`, `[1]`, `[2]` 應在 Expressions 視窗中完全等於傳送值 `0x1234U`, `0x00AAU`, `0xBB00U`，且 `u16SpiTimeoutCnt` 為 0。
2.  **還原 Production 模式**：
    *   註解掉 `#define SPI_LOOPBACK_TEST`。
    *   重新編譯燒錄，接回外部測試板。
    *   **預期行為**：SPI 自環停用，讀值恢復為外部實體 ADC 的即時反饋。

### 3. 判定標準
*   **通過**：Loopback 模式下讀寫字 100% 相同；關閉後能立刻讀取實體類比電壓。

---

## 七、 測試項目五：中斷執行開銷與系統即時性收斂測試
*(系統即時控制的終極保障驗證)*

### 1. 測試目的
驗證在中斷迴路中，執行外部 SPI 複合讀寫（3-word 傳輸）的 CPU 時間開銷，確保控制迴路在 10 us (100 kHz) 的週期內有足夠的即時性收斂裕度。

### 2. 測試步驟
1.  啟用 `isr.c` 中的計時測量代碼（使用內部 CPUTimer 監控計數差值）：
    ```c
    startTimerMeasure(&sDrv.tpIsrCost);
    handleMeasDdsIsr();
    stopTimerMeasure(&sDrv.tpIsrCost);
    ```
2.  在 CCS Expressions 中讀取 `sDrv.tpIsrCost` 與 `sDrv.tpIsrLength` 所記錄的運行時間。
3.  **IO 翻轉物理驗證 (嚴格交叉驗證)**：
    *   在 `handleMeasDdsIsr()` 入口處，將一個測試 GPIO 翻轉為 **HIGH**。
    *   在 `handleMeasDdsIsr()` 出口處，將該測試 GPIO 翻轉為 **LOW**。
    *   使用示波器測量該 GPIO 高電平脈寬，直接取得最真實的 ISR 物理執行時間。

### 3. 判定標準
*   **通過**：中斷總執行時間（含 SPI 輪詢等待）小於 **`5.5 microseconds`**，即時性餘裕大於 45%。
*   **失敗**：ISR 執行時間大於 7.5 us，或輪詢超時頻繁發生，導致即時控制任務溢出 (Overrun)。
