/*
 * fts.c
 *
 * FTS Capacitive touch screen controller (FingerTipS)
 *
 * Copyright (C) 2016, STMicroelectronics Limited.
 * Authors: AMG(Analog Mems Group)
 *
 *		marco.cali@st.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef _LINUX_FTS_I2C_H_
#define _LINUX_FTS_I2C_H_

#include "fts_lib/ftsSoftware.h"
#include "fts_lib/ftsHardware.h"

#define FTS_POWER_ON     1
#define FTS_POWER_OFF    0

#define PHONE_KEY
#define PHONE_PALM

#define PHONE_GESTURE

#define SCRIPTLESS

#define DRIVER_TEST

#ifdef FW_H_FILE
#define FW_SIZE_NAME myArray_size
#define FW_ARRAY_NAME myArray
#endif

#ifdef LIMITS_H_FILE
#define LIMITS_SIZE_NAME myArray2_size
#define LIMITS_ARRAY_NAME myArray2
#endif

#ifdef SCRIPTLESS
#endif


#define FTS_TS_DRV_NAME                     "fts"
#define FTS_TS_DRV_VERSION                  "4.1.2"


#define X_AXIS_MAX                          1080
#define X_AXIS_MIN                          0
#define Y_AXIS_MAX                          2160
#define Y_AXIS_MIN                          0

#define PRESSURE_MIN                        0
#define PRESSURE_MAX                        127

#define FINGER_MAX                          10
#define STYLUS_MAX                          1
#define TOUCH_ID_MAX                        (FINGER_MAX + STYLUS_MAX)

#define AREA_MIN                            PRESSURE_MIN
#define AREA_MAX                            PRESSURE_MAX

#define PINCTRL_STATE_ACTIVE		"pmx_ts_active"
#define PINCTRL_STATE_SUSPEND		"pmx_ts_suspend"
#define PINCTRL_STATE_RELEASE		"pmx_ts_release"
/*********************************************************/




/* Flash programming */

#define INIT_FLAG_CNT                       3

/* KEYS */
#define KEY1		0x02
#define KEY2		0x01
#define KEY3		0x04

/*
 * Configuration mode
 */
#define MODE_NORMAL	                        0
#define MODE_GESTURE				1
#define MODE_GLOVE				2
#define MODE_SENSEOFF				3

/*
 * Status Event Field:
 *     id of command that triggered the event
 */

#define FTS_FLASH_WRITE_CONFIG              0x03
#define FTS_FLASH_WRITE_COMP_MEMORY         0x04
#define FTS_FORCE_CAL_SELF_MUTUAL           0x05
#define FTS_FORCE_CAL_SELF                  0x06
#define FTS_WATER_MODE_ON                   0x07
#define FTS_WATER_MODE_OFF                  0x08


#define EXP_FN_WORK_DELAY_MS		1000

#define CMD_STR_LEN			32


#ifdef SCRIPTLESS
/*
 * I2C Command Read/Write Function
 */

