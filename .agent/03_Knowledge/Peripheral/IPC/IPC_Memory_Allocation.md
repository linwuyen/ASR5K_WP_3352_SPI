# F28388D 雙核通訊記憶體分配詳解 (Memory Layout)

雙核通訊內存被嚴格劃分為 **控制平面 (MSGRAM)** 與 **數據平面 (GSRAM)**。

---

## 1. 控制平面：Message RAM (MSGRAM)
每個核心各擁有一塊專屬發送區域（CPU1: 0x3A000, CPU2: 0x3B000）。

*   **IPC Command Registers (A 類模式信箱)**：
    *   **註記**：為了追求極致效能，A 類短指令已從 MSGRAM 中完全**移除**。
    *   現在透過純硬體暫存器 (`IPCSENDCOM`, `ADDR`, `DATA`) 存取，物理上完全不經過 RAM。
*   **stPayload (B 類模式)**：
    *   `f32VbusFF`：實例化於 MSGRAM 的直連浮點位置。
    *   `f32Iavg`：實例化於 MSGRAM 的直連浮點位置。
    *   `u32Status`：物理狀態字。
    *   **優化**：移除了所有 `u32Reserved` 空間，確保最小緩存佔用。

## 2. 數據平面：全域共享 RAM (GSRAM)
用於傳遞大型數組、波形數據或非即時性的大數據塊。在 F28388D 中，GSRAM 預設的 Master 權限為 CPU1，但 CPU1 可透過底層硬體暫存器 (`MEMCFG_SECT_GSx`) 開放權限給 CPU2。

目前程式碼透過以下設定，將特定區塊開放給 CPU2 存取 (Shared Access)：
```c
ALLOW_CPU2_ACCESS_GSRAM(MEMCFG_SECT_GS5 | MEMCFG_SECT_GS6 | MEMCFG_SECT_GS7 | 
                        MEMCFG_SECT_GS8 | MEMCFG_SECT_GS9 | MEMCFG_SECT_GS10 | 
                        MEMCFG_SECT_GS11 | MEMCFG_SECT_GS15);
```

*   **開放讀寫區域 (Shared to CPU2)**：包含了自 `GS5` 至 `GS11` 連續區段，以及 `GS15`。這些區塊的寫入防護已被解鎖，CPU2 可以直接對這些物理位址進行讀寫操作（例如 `IPC_CMD_GET` 或大陣列資料拋轉）。
*   **RAMGS10_11** (由 CPU1 控制): 雖然開放了 CPU2 存取，但架構上仍約定為主要存放 CPU1 的算法計算結果，供 CPU2 異步讀取。
*   **RAMGS12_13 等未列出區域** (由 CPU1 獨佔): 屬於 CPU1 私有數據區，不對外開放。如果作為指令目標位址（`u32Addr`），存取將觸發硬體 Illegal Address 保護。

---

## 3. 分配邏輯總結表

| 數據類型 | 存儲位置 | 通知機制 | 存取屬性 |
| :--- | :--- | :--- | :--- |
| **核心指令** | **實體暫存器 (Bypass RAM)** | Flag 0 (Doorbell) | **極致零延遲** |
| **高頻遙測** | MSGRAM (stPayload) | **無 (直連讀寫)** | **零開銷、即時** |
| **大容量數組** | GSRAM (RAMGSx) | Addr 指向 | 批量、異步 |

---

## 4. 記憶體完整性 (Memory Integrity)
本模組利用硬體 `EALLOW/EDIS` 保護，防止非預期的 GSRAM 指令寫入。
所有的數據結構都在 `IPC_ShareRAM.h` 中有嚴格的大小斷言。
