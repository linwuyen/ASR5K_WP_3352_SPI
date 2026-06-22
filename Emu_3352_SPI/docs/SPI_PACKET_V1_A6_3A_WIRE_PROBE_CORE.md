# SPI Packet V1 A6-3A - Passive Wire Recognizer Core (Pure-C, Host-Test Only)

Status: **HOST TEST PASS** (`PASS=80 FAIL=0`, gcc `-std=c99 -Wall -Wextra -Werror`)
Board-verifiable: **NO** (pure-C logic only; no firmware behavior change)
Wire-level verified: **NO**
Builds on: A2 parser/encoder + CRC16, A6-1 board PING probe, A6-2 wire probe plan.

Base branch: `feature/spi-packet-v1-a6-2-wire-probe-plan`
Base commit: `f6d71703c60c6264818b2ad07ff5fd72fa7c21ce`
This branch: `feature/spi-packet-v1-a6-3a-wire-probe-core`

ASCII-ONLY for MS950 / CCS compilation and review safety.

---

## 1. Task scope

Implement a **pure-C, passive, detect-only** Packet V1 wire recognizer core that
consumes 16-bit words exactly as if they had arrived from the SPI wire, detects
Packet V1 frames (header `0xA55A`, command id, length, payload), validates the
declared length and the CRC16/CCITT-FALSE, and exposes a compact result struct.

This task is the first step of the staged path defined by A6-2:

```text
A6-2  : wire-level probe plan (design only).                         [done]
A6-3A : pure-C passive recognizer core (this task).                  [this]
A6-3B / A7-0 : gated SPIB passive hook only, no response.            [next]
A7    : PING response / PONG only after passive receive is proven.   [later]
```

A6-3A proves the recognizer logic in isolation on the host **before** any SPIB
RX/DMA runtime is touched. It does not hook into runtime, send a response, or
make any board claim.

---

## 2. Files changed

New:
- `Emu_3352_SPI/SPI_PacketV1/spi_packet_v1_wire_probe.h` - recognizer API,
  state/result enums, `ST_PKTV1_WIRE_PROBE` result struct, payload cap.
- `Emu_3352_SPI/SPI_PacketV1/spi_packet_v1_wire_probe.c` - pure-C recognizer
  implementation (FSM + incremental CRC).
- `Emu_3352_SPI/SPI_PacketV1/spi_packet_v1_wire_probe_test.c` - host test harness
  (guarded by `SPI_PACKET_V1_HOST_TEST`; empty translation unit otherwise).
- `Emu_3352_SPI/docs/SPI_PACKET_V1_A6_3A_WIRE_PROBE_CORE.md` - this document.

Modified (additive only):
- `.github/workflows/spi_packet_v1_host_tests.yml` - appended a conditional
  A6-3A step after A6-1; A2/A4/A5/A6-1 steps are unchanged.

No other files. No runtime/source change outside `SPI_PacketV1/`.

---

## 3. Why this is pure-C only

The recognizer must be provable in isolation and portable to the C28x firmware
without dragging in hardware or runtime state. It therefore depends only on
`<stdint.h>` and the existing pure-C Packet V1 sources (`spi_packet_crc16.*`,
`spi_packet_v1.h` for the shared header magic). It uses:

- no driverlib, no `device.h`, no `board.h`;
- no `spi_slave.h` / `SPI_master.h` / any SPIA/SPIB header;
- no `cmd_id.h`, no hardware registers;
- no global diagnostics, no `malloc`, no `stdio` in the production source;
- C99-compatible constructs only.

C28x safety: frame sizing uses explicit word counts (overhead = 4 words, payload
cap = 16 words). It never uses `sizeof(buffer)/2`, because on C28x
`sizeof(uint16_t) == 1` and that idiom is wrong there.

The CRC is accumulated incrementally with `SpiPacketCrc16_UpdateWord()` starting
from `SPI_PACKET_CRC16_INIT`, over Header + Command + Length + Payload (the CRC
word is excluded). This is bit-for-bit identical to the span covered by the A2
`SpiPacketCrc16_ComputeWords()` and the A2 streaming parser, so the recognizer
and the encoder agree by construction.

---

## 4. Why no SPIB runtime is touched

Per the A6-2 plan, the riskiest unknown (wire reception coupled to master clock /
DMA framing) must be deferred. Touching `spi_b_slave.c` / `spi_slave.h` / SPIB
DMA now would risk the legacy 2-word register cadence and the wave-burst DMA path
for no proof benefit. A6-3A keeps the recognizer entirely in caller-owned host
memory:

- it is fed words by a test harness, not by an ISR or DMA;
- it holds no global state and produces no response (no MISO, no PONG);
- it shares no handler with the legacy 2-word path or the existing `0xA55A`
  SPIB packet FSM.

Connecting the recognizer to the SPIB RX path is the explicitly separate, gated
A6-3B / A7-0 task.

---

## 5. Recognizer state machine

States: `IDLE`, `COLLECTING`, `FRAME_OK`, `FRAME_ERROR`.

```text
            non-header word (ignored)
            +-----------------------------+
            v                             |
        [ IDLE ] --- word == 0xA55A ---> [ COLLECTING ]
            ^                                 |
            |                  word 1 = command id
            |                  word 2 = payload length N
            |                     |  N > 16 (cap)  -> [ FRAME_ERROR ] (BAD_LENGTH)
            |                     v
            |             collect N payload words (CRC-covered)
            |                     |
            |             final word = CRC16
            |              CRC match -> [ FRAME_OK ]   (frame_ok_count++)
            |              CRC fail  -> [ FRAME_ERROR ] (BAD_CRC, frame_error_count++)
            |
       SpiPacketV1_WireProbe_Reset()  (full reset, clears counters)
```

