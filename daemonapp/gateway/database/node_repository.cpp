#include "node_repository.hpp"
#include <chrono>

NodeRepository::NodeRepository(std::shared_ptr<IDatabase> db) : db(db) {}

uint64_t NodeRepository::now() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

DeviceRecord NodeRepository::row_to_device(
    const std::map<std::string, std::string>& row) {
    auto get = [&](const std::string& k, const std::string& def = "") {
        auto it = row.find(k); return it != row.end() ? it->second : def;
    };
    DeviceRecord r;
    r.node_id          = get("node_id");
    r.unicast          = (uint16_t)std::stoul(get("unicast",   "0"));
    r.gateway_id       = get("gateway_id");
    r.name             = get("name");
    r.type             = get("type",    "unknown");
    r.status           = get("status",  "provisioned");
    r.uuid             = get("uuid");
    r.mac              = get("mac");
    r.net_idx          = (uint16_t)std::stoul(get("net_idx",   "0"));
    r.app_idx          = (uint16_t)std::stoul(get("app_idx",   "0"));
    r.elem_num         = (uint8_t )std::stoul(get("elem_num",  "1"));
    r.firmware_version = get("firmware_version");
    r.last_seen        = std::stoull(get("last_seen",  "0"));
    r.created_at       = std::stoull(get("created_at", "0"));
    return r;
}

DeviceGroupRecord NodeRepository::row_to_group(
    const std::map<std::string, std::string>& row) {
    auto get = [&](const std::string& k, const std::string& def = "") {
        auto it = row.find(k); return it != row.end() ? it->second : def;
    };
    DeviceGroupRecord r;
    r.id         = std::stoll (get("id",         "0"));
    r.node_id    = get("node_id");
    r.group_addr = (uint16_t)std::stoul(get("group_addr", "0"));
    r.model_id   = (uint16_t)std::stoul(get("model_id",   "65535"));
    r.company_id = (uint16_t)std::stoul(get("company_id", "65535"));
    r.created_at = std::stoull(get("created_at", "0"));
    return r;
}

// ── device ────────────────────────────────────────────────────────────────────
data_status_t NodeRepository::save(const DeviceRecord& r) {
    auto rows = db->query("SELECT node_id FROM device WHERE node_id='"
                          + r.node_id + "' LIMIT 1;");
    if (rows.empty()) {
        DeviceRecord rec = r;
        if (rec.created_at == 0) rec.created_at = now();
        if (rec.last_seen  == 0) rec.last_seen  = now();
        return db->insert(rec);
    }
    return db->update(r, "node_id='" + r.node_id + "'");
}

std::optional<DeviceRecord> NodeRepository::find_by_addr(uint16_t unicast) {
    auto rows = db->query("SELECT * FROM device WHERE unicast="
                          + std::to_string(unicast) + " LIMIT 1;");
    if (rows.empty()) return std::nullopt;
    return row_to_device(rows[0]);
}

std::optional<DeviceRecord> NodeRepository::find_by_node_id(
    const std::string& node_id) {
    auto rows = db->query("SELECT * FROM device WHERE node_id='"
                          + node_id + "' LIMIT 1;");
    if (rows.empty()) return std::nullopt;
    return row_to_device(rows[0]);
}

std::vector<DeviceRecord> NodeRepository::find_all() {
    auto rows = db->query("SELECT * FROM device ORDER BY created_at ASC;");
    std::vector<DeviceRecord> result;
    for (auto& row : rows) result.push_back(row_to_device(row));
    return result;
}

std::vector<DeviceRecord> NodeRepository::find_by_status(
    const std::string& status) {
    auto rows = db->query("SELECT * FROM device WHERE status='"
                          + status + "';");
    std::vector<DeviceRecord> result;
    for (auto& row : rows) result.push_back(row_to_device(row));
    return result;
}

std::vector<DeviceRecord> NodeRepository::find_by_type(
    const std::string& type) {
    auto rows = db->query("SELECT * FROM device WHERE type='"
                          + type + "';");
    std::vector<DeviceRecord> result;
    for (auto& row : rows) result.push_back(row_to_device(row));
    return result;
}

std::vector<DeviceRecord> NodeRepository::find_offline(
    uint64_t older_than_sec) {
    uint64_t threshold = now() - older_than_sec;
    auto rows = db->query(
        "SELECT * FROM device WHERE last_seen < "
        + std::to_string(threshold) + ";");
    std::vector<DeviceRecord> result;
    for (auto& row : rows) result.push_back(row_to_device(row));
    return result;
}

data_status_t NodeRepository::update_status(uint16_t unicast,
                                             const std::string& status) {
    return db->exec_raw("UPDATE device SET status='" + status
                        + "' WHERE unicast=" + std::to_string(unicast) + ";");
}

data_status_t NodeRepository::update_last_seen(uint16_t unicast, uint64_t ts) {
    return db->exec_raw("UPDATE device SET last_seen=" + std::to_string(ts)
                        + " WHERE unicast=" + std::to_string(unicast) + ";");
}

data_status_t NodeRepository::remove(uint16_t unicast) {
    return db->remove("device", "unicast=" + std::to_string(unicast));
}

