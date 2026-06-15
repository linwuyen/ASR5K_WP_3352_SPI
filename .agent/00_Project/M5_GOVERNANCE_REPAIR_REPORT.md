# M5 Governance Repair Report

Status: PASS WITH OPEN PRODUCTION GATES
Date: 2026-06-14
Scope: `.agent` governance, M5 specification, tooling declarations, and
minimum CI checks only

## Boot Report

### Task

Repair the ASR5K `.agent` knowledge base before any production EMIF1 backend
or other firmware modification.

### Controlling Documents Read

1. `.agent/AGENT_ENTRYPOINT.md`
2. `.agent/00_Project/ARCHITECTURE_AUTHORITY.md`
3. `.agent/00_Project/ASR5K_DECISIONS.md`
4. `.agent/ARCHITECTURE_CONFLICT_REGISTER.md`
5. `.agent/DOCUMENT_STATUS_REGISTRY.md`
6. `.agent/01_Architecture/ASR5K設計文件/asr5k_readme.pdf`
7. Applicable D01, D02, D03, D04, D05, D07, and D10 documents
8. `.agent/01_Architecture/SPEC_M5_WAVE_DOWNLOAD.md`
9. `.agent/02_Milestones/M5R_PHASE2_BURST_TRANSPORT.md`
10. `.agent/00_Project/ASR5K_HANDOFF.md`
11. `.agent/00_Project/ASR5K_STATUS.md`
12. Current `Emu_3352_SPI` wave-download headers, implementation, parser, and
    self-test evidence

The formal product PDF contains older Flash-runtime and CPU ownership
assumptions. FD-012 and CR-001/CR-004 apply the approved scoped override for
wave runtime source and CPU ownership.

### Frozen Assertions

- Legacy Register Protocol remains production.
- SPIB RX owns DMA CH3; SPIB TX owns DMA CH4.
- DMA CH5 and CH6 remain reserved.
- CPU1 owns system control, parser, download, validation, and writes.
- CPU2 owns DDS runtime.
- EMIF1 SDRAM is the Wave Runtime Source.
- W25Q64 is limited to Boot, OTA, and Maintenance and is not a DDS runtime
  source.
- D11 Packet Protocol remains candidate-only.

### Known Conflicts and Applied Resolutions

- CR-001: CPU2 remains the DDS runtime owner.
- CR-002: CH5 and CH6 remain reserved.
- CR-003 and CR-005: Legacy remains production; Packet remains candidate-only.
- CR-004: CPU1 writes and administers waveform data; CPU2 reads approved EMIF1
  waveform data at runtime.
- M5 state-model drift: D10 and current implementation use
  `EMPTY=0` through `LOCKED=6`; the old M5 support specification used
  incompatible `VALID_STUB`, `VALID_TEST`, `ACTIVE`, and `ERROR` states.
- D10 compliance gap: current M5 evidence uses `VALID=4` and `LOCKED=6`, but
  per-sample checksum validation is not implemented. Hardware transport
  evidence must not be presented as full D10 production-validation closure.

### Evidence Still Required

- Production EMIF1 storage backend integration and hardware verification.
- Full D10 checksum implementation and verification, or a separately approved
  scoped decision changing that requirement.
- Availability of the actual Google Antigravity hook SDK in any environment
  that claims the hooks are active.

### Intended Files

- Governance and project records under `.agent/00_Project/`
- `.agent/ARCHITECTURE_CONFLICT_REGISTER.md`
- `.agent/DOCUMENT_STATUS_REGISTRY.md`
- `.agent/01_Architecture/SPEC_M5_WAVE_DOWNLOAD.md`
- Operational tooling under `.agent/skills/`
- Minimum checks under `.agent/ci/`

No firmware source file is authorized for modification in this pass.

## Conflict Impact Report

### CR-006 M5 State and Validation Evidence

- The canonical page-state contract is now `EMPTY=0` through `LOCKED=6`.
- `PageState=6` is explicitly `LOCKED`.
- `VALID_STUB`, `VALID_TEST`, `ACTIVE`, and `ERROR` are removed as production
  state identifiers.
- `WAVE_BURST_BEGIN (0x095B)` and the complete 4096-sample verified sequence
  are documented as Legacy register traffic.
- Current hardware evidence is scoped to M5 transport and lifecycle behavior.
  It is not proof of full D10 checksum compliance.

### Architecture Impact

No frozen production architecture decision changed:

- No DMA CH5 or CH6 allocation was introduced.
- Packet Protocol was not promoted.
- DDS runtime remains on CPU2.
- EMIF1 SDRAM remains the runtime waveform source.
- W25Q64 remains excluded from DDS runtime.

## Files Modified

1. `.agent/00_Project/AGENT_INDEX.md`
2. `.agent/00_Project/ASR5K_HANDOFF.md`
3. `.agent/00_Project/ASR5K_STATUS.md`
4. `.agent/00_Project/M5_GOVERNANCE_REPAIR_REPORT.md`
5. `.agent/01_Architecture/SPEC_M5_WAVE_DOWNLOAD.md`
6. `.agent/ARCHITECTURE_CONFLICT_REGISTER.md`
7. `.agent/DOCUMENT_STATUS_REGISTRY.md`
8. `.agent/skills/agent_hooks.py`
9. `.agent/skills/c2000_copilot.yaml`
10. `.agent/skills/pre_flight_check.py`
11. `.agent/skills/query_trm.py`
12. `.agent/skills/readme.md`
13. `.agent/skills/requirements.txt`
14. `.agent/ci/run_checks.py`
15. `.agent/ci/README.md`

No firmware source file was modified.

## Check Results

- PASS: Python syntax, 5 files.
- PASS: YAML parse, 2 files.
- PASS: stale links in 17 ACTIVE Markdown documents.
- PASS: frozen architecture assertions and M5 state contract.
- PASS: `query_driverlib.py` found `SPI_setConfig` in local C2000Ware
  `6.00.01.00`.
- PASS: preflight scanner accepted the current wave-download header.
- PASS: division scanner rejected three existing division operations in
  `spi_b_slave.c`, proving non-zero failure behavior.
- PASS: missing Antigravity SDK produces an explicit fail-closed error.
- PASS: `git diff --check`.
- EXPECTED UNAVAILABLE: PyMuPDF is not installed in the current Python
  environment. `query_trm.py --dependency-check` returns non-zero with the
  declared installation command.

## Remaining Issues

1. Install `.agent/skills/requirements.txt` in environments that require TRM
   search.
2. Install and verify the real Google Antigravity SDK before claiming hook
   enforcement is active.
3. The all-document link audit reports 71 historical/reference issues:
   70 non-portable `file:///` links and one broken relative D01 link.
4. D01-D05, D11, and older formal/milestone text still contain governed source
   conflicts.
5. Full D10 per-sample checksum implementation and hardware evidence remain
   open.
6. Production EMIF1 backend integration and CPU1-write/CPU2-read hardware
   verification remain open.
