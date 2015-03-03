#include "application.h"

#if PORT_DEBUG
#define port_debug(m, ...)    debug_out(m, ##__VA_ARGS__)
#else
#define port_debug(m, ...)
#endif

static int wifi_enable = 0;

static uint8_t g_network_status = 0;

//-----------------------------------------
enum
{
    DEVMSG_STATUS = 1,
    DEVMSG_WARNING,
    DEVMSG_INVALID_CODE,
    DEVMSG_MACHINE_VERSION,
    DEVMSG_MACHINE_TYPE,
};

/* this makes remote thread simply call report function */
static int g_message_type;

static device_status_t g_device_status;
static int g_machine_warning;
static int g_invalid_code;
static char g_machine_version[16];
static char g_machine_type[21];

//############################################################
// Misc

void system_version(char *str, int len)
{
    sprintf(str, FIRMWARE_VERSION);
}

//############################################################
// Port for Send and Recv

static void send_to_device(uint8_t *buffer, uint32_t len)
{
    MicoUartSend(UART_FOR_APP, buffer, len);

    msleep(200);
}

static int recv_from_device(uint8_t *buffer, uint32_t len, uint32_t timeout)
{
    int ret;

    ret = MicoUartRecv(UART_FOR_APP, buffer, len, timeout);

    return ret;
}

static mico_semaphore_t sem_ack = NULL;

int wait_for_ack(uint32_t ms)
{
    int ret;

    if(NULL == sem_ack)
    {
        mico_rtos_init_semaphore(&sem_ack, 1);
    }

    ret = mico_rtos_get_semaphore(&sem_ack, ms);

    if(0 != ret)
    {
        return -1;
    }
    else
    {
        return 0;
    }
}

//############################################################
// Advanced Packup Send

static int generate_device_frame(uint8_t *dest, uint8_t *data, uint32_t datalen)
{
    uint16_t crc_result;

    dest[0] = 0xA5;
    dest[1] = 0x5A;
    dest[2] = 8 + datalen;
    memset(&dest[3], 0, 6);
    memcpy(&dest[9], data, datalen);
    crc_result = crc16_calc(&dest[2], 7 + datalen);
    dest[9 + datalen] = (uint8_t)(crc_result >> 8);
    dest[10 + datalen] = (uint8_t)(crc_result & 0xFF);

    return datalen + 11;
}

void SendFrame_ACK(uint8_t ack)
{
    int ret;
    uint8_t ackbuf[16];

    ret = generate_device_frame(ackbuf, &ack, 1);

    send_to_device(ackbuf, ret);
}

void SendFrame_ACK_EX(uint8_t ack, uint8_t extra)
{
    int ret;
    uint8_t tmpbuf[2];
    uint8_t ackbuf[16];

    tmpbuf[0] = ack;
    tmpbuf[1] = extra;

    ret = generate_device_frame(ackbuf, tmpbuf, 2);

    send_to_device(ackbuf, ret);
}

void SendFrame_CTRL(uint8_t *buffer, uint32_t len)
{
    send_to_device(buffer, len);
}
void SendFrame_NetLink(void)
{
    int ret;
    uint8_t cmdbuf[64];
    uint8_t databuf[5];

    databuf[0] =0x06;
    databuf[1] = 0x00;
    databuf[2] = g_network_status;
    databuf[3] = 0x00;
    databuf[4] = Get_WIFI_Strength();

    ret = generate_device_frame(cmdbuf, databuf, 5);

    send_to_device(cmdbuf, ret);
}
void SendFrame_FacotryTestResult(uint8_t result)
{
    uint8_t ackbuf[32];
    uint8_t databuf[2];
    int ret;

    databuf[0] = CMD_FACTORY_CHECK_RESULT;
    databuf[1] = result;

    ret = generate_device_frame(ackbuf, databuf, 2);
    send_to_device(ackbuf, ret);
}
void SendFrame_CheckStatus(void)
{
    uint8_t cmdbuf[64];
    uint8_t databuf[5];
    int ret;

    databuf[0] =0x1;
    databuf[1] = 0x1A;
    databuf[2] = 0x03;

    ret = generate_device_frame(cmdbuf, databuf, 3);

    send_to_device(cmdbuf, ret);
}

//############################################################
// System Event

