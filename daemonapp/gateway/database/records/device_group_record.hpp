#ifndef DEVICE_GROUP_RECORD_HPP
#define DEVICE_GROUP_RECORD_HPP

#include "../db_record.hpp"
#include <cstdint>

struct DeviceGroupRecord : public DBRecord {
    int64_t     id         = 0;
    std::string node_id;
    uint16_t    group_addr = 0;
    uint16_t    model_id   = 0xFFFF;
    uint16_t    company_id = 0xFFFF;
    uint64_t    created_at = 0;

    std::string tableName() const override { return "device_group"; }
    std::map<std::string, std::string> toMap() const override {
        return {
            {"node_id",    node_id},
            {"group_addr", std::to_string(group_addr)},
            {"model_id",   std::to_string(model_id)},
            {"company_id", std::to_string(company_id)},
            {"created_at", std::to_string(created_at)}
        };
    }
};

#endif
