/***************************************************************************
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
 *    File	: lgtp_device_s3320.h
 *    Author(s)   : D3 BSP Touch Team < d3-bsp-touch@lge.com >
 *    Description :
 *
 ***************************************************************************/

#if !defined(_LGTP_DEVICE_FT8707_H_)
#define _LGTP_DEVICE_FT8707_H_

/****************************************************************************
* Nested Include Files
****************************************************************************/
#include <lgtp_common.h>
#include <linux/syscalls.h>


/****************************************************************************
* Mainfest Constants / Defines
****************************************************************************/
#define LPWG_DEBUG_ENABLE					0
#define DELAY_ENABLE						0
#define KNOCKON_DELAY					700

#define MAX_NUM_OF_FINGERS					10
/*Number of channel*/
#if defined(TOUCH_MODEL_PH1)
#define MAX_ROW                                                         32
#define MAX_COL                                                         18
#else
#define MAX_ROW								26
#define MAX_COL								15
#endif
#define MIT_ROW_NUM				0x0B
#define MIT_COL_NUM				0x0C

#define SECTION_NUM     3
#define PAGE_HEADER     3
#define PAGE_DATA       1024
#define PAGE_CRC        2
#define PACKET_SIZE     (PAGE_HEADER + PAGE_DATA + PAGE_CRC)


/*MIP4_REG*/
/* Address */
#define MIP_R0_INFO						0x01
#define MIP_R1_INFO_PRODUCT_NAME			0x00
#define MIP_R1_INFO_RESOLUTION_X			0x10
#define MIP_R1_INFO_RESOLUTION_Y			0x12
#define MIP_R1_INFO_NODE_NUM_X			0x14
#define MIP_R1_INFO_NODE_NUM_Y			0x15
#define MIP_R1_INFO_KEY_NUM				0x16
#define MIP_R1_INFO_VERSION_BOOT			0x20
#define MIP_R1_INFO_VERSION_CORE			0x22
#define MIP_R1_INFO_VERSION_CUSTOM		0x24
#define MIP_R1_INFO_VERSION_PARAM		0x26
#define MIP_R1_INFO_SECT_BOOT_START		0x30
#define MIP_R1_INFO_SECT_BOOT_END		0x31
#define MIP_R1_INFO_SECT_CORE_START		0x32
#define MIP_R1_INFO_SECT_CORE_END		0x33
#define MIP_R1_INFO_SECT_CUSTOM_START	0x34
#define MIP_R1_INFO_SECT_CUSTOM_END		0x35
#define MIP_R1_INFO_SECT_PARAM_START	0x36
#define MIP_R1_INFO_SECT_PARAM_END		0x37
#define MIP_R1_INFO_BUILD_DATE			0x40
#define MIP_R1_INFO_BUILD_TIME			0x44
#define MIP_R1_INFO_CHECKSUM_PRECALC	0x48
#define MIP_R1_INFO_CHECKSUM_REALTIME	0x4A
#define MIP_R1_INFO_CHECKSUM_CALC		0x4C
#define MIP_R1_INFO_PROTOCOL_NAME		0x50
#define MIP_R1_INFO_PROTOCOL_VERSION	0x58
#define MIP_R1_INFO_IC_ID					0x70
#define MIP_R1_INFO_IC_NAME					0x71

#define MIP_R0_EVENT						0x02
#define MIP_R1_EVENT_SUPPORTED_FUNC		0x00
#define MIP_R1_EVENT_FORMAT				0x04
#define MIP_R1_EVENT_SIZE					0x06
#define MIP_R1_EVENT_PACKET_INFO			0x10
#define MIP_R1_EVENT_PACKET_DATA			0x11

#define MIP_R0_CTRL						0x06
#define MIP_R1_CTRL_READY_STATUS			0x00
#define MIP_R1_CTRL_EVENT_READY			0x01
#define MIP_R1_CTRL_MODE					0x10
#define MIP_R1_CTRL_EVENT_TRIGGER_TYPE	0x11
#define MIP_R1_CTRL_RECALIBRATE			0x12
#define MIP_R1_CTRL_POWER_STATE			0x13
#define MIP_R1_CTRL_GESTURE_TYPE			0x14
#define MIP_R1_CTRL_DISABLE_ESD_ALERT	0x18
#define MIP_R1_CTRL_CHARGER_MODE			0x19
#define MIP_R1_CTRL_GLOVE_MODE			0x1A
#define MIP_R1_CTRL_WINDOW_MODE			0x1B
#define MIP_R1_CTRL_PALM_REJECTION		0x1C
#define MIP_R1_CTRL_EDGE_EXPAND			0x1D
#define MIP_R1_CTRL_LPWG_DEBUG_ENABLE	0x1F

