SECTIONS
{
    spia_master_wave_ram : > RAMGS0
    spib_block_ram : > RAMGS2
    spib_slave_state : > RAMGS3
    asr5k_spi_selftest_state : > RAMGS3
    asr5k_spi_selftest_config : > RAMGS3
    spi_fifo_state : > RAMGS3
    spib_pingpong_state : > RAMGS3
    spib_rx_wave_buf0 : > RAMGS7
    fake_sdram_page0 : > RAMGS4, type = NOINIT
    fake_sdram_page1 : > RAMGS5, type = NOINIT
    fake_sdram_page2 : > RAMGS6, type = NOINIT
}
