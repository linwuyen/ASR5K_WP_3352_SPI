# Modbus Slave Implementation

This directory contains the implementation of a Modbus Slave for the F28377D V2 BUCK CPU1 project. The Modbus Slave allows communication over a serial interface (SCI) using the Modbus RTU protocol, enabling remote monitoring and control of the device's parameters and status.

## Overview

The Modbus Slave implementation supports the following key features:
- Serial communication via SCI (Serial Communication Interface)
- Support for Modbus function codes: 0x03 (Read Holding Registers), 0x06 (Write Single Register), and 0x10 (Write Multiple Registers)
- CRC16 checksum validation for data integrity
- Timeout handling for communication reliability
- Error logging and reporting
- Integration with shared memory for data exchange between CPU cores

## File Structure

- **ModbusSlave.c**: Main implementation file containing the core Modbus Slave logic, including state machine, FIFO management, and execution functions.
- **ModbusFunction.c**: Implements specific Modbus function handlers for reading and writing registers.
- **ModbusCommon.h**: Common definitions, enumerations, and macros used across the Modbus implementation.
- **ModbusSlave.h**: Header file defining structures, enums, and function prototypes for the Modbus Slave.
- **linkVariables.c**: Handles the linking of Modbus registers to actual device variables, including initialization, reading, and writing operations.

## Key Components

### SCI_MODBUS Structure
The central structure managing the Modbus Slave state, including:
- FIFO buffers for transmit and receive data
- CRC calculation and validation
- Timeout counters
- Function pointers for register operations
- Error logging

### Supported Modbus Functions
- **0x03 - Read Holding Registers**: Reads multiple holding registers from the device.
- **0x06 - Write Single Register**: Writes a single value to a holding register.
- **0x10 - Write Multiple Registers**: Writes multiple values to consecutive holding registers.

### Register Mapping
Registers are mapped to device parameters through the `linkVariables.c` file, which provides functions to:
- Initialize register values (`initRegN`)
- Read current values into registers (`readRegN`)
- Write register values to device variables (`writeReg`, `writeRegN`)

## Configuration

### Serial Port Configuration
The Modbus Slave is configured to use `DEBUG_SCI_BASE` as the SCI base address. Key settings include:
- Slave ID: 0x03 (configurable)
- Baud rate and other serial parameters are handled by the underlying SCI driver

### Timeout Settings
- Timeout threshold: `MBUS_TIMEOUT` (defined as `T_10MS`)
- Timer base: `SWTIMER_BASE`

### Buffer Sizes
- Receive/Transmit FIFO size: `MBUS_BUFFER_SIZE` (128 words)
- Shared RAM size for CPU1: `MBUS_SHARERAM_SIZE`

## Usage

### Initialization
Call `initMbusConfig(&mbcomm)` to initialize the Modbus Slave with default settings.

### Main Loop Integration
In the main application loop, call `exeModbusSlave(&mbcomm)` to process incoming Modbus requests and handle responses.

### Interrupt Handling
Ensure SCI receive interrupts are enabled and configured to call relevant receive functions (e.g., `getMbusRxFIFO`).

### Register Access
- Use `readRegN` to update register values from device state
- Use `writeReg` or `writeRegN` for single or multiple register writes to update device parameters

## Dependencies

- `driverlib.h`: TI C2000Ware driver library
- `device.h`: Device-specific definitions
- `common.h`: Common project definitions
- `CRC16.h`: CRC16 calculation functions
- `shareram.h`: Shared memory definitions for inter-core communication

## Error Handling

The implementation includes comprehensive error handling:
- Invalid function codes return error responses
- Address range validation prevents out-of-bounds access
- CRC errors trigger message rejection
- Timeout detection resets communication state
- Error logging stores up to 16 recent errors

## Notes

- This implementation is designed for the TMS320F28377D microcontroller
- Assumes little-endian byte order for Modbus communication
- Uses Q15 fixed-point format for certain floating-point values (via `MB_SFtoQ15` and `MB_Q15toSF` macros)
- Shared memory is used for communication with CPU2 (CLA core)

For more details on specific functions or register mappings, refer to the source code comments and the Modbus specification.