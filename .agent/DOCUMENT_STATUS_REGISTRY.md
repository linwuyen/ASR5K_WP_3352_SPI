# ASR5K Document Status Registry

## Status Definitions

| Status | Meaning |
|---|---|
| ACTIVE | Current and usable within its authority and stated scope. |
| SUPERSEDED | Contains obsolete or conflicting claims. Only content explicitly preserved by an approved decision or conflict resolution may be used. |
| HISTORICAL | Retained as evidence of past work, results, or abandoned direction. |
| REFERENCE_ONLY | Useful context or candidate material, but not production architecture authority. |

Status does not replace the authority hierarchy. An ACTIVE lower-tier document
still cannot override a higher-tier source. Directory-group entries cover every
file below that directory unless a more specific entry assigns another status.

## Governance and Project Documents

| File | Status | Governing Note |
|---|---|---|
| `.agent/AGENT_ENTRYPOINT.md` | ACTIVE | Mandatory first-read and boot procedure. |
| `.agent/DOCUMENT_STATUS_REGISTRY.md` | ACTIVE | Controlled status classification. |
| `.agent/ARCHITECTURE_CONFLICT_REGISTER.md` | ACTIVE | Approved resolution of known conflicts. |
| `.agent/00_Project/ARCHITECTURE_AUTHORITY.md` | ACTIVE | Current authority hierarchy. |
| `.agent/00_Project/ASR5K_DECISIONS.md` | ACTIVE | Frozen production decisions. |
| `.agent/00_Project/AGENT_INDEX.md` | ACTIVE | Phase 2B governance-first document index. |
| `.agent/00_Project/ASR5K_HANDOFF.md` | ACTIVE | Current M5A Wave Download/Test9 handoff. |
| `.agent/00_Project/ASR5K_STATUS.md` | ACTIVE | Current governance and milestone status. |
| `.agent/00_Project/M3_HANDOFF_PROMPT.md` | HISTORICAL | M3-era handoff evidence only. |

## Architecture Documents

| File or Pattern | Status | Governing Note |
|---|---|---|
| `.agent/01_Architecture/ASR5K設計文件/asr5k_readme.pdf` | ACTIVE | Formal product document; scoped wave/runtime assumptions are subject to the approved D04/D05/D07 override. |
| `.agent/01_Architecture/ASR5K設計文件/D01_DESIGN_CONTROL_ARCHITECTURE*.md` | SUPERSEDED | CPU1 DDS algorithm/output ownership conflicts with frozen CPU2 DDS runtime ownership. |
| `.agent/01_Architecture/ASR5K設計文件/D02_COMM_ARCHITECTURE*.md` | SUPERSEDED | Contains CPU1 waveform-core, Packet Protocol, and CH5/CH6 allocation conflicts. |
| `.agent/01_Architecture/ASR5K設計文件/D02_2_1_SPIB_TO_AM3352*.md` | SUPERSEDED | DMA CH3/CH4 mapping is valid, but packet-buffer and CRC framing claims conflict with Legacy production protocol. |
| `.agent/01_Architecture/ASR5K設計文件/D02_2_3_I2CA_MASTER_TO_M0G3519*.md` | REFERENCE_ONLY | I2C proposal/reference; not a frozen production decision. |
| `.agent/01_Architecture/ASR5K設計文件/D03_MEMORY_ARCHITECTURE*.md` | SUPERSEDED | Runtime-source claims are useful, but obsolete ownership wording must be resolved through governance. |
| `.agent/01_Architecture/ASR5K設計文件/D04_WAVE_DATA_PIPELINE*.md` | SUPERSEDED | Wave runtime guidance is retained only in the scope preserved by approved decisions and conflict resolutions. |
| `.agent/01_Architecture/ASR5K設計文件/D05_EMIF1_MEMORY_MAP*.md` | SUPERSEDED | EMIF1 map is relevant, but ownership wording conflicts with frozen decisions. |
| `.agent/01_Architecture/ASR5K設計文件/D07_DDS_RUNTIME_MANAGER*.md` | ACTIVE | Current DDS runtime and EMIF1 source authority; interpreted with CPU2 runtime ownership decision. |
| `.agent/01_Architecture/ASR5K設計文件/D10_WAVE_VALIDATION_POLICY*.md` | ACTIVE | Current validation-policy authority. |
| `.agent/01_Architecture/ASR5K設計文件/D11_AM3352_PROTOCOL*.md` | REFERENCE_ONLY | Protocol study/candidate only; cannot select production protocol. |
| `.agent/01_Architecture/SPEC_FIRMWARE_ARCH.md` | ACTIVE | Supporting firmware architecture where consistent with higher authority. |
| `.agent/01_Architecture/SPEC_M5_WAVE_DOWNLOAD.md` | ACTIVE | Supporting download specification where consistent with frozen ownership and protocol decisions. |
| `.agent/01_Architecture/cpu1_research_plan.md` | REFERENCE_ONLY | CPU1 research plan; not current project status or production authority. |
| `.agent/01_Architecture/cpu2_research_plan.md` | REFERENCE_ONLY | CPU2 research plan; not current project status or production authority. |
| `.agent/01_Architecture/ASR5K設計文件/R*.md` | REFERENCE_ONLY | R-series research documents. |
| `.agent/01_Architecture/ASR5K設計文件/[音檔說明]*.md` | REFERENCE_ONLY | Audio-derived explanatory notes. |
| `.agent/01_Architecture/ASR5K設計文件/ASR5K_M0_COMM*.md` | REFERENCE_ONLY | M0 communication reference material. |
| `.agent/01_Architecture/ASR5K設計文件/M0_README*.md` | REFERENCE_ONLY | M0 reference overview. |
| `.agent/01_Architecture/ASR5K設計文件/剖析硬體非線性誤差來源與校準補償策略說明*.md` | REFERENCE_ONLY | Calibration research note. |
| `.agent/01_Architecture/ASR5K設計文件/設定校準（Setting Calibration）*.md` | REFERENCE_ONLY | Calibration reference note. |

