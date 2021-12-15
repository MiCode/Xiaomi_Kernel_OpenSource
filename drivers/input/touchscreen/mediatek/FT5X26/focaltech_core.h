/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __LINUX_FTXXXX_H__
#define __LINUX_FTXXXX_H__
/***********************************************************************
 *
 * File Name: focaltech_core.h
 *
 *    Author: Xu YongFeng
 *
 *   Created: 2015-01-29
 *
 *  Abstract:
 *
 * Reference:
 *
 **************************************************************************/

/**************************************************************************
 * 1.Included header files
 **************************************************************************/

#include "tpd.h"
/* #include "tpd_custom_fts.h" */
/* #include "cust_gpio_usage.h" */
#include <asm/unistd.h>
#include <linux/bitops.h>
#include <linux/byteorder/generic.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/syscalls.h>
#include <linux/time.h>
#include <linux/vmalloc.h>
#include <uapi/linux/sched/types.h>
/* #include <mach/mt_pm_ldo.h> */
/* #include <mach/mt_typedefs.h> */
/* #include <mach/mt_boot.h> */
/* #include <mach/irqs.h> */
/* #include <cust_eint.h> */
#include <linux/jiffies.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include "include/tpd_ft5x0x_common.h"
#include <../fs/proc/internal.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/mount.h>
#include <linux/netdevice.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/unistd.h>
#include <linux/version.h>
/*************************************************************************
 * Private constant and macro definitions using #define
 **************************************************************************/

/**********************Custom define
 * begin**********************************************/

#if defined(MODULE) || defined(CONFIG_HOTPLUG)
#define __devexit_p(x) x
#else
#define __devexit_p(x) NULL
#endif

#define TPD_POWER_SOURCE_CUSTOM MT6323_POWER_LDO_VGP1
#define IIC_PORT                                                               \
	0
/* MT6572: 1  MT6589:0 , Based on the I2C index you choose for TPM */
#define TPD_HAVE_BUTTON       /*if have virtual key,need define the MACRO*/
#define TPD_BUTTON_HEIGH (40) /* 100 */
#define TPD_KEY_COUNT 3       /* 4 */
#define TPD_KEYS                                                               \
	{                                                                      \
		KEY_MENU, KEY_HOMEPAGE, KEY_BACK                               \
	}
#define TPD_KEYS_DIM                                                           \
	{                                                                      \
		{80, 900, 20, TPD_BUTTON_HEIGH},                               \
			{240, 900, 20, TPD_BUTTON_HEIGH},                      \
		{                                                              \
			400, 900, 20, TPD_BUTTON_HEIGH                         \
		}                                                              \
	}
#define FT_ESD_PROTECT 0
/*********************Custom Define
 * end*************************************************/
#define MT_PROTOCOL_B
#define A_TYPE 0
#define TPD_TYPE_CAPACITIVE
#define TPD_TYPE_RESISTIVE
#define TPD_POWER_SOURCE
#define TPD_NAME "FTS"
#define TPD_I2C_NUMBER 0
#define TPD_WAKEUP_TRIAL 60
#define TPD_WAKEUP_DELAY 100
#define TPD_VELOCITY_CUSTOM_X 15
#define TPD_VELOCITY_CUSTOM_Y 20

#define CFG_MAX_TOUCH_POINTS 5
#define MT_MAX_TOUCH_POINTS 10
#define FTS_MAX_ID 0x0F
#define FTS_TOUCH_STEP 6
#define FTS_FACE_DETECT_POS 1
#define FTS_TOUCH_X_H_POS 3
#define FTS_TOUCH_X_L_POS 4
#define FTS_TOUCH_Y_H_POS 5
#define FTS_TOUCH_Y_L_POS 6
#define FTS_TOUCH_EVENT_POS 3
#define FTS_TOUCH_ID_POS 5
#define FT_TOUCH_POINT_NUM 2
#define FTS_TOUCH_XY_POS 7
#define FTS_TOUCH_MISC 8
#define POINT_READ_BUF (3 + FTS_TOUCH_STEP * CFG_MAX_TOUCH_POINTS)
#define FT_FW_NAME_MAX_LEN 50
#define TPD_DELAY (2 * HZ / 100)
/* #define TPD_RES_X					1080//480 */
/* #define TPD_RES_Y					1280//800 */
#define TPD_CALIBRATION_MATRIX                                                 \
	{                                                                      \
		962, 0, 0, 0, 1600, 0, 0, 0                                    \
	}
