# Milestone: M2R_RXFRAME_PINGPONG (RxFrame Ping/Pong Buffer)

## 1. Goal
Establish a dual legacy-frame buffer (Ping/Pong) structure to receive high-speed waveform download data without CPU bottlenecks. This milestone implements the core buffer management and state transitions, validated by a self-test suite, before physical SPI integration.

## 2. Architecture & Data Flow
Models the data flow:
`AM3352 Host` -> `SPIB RX FIFO` -> `DMA CH3` -> `RxFramePing / RxFramePong` -> `Background Parser`

* **Contract Specification:** Details located in [RxFramePingPong_Contract.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/.agent/02_Milestones/M2R_RXFRAME_PINGPONG/RxFramePingPong_Contract.md).
* **Buffer Content:** Legacy `RxFrame_t` register frame format (`cmd` + `data`). No packet headers or footers.
* **DMA CH3 Connection:** Not connected yet (simulated by test suite only).
* **Execution Level:** The simulated DMA write runs in a high-priority context (representing an ISR/DMA trigger) while the parser processes full buffers in a low-priority context (representing the main loop).

## 3. Verification Plan
`RxFramePingPong_Test_Run()` must verify:
1. **Initial State Verification**: Ping and Pong buffers start as `EMPTY`.
2. **Buffer Switch & Fill**: Simulates DMA writing into Ping, triggering a switch to Pong, and marking Ping as `FULL`.
3. **Concurrent Parse & Fill**: Simulates background parser processing Ping (`FULL` -> `PARSING` -> `EMPTY`) while DMA fills Pong.
4. **Overrun (Collision) Detection**: Simulates DMA completing a transfer to Pong, but Ping is still `FULL`/`PARSING`, validating that the system correctly registers an overrun fault.
5. **Data Consistency**: Verifies that the data written by simulated DMA matches the data processed by the parser directly.

## 4. Status
* **Status:** **Completed & Verified (Build Success)**
