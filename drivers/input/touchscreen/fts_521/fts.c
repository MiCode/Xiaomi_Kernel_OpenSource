/*
 * fts.c
 *
 * FTS Capacitive touch screen controller (FingerTipS)
 *
 * Copyright (C) 2016, STMicroelectronics Limited.
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
* \file fts.c
* \brief It is the main file which contains all the most important functions generally used by a device driver the driver
*/
#include <linux/device.h>

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/hrtimer.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/spi/spi.h>
#include <linux/completion.h>
#ifdef CONFIG_SECURE_TOUCH
#include <linux/atomic.h>
#include <linux/sysfs.h>
#include <linux/hardirq.h>
#endif

#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>

#include <linux/notifier.h>
#ifdef CONFIG_DRM
#include <drm/drm_notifier.h>
#endif
#include <linux/backlight.h>


#include <linux/fb.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/hwinfo.h>
#ifdef KERNEL_ABOVE_2_6_38
#include <linux/input/mt.h>
#endif

#include "fts.h"
#include "fts_lib/ftsCompensation.h"
#include "fts_lib/ftsCore.h"
#include "fts_lib/ftsIO.h"
#include "fts_lib/ftsError.h"
#include "fts_lib/ftsFlash.h"
#include "fts_lib/ftsFrame.h"
#include "fts_lib/ftsGesture.h"
#include "fts_lib/ftsTest.h"
#include "fts_lib/ftsTime.h"
#include "fts_lib/ftsTool.h"
#ifdef CONFIG_INPUT_PRESS_NDT
#include "./../ndt_core.h"
#endif
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE
#include "../xiaomi/xiaomi_touch.h"
#endif

/**
 * Event handler installer helpers
 */
#define event_id(_e)     (EVT_ID_##_e>>4)
#define handler_name(_h) fts_##_h##_event_handler

#define install_handler(_i, _evt, _hnd) \
do { \
	_i->event_dispatch_table[event_id(_evt)] = handler_name(_hnd); \
} while (0)

#ifdef KERNEL_ABOVE_2_6_38
#define TYPE_B_PROTOCOL
#endif

#define INPUT_EVENT_START			0
#define INPUT_EVENT_SENSITIVE_MODE_OFF		0
#define INPUT_EVENT_SENSITIVE_MODE_ON		1
#define INPUT_EVENT_STYLUS_MODE_OFF		2
#define INPUT_EVENT_STYLUS_MODE_ON		3
#define INPUT_EVENT_WAKUP_MODE_OFF		4
#define INPUT_EVENT_WAKUP_MODE_ON		5
#define INPUT_EVENT_COVER_MODE_OFF		6
#define INPUT_EVENT_COVER_MODE_ON		7
#define INPUT_EVENT_SLIDE_FOR_VOLUME		8
#define INPUT_EVENT_DOUBLE_TAP_FOR_VOLUME		9
#define INPUT_EVENT_SINGLE_TAP_FOR_VOLUME		10
#define INPUT_EVENT_LONG_SINGLE_TAP_FOR_VOLUME		11
#define INPUT_EVENT_PALM_OFF		12
#define INPUT_EVENT_PALM_ON		13
#define INPUT_EVENT_END				13

extern SysInfo systemInfo;
extern TestToDo tests;
#ifdef GESTURE_MODE
extern struct mutex gestureMask_mutex;
#endif

char tag[8] = "[ FTS ]\0";
/* buffer which store the input device name assigned by the kernel  */
char fts_ts_phys[64];
/* buffer used to store the command sent from the MP device file node  */
static u32 typeOfComand[CMD_STR_LEN] = { 0 };

/* number of parameter passed through the MP device file node  */
static int numberParameters;
#ifdef USE_ONE_FILE_NODE
static int feature_feasibility = ERROR_OP_NOT_ALLOW;
#endif
#ifdef GESTURE_MODE
static u8 mask[GESTURE_MASK_SIZE + 2];
extern u16 gesture_coordinates_x[GESTURE_MAX_COORDS_PAIRS_REPORT];
extern u16 gesture_coordinates_y[GESTURE_MAX_COORDS_PAIRS_REPORT];
extern int gesture_coords_reported;
extern struct mutex gestureMask_mutex;
#endif
/* store the last update of the key mask published by the IC */
#ifdef PHONE_KEY
static u8 key_mask;
#endif
#ifdef CONFIG_INPUT_PRESS_NDT
bool fts_fod_status;
#endif
extern void lpm_disable_for_input(bool on);
extern spinlock_t fts_int;
struct fts_ts_info *fts_info;

static int fts_init_sensing(struct fts_ts_info *info);
static int fts_mode_handler(struct fts_ts_info *info, int force);
static int fts_chip_initialization(struct fts_ts_info *info, int init_type);
static const char *fts_get_limit(struct fts_ts_info *info);
extern const char *dsi_get_display_name(void);

static irqreturn_t fts_event_handler(int irq, void *ts_info);
bool wait_queue_complete;


/**
* Release all the touches in the linux input subsystem
* @param info pointer to fts_ts_info which contains info about the device and its hw setup
*/
void release_all_touches(struct fts_ts_info *info)
{
	unsigned int type = MT_TOOL_FINGER;
	int i;

	for (i = 0; i < TOUCH_ID_MAX; i++) {
#ifdef STYLUS_MODE
		if (test_bit(i, &info->stylus_id))
			type = MT_TOOL_PEN;
		else
			type = MT_TOOL_FINGER;
#endif
		input_mt_slot(info->input_dev, i);
		input_mt_report_slot_state(info->input_dev, type, 0);
		input_report_abs(info->input_dev, ABS_MT_TRACKING_ID, -1);
	}
	input_sync(info->input_dev);
	lpm_disable_for_input(false);
	info->touch_id = 0;
#ifdef STYLUS_MODE
	info->stylus_id = 0;
#endif
}

/**
 * @defgroup file_nodes Driver File Nodes
 * Driver publish a series of file nodes used to provide several utilities to the host and give him access to different API.
 * @{
 */

/**
 * @defgroup device_file_nodes Device File Nodes
 * @ingroup file_nodes
 * Device File Nodes \n
 * There are several file nodes that are associated to the device and which are designed to be used by the host to enable/disable features or trigger some system specific actions \n
 * Usually their final path depend on the definition of device tree node of the IC (e.g /sys/devices/soc.0/f9928000.i2c/i2c-6/6-0049)
 * @{
 */
/***************************************** FW UPGGRADE ***************************************************/

/**
 * File node function to Update firmware from shell \n
 * echo path_to_fw X Y > fwupdate   perform a fw update \n
 * where: \n
 * path_to_fw = file name or path of the the FW to burn, if "NULL" the default approach selected in the driver will be used\n
 * X = 0/1 to force the FW update whichever fw_version and config_id; 0=perform a fw update only if the fw in the file is newer than the fw in the chip \n
 * Y = 0/1 keep the initialization data; 0 = will erase the initialization data from flash, 1 = will keep the initialization data
 * the string returned in the shell is made up as follow: \n
 * { = start byte \n
 * X1X2X3X4 = 4 bytes in HEX format which represent an error code (00000000 no error) \n
 * } = end byte
 */
static ssize_t fts_fwupdate_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	int ret, mode[2];
	char path[100];
	struct fts_ts_info *info = dev_get_drvdata(dev);

	/* by default(if not specified by the user) set the force = 0 and keep_cx to 1 */
	mode[0] = 0;
	mode[1] = 1;

	/* reading out firmware upgrade parameters */
	sscanf(buf, "%100s %d %d", path, &mode[0], &mode[1]);
	logError(1, "%s fts_fwupdate_store: mode = %s \n", tag, path);

	ret = flashProcedure(path, mode[0], mode[1]);

	info->fwupdate_stat = ret;

	if (ret < OK)
		logError(1, "%s  %s Unable to upgrade firmware! ERROR %08X\n",
			 tag, __func__, ret);
	return count;
}

static ssize_t fts_fwupdate_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct fts_ts_info *info = dev_get_drvdata(dev);

	/*fwupdate_stat: ERROR code Returned by flashProcedure. */
	return snprintf(buf, PAGE_SIZE, "{ %08X }\n", info->fwupdate_stat);
}

/***************************************** UTILITIES (current fw_ver/conf_id, active mode, file fw_ver/conf_id)  ***************************************************/
/**
* File node to show on terminal external release version in Little Endian (first the less significant byte) \n
* cat appid			show on the terminal external release version of the FW running in the IC
*/
static ssize_t fts_appid_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	int error;
	char temp[100] = { 0x00, };

	error = snprintf(buf, PAGE_SIZE, "%s\n",
			 printHex("EXT Release = ", systemInfo.u8_releaseInfo,
				  EXTERNAL_RELEASE_INFO_SIZE, temp));

	return error;
}

/**
 * File node to show on terminal the mode that is active on the IC \n
 * cat mode_active		    to show the bitmask which indicate the modes/features which are running on the IC in a specific instant of time
 * the string returned in the shell is made up as follow: \n
 * { = start byte \n
 * X1 = 1 byte in HEX format which represent the actual running scan mode (@link scan_opt Scan Mode Options @endlink) \n
 * X2 = 1 byte in HEX format which represent the bitmask on which is running the actual scan mode \n
 * X3X4 = 2 bytes in HEX format which represent a bitmask of the features that are enabled at this moment (@link feat_opt Feature Selection Options @endlink) \n
 * } = end byte
 * @see fts_mode_handler()
 */
static ssize_t fts_mode_active_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct fts_ts_info *info = dev_get_drvdata(dev);

	logError(1, "%s Current mode active = %08X\n", tag, info->mode);
	return snprintf(buf, PAGE_SIZE, "{ %08X }\n", info->mode);
}

/**
 * File node to show the fw_ver and config_id of the FW file
 * cat fw_file_test			show on the kernel log fw_version and config_id of the FW stored in the fw file/header file
 */
static ssize_t fts_fw_test_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	Firmware fw;
	int ret;
	char temp[100] = { 0x00, };
	struct fts_ts_info *info = dev_get_drvdata(dev);

	fw.data = NULL;
	ret = readFwFile(info->board->default_fw_name, &fw, 0);

	if (ret < OK) {
		logError(1, "%s Error during reading FW file! ERROR %08X\n",
			 tag, ret);
	} else {
		logError(1, "%s %s, size = %d bytes\n", tag,
			 printHex("EXT Release = ", systemInfo.u8_releaseInfo,
				  EXTERNAL_RELEASE_INFO_SIZE, temp),
			 fw.data_size);
	}

	kfree(fw.data);
	return 0;
}

/***************************************** FEATURES ***************************************************/

/*TODO: edit this function according to the features policy to allow during the screen on/off, following is shown an example but check always with ST for more details*/
/**
 * Check if there is any conflict in enable/disable a particular feature considering the features already enabled and running
 * @param info pointer to fts_ts_info which contains info about the device and its hw setup
 * @param feature code of the feature that want to be tested
 * @return OK if is possible to enable/disable feature, ERROR_OP_NOT_ALLOW in case of any other conflict
 */
int check_feature_feasibility(struct fts_ts_info *info, unsigned int feature)
{
	int res = OK;

	switch (feature) {
	case FEAT_SEL_GESTURE:
		if (info->cover_enabled == 1) {
			res = ERROR_OP_NOT_ALLOW;
			logError(1,
				 "%s %s: Feature not allowed when in Cover mode! ERROR %08X \n",
				 tag, __func__, res);
			/*for example here can be placed a code for disabling the cover mode when gesture is activated */
		}
		break;

	case FEAT_SEL_GLOVE:
		if (info->gesture_enabled == 1) {
			res = ERROR_OP_NOT_ALLOW;
			logError(1,
				 "%s %s: Feature not allowed when Gestures enabled! ERROR %08X \n",
				 tag, __func__, res);
			/*for example here can be placed a code for disabling the gesture mode when cover is activated (that means that cover mode has an higher priority on gesture mode) */
		}
		break;

	default:
		logError(1, "%s %s: Feature Allowed! \n", tag, __func__);

	}

	return res;

}

#ifdef USE_ONE_FILE_NODE
/**
 * File node to enable some feature
 * echo XX 00/01 > feature_enable		to enable/disable XX (possible values @link feat_opt Feature Selection Options @endlink) feature \n
 * cat feature_enable					to show the result of enabling/disabling process \n
 * echo 01/00 > feature_enable; cat feature_enable 		to perform both actions stated before in just one call \n
 * the string returned in the shell is made up as follow: \n
 * { = start byte \n
 * X1X2X3X4 = 4 bytes in HEX format which represent an error code (00000000 = no error) \n
 * } = end byte
 */
static ssize_t fts_feature_enable_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct fts_ts_info *info = dev_get_drvdata(dev);
	char *p = (char *)buf;
	unsigned int temp;
	int res = OK;

	if ((count - 2 + 1) / 3 != 1) {
		logError(1,
			 "%s fts_feature_enable: Number of parameter wrong! %d > %d \n",
			 tag, (count - 2 + 1) / 3, 1);
	} else {
		sscanf(p, "%02X ", &temp);
		p += 9;
		res = check_feature_feasibility(info, temp);
		if (res >= OK) {
			switch (temp) {

#ifdef GESTURE_MODE
			case FEAT_SEL_GESTURE:
				sscanf(p, "%02X ", &info->gesture_enabled);
				logError(1,
					 "%s fts_feature_enable: Gesture Enabled = %d \n",
					 tag, info->gesture_enabled);
				break;
#endif

#ifdef GLOVE_MODE
			case FEAT_SEL_GLOVE:
				sscanf(p, "%02X ", &info->glove_enabled);
				logError(1,
					 "%s fts_feature_enable: Glove Enabled = %d \n",
					 tag, info->glove_enabled);

				break;
#endif

#ifdef STYLUS_MODE
			case FEAT_SEL_STYLUS:
				sscanf(p, "%02X ", &info->stylus_enabled);
				logError(1,
					 "%s fts_feature_enable: Stylus Enabled = %d \n",
					 tag, info->stylus_enabled);

				break;
#endif

#ifdef COVER_MODE
			case FEAT_SEL_COVER:
				sscanf(p, "%02X ", &info->cover_enabled);
				logError(1,
					 "%s fts_feature_enable: Cover Enabled = %d \n",
					 tag, info->cover_enabled);

				break;
#endif

#ifdef CHARGER_MODE
			case FEAT_SEL_CHARGER:
				sscanf(p, "%02X ", &info->charger_enabled);
				logError(1,
					 "%s fts_feature_enable: Charger Enabled = %d \n",
					 tag, info->charger_enabled);

				break;
#endif

#ifdef GRIP_MODE
			case FEAT_SEL_GRIP:
				sscanf(p, "%02X ", &info->grip_enabled);
				logError(1,
					 "%s fts_feature_enable: Grip Enabled = %d \n",
					 tag, info->grip_enabled);

				break;
#endif
			default:
				logError(1,
					 "%s fts_feature_enable: Feature %08X not valid! ERROR %08X\n",
					 tag, temp, ERROR_OP_NOT_ALLOW);
				res = ERROR_OP_NOT_ALLOW;
			}
			feature_feasibility = res;
		}
		if (feature_feasibility >= OK)
			feature_feasibility = fts_mode_handler(info, 1);
		else {
			logError(1,
				 "%s %s: Call echo XX 00/01 > feature_enable with a correct feature value (XX)! ERROR %08X \n",
				 tag, __func__, res);
		}

	}
	return count;
}

static ssize_t fts_feature_enable_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	int size = (6 * 2) + 1, index = 0;
	u8 *all_strbuff = NULL;
	int count = 0;

	if (feature_feasibility < OK) {
		logError(1,
			 "%s %s: Call before echo XX 00/01 > feature_enable with a correct feature value (XX)! ERROR %08X \n",
			 tag, __func__, feature_feasibility);
	}

	all_strbuff = (u8 *) kzalloc(size, GFP_KERNEL);
	if (all_strbuff != NULL) {
		index +=
		    snprintf(&all_strbuff[index], 13, "{ %08X }",
			     feature_feasibility);
		count = snprintf(buf, TSP_BUF_SIZE, "%s\n", all_strbuff);
		kfree(all_strbuff);
	} else {
		logError(1,
			 "%s fts_feature_enable_show: Unable to allocate all_strbuff! ERROR %08X\n",
			 tag, ERROR_ALLOC);
	}

	feature_feasibility = ERROR_OP_NOT_ALLOW;
	return count;
}
#else

#ifdef GRIP_MODE
/**
 * File node to set the grip mode
 * echo 01/00 > grip_mode		to enable/disable glove mode \n
 * cat grip_mode				to show the status of the grip_enabled switch \n
 * echo 01/00 > grip_mode; cat grip_mode 		to enable/disable grip mode and see the switch status in just one call \n
 * the string returned in the shell is made up as follow: \n
 * { = start byte \n
 * X1X2X3X4 = 4 bytes in HEX format which represent the value info->grip_enabled (1 = enabled; 0= disabled) \n
 * } = end byte
 */
static ssize_t fts_grip_mode_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{

	int size = (6 * 2) + 1, index = 0;
	u8 *all_strbuff = NULL;
	int count = 0;
	struct fts_ts_info *info = dev_get_drvdata(dev);

	logError(0, "%s %s: grip_enabled = %d \n", tag, __func__,
		 info->grip_enabled);

	all_strbuff = (u8 *) kzalloc(size, GFP_KERNEL);
	if (all_strbuff != NULL) {

		index +=
		    snprintf(&all_strbuff[index], 13, "{ %08X }",
			     info->grip_enabled);

		count = snprintf(buf, TSP_BUF_SIZE, "%s\n", all_strbuff);
		kfree(all_strbuff);
	} else {
		logError(1,
			 "%s %s: Unable to allocate all_strbuff! ERROR %08X\n",
			 tag, __func__, ERROR_ALLOC);
	}

	return count;
}

static ssize_t fts_grip_mode_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	char *p = (char *)buf;
	unsigned int temp;
	int res;
	struct fts_ts_info *info = dev_get_drvdata(dev);

/*in case of a different elaboration of the input, just modify this initial part of the code according to customer needs*/
	if ((count + 1) / 3 != 1) {
		logError(1,
			 "%s %s: Number of bytes of parameter wrong! %d != %d byte\n",
			 tag, __func__, (count + 1) / 3, 1);
	} else {
		sscanf(p, "%02X ", &temp);
		p += 3;
/*
*this is a standard code that should be always used when a feature is enabled!
*first step : check if the wanted feature can be enabled
*second step: call fts_mode_handler to actually enable it
*NOTE: Disabling a feature is always allowed by default
*/
		res = check_feature_feasibility(info, FEAT_SEL_GRIP);
		if (res >= OK || temp == FEAT_DISABLE) {
			info->grip_enabled = temp;
			res = fts_mode_handler(info, 1);
			if (res < OK) {
				logError(1,
					 "%s %s: Error during fts_mode_handler! ERROR %08X\n",
					 tag, __func__, res);
			}
		}
	}
	return count;
}
#endif

#ifdef CHARGER_MODE
/**
 * File node to set the glove mode
 * echo XX/00 > charger_mode		to value >0 to enable (possible values: @link charger_opt Charger Options @endlink),00 to disable charger mode \n
 * cat charger_mode				to show the status of the charger_enabled switch \n
 * echo 01/00 > charger_mode; cat charger_mode 		to enable/disable charger mode and see the switch status in just one call \n
 * the string returned in the shell is made up as follow: \n
 * { = start byte \n
 * X1X2X3X4 = 4 bytes in HEX format which represent the value info->charger_enabled (>0 = enabled; 0= disabled) \n
 * } = end byte
 */
static ssize_t fts_charger_mode_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	int size = (6 * 2) + 1, index = 0;
	u8 *all_strbuff = NULL;
	int count = 0;
	struct fts_ts_info *info = dev_get_drvdata(dev);

	logError(0, "%s %s: charger_enabled = %d \n", tag, __func__,
		 info->charger_enabled);

	all_strbuff = (u8 *) kzalloc(size, GFP_KERNEL);
	if (all_strbuff != NULL) {
		index +=
		    snprintf(&all_strbuff[index], 13, "{ %08X }",
			     info->charger_enabled);
		count = snprintf(buf, TSP_BUF_SIZE, "%s\n", all_strbuff);
		kfree(all_strbuff);
	} else {
		logError(1,
			 "%s %s: Unable to allocate all_strbuff! ERROR %08X\n",
			 tag, __func__, ERROR_ALLOC);
	}

	return count;
}

static ssize_t fts_charger_mode_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	char *p = (char *)buf;
	unsigned int temp;
	int res;
	struct fts_ts_info *info = dev_get_drvdata(dev);

/*in case of a different elaboration of the input, just modify this initial part of the code according to customer needs*/
	if ((count + 1) / 3 != 1) {
		logError(1,
			 "%s %s: Number of bytes of parameter wrong! %d != %d byte\n",
			 tag, __func__, (count + 1) / 3, 1);
	} else {
		sscanf(p, "%02X ", &temp);
		p += 3;
/*
*this is a standard code that should be always used when a feature is enabled!
*first step : check if the wanted feature can be enabled
*second step: call fts_mode_handler to actually enable it
*NOTE: Disabling a feature is always allowed by default
*/
		res = check_feature_feasibility(info, FEAT_SEL_CHARGER);
		if (res >= OK || temp == FEAT_DISABLE) {
			info->charger_enabled = temp;
			res = fts_mode_handler(info, 1);
			if (res < OK) {
				logError(1,
					 "%s %s: Error during fts_mode_handler! ERROR %08X\n",
					 tag, __func__, res);
			}
		}
	}
	return count;
}
#endif

#ifdef GLOVE_MODE
/**
 * File node to set the glove mode
 * echo 01/00 > glove_mode		to enable/disable glove mode \n
 * cat glove_mode				to show the status of the glove_enabled switch \n
 * echo 01/00 > glove_mode; cat glove_mode 		to enable/disable glove mode and see the switch status in just one call \n
 * the string returned in the shell is made up as follow: \n
 * { = start byte \n
 * X1X2X3X4 = 4 bytes in HEX format which represent the of value info->glove_enabled (1 = enabled; 0= disabled) \n
 * } = end byte
 */
static ssize_t fts_glove_mode_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int size = (6 * 2) + 1, index = 0;
	u8 *all_strbuff = NULL;
	int count = 0;
	struct fts_ts_info *info = dev_get_drvdata(dev);

	logError(0, "%s %s: glove_enabled = %d \n", tag, __func__,
		 info->glove_enabled);

	all_strbuff = (u8 *) kzalloc(size, GFP_KERNEL);
	if (all_strbuff != NULL) {

		index +=
		    snprintf(&all_strbuff[index], 13, "{ %08X }",
			     info->glove_enabled);
		count = snprintf(buf, TSP_BUF_SIZE, "%s\n", all_strbuff);
		kfree(all_strbuff);
	} else {
		logError(1,
			 "%s %s: Unable to allocate all_strbuff! ERROR %08X\n",
			 tag, __func__, ERROR_ALLOC);
	}

	return count;
}

static ssize_t fts_glove_mode_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	char *p = (char *)buf;
	unsigned int temp;
	int res;
	struct fts_ts_info *info = dev_get_drvdata(dev);

/*in case of a different elaboration of the input, just modify this initial part of the code according to customer needs*/
	if ((count + 1) / 3 != 1) {
		logError(1,
			 "%s %s: Number of bytes of parameter wrong! %d != %d byte\n",
			 tag, __func__, (count + 1) / 3, 1);
	} else {
		sscanf(p, "%02X ", &temp);
		p += 3;
/*
*this is a standard code that should be always used when a feature is enabled!
*first step : check if the wanted feature can be enabled
*second step: call fts_mode_handler to actually enable it
*NOTE: Disabling a feature is always allowed by default
*/
		res = check_feature_feasibility(info, FEAT_SEL_GLOVE);
		if (res >= OK || temp == FEAT_DISABLE) {
			info->glove_enabled = temp;
			res = fts_mode_handler(info, 1);
			if (res < OK) {
				logError(1,
					 "%s %s: Error during fts_mode_handler! ERROR %08X\n",
					 tag, __func__, res);
			}
		}
	}

	return count;
}
#endif

#ifdef COVER_MODE
/**
 * File node to set the cover mode
 * echo 01/00 > cover_mode		to enable/disable cover mode \n
 * cat cover_mode				to show the status of the cover_enabled switch \n
 * echo 01/00 > cover_mode; cat cover_mode 		to enable/disable cover mode and see the switch status in just one call \n
 * the string returned in the shell is made up as follow: \n
 * { = start byte \n
 * X1X2X3X4 = 4 bytes in HEX format which is the value of info->cover_enabled (1 = enabled; 0= disabled)\n
 * } = end byte\n
 * NOTE: \n
 * the cover can be handled also using a notifier, in this case the body of these functions should be copied in the notifier callback
 */
static ssize_t fts_cover_mode_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int size = (6 * 2) + 1, index = 0;
	u8 *all_strbuff = NULL;
	int count = 0;
	struct fts_ts_info *info = dev_get_drvdata(dev);

	logError(0, "%s %s: cover_enabled = %d \n", tag, __func__,
		 info->cover_enabled);

	all_strbuff = (u8 *) kzalloc(size, GFP_KERNEL);
	if (all_strbuff != NULL) {

		index +=
		    snprintf(&all_strbuff[index], 13, "{ %08X }",
			     info->cover_enabled);
		count = snprintf(buf, TSP_BUF_SIZE, "%s\n", all_strbuff);
		kfree(all_strbuff);
	} else {
		logError(1,
			 "%s %s: Unable to allocate all_strbuff! ERROR %08X\n",
			 tag, __func__, ERROR_ALLOC);
	}

	return count;
}

static ssize_t fts_cover_mode_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	char *p = (char *)buf;
	unsigned int temp;
	int res;
	struct fts_ts_info *info = dev_get_drvdata(dev);

/*in case of a different elaboration of the input, just modify this initial part of the code according to customer needs*/
	if ((count + 1) / 3 != 1) {
		logError(1,
			 "%s %s: Number of bytes of parameter wrong! %d != %d byte\n",
			 tag, __func__, (count + 1) / 3, 1);
	} else {
		sscanf(p, "%02X ", &temp);
		p += 3;
/*
*this is a standard code that should be always used when a feature is enabled!
*first step : check if the wanted feature can be enabled
*second step: call fts_mode_handler to actually enable it
*NOTE: Disabling a feature is always allowed by default
*/
		res = check_feature_feasibility(info, FEAT_SEL_COVER);
		if (res >= OK || temp == FEAT_DISABLE) {
			info->cover_enabled = temp;
			res = fts_mode_handler(info, 1);
			if (res < OK) {
				logError(1,
					 "%s %s: Error during fts_mode_handler! ERROR %08X\n",
					 tag, __func__, res);
			}
		}
	}

	return count;
}
#endif

#ifdef STYLUS_MODE
/**
 * File node to enable the stylus report
 * echo 01/00 > stylus_mode		to enable/disable stylus mode\n
 * cat stylus_mode				to show the status of the stylus_enabled switch\n
 * echo 01/00 > stylus_mode; cat stylus_mode 		to enable/disable stylus mode and see the switch status in just one call\n
 * the string returned in the shell is made up as follow:\n
 * { = start byte \n
 * X1X2X3X4 = 4 bytes in HEX format which is the value of info->stylus_enabled (1 = enabled; 0= disabled)\n
 * } = end byte
 */
static ssize_t fts_stylus_mode_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	int size = (6 * 2) + 1, index = 0;
	u8 *all_strbuff = NULL;
	int count = 0;
	struct fts_ts_info *info = dev_get_drvdata(dev);

	logError(0, "%s %s: stylus_enabled = %d \n", tag, __func__,
		 info->stylus_enabled);

	all_strbuff = (u8 *) kzalloc(size, GFP_KERNEL);
	if (all_strbuff != NULL) {

		index +=
		    snprintf(&all_strbuff[index], 13, "{ %08X }",
			     info->stylus_enabled);

		count = snprintf(buf, TSP_BUF_SIZE, "%s\n", all_strbuff);
		kfree(all_strbuff);
	} else {
		logError(1,
			 "%s %s: Unable to allocate all_strbuff! ERROR %08X\n",
			 tag, __func__, ERROR_ALLOC);
	}

	return count;
}

static ssize_t fts_stylus_mode_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	char *p = (char *)buf;
	unsigned int temp;
	struct fts_ts_info *info = dev_get_drvdata(dev);

