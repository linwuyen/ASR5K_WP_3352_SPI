import unittest
from src.main import TableManagerFSM

class TestTableManagerFSM(unittest.TestCase):
    def test_init(self):
        fsm = TableManagerFSM()
        self.assertEqual(fsm.state, 'INIT')

if __name__ == '__main__':
    unittest.main()