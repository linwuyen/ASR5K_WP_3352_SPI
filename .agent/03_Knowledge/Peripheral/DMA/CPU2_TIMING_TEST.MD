# ASR5K V2 CPU2 — DMA 時序與測試計畫

> 文件版本：v1.0
> 建立日期：2026-04-23
> 作者：roger_lin
> 適用架構：5-CH 整包複製 + 覆寫（Copy then Fill）

---

## 一、全域時基（EPWM1，100 kHz）

CPU2 使用 **EPWM1** 作為 DMA 觸發時基，週期 Ts = 10µs。

```
EPWM1 Up-Down 模式，PWM_CNTS = 1000 (200MHz / 100kHz / 2)

T = 0.00µs  CTR = 0    （底部）  SOCA  → 觸發 Wave 1 DMA
T = 5.20µs  CTR = 520  （上升段）SOCB  → 觸發 Wave 2 DMA  [待修正 ISS-001]
T = 8.00µs  CTR = 400  （下降段）EPWM6 → 觸發 Wave 3 收割 [待修正 ISS-001]
T = 10.00µs 週期結束
```

---

## 二、設計目標時序（規格）

| 時間點 | 觸發事件 | DMA 動作 |
|---|---|---|
| T = 0.00µs | SOCA（CTR=0） | Wave 1：CH1 整包複製 Fsi_M → Fsi_S1（16W）+ CH2 注入 Local_S1（3W） |
| T = 5.20µs | SOCB（CTR=520↑） | Wave 2：CH3 整包複製 Fsi_S1 → Fsi_S2（16W）+ CH4 注入 Local_S2（3W） |
| T = 8.00µs | EPWM6 SOCA | Wave 3：CH5 收割 Fsi_S2 → Fsi_M（16W，閉環） |
| T = 10.00µs | 週期結束 | 下一週期開始 |

---

## 三、現行程式碼時序（實際，含已知問題）

| 通道 | 現行觸發源 | 正確觸發源 | 狀態 |
|---|---|---|---|
| CH1 | `EPWM1SOCA` | `EPWM1SOCA` | ✓ 正確 |
| CH2 | `EPWM1SOCA` | `EPWM1SOCA` | ✓ 正確 |
| CH3 | `EPWM1SOCB` | `EPWM1SOCB` | ✓ 正確 |
| CH4 | `EPWM1SOCB` | `EPWM1SOCB` | ✓ 正確 |
| CH5 | `EPWM6SOCA` | `EPWM6SOCA` | ✓ 正確 |

---

## 四、EPWM 比較值設定（修正後目標值）

| 比較器 | 計數值 | 對應時間 | 用途 |
|---|---|---|---|
| SOCA（CTR=0） | 0 | T=0.00µs | Wave 1 觸發 |
| CMPB（上升段） | 520 | T=5.20µs | Wave 2 觸發（SOCB） |
| EPWM6 相位偏移 | 800 counts | T=8.00µs | Wave 3 收割觸發 |

---

## 五、效能監控

ISR 從 T=8.00µs 開始，必須在 T=10.00µs 前完成，可用窗口 **2.00µs**。
使用 GPIO 翻轉配合示波器觀察 ISR 執行時間。

---

## 六、測試前置條件

### 6.1 測試環境

| 項目 | 規格 |
|---|---|
| 硬體 | TMS320F28388D（ASR5K 控制板） |
| 開發工具 | CCS + C2000Ware |
| CPU 頻率 | 200 MHz |
| PWM 頻率 | 100 kHz（週期 10µs） |
| 測試模式 | 單核模擬（不需外部硬體） |

---

## 七、測試用例

### TC-01：Wave 1 資料完整性（Fsi_S1 驗證）

**目的**：驗證 CH1 整包複製 + CH2 S1 遙測注入正確。

**前置設定**：

```c
Fsi_M.Master_Payload.L1_Ref  = 0x0111;
Fsi_M.Master_Payload.CC_DA   = 0x0444;
Local_S1.Vout       = 0x1234;
Local_S1.Iout       = 0x5678;
Local_S1.Status.all = 0xAAAA;
```

**驗證（等待 1 個 PWM 週期後）**：

| 觀察變數 | 預期值 | 驗證項目 |
|---|---|---|
| `Fsi_S1.Master_Payload.L1_Ref` | `0x0111` | CH1 整包複製正確 |
| `Fsi_S1.Master_Payload.CC_DA` | `0x0444` | CH1 整包複製正確 |
| `Fsi_S1.Slave_Payload[0].Vout` | `0x1234` | CH2 S1 遙測注入正確 |
| `Fsi_S1.Slave_Payload[0].Iout` | `0x5678` | CH2 S1 遙測注入正確 |
| `Fsi_S1.Slave_Payload[0].Status.all` | `0xAAAA` | CH2 S1 遙測注入正確 |

---

### TC-02：Wave 2 資料完整性（Fsi_S2 驗證）

