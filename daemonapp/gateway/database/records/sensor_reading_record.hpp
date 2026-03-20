#ifndef SENSOR_READING_RECORD_HPP
#define SENSOR_READING_RECORD_HPP

#include "../db_record.hpp"
#include <cstdint>

struct SensorReadingRecord : public DBRecord {
    int64_t     id          = 0;
    std::string node_id;
    uint16_t    sensor_id   = 0;
    float       temperature = 0.0f;
    float       humidity    = 0.0f;
    float       lux         = 0.0f;
    bool        motion      = false;
    uint8_t     battery     = 100;
    uint8_t     status      = 0;
    uint64_t    timestamp   = 0;

    std::string tableName() const override { return "sensor_reading"; }
    std::map<std::string, std::string> toMap() const override {
        return {
            {"node_id",     node_id},
            {"sensor_id",   std::to_string(sensor_id)},
            {"temperature", std::to_string(temperature)},
            {"humidity",    std::to_string(humidity)},
            {"lux",         std::to_string(lux)},
            {"motion",      std::to_string(motion ? 1 : 0)},
            {"battery",     std::to_string(battery)},
            {"status",      std::to_string(status)},
            {"timestamp",   std::to_string(timestamp)}
        };
    }
};

#endif
