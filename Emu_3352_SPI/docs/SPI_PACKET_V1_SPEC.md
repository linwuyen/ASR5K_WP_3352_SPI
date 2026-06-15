# SPI Packet V1 Specification

Status: **Draft / Experimental (spec + pure-C skeleton only)**
Scope: `WP_3352_SPI/Emu_3352_SPI`
Owner module: `SPI_PacketV1/` (pure C, standalone)

---

## 1. Purpose

SPI Packet V1 defines a **self-describing, CRC-protected, variable-length frame
format** for SPI payloads, layered on top of the existing 16-bit-word SPI
transport.

The goal of this first version is narrow and deliberate:

- Pin down a **stable, versioned wire format** (header / command / length /
  payload / CRC) so future host and slave firmware agree on framing.
- Provide a **portable, pure-C reference implementation** (encoder + parser +
  CRC) that can be unit-tested on a host PC with no target dependencies.
- Provide a **golden test matrix** so any later runtime integration can be
  validated against the same vectors.

This version **does not** touch the running firmware. It is a paper spec plus an
isolated code skeleton. Wiring it into the SPIB runtime is explicitly future
work, gated by the rules in section 12.

---

## 2. Non-Goals / Hard Constraints

This version is intentionally limited. The following are **out of scope** and
must not be done as part of Packet V1 bring-up:

- **No SPIB runtime integration.** No call into, or modification of, the SPIB
  DMA RX/TX path, ping-pong buffers, or `runSPIBslave()`.
- **No legacy register protocol changes.** The existing 2-word
  `(address, data)` register frame and `SPIB_ParseRegFrame()` semantics stay
  byte-for-byte identical.
- **No `syscfg` changes.** `main.syscfg` is untouched.
- **No linker / memory map changes.** No `.cmd` / linker command file edits,
  no new sections.
- **No new CCS Watch global variables.** Packet V1 introduces no `extern`
  globals that would show up in the debugger watch set. All state is passed by
  argument or lives on the caller's stack.
- **No new dependencies.** The reference code includes only `<stdint.h>` (and,
  in the test harness only, `<stdio.h>`/`<string.h>`). No `driverlib`, no
  `device.h`, no `spi_slave.h`, no `wave_download.h`.

Hard pure-C rules for `SPI_PacketV1/`:

| Rule | Status |
|------|--------|
| pure C only | required |
| include `driverlib` | forbidden |
| include `device.h` | forbidden |
| include `spi_slave.h` | forbidden |
| include `wave_download.h` | forbidden |
| call `SPIB_ParseRegFrame()` | forbidden |
| call any legacy runtime function | forbidden |
| modify existing files | forbidden (report first if ever needed) |
| add CCS Watch global variable | forbidden |

---

## 3. Baseline Compatibility

Packet V1 is additive and isolated. The B01F baseline must remain intact:

- Baseline tree: `WP_3352_SPI / Emu_3352_SPI`.
- B01F **Test1~Test9 = PASS** must remain reproducible.
- Test9 **delta minimum = 4107** must be unchanged.
- Final wave page state = **LOCKED** must be unchanged.
- Legacy register protocol must remain unbroken.

Because Packet V1 lives in a new directory that is **not added to the CCS build**
and is **not referenced by any existing translation unit**, it cannot affect the
compiled firmware or the baseline test results. See the isolation checklist in
section 13.

Note: the legacy `spi_slave.h` already defines `SPIB_PACKET_HEADER_MAGIC =
0xA55A` for an internal experimental path. Packet V1 deliberately reuses the same
magic value (`0xA55A`) for conceptual continuity, but defines its **own**
independent constant `SPI_PACKET_V1_HEADER_MAGIC` and does not include or depend
on `spi_slave.h`.

---

## 4. Packet Word Format

A Packet V1 frame is an array of **16-bit words**. All multi-word counts are in
**16-bit words**, never bytes.