## Rules

| File | Status | Governing Note |
|---|---|---|
| `.agent/rules/ASR5K_Context.md` | ACTIVE | Behavioral and project context; explicitly defers to the governance core. |
| `.agent/rules/rules.md` | ACTIVE | General engineering behavior rules; explicitly defers to the governance core. |
| `.agent/rules/Review_Checklist.md` | ACTIVE | Review procedure; not an architecture authority. |

## Workflows

| File or Pattern | Status | Governing Note |
|---|---|---|
| `.agent/workflows/**` | SUPERSEDED | Existing Antigravity workflow bundle contains stale paths or unsupported actions and must not control current work until repaired. |

## Milestone and Handoff Documents

| File | Status | Governing Note |
|---|---|---|
| `.agent/02_Milestones/M1_M4_VERIFIED_PATH.md` | HISTORICAL | Consolidated M1-M4 path evidence. |
| `.agent/02_Milestones/M1_SPI_BASELINE.md` | HISTORICAL | Completed M1 baseline evidence. |
| `.agent/02_Milestones/M2_FIFO.md` | HISTORICAL | Experimental FIFO evidence; not the runtime main path. |
| `.agent/02_Milestones/M2R_RXFRAME_PINGPONG.md` | HISTORICAL | Completed M2R milestone evidence. |
| `.agent/02_Milestones/M2R_RXFRAME_PINGPONG/RxFramePingPong_Contract.md` | HISTORICAL | M2R design-contract evidence. |
| `.agent/02_Milestones/M3_CPU1_DMA_RXFRAME_INTEGRATION.md` | HISTORICAL | Completed M3 integration evidence. |
| `.agent/02_Milestones/M4_LEGACY_COMMAND_VALIDATION.md` | HISTORICAL | Completed M4 validation evidence. |
| `.agent/02_Milestones/M5_WAVE_DOWNLOAD_PLAN.md` | SUPERSEDED | Preparation plan does not represent the current unresolved M5A receive direction. |
| `.agent/02_Milestones/M5_LEGACY_COMMAND_TO_DDS_INTEGRATION.md` | SUPERSEDED | Direct DDS execution assumptions conflict with frozen CPU ownership. |
| `.agent/02_Milestones/M5R_PORTABILITY_REVIEW.md` | HISTORICAL | M5A-era portability snapshot and review evidence. |

## Knowledge and Operational Material

| File or Pattern | Status | Governing Note |
|---|---|---|
| `.agent/03_Knowledge/**` | REFERENCE_ONLY | Supporting knowledge and binary references; validate against controlled sources before use. |
| `.agent/skills/**` | REFERENCE_ONLY | Operational tooling and instructions; not architecture authority. |

## Usage Rule

When a document contains both useful and obsolete content, the whole document
remains SUPERSEDED until its conflicting statements are corrected and reviewed.
Agents may use only the portions explicitly preserved by an approved decision
or conflict-register resolution.
