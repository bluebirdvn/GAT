# Gateway Control System - Hướng dẫn triển khai chi tiết

## 📋 Tổng quan dự án

**Mục tiêu**: Xây dựng hệ thống Gateway IoT toàn diện cho việc thu thập, xử lý và đồng bộ dữ liệu từ các thiết bị công nghiệp.

**Thời gian**: 20 tuần (~5 tháng)  
**Effort**: 555 giờ (6h/ngày, 5 ngày/tuần)  
**Tech Stack**: C++17, Qt5/6, CMake, SQLite, Modbus, RS485, MQTT, Node.js, React

---

## Phase 1: Foundation & Setup (2 tuần - 60 giờ)

### 1. Setup môi trường dev (8h)

**Mục tiêu**: Cài đặt và cấu hình các công cụ cần thiết

**Các bước thực hiện**:
```bash
# 1. Cài đặt build tools
sudo apt install build-essential cmake git

# 2. Cài đặt Qt (chọn 1 trong 2)
# Option A: Qt từ package manager
sudo apt install qt5-default qtcreator

# Option B: Qt từ installer (khuyến nghị)
# Download từ qt.io và cài Qt 5.15 hoặc 6.x

# 3. Cài đặt các thư viện cần thiết
sudo apt install libsqlite3-dev libssl-dev

# 4. Clone và setup spdlog
git clone https://github.com/gabime/spdlog.git external/spdlog

# 5. Clone và setup các thư viện Modbus
git clone https://github.com/stephane/libmodbus.git external/libmodbus
```

**Kiểm tra**:
- Chạy được `cmake --version`, `qmake --version`
- Qt Creator mở được và tạo được project mẫu
- Compile được chương trình Qt "Hello World"

---

### 2. Thiết kế database schema (6h)

**Mục tiêu**: Thiết kế cấu trúc database để lưu trữ dữ liệu thiết bị, cấu hình, và lịch sử

**Schema cơ bản**:
```sql
-- devices table
CREATE TABLE devices (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    device_type TEXT NOT NULL,  -- 'modbus', 'rs485', 'canbus'
    address TEXT NOT NULL,      -- địa chỉ thiết bị
    protocol_config TEXT,       -- JSON config
    enabled INTEGER DEFAULT 1,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- sensor_data table
CREATE TABLE sensor_data (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id INTEGER NOT NULL,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    data_type TEXT NOT NULL,    -- 'temperature', 'humidity', etc.
    value REAL NOT NULL,
    unit TEXT,
    synced INTEGER DEFAULT 0,   -- đã sync lên cloud chưa
    FOREIGN KEY (device_id) REFERENCES devices(id)
);

-- alarms table
CREATE TABLE alarms (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id INTEGER NOT NULL,
    alarm_type TEXT NOT NULL,
    severity TEXT NOT NULL,     -- 'critical', 'warning', 'info'
    message TEXT,
    acknowledged INTEGER DEFAULT 0,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (device_id) REFERENCES devices(id)
);

-- config table
CREATE TABLE config (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

**File tạo**: `src/database/schema.sql`

---

### 3. Tạo project structure & CMake config (8h)

**Mục tiêu**: Tổ chức code theo cấu trúc rõ ràng, dễ maintain

**Cấu trúc thư mục**:
```
gateway-system/
├── CMakeLists.txt           # Root CMake
├── src/
│   ├── main.cpp
│   ├── core/
│   │   ├── EventBus.h/cpp
│   │   ├── Logger.h/cpp
│   │   └── Config.h/cpp
│   ├── database/
│   │   ├── Database.h/cpp
│   │   └── schema.sql
│   ├── protocols/
│   │   ├── ModbusHandler.h/cpp
│   │   ├── RS485Handler.h/cpp
│   │   └── IProtocolHandler.h
│   ├── devices/
│   │   ├── DeviceManager.h/cpp
│   │   └── Device.h/cpp
│   ├── cloud/
│   │   └── CloudSyncService.h/cpp
│   └── gui/
│       ├── MainWindow.h/cpp
│       └── widgets/
├── include/            # Public headers
├── external/           # Third-party libraries
├── tests/             # Unit tests
└── docs/              # Documentation
```

**CMakeLists.txt cơ bản**:
```cmake
cmake_minimum_required(VERSION 3.16)
project(GatewaySystem VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt
find_package(Qt5 COMPONENTS Core Widgets Charts Network REQUIRED)

# Find SQLite
find_package(SQLite3 REQUIRED)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/external/spdlog/include
)

# Add subdirectories
add_subdirectory(src/core)
add_subdirectory(src/database)
add_subdirectory(src/protocols)
add_subdirectory(src/devices)
add_subdirectory(src/cloud)
add_subdirectory(src/gui)

# Main executable
add_executable(gateway_system src/main.cpp)
target_link_libraries(gateway_system
    Qt5::Core
    Qt5::Widgets
    Qt5::Charts
    Qt5::Network
    SQLite::SQLite3
    core
    database
    protocols
    devices
    cloud
    gui
)
```

**Tạo các CMakeLists.txt con** cho từng module (ví dụ `src/core/CMakeLists.txt`):
```cmake
add_library(core STATIC
    EventBus.cpp
    Logger.cpp
    Config.cpp
)
target_link_libraries(core Qt5::Core)
```

---

### 4. Implement Event Bus cơ bản (12h)

**Mục tiêu**: Tạo hệ thống giao tiếp giữa các module theo pattern pub-sub

**EventBus.h**:
```cpp
#ifndef EVENTBUS_H
#define EVENTBUS_H

#include <QObject>
#include <QMap>
#include <QList>
#include <functional>
#include <memory>

enum class EventType {
    DeviceConnected,
    DeviceDisconnected,
    DataReceived,
    AlarmTriggered,
    ConfigChanged,
    CloudSyncSuccess,
    CloudSyncFailed
};

struct Event {
    EventType type;
    QVariantMap data;
    qint64 timestamp;
    
    Event(EventType t, const QVariantMap& d = {})
        : type(t), data(d), timestamp(QDateTime::currentMSecsSinceEpoch()) {}
};

class EventBus : public QObject {
    Q_OBJECT
    
public:
    static EventBus& instance();
    
    // Subscribe to events
    void subscribe(EventType type, std::function<void(const Event&)> callback);
    
    // Publish events
    void publish(const Event& event);
    void publish(EventType type, const QVariantMap& data = {});
    
signals:
    void eventPublished(const Event& event);
    
private:
    EventBus();
    QMap<EventType, QList<std::function<void(const Event&)>>> subscribers_;
};

#endif // EVENTBUS_H
```

**EventBus.cpp**:
```cpp
#include "EventBus.h"
#include <QDebug>

EventBus& EventBus::instance() {
    static EventBus instance;
    return instance;
}

EventBus::EventBus() {
    // Connect signal to process events
    connect(this, &EventBus::eventPublished, this, [this](const Event& event) {
        if (subscribers_.contains(event.type)) {
            for (auto& callback : subscribers_[event.type]) {
                callback(event);
            }
        }
    }, Qt::QueuedConnection);
}

void EventBus::subscribe(EventType type, std::function<void(const Event&)> callback) {
    subscribers_[type].append(callback);
}

void EventBus::publish(const Event& event) {
    emit eventPublished(event);
}

void EventBus::publish(EventType type, const QVariantMap& data) {
    publish(Event(type, data));
}
```

**Sử dụng**:
```cpp
// Subscribe
EventBus::instance().subscribe(EventType::DataReceived, [](const Event& e) {
    qDebug() << "Data received:" << e.data;
});

// Publish
QVariantMap data;
data["device_id"] = 1;
data["value"] = 25.5;
EventBus::instance().publish(EventType::DataReceived, data);
```

---

### 5. Setup logging system (6h)

**Mục tiêu**: Tích hợp spdlog để ghi log hiệu quả

**Logger.h**:
```cpp
#ifndef LOGGER_H
#define LOGGER_H

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <memory>

class Logger {
public:
    static void init();
    static std::shared_ptr<spdlog::logger> get();
    
private:
    static std::shared_ptr<spdlog::logger> logger_;
};

// Macros for convenient logging
#define LOG_TRACE(...) Logger::get()->trace(__VA_ARGS__)
#define LOG_DEBUG(...) Logger::get()->debug(__VA_ARGS__)
#define LOG_INFO(...)  Logger::get()->info(__VA_ARGS__)
#define LOG_WARN(...)  Logger::get()->warn(__VA_ARGS__)
#define LOG_ERROR(...) Logger::get()->error(__VA_ARGS__)
#define LOG_CRITICAL(...) Logger::get()->critical(__VA_ARGS__)

#endif // LOGGER_H
```

**Logger.cpp**:
```cpp
#include "Logger.h"

std::shared_ptr<spdlog::logger> Logger::logger_;

void Logger::init() {
    try {
        // Console sink
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::debug);
        
        // File sink (rotating, 10MB per file, max 3 files)
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            "logs/gateway.log", 1024 * 1024 * 10, 3);
        file_sink->set_level(spdlog::level::trace);
        
        // Combine sinks
        std::vector<spdlog::sink_ptr> sinks {console_sink, file_sink};
        logger_ = std::make_shared<spdlog::logger>("gateway", sinks.begin(), sinks.end());
        
        // Set pattern
        logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");
        logger_->set_level(spdlog::level::trace);
        
        spdlog::register_logger(logger_);
        
        LOG_INFO("Logger initialized successfully");
    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Log initialization failed: " << ex.what() << std::endl;
    }
}

std::shared_ptr<spdlog::logger> Logger::get() {
    return logger_;
}
```

---

### 6. Tạo Qt main window template (10h)

**Mục tiêu**: Tạo giao diện chính của ứng dụng với các panel cơ bản

**MainWindow.h**:
```cpp
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTabWidget>
#include <QStatusBar>

class DashboardWidget;
class DeviceListWidget;
class AlarmPanelWidget;
class SettingsWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    
private slots:
    void onDeviceStatusChanged();
    void onAlarmReceived();
    
private:
    void setupUI();
    void createMenuBar();
    void createStatusBar();
    
    QTabWidget* tabWidget_;
    DashboardWidget* dashboardWidget_;
    DeviceListWidget* deviceListWidget_;
    AlarmPanelWidget* alarmPanel_;
    SettingsWidget* settingsWidget_;
};

