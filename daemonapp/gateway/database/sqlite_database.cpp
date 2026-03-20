#include "sqlite_database.hpp"
#include <iostream>
#include <sstream>

SQLiteDatabase::SQLiteDatabase(const std::string& path) : db_path(path) {}
SQLiteDatabase::~SQLiteDatabase() { de_init(); }

data_status_t SQLiteDatabase::init() {
    std::lock_guard<std::mutex> lk(mutex);
    if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) {
        std::cerr << "[DB] Cannot open: " << sqlite3_errmsg(db) << "\n";
        return DB_FAILED;
    }
    sqlite3_exec(db, "PRAGMA journal_mode=WAL;",   nullptr, nullptr, nullptr);
    sqlite3_exec(db, "PRAGMA foreign_keys=ON;",    nullptr, nullptr, nullptr);
    sqlite3_exec(db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);
    create_tables();
    std::cout << "[DB] Opened: " << db_path << "\n";
    return DB_SUCCESS;
}

data_status_t SQLiteDatabase::de_init() {
    std::lock_guard<std::mutex> lk(mutex);
    if (db) { sqlite3_close(db); db = nullptr; }
    return DB_SUCCESS;
}

void SQLiteDatabase::create_tables() {
    const char* sql = R"(
    CREATE TABLE IF NOT EXISTS gateway (
        id         TEXT    PRIMARY KEY,
        name       TEXT    NOT NULL DEFAULT 'Gateway',
        location   TEXT    NOT NULL DEFAULT '',
        version    TEXT    NOT NULL DEFAULT '1.0.0',
        created_at INTEGER NOT NULL DEFAULT (strftime('%s','now'))
    );

    CREATE TABLE IF NOT EXISTS device (
        node_id          TEXT    PRIMARY KEY,
        unicast          INTEGER NOT NULL UNIQUE,
        gateway_id       TEXT    NOT NULL DEFAULT 'gw_default',
        name             TEXT    NOT NULL DEFAULT '',
        type             TEXT    NOT NULL DEFAULT 'unknown',
        status           TEXT    NOT NULL DEFAULT 'provisioned',
        uuid             TEXT    NOT NULL DEFAULT '',
        mac              TEXT    NOT NULL DEFAULT '',
        net_idx          INTEGER NOT NULL DEFAULT 0,
        app_idx          INTEGER NOT NULL DEFAULT 0,
        elem_num         INTEGER NOT NULL DEFAULT 1,
        firmware_version TEXT    NOT NULL DEFAULT '',
        last_seen        INTEGER NOT NULL DEFAULT 0,
        created_at       INTEGER NOT NULL DEFAULT (strftime('%s','now'))
    );
    CREATE INDEX IF NOT EXISTS idx_device_status ON device(status);

    CREATE TABLE IF NOT EXISTS device_group (
        id         INTEGER PRIMARY KEY AUTOINCREMENT,
        node_id    TEXT    NOT NULL REFERENCES device(node_id) ON DELETE CASCADE,
        group_addr INTEGER NOT NULL,
        model_id   INTEGER NOT NULL DEFAULT 65535,
        company_id INTEGER NOT NULL DEFAULT 65535,
        created_at INTEGER NOT NULL DEFAULT (strftime('%s','now')),
        UNIQUE(node_id, group_addr, model_id)
    );

    CREATE TABLE IF NOT EXISTS sensor_reading (
        id          INTEGER PRIMARY KEY AUTOINCREMENT,
        node_id     TEXT    NOT NULL REFERENCES device(node_id) ON DELETE CASCADE,
        sensor_id   INTEGER NOT NULL DEFAULT 0,
        temperature REAL    NOT NULL DEFAULT 0,
        humidity    REAL    NOT NULL DEFAULT 0,
        lux         REAL    NOT NULL DEFAULT 0,
        motion      INTEGER NOT NULL DEFAULT 0,
        battery     INTEGER NOT NULL DEFAULT 100,
        status      INTEGER NOT NULL DEFAULT 0,
        timestamp   INTEGER NOT NULL DEFAULT (strftime('%s','now'))
    );
    CREATE INDEX IF NOT EXISTS idx_sensor_node ON sensor_reading(node_id);
    CREATE INDEX IF NOT EXISTS idx_sensor_time ON sensor_reading(timestamp);

    CREATE TABLE IF NOT EXISTS actuator_state (
        node_id          TEXT PRIMARY KEY REFERENCES device(node_id) ON DELETE CASCADE,
        actuator_id      INTEGER NOT NULL DEFAULT 0,
        present_setpoint REAL    NOT NULL DEFAULT 0,
        target_setpoint  REAL    NOT NULL DEFAULT 0,
        onoff            INTEGER NOT NULL DEFAULT 0,
        status           INTEGER NOT NULL DEFAULT 0,
        updated_at       INTEGER NOT NULL DEFAULT (strftime('%s','now'))
    );

    CREATE TABLE IF NOT EXISTS actuator_command (
        id          INTEGER PRIMARY KEY AUTOINCREMENT,
        node_id     TEXT    NOT NULL REFERENCES device(node_id) ON DELETE CASCADE,
        actuator_id INTEGER NOT NULL DEFAULT 0,
        setpoint    REAL    NOT NULL DEFAULT 0,
        onoff       INTEGER NOT NULL DEFAULT 0,
        result      TEXT    NOT NULL DEFAULT 'pending',
        issued_at   INTEGER NOT NULL DEFAULT (strftime('%s','now')),
        acked_at    INTEGER NOT NULL DEFAULT 0
    );
    CREATE INDEX IF NOT EXISTS idx_cmd_node   ON actuator_command(node_id);
    CREATE INDEX IF NOT EXISTS idx_cmd_result ON actuator_command(result);

    CREATE TABLE IF NOT EXISTS net_key (
        net_idx    INTEGER PRIMARY KEY,   -- 0, 1, 2...
        name       TEXT    NOT NULL DEFAULT '',
        created_at INTEGER NOT NULL DEFAULT (strftime('%s','now'))
    );

    -- Lưu danh sách AppKey và mapping với NetKey
    CREATE TABLE IF NOT EXISTS app_key (
        app_idx    INTEGER PRIMARY KEY,   -- 0, 1, 2...
        net_idx    INTEGER NOT NULL REFERENCES net_key(net_idx),
        name       TEXT    NOT NULL DEFAULT '',
        created_at INTEGER NOT NULL DEFAULT (strftime('%s','now'))
    );

    -- Mapping node ↔ AppKey (1 node bind nhiều AppKey)
    CREATE TABLE IF NOT EXISTS node_app_key (
        id         INTEGER PRIMARY KEY AUTOINCREMENT,
        node_id    TEXT    NOT NULL REFERENCES device(node_id) ON DELETE CASCADE,
        app_idx    INTEGER NOT NULL REFERENCES app_key(app_idx),
        model_id   INTEGER NOT NULL DEFAULT 65535,
        company_id INTEGER NOT NULL DEFAULT 65535,
        UNIQUE(node_id, app_idx, model_id)
    );
    )";

    char* err = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "[DB] create_tables error: " << err << "\n";
        sqlite3_free(err);
    }
}

