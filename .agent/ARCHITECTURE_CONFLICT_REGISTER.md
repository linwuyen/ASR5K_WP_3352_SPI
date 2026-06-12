# ASR5K Architecture Conflict Register

## Status

- Registry state: ACTIVE
- Resolution state: APPROVED
- Source-document cleanup: PENDING

This register records approved interpretations for known conflicts. It does not
rewrite the source documents. Until those documents are corrected, agents must
apply the resolutions below and report the affected source as superseded.

## Registered Conflicts

| ID | Conflict | Conflicting Claims | Approved Resolution | Affected Documents | Cleanup State |
|---|---|---|---|---|---|
| CR-001 | CPU1 DDS ownership conflict | Older architecture and milestone text assigns DDS algorithm, output, or direct runtime execution to CPU1. | CPU1 owns system control, parser, and download/write path. CPU2 owns DDS runtime and consumes waveform data from EMIF1 SDRAM. | D01, D02, D05 wording, M5 Legacy Command to DDS | OPEN - source correction deferred |
| CR-002 | CH5/CH6 ownership conflict | D-series documents assign DMA CH5/CH6 to SPIA RX/TX, Boot, or OTA flows. | DMA CH5 is reserved. DMA CH6 is reserved. Neither channel has a production owner without a new approved decision. | D02, D03, D04, D05, D11 | OPEN - source correction deferred |
| CR-003 | Legacy vs Packet protocol conflict | Some communication documents describe Header/Command/Length/Payload/CRC packet framing as the implementation model. | Legacy Register Protocol is the production protocol. Packet Protocol is candidate-only and cannot drive production implementation. | D02, D02_2_1, D11, related plans | OPEN - source correction deferred |
| CR-004 | EMIF1 ownership wording conflict | Some documents describe EMIF1 as CPU1-exclusive or blur memory administration with runtime consumption. | EMIF1 SDRAM is the Wave Runtime Source. CPU1 owns download, validation, and writes; CPU2 reads approved waveform data for DDS runtime. | D03, D05, older PDF assumptions | OPEN - wording correction deferred |
| CR-005 | D11 protocol-selection conflict | D11 leaves Legacy and Packet protocols as candidates or leaves production selection open. | D11 is a candidate/reference document only. Production selection is frozen to Legacy Register Protocol. | D11 | OPEN - source correction deferred |

## Application Rules

1. The Approved Resolution column governs each listed conflict.
2. Do not combine conflicting statements into a new hybrid architecture.
3. Do not treat implementation evidence as approval to change a resolution.
4. Do not allocate CH5/CH6, promote Packet Protocol, move DDS runtime to CPU1,
   or use W25Q64 as a DDS runtime source.
5. If implementation depends on an affected statement, cite this conflict ID
   and use the approved resolution.
6. Source documents remain unchanged until a separately approved cleanup phase.

## Unresolved Source Conflicts

The five architecture questions above are resolved for interpretation, but the
conflicting statements still remain in D01, D02-family, D03, D04, D05, D11, and
older milestone/PDF material. Their textual cleanup is intentionally unresolved
because Phase 2A does not authorize edits to D01-D11, formal product documents,
milestones, rules, or workflows.
