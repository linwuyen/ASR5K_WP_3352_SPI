# ASR5K_F28388D_CPU2 研究與開發計畫

本計畫旨在針對 `ASR5K_F28388D_CPU2` 目前的代碼實作進行深度分析與優化，解決硬體資源衝突，並確保其符合三層架構與 TI C2000 開發規範。

## 1. 目前狀態評估 (Current State Audit)

### 1.1 硬體資源衝突 (Hardware Resource Conflict) - **CRITICAL**
- **發現**: `DMA_module/dma_chain.c` 使用 DMA CH1/CH2 進行 FSI Daisy Chain 封包轉發，而 `McBSP_module/mcbsp_spi.c` 同樣使用 DMA CH1/CH2 進行補償迴路通訊。
- **影響**: `main.c` 初始化序列中，後呼叫的 `Init_DMA_CompensationLoop` 將覆蓋前者的設定，導致 FSI 功能失效。
- **行動**: 重新分配 DMA 通道（如：FSI 使用 CH1-CH5，McBSP 使用 CH6）。

### 1.2 系統通訊邏輯缺漏 (System Integration)
- **發現**: `IPC_Module/IPC_Core.c` 定義了 `IPC_Run()`，但 `main.c` 的主迴圈與 `timetask.c` 中皆未呼叫。
- **影響**: 跨核通訊旗標無法被正確清除或更新，導致通訊掛起。
- **行動**: 將 `IPC_Run()` 整合至 `main.c` 或 `task1msec` 中。

### 1.3 暫存器配置矛盾 (Peripheral Config Inconsistency)
- **發現**: `McBSP_module/mcbsp_spi.c` 中 `loopbackModeFlag` 設為 `true`，但註解說明需使用外部跳線。
- **行動**: 根據實際硬體電路（`Board.c`/SysConfig）確認是否開啟數位迴路測試。

## 2. 研究計畫目標 (Research Objectives)

### 第一階段：基礎建設與規格對齊 (Infrastructure & SDD)
1.  **更新 SDD 文件**: 針對 `DMA` 與 `McBSP` 模組，先在 `docs/` 內建立/更新詳細的規格說明書 (SDD)，明確定義硬體觸發源與 DMA 通道分配。
2.  **型態與合約檢查**: 確保所有 `.h` 檔案符合 `coding_style.md` 要求，並移除任何不必要的 `malloc` 或阻塞式 `while` 迴圈。

### 第二階段：核心功能修正與整合 (Core Logic & Integration)
1.  **DMA 通道重排**: 實施 DMA 通道去衝突化，確保 FSI 與 McBSP 能同時運作。
2.  **IPC 健全性檢驗**:
    - 實作 `IPC_Run()` 調度。
    - 驗證 `GSRAM` 存取權限是否正確分配給 CPU2（需查閱 CPU1 端 `device.c` 或 SysConfig）。
3.  **時序驗證**: 利用 `cTimeMeas.h` 工具量測 `100kHz` 控制迴路中 DMA 觸發至 McBSP 完成的實際 Latency，確保滿足 `10us` 內的即時性要求。

### 第三階段：效能壓榨與安全性檢查 (Optimization & Safety)
1.  **RAMfunc 移轉**: 確保所有在主迴圈高頻呼叫的函數（如 `pollTimeTask`, `IPC_Run`）皆宣告為 `.TI.ramfunc`，消除 Flash 等待週期。
2.  **CLA 任務開發**: 若 CPU2 主頻負擔過重，評估將補償迴路邏輯移轉至 `cla/cla_task.cla`。
3.  **Modbus 通訊整合**: 驗證 `mb_slave` 與應用層變數的連結邏輯。

## 3. 預期產出 (Deliverables)

- [ ] **更新後的規格文件 (SDD)**: 包含最新的硬體資源分配表。
- [ ] **修正後的初始化代碼**: 解決 DMA 衝突問題。
- [ ] **效能報告**: 包含主迴圈 Cost 與中斷延遲量測數據。
- [ ] **符合規範的 IPC 模組**: 確保 CPU1/CPU2 穩定同步。

---

## 4. 驗證清單 (Validation Checklist)

- [ ] **Zero-Malloc**: 搜尋代碼確保無動態分配。
- [ ] **No-Blocking**: 檢查 `while` 迴圈是否具備 Timeout 或 FSM 化。
- [ ] **SDD-First**: 任何修改前是否已更新 `docs/` 內的 Markdown？
- [ ] **DMA Alignment**: FSI 與 McBSP 是否同時正常運作？
