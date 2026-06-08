# DMA CH5 註解原因分析 (2026-04-22)

## 1. 現狀描述
在 `dma_chain.c` 中，負責「收割 (Harvesting)」數據的 CH5 通道被註解掉，且觸發源設定為 `DMA_TRIGGER_EPWM6SOCB`。

## 2. 分析結果
經過代碼與設計文檔 (`時序線路.MD`) 對比，發現以下衝突點：

*   **時序不匹配**：設計文檔要求 CH5 在 **8.00 µs** 由 `EPWM6_INT` 觸發，但程式碼中被註解的部分使用 **5.20 µs** 的 `SOCB`。如果在 5.20 µs 觸發，數據尚未完成 Slave 2 的本地填充。
*   **資源衝突**：`pinmux.syscfg` 中 `DMA_CH5` 已被分配給 `DMA_SPIA_TX`。若 SPI-A 正在使用 DMA，則無法供 FSI Daisy Chain 使用。
*   **系統狀態**：`DMA_Config` 函數目前未在 `main.c` 中呼叫，該模組目前處於模擬/開發階段。

## 3. 建議
若要啟用收割功能，需將 CH5 觸發源修正為 `EPWM6_INT` 或 `CMPC`，並確認 `DMA_CH5` 的硬體分配權限。
