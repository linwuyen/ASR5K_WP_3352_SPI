# D10_WAVE_VALIDATION_POLICY.md

可以，D10 先定義「什麼叫一個 Wave Page 有效」。

我建議 D10 v1.0 先採：

```
Download Complete Command
+
4096 samples received
+
address coverage check
+
per-sample checksum word
+
Output OFF condition
```

下面是可直接存進 Project 的版本。

```markdown
# D10_WAVE_VALIDATION_POLICY.md

## 1. Purpose

本文件定義 ASR5K Wave Data 的驗證政策。

目的：

- 判定 AM3352 下載的 Wave Page 是否完整
- 判定 Wave Page 是否允許被 DDS Runtime 使用
- 定義 Download Complete Command 之後的 validation 流程
- 定義 checksum / address / sample count / Output state 的檢查規則
- 定義 validation fail 時的系統行為

本文件延伸自：

- D03_MEMORY_ARCHITECTURE.md
- D04_WAVE_DATA_PIPELINE.md
- D05_EMIF1_MEMORY_MAP.md
- D07_DDS_RUNTIME_MANAGER.md

---

## 2. Design Goals

Wave Validation 必須滿足：

1. 不允許不完整波形進入 DDS Runtime
2. 不允許 Output ON 狀態下修改 Wave Page
3. 支援 AM3352 Register-Based Protocol
4. 支援 Download Complete Command
5. 支援 per-sample checksum
6. 支援 AM3352 / ATE readback verification
7. Validation 不得干擾 100kHz DDS Runtime

---

## 3. Validation Scope

Validation 對象：

```text
Single Wave Page
```

每頁包含：

```
4096 samples
16-bit data
4096 checksum words
```

Wave Page 儲存於：

```
EMIF1 SDRAM Wave Data Area
```

Checksum 儲存於：

```
EMIF1 SDRAM Wave Checksum Area
```

---

## 4. Wave Page State Machine

每個 Wave Page 應維護狀態：

```
EMPTY
DOWNLOADING
DOWNLOAD_COMPLETE
VALIDATING
VALID
INVALID
LOCKED
```

---

### 4.1 EMPTY

尚未下載。

---

### 4.2 DOWNLOADING

AM3352 已選擇 page 並開始寫入：

```
0x0958 = page_id
0x3000 ~ 0x3FFF = sample data
```

---

### 4.3 DOWNLOAD_COMPLETE

AM3352 已送出 Download Complete Command。

此狀態只代表：

```
AM3352 宣告下載結束
```

不代表 page 已有效。

---

### 4.4 VALIDATING

C2000 正在檢查：

```
Page ID
Sample Count
Address Coverage
Checksum
Output State
```

---

### 4.5 VALID

Wave Page 通過驗證，可被 DDS Runtime 使用。

---

### 4.6 INVALID

Wave Page 驗證失敗，不可被 DDS Runtime 使用。

---

### 4.7 LOCKED

Output ON 時，目前 DDS 使用中的 page 必須視為 locked。

LOCKED page 禁止：

```
Write
Erase
Overwrite
Validation overwrite
Activation replace
```

---

## 5. Download Preconditions

AM3352 開始下載前，C2000 必須確認：

```
Output OFF
Page ID valid
Target page not locked
System not in OTA
System not in Flash erase/write
```

若不符合：

```
Reject write
Return BUSY or ERROR
Keep original page unchanged
```

---

## 6. Download Complete Rule

Wave Page 不以收到 `0x3FFF` 作為唯一完成條件。

完整條件：

```
1. AM3352 writes 4096 samples
2. AM3352 sends Download Complete Command
3. C2000 receives Download Complete Command while Output OFF
```

若未收到 Download Complete Command：

```
Page remains DOWNLOADING or INVALID
Do not activate
Do not mark valid
```

---

## 7. Address Coverage Rule

C2000 應追蹤 Wave Window：

```
0x3000 ~ 0x3FFF
```

對應：

```
sample_index = address - 0x3000
```

Validation 時必須確認：

```
All 4096 sample indexes received
No missing address
No out-of-range address
Page ID remains unchanged during download
```

建議實作方式：

```
Received bitmap
or
Sequential counter
or
Range coverage table
```

若 AM3352 保證連續寫，可先採：

```
Sequential address check
```

未來可升級為：

```
4096-bit coverage bitmap
```

---

## 8. Sample Count Rule

每個 Wave Page 必須收到：

```
4096 samples
```

Sample count 不可：

```
less than 4096
greater than 4096
```

超過範圍視為 protocol error。

---

## 9. Checksum Rule

本版採用：

```
one checksum word per sample
```

Checksum 計算：

```c
checksum = ((sample >> 8) & 0x00FF) + (sample & 0x00FF);
```

每個 sample 對應一個 checksum word。

---

## 10. Checksum Storage

Sample 儲存於：

```
Wave Data Area
```

Checksum 儲存於：

```
Wave Checksum Area
```

對應關係：

```
WaveData[page_id][sample_index]
WaveChecksum[page_id][sample_index]
```

---

## 11. Validation Procedure

收到 Download Complete Command 後，C2000 執行：

```
Step 1: Check Output OFF
Step 2: Check page_id valid
Step 3: Check sample count == 4096
Step 4: Check address coverage complete
Step 5: Recompute checksum for each sample
Step 6: Compare checksum area
Step 7: Mark page VALID or INVALID
Step 8: Report result to AM3352
```

---

## 12. Output ON Protection

Output ON 時禁止：

```
Wave Download
Wave Modification
Wave Validation overwrite
Wave Activation
Flash erase/write
OTA update
```

若 AM3352 在 Output ON 時寫入 wave window：

```
Reject write
Set error status
Keep current wave page unchanged
```

---

## 13. Validation Fail Behavior

Validation fail 時：

```
Mark page INVALID
Do not activate page
Keep previous valid page active
Return error status to AM3352
```

錯誤原因需記錄：

```
Invalid Page ID
Output ON
Missing Sample
Address Discontinuity
Checksum Error
Sample Count Error
Download Complete Missing
Page Locked
```

---

## 14. Validation Pass Behavior

Validation pass 時：

```
Mark page VALID
Allow activation while Output OFF
Allow AM3352 readback
Allow maintenance save to W25Q64
```

但不代表立即輸出。

Wave Activation 仍需獨立命令或狀態切換。

---

## 15. AM3352 Readback Verification

Validation pass 後，AM3352 / ATE 可讀回：

```
Wave Data
Checksum Data
Page Status
Error Code
Sample Count
```

Readback 路徑：

```
EMIF1 SDRAM
↓
Readback Service
↓
TxPing/Pong
↓
DMA CH4
↓
SPIB TX FIFO
↓
AM3352
```

---

## 16. Recommended Page Metadata

每個 page 建議維護 metadata：

```c
typedef struct
{
    uint16_t page_id;
    uint16_t state;
    uint16_t sample_count;
    uint16_t error_code;
    uint16_t last_address;
    uint16_t checksum_error_count;
    uint32_t received_mask_status;
} WavePageMeta;
```

第一版可簡化為：

```
page_id
state
sample_count
last_address
error_code
```

---

## 17. Error Codes

建議錯誤碼：

```
0x0000 OK
0x0001 INVALID_PAGE_ID
0x0002 OUTPUT_ON_REJECT
0x0003 ADDRESS_OUT_OF_RANGE
0x0004 ADDRESS_DISCONTINUITY
0x0005 SAMPLE_COUNT_ERROR
0x0006 CHECKSUM_ERROR
0x0007 DOWNLOAD_COMPLETE_MISSING
0x0008 PAGE_LOCKED
0x0009 VALIDATION_TIMEOUT
```

---

## 18. Runtime Rule

Validation 不得進入 100kHz ISR。

Validation 應於：

```
Background Task
Maintenance Task
Output OFF State
```

執行。

---

## 19. Future Enhancements

未來可擴充：

```
CRC16 per page
CRC32 per page
Bitmap-based address coverage
Dual-bank wave validation
Partial page update
Versioned wave package
ATE validation report
```

---

## 20. Related Documents

```
D03_MEMORY_ARCHITECTURE.md
D04_WAVE_DATA_PIPELINE.md
D05_EMIF1_MEMORY_MAP.md
D07_DDS_RUNTIME_MANAGER.md
D11_AM3352_PROTOCOL.md
D08_FLASH_PARTITION_LAYOUT.md
```

---

## 21. Revision History

| Version | Description |
| --- | --- |
| v1.0 | Initial Wave Validation Policy |

```

下一份我建議做 **D11_AM3352_PROTOCOL.md**，因為 D10 已經開始引用 Download Complete Command，但 D11 要正式定義它的 address / response / error code。
```