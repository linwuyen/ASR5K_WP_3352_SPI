# SPI Packet V1 A6-2 - Wire-Level Probe Plan (Design / Audit Only)

Status: **DESIGN / PLAN ONLY - NOT board-verifiable**
Board-verifiable: **NO** (this task produces no runtime code and changes no
firmware behavior).
Type: design / audit / planning document.
Builds on: A2 parser/encoder, A4 dispatcher, A5 loopback, A6-0 plan, A6-1
board-observable PING probe.

Base branch: `feature/spi-packet-v1-a6-1-ping-selftest-probe`
Base commit: `20b72ac53234a62f0c5da84a63ede7df8518b419`
This branch: `feature/spi-packet-v1-a6-2-wire-probe-plan`

ASCII-ONLY for MS950 / CCS compilation and review safety.

---

## 0. Scope and hard boundaries of this task

This document is the next planning step toward Packet V1 **wire-level** testing.
It does not implement, modify, or authorize any runtime change. It exists to
make the next implementation step safe, small, and reversible.

This task explicitly does NOT:

- modify SPIB runtime (`spi_b_slave.*`, `spi_slave.h`, `wave_download.*`);
- modify the SPIA master (`SPI_master.*`);
- modify DMA configuration or DMA frame strategy;
- modify SysConfig, linker command files, or `.cproject`;
- modify `cmd_id.h` or any legacy command definitions;
- add or change runtime code, ISRs, or global diagnostics;
- open a pull request or merge anything.

The only deliverable is this single Markdown file.

---

## 1. What A6-1 actually proved

A6-1 is **closed** and remains closed. Its confirmed claims:

- **Packet V1 logic runs on the target C28x CPU.** The pure-C encode / parse /
  loopback / dispatch / probe sources now link into firmware (the `.cproject`
  exclusion was narrowed to exclude only the four `*_test.c` host harnesses).
- **Test10 in-memory PING/PONG PASS.** `Validate_Test10_PacketV1Ping()` calls
  `SpiPacketV1_RunPingProbe()`, which encodes a PING, runs an in-RAM loopback,
  parses the response, and asserts response cmd `0x8000`, payload length 1, and
  pong word `0x504F`.
- **Board-observable through the existing selftest surface.** On success,
  `g_asr5kSpiSelfTest.test[9].actual == 0x8000504F` (`response_cmd << 16 |
  pong_word`), matching `expected`. `completed_test_count == 10`.
- **Legacy Test1~Test9 baseline still PASS** on the A6-1 branch; Test10 is
  strictly appended (Test2-equivalent benign `READ 0x0400`, DMA delta = 2).
- **Trigger was CCS Watch, not UART.** `g_asr5kSpiSelfTest.start = 1` set from
  the debugger; `uart_command_count == 0`. UART / Modbus was not used.

What A6-1 did **NOT** prove (the gap this plan addresses):

- No Packet V1 frame ever crossed the **SPI wire**.
- Packet V1 was never routed through the **SPIB RX / DMA** receive path.
- No real SPI master ever clocked a `0xA55A` frame into the slave.
- The slave never shifted a PONG back over MISO under master clock.
- The PING request and PONG response were built and checked entirely in target
  RAM by one CPU.

A6-1 validates Packet V1 **logic on the target CPU only**. SPI **wire
transport** is unproven.

---

## 2. What A6-2 / A7 must eventually prove

The wire-level goal (future tasks, not this one) is to show, on the bench:

1. A real SPI master emits one Packet V1 frame (`w0 = 0xA55A`, cmd, length,
   payload, CRC16/CCITT-FALSE) over MOSI under master clock and CS timing.
2. The C2000 SPIB slave **receives** that frame without breaking legacy traffic.
3. The `0xA55A` Packet V1 header is recognized **only** inside an isolated /
   protected path, never by widening or mutating the legacy 2-word register
   parser.
4. A PING response (`0x8000` / `0x504F`) can be **observed** - first as a slave
   recognition event, and only later (A7) as an actual MISO PONG.
5. Legacy B01F **Test1~Test9 still PASS** after the wire test, byte-for-byte
   unchanged in sequence, expected values, and DMA deltas.

A6-2 itself proves none of the above on hardware. A6-2 only fixes the safe path
and acceptance criteria so the first wire task is low-risk.

---

## 3. Why direct runtime integration is dangerous

Packet V1 wire reception cannot simply be "turned on" in the live SPIB path.
The following facts make naive integration high-risk:

- **The master owns timing, not the slave.** The AM3352 / SPIA master drives
  SPICLK and CS. A C2000 SPIB slave can only shift out whatever already sits in
  its TX FIFO when the master clocks. A PONG response is therefore
  clock-coupled to the master transaction and cannot be produced "late."
