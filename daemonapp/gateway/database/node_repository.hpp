#ifndef NODE_REPOSITORY_HPP
#define NODE_REPOSITORY_HPP

#include "idatabase.hpp"
#include "irepository.hpp"
#include "records/device_record.hpp"
#include "records/device_group_record.hpp"
#include "records/net_key_record.hpp"
#include "records/app_key_record.hpp"
#include "records/node_app_key_record.hpp"

#include <memory>
#include <optional>
#include <vector>

class NodeRepository : public IRepository<DeviceRecord> {
public:
    explicit NodeRepository(std::shared_ptr<IDatabase> db);

    // ── device table ───────────────────────────────────────────────────
    data_status_t               save(const DeviceRecord& r)        override;
    std::optional<DeviceRecord> find_by_addr(uint16_t unicast)     override;
    std::vector<DeviceRecord>   find_all()                         override;
    data_status_t               remove(uint16_t unicast)           override;

    std::optional<DeviceRecord> find_by_node_id(const std::string& node_id);
    std::vector<DeviceRecord>   find_by_status(const std::string& status);
    std::vector<DeviceRecord>   find_by_type(const std::string& type);
    std::vector<DeviceRecord>   find_offline(uint64_t older_than_sec);
    data_status_t               update_status(uint16_t unicast,
                                              const std::string& status);
    data_status_t               update_last_seen(uint16_t unicast,
                                                  uint64_t ts);

    // ── device_group table ─────────────────────────────────────────────
    data_status_t                  add_to_group(const DeviceGroupRecord& r);
    data_status_t                  remove_from_group(const std::string& node_id,
                                                     uint16_t group_addr);
    std::vector<DeviceGroupRecord> find_groups(const std::string& node_id);
    std::vector<DeviceRecord>      find_by_group(uint16_t group_addr);
    data_status_t             save_net_key(const NetKeyRecord& r);
    std::vector<NetKeyRecord> find_net_keys();
    data_status_t             remove_net_key(uint16_t net_idx);

    // ── app_key table ──────────────────────────────────────────────────
    data_status_t             save_app_key(const AppKeyRecord& r);
    std::vector<AppKeyRecord> find_app_keys(uint16_t net_idx);
    std::vector<AppKeyRecord> find_all_app_keys();
    data_status_t             remove_app_key(uint16_t app_idx);

    // ── node_app_key table ─────────────────────────────────────────────
    data_status_t                  bind_app_key(const NodeAppKeyRecord& r);
    data_status_t                  unbind_app_key(const std::string& node_id,
                                                   uint16_t app_idx,
                                                   uint16_t model_id);
    std::vector<NodeAppKeyRecord>  find_bindings(const std::string& node_id);
    std::vector<DeviceRecord>      find_nodes_by_app_key(uint16_t app_idx);
    
private:
    std::shared_ptr<IDatabase> db;
    DeviceRecord      row_to_device(const std::map<std::string,std::string>& r);
    DeviceGroupRecord row_to_group (const std::map<std::string,std::string>& r);
    static uint64_t   now();
};

#endif
