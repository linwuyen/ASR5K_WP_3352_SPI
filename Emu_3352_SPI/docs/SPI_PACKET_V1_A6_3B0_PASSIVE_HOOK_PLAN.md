# SPI Packet V1 A6-3B0 - Gated SPIB Passive Wire Hook Implementation Plan (Design Only)

Status: **PLAN ONLY - NO CODE, NOT board-verifiable**
Board-verifiable: **NO** (this task changes no firmware behavior)
Type: design / audit / planning document.
Builds on: A6-1 board PING probe, A6-2 wire probe plan, A6-3A pure-C recognizer core.

Base branch: `feature/spi-packet-v1-a6-3a-wire-probe-core`
Base commit: `1609709660e0ea026e4be5be388440b7c73941c0`
This branch: `feature/spi-packet-v1-a6-3b0-passive-hook-plan`

ASCII-ONLY for MS950 / CCS compilation and review safety.

All line numbers below are as of base commit `1609709` in
`Emu_3352_SPI/SPIB_Slave/spi_b_slave.c` (1997 lines) and will shift once the file
is edited; function names are the stable anchors.

---

## 0. Scope and hard boundaries of this task

This document prepares the exact implementation plan for the next task
(A6-3B / A7-0): a **gated, passive, detect-only** SPIB wire hook that feeds the
already-proven A6-3A recognizer from frames the SPIB RX path already receives.

This task implements nothing. It does NOT modify `spi_b_slave.c`, `spi_slave.h`,
`SPI_master.*`, `wave_download.*`, `cmd_id.h`, SPIB DMA, wave burst behavior,
syscfg, linker, or `.cproject`; it adds no CCS Watch variable, no global
diagnostic, no Test11, uses no UART/Modbus, and sends no PONG. The only
deliverable is this one Markdown file.

---

## 1. Where exactly can the hook live?

The single RX entry point is `pollReceiveFromSpi()` (def line 1590). On each DMA
"done" for a normal (non-wave) frame, the body at lines 1656-1714 already exposes
everything the hook needs.

### 1.1 Where the 2-word RX frame is available

`pollReceiveFromSpi()`, lines 1665-1667:

```c
pFrame  = SpibPingPong_SwapAndGetFrame(&s_rxPingPong);   /* 1665 */
u16Cmd  = pFrame[0];                                      /* 1666 */
u16Data = pFrame[1];                                      /* 1667 */
```

This is the canonical, already-latched two-word legacy frame
(`SPIB_RX_REG_WORDS == 2`). `s_u16LastRxCmd` / `s_u16LastRxData` mirror it
(1669-1670). This is the only place the hook should read words from.

### 1.2 Where legacy parse currently occurs

Line 1687:

```c
bParseOk = SPIB_ParseRegFrame(u16Cmd, u16Data);          /* 1687 */
```

`SPIB_ParseRegFrame()` (def 1418) is the dispatcher; it routes to either the
existing `0xA55A` packet FSM or `SPIB_ParseLegacyRegFrame()` (def 1256, the
fast-path + fallback-FIFO register path). The candidate hook point is the few
lines immediately before this call (after `SPIB_RxDmaClearDone()` at 1685).

### 1.3 Where DMA re-arm occurs

- Normal frame re-arm: `SPIB_RxRestartRegFrameDma()` at line 1710 (def 472).
- Wave-entry pre-arm (preamble seen): `SPIB_RxWaveEnterBlockMode()` at line 1706.
- Post-wave re-arm onto the command buffer: lines 1640-1642
  (`SPIB_RxDma_Configure(... SPIB_RX_REG_WORDS, SPIB_POST_WAVE_CMD_FRAMES)`).

The hook must sit BEFORE the re-arm (it only reads words; it must not change the
re-arm sequence, channel, length, or trigger).

### 1.4 Where wave burst mode branches away

- DMA-done wave branch: `if (s_bSpibRxInWaveMode)` at line 1630 (handles block
  completion; the hook must NOT run here).
- Sliced wave-block parse early return: lines 1613-1617
  (`if (g_u16WaveRawParsePending != 0U) { SPIB_RxWaveParseChunk(); return; }`).
- Wave-begin transition after a normal frame: `s_bWaveBurstBegin` at line 1701
  -> `SPIB_RxWaveEnterBlockMode()` (1706).

The hook lives strictly inside the normal-frame `else` block (1656-1714) and must
be skipped whenever `s_bSpibRxInWaveMode` is true or a wave parse is pending.

### 1.5 Where parse_ok / parse_fail counters are updated

Lines 1689-1699:

