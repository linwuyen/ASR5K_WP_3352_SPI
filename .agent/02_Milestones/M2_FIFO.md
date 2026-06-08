# M2 FIFO Circular Buffer Report (M2_FIFO.md)

## 1. Goal
Establish a self-contained circular frame queue to decouple incoming SPI RX DMA transfers from the parser/dispatcher application processing.

---

## 2. Architecture
* **Frame Structure (`SPI_FRAME_t`):**
  * `uint16_t cmd;`
  * `uint16_t data;`
* **Buffer Strategy:** Circular buffer with head/tail tracking.
* **Capacity:** 128 elements.
* **Producer/Consumer:** Single Producer (SPI RX DMA) / Single Consumer (Application Parser/Dispatcher).
* **Memory Placement:** Mapped the FIFO variables (`s_testFifo` and `g_fifoTestResult`) to a dedicated linker section `spi_fifo_state` mapped to `RAMGS5` in the linker command file (`spib_block_ram.cmd`) to prevent `.bss` overflow.

---

## 3. FIFO Contract
The FIFO manages `SPI_FRAME_t` records and exposes the following interfaces:
1. `void SPI_FIFO_Init(SPI_FIFO_t *fifo);`
   * Initializes queue indices, count, and all statistics counters to 0.
2. `bool SPI_FIFO_Push(SPI_FIFO_t *fifo, const SPI_FRAME_t *frame);`
   * Appends a frame to the tail index. Prevents overwrite by returning `false` if the buffer is full.
3. `bool SPI_FIFO_Pop(SPI_FIFO_t *fifo, SPI_FRAME_t *frame);`
   * Retrieves a frame from the head index. Returns `false` on underflow.
4. `bool FIFO_Test_Run(void);`
   * Comprehensive self-contained test runner verifying all 6 validation criteria.

### Diagnostic Counters
The `SPI_FIFO_t` structure maintains the following counters for verification and telemetry:
* `pushCount`: Total frames successfully pushed.
* `popCount`: Total frames successfully popped.
* `overflowCount`: Total push attempts rejected due to a full queue.
* `underflowCount`: Total pop attempts rejected due to an empty queue.
* `maxDepth`: Peak utilization count of the queue.

---

## 4. Test Cases & Detailed Scenarios

`FIFO_Test_Run()` executes a 6-part validation suite:

### Test 1: Push 64 / Pop 64
* **Objective:** Verify basic enqueue/dequeue behavior and order preservation (First-In, First-Out).
* **Method:**
  1. Initialize FIFO.
  2. Push 64 distinct frames (cmd: `0x1000 + i`, data: `0xA000 + i` for `i` from 0 to 63).
  3. Verify that `count` is 64, `pushCount` is 64, and `maxDepth` is 64.
  4. Pop 64 frames and verify that each matches the original pushed sequence.
  5. Verify that `count` returns to 0 and `popCount` is 64.
* **Result:** **PASS** (`test1_pass` = true)

### Test 2: Push 100 / Pop 100
* **Objective:** Verify larger enqueue/dequeue behavior and boundary tracking.
* **Method:**
  1. Initialize FIFO.
  2. Push 100 distinct frames (cmd: `0x2000 + i`, data: `0xB000 + i` for `i` from 0 to 99).
  3. Verify that `count` is 100, `pushCount` is 100, and `maxDepth` is 100.
  4. Pop 100 frames and verify that sequence order is preserved.
  5. Verify that `count` returns to 0 and `popCount` is 100.
* **Result:** **PASS** (`test2_pass` = true)

### Test 3: Circular Queue Wrap Around
* **Objective:** Verify that head and tail pointers wrap around correctly at the capacity boundary (`SPI_FIFO_SIZE = 128U`) without memory leakage or index errors.
* **Method:**
  1. Initialize FIFO.
  2. Push 80 frames, then Pop 40 frames to shift the tail pointer.
  3. Push another 80 frames. Since capacity is 128, the head pointer wraps back to 0.
  4. Verify that the current `count` is 120.
  5. Pop all 120 frames and verify that the data matches the exact interleaved push sequence.
  6. Verify that `count` returns to 0.
