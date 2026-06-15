# Skill: C28x ePWM Configuration

## 1. When to use (觸發時機)
- 當任務涉及「初始化 ePWM 模組」、「設定 PWM 頻率/工作週期 (Duty Cycle)」、「生成數位電源控制訊號」或「設定 Action Qualifier (AQ) / Dead-Band (DB)」時，必須立刻觸發此技能。
- 適用平台：TI C2000 / C28x 系列微控制器。

## 2. Tool Invocation (前置硬體審核)
- **強制動作**：在撰寫或修改任何代碼前，必須呼叫 TRM 檢索腳本（例如 `query_trm.py ePWM`）來確認 ePWM 暫存器的精確定義與偏移量。
- 若無法取得外部工具回傳的真實數據（HARDWARE_AUDIT 失敗），AI 必須立即中斷生成並向工程師回報警告。

## 3. How to use (執行步驟與邊界約束)
- **硬體保護解鎖**：任何受保護的系統暫存器（如 GPIO MUX、Trip Zone）寫入，必須以 `EALLOW;` 開始，並以 `EDIS;` 結束。
- **時序與狀態機控制**：修改 ePWM 時基暫存器前，**必須**先禁用時鐘同步機制 `CpuSysRegs.PCLKCR0.bit.TBCLKSYNC = 0;`，設定完成後再重新啟用 `CpuSysRegs.PCLKCR0.bit.TBCLKSYNC = 1;`。
- **遵守三層架構**：硬體暫存器操作僅限於 HAL (硬體抽象層)，嚴禁將其暴露給 Control Layer。

## 4. Response Format (預期輸出與代碼規範)
請嚴格遵守以下格式，**嚴禁產生任何 Magic Number 或憑空捏造位址**，**絕對禁止使用絕對記憶體位址（Absolute addresses）**。必須使用 TI 官方的位元欄位結構體（Bit-field structs）：

```c
// ✅ 預期代碼格式範例 (Few-shot)
// [PWM 參數說明: 頻率, Duty]
void InitEPwm_Custom(void) {
    // 1. 禁用 TBCLKSYNC (受保護暫存器需解鎖)
    EALLOW;
    CpuSysRegs.PCLKCR0.bit.TBCLKSYNC = 0;
    EDIS;
    
    // 2. 使用 TI 官方結構體指標進行位元級別操作
    // Time Base SubModule Registers
    EPwm1Regs.TBCTL.bit.PRDLD = TB_SHADOW;
    EPwm1Regs.TBPRD = [Calculated_Period]; 
    EPwm1Regs.CMPA.bit.CMPA = [Calculated_Duty];
    EPwm1Regs.TBCTL.bit.CTRMODE = TB_COUNT_UPDOWN;

    // Action Qualifier SubModule Registers
    EPwm1Regs.AQCTLA.bit.CAU = AQ_SET;
    EPwm1Regs.AQCTLA.bit.CAD = AQ_CLEAR;
    
    // 3. 重新啟用 TBCLKSYNC (受保護暫存器需解鎖)
    EALLOW;
    CpuSysRegs.PCLKCR0.bit.TBCLKSYNC = 1;
    EDIS;
}
```