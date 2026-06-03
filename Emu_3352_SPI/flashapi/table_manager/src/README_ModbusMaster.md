# ModbusMaster.py

A Python-based Modbus Master implementation for communicating with Modbus slaves via PC COM port.

## Requirements

- Python 3.6+
- pyserial library

## Installation

1. Install Python dependencies:
```bash
pip install -r requirements.txt
```

## Usage

The script supports the following Modbus functions:

- **Function 0x03**: Read Holding Registers
- **Function 0x06**: Write Single Register
- **Function 0x10**: Write Multiple Registers

### Examples

#### Read Holding Registers
Read 10 registers starting from address 0 from slave ID 1 on COM1:
```bash
python ModbusMaster.py --port COM1 --slave 1 --function 3 --address 0 --quantity 10
```

#### Write Single Register
Write value 1234 to address 5 on slave ID 1:
```bash
python ModbusMaster.py --port COM1 --slave 1 --function 6 --address 5 --value 1234
```

#### Write Multiple Registers
Write values 100, 200, 300 to addresses starting from 10:
```bash
python ModbusMaster.py --port COM1 --slave 1 --function 16 --address 10 --values 100 200 300
```

## Command Line Arguments

- `--port`: COM port (required, e.g., COM1, COM2)
- `--baudrate`: Baud rate (default: 9600)
- `--slave`: Slave ID (required, 1-247)
- `--function`: Function code (required: 3, 6, or 16)
- `--address`: Starting address (required)
- `--quantity`: Number of registers to read (required for function 3)
- `--value`: Value to write (required for function 6)
- `--values`: Space-separated values to write (required for function 16)

## Notes

- Default serial settings: 8 data bits, no parity, 1 stop bit
- Timeout is set to 1 second
- CRC-16 checksum is automatically calculated and verified
- The script will display sent/received data in hex format for debugging