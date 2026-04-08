#include "message_protocol.h"
#include <esp_random.h>

// ─────────────────────────────────────────────────────────────────────────────
// Envelope required fields — present in every message regardless of type
// ─────────────────────────────────────────────────────────────────────────────
const char* MessageProtocol::_envelopeFields[] = {
    Field::MSG_ID,
    Field::FROM,
    Field::TO,
    Field::TYPE,
    Field::PAYLOAD,
    nullptr
};

// ─────────────────────────────────────────────────────────────────────────────
// Schema registry — required payload fields per message type.
// Lives in flash (const), costs zero RAM.
// nullptr marks end of each field list.
// ─────────────────────────────────────────────────────────────────────────────
const MessageProtocol::PayloadSchema MessageProtocol::_schemas[] = {
    {
        MsgType::JOB_SUBMIT,
        { Field::TXN_TYPE, Field::ACCOUNT, Field::AMOUNT, Field::PRIORITY, nullptr }
    },
    {
        MsgType::JOB_DISPATCH,
        { Field::JOB_ID, Field::TXN_TYPE, Field::ACCOUNT, Field::AMOUNT, nullptr }
    },
    {
        MsgType::JOB_COMPLETE,
        { Field::JOB_ID, Field::STATUS, nullptr }
    },
    {
        MsgType::JOB_RESULT,
        { Field::JOB_ID, Field::STATUS, nullptr }
    },
    {
        MsgType::DB_READ,
        { Field::ACCOUNT, nullptr }
    },
    {
        MsgType::DB_READ_RESULT,
        { Field::ACCOUNT, Field::BALANCE, Field::STATUS, nullptr }
    },
    {
        MsgType::DB_WRITE,
        { Field::ACCOUNT, Field::AMOUNT, Field::NEW_BAL, Field::TXN_TYPE, nullptr }
    },
    {
        MsgType::DB_WRITE_ACK,
        { Field::ACCOUNT, Field::STATUS, nullptr }
    },
    {
        MsgType::HEARTBEAT,
        { Field::TIMESTAMP, nullptr }
    },
    {
        MsgType::HEARTBEAT_ACK,
        { Field::STATUS, Field::UPTIME, nullptr }
    },
    {
        MsgType::ERROR,
        { Field::CODE, Field::DETAIL, nullptr }
    },
};

const size_t MessageProtocol::_schemaCount =
    sizeof(MessageProtocol::_schemas) / sizeof(MessageProtocol::_schemas[0]);

// ─────────────────────────────────────────────────────────────────────────────
// UUID v4 — ESP32 hardware RNG
// Format: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx (RFC 4122)
// ─────────────────────────────────────────────────────────────────────────────
void MessageProtocol::generateUUID(char* buf, size_t bufLen) {
    if (bufLen < 37) {
        Serial.println("[MessageProtocol] generateUUID: buffer too small");
        return;
    }

    uint8_t b[16];
    uint32_t r1 = esp_random();
    uint32_t r2 = esp_random();
    uint32_t r3 = esp_random();
    uint32_t r4 = esp_random();
    memcpy(b,      &r1, 4);
    memcpy(b + 4,  &r2, 4);
    memcpy(b + 8,  &r3, 4);
    memcpy(b + 12, &r4, 4);

    // Set version 4 and variant bits (RFC 4122)
    b[6] = (b[6] & 0x0F) | 0x40;
    b[8] = (b[8] & 0x3F) | 0x80;

    snprintf(buf, bufLen,
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        b[0],  b[1],  b[2],  b[3],
        b[4],  b[5],
        b[6],  b[7],
        b[8],  b[9],
        b[10], b[11], b[12], b[13], b[14], b[15]
    );
}

// ─────────────────────────────────────────────────────────────────────────────
// createEnvelope
// ─────────────────────────────────────────────────────────────────────────────
JsonDocument MessageProtocol::createEnvelope(uint8_t from, uint8_t to, uint8_t type) {
    JsonDocument doc;

    char uuid[37];
    generateUUID(uuid, sizeof(uuid));

    doc[Field::MSG_ID]  = uuid;
    doc[Field::FROM]    = from;
    doc[Field::TO]      = to;
    doc[Field::TYPE]    = type;
    doc[Field::PAYLOAD].to<JsonObject>(); // v7: replaces deprecated createNestedObject()

    return doc;
}

// ─────────────────────────────────────────────────────────────────────────────
// serialize
// ─────────────────────────────────────────────────────────────────────────────
bool MessageProtocol::serialize(JsonDocument& doc, char* buf, size_t bufLen) {
    size_t written = serializeJson(doc, buf, bufLen);
    if (written == 0 || written >= bufLen) {
        Serial.printf("[MessageProtocol] serialize failed: %u bytes, buf %u\n",
            written, bufLen);
        return false;
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// parse
// ─────────────────────────────────────────────────────────────────────────────
JsonDocument MessageProtocol::parse(const char* buf) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, buf);
    if (err) {
        Serial.printf("[MessageProtocol] parse failed: %s\n", err.c_str());
        return JsonDocument();
    }
    return doc;
}

