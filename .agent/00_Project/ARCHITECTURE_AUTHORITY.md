# ASR5K Architecture Authority Mapping

This document specifies the authoritative hierarchy and priority of all files inside the `.agent/` directory. All developers and AI agents MUST adhere to this classification when reading, writing, or modifying code.

---

## 1. Documentation Hierarchy & Authority Levels

To ensure system consistency, documents are divided into four distinct tiers with descending authority:

### Tier 1: System Rules & Constraints (Highest Authority)
These files define strict behavioral constraints and coding boundaries for AI agents and human developers.
* **[ASR5K_Context.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/.agent/rules/ASR5K_Context.md)**: Hardware/Software configurations, CPU roles, core data flow model.
* **[rules.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/.agent/rules/rules.md)**: Core programming guidelines and minimal change directives.
* **[Review_Checklist.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/.agent/rules/Review_Checklist.md)**: Review criteria.

### Tier 2: Active System Specifications
These files describe the current, verified, and officially active architecture of the firmware modules. They represent the current source of truth for design integration.
* **[SPEC_FIRMWARE_ARCH.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/.agent/01_Architecture/SPEC_FIRMWARE_ARCH.md)**: Global system-level mapping, dual-core memory regions, ePWM/ADC scheduler timing, and hardware validation modules.
* **[CPU1_ARCH_DESIGN.MD](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/.agent/03_Knowledge/Peripheral/DMA/CPU1_ARCH_DESIGN.MD)**: Official CPU1 DMA precision multi-stage daisy-chain specification.
* **[CPU2_ARCH_DESIGN.MD](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/.agent/03_Knowledge/Peripheral/DMA/CPU2_ARCH_DESIGN.MD)**: Official CPU2 DMA multi-stage specification.

### Tier 3: Experimental Prototypes & Milestone Reports
These files record point-in-time progress, isolated unit tests, and milestone verification reports. 
> [!WARNING]
> **These documents are NOT global system architecture specifications.** They often represent isolated configurations, local selftests, or experimental utility states that are NOT integrated into the production runtime main path.
* **[M2_FIFO.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/.agent/02_Milestones/M2_FIFO.md)**: FIFO Selftest/Utility report. Status: *Experimental Utility / Not Runtime Main Path*.
* **[PH_2_FIRMWARE_VERIFY.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/.agent/03_Knowledge/HardWare_Test/PH_2_FIRMWARE_VERIFY.md)**: Firmware hardware loop tests.

### Tier 4: Research Notes & Static References
These files contain temporary exploratory studies, timing budgets, or vendor PDFs. They do not constitute active design decisions until formalized.
* **Research Notes**: e.g., `cpu1_research_plan.md`, `cpu2_research_plan.md`, `pwm_adc_dac_timing_analysis.md`, `analysis_dma_ch5.md`.
* **Knowledge Base**: Manuals and guides under `03_Knowledge/knowledge/` and device TRM PDFs.

---

## 2. Crucial Rules for AI Agents

1. **Do Not Synthesize Specs from Milestone Reports**: If a file resides in `02_Milestones/` (or is tagged as `REPORT_`), its design applies only to that isolated milestone validation. Do not assume its configuration represents the global production architecture.
2. **Prioritize Tier 1 and Tier 2**: When implementing features, reference ONLY the official Active Specifications (Tier 2) and Rules (Tier 1). If there is a conflict between a milestone report and an architecture specification, the Tier 2 specification wins.