void netlink_message_process(MSG *msg)
{
    ScanResult_adv *param = (ScanResult_adv*)msg->param;

    switch(msg->msg)
    {
        case NM_EL_ON:
            RFLED_Set_Mode(RFLED_MODE_QUICK);
            break;
        case NM_EL_OFF:
            RFLED_Set_Mode(RFLED_MODE_OFF);
            break;
        case NM_SCAN_ON:
            RFLED_Set_Mode(RFLED_MODE_QUICK);
            break;
        case NM_SCAN_OFF:


            RFLED_Set_Mode(RFLED_MODE_OFF);

            if((param->ApNum > 0) && (param->ApList[0].ApPower >= 70))
            {
                SendFrame_FacotryTestResult(1);
            }
            else
            {
                SendFrame_FacotryTestResult(0);
            }

            break;
        case NM_STA_ON:

            g_network_status = 1;

            RFLED_Set_Mode(RFLED_MODE_NORMAL);
            SendFrame_NetLink();
            break;
        case NM_STA_UP:

            g_network_status = 2;

            RFLED_Set_Mode(RFLED_MODE_SLOW);
            SendFrame_NetLink();
            break;
        case NM_STA_DOWN:

            g_network_status = 1;

            RFLED_Set_Mode(RFLED_MODE_NORMAL);
            SendFrame_NetLink();
            break;
        case NM_CLOUD_ON:

            g_network_status = 3;

            RFLED_Set_Mode(RFLED_MODE_OFF);
            SendFrame_NetLink();
            break;
        case NM_CLOUD_OFF:

            g_network_status = 2;

            RFLED_Set_Mode(RFLED_MODE_SLOW);
            SendFrame_NetLink();
            break;
    }
}


//############################################################
// Data from Device

int get_device_frame(uint8_t* buffer, uint32_t buflen, uint32_t timeout)
{
    int ret;
    int length;

    recv_from_device(&buffer[0], 1, timeout);

    if (0xA5 != buffer[0])
    {
        return -1;
    }

    ret = recv_from_device(&buffer[1], 1, 100);

    if ((-1 == ret) || (0x5A != buffer[1]))
    {
        return -1;
    }

    ret = recv_from_device(&buffer[2], 1, 100);

    if (-1 == ret)
    {
        return -1;
    }

    length = buffer[2];

    ret = recv_from_device(&buffer[3], length, 100);

    if (-1 == ret)
    {
        return -1;
    }

    return length + 3;
}

static void update_device_status(device_status_t *status, uint8_t *buffer)
{
    memset(status, 0, sizeof(device_status_t));

    //--------------------------------------------------------------------------
    sprintf(status->current, "%d.%d",
            bcd_decode_b(buffer[0]),
            bcd_decode_b(buffer[1]));
    sprintf(status->voltage, "%d",
            bcd_decode_b(buffer[2]) * 100 +
            bcd_decode_b(buffer[3]));
    sprintf(status->power1, "%d",
            bcd_decode_b(buffer[4]) * 100 +
            bcd_decode_b(buffer[5]));
    sprintf(status->power2, "%d",
            bcd_decode_b(buffer[6]) * 100 +
            bcd_decode_b(buffer[7]));

    sprintf(status->kwh, "%d.%d",
            // uint16_t can store number > 256
            (uint16_t)(
            (uint16_t)bcd_decode_b(buffer[8]) * 10000 +
            (uint16_t)bcd_decode_b(buffer[9]) * 100 +
            (uint16_t)bcd_decode_b(buffer[10])
            ),
            bcd_decode_b(buffer[11]));

    status->last_hour = buffer[12];
    status->last_min = buffer[13];
    status->cur_state = buffer[14] & 0xF;
    status->if_stop = (buffer[14] & 0x10) ? 1 : 0;
    status->if_drainoff = (buffer[14] & 0x20) ? 1 : 0;
    status->if_door_lock_on = (buffer[14] & 0x40) ? 1 : 0;
    status->if_has_warning = (buffer[14] & 0x80) ? 1 : 0;
    status->drainoff_timelast = buffer[15] & 0xF;
    status->if_ret_not_by_check = (buffer[15] & 0x080) ? 1 : 0;

    //--------------------------------------------------------------------------

    status->program = buffer[16];
    status->minites2wash = buffer[17];
    status->rinse_cnt = buffer[18] & 0x7;
    status->minites2dry = buffer[18] >> 3;
    status->water_temp = buffer[19] & 0x1F;
    status->speed2dry = buffer[19] >> 5;
    status->order_hours = buffer[20];
    status->soak_minites = buffer[21];
    status->dry_type = buffer[22] & 0xF;
    status->water_line = buffer[22]>>4;
    status->dry_minites = buffer[23];

    //--------------------------------------------------------------------------

    status->switch_speed_up = (buffer[24] & 0x01) ? 1 : 0;
    status->switch_child_lock = (buffer[24] & 0x02) ? 1 : 0;
    status->switch_drain_off_water = (buffer[24] & 0x04) ? 1 : 0;
    status->switch_cancel_end_beep = (buffer[24] & 0x08) ? 1 : 0;
    status->switch_strength_wash = (buffer[24] & 0x10) ? 1 : 0;
    status->switch_add_rinse = (buffer[24] & 0x20) ? 1 : 0;
    status->switch_remember = (buffer[24] & 0x40) ? 1 : 0;
    status->switch_led_on = (buffer[24] & 0x80) ? 1 : 0;
    status->switch_degerming = (buffer[25] & 0x01) ? 1 : 0;
    status->switch_night_wash = (buffer[25] & 0x02) ? 1 : 0;
    status->switch_power_save = (buffer[25] & 0x04) ? 1 : 0;
    status->switch_stop_if_dry = (buffer[25] & 0x08) ? 1 : 0;
}


