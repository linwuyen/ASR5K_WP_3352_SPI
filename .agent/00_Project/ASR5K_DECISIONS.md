# ASR5K Architecture & Frozen Decisions

This document logs all frozen architecture decisions and restrictions. These decisions are critical to maintaining system stability, hardware compatibility, and compatibility with the production ASR5K environment.

## 1. Architecture Decisions
* **D003 [EMIF1 Runtime Source]:** The runtime wave table and waveform generation logic read directly from EMIF1 SDRAM. CPU2 owns the DDS runtime using this source.
* **D004 [Output ON Protection]:** System output initialization and activation (Output ON) is prohibited while downloading new waveform tables.

## 2. DMA Ownership
* **D002 [DMA CH3]:** Channel 3 of the DMA controller is exclusively owned by SPIB RX. No other module or task may reconfigure or trigger DMA Channel 3.
* **D002b [DMA CH4]:** Channel 4 of the DMA controller is reserved and owned by SPIB TX.

## 3. Memory Ownership
* **D003 [EMIF1 SDRAM]:** EMIF1 SDRAM memory blocks are dedicated to storing Wave Tables. CPU1 writes to this region during download, and CPU2 reads from this region during real-time wave generation.

## 4. Protocol Ownership
* **D001 [Legacy SPIB Protocol]:** The SPIB Slave protocol must remain compatible with the production AM3352 SPI protocol contract (fixed-frame registers). Any modifications to packet header magic, framing, or register mapping require explicit system-wide approval.

## 5. Runtime Restrictions
* **D004 [Output ON Protection]:** The system enforces protection prohibiting `OUTPUT_ON` transitions or state changes during flash writes or waveform downloads to avoid high-power transient hazards.
