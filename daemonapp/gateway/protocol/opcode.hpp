// protocol/opcode.hpp
#ifndef OPCODE_HPP
#define OPCODE_HPP
#include <cstdint>

enum class OpCode : uint8_t {
    // ── CMD: Provisioning Control (0x01–0x1F) ──────────────────
    CMD_PROV_ENABLE             = 0x01,
    CMD_PROV_DISABLE            = 0x02,
    CMD_ADD_UNPROV_DEV          = 0x03,
    CMD_SET_DEV_UUID_MATCH      = 0x04,
    CMD_SET_NODE_NAME           = 0x05,
    CMD_DELETE_NODE             = 0x06,
    CMD_ADD_LOCAL_APP_KEY       = 0x07,
    CMD_BIND_APP_KEY_TO_MODEL   = 0x08,
    CMD_GROUP_ADD               = 0x09,
    CMD_GROUP_DELETE            = 0x0A,
    CMD_MODEL_PUB_SET           = 0x0B,

    // ── CMD: Generic On/Off (0x20–0x2F) ────────────────────────
    CMD_GENERIC_ONOFF_GET       = 0x20,
    CMD_GENERIC_ONOFF_SET       = 0x21,
    CMD_GENERIC_ONOFF_SET_UNACK = 0x22,

    // ── CMD: Sensor / Actuator (0x30–0x3F) ─────────────────────
    CMD_SENSOR_GET              = 0x30,
    CMD_ACTUATOR_GET            = 0x31,
    CMD_ACTUATOR_SET            = 0x32,
    CMD_ACTUATOR_SET_UNACK      = 0x33,

    // ── CMD: Remote Provisioning (0x40–0x4F) ───────────────────
    CMD_RPR_SCAN_GET            = 0x40,
    CMD_RPR_SCAN_START          = 0x41,
    CMD_RPR_SCAN_STOP           = 0x42,
    CMD_RPR_LINK_GET            = 0x43,
    CMD_RPR_LINK_OPEN           = 0x44,
    CMD_RPR_LINK_CLOSE          = 0x45,
    CMD_RPR_START_PROV          = 0x46,

    // ── EVT: System / Health (0x50–0x5F) ───────────────────────
    EVT_NODE_RESET              = 0x50,
    EVT_HEALTH_FAULT            = 0x51,
    EVT_HEALTH_ATTENTION        = 0x52,

    // ── CMD: Feature Control (0x70–0x7F) ───────────────────────
    CMD_RELAY_SET               = 0x70,
    CMD_RELAY_GET               = 0x71,
    CMD_PROXY_SET               = 0x72,
    CMD_PROXY_GET               = 0x73,
    CMD_FRIEND_SET              = 0x74,
    CMD_FRIEND_GET              = 0x75,

    // ── EVT: Provisioning (0x80–0x8F) ──────────────────────────
    EVT_PROV_ENABLE_COMP        = 0x80,
    EVT_PROV_DISABLE_COMP       = 0x81,
    EVT_RECV_UNPROV_ADV_PKT     = 0x82,
    EVT_PROV_LINK_OPEN          = 0x83,
    EVT_PROV_LINK_CLOSE         = 0x84,
    EVT_PROV_COMPLETE           = 0x85,
    EVT_ADD_UNPROV_DEV_COMP     = 0x86,
    EVT_SET_UUID_MATCH_COMP     = 0x87,
    EVT_SET_NODE_NAME_COMP      = 0x88,
    EVT_ADD_APP_KEY_COMP        = 0x89,
    EVT_BIND_APP_KEY_COMP       = 0x8A,
    EVT_GROUP_ADD_COMP          = 0x8B,
    EVT_GROUP_REMOVE_COMP       = 0x8C,

    // ── EVT: Config Client (0xA0–0xAF) ─────────────────────────
    EVT_CFG_CLIENT_GET_STATE    = 0xA0,
    EVT_MODEL_COMP_DATA_STATUS  = 0xA1,
    EVT_CFG_CLIENT_SET_STATE    = 0xA2,
    EVT_MODEL_APP_KEY_STATUS    = 0xA3,
    EVT_MODEL_APP_BIND_STATUS   = 0xA4,
    EVT_CFG_CLIENT_PUBLISH      = 0xA5,
    EVT_CFG_CLIENT_TIMEOUT      = 0xA6,

    // ── EVT: Generic Client (0xB0–0xBF) ────────────────────────
    EVT_GENERIC_ONOFF_STATUS    = 0xB0,
    EVT_SENSOR_STATUS           = 0xB1,
    EVT_ACTUATOR_STATUS         = 0xB2,
    EVT_GENERIC_CLIENT_TIMEOUT  = 0xB3,

    // ── EVT: Remote Provisioning (0xC0–0xCF) ───────────────────
    EVT_RPR_SCAN_STATUS         = 0xC0,
    EVT_RPR_SCAN_REPORT         = 0xC1,
    EVT_RPR_LINK_STATUS         = 0xC2,
    EVT_RPR_LINK_OPEN           = 0xC3,
    EVT_RPR_LINK_CLOSE          = 0xC4,
    EVT_RPR_LINK_REPORT         = 0xC5,
    EVT_RPR_START_PROV_COMP     = 0xC6,
    EVT_RPR_PROV_COMPLETE       = 0xC7,

    // ── EVT: Group / Heartbeat (0xD0–0xDF) ─────────────────────
    EVT_MODEL_SUBSCRIBE_STATUS  = 0xD0,
    EVT_MODEL_UNSUBSCRIBE_STATUS= 0xD1,
    EVT_MODEL_PUBLISH_STATUS    = 0xD2,
    EVT_HEARTBEAT               = 0xD3,
    EVT_HEARTBEAT_PUB_STATUS    = 0xD4,
    EVT_HEARTBEAT_SUB_STATUS    = 0xD5,
    CMD_HEARTBEAT_SUB           = 0xD8,
    CMD_HEARTBEAT_PUB           = 0xD9,

    // ── EVT: Network / Feature State (0xE0–0xEF) ───────────────
    EVT_RELAY_STATUS            = 0xE0,
    EVT_PROXY_STATUS            = 0xE1,
    EVT_FRIEND_STATUS           = 0xE2,
    EVT_FRIENDSHIP_ESTABLISHED  = 0xE3,
    EVT_FRIENDSHIP_TERMINATED   = 0xE4,
    EVT_NET_KEY_ADD             = 0xE5,
    EVT_NET_KEY_UPDATE          = 0xE6,
    EVT_NET_KEY_DELETE          = 0xE7,

    UNKNOWN                     = 0xFF
};

#endif
