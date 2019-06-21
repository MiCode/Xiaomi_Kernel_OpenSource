#ifndef __AW20036_H__
#define __AW20036_H__


unsigned char aw87519_spk_cfg_default[]={
    0x01,0xF0,
    0x02,0x09,
    0x03,0xE8,
    0x04,0x11,
    0x05,0x10,
    0x06,0x43,
    0x07,0x4E,
    0x08,0x03,
    0x09,0x08,
    0x0A,0x4A,
    0x60,0x16,
    0x61,0x20,
    0x62,0x01,
    0x63,0x0B,
    0x64,0xC5,
    0x65,0xA4,
    0x66,0x78,
    0x67,0xC4,
    0x68,0XD0
};

unsigned char aw87519_rcv_cfg_default[]={
    0x01,0xF8,
    0x02,0x09,
    0x03,0xC8,
    0x04,0x11,
    0x05,0x05,
    0x06,0x53,
    0x07,0x4E,
    0x08,0x0B,
    0x09,0x08,
    0x0A,0x4B,
    0x60,0x16,
    0x61,0x20,
    0x62,0x01,
    0x63,0x0B,
    0x64,0xC5,
    0x65,0xA4,
    0x66,0x78,
    0x67,0xC4,
    0x68,0XD0
};

/******************************************************
 *
 *Load config function
 *This driver will use load firmware if AW20036_BIN_CONFIG be defined
 *****************************************************/
#define AWINIC_CFG_UPDATE_DELAY
#define AW_I2C_RETRIES 5
#define AW_I2C_RETRY_DELAY 2
#define AW_READ_CHIPID_RETRIES 5
#define AW_READ_CHIPID_RETRY_DELAY 2

#define REG_CHIPID            0x00
#define REG_SYSCTRL           0x01
#define REG_BATSAFE           0x02
#define REG_BSTOVR            0x03
#define REG_BSTVPR            0x04
#define REG_PAGR              0x05
#define REG_PAGC3OPR          0x06
#define REG_PAGC3PR           0x07
#define REG_PAGC2OPR          0x08
#define REG_PAGC2PR           0x09
#define REG_PAGC1PR           0x0A

#define AW87519_CHIPID      0x59
#define AW87519_REG_MAX     11

struct aw87519_container{
    int len;
    unsigned char data[];
};

struct aw87519 {
    struct i2c_client *i2c_client;
    int reset_gpio;
    unsigned char hwen_flag;
    unsigned char spk_cfg_update_flag;
    unsigned char rcv_cfg_update_flag;
    unsigned char spk_cfg_data[sizeof(aw87519_spk_cfg_default)/sizeof(char)];
    unsigned char rcv_cfg_data[sizeof(aw87519_rcv_cfg_default)/sizeof(char)];
    struct hrtimer cfg_timer;
    struct mutex cfg_lock;
    struct work_struct cfg_work;
    struct delayed_work ram_work;
};
#endif
