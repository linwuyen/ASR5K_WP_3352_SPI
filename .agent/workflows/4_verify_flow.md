---
name: verify
description: "[SDD 階段四] 實體硬體驗證與 Git 交付。要求物理驗證數據（示波器波形/暫存器 Log），通過後生成 Conventional Commit 並進行雙源推送。"
---

# 階段四：實體驗證與交付 (Hardware Verification & Delivery)

**目標**：代碼燒錄至 C2000 晶片後，透過讀取暫存器鏡像、串口 Log 或示波器數據，確保軟硬體物理表現與 `/design` 階段的預期完全一致，並在通過後安全同步至遠端雙倉庫。

**AI 執行動作**：

1. **數據採集 (Data Acquisition)**：
   - 啟動時必須檢核「已完成編譯/燒錄代碼且硬體已上電就緒」的前置條件。
   - 主動要求使用者提供實體硬體狀態數據，包括關鍵暫存器數值 (Register View)、示波器/邏輯分析儀波形 (Scope Capture) 或串口輸出內容 (Serial Log)。

2. **差異分析 (Gap Analysis)**：
   - 比對實測波形頻率是否符合 `LSPCLK` 與 `BRR` 配置以確認時序對齊。
   - 檢查實測 Log 中的狀態轉移是否符合 FSM 設計，並核對讀回的數據與 TRM/Datasheet 手冊之一致性。

3. **異常處理與回溯 (Anomaly Resolution & Rollback)**：
   - 若實測結果不符，必須主動檢查 `GpioCtrlRegs` 腳位配置與 `SysCtrlRegs` 時脈樹配置。
   - 若屬設計缺陷，必須自動觸發「回溯設計修正」，回頭更新 `design.md` 或 `spec.md` 文件。

4. **生成驗證報告 (Verification Report Generation)**：
   - 自動生成或更新專案的 `VERIFICATION_REPORT.md`。
   - 報告必須嚴格列出對應 `spec.md` 的 Test Case ID、明確的 Pass/Fail 結果，以及物理證據紀錄。

5. **排除編譯產物與精準暫存 (Code Sync Stage)**：
   - 驗證通過後，在執行 Git stage 時**必須自動過濾並排除所有本地 CCS 編譯暫存檔** (如 `.obj`, `.out`, `.map` 及 `CPU1_RAM/`、`CPU1_FLASH/` 下的產物)。
   - 執行精準 `git add`，指定特定的 `.c`, `.h`, `.syscfg`, `.md` 檔案，嚴禁 `git add .` 將無效二進位檔帶入。

6. **雙源推送與交付 (Double-Source Git Push)**：
   - 使用 Conventional Commits 規範提交變更 `git commit -m "..."`。
   - 同時執行雙推送，推送到 GitHub 遠端倉庫 `git push origin main` 與 GitLab 遠端倉庫 `git push lab main`，確保雙源 100% 物理對齊。

7. **關鍵防呆限制 (Verification Guardrail)**：
   - **核心準則：嚴禁推測 (No Guessing)**：AI 絕對不得擅自判定測試已經成功。必須由人類工程師提供物理性證據 (Evidence) 後，方可判定 Pass 並關閉任務。
   - 所有的報錯或異常判定必須引用 TRM 中的特定章節或暫存器位元定義。
