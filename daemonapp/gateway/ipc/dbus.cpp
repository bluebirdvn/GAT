// dbus.cpp
#include "dbus.hpp"
#include <iostream>

DBus::DBus(const std::string& svc, const std::string& obj, const std::string& iface)
    : service_name(svc), object_path(obj), interface_name(iface) {}

DBus::~DBus() { de_init(); }

data_status_t DBus::init() {
    if (sd_bus_open_system(&bus) < 0) {
        std::cerr << "[DBus] Open fail\n";
        return FAILED;
    }
    return SUCCESS;
}

data_status_t DBus::de_init() {
    if (bus) { sd_bus_unref(bus); bus = nullptr; }
    return SUCCESS;
}

data_status_t DBus::send(const std::vector<uint8_t>& data) {
    sd_bus_message* msg = nullptr;

    int r = sd_bus_message_new_signal (bus, &msg, object_path.c_str(), interface_name.c_str(), "DataEvent");
    if (r < 0) {
        return FAILED;
    }

    r = sd_bus_message_append_array(msg, 'y', data.data(), data.size());
    if (r < 0) {
        sd_bus_message_unref(msg);
        return FAILED;
    }

    r = sd_bus_send(bus, msg, nullptr);
    sd_bus_message_unref(msg);
    return (r >= 0) ? SUCCESS : FAILED;
}

data_status_t DBus::receive(std::vector<uint8_t>& out) {
    int r = sd_bus_process(bus, nullptr);

    if (r == 0) {
        sd_bus_wait(bus, 100000);
    }

    if (last_received.empty()) {
        return FAILED;
    }

    out = std::move(last_received);
    last_received.clear();
    return SUCCESS;
}

data_status_t DBus::call_method(const std::string& method,
                                std::string service_name,
                                std::string object_path,
                                std::string interface_name,
                               const std::vector<uint8_t>& payload) {
    if (!bus) return FAILED;
    sd_bus_error error  = SD_BUS_ERROR_NULL;
    sd_bus_message* msg = nullptr;
    int ret = sd_bus_call_method(bus,
        service_name.c_str(), object_path.c_str(),
        interface_name.c_str(), method.c_str(),
        &error, &msg, "ay", payload.size(), payload.data());
    sd_bus_error_free(&error);
    if (msg) sd_bus_message_unref(msg);
    return (ret >= 0) ? SUCCESS : FAILED;
}
