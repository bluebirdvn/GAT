#ifndef ACTUATOR_STATE_RECORD_HPP
#define ACTUATOR_STATE_RECORD_HPP

#include "../db_record.hpp"
#include <cstdint>

struct ActuatorStateRecord : public DBRecord {
    std::string node_id;
    uint16_t    actuator_id      = 0;
    float       present_setpoint = 0.0f;
    float       target_setpoint  = 0.0f;
    bool        onoff            = false;
    uint8_t     status           = 0;
    uint64_t    updated_at       = 0;

    std::string tableName() const override { return "actuator_state"; }
    std::map<std::string, std::string> toMap() const override {
        return {
            {"node_id",          node_id},
            {"actuator_id",      std::to_string(actuator_id)},
            {"present_setpoint", std::to_string(present_setpoint)},
            {"target_setpoint",  std::to_string(target_setpoint)},
            {"onoff",            std::to_string(onoff ? 1 : 0)},
            {"status",           std::to_string(status)},
            {"updated_at",       std::to_string(updated_at)}
        };
    }
};

#endif
