# ASR5K M1-M4 Verified Communication Path

## End-to-End Verified Path
```text
AM3352 (Host)
     ↓
   SPIB (Hardware Interface)
     ↓
SPIB RX FIFO
     ↓
  DMA CH3 (Autonomous Transfer)
     ↓
Ping/Pong Buffer (Double Buffering)
     ↓
SPIB_ParseRegFrame() (Register Parser)
     ↓
parseRemoteCommand() (Protocol Decoder)
     ↓
Command Handler (Execution Unit)
```

---

## Future Integration Note
> [!IMPORTANT]
> **M5_WAVE_DOWNLOAD_PATH** shall continue from **Command Handler** into **Wave Download Service** / **SDRAM Wave Store**.
