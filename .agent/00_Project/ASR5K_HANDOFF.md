# ASR5K Project Handoff

> [!IMPORTANT]
> **All development and AI agents MUST read [ARCHITECTURE_AUTHORITY.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/.agent/00_Project/ARCHITECTURE_AUTHORITY.md) first before analyzing code or files.**

This document helps engineers and AI processes quickly understand the current state of the project, including risks, next steps, and crucial reading materials.

## 1. Where We Are (Current Milestone Status)
* **Milestone M2R: RX Frame Ping/Pong Buffer:** **CLOSED / PASS**. Implemented `spi_pingpong.h`/`spi_pingpong.c` with the self-test `RxFramePingPong_Test_Run()`.
* **M2 FIFO Buffer Status:** Completed and verified, but designated strictly as an **experimental/utility module**. Per D005, it will NOT be integrated into the SPIB RX production download path.
* **Ping/Pong Isolation:** The Ping/Pong buffer module is fully implemented, verified, and closed. Real DMA CH3 configuration and SPI baseline functionality remain completely untouched.

## 2. Key Risks & Observations
* **Memory Constraints:** Like the M2 FIFO, the Ping/Pong buffers require substantial memory (2 buffers of 64 legacy frames = 256 words). To prevent linker out-of-memory errors on `.bss`, these buffers are assigned to the `spib_pingpong_state` section on `RAMGS6` in the linker command file ([spib_block_ram.cmd](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/Emu_3352_SPI/spib_block_ram.cmd)).
* **Concurrency/Overruns:** In the future SPI integration, simulated DMA write (in ISR context) and background parsing (in main loop) will execute concurrently. Overrun detection must block further DMA writes and signal a fault when the alternate buffer is not empty.

## 3. Next Steps
* Flash and run target-based validation for `M2R_RXFRAME_PINGPONG` on CCS, capturing Watch expressions.
* Transition to M3 (CPU2 Consumer Pipeline) and establish IPC transfer paths.

## 4. Required Reading & Key Files
* **Core Context Rules:** [ASR5K_Context.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/.agent/rules/ASR5K_Context.md) (Architecture overview).
* **Frozen Decisions:** [ASR5K_DECISIONS.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/.agent/00_Project/ASR5K_DECISIONS.md) (Check D005, D006, and D007).
* **Ping/Pong Design Contract:** [RxFramePingPong_Contract.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/.agent/02_Milestones/M2R_RXFRAME_PINGPONG/RxFramePingPong_Contract.md) (The interface, states, and verification rules).
* **Milestone Tracking:** [ASR5K_STATUS.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/.agent/00_Project/ASR5K_STATUS.md) and [M2R_RXFRAME_PINGPONG.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/.agent/02_Milestones/M2R_RXFRAME_PINGPONG.md).
