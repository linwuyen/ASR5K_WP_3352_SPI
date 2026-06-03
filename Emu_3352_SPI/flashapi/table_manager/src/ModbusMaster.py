#!/usr/bin/env python3
"""
ModbusMaster.py - A Python Modbus Master implementation using pymodbus library

This script provides a Modbus Master that can communicate with Modbus slaves
via serial port (COM port) on a PC using the pymodbus library.

Usage:
    python ModbusMaster.py --port COM1 --slave 1 --function 3 --address 0 --quantity 10

Functions supported:
- 0x03: Read Holding Registers
- 0x06: Write Single Register
- 0x10: Write Multiple Registers
"""

import argparse
import sys
from pymodbus.client import ModbusSerialClient
from pymodbus.framer import FramerType

class ModbusMaster:
    def __init__(self, port, baudrate=115200, timeout=0.1):
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.client = ModbusSerialClient(
            framer=FramerType.RTU,
            port=self.port,
            baudrate=self.baudrate,
            timeout=self.timeout,
            parity='E',  # Even parity
            stopbits=1,
            retries=0  # Reduce retries to minimize extra IOCTL calls
        )

    def connect(self):
        """Open serial connection"""
        try:
            result = self.client.connect()
            if result:
                print(f"Connected to {self.port} at {self.baudrate} baud")
                return True
            else:
                print(f"Failed to connect to {self.port}")
                return False
        except Exception as e:
            print(f"Failed to connect to {self.port}: {e}")
            return False

    def disconnect(self):
        """Close serial connection"""
        if self.client:
            self.client.close()
            print("Disconnected")

    def read_holding_registers(self, slave_id, start_address, quantity):
        """Function 0x03: Read Holding Registers"""
        if quantity < 1 or quantity > 125:
            print("Quantity must be 1-125")
            return None

        try:
            response = self.client.read_holding_registers(
                address=start_address,
                count=quantity,
                device_id=slave_id
            )
            if response.isError():
                print(f"Modbus error: {response}")
                return None
            return response.registers
        except Exception as e:
            print(f"Read error: {e}")
            return None

    def write_single_register(self, slave_id, address, value):
        """Function 0x06: Write Single Register"""
        try:
            response = self.client.write_register(
                address=address,
                value=value,
                device_id=slave_id
            )
            if response.isError():
                print(f"Modbus error: {response}")
                return False
            return True
        except Exception as e:
            print(f"Write error: {e}")
            return False

    def write_multiple_registers(self, slave_id, start_address, values):
        """Function 0x10: Write Multiple Registers"""
        if len(values) < 1 or len(values) > 123:
            print("Number of registers must be 1-123")
            return False

        try:
            response = self.client.write_registers(
                address=start_address,
                values=values,
                device_id=slave_id
            )
            if response.isError():
                print(f"Modbus error: {response}")
                return False
            return True
        except Exception as e:
            print(f"Write error: {e}")
            return False

def main():
    parser = argparse.ArgumentParser(description='Modbus Master for PC COM port using pymodbus')
    parser.add_argument('--port', required=True, help='COM port (e.g., COM1)')
    parser.add_argument('--baudrate', type=int, default=9600, help='Baud rate (default: 9600)')
    parser.add_argument('--slave', type=int, required=True, help='Slave ID')
    parser.add_argument('--function', type=int, required=True,
                       choices=[3, 6, 16], help='Function code: 3=Read Holding, 6=Write Single, 16=Write Multiple')
    parser.add_argument('--address', type=int, required=True, help='Starting address')
    parser.add_argument('--quantity', type=int, help='Quantity for read operations')
    parser.add_argument('--value', type=int, help='Value for single write')
    parser.add_argument('--values', nargs='+', type=int, help='Values for multiple write (space separated)')

    args = parser.parse_args()

    # Validate arguments
    if args.function == 3 and not args.quantity:
        print("Quantity required for read operation")
        sys.exit(1)
    elif args.function == 6 and args.value is None:
        print("Value required for single write operation")
        sys.exit(1)
    elif args.function == 16 and not args.values:
        print("Values required for multiple write operation")
        sys.exit(1)

    master = ModbusMaster(args.port, args.baudrate)

    if not master.connect():
        sys.exit(1)

    try:
        if args.function == 3:  # Read Holding Registers
            result = master.read_holding_registers(args.slave, args.address, args.quantity)
            if result:
                print(f"Read {len(result)} registers: {result}")
            else:
                print("Read failed")

        elif args.function == 6:  # Write Single Register
            success = master.write_single_register(args.slave, args.address, args.value)
            if success:
                print(f"Successfully wrote {args.value} to address {args.address}")
            else:
                print("Write failed")

        elif args.function == 16:  # Write Multiple Registers
            success = master.write_multiple_registers(args.slave, args.address, args.values)
            if success:
                print(f"Successfully wrote {len(args.values)} registers starting at address {args.address}")
            else:
                print("Write failed")

    finally:
        master.disconnect()

if __name__ == "__main__":
    # PS> python ModbusMaster.py --port COM1 --slave 1 --function 3 --address 0 --quantity 10
    main()