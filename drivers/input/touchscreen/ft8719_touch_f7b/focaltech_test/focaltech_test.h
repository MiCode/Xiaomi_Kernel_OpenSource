/************************************************************************
* Copyright (C) 2012-2018, Focaltech Systems (R)£¬All Rights Reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
*
* File Name: focaltech_test.h
*
* Author: Focaltech Driver Team
*
* Created: 2016-08-01
*
* Abstract: test entry for all IC
*
************************************************************************/
#ifndef _TEST_LIB_H
#define _TEST_LIB_H

/*****************************************************************************
* Included header files
*****************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <asm/uaccess.h>
#include <linux/i2c.h>//iic
#include <linux/delay.h>//msleep
#include <linux/string.h>
#include <asm/unistd.h>
#include <linux/vmalloc.h>

#include "../focaltech_core.h"
#include "focaltech_test_detail_threshold.h"
#include "focaltech_test_ini.h"

/*****************************************************************************
* Macro definitions using #define
*****************************************************************************/
#define FTS_INI_FILE_PATH               "/mnt/user/"
#define FTS_TESTDATA_FILE_NAME          "testdata.csv"
#define FTS_TESTRESULT_FILE_NAME        "testresult.txt"
#define false 0
#define true  1
#define MAX_IC_NAME_LEN                         20
#define TEST_ICSERIES_LEN                       8
#define TEST_ICSERIES(x)                        ((x) >> TEST_ICSERIES_LEN)

#define TEST_OPEN_MAX_VALUE                     255
#define MAX_TEST_ITEM                           20
#define BYTES_PER_TIME                          32  //max:128
#define FTS_TEST_STORE_DATA_SIZE                80*1024
/*buff length*/
#define BUFF_LEN_STORE_MSG_AREA                 1024*10
#define BUFF_LEN_MSG_AREA_LINE2                 1024*4
#define BUFF_LEN_STORE_DATA_AREA                1024*80
#define BUFF_LEN_TMP_BUFFER                     1024*16
#define BUFF_LEN_TESTRESULT_BUFFER              1024*80*5

/*-----------------------------------------------------------
Error Code for Comm
-----------------------------------------------------------*/
#define ERROR_CODE_OK                           0x00
#define ERROR_CODE_INVALID_COMMAND              0x02
#define ERROR_CODE_INVALID_PARAM                0x03
#define ERROR_CODE_WAIT_RESPONSE_TIMEOUT        0x07
#define ERROR_CODE_COMM_ERROR                   0x0c
#define ERROR_CODE_ALLOCATE_BUFFER_ERROR        0x0d

/*-----------------------------------------------------------
Test Status
-----------------------------------------------------------*/
#define RESULT_NULL                             0
#define RESULT_PASS                             1
#define RESULT_NG                               2

/*
 * factory test registers
 */
#define ENTER_WORK_FACTORY_RETRIES              5
#define KEY_NUM_MAX                             6

#define START_SCAN_RETRIES_INCELL               20
#define START_SCAN_RETRIES_DELAY_INCELL         16
#define FACTORY_TEST_RETRY                      50
#define FACTORY_TEST_DELAY                      18
#define FACTORY_TEST_RETRY_DELAY                100

#define DEVIDE_MODE_ADDR                        0x00
#define REG_FW_VERSION                          0xA6
#define REG_VA_TOUCH_THR                        0x80
#define REG_VKEY_TOUCH_THR                      0x82

#define FACTORY_REG_LINE_ADDR                   0x01
#define FACTORY_REG_CHX_NUM                     0x02
#define FACTORY_REG_CHY_NUM                     0x03
#define FACTORY_REG_CLB                         0x04

#define FACTORY_REG_DATA_SELECT                 0x06
#define FACTORY_REG_RAWBUF_SELECT               0x09
#define FACTORY_REG_KEY_CBWIDTH                 0x0B
#define FACTORY_REG_PARAM_UPDATE_STATE          0x0E
#define FACTORY_REG_SHORT_TEST_EN               0x0F
#define FACTORY_REG_SHORT_TEST_STATE            0x10
#define FACTORY_REG_LCD_NOISE_START             0x11
#define FACTORY_REG_LCD_NOISE_FRAME             0x12
#define FACTORY_REG_LCD_NOISE_NUMBER            0x13
#define FACTORY_REG_CB_ADDR_H                   0x18
#define FACTORY_REG_CB_ADDR_L                   0x19

