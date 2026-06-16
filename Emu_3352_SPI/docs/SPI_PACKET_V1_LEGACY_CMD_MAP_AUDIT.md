# SPI Packet V1 Legacy Command Map Audit

> **A3-0 audit (read-only).** This document inventories the existing legacy
> register / command address map before any Packet V1 Command Catalog is
> designed. It modifies no code and allocates no command IDs.

## Purpose

Packet V1 Word1 (Command ID) is a brand-new 16-bit semantic field. The legacy
SPI protocol already uses a 16-bit *register address* space defined in
`cmd_id.h`. Before designing the Packet V1 Command Catalog (A3), we must map the
legacy address space in full so that Packet V1 `command_id` values **do not
collide** — numerically or semantically — with established legacy register
addresses. A collision would let a Packet V1 command be mistaken for a legacy
register access (or vice versa) by a human reader, a debugger watch, or a future
compatibility wrapper.

## Source File

Two copies of `cmd_id.h` exist in the split repo (master and slave side of the
same protocol):

| Path | Role | Active `#define` | of which `*_spi_addr` |
|------|------|------------------|-----------------------|
| `Emu_3352_SPI/SPIB_Slave/cmd_id.h`  | Slave-side register map (authoritative for Packet V1 coexistence) | 225 | 210 |
| `Emu_3352_SPI/SPIA_Master/cmd_id.h` | Master-side copy of the same map | 231 | 210 |

Both define the **same 210 `*_spi_addr` register addresses** over the same
ranges. The master copy adds 6 extra non-address constants (the
`SPI_BLOCK_STATUS_*` FSM **status values**, not addresses). 4 lines in each file
are commented-out historical addresses (`0x041A`, `0x041C`, `0x0420`, `0x0422`).
Packet V1 lives on the slave side, so `SPIB_Slave/cmd_id.h` is treated as the
authoritative legacy map; the master copy is mirror-equivalent for address
purposes.

## Legacy Address Ranges

