# SPI Packet V1 A4 — Pure-C Command Dispatcher Mock

Status: **Draft / Experimental (pure-C dispatcher mock + host unit tests)**
Repository: `ASR5K_WP_3352_SPI` split repo
Scope: `Emu_3352_SPI` / `SPI_PacketV1/` (pure C, standalone)
Builds on:
- A2 — total-buffer + streaming parser (`SPI_PACKET_V1_SPEC.md` §7/§10)
- A3-0 — legacy command map audit (`SPI_PACKET_V1_LEGACY_CMD_MAP_AUDIT.md`)
- A3 — command catalog & dispatch boundary spec (`SPI_PACKET_V1_COMMAND_CATALOG_A3.md`)

Current verification: A4 dispatcher host test **PASS=104 FAIL=0**; A2 baseline
unchanged at **PASS=126 FAIL=0**.

---

## 1. A4 Scope

A4 implements, as a **pure-C mock**, the command dispatcher that sits on the
A3 *dispatch boundary*: it consumes a packet the A2 parser has already validated
(framing + CRC) and produces a deterministic **response object** based solely on
`command_id` / payload length / payload content.

Deliverables (all under `Emu_3352_SPI/SPI_PacketV1/` and `Emu_3352_SPI/docs/`):

| File | Role |
|------|------|
| `SPI_PacketV1/spi_packet_v1_cmd.h` | command catalog constants, forbidden ranges, response field values, wire error codes |
| `SPI_PacketV1/spi_packet_v1_dispatch.h` | dispatch result enum, response object, `SpiPacketV1_Dispatch()` declaration |
| `SPI_PacketV1/spi_packet_v1_dispatch.c` | pure-C dispatcher implementation |
| `SPI_PacketV1/spi_packet_v1_dispatch_test.c` | host unit test (guarded by `SPI_PACKET_V1_HOST_TEST`) |
| `docs/SPI_PACKET_V1_A4_DISPATCHER_MOCK.md` | this document |

It is a "mock" in the sense that it computes a response **model**; it does not
emit anything onto a transport and is not wired into any runtime.

## 2. Non-Goals

A4 does **not**:

- integrate with the SPIB runtime, DMA, FIFO, or ping-pong buffers;
- change the legacy register protocol or include `cmd_id.h`;
- call any legacy register parser (`SPIB_ParseRegFrame()` etc.);
- touch `wave_download`, open `OUTPUT_ON`, or engage the power stage;
- verify CRC or do any framing (that is the A2 parser/encoder layer);
- handle malformed-framing cases (those remain A2 parser tests);
- add `syscfg` / linker / `.cproject` / `.project` / `.settings` / `.agent`
  changes, CCS-watch variables, or runtime globals;
- modify `SPIA_Master`, `SPIB_Slave` runtime, or any existing A2/A3 source.

All A4 experiments are isolated from the legacy B01F path.

## 3. Dispatcher Boundary

```
   SPI words ─▶ A2 parser ─▶ ST_SPI_PACKET_V1 ─┃─▶ SpiPacketV1_Dispatch() ─▶ ST_PKTV1_DISPATCH_RSP
   (transport)  (framing/CRC)  (validated view) ┃   (command semantics)        (response model)
                                                ┃
                                  ==== A4 dispatch boundary ====
```

- **Input:** `const ST_SPI_PACKET_V1 *req` — the A2 parser output
  (`cmdId`, `payloadWords`, `payload`, `crc`). The dispatcher reads `cmdId`,
  `payloadWords`, and (for ECHO) `payload`. It never re-checks CRC/framing.
- **Output:** `ST_PKTV1_DISPATCH_RSP *rsp` — a caller-owned response object with
  a fixed internal payload buffer. The dispatcher fills it and keeps no pointer.
- **Purity:** `SpiPacketV1_Dispatch()` is a pure function of `req`; it uses no
  globals and performs no I/O. Identical input always yields identical output.

## 4. Command Catalog

From A3 (`command_id` namespace `0x8000+`):

| `command_id` | Symbol               | Dir. | Request len | Response |
|--------------|----------------------|------|-------------|----------|
| `0x8000`     | `PKTV1_CMD_PING`        | H→S | 0 words | `{0x504F}` (pong) |
| `0x8001`     | `PKTV1_CMD_GET_VERSION` | H→S | 0 words | `{major, minor, patch, spec_rev}` = `{1,0,0,3}` |
| `0x8002`     | `PKTV1_CMD_GET_CAPS`    | H→S | 0 words | `{max_payload_words, feature_flags, crc_algo_id}` |
| `0x8003`     | `PKTV1_CMD_ECHO`        | H→S | 0..`MAX` | request payload copied back verbatim |
| `0x80FF`     | `PKTV1_RSP_ERROR`       | S→H | — | `{original_cmd, error_code}` (response-only) |

Deterministic response details:

