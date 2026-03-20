#ifndef MESH_COMMAND_SENDER_HPP
#define MESH_COMMAND_SENDER_HPP

#include "../transport/reliable_transport.hpp"
#include "../protocol/frame_codec.hpp"
#include "../protocol/opcode.hpp"
#include "uart_msgs_def.h"
#include <memory>
#include <string>
#include <vector>
#include <atomic>

class MeshCommandSender {
public:
    MeshCommandSender(std::shared_ptr<ReliableTransport> transport,
                      std::shared_ptr<FrameCodec>        codec);

    // ── SECTION 1: Provisioning Control ────────────────────────────────
    void prov_enable();
    void prov_disable();
    void add_unprov_dev(const uint8_t uuid[16], uint8_t bearer);
    void set_dev_uuid_match(const uint8_t uuid_match[8]);
    void set_node_name(uint16_t addr, const std::string& name);
    void delete_node(uint16_t addr);
    void add_app_key(uint16_t addr, uint16_t net_idx,
                     uint16_t app_idx, const uint8_t key[16]);
    void bind_app_key(uint16_t addr, uint16_t app_idx,
                      uint16_t model_id, uint16_t company_id = 0xFFFF);
    void group_add(uint16_t addr, uint16_t element_addr,
                   uint16_t group_addr,
                   uint16_t model_id, uint16_t company_id = 0xFFFF);
    void group_delete(uint16_t addr, uint16_t element_addr,
                      uint16_t group_addr,
                      uint16_t model_id, uint16_t company_id = 0xFFFF);
    void model_pub_set(uint16_t addr, uint16_t element_addr,
                       uint16_t pub_addr, uint16_t model_id,
                       uint16_t company_id = 0xFFFF,
                       uint8_t pub_ttl = 0x07, uint8_t pub_period = 0x00);

    // ── SECTION 2: Generic On/Off ───────────────────────────────────────
    void on_off_get(uint16_t addr);
    void on_off_set(uint16_t addr, bool on, uint8_t tid);
    void on_off_set_unack(uint16_t addr, bool on, uint8_t tid);

    // ── SECTION 3: Sensor / Actuator ───────────────────────────────────
    void sensor_get(uint16_t addr, uint16_t sensor_id);
    void actuator_get(uint16_t addr);
    void actuator_set(uint16_t addr, uint16_t act_id,
                      uint16_t setpoint, uint8_t status);
    void actuator_set_unack(uint16_t addr, uint16_t act_id,
                             uint16_t setpoint, uint8_t status);

    // ── SECTION 4: Remote Provisioning (RPR) ───────────────────────────
    void rpr_scan_get(uint16_t server_addr);
    void rpr_scan_start(uint16_t server_addr,
                        uint8_t scan_items_limit, uint8_t timeout,
                        bool uuid_filter = false,
                        const uint8_t uuid[16] = nullptr);
    void rpr_scan_stop(uint16_t server_addr);
    void rpr_link_get(uint16_t server_addr);
    void rpr_link_open(uint16_t server_addr, const uint8_t uuid[16],
                       bool timeout_en = false, uint8_t timeout = 0);
    void rpr_link_close(uint16_t server_addr, uint8_t reason = 0x00);
    void rpr_start_prov(uint16_t server_addr);

    // ── SECTION 6: Feature Control ─────────────────────────────────────
    void relay_set(uint16_t addr, uint8_t state,
                   uint8_t retransmit_count     = 0,
                   uint8_t retransmit_interval  = 0);
    void relay_get(uint16_t addr);
    void proxy_set(uint16_t addr, uint8_t state);
    void proxy_get(uint16_t addr);
    void friend_set(uint16_t addr, uint8_t state);
    void friend_get(uint16_t addr);

    // ── SECTION 11: Heartbeat ───────────────────────────────────────────
    void heartbeat_sub(uint16_t addr, uint16_t group_addr);
    void heartbeat_pub(uint16_t addr, uint16_t dst,
                       uint8_t period, uint8_t ttl);

private:
    std::shared_ptr<ReliableTransport> transport;
    std::shared_ptr<FrameCodec>        codec;

    void send_cmd(OpCode opcode, uint16_t addr,
                  const uint8_t* payload = nullptr,
                  uint8_t len = 0);

    template<typename T>
    void send_struct(OpCode opcode, uint16_t addr, const T& s) {
        send_cmd(opcode, addr,
                 reinterpret_cast<const uint8_t*>(&s),
                 static_cast<uint8_t>(sizeof(T)));
    }
};

#endif
