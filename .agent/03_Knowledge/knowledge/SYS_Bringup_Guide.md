---
Category: System & Architecture
Status: Verified
Related Files: [README.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/.agent/specs/knowledge/README.md)
---

# ASR5K 控制卡系統啟動與硬體驗證指南


## 1. 系統啟動序列 (單一真理來源)

ASR5K 韌體遵循「SysConfig 驅動」架構。所有硬體配置必須在 `pinmux.syscfg` 中定義，並透過 `Board_init()` 進行初始化。

### 啟動路徑：
1. **上電復位 (POR)**：硬體從 Flash 啟動。
2. **Device_init()**：配置核心時脈 (PLL 200MHz)、看門狗與 LSPCLK。
3. **Board_init()**：
   - **嚴禁**在 `main.c` 中手動使用 `EALLOW/EDIS` 寫入暫存器。
   - 所有週邊設定 (ADC, PWM, FSI, GPIO) 均由 SysConfig 生成的代碼在此處處理。
4. **HwVerification_Init()**：此函式目前已棄用/清空，因為所有設定已遷移至 `Board_init()`。
5. **主迴圈 (Main Loop)**：執行非阻塞任務 (`HwVerification_Update`, `Modbus_Update`)。

---

## 2. ADC 與溫度監控

### 經過校準的溫度換算
MCU 內部的溫度感測器在不同晶片之間並非完全線性。TI 提供了存儲在 OTP 中的工廠校準數據。

- **週邊**：ADCA (ADCIN13, SOC1)
- **函式庫呼叫**：`ADC_getTemperatureC(uint16_t adcSample, float32_t vref)`
- **約束條件**：
  - ASR5K 控制卡的 VREF 假設為 **3.0V**。
  - 採樣窗口 (Sample Window) 必須至少為 **500ns** (100 個 SYSCLK 週期)，以補償內部感測器的高阻抗。
- **原理**：此方法會從晶片的修整暫存器 (Trim Registers) 中獲取 `TSN_SLOPE` 與 `TSN_OFFSET`，確保精度在 ±3°C 內，無需人工手動校準。

---

## 3. FSI 菊花鏈驗證 (FSI Daisy Chain)

FSI (高速串列介面) 使用基於狀態機的環路進行測試。

### 驗證步驟：
1. **初始化**：透過 `Board_init()` 執行 `FSI_performTxInitialization` 與 `FSI_performRxInitialization`。
2. **同步序列**：在首次使用前執行 `FSI_executeTxFlushSequence` 以同步接收端位元流。
3. **狀態機 (FSM)**：
   - `FSI_STATE_IDLE`：每 100 萬次迴圈觸發一次傳輸。
   - `FSI_STATE_WAIT_RX`：輪詢 `FSI_getRxEventStatus`，不阻塞 CPU。
4. **API 使用**：
   - `FSI_writeTxBuffer(base, data, length, offset)`：寫入循環緩衝區。
   - `FSI_readRxBuffer(base, data, length, offset)`：從循環緩衝區讀取。
   - 成功接收後務必使用 `FSI_clearRxEvents` 清除旗標，以允許下次觸發。

---

## 4. SDRAM (EMIF1) 啟動

外部 SDRAM 對於大數據緩衝區與控制演算法至關重要。

- **介面**：EMIF1 (CS0)
- **時脈**：EMIF_CLK = SYSCLK / 2 (100MHz)。
- **配置**：由 SysConfig 中的 EMIF1 模組管理。
- **驗證方式**：
  - 檢查 `EMIF_A[0:12]` 與 `EMIF_D[0:15]` 的 PinMux。
  - 在位址 `0x80000000` 執行 Walking-bit 測試或簡單的記憶體讀寫檢查。
  - **EALLOW 警告**：EMIF 保護暫存器屬於安全關鍵區域，任何手動修改必須包覆在 `EALLOW/EDIS` 中。

---

## 5. 架構約束提醒 (Architectural Reminders)

- **非阻塞 (Non-blocking)**：所有硬體檢查必須使用狀態機 (FSM)。嚴禁使用 `while()` 迴圈等待旗標。
- **禁止除法**：在控制迴圈中，使用 `__divf32()` 或乘法逆元進行除法運算。
- **僅限 ASCII**：代碼與註解必須使用英文 (ASCII)，以防止 CCS (MS950/UTF-8) 與 Git 之間的編碼衝突。
- **匈牙利命名法**：變數請使用 `u16_`, `f32_`, `b_` 等前綴。