void device_cmd_process(uint8_t *buffer, uint32_t len)
{
    static uint8_t same_compare[64] = {0};

    uint8_t cmd;
    uint8_t ackbuf[16];
    uint8_t databuf[8];
    device_status_t device_status;
    int ret;
    int i;

    cmd = buffer[9];

    switch (cmd)
    {
    case CMD_ENABLE_WIFI:

        port_debug("user enable wifi\r\n");

        wifi_enable = 1;

        SendFrame_ACK(ACK_ENABLE_WIFI);

        SendFrame_NetLink();

        break;

    case CMD_DISABLE_WIFI:

        port_debug("user disable wifi\r\n");

        wifi_enable = 0;

        SendFrame_ACK(ACK_DISABLE_WIFI);

        break;

    case ACK_DEVICE_STATUS:

        // if the device status has changed
        if(0 != memcmp(same_compare, buffer, len))
        {
            memcpy(same_compare, buffer, len);
        }
        else
        {
            break;
        }

        port_debug("new device status\r\n");

        g_message_type = DEVMSG_STATUS;

        update_device_status(&device_status, &buffer[12]);
        uart_notify_remote();

        break;

    case CMD_WARNING:

        port_debug("device warning report\r\n");

        g_message_type = DEVMSG_WARNING;

        if(buffer[17] & 0x1)
        {
            g_invalid_code = 1;
        }
        else if(buffer[17] & 0x2)
        {
            g_invalid_code = 2;
        }
        else if(buffer[17] & 0x8)
        {
            g_invalid_code = 3;
        }
        else if(buffer[16] & 0x1)
        {
            g_invalid_code = 4;
        }
        else if(buffer[13] & 0x2)
        {
            g_invalid_code = 5;
        }
        else if(buffer[13] & 0x4)
        {
            g_invalid_code = 6;
        }
        else if(buffer[12] & 0x4)
        {
            g_invalid_code = 7;
        }
        else
        {
            g_invalid_code = 0;
        }
        uart_notify_remote();

        break;

    case CMD_EASYLINK:

        port_debug("user start easylink\r\n");

        SendFrame_ACK(ACK_EASYLINK);

        User_Start_Easylink();

        break;

    case CMD_CHECK_NETSTATE:

        port_debug("user check netlink\r\n");

        SendFrame_ACK(ACK_CHECK_NETSTATE);

        SendFrame_NetLink();

        break;

    case CMD_ENTER_FACTORY_CHECK:

        port_debug("user enter factory check\r\n");

        SendFrame_ACK(ACK_ENTER_FACTORY_CHECK);

        User_Start_Scan();

        break;

    case CMD_CHECK_MODULE_VER:

        port_debug("user get wifi-firmware version\r\n");

        SendFrame_ACK_EX(ACK_CHECK_MODULE_VER, 0x10);

        break;

    default:

        port_debug("Warning, unsupported command:\r\n");

        for(i=0; i<len; i++)
        {
            port_debug("%02X ", buffer[i]);
        }

        port_debug("\r\n");

        break;
    }
}

static const char stream_machine_warning[] =
"{\"code\":103,\"device\":{\"feed_id\":\"%s\",\"access_key\":\"%s\"},\
\"streams\":[\
{\"stream_id\":\"machine_warnig\",\
\"datapoints\":[{\"value\":\"%d\"}]\
}\
]}\
\r\n";