#define MIP_R0_PARAM						0x08
#define MIP_R1_PARAM_BUFFER_ADDR			0x00
#define MIP_R1_PARAM_PROTOCOL			0x04
#define MIP_R1_PARAM_MODE					0x10

#define MIP_R0_TEST						0x0A
#define MIP_R1_TEST_BUF_ADDR				0x00
#define MIP_R1_TEST_PROTOCOL				0x02
#define MIP_R1_TEST_TYPE					0x10
#define MIP_R1_TEST_DATA_FORMAT			0x20
#define MIP_R1_TEST_ROW_NUM				0x20
#define MIP_R1_TEST_COL_NUM				0x21
#define MIP_R1_TEST_BUFFER_COL_NUM		0x22
#define MIP_R1_TEST_COL_AXIS				0x23
#define MIP_R1_TEST_KEY_NUM				0x24
#define MIP_R1_TEST_DATA_TYPE			0x25

#define MIP_R0_IMAGE						0x0C
#define MIP_R1_IMAGE_BUF_ADDR			0x00
#define MIP_R1_IMAGE_PROTOCOL_ID			0x04
#define MIP_R1_IMAGE_TYPE					0x10
#define MIP_R1_IMAGE_DATA_FORMAT			0x20
#define MIP_R1_IMAGE_ROW_NUM				0x20
#define MIP_R1_IMAGE_COL_NUM				0x21
#define MIP_R1_IMAGE_BUFFER_COL_NUM		0x22
#define MIP_R1_IMAGE_COL_AXIS			0x23
#define MIP_R1_IMAGE_KEY_NUM				0x24
#define MIP_R1_IMAGE_DATA_TYPE			0x25
#define MIP_R1_IMAGE_FINGER_NUM			0x30
#define MIP_R1_IMAGE_FINGER_AREA			0x31

#define MIP_R0_LPWG						0x0E
#define MIP_R1_VENDOR_INFO				0x00

#define MIP_R0_LOG							0x10
#define MIP_R1_LOG_TRIGGER				0x14

/* Value */
#define MIP_EVENT_INPUT_PRESS			0x80
#define MIP_EVENT_INPUT_SCREEN			0x40
#define MIP_EVENT_INPUT_HOVER			0x20
#define MIP_EVENT_INPUT_PALM				0x10
#define MIP_EVENT_INPUT_ID				0x0F

#define MIP_EVENT_GESTURE_C				1
#define MIP_EVENT_GESTURE_W				2
#define MIP_EVENT_GESTURE_V				3
#define MIP_EVENT_GESTURE_M				4
#define MIP_EVENT_GESTURE_S				5
#define MIP_EVENT_GESTURE_Z				6
#define MIP_EVENT_GESTURE_O				7
#define MIP_EVENT_GESTURE_E				8
#define MIP_EVENT_GESTURE_V_90			9
#define MIP_EVENT_GESTURE_V_180			10
#define MIP_EVENT_GESTURE_FLICK_RIGHT	20
#define MIP_EVENT_GESTURE_FLICK_DOWN	21
#define MIP_EVENT_GESTURE_FLICK_LEFT	22
#define MIP_EVENT_GESTURE_FLICK_UP		23
#define MIP_EVENT_GESTURE_DOUBLE_TAP	24
#define MIP_EVENT_GESTURE_MULTI_TAP		25
#define MIP_EVENT_GESTURE_ALL			0xFFFFFFFF

#define MIP_ALERT_ESD					1
#define MIP_ALERT_WAKEUP				2
#define MIP_ALERT_INPUTTYPE			3
#define MIP_ALERT_F1				0xF1
#define MIP_LPWG_EVENT_TYPE_FAIL		1

#define MIP_CTRL_STATUS_NONE			0x05
#define MIP_CTRL_STATUS_READY		0xA0
#define MIP_CTRL_STATUS_LOG			0x77

#define MIP_CTRL_MODE_NORMAL			0
#define MIP_CTRL_MODE_PARAM			1
#define MIP_CTRL_MODE_TEST_CM		2

#define MIP_CTRL_POWER_ACTIVE		0
#define MIP_CTRL_POWER_LOW			1

