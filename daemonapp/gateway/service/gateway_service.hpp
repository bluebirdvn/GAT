#pragma once
#include "../transport/reliable_transport.hpp"
#include "../protocol/frame_codec.hpp"
#include "../mesh/mesh_command_sender.hpp"
#include "../mesh/mesh_event_dispatcher.hpp"
#include "../database/idatabase.hpp"
#include "../database/node_repository.hpp"
#include "../database/sensor_repository.hpp"
#include "../database/actuator_repository.hpp"
#include "../ipc/ipc.hpp"
#include "../protocol/mesh_frame.hpp"
#include <memory>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <string>
#include <vector>
#include <optional>

class GatewayService {
public:
    // ─────────────────────────────────────────────────────────────────
    // Khởi tạo: nhận 3 dependency chính qua shared_ptr (DI pattern)
    // ─────────────────────────────────────────────────────────────────
    GatewayService(std::shared_ptr<IDatabase>   db,
                   std::shared_ptr<ISerialPort> serial,
                   std::shared_ptr<IPC>         ipc);
    ~GatewayService();

    void start();   // khởi động tất cả thread + transport
    void stop();    // dừng graceful: wakeup tất cả cv → join thread

    // ── Provisioning ──────────────────────────────────────────────────
    void enable_provisioning();
    void disable_provisioning();
    void add_unprovisioned_device(const std::string& uuid_hex,
                                  uint8_t bearer = 0);
    void set_dev_uuid_match(const std::string& uuid_match_hex);
    void set_node_name(uint16_t unicast, const std::string& name);
    void delete_node  (uint16_t unicast);

    // ── App Key / Group / Pub ─────────────────────────────────────────
    void add_app_key  (uint16_t unicast, uint16_t net_idx, uint16_t app_idx,
                       const std::string& key_hex);
    void bind_app_key (uint16_t unicast, uint16_t app_idx,
                       uint16_t model_id, uint16_t company_id);
    void group_add    (uint16_t unicast, uint16_t elem, uint16_t group,
                       uint16_t model,   uint16_t company);
    void group_delete (uint16_t unicast, uint16_t elem, uint16_t group,
                       uint16_t model,   uint16_t company);
    void model_pub_set(uint16_t unicast, uint16_t elem, uint16_t pub_addr,
                       uint16_t model,   uint16_t company,
                       uint8_t ttl = 0x07, uint8_t period = 0);

    // ── Generic On/Off ────────────────────────────────────────────────
    void on_off_get      (uint16_t unicast);
    void on_off_set      (uint16_t unicast, bool on, uint8_t tid);
    void on_off_set_unack(uint16_t unicast, bool on, uint8_t tid);

    // ── Sensor ────────────────────────────────────────────────────────
    void sensor_get(uint16_t unicast, uint16_t sensor_id = 0);

    // ── Actuator ──────────────────────────────────────────────────────
    void actuator_get      (uint16_t unicast);
    void actuator_set      (uint16_t unicast, uint16_t act_id,
                            uint16_t setpoint, uint8_t status);
    void actuator_set_unack(uint16_t unicast, uint16_t act_id,
                            uint16_t setpoint, uint8_t status);

    // ── Remote Provisioning (RPR) ─────────────────────────────────────
    void rpr_scan_get  (uint16_t server_addr);
    void rpr_scan_start(uint16_t server_addr, uint8_t limit,
                        uint8_t timeout, bool filter = false,
                        const uint8_t* uuid = nullptr);
    void rpr_scan_stop (uint16_t server_addr);
    void rpr_link_get  (uint16_t server_addr);
    void rpr_link_open (uint16_t server_addr, const uint8_t uuid[16],
                        bool timeout_en = false, uint8_t timeout = 0);
    void rpr_link_close(uint16_t server_addr, uint8_t reason = 0);
    void rpr_start_prov(uint16_t server_addr);

    // ── Config: Relay / Proxy / Friend / Heartbeat ────────────────────
    void relay_set    (uint16_t unicast, uint8_t state,
                       uint8_t rc = 0, uint8_t ri = 0);
    void relay_get    (uint16_t unicast);
    void proxy_set    (uint16_t unicast, uint8_t state);
    void proxy_get    (uint16_t unicast);
    void friend_set   (uint16_t unicast, uint8_t state);
    void friend_get   (uint16_t unicast);
    void heartbeat_sub(uint16_t unicast, uint16_t group);
    void heartbeat_pub(uint16_t unicast, uint16_t dst,
                       uint8_t period, uint8_t ttl);