/*in case of a different elaboration of the input, just modify this initial part of the code according to customer needs*/
	if ((count + 1) / 3 != 1) {
		logError(1,
			 "%s %s: Number of bytes of parameter wrong! %d != %d byte\n",
			 tag, __func__, (count + 1) / 3, 1);
	} else {
		sscanf(p, "%02X ", &temp);
		p += 3;
		info->stylus_enabled = temp;

	}
	return count;
}
#endif

#endif

/***************************************** GESTURES ***************************************************/
#ifdef GESTURE_MODE
#ifdef USE_GESTURE_MASK
/**
 * File node used by the host to set the gesture mask to enable or disable
 * echo EE X1 X2 ~~ > gesture_mask  set the gesture to disable/enable; EE = 00(disable) or 01(enable)\n
 *                                  X1 ~~  = gesture mask (example 06 00 ~~ 00 this gesture mask represents the gestures with ID = 1 and 2) can be specified from 1 to GESTURE_MASK_SIZE bytes,\n
 *                                  if less than GESTURE_MASK_SIZE bytes are passed as arguments, the omit bytes of the mask maintain the previous settings\n
 *                                  if one or more gestures is enabled the driver will automatically enable the gesture mode, If all the gestures are disabled the driver automatically will disable the gesture mode\n
 * cat gesture_mask                 set inside the specified mask and return an error code for the operation \n
 * the string returned in the shell is made up as follow:\n
 * { = start byte \n
 * X1X2X3X4 = 4 bytes in HEX format which represent an error code for enabling the mask (00000000 = no error)\n
 * } = end byte \n\n
 * if USE_GESTURE_MASK is not define the usage of the function become: \n\n
 * echo EE X1 X2 ~~ > gesture_mask   set the gesture to disable/enable; EE = 00(disable) or 01(enable)\n
 *                                   X1 ~~ = gesture IDs (example 01 02 05 represent the gestures with ID = 1, 2 and 5) there is no limit of the IDs passed as arguments, (@link gesture_opt Gesture IDs @endlink)\n
 *                                   if one or more gestures is enabled the driver will automatically enable the gesture mode. If all the gestures are disabled the driver automatically will disable the gesture mode.\n
 * cat gesture_mask                  to show the status of the gesture enabled switch \n
 * the string returned in the shell is made up as follow:\n
 * { = start byte \n
 * X1X2X3X4 = 4 bytes in HEX format which is the value of info->gesture_enabled (1 = enabled; 0= disabled)\n
 * } = end byte
 */
static ssize_t fts_gesture_mask_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	int size = (6 * 2) + 1, index = 0;
	u8 *all_strbuff = NULL;
	int count = 0, res, temp;
	struct fts_ts_info *info = dev_get_drvdata(dev);

	if (mask[0] == 0) {
		res = ERROR_OP_NOT_ALLOW;
		logError(1,
			 "%s %s: Call before echo enable/disable xx xx .... > gesture_mask with a correct number of parameters! ERROR %08X \n",
			 tag, __func__, res);
	} else {

		if (mask[1] == FEAT_ENABLE || mask[1] == FEAT_DISABLE)
			res = updateGestureMask(&mask[2], mask[0], mask[1]);
		else
			res = ERROR_OP_NOT_ALLOW;

		if (res < OK) {
			logError(1, "%s fts_gesture_mask_store: ERROR %08X \n",
				 tag, res);
		}
	}
	res |= check_feature_feasibility(info, FEAT_SEL_GESTURE);
	temp = isAnyGestureActive();
	if (res >= OK || temp == FEAT_DISABLE) {
		info->gesture_enabled = temp;
	}

	logError(1, "%s fts_gesture_mask_store: Gesture Enabled = %d \n", tag,
		 info->gesture_enabled);

	all_strbuff = (u8 *) kzalloc(size, GFP_KERNEL);
	if (all_strbuff != NULL) {

		index += snprintf(&all_strbuff[index], 13, "{ %08X }", res);

		count = snprintf(buf, TSP_BUF_SIZE, "%s\n", all_strbuff);
		kfree(all_strbuff);
	} else {
		logError(1,
			 "%s fts_gesture_mask_show: Unable to allocate all_strbuff! ERROR %08X\n",
			 tag, ERROR_ALLOC);
	}

	mask[0] = 0;
	return count;
}

static ssize_t fts_gesture_mask_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	char *p = (char *)buf;
	int n;
	unsigned int temp;

	if ((count + 1) / 3 > GESTURE_MASK_SIZE + 1) {
		logError(1,
			 "%s fts_gesture_mask_store: Number of bytes of parameter wrong! %d > (enable/disable + %d )\n",
			 tag, (count + 1) / 3, GESTURE_MASK_SIZE);
		mask[0] = 0;
	} else {
		mask[0] = ((count + 1) / 3) - 1;
		for (n = 1; n <= (count + 1) / 3; n++) {
			sscanf(p, "%02X ", &temp);
			p += 3;
			mask[n] = (u8) temp;
			logError(0, "%s mask[%d] = %02X \n", tag, n, mask[n]);

		}
	}

	return count;
}

#else
/**
 * File node used by the host to set the gesture mask to enable or disable
 * echo EE X1 X2 ~~ > gesture_mask	set the gesture to disable/enable; EE = 00(disable) or 01(enable)\n
 *									X1 ~ = gesture IDs (example 01 02 05 represent the gestures with ID = 1, 2 and 5) there is no limit of the IDs passed as arguments, (@link gesture_opt Gesture IDs @endlink) \n
 *									if one or more gestures is enabled the driver will automatically enable the gesture mode, If all the gestures are disabled the driver automatically will disable the gesture mode \n
 * cat gesture_mask					to show the status of the gesture enabled switch \n
 * the string returned in the shell is made up as follow: \n
 * { = start byte \n
 * X1X2X3X4 = 4 bytes in HEX format which is the value of info->gesture_enabled (1 = enabled; 0= disabled)\n
 * } = end byte
 */
static ssize_t fts_gesture_mask_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	int size = (6 * 2) + 1, index = 0;
	u8 *all_strbuff = NULL;
	int count = 0;
	struct fts_ts_info *info = dev_get_drvdata(dev);

	logError(0, "%s fts_gesture_mask_show: gesture_enabled = %d \n", tag,
		 info->gesture_enabled);

	all_strbuff = (u8 *) kzalloc(size, GFP_KERNEL);
	if (all_strbuff != NULL) {

		index +=
		    snprintf(&all_strbuff[index], 13, "{ %08X }",
			     info->gesture_enabled);

		count = snprintf(buf, TSP_BUF_SIZE, "%s\n", all_strbuff);
		kfree(all_strbuff);
	} else {
		logError(1,
			 "%s fts_gesture_mask_show: Unable to allocate all_strbuff! ERROR %08X\n",
			 tag, ERROR_ALLOC);
	}

	return count;
}

static ssize_t fts_gesture_mask_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	char *p = (char *)buf;
	int n;
	unsigned int temp;
	int res;
	struct fts_ts_info *info = dev_get_drvdata(dev);

	if ((count + 1) / 3 < 2 || (count + 1) / 3 > GESTURE_MASK_SIZE + 1) {
		logError(1,
			 "%s fts_gesture_mask_store: Number of bytes of parameter wrong! %d < or > (enable/disable + at least one gestureID or max %d bytes)\n",
			 tag, (count + 1) / 3, GESTURE_MASK_SIZE);
		mask[0] = 0;
	} else {
		memset(mask, 0, GESTURE_MASK_SIZE + 2);
		mask[0] = ((count + 1) / 3) - 1;
		sscanf(p, "%02X ", &temp);
		p += 3;
		mask[1] = (u8) temp;
		for (n = 1; n < (count + 1) / 3; n++) {
			sscanf(p, "%02X ", &temp);
			p += 3;
			fromIDtoMask((u8) temp, &mask[2], GESTURE_MASK_SIZE);

		}

		for (n = 0; n < GESTURE_MASK_SIZE + 2; n++) {
			logError(1, "%s mask[%d] = %02X \n", tag, n, mask[n]);

		}

	}

	if (mask[0] == 0) {
		res = ERROR_OP_NOT_ALLOW;
		logError(1,
			 "%s %s: Call before echo enable/disable xx xx .... > gesture_mask with a correct number of parameters! ERROR %08X \n",
			 tag, __func__, res);
	} else {

		if (mask[1] == FEAT_ENABLE || mask[1] == FEAT_DISABLE)
			res = updateGestureMask(&mask[2], mask[0], mask[1]);
		else
			res = ERROR_OP_NOT_ALLOW;

		if (res < OK) {
			logError(1, "%s fts_gesture_mask_store: ERROR %08X \n",
				 tag, res);
		}

	}

	res = check_feature_feasibility(info, FEAT_SEL_GESTURE);
	temp = isAnyGestureActive();
	if (res >= OK || temp == FEAT_DISABLE) {
		info->gesture_enabled = temp;
	}
	res = fts_mode_handler(info, 0);

	return count;
}

#endif

/**
 * File node to read the coordinates of the last gesture drawn by the user \n
 * cat gesture_coordinates			to obtain the gesture coordinates \n
 * the string returned in the shell follow this up as follow: \n
 * { = start byte \n
 * X1X2X3X4 = 4 bytes in HEX format which represent an error code (00000000 no error) \n
 * \n if error code = 00000000 \n
 * CC = 1 byte in HEX format number of coords (pair of x,y) returned \n
 * XXiYYi ... = XXi 2 bytes in HEX format for x[i] and YYi 2 bytes in HEX format for y[i] (big endian) \n
 * \n
 * } = end byte
 */
static ssize_t fts_gesture_coordinates_show(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	int size = (6 * 2) + 1, index = 0;
	u8 *all_strbuff = NULL;
	int count = 0, res, i = 0;

	logError(0, "%s %s: Getting gestures coordinates... \n", tag, __func__);

	if (gesture_coords_reported < OK) {
		logError(1, "%s %s: invalid coordinates! ERROR %08X \n", tag,
			 __func__, gesture_coords_reported);
		res = gesture_coords_reported;
	} else {
		size += gesture_coords_reported * 2 * 4 + 2;
		res = OK;
	}

	all_strbuff = (u8 *) kzalloc(size, GFP_KERNEL);
	if (all_strbuff != NULL) {

		snprintf(&all_strbuff[index], 11, "{ %08X", res);
		index += 10;

		if (res >= OK) {
			snprintf(&all_strbuff[index], 3, "%02X",
				 gesture_coords_reported);
			index += 2;

			for (i = 0; i < gesture_coords_reported; i++) {
				snprintf(&all_strbuff[index], 5, "%04X",
					 gesture_coordinates_x[i]);
				index += 4;
				snprintf(&all_strbuff[index], 5, "%04X",
					 gesture_coordinates_y[i]);
				index += 4;
			}
		}

		index += snprintf(&all_strbuff[index], 3, " }");

		count = snprintf(buf, TSP_BUF_SIZE, "%s\n", all_strbuff);
		kfree(all_strbuff);
		logError(0, "%s %s: Getting gestures coordinates FINISHED! \n",
			 tag, __func__);

	} else {
		logError(1,
			 "%s %s: Unable to allocate all_strbuff! ERROR %08X\n",
			 tag, ERROR_ALLOC);
	}

	return count;
}
#endif

/***************************************** PRODUCTION TEST ***************************************************/

/**
 * File node to execute the Mass Production Test or to get data from the IC (raw or ms/ss init data)
 * echo cmd > stm_fts_cmd		to execute a command \n
 * cat stm_fts_cmd				to show the result of the command \n
 * echo cmd > stm_fts_cmd; cat stm_fts_cmd 		to execute and show the result in just one call \n
 * the string returned in the shell is made up as follow: \n
 * { = start byte \n
 * X1X2X3X4 = 4 bytes in HEX format which represent an error_code (00000000 = OK)\n
 * (optional) data = data coming from the command executed represented as HEX string \n
 *                   Not all the command return additional data \n
 * } = end byte
 * \n
 * Possible commands (cmd): \n
 * - 00 = MP Test -> return erro_code \n
 * - 01 = ITO Test -> return error_code \n
 * - 03 = MS Raw Test -> return error_code \n
 * - 04 = MS Init Data Test -> return error_code \n
 * - 05 = SS Raw Test -> return error_code \n
 * - 06 = SS Init Data Test -> return error_code \n
 * - 13 = Read 1 MS Raw Frame -> return additional data: MS frame row after row \n
 * - 14 = Read MS Init Data -> return additional data: MS init data row after row \n
 * - 15 = Read 1 SS Raw Frame -> return additional data: SS frame, force channels followed by sense channels \n
 * - 16 = Read SS Init Data -> return additional data: SS Init data, first IX for force and sense channels and then CX for force and sense channels \n
 * - F0 = Perform a system reset -> return error_code \n
 * - F1 = Perform a system reset and reenable the sensing and the interrupt
 */
static ssize_t stm_fts_cmd_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	int n;
	char *p = (char *)buf;

	memset(typeOfComand, 0, CMD_STR_LEN * sizeof(u32));

	logError(1, "%s \n", tag);
	for (n = 0; n < (count + 1) / 3; n++) {
		sscanf(p, "%02X ", &typeOfComand[n]);
		p += 3;
		logError(1, "%s typeOfComand[%d] = %02X \n", tag, n,
			 typeOfComand[n]);

	}

	numberParameters = n;
	logError(1, "%s Number of Parameters = %d \n", tag, numberParameters);
	return count;
}

static ssize_t stm_fts_cmd_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int res, j, doClean = 0, count = 0, index = 0;
	char buff[CMD_STR_LEN] = { 0 };

	int size = (6 * 2) + 1;
	int init_type = SPECIAL_PANEL_INIT;
	u8 *all_strbuff = NULL;
	const char *limit_file_name = NULL;
	struct fts_ts_info *info = dev_get_drvdata(dev);

	MutualSenseData compData;
	SelfSenseData comData;
	MutualSenseFrame frameMS;
	SelfSenseFrame frameSS;

	if (numberParameters >= 1) {
		res = fts_disableInterrupt();
		if (res < 0) {
			logError(0, "%s fts_disableInterrupt: ERROR %08X \n",
				 tag, res);
			res = (res | ERROR_DISABLE_INTER);
			goto END;
		}
#ifdef CONFIG_DRM
		res = drm_unregister_client(&info->notifier);
		if (res < 0) {
			logError(1, "%s ERROR: unregister notifier failed!\n",
				 tag);
			goto END;
		}
#endif
		switch (typeOfComand[0]) {
			/*ITO TEST */
		case 0x01:
			res = production_test_ito(LIMITS_FILE, &tests);
			break;
			/*PRODUCTION TEST */
		case 0x00:

			if (systemInfo.u8_cfgAfeVer != systemInfo.u8_cxAfeVer) {
				res = ERROR_OP_NOT_ALLOW;
				logError(0,
					 "%s Miss match in CX version! MP test not allowed with wrong CX memory! ERROR %08X \n",
					 tag, res);
				break;
			}

			limit_file_name = fts_get_limit(info);
			res =
			    production_test_main(LIMITS_FILE, 1, init_type,
						 &tests);
			break;
			/*read mutual raw */
		case 0x13:
			logError(0, "%s Get 1 MS Frame \n", tag);
			setScanMode(SCAN_MODE_ACTIVE, 0x01);
			mdelay(WAIT_FOR_FRESH_FRAMES);
			setScanMode(SCAN_MODE_ACTIVE, 0x00);
			mdelay(WAIT_AFTER_SENSEOFF);
			flushFIFO();
			res = getMSFrame3(MS_RAW, &frameMS);
			if (res < 0) {
				logError(0,
					 "%s Error while taking the MS frame... ERROR %08X \n",
					 tag, res);

			} else {
				logError(0, "%s The frame size is %d words\n",
					 tag, res);
				size = (res * (sizeof(short) * 2 + 1)) + 10;
				res = OK;
				print_frame_short("MS frame =",
						  array1dTo2d_short
						  (frameMS.node_data,
						   frameMS.node_data_size,
						   frameMS.header.sense_node),
						  frameMS.header.force_node,
						  frameMS.header.sense_node);
			}
			break;
			/*read self raw */
		case 0x15:
			logError(0, "%s Get 1 SS Frame \n", tag);
			setScanMode(SCAN_MODE_ACTIVE, 0x01);
			mdelay(WAIT_FOR_FRESH_FRAMES);
			setScanMode(SCAN_MODE_ACTIVE, 0x00);
			mdelay(WAIT_AFTER_SENSEOFF);
			flushFIFO();
			res = getSSFrame3(SS_RAW, &frameSS);

			if (res < OK) {
				logError(0,
					 "%s Error while taking the SS frame... ERROR %08X \n",
					 tag, res);

			} else {
				logError(0, "%s The frame size is %d words\n",
					 tag, res);
				size = (res * (sizeof(short) * 2 + 1)) + 10;
				res = OK;
				print_frame_short("SS force frame =",
						  array1dTo2d_short
						  (frameSS.force_data,
						   frameSS.header.force_node,
						   1),
						  frameSS.header.force_node, 1);
				print_frame_short("SS sense frame =",
						  array1dTo2d_short
						  (frameSS.sense_data,
						   frameSS.header.sense_node,
						   frameSS.header.sense_node),
						  1, frameSS.header.sense_node);
			}

			break;

		case 0x14:
			logError(0, "%s Get MS Compensation Data \n", tag);
			res =
			    readMutualSenseCompensationData(LOAD_CX_MS_TOUCH,
							    &compData);

			if (res < 0) {
				logError(0,
					 "%s Error reading MS compensation data ERROR %08X \n",
					 tag, res);
			} else {
				logError(0,
					 "%s MS Compensation Data Reading Finished! \n",
					 tag);
				size =
				    (compData.node_data_size * sizeof(u8)) * 3 +
				    1;
				print_frame_i8("MS Data (Cx2) =",
					       array1dTo2d_i8
					       (compData.node_data,
						compData.node_data_size,
						compData.header.sense_node),
					       compData.header.force_node,
					       compData.header.sense_node);
			}
			break;

		case 0x16:
			logError(0, "%s Get SS Compensation Data... \n", tag);
			res =
			    readSelfSenseCompensationData(LOAD_CX_SS_TOUCH,
							  &comData);
			if (res < 0) {
				logError(0,
					 "%s Error reading SS compensation data ERROR %08X\n",
					 tag, res);
			} else {
				logError(0,
					 "%s SS Compensation Data Reading Finished! \n",
					 tag);
				size =
				    ((comData.header.force_node +
				      comData.header.sense_node) * 2 +
				     12) * sizeof(u8) * 2 + 1;
				print_frame_u8("SS Data Ix2_fm = ",
					       array1dTo2d_u8(comData.ix2_fm,
							      comData.
							      header.force_node,
							      1),
					       comData.header.force_node, 1);
				print_frame_i8("SS Data Cx2_fm = ",
					       array1dTo2d_i8(comData.cx2_fm,
							      comData.
							      header.force_node,
							      1),
					       comData.header.force_node, 1);
				print_frame_u8("SS Data Ix2_sn = ",
					       array1dTo2d_u8(comData.ix2_sn,
							      comData.
							      header.sense_node,
							      comData.
							      header.sense_node),
					       1, comData.header.sense_node);
				print_frame_i8("SS Data Cx2_sn = ",
					       array1dTo2d_i8(comData.cx2_sn,
							      comData.
							      header.sense_node,
							      comData.
							      header.sense_node),
					       1, comData.header.sense_node);
			}
			break;

		case 0x03:
			res = fts_system_reset();
			if (res >= OK)
				res =
				    production_test_ms_raw(LIMITS_FILE, 1,
							   &tests);
			break;

		case 0x04:
			res = fts_system_reset();
			if (res >= OK)
				res =
				    production_test_ms_cx(LIMITS_FILE, 1,
							  &tests);
			break;

		case 0x05:
			res = fts_system_reset();
			if (res >= OK)
				res =
				    production_test_ss_raw(LIMITS_FILE, 1,
							   &tests);
			break;

		case 0x06:
			res = fts_system_reset();
			if (res >= OK)
				res =
				    production_test_ss_ix_cx(LIMITS_FILE, 1,
							     &tests);
			break;

		case 0xF0:
		case 0xF1:
			doClean = (int)(typeOfComand[0] & 0x01);
			res = cleanUp(doClean);
			break;

		default:
			logError(1,
				 "%s COMMAND NOT VALID!! Insert a proper value ...\n",
				 tag);
			res = ERROR_OP_NOT_ALLOW;
			break;
		}

		doClean = fts_mode_handler(info, 1);
		if (typeOfComand[0] != 0xF0)
			doClean |= fts_enableInterrupt();
		if (doClean < 0) {
			logError(0, "%s %s: ERROR %08X \n", tag, __func__,
				 (doClean | ERROR_ENABLE_INTER));
		}
	} else {
		logError(1,
			 "%s NO COMMAND SPECIFIED!!! do: 'echo [cmd_code] [args] > stm_fts_cmd' before looking for result!\n",
			 tag);
		res = ERROR_OP_NOT_ALLOW;

	}
#ifdef CONFIG_DRM
	if (drm_register_client(&info->notifier) < 0) {
		logError(1, "%s ERROR: register notifier failed!\n", tag);
	}
#endif
END:
	all_strbuff = (u8 *) kzalloc(size, GFP_KERNEL);

	if (res >= OK) {
		/*all the other cases are already fine printing only the res. */
		switch (typeOfComand[0]) {
		case 0x13:
			snprintf(all_strbuff, size, "ms_frame\n");
			for (j = 0; j < frameMS.node_data_size; j++) {
				if ((j + 1) % frameMS.header.sense_node)
					snprintf(buff, sizeof(buff), "%04d ",
						 frameMS.node_data[j]);
				else
					snprintf(buff, sizeof(buff), "%04d\n",
						 frameMS.node_data[j]);

				strlcat(all_strbuff, buff, size);
			}

			kfree(frameMS.node_data);
			frameMS.node_data = NULL;
			break;

		case 0x15:
			snprintf(all_strbuff, size, "ss_frame\n");
			for (j = 0; j < frameSS.header.force_node - 1; j++) {
				snprintf(buff, sizeof(buff), "%04d ",
					 frameSS.force_data[j]);
				strlcat(all_strbuff, buff, size);
			}

			if (j == frameSS.header.force_node - 1) {
				snprintf(buff, sizeof(buff), "%04d\n",
					 frameSS.force_data[j]);
				strlcat(all_strbuff, buff, size);
			}

			for (j = 0; j < frameSS.header.sense_node - 1; j++) {
				snprintf(buff, sizeof(buff), "%04d ",
					 frameSS.sense_data[j]);
				strlcat(all_strbuff, buff, size);
			}

			if (j == frameSS.header.sense_node - 1) {
				snprintf(buff, sizeof(buff), "%04d\n",
					 frameSS.sense_data[j]);
				strlcat(all_strbuff, buff, size);
			}

			kfree(frameSS.force_data);
			kfree(frameSS.sense_data);

			break;

		case 0x14:
			snprintf(buff, sizeof(buff), "%02X",
				 (u8) compData.header.force_node);
			strlcat(all_strbuff, buff, size);
			snprintf(buff, sizeof(buff), "%02X",
				 (u8) compData.header.sense_node);
			strlcat(all_strbuff, buff, size);
			snprintf(buff, sizeof(buff), "%02X", compData.cx1);
			strlcat(all_strbuff, buff, size);

			for (j = 0; j < compData.node_data_size; j++) {
				snprintf(buff, sizeof(buff), "%02X",
					 *(compData.node_data + j));
				strlcat(all_strbuff, buff, size);
			}

			kfree(compData.node_data);
			compData.node_data = NULL;

			break;

		case 0x16:
			snprintf(buff, sizeof(buff), "%02X",
				 comData.header.force_node);
			strlcat(all_strbuff, buff, size);
			snprintf(buff, sizeof(buff), "%02X",
				 comData.header.sense_node);
			strlcat(all_strbuff, buff, size);
			snprintf(buff, sizeof(buff), "%02X", comData.f_ix1);
			strlcat(all_strbuff, buff, size);
			snprintf(buff, sizeof(buff), "%02X", comData.s_ix1);
			strlcat(all_strbuff, buff, size);
			snprintf(buff, sizeof(buff), "%02X", comData.f_cx1);
			strlcat(all_strbuff, buff, size);
			snprintf(buff, sizeof(buff), "%02X", comData.s_cx1);
			strlcat(all_strbuff, buff, size);

			for (j = 0; j < comData.header.force_node; j++) {
				snprintf(buff, sizeof(buff), "%02X",
					 comData.ix2_fm[j]);
				strlcat(all_strbuff, buff, size);
			}

			for (j = 0; j < comData.header.sense_node; j++) {
				snprintf(buff, sizeof(buff), "%02X",
					 comData.ix2_sn[j]);
				strlcat(all_strbuff, buff, size);
			}

			for (j = 0; j < comData.header.force_node; j++) {
				snprintf(buff, sizeof(buff), "%02X",
					 comData.cx2_fm[j]);
				strlcat(all_strbuff, buff, size);
			}

			for (j = 0; j < comData.header.sense_node; j++) {
				snprintf(buff, sizeof(buff), "%02X",
					 comData.cx2_sn[j]);
				strlcat(all_strbuff, buff, size);
			}

			kfree(comData.ix2_fm);
			kfree(comData.ix2_sn);
			kfree(comData.cx2_fm);
			kfree(comData.cx2_sn);

			break;

		default:
			snprintf(&all_strbuff[index], 11, "{ %08X", res);
			index += 10;
			snprintf(&all_strbuff[index], 3, " }");
			index += 2;

			break;

		}
	} else {
		snprintf(&all_strbuff[index], 11, "{ %08X", res);
		index += 10;
		snprintf(&all_strbuff[index], 3, " }");
		index += 2;
	}

	count = snprintf(buf, TSP_BUF_SIZE, "%s\n", all_strbuff);
	numberParameters = 0;
	kfree(all_strbuff);

	return count;
}

static ssize_t fts_panel_color_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct fts_ts_info *info = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%c\n", info->lockdown_info[2]);
}

static ssize_t fts_panel_vendor_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct fts_ts_info *info = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%c\n", info->lockdown_info[6]);
}

static ssize_t fts_lockdown_info_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct fts_ts_info *info = dev_get_drvdata(dev);
	int ret;
	ret = fts_get_lockdown_info(info->lockdown_info, info);

	if (ret != OK) {
		logError(1, "%s get lockdown info error\n", tag);
		return 0;
	}

	return snprintf(buf, PAGE_SIZE,
			"0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x\n",
			info->lockdown_info[0], info->lockdown_info[1],
			info->lockdown_info[2], info->lockdown_info[3],
			info->lockdown_info[4], info->lockdown_info[5],
			info->lockdown_info[6], info->lockdown_info[7]);
}

static ssize_t fts_lockdown_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	int n, i, ret;
	char *p = (char *)buf;
	u8 *typecomand = NULL;

	memset(typeOfComand, 0, CMD_STR_LEN * sizeof(u32));
	logError(1, "%s \n", tag);
	for (n = 0; n < (count + 1) / 3; n++) {
		sscanf(p, "%02X ", &typeOfComand[n]);
		p += 3;
		logError(1, "%s command_sequence[%d] = %02X\n", tag, n,
			 typeOfComand[n]);
	}
	numberParameters = n;
	if (numberParameters < 3)
		goto END;
	logError(1, "%s %d = %d \n", tag, n, numberParameters);

	typecomand =
	    (u8 *) kmalloc((numberParameters - 2) * sizeof(u8), GFP_KERNEL);
	if (typecomand != NULL) {
		for (i = 0; i < numberParameters - 2; i++) {
			typecomand[i] = (u8) typeOfComand[i + 2];
			logError(1, "%s typecomand[%d] = %X \n", tag, i,
				 typecomand[i]);
		}
	} else {
		goto END;
	}

	ret =
	    writeLockDownInfo(typecomand, numberParameters - 2,
			      typeOfComand[0]);
	if (ret < 0) {
		logError(1, "%s fts_lockdown_store failed\n", tag);
	}
	kfree(typecomand);
END:
	logError(1, "%s Number of Parameters = %d \n", tag, numberParameters);

	return count;
}

