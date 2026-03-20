#include "transport/serial_port_config.hpp"
#include "transport/reliable_transport.hpp"
#include "protocol/frame_codec.hpp"
#include "mesh/mesh_command_sender.hpp"
#include "mesh/mesh_event_dispatcher.hpp"
#include "database/sqlite_database.hpp"
#include "database/node_repository.hpp"
#include "database/sensor_repository.hpp"
#include "database/fota_repository.hpp"
#include "ipc/dbus.hpp"
#include "service/gateway_service.hpp"
#include <csignal>
#include <atomic>
#include <thread>

static std::atomic<bool> g_running { true };
void on_signal(int) { g_running = false; }

int main() {
    signal(SIGINT,  on_signal);
    signal(SIGTERM, on_signal);

    // ── Transport ───────────────────────────────────────────────────────
    auto serial = std::make_shared<SerialPort>(
        SerialPortConfig("/dev/ttyUSB0")
            .set_baudrate(BAUDRATE_115200)
            .set_data_bit(DATA_BIT_8)
            .set_parity(PARITY_NONE)
            .set_stop_bit(STOP_ONE)
            .set_flow_control(FLOW_CONTROL_NONE)
            .build()
    );
    auto transport = std::make_shared<ReliableTransport>(serial);

    // ── Protocol + Mesh ─────────────────────────────────────────────────
    auto codec      = std::make_shared<FrameCodec>();
    auto sender     = std::make_shared<MeshCommandSender>(transport, codec);
    auto dispatcher = std::make_shared<MeshEventDispatcher>();

    // ── Database ────────────────────────────────────────────────────────
    auto db = std::make_shared<SQLiteDatabase>("/var/db/gateway.db");
    db->init();
    auto node_repo   = std::make_shared<NodeRepository>(db);
    auto sensor_repo = std::make_shared<SensorRepository>(db);
    auto fota_repo   = std::make_shared<FotaRepository>(db);

    // ── IPC ─────────────────────────────────────────────────────────────
    auto dbus = std::make_shared<DBus>(
        "com.gateway.service",
        "/com/gateway/mesh",
        "com.gateway.Mesh"
    );
    dbus->init();

    // ── Service ─────────────────────────────────────────────────────────
    GatewayService gateway(transport, codec, sender, dispatcher,
                           node_repo, sensor_repo, fota_repo, dbus);
    gateway.start();

    while (g_running.load())
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

    gateway.stop();
    db->de_init();
    dbus->de_init();
    return 0;
}
