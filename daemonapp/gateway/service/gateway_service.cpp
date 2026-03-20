#include "gateway_service.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <cstring>

/* ══════════════════════════════════════════════════════════════════════
   Constructor / Destructor
   ══════════════════════════════════════════════════════════════════════ */

GatewayService::GatewayService(std::shared_ptr<IDatabase>   db,
                                std::shared_ptr<ISerialPort> serial,
                                std::shared_ptr<IPC>         ipc)
    : db(db), ipc(ipc)
{
    transport     = std::make_shared<ReliableTransport>(serial);
    codec         = std::make_shared<FrameCodec>();
    sender        = std::make_shared<MeshCommandSender>(transport, codec);
    dispatcher    = std::make_shared<MeshEventDispatcher>();
    node_repo     = std::make_shared<NodeRepository>(db);
    sensor_repo   = std::make_shared<SensorRepository>(db);
    actuator_repo = std::make_shared<ActuatorRepository>(db);
}

GatewayService::~GatewayService() { stop(); }

/* ══════════════════════════════════════════════════════════════════════
   start / stop
   ══════════════════════════════════════════════════════════════════════ */

void GatewayService::start() {
    db->init();
    ipc->init();
    register_handlers();

    /*
     * Callback này chạy trong rx_thread của ReliableTransport.
     * Mỗi khi nhận được 1 RawFrame hợp lệ (đã kiểm CRC + gửi ACK):
     *   1. Decode RawFrame → MeshFrame
     *   2. Push vào mesh_event_queue
     *   3. Notify worker_thread
     * Dùng lock_guard (không cần wait) để bảo vệ push.
     */
    transport->set_frame_callback([this](const RawFrame& raw) {
        auto mesh = codec->decode(raw);
        if (!mesh) return;
        {
            std::lock_guard<std::mutex> lk(mesh_mutex);
            mesh_event_queue.push(*mesh);
        }
        mesh_cv.notify_one();
    });

    /*
     * Expose method "HandleCommand" lên DBus.
     * Khi qt_app / mqtt_app gọi method này:
     *   1. Push payload vào ipc_cmd_queue
     *   2. Notify cmd_worker_thread
     */
    ipc->expose_method("HandleCommand",
        [this](const std::vector<uint8_t>& payload) -> data_status_t {
            {
                std::lock_guard<std::mutex> lk(cmd_mutex);
                ipc_cmd_queue.push(payload);
            }
            cmd_cv.notify_one();
            return DB_SUCCESS;
        });

    transport->start();
    is_running = true;

    worker_thread     = std::thread(&GatewayService::worker_loop,     this);
    cmd_worker_thread = std::thread(&GatewayService::cmd_worker_loop, this);
    keepalive_thread  = std::thread(&GatewayService::keepalive_loop,  this);
    ipc_recv_thread   = std::thread(&GatewayService::ipc_recv_loop,   this);

    std::cout << "[GW] Started\n";
}

void GatewayService::stop() {
    // exchange(false) trả về giá trị cũ → tránh double-stop
    if (!is_running.exchange(false)) return;

    // Đánh thức tất cả thread đang wait để chúng thoát vòng lặp
    mesh_cv.notify_all();
    cmd_cv.notify_all();

    transport->stop();

    if (worker_thread.joinable())     worker_thread.join();
    if (cmd_worker_thread.joinable()) cmd_worker_thread.join();
    if (keepalive_thread.joinable())  keepalive_thread.join();
    if (ipc_recv_thread.joinable())   ipc_recv_thread.join();

    ipc->de_init();
    db->de_init();
    std::cout << "[GW] Stopped\n";
}

/* ══════════════════════════════════════════════════════════════════════
   register_handlers
   Đăng ký tất cả opcode → callback cho MeshEventDispatcher.
   Dispatcher sẽ gọi đúng handler dựa trên opcode của MeshFrame.
   ══════════════════════════════════════════════════════════════════════ */