All values below are 16-bit register addresses (the legacy protocol's Word0).

### 1. General Read Only

- Approximate range: **`0x0400` – `0x042E`**
- Representative symbols:
  - `C2000_Version_spi_addr` = `0x0400`
  - `Machine_Status_spi_addr` = `0x0401`
  - `Vrms_MSB_spi_addr` = `0x0403`
  - `Calibration_State_spi_addr` = `0x041F`
  - `Temperature_MSB_spi_addr` = `0x042B`
  - `Vpeak_hold_LSB_spi_addr` = `0x042E`
- (Commented-out/reserved holes: `0x041A`, `0x041C`, `0x0420`, `0x0422`.)

### 2. Calibration Data

- Approximate range: **`0x0430` – `0x0485`**
- Representative symbols:
  - `Calibration_CheckSum_spi_addr` = `0x0430`
  - `Calibration_Data_Version_spi_addr` = `0x0431`
  - `ADC_to_V_FB_Scale_P_LSB_spi_addr_lr` = `0x0432`
  - `VSET_to_DAC_Scale_LSB_spi_addr_lr` = `0x0452`
  - `n_I_peak_Offset_MSB_spi_addr_hr` = `0x0485`
- Densely packed LSB/MSB × lr/hr scale/offset coefficients.

### 3. General Read / Write

- Approximate range: **`0x0700` – `0x070B`** (group `0x0700`)
- Representative symbols:
  - `V_and_I_Raw_Data_Counter_spi_addr` = `0x0700`
  - `External_Input_spi_addr` = `0x0701`
  - `Wave_Data_Address_Page_spi_addr` = `0x0704`
  - `Spi_Block_Status_spi_addr` = `0x0707`
  - `Spi_Block_Write_Index_spi_addr` = `0x0708`
  - `Spi_Block_Expected_CheckSum_spi_addr` = `0x070B`
- Note: the `Spi_Block_*` polling registers (`0x0707`–`0x070B`) sit in this
  R/W window; the actual block **data** payload lives at `0x3000+` (section 7).

### 4. Setting / Status / Fault Commands (`0x0800` group)

- Approximate range: **`0x0800` – `0x0805`**
- Representative symbols:
  - `Update_C2000_Setting_spi_addr` = `0x0800`
  - `V_and_I_Raw_Data_Operate_spi_addr` = `0x0801`
  - `RD_Mode_IO_Control_spi_addr` = `0x0802`
  - `Print_All_Cali_Variable_spi_addr` = `0x0803`
  - `C2000_Fault_Clear_spi_addr` = `0x0804`
  - `Spi_Status_Packet_Req_spi_addr` = `0x0805`
- Associated non-address constants: `SPIB_STATUS_PACKET_WORDS` = `16`,
  `SPIB_STATUS_PACKET_MAGIC` = `0x5AB0`.

### 5. Control Section (`0x0900` / `0x0A00` group)

- Approximate range: **`0x0900` – `0x0940`** (the source comment labels the group
  "`0x0900 & 0x0A00`", but the actual defined addresses stop at `0x0940`; no
  literal `0x0A00` address is currently defined — `0x0A00+` is reserved headroom).
- Two clusters:
  - Control toggles/commands `0x0900`–`0x091A`
  - Setpoint MSB/LSB values `0x0920`–`0x0940`
- Representative symbols:
  - `Output_ON_OFF_spi_addr` = `0x0900`
  - `Fan_Speed_spi_addr` = `0x0901`
  - `CPU_State_spi_addr` = `0x091A`
  - `I_RMS_Limiter_Set_MSB_spi_addr` = `0x0920`
  - `AC_Voltage_Set_MSB_spi_addr` = `0x092A`
  - `n_V_Peak_Trip_Set_LSB_spi_addr` = `0x0940`

### 6. Wave Download / M5 Commands

- Approximate range: **`0x0958` – `0x0961`**
- Address symbols:
  - `WAVE_PAGE_SELECT_ADDR` = `0x0958`
  - `WAVE_PAGE_STATUS_ADDR` = `0x095A`
  - `WAVE_BURST_BEGIN_ADDR` = `0x095B`
  - `WAVE_VALIDATE_ADDR` = `0x0960`
  - `WAVE_ACTIVATE_ADDR` = `0x0961`
- Associated non-address constants (status **values** / counts, not addresses):
  - `WAVE_STATUS_REG_READY` = `0x0001`, `WAVE_STATUS_RX_DONE` = `0x0002`,
    `WAVE_STATUS_ERROR` = `0x0004`
  - `WAVE_BURST_SAMPLE_COUNT` = `4096`
  - `WAVE_BURST_GUARD_FRAME_COUNT` = `2`, `WAVE_BURST_TRAILING_FRAME_COUNT` = `1`

### 7. Block Data Address Range

- Approximate range: **`0x3000` – `0x3FFF`**
- Symbols:
  - `Spi_Block_Data_Base_spi_addr` = `0x3000`
  - `Spi_Block_Data_Last_spi_addr` = `0x3FFE` (carries up to 4095 points)
  - `Spi_Block_End_spi_addr` = `0x3FFF` (RAM-page-ready marker)
- Associated non-address constants (master copy only — block FSM **status
  values**, not addresses): `SPI_BLOCK_STATUS_IDLE` = `0x0000`,
  `_RECEIVING` = `0x0001`, `_BUSY` = `0x0002`, `_READY` = `0x0003`,
  `_OVERFLOW` = `0x8000`, `_ERROR` = `0x8001`.

### Summary of occupied legacy address bands

| Band | Use |
|------|-----|
| `0x0400` – `0x042E` | General Read Only |
| `0x0430` – `0x0485` | Calibration Data |
| `0x0700` – `0x070B` | General Read / Write (incl. block polling) |
| `0x0800` – `0x0805` | Setting / Status / Fault |
| `0x0900` – `0x0940` | Control (toggles + setpoints) |
| `0x0958` – `0x0961` | Wave Download / M5 |
| `0x3000` – `0x3FFF` | Block Data streaming window |

## Collision Risk

Packet V1 Word1 (`command_id`) is numerically a 16-bit value in the same width
as a legacy register address. If a Packet V1 `command_id` reuses a number that is
also a legacy address, the value becomes ambiguous to humans and to any future
adapter that bridges the two protocols.

Concrete example risks:

- `Word1 = 0x0958` could be confused with `WAVE_PAGE_SELECT_ADDR`.
- `Word1 = 0x3000` could be confused with `Spi_Block_Data_Base_spi_addr` (wave/
  block data address).
- `Word1 = 0x0805` could be confused with `Spi_Status_Packet_Req_spi_addr`.
- `Word1 = 0x0800` could be confused with `Update_C2000_Setting_spi_addr`.
- `Word1 = 0x0400` could be confused with `C2000_Version_spi_addr`.

Additional values to keep in mind (not legacy *addresses*, but legacy *magic /
status values* that a reader could confuse with a Packet V1 ID):

- `0xA55A` — Packet V1 header magic (Word0). Distinct field from Word1, but a
  `command_id` of `0xA55A` would be visually confusing in a dump.
- `0x5AB0` — `SPIB_STATUS_PACKET_MAGIC`.
- `0x8000` / `0x8001` — legacy block-FSM `OVERFLOW` / `ERROR` status values
  (these are status *values*, not addresses or command IDs, so there is no
  address collision — but see the namespace note below).

## Initial Design Rule

> **Proposal only — not implementation.** No command IDs are allocated by this
> audit. The following is a recommended starting rule for A3 review.

- Legacy addresses remain **owned by the legacy register protocol**. Packet V1
  does not redefine, alias, or reuse any `*_spi_addr` value.
- Packet V1 `command_id` must use a **separate namespace** from legacy addresses.
- Packet V1 `command_id` values must **not** use `0x0400`–`0x0FFF` or
  `0x3000`–`0x3FFF` unless explicitly documented as a compatibility wrapper.
- Candidate Packet V1 namespace:
  - `0x8000`–`0x8FFF` — Packet V1 control / management commands.
  - `0x9000`–`0x9FFF` — reserved for Packet V1 wave-related future commands.
  - `0xA000`–`0xAFFF` — reserved; **avoid collision with header magic `0xA55A`**
    by an explicit carve-out rule if this band is ever used (e.g. never assign
    `command_id == 0xA55A`).
- Observation for A3: the proposed `0x8000`+ band does not collide with any
  legacy *address* (legacy addresses top out at `0x3FFF`), but `0x8000`/`0x8001`
  already appear as legacy block-FSM *status values*. Since `command_id` and
  block-status-value are different fields, this is namespace-distinct; document
  it explicitly so the overlap is intentional rather than accidental.

## Open Questions

For A3 proper to resolve before allocating any IDs:

1. **Should Packet V1 support a legacy-register wrapper command?**
   e.g. `PKTV1_CMD_LEGACY_REG_ACCESS` with `payload = { address, value, op }`,
   letting Packet V1 tunnel a legacy register read/write without giving the
   command itself a legacy address.
2. **Should Packet V1 wave commands mirror legacy wave commands or use new IDs?**
   (Mirror `WAVE_*` semantics under new `command_id`s, vs. invent a fresh wave
   command set.)
3. **Should legacy wave download remain entirely legacy for B01F compatibility?**
   (i.e. Packet V1 deliberately does *not* touch the wave path until much later.)
4. **Should Packet V1 initially define only meta commands** such as:
   - `PING`
   - `GET_VERSION`
   - `GET_CAPS`
   - `ECHO`
   - `ERROR_RESPONSE`

## Recommendation

- A3 should **not** allocate command IDs until this audit is reviewed.
- A3 should **start from the existing `cmd_id.h` map**, not from a blank catalog,
  so the Packet V1 namespace is chosen to provably avoid the occupied legacy
  bands documented above.
