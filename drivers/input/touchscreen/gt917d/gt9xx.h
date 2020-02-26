/*
 * Goodix GT9xx touchscreen driver
 *
 * Copyright  (C)  2016 - 2017 Goodix. Ltd.
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
 * Version: 2.8.0.2
 * Release Date: 2017/12/14
 */

#ifndef _GOODIX_GT9XX_H_
#define _GOODIX_GT9XX_H_

#include <linux/kernel.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/major.h>
#include <linux/kdev_t.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif
#ifdef CONFIG_FB
#include <linux/notifier.h>
#include <linux/fb.h>
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/usb.h>
#include <linux/power_supply.h>

#define CONFIG_TOUCHSCREEN_GT9XX_UPDATE
#define CONFIG_TOUCHSCREEN_GT9XX_TOOL

#define GTP_TOOL_PEN	1
#define GTP_TOOL_FINGER 2

#define MAX_KEY_NUMS 4
#define GTP_CONFIG_MAX_LENGTH 240
#define GTP_ADDR_LENGTH       2

/***************************PART1:ON/OFF define*******************************/
#define GTP_DEBUG_ON          1
#define GTP_DEBUG_ARRAY_ON    0
#define GTP_DEBUG_FUNC_ON     1

#define GTP_CHARGER_SWITCH       1           /*gexiantao@20180403*/

struct goodix_point_t {
	int id;
	int x;
	int y;
	int w;
	int p;
	int tool_type;
};

struct goodix_config_data {
	int length;
	u8 data[GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH];
};

struct goodix_ts_platform_data {
	int irq_gpio;
	int rst_gpio;
	u32 irq_flags;
	u32 abs_size_x;
	u32 abs_size_y;
	u32 max_touch_id;
	u32 max_touch_width;
	u32 max_touch_pressure;
	u32 key_map[MAX_KEY_NUMS];
	u32 key_nums;
	u32 int_sync;
	u32 driver_send_cfg;
	u32 swap_x2y;
	u32 slide_wakeup;
	u32 auto_update;
	u32 auto_update_cfg;
	u32 esd_protect;
	u32 type_a_report;
	u32 power_off_sleep;
	u32 resume_in_workqueue;
	u32 pen_suppress_finger;
	u32 charger_cmd;/*gexiantao@20180403*/
	struct goodix_config_data config;
};

struct goodix_ts_esd {
	struct delayed_work delayed_work;
	struct mutex mutex;
	bool esd_on;
};
#if GTP_CHARGER_SWITCH /*gexiantao@20180403*/
struct goodix_ts_charger{
    struct delayed_work delayed_work;
	struct mutex mutex;
	bool charger_on;
};
#endif

enum {
	WORK_THREAD_ENABLED = 0,
	HRTIMER_USED,
	FW_ERROR,

	DOZE_MODE,
	SLEEP_MODE,
	POWER_OFF_MODE,
	RAW_DATA_MODE,

	FW_UPDATE_RUNNING,
	PANEL_RESETTING
};

struct goodix_pinctrl {
	struct pinctrl *pinctrl;
	struct pinctrl_state *default_sta;
	struct pinctrl_state *int_out_high;
	struct pinctrl_state *int_out_low;
	struct pinctrl_state *int_input;
};

struct goodix_fw_info {
	u8 pid[6];
	u16 version;
	u8 sensor_id;
	u16 cfg_ver;
};

struct goodix_ts_data {
	unsigned long flags; /* This member record the device status */

	struct goodix_ts_esd ts_esd;
#if GTP_CHARGER_SWITCH /*gexiantao@20180403*/
    struct goodix_ts_charger ts_charger;
#endif
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct input_dev *pen_dev;
	struct goodix_ts_platform_data *pdata;
	/* use pinctrl control int-pin output low or high */
	struct goodix_pinctrl pinctrl;
	struct hrtimer timer;
	struct mutex lock;
	struct notifier_block ps_notif;
	struct regulator *vdd_ana;
	struct regulator *vcc_i2c;
#if defined(CONFIG_FB)
	struct notifier_block notifier;
	struct work_struct fb_notify_work;
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif
	struct goodix_fw_info fw_info;
	bool force_update;
	bool init_done;
	u16 pre_touch;
};

