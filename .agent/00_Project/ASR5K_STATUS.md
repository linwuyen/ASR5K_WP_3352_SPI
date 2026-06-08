# ASR5K Project Status

## 1. Current Status
* **M1: SPI Baseline:** PASS
* **M2_FIFO (FIFO Circular Buffer):** Experimental Utility PASS
* **M2R_RXFRAME_PINGPONG (RX Frame Ping/Pong Buffer):** Official Runtime Path / PASS / CLOSED
* **M3: CPU2 Consumer Pipeline:** In Progress / Pending

## 2. Current Milestone Details: M2R_RXFRAME_PINGPONG
* **Progress:**
  - Designed the RxFrame Ping/Pong buffer state machine and switch/overrun logic.
  - Defined the architecture and memory constraints contract in [RxFramePingPong_Contract.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/.agent/02_Milestones/M2R_RXFRAME_PINGPONG/RxFramePingPong_Contract.md).
  - Registered the milestone specification in [M2R_RXFRAME_PINGPONG.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/.agent/02_Milestones/M2R_RXFRAME_PINGPONG.md).
  - Mapped variables to the `spib_pingpong_state` section on `RAMGS6` in the linker command file to ensure safety against `.bss` limits.
  - Resolved `CPU1_FLASH` build linker size overflow by expanding `.TI.ramfunc` execution mapping in `main.syscfg` to both `RAMLS0` and `RAMLS1`.
  - Implemented `spi_pingpong.h` / `spi_pingpong.c` with the self-test `RxFramePingPong_Test_Run()`.
  - Refactored implementation to remove simulated EMIF SDRAM dependencies, verifying data integrity in-place directly on the Ping/Pong buffers before release.
* **Status:** **PASS / CLOSED**

## 3. Next Step
* Transition to M3 (CPU2 Consumer Pipeline) and establish IPC transfer paths.
