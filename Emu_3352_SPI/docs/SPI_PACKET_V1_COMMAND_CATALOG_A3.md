# SPI Packet V1 Command Catalog & Dispatch Boundary Spec (A3)

Status: **Draft / Proposal (documentation only — no code, no runtime integration)**
Repository: `ASR5K_WP_3352_SPI` split repo
Scope: `Emu_3352_SPI` / `SPI_PacketV1/` (pure-C contract; spec only)
Builds on:
- A2 — total-buffer + streaming parser (`SPI_PACKET_V1_SPEC.md` §7, §10)
- A3-0 — legacy command map audit (`SPI_PACKET_V1_LEGACY_CMD_MAP_AUDIT.md`)

> **A3 proper, paper spec only.** This document defines (1) the Packet V1
> `command_id` namespace and an initial command catalog, and (2) the *dispatch
> boundary* — the pure-C contract between the parser and a future command-handler
> layer. It **implements nothing**: no SPIB runtime integration, no DMA, no
> legacy register-protocol changes, no `syscfg`/linker/`.cproject`/`.settings`/
> `.agent` changes, and **no `command_id` that collides with a `cmd_id.h` legacy
> register address**.

---

## 1. Purpose

A2 gives us a validated frame: a parser turns wire words into an
`ST_SPI_PACKET_V1 { cmdId, payloadWords, payload, crc }`. A2 deliberately treats
`cmdId` as **opaque** — it validates framing/CRC but assigns no meaning to the
command.

A3 adds the next layer *on paper*:

1. **Command Catalog** — a collision-free `command_id` namespace plus a small,
   frozen set of initial meta commands.
2. **Dispatch Boundary** — the contract describing how a (future, separate)
   handler layer consumes a parsed packet and produces a response, and the hard
   rules that keep that layer isolated from the legacy register protocol and the
   SPIB runtime.

No handler code is written here. This spec is the reviewable design that a later,
explicitly-scoped work package would implement and test (with its own pure-C host
tests and golden vectors, mirroring A2).

---

## 2. Relationship to existing specs

- **A2 parser output is the dispatch input.** The dispatch layer only ever sees a
  packet that already returned `SPI_PACKET_V1_OK`. Malformed frames are rejected
  by the parser (`SPI_PACKET_V1_SPEC.md` §7/§8) and never reach dispatch.
- **A3-0 audit defines the forbidden numbers.** Legacy `*_spi_addr` addresses
  occupy `0x0400`–`0x0FFF` and `0x3000`–`0x3FFF`. The catalog below is chosen to
  sit entirely above `0x7FFF`, so it is **collision-free by construction**.
- **Supersedes the placeholder policy in `SPI_PACKET_V1_SPEC.md` §9.** That
  section sketched an allocation (`0x0100`–`0x0FFF` "application range") *before*
  the legacy map was audited; that range **overlaps** legacy addresses
  `0x0400`–`0x0FFF` and must not be used for `command_id`. This A3 catalog is the
  authoritative allocation. In this A3 change, `SPEC.md` §9 is updated to point
  here and to record that the old `0x0100`–`0x0FFF` sketch is withdrawn.

---

## 3. Command ID Namespace (collision-free by construction)

`command_id` is the frame's Word1 — a flat 16-bit value, **independent of and
disjoint from** the legacy register address space.

### 3.1 Forbidden / reserved values (never assign as a `command_id`)

| Value / range        | Reason |
|----------------------|--------|
| `0x0000`             | NULL / no-op sentinel (also matches A2 "failure-output" zeroing). |
| `0x0400`–`0x0FFF`    | **Legacy register addresses** (RO, calibration, R/W, setting/fault, control, wave/M5). See A3-0 audit. |
| `0x3000`–`0x3FFF`    | **Legacy block-data** streaming window. See A3-0 audit. |
| `0xA55A`             | Packet V1 **header magic** (Word0). Never assign as a command to avoid dump confusion. |

> The legacy values `0x5AB0` (`SPIB_STATUS_PACKET_MAGIC`) and `0x8000`/`0x8001`
> (block-FSM `OVERFLOW`/`ERROR`) are *status/magic values*, not register
> addresses and not command IDs — they live in different fields, so there is no
> `command_id` collision. They are noted here only so the `0x8000+` choice below
> is documented as intentional, not accidental.

### 3.2 Packet V1 `command_id` bands

All Packet V1 commands live **at or above `0x8000`**, which is strictly above the
highest legacy address (`0x3FFF`) — guaranteeing no `cmd_id.h` collision.

| Band              | Purpose |
|-------------------|---------|
| `0x8000`–`0x80FF` | **Protocol / meta / control** commands (ping, version, caps, echo, error). Allocated in §4. |
| `0x8100`–`0x8FFF` | Packet V1 application / management commands (future; unallocated). |
| `0x9000`–`0x9FFF` | **Reserved** for Packet V1 wave-related future commands (do not allocate until the wave-path question in §9 is answered). |
| `0xA000`–`0xAFFF` | **Reserved.** Explicit carve-out: `command_id == 0xA55A` is permanently forbidden (header-magic confusion). |
| `0xB000`–`0xFEFF` | Reserved for future expansion. |
| `0xFF00`–`0xFFFF` | Diagnostics / experimental. |

