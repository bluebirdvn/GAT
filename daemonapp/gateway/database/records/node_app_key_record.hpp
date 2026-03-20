#ifndef _NODE_APP_KEY_RECORD_HPP
#define _NODE_APP_KEY_RECORD_HPP

struct NodeAppKeyRecord : public DBRecord {
    int64_t     id         = 0;
    std::string node_id;
    uint16_t    app_idx    = 0;
    uint16_t    model_id   = 0xFFFF;
    uint16_t    company_id = 0xFFFF;

    std::string tableName() const override { return "node_app_key"; }
    std::map<std::string, std::string> toMap() const override {
        return {
            {"node_id",    node_id},
            {"app_idx",    std::to_string(app_idx)},
            {"model_id",   std::to_string(model_id)},
            {"company_id", std::to_string(company_id)}
        };
    }
};

#endif //_NODE_APP_KEY_RECORD_HPP