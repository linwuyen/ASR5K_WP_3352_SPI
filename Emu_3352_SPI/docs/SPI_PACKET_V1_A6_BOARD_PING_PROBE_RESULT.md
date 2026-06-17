# SPI Packet V1 A6-1 — Board-Observable PING Probe Result

Status: **Implemented (pure-C probe + Test10 selftest integration + host test + CI)**
Board-verifiable: **YES** (logic on target CPU, observable via the existing SPI
selftest result struct + UART).
Builds on: A2 parser/encoder, A4 dispatcher, A5 loopback, A6-0 plan.

## Exact limitation

This validates that the Packet V1 **logic executes correctly on the target C28x
CPU** and exposes a visible PASS/FAIL through the existing selftest surface. It
**does NOT** test SPI **wire** transport, does NOT route Packet V1 through the
SPIB RX/DMA path, and does NOT touch the existing live `0xA55A` runtime packet
state machine. The PING request/response is built and checked entirely in target
RAM.

## Build-integration note (why a .cproject change was required)

The CCS project (`Emu_3352_SPI/.cproject`) previously excluded the entire
`SPI_PacketV1/` folder from both build configurations
(`excluding="SPI_PacketV1"`), so none of the A2/A4/A5 logic was ever compiled
into firmware. A board-observable probe therefore could not link without a
project-config change. With explicit authorization, the exclusion was **narrowed**
to exclude only the four host-test files, so the production sources
(`spi_packet_crc16.c`, `spi_packet_v1.c`, `spi_packet_v1_dispatch.c`,
`spi_packet_v1_loopback.c`, `spi_packet_v1_probe.c`) now build into firmware while
the `*_test.c` harnesses remain excluded (and are additionally guarded by
`SPI_PACKET_V1_HOST_TEST`, so they would be empty translation units even if
compiled).

Both build configs now read:

```
excluding="SPI_PacketV1/spi_packet_v1_test.c|SPI_PacketV1/spi_packet_v1_dispatch_test.c|SPI_PacketV1/spi_packet_v1_loopback_test.c|SPI_PacketV1/spi_packet_v1_probe_test.c"
```

## Files changed

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
`-Werror`. (Pushing the branch triggers the workflow; observe the run on GitHub.)

## CCS / firmware build

The host tests pass locally. A native CCS/TI C28x firmware build was **not run**
in this environment (no TI toolchain available here) and is **not faked**. The
Packet V1 sources are written C28x-safe (no `uint8_t`; `uint16_t`-based CRC; word
counts, never `sizeof()/2`), but the actual firmware build and the on-board
result must be verified by the user in CCS.

## Board test flow (user)

```text
1. git checkout feature/spi-packet-v1-a6-1-ping-selftest-probe
2. Build firmware in CCS (CPU1_RAM or CPU1_FLASH config).
3. Flash the board.
4. Open the UART terminal used by the existing SPI selftest.
5. Trigger the selftest, e.g. send "spi_test all".
6. Expected:
   - UART final result = PASS
   - g_asr5kSpiSelfTest.result_text == "PASS"
   - Test1~Test9 remain PASS
   - Test10 Packet V1 PING probe == PASS
   - g_asr5kSpiSelfTest.test[9].actual == 0x8000504F  (response_cmd 0x8000, pong 0x504F)
7. On failure, capture:
   - g_asr5kSpiSelfTest.status / result_text
   - g_asr5kSpiSelfTest.current_test_id / completed_test_count
   - g_asr5kSpiSelfTest.failed_test_id / failed_step / fault_code (0x3017 = PING probe)
   - g_asr5kSpiSelfTest.test[9].actual  (packed: result/loopback/parser/dispatch)
   - g_asr5kSpiSelfTest.test[9].fail_step
```

## B01F impact statement

- Test1~Test9 are unchanged (only appended Test10), so the B01F baseline
  behavior, expected values, and timing are preserved.
- No SPIB runtime, DMA, wave_download, SPIA master, syscfg, linker, or live
  receive-path code was modified; the existing `0xA55A` runtime packet state
  machine is untouched.
- The only build-config change is narrowing a source-exclusion rule in
  `.cproject` (authorized), which adds the pure-C Packet V1 sources to the image
  but introduces no new runtime behavior unless the new Test10 is run.
- Net: B01F Test1~Test9 should remain PASS; the user must confirm on-board after
  a CCS build.
