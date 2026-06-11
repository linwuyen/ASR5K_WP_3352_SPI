# M5_LEGACY_COMMAND_TO_DDS_INTEGRATION Architecture Contract

版本: 1.0  
類型: Tier 3 (Milestone Spec & Verification Contract)  
維護者: Antigravity AI  

---

## 1. Milestone Scope (里程碑範圍)

本里程碑 (M5) 旨在實作從 SPIB 接收之舊有 2-word 暫存器指令到內部命令事件 (Command Event) 的轉換路徑，並透過系統狀態機 (System State Machine) 與分派器 (Dispatcher) 安全地控制 DDS 運行狀態與參數。

### 1.1 支援之暫存器指令集
僅允許處理與響應以下舊有指令（Word0 = 暫存器位址，Word1 = 16-bit 資料）：
*   `0x0900`：OUTPUT_ON (啟動輸出)
*   `0x0901`：OUTPUT_OFF (停止輸出)
*   `0x0910`：DDS_FREQ (設定 DDS 頻率，單位為 Hz)
*   `0x0911`：DDS_AMP (設定 DDS 振幅，範圍 0-65535 代表 0.0-1.0 PU)
*   `0x0912`：DDS_OFFSET (設定 DDS 偏移，中心點為 32768)
*   `0x0970`：CLEAR_FAULT (清除系統故障)

### 1.2 非本里程碑範圍 (Out of Scope)
*   禁止導入任何 Packet Protocol、Header Magic 或多字組封包格式。
*   禁止修改波形下載服務與快閃記憶體寫入 (Flash Commit) 邏輯。
*   禁止修改 CPU2 與雙核 IPC 通訊邏輯。
*   禁止重新分配或更改任何 DMA 通道映射：
    *   DMA CH1：SPIC TX
    *   DMA CH2：SPIC RX
    *   DMA CH3：SPIB RX
    *   DMA CH4：SPIB TX

---

## 2. Architecture Rules (架構規則)

1.  **解耦設計 (Decoupled Design)**：
    SPIB 暫存器解析器 (Parser) 絕對禁止直接呼叫 DDS 底層更新 API。解析器必須僅生成內部命令事件 (`SysCmdEvent_e`) 並將其遞交給狀態機。
2.  **狀態機驗證 (State Validation)**：
    系統分派器/狀態機必須在背景輪詢中驗證當前系統狀態與安全條件，確認無故障且 DDS 初始化完成時，方可響應 `OUTPUT_ON`。
3.  **安全停止原則 (Safe Output OFF)**：
    `OUTPUT_OFF` 必須為最高優先級、確定性且安全的停止路徑。一旦收到停止事件，必須立即呼叫 `DDS_Stop()`，並清除輸出狀態。
4.  **互鎖保護 (Interlock Protection)**：
    當系統處於 `SYS_STATE_RUNNING` (Output ON) 時，系統必須拒絕任何快閃記憶體寫入或波形下載等可能影響即時控制的耗時操作。
5.  **故障保護 (Fault Trip)**：
    若 SPIB/SPIA 通訊發生嚴重驅動故障 (如 FIFO 溢位或 DMA 錯誤)，狀態機必須立即進入 `SYS_STATE_FAULT`，強行停止 DDS 輸出，且必須由 `CLEAR_FAULT` 指令顯式清除後方可恢復。

---

## 3. Data Structures & API Contract (資料結構與介面合約)

### 3.1 內部命令事件列舉 (Command Event Enum)
```c
typedef enum {
    SYS_EVENT_NONE = 0,
    SYS_EVENT_OUTPUT_ON,
    SYS_EVENT_OUTPUT_OFF,
    SYS_EVENT_DDS_FREQ,
    SYS_EVENT_DDS_AMP,
    SYS_EVENT_DDS_OFFSET,
    SYS_EVENT_CLEAR_FAULT
} SysCmdEvent_e;
```

### 3.2 最小系統狀態列舉 (System State Enum)
```c
typedef enum {
    SYS_STATE_STOPPED = 0,      /* 輸出關閉，DDS 停止 */
    SYS_STATE_RUNNING,          /* 輸出開啟，DDS 運行中 */
    SYS_STATE_FAULT             /* 故障觸發，輸出強行關閉 */
} SysState_e;
```

### 3.3 狀態機與控制結構體
```c
typedef struct {
    SysState_e eState;              /* 當前系統狀態 */
    uint16_t u16FaultFlags;         /* 系統故障旗標位元 */
    uint16_t u16LastEvent;          /* 最後一次處理的事件 */
    uint16_t u16LastEventData;      /* 最後一次事件所帶的資料 */
    uint32_t u32EventCount;         /* 累計處理的事件總數 */
    uint32_t u32RejectedCount;      /* 被拒絕的非法操作事件數 */
} ST_SYS_STATE_MACHINE;
```

