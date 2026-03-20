#ifndef DB_RECORD_HPP
#define DB_RECORD_HPP

#include <string>
#include <map>

class DBRecord {
public:
    virtual ~DBRecord() = default;
    virtual std::string tableName() const = 0;
    virtual std::map<std::string, std::string> toMap() const = 0;
};

#endif