static ssize_t fts_lockdown_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int i, ret;
	int size = 0, count = 0;
	u8 type;
	u8 *temp_buffer = NULL;

	temp_buffer = (u8 *) kmalloc(LOCKDOWN_LENGTH * sizeof(u8), GFP_KERNEL);
	if (temp_buffer == NULL || numberParameters < 2) {
		count +=
		    snprintf(&buf[count], PAGE_SIZE, "prepare read lockdown failded\n");
		return count;
	}
	type = typeOfComand[0];
	size = (int)(typeOfComand[1]);
	count += snprintf(&buf[count], PAGE_SIZE, "read lock down code:\n");
	ret = readLockDownInfo(temp_buffer, type, size);
	if (ret < OK) {
		count += snprintf(&buf[count], PAGE_SIZE, "read lockdown failded\n");
		goto END;
	}
	for (i = 0; i < size; i++) {
		count += snprintf(&buf[count], PAGE_SIZE, "%02X ", temp_buffer[i]);
	}
	count += snprintf(&buf[count], PAGE_SIZE, "\n");

END:
	numberParameters = 0;
	kfree(temp_buffer);
	return count;
}

static ssize_t fts_selftest_info_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	int res = 0, i = 0, count = 0, force_node = 0, sense_node = 0, pos =
	    0, last_pos = 0;
	MutualSenseFrame frameMS;
	char buff[80];
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	res = fts_disableInterrupt();
	if (res < OK)
		goto END;

	setScanMode(SCAN_MODE_ACTIVE, 0x01);
	mdelay(WAIT_FOR_FRESH_FRAMES);
	setScanMode(SCAN_MODE_ACTIVE, 0x00);
	mdelay(WAIT_AFTER_SENSEOFF);
	flushFIFO();
	res = getMSFrame3(MS_RAW, &frameMS);
	if (res < 0) {
		logError(0,
			 "%s Error while taking the MS frame... ERROR %08X \n",
			 tag, res);
		goto END;
	}
	fts_mode_handler(info, 1);

	sense_node = frameMS.header.sense_node;
	force_node = frameMS.header.force_node;

	for (i = 0; i < RELEASE_INFO_SIZE; i++) {
		if (i == 0) {
			pos +=
			    snprintf(buff + last_pos, PAGE_SIZE, "0x%02x",
				     systemInfo.u8_releaseInfo[i]);
			last_pos = pos;
		} else {
			pos +=
			    snprintf(buff + last_pos, PAGE_SIZE, "%02x",
				     systemInfo.u8_releaseInfo[i]);
			last_pos = pos;
		}
	}
	count =
	    snprintf(buf, PAGE_SIZE,
		     "Device address:,0x49\nChip Id:,0x%04x\nFw version:,0x%04x\nConfig version:,0x%04x\nChip serial number:,%s\nForce lines count:,%02d\nSense lines count:,%02d\n\n",
		     systemInfo.u16_chip0Id, systemInfo.u16_fwVer,
		     systemInfo.u16_cfgVer, buff, force_node, sense_node);
END:
	fts_enableInterrupt();
	return count;

}

static ssize_t fts_ms_raw_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	int res = 0, count = 0, j = 0, sense_node = 0, force_node = 0, pos =
	    0, last_pos = 0;
	char *all_strbuff = NULL;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);
	MutualSenseFrame frameMS;

	res = fts_disableInterrupt();
	if (res < OK)
		goto END;
	all_strbuff = vmalloc(PAGE_SIZE);
	if (!all_strbuff) {
		logError(1, "%s %s alloc all_strbuff fail\n", tag, __func__);
		goto END;
	} else
		memset(all_strbuff, 0, PAGE_SIZE);

	setScanMode(SCAN_MODE_ACTIVE, 0x01);
	mdelay(WAIT_FOR_FRESH_FRAMES);
	setScanMode(SCAN_MODE_ACTIVE, 0x00);
	mdelay(WAIT_AFTER_SENSEOFF);
	flushFIFO();
	res = getMSFrame3(MS_RAW, &frameMS);

	fts_mode_handler(info, 1);
	sense_node = frameMS.header.sense_node;
	force_node = frameMS.header.force_node;
	pos +=
	    snprintf(all_strbuff + last_pos, PAGE_SIZE,
		     "MsTouchRaw,%2d,%2d\n ,", force_node, sense_node);
	last_pos = pos;
	if (res >= OK) {
		for (j = 0; j < sense_node; j++)
			if ((j + 1) % sense_node) {
				pos +=
				    snprintf(all_strbuff + last_pos, PAGE_SIZE,
					     "C%02d,", j);
				last_pos = pos;
			} else {
				pos +=
				    snprintf(all_strbuff + last_pos, PAGE_SIZE,
					     "C%02d\nR00,", j);
				last_pos = pos;
			}
		for (j = 0; j < sense_node * force_node; j++) {
			if ((j + 1) % sense_node) {
				pos +=
				    snprintf(all_strbuff + last_pos, PAGE_SIZE,
					     "%4d,", frameMS.node_data[j]);
				last_pos = pos;
			} else {
				if ((j + 1) / sense_node != force_node)
					pos +=
					    snprintf(all_strbuff + last_pos,
						     PAGE_SIZE, "%4d\nR%02d,",
						     frameMS.node_data[j],
						     (j + 1) / sense_node);
				else
					pos +=
					    snprintf(all_strbuff + last_pos,
						     PAGE_SIZE, "%4d\n",
						     frameMS.node_data[j]);
				last_pos = pos;
			}
		}
		if (frameMS.node_data) {
			kfree(frameMS.node_data);
			frameMS.node_data = NULL;
		}
	}

	count = snprintf(buf, PAGE_SIZE, "%s\n", all_strbuff);
	vfree(all_strbuff);
END:
	fts_enableInterrupt();
	return count;
}

static ssize_t fts_ms_cx_total_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	int res = 0, pos = 0, last_pos = 0, count = 0, j = 0, sense_node =
	    0, force_node = 0;
	char *all_strbuff = NULL;
	TotMutualSenseData totCompData;

	res = fts_disableInterrupt();
	if (res < OK)
		goto END;
	all_strbuff = vmalloc(PAGE_SIZE);
	if (!all_strbuff) {
		logError(1, "%s %s alloc all_strbuff fail\n", tag, __func__);
		goto END;
	} else
		memset(all_strbuff, 0, PAGE_SIZE);

	res =
	    readTotMutualSenseCompensationData(LOAD_PANEL_CX_TOT_MS_TOUCH,
					       &totCompData);
	if (res >= OK) {
		sense_node = totCompData.header.sense_node;
		force_node = totCompData.header.force_node;
		pos +=
		    snprintf(all_strbuff + last_pos, PAGE_SIZE,
			     "MsTouchTotalCx,%2d,%2d\n ,", force_node,
			     sense_node);
		last_pos = pos;
		for (j = 0; j < sense_node; j++)
			if ((j + 1) % sense_node) {
				pos +=
				    snprintf(all_strbuff + last_pos, PAGE_SIZE,
					     "C%02d,", j);
				last_pos = pos;
			} else {
				pos +=
				    snprintf(all_strbuff + last_pos, PAGE_SIZE,
					     "C%02d\nR00,", j);
				last_pos = pos;
			}
		for (j = 0; j < sense_node * force_node; j++) {
			if ((j + 1) % sense_node) {
				pos +=
				    snprintf(all_strbuff + last_pos, PAGE_SIZE,
					     "%4d,", totCompData.node_data[j]);
				last_pos = pos;
			} else {
				if ((j + 1) / sense_node != force_node)
					pos +=
					    snprintf(all_strbuff + last_pos,
						     PAGE_SIZE, "%4d\nR%02d,",
						     totCompData.node_data[j],
						     (j + 1) / sense_node);
				else
					pos +=
					    snprintf(all_strbuff + last_pos,
						     PAGE_SIZE, "%4d\n",
						     totCompData.node_data[j]);
				last_pos = pos;
			}
		}
		if (totCompData.node_data) {
			kfree(totCompData.node_data);
			totCompData.node_data = NULL;
		}
	}

	count = snprintf(buf, PAGE_SIZE, "%s\n", all_strbuff);
	vfree(all_strbuff);
END:
	fts_enableInterrupt();
	return count;

}

static ssize_t fts_ss_ix_total_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	int ret = 0, pos = 0, last_pos = 0, count = 0, j = 0, sense_node =
	    0, force_node = 0;
	char *all_strbuff = NULL;
	TotSelfSenseData totCompData;

	ret = fts_disableInterrupt();
	if (ret < OK)
		goto END;
	all_strbuff = vmalloc(PAGE_SIZE);
	if (!all_strbuff) {
		logError(1, "%s %s alloc all_strbuff fail\n", tag, __func__);
		goto END;
	} else {
		memset(all_strbuff, 0, PAGE_SIZE);
	}
	ret =
	    readTotSelfSenseCompensationData(LOAD_PANEL_CX_TOT_SS_TOUCH,
					     &totCompData);
	if (ret < 0) {
		logError(1,
			 "%s production_test_data: readTotSelfSenseCompensationData failed... ERROR %08X \n",
			 tag, ERROR_PROD_TEST_DATA);
		goto END;
	}

	sense_node = 1;
	force_node = totCompData.header.force_node;

	pos +=
	    snprintf(all_strbuff + last_pos, PAGE_SIZE,
		     "SsTouchForceTotalIx,%2d,1\n ,C00\n", force_node);
	last_pos = pos;
	for (j = 0; j < force_node; j++) {
		pos +=
		    snprintf(all_strbuff + last_pos, PAGE_SIZE, "R%02d,%4d\n",
			     j, totCompData.ix_fm[j]);
		last_pos = pos;
	}

	pos +=
	    snprintf(all_strbuff + last_pos, PAGE_SIZE,
		     "SsTouchForceTotalCx,%2d,1\n ,C00\n", force_node);
	last_pos = pos;
	for (j = 0; j < force_node; j++) {
		pos +=
		    snprintf(all_strbuff + last_pos, PAGE_SIZE, "R%02d,%4d\n",
			     j, totCompData.cx_fm[j]);
		last_pos = pos;
	}

	sense_node = totCompData.header.sense_node;
	force_node = 1;

	pos +=
	    snprintf(all_strbuff + last_pos, PAGE_SIZE,
		     "SsTouchsenseTotalIx,%2d,1\n ,C00\n", sense_node);
	last_pos = pos;
	for (j = 0; j < sense_node; j++) {
		pos +=
		    snprintf(all_strbuff + last_pos, PAGE_SIZE, "R%02d,%4d\n",
			     j, totCompData.ix_sn[j]);
		last_pos = pos;
	}

	pos +=
	    snprintf(all_strbuff + last_pos, PAGE_SIZE,
		     "SsTouchsenseTotalCx,%2d,1\n ,C00\n", sense_node);
	last_pos = pos;
	for (j = 0; j < sense_node; j++) {
		pos +=
		    snprintf(all_strbuff + last_pos, PAGE_SIZE, "R%02d,%4d\n",
			     j, totCompData.cx_sn[j]);
		last_pos = pos;
	}

	if (totCompData.ix_fm != NULL) {
		kfree(totCompData.ix_fm);
		totCompData.ix_fm = NULL;
	}

	if (totCompData.cx_fm != NULL) {
		kfree(totCompData.cx_fm);
		totCompData.cx_fm = NULL;
	}

	if (totCompData.ix_sn != NULL) {
		kfree(totCompData.ix_sn);
		totCompData.ix_sn = NULL;
	}

	if (totCompData.cx_sn != NULL) {
		kfree(totCompData.cx_sn);
		totCompData.cx_sn = NULL;
	}

	count = snprintf(buf, PAGE_SIZE, "%s\n", all_strbuff);
	vfree(all_strbuff);
END:
	fts_enableInterrupt();
	return count;
}

static ssize_t fts_ss_raw_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	int res = 0, count = 0, j = 0, sense_node = 0, force_node = 0, pos =
	    0, last_pos = 0;
	char *all_strbuff = NULL;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);
	SelfSenseFrame frameSS;

	res = fts_disableInterrupt();
	if (res < OK)
		goto END;
	all_strbuff = vmalloc(PAGE_SIZE * 4);
	if (!all_strbuff) {
		logError(1, "%s %s alloc all_strbuff fail\n", tag, __func__);
		goto END;
	} else
		memset(all_strbuff, 0, PAGE_SIZE);
	setScanMode(SCAN_MODE_ACTIVE, 0x01);
	mdelay(WAIT_FOR_FRESH_FRAMES);
	setScanMode(SCAN_MODE_ACTIVE, 0x00);
	mdelay(WAIT_AFTER_SENSEOFF);
	flushFIFO();
	res = getSSFrame3(SS_RAW, &frameSS);

	fts_mode_handler(info, 1);
	sense_node = frameSS.header.sense_node;
	force_node = frameSS.header.force_node;
	pos +=
	    snprintf(all_strbuff + last_pos, PAGE_SIZE, "SsTouchRaw,%2d,%2d\n",
		     force_node, sense_node);
	last_pos = pos;
	if (res >= OK) {
		pos +=
		    snprintf(all_strbuff + last_pos, PAGE_SIZE,
			     "SS force frame\n ,");
		last_pos = pos;

		for (j = 0; j < frameSS.header.force_node - 1; j++) {
			pos +=
			    snprintf(all_strbuff + last_pos, PAGE_SIZE, "%04d,",
				     frameSS.force_data[j]);
			last_pos = pos;
		}

		if (j == frameSS.header.force_node - 1) {
			pos +=
			    snprintf(all_strbuff + last_pos, PAGE_SIZE,
				     "%04d\n", frameSS.force_data[j]);
			last_pos = pos;
		}

		pos +=
		    snprintf(all_strbuff + last_pos, PAGE_SIZE,
			     "SS sense frame\n ,");
		last_pos = pos;

		for (j = 0; j < frameSS.header.sense_node - 1; j++) {
			pos +=
			    snprintf(all_strbuff + last_pos, PAGE_SIZE, "%04d,",
				     frameSS.sense_data[j]);
			last_pos = pos;
		}

		if (j == frameSS.header.sense_node - 1) {
			pos +=
			    snprintf(all_strbuff + last_pos, PAGE_SIZE,
				     "%04d\n", frameSS.sense_data[j]);
			last_pos = pos;
		}

		if (frameSS.force_data) {
			kfree(frameSS.force_data);
			frameSS.force_data = NULL;
		}
		if (frameSS.sense_data) {
			kfree(frameSS.sense_data);
			frameSS.sense_data = NULL;
		}

	}

	count = snprintf(buf, PAGE_SIZE, "%s\n", all_strbuff);
	vfree(all_strbuff);
END:
	fts_enableInterrupt();
	return count;
}

static ssize_t fts_strength_frame_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	MutualSenseFrame frame;
	int res = 0, count = 0, j = 0, size = 0;
	char *all_strbuff = NULL;
	char buff[CMD_STR_LEN] = { 0 };
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);
	frame.node_data = NULL;

	res = fts_disableInterrupt();
	if (res < OK)
		goto END;

	res = getMSFrame3(MS_STRENGTH, &frame);

	if (res < OK) {
		logError(1, "%s %s: could not get the frame! ERROR %08X \n",
			 tag, __func__, res);
		goto END;
	}
	size = (res * 5) + 11;

	/*
	   flushFIFO();
	 */
	fts_mode_handler(info, 1);
	all_strbuff = (char *)kmalloc(PAGE_SIZE * sizeof(char), GFP_KERNEL);

	if (all_strbuff != NULL) {
		memset(all_strbuff, 0, size);
		snprintf(all_strbuff, size, "ms_differ\n");
		if (res >= OK) {
			for (j = 0; j < frame.node_data_size; j++) {
				if ((j + 1) % frame.header.sense_node)
					snprintf(buff, sizeof(buff), "%4d,",
						 frame.node_data[j]);
				else
					snprintf(buff, sizeof(buff), "%4d\n",
						 frame.node_data[j]);

				strlcat(all_strbuff, buff, size);
			}

			kfree(frame.node_data);
			frame.node_data = NULL;
		}

		count = snprintf(buf, PAGE_SIZE, "%s\n", all_strbuff);
		kfree(all_strbuff);
	} else {
		logError(1,
			 "%s %s: Unable to allocate all_strbuff! ERROR %08X\n",
			 tag, __func__, ERROR_ALLOC);
	}

END:
	fts_enableInterrupt();

	return count;
}

static ssize_t fts_doze_time_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct fts_ts_info *info = dev_get_drvdata(dev);

	return snprintf(buf, TSP_BUF_SIZE, "%u\n", info->doze_time);
}

static ssize_t fts_doze_time_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	u8 cmd[4] = {FTS_CMD_CUSTOM, 0x00, 0x00, 0x00};
	int ret = 0;
	u16 reg_val = 0;
	struct fts_ts_info *info = dev_get_drvdata(dev);

	pr_info("%s,buf:%s,count:%zu\n", __func__, buf, count);
	sscanf(buf, "%u", &info->doze_time);
	/*reg value * 10 represents of the num of frames ,one frame is about 8ms, the input value is ms*/
	reg_val = (info->doze_time / 8 - 1) / 10;
	cmd[3] = reg_val;
	ret = fts_write_dma_safe(cmd, ARRAY_SIZE(cmd));
	if (ret < OK) {
		logError(1, "%s %s: write failed...ERROR %08X !\n", tag,
			 __func__, ret);
		return -EPERM;
	}
	return count;
}

static ssize_t fts_grip_enable_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct fts_ts_info *info = dev_get_drvdata(dev);

	return snprintf(buf, TSP_BUF_SIZE, "%d\n", info->grip_enabled);
}

static ssize_t fts_grip_enable_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	u8 cmd[3] = {FTS_CMD_FEATURE, 0x04, 0x01};
	int ret = 0;
	struct fts_ts_info *info = dev_get_drvdata(dev);

	pr_info("%s,buf:%s,count:%zu\n", __func__, buf, count);
	sscanf(buf, "%u", &info->grip_enabled);
	cmd[2] = info->grip_enabled;
	ret = fts_write_dma_safe(cmd, ARRAY_SIZE(cmd));
	if (ret < OK) {
		logError(1, "%s %s: write failed...ERROR %08X !\n", tag,
			 __func__, ret);
		return -EPERM;
	}
	return count;
}

static ssize_t fts_grip_area_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct fts_ts_info *info = dev_get_drvdata(dev);

	return snprintf(buf, TSP_BUF_SIZE, "%d\n", info->grip_pixel);
}

static ssize_t fts_grip_area_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	u8 cmd[4] = {FTS_CMD_CUSTOM, 0x01, 0x01, 0x00};
	int ret = 0;
	struct fts_ts_info *info = dev_get_drvdata(dev);
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_GAMEMODE
	return -EPERM;
#endif
	logError(1, " %s %s,buf:%s,count:%zu\n", tag, __func__, buf, count);
	sscanf(buf, "%u", &info->grip_pixel);
	cmd[3] = info->grip_pixel;
	if (atomic_read(&info->system_is_resetting)) {
		logError(1, "%s %s system is resetting ,wait reset done\n", tag, __func__);
		ret = wait_for_completion_timeout(&info->tp_reset_completion, msecs_to_jiffies(40));
		if (!ret) {
			logError(1, "%s %s wait tp reset timeout, wrtie grip area error\n", tag, __func__);
			return count;
		}
	}
	ret = fts_write_dma_safe(cmd, ARRAY_SIZE(cmd));
	if (ret < OK) {
		logError(1, "%s %s: write failed...ERROR %08X !\n", tag,
			 __func__, ret);
		return -EPERM;
	}
	return count;
}

static ssize_t fts_fod_status_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct fts_ts_info *info = dev_get_drvdata(dev);

	return snprintf(buf, TSP_BUF_SIZE, "%d\n", info->fod_status);
}

static ssize_t fts_fod_status_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct fts_ts_info *info = dev_get_drvdata(dev);

	logError(1, " %s %s,buf:%s,count:%zu\n", tag, __func__, buf, count);
	sscanf(buf, "%u", &info->fod_status);
	return count;
}

#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE
static struct xiaomi_touch_interface xiaomi_touch_interfaces;
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_GAMEMODE
static ssize_t fts_touchgame_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	u8 get_value[7] = {0x0,};
	u8 get_cmd[2] = {0xc1, 0x05};
	u8 grip_rcmd[2] = {0xc1, 0x08};
	u8 grip_value[7] = {0x0,};
	int ret;

	ret = fts_writeRead(get_cmd, sizeof(get_cmd) / sizeof(u8), get_value,
				 sizeof(get_value) / sizeof(u8));
	if (ret < OK) {
		logError(1,
			 "%s %s: error while reading touchmode data ERROR %08X\n",
			 tag, __func__, ret);
	}
	ret = fts_writeRead(grip_rcmd, sizeof(grip_rcmd) / sizeof(u8), grip_value,
			     sizeof(grip_value) / sizeof(u8));
	if (ret < OK) {
		logError(1,
			 "%s %s: error while reading edge filter data ERROR %08X\n",
			 tag, __func__, ret);
	}

	return 	snprintf(buf, PAGE_SIZE, "game mode:%d,%d,%d,%d,%d,%d,%d\n"
		"grip mode:0x%x, 0x%x, 0x%x, %d, %d, %d, %d\n",
		get_value[0], get_value[1], get_value[2], get_value[3],
		get_value[4], get_value[5], get_value[6],
		grip_value[0], grip_value[1], grip_value[2], grip_value[3],
		grip_value[4], grip_value[5], grip_value[6]);
}

static int fts_read_touchmode_data(void)
{
	int ret = 0;
	u8 get_cmd[2] = {0xc1, 0x05};
	u8 get_value[Touch_Mode_NUM] = {0x0,};
	u8 grip_cmd[2] = {0xc1, 0x08};
	u8 grip_value[Touch_Mode_NUM] = {0x0,};
	int readBytes = 7;
	int i;

	ret = fts_writeRead(get_cmd, sizeof(get_cmd) / sizeof(u8), get_value,
			     readBytes);
	if (ret < OK) {
		logError(1,
			 "%s %s: error while reading touchmode data ERROR %08X\n",
			 tag, __func__, ret);
		return -EINVAL;
	}
	ret = fts_writeRead(grip_cmd, sizeof(grip_cmd) / sizeof(u8), grip_value,
			     readBytes);
	if (ret < OK) {
		logError(1,
			 "%s %s: error while reading edge filter data ERROR %08X\n",
			 tag, __func__, ret);
		return -EINVAL;
	}

	for (i = 0; i < Touch_Mode_NUM; i++) {
		if (i == Touch_Edge_Filter)
			xiaomi_touch_interfaces.touch_mode[i][GET_CUR_VALUE] = grip_value[0];
		else
			xiaomi_touch_interfaces.touch_mode[i][GET_CUR_VALUE] = get_value[i];
	}

	for (i = 0; i < 7; i++) {
		xiaomi_touch_interfaces.touch_edge[i] = grip_value[i];
	}

	logError(1,
		"%s %s: game_mode:%d, active_mode:%d, up_threshold:%d, landlock:%d, wgh:%d, %d, %d\n",
		tag, __func__, get_value[0], get_value[1], get_value[2], get_value[3],
		get_value[4], get_value[5], get_value[6]);
	logError(1,
		"%s %s: grip:0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
		tag, __func__, grip_value[0], grip_value[1], grip_value[2], grip_value[3],
		grip_value[4], grip_value[5], grip_value[6]);

	return ret;
}

static void fts_init_touchmode_data(void)
{
	int i;
	struct fts_hw_platform_data *bdata = NULL;

	if (!fts_info)
		return;
	else
		bdata = fts_info->board;

	/* default value should equl the first initial value */
	for (i = 0; i < Touch_Mode_NUM; i++) {
		xiaomi_touch_interfaces.touch_mode[i][GET_DEF_VALUE] =
			xiaomi_touch_interfaces.touch_mode[i][GET_CUR_VALUE];
		xiaomi_touch_interfaces.touch_mode[i][SET_CUR_VALUE] =
			xiaomi_touch_interfaces.touch_mode[i][GET_CUR_VALUE];
	}
	/* Touch Game Mode Switch */
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_MAX_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_DEF_VALUE] = 0;

	/* Acitve Mode */
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][GET_MAX_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][GET_DEF_VALUE] = 0;

	/* finger hysteresis */
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_DEF_VALUE] = bdata->touch_up_threshold_def;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][SET_CUR_VALUE] = bdata->touch_up_threshold_def;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_CUR_VALUE] = bdata->touch_up_threshold_def;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_MAX_VALUE] = bdata->touch_up_threshold_max;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_MIN_VALUE] = bdata->touch_up_threshold_min;

	/*  Tolerance */
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_DEF_VALUE] = bdata->touch_tolerance_def;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_MAX_VALUE] = bdata->touch_tolerance_max;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_MIN_VALUE] = bdata->touch_tolerance_min;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][SET_CUR_VALUE] = bdata->touch_tolerance_def;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_CUR_VALUE] = bdata->touch_tolerance_def;

	/*	Wgh Min */
	xiaomi_touch_interfaces.touch_mode[Touch_Wgh_Min][GET_DEF_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Wgh_Min][GET_CUR_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Wgh_Min][SET_CUR_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Wgh_Min][GET_MAX_VALUE] = 15;
	xiaomi_touch_interfaces.touch_mode[Touch_Wgh_Min][GET_MIN_VALUE] = 0;

	/*	Wgh Max */
	xiaomi_touch_interfaces.touch_mode[Touch_Wgh_Max][GET_DEF_VALUE] = 5;
	xiaomi_touch_interfaces.touch_mode[Touch_Wgh_Max][GET_CUR_VALUE] = 5;
	xiaomi_touch_interfaces.touch_mode[Touch_Wgh_Max][SET_CUR_VALUE] = 5;
	xiaomi_touch_interfaces.touch_mode[Touch_Wgh_Max][GET_MAX_VALUE] = 15;
	xiaomi_touch_interfaces.touch_mode[Touch_Wgh_Max][GET_MIN_VALUE] = 0;

	/*	Wgh Step */
	xiaomi_touch_interfaces.touch_mode[Touch_Wgh_Step][GET_DEF_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Wgh_Step][GET_CUR_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Wgh_Step][SET_CUR_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Wgh_Step][GET_MAX_VALUE] = 2;
	xiaomi_touch_interfaces.touch_mode[Touch_Wgh_Step][GET_MIN_VALUE] = 0;

	/*	edge filter */
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_MAX_VALUE] = 3;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_DEF_VALUE] = 2;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][SET_CUR_VALUE] = 2;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_CUR_VALUE] = 2;

	/*	Orientation */
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_MAX_VALUE] = 3;
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_DEF_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_CUR_VALUE] = 0;

	for (i = 0; i < Touch_Mode_NUM; i++) {
		logError(1,
			 "%s %s: mode:%d, set cur:%d, get cur:%d, def:%d min:%d max:%d\n",
			 tag, __func__,
			i,
			xiaomi_touch_interfaces.touch_mode[i][SET_CUR_VALUE],
			xiaomi_touch_interfaces.touch_mode[i][GET_CUR_VALUE],
			xiaomi_touch_interfaces.touch_mode[i][GET_DEF_VALUE],
			xiaomi_touch_interfaces.touch_mode[i][GET_MIN_VALUE],
			xiaomi_touch_interfaces.touch_mode[i][GET_MAX_VALUE]);
	}

	return;
}

static void fts_edge_rejection(bool on, int value)
{
	/* the fixed edge_reject is 65 */
	int ret = 0;
	u8 edge_of_leftright[4] = {0xc0, 0x01, 0x00, 0x00};
	u8 edge_of_topbottom[4] = {0xc0, 0x01, 0x01, 0x00};
	struct fts_hw_platform_data *pdata = NULL;

	if (!fts_info)
		return;
	else
		pdata = fts_info->board;

	if (on) {
		edge_of_topbottom[3] = value;
		edge_of_leftright[3] = 0;
	} else {
		edge_of_leftright[3] = pdata->edgefilter_leftright_def;
		edge_of_topbottom[3] = pdata->edgefilter_topbottom_def;
	}

	ret = fts_write(edge_of_leftright, sizeof(edge_of_leftright) / sizeof(u8));
	if (ret < OK) {
		logError(1,
			 "%s %s: error while writing edge left/right data ERROR %08X\n",
			 tag, __func__, ret);
		return;
	}

	ret = fts_write(edge_of_topbottom, sizeof(edge_of_topbottom) / sizeof(u8));
	if (ret < OK) {
		logError(1,
			 "%s %s: error while writing edge top/bottom data ERROR %08X\n",
			 tag, __func__, ret);
		return;
	}
	logError(1, "%s %s: on:%d, value:%d\n",
				 tag, __func__, on, value);

	return;
}

