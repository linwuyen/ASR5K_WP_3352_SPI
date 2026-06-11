import unittest
import sys
import os

# Add the src directory to the Python path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src'))

from ModbusMaster import ModbusMaster, ModbusCRC  # Import ModbusCRC class

class TestModbusMaster(unittest.TestCase):
    def setUp(self):
        # Use your actual settings: COM4, 115200, default timeout=1.0
        self.master = ModbusMaster(port='COM4', baudrate=115200)
        self.slave_id = 3  # Your Slave ID
        self.connected = self.master.connect()

    def test_read_holding_registers(self):
        if not self.connected:
            self.skipTest("No device connected")
        # Simulate reading 10 registers from address 0 (actual test requires real device)
        # Note: This test will fail without a device; used to verify logic
        response = self.master.read_holding_registers(self.slave_id, 0, 10)
        self.assertIsInstance(response, list)
        self.assertEqual(len(response), 10)

    def test_write_single_register(self):
        if not self.connected:
            self.skipTest("No device connected")
        # Simulate writing a single register (actual test requires real device)
        success = self.master.write_single_register(self.slave_id, 5, 1234)
        self.assertTrue(success)

    def test_write_multiple_registers(self):
        if not self.connected:
            self.skipTest("No device connected")
        # Simulate writing multiple registers (actual test requires real device)
        success = self.master.write_multiple_registers(self.slave_id, 10, [100, 200, 300])
        self.assertTrue(success)

    def test_crc_calculation(self):
        # Test CRC-16 calculation using ModbusCRC class
        data = b'\x01\x03\x00\x00\x00\x0A'  # Example request
        crc = ModbusCRC.calculate(data)
        self.assertEqual(crc, 0xCDC5)  # Calculated CRC value

if __name__ == '__main__':
    unittest.main()