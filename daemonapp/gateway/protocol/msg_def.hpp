/**
 * @file    uart_msgs_def.h
 * @brief   Định nghĩa PAYLOAD cho các gói tin UART giữa ESP32 BLE Mesh Provisioner
 *          và Raspberry Pi Gateway
 *
 * ┌──────────────────────────────────────────────────────────────────────────┐
 * │                          FRAME FORMAT                                    │
 * │                                                                          │
 * │  Byte:  0      1        2–3     4      5       6      7…(6+LEN)  end-2  │
 * │       [0xAA | OPCODE | ADDR  | SEQ  | TYPE  | LEN  | PAYLOAD  | CRC16] │
 * │                                                                          │
 * │  - START  : 0xAA (fixed)                                                 │
 * │  - OPCODE : loại lệnh/sự kiện (1 byte)                                  │
 * │  - ADDR   : unicast của node, little-endian (0x0000 nếu không áp dụng)  │
 * │  - SEQ    : sequence number, tăng dần mỗi frame DATA (dùng cho ARQ)     │
 * │  - TYPE   : 0x00=DATA, 0x01=ACK, 0x02=NACK                              │
 * │  - LEN    : số byte PAYLOAD (0 nếu không có)                             │
 * │  - PAYLOAD: nội dung — các struct bên dưới định nghĩa phần này           │
 * │  - CRC16  : CRC-CCITT(poly=0x1021, init=0xFFFF)                         │
 * │             tính từ OPCODE đến hết PAYLOAD (bỏ START và 2 byte CRC)     │
 * └──────────────────────────────────────────────────────────────────────────┘
 *
 * Quy ước opcode:
 *   0x01 – 0x1F : CMD  – Provisioning Control          (RPi → ESP32)
 *   0x20 – 0x2F : CMD  – Node Control (Generic)        (RPi → ESP32)
 *   0x30 – 0x3F : CMD  – Sensor / Actuator             (RPi → ESP32)
 *   0x40 – 0x4F : CMD  – Remote Provisioning (RPR)     (RPi → ESP32)
 *   0x50 – 0x5F : EVT  – System / Health Events        (ESP32 → RPi)
 *   0x60 – 0x6F : (Reserved)
 *   0x70 – 0x7F : CMD  – Feature Control               (RPi → ESP32)
 *   0x80 – 0x8F : EVT  – Provisioning Events           (ESP32 → RPi)
 *   0x90 – 0x9F : (Reserved)
 *   0xA0 – 0xAF : EVT  – Config Client Events          (ESP32 → RPi)
 *   0xB0 – 0xBF : EVT  – Generic Client Events         (ESP32 → RPi)
 *   0xC0 – 0xCF : EVT  – RPR Events                    (ESP32 → RPi)
 *   0xD0 – 0xDF : EVT  – Group / Heartbeat Events      (ESP32 → RPi)
 *   0xE0 – 0xEF : EVT  – Network / Feature State       (ESP32 → RPi)
 *
 * @note  Mỗi struct bên dưới mô tả CHỈ phần PAYLOAD của frame.
 *        ADDR trong frame = trường địa chỉ chính (ghi chú trong từng struct).
 *        Dùng: uart_frame_send(opcode, addr, (uint8_t*)&payload, sizeof(payload))
 *
 * @version 3.0
 */

#ifndef _MSGS_DEF_H
#define _MSGS_DEF_H

#include <stdint.h>

/* =========================================================================
 * FRAME CONSTANTS & OFFSETS
 * ========================================================================= */

#define UART_START_BYTE             0xAA

#define UART_FRAME_OFF_START        0
#define UART_FRAME_OFF_OPCODE       1
#define UART_FRAME_OFF_ADDR_LO      2
#define UART_FRAME_OFF_ADDR_HI      3
#define UART_FRAME_OFF_SEQ          4
#define UART_FRAME_OFF_TYPE         5
#define UART_FRAME_OFF_LEN          6
#define UART_FRAME_OFF_PAYLOAD      7

#define UART_FRAME_HEADER_LEN       7   /**< START + OPCODE + ADDR(2) + SEQ + TYPE + LEN */
#define UART_FRAME_CRC_LEN          2
#define UART_FRAME_OVERHEAD         (UART_FRAME_HEADER_LEN + UART_FRAME_CRC_LEN)

#define UART_FRAME_TYPE_DATA        0x00
#define UART_FRAME_TYPE_ACK         0x01
#define UART_FRAME_TYPE_NACK        0x02

#define UART_ARQ_MAX_RETRY          3
#define UART_ARQ_TIMEOUT_MS         500

typedef uint8_t uart_opcode_t;


/* =========================================================================
 * SECTION 1 — CMD: PROVISIONING CONTROL  (0x01 – 0x1F)  RPi → ESP32
 * ========================================================================= */

#define OPCODE_CMD_PROV_ENABLE              0x01  /**< Bật chế độ Provisioning */
#define OPCODE_CMD_PROV_DISABLE             0x02  /**< Tắt chế độ Provisioning */
#define OPCODE_CMD_ADD_UNPROV_DEV           0x03  /**< Thêm thiết bị vào danh sách theo dõi */
#define OPCODE_CMD_SET_DEV_UUID_MATCH       0x04  /**< Lọc thiết bị theo UUID prefix */
#define OPCODE_CMD_SET_NODE_NAME            0x05  /**< Đặt tên cho node */
#define OPCODE_CMD_DELETE_NODE              0x06  /**< Xóa node khỏi mạng Mesh */
#define OPCODE_CMD_ADD_LOCAL_APP_KEY        0x07  /**< Thêm AppKey vào Provisioner local */
#define OPCODE_CMD_BIND_APP_KEY_TO_MODEL    0x08  /**< Bind AppKey vào Model của node */
#define OPCODE_CMD_GROUP_ADD                0x09  /**< Thêm node vào group */
#define OPCODE_CMD_GROUP_DELETE             0x0A  /**< Xóa node khỏi group */
#define OPCODE_CMD_MODEL_PUB_SET            0x0B  /**< Cấu hình Model Publish */

