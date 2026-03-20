#ifndef ACTUATOR_COMMAND_RECORD_HPP
#define ACTUATOR_COMMAND_RECORD_HPP

#include "../db_record.hpp"
#include <cstdint>

struct ActuatorCommandRecord : public DBRecord {
    int64_t     id          = 0;
    std::string node_id;
    uint16_t    actuator_id = 0;
    float       setpoint    = 0.0f;
    bool        onoff       = false;
    std::string result      = "pending";
    uint64_t    issued_at   = 0;
    uint64_t    acked_at    = 0;

    std::string tableName() const override { return "actuator_command"; }
    std::map<std::string, std::string> toMap() const override {
        return {
            {"node_id",     node_id},
            {"actuator_id", std::to_string(actuator_id)},
            {"setpoint",    std::to_string(setpoint)},
            {"onoff",       std::to_string(onoff ? 1 : 0)},
            {"result",      result},
            {"issued_at",   std::to_string(issued_at)},
            {"acked_at",    std::to_string(acked_at)}
        };
    }
};

#endif