Word layout of a candidate frame (total = 4 + N words):

```text
w0        0xA55A          header / magic
w1        command id
w2        N               payload length in 16-bit words (cap 16)
w3..3+N-1 payload          N words (CRC-covered, not stored)
w3+N      CRC16/CCITT-FALSE over w0..2+N
```

Behavior summary:

1. Starts `IDLE`; ignores non-header words while between frames.
2. A `0xA55A` word starts `COLLECTING` (also resyncs from `FRAME_OK` /
   `FRAME_ERROR`, so a continuous wire stream is handled).
3. On the final CRC word: `FRAME_OK` + `frame_ok_count++` on match; otherwise
   `FRAME_ERROR` + `frame_error_count++`.
4. Declared length above the 16-word cap is rejected as `BAD_LENGTH` at the
   length word.
5. `FeedWord` returns OK while a frame is still collecting (inspect `state` to
   distinguish `COLLECTING` from `FRAME_OK`); a non-OK return is a definitive
   per-word error. `FeedWords` stops on the first definitive error.
6. The recognizer never sends a response and never alters any legacy path; it is
   not connected to runtime.
7. `Reset` performs a simple full reset, clearing the in-progress frame fields
   AND the ok/error counters.

API:

```c
void SpiPacketV1_WireProbe_Reset(ST_PKTV1_WIRE_PROBE *p);
PKTV1_WIRE_PROBE_RESULT_e SpiPacketV1_WireProbe_FeedWord(
    ST_PKTV1_WIRE_PROBE *p, uint16_t word);
PKTV1_WIRE_PROBE_RESULT_e SpiPacketV1_WireProbe_FeedWords(
    ST_PKTV1_WIRE_PROBE *p, const uint16_t *words, uint16_t word_count);
```

---

## 6. Host test command

```bash
cd Emu_3352_SPI
gcc -std=c99 -Wall -Wextra -Werror -DSPI_PACKET_V1_HOST_TEST \
  SPI_PacketV1/spi_packet_crc16.c \
  SPI_PacketV1/spi_packet_v1.c \
  SPI_PacketV1/spi_packet_v1_wire_probe.c \
  SPI_PacketV1/spi_packet_v1_wire_probe_test.c \
  -o spi_packet_v1_wire_probe_test
./spi_packet_v1_wire_probe_test
```

The harness covers the 16 required scenarios (some with several assertions):
reset init, NULL rejection, idle ignores non-header, valid PING, valid ECHO with
payload, single-word split feed, `FeedWords()` feed, bad CRC, declared length too
large, truncated frame stays collecting, noise before header, two frames with
reset between, zero-length payload accepted, command id preserved, CRC
actual/expected exposed on error, and the compile-time guard.

Firmware-build harmlessness was verified by compiling the test file **without**
`-DSPI_PACKET_V1_HOST_TEST`: it produces an empty translation unit (no `main`,
no `stdio`), exactly like the A2/A4/A5/A6-1 harnesses.

---

## 7. Host test result

```text
A6-3A wire-probe core: PASS=80  FAIL=0
gcc -std=c99 -Wall -Wextra -Werror, exit 0, 0 warnings
```

Full suite (host, all warning-free under -Werror):

```text
A2    : PASS=126 FAIL=0
A4    : PASS=104 FAIL=0
A5    : PASS=107 FAIL=0
A6-1  : PASS=14  FAIL=0
A6-3A : PASS=80  FAIL=0
```

Existing A2/A4/A5/A6-1 results are unchanged by this task.

---

## 8. CI result expectation

`.github/workflows/spi_packet_v1_host_tests.yml` gains a conditional A6-3A step
after the A6-1 step. It compiles with `-std=c99 -Wall -Wextra -Werror`, runs the
recognizer test, and asserts:

```text
grep -E "PASS=80[[:space:]]+FAIL=0"
```

The A2/A4/A5/A6-1 steps are unchanged. The step is guarded by an
`if [ -f ... ]` presence check, consistent with the other conditional steps.

---

## 9. Limitations

This task explicitly does NOT do or claim any of:

- **No board test.** Pure-C host logic only; not run on target silicon here.
- **No wire test.** No frame crossed a physical SPI bus; words are fed from a
  host test buffer.
- **No SPIB DMA.** The SPIB RX / DMA CH3 path, ping-pong, and wave-burst DMA are
  untouched.
- **No MISO PONG.** The recognizer is passive and detect-only; it never produces
  a response.
- No SPIA master, `SPI_master.*`, `wave_download.*`, `cmd_id.h`, syscfg, linker,
  `.cproject`, `.project`, `.settings`, or `.agent` change.
- No board runtime globals, no selftest Test11, no UART / Modbus.

A6-1 remains CLOSED and is not reopened or reinterpreted by this task.

---

## 10. Next recommended task

**A6-3B / A7-0 - gated SPIB passive hook only, no response.**

Connect this proven recognizer to the SPIB RX path behind an explicit selftest
gate or compile-time guard, observe a wire-delivered `0xA55A` Packet V1 frame
without disturbing the legacy 2-word path, the existing `0xA55A` SPIB packet FSM,
or the wave-burst DMA, and require legacy Test1~Test9 to PASS before and after.
Still NO response (PONG) until passive receive is board-proven; the PONG response
is the later A7 task.
