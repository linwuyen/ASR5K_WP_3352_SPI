# ASR5K V2 CPU1 — DMA 時序線路

> 文件版本：v1.0
> 建立日期：2026-04-23
> 作者：roger_lin
> 適用架構：6-CH 3-Stage Precision Assembly Daisy-Chain

---

## 一、全域時基（EPWM1，100 kHz）

CPU1 使用 **EPWM1** 作為 DMA 觸發時基，週期 Ts = 10µs。

```
EPWM1 Up-Down 模式，PWM_CNTS = 1000 (200MHz / 100kHz / 2)

T = 0.00µs  CTR = 0    （底部）  SOCA  → 觸發 Wave 1 DMA（CH1/CH2/CH3）
T = 5.20µs  CTR = 520  （上升段）SOCB  → 觸發 Wave 2 DMA（CH4/CH5）
T = 8.00µs  EPWM6 SOCA            → 觸發 Wave 3 收割（CH6）
T = 10.00µs 週期結束
```

---

## 二、設計目標時序（規格）

| 時間點 | 觸發事件 | DMA 動作 |
|---|---|---|
| T = 0.00µs | SOCA（CTR=0） | Wave 1：CH1（7W）+ CH2（3W）+ CH3（6W）精準拼裝 Fsi_S1 |
| T = 5.20µs | SOCB（CTR=520↑） | Wave 2：CH4（10W）+ CH5（3W）精準拼裝 Fsi_S2 |
| T = 8.00µs | EPWM6 SOCA | Wave 3：CH6（16W）收割 Fsi_S2 → Fsi_M（閉環） |
| T = 10.00µs | 週期結束 | 下一週期開始 |

---

## 三、程式碼觸發源設定（`dma_chain.c`）

| 通道 | 觸發源 | 狀態 |
|---|---|---|
| CH1 | `EPWM1SOCA` | ✓ 正確 |
| CH2 | `EPWM1SOCA` | ✓ 正確 |
| CH3 | `EPWM1SOCA` | ✓ 正確 |
| CH4 | `EPWM1SOCB` | ✓ 正確 |
| CH5 | `EPWM1SOCB` | ✓ 正確 |
| CH6 | `EPWM6SOCA` | ✓ 正確 |

---

## 四、DMA 仲裁機制

同源觸發時，CH1 擁有最高優先級（`DMA_setPriorityMode(true)`），依序執行：

```
SOCA 觸發（T=0.00µs）：
  CH1（7W）→ CH2（3W）→ CH3（6W）依序接力，共 16W

SOCB 觸發（T=5.20µs）：
  CH4（10W）→ CH5（3W）依序接力，共 13W

EPWM6 SOCA 觸發（T=8.00µs）：
  CH6（16W）獨立執行，與 Wave 1/2 完全隔離
```

---

## 五、EPWM 比較值設定

| 比較器 | 計數值 | 對應時間 | 用途 |
|---|---|---|---|
| SOCA（CTR=0） | 0 | T=0.00µs | Wave 1 觸發 |
| CMPB（上升段） | 520 | T=5.20µs | Wave 2 觸發（SOCB） |
| EPWM6 相位偏移 | 800 counts | T=8.00µs | Wave 3 收割觸發 |

---

## 六、效能監控

ISR 從 T=8.00µs 開始，必須在 T=10.00µs 前完成，可用窗口 **2.00µs**。
使用 GPIO 翻轉配合示波器觀察 ISR 執行時間。

---

## 七、修改紀錄

| 修改人 | 日期 | 版本 | 修改內容 |
|---|---|---|---|
| roger_lin | 2026-04-23 | v1.0 | 建立 CPU1 專屬時序文件，對應 6-CH 精準拼裝實作 |
