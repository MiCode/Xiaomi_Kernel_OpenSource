/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2012-2019, FocalTech Systems, Ltd., all rights reserved.
 */
/*****************************************************************************
 *
 * File Name: focaltech_core.h
 *
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
#include "focaltech_common.h"
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/firmware.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/regulator/consumer.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/time.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <linux/workqueue.h>

/*****************************************************************************
 * Private constant and macro definitions using #define
 *****************************************************************************/
#define FTS_MAX_POINTS_SUPPORT 10 /* constant value, can't be changed */
#define FTS_MAX_KEYS 4
#define FTS_KEY_DIM 10
#define FTS_ONE_TCH_LEN 6
#define FTS_TOUCH_DATA_LEN (FTS_MAX_POINTS_SUPPORT * FTS_ONE_TCH_LEN + 3)

#if (FTS_CHIP_TYPE == _FT8006P)
#define FTS_SPI_CLK_MAX 6000000
#else
#define FTS_SPI_CLK_MAX 10000000
#endif

#define FTS_GESTURE_POINTS_MAX 6
#define FTS_GESTURE_DATA_LEN (FTS_GESTURE_POINTS_MAX * 4 + 4)

#define FTS_MAX_ID 0x0A
#define FTS_TOUCH_X_H_POS 3
#define FTS_TOUCH_X_L_POS 4
#define FTS_TOUCH_Y_H_POS 5
#define FTS_TOUCH_Y_L_POS 6
#define FTS_TOUCH_PRE_POS 7
#define FTS_TOUCH_AREA_POS 8
#define FTS_TOUCH_POINT_NUM 2
#define FTS_TOUCH_EVENT_POS 3
#define FTS_TOUCH_ID_POS 5
#define FTS_COORDS_ARR_SIZE 4
#define FTS_X_MIN_DISPLAY_DEFAULT 0
#define FTS_Y_MIN_DISPLAY_DEFAULT 0
#define FTS_X_MAX_DISPLAY_DEFAULT 720
#define FTS_Y_MAX_DISPLAY_DEFAULT 1280

#define FTS_TOUCH_DOWN 0
#define FTS_TOUCH_UP 1
#define FTS_TOUCH_CONTACT 2
#define EVENT_DOWN(flag)                                                       \
	((flag == FTS_TOUCH_DOWN) || (flag == FTS_TOUCH_CONTACT))
#define EVENT_UP(flag) (flag == FTS_TOUCH_UP)
#define EVENT_NO_DOWN(data) (!data->point_num)

#define FTX_MAX_COMPATIBLE_TYPE 4
#define FTX_MAX_COMMMAND_LENGTH 16

/*****************************************************************************
 * Private enumerations, structures and unions using typedef
 *****************************************************************************/
struct ftxxxx_proc {
	struct proc_dir_entry *proc_entry;
	u8 opmode;
	u8 cmd_len;
	u8 cmd[FTX_MAX_COMMMAND_LENGTH];
};

struct fts_ts_platform_data {
	u32 irq_gpio;
	u32 irq_gpio_flags;
	u32 reset_gpio;
	u32 reset_gpio_flags;
	bool have_key;
	u32 key_number;
	u32 keys[FTS_MAX_KEYS];
	u32 key_y_coords[FTS_MAX_KEYS];
	u32 key_x_coords[FTS_MAX_KEYS];
	u32 x_max;
	u32 y_max;
	u32 x_min;
	u32 y_min;
	u32 max_touch_number;
};

struct ts_event {
	int x;    /*x coordinate */
	int y;    /*y coordinate */
	int p;    /* pressure */
	int flag; /* touch event flag: 0 -- down; 1-- up; 2 -- contact */
	int id;   /*touch ID */
	int area;
};

