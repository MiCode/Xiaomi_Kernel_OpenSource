/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2012-2020, Focaltech Ltd. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
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
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/uaccess.h>
#include <linux/firmware.h>
#include <linux/debugfs.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/dma-mapping.h>
#include <linux/power_supply.h>
#include "focaltech_common.h"

/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/
#define FTS_MAX_POINTS_SUPPORT              10 /* constant value, can't be changed */
#define FTS_MAX_KEYS                        4
#define FTS_KEY_DIM                         10
#define FTS_ONE_TCH_LEN                     6
#define FTS_TOUCH_DATA_LEN  (FTS_MAX_POINTS_SUPPORT * FTS_ONE_TCH_LEN + 3)

#define FTS_GESTURE_POINTS_MAX              6
#define FTS_GESTURE_DATA_LEN               (FTS_GESTURE_POINTS_MAX * 4 + 4)

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
#define FTS_X_MIN_DISPLAY_DEFAULT           0
#define FTS_Y_MIN_DISPLAY_DEFAULT           0
#define FTS_X_MAX_DISPLAY_DEFAULT           720
#define FTS_Y_MAX_DISPLAY_DEFAULT           1280

#define FTS_TOUCH_DOWN                      0
#define FTS_TOUCH_UP                        1
#define FTS_TOUCH_CONTACT                   2
#define EVENT_DOWN(flag)                    ((FTS_TOUCH_DOWN == flag) || (FTS_TOUCH_CONTACT == flag))
#define EVENT_UP(flag)                      (FTS_TOUCH_UP == flag)
#define EVENT_NO_DOWN(data)                 (!data->point_num)

#define FTX_MAX_COMPATIBLE_TYPE             4
#define FTX_MAX_COMMMAND_LENGTH             16

#define FTS_TEST_OPEN_MIN                   3000
#define FTS_LOCKDOWN_INFO_ADDR              0x1F000
#define FTS_LOCKDOWN_INFO_SIZE              8

#define FTS_XIAOMI_TOUCHFEATURE

/*****************************************************************************
*  Alternative mode (When something goes wrong, the modules may be able to solve the problem.)
*****************************************************************************/
/*
 * For commnication error in PM(deep sleep) state
 */
#define FTS_PATCH_COMERR_PM                 1
#define FTS_TIMEOUT_COMERR_PM               700
#define SPI_CMD_BYTE                        4
#define SPI_CRC_BYTE                        2
#define SPI_DUMMY_BYTE                      3
#define SPI_HEADER_BYTE ((SPI_CMD_BYTE) + (SPI_DUMMY_BYTE) + (SPI_CRC_BYTE))
/*(FT5652 MAX 26)*/
#define HT_HAL_ROW_NUM                      16
/*(FT5652 MAX 42)*/
#define HT_HAL_COL_NUM                      35
#define HT_HAL_NODE_NUM                     ((HT_HAL_ROW_NUM) * (HT_HAL_COL_NUM))
#define HT_HAL_SNODE_NUM                    ((HT_HAL_ROW_NUM) + (HT_HAL_COL_NUM))
#define HT_CMD_GET_FRAME                    0x3A

#define FTS_DIFF_DATA_LEN          HT_HAL_NODE_NUM * 2 + 1

#define PANEL_ORIENTATION_DEGREE_0			0	/* normal portrait orientation */
#define PANEL_ORIENTATION_DEGREE_90			1	/* anticlockwise 90 degrees */
#define PANEL_ORIENTATION_DEGREE_180		2	/* anticlockwise 180 degrees */
#define PANEL_ORIENTATION_DEGREE_270		3	/* anticlockwise 270 degrees */

struct tp_raw {
	uint16_t frm_idx;
	uint16_t scan_freq;
	uint16_t report_rate;
	uint16_t tp_status;
	uint16_t tp_noise;
	uint16_t frm_id;
	uint16_t dbg_info[8];
	uint16_t main_raw[HT_HAL_NODE_NUM];
	uint16_t scap1_raw[HT_HAL_SNODE_NUM];
	uint16_t scap2_raw[HT_HAL_SNODE_NUM];
	uint16_t frm_confirm_idx;
	uint16_t ecc;
};

struct tp_frame {
	struct timeval tv0;
	struct timeval tv;
	char tp_raw[sizeof(struct tp_raw)];
};

/*****************************************************************************
* Private enumerations, structures and unions using typedef
*****************************************************************************/
struct ftxxxx_proc {
	struct proc_dir_entry *proc_entry;
	struct proc_dir_entry *tp_lockdown_info_proc;
	struct proc_dir_entry *tp_fw_version_proc;
	struct proc_dir_entry *tp_test_data_proc;
	struct proc_dir_entry *tp_test_result_proc;
	struct proc_dir_entry *tp_selftest_proc;
	struct proc_dir_entry *tp_data_dump_proc;
	u8 opmode;
	u8 cmd_len;
	u8 cmd[FTX_MAX_COMMMAND_LENGTH];
};

