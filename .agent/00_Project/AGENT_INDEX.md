# ASR5K Agent 文件索引

## 請先閱讀

使用任何架構、里程碑、交接、研究或實作資料之前，請依照下列順序閱讀治理文件：

1. [`.agent/AGENT_ENTRYPOINT.md`](../AGENT_ENTRYPOINT.md)
2. [`.agent/00_Project/ARCHITECTURE_AUTHORITY.md`](ARCHITECTURE_AUTHORITY.md)
3. [`.agent/00_Project/ASR5K_DECISIONS.md`](ASR5K_DECISIONS.md)
4. [`.agent/DOCUMENT_STATUS_REGISTRY.md`](../DOCUMENT_STATUS_REGISTRY.md)
5. [`.agent/ARCHITECTURE_CONFLICT_REGISTER.md`](../ARCHITECTURE_CONFLICT_REGISTER.md)

## 目前專案狀態

- [`.agent/00_Project/ASR5K_HANDOFF.md`](ASR5K_HANDOFF.md) 彙整 M5R Phase 2
  傳輸結案後的目前工程交接狀態。
- [`.agent/00_Project/ASR5K_STATUS.md`](ASR5K_STATUS.md) 記錄治理與里程碑狀態。
- [`.agent/00_Project/M5_GOVERNANCE_REPAIR_REPORT.md`](M5_GOVERNANCE_REPAIR_REPORT.md)
  記錄 M5 治理修復、衝突影響與量產前置條件。
- [`.agent/02_Milestones/M5R_PHASE2_BURST_TRANSPORT.md`](../02_Milestones/M5R_PHASE2_BURST_TRANSPORT.md)
  記錄已驗證的 Legacy burst 傳輸與回歸監控清單。

## 受控架構

架構文件必須依照 `ARCHITECTURE_AUTHORITY.md` 定義的權限順序閱讀：
正式產品文件、D01、D02、D03、D04、D05、D07、D10。D11 屬於研究或候選
文件，不能用來決定量產通訊協定。

請使用 [`.agent/DOCUMENT_STATUS_REGISTRY.md`](../DOCUMENT_STATUS_REGISTRY.md)
確認每份文件的狀態是 `ACTIVE`、`SUPERSEDED`、`HISTORICAL` 或
`REFERENCE_ONLY`。只看檔名或本索引，不能取代實際閱讀具有控制權的文件。

## 里程碑證據

[`.agent/02_Milestones/`](../02_Milestones/) 內的文件屬於里程碑證據。
M1 至 M5A，以及 M5R Phase 2，皆已具備硬體證據並完成結案。里程碑文件
不能建立或覆寫架構決策。

## 測試與導覽指南

- [一頁式硬體測試 SOP](../03_Knowledge/HardWare_Test/ASR5K_TEST_SOP.md)
  提供最精簡的建置、載入、自檢、驗收與故障判斷流程。
- [模組架構圖](../03_Knowledge/ASR5K_MODULE_ARCHITECTURE.md)
  說明目前模擬器執行架構，以及尚未驗證的量產延伸路徑。
- [Top 10 實機測項](../03_Knowledge/HardWare_Test/ASR5K_TOP10_HARDWARE_TESTS.md)
  依序涵蓋啟動、功能、負向、burst 與復原測試。
- [SCIA Modbus 除錯操作指南](../03_Knowledge/knowledge/SCIA_MODBUS_DEBUG_GUIDE.md)
  說明接線、通訊參數、第一筆讀取、寫入測試與 CCS 故障判斷。

這些指南的狀態是 `REFERENCE_ONLY`。受控架構文件與有效的里程碑證據仍具有
最終權威。

## 規則與工作流程

- [`.agent/rules/`](../rules/) 包含 Agent 行為與審查限制。
- [`.agent/workflows/`](../workflows/) 包含操作流程。

規則與工作流程不具有架構決策權。若內容與治理核心衝突，應以治理核心為準。

進行架構敏感的實作之前，請在 repository 根目錄執行
`python .agent/ci/run_checks.py`。檢查通過不能取代硬體驗證或文件權限審查。

## 研究與參考資料

`.agent/01_Architecture/Research/` 與 `.agent/03_Knowledge/` 內的研究、
知識、硬體手冊和候選設計，原則上都只是參考資料。除非經過核准的架構決策
明確提升其權限，否則不能視為量產架構依據。
