# D04_WAVE_DATA_PIPELINE.md

## 1. Purpose

本文件定義 ASR5K 系統中 Wave Data 的完整生命週期 (Lifecycle)。

包含：

- Wave Download
- Wave Storage
- Wave Validation
- Wave Activation
- Wave Runtime Usage
- Wave Persistence

本文件描述：

```
Wave Data Flow
```

不描述：

```
DDS Algorithm
EMIF Driver
Flash Driver
```

上述內容由後續文件定義。

---

## 2. Design Goals

Wave Pipeline 必須滿足：

- No Data Loss
- DMA Based Transfer
- Deterministic Runtime
- No Runtime Flash Access
- Minimal CPU Overhead
- Output ON Safety Protection
- Future Expansion

---

## 3. Wave Lifecycle

```
AM3352 Download
↓
SDRAM Store
↓
Validation
↓
Activation
↓
DDS Runtime Use
↓
Optional Save To Flash
```

---

## 4. Download Architecture

### Wave Page Select

AM3352 透過：

```
CMD  = 0x0958
DATA = Page ID
```

指定目標 Wave Page。

---

### Wave Sample Download

AM3352 使用：

```
0x3000
~
0x3FFF
```

下載：

```
4096 Samples
```

每筆：

```
Address
+
Data
```

組成。

---

## 5. Download Data Flow

```
AM3352
↓
SPIB RX FIFO
↓
DMA CH3
↓
RxFrame Ping Buffer
RxFrame Pong Buffer
↓
Parser
↓
EMIF1 SDRAM
```

說明：

- DMA CH3 負責搬運 SPIB RX 資料
- Parser 負責解析 Register Frame
- Parser 決定 SDRAM 寫入位置
- Parser 不直接操作 Flash

---

## 6. Download Complete

Wave Download 完成條件：

```
4096 Samples Received
```

且：

```
AM3352 Send Download Complete Command
```

兩者同時成立。

---

若未收到：

```
Download Complete Command
```

則：

```
Wave Page = Incomplete
```

不得啟用。

---

## 7. Wave Validation

Validation 項目：

```
Page ID Valid
Address Continuous
Sample Count Correct
Checksum Valid
Download Complete Received
```

Validation 成功：

```
Wave Page Status = Valid
```

Validation 失敗：

```
Wave Page Status = Invalid
```

不得進入 Runtime。

---

## 8. Wave Storage

Wave Data 存放於：

```
EMIF1 SDRAM
```

用途：

```
Wave Database
```

EMIF1 SDRAM 為：

```
Primary Runtime Wave Source
```

---

## 9. Runtime Architecture

本文件定義：

```
Wave Runtime Source
=
EMIF1 SDRAM
```

---

Runtime Data Flow：

```
EMIF1 SDRAM
↓
DDS Runtime
↓
SPIC
↓
AD5543
```

---

注意：

本文件不規定：

```
CPU Direct Read

DMA Direct Read

DMA Preload

Hybrid Access
```

上述由：

```
D07_DDS_RUNTIME_MANAGER.md
```

定義。

---

## 10. Wave Activation

Activation 條件：

```
Wave Page Valid
Output OFF
```

---

Activation 動作：

```
Select Active Page
Update Runtime Context
Mark Runtime Ready
```

---

## 11. Runtime Protection

### Output OFF

允許：

```
Download Wave
Validate Wave
Activate Wave
Save Wave
Load Wave
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
Flash Write
Flash Erase
OTA Update
```

---

## 12. Wave Readback

Readback Flow：

```
SDRAM
↓
Readback Service
↓
TxPing/Pong Buffer
↓
DMA CH4
↓
SPIB TX FIFO
↓
AM3352
```

---

用途：

```
Debug
Verification
ATE
Maintenance
```

---

## 13. Flash Persistence

Flash 不參與 Runtime。

---

Load Flow：

```
W25Q64
↓
SPIA DMA CH5/CH6
↓
EMIF1 SDRAM
```

---

Save Flow：

```
EMIF1 SDRAM
↓
SPIA DMA CH5/CH6
↓
W25Q64
```

---

允許：

```
Boot
Maintenance
OTA
```

禁止：

```
Output ON Runtime
```

---

## 14. Runtime DMA Ownership

| DMA | Function |
| --- | --- |
| CH1 | SPIC TX |
| CH2 | SPIC RX |
| CH3 | SPIB RX |
| CH4 | SPIB TX |
| CH5 | SPIA RX |
| CH6 | SPIA TX |

---

Runtime：

```
CH1 ~ CH4 Reserved
```

---

Boot / OTA：

```
CH5 ~ CH6 Reserved
```

---

CH5 / CH6 不得於 Runtime 被重新配置。

---

## 15. Future Features

未來可擴充：

```
Wave Hot Switch

Background Wave Download

Dual Wave Bank

Wave Compression

Wave Encryption
```

---

## 16. Related Documents

```
D03_MEMORY_ARCHITECTURE.md

D05_EMIF1_MEMORY_MAP.md

D06_EMIF1_DRIVER.md

D07_DDS_RUNTIME_MANAGER.md

D10_WAVE_VALIDATION_POLICY.md

D11_AM3352_PROTOCOL.md
```

---

## 17. Revision History

| Version | Description |
| --- | --- |
| v1.0 | Initial Wave Pipeline |
| v1.1 | Align with D03 Memory Architecture and defer Runtime Access Method to D07 |