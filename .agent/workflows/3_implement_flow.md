---
name: implement
description: "[SDD 階段三] 程式碼實作、沙盒預覽與自動化驗證。依據 design.md 嚴格生成代碼於 Artifacts，強制觸發安全自檢腳本，經人工核准後方可寫入實體專案檔案。"
---

# 階段三：實作、自檢與沙盒預覽 (Implementation, Verification & Sandbox Preview)

**目標**：將技術設計轉化為符合 C2000 全域規範 (Global Rules) 的底層 C 語言代碼。基於高頻電源控制「安全與效能第一」的要求，實作過程強制導入沙盒預覽與自動腳本自檢，未經人工核准嚴禁變更實體檔案。

**AI 執行動作**：

1. **任務拆解 (Task Breakdown)**
   - 讀取上一階段的技術設計。
   - 在對話框中列出分步實作 Check-list（例如：1. 實作 HAL 層初始化 -> 2. 實作 ISR 邏輯 -> 3. 綁定中斷向量）。
2. **生成沙盒預覽 (Sandbox Preview Generation)**
   - **【強制沙盒 — 工具級鎖定】** 在取得人工核准前，**嚴禁呼叫 `replace_file_content`、`multi_replace_file_content`、或 `write_to_file`（`IsArtifact=false`）修改專案目錄內的 `.c` 或 `.h` 檔案**。
   - 代碼預覽**只能**使用 `write_to_file`（`IsArtifact=true`）產出至 Artifacts 目錄，檔名以 `_preview.c` 或 `_preview.h` 結尾。
   - 程式碼與註解嚴禁包含非 ASCII 字符 (遵守 ASCII_ONLY_CODE)。
   - 100% 遵守 `rules.md` 零違規限制：
      - **契約對齊**：`.c` 的內部邏輯必須 100% 吻合 `/design` 階段產出的 `.h` API 契約與 SDD，嚴禁擅自更改函式原型。
      - **STRICT_STDINT**：嚴格遵守 16-bit Byte 架構，禁止出現 `int`, `char`, `long`。
      - **架構限制**：嚴格執行三層解耦架構（跨層資料傳遞必須透過 `struct` 指標）。
      - **ZERO_DIVISION**：嚴禁使用除法運算子 (`/`)。
      - **ZERO_MALLOC**：嚴禁使用動態記憶體配置。
      - **安全防護**：暫存器操作必須具備 `EALLOW` / `EDIS` 防護。
      - **執行效能**：關鍵 ISR 必須標註 `#pragma CODE_SECTION`，確保在 `.TI.ramfunc` 零等待區段執行。
3. **自動化自檢 (Automated Self-Check)**
   - 預覽代碼生成後，**【強制】主動呼叫 `check-division` Skill** 掃描剛生成的 Artifacts 內容。
   - 若腳本回報違規除法，必須立即觸發自我修正，以預計算倒數或 C28x TMU 硬體指令替換之。
   - 將最終的安全檢測報告與預覽代碼一併提交。
4. **等待人工核准 (Wait for Human Approval)**
   - 明確向工程師回報：「請審閱預覽代碼與自檢報告。若確認無誤，請點擊 **Approve (核准)** 或指示 **『寫入專案』**」。
   - **通行閘口 (Gateway)**：必須由人類工程師給予最終的「Approve (承認)」，才允許覆寫正式專案代碼。獲得授權前，流程強制凍結，阻斷任何實體檔案寫入動作。
5. **實體檔案寫入 (Physical File Writing)**
   - 取得工程師明確核准後，解鎖寫入權限。
   - 使用文件編輯工具，將預覽區代碼精確注入工作區對應的實體 `.c` 與 `.h` 檔案中。
6. **後置作業 (Post-Implementation)**
   - 寫入完成後，主動提示工程師執行編譯測試 (Build Test)，驗證無語法錯誤及記憶體區段 (Memory Map) 衝突。