#define MIP_TEST_TYPE_NONE			0
#define MIP_TEST_TYPE_CM_DELTA		1
#define MIP_TEST_TYPE_CM_ABS			2
#define MIP_TEST_TYPE_CM_JITTER		3
#define MIP_TEST_TYPE_SHORT			4
#define MIP_TEST_TYPE_INTR_H				5
#define MIP_TEST_TYPE_INTR_L				6
#define MIP_TEST_TYPE_SHORT2				7

#define MIP_IMG_TYPE_NONE				0
#define MIP_IMG_TYPE_INTENSITY		1
#define MIP_IMG_TYPE_RAWDATA			2
#define MIP_IMG_TYPE_WAIT				255

#define MIP_TRIGGER_TYPE_NONE		0
#define MIP_TRIGGER_TYPE_INTR		1
#define MIP_TRIGGER_TYPE_REG			2

#define MIP_LOG_MODE_NONE				0
#define MIP_LOG_MODE_TRIG				1

/* LPWG Register map */
/* Control */
#define MIP_R1_LPWG_START					0x10
#define MIP_R1_LPWG_ENABLE_SENSING			0x11

/* Public */
#define MIP_R1_LPWG_IDLE_REPORTRATE			0x21
#define MIP_R1_LPWG_ACTIVE_REPORTRATE		0x22
#define MIP_R1_LPWG_SENSITIVITY				0x23
#define MIP_R1_LPWG_ACTIVE_AREA			0x24
#define MIP_R1_LPWG_FAIL_REASON			0x2C

/* Knock On */
#define MIP_R1_LPWG_ENABLE					0x40
#define MIP_R1_LPWG_WAKEUP_TAP_COUNT		0x41
#define MIP_R1_LPWG_TOUCH_SLOP				0x42
#define MIP_R1_LPWG_MIN_INTERTAP_DISTANCE	0x44
#define MIP_R1_LPWG_MAX_INTERTAP_DISTANCE	0x46
#define MIP_R1_LPWG_MIN_INTERTAP_TIME		0x48
#define MIP_R1_LPWG_MAX_INTERTAP_TIME		0x4A
#define MIP_R1_LPWG_INT_DELAY_TIME			0x4C

/* Knock Code */
#define MIP_R1_LPWG_ENABLE2					0x50
#define MIP_R1_LPWG_WAKEUP_TAP_COUNT2		0x51
#define MIP_R1_LPWG_TOUCH_SLOP2				0x52
#define MIP_R1_LPWG_MIN_INTERTAP_DISTANCE2	0x54
#define MIP_R1_LPWG_MAX_INTERTAP_DISTANCE2	0x56
#define MIP_R1_LPWG_MIN_INTERTAP_TIME2		0x58
#define MIP_R1_LPWG_MAX_INTERTAP_TIME2		0x5A
#define MIP_R1_LPWG_INT_DELAY_TIME2			0x5C


/****************************************************************************
* Type Definitions
****************************************************************************/
#if 0
struct melfas_ts_data {
	struct i2c_client *client;
	TouchState currState;
	LpwgSetting lpwgSetting;
	struct class *class;
	dev_t mip_dev;
	struct cdev cdev;
	u8 *dev_fs_buf;
};

/* Firmware update error code */
enum fw_update_errno {
	fw_err_file_read = -4,
	fw_err_file_open = -3,
	fw_err_file_type = -2,
	fw_err_download = -1,
	fw_err_none = 0,
	fw_err_uptodate = 1,
};

enum {
	RAW_DATA_SHOW = 0,
	INTENSITY_SHOW,
	ABS_SHOW,
	DELTA_SHOW,
	JITTER_SHOW,
	OPENSHORT_SHOW,
	MUXSHORT_SHOW,
	LPWG_JITTER_SHOW,
	LPWG_ABS_SHOW,
};

enum {
	OUT_OF_AREA = 1,
	PALM_DETECTED,
	DELAY_TIME,
	TAP_TIME,
	TAP_DISTACE,
	TOUCH_SLOPE,
	MULTI_TOUCH,
	LONG_PRESS
};
#endif

/****************************************************************************
* Exported Variables
****************************************************************************/


/****************************************************************************
* Macros
****************************************************************************/


/****************************************************************************
* Global Function Prototypes
****************************************************************************/
#if 0
int mip_bin_fw_version(struct melfas_ts_data *ts, const u8 *fw_data, size_t fw_size, u8 *ver_buf);
int mip_flash_fw(struct melfas_ts_data *ts, const u8 *fw_data, size_t fw_size, bool force,
		 bool section);
