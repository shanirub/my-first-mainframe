# ADR-006: Balance enquiry result displayed on OLED and web console

## Status
Accepted

## Context
When a BALANCE enquiry completes, MCU #5 receives JOB_RESULT containing
the account balance. A decision is needed on where to display it.

Options considered:
1. OLED only
2. Web console only
3. Both OLED and web console

## Decision
Display balance on both MCU #5 OLED and MCU #5 web console simultaneously.

## Reasoning
- OLED provides physical feedback — visible without a browser, good for demos
- Web console provides readable formatted output ($1,000.00 vs raw cents)
- The two displays serve different audiences:
  OLED = physical presence, immediate confirmation
  Web = human-readable detail, copy-paste friendly
- No technical conflict — both are updated in the same JOB_RESULT handler
- Consistent with MCU #5's role as I/O Controller (channel subsystem) —
  it owns all customer-facing output

## Consequences
- MCU #5 JOB_RESULT handler must update both OLED and serve balance
  via HTTP response when a pending web request is waiting
- OLED shows abbreviated form: "BAL: $1000.00"
- Web console shows full formatted response with account ID and timestamp
- Cents-to-dollars formatting ($1000.00) done at display layer only —
  raw cents stored and transmitted everywhere else
