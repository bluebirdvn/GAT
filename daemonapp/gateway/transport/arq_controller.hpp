#ifndef ARQ_CONTROLLER_HPP
#define ARQ_CONTROLLER_HPP

#include "iserial_port.hpp"
#include "ack_builder.hpp"
#include <mutex>
#include <condition_variable>
#include <memory>

#define ARQ_MAX_RETRY   3
#define ARQ_TIMEOUT_MS  500

class ArqController {
public:
    ArqController(std::shared_ptr<ISerialPort> port,
                  const AckBuilder& ack_builder);

    communication_status_t send_reliable(const std::vector<uint8_t>& frame,
                                          uint8_t seq);

    void notify_ack (uint8_t seq);
    void notify_nack(uint8_t seq);

private:
    std::shared_ptr<ISerialPort> port;
    const AckBuilder&            ack_builder;
    std::mutex                   ack_mutex;
    std::condition_variable      ack_cv;
    uint8_t                      last_acked  { 0xFF };
    uint8_t                      last_nacked { 0xFF };
};

#endif
