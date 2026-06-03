#!/usr/bin/env python3
"""
ExportBBox.py - Export blackbox data from DSP via Modbus

This script exports calibration parameters and system variables from the DSP
blackbox storage using Modbus communication.

Usage:
    python ExportBBox.py --output bbox_export.json
    python flashapi/table_manager/ExportBBox.py --output bbox_export.json

Dependencies:
    - ModbusMaster.py (in the same directory)
    - minimalmodbus (pip install minimalmodbus)
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

class BlackboxExporter:
    """
    Finite State Machine for exporting blackbox data from DSP via Modbus
    """
    
    def __init__(self, master, slave_id, output_file=None):
        self.master = master
        self.slave_id = slave_id
        self.output_file = output_file
        self.state = 'LOAD_CONFIG'
        self.var_names = []
        self.NUM_VARS = 0
        self.exported_data = {}
        self.i = 0  # Current variable index
        self.start_time = 0  # For timeout
        self.json_path = os.path.join(SCRIPT_DIR, '..', 'PRB6K_BUCK_BBOX.json')
        
    def run(self):
        """
        Run the FSM until completion
        
        Returns:
            dict: Exported data
        """
        print("Starting blackbox data export (FSM)...")
        
        while self.state != 'DONE':
            if self.state == 'LOAD_CONFIG':
                self._load_config()
            elif self.state == 'RESET_DSP':
                self._reset_dsp()
            elif self.state == 'SEND_EXPORT_CMD':
                self._send_export_cmd()
            elif self.state == 'EXPORT_LOOP':
                self._export_loop()
            elif self.state == 'WAIT_RESPONSE':
                self._wait_response()
            elif self.state == 'READ_DATA':
                self._read_data()
            elif self.state == 'SEND_EXIT_CMD':
                self._send_exit_cmd()
            elif self.state == 'CALCULATE_CHECKSUM':
                self._calculate_checksum()
            elif self.state == 'SAVE_FILE':
                self._save_file()
        
        return self.exported_data
    
    def _load_config(self):
        """Load variable definitions from JSON file"""
        try:
            with open(self.json_path, 'r', encoding='utf-8') as f:
                var_sizes = json.load(f)
            self.var_names = list(var_sizes.keys())
            self.NUM_VARS = len(self.var_names)
            print(f"Loaded {len(self.var_names)} variable definitions")
            self.state = 'RESET_DSP'
        except FileNotFoundError:
            raise Exception(f"Could not load variable definitions from {self.json_path}")
    
    def _reset_dsp(self):
        """Reset DSP state"""
        print("Resetting DSP state...")
        success = self.master.write_multiple_registers(self.slave_id, RETURN_ADDR, [0, 0])
        if not success:
            print("Warning: Failed to reset DSP state, continuing anyway...")
        time.sleep(DSP_PROCESS_DELAY)
        self.state = 'SEND_EXPORT_CMD'
    
    def _send_export_cmd(self):
        """Send export command to DSP"""
        success = self.master.write_single_register(self.slave_id, CMD_ADDR, EXPORT_CMD)
        if not success:
            raise Exception("Failed to send export command")
        time.sleep(DSP_PROCESS_DELAY)
        self.i = 1  # Start from index 1
        self.state = 'EXPORT_LOOP'
    
    def _export_loop(self):
        """Export variables in a loop"""
        if self.i > self.NUM_VARS:
            self.state = 'SEND_EXIT_CMD'
            return
        
        print(f"Index: {self.i}")
        
        # Write index to u32Return
        success = self.master.write_multiple_registers(self.slave_id, RETURN_ADDR, [self.i, 0])
        if not success:
            raise Exception(f"Failed to write index {self.i} to RETURN_ADDR")
        
        time.sleep(DSP_PROCESS_DELAY)
        self.start_time = time.time()
        self.state = 'WAIT_RESPONSE'
    
    def _wait_response(self):
        """Wait for DSP response"""
        timeout = 10
        if time.time() - self.start_time > timeout:
            raise Exception(f"Timeout waiting for DSP response for variable {self.i}")
        
        return_regs = self.master.read_holding_registers(self.slave_id, RETURN_ADDR, 2)
        if return_regs and len(return_regs) == 2:
            return_value = (return_regs[1] << 16) | return_regs[0]
            if return_value == 0:
                self.state = 'READ_DATA'
                return
        
        time.sleep(DSP_PROCESS_DELAY)
        # Stay in WAIT_RESPONSE
    
    def _read_data(self):
        """Read data from DSP"""
        data_regs = self.master.read_holding_registers(self.slave_id, DATA_ADDR, 2)
        if not data_regs or len(data_regs) != 2:
            raise Exception(f"Failed to read 32-bit data for variable {self.i}")
        
        value = (data_regs[1] << 16) | data_regs[0]
        var_name = self.var_names[self.i-1] if self.i-1 < len(self.var_names) else f"var_{self.i}"
        self.exported_data[var_name] = value
        
        self.i += 1
        self.state = 'EXPORT_LOOP'
    
    def _send_exit_cmd(self):
        """Send exit command"""
        self.master.write_single_register(self.slave_id, ACK_ADDR, EXIT_CMD)
        self.state = 'CALCULATE_CHECKSUM'
    
    def _calculate_checksum(self):
        """Calculate CRC32 checksum"""
        values = list(self.exported_data.values())
        byte_data = b''.join(v.to_bytes(4, 'little') for v in values if isinstance(v, int))
        checksum = zlib.crc32(byte_data)
        self.exported_data['CRC32'] = checksum
        self.state = 'SAVE_FILE'
    
    def _save_file(self):
        """Save data to file"""
        if self.output_file is None:
            out_dir = os.path.join(SCRIPT_DIR, 'out')
            os.makedirs(out_dir, exist_ok=True)
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            self.output_file = os.path.join(out_dir, f"BBOX_{timestamp}.json")
        
        with open(self.output_file, 'w', encoding='utf-8') as f:
            json.dump(self.exported_data, f, indent=2, ensure_ascii=False)
        print(f"Exported data saved to {self.output_file}")
        print(f"Successfully exported {len(self.exported_data)} variables")
        self.state = 'DONE'

def export_bbox_data(master, slave_id, output_file=None):
    """
    Export blackbox data from DSP using FSM

    Args:
        master: ModbusMaster instance
        slave_id: Modbus slave ID
        output_file: Optional output file path

    Returns:
        dict: Exported data
    """
    exporter = BlackboxExporter(master, slave_id, output_file)
    return exporter.run()

def main():
    parser = argparse.ArgumentParser(description='Export blackbox data from DSP via Modbus')
    parser.add_argument('--port', default='COM4', help='COM port (default: COM4)')
    parser.add_argument('--baudrate', type=int, default=115200, help='Baud rate (default: 115200)')
    parser.add_argument('--slave', type=int, default=0x03, help='Modbus slave ID (default: 0x03)')
    parser.add_argument('--output', help='Output JSON file path')
    parser.add_argument('--timeout', type=float, default=0.2, help='Modbus timeout (default: 0.2)')

    args = parser.parse_args()

    # Create Modbus master
    master = ModbusMaster(args.port, args.baudrate, args.timeout)

    try:
        # Connect
        if not master.connect():
            print("Failed to connect to Modbus device")
            sys.exit(1)

        # Export data
        data = export_bbox_data(master, args.slave, args.output)

    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)

    finally:
        master.disconnect()

if __name__ == "__main__":
    main()

# python ExportBBox.py --slave 1 --output exported_data.json
