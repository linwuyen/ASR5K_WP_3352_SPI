# M1_SPI_BASELINE

## Status
M1_SPI_BASELINE
PASS / CLOSED

---

## 1. Purpose
Verify basic SPI clock, frame, RX path, and TX path baseline communication between SPIA Master and SPIB Slave.

---

## 2. Verified Components

### SPI Clock
* **Configured frequency**: 12.5 MHz
* **SPI Clock Mode**: `SPI_PROT_POL0PHA0` (Mode 0)

### SPI Frame
* **Word Width**: 16-bit data characters.
* **Format**: 2-word Legacy Register Frame (`CMD/Address` + `DATA`).

### RX Path
* SPIB Slave RX FIFO reads 2-word frames from SPIA Master.
* Polling-based read operation verified.

### TX Path
* SPIB Slave TX FIFO writes echo responses back to SPIA Master.
* Verification of address checksum calculations.

---

## 3. Verification Result
PASS
