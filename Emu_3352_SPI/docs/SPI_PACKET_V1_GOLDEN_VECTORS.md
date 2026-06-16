# SPI Packet V1 Golden Vectors

Deterministic reference vectors for SPI Packet V1. All CRC values below were
**computed from the production implementation** (`spi_packet_crc16.c` +
`spi_packet_v1.c`, via `SpiPacketV1_Encode` / `SpiPacketCrc16_*`), not by hand.

Frame layout (all values are 16-bit words):

```
w0 = Header/Magic (0xA55A)   w1 = Command ID   w2 = Data Length (N words)
w3..3+N-1 = Payload          w3+N = CRC16/CCITT-FALSE over w0..2+N
```

CRC parameters: CRC16/CCITT-FALSE — poly `0x1021`, init `0xFFFF`, refin/refout
false, xorout `0x0000`, check (`"123456789"`) = `0x29B1`. Each 16-bit word is fed
to the CRC big-endian (high byte first, then low byte). The CRC word itself is
excluded from the CRC coverage.

---

## Vector 1 — CRC check constant

- Input string: `"123456789"` (bytes `0x31 0x32 ... 0x39`)
- Expected CRC16-CCITT-FALSE: **`0x29B1`**
- Confirmed by implementation: `SpiPacketCrc16_ComputeBytes("123456789", 9) == 0x29B1`

## Vector 2 — empty payload packet

- Header (w0)     = `0xA55A`
- Command ID (w1) = `0x1234`
- Data Length (w2) = `0x0000` (N = 0)
- Payload         = none
- CRC (w3)        = **`0x100F`** (computed)
- Full frame (4 words):

```text
0xA55A 0x1234 0x0000 0x100F
```

- Expected parse result = `SPI_PACKET_V1_OK`, `cmdId = 0x1234`, `payloadWords = 0`,
  `payload = NULL`

## Vector 3 — 3-word payload packet

- Header (w0)     = `0xA55A`
- Command ID (w1) = `0x0100`
- Data Length (w2) = `0x0003` (N = 3)
- Payload (w3..w5) = `0xDEAD, 0xBEEF, 0x0042`
- CRC (w6)        = **`0x724F`** (computed)
- Full frame (7 words):

```text
0xA55A 0x0100 0x0003 0xDEAD 0xBEEF 0x0042 0x724F
```

- Expected parse result = `SPI_PACKET_V1_OK`, `cmdId = 0x0100`, `payloadWords = 3`,
  payload preserved

## Vector 4 — max payload boundary (4096)

- Command ID      = `0x0200`
- Data Length     = `4096` (= `SPI_PACKET_V1_MAX_PAYLOAD_WORDS`, the spec maximum)
- Payload pattern = `payload[i] = (uint16_t)(i + 1)`
  - First payload word (w3)        = `0x0001`
  - Last payload word (w[3+4095])  = `0x1000`
- Total frame words = `4 + 4096` = **`4100`**
- CRC (last word)   = **`0x25CA`** (computed for the pattern above)
- Expected result   = `SPI_PACKET_V1_OK`
- The full 4100-word frame is intentionally **not** dumped here; the boundary is
  characterized by length, first/last payload word, and CRC.

## Vector 5 — over-max reject (4097)

- Data Length = `4097` (= `SPI_PACKET_V1_MAX_PAYLOAD_WORDS + 1`)
- Expected encode result = `SPI_PACKET_V1_ERR_PAYLOAD_TOO_LARGE`
- Expected parse result (frame declaring length 4097) =
  `SPI_PACKET_V1_ERR_PAYLOAD_TOO_LARGE`

---

## Reproduction

Vectors 1–5 are derived from the same source built by the host test. The CRC
check constant (Vector 1) and the encode/parse boundary behavior (Vectors 4–5)
are also asserted by `spi_packet_v1_test.c` under `SPI_PACKET_V1_HOST_TEST`
(`PASS=126 FAIL=0`). The computed CRCs for Vectors 2–3 were produced by linking
the production `spi_packet_crc16.c` + `spi_packet_v1.c` and calling
`SpiPacketV1_Encode`; the encoder/parser source was not modified to generate
them.
