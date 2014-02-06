/* drivers/input/touchscreen/gt9xx.h
 *
 * Copyright (c) 2013-2015, The Linux Foundation. All rights reserved.
 *
 * Linux Foundation chooses to take subject only to the GPLv2 license
 * terms, and distributes only under these terms.
 *
 * 2010 - 2013 Goodix Technology.
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
 */

#ifndef _GOODIX_GT9XX_H_
#define _GOODIX_GT9XX_H_

#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>

#if defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#elif defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#define GOODIX_SUSPEND_LEVEL 1
#endif

#define MAX_BUTTONS 4
#define GOODIX_MAX_CFG_GROUP	6
#define GTP_FW_NAME_MAXSIZE	50

struct goodix_ts_platform_data {
	int irq_gpio;
	u32 irq_gpio_flags;
	int reset_gpio;
	u32 reset_gpio_flags;
	const char *product_id;
	const char *fw_name;
	u32 x_max;
	u32 y_max;
	u32 x_min;
	u32 y_min;
	u32 panel_minx;
	u32 panel_miny;
	u32 panel_maxx;
	u32 panel_maxy;
	bool force_update;
	bool i2c_pull_up;
	bool enable_power_off;
	size_t config_data_len[GOODIX_MAX_CFG_GROUP];
	u8 *config_data[GOODIX_MAX_CFG_GROUP];
	u32 button_map[MAX_BUTTONS];
	u8 num_button;
	bool have_touch_key;
	bool driver_send_cfg;
	bool change_x2y;
	bool with_pen;
	bool slide_wakeup;
	bool dbl_clk_wakeup;
};
struct goodix_ts_data {
	spinlock_t irq_lock;
	struct i2c_client *client;
	struct input_dev  *input_dev;
	struct goodix_ts_platform_data *pdata;
	struct hrtimer timer;
	struct workqueue_struct *goodix_wq;
	struct work_struct	work;
	char fw_name[GTP_FW_NAME_MAXSIZE];
	struct delayed_work goodix_update_work;
	s32 irq_is_disabled;
	s32 use_irq;
	u16 abs_x_max;
	u16 abs_y_max;
	u16 addr;
	u8  max_touch_num;
	u8  int_trigger_type;
	u8  green_wake_mode;
	u8  chip_type;
	u8 *config_data;
	u8  enter_update;
	u8  gtp_is_suspend;
	u8  gtp_rawdiff_mode;
	u8  gtp_cfg_len;
	u8  fixed_cfg;
	u8  esd_running;
	u8  fw_error;
	bool power_on;
	struct mutex lock;
	bool fw_loading;
	bool force_update;
	struct regulator *avdd;
	struct regulator *vdd;
	struct regulator *vcc_i2c;
#if defined(CONFIG_FB)
	struct notifier_block fb_notif;
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif
	struct dentry *debug_base;
};

extern u16 show_len;
extern u16 total_len;

/***************************PART1:ON/OFF define*******************************/
#define GTP_CUSTOM_CFG			0
#define GTP_ESD_PROTECT			0

#define GTP_IRQ_TAB            {\
				IRQ_TYPE_EDGE_RISING,\
				IRQ_TYPE_EDGE_FALLING,\
				IRQ_TYPE_LEVEL_LOW,\
				IRQ_TYPE_LEVEL_HIGH\
				}


#define GTP_IRQ_TAB_RISING	0
#define GTP_IRQ_TAB_FALLING	1
#if GTP_CUSTOM_CFG
#define GTP_MAX_HEIGHT		800
#define GTP_MAX_WIDTH		480
#define GTP_INT_TRIGGER		GTP_IRQ_TAB_RISING
#else
#define GTP_MAX_HEIGHT		4096
#define GTP_MAX_WIDTH		4096
#define GTP_INT_TRIGGER		GTP_IRQ_TAB_FALLING
#endif

