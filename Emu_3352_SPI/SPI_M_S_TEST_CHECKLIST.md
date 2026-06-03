# SPI Master / Slave Test Checklist

## Hardware Wiring

- [ ] Power is off before installing jumpers.
- [ ] SPIA GPIO16 (PICO/SIMO/MOSI) is connected to SPIB GPIO63 (PICO/SIMO/MOSI).
- [ ] SPIA GPIO17 (POCI/SOMI/MISO) is connected to SPIB GPIO64 (POCI/SOMI/MISO).
- [ ] SPIA GPIO18 (CLK) is connected to SPIB GPIO65 (CLK).
- [ ] SPIA GPIO19 (PTE/STE) is connected to SPIB GPIO66 (PTE/STE).
- [ ] Master and Slave grounds are connected.
- [ ] No SPI internal loopback is enabled during M/S testing.

## SysConfig

- [ ] SPIA `mySPI0` is Controller mode.
- [ ] SPIA uses GPIO16, GPIO17, GPIO18, and GPIO19.
- [ ] SPIA is 12.5 MHz, 16-bit, `SPI_PROT_POL0PHA0`, and PTE active-low.
- [ ] SPIB `SPIB_SYSTEM` is Peripheral mode.
- [ ] SPIB uses GPIO63, GPIO64, GPIO65, and GPIO66.
- [ ] SPIB is 12.5 MHz, 16-bit, `SPI_PROT_POL0PHA0`, PTE active-low, FIFO enabled, and high-speed mode enabled.
- [ ] Generated RAM and Flash `board.c` files contain the same SPIA and SPIB settings.

## Build

- [ ] `CPU1_RAM` builds without errors.
- [ ] `CPU1_FLASH` builds without errors.
- [ ] The linker map places `g_u16SpiBlockRam` / `spib_block_ram` in `RAMGS2`.
- [ ] `RAMGS2` is not assigned to another section.

## Basic Communication

- [ ] `C2000_Version_spi_addr` returns `DSP_FW_Version_Code_CPU1`.
- [ ] `CPU2_Version_spi_addr` returns `DSP_FW_Version_Code_CPU2`.
- [ ] `Startup_State_spi_addr` returns `startupFlags`.
- [ ] `Output_ON_OFF_spi_addr` write returns the written bit value.
- [ ] Response address equals request address plus the byte checksum of response data.
- [ ] Null frame clocks out the prepared Slave response without shifting the pipeline.

## Block Transfer

- [ ] Short block write stores the expected data in `g_u16SpiBlockRam`.
- [ ] `Spi_Block_Write_Index_spi_addr` reports the expected write count.
- [ ] `Spi_Block_CheckSum_spi_addr` reports the expected checksum.
- [ ] `Spi_Block_End_spi_addr` changes status to busy before ready.
- [ ] `Spi_Block_Progress_spi_addr` advances to 100.
- [ ] Busy guard rejects a new block write while commit is active.
- [ ] 4095-point waveform transfer completes without overflow.
- [ ] 4095-point sine waveform transfer completes without overflow.

## Recovery

- [ ] Slave silence timeout resets RX/TX FIFOs and restores alignment.
- [ ] Master timeout recovery clears its queue and returns to idle.
- [ ] Communication resumes after a disconnected or incomplete frame.
- [ ] `spiB_slave.stat` does not retain `_SSS_GET_ERROR` after recovery.

## F28384D Return Migration

- [ ] Only `cmd_id.h`, `cmd_parser.h`, `spi_slave.h`, and `spi_b_slave.c` are synchronized back.
- [ ] F28384D keeps SPIB GPIO63, GPIO64, GPIO65, and GPIO66.
- [ ] F28384D keeps Peripheral mode, 16-bit, `SPI_PROT_POL0PHA0`, PTE active-low, and high-speed mode.
- [ ] F28384D keeps the production 40 MHz setting.
- [ ] F28384D keeps `spib_block_ram` in `RAMGS1`.
- [ ] F28384D does not receive SPIA Master, 12.5 MHz test, jumper, or loopback-only code.
