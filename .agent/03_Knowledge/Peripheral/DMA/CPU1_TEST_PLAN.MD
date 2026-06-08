# DMA 6-CH 3-Stage 精準拼裝測試計畫

> 文件版本：v1.0
> 建立日期：2026-04-22
> 作者：roger_lin
> 適用架構：6-CH 3-Stage Precision Assembly Daisy-Chain

---

## 一、測試目標

驗證 6-CH 3-Stage 精準拼裝架構在單核模擬環境下的正確性，確認：

1. **Wave 1（T=0.00µs，SOCA）**：CH1/CH2/CH3 依序完成 `Fsi_S1` 的精準拼裝
2. **Wave 2（T=5.20µs，SOCB）**：CH4/CH5 依序完成 `Fsi_S2` 的精準拼裝
3. **Wave 3（T=8.00µs，EPWM6 SOCA）**：CH6 將最終封包收割回 `Fsi_M`，完成閉環
4. **DMA 優先級仲裁**：同源觸發下 CH1 > CH2 > CH3、CH4 > CH5 的執行順序正確
5. **位址穩定性**：WrapSize=1 機制確保每次觸發後位址正確歸零，長時間運行無漂移

---

## 二、封包結構與 Word 映射

```
FSI_Frame (16 Words)
+--------+--------+--------+--------+--------+--------+--------+
| W0     | W1     | W2     | W3     | W4     | W5     | W6     |
| L1_Ref | L2_Ref | L3_Ref | CC_DA  | ID_Func| Data_H | Data_L |
+--------+--------+--------+--------+--------+--------+--------+
  Master_Payload (7W) — 由 Master 下發，CH1 負責搬運

+--------+--------+--------+
| W7     | W8     | W9     |
| Vout   | Iout   | Status |
+--------+--------+--------+
  Slave_Payload[0] (3W) — S1 本地遙測，CH2 負責注入

+--------+--------+--------+--------+--------+--------+
| W10    | W11    | W12    | W13    | W14    | W15    |
| Vout   | Iout   | Status | Rsv[0] | Rsv[1] | Rsv[2] |
+--------+--------+--------+--------+--------+--------+
  Slave_Payload[1] (3W) + Reserved (3W) — CH3 補齊 / CH5 注入 S2 遙測
```

---

## 三、測試前置條件

### 3.1 測試環境

| 項目 | 規格 |
|---|---|
| 硬體 | TMS320F28388D（ASR5K 控制板） |
| 開發工具 | Code Composer Studio (CCS) + C2000Ware |
| CPU 頻率 | 200 MHz |
| PWM 頻率 | 100 kHz（週期 10µs） |
| 測試模式 | 單核模擬（不需外部硬體） |

### 3.2 初始化確認

在執行測試前，確認以下初始化已完成：

```c
// main.c 中的呼叫順序
Board_init();          // SysConfig 初始化（含 EPWM1 基本設定）
initPWM();             // EPWM1 HRPWM 設定 + SOCA/SOCB/CMPC 比較值
DMA_Config();          // 6-CH DMA 設定
DMA_Start();           // 反向啟動所有通道
EINT; ERTM;            // 開啟全域中斷
```

### 3.3 EPWM 比較值確認

| 事件 | 比較器 | 計數值 | 對應時間 |
|---|---|---|---|
| SOCA | CTR=0（底部） | 0 | T=0.00µs |
| SOCB | CMPB（上升段） | 520 | T=5.20µs |
| EPWM6 SOCA | EPWM6 相位同步 | 800 counts offset | T=8.00µs |

---

## 四、測試用例

### TC-01：Wave 1 資料完整性測試（Fsi_S1 拼裝驗證）

**目的**：驗證 CH1/CH2/CH3 在 SOCA 觸發後，`Fsi_S1` 被正確拼裝。

**前置設定（在 main.c 或 asapTask 中設定已知值）**：

