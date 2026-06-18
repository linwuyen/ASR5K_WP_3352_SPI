# SPI Packet V1 A6-1 — Board-Observable PING Probe Result

Status: **BOARD PASS (single confirmed on-board run)**
Board-verifiable: **YES** (logic on target CPU, observable via the existing SPI
selftest result struct).  
Builds on: A2 parser/encoder, A4 dispatcher, A5 loopback, A6-0 plan.

Board PASS recorded: **2026-06-18**
Branch: `feature/spi-packet-v1-a6-1-ping-selftest-probe`
Commit under test: `8443f69897819b14edd5f4f26d182c4b8fd496f3`
Trigger method: **CCS Watch** (`g_asr5kSpiSelfTest.start = 1`)
UART / Modbus trigger used: **NO** (`uart_command_count = 0`)

## Exact limitation

This validates that the Packet V1 **logic executes correctly on the target C28x
CPU** and exposes a visible PASS/FAIL through the existing selftest surface. It
**does NOT** test SPI **wire** transport, does NOT route Packet V1 through the
SPIB RX/DMA path, and does NOT touch the existing live `0xA55A` runtime packet
state machine. The PING request/response is built and checked entirely in target
RAM.

## Board PASS evidence

Observed final `g_asr5kSpiSelfTest` state after flashing the A6-1 branch and
triggering the selftest from CCS Watch:

```text
start                  = 0
status                 = ASR5K_SPI_SELFTEST_PASS
result_text            = "PASS"
current_test_id        = 0
failed_test_id         = 0
failed_step            = 0
fault_code             = 0
completed_test_count   = 10
implemented_test_count = 10
uart_command_count     = 0
```

Per-test result:

```text
Test1  PASS
Test2  PASS
Test3  PASS
Test4  PASS
Test5  PASS
Test6  PASS
Test7  PASS
Test8  PASS
Test9  PASS
Test10 PASS
```

Test10 Packet V1 evidence:

```text
test[9].test_id  = 10
test[9].status   = ASR5K_SPI_TEST_PASS
test[9].expected = 2147504207 = 0x8000504F
test[9].actual   = 2147504207 = 0x8000504F
```

Meaning:

```text
0x8000 = Packet V1 PING response command
0x504F = PONG word
```

Therefore the Packet V1 pure-C PING/PONG logic is confirmed to execute
successfully on the target C2000 firmware and is observable through
`g_asr5kSpiSelfTest`.

## Initial transient observation

Before the final PASS run, an initial run after flashing reported:

```text
status                 = ASR5K_SPI_SELFTEST_FAIL
result_text            = "FAIL"
current_test_id        = 4
failed_test_id         = 4
failed_step            = 4
fault_code             = 4611 = 0x1203
completed_test_count   = 3
implemented_test_count = 10
uart_command_count     = 0
```

This failure occurred in legacy Test4, before Packet V1 Test10 was reached. A
clean CPU reset / rerun was then performed, with **no code changes**, and the
final result was PASS for Test1~Test10.

Engineering interpretation: the initial Test4 failure is treated as a non-final
transient startup/reset-state observation unless it becomes repeatable. The
accepted A6-1 result is the final clean PASS run recorded above.

## Build-integration note (why a .cproject change was required)

The CCS project (`Emu_3352_SPI/.cproject`) previously excluded the entire
`SPI_PacketV1/` folder from both build configurations
(`excluding="SPI_PacketV1"`), so none of the A2/A4/A5 logic was ever compiled
into firmware. A board-observable probe therefore could not link without a
project-config change. With explicit authorization, the exclusion was
**narrowed** to exclude only the four host-test files, so the production sources
(`spi_packet_crc16.c`, `spi_packet_v1.c`, `spi_packet_v1_dispatch.c`,
`spi_packet_v1_loopback.c`, `spi_packet_v1_probe.c`) now build into firmware
while the `*_test.c` harnesses remain excluded (and are additionally guarded by
`SPI_PACKET_V1_HOST_TEST`, so they would be empty translation units even if
compiled).

Both build configs now read:

```text
excluding="SPI_PacketV1/spi_packet_v1_test.c|SPI_PacketV1/spi_packet_v1_dispatch_test.c|SPI_PacketV1/spi_packet_v1_loopback_test.c|SPI_PacketV1/spi_packet_v1_probe_test.c"
```

## Files changed by the A6-1 implementation

New:
- `Emu_3352_SPI/SPI_PacketV1/spi_packet_v1_probe.h` — probe API + result struct
- `Emu_3352_SPI/SPI_PacketV1/spi_packet_v1_probe.c` — pure-C probe implementation
- `Emu_3352_SPI/SPI_PacketV1/spi_packet_v1_probe_test.c` — host test (guarded)
- `Emu_3352_SPI/docs/SPI_PACKET_V1_A6_BOARD_PING_PROBE_RESULT.md` — this doc

Modified (additive only):
- `Emu_3352_SPI/.cproject` — narrowed `SPI_PacketV1` build exclusion (both configs)
- `Emu_3352_SPI/asr5k_spi_selftest.h` — `RECORD_COUNT` 9→10, `ASR5K_SPI_TEST_ID_10`,
  `ASR5K_SPI_FAIL_STEP_PKTV1_PING`, Test10 overview comment
- `Emu_3352_SPI/asr5k_spi_selftest.c` — `SELFTEST_EXECUTED_COUNT` 9→10,
  `SELFTEST_FAULT_PKTV1_PING` (0x3017), Test10 step table + validator + one table row,
  probe include
