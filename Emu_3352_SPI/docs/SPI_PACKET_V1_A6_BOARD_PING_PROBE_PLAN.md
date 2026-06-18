# SPI Packet V1 A6-0 — Board-Observable PING Probe Plan

## 1. Status

- **A6-0 = design / inspection only.** No code, no probe implementation.
- **Board-verifiable (A6-0): NO** — this is a plan document.
- **A6-1 target: board-verifiable YES** — the next task should flash the board
  and show a clear "Packet V1 PING recognized" result.
- Hard constraint carried forward: **B01F legacy SPIB / DMA / Wave Download must
  not be broken.** B01F Test1~Test9 must still PASS.

## 2. Current proven layers (pure-C, host-tested)

| Layer | Module | Host test |
|-------|--------|-----------|
| A2 | `spi_packet_v1.{c,h}` (+ `spi_packet_crc16.{c,h}`) — parser + encoder + CRC | `PASS=126 FAIL=0` |
| A4 | `spi_packet_v1_dispatch.{c,h}` + `spi_packet_v1_cmd.h` — command dispatcher mock | `PASS=104 FAIL=0` |
| A5 | `spi_packet_v1_loopback.{c,h}` — raw words → parse → dispatch → encode → raw words | `PASS=107 FAIL=0` |
| CI | `.github/workflows/spi_packet_v1_host_tests.yml` — A2/A4/A5 under `-Werror` | green on push |

The full request→response path is proven in host memory. What is **not** yet
proven is that this logic runs on the **target C28x CPU** and that a result is
**visible on the board**.

## 3. Existing B01F runtime summary (inspected, not modified)

All references below are read-only observations of the current runtime.

### 3.1 How legacy SPIB receives frames

- Top entry: `runSPIBslave()` (`SPIB_Slave/spi_b_slave.c:1960`) → polling loop
  `pollReceiveFromSpi()` (`spi_b_slave.c:1590`). **Polled, not ISR-driven.**
- RX transport: **DMA CH3**, edge-triggered at the SPI-B RX FIFO watermark,
  **2 words per frame** (`SPIB_RX_REG_WORDS = 2`). Ping-pong buffers
  (`s_rxPingPong`, `spi_pingpong.c`) are swapped *after* DMA-done so the parser
  reads the just-filled buffer.
- TX transport: **DMA CH4** streams a 16-word status packet from a ping-pong
  buffer (`SPIB_TxStatusArm()` / `SPIB_TxStatusRefresh()`).

### 3.2 Where the parser entry point is

- `SPIB_ParseRegFrame(uint16_t u16Cmd, uint16_t u16Data)` (`spi_b_slave.c:1418`).
- It branches: **if `ePacketState != IDLE` OR `u16Cmd == 0xA55A`** it routes to an
  **existing in-runtime packet state machine** `feedPacketWord()`
  (`spi_b_slave.c:1296`); otherwise it calls
  `SPIB_ParseLegacyRegFrame()` (`spi_b_slave.c:1256`) →
  `tryHandleFastPath()` (`spi_b_slave.c:1118`).
- Legacy frame = 2 words `(address, data)`.

> ⚠️ **Critical finding:** the magic `0xA55A` (`SPIB_PACKET_HEADER_MAGIC`,
> `spi_slave.h:35`) is **already consumed** by an *existing experimental* packet
> state machine in the runtime (`feedPacketWord`, `spi_b_slave.c:1296-1404`).
> This is **separate from** our pure-C SPI Packet V1 (A2–A5). Any attempt to wire
> our Packet V1 into the live receive path would **collide** with this existing
> 0xA55A handler. This materially raises the risk of Options C/D below.

### 3.3 Where DMA is involved

- RX: `initSpibRxDma()` (`:386`), `SPIB_RxDma_ConfigureRegFrame()` (`:451`),
  `SPIB_RxRestartRegFrameDma()` (`:472`, with edge-trigger recovery `:499`).