/**
 * PROV_ENABLE / PROV_DISABLE / DELETE_NODE
 *   ADDR   : 0x0000 (PROV_ENABLE/DISABLE) | unicast node (DELETE_NODE)
 *   PAYLOAD: (trống)
 */

/** ADD_UNPROV_DEV — PAYLOAD
 *  ADDR: 0x0000
 */
typedef struct __attribute__((packed)) {
    uint8_t  uuid[16];   /**< UUID của thiết bị cần theo dõi */
    uint8_t  bearer;     /**< 0x00: PB-ADV, 0x01: PB-GATT */
} mesh_cmd_add_unprov_dev_t;

/** SET_DEV_UUID_MATCH — PAYLOAD
 *  ADDR: 0x0000
 */
typedef struct __attribute__((packed)) {
    uint8_t  uuid_match[8]; /**< 8 byte prefix để match UUID */
} mesh_cmd_set_uuid_match_t;

/** SET_NODE_NAME — PAYLOAD
 *  ADDR: unicast của node
 */
typedef struct __attribute__((packed)) {
    uint8_t  name[16];   /**< Tên node (null-terminated nếu có thể) */
} mesh_cmd_set_node_name_t;

/** ADD_LOCAL_APP_KEY — PAYLOAD
 *  ADDR: 0x0000  (thao tác local trên Provisioner)
 */
typedef struct __attribute__((packed)) {
    uint16_t net_idx;       /**< Network Key Index */
    uint16_t app_idx;       /**< Application Key Index */
    uint8_t  app_key[16];   /**< 128-bit Application Key */
} mesh_cmd_add_app_key_t;

/** BIND_APP_KEY_TO_MODEL — PAYLOAD
 *  ADDR: unicast của node
 */
typedef struct __attribute__((packed)) {
    uint16_t app_idx;       /**< Application Key Index cần bind */
    uint16_t model_id;      /**< SIG Model ID hoặc Vendor Model ID */
    uint16_t company_id;    /**< 0xFFFF nếu là SIG Model */
} mesh_cmd_bind_app_key_t;

/** GROUP_ADD — PAYLOAD
 *  ADDR: unicast của node
 */
typedef struct __attribute__((packed)) {
    uint16_t element_addr;  /**< Địa chỉ element chứa Model */
    uint16_t group_addr;    /**< Group address cần subscribe (VD: 0xC000) */
    uint16_t company_id;    /**< 0xFFFF nếu là SIG Model */
    uint16_t model_id;      /**< SIG Model ID hoặc Vendor Model ID */
} mesh_cmd_add_dev_to_group_t;

/** GROUP_DELETE — PAYLOAD
 *  ADDR: unicast của node
 */
typedef struct __attribute__((packed)) {
    uint16_t element_addr;  /**< Địa chỉ element cần unsubscribe */
    uint16_t group_addr;    /**< Group address cần unsubscribe */
    uint16_t company_id;    /**< 0xFFFF nếu là SIG Model */
    uint16_t model_id;      /**< SIG Model ID hoặc Vendor Model ID */
} mesh_cmd_remove_dev_from_group_t;

/** MODEL_PUB_SET — PAYLOAD
 *  ADDR: unicast của node
 */
typedef struct __attribute__((packed)) {
    uint16_t element_addr;  /**< Element chứa model cần phát */
    uint16_t pub_addr;      /**< Địa chỉ đích để phát tới (Group VD: 0xC000) */
    uint16_t company_id;    /**< 0xFFFF nếu là SIG Model */
    uint16_t model_id;      /**< SIG Model ID (VD: 0x1000) */
    uint8_t  pub_ttl;       /**< Default TTL */
    uint8_t  pub_period;    /**< Chu kỳ phát (Resolution & Steps theo chuẩn BLE Mesh) */
} mesh_cmd_model_pub_set_t;


/* =========================================================================
 * SECTION 2 — CMD: NODE CONTROL – GENERIC ON/OFF  (0x20 – 0x2F)  RPi → ESP32
 * ========================================================================= */

#define OPCODE_CMD_GENERIC_ONOFF_GET        0x20  /**< Đọc trạng thái On/Off */
#define OPCODE_CMD_GENERIC_ONOFF_SET        0x21  /**< Set On/Off có ACK */
#define OPCODE_CMD_GENERIC_ONOFF_SET_UNACK  0x22  /**< Set On/Off không cần ACK */

/**
 * GENERIC_ONOFF_GET
 *   ADDR   : unicast của node đích
 *   PAYLOAD: (trống)
 */

/** GENERIC_ONOFF_SET / SET_UNACK — PAYLOAD
 *  ADDR: unicast của node đích
 */
typedef struct __attribute__((packed)) {
    uint8_t  onoff;  /**< 0x00: Tắt, 0x01: Bật */
    uint8_t  tid;    /**< Transaction ID, tăng dần mỗi lệnh mới */
} mesh_cmd_onoff_set_t;


/* =========================================================================
 * SECTION 3 — CMD: SENSOR / ACTUATOR  (0x30 – 0x3F)  RPi → ESP32
 * ========================================================================= */

#define OPCODE_CMD_SENSOR_GET               0x30  /**< Đọc dữ liệu cảm biến từ node */
#define OPCODE_CMD_ACTUATOR_GET             0x31  /**< Đọc trạng thái actuator từ node */
#define OPCODE_CMD_ACTUATOR_SET             0x32  /**< Điều khiển actuator (có ACK) */
#define OPCODE_CMD_ACTUATOR_SET_UNACK       0x33  /**< Điều khiển actuator (không cần ACK) */

