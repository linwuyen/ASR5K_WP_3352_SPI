# ASR5K V2 CPU1 — DMA 架構設計

> 文件版本：v1.0
> 建立日期：2026-04-23
> 作者：roger_lin
> 適用架構：6-CH 3-Stage Precision Assembly Daisy-Chain

---

## 一、設計概述

CPU1 採用「**6-CH 3-Stage 精準拼裝**」策略，每個 Wave 只搬運必要的 Word，無冗餘複製。
與 CPU2 的「整包複製 + 覆寫」不同，各 Stage 來源不重疊，天然避免 Race Condition。

### 與 CPU2 架構差異

| 比較項目 | CPU1（6-CH 精準拼裝） | CPU2（5-CH 整包複製+覆寫） |
|---|---|---|
| **通道數** | 6 | 5 |
| **Wave 1 搬運量** | 7W + 3W + 6W = 16W（無冗餘） | 16W + 3W = 19W（有冗餘） |
| **Wave 2 搬運量** | 10W + 3W = 13W | 16W + 3W = 19W |
| **Race Condition** | 天然安全（來源不重疊） | CH1 必須先於 CH2 完成 |
| **實作複雜度** | 較高 | 較低 |

---

## 二、通訊封包資料結構

所有結構定義於 `dma_chain.h`，以 16-bit 字組 (Word) 為單位。

### 頂層封包結構 (FSI_Frame) — 16 Words

```c
#define MAX_SLAVES 2

typedef struct {
    Master_st   Master_Payload;             // Words 0~6   (Master 下發指令)
    Slaves_st   Slave_Payload[MAX_SLAVES];  // Words 7~12  (Slave 回授遙測)
    uint16_t    Reserved[3];                // Words 13~15 (填充至 16 Words)
} FSI_Frame;
```

### 主控指令 (Master_st) — 7 Words

```c
typedef struct {
    uint16_t    L1_Ref;    // Word 0: L1 電感電流參考值
    uint16_t    L2_Ref;    // Word 1: L2 電感電流參考值
    uint16_t    L3_Ref;    // Word 2: L3 電感電流參考值
    uint16_t    CC_DA;     // Word 3: 控制指令 DA 值
    FUNC_REG    ID_Func;   // Word 4: 節點 ID (4b) + 指令碼 (12b)
    uint16_t    Data_H;    // Word 5: 附帶資料高位
    uint16_t    Data_L;    // Word 6: 附帶資料低位
} Master_st;
```

### 從端遙測 (Slaves_st) — 3 Words

```c
typedef struct {
    uint16_t    Vout;      // Word +0: 輸出電壓量測值
    uint16_t    Iout;      // Word +1: 輸出電流量測值
    Slave_REG   Status;    // Word +2: 狀態旗標 (FSM 12b + IO_1 2b + IO_2 2b)
} Slaves_st;
```

### 封包 Word 映射表

```
FSI_Frame (16 Words)
+------+------+------+------+------+------+------+
| W0   | W1   | W2   | W3   | W4   | W5   | W6   |
|L1_Ref|L2_Ref|L3_Ref|CC_DA |IDFunc|Data_H|Data_L|
+------+------+------+------+------+------+------+
  <--- Master_Payload (7W) ---> CH1 搬運

+------+------+------+
| W7   | W8   | W9   |
| Vout | Iout |Status|
+------+------+------+
  <--- Slave_Payload[0] (3W) ---> CH2 注入 Local_S1

+------+------+------+------+------+------+
| W10  | W11  | W12  | W13  | W14  | W15  |
| Vout | Iout |Status|Rsv[0]|Rsv[1]|Rsv[2]|
+------+------+------+------+------+------+
  <--- Slave_Payload[1] (3W) + Reserved (3W) ---> CH3 補齊（來自 Fsi_M.Slave_Payload[1]）
```

---

## 三、記憶體配置

| 變數 | 記憶體區段 | 角色 |
|---|---|---|
| `Fsi_M` | `"MASTER"` (GS RAM) | Master 封包緩衝區，循環起點與終點（16-word） |
| `Fsi_S1` | `"SLAVE1"` (GS RAM) | Slave 1 封包緩衝區（16-word） |
| `Fsi_S2` | `"SLAVE2"` (GS RAM) | Slave 2 封包緩衝區（16-word） |
| `Local_S1` | 預設 RAM | S1 本地 ADC 量測值（由 ISR/CLA 更新，3-word） |
| `Local_S2` | 預設 RAM | S2 本地 ADC 量測值（由 ISR/CLA 更新，3-word） |

