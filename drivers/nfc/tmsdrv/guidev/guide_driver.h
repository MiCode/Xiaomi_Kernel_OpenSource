/*****************************************************************************************
 * Copyright (c)  2021- 2022  TsingTeng MicroSystem Corp.Ltd.
 * TMS_EDIT
 * File       : guide_driver.h
 * Description: Source file for tms devices guide
 * Version    : 1.0
 * Date       : 2022/4/11
 * Team       : NFC Middleware
 * Author     : Guoliang Wu
 * --------------------------------- Revision History: ---------------------------------
 *   <version>    <date>          < author >                            <desc>
 *******************************************************************************************/
#ifndef _TMS_GUIDEV_H_
#define _TMS_GUIDEV_H_
/*********** PART0: Head files ***********/
#include "../nfc/nfc_driver.h"
#include <linux/i2c.h>
#include <linux/gpio.h>
#if defined(CONFIG_TMS_ESE_DEVICE) || defined(CONFIG_TMS_ESE_DEVICE_MODULE)
#include "../ese/ese_driver.h"
#endif
/*add hardware info*/
//#include <linux/hardware_info.h>
/*********** PART1: Define Area ***********/
//#define GUIDEDEV_NAME         "tms,nfc"
#define GUIDEDEV_NAME         "qcom,nq-nci"
#ifdef TMS_MOUDLE
#undef TMS_MOUDLE
#define TMS_MOUDLE            "Guidev"
#endif
#define GUIDEV_VERSION        "1.0.221230"

#define MAX_MAJOR_VERSION_NUM (10)
#define MAX_CMD_LEN           (50)
#define TMS_CMD_HEAD_LEN      (3)
#define TMS_FW_CMP_BYTE       (3)
#define TMS_BL_CMP_BYTE       (4)
#define TMS_VERSION_MASK      (0xF0)

/* NCI */
#define NCI_HDR_LEN             (3)
#define NCI_PAYLOAD_LEN_BYTE    (2)
#define HEAD_PAYLOAD_BYTE       (2)
#define NCI_VENDOR_ID_BYTE      (6)

/* T1 Protocol Constants */  
#define T1_HDR_LEN              (3)
#define T1_PAYLOAD_LEN_BYTE     (2)
#define T1_LRC_LEN              (1)

/* Buffer Sizes */
#define MAX_T1_BUFFER_SIZE      (256)

#define WAIT_TIME_NONE          (0)
#define I2C_POLL_WAIT_TIME      (2000)
#define LOG_LEVEL_ERROR         (1)

static const uint8_t core_reset[] = {0x20, 0x00, 0x01, 0x00};
static const uint8_t bl_reset[] = {0x5A, 0xC4, 0x00, 0x9E};
static const uint8_t get_bl_ver[] = {0x5A, 0x00, 0x05, 0x00, 0x4A, 0x02, 0x00, 0x00, 0x17};
static const uint8_t get_hw_status[] = {0x5A, 0x40, 0x05, 0x00, 0x4A, 0xFE, 0x00, 0x00, 0xAB};

/*********** PART2: Struct Area ***********/
typedef enum {
    TMS_THN31,
    NXP_SN110X,
    SAMPLE_DEV_2,
    UNMATCH,
} chip_t;

typedef enum {
    TMS_FW,
    TMS_BL,
    SAMPLE_MATCH_1,
    SAMPLE_MATCH_2,
    UNKNOW,
} match_t;

struct match_info {
    int          sum;
    int          write_len;
    int          check_sum;
    int          ver_num;
    const int    read_retry;
    const int    write_retry;
    uint8_t      cmp;
    uint8_t      major_ver[MAX_MAJOR_VERSION_NUM];
    uint8_t      cmd[MAX_CMD_LEN];
    char         *name;
    chip_t       type;
    match_t      pattern;
};

struct guide_dev {
    struct i2c_client     *client;
    struct device         *dev;
    struct hw_resource    hw_res;
    struct tms_info       *tms;          /* tms common data */
};

/*********** PART3: Function or variables for other files ***********/
int tms_guide_init(void);
void tms_guide_exit(void);
void tms_nfc_hard_reset(struct tms_info *tms);
bool tms_nfc_write(struct i2c_client *client, const uint8_t *cmd, size_t len);
bool tms_nfc_read(struct i2c_client *client, unsigned int irq_gpio,
              uint8_t *data, size_t len, const char *msg);
#endif /* _TMS_GUIDEV_H_ */
