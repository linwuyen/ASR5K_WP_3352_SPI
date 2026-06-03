# ASR5K 底層 HRPWM 與硬體參數配置移植說明文件

本文件旨在為工程師提供 ASR5K 逆變器控制底層中，高解析度 PWM (HRPWM) 與硬體參數配置的底層物理邏輯、暫存器設置以及安全防護機制說明。

---

## 一、 硬體參數配置與消滅除法設計 (`HwConfig.c` / `HwConfig.h`)

在 ASR5K 控制底層的即時性運算中，系統嚴格遵循 **ZERO_DIVISION (零除法)** 的物理安全規範。在控制中斷（每 10 微秒，即 100 kHz 執行一次）中，除法會消耗極高的 CPU 指令週期，對系統的即時性收斂帶來極大危害。

### 1. 物理比率參數
在 `HwConfig.c` 中，定義了將 ADC 轉換 tick 換算為物理數值的基本係數：
*   **f32VoutScale**: 電壓比例係數（電壓物理量 / ADC_Unit）
*   **f32IoutScale**: 電流比例係數（電流物理量 / ADC_Unit）

### 2. 乘法倒數設計 (Multiplication Inversion)
為了消滅除法，代碼在初始化函數 `initHwConfig()` 中執行了倒數預計算：
```c
sHwDerive.f32InvVoutScale = 1.0f / p->f32VoutScale;
```
*   **底層邏輯**：在隨後的控制迴路中，原先的除法換算 `voltage = ADC_Value / scale` 被完全轉換為乘法運算：`voltage = ADC_Value * f32InvVoutScale`。
*   **物理效果**：C28x CPU 執行乘法僅需 1 個時脈週期，比起除法的數十個週期，執行效率提升了近 20 倍。

---

## 二、 HRPWM 物理計數與解析度推演 (`initHRPWM.h`)

在 `initHRPWM.h` 中，系統定義了與 ePWM 動作限定器 (Action Qualifier) 對齊的核心時脈參數：

### 1. 半週期計數值 `PWM_CNTS` = 500 ticks
*   **時脈背景**：ePWM 運作時脈 `EPWMCLK` 為 `100 MHz` (每個 tick 為 10 ns)；PWM 載波頻率 `PWM_FREQ_HZ` 為 `100 kHz` (週期為 10,000 ns)。
*   **物理推導**：
    *   一個完整的 PWM 週期所對應的 tick 數為：10,000 ns / 10 ns = 1000 ticks。
    *   由於 PWM 配置為遞增/遞減 (Up-Down) 中心對齊模式，計數器從 0 遞增至頂部再遞減回 0。因此，半週期的計數頂值為 `1000 ticks / 2 = 500 ticks`。

### 2. 高解析度 Q8 格式週期 `HRPWM_CNTS` = 128,000
*   **硬體原理**：TI C2000 的 HRPWM 模組採用 Q8 小數點格式。它將 1 個標準的 10 ns tick，在硬體上進一步細分為 256 個微小的步長（每步約 39 ps 脈寬）。
*   **計算方式**：`HRPWM_CNTS` = `500 * 256 = 128,000` (即 `PWM_CNTS << 8`)，用於向 HRPWM 週期暫存器提供極高精度的時序配置。

---

## 三、 高精準度死區與安全限制 (`SAFE_DB_COARSE`)

死區時間（Deadband）用於防止橋式逆變電路中，上橋與下橋功率管同時導通而引起的電源直通短路故障。

### 1. 死區 Tick 數 `PWM_DB_CNTS` = 50 ticks
*   系統死區時間限制 `T_DB_NSEC` 為 `500 ns`。
*   500 ns / 10 ns per tick = 50 ticks。

### 2. 安全邊界防呆設計 `SAFE_DB_COARSE(v)`
```c
#define SAFE_DB_COARSE(v)     CLAMP_U16((v), 3u, PWM_CNTS)
```
*   **下限限制 3u (30 ns)**：在物理上，如果死區時間低於 30 ns，由於功率開關管的固有物理關斷延遲，極易發生上下橋微直通。因此，系統底層強制不允許死區低於 3 ticks。
*   **上限限制 `PWM_CNTS` (500 ticks)**：死區時間最多不能超過半個週期（5,000 ns），否則 PWM 將完全沒有可輸出的佔空比。
*   此設計能確保在調試時，即使誤輸入了錯誤的死區數值，硬體也不會因為直通而燒毀。

