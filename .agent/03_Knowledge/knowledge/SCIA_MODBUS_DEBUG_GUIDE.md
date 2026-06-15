# SCIA Modbus 除錯操作指南

狀態：`REFERENCE_ONLY`  
更新日期：2026-06-15  
適用專案：`Emu_3352_SPI`

## 1. 目前韌體設定

| 項目 | 設定 |
|---|---|
| MCU 介面 | `SCIA_BASE` |
| TX | GPIO135 |
| RX | GPIO136 |
| Baud rate | `115200` |
| Data bits | `8` |
| Parity | `Even` |
| Stop bits | `1` |
| Modbus Slave ID | `3` |
| 支援功能 | `0x03`、`0x06`、`0x10` |
| Holding register 範圍 | `0..46` |

目前 SCIA Modbus 僅在 `CPU1_FLASH` 組態啟用。`CPU1_RAM` 已接近記憶體容量
上限，加入完整 Modbus 路徑後無法通過 linker placement。

重要：`main()` 必須持續呼叫 `runDebug()`，才會初始化 SCIA 並處理
Modbus。若 CCS 中 `sDebug.u32Fsm` 一直停在 `_INIT_DEBUG`，代表背景工作
沒有被執行。

## 2. 正確接線

使用獨立的 3.3 V USB-to-TTL 模組，例如 CH340 或 FTDI，接到轉接板
`J1402 (MODBUS DEBUG)`：

| USB-to-TTL | J1402 |
|---|---|
| TX | Pin 3，板端 RX |
| RX | Pin 2，板端 TX |
| GND | Pin 4 或 Pin 5 |

只接 TX、RX、GND，不要連接 USB-to-TTL 的 3.3 V 電源。

不要使用標準 XDS110 JTAG 排線傳輸 UART。該排線的相關腳位可能接地，
會讓 `SCIRXBUF` 永遠收不到資料。

Windows 出現 `XDS100 Class USB Serial Port` 只代表 debug probe 內的 FTDI
serial channel 存在，不代表該 channel 已經透過 JTAG 排線連到 GPIO135/136。
本專案應使用獨立 USB-to-TTL 接到 `J1402`。

## 3. 確認 Windows COM Port

在 PowerShell 執行：

```powershell
Get-CimInstance Win32_SerialPort |
  Select-Object DeviceID, Name
```

記下 USB-to-TTL 對應的 COM，例如 `COM4`。

首次使用 PC 工具時安裝依賴：

```powershell
python -m pip install -r `
  Emu_3352_SPI\flashapi\table_manager\requirements.txt
```

## 4. 建置與載入韌體

```powershell
C:\ti\ccs1281\ccs\utils\bin\gmake.exe `
  -C Emu_3352_SPI\CPU1_FLASH all
```

在 CCS 載入並執行：

```text
Emu_3352_SPI\CPU1_FLASH\Emu_3352_SPI.out
```

## 5. 第一筆 Modbus 讀取

在 repository 根目錄執行，將 `COM4` 換成實際 COM Port：

```powershell
python Emu_3352_SPI\flashapi\table_manager\src\ModbusMaster.py `
  --port COM4 `
  --baudrate 115200 `
  --slave 3 `
  --function 3 `
  --address 0 `
  --quantity 6
```

預期讀到：

```text
address 0 = 2423
address 1 = 4
address 2 = 6
address 3 = 16
address 4 = 21
address 5 = 2470
```

這六個值由 `initRegN()` 初始化，是最適合確認通訊是否正常的固定資料。

## 6. 常用讀取命令

讀取版本與 build date，address 14 至 15：

```powershell
python Emu_3352_SPI\flashapi\table_manager\src\ModbusMaster.py `
  --port COM4 --baudrate 115200 --slave 3 `
  --function 3 --address 14 --quantity 2
```

讀取 C28 與 CLA heartbeat，address 24 至 27：

```powershell
python Emu_3352_SPI\flashapi\table_manager\src\ModbusMaster.py `
  --port COM4 --baudrate 115200 --slave 3 `
  --function 3 --address 24 --quantity 4
```

讀取硬體測試資料，address 28 至 46：

