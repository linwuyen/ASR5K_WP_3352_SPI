# ASR5K Control Card - Phase 2: Firmware Functional Verification

本文件定義 ASR5K Control Card 的自動化韌體驗證項目，透過執行特定測試代碼來確認系統的高速性能。

## 1. 系統運行指示 (Heartbeat & Basic IO)
| 項目 | 檢查重點 | 實作細節 | 預期結果 | 狀態 |
| :--- | :--- | :--- | :--- | :--- |
| **F1.1** | **STAT CPU1 LED** | 控制 **STAT_CPU1 (GPIO94)** | LED1000 正常閃爍 | ✅ PASS |
| **F1.2** | **STAT CPU2/CM LED** | 控制 **GPIO92, GPIO93** | 對應 LED 正常閃爍 | ✅ PASS |
| **F1.3** | **主頻時鐘驗證** | SysConfig 頻率輸出 | 核心工作於 200MHz | ✅ PASS |
| **F1.4** | **序列埠 (SCI) 通訊** | SCIA_TX(135) / SCIA_RX(136) | Modbus Master 連線成功 | ✅ PASS |
| **F1.5** | **數位輸入 (DIN1~8)** | 讀取 GPIO152~159 | 狀態隨外部輸入變更 | ✅ PASS |
| **F1.6** | **數位輸出 (DOUT1~8)**| 切換 GPIO140~151 | 電位隨軟體控制變更 | ✅ PASS |
| **F1.7** | **I2C 控制驗證** | GPIO42/43 波形輸出 | 示波器觀測到完整 I2C 訊框 | ✅ PASS |
| **F1.9** | **內部溫度監測** | 讀取 MCU Die Temp | 數值與環境吻合 | ✅ PASS (48°C) |
| **F1.10** | **EN_SLAVE 控制** | GPIO3 輸出控制 | U2007 Enable 腳位電位正確 | ✅ PASS |

## 2. 高速通訊鏈路 (FSI Interface)
| 項目 | 檢查重點 | 檢查位置 | 預期結果 | 狀態 |
| :--- | :--- | :--- | :--- | :--- |
| **F2.1** | **FSI TCK 波形** | 量測 GPIO02 | 穩定且清晰的時鐘方波 | ⚠️ 示波器頻寬受限 (Skip) |
| **F2.2** | **FSI Link Loopback** | 50MHz, 16-word | 資料校驗 100% 正確 | ✅ PASS |
| **F2.3** | **FSI 頻寬邊界測試** | 5MHz ~ 50MHz 掃描 | 找出訊號完整性失效的頻率點 | ⏳ 待測 |

## 3. 大容量存儲驗證 (SDRAM / EMIF1)
| 項目 | 檢查重點 | 測試算法 | 預期結果 | 狀態 |
| :--- | :--- | :--- | :--- | :--- |
| **F3.1** | **EMIF CLK 波形** | 量測 GPIO30 | 100MHz 方波 | ⚠️ Skip |
| **F3.2** | **Walking 1s/0s** | 全址 0x80000000 | 無資料位短路或開路 | ✅ PASS |
| **F3.3** | **壓力讀寫測試** | 1000 次重複寫讀 | 資料完整率 100% | ✅ PASS |
| **F3.4** | **存儲可靠性 (Retention)** | 靜置 60s 後讀取 | 刷新機制在干擾下仍可保值 | ⏳ 待測 |
| **F3.5** | **高速 DMA 搬移** | 全速 Bank-to-Bank | 驗證線路抗串擾 (Crosstalk) 能力 | ⏳ 待測 |

## 4. 外部通訊轉發 (SPI-over-LVDS)
| 項目 | 檢查重點 | 測試細節 | 預期結果 | 狀態 |
| :--- | :--- | :--- | :--- | :--- |
| **F4.1** | **SPIB Echo 測試** | 與 AM3352 對傳 | 數據環回無誤 | ⚠️ Skip |
| **F4.2** | **SPIA Flash 驗證** | W25Q64 完整循環 | **ID/抹/寫/讀 測試成功** | ✅ PASS |
| **F4.3** | **SPIC AD5543 驗證** | DAC 16-bit 輸出 | 輸出電壓與 Setpoint 吻合 | ⏳ 待測 |
| **F4.4** | **SPIC LTC2353 驗證** | ADC 16-bit 讀取 | 讀取值與輸入電壓吻合 | ⏳ 待測 |

## 5. ADC/DAC 與控制防護 (ADC/DAC & Control Protection)
| 項目 | 檢查重點 | 測試算法 | 預期結果 | 狀態 |
| :--- | :--- | :--- | :--- | :--- |
| **F5.1** | **DAC/ADC 動態環回** | DAC 1.5V -> ADC 讀取 | 誤差 < 1% (實測 2068 LSB) | ✅ PASS |
| **F5.2** | **ePWM Trip-Zone** | 手動觸發 TZ 信號 | PWM 輸出立即切斷 | ⏳ 待測 |
| **F5.3** | **XINT 軟體響應** | GPIO8 觸發 XINT1 | 記錄中斷觸發時間 | ⏳ 待測 |
| **F5.4** | **PWM 靜態保護狀態** | TZ 觸發後的電位 | 確保強制為 Low 而非 Hi-Z | ⏳ 待測 |
| **F5.5** | **ADC 基準穩定性** | 高負載下的 LSB 抖動 | Jitter < 3 LSBs | ⏳ 待測 |
| **F5.6** | **ADC 線性度校準** | 0V - 3V 斜坡測試 | 建立 Gain/Offset 補償表 | ⏳ 待測 |

## 6. 多核心與系統穩定性 (Multi-core & Stability)
| 項目 | 檢查重點 | 測試算法 | 預期結果 | 狀態 |
| :--- | :--- | :--- | :--- | :--- |
| **F6.1** | **IPC 多核心通訊** | CPU1 與 CPU2 數據交換 | IPC 旗標傳遞無誤 | ⏳ 待測 |
| **F6.2** | **內部 Flash 讀寫** | User Sector 參數存取 | 斷電後資料不消失 | ⏳ 待測 |
| **F6.3** | **ADC 靜態熱飄移** | 長時間監控 ADC 零位 | 飄移 < 5 LSBs | ⏳ 待測 |
| **F6.4** | **CPU 負載監測** | 統計迴圈 Cycle Count | 36 Cycles (良好) | ✅ PASS |
| **F6.5** | **IPC 指令延遲** | CPU1 -> CPU2 LED 翻轉 | 延遲時間 < 10µs | ⏳ 待測 |
| **F6.6** | **CPU2 獨立控制權** | GPIO92 控制移交 | CPU2 可獨立操作 LED | ⏳ 待測 |

---
**備註**：
- SDRAM 壓力測試已完成，關鍵參數設定為 CAWIDTH=9 (512 Words) 且已放寬 tRFC/tRP 等時序參數以確保 100MHz 穩定度。
- 下一階段優先驗證 FSI Link (F2.2) 與 PWM 保護機制 (F5.2)。