/************************* PART2:TODO define *******************************/
/* STEP_1(REQUIRED): Define Configuration Information Group(s)
 Sensor_ID Map:
	 sensor_opt1 sensor_opt2 Sensor_ID
		GND         GND          0
		VDDIO      GND          1
		NC           GND          2
		GND         NC/300K    3
		VDDIO      NC/300K    4
		NC           NC/300K    5
*/
/* TODO: define your own default or for Sensor_ID == 0 config here.
	 The predefined one is just a sample config,
	 which is not suitable for your tp in most cases. */
#define CTP_CFG_GROUP0 {\
	0x41, 0xD0, 0x02, 0x00, 0x05, 0x0A, 0x34, \
	0x00, 0x01, 0x08, 0x28, 0x05, 0x50, 0x32, \
	0x03, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, \
	0x00, 0x00, 0x17, 0x19, 0x1E, 0x14, 0x8C, \
	0x2D, 0x0E, 0x3C, 0x3E, 0x82, 0x0A, 0x82, \
	0x0A, 0x00, 0x99, 0x33, 0x1D, 0x00, 0x00, \
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
	0x00, 0x2B, 0x19, 0x64, 0x94, 0xC0, 0x02, \
	0x08, 0x00, 0x00, 0x04,	0xF2, 0x1C, 0x00, \
	0xB9, 0x26, 0x00, 0x93, 0x32, 0x00, 0x77, \
	0x42, 0x00, 0x62, 0x57, 0x00, 0x62, 0x00, \
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
	0x00, 0x00, 0x00, 0xFF, 0x65, 0x00, 0x00, \
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
	0x19, 0x46, 0x00, 0x00, 0x00, 0x00, 0x32, \
	0x1C, 0x1A, 0x18, 0x16, 0x14, 0x12, 0x10, \
	0x0E, 0x0C, 0x0A, 0x08, 0x06, 0x04, 0x02, \
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
	0x00, 0x00, 0x00, 0x02,	0x04, 0x06, 0x08, \
	0x0A, 0x0C, 0x0F, 0x10, 0x12, 0x13, 0x14, \
	0x18, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, \
	0x22, 0x24, 0x26, 0x28, 0x29, 0x2A, 0xFF, \
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
	0x00, 0x00, 0xB8, 0x01\
}

/* TODO: define your config for Sensor_ID == 1 here, if needed */
#define CTP_CFG_GROUP1 {\
}

/* TODO: define your config for Sensor_ID == 2 here, if needed */
#define CTP_CFG_GROUP2 {\
}

/* TODO: define your config for Sensor_ID == 3 here, if needed */
#define CTP_CFG_GROUP3 {\
}
/* TODO: define your config for Sensor_ID == 4 here, if needed */
#define CTP_CFG_GROUP4 {\
	0x53, 0xD0, 0x02, 0x00, 0x05, 0x05, 0xF5, 0xD5, 0x21, 0x48, 0x2D, 0x0F,\
	0x5A, 0x41, 0x0E, 0x05, 0x00, 0x00, 0x32, 0x32, 0x20, 0x00, 0x05, 0x14,\
	0x14, 0x1A, 0x14, 0x8B, 0x2B, 0x0C, 0xB5, 0xB7, 0xEB, 0x04, 0xFF, 0xFE,\
	0x00, 0x22, 0x33, 0x10, 0x3C, 0x80, 0x00, 0x00, 0x00, 0x1E, 0x12, 0x41,\
	0x23, 0x12, 0x5A, 0xAA, 0xBE, 0x4A, 0x55, 0x04, 0x00, 0x14, 0x19, 0x04,\
	0x80, 0xAB, 0x00, 0x7F, 0xAF, 0x64, 0x7E, 0xB3, 0x00, 0x7E, 0xB7, 0x00,\
	0x7B, 0xBB, 0x3C, 0x7B, 0x08, 0x30, 0x00, 0x00, 0xF8, 0x70, 0x50, 0xFF,\
	0xFF, 0x17, 0x00, 0x46, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1E,\
	0x08, 0x46, 0x80, 0x08, 0x0A, 0x00, 0xA0, 0x00, 0x3C, 0x28, 0x19, 0x19,\
	0x80, 0x11, 0x00, 0x00, 0x18, 0x16, 0x14, 0x12, 0x10, 0x0E, 0x0C, 0x0A,\
	0x08, 0x06, 0x04, 0x02, 0xFF, 0xFF, 0x28, 0x00, 0x32, 0x20, 0x00, 0x06,\
	0x00, 0x00, 0x0A, 0x06, 0x10, 0x08, 0x0A, 0x22, 0xEB, 0x04, 0x26, 0x24,\
	0x22, 0x21, 0x20, 0x1F, 0x1E, 0x1D, 0x1C, 0x18, 0x16, 0x12, 0x10, 0x0F,\
	0x0C, 0x0A, 0x08, 0x06, 0x04, 0x02, 0x00, 0x13, 0xFF, 0xFF, 0xFF, 0xFF,\
	0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,\
	0x00, 0x00, 0x00, 0x00, 0x28, 0x0B, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00,\
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,\
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,\
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE6, 0x10, 0xEF, 0x01\
}

