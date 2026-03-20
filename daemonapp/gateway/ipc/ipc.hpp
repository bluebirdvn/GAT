#ifndef _IPC_HPP
#define _IPC_HPP
#include "../database/data_status.hpp"
#include <vector>
#include <cstdint>

class IPC {
public:
    virtual ~IPC() = default;
    virtual data_status_t init()   = 0;
    virtual data_status_t de_init()= 0;
    virtual data_status_t send(const std::vector<uint8_t>& data) = 0;
    virtual data_status process() = 0;
    virtual data_status_t receive(std::vector<uint8_t>& out)     = 0;
    
};
#endif
