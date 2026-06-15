---
name: specify
description: "[SDD 階段一] 需求與規格定義。與工程師討論系統需求，並使用 EARS 語法產出標準化 spec.md，嚴禁生成任何程式碼。"
---

# 階段一：系統需求與規格定義 (Requirements Specification)

**目標**：在不觸碰任何 C 語言代碼的前提下，將工程師的需求收斂為嚴謹的系統規格。

**AI 執行動作**：

1. **【強制】系統底層盤點 (System Context Audit)**：
   - 在詢問需求前，AI 必須主動讀取專案內的 `device/device.c` 與 `device.h` (或其他底層初始化檔)。
   - 提取全局的系統時脈 (SYSCLK)、震盪器頻率 (XTAL)、以及硬體腳位占用狀態，確保規格的時間基準 (Time-base) 絕對正確。
2. **需求訪談 (Requirement Gathering)**：
   - 詢問工程師當前的電力電子拓撲 (Topology)、開關頻率 (Switching Frequency)、控制目標 (Control Targets) 與硬體保護機制。
   - 若工程師的提示詞資訊不足，必須主動提問。
3. **撰寫規格書 (Spec Generation)**：
   - 使用 Antigravity **Artifacts** 生成一份繁體中文的規格書。
   - 規格定義**必須**採用 **EARS 語法 (Easy Approach to Requirements Syntax)** 以確保邏輯嚴謹性，並確保 AI 與人類工程師之間的認知對齊，消除幻覺與誤差。
      - *範例 (Event-driven)*：「當 `[ADC EOC 中斷觸發]` 時，系統 `[必須]` 在 20 週期內完成 `[PI 控制器計算]`。」
      - *範例 (State-driven)*：「當系統處於 `[FAULT_ACTIVE 狀態]` 時，系統 `[必須]` 強制將 `[EPWM 輸出拉低]`。」
4. **防呆限制**：
   - **核心準則：絕對禁止**在此階段生成任何 `.c` 或 `.h` 檔案的程式碼。若偵測到代碼生成意圖，全域守護進程將強制中斷 (Abort)。
   - 在工程師於 Artifacts 面板點選 Approve (核准) 之前，流程不得進入下一階段。
   - 提醒工程師核准後，可輸入 `/design` 進入技術設計階段。