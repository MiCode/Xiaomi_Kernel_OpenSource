/*
 * fts.c
 *
 * FTS Capacitive touch screen controller (FingerTipS)
 *
 * Copyright (C) 2017, STMicroelectronics
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
#include <linux/completion.h>
/****************** CONFIGURATION SECTION ******************/
/** @defgroup conf_section	 Driver Configuration Section
* Settings of the driver code in order to suit the HW set up and the application behavior
* @{
*/

/**** CODE CONFIGURATION ****/
#define FTS_TS_DRV_NAME                     "fts"			/*driver name*/
#define FTS_TS_DRV_VERSION                  "5.2.4.1"			/*driver version string format*/
#define FTS_TS_DRV_VER						0x05020401		/*driver version u32 format*/

#define PINCTRL_STATE_ACTIVE		"pmx_ts_active"
#define PINCTRL_STATE_SUSPEND		"pmx_ts_suspend"
#define PINCTRL_STATE_RELEASE		"pmx_ts_release"

/*** save power mode ***/
#define FTS_POWER_SAVE_MODE

#define TOUCH_THP_SUPPORT
#define TOUCH_THP_FW

#define DRIVER_TEST

#define PRE_SAVED_METHOD

/*#define FW_H_FILE*/
#define FTS_FW_UPDATE
#define FW_UPDATE_ON_PROBE
#ifdef FW_H_FILE
#define FW_SIZE_NAME myArray_size
#define FW_ARRAY_NAME myArray
#endif

/*#define LIMITS_H_FILE*/
#ifdef LIMITS_H_FILE
#define LIMITS_SIZE_NAME myArray2_size
#define LIMITS_ARRAY_NAME myArray2
#endif

#define FTS_XIAOMI_TOUCHFEATURE
#define FTS_FOD_AREA_REPORT
#define FTS_DEBUG_FS
#define CONFIG_I2C_BY_DMA 1
#define DEBUG

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



/**** END ****/

/**** PANEL SPECIFICATION ****/
#define X_AXIS_MAX                          1080
#define X_AXIS_MIN                          0
#define Y_AXIS_MAX                          2340
#define Y_AXIS_MIN                          0

#define PRESSURE_MIN                        0
#ifdef CONFIG_INPUT_PRESS_NDT
#define PRESSURE_MAX                        2048
#else
#define PRESSURE_MAX                        127
#endif

#define DISTANCE_MIN						0
#define DISTANCE_MAX						127

#define TOUCH_ID_MAX                        10

#define AREA_MIN                            PRESSURE_MIN
#define AREA_MAX                            PRESSURE_MAX
#define TXNODE_MAX							40
#define RXNODE_MAX							40
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
#define MODE_ACTIVE(_mask, _sett)\
do {\
	_mask |= (SCAN_MODE_ACTIVE << 24)|(_sett << 16);\
} while (0)
#define MODE_LOW_POWER(_mask, _sett)\
do {\
	_mask |= (SCAN_MODE_LOW_POWER << 24)|(_sett << 16);\
} while (0)
/** @}*/

#define CMD_STR_LEN							32

#define TSP_BUF_SIZE						PAGE_SIZE


/**
 * Struct which contains information about the HW platform and set up
 */
#define FTS_LOCKDOWN_SIZE 8
#define FTS_RESULT_INVALID 0
#define FTS_RESULT_PASS 2
#define FTS_RESULT_FAIL 1
#define FTS_SELFTEST_FORCE_CAL

#define GRIP_MODE_DEBUG
#define GRIP_RECT_NUM 12
#define GRIP_PARAMETER_NUM 8
#define EXPERT_ARRAY_SIZE 3

/*#define CONFIG_FTS_POWERSUPPLY_CB*/
enum charge_status {
	NOT_CHARGING,
	CHARGING,
	WIRED_CHARGING,
	WIRELESS_CHARGING,
};

struct fts_config_info {
	u8 tp_vendor;
	u8 tp_color;
	u8 tp_hw_version;
	u8 tp_module;
	const char *fts_cfg_name;
	const char *fts_limit_name;
};

