# M5A Wave Download Hardware Validation Checklist

This checklist defines the tomorrow morning hardware validation procedure for the **M5A Wave Download Path** implementation on CPU1.

*   **Target Board**: TMDSCNCD28388D ControlCARD
*   **Compile Target**: CPU1_RAM
*   **Commit ID**: `5d9e6082fb9948913ae0762a8a860e77795f06ea`

---

## 1. Build Verification

| Check Item | Target / Criteria | Expected | Checked | Notes |
| :--- | :--- | :--- | :---: | :--- |
| **Clean Build** | `gmake clean && gmake all` | Returns 0, `Emu_3352_SPI.out` generated | [ ] | |
| **Compiler Warnings**| Review build stdout | No warnings in `wave_download` or `selftest` | [ ] | |
| **Linker placement** | Review `Emu_3352_SPI.map` | `.text` fits in `RAMGS1~3 | RAMLS0~4` | [ ] | Optimization level `-O2` enabled |
| **No GS RAM Overflow**| Review map file | `RAMGS7` ~ `RAMGS14` completely unused | [ ] | Verified free for CPU2/DDS |

---

## 2. SPI Communication (Physical/DMA Layer)

| Check Item | Description | CCS Watch / Oscilloscope Target | Checked | Notes |
| :--- | :--- | :--- | :---: | :--- |
| **SPIA Master Emulator**| Emulator running Test 9 | `spiA_master.stDriver.fsm` is active | [ ] | |
| **SPIB Slave FSM** | Slave parser running | `spiB_slave.fsm` is `_POP_RXD_FROM_SPI` | [ ] | |
| **DMA CH3 RX** | SPIB RX DMA working | `gSpibRxDmaDoneCount` increments | [ ] | |
| **DMA CH4 TX** | SPIB TX DMA working | `spiB_slave.u32FastPathCount` increments | [ ] | |

---

## 3. Wave Download Flow (D11 Register Access)

| Register Address | Command Name | Input Value | Expected Response | Checked | Notes |
| :--- | :--- | :--- | :--- | :---: | :--- |
| **0x0958** | WAVE_PAGE_SELECT | `0x0001U` | `0x0001U` | [ ] | Select Page 1 |
| **0x3000 ~ 0x3FFF** | WAVE_DATA_WINDOW | 4096 Samples | `u16Sample` (Ramp Data Echo) | [ ] | Sample 0=16, Sample 4095=65536 |
| **0x0959** | WAVE_DOWNLOAD_COMPLETE | `0x0001U` | `0x0001U` | [ ] | Complete download |
| **0x0960** | WAVE_VALIDATE | `0x0001U` | `0x0004U` (WAVE_PAGE_STATE_VALID) | [ ] | Validate checks |
| **0x0961** | WAVE_ACTIVATE | `0x0001U` | `0x0006U` (WAVE_PAGE_STATE_LOCKED) | [ ] | Active page updates to 1 |

---

## 4. State Machine Verification (D10 State Transitions)

Monitor `g_waveDownload.u16PageState[1]` during the download run. Verify it follows the expected sequential path:

- [ ] **EMPTY (0)**: Initial state before any select/download commands.
- [ ] **DOWNLOADING (1)**: Transitions immediately after `0x0958` writes Page 1.
- [ ] **DOWNLOAD_COMPLETE (2)**: Transitions after write to `0x0959` with data 1.
- [ ] **VALIDATING (3)** -> **VALID (4)**: Transitions during write to `0x0960` (WAVE_VALIDATE).
- [ ] **LOCKED (6)**: Transitions after write to `0x0961` (WAVE_ACTIVATE).

*Verify that no illegal transition is allowed (e.g. state must not jump from DOWNLOADING to VALID).*

---

## 5. Storage Verification (Fake Storage Backend)

Verify that the fake storage RAM backend functions correctly on Page 1:

- [ ] **RAM Block Placement**: Verify `g_u16FakeSdram1` is allocated in `RAMGS5` (address `0x012000`).
- [ ] **Sample Count Check**: `g_waveDownload.u16SampleCount[1] == 4096U`.
- [ ] **Continuity Check**: `g_waveDownload.bAddressContinuous[1] == true`.
- [ ] **Last Address Check**: `g_waveDownload.u16LastAddress[1] == 0x3FFF`.
- [ ] **Data Readback Verification**: Verify that reading Page 1 storage retrieves the expected ramp values:
    *   `WaveDownload_ReadSample(1U, 0U) == 16U`
    *   `WaveDownload_ReadSample(1U, 4095U) == 65536U`

---

## 6. Negative Tests (Robustness Verification)

| Test Scenario | Action / Injection | Expected Behavior | Checked | Notes |
| :--- | :--- | :--- | :---: | :--- |
| **Activate before Validate**| Write `0x0961` (Activate) when state is `DOWNLOAD_COMPLETE` | Activation rejected; active page remains unchanged; returns `0xFFFFU`. | [ ] | |
| **Validate before Complete**| Write `0x0960` (Validate) when state is `DOWNLOADING` | Validation fails; page state transitions to `INVALID (5)`. | [ ] | |
| **Invalid Page Index** | Write `0x0958` with index `0x0100` (Page 256) | Page selection rejected; returns `0xFFFFU`. | [ ] | Out-of-bounds page index |
| **Output ON Rejection** | Write `0x0958` while `OUTPUT_ON == 1` | Action rejected by parser/dispatcher. | [ ] | Interlock protection check |

---

## 7. Evidence Collection

For each validation step, record the physical execution log:

```text
================================================================================
Test Run ID: ASR5K_M5A_HW_001
Timestamp: 2026-06-09T__:__:__
Tester: Roger Lin
================================================================================

[BUILD VERIFICATION]
* Commit ID: 5d9e6082fb9948913ae0762a8a860e77795f06ea
* Linker Placement Result: [PASS/FAIL]
* Observed Code Size (.text): ________ words

[SPI & DMA PHYSICAL LAYER]
* DMA RX Done Count: ________
* Fast Path Handled Count: ________
* Result: [PASS/FAIL]

[WAVE DOWNLOAD SEQUENCE]
* WAVE_PAGE_SELECT response: ________
* WAVE_DOWNLOAD_COMPLETE response: ________
* WAVE_VALIDATE response: ________
* WAVE_ACTIVATE response: ________
* Active Page (g_waveDownload.u16ActivePage): ________
* Result: [PASS/FAIL]

[STORAGE BACKEND INTEGRITY]
* Page 1 Sample Count: ________
* Page 1 Continuity Flag: [TRUE/FALSE]
* Page 1 Sample[0] value: ________ (Expected: 16)
* Page 1 Sample[4095] value: ________ (Expected: 65536)
* Result: [PASS/FAIL]

[NEGATIVE TEST RUNS]
* Early Activation response: ________
* Early Validation response: ________
* Out-of-bounds Page Select response: ________
* Result: [PASS/FAIL]

================================================================================
Final Status: [PASS / PASS WITH ISSUES / FAIL]
Comments:
================================================================================
```