ssize_t MIT300_GetTestResult(struct i2c_client *client, char *buf, int *result, int type);
void MIT300_Reset(int status, int delay);
#endif

#endif				/* _LGTP_DEVICE_MIT200_H_ */

/* End Of File */









/************************ FTK Header ************************/

#if 1				/*FTK Core.h */

/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2010-2015, Focaltech Ltd. All rights reserved.
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

#ifndef __LINUX_FTXXXX_H__
#define __LINUX_FTXXXX_H__
 /*******************************************************************************
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
*******************************************************************************/

/*******************************************************************************
* 1.Included header files
*******************************************************************************/



#if 0
#include "tpd.h"
/* #include "tpd_custom_fts.h" */
#include "cust_gpio_usage.h"
#include <linux/hrtimer.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/semaphore.h>
#include <linux/mutex.h>
#include <linux/syscalls.h>
#include <linux/byteorder/generic.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/rtpm_prio.h>
#include <asm/unistd.h>
#include <mach/mt_pm_ldo.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_boot.h>
#include <mach/irqs.h>
#include <cust_eint.h>
#include <linux/jiffies.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/version.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mount.h>
#include <linux/unistd.h>
#include <linux/proc_fs.h>
#include <linux/netdevice.h>
#include <../fs/proc/internal.h>
#endif
/*******************************************************************************
* Private constant and macro definitions using #define
*******************************************************************************/

/**********************Custom define begin**********************************************/

#define TPD_POWER_SOURCE_CUSTOM		MT6323_POWER_LDO_VGP1
#define IIC_PORT							0
#define TPD_HAVE_BUTTON		/* if have virtual key,need define the MACRO */
#define TPD_BUTTON_HEIGH					(40)	/* 100 */
#define TPD_KEY_COUNT					3	/* 4 */
#define TPD_KEYS							{ KEY_MENU, KEY_HOMEPAGE, KEY_BACK}
#define TPD_KEYS_DIM							{{80, 900, 20, TPD_BUTTON_HEIGH},\
{240, 900, 20, TPD_BUTTON_HEIGH}, {400, 900, 20, TPD_BUTTON_HEIGH} }
#define FT_ESD_PROTECT									0
/*********************Custom Define end*************************************************/
#define MT_PROTOCOL_B
#define A_TYPE												0
#define TPD_TYPE_CAPACITIVE
#define TPD_TYPE_RESISTIVE
#define TPD_POWER_SOURCE
#define TPD_NAME							"FTS"
#define TPD_I2C_NUMBER					0
#define TPD_WAKEUP_TRIAL					60
#define TPD_WAKEUP_DELAY					100
#define TPD_VELOCITY_CUSTOM_X				15
#define TPD_VELOCITY_CUSTOM_Y				20

#define CFG_MAX_TOUCH_POINTS				10	/* 5 */
#define MT_MAX_TOUCH_POINTS				10
#define FTS_MAX_ID							0x0F
#define FTS_TOUCH_STEP						6
#define FTS_FACE_DETECT_POS				1
#define FTS_TOUCH_X_H_POS					3
#define FTS_TOUCH_X_L_POS					4
#define FTS_TOUCH_Y_H_POS					5
#define FTS_TOUCH_Y_L_POS					6
#define FTS_TOUCH_EVENT_POS				3
#define FTS_TOUCH_ID_POS					5
#define FT_TOUCH_POINT_NUM				2
#define FTS_TOUCH_XY_POS					7
#define FTS_TOUCH_MISC						8
#define POINT_READ_BUF						(3 + FTS_TOUCH_STEP * CFG_MAX_TOUCH_POINTS)
#define FT_FW_NAME_MAX_LEN				50
#define TPD_DELAY							(2*HZ/100)
#define TPD_RES_X							1080	/* 480 */
#define TPD_RES_Y							1280	/* 800 */
/* #define TPD_CALIBRATION_MATRIX				{962, 0, 0, 0, 1600, 0, 0, 0} */
#define FT_PROXIMITY_ENABLE				0
/* #define TPD_AUTO_UPGRADE */
/* #define TPD_HAVE_CALIBRATION */
/* #define TPD_HAVE_TREMBLE_ELIMINATION */
/* #define TPD_CLOSE_POWER_IN_SLEEP */
/******************************************************************************/
/* Chip Device Type */
#define IC_FT5X06							0	/* x=2,3,4 */
#define IC_FT5606							1	/* ft5506/FT5606/FT5816 */
#define IC_FT5316							2	/* ft5x16 */
#define IC_FT6208							3	/* ft6208 */
#define IC_FT6x06							4	/* ft6206/FT6306 */
#define IC_FT5x06i						5	/* ft5306i */
#define IC_FT5x36							6	/* ft5336/ft5436/FT5436i */



