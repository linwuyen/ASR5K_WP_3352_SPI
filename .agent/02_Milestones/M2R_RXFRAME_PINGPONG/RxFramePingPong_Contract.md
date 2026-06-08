# RxFrame Ping/Pong Buffer Architecture & Design Contract

This document defines the formal architecture contract for the `M2R_RXFRAME_PINGPONG` milestone. It outlines the legacy frame data structure, buffer state machines, simulated DMA/Parser interface, and memory mapping rules.

## 1. Context & Data Flow Model

The contract models the production data path:

```text
AM3352 Host
  ↓ (SPI Physical Layer)
SPIB RX FIFO
  ↓ (DMA CH3 Autonomous Transfer)
RxFramePing / RxFramePong Buffers
  ↓ (Background Parser / Main Loop)
Decoded Waveform Destination (Application Domain)
```

To prevent CPU bottlenecks during high-speed waveform downloads, the DMA controller writes autonomously to one buffer (e.g., Ping) while the background parser processes data in the other buffer (e.g., Pong).

---

## 2. Legacy Register Frame Contract

The buffer content must strictly use the legacy register frame format rather than the packet protocol:

```c
typedef struct {
    uint16_t cmd;   /* Register address / command ID */
    uint16_t data;  /* Register data value */
} RxFrame_t;
```

---

## 3. Ping/Pong Buffer State Machine

### 3.1 Buffer States
Each buffer (Ping and Pong) operates under a strict state contract:
* **`BUFF_STATE_EMPTY`**: The buffer contains no unparsed data and is ready to be assigned to the DMA controller.
* **`BUFF_STATE_FILLING`**: The DMA controller is actively writing incoming frames into this buffer.
* **`BUFF_STATE_FULL`**: DMA has filled the buffer to capacity; it is waiting for background parsing.
* **`BUFF_STATE_PARSING`**: The background parser is actively reading and validating frames from this buffer.

### 3.2 State Transitions

```mermaid
stateDiagram-v2
    [*] --> BUFF_STATE_EMPTY : Initialization
    
    BUFF_STATE_EMPTY --> BUFF_STATE_FILLING : DMA Start / Switch
    note on link
        Triggered when the alternate buffer 
        becomes FULL and this buffer is EMPTY.
    end note
    
    BUFF_STATE_FILLING --> BUFF_STATE_FULL : DMA Done
    note on link
        Triggered when writeIndex reaches RX_BUFFER_SIZE.
    end note
    
    BUFF_STATE_FULL --> BUFF_STATE_PARSING : Parser Take
    note on link
        Triggered when background task detects 
        a FULL buffer and locks it.
    end note
    
    BUFF_STATE_PARSING --> BUFF_STATE_EMPTY : Parser Release
    note on link
        Triggered when parser finishes processing 
        all frames and clears the buffer.
    end note
```

---

## 4. API Contract & Interface

```c
#ifndef SPI_PINGPONG_H_
#define SPI_PINGPONG_H_

#include <stdint.h>
#include <stdbool.h>

#define RX_BUFFER_SIZE 64U  /* Size of each buffer in legacy frames */

/* Legacy frame structure */
typedef struct {
    uint16_t cmd;
    uint16_t data;
} RxFrame_t;

/* Buffer States */
typedef enum {
    BUFF_STATE_EMPTY = 0,
    BUFF_STATE_FILLING,
    BUFF_STATE_FULL,
    BUFF_STATE_PARSING
} RxBufferState_e;

/* Ping/Pong Management Structure */
typedef struct {
    RxFrame_t pingBuffer[RX_BUFFER_SIZE];
    RxFrame_t pongBuffer[RX_BUFFER_SIZE];
    
    volatile RxBufferState_e pingState;
    volatile RxBufferState_e pongState;
    
    uint16_t activeDmaBuffer;      /* 0 = Ping, 1 = Pong */
    uint16_t activeParserBuffer;   /* 0 = Ping, 1 = Pong */
    
    uint16_t dmaWriteIdx;          /* Index for DMA writes */
    uint16_t parserReadIdx;        /* Index for parser reads */
    
    /* Diagnostics and Counters */
    uint32_t pingFullCount;
    uint32_t pongFullCount;
    uint32_t overrunCount;
    uint32_t parseSuccessCount;
    uint32_t parseFailCount;
} RxFramePingPong_t;

/* Public API */
void RxFramePingPong_Init(void);
bool RxFramePingPong_DmaWrite(const RxFrame_t *frame);
void RxFramePingPong_Process(void);
bool RxFramePingPong_Test_Run(void);

#endif /* SPI_PINGPONG_H_ */
```

---

## 5. Switch & Overrun Fault Logic

1. **Buffer Switch Mechanism:**
   When `dmaWriteIdx` reaches `RX_BUFFER_SIZE`:
   - The active buffer's state is changed to `BUFF_STATE_FULL`.
   - The system checks the state of the other buffer. If it is `BUFF_STATE_EMPTY`, the **software model of DMA destination switching** changes `activeDmaBuffer` to the other buffer, resets `dmaWriteIdx` to `0`, and sets its state to `BUFF_STATE_FILLING`.
   
2. **Overrun (Collision) Condition:**
   - If the alternate buffer is **not** in `BUFF_STATE_EMPTY` when a buffer switch is triggered, a buffer overrun has occurred.
   - The system increments `overrunCount`, retains the current buffer as active, and halts/discards new incoming frames to prevent data corruption.

---

## 6. Memory Allocation Policy (F28388D Constraints)

To prevent linker errors (`#10099-D: program will not fit into available memory`) due to tight RAM limits on `RAMGS0`/`RAMLS5`:
* **Dedicated Linker Section:** The global Ping/Pong buffers and management structures must be placed in `spib_pingpong_state` section.
* **Linker Command Mapping:** Map `spib_pingpong_state` to `RAMGS6` in [spib_block_ram.cmd](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/Emu_3352_SPI/spib_block_ram.cmd):
  ```cmd
  spib_pingpong_state : > RAMGS6
  ```