- Wave block capture: `SPIB_RxWaveEnterBlockMode()` (`:548`) reconfigures CH3 to
  capture 4096 frames into `g_u16WaveRawRxBuffer`.
- TX: CH4 status streaming.

### 3.4 Where legacy wave / block addresses are handled

| Address | Handler |
|---------|---------|
| `0x0958` WAVE_PAGE_SELECT | `WaveDownload_SetPage()` (`wave_download.c:55`) |
| `0x095A` WAVE_PAGE_STATUS | `WaveDownload_GetStatus()` (`:260`) |
| `0x095B` WAVE_BURST_BEGIN | `tryHandleFastPath()` + `WaveDownload_BeginBurst()` (`:188`) → arms block DMA |
| `0x0960` WAVE_VALIDATE | `WaveDownload_ValidatePage()` (`:265`) |
| `0x0961` WAVE_ACTIVATE | `WaveDownload_ActivatePage()` (`:372`) |
| `0x3000..0x3FFF` block data | `WaveDownload_WriteSample()` (`:132`), block parse `SPIB_RxWaveParseChunk()` (`:728`) |

### 3.5 What must NOT be changed

- The **post-burst RX framing recovery** in `SPIB_RxWaveFinishPostWave()`
  (`spi_b_slave.c:619-724`): a full `SPI_disableModule()/enableModule()` reset is
  the *only* documented cure (B01C) for RX de-framing after a long wave burst.
- DMA **ping-pong swap timing** (`:1665`) and the wave-mode **edge-trigger
  recovery** force-kick loop (`:573-586`).
- The existing `0xA55A` `feedPacketWord()` state machine and `SPIB_ParseRegFrame`
  branch order.
- `cmd_id.h`, syscfg, linker, DMA channel setup.

## 4. Board-observable target (A6-1)

The exact future A6-1 goal:

1. User builds the firmware and **flashes the board**.
2. User triggers **one** SPI Packet V1 PING (in the recommended path: an in-memory
   PING fed through the existing selftest, not over the SPI wire — see §7).
3. The board exposes a **clear result** through an existing observability surface:
   - **UART**: the selftest already prints `SPI_TEST RUN` and a final
     `PASS / FAIL ... fault_code=XXXX` line
     (`asr5k_spi_selftest.c:1118-1140`).
   - **CCS Watch**: the global `volatile ST_ASR5K_SPI_SELFTEST_RESULT
     g_asr5kSpiSelfTest` (`asr5k_spi_selftest.h:131`, placed in its own
     `DATA_SECTION`) holds `result_text` ("PASS"/"FAIL"), per-test results,
     `fault_code`, and a test-specific `actual` value.
4. **B01F Test1~Test9 still PASS** (the new probe is additive and pure-C).

## 5. Candidate implementation options

### Option A — Pure firmware selftest probe (recommended)
- Build a Packet V1 PING request frame in firmware RAM, call
  `SpiPacketV1_Loopback()` (A5) in-memory, and expose the result through the
  **existing selftest result struct + UART** (e.g. a new additive **Test10**).
- Proves Packet V1 logic on the **target CPU** (real C28x toolchain, real memory,
  `CHAR_BIT==16`) — but **not** SPI wire transport.
- **Lowest risk**: no SPIB/DMA/wave touch; cannot affect Test1~Test9.

### Option B — SPIA master sends a Packet V1 frame to SPIB
- Use the SPIA master (`SPI_master.c`) to clock real Packet V1 words to the SPIB
  slave on-board. More representative (exercises the SPI wire).
- **Blocker:** the SPIB receive path is the **2-word register protocol**, and
  `0xA55A` already triggers the *existing* `feedPacketWord` state machine — so the
  slave would not route our Packet V1 frame to A2–A5 without runtime changes.
  Master `triggerSpiMasterSelfTest()` (`SPI_master.c:1357`) is a **master-only HW
  loopback** (no SPIB), so it cannot carry a slave-side Packet V1 decode either.
