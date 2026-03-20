// dbus.hpp
#ifndef _DBUS_HPP
#define _DBUS_HPP

#include "ipc.hpp"
#include <systemd/sd-bus.h>
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

// Callback khi nhận được method call từ bên ngoài
// param: payload từ caller, out: reply payload
using MethodHandler = std::function<data_status_t(
    const std::vector<uint8_t>& payload,
    std::vector<uint8_t>&       reply)>;

// Callback khi nhận được signal từ bên ngoài (subscribe)
using SignalHandler = std::function<void(
    const std::vector<uint8_t>& payload)>;

class DBus : public IPC {
public:
    DBus(const std::string& service_name,
         const std::string& object_path,
         const std::string& interface_name);
    ~DBus() override;

    // Khởi tạo bus, request service name
    data_status_t init()    override;
    data_status_t de_init() override;

    // Emit signal lên bus (broadcast tới tất cả subscriber)
    // signal_name: tên signal, ví dụ "SensorStatus"
    // payload:     dữ liệu đính kèm (serialized)
    data_status_t emit_signal(const std::string&          signal_name,
                              const std::vector<uint8_t>& payload);

    // Expose một method để process khác có thể gọi vào
    // Phải gọi trước init() hoặc sau khi có bus
    data_status_t expose_method(const std::string& method_name,
                                MethodHandler      handler);

    // Subscribe signal từ service khác
    data_status_t subscribe_signal(const std::string& sender_service,
                                   const std::string& sender_path,
                                   const std::string& sender_iface,
                                   const std::string& signal_name,
                                   SignalHandler      handler);

    // Gọi method của process khác, nhận reply đồng bộ
    data_status_t call_method(const std::string&          dest_service,
                              const std::string&          dest_path,
                              const std::string&          dest_iface,
                              const std::string&          method_name,
                              const std::vector<uint8_t>& payload,
                              std::vector<uint8_t>&       reply_out);

    // Gọi trong event loop — xử lý tất cả pending message
    // Trả về DB_SUCCESS nếu có message được xử lý
    data_status_t process();

    // Trả về fd để integrate với epoll/select nếu cần
    int get_fd() const;

private:
    std::string service_name_;
    std::string object_path_;
    std::string interface_name_;

    sd_bus*          bus_     = nullptr;
    sd_bus_slot*     slot_    = nullptr;  // vtable slot

    // method_name -> handler
    std::unordered_map<std::string, MethodHandler> method_handlers_;

    // match_string -> handler (cho subscribe_signal)
    std::unordered_map<std::string, SignalHandler> signal_handlers_;

    // sd-bus vtable callback (static, bridge vào method_handlers_)
    static int on_method_call(sd_bus_message* msg,
                              void*           userdata,
                              sd_bus_error*   err);

    // sd-bus match callback (static, bridge vào signal_handlers_)
    static int on_signal_match(sd_bus_message* msg,
                               void*           userdata,
                               sd_bus_error*   err);

    // Helper: đọc payload từ sd_bus_message ("ay" format)
    static data_status_t read_payload(sd_bus_message*       msg,
                                      std::vector<uint8_t>& out);

    // Helper: ghi payload vào sd_bus_message ("ay" format)
    static data_status_t write_payload(sd_bus_message*             msg,
                                       const std::vector<uint8_t>& data);
};

#endif // _DBUS_HPP