#define FOCAL_MAX_STR_LABLE_LEN		32
struct fts_ts_platform_data {
	char avdd_name[FOCAL_MAX_STR_LABLE_LEN];
	char iovdd_name[FOCAL_MAX_STR_LABLE_LEN];
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
	u32 open_min;
};

struct ts_event {
	int x;      /*x coordinate */
	int y;      /*y coordinate */
	int p;      /* pressure */
	int flag;   /* touch event flag: 0 -- down; 1-- up; 2 -- contact */
	int id;     /*touch ID */
	int area;
};

struct fts_hw_info {
	uint8_t row_num;
	uint8_t col_num;
	uint8_t scap1_num;
	uint8_t scap2_num;
	uint16_t pitch_size;
	uint8_t scan_freq_num;
	uint16_t *scan_freq_val;
	uint8_t report_rate_num;
	uint16_t *report_rate_val;
} ;

struct fts_ts_data {
	struct i2c_client *client;
	struct spi_device *spi;
	struct device *dev;
	struct input_dev *input_dev;
	struct fts_ts_platform_data *pdata;
	struct fts_hw_info *hw_info;
	struct ts_ic_info ic_info;
	struct workqueue_struct *ts_workqueue;
	struct work_struct fwupg_work;
	struct delayed_work esdcheck_work;
	struct delayed_work prc_work;
	struct work_struct resume_work;
	struct ftxxxx_proc proc;
	spinlock_t irq_lock;
	struct mutex report_mutex;
	struct mutex bus_lock;
	struct tp_frame tp_frame;
	int irq;
	int log_level;
	int fw_is_running;      /* confirm fw is running when using spi:default 0 */
	int dummy_byte;
	bool enable_touch_raw;
#if defined(CONFIG_PM) && FTS_PATCH_COMERR_PM
	struct completion pm_completion;
	bool pm_suspend;
#endif
	bool suspended;
	bool fw_loading;
	bool irq_disabled;
	bool power_disabled;
	bool glove_mode;
	bool cover_mode;
	bool charger_mode;
	bool gesture_mode;      /* gesture enable or disable, default: disable */
	/* multi-touch */
	struct ts_event *events;
	u8 *bus_tx_buf;
	u8 *bus_rx_buf;
	int bus_type;
	u8 *point_buf;
	int pnt_buf_size;
	int touchs;
	int key_state;
	int touch_point;
	int point_num;
	struct regulator *vdd;
	struct regulator *vcc_i2c;
	u8 lockdown_info[FTS_LOCKDOWN_INFO_SIZE];
#if FTS_PINCTRL_EN
	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_active;
	struct pinctrl_state *pins_suspend;
	struct pinctrl_state *pins_release;
#endif
#if defined(CONFIG_DRM) || defined(CONFIG_FB)
	struct notifier_block fb_notif;
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif
	struct dentry *tpdbg_dentry;
	bool poweroff_on_sleep;
	u8 gesture_status;
	struct mutex cmd_update_mutex;
	int palm_sensor_switch;
	struct work_struct power_supply_work;
	struct notifier_block power_supply_notifier;
	bool is_usb_exist;
	int clicktouch_count;
	int clicktouch_num;
	bool clicktouch_enable;
	u8 fps_cmd;
	u8 fps_state;
};

enum GESTURE_MODE_TYPE {
	GESTURE_DOUBLETAP,
	GESTURE_AOD,
	GESTURE_FOD,
};

enum _FTS_BUS_TYPE {
	BUS_TYPE_NONE,
	BUS_TYPE_I2C,
	BUS_TYPE_SPI,
	BUS_TYPE_SPI_V2,
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
int fts_spi_transfer(u8 *tx_buf, u8 *rx_buf, u32 len);
int rdata_check(u8 *rdata, u32 rlen);

void fts_hid2std(void);
int fts_bus_init(struct fts_ts_data *ts_data);
int fts_bus_exit(struct fts_ts_data *ts_data);

/* Gesture functions */
int fts_gesture_init(struct fts_ts_data *ts_data);
int fts_gesture_exit(struct fts_ts_data *ts_data);
void fts_gesture_recovery(struct fts_ts_data *ts_data);
int fts_gesture_readdata(struct fts_ts_data *ts_data, u8 *data);
int fts_gesture_suspend(struct fts_ts_data *ts_data);
int fts_gesture_resume(struct fts_ts_data *ts_data);

/* Apk and functions */
int fts_create_proc(struct fts_ts_data *ts_data);
void fts_remove_proc(struct fts_ts_data *ts_data);

/* ADB functions */
int fts_create_sysfs(struct fts_ts_data *ts_data);
int fts_remove_sysfs(struct fts_ts_data *ts_data);

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
int fts_fwupg_by_name(char *fw_name);

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
#endif /* __LINUX_FOCALTECH_CORE_H__ */