/** SENSOR_GET — PAYLOAD
 *  ADDR: unicast của node đích
 */
typedef struct __attribute__((packed)) {
    uint16_t sensor_id;     /**< ID cảm biến cần đọc */
} mesh_cmd_sensor_get_t;

/**
 * ACTUATOR_GET
 *   ADDR   : unicast của node đích
 *   PAYLOAD: (trống)
 */

/** ACTUATOR_SET / SET_UNACK — PAYLOAD
 *  ADDR: unicast của node đích
 */
typedef struct __attribute__((packed)) {
    uint16_t actuator_id;   /**< ID của actuator */
    uint16_t setpoint;      /**< Giá trị điều khiển (VD: 0–10000 cho dimmer %) */
    uint8_t  status;        /**< 0x00: OFF, 0x01: ON, 0xFF: ignore */
} mesh_cmd_actuator_set_t;


/* =========================================================================
 * SECTION 4 — CMD: REMOTE PROVISIONING (RPR)  (0x40 – 0x4F)  RPi → ESP32
 * ========================================================================= */

#define OPCODE_CMD_RPR_SCAN_GET             0x40  /**< Lấy trạng thái scan từ RPR Server */
#define OPCODE_CMD_RPR_SCAN_START           0x41  /**< Bắt đầu quét thiết bị qua RPR */
#define OPCODE_CMD_RPR_SCAN_STOP            0x42  /**< Dừng quét */
#define OPCODE_CMD_RPR_LINK_GET             0x43  /**< Lấy trạng thái link RPR */
#define OPCODE_CMD_RPR_LINK_OPEN            0x44  /**< Mở link tới thiết bị unprovisioned */
#define OPCODE_CMD_RPR_LINK_CLOSE           0x45  /**< Đóng link */
#define OPCODE_CMD_RPR_START_PROV           0x46  /**< Bắt đầu provisioning qua RPR */

/**
 * RPR_SCAN_GET / RPR_SCAN_STOP / RPR_LINK_GET
 *   ADDR   : unicast của RPR Server node
 *   PAYLOAD: (trống)
 */

/** RPR_SCAN_START — PAYLOAD
 *  ADDR: unicast của RPR Server node
 */
typedef struct __attribute__((packed)) {
    uint8_t  scan_items_limit;  /**< Số thiết bị tối đa (0 = không giới hạn) */
    uint8_t  timeout;           /**< Thời gian quét (giây) */
    uint8_t  uuid_filter_en;    /**< 0x01: lọc theo UUID, 0x00: quét tất cả */
    uint8_t  uuid[16];          /**< UUID cần lọc (chỉ dùng khi uuid_filter_en=1) */
} mesh_cmd_rpr_scan_start_t;

/** RPR_LINK_OPEN — PAYLOAD
 *  ADDR: unicast của RPR Server node
 */
typedef struct __attribute__((packed)) {
    uint8_t  uuid[16];      /**< UUID của thiết bị unprovisioned muốn kết nối */
    uint8_t  timeout_en;    /**< 0x01: có giới hạn thời gian, 0x00: không giới hạn */
    uint8_t  timeout;       /**< Thời gian chờ (giây), chỉ dùng khi timeout_en=1 */
} mesh_cmd_rpr_link_open_t;

/** RPR_LINK_CLOSE — PAYLOAD
 *  ADDR: unicast của RPR Server node
 */
typedef struct __attribute__((packed)) {
    uint8_t  reason;    /**< 0x00: Success, 0x01: Prohibited, 0x02: Fail */
} mesh_cmd_rpr_link_close_t;


/* =========================================================================
 * SECTION 5 — EVT: SYSTEM / HEALTH  (0x50 – 0x5F)  ESP32 → RPi
 * ========================================================================= */

#define OPCODE_EVT_NODE_RESET               0x50  /**< Node đã reset về factory */
#define OPCODE_EVT_HEALTH_FAULT             0x51  /**< Node báo lỗi (Health Fault) */
#define OPCODE_EVT_HEALTH_ATTENTION         0x52  /**< Node yêu cầu chú ý (Attention) */

/**
 * NODE_RESET
 *   ADDR   : unicast của node vừa reset
 *   PAYLOAD: (trống)
 */

/** HEALTH_FAULT — PAYLOAD
 *  ADDR: unicast của node báo lỗi
 */
typedef struct __attribute__((packed)) {
    uint8_t  fault_array[4];    /**< Mảng mã lỗi (tối đa 4 lỗi) */
} mesh_evt_health_fault_t;

/** HEALTH_ATTENTION — PAYLOAD
 *  ADDR: unicast của node
 */
typedef struct __attribute__((packed)) {
    uint8_t  attention;     /**< Thời gian Attention (giây), 0 = tắt */
} mesh_evt_health_attention_t;


/* =========================================================================
 * SECTION 6 — CMD: FEATURE CONTROL  (0x70 – 0x7F)  RPi → ESP32
 * ========================================================================= */

#define OPCODE_CMD_RELAY_SET                0x70  /**< Cấu hình Relay */
#define OPCODE_CMD_RELAY_GET                0x71  /**< Đọc trạng thái Relay */
#define OPCODE_CMD_PROXY_SET                0x72  /**< Cấu hình GATT Proxy */
#define OPCODE_CMD_PROXY_GET                0x73  /**< Đọc trạng thái Proxy */
#define OPCODE_CMD_FRIEND_SET               0x74  /**< Cấu hình Friend */
#define OPCODE_CMD_FRIEND_GET               0x75  /**< Đọc trạng thái Friend */

