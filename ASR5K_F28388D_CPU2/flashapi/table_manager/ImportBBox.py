#!/usr/bin/env python3
"""
ImportBBox.py - Import blackbox data to DSP via Modbus

This script imports calibration parameters and system variables from a JSON file
to the DSP blackbox storage using Modbus communication.

Usage:
    python ImportBBox.py --input bbox_export.json
    python flashapi/table_manager/ImportBBox.py --input bbox_export.json

Dependencies:
    - ModbusMaster.py (in the same directory)
"""

import json
import time
import argparse
import sys
import os
import zlib
from datetime import datetime

# Get the script directory
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

# Add the src and configs directories to the Python path
sys.path.insert(0, os.path.join(SCRIPT_DIR, 'src'))
sys.path.insert(0, os.path.join(SCRIPT_DIR, 'configs'))

from ModbusMaster import ModbusMaster
from configs.bbox_config import *

class BlackboxImporter:
    """
    Finite State Machine for importing blackbox data to DSP via Modbus
    """
    
    def __init__(self, master, slave_id, input_file):
        self.master = master
        self.slave_id = slave_id
        self.input_file = input_file
        self.state = 'LOAD_DATA'
        self.import_data = {}
        self.var_names = []
        self.var_values = []
        self.i = 0  # Current variable index
        self.start_time = 0  # For timeout
        
    def run(self):
        """
        Run the FSM until completion
        
        Returns:
            bool: True if successful
        """
        print("Starting blackbox data import (FSM)...")
        
        while self.state != 'DONE':
            if self.state == 'LOAD_DATA':
                self._load_data()
            elif self.state == 'RESET_DSP':
                self._reset_dsp()
            elif self.state == 'SEND_IMPORT_CMD':
                self._send_import_cmd()
            elif self.state == 'IMPORT_LOOP':
                self._import_loop()
            elif self.state == 'WRITE_DATA':
                self._write_data()
            elif self.state == 'WAIT_CONFIRM':
                self._wait_confirm()
            elif self.state == 'SEND_SAVE_CMD':
                self._send_save_cmd()
        
        return True
    
    def _load_data(self):
        """Load and verify data from JSON file"""
        try:
            with open(self.input_file, 'r', encoding='utf-8') as f:
                data_dict = json.load(f)
            print(f"Loaded data from {self.input_file}")
        except FileNotFoundError:
            raise Exception(f"File {self.input_file} not found")
        except json.JSONDecodeError as e:
            raise Exception(f"Invalid JSON file: {e}")
        
        # Verify CRC32
        if not self._verify_crc32(data_dict):
            raise Exception("CRC32 verification failed")
        
        # Remove CRC32 from data for import
        self.import_data = {k: v for k, v in data_dict.items() if k != 'CRC32'}
        self.var_names = list(self.import_data.keys())
        self.var_values = list(self.import_data.values())
        self.state = 'RESET_DSP'
    
    def _verify_crc32(self, data_dict):
        """Verify CRC32 checksum"""
        if 'CRC32' not in data_dict:
            print("Error: CRC32 field not found in JSON file")
            return False
        
        expected_crc = data_dict['CRC32']
        data_without_crc = {k: v for k, v in data_dict.items() if k != 'CRC32'}
        values = list(data_without_crc.values())
        byte_data = b''.join(v.to_bytes(4, 'little') for v in values if isinstance(v, int))
        calculated_crc = zlib.crc32(byte_data)
        
        if calculated_crc == expected_crc:
            print(f"CRC32 verification successful: {calculated_crc:08X}")
            return True
        else:
            print(f"CRC32 verification failed: expected {expected_crc:08X}, calculated {calculated_crc:08X}")
            return False
    
    def _reset_dsp(self):
        """Reset DSP state"""
        print("Resetting DSP state...")
        success = self.master.write_multiple_registers(self.slave_id, RETURN_ADDR, [0, 0])
        if not success:
            print("Warning: Failed to reset DSP state, continuing anyway...")
        time.sleep(DSP_PROCESS_DELAY)
        self.state = 'SEND_IMPORT_CMD'
    
    def _send_import_cmd(self):
        """Send import command to DSP"""
        success = self.master.write_single_register(self.slave_id, CMD_ADDR, IMPORT_CMD)
        if not success:
            raise Exception("Failed to send import command")
        time.sleep(DSP_PROCESS_DELAY)
        self.i = 0  # Start from index 0
        self.state = 'IMPORT_LOOP'
    
    def _import_loop(self):
        """Import variables in a loop"""
        if self.i >= len(self.var_names):
            self.state = 'SEND_SAVE_CMD'
            return
        
        var_name = self.var_names[self.i]
        value = self.var_values[self.i]
        print(f"Index: {self.i + 1}, Name: {var_name}, Value: {value}")
        
        # Split 32-bit value into two 16-bit registers (little-endian)
        low_word = value & 0xFFFF
        high_word = (value >> 16) & 0xFFFF
        self.data_regs = [low_word, high_word]
        
        self.state = 'WRITE_DATA'
    
    def _write_data(self):
        """Write data to DSP"""
        success = self.master.write_multiple_registers(self.slave_id, DATA_ADDR, self.data_regs)
        if not success:
            raise Exception(f"Failed to write data for variable {self.i + 1} ({self.var_names[self.i]})")
        
        # Write index to u32Return
        success = self.master.write_multiple_registers(self.slave_id, RETURN_ADDR, [self.i + 1, 0])
        if not success:
            raise Exception(f"Failed to write index {self.i + 1} for variable {self.var_names[self.i]}")
        
        time.sleep(DSP_PROCESS_DELAY)
        self.start_time = time.time()
        self.state = 'WAIT_CONFIRM'
    
    def _wait_confirm(self):
        """Wait for DSP confirmation"""
        timeout = 10
        if time.time() - self.start_time > timeout:
            raise Exception(f"Timeout waiting for DSP confirmation for variable {self.i + 1} ({self.var_names[self.i]})")
        
        return_regs = self.master.read_holding_registers(self.slave_id, RETURN_ADDR, 2)
        if return_regs and len(return_regs) == 2:
            return_value = (return_regs[1] << 16) | return_regs[0]
            if return_value == 0:
                self.i += 1
                self.state = 'IMPORT_LOOP'
                return
        
        time.sleep(DSP_PROCESS_DELAY)
        # Stay in WAIT_CONFIRM
    
    def _send_save_cmd(self):
        """Send save command"""
        success = self.master.write_single_register(self.slave_id, ACK_ADDR, SAVE_CMD)
        if not success:
            raise Exception("Failed to send save command")
        
        print(f"Successfully imported {len(self.var_values)} variables")
        self.state = 'DONE'