static const char stream_device_status[] =
"{\"code\":103,\"device\":{\"feed_id\":\"%s\",\"access_key\":\"%s\"},\
\"streams\":[\
{\"stream_id\":\"current\",\
\"datapoints\":[{\"value\":\"%s\"}]\
},\
{\"stream_id\":\"voltage\",\
\"datapoints\":[{\"value\":\"%s\"}]\
},\
{\"stream_id\":\"power1\",\
\"datapoints\":[{\"value\":\"%s\"}]\
},\
{\"stream_id\":\"power2\",\
\"datapoints\":[{\"value\":\"%s\"}]\
},\
{\"stream_id\":\"kwh\",\
\"datapoints\":[{\"value\":\"%s\"}]\
},\
{\"stream_id\":\"last_hour\",\
\"datapoints\":[{\"value\":%d}]\
},\
{\"stream_id\":\"last_min\",\
\"datapoints\":[{\"value\":%d}]\
},\
{\"stream_id\":\"status\",\
\"datapoints\":[{\"value\":%d}]\
},\
{\"stream_id\":\"door_lock\",\
\"datapoints\":[{\"value\":%d}]\
},\
{\"stream_id\":\"wash_mode\",\
\"datapoints\":[{\"value\":%d}]\
},\
{\"stream_id\":\"minites2wash\",\
\"datapoints\":[{\"value\":%d}]\
},\
{\"stream_id\":\"rinse_cnt\",\
\"datapoints\":[{\"value\":%d}]\
},\
{\"stream_id\":\"minites2dry\",\
\"datapoints\":[{\"value\":%d}]\
},\
{\"stream_id\":\"water_temp\",\
\"datapoints\":[{\"value\":%d}]\
},\
{\"stream_id\":\"speed2dry\",\
\"datapoints\":[{\"value\":%d}]\
},\
{\"stream_id\":\"order_hours\",\
\"datapoints\":[{\"value\":%d}]\
},\
{\"stream_id\":\"soak_minites\",\
\"datapoints\":[{\"value\":%d}]\
},\
{\"stream_id\":\"dry_type\",\
\"datapoints\":[{\"value\":%d}]\
},\
{\"stream_id\":\"water_line\",\
\"datapoints\":[{\"value\":%d}]\
},\
{\"stream_id\":\"dry_minites\",\
\"datapoints\":[{\"value\":%d}]\
},\
{\"stream_id\":\"switch_child_lock\",\
\"datapoints\":[{\"value\":%d}]\
},\
{\"stream_id\":\"switch_drain_off_water\",\
\"datapoints\":[{\"value\":%d}]\
},\
{\"stream_id\":\"switch_cancel_end_beep\",\
\"datapoints\":[{\"value\":%d}]\
},\
{\"stream_id\":\"switch_strength_wash\",\
\"datapoints\":[{\"value\":%d}]\
},\
{\"stream_id\":\"switch_add_rinse\",\
\"datapoints\":[{\"value\":%d}]\
},\
{\"stream_id\":\"switch_remember\",\
\"datapoints\":[{\"value\":%d}]\
},\
{\"stream_id\":\"switch_led_on\",\
\"datapoints\":[{\"value\":%d}]\
},\
{\"stream_id\":\"switch_degerming\",\
\"datapoints\":[{\"value\":%d}]\
},\
{\"stream_id\":\"switch_night_wash\",\
\"datapoints\":[{\"value\":%d}]\
},\
{\"stream_id\":\"switch_power_save\",\
\"datapoints\":[{\"value\":%d}]\
},\
{\"stream_id\":\"switch_stop_if_dry\",\
\"datapoints\":[{\"value\":%d}]\
},\
{\"stream_id\":\"switch_speed_up\",\
\"datapoints\":[{\"value\":%d}]\
}\
]}\
\r\n";

int generate_device_status_stream(uint8_t *buffer, uint32_t buflen, device_status_t *device_status)
{
    appinfo_t *info = appinfo_get();

    sprintf(buffer, stream_device_status,
        info->jd.feed_id,
        info->jd.access_key,

        device_status->current,
        device_status->voltage,
        device_status->power1,
        device_status->power2,
        device_status->kwh,
        device_status->last_hour,
        device_status->last_min,
        device_status->cur_state,
        device_status->if_door_lock_on,

        device_status->program,
        device_status->minites2wash,
        device_status->rinse_cnt,
        device_status->minites2dry,
        device_status->water_temp,
        device_status->speed2dry,
        device_status->order_hours,
        device_status->soak_minites,
        device_status->dry_type,
        device_status->water_line,
        device_status->dry_minites,

        device_status->switch_child_lock,
        device_status->switch_drain_off_water,
        device_status->switch_cancel_end_beep,
        device_status->switch_strength_wash,
        device_status->switch_add_rinse,
        device_status->switch_remember,
        device_status->switch_led_on,
        device_status->switch_degerming,
        device_status->switch_night_wash,
        device_status->switch_power_save,
        device_status->switch_stop_if_dry,
        device_status->switch_speed_up
        );

    return strlen(buffer);
}

