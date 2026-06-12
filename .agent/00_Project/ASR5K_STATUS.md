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
| M5A Wave Download/Test9 | CLOSED - HARDWARE VERIFIED |
| M5R Phase 2 Burst Transport | CLOSED - HARDWARE VERIFIED |

## M5 Technical Status

- `Emu_3352_SPI` Test1 through Test9 pass on hardware.
- The Legacy wave path uses `WAVE_BURST_BEGIN (0x095B)`, two guard frames,
  DMA CH3 ping-pong blocks, and one trailing frame.
- Full-page reception is verified at 4096 samples with continuous addresses,
  zero parse failures, and no SPIA/SPIB fault.
- Per-sample block ACKs are suppressed so the first post-burst control response
  remains aligned.
- Production EMIF1 backend integration remains separate from this emulator
  transport milestone.

## Frozen Ownership

- Legacy Register Protocol remains production.
- CPU1 owns parser, download, validation, and writes.
- CPU2 owns DDS runtime and reads waveform data from EMIF1 SDRAM.
- DMA CH3 belongs to SPIB RX; DMA CH4 belongs to SPIB TX.
- DMA CH5 and CH6 are reserved.

## M5R Phase 2 Evidence

See
[`M5R_PHASE2_BURST_TRANSPORT.md`](../02_Milestones/M5R_PHASE2_BURST_TRANSPORT.md)
for the protocol sequence, transport invariants, hardware evidence, and
regression watch list.
