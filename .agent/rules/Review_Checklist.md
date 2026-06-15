---
trigger: always_on
---

# Firmware Review Checklist

Version: 1.0

---

# Review Philosophy

Review objectives:

1. Correctness
2. Safety
3. Reliability
4. Maintainability
5. Compatibility

Priority:

Correctness

>

Safety

>

Reliability

>

Maintainability

>

Optimization

Never sacrifice correctness for optimization.

---

# Review Summary

Before detailed review provide:

## Overall Result

PASS

or

PASS WITH ISSUES

or

FAIL

---

## Risk Level

LOW

MEDIUM

HIGH

CRITICAL

---

## Key Findings

List major findings.

---

# Architecture Review

## Requirements

□ Requirement is clearly understood

□ Requirement is fully implemented

□ No missing functionality

□ No unintended functionality

---

## Architecture Consistency

□ Existing architecture preserved

□ Existing data flow preserved

□ Existing module boundaries preserved

□ Existing protocol preserved

□ Existing state machine preserved

---

## Scope Control

□ Only requested files modified

□ No unrelated modules modified

□ No unnecessary abstraction added

□ No future features added

---

# Signal Flow Review

Verify signal path:

Source
→ Interface
→ Buffer
→ Parser
→ Dispatcher
→ Action

Check:

□ Data source correct

□ Data destination correct

□ Data ownership clear

□ No missing stage

□ No duplicated stage

---

# Correctness Review

## Logic

□ Logic matches requirement

□ All branches handled

□ Boundary conditions handled

□ Error paths handled

□ Invalid input handled

---

## State Machine

□ Valid transitions only

□ No dead states

□ No unreachable states

□ Recovery path exists

---

## Protocol

□ Packet format correct

□ Endianness correct

□ Field alignment correct

□ Length validation present

□ CRC/checksum handled if required

---

# Memory Review

## Buffers

□ No buffer overflow

□ No buffer underflow

□ Array bounds protected

□ Length checks present

---

## Pointers

□ Null pointer checked

□ Pointer lifetime valid

□ No dangling pointer

□ No invalid cast

---

## Ownership

□ Ownership defined

□ Shared memory protected

□ IPC ownership clear

□ DMA ownership clear

---

# DMA Review

## Configuration

□ Source address correct

□ Destination address correct

□ Transfer size correct

□ Trigger source correct

□ Interrupt configuration correct

---

## Runtime

□ Transfer completion handled

□ Timeout considered

□ Error recovery exists

□ Buffer ownership defined

---

# Interrupt Review

## ISR Design

□ ISR purpose clear

□ ISR execution time reasonable

□ ISR does not block

□ ISR does not allocate memory

□ ISR does not perform heavy computation

---

## Shared Resources

□ Volatile used where required

□ Shared variables protected

□ Race conditions considered

□ Atomic access considered

---

# Timing Review

## Real-Time Behavior

□ Timing requirements identified

□ Latency acceptable

□ Jitter acceptable

□ No unnecessary delays

---

## Blocking Operations

□ No blocking call inside ISR

□ Timeout exists where needed

□ Deadlock impossible

□ Infinite loop avoided

---

# IPC Review

## Message Format

□ Sender identified

□ Receiver identified

□ Message structure defined

□ Version compatibility preserved

---

## Synchronization

□ Mailbox handling correct

□ Message ownership clear

□ No race condition

□ Error handling exists

---

# SPI Review

## Hardware

□ SPI mode correct

□ Clock polarity correct

□ Clock phase correct

□ Bit order correct

---

## Data Transfer

□ FIFO handling correct

□ DMA handling correct

□ Packet alignment correct

□ Error recovery exists

---

# SCI/UART Review

## RX Path

□ FIFO handling correct

□ Buffer handling correct

□ Overflow handled

□ Invalid data handled

---

## TX Path

□ Buffer handling correct

□ Completion handling correct

□ Timeout considered

---

# Resource Review

## RAM

□ RAM usage acceptable

□ Stack usage acceptable

□ No unnecessary allocation

---

## Flash

□ Flash usage acceptable

□ No duplicated code

□ No dead code

---

## CPU

□ CPU load acceptable

□ ISR load acceptable

□ Polling justified

---

# Error Handling Review

## Detection

□ Errors detectable

□ Error source identifiable

---

## Recovery

□ Recovery path exists

□ Recovery tested

□ Safe fallback exists

---

# Documentation Review

□ Code matches design document

□ Comments accurate

□ Assumptions documented

□ Limitations documented

---

# Testability Review

## Unit Test

□ Logic testable

□ Edge cases testable

□ Error paths testable

---

## Integration Test

□ Module interactions testable

□ Communication paths testable

□ Hardware validation possible

---

# Maintainability Review

## Readability

□ Naming consistent

□ Function size reasonable

□ Logic understandable

□ Magic numbers minimized

---

## Modularity

□ Responsibility clear

□ Coupling acceptable

□ Reuse possible

---

# Compatibility Review

## Existing System

□ Existing interfaces preserved

□ Existing protocol preserved

□ Existing behavior preserved

□ Existing timing preserved

---

# Final Decision

PASS

PASS WITH ISSUES

FAIL

---

# Findings Format

Use:

[CRITICAL]
System failure possible.

[HIGH]
Major issue.

[MEDIUM]
Should be fixed.

[LOW]
Improvement suggestion.

[INFO]
Observation only.

---

# Review Principle

A working system is preferred over a perfect design.

Do not recommend refactoring unless:

1. There is a real defect
2. There is a measurable benefit
3. The change risk is acceptable
