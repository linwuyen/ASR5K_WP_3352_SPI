#!/usr/bin/env python3
"""
test_modbus_interactive.py - Interactive Modbus testing tool

This script provides an interactive menu to test individual Modbus functions
independently for debugging and verification purposes.
"""

import sys
import os

# Add the current directory to Python path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from ModbusMaster import ModbusMaster

def print_menu():
    """Print the interactive menu"""
    print("\n" + "="*60)
    print("Modbus Testing Tool - Individual Function Testing")
    print("="*60)
    print("1. Connect to COM4 (115200 baud)")
    print("2. Disconnect")
    print("3. Test Read Holding Registers (Function 0x03)")
    print("4. Test Write Single Register (Function 0x06)")
    print("5. Test Write Multiple Registers (Function 0x10)")
    print("6. Test DSP Blackbox Commands")
    print("7. Test DSP Register Reads")
    print("8. Run Logged Test Commands")
    print("9. Exit")
    print("="*60)

def get_user_choice():
    """Get user menu choice"""
    try:
        choice = input("Enter your choice (1-9): ").strip()
        return int(choice)
    except ValueError:
        return None

def get_slave_id():
    """Get slave ID from user"""
    try:
        slave_id = int(input("Enter slave ID (default 3): ").strip() or "3")
        return slave_id
    except ValueError:
        print("Invalid slave ID, using default 3")
        return 3

def test_read_holding_registers(master):
    """Test read holding registers function"""
    slave_id = get_slave_id()
    try:
        address = int(input("Enter starting address: ").strip())
        quantity = int(input("Enter quantity (1-125): ").strip())

        print(f"Reading {quantity} registers from address {address}, slave {slave_id}...")
        result = master.read_holding_registers(slave_id, address, quantity)

        if result:
            print(f"Success! Read {len(result)} registers:")
            for i, val in enumerate(result):
                print(f"  [{address + i}] = 0x{val:04X} ({val})")
        else:
            print("Read failed!")

    except ValueError:
        print("Invalid input!")

def test_write_single_register(master):
    """Test write single register function"""
    slave_id = get_slave_id()
    try:
        address = int(input("Enter address: ").strip())
        value = int(input("Enter value (0-65535): ").strip())

        print(f"Writing 0x{value:04X} ({value}) to address {address}, slave {slave_id}...")
        success = master.write_single_register(slave_id, address, value)

        if success:
            print("Write successful!")
        else:
            print("Write failed!")

    except ValueError:
        print("Invalid input!")

def test_write_multiple_registers(master):
    """Test write multiple registers function"""
    slave_id = get_slave_id()
    try:
        address = int(input("Enter starting address: ").strip())
        values_input = input("Enter values separated by space: ").strip()
        values = [int(x) for x in values_input.split()]

        print(f"Writing {len(values)} registers to address {address}, slave {slave_id}...")
        print(f"Values: {[f'0x{v:04X} ({v})' for v in values]}")
        success = master.write_multiple_registers(slave_id, address, values)

        if success:
            print("Write successful!")
        else:
            print("Write failed!")

    except ValueError:
        print("Invalid input!")

def test_dsp_commands(master):
    """Test DSP blackbox commands"""
    print("\nDSP Blackbox Commands:")
    print("1. EXPORT (0x4003)")
    print("2. IMPORT (0x4002)")
    print("3. SAVE (0x4004)")
    print("4. EXIT (0x4005)")

    try:
        cmd_choice = int(input("Choose command (1-4): ").strip())
        slave_id = get_slave_id()

        commands = {
            1: ("EXPORT", 0x4003),
            2: ("IMPORT", 0x4002),
            3: ("SAVE", 0x4004),
            4: ("EXIT", 0x4005)
        }

        if cmd_choice in commands:
            cmd_name, cmd_value = commands[cmd_choice]
            print(f"Sending {cmd_name} command (0x{cmd_value:04X}) to CMD_ADDR (82), slave {slave_id}...")
            success = master.write_single_register(slave_id, 82, cmd_value)

            if success:
                print(f"{cmd_name} command sent successfully!")
            else:
                print(f"Failed to send {cmd_name} command!")
        else:
            print("Invalid command choice!")

    except ValueError:
        print("Invalid input!")

