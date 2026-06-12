# ASR5K Approved Decisions

## Status

- Governance state: ACTIVE
- Decision state: APPROVED AND FROZEN
- Scope: ASR5K production architecture

These decisions are the approved interpretation of the controlled architecture
set. Candidate documents, milestones, research notes, handoffs, workflows, and
implementation experiments cannot override them.

## Frozen Architecture Decisions

| ID | Approved Decision |
|---|---|
| FD-001 | Legacy Register Protocol is the production protocol. |
| FD-002 | SPIB RX owns DMA CH3. |
| FD-003 | SPIB TX owns DMA CH4. |
| FD-004 | DMA CH5 is reserved. |
| FD-005 | DMA CH6 is reserved. |
| FD-006 | EMIF1 SDRAM is the Wave Runtime Source. |
| FD-007 | CPU1 owns system control, parser, and download path. |
| FD-008 | CPU2 owns DDS runtime. |
| FD-009 | W25Q64 is for Boot, OTA, and Maintenance only. |
| FD-010 | W25Q64 is not a DDS runtime source. |
| FD-011 | D11 Packet Protocol is candidate only. |
| FD-012 | D04, D05, and D07 override older PDF assumptions for wave pipeline, EMIF1 memory use, and DDS runtime behavior. |
| FD-013 | Research and milestone documents cannot create architecture decisions. |

## Ownership Consequences

- CPU1 receives and parses production commands, controls system state, and owns
  the waveform download/write path.
- CPU2 consumes approved waveform data from EMIF1 SDRAM and owns DDS runtime
  execution.
- CPU1 may write and manage EMIF1 waveform data; this does not make CPU1 the DDS
  runtime owner.
- CPU2 runtime reads from EMIF1 SDRAM. It must not use W25Q64 as the live DDS
  waveform source.
- DMA CH5 and CH6 have no production owner. Allocation requires a new approved
  architecture decision.
- Packet framing described by D11 or older communication documents must not be
  implemented as the production protocol without explicit approval.

## Additional Approved Constraints

| ID | Approved Constraint |
|---|---|
| AC-001 | Ping-pong buffering is the production buffering model; FIFO remains experimental unless separately approved. |
| AC-002 | Runtime output restrictions and validation policy remain governed by D04 and D10. |
| AC-003 | Linker placement and shared-memory details must follow the currently approved memory architecture and verified linker evidence. |

## Scoped PDF Override

Formal Product Documents remain Tier 1. However, where older PDF assumptions
conflict with later controlled detail for the wave data pipeline, EMIF1 memory
use, or DDS runtime behavior, FD-012 explicitly selects D04, D05, and D07 for
those subjects. This is a scoped approved override, not a general demotion of
formal product documentation.

## Change Control

Changing any frozen decision requires explicit project approval and a matching
update to:

1. `ARCHITECTURE_AUTHORITY.md`
2. `ASR5K_DECISIONS.md`
3. `DOCUMENT_STATUS_REGISTRY.md`
4. `ARCHITECTURE_CONFLICT_REGISTER.md`

An implementation, test result, milestone note, research document, or candidate
protocol cannot change a frozen decision by itself.
