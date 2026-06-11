# D07_DDS_RUNTIME_MANAGER.md v0.1

# D07_DDS_RUNTIME_MANAGER.md v0.1 設計目標

## Runtime Frequency

```
1Hz ~ 2kHz
(目前已知常用 1~2kHz)
```

---

## Runtime Tick

```
100kHz
10us ISR
```

---

## Wave Table

```
Storage:
EMIF1 SDRAM

Page Size:
4096 Samples

Sample Width:
16-bit
```

---

## DDS Algorithm

```
Phase Accumulator
+
Phase Step
```

架構：

```
phase_acc += phase_step

index = phase_acc >> 20

sample = WaveTable[index]
```

---

## Runtime Source

```
Wave Runtime Source
=
EMIF1 SDRAM
```

---

## Runtime Access Method

```
CPU Direct Read
```

第一版不考慮：

```
DMA Preload
DMA Streaming
GSRAM Active Buffer
```

---

## Runtime Path

```
EMIF1 SDRAM
↓
CPU Read Sample
↓
Amplitude
↓
Offset
↓
Output Limit
↓
SPIC
↓
AD5543
```

---

## GSRAM Role

```
SPIB RX Ping/Pong Buffer

SPIB TX Ping/Pong Buffer

Runtime Scratch Buffer
```

不作為：

```
Wave Cache

ActiveWaveBuffer
```

---

## DMA Ownership

Runtime：

```
CH1
SPIC TX

CH2
SPIC RX

CH3
SPIB RX

CH4
SPIB TX
```

---

## Future Optimization

若未來量測：

```
EMIF1 Access Latency
```

不足以支援：

```
100kHz Runtime
```

則評估：

```
D07 v0.2

DMA Preload

or

Wave Cache Architecture
```

---

我覺得這樣最大的好處是：

```
先讓系統跑起來
```

而不是一開始就掉進：

```
DMA
FIFO
Double Buffer
Streaming Engine
```

的複雜度。

而且這也符合你主管一直強調的思路：

```
Architecture First
Implementation Second
```

先把 D07 v0.1 建出來，後面真的測到 EMIF 慢，再演進到 v0.2。這樣風險最低。