#endif // MAINWINDOW_H
```

**MainWindow.cpp (skeleton)**:
```cpp
#include "MainWindow.h"
#include <QMenuBar>
#include <QToolBar>
#include <QAction>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUI();
    createMenuBar();
    createStatusBar();
    
    resize(1200, 800);
    setWindowTitle("Gateway Control System");
}

void MainWindow::setupUI() {
    // Central tab widget
    tabWidget_ = new QTabWidget(this);
    setCentralWidget(tabWidget_);
    
    // Create placeholder widgets (implement later in Phase 4)
    dashboardWidget_ = new DashboardWidget(this);
    deviceListWidget_ = new DeviceListWidget(this);
    alarmPanel_ = new AlarmPanelWidget(this);
    settingsWidget_ = new SettingsWidget(this);
    
    tabWidget_->addTab(dashboardWidget_, "Dashboard");
    tabWidget_->addTab(deviceListWidget_, "Devices");
    tabWidget_->addTab(alarmPanel_, "Alarms");
    tabWidget_->addTab(settingsWidget_, "Settings");
}

void MainWindow::createMenuBar() {
    QMenu* fileMenu = menuBar()->addMenu("&File");
    
    QAction* exitAction = new QAction("E&xit", this);
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QMainWindow::close);
    fileMenu->addAction(exitAction);
    
    QMenu* deviceMenu = menuBar()->addMenu("&Devices");
    QAction* addDeviceAction = new QAction("Add Device", this);
    deviceMenu->addAction(addDeviceAction);
    
    QMenu* helpMenu = menuBar()->addMenu("&Help");
    QAction* aboutAction = new QAction("About", this);
    helpMenu->addAction(aboutAction);
}

void MainWindow::createStatusBar() {
    statusBar()->showMessage("Ready");
}
```

---

### 7. Unit test framework setup (6h)

**Mục tiêu**: Setup Google Test hoặc Qt Test framework

**Sử dụng Qt Test** (khuyến nghị vì đã có Qt):

**tests/CMakeLists.txt**:
```cmake
find_package(Qt5 COMPONENTS Test REQUIRED)

enable_testing()

# Test cho EventBus
add_executable(test_eventbus test_eventbus.cpp)
target_link_libraries(test_eventbus Qt5::Test core)
add_test(NAME EventBusTest COMMAND test_eventbus)

# Test cho Database
add_executable(test_database test_database.cpp)
target_link_libraries(test_database Qt5::Test database)
add_test(NAME DatabaseTest COMMAND test_database)
```

**tests/test_eventbus.cpp**:
```cpp
#include <QtTest/QtTest>
#include "../src/core/EventBus.h"

class TestEventBus : public QObject {
    Q_OBJECT
    
private slots:
    void testPublishSubscribe() {
        bool called = false;
        EventBus::instance().subscribe(EventType::DataReceived, 
            [&called](const Event& e) {
                called = true;
                QCOMPARE(e.data["value"].toInt(), 42);
            });
        
        QVariantMap data;
        data["value"] = 42;
        EventBus::instance().publish(EventType::DataReceived, data);
        
        QTest::qWait(100); // Wait for async event
        QVERIFY(called);
    }
};

QTEST_MAIN(TestEventBus)
#include "test_eventbus.moc"
```

**Chạy tests**:
```bash
mkdir build && cd build
cmake ..
make
ctest --verbose
```

---

### 8. Buffer: Debug & fixes (4h)

**Mục tiêu**: Sửa các lỗi phát sinh, optimize

**Checklist**:
- [ ] Build project thành công không có warning
- [ ] All tests pass
- [ ] Memory leak check với Valgrind: `valgrind --leak-check=full ./gateway_system`
- [ ] Code formatting consistent (sử dụng clang-format)
- [ ] Update documentation

---

## Phase 2: Protocol Handlers (3 tuần - 90 giờ)

### 1. Modbus RTU handler implementation (20h)

**Mục tiêu**: Implement giao tiếp Modbus RTU qua serial port

**Sử dụng thư viện libmodbus**:

**IProtocolHandler.h (interface)**:
```cpp
#ifndef IPROTOCOLHANDLER_H
#define IPROTOCOLHANDLER_H

#include <QObject>
#include <QVariant>
#include <memory>

enum class ProtocolType {
    ModbusRTU,
    ModbusTCP,
    RS485,
    CANBus
};

class IProtocolHandler : public QObject {
    Q_OBJECT
    
public:
    virtual ~IProtocolHandler() = default;
    
    virtual bool connect(const QString& address, const QVariantMap& config) = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;
    
    virtual QVariant readData(const QString& datapoint) = 0;
    virtual bool writeData(const QString& datapoint, const QVariant& value) = 0;
    
    virtual ProtocolType type() const = 0;
    
signals:
    void connected();
    void disconnected();
    void dataReceived(const QString& datapoint, const QVariant& value);
    void errorOccurred(const QString& error);
};

#endif
```

**ModbusHandler.h**:
```cpp
#ifndef MODBUSHANDLER_H
#define MODBUSHANDLER_H

#include "IProtocolHandler.h"
#include <modbus/modbus.h>
#include <QTimer>

class ModbusHandler : public IProtocolHandler {
    Q_OBJECT
    
public:
    explicit ModbusHandler(QObject* parent = nullptr);
    ~ModbusHandler() override;
    
    bool connect(const QString& device, const QVariantMap& config) override;
    void disconnect() override;
    bool isConnected() const override;
    
    QVariant readData(const QString& datapoint) override;
    bool writeData(const QString& datapoint, const QVariant& value) override;
    
    ProtocolType type() const override { return ProtocolType::ModbusRTU; }
    
    // Modbus specific
    bool readHoldingRegisters(int addr, int nb, uint16_t* dest);
    bool readInputRegisters(int addr, int nb, uint16_t* dest);
    bool writeRegister(int addr, int value);
    
private:
    modbus_t* ctx_;
    bool connected_;
    int slaveId_;
    
    void setupModbusContext(const QVariantMap& config);
};

#endif
```

**ModbusHandler.cpp**:
```cpp
#include "ModbusHandler.h"
#include "../core/Logger.h"

ModbusHandler::ModbusHandler(QObject* parent)
    : IProtocolHandler(parent), ctx_(nullptr), connected_(false), slaveId_(1)
{
}

ModbusHandler::~ModbusHandler() {
    disconnect();
}

bool ModbusHandler::connect(const QString& device, const QVariantMap& config) {
    // Create Modbus context
    // Example: /dev/ttyUSB0, 9600, 'N', 8, 1
    int baudRate = config.value("baudRate", 9600).toInt();
    char parity = config.value("parity", "N").toString().at(0).toLatin1();
    int dataBits = config.value("dataBits", 8).toInt();
    int stopBits = config.value("stopBits", 1).toInt();
    slaveId_ = config.value("slaveId", 1).toInt();
    
    ctx_ = modbus_new_rtu(device.toUtf8().constData(), 
                          baudRate, parity, dataBits, stopBits);
    
    if (!ctx_) {
        LOG_ERROR("Failed to create Modbus context");
        emit errorOccurred("Failed to create Modbus context");
        return false;
    }
    
    // Set slave ID
    modbus_set_slave(ctx_, slaveId_);
    
    // Set timeout
    modbus_set_response_timeout(ctx_, 1, 0); // 1 second
    
    // Connect
    if (modbus_connect(ctx_) == -1) {
        LOG_ERROR("Modbus connection failed: {}", modbus_strerror(errno));
        modbus_free(ctx_);
        ctx_ = nullptr;
        emit errorOccurred("Connection failed");
        return false;
    }
    
    connected_ = true;
    LOG_INFO("Modbus connected to {} at {} baud", device.toStdString(), baudRate);
    emit connected();
    return true;
}

void ModbusHandler::disconnect() {
    if (ctx_) {
        modbus_close(ctx_);
        modbus_free(ctx_);
        ctx_ = nullptr;
    }
    connected_ = false;
    emit disconnected();
}

bool ModbusHandler::isConnected() const {
    return connected_;
}

bool ModbusHandler::readHoldingRegisters(int addr, int nb, uint16_t* dest) {
    if (!connected_ || !ctx_) return false;
    
    int rc = modbus_read_registers(ctx_, addr, nb, dest);
    if (rc == -1) {
        LOG_ERROR("Read holding registers failed: {}", modbus_strerror(errno));
        return false;
    }
    return true;
}

QVariant ModbusHandler::readData(const QString& datapoint) {
    // Parse datapoint: "holding:100" or "input:200"
    QStringList parts = datapoint.split(':');
    if (parts.size() != 2) return QVariant();
    
    QString type = parts[0];
    int addr = parts[1].toInt();
    
    uint16_t value;
    bool success = false;
    
    if (type == "holding") {
        success = readHoldingRegisters(addr, 1, &value);
    } else if (type == "input") {
        success = (modbus_read_input_registers(ctx_, addr, 1, &value) != -1);
    }
    
    if (success) {
        emit dataReceived(datapoint, value);
        return value;
    }
    
    return QVariant();
}

bool ModbusHandler::writeData(const QString& datapoint, const QVariant& value) {
    QStringList parts = datapoint.split(':');
    if (parts.size() != 2) return false;
    
    int addr = parts[1].toInt();
    int val = value.toInt();
    
    return writeRegister(addr, val);
}

bool ModbusHandler::writeRegister(int addr, int value) {
    if (!connected_ || !ctx_) return false;
    
    int rc = modbus_write_register(ctx_, addr, value);
    if (rc == -1) {
        LOG_ERROR("Write register failed: {}", modbus_strerror(errno));
        return false;
    }
    return true;
}
```

---

### 2. RS485 communication layer (16h)

**Mục tiêu**: Implement RS485 với custom protocol hoặc sử dụng Modbus over RS485

**RS485Handler.h**:
```cpp
#ifndef RS485HANDLER_H
#define RS485HANDLER_H

#include "IProtocolHandler.h"
#include <QSerialPort>
#include <QTimer>

