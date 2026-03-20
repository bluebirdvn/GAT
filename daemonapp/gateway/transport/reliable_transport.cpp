// reliable_transport.cpp
#include "reliable_transport.hpp"
#include <iostream>

ReliableTransport::ReliableTransport(std::shared_ptr<ISerialPort> port)
    : port(port),
      parser(crc),
      ack_builder(crc),
      arq(port, ack_builder) {}

ReliableTransport::~ReliableTransport() { stop(); }

communication_status_t ReliableTransport::start() {
    auto st = port->open();
    if (st != COMMUNICATION_SUCCESS) return st;

    is_running = true;
    rx_thread  = std::thread(&ReliableTransport::rx_loop, this);
    tx_thread  = std::thread(&ReliableTransport::tx_loop, this);
    return COMMUNICATION_SUCCESS;
}

communication_status_t ReliableTransport::stop() {
    if (!is_running) return COMMUNICATION_SUCCESS;
    is_running = false;
    tx_cv.notify_all();
    if (rx_thread.joinable()) rx_thread.join();
    if (tx_thread.joinable()) tx_thread.join();
    port->close();
    return COMMUNICATION_SUCCESS;
}

void ReliableTransport::enqueue_frame(const std::vector<uint8_t>& raw_frame,
                                       bool reliable) {
    uint8_t seq = raw_frame.size() > FRAME_OFF_SEQ ? raw_frame[FRAME_OFF_SEQ] : 0;
    {
        std::lock_guard<std::mutex> lk(queue_mutex);
        tx_queue.push({ raw_frame, reliable, seq });
    }
    tx_cv.notify_one();
}

// ── RX Loop: đọc từng byte → parser → xử lý ACK/NACK/DATA ───────────────
void ReliableTransport::rx_loop() {
    uint8_t byte = 0;
    while (is_running.load()) {
        if (port->read_byte(byte) <= 0) continue;

        auto result = parser.feed(byte);
        if (!result) continue;

        RawFrame& f = *result;

        if (ack_builder.is_ack(f)) {
            arq.notify_ack(f.seq);                              // → ArqController

        } else if (ack_builder.is_nack(f)) {
            arq.notify_nack(f.seq);                             // → ArqController

        } else {
            // DATA frame hợp lệ
            auto ack = ack_builder.build_ack(f.seq);
            port->write_bytes(ack);                             // → SerialPort

            if (on_frame_cb) on_frame_cb(f);                   // → GatewayService
        }
    }
}

// ── TX Loop: lấy từ queue → gửi qua ARQ hoặc gửi thẳng ──────────────────
void ReliableTransport::tx_loop() {
    while (is_running.load()) {
        TxItem item;
        {
            std::unique_lock<std::mutex> lk(queue_mutex);
            tx_cv.wait(lk, [this] {
                return !tx_queue.empty() || !is_running.load();
            });
            if (!is_running.load()) break;
            item = tx_queue.front();
            tx_queue.pop();
        }

        if (item.reliable) {
            arq.send_reliable(item.frame, item.seq);            // → ArqController
        } else {
            port->write_bytes(item.frame);                      // → SerialPort trực tiếp
        }
    }
}