---

## 四、 物理與邏輯雙重安全鎖定與釋放機制

這是在移植代碼中，GPIO 145 (MEAS_CNV) 不會動的根本原因所在。

### 1. 安全復位與強制鎖死 `RST_HRPWMAB_DUTY`
```c
#define RST_HRPWMAB_DUTY(base)                      \
    do {                                            \
        uint16_t _db = SAFE_DB_COARSE(DB_COARSE_HALF_PERIOD); \
        SET_HRCMPA((HRPWM_CNTS >> 1), (base));      \
        SET_DBRED_COARSE(_db, (base));              \
        SET_DBFED_COARSE(_db, (base));              \
        EPWM_forceTripZoneEvent(base, EPWM_TZ_FORCE_EVENT_OST); \
        FG_RST(_CSTAT_OUTPUT_ON, sDrv.fgStatus); \
    } while (0)
```
該巨集在初始化與復位時執行以下多重防護：
*   **死區防護**：把死區時間強行拉至半週期最大安全值。
*   **硬體物理鎖死**：呼叫 `EPWM_forceTripZoneEvent(..., EPWM_TZ_FORCE_EVENT_OST)`，強行觸發 ePWM 的 **One-Shot (OST) Trip-Zone** 保護。這是一個純硬體的超高速安全機制，它直接繞過 CPU，在幾十奈秒內強行將 GPIO 145 卡死在低電位，禁止任何物理波形噴出，確保開機安全。
*   **軟體狀態復位**：在驅動狀態字 `sDrv.fgStatus` 中，清除 `_CSTAT_OUTPUT_ON` 標誌。

### 2. 解鎖與恢復輸出 `SET_HRPWMAB_DUTY`
原先的移植代碼在初始化時執行了 `RST_HRPWMAB_DUTY`，把引腳死死鎖住。但在後續初始化全部完成、開始正常運行時，**卻遺漏了解鎖動作**。

要解除硬體的強制 One-Shot 鎖定，軟體必須調用開鎖巨集：
```c
#define SET_HRPWMAB_DUTY(cmp, base)                 \
    do {                                            \
        uint16_t _db = SAFE_DB_COARSE(DB_COARSE_NORMAL); \
        SET_HRCMPA((cmp), (base));                  \
        SET_DBRED_COARSE(_db, (base));              \
        SET_DBFED_COARSE(_db, (base));              \
        EPWM_clearTripZoneFlag(base, EPWM_TZ_FLAG_OST); \
        FG_SET(_CSTAT_OUTPUT_ON, sDrv.fgStatus); \
    } while (0)
```
*   **關鍵開鎖鑰匙**：`EPWM_clearTripZoneFlag(base, EPWM_TZ_FLAG_OST)` 清除了 One-Shot 鎖定狀態，將死區還原為正常的 50 ticks (500 ns)，使 GPIO 145 恢復正常的 HRPWM 波形輸出。

---

## 五、 移植調試下步行動

為了解鎖 `MEAS_CNV` (GPIO 145) 的啟動轉換脈寬輸出，必須在初始化流程的最後（開啟 `TBCLKSYNC` 之後），手動為其執行一次 Trip-Zone 清除。

請在 CPU1 實體專案 [main.c](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_28384/ASR5K_F28384D_CPU1/main.c) 第 112 行之後，寫入解鎖補丁：
```c
    // Clear the One-Shot Trip-Zone event forced during initialization to unlock GPIO 145 (MEAS_CNV)
    EPWM_clearTripZoneFlag(MEAS_CNV_BASE, EPWM_TZ_FLAG_OST);
```
解鎖後，GPIO 145 將開始輸出穩定的 100 kHz 啟動轉換窄脈寬，外部 ADC LTC2353-16 即可正常接收 CNV 訊號並啟動精密轉換。
