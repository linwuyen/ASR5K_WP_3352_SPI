# ASR5K 系統外設（PWM, ADC, DAC）設置與高速時序對齊分析報告

本報告針對 ASR5K 專案韌體中，微控制器 (TI F2838x) 內部外設 (**ePWM / HRPWM**, **ADC**, **DAC**) 的底層暫存器設定、採樣時序對齊以及外部高速複合 SPI 串列傳輸（**LTC2353-16 ADC** 與 **AD5543 DAC**）之時序邏輯進行深入剖析。

---

## 一、 ePWM / HRPWM 高解析度脈寬調變配置

系統採用 C2000 內部高解析度 HRPWM 模組，配置為互補式中心對齊對稱波形，專職控制逆變功率元件的開關與硬體中斷基準。

### 1. 時脈與基本物理參數
*   **系統主頻 (`SYSCLK`)**: `200 MHz` (單一時脈週期 T_SYSCLK = 5 ns)
*   **ePWM 外設時脈 (`EPWMCLK`)**: `100 MHz` (由系統時脈二分頻所得，T_EPWMCLK = 10 ns)
*   **PWM 載波頻率 (`PWM_FREQ_HZ`)**: `100 kHz` (對應總開關週期 T_PERIOD = 10,000 ns)
*   **計數器模式**: `EPWM_COUNTER_MODE_UP_DOWN` (遞增/遞減計數器模式，用於產生中心對齊波形)
*   **半週期計數值 (`PWM_CNTS`)**: 500 ticks
    PWM_CNTS = (EPWMCLK_FREQ / PWM_FREQ_HZ) >> 1 = (100 MHz / 100 kHz) * 0.5 = 500 ticks
*   **高解析度週期計數值 (`HRPWM_CNTS`)**: 128,000 (採用 Q8 格式表示：500 * 256)

### 2. 波形觸發與動作限定器 (Action Qualifier) 設定
在 HRPWM 初始化 (`initHRPWMxAB`) 中，對通道 A 的動作限定器進行對稱配置：
*   **遞增計數且等於比較值 (Up-CMPA)**: 輸出拉至 **LOW**。
*   **遞減計數且等於比較值 (Down-CMPA)**: 輸出拉至 **HIGH**。
*   *時序特徵*: 以時計數器等於零點 (Zero) 為對稱中心，CMPA 決定了脈寬的開啟/關閉邊緣，產生典型的對稱中心對齊 PWM。

### 3. 死區時間與極性控制 (Deadband Submodule)
*   **物理死區時間 (`T_BUCK_PWM_DEADBAND_NSEC`)**: `500 ns`
*   **死區時脈 tick 數 (`PWM_DB_CNTS`)**: 50 ticks
    PWM_DB_CNTS = (EPWMCLK_FREQ / 10^3) * (T_DB_NSEC / 10^6) = 10^5 * (500 / 10^6) = 50 ticks
*   **互補控制**:
    *   **FED (Falling Edge Delay)**: 極性設定為 `EPWM_DB_POLARITY_ACTIVE_LOW`。
    *   **模式**: RED 與 FED 雙邊延遲均啟用（Active High Complementary Mode），產生無重疊的上下橋臂驅動訊號。
    *   **硬體安全限制**: 軟體內置安全死區 macro `SAFE_DB_COARSE(v)`，強迫 coarse 死區值落於 [3, 500] ticks 區間內，絕不允許出現小於 30 ns 的危險死區。

### 4. 佔空比邊界與保護
*   **最小脈寬 (Min Duty)**: `500 ns` (`T_MIN_DUTY_NSEC`)
*   **實質最小佔空比 (`MIN_DUTY_PU`)**: `10%` (考慮了脈寬限制與死區時間)
    MIN_DUTY_PU = (T_MIN_DUTY_NSEC + T_DEADBAND_NSEC) / T_PERIOD_NSEC = (500 ns + 500 ns) / 10,000 ns = 0.10