#define FT_PROXIMITY_ENABLE 0
/* #define TPD_AUTO_UPGRADE */
/* #define TPD_HAVE_CALIBRATION */
/* #define TPD_HAVE_TREMBLE_ELIMINATION */
/* #define TPD_CLOSE_POWER_IN_SLEEP */
/******************************************************************************/
/* Chip Device Type */
#define IC_FT5X06 0  /* x=2,3,4 */
#define IC_FT5606 1  /* ft5506/FT5606/FT5816 */
#define IC_FT5316 2  /* ft5x16 */
#define IC_FT6208 3  /* ft6208 */
#define IC_FT6x06 4  /* ft6206/FT6306 */
#define IC_FT5x06i 5 /* ft5306i */
#define IC_FT5x36 6  /* ft5336/ft5436/FT5436i */

/*register address*/
#define FTS_REG_CHIP_ID 0xA3    /* chip ID */
#define FTS_REG_FW_VER 0xA6     /* FW  version */
#define FTS_REG_VENDOR_ID 0xA8  /* TP vendor ID */
#define FTS_REG_POINT_RATE 0x88 /* report rate */
#define TPD_MAX_POINTS_2 2
#define TPD_MAX_POINTS_5 5
#define TPD_MAX_POINTS_10 10
#define AUTO_CLB_NEED 1
#define AUTO_CLB_NONEED 0
#define LEN_FLASH_ECC_MAX 0xFFFE
/* #define FTS_PACKET_LENGTH				120 */
#define FTS_GESTRUE_POINTS 255
#define FTS_GESTRUE_POINTS_ONETIME 62
#define FTS_GESTRUE_POINTS_HEADER 8
#define FTS_GESTURE_OUTPUT_ADDRESS 0xD3
#define FTS_GESTURE_OUTPUT_UNIT_LENGTH 4

/*
 *#define KEY_GESTURE_U					KEY_U
 *#define KEY_GESTURE_UP						KEY_UP
 *#define KEY_GESTURE_DOWN					KEY_DOWN
 *#define KEY_GESTURE_LEFT					KEY_LEFT
 *#define KEY_GESTURE_RIGHT					KEY_RIGHT
 *#define KEY_GESTURE_O						KEY_O
 *#define KEY_GESTURE_E						KEY_E
 *#define KEY_GESTURE_M						KEY_M
 *#define KEY_GESTURE_L						KEY_L
 *#define KEY_GESTURE_W						KEY_W
 *#define KEY_GESTURE_S						KEY_S
 *#define KEY_GESTURE_V						KEY_V
 *#define KEY_GESTURE_Z						KEY_Z
 */
#define GESTURE_LEFT 0x20
#define GESTURE_RIGHT 0x21
#define GESTURE_UP 0x22
#define GESTURE_DOWN 0x23
#define GESTURE_DOUBLECLICK 0x24
#define GESTURE_O 0x30
#define GESTURE_W 0x31
#define GESTURE_M 0x32
#define GESTURE_E 0x33
#define GESTURE_L 0x44
#define GESTURE_S 0x46
#define GESTURE_V 0x54
#define GESTURE_Z 0x41
/**************************************************************************
 * Private enumerations, structures and unions using typedef
 *************************************************************************/