/*register address*/
#define FTS_REG_CHIP_ID						0xA3	/* chip ID */
#define FTS_REG_FW_VER						0xA6	/* FW  version */
#define FTS_REG_VENDOR_ID					0xA8	/* TP vendor ID */
#define FTS_REG_POINT_RATE					0x88	/* report rate */
#define TPD_MAX_POINTS_2					2
#define TPD_MAX_POINTS_5					5
#define TPD_MAX_POINTS_10				10
#define AUTO_CLB_NEED					1
#define AUTO_CLB_NONEED					0
#define LEN_FLASH_ECC_MAX					0xFFFE
#define FTS_PACKET_LENGTH					32	/* 120 */
#define FTS_GESTRUE_POINTS				255
#define FTS_GESTRUE_POINTS_ONETIME		62
#define FTS_GESTRUE_POINTS_HEADER		8
#define FTS_GESTURE_OUTPUT_ADDRESS		0xD3
#define FTS_GESTURE_OUTPUT_UNIT_LENGTH	4

#define KEY_GESTURE_U						KEY_U
#define KEY_GESTURE_UP						KEY_UP
#define KEY_GESTURE_DOWN					KEY_DOWN
#define KEY_GESTURE_LEFT					KEY_LEFT
#define KEY_GESTURE_RIGHT					KEY_RIGHT
#define KEY_GESTURE_O						KEY_O
#define KEY_GESTURE_E						KEY_E
#define KEY_GESTURE_M						KEY_M
#define KEY_GESTURE_L						KEY_L
#define KEY_GESTURE_W						KEY_W
#define KEY_GESTURE_S						KEY_S
#define KEY_GESTURE_V						KEY_V
#define KEY_GESTURE_Z						KEY_Z

#define GESTURE_LEFT						0x20
#define GESTURE_RIGHT						0x21
#define GESTURE_UP							0x22
#define GESTURE_DOWN						0x23
#define GESTURE_DOUBLECLICK				0x24
#define GESTURE_O							0x30
#define GESTURE_W							0x31
#define GESTURE_M							0x32
#define GESTURE_E							0x33
#define GESTURE_L							0x44
#define GESTURE_S							0x46
#define GESTURE_V							0x54
#define GESTURE_Z							0x41


/* TOUCH */
#define FTK_TOUCH_DOWN	0x00
#define FTK_TOUCH_UP	0x01
#define FTK_TOUCH_CONTACT	0x02
#define FTK_TOUCH_NO_EVENT	0x03




/*******************************************************************************
* Private enumerations, structures and unions using typedef
*******************************************************************************/
/* IC info */
struct fts_Upgrade_Info {
	u8 CHIP_ID;
	u8 TPD_MAX_POINTS;
	u8 AUTO_CLB;
	u16 delay_aa;		/* delay of write FT_UPGRADE_AA */
	u16 delay_55;		/* delay of write FT_UPGRADE_55 */
	u8 upgrade_id_1;	/* upgrade id 1 */
	u8 upgrade_id_2;	/* upgrade id 2 */
	u16 delay_readid;	/* delay of read id */
	u16 delay_earse_flash;	/* delay of earse flash */
};

/*touch event info*/
struct ts_event {
	u16 au16_x[CFG_MAX_TOUCH_POINTS];	/* x coordinate */
	u16 au16_y[CFG_MAX_TOUCH_POINTS];	/* y coordinate */
	u8 au8_touch_event[CFG_MAX_TOUCH_POINTS];	/* touch event: 0 -- down; 1-- up; 2 -- contact */
	u8 au8_finger_id[CFG_MAX_TOUCH_POINTS];	/* touch ID */
	u16 pressure[CFG_MAX_TOUCH_POINTS];
	u16 area[CFG_MAX_TOUCH_POINTS];
	u8 touch_point;
	int touchs;
	u8 touch_point_num;
};

struct fts_ts_data {
#if 1
	struct i2c_client *client;
	TouchState currState;
	LpwgSetting lpwgSetting;
	struct class *class;
	dev_t mip_dev;
	struct cdev cdev;
	u8 *dev_fs_buf;
	u8 fw_ver[3];
	u8 fw_vendor_id;

#else
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

#endif
};

