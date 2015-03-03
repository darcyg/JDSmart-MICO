#ifndef TRANSLATER_H
#define TRANSLATER_H

#define FIRMWARE_VERSION        "MXCHIP3162_JD_0101_001_V10"
#define FIRMWARE_VERSION_MAIN   1

#define PRODUCT_UUID            "EKPZ7T"

enum
{
    CMD_CTRL = 0x01,
        
    ACK_DEVICE_STATUS = 0x02,
    ACK_INVALID_CMD = 0x03,
    
    CMD_WARNING = 0x04,
    
    CMD_CHECK_VER = 7,
    ACK_CHECK_VER = 8,
    
    CMD_CHECK_TYPE = 9,
    ACK_CHECK_TYPE = 10,

    CMD_CHECK_WARNING = 0x0b,
    ACK_CHECK_WARNING = 0x0c,

    CMD_EASYLINK = 0x0d,
    ACK_EASYLINK = 0x05,

    CMD_LINKSTATE = 0x06,
    ACK_LINKSTATE = 0x1a,

    CMD_CHECK_NETSTATE = 0x12,
    ACK_CHECK_NETSTATE = 0x13,
    
    CMD_ENABLE_WIFI = 0x14,
    ACK_ENABLE_WIFI = 0x15,
    
    CMD_DISABLE_WIFI = 0x16,
    ACK_DISABLE_WIFI = 0x17,

    CMD_CHECK_IF_WIFI_ENABLE = 0x18,
    ACK_CHECK_IF_WIFI_ENABLE = 0x19,

    CMD_ENTER_FACTORY_CHECK = 0x0e,
    ACK_ENTER_FACTORY_CHECK = 0x1b,
    CMD_FACTORY_CHECK_RESULT = 0x0f,
    ACK_FACTORY_CHECK_RESULT = 0x10,

    CMD_CHECK_MODULE_VER = 0x1C,
    ACK_CHECK_MODULE_VER = 0x1D,
};

typedef struct
{
    // status
    uint8_t current[8]; // "2.16"A
    uint8_t voltage[8]; // "220"V
    uint8_t power1[8];
    uint8_t power2[8];
    uint8_t kwh[16]; // 3071.36 KW
    uint8_t last_hour;
    uint8_t last_min;
    uint8_t cur_state;
    uint8_t if_stop;
    uint8_t if_drainoff;
    uint8_t if_door_lock_on;
    uint8_t if_has_warning;
    uint8_t drainoff_timelast;
    uint8_t if_ret_not_by_check;

    // control info
    uint8_t program;
    uint8_t minites2wash;
    uint8_t rinse_cnt;
    uint8_t minites2dry;
    uint8_t water_temp;
    uint8_t speed2dry;
    uint8_t order_hours;
    uint8_t soak_minites;
    uint8_t dry_type;
    uint8_t water_line;
    uint8_t dry_minites;
    
    uint8_t switch_child_lock;
    uint8_t switch_drain_off_water;
    uint8_t switch_cancel_end_beep;
    uint8_t switch_strength_wash;
    uint8_t switch_add_rinse;
    uint8_t switch_remember;
    uint8_t switch_led_on;
    uint8_t switch_degerming;
    uint8_t switch_night_wash;
    uint8_t switch_power_save;
    uint8_t switch_stop_if_dry;
    uint8_t switch_speed_up;
}device_status_t;

#endif