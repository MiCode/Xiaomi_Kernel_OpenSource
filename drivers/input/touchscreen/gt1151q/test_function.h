/* drivers/input/touchscreen/test_function.h
 *
 * 2010 - 2014 Shenzhen Huiding Technology Co.,Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be a reference
 * to you, when you are integrating the GOODiX's CTP IC into your system,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * Version: 1.0
 * Release Date: 2014/11/14
 *
 */

#ifndef _TEST_FUNCTION_HEADER_
#define _TEST_FUNCTION_HEADER_

#ifdef __cplusplus

#endif

struct system_variable {
	unsigned char chip_type;
	const char    *chip_name;
	unsigned char sensor_num;
	unsigned char driver_num;
	unsigned char sc_driver_num;
	unsigned char sc_sensor_num;
	unsigned char max_sensor_num;
	unsigned char max_driver_num;
	unsigned char short_head;
	unsigned char key_number;
	unsigned short reg_rawdata_base;
	unsigned short config_length;
	unsigned short key_offest;
};

typedef struct {
	unsigned char master;
	unsigned char position;
	unsigned char slave;
	unsigned short short_code;
} strShortRecord;


#define _NEED_CHECK                     0x00
#define _NEED_NOT_CHECK                 0x01

#define _MAX_CHECK                      0x0001
#define _MIN_CHECK                      0x0002
#define _ACCORD_CHECK                   0x0004
#define _OFFSET_CHECK                   0x0008
#define _JITTER_CHECK                   0x0010
#define _SPECIAL_CHECK                  0x0020
#define _SHORT_CHECK                    0x0040
#define _KEY_MAX_CHECK                  0x0100
#define _KEY_MIN_CHECK                  0x0200

#define _IC_RESET_CHECK                 0x00010000
#define _VER_EQU_CHECK                  0x00020000
#define _VER_GREATER_CHECK              0x00040000
#define _VER_BETWEEN_CHECK              0x00080000
#define _MAX_CURRENT_CHECK              0x00100000
#define _MODULE_TYPE_CHECK              0x00200000
#define _MODULE_SHORT_CHECK             0x00400000
#define _TEST_FIX_CFG                   0x00800000
#define _LINE_CHECK                     0x01000000
#define _KEY_CHECK                      0x02000000
#define _TEST_CLR_FIX_CFG               0x04000000

#define _UNIFORMITY_CHECK               0x08000000

#define _SPECIAL_LIMIT_CHANNEL_NUM      0x03
#define _FAST_TEST_MODE                 0x1000
#define _TEST_RESULT_SAVE               0x2000
#define _CHANNEL_TX_FLAG                0x80
#define _GT9_SHORT_HEAD                 3
#define _GT9_CHK_SHORT_TOTAL            200
#define _GT9_UPLOAD_SHORT_TOTAL         15
#define _GT9_SHORT_TEST_ERR             88
#define MAX_BUFFER_SIZE                 4096
#define PACKAGE_SIZE                    256




#define MAX_KEY_RAWDATA                 42
#define GTP_TEST_PARAMS_FROM_INI        1
#define GTP_SAVE_TEST_DATA              1
#define FLOAT_AMPLIFIER                 1000
/*******************************************************************/



#define HEX(a) ((a >= '0' && a <= '9') || (a >= 'A' && a <= 'F') || (a >= 'a' && a <= 'f'))

/*----------------------------------error_type--------------------------*/

#define INI_FILE_OPEN_ERR       ((0x1<<0)|0x80000000)

#define INI_FILE_READ_ERR       ((0x2<<0)|0x80000000)
#define INI_FILE_ILLEGAL        ((0x4<<0)|0x80000000)