**目的**：驗證 CH3 整包複製（含 S1 資料轉發）+ CH4 S2 遙測注入正確。

**前置設定**（在 TC-01 基礎上）：

```c
Local_S2.Vout       = 0xABCD;
Local_S2.Iout       = 0xEF01;
Local_S2.Status.all = 0x5555;
```

**驗證**：

| 觀察變數 | 預期值 | 驗證項目 |
|---|---|---|
| `Fsi_S2.Master_Payload.L1_Ref` | `0x0111` | CH3 Master 指令轉發正確 |
| `Fsi_S2.Slave_Payload[0].Vout` | `0x1234` | CH3 S1 遙測轉發正確 |
| `Fsi_S2.Slave_Payload[1].Vout` | `0xABCD` | CH4 S2 遙測注入正確 |
| `Fsi_S2.Slave_Payload[1].Iout` | `0xEF01` | CH4 S2 遙測注入正確 |
| `Fsi_S2.Slave_Payload[1].Status.all` | `0x5555` | CH4 S2 遙測注入正確 |

---

### TC-03：Wave 3 閉環收割（Fsi_M 更新驗證）

**目的**：驗證 CH5 在 T=8.00µs 將最終封包收割回 Fsi_M。

**驗證**：

| 觀察變數 | 預期值 | 驗證項目 |
|---|---|---|
| `Fsi_M.Slave_Payload[0].Vout` | `0x1234` | CH5 收割 S1 遙測成功 |
| `Fsi_M.Slave_Payload[1].Vout` | `0xABCD` | CH5 收割 S2 遙測成功 |
| `Fsi_M.Master_Payload.L1_Ref` | `0x0111` | CH5 收割 Master 指令保留正確 |

---

### TC-04：DMA 優先級仲裁順序測試

**目的**：驗證 CH1 先於 CH2 執行（整包複製先於遙測注入）。

**測試方法**：

1. 設定 `Fsi_M.Slave_Payload[0].Vout = 0xDEAD`（CH1 複製來源中的 S1 欄位）
2. 設定 `Local_S1.Vout = 0xBEEF`（CH2 注入來源）
3. 等待 1 個 PWM 週期

**驗證**：

| 觀察變數 | 預期值 | 說明 |
|---|---|---|
| `Fsi_S1.Slave_Payload[0].Vout` | `0xBEEF` | CH2 後執行，覆蓋了 CH1 複製的 `0xDEAD` |

> 若結果為 `0xDEAD`，表示 CH2 先於 CH1 執行，優先級設定錯誤。

---

### TC-05：位址穩定性長時測試

**目的**：驗證 WrapSize=1 確保 100 kHz 連續運行下位址不漂移。

**測試方法**：設定固定測試值，連續運行 10 秒（100 萬個 PWM 週期），每隔 1 秒記錄一次。

**驗證**：

| 時間點 | 觀察變數 | 通過標準 |
|---|---|---|
| T+1s | `Fsi_M.Slave_Payload[0].Vout` | 恆為 `0x1234` |
| T+5s | `Fsi_M.Slave_Payload[0].Vout` | 恆為 `0x1234` |
| T+10s | `Fsi_M.Slave_Payload[0].Vout` | 恆為 `0x1234` |

---

### TC-06：靜默效能測試（Silence Check）

**目的**：驗證 T=0.00µs ~ T=8.00µs 之間 CPU 完全不介入，無 Context Switch 抖動。

**測試方法**：使用示波器監測 ISR Profiling GPIO（如 GPIO94 STAT_CPU2）。

**驗證**：

| 時間區間 | 預期 GPIO 狀態 | 說明 |
|---|---|---|
| T=0.00µs ~ T=8.00µs | 無動作 | DMA 全自動，CPU 沉睡 |
| T=8.00µs | 出現唯一脈衝 | ISR 喚醒點 |

---

## 八、測試結果記錄表

| 測試編號 | 測試項目 | 通過標準 | 結果 | 備註 |
|---|---|---|---|---|
| TC-01 | Wave 1 Fsi_S1 完整性 | 所有欄位值與預期一致 | [ ] | |
| TC-02 | Wave 2 Fsi_S2 完整性 | 所有欄位值與預期一致 | [ ] | |
| TC-03 | Wave 3 Fsi_M 閉環收割 | Fsi_M 包含完整 M+S1+S2 資料 | [ ] | |
| TC-04 | DMA 優先級仲裁 | CH1 > CH2 執行順序正確 | [ ] | |
| TC-05 | 位址穩定性（10 秒） | 100 萬週期後無漂移 | [ ] | |
| TC-06 | 靜默效能 | 0~8µs 無 GPIO 動作 | [ ] | |

---

## 九、修改紀錄

| 修改人 | 日期 | 版本 | 修改內容 |
|---|---|---|---|
| roger_lin | 2026-04-23 | v1.0 | 合併 時序線路.MD + 測試計畫.MD，統一為時序與測試文件 |
