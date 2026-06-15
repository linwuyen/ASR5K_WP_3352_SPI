# ASR5K SPIB 週邊硬體配置與接口規格說明書 (spib_hardware_spec.md)

本文件概述了 ASR5K 逆變器控制板上 C2000 與 AM3352 主控晶片間的 **SPIB 實體層配置**、引腳多工、阻抗匹配電阻及暫存器工作屬性。本文件為硬體對接與底層驅動運作的單一真相來源 (SSOT)。

---

## 1. 物理引腳多工與接線對照 (GPIO Configuration)

在 ASR5K 系統中，C2000（從機）與 AM3352 主控晶片（主機）之間使用高速 SPI 鏈路進行暫存器級指令收發。其物理引腳多工對應與接線關係如下：

| C2000 訊號名稱 | C2000 GPIO 腳位 | 物理接線方向 | 阻抗匹配電阻 | 功能描述 |
| :--- | :--- | :--- | :--- | :--- |
| **SOMI** (POCI) | **GPIO64** | Output (Slave Out) | `R2039` (22 Ω) | 從機輸出，主機輸入。連接至 AM3352 RX 端。 |
| **SIMO** (PICO) | **GPIO63** | Input (Slave In) | `R2037` (22 Ω) | 從機輸入，主機輸出。連接至 AM3352 TX 端。 |
| **CLK** | **GPIO65** | Input (Clock In) | `R2040` (22 Ω) | 串列時脈。由 Master (AM3352) 提供 10 MHz 物理時脈。 |
| **STE** (CS) | **GPIO66** | Input (Enable In) | `R2041` (22 Ω) | 從機片選。由 Master 驅動之主控片選致能訊號 (Active Low)。 |

### 1.1 物理阻抗匹配限制
* 各訊號線上均串接了 **`22 Ω`** 的匹配電阻 (`R2037`, `R2039`, `R2040`, `R2041`)，用以抑制在 10 MHz 高速通訊下的高頻訊號反射，確保波形品質。
* 偵錯/時序觀測引腳：**`STAT_CPU1` (GPIO 94)** 在從機發送程序中執行電平翻轉，作為量測軟體處理延遲與中斷響應的時間戳記。

> [!WARNING]
> **SysConfig 命名對照混淆警告：**
> 在 `pinmux.syscfg` 軟體設定中：
> *   邏輯實例名稱配置為 **`SPIA_SYSTEM`**，但其實體硬體分配為 **SPIB 週邊** (`spi1` 實例)。
> *   邏輯實例名稱配置為 **`SPIB_EXTFLASH`**，但其實體硬體分配為 **SPIA 週邊** (`spi0` 實例)。
>
> 核心 GPIO 初始化與多工統一由 SysConfig 產出的 `board.c` 之 `PinMux_init()` 自動處理，請勿手動編寫暫存器配置。

---

## 2. 週邊工作屬性與暫存器配置 (SPIB Peripheral Settings)

依據底層驅動實作，C2000 SPIB 從機端之暫存器運作參數配置如下：

1.  **主從模式**：**從機模式 (SPI Slave Mode)**。由外部主控晶片 AM3352 (Master) 主導 SPI 傳輸與時鐘訊號。
    *   *註：開發測試中提供 `SPI_Master` 模擬器（模擬主機行為），此時測試代碼會以 Master 模式驅動另一路 SPI 用於測試。*
2.  **通訊極性與相位**：**Mode 1** (`SPI_PROT_POL0PHA1`，時脈空閒為低，在下降沿鎖存輸入)。
3.  **資料字長**：**16-bit** (每次物理傳輸 16 位元，藉此對齊 FIFO 暫存器)。
4.  **高速模式 (High-Speed Mode)**：**啟用**，以支持高速信號完整性。
5.  **週邊時脈使能**：在啟動程序中透過 `SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_SPIB)` 開啟時脈閘。
6.  **波特率限制**：
    *   高速模式（HSMode）硬體極限設計最高可支援 50 MHz。
    *   在實際系統與 AM3352 對接時，工作波特率統一運作在 **10 MHz** (10,000,000 Hz) 以確保高頻傳輸的絕對穩定性。
7.  **FIFO 配置**：
    *   啟用 FIFO 機制：`SPI_enableFIFO()`。
    *   FIFO 中斷閾值：發送與接收 FIFO 級別設定為 2 (`SPI_FIFO_TX2` / `SPI_FIFO_RX2`)，以對齊 32-bit (即 2 個 16-bit Word) 的協定封包。

---

## 3. 底層初始化序列 (Initialization Sequence)

1.  **時脈開啟 (Clock Gating)**：
    ```c
    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_SPIB);
    ```
2.  **腳位多工 (Pin Multiplexing)**：
    `Device_initGPIO()` 函數設定基本的 GPIO 組態（由 `PinMux_init()` 統一處理）。
3.  **週邊初始化 (Peripheral Initialization)**：
    由 SysConfig 產生的 `Board_init()` 函數執行 SPIB 週邊的詳細設定：
    *   配置為從機模式（Slave Mode）。
    *   配置協議模式為 Mode 1。
    *   啟用 HSMode 高速通道與 FIFO。
    *   致能 SPIB 模組。