### 3.4 API 函式原型
```c
/**
 * @brief 初始化系統狀態機與變數。
 */
void SysStateMachine_Init(void);

/**
 * @brief 向狀態機遞交一個命令事件。
 * @param eEvent 事件類型
 * @param u16Data 伴隨之數據值
 */
void SysStateMachine_PostEvent(SysCmdEvent_e eEvent, uint16_t u16Data);

/**
 * @brief 在背景主迴圈定期執行狀態機轉換與分派。
 */
void SysStateMachine_Process(void);

/**
 * @brief 觸發系統故障，強行關閉 DDS。
 * @param u16FaultReason 故障代碼原因
 */
void SysStateMachine_TripFault(uint16_t u16FaultReason);
```

---

## 4. State Transition Table (狀態轉移矩陣)

| 當前狀態 | 輸入事件 | 轉移條件 | 動作 / 影響 | 次要狀態 |
| :--- | :--- | :--- | :--- | :--- |
| **STOPPED** | `OUTPUT_ON` | `u16FaultFlags == 0` && `DDS_IsInitComplete()` | 呼叫 `DDS_Start()`, `OUTPUT_ON = 1` | **RUNNING** |
| **STOPPED** | `OUTPUT_ON` | `u16FaultFlags != 0` 或未建表完成 | 拒絕執行，`u32RejectedCount++` | **STOPPED** |
| **STOPPED** | `CLEAR_FAULT` | 無條件 | 清除局部變數，無狀態變化 | **STOPPED** |
| **RUNNING** | `OUTPUT_OFF` | 無條件 | 呼叫 `DDS_Stop()`, `OUTPUT_ON = 0` | **STOPPED** |
| **RUNNING** | `DDS_FREQ` | 有效頻率值 | 呼叫 `DDS_SetFrequency(data * 100)` | **RUNNING** |
| **RUNNING** | `DDS_AMP` | 有效振幅值 | 呼叫 `DDS_SetAmplitude(data)` | **RUNNING** |
| **RUNNING** | `DDS_OFFSET` | 有效偏移值 | 呼叫 `DDS_SetOffset(data)` | **RUNNING** |
| **RUNNING** | (通訊故障) | 驅動程式回報 Fault | 呼叫 `DDS_Stop()`, `OUTPUT_ON = 0` | **FAULT** |
| **FAULT** | `CLEAR_FAULT` | 無條件 | `u16FaultFlags = 0`, 清除通訊診斷 | **STOPPED** |
| **FAULT** | 任何其他事件 | 無條件 | 拒絕執行，`u32RejectedCount++` | **FAULT** |

---

## 5. Verification Plan & Watch Window Variables (驗證計畫與觀測變數)

### 5.1 CCS Watch Window 觀測指標
驗證通過之物理證據必須包含以下變數觀測值：
*   `g_sysSM.eState`：確認狀態轉移（0=STOPPED, 1=RUNNING, 2=FAULT）。
*   `g_sysSM.u16FaultFlags`：確認故障置位與清除。
*   `g_sysSM.u16LastEvent`：最後事件類型。
*   `g_sysSM.u32EventCount`：確認事件有正確遞增。
*   `g_sysSM.u32RejectedCount`：確認非法轉換被正確攔截。
*   `OUTPUT_ON`：觀測是否與系統狀態同步。

### 5.2 自動化自檢驗證 (Self-Test Case ID 9)
新增測試案例 `ASR5K_SPI_TEST_ID_9` 執行以下程序：
1.  驗證在 `STOPPED` 下寫入頻率、振幅及偏移（不改變輸出狀態）。
2.  發送 `0x0900` (OUTPUT_ON) 指令，確認 `g_sysSM.eState` 轉為 `SYS_STATE_RUNNING` 且 `OUTPUT_ON` 為 1，且 DDS 狀態轉為 `DDS_STATE_RUNNING` (或對應之啟動/延遲狀態)。
3.  在 `RUNNING` 狀態下，寫入新頻率 `60` (60 Hz)，確認 `DDS_GetFrequency()` 正確更新為 `6000`。
4.  在 `RUNNING` 狀態下，嘗試寫入 `0x3000` (Block Write)，驗證是否被系統狀態拒絕 (`u32RejectedCount` 遞增，且 Block Write 不會啟動)。
5.  發送 `0x0901` (OUTPUT_OFF) 指令，確認 `g_sysSM.eState` 回到 `SYS_STATE_STOPPED` 且 `OUTPUT_ON` 歸零。
6.  模擬通訊 Fault，確認狀態機轉為 `SYS_STATE_FAULT` 且無法發送 `OUTPUT_ON`，直至發送 `0x0970` 清除故障。
