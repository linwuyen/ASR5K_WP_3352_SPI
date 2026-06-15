# M5_WAVE_SDRAM_TRANSPORT_CLOSURE_PLAN

建立：2026-06-14
性質：**設計/收斂計畫。本計畫不修改任何正式架構文件，亦不在本階段改 code。**
對照文件：D04_WAVE_DATA_PIPELINE、D05_EMIF1_MEMORY_MAP、D10_WAVE_VALIDATION_POLICY
治理依據：ARCHITECTURE_AUTHORITY、ASR5K_DECISIONS（FD-001/002/003/006）、CONFLICT_REGISTER（CR-003/CR-004）

---

## 1. Scope / Non-Scope

### 1.1 In-Scope（本階段唯一主線）

驗證並收尾 **3352 → SDRAM** 的 transport 鏈：

```
AM3352 / SPI Master Emulator
→ SPIB Slave
→ DMA CH3
→ Legacy Register Parser
→ WaveDownload_WriteSample()
→ Fake SDRAM / EMIF1 SDRAM abstraction
→ WaveDownload_ReadSample() readback verification
```

收尾工作項：
1. 確認上述鏈已端到端可驗（Test1–Test9 regression）。
2. 讓 `WaveDownload_ValidatePage()` 達到 **D10 基本要求**，補上 per-sample
   checksum（定位為 **SDRAM storage consistency**，見 §4）。
3. 將 **EMIF1 address formula** 列為 HIGH PRIORITY NEEDS_VERIFICATION 並定義
   解決路徑（見 §5）。
4. 定義 readback verification 方法與 closure / exit criteria（§6、§9）。

### 1.2 Non-Scope（本階段明確不做）

- DDS runtime
- 100kHz ISR / CPUTimer2 ISR / EPWM ISR
- SPIC / AD5543 output path
- CPU2
- DMA CH1 / CH2
- Packet Protocol 導入
- SPIB TX CH4 DMA 補強（僅登記為 migration blocker，見 §10）
- Active Page → DDS getter（D07 範疇，延後）

---

## 2. Transport Chain Under Test（現況，as-is）

| 階段 | 實作位置 | 狀態 |
|---|---|---|
| SPIB RX FIFO → DMA CH3 | `spi_b_slave.c`（RX/DMA 段） | ✅ 已驗證，**不在本計畫改動** |
| Legacy Register Parser | `spi_b_slave.c` `WaveDownload_HandleWrite` 路由 | ✅ |
| WriteSample | `SPIB_Slave/wave_download.c:102` | ✅ 寫入＋連續性/count 追蹤 |
| SDRAM abstraction（寫） | `wave_download.c:71` `storage_write_sample` | ✅ Fake(GSRAM)/EMIF1 雙後端 |
| SDRAM abstraction（讀） | `wave_download.c:84` `WaveDownload_ReadSample` | ✅ bounds-checked |
| Readback verification | self-test Test6/Test8（`asr5k_spi_selftest.c:590/718`） | ✅ 逐點比對 |
| Download Complete / Validate / Activate | `wave_download.c:150/163/204` | ⚠️ Validate 缺 checksum（§4） |
| OUTPUT_ON 保護 | `wave_download.c:252` | ✅ 符合 D04 §11 / D10 §12 |

State machine（EMPTY→DOWNLOADING→DOWNLOAD_COMPLETE→VALIDATING→VALID/INVALID→
LOCKED）與 D10 §4 一致。`[CODE][DOC]`

---

## 3. Current Closure Status（gap 清單）

| # | 項目 | 現況 | closure 需求 |
|---|---|---|---|
| G1 | Validate per-sample checksum | **未實作**（`wave_download.c:194` 註明） | 補最小實作（§4） |
| G2 | EMIF1 address formula | `pageId*8192` 以 `uint16_t*` 位移＝16KB/page，疑與 D05（8KB/page=4096 words）不符 | HIGH PRIORITY 驗證/修正（§5） |
| G3 | Validate 失敗 error code | 僅回 INVALID，未填 D10 §17 error code | 補 error code（§4） |
| G4 | Fake checksum 儲存區段 | 不存在 | 新增 RAMGS7 區段（§7） |

---

## 4. Checksum Minimum Implementation Decision

**裁示採納**：本階段補 per-sample checksum，使 `ValidatePage()` 符合 D10 基本要求。

