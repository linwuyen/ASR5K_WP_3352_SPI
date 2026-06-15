# M5R Phase 2 Burst Transport

Status: HARDWARE VERIFIED  
Verification date: 2026-06-12  
Scope: `Emu_3352_SPI` Legacy Register Protocol wave-download transport

## Architecture Boundaries

This milestone is implementation evidence only. It follows the frozen project
decisions:

- Legacy Register Protocol remains the production protocol.
- SPIB RX uses DMA CH3; SPIB TX uses DMA CH4.
- CPU1 owns parsing and the waveform download path.
- The new burst-begin command is a normal Legacy `ADDR + DATA` register write.
- D11 Packet Protocol remains candidate-only.

## Protocol Sequence

The lossless full-page transfer is:

```text
0x0958 = page_id
0x095B = 4096
2 x (0xFFFF, 0x0000) guard frames
0x3000..0x3FFF = 4096 sample frames
1 x (0xFFFF, 0x0000) trailing flush frame
0x0959 = 1
0x0960 = 1
0x0961 = 1
```

`0x095B` is `WAVE_BURST_BEGIN_ADDR`. The slave accepts the production burst
only when a page is selected and the announced sample count is exactly 4096.
An invalid begin returns `0xFFFF` and does not arm block DMA.

## Transport Invariants

1. **Pre-arm before sample 0**

   The slave parses `WAVE_BURST_BEGIN`, then arms block DMA before the first
   wave sample. Two guard frames provide the main-loop opportunity required
   for the mode switch.

2. **Re-arm every block**

   SPI RX DMA requests depend on the RX FIFO watermark edge. At a block
   boundary the FIFO may remain above the watermark. The slave uses bounded
   `DMA_forceTrigger()` calls to consume the backlog below the watermark so
   later wire traffic can generate a new hardware edge.

3. **No per-sample TX replies in block mode**

   Wave samples are parsed and stored normally, but their direct ACK writes are
   suppressed while a DMA block is being parsed. Burst MISO is transport
   filler and is ignored by the master. This prevents stale sample ACKs from
   corrupting the response to `WAVE_DOWNLOAD_COMPLETE`.

The compatibility overflow detector remains available for an older sender that
omits `WAVE_BURST_BEGIN`. That path is best-effort and does not guarantee
zero-loss entry.

## Verification Evidence

Hardware Test1 through Test9 completed with:

- Overall selftest status: `PASS`
- Completed tests: `9 / 9`
- Test9 expected/actual state: `6 / 6` (`LOCKED`)
- Test9 DMA done delta: `4108`
- Test9 parse-ok delta: `4108`
- Test9 parse-fail delta: `0`
- Test9 DMA restart delta: `4108`
- Page 1 sample count: `4096`
- SPIA fault: `0`
- SPIB fault: `0`
- SPIB error flags: `0`

The verified block shape is eight full 512-frame blocks followed by one
3-frame block: two guard frames, 4096 samples, and one trailing frame.

## Regression Watch List

Power-cycle the target, run Test1 through Test9, then check:

| Expression | Required result |
|---|---|
| `g_asr5kSpiSelfTest.status` | `ASR5K_SPI_SELFTEST_PASS` |
| `g_asr5kSpiSelfTest.completed_test_count` | `9` |
| `g_asr5kSpiSelfTest.failed_test_id` | `0` |
| `g_asr5kSpiSelfTest.fault_code` | `0` |
| `g_asr5kSpiSelfTest.test[8].status` | `ASR5K_SPI_TEST_PASS` |
| `g_asr5kSpiSelfTest.test[8].expected` | `6` |
| `g_asr5kSpiSelfTest.test[8].actual` | `6` |
| `g_asr5kSpiSelfTest.test[8].delta.dma_done` | `>= 4108` |
| `g_asr5kSpiSelfTest.test[8].delta.parse_ok` | equal to `dma_done` |
| `g_asr5kSpiSelfTest.test[8].delta.parse_fail` | `0` |
| `g_asr5kSpiSelfTest.test[8].delta.dma_restart` | `>= dma_done` |
| `g_asr5kSpiSelfTest.test[8].spiA_fault` | `0` |
| `g_asr5kSpiSelfTest.test[8].spiB_fault` | `0` |
| `g_asr5kSpiSelfTest.test[8].error_flags` | `0` |
| `g_waveDownload.u16SampleCount[1]` | `4096` |
| `g_waveDownload.bAddressContinuous[1]` | `true` |
| `g_waveDownload.u16LastAddress[1]` | `0x3FFF` |
| `g_waveDownload.bDownloadComplete[1]` | `true` |
| `g_waveDownload.u16PageState[1]` | `WAVE_PAGE_STATE_LOCKED` (`6`) |
| `g_waveDownload.u16ActivePage` | `1` |

Use deltas for DMA/parser counters. Their absolute values include earlier
tests and can change when the suite changes.

## Build Regression

Both configurations must compile and link:

```powershell
C:\ti\ccs1281\ccs\utils\bin\gmake.exe -C Emu_3352_SPI\CPU1_RAM all
C:\ti\ccs1281\ccs\utils\bin\gmake.exe -C Emu_3352_SPI\CPU1_FLASH all
```

Temporary B-series debug arrays, event rings, sequence captures, MOSI/MISO
traces, and firmware build tags are not part of the production regression
interface.
