# ASR5K SPI 一頁式測試 SOP

狀態：`REFERENCE_ONLY`  
更新日期：2026-06-15  
測試目標：TMDSCNCD28388D、`Emu_3352_SPI`、CPU1

本 SOP 是驗證目前已通過硬體測試之 SPIA Master 對 SPIB Slave 模擬器的
最短可重複流程。控制性證據請以 `M5R_PHASE2_BURST_TRANSPORT.md` 為準。

## 1. 測試前置條件

- CCS 12.8.1 與 C2000Ware 5.04 已安裝於 `C:\ti`。
- Target board 已供電，使用共地與 3.3 V 邏輯準位。
- 將 SPIA Master 連接至 SPIB Slave：

| 訊號 | SPIA Master | SPIB Slave |
|---|---|---|
| MOSI/PICO | GPIO16 | GPIO63 |
| MISO/POCI | GPIO17 | GPIO64 |
| CLK | GPIO18 | GPIO65 |
| CS/PTE | GPIO19 | GPIO66 |

選用的 UART 自檢 console：SCIA TX GPIO135、RX GPIO136、`115200 8E1`。

## 2. Repository 與建置檢查

在 repository 根目錄執行：

```powershell
python .agent\ci\run_checks.py
python -B -m unittest discover `
  -s Emu_3352_SPI\flashapi\table_manager\tests `
  -t Emu_3352_SPI\flashapi\table_manager -v
C:\ti\ccs1281\ccs\utils\bin\gmake.exe -C Emu_3352_SPI\CPU1_RAM all
C:\ti\ccs1281\ccs\utils\bin\gmake.exe -C Emu_3352_SPI\CPU1_FLASH all
```

兩種韌體組態都必須成功產生 `Emu_3352_SPI.out`，且不能出現 build error。

## 3. 載入與啟動

1. 將 target board 重新上電。
2. 在 CCS 載入 `Emu_3352_SPI\CPU1_RAM\Emu_3352_SPI.out`。
3. 執行 CPU1。
4. 在 CCS Expressions 加入：

```text
spiA_master
spiB_slave
g_asr5kSpiSelfTest
g_waveDownload
gSpibRxDmaDoneCount
gSpibRxParseOkCount
gSpibRxParseFailCount
gSpibRxDmaRestartCount
gSpibRxErrorFlags
```

## 4. 啟動 Test1 至 Test9

建議使用 CCS：

```text
g_asr5kSpiSelfTest.start = 1
```

也可使用 UART：

```text
spi_test all<CR><LF>
```

UART 啟動後預期收到 `SPI_TEST RUN`。等待最終的 `PASS`，或
`FAIL failed_test_id=... failed_step=... fault_code=...` 結果。

## 5. 通過條件

| Expression | 必要結果 |
|---|---|
| `g_asr5kSpiSelfTest.status` | `ASR5K_SPI_SELFTEST_PASS` |
| `completed_test_count` | `9` |
| `failed_test_id` | `0` |
| `fault_code` | `0` |
| `test[8].expected / actual` | `6 / 6`（`LOCKED`） |
| `test[8].delta.dma_done` | `>= 4107`（minimum；status poll retry 時可更高） |
| `test[8].delta.parse_ok` | 等於 `dma_done` |
| `test[8].delta.parse_fail` | `0` |
| `test[8].delta.dma_restart` | `>= dma_done` |
| `test[8].spiA_fault / spiB_fault` | `0 / 0` |
| `test[8].error_flags` | `0` |
| `g_waveDownload.u16SampleCount[1]` | `4096` |
| `bAddressContinuous[1]` | `true` |
| `u16LastAddress[1]` | `0x3FFF` |
| `u16PageState[1]` | `6`（`LOCKED`） |
| `u16ActivePage` | `1` |

判斷 DMA/parser counter 時，請使用 delta，不要使用絕對累計值。

## 6. 失敗判斷流程

1. 記錄 `failed_test_id`、`failed_step`、`fault_code`、`spiA_fault`、
   `spiB_fault` 與 `error_flags`。
2. 檢查四條 SPI 訊號線與共地。
3. 確認 SPIA 和 SPIB 都設定為 12.5 MHz、16-bit word。
4. 重新執行前先將 target board 重新上電。若沒有 reset，已鎖定的 page
   或已 latch 的 fault 可能使重複測試失效。
5. 若 Test9 失敗，優先檢查 sample count、last address、位址連續性，
   以及 Test9 的 DMA/parser delta。

## 驗證範圍限制

通過本 SOP 代表模擬器的 Legacy burst transport 與 fake-storage 波形生命週期
已通過驗證，但不代表量產 EMIF1 backend 或完整 D10 per-sample checksum
已經驗證完成。
