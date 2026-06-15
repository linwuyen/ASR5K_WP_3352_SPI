# M3 Handoff Prompt

This document serves as the official handoff prompt to continue development from **M3** in a new session. It defines project context, architectural guidelines, current milestone status, and the immediate next steps.

---

## Project Identity

* **Project:** `WP_3352_SPI`
* **Role:** ASR5K Runtime Prototype
* **Architecture Authority:** Read the following documents before performing any actions:
  1. [ARCHITECTURE_AUTHORITY.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/.agent/00_Project/ARCHITECTURE_AUTHORITY.md)
  2. [AGENT_INDEX.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/.agent/00_Project/AGENT_INDEX.md)
  3. [ASR5K_STATUS.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/.agent/00_Project/ASR5K_STATUS.md)
  4. [ASR5K_HANDOFF.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/.agent/00_Project/ASR5K_HANDOFF.md)
  5. [ASR5K_DECISIONS.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/.agent/00_Project/ASR5K_DECISIONS.md)
  6. [ASR5K_Context.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/.agent/rules/ASR5K_Context.md)
  7. [rules.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/.agent/rules/rules.md)
  8. [Review_Checklist.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/.agent/rules/Review_Checklist.md)

---

## Current Milestone Status

### M1_SPI_BASELINE
* **Status:** PASS / CLOSED

### M2_FIFO
* **Status:** PASS / CLOSED
* **Note:** Experimental Utility Only (not on runtime main path).

### M2R_RXFRAME_PINGPONG
* **Status:** PASS / CLOSED
* **Note:** Official Runtime Path.
* **Runtime Verification Evidence:** Verified via CCS Watch Window with:
  ```text
  g_pingpongTestResult.test1_pass = 1
  g_pingpongTestResult.test2_pass = 1
  g_pingpongTestResult.test3_pass = 1
  g_pingpongTestResult.test4_pass = 1
  g_pingpongTestResult.test5_pass = 1
  g_pingpongTestResult.failed_step = 0
  ```

---

## Official Runtime Data Path

```text
AM3352 Host
  ↓
SPIB RX FIFO
  ↓
DMA CH3
  ↓
RxFramePing / RxFramePong Buffers
  ↓
Background Parser
```

---

## Next Milestone: M3_CPU1_DMA_RXFRAME_INTEGRATION

* **Goal:** Connect existing SPIB DMA CH3 implementation to the validated `RxFramePing`/`Pong` manager.
* **Target Path:**
  ```text
  SPIB RX FIFO
    ↓
  DMA CH3
    ↓
  RxFramePing / RxFramePong
    ↓
  CPU1 Background Parser
  ```

---

## Out Of Scope
Do not modify:
* CPU2
* IPC
* MSGRAM
* EMIF1
* Flash
* DDS Runtime
* SPI Protocol
* Wave Download Protocol

---

## Required First Deliverable
Before writing code, create:
`M3_CPU1_DMA_RXFRAME_INTEGRATION.md` (Architecture Contract)

The contract must define:
1. DMA CH3 Ownership
2. RxFramePing/Pong Ownership
3. DMA Destination Switching Rule
4. `FULL` State Rule
5. `PARSING` State Rule
6. Overrun Policy
7. Parser Scheduling Policy
8. Verification Method
9. CCS Watch Evidence Required

**No implementation is allowed before architecture review and approval.**

---

## Important Rule
* **Compile PASS does not mean Runtime PASS.**
* Every milestone must satisfy:
  ```text
  Contract Review
    ↓
  Implementation
    ↓
  Compile Verification
    ↓
  Runtime Verification
    ↓
  Milestone Close
  ```
* Runtime PASS must be supported by physical CCS Watch Window evidence.
