# ASR5K Agent Entrypoint

## Purpose

This is the mandatory first-read file for any agent working in this repository.
It prevents architecture drift, accidental promotion of historical material,
and implementation based on unresolved document conflicts.

## Required Reading Order

Before analysis, planning, or modification:

1. Read this file.
2. Read `.agent/00_Project/ARCHITECTURE_AUTHORITY.md`.
3. Read `.agent/00_Project/ASR5K_DECISIONS.md`.
4. Read `.agent/ARCHITECTURE_CONFLICT_REGISTER.md`.
5. Read `.agent/DOCUMENT_STATUS_REGISTRY.md`.
6. Read applicable Formal Product Documents.
7. Read applicable D-series documents in authority order: D01, D02, D03, D04,
   D05, D07, and D10.
8. Read milestone or research material only after the controlled architecture
   set, and only for evidence or context.

## Mandatory Architecture Check

Every agent must confirm these assertions before architecture-sensitive work:

- Production protocol: Legacy Register Protocol.
- SPIB RX DMA channel: CH3.
- SPIB TX DMA channel: CH4.
- DMA CH5 state: reserved.
- DMA CH6 state: reserved.
- Wave Runtime Source: EMIF1 SDRAM.
- CPU1 role: system control, parser, and download path.
- CPU2 role: DDS runtime.
- W25Q64 role: Boot, OTA, and Maintenance only.
- W25Q64 DDS runtime role: prohibited.
- D11 Packet Protocol state: candidate only.
- Older PDF assumptions: overridden by D04, D05, and D07 in their approved
  wave-pipeline, memory-use, and runtime scopes.

## Document Use Rules

- `ACTIVE` documents may be used within their authority scope.
- `SUPERSEDED` documents may be read to locate obsolete claims but must not
  drive current design or implementation.
- `HISTORICAL` documents are evidence of completed or abandoned work only.
- `REFERENCE_ONLY` documents may inform investigation but cannot establish
  production architecture.
- A filename, index, summary, handoff, or prior audit is not a substitute for
  reading the actual controlling document.
- Research and milestone documents cannot create architecture decisions.
- Never resolve conflicting architecture by majority vote across documents.

## Stop Conditions

Stop architecture-changing work and report the issue when:

- an applicable conflict is not resolved by an approved decision or conflict
  register entry;
- a requested change would allocate DMA CH5 or CH6;
- a requested change makes CPU1 the DDS runtime owner;
- a requested change uses W25Q64 as the DDS runtime source;
- a requested change promotes Packet Protocol to production;
- a document's status or authority cannot be established.


## Mandatory AI Boot Guardrails

Before starting any task in the ASR5K project, the AI must verify and record:

1. Current repository root path.
2. Current Git branch and commit hash.
3. Total count of Markdown files under `.agent/`.
   - The count must be reported.
   - If the count differs from a previous audit, output a warning.
   - Do not fail solely because the count changed.
4. Explicit confirmation that `DOCUMENT_STATUS_REGISTRY.md` has been read and applied.
5. Explicit confirmation that `ASR5K_DECISIONS.md` has been read and applied.
6. Explicit confirmation that `SPEC_M5_WAVE_DOWNLOAD.md` has been read and applied.
7. Explicit confirmation that `0x095B` is parsed as a normal Legacy register write, NOT a Packet Protocol frame.
8. Explicit confirmation that `M5R_PHASE2_BURST_TRANSPORT.md` is treated strictly as `ACTIVE_EVIDENCE` for regression watch lists, not as an architectural specification.

## Required Boot Report

Before substantial work, record:

- task and intended scope;
- controlling documents actually read;
- frozen assertions relevant to the task;
- known conflicts and their registered resolutions;
- evidence still required;
- files intended for modification.

The boot report may be concise, but it must reflect actual document reads.