int generate_warning_stream(uint8_t *buffer, uint32_t buflen, int warning)
{
    appinfo_t *info = appinfo_get();

    sprintf(buffer, stream_machine_warning,
        info->jd.feed_id,
        info->jd.access_key,

        warning);

    return strlen(buffer);
}

int generate_network_stream(uint8_t *buffer, uint32_t buflen)
{
    int length;

    switch(g_message_type)
    {
        case DEVMSG_STATUS:
            length = generate_device_status_stream(buffer, buflen, &g_device_status);
            break;
        case DEVMSG_WARNING:
            length = generate_warning_stream(buffer, buflen, g_machine_warning);
            break;
        default:
            length = 0;
            break;
    }

    return length;
}

//############################################################
// Data from Network

int app_cmd_proc(uint8_t *key, uint32_t value)
{
    uint8_t cmdbuf[64];
    uint8_t databuf[5];

    // control type
    databuf[0] =0x1;

    if (0 == strcmp(key, "switch"))
    {
        databuf[1] = 0x1A;
        databuf[2] = (0 == value) ? 0 : 1;
        generate_device_frame(cmdbuf, databuf, 3);
    }
    else if(0 == strcmp(key, "wash"))
    {
        databuf[1] = 0x1A;
        databuf[2] = (1==value)?0x0f:0x02;
        generate_device_frame(cmdbuf, databuf, 3);
    }
    else if(0 == strcmp(key, "baby_lock"))
    {
        databuf[1] = 0x1A;
        databuf[2] = (1==value)?0x04:0x05;
        generate_device_frame(cmdbuf, databuf, 3);
    }
    else if(0 == strcmp(key, "door_lock"))
    {
        databuf[1] = 0x1A;
        databuf[2] = (1==value)?0x0B:0x0C;
        generate_device_frame(cmdbuf, databuf, 3);
    }
    else if(0 == strcmp(key, "exclude_water"))
    {
        databuf[1] = 0x1A;
        databuf[2] = (1==value)?0x08:0x09;
        generate_device_frame(cmdbuf, databuf, 3);
    }
    else if(0 == strcmp(key, "silence"))
    {
        databuf[1] = 0x1A;
        databuf[2] = (1==value)?0x06:0x07;
        generate_device_frame(cmdbuf, databuf, 3);
    }
    else if(0 == strcmp(key, "wash_mode"))
    {
        databuf[1] = 0x1A;
        databuf[2] = 0x01;
        databuf[3] = value;
        generate_device_frame(cmdbuf, databuf, 4);
    }
    else if(0 == strcmp(key, "appointment_wash"))
    {
        databuf[1] = 0x1A;
        databuf[2] = 0x07;
        if(0 == value)
        {
            databuf[3] = 0x00;
            generate_device_frame(cmdbuf, databuf, 4);
        }
        else
        {
            return -1;
        }
    }
    else if(0 == strcmp(key, "appointment_time"))
    {
        databuf[1] = 0x1A;
        databuf[2] = 0x07;
        databuf[3] = value;
        generate_device_frame(cmdbuf, databuf, 4);
    }
    else if(0 == strcmp(key, "wash_time"))
    {
        databuf[1] = 0x1A;
        databuf[2] = 0x02;
        databuf[3] = value;
        generate_device_frame(cmdbuf, databuf, 4);
    }
    else if(0 == strcmp(key, "rinse"))
    {
        databuf[1] = 0x1A;
        databuf[2] = 0x03;
        databuf[3] = value;
        generate_device_frame(cmdbuf, databuf, 4);
    }
    else if(0 == strcmp(key, "dehydratine_time"))
    {
        databuf[1] = 0x1A;
        databuf[2] = 0x04;
        databuf[3] = value;
        generate_device_frame(cmdbuf, databuf, 4);
    }
    else if(0 == strcmp(key, "dehydratine_rotate_speed"))
    {
        databuf[1] = 0x1A;
        databuf[2] = 0x06;
        switch(value)
        {
            case 500:
                databuf[3] = 1;
                break;
            case 700:
                databuf[3] = 2;
                break;
            case 900:
                databuf[3] = 3;
                break;
            case 1000:
                databuf[3] = 4;
                break;
            default:
                return -1;
        }
        generate_device_frame(cmdbuf, databuf, 4);
    }
    else
    {
        // not supported cmmmand
        return -1;
    }

    return 3;
}

