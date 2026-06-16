# ASR5K Agent Reorg Manifest

Scope: inventory and proposed destination planning only. No moves performed.

Legend:
- `move_action`: `git mv` when path is confidently classified, `review` when human review is required.
- `authority_tier`: from `ARCHITECTURE_AUTHORITY.md`.
- `registry_status`: from `DOCUMENT_STATUS_REGISTRY.md`.

## Governance

| current_path | filename | file_type | first_heading | detected_role | registry_status | authority_tier | proposed_new_path | move_action | reason | link_update_required | human_review_required |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `.agent/AGENT_ENTRYPOINT.md` | `AGENT_ENTRYPOINT.md` | md | `# ASR5K Agent Entrypoint` | boot / mandatory read | ACTIVE | Tier 0 | `.agent/00_Project/Governance/AGENT_ENTRYPOINT.md` | git mv | governance core; should live with project governance | yes | no |
| `.agent/DOCUMENT_STATUS_REGISTRY.md` | `DOCUMENT_STATUS_REGISTRY.md` | md | `# ASR5K Document Status Registry` | status registry | ACTIVE | Tier 0 | `.agent/00_Project/Governance/DOCUMENT_STATUS_REGISTRY.md` | git mv | governance core; controlled status ledger | yes | no |
| `.agent/ARCHITECTURE_CONFLICT_REGISTER.md` | `ARCHITECTURE_CONFLICT_REGISTER.md` | md | `# ASR5K Architecture Conflict Register` | conflict register | ACTIVE | Tier 2 | `.agent/00_Project/Governance/ARCHITECTURE_CONFLICT_REGISTER.md` | git mv | governance core; conflict authority | yes | no |
| `.agent/00_Project/ARCHITECTURE_AUTHORITY.md` | `ARCHITECTURE_AUTHORITY.md` | md | `# ASR5K Architecture Authority` | authority hierarchy | ACTIVE | Tier 0 | `.agent/00_Project/Governance/ARCHITECTURE_AUTHORITY.md` | git mv | governance core; authority rule set | yes | no |
| `.agent/00_Project/ASR5K_DECISIONS.md` | `ASR5K_DECISIONS.md` | md | `# ASR5K Approved Decisions` | frozen decisions | ACTIVE | Tier 1 | `.agent/00_Project/Governance/ASR5K_DECISIONS.md` | git mv | governance core; approved decisions | yes | no |
| `.agent/00_Project/AGENT_INDEX.md` | `AGENT_INDEX.md` | md | `# ASR5K Agent 文件索引` | current index | ACTIVE | Tier 5 | `.agent/00_Project/Current_State/AGENT_INDEX.md` | git mv | current-state index | yes | no |
| `.agent/00_Project/ASR5K_HANDOFF.md` | `ASR5K_HANDOFF.md` | md | `# ASR5K Project Handoff` | handoff | ACTIVE | Tier 5 | `.agent/00_Project/Current_State/ASR5K_HANDOFF.md` | git mv | current-state handoff | yes | no |
| `.agent/00_Project/ASR5K_STATUS.md` | `ASR5K_STATUS.md` | md | `# ASR5K Project Status` | current status | ACTIVE | Tier 5 | `.agent/00_Project/Current_State/ASR5K_STATUS.md` | git mv | current-state status | yes | no |
| `.agent/00_Project/M5_GOVERNANCE_REPAIR_REPORT.md` | `M5_GOVERNANCE_REPAIR_REPORT.md` | md | `# M5 Governance Repair Report` | governance report | ACTIVE | Tier 5 | `.agent/00_Project/Governance/M5_GOVERNANCE_REPAIR_REPORT.md` | git mv | governance evidence, not architecture authority | yes | no |
| `.agent/00_Project/PROJECT_AUDIT.md` | `PROJECT_AUDIT.md` | md | `# ASR5K Project Audit Report (WP_3352_SPI)` | audit report | HISTORICAL | Tier 7 | `.agent/00_Project/02_Historical/PROJECT_AUDIT.md` | git mv | historical audit evidence | yes | no |
| `.agent/00_Project/M3_HANDOFF_PROMPT.md` | `M3_HANDOFF_PROMPT.md` | md | `# M3 Handoff Prompt` | historical handoff prompt | HISTORICAL | Tier 7 | `.agent/00_Project/02_Historical/M3_HANDOFF_PROMPT.md` | git mv | historical evidence only | yes | no |

## Architecture

| current_path | filename | file_type | first_heading | detected_role | registry_status | authority_tier | proposed_new_path | move_action | reason | link_update_required | human_review_required |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `.agent/01_Architecture/SPEC_FIRMWARE_ARCH.md` | `SPEC_FIRMWARE_ARCH.md` | md | `# ASR5K 韌體系統架構深度規格書 (Firmware Architecture Deep Spec)` | active architecture spec | ACTIVE | Tier 3 | `.agent/01_Architecture/ACTIVE/SPEC_FIRMWARE_ARCH.md` | git mv | active spec | yes | no |
| `.agent/01_Architecture/SPEC_M5_WAVE_DOWNLOAD.md` | `SPEC_M5_WAVE_DOWNLOAD.md` | md | `# ASR5K M5 Wave Download Service Specification` | active architecture spec | ACTIVE | Tier 3 | `.agent/01_Architecture/ACTIVE/SPEC_M5_WAVE_DOWNLOAD.md` | git mv | active spec | yes | no |
| `.agent/01_Architecture/cpu1_research_plan.md` | `cpu1_research_plan.md` | md | `# ASR5K_F28388D_CPU1 研究與開發計畫` | research | REFERENCE_ONLY | Tier 6 | `.agent/01_Architecture/REFERENCE_ONLY/cpu1_research_plan.md` | git mv | research note | yes | no |
| `.agent/01_Architecture/cpu2_research_plan.md` | `cpu2_research_plan.md` | md | `# ASR5K_F28388D_CPU2 研究與開發計畫` | research | REFERENCE_ONLY | Tier 6 | `.agent/01_Architecture/REFERENCE_ONLY/cpu2_research_plan.md` | git mv | research note | yes | no |
| `.agent/01_Architecture/ASR5K設計文件/*` | `ASR5K設計文件/*` | md/pdf | mixed | formal product / design docs | mixed | Tier 3/6/8 | see per-file handling in move phase | review | contains active, reference, superseded, and formal product material; requires file-by-file placement | yes | yes |

