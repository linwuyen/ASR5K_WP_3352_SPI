---
trigger: always_on
---

# ASR5K Context

Version: 0.1

---

# Project Purpose

ASR5K-Lite is a simplified learning and development platform derived from the production ASR5K system.

Goals:

* Understand ASR5K architecture
* Practice firmware architecture
* Verify communication paths
* Verify DMA architecture
* Verify IPC architecture
* Verify DDS architecture
* Build a reusable embedded firmware framework

Non-Goals:

* Full product functionality
* Complete power control implementation
* Production-grade feature set

---

# Hardware Platform

## MCU

TI F28388D

Features:

* CPU1 (C28x)
* CPU2 (C28x)
* CM (Cortex-M4)

---

## ASR5K-Lite Configuration

Current Lite Configuration:

CPU1 : Enabled

CPU2 : Enabled

CM : Disabled

Reason:

Reduce complexity during bring-up.

---

# System Architecture

## CPU1 Responsibilities

CPU1 is responsible for:

* External communication
* Command processing
* Register access
* Packet parsing
* Dispatcher
* System management

CPU1 should not generate DDS waveforms.

---

## CPU2 Responsibilities

CPU2 is responsible for:

* DDS runtime
* Wave generation
* PWM update
* Real-time execution

CPU2 should not handle external communication.

---

## CM Responsibilities

Current Status:

Disabled

Future Possibilities:

* Ethernet
* TCP/IP
* Web Interface
* Diagnostics

Not part of Lite v0.1.

---

# Communication Architecture

## Lite Version

SCI
→ Line Buffer
→ Parser
→ Dispatcher
→ Action

Purpose:

Replace AM3352 during development.

---

## Production Version

AM3352
→ SPI
→ DMA
→ RX Buffer
→ Parser
→ Dispatcher
→ Action

This data flow must remain compatible.

---

# Data Flow Model

All features should be analyzed using:

Source
→ Interface
→ Buffer
→ Parser
→ Dispatcher
→ Action

Example:

SCI
→ RX FIFO
→ Line Buffer
→ Parser
→ Dispatcher
→ Command Handler

---

# Command Flow

Command Input

↓

Parser

↓

Command Structure

↓

Dispatcher

↓

Target Module

↓

Execution

---

# Dispatcher Responsibilities

Dispatcher shall:

* Decode command type
* Route command
* Call target module

Dispatcher shall NOT:

* Perform hardware actions
* Contain protocol logic
* Generate waveforms

---

# Parser Responsibilities

Parser shall:

* Validate input format
* Decode command fields
* Build internal command structure

Parser shall NOT:

* Access hardware
* Execute actions

---

# DMA Architecture

DMA is used for:

* SPI Receive
* SPI Transmit
* High-speed data movement

DMA should move data only.

DMA should NOT:

* Parse packets
* Interpret commands

---

# SPI Architecture

Production Architecture:

AM3352
→ SPIB
→ RX FIFO
→ DMA
→ RX Buffer

SPI Protocol Compatibility is critical.

Packet format must not be modified without approval.

---

# IPC Architecture

CPU1
↔ IPC
↔ CPU2

IPC Responsibilities:

* Command transfer
* Status transfer
* Event notification

IPC shall not contain business logic.

---

# DDS Architecture

CPU2 owns DDS runtime.

Responsibilities:

* Frequency control
* Phase control
* Amplitude control
* Wave generation

DDS output should remain independent from communication modules.

---

# Memory Ownership

## CPU1

Owns:

* SCI Buffers
* SPI Buffers
* Parser Data
* Dispatcher Data

---

## CPU2

Owns:

* DDS Data
* Wave Tables
* Runtime Data

---

## Shared Memory

Used for:

* IPC Messages
* Shared Status

Ownership must be clearly defined.

---

# Project Structure

ASR5K-Lite/

cpu1/

* parser
* dispatcher
* communication

cpu2/

* dds
* runtime

common/

* shared definitions
* protocol definitions

test/

* smoke tests
* integration tests

docs/

* project documentation

---

# Development Milestones

## M1

SCI Bring-up

Goal:

SCI
→ Line Buffer
→ Parser

---

## M2

Dispatcher Bring-up

Goal:

Parser
→ Dispatcher

---

## M3

CPU2 IPC Bring-up

Goal:

CPU1
→ IPC
→ CPU2

---

## M4

DDS Bring-up

Goal:

CPU2 DDS Runtime

---

## M5

SPI DMA Bring-up

Goal:

SPI
→ DMA
→ Buffer

---

## M6

Production Architecture Validation

Goal:

AM3352
→ SPI
→ DMA
→ Parser
→ Dispatcher

---

# Design Philosophy

The system should be:

* Understandable
* Testable
* Modular
* Maintainable

Priority:

Working System

>

Clean Architecture

>

Optimization

Make the current system work first.

Optimize later.

---

# Document Authority Hierarchy

When analyzing this workspace and its metadata inside `.agent/`, refer to [ARCHITECTURE_AUTHORITY.md](file:///c:/Users/roger_lin/Documents/GitHub/ASR5K_GITLAB_GW/ASR5K_V2_Function/WP_3352_SPI/.agent/00_Project/ARCHITECTURE_AUTHORITY.md) for document roles.

* **Tier 1 (Rules)**: `rules/ASR5K_Context.md`, `rules/rules.md`. Highest authority.
* **Tier 2 (Specifications)**: Active specs like `SPEC_FIRMWARE_ARCH.md`, `03_Knowledge/Peripheral/DMA/CPU1_ARCH_DESIGN.MD`. Contains authoritative design contracts.
* **Tier 3 (Milestones/Prototypes)**: Milestone closed reports (e.g. `02_Milestones/M2_FIFO.md`). **These represent historical checkpoint logs or experimental utilities and are NOT active system specifications.**
* **Tier 4 (Research/References)**: Exploratory research notes and static PDFs.

AI agents MUST NOT mix up Tier 3/4 documents with Tier 1/2 specifications.

