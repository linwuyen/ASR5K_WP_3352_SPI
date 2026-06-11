# D05_EMIF1_MEMORY_MAP.md

## 1. Purpose

本文件定義 ASR5K CPU1 專用 EMIF1 SDRAM 記憶體配置。

內容包含：

- Wave Data Storage
- Wave Checksum Storage
- Voltage Raw Data Storage
- Current Raw Data Storage
- Calibration Storage
- Parameter Storage
- OTA Working Area
- Debug Area
- Future Expansion Area

本文件不定義：

- EMIF Driver
- DDS Runtime Access Method
- Flash Driver

上述內容由後續文件定義。

---

# 2. Hardware Information

## SDRAM Device

```
Part Number:
IS42S16160J
```

組織：

```
16M x16
```

容量：

```
32 MBytes
```

介面：

```
EMIF1
```

Owner：

```
CPU1
```

---

# 3. EMIF1 Address Space

EMIF1 SDRAM 存在兩種映射：

---

## Large Data Space

```
Base:
0x80000000

End:
0x8FFFFFFF
```

用途：

```
Wave Database

Raw Data Storage

Calibration Storage

Parameter Storage

OTA Storage

Debug Storage
```

主要用途：

```
Bulk Data Storage
DMA Source
DMA Destination
```

---

## Program + Data Window

```
Base:
0x00200000

End:
0x002FFFFF
```

用途：

```
CPU Accessible Window

Future Runtime Window

Future Cache Window
```

本版暫不使用。

保留給：

```
D07 DDS Runtime Manager
```

決定。

---

# 4. Design Principles

遵守：

```
Wave Database Stored In SDRAM

Output ON Protect Wave Data

Flash Not Used In Runtime

DMA Preferred For Large Transfer

Future Expansion Friendly
```

---

# 5. Global Memory Layout

使用：

```
0x80000000
```

Large Data Space。

---

## Wave Data Area

Offset：

```
0x00000000
```

Size：

```
2 MB
```

用途：

```
Wave Database
```

---

## Wave Checksum Area

Offset：

```
0x00200000
```

Size：

```
2 MB
```

用途：

```
Wave Checksum
```

---

## Voltage Raw Data Area

Offset：

```
0x00400000
```

Size：

```
2 MB
```

用途：

```
Voltage Capture

Voltage Logging

Debug
```

---

## Current Raw Data Area

Offset：

```
0x00600000
```

Size：

```
2 MB
```

用途：

```
Current Capture

Current Logging

Debug
```

---

## Calibration Area

Offset：

```
0x00800000
```

Size：

```
1 MB
```

用途：

```
Voltage Calibration

Current Calibration

DAC Calibration

Factory Data
```

---

## Parameter Area

Offset：

```
0x00900000
```

Size：

```
1 MB
```

用途：

```
System Parameter

Startup Configuration

Protection Setting

User Setting
```

---

## OTA / Maintenance Area

Offset：

```
0x00A00000
```

Size：

```
4 MB
```

用途：

```
OTA Working Buffer

Wave Backup

Maintenance Data

Flash Readback Verify
```

---

## Debug Area

Offset：

```
0x00E00000
```

Size：

```
2 MB
```

用途：

```
Runtime Trace

Fault Snapshot

DMA Debug

SPIB Debug
```

---

## Reserved Area

Offset：

```
0x01000000
```

Size：

```
16 MB
```

用途：

```
Future Expansion
```

---

# 6. Wave Page Definition

每頁：

```
4096 Samples
```

資料格式：

```
16-bit
```

容量：

```
4096 × 2

=
8192 Bytes

=
8 KB
```

---

# 7. Wave Data Address Formula

```c
#define WAVE_PAGE_SIZE_BYTES    8192U

wave_addr =
EMIF1_BASE
+
WAVE_DATA_OFFSET
+
(page_id * WAVE_PAGE_SIZE_BYTES)
```

---

Sample：

```c
sample_addr =
wave_addr
+
(sample_index * 2)
```

---

# 8. Checksum Page Definition

採用：

```
One Checksum Word Per Sample
```

---

每頁：

```
4096 Checksum Words
```

容量：

```
8 KB
```

---

Checksum Address：

```c
checksum_addr =
EMIF1_BASE
+
WAVE_CHECKSUM_OFFSET
+
(page_id * WAVE_PAGE_SIZE_BYTES)
```

---

# 9. Supported Wave Capacity

Wave Area：

```
2 MB
```

每頁：

```
8 KB
```

支援：

```
256 Pages
```

Page ID：

```
0x0000
~
0x00FF
```

---

目前產品使用：

```
0x0000
~
0x0012
```

共：

```
19 Pages
```

---

# 10. Runtime Access Policy

Output OFF：

允許：

```
Wave Download

Wave Validation

Wave Activation

Calibration Update

Parameter Update

Flash Save
```

---

Output ON：

允許：

```
DDS Runtime Read

Status Read

Wave Readback
```

禁止：

```
Wave Download

Wave Modification

Calibration Update

Parameter Update

Flash Erase

Flash Write

OTA Update
```

---

# 11. DMA Policy

Runtime：

```
CH1
CH2
CH3
CH4
```

Reserved。

---

Boot / OTA：

```
CH5
CH6
```

Reserved。

---

不得於 Runtime 重新配置：

```
CH5
CH6
```

---

# 12. Future Documents

相關文件：

```
D06_EMIF1_DRIVER.md

D07_DDS_RUNTIME_MANAGER.md

D08_FLASH_PARTITION_LAYOUT.md

D10_WAVE_VALIDATION_POLICY.md

D11_AM3352_PROTOCOL.md

D12_CALIBRATION_DATA_MAP.md

D13_PARAMETER_MAP.md
```

---

# 13. Revision History

| Version | Description |
| --- | --- |
| v1.0 | Initial EMIF1 Memory Map |
| v1.1 | Add Large Data Space / Program Window Architecture |