class RS485Handler : public IProtocolHandler {
    Q_OBJECT
    
public:
    explicit RS485Handler(QObject* parent = nullptr);
    ~RS485Handler() override;
    
    bool connect(const QString& portName, const QVariantMap& config) override;
    void disconnect() override;
    bool isConnected() const override;
    
    QVariant readData(const QString& datapoint) override;
    bool writeData(const QString& datapoint, const QVariant& value) override;
    
    ProtocolType type() const override { return ProtocolType::RS485; }
    
private slots:
    void onReadyRead();
    void onErrorOccurred(QSerialPort::SerialPortError error);
    
private:
    bool sendCommand(const QByteArray& command);
    QByteArray receiveResponse(int timeoutMs = 1000);
    
    QSerialPort* serialPort_;
    QByteArray buffer_;
};

#endif
```

**RS485Handler.cpp (outline)**:
```cpp
#include "RS485Handler.h"
#include "../core/Logger.h"

RS485Handler::RS485Handler(QObject* parent)
    : IProtocolHandler(parent)
{
    serialPort_ = new QSerialPort(this);
    connect(serialPort_, &QSerialPort::readyRead, this, &RS485Handler::onReadyRead);
    connect(serialPort_, &QSerialPort::errorOccurred, this, &RS485Handler::onErrorOccurred);
}

bool RS485Handler::connect(const QString& portName, const QVariantMap& config) {
    serialPort_->setPortName(portName);
    serialPort_->setBaudRate(config.value("baudRate", 9600).toInt());
    serialPort_->setDataBits(QSerialPort::Data8);
    serialPort_->setParity(QSerialPort::NoParity);
    serialPort_->setStopBits(QSerialPort::OneStop);
    serialPort_->setFlowControl(QSerialPort::NoFlowControl);
    
    if (!serialPort_->open(QIODevice::ReadWrite)) {
        LOG_ERROR("Failed to open RS485 port: {}", 
                  serialPort_->errorString().toStdString());
        return false;
    }
    
    LOG_INFO("RS485 connected to {}", portName.toStdString());
    emit connected();
    return true;
}

void RS485Handler::disconnect() {
    if (serialPort_->isOpen()) {
        serialPort_->close();
    }
    emit disconnected();
}

bool RS485Handler::isConnected() const {
    return serialPort_->isOpen();
}

bool RS485Handler::sendCommand(const QByteArray& command) {
    if (!serialPort_->isOpen()) return false;
    
    qint64 written = serialPort_->write(command);
    return (written == command.size() && serialPort_->flush());
}

void RS485Handler::onReadyRead() {
    buffer_.append(serialPort_->readAll());
    
    // Parse complete frames from buffer
    // This depends on your protocol
    // Example: if frame ends with \r\n
    while (buffer_.contains("\r\n")) {
        int idx = buffer_.indexOf("\r\n");
        QByteArray frame = buffer_.left(idx);
        buffer_.remove(0, idx + 2);
        
        // Process frame
        LOG_DEBUG("RS485 frame received: {}", frame.toHex().toStdString());
    }
}
```

---

### 3. Device abstraction interfaces (12h)

**Device.h**:
```cpp
#ifndef DEVICE_H
#define DEVICE_H

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <memory>
#include "../protocols/IProtocolHandler.h"

enum class DeviceStatus {
    Disconnected,
    Connected,
    Error,
    Polling
};

struct DeviceInfo {
    int id;
    QString name;
    QString type;
    QString address;
    ProtocolType protocol;
    QVariantMap config;
    bool enabled;
};

class Device : public QObject {
    Q_OBJECT
    
public:
    explicit Device(const DeviceInfo& info, QObject* parent = nullptr);
    
    int id() const { return info_.id; }
    QString name() const { return info_.name; }
    DeviceStatus status() const { return status_; }
    
    bool connect();
    void disconnect();
    void startPolling(int intervalMs = 1000);
    void stopPolling();
    
    QVariant readDatapoint(const QString& datapoint);
    bool writeDatapoint(const QString& datapoint, const QVariant& value);
    
signals:
    void statusChanged(DeviceStatus status);
    void dataUpdated(const QString& datapoint, const QVariant& value);
    void errorOccurred(const QString& error);
    
private slots:
    void onPollTimeout();
    void onProtocolDataReceived(const QString& datapoint, const QVariant& value);
    
private:
    DeviceInfo info_;
    DeviceStatus status_;
    std::unique_ptr<IProtocolHandler> protocolHandler_;
    QTimer* pollTimer_;
    QStringList datapointsToRead_; // Configured datapoints
};

#endif
```

---

### 4. Protocol handler factory pattern (10h)

**ProtocolHandlerFactory.h**:
```cpp
#ifndef PROTOCOLHANDLERFACTORY_H
#define PROTOCOLHANDLERFACTORY_H

#include "IProtocolHandler.h"
#include <memory>
#include <QVariantMap>

class ProtocolHandlerFactory {
public:
    static std::unique_ptr<IProtocolHandler> create(
        ProtocolType type, 
        const QVariantMap& config = QVariantMap()
    );
    
private:
    ProtocolHandlerFactory() = default;
};

#endif
```

**ProtocolHandlerFactory.cpp**:
```cpp
#include "ProtocolHandlerFactory.h"
#include "ModbusHandler.h"
#include "RS485Handler.h"
#include "../core/Logger.h"

std::unique_ptr<IProtocolHandler> ProtocolHandlerFactory::create(
    ProtocolType type, 
    const QVariantMap& config)
{
    switch (type) {
        case ProtocolType::ModbusRTU:
            return std::make_unique<ModbusHandler>();
            
        case ProtocolType::RS485:
            return std::make_unique<RS485Handler>();
            
        // Add more protocols
        
        default:
            LOG_ERROR("Unknown protocol type");
            return nullptr;
    }
}
```

---

### 5. Data parser & validator (12h)

**DataParser.h**:
```cpp
#ifndef DATAPARSER_H
#define DATAPARSER_H

#include <QVariant>
#include <QString>
#include <functional>

enum class DataType {
    Integer,
    Float,
    Boolean,
    String,
    ByteArray
};

struct DataPointConfig {
    QString name;
    DataType type;
    double minValue;
    double maxValue;
    QString unit;
    std::function<double(double)> transformation; // e.g., raw to engineering units
};

class DataParser {
public:
    static QVariant parse(const QByteArray& rawData, const DataPointConfig& config);
    static bool validate(const QVariant& value, const DataPointConfig& config);
    
    // Convert raw register values to engineering units
    static double applyScaling(double rawValue, double scale, double offset);
    
    // Parse multi-register values (e.g., float from 2 registers)
    static float parseFloat32(uint16_t reg1, uint16_t reg2);
    static double parseFloat64(uint16_t* regs);
};

#endif
```

**DataParser.cpp**:
```cpp
#include "DataParser.h"
#include <cstring>

QVariant DataParser::parse(const QByteArray& rawData, const DataPointConfig& config) {
    switch (config.type) {
        case DataType::Integer: {
            int value = rawData.toInt();
            if (config.transformation) {
                value = config.transformation(value);
            }
            return value;
        }
        case DataType::Float: {
            float value = rawData.toFloat();
            if (config.transformation) {
                value = config.transformation(value);
            }
            return value;
        }
        case DataType::Boolean:
            return rawData.toInt() != 0;
            
        default:
            return rawData;
    }
}

bool DataParser::validate(const QVariant& value, const DataPointConfig& config) {
    if (!value.isValid()) return false;
    
    if (config.type == DataType::Float || config.type == DataType::Integer) {
        double val = value.toDouble();
        return (val >= config.minValue && val <= config.maxValue);
    }
    
    return true;
}

double DataParser::applyScaling(double rawValue, double scale, double offset) {
    return rawValue * scale + offset;
}

float DataParser::parseFloat32(uint16_t reg1, uint16_t reg2) {
    uint32_t combined = (static_cast<uint32_t>(reg1) << 16) | reg2;
    float result;
    std::memcpy(&result, &combined, sizeof(float));
    return result;
}
```

---

### 6. Integration tests với simulated devices (14h)

**test_protocol_integration.cpp**:
```cpp
#include <QtTest/QtTest>
#include "../src/protocols/ModbusHandler.h"
#include "../src/devices/Device.h"

// Modbus simulator class
class ModbusSimulator {
public:
    void start(int port = 5020);
    void setRegisterValue(int addr, uint16_t value);
    uint16_t getRegisterValue(int addr);
    
private:
    std::map<int, uint16_t> registers_;
};

class TestProtocolIntegration : public QObject {
    Q_OBJECT
    
private slots:
    void initTestCase() {
        // Start simulator
        simulator_.start();
    }
    
    void testModbusRead() {
        ModbusHandler handler;
        
        // Setup
        QVariantMap config;
        config["baudRate"] = 9600;
        config["slaveId"] = 1;
        
        // Connect
        QVERIFY(handler.connect("/dev/ttyUSB0", config));
        
        // Set expected value in simulator
        simulator_.setRegisterValue(100, 42);
        
        // Read
        QVariant value = handler.readData("holding:100");
        QCOMPARE(value.toInt(), 42);
        
        handler.disconnect();
    }
    
    void testDevicePolling() {
        // Test full device polling cycle
        DeviceInfo info;
        info.id = 1;
        info.name = "Test Device";
        info.protocol = ProtocolType::ModbusRTU;
        
        Device device(info);
        QVERIFY(device.connect());
        
        bool dataReceived = false;
        connect(&device, &Device::dataUpdated, 
                [&dataReceived](const QString& dp, const QVariant& val) {
            dataReceived = true;
        });
        
        device.startPolling(100); // Poll every 100ms
        QTest::qWait(500);
        
        QVERIFY(dataReceived);
        device.stopPolling();
    }
    
private:
    ModbusSimulator simulator_;
};

QTEST_MAIN(TestProtocolIntegration)
#include "test_protocol_integration.moc"
```

---

*[Tiếp tục với Phase 3-8...]*

## Phase 3: Core Business Logic (2.5 tuần - 75 giờ)

### 1. DeviceManager implementation (16h)

**Mục tiêu**: Quản lý tất cả devices, connection pool, lifecycle

**DeviceManager.h**:
```cpp
#ifndef DEVICEMANAGER_H
#define DEVICEMANAGER_H

