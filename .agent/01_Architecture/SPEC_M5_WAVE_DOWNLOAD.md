# ASR5K M5 Wave Download Service 規格書 (SPEC_M5_WAVE_DOWNLOAD)

版本: 4.0  
類型: Tier 2 (Active System Specification)  
維護者: Antigravity AI  

---

## 1. 總覽與目的 (Overview & Objective)

本規格書定義 Milestone 5 (M5_WAVE_DOWNLOAD_PATH) 的波形下載通道設計合約。  
本模組負責將來自 AM3352 的波形資料，經由 SPIB 接收通道 (DMA CH3 + Background Parser) 寫入至外部 EMIF1 SDRAM 中對應的 Page 區段，作為 DDS 運行時的波形來源。

---

## 2. 系統硬體與記憶體映射 (Physical Hardware & Memory Mapping)

*   **每頁波形點數**: 4096 Samples/Page
*   **每頁記憶體大小**: 8192 Bytes/Page (C28x 每字組為 16-bit，對應 4096 點)
*   **EMIF1 SDRAM address formula**:
    $$\text{byte\_addr} = \text{0x80000000} + \text{page\_id} \times \text{8192} + \text{sample\_index} \times 2$$

    *If using uint16_t pointer:*
    ```c
    uint16_t *page = (uint16_t *)(0x80000000 + page_id * 8192);
    page[sample_index] = sample;
    ```
*   **Page 5 base address**:
    $$\text{0x80000000} + 5 \times \text{0x2000} = \text{0x8000A000}$$
*   **最大支援頁數**: 256 頁 (分頁索引 `0x00` 至 `0xFF`，相容 D05 規格)
    *   *註：當前產品實際使用範圍為 `0x00` 至 `0x12` (19頁)*
*   **Command allocation (D11 協定規格)**:
    *   `0x0958` = WAVE_PAGE_SELECT (寫入範圍 `0` 至 `255`)
    *   `0x0959` = WAVE_DOWNLOAD_COMPLETE (Data = `0x0001` only)
    *   `0x0960` = WAVE_VALIDATE (觸發所選分頁之驗證)
    *   `0x0961` = WAVE_ACTIVATE (觸發所選分頁之啟用)
    *   `0x095A` = WAVE_PAGE_STATUS (讀取分頁狀態)
*   **波形串流視窗空間 (Wave Data Window)**: `0x3000` 至 `0x3FFF` (共 4096 個暫存器位址，對應偏移量 `0` 至 `4095`)

---

## 3. EARS 系統需求定義 (EARS System Requirements)

*   **[REQ-M5-001] (Event-driven)**:  
    當 `[SPI 寫入訊號寫入至 0x0958 暫存器]` 且 `[資料值小於 256]` 時，系統 `[必須]` 將當前選定的波形頁索引更新為該資料值，並回傳該資料值給主機。

*   **[REQ-M5-002] (Event-driven)**:  
    當 `[SPI 寫入訊號寫入至 0x0958 暫存器]` 且 `[資料值大於等於 256]` 時，系統 `[必須]` 拒絕更新，並將當前選定頁面設為無效狀態 (`WAVE_PAGE_INVALID = 0xFFFF`)，同時回傳 `0xFFFF`。

*   **[REQ-M5-003] (State-driven)**:  
    當 `[波形下載模式啟用 (SelectedPage != 0xFFFF)]` 且 `[SPI 寫入訊號寫入至 0x3000~0x3FFF 暫存器空間]` 時，系統 `[必須]` 將資料寫入對應的 SDRAM 記憶體中，同時依據編譯選項回傳數據：
    *   若啟用 `DIAG_SELFTEST`：從 SDRAM 讀回寫入值以進行即時比對驗證 (Read-back Echo Verification) 並回傳該值。
    *   若為 `Product Path` (預設)：直接回傳寫入數值，不進行每點讀回比對，以維護最高傳輸效能。

*   **[REQ-M5-004] (State-driven) (DIAG_COMPAT_ONLY)**:  
    當 `[波形下載模式未啟用 (SelectedPage == 0xFFFF)]` 且 `[SPI 寫入訊號寫入至 0x3000~0x3FFF]` 時，系統 `[必須]` 拒絕寫入 SDRAM，並將寫入請求交給舊有 `tryHandleBlockPath` 處理，以維持模擬器相容性。解析器本身禁止直接操作任何實體 Flash 寫入。

*   **[REQ-M5-005] (Event-driven)**:  
    當 `[SPI 寫入訊號寫入至 0x0959 暫存器且資料值為 0x0001]` 時，系統 `[必須]` 僅將當前選定分頁的狀態標記為 `DOWNLOAD_COMPLETE` (下載完成)。本操作不得直接將分頁狀態變更為 `VALID`。

*   **[REQ-M5-006] (Event-driven)**:  
    當 `[SPI 寫入訊號寫入至 0x0960 暫存器]` 時，系統 `[必須]` 對當前分頁觸發驗證。
    *   **M5 validation**:
        *   `WAVE_VALIDATE` 可被 stubbed。
        *   系統驗證程序必須至少檢查：
            *   選定分頁有效 (`SelectedPage != 0xFFFF`)
            *   已接收樣本數 `sample_count == 4096`
            *   `download_complete == true`
            *   `output_off == true` (輸出處於關閉狀態)
        *   系統禁止聲稱符合 D10 校验和規範，除非實現了逐點校驗和 (per-sample checksum) 儲存與比對。驗證 stub 通過後，分頁狀態可變更為 `VALID_STUB` 或 `VALID_TEST`，而非最終的 D10 `VALID`。

*   **[REQ-M5-007] (Event-driven)**:  
    當 `[SPI 寫入訊號寫入至 0x0961 暫存器]` 且 `[分頁狀態為 VALID_STUB 或 VALID_TEST]` 時，系統 `[必須]` 觸發分頁啟用 (Activation)。

