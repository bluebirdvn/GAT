#include "mesh_command_sender.hpp"
#include <cstring>
#include <iostream>

MeshCommandSender::MeshCommandSender(
    std::shared_ptr<ReliableTransport> transport,
    std::shared_ptr<FrameCodec>        codec)
    : transport(transport), codec(codec) {}

// ─── PRIVATE: send_cmd ────────────────────────────────────────────────────────
void MeshCommandSender::send_cmd(OpCode opcode, uint16_t addr,
                                  const uint8_t* payload, uint8_t len) {
    MeshFrame f;
    f.opcode  = opcode;
    f.addr    = addr;
    f.type    = UART_FRAME_TYPE_DATA;
    if (payload && len > 0)
        f.payload.assign(payload, payload + len);

    auto raw = codec->encode(f);
    transport->enqueue_frame(raw, /*reliable=*/true);
}


/* ═══════════════════════════════════════════════════════════════════════════
 * SECTION 1 — Provisioning Control
 * ═══════════════════════════════════════════════════════════════════════════ */

void MeshCommandSender::prov_enable() {
    // PAYLOAD: trống
    send_cmd(OpCode::CMD_PROV_ENABLE, 0x0000);
}

void MeshCommandSender::prov_disable() {
    // PAYLOAD: trống
    send_cmd(OpCode::CMD_PROV_DISABLE, 0x0000);
}

void MeshCommandSender::add_unprov_dev(const uint8_t uuid[16], uint8_t bearer) {
    mesh_cmd_add_unprov_dev_t p{};
    std::memcpy(p.uuid, uuid, 16);
    p.bearer = bearer;
    send_struct(OpCode::CMD_ADD_UNPROV_DEV, 0x0000, p);
}

void MeshCommandSender::set_dev_uuid_match(const uint8_t uuid_match[8]) {
    mesh_cmd_set_uuid_match_t p{};
    std::memcpy(p.uuid_match, uuid_match, 8);
    send_struct(OpCode::CMD_SET_DEV_UUID_MATCH, 0x0000, p);
}

void MeshCommandSender::set_node_name(uint16_t addr, const std::string& name) {
    mesh_cmd_set_node_name_t p{};
    std::memset(p.name, 0, sizeof(p.name));
    std::memcpy(p.name, name.c_str(),
                std::min(name.size(), sizeof(p.name) - 1));
    send_struct(OpCode::CMD_SET_NODE_NAME, addr, p);
}

void MeshCommandSender::delete_node(uint16_t addr) {
    // PAYLOAD: trống
    send_cmd(OpCode::CMD_DELETE_NODE, addr);
}

void MeshCommandSender::add_app_key(uint16_t addr, uint16_t net_idx,
                                     uint16_t app_idx, const uint8_t key[16]) {
    mesh_cmd_add_app_key_t p{};
    p.net_idx = net_idx;
    p.app_idx = app_idx;
    std::memcpy(p.app_key, key, 16);
    send_struct(OpCode::CMD_ADD_LOCAL_APP_KEY, addr, p);
}

void MeshCommandSender::bind_app_key(uint16_t addr, uint16_t app_idx,
                                      uint16_t model_id, uint16_t company_id) {
    mesh_cmd_bind_app_key_t p{};
    p.app_idx    = app_idx;
    p.model_id   = model_id;
    p.company_id = company_id;
    send_struct(OpCode::CMD_BIND_APP_KEY_TO_MODEL, addr, p);
}

void MeshCommandSender::group_add(uint16_t addr, uint16_t element_addr,
                                   uint16_t group_addr,
                                   uint16_t model_id, uint16_t company_id) {
    mesh_cmd_add_dev_to_group_t p{};
    p.element_addr = element_addr;
    p.group_addr   = group_addr;
    p.company_id   = company_id;
    p.model_id     = model_id;
    send_struct(OpCode::CMD_GROUP_ADD, addr, p);
}