#include <QObject>
#include <QMap>
#include <memory>
#include "Device.h"
#include "../database/Database.h"

class DeviceManager : public QObject {
    Q_OBJECT
    
public:
    static DeviceManager& instance();
    
    void initialize(Database* db);
    
    // Device operations
    int addDevice(const DeviceInfo& info);
    bool removeDevice(int deviceId);
    Device* getDevice(int deviceId);
    QList<Device*> getAllDevices();
    
    bool connectDevice(int deviceId);
    void disconnectDevice(int deviceId);
    void connectAll();
    void disconnectAll();
    
signals:
    void deviceAdded(int deviceId);
    void deviceRemoved(int deviceId);
    void deviceStatusChanged(int deviceId, DeviceStatus status);
    
private:
    DeviceManager();
    void loadDevicesFromDatabase();
    
    QMap<int, std::unique_ptr<Device>> devices_;
    Database* database_;
};

#endif
```

**Hướng dẫn**:
- Load tất cả devices từ DB khi initialize
- Maintain connection pool
- Auto-reconnect khi mất kết nối
- Thread-safe operations

---

### 2. Rule Engine cơ bản (14h)

**RuleEngine.h**:
```cpp
#ifndef RULEENGINE_H
#define RULEENGINE_H

#include <QObject>
#include <functional>
#include <QVariantMap>

enum class RuleCondition {
    GreaterThan,
    LessThan,
    Equals,
    NotEquals,
    Between,
    OutOfRange
};

enum class RuleAction {
    TriggerAlarm,
    SendNotification,
    ControlDevice,
    LogEvent
};

struct Rule {
    int id;
    QString name;
    bool enabled;
    
    // Condition
    int deviceId;
    QString datapoint;
    RuleCondition condition;
    QVariant threshold;
    QVariant threshold2; // For Between/OutOfRange
    
    // Action
    RuleAction action;
    QVariantMap actionParams;
};

class RuleEngine : public QObject {
    Q_OBJECT
    
public:
    static RuleEngine& instance();
    
    void addRule(const Rule& rule);
    void removeRule(int ruleId);
    void evaluateRules();
    
    // Called when new data arrives
    void onDataReceived(int deviceId, const QString& datapoint, const QVariant& value);
    
signals:
    void ruleTriggered(int ruleId, const QString& message);
    
private:
    RuleEngine();
    bool evaluateCondition(const Rule& rule, const QVariant& value);
    void executeAction(const Rule& rule, const QVariant& value);
    
    QMap<int, Rule> rules_;
};

#endif
```

**Cách dùng**:
```cpp
// Add rule: If temperature > 30, trigger alarm
Rule rule;
rule.name = "High Temperature";
rule.deviceId = 1;
rule.datapoint = "temperature";
rule.condition = RuleCondition::GreaterThan;
rule.threshold = 30.0;
rule.action = RuleAction::TriggerAlarm;
rule.actionParams["severity"] = "critical";
rule.actionParams["message"] = "Temperature exceeded threshold!";

RuleEngine::instance().addRule(rule);
```

---

### 3. Alarm & notification system (12h)

**AlarmManager.h**:
```cpp
#ifndef ALARMMANAGER_H
#define ALARMMANAGER_H

#include <QObject>
#include <QQueue>

enum class AlarmSeverity {
    Info,
    Warning,
    Critical
};

struct Alarm {
    int id;
    int deviceId;
    QString type;
    AlarmSeverity severity;
    QString message;
    QDateTime timestamp;
    bool acknowledged;
};

class AlarmManager : public QObject {
    Q_OBJECT
    
public:
    static AlarmManager& instance();
    
    int raiseAlarm(int deviceId, const QString& type, 
                   AlarmSeverity severity, const QString& message);
    void acknowledgeAlarm(int alarmId);
    void clearAlarm(int alarmId);
    
    QList<Alarm> getActiveAlarms();
    QList<Alarm> getAlarmHistory(const QDateTime& from, const QDateTime& to);
    
signals:
    void alarmRaised(const Alarm& alarm);
    void alarmAcknowledged(int alarmId);
    void alarmCleared(int alarmId);
    
private:
    AlarmManager();
    void saveToDatabase(const Alarm& alarm);
    
    QMap<int, Alarm> activeAlarms_;
};

#endif
```

---

### 4. Local database operations (10h)

**Database.h**:
```cpp
#ifndef DATABASE_H
#define DATABASE_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include "../devices/Device.h"

class Database : public QObject {
    Q_OBJECT
    
public:
    explicit Database(const QString& dbPath, QObject* parent = nullptr);
    ~Database();
    
    bool open();
    void close();
    
    // Device operations
    QList<DeviceInfo> loadDevices();
    bool saveDevice(const DeviceInfo& info);
    bool deleteDevice(int deviceId);
    
    // Sensor data
    bool insertSensorData(int deviceId, const QString& dataType, 
                         double value, const QString& unit);
    QList<QVariantMap> querySensorData(int deviceId, 
                                       const QDateTime& from, 
                                       const QDateTime& to);
    
    // Alarms
    bool insertAlarm(const Alarm& alarm);
    QList<Alarm> queryAlarms(bool onlyActive = false);
    
    // Config
    bool setConfig(const QString& key, const QString& value);
    QString getConfig(const QString& key, const QString& defaultValue = "");
    
private:
    bool executeSchema();
    QSqlDatabase db_;
    QString dbPath_;
};

#endif
```

**Hướng dẫn**:
- Sử dụng prepared statements để tránh SQL injection
- Implement transaction cho batch operations
- Regular cleanup old data (older than X days)

---

### 5. Configuration management (8h)

**ConfigManager.h**:
```cpp
#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QSettings>
#include <QVariantMap>

class ConfigManager : public QObject {
    Q_OBJECT
    
public:
    static ConfigManager& instance();
    
    // Application settings
    QVariant get(const QString& key, const QVariant& defaultValue = QVariant());
    void set(const QString& key, const QVariant& value);
    
    // Device configurations
    QVariantMap getDeviceConfig(int deviceId);
    void setDeviceConfig(int deviceId, const QVariantMap& config);
    
    // Network settings
    QString getMqttBroker();
    int getMqttPort();
    QString getApiEndpoint();
    
signals:
    void configChanged(const QString& key, const QVariant& value);
    
private:
    ConfigManager();
    QSettings* settings_;
};

#endif
```

---

### 6. Data aggregation & processing (10h)

**DataAggregator.h**:
```cpp
#ifndef DATAAGGREGATOR_H
#define DATAAGGREGATOR_H

#include <QObject>
#include <QMap>
#include <QQueue>

struct AggregatedData {
    double min;
    double max;
    double avg;
    double sum;
    int count;
    QDateTime periodStart;
    QDateTime periodEnd;
};

class DataAggregator : public QObject {
    Q_OBJECT
    
public:
    static DataAggregator& instance();
    
    void addDataPoint(int deviceId, const QString& dataType, 
                     double value, const QDateTime& timestamp);
    
    AggregatedData getAggregated(int deviceId, const QString& dataType,
                                const QDateTime& from, const QDateTime& to);
    
    // Real-time aggregation (e.g., 1-minute averages)
    void startRealtimeAggregation(int intervalSeconds = 60);
    
signals:
    void aggregatedDataReady(int deviceId, const QString& dataType, 
                            const AggregatedData& data);
    
private:
    DataAggregator();
    void performAggregation();
    
    struct DataBuffer {
        QQueue<QPair<QDateTime, double>> values;
        int maxSize = 1000;
    };
    
    QMap<QString, DataBuffer> buffers_; // key: "deviceId:dataType"
};

#endif
```

---

## Phase 4: Qt GUI Development (3 tuần - 90 giờ)

### 1. Dashboard design & layout (12h)

**DashboardWidget.h**:
```cpp
#ifndef DASHBOARDWIDGET_H
#define DASHBOARDWIDGET_H

#include <QWidget>
#include <QGridLayout>
#include <QLabel>

class DeviceStatusCard;
class QuickStatsWidget;
class RecentAlarmsWidget;

class DashboardWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit DashboardWidget(QWidget* parent = nullptr);
    
private slots:
    void updateStatistics();
    void onDeviceStatusChanged(int deviceId, DeviceStatus status);
    
private:
    void setupUI();
    
    QGridLayout* mainLayout_;
    QuickStatsWidget* statsWidget_;
    QList<DeviceStatusCard*> deviceCards_;
    RecentAlarmsWidget* alarmsWidget_;
};

#endif
```

**UI Layout**:
```
┌─────────────────────────────────────────────┐
│ Quick Stats                                 │
│ [Total: 10] [Online: 8] [Alarms: 2]       │
├──────────────┬──────────────┬──────────────┤
│ Device Card  │ Device Card  │ Device Card  │
│ ┌──────────┐│ ┌──────────┐ │ ┌──────────┐ │
│ │  Status  ││ │  Status  │ │ │  Status  │ │
│ │  Temp: 25││ │  Temp: 30│ │ │  Offline │ │
│ └──────────┘│ └──────────┘ │ └──────────┘ │
├──────────────┴──────────────┴──────────────┤
│ Recent Alarms                               │
│ [!] Device 3: High temperature (10:23 AM)  │
│ [!] Device 5: Connection lost (10:15 AM)   │
└─────────────────────────────────────────────┘
```

---

### 2. Device list widget với real-time updates (14h)

**DeviceListWidget.h**:
```cpp
#ifndef DEVICELISTWIDGET_H
#define DEVICELISTWIDGET_H

#include <QWidget>
#include <QTableWidget>
#include <QPushButton>

class DeviceListWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit DeviceListWidget(QWidget* parent = nullptr);
    
private slots:
    void onAddDeviceClicked();
    void onRemoveDeviceClicked();
    void onDeviceDoubleClicked(int row, int column);
    void refreshDeviceList();
    void onDeviceDataUpdated(int deviceId, const QString& datapoint, const QVariant& value);
    
private:
    void setupUI();
    void populateTable();
    
    QTableWidget* tableWidget_;
    QPushButton* addButton_;
    QPushButton* removeButton_;
    QPushButton* refreshButton_;
};