#define SHORT_TEST_ERROR        ((0x8<<0)|0x80000000)
#define CFG_FILE_OPEN_ERR       ((0x1<<4)|0x80000000)
#define CFG_FILE_READ_ERR       ((0x2<<4)|0x80000000)
#define CFG_FILE_ILLEGAL        ((0x4<<4)|0x80000000)
#define UPDATE_FILE_OPEN_ERR    ((0x1<<8)|0x80000000)
#define UPDATE_FILE_READ_ERR    ((0x2<<8)|0x80000000)
#define UPDATE_FILE_ILLEGAL     ((0x4<<8)|0x80000000)
#define FILE_OPEN_CREATE_ERR    ((0x1<<12)|0x80000000)
#define FILE_READ_WRITE_ERR     ((0x2<<12)|0x80000000)
#define FILE_ILLEGAL            ((0x4<<12)|0x80000000)
#define NODE_OPEN_ERR           ((0x1<<16)|0x80000000)
#define NODE_READ_ERR           ((0x2<<16)|0x80000000)
#define NODE_WRITE_ERR          ((0x4<<16)|0x80000000)
#define DRIVER_NUM_WRONG        ((0x1<<20)|0x80000000)
#define SENSOR_NUM_WRONG        ((0x2<<20)|0x80000000)
#define PARAMETERS_ILLEGL       ((0x4<<20)|0x80000000)
#define I2C_UNLOCKED            ((0x8<<20)|0x80000000)
#define MEMORY_ERR              ((0x1<<24)|0x80000000)
#define I2C_TRANS_ERR           ((0x2<<24)|0x80000000)
#define COMFIRM_ERR             ((0x4<<24)|0x80000000)
#define ENTER_UPDATE_MODE_ERR   ((0x8<<24)|0x80000000)
#define VENDOR_ID_ILLEGAL       ((0x1<<28)|0x80000000)
#define STORE_ERROR             ((0x2<<28)|0x80000000)
#define NO_SUCH_CHIP_TYPE       ((0x4<<28)|0x80000000)

#define FILE_NOT_EXIST          ((0x8<<28)|0x80000000)

#define _CHANNEL_PASS               0x0000
#define _BEYOND_MAX_LIMIT           0x0001
#define _BEYOND_MIN_LIMIT           0x0002
#define _BEYOND_ACCORD_LIMIT        0x0004
#define _BEYOND_OFFSET_LIMIT        0x0008
#define _BEYOND_JITTER_LIMIT        0x0010
#define _SENSOR_SHORT               0x0020
#define _DRIVER_SHORT               0x0040
#define _COF_FAIL                   0x0080
#define _TEST_PASS                  0x800000
#define _I2C_ERR                    0x0100
#define _CONFIG_MSG_WRITE_ERR       0x0200
#define _GET_RAWDATA_TIMEOUT        0x0400
#define _GET_TEST_RESULT_TIMEOUT    0x0800
#define _KEY_BEYOND_MAX_LIMIT       0x1000
#define _KEY_BEYOND_MIN_LIMIT       0x2000
#define _INT_ERROR                  0x4000
#define _TEST_NG                    0x00008000
#define _VERSION_ERR                0x00010000
#define _RESET_ERR                  0x00020000
#define _CURRENT_BEYOND_MAX_LIMIT   0x00040000
#define _MODULE_TYPE_ERR            0x00080000
#define _MST_ERR                    0x00100000
#define _NVRAM_ERR                  0x00200000
#define _GT_SHORT                   0x00400000
#define _BEYOND_UNIFORMITY_LIMIT    0x00800000
#define _BETWEEN_ACCORD_AND_LINE    0x40000000

/*------------------------------------ SHORT TEST PART--------------------------------------*/
#define _bRW_MISCTL__SRAM_BANK          0x4048
#define _bRW_MISCTL__MEM_CD_EN          0x4049
#define _bRW_MISCTL__CACHE_EN           0x404b
#define _bRW_MISCTL__TMR0_EN            0x40b0
#define _rRW_MISCTL__SWRST_B0_          0x4180
#define _bWO_MISCTL__CPU_SWRST_PULSE    0x4184
#define _rRW_MISCTL__BOOTCTL_B0_        0x4190
#define _rRW_MISCTL__BOOT_OPT_B0_       0x4218
#define _bRW_MISCTL__RG_OSC_CALIB       0x4268
#define _rRW_MISCTL__BOOT_CTL_          0x5094
#define _rRW_MISCTL__SHORT_BOOT_FLAG    0x5095

/*-------------------------------test_externs----------------------------------*/
extern int ini_read_int(char *file, const char *key);
extern long ini_read_float(char *file, const char *key);
#if 0
extern int ini_read_text(char *file, const char *key, unsigned char *text);
#endif
extern int ini_read_cfg(char *file, const char *key, unsigned char *text);
extern int ini_read_hex(char *file, const char *key);
extern int getrid_space(char *data, int len);
extern int ini_read(char *file, const char *key, char *value);
extern int auto_find_ini(char *findDir, char *format, char *inipath);
extern int save_short_data(u8 *short_data, strShortRecord r_data, u16 short_code);
extern int open_short_test(unsigned char *short_result);
/*-----------------------------------------------------------------------------*/
#ifdef __cplusplus
extern int (*read_sensorid) (void);
extern int (*read_config) (unsigned char *config, int len);
extern int (*send_config) (unsigned char *config, int len);
extern int (*check_version) (unsigned char *ini_version);
extern int (*read_raw_data) (unsigned short *data, int len);
extern int (*be_normal) (void);
extern int (*disable_hopping) (unsigned char *config, int len, int flag);
extern int (*enter_update_mode_noreset) (void);
extern int (*dsp_fw_startup) (unsigned char dsp_flag);
extern void (*test_to_show) (unsigned char *test_result, unsigned short upload_cnt);
extern void (*write_test_params) (unsigned char *config);
extern int (*short_test_analysis) (unsigned char *result, int t_size);
extern int (*load_dsp_code_check) (void);
extern int (*short_test_end) (void);

