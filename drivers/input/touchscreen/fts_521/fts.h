/*
 * fts.c
 *
 * FTS Capacitive touch screen controller (FingerTipS)
 *
 * Copyright (C) 2017, STMicroelectronics
 * Copyright (C) 2019 XiaoMi, Inc.
 * Authors: AMG(Analog Mems Group)
 *
 * 		marco.cali@st.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THE PRESENT SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES
 * OR CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, FOR THE SOLE
 * PURPOSE TO SUPPORT YOUR APPLICATION DEVELOPMENT.
 * AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY DIRECT,
 * INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING FROM THE
 * CONTENT OF SUCH SOFTWARE AND/OR THE USE MADE BY CUSTOMERS OF THE CODING
 * INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
 *
 * THIS SOFTWARE IS SPECIFICALLY DESIGNED FOR EXCLUSIVE USE WITH ST PARTS.
 */

/*!
* \file fts.h
* \brief Contains all the definitions and structs used generally by the driver
*/

#ifndef _LINUX_FTS_I2C_H_
#define _LINUX_FTS_I2C_H_

#include <linux/workqueue.h>
#include <linux/sysfs.h>
#include <linux/notifier.h>
#include <linux/mutex.h>
#include "fts_lib/ftsSoftware.h"
#include "fts_lib/ftsHardware.h"

/****************** CONFIGURATION SECTION ******************/
/** @defgroup conf_section	 Driver Configuration Section
* Settings of the driver code in order to suit the HW set up and the application behavior
* @{
*/
/**** CODE CONFIGURATION ****/
#define FTS_TS_DRV_NAME                     "fts"			/*driver name*/
#define FTS_TS_DRV_VERSION                  "5.2.4"			/*driver version string format*/
#define FTS_TS_DRV_VER						0x05020400		/*driver version u32 format*/

#define PINCTRL_STATE_ACTIVE		"pmx_ts_active"
#define PINCTRL_STATE_SUSPEND		"pmx_ts_suspend"
#define PINCTRL_STATE_RELEASE		"pmx_ts_release"

#define DEBUG

#define DRIVER_TEST

#define PRE_SAVED_METHOD

/*#define FW_H_FILE*/
#ifdef FW_H_FILE
#define FW_SIZE_NAME myArray_size
#define FW_ARRAY_NAME myArray
/*#define FW_UPDATE_ON_PROBE*/
#endif

#ifndef FW_UPDATE_ON_PROBE
/*#define LIMITS_H_FILE*/
#ifdef LIMITS_H_FILE
#define LIMITS_SIZE_NAME myArray2_size
#define LIMITS_ARRAY_NAME myArray2
#endif
#else
#define LIMITS_H_FILE
#define LIMITS_SIZE_NAME myArray2_size
#define LIMITS_ARRAY_NAME myArray2
#endif

/*#define USE_ONE_FILE_NODE*/

#ifndef FW_UPDATE_ON_PROBE
#define EXP_FN_WORK_DELAY_MS				1000
#endif

/**** END ****/

/**** FEATURES USED IN THE IC ****/

/*#define PHONE_KEY*/

#define GESTURE_MODE
#ifdef GESTURE_MODE
#define USE_GESTURE_MASK
#endif

#define CHARGER_MODE

#define GLOVE_MODE

#define COVER_MODE

#define STYLUS_MODE

#define GRIP_MODE

/**** END ****/

/**** PANEL SPECIFICATION ****/
#define X_AXIS_MAX                          1080
#define X_AXIS_MIN                          0
#define Y_AXIS_MAX                          2244
#define Y_AXIS_MIN                          0

#define PRESSURE_MIN                        0
#define PRESSURE_MAX                        127

#define DISTANCE_MIN						0
#define DISTANCE_MAX						127

#define TOUCH_ID_MAX                        10

#define AREA_MIN                            PRESSURE_MIN
#define AREA_MAX                            PRESSURE_MAX
/**** END ****/
/**@}*/
/*********************************************************/

/*
 * Configuration mode
 *
 * bitmask which can assume the value defined as features in ftsSoftware.h or the following values
 */

/** @defgroup mode_section	 IC Status Mode
* Bitmask which keeps track of the features and working mode enabled in the IC.
* The meaning of the the LSB of the bitmask must be interpreted considering that the value defined in @link feat_opt Feature Selection Option @endlink correspond to the position of the corresponding bit in the mask
* @{
*/
#define MODE_NOTHING						0x00000000
#define MODE_ACTIVE(_mask, _sett)           _mask |= (SCAN_MODE_ACTIVE<<24)|(_sett<<16)
#define MODE_LOW_POWER(_mask, _sett)        _mask |= (SCAN_MODE_LOW_POWER<<24)|(_sett<<16)
/** @}*/

#define CMD_STR_LEN							32

#define TSP_BUF_SIZE						PAGE_SIZE

#define CONFIG_FTS_TOUCH_COUNT_DUMP

#ifdef CONFIG_FTS_TOUCH_COUNT_DUMP
#define TOUCH_COUNT_FILE_MAXSIZE 50
#endif

/**
 * Struct which contains information about the HW platform and set up
 */
#define FTS_LOCKDOWN_SIZE 8
#define FTS_RESULT_INVALID 0
#define FTS_RESULT_PASS 2
#define FTS_RESULT_FAIL 1

