#ifndef SQLITE_DATABASE_HPP
#define SQLITE_DATABASE_HPP

#include "idatabase.hpp"
#include <sqlite3.h>
#include <mutex>
#include <string>

class SQLiteDatabase : public IDatabase {
public:
    explicit SQLiteDatabase(const std::string& db_path);
    ~SQLiteDatabase() override;

    data_status_t init()    override;
    data_status_t de_init() override;

    data_status_t insert  (const DBRecord& record)           override;
    data_status_t update  (const DBRecord& record,
                           const std::string& where)         override;
    data_status_t remove  (const std::string& table,
                           const std::string& where)         override;
    data_status_t exec_raw(const std::string& sql)           override;
    std::vector<std::map<std::string, std::string>>
                  query   (const std::string& sql)           override;

private:
    std::string db_path;
    sqlite3*    db { nullptr };
    std::mutex  mutex;

    void        create_tables();
    std::string build_insert_sql(const DBRecord& record);
    std::string build_update_sql(const DBRecord& record,
                                 const std::string& where);

    static int query_callback(void* data, int argc,
                               char** argv, char** col_names);
};

#endif
