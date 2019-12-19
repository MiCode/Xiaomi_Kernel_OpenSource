/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2010-2017, Focaltech Ltd. All rights reserved.
 * Copyright (C) 2019 XiaoMi, Inc.
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
* File Name: focaltech_core.h

* Author: Focaltech Driver Team
*
* Created: 2016-08-08
*
* Abstract:
*
* Reference:
*
*****************************************************************************/

#ifndef __LINUX_FOCALTECH_CORE_H__
#define __LINUX_FOCALTECH_CORE_H__
/*****************************************************************************
* Included header files
*****************************************************************************/
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/firmware.h>
#include <linux/debugfs.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/workqueue.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mount.h>
#include <linux/netdevice.h>
#include <linux/unistd.h>
#include <linux/ioctl.h>
#include <linux/vmalloc.h>
#include "focaltech_common.h"
#include <linux/firmware.h>
#include <linux/power_supply.h>

/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/
#define FTS_MAX_POINTS_SUPPORT              10	/* constant value, can't be changed */
#define FTS_MAX_KEYS                        4
#define FTS_KEY_WIDTH                       50
#define FTS_ONE_TCH_LEN                     6

#define FTS_MAX_ID                          0x0A
#define FTS_TOUCH_X_H_POS                   3
#define FTS_TOUCH_X_L_POS                   4
#define FTS_TOUCH_Y_H_POS                   5
#define FTS_TOUCH_Y_L_POS                   6
#define FTS_TOUCH_PRE_POS                   7
#define FTS_TOUCH_AREA_POS                  8
#define FTS_TOUCH_POINT_NUM                 2
#define FTS_TOUCH_EVENT_POS                 3
#define FTS_TOUCH_ID_POS                    5
#define FTS_COORDS_ARR_SIZE                 4

#define FTS_TOUCH_DOWN                      0
#define FTS_TOUCH_UP                        1
#define FTS_TOUCH_CONTACT                   2
#define EVENT_DOWN(flag)                    ((FTS_TOUCH_DOWN == flag) || (FTS_TOUCH_CONTACT == flag))
#define EVENT_UP(flag)                      (FTS_TOUCH_UP == flag)
#define EVENT_NO_DOWN(data)                 (!data->point_num)
#define KEY_EN(data)                        (data->pdata->have_key)
#define TOUCH_IS_KEY(y, key_y)              (y == key_y)
#define TOUCH_IN_RANGE(val, key_val, half)  ((val > (key_val - half)) && (val < (key_val + half)))
#define TOUCH_IN_KEY(x, key_x)              TOUCH_IN_RANGE(x, key_x, FTS_KEY_WIDTH)

#define FTS_LOCKDOWN_INFO_SIZE				8
/*****************************************************************************
* Private enumerations, structures and unions using typedef
*****************************************************************************/
struct fts_ts_platform_data {
	u32 irq_gpio;
	u32 irq_gpio_flags;
	u32 reset_gpio;
	u32 reset_gpio_flags;
	bool have_key;
	u32 key_number;
	u32 keys[FTS_MAX_KEYS];
	u32 key_y_coord;
	u32 key_x_coords[FTS_MAX_KEYS];
	u32 x_max;
	u32 y_max;
	u32 x_min;
	u32 y_min;
	u32 max_touch_number;
	u32 timeout_read_reg;
	const char *project_name;
	u32 open_min;
	bool reset_when_resume;
	u32 lockdown_info_addr;
	bool check_display_name;
	bool cutoff_power;
};

struct ts_event {
	int x;			/*x coordinate */
	int y;			/*y coordinate */
	int p;			/* pressure */
	int flag;		/* touch event flag: 0 -- down; 1-- up; 2 -- contact */
	int id;			/*touch ID */
	int area;
};

struct fts_ts_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct fts_ts_platform_data *pdata;
	struct ts_ic_info ic_info;
	struct workqueue_struct *ts_workqueue;
	struct work_struct fwupg_work;
	struct delayed_work esdcheck_work;
	struct delayed_work prc_work;
	struct regulator *vsp;
	struct regulator *vsn;
	struct regulator *vddio;
	struct regulator *avdd;
	spinlock_t irq_lock;
	struct mutex report_mutex;
	int irq;
	bool suspended;
	bool fw_loading;
	bool irq_disabled;
#if FTS_POWER_SOURCE_CUST_EN
	bool power_disabled;
#endif
	/* multi-touch */
	struct ts_event *events;
	u8 *point_buf;
	int pnt_buf_size;
	int touchs;
	bool key_down;
	int touch_point;
	int point_num;
	int fw_ver_in_host;
	int fw_ver_in_tp;
	short chipid;
	struct proc_dir_entry *proc;
	u8 proc_opmode;
	u8 lockdown_info[FTS_LOCKDOWN_INFO_SIZE];
	bool dev_pm_suspend;
	bool lpwg_mode;
	bool fw_forceupdate;
	struct work_struct suspend_work;
	struct work_struct resume_work;