def test_dsp_register_reads(master):
    """Test reading DSP registers"""
    print("\nDSP Register Reads:")
    print("1. Read CMD register (82)")
    print("2. Read ACK register (83)")
    print("3. Read DATA registers (146-147)")
    print("4. Read RETURN registers (148-149)")
    print("5. Read all status registers")

    try:
        read_choice = int(input("Choose read option (1-5): ").strip())
        slave_id = get_slave_id()

        if read_choice == 1:
            print(f"Reading CMD register (82), slave {slave_id}...")
            result = master.read_holding_registers(slave_id, 82, 1)
            if result:
                print(f"CMD register: 0x{result[0]:04X} ({result[0]})")

        elif read_choice == 2:
            print(f"Reading ACK register (83), slave {slave_id}...")
            result = master.read_holding_registers(slave_id, 83, 1)
            if result:
                print(f"ACK register: 0x{result[0]:04X} ({result[0]})")

        elif read_choice == 3:
            print(f"Reading DATA registers (146-147), slave {slave_id}...")
            result = master.read_holding_registers(slave_id, 146, 2)
            if result:
                value = (result[1] << 16) | result[0]
                print(f"DATA registers: [{result[0]}, {result[1]}] = 0x{value:08X} ({value})")

        elif read_choice == 4:
            print(f"Reading RETURN registers (148-149), slave {slave_id}...")
            result = master.read_holding_registers(slave_id, 148, 2)
            if result:
                value = (result[1] << 16) | result[0]
                print(f"RETURN registers: [{result[0]}, {result[1]}] = 0x{value:08X} ({value})")

        elif read_choice == 5:
            print(f"Reading all status registers, slave {slave_id}...")
            # Read CMD
            result = master.read_holding_registers(slave_id, 82, 1)
            if result:
                print(f"CMD (82): 0x{result[0]:04X} ({result[0]})")

            # Read ACK
            result = master.read_holding_registers(slave_id, 83, 1)
            if result:
                print(f"ACK (83): 0x{result[0]:04X} ({result[0]})")

            # Read DATA
            result = master.read_holding_registers(slave_id, 146, 2)
            if result:
                value = (result[1] << 16) | result[0]
                print(f"DATA (146-147): 0x{value:08X} ({value})")

            # Read RETURN
            result = master.read_holding_registers(slave_id, 148, 2)
            if result:
                value = (result[1] << 16) | result[0]
                print(f"RETURN (148-149): 0x{value:08X} ({value})")

        else:
            print("Invalid read choice!")

    except ValueError:
        print("Invalid input!")

def test_logged_commands(master):
    """Run the logged test commands from the serial log"""
    print("\nRunning Logged Test Commands:")
    print("These commands replicate the requests from the provided serial log.")
    
    slave_id = 3  # All logged commands use slave ID 3
    
    # Test 1: Write Single Register - Address 82, Value 16387
    print("\n1. Write Single Register: Address 82, Value 16387 (0x4003)")
    success = master.write_single_register(slave_id, 82, 16387)
    if success:
        print("   Success!")
    else:
        print("   Failed!")
    
    # Test 2: Write Multiple Registers - Address 148, Values [1, 0]
    print("\n2. Write Multiple Registers: Address 148, Values [1, 0]")
    success = master.write_multiple_registers(slave_id, 148, [1, 0])
    if success:
        print("   Success!")
    else:
        print("   Failed!")
    
    # Test 3: Write Multiple Registers - Address 148, Values [2, 0]
    print("\n3. Write Multiple Registers: Address 148, Values [2, 0]")
    success = master.write_multiple_registers(slave_id, 148, [2, 0])
    if success:
        print("   Success!")
    else:
        print("   Failed!")
    
    # Test 4: Write Single Register - Address 83, Value 16389
    print("\n4. Write Single Register: Address 83, Value 16389 (0x4005)")
    success = master.write_single_register(slave_id, 83, 16389)
    if success:
        print("   Success!")
    else:
        print("   Failed!")

def main():
    master = None
    connected = False

    print("Modbus Testing Tool - Individual Function Testing")
    print("Default connection: COM4, 115200 baud, slave ID 3")

    while True:
        print_menu()
        choice = get_user_choice()

        if choice is None:
            print("Invalid choice. Please enter a number between 1 and 9.")
            continue

        if choice == 1:  # Connect
            if connected:
                print("Already connected. Disconnect first.")
                continue

            print("Connecting to COM4 at 115200 baud...")
            master = ModbusMaster("COM4", 115200, timeout=1.0)
            if master.connect():
                connected = True
                print("Connected successfully!")
            else:
                print("Failed to connect.")

        elif choice == 2:  # Disconnect
            if not connected:
                print("Not connected.")
                continue

            master.disconnect()
            connected = False
            print("Disconnected.")

        elif choice == 3:  # Test Read Holding Registers
            if not connected:
                print("Not connected. Please connect first.")
                continue
            test_read_holding_registers(master)

        elif choice == 4:  # Test Write Single Register
            if not connected:
                print("Not connected. Please connect first.")
                continue
            test_write_single_register(master)

        elif choice == 5:  # Test Write Multiple Registers
            if not connected:
                print("Not connected. Please connect first.")
                continue
            test_write_multiple_registers(master)

        elif choice == 6:  # Test DSP Commands
            if not connected:
                print("Not connected. Please connect first.")
                continue
            test_dsp_commands(master)

        elif choice == 7:  # Test DSP Register Reads
            if not connected:
                print("Not connected. Please connect first.")
                continue
            test_dsp_register_reads(master)

        elif choice == 8:  # Run Logged Test Commands
            if not connected:
                print("Not connected. Please connect first.")
                continue
            test_logged_commands(master)

        elif choice == 9:  # Exit
            if connected:
                master.disconnect()
                print("Disconnected.")
            print("Exiting program.")
            break

        else:
            print("Invalid choice. Please enter a number between 1 and 9.")

        # Small delay and wait for user input
        input("\nPress Enter to continue...")

if __name__ == "__main__":
    main()