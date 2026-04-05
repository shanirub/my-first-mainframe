# ADR-004: I2C buffer expanded to 256 bytes

## Status
Accepted

## Context
Default Arduino Wire I2C buffer is 128 bytes. Human-readable JSON messages
with full field names approach this limit quickly. A typical JOB_SUBMIT
message is ~120 bytes uncompressed.

Options considered:
1. Keep 128 bytes, use short field names ("f", "t", "p")
2. Expand buffer via build flag
3. Implement message compression (zlib/miniz)
4. Switch to binary protocol now

## Decision
Expand I2C buffer to 256 bytes via build flag in all platformio.ini files:
    -D I2C_BUFFER_LENGTH=256

## Reasoning
- ESP32-C3 has 320KB RAM — 256 byte buffer is negligible overhead
- Human-readable JSON is essential for debugging in Phases 1–4
- Compression saves only 15–20 bytes on small messages — not worth complexity
- Binary protocol is a Phase 5 optimisation, not needed now
- Short field names reduce readability without solving the root constraint
- 256 bytes is sufficient for all planned message types with room to spare

## Consequences
- All 5 MCU platformio.ini files must include -D I2C_BUFFER_LENGTH=256
- Maximum message size is now 256 bytes — enforced by SharedBus.send()
- PulseView captures will show longer transmission windows — still fine at 400kHz
- Binary protocol remains a valid Phase 5 optimisation if needed