struct fts_hw_platform_data {
	int (*power) (bool on);
	int irq_gpio;
	int reset_gpio;
	int avdd_gpio;
	unsigned long irq_flags;
	unsigned int x_max;
	unsigned int y_max;
	const char *vdd_reg_name;
	const char *avdd_reg_name;
	const char *default_fw_name;
	const char *thp_fw_name;
	size_t config_array_size;
	struct fts_config_info *config_array;
	int current_index;
#ifdef PHONE_KEY
	size_t nbuttons;
	int *key_code;
#endif
	unsigned long keystates;
	bool swap_x;
	bool swap_y;
	unsigned int fod_lx;
	unsigned int fod_ly;
	unsigned int fod_x_size;
	unsigned int fod_y_size;
#ifdef FTS_XIAOMI_TOUCHFEATURE
	u32 touch_follow_per_def;
	u32 touch_tap_sensitivity_def;
	u32 touch_aim_sensitivity_def;
	u32 touch_tap_stability_def;
	u32 cornerfilter_area_step1;
	u32 cornerfilter_area_step2;
	u32 cornerfilter_area_step3;
	u32 non_curved_display;
	u32 support_super_resolution;
	u32 deadzone_filter_ver[4 * GRIP_PARAMETER_NUM];
	u32 deadzone_filter_hor[4 * GRIP_PARAMETER_NUM];
	u32 edgezone_filter_ver[4 * GRIP_PARAMETER_NUM];
	u32 edgezone_filter_hor[4 * GRIP_PARAMETER_NUM];
	u32 cornerzone_filter_ver[4 * GRIP_PARAMETER_NUM];
	u32 cornerzone_filter_hor1[4 * GRIP_PARAMETER_NUM];
	u32 cornerzone_filter_hor2[4 * GRIP_PARAMETER_NUM];
	u32 normal_deadzone_filter_hor[4 * GRIP_PARAMETER_NUM];
	u32 normal_edgezone_filter_hor[4 * GRIP_PARAMETER_NUM];
	u32 normal_cornerzone_filter_hor1[4 * GRIP_PARAMETER_NUM];
	u32 normal_cornerzone_filter_hor2[4 * GRIP_PARAMETER_NUM];
	u32 touch_follow_performance[3 * 5];
	u32 touch_tap_sensitivity[5];
	u32 touch_aim_sensitivity[5];
	u32 touch_tap_stability[5];
	u32 touch_expert_array[6 * EXPERT_ARRAY_SIZE];
#endif
	bool support_fod;
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

#ifdef CONFIG_SECURE_TOUCH
struct fts_secure_delay {
	bool palm_pending;
	int palm_value;
};

struct fts_secure_info {
	bool secure_inited;
	atomic_t st_1st_complete;
	atomic_t st_enabled;
	atomic_t st_pending_irqs;
	struct completion st_irq_processed;
	struct completion st_powerdown;
	struct fts_secure_delay scr_delay;
	struct mutex palm_lock;
	void *fts_info;
};
#endif

#ifdef CONFIG_I2C_BY_DMA
struct fts_dma_buf {
	struct mutex dmaBufLock;
	u8 *rdBuf;
	u8 *wrBuf;
};
#endif

struct tp_frame {
	s64 time_ns;
	u64 frm_cnt;
	char thp_frame_buf[PAGE_SIZE];
};
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
	struct work_struct cmd_update_work;
	struct work_struct sleep_work;
	struct workqueue_struct *event_wq;
	struct workqueue_struct *irq_wq;
	struct workqueue_struct *touch_feature_wq;

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
#ifdef STYLUS_MODE
	unsigned long stylus_id;
#endif
	struct fts_hw_platform_data *board;
	struct regulator *vdd_reg;
	struct regulator *avdd_reg;
	struct regulator *avddold_reg;

	int resume_bit;
	int fwupdate_stat;

	struct notifier_block notifier;
	struct notifier_block bl_notifier;
	void *notifier_cookie;
	bool sensor_sleep;
	bool sensor_scan;
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
	unsigned int grip_enabled;
	unsigned int grip_pixel;
	unsigned int doze_time;
	unsigned int grip_pixel_def;
	unsigned int doze_time_def;
#ifdef FTS_DEBUG_FS
	struct dentry *debugfs;
#endif
	struct class *fts_tp_class;
	struct device *fts_touch_dev;
#ifdef CONFIG_SECURE_TOUCH
	struct fts_secure_info *secure_info;
#endif
#ifdef CONFIG_I2C_BY_DMA
	struct fts_dma_buf *dma_buf;
#endif
	bool lockdown_is_ok;
	bool irq_status;
	wait_queue_head_t 	wait_queue;
	struct completion tp_reset_completion;
	atomic_t system_is_resetting;
	int fod_status;
	unsigned int fod_overlap;
	unsigned long fod_id;
	unsigned long fod_x;
	unsigned long fod_y;
	struct mutex fod_mutex;
	struct mutex cmd_update_mutex;
	bool fod_coordinate_update;
	bool fod_pressed;
	bool prox_sensor_changed;
	bool prox_sensor_switch;
	bool palm_sensor_changed;
	bool palm_sensor_switch;
	bool enable_touch_raw;
	bool enable_touch_delta;
	bool enable_thp_fw;
	int clicktouch_count;
	int clicktouch_num;
	char *data_dump_buf;
	short strength_buf[PAGE_SIZE];
	struct tp_frame thp_frame;
	int aod_status;
	bool tp_pm_suspend;
	struct completion pm_resume_completion;
	bool gamemode_enable;
	int width_major;
	int width_minor;
	int orientation;
	int last_x[TOUCH_ID_MAX];
	int last_y[TOUCH_ID_MAX];
	struct work_struct switch_mode_work;
	struct work_struct grip_mode_work;
	bool big_area_fod;
	struct delayed_work power_supply_work;
	int charging_status;
	struct notifier_block power_supply_notifier;
	bool probe_ok;
	struct mutex charge_lock;
	int fod_icon_status;
	int nonui_status;
	bool fod_down;
};

extern int fts_chip_powercycle(struct fts_ts_info *info);
extern int input_register_notifier_client(struct notifier_block *nb);
extern int input_unregister_notifier_client(struct notifier_block *nb);

extern int fts_proc_init(void);
extern int fts_proc_remove(void);
#ifdef FTS_FOD_AREA_REPORT
#define CENTER_X 540
#define CENTER_Y 1910
bool fts_is_infod(void);
#endif
void fts_restore_regvalues(void);
const char *fts_get_limit(struct fts_ts_info *info);
#ifdef FTS_XIAOMI_TOUCHFEATURE
int fts_palm_sensor_cmd(int input);
int fts_prox_sensor_cmd(int input);
bool fts_touchmode_edgefilter(unsigned int touch_id, int x, int y);
#endif
#endif