#define GTP_PRODUCT_ID_MAXSIZE	5
#define GTP_PRODUCT_ID_BUFFER_MAXSIZE	6
#define GTP_FW_VERSION_BUFFER_MAXSIZE	4
#define GTP_MAX_TOUCH		5
#define GTP_ESD_CHECK_CIRCLE	2000      /* jiffy: ms */

/***************************PART3:OTHER define*********************************/
#define GTP_DRIVER_VERSION	"V1.8.1<2013/09/01>"
#define GTP_I2C_NAME		"Goodix-TS"
#define GTP_POLL_TIME		10     /* jiffy: ms*/
#define GTP_ADDR_LENGTH		2
#define GTP_CONFIG_MIN_LENGTH	186
#define GTP_CONFIG_MAX_LENGTH	240
#define FAIL			0
#define SUCCESS			1
#define SWITCH_OFF		0
#define SWITCH_ON		1

/* Registers define */
#define GTP_READ_COOR_ADDR	0x814E
#define GTP_REG_SLEEP		0x8040
#define GTP_REG_SENSOR_ID	0x814A
#define GTP_REG_CONFIG_DATA	0x8047
#define GTP_REG_FW_VERSION	0x8144
#define GTP_REG_PRODUCT_ID	0x8140

#define GTP_I2C_RETRY_3		3
#define GTP_I2C_RETRY_5		5
#define GTP_I2C_RETRY_10	10

#define RESOLUTION_LOC		3
#define TRIGGER_LOC		8

/* HIGH: 0x28/0x29, LOW: 0xBA/0xBB */
#define GTP_I2C_ADDRESS_HIGH	0x14
#define GTP_I2C_ADDRESS_LOW	0x5D
#define GTP_VALID_ADDR_START	0x8040
#define GTP_VALID_ADDR_END	0x8177

#define CFG_GROUP_LEN(p_cfg_grp) (sizeof(p_cfg_grp) / sizeof(p_cfg_grp[0]))

/* GTP CM_HEAD RW flags */
#define GTP_RW_READ			0
#define GTP_RW_WRITE			1
#define GTP_RW_READ_IC_TYPE		2
#define GTP_RW_WRITE_IC_TYPE		3
#define GTP_RW_FILL_INFO		4
#define GTP_RW_NO_WRITE			5
#define GTP_RW_READ_ERROR		6
#define GTP_RW_DISABLE_IRQ		7
#define GTP_RW_READ_VERSION		8
#define GTP_RW_ENABLE_IRQ		9
#define GTP_RW_ENTER_UPDATE_MODE	11
#define GTP_RW_LEAVE_UPDATE_MODE	13
#define GTP_RW_UPDATE_FW		15
#define GTP_RW_CHECK_RAWDIFF_MODE	17

/* GTP need flag or interrupt */
#define GTP_NO_NEED			0
#define GTP_NEED_FLAG			1
#define GTP_NEED_INTERRUPT		2

/*****************************End of Part III********************************/

void gtp_esd_switch(struct i2c_client *client, int on);

int gtp_i2c_read_dbl_check(struct i2c_client *client, u16 addr,
					u8 *rxbuf, int len);
int gtp_send_cfg(struct goodix_ts_data *ts);
void gtp_reset_guitar(struct goodix_ts_data *ts, int ms);
void gtp_irq_disable(struct goodix_ts_data *ts);
void gtp_irq_enable(struct goodix_ts_data *ts);

#ifdef CONFIG_GT9XX_TOUCHPANEL_DEBUG
s32 init_wr_node(struct i2c_client *client);
void uninit_wr_node(void);
#endif

u8 gup_init_update_proc(struct goodix_ts_data *ts);
s32 gup_enter_update_mode(struct i2c_client *client);
void gup_leave_update_mode(struct i2c_client *client);
s32 gup_update_proc(void *dir);
extern struct i2c_client  *i2c_connect_client;
#endif /* _GOODIX_GT9XX_H_ */