```c
if (bParseOk) { gSpibRxParseOkCount++; }                 /* 1689-1691 */
else {
    gSpibRxParseFailCount++;                             /* 1695 */
    gSpibRxErrorFlags |= SPIB_RX_ERR_FRAME_PARSE_FAIL;   /* 1696 */
    reportSlaveError(SPIB_FAULT_SOURCE_PROTOCOL,
                     (uint16_t)SPIB_PROT_FAULT_FRAME_PARSE_FAIL);  /* 1697 */
}
```

Globals declared at lines 41-44 (`gSpibRxParseOkCount`, `gSpibRxParseFailCount`).
The self-test reads these as the `parse_ok` / `parse_fail` delta counters. The
hook must NOT perturb these for legacy frames; for a consumed Packet V1 frame it
must avoid the `else` branch entirely (see sections 4 and 5).

### 1.6 Where the existing `0xA55A` packet FSM is already involved

`SPIB_ParseRegFrame()` (1418) decides ownership at lines 1425-1426:

```c
if ((spiB_slave.ePacketState != SPIB_PACKET_STATE_IDLE) ||
    (u16Cmd == SPIB_PACKET_HEADER_MAGIC))                /* 1425-1426 */
```

If word0 is `SPIB_PACKET_HEADER_MAGIC` (== `0xA55A`, `spi_slave.h:35`) OR a packet
is already mid-parse, BOTH words are routed through `feedPacketWord()` (def 1296),
the OLD packet FSM: states `SPIB_PACKET_STATE_IDLE/CMD/LENGTH/PAYLOAD/CHECKSUM`,
checksum via `calcSpiPacketChecksum()` (a byte-sum, line 770), reset via
`resetPacketParser()` (1278), fail via `failPacketParser()` (1288). Its state
lives in `spiB_slave` (`ePacketState`, `u16PacketCmd`, `u16PacketLength`,
`u16PacketChecksum`, ...).

This is the critical fact for section 4: **`0xA55A` is already claimed by the old
FSM today**, and that FSM uses a byte-sum checksum, NOT the Packet V1
CRC16/CCITT-FALSE.

### 1.7 Recommended hook point

A single gated block between line 1685 (`SPIB_RxDmaClearDone()`) and line 1687
(`SPIB_ParseRegFrame()`), guarded so it is inert unless armed:

```text
... pFrame read (1665-1667) ...
SPIB_RxDmaClearDone();                       /* 1685 */
/* [HOOK] if armed and (u16Cmd==0xA55A || recognizer COLLECTING):
 *        feed {u16Cmd,u16Data} to the A6-3A recognizer and CONSUME the
 *        frame (skip SPIB_ParseRegFrame + counter block for this frame). */
bParseOk = SPIB_ParseRegFrame(u16Cmd, u16Data);   /* 1687 (unchanged when OFF) */
```

---

## 2. How can Packet V1 be detected without changing DMA?

The hook assumes, and must preserve:

```text
SPIB DMA remains 2-word legacy frame mode.
No DMA frame length change.
No DMA channel change.
No DMA trigger change.
No wave burst behavior change.
```

Detection works purely on the words the DMA already delivers. Each normal DMA-done
yields exactly two words (`u16Cmd`, `u16Data`, lines 1666-1667). The hook feeds
those two words into the A6-3A recognizer:

```c
SpiPacketV1_WireProbe_FeedWords(&probe, twoWords, 2U);
```

A Packet V1 PING frame `{0xA55A, 0x8000, 0x0000, CRC16}` spans two DMA frames:

```text
DMA-done #1 -> {0xA55A, 0x8000}  -> recognizer: COLLECTING (header, cmd)
DMA-done #2 -> {0x0000, CRC16}   -> recognizer: FRAME_OK or FRAME_ERROR
```

A6-3A already proved the recognizer accepts a frame fed two-words-at-a-time and
split across feed calls (its host tests "frame split across one-word feeds" and
"frame fed via FeedWords"). The recognizer is allocation-free, holds its own
`COLLECTING` state across DMA-done events, and needs no buffer in the SPIB path.
No DMA register, channel, trigger, length, or wave path is read or written by the
hook - it only consumes already-received words.

---

## 3. Gating mechanism (most important section)

### Option A - compile-time guard only

```c
#define SPI_PACKET_V1_WIRE_PROBE_ENABLE 0   /* default OFF in mainline */
```

- Default firmware behavior: byte-identical to today when `0`; the hook code is
  not compiled in. No runtime cost.
- Risk to legacy traffic: none when `0`.
- New CCS Watch variable: no.
- New global diagnostics: no.
- How to arm during board test: rebuild a test image with the macro set to `1`.
- How to turn off after test: rebuild / reflash the default image (`0`).
- Weakness: arming requires a rebuild + reflash; no runtime control, so it cannot
  be armed for a single transaction and disarmed without another flash.

