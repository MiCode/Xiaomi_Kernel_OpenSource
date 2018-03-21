/*
 *
 * FocalTech ft5x06 TouchScreen driver header file.
 *
 * Copyright (c) 2010  Focal tech Ltd.
 * Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
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
#ifndef __LINUX_FT5X06_TS_H__
#define __LINUX_FT5X06_TS_H__

#define FT5X06_ID       0x55
#define FT5X16_ID       0x0A
#define FT5X36_ID       0x14
#define FT6X06_ID       0x06
#define FT6X36_ID       0x36

#define FT_DRIVER_VERSION   0x02

#define FT_META_REGS        3
#define FT_ONE_TCH_LEN      6
#define FT_TCH_LEN(x)       (FT_META_REGS + FT_ONE_TCH_LEN * x)

#define FT_PRESS        0x7F
#define FT_MAX_ID       0x0F
#define FT_TOUCH_X_H_POS    3
#define FT_TOUCH_X_L_POS    4
#define FT_TOUCH_Y_H_POS    5
#define FT_TOUCH_Y_L_POS    6
#define FT_TD_STATUS        2
#define FT_TOUCH_EVENT_POS  3
#define FT_TOUCH_ID_POS     5
#define FT_TOUCH_DOWN       0
#define FT_TOUCH_CONTACT    2
#define FT_LOCKDOWN_SIZE	8


/*register address*/
#define FT_REG_DEV_MODE     0x00
#define FT_DEV_MODE_REG_CAL 0x02
#define FT_REG_ID       0xA3
#define FT_REG_PMODE        0xA5
#define FT_REG_FW_VER       0xA6
#define FT_REG_FW_VENDOR_ID 0xA8
#define FT_REG_POINT_RATE   0x88
#define FT_REG_THGROUP      0x80
#define FT_REG_ECC      0xCC
#define FT_REG_RESET_FW     0x07
#define FT_REG_FW_MIN_VER   0xB2
#define FT_REG_FW_SUB_MIN_VER   0xB3

/*Firmware vendors*/
#define VENDOR_O_FILM		0x51
#define VENDOR_Lens		0x3B
#define VENDOR_DS			0x53

/*IC name*/

#define TP_IC_FT5X06               0x55
#define TP_IC_FT5606               0x08
#define TP_IC_FT5X16               0x0A
#define TP_IC_FT6208               0x05
#define TP_IC_FT6X06               0x06
#define TP_IC_FT6X36               0x36
#define TP_IC_FT5336               0x14
#define TP_IC_FT3316               0x13
#define TP_IC_FT5436i              0x12
#define TP_IC_FT5336i              0x11
#define TP_IC_FT5346               0x54


/*TP Color*/
#define TP_White      0x31
#define  TP_Black      0x32
#define  TP_Golden    0x38

/*TP Maker*/
#define TP_OUFEI      0x34
#define  TP_LENS      0x32
#define  TP_DS           0x45

/*LCD Maker*/
#define TP_TIANMA      0x36

#define TP_EBBG          0x37

/*PROJECT Id*/
#define PROJECT_C3B 0x0c
#define PROJECT_C3N 0xc3

/* power register bits*/
#define FT_PMODE_ACTIVE     0x00
#define FT_PMODE_MONITOR    0x01
#define FT_PMODE_STANDBY    0x02
#define FT_PMODE_HIBERNATE  0x03
#define FT_FACTORYMODE_VALUE    0x40
#define FT_WORKMODE_VALUE   0x00
#define FT_RST_CMD_REG1     0xFC
#define FT_RST_CMD_REG2     0xBC
#define FT_READ_ID_REG      0x90
#define FT_ERASE_APP_REG    0x61
#define FT_ERASE_PANEL_REG  0x63
#define FT_FW_START_REG     0xBF

#define FT_STATUS_NUM_TP_MASK   0x0F

#define FT_VTG_MIN_UV       2600000
#define FT_VTG_MAX_UV       3300000
#define FT_I2C_VTG_MIN_UV   1800000
#define FT_I2C_VTG_MAX_UV   1800000

#define FT_COORDS_ARR_SIZE  4
#define MAX_BUTTONS     4

#define FT_8BIT_SHIFT       8
#define FT_4BIT_SHIFT       4
#define FT_FW_NAME_MAX_LEN  50

