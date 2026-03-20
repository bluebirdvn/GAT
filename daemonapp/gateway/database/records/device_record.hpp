#ifndef DEVICE_RECORD_HPP
#define DEVICE_RECORD_HPP

#include "../db_record.hpp"
#include <cstdint>

struct DeviceRecord : public DBRecord {
    std::string node_id;
    uint16_t    unicast          = 0;
    std::string gateway_id;
    std::string name;
    std::string type             = "unknown";
    std::string status           = "provisioned";
    std::string uuid;
    std::string mac;
    uint16_t    net_idx          = 0;
    uint16_t    app_idx          = 0;
    uint8_t     elem_num         = 1;
    std::string firmware_version;
    uint64_t    last_seen        = 0;
    uint64_t    created_at       = 0;

    std::string tableName() const override { return "device"; }
    std::map<std::string, std::string> toMap() const override {
        return {
            {"node_id",          node_id},
            {"unicast",          std::to_string(unicast)},
            {"gateway_id",       gateway_id},
            {"name",             name},
            {"type",             type},
            {"status",           status},
            {"uuid",             uuid},
            {"mac",              mac},
            {"net_idx",          std::to_string(net_idx)},
            {"app_idx",          std::to_string(app_idx)},
            {"elem_num",         std::to_string(elem_num)},
            {"firmware_version", firmware_version},
            {"last_seen",        std::to_string(last_seen)},
            {"created_at",       std::to_string(created_at)}
        };
    }
};

#endif