### Option B - selftest-owned runtime gate (recommended, layered under A)

A single arm flag lives inside the EXISTING self-test flow (e.g. an internal
state owned by `Asr5kSpiSelfTest_Run()` / a dedicated probe step), NOT a new
free-standing global. The hook reads an accessor such as
`SpibWireProbe_IsArmed()` whose backing state is set/cleared only by the
self-test sequence.

- Default firmware behavior: disarmed at boot; identical to today until the
  self-test explicitly arms it for the probe step.
- Risk to legacy traffic: none while disarmed; while armed it is confined to the
  dedicated probe window between legacy runs (see section 7).
- New CCS Watch variable: no - it reuses the existing `g_asr5kSpiSelfTest`
  surface, which is already the single watch target.
- New global diagnostics: no new free global; the arm bit is part of the existing
  self-test state.
- How to arm during board test: the existing CCS-Watch trigger
  (`g_asr5kSpiSelfTest.start = 1`) running a probe step arms it; no UART.
- How to turn off after test: the probe step disarms it automatically at step
  end, and a CPU reset clears it.

### Option C - temporary board-build guard

Enable the hook only on a throwaway test branch / build, never on mainline.

- Default firmware behavior: mainline unchanged (hook never present).
- Risk to legacy traffic: none on mainline; the test branch carries the risk.
- New CCS Watch variable: no.
- New global diagnostics: depends on the branch.
- How to arm: check out and flash the test branch.
- How to turn off: flash mainline.
- Weakness: same rebuild/reflash friction as A, plus branch drift risk; useful
  only as an extra isolation layer, not as the primary mechanism.

### Option D - always-on passive recognizer

The recognizer runs unconditionally on every frame.

- REJECTED. It changes default firmware behavior, collides permanently with the
  existing `0xA55A` packet FSM, and risks legacy `parse_fail` pollution. Not
  acceptable unless separately proven harmless, which defeats the staged purpose.

### Recommendation

**Option B (selftest-owned runtime gate), layered under Option A (compile-time
`SPI_PACKET_V1_WIRE_PROBE_ENABLE`, default 0).** The compile guard keeps the
mainline image byte-identical and removes the hook from production builds; the
self-test-owned runtime arm gives single-window control during a board session
without a reflash and without a new free global or CCS Watch variable. No
UART/Modbus is involved. Option C may be used additionally as a belt-and-braces
test-branch isolation but is not the primary mechanism.

---

## 4. Avoiding collision with the existing `0xA55A` runtime packet FSM

The codebase already owns `0xA55A` via the OLD packet FSM (section 1.6); that FSM
uses a byte-sum checksum, not Packet V1 CRC16. The plan defines ownership
explicitly:

- **Gate OFF (default):** a `0xA55A` word is owned by the EXISTING packet FSM,
  exactly as today - `SPIB_ParseRegFrame()` (1418) diverts it to
  `feedPacketWord()` at lines 1425-1453. The A6-3A recognizer is never invoked.
  Zero behavior change.
- **Gate ON (armed):** the hook (section 1.7) intercepts the frame BEFORE line
  1687. If `u16Cmd == 0xA55A` OR the recognizer is already `COLLECTING`, the two
  words go to the A6-3A recognizer and the frame is CONSUMED - `SPIB_ParseRegFrame()`
  and the parse-counter block (1687-1699) are skipped for that frame, so the old
  FSM never sees it.
- **Consume vs observe:** when armed, the probe **consumes** the candidate frame
  (it does not also forward it to the old FSM). This is mandatory because feeding
  the same `0xA55A` frame to the old byte-sum FSM guarantees a checksum mismatch
  against the CRC16 word and a `parse_fail` (the old FSM at line 1378-1387 would
  reject the CRC16 word).
- **No double-routing:** the dispatch decision is single-valued per DMA frame -
  armed+header/collecting routes to the recognizer ONLY; everything else routes
  to the existing path ONLY. The arming condition mirrors the existing FSM's own
  guard (`ePacketState != IDLE || cmd == 0xA55A`) so the recognizer and the old
  FSM can never both claim the same frame.
- **No false legacy parse failures:** consumed Packet V1 frames never reach
  `feedPacketWord()` or `SPIB_ParseLegacyRegFrame()`, so they never bump
  `gSpibRxParseFailCount` or latch `SPIB_RX_ERR_FRAME_PARSE_FAIL` (1695-1696).

