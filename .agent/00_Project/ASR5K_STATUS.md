# ASR5K Project Status

## Governance

- Phase 2A Governance Core: PASS.
- The authority hierarchy, frozen decisions, document status registry, conflict
  register, and mandatory agent entrypoint are established.
- Source conflicts remain in D01-D05 and D11, but their interpretation is
  governed by
  [`.agent/ARCHITECTURE_CONFLICT_REGISTER.md`](../ARCHITECTURE_CONFLICT_REGISTER.md).
- Conflicting source text has not yet been corrected.

## Milestones

| Milestone | Status |
|---|---|
| M1-M4 | CLOSED |
| M5A Wave Download/Test9 | CURRENT WORK - UNRESOLVED |

## M5 Technical Status

- The master can send 4096 waveform samples.
- The slave's old two-word DMA restart model cannot reliably receive the
  complete waveform stream.
- Current direction: `SPIB RX FIFO -> DMA CH3 -> GSRAM RX packet/block -> EMIF1 SDRAM`.
- The receive-block design, complete-stream verification, overrun handling, and
  transfer into EMIF1 SDRAM remain unresolved.

## Frozen Ownership

- Legacy Register Protocol remains production.
- CPU1 owns parser, download, validation, and writes.
- CPU2 owns DDS runtime and reads waveform data from EMIF1 SDRAM.
- DMA CH3 belongs to SPIB RX; DMA CH4 belongs to SPIB TX.
- DMA CH5 and CH6 are reserved.

## Phase 2B Scope

This phase updates project index, handoff, and status documentation only. No
firmware source code and no D01-D11 document is modified in Phase 2B.