### 3.3 Allocation rules

- **Append-only / frozen.** Once a `command_id` has a defined meaning, that
  meaning never changes; new behavior takes a new ID.
- **Disjoint from legacy.** No `command_id` ever equals a `*_spi_addr` value.
  The forbidden ranges in §3.1 are normative.
- **No legacy aliasing.** A Packet V1 command never implies a legacy register
  access by number. If a legacy-register bridge is ever wanted, it is a *new*
  command in the `0x8000+` space whose payload names the address (see §9 Q1) —
  the command itself still does not reuse a legacy address.

---

## 4. Initial Command Catalog (meta commands only)

All entries are **Proposed — not yet implemented**. Direction is from the host
(SPI master / AM3352) toward the slave (C2000) unless noted. Lengths are in
16-bit words (matching the wire format).

| `command_id` | Name                       | Dir.   | Request payload                     | Response payload |
|--------------|----------------------------|--------|-------------------------------------|------------------|
| `0x8000`     | `PKTV1_CMD_PING`           | H→S    | 0 words, or 1-word opaque token     | echoes the token (0 or 1 word) |
| `0x8001`     | `PKTV1_CMD_GET_VERSION`    | H→S    | 0 words                             | spec/impl version words (see §4.1) |
| `0x8002`     | `PKTV1_CMD_GET_CAPS`       | H→S    | 0 words                             | capability descriptor (see §4.2) |
| `0x8003`     | `PKTV1_CMD_ECHO`           | H→S    | N words (`0 ≤ N ≤ MAX_PAYLOAD`)     | the same N words, unchanged |
| `0x80FF`     | `PKTV1_RSP_ERROR`          | S→H    | (n/a — this *is* a response)        | error descriptor (see §5) |

### 4.1 `GET_VERSION` response (proposed shape)

Response is a packet with `command_id = 0x8001` and payload:

```
word0 = PKTV1_SPEC_VERSION_MAJOR   (e.g. 0x0001)
word1 = PKTV1_SPEC_VERSION_MINOR   (e.g. 0x0000)
word2 = PKTV1_IMPL_BUILD_ID        (opaque build tag, optional)
```

### 4.2 `GET_CAPS` response (proposed shape)

Response is a packet with `command_id = 0x8002` and payload advertising what the
implementation supports, so a host can negotiate without hard-coding:

```
word0 = capability bitmap (bit0 = streaming parser, bit1 = echo,
                           bit2 = error-response envelope, …)
word1 = MAX_PAYLOAD_WORDS  (= 4096; from SPI_PACKET_V1_MAX_PAYLOAD_WORDS)
word2 = CRC algorithm id   (1 = CRC16/CCITT-FALSE)
```

> The catalog deliberately stops at meta commands. No application, control, or
> wave commands are allocated by A3 — those wait on the open questions in §9.

---

## 5. Response & Error Model (proposed)

### 5.1 Response addressing convention

A response packet **reuses the request's `command_id`** (e.g. a `GET_VERSION`
reply is itself a `0x8001` packet whose payload carries the version). The single
exception is the dedicated error envelope `PKTV1_RSP_ERROR` (`0x80FF`), used when
the request cannot be answered in its own command shape.

This convention is a **proposal** — see §9 Q4 for the half-duplex correlation
question it leaves open.

### 5.2 Dispatch-layer error codes

These are **distinct from** the A2 parser result enum (`SPI_PACKET_V1_RESULT_e`).
Parser errors are framing/CRC problems and are reported by A2 *before* dispatch.
Dispatch errors are command-semantic problems, carried inside a `PKTV1_RSP_ERROR`
(`0x80FF`) packet:

```
word0 = error code (see table)
word1 = offending command_id (the request's Word1, for correlation)
```

| Proposed error code            | Meaning |
|--------------------------------|---------|
| `PKTV1_ERR_UNKNOWN_CMD`        | `command_id` not in the catalog. |
| `PKTV1_ERR_BAD_PAYLOAD_LEN`    | Payload length wrong for this command. |
| `PKTV1_ERR_UNSUPPORTED`        | Command known but not implemented on this build. |
| `PKTV1_ERR_INTERNAL`           | Handler-internal failure. |

---

## 6. Dispatch Boundary Contract (pure-C, design only)

The **dispatch boundary** is the line where wire-format ends and command
*semantics* begin:

```
   SPI words ──▶ A2 parser ──▶ ST_SPI_PACKET_V1 ──┃──▶ command handler ──▶ response packet
   (transport)   (framing/CRC)   (validated)      ┃    (semantics)
                                                  ┃
                                   ==== DISPATCH BOUNDARY ====
```

Everything left of the boundary is pure framing (A2, already done). Everything
right of it interprets `command_id`. A3 specifies the boundary; it does not build
the right-hand side.