#define FT5316_ID       0x0A
#define FT5306I_ID      0x55
#define FT6X06_ID       0x06
#define FT6X36_ID       0x36

#define FT_UPGRADE_AA       0xAA
#define FT_UPGRADE_55       0x55

#define FT_FW_MIN_SIZE      8
#define FT_FW_MAX_SIZE      32768

#define FT5x0x_REG_FW_VER   0xA6


/* Firmware file is not supporting minor and sub minor so use 0 */
#define FT_FW_FILE_MAJ_VER(x)   ((x)->data[(x)->size - 2])
#define FT_FW_FILE_MIN_VER(x)   0
#define FT_FW_FILE_SUB_MIN_VER(x) 0
#define FT_FW_FILE_VENDOR_ID(x) ((x)->data[(x)->size - 1])

#define FT_FW_FILE_MAJ_VER_FT6X36(x)    ((x)->data[0x10a])
#define FT_FW_FILE_VENDOR_ID_FT6X36(x)  ((x)->data[0x108])

/**
* Application data verification will be run before upgrade flow.
* Firmware image stores some flags with negative and positive value
* in corresponding addresses, we need pick them out do some check to
* make sure the application data is valid.
*/
#define FT_FW_CHECK(x, ts_data) \
	(ts_data->family_id == FT6X36_ID ? \
	(((x)->data[0x104] ^ (x)->data[0x105]) == 0xFF \
	&& ((x)->data[0x106] ^ (x)->data[0x107]) == 0xFF) : \
	(((x)->data[(x)->size - 8] ^ (x)->data[(x)->size - 6]) == 0xFF \
	&& ((x)->data[(x)->size - 7] ^ (x)->data[(x)->size - 5]) == 0xFF \
	&& ((x)->data[(x)->size - 3] ^ (x)->data[(x)->size - 4]) == 0xFF))

#define FT_MAX_TRIES        5
#define FT_RETRY_DLY        20

#define FT_MAX_WR_BUF       10
#define FT_MAX_RD_BUF       2
#define FT_FW_PKT_LEN       128
#define FT_FW_PKT_META_LEN  6
#define FT_FW_PKT_DLY_MS    20
#define FT_FW_LAST_PKT      0x6ffa
#define FT_EARSE_DLY_MS     100
#define FT_55_AA_DLY_NS     5000

#define FT_UPGRADE_LOOP     30
#define FT_CAL_START        0x04
#define FT_CAL_FIN      0x00
#define FT_CAL_STORE        0x05
#define FT_CAL_RETRY        100
#define FT_REG_CAL      0x00
#define FT_CAL_MASK     0x7C

#define FT_INFO_MAX_LEN     512
#define FTS_PACKET_LENGTH	128


#define FT_BLOADER_SIZE_OFF 12
#define FT_BLOADER_NEW_SIZE 30
#define FT_DATA_LEN_OFF_OLD_FW  8
#define FT_DATA_LEN_OFF_NEW_FW  14
#define FT_FINISHING_PKT_LEN_OLD_FW 6
#define FT_FINISHING_PKT_LEN_NEW_FW 12
#define FT_MAGIC_BLOADER_Z7 0x7bfa
#define FT_MAGIC_BLOADER_LZ4    0x6ffa
#define FT_MAGIC_BLOADER_GZF_30 0x7ff4
#define FT_MAGIC_BLOADER_GZF    0x7bf4

enum {
	FT_BLOADER_VERSION_LZ4 = 0,
	FT_BLOADER_VERSION_Z7 = 1,
	FT_BLOADER_VERSION_GZF = 2,
};

enum {
	FT_FT5336_FAMILY_ID_0x11 = 0x11,
	FT_FT5336_FAMILY_ID_0x12 = 0x12,
	FT_FT5336_FAMILY_ID_0x13 = 0x13,
	FT_FT5336_FAMILY_ID_0x14 = 0x14,
};

struct fw_upgrade_info {
	bool auto_cal;
	u16 delay_aa;
	u16 delay_55;
	u8 upgrade_id_1;
	u8 upgrade_id_2;
	u16 delay_readid;
	u16 delay_erase_flash;
};

