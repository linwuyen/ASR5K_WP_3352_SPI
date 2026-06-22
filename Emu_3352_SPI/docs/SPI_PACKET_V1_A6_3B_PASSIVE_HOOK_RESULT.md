# SPI Packet V1 A6-3B - Gated SPIB Passive Wire Hook (Result)

Status: **IMPLEMENTED, host + CCS build verified. Board wire verification: NOT RUN.**
Board-verifiable: only when built with `SPI_PACKET_V1_WIRE_PROBE_ENABLE == 1`
AND an external SPI master sends a Packet V1 frame. Neither is claimed as a board
PASS here.
Builds on: A6-3A pure-C recognizer, A6-3B0 hook plan.

Base branch: `feature/spi-packet-v1-a6-3b0-passive-hook-plan`
Base commit: `b13d533412fd67c4807a9904f1a36533daf2b47f`
This branch: `feature/spi-packet-v1-a6-3b-passive-hook`

ASCII-ONLY for MS950 / CCS compilation and review safety.

---

## 1. Scope

First gated SPIB passive Packet V1 wire hook. When explicitly enabled and armed,
it consumes a candidate Packet V1 frame from the words the SPIB RX path already
receives, feeds the A6-3A recognizer, and records the receive result on the
existing self-test surface. It is passive: **no PONG / no MISO response**. It does
not change SPIB DMA behavior, does not change legacy behavior when disabled, and
uses no UART / Modbus.

This is passive receive only. PONG / MISO response is explicitly NOT implemented
(future A7).

## 2. Exact files changed

- `Emu_3352_SPI/SPIB_Slave/spi_b_slave.c` - gated recognizer instance + arm flag +
  five accessors + the consume-when-header-seen hook in `pollReceiveFromSpi()`.
- `Emu_3352_SPI/asr5k_spi_selftest.c` - gated Test11 (fault codes, table row,
  placeholder validator, dedicated arm/wait/disarm runner, run-loop diversion),
  conditional `SELFTEST_EXECUTED_COUNT`.
- `Emu_3352_SPI/asr5k_spi_selftest.h` - macro default, conditional
  `ASR5K_SPI_SELFTEST_RECORD_COUNT`, appended (guarded) `ASR5K_SPI_TEST_ID_11`
  and `ASR5K_SPI_FAIL_STEP_PKTV1_WIRE`.
- `Emu_3352_SPI/docs/SPI_PACKET_V1_A6_3B_PASSIVE_HOOK_RESULT.md` - this document.

No other files changed. `spi_slave.h`, `SPI_master.*`, `wave_download.*`,
`cmd_id.h`, `.cproject`, `.project`, `.settings`, syscfg, linker, the CI workflow,
and `.agent` are untouched.

## 3. Compile guard behavior

The single guard is, defaulted at source level (NOT in `.cproject`):

```c
#ifndef SPI_PACKET_V1_WIRE_PROBE_ENABLE
#define SPI_PACKET_V1_WIRE_PROBE_ENABLE 0
#endif
```

It is defined (default 0) in both `asr5k_spi_selftest.h` and `spi_b_slave.c`. A
test build sets it to 1 globally (e.g. `--define=SPI_PACKET_V1_WIRE_PROBE_ENABLE=1`),
never by editing `.cproject`.

## 4. Default behavior when disabled (macro == 0)

Verified: nothing of A6-3B is compiled.

```text
no hook code, no recognizer instance, no accessors in spi_b_slave.c
no Test11, no Test11 enum members, no Test11 fault codes
ASR5K_SPI_SELFTEST_RECORD_COUNT == 10
SELFTEST_EXECUTED_COUNT == 10
Test1~Test10 behavior unchanged
completed_test_count == 10, implemented_test_count == 10
A6-1 Test10 unchanged; B01F Test1~Test9 unchanged
```

`pollReceiveFromSpi()` is byte-identical to base: the `#if` hook block is removed
by the preprocessor, so `SPIB_ParseRegFrame()` is called exactly as before.

## 5. Behavior when enabled (macro == 1)

```text
recognizer instance + arm flag + accessors compiled into spi_b_slave.c
hook active in pollReceiveFromSpi() only while armed
Test11 appended; RECORD_COUNT == 11, SELFTEST_EXECUTED_COUNT == 11
Test11 requires an EXTERNAL SPI master to send one Packet V1 PING frame
no PONG / no MISO response
```

The probe is armed ONLY during Test11; Test1~Test10 still run with the probe
disarmed, so they behave exactly as in the default build.

## 6. Hook location

In `pollReceiveFromSpi()`, inside the normal (non-wave) DMA-done branch, after the
2-word frame is latched (`u16Cmd`/`u16Data`) and `SPIB_RxDmaClearDone()` is
called, immediately BEFORE `bParseOk = SPIB_ParseRegFrame(u16Cmd, u16Data);`.

## 7. Consume-when-header-seen mode

When armed and (`u16Cmd == SPI_PACKET_V1_HEADER_MAGIC` OR the recognizer is already
`PKTV1_WIRE_PROBE_COLLECTING`):

- feed `{u16Cmd, u16Data}` to `SpiPacketV1_WireProbe_FeedWords(... , 2U)`;
- CONSUME the frame: it is NOT passed to `SPIB_ParseRegFrame()`, the old `0xA55A`
  packet FSM, or `SPIB_ParseLegacyRegFrame()`;
- count it parse-ok (`gSpibRxParseOkCount++`) so the health invariant
  `parse_ok == dma_done` holds across the gated receive window; it never bumps
  `gSpibRxParseFailCount`, never sets `SPIB_RX_ERR_FRAME_PARSE_FAIL`, and never
  calls protocol-fault reporting;