#endif
```

**Columns**: ID | Name | Type | Status | Last Value | Last Update | Actions

**Real-time update**:
```cpp
void DeviceListWidget::setupUI() {
    // ...
    
    // Connect to DeviceManager signals for real-time updates
    connect(&DeviceManager::instance(), &DeviceManager::deviceStatusChanged,
            this, [this](int deviceId, DeviceStatus status) {
        // Update table row for this device
        updateDeviceRow(deviceId);
    });
    
    // Subscribe to data updates via EventBus
    EventBus::instance().subscribe(EventType::DataReceived, 
        [this](const Event& e) {
            int deviceId = e.data["device_id"].toInt();
            QString datapoint = e.data["datapoint"].toString();
            QVariant value = e.data["value"];
            
            onDeviceDataUpdated(deviceId, datapoint, value);
        });
}
```

---

### 3. Real-time charts (Qt Charts) (16h)

**ChartWidget.h**:
```cpp
#ifndef CHARTWIDGET_H
#define CHARTWIDGET_H

#include <QWidget>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QTimer>

QT_CHARTS_USE_NAMESPACE

class ChartWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit ChartWidget(QWidget* parent = nullptr);
    
    void setDevice(int deviceId, const QString& datapoint);
    void addDataPoint(double value);
    void setMaxDataPoints(int max) { maxDataPoints_ = max; }
    
private slots:
    void onDataReceived(int deviceId, const QString& dp, const QVariant& value);
    
private:
    void setupChart();
    
    QChartView* chartView_;
    QChart* chart_;
    QLineSeries* series_;
    QValueAxis* axisX_;
    QValueAxis* axisY_;
    
    int deviceId_;
    QString datapoint_;
    int maxDataPoints_ = 100;
    qint64 startTime_;
};

#endif
```

**ChartWidget.cpp**:
```cpp
#include "ChartWidget.h"
#include <QVBoxLayout>

ChartWidget::ChartWidget(QWidget* parent)
    : QWidget(parent), deviceId_(-1), startTime_(QDateTime::currentMSecsSinceEpoch())
{
    setupChart();
}

void ChartWidget::setupChart() {
    chart_ = new QChart();
    chart_->setTitle("Real-time Data");
    chart_->setAnimationOptions(QChart::NoAnimation); // For better performance
    
    series_ = new QLineSeries();
    chart_->addSeries(series_);
    
    // X axis (time in seconds)
    axisX_ = new QValueAxis();
    axisX_->setTitleText("Time (s)");
    axisX_->setRange(0, 60); // Show last 60 seconds
    chart_->addAxis(axisX_, Qt::AlignBottom);
    series_->attachAxis(axisX_);
    
    // Y axis (value)
    axisY_ = new QValueAxis();
    axisY_->setTitleText("Value");
    axisY_->setRange(0, 100); // Auto-adjust later
    chart_->addAxis(axisY_, Qt::AlignLeft);
    series_->attachAxis(axisY_);
    
    chartView_ = new QChartView(chart_);
    chartView_->setRenderHint(QPainter::Antialiasing);
    
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(chartView_);
}

void ChartWidget::addDataPoint(double value) {
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    double timeSeconds = (now - startTime_) / 1000.0;
    
    series_->append(timeSeconds, value);
    
    // Keep only last N points
    if (series_->count() > maxDataPoints_) {
        series_->remove(0);
    }
    
    // Auto-adjust X axis to show rolling window
    if (timeSeconds > 60) {
        axisX_->setRange(timeSeconds - 60, timeSeconds);
    }
    
    // Auto-adjust Y axis
    double minY = 1e9, maxY = -1e9;
    for (const QPointF& point : series_->points()) {
        minY = qMin(minY, point.y());
        maxY = qMax(maxY, point.y());
    }
    double margin = (maxY - minY) * 0.1;
    axisY_->setRange(minY - margin, maxY + margin);
}
```

**Multi-chart dashboard**:
```cpp
// Create grid of charts for different parameters
QGridLayout* chartGrid = new QGridLayout();
chartGrid->addWidget(new ChartWidget("Temperature", deviceId, "temp"), 0, 0);
chartGrid->addWidget(new ChartWidget("Humidity", deviceId, "humidity"), 0, 1);
chartGrid->addWidget(new ChartWidget("Pressure", deviceId, "pressure"), 1, 0);
```

---

### 4. Configuration dialogs (10h)

**AddDeviceDialog.h**:
```cpp
#ifndef ADDDEVICEDIALOG_H
#define ADDDEVICEDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>

class AddDeviceDialog : public QDialog {
    Q_OBJECT
    
public:
    explicit AddDeviceDialog(QWidget* parent = nullptr);
    
    DeviceInfo getDeviceInfo() const;
    
private slots:
    void onProtocolChanged(int index);
    void onTestConnection();
    
private:
    void setupUI();
    bool validateInput();
    
    QLineEdit* nameEdit_;
    QComboBox* protocolCombo_;
    QLineEdit* addressEdit_;
    
    // Protocol-specific widgets
    QSpinBox* baudRateSpinBox_;
    QComboBox* parityCombo_;
    QSpinBox* slaveIdSpinBox_;
    
    QPushButton* testButton_;
    QLabel* statusLabel_;
};

#endif
```

**Settings dialog**:
```cpp
// MQTT Settings
QGroupBox* mqttGroup = new QGroupBox("MQTT Settings");
QFormLayout* mqttLayout = new QFormLayout();
mqttLayout->addRow("Broker:", brokerEdit);
mqttLayout->addRow("Port:", portSpinBox);
mqttLayout->addRow("Username:", usernameEdit);
mqttLayout->addRow("Password:", passwordEdit);
mqttGroup->setLayout(mqttLayout);

// Database Settings
QGroupBox* dbGroup = new QGroupBox("Database Settings");
// ...

// Logging Settings
QGroupBox* logGroup = new QGroupBox("Logging");
// ...
```

---

### 5. Alarm panel & notifications UI (12h)

**AlarmPanelWidget.h**:
```cpp
#ifndef ALARMPANELWIDGET_H
#define ALARMPANELWIDGET_H

#include <QWidget>
#include <QListWidget>
#include <QSystemTrayIcon>

class AlarmPanelWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit AlarmPanelWidget(QWidget* parent = nullptr);
    
private slots:
    void onAlarmRaised(const Alarm& alarm);
    void onAcknowledgeClicked();
    void onClearClicked();
    void onFilterChanged(const QString& filter);
    
private:
    void setupUI();
    void showSystemNotification(const Alarm& alarm);
    void playAlarmSound(AlarmSeverity severity);
    
    QListWidget* alarmList_;
    QPushButton* acknowledgeButton_;
    QPushButton* clearButton_;
    QComboBox* filterCombo_;
    
    QSystemTrayIcon* trayIcon_;
};

#endif
```

**System notifications**:
```cpp
void AlarmPanelWidget::showSystemNotification(const Alarm& alarm) {
    if (!trayIcon_) {
        trayIcon_ = new QSystemTrayIcon(this);
        trayIcon_->setIcon(QIcon(":/icons/alarm.png"));
    }
    
    QString title = QString("Alarm: %1").arg(alarm.type);
    QString message = alarm.message;
    
    QSystemTrayIcon::MessageIcon icon;
    switch (alarm.severity) {
        case AlarmSeverity::Critical:
            icon = QSystemTrayIcon::Critical;
            break;
        case AlarmSeverity::Warning:
            icon = QSystemTrayIcon::Warning;
            break;
        default:
            icon = QSystemTrayIcon::Information;
    }
    
    trayIcon_->show();
    trayIcon_->showMessage(title, message, icon, 5000);
}
```

---

## Phase 5: Cloud Integration (2.5 tuần - 75 giờ)

### 1. MQTT client implementation (14h)

**MqttClient.h**:
```cpp
#ifndef MQTTCLIENT_H
#define MQTTCLIENT_H

#include <QObject>
#include <QMqttClient>
#include <QTimer>

class MqttClient : public QObject {
    Q_OBJECT
    
public:
    explicit MqttClient(QObject* parent = nullptr);
    ~MqttClient();
    
    bool connectToBroker(const QString& host, quint16 port,
                        const QString& clientId = QString());
    void disconnect();
    bool isConnected() const;
    
    bool publish(const QString& topic, const QByteArray& payload, 
                quint8 qos = 0, bool retain = false);
    bool subscribe(const QString& topic, quint8 qos = 0);
    void unsubscribe(const QString& topic);
    
signals:
    void connected();
    void disconnected();
    void messageReceived(const QString& topic, const QByteArray& payload);
    void errorOccurred(const QString& error);
    
private slots:
    void onConnected();
    void onDisconnected();
    void onMessageReceived(const QByteArray& message, const QMqttTopicName& topic);
    void onStateChanged(QMqttClient::ClientState state);
    
private:
    QMqttClient* client_;
    QString lastWillTopic_;
};

#endif
```

**MqttClient.cpp**:
```cpp
#include "MqttClient.h"
#include "../core/Logger.h"

MqttClient::MqttClient(QObject* parent)
    : QObject(parent)
{
    client_ = new QMqttClient(this);
    
    connect(client_, &QMqttClient::connected, this, &MqttClient::onConnected);
    connect(client_, &QMqttClient::disconnected, this, &MqttClient::onDisconnected);
    connect(client_, &QMqttClient::messageReceived, this, &MqttClient::onMessageReceived);
    connect(client_, &QMqttClient::stateChanged, this, &MqttClient::onStateChanged);
}

bool MqttClient::connectToBroker(const QString& host, quint16 port, const QString& clientId) {
    client_->setHostname(host);
    client_->setPort(port);
    
    if (!clientId.isEmpty()) {
        client_->setClientId(clientId);
    } else {
        client_->setClientId(QString("gateway_%1").arg(QDateTime::currentMSecsSinceEpoch()));
    }
    
    // Set Last Will
    lastWillTopic_ = QString("gateway/%1/status").arg(client_->clientId());
    client_->setWillTopic(lastWillTopic_);
    client_->setWillMessage("offline");
    client_->setWillQoS(1);
    client_->setWillRetain(true);
    
    client_->connectToHost();
    return true;
}

