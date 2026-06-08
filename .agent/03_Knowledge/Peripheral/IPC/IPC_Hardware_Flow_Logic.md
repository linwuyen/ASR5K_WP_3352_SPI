# IPC 通訊硬體流轉與邏輯詳解 (Hardware Flow Detail)
---

## 1. 核心組件 (Physical Components)

*   **IPC Command Registers (實體命令暫存器)**：
    *   **定義**：專用的 32-bit 硬體暫存器 (`IPCSENDCOM`, `ADDR`, `DATA`)。
    *   **特性**：**物理上繞過 RAM**，直接透過內部外設匯流排傳輸，速度最快且不佔用記憶體。
*   **Message RAM (專用共享記憶體)**：
    *   **定義**：位於 `0x3A000` (CPU1->2) 或 `0x3B000` (CPU2->1) 的 2KB SRAM。
    *   **特性**：用於存放持續更新的數據流 (B 類)，不需旗標通知。
*   **IPC Flag (旗標/門鈴)**：
    *   **功能**：點亮 Flag 位元使對端產生中斷或狀態變化。
*   **IPC Register (暫存器控制)**：
    *   `IPCSET` / `IPCCLR` / `IPCACK`：主動點火、清除或簽收旗標。

---

## 2. A 類模式：指令信箱流程 (Instruction Mailbox Flow)
用於發送指令（如 `IPC_CMD_SET`）。**利用硬體暫存器實現零內存開銷傳輸。**

```mermaid
sequenceDiagram
    participant Sender as 「發送核」(CPU1)
    participant REG as 「硬體命令暫存器」
    participant Receiver as 「接收核」(CPU2)

    Note over Sender: 1. 檢查 Flag 0 是否為 0? (isFlagBusy)
    Sender->>REG: 2. 寫入 IPCSEND COM/ADDR/DATA (IPC_sendCommand)
    Note over REG: 硬體觸發 Flag 0 並通知對端
    Note over Receiver: 3. 輪詢/中斷發現 Flag 0 亮了!
    Receiver->>REG: 4. 讀取 IPCRECV COM/ADDR/DATA (IPC_readCommand)
    Note over Receiver: 執行 Handler (不須存取 RAM)
    Receiver->>REG: 5. 處理完畢，簽收 Flag 0 (ackFlagRtoL)
    Note over REG: 硬體旗標 Bit 0 變為 0 (Sender 可發下一筆)
```

---

## 3. B 類模式：大容量數據流流程 (Data Streaming)
用於 10us ISR 高頻遙測。**利用 Message RAM 實現即時共享。**

```mermaid
sequenceDiagram
    participant Sender as 「發送核」(CPU1)
    participant RAM as 「MSGRAM stPayload」
    participant Receiver as 「接收核」(CPU2)

    Note over Sender: 10us ISR 觸發
    Sender->>RAM: 直接覆寫 stPayload.f32Vin (Direct Register Map)
    Note over RAM: 內存數據更新 (<0.05us)
    Note over Receiver: 隨時需要數據時...
    Receiver->>RAM: 直接讀取 stPayload 最新值
    Note over Receiver: 獲取最新遙測，不需中斷或旗標通知
```

## 結論

*   **A 類 (Command)** 是「實體掛號信」，利用硬體暫存器直接對傳，確保指令抵達且不經 RAM 搬運。
*   **B 類 (Streaming)** 是「即時共享窗」，利用專用 RAM 區段展示數據，實現真正的數據層級解耦。