- **Medium–high risk**, and not achievable without §5-C runtime work first.

### Option C — SPIB runtime shadow probe
- Add an explicitly-gated Packet V1 detector near the legacy receive path.
- **Highest risk**: it sits next to DMA/ping-pong and **collides with the existing
  `0xA55A` `feedPacketWord` handler**; any misstep risks the fragile post-burst
  framing recovery and B01F.

### Option D — AM3352 external master path
- Real end-to-end path; requires an AM3352-side Packet V1 generator and wiring.
- **Highest integration complexity**; not a sensible first A6 step.

## 6. Risk matrix

| Dimension | A (selftest probe) | B (SPIA→SPIB) | C (SPIB shadow) | D (AM3352) |
|-----------|--------------------|---------------|-----------------|------------|
| Board-verifiable? | **YES** (UART/Watch) | YES (if reachable) | YES | YES |
| Tests real SPI transport? | **NO** | YES | YES | YES |
| Touches SPIB runtime? | **NO** | indirectly (needs slave routing) | **YES** | YES |
| Touches DMA? | **NO** | maybe | **YES (near CH3)** | YES |
| Collides with existing `0xA55A` handler? | NO | **YES** | **YES** | YES |
| Risk to B01F | **LOW** | MED–HIGH | **HIGH** | HIGH |
| Files to modify in A6-1 | `asr5k_spi_selftest.{c,h}` (+ small new probe file) | + `SPI_master.*` + SPIB routing | + `spi_b_slave.c` (RX path) | + AM3352 tooling |
| User test procedure | flash → `spi_test all` → read PASS + Test10 | flash → master test → observe slave decode | flash → drive frame → observe shadow | external master rig |

## 7. Recommended A6-1 path

**Recommended: Option A — Firmware Packet V1 PING selftest probe.**

Rationale (conservative, smallest board-observable step that cannot risk B01F):

- It reuses the already-proven A5 `SpiPacketV1_Loopback()` unchanged and the
  already-existing selftest reporting surface, so the only new firmware code is a
  thin pure-C probe plus one additive selftest row.
- It touches **no** SPIB runtime, **no** DMA, **no** wave path, and does **not**
  go near the fragile post-burst reset or the existing `0xA55A` `feedPacketWord`
  state machine. Test1~Test9 are structurally unaffected.
- It is genuinely board-observable: the user flashes, runs the existing
  `spi_test` command, and sees a PASS plus a Packet V1 PING result in UART and in
  the `g_asr5kSpiSelfTest` CCS Watch struct.

**Explicit limitation (must be stated to the user):** Option A proves the Packet
V1 **logic executes correctly on the target CPU**, but it does **not** prove SPI
**wire** transport. Driving a Packet V1 frame across the actual SPI bus is a later
step (Option B/C) that requires deliberate, separately-gated SPIB runtime work and
must resolve the `0xA55A` collision first.

## 8. A6-1 proposed allowed files

The next implementation task (A6-1) should be allowed to change **only**:

- `Emu_3352_SPI/SPI_PacketV1/spi_packet_v1_probe.h` *(new)* — pure-C probe API:
  build a fixed PING request, call `SpiPacketV1_Loopback()`, return a small
  observable result struct. No globals beyond a caller-owned result.
- `Emu_3352_SPI/SPI_PacketV1/spi_packet_v1_probe.c` *(new)* — implementation
  (pure C; depends only on the A2–A5 headers and `<stdint.h>`).
- `Emu_3352_SPI/SPI_PacketV1/spi_packet_v1_probe_test.c` *(new)* — host test for
  the probe (guarded by `SPI_PACKET_V1_HOST_TEST`).