- **PING** reply payload is the single fixed word `PKTV1_PONG_WORD = 0x504F`
  ("PO"). A4 narrows A3's optional-token idea: the **PING request must be length
  0**; a non-zero payload is `BAD_LENGTH` (keeps the mock deterministic).
- **GET_VERSION** reply = `{PKTV1_VERSION_MAJOR=1, _MINOR=0, _PATCH=0,
  PKTV1_SPEC_REV=3}`.
- **GET_CAPS** reply = `{ PKTV1_DISPATCH_MAX_PAYLOAD_WORDS,
  PKTV1_CAP_FEATURES, PKTV1_CAP_CRC_ALGO_CCITT_FALSE }`.
  - `max_payload_words` is the **dispatcher's** payload cap
    (`PKTV1_DISPATCH_MAX_PAYLOAD_WORDS = 64`), which is `<=` the wire/parser
    limit `SPI_PACKET_V1_MAX_PAYLOAD_WORDS = 4096`. A3 §4.2's `4096` was
    illustrative; the dispatcher advertises what it can actually service.
  - `feature_flags` advertises **meta commands only** (PING / GET_VERSION /
    GET_CAPS / ECHO / ERROR envelope). No wave / runtime / DMA capability is
    claimed.
- **ECHO** reply `command_id = 0x8003`, payload = an independent **copy** of the
  request payload (never an alias).
- **ERROR** reply `command_id = 0x80FF`, payload = `{original_cmd, error_code}`.

## 5. Forbidden Command Policy

`SpiPacketV1_Dispatch()` classifies a `command_id` as **forbidden** when it is:

- `== 0xA55A` (Packet V1 header magic), or
- in `0x0400 .. 0x0FFF` (legacy register addresses), or
- in `0x3000 .. 0x3FFF` (legacy block-data window), or
- `< 0x8000` (below the Packet V1 namespace — covers `0x0000`, `0x7FFF`, and the
  legacy ranges above).

A forbidden command yields `PKTV1_DISPATCH_ERR_FORBIDDEN_CMD` and an ERROR
response `{original_cmd, PKTV1_ERRCODE_FORBIDDEN}`. A `command_id >= 0x8000` that
is not in the catalog yields `PKTV1_DISPATCH_ERR_UNSUPPORTED_CMD`
(`PKTV1_ERRCODE_UNSUPPORTED`). `0x80FF` is response-only and is treated as
unsupported when received as a request.

> No legacy address is ever dispatched as a command. The dispatcher does not
> read, alias, or forward to the legacy register map.

## 6. Response / Error Model

Response object (`ST_PKTV1_DISPATCH_RSP`, caller-owned):

```c
typedef struct {
    uint16_t                command_id;
    uint16_t                payload_length_words;
    uint16_t                payload[PKTV1_DISPATCH_MAX_PAYLOAD_WORDS];  /* fixed; no aliasing */
    PKTV1_DISPATCH_RESULT_e status;
} ST_PKTV1_DISPATCH_RSP;
```

Dispatch result enum (`PKTV1_DISPATCH_RESULT_e`, also mirrored into `rsp.status`):

| Status | Meaning | Response on this status |
|--------|---------|-------------------------|
| `PKTV1_DISPATCH_OK`                 | command handled | the command's normal reply |
| `PKTV1_DISPATCH_ERR_NULL`           | `req`/`rsp` NULL, or payload NULL with len>0 | `rsp` zeroed (no error envelope) |
| `PKTV1_DISPATCH_ERR_FORBIDDEN_CMD`  | command_id forbidden | ERROR `{cmd, FORBIDDEN}` |
| `PKTV1_DISPATCH_ERR_UNSUPPORTED_CMD`| unknown `0x8000+` command | ERROR `{cmd, UNSUPPORTED}` |
| `PKTV1_DISPATCH_ERR_BAD_LENGTH`     | wrong payload length for command | ERROR `{cmd, BAD_LENGTH}` |
| `PKTV1_DISPATCH_ERR_RSP_OVERFLOW`   | response would exceed capacity | ERROR `{cmd, RSP_OVERFLOW}` |

Wire error codes in the ERROR payload word1: `PKTV1_ERRCODE_FORBIDDEN=1`,
`_UNSUPPORTED=2`, `_BAD_LENGTH=3`, `_RSP_OVERFLOW=4`.

**Fixed NULL rule (documented):** `req == NULL` or `rsp == NULL` → `ERR_NULL`. A
request with `payloadWords > 0` but `payload == NULL` is an API-level malformed
argument and also returns `ERR_NULL` (the `rsp` is zeroed; **no** ERROR envelope
is produced, because there is no trustworthy command to echo back). This rule is
distinct from a valid command carrying the wrong length, which is `BAD_LENGTH`.

**No aliasing:** `rsp.payload` is a fixed buffer inside the response object;
ECHO copies the request payload into it. The response never points into the
request buffer.

## 7. Unit Test Matrix