// ── device_group ──────────────────────────────────────────────────────────────
data_status_t NodeRepository::add_to_group(const DeviceGroupRecord& r) {
    return db->insert(r);
}

data_status_t NodeRepository::remove_from_group(const std::string& node_id,
                                                  uint16_t group_addr) {
    return db->remove("device_group",
        "node_id='" + node_id
        + "' AND group_addr=" + std::to_string(group_addr));
}

std::vector<DeviceGroupRecord> NodeRepository::find_groups(
    const std::string& node_id) {
    auto rows = db->query("SELECT * FROM device_group WHERE node_id='"
                          + node_id + "' ORDER BY created_at ASC;");
    std::vector<DeviceGroupRecord> result;
    for (auto& row : rows) result.push_back(row_to_group(row));
    return result;
}

std::vector<DeviceRecord> NodeRepository::find_by_group(uint16_t group_addr) {
    auto rows = db->query(
        "SELECT d.* FROM device d "
        "JOIN device_group g ON d.node_id=g.node_id "
        "WHERE g.group_addr=" + std::to_string(group_addr) + ";");
    std::vector<DeviceRecord> result;
    for (auto& row : rows) result.push_back(row_to_device(row));
    return result;
}


// net_key
data_status_t NodeRepository::save_net_key(const NetKeyRecord& r) {
    auto rows = db->query("SELECT net_idx FROM net_key WHERE net_idx="
                          + std::to_string(r.net_idx) + " LIMIT 1;");
    if (rows.empty()) return db->insert(r);
    return db->update(r, "net_idx=" + std::to_string(r.net_idx));
}

std::vector<NetKeyRecord> NodeRepository::find_net_keys() {
    auto rows = db->query("SELECT * FROM net_key ORDER BY net_idx;");
    std::vector<NetKeyRecord> result;
    for (auto& row : rows) {
        NetKeyRecord r;
        r.net_idx    = (uint16_t)std::stoul(row["net_idx"]);
        r.name       = row["name"];
        r.created_at = std::stoull(row["created_at"]);
        result.push_back(r);
    }
    return result;
}

data_status_t NodeRepository::remove_net_key(uint16_t net_idx) {
    return db->remove("net_key", "net_idx=" + std::to_string(net_idx));
}

// app_key
data_status_t NodeRepository::save_app_key(const AppKeyRecord& r) {
    auto rows = db->query("SELECT app_idx FROM app_key WHERE app_idx="
                          + std::to_string(r.app_idx) + " LIMIT 1;");
    if (rows.empty()) return db->insert(r);
    return db->update(r, "app_idx=" + std::to_string(r.app_idx));
}

std::vector<AppKeyRecord> NodeRepository::find_app_keys(uint16_t net_idx) {
    auto rows = db->query("SELECT * FROM app_key WHERE net_idx="
                          + std::to_string(net_idx) + " ORDER BY app_idx;");
    std::vector<AppKeyRecord> result;
    for (auto& row : rows) {
        AppKeyRecord r;
        r.app_idx    = (uint16_t)std::stoul(row["app_idx"]);
        r.net_idx    = (uint16_t)std::stoul(row["net_idx"]);
        r.name       = row["name"];
        r.created_at = std::stoull(row["created_at"]);
        result.push_back(r);
    }
    return result;
}

data_status_t NodeRepository::remove_app_key(uint16_t app_idx) {
    return db->remove("app_key", "app_idx=" + std::to_string(app_idx));
}

// node_app_key
data_status_t NodeRepository::bind_app_key(const NodeAppKeyRecord& r) {
    return db->insert(r);  // INSERT OR IGNORE (UNIQUE constraint)
}

data_status_t NodeRepository::unbind_app_key(const std::string& node_id,
                                              uint16_t app_idx,
                                              uint16_t model_id) {
    return db->remove("node_app_key",
        "node_id='" + node_id
        + "' AND app_idx=" + std::to_string(app_idx)
        + " AND model_id=" + std::to_string(model_id));
}

std::vector<NodeAppKeyRecord> NodeRepository::find_bindings(
    const std::string& node_id) {
    auto rows = db->query(
        "SELECT * FROM node_app_key WHERE node_id='" + node_id + "';");
    std::vector<NodeAppKeyRecord> result;
    for (auto& row : rows) {
        NodeAppKeyRecord r;
        r.id         = std::stoll(row["id"]);
        r.node_id    = row["node_id"];
        r.app_idx    = (uint16_t)std::stoul(row["app_idx"]);
        r.model_id   = (uint16_t)std::stoul(row["model_id"]);
        r.company_id = (uint16_t)std::stoul(row["company_id"]);
        result.push_back(r);
    }
    return result;
}

std::vector<DeviceRecord> NodeRepository::find_nodes_by_app_key(
    uint16_t app_idx) {
    auto rows = db->query(
        "SELECT d.* FROM device d "
        "JOIN node_app_key k ON d.node_id=k.node_id "
        "WHERE k.app_idx=" + std::to_string(app_idx) + ";");
    std::vector<DeviceRecord> result;
    for (auto& row : rows) result.push_back(row_to_device(row));
    return result;
}