---

## 4. 介面契約與資料結構 (Interface Contract & Data Structures)

### 4.1 常數與分頁狀態定義 (Constants & Enums)

```c
#ifndef WAVE_DOWNLOAD_H_
#define WAVE_DOWNLOAD_H_

#include <stdint.h>
#include <stdbool.h>

#define WAVE_PAGE_SELECT_ADDR       0x0958U
#define WAVE_DOWNLOAD_CTRL_ADDR     0x0959U
#define WAVE_PAGE_STATUS_ADDR       0x095AU
#define WAVE_VALIDATE_ADDR          0x0960U
#define WAVE_ACTIVATE_ADDR          0x0961U

#define WAVE_DATA_WINDOW_BASE       0x3000U
#define WAVE_DATA_WINDOW_LIMIT      0x3FFFU

#define WAVE_SAMPLES_PER_PAGE       4096U
#define WAVE_SDRAM_BASE_ADDR        0x80000000UL

#define WAVE_MAX_PAGES              256U
#define WAVE_PAGE_INVALID           0xFFFFU

/* 波形分頁下載狀態列舉 */
typedef enum {
    WAVE_PAGE_STATE_EMPTY = 0,          /* 未下載數據 */
    WAVE_PAGE_STATE_DOWNLOADING,        /* 資料傳輸中 */
    WAVE_PAGE_STATE_DOWNLOAD_COMPLETE,  /* 下載完成，等待校驗 */
    WAVE_PAGE_STATE_VALID_STUB,         /* M5 階段驗證 Stub 通過 */
    WAVE_PAGE_STATE_VALID_TEST,         /* M5 階段測試驗證通過 */
    WAVE_PAGE_STATE_ACTIVE,             /* 分頁已啟用 */
    WAVE_PAGE_STATE_ERROR               /* 校驗失敗或發生溢位錯誤 */
} ST_WAVE_PAGE_STATE;
```

### 4.2 狀態與診斷結構 (Status & Diagnostics)

```c
typedef struct {
    uint32_t u32WriteCount;         /* 累計寫入 SDRAM 的單字數 */
    uint32_t u32VerifyFailCount;    /* 僅在 DIAG_SELFTEST 下有效，累計讀寫驗證失敗次數 */
    uint16_t u16LastVerifyExpected; /* 僅在 DIAG_SELFTEST 下有效，最後一次驗證期望值 */
    uint16_t u16LastVerifyObserved; /* 僅在 DIAG_SELFTEST 下有效，最後一次驗證觀察值 */
} ST_WAVE_DOWNLOAD_DIAG;

typedef struct {
    uint16_t u16SelectedPage;                   /* 當前選定的 Page Index (0~255 或 0xFFFF) */
    uint8_t  u8PageState[WAVE_MAX_PAGES];       /* 記錄 256 個分頁的當前狀態 (ST_WAVE_PAGE_STATE) */
    uint16_t u16SampleCount[WAVE_MAX_PAGES];    /* 記錄各頁面已接收之點數 */
    bool     bDownloadComplete[WAVE_MAX_PAGES]; /* 記錄各頁面是否已下載完成 */
    ST_WAVE_DOWNLOAD_DIAG stDiag;               /* 診斷計數器 */
} ST_WAVE_DOWNLOAD;
```

### 4.3 函式原型 API (Function Prototypes)

```c
/**
 * @brief 初始化波形下載服務。
 */
void WaveDownload_Init(void);

/**
 * @brief 設定當前下載的波形頁面索引。
 */
void WaveDownload_SetPage(uint16_t u16Page);

/**
 * @brief 取得當前下載的波形頁面索引。
 */
uint16_t WaveDownload_GetPage(void);

/**
 * @brief 寫入波形資料點至當前頁面 SDRAM 中。
 */
uint16_t WaveDownload_WriteSample(uint16_t u16Offset, uint16_t u16Sample);

/**
 * @brief 標記下載完成。
 */
uint16_t WaveDownload_CompleteDownload(uint16_t u16Data);

/**
 * @brief 觸發分頁驗證。
 * @note 必須至少確認選定分頁有效、sample_count == 4096、download_complete == true 且 output_off == true。
 *       驗證成功狀態轉變為 VALID_STUB 或 VALID_TEST。
 */
uint16_t WaveDownload_ValidatePage(void);

/**
 * @brief 觸發分頁啟用。
 */
uint16_t WaveDownload_ActivatePage(void);

/**
 * @brief 取得指定分頁的當前下載狀態。
 */
uint8_t WaveDownload_GetPageState(uint16_t u16Page);

/**
 * @brief 檢查暫存器寫入是否屬於波形下載服務並執行。
 */
bool WaveDownload_HandleWrite(uint16_t u16Address, uint16_t u16Data, uint16_t *pu16Response);

#endif /* WAVE_DOWNLOAD_H_ */
```

---

## 5. 防呆限制與設計原則 (Safety Rules & Design Principles)

*   **無代碼原則**: 本階段僅定義架構合約與規格書。在合約審閱完成並獲得 Approved 前，禁止建立或修改任何 `.c` 或 `.h` 的實體程式碼。
*   **邊界保護**: 寫入 SDRAM 之前，必須進行 `u16Offset < 4096` 與 `u16SelectedPage < 256` 的硬性邊界檢查，防止寫入超出分頁區域而覆蓋 SDRAM 其他代碼或數據。
*   **生命週期管理**: 分頁資料必須經過 `DOWNLOAD_COMPLETE` 及檢驗通過轉變為 `VALID_STUB` 或 `VALID_TEST` 之後，DDS 運行時才可對其進行載入與輸出。
