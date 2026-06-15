---
Category: Protocols
Status: Verified
Related Files: [PROTO_Modbus_QuickStart.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/.agent/specs/knowledge/PROTO_Modbus_QuickStart.md)
---

# ASR5K Modbus 模型驅動開發 (MDD) 工作流程指南


## 1. 架構概述
本專案採用「模型驅動開發 (Model-Driven Development, MDD)」架構來處理 Modbus 通訊與變數對應。
核心思想為：**以 Excel 試算表作為單一真相來源 (Single Source of Truth)**。所有的 C 語言暫存器指標映射、結構體配置，以及 PC 端測試軟體 (`MultiModbusPoll`) 的 UI 介面，均由 Excel 內的 VBA 腳本自動生成。此設計徹底消除了手動對齊 Modbus 地址的痛苦與人為失誤。

## 2. 硬體物理連線 (除錯冶具設定)
在實驗室開發階段，為了同時兼顧 JTAG 除錯與 Modbus 通訊，必須使用專屬的「轉接板 (Interposer Board)」。

### 🚨 物理層陷阱與嚴格禁忌
- **嚴禁使用 JTAG 排線傳輸 UART**：標準 XDS110 JTAG 排線的 Pin 10 與 Pin 12 內部為 `GND`。若期望訊號能透過 JTAG 燒錄器內建的 Virtual COM Port 傳遞，訊號會直接被排線短路接地，導致 `SCIRXBUF` 永遠為空。
- **嚴禁對接 3.3V 電源**：在雙方設備（電腦端與控制板端）皆有獨立供電的情況下，絕對不可連接 3.3V 線，否則兩端微小壓差將導致大電流灌入，燒毀主機板電源 IC。

### ✅ 正確的 UART 跨接方法
1. 準備一條獨立的 **USB 轉 TTL (3.3V)** 序列線（如 CH340 / FTDI）。
2. 將其接至轉接板獨立分流出來的 **`J1402 (MODBUS DEBUG)`** 接口：
   - `USB-TX` 接 `J1402 Pin 3 (RX)`
   - `USB-RX` 接 `J1402 Pin 2 (TX)`
   - `USB-GND` 接 `J1402 Pin 4 或是 Pin 5 (GND)`

## 3. 韌體變數新增與除錯工作流程 (SOP)
當工程師需要驗證新的硬體功能 (如 SDRAM, ADC, FSI) 時，請嚴格遵守以下 4 步標準流程：

### 步驟一：韌體端變數宣告
在 CCS 專案中 (例如 `HwVerification.c` 或 `main.c`) 完成底層驅動撰寫，並將需要被監控或被外部寫入的數值存入全域結構體中。
- *範例*：`sDrv.sdramTestResult` 或是 `sCLA.u32HeartBeat`。

### 步驟二：於 Excel 定義變數 (Model Definition)
1. 打開 `mb_slave/buildupBuckTable.xlsm`。
2. 於表格最下方新增您的變數定義：
   - **Menu**: 填入人類可讀的變數名稱 (這將顯示在 PC 端介面上，例如：`SDRAM Test Result`)。
   - **Format**: 選擇資料型態 (例如：`T_U16` 或 `T_F32`)。
   - **Words**: 16-bit 資料填 `1`，32-bit 資料 (含 Float) 填 `2`。
   - **Link**: 精確填寫步驟一在 CCS 原始碼中宣告的真實變數名稱 (例如：`sDrv.sdramTestResult`)。

### 步驟三：自動生成與編譯 (Code Generation)
1. 點擊 Excel 表格中的巨集生成按鈕 (執行 `gen_mbslave.vba` 腳本)。
2. VBA 腳本將自動於背景執行兩項任務：
   - **更新韌體映射**：自動覆寫 `mb_slave/linkVariables.c`，生成 C 語言的變數指標映射函數。
   - **產出 UI 設定檔**：自動生成 `BusPollScript_ID_3.csv` 檔案。
3. 回到 CCS，按下 **Rebuild** 重新編譯整個 CPU1 專案，並將 `.out` 檔燒錄至 ASR5K 控制卡。

### 步驟四：PC 端自動同步與測試 (Verification)
1. 開啟 PC 端的專屬軟體 `MultiModbusPoll`。
2. 匯入剛剛由 Excel 生成的 `BusPollScript_ID_3.csv`。
3. 點擊 Connect 建立連線 (預設參數：Baud `115200`, Data `8`, Parity `Even`, Stop `1`, ID `3`)。
4. 軟體介面將自動擴充出您剛剛新增的變數列，並即時顯示韌體回傳的數值，完成除錯循環。

## 4. 架構設計思維分析
為何不在控制卡上預留獨立的 SCI 接口，而是選擇將訊號與 JTAG 複合在 `J2000` 接頭上？

- **量產環境適應性 (Field Application)**：控制卡 (Control Card) 在最終量產出貨時會插拔在主功率板上，此時並不需要 JTAG 燒錄器介入。將 UART 訊號與 `J2000` 複合，能讓主功率板透過排線直接擷取 Modbus 訊號並轉換為 RS-485 介面提供給客戶使用，精簡了控制卡本身的連接器體積與成本。
- **實驗室物理隔離 (Noise Immunity)**：在 R&D 開發階段，JTAG 訊號頻率極高且脆弱，而 Modbus 傳輸經常帶有外部接地雜訊。透過專用轉接板將 `J1400 (JTAG)` 與 `J1402 (Modbus)` 進行物理分流，能避免通訊雜訊耦合干擾燒錄品質，確保開發環境的高穩定度。
