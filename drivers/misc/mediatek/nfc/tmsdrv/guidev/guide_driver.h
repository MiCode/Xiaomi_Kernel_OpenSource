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
#if defined(CONFIG_TMS_ESE_DEVICE) || defined(CONFIG_TMS_ESE_DEVICE_MODULE)
#include "../ese/ese_driver.h"
#endif

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
#endif /* _TMS_GUIDEV_H_ */
