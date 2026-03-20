#include "sensor_repository.hpp"

SensorRepository::SensorRepository(std::shared_ptr<IDatabase> db) : db(db) {}

SensorReadingRecord SensorRepository::row_to_record(
    const std::map<std::string, std::string>& row) {
    auto get = [&](const std::string& k, const std::string& def = "") {
        auto it = row.find(k); return it != row.end() ? it->second : def;
    };
    SensorReadingRecord r;
    r.id          = std::stoll (get("id",          "0"));
    r.node_id     = get("node_id");
    r.sensor_id   = (uint16_t)std::stoul(get("sensor_id",  "0"));
    r.temperature = std::stof (get("temperature",  "0"));
    r.humidity    = std::stof (get("humidity",     "0"));
    r.lux         = std::stof (get("lux",          "0"));
    r.motion      = get("motion") == "1";
    r.battery     = (uint8_t)std::stoul(get("battery", "100"));
    r.status      = (uint8_t)std::stoul(get("status",  "0"));
    r.timestamp   = std::stoull(get("timestamp",   "0"));
    return r;
}

std::string SensorRepository::node_id_from_unicast(uint16_t unicast) {
    auto rows = db->query("SELECT node_id FROM device WHERE unicast="
                          + std::to_string(unicast) + " LIMIT 1;");
    if (rows.empty()) return "";
    return rows[0].at("node_id");
}

data_status_t SensorRepository::save(const SensorReadingRecord& r) {
    return db->insert(r);
}

std::optional<SensorReadingRecord> SensorRepository::find_by_addr(
    uint16_t unicast) {
    auto rows = db->query(
        "SELECT s.* FROM sensor_reading s "
        "JOIN device d ON s.node_id=d.node_id "
        "WHERE d.unicast=" + std::to_string(unicast)
        + " ORDER BY s.timestamp DESC LIMIT 1;");
    if (rows.empty()) return std::nullopt;
    return row_to_record(rows[0]);
}

std::vector<SensorReadingRecord> SensorRepository::find_all() {
    auto rows = db->query(
        "SELECT * FROM sensor_reading ORDER BY timestamp DESC;");
    std::vector<SensorReadingRecord> result;
    for (auto& row : rows) result.push_back(row_to_record(row));
    return result;
}

std::vector<SensorReadingRecord> SensorRepository::find_latest(
    uint16_t unicast, int limit) {
    auto rows = db->query(
        "SELECT s.* FROM sensor_reading s "
        "JOIN device d ON s.node_id=d.node_id "
        "WHERE d.unicast=" + std::to_string(unicast)
        + " ORDER BY s.timestamp DESC LIMIT " + std::to_string(limit) + ";");
    std::vector<SensorReadingRecord> result;
    for (auto& row : rows) result.push_back(row_to_record(row));
    return result;
}

std::vector<SensorReadingRecord> SensorRepository::find_by_time_range(
    uint16_t unicast, uint64_t from, uint64_t to) {
    auto rows = db->query(
        "SELECT s.* FROM sensor_reading s "
        "JOIN device d ON s.node_id=d.node_id "
        "WHERE d.unicast=" + std::to_string(unicast)
        + " AND s.timestamp BETWEEN " + std::to_string(from)
        + " AND " + std::to_string(to)
        + " ORDER BY s.timestamp ASC;");
    std::vector<SensorReadingRecord> result;
    for (auto& row : rows) result.push_back(row_to_record(row));
    return result;
}

std::vector<SensorReadingRecord> SensorRepository::find_by_sensor_id(
    uint16_t unicast, uint16_t sensor_id, int limit) {
    auto rows = db->query(
        "SELECT s.* FROM sensor_reading s "
        "JOIN device d ON s.node_id=d.node_id "
        "WHERE d.unicast=" + std::to_string(unicast)
        + " AND s.sensor_id=" + std::to_string(sensor_id)
        + " ORDER BY s.timestamp DESC LIMIT " + std::to_string(limit) + ";");
    std::vector<SensorReadingRecord> result;
    for (auto& row : rows) result.push_back(row_to_record(row));
    return result;
}

data_status_t SensorRepository::delete_older_than(uint64_t timestamp) {
    return db->remove("sensor_reading",
                      "timestamp < " + std::to_string(timestamp));
}

int64_t SensorRepository::count_by_node(uint16_t unicast) {
    auto rows = db->query(
        "SELECT COUNT(*) as cnt FROM sensor_reading s "
        "JOIN device d ON s.node_id=d.node_id "
        "WHERE d.unicast=" + std::to_string(unicast) + ";");
    if (rows.empty()) return 0;
    auto it = rows[0].find("cnt");
    return it != rows[0].end() ? std::stoll(it->second) : 0;
}

data_status_t SensorRepository::remove(uint16_t unicast) {
    std::string nid = node_id_from_unicast(unicast);
    if (nid.empty()) return DB_NOT_FOUND;
    return db->remove("sensor_reading", "node_id='" + nid + "'");
}