```c
// Master 指令
Fsi_M.Master_Payload.L1_Ref  = 0x0111;
Fsi_M.Master_Payload.L2_Ref  = 0x0222;
Fsi_M.Master_Payload.L3_Ref  = 0x0333;
Fsi_M.Master_Payload.CC_DA   = 0x0444;
Fsi_M.Master_Payload.ID_Func.all = 0x0555;
Fsi_M.Master_Payload.Data_H  = 0x0666;
Fsi_M.Master_Payload.Data_L  = 0x0777;

// S1 本地遙測
Local_S1.Vout       = 0x1234;
Local_S1.Iout       = 0x5678;
Local_S1.Status.all = 0xAAAA;

// Master 封包 Word 10~15 清零（CH3 來源）
Fsi_M.Slave_Payload[1].Vout       = 0x0000;
Fsi_M.Slave_Payload[1].Iout       = 0x0000;
Fsi_M.Slave_Payload[1].Status.all = 0x0000;
Fsi_M.Reserved[0] = 0x0000;
Fsi_M.Reserved[1] = 0x0000;
Fsi_M.Reserved[2] = 0x0000;
```

**驗證方法**：在 CCS Expressions 視窗觀察，等待至少 1 個 PWM 週期後：

| 觀察變數 | 預期值 | 驗證項目 |
|---|---|---|
| `Fsi_S1.Master_Payload.L1_Ref` | `0x0111` | CH1 Stage1 正確搬運 |
| `Fsi_S1.Master_Payload.L2_Ref` | `0x0222` | CH1 Stage1 正確搬運 |
| `Fsi_S1.Master_Payload.CC_DA` | `0x0444` | CH1 Stage1 正確搬運 |
| `Fsi_S1.Slave_Payload[0].Vout` | `0x1234` | CH2 Stage2 S1 注入正確 |
| `Fsi_S1.Slave_Payload[0].Iout` | `0x5678` | CH2 Stage2 S1 注入正確 |
| `Fsi_S1.Slave_Payload[0].Status.all` | `0xAAAA` | CH2 Stage2 S1 注入正確 |
| `Fsi_S1.Slave_Payload[1].Vout` | `0x0000` | CH3 Stage3 補齊正確 |
| `Fsi_S1.Reserved[0]` | `0x0000` | CH3 Stage3 補齊正確 |

**通過標準**：所有欄位值與預期完全一致。

---

### TC-02：Wave 2 資料完整性測試（Fsi_S2 拼裝驗證）

**目的**：驗證 CH4/CH5 在 SOCB 觸發後，`Fsi_S2` 被正確拼裝（包含 S1 資料的轉發）。

**前置設定**：在 TC-01 基礎上，額外設定 S2 本地遙測：

```c
Local_S2.Vout       = 0xABCD;
Local_S2.Iout       = 0xEF01;
Local_S2.Status.all = 0x5555;
```

**驗證方法**：等待至少 1 個 PWM 週期後：

| 觀察變數 | 預期值 | 驗證項目 |
|---|---|---|
| `Fsi_S2.Master_Payload.L1_Ref` | `0x0111` | CH4 Stage1 Master 指令轉發正確 |
| `Fsi_S2.Master_Payload.CC_DA` | `0x0444` | CH4 Stage1 Master 指令轉發正確 |
| `Fsi_S2.Slave_Payload[0].Vout` | `0x1234` | CH4 Stage1 S1 遙測轉發正確 |
| `Fsi_S2.Slave_Payload[0].Iout` | `0x5678` | CH4 Stage1 S1 遙測轉發正確 |
| `Fsi_S2.Slave_Payload[1].Vout` | `0xABCD` | CH5 Stage2 S2 注入正確 |
| `Fsi_S2.Slave_Payload[1].Iout` | `0xEF01` | CH5 Stage2 S2 注入正確 |
| `Fsi_S2.Slave_Payload[1].Status.all` | `0x5555` | CH5 Stage2 S2 注入正確 |

**通過標準**：所有欄位值與預期完全一致。

---

### TC-03：Wave 3 閉環收割測試（Fsi_M 更新驗證）

**目的**：驗證 CH6 在 T=8.00µs 觸發後，`Fsi_S2` 的最終封包被正確收割回 `Fsi_M`，完成閉環。