- `Emu_3352_SPI/asr5k_spi_selftest.c` — **additive only**: one new `Test10` row in
  `s_testTable[]` plus a validator that calls the probe and records the result.
  Test1~Test9 rows, steps, and validators must be byte-for-byte unchanged.
- `Emu_3352_SPI/asr5k_spi_selftest.h` — **additive only**: a new test id
  (e.g. `ASR5K_SPI_TEST_ID_10`) and, if needed, one new fault-code enum value.
- `Emu_3352_SPI/docs/SPI_PACKET_V1_A6_BOARD_PING_PROBE_RESULT.md` *(new)* — A6-1
  result/evidence doc.
- `.github/workflows/spi_packet_v1_host_tests.yml` — optional additive step to run
  the new probe host test (no change to existing steps).

## 9. A6-1 proposed forbidden files

Must remain untouched in A6-1:

- `Emu_3352_SPI/SPIB_Slave/*` — `spi_b_slave.c`, `spi_slave.h`, `spi_fifo.*`,
  `spi_pingpong.*`, `wave_download.*`, `cmd_parser.h`, `cmd_id.h`
- `Emu_3352_SPI/SPIA_Master/*` — `SPI_master.*`, `cmd_id.h`
- A2–A5 sources: `spi_packet_v1.{c,h}`, `spi_packet_crc16.{c,h}`,
  `spi_packet_v1_dispatch.{c,h}`, `spi_packet_v1_cmd.h`,
  `spi_packet_v1_loopback.{c,h}` and their existing test files
- DMA setup, syscfg, linker `.cmd`, `.cproject`, `.project`, `.settings`,
  `.agent`
- `OUTPUT_ON` / power-stage logic, OTP / BOOTCFG

## 10. A6-1 board test flow (for the user)

```text
1. git checkout feature/spi-packet-v1-a6-1-ping-selftest-probe   (A6-1 branch)
2. Build the firmware in CCS (full target image, C28x).
3. Flash the board.
4. Open the UART terminal used by the selftest.
5. Trigger the selftest: send "spi_test all".
6. Observe:
   - UART: "SPI_TEST RUN" then a final "PASS ..." line.
   - CCS Watch: g_asr5kSpiSelfTest.result_text == "PASS"
                g_asr5kSpiSelfTest.test[9].status == PASS   (Test10 = Packet V1 PING)
                g_asr5kSpiSelfTest.test[9].actual encodes response_cmd (0x8000) + pong word (0x504F)
7. Expected PASS result:
   - Test1~Test9 == PASS (B01F baseline intact)
   - Test10 (Packet V1 PING) == PASS, response command_id == PKTV1_CMD_PING (0x8000)
8. On failure, dump:
   - g_asr5kSpiSelfTest.failed_test_id, .fault_code
   - g_asr5kSpiSelfTest.test[9].actual / .fail_step
   - the loopback diag fields (result, parser_result, dispatch_result, response_cmd)
```

## 11. Stop condition

A safe board-observable A6-1 path **does** exist (Option A). Therefore the
"no safe path" condition does **not** apply. For completeness: a safe path that
also exercises real **SPI wire transport** does **not** exist today without SPIB
runtime work, because the live receive path is the 2-word register protocol and
`0xA55A` is already owned by the existing `feedPacketWord` handler.

## 12. Final recommendation

- **Recommended A6-1 name:** *Firmware Packet V1 PING selftest probe (Test10)* —
  pure-C logic on target CPU, observable via the existing selftest result struct +
  UART; no SPIB runtime change.
- **Board-verifiable:** **YES** (logic on target + UART/CCS Watch result).
  Caveat: proves logic, **not** SPI wire transport.
- **Expected risk:** **LOW** — additive selftest row + new pure-C probe module; no
  SPIB/DMA/wave touch; B01F Test1~Test9 structurally unaffected.
- **Exact next Claude/Codex task title:**
  `A6-1 — Add board-observable SPI Packet V1 PING selftest probe (Test10), pure-C, no SPIB runtime change`