    // ── DB Query API ──────────────────────────────────────────────────
    std::vector<DeviceRecord>          get_all_nodes();
    std::optional<DeviceRecord>        get_node(uint16_t unicast);
    std::vector<DeviceRecord>          get_nodes_by_status(const std::string& s);

    std::optional<SensorReadingRecord> get_latest_sensor(uint16_t unicast);
    std::vector<SensorReadingRecord>   get_sensor_history(uint16_t unicast,
                                                          int limit = 20);
    std::vector<SensorReadingRecord>   get_sensor_range(uint16_t unicast,
                                                        uint64_t from,
                                                        uint64_t to);
    std::optional<ActuatorStateRecord>  get_actuator_state(uint16_t unicast);
    std::vector<ActuatorCommandRecord>  get_actuator_commands(uint16_t unicast,
                                                               int limit = 20);
    std::vector<ActuatorCommandRecord>  get_pending_commands();

private:
    // ─────────────────────────────────────────────────────────────────
    // Dependencies (injected)
    // ─────────────────────────────────────────────────────────────────
    std::shared_ptr<IDatabase>           db;
    std::shared_ptr<IPC>                 ipc;

    // ─────────────────────────────────────────────────────────────────
    // Owned subsystems (khởi tạo trong constructor)
    // ─────────────────────────────────────────────────────────────────
    std::shared_ptr<ReliableTransport>   transport;
    std::shared_ptr<FrameCodec>          codec;
    std::shared_ptr<MeshCommandSender>   sender;
    std::shared_ptr<MeshEventDispatcher> dispatcher;
    std::shared_ptr<NodeRepository>      node_repo;
    std::shared_ptr<SensorRepository>    sensor_repo;
    std::shared_ptr<ActuatorRepository>  actuator_repo;

    // ─────────────────────────────────────────────────────────────────
    // Queue 1: RawFrame (rx_thread) ──► MeshFrame ──► worker_thread
    // ─────────────────────────────────────────────────────────────────
    std::queue<MeshFrame>   mesh_event_queue;
    std::mutex              mesh_mutex;
    std::condition_variable mesh_cv;

    // ─────────────────────────────────────────────────────────────────
    // Queue 2: IPC bytes (ipc_recv_thread) ──► cmd_worker_thread
    // ─────────────────────────────────────────────────────────────────
    std::queue<std::vector<uint8_t>> ipc_cmd_queue;
    std::mutex                       cmd_mutex;
    std::condition_variable          cmd_cv;

    // ─────────────────────────────────────────────────────────────────
    // Threads
    // ─────────────────────────────────────────────────────────────────
    std::atomic<bool> is_running { false };
    std::thread       worker_thread;      // mesh event → DB + IPC emit
    std::thread       cmd_worker_thread;  // IPC cmd → sender
    std::thread       keepalive_thread;   // periodic offline check
    std::thread       ipc_recv_thread;    // pump dbus event loop

    // ─────────────────────────────────────────────────────────────────
    // Private methods
    // ─────────────────────────────────────────────────────────────────
    void register_handlers();

    void worker_loop();       // tiêu thụ mesh_event_queue
    void cmd_worker_loop();   // tiêu thụ ipc_cmd_queue
    void keepalive_loop();    // mỗi 30s scan node offline
    void ipc_recv_loop();     // sd_bus_process loop

    data_status_t handle_ipc_command(const std::vector<uint8_t>& payload);
    void save_onoff_command   (uint16_t unicast, bool on, uint8_t tid);
    void save_actuator_command(uint16_t unicast, uint16_t act_id,
                               uint16_t setpoint, uint8_t status);

    // Event handlers — gọi từ dispatcher trong worker_thread
    void on_prov_enable_comp   (const MeshFrame& f);
    void on_recv_unprov_adv_pkt(const MeshFrame& f);
    void on_prov_complete      (const MeshFrame& f);
    void on_onoff_status       (const MeshFrame& f);
    void on_sensor_status      (const MeshFrame& f);
    void on_actuator_status    (const MeshFrame& f);
    void on_health_fault       (const MeshFrame& f);
    void on_cfg_timeout        (const MeshFrame& f);

    static bool     should_emit        (OpCode opcode);
    static bool     hex_to_bytes       (const std::string& hex,
                                        uint8_t* out, size_t len);
    static uint64_t now                ();
    std::string     node_id_from_unicast(uint16_t unicast);
};
