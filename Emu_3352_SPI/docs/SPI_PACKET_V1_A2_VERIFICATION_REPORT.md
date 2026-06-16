# SPI Packet V1 A2 Verification Report

## Scope

Verify the pure-C Packet V1 encoder, total-buffer parser, CRC16, malformed
matrix, and A2 streaming parser.

## Non-Scope

No SPIB runtime integration.
No DMA.
No legacy register protocol changes.
No CCS/syscfg/linker changes.
No board-level validation.

## Verification Level

- L1: host compile
- L2: host unit test
- L3: golden vector check
- L4: malformed / boundary matrix

## Test Environment

- Repository: `ASR5K_WP_3352_SPI` (split repo; repo root is the `WP_3352_SPI` level)
- Path: `Emu_3352_SPI`
- Host test macro: `SPI_PACKET_V1_HOST_TEST`
- Compiler: `gcc`, standard `c99`, warnings `-Wall -Wextra`
- Compiler command:

```bash
cd Emu_3352_SPI
gcc -std=c99 -Wall -Wextra -DSPI_PACKET_V1_HOST_TEST \
    SPI_PacketV1/spi_packet_crc16.c \
    SPI_PacketV1/spi_packet_v1.c \
    SPI_PacketV1/spi_packet_v1_test.c \
    -o spi_packet_v1_test.exe
./spi_packet_v1_test.exe
rm -f spi_packet_v1_test.exe
```

## Test Result

```text
PASS=126
FAIL=0
Exit code=0
Warnings=0
```

## Covered Behaviors

- CRC16-CCITT-FALSE check vector
- empty payload encode/parse
- 3-word payload round-trip
- 4096-word payload boundary
- 4097 payload reject
- bad header reject
- length mismatch reject
- CRC mismatch reject
- encoder defensive cases
- parser failure output safety
- streaming parser success
- streaming parser back-to-back frames
- streaming parser resync
- streaming parser finalize/truncated
- streaming parser null defensive behavior
- streaming parser vs total-buffer parser cross-check

## Golden Vectors

Deterministic golden vectors (CRC values computed from the production
encoder/CRC implementation) are recorded separately in
[`SPI_PACKET_V1_GOLDEN_VECTORS.md`](SPI_PACKET_V1_GOLDEN_VECTORS.md). The L1/L2
host run above re-derives the same CRC check constant (`0x29B1`) and exercises
the same encode/parse paths, so the golden vectors are reproducible from the
current source.

## Not Yet Verified

- TI C2000 compiler build
- CCS firmware build
- SPIB DMA runtime integration
- board-level SPI transfer
- AM3352 master interoperability

## Conclusion

A2 is verified at pure-C host-test level only.
A2 is not yet runtime-integrated.
