---
Category: Hardware & Peripherals
Status: Verified
Last Updated: 2026-05-11
Related Files: [SYS_Bringup_Guide.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/.agent/specs/knowledge/SYS_Bringup_Guide.md)
---

# ASR5K 控制核心：ADC 溫度監控技術指南


本文件詳細說明如何正確配置 ASR5K 控制核心的內部溫度感測器 (Internal Temperature Sensor)，並將其轉換為精確的攝氏溫度 (°C)。

## 1. 核心實作方式 (推薦)

ASR5K 專案**強制要求**使用 TI DriverLib 提供的校準函式。該函式會自動讀取晶片內部的 OTP (One-Time Programmable) 預留校準值，補償製造誤差。

### 程式碼範例
```c
// 標頭檔要求：adc.h
// 呼叫範例 (VREF = 3.0V)
g_hwTest.mcuTemp = ADC_getTemperatureC(u16_raw, 3.0f);
```

---

## 2. SysConfig 關鍵配置 (硬體驗證要點)

若要在軟體中取得正確的讀值，必須在 `pinmux.syscfg` 中進行以下關鍵設定，否則讀值會出現極大誤差或不穩定：

### A. 採樣窗口 (Sample Window) - 物理限制
- **要求值**：至少 **500ns** (在 200MHz 系統下約為 **100 SYSCLK**)。
- **原理**：內部溫度感測器具備較高的輸出阻抗。如果採樣時間太短 (例如預設的 15 SYSCLK)，ADC 的內部電容 (Chold) 無法充電至穩定電壓，會導致讀值大幅偏低（常見現象是讀到 1.2V 左右，換算後溫度極低）。

### B. 通道配置
- **ADC 模組**：ADCA
- **通道**：`ADC_CH_ADCIN13` (內部感測器固定連接至此)
- **SOC 觸發**：建議使用 `ADC_TRIGGER_SW_ONLY` (軟體觸發) 或與定時任務同步。

---

## 3. 校準原理：為什麼不能只用線性公式？

傳統的線性公式：
Temp = (Vobs - V25) / Slope + 25

### 為什麼這在 C2000 上不夠準？
1. **底噪偏差 (Offset Error)**：每顆晶片在生產時，25°C 的基準電壓 (V25) 會有微小差異。
2. **增益偏差 (Gain Error)**：溫度斜率 (Slope) 也會隨製程波動。

**`ADC_getTemperatureC` 的運作機制：**
該函式會讀取 `TSN_SLOPE` 與 `TSN_OFFSET` 兩個暫存器。這兩個值是在 TI 出廠測試時，將晶片放入精確溫箱後寫入的校準碼。使用此函式可以將誤差從 ±15°C 縮小到 **±3°C**。

---

## 4. 疑難排解與驗證清單 (Troubleshooting)

| 現象 | 可能原因 | 解決對策 |
| :--- | :--- | :--- |
| 溫度讀值固定在 -40°C 以下 | 採樣時間不足 | 將 SOC 的 Sample Window 增加至 100 以上。 |
| 溫度隨負載大幅波動 | VREF 不穩定 | 檢查硬體 VREFHI 腳位的濾波電容，並確認 `ADC_getTemperatureC` 的第二個參數與實際電壓吻合。 |
| 讀值比環境溫度高 10-20°C | 正常現象 | MCU 運行時內核 (Die) 溫度本來就高於環境，此為「自發熱」。 |

---

## 5. 參考文件
- [ASR5K 系統啟動指南](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/.agent/specs/knowledge/SYS_Bringup_Guide.md)
- TI F2838x Technical Reference Manual (TRM) - ADC Chapter
