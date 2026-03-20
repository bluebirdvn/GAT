#ifndef SENSOR_REPOSITORY_HPP
#define SENSOR_REPOSITORY_HPP

#include "idatabase.hpp"
#include "irepository.hpp"
#include "records/sensor_reading_record.hpp"
#include <memory>
#include <optional>
#include <vector>

class SensorRepository : public IRepository<SensorReadingRecord> {
public:
    explicit SensorRepository(std::shared_ptr<IDatabase> db);

    data_status_t                      save(const SensorReadingRecord& r) override;
    std::optional<SensorReadingRecord> find_by_addr(uint16_t unicast)     override;
    std::vector<SensorReadingRecord>   find_all()                         override;
    data_status_t                      remove(uint16_t unicast)           override;

    std::vector<SensorReadingRecord> find_latest(uint16_t unicast,
                                                  int limit = 100);
    std::vector<SensorReadingRecord> find_by_time_range(uint16_t unicast,
                                                         uint64_t from,
                                                         uint64_t to);
    std::vector<SensorReadingRecord> find_by_sensor_id(uint16_t unicast,
                                                        uint16_t sensor_id,
                                                        int limit = 50);
    data_status_t delete_older_than(uint64_t timestamp);
    int64_t       count_by_node(uint16_t unicast);

private:
    std::shared_ptr<IDatabase> db;
    SensorReadingRecord row_to_record(
        const std::map<std::string, std::string>& row);
    std::string node_id_from_unicast(uint16_t unicast);
};

#endif
