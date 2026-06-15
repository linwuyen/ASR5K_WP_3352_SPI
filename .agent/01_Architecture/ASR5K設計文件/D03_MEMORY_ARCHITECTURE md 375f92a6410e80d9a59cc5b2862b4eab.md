# D03_MEMORY_ARCHITECTURE.md

## 1. Purpose

本文件定義 ASR5K 系統之 Memory Architecture。

內容包含：

- Memory Hierarchy
- Memory Ownership
- Runtime Ownership
- DMA Ownership
- Wave Storage Architecture
- Non-Volatile Storage Architecture

本文件不定義：

- DDS Runtime Algorithm
- EMIF Driver Implementation
- Flash Driver Implementation

上述內容由後續文件定義。

---

## 2. Design Goals

ASR5K Memory Architecture 必須滿足：

- 100kHz Runtime Control
- Large Wave Storage
- DMA-Based Data Movement
- Deterministic Runtime Behavior
- Minimal CPU Overhead
- Future OTA Expansion
- Future Calibration Expansion

---

## 3. Memory Hierarchy

```
W25Q64
↓
EMIF1 SDRAM
↓
GSRAM / LSRAM
```

### W25Q64

用途：

- Non-Volatile Storage
- Wave Backup
- Calibration Backup
- Parameter Backup
- Future OTA Image

---

### EMIF1 SDRAM

用途：

- Wave Database
- Wave Download Target
- Checksum Storage
- Raw Data Storage
- Calibration Storage
- Runtime Data Storage

EMIF1 SDRAM 為：

```
Primary Bulk Data Storage
```

---

### GSRAM / LSRAM

用途：

- SPIB RX Buffer
- SPIB TX Buffer
- DMA Staging Buffer
- Runtime Scratch Buffer

不作為：

```
Permanent Wave Database
```

---

## 4. Memory Ownership

### W25Q64

Owner：

```
Boot Loader
Maintenance Service
SPIA Driver
```

Runtime：

```
No Access
```

---

### EMIF1 SDRAM

Owner：

```
CPU1
DDS Runtime
Wave Download Service
Wave Validation Service
```

用途：

```
Wave Database
```

EMIF1 SDRAM 不限定 Runtime Access Method。

實際 Runtime 是否採用：

```
CPU Direct Access
DMA Access
Hybrid Access
```

由後續 DDS Runtime Architecture 決定。

---

### GSRAM / LSRAM

Owner：

```
SPIB RX
SPIB TX
DMA
Runtime Scratch
```

---

## 5. Wave Storage Architecture

### Wave Page

每頁：

```
4096 Samples
16-bit Data
```

容量：

```
8192 Bytes
```

---

### Current Page Range

目前保留：

```
Page 0x0000
~
Page 0x0012
```

共：

```
19 Pages
```

---

### Future Expansion

實際可支援頁數由：

```
D05_EMIF1_MEMORY_MAP.md
```

定義。

---

## 6. Runtime Data Path

### Boot Path

```
W25Q64
↓
SPIA DMA
↓
EMIF1 SDRAM
```

---

### Download Path

```
AM3352
↓
SPIB RX FIFO
↓
DMA CH3
↓
RxFrame Ping/Pong
↓
Parser
↓
EMIF1 SDRAM
```

---

### Runtime Path

```
EMIF1 SDRAM
↓
DDS Runtime
↓
SPIC
↓
AD5543
```

注意：

本文件僅定義：

```
EMIF1 SDRAM
=
Wave Runtime Source
```

不定義：

```
CPU Direct Read

DMA Direct Read

DMA Preload
```

上述內容由：

```
D07_DDS_RUNTIME_MANAGER.md
```

定義。

---

## 7. Communication Buffer Architecture

### RX

```
SPIB RX FIFO
↓
DMA CH3
↓
RxFrame Ping/Pong
```

用途：

```
Wave Download
Register Write
Command Reception
```

---

### TX

```
Status
↓
TxPing/Pong
↓
DMA CH4
↓
SPIB TX FIFO
```

用途：

```
Status Response
Fault Response
Wave Readback
```

---

## 8. Memory Protection Policy

### Output OFF

允許：

```
Wave Download
Wave Validation
Wave Activation
Flash Save
Flash Load
```

---

### Output ON

允許：

```
Status Read
Fault Read
Wave Readback
```

禁止：

```
Wave Download
Wave Modification
Wave Activation
Flash Erase
Flash Write
OTA Update
```

---

## 9. DMA Ownership

| DMA | Owner | Purpose |
| --- | --- | --- |
| CH1 | SPIC TX | DDS Output |
| CH2 | SPIC RX | ADC Capture |
| CH3 | SPIB RX | AM3352 Receive |
| CH4 | SPIB TX | AM3352 Response |
| CH5 | SPIA RX | Flash Read |
| CH6 | SPIA TX | Flash Write |

### DMA Runtime Rule

Runtime：

```
CH1~CH4 Reserved
```

Boot / OTA：

```
CH5~CH6 Reserved
```

不得於 Runtime 重新配置 CH5 / CH6。

---

## 10. Future Documents

後續文件：

```
D05_EMIF1_MEMORY_MAP.md

D06_EMIF1_DRIVER.md

D07_DDS_RUNTIME_MANAGER.md

D08_FLASH_PARTITION_LAYOUT.md

D10_WAVE_VALIDATION_POLICY.md

D11_AM3352_PROTOCOL.md
```

---

## 11. Revision History

| Version | Description |
| --- | --- |
| v1.0 | Initial Memory Architecture |
| v1.1 | Clarify SDRAM as Wave Database and defer Runtime Access Method to D07 |