```powershell
python Emu_3352_SPI\flashapi\table_manager\src\ModbusMaster.py `
  --port COM4 --baudrate 115200 --slave 3 `
  --function 3 --address 28 --quantity 19
```

## 7. 寫入測試

address 46 對應 `g_hwTest.u16SPIBLOOP`。寫入前先記錄原值：

```powershell
python Emu_3352_SPI\flashapi\table_manager\src\ModbusMaster.py `
  --port COM4 --baudrate 115200 --slave 3 `
  --function 6 --address 46 --value 1
```

再讀回確認：

```powershell
python Emu_3352_SPI\flashapi\table_manager\src\ModbusMaster.py `
  --port COM4 --baudrate 115200 --slave 3 `
  --function 3 --address 46 --quantity 1
```

不要隨意寫入 address 17 至 45，這些暫存器會連動控制狀態、Flash 與 SDRAM
測試結構。

## 8. CCS 除錯觀察點

在 CCS Expressions 加入：

```text
sDebug.u32Fsm
g_sciaDebugDiag
mbcomm.evstep
mbcomm.state
mbcomm.sFiFo.pushRcnts
mbcomm.sFiFo.popRcnts
mbcomm.sFiFo.pushTcnts
mbcomm.sFiFo.popTcnts
mbcomm.rmtSlaveID
mbcomm.crc
mbcomm.amount_of_error
regMbusData
```

正常現象：

- `sDebug.u32Fsm` 從 `_INIT_DEBUG` 進入 `_RUN_MODBUS`。
- `g_sciaDebugDiag.u32InitCount` 大於 0。
- 收到請求時 `mbcomm.sFiFo.pushRcnts` 增加。
- `mbcomm.rmtSlaveID` 顯示 `3`。
- 完整封包收到後 CRC 結果為 `0`。
- 回覆資料時 `pushTcnts` 與 `popTcnts` 會增加。

`g_sciaDebugDiag` 判讀：

| 欄位 | 意義 |
|---|---|
| `u32InitCount` | SCIA 初始化次數；必須大於 0。 |
| `u32RxByteCount` | GPIO136 實際收到的 byte 數。 |
| `u32ValidFrameCount` | CRC 正確的完整 Modbus frame 數。 |
| `u32TxByteCount` | GPIO135 實際送出的 byte 數。 |
| `u32RxTimeoutCount` | 收到不完整 frame 後發生 timeout 的次數。 |

送出一筆 PC request 後：

- `RxByteCount == 0`：實體接線、接頭或 pin route 問題。
- `RxByteCount > 0`、`ValidFrameCount == 0`：baud、parity、Slave ID 或 CRC
  frame 問題。
- `ValidFrameCount > 0`、`TxByteCount == 0`：韌體 response path 問題。
- `TxByteCount > 0`、PC 仍收不到：板端 TX 接線或 PC RX 路徑問題。

## 9. 沒有回覆時依序檢查

1. 確認韌體已重新編譯、載入，且 `runDebug()` 正在執行。
2. 確認 USB TX 接板端 RX、USB RX 接板端 TX。
3. 確認雙方共地，且沒有連接 3.3 V 電源線。
4. 確認使用 `115200 8E1`，不是常見的 `8N1`。
5. 確認 Slave ID 是 `3`，不是 Python 範例常見的 `1`。
6. 關閉其他占用 COM Port 的 terminal 或 Modbus 軟體。
7. 在 CCS 檢查 `mbcomm.sFiFo.pushRcnts`：
   - 完全不增加：接線、COM Port、pinmux 或 baud/parity 問題。
   - 有增加但無回覆：Slave ID、CRC、封包長度或 address 問題。
8. 用示波器確認 GPIO136 有 RX 波形、GPIO135 有 TX 回覆波形。

## 10. 與 `spi_test all` 的關係

Modbus 和 `spi_test all` 共用 SCIA。只有在 Modbus port idle 時，開頭為
小寫 `s` 的文字命令才會被 self-test command parser 接管。一般 Modbus RTU
封包不會與此命令衝突，但測試時不要同時開啟兩個 PC 程式操作同一個 COM
Port。