static void fts_update_grip_mode(void)
{
	int i, ret;
	u8 grip_cmd[9] = {0xc0, 0x08, 0x00,};
	u8 grip_rcmd[2] = {0xc1, 0x08};
	u8 grip_value[7] = {0x0,};
	int temp_value = 0;
	bool topbottom_edge_filter = false;
	int direction = 0;
	struct fts_hw_platform_data *pdata = NULL;

	if (!fts_info)
		return;
	else
		pdata = fts_info->board;

	logError(1,
			 "%s %s: value:%d\n",
			 tag, __func__, xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][SET_CUR_VALUE]);
	switch (xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][SET_CUR_VALUE]) {
	case 0:
		temp_value = 0;
		break;
	case 1:
		temp_value = pdata->edgefilter_area_step1;
		break;
	case 2:
		temp_value = pdata->edgefilter_area_step2;
		break;
	case 3:
		temp_value = pdata->edgefilter_area_step3;
		break;
	default:
		temp_value = 0;
		logError(1,
		 "%s %s: no support value %08d\n",
		 tag, __func__, temp_value);
		break;
	}

	direction = xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][SET_CUR_VALUE];

	switch (direction) {
	case 0:
	case 2:
		/* 0 and 180, filter the leftright edge */
		topbottom_edge_filter = false;
		xiaomi_touch_interfaces.touch_edge[0] = 0;
		xiaomi_touch_interfaces.touch_edge[1] = 0x0f;
		xiaomi_touch_interfaces.touch_edge[2] = 0x00;
		xiaomi_touch_interfaces.touch_edge[3] = 0;
		xiaomi_touch_interfaces.touch_edge[4] = 0;
		xiaomi_touch_interfaces.touch_edge[5] = 0;
		xiaomi_touch_interfaces.touch_edge[6] = 0;
		break;
	case 1:
		topbottom_edge_filter = true;
		xiaomi_touch_interfaces.touch_edge[0] = 1;
		xiaomi_touch_interfaces.touch_edge[1] = 0x0f;
		xiaomi_touch_interfaces.touch_edge[2] = 0x00;
		xiaomi_touch_interfaces.touch_edge[3] = temp_value;
		xiaomi_touch_interfaces.touch_edge[4] = temp_value;
		xiaomi_touch_interfaces.touch_edge[5] = 0;
		xiaomi_touch_interfaces.touch_edge[6] = 0;
		break;
	case 3:
		topbottom_edge_filter = true;
		xiaomi_touch_interfaces.touch_edge[0] = 1;
		xiaomi_touch_interfaces.touch_edge[1] = 0x0f;
		xiaomi_touch_interfaces.touch_edge[2] = 0x00;
		xiaomi_touch_interfaces.touch_edge[3] = 0;
		xiaomi_touch_interfaces.touch_edge[4] = 0;
		xiaomi_touch_interfaces.touch_edge[5] = temp_value;
		xiaomi_touch_interfaces.touch_edge[6] = temp_value;
		break;
	default:
			/* Dont't support */
			break;
	};

	if (!xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_CUR_VALUE] || !topbottom_edge_filter) {
		logError(1,
			 "%s %s: touch game mode off or portrait\n",
			 tag, __func__);
		fts_edge_rejection(false, 0);
	}

	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_CUR_VALUE] =
		xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][SET_CUR_VALUE];

	for (i = 0; i < 7; i++) {
		grip_cmd[2 + i] = (u8)(xiaomi_touch_interfaces.touch_edge[i]);
	}
	logError(1,
		"%s %s: write grip:0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
		tag, __func__, grip_cmd[0], grip_cmd[1], grip_cmd[2], grip_cmd[3],
		grip_cmd[4], grip_cmd[5], grip_cmd[6], grip_cmd[7], grip_cmd[8]);

	ret = fts_write(grip_cmd, sizeof(grip_cmd) / sizeof(u8));
	if (ret < OK) {
		logError(1,
			 "%s %s: error while writing touchmode data ERROR %08X\n",
			 tag, __func__, ret);
		return;
	}
	msleep(10);
	ret = fts_writeRead(grip_rcmd, sizeof(grip_rcmd) / sizeof(u8), grip_value,
			     sizeof(grip_value) / sizeof(u8));
	if (ret < OK) {
		logError(1,
			 "%s %s: error while reading edge filter data ERROR %08X\n",
			 tag, __func__, ret);
		return;
	}
	logError(1,
		"%s %s: read grip:0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
		tag, __func__, grip_value[0], grip_value[1], grip_value[2], grip_value[3],
		grip_value[4], grip_value[5], grip_value[6]);

	logError(1,
		 "%s %s: grip enable:%d, direction:%d\n",
		 tag, __func__, grip_value[0], direction);

	return;
}

static void fts_update_touchmode_data(void)
{
	bool update = false;
	int i, j, ret = 0;
	u8 set_cmd[9] = {0xc0, 0x05, 0,};
	u8 get_cmd[2] = {0xc1, 0x05};
	u8 get_value[7] = {0x0,};
	int temp_value = 0;

	ret = wait_event_interruptible_timeout(fts_info->wait_queue, !(fts_info->irq_status),  msecs_to_jiffies(500));

	if (ret <= 0) {
		logError(1, "%s %s: wait touch irq status timeout\n", tag, __func__);
		return;
	}

	mutex_lock(&fts_info->cmd_update_mutex);
	for (i = 0; i < Touch_Mode_NUM; i++) {
		if (xiaomi_touch_interfaces.touch_mode[i][GET_CUR_VALUE] !=
				xiaomi_touch_interfaces.touch_mode[i][SET_CUR_VALUE]) {

			xiaomi_touch_interfaces.touch_mode[i][GET_CUR_VALUE] =
				xiaomi_touch_interfaces.touch_mode[i][SET_CUR_VALUE];
			logError(1, "%s %s: mode:%d changed, value:%d\n", tag, __func__, i,
				xiaomi_touch_interfaces.touch_mode[i][SET_CUR_VALUE]);
			update = true;
		}
	}

	if (update) {
		for (j = 2; j < sizeof(set_cmd) / sizeof(u8); j++) {
			if (j - 2 == Touch_UP_THRESHOLD ||
				j - 2 == Touch_Tolerance)
				temp_value = xiaomi_touch_interfaces.touch_mode[j - 2][GET_MAX_VALUE] -
					xiaomi_touch_interfaces.touch_mode[j - 2][GET_CUR_VALUE] + xiaomi_touch_interfaces.touch_mode[j - 2][GET_MIN_VALUE];
			else
				temp_value = (xiaomi_touch_interfaces.touch_mode[j - 2][GET_CUR_VALUE]);

			set_cmd[j] = (u8)temp_value;
		}
		logError(1,
			"%s %s: write game:0x%x, 0x%x, %d, %d, %d, %d, %d, %d, %d\n",
			tag, __func__, set_cmd[0], set_cmd[1], set_cmd[2], set_cmd[3],
			set_cmd[4], set_cmd[5], set_cmd[6], set_cmd[7], set_cmd[8]);

		ret = fts_write(set_cmd, sizeof(set_cmd) / sizeof(u8));
		if (ret < OK) {
			logError(1,
				 "%s %s: error while writing touchmode data ERROR %08X\n",
				 tag, __func__, ret);
			goto end;
		}

		ret = fts_writeRead(get_cmd, sizeof(get_cmd) / sizeof(u8), get_value,
					 sizeof(get_value) / sizeof(u8));
		if (ret < OK) {
			logError(1,
				 "%s %s: error while reading touchmode data ERROR %08X\n",
				 tag, __func__, ret);
			goto end;
		}

		logError(1,
			"%s %s: read game:%d, active_mode:%d, up_threshold:%d, landlock:%d, wgh:%d, %d, %d\n",
			tag, __func__, get_value[0], get_value[1], get_value[2], get_value[3],
			get_value[4], get_value[5], get_value[6]);
		fts_update_grip_mode();
	} else {
		logError(1, "%s %s: no update\n", tag, __func__);
	}

end:
	mutex_unlock(&fts_info->cmd_update_mutex);
	return;
}

static void fts_cmd_update_work(struct work_struct *work)
{
	fts_update_touchmode_data();

	return;
}

static int fts_set_cur_value(int mode, int value)
{

	if (mode < Touch_Mode_NUM && mode >= 0) {

		xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE] = value;

		if (xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE] >
			xiaomi_touch_interfaces.touch_mode[mode][GET_MAX_VALUE]) {

			xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE] =
				xiaomi_touch_interfaces.touch_mode[mode][GET_MAX_VALUE];

		} else if (xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE] <
			xiaomi_touch_interfaces.touch_mode[mode][GET_MIN_VALUE]) {

		xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE] =
				xiaomi_touch_interfaces.touch_mode[mode][GET_MIN_VALUE];
		}
	} else {
		logError(1, "%s %s, don't support\n", tag, __func__);
	}
	logError(1, "%s %s, mode:%d, value:%d\n", tag, __func__, mode, value);

	queue_work(fts_info->touch_feature_wq, &fts_info->cmd_update_work);

	return 0;
}

static int fts_get_mode_value(int mode, int value_type)
{
	int value = -1;

	if (mode < Touch_Mode_NUM && mode >= 0)
		value = xiaomi_touch_interfaces.touch_mode[mode][value_type];
	else
		logError(1, "%s, %s, don't support\n", tag, __func__);

	return value;
}

static int fts_get_mode_all(int mode, int *value)
{
	if (mode < Touch_Mode_NUM && mode >= 0) {
		value[0] = xiaomi_touch_interfaces.touch_mode[mode][GET_CUR_VALUE];
		value[1] = xiaomi_touch_interfaces.touch_mode[mode][GET_DEF_VALUE];
		value[2] = xiaomi_touch_interfaces.touch_mode[mode][GET_MIN_VALUE];
		value[3] = xiaomi_touch_interfaces.touch_mode[mode][GET_MAX_VALUE];
	} else {
		logError(1, "%s %s, don't support\n", tag, __func__);
	}
	logError(1, "%s %s, mode:%d, value:%d:%d:%d:%d\n", tag, __func__, mode, value[0],
					value[1], value[2], value[3]);

	return 0;
}

static int fts_reset_mode(int mode)
{
	int i = 0;

	if (mode < Touch_Mode_NUM && mode > 0) {
		xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE] =
			xiaomi_touch_interfaces.touch_mode[mode][GET_DEF_VALUE];
	} else if (mode == 0) {
		for (i = 0; i < Touch_Mode_NUM; i++) {
			xiaomi_touch_interfaces.touch_mode[i][SET_CUR_VALUE] =
			xiaomi_touch_interfaces.touch_mode[i][GET_DEF_VALUE];
		}
	} else {
		logError(1, "%s %s, don't support\n", tag, __func__);
	}

	logError(1, "%s %s, mode:%d\n", tag, __func__, mode);

	queue_work(fts_info->event_wq, &fts_info->cmd_update_work);

	return 0;
}
#endif

#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_SENSOR
static int fts_finger_report_disable(struct fts_ts_info *info, bool enable)
{
	u8 cmd[4] = {0xc0, 0x09, 0x00, 0x00};

	if (enable)
		cmd[2] = 0x01;
	else
		cmd[2] = 0x00;
	return fts_write(cmd, ARRAY_SIZE(cmd));
}

static int fts_p_sensor_cmd(int on)
{
	int ret = 0;
	u8 cmd_on[] = {0xa0, 0x00, 0x05};
	u8 cmd_off[] = {0xa0, 0x00, 0x01};
	u8 hover_on[] = {0xc0, 0x03, 0x01, 0x24};
	u8 hover_off[] = {0xc0, 0x03, 0x00, 0x00};

	if (on) {
		ret = fts_write_dma_safe(cmd_on, sizeof(cmd_on));
		ret = fts_write_dma_safe(hover_on, sizeof(hover_on));
	} else {
		ret = fts_write_dma_safe(cmd_off, sizeof(cmd_off));
		ret = fts_write_dma_safe(hover_off, sizeof(hover_off));
	}

	if (ret < OK) {
		logError(1, "%s %s: write ear det cmd on...ERROR %08X !\n", tag,
			 __func__, ret);
		return -EINVAL;
	}
	logError(1, "%s %s: on:%d\n", tag, __func__, on);
	if (!on) {
		ret = fts_finger_report_disable(fts_info, false);
		if (ret < OK)
				logError(1, "%s %s: enable finger report error\n", tag, __func__);
		else
				logError(1, "%s %s: enable finger report\n", tag, __func__);
	}
	return 0;
}

static int fts_p_sensor_write(int on)
{
	int ret = 0;

	fts_info->p_sensor_switch = on;
	if (fts_info->sensor_sleep) {
		fts_info->p_sensor_changed = false;
		return 0;
	}
	ret = fts_p_sensor_cmd(on);

	if (!ret)
		fts_info->p_sensor_changed = true;
	return ret;
}

static int fts_palm_sensor_cmd(int on)
{
	int ret;
	u8 cmd_on[] = {0xc0, 0x07, 0x01};
	u8 cmd_off[] = {0xc0, 0x07, 0x00};

	if (on) {
		ret = fts_write(cmd_on, sizeof(cmd_on));
	} else {
		ret = fts_write(cmd_off, sizeof(cmd_off));
	}

	if (ret < OK) {
		logError(1, "%s %s: write anti mis-touch cmd on...ERROR %08X !\n", tag,
			 __func__, ret);
		return -EINVAL;
	}
	logError(1, "%s %s %d\n", tag, __func__, on);

	return 0;
}

static int fts_palm_sensor_write(int value)
{
	int ret = 0;
	fts_info->palm_sensor_switch = value;

	if (fts_info->sensor_sleep) {
		fts_info->palm_sensor_changed = false;
		return 0;
	}
	ret = fts_palm_sensor_cmd(value);

	if (!ret)
		fts_info->palm_sensor_changed = true;

	return ret;
}
#endif
#endif

#ifdef CONFIG_SECURE_TOUCH
static void fts_secure_touch_notify (struct fts_ts_info *info)
{
	/*might sleep*/
	sysfs_notify(&info->dev->kobj, NULL, "secure_touch");
	logError(1, "%s %s SECURE_NOTIFY:notify secure_touch\n", tag, __func__);
}

static int fts_secure_stop(struct fts_ts_info *info, bool block)
{
	struct fts_secure_info *scr_info = info->secure_info;

	logError(1, "%s %s SECURE_STOP: block = %d\n", tag, __func__, (int)block);
	if (atomic_read(&scr_info->st_enabled) == 0) {
		logError(1, "%s %s secure touch is already disabled\n", tag, __func__);
		return OK;
	}

	atomic_set(&scr_info->st_pending_irqs, -1);
	fts_secure_touch_notify(info);
	if (block) {
		if (wait_for_completion_interruptible(&scr_info->st_powerdown) == -ERESTARTSYS) {
			logError(1, "%s %s SECURE_STOP:st_powerdown be interrupted\n",
				tag, __func__);
		} else {
			logError(1, "%s %s SECURE_STOP:st_powerdown be completed\n", tag, __func__);
		}
	}
	return OK;
}

static void fts_secure_work(struct fts_secure_info *scr_info)
{
	struct fts_ts_info *info = (struct fts_ts_info *)scr_info->fts_info;


	fts_secure_touch_notify(info);
	atomic_set(&scr_info->st_1st_complete, 1);
	if (wait_for_completion_interruptible(&scr_info->st_irq_processed) == -ERESTARTSYS) {
		logError(1, "%s %s SECURE_FILTER:st_irq_processed be interrupted\n", tag, __func__);
	} else {
		logError(1, "%s %s SECURE_FILTER:st_irq_processed be completed\n", tag, __func__);
	}

	fts_enableInterrupt();
	logError(1, "%s %s SECURE_FILTER:enable irq\n", tag, __func__);
}

/*
static void fts_palm_store_delay(struct fts_secure_info *scr_info)
{
	int ret;
	struct fts_ts_info *info = scr_info->fts_info;

	logError(1, "%s %s IN", tag, __func__);
	ret = fts_palm_sensor_cmd(scr_info->scr_delay.palm_value);
	if (!ret)
		info->palm_sensor_changed = true;
	logError(1, "%s %s OUT", tag, __func__);
}


static void fts_flush_delay_task(struct fts_secure_info *scr_info)
{
	if (scr_info->scr_delay.palm_pending) {
		fts_palm_store_delay(scr_info);
		scr_info->scr_delay.palm_pending = false;
	}
}
*/

static int fts_secure_filter_interrupt(struct fts_ts_info *info)
{
	struct fts_secure_info *scr_info = info->secure_info;

	/*inited and enable first*/
	if (!scr_info->secure_inited || atomic_read(&scr_info->st_enabled) == 0) {
		return -EPERM;
	}

	fts_disableInterruptNoSync();
	logError(1, "%s %s SECURE_FILTER:disable irq\n", tag, __func__);
	/*check and change irq pending state
	 *change irq pending here, secure_touch_show, secure_touch_enable_store
	 *completion st_irq_processed at secure_touch_show, secure_touch_enable_stroe
	 */
	logError(1, "%s %s SECURE_FILTER:st_pending_irqs = %d\n",
		tag, __func__, atomic_read(&scr_info->st_pending_irqs));
	if (atomic_cmpxchg(&scr_info->st_pending_irqs, 0, 1) == 0) {
		fts_secure_work(scr_info);
		logError(1, "%s %s SECURE_FILTER:secure_work return\n", tag, __func__);
	}

	return 0;
}

static ssize_t fts_secure_touch_enable_show (struct device *dev,
										struct device_attribute *attr, char *buf)
{
	struct fts_ts_info *info = dev_get_drvdata(dev);
	struct fts_secure_info *scr_info = info->secure_info;

	logError(1, "%s %s SECURE_TOUCH_ENABLE[R]:st_enabled = %d\n", tag, __func__, atomic_read(&scr_info->st_enabled));
	return scnprintf(buf, PAGE_SIZE, "%d", atomic_read(&scr_info->st_enabled));
}

/* 	echo 0 > secure_touch_enable to disable secure touch
 * 	echo 1 > secure_touch_enable to enable secure touch
 */
static ssize_t fts_secure_touch_enable_store (struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	unsigned long value;
	struct fts_ts_info *info = dev_get_drvdata(dev);
	struct fts_secure_info *scr_info = info->secure_info;

	atomic_set(&scr_info->st_1st_complete, 0);
	logError(1, "%s %s SECURE_TOUCH_ENABLE[W]:st_1st_complete=0\n", tag, __func__);
	logError(1, "%s %s SECURE_TOUCH_ENABLE[W]:parse parameter\n", tag, __func__);
	/*check and get cmd*/
	if (count > 2)
		return -EINVAL;
	ret = kstrtoul(buf, 10, &value);
	if (ret != 0)
		return ret;

	if (!scr_info->secure_inited)
		return -EIO;

	ret = count;

	logError(1, "%s %s SECURE_TOUCH_ENABLE[W]:st_enabled = %d\n", tag, __func__, value);
	switch (value) {
	case 0:
		if (atomic_read(&scr_info->st_enabled) == 0) {
			logError(1, "%s %s secure touch is already disabled\n",
				tag, __func__);
			return ret;
		}
//		mutex_lock(&scr_info->palm_lock);
		atomic_set(&scr_info->st_enabled, 0);
		fts_secure_touch_notify(info);
		complete(&scr_info->st_irq_processed);
		fts_event_handler(info->client->irq, info);
		complete(&scr_info->st_powerdown);
//		fts_flush_delay_task(scr_info);
//		mutex_unlock(&scr_info->palm_lock);
		logError(1, "%s %s SECURE_TOUCH_ENABLE[W]:disable secure touch successful\n",
			tag, __func__);
	break;
	case 1:
		if (atomic_read(&scr_info->st_enabled) == 1) {
			logError(1, "%s %s secure touch is already enabled\n",
				tag, __func__);
			return ret;
		}
//		mutex_lock(&scr_info->palm_lock);
		/*wait until finish process all normal irq*/
		synchronize_irq(info->client->irq);

		/*enable secure touch*/
		reinit_completion(&scr_info->st_powerdown);
		reinit_completion(&scr_info->st_irq_processed);
		atomic_set(&scr_info->st_pending_irqs, 0);
		atomic_set(&scr_info->st_enabled, 1);
//		mutex_unlock(&scr_info->palm_lock);
		logError(1, "%s %s SECURE_TOUCH_ENABLE[W]:enable secure touch successful\n",
			tag, __func__);
	break;
	default:
		logError(1, "%s %s %d in secure_touch_enable is not support\n",
			tag, __func__, value);
	break;
	}
	return ret;
}

static ssize_t fts_secure_touch_show (struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fts_ts_info *info = dev_get_drvdata(dev);
	struct fts_secure_info *scr_info = info->secure_info;
	int value = 0;

	logError(1, "%s %s SECURE_TOUCH[R]:st_1st_complete = %d\n",
		tag, __func__, atomic_read(&scr_info->st_1st_complete));
	logError(1, "%s %s SECURE_TOUCH[R]:st_pending_irqs = %d\n",
		tag, __func__, atomic_read(&scr_info->st_pending_irqs));

	if (atomic_read(&scr_info->st_enabled) == 0) {
		return -EBADF;
	}

	if (atomic_cmpxchg(&scr_info->st_pending_irqs, -1, 0) == -1)
		return -EINVAL;

	if (atomic_cmpxchg(&scr_info->st_pending_irqs, 1, 0) == 1) {
		value = 1;
	} else if (atomic_cmpxchg(&scr_info->st_1st_complete, 1, 0) == 1) {
		complete(&scr_info->st_irq_processed);
		logError(1, "%s %s SECURE_TOUCH[R]:comlpetion st_irq_processed\n", tag, __func__);
	}
	return scnprintf(buf, PAGE_SIZE, "%d", value);
}
#endif


static DEVICE_ATTR(fts_lockdown, (S_IRUGO | S_IWUSR | S_IWGRP),
		   fts_lockdown_show, fts_lockdown_store);
static DEVICE_ATTR(fwupdate, (S_IRUGO | S_IWUSR | S_IWGRP), fts_fwupdate_show,
		   fts_fwupdate_store);
static DEVICE_ATTR(panel_vendor, (S_IRUGO), fts_panel_vendor_show, NULL);
static DEVICE_ATTR(panel_color, (S_IRUGO), fts_panel_color_show, NULL);
static DEVICE_ATTR(ms_strength, (S_IRUGO), fts_strength_frame_show, NULL);
static DEVICE_ATTR(lockdown_info, (S_IRUGO), fts_lockdown_info_show, NULL);
static DEVICE_ATTR(appid, (S_IRUGO), fts_appid_show, NULL);
static DEVICE_ATTR(mode_active, (S_IRUGO), fts_mode_active_show, NULL);
static DEVICE_ATTR(fw_file_test, (S_IRUGO), fts_fw_test_show, NULL);
static DEVICE_ATTR(selftest_info, (S_IRUGO), fts_selftest_info_show, NULL);
static DEVICE_ATTR(ms_raw, (S_IRUGO), fts_ms_raw_show, NULL);
static DEVICE_ATTR(ss_raw, (S_IRUGO), fts_ss_raw_show, NULL);
static DEVICE_ATTR(ms_cx_total, (S_IRUGO), fts_ms_cx_total_show, NULL);
static DEVICE_ATTR(ss_ix_total, (S_IRUGO), fts_ss_ix_total_show, NULL);

static DEVICE_ATTR(stm_fts_cmd, (S_IRUGO | S_IWUSR | S_IWGRP), stm_fts_cmd_show,
		   stm_fts_cmd_store);
#ifdef USE_ONE_FILE_NODE
static DEVICE_ATTR(feature_enable, (S_IRUGO | S_IWUSR | S_IWGRP),
		   fts_feature_enable_show, fts_feature_enable_store);
#else

#ifdef GRIP_MODE
static DEVICE_ATTR(grip_mode, (S_IRUGO | S_IWUSR | S_IWGRP), fts_grip_mode_show,
		   fts_grip_mode_store);
#endif

#ifdef CHARGER_MODE
static DEVICE_ATTR(charger_mode, (S_IRUGO | S_IWUSR | S_IWGRP),
		   fts_charger_mode_show, fts_charger_mode_store);
#endif

#ifdef GLOVE_MODE
static DEVICE_ATTR(glove_mode, (S_IRUGO | S_IWUSR | S_IWGRP),
		   fts_glove_mode_show, fts_glove_mode_store);
#endif

#ifdef COVER_MODE
static DEVICE_ATTR(cover_mode, (S_IRUGO | S_IWUSR | S_IWGRP),
		   fts_cover_mode_show, fts_cover_mode_store);
#endif

#ifdef STYLUS_MODE
static DEVICE_ATTR(stylus_mode, (S_IRUGO | S_IWUSR | S_IWGRP),
		   fts_stylus_mode_show, fts_stylus_mode_store);
#endif

#endif

#ifdef GESTURE_MODE
static DEVICE_ATTR(gesture_mask, (S_IRUGO | S_IWUSR | S_IWGRP),
		   fts_gesture_mask_show, fts_gesture_mask_store);
static DEVICE_ATTR(gesture_coordinates, (S_IRUGO | S_IWUSR | S_IWGRP),
		   fts_gesture_coordinates_show, NULL);
#endif
static DEVICE_ATTR(doze_time, (S_IRUGO | S_IWUSR | S_IWGRP),
		   fts_doze_time_show, fts_doze_time_store);
static DEVICE_ATTR(grip_enable, (S_IRUGO | S_IWUSR | S_IWGRP),
		   fts_grip_enable_show, fts_grip_enable_store);
static DEVICE_ATTR(grip_area, (S_IRUGO | S_IWUSR | S_IWGRP),
		   fts_grip_area_show, fts_grip_area_store);
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_GAMEMODE
static DEVICE_ATTR(touchgame, (S_IRUGO | S_IWUSR | S_IWGRP),
		   fts_touchgame_show, NULL);
#endif

static struct attribute *fts_attr_group[] = {
	&dev_attr_fwupdate.attr,
	&dev_attr_appid.attr,
	&dev_attr_mode_active.attr,
	&dev_attr_fw_file_test.attr,
	&dev_attr_stm_fts_cmd.attr,
#ifdef USE_ONE_FILE_NODE
	&dev_attr_feature_enable.attr,
#else

#ifdef GRIP_MODE
	&dev_attr_grip_mode.attr,
#endif
#ifdef CHARGER_MODE
	&dev_attr_charger_mode.attr,
#endif
#ifdef GLOVE_MODE
	&dev_attr_glove_mode.attr,
#endif
#ifdef COVER_MODE
	&dev_attr_cover_mode.attr,
#endif
#ifdef STYLUS_MODE
	&dev_attr_stylus_mode.attr,
#endif
#endif
	&dev_attr_fts_lockdown.attr,
	&dev_attr_panel_vendor.attr,
	&dev_attr_panel_color.attr,
	&dev_attr_lockdown_info.attr,
#ifdef GESTURE_MODE
	&dev_attr_gesture_mask.attr,
	&dev_attr_gesture_coordinates.attr,
#endif
	&dev_attr_selftest_info.attr,
	&dev_attr_ms_raw.attr,
	&dev_attr_ss_raw.attr,
	&dev_attr_ms_cx_total.attr,
	&dev_attr_ss_ix_total.attr,
	&dev_attr_ms_strength.attr,
	&dev_attr_doze_time.attr,
	&dev_attr_grip_enable.attr,
	&dev_attr_grip_area.attr,
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_GAMEMODE
	&dev_attr_touchgame.attr,
#endif
	NULL,
};

static DEVICE_ATTR(fod_status, (S_IRUGO | S_IWUSR | S_IWGRP),
		   fts_fod_status_show, fts_fod_status_store);


#ifdef CONFIG_SECURE_TOUCH
DEVICE_ATTR(secure_touch_enable, (S_IRUGO | S_IWUSR | S_IWGRP), fts_secure_touch_enable_show,  fts_secure_touch_enable_store);
DEVICE_ATTR(secure_touch, (S_IRUGO | S_IWUSR | S_IWGRP), fts_secure_touch_show,  NULL);
#endif
/**@}*/
/**@}*/

/**
 * @defgroup isr Interrupt Service Routine (Event Handler)
 * The most important part of the driver is the ISR (Interrupt Service Routine) called also as Event Handler \n
 * As soon as the interrupt pin goes low, fts_interrupt_handler() is called and the chain to read and parse the event read from the FIFO start.\n
 * For any different kind of EVT_ID there is a specific event handler which will take the correct action to report the proper info to the host. \n
 * The most important events are the one related to touch informations, status update or user report.
 * @{
 */