def import_bbox_data(master, slave_id, input_file):
    """
    Import blackbox data to DSP using FSM

    Args:
        master: ModbusMaster instance
        slave_id: Modbus slave ID
        input_file: Input JSON file path

    Returns:
        bool: True if successful, False otherwise
    """
    importer = BlackboxImporter(master, slave_id, input_file)
    return importer.run()

def main():
    parser = argparse.ArgumentParser(description='Import blackbox data to DSP via Modbus')
    parser.add_argument('--input', required=True, help='Input JSON file path')
    parser.add_argument('--port', default='COM4', help='COM port (default: COM4)')
    parser.add_argument('--baudrate', type=int, default=115200, help='Baud rate (default: 115200)')
    parser.add_argument('--slave', type=int, default=0x03, help='Modbus slave ID (default: 0x03)')
    parser.add_argument('--timeout', type=float, default=0.1, help='Modbus timeout (default: 0.1)')

    args = parser.parse_args()

    # Check if input file exists
    if not os.path.exists(args.input):
        print(f"Error: Input file {args.input} does not exist")
        sys.exit(1)

    # Create Modbus master
    master = ModbusMaster(args.port, args.baudrate, args.timeout)

    try:
        # Connect
        if not master.connect():
            print("Failed to connect to Modbus device")
            sys.exit(1)

        # Import data
        success = import_bbox_data(master, args.slave, args.input)
        if success:
            print("Import completed successfully")
        else:
            print("Import failed")
            sys.exit(1)

    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)

    finally:
        master.disconnect()

if __name__ == "__main__":
    main()

# python ImportBBox.py --input BBOX_20251107_090703.json --slave 1