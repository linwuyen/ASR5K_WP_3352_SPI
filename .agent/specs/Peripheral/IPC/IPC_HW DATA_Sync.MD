
# 4 種硬體資料傳輸與同步機制


##  1. IPC Flags & Interrupts (事件旗標與中斷)
硬體層次：32-bit 的 IPCSET、IPCCLR (發送端) 與 IPCSTS、IPCACK (接收端) 暫存器。
傳輸機制：純硬體信號 (Event signaling)，不攜帶 Payload。其中 Flag 03 直接連線至 Remote CPU 的 PIE (Peripheral Interrupt Expansion) 中斷控制器，Flag 431 僅供 Polling 輪詢。
架構特性：極低延遲的 Non-blocking 同步。
最佳實踐場景：跨核 FSM 狀態切換通知、保護機制 (Fault 觸發) 或配合 Message RAM 達成非同步的 Check-Write-Set Pipeline 握手。

*  **IPC Flags & Interrupts**
    *   **開機交握**：在 `IPC_Init()` 中調用 `IPC_sync(..., IPC_FLAG31)`，利用硬體 Flag 進行 CPU1/CPU2 Barrier 同步。
    *   **無鎖輪詢**：在 `IPC_Rx_PollTask()` 中透過 `IPC_isFlagBusyRtoL(..., IPC_FLAG0)` 進行非阻塞偵測，並利用 `IPC_ackFlagRtoL()` 清除狀態，嚴禁使用 `while()` 等待。
## 2. IPC Command Registers (命令與資料暫存器)
硬體層次：IPCSENDCOM, IPCSENDADDR, IPCSENDDATA (3x 32-bit 暫存器群) 以及 IPCLOCALREPLY。
傳輸機制：藉由內部周邊匯流排 (Peripheral Bus) 直接存取，物理上完全不經過 RAM。
架構特性：寫入速度最快，無需擔心 RAM 的存取衝突或 Cache 一致性問題。
最佳實踐場景：適合高頻率的短指令傳輸 (如：控制命令 0x01、特定變數 Pointer 位置拋轉)。

*   **IPC Command Registers**
    *   **A類短指令 (信箱)**：在 `IPC_Core.c` 的 `IPC_Tx_ProcessTask()` 與 `IPC_Core_SendPacket()` 中，改為呼叫 Driverlib `IPC_sendCommand()` 直接對**實體匯流排暫存器**寫入 (`IPCSENDCOM`, `ADDR`, `DATA`)。
    *   **極致扁平化**：為了對齊硬體只具備一個 32-bit Data 暫存器的物理限制，我們刻意移除 `IPC_MsgPacket_t` 與 Handler 中的 `u32Data2` 冗餘引數，徹底消除額外 LDR/STR 指令。

## 3. Message RAMs (專用共享訊息記憶體)
硬體層次：兩塊絕對獨立且單向的 2KB 單埠 SRAM 區塊 (CPU1_TO_CPU2_MSGRAM 位於 0x03FC00，CPU2_TO_CPU1 位於 0x03F800)。
傳輸機制：物理層面上，發送端 (及所屬 DMA) 具備 R/W 權限，接收端僅具 R 權限，硬體強制避免寫入衝突。讀寫 RAM 本身不會觸發任何 Remote CPU 事件。
架構特性：大吞吐量傳輸。若結合 DMA 可實現在背景搬運資料而不佔用 CPU Cycle。
最佳實踐場景：陣列數據、長型資料結構 (如高頻 DSP 濾波器輸出 Buffer、示波器觀測波形 Array)。通常要求嚴格的軟體定義通訊協定，寫入後必須搭配 IPC Flags 通知 Remote CPU。

*   **Message RAMs**
    *   **B類資料流 (Streaming)**：我們透過 `#pragma DATA_SECTION` 將 `IPC_SharedMemory_t` 定址於專用的硬體隔離 RAM 區塊 (如 `CPU1_TO_CPU2_MSGRAM`)。
    *   **職責分離**：此區塊已**剝離 Mailbox 職責**，現在專門用於 ASR 系統的高頻大吞吐量更新 (如 `f32Vin`, `f32Iout` 電源遙測數據) 與 `stSnapshot` (故障快照)，完全不擠佔 Command Register 頻寬。

## 4. IPC Boot-Mode Registers (開機模式暫存器)
硬體層次：IPCBOOTMODE 與 IPCBOOTSTS。
傳輸機制：特殊用途的 32-bit 暫存器，CPU1 擁有前者的 Write 權限，CPU2 擁有後者的 Write 權限。
最佳實踐場景：專門用於 Boot ROM 啟動階段。CPU1 指定 CPU2 的啟動入口 (例如 Boot to SARAM 或是 Boot to Flash)，並接收 CPU2 的狀態回報。

*   **IPC Boot-Mode Registers**
    *   **底層代管**：由 CPU1 Boot ROM 流程或 `Device_init()` 階段中的 `IPC_setBootMode` 設定，用於開機初期分配資源，無需上層業務邏輯介入。
---