### 6.1 Conceptual dispatcher (illustrative, NOT to be built in this WP)

```c
/* Illustrative signature only — not implemented in A3. */
typedef SPI_PACKET_V1_RESULT_e (*PktV1_Handler)(
    const ST_SPI_PACKET_V1 *req,     /* validated request (parser output)     */
    uint16_t               *outBuf,  /* caller-owned response word buffer      */
    uint16_t                outCap,  /* capacity of outBuf, in words           */
    uint16_t               *outLen); /* receives response frame length         */

/* A dispatch table maps command_id -> handler; unknown -> error response.    */
```

### 6.2 Boundary rules (normative for any future implementation)

1. **Runs only on `OK`.** The dispatcher is invoked only for a packet the parser
   accepted. It never re-validates framing/CRC and never sees a malformed frame.
2. **Closed world.** An unknown `command_id` produces a `PKTV1_RSP_ERROR`
   (`PKTV1_ERR_UNKNOWN_CMD`). It must **never** fall through to legacy register
   handling or any default side effect.
3. **No legacy coupling.** A handler must not read/write the legacy `*_spi_addr`
   register map, must not call `SPIB_ParseRegFrame()`, and must not change legacy
   register-protocol behavior.
4. **No SPIB runtime / DMA.** A handler must not touch the SPIB RX/TX DMA path,
   ping-pong buffers, or `runSPIBslave()`. Wiring dispatch to a transport is a
   separate, later adapter WP (gated by `SPI_PACKET_V1_SPEC.md` §12).
5. **Pure & isolated.** Handlers are pure-C, depend only on `<stdint.h>` (+ the
   Packet V1 headers), introduce no `extern`/CCS-watch globals, and live in their
   own translation unit separate from both `SPI_PacketV1/` core and the SPIB
   runtime. State is caller-owned (same discipline as A2).
6. **No build coupling.** Adding the dispatch layer must not modify `syscfg`,
   linker/`.cmd`, `.cproject`, `.settings`, or `.agent`, and must not be added to
   the firmware CCS build until an explicit integration WP says so.

### 6.3 What the boundary guarantees

- The B01F baseline and the legacy register protocol are **unaffected** by the
  existence of a Packet V1 command layer, because nothing crosses the boundary
  back toward legacy state.
- A2's framing guarantees (total parser ↔ streaming parser equivalence, CRC
  coverage, failure-output safety) are preserved; dispatch sits strictly above
  them and cannot weaken them.

---

## 7. Isolation / Non-Goals (restated)

This WP does **not**:

- integrate with the SPIB runtime or DMA;
- modify the legacy register protocol or any `cmd_id.h` value;
- allocate any `command_id` inside a legacy address range (`0x0400`–`0x0FFF`,
  `0x3000`–`0x3FFF`) or equal to `0xA55A`;
- add or modify `syscfg`, linker, `.cproject`, `.settings`, or `.agent`;
- add code, tests, or build entries — it is a specification only.

---

## 8. Verification note

A3 is a paper spec; there is **no host-test change** in this WP (A2 remains at
`PASS=126 FAIL=0`). When the catalog/dispatch layer is implemented in a future,
explicitly-scoped WP, it must ship with its own pure-C host tests and golden
vectors (e.g. PING round-trip, ECHO payload preservation, unknown-command →
`PKTV1_RSP_ERROR`), built behind a host-test macro exactly as A2's tests are, so
nothing leaks into the firmware build.

---

## 9. Open Questions (carry into the implementation WP)

1. **Legacy-register wrapper command?** Should there be a
   `PKTV1_CMD_LEGACY_REG_ACCESS` (in `0x8100+`) whose payload is
   `{ address, value, op }`, tunnelling a legacy register read/write *without*
   the command itself reusing a legacy address? (Deferred; the namespace leaves
   room.)
2. **Wave commands: mirror or new?** Should Packet V1 wave commands (band
   `0x9000`) mirror legacy `WAVE_*` semantics or define a fresh set?
3. **Keep legacy wave download fully legacy for B01F?** i.e. Packet V1 stays away
   from the wave path entirely until much later.
4. **Half-duplex response correlation.** On a half-duplex SPI link, how is a
   response matched to its request — reuse-`command_id` (this draft) plus a
   sequence/token word in the payload, or a dedicated response-bit convention?
5. **`GET_CAPS` contents.** Exact capability bitmap layout and whether it should
   advertise spec version, max payload, CRC params, and streaming support.

---

## 10. Recommendation

- Treat §3 (namespace) and §6 (dispatch boundary rules) as the stable,
  reviewable core; treat §4/§5 (catalog/error shapes) as proposals to be
  ratified before any code is written.
- Do **not** implement handlers until this spec is reviewed and the §9 questions
  are resolved.
- As a small follow-up, reconcile `SPI_PACKET_V1_SPEC.md` §9 to reference this
  catalog (so the superseded `0x0100`–`0x0FFF` sketch can't be mistaken for an
  allocatable range).
