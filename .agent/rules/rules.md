---
trigger: always_on
---

# ASR5K Firmware Rules (Compact)
> ASR5K 專案 AI Agent 精簡規範。所有代理系統、MCP 伺服器與工程師必須 100% 遵循。
> 權衡原則：**謹慎高於速度**。

---

## 0. 執行原則 [EXECUTION_PRINCIPLES]

**寫程式前先思考：** 不確定時停下來問，不要默默假設。若有多種解讀，列出來讓人選。有更簡單的方法時主動說。

**簡潔第一：** 只實作被要求的功能。禁止：未被要求的抽象層、彈性、可配置性、不可能發生的錯誤處理。

**微創式修改：** 只動必須改的部分。不順便改進相鄰代碼。因自己修改產生的孤立 import/變數/函式 → 移除。先前已存在的死代碼 → 提及，不刪除。

**目標驅動執行：** 任務必須轉化為可驗證目標。多步驟任務先列計畫：[步驟]→驗證：[檢查項]

---

## 1. 硬性約束 [HARD CONSTRAINTS — 任一違反立即 ABORT]

STRICT_STDINT: 僅允許 u16/i16/u32/i32/f32。禁 int/char/long/uint8_t/int8_t。
HUNGARIAN: 前綴：u16=uint16_t, i16=int16_t, u32=uint32_t, i32=int32_t, f32=float。修飾：g_全域, s_靜態, p指標, pf函式指標, st局部結構體, b布林。後綴：_pu標幺值。型態：ST_結構體, 列舉大寫+_e。函數：verbCamelCase。
ZERO_DIVISION: ISR/Control/Task 禁 `/`，改用倒數乘法/TMU/IQmath。main()初始化不限。
ZERO_MALLOC: 禁 malloc/free，靜態配置。
VOLATILE: 跨CPU/CLA/DMA共用變數加volatile。DMA緩衝區加aligned(8)。
ZERO_BLOCKING: ISR/關鍵路徑禁polling迴圈，改FSM或callback。
HW_PROTECTION: 受保護暫存器用EALLOW/EDIS包覆。ePWM須Trip-Zone。
CODE_LOCATION: 時間關鍵ISR加 #pragma CODE_SECTION(.TI.ramfunc)。
ASCII_ONLY_CODE: .c/.h及註解禁非ASCII字符。
FPU_PRIORITY: 有FPU32時用原生float，禁混用IQmath。
LINKER_CHECK: 新增>1KB陣列或自訂段前查.cmd確認空間。
MDD_READONLY: 禁手動修改mb_slave/下的自動生成檔。
HW_API_VERIFY: 呼叫外設函式前必須讀driverlib/*.h確認簽名，禁腦補。

---

## 2. 記憶體與 Linker [MEMORY]

GSRAM：GS0–4=CPU1 / GS5–11,15=CPU2 / GS12–13=CPU1寫CPU2讀 / LS0–7=CLA專用禁越界。
IPC屏障：跨核變數寫入後立即 `asm(" RPT #3 || NOP");`。
CLA：無堆疊，禁遞迴，禁呼叫任何標準C函式庫（printf/malloc/memcpy/memset等）。
MDD：操作mb_slave/前讀取 .agent/specs/knowledge/ 下的 PROTO_Modbus_Workflow.md 與 PROTO_Modbus_QuickStart.md。

---

## 3. 系統架構 [ARCHITECTURE]

```
HAL → Module → Control（禁跨層呼叫）
```
CLA任務代碼→Control層；CLA暫存器配置→HAL層。
IoC：callback取代polling / Facade：主迴圈只呼叫單一介面 / Queue：指令入佇列，FSM消化，禁Busy直接拒絕。

---

## 4. 開發工作流 [SDD WORKFLOW]

嚴禁跳躍階段：
```
/specify  → 產出spec.md，鎖定代碼生成直到Approve
/design   → 僅輸出.h介面契約，鎖定實作直到Approve
/implement→ 產出filename_preview.c，禁直接覆蓋實體檔案
/verify   → 提供物理證據（示波器/記憶體Dump/HIL），通過後產生繁中Conventional Commits訊息
```

---

## 5. 語言與排版 [LANGUAGE]

對話/計畫/*.md：繁體中文。.c/.h及註解：純ASCII。禁LaTeX（$/$$），數學用純文字如 `tr = 2.2 * R * C`。回覆格式：結論→證據→行動。禁用無效情緒詞。
數學公式呈現要用 Latex，方塊圖跟流程圖要用Mermaid or Graphviz，時序圖要用wavedrom ，數據分析優先用Python。
只要是程式類別設計，都要使用 FSM
---

## 6. 起飛前檢核 [PRE-FLIGHT — 輸出任何 C/C++ 前必須執行]

```xml
<pre_flight_check>
- 匈牙利命名        [YES/NO]
- p/b前綴           [YES/NO]
- 無禁用型態        [YES/NO]
- 無polling迴圈     [YES/NO]
- 無malloc/free     [YES/NO]
- volatile共享變數  [YES/NO/N/A]
- 無/除法           [YES/NO]
- EALLOW/EDIS       [YES/NO/N/A]
- Trip-Zone         [YES/NO/N/A]
- ramfunc pragma    [YES/NO/N/A]
- 純ASCII代碼       [YES/NO]
- 無LaTeX           [YES/NO]
- Linker空間確認    [YES/NO/N/A]
- Artifacts預覽     [YES/NO]
</pre_flight_check>
```
> 任一項為 `NO` → **立即 ABORT**