### 4.1 演算法（D10 §9）

```c
chk = ((sample >> 8) & 0x00FF) + (sample & 0x00FF);   /* one word per sample */
```

### 4.2 流程

- **WriteSample 時**：算出該 sample 的 `chk`，寫入 checksum storage
  `[pageId][offset]`（與 sample 並行儲存）。
- **ValidatePage 時**：對 4096 點逐一 `ReadSample()` 重算 `chk`，與 checksum
  storage 比對；任一不符 → page = INVALID、error_code = `CHECKSUM_ERROR(0x0006)`、
  `checksum_error_count++`。比對於背景、Output OFF 執行（D10 §18，本階段無 ISR）。

### 4.3 語意界定（重要，避免過度宣稱）

> 目前 AM3352 Legacy 2-word frame **不攜帶獨立 checksum word**。因此本版 checksum
> 驗證的是 **SDRAM storage consistency（write → SDRAM → readback 路徑完整性）**，
> **不**宣稱為 end-to-end transport checksum。end-to-end transport checksum 需待
> protocol 攜帶獨立 checksum（D10/D11 後續版本）才成立。`[DOC][CODE]`

---

## 5. EMIF1 Address Formula — HIGH PRIORITY NEEDS_VERIFICATION

**裁示採納**：列為 HIGH PRIORITY；家中 Fake SDRAM 階段不阻塞，啟用
`ASR5K_HAS_EMIF1_SDRAM` 前必須修正或證明無誤。

**疑點** `[CODE][NEEDS_VERIFICATION]`：
`wave_download.c:74/91` 使用
`(uint16_t *)(WAVE_SDRAM_BASE_ADDR + (uint32_t)u16PageId * 8192UL)` 再 `page[offset]`。
C28x 為 16-bit word-addressed，`uint16_t*` 位移以 **word** 計：
- 現碼 page stride = 8192 **words** = 16 KB/page。
- D05 §6/§7 定義 page = 8192 **bytes** = 4096 words = 8 KB/page。
→ 現碼疑為 D05 的 **2×** spacing，且與外部 byte-based address map / 未來 readback
定址不一致；Wave Checksum Area（D05 §8 offset `0x00200000`）亦尚未配置。

**解決路徑（擇一，動工前定案）**：
- (A) 修正 stride 為 `pageId * WAVE_SAMPLES_PER_PAGE`（4096 words），並對齊 D05
  byte/word 換算後再開 `ASR5K_HAS_EMIF1_SDRAM`；或
- (B) 提出書面證明（量測/反組譯位址）證實現行 16KB stride 為刻意設計且與 D05
  一致解讀，登記於 deviation。

**本階段 closure 僅以 Fake SDRAM（pages 0–2）達成**；`ASR5K_HAS_EMIF1_SDRAM`
維持 OFF，直到 G2 結案。

---

## 6. Readback Verification Method

closure 的「驗證」以既有 `WaveDownload_ReadSample()` 為唯一讀路徑：

1. **內容比對**：Master 端 `g_u16SpiMasterWaveRam[0..4094]` vs
   `ReadSample(page, idx)` 逐點相同（self-test Test6 sine 已做）。
2. **storage consistency**：ValidatePage 的 checksum recompute（§4）。
3. **連續性/數量**：sample_count==4096、address 連續、last==0x3FFF、
   download_complete（既有 ValidatePage 條款）。
4. **計數一致性**：`gSpibRxParseFailCount` 差值 == 0、`gSpibRxErrorFlags` == 0。

---

## 7. Files To Modify（本階段）

| 檔案 | 動作 | 理由 |
|---|---|---|
| `SPIB_Slave/wave_download.c` | 改 | WriteSample 寫 checksum；ValidatePage recompute/compare + error code；checksum 診斷計數 |
| `SPIB_Slave/wave_download.h` | 改 | checksum storage 宣告、error code 常數、metadata 欄位（`checksum_error_count`/`error_code`） |
| `main.syscfg`（MEMORY_CONFIG 段） | 改 | 新增 `fake_checksum_page0..2` → **RAMGS7**（conformance §5 已釋出）；**不碰** SPI/DMA/timer 設定 |

> 註：build 產出的 linker `.cmd` 位於 gitignore 的 `CPU1_RAM/`，由 syscfg 產生；
> 區段放置遵循 AC-003（approved memory architecture + verified linker evidence）。

