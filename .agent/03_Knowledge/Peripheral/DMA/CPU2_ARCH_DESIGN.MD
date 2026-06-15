# ASR5K V2 CPU2 — DMA 架構設計

> 文件版本：v1.0
> 建立日期：2026-04-23
> 作者：roger_lin
> 適用架構：5-CH 整包複製 + 覆寫（Copy then Fill）

---

## 一、設計概述

本模組模擬 FSI 菊花鏈通訊中各節點「接收 → 填充 → 轉發」的循環動態。
CPU2 採用「**整包複製 + 本地覆寫**」策略（5-CH），與 CPU1 的 3-Stage 精準拼裝不同。

### 與 CPU1 架構差異

| 比較項目 | CPU2（整包複製+覆寫） | CPU1（3-Stage 精準拼裝） |
|---|---|---|
| **通道數** | 5-CH | 6-CH |
| **Wave 1 搬運量** | 16W + 3W = 19W | 7W + 3W + 6W = 16W |
| **冗餘搬運** | 有（S1/S2 舊資料被複製後立刻覆蓋） | 無 |
| **Race Condition 風險** | CH1 必須在 CH2 前完成 | 各 Stage 來源不重疊，天然安全 |
| **實作複雜度** | 較低 | 較高 |

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
    uint16_t    CC_DA;     // Word 3: 控制指令 DA 值 (CMPC 動作點核心)
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

### 位元欄位定義

```c
// FUNC_REG — Master 指令碼
typedef union {
    uint16_t all;
    struct {
        uint16_t Cmd_ID  : 12;  // Bits 0~11: 指令碼
        uint16_t Node_ID : 4;   // Bits 12~15: 目標節點 ID
    } ID_FUNC_BITS;
} FUNC_REG;

// Slave_REG — Slave 狀態旗標
typedef union {
    uint16_t all;
    struct {
        uint16_t FSM  : 12;  // Bits 0~11: 狀態機狀態
        uint16_t IO_1 : 2;   // Bits 12~13: 數位輸入 1
        uint16_t IO_2 : 2;   // Bits 14~15: 數位輸入 2
    } Status_BITS;
} Slave_REG;
```

### 封包 Word 映射表

```
FSI_Frame (16 Words)
+------+------+------+------+------+------+------+
| W0   | W1   | W2   | W3   | W4   | W5   | W6   |
|L1_Ref|L2_Ref|L3_Ref|CC_DA |IDFunc|Data_H|Data_L|
+------+------+------+------+------+------+------+
  <--- Master_Payload (7W) --->
  由 CH1 整包複製，CH2 不覆蓋此區域

+------+------+------+
| W7   | W8   | W9   |
| Vout | Iout |Status|
+------+------+------+
  <--- Slave_Payload[0] (3W) --->
  CH2 注入 Local_S1 至此位置

+------+------+------+------+------+------+
| W10  | W11  | W12  | W13  | W14  | W15  |
| Vout | Iout |Status|Rsv[0]|Rsv[1]|Rsv[2]|
+------+------+------+------+------+------+
  <--- Slave_Payload[1] (3W) + Reserved (3W) --->
  CH4 注入 Local_S2 至 W10~W12
```

---

## 三、記憶體配置

| 變數 | 記憶體區段 | 角色 |
|---|---|---|
| `Fsi_M` | `"MASTER"` (GS RAM) | Master 封包緩衝區，循環起點與終點（16-word） |
| `Fsi_S1` | `"SLAVE1"` (GS RAM) | Slave 1 封包緩衝區（16-word） |
| `Fsi_S2` | `"SLAVE2"` (GS RAM) | Slave 2 封包緩衝區（16-word） |
| `Local_S1` | 預設 RAM | S1 本地 ADC 量測值（由 ISR 更新，3-word） |
| `Local_S2` | 預設 RAM | S2 本地 ADC 量測值（由 ISR 更新，3-word） |

---

## 四、DMA 通道配置與資料路徑

| 通道 | 觸發源（現行） | 觸發源（正確） | 來源 (Src) | 目的 (Dst) | Words | 功能 |
|---|---|---|---|---|---|---|
| **CH1** | `EPWM1SOCA` | `&Fsi_M` | `&Fsi_S1` | 16 | 整包複製 Master → S1 |
| **CH2** | `EPWM1SOCA` | `&Local_S1` | `&Fsi_S1.Slave_Payload[0]` | 3 | 覆寫 S1 本地遙測 |
| **CH3** | `EPWM1SOCB` | `&Fsi_S1` | `&Fsi_S2` | 16 | 整包複製 S1 → S2 |
| **CH4** | `EPWM1SOCB` | `&Local_S2` | `&Fsi_S2.Slave_Payload[1]` | 3 | 覆寫 S2 本地遙測 |
| **CH5** | `EPWM6SOCA` | `&Fsi_S2` | `&Fsi_M` | 16 | 收割 S2 → Master |

---

## 五、DMA 運作機制：整包複製 + 覆寫（Copy then Fill）

```
Wave 1（T=0.00µs，SOCA）：
  Step 1 — CH1（16W）：Fsi_M 整包複製到 Fsi_S1
  Step 2 — CH2（3W）：Local_S1 覆寫 Fsi_S1.Slave_Payload[0]（Word 7~9）

Wave 2（T=5.20µs，SOCB）：
  Step 1 — CH3（16W）：Fsi_S1 整包複製到 Fsi_S2
  Step 2 — CH4（3W）：Local_S2 覆寫 Fsi_S2.Slave_Payload[1]（Word 10~12）

Wave 3（T=8.00µs，EPWM6 SOCA）：
  CH5（16W）：Fsi_S2 整包收割回 Fsi_M（Master 取得完整 M+S1+S2 資料）
```

### 閉環資料流向

```
Fsi_M（Master 封包）
  │
  │ CH1（16W，SOCA）
  ▼
Fsi_S1（S1 封包）← CH2（3W，SOCA）← Local_S1（S1 ADC 量測）
  │
  │ CH3（16W，SOCB）
  ▼
Fsi_S2（S2 封包）← CH4（3W，SOCB）← Local_S2（S2 ADC 量測）
  │
  │ CH5（16W，EPWM6）
  ▼
Fsi_M（更新後的 Master 封包，含 S1+S2 回授）
```

### DMA 仲裁機制

同源觸發時，CH1 優先於 CH2（整包複製先於遙測注入），CH3 優先於 CH4，確保覆寫順序正確。

---

## 六、Wave 3 觸發源說明

F28388D DMA 只支援 `SOCA`/`SOCB` 兩種 EPWM 觸發源，不存在 `EPWM1CMPC` 觸發。
CH5 使用 **EPWM6 SOCA** 作為 Wave 3 收割觸發：

- EPWM6 設定為相位同步模式，跟隨 EPWM1
- EPWM6 相位偏移 = 800 counts，對應 T=8.00µs
- 需確認 EPWM6 已在 `initHRPWM.h` 的 `initHarvestPWM()` 中正確初始化

---

## 七、修改紀錄

| 修改人 | 日期 | 版本 | 修改內容 |
|---|---|---|---|
| roger_lin | 2026-04-23 | v1.0 | 合併 資料結構.MD + 模擬行為.MD，統一為架構設計文件 |