void GatewayService::register_handlers() {
    using O = OpCode;
    dispatcher->on(O::EVT_PROV_ENABLE_COMP,
        [this](const MeshFrame& f){ on_prov_enable_comp(f); });
    dispatcher->on(O::EVT_RECV_UNPROV_ADV_PKT,
        [this](const MeshFrame& f){ on_recv_unprov_adv_pkt(f); });
    dispatcher->on(O::EVT_PROV_COMPLETE,
        [this](const MeshFrame& f){ on_prov_complete(f); });
    dispatcher->on(O::EVT_GENERIC_ONOFF_STATUS,
        [this](const MeshFrame& f){ on_onoff_status(f); });
    dispatcher->on(O::EVT_SENSOR_STATUS,
        [this](const MeshFrame& f){ on_sensor_status(f); });
    dispatcher->on(O::EVT_ACTUATOR_STATUS,
        [this](const MeshFrame& f){ on_actuator_status(f); });
    dispatcher->on(O::EVT_HEALTH_FAULT,
        [this](const MeshFrame& f){ on_health_fault(f); });
    dispatcher->on(O::EVT_CFG_CLIENT_TIMEOUT,
        [this](const MeshFrame& f){ on_cfg_timeout(f); });
}

/* ══════════════════════════════════════════════════════════════════════
   worker_loop
   ── Tiêu thụ mesh_event_queue ────────────────────────────────────────
   Flow: rx_thread → [mesh_event_queue] → worker_thread
                                              ↓
                                     dispatcher.dispatch()
                                              ↓
                                    on_xxx() → DB.save()
                                              ↓
                                    should_emit() → ipc.send()
   ══════════════════════════════════════════════════════════════════════ */

void GatewayService::worker_loop() {
    while (is_running.load()) {
        MeshFrame frame;
        {
            std::unique_lock<std::mutex> lk(mesh_mutex);
            /*
             * unique_lock bắt buộc ở đây vì condition_variable::wait()
             * cần tạm thời UNLOCK mutex trong khi ngủ, rồi RE-LOCK
             * khi được đánh thức — lock_guard không làm được điều này.
             */
            mesh_cv.wait(lk, [this] {
                return !mesh_event_queue.empty() || !is_running.load();
            });
            if (!is_running.load()) break;

            frame = mesh_event_queue.front();
            mesh_event_queue.pop();
        } // unlock ở đây — cho phép rx_thread push tiếp

        // Gọi đúng on_xxx_handler() theo opcode → lưu DB bên trong
        dispatcher->dispatch(frame);

        // Cập nhật thời điểm nhận packet cuối của node
        node_repo->update_last_seen(frame.addr, now());

        // Phát event lên IPC cho qt_app / mqtt_app nếu cần
        if (should_emit(frame.opcode)) {
            ipc->send(codec->encode(frame, frame.seq));
        }
    }
}

/* ══════════════════════════════════════════════════════════════════════
   cmd_worker_loop
   ── Tiêu thụ ipc_cmd_queue ───────────────────────────────────────────
   Flow: qt_app/mqtt_app → DBus HandleCommand
                               → [ipc_cmd_queue] → cmd_worker_thread
                                                         ↓
                                               handle_ipc_command()
                                                         ↓
                                              DB.save() + sender.xxx()
   ══════════════════════════════════════════════════════════════════════ */

void GatewayService::cmd_worker_loop() {
    while (is_running.load()) {
        std::vector<uint8_t> payload;
        {
            std::unique_lock<std::mutex> lk(cmd_mutex);
            cmd_cv.wait(lk, [this] {
                return !ipc_cmd_queue.empty() || !is_running.load();
            });
            if (!is_running.load()) break;

            payload = ipc_cmd_queue.front();
            ipc_cmd_queue.pop();
        }

        handle_ipc_command(payload);
    }
}

/* ══════════════════════════════════════════════════════════════════════
   keepalive_loop
   Mỗi 30 giây: tìm node không phản hồi > 120s → đánh dấu "offline"
   ══════════════════════════════════════════════════════════════════════ */

