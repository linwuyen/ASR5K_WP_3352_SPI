# ASR5K Project Handoff

This document helps engineers and AI processes quickly understand the current state of the project, including risks, next steps, and crucial reading materials.

## 1. Where We Are (Current Milestone Status)
* **Milestone 2 (M2) Circular Buffer:** **Completed & Validated**.
* **Status:** `FIFO_Test_Run()` executes a 6-part test suite (Push/Pop 64, Push/Pop 100, Wrap Around, Overflow, Underflow, 1000-Frame Stress) and passes.
* **Integrations:** Conditional compilation enabled via `#if ASR5K_ENABLE_FIFO_SELFTEST` inside [main.c](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/Emu_3352_SPI/main.c).
* **Isolation:** The FIFO is completely self-contained. No active modifications have been made to SPI DMA channel 3, command parser, or dispatcher to keep the baseline working.

## 2. Key Risks & Observations
* **Memory Constraints:** RAMGS0/RAMLS5 are almost fully occupied. The FIFO variables (`s_testFifo` and `g_fifoTestResult`) had to be pragmed (`spi_fifo_state`) into `RAMGS5` in the linker command file ([spib_block_ram.cmd](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/Emu_3352_SPI/spib_block_ram.cmd)) to prevent linker error `#10099-D` (out of memory).
* **Thread Safety / Concurrency:** The circular buffer is single-producer, single-consumer. It does not use mutexes or disables interrupts. When integrated into the actual interrupt/DMA path in Milestone 3, we must ensure race conditions between the DMA ISR (producer) and the main loop parser (consumer) are prevented.

## 3. Next Steps
* **Milestone 3 (M3) Consumer Pipeline:**
  1. Integrate the `SPI_FIFO_t` queue into the SPIB RX path (`pollReceiveFromSpi()`).
  2. Read incoming frames from the queue and parse them asynchronously.
  3. Verify throughput under test conditions.

## 4. Required Reading & Key Files
* **Core Context Rules:** [ASR5K_Context.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/ASR5K_Context.md) (Platform architecture and roles).
* **Frozen Decisions:** [ASR5K_DECISIONS.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/.agent/00_Project/ASR5K_DECISIONS.md) (All constraints on DMA, protocol compatibility, etc.).
* **Current Progress Details:** [ASR5K_STATUS.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/.agent/00_Project/ASR5K_STATUS.md) (Milestone tracking).
* **Validation Evidence:** [M2_FIFO_Status.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/.agent/00_Project/M2_FIFO_Status.md) (The 6-scenario verification result).
