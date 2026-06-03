# ModbusMaster.py - Python Modbus RTU Master Implementation

## 概述 (Overview)

`ModbusMaster.py` 是一個用於 PC 串口與 Modbus Slave 設備通訊的 Python Modbus Master 實作。支援 Modbus RTU 協定，可用於讀寫 Modbus 設備的暫存器。

## 支援的功能 (Supported Functions)

- **0x03**: 讀取保持暫存器 (Read Holding Registers)
- **0x06**: 寫入單一暫存器 (Write Single Register)
- **0x10**: 寫入多個暫存器 (Write Multiple Registers)

## 系統需求 (Requirements)

- Python 3.6+
- pyserial 程式庫

## 安裝 (Installation)

```bash
pip install pyserial
```

## 使用方法 (Usage)

### 命令列介面 (Command Line Interface)

```bash
python ModbusMaster.py --port COM1 --slave 1 --function 3 --address 0 --quantity 10
```

### 類別使用 (Class Usage)

```python
from ModbusMaster import ModbusMaster

# 建立 Modbus Master 實例
master = ModbusMaster(port='COM4', baudrate=115200)

# 連線
if master.connect():
    # 讀取保持暫存器
    result = master.read_holding_registers(slave_id=3, start_address=0, quantity=10)

    # 寫入單一暫存器
    success = master.write_single_register(slave_id=3, address=5, value=1234)

    # 寫入多個暫存器
    success = master.write_multiple_registers(slave_id=3, start_address=10, values=[100, 200, 300])

    # 斷開連線
    master.disconnect()
```

## 命令列參數 (Command Line Arguments)

| 參數         | 說明                      | 範例           |
| ------------ | ------------------------- | -------------- |
| `--port`     | COM 埠號 (必要)           | `COM1`, `COM4` |
| `--baudrate` | 鮑率 (預設: 9600)         | `115200`       |
| `--slave`    | Slave ID (必要, 1-247)    | `3`            |
| `--function` | 功能碼 (必要: 3, 6, 16)   | `3`            |
| `--address`  | 起始地址 (必要)           | `0`, `10`      |
| `--quantity` | 讀取數量 (功能 3 必要)    | `10`           |
| `--value`    | 寫入值 (功能 6 必要)      | `1234`         |
| `--values`   | 多個寫入值 (功能 16 必要) | `100 200 300`  |

## 使用範例 (Examples)

### 讀取保持暫存器
```bash
python ModbusMaster.py --port COM4 --slave 3 --function 3 --address 0 --quantity 10
```

### 寫入單一暫存器
```bash
python ModbusMaster.py --port COM4 --slave 3 --function 6 --address 5 --value 1234
```

### 寫入多個暫存器
```bash
python ModbusMaster.py --port COM4 --slave 3 --function 16 --address 10 --values 100 200 300
```

## 技術實作細節 (Technical Implementation Details)

### Modbus RTU 訊框格式 (Modbus RTU Frame Format)

所有 Modbus RTU 訊框都包含：
- Slave ID (1 byte)
- Function Code (1 byte)
- Data (變動長度)
- CRC-16 (2 bytes)

### struct.pack 格式字串說明 (struct.pack Format String Explanation)

Python 的 `struct` 模組用於二進位資料打包：

| 格式字元 | 說明                  | 位元組數 |
| -------- | --------------------- | -------- |
| `>`      | 大端序 (Big-endian)   | -        |
| `H`      | 無符號短整數 (16-bit) | 2        |
| `B`      | 無符號位元組 (8-bit)  | 1        |

**常見格式：**
- `'>HH'` - 兩個 16-bit 大端序整數 (地址 + 數量)
- `'>HHB'` - 兩個 16-bit + 一個 8-bit (地址 + 數量 + 位元組計數)
- `'<H'` - 16-bit 小端序 (CRC)

### 重要修正 (Important Bug Fix)

在 `write_multiple_registers` 方法中，曾經有個錯誤的實作：

**錯誤版本 (Buggy Version):**
```python
expected_response = struct.pack('>HHH', slave_id, 0x10, start_address) + struct.pack('>H', quantity)
```

**正確版本 (Correct Version):**
```python
expected_response = bytes([slave_id, 0x10]) + struct.pack('>HH', start_address, quantity)
```

**問題原因：**
- Slave ID 和 Function Code 在 Modbus 協定中是 1 位元組值
- `struct.pack('>HHH', ...)` 會將它們打包為 2 位元組，造成格式錯誤

### CRC-16 計算 (CRC-16 Calculation)

使用 Modbus 標準 CRC-16 演算法：
- 多項式: 0xA001
- 初始值: 0xFFFF
- 小端序輸出

## 測試 (Testing)

執行單元測試：
```bash
python -m unittest test_modbus.py
```

測試檔案位於 `../tests/test_modbus.py`

## 序列埠設定 (Serial Port Configuration)

- 鮑率: 可設定 (預設 9600)
- 資料位元: 8
- 停止位元: 1
- 同位檢查: 偶同位 (Even Parity)
- 逾時: 1 秒

## 除錯資訊 (Debug Information)

程式會輸出詳細的除錯資訊：
- 送出的十六進位資料
- 接收的十六進位資料
- CRC 檢查結果
- 錯誤訊息

## 注意事項 (Notes)

- 確保 COM 埠未被其他程式佔用
- Slave ID 範圍: 1-247
- 地址範圍: 0-65535
- 讀取數量限制: 1-125 個暫存器
- 寫入數量限制: 1-123 個暫存器