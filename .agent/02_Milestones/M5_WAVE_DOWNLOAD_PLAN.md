# M5_WAVE_DOWNLOAD_PLAN

## 1. Purpose
This is a preparation document outlining the wave download data path, architecture rules, and boundaries for Milestone 5. No source implementation is performed under this milestone phase.

---

## 2. Data Paths

### Current Verified Path
```text
AM3352
   ↓
 SPIB
   ↓
SPIB RX FIFO
   ↓
 DMA CH3
   ↓
Ping/Pong Buffer
   ↓
SPIB_ParseRegFrame()
   ↓
parseRemoteCommand()
   ↓
Command Handler
```

### Next Extension (M5)
```text
Command Handler
   ↓
Wave Download Service
   ↓
EMIF1 SDRAM
```

---

## 3. Architecture Rules

* **Wave Page Select Command**: `0x0958`
* **Wave Data Window**: `0x3000` ~ `0x3FFF`
* **Page Size (Samples)**: 4096 samples/page
* **Page Size (Bytes)**: 8192 bytes/page
* **EMIF1 Base Address**: `0x80000000`

### Page Address Formula
```text
PageBase = 0x80000000 + (PageIndex * 0x2000)
```

---

## 4. Non-Goals
* Packet Protocol
* CRC16
* Variable Length Packet
* CPU2 Integration
* DDS Runtime
* Flash Save
* OTA
* IPC
