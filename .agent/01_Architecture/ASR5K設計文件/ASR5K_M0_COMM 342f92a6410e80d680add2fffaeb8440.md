# ASR5K_M0_COMM

# ASR5K_M0 通訊接口與內存映射規格

## I2C 標準通訊封包實施 (Standard I2C Packet)

本章節定義 ASR5K_M0 在 I2C 主機模式下的標準數據傳輸波形。

### I2C 寫入操作時序

下圖展示了一個標準的 7-bit 地址寫入操作：[START] -> [ADDR:7] -> [W:1] -> [ACK] -> [DATA:8] -> [ACK] -> [STOP]。

```
{ "signal": [
  { "name": "SCL", "wave": "1.n.n.n.n.n.n.n.n.n.n.n.n.n.n.n.n.n.1", "period": 1 },
  { "name": "SDA", "wave": "10.=.=.=.=.=.=.=.=.1.=.=.=.=.=.=.=.10", "data": ["A6", "A5", "A4", "A3", "A2", "A1", "A0", "W", "D7", "D6", "D5", "D4", "D3", "D2", "D1", "D0"] },
  { "name": "State", "wave": "x.2...............2.3...............x", "data": ["Address Phase", "Data Phase"] }
],
  "edge": ["p0-s1 START", "s16-s17 ACK", "s34-s35 ACK", "s35-s36 STOP"],
  "config": { "hscale": 1 }
}
```

## 系統記憶體映射 (System Memory Map)

ASR5K_M0 採用 32-bit 位址空間規劃，具體定義如下：

| 區段名稱 (Section) | 起始位址 (Start Addr) | 結束位址 (End Addr) | 容量 (Size) | 說明 (Description) |
| --- | --- | --- | --- | --- |
| **Code Flash** | `0x0000_0000` | `0x0007_FFFF` | 512 KB | 存放啟動編碼與應用程序 |
| **Internal SRAM** | `0x2000_0000` | `0x2000_7FFF` | 32 KB | 系統運行數據緩衝區 |
| **I2C Peripheral** | `0x4000_3000` | `0x4000_33FF` | 1 KB | I2C 控制與數據暫存器 |
| **System Control** | `0xE000_E000` | `0xE000_EFFF` | 4 KB | NVIC 與系統控制區 |

## 暫存器說明 (Register Descriptions)

### I2C 控制暫存器 (I2C_CON)

**位址**: `0x4000_3000`

```
{ "reg": [
  { "name": "I2C_EN", "bits": 1, "attr": "Module Enable", "type": 2 },
  { "name": "SLV_MD", "bits": 1, "attr": "Master/Slave Select", "type": 3 },
  { "name": "SPEED", "bits": 2, "attr": "0:SS, 1:FS, 2:HS", "type": 4 },
  { "name": "RSV", "bits": 4, "attr": "Reserved", "type": 0 },
  { "name": "TX_FIFO_TH", "bits": 8, "attr": "TX FIFO Threshold", "type": 5 },
  { "name": "RX_FIFO_TH", "bits": 8, "attr": "RX FIFO Threshold", "type": 6 },
  { "name": "IRQ_MSK", "bits": 8, "attr": "Interrupt Mask", "type": 7 }
], "config": { "bits": 32, "lanes": 1 }
}
```

### I2C 目標位址暫存器 (I2C_TAR)

**位址**: `0x4000_3004`

```
{ "reg": [
  { "name": "ADDR", "bits": 10, "attr": "Target Address", "type": 2 },
  { "name": "GC_EN", "bits": 1, "attr": "General Call Enable", "type": 3 },
  { "name": "10BIT", "bits": 1, "attr": "10-bit addressing", "type": 4 },
  { "name": "RSV", "bits": 20, "attr": "Reserved", "type": 0 }
], "config": { "bits": 32, "lanes": 1 }
}
```

### I2C 風扇控制暫存器 (I2C_FAN_CTRL)

**位址**: `0x4000_3008`

```
{ "reg": [
  { "name": "SPD_CMD", "bits": 16, "attr": "風扇控制占空比 (Speed Command in Duty)", "type": 2 },
  { "name": "SPD_ECAP", "bits": 16, "attr": "風扇讀取轉速 (Speed Ecap)", "type": 3 }
], "config": { "bits": 32, "lanes": 1 }
}
```

## 注意事項

- **時鐘頻率**: 標準模式 (Standard Speed) 支援最高 100 kHz，快速模式 (Fast Mode) 最高 400 kHz。
- **上拉電阻**: 根據物理總線電容，建議 SCL/SDA 使用 4.7kΩ 以上的上拉電阻。