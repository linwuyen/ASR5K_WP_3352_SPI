# ASR5K_F28388D_CPU1 研究與開發計畫

本計畫針對 `ASR5K_F28388D_CPU1` 作為系統「測量核心與總司令」的角色，規劃後續的開發與優化方向，確保其滿足 100kHz 高頻控制迴圈與極致穩定性要求。

## 1. 目前狀態評估 (Current State Audit)

### 1.1 核心時序與 ISR 負擔
- **發現**: `isr.c` 中的 `INT_CPU1_ADCA_1_ISR` 是系統心臟，負責 DDS 演算與 SPIC 直接驅動。目前 ISR 內含一個 `while` 輪詢迴圈（等待 SPIC RX FIFO），雖有 Timeout 機制，但仍違反了 `RULES.md` 中的 **NON_BLOCKING_HARDWARE** 禁令。
- **影響**: 輪詢期間 CPU 無法處理其他事務，且若 SPI 硬體延遲增加，會直接推遲 ISR 退出時間。
- **行動**: 評估是否能將 SPIC 傳輸改為由 DMA 觸發，或將 `while` 邏輯改寫為更符合規範的狀態檢查，以優化 ISR Budget。

### 1.2 跨核數據交換 (Cross-Core Data Flow)
- **發現**: `main.c` 已正確配置 GSRAM Master 選項（GS12-GS13 給 CPU1 寫入 `M_Ref`）。
- **驗證**: 需確認 CPU1 在寫入 `M_Ref` 後的 Memory Barrier (`asm(" RPT #3 || NOP");`) 是否足以確保 CPU2 在同一週期（T=1.50us 點）讀取到正確數據。
- **行動**: 進行跨核資料一致性壓力測試。

### 1.3 DDS 演算性能
- **發現**: `dds_api.h` 實作了振幅、頻率、Offset 三軸獨立 Ramp。
- **行動**: 驗證 Ramp 過渡期間的相位連續性，確保在切換頻率時不會產生不連續的階躍（Glitch）。

## 2. 研究計畫目標 (Research Objectives)

### 第一階段：規則合規化與 ISR 優化 (Rule Compliance & ISR)
1.  **消除阻塞代碼**: 重構 `MEAS_DDS_ISR_handler()` 中的 `while` 迴圈。目標是將 SPI 讀取改為非阻塞模式，或證明目前的 500 次輪詢 (約 2.5us) 是在時序預算內的最小代價。
2.  **RAMfunc 覆蓋率檢查**: 確保 `DDS_Step`, `getDDS`, `MEAS_DDS_ISR_handler` 等所有在 ISR 呼叫的路徑皆正確配置於 `.TI.ramfunc`。

### 第二階段：測量模組 (SPIC) 與 DDS 精度驗證
1.  **LTC2353 解析邏輯校準**: 驗證 `spi_rx_buf` 到 `adc_chx_V` 的換算公式與 LTC2353 實際硬體規格是否吻合（考慮 SoftSpan 設定）。
2.  **DDS 頻率精度**: 測試在 1.00Hz 到 1000.00Hz 範圍內，查表演算法的頻率漂移量，並確認 4096 點查表在 100kHz 下的諧波失真 (THD) 表現。

### 第三階段：通訊與保護機制整合
1.  **SPIB Slave (Host Comms)**: 實作 `mb_slave` 與 `sDrv` 結構體的鏈接，確保上位機可以透過 SPIB 即時下達 `DDS_Start/Stop` 命令。
2.  **FSI 失聯保護 (REQ-S-02)**: 實作 CPU1 對 CPU2 心跳的監控邏輯。當 CPU2 FSI 網路異常時，CPU1 必須能觸發安全停機 (Safe Stop) 流程。

## 3. 預期產出 (Deliverables)

- [ ] **ISR 效能分析報告**: 使用 `cTimeMeas.h` 記錄的平均/最大執行時間（目標 < 1.70us）。
- [ ] **重構後的 MEAS_DDS 模組**: 符合 Zero-Blocking 規範。
- [ ] **DDS/SPIC 驗證數據**: 包含頻率精度與 ADC 量測穩定度。
- [ ] **完備的系統錯誤矩陣**: 包含 SPI Timeout、CPU2 Timeout 等保護觸發邏輯。

---

## 4. 驗證清單 (Validation Checklist)

- [ ] **Zero-Malloc**: 已確認無動態記憶體分配。
- [ ] **Zero-Division**: 控制路徑內是否已全數替換為倒數乘法？
- [ ] **Master Ownership**: GSRAM 權限是否在 `main.c` 啟動時正確分配？
- [ ] **NMI Watchdog**: 是否已啟用 1ms 級別的看門狗監控？