#define FACTORY_REG_LEFT_KEY                    0x1E
#define FACTORY_REG_RIGHT_KEY                   0x1F
#define FACTORY_REG_GIP_DRIVER_MODE             0x20
#define FACTORY_REG_SOURCE_DRIVER_MODE          0x21
#define FACTORY_REG_K1                          0x31
#define FACTORY_REG_K2                          0x32
#define FACTORY_REG_RAWDATA_ADDR                0x6A
#define FACTORY_REG_CB_ADDR                     0x6E
#define FACTORY_REG_SHORT_ADDR                  0x89
#define FACTORY_REG_RAWDATA_TEST_EN             0x9E
#define FACTORY_REG_CB_TEST_EN                  0x9F
#define FACTORY_REG_OPEN_TEST_EN                0xA0

#define TEST_RETVAL_00                          0x00
#define TEST_RETVAL_AA                          0xAA

/*****************************************************************************
* enumerations, structures and unions
*****************************************************************************/
struct test_funcs {
    u32 ic_series;
    void (*init_testitem)(char *);
    void (*init_basicthreshold)(char *);
    void (*init_detailthreshold)(char *);
    void (*set_testitem_sequence)(void);
    bool (*start_test)(void);
};

struct test_ic_type {
    char ic_name[MAX_IC_NAME_LEN];
    u8 len;
    u32 ic_type;
};

/*
 * selected_ic - ic type need test
 * normalize - auto normalize or overall normalize
 * tx_num - tx number
 * rx_num - rx number
 * channels_num - for 6x36
 * key_num - actually key number used
 * key_num_total - equal max key number
 * used_max_tx_num - tx_num <= used_max_tx_num
 * used_max_rx_num - rx_num <= used_max_rx_num
 * left_key1/2/3 - left key flag
 * right_key1/2/3 - right key flag
 */
struct screen_setting {
    int selected_ic;
    int normalize;
    int tx_num;
    int rx_num;
    u8 channels_num;
    u8 key_num;
    u8 key_num_total;
    u8 key_flag;
    int used_max_tx_num;
    int used_max_rx_num;

    bool left_key1;
    bool left_key2;
    bool left_key3;
    bool left_key4;
    bool right_key1;
    bool right_key2;
    bool right_key3;
};

/*
 * itemtype -
 * testnum - Test number
 * testresult - Test result, NG\PASS\TESTING
 * itemcode - Test item code
 */
struct test_item {
    u8 itemtype;
    u8 testnum;
    u8 testresult;
    u8 itemcode;
};

/*
 * test_num - test item numbers
 * testresult - test result write to testresult.txt
 * testresult_len - length of testresult
 * va_touch_thr - touch threshold in va
 * key_touch_thr - touch threshold in key
 * buffer - save detail data temporary
 * ini_ic_name - ic name of ini file
 * ini_keyword_num - keyword line number of ini file
 * ini_data - save ini data as struct _ini_data
 * func - test main function
 * test_item - save whole test item
 * screen_param - test basic parameter: tx/rx/key num...
 */
struct fts_test_data {
    int test_num;                                           // test item num
    char *store_all_data;                                      // store all data in  testdata.csv
    char *tmp_buffer;                                            // store temp buff in testdata.csv
    char *store_msg_area;                                    // store message  in testdata.csv
    int len_store_msg_area;                                      // store message len in testdata.csv
    char *msg_area_line2;                                    // strore second line in testdata.csv
    int len_msg_area_line2;                                      //strore second line len in testdata.csv
    char *store_data_area;                                     // store data  in testdata.csv
    int len_store_data_area;                                     // store data len in testdata.csv
    u8 test_item_code;                        //TestItemCode in testdata.csv
    int start_line;                                               //The Start Line of Data Area
    int test_data_count;                                       // test data count in testdata.csv
    char *testresult;
    int testresult_len;
    u8 va_touch_thr;
    u8 key_touch_thr;
    int *buffer;
    char ini_ic_name[20];
    int ini_keyword_num;
    struct _ini_data *ini_data;
    struct test_funcs *func;
    struct test_item test_item[MAX_TEST_ITEM];
    struct screen_setting screen_param;
    struct detailthreshold_mcap mcap_detail_thr;  // detailthreshold of MCap
    struct detailthreshold_scap scap_detail_thr;   // detailthreshold of SCap
    struct detailthreshold_incell incell_detail_thr;   // detailthreshold of Incell
};