/**
 * Report to the linux input system the pressure and release of a button handling concurrency
 * @param info pointer to fts_ts_info which contains info about the device and its hw setup
 * @param key_code	button value
 */
void fts_input_report_key(struct fts_ts_info *info, int key_code)
{
	mutex_lock(&info->input_report_mutex);
	input_report_key(info->input_dev, key_code, 1);
	input_sync(info->input_dev);
	input_report_key(info->input_dev, key_code, 0);
	input_sync(info->input_dev);
	mutex_unlock(&info->input_report_mutex);
}

/**
* Event Handler for no events (EVT_ID_NOEVENT)
*/
static void fts_nop_event_handler(struct fts_ts_info *info,
				  unsigned char *event)
{
	logError(1,
		 "%s %s Doing nothing for event = %02X %02X %02X %02X %02X %02X %02X %02X\n",
		 tag, __func__, event[0], event[1], event[2], event[3],
		 event[4], event[5], event[6], event[7]);
}

#ifdef CONFIG_INPUT_PRESS_NDT
static int fts_infod;
static int fts_x;
static int fts_y;
bool fts_is_infod(void)
{
	return (fts_infod == 0 ? false : true);
}

static bool fts_is_in_fodarea(int x, int y)
{
	int d = 0;

	d = (x - CENTER_X) * (x - CENTER_X) + (y - CENTER_Y) * (y - CENTER_Y);
	if (d < CIRCLE_R * CIRCLE_R)
		return true;
	else
		return false;
}

bool finger_report_flag;

void fts_get_pointer(int *touch_flag, int *x, int *y)
{
	*touch_flag = fts_infod;
	*x = fts_x;
	*y = fts_y;
}
#endif
/**
* Event handler for enter and motion events (EVT_ID_ENTER_POINT, EVT_ID_MOTION_POINT )
* report to the linux input system touches with their coordinated and additional informations
*/
static void fts_enter_pointer_event_handler(struct fts_ts_info *info,
					    unsigned char *event)
{
	unsigned char touchId;
	unsigned int touch_condition = 1, tool = MT_TOOL_FINGER;
	int x, y, z, distance;
	u8 touchType;

#ifndef CONFIG_INPUT_PRESS_NDT
	if (!info->resume_bit)
		goto no_report;
#endif
	touchType = event[1] & 0x0F;
	touchId = (event[1] & 0xF0) >> 4;

	x = (((int)event[3] & 0x0F) << 8) | (event[2]);
	y = ((int)event[4] << 4) | ((event[3] & 0xF0) >> 4);

	z = 1;
	distance = 0;

#ifdef CONFIG_INPUT_PRESS_NDT
	if (fts_is_in_fodarea(x, y)) {
		z = ndt_get_pressure(1, x, y);
		if (!finger_report_flag) {
			logError(1, "%s  %s finger down in the fod area\n", tag, __func__);
			finger_report_flag = true;
		}
		input_report_key(info->input_dev, BTN_INFO, 1);
		input_sync(info->input_dev);
		fts_infod |= BIT(touchId);
	} else
		fts_infod &= ~BIT(touchId);
	fts_x = x;
	fts_y = y;
#endif
	input_mt_slot(info->input_dev, touchId);
	switch (touchType) {

#ifdef STYLUS_MODE
	case TOUCH_TYPE_STYLUS:
		logError(0, "%s  %s : It is a stylus!\n", tag, __func__);
		if (info->stylus_enabled == 1) {
			tool = MT_TOOL_PEN;
			touch_condition = 1;
			__set_bit(touchId, &info->stylus_id);
			break;
		}
#endif
	case TOUCH_TYPE_FINGER:
		/*logError(0, "%s  %s : It is a finger!\n",tag,__func__); */
	case TOUCH_TYPE_GLOVE:
		/*logError(0, "%s  %s : It is a glove!\n",tag,__func__); */
	case TOUCH_TYPE_PALM:
		/*logError(0, "%s  %s : It is a palm!\n",tag,__func__); */
		tool = MT_TOOL_FINGER;
		touch_condition = 1;
		__set_bit(touchId, &info->touch_id);
		break;

	case TOUCH_TYPE_HOVER:
		tool = MT_TOOL_FINGER;
		touch_condition = 0;
		z = 0;
		__set_bit(touchId, &info->touch_id);
		distance = DISTANCE_MAX;
		break;

	case TOUCH_TYPE_INVALID:
	default:
		logError(1, "%s  %s : Invalid touch type = %d ! No Report...\n",
			 tag, __func__, touchType);
#ifndef CONFIG_INPUT_PRESS_NDT
		goto no_report;
#endif

	}

	input_mt_report_slot_state(info->input_dev, tool, 1);
	input_report_key(info->input_dev, BTN_TOUCH, touch_condition);
	if (touch_condition)
		input_report_key(info->input_dev, BTN_TOOL_FINGER, 1);

	/*input_report_abs(info->input_dev, ABS_MT_TRACKING_ID, touchId); */
		input_report_abs(info->input_dev, ABS_MT_POSITION_X, x);
		input_report_abs(info->input_dev, ABS_MT_POSITION_Y, y);
		input_report_abs(info->input_dev, ABS_MT_TOUCH_MAJOR, z);
		input_report_abs(info->input_dev, ABS_MT_TOUCH_MINOR, z);
		input_report_abs(info->input_dev, ABS_MT_DISTANCE, distance);
#ifdef CONFIG_INPUT_PRESS_NDT
		input_report_abs(info->input_dev, ABS_MT_PRESSURE, z);
#endif
		input_sync(info->input_dev);
	dev_dbg(info->dev,
		"%s  %s :  Event 0x%02x - ID[%d], (x, y, z) = (%3d, %3d, %3d) type = %d\n",
		tag, __func__, *event, touchId, x, y, z, touchType);

#ifndef CONFIG_INPUT_PRESS_NDT
no_report:
	return;
#endif
}

/**
* Event handler for leave event (EVT_ID_LEAVE_POINT )
* Report to the linux input system that one touch left the display
*/
static void fts_leave_pointer_event_handler(struct fts_ts_info *info,
					    unsigned char *event)
{
	unsigned char touchId;
	unsigned int tool = MT_TOOL_FINGER;
	unsigned int touch_condition = 0;
	u8 touchType;
#ifdef CONFIG_INPUT_PRESS_NDT
	int x, y;
#endif

	touchType = event[1] & 0x0F;
	touchId = (event[1] & 0xF0) >> 4;
#ifdef CONFIG_INPUT_PRESS_NDT
	x = (event[2] << 4) | (event[4] & 0xF0) >> 4;
	y = (event[3] << 4) | (event[4] & 0x0F);
#endif

	input_mt_slot(info->input_dev, touchId);
	switch (touchType) {

#ifdef STYLUS_MODE
	case TOUCH_TYPE_STYLUS:
		logError(0, "%s  %s : It is a stylus!\n", tag, __func__);
		if (info->stylus_enabled == 1) {
			tool = MT_TOOL_PEN;
			__clear_bit(touchId, &info->stylus_id);
			break;
		}
#endif

	case TOUCH_TYPE_FINGER:
		/*logError(0, "%s  %s : It is a finger!\n",tag,__func__); */
	case TOUCH_TYPE_GLOVE:
		/*logError(0, "%s  %s : It is a glove!\n",tag,__func__); */
	case TOUCH_TYPE_PALM:
		/*logError(0, "%s  %s : It is a palm!\n",tag,__func__); */
		tool = MT_TOOL_FINGER;
		touch_condition = 0;
		__clear_bit(touchId, &info->touch_id);
		break;
	case TOUCH_TYPE_HOVER:
		tool = MT_TOOL_FINGER;
		touch_condition = 1;
		__clear_bit(touchId, &info->touch_id);
		break;

	case TOUCH_TYPE_INVALID:
	default:
		logError(1, "%s  %s : Invalid touch type = %d ! No Report...\n",
			 tag, __func__, touchType);
		return;

	}

	input_mt_report_slot_state(info->input_dev, tool, 0);
	if (info->touch_id == 0) {
		input_report_key(info->input_dev, BTN_TOUCH, touch_condition);
		if (!touch_condition)
			input_report_key(info->input_dev, BTN_TOOL_FINGER, 0);
		lpm_disable_for_input(false);
	}
	input_report_abs(info->input_dev, ABS_MT_TRACKING_ID, -1);
	dev_dbg(info->dev,
		"%s  %s :  Event 0x%02x - release ID[%d] type = %d\n", tag,
		__func__, event[0], touchId, touchType);
#ifdef CONFIG_INPUT_PRESS_NDT
	fts_infod &= ~BIT(touchId);
	input_report_key(info->input_dev, BTN_INFO, 0);
	input_sync(info->input_dev);
	finger_report_flag = false;
	fts_x = 0;
	fts_y = 0;
	ndt_get_pressure(0, 0, 0);
	fts_fod_status = false;
#endif

}

/* EventId : EVT_ID_MOTION_POINT */
#define fts_motion_pointer_event_handler fts_enter_pointer_event_handler

/**
* Event handler for error events (EVT_ID_ERROR)
* Handle unexpected error events implementing recovery strategy and restoring the sensing status that the IC had before the error occured
*/
static void fts_error_event_handler(struct fts_ts_info *info,
				    unsigned char *event)
{
	int error = 0;
	logError(1,
		 "%s %s Received event %02X %02X %02X %02X %02X %02X %02X %02X\n",
		 tag, __func__, event[0], event[1], event[2], event[3],
		 event[4], event[5], event[6], event[7]);

	switch (event[1]) {
	case EVT_TYPE_ERROR_ESD:
		{
			release_all_touches(info);

			fts_chip_powercycle(info);

			error = fts_system_reset();
			error |= fts_mode_handler(info, 0);
			error |= fts_enableInterrupt();
			if (error < OK) {
				logError(1,
					 "%s %s Cannot restore the device ERROR %08X\n",
					 tag, __func__, error);
			}
		}
		break;
	case EVT_TYPE_ERROR_WATCHDOG:
		{
			dumpErrorInfo(NULL, 0);
			release_all_touches(info);
			error = fts_system_reset();
			error |= fts_mode_handler(info, 0);
			error |= fts_enableInterrupt();
			if (error < OK) {
				logError(1,
					 "%s %s Cannot reset the device ERROR %08X\n",
					 tag, __func__, error);
			}
		}
		break;

	}
}

/**
* Event handler for controller ready event (EVT_ID_CONTROLLER_READY)
* Handle controller events received after unexpected reset of the IC updating the resets flag and restoring the proper sensing status
*/
static void fts_controller_ready_event_handler(struct fts_ts_info *info,
					       unsigned char *event)
{
	int error;
	logError(1,
		 "%s %s Received event %02X %02X %02X %02X %02X %02X %02X %02X\n",
		 tag, __func__, event[0], event[1], event[2], event[3],
		 event[4], event[5], event[6], event[7]);
	release_all_touches(info);
	setSystemResetedUp(1);
	setSystemResetedDown(1);
	error = fts_mode_handler(info, 0);
	if (error < OK) {
		logError(1,
			 "%s %s Cannot restore the device status ERROR %08X\n",
			 tag, __func__, error);
	}
}

/**
* Event handler for status events (EVT_ID_STATUS_UPDATE)
* Handle status update events
*/
static void fts_status_event_handler(struct fts_ts_info *info,
				     unsigned char *event)
{
	switch (event[1]) {

	case EVT_TYPE_STATUS_ECHO:
		logError(0,
			 "%s %s Echo event of command = %02X %02X %02X %02X %02X %02X\n",
			 tag, __func__, event[2], event[3], event[4], event[5],
			 event[6], event[7]);
		break;

	case EVT_TYPE_STATUS_FORCE_CAL:
		switch (event[2]) {
		case 0x00:
			logError(1,
				 "%s %s Continuous frame drop Force cal = %02X %02X %02X %02X %02X %02X\n",
				 tag, __func__, event[2], event[3], event[4],
				 event[5], event[6], event[7]);
			break;

		case 0x01:
			logError(1,
				 "%s %s Mutual negative detect Force cal = %02X %02X %02X %02X %02X %02X\n",
				 tag, __func__, event[2], event[3], event[4],
				 event[5], event[6], event[7]);
			break;

		case 0x02:
			logError(1,
				 "%s %s Mutual calib deviation Force cal = %02X %02X %02X %02X %02X %02X\n",
				 tag, __func__, event[2], event[3], event[4],
				 event[5], event[6], event[7]);
			break;

		case 0x11:
			logError(1,
				 "%s %s SS negative detect Force cal = %02X %02X %02X %02X %02X %02X\n",
				 tag, __func__, event[2], event[3], event[4],
				 event[5], event[6], event[7]);
			break;

		case 0x12:
			logError(1,
				 "%s %s SS negative detect Force cal in Low Power mode = %02X %02X %02X %02X %02X %02X\n",
				 tag, __func__, event[2], event[3], event[4],
				 event[5], event[6], event[7]);
			break;

		case 0x13:
			logError(1,
				 "%s %s SS negative detect Force cal in Idle mode = %02X %02X %02X %02X %02X %02X\n",
				 tag, __func__, event[2], event[3], event[4],
				 event[5], event[6], event[7]);
			break;

		case 0x20:
			logError(1,
				 "%s %s SS invalid Mutual Strength soft Force cal = %02X %02X %02X %02X %02X %02X\n",
				 tag, __func__, event[2], event[3], event[4],
				 event[5], event[6], event[7]);
			break;

		case 0x21:
			logError(1,
				 "%s %s SS invalid Self Strength soft Force cal = %02X %02X %02X %02X %02X %02X\n",
				 tag, __func__, event[2], event[3], event[4],
				 event[5], event[6], event[7]);
			break;

		case 0x22:
			logError(1,
				 "%s %s SS invalid Self Island soft Force cal = %02X %02X %02X %02X %02X %02X\n",
				 tag, __func__, event[2], event[3], event[4],
				 event[5], event[6], event[7]);
			break;

		case 0x30:
			logError(1,
				 "%s %s MS invalid Mutual Strength soft Force cal = %02X %02X %02X %02X %02X %02X\n",
				 tag, __func__, event[2], event[3], event[4],
				 event[5], event[6], event[7]);
			break;

		case 0x31:
			logError(1,
				 "%s %s MS invalid Self Strength soft Force cal = %02X %02X %02X %02X %02X %02X\n",
				 tag, __func__, event[2], event[3], event[4],
				 event[5], event[6], event[7]);
			break;

		default:
			logError(1,
				 "%s %s Force cal = %02X %02X %02X %02X %02X %02X\n",
				 tag, __func__, event[2], event[3], event[4],
				 event[5], event[6], event[7]);

		}
		break;

	case EVT_TYPE_STATUS_FRAME_DROP:
		switch (event[2]) {
		case 0x01:
			logError(1,
				 "%s %s Frame drop noisy frame = %02X %02X %02X %02X %02X %02X\n",
				 tag, __func__, event[2], event[3], event[4],
				 event[5], event[6], event[7]);
			break;

		case 0x02:
			logError(1,
				 "%s %s Frame drop bad R = %02X %02X %02X %02X %02X %02X\n",
				 tag, __func__, event[2], event[3], event[4],
				 event[5], event[6], event[7]);
			break;

		case 0x03:
			logError(1,
				 "%s %s Frame drop invalid processing state = %02X %02X %02X %02X %02X %02X\n",
				 tag, __func__, event[2], event[3], event[4],
				 event[5], event[6], event[7]);
			break;

		default:
			logError(1,
				 "%s %s Frame drop = %02X %02X %02X %02X %02X %02X\n",
				 tag, __func__, event[2], event[3], event[4],
				 event[5], event[6], event[7]);

		}
		break;

	case EVT_TYPE_STATUS_SS_RAW_SAT:
		if (event[2] == 1)
			logError(1,
				 "%s %s SS Raw Saturated = %02X %02X %02X %02X %02X %02X\n",
				 tag, __func__, event[2], event[3], event[4],
				 event[5], event[6], event[7]);
		else
			logError(1,
				 "%s %s SS Raw No more Saturated = %02X %02X %02X %02X %02X %02X\n",
				 tag, __func__, event[2], event[3], event[4],
				 event[5], event[6], event[7]);
		break;

	case EVT_TYPE_STATUS_WATER:
		if (event[2] == 1)
			logError(1,
				 "%s %s Enter Water mode = %02X %02X %02X %02X %02X %02X\n",
				 tag, __func__, event[2], event[3], event[4],
				 event[5], event[6], event[7]);
		else
			logError(1,
				 "%s %s Exit Water mode = %02X %02X %02X %02X %02X %02X\n",
				 tag, __func__, event[2], event[3], event[4],
				 event[5], event[6], event[7]);
		break;
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_SENSOR
	case EVT_TYPE_STATUS_POCKET:
		if (event[0] == 0x43 && event[2] == 0x01) {
			update_palm_sensor_value(1);
		} else if (event[0] == 0x43 && event[2] == 0x00) {
			update_palm_sensor_value(0);
		}
		break;
#endif

	default:
		logError(1,
			 "%s %s Received unhandled status event = %02X %02X %02X %02X %02X %02X %02X %02X\n",
			 tag, __func__, event[0], event[1], event[2], event[3],
			 event[4], event[5], event[6], event[7]);
		break;
	}

}

#ifdef PHONE_KEY
/**
 * Event handler for status events (EVT_TYPE_USER_KEY)
 * Handle keys update events, the third byte of the event is a bitmask where if the bit set means that the corresponding key is pressed.
 */
static void fts_key_event_handler(struct fts_ts_info *info,
				  unsigned char *event)
{

	logError(0,
		 "%s %s Received event %02X %02X %02X %02X %02X %02X %02X %02X\n",
		 tag, __func__, event[0], event[1], event[2], event[3],
		 event[4], event[5], event[6], event[7]);

	if (event[0] == EVT_ID_USER_REPORT && event[1] == EVT_TYPE_USER_KEY) {

		if ((event[2] & FTS_KEY_0) == 0 && (key_mask & FTS_KEY_0) > 0) {
			logError(0,
				 "%s %s: Button HOME pressed and released! \n",
				 tag, __func__);
			fts_input_report_key(info, KEY_HOMEPAGE);
		}

		if ((event[2] & FTS_KEY_1) == 0 && (key_mask & FTS_KEY_1) > 0) {
			logError(0,
				 "%s %s: Button Back pressed and released! \n",
				 tag, __func__);
			fts_input_report_key(info, KEY_BACK);
		}

		if ((event[2] & FTS_KEY_2) == 0 && (key_mask & FTS_KEY_2) > 0) {
			logError(0, "%s %s: Button Menu pressed! \n", tag,
				 __func__);
			fts_input_report_key(info, KEY_MENU);
		}

		key_mask = event[2];
	} else {
		logError(1, "%s %s: Invalid event passed as argument! \n", tag,
			 __func__);
	}

}
#endif

#ifdef GESTURE_MODE
/**
 * Event handler for gesture events (EVT_TYPE_USER_GESTURE)
 * Handle gesture events and simulate the click on a different button for any gesture detected (@link gesture_opt Gesture IDs @endlink)
 */
static void fts_gesture_event_handler(struct fts_ts_info *info,
				      unsigned char *event)
{
	int value;
	int needCoords = 0;
#ifdef CONFIG_INPUT_PRESS_NDT
	int x = (event[4] << 8) | (event[3]);
	int y = (event[6] << 8) | (event[5]);
	int z = ndt_get_pressure(1, x, y);
	int touchId = 0;

	fts_x = x;
	fts_y = y;
#endif

	logError(0,
		 "%s  gesture event data: %02X %02X %02X %02X %02X %02X %02X %02X\n",
		 tag, event[0], event[1], event[2], event[3], event[4],
		 event[5], event[6], event[7]);
#ifdef CONFIG_INPUT_PRESS_NDT
	pr_debug("FTS:%s,x:%d,y:%d,z:%d\n", __func__, x, y, z);
#endif

	if (event[0] == EVT_ID_USER_REPORT && event[1] == EVT_TYPE_USER_GESTURE) {
		needCoords = 1;
#ifdef CONFIG_INPUT_PRESS_NDT
		if (event[2] == GEST_ID_LONG_PRESS && info->fod_status) {
			fts_fod_status = true;
			if (fts_is_in_fodarea(x, y)) {
				if (!finger_report_flag) {
					logError(1, "%s  %s finger down in the fod area\n", tag, __func__);
					finger_report_flag = true;
				}
				input_report_key(info->input_dev, BTN_INFO, 1);
				input_sync(info->input_dev);
				fts_infod |= BIT(touchId);
			} else
				fts_infod &= ~BIT(touchId);
			input_mt_slot(info->input_dev, touchId);
			input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 1);
			input_report_key(info->input_dev, BTN_TOUCH, 1);
			input_report_key(info->input_dev, BTN_TOOL_FINGER, 1);
			/*input_report_abs(info->input_dev, ABS_MT_TRACKING_ID, touchId); */
			input_report_abs(info->input_dev, ABS_MT_POSITION_X, x);
			input_report_abs(info->input_dev, ABS_MT_POSITION_Y, y);
			input_report_abs(info->input_dev, ABS_MT_TOUCH_MAJOR, z);
			input_report_abs(info->input_dev, ABS_MT_TOUCH_MINOR, z);
			input_report_abs(info->input_dev, ABS_MT_DISTANCE, 0);
			input_report_abs(info->input_dev, ABS_MT_PRESSURE, z);
			goto gesture_done;
		}
		if (event[2] == GEST_ID_SINGTAP) {
			input_report_key(info->input_dev, KEY_GOTO, 1);
			input_sync(info->input_dev);
			input_report_key(info->input_dev, KEY_GOTO, 0);
			input_sync(info->input_dev);
			goto gesture_done;
		}
#endif
		switch (event[2]) {
		case GEST_ID_DBLTAP:
			if (!info->gesture_enabled)
				goto gesture_done;
			value = KEY_WAKEUP;
			logError(0, "%s %s: double tap ! \n", tag, __func__);
			needCoords = 0;
			break;

		case GEST_ID_AT:
			value = KEY_WWW;
			logError(0, "%s %s: @ ! \n", tag, __func__);
			break;

		case GEST_ID_C:
			value = KEY_C;
			logError(0, "%s %s: C ! \n", tag, __func__);
			break;

		case GEST_ID_E:
			value = KEY_E;
			logError(0, "%s %s: e ! \n", tag, __func__);
			break;

		case GEST_ID_F:
			value = KEY_F;
			logError(0, "%s %s: F ! \n", tag, __func__);
			break;

		case GEST_ID_L:
			value = KEY_L;
			logError(0, "%s %s: L ! \n", tag, __func__);
			break;

		case GEST_ID_M:
			value = KEY_M;
			logError(0, "%s %s: M ! \n", tag, __func__);
			break;

		case GEST_ID_O:
			value = KEY_O;
			logError(0, "%s %s: O ! \n", tag, __func__);
			break;

		case GEST_ID_S:
			value = KEY_S;
			logError(0, "%s %s: S ! \n", tag, __func__);
			break;

		case GEST_ID_V:
			value = KEY_V;
			logError(0, "%s %s:  V ! \n", tag, __func__);
			break;

		case GEST_ID_W:
			value = KEY_W;
			logError(0, "%s %s:  W ! \n", tag, __func__);
			break;

		case GEST_ID_Z:
			value = KEY_Z;
			logError(0, "%s %s:  Z ! \n", tag, __func__);
			break;

		case GEST_ID_RIGHT_1F:
			value = KEY_RIGHT;
			logError(0, "%s %s:  -> ! \n", tag, __func__);
			break;

		case GEST_ID_LEFT_1F:
			value = KEY_LEFT;
			logError(0, "%s %s:  <- ! \n", tag, __func__);
			break;

		case GEST_ID_UP_1F:
			value = KEY_UP;
			logError(0, "%s %s:  UP ! \n", tag, __func__);
			break;

		case GEST_ID_DOWN_1F:
			value = KEY_DOWN;
			logError(0, "%s %s:  DOWN ! \n", tag, __func__);
			break;

		case GEST_ID_CARET:
			value = KEY_APOSTROPHE;
			logError(0, "%s %s:  ^ ! \n", tag, __func__);
			break;

		case GEST_ID_LEFTBRACE:
			value = KEY_LEFTBRACE;
			logError(0, "%s %s:  < ! \n", tag, __func__);
			break;

		case GEST_ID_RIGHTBRACE:
			value = KEY_RIGHTBRACE;
			logError(0, "%s %s:  > ! \n", tag, __func__);
			break;

		default:
			logError(0, "%s %s:  No valid GestureID! \n", tag,
				 __func__);
			goto gesture_done;

		}

		if (needCoords == 1)
			readGestureCoords(event);

		fts_input_report_key(info, value);

gesture_done:
		return;
	} else {
		logError(1, "%s %s: Invalid event passed as argument! \n", tag,
			 __func__);
	}

}
#endif

/**
 * Event handler for user report events (EVT_ID_USER_REPORT)
 * Handle user events reported by the FW due to some interaction triggered by an external user (press keys, perform gestures, etc.)
 */
static void fts_user_report_event_handler(struct fts_ts_info *info,
					  unsigned char *event)
{

	switch (event[1]) {

#ifdef PHONE_KEY
	case EVT_TYPE_USER_KEY:
		fts_key_event_handler(info, event);
		break;
#endif

	case EVT_TYPE_USER_PROXIMITY:
		if (event[2] == 0) {
			logError(1, "%s %s No proximity!\n", tag, __func__);
		} else {
			logError(1, "%s %s Proximity Detected!\n", tag,
				 __func__);
		}
		break;

#ifdef GESTURE_MODE
	case EVT_TYPE_USER_GESTURE:
		fts_gesture_event_handler(info, event);
		break;
#endif
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_SENSOR
	case EVT_TYPE_USER_EARDET:
		if (event[2] == 0xAA) {
			logError(1, "%s %s eardet enter\n", tag, __func__);
			update_p_sensor_value(1);
		} else if (event[2] == 0xBB) {
			logError(1, "%s %s eardet leave\n", tag, __func__);
			update_p_sensor_value(0);
		} else if (event[2] == 0xCC) {
			logError(1, "%s %s eardet hover enter\n", tag, __func__);
			update_p_sensor_value(2);
		}
		break;
#endif
	default:
		logError(1,
			 "%s %s Received unhandled user report event = %02X %02X %02X %02X %02X %02X %02X %02X\n",
			 tag, __func__, event[0], event[1], event[2], event[3],
			 event[4], event[5], event[6], event[7]);
		break;
	}

}

/**
 * Bottom Half Interrupt Handler function
 * This handler is called each time there is at least one new event in the FIFO and the interrupt pin of the IC goes low.
 * It will read all the events from the FIFO and dispatch them to the proper event handler according the event ID
 */
static irqreturn_t fts_event_handler(int irq, void *ts_info)
{
	struct fts_ts_info *info = ts_info;
	int error = 0, count = 0;
	unsigned char regAdd = FIFO_CMD_READALL;
	unsigned char data[FIFO_EVENT_SIZE * FIFO_DEPTH] = {0};
	unsigned char eventId;
	const unsigned char EVENTS_REMAINING_POS = 7;
	const unsigned char EVENTS_REMAINING_MASK = 0x1F;
	unsigned char events_remaining = 0;
	unsigned char *evt_data;
	event_dispatch_handler_t event_handler;

#ifdef CONFIG_SECURE_TOUCH
	if (!fts_secure_filter_interrupt(info)) {
		return IRQ_HANDLED;
	}
#endif

	lpm_disable_for_input(true);
	if (info->dev_pm_suspend) {
		error = wait_for_completion_timeout(&info->dev_pm_suspend_completion, msecs_to_jiffies(700));
		if (!error) {
			logError(1, "%s system(i2c) can't finished resuming procedure, skip it", tag);
			lpm_disable_for_input(false);
			return IRQ_HANDLED;
			}
	}
	info->irq_status = true;
	error = fts_writeReadU8UX(regAdd, 0, 0, data, FIFO_EVENT_SIZE,
				  DUMMY_FIFO);
	events_remaining = data[EVENTS_REMAINING_POS] & EVENTS_REMAINING_MASK;
	events_remaining = (events_remaining > FIFO_DEPTH - 1) ?
				FIFO_DEPTH - 1 : events_remaining;

	/*Drain the rest of the FIFO, up to 31 events*/
	if (error == OK && events_remaining > 0) {
		error = fts_writeReadU8UX(regAdd, 0, 0, &data[FIFO_EVENT_SIZE],
					  FIFO_EVENT_SIZE * events_remaining,
					  DUMMY_FIFO);
	}
	if (error != OK) {
		logError(1,
		    "Error (%d) while reading from FIFO in fts_event_handler",
		    error);
	} else {
		for (count = 0; count < events_remaining + 1; count++) {
			evt_data = &data[count * FIFO_EVENT_SIZE];

			if (evt_data[0] == EVT_ID_NOEVENT)
				break;
			eventId = evt_data[0] >> 4;
			/*Ensure event ID is within bounds*/
			if (eventId < NUM_EVT_ID) {
				event_handler = info->event_dispatch_table[eventId];
				event_handler(info, (evt_data));
			}
		}
	}
	input_sync(info->input_dev);
	info->irq_status = false;
	if (!info->touch_id)
		lpm_disable_for_input(false);
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_GAMEMODE
	if (wait_queue_complete)
		wake_up(&info->wait_queue);
	else
		logError(1, "%s: haven't complete init wait_queue", tag);
#endif
	return IRQ_HANDLED;
}

