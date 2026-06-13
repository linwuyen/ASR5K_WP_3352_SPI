# ASR5K M5 Wave Download Service Specification

Version: 4.1
Status: ACTIVE SUPPORTING SPECIFICATION
Date: 2026-06-14

## 1. Authority and Scope

This specification defines the M5 waveform download service and its verified
Legacy burst transport. It is usable only where consistent with:

1. `.agent/00_Project/ASR5K_DECISIONS.md`
2. `.agent/ARCHITECTURE_CONFLICT_REGISTER.md`
3. D10 Wave Validation Policy
4. `.agent/02_Milestones/M5R_PHASE2_BURST_TRANSPORT.md`

The milestone supplies implementation evidence only. It does not create
architecture. D11 Packet Protocol remains candidate-only and is not the source
of the production command model.

This specification does not claim that the production EMIF1 backend or full
D10 checksum validation is hardware verified.

## 2. Frozen Architecture Boundaries

- Production protocol: Legacy Register Protocol.
- SPIB RX DMA channel: CH3.
- SPIB TX DMA channel: CH4.
- DMA CH5 and CH6: reserved.
- CPU1: parser, download, validation, and waveform writes.
- CPU2: DDS runtime and approved waveform reads.
- Wave Runtime Source: EMIF1 SDRAM.
- W25Q64: Boot, OTA, and Maintenance only; prohibited as the DDS runtime
  waveform source.

## 3. Data Flow

The hardware-verified emulator receive path is:

```text
AM3352 / emulator master
-> SPIB RX FIFO
-> DMA CH3
-> GSRAM RX ping-pong/block buffer
-> CPU1 Legacy parser
-> Wave Download Service
-> emulator storage abstraction
```

The frozen production continuation is:

```text
Wave Download Service
-> CPU1 validation and write path
-> EMIF1 SDRAM
-> CPU2 DDS runtime read path
```

The emulator fake-storage result does not verify the production EMIF1 backend.

## 4. Memory and Register Contract

- Samples per page: `4096`
- Sample width: `16-bit`
- Bytes per page: `8192`
- Maximum page count: `256`
- Wave data window: `0x3000..0x3FFF`
- Invalid page sentinel: `0xFFFF`
- EMIF1 base address: `0x80000000`

The byte-address formula is:

```text
0x80000000 + page_id * 8192 + sample_index * 2
```

Production Legacy register writes:

| Address | Name | Write data | Purpose |
|---|---|---|---|
| `0x0958` | `WAVE_PAGE_SELECT` | `0..255` | Select download page. |
| `0x0959` | `WAVE_DOWNLOAD_COMPLETE` | `1` | Declare transfer complete. |
| `0x095A` | `WAVE_PAGE_STATUS` | page/query value | Read page state through the existing Legacy response path. |
| `0x095B` | `WAVE_BURST_BEGIN` | `4096` | Pre-arm the verified block-DMA burst path. |
| `0x0960` | `WAVE_VALIDATE` | `1` | Validate the selected page. |
| `0x0961` | `WAVE_ACTIVATE` | `1` | Activate and lock a valid selected page. |

`WAVE_BURST_BEGIN` is a normal Legacy `ADDR + DATA` register write. It does not
introduce Packet Protocol framing.

## 5. Canonical Page State Model

The M5 page-state values are fixed to the D10-compatible model verified by the
current implementation and hardware tests:

| Value | State | Meaning |
|---|---|---|
| `0` | `EMPTY` | No download is present. |
| `1` | `DOWNLOADING` | Page is selected and receiving samples. |
| `2` | `DOWNLOAD_COMPLETE` | Host declared completion; validation has not passed. |
| `3` | `VALIDATING` | Background validation is running. |
| `4` | `VALID` | Current M5 validation checks passed. |
| `5` | `INVALID` | Validation failed; page cannot be activated. |
| `6` | `LOCKED` | Page is active and protected from mutation. |

The removed names `VALID_STUB`, `VALID_TEST`, `ACTIVE`, and `ERROR` are not
production state identifiers and must not be reintroduced as aliases.

`PageState == 6` means `LOCKED`.

### D10 Compliance Qualification

The current M5 implementation verifies page range, sample count, address
continuity, last address, download-complete state, and Output OFF. It does not
implement D10 per-sample checksum storage and comparison.

Therefore:

- `VALID=4` and `LOCKED=6` are the current hardware-verified M5 transport and
  lifecycle values.
- M5 transport verification is not full D10 production-validation closure.
- Production EMIF1 integration must not claim full D10 compliance until the
  checksum requirement is implemented and verified, or changed by an approved
  architecture decision.

## 6. Verified Burst Protocol

The lossless full-page sequence is:

```text
0x0958 = page_id
0x095B = 4096
2 x (0xFFFF, 0x0000) guard frames
0x3000..0x3FFF = 4096 sample frames
1 x (0xFFFF, 0x0000) trailing flush frame
0x0959 = 1
0x0960 = 1
0x0961 = 1
```