/*-----------------------------------------------------------
 * IC corresponding code, each of the IC code is 16 bit, high 8
 * bit on behalf of the same series, low 8 bit on behalf of the specific IC
 *-----------------------------------------------------------*/
enum ic_type {
    IC_FT5X36  = 0x0000,
    IC_FT5X36i = 0x0001,
    IC_FT3X16  = 0x0002,
    IC_FT3X26  = 0x0003,

    IC_FT5X46  = 0x0100,
    IC_FT5X46I = 0x0101,
    IC_FT5526  = 0x0102,
    IC_FT3X17  = 0x0103,
    IC_FT5436  = 0x0104,
    IC_FT3X27  = 0x0105,
    IC_FT5526I = 0x0106,
    IC_FT5416  = 0x0107,
    IC_FT5426  = 0x0108,
    IC_FT5435  = 0x0109,
    IC_FT7681  = 0x010A,
    IC_FT7661  = 0x010B,
    IC_FT7511  = 0x010C,
    IC_FT7421  = 0x010D,
    IC_FT7311  = 0x010E,
    IC_FT3327DQQ_001  = 0x010F,
    IC_FT5446DQS_W01  = 0x0110,
    IC_FT5446DQS_W02  = 0x0111,
    IC_FT5446DQS_002  = 0x0112,
    IC_FT5446DQS_Q02  = 0x0113,

    IC_FT6X06  = 0x0200,
    IC_FT3X06  = 0x0201,

    IC_FT6X36  = 0x0300,
    IC_FT3X07  = 0x0301,
    IC_FT6416  = 0x0302,
    IC_FT6426  = 0x0303,
    IC_FT7401  = 0x0304,
    IC_FT3407U = 0x0305,
    IC_FT6236U = 0x0306,
    IC_FT6436U = 0x0307,
    IC_FT3267  = 0x0308,
    IC_FT3367  = 0x0309,

    IC_FT5X16  = 0x0400,
    IC_FT5X12  = 0x0401,

    IC_FT5506  = 0x0500,
    IC_FT5606  = 0x0501,
    IC_FT5816  = 0x0502,

    IC_FT5822  = 0x0600,
    IC_FT5626  = 0x0601,
    IC_FT5726  = 0x0602,
    IC_FT5826B = 0x0603,
    IC_FT3617  = 0x0604,
    IC_FT3717  = 0x0605,
    IC_FT7811  = 0x0606,
    IC_FT5826S = 0x0607,
    IC_FT3517U = 0x0608,

    IC_FT5306  = 0x0700,
    IC_FT5406  = 0x0701,

    IC_FT3C47U = 0x0800,

    IC_FT3D47  = 0x0900,

    IC_FT5442  = 0x0A00,

    IC_FT3428U = 0x0B00,

    IC_FT5452  = 0x0C00,
    IC_FT3518  = 0x0C01,
    IC_FT3558  = 0x0C02,

    IC_FT8606  = 0x8000,

    IC_FT8716  = 0x8100,
    IC_FT8716U = 0x8101,
    IC_FT8613  = 0x8102,
    IC_FT8716F = 0x8103,

    IC_FT8607  = 0x8200,
    IC_FT8607U = 0x8201,

    IC_FT8707  = 0x8300,

    IC_FT8736  = 0x8400,

    IC_FTE716  = 0x8500,

    IC_FT8006M = 0x8600,
    IC_FT7250  = 0x8601,

    IC_FTE736  = 0x8700,

    IC_FT8006U = 0x8800,

    IC_FT8201  = 0x8900,

    IC_FT8719  = 0x8A00,
    IC_FT8615  = 0x8A01,

    IC_FT8739  = 0x8B00,
};

enum normalize_type {
    OVERALL_NORMALIZE = 0,
    AUTO_NORMALIZE = 1,
};

