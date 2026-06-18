# SPI Packet V1 A5 — Pure-C Parser → Dispatcher → Response Loopback

Status: **Draft / Experimental (pure-C loopback glue + host unit tests)**
Repository: `ASR5K_WP_3352_SPI` split repo
Scope: `Emu_3352_SPI` / `SPI_PacketV1/` (pure C, standalone)
Board-verifiable: **NO**
Builds on:
- A2 — total-buffer parser + encoder (`spi_packet_v1.{h,c}`)
- A3 — command catalog & dispatch boundary spec (`SPI_PACKET_V1_COMMAND_CATALOG_A3.md`)
- A4 — pure-C command dispatcher mock (`spi_packet_v1_dispatch.{h,c}`)

Current verification: A5 host test **PASS=107 FAIL=0**; A2 **PASS=126 FAIL=0**
and A4 **PASS=104 FAIL=0** unchanged (combined **PASS=337 FAIL=0**).

---

## 1. A5 Scope

A5 wires the existing pieces into one pure-C round trip on **raw 16-bit words**:
raw Packet V1 request words → A2 parser → A4 dispatcher → A2 response encoder →
raw Packet V1 response words. It adds a thin glue module plus a host test; it
introduces **no new wire behavior** and reuses the existing encoder/parser and
dispatcher unchanged.

Deliverables (under `Emu_3352_SPI/`):

| File | Role |
|------|------|
| `SPI_PacketV1/spi_packet_v1_loopback.h` | loopback result enum, diag struct, `SpiPacketV1_Loopback()` |
| `SPI_PacketV1/spi_packet_v1_loopback.c` | glue implementation (parser → dispatcher → encoder) |
| `SPI_PacketV1/spi_packet_v1_loopback_test.c` | host unit test (guarded by `SPI_PACKET_V1_HOST_TEST`) |
| `docs/SPI_PACKET_V1_A5_LOOPBACK.md` | this document |

## 2. Board-Verifiable: NO — why A5 is still pure-C

A5 is **not** a board / SPIB integration task. It verifies only the pure-C
command path in host memory: words in → words out. It does **not** touch the
SPIB runtime, DMA, FIFO, or any transport, so there is nothing to observe on
hardware. Do not claim A5 is board-testable.

## 3. Pipeline

```
request_words[]
   │
   ▼  SpiPacketV1_ParseWords()        (A2 — framing + CRC validation)
ST_SPI_PACKET_V1  (validated request view)
   │
   ▼  SpiPacketV1_Dispatch()          (A4 — command semantics)
ST_PKTV1_DISPATCH_RSP  (response model: normal reply or PKTV1_RSP_ERROR)
   │
   ▼  SpiPacketV1_Encode()            (A2 encoder, reused — shared CRC16)
response_words[]
```

Public API:

```c
PKTV1_LOOPBACK_RESULT_e SpiPacketV1_Loopback(
    const uint16_t         *req_words,
    uint16_t                req_word_count,
    uint16_t               *rsp_words,
    uint16_t                rsp_word_capacity,
    uint16_t               *rsp_word_count,
    ST_PKTV1_LOOPBACK_DIAG *diag);   /* optional, may be NULL */
```

`ST_PKTV1_LOOPBACK_DIAG` exposes per-stage detail: `result`, `parser_result`
(A2), `dispatch_result` (A4), `request_cmd`, `response_cmd`, `response_words`.

## 4. Parser Error Policy

If the A2 parser rejects the request (bad header / bad CRC / length mismatch /
truncated), the loopback returns `PKTV1_LOOPBACK_ERR_PARSE`, sets
`*rsp_word_count = 0`, and **produces no response frame**. The dispatcher is
**not** called — malformed wire framing is an A2 concern and is never dispatched
or answered with a fabricated response. `diag.parser_result` records the exact
A2 error; `diag.dispatch_result` and `diag.response_cmd` stay 0.

