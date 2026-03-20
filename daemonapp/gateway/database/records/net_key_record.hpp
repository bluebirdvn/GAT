#ifndef _NET_KEY_RECORD_HPP
#define _NET_KEY_RECORD_HPP


struct NetKeyRecord : public DBRecord {
    uint16_t    net_idx    = 0;
    std::string name;
    uint64_t    created_at = 0;

    std::string tableName() const override { return "net_key"; }
    std::map<std::string, std::string> toMap() const override {
        return {
            {"net_idx",    std::to_string(net_idx)},
            {"name",       name},
            {"created_at", std::to_string(created_at)}
        };
    }
};

#endif