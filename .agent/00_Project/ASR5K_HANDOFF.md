# ASR5K Project Handoff

## Required Reading

Start with [`.agent/AGENT_ENTRYPOINT.md`](../AGENT_ENTRYPOINT.md), then follow
the authority, decision, status-registry, and conflict-register reading order.
This handoff summarizes current work but does not create architecture.

## Current State

- M1 through M4 are closed.
- M5A Wave Download/Test9 and M5R Phase 2 burst transport are hardware verified.
- Test1 through Test9 pass with a complete 4096-sample page.
- The current emulator evidence has zero parse failures and no SPIA/SPIB fault.

## Verified Transport

The hardware-verified emulator transport is:

`SPIB RX FIFO -> DMA CH3 -> GSRAM RX block -> Wave Download Service`

The frozen production direction continues from the Wave Download Service to
EMIF1 SDRAM.

The `Emu_3352_SPI` transport uses:

1. Legacy `WAVE_BURST_BEGIN (0x095B)` with sample count 4096.
2. Two guard frames before sample 0.
3. DMA CH3 block reception with bounded force-trigger re-arming.
4. Suppressed per-sample block ACKs to keep post-burst control replies aligned.
5. One trailing frame before normal Legacy control traffic resumes.

Detailed evidence and the regression watch list are in
[`M5R_PHASE2_BURST_TRANSPORT.md`](../02_Milestones/M5R_PHASE2_BURST_TRANSPORT.md).

## Frozen Architecture Boundaries

- Legacy Register Protocol remains the production protocol.
- SPIB RX owns DMA CH3.
- SPIB TX owns DMA CH4.
- DMA CH5 and CH6 remain reserved.
- CPU1 owns system control, parser, download, validation, and waveform writes.
- CPU2 owns DDS runtime and waveform reads.
- EMIF1 SDRAM is the Wave Runtime Source.
- W25Q64 is not a DDS runtime source.
- D11 Packet Protocol remains candidate-only.

## Handoff Risks

- A sender that omits `WAVE_BURST_BEGIN` falls back to a best-effort overflow
  detector and is not guaranteed lossless.
- Burst MISO is intentionally not a per-sample acknowledgement stream.
- Changes to guard count, block size, FIFO watermark, force-trigger behavior,
  or ACK suppression require the full Test1 through Test9 regression.
- The emulator fake-storage result does not by itself verify the production
  EMIF1 backend.
- Source conflicts remain in D01-D05 and D11. Apply
  [`.agent/ARCHITECTURE_CONFLICT_REGISTER.md`](../ARCHITECTURE_CONFLICT_REGISTER.md)
  rather than following their conflicting statements.

## Next Verification Target

Integrate and verify the production EMIF1 storage backend without changing the
verified Legacy burst transport or the frozen CPU/DMA ownership model.

Entry to that work is gated by:

1. Passing `python .agent/ci/run_checks.py`.
2. Preserving the canonical `EMPTY=0` through `LOCKED=6` state model.
3. Implementing and verifying D10 checksum behavior, or obtaining a separately
   approved scoped decision.