/* Firmware update error code */
enum fw_update_errno {
	fw_err_file_read = -4,
	fw_err_file_open = -3,
	fw_err_file_type = -2,
	fw_err_download = -1,
	fw_err_none = 0,
	fw_err_uptodate = 1,
};

enum {
	RAW_DATA_SHOW = 0,
	INTENSITY_SHOW,
	ABS_SHOW,
	DELTA_SHOW,
	JITTER_SHOW,
	OPENSHORT_SHOW,
	MUXSHORT_SHOW,
	LPWG_JITTER_SHOW,
	LPWG_ABS_SHOW,
};

enum {
	OUT_OF_AREA = 1,
	PALM_DETECTED,
	DELAY_TIME,
	TAP_TIME,
	TAP_DISTACE,
	TOUCH_SLOPE,
	MULTI_TOUCH,
	LONG_PRESS
};


/*******************************************************************************
* Static variables
*******************************************************************************/




/*******************************************************************************
* Global variable or extern global variabls/functions
*******************************************************************************/
/* Function Switches: define to open,  comment to close */
#define FTS_GESTRUE_EN							0
#define MTK_EN									1
#define FTS_APK_DEBUG
#define FT_TP									0
/* #define CONFIG_TOUCHPANEL_PROXIMITY_SENSOR */
/* #if FT_ESD_PROTECT */
/* extern int apk_debug_flag; */
/* #endif */

extern struct i2c_client *fts_i2c_client;
extern struct input_dev *fts_input_dev;
extern struct tpd_device *tpd;
extern int apk_debug_flag;
extern int lockscreen_stat;
extern struct workqueue_struct *touch_wq;


extern struct fts_Upgrade_Info fts_updateinfo_curr;
int fts_rw_iic_drv_init(struct i2c_client *client);
void fts_rw_iic_drv_exit(void);
void fts_get_upgrade_array(struct i2c_client *client);
#if FTS_GESTRUE_EN
extern int fts_Gesture_init(struct input_dev *input_dev);
extern int fts_read_Gestruedata(void);
#endif

#if 0
extern int fts_write_reg(struct i2c_client *client, u8 regaddr, u8 regvalue);
extern int fts_read_reg(struct i2c_client *client, u8 regaddr, u8 *regvalue);
extern int fts_i2c_read(struct i2c_client *client, char *writebuf, int writelen, char *readbuf,
			int readlen);
extern int fts_i2c_write(struct i2c_client *client, char *writebuf, int writelen);
#endif

extern int HidI2c_To_StdI2c(struct i2c_client *client);
extern int fts_ctpm_fw_upgrade_with_app_file(struct i2c_client *client, char *firmware_name);
extern int fts_ctpm_auto_clb(struct i2c_client *client);
extern int fts_ctpm_fw_upgrade_with_i_file(struct i2c_client *client, u8 *fw_buf, int len,
					   u8 *bootfw_buf, int boot_len);
extern int fts_ctpm_get_i_file_ver(void);
extern int fts_remove_sysfs(struct i2c_client *client);
extern void fts_release_apk_debug_channel(void);
extern int fts_ctpm_auto_upgrade(struct i2c_client *client, u8 *fw_buf, int len, u8 *bootfw_buf,
				 int boot_len);
#if FT_ESD_PROTECT
extern void esd_switch(s32 on);
#endif
extern void fts_reset_tp(int HighOrLow);
extern int fts_create_sysfs(struct i2c_client *client, TouchDriverData *pDriverData);
/* Apk and ADB functions */
extern int fts_create_apk_debug_channel(struct i2c_client *client);

int FT8707_RequestFirmware(char *name_app_fw, char *name_boot_fw, const struct firmware **fw_app,
			   const struct firmware **fw_boot);
void FT8707_Get_DefaultFWName(char **app_name, char **boot_name);

extern void reset_lcd_module(unsigned char reset);
extern void init_lcm_registers_sleep_out(void);
extern void FT8707_Reset(int status, int delay);
extern void lcm_init(void);

/*******************************************************************************
* Static function prototypes
*******************************************************************************/

#define FTS_DBGS
#ifdef FTS_DBGS
#define FTS_DBG(fmt, args...)				pr_err("[FTS]" fmt, ## args)
#else
#define FTS_DBG(fmt, args...)				do {} while (0)
#endif
#endif

#endif				/* FTK Core.h End */
