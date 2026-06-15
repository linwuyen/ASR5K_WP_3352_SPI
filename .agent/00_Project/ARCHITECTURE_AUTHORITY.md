# ASR5K Architecture Authority

## Purpose

This file defines the only approved authority order for interpreting ASR5K
architecture. It applies to agents, reviewers, planning documents, workflows,
milestones, and implementation work.

## Authority Hierarchy

| Tier | Authority |
|---|---|
| Tier 0 | Boot / Registry / Authority Meta Rules (`AGENT_ENTRYPOINT.md`, `DOCUMENT_STATUS_REGISTRY.md`, `ARCHITECTURE_AUTHORITY.md`) |
| Tier 1 | Approved Decisions (`ASR5K_DECISIONS.md`) |
| Tier 2 | Approved Conflict Register (`ARCHITECTURE_CONFLICT_REGISTER.md`) |
| Tier 3 | ACTIVE SPEC (`SPEC_FIRMWARE_ARCH.md`, `SPEC_M5_WAVE_DOWNLOAD.md`, D07, D10) |
| Tier 4 | ACTIVE_EVIDENCE (`M5R_PHASE2_BURST_TRANSPORT.md`) |
| Tier 5 | Project State / Handoff (`ASR5K_STATUS.md`, `ASR5K_HANDOFF.md`) |
| Tier 6 | REFERENCE_ONLY (peripherals, DMA/IPC manuals, test SOPs) |
| Tier 7 | HISTORICAL (completed milestone evidence logs) |
| Tier 8 | SUPERSEDED (obsolete D-series, formal PDFs, workflows) |


The D-series names refer to the controlled documents under
`.agent/01_Architecture/ASR5K設計文件/`. D02 includes its controlled companion
documents, such as D02_2_1 and D02_2_3.

## Interpretation Rules

1. Read higher tiers before lower tiers.
2. A lower-tier document cannot silently override a higher-tier document.
3. Approved Decisions and the Approved Conflict Register may explicitly freeze
   a decision or mark a named statement as superseded. Such an explicit
   resolution controls that specific conflict; it does not promote the entire
   lower-tier document above the hierarchy.
4. When documents at the same tier disagree, do not guess. Use an approved
   decision or conflict-register entry. If none exists, report the ambiguity
   and stop architecture-changing work.
5. Milestone evidence (`ACTIVE_EVIDENCE` or `HISTORICAL`) provides implementation evidence only. `ACTIVE_EVIDENCE` may validate an implementation but must not define or override architecture. Milestone PASS does not imply D10 checksum compliance is closed. Checksum verification remains a distinct production requirement.
6. Research, proposals, experiments, and candidate documents (`REFERENCE_ONLY`) provide context only. They cannot create or change production architecture.
7. Rules and workflows constrain agent behavior and procedure. They are not
   architecture sources.

## Frozen Architecture Assertions

- Legacy Register Protocol is the production protocol.
- SPIB RX owns DMA CH3.
- SPIB TX owns DMA CH4.
- DMA CH5 is reserved.
- DMA CH6 is reserved.
- EMIF1 SDRAM is the Wave Runtime Source.
- CPU1 owns system control, parser, and download path.
- CPU2 owns DDS runtime.
- W25Q64 is for Boot, OTA, and Maintenance only.
- W25Q64 is not a DDS runtime source.
- D11 Packet Protocol is candidate only.
- D04, D05, and D07 override older PDF assumptions for the wave data pipeline,
  EMIF1 memory use, and DDS runtime behavior.
- Research and milestone documents cannot create architecture decisions.

## Mandatory Conflict Handling

Before using a statement that affects CPU ownership, DMA ownership, protocol
selection, runtime memory source, or flash use:

1. Check `.agent/00_Project/ASR5K_DECISIONS.md`.
2. Check `.agent/ARCHITECTURE_CONFLICT_REGISTER.md`.
3. Check `.agent/DOCUMENT_STATUS_REGISTRY.md`.
4. Reject statements marked superseded, historical, or reference-only as
   production authority.
5. Report any unresolved conflict instead of inventing a resolution.

## Change Control

The hierarchy and frozen assertions may change only through explicit project
approval. Editing a milestone, research note, workflow, handoff, or candidate
document does not constitute approval.