/**@}*/

static const char *fts_get_config(struct fts_ts_info *info)
{
	struct fts_hw_platform_data *pdata = info->board;
	int i = 0, ret = 0;

	ret = fts_get_lockdown_info(info->lockdown_info, info);

	if (ret < OK) {
		logError(1, "%s can't read lockdown info", tag);
		return pdata->default_fw_name;
	}

	ret |= fts_enableInterrupt();

	for (i = 0; i < pdata->config_array_size; i++) {
		if ((info->lockdown_info[0] ==
		     pdata->config_array[i].tp_vendor))
			break;
	}

	if (i >= pdata->config_array_size) {
		logError(1, "%s can't find right config", tag);
		return pdata->default_fw_name;
	}

	logError(1, "%s Choose config %d: %s", tag, i,
		 pdata->config_array[i].fts_cfg_name);
	pdata->current_index = i;

	return pdata->config_array[i].fts_cfg_name;
}

static const char *fts_get_limit(struct fts_ts_info *info)
{
	struct fts_hw_platform_data *pdata = info->board;
	int i = 0, ret = 0;

	ret = fts_get_lockdown_info(info->lockdown_info, info);

	if (ret < OK) {
		logError(1, "%s can't read lockdown info", tag);
		return LIMITS_FILE;
	}

	ret |= fts_enableInterrupt();

	for (i = 0; i < pdata->config_array_size; i++) {
		if ((info->lockdown_info[0] ==
		     pdata->config_array[i].tp_vendor))
			break;
	}

	if (i >= pdata->config_array_size) {
		logError(1, "%s can't find right limit", tag);
		return LIMITS_FILE;
	}

	logError(1, "%s Choose limit file %d: %s", tag, i,
		 pdata->config_array[i].fts_limit_name);
	pdata->current_index = i;
	return pdata->config_array[i].fts_limit_name;
}

/**
*	Implement the fw update and initialization flow of the IC that should be executed at every boot up.
*	The function perform a fw update of the IC in case of crc error or a new fw version and then understand if the IC need to be re-initialized again.
*	@return  OK if success or an error code which specify the type of error encountered
*/
int fts_fw_update(struct fts_ts_info *info, const char *fw_name, int force)
{

	u8 error_to_search[4] = {EVT_TYPE_ERROR_CRC_CX_HEAD, EVT_TYPE_ERROR_CRC_CX,
		EVT_TYPE_ERROR_CRC_CX_SUB_HEAD, EVT_TYPE_ERROR_CRC_CX_SUB
	};
	int retval = 0;
	int retval1 = 0;
	int ret;
	int crc_status = 0;
	int error = 0;
	int init_type = NO_INIT;
#ifdef PRE_SAVED_METHOD
	int keep_cx = 1;
#else
	int keep_cx = 0;
#endif

	logError(1, "%s Fw Auto Update is starting... \n", tag);

	ret = fts_crc_check();
	if (ret > OK) {
		logError(1, "%s %s: CRC Error or NO FW!\n", tag, __func__);
		crc_status = ret;
	} else {
		crc_status = 0;
		logError(1,
			 "%s %s: NO CRC Error or Impossible to read CRC register! \n",
			 tag, __func__);
	}

	if (fw_name == NULL) {
		fw_name = fts_get_config(info);
		if (fw_name == NULL)
			logError(1, "%s not found mached config!", tag);
	}

	if (fw_name) {
		if (force)
			retval = flashProcedure(fw_name, 1, keep_cx);
		else
			retval = flashProcedure(fw_name, crc_status, keep_cx);

		if ((retval & 0xFF000000) == ERROR_FLASH_PROCEDURE) {
			logError(1,
				 "%s %s: firmware update failed and retry! ERROR %08X\n",
				 tag, __func__, retval);
			fts_chip_powercycle(info);
			retval1 = flashProcedure(info->board->default_fw_name, crc_status, keep_cx);
			if ((retval1 & 0xFF000000) == ERROR_FLASH_PROCEDURE) {
				logError(1,
					 "%s %s: firmware update failed again!  ERROR %08X\n",
					 tag, __func__, retval1);
				logError(1, "%s Fw Auto Update Failed!\n", tag);
			}
		}
	}

	logError(1, "%s %s: Verifying if CX CRC Error...\n", tag, __func__,
		 ret);
	ret = fts_system_reset();
	if (ret >= OK) {
		ret = pollForErrorType(error_to_search, 4);
		if (ret < OK) {
			logError(1, "%s %s: No Cx CRC Error Found! \n", tag,
				 __func__);
			logError(1, "%s %s: Verifying if Panel CRC Error... \n",
				 tag, __func__);
			error_to_search[0] = EVT_TYPE_ERROR_CRC_PANEL_HEAD;
			error_to_search[1] = EVT_TYPE_ERROR_CRC_PANEL;
			ret = pollForErrorType(error_to_search, 2);
			if (ret < OK) {
				logError(1,
					 "%s %s: No Panel CRC Error Found! \n",
					 tag, __func__);
				init_type = NO_INIT;
			} else {
				logError(1,
					 "%s %s: Panel CRC Error FOUND! CRC ERROR = %02X\n",
					 tag, __func__, ret);
				init_type = SPECIAL_PANEL_INIT;
			}
		} else {
			logError(1,
				 "%s %s: Cx CRC Error FOUND! CRC ERROR = %02X\n",
				 tag, __func__, ret);

			logError(1,
				 "%s %s: Try to recovery with CX in fw file...\n",
				 tag, __func__, ret);
			flashProcedure(info->board->default_fw_name, CRC_CX, 0);
			logError(1, "%s %s: Refresh panel init data... \n", tag,
				 __func__, ret);
		}
	} else {
		logError(1,
			 "%s %s: Error while executing system reset! ERROR %08X\n",
			 tag, __func__, ret);
	}

	if ((init_type == NO_INIT)) {
#ifdef PRE_SAVED_METHOD
		if (systemInfo.u8_cfgAfeVer != systemInfo.u8_cxAfeVer) {
			init_type = SPECIAL_FULL_PANEL_INIT;
			logError(0,
				 "%s %s: Different CX AFE Ver: %02X != %02X... Execute FULL Panel Init! \n",
				 tag, __func__, systemInfo.u8_cfgAfeVer,
				 systemInfo.u8_cxAfeVer);
		} else
#endif

		if (systemInfo.u8_cfgAfeVer != systemInfo.u8_panelCfgAfeVer) {
			init_type = SPECIAL_PANEL_INIT;
			logError(0,
				 "%s %s: Different Panel AFE Ver: %02X != %02X... Execute Panel Init! \n",
				 tag, __func__, systemInfo.u8_cfgAfeVer,
				 systemInfo.u8_panelCfgAfeVer);
		} else {
			init_type = NO_INIT;
		}
	}

	if (init_type != NO_INIT) {
		error = fts_chip_initialization(info, init_type);
		if (error < OK) {
			logError(1,
				 "%s %s Cannot initialize the chip ERROR %08X\n",
				 tag, __func__, error);
		}
	}

	error = fts_init_sensing(info);
	if (error < OK) {
		logError(1,
			 "%s Cannot initialize the hardware device ERROR %08X\n",
			 tag, error);
	}

	logError(1, "%s Fw Update Finished! error = %08X\n", tag, error);
	return error;
}

#ifndef FW_UPDATE_ON_PROBE

/**
*	Function called by the delayed workthread executed after the probe in order to perform the fw update flow
*	@see  fts_fw_update()
*/
static void fts_fw_update_auto(struct work_struct *work)
{
	struct delayed_work *fwu_work =
	    container_of(work, struct delayed_work, work);
	struct fts_ts_info *info =
	    container_of(fwu_work, struct fts_ts_info, fwu_work);
	fts_fw_update(info, NULL, 0);
}
#endif

/**
*	Execute the initialization of the IC (supporting a retry mechanism), checking also the resulting data
*	@see  production_test_main()
*/
static int fts_chip_initialization(struct fts_ts_info *info, int init_type)
{
	int ret2 = 0;
	int retry;
	int initretrycnt = 0;

	for (retry = 0; retry <= RETRY_INIT_BOOT; retry++) {
		ret2 = production_test_initialization(init_type);
		if (ret2 == OK)
			break;
		initretrycnt++;
		logError(1,
			 "%s initialization cycle count = %04d - ERROR %08X \n",
			 tag, initretrycnt, ret2);
		fts_chip_powercycle(info);
	}

	if (ret2 < OK) {
		logError(1, "%s fts initialization failed 3 times \n", tag);
	}

	return ret2;
}

/**
*	Initialize the dispatch table with the event handlers for any possible event ID and the interrupt routine behavior (triggered when the IRQ pin is low and associating the top half interrupt handler function).
*	@see fts_interrupt_handler()
*/
static int fts_interrupt_install(struct fts_ts_info *info)
{
	int i, error = 0;

	info->event_dispatch_table =
	    kzalloc(sizeof(event_dispatch_handler_t) * NUM_EVT_ID, GFP_KERNEL);

	if (!info->event_dispatch_table) {
		logError(1, "%s OOM allocating event dispatch table\n", tag);
		return -ENOMEM;
	}

	for (i = 0; i < NUM_EVT_ID; i++)
		info->event_dispatch_table[i] = fts_nop_event_handler;

	install_handler(info, ENTER_POINT, enter_pointer);
	install_handler(info, LEAVE_POINT, leave_pointer);
	install_handler(info, MOTION_POINT, motion_pointer);
	install_handler(info, ERROR, error);
	install_handler(info, CONTROLLER_READY, controller_ready);
	install_handler(info, STATUS_UPDATE, status);
	install_handler(info, USER_REPORT, user_report);

	/* disable interrupts in any case */
	error = fts_disableInterrupt();
	logError(1, "%s Interrupt Mode\n", tag);
	if (request_threaded_irq(info->client->irq, NULL, fts_event_handler, info->board->irq_flags,
			 FTS_TS_DRV_NAME, info)) {
		logError(1, "%s Request irq failed\n", tag);
		kfree(info->event_dispatch_table);
		error = -EBUSY;
	} else {
		disable_irq(info->client->irq);
	}

	return error;
}

/**
*	Clean the dispatch table and the free the IRQ.
*	This function is called when the driver need to be removed
*/
static void fts_interrupt_uninstall(struct fts_ts_info *info)
{

	fts_disableInterrupt();

	kfree(info->event_dispatch_table);

	free_irq(info->client->irq, info);

}

/**@}*/

/**
* This function try to attempt to communicate with the IC for the first time during the boot up process in order to acquire the necessary info for the following stages.
* The function execute a system reset, read fundamental info (system info) from the IC and install the interrupt
* @return OK if success or an error code which specify the type of error encountered
*/
static int fts_init(struct fts_ts_info *info)
{
	int error;

	error = fts_system_reset();
	if (error < OK && isI2cError(error)) {
		logError(1, "%s Cannot reset the device! ERROR %08X\n", tag,
			 error);
		return error;
	} else {
		if (error == (ERROR_TIMEOUT | ERROR_SYSTEM_RESET_FAIL)) {
			logError(1, "%s Setting default Sys INFO! \n", tag);
			error = defaultSysInfo(0);
		} else {
			error = readSysInfo(0);
			if (error < OK) {
				if (!isI2cError(error))
					error = OK;
				logError(1,
					 "%s Cannot read Sys Info! ERROR %08X\n",
					 tag, error);
			}
		}
	}

	return error;
}

/**
* Execute a power cycle in the IC, toggling the power lines (AVDD and DVDD)
* @param info pointer to fts_ts_info struct which contain information of the regulators
* @return 0 if success or another value if fail
*/
int fts_chip_powercycle(struct fts_ts_info *info)
{
	int error = 0;

	logError(1, "%s %s: Power Cycle Starting... \n", tag, __func__);
	logError(1, "%s %s: Disabling IRQ... \n", tag, __func__);

	fts_disableInterruptNoSync();

	if (info->vdd_reg) {
		error = regulator_disable(info->vdd_reg);
		if (error < 0) {
			logError(1, "%s %s: Failed to disable DVDD regulator\n",
				 tag, __func__);
		}
	}

	if (info->avdd_reg) {
		error = regulator_disable(info->avdd_reg);
		if (error < 0) {
			logError(1, "%s %s: Failed to disable AVDD regulator\n",
				 tag, __func__);
		}
	}

	if (info->board->reset_gpio != GPIO_NOT_DEFINED)
		gpio_set_value(info->board->reset_gpio, 0);
	else
		mdelay(300);

	if (info->vdd_reg) {
		error = regulator_enable(info->vdd_reg);
		if (error < 0) {
			logError(1, "%s %s: Failed to enable DVDD regulator\n",
				 tag, __func__);
		}
	}

	mdelay(1);

	if (info->avdd_reg) {
		error = regulator_enable(info->avdd_reg);
		if (error < 0) {
			logError(1, "%s %s: Failed to enable AVDD regulator\n",
				 tag, __func__);
		}
	}

	mdelay(5);

	if (info->board->reset_gpio != GPIO_NOT_DEFINED) {
		mdelay(10);
		gpio_set_value(info->board->reset_gpio, 1);
	}

	release_all_touches(info);

	logError(1, "%s %s: Power Cycle Finished! ERROR CODE = %08x\n", tag,
		 __func__, error);
	setSystemResetedUp(1);
	setSystemResetedDown(1);
	return error;
}

/**
 * Complete the boot up process, initializing the sensing of the IC according to the current setting chosen by the host and register the notifier for the suspend/resume actions and the event handler
 * @return OK if success or an error code which specify the type of error encountered
 */
static int fts_init_sensing(struct fts_ts_info *info)
{
	int error = 0;
#ifdef CONFIG_DRM
	error |= drm_register_client(&info->notifier);
#endif
	error |= fts_interrupt_install(info);
	error |= fts_mode_handler(info, 0);
#ifdef CONFIG_INPUT_PRESS_NDT
	error |= setScanMode(SCAN_MODE_ACTIVE, 0x00);
	mdelay(WAIT_AFTER_SENSEOFF);
	error |= setScanMode(SCAN_MODE_ACTIVE, 0x01);
#endif
	error |= fts_enableInterrupt();

	if (error < OK)
		logError(1, "%s %s Init after Probe error (ERROR = %08X)\n",
			 tag, __func__, error);

	return error;
}

/**
 * @ingroup mode_section
 * @{
 */
/**
 * The function handle the switching of the mode in the IC enabling/disabling the sensing and the features set from the host
 * @param info pointer to fts_ts_info which contains info about the device and its hw setup
 * @param force if 1, the enabling/disabling command will be send even if the feature was alredy enabled/disabled otherwise it will judge if the feature changed status or the IC had s system reset and therefore the features need to be restored
 * @return OK if success or an error code which specify the type of error encountered
 */
static int fts_mode_handler(struct fts_ts_info *info, int force)
{
	int res = OK;
	int ret = OK;
	u8 settings[4] = { 0 };
#ifdef CONFIG_INPUT_PRESS_NDT
	u8 gesture_cmd[6] = {0xA2, 0x03, 0x20, 0x00, 0x00, 0x01};
	u8 single_only_cmd[4] = {0xC0, 0x02, 0x00, 0x00};
	u8 single_double_cmd[4] = {0xC0, 0x02, 0x01, 0x1E};
#endif

	info->mode = MODE_NOTHING;
	logError(0, "%s %s: Mode Handler starting... \n", tag, __func__);
	switch (info->resume_bit) {
	case 0:
		logError(0, "%s %s: Screen OFF... \n", tag, __func__);
#ifndef CONFIG_INPUT_PRESS_NDT
		logError(0, "%s %s: Sense OFF! \n", tag, __func__);
		ret = setScanMode(SCAN_MODE_ACTIVE, 0x00);
		res |= ret;
#endif
#ifdef CONFIG_INPUT_PRESS_NDT
		logError(1, "%s %s,send long press and gesture cmd\n", tag, __func__);
		res = fts_write_dma_safe(gesture_cmd, ARRAY_SIZE(gesture_cmd));
		if (res < OK)
				logError(1, "%s %s: enter gesture and longpress failed! ERROR %08X recovery in senseOff...\n",
					 tag, __func__, res);
		res = setScanMode(SCAN_MODE_LOW_POWER, 0);
		res |= ret;
		if (info->gesture_enabled == 1) {
			res = fts_write_dma_safe(single_double_cmd, ARRAY_SIZE(single_double_cmd));
			if (res < OK)
					logError(1, "%s %s: set single and double tap delay time failed! ERROR %08X\n", tag, __func__, res);
		} else {
			res = fts_write_dma_safe(single_only_cmd, ARRAY_SIZE(single_only_cmd));
			if (res < OK)
					logError(1, "%s %s: set single only delay time failed! ERROR %08X\n", tag, __func__, res);
		}
		ret = fts_enableInterrupt();
		if (ret < OK)
			logError(1, "%s enterGestureMode: fts_enableInterrupt ERROR %08X\n", tag, res | ERROR_ENABLE_INTER);
		else
			logError(1, "%s enterGestureMode: fts_enableInterrupt\n", tag);
#else
		if (info->gesture_enabled == 1) {
			logError(0, "%s %s: enter in gesture mode ! \n", tag,
				 __func__);
			res = enterGestureMode(isSystemResettedDown());
			if (res >= OK) {
				fromIDtoMask(FEAT_SEL_GESTURE,
					     (u8 *)&info->mode,
					     sizeof(info->mode));
				MODE_LOW_POWER(info->mode, 0);
			} else {
				logError(1,
					 "%s %s: enterGestureMode failed! ERROR %08X recovery in senseOff...\n",
					 tag, __func__, res);
			}
		}
#endif
		setSystemResetedDown(0);
		break;

	case 1:
		logError(0, "%s %s: Screen ON... \n", tag, __func__);

#ifdef GLOVE_MODE
		if ((info->glove_enabled == FEAT_ENABLE && isSystemResettedUp())
		    || force == 1) {
			logError(0, "%s %s: Glove Mode setting... \n", tag,
				 __func__);
			settings[0] = info->glove_enabled;
			ret = setFeatures(FEAT_SEL_GLOVE, settings, 1);
			if (ret < OK) {
				logError(1,
					 "%s %s: error during setting GLOVE_MODE! ERROR %08X\n",
					 tag, __func__, ret);
			}
			res |= ret;

			if (ret >= OK && info->glove_enabled == FEAT_ENABLE) {
				fromIDtoMask(FEAT_SEL_GLOVE,
					     (u8 *)&info->mode,
					     sizeof(info->mode));
				logError(1, "%s %s: GLOVE_MODE Enabled! \n",
					 tag, __func__);
			} else {
				logError(1, "%s %s: GLOVE_MODE Disabled! \n",
					 tag, __func__);
			}

		}
#endif

#ifdef COVER_MODE
		if ((info->cover_enabled == FEAT_ENABLE && isSystemResettedUp())
		    || force == 1) {
			logError(0, "%s %s: Cover Mode setting... \n", tag,
				 __func__);
			settings[0] = info->cover_enabled;
			ret = setFeatures(FEAT_SEL_COVER, settings, 1);
			if (ret < OK) {
				logError(1,
					 "%s %s: error during setting COVER_MODE! ERROR %08X\n",
					 tag, __func__, ret);
			}
			res |= ret;

			if (ret >= OK && info->cover_enabled == FEAT_ENABLE) {
				fromIDtoMask(FEAT_SEL_COVER,
					     (u8 *)&info->mode,
					     sizeof(info->mode));
				logError(1, "%s %s: COVER_MODE Enabled! \n",
					 tag, __func__);
			} else {
				logError(1, "%s %s: COVER_MODE Disabled! \n",
					 tag, __func__);
			}

		}
#endif
#ifdef CHARGER_MODE
		if ((info->charger_enabled > 0 && isSystemResettedUp())
		    || force == 1) {
			logError(0, "%s %s: Charger Mode setting... \n", tag,
				 __func__);

			settings[0] = info->charger_enabled;
			ret = setFeatures(FEAT_SEL_CHARGER, settings, 1);
			if (ret < OK) {
				logError(1,
					 "%s %s: error during setting CHARGER_MODE! ERROR %08X\n",
					 tag, __func__, ret);
			}
			res |= ret;

			if (ret >= OK && info->charger_enabled == FEAT_ENABLE) {
				fromIDtoMask(FEAT_SEL_CHARGER,
					     (u8 *)&info->mode,
					     sizeof(info->mode));
				logError(1, "%s %s: CHARGER_MODE Enabled! \n",
					 tag, __func__);
			} else {
				logError(1, "%s %s: CHARGER_MODE Disabled! \n",
					 tag, __func__);
			}

		}
#endif

#ifdef GRIP_MODE
		if ((info->grip_enabled == FEAT_ENABLE && isSystemResettedUp())
		    || force == 1) {
			logError(0, "%s %s: Grip Mode setting... \n", tag,
				 __func__);
			settings[0] = info->grip_enabled;
			ret = setFeatures(FEAT_SEL_GRIP, settings, 1);
			if (ret < OK) {
				logError(1,
					 "%s %s: error during setting GRIP_MODE! ERROR %08X\n",
					 tag, __func__, ret);
			}
			res |= ret;

			if (ret >= OK && info->grip_enabled == FEAT_ENABLE) {
				fromIDtoMask(FEAT_SEL_GRIP, (u8 *)&info->mode,
					     sizeof(info->mode));
				logError(1, "%s %s: GRIP_MODE Enabled! \n", tag,
					 __func__);
			} else {
				logError(1, "%s %s: GRIP_MODE Disabled! \n",
					 tag, __func__);
			}

		}
#endif
#ifdef CONFIG_INPUT_PRESS_NDT
		if (fts_fod_status) {
			logError(1, "%s %s: Sense OFF \n", tag, __func__);
			res |= setScanMode(SCAN_MODE_ACTIVE, 0x00);
			logError(1, "%s %s: Sense ON without cal \n", tag, __func__);
			res |= setScanMode(SCAN_MODE_ACTIVE, 0x20);
		} else {
			logError(1, "%s %s: Sense ON\n", tag, __func__);
			res |= setScanMode(SCAN_MODE_ACTIVE, 0x01);
		}
#else
		settings[0] = 0x01;
		logError(1, "%s %s: Sense ON! \n", tag, __func__);
		res |= setScanMode(SCAN_MODE_ACTIVE, settings[0]);
		info->mode |= (SCAN_MODE_ACTIVE << 24);
		MODE_ACTIVE(info->mode, settings[0]);
#endif
		setSystemResetedUp(0);
		break;

	default:
		logError(1,
			 "%s %s: invalid resume_bit value = %d! ERROR %08X \n",
			 tag, __func__, info->resume_bit, ERROR_OP_NOT_ALLOW);
		res = ERROR_OP_NOT_ALLOW;
	}

	logError(0, "%s %s: Mode Handler finished! res = %08X mode = %08X \n",
		 tag, __func__, res, info->mode);
	return res;

}

/**
 * Resume work function which perform a system reset, clean all the touches from the linux input system and prepare the ground for enabling the sensing
 */
static void fts_resume_work(struct work_struct *work)
{
	struct fts_ts_info *info;
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_SENSOR
	int res;
#endif
	info = container_of(work, struct fts_ts_info, resume_work);

#ifdef CONFIG_SECURE_TOUCH
	fts_secure_stop(info, true);
#endif

#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_SENSOR
	res = fts_finger_report_disable(info, false);
	if (res < OK)
			logError(1, "%s %s: enable finger report error\n", tag, __func__);
	else
			logError(1, "%s %s: enable finger report\n", tag, __func__);

	if (!info->sensor_sleep || info->resume_bit) {
		logError(1, "%s %s: skip resume\n", tag, __func__);
		return;
	}
#endif

	info->resume_bit = 1;
#ifdef CONFIG_INPUT_PRESS_NDT
	if (!fts_fod_status) {
#endif
		fts_system_reset();
		release_all_touches(info);
#ifdef CONFIG_INPUT_PRESS_NDT
	}
#endif

	fts_mode_handler(info, 0);
	info->sensor_sleep = false;

	fts_enableInterrupt();
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_SENSOR
	if (info->palm_sensor_switch && !info->palm_sensor_changed) {
		fts_palm_sensor_cmd(info->palm_sensor_switch);
		info->palm_sensor_changed = true;
	}
	if (info->p_sensor_switch && !info->p_sensor_changed) {
		fts_p_sensor_cmd(info->p_sensor_switch);
		info->p_sensor_changed = true;
	}
#endif
}

/**
 * Suspend work function which clean all the touches from Linux input system and prepare the ground to disabling the sensing or enter in gesture mode
 */
static void fts_suspend_work(struct work_struct *work)
{
	struct fts_ts_info *info;
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_SENSOR
	int res;
#endif
	info = container_of(work, struct fts_ts_info, suspend_work);

#ifdef CONFIG_SECURE_TOUCH
	fts_secure_stop(info, true);
#endif

#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_SENSOR
	if (fts_info->p_sensor_switch) {
		res = fts_finger_report_disable(info, true);
		if (res < OK)
				logError(1, "%s %s: disable finger report error\n", tag, __func__);
		else
				logError(1, "%s %s: disable finger report\n", tag, __func__);
		logError(0, "%s %s: eardet enabled, disable finger report and skip suspend\n", tag, __func__);
		return;
	}
	if (info->palm_sensor_switch) {
		logError(1, "%s %s: palm sensor on status, switch to off\n", tag,
			 __func__);
		update_palm_sensor_value(0);
		fts_palm_sensor_cmd(0);
		info->palm_sensor_switch = false;
	}
#endif
	info->resume_bit = 0;
	fts_mode_handler(info, 0);

	release_all_touches(info);

	info->sensor_sleep = true;
}

#ifdef CONFIG_DRM
/**@}*/

/**
 * Callback function used to detect the suspend/resume events generated by clicking the power button.
 * This function schedule a suspend or resume work according to the event received.
 */
static int fts_drm_state_chg_callback(struct notifier_block *nb,
				      unsigned long val, void *data)
{
	struct fts_ts_info *info =
	    container_of(nb, struct fts_ts_info, notifier);
	struct fb_event *evdata = data;
	unsigned int blank;

	logError(0, "%s %s: fts notifier begin!\n", tag, __func__);

	if (evdata && evdata->data && info) {

		blank = *(int *)(evdata->data);
		logError(1, "%s %s: val:%lu,blank:%u\n", tag, __func__, val, blank);

		flush_workqueue(info->event_wq);
		if (val == DRM_EARLY_EVENT_BLANK && blank == DRM_BLANK_POWERDOWN) {
			if (info->sensor_sleep)
				return NOTIFY_OK;

			logError(1, "%s %s: FB_BLANK_POWERDOWN\n", tag, __func__);
			queue_work(info->event_wq, &info->suspend_work);
		} else if (val == DRM_EVENT_BLANK && blank == DRM_BLANK_UNBLANK) {
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_SENSOR
			if (!info->sensor_sleep && !info->p_sensor_switch)
				return NOTIFY_OK;
#else
			if (!info->sensor_sleep)
				return NOTIFY_OK;
#endif
			logError(1, "%s %s: FB_BLANK_UNBLANK\n", tag, __func__);

			queue_work(info->event_wq, &info->resume_work);
		}
	}
	return NOTIFY_OK;
}

