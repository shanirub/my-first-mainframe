# 🧪 Test Scenarios

Six concrete scenarios drive development and validation of the system.

---

## Scenario 1 — Simple Deposit

**Command:** `DEPOSIT 12345678 100.00`

**Goal:** Verify basic transaction flow end-to-end — from I/O Controller through Transaction Processor to Database Controller and back.

**Learning focus:** Message routing, data validation, persistent storage.

---

## Scenario 2 — Insufficient Funds

**Command:** `WITHDRAW 87654321 100.00`

**Goal:** Demonstrate error handling when account balance is insufficient.

**Learning focus:** Business logic validation, error propagation across subsystems.

---

## Scenario 3 — Account Transfer

**Command:** `TRANSFER 12345678 87654321 50.00`

**Goal:** Multi-account atomic transaction — debit one account and credit another as a single indivisible operation.

**Learning focus:** Transaction atomicity, locking mechanisms.

---

## Scenario 4 — Priority Scheduling

**Goal:** Submit jobs at different priority levels and observe HIGH priority jobs preempting LOW priority queue entries.

**Learning focus:** Priority scheduling algorithms, queue management.

---

## Scenario 5 — Load Test

**Command:** `LOADTEST 50`

**Goal:** Submit 50 rapid transactions and observe system behaviour under load.

**Learning focus:** System bottlenecks, message queueing, performance characteristics.

---

## Scenario 6 — Subsystem Failure

**Setup:** Physically disconnect the Database Controller during operation.

**Goal:** Observe how the system handles a missing subsystem — timeouts, error propagation, health monitoring.

**Learning focus:** Failure modes, timeout handling, health monitoring.
