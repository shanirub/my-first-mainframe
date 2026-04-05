# ADR-005: JSON as inter-MCU message format

## Status
Accepted

## Context
Raw string messages ("HELLO FROM MCU1") cannot carry structured data needed
for Phase 2+ — sender identity, message type, job IDs, payload fields.
A structured format is required.

Options considered:
1. Custom delimited string ("FROM:0x08|TO:0x09|TYPE:JOB_SUBMIT|...")
2. JSON with ArduinoJson library
3. MessagePack (binary JSON)
4. Custom binary protocol

## Decision
Use JSON encoded with ArduinoJson library for all inter-MCU messages in
Phases 2–4. Evaluate binary in Phase 5.

## Reasoning
- ArduinoJson is mature, well-documented, ESP32-compatible
- JSON is human-readable — essential for debugging with serial monitor
  and PulseView during development
- No custom parser needed — ArduinoJson handles serialisation/deserialisation
- Structured fields enable type-safe message handling
- MessagePack saves ~30% space but loses human readability — not worth it now
- Custom binary is fastest but requires custom tooling to debug

## Consequences
- ArduinoJson added to lib_deps in all platformio.ini files
- Messages must fit in 256 byte I2C buffer (see ADR-004)
- All messages share a common envelope (msgId, from, to, type)
- Payload varies by message type — defined in message_protocol.md
- Phase 5 may replace JSON with binary for performance if needed