void MeshCommandSender::group_delete(uint16_t addr, uint16_t element_addr,
                                      uint16_t group_addr,
                                      uint16_t model_id, uint16_t company_id) {
    mesh_cmd_remove_dev_from_group_t p{};
    p.element_addr = element_addr;
    p.group_addr   = group_addr;
    p.company_id   = company_id;
    p.model_id     = model_id;
    send_struct(OpCode::CMD_GROUP_DELETE, addr, p);
}

void MeshCommandSender::model_pub_set(uint16_t addr, uint16_t element_addr,
                                       uint16_t pub_addr, uint16_t model_id,
                                       uint16_t company_id, uint8_t pub_ttl,
                                       uint8_t pub_period) {
    mesh_cmd_model_pub_set_t p{};
    p.element_addr = element_addr;
    p.pub_addr     = pub_addr;
    p.company_id   = company_id;
    p.model_id     = model_id;
    p.pub_ttl      = pub_ttl;
    p.pub_period   = pub_period;
    send_struct(OpCode::CMD_MODEL_PUB_SET, addr, p);
}


/* ═══════════════════════════════════════════════════════════════════════════
 * SECTION 2 — Generic On/Off
 * ═══════════════════════════════════════════════════════════════════════════ */

void MeshCommandSender::on_off_get(uint16_t addr) {
    // PAYLOAD: trống
    send_cmd(OpCode::CMD_GENERIC_ONOFF_GET, addr);
}

void MeshCommandSender::on_off_set(uint16_t addr, bool on, uint8_t tid) {
    mesh_cmd_onoff_set_t p{};
    p.onoff = on ? 0x01 : 0x00;
    p.tid   = tid;
    send_struct(OpCode::CMD_GENERIC_ONOFF_SET, addr, p);
}

void MeshCommandSender::on_off_set_unack(uint16_t addr, bool on, uint8_t tid) {
    mesh_cmd_onoff_set_t p{};
    p.onoff = on ? 0x01 : 0x00;
    p.tid   = tid;
    send_struct(OpCode::CMD_GENERIC_ONOFF_SET_UNACK, addr, p);
}


/* ═══════════════════════════════════════════════════════════════════════════
 * SECTION 3 — Sensor / Actuator
 * ═══════════════════════════════════════════════════════════════════════════ */

void MeshCommandSender::sensor_get(uint16_t addr, uint16_t sensor_id) {
    mesh_cmd_sensor_get_t p{};
    p.sensor_id = sensor_id;
    send_struct(OpCode::CMD_SENSOR_GET, addr, p);
}

void MeshCommandSender::actuator_get(uint16_t addr) {
    // PAYLOAD: trống
    send_cmd(OpCode::CMD_ACTUATOR_GET, addr);
}

void MeshCommandSender::actuator_set(uint16_t addr, uint16_t act_id,
                                      uint16_t setpoint, uint8_t status) {
    mesh_cmd_actuator_set_t p{};
    p.actuator_id = act_id;
    p.setpoint    = setpoint;
    p.status      = status;
    send_struct(OpCode::CMD_ACTUATOR_SET, addr, p);
}

void MeshCommandSender::actuator_set_unack(uint16_t addr, uint16_t act_id,
                                            uint16_t setpoint, uint8_t status) {
    mesh_cmd_actuator_set_t p{};
    p.actuator_id = act_id;
    p.setpoint    = setpoint;
    p.status      = status;
    send_struct(OpCode::CMD_ACTUATOR_SET_UNACK, addr, p);
}


/* ═══════════════════════════════════════════════════════════════════════════
 * SECTION 4 — Remote Provisioning (RPR)
 * ═══════════════════════════════════════════════════════════════════════════ */

void MeshCommandSender::rpr_scan_get(uint16_t server_addr) {
    // PAYLOAD: trống
    send_cmd(OpCode::CMD_RPR_SCAN_GET, server_addr);
}

void MeshCommandSender::rpr_scan_start(uint16_t server_addr,
                                        uint8_t scan_items_limit,
                                        uint8_t timeout,
                                        bool uuid_filter,
                                        const uint8_t uuid[16]) {
    mesh_cmd_rpr_scan_start_t p{};
    p.scan_items_limit = scan_items_limit;
    p.timeout          = timeout;
    p.uuid_filter_en   = uuid_filter ? 0x01 : 0x00;
    if (uuid_filter && uuid)
        std::memcpy(p.uuid, uuid, 16);
    else
        std::memset(p.uuid, 0, 16);
    send_struct(OpCode::CMD_RPR_SCAN_START, server_addr, p);
}

