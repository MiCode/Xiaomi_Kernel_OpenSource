/*
 * FTS Capacitive touch screen controller (FingerTipS)
 *
 * Copyright (C) 2016-2018, STMicroelectronics Limited.
 * Authors: AMG(Analog Mems Group) <marco.cali@st.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _LINUX_FTS_I2C_H_
#define _LINUX_FTS_I2C_H_

/*#include <linux/wakelock.h>*/
#include <linux/pm_wakeup.h>

#include "fts_lib/ftsSoftware.h"
#include "fts_lib/ftsHardware.h"
#include "fts_lib/ftsGesture.h"

#define FTS_POWER_ON     1
#define FTS_POWER_OFF    0

/****************** CONFIGURATION SECTION ******************/

/**** CODE CONFIGURATION ****/

#define FTS_TS_DRV_NAME      "fts"
#define FTS_TS_DRV_VERSION   "4.2.14" /* version */

#define SCRIPTLESS /*allow to work in scriptless mode with the GUI*/
#ifdef SCRIPTLESS
#define SCRIPTLESS_DEBUG
/**
 * uncomment this macro definition to print debug
 * message for script less  support
 */
#endif

#define DRIVER_TEST

/* #define FW_H_FILE */ /*include the FW as header file*/
#ifdef FW_H_FILE
	#define FW_SIZE_NAME myArray_size
	#define FW_ARRAY_NAME myArray
#endif

/*#define LIMITS_H_FILE*/ /*include the limits file as header file*/
#ifdef LIMITS_H_FILE
	#define LIMITS_SIZE_NAME myArray2_size
	#define LIMITS_ARRAY_NAME myArray2
#endif

/**** END ****/


/**** FEATURES USED IN THE IC ***/
#define PHONE_KEY /*enable the keys*/

#define PHONE_GESTURE /*allow to use the gestures*/
#ifdef PHONE_GESTURE
	#define USE_GESTURE_MASK
	#define USE_CUSTOM_GESTURES
#endif

#define USE_ONE_FILE_NODE
/*allow to enable/disable all the features just using one file node*/

#define EDGE_REJ
/*allow edge rej feature (comment to disable)*/

#define CORNER_REJ
/*allow corn rej feature (comment to disable)*/

#define EDGE_PALM_REJ
/*allow edge palm rej feature (comment to disable)*/

#define CHARGER_MODE
/*allow charger mode feature (comment to disable)*/

#define GLOVE_MODE
/*allow glove mode feature (comment to disable)*/

#define VR_MODE
/*allow vr mode feature (comment to disable)*/

#define COVER_MODE
/*allow cover mode feature (comment to disable)*/

#define STYLUS_MODE
/*allow stylus mode feature (comment to disable)*/

#define USE_NOISE_PARAM
/*set noise params during resume (comment to disable)*/

/**** END ****/


/**** PANEL SPECIFICATION ****/
#define X_AXIS_MAX           1440
#define X_AXIS_MIN           0
#define Y_AXIS_MAX           2880
#define Y_AXIS_MIN           0

#define PRESSURE_MIN         0
#define PRESSURE_MAX         127

#define TOUCH_ID_MAX         10

#define AREA_MIN             PRESSURE_MIN
#define AREA_MAX             PRESSURE_MAX
/**** END ****/

/*********************************************************/

/* Flash programming */

#define INIT_FLAG_CNT        3

/* KEYS */
#define KEY1                 0x02
#define KEY2                 0x01
#define KEY3                 0x04

/*
 * Configuration mode
 */
/**
 * bitmask which can assume the value defined as
 * features in ftsSoftware.h or the following values
 */

#define MODE_NOTHING         0x00000000
#define MODE_SENSEON         0x10000000
#define MODE_SENSEOFF        0x20000000
#define FEAT_GESTURE         0x40000000


/*
 * Status Event Field:
 * id of command that triggered the event
 */

#define FTS_FLASH_WRITE_CONFIG      0x03
#define FTS_FLASH_WRITE_COMP_MEMORY 0x04
#define FTS_FORCE_CAL_SELF_MUTUAL   0x05
#define FTS_FORCE_CAL_SELF          0x06
#define FTS_WATER_MODE_ON           0x07
#define FTS_WATER_MODE_OFF          0x08


#define EXP_FN_WORK_DELAY_MS 1000

#define CMD_STR_LEN		32
#define I2C_DATA_MAX_LEN	32

#ifdef SCRIPTLESS
/*
 * I2C Command Read/Write Function
 */

#define CMD_RESULT_STR_LEN   2048
#endif

#define TSP_BUF_SIZE         4096