*   **實質最大佔空比 (`MAX_DUTY_PU`)**: `90%` (`1.0 - MIN_DUTY_PU`)
*   **關閉狀態佔空比 (`SET_DUTY_OFF`)**: `5%` (`500 ns / 10,000 ns`)
*   **硬體保護 (Trip Zone)**: 當觸發 Trip-Zone (TZA/TZB) 時，EPWM 輸出會立刻被強拉至 **LOW**，且無法單靠軟體覆寫。

---

## 二、 內部 ADC 配置與採樣時序對齊

系統採用的內部 ADC 系統主要配合 PWM 的對稱中點進行高精準度電流與電壓採樣。

### 1. 基礎暫存器設定
*   **運作時脈 (ADC Clock)**: `50 MHz` (SYSCLK 200 MHz 經 `ADC_CLK_DIV_4_0` 四分頻)。
*   **解析度與訊號模式**: `12-bit` 解析度、`Single-Ended` 單端訊號輸入。
*   **中斷產生時序**: 設置為 `ADC_PULSE_END_OF_ACQ_WIN`，即在**採樣窗口結束時**立即產生中斷脈衝，而不是轉換結束時。這極大地提高了中斷響應速度。

### 2. 精準中心採樣時序對齊 (ADC Trigger Timing)
*   **觸發源配置**: `ADCA_AIN1_SOC0` 配置觸發源為 **`ADC_TRIGGER_EPWM1_SOCA`**。
*   **觸發事件**: 在 `enableAdcTriggerbyPWM` 中，EPWM1 設為當 **`EPWM_SOC_TBCTR_ZERO`** (計數器等於 0) 時，發送 SOCA 訊號。
*   *時序對齊原理分析*: 
    由於 PWM 運行在 Up-Down 遞增/遞減模式下，計數器等於 0 的時刻剛好位於**波形的幾何對稱中心點**（此時功率管開關狀態不發生切換，電感電流正處於其平均值處）。
    在這個時刻觸發 ADC 採樣（中心採樣法），可以完美避開功率元件開關切換時所引起的 dv/dt 與 di/dt 高頻電磁耦合雜訊，獲取最乾淨、最真實的基礎物理量數據。
*   **採樣窗寬度 (Sample Window)**:
    *   高速採樣 (SOC0): 15 個 SYSCLK 週期（即 **`75 ns`**）。
    *   慢速採樣 (SOC1): 100 或 500 個 SYSCLK 週期，用於非即時性變數採樣。

---

## 三、 內部 DAC 外設配置

*   **DACA (AOUT1)**:
    *   **參考電壓**: `DAC_REF_ADC_VREFHI` (鎖定與內部 ADC 相同的精密外部參考電壓，確保兩者標置基準一致)。
    *   **載入機制**: `DAC_LOAD_SYSCLK` (基於系統時脈立即更新)。
    *   **狀態**: 影暫存器初始值設為 `0`，啟用緩衝輸出，並在中斷開啟前提供 `500 us` 的穩定延遲以防開機突波。
*   **DACB (AOUT2)**:
    *   **參考電壓**: `DAC_REF_VDAC`。

---

## 四、 外部精密 ADC (LTC2353) 與精密 DAC (AD5543) 複合傳輸與時序

逆變器底層在中斷服務程序中，透過 **SPIC** 外設與外部 ADC/DAC 高速複合晶片進行時序傳輸，實現每週期的即時狀態讀取與指令寫入。

### 1. 硬體連接與映射
*   **SPI 周邊**: **SPIC** (`SPIC_MEAS_DDS_BASE`，暫存器對齊，分配至 GPIO 100~103)。
*   **測試引腳**: `EN_SDI` 映射至 GPIO 4 (僅在開發板開發與模擬時使用)。

