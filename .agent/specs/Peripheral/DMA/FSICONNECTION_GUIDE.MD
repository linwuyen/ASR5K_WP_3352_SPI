# ASR5K V2 CPU2 — 實體 FSI 串接指南

本指南說明如何將 CPU2 目前的「DMA 模擬架構」遷移至「實體 FSI 硬體網絡」。

---

## 一、遷移前提條件

在切換至實體 FSI 前，必須先完成以下工作：

1. **修正 ISS-001**：將 CH3/CH4 觸發源改為 `EPWM1SOCB`，CH5 改為 `EPWM6SOCA`
2. **完成 TC-01 ~ TC-06 測試**：確認模擬架構邏輯正確
3. **確認 FSI 硬體初始化**：CPU2 的 `board.c` 已初始化 FSITXA 與 FSIRXA

---

## 二、硬體初始化確認

CPU2 的 `board.c` 已完成以下 FSI 初始化（SysConfig 生成）：

```c
// FSI TX（FSITXA_BASE）
FSI_setTxSoftwareFrameSize(FSI_DAISY_TX_BASE, 14);
FSI_setTxDataWidth(FSI_DAISY_TX_BASE, FSI_DATA_WIDTH_2_LANE);
FSI_performTxInitialization(FSI_DAISY_TX_BASE, 2);

// FSI RX（FSIRXA_BASE）
FSI_setRxSoftwareFrameSize(FSI_DAISY_RX_BASE, 14);
FSI_setRxDataWidth(FSI_DAISY_RX_BASE, FSI_DATA_WIDTH_2_LANE);
FSI_performRxInitialization(FSI_DAISY_RX_BASE);
```

GPIO 腳位（已由 SysConfig 設定）：

| 訊號 | GPIO | 說明 |
|---|---|---|
| FSITXA D0 | GPIO0 | FSI TX 資料線 0 |
| FSITXA D1 | GPIO1 | FSI TX 資料線 1 |
| FSITXA CLK | GPIO2 | FSI TX 時脈 |
| FSIRXA D0 | GPIO12 | FSI RX 資料線 0 |
| FSIRXA D1 | GPIO11 | FSI RX 資料線 1 |
| FSIRXA CLK | GPIO13 | FSI RX 時脈 |

---

## 三、DMA 觸發源切換（核心變更）

將 `dma_chain.c` 中的模擬觸發源替換為實體 FSI 事件：

| 通道 | 模擬觸發源 | 實體觸發源 | 說明 |
|---|---|---|---|
| CH1 | `EPWM1SOCA` | `DMA_TRIGGER_FSIRXA` | FSI RX 收到封包後觸發 |
| CH2 | `EPWM1SOCA` | `DMA_TRIGGER_FSIRXA` | 同源觸發，CH1 優先 |
| CH3 | `EPWM1SOCB` | `DMA_TRIGGER_FSIRXA` | S1 轉發後觸發 |
| CH4 | `EPWM1SOCB` | `DMA_TRIGGER_FSIRXA` | 同源觸發，CH3 優先 |
| CH5 | `EPWM6SOCA` | `DMA_TRIGGER_FSIRXA` | 最終封包收到後觸發 |

**切換方式**（只需修改觸發源，其餘設定不變）：

```c
// 模擬版本
Setup_DMA_Channel(DMA_CH1_BASE, &Fsi_S1, &Fsi_M, 16, DMA_TRIGGER_EPWM1SOCA, false);

// 實體版本（只改這一個參數）
Setup_DMA_Channel(DMA_CH1_BASE, &Fsi_S1, &Fsi_M, 16, DMA_TRIGGER_FSIRXA, false);
```

---

## 四、FSI TX 發送邏輯（實體版新增）

在實體架構中，每個節點收到封包並填入本地資料後，需主動發送至下一站。
在 ISR 或 DMA 完成回呼中加入：

```c
// 將 Fsi_S1 封包發送至 Slave 2
FSI_writeTxBuffer(FSI_DAISY_TX_BASE, (uint16_t *)&Fsi_S1, sizeof(FSI_Frame) / 2);
FSI_startTx(FSI_DAISY_TX_BASE);
```

---

## 五、全域 PWM SYNC 對齊

實體多板架構中，需確保所有晶片的 T=0.00µs 在同一時刻：

1. 連通硬體 SYNC 線路（GPIO6 SYNCOUT → GPIO90 SYNCIN）
2. 在 `initHRPWM.h` 中啟用 `ENABLE_SYNCOUT()` 巨集
3. 確認 `SYNC_PWM_BASE` 已在 `board.h` 中定義

---

## 六、遷移驗證步驟

1. 完成模擬架構所有測試（TC-01 ~ TC-06）
2. 修改觸發源為 `DMA_TRIGGER_FSIRXA`
3. 加入 FSI TX 發送邏輯
4. 連接實體 FSI 硬體線路
5. 重新執行 TC-01 ~ TC-06，驗證實體架構行為與模擬一致

---

## 七、修改紀錄

| 修改人 | 日期 | 版本 | 修改內容 |
|---|---|---|---|
| AI Agent | 2026-04-22 | v2.0 | 依據 CPU2 board.c 實際 FSI 初始化內容重寫，補充遷移前提條件 |
