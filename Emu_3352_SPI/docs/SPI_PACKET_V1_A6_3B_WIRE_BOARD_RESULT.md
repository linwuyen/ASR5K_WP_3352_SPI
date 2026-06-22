# SPI Packet V1 A6-3B-WIRE - Board Verification Result

Status: **BOARD WIRE VERIFICATION: NOT RUN**
Board-verifiable: requires physical hardware not available in this environment.
Scope: verification only (no source change). A7 remains BLOCKED.

Branch under test: `feature/spi-packet-v1-a6-3b-wire-board-verify`
Code under test (A6-3B implementation): `dc896e3f0b54dd605e128aadcecc15c56782d87b`
(this branch adds only this document on top of that commit)

ASCII-ONLY for MS950 / CCS compilation and review safety.

---

## 0. Headline

The three board phases of A6-3B-WIRE (flash + CCS-Watch trigger + EXTERNAL SPI
master transaction + live Watch observation) were **NOT executed**, because this
environment has:

- no physical F2838x board to flash and run,
- no external SPI master wired to the C2000 SPIB pins,
- no live JTAG / CCS debug session to set `g_asr5kSpiSelfTest.start` or read the
  Watch values.

Per the task's hard-stop rule, **no PASS is claimed**. This document records what
was verifiable in-environment (test-vector CRC, build readiness) and specifies the
exact bench procedure so the verification can be completed on real hardware.

---

## 1. Branch / commit under test

- Verification branch: `feature/spi-packet-v1-a6-3b-wire-board-verify`
- Base branch: `feature/spi-packet-v1-a6-3b-passive-hook`
- A6-3B implementation commit under test: `dc896e3f0b54dd605e128aadcecc15c56782d87b`
- No runtime source, `.cproject`, syscfg, or linker change in this task.

## 2. Board setup

**NOT RUN.** Intended setup (for the bench operator):

- TI F2838x control card (the existing Emu_3352_SPI target), CPU1.
- CCS 12.8.1 + XDS debug probe for flashing and Watch.
- An external SPI master device wired to the C2000 **SPIB** slave pins
  (SIMO/SOMI/CLK/STE) used by the existing slave path, common ground.
- No SPIA master used. No UART / Modbus used.

## 3. External SPI master source

**NONE available in this environment.** No external SPI master device was
connected or operated. (Required for the bench run: a real SPI master, e.g. a
second MCU / bench SPI host / logic-analyzer pattern generator capable of clocking
four contiguous 16-bit words.)

## 4. SPI mode / word size / clock rate used

**NOT RUN.** Required for the bench run: 16-bit data words, with SPI mode
(polarity/phase), bit order, and electrical levels matching the existing SPIB
slave configuration established by SysConfig `Board_init()` (the same settings the
legacy 2-word register path already uses). The master clocks; the C2000 SPIB is
the slave. DMA CH3 latches the words as two 2-word frames.

## 5. Packet to be sent

One Packet V1 PING frame, four contiguous 16-bit words (no idle word between
them):

```text
Word0 Header:      0xA55A
Word1 Command ID:  0x8000
Word2 Length:      0x0000
Word3 CRC16:       0x229D
```

Expected DMA grouping:

```text
DMA done #1: 0xA55A 0x8000
DMA done #2: 0x0000 0x229D
```

**Test-vector CRC verified in-environment** using the repository's own
`SpiPacketCrc16_ComputeWords()` and `SpiPacketV1_Encode()` (host, gcc -Werror):

```text
CRC16/CCITT-FALSE over {0xA55A, 0x8000, 0x0000} = 0x229D   (matches)
SpiPacketV1_Encode(0x8000, NULL, 0) -> 0xA55A 0x8000 0x0000 0x229D
```

So the frame above is the correct PING and the A6-3A recognizer would accept it
(cmd_id 0x8000, payload_words 0, CRC OK).

## 6. Phase 1 - default build baseline (macro == 0)

**BOARD RUN: NOT RUN** (no board to flash / no Watch session).

Build readiness (in-environment, not a board result): the default firmware
(`SPI_PACKET_V1_WIRE_PROBE_ENABLE == 0`) links cleanly via
`gmake -C Emu_3352_SPI/CPU1_RAM all` (exit 0, `Emu_3352_SPI.out` produced). In
this configuration `ASR5K_SPI_SELFTEST_RECORD_COUNT == 10`, Test11 is not
compiled, and the selftest is Test1~Test10 (confirmed by host-compiling the
header: RECORD_COUNT resolves to 10).

Watch values to capture on the bench (NOT captured here):

```text
status, result_text, completed_test_count(=10), implemented_test_count(=10),
current_test_id, failed_test_id(=0), failed_step, fault_code(=0),
test[0..9].status (all PASS), gSpibRxParseFailCount, gSpibRxErrorFlags(=0)
```