std::string SQLiteDatabase::build_insert_sql(const DBRecord& record) {
    auto m = record.toMap();
    std::ostringstream cols, vals;
    bool first = true;
    for (auto& [k, v] : m) {
        if (!first) { cols << ","; vals << ","; }
        cols << k;
        vals << "'" << v << "'";
        first = false;
    }
    return "INSERT OR IGNORE INTO " + record.tableName()
         + "(" + cols.str() + ") VALUES(" + vals.str() + ");";
}

std::string SQLiteDatabase::build_update_sql(const DBRecord& record,
                                              const std::string& where) {
    auto m = record.toMap();
    std::ostringstream set;
    bool first = true;
    for (auto& [k, v] : m) {
        if (!first) set << ",";
        set << k << "='" << v << "'";
        first = false;
    }
    return "UPDATE " + record.tableName()
         + " SET " + set.str()
         + " WHERE " + where + ";";
}

data_status_t SQLiteDatabase::insert(const DBRecord& record) {
    return exec_raw(build_insert_sql(record));
}

data_status_t SQLiteDatabase::update(const DBRecord& record,
                                      const std::string& where) {
    return exec_raw(build_update_sql(record, where));
}

data_status_t SQLiteDatabase::remove(const std::string& table,
                                      const std::string& where) {
    return exec_raw("DELETE FROM " + table + " WHERE " + where + ";");
}

data_status_t SQLiteDatabase::exec_raw(const std::string& sql) {
    std::lock_guard<std::mutex> lk(mutex);
    char* err = nullptr;
    if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "[DB] exec error: " << err << "\nSQL: " << sql << "\n";
        sqlite3_free(err);
        return DB_FAILED;
    }
    return DB_SUCCESS;
}

int SQLiteDatabase::query_callback(void* data, int argc,
                                    char** argv, char** col_names) {
    auto* rows = static_cast<
        std::vector<std::map<std::string, std::string>>*>(data);
    std::map<std::string, std::string> row;
    for (int i = 0; i < argc; i++)
        row[col_names[i]] = argv[i] ? argv[i] : "";
    rows->push_back(row);
    return 0;
}

std::vector<std::map<std::string, std::string>>
SQLiteDatabase::query(const std::string& sql) {
    std::lock_guard<std::mutex> lk(mutex);
    std::vector<std::map<std::string, std::string>> rows;
    char* err = nullptr;
    if (sqlite3_exec(db, sql.c_str(), query_callback, &rows, &err) != SQLITE_OK) {
        std::cerr << "[DB] query error: " << err << "\nSQL: " << sql << "\n";
        sqlite3_free(err);
    }
    return rows;
}