## 5. Dispatcher Error Policy

If the parser succeeds, the A4 dispatcher always yields a **valid response
model**: a normal reply on success, or a `PKTV1_RSP_ERROR` envelope on a semantic
error (forbidden / unsupported / bad-length / echo-overflow). The loopback
encodes that model into a valid Packet V1 response frame and returns
`PKTV1_LOOPBACK_OK`; the semantic detail is available in `diag.dispatch_result`
and in the response payload.

Example — legacy command `0x0958` (a forbidden value) produces a valid response
frame:

```
command_id = PKTV1_RSP_ERROR (0x80FF)
payload    = { 0x0958, PKTV1_ERRCODE_FORBIDDEN }
```

> Rationale for OK-with-diag: the loopback result answers "was a valid response
> frame produced?"; the command-semantic outcome lives in `diag.dispatch_result`
> and the error envelope. The only post-parse non-OK results are
> `PKTV1_LOOPBACK_ERR_RSP_OVERFLOW` (response frame does not fit the caller's
> buffer; no frame written, no overwrite) and the defensive
> `PKTV1_LOOPBACK_ERR_DISPATCH` (a dispatcher NULL result, which cannot occur
> after a successful parse).

## 6. Response Encoding Policy

The response frame is built with the **existing A2 encoder**
(`SpiPacketV1_Encode`) — no encoder logic is duplicated, and the shared
CRC16-CCITT-FALSE is reused. Frame format (A2 wire format):

```
Word0      : 0xA55A  (header magic)
Word1      : response command_id
Word2      : response payload length (N words)
Word3..2+N : response payload
Word3+N    : CRC16-CCITT-FALSE over words 0..2+N
```

The encoder validates `rsp_word_capacity` **before** writing, so an undersized
buffer yields `PKTV1_LOOPBACK_ERR_RSP_OVERFLOW` with zero bytes written (verified
by the "no memory overwrite" test).

## 7. Unit Test Matrix

Host test `spi_packet_v1_loopback_test.c` (guarded by `SPI_PACKET_V1_HOST_TEST`,
same `check()/g_pass/g_fail` style as A2/A4):

| # | Case | Expected |
|---|------|----------|
| 1-4 | PING | loopback OK; response parses OK; cmd `0x8000`; len 1; payload[0] `0x504F` |
| 5-6 | GET_VERSION | OK; payload `{1,0,0,3}` |
| 7-8 | GET_CAPS | OK; payload[0] `64` |
| 9 | ECHO len 0 | OK; empty response payload |
| 10 | ECHO len 1 | OK; payload preserved |
| 11 | ECHO multi-word | OK; payload preserved |
| 12 | ECHO max (64) | OK; every word preserved |
| 13 | ECHO len 65 | valid error response, errcode `RSP_OVERFLOW` |
| 14 | PING + payload | valid error response, `BAD_LENGTH` |
| 15 | GET_VERSION + payload | valid error response, `BAD_LENGTH` |
| 16 | GET_CAPS + payload | valid error response, `BAD_LENGTH` |
| 17 | command `0x0958` | valid error response, `FORBIDDEN` |
| 18 | command `0x0960` | valid error response, `FORBIDDEN` |
| 19 | command `0x3000` | valid error response, `FORBIDDEN` |
| 20 | command `0x3FFF` | valid error response, `FORBIDDEN` |
| 21 | command `0xA55A` | valid error response, `FORBIDDEN` |
| 22 | unknown `0x8004` | valid error response, `UNSUPPORTED` |
| 23 | unknown `0xFFFF` | valid error response, `UNSUPPORTED` |
| 24 | malformed bad header | `ERR_PARSE`, no response |
| 25 | malformed bad CRC | `ERR_PARSE`, no response |
| 26 | malformed length mismatch | `ERR_PARSE`, no response |
| 27 | NULL request pointer | `ERR_NULL` |
| 28 | NULL response pointer | `ERR_NULL` |
| 29 | NULL response length pointer | `ERR_NULL` |
| 30 | response capacity too small | `ERR_RSP_OVERFLOW`, no overwrite |
| 31 | repeated PING | deterministic |
| 32 | repeated ECHO | deterministic |
| 33 | response CRC validates | A2 parser accepts response; tampered response rejected |
| 34 | response vs input | distinct buffer; request mutation does not change response |
| 35 | parser failure ⇒ no dispatch | diag shows parse error, `dispatch_result`/`response_cmd` = 0 |