/**
 * RELAY_GET / PROXY_GET / FRIEND_GET
 *   ADDR   : unicast của node đích
 *   PAYLOAD: (trống)
 */

/** RELAY_SET — PAYLOAD
 *  ADDR: unicast của node đích
 */
typedef struct __attribute__((packed)) {
    uint8_t  relay_state;               /**< 0x00: Disable, 0x01: Enable */
    uint8_t  retransmit_count;          /**< Số lần relay lại (0–7) */
    uint8_t  retransmit_interval_ms;    /**< Khoảng cách giữa relay (bước 10ms, 0–31) */
} mesh_cmd_relay_set_t;

/** PROXY_SET / FRIEND_SET — PAYLOAD
 *  ADDR: unicast của node đích
 */
typedef struct __attribute__((packed)) {
    uint8_t  state;     /**< 0x00: Disable, 0x01: Enable */
} mesh_cmd_feature_set_t;


/* =========================================================================
 * SECTION 7 — EVT: PROVISIONING  (0x80 – 0x8F)  ESP32 → RPi
 * ========================================================================= */

#define OPCODE_EVT_PROV_ENABLE_COMP         0x80  /**< Chế độ Provisioning đã bật */
#define OPCODE_EVT_PROV_DISABLE_COMP        0x81  /**< Chế độ Provisioning đã tắt */
#define OPCODE_EVT_RECV_UNPROV_ADV_PKT      0x82  /**< Nhận gói quảng bá thiết bị chưa provision */
#define OPCODE_EVT_PROV_LINK_OPEN           0x83  /**< Link Provisioning đã mở */
#define OPCODE_EVT_PROV_LINK_CLOSE          0x84  /**< Link Provisioning đã đóng */
#define OPCODE_EVT_PROV_COMPLETE            0x85  /**< Provision thành công */
#define OPCODE_EVT_ADD_UNPROV_DEV_COMP      0x86  /**< Đã thêm thiết bị vào danh sách */
#define OPCODE_EVT_SET_DEV_UUID_MATCH_COMP  0x87  /**< Đã cấu hình UUID match filter */
#define OPCODE_EVT_SET_NODE_NAME_COMP       0x88  /**< Đã đặt tên node */
#define OPCODE_EVT_ADD_APP_KEY_COMP         0x89  /**< Đã thêm AppKey vào local */
#define OPCODE_EVT_BIND_APP_KEY_COMP        0x8A  /**< Đã bind AppKey vào Model */
#define OPCODE_EVT_GROUP_ADD_COMP           0x8B  /**< Node đã được thêm vào group */
#define OPCODE_EVT_GROUP_REMOVE_COMP        0x8C  /**< Node đã bị xóa khỏi group */

/** EVT status đơn giản — PAYLOAD
 *  Dùng cho: PROV_ENABLE_COMP, PROV_DISABLE_COMP, ADD_UNPROV_DEV_COMP,
 *            SET_UUID_MATCH_COMP, SET_NODE_NAME_COMP, ADD_APP_KEY_COMP,
 *            BIND_APP_KEY_COMP, GROUP_ADD_COMP, GROUP_REMOVE_COMP
 *  ADDR: unicast của node (hoặc 0x0000 nếu không có node cụ thể)
 */
typedef struct __attribute__((packed)) {
    uint8_t  status_code;   /**< 0x00: OK, khác: mã lỗi */
} mesh_evt_status_t;

/** RECV_UNPROV_ADV_PKT — PAYLOAD
 *  ADDR: 0x0000 (chưa có địa chỉ unicast)
 */
typedef struct __attribute__((packed)) {
    uint8_t  uuid[16];      /**< UUID của thiết bị unprovisioned */
    uint8_t  bd_addr[6];    /**< Bluetooth Device Address */
    uint16_t oob_info;      /**< Out-of-Band Info bitfield */
    uint8_t  bearer;        /**< 0x00: PB-ADV, 0x01: PB-GATT */
    int8_t   rssi;          /**< Cường độ tín hiệu (dBm) */
} mesh_evt_unprov_adv_t;

/** PROV_LINK_OPEN / PROV_LINK_CLOSE — PAYLOAD
 *  ADDR: 0x0000
 */
typedef struct __attribute__((packed)) {
    uint8_t  bearer;    /**< 0x00: PB-ADV, 0x01: PB-GATT */
    uint8_t  reason;    /**< 0x00: Success, 0x01: Timeout, 0x02: Fail */
} mesh_evt_prov_link_t;

/** PROV_COMPLETE — PAYLOAD
 *  ADDR: unicast được cấp cho node
 */
typedef struct __attribute__((packed)) {
    uint16_t net_idx;       /**< Network Key Index */
    uint8_t  elem_num;      /**< Số lượng element của node */
    uint8_t  uuid[16];      /**< UUID của node vừa được provision */
} mesh_evt_prov_complete_t;


/* =========================================================================
 * SECTION 8 — EVT: CONFIG CLIENT & MODEL BINDING  (0xA0 – 0xAF)  ESP32 → RPi
 * ========================================================================= */

#define OPCODE_EVT_CFG_CLIENT_GET_STATE     0xA0  /**< Response GET từ node */
#define OPCODE_EVT_MODEL_COMP_DATA_STATUS   0xA1  /**< Composition Data của node */
#define OPCODE_EVT_CFG_CLIENT_SET_STATE     0xA2  /**< Response SET từ node */
#define OPCODE_EVT_MODEL_APP_KEY_STATUS     0xA3  /**< Trạng thái sau khi thêm AppKey */
#define OPCODE_EVT_MODEL_APP_BIND_STATUS    0xA4  /**< Trạng thái sau khi bind AppKey */
#define OPCODE_EVT_CFG_CLIENT_PUBLISH       0xA5  /**< Node tự publish thông tin thay đổi */
#define OPCODE_EVT_CFG_CLIENT_TIMEOUT       0xA6  /**< Node không phản hồi (timeout) */