```
Word index   Field            Notes
----------   --------------   ---------------------------------------------
   0         Header / Magic   0xA55A
   1         Command ID       uint16_t
   2         Data Length      payload length N, in 16-bit words
   3         Payload[0]       present only if N >= 1
   ...       ...
   3+N-1     Payload[N-1]
   3+N       CRC16            CRC16-CCITT-FALSE over words [0 .. 3+N-1]
```

Frame size:

```
total_words = 4 + N           (Header + Command + Length + CRC = 4 overhead words)
min frame   = 4 words         (N = 0, empty payload)
```

- Overhead is a fixed **4 words** (`SPI_PACKET_V1_OVERHEAD_WORDS`).
- Payload occupies words `3 .. 3+N-1`.
- The CRC is always the **last word**, at index `3+N`.

ASCII layout for an empty-payload frame (N = 0):

```
+--------+--------+--------+--------+
| 0xA55A |  CMD   | 0x0000 |  CRC   |
+--------+--------+--------+--------+
   w0       w1       w2       w3
```

ASCII layout for a 3-word payload (N = 3):

```
+--------+--------+--------+------+------+------+--------+
| 0xA55A |  CMD   | 0x0003 |  P0  |  P1  |  P2  |  CRC   |
+--------+--------+--------+------+------+------+--------+
   w0       w1       w2      w3     w4     w5      w6
```

---

## 5. Field Definition

| Field        | Width   | Range / value         | Description |
|--------------|---------|-----------------------|-------------|
| Header       | 16-bit  | `0xA55A` (fixed)      | Frame start / magic. Any other value rejects the frame. |
| Command ID   | 16-bit  | `0x0000 .. 0xFFFF`    | Opaque command selector. Allocation policy in section 11. |
| Data Length  | 16-bit  | `0 .. MAX_PAYLOAD`    | Payload length **N** in 16-bit words. `0` = empty payload. |
| Payload      | N words | any                   | `N` 16-bit words of command-specific data. |
| CRC16        | 16-bit  | computed              | CRC16-CCITT-FALSE over Header+Command+Length+Payload. |

- `MAX_PAYLOAD` = `SPI_PACKET_V1_MAX_PAYLOAD_WORDS` = **4096 words**. This is the
  Packet V1 parser/spec limit and is **deliberately independent** of the legacy
  `SIZE_OF_SPI_BLOCK_RAM` (4095). See section 5.1 for the rationale. Packet V1
  does **not** reference the legacy symbol; it defines its own constant.
- All fields are unsigned. There is no signed interpretation anywhere in V1.
- There is no explicit version field in V1; the `0xA55A` magic plus the fixed
  layout *is* the version. A future V2 must use a different magic or add a
  version word (see section 12).

### 5.1 Design decision: payload limit is decoupled from legacy block size

`SPI_PACKET_V1_MAX_PAYLOAD_WORDS` (= **4096**) is purely the **Packet V1
parser/spec limit**. It is chosen by the wire format, not by any runtime buffer.

- It is **deliberately independent** of the legacy `SIZE_OF_SPI_BLOCK_RAM`
  (4095). The earlier draft pinned the two together; that coupling is removed so
  the pure-C packet format is not held hostage to a legacy transport detail. The
  one-word difference (4096 vs 4095) is intentional and harmless — it makes the
  independence explicit rather than accidental.
- A future SPIB runtime adapter that is constrained by DMA buffer size or the
  legacy block size **must segment or reject at the adapter layer**. It must
  **not** shrink `SPI_PACKET_V1_MAX_PAYLOAD_WORDS` or otherwise push a transport
  limitation back into the pure-C packet spec/parser.
- The Packet V1 pure-C module enforces this isolation in code: it does **not**
  `#include "spi_slave.h"`, does **not** `#include "SPI_master.h"`, and does
  **not** reuse the legacy `SPIB_PACKET_*` naming. Its constants
  (`SPI_PACKET_V1_*`) are its own namespace.

---