enum NodeType {
    NODE_INVALID_TYPE = 0,
    NODE_VALID_TYPE = 1,
    NODE_KEY_TYPE = 2,
    NODE_AST_TYPE = 3,
};

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/
extern struct test_funcs test_func_ft8719;
extern struct test_funcs *test_funcs_list[];
extern struct test_ic_type ic_types[];
extern struct fts_test_data test_data;
/* add tp_test TestResult by zhangchaofan start, 2018-11-30*/
static int32_t TestResult_Maininit = 0;
static int32_t TestResult_Testparams = 0;
static int32_t TestResult_Start = 0;
static int32_t TestResult_Workmode = 0;
static int32_t TestResult_Mainexit = 0;
/* add tp_test TestResult by zhangchaofan end, 2018-11-30*/
int init_test_funcs(u32 ic_type);
u32 fts_ic_table_get_ic_code_from_ic_name(char *strIcName);
void sys_delay(int ms);
int focal_abs(int value);
u8 fts_i2c_read_write(unsigned char *writebuf, int  writelen, unsigned char *readbuf, int readlen);
int fts_test_i2c_read(u8 *writebuf, int writelen, u8 *readbuf, int readlen);
int fts_test_i2c_write(u8 *writebuf, int writelen);
int read_reg(u8 addr, u8 *val);
int write_reg(u8 addr, u8 val);
int enter_work_mode(void);
int enter_factory_mode(void);
int read_mass_data(u8 addr, int byte_num, int *buf);
int chip_clb_incell(void);
int start_scan_incell(void);
int wait_state_update(void);
int get_rawdata_incell(int *data);
int get_cb_incell(u16 saddr, int byte_num, int *cb_buf);
int weakshort_get_adc_data_incell(u8 retval, u8 ch_num, int byte_num, int *adc_buf);
void show_data_incell(int *data, bool include_key);
bool compare_data_incell(int *data, int min, int max, int vk_min, int vk_max, bool include_key);
bool compare_detailthreshold_data_incell(int *data, int *data_min, int *data_max, bool include_key);
void save_testdata_incell(int *data, char *test_num, int index, u8 row, u8 col, u8 item_count);
void fts_set_testitem(unsigned char ucitemcode);
void *fts_malloc(size_t size);
void fts_free_proc(void *p);
int get_channel_num(void);

#define fts_free(p) do {\
    if (p) {\
        fts_free_proc(p);\
        p = NULL;\
    }\
} while(0)


#define FOCAL_TEST_DEBUG_EN     1
#if (FOCAL_TEST_DEBUG_EN)
#define FTS_TEST_DBG(fmt, args...) do {printk("[FTS] [TEST]%s. line: %d.  "fmt"\n",  __FUNCTION__, __LINE__, ##args);} while (0)
#define FTS_TEST_FUNC_ENTER() printk("[FTS][TEST]%s: Enter(%d)\n", __func__, __LINE__)
#define FTS_TEST_FUNC_EXIT()  printk("[FTS][TEST]%s: Exit(%d)\n", __func__, __LINE__)
#else
#define FTS_TEST_DBG(fmt, args...) do{}while(0)
#define FTS_TEST_FUNC_ENTER()
#define FTS_TEST_FUNC_EXIT()
#endif

#define FTS_TEST_INFO(fmt, args...) do { printk(KERN_ERR "[FTS][TEST][Info]%s. line: %d.  "fmt"\n",  __FUNCTION__, __LINE__, ##args);} while (0)
#define FTS_TEST_ERROR(fmt, args...) do { printk(KERN_ERR "[FTS][TEST][Error]%s. line: %d.  "fmt"\n",  __FUNCTION__, __LINE__, ##args);} while (0)

#define FTS_TEST_SAVE_INFO(fmt, args...)  do { \
    if (test_data.testresult) { \
        test_data.testresult_len += snprintf( \
        test_data.testresult + test_data.testresult_len, \
        BUFF_LEN_TESTRESULT_BUFFER, \
        fmt, ##args);\
    }; \
} while (0)

#define FTS_TEST_SAVE_ERR(fmt, args...)  do { \
    if (test_data.testresult) { \
        test_data.testresult_len += snprintf( \
        test_data.testresult + test_data.testresult_len, \
        BUFF_LEN_TESTRESULT_BUFFER, \
        fmt, ##args);\
    } \
    printk(KERN_ERR "[FTS][TEST][Error]%s. line: %d.  "fmt"\n",  __FUNCTION__, __LINE__, ##args);\
} while (0)
#endif