/** CFG_MODEL_OP: App Key Add/Bind Status — PAYLOAD
 *  Dùng cho: EVT_MODEL_APP_KEY_STATUS, EVT_MODEL_APP_BIND_STATUS
 *  ADDR: unicast của node
 */
typedef struct __attribute__((packed)) {
    uint16_t model_id;      /**< Model ID liên quan */
    uint16_t company_id;    /**< 0xFFFF nếu là SIG Model */
    uint16_t app_idx;       /**< App Key Index */
    uint8_t  status_code;   /**< 0x00: OK, khác: mã lỗi theo BLE Mesh spec */
} mesh_evt_cfg_model_op_t;

/** MODEL_COMP_DATA_STATUS — PAYLOAD
 *  ADDR: unicast của node
 */
typedef struct __attribute__((packed)) {
    uint8_t  page;          /**< Trang Composition Data (thường là 0) */
    uint16_t data_len;      /**< Độ dài thực tế của data[] */
    uint8_t  data[60];      /**< Raw Composition Data */
} mesh_evt_comp_data_t;

/** CFG_CLIENT_TIMEOUT — PAYLOAD
 *  ADDR: unicast của node không phản hồi
 */
typedef struct __attribute__((packed)) {
    uint8_t  orig_opcode;   /**< Opcode của lệnh bị timeout */
} mesh_evt_cfg_timeout_t;


/* =========================================================================
 * SECTION 9 — EVT: GENERIC CLIENT  (0xB0 – 0xBF)  ESP32 → RPi
 * ========================================================================= */

#define OPCODE_EVT_GENERIC_ONOFF_STATUS     0xB0  /**< Trạng thái On/Off */
#define OPCODE_EVT_SENSOR_STATUS            0xB1  /**< Dữ liệu cảm biến từ node */
#define OPCODE_EVT_ACTUATOR_STATUS          0xB2  /**< Trạng thái actuator từ node */
#define OPCODE_EVT_GENERIC_CLIENT_TIMEOUT   0xB3  /**< Timeout khi gửi lệnh Generic */

/** GENERIC_ONOFF_STATUS — PAYLOAD
 *  ADDR: unicast của node nguồn
 */
typedef struct __attribute__((packed)) {
    uint8_t  present_onoff; /**< Trạng thái hiện tại: 0x00 Tắt, 0x01 Bật */
    uint8_t  target_onoff;  /**< Trạng thái đích (= present nếu không đang chuyển) */
} mesh_evt_onoff_status_t;

/** SENSOR_STATUS — PAYLOAD
 *  ADDR: unicast của node nguồn
 */
typedef struct __attribute__((packed)) {
    uint16_t sensor_id;     /**< ID cảm biến */
    uint16_t lux;           /**< Độ chiếu sáng (lux) */
    int16_t  temperature;   /**< Nhiệt độ x10 (VD: 255 = 25.5°C) */
    uint8_t  humidity;      /**< Độ ẩm (%) */
    uint8_t  motion;        /**< 0x00: không có, 0x01: có chuyển động */
    uint8_t  battery;       /**< Mức pin (%) */
    uint8_t  status;        /**< 0x00: OK, khác: lỗi */
} mesh_evt_sensor_status_t;

/** ACTUATOR_STATUS — PAYLOAD
 *  ADDR: unicast của node nguồn
 */
typedef struct __attribute__((packed)) {
    uint16_t actuator_id;       /**< ID của actuator */
    uint16_t present_setpoint;  /**< Giá trị setpoint hiện tại */
    uint16_t target_setpoint;   /**< Giá trị setpoint đích */
    uint8_t  status;            /**< 0x00: OK, khác: lỗi */
} mesh_evt_actuator_status_t;

/** GENERIC_CLIENT_TIMEOUT — PAYLOAD
 *  ADDR: unicast của node không phản hồi
 */
typedef struct __attribute__((packed)) {
    uint8_t  orig_opcode;   /**< Opcode của lệnh bị timeout */
} mesh_evt_generic_timeout_t;


/* =========================================================================
 * SECTION 10 — EVT: REMOTE PROVISIONING (RPR)  (0xC0 – 0xCF)  ESP32 → RPi
 * ========================================================================= */

#define OPCODE_EVT_RPR_SCAN_STATUS          0xC0  /**< Trạng thái quét RPR Server */
#define OPCODE_EVT_RPR_SCAN_REPORT          0xC1  /**< Phát hiện thiết bị qua RPR */
#define OPCODE_EVT_RPR_LINK_STATUS          0xC2  /**< Trạng thái link RPR */
#define OPCODE_EVT_RPR_LINK_OPEN            0xC3  /**< Link RPR đã mở */
#define OPCODE_EVT_RPR_LINK_CLOSE           0xC4  /**< Link RPR đã đóng */
#define OPCODE_EVT_RPR_LINK_REPORT          0xC5  /**< Báo cáo trạng thái link RPR */
#define OPCODE_EVT_RPR_START_PROV_COMP      0xC6  /**< Lệnh Start Prov qua RPR được chấp nhận */
#define OPCODE_EVT_RPR_PROV_COMPLETE        0xC7  /**< Provision qua RPR hoàn tất */

/** RPR_SCAN_STATUS — PAYLOAD
 *  ADDR: unicast của RPR Server
 */