/* IC info */
struct fts_Upgrade_Info {
	u8 CHIP_ID;
	u8 TPD_MAX_POINTS;
	u8 AUTO_CLB;
	u16 delay_aa;	  /* delay of write FT_UPGRADE_AA */
	u16 delay_55;	  /* delay of write FT_UPGRADE_55 */
	u8 upgrade_id_1;       /* upgrade id 1 */
	u8 upgrade_id_2;       /* upgrade id 2 */
	u16 delay_readid;      /* delay of read id */
	u16 delay_erase_flash; /* delay of earse flash */
};

/*touch event info*/
struct ts_event {
	u16 au16_x[CFG_MAX_TOUCH_POINTS]; /* x coordinate */
	u16 au16_y[CFG_MAX_TOUCH_POINTS]; /* y coordinate */

	u8
		au8_touch_event[CFG_MAX_TOUCH_POINTS];
	/* touch event: 0 -- down; 1-- up; 2 -- */
				/* contact */
	u8 au8_finger_id[CFG_MAX_TOUCH_POINTS];	/* touch ID */
	u16 pressure[CFG_MAX_TOUCH_POINTS];
	u16 area[CFG_MAX_TOUCH_POINTS];
	u8 touch_point;
	int touchs;
	u8 touch_point_num;
};
struct fts_ts_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct ts_event event;
	const struct ftxxxx_ts_platform_data *pdata;
	struct work_struct touch_event_work;
	struct workqueue_struct *ts_workqueue;
	struct regulator *vdd;
	struct regulator *vcc_i2c;
	char fw_name[FT_FW_NAME_MAX_LEN];
	bool loading_fw;
	u8 family_id;
	struct dentry *dir;
	u16 addr;
	bool suspended;
	char *ts_info;
	u8 *tch_data;
	u32 tch_data_len;
	u8 fw_ver[3];
	u8 fw_vendor_id;
#if defined(CONFIG_FB)
	struct notifier_block fb_notif;
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif
};
/***********************************************************************
 * Static variables
 ***********************************************************************/

/***********************************************************************
 * Global variable or extern global variabls/functions
 ***********************************************************************/
/* Function Switches: define to open,  comment to close */
#define FTS_GESTRUE_EN 0
#define MTK_EN 1
#define FTS_APK_DEBUG
#define SYSFS_DEBUG
#define TPD_AUTO_UPGRADE 1
#define MTK_CTP_NODE 1
#define USB_CHARGE_DETECT 0

#if TPD_AUTO_UPGRADE
#define Boot_Upgrade_Protect
/* 开机升级保护。升级失败后，再次开机可重新升级 */
#define FTS_CHIP_ID 0x58     /* FT3427、FT5x46 CHIP ID = 0x54 */
#define FTS_Vendor_1_ID 0x51 /* Ofilm TP VID = 0x51 */
#define FTS_Vendor_2_ID 0x01 /* Ofilm TP new VID */
#endif
#define FIRMWARE_VERTION_NODE
#define FT_TP 0
/* #define CONFIG_TOUCHPANEL_PROXIMITY_SENSOR */
/* #if FT_ESD_PROTECT */
/* extern int apk_debug_flag; */
/* #endif */

extern struct i2c_client *fts_i2c_client;
extern struct input_dev *fts_input_dev;
extern struct tpd_device *tpd;

extern struct fts_Upgrade_Info fts_updateinfo_curr;
int fts_rw_iic_drv_init(struct i2c_client *client);
void fts_rw_iic_drv_exit(void);
void fts_get_upgrade_array(void);
#if FTS_GESTRUE_EN
extern int fts_Gesture_init(struct input_dev *input_dev);
extern int fts_read_Gestruedata(void);
#endif
extern int fts_write_reg(struct i2c_client *client, u8 regaddr, u8 regvalue);
extern int fts_read_reg(struct i2c_client *client, u8 regaddr, u8 *regvalue);
extern int fts_i2c_read(struct i2c_client *client, char *writebuf, int writelen,
			char *readbuf, int readlen);
extern int fts_i2c_write(struct i2c_client *client, char *writebuf,
			 int writelen);