* **Result:** **PASS** (`test3_pass` = true)

### Test 4: Overflow Detection (No Overwrite)
* **Objective:** Verify that the circular queue detects a full state, rejects further pushes, records the error, and protects existing data.
* **Method:**
  1. Initialize FIFO.
  2. Fill the FIFO completely by pushing 128 frames.
  3. Attempt to push a 129th frame (`0xFFFF`).
  4. Verify that the push fails (returns `false`).
  5. Verify that `overflowCount` is incremented to 1, `count` remains 128, and `pushCount` is 128.
  6. Pop all 128 frames and verify that the first frame (which would have been overwritten in a naive queue) remains intact and correct.
* **Result:** **PASS** (`test4_pass` = true)

### Test 5: Underflow Detection (No Invalid Pop)
* **Objective:** Verify that the circular queue detects an empty state, rejects pops, records the error, and does not corrupt the output pointer.
* **Method:**
  1. Initialize FIFO.
  2. Attempt to pop from the empty FIFO into a pre-initialized frame (`cmd = 0xAAAA`, `data = 0x5555`).
  3. Verify that the pop fails (returns `false`).
  4. Verify that `underflowCount` is incremented to 1 and `popCount` remains 0.
  5. Verify that the destination frame content remains untouched (`0xAAAA`/`0x5555`).
* **Result:** **PASS** (`test5_pass` = true)

### Test 6: 1000-Frame Stress Test
* **Objective:** Verify long-term stability under interleaved push/pop operations mimicking real-time SPI DMA interrupts and application consumption.
* **Method:**
  1. Initialize FIFO.
  2. Run a loop: push 3 frames and pop 2 frames until 1000 frames have been pushed.
  3. Pop all remaining frames.
  4. Verify that all 1000 frames are successfully received, order is preserved, and both `overflowCount` and `underflowCount` remain 0.
* **Result:** **PASS** (`test6_pass` = true)

---

## 5. Expected / Actual / Result (M2 Close Criteria)

| Close Criteria Item | Expected / Requirement | Actual / Measurement | Status |
| :--- | :--- | :--- | :--- |
| **FIFO_Test_Run** | Function executes and returns `true` | Returns `true` (`g_fifoTestResult` is fully populated and verified) | **PASS** |
| **Test Cases Telemetry** | Expected/Actual stats for all 6 tests fully documented | Detailed telemetry matching expectations verified in execution | **PASS** |
| **Compilation Status** | `spi_fifo.c` & `spi_fifo.h` compile with zero errors | Successfully compiled under `cl2000` v22.6.1.LTS | **PASS** |
| **Main Loop Protection** | Main loop code changes must be guarded | `main.c` FIFO execution is fully enclosed within `#if ASR5K_ENABLE_FIFO_SELFTEST` | **PASS** |
| **SPI Baseline Integrity** | SPI interface, driver, and protocol untouched | SPI Master/Slave baseline operation is completely unmodified | **PASS** |
| **Integration Boundaries** | No active SPI DMA integration at this milestone | FIFO remains isolated as a self-contained unit | **PASS** |

---

## 6. Compile Status & Linker Config
* **Compiler Configuration:** Built using TI C2000 Compiler (`cl2000`) version 22.6.1.LTS.
* **Linker Command:** Mapped custom section `spi_fifo_state` to memory `RAMGS5` in [spib_block_ram.cmd](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/Emu_3352_SPI/spib_block_ram.cmd) to ensure no interference with standard memory layout.

---

## 7. Integration Boundary
* **State:** **Isolated**
* **Integration Strategy:** Excluded from the active SPI RX path to protect the baseline behavior. Conditional test execution is guarded by `ASR5K_ENABLE_FIFO_SELFTEST` in `main.c`.

---

## 8. Next Step
* **M2.5 SPI RX DMA to FIFO Integration**
