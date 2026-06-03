---
name: design
description: "[SDD 階段二] 系統架構與硬體設計。強制雙重查詢（TRM 暫存器 + DriverLib 函式原型），確認物理證據後方可輸出介面契約。"
---

# 階段二：技術設計與硬體映射 (Technical Design)

**目標**：將 `specify` 階段核准的規格，轉化為 C2000 底層硬體配置邏輯與軟體架構圖。

**AI 執行動作**：

1. **TRM 暫存器檢索 (Hardware Mapping)**：
   - **【強制 Step 1】** 必須呼叫 `query_trm` 工具（執行 `query_trm.py` 腳本）查詢本次設計會用到的周邊模組（如 EPWM, ADC, CMPSS）。
   - 根據腳本回傳的精確暫存器定義，規劃硬體初始化邏輯。
   - **嚴禁腦補或猜測暫存器位址及位元名稱。**

2. **DriverLib 函式原型查詢 (API Contract Verification)**：
   - **【強制 Step 2】** 緊接著必須呼叫 `query_driverlib` 工具（執行 `query_driverlib.py` 腳本）查詢本次設計涉及的所有 C2000Ware DriverLib 函式原型。
   - 確認函式簽名的回傳型別、參數型別（必須為 `uint32_t`/`uint16_t` 等 stdint 型別）。
   - **嚴禁憑印象推測 API 參數名稱或型別，防止 API Hallucination。**

3. **產出設計文件與介面契約 (Design Document)**：
   - **【Step 3】** 整合 TRM 與 DriverLib 雙重物理證據後，使用 Antigravity **Artifacts** 產出繁體中文的設計文件。
   - 內容必須包含：
      - **硬體資源分配表**：例如 EPWM1A 對應 High-side，ADCINA0 對應電壓回授。
      - **非阻塞流程圖**：若涉及複雜時序，必須使用 **Mermaid** 語法繪製 FSM (有限狀態機) 或 ISR 執行時序圖。
      - **資料結構設計**：定義 Functional Layer 會用到的 `struct`。
   - **核心準則：介面契約優先 (Interface Contract First)**：
      - 根據設計文件，在 Artifacts 中**優先且僅能輸出標頭檔 (`.h`)** 作為 API 契約。
      - 標頭檔必須嚴格遵循 `STRICT_STDINT` 規範（強制使用 `int16_t`, `uint32_t`, `float32_t` 等，嚴禁原生 `int`, `char`）。

4. **防呆限制**：
   - **絕對禁止**在此階段直接生成任何 `.c` 實作邏輯。若違反，將觸發全域守護進程攔截。
   - 提醒工程師審閱設計與 `.h` 介面，確認後請輸入 `/implement` 進入實作階段。