---

## 四、DMA 通道配置與資料路徑

| 通道 | 觸發源 | 來源 (Src) | 目的 (Dst) | Words | 功能 |
|---|---|---|---|---|---|
| **CH1** | `EPWM1SOCA` | `&Fsi_M.Master_Payload` | `&Fsi_S1.Master_Payload` | 7 | Stage1：Master 指令 → S1 |
| **CH2** | `EPWM1SOCA` | `&Local_S1` | `&Fsi_S1.Slave_Payload[0]` | 3 | Stage2：S1 本地遙測注入 |
| **CH3** | `EPWM1SOCA` | `&Fsi_M.Slave_Payload[1]` | `&Fsi_S1.Slave_Payload[1]` | 6 | Stage3：補齊 W10~W15 |
| **CH4** | `EPWM1SOCB` | `&Fsi_S1.Master_Payload` | `&Fsi_S2.Master_Payload` | 10 | Stage1：S1 前 10W → S2 |
| **CH5** | `EPWM1SOCB` | `&Local_S2` | `&Fsi_S2.Slave_Payload[1]` | 3 | Stage2：S2 本地遙測注入 |
| **CH6** | `EPWM6SOCA` | `&Fsi_S2` | `&Fsi_M` | 16 | 收割：S2 整包 → Master |

> Wave 2 的 Stage3 省略：`Fsi_S2[13~15]` 保留 CH3 在 Wave 1 寫入的值（來自 `Fsi_M.Reserved[]`），不影響有效資料（W0~W12）。

---

## 五、DMA 運作機制：3-Stage 精準拼裝

```
Wave 1（T=0.00µs，SOCA）：
  CH1（7W）：Fsi_M.Master_Payload → Fsi_S1.Master_Payload（Master 指令）
  CH2（3W）：Local_S1 → Fsi_S1.Slave_Payload[0]（S1 本地遙測）
  CH3（6W）：Fsi_M.Slave_Payload[1] → Fsi_S1.Slave_Payload[1]（補齊 W10~W15）

Wave 2（T=5.20µs，SOCB）：
  CH4（10W）：Fsi_S1.Master_Payload → Fsi_S2.Master_Payload（Master 7W + S1 遙測 3W）
  CH5（3W）：Local_S2 → Fsi_S2.Slave_Payload[1]（S2 本地遙測）

Wave 3（T=8.00µs，EPWM6 SOCA）：
  CH6（16W）：Fsi_S2 整包 → Fsi_M（Master 取得完整 M+S1+S2 資料，閉環）
```

### 閉環資料流向

```
Fsi_M（Master 封包）
  │
  │ CH1（7W，SOCA）Master 指令
  │ CH3（6W，SOCA）補齊 W10~W15
  ▼
Fsi_S1 ← CH2（3W，SOCA）← Local_S1（S1 ADC 量測）
  │
  │ CH4（10W，SOCB）Master 指令 + S1 遙測
  ▼
Fsi_S2 ← CH5（3W，SOCB）← Local_S2（S2 ADC 量測）
  │
  │ CH6（16W，EPWM6）
  ▼
Fsi_M（更新後的 Master 封包，含 S1+S2 回授）
```

---

## 六、Wave 3 觸發源說明

F28388D DMA 只支援 `SOCA`/`SOCB` 兩種 EPWM 觸發源，不存在 `EPWM1CMPC` 觸發。
CH6 使用 **EPWM6 SOCA** 作為 Wave 3 收割觸發：

- EPWM6 設定為相位同步模式，跟隨 EPWM1
- EPWM6 相位偏移 = 800 counts，對應 T=8.00µs
- 需確認 EPWM6 已在 `initHRPWM.h` 的 `initHarvestPWM()` 中正確初始化

---

## 七、已知狀態

`DMA_Config()` / `DMA_Start()` 目前未在 `main.c` 中呼叫，模組處於開發中狀態。
啟用前需確認 `DMA_CH5` 硬體分配（`pinmux.syscfg` 中 CH5 目前分配給 `DMA_SPIA_TX`）。

---

## 八、修改紀錄

| 修改人 | 日期 | 版本 | 修改內容 |
|---|---|---|---|
| roger_lin | 2026-04-23 | v1.0 | 建立 CPU1 專屬架構設計文件，對應 6-CH 精準拼裝實作 |