**驗證方法**：等待至少 1 個 PWM 週期後：

| 觀察變數 | 預期值 | 驗證項目 |
|---|---|---|
| `Fsi_M.Slave_Payload[0].Vout` | `0x1234` | CH6 收割 S1 遙測成功 |
| `Fsi_M.Slave_Payload[0].Iout` | `0x5678` | CH6 收割 S1 遙測成功 |
| `Fsi_M.Slave_Payload[1].Vout` | `0xABCD` | CH6 收割 S2 遙測成功 |
| `Fsi_M.Slave_Payload[1].Iout` | `0xEF01` | CH6 收割 S2 遙測成功 |
| `Fsi_M.Master_Payload.L1_Ref` | `0x0111` | CH6 收割 Master 指令保留正確 |

**通過標準**：`Fsi_M` 包含完整的 Master 指令 + S1 遙測 + S2 遙測，閉環成功。

---

### TC-04：DMA 優先級仲裁順序測試

**目的**：驗證同源觸發下 CH1 > CH2 > CH3 的執行順序，確保「先搬 Master 指令，再注入 S1 遙測，最後補齊剩餘」的原子性。

**測試方法**：

1. 將 `Fsi_M.Master_Payload.L1_Ref` 設為 `0xBEEF`
2. 將 `Local_S1.Vout` 設為 `0xDEAD`
3. 同時將 `Fsi_S1.Master_Payload.L1_Ref` 預先設為 `0x0000`（確認 CH1 有執行）
4. 同時將 `Fsi_S1.Slave_Payload[0].Vout` 預先設為 `0x0000`（確認 CH2 有執行）

**驗證**：

| 觀察變數 | 預期值 | 驗證項目 |
|---|---|---|
| `Fsi_S1.Master_Payload.L1_Ref` | `0xBEEF` | CH1 先執行，Master 指令已搬運 |
| `Fsi_S1.Slave_Payload[0].Vout` | `0xDEAD` | CH2 後執行，S1 遙測已注入 |

**關鍵驗證點**：若 CH2 先執行（優先級錯誤），`Fsi_S1.Slave_Payload[0].Vout` 會被 CH1 的 16W 搬運覆蓋為 `Fsi_M.Slave_Payload[0].Vout` 的值，而非 `Local_S1.Vout`。

---

### TC-05：位址穩定性長時測試（Address Drift）

**目的**：驗證 WrapSize=1 機制確保 100 kHz 連續運行下位址不漂移。

**測試方法**：

1. 設定固定的測試值（同 TC-01）
2. 讓系統以 100 kHz 連續運行 **10 秒**（100 萬個 PWM 週期）
3. 每隔 1 秒在 CCS Watch Window 記錄一次 `Fsi_M.Slave_Payload[0].Vout`

**驗證**：

| 時間點 | 觀察值 | 通過標準 |
|---|---|---|
| T+1s | `Fsi_M.Slave_Payload[0].Vout` | 恆為 `0x1234` |
| T+5s | `Fsi_M.Slave_Payload[0].Vout` | 恆為 `0x1234` |
| T+10s | `Fsi_M.Slave_Payload[0].Vout` | 恆為 `0x1234` |

**通過標準**：10 秒內所有觀察值恆定，無任何漂移或異常值。

---

### TC-06：動態更新測試（即時資料更新驗證）

**目的**：驗證 `Local_S1` / `Local_S2` 的值被更新後，下一個 PWM 週期的 `Fsi_M` 能即時反映新值。

**測試方法**：

1. 初始設定 `Local_S1.Vout = 0x1111`，觀察 `Fsi_M.Slave_Payload[0].Vout` 穩定為 `0x1111`
2. 透過 CCS Memory Browser 或 Watch Window 即時修改 `Local_S1.Vout = 0x9999`
3. 等待 1 個 PWM 週期（10µs）後觀察 `Fsi_M.Slave_Payload[0].Vout`

**驗證**：

| 步驟 | 觀察變數 | 預期值 |
|---|---|---|
| 修改前 | `Fsi_M.Slave_Payload[0].Vout` | `0x1111` |
| 修改後（1 週期） | `Fsi_M.Slave_Payload[0].Vout` | `0x9999` |