typedef struct __attribute__((packed)) {
    uint8_t  status;                /**< 0x00: OK, khác: mã lỗi */
    uint8_t  rpr_scanning;          /**< 0x00: không quét, 0x01: đang quét */
    uint8_t  scan_items_limit;      /**< Giới hạn số thiết bị quét */
    uint8_t  timeout;               /**< Thời gian còn lại (giây) */
} mesh_evt_rpr_scan_status_t;

/** RPR_SCAN_REPORT — PAYLOAD
 *  ADDR: unicast của RPR Server
 */
typedef struct __attribute__((packed)) {
    int8_t   rssi;          /**< Cường độ tín hiệu (dBm) */
    uint8_t  uuid[16];      /**< UUID của thiết bị phát hiện */
    uint16_t oob_info;      /**< Out-of-Band Info bitfield */
} mesh_evt_rpr_scan_report_t;

/** RPR_LINK_STATUS / RPR_LINK_REPORT — PAYLOAD
 *  ADDR: unicast của RPR Server
 */
typedef struct __attribute__((packed)) {
    uint8_t  status;        /**< 0x00: OK, khác: mã lỗi */
    uint8_t  rpr_state;     /**< 0x00: Idle, 0x01: Link Opening, 0x02: Link Active */
    uint8_t  reason;        /**< Lý do (0x00: không có lỗi) */
} mesh_evt_rpr_link_status_t;

/** RPR_LINK_OPEN / RPR_LINK_CLOSE / RPR_START_PROV_COMP — PAYLOAD
 *  ADDR: unicast của RPR Server
 */
typedef struct __attribute__((packed)) {
    uint8_t  status_code;   /**< 0x00: OK, khác: mã lỗi */
} mesh_evt_rpr_simple_t;

/** RPR_PROV_COMPLETE — PAYLOAD
 *  ADDR: unicast của RPR Server
 */
typedef struct __attribute__((packed)) {
    uint16_t net_idx;       /**< Network Key Index */
    uint16_t unicast;       /**< Địa chỉ unicast cấp cho node mới */
    uint8_t  elem_num;      /**< Số lượng element của node mới */
    uint8_t  uuid[16];      /**< UUID của node vừa được provision */
} mesh_evt_rpr_prov_complete_t;


/* =========================================================================
 * SECTION 11 — EVT: GROUP / HEARTBEAT  (0xD0 – 0xDF)  ESP32 → RPi
 * ========================================================================= */

#define OPCODE_EVT_MODEL_SUBSCRIBE_STATUS   0xD0  /**< Kết quả subscribe group */
#define OPCODE_EVT_MODEL_UNSUBSCRIBE_STATUS 0xD1  /**< Kết quả unsubscribe group */
#define OPCODE_EVT_MODEL_PUBLISH_STATUS     0xD2  /**< Kết quả cấu hình publish */
#define OPCODE_EVT_HEARTBEAT                0xD3  /**< Nhận được Heartbeat từ node */
#define OPCODE_EVT_HEARTBEAT_PUB_STATUS     0xD4  /**< Trạng thái cấu hình Heartbeat Publish */
#define OPCODE_EVT_HEARTBEAT_SUB_STATUS     0xD5  /**< Trạng thái cấu hình Heartbeat Subscribe */
#define OPCODE_CMD_HEARTBEAT_SUB            0xD8  /**< Cấu hình Heartbeat Subscribe */
#define OPCODE_CMD_HEARTBEAT_PUB            0xD9  /**< Cấu hình Heartbeat Publish */

/** MODEL_SUBSCRIBE / UNSUBSCRIBE / PUBLISH STATUS — PAYLOAD
 *  ADDR: unicast của node
 */
typedef struct __attribute__((packed)) {
    uint16_t model_id;      /**< Model ID liên quan */
    uint16_t company_id;    /**< 0xFFFF nếu là SIG Model */
    uint16_t group_addr;    /**< Địa chỉ group */
    uint8_t  status_code;   /**< 0x00: OK, khác: mã lỗi */
} mesh_evt_group_op_t;

/** HEARTBEAT — PAYLOAD
 *  ADDR: src_unicast (node gửi Heartbeat)
 */
typedef struct __attribute__((packed)) {
    uint8_t  init_ttl;  /**< TTL ban đầu khi gửi Heartbeat */
    uint8_t  hops;      /**< Số hop thực tế đã đi qua */
    uint16_t features;  /**< Bitfield: bit0=Relay, bit1=Proxy, bit2=Friend, bit3=LPN */
} mesh_evt_heartbeat_t;

/** HEARTBEAT_PUB_STATUS — PAYLOAD
 *  ADDR: unicast của node
 */
typedef struct __attribute__((packed)) {
    uint8_t  status;    /**< 0x00: OK, khác: mã lỗi */
    uint16_t dst;       /**< Địa chỉ đích publish Heartbeat */
    uint8_t  period;    /**< Chu kỳ phát (log scale) */
    uint8_t  ttl;       /**< TTL mặc định */
} mesh_evt_heartbeat_pub_status_t;

/** HEARTBEAT_SUB_STATUS — PAYLOAD
 *  ADDR: unicast của node
 */
typedef struct __attribute__((packed)) {
    uint8_t  status;    /**< 0x00: OK, khác: mã lỗi */
    uint16_t dst;       /**< Địa chỉ đích subscribe Heartbeat */
    uint8_t  period;    /**< Chu kỳ nhận (log scale) */
} mesh_evt_heartbeat_sub_status_t;

/** CMD_HEARTBEAT_SUB — PAYLOAD
 *  ADDR: unicast của node đích
 */
typedef struct __attribute__((packed)) {
    uint16_t group_addr;    /**< Group address cần subscribe Heartbeat */
} mesh_cmd_heartbeat_sub_t;

/** CMD_HEARTBEAT_PUB — PAYLOAD
 *  ADDR: unicast của node đích
 */