### Burst Preconditions

The slave accepts `WAVE_BURST_BEGIN` only when:

- Output is OFF.
- A valid page is selected.
- The announced sample count is exactly `4096`.

An invalid begin returns `0xFFFF` and must not arm block DMA.

### Transport Invariants

1. The slave parses `WAVE_BURST_BEGIN` and pre-arms DMA CH3 block reception
   before sample 0.
2. Two guard frames provide the main-loop opportunity required for the mode
   transition.
3. DMA is re-armed at each block boundary. Bounded force-trigger handling may
   drain FIFO backlog below the watermark so a later hardware edge can occur.
4. Direct per-sample ACK writes are suppressed during block parsing.
5. Burst MISO is filler and is ignored by the master.
6. One trailing frame flushes the final transfer before normal Legacy control
   traffic resumes.

A sender that omits `WAVE_BURST_BEGIN` may enter the compatibility overflow
detector. That path is best-effort and is not guaranteed lossless.

## 7. Requirements

### REQ-M5-001 Page Selection

For `0x0958` with data `0..255`, select the page, reset non-locked download
metadata, and enter `DOWNLOADING`. A `LOCKED` page must not be reset.

For an invalid page value, reject the selection and set the selected-page
sentinel to `0xFFFF`.

### REQ-M5-002 Sample Write

While a valid non-locked page is selected, writes to `0x3000..0x3FFF` must:

- enforce page and offset bounds;
- write through the storage abstraction;
- update sample count, last address, and address-continuity metadata;
- avoid direct physical Flash writes.

### REQ-M5-003 Download Complete

`0x0959 = 1` marks the selected page `DOWNLOAD_COMPLETE`. It must not directly
mark the page `VALID` or `LOCKED`.

### REQ-M5-004 Validation

`0x0960 = 1` runs in background context and must not run in the 100 kHz ISR.
The current M5 minimum checks are:

- valid selected page;
- sample count equals `4096`;
- address sequence is continuous;
- last address is `0x3FFF`;
- download-complete flag is set;
- Output is OFF.

Pass transitions to `VALID=4`; failure transitions to `INVALID=5`.

### REQ-M5-005 Activation

`0x0961 = 1` may activate only a selected page in `VALID=4` while Output is
OFF. Successful activation sets the active page and transitions it to
`LOCKED=6`.

Activation failure must leave the previous active page unchanged.

### REQ-M5-006 Locked-Page Protection

A `LOCKED` page rejects:

- write;
- erase;
- overwrite;
- validation overwrite;
- activation replacement that would mutate the locked page.

### REQ-M5-007 Output Protection

Output ON rejects waveform mutation, validation overwrite, activation, Flash
erase/write, and OTA update. The active page remains unchanged.

## 8. Interface Contract

The supporting interface shape is:

```c
typedef enum {
    WAVE_PAGE_STATE_EMPTY = 0,
    WAVE_PAGE_STATE_DOWNLOADING = 1,
    WAVE_PAGE_STATE_DOWNLOAD_COMPLETE = 2,
    WAVE_PAGE_STATE_VALIDATING = 3,
    WAVE_PAGE_STATE_VALID = 4,
    WAVE_PAGE_STATE_INVALID = 5,
    WAVE_PAGE_STATE_LOCKED = 6
} ST_WAVE_PAGE_STATE;
```

Required metadata:

```c
typedef struct {
    uint16_t u16SelectedPage;
    uint16_t u16ActivePage;
    uint16_t u16PageState[256];
    uint16_t u16SampleCount[256];
    uint16_t u16LastAddress[256];
    bool bAddressContinuous[256];
    bool bDownloadComplete[256];
} ST_WAVE_DOWNLOAD;
```

Implementation details may add diagnostics, but must preserve the state values,
Legacy addresses, frozen ownership, and bounds.

## 9. Production EMIF1 Gate

Production EMIF1 backend work may start only after:

1. This M5 governance repair is complete.
2. The canonical state model is used consistently.
3. `WAVE_BURST_BEGIN` and burst invariants are documented.
4. Tool declarations match executable implementations.
5. Minimum `.agent` checks pass.

Production closure additionally requires:

1. EMIF1 linker and address evidence.
2. CPU1 write and CPU2 read ownership verification.
3. D10 checksum implementation or an approved scoped decision.
4. RAM and Flash builds.
5. Manual hardware Test1 through Test9 regression.

## 10. Prohibited Interpretations

This specification must not be used to:

- allocate DMA CH5 or CH6;
- promote Packet Protocol;
- move DDS runtime to CPU1;
- use W25Q64 as the runtime waveform source;
- treat emulator fake storage as production EMIF1 verification;
- treat M5 transport evidence as full D10 checksum compliance.
