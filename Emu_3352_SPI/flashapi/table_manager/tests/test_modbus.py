import os
import sys
import unittest
from unittest.mock import MagicMock, patch

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "src"))

import ModbusMaster as modbus_module
from ModbusMaster import ModbusMaster


class TestModbusMaster(unittest.TestCase):
    def setUp(self):
        patcher = patch.object(modbus_module, "ModbusSerialClient")
        self.addCleanup(patcher.stop)
        self.client_class = patcher.start()
        self.client = MagicMock()
        self.client_class.return_value = self.client
        self.master = ModbusMaster(port="COM4", baudrate=115200)

    def test_connect(self):
        self.client.connect.return_value = True

        self.assertTrue(self.master.connect())
        self.client.connect.assert_called_once_with()

    def test_read_holding_registers(self):
        response = MagicMock()
        response.isError.return_value = False
        response.registers = list(range(10))
        self.client.read_holding_registers.return_value = response

        result = self.master.read_holding_registers(3, 0, 10)

        self.assertEqual(result, list(range(10)))
        self.client.read_holding_registers.assert_called_once_with(
            address=0, count=10, slave=3
        )

    def test_read_holding_registers_rejects_invalid_quantity(self):
        self.assertIsNone(self.master.read_holding_registers(3, 0, 0))
        self.client.read_holding_registers.assert_not_called()

    def test_write_single_register(self):
        response = MagicMock()
        response.isError.return_value = False
        self.client.write_register.return_value = response

        self.assertTrue(self.master.write_single_register(3, 5, 1234))
        self.client.write_register.assert_called_once_with(
            address=5, value=1234, slave=3
        )

    def test_write_multiple_registers(self):
        response = MagicMock()
        response.isError.return_value = False
        self.client.write_registers.return_value = response

        self.assertTrue(
            self.master.write_multiple_registers(3, 10, [100, 200, 300])
        )
        self.client.write_registers.assert_called_once_with(
            address=10, values=[100, 200, 300], slave=3
        )

    def test_write_multiple_registers_rejects_empty_values(self):
        self.assertFalse(self.master.write_multiple_registers(3, 10, []))
        self.client.write_registers.assert_not_called()


if __name__ == "__main__":
    unittest.main()