// ─────────────────────────────────────────────────────────────────────────────
// _validateFields — internal helper
// Checks that every field in the null-terminated fields[] list exists in obj.
// ─────────────────────────────────────────────────────────────────────────────
ValidationResult MessageProtocol::_validateFields(
    const JsonObjectConst& obj,
    const char* const* fields
) {
    for (int i = 0; fields[i] != nullptr; i++) {
        if (!obj[fields[i]].is<JsonVariantConst>() || obj[fields[i]].isNull()) {
            ValidationResult r;
            r.valid     = false;
            r.errorCode = Status::SCHEMA_ERROR;
            snprintf(r.detail, sizeof(r.detail), "missing field: %s", fields[i]);
            return r;
        }
    }
    ValidationResult r;
    r.valid     = true;
    r.errorCode = Status::OK;
    r.detail[0] = '\0';
    return r;
}

// ─────────────────────────────────────────────────────────────────────────────
// validate — strict schema validation
// ─────────────────────────────────────────────────────────────────────────────
ValidationResult MessageProtocol::validate(const JsonDocument& doc) {
    ValidationResult r;

    // 1. Check envelope fields
    JsonObjectConst root = doc.as<JsonObjectConst>();
    r = _validateFields(root, _envelopeFields);
    if (!r.valid) return r;

    // 2. Get message type
    uint8_t type = doc[Field::TYPE].as<uint8_t>();

    // 3. Find schema for this type
    const PayloadSchema* schema = nullptr;
    for (size_t i = 0; i < _schemaCount; i++) {
        if (_schemas[i].msgType == type) {
            schema = &_schemas[i];
            break;
        }
    }

    if (schema == nullptr) {
        r.valid     = false;
        r.errorCode = Status::SCHEMA_ERROR;
        snprintf(r.detail, sizeof(r.detail), "unknown message type: %d", type);
        return r;
    }

    // 4. Check payload fields
    JsonObjectConst payload = doc[Field::PAYLOAD].as<JsonObjectConst>();
    return _validateFields(payload, schema->requiredFields);
}

// ─────────────────────────────────────────────────────────────────────────────
// Debug name helpers
// For serial output only — do not use in logic comparisons
// ─────────────────────────────────────────────────────────────────────────────
const char* MessageProtocol::msgTypeName(uint8_t type) {
    switch (type) {
        case MsgType::JOB_SUBMIT:     return "JOB_SUBMIT";
        case MsgType::JOB_DISPATCH:   return "JOB_DISPATCH";
        case MsgType::JOB_COMPLETE:   return "JOB_COMPLETE";
        case MsgType::JOB_RESULT:     return "JOB_RESULT";
        case MsgType::DB_READ:        return "DB_READ";
        case MsgType::DB_READ_RESULT: return "DB_READ_RESULT";
        case MsgType::DB_WRITE:       return "DB_WRITE";
        case MsgType::DB_WRITE_ACK:   return "DB_WRITE_ACK";
        case MsgType::HEARTBEAT:      return "HEARTBEAT";
        case MsgType::HEARTBEAT_ACK:  return "HEARTBEAT_ACK";
        case MsgType::ERROR:          return "ERROR";
        default:                      return "UNKNOWN";
    }
}

const char* MessageProtocol::statusName(uint8_t status) {
    switch (status) {
        case Status::OK:                 return "OK";
        case Status::SUCCESS:            return "SUCCESS";
        case Status::FAILED:             return "FAILED";
        case Status::NOT_FOUND:          return "NOT_FOUND";
        case Status::INSUFFICIENT_FUNDS: return "INSUFFICIENT_FUNDS";
        case Status::DB_ERROR:           return "DB_ERROR";
        case Status::TIMEOUT:            return "TIMEOUT";
        case Status::QUEUE_FULL:         return "QUEUE_FULL";
        case Status::PARSE_ERROR:        return "PARSE_ERROR";
        case Status::SCHEMA_ERROR:       return "SCHEMA_ERROR";
        default:                         return "UNKNOWN";
    }
}

const char* MessageProtocol::txnTypeName(uint8_t txnType) {
    switch (txnType) {
        case TxnType::DEPOSIT:  return "DEPOSIT";
        case TxnType::WITHDRAW: return "WITHDRAW";
        case TxnType::TRANSFER: return "TRANSFER";
        case TxnType::BALANCE:  return "BALANCE";
        default:                return "UNKNOWN";
    }
}

const char* MessageProtocol::priorityName(uint8_t priority) {
    switch (priority) {
        case Priority::HI:     return "HIGH";
        case Priority::MEDIUM: return "MEDIUM";
        case Priority::LO:     return "LOW";
        default:               return "UNKNOWN";
    }
}