- re-arm RX DMA via the normal `SPIB_RxRestartRegFrameDma()` and return.

On recognizer error (bad CRC / bad length) the probe disarms; the consumed frame
is still not routed to the legacy parser, and no response is sent. Non-header,
non-collecting frames (and all frames when disarmed) fall through to the normal
legacy path unchanged.

## 8. No DMA change

The hook reads only already-received words and re-arms through the existing
`SPIB_RxRestartRegFrameDma()`. Unchanged: `SPIB_RX_REG_WORDS`, DMA channel, DMA
transfer length, DMA re-arm sequence, wave burst mode, post-wave command capture.

## 9. No SPIA master change

`SPIA_Master/SPI_master.*` is untouched. Test11 issues no SPIA master command; it
arms the SPIB probe and waits for an EXTERNAL master. The Test11 step-table entry
is `SPI_MASTER_TEST_CMD_NONE` and is never executed (the run loop diverts Test11
to a dedicated flow).

## 10. No PONG / no MISO response

The hook never writes the SPIB TX FIFO and never calls any response/PONG path. TX
behavior is untouched. This is passive receive only.

## 11. How Test11 works when enabled

Test11 is diverted in `Asr5kSpiSelfTest_Run()` to `selfTestRunPktV1Wire()`:

1. START: capture baseline counters, `SPIB_PacketV1WireProbe_Arm()` (arm + reset
   recognizer), enter a bounded wait.
2. WAIT: poll `SPIB_PacketV1WireProbe_GetSnapshot()`:
   - `FRAME_OK` with `cmd_id == 0x8000` and `payload_words == 0` -> PASS, disarm;
   - `FRAME_OK` with mismatched cmd/len, or `FRAME_ERROR` (bad CRC/len) -> FAIL
     (`SELFTEST_FAULT_PKTV1_WIRE_BAD_FRAME`), disarm;
   - still IDLE/COLLECTING -> increment a bounded wait counter.
3. TIMEOUT: when the bounded wait limit is exceeded -> FAIL
   (`SELFTEST_FAULT_PKTV1_WIRE_TIMEOUT`), disarm. Test11 can never hang.

The probe is disarmed on success, bad-frame, and timeout.

## 12. External frame required

When enabled, Test11 needs an external SPI master to clock one Packet V1 PING:

```text
0xA55A
0x8000
0x0000
CRC16_CCITT_FALSE(0xA55A, 0x8000, 0x0000)
```

The four words must be sent contiguously (no idle word injected mid-frame) so the
two DMA frames `{0xA55A,0x8000}` and `{0x0000,CRC16}` reach the recognizer back to
back. No UART, no SPIA master, no response expected.

## 13. Result encoding

```text
Test11 success: g_asr5kSpiSelfTest.test[10].actual = 0x8000504F
    0x8000 = received Packet V1 cmd_id
    0x504F = passive-receive-OK marker (NOT a MISO PONG)
Test11 bad frame: fault_code = 0x3019 (SELFTEST_FAULT_PKTV1_WIRE_BAD_FRAME)
Test11 timeout:   fault_code = 0x3018 (SELFTEST_FAULT_PKTV1_WIRE_TIMEOUT)
fail_step (both): ASR5K_SPI_FAIL_STEP_PKTV1_WIRE
```

`0x504F` here is a receive marker only; no PONG is transmitted.

## 14. Host test results

Unchanged from A6-3A (these compile only the pure-C `SPI_PacketV1/` sources, which
this task did not modify), all under `gcc -std=c99 -Wall -Wextra -Werror`:

```text
A2    PASS=126 FAIL=0
A4    PASS=104 FAIL=0
A5    PASS=107 FAIL=0
A6-1  PASS=14  FAIL=0
A6-3A PASS=87  FAIL=0
```

`asr5k_spi_selftest.h` was additionally host-compiled with gcc `-Werror` in both
modes: `ASR5K_SPI_SELFTEST_RECORD_COUNT` resolves to 10 (default) and 11
(enabled).

## 15. CCS build status

TI toolchain present (`C:/ti/ccs1281`, cl2000 22.6.1.LTS). Results:

```text
Default build (macro == 0):
  gmake -C Emu_3352_SPI/CPU1_RAM all  -> SUCCESS (exit 0)
  asr5k_spi_selftest.c, spi_b_slave.c, main.c recompiled; linked Emu_3352_SPI.out.

Enabled-path compile check (macro == 1), throwaway objects:
  cl2000 ... --define=SPI_PACKET_V1_WIRE_PROBE_ENABLE=1 asr5k_spi_selftest.c -> OK
  cl2000 ... --define=SPI_PACKET_V1_WIRE_PROBE_ENABLE=1 spi_b_slave.c        -> OK
  (both: exit 0, no errors/warnings; objects discarded, .cproject untouched)

CPU1_FLASH: no generated makefile in this checkout; CPU1_FLASH build not run.
```

## 16. Board wire status

```text
Board wire verification: NOT RUN
```

No external SPI master transaction was performed in this environment, so no wire
PASS is claimed. The default (macro == 0) firmware links and is unchanged in
behavior; the enabled (macro == 1) firmware compiles. Wire-level PASS requires a
bench run with an external master sending the Packet V1 PING frame and observing
`test[10].actual == 0x8000504F` with Test1~Test9 passing before and after.

## Appendix - next task

A7 - PING response (PONG) over the wire, only after this passive receive is
board-proven. Not part of A6-3B.