static struct notifier_block fts_noti_block = {
	.notifier_call = fts_drm_state_chg_callback,
};
#endif

static int fts_bl_state_chg_callback(struct notifier_block *nb,
				      unsigned long val, void *data)
{
	struct fts_ts_info *info = container_of(nb, struct fts_ts_info, bl_notifier);
	unsigned int blank;
	int ret;

	if (val != BACKLIGHT_UPDATED)
		return NOTIFY_OK;
	if (data && info) {
		blank = *(int *)(data);
		logError(1, "%s %s: val:%lu,blank:%u\n", tag, __func__, val, blank);
		flush_workqueue(info->event_wq);
		if (blank == BACKLIGHT_OFF) {
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_SENSOR
			if (info->p_sensor_switch) {
				logError(1, "%s eardet enabled, skip disableirq\n", tag, __func__);
				return NOTIFY_OK;
			}
#endif
			if (info->sensor_sleep)
				return NOTIFY_OK;
			logError(1, "%s %s: BL_EVENT_BLANK\n", tag, __func__);
			ret = fts_disableInterrupt();
			if (ret < OK)
				logError(1, "%s fts_disableInterrupt ERROR %08X\n", tag, ret | ERROR_ENABLE_INTER);
			else
				logError(1, "%s fts_disableInterrupt\n", tag, ret | ERROR_ENABLE_INTER);
		} else if (blank == BACKLIGHT_ON) {
			logError(1, "%s %s: BL_EVENT_UNBLANK\n", tag, __func__);
			if (!info->sensor_sleep) {
				ret = fts_enableInterrupt();
				if (ret < OK)
					logError(1, "%s fts_enableInterrupt ERROR %08X\n", tag, ret | ERROR_ENABLE_INTER);
				else
					logError(1, "%s fts_enableInterrupt\n", tag, ret | ERROR_ENABLE_INTER);
			}
		}
	}
	return NOTIFY_OK;
}

static struct notifier_block fts_bl_noti_block = {
	.notifier_call = fts_bl_state_chg_callback,
};

/**
 * From the name of the power regulator get/put the actual regulator structs (copying their references into fts_ts_info variable)
 * @param info pointer to fts_ts_info which contains info about the device and its hw setup
 * @param get if 1, the regulators are get otherwise they are put (released) back to the system
 * @return OK if success or an error code which specify the type of error encountered
 */
static int fts_get_reg(struct fts_ts_info *info, bool get)
{
	int retval;
	const struct fts_hw_platform_data *bdata = info->board;

	if (!get) {
		retval = 0;
		goto regulator_put;
	}

	if ((bdata->vdd_reg_name != NULL) && (*bdata->vdd_reg_name != 0)) {
		info->vdd_reg = regulator_get(info->dev, bdata->vdd_reg_name);
		if (IS_ERR(info->vdd_reg)) {
			logError(1, "%s %s: Failed to get power regulator\n",
				 tag, __func__);
			retval = PTR_ERR(info->vdd_reg);
			goto regulator_put;
		}
	}

	if ((bdata->avdd_reg_name != NULL) && (*bdata->avdd_reg_name != 0)) {
		info->avdd_reg = regulator_get(info->dev, bdata->avdd_reg_name);
		if (IS_ERR(info->avdd_reg)) {
			logError(1,
				 "%s %s: Failed to get bus pullup regulator\n",
				 tag, __func__);
			retval = PTR_ERR(info->avdd_reg);
			goto regulator_put;
		}
	}

	return OK;

regulator_put:
	if (info->vdd_reg) {
		regulator_put(info->vdd_reg);
		info->vdd_reg = NULL;
	}

	if (info->avdd_reg) {
		regulator_put(info->avdd_reg);
		info->avdd_reg = NULL;
	}

	return retval;
}

/**
 * Enable or disable the power regulators
 * @param info pointer to fts_ts_info which contains info about the device and its hw setup
 * @param enable if 1, the power regulators are turned on otherwise they are turned off
 * @return OK if success or an error code which specify the type of error encountered
 */
static int fts_enable_reg(struct fts_ts_info *info, bool enable)
{
	int retval;

	if (!enable) {
		retval = 0;
		goto disable_pwr_reg;
	}

	if (info->vdd_reg) {
		retval = regulator_enable(info->vdd_reg);
		if (retval < 0) {
			logError(1, "%s %s: Failed to enable bus regulator\n",
				 tag, __func__);
			goto exit;
		}
	}

	if (info->avdd_reg) {
		retval = regulator_enable(info->avdd_reg);
		if (retval < 0) {
			logError(1, "%s %s: Failed to enable power regulator\n",
				 tag, __func__);
			goto disable_bus_reg;
		}
	}

	return OK;

disable_pwr_reg:
	if (info->avdd_reg)
		regulator_disable(info->vdd_reg);

disable_bus_reg:
	if (info->vdd_reg)
		regulator_disable(info->avdd_reg);

exit:
	return retval;
}

/**
 * Configure a GPIO according to the parameters
 * @param gpio gpio number
 * @param config if true, the gpio is set up otherwise it is free
 * @param dir direction of the gpio, 0 = in, 1 = out
 * @param state initial value (if the direction is in, this parameter is ignored)
 * return error code
 */
static int fts_gpio_setup(int gpio, bool config, int dir, int state)
{
	int retval = 0;
	unsigned char buf[16];

	if (config) {
		snprintf(buf, 16, "fts_gpio_%u\n", gpio);

		retval = gpio_request(gpio, buf);
		if (retval) {
			logError(1, "%s %s: Failed to get gpio %d (code: %d)",
				 tag, __func__, gpio, retval);
			return retval;
		}

		if (dir == 0)
			retval = gpio_direction_input(gpio);
		else
			retval = gpio_direction_output(gpio, state);
		if (retval) {
			logError(1, "%s %s: Failed to set gpio %d direction",
				 tag, __func__, gpio);
			return retval;
		}
	} else {
		gpio_free(gpio);
	}

	return retval;
}

/**
 * Setup the IRQ and RESET (if present) gpios.
 * If the Reset Gpio is present it will perform a cycle HIGH-LOW-HIGH in order to assure that the IC has been reset properly
 */
static int fts_set_gpio(struct fts_ts_info *info)
{
	int retval;
	struct fts_hw_platform_data *bdata = info->board;

	retval = fts_gpio_setup(bdata->irq_gpio, true, 0, 0);
	if (retval < 0) {
		logError(1, "%s %s: Failed to configure irq GPIO\n", tag,
			 __func__);
		goto err_gpio_irq;
	}

	if (bdata->reset_gpio >= 0) {
		retval = fts_gpio_setup(bdata->reset_gpio, true, 1, 0);
		if (retval < 0) {
			logError(1, "%s %s: Failed to configure reset GPIO\n",
				 tag, __func__);
			goto err_gpio_reset;
		}
	}
	if (bdata->reset_gpio >= 0) {
		gpio_set_value(bdata->reset_gpio, 0);
		mdelay(10);
		gpio_set_value(bdata->reset_gpio, 1);
	}

	return OK;

err_gpio_reset:
	fts_gpio_setup(bdata->irq_gpio, false, 0, 0);
	bdata->reset_gpio = GPIO_NOT_DEFINED;
err_gpio_irq:
	return retval;
}

static int fts_pinctrl_init(struct fts_ts_info *info)
{
	int retval = 0;
	/* Get pinctrl if target uses pinctrl */
	info->ts_pinctrl = devm_pinctrl_get(info->dev);

	if (IS_ERR_OR_NULL(info->ts_pinctrl)) {
		retval = PTR_ERR(info->ts_pinctrl);
		dev_err(info->dev, "Target does not use pinctrl %d\n", retval);
		goto err_pinctrl_get;
	}

	info->pinctrl_state_active
	    = pinctrl_lookup_state(info->ts_pinctrl, PINCTRL_STATE_ACTIVE);

	if (IS_ERR_OR_NULL(info->pinctrl_state_active)) {
		retval = PTR_ERR(info->pinctrl_state_active);
		dev_err(info->dev, "Can not lookup %s pinstate %d\n",
			PINCTRL_STATE_ACTIVE, retval);
		goto err_pinctrl_lookup;
	}

	info->pinctrl_state_suspend
	    = pinctrl_lookup_state(info->ts_pinctrl, PINCTRL_STATE_SUSPEND);

	if (IS_ERR_OR_NULL(info->pinctrl_state_suspend)) {
		retval = PTR_ERR(info->pinctrl_state_suspend);
		dev_dbg(info->dev, "Can not lookup %s pinstate %d\n",
			PINCTRL_STATE_SUSPEND, retval);
		goto err_pinctrl_lookup;
	}

	return 0;
err_pinctrl_lookup:
	devm_pinctrl_put(info->ts_pinctrl);
err_pinctrl_get:
	info->ts_pinctrl = NULL;
	return retval;
}

#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_GAMEMODE
static int parse_gamemode_dt(struct device *dev, struct fts_hw_platform_data *bdata)
{
	int retval;
	u32 temp_val;
	struct device_node *np = dev->of_node;

	retval = of_property_read_u32(np, "fts,touch-up-threshold-min", &temp_val);
	if (retval < 0)
		return retval;
	else
		bdata->touch_up_threshold_min = temp_val;
	retval = of_property_read_u32(np, "fts,touch-up-threshold-max", &temp_val);
	if (retval < 0)
		return retval;
	else
		bdata->touch_up_threshold_max = temp_val;
	retval = of_property_read_u32(np, "fts,touch-up-threshold-def", &temp_val);
	if (retval < 0)
		return retval;
	else
		bdata->touch_up_threshold_def = temp_val;
	retval = of_property_read_u32(np, "fts,touch-tolerance-min", &temp_val);
	if (retval < 0)
		return retval;
	else
		bdata->touch_tolerance_min = temp_val;
	retval = of_property_read_u32(np, "fts,touch-tolerance-max", &temp_val);
	if (retval < 0)
		return retval;
	else
		bdata->touch_tolerance_max = temp_val;
	retval = of_property_read_u32(np, "fts,touch-tolerance-def", &temp_val);
	if (retval < 0)
		return retval;
	else
		bdata->touch_tolerance_def = temp_val;
	retval = of_property_read_u32(np, "fts,edgefilter-leftrigt-def", &temp_val);
	if (retval < 0)
		return retval;
	else
		bdata->edgefilter_leftright_def = temp_val;
	retval = of_property_read_u32(np, "fts,edgefilter-topbottom-def", &temp_val);
	if (retval < 0)
		return retval;
	else
		bdata->edgefilter_topbottom_def = temp_val;
	retval = of_property_read_u32(np, "fts,edgefilter-area-step1", &temp_val);
	if (retval < 0)
		return retval;
	else
		bdata->edgefilter_area_step1 = temp_val;
	retval = of_property_read_u32(np, "fts,edgefilter-area-step2", &temp_val);
	if (retval < 0)
		return retval;
	else
		bdata->edgefilter_area_step2 = temp_val;
	retval = of_property_read_u32(np, "fts,edgefilter-area-step3", &temp_val);
	if (retval < 0)
		return retval;
	else
		bdata->edgefilter_area_step3 = temp_val;
	return retval;

}
#endif

/**
 * Retrieve and parse the hw information from the device tree node defined in the system.
 * the most important information to obtain are: IRQ and RESET gpio numbers, power regulator names
 * In the device file node is possible to define additional optional information that can be parsed here.
 */
static int parse_dt(struct device *dev, struct fts_hw_platform_data *bdata)
{
	int retval;
	const char *name;
	struct device_node *temp, *np = dev->of_node;
	struct fts_config_info *config_info;
	u32 temp_val;

	bdata->irq_gpio = of_get_named_gpio_flags(np, "fts,irq-gpio", 0, NULL);

	logError(0, "%s irq_gpio = %d\n", tag, bdata->irq_gpio);

	retval = of_property_read_string(np, "fts,pwr-reg-name", &name);
	if (retval == -EINVAL)
		bdata->vdd_reg_name = NULL;
	else if (retval < 0)
		return retval;
	else {
		bdata->vdd_reg_name = name;
		logError(0, "%s pwr_reg_name = %s\n", tag, name);
	}

	retval = of_property_read_string(np, "fts,bus-reg-name", &name);
	if (retval == -EINVAL)
		bdata->avdd_reg_name = NULL;
	else if (retval < 0)
		return retval;
	else {
		bdata->avdd_reg_name = name;
		logError(0, "%s bus_reg_name = %s\n", tag, name);
	}

	if (of_property_read_bool(np, "fts,reset-gpio-enable")) {
		bdata->reset_gpio = of_get_named_gpio_flags(np,
							    "fts,reset-gpio", 0,
							    NULL);
		logError(0, "%s reset_gpio =%d\n", tag, bdata->reset_gpio);
	} else {
		bdata->reset_gpio = GPIO_NOT_DEFINED;
	}

	retval = of_property_read_u32(np, "fts,irq-flags", &temp_val);
	if (retval < 0)
		return retval;
	else
		bdata->irq_flags = temp_val;
	retval = of_property_read_u32(np, "fts,x-max", &temp_val);
	if (retval < 0)
		bdata->x_max = X_AXIS_MAX;
	else
		bdata->x_max = temp_val;

	retval = of_property_read_u32(np, "fts,y-max", &temp_val);
	if (retval < 0)
		bdata->y_max = Y_AXIS_MAX;
	else
		bdata->y_max = temp_val;
	retval = of_property_read_string(np, "fts,default-fw-name",
					 &bdata->default_fw_name);
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_GAMEMODE
	retval = parse_gamemode_dt(dev, bdata);
	if (retval < 0)
		logError(1, "%s Unable to parse gamemode parameters\n", tag);
#endif

	retval =
	    of_property_read_u32(np, "fts,config-array-size",
				 (u32 *)&bdata->config_array_size);

	if (retval) {
		logError(1, "%s Unable to get array size\n", tag);
		return retval;
	}

	bdata->config_array = devm_kzalloc(dev, bdata->config_array_size *
					   sizeof(struct fts_config_info),
					   GFP_KERNEL);

	if (!bdata->config_array) {
		logError(1, "%s Unable to allocate memory\n", tag);
		return -ENOMEM;
	}

	bdata->check_display_name = of_property_read_bool(np, "fts,check-display-name");

	config_info = bdata->config_array;
	for_each_child_of_node(np, temp) {
		retval = of_property_read_u32(temp, "fts,tp-vendor", &temp_val);

		if (retval) {
			logError(1, "%s Unable to read tp vendor\n", tag);
		} else {
			config_info->tp_vendor = (u8) temp_val;
			logError(1, "%s %s:tp vendor: %u", tag, __func__,
				 config_info->tp_vendor);
		}
		retval = of_property_read_u32(temp, "fts,tp-color", &temp_val);
		if (retval) {
			logError(1, "%s Unable to read tp color\n", tag);
		} else {
			config_info->tp_color = (u8) temp_val;
			logError(1, "%s %s:tp color: %u", tag, __func__,
				 config_info->tp_color);
		}

		retval =
		    of_property_read_u32(temp, "fts,tp-hw-version", &temp_val);

		if (retval) {
			logError(1, "%s Unable to read tp hw version\n", tag);
		} else {
			config_info->tp_hw_version = (u8) temp_val;
			logError(1, "%s %s:tp color: %u", tag, __func__,
				 config_info->tp_hw_version);
		}

		retval = of_property_read_string(temp, "fts,fw-name",
						 &config_info->fts_cfg_name);

		if (retval && (retval != -EINVAL)) {
			logError(1, "%s Unable to read cfg name\n", tag);
		} else {
			logError(1, "%s %s:fw_name: %s", tag, __func__,
				 config_info->fts_cfg_name);
		}
		retval = of_property_read_string(temp, "fts,limit-name",
						 &config_info->fts_limit_name);

		if (retval && (retval != -EINVAL)) {
			logError(1, "%s Unable to read limit name\n", tag);
		} else {
			logError(1, "%s %s:limit_name: %s", tag, __func__,
				 config_info->fts_limit_name);
		}

		config_info++;
	}

	return OK;
}

static void fts_switch_mode_work(struct work_struct *work)
{
	struct fts_mode_switch *ms =
	    container_of(work, struct fts_mode_switch, switch_mode_work);

	struct fts_ts_info *info = ms->info;
	unsigned char value = ms->mode;
	static const char *fts_gesture_on = "01 20";
	char *gesture_result;
	int size = 6 * 2 + 1;

	logError(1, "%s %s mode:%d\n", tag, __func__, value);

	if (value >= INPUT_EVENT_WAKUP_MODE_OFF
	    && value <= INPUT_EVENT_WAKUP_MODE_ON) {
		info->gesture_enabled = value - INPUT_EVENT_WAKUP_MODE_OFF;
		if (info->gesture_enabled) {
			gesture_result = (u8 *) kzalloc(size, GFP_KERNEL);
			if (gesture_result != NULL) {
				fts_gesture_mask_store(info->dev, NULL,
						       fts_gesture_on,
						       strlen(fts_gesture_on));
				fts_gesture_mask_show(info->dev, NULL,
						      gesture_result);
				if (strncmp
				    ("{ 00000000 }", gesture_result, size - 1))
					logError(1,
						 "%s %s:store gesture mask error\n",
						 tag, __func__);
				kfree(gesture_result);
				gesture_result = NULL;
			}
		}
	} else if (value >= INPUT_EVENT_COVER_MODE_OFF
		   && value <= INPUT_EVENT_COVER_MODE_ON) {
		info->glove_enabled = value - INPUT_EVENT_COVER_MODE_OFF;
		fts_mode_handler(info, 1);
	}
#ifdef EDGEHOVER_FOR_VOLUME
	if (value >= INPUT_EVENT_SLIDE_FOR_VOLUME
	    && value <= INPUT_EVENT_LONG_SINGLE_TAP_FOR_VOLUME) {
		info->volume_type = value;
		if (fts_info->volume_type == INPUT_EVENT_SINGLE_TAP_FOR_VOLUME) {
			fts_info->single_press_time_low = 30;
			fts_info->single_press_time_hi = 800;
		} else if (fts_info->volume_type ==
			   INPUT_EVENT_LONG_SINGLE_TAP_FOR_VOLUME) {
			fts_info->single_press_time_low = 300;
			fts_info->single_press_time_hi = 800;
		}
	}
#endif
#ifdef PHONE_PALM
	if (value >= INPUT_EVENT_PALM_OFF && value <= INPUT_EVENT_PALM_ON)
		info->palm_enabled = value - INPUT_EVENT_PALM_OFF;
#endif
	if (ms != NULL) {
		kfree(ms);
		ms = NULL;
	}
}

static int fts_input_event(struct input_dev *dev, unsigned int type,
			   unsigned int code, int value)
{
	struct fts_ts_info *info = input_get_drvdata(dev);
	struct fts_mode_switch *ms;

	logError(1, "%s %s:set input event value = %d\n", tag, __func__, value);

	if (!info) {
		printk("%s fts_ts_info is NULL\n", __func__);
		return 0;
	}

	if (type == EV_SYN && code == SYN_CONFIG) {
		if (value >= INPUT_EVENT_START && value <= INPUT_EVENT_END) {
			ms = (struct fts_mode_switch *)
			    kmalloc(sizeof(struct fts_mode_switch), GFP_ATOMIC);

			if (ms != NULL) {
				ms->info = info;
				ms->mode = (unsigned char)value;
				INIT_WORK(&ms->switch_mode_work,
					  fts_switch_mode_work);
				schedule_work(&ms->switch_mode_work);
			} else {
				logError(1,
					 "%s %s:failed in allocating memory for switching mode\n",
					 tag, __func__);
				return -ENOMEM;
			}
		} else {
			logError(1, "%s %s:Invalid event value\n", tag,
				 __func__);
			return -EINVAL;
		}
	}

	return 0;
}

static int fts_short_open_test(void)
{
	TestToDo selftests;
	int res = -1;
	int init_type = SPECIAL_PANEL_INIT;

	selftests.MutualRawAdjITO = 0;
	selftests.MutualRaw = 0;
	selftests.MutualRawEachNode = 1;
	selftests.MutualRawGap = 0;
	selftests.MutualRawAdj = 0;
	selftests.MutualRawLP = 0;
	selftests.MutualRawGapLP = 0;
	selftests.MutualRawAdjLP = 0;
	selftests.MutualCx1 = 0;
	selftests.MutualCx2 = 0;
	selftests.MutualCx2Adj = 0;
	selftests.MutualCxTotal = 0;
	selftests.MutualCxTotalAdj = 0;
	selftests.MutualCx1LP = 0;
	selftests.MutualCx2LP = 0;
	selftests.MutualCx2AdjLP = 0;
	selftests.MutualCxTotalLP = 0;
	selftests.MutualCxTotalAdjLP = 0;
#ifdef PHONE_KEY
	selftests.MutualKeyRaw = 0;
#else
	selftests.MutualKeyRaw = 0;
#endif
	selftests.MutualKeyCx1 = 0;
	selftests.MutualKeyCx2 = 0;
#ifdef PHONE_KEY
	selftests.MutualKeyCxTotal = 0;
#else
	selftests.MutualKeyCxTotal = 0;
#endif
	selftests.SelfForceRaw = 1;
	selftests.SelfForceRawGap = 0;
	selftests.SelfForceRawLP = 0;
	selftests.SelfForceRawGapLP = 0;
	selftests.SelfForceIx1 = 0;
	selftests.SelfForceIx2 = 0;
	selftests.SelfForceIx2Adj = 0;
	selftests.SelfForceIxTotal = 0;
	selftests.SelfForceIxTotalAdj = 0;
	selftests.SelfForceCx1 = 0;
	selftests.SelfForceCx2 = 0;
	selftests.SelfForceCx2Adj = 0;
	selftests.SelfForceCxTotal = 0;
	selftests.SelfForceCxTotalAdj = 0;
	selftests.SelfSenseRaw = 1;
	selftests.SelfSenseRawGap = 0;
	selftests.SelfSenseRawLP = 0;
	selftests.SelfSenseRawGapLP = 0;
	selftests.SelfSenseIx1 = 0;
	selftests.SelfSenseIx2 = 0;
	selftests.SelfSenseIx2Adj = 0;
	selftests.SelfSenseIxTotal = 0;
	selftests.SelfSenseIxTotalAdj = 0;
	selftests.SelfSenseCx1 = 0;
	selftests.SelfSenseCx2 = 0;
	selftests.SelfSenseCx2Adj = 0;
	selftests.SelfSenseCxTotal = 0;
	selftests.SelfSenseCxTotalAdj = 0;

	res = fts_disableInterrupt();
	if (res < 0) {
		logError(0, "%s fts_disableInterrupt: ERROR %08X \n",
			 tag, res);
		res = (res | ERROR_DISABLE_INTER);
		goto END;
	}
	res = production_test_main(LIMITS_FILE, 1, init_type, &selftests);
END:
	fts_mode_handler(fts_info, 1);
	fts_enableInterrupt();
	if (res == OK)
		return FTS_RESULT_PASS;
	else
		return FTS_RESULT_FAIL;
}

static int fts_i2c_test(void)
{
	int ret = 0;
	u8 data[SYS_INFO_SIZE] = { 0 };

	logError(0, "%s %s: Reading System Info...\n", tag, __func__);
	ret =
	    fts_writeReadU8UX(FTS_CMD_FRAMEBUFFER_R, BITS_16, ADDR_FRAMEBUFFER,
			      data, SYS_INFO_SIZE, DUMMY_FRAMEBUFFER);
	if (ret < OK) {
		logError(1,
			 "%s %s: error while reading the system data ERROR %08X\n",
			 tag, __func__, ret);
		return FTS_RESULT_FAIL;
	}

	return FTS_RESULT_PASS;
}

static ssize_t fts_selftest_read(struct file *file, char __user *buf,
				 size_t count, loff_t *pos)
{
	char tmp[5] = { 0 };
	int cnt;

	if (*pos != 0)
		return 0;
	cnt =
	    snprintf(tmp, sizeof(fts_info->result_type), "%d\n",
		     fts_info->result_type);
	if (copy_to_user(buf, tmp, strlen(tmp))) {
		return -EFAULT;
	}
	*pos += cnt;
	return cnt;
}

static ssize_t fts_selftest_write(struct file *file, const char __user *buf,
				  size_t count, loff_t *pos)
{
	int retval = 0;
	char tmp[6];

	if (copy_from_user(tmp, buf, count)) {
		retval = -EFAULT;
		goto out;
	}

	if (!strncmp("short", tmp, 5) || !strncmp("open", tmp, 4)) {
		retval = fts_short_open_test();
	} else if (!strncmp("i2c", tmp, 3))
		retval = fts_i2c_test();

	fts_info->result_type = retval;
out:
	if (retval >= 0)
		retval = count;

	return retval;
}

static const struct file_operations fts_selftest_ops = {
	.read = fts_selftest_read,
	.write = fts_selftest_write,
};

static ssize_t fts_datadump_read(struct file *file, char __user *buf,
				 size_t count, loff_t *pos)
{
	int ret = 0, cnt1 = 0, cnt2 = 0, cnt3 = 0;
	char *tmp;

	if (*pos != 0)
		return 0;

	tmp = vmalloc(PAGE_SIZE * 2);
	if (tmp == NULL)
		return 0;
	else
		memset(tmp, 0, PAGE_SIZE * 2);

	cnt1 = fts_strength_frame_show(fts_info->dev, NULL, tmp);
	if (cnt1 == 0) {
		ret = 0;
		goto out;
	}

	ret = stm_fts_cmd_store(fts_info->dev, NULL, "13", 2);
	if (ret == 0)
		goto out;
	cnt2 = stm_fts_cmd_show(fts_info->dev, NULL, tmp + cnt1);
	if (cnt2 == 0) {
		ret = 0;
		goto out;
	}

	ret = stm_fts_cmd_store(fts_info->dev, NULL, "15", 2);
	if (ret == 0)
		goto out;
	cnt3 = stm_fts_cmd_show(fts_info->dev, NULL, tmp + cnt1 + cnt2);
	if (cnt3 == 0) {
		ret = 0;
		goto out;
	}

	if (copy_to_user(buf, tmp, cnt1 + cnt2 + cnt3))
		ret = -EFAULT;

out:
	if (tmp) {
		vfree(tmp);
		tmp = NULL;
	}
	*pos += (cnt1 + cnt2 + cnt3);
	if (ret <= 0)
		return ret;
	return cnt1 + cnt2 + cnt3;
}

static const struct file_operations fts_datadump_ops = {
	.read = fts_datadump_read,
};

#define TP_INFO_MAX_LENGTH 50

static ssize_t fts_fw_version_read(struct file *file, char __user *buf,
				   size_t count, loff_t *pos)
{
	int cnt = 0, ret = 0;
	char tmp[TP_INFO_MAX_LENGTH];

	if (*pos != 0)
		return 0;

	cnt =
	    snprintf(tmp, TP_INFO_MAX_LENGTH, "%x.%x\n", systemInfo.u16_fwVer,
		     systemInfo.u16_cfgVer);
	ret = copy_to_user(buf, tmp, cnt);
	*pos += cnt;
	if (ret != 0)
		return 0;
	else
		return cnt;
}

static const struct file_operations fts_fw_version_ops = {
	.read = fts_fw_version_read,
};

static ssize_t fts_lockdown_info_read(struct file *file, char __user *buf,
				      size_t count, loff_t *pos)
{
	int cnt = 0, ret = 0;
	char tmp[TP_INFO_MAX_LENGTH];

	if (*pos != 0)
		return 0;

	ret = fts_get_lockdown_info(fts_info->lockdown_info, fts_info);
	if (ret != OK) {
		logError(1, "%s %s get lockdown info error\n", tag, __func__);
		goto out;
	}

	cnt =
	    snprintf(tmp, TP_INFO_MAX_LENGTH,
		     "0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x\n",
		     fts_info->lockdown_info[0], fts_info->lockdown_info[1],
		     fts_info->lockdown_info[2], fts_info->lockdown_info[3],
		     fts_info->lockdown_info[4], fts_info->lockdown_info[5],
		     fts_info->lockdown_info[6], fts_info->lockdown_info[7]);
	ret = copy_to_user(buf, tmp, cnt);
out:
	*pos += cnt;
	if (ret != 0)
		return 0;
	else
		return cnt;
}

static const struct file_operations fts_lockdown_info_ops = {
	.read = fts_lockdown_info_read,
};