struct fts_ts_data {
	struct i2c_client *client;
	struct spi_device *spi;
	struct device *dev;
	struct input_dev *input_dev;
	struct fts_ts_platform_data *pdata;
	struct ts_ic_info ic_info;
	struct workqueue_struct *ts_workqueue;
	struct work_struct fwupg_work;
	struct delayed_work esdcheck_work;
	struct delayed_work prc_work;
	struct task_struct *thread_tpd;
	struct ftxxxx_proc proc;
	spinlock_t irq_lock;
	struct mutex report_mutex;
	struct mutex bus_lock;
	int irq;
	int log_level;
	int fw_is_running; /* confirm fw is running when using spi:default 0 */
	bool suspended;
	bool fw_loading;
	bool irq_disabled;
	bool power_disabled;
	bool glove_mode;
	bool cover_mode;
	bool charger_mode;
	/* multi-touch */
	struct ts_event *events;
	u8 *bus_buf;
	u8 *point_buf;
	int pnt_buf_size;
	int touchs;
	int key_state;
	int touch_point;
	int point_num;
	struct regulator *vdd;
	struct pinctrl *pinctrl;
	struct pinctrl_state *spi_default;
	struct pinctrl_state *spi_active;
};

/*****************************************************************************
 * Global variable or extern global variabls/functions
 *****************************************************************************/
extern struct fts_ts_data *fts_data;

/* communication interface */
int fts_read(u8 *cmd, u32 cmdlen, u8 *data, u32 datalen);
int fts_read_reg(u8 addr, u8 *value);
int fts_write(u8 *writebuf, u32 writelen);
int fts_write_reg(u8 addr, u8 value);
void fts_hid2std(void);
int fts_bus_init(struct fts_ts_data *ts_data);
int fts_bus_exit(struct fts_ts_data *ts_data);

/* Gesture functions */
#if FTS_GESTURE_EN
int fts_gesture_init(struct fts_ts_data *ts_data);
int fts_gesture_exit(struct fts_ts_data *ts_data);
void fts_gesture_recovery(struct fts_ts_data *ts_data);
int fts_gesture_readdata(struct fts_ts_data *ts_data, u8 *data);
int fts_gesture_suspend(struct fts_ts_data *ts_data);
int fts_gesture_resume(struct fts_ts_data *ts_data);
#endif

/* Apk and functions */
#if FTS_APK_NODE_EN
int fts_create_apk_debug_channel(struct fts_ts_data *ts_data);
void fts_release_apk_debug_channel(struct fts_ts_data *ts_data);
#endif

/* ADB functions */
#if FTS_SYSFS_NODE_EN
int fts_create_sysfs(struct fts_ts_data *ts_data);
int fts_remove_sysfs(struct fts_ts_data *ts_data);
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
int fts_test_init(struct fts_ts_data *ts_data);
int fts_test_exit(struct fts_ts_data *ts_data);
#endif

/* Point Report Check*/
#if FTS_POINT_REPORT_CHECK_EN
int fts_point_report_check_init(struct fts_ts_data *ts_data);
int fts_point_report_check_exit(struct fts_ts_data *ts_data);
void fts_prc_queue_work(struct fts_ts_data *ts_data);
#endif

/* FW upgrade */
int fts_fwupg_init(struct fts_ts_data *ts_data);
int fts_fwupg_exit(struct fts_ts_data *ts_data);
int fts_upgrade_bin(char *fw_name, bool force);
int fts_enter_test_environment(bool test_state);

/* Other */
int fts_reset_proc(int hdelayms);
int fts_wait_tp_to_valid(void);
void fts_release_all_finger(void);
void fts_tp_state_recovery(struct fts_ts_data *ts_data);
int fts_ex_mode_init(struct fts_ts_data *ts_data);
int fts_ex_mode_exit(struct fts_ts_data *ts_data);
int fts_ex_mode_recovery(struct fts_ts_data *ts_data);

void fts_irq_disable(void);
void fts_irq_enable(void);

#if FTS_PSENSOR_EN
int fts_proximity_init(void);
int fts_proximity_exit(void);
int fts_proximity_readdata(struct fts_ts_data *ts_data);
int fts_proximity_suspend(void);
int fts_proximity_resume(void);
int fts_proximity_recovery(struct fts_ts_data *ts_data);
#endif

#endif /* __LINUX_FOCALTECH_CORE_H__ */
