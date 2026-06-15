# ASR5K Top 10 Hardware Tests

Status: REFERENCE_ONLY  
Updated: 2026-06-15

Run these in order. Stop and diagnose the first failure because later tests
depend on the earlier transport layers.

| Priority | Test | Action | Pass condition |
|---:|---|---|---|
| 1 | Build and governance gate | Run CI plus CPU1 RAM and FLASH builds. | All commands return 0 and both `.out` files exist. |
| 2 | Cold boot health | Power-cycle, load CPU1 RAM image, run, inspect module diagnostics. | SPIA/SPIB leave INIT; health is OK; fault codes and `gSpibRxErrorFlags` are 0. |
| 3 | Single register write | Run self-test Test1 or write `0x0401 = 0x1234`. | Test1 PASS; detail `0x04471234`; DMA and parser deltas agree. |
| 4 | Single register read | Run Test2 or read `0x0400`. | Test2 PASS; detail `0x051A8A90`; no checksum/protocol fault. |
| 5 | Output toggle and protection | Run Test3, which sets and clears `0x0900`. | Output becomes ON then OFF; final state OFF; Test3 PASS. |
| 6 | Sequential block write | Run Test4 for the 16-word block. | Test4 PASS; block result `0x10110010`; no parse failure or overflow. |
| 7 | Wave metadata path | Run Test5-Test7: select page 1, write four samples, mark complete. | State moves `DOWNLOADING -> DOWNLOAD_COMPLETE`; count is 4; last address is `0x3003`; continuity is true. |
| 8 | Negative validation gate | Run Test8 on the incomplete four-sample page. | Validation is rejected; page becomes `INVALID=5`; active page is unchanged; Output remains OFF. |
| 9 | Full burst pipeline | Run Test9: select, burst 4096 samples, complete, validate, activate. | Test9 PASS; state `LOCKED=6`; sample count 4096; DMA delta `>=4108`; parse fail and all faults are 0. |
| 10 | Recovery and repeatability | Power-cycle and run Test1-Test9 three times. On a separate run, abort a burst or send invalid `0x095B` count. | Three clean 9/9 passes. Invalid/aborted burst does not activate a page; RX returns to register-frame mode after timeout/recovery. |

## Evidence to Save

For every formal run, record:

```text
Date/time:
Commit:
Image: CPU1_RAM or CPU1_FLASH
Board:
Overall status:
Completed tests:
Failed test / step / fault:
Test9 DMA done / parse ok / parse fail / DMA restart:
Page 1 sample count / last address / state / active page:
SPIA fault / SPIB fault / error flags:
Notes:
```

The minimum release regression is Priority 1, 2, 9, and 10. Priorities 3-8
remain useful because they identify the first broken layer much faster than a
Test9-only failure.