- **Legacy framing is 2 words, fixed.** `SPIB_RX_REG_WORDS == 2`. DMA CH3
  receives exactly 2 words per legacy transfer (ping-pong, polled done, DMA
  re-armed before parse). A Packet V1 frame is `4 + N` words. Any change to the
  CH3 RX frame length to "fit packets" would corrupt the legacy register cadence
  and break B01F Test1~Test9.
- **The wave burst path has special DMA behavior.** The wave block DMA captures
  `WAVE_BURST_SAMPLE_COUNT` (4096) frames = 8192 words spanning two physically
  adjacent RAMGS arrays, with a bounded force-trigger re-arm, a post-wave
  32-frame command-capture buffer, and a transfer-count stall watchdog. This
  path is fragile by design; a packet recognizer must never run inside or race
  it.
- **`0xA55A` is already a live header in this codebase.** It is NOT a free magic
  number:
  - `SPIB_Slave/spi_slave.h` defines `SPIB_PACKET_HEADER_MAGIC = 0xA55A`,
    `SPIB_PACKET_PADDING_WORD = 0x0000`, a `SPIB_PACKET_STATE_e` FSM
    (`IDLE/CMD/LENGTH/PAYLOAD/CHECKSUM`), and packet error codes. The runtime
    struct carries `ePacketState`, `u16PacketCmd`, `u16PacketLength`,
    `u16PacketErrorCode`. The header comment states legacy 2-word frame and
    packet mode **share handlers**.
  - `SPIA_Master/SPI_master.h` defines `SPI_PACKET_HEADER_MAGIC = 0xA55A`, a
    `SPI_APP_STATE_PACKET_WRITE` / `SPI_MASTER_TEST_CMD_PACKET_WRITE` path, and a
    protocol FSM with `BAD_HEADER` / `BAD_LENGTH` / `CHECKSUM` / `PADDING`
    faults.
  - This **pre-existing emulator "packet" path uses a byte-sum checksum and a
    padding word** - it is a DIFFERENT framing from the new pure-C Packet V1,
    which uses **CRC16/CCITT-FALSE** and a 4-word overhead (header, cmd, length,
    crc). The A6-1 note that it "does not touch the existing live `0xA55A`
    runtime packet state machine" refers to this older path.
  - Consequence: a Packet V1 wire probe must not be confused with, and must not
    collide with, the existing `0xA55A` SPIB packet FSM. Two different consumers
    of the same header on one wire is the principal integration hazard.
- **Header collision against legacy register traffic.** If `0xA55A` arrives as
  the first word of a 2-word legacy frame, the legacy parser would treat it as a
  register address. Recognition must be gated so it can never alter legacy
  register decode or its `parse_fail` accounting.

---

## 4. Candidate strategies

At least four options were considered. Each is scored on what it touches and
what it risks.

### Option A - selftest-only SPIA raw Packet V1 wire probe

- The on-board SPIA master emits one Packet V1 PING frame (`0xA55A`, cmd,
  length, payload, CRC16) over the physical wire to SPIB.
- SPIB receives it through a **new isolated probe path**, separate from the
  legacy parser and from the existing `0xA55A` packet FSM.
- Must NOT replace or widen the legacy register parser.
- Pros: fully self-contained on one board; reuses the existing selftest trigger
  and `g_asr5kSpiSelfTest` observation surface; no external tooling.
- Risk: requires touching **both** the SPIA master (to emit the exact CRC16
  frame, which the existing PACKET_WRITE path does not produce - it uses a
  byte-sum checksum) **and** the SPIB RX path (to recognize it). That is two
  runtime edits on the most sensitive modules, plus disambiguation from the
  existing packet FSM. High blast radius if done in one step.

### Option B - external AM3352 / external SPI master wire probe

- No SPIA master changes. The Packet V1 frame is emitted from the actual master
  side (AM3352 or a bench SPI host / logic-analyzer pattern generator).
- SPIB still needs a passive way to observe the bytes, but the transmit side is
  external and authoritative.
- Pros: no SPIA master edit; exercises the real master that production will use;
  proves true wire compatibility rather than a C2000-to-C2000 loopback.
- Risk: needs external tooling and exact transaction framing (CS boundaries,
  word order, clock polarity/phase, inter-word gaps). Harder to make repeatable
  in CI; depends on hardware availability. Still needs a non-invasive SPIB-side
  observer.

### Option C - staged SPIB passive recognizer