extern int hidi2c_to_stdi2c(struct i2c_client *client);
extern int fts_ctpm_fw_upgrade_with_app_file(struct i2c_client *client,
					     char *firmware_name);
extern int fts_ctpm_auto_clb(struct i2c_client *client);
extern int fts_ctpm_fw_upgrade_with_i_file(struct i2c_client *client);
extern int taiguan_fw_upgrade_with_i_file(
	struct i2c_client *client); /* Neostra huangxiaohui add   20160726 */
extern int fts_ctpm_get_i_file_ver(void);
extern int fts_remove_sysfs(struct i2c_client *client);
extern void fts_release_apk_debug_channel(void);
extern int fts_ctpm_auto_upgrade(struct i2c_client *client);
#if FT_ESD_PROTECT
extern void esd_switch(s32 on);
#endif
extern void fts_reset_tp(int HighOrLow);
extern int fts_create_sysfs(struct i2c_client *client);
/* Apk and ADB functions */
extern int fts_create_apk_debug_channel(struct i2c_client *client);
extern u32 get_devinfo_with_index(u32 index);
extern void fts_get_upgrade_array(void);

#define AC_CHARGE_DETECT 0
#if AC_CHARGE_DETECT
extern bool upmu_is_chr_det(void);
#endif
/***********************************************************************
 * Static function prototypes
 ***********************************************************************/
int fts_6x36_ctpm_fw_upgrade(struct i2c_client *client, u8 *pbt_buf,
			     u32 dw_length);
int fts_6336GU_ctpm_fw_upgrade(struct i2c_client *client, u8 *pbt_buf,
			       u32 dw_length);
int fts_6x06_ctpm_fw_upgrade(struct i2c_client *client, u8 *pbt_buf,
			     u32 dw_length);
int fts_5x36_ctpm_fw_upgrade(struct i2c_client *client, u8 *pbt_buf,
			     u32 dw_length);
int fts_5x06_ctpm_fw_upgrade(struct i2c_client *client, u8 *pbt_buf,
			     u32 dw_length);
int fts_5x46_ctpm_fw_upgrade(struct i2c_client *client, u8 *pbt_buf,
			     u32 dw_length);
int fts_5822_ctpm_fw_upgrade(struct i2c_client *client, u8 *pbt_buf,
			     u32 dw_length);
int fts_5x26_ctpm_fw_upgrade(struct i2c_client *client, u8 *pbt_buf,
			     u32 dw_length);
int fts_8606_ctpm_fw_upgrade(struct i2c_client *client, u8 *pbt_buf,
			     u32 dw_length);
int fts_8606_writepram(struct i2c_client *client, u8 *pbt_buf, u32 dw_length);
int fts_8716_ctpm_fw_upgrade(struct i2c_client *client, u8 *pbt_buf,
			     u32 dw_length);
int fts_8716_writepram(struct i2c_client *client, u8 *pbt_buf, u32 dw_length);
int fts_3x07_ctpm_fw_upgrade(struct i2c_client *client, u8 *pbt_buf,
			     u32 dw_length);
int hidi2c_to_stdi2c(struct i2c_client *client);

#if USB_CHARGE_DETECT
extern int FG_charging_status;
#endif
/* #if FT_ESD_PROTECT */
extern int apk_debug_flag;
/* #endif */
/***********************************************************************
 * Static function prototypes
 ***********************************************************************/
#define FTS_DBG_FEATURE
#ifdef FTS_DBG_FEATURE
#define FTS_DBG(fmt, args...)                                                  \
	pr_debug("[FTS] <-dbg-> [%04d] [@%s]" fmt, __LINE__, __func__, ##args)
#else
#define FTS_DBG(fmt, args...)                                                  \
	do {                                                                   \
	} while (0)
#endif
#define FTS_ERR(fmt, args...)                                                  \
	pr_notice("[FTS] <-err->[%04d] [@%s]" fmt, __LINE__, __func__, ##args)
#endif
