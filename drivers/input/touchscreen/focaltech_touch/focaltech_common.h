/*
 *
 * FocalTech fts TouchScreen driver.
 *
 * Copyright (c) 2010-2017, Focaltech Ltd. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
/*****************************************************************************
*
* File Name: focaltech_common.h
*
* Author: Focaltech Driver Team
*
* Created: 2016-08-16
*
* Abstract:
*
* Reference:
*
*****************************************************************************/

#ifndef __LINUX_FOCALTECH_COMMON_H__
#define __LINUX_FOCALTECH_COMMON_H__

#include "focaltech_config.h"

/*****************************************************************************
* Macro definitions using #define
*****************************************************************************/
#define FTS_DRIVER_VERSION                  "Focaltech V1.3 20170306"

#define FLAGBIT(x)              (0x00000001 << (x))
#define FLAGBITS(x, y)          ((0xFFFFFFFF >> (32 - (y) - 1)) << (x))

#define FLAG_ICSERIALS_LEN      5
#define FLAG_IDC_BIT            11

#define IC_SERIALS              (FTS_CHIP_TYPE & \
					FLAGBITS(0, FLAG_ICSERIALS_LEN-1))
#define FTS_CHIP_IDC            ((FTS_CHIP_TYPE & FLAGBIT(FLAG_IDC_BIT)) \
						== FLAGBIT(FLAG_IDC_BIT))

#define FTS_CHIP_TYPE_MAPPING   { \
	{0x01, 0x58, 0x22, 0x58, 0x22, 0x00, 0x00, 0x58, 0x2C}, \
	{0x02, 0x54, 0x22, 0x54, 0x22, 0x00, 0x00, 0x54, 0x2C}, \
	{0x03, 0x64, 0x26, 0x64, 0x26, 0x00, 0x00, 0x79, 0x1C}, \
	{0x04, 0x33, 0x67, 0x64, 0x26, 0x00, 0x00, 0x79, 0x1C}, \
	{0x05, 0x87, 0x16, 0x87, 0x16, 0x87, 0xA6, 0x00, 0x00}, \
	{0x06, 0x87, 0x36, 0x87, 0x36, 0x87, 0xC6, 0x00, 0x00}, \
	{0x07, 0x80, 0x06, 0x80, 0x06, 0x80, 0xC6, 0x80, 0xB6}, \
	{0x08, 0x86, 0x06, 0x86, 0x06, 0x86, 0xA6, 0x00, 0x00}, \
	{0x09, 0x86, 0x07, 0x86, 0x07, 0x86, 0xA7, 0x00, 0x00}, \
	{0x0A, 0xE7, 0x16, 0x87, 0x16, 0xE7, 0xA6, 0x87, 0xB6}, \
}

#define I2C_BUFFER_LENGTH_MAXINUM           256
#define FILE_NAME_LENGTH                    128
#define ENABLE                              1
#define DISABLE                             0
/*register address*/
#define FTS_REG_INT_CNT                     0x8F
#define FTS_REG_FLOW_WORK_CNT               0x91
#define FTS_REG_WORKMODE                    0x00
#define FTS_REG_WORKMODE_FACTORY_VALUE      0x40
#define FTS_REG_WORKMODE_WORK_VALUE         0x00
#define FTS_REG_CHIP_ID                     0xA3
#define FTS_REG_CHIP_ID2                    0x9F
#define FTS_REG_POWER_MODE                  0xA5
#define FTS_REG_POWER_MODE_SLEEP_VALUE      0x03
#define FTS_REG_FW_VER                      0xA6
#define FTS_REG_VENDOR_ID                   0xA8
#define FTS_REG_LCD_BUSY_NUM                0xAB
#define FTS_REG_FACE_DEC_MODE_EN            0xB0
#define FTS_REG_GLOVE_MODE_EN               0xC0
#define FTS_REG_COVER_MODE_EN               0xC1
#define FTS_REG_CHARGER_MODE_EN             0x8B
#define FTS_REG_GESTURE_EN                  0xD0
#define FTS_REG_GESTURE_OUTPUT_ADDRESS      0xD3
#define FTS_REG_ESD_SATURATE                0xED



/*****************************************************************************
*  Alternative mode (When something goes wrong,
*  the modules may be able to solve the problem.)
*****************************************************************************/
/*
 * point report check
 * default: disable
 */