#else
extern int (read_sensorid) (void);
extern int (read_config) (unsigned char *config, int len);
extern int (send_config) (unsigned char *config, int len);
extern int (check_version) (unsigned char *ini_version);
extern int (read_raw_data) (unsigned short *data, int len);
extern int (be_normal) (void);
extern int (disable_hopping) (unsigned char *config, int len, int flag);
extern int (enter_update_mode_noreset) (void);
extern int (dsp_fw_startup) (unsigned char dsp_flag);
extern void (test_to_show) (unsigned char *test_result, unsigned short upload_cnt);
extern void (write_test_params) (unsigned char *config);
extern int (short_test_analysis) (unsigned char *result, int t_size);
extern int (load_dsp_code_check) (void);
extern int (short_test_end) (void);

#endif

extern int i2c_read_data(unsigned short addr, unsigned char *data, int length);
extern int i2c_write_data(unsigned short addr, unsigned char *data, int length);
extern int enable_irq_esd(void);
extern int disable_irq_esd(void);
extern int reset_guitar(void);
extern int enter_update_mode(void);
extern int parse_config(unsigned char *config);
extern struct system_variable sys;
extern unsigned short gt900_short_threshold;
extern unsigned short gt900_drv_drv_resistor_threshold;
extern unsigned short gt900_drv_sen_resistor_threshold;
extern unsigned short gt900_sen_sen_resistor_threshold;
extern unsigned short gt900_resistor_warn_threshold;
extern unsigned short gt900_drv_gnd_resistor_threshold;
extern unsigned short gt900_sen_gnd_resistor_threshold;
extern unsigned char gt900_short_test_times;
extern int sys_avdd;

#define GT1X_DRIVER_TEST
#ifdef GT1X_DRIVER_TEST
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/timex.h>
#include <linux/rtc.h>
#define DEBUG(fmt, arg...) printk("<<GTP-TEST-DBG>>[%s:%d] "fmt"\n", __func__, __LINE__, ##arg)
#define WARNING(fmt, arg...) printk("<<GTP-TEST-WAR>>[%s:%d] "fmt"\n", __func__, __LINE__, ##arg)
#define DEBUG_ARRAY(array, num)
#define DEBUG_DATA(a, b)
#define malloc(len) kmalloc(len, GFP_KERNEL)
#define free(p) kfree(p)
#define usleep(ms) msleep(ms/1000)
#define FORMAT_PATH(path,mdir,name) do{\
                               struct timex txc;\
                               struct rtc_time tm;\
                               do_gettimeofday(&(txc.time));\
                               rtc_time_to_tm(txc.time.tv_sec,&tm);\
                               sprintf((char*)path,"%s%s_%04d%02d%02d%02d%02d%02d.csv",mdir,name,(tm.tm_year+1900),(tm.tm_mon + 1),tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec);\
                              }while(0)

typedef enum {
	_GT1143,
	_GT1151,
	_GT1152,
	_GT9P,
} CHIP_TYPE;
typedef struct file FILE;
FILE *fopen(const char *path, const char *mode);
size_t fread(void *buffer, size_t size, size_t count, FILE *filp);
size_t fwrite(const void *buffer, size_t size, size_t count, FILE *filp);
int fclose(FILE *filp);
char *strtok(char *s, const char *delim);
int atoi(const char *str);
long atof(char const *s);

#else
#include <time.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <sys/timeb.h>
#define FORMAT_PATH(path,mdir,name) do{\
                      struct timeb tp;\
                      struct tm *tm;\
                      ftime(&tp);\
                      tm = localtime(&(tp.time));\
                      sprintf((char*)path,"%s%s_%04d%02d%02d%02d%02d%02d.csv",mdir,name,(1990 + tm->tm_year),1+tm->tm_mon,tm->tm_mday,tm->tm_hour,tm->tm_min,tm->tm_sec);\
                     }while(0)

#endif

#ifdef __cplusplus

#endif

#endif
