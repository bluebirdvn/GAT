#ifndef IDATABASE_HPP
#define IDATABASE_HPP

#include "data_status.hpp"
#include "db_record.hpp"
#include <string>
#include <vector>
#include <map>

class IDatabase {
public:
    virtual ~IDatabase() = default;

    virtual data_status_t init()    = 0;
    virtual data_status_t de_init() = 0;

    virtual data_status_t insert  (const DBRecord& record)                    = 0;
    virtual data_status_t update  (const DBRecord& record,
                                   const std::string& where)                  = 0;
    virtual data_status_t remove  (const std::string& table,
                                   const std::string& where)                  = 0;
    virtual data_status_t exec_raw(const std::string& sql)                    = 0;
    virtual std::vector<std::map<std::string, std::string>>
                          query   (const std::string& sql)                    = 0;
};

#endif