### 2. 複合時序傳輸序列 (48-bit High-Speed Frame)
在內部 `INT_ADCA_AIN1_1_ISR` 中斷被觸發後，中斷服務程序隨即執行 `handleMeasDdsIsr()`：
*   每次傳輸固定寫入 **3 個 16-bit 字 (共 48 bits)** 到 SPIC 的 TX FIFO：
    *   **Word 0**: LTC2353 配置字 (`LTC2353_CONFIG_TX = 0xFC00U`)。
    *   **Word 1**: 固定 Dummy Word (`0x0000U`)。
    *   **Word 2**: 來自控制環路動態計算出的 16-bit 精密 DAC 寫入指令值 `g_sMeasDds.u16DacOut` (直接寫入 AD5543)。
*   這 3 個字在 SPI 物理線路上同時發送。外部 ADC LTC2353 進行兩通道取樣讀回的同時，精密 DAC AD5543 亦在同一個 SPI 的 daisy chain 訊號鏈路上收到了寫入值。

### 3. SPI 接收 poll 迴圈與資料解析
*   為防範死鎖 (Deadlock)，設有安全計數器限制 `SPI_RX_POLL_MAX = 500` (在 200 MHz 下約耗時 2.5 微秒)。若超時，將遞增超時故障計數器。
*   讀回 3 個 16-bit 接收字：
    *   `g_u16SpiRxBuf[0]`: LTC2353 通道 0 的 16-bit ADC 資料（Q15 補數格式）。
    *   `g_u16SpiRxBuf[1]`: 
        *   高 8 位: 通道 0 的狀態位 (Status Byte)，包含通道標識 `u16AdcCh0Id` 與配置 SoftSpan 量程 `u16AdcCh0Ss`。
        *   低 8 位: 通道 1 的高 8 位資料。
    *   `g_u16SpiRxBuf[2]`:
        *   高 8 位: 通道 1 的低 8 位資料。與 Word 1 低八位進行拼接得到完整的通道 1 16-bit 資料：
            i16AdcCh1Raw = ((g_u16SpiRxBuf[1] & 0x00FF) << 8) | ((g_u16SpiRxBuf[2] >> 8) & 0x00FF)
        *   低 8 位: 通道 1 的狀態位 (Status Byte)，內含通道標識與量程配置。
*   **物理值換算 (零除法安全乘法)**:
    f32AdcCh0V = (float32_t)i16AdcCh0Raw * LTC2353_SCALE_V
    f32AdcCh1V = (float32_t)i16AdcCh1Raw * LTC2353_SCALE_V
    *(其中 LTC2353_SCALE_V = 10.24 V / 32768.0，透過乘法倒數確保實時控制中斷內無除法阻礙)*。

---

## 五、 控制時序閉環時序圖 (Timing Sequence Flow)

以下為逆變器單一調變週期內的控制、採樣與通訊時序邏輯流程：

1.  **PWM 計數器觸發 (TBCTR = 0)**:
    *   產生 `EPWM1_SOCA` 硬體觸發脈衝。
2.  **內部 ADCA 開始轉換 (`75 ns` 採樣 window)**:
    *   內部 ADC 執行高速轉換，完成後產生 `INT_ADCA_AIN1_1` 中斷。
3.  **進入 ISR 中斷服務程序 (`handleMeasDdsIsr`)**:
    *   清空 RX FIFO，避免讀取到上一週期的髒資料。
    *   寫入 3-word 傳輸序列到 SPIC TXBUF，推動外部 SPI 傳輸。
    *   外部 LTC2353 與精密 DAC AD5543 同步進行硬體讀取/寫入交互。
4.  **輪詢與解析**:
    *   軟體執行 2.5 us 內的有限輪詢，讀回 48-bit 複合資料。
    *   解析 Ch0 & Ch1 物理量，將結果儲存至 GSRAM，更新 `u32HeartBeat_CPU1` 心跳計數器。
    *   在中斷結束前，清除中斷標誌與 ACK 組別，準備下一開關週期的閉環處理。