#define FTS_POINT_REPORT_CHECK_EN               0


/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/
struct ft_chip_t {
	unsigned long type;
	unsigned char chip_idh;
	unsigned char chip_idl;
	unsigned char rom_idh;
	unsigned char rom_idl;
	unsigned char pramboot_idh;
	unsigned char pramboot_idl;
	unsigned char bootloader_idh;
	unsigned char bootloader_idl;
};

/* i2c communication*/
int fts_i2c_write_reg(struct i2c_client *client, u8 regaddr, u8 regvalue);
int fts_i2c_read_reg(struct i2c_client *client, u8 regaddr, u8 *regvalue);
int fts_i2c_read(struct i2c_client *client, char *writebuf, int writelen,
						char *readbuf, int readlen);
int fts_i2c_write(struct i2c_client *client, char *writebuf, int writelen);
int fts_i2c_init(void);
int fts_i2c_exit(void);

/* Gesture functions */
#if FTS_GESTURE_EN
int fts_gesture_init(struct input_dev *input_dev, struct i2c_client *client);
int fts_gesture_exit(struct i2c_client *client);
void fts_gesture_recovery(struct i2c_client *client);
int fts_gesture_readdata(struct i2c_client *client);
int fts_gesture_suspend(struct i2c_client *i2c_client);
int fts_gesture_resume(struct i2c_client *client);
#endif

/* Apk and functions */
#if FTS_APK_NODE_EN
int fts_create_apk_debug_channel(struct i2c_client *client);
void fts_release_apk_debug_channel(void);
#endif

/* ADB functions */
#if FTS_SYSFS_NODE_EN
int fts_create_sysfs(struct i2c_client *client);
int fts_remove_sysfs(struct i2c_client *client);
#endif

/* ESD */
#if FTS_ESDCHECK_EN
int fts_esdcheck_init(void);
int fts_esdcheck_exit(void);
int fts_esdcheck_switch(bool enable);
int fts_esdcheck_proc_busy(bool proc_debug);
int fts_esdcheck_set_intr(bool intr);
int fts_esdcheck_suspend(void);
int fts_esdcheck_resume(void);
int fts_esdcheck_get_status(void);
#endif

/* Production test */
#if FTS_TEST_EN
int fts_test_init(struct i2c_client *client);
int fts_test_exit(struct i2c_client *client);
#endif

/* Point Report Check*/
#if FTS_POINT_REPORT_CHECK_EN
int fts_point_report_check_init(void);
int fts_point_report_check_exit(void);
void fts_point_report_check_queue_work(void);
#endif

/* Other */
extern int g_show_log;
int fts_reset_proc(int hdelayms);
int fts_wait_tp_to_valid(struct i2c_client *client);
void fts_tp_state_recovery(struct i2c_client *client);
int fts_ex_mode_init(struct i2c_client *client);
int fts_ex_mode_exit(struct i2c_client *client);
int fts_ex_mode_recovery(struct i2c_client *client);

void fts_irq_disable(void);
void fts_irq_enable(void);

/*****************************************************************************
* DEBUG function define here
*****************************************************************************/
#if FTS_DEBUG_EN
#define FTS_DEBUG_LEVEL     1

#if (FTS_DEBUG_LEVEL == 2)
#define FTS_DEBUG(fmt, args...) pr_err("[FTS][%s]"fmt"\n", __func__, ##args)
#else
#define FTS_DEBUG(fmt, args...) pr_err("[FTS]"fmt"\n", ##args)
#endif

#define FTS_FUNC_ENTER() pr_err("[FTS]%s: Enter\n", __func__)
#define FTS_FUNC_EXIT()  pr_err("[FTS]%s: Exit(%d)\n", __func__, __LINE__)
#else
#define FTS_DEBUG(fmt, args...)
#define FTS_FUNC_ENTER()
#define FTS_FUNC_EXIT()
#endif

#define FTS_INFO(fmt, args...) do { \
		if (g_show_log) \
			pr_err("[FTS][Info]"fmt"\n", ##args); \
	}  while (0)

#define FTS_ERROR(fmt, args...)  do { \
		if (g_show_log) \
			pr_err("[FTS][Error]"fmt"\n", ##args); \
	}  while (0)


#endif /* __LINUX_FOCALTECH_COMMON_H__ */