void MqttClient::onConnected() {
    LOG_INFO("MQTT connected to {}:{}", 
             client_->hostname().toStdString(), client_->port());
    
    // Publish online status
    publish(lastWillTopic_, "online", 1, true);
    
    emit connected();
}

bool MqttClient::publish(const QString& topic, const QByteArray& payload, 
                        quint8 qos, bool retain) {
    if (!isConnected()) {
        LOG_ERROR("Cannot publish: MQTT not connected");
        return false;
    }
    
    client_->publish(topic, payload, qos, retain);
    return true;
}

bool MqttClient::subscribe(const QString& topic, quint8 qos) {
    if (!isConnected()) return false;
    
    auto subscription = client_->subscribe(topic, qos);
    if (!subscription) {
        LOG_ERROR("Failed to subscribe to topic: {}", topic.toStdString());
        return false;
    }
    
    LOG_INFO("Subscribed to topic: {}", topic.toStdString());
    return true;
}
```

---

### 2. CloudSyncService với retry logic (16h)

**CloudSyncService.h**:
```cpp
#ifndef CLOUDSYNCSERVICE_H
#define CLOUDSYNCSERVICE_H

#include <QObject>
#include <QQueue>
#include <QTimer>
#include "MqttClient.h"
#include "../database/Database.h"

struct SyncJob {
    qint64 id;
    QString topic;
    QByteArray payload;
    int retryCount = 0;
    QDateTime timestamp;
};

class CloudSyncService : public QObject {
    Q_OBJECT
    
public:
    static CloudSyncService& instance();
    
    void initialize(MqttClient* mqtt, Database* db);
    void start();
    void stop();
    
    // Queue data for sync
    void queueSensorData(int deviceId, const QString& dataType, 
                        double value, const QDateTime& timestamp);
    void queueAlarm(const Alarm& alarm);
    void queueEvent(const QString& eventType, const QVariantMap& data);
    
signals:
    void syncSucceeded(qint64 jobId);
    void syncFailed(qint64 jobId, const QString& reason);
    
private slots:
    void onSyncTimerTimeout();
    void onMqttConnected();
    void onMqttDisconnected();
    void onCommandReceived(const QString& topic, const QByteArray& payload);
    
private:
    CloudSyncService();
    
    void processSyncQueue();
    void retryFailedJobs();
    void saveToOfflineBuffer(const SyncJob& job);
    void loadOfflineBuffer();
    
    MqttClient* mqtt_;
    Database* database_;
    QTimer* syncTimer_;
    
    QQueue<SyncJob> syncQueue_;
    QList<SyncJob> failedJobs_;
    bool isOnline_ = false;
    
    const int MAX_RETRY = 3;
    const int SYNC_INTERVAL_MS = 5000; // 5 seconds
};

#endif
```

**CloudSyncService.cpp (key methods)**:
```cpp
void CloudSyncService::queueSensorData(int deviceId, const QString& dataType,
                                      double value, const QDateTime& timestamp) {
    SyncJob job;
    job.id = QDateTime::currentMSecsSinceEpoch();
    job.topic = QString("gateway/data/%1/%2").arg(deviceId).arg(dataType);
    
    // Create JSON payload
    QJsonObject json;
    json["device_id"] = deviceId;
    json["data_type"] = dataType;
    json["value"] = value;
    json["timestamp"] = timestamp.toString(Qt::ISODate);
    
    job.payload = QJsonDocument(json).toJson(QJsonDocument::Compact);
    job.timestamp = QDateTime::currentDateTime();
    
    syncQueue_.enqueue(job);
}

void CloudSyncService::processSyncQueue() {
    if (!isOnline_ || syncQueue_.isEmpty()) return;
    
    while (!syncQueue_.isEmpty()) {
        SyncJob job = syncQueue_.dequeue();
        
        bool success = mqtt_->publish(job.topic, job.payload, 1, false);
        
        if (success) {
            LOG_DEBUG("Synced job {} to topic {}", job.id, job.topic.toStdString());
            emit syncSucceeded(job.id);
            
            // Mark as synced in database
            database_->executeSql("UPDATE sensor_data SET synced=1 WHERE id=?", {job.id});
        } else {
            job.retryCount++;
            
            if (job.retryCount < MAX_RETRY) {
                failedJobs_.append(job);
                LOG_WARN("Job {} failed, queued for retry ({}/{})", 
                        job.id, job.retryCount, MAX_RETRY);
            } else {
                LOG_ERROR("Job {} failed after {} retries, saving to offline buffer",
                         job.id, MAX_RETRY);
                saveToOfflineBuffer(job);
                emit syncFailed(job.id, "Max retries exceeded");
            }
        }
    }
}

void CloudSyncService::saveToOfflineBuffer(const SyncJob& job) {
    // Save to database for later sync
    QSqlQuery query(database_->db());
    query.prepare("INSERT INTO offline_buffer (job_id, topic, payload, timestamp) "
                 "VALUES (?, ?, ?, ?)");
    query.addBindValue(job.id);
    query.addBindValue(job.topic);
    query.addBindValue(job.payload);
    query.addBindValue(job.timestamp);
    query.exec();
}

void CloudSyncService::onMqttConnected() {
    isOnline_ = true;
    LOG_INFO("Cloud sync service online");
    
    // Subscribe to command topics
    mqtt_->subscribe("gateway/commands/#");
    
    // Load and sync offline buffer
    loadOfflineBuffer();
}
```

---

### 3. Offline mode & data buffering (14h)

**OfflineBuffer implementation**:

Add table to schema:
```sql
CREATE TABLE offline_buffer (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    job_id INTEGER NOT NULL,
    topic TEXT NOT NULL,
    payload BLOB NOT NULL,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    retry_count INTEGER DEFAULT 0
);
```

**Auto cleanup old offline data**:
```cpp
void CloudSyncService::cleanupOldOfflineData() {
    // Delete offline data older than 7 days
    QDateTime cutoff = QDateTime::currentDateTime().addDays(-7);
    
    QSqlQuery query(database_->db());
    query.prepare("DELETE FROM offline_buffer WHERE timestamp < ?");
    query.addBindValue(cutoff);
    query.exec();
    
    LOG_INFO("Cleaned up {} old offline buffer entries", query.numRowsAffected());
}
```

---

### 4. Command handling từ cloud (12h)

**Command types**:
- Control device (turn on/off, set value)
- Update configuration
- Request status/diagnostic
- Firmware update trigger

**CommandHandler.h**:
```cpp
#ifndef COMMANDHANDLER_H
#define COMMANDHANDLER_H

#include <QObject>
#include <QJsonObject>

enum class CommandType {
    ControlDevice,
    UpdateConfig,
    RequestStatus,
    FirmwareUpdate,
    Unknown
};

class CommandHandler : public QObject {
    Q_OBJECT
    
public:
    static CommandHandler& instance();
    
    void handleCommand(const QString& topic, const QByteArray& payload);
    
signals:
    void commandExecuted(const QString& commandId, bool success, const QString& result);
    
private:
    CommandHandler();
    
    CommandType parseCommandType(const QJsonObject& json);
    void executeControlDevice(const QJsonObject& json);
    void executeUpdateConfig(const QJsonObject& json);
    void executeRequestStatus(const QJsonObject& json);
    
    void sendResponse(const QString& commandId, bool success, 
                     const QString& result = QString());
};

#endif
```

**Command format (JSON)**:
```json
{
  "command_id": "cmd_12345",
  "type": "control_device",
  "device_id": 1,
  "action": "write",
  "datapoint": "output:0",
  "value": 1,
  "timestamp": "2024-01-15T10:30:00Z"
}
```

---

## Phase 6: Backend API (2 tuần - 60 giờ)

### 1. Node.js Express setup cơ bản (6h)

**Project structure**:
```
backend/
├── package.json
├── server.js
├── config/
│   └── database.js
├── routes/
│   ├── devices.js
│   ├── data.js
│   └── alarms.js
├── models/
│   ├── Device.js
│   ├── SensorData.js
│   └── Alarm.js
├── middleware/
│   └── auth.js
└── services/
    ├── mqttService.js
    └── dataService.js
```

**package.json**:
```json
{
  "name": "gateway-backend",
  "version": "1.0.0",
  "dependencies": {
    "express": "^4.18.0",
    "pg": "^8.11.0",
    "mqtt": "^4.3.7",
    "jsonwebtoken": "^9.0.0",
    "bcrypt": "^5.1.0",
    "cors": "^2.8.5",
    "dotenv": "^16.0.0",
    "ws": "^8.13.0"
  }
}
```

**server.js**:
```javascript
const express = require('express');
const cors = require('cors');
require('dotenv').config();

const app = express();

// Middleware
app.use(cors());
app.use(express.json());

// Routes
app.use('/api/devices', require('./routes/devices'));
app.use('/api/data', require('./routes/data'));
app.use('/api/alarms', require('./routes/alarms'));

// Health check
app.get('/health', (req, res) => {
  res.json({ status: 'ok', timestamp: new Date() });
});

const PORT = process.env.PORT || 3000;
app.listen(PORT, () => {
  console.log(`Server running on port ${PORT}`);
});
```

---

### 2. REST API endpoints chính (14h)

**routes/devices.js**:
```javascript
const express = require('express');
const router = express.Router();
const { pool } = require('../config/database');