/* TODO: define your config for Sensor_ID == 5 here, if needed */
#define CTP_CFG_GROUP5 {\
}

/* STEP_2(REQUIRED): Customize your I/O ports & I/O operations */
#define GTP_RST_PORT    64 /* EXYNOS4_GPX2(0) */
#define GTP_INT_PORT    65 /* EXYNOS4_GPX2(1) */

#define GTP_GPIO_AS_INPUT(pin)          (gpio_direction_input(pin))
#define GTP_GPIO_AS_INT(pin)            (GTP_GPIO_AS_INPUT(pin))
#define GTP_GPIO_GET_VALUE(pin)         gpio_get_value(pin)
#define GTP_GPIO_OUTPUT(pin, level)      gpio_direction_output(pin, level)
#define GTP_GPIO_REQUEST(pin, label)    gpio_request(pin, label)
#define GTP_GPIO_FREE(pin)              gpio_free(pin)

/* STEP_3(optional): Specify your special config info if needed */
#define GTP_DEFAULT_MAX_X	 720    /* default coordinate max values */
#define GTP_DEFAULT_MAX_Y	 1080
#define GTP_DEFAULT_MAX_WIDTH	 1024
#define GTP_DEFAULT_MAX_PRESSURE 1024
#define GTP_DEFAULT_INT_TRIGGER	 1 /* 1 rising, 2 falling */
#define GTP_MAX_TOUCH_ID	 16

/* STEP_4(optional): If keys are available and reported as keys,
config your key info here */
#define GTP_KEY_TAB {KEY_MENU, KEY_HOME, KEY_BACK, KEY_HOMEPAGE, \
	KEY_F1, KEY_F2, KEY_F3}

/**************************PART3:OTHER define*******************************/
#define GTP_DRIVER_VERSION	"V2.8.0.2<2017/12/14>"
#define GTP_I2C_NAME		"goodix-ts"
#define GT91XX_CONFIG_PROC_FILE	"gt9xx_config"
#define GT9XX_LOCKDOWN_PROC_FILE	"tp_lockdown_info"
#define GT9XX_WAKEUP_GESTURE_PROC	"tp_wakeup_gesture"
#define GTP_POLL_TIME		10
#define GTP_CONFIG_MIN_LENGTH	186
#define GTP_ESD_CHECK_VALUE	0xAA
#define RETRY_MAX_TIMES		5
#define PEN_TRACK_ID		9
#define MASK_BIT_8		0x80
#define FAIL			0
#define SUCCESS			1

/* Registers define */
#define GTP_REG_COMMAND		0x8040
#define GTP_REG_ESD_CHECK	0x8041
#define GTP_REG_COMMAND_CHECK	0x8046
#define GTP_REG_CONFIG_DATA	0x8047
#define GTP_REG_VERSION		0x8140
#define GTP_REG_SENSOR_ID	0x814A
#define GTP_REG_DOZE_BUF	0x814B
#define GTP_READ_COOR_ADDR	0x814E
/*Add by HQ-zmc [Date: 2018-03-03 17:32:53]*/
#define GTP_READ_CFG_VER	0x8047

/* Sleep time define */
#define GTP_1_DLY_MS		1
#define GTP_2_DLY_MS		2
#define GTP_10_DLY_MS		10
#define GTP_20_DLY_MS		20
#define GTP_50_DLY_MS		50
#define GTP_58_DLY_MS		58
#define GTP_100_DLY_MS		100
#define GTP_500_DLY_MS		500
#define GTP_1000_DLY_MS		1000
#define GTP_3000_DLY_MS		3000