typedef struct __attribute__((packed)) {
    uint16_t dst;       /**< Địa chỉ đích để phát Heartbeat */
    uint8_t  period;    /**< Chu kỳ phát (log scale) */
    uint8_t  ttl;       /**< TTL mặc định */
} mesh_cmd_heartbeat_pub_t;


/* =========================================================================
 * SECTION 12 — EVT: NETWORK / FEATURE STATE  (0xE0 – 0xEF)  ESP32 → RPi
 * ========================================================================= */

#define OPCODE_EVT_RELAY_STATUS             0xE0  /**< Trạng thái Relay của node */
#define OPCODE_EVT_PROXY_STATUS             0xE1  /**< Trạng thái GATT Proxy của node */
#define OPCODE_EVT_FRIEND_STATUS            0xE2  /**< Trạng thái Friend của node */
#define OPCODE_EVT_FRIENDSHIP_ESTABLISHED   0xE3  /**< LPN và Friend đã thiết lập quan hệ */
#define OPCODE_EVT_FRIENDSHIP_TERMINATED    0xE4  /**< Quan hệ LPN-Friend bị hủy */
#define OPCODE_EVT_NET_KEY_ADD              0xE5  /**< NetKey đã được thêm vào node */
#define OPCODE_EVT_NET_KEY_UPDATE           0xE6  /**< NetKey đã được cập nhật */
#define OPCODE_EVT_NET_KEY_DELETE           0xE7  /**< NetKey đã bị xóa */

/** RELAY_STATUS — PAYLOAD
 *  ADDR: unicast của node
 */
typedef struct __attribute__((packed)) {
    uint8_t  relay_state;               /**< 0x00: Disable, 0x01: Enable, 0x02: Not Supported */
    uint8_t  retransmit_count;          /**< Số lần relay (0–7) */
    uint8_t  retransmit_interval_ms;    /**< Khoảng cách giữa relay (bước 10ms) */
} mesh_evt_relay_status_t;

/** PROXY_STATUS / FRIEND_STATUS — PAYLOAD
 *  ADDR: unicast của node
 */
typedef struct __attribute__((packed)) {
    uint8_t  state;     /**< 0x00: Disable, 0x01: Enable, 0x02: Not Supported */
} mesh_evt_feature_simple_t;

/** FRIENDSHIP_ESTABLISHED / FRIENDSHIP_TERMINATED — PAYLOAD
 *  ADDR: lpn_unicast
 */
typedef struct __attribute__((packed)) {
    uint16_t friend_unicast;    /**< Địa chỉ Friend node */
} mesh_evt_friendship_t;

/** NET_KEY_ADD / UPDATE / DELETE — PAYLOAD
 *  ADDR: unicast của node liên quan
 */
typedef struct __attribute__((packed)) {
    uint16_t net_idx;       /**< Network Key Index */
    uint8_t  status_code;   /**< 0x00: OK, khác: mã lỗi */
} mesh_evt_net_key_t;


/* =========================================================================
 * PAYLOAD SIZE DEFINITIONS
 * ========================================================================= */

/* --- SECTION 1: CMD Provisioning Control --- */
#define PAYLOAD_SIZE_CMD_PROV_ENABLE        0
#define PAYLOAD_SIZE_CMD_PROV_DISABLE       0
#define PAYLOAD_SIZE_CMD_DELETE_NODE        0
#define PAYLOAD_SIZE_CMD_ADD_UNPROV_DEV     sizeof(mesh_cmd_add_unprov_dev_t)           /* 17 B */
#define PAYLOAD_SIZE_CMD_SET_UUID_MATCH     sizeof(mesh_cmd_set_uuid_match_t)           /*  8 B */
#define PAYLOAD_SIZE_CMD_SET_NODE_NAME      sizeof(mesh_cmd_set_node_name_t)            /* 16 B */
#define PAYLOAD_SIZE_CMD_ADD_APP_KEY        sizeof(mesh_cmd_add_app_key_t)              /* 20 B */
#define PAYLOAD_SIZE_CMD_BIND_APP_KEY       sizeof(mesh_cmd_bind_app_key_t)             /*  6 B */
#define PAYLOAD_SIZE_CMD_GROUP_ADD          sizeof(mesh_cmd_add_dev_to_group_t)         /*  8 B */
#define PAYLOAD_SIZE_CMD_GROUP_DELETE       sizeof(mesh_cmd_remove_dev_from_group_t)    /*  8 B */
#define PAYLOAD_SIZE_CMD_MODEL_PUB_SET      sizeof(mesh_cmd_model_pub_set_t)            /* 10 B */

/* --- SECTION 2: CMD Generic On/Off --- */
#define PAYLOAD_SIZE_CMD_ONOFF_GET          0
#define PAYLOAD_SIZE_CMD_ONOFF_SET          sizeof(mesh_cmd_onoff_set_t)                /*  2 B */

/* --- SECTION 3: CMD Sensor / Actuator --- */
#define PAYLOAD_SIZE_CMD_SENSOR_GET         sizeof(mesh_cmd_sensor_get_t)               /*  2 B */
#define PAYLOAD_SIZE_CMD_ACTUATOR_GET       0
#define PAYLOAD_SIZE_CMD_ACTUATOR_SET       sizeof(mesh_cmd_actuator_set_t)             /*  5 B */

/* --- SECTION 4: CMD Remote Provisioning (RPR) --- */
#define PAYLOAD_SIZE_CMD_RPR_SCAN_GET       0
#define PAYLOAD_SIZE_CMD_RPR_SCAN_START     sizeof(mesh_cmd_rpr_scan_start_t)           /* 19 B */
#define PAYLOAD_SIZE_CMD_RPR_SCAN_STOP      0
#define PAYLOAD_SIZE_CMD_RPR_LINK_GET       0
#define PAYLOAD_SIZE_CMD_RPR_LINK_OPEN      sizeof(mesh_cmd_rpr_link_open_t)            /* 18 B */
#define PAYLOAD_SIZE_CMD_RPR_LINK_CLOSE     sizeof(mesh_cmd_rpr_link_close_t)           /*  1 B */