// GET all devices
router.get('/', async (req, res) => {
  try {
    const result = await pool.query('SELECT * FROM devices WHERE enabled = true');
    res.json(result.rows);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

// GET device by ID
router.get('/:id', async (req, res) => {
  try {
    const { id } = req.params;
    const result = await pool.query('SELECT * FROM devices WHERE id = $1', [id]);
    
    if (result.rows.length === 0) {
      return res.status(404).json({ error: 'Device not found' });
    }
    
    res.json(result.rows[0]);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

// POST create device
router.post('/', async (req, res) => {
  try {
    const { name, device_type, address, protocol_config } = req.body;
    
    const result = await pool.query(
      'INSERT INTO devices (name, device_type, address, protocol_config) VALUES ($1, $2, $3, $4) RETURNING *',
      [name, device_type, address, JSON.stringify(protocol_config)]
    );
    
    res.status(201).json(result.rows[0]);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

// PUT update device
router.put('/:id', async (req, res) => {
  // Implementation
});

// DELETE device
router.delete('/:id', async (req, res) => {
  // Implementation
});

module.exports = router;
```

**routes/data.js**:
```javascript
const express = require('express');
const router = express.Router();
const { pool } = require('../config/database');

// GET sensor data with filters
router.get('/', async (req, res) => {
  try {
    const { device_id, data_type, from, to, limit = 1000 } = req.query;
    
    let query = 'SELECT * FROM sensor_data WHERE 1=1';
    const params = [];
    let paramIndex = 1;
    
    if (device_id) {
      query += ` AND device_id = ${paramIndex++}`;
      params.push(device_id);
    }
    
    if (data_type) {
      query += ` AND data_type = ${paramIndex++}`;
      params.push(data_type);
    }
    
    if (from) {
      query += ` AND timestamp >= ${paramIndex++}`;
      params.push(from);
    }
    
    if (to) {
      query += ` AND timestamp <= ${paramIndex++}`;
      params.push(to);
    }
    
    query += ` ORDER BY timestamp DESC LIMIT ${paramIndex}`;
    params.push(limit);
    
    const result = await pool.query(query, params);
    res.json(result.rows);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

// GET aggregated data
router.get('/aggregated', async (req, res) => {
  try {
    const { device_id, data_type, from, to, interval = '1 hour' } = req.query;
    
    const query = `
      SELECT 
        date_trunc($1, timestamp) as period,
        MIN(value) as min_value,
        MAX(value) as max_value,
        AVG(value) as avg_value,
        COUNT(*) as count
      FROM sensor_data
      WHERE device_id = $2 
        AND data_type = $3
        AND timestamp BETWEEN $4 AND $5
      GROUP BY period
      ORDER BY period
    `;
    
    const result = await pool.query(query, [interval, device_id, data_type, from, to]);
    res.json(result.rows);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

module.exports = router;
```

---

### 3. WebSocket cho real-time (10h)

**websocketServer.js**:
```javascript
const WebSocket = require('ws');
const mqtt = require('mqtt');

class WebSocketServer {
  constructor(server) {
    this.wss = new WebSocket.Server({ server });
    this.clients = new Map();
    
    this.setupWebSocket();
    this.setupMQTTBridge();
  }
  
  setupWebSocket() {
    this.wss.on('connection', (ws, req) => {
      const clientId = req.headers['sec-websocket-key'];
      console.log(`Client connected: ${clientId}`);
      
      this.clients.set(clientId, { ws, subscriptions: new Set() });
      
      ws.on('message', (message) => {
        try {
          const data = JSON.parse(message);
          this.handleClientMessage(clientId, data);
        } catch (err) {
          console.error('Invalid message:', err);
        }
      });
      
      ws.on('close', () => {
        console.log(`Client disconnected: ${clientId}`);
        this.clients.delete(clientId);
      });
      
      // Send initial connection acknowledgment
      ws.send(JSON.stringify({
        type: 'connected',
        clientId: clientId
      }));
    });
  }
  
  setupMQTTBridge() {
    this.mqttClient = mqtt.connect('mqtt://localhost:1883');
    
    this.mqttClient.on('connect', () => {
      console.log('WebSocket bridge connected to MQTT');
      this.mqttClient.subscribe('gateway/data/#');
      this.mqttClient.subscribe('gateway/alarms/#');
    });
    
    this.mqttClient.on('message', (topic, payload) => {
      // Broadcast to all connected WebSocket clients
      this.broadcast({
        type: 'mqtt_message',
        topic: topic,
        data: JSON.parse(payload.toString())
      });
    });
  }
  
  handleClientMessage(clientId, message) {
    switch (message.type) {
      case 'subscribe':
        this.handleSubscribe(clientId, message.topics);
        break;
      case 'unsubscribe':
        this.handleUnsubscribe(clientId, message.topics);
        break;
      case 'request_history':
        this.handleHistoryRequest(clientId, message);
        break;
    }
  }
  
  broadcast(message) {
    const payload = JSON.stringify(message);
    this.clients.forEach((client) => {
      if (client.ws.readyState === WebSocket.OPEN) {
        client.ws.send(payload);
      }
    });
  }
  
  sendToClient(clientId, message) {
    const client = this.clients.get(clientId);
    if (client && client.ws.readyState === WebSocket.OPEN) {
      client.ws.send(JSON.stringify(message));
    }
  }
}

module.exports = WebSocketServer;
```

**Usage in server.js**:
```javascript
const http = require('http');
const server = http.createServer(app);
const WebSocketServer = require('./websocketServer');

const wsServer = new WebSocketServer(server);

server.listen(PORT, () => {
  console.log(`Server with WebSocket running on port ${PORT}`);
});
```

---

## Phase 7: Web Frontend (2 tuần - 60 giờ)

### 1. React setup với TypeScript (6h)

**Create project**:
```bash
npx create-react-app gateway-web --template typescript
cd gateway-web
npm install axios recharts @tanstack/react-query socket.io-client
```

**Project structure**:
```
src/
├── components/
│   ├── Dashboard/
│   │   ├── Dashboard.tsx
│   │   ├── DeviceCard.tsx
│   │   └── QuickStats.tsx
│   ├── Devices/
│   │   ├── DeviceList.tsx
│   │   └── DeviceDetails.tsx
│   ├── Charts/
│   │   └── RealtimeChart.tsx
│   └── Common/
│       ├── Layout.tsx
│       └── Navbar.tsx
├── services/
│   ├── api.ts
│   └── websocket.ts
├── hooks/
│   ├── useDevices.ts
│   └── useRealtimeData.ts
├── types/
│   └── index.ts
└── App.tsx
```

---

### 2. Dashboard UI components (14h)

**types/index.ts**:
```typescript
export interface Device {
  id: number;
  name: string;
  device_type: string;
  address: string;
  status: 'connected' | 'disconnected' | 'error';
  last_value?: number;
  last_update?: string;
}

export interface SensorData {
  id: number;
  device_id: number;
  timestamp: string;
  data_type: string;
  value: number;
  unit: string;
}

export interface Alarm {
  id: number;
  device_id: number;
  alarm_type: string;
  severity: 'info' | 'warning' | 'critical';
  message: string;
  timestamp: string;
  acknowledged: boolean;
}
```

**components/Dashboard/Dashboard.tsx**:
```typescript
import React, { useState, useEffect } from 'react';
import { useQuery } from '@tanstack/react-query';
import { fetchDevices, fetchAlarms } from '../../services/api';
import DeviceCard from './DeviceCard';
import QuickStats from './QuickStats';
import RealtimeChart from '../Charts/RealtimeChart';
import './Dashboard.css';

const Dashboard: React.FC = () => {
  const { data: devices, isLoading } = useQuery({
    queryKey: ['devices'],
    queryFn: fetchDevices,
    refetchInterval: 5000 // Refresh every 5 seconds
  });
  
  const { data: alarms } = useQuery({
    queryKey: ['alarms'],
    queryFn: () => fetchAlarms({ active: true })
  });
  
  if (isLoading) return <div>Loading...</div>;
  
  const onlineDevices = devices?.filter(d => d.status === 'connected').length || 0;
  const activeAlarms = alarms?.length || 0;
  
  return (
    <div className="dashboard">
      <h1>Gateway Dashboard</h1>
      
      <QuickStats 
        totalDevices={devices?.length || 0}
        onlineDevices={onlineDevices}
        activeAlarms={activeAlarms}
      />
      
      <div className="device-grid">
        {devices?.map(device => (
          <DeviceCard key={device.id} device={device} />
        ))}
      </div>
      
      <div className="charts-section">
        <h2>Real-time Data</h2>
        <RealtimeChart deviceId={1} dataType="temperature" />
      </div>
    </div>
  );
};

export default Dashboard;
```

---

### 3. Real-time data visualization (12h)

**services/websocket.ts**:
```typescript
import { io, Socket } from 'socket.io-client';

class WebSocketService {
  private socket: Socket | null = null;
  private listeners: Map<string, Function[]> = new Map();
  
  connect(url: string = 'http://localhost:3000') {
    this.socket = io(url);
    
    this.socket.on('connect', () => {
      console.log('WebSocket connected');
    });
    
    this.socket.on('mqtt_message', (data) => {
      this.notifyListeners('data', data);
    });
    
    this.socket.on('disconnect', () => {
      console.log('WebSocket disconnected');
    });
  }
  
  subscribe(topic: string, callback: Function) {
    if (!this.listeners.has(topic)) {
      this.listeners.set(topic, []);
    }
    this.listeners.get(topic)!.push(callback);
  }
  
  unsubscribe(topic: string, callback: Function) {
    const callbacks = this.listeners.get(topic);
    if (callbacks) {
      const index = callbacks.indexOf(callback);
      if (index > -1) {
        callbacks.splice(index, 1);
      }
    }
  }
  
  private notifyListeners(topic: string, data: any) {
    const callbacks = this.listeners.get(topic);
    if (callbacks) {
      callbacks.forEach(callback => callback(data));
    }
  }
  
  disconnect() {
    if (this.socket) {
      this.socket.disconnect();
      this.socket = null;
    }
  }
}

export default new WebSocketService();
```

**hooks/useRealtimeData.ts**:
```typescript
import { useState, useEffect } from 'react';
import websocketService from '../services/websocket';

interface DataPoint {
  timestamp: Date;
  value: number;
}

export const useRealtimeData = (deviceId: number, dataType: string, maxPoints: number = 100) => {
  const [data, setData] = useState<DataPoint[]>([]);
  
  useEffect(() => {
    const handleData = (message: any) => {
      if (message.data.device_id === deviceId && message.data.data_type === dataType) {
        setData(prev => {
          const newData = [...prev, {
            timestamp: new Date(message.data.timestamp),
            value: message.data.value
          }];
          
          // Keep only last N points
          if (newData.length > maxPoints) {
            newData.shift();
          }
          
          return newData;
        });
      }
    };
    
    websocketService.subscribe('data', handleData);
    
    return () => {
      websocketService.unsubscribe('data', handleData);
    };
  }, [deviceId, dataType, maxPoints]);
  
  return data;
};
```

**components/Charts/RealtimeChart.tsx**:
```typescript
import React from 'react';
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer } from 'recharts';
import { useRealtimeData } from '../../hooks/useRealtimeData';

interface Props {
  deviceId: number;
  dataType: string;
}

const RealtimeChart: React.FC<Props> = ({ deviceId, dataType }) => {
  const data = useRealtimeData(deviceId, dataType);
  
  return (
    <ResponsiveContainer width="100%" height={300}>
      <LineChart data={data}>
        <CartesianGrid strokeDasharray="3 3" />
        <XAxis 
          dataKey="timestamp" 
          tickFormatter={(timestamp) => new Date(timestamp).toLocaleTimeString()}
        />
        <YAxis />
        <Tooltip 
          labelFormatter={(timestamp) => new Date(timestamp).toLocaleString()}
        />
        <Line type="monotone" dataKey="value" stroke="#8884d8" dot={false} />
      </LineChart>
    </ResponsiveContainer>
  );
};

export default RealtimeChart;
```

---

## Phase 8: Testing & Polish (1.5 tuần - 45 giờ)

### 1. End-to-end testing (12h)

**Setup Pytest cho Python tests** (nếu có Python scripts):
```bash
pip install pytest pytest-asyncio
```

**Test scenarios**:
```cpp
// tests/e2e/test_full_flow.cpp
class TestEndToEnd : public QObject {
    Q_OBJECT
    
private slots:
    void testCompleteDataFlow() {
        // 1. Create and connect device
        DeviceInfo info;
        info.name = "Test Device";
        info.protocol = ProtocolType::ModbusRTU;
        
        int deviceId = DeviceManager::instance().addDevice(info);
        QVERIFY(deviceId > 0);
        
        DeviceManager::instance().connectDevice(deviceId);
        QTest::qWait(1000);
        
        Device* device = DeviceManager::instance().getDevice(deviceId);
        QVERIFY(device != nullptr);
        QCOMPARE(device->status(), DeviceStatus::Connected);
        
        // 2. Read data
        bool dataReceived = false;
        EventBus::instance().subscribe(EventType::DataReceived, 
            [&dataReceived](const Event& e) {
                dataReceived = true;
            });
        
        device->startPolling(100);
        QTest::qWait(2000);
        QVERIFY(dataReceived);
        
        // 3. Check database
        auto sensorData = database_->querySensorData(deviceId, 
                                                     QDateTime::currentDateTime().addSecs(-10),
                                                     QDateTime::currentDateTime());
        QVERIFY(!sensorData.isEmpty());
        
        // 4. Check cloud sync
        QTest::qWait(5000); // Wait for sync
        // Verify data in cloud/MQTT
        
        // Cleanup
        device->stopPolling();
        DeviceManager::instance().disconnectDevice(deviceId);
    }
    
    void testAlarmFlow() {
        // Test alarm triggering and notification
    }
    
    void testOfflineMode() {
        // Test offline buffering and sync after reconnect
    }
};
```

---

### 2. Performance testing & optimization (10h)

**Load testing**:
```cpp
void TestPerformance::testHighFrequencyPolling() {
    const int NUM_DEVICES = 10;
    const int POLL_INTERVAL_MS = 100;
    const int TEST_DURATION_SEC = 60;
    
    QList<Device*> devices;
    
    // Create multiple devices
    for (int i = 0; i < NUM_DEVICES; ++i) {
        DeviceInfo info;
        info.name = QString("Device %1").arg(i);
        info.protocol = ProtocolType::ModbusRTU;
        
        int id = DeviceManager::instance().addDevice(info);
        Device* device = DeviceManager::instance().getDevice(id);
        device->connect();
        device->startPolling(POLL_INTERVAL_MS);
        devices.append(device);
    }
    
    QElapsedTimer timer;
    timer.start();
    
    int totalDataPoints = 0;
    EventBus::instance().subscribe(EventType::DataReceived,
        [&totalDataPoints](const Event&) {
            totalDataPoints++;
        });
    
    // Run for test duration
    QTest::qWait(TEST_DURATION_SEC * 1000);
    
    qint64 elapsed = timer.elapsed();
    double dataRate = (totalDataPoints * 1000.0) / elapsed;
    
    qDebug() << "Performance test results:";
    qDebug() << "  Duration:" << elapsed << "ms";
    qDebug() << "  Total data points:" << totalDataPoints;
    qDebug() << "  Data rate:" << dataRate << "points/sec";
    qDebug() << "  Per device:" << (dataRate / NUM_DEVICES) << "points/sec";
    
    // Check memory usage
    // Check CPU usage
    
    // Cleanup
    for (auto device : devices) {
        device->stopPolling();
    }
}
```

**Memory profiling**:
```bash
valgrind --tool=massif --massif-out-file=massif.out ./gateway_system
ms_print massif.out
```

**CPU profiling**:
```bash
perf record -g ./gateway_system
perf report
```

---

### 3. Bug fixes từ testing (12h)

**Common bugs checklist**:
- [ ] Memory leaks (use Valgrind)
- [ ] Thread safety issues (use ThreadSanitizer)
- [ ] Null pointer dereferences
- [ ] Off-by-one errors in loops
- [ ] Resource leaks (file handles, sockets)
- [ ] Race conditions
- [ ] Deadlocks in multi-threading
- [ ] SQL injection vulnerabilities
- [ ] Buffer overflows
- [ ] Integer overflows

**Static analysis**:
```bash
# Clang-Tidy
clang-tidy src/**/*.cpp -- -std=c++17

# Cppcheck
cppcheck --enable=all --inconclusive src/
```

---

### 4. Documentation (6h)

**README.md structure**:
```markdown
# Gateway Control System

## Overview
Brief description of the system

## Features
- Real-time device monitoring
- Multi-protocol support (Modbus RTU, RS485, CAN)
- Cloud synchronization via MQTT
- Web-based dashboard
- Alarm and notification system

## Architecture
[Diagram showing components]

## Installation

### Prerequisites
- Qt 5.15 or later
- CMake 3.16+
- C++17 compiler
- Node.js 16+ (for backend)
- PostgreSQL 13+ (for backend database)

### Building from source
\`\`\`bash
mkdir build && cd build
cmake ..
make
\`\`\`

## Configuration

### Device Configuration
[Example JSON]

### MQTT Configuration
[Example]

## Usage

### Starting the Gateway
\`\`\`bash
./gateway_system --config config.json
\`\`\`

### Adding Devices
[Screenshots and instructions]

## API Documentation
[Link to API docs or inline documentation]

## Troubleshooting
Common issues and solutions

## Contributing
Guidelines for contributors

## License
MIT License

## Contact
[Your contact information]
```

**API documentation** (use Swagger/OpenAPI):
```yaml
openapi: 3.0.0
info:
  title: Gateway Control System API
  version: 1.0.0
paths:
  /api/devices:
    get:
      summary: Get all devices
      responses:
        '200':
          description: Success
          content:
            application/json:
              schema:
                type: array
                items:
                  $ref: '#/components/schemas/Device'
components:
  schemas:
    Device:
      type: object
      properties:
        id:
          type: integer
        name:
          type: string
        device_type:
          type: string
        status:
          type: string
          enum: [connected, disconnected, error]
```

**Code documentation** (Doxygen):
```bash
# Install Doxygen
sudo apt install doxygen graphviz

# Generate docs
doxygen Doxyfile

# View in browser
firefox docs/html/index.html
```

---

### 5. Code cleanup & refactoring (5h)

**Checklist**:
- [ ] Remove dead code
- [ ] Extract magic numbers to constants
- [ ] Improve variable/function names
- [ ] Add comments for complex logic
- [ ] Consistent code style (use clang-format)
- [ ] Split large files into smaller modules
- [ ] Reduce code duplication
- [ ] Simplify complex conditionals

**clang-format configuration** (.clang-format):
```yaml
BasedOnStyle: Google
IndentWidth: 4
ColumnLimit: 100
PointerAlignment: Left
```

**Run formatter**:
```bash
find src -name "*.cpp" -o -name "*.h" | xargs clang-format -i
```

---

## 📚 Tài liệu tham khảo

### C++ & Qt
- Qt Documentation: https://doc.qt.io/
- Qt Examples: https://doc.qt.io/qt-5/qtexamples.html
- C++ Core Guidelines: https://isocpp.github.io/CppCoreGuidelines/

### Protocols
- Modbus: https://modbus.org/docs/Modbus_Application_Protocol_V1_1b3.pdf
- libmodbus: https://libmodbus.org/documentation/
- RS485 Tutorial: https://www.ti.com/lit/an/slyt485/slyt485.pdf

### MQTT
- MQTT Specification: https://mqtt.org/mqtt-specification/
- Paho MQTT C++: https://github.com/eclipse/paho.mqtt.cpp
- Qt MQTT: https://doc.qt.io/qt-5/qtmqtt-index.html

### Testing
- Qt Test: https://doc.qt.io/qt-5/qtest-overview.html
- Google Test: https://github.com/google/googletest
- Valgrind: https://valgrind.org/docs/manual/manual.html

---

## 🎯 Tips thành công

1. **Commit thường xuyên**: Commit sau mỗi task nhỏ hoàn thành
2. **Test ngay**: Đừng để tích lũy bugs
3. **Document as you go**: Viết docs cùng lúc code
4. **Ask for help**: Tham gia Qt forum, Stack Overflow
5. **Keep it simple**: MVP trước, features sau
6. **Regular reviews**: Review code của chính mình sau 1-2 ngày
7. **Backup regularly**: Use Git, backup database
8. **Monitor progress**: Track hours spent vs estimated

## 📊 Progress Tracking Template

Tạo file `progress.md` để theo dõi:

```markdown
# Progress Tracking

## Week 1-2: Phase 1
- [x] Setup môi trường dev (8h) - Completed 2024-01-15
- [x] Thiết kế database schema (6h) - Completed 2024-01-16
- [ ] Tạo project structure (8h) - In progress
- [ ] Implement Event Bus (12h)
- [ ] Setup logging (6h)
- [ ] Qt main window (10h)
- [ ] Unit tests setup