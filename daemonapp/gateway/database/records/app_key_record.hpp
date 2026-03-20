#ifndef _APP_KEY_RECORD_HPP
#define _APP_KEY_RECORD_HPP


struct AppKeyRecord : public DBRecord {
    uint16_t    app_idx    = 0;
    uint16_t    net_idx    = 0;
    std::string name;
    uint64_t    created_at = 0;

    std::string tableName() const override { return "app_key"; }
    std::map<std::string, std::string> toMap() const override {
        return {
            {"app_idx",    std::to_string(app_idx)},
            {"net_idx",    std::to_string(net_idx)},
            {"name",       name},
            {"created_at", std::to_string(created_at)}
        };
    }
};

#endif //_APP_KEY_RECORD_HPP