#define RESOLUTION_LOC        3
#define TRIGGER_LOC           8

/*Add by HQ-zmc [Date: 2018-01-19 09:28:17]*/
#define GTP_REG_COLOR_GT915     0xc8a4
#define GTP_REG_COLOR_GT917     0x81a0

#define CFG_GROUP_LEN(p_cfg_grp)  (sizeof(p_cfg_grp) / sizeof(p_cfg_grp[0]))
/* Log define */
#define GTP_DEBUG(fmt, arg...) \
do { \
	if (GTP_DEBUG_ON) {\
		pr_info("<<-GTP-DEBUG->> [%d]"fmt"\n", __LINE__, ##arg);\
	} \
} while (0)
#define GTP_DEBUG_ARRAY(array, num) \
do { \
	s32 i;\
	u8 *a = array;\
	if (GTP_DEBUG_ARRAY_ON) {\
		pr_warn("<<-GTP-DEBUG-ARRAY->>\n");\
		for (i = 0; i < (num); i++) {\
			pr_warn("%02x  ", (a)[i]);\
			if ((i + 1) % 10 == 0) {\
				pr_warn("\n");\
			} \
		} \
		pr_warn("\n");\
	} \
} while (0)
#define GTP_DEBUG_FUNC() \
do {\
	if (GTP_DEBUG_FUNC_ON) {\
		pr_warn("<<-GTP-FUNC->>  Func:%s@Line:%d\n", \
		__func__, __LINE__);\
	} \
} while (0)
#define GTP_SWAP(x, y) \
do {\
	typeof(x) z = x;\
	x = y;\
	y = z;\
} while (0)

/******************************End of Part III********************************/
#ifdef CONFIG_OF
extern int gtp_parse_dt_cfg(struct device *dev, u8 *cfg, int *cfg_len, u8 sid);
#endif

extern void gtp_reset_guitar(struct i2c_client *client, s32 ms);
extern void gtp_int_sync(struct goodix_ts_data *ts, s32 ms);
extern void gtp_esd_on(struct goodix_ts_data *ts);
extern void gtp_esd_off(struct goodix_ts_data *ts);
extern void gtp_work_control_enable(struct goodix_ts_data *ts, bool enable);
extern void gtp_charger_on(struct goodix_ts_data *ts);
extern void gtp_charger_off(struct goodix_ts_data *ts);

#ifdef CONFIG_TOUCHSCREEN_GT9XX_UPDATE
extern u16 show_len;
extern u16 total_len;
extern u8 gup_init_update_proc(struct goodix_ts_data *);
extern s32 gup_update_proc(void *dir);
extern s32 gup_enter_update_mode(struct i2c_client *client);
extern void gup_leave_update_mode(struct i2c_client *client);
#endif

#ifdef CONFIG_TOUCHSCREEN_GT9XX_TOOL
extern s32 init_wr_node(struct i2c_client *);
extern void uninit_wr_node(void);
#endif

/*********** For gt9xx_update Start *********/
extern struct i2c_client *i2c_connect_client;
extern void gtp_reset_guitar(struct i2c_client *client, s32 ms);
extern void gtp_int_output(struct goodix_ts_data *ts, int level);
extern s32 gtp_send_cfg(struct i2c_client *client);
extern s32 gtp_get_fw_info(struct i2c_client *, struct goodix_fw_info *fw_info);
extern s32 gtp_i2c_read_dbl_check(struct i2c_client *, u16, u8 *, int);
extern int gtp_i2c_read(struct i2c_client *, u8 *, int);
extern int gtp_i2c_write(struct i2c_client *, u8 *, int);
extern s32 gtp_fw_startup(struct i2c_client *client);
extern int gtp_ascii_to_array(const u8 *src_buf, int src_len, u8 *dst_buf);
extern void ctp_vendor_info(struct i2c_client *client, struct goodix_fw_info *gt_fw_info);
/*********** For gt9xx_update End *********/
extern int gtp_i2c_test(struct i2c_client *client);

#endif /* _GOODIX_GT9XX_H_ */