#define PINCTRL_STATE_ACTIVE    "pmx_ts_active"
#define PINCTRL_STATE_SUSPEND   "pmx_ts_suspend"
#define PINCTRL_STATE_RELEASE   "pmx_ts_release"

/*add by guchong*/
#ifdef PHONE_GESTURE
extern u16 gesture_coordinates_x[GESTURE_COORDS_REPORT_MAX];
extern u16 gesture_coordinates_y[GESTURE_COORDS_REPORT_MAX];
extern int gesture_coords_reported;
extern struct mutex gestureMask_mutex;
#endif

struct fts_i2c_platform_data {
	bool x_flip;
	bool y_flip;
	int (*power)(bool on);
	int irq_gpio;
	int reset_gpio;
	const char *pwr_reg_name;
	const char *bus_reg_name;
};

/*
 * Forward declaration
 */
struct fts_ts_info;

/*
 * Dispatch event handler
 */
typedef void (*event_dispatch_handler_t)
		(struct fts_ts_info *info, unsigned char *data);
/*
 * struct fts_ts_info - FTS capacitive touch screen device information
 * @dev:                  Pointer to the structure device
 * @client:               I2C client structure
 * @input_dev             Input device structure
 * @work                  Work thread
 * @event_wq              Event queue for work thread
 * @event_dispatch_table  Event dispatch table handlers
 * @attrs                 SysFS attributes
 * @mode                  Device operating mode (bitmask)
 * @touch_id              Bitmask for touch id (mapped to input slots)
 * @stylus_id             Bitmask for tracking the stylus touches
 * (mapped using the touchId)
 * @timer                 Timer when operating in polling mode
 * @power                 Power on/off routine
 * @bdata                 HW info retrived from device tree
 * @pwr_reg               DVDD power regulator
 * @bus_reg               AVDD power regulator
 * @resume_bit            Indicate if screen off/on
 * @fwupdate_stat         Store the result of a fw update triggered by the host
 * @notifier              Used for be notified from a suspend/resume event
 * @sensor_sleep          true susped was called, false resume was called
 * @wakelock              Wake Lock struct
 * @input_report_mutex    mutex for handling the pressure of keys
 * @series of switches    to store the enabling status of a particular
 * feature from the host
 */
struct fts_ts_info {
	struct device            *dev;
	struct i2c_client        *client;
	struct input_dev         *input_dev;

	struct work_struct       work;
	struct work_struct       suspend_work;
	struct work_struct       resume_work;
	struct workqueue_struct  *event_wq;

	struct delayed_work      fwu_work;
	struct workqueue_struct  *fwu_workqueue;
	struct completion        cmd_done;

	struct pinctrl *ts_pinctrl;
	struct pinctrl_state *pinctrl_state_active;
	struct pinctrl_state *pinctrl_state_suspend;
	struct pinctrl_state *pinctrl_state_release;

	event_dispatch_handler_t *event_dispatch_table;

	struct attribute_group    attrs;

	unsigned int              mode;
	unsigned long             touch_id;
#ifdef STYLUS_MODE
	unsigned long             stylus_id;
#endif


#ifdef FTS_USE_POLLING_MODE
	struct hrtimer            timer;
#endif


#ifdef SCRIPTLESS
	/*I2C cmd*/
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
	struct regulator          *pwr_reg;
	struct regulator          *bus_reg;

	int resume_bit;
	int fwupdate_stat;

	struct notifier_block notifier;
	bool sensor_sleep;
	struct wakeup_source wakeup_source;

	/* input lock */
	struct mutex input_report_mutex;

	/*switches for features*/
	int gesture_enabled;
	int glove_enabled;
	int charger_enabled;
	int stylus_enabled;
	int vr_enabled;
	int cover_enabled;
	int edge_rej_enabled;
	int corner_rej_enabled;
	int edge_palm_rej_enabled;

	uint8_t *i2c_data;
	uint8_t i2c_data_len;
};

extern struct chipInfo ftsInfo;

int fts_chip_powercycle(struct fts_ts_info *info);
int fts_chip_powercycle2(struct fts_ts_info *info, unsigned long sleep);
/*int fts_get_fw_version(struct fts_ts_info *info);*/
/*extern unsigned int le_to_uint(const unsigned char *ptr);*/
/*extern unsigned int be_to_uint(const unsigned char *ptr);*/
extern int input_register_notifier_client(struct notifier_block *nb);
extern int input_unregister_notifier_client(struct notifier_block *nb);

#ifdef SCRIPTLESS
extern struct attribute_group	i2c_cmd_attr_group;
#endif

#ifdef DRIVER_TEST
extern struct attribute_group	test_cmd_attr_group;
#endif


#endif

