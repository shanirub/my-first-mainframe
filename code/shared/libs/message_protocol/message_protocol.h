#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include "shared_config.h"

// ─────────────────────────────────────────────────────────────────────────────
// Field — wire-format key constants
//
// Short string values save I2C bus space.
// Always use Field::ACCOUNT in code, never the raw string "ac".
// ─────────────────────────────────────────────────────────────────────────────
namespace Field {
    constexpr const char* MSG_ID    = "mid";
    constexpr const char* FROM      = "f";
    constexpr const char* TO        = "t";
    constexpr const char* TYPE      = "tp";
    constexpr const char* PAYLOAD   = "p";
    constexpr const char* JOB_ID    = "jid";
    constexpr const char* TXN_TYPE  = "tt";
    constexpr const char* ACCOUNT   = "ac";
    constexpr const char* AMOUNT    = "am";
    constexpr const char* BALANCE   = "bal";
    constexpr const char* NEW_BAL   = "nb";
    constexpr const char* PRIORITY  = "pr";
    constexpr const char* STATUS    = "st";
    constexpr const char* REASON    = "rs";
    constexpr const char* UPTIME    = "up";
    constexpr const char* TIMESTAMP = "ts";
    constexpr const char* CODE      = "cd";
    constexpr const char* DETAIL    = "dt";
}

// ─────────────────────────────────────────────────────────────────────────────
// MsgType — message type identifiers (uint8_t, 1 byte on wire)
// ─────────────────────────────────────────────────────────────────────────────
namespace MsgType {
    constexpr uint8_t JOB_SUBMIT     = 1;
    constexpr uint8_t JOB_DISPATCH   = 2;
    constexpr uint8_t JOB_COMPLETE   = 3;
    constexpr uint8_t JOB_RESULT     = 4;
    constexpr uint8_t DB_READ        = 5;
    constexpr uint8_t DB_READ_RESULT = 6;
    constexpr uint8_t DB_WRITE       = 7;
    constexpr uint8_t DB_WRITE_ACK   = 8;
    constexpr uint8_t HEARTBEAT      = 9;
    constexpr uint8_t HEARTBEAT_ACK  = 10;
    constexpr uint8_t ERROR          = 11;
}

// ─────────────────────────────────────────────────────────────────────────────
// TxnType — transaction type identifiers (uint8_t)
// ─────────────────────────────────────────────────────────────────────────────
namespace TxnType {
    constexpr uint8_t DEPOSIT  = 1;
    constexpr uint8_t WITHDRAW = 2;
    constexpr uint8_t TRANSFER = 3;
    constexpr uint8_t BALANCE  = 4;
}

// ─────────────────────────────────────────────────────────────────────────────
// Status — status and error codes (uint8_t)
// ─────────────────────────────────────────────────────────────────────────────
namespace Status {
    constexpr uint8_t OK                 = 0;
    constexpr uint8_t SUCCESS            = 1;
    constexpr uint8_t FAILED             = 2;
    constexpr uint8_t NOT_FOUND          = 3;
    constexpr uint8_t INSUFFICIENT_FUNDS = 4;
    constexpr uint8_t DB_ERROR           = 5;
    constexpr uint8_t TIMEOUT            = 6;
    constexpr uint8_t QUEUE_FULL         = 7;
    constexpr uint8_t PARSE_ERROR        = 8;
    constexpr uint8_t SCHEMA_ERROR       = 9;
}

// ─────────────────────────────────────────────────────────────────────────────
// Priority — job priority levels (uint8_t)
// ─────────────────────────────────────────────────────────────────────────────
namespace Priority {
    constexpr uint8_t HI     = 0;
    constexpr uint8_t MEDIUM = 1;
    constexpr uint8_t LO     = 2;
}

// ─────────────────────────────────────────────────────────────────────────────
// ValidationResult — returned by validate()
// ─────────────────────────────────────────────────────────────────────────────
struct ValidationResult {
    bool    valid;
    uint8_t errorCode;   // Status:: constant
    char    detail[64];  // human-readable reason, empty on success
};

// ─────────────────────────────────────────────────────────────────────────────
// MessageProtocol — message building, parsing, and validation
//
// All methods are static — no instance needed.
// Used identically on all five MCUs.
// ─────────────────────────────────────────────────────────────────────────────
class MessageProtocol {
public:

    // UUID v4 generation using ESP32 hardware RNG (esp_random).
    // buf must be at least 37 bytes (36 chars + null terminator).
    static void generateUUID(char* buf, size_t bufLen);

    // Build a message envelope with auto-generated UUID.
    // Returns JsonDocument with msgId, from, to, type, and empty payload.
    // Caller populates doc[Field::PAYLOAD][Field::ACCOUNT] etc. afterwards.
    static JsonDocument createEnvelope(uint8_t from, uint8_t to, uint8_t type);

    // Serialize JsonDocument to a char buffer for SharedBus.send().
    // Returns false if message exceeds bufLen — message is dropped, not truncated.
    static bool serialize(JsonDocument& doc, char* buf, size_t bufLen);

    // Parse a raw char buffer received from SharedBus.poll().
    // Returns empty JsonDocument on failure — check with doc.isNull().
    static JsonDocument parse(const char* buf);

    // Validate envelope and payload fields against the schema registry.
    // Strict: rejects if any required field is absent.
    // Returns ValidationResult with valid=true on success, detail string on failure.
    static ValidationResult validate(const JsonDocument& doc);

    // Human-readable name helpers — for serial monitor output only.
    // Do not use return values for logic — compare against MsgType:: / Status:: constants.
    static const char* msgTypeName(uint8_t type);
    static const char* statusName(uint8_t status);
    static const char* txnTypeName(uint8_t txnType);
    static const char* priorityName(uint8_t priority);

private:
    struct PayloadSchema {
        uint8_t     msgType;
        const char* requiredFields[8];  // null-terminated list of Field:: constants
    };

    static const PayloadSchema _schemas[];
    static const size_t        _schemaCount;
    static const char*         _envelopeFields[];

    static ValidationResult _validateFields(
        const JsonObjectConst& obj,
        const char* const* fields
    );
};