struct Upgrade_Info {
	u8 CHIP_ID;
	u8 FTS_NAME[20];
	u8 TPD_MAX_POINTS;
	u8 AUTO_CLB;
	u16 delay_aa;       /*delay of write FT_UPGRADE_AA */
	u16 delay_55;       /*delay of write FT_UPGRADE_55 */
	u8 upgrade_id_1;    /*upgrade id 1 */
	u8 upgrade_id_2;    /*upgrade id 2 */
	u16 delay_readid;   /*delay of read id */
	u16 delay_earse_flash; /*delay of earse flash*/
};


struct ft5x06_ts_platform_data {
	struct fw_upgrade_info info;
	const char *name;
	const char *fw_name;
	u32 irqflags;
	u32 irq_gpio;
	u32 irq_gpio_flags;
	u32 reset_gpio;
	u32 reset_gpio_flags;
	u32 family_id;
	u32 x_max;
	u32 y_max;
	u32 x_min;
	u32 y_min;
	u32 panel_minx;
	u32 panel_miny;
	u32 panel_maxx;
	u32 panel_maxy;
	u32 group_id;
	u32 hard_rst_dly;
	u32 soft_rst_dly;
	u32 num_max_touches;
	bool fw_vkey_support;
	bool no_force_update;
	bool i2c_pull_up;
	bool ignore_id_check;
	int (*power_init) (bool);
	int (*power_on) (bool);
};

struct ft5x06_ts_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	const struct ft5x06_ts_platform_data *pdata;
	struct regulator *vdd;
	struct regulator *vcc_i2c;
	char fw_name[FT_FW_NAME_MAX_LEN];
	u8 lockdown_info[FT_LOCKDOWN_SIZE];
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
	struct work_struct fb_notify_work;
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif
	struct pinctrl *ts_pinctrl;
	struct pinctrl_state *gpio_state_active;
	struct pinctrl_state *gpio_state_suspend;
};



#define CTP_IC_TYPE_0 0x12
#define CTP_IC_TYPE_1 0x14
#define CTP_IC_TYPE_2 0x54

#define CTP_SYS_APK_UPDATE 0

#define TPD_AUTO_UPGRADE 1
#define FTS_PROC_APK_DEBUG 1

#define CTP_CHARGER_DETECT 1

#define CTP_PROC_INTERFACE 1



#if CTP_PROC_INTERFACE
extern int fts_open_short_test(char *ini_file_name, char *bufdest, ssize_t *pinumread);
#endif
#define WT_ADD_CTP_INFO    1
#define WT_CTP_GESTURE_SUPPORT 1
#define KEYCODE_WAKEUP 143
#define MXT_INPUT_EVENT_START			0
#define MXT_INPUT_EVENT_SENSITIVE_MODE_OFF	0
#define MXT_INPUT_EVENT_SENSITIVE_MODE_ON	1
#define MXT_INPUT_EVENT_STYLUS_MODE_OFF		2
#define MXT_INPUT_EVENT_STYLUS_MODE_ON		3
#define MXT_INPUT_EVENT_WAKUP_MODE_OFF		4
#define MXT_INPUT_EVENT_WAKUP_MODE_ON		5
#define MXT_INPUT_EVENT_EDGE_DISABLE		6
#define MXT_INPUT_EVENT_EDGE_FINGER		7
#define MXT_INPUT_EVENT_EDGE_HANDGRIP		8
#define MXT_INPUT_EVENT_EDGE_FINGER_HANDGRIP	9
#define MXT_INPUT_EVENT_END			9
#define CTP_DEBUG_ON 0
#define CTP_DEBUG_FUNC_ON 1
#define CTP_INFO(fmt, arg...)           printk("FT5X06-TP-TAG INFO:"fmt"\n", ##arg)

#define CTP_ERROR(fmt, arg...)          printk("FT5X06-TP-TAG ERROR:"fmt"\n", ##arg)

#define CTP_DEBUG(fmt, arg...)          do {\
									     if (CTP_DEBUG_ON)\
									     printk("FT5X06-TP-TAG DEBUG:[%d]"fmt"\n", __LINE__, ##arg);\
									   } while (0)
#define CTP_DEBUG_FUNC()               do {\
									     if (CTP_DEBUG_FUNC_ON)\
									     printk("FT5X06-TP-TAG Func:%s@Line:%d\n", __func__, __LINE__);\
									   } while (0)


#endif