**通過標準**：修改後下一個週期即反映新值，延遲不超過 10µs（1 個 PWM 週期）。

---

## 五、CCS 操作步驟

### 5.1 設定 Expressions 觀察視窗

在 CCS 的 **Expressions** 視窗加入以下觀察變數：

```
Fsi_M
Fsi_S1
Fsi_S2
Local_S1
Local_S2
```

展開結構體可直接觀察各 Word 的值。

### 5.2 設定中斷點

在 `isr.c` 的 ISR 函式末尾（`Interrupt_clearACKGroup` 之前）設定中斷點，可在每個 100 kHz 週期結束時暫停並觀察 DMA 搬運結果。

### 5.3 使用 Memory Browser 驗證

在 CCS 的 **Memory Browser** 中輸入以下位址（需對應 linker cmd 中的 GSRAM 配置）：

- `&Fsi_M`：Master 封包緩衝區
- `&Fsi_S1`：Slave 1 封包緩衝區
- `&Fsi_S2`：Slave 2 封包緩衝區

以 16-bit Word 格式顯示，可直接比對 16 個 Word 的完整內容。

---

## 六、測試結果記錄表

| 測試編號 | 測試項目 | 通過標準 | 結果 | 備註 |
|---|---|---|---|---|
| TC-01 | Wave 1 Fsi_S1 拼裝完整性 | 所有欄位值與預期一致 | [ ] | |
| TC-02 | Wave 2 Fsi_S2 拼裝完整性 | 所有欄位值與預期一致 | [ ] | |
| TC-03 | Wave 3 Fsi_M 閉環收割 | Fsi_M 包含完整 M+S1+S2 資料 | [ ] | |
| TC-04 | DMA 優先級仲裁順序 | CH1 > CH2 > CH3 執行順序正確 | [ ] | |
| TC-05 | 位址穩定性（10 秒） | 100 萬週期後位址無漂移 | [ ] | |
| TC-06 | 動態更新即時性 | 修改後 1 週期內反映新值 | [ ] | |

---

## 七、已知限制與注意事項

### 7.1 Wave 3 觸發源說明

F28388D 的 DMA 硬體只支援 `SOCA` 與 `SOCB` 兩種 EPWM 觸發源，不存在 `EPWM1CMPC` 觸發。Wave 3 收割（CH6）使用 **EPWM6 SOCA** 作為替代方案：

- EPWM6 設定為相位同步模式，跟隨 EPWM1
- EPWM6 的相位偏移設定為 800 counts，對應 T=8.00µs
- 需確認 EPWM6 已在 `initHRPWM.h` 的 `initHarvestPWM()` 中正確初始化

### 7.2 Wave 2 Stage3 省略說明

`Fsi_S2[13~15]`（Reserved 欄位）不由 CH5 搬運，而是保留 CH3 在 Wave 1 時寫入的值（來自 `Fsi_M.Reserved[]`）。這是設計上的有意省略，不影響有效資料（Word 0~12）的正確性。

### 7.3 未來切換至實體 FSI

當切換至實體 FSI 硬體時，只需將觸發源替換：

| 通道 | 模擬觸發源 | 實體觸發源 |
|---|---|---|
| CH1/CH2/CH3 | `DMA_TRIGGER_EPWM1SOCA` | `DMA_TRIGGER_FSIRXA` |
| CH4/CH5 | `DMA_TRIGGER_EPWM1SOCB` | `DMA_TRIGGER_FSIRXA`（相位延遲） |
| CH6 | `DMA_TRIGGER_EPWM6SOCA` | `DMA_TRIGGER_FSIRXA`（最終封包） |

其餘 DMA 設定（位址、Burst 大小、Wrap 機制）完全不需修改。

---

## 八、修改紀錄

| 修改人 | 日期 | 版本 | 修改內容 |
|---|---|---|---|
| AI Agent | 2026-04-22 | v1.0 | 建立初始版本，涵蓋 TC-01 ~ TC-06 |