## 6. CRC16-CCITT-FALSE Definition

Packet V1 uses the standard **CRC16/CCITT-FALSE** algorithm.

| Parameter   | Value      |
|-------------|------------|
| Width       | 16 bits    |
| Polynomial  | `0x1021`   |
| Initial     | `0xFFFF`   |
| RefIn       | false      |
| RefOut      | false      |
| XorOut      | `0x0000`   |
| Check       | `0x29B1`   |

- **Check vector:** the ASCII string `"123456789"` (bytes `0x31 0x32 ... 0x39`)
  produces CRC `0x29B1`. This is the canonical self-test of the implementation.

> **C28x byte representation.** The target (TI C2000 / C28x) has no 8-bit type:
> `CHAR_BIT == 16` and `<stdint.h>` does not define `uint8_t`. The reference
> code therefore never uses `uint8_t`. Byte values are carried in `uint16_t`
> (only the low 8 bits are significant), and the CRC byte routine masks with
> `0xFF`. This keeps the same source compiling on both the C28x compiler and a
> host PC.

### 6.1 Byte processing (the core)

CRC16/CCITT-FALSE is defined over a **byte stream**. The bit-serial core is:

```
crc ^= (byte << 8)
repeat 8 times:
    if (crc & 0x8000) crc = (crc << 1) ^ 0x1021
    else              crc = (crc << 1)
crc &= 0xFFFF
```

### 6.2 Word processing (how a 16-bit word maps to bytes)

The packet is an array of 16-bit words, but CRC is byte-defined. Each 16-bit
word is fed to the CRC engine **big-endian: high byte first, then low byte**:

```
crc = UpdateByte(crc, (word >> 8) & 0xFF)   // MSB
crc = UpdateByte(crc, (word     ) & 0xFF)   // LSB
```

This byte order is fixed and part of the spec. It keeps the word-oriented CRC
consistent with a byte-oriented transmission of the same words.

### 6.3 CRC coverage

- CRC **covers**: Header (w0) + Command ID (w1) + Data Length (w2) + Payload
  (w3 .. 3+N-1).
- CRC **excludes**: the CRC word itself (w 3+N).
- Computation always starts from `Initial = 0xFFFF`.

```
crc = SpiPacketCrc16_ComputeWords(&frame[0], 3 + N)   // 3 + N words covered
frame[3 + N] = crc
```

---

## 7. Parser Rules

Given an input word array `words[0..wordCount-1]`, the parser applies these
checks **in order** and rejects on the first failure:

1. **Null / arg check.** `words` and the output struct must be non-NULL.
   -> `ERR_NULL_ARG`.
2. **Minimum length.** `wordCount >= SPI_PACKET_V1_OVERHEAD_WORDS` (4). If the
   frame is too short to even contain header+cmd+len+crc -> `ERR_TRUNCATED`.
3. **Header.** `words[0] == 0xA55A`. Otherwise -> `ERR_BAD_HEADER`.
4. **Declared length sanity.** `N = words[2]`; require `N <=
   MAX_PAYLOAD_WORDS`. Otherwise -> `ERR_PAYLOAD_TOO_LARGE`.
5. **Exact length match.** `wordCount == 4 + N`. The frame must be exactly the
   size its length field declares — no trailing or missing words. Otherwise
   -> `ERR_LENGTH_MISMATCH`.
6. **CRC.** Recompute CRC over `words[0 .. 2+N]` and compare with `words[3+N]`.
   Mismatch -> `ERR_CRC_MISMATCH`.

On success the parser fills the output struct:

```c
out->cmdId        = words[1];
out->payloadWords = N;
out->payload      = (N > 0) ? &words[3] : NULL;   // points into source buffer
out->crc          = words[3 + N];
```

The parser performs **no payload copy** — `out->payload` aliases the caller's
buffer. The caller owns the buffer lifetime.

---

## 8. Malformed Packet Handling

The parser is **total**: every input either parses cleanly or returns a specific
non-OK result code. It never reads out of bounds and never mutates the input.