Note: because arming is confined to the dedicated probe window (section 7) and the
legacy Test1~Test9 do not run during that window, suppressing the old FSM for the
single armed PING frame does not affect any legacy test.

---

## 5. Consume or observe?

- **Mode 1 - observe-only, legacy still receives the words.** REJECTED as the
  A6-3B mode. The old `0xA55A` FSM would still consume the frame and raise a
  byte-sum checksum mismatch -> `failPacketParser()` -> protocol fault, plus
  `gSpibRxParseFailCount++` and `gSpibRxErrorFlags |= SPIB_RX_ERR_FRAME_PARSE_FAIL`.
  That pollutes exactly the counters the PASS criteria protect.
- **Mode 2 - gated consume-when-header-seen.** RECOMMENDED. While armed, an
  `0xA55A` frame (or an in-progress recognizer frame) is routed to the A6-3A
  recognizer and consumed; the old FSM and the legacy parser are bypassed for that
  frame. Behavior change is confined to the armed window; legacy counters stay
  clean; no response is produced.
- **Mode 3 - record-only after legacy parse.** REJECTED. The old FSM at line 1687
  has already consumed and failed the `0xA55A` frame before any post-parse record
  could run, so a real Packet V1 wire frame is unusable through this mode.

**Recommendation: Mode 2 (gated consume-when-header-seen).** It is the only mode
that both observes a real wire frame and keeps `parse_fail` / error flags clean,
and it does so without any DMA or wave change. This plan does NOT implement it.

---

## 6. Observable result surface

Constraints: no new free-floating diagnostics, no new CCS Watch variables.

- The recognizer instance is a `static` file-scope `ST_PKTV1_WIRE_PROBE` inside
  `spi_b_slave.c` (NOT an exported global), surfaced through ONE small accessor.
- Its outcome is exposed only through the EXISTING `g_asr5kSpiSelfTest` structure
  (already the single board-watch surface, `asr5k_spi_selftest.h:138`). The fields
  needed - header seen, `cmd_id`, `payload_words`, CRC OK, `frame_ok_count` - map
  directly onto an existing `ST_ASR5K_SPI_TEST_RESULT` record (`expected` /
  `actual` / `status`), exactly as A6-1 surfaced its PING result via
  `test[9].actual == 0x8000504F`.
- Recommended surface: an appended **Test11** record (bump
  `ASR5K_SPI_SELFTEST_RECORD_COUNT` 10 -> 11 and add `ASR5K_SPI_TEST_ID_11` +
  an append-only `ASR5K_SPI_FAIL_STEP_PKTV1_WIRE`), following the proven Test10
  pattern. This reuses the existing watch struct, so it adds no new free global
  and no new CCS Watch variable.
- Rejected alternatives: a new `extern` probe-result global (would be a new CCS
  Watch variable / global diagnostic) and free-floating counters in `spi_b_slave.c`
  exported for watching (same objection).

This plan adds NO Test11 and NO code; Test11 is the A6-3B deliverable. If even
Test11 is to be deferred, the fallback is to read the static accessor into an
existing spare test-record field, but Test11 is the cleaner, precedent-following
choice.

---

## 7. Future board test flow (A6-3B / A7-0) - do NOT execute here

1. Confirm baseline before the packet probe (CCS Watch trigger
   `g_asr5kSpiSelfTest.start = 1`, no UART):
   ```text
   Test1~Test9 PASS
   Test10 PASS
   fault_code = 0
   ```
2. Arm the passive probe via the self-test gate (Option B); the default image is
   unchanged until this step.
3. An EXTERNAL SPI master sends exactly one Packet V1 PING frame on the wire:
   ```text
   0xA55A
   0x8000
   0x0000
   CRC16   (CRC16/CCITT-FALSE over 0xA55A,0x8000,0x0000)
   ```
4. The probe records the receive result (header seen, cmd_id, payload_words, CRC
   OK) into the existing self-test surface.
5. Run the legacy baseline again:
   ```text
   Test1~Test9 PASS
   no parse_fail regression
   no DMA overflow
   no wave regression
   ```
6. No PONG / no MISO response is sent or expected.

The external master is required precisely so the result proves WIRE transport, not
a C2000-internal loopback. Using the on-board SPIA master is deliberately out of
scope for A6-3B (it would also drag in `SPI_master.*` and its byte-sum packet
path); SPIA-as-source is a separate later option.

---

## 8. Future PASS criteria (A6-3B / A7-0)

Mandatory:

```text
SPIB passive recognizer saw header 0xA55A
cmd_id == 0x8000
payload_words == 0
CRC OK
no MISO response
legacy Test1~Test9 PASS before
legacy Test1~Test9 PASS after
A6-1 Test10 still PASS
gSpibRxParseFailCount does not regress during legacy tests
gSpibRxErrorFlags remains 0 during legacy tests
wave download Test9 still PASS
UART/Modbus not used
```