void GatewayService::keepalive_loop() {
    constexpr uint64_t OFFLINE_THRESHOLD = 120; // giây
    constexpr int      CHECK_PERIOD      = 30;  // giây

    while (is_running.load()) {
        // Sleep theo từng giây để phản hồi stop() nhanh
        for (int i = 0; i < CHECK_PERIOD; i++) {
            if (!is_running.load()) return;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        auto offline_nodes = node_repo->find_offline(OFFLINE_THRESHOLD);
        for (auto& node : offline_nodes) {
            node_repo->update_status(node.unicast, "offline");

            // Phát EVT_NODE_OFFLINE lên IPC
            MeshFrame evt;
            evt.opcode = OpCode::EVT_NODE_OFFLINE;
            evt.addr   = node.unicast;
            ipc->send(codec->encode(evt, 0));

            std::cerr << "[GW] Node offline addr=0x"
                      << std::hex << node.unicast << "\n";
        }
    }
}

/* ══════════════════════════════════════════════════════════════════════
   ipc_recv_loop
   Pump sd_bus_process() liên tục để nhận method call và signal từ DBus.
   ══════════════════════════════════════════════════════════════════════ */

void GatewayService::ipc_recv_loop() {
    while (is_running.load()) {
        ipc->process();
    }
}

/* ══════════════════════════════════════════════════════════════════════
   handle_ipc_command
   Decode binary payload → route đúng sender.xxx()
   Binary format: [0xAA][OPCODE][ADDR_H][ADDR_L][SEQ][TYPE][LEN][PL...][CRC_H][CRC_L]
   ══════════════════════════════════════════════════════════════════════ */

data_status_t GatewayService::handle_ipc_command(
    const std::vector<uint8_t>& raw)
{
    if (raw.size() < FRAME_MIN_SIZE) return DB_FAILED;
    if (raw[FRAME_OFF_START] != FRAME_START) return DB_FAILED;

    RawFrame rf;
    rf.opcode = raw[FRAME_OFF_OPCODE];
    rf.addr   = ((uint16_t)raw[FRAME_OFF_ADDR_H] << 8) | raw[FRAME_OFF_ADDR_L];
    rf.seq    = raw[FRAME_OFF_SEQ];
    rf.type   = raw[FRAME_OFF_TYPE];
    uint8_t plen = raw[FRAME_OFF_LEN];
    if (raw.size() >= (size_t)(FRAME_OFF_PAYLOAD + plen + FRAME_CRC_SIZE))
        rf.payload.assign(raw.begin() + FRAME_OFF_PAYLOAD,
                          raw.begin() + FRAME_OFF_PAYLOAD + plen);

    auto mesh = codec->decode(rf);
    if (!mesh) return DB_FAILED;

    auto& p = mesh->payload; // alias ngắn

    switch (mesh->opcode) {

    /* ── Provisioning ─────────────────────────────────────────────── */
    case OpCode::CMD_PROV_ENABLE:   sender->prov_enable();  break;
    case OpCode::CMD_PROV_DISABLE:  sender->prov_disable(); break;

    case OpCode::CMD_ADD_UNPROV_DEV:
        if (p.size() >= 17)
            sender->add_unprov_dev(p.data(), p[16]);
        break;

    case OpCode::CMD_SET_NODE_NAME:
        if (!p.empty())
            sender->set_node_name(mesh->addr,
                std::string(p.begin(), p.end()));
        break;

    case OpCode::CMD_DELETE_NODE:
        node_repo->remove(mesh->addr);
        sender->delete_node(mesh->addr);
        break;

    /* ── App Key ──────────────────────────────────────────────────── */
    case OpCode::CMD_ADD_APP_KEY:
        if (p.size() >= 20) {
            uint16_t ni = ((uint16_t)p[0]<<8)|p[1];
            uint16_t ai = ((uint16_t)p[2]<<8)|p[3];
            AppKeyRecord r;
            r.app_idx=ai; r.net_idx=ni; r.created_at=now();
            node_repo->save_app_key(r);
            sender->add_app_key(mesh->addr, ni, ai, &p[4]);
        }
        break;

    case OpCode::CMD_BIND_APP_KEY:
        if (p.size() >= 6) {
            uint16_t ai = ((uint16_t)p[0]<<8)|p[1];
            uint16_t mi = ((uint16_t)p[2]<<8)|p[3];
            uint16_t ci = ((uint16_t)p[4]<<8)|p[5];
            NodeAppKeyRecord r;
            r.node_id=node_id_from_unicast(mesh->addr);
            r.app_idx=ai; r.model_id=mi; r.company_id=ci;
            node_repo->bind_app_key(r);
            sender->bind_app_key(mesh->addr, ai, mi, ci);
        }
        break;

    /* ── On/Off ───────────────────────────────────────────────────── */
    case OpCode::CMD_GENERIC_ONOFF_GET:
        sender->on_off_get(mesh->addr);
        break;

    case OpCode::CMD_GENERIC_ONOFF_SET:
        if (p.size() >= 2) {
            save_onoff_command(mesh->addr, p[0] != 0, p[1]);
            sender->on_off_set(mesh->addr, p[0] != 0, p[1]);
        }
        break;

    case OpCode::CMD_GENERIC_ONOFF_SET_UNACK:
        if (p.size() >= 2) {
            save_onoff_command(mesh->addr, p[0] != 0, p[1]);
            sender->on_off_set_unack(mesh->addr, p[0] != 0, p[1]);
        }
        break;

    /* ── Sensor ───────────────────────────────────────────────────── */
    case OpCode::CMD_SENSOR_GET: {
        uint16_t sid = (p.size() >= 2)
            ? ((uint16_t)p[0]<<8)|p[1] : 0;
        sender->sensor_get(mesh->addr, sid);
        break;
    }

    /* ── Actuator ─────────────────────────────────────────────────── */
    case OpCode::CMD_ACTUATOR_GET:
        sender->actuator_get(mesh->addr);
        break;

    case OpCode::CMD_ACTUATOR_SET:
        if (p.size() >= 5) {
            uint16_t ai = ((uint16_t)p[0]<<8)|p[1];
            uint16_t sp = ((uint16_t)p[2]<<8)|p[3];
            save_actuator_command(mesh->addr, ai, sp, p[4]);
            sender->actuator_set(mesh->addr, ai, sp, p[4]);
        }
        break;

    case OpCode::CMD_ACTUATOR_SET_UNACK:
        if (p.size() >= 5) {
            uint16_t ai = ((uint16_t)p[0]<<8)|p[1];
            uint16_t sp = ((uint16_t)p[2]<<8)|p[3];
            save_actuator_command(mesh->addr, ai, sp, p[4]);
            sender->actuator_set_unack(mesh->addr, ai, sp, p[4]);
        }
        break;

    /* ── Config ───────────────────────────────────────────────────── */
    case OpCode::CMD_RELAY_SET:
        if (p.size() >= 3)
            sender->relay_set(mesh->addr, p[0], p[1], p[2]);
        break;
    case OpCode::CMD_RELAY_GET:   sender->relay_get(mesh->addr);            break;
    case OpCode::CMD_PROXY_SET:
        if (!p.empty()) sender->proxy_set(mesh->addr, p[0]);               break;
    case OpCode::CMD_PROXY_GET:   sender->proxy_get(mesh->addr);            break;
    case OpCode::CMD_FRIEND_SET:
        if (!p.empty()) sender->friend_set(mesh->addr, p[0]);              break;
    case OpCode::CMD_FRIEND_GET:  sender->friend_get(mesh->addr);           break;

    /* ── Heartbeat ────────────────────────────────────────────────── */
    case OpCode::CMD_HEARTBEAT_SUB:
        if (p.size() >= 2)
            sender->heartbeat_sub(mesh->addr,
                ((uint16_t)p[0]<<8)|p[1]);
        break;
    case OpCode::CMD_HEARTBEAT_PUB:
        if (p.size() >= 4)
            sender->heartbeat_pub(mesh->addr,
                ((uint16_t)p[0]<<8)|p[1], p[2], p[3]);
        break;

    /* ── Remote Provisioning ──────────────────────────────────────── */
    case OpCode::CMD_RPR_SCAN_GET:   sender->rpr_scan_get(mesh->addr);   break;
    case OpCode::CMD_RPR_SCAN_STOP:  sender->rpr_scan_stop(mesh->addr);  break;
    case OpCode::CMD_RPR_LINK_GET:   sender->rpr_link_get(mesh->addr);   break;
    case OpCode::CMD_RPR_START_PROV: sender->rpr_start_prov(mesh->addr); break;

    case OpCode::CMD_RPR_SCAN_START:
        if (p.size() >= 3)
            sender->rpr_scan_start(mesh->addr, p[0], p[1],
                p[2] != 0,
                (p.size() >= 19) ? &p[3] : nullptr);
        break;

    case OpCode::CMD_RPR_LINK_OPEN:
        if (p.size() >= 18)
            sender->rpr_link_open(mesh->addr, p.data(),
                p[16] != 0, p[17]);
        break;

    case OpCode::CMD_RPR_LINK_CLOSE:
        sender->rpr_link_close(mesh->addr,
            p.empty() ? 0 : p[0]);
        break;

    default:
        std::cerr << "[GW] Unknown IPC cmd opcode=0x"
                  << std::hex << (int)rf.opcode << "\n";
        return DB_FAILED;
    }
    return DB_SUCCESS;
}

/* ══════════════════════════════════════════════════════════════════════
   Event handlers
   ══════════════════════════════════════════════════════════════════════ */

void GatewayService::on_prov_enable_comp(const MeshFrame& f) {
    bool ok = f.payload.empty() || f.payload[0] == 0;
    std::cout << "[GW] Provisioning "
              << (ok ? "ENABLED" : "FAILED") << "\n";
}

void GatewayService::on_recv_unprov_adv_pkt(const MeshFrame& f) {
    // Payload: [uuid(16)][rssi(1)]
    if (f.payload.size() < 17) return;

    std::ostringstream ss;
    for (int i = 0; i < 16; i++)
        ss << std::hex << std::setw(2) << std::setfill('0')
           << (int)f.payload[i];
    int8_t rssi = (int8_t)f.payload[16];
    std::cout << "[GW] Unprov device uuid=" << ss.str()
              << " rssi=" << (int)rssi << "dBm\n";
}

void GatewayService::on_prov_complete(const MeshFrame& f) {
    // Payload: [unicast_H][unicast_L][uuid(16)][elem_num]
    if (f.payload.size() < 19) return;

    uint16_t unicast = ((uint16_t)f.payload[0] << 8) | f.payload[1];

    std::ostringstream ss;
    for (int i = 2; i < 18; i++)
        ss << std::hex << std::setw(2) << std::setfill('0')
           << (int)f.payload[i];

    DeviceRecord rec;
    rec.node_id    = node_id_from_unicast(unicast);
    rec.unicast    = unicast;
    rec.uuid       = ss.str();
    rec.elem_num   = f.payload[18];
    rec.status     = "provisioned";
    rec.created_at = now();
    rec.last_seen  = now();
    node_repo->save(rec);

    std::cout << "[GW] Prov complete addr=0x"
              << std::hex << unicast << "\n";
}

void GatewayService::on_onoff_status(const MeshFrame& f) {
    // Payload: [present_onoff(1)]
    if (f.payload.empty()) return;

    ActuatorStateRecord rec;
    rec.node_id    = node_id_from_unicast(f.addr);
    rec.onoff      = (f.payload[0] != 0);
    rec.updated_at = now();
    actuator_repo->save(rec);

    // Đánh dấu pending command là "success"
    for (auto& cmd : actuator_repo->find_pending_commands())
        if (cmd.node_id == rec.node_id)
            actuator_repo->update_command_result(cmd.id, "success", now());
}

void GatewayService::on_sensor_status(const MeshFrame& f) {
    // Payload: [sensor_id(2)][temp(4f)][humi(4f)][lux(4f)]
    //          [motion(1)][battery(1)][status(1)]  tổng = 17 bytes
    if (f.payload.size() < 17) return;

    SensorReadingRecord rec;
    rec.node_id   = node_id_from_unicast(f.addr);
    rec.sensor_id = ((uint16_t)f.payload[0] << 8) | f.payload[1];
    // memcpy tránh UB do unaligned float access
    memcpy(&rec.temperature, &f.payload[2],  sizeof(float));
    memcpy(&rec.humidity,    &f.payload[6],  sizeof(float));
    memcpy(&rec.lux,         &f.payload[10], sizeof(float));
    rec.motion    = f.payload[14] != 0;
    rec.battery   = f.payload[15];
    rec.status    = f.payload[16];
    rec.timestamp = now();
    sensor_repo->save(rec);
}

void GatewayService::on_actuator_status(const MeshFrame& f) {
    // Payload: [act_id(2)][present_sp(4f)][target_sp(4f)][onoff(1)][status(1)] = 12 bytes
    if (f.payload.size() < 12) return;

    ActuatorStateRecord rec;
    rec.node_id     = node_id_from_unicast(f.addr);
    rec.actuator_id = ((uint16_t)f.payload[0] << 8) | f.payload[1];
    memcpy(&rec.present_setpoint, &f.payload[2], sizeof(float));
    memcpy(&rec.target_setpoint,  &f.payload[6], sizeof(float));
    rec.onoff      = f.payload[10] != 0;
    rec.status     = f.payload[11];
    rec.updated_at = now();
    actuator_repo->save(rec);

    // Cập nhật kết quả lệnh đang pending
    for (auto& cmd : actuator_repo->find_pending_commands())
        if (cmd.node_id == rec.node_id &&
            cmd.actuator_id == rec.actuator_id)
            actuator_repo->update_command_result(cmd.id, "success", now());
}

void GatewayService::on_health_fault(const MeshFrame& f) {
    node_repo->update_status(f.addr, "fault");
    std::cerr << "[GW] Health fault addr=0x"
              << std::hex << f.addr << "\n";
}

void GatewayService::on_cfg_timeout(const MeshFrame& f) {
    node_repo->update_status(f.addr, "timeout");
    std::cerr << "[GW] Cfg timeout addr=0x"
              << std::hex << f.addr << "\n";
}

/* ══════════════════════════════════════════════════════════════════════
   DB save helpers
   ══════════════════════════════════════════════════════════════════════ */

void GatewayService::save_onoff_command(uint16_t unicast,
                                         bool on, uint8_t tid) {
    ActuatorCommandRecord cmd;
    cmd.node_id   = node_id_from_unicast(unicast);
    cmd.onoff     = on;
    cmd.tid       = tid;
    cmd.result    = "pending";
    cmd.issued_at = now();
    actuator_repo->save_command(cmd);
}

void GatewayService::save_actuator_command(uint16_t unicast,
                                            uint16_t act_id,
                                            uint16_t setpoint,
                                            uint8_t status) {
    ActuatorCommandRecord cmd;
    cmd.node_id     = node_id_from_unicast(unicast);
    cmd.actuator_id = act_id;
    cmd.setpoint    = (float)setpoint;
    cmd.status      = status;
    cmd.result      = "pending";
    cmd.issued_at   = now();
    actuator_repo->save_command(cmd);
}

/* ══════════════════════════════════════════════════════════════════════
   Public API — Facade
   Mỗi hàm public chỉ làm 2 việc:
     1. Lưu DB nếu cần (command tracking)
     2. Delegate sang MeshCommandSender
   ══════════════════════════════════════════════════════════════════════ */

void GatewayService::enable_provisioning()  { sender->prov_enable();  }
void GatewayService::disable_provisioning() { sender->prov_disable(); }

void GatewayService::add_unprovisioned_device(const std::string& hex,
                                               uint8_t bearer) {
    uint8_t uuid[16]{};
    hex_to_bytes(hex, uuid, 16);
    sender->add_unprov_dev(uuid, bearer);
}
void GatewayService::set_dev_uuid_match(const std::string& hex) {
    uint8_t m[8]{};
    hex_to_bytes(hex, m, 8);
    sender->set_dev_uuid_match(m);
}
void GatewayService::set_node_name(uint16_t u, const std::string& n) {
    sender->set_node_name(u, n);
}
void GatewayService::delete_node(uint16_t u) {
    node_repo->remove(u);
    sender->delete_node(u);
}
void GatewayService::add_app_key(uint16_t u, uint16_t ni,
                                  uint16_t ai, const std::string& hex) {
    uint8_t key[16]{};
    hex_to_bytes(hex, key, 16);
    AppKeyRecord r; r.app_idx=ai; r.net_idx=ni; r.created_at=now();
    node_repo->save_app_key(r);
    sender->add_app_key(u, ni, ai, key);
}
void GatewayService::bind_app_key(uint16_t u, uint16_t ai,
                                   uint16_t mi, uint16_t ci) {
    NodeAppKeyRecord r;
    r.node_id    = node_id_from_unicast(u);
    r.app_idx    = ai; r.model_id = mi; r.company_id = ci;
    node_repo->bind_app_key(r);
    sender->bind_app_key(u, ai, mi, ci);
}
void GatewayService::group_add(uint16_t u, uint16_t el, uint16_t gr,
                                uint16_t m, uint16_t c) {
    DeviceGroupRecord r;
    r.node_id    = node_id_from_unicast(u); r.group_addr = gr;
    r.model_id   = m; r.company_id = c;     r.created_at = now();
    node_repo->add_to_group(r);
    sender->group_add(u, el, gr, m, c);
}
void GatewayService::group_delete(uint16_t u, uint16_t el, uint16_t gr,
                                   uint16_t m, uint16_t c) {
    node_repo->remove_from_group(node_id_from_unicast(u), gr);
    sender->group_delete(u, el, gr, m, c);
}
void GatewayService::model_pub_set(uint16_t u, uint16_t el, uint16_t pa,
                                    uint16_t m, uint16_t c,
                                    uint8_t ttl, uint8_t p) {
    sender->model_pub_set(u, el, pa, m, c, ttl, p);
}
void GatewayService::on_off_get(uint16_t u) { sender->on_off_get(u); }
void GatewayService::on_off_set(uint16_t u, bool on, uint8_t tid) {
    save_onoff_command(u, on, tid);
    sender->on_off_set(u, on, tid);
}
void GatewayService::on_off_set_unack(uint16_t u, bool on, uint8_t tid) {
    save_onoff_command(u, on, tid);
    sender->on_off_set_unack(u, on, tid);
}
void GatewayService::sensor_get(uint16_t u, uint16_t sid) {
    sender->sensor_get(u, sid);
}
void GatewayService::actuator_get(uint16_t u) { sender->actuator_get(u); }
void GatewayService::actuator_set(uint16_t u, uint16_t ai,
                                   uint16_t sp, uint8_t st) {
    save_actuator_command(u, ai, sp, st);
    sender->actuator_set(u, ai, sp, st);
}
void GatewayService::actuator_set_unack(uint16_t u, uint16_t ai,
                                         uint16_t sp, uint8_t st) {
    save_actuator_command(u, ai, sp, st);
    sender->actuator_set_unack(u, ai, sp, st);
}
void GatewayService::relay_set(uint16_t u,uint8_t s,uint8_t rc,uint8_t ri)
    { sender->relay_set(u,s,rc,ri); }
void GatewayService::relay_get(uint16_t u)     { sender->relay_get(u); }
void GatewayService::proxy_set(uint16_t u,uint8_t s){ sender->proxy_set(u,s); }
void GatewayService::proxy_get(uint16_t u)     { sender->proxy_get(u); }
void GatewayService::friend_set(uint16_t u,uint8_t s){ sender->friend_set(u,s); }
void GatewayService::friend_get(uint16_t u)    { sender->friend_get(u); }
void GatewayService::heartbeat_sub(uint16_t u,uint16_t g)
    { sender->heartbeat_sub(u,g); }
void GatewayService::heartbeat_pub(uint16_t u,uint16_t d,uint8_t p,uint8_t t)
    { sender->heartbeat_pub(u,d,p,t); }
void GatewayService::rpr_scan_get(uint16_t s)  { sender->rpr_scan_get(s); }
void GatewayService::rpr_scan_start(uint16_t s,uint8_t lim,uint8_t to,
                                     bool f,const uint8_t* u)
    { sender->rpr_scan_start(s,lim,to,f,u); }
void GatewayService::rpr_scan_stop(uint16_t s)  { sender->rpr_scan_stop(s); }
void GatewayService::rpr_link_get(uint16_t s)   { sender->rpr_link_get(s); }
void GatewayService::rpr_link_open(uint16_t s,const uint8_t u[16],
                                    bool te,uint8_t to)
    { sender->rpr_link_open(s,u,te,to); }
void GatewayService::rpr_link_close(uint16_t s,uint8_t r)
    { sender->rpr_link_close(s,r); }
void GatewayService::rpr_start_prov(uint16_t s) { sender->rpr_start_prov(s); }

/* ── DB Query API ──────────────────────────────────────────────────── */
std::vector<DeviceRecord>
    GatewayService::get_all_nodes()
    { return node_repo->find_all(); }

std::optional<DeviceRecord>
    GatewayService::get_node(uint16_t u)
    { return node_repo->find_by_addr(u); }

std::vector<DeviceRecord>
    GatewayService::get_nodes_by_status(const std::string& s)
    { return node_repo->find_by_status(s); }

std::optional<SensorReadingRecord>
    GatewayService::get_latest_sensor(uint16_t u)
    { return sensor_repo->find_by_addr(u); }

std::vector<SensorReadingRecord>
    GatewayService::get_sensor_history(uint16_t u, int lim)
    { return sensor_repo->find_latest(node_id_from_unicast(u), lim); }

std::vector<SensorReadingRecord>
    GatewayService::get_sensor_range(uint16_t u, uint64_t f, uint64_t t)
    { return sensor_repo->find_by_time_range(node_id_from_unicast(u),f,t); }

std::optional<ActuatorStateRecord>
    GatewayService::get_actuator_state(uint16_t u)
    { return actuator_repo->find_by_addr(u); }

std::vector<ActuatorCommandRecord>
    GatewayService::get_actuator_commands(uint16_t u, int lim)
    { return actuator_repo->find_commands(node_id_from_unicast(u), lim); }

std::vector<ActuatorCommandRecord>
    GatewayService::get_pending_commands()
    { return actuator_repo->find_pending_commands(); }

/* ══════════════════════════════════════════════════════════════════════
   Static utilities
   ══════════════════════════════════════════════════════════════════════ */

bool GatewayService::should_emit(OpCode opcode) {
    switch (opcode) {
    case OpCode::EVT_PROV_ENABLE_COMP:
    case OpCode::EVT_RECV_UNPROV_ADV_PKT:
    case OpCode::EVT_PROV_COMPLETE:
    case OpCode::EVT_GENERIC_ONOFF_STATUS:
    case OpCode::EVT_SENSOR_STATUS:
    case OpCode::EVT_ACTUATOR_STATUS:
    case OpCode::EVT_HEALTH_FAULT:
    case OpCode::EVT_CFG_CLIENT_TIMEOUT:
    case OpCode::EVT_NODE_OFFLINE:      return true;
    default:                            return false;
    }
}

bool GatewayService::hex_to_bytes(const std::string& hex,
                                   uint8_t* out, size_t len) {
    if (hex.size() < len * 2) return false;
    for (size_t i = 0; i < len; i++)
        out[i] = (uint8_t)std::stoul(hex.substr(i * 2, 2), nullptr, 16);
    return true;
}

uint64_t GatewayService::now() {
    return (uint64_t)std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string GatewayService::node_id_from_unicast(uint16_t unicast) {
    std::ostringstream ss;
    ss << "node_0x"
       << std::hex << std::setw(4) << std::setfill('0') << unicast;
    return ss.str();
}