- SPIB runtime only **detects** the `0xA55A` Packet V1 header on the wire and
  records a non-invasive counter / result (e.g. "saw a candidate Packet V1
  header", header word, declared length, CRC-OK flag). **No response** is sent.
- The recognizer must be behind an explicit selftest gate or compile-time guard,
  and must run without changing the legacy 2-word frame decode, DMA frame
  length, or the existing packet FSM.
- Pros: smallest possible runtime footprint that still produces wire evidence;
  observation-only means no master-clock response coupling; reversible via the
  guard.
- Risk: still touches SPIB runtime (the highest-sensitivity module), so it must
  be gated, minimal, and regression-checked against B01F. Must coexist with the
  existing `0xA55A` packet FSM without ambiguity.

### Option D - do nothing further in runtime until a full transport spec exists

- Keep all Packet V1 work in pure-C / selftest space (where A6-1 already lives)
  and write a complete wire transport specification first.
- Pros: zero runtime risk; forces the master/slave framing, CS, and arbitration
  questions to be answered on paper before any silicon change.
- Risk: does not advance wire proof at all; the gap from "logic PASS" to "wire
  PASS" stays open indefinitely.

### Option comparison

```text
Option | SPIA master | SPIB runtime | DMA | External HW | Wire proof | Risk
-------+-------------+--------------+-----+-------------+------------+------
A      | YES (emit)  | YES (recv)   | no* | no          | full RTT   | HIGH
B      | no          | YES (observe)| no* | YES         | full wire  | MED-HIGH
C      | no          | YES (detect) | no* | YES (master)| receive    | MED (gated)
D      | no          | no           | no  | no          | none       | ZERO
```

`no*` = none of A/B/C may change DMA frame strategy; any such change is a
separate, separately-approved task (see section 6 and 8).

---

## 5. Recommended safest next implementation

Recommendation is deliberately conservative and **staged**. Do not jump to a
full request/response wire integration.

```text
A6-2  (this doc) : document-only plan. No runtime change.            [now]
A6-3 / A7-0      : passive wire recognizer only (Option C), board-   [next]
                   gated, NO response. Prove SPIB can SEE a 0xA55A
                   Packet V1 frame on the wire without disturbing
                   B01F or the existing packet FSM.
A7               : PING response (PONG) only AFTER passive receive    [later]
                   is proven. Adds the clock-coupled MISO response
                   under explicit gating.
```

Rationale:

- The riskiest unknown is "can the slave observe a wire-delivered `0xA55A`
  Packet V1 frame at all, without perturbing legacy traffic." Prove that in
  isolation (passive, no response) before adding response timing.
- Passive receive (Option C) has no master-clock response coupling, so it
  removes one entire failure class from the first wire test.
- For the **transmit** side of A6-3/A7-0, prefer Option B (external/real master)
  or a minimal, gated reuse of the SPIA master; do not co-mingle a new CRC16
  emitter into the existing byte-sum PACKET_WRITE path.
- A response (A7) is added only once passive receive is board-confirmed.

This plan does **not** recommend replacing the legacy register protocol at any
stage. Packet V1 is additive and isolated.

---

## 6. Required isolation rules for future implementation

Any A6-3 / A7-0 / A7 implementation MUST observe all of the following:

- **Packet V1 stays isolated from the legacy B01F path.** No change to the
  legacy 2-word register decode, its expected values, its waits, or its
  `parse_fail` / DMA-delta accounting.
- **No SysConfig / linker / `.cproject` structural changes** beyond what A6-1
  already did (source-exclusion narrowing). No new build configs, no pin/clock
  changes, no section remaps of the wave RAMGS arrays.
- **No SPIB DMA strategy change** (frame length, channel, ping-pong, wave-block
  behavior) unless that DMA change is its own separately approved task.
- **No `OUTPUT_ON` / power-stage interaction.** The probe must be inert with
  respect to power output and must not depend on or toggle output state.
- **No OTP / BOOTCFG access** of any kind.
- **No new global diagnostics** unless explicitly justified in that task's plan.
  Prefer reusing the existing `g_asr5kSpiSelfTest` surface (e.g. a new appended
  Test record) for observation rather than adding free-floating globals.
- **Disambiguate from the existing `0xA55A` packet FSM.** The new Packet V1
  recognizer (CRC16) must not be conflated with the existing SPIB packet FSM
  (byte-sum checksum). The plan for that task must state explicitly which
  consumer owns the wire during the probe and how the other is held inert.
- **UART / Modbus must NOT be used as Packet V1 transport** or as the trigger of
  record. The selftest trigger remains CCS Watch (`g_asr5kSpiSelfTest.start`),
  consistent with A6-1 (`uart_command_count == 0`).
- **The recognizer must be gated** (selftest gate or compile-time guard) so the
  default firmware image behaves exactly like the legacy baseline when the probe
  is not armed.

---

## 7. Proposed future board acceptance criteria

These define PASS for the **first wire-level task** (A6-3 / A7-0 passive
recognizer). A response-bearing task (A7) adds the PONG criterion.

Mandatory for the first wire task:

- **Legacy regression intact:** Test1~Test9 PASS **before** and **after** the
  wire test, with unchanged sequences and DMA deltas (e.g. Test2 `READ 0x0400`
  DMA delta = 2, exact).
- **Packet V1 receive observation PASS:** SPIB records a valid wire-delivered
  `0xA55A` Packet V1 frame (header recognized, declared length consistent, CRC16
  verified) through the gated probe surface.
- **No legacy `parse_fail` increase** on the legacy tests attributable to the
  probe.
- **No DMA overflow / no RX error flags** raised by the probe path
  (`gSpibRxErrorFlags` unchanged by legacy traffic).
- **No wave download regression:** wave burst transport unchanged and still
  passing.
- **No Modbus / UART involvement:** `uart_command_count == 0`; trigger via CCS
  Watch.

Additional, only if the task includes a response (A7):

- **PING response observed as `0x8000504F`** (response cmd `0x8000`, pong word
  `0x504F`) - first on the slave-side record, and where instrumented, as an
  actual MISO PONG under master clock.

A run is accepted only when the legacy-before and legacy-after checks both pass;
a wire-receive PASS that regresses any legacy test is a FAIL overall.

---

## 8. Reject conditions

The plan explicitly **rejects** any future implementation that:

- **Replaces or supersedes the legacy register protocol** with Packet V1.
- **Broadly changes SPIB DMA CH3 behavior** (frame length, ping-pong scheme,
  trigger model) to accommodate packets.
- **Changes the wave burst transport** (block DMA span, RAMGS adjacency,
  force-trigger, post-wave capture, stall watchdog).
- **Requires SysConfig / linker / `.cproject` structural edits** beyond the
  already-authorized A6-1 source-exclusion narrowing.
- **Uses UART / Modbus as the Packet V1 transport.**
- **Requires the power stage or `OUTPUT_ON`** to be active, or interacts with
  output state.
- **Claims AM3352 wire compatibility without an actual wire test** - i.e. a
  C2000-to-C2000 loopback or an in-RAM result must never be reported as
  AM3352-master wire validation.
- **Adds an ungated runtime recognizer** that alters default-image behavior.

Any task hitting a reject condition must stop and be re-scoped / re-approved
before implementation.

---

## 9. Deliverables

Exactly one file is produced by A6-2:

```text
Emu_3352_SPI/docs/SPI_PACKET_V1_A6_2_WIRE_PROBE_PLAN.md
```

No source code is changed. No SPIB runtime, DMA, SysConfig, linker, `.cproject`,
`cmd_id.h`, `wave_download.*`, `SPI_master.*`, or `spi_b_slave.*` file is
touched. No PR is opened and nothing is merged.

---

## Appendix A - grounding references (read-only analysis)

- `docs/SPI_PACKET_V1_A6_BOARD_PING_PROBE_RESULT.md` - A6-1 board PASS,
  `test[9].actual == 0x8000504F`, CCS Watch trigger, `uart_command_count == 0`.
- `SPI_PacketV1/spi_packet_v1.h` - Packet V1 frame: `w0 = 0xA55A`, cmd, length
  N, payload, CRC16/CCITT-FALSE over `w0..2+N`; total `4 + N` words.
- `SPIB_Slave/spi_slave.h` - `SPIB_RX_REG_WORDS == 2`;
  `SPIB_PACKET_HEADER_MAGIC == 0xA55A`; `SPIB_PACKET_STATE_e` FSM; "legacy
  2-word frame and packet mode share handlers."
- `SPIB_Slave/spi_b_slave.c` - DMA CH3 2-word legacy frame ping-pong; wave block
  DMA (8192 words across adjacent RAMGS arrays), force-trigger, post-wave 32
  command-frame capture, stall watchdog.
- `SPIA_Master/SPI_master.h` - `SPI_PACKET_HEADER_MAGIC == 0xA55A`,
  `SPI_MASTER_TEST_CMD_PACKET_WRITE`, block / wave transfer, raw-frame stress,
  `startSPIAmasterTest()`; existing master "packet" uses a byte-sum checksum,
  not CRC16.
- `asr5k_spi_selftest.h/.c` - `g_asr5kSpiSelfTest` surface, Test10 record
  (`test[9]`), `RECORD_COUNT == 10`.

## Appendix B - A6-1 closure statement

A6-1 remains **CLOSED** and is not reopened, reinterpreted, or invalidated by
this plan. A6-2 is additive design work layered on top of the A6-1 result.