## 8. Exact Test Commands

From `Emu_3352_SPI`:

```bash
# A2 baseline (must stay PASS=126 FAIL=0)
gcc -std=c99 -Wall -Wextra -DSPI_PACKET_V1_HOST_TEST \
    SPI_PacketV1/spi_packet_crc16.c \
    SPI_PacketV1/spi_packet_v1.c \
    SPI_PacketV1/spi_packet_v1_test.c \
    -o spi_packet_v1_test && ./spi_packet_v1_test ; rm -f spi_packet_v1_test

# A4 dispatcher (must stay PASS=104 FAIL=0)
gcc -std=c99 -Wall -Wextra -DSPI_PACKET_V1_HOST_TEST \
    SPI_PacketV1/spi_packet_v1_dispatch.c \
    SPI_PacketV1/spi_packet_v1_dispatch_test.c \
    -o spi_packet_v1_dispatch_test && ./spi_packet_v1_dispatch_test ; rm -f spi_packet_v1_dispatch_test

# A5 loopback (uses A2 parser/encoder + A4 dispatcher + the new glue)
gcc -std=c99 -Wall -Wextra -DSPI_PACKET_V1_HOST_TEST \
    SPI_PacketV1/spi_packet_crc16.c \
    SPI_PacketV1/spi_packet_v1.c \
    SPI_PacketV1/spi_packet_v1_dispatch.c \
    SPI_PacketV1/spi_packet_v1_loopback.c \
    SPI_PacketV1/spi_packet_v1_loopback_test.c \
    -o spi_packet_v1_loopback_test && ./spi_packet_v1_loopback_test ; rm -f spi_packet_v1_loopback_test
```

All three compile warning-free under `-Wall -Wextra`.

## 9. Final PASS/FAIL Result

```text
A2 baseline       : PASS=126  FAIL=0   (exit 0, 0 warnings)
A4 dispatcher mock: PASS=104  FAIL=0   (exit 0, 0 warnings)
A5 loopback       : PASS=107  FAIL=0   (exit 0, 0 warnings)
Combined          : PASS=337  FAIL=0
```

## 10. Isolation Statement

A5 does **not**:

- integrate with the SPIB runtime, DMA, or FIFO;
- modify the legacy register protocol or include/modify `cmd_id.h`;
- modify `wave_download.*`, `SPI_master.*`, or `spi_b_slave.*`;
- modify `syscfg`, linker `.cmd`, `.cproject`, `.project`, `.settings`, `.agent`;
- add CCS-watch variables or runtime globals;
- touch `OUTPUT_ON`, power-stage logic, or OTP / BOOTCFG;
- modify any existing A2/A4 source — the parser, encoder, and dispatcher are
  reused as-is. A5 adds only new standalone files.

A5 sources depend only on `<stdint.h>` (+ host test `<stdio.h>` under the guard)
and the Packet V1 headers. The harness compiles only when
`SPI_PACKET_V1_HOST_TEST` is defined, so the files contribute no second `main()`
and no CIO/printf to any firmware build.

## 11. B01F Baseline Impact

**Unaffected.** A5 is pure-C and isolated: the new files are not in the CCS build
and are not referenced by any firmware translation unit, and no legacy / runtime
/ build / config file is touched. The compiled firmware and B01F Test1~Test9
cannot be changed by this work.
