#ifndef ACTUATOR_REPOSITORY_HPP
#define ACTUATOR_REPOSITORY_HPP

#include "idatabase.hpp"
#include "irepository.hpp"
#include "records/actuator_state_record.hpp"
#include "records/actuator_command_record.hpp"
#include <memory>
#include <optional>
#include <vector>

class ActuatorRepository : public IRepository<ActuatorStateRecord> {
public:
    explicit ActuatorRepository(std::shared_ptr<IDatabase> db);

    // ── actuator_state table ───────────────────────────────────────────
    data_status_t                      save(const ActuatorStateRecord& r) override;
    std::optional<ActuatorStateRecord> find_by_addr(uint16_t unicast)     override;
    std::vector<ActuatorStateRecord>   find_all()                         override;
    data_status_t                      remove(uint16_t unicast)           override;

    // ── actuator_command table ─────────────────────────────────────────
    data_status_t                       save_command(const ActuatorCommandRecord& r);
    std::vector<ActuatorCommandRecord>  find_commands(uint16_t unicast,
                                                       int limit = 50);
    std::vector<ActuatorCommandRecord>  find_pending_commands();
    data_status_t                       update_command_result(
                                            int64_t id,
                                            const std::string& result,
                                            uint64_t acked_at = 0);

private:
    std::shared_ptr<IDatabase> db;
    ActuatorStateRecord   row_to_state  (const std::map<std::string,std::string>& r);
    ActuatorCommandRecord row_to_command(const std::map<std::string,std::string>& r);
    std::string node_id_from_unicast(uint16_t unicast);
};

#endif