---

## 8. Files NOT To Modify（保護凍結基線）

- `spi_b_slave.c` 的 **Legacy parser / DMA CH3 RX 路徑**（已驗證 transport baseline）。
- `spi_fifo.c` / `spi_pingpong.c`（Slave 內部緩衝）。
- `SPIA_Master/*`（Master 測試模擬器，僅作為 stimulus）。
- `cmd_id.h` / `cmd_parser.h` 暫存器/封包定義（**不導入 Packet Protocol**，CR-003）。
- **DMA CH3/CH4 配置不動；CH5/CH6 完全不碰**（CR-002）。
- `WaveDownload_ReadSample/WriteSample` 既有 **public 簽章**維持穩定。
- `asr5k_spi_selftest*` 框架（可**新增** checksum 測項，不破壞 Test1–9）。
- 無新增：DDS / ISR / CPUTimer2 / EPWM / SPIC / CPU2 / DMA CH1-2（§1.2）。

---

## 9. Test Matrix

| ID | 測試 | 通過條件 | 執行 |
|---|---|---|---|
| T-REG | Test1–Test9 regression | 全維持既有 PASS；ParseFail 差值=0、ErrorFlags=0 | 手動(HW) |
| T-RB | Test6 sine readback | `MasterWaveRam` vs `ReadSample(page)` 逐點相同 | 手動(HW) |
| T-CK+ | checksum 正路徑 | 下載完整 page → Validate=VALID、checksum_error_count=0 | 手動(HW) |
| T-CK- | checksum 負路徑 | 以 debugger 改 SDRAM 單字 → 重 Validate=INVALID、error=0x0006 | 手動(HW) |
| T-VAL | Validate 前置 | 缺 sample / 不連續 / 無 complete / Output ON → INVALID + 對應 D10 §17 error code | 手動(HW) |
| T-BG | 背景時序 | Validate 4096-loop 於 Output OFF 背景執行，不影響 SPIB DMA 計數 | 手動(HW) |
| T-EMIF | EMIF1 formula 證明 | （G2 結案才跑）pages 0–2 不同 pattern 寫入/讀回無 cross-page aliasing，位址符 D05 | 延後 |

> Hardware Execution Rule：上機為**手動**、需使用者授權；本計畫預設 static
> inspection + build-only，不自動燒錄。

---

## 10. Risks / Migration Blockers

| 類別 | 項目 | 處置 |
|---|---|---|
| **HIGH PRIORITY NEEDS_VERIFICATION** | EMIF1 address formula（word/byte，§5 G2） | gate `ASR5K_HAS_EMIF1_SDRAM`，本階段 Fake-only closure |
| **Migration blocker** | SPIB TX CH4 DMA（正式 readback/status response 架構需求） | **僅登記**，不在本階段 blocking（D04 §12 / D10 §15） |
| **Migration blocker（延後）** | DDS runtime / 100kHz timing（D07，最終回到 D01 EPWM master / ADC EOC / SPIC 同步鏈） | 後續階段 |
| 語意風險 | checksum 僅 storage consistency，非 end-to-end transport | §4.3 已界定，文件勿過度宣稱 |
| 容量範圍 | Fake SDRAM 僅 3 頁；產品 19 頁（D05 §9） | closure 範圍限 pages 0–2 |
| Race（資訊性） | 本階段無 ISR，Validate 與 background 同 context，無並發 race | 維持單 context |

---

## 11. Closure / Exit Criteria

本階段於下列全數成立時可宣告 **M5 Wave SDRAM Transport Closure（Fake-only）**：

1. T-REG / T-RB / T-CK+ / T-CK- / T-VAL / T-BG 全部 EXECUTED_PASS。
2. `ValidatePage()` 達 D10 基本要求（count/continuity/complete/output-off +
   storage-consistency checksum + D10 §17 error code）。
3. G2（EMIF1 formula）已**結案或明確延後**且 `ASR5K_HAS_EMIF1_SDRAM` 保持 OFF。
4. deviation 登記完成：SPIB TX CH4（migration blocker）、checksum 語意界定、
   EMIF1 formula NEEDS_VERIFICATION。

T-EMIF 與 EMIF1 實機驗證**不屬本階段 closure**，列入下一階段（EMIF1 enablement）。