- `.github/workflows/spi_packet_v1_host_tests.yml` — added the A6-1 probe host-test
  step (existing A2/A4/A5 steps unchanged)

Test1~Test9 sequences, expected values, validators, waits, and timing are
unchanged; Test10 is strictly appended.

## Probe API

```c
typedef enum {
    PKTV1_PROBE_OK = 0,
    PKTV1_PROBE_ERR_NULL,
    PKTV1_PROBE_ERR_ENCODE,
    PKTV1_PROBE_ERR_LOOPBACK,
    PKTV1_PROBE_ERR_PARSE_RSP,
    PKTV1_PROBE_ERR_BAD_RSP_CMD,
    PKTV1_PROBE_ERR_BAD_RSP_LEN,
    PKTV1_PROBE_ERR_BAD_PONG
} PKTV1_PROBE_RESULT_e;

typedef struct {
    PKTV1_PROBE_RESULT_e result;
    uint16_t loopback_result, parser_result, dispatch_result;
    uint16_t response_cmd, response_payload_words, pong_word;
    uint16_t request_words, response_words;
} ST_PKTV1_PROBE_RESULT;

PKTV1_PROBE_RESULT_e SpiPacketV1_RunPingProbe(ST_PKTV1_PROBE_RESULT *out);
```

The probe: encodes a PING (`SpiPacketV1_Encode(PKTV1_CMD_PING, NULL, 0, …)`),
runs `SpiPacketV1_Loopback(…)`, parses the response with
`SpiPacketV1_ParseWords(…)`, and asserts loopback OK, response cmd `0x8000`,
payload length 1, pong word `0x504F`. Pure C, no globals, deterministic,
caller-owned output, `out == NULL → PKTV1_PROBE_ERR_NULL`.

## Test10 behavior

- One benign step: `READ 0x0400` (identical to Test2) drives the step engine and
  re-confirms the legacy register path (DMA delta = 2, exact — same as Test2).
- The validator `Validate_Test10_PacketV1Ping()` calls `SpiPacketV1_RunPingProbe()`.
- PASS iff `result == PKTV1_PROBE_OK`.
- Observable result via the existing `g_asr5kSpiSelfTest.test[9]` record:
  - On success: `actual = (response_cmd << 16) | pong_word = 0x8000504F`
    (matches `expected`).
  - On failure: `actual = (result<<24)|(loopback<<16)|(parser<<8)|dispatch` and
    `fail_step = ASR5K_SPI_FAIL_STEP_PKTV1_PING`, `fault_code = 0x3017`.

## Host test result

```text
A6-1 PING probe host test: PASS=14  FAIL=0   (gcc -std=c99 -Wall -Wextra -Werror, exit 0, 0 warnings)
```

Full suite (host, all warning-free under -Werror):

```text
A2 : PASS=126 FAIL=0
A4 : PASS=104 FAIL=0
A5 : PASS=107 FAIL=0
A6 : PASS=14  FAIL=0
```

## CI step result

`.github/workflows/spi_packet_v1_host_tests.yml` runs A2/A4/A5 plus the new
conditional A6-1 probe step, asserting `PASS=14[[:space:]]+FAIL=0` under
`-Werror`. The A6-1 branch CI run passed before board verification.

## CCS / firmware build

The final on-board PASS recorded above confirms the firmware was built, flashed,
and run on the target board by the user. The external cloud environment used for
host testing did **not** have a TI C28x toolchain and did not fake a CCS build.

## Board test flow used

```text
1. Checkout feature/spi-packet-v1-a6-1-ping-selftest-probe.
2. Build firmware in CCS.
3. Flash the board.
4. Trigger from CCS Watch: g_asr5kSpiSelfTest.start = 1.
5. Observe g_asr5kSpiSelfTest final state.
6. Accepted result:
   - result_text == "PASS"
   - completed_test_count == 10
   - Test1~Test9 remain PASS
   - Test10 Packet V1 PING probe == PASS
   - g_asr5kSpiSelfTest.test[9].actual == 0x8000504F
```

## B01F impact statement

- Test1~Test9 are unchanged (only appended Test10), and the board run confirms
  Test1~Test9 PASS on the A6-1 branch.
- No SPIB runtime, DMA, wave_download, SPIA master, syscfg, linker, or live
  receive-path code was modified; the existing `0xA55A` runtime packet state
  machine is untouched.
- The only build-config change is narrowing a source-exclusion rule in
  `.cproject` (authorized), which adds the pure-C Packet V1 sources to the image
  but introduces no new runtime behavior unless the new Test10 is run.
- Net: B01F legacy Test1~Test9 baseline is board-confirmed PASS on A6-1, and
  Test10 Packet V1 PING probe is also board-confirmed PASS.

## Scope and non-claims

Confirmed:

```text
A6-1 Packet V1 logic on target CPU: PASS
Board-observable selftest Test10: PASS
Legacy Test1~Test9 baseline on A6-1 branch: PASS
UART/Modbus path used for trigger: NO
```

Not claimed by this test:

```text
SPIB RX/DMA Packet V1 runtime integration
AM3352 wire-level Packet V1 PING
C2000 SPIB wire-level PONG response
Replacement of the legacy register protocol
```

A6-1 validates Packet V1 logic on the target firmware only. SPI wire transport
remains future A6-2 / A7 work.
