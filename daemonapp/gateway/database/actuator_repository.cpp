#include "actuator_repository.hpp"

ActuatorRepository::ActuatorRepository(std::shared_ptr<IDatabase> db) : db(db) {}

ActuatorStateRecord ActuatorRepository::row_to_state(
    const std::map<std::string, std::string>& row) {
    auto get = [&](const std::string& k, const std::string& def = "") {
        auto it = row.find(k); return it != row.end() ? it->second : def;
    };
    ActuatorStateRecord r;
    r.node_id          = get("node_id");
    r.actuator_id      = (uint16_t)std::stoul(get("actuator_id",      "0"));
    r.present_setpoint = std::stof(get("present_setpoint", "0"));
    r.target_setpoint  = std::stof(get("target_setpoint",  "0"));
    r.onoff            = get("onoff") == "1";
    r.status           = (uint8_t)std::stoul(get("status", "0"));
    r.updated_at       = std::stoull(get("updated_at", "0"));
    return r;
}

ActuatorCommandRecord ActuatorRepository::row_to_command(
    const std::map<std::string, std::string>& row) {
    auto get = [&](const std::string& k, const std::string& def = "") {
        auto it = row.find(k); return it != row.end() ? it->second : def;
    };
    ActuatorCommandRecord r;
    r.id          = std::stoll(get("id", "0"));
    r.node_id     = get("node_id");
    r.actuator_id = (uint16_t)std::stoul(get("actuator_id", "0"));
    r.setpoint    = std::stof(get("setpoint", "0"));
    r.onoff       = get("onoff") == "1";
    r.result      = get("result", "pending");
    r.issued_at   = std::stoull(get("issued_at", "0"));
    r.acked_at    = std::stoull(get("acked_at",  "0"));
    return r;
}

std::string ActuatorRepository::node_id_from_unicast(uint16_t unicast) {
    auto rows = db->query("SELECT node_id FROM device WHERE unicast="
                          + std::to_string(unicast) + " LIMIT 1;");
    if (rows.empty()) return "";
    return rows[0].at("node_id");
}

// ── actuator_state: UPSERT ────────────────────────────────────────────────────
data_status_t ActuatorRepository::save(const ActuatorStateRecord& r) {
    auto rows = db->query("SELECT node_id FROM actuator_state WHERE node_id='"
                          + r.node_id + "' LIMIT 1;");
    if (rows.empty()) return db->insert(r);
    return db->update(r, "node_id='" + r.node_id + "'");
}

std::optional<ActuatorStateRecord> ActuatorRepository::find_by_addr(
    uint16_t unicast) {
    auto rows = db->query(
        "SELECT a.* FROM actuator_state a "
        "JOIN device d ON a.node_id=d.node_id "
        "WHERE d.unicast=" + std::to_string(unicast) + " LIMIT 1;");
    if (rows.empty()) return std::nullopt;
    return row_to_state(rows[0]);
}

std::vector<ActuatorStateRecord> ActuatorRepository::find_all() {
    auto rows = db->query("SELECT * FROM actuator_state;");
    std::vector<ActuatorStateRecord> result;
    for (auto& row : rows) result.push_back(row_to_state(row));
    return result;
}

data_status_t ActuatorRepository::remove(uint16_t unicast) {
    std::string nid = node_id_from_unicast(unicast);
    if (nid.empty()) return DB_NOT_FOUND;
    db->remove("actuator_command", "node_id='" + nid + "'");
    return db->remove("actuator_state", "node_id='" + nid + "'");
}

// ── actuator_command: INSERT (audit trail) ────────────────────────────────────
data_status_t ActuatorRepository::save_command(const ActuatorCommandRecord& r) {
    return db->insert(r);
}

std::vector<ActuatorCommandRecord> ActuatorRepository::find_commands(
    uint16_t unicast, int limit) {
    auto rows = db->query(
        "SELECT c.* FROM actuator_command c "
        "JOIN device d ON c.node_id=d.node_id "
        "WHERE d.unicast=" + std::to_string(unicast)
        + " ORDER BY c.issued_at DESC LIMIT " + std::to_string(limit) + ";");
    std::vector<ActuatorCommandRecord> result;
    for (auto& row : rows) result.push_back(row_to_command(row));
    return result;
}

std::vector<ActuatorCommandRecord> ActuatorRepository::find_pending_commands() {
    auto rows = db->query(
        "SELECT * FROM actuator_command WHERE result='pending' "
        "ORDER BY issued_at ASC;");
    std::vector<ActuatorCommandRecord> result;
    for (auto& row : rows) result.push_back(row_to_command(row));
    return result;
}

data_status_t ActuatorRepository::update_command_result(int64_t id,
                                                         const std::string& result,
                                                         uint64_t acked_at) {
    return db->exec_raw(
        "UPDATE actuator_command SET result='" + result
        + "', acked_at=" + std::to_string(acked_at)
        + " WHERE id=" + std::to_string(id) + ";");
}
