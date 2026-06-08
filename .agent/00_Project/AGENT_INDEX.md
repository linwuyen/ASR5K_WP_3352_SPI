# Agent Document Index

This document indexes all documentation files inside the `.agent` directory, classifying them by authority level and purpose.

> [!WARNING]
> Milestone reports and research notes must not override ARCHITECTURE_AUTHORITY.md or official SPEC documents.

---

## 1. Must Read First
These documents define the global rules, state variables, constraints, authority hierarchy, and current engineering status.

* **[ARCHITECTURE_AUTHORITY.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/.agent/00_Project/ARCHITECTURE_AUTHORITY.md)**: Establishes the authoritative status of all documents in the project.
* **[ASR5K_HANDOFF.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/.agent/00_Project/ASR5K_HANDOFF.md)**: Handoff summary for developers and AI agents.
* **[ASR5K_STATUS.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/.agent/00_Project/ASR5K_STATUS.md)**: Current milestone checklist and progress tracking log.
* **[ASR5K_DECISIONS.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/.agent/00_Project/ASR5K_DECISIONS.md)**: Architectural and peripheral frozen decisions.
* **[ASR5K_Context.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/.agent/rules/ASR5K_Context.md)**: MCU responsibilities and data flow context.
* **[rules.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/.agent/rules/rules.md)**: Core rules and priority hierarchy for AI agents.
* **[Review_Checklist.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/.agent/rules/Review_Checklist.md)**: Review checklist for firmware changes.

---

## 2. Official Runtime Prototype Specs
Active design contracts, interfaces, and architecture specifications for runtime modules.

* **[M2R_RXFRAME_PINGPONG.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/.agent/02_Milestones/M2R_RXFRAME_PINGPONG.md)**: Specification and verification plan for the official runtime Ping/Pong path.
* **[RxFramePingPong_Contract.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/.agent/02_Milestones/M2R_RXFRAME_PINGPONG/RxFramePingPong_Contract.md)**: Interface definition, states, and transition rules.
* **[M3_CPU1_DMA_RXFRAME_INTEGRATION.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/.agent/02_Milestones/M3_CPU1_DMA_RXFRAME_INTEGRATION.md)**: Architecture contract and verification report for connecting DMA CH3 to Ping/Pong manager (PASS / CLOSED).
* **[SPEC_FIRMWARE_ARCH.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/.agent/01_Architecture/SPEC_FIRMWARE_ARCH.md)**: Core firmware architecture design spec.

---

## 3. Milestone Reports
Historical checkpoint logs, completed test summaries, and experimental utility documentation.

* **[M2_FIFO.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/.agent/02_Milestones/M2_FIFO.md)**: Design log and status of the experimental FIFO utility.
* **Completed Test Reports / Checkpoints**: Historical logs and execution results.

---

## 4. Research / Reference Only
Exploratory design notes, datasheet guidelines, timing analysis, and hardware reference material.

* **[cpu1_research_plan.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/.agent/01_Architecture/cpu1_research_plan.md)**: CPU1 architecture research plan.
* **[cpu2_research_plan.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/.agent/01_Architecture/cpu2_research_plan.md)**: CPU2 architecture research plan.
* **[pwm_adc_dac_timing_analysis.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/.agent/03_Knowledge/pwm_adc_dac_timing_analysis.md)**: Timing analysis reference.
* **[hrpwm_hw_config_porting_guide.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/.agent/03_Knowledge/hrpwm_hw_config_porting_guide.md)**: HRPWM configuration guide.
* **TRM PDFs and Hardware Manuals** (e.g. under `.agent/03_Knowledge/Peripheral/`)
* **IPC/SPI Reference Documents** (e.g. under `.agent/03_Knowledge/SPI_Docs/`)
