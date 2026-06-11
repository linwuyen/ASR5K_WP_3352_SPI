"""
Blackbox configuration constants for DSP communication.

This module contains command addresses and commands used for blackbox data export/import
via Modbus communication.
"""

# Command addresses (based on system documentation)
CMD_ADDR = 16      # Calibration command register (1 WORD - 16-bit)
ACK_ADDR = 17      # Calibration ack register (1 WORD - 16-bit)
DATA_ADDR = 22     # sRmtCal.u32Data register (2 WORDS - 32-bit)
RETURN_ADDR = 24   # sRmtCal.u32Return register (2 WORDS - 32-bit)

# Commands
EXPORT_CMD = 0x4003  # _EXPORT_BLOCK_BINARY_FROM_BBOX
IMPORT_CMD = 0x4002  # _IMPORT_BLOCK_BINARY_TO_BBOX
SAVE_CMD = 0x4004    # _EXIT_ACCESS_AND_SAVE_BBOX
EXIT_CMD = 0x4005    # _EXIT_ACCESS_BBOX

# Timing delays (in seconds)
DSP_PROCESS_DELAY = 0.2  # Delay for DSP to process commands/data