/* --- SECTION 5: EVT System / Health --- */
#define PAYLOAD_SIZE_EVT_NODE_RESET         0
#define PAYLOAD_SIZE_EVT_HEALTH_FAULT       sizeof(mesh_evt_health_fault_t)             /*  4 B */
#define PAYLOAD_SIZE_EVT_HEALTH_ATTENTION   sizeof(mesh_evt_health_attention_t)         /*  1 B */

/* --- SECTION 6: CMD Feature Control --- */
#define PAYLOAD_SIZE_CMD_FEATURE_GET        0
#define PAYLOAD_SIZE_CMD_RELAY_SET          sizeof(mesh_cmd_relay_set_t)                /*  3 B */
#define PAYLOAD_SIZE_CMD_FEATURE_SET        sizeof(mesh_cmd_feature_set_t)              /*  1 B */

/* --- SECTION 7: EVT Provisioning --- */
#define PAYLOAD_SIZE_EVT_STATUS             sizeof(mesh_evt_status_t)                   /*  1 B */
#define PAYLOAD_SIZE_EVT_UNPROV_ADV         sizeof(mesh_evt_unprov_adv_t)               /* 26 B */
#define PAYLOAD_SIZE_EVT_PROV_LINK          sizeof(mesh_evt_prov_link_t)                /*  2 B */
#define PAYLOAD_SIZE_EVT_PROV_COMPLETE      sizeof(mesh_evt_prov_complete_t)            /* 19 B */

/* --- SECTION 8: EVT Config Client --- */
#define PAYLOAD_SIZE_EVT_CFG_MODEL_OP       sizeof(mesh_evt_cfg_model_op_t)             /*  7 B */
#define PAYLOAD_SIZE_EVT_COMP_DATA          sizeof(mesh_evt_comp_data_t)                /* 63 B */
#define PAYLOAD_SIZE_EVT_CFG_TIMEOUT        sizeof(mesh_evt_cfg_timeout_t)              /*  1 B */

/* --- SECTION 9: EVT Generic Client --- */
#define PAYLOAD_SIZE_EVT_ONOFF_STATUS       sizeof(mesh_evt_onoff_status_t)             /*  2 B */
#define PAYLOAD_SIZE_EVT_SENSOR_STATUS      sizeof(mesh_evt_sensor_status_t)            /* 10 B */
#define PAYLOAD_SIZE_EVT_ACTUATOR_STATUS    sizeof(mesh_evt_actuator_status_t)          /*  7 B */
#define PAYLOAD_SIZE_EVT_GENERIC_TIMEOUT    sizeof(mesh_evt_generic_timeout_t)          /*  1 B */

/* --- SECTION 10: EVT Remote Provisioning (RPR) --- */
#define PAYLOAD_SIZE_EVT_RPR_SCAN_STATUS    sizeof(mesh_evt_rpr_scan_status_t)          /*  4 B */
#define PAYLOAD_SIZE_EVT_RPR_SCAN_REPORT    sizeof(mesh_evt_rpr_scan_report_t)          /* 19 B */
#define PAYLOAD_SIZE_EVT_RPR_LINK_STATUS    sizeof(mesh_evt_rpr_link_status_t)          /*  3 B */
#define PAYLOAD_SIZE_EVT_RPR_SIMPLE         sizeof(mesh_evt_rpr_simple_t)               /*  1 B */
#define PAYLOAD_SIZE_EVT_RPR_PROV_COMPLETE  sizeof(mesh_evt_rpr_prov_complete_t)        /* 21 B */

/* --- SECTION 11: EVT Group / Heartbeat --- */
#define PAYLOAD_SIZE_EVT_GROUP_OP           sizeof(mesh_evt_group_op_t)                 /*  7 B */
#define PAYLOAD_SIZE_EVT_HEARTBEAT          sizeof(mesh_evt_heartbeat_t)                /*  4 B */
#define PAYLOAD_SIZE_EVT_HB_PUB_STATUS      sizeof(mesh_evt_heartbeat_pub_status_t)     /*  5 B */
#define PAYLOAD_SIZE_EVT_HB_SUB_STATUS      sizeof(mesh_evt_heartbeat_sub_status_t)     /*  4 B */
#define PAYLOAD_SIZE_CMD_HEARTBEAT_SUB      sizeof(mesh_cmd_heartbeat_sub_t)            /*  2 B */
#define PAYLOAD_SIZE_CMD_HEARTBEAT_PUB      sizeof(mesh_cmd_heartbeat_pub_t)            /*  4 B */

/* --- SECTION 12: EVT Network / Feature State --- */
#define PAYLOAD_SIZE_EVT_RELAY_STATUS       sizeof(mesh_evt_relay_status_t)             /*  3 B */
#define PAYLOAD_SIZE_EVT_FEATURE_SIMPLE     sizeof(mesh_evt_feature_simple_t)           /*  1 B */
#define PAYLOAD_SIZE_EVT_FRIENDSHIP         sizeof(mesh_evt_friendship_t)               /*  2 B */
#define PAYLOAD_SIZE_EVT_NET_KEY            sizeof(mesh_evt_net_key_t)                  /*  3 B */

/** Payload lớn nhất — dùng để cấp phát RX buffer tĩnh */
#define PAYLOAD_SIZE_MAX                    sizeof(mesh_evt_comp_data_t)                /* 63 B */

#endif /* _MSGS_DEF_H */