A run is accepted only if BOTH the before and after legacy baselines pass and the
probe recorded a CRC-OK PING; a probe success that regresses any legacy test or
latches an error flag is an overall FAIL.

---

## 9. Reject conditions

The A6-3B implementation MUST be rejected if it:

- changes SPIB DMA frame length;
- changes DMA channels;
- changes wave burst mode;
- changes the SPIA master (`SPI_master.*`) in the same task;
- sends a PONG in A6-3B;
- uses UART / Modbus;
- changes syscfg / linker / `.cproject`;
- adds an always-on recognizer (Option D / Mode 1 unguarded);
- claims a wire PASS without an actual external SPI transaction;
- breaks legacy Test1~Test9 (or Test10).

Any of these stops the task for re-scoping.

---

## 10. Recommended next implementation task

**A6-3B - gated SPIB passive hook, no response.**

Implement Mode 2 (gated consume-when-header-seen) at the section 1.7 hook point,
gated by Option B under the Option A compile guard, feeding the unmodified A6-3A
recognizer, exposed via an appended Test11 on the existing self-test surface. No
PONG.

Minimal allowed files, each justified:

```text
Emu_3352_SPI/SPIB_Slave/spi_b_slave.c
    - the ONLY runtime touch: the gated hook block before line 1687, the static
      ST_PKTV1_WIRE_PROBE instance, one accessor, behind
      SPI_PACKET_V1_WIRE_PROBE_ENABLE (default 0).

Emu_3352_SPI/asr5k_spi_selftest.c
    - the arm/disarm probe step and population of the Test11 record from the
      recognizer accessor (mirrors the Test10 validator pattern).

Emu_3352_SPI/asr5k_spi_selftest.h
    - RECORD_COUNT 10 -> 11, ASR5K_SPI_TEST_ID_11, append-only
      ASR5K_SPI_FAIL_STEP_PKTV1_WIRE. Append-only to preserve host decoder
      compatibility.

Emu_3352_SPI/docs/SPI_PACKET_V1_A6_3B_PASSIVE_HOOK_RESULT.md
    - the A6-3B result/evidence document.
```

Reused but NOT modified (link-only): `SPI_PacketV1/spi_packet_v1_wire_probe.{h,c}`
and `SPI_PacketV1/spi_packet_crc16.{h,c}` (already proven by A6-3A; already built
into firmware because the A6-1 `.cproject` exclusion denylists only the `*_test.c`
harnesses, so no `.cproject` change is needed).

Explicitly NOT in the allowed set (hard restrictions carried forward):
`spi_slave.h`, `SPI_master.*`, `wave_download.*`, `cmd_id.h`, syscfg, linker,
`.cproject`.

---

## Appendix A - grounding references (read-only)

- `SPIB_Slave/spi_b_slave.c`: `pollReceiveFromSpi()` (1590); 2-word frame
  (1665-1667); legacy parse `SPIB_ParseRegFrame()` (1687 / def 1418); counters
  (1689-1699 / globals 41-44); wave branch (1630), wave-pending early return
  (1613-1617), wave entry (1706); DMA re-arm `SPIB_RxRestartRegFrameDma()` (1710 /
  def 472); existing `0xA55A` FSM `feedPacketWord()` (1296), dispatcher guard
  (1425-1426), `resetPacketParser()` (1278).
- `SPIB_Slave/spi_slave.h`: `SPIB_PACKET_HEADER_MAGIC == 0xA55A` (35);
  `SPIB_RX_REG_WORDS == 2` (33); packet FSM states / fields (62-75, 198-201).
- `SPI_PacketV1/spi_packet_v1_wire_probe.{h,c}`: A6-3A recognizer
  (`SpiPacketV1_WireProbe_Reset/FeedWord/FeedWords`, `ST_PKTV1_WIRE_PROBE`).
- `asr5k_spi_selftest.h`: `g_asr5kSpiSelfTest` surface (138), RECORD_COUNT 10
  (32), Test ids / fail-step enums (34-100), result record (102-122).
- `docs/SPI_PACKET_V1_A6_2_WIRE_PROBE_PLAN.md`,
  `docs/SPI_PACKET_V1_A6_3A_WIRE_PROBE_CORE.md`: staged path and recognizer scope.

## Appendix B - closure statement

A6-1, A6-2, and A6-3A remain CLOSED and are not reopened or reinterpreted by this
plan. A6-3B0 is additive design work only; it changes no source and no firmware
behavior.
