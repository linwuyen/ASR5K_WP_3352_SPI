# SPIB 週邊設定 (`SPIA_SYSTEM`)

本文件概述了 SPIB 週邊的設定，它在 SysConfig (`pinmux.syscfg`) 環境中被邏輯命名為 `SPIA_SYSTEM`。

## 1. 總覽

SPIB 週邊是透過德州儀器 (Texas Instruments) 的 SysConfig 工具進行設定與初始化。其產生的輸出檔案，很可能是 `board.c` 和 `board.h`（位於像 `FLASH/syscfg` 這樣的建置資料夾中），其中包含了基於 `pinmux.syscfg` 設定的執行時期組態程式碼。

初始化由 `main.c` 中的 `Board_init()` 函數呼叫觸發。

## 2. 週邊與腳位設定

下表詳細說明了分配給 SPIB 週邊的特定 GPIO 腳位及其功能。

| 訊號 | 腳位   | SysConfig 名稱 | 備註                               |
| :--- | :----- | :------------- | :--------------------------------- |
| SOMI | GPIO63 | `spi_picoPin`  | 週邊輸入，控制器輸出 (Peripheral In, Controller Out) |
| SIMO | GPIO64 | `spi_pociPin`  | 週邊輸出，控制器輸入 (Peripheral Out, Controller In) |
| CLK  | GPIO65 | `spi_clkPin`   | 串列時脈 (Serial Clock)              |
| STE  | GPIO66 | `spi_ptePin`   | 從機傳輸致能 (Slave Transmit Enable / Chip Select) |

此腳位對應關係定義在 `pinmux.syscfg` 的 `spi1` 實例中，該實例被指派給 `SPIB` 硬體模組。

## 3. 通訊協定設定

SPIB 模組設定了以下參數：

-   **位元率 (Bit Rate)**: 50,000,000 bps (50 MHz)
-   **高速模式 (High-Speed Mode)**: 啟用
-   **模式 (Mode)**: 主機模式 (Master) (由 SysConfig 設定推斷，但在提供的片段中未明確說明)
-   **資料位元 (Data Bits)**: 8 位元 (這是驅動程式的預設值)
-   **時脈極性與相位 (Clock Polarity & Phase)**: 除非特別指定，否則通常使用預設設定。

## 4. 初始化序列

1.  **時脈閘控 (Clock Gating)**: 在 `device.c` 的啟動過程中，透過以下呼叫來啟用 SPIB 的週邊時脈：
    ```c
    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_SPIB);
    ```
2.  **腳位多工 (Pin Multiplexing)**: `Device_initGPIO()` 函數設定基本的 GPIO 組態。
3.  **週邊初始化 (Peripheral Initialization)**: 由 SysConfig 產生的 `Board_init()` 函數執行 SPIB 週邊的詳細設定，包括：
    -   設定 SPI 協定模式（主機/從機，時脈極性/相位）。
    -   設定位元率。
    -   如果使用 FIFO，則啟用它。
    -   致能 SPI 模組。

## 5. 來源組態 (`pinmux.syscfg`)

此設定來自 `pinmux.syscfg` 中的 `spi1` 實例：

```javascript
// Instance definition
const spi1 = spi.addInstance();

// Configuration
spi1.$name                   = "SPIA_SYSTEM";
spi1.useHSMode               = true;
spi1.bitRate                 = 50000000;
spi1.spi.$assign             = "SPIB";
spi1.spi.spi_picoPin.$assign = "GPIO63";
spi1.spi.spi_pociPin.$assign = "GPIO64";
spi1.spi.spi_clkPin.$assign  = "GPIO65";
spi1.spi.spi_ptePin.$assign  = "GPIO66";
```

**注意**: 在 `pinmux.syscfg` 中存在一個可能引起混淆的命名慣例。名為 `SPIA_SYSTEM` 的實例使用了 **SPIB** 週邊，而名為 `SPIB_EXTFLASH` 的實例則使用了 **SPIA** 週邊。必須注意區分邏輯名稱與硬體模組。

---

## 6. 功能設計規劃

本章節規劃一個基於輪詢 (Polling) 機制的 SPI Master 驅動程式。

### 6.1 設計目標

-   實現一個 SPI Master 驅動程式，用於與從機設備進行通訊。
-   採用輪詢方式檢查通訊狀態，不使用中斷 (Interrupt)。
-   驅動程式應提供一個簡單的介面，用於傳送和接收單一資料字元 (word)。

### 6.2 函式原型 (Prototype)

將設計一個核心函式來處理 SPI 的讀寫操作。

```c
/**
 * @brief Transmits and receives a single word via SPI.
 *
 * This function sends a word to the SPI slave device and waits for a word
 * to be received in return. It operates in a blocking (polling) manner.
 *
 * @param base The base address of the SPI peripheral.
 * @param data The data word (e.g., 8-bit or 16-bit) to be transmitted.
 * @return The data word received from the slave device.
 */
uint16_t SPI_transmitReceiveWord(uint32_t base, uint16_t data);
```

### 6.3 執行流程

`SPI_transmitReceiveWord` 函式的內部執行步驟如下：

1.  **檢查傳送緩衝區**: 使用輪詢方式持續檢查 SPI 狀態暫存器，直到傳送 FIFO (TX FIFO) 或傳送緩衝區不是滿的狀態。這可以透過檢查 `SPI_FLAG_TXFF` 旗標來完成。
    -   `while(SPI_getInterruptStatus(base) & SPI_FLAG_TXFF)`
2.  **寫入資料**: 將 `data` 參數寫入 SPI 的傳送緩衝區暫存器 (`SPITXBUF`)。
    -   `SPI_writeDataBlocking(base, data)`
3.  **等待接收資料**: 使用輪詢方式持續檢查 SPI 狀態暫存器，直到接收 FIFO (RX FIFO) 或接收緩衝區中有新資料。這可以透過檢查 `SPI_FLAG_RXFF` 旗標來完成。
    -   `while((SPI_getInterruptStatus(base) & SPI_FLAG_RXFF) == 0)`
4.  **讀取資料**: 從 SPI 的接收緩衝區暫存器 (`SPIRXBUF`) 讀取接收到的資料。
    -   `uint16_t receivedData = SPI_readDataBlocking(base)`
5.  **回傳資料**: 回傳從 `SPIRXBUF` 讀取到的值。

### 6.4 DriverLib API 使用

此設計將依賴現有的 `driverlib.h` 中的函式，主要包括：

-   `SPI_writeDataBlocking(base, data)`: 將資料寫入傳送緩衝區（此函式內部已包含輪詢等待 TX 緩衝區可用的邏輯）。
-   `SPI_readDataBlocking(base)`: 從接收緩衝區讀取資料（此函式內部已包含輪uin詢等待 RX 緩衝區有資料的邏輯）。

或者，也可以直接操作暫存器並手動檢查旗標：

-   `SPI_writeDataNonBlocking(base, data)`
-   `SPI_isBusy(base)`
-   `SPI_readDataNonBlocking(base)`
-   `SPI_getInterruptStatus(base)` 來檢查 `SPI_FLAG_TXFF` 和 `SPI_FLAG_RXFF`。

考量到簡易性，優先使用 `SPI_writeDataBlocking` 和 `SPI_readDataBlocking` 可能是較佳的起點。