| Condition                              | Result code               |
|----------------------------------------|---------------------------|
| NULL `words` or NULL output struct     | `ERR_NULL_ARG`            |
| `wordCount < 4`                         | `ERR_TRUNCATED`           |
| `words[0] != 0xA55A`                    | `ERR_BAD_HEADER`          |
| `words[2] > MAX_PAYLOAD_WORDS`          | `ERR_PAYLOAD_TOO_LARGE`   |
| `wordCount != 4 + words[2]`             | `ERR_LENGTH_MISMATCH`     |
| recomputed CRC != frame CRC            | `ERR_CRC_MISMATCH`        |
| all checks pass                         | `OK`                      |

Rules:

- On any non-OK result, the output struct contents are **undefined** and must
  not be used. (The reference implementation zeroes it for safety.)
- The order in section 7 is normative: e.g. a frame with both a bad header and
  a bad CRC reports `ERR_BAD_HEADER`, because the header is checked first.
- A length field larger than `MAX_PAYLOAD_WORDS` is rejected *before* the exact
  length comparison, so an absurd `N` can never drive a large read.

Encoder error handling (`SpiPacketV1_Encode`) is symmetric:

| Condition                                   | Result code              |
|---------------------------------------------|--------------------------|
| NULL output, or NULL payload with `N > 0`   | `ERR_NULL_ARG`           |
| `payloadWords > MAX_PAYLOAD_WORDS`          | `ERR_PAYLOAD_TOO_LARGE`  |
| `outCapacity < 4 + payloadWords`            | `ERR_BUFFER_TOO_SMALL`   |
| otherwise                                   | `OK`                     |

---

## 9. Command ID Allocation Policy

Command IDs are a flat 16-bit space, independent of legacy register addresses.

- **`0x0000`** — reserved (NULL / no-op). Do not assign.
- **`0x0001 .. 0x00FF`** — reserved for Packet V1 protocol/control commands
  (e.g. ping, version query, capability negotiation). Defined as needed.
- **`0x0100 .. 0x0FFF`** — application command range (downloads, config, etc.).
- **`0x1000 .. 0xFEFF`** — reserved for future expansion.
- **`0xFF00 .. 0xFFFF`** — reserved for diagnostics / experimental use.

Allocation rules:

- Command IDs are **append-only**. Once a command ID has a defined meaning, that
  meaning is frozen; new behavior takes a new command ID.
- The Packet V1 command space is **separate** from the legacy
  `*_spi_addr` register map in `SPIB_Slave/cmd_id.h`. There is no implied
  overlap or aliasing between a Packet V1 command ID and a legacy register
  address.
- This spec does not yet define any concrete command IDs. The first integration
  PR that consumes packets is responsible for allocating them from the ranges
  above and documenting them here.

---

## 10. (reserved)

---

## 11. Pure-C Test Matrix

The reference test (`spi_packet_v1_test.c`) is a standalone pure-C program that
can be built and run on a host PC. It must cover, at minimum:

| # | Test                          | Expectation |
|---|-------------------------------|-------------|
| 1 | CRC self-check                | `crc("123456789") == 0x29B1` |
| 2 | encode empty payload          | frame = `[0xA55A, cmd, 0, crc]`, len = 4 |
| 3 | parse empty payload           | OK, `cmdId` matches, `payloadWords == 0`, `payload == NULL` |
| 4 | encode/parse 3-word payload   | OK, payload bytes preserved, `payloadWords == 3` |
| 5 | bad header reject             | parse returns `ERR_BAD_HEADER` |
| 6 | length mismatch reject        | parse returns `ERR_LENGTH_MISMATCH` |
| 7 | CRC mismatch reject           | parse returns `ERR_CRC_MISMATCH` |
| 8 | payload too large reject      | encode and parse return `ERR_PAYLOAD_TOO_LARGE` |
| 9 | encode -> parse round-trip    | encode then parse yields identical cmd + payload |
| 10 | encode max payload (4096)    | OK, frame len = 4100 |
| 11 | parse max payload (4096)     | OK, `payloadWords == 4096` |
| 12 | encode over-max (4097)       | `ERR_PAYLOAD_TOO_LARGE` |
| 13 | parse declared length 4097   | `ERR_PAYLOAD_TOO_LARGE` |
| 14 | round-trip 4096 payload      | cmd id, `payloadWords == 4096`, first & last word preserved |