#ifdef CONFIG_PM
static int fts_pm_suspend(struct device *dev)
{
	struct fts_ts_info *info = dev_get_drvdata(dev);

	info->dev_pm_suspend = true;
#ifndef CONFIG_INPUT_PRESS_NDT
	if (device_may_wakeup(dev) && info->gesture_enabled) {
		logError(1, "%s enable touch irq wake\n", tag);
		enable_irq_wake(info->client->irq);
	}
#else
	enable_irq_wake(info->client->irq);
#endif
	reinit_completion(&info->dev_pm_suspend_completion);

	return 0;

}

static int fts_pm_resume(struct device *dev)
{
	struct fts_ts_info *info = dev_get_drvdata(dev);

	info->dev_pm_suspend = false;
#ifndef CONFIG_INPUT_PRESS_NDT
	if (device_may_wakeup(dev) && info->gesture_enabled) {
		logError(1, "%s disable touch irq wake\n", tag);
		disable_irq_wake(info->client->irq);
	}
#else
	disable_irq_wake(info->client->irq);
#endif
	complete(&info->dev_pm_suspend_completion);

	return 0;
}

static const struct dev_pm_ops fts_dev_pm_ops = {
	.suspend = fts_pm_suspend,
	.resume = fts_pm_resume,
};
#endif

#ifdef CONFIG_TOUCHSCREEN_ST_DEBUG_FS
static void tpdbg_shutdown(struct fts_ts_info *info, bool sleep)
{
	u8 settings[4] = { 0 };
	info->mode = MODE_NOTHING;

	if (sleep) {
		logError(0, "%s %s: Sense OFF! \n", tag, __func__);
		setScanMode(SCAN_MODE_ACTIVE, 0x00);
	} else {
		settings[0] = 0x01;
		logError(0, "%s %s: Sense ON! \n", tag, __func__);
		setScanMode(SCAN_MODE_ACTIVE, settings[0]);
		info->mode |= (SCAN_MODE_ACTIVE << 24);
		MODE_ACTIVE(info->mode, settings[0]);
	}
}

static void tpdbg_suspend(struct fts_ts_info *info, bool enable)
{
	if (enable)
		queue_work(info->event_wq, &info->suspend_work);
	else
		queue_work(info->event_wq, &info->resume_work);
}

static int tpdbg_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;

	return 0;
}

static ssize_t tpdbg_read(struct file *file, char __user *buf, size_t size,
			  loff_t *ppos)
{
	const char *str = "cmd support as below:\n \
				\necho \"irq-disable\" or \"irq-enable\" to ctrl irq\n \
				\necho \"tp-sd-en\" of \"tp-sd-off\" to ctrl panel in or off sleep mode\n \
				\necho \"tp-suspend-en\" or \"tp-suspend-off\" to ctrl panel in or off suspend status\n";

	loff_t pos = *ppos;
	int len = strlen(str);

	if (pos < 0)
		return -EINVAL;
	if (pos >= len)
		return 0;

	if (copy_to_user(buf, str, len))
		return -EFAULT;

	*ppos = pos + len;

	return len;
}

static ssize_t tpdbg_write(struct file *file, const char __user *buf,
			   size_t size, loff_t *ppos)
{
	struct fts_ts_info *info = file->private_data;
	char *cmd = kzalloc(size + 1, GFP_KERNEL);
	int ret = size;

	if (!cmd)
		return -ENOMEM;

	if (copy_from_user(cmd, buf, size)) {
		ret = -EFAULT;
		goto out;
	}

	cmd[size] = '\0';

	if (!strncmp(cmd, "irq-disable", 11))
		disable_irq(info->client->irq);
	else if (!strncmp(cmd, "irq-enable", 10))
		enable_irq(info->client->irq);
	else if (!strncmp(cmd, "tp-sd-en", 8))
		tpdbg_shutdown(info, true);
	else if (!strncmp(cmd, "tp-sd-off", 9))
		tpdbg_shutdown(info, false);
	else if (!strncmp(cmd, "tp-suspend-en", 13))
		tpdbg_suspend(info, true);
	else if (!strncmp(cmd, "tp-suspend-off", 14))
		tpdbg_suspend(info, false);
out:
	kfree(cmd);

	return ret;
}

static int tpdbg_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;

	return 0;
}

static const struct file_operations tpdbg_operations = {
	.owner = THIS_MODULE,
	.open = tpdbg_open,
	.read = tpdbg_read,
	.write = tpdbg_write,
	.release = tpdbg_release,
};
#endif

#ifdef CONFIG_SECURE_TOUCH
int fts_secure_init(struct fts_ts_info *info)
{
	int ret;
	struct fts_secure_info *scr_info = kmalloc(sizeof(*scr_info), GFP_KERNEL);
	if (!scr_info) {
		logError(1, "%s %s alloc fts_secure_info failed\n", tag, __func__);
		return -ENOMEM;
	}

	logError(1, "%s fts_secure_init\n", tag);

//	mutex_init(&scr_info->palm_lock);

	init_completion(&scr_info->st_powerdown);
	init_completion(&scr_info->st_irq_processed);

	atomic_set(&scr_info->st_enabled, 0);
	atomic_set(&scr_info->st_pending_irqs, 0);

	info->secure_info = scr_info;

	ret = sysfs_create_file(&info->dev->kobj, &dev_attr_secure_touch_enable.attr);
	if (ret < 0) {
		logError(1, "%s %s create sysfs attribute secure_touch_enable failed\n", tag, __func__);
		goto err;
	}

	ret = sysfs_create_file(&info->dev->kobj, &dev_attr_secure_touch.attr);
	if (ret < 0) {
		logError(1, "%s %s create sysfs attribute secure_touch failed\n", tag, __func__);
		goto err;
	}

	scr_info->fts_info = info;
	scr_info->secure_inited = true;

	return 0;

err:
	kfree(scr_info);
	info->secure_info = NULL;
	return ret;
}

void fts_secure_remove(struct fts_ts_info *info)
{
	struct fts_secure_info *scr_info = info->secure_info;

	sysfs_remove_file(&info->dev->kobj, &dev_attr_secure_touch_enable.attr);
	sysfs_remove_file(&info->dev->kobj, &dev_attr_secure_touch.attr);
	kfree(scr_info);
}

#endif


/**
 * Probe function, called when the driver it is matched with a device with the same name compatible name
 * This function allocate, initialize and define all the most important function and flow that are used by the driver to operate with the IC.
 * It allocates device variables, initialize queues and schedule works, registers the IRQ handler, suspend/resume callbacks, registers the device to the linux input subsystem etc.
 */
#ifdef I2C_INTERFACE
static int fts_probe(struct i2c_client *client, const struct i2c_device_id *idp)
{
#else
static int fts_probe(struct spi_device *client)
{
#endif

	struct fts_ts_info *info = NULL;
	int error = 0;
	struct device_node *dp = client->dev.of_node;
	int retval;
	int skip_5_1 = 0;
	u16 bus_type;
	u8 *tp_maker;
	const char *display_name;

	logError(1, "%s %s: driver ver: %s\n", tag, __func__,
		 FTS_TS_DRV_VERSION);

#ifdef I2C_INTERFACE
	logError(0, "%s I2C interface... \n", tag);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		logError(1, "%s Unsupported I2C functionality\n", tag);
		error = -EIO;
		goto ProbeErrorExit_0;
	}

	logError(0, "%s i2c address: %x \n", tag, client->addr);
	bus_type = BUS_I2C;
#else
	logError(0, "%s SPI interface... \n", tag);
	client->mode = SPI_MODE_0;
#ifndef SPI4_WIRE
	client->mode |= SPI_3WIRE;
#endif
	client->max_speed_hz = SPI_CLOCK_FREQ;
	client->bits_per_word = 8;
	if (spi_setup(client) < 0) {
		logError(1, "%s Unsupported SPI functionality\n", tag);
		error = -EIO;
		goto ProbeErrorExit_0;
	}
	bus_type = BUS_SPI;
#endif

	logError(0, "%s SET Device driver INFO: \n", tag);

	info = kzalloc(sizeof(struct fts_ts_info), GFP_KERNEL);
	if (!info) {
		logError(1,
			 "%s Out of memory... Impossible to allocate struct info!\n",
			 tag);
		error = -ENOMEM;
		goto ProbeErrorExit_0;
	}

	info->client = client;
	info->dev = &info->client->dev;
	dev_set_drvdata(info->dev, info);

	if (dp) {
		info->board =
		    devm_kzalloc(&client->dev,
				 sizeof(struct fts_hw_platform_data),
				 GFP_KERNEL);
		if (!info->board) {
			logError(1, "%s ERROR:info.board kzalloc failed \n",
				 tag);
			goto ProbeErrorExit_1;
		}
		parse_dt(&client->dev, info->board);
	}
	if (info->board->check_display_name) {
		display_name = dsi_get_display_name();
		if (display_name) {
			logError(1, "%s display_name:%s\n", tag, display_name);
			if (strncmp(display_name, "dsi_samsung", 11)) {
				logError(1, "%s not the right display, do not need to do probe%s\n", tag);
				return -EINVAL;
			}
		}
	}

	logError(0, "%s SET Regulators: \n", tag);
	retval = fts_get_reg(info, true);
	if (retval < 0) {
		logError(1, "%s ERROR: %s: Failed to get regulators\n", tag,
			 __func__);
		error = -EINVAL;
		goto ProbeErrorExit_1;
	}

	retval = fts_enable_reg(info, true);
	if (retval < 0) {
		logError(1, "%s %s: ERROR Failed to enable regulators\n", tag,
			 __func__);
		goto ProbeErrorExit_2;
	}

	logError(0, "%s SET GPIOS: \n", tag);
	retval = fts_set_gpio(info);
	if (retval < 0) {
		logError(1, "%s %s: ERROR Failed to set up GPIO's\n", tag,
			 __func__);
		error = -EINVAL;
		goto ProbeErrorExit_2;
	}

	error = fts_pinctrl_init(info);

	if (!error && info->ts_pinctrl) {
		error =
		    pinctrl_select_state(info->ts_pinctrl,
					 info->pinctrl_state_active);

		if (error < 0) {
			dev_err(&client->dev,
				"%s: Failed to select %s pinstate %d\n",
				__func__, PINCTRL_STATE_ACTIVE, error);
		}
	} else {
		dev_err(&client->dev, "%s: Failed to init pinctrl\n", __func__);
	}

	info->client->irq = gpio_to_irq(info->board->irq_gpio);

	logError(0, "%s SET Event Handler: \n", tag);

	info->event_wq =
	    alloc_workqueue("fts-event-queue",
			    WQ_UNBOUND | WQ_HIGHPRI | WQ_CPU_INTENSIVE, 1);
	if (!info->event_wq) {
		logError(1, "%s ERROR: Cannot create work thread\n", tag);
		error = -ENOMEM;
		goto ProbeErrorExit_4;
	}
	INIT_WORK(&info->resume_work, fts_resume_work);
	INIT_WORK(&info->suspend_work, fts_suspend_work);
	init_completion(&info->tp_reset_completion);
	logError(0, "%s SET Input Device Property: \n", tag);
	info->dev = &info->client->dev;
	info->input_dev = input_allocate_device();
	if (!info->input_dev) {
		logError(1, "%s ERROR: No such input device defined! \n", tag);
		error = -ENODEV;
		goto ProbeErrorExit_5;
	}
	info->input_dev->dev.parent = &client->dev;
	info->input_dev->name = FTS_TS_DRV_NAME;
	snprintf(fts_ts_phys, sizeof(fts_ts_phys), "%s/input0",
		 info->input_dev->name);
	info->input_dev->phys = fts_ts_phys;
	info->input_dev->id.bustype = bus_type;
	info->input_dev->id.vendor = 0x0001;
	info->input_dev->id.product = 0x0002;
	info->input_dev->id.version = 0x0100;
	info->input_dev->event = fts_input_event;
	input_set_drvdata(info->input_dev, info);

	__set_bit(EV_SYN, info->input_dev->evbit);
	__set_bit(EV_KEY, info->input_dev->evbit);
	__set_bit(EV_ABS, info->input_dev->evbit);
	__set_bit(BTN_TOUCH, info->input_dev->keybit);
	__set_bit(BTN_TOOL_FINGER, info->input_dev->keybit);
	/*__set_bit(BTN_TOOL_PEN, info->input_dev->keybit);*/

	input_mt_init_slots(info->input_dev, TOUCH_ID_MAX, INPUT_MT_DIRECT);

	/*input_mt_init_slots(info->input_dev, TOUCH_ID_MAX); */

	input_set_abs_params(info->input_dev, ABS_MT_POSITION_X, X_AXIS_MIN,
		     info->board->x_max, 0, 0);
	input_set_abs_params(info->input_dev, ABS_MT_POSITION_Y, Y_AXIS_MIN,
		     info->board->y_max, 0, 0);
	input_set_abs_params(info->input_dev, ABS_MT_TOUCH_MAJOR, AREA_MIN,
			     AREA_MAX, 0, 0);
	input_set_abs_params(info->input_dev, ABS_MT_TOUCH_MINOR, AREA_MIN,
			     AREA_MAX, 0, 0);
#ifdef CONFIG_INPUT_PRESS_NDT
	input_set_abs_params(info->input_dev, ABS_MT_PRESSURE, PRESSURE_MIN, PRESSURE_MAX, 0, 0);
#endif
	input_set_abs_params(info->input_dev, ABS_MT_DISTANCE, DISTANCE_MIN,
			     DISTANCE_MAX, 0, 0);

#ifdef GESTURE_MODE
	input_set_capability(info->input_dev, EV_KEY, KEY_WAKEUP);

	input_set_capability(info->input_dev, EV_KEY, KEY_M);
	input_set_capability(info->input_dev, EV_KEY, KEY_O);
	input_set_capability(info->input_dev, EV_KEY, KEY_E);
	input_set_capability(info->input_dev, EV_KEY, KEY_W);
	input_set_capability(info->input_dev, EV_KEY, KEY_C);
	input_set_capability(info->input_dev, EV_KEY, KEY_L);
	input_set_capability(info->input_dev, EV_KEY, KEY_F);
	input_set_capability(info->input_dev, EV_KEY, KEY_V);
	input_set_capability(info->input_dev, EV_KEY, KEY_S);
	input_set_capability(info->input_dev, EV_KEY, KEY_Z);
	input_set_capability(info->input_dev, EV_KEY, KEY_WWW);

	input_set_capability(info->input_dev, EV_KEY, KEY_LEFT);
	input_set_capability(info->input_dev, EV_KEY, KEY_RIGHT);
	input_set_capability(info->input_dev, EV_KEY, KEY_UP);
	input_set_capability(info->input_dev, EV_KEY, KEY_DOWN);

	input_set_capability(info->input_dev, EV_KEY, KEY_F1);
	input_set_capability(info->input_dev, EV_KEY, KEY_F2);
	input_set_capability(info->input_dev, EV_KEY, KEY_F3);
	input_set_capability(info->input_dev, EV_KEY, KEY_F4);
	input_set_capability(info->input_dev, EV_KEY, KEY_F5);

	input_set_capability(info->input_dev, EV_KEY, KEY_LEFTBRACE);
	input_set_capability(info->input_dev, EV_KEY, KEY_RIGHTBRACE);
#endif

#ifdef PHONE_KEY
	/*KEY associated to the touch screen buttons */
	input_set_capability(info->input_dev, EV_KEY, KEY_HOMEPAGE);
	input_set_capability(info->input_dev, EV_KEY, KEY_BACK);
	input_set_capability(info->input_dev, EV_KEY, KEY_MENU);
#endif
#ifdef CONFIG_INPUT_PRESS_NDT
	input_set_capability(info->input_dev, EV_KEY, BTN_INFO);
	input_set_capability(info->input_dev, EV_KEY, KEY_GOTO);
#endif
	mutex_init(&(info->input_report_mutex));
#ifdef GESTURE_MODE
	mutex_init(&gestureMask_mutex);
#endif

	spin_lock_init(&fts_int);

	/* register the multi-touch input device */
	error = input_register_device(info->input_dev);
	if (error) {
		logError(1, "%s ERROR: No such input device\n", tag);
		error = -ENODEV;
		goto ProbeErrorExit_5_1;
	}

	skip_5_1 = 1;
	/* track slots */
	info->touch_id = 0;
#ifdef STYLUS_MODE
	info->stylus_id = 0;
#endif

	/* init feature switches (by default all the features are disable, if one feature want to be enabled from the start, set the corresponding value to 1) */
	info->gesture_enabled = 0;
	info->glove_enabled = 0;
	info->charger_enabled = 0;
	info->cover_enabled = 0;
	info->grip_enabled = 0;
	info->grip_pixel_def = 30;
	info->grip_pixel = info->grip_pixel_def;

	info->resume_bit = 1;
	info->lockdown_is_ok = false;
#ifdef CONFIG_DRM
	info->notifier = fts_noti_block;
#endif
	info->bl_notifier = fts_bl_noti_block;
	logError(0, "%s Init Core Lib: \n", tag);
	initCore(info);
	/* init hardware device */
	logError(0, "%s Device Initialization: \n", tag);
	error = fts_init(info);
	if (error < OK) {
		logError(1, "%s Cannot initialize the device ERROR %08X\n", tag,
			 error);
		error = -ENODEV;
		goto ProbeErrorExit_6;
	}

#ifdef CONFIG_SECURE_TOUCH
	logError(1, "%s %s create secure touch file...\n", tag, __func__);
	error = fts_secure_init(info);
	if (error < 0) {
		logError(1, "%s %s init secure touch failed\n", tag, __func__);
		goto ProbeErrorExit_7;
	}
	logError(1, "%s %s create secure touch file successful\n", tag, __func__);
	fts_secure_stop(info, 1);
#endif

#ifdef CONFIG_I2C_BY_DMA
	/*dma buf init*/
	info->dma_buf = (struct fts_dma_buf *)kzalloc(sizeof(*info->dma_buf), GFP_KERNEL);
	if (!info->dma_buf) {
		logError(1, "%s %s:ERROR alloc mem failed!", tag, __func__);
		goto ProbeErrorExit_7;
	}
	mutex_init(&info->dma_buf->dmaBufLock);
	info->dma_buf->rdBuf = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (!info->dma_buf->rdBuf) {
		logError(1, "%s %s:ERROR alloc mem failed!", tag, __func__);
		goto ProbeErrorExit_7;
	}
	info->dma_buf->wrBuf = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (!info->dma_buf->wrBuf) {
		logError(1, "%s %s:ERROR alloc mem failed!", tag, __func__);
		goto ProbeErrorExit_7;
	}
#endif
	fts_info = info;
	error = fts_get_lockdown_info(info->lockdown_info, info);

	if (error < OK)
		logError(1, "%s can't get lockdown info", tag);
	else {
		logError(1,
			 "%s Lockdown:0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x\n",
			 tag, info->lockdown_info[0], info->lockdown_info[1],
			 info->lockdown_info[2], info->lockdown_info[3],
			 info->lockdown_info[4], info->lockdown_info[5],
			 info->lockdown_info[6], info->lockdown_info[7]);
		info->lockdown_is_ok = true;
	}

#ifdef FW_UPDATE_ON_PROBE
	logError(1, "%s FW Update and Sensing Initialization: \n", tag);
	error = fts_fw_update(info, NULL, 0);
	if (error < OK) {
		logError(1,
			 "%s Cannot execute fw upgrade the device ERROR %08X\n",
			 tag, error);
		error = -ENODEV;
		goto ProbeErrorExit_7;
	}
#else
	logError(0, "%s SET Auto Fw Update: \n", tag);
	info->fwu_workqueue =
	    alloc_workqueue("fts-fwu-queue",
			    WQ_UNBOUND | WQ_HIGHPRI | WQ_CPU_INTENSIVE, 1);
	if (!info->fwu_workqueue) {
		logError(1, "%s ERROR: Cannot create fwu work thread\n", tag);
		goto ProbeErrorExit_7;
	}
	INIT_DELAYED_WORK(&info->fwu_work, fts_fw_update_auto);
#endif

	logError(0, "%s SET Device File Nodes: \n", tag);
	/* sysfs stuff */
	info->attrs.attrs = fts_attr_group;
	error = sysfs_create_group(&client->dev.kobj, &info->attrs);
	if (error) {
		logError(1, "%s ERROR: Cannot create sysfs structure!\n", tag);
		error = -ENODEV;
		goto ProbeErrorExit_7;
	}


	error = fts_proc_init();
	if (error < OK)
		logError(1, "%s Error: can not create /proc file! \n", tag);
	info->dbclick_count = 0;

	tp_maker = kzalloc(20, GFP_KERNEL);
	if (tp_maker == NULL)
		logError(1, "%s fail to alloc vendor name memory\n", tag);

	device_init_wakeup(&client->dev, 1);
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE
	memset(&xiaomi_touch_interfaces, 0x00, sizeof(struct xiaomi_touch_interface));
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_GAMEMODE
	INIT_WORK(&info->cmd_update_work, fts_cmd_update_work);
	mutex_init(&info->cmd_update_mutex);
	xiaomi_touch_interfaces.getModeValue = fts_get_mode_value;
	xiaomi_touch_interfaces.setModeValue = fts_set_cur_value;
	xiaomi_touch_interfaces.resetMode = fts_reset_mode;
	xiaomi_touch_interfaces.getModeAll = fts_get_mode_all;
	info->touch_feature_wq =
	    alloc_workqueue("fts-touch-feature",
			    WQ_UNBOUND | WQ_HIGHPRI | WQ_CPU_INTENSIVE, 1);
	if (!info->touch_feature_wq) {
		logError(1, "%s Cannot create touch feature work thread\n", tag);
		goto ProbeErrorExit_8;
	}
	init_waitqueue_head(&info->wait_queue);
	wait_queue_complete = true;

	fts_read_touchmode_data();
	fts_init_touchmode_data();
#endif
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_SENSOR
	xiaomi_touch_interfaces.p_sensor_write = fts_p_sensor_write;
	xiaomi_touch_interfaces.palm_sensor_write = fts_palm_sensor_write;
#endif
	xiaomitouch_register_modedata(&xiaomi_touch_interfaces);
#endif

	info->dev_pm_suspend = false;
	init_completion(&info->dev_pm_suspend_completion);

	if (backlight_register_notifier(&info->bl_notifier) < 0) {
		logError(1, "%s ERROR: register bl_notifier failed!\n", tag);
	}

#ifdef CONFIG_TOUCHSCREEN_ST_DEBUG_FS
	info->debugfs = debugfs_create_dir("tp_debug", NULL);
	if (info->debugfs) {
		debugfs_create_file("switch_state", 0660, info->debugfs, info,
				    &tpdbg_operations);
	}
#endif
	if (info->fts_tp_class == NULL)
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE
		info->fts_tp_class = get_xiaomi_touch_class();
#else
		info->fts_tp_class = class_create(THIS_MODULE, "touch");
#endif
	info->fts_touch_dev =
	    device_create(info->fts_tp_class, NULL, 0x49, info, "tp_dev");

	if (IS_ERR(info->fts_touch_dev)) {
		logError(1,
			 "%s ERROR: Failed to create device for the sysfs!\n",
			 tag);
		goto ProbeErrorExit_8;
	}

	dev_set_drvdata(info->fts_touch_dev, info);

	error =
	    sysfs_create_file(&info->fts_touch_dev->kobj,
			      &dev_attr_fod_status.attr);
	if (error) {
		logError(1, "%s ERROR: Failed to create fod_status sysfs group!\n", tag);
	}
	info->tp_lockdown_info_proc =
	    proc_create("tp_lockdown_info", 0, NULL, &fts_lockdown_info_ops);
	info->tp_selftest_proc =
	    proc_create("tp_selftest", 0644, NULL, &fts_selftest_ops);
	info->tp_data_dump_proc =
	    proc_create("tp_data_dump", 0, NULL, &fts_datadump_ops);
	info->tp_fw_version_proc =
	    proc_create("tp_fw_version", 0, NULL, &fts_fw_version_ops);

#ifndef FW_UPDATE_ON_PROBE
	queue_delayed_work(info->fwu_workqueue, &info->fwu_work,
			   msecs_to_jiffies(EXP_FN_WORK_DELAY_MS));
#endif

	logError(1, "%s Probe Finished! \n", tag);
	return OK;

ProbeErrorExit_8:
    device_destroy(info->fts_tp_class, 0x49);
    class_destroy(info->fts_tp_class);
    info->fts_tp_class = NULL;

ProbeErrorExit_7:
#ifdef CONFIG_SECURE_TOUCH
		fts_secure_remove(info);
#endif
#ifdef CONFIG_I2C_BY_DMA
	if (info->dma_buf)
		kfree(info->dma_buf);
	if (info->dma_buf->rdBuf)
		kfree(info->dma_buf->rdBuf);
	if (info->dma_buf->wrBuf)
		kfree(info->dma_buf->wrBuf);
#endif
#ifdef CONFIG_DRM
	drm_unregister_client(&info->notifier);
#endif

ProbeErrorExit_6:
	input_unregister_device(info->input_dev);

ProbeErrorExit_5_1:
	if (skip_5_1 != 1)
		input_free_device(info->input_dev);

ProbeErrorExit_5:
	destroy_workqueue(info->event_wq);

ProbeErrorExit_4:
	fts_enable_reg(info, false);

ProbeErrorExit_2:
	fts_get_reg(info, false);

ProbeErrorExit_1:
	kfree(info);

ProbeErrorExit_0:
	logError(1, "%s Probe Failed!\n", tag);

	return error;
}

/**
 * Clear and free all the resources associated to the driver.
 * This function is called when the driver need to be removed.
 */
#ifdef I2C_INTERFACE
static int fts_remove(struct i2c_client *client)
{
#else
static int fts_remove(struct spi_device *client)
{
#endif

	struct fts_ts_info *info = dev_get_drvdata(&(client->dev));

	fts_proc_remove();
	/* sysfs stuff */
	sysfs_remove_group(&client->dev.kobj, &info->attrs);
	/* remove interrupt and event handlers */
	fts_interrupt_uninstall(info);
	backlight_unregister_notifier(&info->bl_notifier);
#ifdef CONFIG_DRM
	drm_unregister_client(&info->notifier);
#endif

	/* unregister the device */
	input_unregister_device(info->input_dev);

	/* Remove the work thread */
	destroy_workqueue(info->event_wq);
#ifndef FW_UPDATE_ON_PROBE
	destroy_workqueue(info->fwu_workqueue);
#endif
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_GAMEMODE
	destroy_workqueue(info->touch_feature_wq);
#endif

	fts_enable_reg(info, false);
	fts_get_reg(info, false);
	fts_info = NULL;
#ifdef CONFIG_SECURE_TOUCH
	fts_secure_remove(info);
#endif
	/* free all */
	kfree(info);

	return OK;
}

/**
* Struct which contains the compatible names that need to match with the definition of the device in the device tree node
*/
static struct of_device_id fts_of_match_table[] = {
	{
	 .compatible = "st,fts",
	 },
	{},
};

#ifdef I2C_INTERFACE
static const struct i2c_device_id fts_device_id[] = {
	{FTS_TS_DRV_NAME, 0},
	{}
};

static struct i2c_driver fts_i2c_driver = {
	.driver = {
		   .name = FTS_TS_DRV_NAME,
		   .of_match_table = fts_of_match_table,
#ifdef CONFIG_PM
		   .pm = &fts_dev_pm_ops,
#endif
		   },
	.probe = fts_probe,
	.remove = fts_remove,
	.id_table = fts_device_id,
};
#else
static struct spi_driver fts_spi_driver = {
	.driver = {
		   .name = FTS_TS_DRV_NAME,
		   .of_match_table = fts_of_match_table,
		   .owner = THIS_MODULE,
		   },
	.probe = fts_probe,
	.remove = fts_remove,
};
#endif

static int __init fts_driver_init(void)
{
#ifdef I2C_INTERFACE
	return i2c_add_driver(&fts_i2c_driver);
#else
	return spi_register_driver(&fts_spi_driver);
#endif
}

static void __exit fts_driver_exit(void)
{
#ifdef I2C_INTERFACE
	i2c_del_driver(&fts_i2c_driver);
#else
	spi_unregister_driver(&fts_spi_driver);
#endif

}

MODULE_DESCRIPTION("STMicroelectronics MultiTouch IC Driver");
MODULE_AUTHOR("STMicroelectronics");
MODULE_LICENSE("GPL");

late_initcall(fts_driver_init);
module_exit(fts_driver_exit);