#ifdef CONFIG_TOUCHSCREEN_FTS_POWER_SUPPLY
	struct work_struct power_supply_work;
	int is_usb_exist;
#endif
	struct workqueue_struct *event_wq;
	struct completion dev_pm_suspend_completion;
#if FTS_PINCTRL_EN
	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_active;
	struct pinctrl_state *pins_suspend;
	struct pinctrl_state *pins_release;
#endif
#ifdef CONFIG_DRM
	struct notifier_block fb_notif;
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif
#ifdef CONFIG_TOUCHSCREEN_FTS_POWER_SUPPLY
	struct notifier_block power_supply_notifier;
#endif
	struct dentry *debugfs;
	struct proc_dir_entry *tp_selftest_proc;
	struct proc_dir_entry *tp_data_dump_proc;
	struct proc_dir_entry *tp_fw_version_proc;
	struct proc_dir_entry *tp_lockdown_info_proc;
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE
	bool palm_sensor_switch;
	bool palm_sensor_changed;
	struct class *tp_class;
	bool gamemode_enabled;
#endif

};

struct fts_mode_switch {
	struct fts_ts_data *ts_data;
	unsigned char mode;
	struct work_struct switch_mode_work;
};

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/
extern struct fts_ts_data *fts_data;

/* i2c communication*/
int fts_i2c_write_reg(struct i2c_client *client, u8 regaddr, u8 regvalue);
int fts_i2c_read_reg(struct i2c_client *client, u8 regaddr, u8 *regvalue);
int fts_i2c_read(struct i2c_client *client, char *writebuf, int writelen, char *readbuf, int readlen);
int fts_i2c_write(struct i2c_client *client, char *writebuf, int writelen);
void fts_i2c_hid2std(struct i2c_client *client);
int fts_i2c_init(void);
int fts_i2c_exit(void);

/* Gesture functions */
#if FTS_GESTURE_EN
int fts_gesture_init(struct fts_ts_data *ts_data);
int fts_gesture_exit(struct i2c_client *client);
void fts_gesture_recovery(struct i2c_client *client);
int fts_gesture_readdata(struct fts_ts_data *ts_data);
int fts_gesture_suspend(struct i2c_client *i2c_client);
int fts_gesture_resume(struct i2c_client *client);
#endif

/* Apk and functions */
#if FTS_APK_NODE_EN
int fts_create_apk_debug_channel(struct fts_ts_data *);
void fts_release_apk_debug_channel(struct fts_ts_data *);
#endif

/* ADB functions */
#if FTS_SYSFS_NODE_EN
int fts_create_sysfs(struct i2c_client *client);
int fts_remove_sysfs(struct i2c_client *client);
#endif

/* ESD */
#if FTS_ESDCHECK_EN
int fts_esdcheck_init(struct fts_ts_data *ts_data);
int fts_esdcheck_exit(struct fts_ts_data *ts_data);
int fts_esdcheck_switch(bool enable);
int fts_esdcheck_proc_busy(bool proc_debug);
int fts_esdcheck_set_intr(bool intr);
int fts_esdcheck_suspend(void);
int fts_esdcheck_resume(void);
#endif

/* Production test */
#if FTS_TEST_EN
int fts_test_init(struct i2c_client *client);
int fts_test_exit(struct i2c_client *client);
#endif

/* Point Report Check*/
#if FTS_POINT_REPORT_CHECK_EN
int fts_point_report_check_init(struct fts_ts_data *ts_data);
int fts_point_report_check_exit(struct fts_ts_data *ts_data);
void fts_prc_queue_work(struct fts_ts_data *ts_data);
#endif

/* FW upgrade */
int fts_upgrade_bin(struct i2c_client *client, char *fw_name, bool force);
int fts_fwupg_init(struct fts_ts_data *ts_data);
int fts_fwupg_exit(struct fts_ts_data *ts_data);

/* Other */
int fts_reset_proc(int hdelayms);
int fts_wait_tp_to_valid(struct i2c_client *client);
void fts_tp_state_recovery(struct i2c_client *client);
int fts_ex_mode_init(struct i2c_client *client);
int fts_ex_mode_exit(struct i2c_client *client);
int fts_ex_mode_recovery(struct i2c_client *client);

void fts_irq_disable(void);
void fts_irq_enable(void);

int fts_flash_read(struct i2c_client *client, u32 addr, u8 *buf, u32 len);
int fts_flash_read_buf(struct i2c_client *client, u32 saddr, u8 *buf, u32 len);
void fts_gesture_enable(bool enable);

#endif /* __LINUX_FOCALTECH_CORE_H__ */
