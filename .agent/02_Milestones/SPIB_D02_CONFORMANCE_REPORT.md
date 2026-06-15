# SPIB_D02_CONFORMANCE_REPORT — SPIB Slave 與 D02_2_1 設計文件符合性

建立：2026-06-12（B015）／更新：2026-06-12（B016）
對照文件：`D02_2_1_SPIB_TO_AM3352`、`D02_COMM_ARCHITECTURE` §2.1、`D01` §2.4
性質：**符合性檢查與 Deviation 記錄。本報告不修改任何正式架構文件。**

## B016 變更摘要

1. **故障回報鏈打通（D01 §2.4「回傳 Faults」）**：
   - `0x0429` = `[15:12]health | [11:8]faultSource | [7:0]faultCode`
   - `0x042A` = `gSpibRxErrorFlags`
   - `0x0804` 寫 1 = fault clear（重開 first-fault latch，回應 eHealth）
2. **per-burst parse-fail 判定**：`SPIB_ParseWaveRawBuffer` 改回傳 bool；
   ACTIVATE 跳過判定不再讀累積旗標（歷史錯誤不再永久封鎖啟用）。
3. **wave block 解析分片化**：4096 frames 以 512/poll 分 8 次解析，
   背景迴圈單次延遲有上界；期間 CH3 掛在 post-wave 命令緩衝。
4. **wave buffer 真相修正**：block DMA 實寫 8192 words（4096 frames×2），
   `g_u16WaveRawRxBufferHi`（RAMGS11）為 DMA 延續空間，**不可移除**；
   `initSPIslave()` 開機檢查兩塊位址連續，否則立即報 DRIVER fault。
   （B015 曾誤判其為死碼移除，B016 還原並補上文件與防護。）
5. 啟動假性 idle-reset 消除（`u32ResetCount` 不再從 1 起跳）。

---

## 1. 設計目標符合性（D02_2_1 §1）

| 設計目標 | 狀態 | 實作方式 |
|---|---|---|
| 不漏資料 (No Data Loss) | ✅ | RX 全程 DMA CH3 搬移；FIFO overflow / DMA error / partial frame 三層偵錯 + 自動復原 |
| 無 CPU 阻擋 (No CPU Stall) | ✅ | TX 使用 `SPI_writeDataNonBlocking`（2 words/回應，FIFO 深度 16，不等待） |
| 零干擾 (Zero Interference) | ✅ | 不使用任何 SPI/DMA PIE 中斷；`pollReceiveFromSpi()` 於背景主迴圈輪詢 DMA 完成旗標 |

## 2. 逐項對照（D02_2_1 §5）

| 設計條款 | 規範 | 程式現況 | 判定 |
|---|---|---|---|
| §5.1 RX 觸發源 | `DMA_TRIGGER_SPIBRX` | 相同（`initSpibRxDma`） | ✅ |
| §5.1 RX DMA 通道 | **CH3** | `DMA_CH3_BASE` | ✅ |
| §5.1 完成旗標輪詢、不開 PIE 中斷 | 背景輪詢 | `SPIB_RxDmaIsDone()` 輪詢 RUN status | ✅ |
| §5.1 連續模式 Auto-Init | 硬體自動重載 | **軟體重載**（每 frame 後 `SPIB_RxDmaRestart`，ping-pong 緩衝） | ⚠️ 等效偏差 |
| §5.2 TX DMA 通道 **CH4** + Ping-Pong | 狀態封包由 DMA 回傳 | **未實作**：TX 為 CPU 直接寫 FIFO（每命令 2-word 回應） | ❌ 偏差（見 §3） |
| §7 斷線保護 | Timeout 看門狗 | 2ms 無流量自動重置 SPI 模組與 DMA | ✅ |
| §7 資料對齊 | 16-bit 對齊 | 全部 16-bit word 框架 | ✅ |
| §9 封包格式 Header/CmdID/Len/Payload/CRC16 | 嚴謹封包 | 0xA55A packet mode + **加總 checksum**（非 CRC16）；主要流量走 2-word legacy reg frame | ⚠️ 部分 |

## 3. 主要 Deviation：TX 未使用 DMA CH4

**現況**：Slave 對每個 2-word 命令立即以 CPU 寫入 2-word 回應至 TX FIFO
（`writeDirectSpiResponse`），Master 於下一個 frame 用 dummy clock 取回。

**為何能滿足設計目標**：回應僅 2 words、FIFO 深度 16、寫入非阻塞，
CPU 耗時 < 1µs 且發生於背景迴圈，不影響 100kHz ISR。三大目標（§1）皆達成。

**何時必須補上 CH4**：當 AM3352 正式協定改為「整包狀態回讀」
（TX_PACKET_SIZE > FIFO 深度）時，依 §5.2 實作 CH4 + Ping-Pong 才有必要。
屆時 Master 端（AM3352 / SPIA 模擬器）需同步改為 packet read 模式並重新上機驗證。

## 4. 文件未涵蓋的擴充（向下相容）

- **Wave burst block transport（M5R Phase2）**：4096-sample 連續下載走
  block DMA（同 CH3 重新配置），含 stall watchdog 與 post-wave 命令緩衝。
  遵循同樣的「全 DMA、無 ISR、背景輪詢」原則。
- 對應暫存器：`WAVE_BURST_BEGIN(0x095B)`、`WAVE_PAGE_SELECT(0x0958)` 等，
  見 `SPIB_Slave/cmd_id.h`。

## 5. 記憶體配置（B015 清理後）

| Section | 內容 | 位置 |
|---|---|---|
| `spib_slave_state` | 狀態/診斷/ping-pong/fallback FIFO | RAMGS3 |
| `spib_block_ram` | 4095-word block RAM | RAMGS2 |
| `spib_rx_wave_raw_1` | wave block buffer 低半部（4096 words） | RAMGS10 |
| `spib_rx_wave_raw_2` | wave block buffer 高半部（DMA 延續，勿移除） | RAMGS11 |
| `fake_sdram_page0~2` | 模擬 SDRAM 三頁（NOINIT） | RAMGS4~6 |

已移除：`spi_fifo_state`、`spib_pingpong_state`、`spib_rx_wave_buf0`
（RAMGS7 釋出可挪用）。RAMGS11 由 wave buffer 高半部佔用，**不可挪用**。

## 6. 搬回 WP_28384_AI 前置條件

1. `handleBackgroundFlashCommit()` 為模擬 stub，須接上真實 extflash 流程。
2. `cmd_parser.h` 內 `DSP_FW_Version_Code_CPU1/CPU2`、`startupFlags` 為硬編碼
   stub，須接回 `sAccessCPU1`（shareram）。
3. Fake SDRAM 路徑須切換 `ASR5K_HAS_EMIF1_SDRAM`，對接 Wave_module 後端。
4. 上機重跑 burst 下載驗證（B015 修正了 block 模式 WriteSample offset bug，
   行為與 B014 不同）。