Host test `spi_packet_v1_dispatch_test.c` (guarded by `SPI_PACKET_V1_HOST_TEST`)
covers the required matrix. Each row is one or more assertions:

| # | Case | Expected |
|---|------|----------|
| 1 | PING | OK, reply `{0x504F}` |
| 2 | GET_VERSION | OK, reply `{1,0,0,3}` |
| 3 | GET_CAPS | OK, reply `{64, features, 1}` |
| 4 | ECHO len 0 | OK, empty reply |
| 5 | ECHO len 1 | OK, word preserved |
| 6 | ECHO multi-word | OK, payload preserved |
| 7 | ECHO max length (64) | OK, all words preserved |
| 8 | ECHO len 65 (over cap) | `ERR_RSP_OVERFLOW` + ERROR response |
| 9 | PING + payload | `ERR_BAD_LENGTH` |
| 10 | GET_VERSION + payload | `ERR_BAD_LENGTH` |
| 11 | GET_CAPS + payload | `ERR_BAD_LENGTH` |
| 12 | command `0x0400` | `ERR_FORBIDDEN_CMD` |
| 13 | command `0x0958` | `ERR_FORBIDDEN_CMD` |
| 14 | command `0x0960` | `ERR_FORBIDDEN_CMD` |
| 15 | command `0x3000` | `ERR_FORBIDDEN_CMD` |
| 16 | command `0x3FFF` | `ERR_FORBIDDEN_CMD` |
| 17 | command `0xA55A` | `ERR_FORBIDDEN_CMD` |
| 18 | command `0x0000` | `ERR_FORBIDDEN_CMD` |
| 19 | command `0x7FFF` | `ERR_FORBIDDEN_CMD` |
| 20 | unknown `0x8004` | `ERR_UNSUPPORTED_CMD` |
| 21 | unknown `0xFFFF` | `ERR_UNSUPPORTED_CMD` (plus `0x80FF` response-only → unsupported) |
| 22 | NULL request pointer | `ERR_NULL`, `rsp` zeroed |
| 23 | NULL response pointer | `ERR_NULL` |
| 24 | NULL payload + len>0 | `ERR_NULL` (fixed rule), `rsp` zeroed |
| 25 | repeated dispatch | deterministic, no hidden state |
| 26 | response vs input | distinct buffer; input mutation does not change reply |

### Build / run

A2 has no shared Makefile/CMake runner; A4 follows the same per-file gcc style,
scoped to `SPI_PacketV1/`, and does not touch the CCS project:

```bash
cd Emu_3352_SPI

# A4 dispatcher mock test (no CRC/parser link needed: input is the parsed view)
gcc -std=c99 -Wall -Wextra -DSPI_PACKET_V1_HOST_TEST \
    SPI_PacketV1/spi_packet_v1_dispatch.c \
    SPI_PacketV1/spi_packet_v1_dispatch_test.c \
    -o spi_packet_v1_dispatch_test.exe
./spi_packet_v1_dispatch_test.exe
rm -f spi_packet_v1_dispatch_test.exe

# A2 baseline (unchanged) — must still report PASS=126 FAIL=0
gcc -std=c99 -Wall -Wextra -DSPI_PACKET_V1_HOST_TEST \
    SPI_PacketV1/spi_packet_crc16.c \
    SPI_PacketV1/spi_packet_v1.c \
    SPI_PacketV1/spi_packet_v1_test.c \
    -o spi_packet_v1_test.exe
./spi_packet_v1_test.exe
rm -f spi_packet_v1_test.exe
```

## 8. Isolation Statement

- **No SPIB runtime integration** — the dispatcher never calls into the SPIB
  DMA/FIFO/ping-pong path or `runSPIBslave()`.
- **No legacy register protocol change** — `cmd_id.h` is neither included nor
  modified; no legacy register parser is called.
- **No DMA change.**
- **No `syscfg` / linker change.**
- **No `cmd_id.h` change.**
- No `.cproject` / `.project` / `.settings` / `.agent` change; no CCS-watch
  variables; no runtime globals; `SPIA_Master` / `SPIB_Slave` / `wave_download`
  untouched; power stage never engaged.
- A4 sources depend only on `<stdint.h>` (+ host test `<stdio.h>` under the
  guard) and the Packet V1 headers. The harness is compiled only when
  `SPI_PACKET_V1_HOST_TEST` is defined, so the files contribute no second
  `main()` and no CIO/printf to any firmware build.

## 9. A4 Host Test Result

```text
A4 dispatcher mock test : PASS=104  FAIL=0   (exit 0, 0 warnings)
A2 baseline (regression): PASS=126  FAIL=0   (exit 0, 0 warnings)
Combined                : PASS=230  FAIL=0
```

Compiler: `gcc -std=c99 -Wall -Wextra -DSPI_PACKET_V1_HOST_TEST`. The B01F
baseline is unaffected (A4 adds only standalone, non-built-in pure-C files).