Phase 1 PASS criteria (to be evaluated on the bench): status=PASS,
completed=10, implemented=10, failed_test_id=0, fault_code=0, Test1~Test10 PASS,
gSpibRxErrorFlags=0.

## 7. Phase 2 - enabled build, Test11 wire receive (macro == 1)

**BOARD RUN: NOT RUN** (no board, no external SPI master, no Watch session).

Build readiness (in-environment, not a board result): with
`SPI_PACKET_V1_WIRE_PROBE_ENABLE=1` supplied as a local/uncommitted build define
(NOT via `.cproject`), `ASR5K_SPI_SELFTEST_RECORD_COUNT == 11` and both modified
translation units (`asr5k_spi_selftest.c`, `SPIB_Slave/spi_b_slave.c`) compile
cleanly under the TI C2000 compiler (verified at this same commit during the
A6-3B implementation task: cl2000 exit 0, no errors/warnings; objects discarded;
`.cproject` untouched).

Intended bench procedure (NOT executed):

1. Flash the macro=1 image.
2. Trigger via CCS Watch only: `g_asr5kSpiSelfTest.start = 1`.
3. Wait until `current_test_id == 11` and `test[10].status == RUNNING` (Test11 has
   armed the SPIB passive probe).
4. Send the four-word PING frame from the external master (contiguous, no idle
   word).
5. Capture the Watch values listed below.

Watch values to capture on the bench (NOT captured here):

```text
status, result_text, completed_test_count, implemented_test_count,
current_test_id, failed_test_id, failed_step, fault_code,
test[10].expected, test[10].actual, test[10].status, test[10].fail_step,
test[10].fault_code, test[10].delta.dma_done, test[10].delta.parse_ok,
test[10].delta.parse_fail, test[10].delta.dma_restart,
gSpibRxParseFailCount, gSpibRxErrorFlags
```

Expected Test11 success (to be evaluated on the bench):

```text
status=PASS, completed=11, implemented=11, failed_test_id=0, fault_code=0,
test[10].status=PASS, test[10].expected=0x8000504F, test[10].actual=0x8000504F,
test[10].delta.dma_done >= 2, test[10].delta.parse_ok >= 2,
test[10].delta.parse_fail = 0, gSpibRxErrorFlags = 0
```

Expected failure encodings (for diagnosis on the bench):

```text
timeout:   fault_code=0x3018, fail_step=ASR5K_SPI_FAIL_STEP_PKTV1_WIRE
bad frame: fault_code=0x3019, fail_step=ASR5K_SPI_FAIL_STEP_PKTV1_WIRE
```

Note: `0x8000504F` is a passive-receive marker only (cmd 0x8000 | 0x504F). It is
NOT proof that MISO sent a PONG; no PONG is expected or implemented in A6-3B.

## 8. Phase 3 - post-test legacy baseline (macro == 0)

**BOARD RUN: NOT RUN.** Intended: reflash the default (macro=0) image, trigger via
CCS Watch, and confirm Test1~Test10 PASS, completed=10, implemented=10,
failed_test_id=0, fault_code=0, gSpibRxErrorFlags=0 (proving no legacy/B01F
regression). Not executed here.

## 9. Exact Watch values

**NOT CAPTURED.** No live JTAG/CCS-Watch session was available, so no target
memory was read.

## 10. Parse counter deltas

**NOT CAPTURED.** `test[10].delta.dma_done / parse_ok / parse_fail / dma_restart`
and `gSpibRxParseFailCount` require a live board run.

## 11. Error flags

**NOT CAPTURED.** `gSpibRxErrorFlags` requires a live board run.

## 12. Conclusion

```text
BOARD WIRE RESULT: NOT RUN
```

- Phase 1 (default baseline): NOT RUN on board (firmware build-ready).
- Phase 2 (Test11 wire receive): NOT RUN on board (no external SPI master).
- Phase 3 (post-test baseline): NOT RUN on board.

No external SPI master transaction was performed; therefore PASS is not claimed
for any phase. The A6-3B code/build remains default-safe and build-verified; only
the hardware-in-the-loop wire verification is outstanding.

## 13. Explicit statement

```text
No PONG / MISO response tested or implemented.
A7 is still blocked until A6-3B-WIRE PASS.
```

## Appendix - what WAS verified in this environment (not board results)

```text
- Test-vector CRC: CRC16/CCITT-FALSE{0xA55A,0x8000,0x0000} = 0x229D (repo code).
- Encoder output: 0xA55A 0x8000 0x0000 0x229D (SpiPacketV1_Encode, host).
- Default (macro=0) firmware links: gmake CPU1_RAM all -> exit 0, .out produced.
- Header RECORD_COUNT resolves 10 (default) / 11 (enabled) under gcc -Werror.
- Enabled (macro=1) TUs compile clean under cl2000 (recorded in the A6-3B task,
  same commit dc896e3).
These are build / host checks only and are NOT a board wire PASS.
```