struct fts_config_info {
	u8 tp_vendor;
	u8 tp_color;
	u8 tp_hw_version;
	const char *fts_cfg_name;
	const char *fts_limit_name;
#ifdef CONFIG_FTS_TOUCH_COUNT_DUMP
		const char *clicknum_file_name;
#endif
};

struct fts_hw_platform_data {
	int (*power) (bool on);
	int irq_gpio;
	int reset_gpio;
	unsigned long irq_flags;
	unsigned int x_max;
	unsigned int y_max;
	const char *vdd_reg_name;
	const char *avdd_reg_name;
	size_t config_array_size;
	struct fts_config_info *config_array;
	int current_index;
#ifdef PHONE_KEY
	size_t nbuttons;
	int *key_code;
#endif
#ifdef CONFIG_FTS_TOUCH_COUNT_DUMP
	bool dump_click_count;
#endif
	unsigned long keystates;
};

/*
 * Forward declaration
 */
struct fts_ts_info;
extern char tag[8];

/*
 * Dispatch event handler
 */
typedef void (*event_dispatch_handler_t)
 (struct fts_ts_info *info, unsigned char *data);

/**
 * FTS capacitive touch screen device information
 * - dev             Pointer to the structure device \n
 * - client          client structure \n
 * - input_dev       Input device structure \n
 * - work            Work thread \n
 * - event_wq        Event queue for work thread \n
 * - event_dispatch_table  Event dispatch table handlers \n
 * - attrs           SysFS attributes \n
 * - mode            Device operating mode (bitmask) \n
 * - touch_id        Bitmask for touch id (mapped to input slots) \n
 * - stylus_id       Bitmask for tracking the stylus touches (mapped using the touchId) \n
 * - timer           Timer when operating in polling mode \n
 * - power           Power on/off routine \n
 * - board           HW info retrieved from device tree \n
 * - vdd_reg         DVDD power regulator \n
 * - avdd_reg        AVDD power regulator \n
 * - resume_bit      Indicate if screen off/on \n
 * - fwupdate_stat   Store the result of a fw update triggered by the host \n
 * - notifier        Used for be notified from a suspend/resume event \n
 * - sensor_sleep    true suspend was called, false resume was called \n
 * - wakelock        Wake Lock struct \n
 * - input_report_mutex  mutex for handling the pressure of keys \n
 * - series_of_switches  to store the enabling status of a particular feature from the host \n
 */
struct fts_ts_info {
	struct device *dev;
#ifdef I2C_INTERFACE
	struct i2c_client *client;
#else
	struct spi_device *client;
#endif
	struct input_dev *input_dev;

	struct work_struct work;
	struct work_struct suspend_work;
	struct work_struct resume_work;
	struct workqueue_struct *event_wq;

#ifndef FW_UPDATE_ON_PROBE
	struct delayed_work fwu_work;
	struct workqueue_struct *fwu_workqueue;
#endif
	event_dispatch_handler_t *event_dispatch_table;

	struct attribute_group attrs;

	unsigned int mode;
	unsigned long touch_id;
	unsigned long sleep_finger;
	unsigned long touch_skip;
	int coor[TOUCH_ID_MAX][2];
#ifdef STYLUS_MODE
	unsigned long stylus_id;
#endif
	struct fts_hw_platform_data *board;
	struct regulator *vdd_reg;
	struct regulator *avdd_reg;

	int resume_bit;
	int fwupdate_stat;

	struct notifier_block notifier;
	bool sensor_sleep;
	struct pinctrl *ts_pinctrl;
	struct pinctrl_state *pinctrl_state_active;
	struct pinctrl_state *pinctrl_state_suspend;
	u8 lockdown_info[FTS_LOCKDOWN_SIZE];
	int result_type;
	struct proc_dir_entry *tp_selftest_proc;
	struct proc_dir_entry *tp_data_dump_proc;
	struct proc_dir_entry *tp_fw_version_proc;
	struct proc_dir_entry *tp_lockdown_info_proc;

	/* input lock */
	struct mutex input_report_mutex;

	int gesture_enabled;
	int glove_enabled;
	int charger_enabled;
	int stylus_enabled;
	int cover_enabled;
	int grip_enabled;
#ifdef CONFIG_TOUCHSCREEN_ST_DEBUG_FS
	struct dentry *debugfs;
#endif
	int dbclick_count;
#ifdef CONFIG_FTS_TOUCH_COUNT_DUMP
	struct class *fts_tp_class;
	struct device *fts_touch_dev;
	char *current_clicknum_file;
#endif
	bool irq_status;
	wait_queue_head_t 	wait_queue;
	bool p_sensor_changed;
	bool p_sensor_switch;
	bool palm_sensor_changed;
	bool palm_sensor_switch;
};

struct fts_mode_switch {
	struct fts_ts_info *info;
	unsigned char mode;
	struct work_struct switch_mode_work;
};

int fts_chip_powercycle(struct fts_ts_info *info);
extern int input_register_notifier_client(struct notifier_block *nb);
extern int input_unregister_notifier_client(struct notifier_block *nb);

extern int fts_proc_init(void);
extern int fts_proc_remove(void);

#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE
int fts_palm_sensor_cmd(int input);
int fts_p_sensor_cmd(int input);
bool fts_touchmode_edgefilter(unsigned int touch_id, int x, int y);
#endif
#endif