#define CMD_RESULT_STR_LEN     2048
#endif

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
};
struct fts_i2c_platform_data {
	int (*power)(bool on);
	int irq_gpio;
	int reset_gpio;
	unsigned long irq_flags;
	const char *pwr_reg_name;
	const char *bus_reg_name;
	size_t config_array_size;
	struct fts_config_info *config_array;
	int current_index;
#ifdef EDGEHOVER_FOR_VOLUME
	bool side_volume;
	u32 y_base;
	u32 y_skip;
	u32 x_base;
	u32 y_offset;
#endif
#ifdef PHONE_KEY
	size_t nbuttons;
	int *key_code;
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
typedef unsigned char * (*event_dispatch_handler_t)(struct fts_ts_info *info, unsigned char *data);

/*
 * struct fts_ts_info - FTS capacitive touch screen device information
 * @dev:                  Pointer to the structure device
 * @client:               I2C client structure
 * @input_dev             Input device structure
 * @work                  Work thread
 * @event_wq              Event queue for work thread
 * @cmd_done              Asyncronous command notification
 * @event_dispatch_table  Event dispatch table handlers
 * @fw_version            Firmware version
 * @attrs                 SysFS attributes
 * @mode                  Device operating mode
 * @touch_id              Bitmask for touch id (mapped to input slots)
 * @buttons               Bitmask for buttons status
 * @timer                 Timer when operating in polling mode
 * @early_suspend         Structure for early suspend functions
 * @power                 Power on/off routine
 */
struct fts_ts_info {
	struct device            *dev;
	struct i2c_client        *client;
	struct input_dev         *input_dev;
	struct work_struct        work;
	struct workqueue_struct  *event_wq;
	struct delayed_work        fwu_work;
	struct workqueue_struct  *fwu_workqueue;
	struct completion         cmd_done;
	event_dispatch_handler_t *event_dispatch_table;
	unsigned int              fw_version;
	unsigned int              config_id;
	struct attribute_group    attrs;
	unsigned int              mode;
	unsigned long             touch_id;
	unsigned int              buttons;
#ifdef FTS_USE_POLLING_MODE
	struct hrtimer timer;
#endif
#ifdef SCRIPTLESS
	struct device             *i2c_cmd_dev;
	char cmd_read_result[CMD_RESULT_STR_LEN];
	char cmd_wr_result[CMD_RESULT_STR_LEN];
	char cmd_write_result[20];
#endif
#ifdef DRIVER_TEST
	struct device             *test_cmd_dev;
#endif
	int (*power)(bool on);
	struct fts_i2c_platform_data *bdata;
	struct regulator *pwr_reg;
	struct regulator *bus_reg;
	bool fw_force;
	int debug_enable;
	int resume_bit;
	int pre_resume_bit;
	int fwupdate_stat;
	int touch_debug;
	struct notifier_block notifier;
	bool sensor_sleep;
	bool stay_awake;
	struct mutex input_report_mutex;
	int gesture_enabled;
	int glove_enabled;
	struct pinctrl *ts_pinctrl;
	struct pinctrl_state *pinctrl_state_active;
	struct pinctrl_state *pinctrl_state_suspend;
	u8 lockdown_info[FTS_LOCKDOWN_SIZE];
	int result_type;
	struct proc_dir_entry *tp_selftest_proc;
	struct proc_dir_entry *tp_data_dump_proc;
	struct proc_dir_entry *tp_fw_version_proc;
	struct proc_dir_entry *tp_lockdown_info_proc;
#ifdef EDGEHOVER_FOR_VOLUME
	int volume_flag;
	bool judgment_mode;
	long long pre_doubletap_time;
	long long pre_judgment_time;
	int pre_y;
	int volume_type;
	int point[2];
	int point_index;
	int samp_interval;
	int slide_thr;
	int slide_thr_start;
	int samp_interval_start;
	int m_slide_thr;
	int m_samp_interval;
	int m_sampdura;
	int debug_abs;
	int doubletap_interval;
	int doubletap_distance;
	int single_press_time_low;
	int single_press_time_hi;
	long long down_time_ns;
#endif
#ifdef PHONE_PALM
	bool palm_enabled;
#endif
	struct work_struct        suspend_work;
	struct work_struct        resume_work;
#ifdef CONFIG_TOUCHSCREEN_ST_DEBUG_FS
	struct dentry *debugfs;
#endif
};

struct fts_mode_switch {
	struct fts_ts_info *info;
	unsigned char mode;
	struct work_struct switch_mode_work;
};
typedef enum {
	ERR_ITO_NO_ERR,
	ERR_ITO_PANEL_OPEN_FORCE,
	ERR_ITO_PANEL_OPEN_SENSE,
	ERR_ITO_F2G,
	ERR_ITO_S2G,
	ERR_ITO_F2VDD,
	ERR_ITO_S2VDD,
	ERR_ITO_P2P_FORCE,
	ERR_ITO_P2P_SENSE,
} errItoSubTypes_t;


static inline ssize_t fts_show_error(struct device *dev, struct device_attribute *attr, char *buf)
{
	dev_warn(dev, "%s Attempted to read from write-only attribute %s\n",
		__func__, attr->attr.name);
	return -EPERM;
}

static inline ssize_t fts_store_error(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	dev_warn(dev, "%s Attempted to write to read-only attribute %s\n",
		__func__, attr->attr.name);
	return -EPERM;
}

int fts_chip_powercycle(struct fts_ts_info *info);
int fts_get_fw_version(struct fts_ts_info *info);
extern unsigned int le_to_uint(const unsigned char *ptr);
extern unsigned int be_to_uint(const unsigned char *ptr);
extern int input_register_notifier_client(struct notifier_block *nb);
extern int input_unregister_notifier_client(struct notifier_block *nb);

#ifdef SCRIPTLESS
extern struct attribute_group i2c_cmd_attr_group;
#endif

#ifdef DRIVER_TEST
extern struct attribute_group test_cmd_attr_group;
#endif
#if defined(CONFIG_INPUT_PRESS_NEXTINPUT) || defined(CONFIG_INPUT_PRESS_NDT)
#define X_LEFT 287
#define Y_LEFT 1025
#define X_RIGHT 887
#define Y_RIGHT 1825
bool fts_is_infod(void);
void fts_senseon_without_cal(void);
void fts_senseoff_without_cal(void);
#endif
#endif
