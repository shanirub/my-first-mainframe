# Requirements — Low-Level Mainframe Simulation

## Functional Requirements

| ID | Requirement | Phase |
|----|-------------|-------|
| FR-01 | System shall accept banking commands via MCU #1 serial console | 3 |
| FR-02 | System shall process DEPOSIT and WITHDRAW transactions | 4 |
| FR-03 | System shall persist account balances across power cycles (SD card) | 3 |
| FR-04 | System shall maintain a transaction log separate from account data | 3 |
| FR-05 | System shall recover incomplete transactions after power loss (crash recovery) | 5 |
| FR-06 | System shall prioritise jobs as HIGH / MEDIUM / LOW | 4 |
| FR-07 | System shall detect and report subsystem failures via HEARTBEAT | 4 |
| FR-08 | System shall display real-time subsystem state on each OLED | 3 |
| FR-09 | System shall reject transactions with insufficient funds | 4 |
| FR-10 | System shall support atomic TRANSFER between accounts (two-phase commit) | 5 |
| FR-11 | MCU #5 shall host a web console for submitting transactions and checking balances | 3 |
| FR-12 | System shall support BALANCE enquiry — read account balance without modifying it | 3 |

## Non-Functional Requirements

| ID | Requirement | Phase |
|----|-------------|-------|
| NFR-01 | Transaction end-to-end latency shall be under 500ms under normal load | 4 |
| NFR-02 | Job queue shall accept and process 50 sequential transactions without dropping any | 5 |
| NFR-03 | I2C messages shall not exceed 256 bytes (expanded buffer via build flag) | 2 |
| NFR-04 | Each MCU shall respond to HEARTBEAT within 1 second | 4 |
| NFR-05 | System shall operate on 5V USB power, total draw under 1A per MCU | 1 |
| NFR-06 | All inter-MCU messages shall be human-readable JSON in Phases 1–4 | 2 |

## Out of Scope (current version)

- RFID card reader integration
- Binary message protocol optimisation (Phase 5 candidate)
- Message encryption
- Web dashboard (MCU #1 WiFi)
- Real network connectivity

## Definition of Done — per requirement

A requirement is done when:
1. A test scenario exists that exercises it
2. The test passes on real hardware
3. Pass/fail result is documented in devlog.md
4. No regressions in previously passing requirements