## Milestones

| current_path | filename | file_type | first_heading | detected_role | registry_status | authority_tier | proposed_new_path | move_action | reason | link_update_required | human_review_required |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `.agent/02_Milestones/M5R_PHASE2_BURST_TRANSPORT.md` | `M5R_PHASE2_BURST_TRANSPORT.md` | md | `# M5R Phase 2 Burst Transport` | active evidence | ACTIVE_EVIDENCE | Tier 4 | `.agent/02_Milestones/ACTIVE_EVIDENCE/M5R_PHASE2_BURST_TRANSPORT.md` | git mv | verified evidence only | yes | no |
| `.agent/02_Milestones/*` | milestone docs | md | mixed | milestone evidence | HISTORICAL or SUPERSEDED | Tier 7/8 | `.agent/02_Milestones/{HISTORICAL,SUPERSEDED}/...` | review | status varies by file; use registry row-by-row in move phase | yes | yes |

## Knowledge

| current_path | filename | file_type | first_heading | detected_role | registry_status | authority_tier | proposed_new_path | move_action | reason | link_update_required | human_review_required |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `.agent/03_Knowledge/ASR5K_MODULE_ARCHITECTURE.md` | `ASR5K_MODULE_ARCHITECTURE.md` | md | `# ASR5K SPI 模組架構` | reference architecture note | REFERENCE_ONLY | Tier 6 | `.agent/03_Knowledge/Reference_Only/ASR5K_MODULE_ARCHITECTURE.md` | git mv | reference only | yes | no |
| `.agent/03_Knowledge/HardWare_Test/*` | hardware test docs | md/pdf | mixed | hardware test SOP | REFERENCE_ONLY | Tier 6 | `.agent/03_Knowledge/Hardware_Test/...` | git mv | hardware test material | yes | no |
| `.agent/03_Knowledge/knowledge/*` | knowledge docs | md | mixed | general reference | REFERENCE_ONLY | Tier 6 | `.agent/03_Knowledge/Reference_Only/...` | git mv | reference only | yes | no |
| `.agent/03_Knowledge/Peripheral/*` | peripheral docs | md/pdf | mixed | TRM / peripheral / datasheet / hardware notes | REFERENCE_ONLY | Tier 6 | `.agent/03_Knowledge/Reference_Only/...` | git mv | peripheral reference | yes | no |
| `.agent/03_Knowledge/SPI_Docs/*` | SPI docs | md/pdf/png | mixed | SPI / DMA / IPC reference | REFERENCE_ONLY | Tier 6 | `.agent/03_Knowledge/SPI_Docs/...` | git mv | SPI reference | yes | no |
| `.agent/03_Knowledge/hrpwm_hw_config_porting_guide.md` | `hrpwm_hw_config_porting_guide.md` | md | `# ASR5K 底層 HRPWM 與硬體參數配置移植說明文件` | reference note | REFERENCE_ONLY | Tier 6 | `.agent/03_Knowledge/Reference_Only/hrpwm_hw_config_porting_guide.md` | git mv | hardware/config reference | yes | no |
| `.agent/03_Knowledge/pwm_adc_dac_timing_analysis.md` | `pwm_adc_dac_timing_analysis.md` | md | `# ASR5K 系統外設（PWM, ADC, DAC）設置與高速時序對齊分析報告` | reference analysis | REFERENCE_ONLY | Tier 6 | `.agent/03_Knowledge/Reference_Only/pwm_adc_dac_timing_analysis.md` | git mv | reference analysis | yes | no |

## Rules / Tooling / Workflows

| current_path | filename | file_type | first_heading | detected_role | registry_status | authority_tier | proposed_new_path | move_action | reason | link_update_required | human_review_required |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `.agent/rules/*` | rules docs | md | mixed | active rules | ACTIVE | Tier 6 | `.agent/05_Rules/ACTIVE/...` | git mv | active behavior rules | yes | no |
| `.agent/ci/*` | ci docs/scripts | md/py | mixed | tooling | REFERENCE_ONLY | Tier 6 | `.agent/06_Tooling/ci/...` | git mv | tooling references | yes | no |
| `.agent/skills/*` | skills docs/scripts | md/py/yaml/txt | mixed | skills | REFERENCE_ONLY | Tier 6 | `.agent/06_Tooling/skills/...` | git mv | skills/tooling references | yes | no |
| `.agent/workflows/*` | workflow docs | md/yaml | mixed | workflows | SUPERSEDED | Tier 8 | `.agent/04_Workflows/SUPERSEDED/...` | git mv | workflows are superseded per registry | yes | no |

## Human Review Flags

- Any file under `.agent/01_Architecture/ASR5K設計文件/` that is not already explicitly covered above.
- Any file whose status is not explicit in the registry.
- Any file with only a filename clue and no clear content/status marker.
