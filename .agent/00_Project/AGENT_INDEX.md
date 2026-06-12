# ASR5K Agent Document Index

## Read First

Read these governance documents in this order before using any architecture,
milestone, handoff, research, or implementation material:

1. [`.agent/AGENT_ENTRYPOINT.md`](../AGENT_ENTRYPOINT.md)
2. [`.agent/00_Project/ARCHITECTURE_AUTHORITY.md`](ARCHITECTURE_AUTHORITY.md)
3. [`.agent/00_Project/ASR5K_DECISIONS.md`](ASR5K_DECISIONS.md)
4. [`.agent/DOCUMENT_STATUS_REGISTRY.md`](../DOCUMENT_STATUS_REGISTRY.md)
5. [`.agent/ARCHITECTURE_CONFLICT_REGISTER.md`](../ARCHITECTURE_CONFLICT_REGISTER.md)

## Current Project State

- [`.agent/00_Project/ASR5K_HANDOFF.md`](ASR5K_HANDOFF.md) summarizes the
  current engineering handoff and M5A technical direction.
- [`.agent/00_Project/ASR5K_STATUS.md`](ASR5K_STATUS.md) records governance and
  milestone status.

## Controlled Architecture

Architecture documents must be read in the authority order defined by
`ARCHITECTURE_AUTHORITY.md`: Formal Product Documents, D01, D02, D03, D04, D05,
D07, and D10. D11 is a research/candidate document and cannot select the
production protocol.

Use [`.agent/DOCUMENT_STATUS_REGISTRY.md`](../DOCUMENT_STATUS_REGISTRY.md) to
determine whether each document is `ACTIVE`, `SUPERSEDED`, `HISTORICAL`, or
`REFERENCE_ONLY`. A filename or this index is not a substitute for reading the
actual controlling document.

## Milestone Evidence

Documents under [`.agent/02_Milestones/`](../02_Milestones/) are milestone
evidence. M1 through M4 are closed. M5A Wave Download/Test9 is the current work.
Milestone documents cannot create or override architecture decisions.

## Rules and Workflows

- [`.agent/rules/`](../rules/) contains agent behavior and review constraints.
- [`.agent/workflows/`](../workflows/) contains operational procedures.

Rules and workflows are not architecture authorities. Where their content
conflicts with the governance core, the governance core controls.

## Research and Reference Material

Research, knowledge, hardware manuals, and candidate designs under
`.agent/01_Architecture/Research/` and `.agent/03_Knowledge/` are reference
material only unless explicitly promoted by an approved architecture decision.
