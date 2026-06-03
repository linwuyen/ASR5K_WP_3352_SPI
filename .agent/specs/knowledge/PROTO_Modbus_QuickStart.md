---
Category: Protocols
Status: Verified
Related Files: [PROTO_Modbus_Workflow.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/.agent/specs/knowledge/PROTO_Modbus_Workflow.md)
---

# Modbus MDD 變數新增操作指南 (SOP)


本專案使用 Excel 模型驅動開發，嚴禁手動修改 `mbcmd.h` 與 `linkVariables.c`。請務必遵循以下步驟：

---

## 一、 操作流程 (Step-by-Step)

### 第一步：韌體變數準備
1.  在 CCS 原始碼中定義全域變數（例如：`uint32_t g_myVar;`）。
2.  確保該變數在標頭檔中有 `extern` 宣告。
3.  **脈絡化筆記**：若編譯報錯 `identifier undefined`，請檢查 `mb_slave/ModbusCommon.h` 是否已包含該標頭檔。**所有 Modbus 需要看到的變數，其頭文件都必須在這裡集合。**

### 第二步：Excel 填寫 (buildupBuckTable.xlsm)
打開 `mb_slave/buildupBuckTable.xlsm`，依照以下規則新增橫列：

| 欄位 | 填寫重點 |
| :--- | :--- |
| **Menu** | 顯示在 PC 端的名稱 (如: `My Test Variable`) |
| **Format** | 選擇型態 (`T_U16`, `T_U32`, `T_F32` 等) |
| **Words** | 16-bit 填 `1`, 32-bit (包含 Float) 填 `2` |
| **Link** | **必須與 CCS 中的變數名稱完全一致** (如: `g_myVar`) |

### 第三步：代碼生成與同步
1.  在 Excel 中點擊 **【Build】** 按鈕。
2.  回到 CCS 進行 **Rebuild**。
3.  燒錄 `.out` 檔至 DSP。
4.  開啟 PC 端 **`MultiModbusPoll`**，匯入新生成的 `BusPollScript_ID_3.csv` 即可觀測。

---

## 二、 核心原理與常見問題 (Why & How)

### 1. 停車場管理員邏輯 (對齊規則)
*   **大車 (32-bit, Words=2)**：寬度佔 2 格，且**車頭必須停在「偶數」編號的停車位**。
*   **小車 (16-bit, Words=1)**：寬度佔 1 格，停哪裡都行。
*   **脈絡背景**：這是因為 C28x 晶片在偶數位址存取 32-bit 數據時效率最高，所以 VBA 腳本強制執行此限制。

### 2. 為什麼會報錯？ (連鎖反應)
*   如果您的表格是：`小車 (ID 28)` -> `大車 (ID 29)`。
*   **結果**：腳本會噴錯！因為小車只佔一格，大車就被擠到了 **29 (奇數)** 的位址。
*   **修正法**：
    *   **湊一對**：在 29 位址再塞一台小車，讓下一個空位變回偶數。
    *   **排隊法 (最推薦)**：**把大車全部推到表格最前面**，小車全部丟到最後面。只要前面都是長度為 2 的大車，偶數加 2 永遠是偶數，保證不會報錯。

### 3. 架構邏輯鏈條 (數據怎麼流動的？)
*   **Excel (設計圖)**：定義數據在通訊包裡的「位置」。
*   **VBA (搬運工)**：按下 Build 時，它會自動寫出一行代碼 `pReg->Var = g_myVar;`。
*   **DSP (實際執行)**：每當 Modbus 任務執行時，這行代碼會把記憶體裡的數值實時「拷貝」進通訊包。
*   **PC (翻譯機)**：PC 軟體讀到原始數據後，根據 Excel 產出的 `.csv` 設定檔將它翻譯成您看得懂的 Float 或 Int。

---
*文件修訂日期：2026-05-05*
*維護人：Antigravity AI (ASR5K 專家)*
