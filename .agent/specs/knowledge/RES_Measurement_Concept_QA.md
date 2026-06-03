---
Category: Research & Concepts
Status: Verified
Related Files: [RES_Legacy_28377_Logic.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/.agent/specs/knowledge/RES_Legacy_28377_Logic.md)
---

# 逆變器量測概念與術語解析 (Q&A)


本文件整理了關於逆變器輸出量測（VO/IO）的核心概念，包含 RMS 計算原理、硬體命名慣例以及物理意義。

---

## 1. 瞬時值與 RMS 的關係

**Q: 從 ADC 讀回來的原始數據 (CHA/CHB) 跟我們看到的 RMS 電壓有什麼關係？**

**A:** ADC 讀回的是「瞬時值」（Instantaneous Value），代表某一微秒下的電壓/電流大小。因為逆變器輸出的是交流電（正弦波），瞬時值會不斷變動且正負交替。

**RMS（均方根值）** 則是這群瞬時值的「有效能量」代表。計算過程如下：
1.  **Square (平方)**：將每個取樣點平方，消除負號。
2.  **Mean (平均)**：將一個完整週期內的平方值加總後除以點數。
3.  **Root (開根號)**：最後開根號還原單位。

這就是為什麼在程式碼中會看到 `V_square_sum` 累加後再執行 `sqrt()` 的原因。

---

## 2. 術語解析：LM-NM 與 CS

**Q: 為什麼 (LM-NM) 代表輸出電壓？**

**A:** 這是電力電子電路設計中的物理命名慣例：
*   **L (Line)**：火線。
*   **N (Neutral)**：中性線（零線）。
*   **M (Monitor/Measure)**：代表這是量測/監測點。
*   **LM-NM**：即量測「火線與中性線之間的電位差」。

在單相逆變器中，負載是接在 L 與 N 之間，因此 L 與 N 之間的差模電壓（Differential Voltage）就是**輸出電壓 (VO)**。

**Q: 什麼是 CS？**

**A:** **CS (Current Sense)** 指的是「電流感測」。這通常是透過比流器（CT）或分流器（Shunt）將輸出電流轉化為 ADC 可讀取的微小電壓訊號。對應到舊專案中，就是 `ADS8353` 的 **Channel B**。

---

## 3. 系統控制中的角色

*   **VO (LM-NM)**：用於**電壓閉迴路控制**。系統必須知道現在的輸出電壓，才能決定 PWM 的占空比（Duty Cycle）要增加還是減少，以維持穩定的輸出。
*   **IO (CS)**：用於**電流限流與過載保護**。當系統發現 IO 的 RMS 值或峰值超過安全閾值時，會立即關閉 PWM 以保護硬體。

---

## 4. 技術紀錄總結

透過理解這些術語與數學關係，開發者可以清楚地在程式碼中定位：
1.  **數據源**：ADC 暫存器 (SpiaRegs.SPIRXBUF)。
2.  **處理層**：CLA (平方累加)。
3.  **顯示層**：CPU (開根號、Offset 補償、傳送至上位機)。