The test program returns exit code `0` when all cases pass, non-zero otherwise,
and prints a per-case PASS/FAIL line.

The harness (its `main()` and `<stdio.h>` use) is compiled only when
`SPI_PACKET_V1_HOST_TEST` is defined. This guard means that if
`spi_packet_v1_test.c` is ever included in the CCS firmware build, it produces
an empty translation unit — no second `main()`, no CIO/printf dependency, no
link collision with the firmware's `main()`. Host build:

```
gcc -std=c99 -Wall -Wextra -DSPI_PACKET_V1_HOST_TEST \
    spi_packet_crc16.c spi_packet_v1.c spi_packet_v1_test.c \
    -o spi_packet_v1_test && ./spi_packet_v1_test
```

---

## 12. Future SPIB Runtime Integration Rules

When (in a later, separate work package) Packet V1 is wired into the SPIB
runtime, the following rules apply. **None of this is done in this version.**

1. **Parser is pure.** `SpiPacketV1_ParseWords()` stays free of side effects and
   target dependencies. Runtime glue lives in a *separate* adapter translation
   unit, not inside `SPI_PacketV1/`.
2. **No legacy path regression.** The existing 2-word register frame and
   `SPIB_ParseRegFrame()` must keep working unchanged. Packet mode must be
   selected by an explicit, reversible switch — never by silently
   reinterpreting legacy frames.
3. **Magic disambiguation.** A receiver distinguishes a legacy register frame
   from a Packet V1 frame by the `0xA55A` header. Any ambiguity must resolve in
   favor of the legacy interpretation to protect the baseline.
4. **No new watch globals for free.** Any runtime state added during integration
   is justified and reviewed; it is not introduced by the pure-C core.
5. **Versioning.** A future Packet V2 uses a different magic or adds an explicit
   version word. V1 parsers must reject unknown versions rather than
   misinterpret them.
6. **Re-run the baseline.** Any integration PR must re-run B01F Test1~Test9 and
   confirm PASS, Test9 delta minimum >= 4107, and final page state LOCKED before
   merge.

---

## 13. B01F Isolation Checklist

Before and after any Packet V1 work, confirm:

- [ ] `SPI_PacketV1/` is **not** added to the CCS project build (no new sources
      compiled into the firmware image).
- [ ] No existing `.c` / `.h` file was modified.
- [ ] `main.syscfg` is unchanged.
- [ ] No linker / `.cmd` / memory-map file was changed.
- [ ] No SPIB DMA runtime code (`spi_b_slave.c`, `spi_pingpong.c`,
      `spi_fifo.c`, `wave_download.c`) was touched.
- [ ] No new `extern` global / CCS Watch variable was introduced.
- [ ] Packet V1 sources include only `<stdint.h>` (test harness may also use
      `<stdio.h>`, under `SPI_PACKET_V1_HOST_TEST`); no `driverlib`,
      `device.h`, `spi_slave.h`, or `wave_download.h`.
- [ ] No `uint8_t` (or other 8-bit exact-width type) is used — the C28x
      `<stdint.h>` does not define them.
- [ ] If `SPI_PacketV1/` is added to the CCS build, `spi_packet_v1_test.c` must
      build without `SPI_PACKET_V1_HOST_TEST` so it contributes no second
      `main()`.
- [ ] B01F Test1~Test9 still PASS; Test9 delta minimum >= 4107; final wave page
      state = LOCKED; legacy register protocol intact.
