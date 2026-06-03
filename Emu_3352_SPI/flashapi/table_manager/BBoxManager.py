#!/usr/bin/env python3
"""
BBoxManager.py - Blackbox Data Manager

A menu-driven interface for exporting and importing blackbox data from/to DSP via Modbus.

Usage:
    python BBoxManager.py

Dependencies:
    - ModbusMaster.py (in src directory)
    - ExportBBox.py (in same directory)
    - ImportBBox.py (in same directory)
"""

import sys
import os
from datetime import datetime

# Get the script directory
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

# Add the src and configs directories to the Python path
sys.path.insert(0, os.path.join(SCRIPT_DIR, 'src'))
sys.path.insert(0, os.path.join(SCRIPT_DIR, 'configs'))

from ModbusMaster import ModbusMaster
from ExportBBox import export_bbox_data
from ImportBBox import import_bbox_data

def display_menu():
    """Display the main menu"""
    print("\n" + "="*50)
    print("Blackbox Data Manager")
    print("="*50)
    print("1. Export blackbox data from DSP")
    print("2. Import blackbox data to DSP")
    print("3. Exit")
    print("="*50)

def get_user_choice():
    """Get user choice from menu"""
    while True:
        try:
            choice = input("Please select an option (1-3): ").strip()
            if choice in ['1', '2', '3']:
                return int(choice)
            else:
                print("Invalid choice. Please enter 1, 2, or 3.")
        except KeyboardInterrupt:
            print("\nExiting...")
            sys.exit(0)
        except:
            print("Invalid input. Please try again.")

def export_data():
    """Handle export operation"""
    print("\n--- Export Blackbox Data ---")

    # Get parameters
    port = input("COM port (default: COM4): ").strip() or "COM4"
    baudrate = input("Baud rate (default: 115200): ").strip()
    baudrate = int(baudrate) if baudrate.isdigit() else 115200
    slave_id = input("Modbus slave ID (default: 0x03): ").strip()
    slave_id = int(slave_id, 16) if slave_id.startswith('0x') else (int(slave_id) if slave_id.isdigit() else 0x03)
    timeout = input("Timeout (default: 0.2): ").strip()
    timeout = float(timeout) if timeout.replace('.', '').isdigit() else 0.2
    output_file = input("Output file path (optional, will auto-generate if empty): ").strip() or None

    # Create Modbus master
    master = ModbusMaster(port, baudrate, timeout)

    try:
        # Connect
        print(f"Connecting to {port} at {baudrate} baud...")
        if not master.connect():
            print("Failed to connect to Modbus device")
            return

        # Export data
        print("Starting export...")
        data = export_bbox_data(master, slave_id, output_file)
        print(f"Export completed. {len(data)} variables exported.")

    except Exception as e:
        print(f"Error during export: {e}")

    finally:
        master.disconnect()
        print("Disconnected from Modbus device.")

def import_data():
    """Handle import operation"""
    print("\n--- Import Blackbox Data ---")

    # Get input file
    while True:
        input_file = input("Enter the path to the JSON file to import: ").strip()
        if os.path.exists(input_file):
            break
        else:
            print(f"File '{input_file}' does not exist. Please try again.")

    # Get parameters
    port = input("COM port (default: COM4): ").strip() or "COM4"
    baudrate = input("Baud rate (default: 115200): ").strip()
    baudrate = int(baudrate) if baudrate.isdigit() else 115200
    slave_id = input("Modbus slave ID (default: 0x03): ").strip()
    slave_id = int(slave_id, 16) if slave_id.startswith('0x') else (int(slave_id) if slave_id.isdigit() else 0x03)
    timeout = input("Timeout (default: 0.1): ").strip()
    timeout = float(timeout) if timeout.replace('.', '').isdigit() else 0.1

    # Create Modbus master
    master = ModbusMaster(port, baudrate, timeout)

    try:
        # Connect
        print(f"Connecting to {port} at {baudrate} baud...")
        if not master.connect():
            print("Failed to connect to Modbus device")
            return

        # Import data
        print(f"Starting import from {input_file}...")
        success = import_bbox_data(master, slave_id, input_file)
        if success:
            print("Import completed successfully.")
        else:
            print("Import failed.")

    except Exception as e:
        print(f"Error during import: {e}")

    finally:
        master.disconnect()
        print("Disconnected from Modbus device.")

def main():
    """Main function"""
    print("Welcome to Blackbox Data Manager")

    while True:
        display_menu()
        choice = get_user_choice()

        if choice == 1:
            export_data()
        elif choice == 2:
            import_data()
        elif choice == 3:
            print("Exiting Blackbox Data Manager. Goodbye!")
            break

        # Pause before showing menu again
        input("\nPress Enter to continue...")

if __name__ == "__main__":
    main()