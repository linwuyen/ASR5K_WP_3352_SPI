# ASR5K Project Handoff

## Required Reading

Start with [`.agent/AGENT_ENTRYPOINT.md`](../AGENT_ENTRYPOINT.md), then follow
the authority, decision, status-registry, and conflict-register reading order.
This handoff summarizes current work but does not create architecture.

## Current State

- M1 through M4 are closed.
- M5A Wave Download/Test9 is the current work.
- The SPI master can send 4096 waveform samples.
- The slave's old two-word DMA restart model cannot reliably receive the full
  waveform stream.
- The M5 technical issue remains unresolved.

## Current Technical Direction

The current receive and storage direction is:

`SPIB RX FIFO -> DMA CH3 -> GSRAM RX packet/block -> EMIF1 SDRAM`

The intended responsibilities are:

1. SPIB RX receives the stream through the hardware FIFO.
2. DMA CH3 transfers an RX packet or block into GSRAM.
3. CPU1 validates and moves the accepted waveform data into EMIF1 SDRAM.
4. CPU2 reads approved waveform data from EMIF1 SDRAM for DDS runtime.

This direction replaces reliance on repeatedly restarting DMA for each old
two-word receive unit. Detailed buffering, block size, completion signaling,
and error recovery still require implementation verification.

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

- A successful 4096-sample master transmission does not prove complete slave
  reception.
- The old two-word DMA restart path can lose continuity across a sustained
  waveform stream.
- GSRAM packet/block capacity, DMA completion boundaries, overrun handling, and
  the GSRAM-to-EMIF1 move must be validated together.
- Source conflicts remain in D01-D05 and D11. Apply
  [`.agent/ARCHITECTURE_CONFLICT_REGISTER.md`](../ARCHITECTURE_CONFLICT_REGISTER.md)
  rather than following their conflicting statements.

## Next Verification Target

Demonstrate that Test9 can receive the complete 4096-sample stream through DMA
CH3 into bounded GSRAM packet/block storage, detect transfer errors or overruns,
and move validated waveform data into EMIF1 SDRAM without changing the frozen
protocol or CPU ownership model.