void MeshCommandSender::rpr_scan_stop(uint16_t server_addr) {
    // PAYLOAD: trống
    send_cmd(OpCode::CMD_RPR_SCAN_STOP, server_addr);
}

void MeshCommandSender::rpr_link_get(uint16_t server_addr) {
    // PAYLOAD: trống
    send_cmd(OpCode::CMD_RPR_LINK_GET, server_addr);
}

void MeshCommandSender::rpr_link_open(uint16_t server_addr,
                                       const uint8_t uuid[16],
                                       bool timeout_en, uint8_t timeout) {
    mesh_cmd_rpr_link_open_t p{};
    std::memcpy(p.uuid, uuid, 16);
    p.timeout_en = timeout_en ? 0x01 : 0x00;
    p.timeout    = timeout;
    send_struct(OpCode::CMD_RPR_LINK_OPEN, server_addr, p);
}

void MeshCommandSender::rpr_link_close(uint16_t server_addr, uint8_t reason) {
    mesh_cmd_rpr_link_close_t p{};
    p.reason = reason;
    send_struct(OpCode::CMD_RPR_LINK_CLOSE, server_addr, p);
}

void MeshCommandSender::rpr_start_prov(uint16_t server_addr) {
    // PAYLOAD: trống
    send_cmd(OpCode::CMD_RPR_START_PROV, server_addr);
}


/* ═══════════════════════════════════════════════════════════════════════════
 * SECTION 6 — Feature Control
 * ═══════════════════════════════════════════════════════════════════════════ */

void MeshCommandSender::relay_set(uint16_t addr, uint8_t state,
                                   uint8_t retransmit_count,
                                   uint8_t retransmit_interval) {
    mesh_cmd_relay_set_t p{};
    p.relay_state               = state;
    p.retransmit_count          = retransmit_count;
    p.retransmit_interval_ms    = retransmit_interval;
    send_struct(OpCode::CMD_RELAY_SET, addr, p);
}

void MeshCommandSender::relay_get(uint16_t addr) {
    // PAYLOAD: trống
    send_cmd(OpCode::CMD_RELAY_GET, addr);
}

void MeshCommandSender::proxy_set(uint16_t addr, uint8_t state) {
    mesh_cmd_feature_set_t p{};
    p.state = state;
    send_struct(OpCode::CMD_PROXY_SET, addr, p);
}

void MeshCommandSender::proxy_get(uint16_t addr) {
    // PAYLOAD: trống
    send_cmd(OpCode::CMD_PROXY_GET, addr);
}

void MeshCommandSender::friend_set(uint16_t addr, uint8_t state) {
    mesh_cmd_feature_set_t p{};
    p.state = state;
    send_struct(OpCode::CMD_FRIEND_SET, addr, p);
}

void MeshCommandSender::friend_get(uint16_t addr) {
    // PAYLOAD: trống
    send_cmd(OpCode::CMD_FRIEND_GET, addr);
}


/* ═══════════════════════════════════════════════════════════════════════════
 * SECTION 11 — Heartbeat
 * ═══════════════════════════════════════════════════════════════════════════ */

void MeshCommandSender::heartbeat_sub(uint16_t addr, uint16_t group_addr) {
    mesh_cmd_heartbeat_sub_t p{};
    p.group_addr = group_addr;
    send_struct(OpCode::CMD_HEARTBEAT_SUB, addr, p);
}

void MeshCommandSender::heartbeat_pub(uint16_t addr, uint16_t dst,
                                       uint8_t period, uint8_t ttl) {
    mesh_cmd_heartbeat_pub_t p{};
    p.dst    = dst;
    p.period = period;
    p.ttl    = ttl;
    send_struct(OpCode::CMD_HEARTBEAT_PUB, addr, p);
}
