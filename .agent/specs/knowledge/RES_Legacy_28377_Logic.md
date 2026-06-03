---
Category: Research & Concepts
Status: Legacy
Related Files: [RES_Measurement_Concept_QA.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/.agent/specs/knowledge/RES_Measurement_Concept_QA.md)
---

# 28377 舊版專案量測邏輯研究紀錄 (VO/IO)


本文件盤點了 `TRACE_ASR5000_F28377S` 專案中關於輸出電壓 (VO) 與電流 (IO) 的底層運算邏輯，作為 `28388` 遷移參考。

---

## 1. 硬體介面與通道定義
系統採用外部 16-bit ADC **ADS8353**，透過 `SPIA` 介面進行通訊。

| 功能名稱 | ADC 通道 | 實體意義 | 關鍵變數 (C代碼) |
| :--- | :--- | :--- | :--- |
| **MEAS VO** | ADS8353 CHA | 輸出電壓 (**LM-NM**) | `sC28CLA.ADS8353_CHA` |
| **MEAS IO** | ADS8353 CHB | 輸出電流 (**CS**) | `sC28CLA.ADS8353_CHB` |

---

## 2. 即時運算流程 (CLA_Task.cla)

在 CLA 的 Task 中，每個取樣週期會執行以下步驟：

### 2.1 原始值讀取與 Offset 校正
```c
// 讀取 SPIA 接收緩衝區 (ADS8353 32-CLK Mode)
CLA_to_CPU_Variables.ADS8353_DUMMY = SpiaRegs.SPIRXBUF;
sC28CLA.ADS8353_CHA = SpiaRegs.SPIRXBUF; // VO
sC28CLA.ADS8353_CHB = SpiaRegs.SPIRXBUF; // IO

// 套用校正偏置 (Offset)
float_buf1 = (float)sC28CLA.ADS8353_CHA + CPU_to_CLA_Variables.ADC_to_V_FB_Offset;
chb_with_offset = (float)sC28CLA.ADS8353_CHB + CPU_to_CLA_Variables.ADC_to_I_FB_Offset;
```

### 2.2 瞬時值換算 (Scale)
```c
// 根據極性乘上比例常數
if (float_buf1 >= 0)
    V_FB = float_buf1 * CPU_to_CLA_Variables.ADC_to_V_FB_Scale_P;
else
    V_FB = float_buf1 * CPU_to_CLA_Variables.ADC_to_V_FB_Scale_N;
```

### 2.3 數值累加 (Accumulation)
為了計算 RMS，CLA 會在一個週期內累加平方值：
*   `V_square_sum_shadow += (double)V_FB * (double)V_FB;`
*   `I_square_sum_shadow += (double)I_FB * (double)I_FB;`
*   `P_sum_shadow += (double)V_FB * (double)I_FB;` (計算有功功率)

---

## 3. 背景統計運算 (Meas_Module.c)

CPU 在背景任務中將 CLA 累加的結果轉換為顯示用的數值：

### 3.1 均方根 (RMS) 計算
```c
// 計算平均平方值
double_buf = CLA_to_CPU_Variables.V_square_sum / (double)CLA_to_CPU_Variables.Total_Sample_Points;

// 開根號得到 RMS
sGB.POWER_DISPLAY_VARIABLES.V_rms_display_shadow = (float)sqrt(double_buf);
```

### 3.2 交流成分過濾 (AC RMS)
若需移除直流成分，系統會先計算平均值 (Avg)，再利用勾股定理推導：
*   `V_avg = V_sum / Total_Samples;`
*   `AC_V_rms = sqrt(Total_RMS^2 - V_avg^2);`

---

## 4. 遷移至 28388 的建議點

1.  **ADC 改良**：`28388` CPU1 雖然有 12/16-bit 內部 ADC，但若要維持同等精度且不佔用 CPU 頻寬，應考慮沿用 `ADS8353` 或是使用 `LTC2353` (目前 CPU1 的專案已在使用)。
2.  **運算搬移**：舊版將平方累加放在 CLA。在 `28388` 中，如果使用 DMA 搬運 SPI 資料，可以考慮直接在 CPU1 的 FPU32 中進行塊運算 (Block Calculation)，效率會更高。
3.  **校正結構**：`ADC_to_V_FB_Scale_P/N` 的結構應保留，以解決硬體電路在正負半週不對稱的問題。
