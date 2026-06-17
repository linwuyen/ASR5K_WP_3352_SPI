# SPI Packet V1 — GitHub Actions Host-Test CI

Status: **Tooling / CI only**
Board-verifiable: **NO** (CI does not change firmware behavior or touch board runtime)
Workflow: `.github/workflows/spi_packet_v1_host_tests.yml`

## Purpose

Run the SPI Packet V1 pure-C host tests automatically on GitHub so future
`feature/spi-packet-v1-*` branches and PRs into `main` are verified without
pasting terminal logs by hand.

## When it runs

- **push** to any branch matching `feature/spi-packet-v1-*`
- **pull_request** targeting `main`

## What it does (`host-tests` job, `ubuntu-latest`)

1. Checkout the repository.
2. Print `gcc --version`.
3. Build + run the **A2** parser/encoder/CRC host test; assert `PASS=126 FAIL=0`.
4. If the **A4** dispatcher files exist, build + run them; assert `PASS=104 FAIL=0`.
5. If the **A5** loopback files exist, build + run them; assert `PASS=107 FAIL=0`.

A4 and A5 steps are guarded by `[ -f ... ]` so the same workflow works on older
branches that predate those modules (it prints a "skipping" note instead of
failing).

## Why it fails correctly

- All builds use `-std=c99 -Wall -Wextra -Werror`, so **any warning fails CI**.
- Each test binary returns nonzero on failure; `set -euo pipefail` plus the
  `grep -E "PASS=NNN[[:space:]]+FAIL=0"` check fail the step if the exact
  expected pass line is missing.
- The expectation values are pinned per module; a regression (e.g. a new failing
  case, or a changed pass count) breaks CI rather than passing silently.

## Local reproduction

The workflow mirrors the documented per-module commands. From `Emu_3352_SPI`:

```bash
# A2
gcc -std=c99 -Wall -Wextra -Werror -DSPI_PACKET_V1_HOST_TEST \
  SPI_PacketV1/spi_packet_crc16.c SPI_PacketV1/spi_packet_v1.c \
  SPI_PacketV1/spi_packet_v1_test.c -o spi_packet_v1_test
./spi_packet_v1_test          # expect PASS=126 FAIL=0

# A4
gcc -std=c99 -Wall -Wextra -Werror -DSPI_PACKET_V1_HOST_TEST \
  SPI_PacketV1/spi_packet_v1_dispatch.c \
  SPI_PacketV1/spi_packet_v1_dispatch_test.c -o spi_packet_v1_dispatch_test
./spi_packet_v1_dispatch_test  # expect PASS=104 FAIL=0

# A5
gcc -std=c99 -Wall -Wextra -Werror -DSPI_PACKET_V1_HOST_TEST \
  SPI_PacketV1/spi_packet_crc16.c SPI_PacketV1/spi_packet_v1.c \
  SPI_PacketV1/spi_packet_v1_dispatch.c SPI_PacketV1/spi_packet_v1_loopback.c \
  SPI_PacketV1/spi_packet_v1_loopback_test.c -o spi_packet_v1_loopback_test
./spi_packet_v1_loopback_test  # expect PASS=107 FAIL=0
```

## Isolation

CI adds only the workflow file (and this doc). It does not modify any firmware
runtime, `SPIB_Slave`/`SPIA_Master`, `wave_download`, `cmd_id.h`, `syscfg`,
linker `.cmd`, `.cproject`/`.project`/`.settings`/`.agent`, or any existing
A2/A4/A5 source. The B01F baseline is unaffected.
