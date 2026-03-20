#ifndef IREPOSITORY_HPP
#define IREPOSITORY_HPP

#include "data_status.hpp"
#include <cstdint>
#include <optional>
#include <vector>

template<typename T>
class IRepository {
public:
    virtual ~IRepository() = default;
    virtual data_status_t    save(const T& record)          = 0;
    virtual std::optional<T> find_by_addr(uint16_t unicast) = 0;
    virtual std::vector<T>   find_all()                     = 0;
    virtual data_status_t    remove(uint16_t unicast)       = 0;
};

#endif
