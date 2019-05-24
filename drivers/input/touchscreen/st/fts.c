// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2016-2019, STMicroelectronics Limited.
 * Authors: AMG(Analog Mems Group)
 *
 *		marco.cali@st.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
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
#include <linux/completion.h>
/*#include <linux/wakelock.h>*/
#include <linux/pm_wakeup.h>

#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>

#if defined(CONFIG_FB_MSM)
#include <linux/notifier.h>
#include <linux/fb.h>
#else
#include <linux/msm_drm_notify.h>
#endif

#ifdef KERNEL_ABOVE_2_6_38
#include <linux/input/mt.h>
#endif

#include <linux/input/touch_event_notify.h>

#include "fts.h"
#include "fts_lib/ftsCompensation.h"
#include "fts_lib/ftsIO.h"
#include "fts_lib/ftsError.h"
#include "fts_lib/ftsFlash.h"
#include "fts_lib/ftsFrame.h"
#include "fts_lib/ftsGesture.h"
#include "fts_lib/ftsTest.h"
#include "fts_lib/ftsTime.h"
#include "fts_lib/ftsTool.h"
#include "linux/moduleparam.h"

#define LINK_KOBJ_NAME "tp"

/*
 * Uncomment to use polling mode instead of interrupt mode.
 *
 */
// #define FTS_USE_POLLING_MODE

/*
 * Event installer helpers
 */
#define event_id(_e) EVENTID_##_e
#define handler_name(_h) fts_##_h##_event_handler

#define install_handler(_i, _evt, _hnd) \
	(_i->event_dispatch_table[event_id(_evt)].handler = handler_name(_hnd))

/*
 * Asyncronouns command helper
 */
#define WAIT_WITH_TIMEOUT(_info, _timeout, _command) \
do { \
	if (wait_for_completion_timeout(&_info->cmd_done, _timeout) == 0) { \
		dev_warn(_info->dev, "Waiting for %s command: timeout\n", \
		#_command); \
	} \
} while (0)

#ifdef KERNEL_ABOVE_2_6_38
#define TYPE_B_PROTOCOL
#endif

#if defined(SCRIPTLESS) || defined(DRIVER_TEST)
static struct class *fts_cmd_class;
#endif

//struct chipInfo ftsInfo;

/**
 * #ifdef PHONE_GESTURE
 * extern struct mutex gestureMask_mutex;
 * #endif
 */

static char tag[8] = "[ FTS ]\0";

static char fts_ts_phys[64];
static u32 typeOfComand[CMD_STR_LEN] = {0};
static int numberParameters;
#ifdef USE_ONE_FILE_NODE
static int feature_feasibility = ERROR_OP_NOT_ALLOW;
#endif
#ifdef PHONE_GESTURE
static u8 mask[GESTURE_MASK_SIZE + 2];
//extern u16 gesture_coordinates_x[GESTURE_COORDS_REPORT_MAX];
//extern u16 gesture_coordinates_y[GESTURE_COORDS_REPORT_MAX];
//extern int gesture_coords_reported;
//extern struct mutex gestureMask_mutex;
#ifdef USE_CUSTOM_GESTURES
static int custom_gesture_res;
#endif
#endif
#ifdef USE_NOISE_PARAM
static u8 noise_params[NOISE_PARAMETERS_SIZE] = {0};
#endif
static void fts_interrupt_enable(struct fts_ts_info *info);
static int fts_init_afterProbe(struct fts_ts_info *info);
static int fts_mode_handler(struct fts_ts_info *info, int force);
static int fts_command(struct fts_ts_info *info, unsigned char cmd);
static int fts_chip_initialization(struct fts_ts_info *info);
static int fts_enable_reg(struct fts_ts_info *info, bool enable);

void touch_callback(unsigned int status)
{
	/* Empty */
}

unsigned int le_to_uint(const unsigned char *ptr)
{
	return (unsigned int) ptr[0] + (unsigned int) ptr[1] * 0x100;
}

unsigned int be_to_uint(const unsigned char *ptr)
{
	return (unsigned int) ptr[1] + (unsigned int) ptr[0] * 0x100;
}

void release_all_touches(struct fts_ts_info *info)
{
	unsigned int type = MT_TOOL_FINGER;
	int i;

	for (i = 0; i < TOUCH_ID_MAX; i++) {
#ifdef STYLUS_MODE
		if (test_bit(i, &info->stylus_id))
			type = MT_TOOL_PEN;
#endif
		input_mt_slot(info->input_dev, i);
		input_mt_report_slot_state(info->input_dev, type, 0);
		input_report_abs(info->input_dev, ABS_MT_TRACKING_ID, -1);
	}
	input_sync(info->input_dev);
	info->touch_id = 0;
#ifdef STYLUS_MODE
	info->stylus_id = 0;
#endif
}

/************************* FW UPGGRADE *********************************/
/* update firmware*/
/**
 * echo 01/00 > fwupdate     perform a fw update taking the FW to burn always
 * from a bin file stored in /system/etc/firmware, 01= force the FW update
 * whicheve fw_version and config_id; 00=perform a fw update only if the fw
 * in the file has a greater fw_version or config_id
 */

/**
 * cat fwupdate to show the result of the burning procedure
 * (example output in the terminal = "AA00000001BB" if the switch is enabled)
 */

/**
 * echo 01/00 > fwupdate; cat fwupdate to perform both operation stated before
 * in just one call
 */
static ssize_t fts_fwupdate_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret, mode;
	/*const struct firmware *fw = NULL;*/
	/*char *firmware_name = "st_fts.bin";*/
	struct Firmware fwD;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);
	int orig_size;
	u8 *orig_data;

	/* reading out firmware upgrade mode */
	ret = kstrtoint(buf, 10, &mode);
	if (ret != 0) {
		pr_err("%s: ret = %d\n", __func__, ret);
		return -EINVAL;
	}

	fwD.data = NULL;
	ret = getFWdata(PATH_FILE_FW, &orig_data, &orig_size, 0);
	if (ret < OK) {
		logError(1, "%s %s: impossible retrieve FW... ERROR %08X\n",
			tag, __func__, ERROR_MEMH_READ);
		ret = (ret | ERROR_MEMH_READ);
		goto END;
	}

	ret = parseBinFile(orig_data, orig_size, &fwD, !mode);
	if (ret < OK) {
		logError(1, "%s %s: impossible parse ERROR %08X\n",
			tag, __func__, ERROR_MEMH_READ);
		ret = (ret | ERROR_MEMH_READ);
		goto END;
	}

	logError(0, "%s Starting flashing procedure...\n", tag);
	ret = flash_burn(fwD, mode, !mode);

	if (ret < OK && ret != (ERROR_FW_NO_UPDATE | ERROR_FLASH_BURN_FAILED))
		logError(0, "%s flashProcedure: ERROR %02X\n",
			tag, ERROR_FLASH_PROCEDURE);
	logError(0, "%s flashing procedure Finished!\n", tag);

END:
	kfree(fwD.data);
	info->fwupdate_stat = ret;

	if (ret < OK)
		logError(1, "%s  %s Unable to upgrade firmware! ERROR %08X\n",
			tag, __func__, ret);

	return count;
}

static ssize_t fts_fwupdate_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	//fwupdate_stat: ERROR code Returned by flashProcedure.
	return snprintf(buf, PAGE_SIZE, "AA%08XBB\n", info->fwupdate_stat);
}

/****UTILITIES (current fw_ver/conf_id, active mode, file fw_ver/conf_id)****/
/**
 * cat appid show on the terminal fw_version.config_id of
 * the FW running in the IC
 */
static ssize_t fts_appid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int error;

	error = snprintf(buf, PAGE_SIZE, "%x.%x\n", ftsInfo.u16_fwVer,
		ftsInfo.u16_cfgId);
	return error;
}

/**
 * cat mode_active to show the bitmask of which indicate the modes/features
 * which are running on the IC in a specific istant oftime (example output in
 * the terminal = "AA10000000BB" only senseOn performed)
 */
static ssize_t fts_mode_active_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	logError(1, "%s Current mode active = %08X\n", tag, info->mode);
	//return sprintf(buf, "AA%08XBB\n", info->mode);
	return snprintf(buf, PAGE_SIZE, "AA%08XBB\n", info->mode);
}

/**
 * cat fw_file_test show on the terminal fw_version and config_id of the FW
 * stored in the fw file/header file
 */
static ssize_t fts_fw_test_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct Firmware fw;
	int ret;

	fw.data = NULL;
	ret = readFwFile(PATH_FILE_FW, &fw, 0);

	if (ret < OK)
		logError(1, "%s Error during reading FW file! ERROR %08X\n",
			tag, ret);
	else {
		logError(1, "%s fw_version = %04X, config_version = %04X, ",
			tag, fw.fw_ver, fw.config_id);
		logError(1, "size = %dbytes\n", fw.data_size);
	}

	kfree(fw.data);
	return 0;
}

/**
 * cat lockdown_info to show the lockdown info on the terminal
 * (example output in the terminal = "AA00000000X1X2..X10BB" )
 * where first 4 bytes correspond t
 */
static ssize_t fts_lockdown_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	u8 data[LOCKDOWN_CODE_SIZE] = {0};
	int ret, size = 100;
	char buff[CMD_STR_LEN] = {0};
	char all_strbuff[100] = {0};

	ret = fts_disableInterrupt();
	if (ret < OK)
		goto END;

	ret = lockDownInfo((u8 *)data, LOCKDOWN_CODE_SIZE);
	if (ret < OK)
		goto END;

END:
	ret |= fts_enableInterrupt();

	snprintf(buff, sizeof(buff), "%02X", 0xAA);
	strlcat(all_strbuff, buff, size);
	snprintf(buff, sizeof(buff), "%08X", ret);
	strlcat(all_strbuff, buff, size);
	if (ret >= OK) {
		for (ret = 0; ret < LOCKDOWN_CODE_SIZE; ret++) {
			snprintf(buff, sizeof(buff), "%02X", data[ret]);
			strlcat(all_strbuff, buff, size);
		}
	} else {
		logError(1, "%s Error while reading lockdown info = %08X\n",
			tag, ret);
	}
	snprintf(buff, sizeof(buff), "%02X", 0xBB);
	strlcat(all_strbuff, buff, size);
	return snprintf(buf, TSP_BUF_SIZE, "%s\n", all_strbuff);
}

/**
 * cat strength_frame	to obtain strength data
 * the string returned in the shell is made up as follow:
 * AA = start byte
 * X1X2X3X4 = 4 bytes in HEX format which represent an
 * error code (00000000 no error)
 *
 * if error code is all 0s
 * FF = 1 byte in HEX format number of rows
 * SS = 1 byte in HEX format number of columns
 * N1, ... = the decimal value of each node separated by a coma
 *
 * BB = end byte
 */
static ssize_t fts_strength_frame_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	char *p = (char *)buf;
	/*unsigned int temp;*/
	/*int res;*/
	/*struct i2c_client *client = to_i2c_client(dev); */
	/*struct fts_ts_info *info = i2c_get_clientdata(client); */

	if (sscanf(p, "%x ", &typeOfComand[0]) != 1)
		return -EINVAL;

	logError(1, "%s %s: Type of Strength Frame selected: %d\n", tag,
		__func__, typeOfComand[0]);
	return count;
}

static ssize_t fts_strength_frame_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct MutualSenseFrame frame;
	int res = ERROR_OP_NOT_ALLOW, j, size = 6*2;
	int count = 0;
	u16 type = 0;
	char *all_strbuff = NULL;
	char buff[CMD_STR_LEN] = {0};
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	frame.node_data = NULL;

	res = fts_disableInterrupt();
	if (res < OK)
		goto END;

	res = senseOn();
#ifdef PHONE_KEY
	res = keyOn();
#endif
	if (res < OK) {
		logError(1, "%s %s: could not start scanning! ERROR %08X\n",
			tag, __func__, res);
		goto END;
	}
	msleep(WAIT_FOR_FRESH_FRAMES);

	res = senseOff();
#ifdef PHONE_KEY
	res = keyOff();
#endif
	if (res < OK) {
		logError(1, "%s %s: could not finish scanning! ERROR %08X\n",
			tag, __func__, res);
		goto END;
	}
	/* mdelay(WAIT_AFTER_SENSEOFF); */
	msleep(WAIT_AFTER_SENSEOFF);
	flushFIFO();

	switch (typeOfComand[0]) {
	case 1:
		type = ADDR_NORM_TOUCH;
		break;
#ifdef PHONE_KEY
	case 2:
		type = ADDR_NORM_MS_KEY;
		break;
#endif
	default:
		logError(1, "%s %s: Strength type %d not valid! ERROR %08X\n",
			tag, __func__, typeOfComand[0], ERROR_OP_NOT_ALLOW);
		res = ERROR_OP_NOT_ALLOW;
		goto END;
	}

	res = getMSFrame(type, &frame, 0);
	if (res < OK) {
		logError(1, "%s %s: could not get the frame! ERROR %08X\n",
			tag, __func__, res);
		goto END;
	} else {
		size += (res * 6);
		logError(0, "%s The frame size is %d words\n", tag, res);
		res = OK;
		print_frame_short("MS Strength frame =",
			array1dTo2d_short(frame.node_data,
				frame.node_data_size,
				frame.header.sense_node),
			frame.header.force_node,
			frame.header.sense_node);
	}

END:
	flushFIFO();
	release_all_touches(info);
	fts_mode_handler(info, 1);

	all_strbuff = kmalloc_array(size, sizeof(char), GFP_KERNEL);

	if (all_strbuff != NULL) {
		memset(all_strbuff, 0, size);
		snprintf(buff, sizeof(buff), "%02X", 0xAA);
		strlcat(all_strbuff, buff, size);

		snprintf(buff, sizeof(buff), "%08X", res);
		strlcat(all_strbuff, buff, size);

		if (res >= OK) {
			snprintf(buff, sizeof(buff), "%02X",
				(u8) frame.header.force_node);
			strlcat(all_strbuff, buff, size);

			snprintf(buff, sizeof(buff), "%02X",
				(u8) frame.header.sense_node);
			strlcat(all_strbuff, buff, size);

			for (j = 0; j < frame.node_data_size; j++) {
				snprintf(buff, sizeof(buff), "%d,",
					frame.node_data[j]);
				strlcat(all_strbuff, buff, size);
			}

			kfree(frame.node_data);
		}

		snprintf(buff, sizeof(buff), "%02X", 0xBB);
		strlcat(all_strbuff, buff, size);

		count = snprintf(buf, TSP_BUF_SIZE, "%s\n", all_strbuff);
		kfree(all_strbuff);
	} else {
		logError(1, "%s %s: Unable toallocate all_strbuff!ERROR %08X\n",
			tag, ERROR_ALLOC);
	}

	fts_enableInterrupt();
	return count;
}

/********** FEATURES *********************/

/**
 * TODO: edit this function according to the features policy to
 * allow during the screen on/off, following is shown an example
 * but check always with ST for more details
 */
int check_feature_feasibility(struct fts_ts_info *info, unsigned int feature)
{
	int res = OK;

	/**
	 * Example based on the status of the screen and
	 * on the feature that is trying to enable
	 */

	/*Example based only on the feature that is going to be activated*/
	switch (feature) {
	case FEAT_GESTURE:
		if (info->cover_enabled == 1) {
			res = ERROR_OP_NOT_ALLOW;

			logError(1, "%s %s:Feature not allowed when in Cover ",
				tag, __func__);
			logError(1, "mode %08X\n", res);
			/**
			 * for example here can be place a code for
			 * disabling the cover mode when gesture is
			 * activated
			 */
		}
		break;

	case FEAT_COVER:
		if (info->gesture_enabled == 1) {
			res = ERROR_OP_NOT_ALLOW;
			/*logError(1,"Feature not allowed*/
			/*when Gestures enabled!");*/
			logError(1, "s %s: Feature not allowed when Gestures ",
				tag, __func__);
			logError(1, "enabled%08X\n", res);
			/**
			 * for example here can be place a code for
			 * disabling the gesture mode when cover is
			 * activated (that means that cover mode has
			 * an higher priority on gesture mode)
			 */
		}
		break;

	default:
		logError(1, "%s %s: Feature Allowed!\n", tag, __func__);
	}
	return res;
}

#ifdef USE_ONE_FILE_NODE
/**
 * echo XXXX 00/01 > feature_enable
 * set the feature to disable/enable.
 * XXXX = 4 bytes which identify the feature
 *
 * cat feature_enable
 * set the enabled mode/features in the IC
 * and return an error code
 *
 * echo XXXX 00/01 > feature_enable;
 * cat feature_enable to perform both action stated
 * before in just one call
 */
static ssize_t fts_feature_enable_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);
	char *p = (char *)buf;
	unsigned int temp;
	int res = OK;

	if ((count - 8 + 1) / 3 != 1) {
		logError(1, "%s fts_feature_enable: ", tag);
		logError(1, "Number of parameter wrong! %d > %d\n",
			(count - 8 + 1) / 3, 1);
		return -EINVAL;
	}

	if (sscanf(p, "%08X ", &temp) != 1)
		return -EINVAL;
	p += 9;
	res = check_feature_feasibility(info, temp);
	if (res < OK)
		return -EINVAL;

	switch (temp) {
#ifdef PHONE_GESTURE
	case FEAT_GESTURE:
		if (sscanf(p, "%02X ", &info->gesture_enabled) != 1)
			return -EINVAL;

		logError(1, "%s fts_feature_enable: Gesture Enabled = %d\n",
			tag, info->gesture_enabled);

		break;
#endif

#ifdef GLOVE_MODE
	case FEAT_GLOVE:
		if (sscanf(p, "%02X ", &info->glove_enabled) != 1)
			return -EINVAL;

		logError(1, "%s fts_feature_enable: Glove Enabled = %d\n",
			tag, info->glove_enabled);

		break;
#endif

#ifdef STYLUS_MODE
	case FEAT_STYLUS:
		if (sscanf(p, "%02X ", &info->stylus_enabled) != 1)
			return -EINVAL;

		logError(1, "%s fts_feature_enable: Stylus Enabled = %d\n",
			tag, info->stylus_enabled);

		break;
#endif

#ifdef COVER_MODE
	case FEAT_COVER:
		if (sscanf(p, "%02X ", &info->cover_enabled) != 1)
			return -EINVAL;

		logError(1, "%s fts_feature_enable: Cover Enabled = %d\n",
			tag, info->cover_enabled);

	break;
#endif

#ifdef CHARGER_MODE
	case FEAT_CHARGER:
		if (sscanf(p, "%02X ", &info->charger_enabled) != 1)
			return -EINVAL;

		logError(1, "%s fts_feature_enable: Charger Enabled= %d\n",
			tag, info->charger_enabled);

		break;
#endif

#ifdef VR_MODE
	case FEAT_VR:
		if (sscanf(p, "%02X ", &info->vr_enabled) != 1)
			return -EINVAL;

		logError(1, "%s fts_feature_enable: VR Enabled = %d\n",
			tag, info->vr_enabled);

		break;
#endif

#ifdef EDGE_REJ
	case FEAT_EDGE_REJECTION:
		if (sscanf(p, "%02X ", &info->edge_rej_enabled) != 1)
			return -EINVAL;
		logError(1, "%s %s: Edge Rejection Enabled= %d\n",
			tag, __func__, info->edge_rej_enabled);

		break;
#endif

#ifdef CORNER_REJ
	case FEAT_CORNER_REJECTION:
		if (sscanf(p, "%02X ", &info->corner_rej_enabled) != 1)
			return -EINVAL;

		logError(1, "%s %s: Corner Rejection Enabled= %d\n",
			tag, __func__, info->corner_rej_enabled);

		break;
#endif

#ifdef EDGE_PALM_REJ
	case FEAT_EDGE_PALM_REJECTION:
		if (sscanf(p, "%02X", &info->edge_palm_rej_enabled) != 1)
			return -EINVAL;

		logError(1, "%s %s:Edge Palm RejectionEnabled= %d\n",
			tag, __func__, info->edge_palm_rej_enabled);

		break;
#endif
	default:
		logError(1, "%s %s: Feature %08X not valid! ERROR %08X\n",
			tag, __func__, temp, ERROR_OP_NOT_ALLOW);
		res = ERROR_OP_NOT_ALLOW;
	}
	feature_feasibility = res;

	if (feature_feasibility >= OK)
		feature_feasibility = fts_mode_handler(info, 1);
	else {
		logError(1, "%s %s: Call echo XXXX 00/01 > feature_enable ",
			tag, __func__);
		logError(1, "with a correct feature! ERROR %08X\n", res);
	}
	return count;
}

static ssize_t fts_feature_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char buff[CMD_STR_LEN] = {0};
	int size = 6 * 2;
	u8 *all_strbuff = NULL;
	int count = 0;

	if (feature_feasibility < OK) {
		logError(1,
			"%s %s:Call before echo 00/01 > feature_enable %08X\n",
			tag, __func__, feature_feasibility);
	}

	all_strbuff = kmalloc(size, GFP_KERNEL);
	if (all_strbuff != NULL) {
		memset(all_strbuff, 0, size);

		snprintf(buff, sizeof(buff), "%02X", 0xAA);
		strlcat(all_strbuff, buff, size);

		snprintf(buff, sizeof(buff), "%08X", feature_feasibility);
		strlcat(all_strbuff, buff, size);

		snprintf(buff, sizeof(buff), "%02X", 0xBB);
		strlcat(all_strbuff, buff, size);

		count = snprintf(buf, TSP_BUF_SIZE, "%s\n", all_strbuff);
		kfree(all_strbuff);
	} else {
		logError(1,
			"%s %s: Unable to allocate all_strbuff! ERROR %08X\n",
			tag, __func__, ERROR_ALLOC);
	}

	feature_feasibility = ERROR_OP_NOT_ALLOW;
	return count;
}

#else

#ifdef EDGE_REJ
/**
 * echo 01/00 > edge_rej    to enable/disable edge rejection
 * cat edge_rej	 to show the status of the edge_rej_enabled
 * switch (example output in the terminal = "AA00000001BB"
 * if the switch is enabled)
 *
 * echo 01/00 > edge_rej; cat edge_rej	to enable/disable
 * edge rejection and see the switch status in just one call
 */
static ssize_t fts_edge_rej_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	char buff[CMD_STR_LEN] = {0};
	int size = 6 * 2;
	u8 *all_strbuff = NULL;
	int count = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	logError(0, "%s %s: edge_rej_enabled = %d\n",
		tag, __func__, info->edge_rej_enabled);

	all_strbuff = kmalloc(size, GFP_KERNEL);
	if (all_strbuff != NULL) {
		memset(all_strbuff, 0, size);

		snprintf(buff, sizeof(buff), "%02X", 0xAA);
		strlcat(all_strbuff, buff, size);

		snprintf(buff, sizeof(buff), "%08X", info->edge_rej_enabled);
		strlcat(all_strbuff, buff, size);

		snprintf(buff, sizeof(buff), "%02X", 0xBB);
		strlcat(all_strbuff, buff, size);

		count = snprintf(buf, TSP_BUF_SIZE, "%s\n", all_strbuff);
		kfree(all_strbuff);
	} else
		logError(1,
			"%s %s: Unable to allocate all_strbuff! ERROR %08X\n",
			tag, __func__, ERROR_ALLOC);

	return count;
}

static ssize_t fts_edge_rej_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	char *p = (char *)buf;
	unsigned int temp;
	int res;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	/**
	 * in case of a different elaboration of the input,
	 * just modify this initial part of the code
	 */
	if ((count + 1) / 3 != 1) {
		logError(1,
			"%s %s:Number bytes of parameter wrong!%d != %d byte\n",
			tag, __func__, (count + 1) / 3, 1);
		return -EINVAL;
	}

	if (sscanf(p, "%02X ", &temp) != 1)
		return -EINVAL;

	p += 3;

	/**
	 * this is a standard code that should be always
	 * used when a feature is enabled!
	 *
	 * first step : check if the wanted feature can be enabled
	 *
	 * second step: call fts_mode_handler to actually enable it
	 * NOTE: Disabling a feature is always allowed by default
	 */
	res = check_feature_feasibility(info, FEAT_EDGE_REJECTION);
	if (res < OK && temp != FEAT_DISABLE)
		return -EINVAL;

	info->edge_rej_enabled = temp;
	res = fts_mode_handler(info, 1);
	if (res < OK) {
		logError(1,
			"%s %s: Error during fts_mode_handler! ERROR %08X\n",
			tag, __func__, res);
		}
	}

	return count;
}
#endif

#ifdef CORNER_REJ
/**
 * echo 01/00 > corner_rej	to enable/disable corner rejection
 * cat corner_rej	to show the status of the
 * corner_rej_enabled switch (example output in the terminal
 * = "AA00000001BB" if the switch is enabled)
 *
 * echo 01/00 > corner_rej; cat corner_rej	to enable/disable
 * corner rejection and see the switch status in just one call
 */
static ssize_t fts_corner_rej_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char buff[CMD_STR_LEN] = {0};
	int size = 6 * 2;
	u8 *all_strbuff = NULL;
	int count = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	logError(0, "%s %s: corner_rej_enabled = %d\n",
		tag, __func__, info->corner_rej_enabled);

	all_strbuff = kmalloc(size, GFP_KERNEL);
	if (all_strbuff != NULL) {
		memset(all_strbuff, 0, size);

		snprintf(buff, sizeof(buff), "%02X", 0xAA);
		strlcat(all_strbuff, buff, size);

		snprintf(buff, sizeof(buff), "%08X", info->corner_rej_enabled);
		strlcat(all_strbuff, buff, size);

		snprintf(buff, sizeof(buff), "%02X", 0xBB);
		strlcat(all_strbuff, buff, size);

		count = snprintf(buf, TSP_BUF_SIZE, "%s\n", all_strbuff);
		kfree(all_strbuff);
	} else {
		logError(1, "%s%s:Unable to allocate all_strbuff! ERROR %08X\n",
			tag, __func__, ERROR_ALLOC);
	}

	return count;
}

static ssize_t fts_corner_rej_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	char *p = (char *)buf;
	unsigned int temp;
	int res;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	/**
	 * in case of a different elaboration of the input,
	 * just modify this initial part of the code according
	 * to customer needs
	 */
	if ((count + 1) / 3 != 1) {
		logError(1,
			"%s %s:Number bytes of parameter wrong!%d != %d byte\n",
			tag, __func__, (count + 1) / 3, 1);
		return -EINVAL;
	}

	/*sscanf(p, "%02X ", &temp);*/
	if (sscanf(p, "%02X ", &temp) != 1)
		return -EINVAL;

	p += 3;

	/**
	 * this is a standard code that should be always
	 * used when a feature is enabled!
	 *
	 * first step : check if the wanted feature
	 * can be enabled
	 *
	 * second step: call fts_mode_handler to
	 * actually enable it
	 *
	 * NOTE: Disabling a feature is always
	 * allowed by default
	 */
	res = check_feature_feasibility(info, FEAT_CORNER_REJECTION);
	if (res >= OK || temp == FEAT_DISABLE) {
		info->corner_rej_enabled = temp;
		res = fts_mode_handler(info, 1);
		if (res < OK) {
			logError(1,
				"%s %s: During fts_mode_handler!ERROR %08X\n",
				tag, __func__, res);
		}
	}

	return count;
}
#endif

#ifdef EDGE_PALM_REJ
/**
 * echo 01/00 > edge_palm_rej
 * to enable/disable edge palm rejection
 *
 * cat edge_palm_rej to show the status of the
 * edge_palm_rej_enabled switch (example output
 * in the terminal = "AA00000001BB" if the switch is enabled)
 *
 * echo 01/00 > edge_palm_rej; cat edge_palm_rej
 * to enable/disable edge palm rejection and see
 * the switch status in just one call
 */
static ssize_t fts_edge_palm_rej_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char buff[CMD_STR_LEN] = {0};
	int size = 6 * 2;
	u8 *all_strbuff = NULL;
	int count = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	logError(0, "%s %s: edge_palm_rej_enabled = %d\n",
		tag, __func__, info->edge_palm_rej_enabled);

	all_strbuff = kmalloc(size, GFP_KERNEL);
	if (all_strbuff != NULL) {
		memset(all_strbuff, 0, size);

		snprintf(buff, sizeof(buff), "%02X", 0xAA);
		strlcat(all_strbuff, buff, size);

		snprintf(buff, sizeof(buff), "%08X",
				info->edge_palm_rej_enabled);
		strlcat(all_strbuff, buff, size);

		snprintf(buff, sizeof(buff), "%02X", 0xBB);
		strlcat(all_strbuff, buff, size);

		count = snprintf(buf, TSP_BUF_SIZE, "%s\n",
				all_strbuff);
		kfree(all_strbuff);
	} else {
		logError(1, "%s%s:Unable to allocate all_strbuff! %08X\n",
			tag, __func__, ERROR_ALLOC);
	}

	return count;
}

static ssize_t fts_edge_palm_rej_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	char *p = (char *)buf;
	unsigned int temp;
	int res;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	/**
	 * in case of a different elaboration of the input,
	 * just modify this initial part of the code according
	 * to customer needs
	 */
	if ((count + 1) / 3 != 1) {
		logError(1,
			"%s%s:Number bytes of parameter wrong! %d != %d byte\n",
			tag, __func__, (count + 1) / 3, 1);
		return -EINVAL;
	}
	/*sscanf(p, "%02X ", &temp);*/
	if (sscanf(p, "%02X ", &temp) != 1)
		return -EINVAL;

	p += 3;

	/**
	 * this is a standard code that should be
	 * always used when a feature is enabled!
	 *
	 * first step : check if the wanted feature can be enabled
	 *
	 * second step: call fts_mode_handler to actually enable it
	 *
	 * NOTE: Disabling a feature is always allowed by default
	 */
	res = check_feature_feasibility(info, FEAT_EDGE_PALM_REJECTION);
	if (res >= OK || temp == FEAT_DISABLE) {
		info->edge_palm_rej_enabled = temp;
		res = fts_mode_handler(info, 1);
		if (res < OK) {
			logError(1, "%s%s:Error in fts_mode_handler!%08X\n",
				tag, __func__, res);
		}
	}

	return count;
}
#endif

#ifdef CHARGER_MODE
/**
 * echo 01/00 > charger_mode    to enable/disable charger mode
 *
 * cat charger_mode	 to show the status of
 * the charger_enabled switch (example output in the terminal
 * = "AA00000001BB" if the switch is enabled)
 *
 * echo 01/00 > charger_mode; cat charger_mode
 * to enable/disable charger mode and see the
 * switch status in just one call
 */
static ssize_t fts_charger_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char buff[CMD_STR_LEN] = {0};
	int size = 6 * 2;
	u8 *all_strbuff = NULL;
	int count = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	logError(0, "%s %s:charger_enabled = %d\n",
		tag, __func__, info->charger_enabled);

	all_strbuff = kmalloc(size, GFP_KERNEL);
	if (all_strbuff != NULL) {
		memset(all_strbuff, 0, size);

		snprintf(buff, sizeof(buff), "%02X", 0xAA);
		strlcat(all_strbuff, buff, size);

		snprintf(buff, sizeof(buff), "%08X", info->charger_enabled);
		strlcat(all_strbuff, buff, size);

		snprintf(buff, sizeof(buff), "%02X", 0xBB);
		strlcat(all_strbuff, buff, size);

		count = snprintf(buf, TSP_BUF_SIZE, "%s\n", all_strbuff);
		kfree(all_strbuff);
	} else {
		logError(1, "%s %s:Unable to allocate all_strbuff! %08X\n",
			tag, __func__, ERROR_ALLOC);
	}

	return count;
}

static ssize_t fts_charger_mode_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	char *p = (char *)buf;
	unsigned int temp;
	int res;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	/**
	 * in case of a different elaboration of the input,
	 * just modify this initial part of the code
	 * according to customer needs
	 */
	if ((count + 1) / 3 != 1) {
		logError(1, "%s %s:Size of parameter wrong! %d != %d byte\n",
			tag, __func__, (count + 1) / 3, 1);
		return -EINVAL;
	}
	/*sscanf(p, "%02X ", &temp);*/
	if (sscanf(p, "%02X ", &temp) != 1)
		return -EINVAL;

	p += 3;

	/**
	 * this is a standard code that should be always
	 * used when a feature is enabled!
	 *
	 * first step : check if the wanted feature
	 * can be enabled
	 * second step: call fts_mode_handler to
	 * actually enable it
	 *
	 * NOTE: Disabling a feature is always
	 * allowed by default
	 */
	res = check_feature_feasibility(info, FEAT_CHARGER);
	if (res >= OK || temp == FEAT_DISABLE) {
		info->charger_enabled = temp;
		res = fts_mode_handler(info, 1);
		if (res < OK) {
			logError(1, "%s %s: Error during fts_mode_handler! ",
				tag, __func__);
			logError(1, "ERROR %08X\n", res);
		}
	}

	return count;
}
#endif

#ifdef GLOVE_MODE
/**
 * echo 01/00 > glove_mode
 * to enable/disable glove mode
 *
 * cat glove_mode	to show the status of
 * the glove_enabled switch (example output in the
 * terminal = "AA00000001BB" if the switch is enabled)
 *
 * echo 01/00 > glove_mode; cat glove_mode
 * to enable/disable glove mode and see the
 * switch status in just one call
 */
static ssize_t fts_glove_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char buff[CMD_STR_LEN] = {0};
	int size = 6 * 2;
	u8 *all_strbuff = NULL;
	int count = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	logError(0, "%s %s:glove_enabled = %d\n",
		tag, __func__, info->glove_enabled);

	all_strbuff = kmalloc(size, GFP_KERNEL);
	if (all_strbuff != NULL) {
		memset(all_strbuff, 0, size);

		snprintf(buff, sizeof(buff), "%02X", 0xAA);
		strlcat(all_strbuff, buff, size);

		snprintf(buff, sizeof(buff), "%08X", info->glove_enabled);
		strlcat(all_strbuff, buff, size);

		snprintf(buff, sizeof(buff), "%02X", 0xBB);
		strlcat(all_strbuff, buff, size);

		count = snprintf(buf, TSP_BUF_SIZE, "%s\n", all_strbuff);
		kfree(all_strbuff);
	} else {
		logError(1, "%s %s:Unable to allocate all_strbuff! %08X\n",
			tag, __func__, ERROR_ALLOC);
	}

	return count;
}

static ssize_t fts_glove_mode_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	char *p = (char *)buf;
	unsigned int temp;
	int res;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	/**
	 * in case of a different elaboration of the input,
	 * just modify this initial part of the code
	 * according to customer needs
	 */
	if ((count + 1) / 3 != 1) {
		logError(1, "%s %s:Size of parameter wrong! %d != %d byte\n",
			tag, __func__, (count + 1) / 3, 1);
		return -EINVAL;
	}

	if (sscanf(p, "%02X ", &temp) != 1)
		return -EINVAL;

	p += 3;

	/**
	 * this is a standard code that should be
	 * always used when a feature is enabled!
	 *
	 * first step : check if the wanted feature can be enabled
	 *
	 * second step: call fts_mode_handler to actually enable it
	 *
	 * NOTE: Disabling a feature is always allowed by default
	 */
	res = check_feature_feasibility(info, FEAT_GLOVE);
	if (res >= OK || temp == FEAT_DISABLE) {
		info->glove_enabled = temp;
		res = fts_mode_handler(info, 1);
		if (res < OK) {
			logError(1, "%s %s: Error during fts_mode_handler! ",
				tag, __func__);
			logError(1, "ERROR %08X\n", res);
		}
	}

	return count;
}
#endif

#ifdef VR_MODE
/**
 * echo 01/00 > vr_mode   to enable/disable vr mode
 *
 * cat vr_mode		 to show the status of
 * the vr_enabled switch (example output in the
 * terminal = "AA00000001BB" if the switch is enabled)
 *
 * echo 01/00 > vr_mode; cat vr_mode  to enable/disable
 * vr mode and see the switch status in just one call
 */
static ssize_t fts_vr_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char buff[CMD_STR_LEN] = {0};
	int size = 6 * 2;
	u8 *all_strbuff = NULL;
	int count = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	logError(0, "%s %s: vr_enabled = %d\n",
		tag, __func__, info->vr_enabled);

	all_strbuff = kmalloc(size, GFP_KERNEL);
	if (all_strbuff != NULL) {
		memset(all_strbuff, 0, size);

		snprintf(buff, sizeof(buff), "%02X", 0xAA);
		strlcat(all_strbuff, buff, size);

		snprintf(buff, sizeof(buff), "%08X", info->vr_enabled);
		strlcat(all_strbuff, buff, size);

		snprintf(buff, sizeof(buff), "%02X", 0xBB);
		strlcat(all_strbuff, buff, size);

		count = snprintf(buf, TSP_BUF_SIZE, "%s\n", all_strbuff);
		kfree(all_strbuff);
	} else {
		logError(1,
			"%s %s: Unable to allocate all_strbuff! ERROR %08X\n",
			tag, __func__, ERROR_ALLOC);
	}

	return count;
}

static ssize_t fts_vr_mode_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	char *p = (char *)buf;
	unsigned int temp;
	int res;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	/**
	 * in case of a different elaboration of the input,
	 * just modify this initial part of the code
	 * according to customer needs
	 */
	if ((count + 1) / 3 != 1) {
		logError(1,
			"%s %s:Number bytes of parameter wrong!%d != %d byte\n",
			tag, __func__, (count + 1) / 3, 1);
		return -EINVAL;
	}

	if (sscanf(p, "%02X ", &temp) != 1)
		return -EINVAL;

	p += 3;

	/**
	 * this is a standard code that should be always
	 * used when a feature is enabled!
	 *
	 * first step : check if the wanted feature can be enabled
	 * second step: call fts_mode_handler to actually enable it
	 *
	 * NOTE: Disabling a feature is always allowed by default
	 */
	res = check_feature_feasibility(info, FEAT_VR);
	if (res >= OK || temp == FEAT_DISABLE) {
		info->vr_enabled = temp;
		res = fts_mode_handler(info, 1);
		if (res < OK) {
			logError(1, "%s %s: Error in fts_mode_handler!%08X\n",
				tag, __func__, res);
		}
	}

	return count;
}
#endif

#ifdef COVER_MODE
/**
 * echo 01/00 > cover_mode	to enable/disable cover mode
 * cat cover_mode  to show the status of the
 * cover_enabled switch (example output in the
 * terminal = "AA00000001BB" if the switch is enabled)
 *
 * echo 01/00 > cover_mode; cat cover_mode	to
 * enable/disable cover mode and see the switch
 * status in just one call
 *
 * NOTE: the cover can be handled also using a notifier,
 * in this case the body of these functions
 * should be copied in the notifier callback
 */
static ssize_t fts_cover_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char buff[CMD_STR_LEN] = {0};
	int size = 6 * 2;
	u8 *all_strbuff = NULL;
	int count = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	logError(0, "%s %s: cover_enabled = %d\n",
		tag, __func__, info->cover_enabled);

	all_strbuff = kmalloc(size, GFP_KERNEL);
	if (all_strbuff != NULL) {
		memset(all_strbuff, 0, size);

		snprintf(buff, sizeof(buff), "%02X", 0xAA);
		strlcat(all_strbuff, buff, size);

		snprintf(buff, sizeof(buff), "%08X", info->cover_enabled);
		strlcat(all_strbuff, buff, size);

		snprintf(buff, sizeof(buff), "%02X", 0xBB);
		strlcat(all_strbuff, buff, size);

		count = snprintf(buf, TSP_BUF_SIZE, "%s\n", all_strbuff);
		kfree(all_strbuff);
	} else {
		logError(1,
			"%s %s:Unable to allocate all_strbuff! ERROR %08X\n",
			tag, __func__, ERROR_ALLOC);
	}

	return count;
}

static ssize_t fts_cover_mode_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	char *p = (char *)buf;
	unsigned int temp;
	int res;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);


	/**
	 * in case of a different elaboration of the input,
	 * just modify this initial part of the code according
	 * to customer needs
	 */
	if ((count + 1) / 3 != 1) {
		logError(1,
			"%s %s:Number bytes of parameter wrong!%d != %d byte\n",
			tag, __func__, (count + 1) / 3, 1);
			return -EINVAL;

	}

	if (sscanf(p, "%02X ", &temp) != 1)
		return -EINVAL;
	p += 3;

	/**
	 * this is a standard code that should be
	 * always used when a feature is enabled!
	 *
	 * first step : check if the wanted feature can be enabled
	 * second step: call fts_mode_handler to actually enable it
	 * NOTE: Disabling a feature is always allowed by default
	 */
	res = check_feature_feasibility(info, FEAT_COVER);
	if (res >= OK || temp == FEAT_DISABLE) {
		info->cover_enabled = temp;
		res = fts_mode_handler(info, 1);
		if (res < OK) {
			logError(1, "%s%s:Error in fts_mode_handler!%08X\n",
				tag, __func__, res);
		}
	}


	return count;
}
#endif

#ifdef STYLUS_MODE
/**
 * echo 01/00 > stylus_mode   to enable/disable stylus mode
 * cat stylus_mode		    to show the status of
 * the stylus_enabled switch (example output in the
 * terminal = "AA00000001BB" if the switch is enabled)
 *
 * echo 01/00 > stylus_mode; cat stylus_mode  to
 * enable/disable stylus mode and see the
 * switch status in just one call
 */
static ssize_t fts_stylus_mode_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char buff[CMD_STR_LEN] = {0};
	int size = 6 * 2;
	u8 *all_strbuff = NULL;
	int count = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	logError(0, "%s %s: stylus_enabled = %d\n",
		tag, __func__, info->stylus_enabled);

	all_strbuff = kmalloc(size, GFP_KERNEL);
	if (all_strbuff != NULL) {
		memset(all_strbuff, 0, size);

		snprintf(buff, sizeof(buff), "%02X", 0xAA);
		strlcat(all_strbuff, buff, size);

		snprintf(buff, sizeof(buff), "%08X", info->stylus_enabled);
		strlcat(all_strbuff, buff, size);

		snprintf(buff, sizeof(buff), "%02X", 0xBB);
		strlcat(all_strbuff, buff, size);

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
	struct device_attribute *attr, const char *buf, size_t count)
{
	char *p = (char *)buf;
	unsigned int temp;
	int res;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	/**
	 * in case of a different elaboration of the input,
	 * just modify this initial part of the code
	 * according to customer needs
	 */
	if ((count + 1) / 3 != 1) {
		logError(1, "%s %s:Size of parameter wrong! %d != %d byte\n",
			tag, __func__, (count + 1) / 3, 1);
		return -EINVAL;
	}

	if (sscanf(p, "%02X ", &temp) != 1)
		return -EINVAL;
	p += 3;

	/**
	 * this is a standard code that should be
	 * always used when a feature is enabled!
	 *
	 * first step : check if the wanted feature can be enabled
	 * second step: call fts_mode_handler to actually enable it
	 * NOTE: Disabling a feature is always allowed by default
	 */
	res = check_feature_feasibility(info, FEAT_STYLUS);
	if (res >= OK || temp == FEAT_DISABLE) {
		info->stylus_enabled = temp;
		res = fts_mode_handler(info, 1);
		if (res < OK) {
			logError(1,
				"%s %s:Error during fts_mode_handler! %08X\n",
				tag, __func__, res);
		}
	}

	return count;
}
#endif

#endif

/************** GESTURES *************/
#ifdef PHONE_GESTURE
#ifdef USE_GESTURE_MASK

/**
 * if this define is used, a gesture bit mask
 * is used as method to select the gestures
 * to enable/disable
 */

/**
 * echo EE X1 X2 ... X8 > gesture_mask   set
 * the gesture mask to disable/enable;
 * EE = 00(disable) or 01(enable);
 * X1 ... X8 = gesture mask (example 06 00 ... 00
 * this gesture mask represent the gestures with ID = 1 and 2)
 * can be specified from 1 to 8 bytes, if less than 8 bytes
 * are specified the remaining bytes are kept as previous settings
 *
 * cat gesture_mask		    enable/disable the given mask,
 * if one or more gestures is enabled the driver will
 * automatically enable the gesture mode.
 * If all the gestures are disabled the driver
 * automatically will disable the gesture mode.
 * At the end an error code will be printed
 * (example output in the terminal = "AA00000000BB"
 * if there are no errors)
 *
 * echo EE X1 X2 ... X8 > gesture_mask;
 * cat gesture_mask        perform in one
 * command both actions stated before
 */
static ssize_t fts_gesture_mask_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	char buff[CMD_STR_LEN] = {0};
	int size = 6 * 2;
	u8 *all_strbuff = NULL;
	int count = 0, res, temp;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	if (mask[0] == 0) {
		res = ERROR_OP_NOT_ALLOW;
		logError(1, "%s %s:Call before echo enable/disable xx xx >",
			tag), __func__;
		logError(1, "%s %s: gesture_mask with a correct number of ",
			tag, __func__);
		logError(1, "parameters! ERROR %08X\n", res);
		return -EINVAL;
	}

	if (mask[1] == FEAT_ENABLE || mask[1] == FEAT_DISABLE)
		res = updateGestureMask(&mask[2], mask[0], mask[1]);
	else
		res = ERROR_OP_NOT_ALLOW;

	if (res < OK) {
		logError(1, "%s fts_gesture_mask_store: ERROR %08X\n",
			tag, res);
	}

	res |= check_feature_feasibility(info, FEAT_GESTURE);
	temp = isAnyGestureActive();
	if (res >= OK || temp == FEAT_DISABLE)
		info->gesture_enabled = temp;

	logError(1, "%s fts_gesture_mask_store:Gesture Enabled = %d\n",
		tag, info->gesture_enabled);

	all_strbuff = kmalloc(size, GFP_KERNEL);
	if (all_strbuff != NULL) {
		memset(all_strbuff, 0, size);

		snprintf(buff, sizeof(buff), "%02X", 0xAA);
		strlcat(all_strbuff, buff, size);

		snprintf(buff, sizeof(buff), "%08X", res);
		strlcat(all_strbuff, buff, size);

		snprintf(buff, sizeof(buff), "%02X", 0xBB);
		strlcat(all_strbuff, buff, size);

		count = snprintf(buf, TSP_BUF_SIZE, "%s\n", all_strbuff);
		kfree(all_strbuff);
	} else {
		logError(1,
			"%s %s:Unable to allocate all_strbuff! ERROR %08X\n",
			tag, __func__, ERROR_ALLOC);
	}

	mask[0] = 0;
	return count;
}


static ssize_t fts_gesture_mask_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	char *p = (char *)buf;
	int n;
	unsigned int temp;

	if ((count + 1) / 3 > GESTURE_MASK_SIZE + 1) {
		logError(1, "%s %s: Number of bytes of parameter wrong! ", tag,
			__func__);
		logError(1, "%d > (enable/disable + %d )\n", (count + 1) / 3,
			GESTURE_MASK_SIZE);
		mask[0] = 0;
		return -EINVAL;
	}
	mask[0] = ((count + 1) / 3) - 1;
	for (n = 1; n <= (count + 1) / 3; n++) {
		if (sscanf(p, "%02X ", &temp) != 1)
			return -EINVAL;
		p += 3;
		mask[n] = (u8)temp;
		logError(1, "%s mask[%d] = %02X\n", tag, n, mask[n]);
	}

	return count;
}

#else

/**
 * if this define is not used,
 * to select the gestures to enable/disable
 * are used the IDs of the gestures
 *
 * echo EE X1 X2 ... > gesture_mask    set
 * the gesture to disable/enable; EE = 00(disable)
 * or 01(enable); X1 ... = gesture IDs
 * (example 01 02 05... represent the gestures with
 * ID = 1, 2 and 5) there is no limit of the parameters
 * that can be passed, but of course the gesture IDs
 * should be valid (all the valid IDs are listed
 * in ftsGesture.h)
 *
 * cat gesture_mask        enable/disable the
 * given gestures, if one or more gestures is enabled
 * the driver will automatically enable the gesture mode.
 * If all the gestures are disabled the driver automatically
 * will disable the gesture mode. At the end an error code
 * will be printed (example output in the terminal =
 * "AA00000000BB" if there are no errors)
 *
 * echo EE X1 X2 ... > gesture_mask; cat gesture_mask
 * perform in one command both actions stated before
 */
static ssize_t fts_gesture_mask_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char buff[CMD_STR_LEN] = {0};
	int size = 6 * 2;
	u8 *all_strbuff = NULL;
	int count = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	logError(0, "%s %s: gesture_enabled = %d\n", tag, __func__,
		info->gesture_enabled);

	all_strbuff = kmalloc(size, GFP_KERNEL);
	if (all_strbuff != NULL) {
		memset(all_strbuff, 0, size);

		snprintf(buff, sizeof(buff), "%02X", 0xAA);
		strlcat(all_strbuff, buff, size);

		snprintf(buff, sizeof(buff), "%08X", info->gesture_enabled);
		strlcat(all_strbuff, buff, size);

		snprintf(buff, sizeof(buff), "%02X", 0xBB);
		strlcat(all_strbuff, buff, size);

		count = snprintf(buf, TSP_BUF_SIZE, "%s\n", all_strbuff);
		kfree(all_strbuff);
	} else {
		logError(1,
			"%s %s: Unable to allocate all_strbuff! ERROR %08X\n",
			tag, __func__, ERROR_ALLOC);
	}

	return count;
}

static ssize_t fts_gesture_mask_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	char *p = (char *)buf;
	int n;
	unsigned int temp;
	int res;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	if ((count + 1) / 3 < 2 || (count + 1) / 3 > GESTURE_MASK_SIZE + 1) {
		logError(1,
			"%s %s:Number bytes of parameter wrong! %d %d bytes)\n",
			tag, __func__, (count + 1) / 3, GESTURE_MASK_SIZE);
		mask[0] = 0;
		return -EINVAL;
	}

	memset(mask, 0, GESTURE_MASK_SIZE + 2);
	mask[0] = ((count + 1) / 3) - 1;
	if (sscanf(p, "%02X ", &temp) != 1)
		return -EINVAL;

	p += 3;
	mask[1] = (u8)temp;
	for (n = 1; n < (count + 1) / 3; n++) {
		/*sscanf(p, "%02X ", &temp);*/
		if (sscanf(p, "%02X ", &temp) != 1)
			return -EINVAL;

		p += 3;
		gestureIDtoGestureMask((u8)temp, &mask[2]);
	}

	for (n = 0; n < GESTURE_MASK_SIZE + 2; n++)
		logError(1, "%s mask[%d] = %02X\n", tag, n, mask[n]);

	if (mask[0] == 0) {
		res = ERROR_OP_NOT_ALLOW;
		logError(1, "%s %s: Call before echo enable/disable xx xx ....",
			tag, __func__);
		logError(1, " > gesture_mask with parameters! ERROR %08X\n",
			res);

	} else {

		if (mask[1] == FEAT_ENABLE || mask[1] == FEAT_DISABLE)
			res = updateGestureMask(&mask[2], mask[0], mask[1]);
		else
			res = ERROR_OP_NOT_ALLOW;

		if (res < OK)
			logError(1, "%s %s: ERROR %08X\n", tag, __func__, res);

	}

	res = check_feature_feasibility(info, FEAT_GESTURE);
	temp = isAnyGestureActive();
	if (res >= OK || temp == FEAT_DISABLE)
		info->gesture_enabled = temp;
	res = fts_mode_handler(info, 0);

	return count;
}
#endif

#ifdef USE_CUSTOM_GESTURES
/**
 * allow to use user defined gestures
 *
 * echo ID X1 Y1 X2 Y2 ... X30 Y30 >
 * add_custom_gesture     add a custom gesture;
 * ID = 1 byte that represent the gesture ID of
 * the custom gesture (can be chosen only between
 * the custom IDs defined in ftsGesture.h);
 * X1 Y1 ... = a series of 30 points (x,y) which
 * represent the gesture template.
 * The loaded gesture is enabled automatically
 *
 * cat add_custom_gesture/remove_custom_gesture
 * Print the error code of the last operation
 * performed with the custom gestures
 * (example output in the terminal = "AA00000000BB"
 * if there are no errors)
 *
 * echo ID X1 Y1 X2 Y2 ... X30 Y30 >
 * add_custom_gesture; cat add_custom_gesture
 * perform in one command both actions stated before
 */
static ssize_t fts_add_custom_gesture_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char buff[CMD_STR_LEN] = {0};
	int size = 6 * 2;
	u8 *all_strbuff = NULL;
	int count = 0;

	logError(0, "%s %s:Last Operation Result = %08X\n",
		tag, __func__, custom_gesture_res);

	all_strbuff = kmalloc(size, GFP_KERNEL);
	if (all_strbuff != NULL) {
		memset(all_strbuff, 0, size);

		snprintf(buff, sizeof(buff), "%02X", 0xAA);
		strlcat(all_strbuff, buff, size);

		snprintf(buff, sizeof(buff), "%08X", custom_gesture_res);
		strlcat(all_strbuff, buff, size);

		snprintf(buff, sizeof(buff), "%02X", 0xBB);
		strlcat(all_strbuff, buff, size);

		count = snprintf(buf, TSP_BUF_SIZE, "%s\n", all_strbuff);
		kfree(all_strbuff);
	} else {
		logError(1,
			"%s %s:Unable to allocate all_strbuff! ERROR %08X\n",
			tag, __func__, ERROR_ALLOC);
	}

	return count;
}

static ssize_t fts_add_custom_gesture_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	char *p = (char *)buf;
	int n;
	unsigned int temp;
	u8 gestureID;
	u8 gestMask[GESTURE_MASK_SIZE] = {0};
	u8 template[GESTURE_CUSTOM_POINTS];
	int res;
	/*struct i2c_client *client = to_i2c_client(dev);*/
	/*struct fts_ts_info *info = i2c_get_clientdata(client);*/

	if ((count + 1) / 3 != GESTURE_CUSTOM_POINTS + 1) {
		logError(1,
			"%s %s: Number bytes of parameter wrong! %d != %d\n",
			tag, __func__,  (count + 1) / 3,
			GESTURE_CUSTOM_POINTS + 1);
		res = ERROR_OP_NOT_ALLOW;
		return -EINVAL;
	}
	if (sscanf(p, "%02X ", &temp) != 1)
		return -EINVAL;
	p += 3;
	gestureID = (u8)temp;

	for (n = 1; n < (count + 1) / 3; n++) {
		/*sscanf(p, "%02X ", &temp);*/
		if (sscanf(p, "%02X ", &temp) != 1)
			return -EINVAL;

		p += 3;
		template[n-1] = (u8)temp;
		logError(1, "%s template[%d] = %02X\n",
			tag, n-1, template[n-1]);
	}

	res = fts_disableInterrupt();
	if (res >= OK) {
		logError(1, "%s %s: Adding custom gesture ID = %02X\n",
			tag, __func__, gestureID);
		res = addCustomGesture(template,
			GESTURE_CUSTOM_POINTS, gestureID);
		if (res < OK) {
			logError(1,
				"%s %s:error during add custom gesture ",
				tag, __func__);
			logError(1, "ERROR %08X\n", res);
		} else {
			logError(1,
				"%s %s:Enabling in the gesture mask...\n",
				tag, __func__);
			gestureIDtoGestureMask(gestureID, gestMask);
			res = enableGesture(gestMask, GESTURE_MASK_SIZE);
			if (res < OK) {
				logError(1, "%s %s:error during enable gesture",
					tag, __func__);
				logError(1, " mask: ERROR %08X\n", res);
			} else {
				/*if(check_feature_feasibility(info,*/
				/*FEAT_GESTURE)==OK)*/
				/*info->gesture_enabled =*/
				/*isAnyGestureActive();*/
				/*uncomment if you want to activate*/
				/* automatically*/
				/*the gesture mode when a custom gesture*/
				/*is loaded*/
				logError(1, "%s %s:Custom Gesture enabled!\n",
					tag, __func__, res);
			}
		}
	}
	res |= fts_enableInterrupt();

	custom_gesture_res = res;

	return count;
}


/**
 * echo ID > remove_custom_gesture
 * remove a custom gesture;
 * ID = 1 byte that represent the gesture ID
 * of the custom gesture (can be chosen only
 * between the custom IDs defined in ftsGesture.h);
 * the same gesture is disabled automatically
 */
static ssize_t fts_remove_custom_gesture_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char buff[CMD_STR_LEN] = {0};
	int size = 6 * 2;
	u8 *all_strbuff = NULL;
	int count = 0;

	logError(0, "%s %s:Last Operation Result = %08X\n",
		tag, __func__, custom_gesture_res);

	all_strbuff = kmalloc(size, GFP_KERNEL);
	if (all_strbuff != NULL) {
		memset(all_strbuff, 0, size);

		snprintf(buff, sizeof(buff), "%02X", 0xAA);
		strlcat(all_strbuff, buff, size);

		snprintf(buff, sizeof(buff), "%08X", custom_gesture_res);
		strlcat(all_strbuff, buff, size);

		snprintf(buff, sizeof(buff), "%02X", 0xBB);
		strlcat(all_strbuff, buff, size);

		count = snprintf(buf, TSP_BUF_SIZE, "%s\n", all_strbuff);
		kfree(all_strbuff);
	} else {
		logError(1,
			"%s %s:Unable to allocate all_strbuff! ERROR %08X\n",
			tag, __func__, ERROR_ALLOC);
	}

	return count;
}

static ssize_t fts_remove_custom_gesture_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	char *p = (char *)buf;
	unsigned int temp;
	int res;
	u8 gestureID;
	u8 gestMask[GESTURE_MASK_SIZE] = {0};
	/*struct i2c_client *client = to_i2c_client(dev);*/
	/*struct fts_ts_info *info = i2c_get_clientdata(client);*/

	if ((count + 1) / 3 < 1) {
		logError(1,
			"%s %s:Number bytes of parameter wrong! %d != %d\n",
			tag, __func__, (count + 1) / 3, 1);
		res = ERROR_OP_NOT_ALLOW;
		return -EINVAL;
	}

	if (sscanf(p, "%02X ", &temp) != 1)
		return -EINVAL;
	p += 3;
	gestureID = (u8)temp;
	res = fts_disableInterrupt();
	if (res >= OK) {
		logError(1,
			"%s %s: Removing custom gesture ID = %02X\n",
			tag, __func__, gestureID);
		res = removeCustomGesture(gestureID);
		if (res < OK) {
			logError(1,
				"%s %s:error in custom gesture:%08X\n",
				tag, __func__, res);
		} else {
			logError(1, "%s %s: Enabling in the gesture mask...\n",
				tag, __func__);
			gestureIDtoGestureMask(gestureID, gestMask);
			res = disableGesture(gestMask, GESTURE_MASK_SIZE);
			if (res < OK) {
				logError(1,
				    "%s %s:error in enable gesture mask:%08X\n",
				    tag, __func__, res);
			} else {
				/*if(check_feature_feasibility*/
				/*(info,FEAT_GESTURE)==OK)*/
				/*info->gesture_enabled = */
				/*isAnyGestureActive();*/
				/**
				 * uncomment if you want to disable
				 * automatically
				 * the gesture mode when a custom gesture is
				 * removed and no other gestures were enabled
				 */
				logError(1, "%s %s: Custom Gesture disabled!\n",
					tag, __func__, res);
			}

		}
	}

	res |= fts_enableInterrupt();

	custom_gesture_res = res;
	return count;
}
#endif


/**
 * cat gesture_coordinates	to obtain the gesture coordinates
 * the string returned in the shell follow this up as follow:
 * AA = start byte
 * X1X2X3X4 = 4 bytes in HEX format
 * which represent an error code (00000000 no error)
 */
 /**** if error code is all 0s ****/
/**
 * CC = 1 byte in HEX format number of coords
 * (pair of x,y) returned
 *
 * X1X2 Y1Y2 ... = X1X2 2 bytes in HEX format for
 * x[i] and Y1Y2 2 bytes in HEX format for y[i] (MSB first)
 */
/********************************/
/* BB = end byte*/
static ssize_t fts_gesture_coordinates_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char buff[CMD_STR_LEN] = {0};
	int size = 6 * 2;
	//u8 coords_num;
	u8 *all_strbuff = NULL;
	int count = 0, res, i = 0;

	logError(0, "%s %s: Getting gestures coordinates...\n", tag, __func__);

	if (gesture_coords_reported < OK) {
		logError(1, "%s %s:invalid coordinates! ERROR %08X\n",
			tag, __func__, gesture_coords_reported);
		res = gesture_coords_reported;
	} else {
		/*coords are pairs of x,y (*2) where each coord*/
		/*is a short(2bytes=4char)(*4) + 1 byte(2char) num*/
		/*of coords (+2)*/
		size += gesture_coords_reported * 2 * 4 + 2;
		/*coords_num = res;*/
		res = OK;
		/*set error code to OK*/
	}

	all_strbuff = kmalloc(size, GFP_KERNEL);
	if (all_strbuff != NULL) {
		memset(all_strbuff, 0, size);

		snprintf(buff, sizeof(buff), "%02X", 0xAA);
		strlcat(all_strbuff, buff, size);

		snprintf(buff, sizeof(buff), "%08X", res);
		strlcat(all_strbuff, buff, size);

		if (res >= OK) {
			snprintf(buff, sizeof(buff), "%02X",
				gesture_coords_reported);
			strlcat(all_strbuff, buff, size);

			for (i = 0; i < gesture_coords_reported; i++) {
				snprintf(buff, sizeof(buff), "%04X",
					gesture_coordinates_x[i]);
				strlcat(all_strbuff, buff, size);

				snprintf(buff, sizeof(buff), "%04X",
					gesture_coordinates_y[i]);
				strlcat(all_strbuff, buff, size);
			}
		}

		snprintf(buff, sizeof(buff), "%02X", 0xBB);
		strlcat(all_strbuff, buff, size);

		count = snprintf(buf, TSP_BUF_SIZE, "%s\n", all_strbuff);
		kfree(all_strbuff);
	} else {
		logError(1,
			"%s %s:Unable to allocate all_strbuff! ERROR %08X\n",
			tag, ERROR_ALLOC);
	}

	return count;
}

static ssize_t fts_gesture_coordinates_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return 0;
}
#endif

/***************** PRODUCTION TEST ****************/
static ssize_t fts_stm_cmd_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int n;
	char *p = (char *) buf;

	memset(typeOfComand, 0, CMD_STR_LEN * sizeof(u32));

	logError(1, "%s\n", tag);
	for (n = 0; n < (count + 1) / 3; n++) {

		if (sscanf(p, "%02X ", &typeOfComand[n]) != 1)
			return -EINVAL;
		p += 3;
		logError(1, "%s typeOfComand[%d] = %02X\n",
			tag, n, typeOfComand[n]);

	}

	numberParameters = n;
	logError(1, "%s Number of Parameters = %d\n", tag, numberParameters);
	return count;
}

static ssize_t fts_stm_cmd_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char buff[CMD_STR_LEN] = {0};
	int res, j, doClean = 0, count;

	int size = 6 * 2;
	u8 *all_strbuff = NULL;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	struct MutualSenseData compData = {0};
	struct SelfSenseData comData = {0};
	struct MutualSenseFrame frameMS = {0};
	struct SelfSenseFrame frameSS = {0};

	/**
	 * struct used for defining which test
	 * perform during the production test
	 */
	struct TestToDo todoDefault;

	todoDefault.MutualRaw = 1;
	todoDefault.MutualRawGap = 1;
	todoDefault.MutualCx1 = 0;
	todoDefault.MutualCx2 = 1;
	todoDefault.MutualCx2Adj = 1;
	todoDefault.MutualCxTotal = 0;
	todoDefault.MutualCxTotalAdj = 0;

	todoDefault.MutualKeyRaw = 0;
	todoDefault.MutualKeyCx1 = 0;
	todoDefault.MutualKeyCx2 = 0;
	todoDefault.MutualKeyCxTotal = 0;

	todoDefault.SelfForceRaw = 1;
	todoDefault.SelfForceRawGap = 0;
	todoDefault.SelfForceIx1 = 0;
	todoDefault.SelfForceIx2 = 0;
	todoDefault.SelfForceIx2Adj = 0;
	todoDefault.SelfForceIxTotal = 1;
	todoDefault.SelfForceIxTotalAdj = 0;
	todoDefault.SelfForceCx1 = 0;
	todoDefault.SelfForceCx2 = 0;
	todoDefault.SelfForceCx2Adj = 0;
	todoDefault.SelfForceCxTotal = 0;
	todoDefault.SelfForceCxTotalAdj = 0;

	todoDefault.SelfSenseRaw = 1;
	todoDefault.SelfSenseRawGap = 0;
	todoDefault.SelfSenseIx1 = 0;
	todoDefault.SelfSenseIx2 = 0;
	todoDefault.SelfSenseIx2Adj = 0;
	todoDefault.SelfSenseIxTotal = 1;
	todoDefault.SelfSenseIxTotalAdj = 0;
	todoDefault.SelfSenseCx1 = 0;
	todoDefault.SelfSenseCx2 = 0;
	todoDefault.SelfSenseCx2Adj = 0;
	todoDefault.SelfSenseCxTotal = 0;
	todoDefault.SelfSenseCxTotalAdj = 0;


	if (numberParameters >= 1) {
		res = fts_disableInterrupt();
		if (res < 0) {
			logError(0, "%s fts_disableInterrupt: ERROR %08X\n",
				tag, res);
			res = (res | ERROR_DISABLE_INTER);
			goto END;
		}

#if defined(CONFIG_FB_MSM)
	res = fb_unregister_client(&info->notifier);
#else
	res = msm_drm_unregister_client(&info->notifier);
#endif
		if (res < 0) {
			logError(1, "%s ERROR: unregister notifier failed!\n",
				tag);
			goto END;
		}

		switch (typeOfComand[0]) {
		/*ITO TEST*/
		case 0x01:
			res = production_test_ito();
			break;
		/*PRODUCTION TEST*/
		case 0x00:
			if (ftsInfo.u32_mpPassFlag != INIT_MP) {
				logError(0, "%s MP Flag not set!\n", tag, res);
				res = production_test_main(LIMITS_FILE, 1, 1,
						&todoDefault, INIT_MP);
			} else {
				logError(0, "%s MP Flag set!\n", tag, res);
				res = production_test_main(LIMITS_FILE, 1, 0,
					&todoDefault, INIT_MP);
			}
			break;
		/*read mutual raw*/
		case 0x13:
			logError(0, "%s Get 1 MS Frame\n", tag);
			//res = getMSFrame(ADDR_RAW_TOUCH, &frame, 0);
			res = getMSFrame2(MS_TOUCH_ACTIVE, &frameMS);
			if (res < 0) {
				logError(0,
					"%s Error in taking MS frame.%02X\n",
					tag, res);

			} else {
				logError(0, "%s The frame size is %d words\n",
					tag, res);
				size = (res * sizeof(short) + 8) * 2;
				/* set res to OK because if getMSFrame is*/
				/* successful res = number of words read*/
				res = OK;
			print_frame_short("MS frame =",
					array1dTo2d_short(frameMS.node_data,
						frameMS.node_data_size,
						frameMS.header.sense_node),
					frameMS.header.force_node,
					frameMS.header.sense_node);
			}
			break;
		/*read self raw*/
		case 0x15:
			logError(0, "%s Get 1 SS Frame\n", tag);
			res = getSSFrame2(SS_TOUCH, &frameSS);

			if (res < OK) {
				logError(0,
				"%s Error while taking the SS frame%02X\n",
					tag, res);

			} else {
				logError(0, "%s The frame size is %d words\n",
						tag, res);
				size = (res * sizeof(short) + 8) * 2 + 1;
				/**
				 * set res to OK because if getMSFrame is
				 * successful res = number of words read
				 */
				res = OK;
				print_frame_short("SS force frame =",
					array1dTo2d_short(frameSS.force_data,
					frameSS.header.force_node, 1),
					frameSS.header.force_node, 1);
				print_frame_short("SS sense frame =",
					array1dTo2d_short(frameSS.sense_data,
						frameSS.header.sense_node,
						frameSS.header.sense_node),
					1,
					frameSS.header.sense_node);
			}
			break;

		/*read mutual comp data*/
		case 0x14:
			logError(0, "%s Get MS Compensation Data\n", tag);
			res = readMutualSenseCompensationData(MS_TOUCH_ACTIVE,
				&compData);

			if (res < 0) {
				logError(0,
					"%s Error MS compensation data%02X\n",
					tag, res);
			} else {
				logError(0,
					"%s MS Data Reading Finished!\n",
					tag);
				size = ((compData.node_data_size + 9) *
						sizeof(u8)) * 2;
				print_frame_u8("MS Data (Cx2) =",
					array1dTo2d_u8(compData.node_data,
						compData.node_data_size,
						compData.header.sense_node),
					compData.header.force_node,
					compData.header.sense_node);
			}
			break;

		/*read self comp data*/
		case 0x16:
			logError(0, "%s Get SS Compensation Data...\n", tag);
			res = readSelfSenseCompensationData(SS_TOUCH, &comData);
			if (res < 0) {
				logError(0, "%s Error reading SS data%02X\n",
					tag, res);
			} else {
				logError(0, "%s SS Data Reading Finished!\n",
					tag);
				size = ((comData.header.force_node
					+ comData.header.sense_node) * 2 + 12);
				size *= sizeof(u8) * 2;
				print_frame_u8("SS Data Ix2_fm = ",
					array1dTo2d_u8(comData.ix2_fm,
						comData.header.force_node, 1),
					comData.header.force_node,
					1);
				print_frame_u8("SS Data Cx2_fm = ",
					array1dTo2d_u8(comData.cx2_fm,
						comData.header.force_node, 1),
					comData.header.force_node,
					1);
				print_frame_u8("SS Data Ix2_sn = ",
					array1dTo2d_u8(comData.ix2_sn,
						comData.header.sense_node,
						comData.header.sense_node),
					1,
					comData.header.sense_node);
				print_frame_u8("SS Data Cx2_sn = ",
					array1dTo2d_u8(comData.cx2_sn,
						comData.header.sense_node,
						comData.header.sense_node),
					1,
					comData.header.sense_node);
			}
			break;

		/* MS Raw DATA TEST */
		case 0x03:
			res = fts_system_reset();
			if (res >= OK)
				res = production_test_ms_raw(LIMITS_FILE,
					1, &todoDefault);
			break;
			/* MS CX DATA TEST */
		case 0x04:
			res = fts_system_reset();
			if (res >= OK)
				res = production_test_ms_cx(LIMITS_FILE,
					1, &todoDefault);
			break;
		/* SS RAW DATA TEST */
		case 0x05:
			res = fts_system_reset();
			if (res >= OK)
				res = production_test_ss_raw(LIMITS_FILE,
					1, &todoDefault);
			break;
		 /* SS IX CX DATA TEST */
		case 0x06:
			res = fts_system_reset();
			if (res >= OK)
				res = production_test_ss_ix_cx(LIMITS_FILE,
					1, &todoDefault);
			break;

		case 0xF0:
		/* TOUCH ENABLE/DISABLE */
		case 0xF1:
			doClean = (int) (typeOfComand[0] & 0x01);
			res = cleanUp(doClean);
			break;

		default:
			logError(1,
				"%s COMMAND NOT VALID!! Insert a proper value\n",
				tag);
			res = ERROR_OP_NOT_ALLOW;
			break;
		}

		doClean = fts_enableInterrupt();
		if (doClean < 0) {
			logError(0, "%s fts_enableInterrupt: ERROR %08X\n",
				tag, (doClean|ERROR_ENABLE_INTER));
		}
	} else {
		logError(1, "%s NO COMMAND SPECIFIED!!!\n", tag);
		res = ERROR_OP_NOT_ALLOW;
	}

#if defined(CONFIG_FB_MSM)
	if (fb_register_client(&info->notifier) < 0)
		logError(1, "%s ERROR: register notifier failed!\n", tag);
#else
	if (msm_drm_register_client(&info->notifier) < 0)
		logError(1, "%s ERROR: register notifier failed!\n", tag);
#endif

END:
	/*here start the reporting phase,*/
	/* assembling the data to send in the file node */
	all_strbuff = kmalloc(size, GFP_KERNEL);
	memset(all_strbuff, 0, size);

	snprintf(buff, sizeof(buff), "%02X", 0xAA);
	strlcat(all_strbuff, buff, 2);

	snprintf(buff, sizeof(buff), "%08X", res);
	strlcat(all_strbuff, buff, 8);

	if (res >= OK) {
		/*all the other cases are already fine printing only the res.*/
		switch (typeOfComand[0]) {
		case 0x13:
			snprintf(buff, sizeof(buff), "%02X",
				(u8) frameMS.header.force_node);
			strlcat(all_strbuff, buff, 2);

			snprintf(buff, sizeof(buff), "%02X",
				(u8) frameMS.header.sense_node);
			strlcat(all_strbuff, buff, 2);

			for (j = 0; j < frameMS.node_data_size; j++) {
				snprintf(buff, sizeof(buff), "%04X",
					frameMS.node_data[j]);
				strlcat(all_strbuff, buff, 4);
			}

			kfree(frameMS.node_data);
			break;

		case 0x15:
			snprintf(buff, sizeof(buff), "%02X",
				(u8) frameSS.header.force_node);
			strlcat(all_strbuff, buff, 2);

			snprintf(buff, sizeof(buff), "%02X",
				(u8) frameSS.header.sense_node);
			strlcat(all_strbuff, buff, 2);

			/* Copying self raw data Force */
			for (j = 0; j < frameSS.header.force_node; j++) {
				snprintf(buff, sizeof(buff), "%04X",
					frameSS.force_data[j]);
				strlcat(all_strbuff, buff, 4);
			}

			/* Copying self raw data Sense */
			for (j = 0; j < frameSS.header.sense_node; j++) {
				snprintf(buff, sizeof(buff), "%04X",
					frameSS.sense_data[j]);
				strlcat(all_strbuff, buff, 4);
			}

			kfree(frameSS.force_data);
			kfree(frameSS.sense_data);
			break;

		case 0x14:
			snprintf(buff, sizeof(buff), "%02X",
					(u8) compData.header.force_node);
			strlcat(all_strbuff, buff, 2);

			snprintf(buff, sizeof(buff), "%02X",
					(u8) compData.header.sense_node);
			strlcat(all_strbuff, buff, 2);

			/* Cpying CX1 value */
			snprintf(buff, sizeof(buff), "%02X", compData.cx1);
			strlcat(all_strbuff, buff, 2);

			/* Copying CX2 values */
			for (j = 0; j < compData.node_data_size; j++) {
				snprintf(buff, sizeof(buff), "%02X",
						*(compData.node_data + j));
				strlcat(all_strbuff, buff, 2);
			}

			kfree(compData.node_data);
			break;

		case 0x16:
			snprintf(buff, sizeof(buff), "%02X",
				comData.header.force_node);
			strlcat(all_strbuff, buff, 2);

			snprintf(buff, sizeof(buff), "%02X",
				comData.header.sense_node);
			strlcat(all_strbuff, buff, 2);

			snprintf(buff, sizeof(buff), "%02X", comData.f_ix1);
			strlcat(all_strbuff, buff, 2);

			snprintf(buff, sizeof(buff), "%02X", comData.s_ix1);
			strlcat(all_strbuff, buff, 2);

			snprintf(buff, sizeof(buff), "%02X", comData.f_cx1);
			strlcat(all_strbuff, buff, 2);

			snprintf(buff, sizeof(buff), "%02X", comData.s_cx1);
			strlcat(all_strbuff, buff, 2);

			/* Copying IX2 Force */
			for (j = 0; j < comData.header.force_node; j++) {
				snprintf(buff, sizeof(buff), "%02X",
					comData.ix2_fm[j]);
				strlcat(all_strbuff, buff, size);
			}

			/* Copying IX2 Sense */
			for (j = 0; j < comData.header.sense_node; j++) {
				snprintf(buff, sizeof(buff), "%02X",
					comData.ix2_sn[j]);
				strlcat(all_strbuff, buff, 2);
			}

			/* Copying CX2 Force */
			for (j = 0; j < comData.header.force_node; j++) {
				snprintf(buff, sizeof(buff), "%02X",
					comData.cx2_fm[j]);
				strlcat(all_strbuff, buff, 2);
			}

			/* Copying CX2 Sense */
			for (j = 0; j < comData.header.sense_node; j++) {
				snprintf(buff, sizeof(buff), "%02X",
					comData.cx2_sn[j]);
				strlcat(all_strbuff, buff, 2);
			}

			kfree(comData.ix2_fm);
			kfree(comData.ix2_sn);
			kfree(comData.cx2_fm);
			kfree(comData.cx2_sn);
			break;

		default:
			break;
		}
	}

	snprintf(buff, sizeof(buff), "%02X", 0xBB);
	strlcat(all_strbuff, buff, 2);

	count = snprintf(buf, TSP_BUF_SIZE, "%s\n", all_strbuff);
	/**
	 * need to reset the number of parameters
	 * in order to wait the next command,
	 * comment if you want to repeat
	 * the last command sent just doing a cat
	 */
	numberParameters = 0;
	/* logError(0,"%s numberParameters = %d\n",tag, numberParameters);*/
	kfree(all_strbuff);

	return count;
}

static DEVICE_ATTR_RW(fts_fwupdate);
static DEVICE_ATTR_RO(fts_appid);
static DEVICE_ATTR_RO(fts_mode_active);
static DEVICE_ATTR_RO(fts_lockdown_info);
static DEVICE_ATTR_RW(fts_strength_frame);
static DEVICE_ATTR_RO(fts_fw_test);
static DEVICE_ATTR_RW(fts_stm_cmd);
#ifdef USE_ONE_FILE_NODE
static DEVICE_ATTR_RW(fts_feature_enable);
#else

#ifdef EDGE_REJ
static DEVICE_ATTR_RW(fts_edge_rej);
#endif

#ifdef CORNER_REJ
static DEVICE_ATTR_RW(fts_corner_rej);
#endif

#ifdef EDGE_PALM_REJ
static DEVICE_ATTR_RW(fts_edge_palm_rej);
#endif

#ifdef CHARGER_MODE
static DEVICE_ATTR_RW(fts_charger_mode);
#endif

#ifdef GLOVE_MODE
static DEVICE_ATTR_RW(fts_glove_mode);
#endif

#ifdef VR_MODE
static DEVICE_ATTR_RW(fts_vr_mode);
#endif

#ifdef COVER_MODE
static DEVICE_ATTR_RW(fts_cover_mode);
#endif

#ifdef STYLUS_MODE
static DEVICE_ATTR_RW(fts_stylus_mode);
#endif

#endif

#ifdef PHONE_GESTURE
static DEVICE_ATTR_RW(fts_gesture_mask);
static DEVICE_ATTR_RW(fts_gesture_coordinates);
#ifdef USE_CUSTOM_GESTURES
static DEVICE_ATTR_RW(fts_add_custom_gesture);
static DEVICE_ATTR_RW(fts_remove_custom_gesture);
#endif
#endif

/*  /sys/devices/soc.0/f9928000.i2c/i2c-6/6-0049  */
static struct attribute *fts_attr_group[] = {
	&dev_attr_fts_fwupdate.attr,
	&dev_attr_fts_appid.attr,
	&dev_attr_fts_mode_active.attr,
	&dev_attr_fts_lockdown_info.attr,
	&dev_attr_fts_strength_frame.attr,
	&dev_attr_fts_fw_test.attr,
	&dev_attr_fts_stm_cmd.attr,
#ifdef USE_ONE_FILE_NODE
	&dev_attr_fts_feature_enable.attr,
#else

#ifdef EDGE_REJ
	&dev_attr_fts_edge_rej.attr,
#endif
#ifdef CORNER_REJ
	&dev_attr_fts_corner_rej.attr,
#endif
#ifdef EDGE_PALM_REJ
	&dev_attr_fts_edge_palm_rej.attr,
#endif
#ifdef CHARGER_MODE
	&dev_attr_fts_charger_mode.attr,
#endif
#ifdef GLOVE_MODE
	&dev_attr_fts_glove_mode.attr,
#endif
#ifdef VR_MODE
	&dev_attr_fts_vr_mode.attr,
#endif
#ifdef COVER_MODE
	&dev_attr_fts_cover_mode.attr,
#endif
#ifdef STYLUS_MODE
	&dev_attr_fts_stylus_mode.attr,
#endif

#endif

#ifdef PHONE_GESTURE
	&dev_attr_fts_gesture_mask.attr,
	&dev_attr_fts_gesture_coordinates.attr,
#ifdef USE_CUSTOM_GESTURES
	&dev_attr_fts_add_custom_gesture.attr,
	&dev_attr_fts_remove_custom_gesture.attr,
#endif

#endif
	NULL,
};

static int fts_command(struct fts_ts_info *info, unsigned char cmd)
{
	unsigned char regAdd;
	int ret;

	regAdd = cmd;

	ret = fts_writeCmd(&regAdd, sizeof(regAdd)); /* 0 = ok */

	logError(0, "%s Issued command 0x%02x, return value %08X\n", cmd, ret);

	return ret;
}

void fts_input_report_key(struct fts_ts_info *info, int key_code)
{
	mutex_lock(&info->input_report_mutex);
	input_report_key(info->input_dev, key_code, 1);
	input_sync(info->input_dev);
	input_report_key(info->input_dev, key_code, 0);
	input_sync(info->input_dev);
	mutex_unlock(&info->input_report_mutex);
}

/*
 * New Interrupt handle implementation
 */
static inline unsigned char *fts_next_event(unsigned char *evt)
{
	/* Nothing to do with this event, moving to the next one */
	evt += FIFO_EVENT_SIZE;

	/* the previous one was the last event ?  */
	return (evt[-1] & 0x1F) ? evt : NULL;
}

/* EventId : 0x00 */
static void fts_nop_event_handler(struct fts_ts_info *info,
	unsigned char *event)
{
	/**
	 * logError(1,
	 * "%s %s Doing nothing for event =
	 * %02X %02X %02X %02X %02X %02X %02X %02X\n",
	 * tag, __func__, event[0], event[1], event[2],
	 * event[3], event[4], event[5], event[6], event[7]);
	 */
	/* return fts_next_event(event); */
}

/* EventId : 0x03 */
static void fts_enter_pointer_event_handler(struct fts_ts_info *info,
			unsigned char *event)
{
	unsigned char touchId, touchcount;
	int x, y;
	int minor;
	int major, distance;
	u8 touchsize;
	struct touch_event *touch = NULL;

	distance = 0;
	if (!info->resume_bit)
		goto no_report;

	touchId = event[1] & 0x0F;
	touchcount = (event[1] & 0xF0) >> 4;
	touchsize = (event[5] & 0xC0) >> 6;
	major = (event[5] & 0x1F); // bit0-bit4: major
	minor = event[6]; // event6:minor

	__set_bit(touchId, &info->touch_id);

	x = (event[2] << 4) | (event[4] & 0xF0) >> 4;
	y = (event[3] << 4) | (event[4] & 0x0F);

	if (info->bdata->x_flip)
		x = X_AXIS_MAX - x;
	if (info->bdata->y_flip)
		y = Y_AXIS_MAX - y;

	if (x == X_AXIS_MAX)
		x--;

	if (y == Y_AXIS_MAX)
		y--;

	input_mt_slot(info->input_dev, touchId);
	input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 1);

	if (touchcount == 1) {
		input_report_key(info->input_dev, BTN_TOUCH, 1);
		input_report_key(info->input_dev, BTN_TOOL_FINGER, 1);
	}
	input_report_abs(info->input_dev, ABS_MT_POSITION_X, x);
	input_report_abs(info->input_dev, ABS_MT_POSITION_Y, y);
	input_report_abs(info->input_dev, ABS_MT_TOUCH_MAJOR, major);
	input_report_abs(info->input_dev, ABS_MT_TOUCH_MINOR, minor);
	input_report_abs(info->input_dev, ABS_MT_DISTANCE, distance);

	if (!info->aoi_notify_enabled)
		goto no_report;

	if ((x >= info->aoi_left && x < info->aoi_right)
		&& (y >= info->aoi_top && y < info->aoi_bottom)) {

		touch = &info->event[touchId];
		touch->x = x;
		touch->y = y;
		touch->fid = touchId;
		touch->type = 'M';
		do_gettimeofday(&touch->time);

		if (!test_bit(touchId, &info->finger_pressed)) {
			touch->type = 'D';
			__set_bit(touchId, &info->finger_pressed);
		}
		__set_bit(touchId, &info->event_mask);
	} else {
		if (test_bit(touchId, &info->finger_pressed)) {

			touch = &info->event[touchId];
			touch->x = 0;
			touch->y = 0;
			touch->fid = touchId;
			touch->type = 'U';
			do_gettimeofday(&touch->time);

			__clear_bit(touchId, &info->finger_pressed);
			__set_bit(touchId, &info->event_mask);
		}
	}

	if (info->event_mask) {
		touch_event_call_notifier(info->event_mask,
				(void *)&info->event[0]);
		info->event_mask = 0;
	}

no_report:
	return;

}

/* EventId : 0x04 */
static void fts_leave_pointer_event_handler(struct fts_ts_info *info,
			unsigned char *event)
{
	unsigned char touchId, touchcount;
	u8 touchsize;
	struct touch_event *touch = NULL;

	touchId = event[1] & 0x0F;
	touchcount = (event[1] & 0xF0) >> 4;
	touchsize = (event[5] & 0xC0) >> 6;

	input_mt_slot(info->input_dev, touchId);

	__clear_bit(touchId, &info->touch_id);
	input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 0);

	if (touchcount == 0) {
		input_report_key(info->input_dev, BTN_TOUCH, 0);
		input_report_key(info->input_dev, BTN_TOOL_FINGER, 0);
	}

	input_report_abs(info->input_dev, ABS_MT_TRACKING_ID, -1);

	if (!info->aoi_notify_enabled)
		return;

	if (test_bit(touchId, &info->finger_pressed)) {
		touch = &info->event[touchId];
		touch->x = 0;
		touch->y = 0;
		touch->fid = touchId;
		touch->type = 'U';
		do_gettimeofday(&touch->time);

		__clear_bit(touchId, &info->finger_pressed);
		__set_bit(touchId, &info->event_mask);
	}

	if (info->event_mask) {
		touch_event_call_notifier(info->event_mask,
				(void *)&info->event[0]);
		info->event_mask = 0;
	}
}

/* EventId : 0x05 */
#define fts_motion_pointer_event_handler fts_enter_pointer_event_handler

#ifdef PHONE_KEY
/* EventId : 0x0E */
static void fts_key_status_event_handler(struct fts_ts_info *info,
			unsigned char *event)
{
	int value;

	logError(0,
		"%s %sReceived event %02X %02X %02X %02X %02X %02X %02X %02X\n",
		tag, __func__, event[0], event[1], event[2], event[3],
		event[4], event[5], event[6], event[7]);
	/*
	 * TODO: the customer should handle the events coming
	 * from the keys according his needs (this is an example
	 * that report only the single pressure of one key at time)
	 */
	/* event[2] contain the bitmask of the keys that are actually pressed */
	if (event[2] != 0) {
		switch (event[2]) {
		case KEY1:
			value = KEY_HOMEPAGE;
			logError(0, "%s %s: Button HOME!\n", tag, __func__);
		break;

		case KEY2:
			value = KEY_BACK;
			logError(0, "%s %s: Button Back !\n", tag, __func__);
			break;

		case KEY3:
			value = KEY_MENU;
			logError(0, "%s %s: Button Menu !\n", tag, __func__);
			break;

		default:
			logError(0,
				"%s %s:No valid Button ID or more than one key pressed!\n",
				tag, __func__);
			return;
		}

		fts_input_report_key(info, value);
	} else {
		logError(0, "%s %s: All buttons released!\n", tag, __func__);
	}
}
#endif

/* EventId : 0x0F */
static void fts_error_event_handler(struct fts_ts_info *info,
		unsigned char *event)
{
	int error = 0;

	logError(0,
		"%s %sReceived event:%02X %02X %02X %02X %02X %02X %02X %02X\n",
		tag, __func__, event[0], event[1], event[2], event[3],
		event[4], event[5], event[6], event[7]);

	switch (event[1]) {
	case EVENT_TYPE_ESD_ERROR: /* esd */
		/* before reset clear all slot */
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
		break;
	case EVENT_TYPE_WATCHDOG_ERROR: /* watch dog timer */
		/* if (event[2] == 0) { */
		dumpErrorInfo();
		/* before reset clear all slot */
		release_all_touches(info);
		error = fts_system_reset();
		error |= fts_mode_handler(info, 0);
		error |= fts_enableInterrupt();
		if (error < OK) {
			logError(1,
				"%s %s Cannot reset the device ERROR %08X\n",
				tag, __func__, error);
		}
		/* } */
		break;
}
    /* return fts_next_event(event); */
}

/* EventId : 0x10 */
static void fts_controller_ready_event_handler(struct fts_ts_info *info,
	unsigned char *event)
{
	int error;

	logError(0, "%s %s Received event 0x%02x\n", tag, __func__, event[0]);
	release_all_touches(info);
	setSystemResettedUp(1);
	setSystemResettedDown(1);
	error = fts_mode_handler(info, 0);
	if (error < OK) {
		logError(1,
			"%s %s Cannot restore the device status ERROR %08X\n",
			tag, __func__, error);
	}
	/* return fts_next_event(event); */
}

/* EventId : 0x16 */
static void fts_status_event_handler(struct fts_ts_info *info,
		unsigned char *event)
{
	/* logError(1, "%s Received event 0x%02x\n", tag, event[0]); */

	switch (event[1]) {
	case EVENT_TYPE_MS_TUNING_CMPL:
	case EVENT_TYPE_SS_TUNING_CMPL:
	case FTS_FORCE_CAL_SELF_MUTUAL:
	case FTS_FLASH_WRITE_CONFIG:
	case FTS_FLASH_WRITE_COMP_MEMORY:
	case FTS_FORCE_CAL_SELF:
	case FTS_WATER_MODE_ON:
	case FTS_WATER_MODE_OFF:
	default:
		logError(0, "%s %s Received unhandled status event = ",
			tag, __func__);
		logError(0, "%02X %02X %02X %02X %02X %02X %02X %02X\n",
			event[0], event[1], event[2], event[3], event[4],
			event[5], event[6], event[7]);
		break;
	}

	/* return fts_next_event(event); */
}

#ifdef PHONE_GESTURE
/**
 * TODO: Customer should implement their own action
 * in respons of a gesture event.
 * This is an example that simply print the gesture received
 */
static void fts_gesture_event_handler(struct fts_ts_info *info,
			unsigned char *event)
{
	unsigned char touchId;
	int value;
	int needCoords = 0;

	logError(0,
		"%s gesture event: %02X %02X %02X %02X %02X %02X %02X %02X\n",
		tag, event[0], event[1], event[2], event[3],
		event[4], event[5], event[6], event[7]);

	if (event[1] == 0x03) {
		logError(1, "%s %s: Gesture ID %02X enable_status = %02X\n",
			tag, __func__, event[2], event[3]);
	}

	if (event[1] == EVENT_TYPE_ENB && event[2] == 0x00) {
		switch (event[3]) {
		case GESTURE_ENABLE:
			logError(1, "%s %s: Gesture Enabled! res = %02X\n",
				tag, __func__, event[4]);
			break;

		case GESTURE_DISABLE:
			logError(1, "%s %s: Gesture Disabled! res = %02X\n",
				tag, __func__, event[4]);
			break;

		default:
			logError(1, "%s %s: Event not Valid!\n", tag, __func__);
		}
	}

	if (event[0] == EVENTID_GESTURE && (event[1] == EVENT_TYPE_GESTURE_DTC1
			|| event[1] == EVENT_TYPE_GESTURE_DTC2)) {
		/* always use touchId zero */
		touchId = 0;
		__set_bit(touchId, &info->touch_id);

		/* by default read the coordinates*/
		/* for all gestures excluding double tap */
		needCoords = 1;

		switch (event[2]) {
		case GES_ID_DBLTAP:
			value = KEY_WAKEUP;
			logError(0, "%s %s: double tap!\n", tag, __func__);
			needCoords = 0;
		break;

		case GES_ID_AT:
			value = KEY_WWW;
			logError(0, "%s %s: @!\n", tag, __func__);
			break;

		case GES_ID_C:
			value = KEY_C;
			logError(0, "%s %s: C !\n", tag, __func__);
			break;

		case GES_ID_E:
			value = KEY_E;
			logError(0, "%s %s: e !\n", tag, __func__);
		break;

		case GES_ID_F:
			value = KEY_F;
			logError(0, "%s %s: F !\n", tag, __func__);
			break;

		case GES_ID_L:
			value = KEY_L;
			logError(0, "%s %s: L !\n", tag, __func__);
			break;

		case GES_ID_M:
			value = KEY_M;
			logError(0, "%s %s: M !\n", tag, __func__);
			break;

		case GES_ID_O:
			value = KEY_O;
			logError(0, "%s %s: O !\n", tag, __func__);
			break;

		case GES_ID_S:
			value = KEY_S;
			logError(0, "%s %s: S !\n", tag, __func__);
			break;

		case GES_ID_V:
			value = KEY_V;
			logError(0, "%s %s:  V !\n", tag, __func__);
			break;

		case GES_ID_W:
			value = KEY_W;
			logError(0, "%s %s:  W !\n", tag, __func__);
			break;

		case GES_ID_Z:
			value = KEY_Z;
			logError(0, "%s %s:  Z !\n", tag, __func__);
			break;

		case GES_ID_HFLIP_L2R:
			value = KEY_RIGHT;
			logError(0, "%s %s:  -> !\n", tag, __func__);
			break;

		case GES_ID_HFLIP_R2L:
			value = KEY_LEFT;
			logError(0, "%s %s:  <- !\n", tag, __func__);
			break;

		case GES_ID_VFLIP_D2T:
			value = KEY_UP;
			logError(0, "%s %s:  UP !\n", tag, __func__);
			break;

		case GES_ID_VFLIP_T2D:
			value = KEY_DOWN;
			logError(0, "%s %s:  DOWN !\n", tag, __func__);
			break;

		case GES_ID_CUST1:
			value = KEY_F1;
			logError(0, "%s %s:  F1 !\n", tag, __func__);
			break;

		case GES_ID_CUST2:
			value = KEY_F1;
			logError(0, "%s %s:  F2 !\n", tag, __func__);
			break;

		case GES_ID_CUST3:
			value = KEY_F3;
			logError(0, "%s %s:  F3 !\n", tag, __func__);
			break;

		case GES_ID_CUST4:
			value = KEY_F1;
			logError(0, "%s %s:  F4 !\n", tag, __func__);
			break;

		case GES_ID_CUST5:
			value = KEY_F1;
			logError(0, "%s %s:  F5 !\n", tag, __func__);
			break;

		case GES_ID_LEFTBRACE:
			value = KEY_LEFTBRACE;
			logError(0, "%s %s:  < !\n", tag, __func__);
			break;

		case GES_ID_RIGHTBRACE:
			value = KEY_RIGHTBRACE;
			logError(0, "%s %s:  > !\n", tag, __func__);
			break;
		default:
			logError(0, "%s %s:  No valid GestureID!\n",
				tag, __func__);
			goto gesture_done;
		}

		/* no coordinates for gestures reported by FW */
		if (event[1] == EVENT_TYPE_GESTURE_DTC1)
			needCoords = 0;

		if (needCoords == 1)
			readGestureCoords(event);

		fts_input_report_key(info, value);

gesture_done:
		/* Done with gesture event, clear bit. */
		__clear_bit(touchId, &info->touch_id);
	}
	/* return fts_next_event(event); */
}
#endif

/* EventId : 0x05 */
#define fts_motion_pointer_event_handler fts_enter_pointer_event_handler

/*
 * This handler is called each time there is at least
 * one new event in the FIFO
 */
static void fts_event_handler(struct work_struct *work)
{
	struct fts_ts_info *info;
	int error = 0, count = 0;
	unsigned char regAdd;
	unsigned char data[FIFO_EVENT_SIZE] = {0};
	unsigned char eventId;

	struct event_dispatch_handler_t event_handler;

	info = container_of(work, struct fts_ts_info, work);
	/*
	 * read all the FIFO and parsing events
	 */

	__pm_wakeup_event(&info->wakeup_source, HZ);
	regAdd = FIFO_CMD_READONE;

	for (count = 0; count < FIFO_DEPTH; count++) {
		error = fts_readCmd(&regAdd, sizeof(regAdd), data,
				FIFO_EVENT_SIZE);
		if (error == OK && data[0] != EVENTID_NO_EVENT)
			eventId = data[0];
		else
			break;

		if (eventId < EVENTID_LAST) {
			event_handler = info->event_dispatch_table[eventId];
			event_handler.handler(info, (data));
		}
	}
	input_sync(info->input_dev);

	fts_interrupt_enable(info);
}

static void fts_fw_update_auto(struct work_struct *work)
{
	u8 cmd[4] = { FTS_CMD_HW_REG_W, 0x00, 0x00, SYSTEM_RESET_VALUE };
	int event_to_search[2] = {(int)EVENTID_ERROR_EVENT,
					(int)EVENT_TYPE_CHECKSUM_ERROR};
	u8 readData[FIFO_EVENT_SIZE] = {0};
	int flag_init = 0;
	int retval = 0;
	int retval1 = 0;
	int ret;
	struct fts_ts_info *info;
	struct delayed_work *fwu_work = container_of(work,
	struct delayed_work, work);
	int crc_status = 0;
	int error = 0;

	info = container_of(fwu_work, struct fts_ts_info, fwu_work);
	logError(0, "%s Fw Auto Update is starting...\n", tag);

	retval = flashProcedure(PATH_FILE_FW, crc_status, 1);
	if ((retval & ERROR_MEMH_READ) || (retval & ERROR_FW_NO_UPDATE)) {
		logError(1, "%s %s: no firmware file or no newer firmware!\n",
			tag, __func__);
		goto NO_FIRMWARE_UPDATE;
	} else if ((retval & 0xFF000000) == ERROR_FLASH_PROCEDURE) {
		logError(1, "%s %s:firmware update retry! ERROR %08X\n",
			tag, __func__, retval);
		fts_chip_powercycle(info);

		retval1 = flashProcedure(PATH_FILE_FW, crc_status, 1);

		if ((retval1 & 0xFF000000) == ERROR_FLASH_PROCEDURE) {
			logError(1, "%s %s: update failed again! ERROR %08X\n",
				tag, __func__, retval1);
			logError(1, "%s Fw Auto Update Failed!\n", tag);
		}
	}

	u16ToU8_be(SYSTEM_RESET_ADDRESS, &cmd[1]);
	ret = fts_writeCmd(cmd, 4);
	if (ret < OK) {
		logError(1, "%s %s Can't send reset command! ERROR %08X\n",
			tag, __func__, ret);
	} else {
		setSystemResettedDown(1);
		setSystemResettedUp(1);
		ret = pollForEvent(event_to_search, 2, readData,
				GENERAL_TIMEOUT);
		if (ret < OK) {
			logError(0, "%s %s: No CX CRC Found!\n", tag, __func__);
		} else {
			if (readData[2] == CRC_CX_MEMORY) {
				logError(1, "%s %s: CRC Error! ERROR:%02X\n\n",
					tag, __func__, readData[2]);

				flag_init = 1;
			}
		}
	}

	if (ftsInfo.u8_msScrConfigTuneVer != ftsInfo.u8_msScrCxmemTuneVer ||
		ftsInfo.u8_ssTchConfigTuneVer != ftsInfo.u8_ssTchCxmemTuneVer)
		ret = ERROR_GET_INIT_STATUS;
	else if (((ftsInfo.u32_mpPassFlag != INIT_MP)
		&& (ftsInfo.u32_mpPassFlag != INIT_FIELD)) || flag_init == 1)
		ret = ERROR_GET_INIT_STATUS;
	else
		ret = OK;

	if (ret == ERROR_GET_INIT_STATUS) {
		error = fts_chip_initialization(info);
		if (error < OK)
			logError(1, "%s %s Can't initialize chip! ERROR %08X",
				tag, __func__, error);
	}

NO_FIRMWARE_UPDATE:
	error = fts_init_afterProbe(info);
	if (error < OK)
		logError(1, "%s Can't initialize hardware device ERROR %08X\n",
			tag, error);

	logError(0, "%s Fw Auto Update Finished!\n", tag);
}

static int fts_chip_initialization(struct fts_ts_info *info)
{
	int ret2 = 0;
	int retry;
	int initretrycnt = 0;
	struct TestToDo todoDefault;

	todoDefault.MutualRaw = 1;
	todoDefault.MutualRawGap = 1;
	todoDefault.MutualCx1 = 0;
	todoDefault.MutualCx2 = 0;
	todoDefault.MutualCx2Adj = 0;
	todoDefault.MutualCxTotal = 0;
	todoDefault.MutualCxTotalAdj = 0;

	todoDefault.MutualKeyRaw = 0;
	todoDefault.MutualKeyCx1 = 0;
	todoDefault.MutualKeyCx2 = 0;
	todoDefault.MutualKeyCxTotal = 0;

	todoDefault.SelfForceRaw = 0;
	todoDefault.SelfForceRawGap = 0;
	todoDefault.SelfForceIx1 = 0;
	todoDefault.SelfForceIx2 = 0;
	todoDefault.SelfForceIx2Adj = 0;
	todoDefault.SelfForceIxTotal = 0;
	todoDefault.SelfForceIxTotalAdj = 0;
	todoDefault.SelfForceCx1 = 0;
	todoDefault.SelfForceCx2 = 0;
	todoDefault.SelfForceCx2Adj = 0;
	todoDefault.SelfForceCxTotal = 0;
	todoDefault.SelfForceCxTotalAdj = 0;

	todoDefault.SelfSenseRaw = 1;
	todoDefault.SelfSenseRawGap = 0;
	todoDefault.SelfSenseIx1 = 0;
	todoDefault.SelfSenseIx2 = 0;
	todoDefault.SelfSenseIx2Adj = 0;
	todoDefault.SelfSenseIxTotal = 0;
	todoDefault.SelfSenseIxTotalAdj = 0;
	todoDefault.SelfSenseCx1 = 0;
	todoDefault.SelfSenseCx2 = 0;
	todoDefault.SelfSenseCx2Adj = 0;
	todoDefault.SelfSenseCxTotal = 0;
	todoDefault.SelfSenseCxTotalAdj = 0;

	for (retry = 0; retry <= INIT_FLAG_CNT; retry++) {
		ret2 = production_test_main(LIMITS_FILE, 1, 1, &todoDefault,
					INIT_FIELD);
		if (ret2 == OK)
			break;
		initretrycnt++;
		logError(1, "%s %s: cycle count = %04d - ERROR %08X\n",
			tag, __func__, initretrycnt, ret2);
		fts_chip_powercycle(info);
	}

	if (ret2 < OK)
		logError(1, "%s failed to initializate 3 times\n", tag);

	return ret2;
}

#ifdef FTS_USE_POLLING_MODE

static enum hrtimer_restart fts_timer_func(struct hrtimer *timer)
{
	struct fts_ts_info *info =
		container_of(timer, struct fts_ts_info, timer);

	queue_work(info->event_wq, &info->work);
	return HRTIMER_NORESTART;
}
#else

static irqreturn_t fts_interrupt_handler(int irq, void *handle)
{
	struct fts_ts_info *info = handle;

	disable_irq_nosync(info->client->irq);

	queue_work(info->event_wq, &info->work);

	return IRQ_HANDLED;
}
#endif

static int fts_interrupt_install(struct fts_ts_info *info)
{
	int i, error = 0;
	size_t len;

	len = sizeof(struct event_dispatch_handler_t) * EVENTID_LAST;
	info->event_dispatch_table = kzalloc(len, GFP_KERNEL);

	if (!info->event_dispatch_table) {
		logError(1, "%s OOM allocating event dispatch table\n", tag);
		return -ENOMEM;
	}

	for (i = 0; i < EVENTID_LAST; i++)
		info->event_dispatch_table[i].handler = fts_nop_event_handler;
	install_handler(info, ENTER_POINTER, enter_pointer);
	install_handler(info, LEAVE_POINTER, leave_pointer);
	install_handler(info, MOTION_POINTER, motion_pointer);
	install_handler(info, ERROR_EVENT, error);
	install_handler(info, CONTROL_READY, controller_ready);
	install_handler(info, STATUS_UPDATE, status);
#ifdef PHONE_GESTURE
	install_handler(info, GESTURE, gesture);
#endif
#ifdef PHONE_KEY
	install_handler(info, KEY_STATUS, key_status);
#endif
	/* disable interrupts in any case */
	error = fts_disableInterrupt();

#ifdef FTS_USE_POLLING_MODE
	logError(0, "%s Polling Mode\n");
	hrtimer_init(&info->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	info->timer.function = fts_timer_func;
	hrtimer_start(&info->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
#else
	logError(0, "%s Interrupt Mode\n", tag);
	if (request_threaded_irq(info->client->irq, NULL, fts_interrupt_handler,
		IRQF_TRIGGER_LOW | IRQF_ONESHOT, info->client->name, info)) {
		logError(1, "%s Request irq failed\n", tag);
		kfree(info->event_dispatch_table);
		error = -EBUSY;
	}
#endif
	return error;
}

static void fts_interrupt_uninstall(struct fts_ts_info *info)
{
	fts_disableInterrupt();

	kfree(info->event_dispatch_table);
#ifdef FTS_USE_POLLING_MODE
	hrtimer_cancel(&info->timer);
#else
	free_irq(info->client->irq, info);
#endif
}

static void fts_interrupt_enable(struct fts_ts_info *info)
{
#ifdef FTS_USE_POLLING_MODE
	hrtimer_start(&info->timer, ktime_set(0, 10000000), HRTIMER_MODE_REL);
#else
	enable_irq(info->client->irq);
#endif
	/* enable the touch IC irq */
	fts_enableInterrupt();
}

static void fts_interrupt_disable(struct fts_ts_info *info)
{
	/* disable the touch IC irq */
	fts_disableInterrupt();

#ifdef FTS_USE_POLLING_MODE
	hrtimer_cancel(&info->timer);
#else
	disable_irq(info->client->irq);
#endif

}

static int fts_init(struct fts_ts_info *info)
{
	int error;

	error = fts_system_reset();
	if (error < OK && error != (ERROR_TIMEOUT | ERROR_SYSTEM_RESET_FAIL)) {
		logError(1, "%s Cannot reset the device! ERROR %08X\n",
			tag, error);
		return error;
	}
	if (error == (ERROR_TIMEOUT | ERROR_SYSTEM_RESET_FAIL)) {
		logError(1, "%s Setting default Chip INFO!\n", tag);
		defaultChipInfo(0);
	} else {
		error = readChipInfo(0);
		if (error < OK) {
			logError(1, "%s Cannot read Chip Info!ERROR:%08X\n",
				tag, error);
		}
	}

	error = fts_interrupt_install(info);

	if (error != OK)
		logError(1, "%s Init (1) error (ERROR  = %08X)\n", tag, error);

	return error;
}

int fts_chip_powercycle(struct fts_ts_info *info)
{
	int error = 0;

	logError(0, "%s %s: Power Cycle Starting...\n", tag, __func__);

	/*
	 * if IRQ pin is short with DVDD a call to
	 * the ISR will triggered when the regulator is turned off
	 */

	logError(0, "%s %s: Disabling IRQ...\n", tag, __func__);
	disable_irq_nosync(info->client->irq);
	if (info->pwr_reg) {
		error = regulator_disable(info->pwr_reg);
		if (error < 0) {
			logError(1, "%s %s: Failed to disable DVDD regulator\n",
				tag, __func__);
		}
	}

	if (info->bus_reg) {
		error = regulator_disable(info->bus_reg);
		if (error < 0) {
			logError(1, "%s %s: Failed to disable AVDD regulator\n",
				tag, __func__);
		}
	}

	if (info->bdata->reset_gpio != GPIO_NOT_DEFINED)
		gpio_set_value(info->bdata->reset_gpio, 0);
	else
		msleep(300);

	if (info->pwr_reg) {
		error = regulator_enable(info->bus_reg);
		if (error < 0) {
			logError(1, "%s %s: Failed to enable AVDD regulator\n",
				tag, __func__);
		}
	}

	if (info->bus_reg) {
		error = regulator_enable(info->pwr_reg);
		if (error < 0) {
			logError(1, "%s %s: Failed to enable DVDD regulator\n",
				tag, __func__);
		}
	}
	/* time needed by the regulators for reaching the regime values */
	msleep(20);

	if (info->bdata->reset_gpio != GPIO_NOT_DEFINED) {
		/* time to wait before bring up the reset */
		/* gpio after the power up of the regulators */
		msleep(20);
		gpio_set_value(info->bdata->reset_gpio, 1);
		/* mdelay(300); */
	}

	release_all_touches(info);

	logError(0, "%s %s: Enabling IRQ...\n", tag, __func__);
	enable_irq(info->client->irq);
	logError(0, "%s %s: Power Cycle Finished! ERROR CODE = %08x\n",
		tag, __func__, error);
	setSystemResettedUp(1);
	setSystemResettedDown(1);
	return error;
}

int fts_chip_powercycle2(struct fts_ts_info *info, unsigned long sleep)
{
	int error = 0;

	logError(0, "%s %s: Power Cycle Starting...\n", tag, __func__);

	if (info->pwr_reg) {
		error = regulator_disable(info->pwr_reg);
		if (error < 0) {
			logError(1, "%s %s: Failed to disable DVDD regulator\n",
				tag, __func__);
		}
	}

	if (info->bus_reg) {
		error = regulator_disable(info->bus_reg);
		if (error < 0) {
			logError(1, "%s %s: Failed to disable AVDD regulator\n",
				tag, __func__);
		}
	}

	if (info->bdata->reset_gpio != GPIO_NOT_DEFINED)
		gpio_set_value(info->bdata->reset_gpio, 0);

	msleep(sleep);
	if (info->pwr_reg) {
		error = regulator_enable(info->bus_reg);
		if (error < 0) {
			logError(1, "%s %s: Failed to enable AVDD regulator\n",
				tag, __func__);
		}
	}

	if (info->bus_reg) {
		error = regulator_enable(info->pwr_reg);
		if (error < 0) {
			logError(1, "%s %s: Failed to enable DVDD regulator\n",
				tag, __func__);
		}
	}
	/* time needed by the regulators for reaching the regime values */
	msleep(500);

	if (info->bdata->reset_gpio != GPIO_NOT_DEFINED) {
		/*
		 * time to wait before bring up the reset
		 * gpio after the power up of the regulators
		 */
		msleep(20);
		gpio_set_value(info->bdata->reset_gpio, 1);
		/* msleep(300); */
	}

	/* before reset clear all slot */
	release_all_touches(info);

	logError(0, "%s %s: Power Cycle Finished! ERROR CODE = %08x\n",
		tag, __func__, error);
	setSystemResettedUp(1);
	setSystemResettedDown(1);
	return error;
}

static int fts_init_afterProbe(struct fts_ts_info *info)
{
	int error = 0;

	/* system reset */
	error = cleanUp(0);

	/* enable the features and the sensing */
	error |= fts_mode_handler(info, 0);

	/* enable the interrupt */
	error |= fts_enableInterrupt();

#if defined(CONFIG_FB_MSM)
	error |= fb_register_client(&info->notifier);
#else
	error |= msm_drm_register_client(&info->notifier);
#endif

	if (error < OK)
		logError(1, "%s %s Init after Probe error (ERROR = %08X)\n",
			tag, __func__, error);

	return error;
}

/*
 * TODO: change this function according with the needs
 * of customer in terms of feature to enable/disable
 */
static int fts_mode_handler(struct fts_ts_info *info, int force)
{
	int res = OK;
	int ret = OK;

	/* initialize the mode to Nothing in order */
	/* to be updated depending on the features enabled */
	info->mode = MODE_NOTHING;

	logError(0, "%s %s: Mode Handler starting...\n", tag, __func__);
	switch (info->resume_bit) {
	case 0:
		/* screen down */
		logError(0, "%s %s: Screen OFF...\n", tag, __func__);
		/*
		 * do sense off in order to avoid the flooding
		 * of the fifo with touch events if someone is
		 * touching the panel during suspend
		 */
		logError(0, "%s %s: Sense OFF!\n", tag, __func__);
		/*
		 *we need to use fts_command for speed reason
		 * (no need to check echo in this case and interrupt
		 * can be enabled)
		 */
		res |= fts_command(info, FTS_CMD_MS_MT_SENSE_OFF);
#ifdef PHONE_KEY
		logError(0, "%s %s: Key OFF!\n", tag, __func__);
		res |= fts_command(info, FTS_CMD_MS_KEY_OFF);
#endif

#ifdef PHONE_GESTURE
		if (info->gesture_enabled == 1) {
			logError(0, "%s %s: enter in gesture mode!\n",
				tag, __func__);
			ret = enterGestureMode(isSystemResettedDown());
			if (ret >= OK) {
				info->mode |= FEAT_GESTURE;
			} else {
				logError(1,
					"%s %s:enterGestureMode failed!%08X recovery in senseOff\n",
					tag, __func__, ret);
			}
			res |= ret;
		}
#endif
		if (info->mode != (FEAT_GESTURE|MODE_NOTHING)
			|| info->gesture_enabled == 0)
			info->mode |= MODE_SENSEOFF;
		setSystemResettedDown(0);
		break;

	case 1:
		/* screen up */
		logError(0, "%s %s: Screen ON...\n", tag, __func__);

#ifdef FEAT_GLOVE
		if ((info->glove_enabled == FEAT_ENABLE &&
			isSystemResettedUp()) || force == 1) {
			logError(0, "%s %s: Glove Mode setting...\n",
				tag, __func__);
			ret = featureEnableDisable(info->glove_enabled,
					FEAT_GLOVE);
			if (ret < OK) {
				logError(1,
				     "%s %s:error in setting GLOVE_MODE!%08X\n",
				     tag, __func__, ret);
			}
			res |= ret;

			if (ret >= OK && info->glove_enabled == FEAT_ENABLE) {
				info->mode |= FEAT_GLOVE;
				logError(1, "%s %s: GLOVE_MODE Enabled!\n",
					tag, __func__);
			} else {
				logError(1, "%s %s: GLOVE_MODE Disabled!\n",
					tag, __func__);
			}
		}
#endif
#ifdef FEAT_STYLUS
		if ((info->stylus_enabled == FEAT_ENABLE &&
			isSystemResettedUp()) || force == 1) {
			logError(0, "%s %s: Stylus Mode setting...\n",
						tag, __func__);
			ret = featureEnableDisable(info->stylus_enabled,
					FEAT_STYLUS);
			if (ret < OK) {
				logError(1,
					"%s %s:error in set STYLUS_MODE!%08X\n",
					tag, __func__, ret);
			}
			res |= ret;

			if (ret >= OK && info->stylus_enabled == FEAT_ENABLE) {
				info->mode |= FEAT_STYLUS;
				logError(1, "%s %s: STYLUS_MODE Enabled!\n",
					tag, __func__);
			} else {
				logError(1, "%s %s: STYLUS_MODE Disabled!\n",
					tag, __func__);
			}
		}
#endif
#ifdef FEAT_COVER
		if ((info->cover_enabled == FEAT_ENABLE &&
			isSystemResettedUp()) || force == 1) {
			logError(0, "%s %s: Cover Mode setting...\n",
				tag, __func__);
			ret = featureEnableDisable(info->cover_enabled,
					FEAT_COVER);
			if (ret < OK) {
				logError(1,
					"%s %s:error setting COVER_MODE!%08X\n",
					tag, __func__, ret);
			}
			res |= ret;

			if (ret >= OK && info->cover_enabled == FEAT_ENABLE) {
				info->mode |= FEAT_COVER;
				logError(1, "%s %s: COVER_MODE Enabled!\n",
					tag, __func__);
			} else {
				logError(1, "%s %s: COVER_MODE Disabled!\n",
					tag, __func__);
			}
		}
#endif
#ifdef FEAT_CHARGER
		if ((info->charger_enabled == FEAT_ENABLE &&
			isSystemResettedUp()) || force == 1) {
			logError(0, "%s %s: Charger Mode setting...\n",
					tag, __func__);
			ret = featureEnableDisable(info->charger_enabled,
				FEAT_CHARGER);
			if (ret < OK) {
				logError(1,
					"%s %s:error set CHARGER_MODE!%08X\n",
					tag, __func__, ret);
			}
			res |= ret;

			if (ret >= OK && info->charger_enabled == FEAT_ENABLE) {
				info->mode |= FEAT_CHARGER;
				logError(1, "%s %s: CHARGER_MODE Enabled!\n",
					tag, __func__);
			} else {
				logError(1, "%s %s: CHARGER_MODE Disabled!\n",
					tag, __func__);
			}
		}
#endif
#ifdef FEAT_VR
		if ((info->vr_enabled == FEAT_ENABLE &&
			isSystemResettedUp()) || force == 1) {
			logError(0, "%s %s: Vr Mode setting\n", tag, __func__);
			ret = featureEnableDisable(info->vr_enabled, FEAT_VR);
			if (ret < OK) {
				logError(1,
					"%s %s:error setting VR_MODE!:%08X\n",
					tag, __func__, ret);
				}
			res |= ret;

			if (ret >= OK && info->vr_enabled == FEAT_ENABLE) {
				info->mode |= FEAT_VR;
				logError(1, "%s %s: VR_MODE Enabled!\n",
					tag, __func__);
			} else {
				logError(1, "%s %s: VR_MODE Disabled!\n",
					tag, __func__);
			}
		}
#endif
#ifdef FEAT_EDGE_REJECTION
		if ((info->edge_rej_enabled == FEAT_ENABLE &&
			isSystemResettedUp()) || force == 1) {
			logError(0, "%s %s: Edge Rejection Mode setting\n",
				tag, __func__);
			ret = featureEnableDisable(info->edge_rej_enabled,
				FEAT_EDGE_REJECTION);
			if (ret < OK) {
				logError(1,
				"%s %s:err set EDGE_REJECTION_MODE!%08X\n",
					tag, __func__, ret);
			}
			res |= ret;

			if (ret >= OK && info->edge_rej_enabled ==
					FEAT_ENABLE) {
				info->mode |= FEAT_EDGE_REJECTION;
				logError(1,
					"%s %s:EDGE_REJECTION_MODE Enabled!\n",
					tag, __func__);
			} else {
				logError(1,
					"%s %s:EDGE_REJECTION_MODE Disabled!\n",
					tag, __func__);
			}
		}
#endif
#ifdef FEAT_CORNER_REJECTION
		if ((info->corner_rej_enabled == FEAT_ENABLE &&
			isSystemResettedUp()) || force == 1) {
			logError(0, "%s %s: Corner rejection Mode setting\n",
				tag, __func__);
			ret = featureEnableDisable(info->corner_rej_enabled,
				FEAT_CORNER_REJECTION);
			if (ret < OK) {
				logError(1,
					"%s%s:err CORNER_REJECTION_MODE!%08X\n",
					tag, __func__, ret);
			}
			res |= ret;

			if (ret >= OK && info->corner_rej_enabled ==
				FEAT_ENABLE) {
				info->mode |= FEAT_CORNER_REJECTION;
				logError(1,
					"%s%s:CORNER_REJECTION_MODE Enabled!\n",
					tag, __func__);
			} else {
				logError(1,
					"%s%s:CORNER_REJECTION_MODE Disabled\n",
					tag, __func__);
			}
		}
#endif
#ifdef FEAT_EDGE_PALM_REJECTION
		if ((info->edge_palm_rej_enabled == FEAT_ENABLE &&
			isSystemResettedUp()) || force == 1) {
			logError(0, "%s %s:Edge Palm rejection Mode setting\n",
				tag, __func__);
			ret = featureEnableDisable(info->edge_palm_rej_enabled,
				FEAT_EDGE_PALM_REJECTION);
			if (ret < OK) {
				logError(1,
				    "%s %s:err EDGE_PALM_REJECTION_MODE!%08X\n",
				    tag, __func__, ret);
			}
			res |= ret;

			if (ret >= OK && info->edge_palm_rej_enabled ==
				FEAT_ENABLE) {
				info->mode |= FEAT_EDGE_PALM_REJECTION;
				logError(1,
				    "%s %s:EDGE_PALM_REJECTION_MODE Enabled!\n",
				    tag, __func__);
			} else {
				logError(1,
				   "%s %s:EDGE_PALM_REJECTION_MODE Disabled!\n",
				    tag, __func__);
			}
		}
#endif
		logError(0, "%s %s: Sense ON!\n", tag, __func__);
		res |= fts_command(info, FTS_CMD_MS_MT_SENSE_ON);
		info->mode |= MODE_SENSEON;
#ifdef PHONE_KEY
		logError(0, "%s %s: Key ON!\n", tag, __func__);
		res |= fts_command(info, FTS_CMD_MS_KEY_ON);
#endif
		setSystemResettedUp(0);
		break;

	default:
		logError(1,
			"%s %s: invalid resume_bit value = %d! ERROR %08X\n",
			tag, __func__, info->resume_bit, ERROR_OP_NOT_ALLOW);
		res = ERROR_OP_NOT_ALLOW;
	}
	logError(0, "%s %s: Mode Handler finished! res = %08X\n", tag, __func__,
		res);
	return res;
}

static int fts_chip_power_switch(struct fts_ts_info *info, bool on)
{
	int error = -1;

	if (info->bdata->pwr_on_suspend) {
		if (!info->ts_pinctrl)
			return 0;

		if (on) {
			error = pinctrl_select_state(info->ts_pinctrl,
				info->pinctrl_state_active);
			if (error < 0)
				logError(1, "%s: Failed to select %s\n",
					__func__, PINCTRL_STATE_ACTIVE);
		} else {
			error = pinctrl_select_state(info->ts_pinctrl,
				info->pinctrl_state_suspend);
			if (error < 0)
				logError(1, "%s: Failed to select %s\n",
					__func__, PINCTRL_STATE_SUSPEND);
		}

		return 0;
	}

	if (on) {
		if (info->pwr_reg) {
			error = regulator_enable(info->bus_reg);
			if (error < 0)
				logError(1, "%s %s: Failed to enable AVDD\n",
					tag, __func__);
		}

		if (info->bus_reg) {
			error = regulator_enable(info->pwr_reg);
			if (error < 0)
				logError(1, "%s %s: Failed to enable DVDD\n",
					tag, __func__);
		}

		if (info->ts_pinctrl) {
			if (pinctrl_select_state(info->ts_pinctrl,
				info->pinctrl_state_active) < 0) {
				logError(1, "%s: Failed to select %s\n",
					__func__, PINCTRL_STATE_ACTIVE);
			}
		}
	} else {
		if (info->bdata->reset_gpio != GPIO_NOT_DEFINED)
			gpio_set_value(info->bdata->reset_gpio, 0);
		else
			msleep(300);

		if (info->ts_pinctrl) {
			if (pinctrl_select_state(info->ts_pinctrl,
				info->pinctrl_state_suspend) < 0) {
				logError(1, "%s: Failed to select %s\n",
					__func__, PINCTRL_STATE_SUSPEND);
			}
		}

		if (info->pwr_reg) {
			error = regulator_disable(info->pwr_reg);
			if (error < 0)
				logError(1, "%s %s: Failed to disable DVDD\n",
					tag, __func__);
		}

		if (info->bus_reg) {
			error = regulator_disable(info->bus_reg);
			if (error < 0)
				logError(1, "%s %s: Failed to disable AVDD\n",
					tag, __func__);
		}
	}
	return error;
}


static void fts_resume_work(struct work_struct *work)
{
	struct fts_ts_info *info;

	info = container_of(work, struct fts_ts_info, resume_work);

	__pm_wakeup_event(&info->wakeup_source, HZ);

	fts_chip_power_switch(info, true);

	info->resume_bit = 1;

	fts_system_reset();
#ifdef USE_NOISE_PARAM
	readNoiseParameters(noise_params);
#endif

#ifdef USE_NOISE_PARAM
	writeNoiseParameters(noise_params);
#endif

	release_all_touches(info);

	fts_mode_handler(info, 0);

	info->sensor_sleep = false;

	fts_interrupt_enable(info);
}

static void fts_suspend_work(struct work_struct *work)
{
	struct fts_ts_info *info;

	info = container_of(work, struct fts_ts_info, suspend_work);

	__pm_wakeup_event(&info->wakeup_source, HZ);

	info->resume_bit = 0;

	fts_mode_handler(info, 0);

	fts_interrupt_disable(info);
	release_all_touches(info);
	info->sensor_sleep = true;

	fts_chip_power_switch(info, false);
}

#if defined(CONFIG_FB_MSM)
static int fts_fb_state_chg_callback(struct notifier_block *nb,
			unsigned long val, void *data)
{
	struct fts_ts_info *info = container_of(nb,
			struct fts_ts_info, notifier);
	struct fb_event *evdata = data;
	unsigned int blank;

	if (!evdata || (evdata->id != 0))
		return 0;

	if (val != FB_EVENT_BLANK)
		return 0;

	logError(0, "%s %s: fts notifier begin!\n", tag, __func__);

	if (evdata->data && val == FB_EVENT_BLANK && info) {

		blank = *(int *) (evdata->data);

		switch (blank) {
		case FB_BLANK_POWERDOWN:
			if (info->sensor_sleep)
				break;

			logError(0, "%s %s: FB_BLANK_POWERDOWN\n",
				tag, __func__);

			queue_work(info->event_wq, &info->suspend_work);

			break;

		case FB_BLANK_UNBLANK:
			if (!info->sensor_sleep)
				break;

			logError(0, "%s %s: FB_BLANK_UNBLANK\n",
				tag, __func__);

			queue_work(info->event_wq, &info->resume_work);
			break;
		default:
			break;
		}
	}
	return NOTIFY_OK;
}

#else
static int fts_fb_state_chg_callback(struct notifier_block *nb,
		unsigned long val, void *data)
{
	struct fts_ts_info *info = container_of(nb, struct fts_ts_info,
				notifier);
	struct msm_drm_notifier *evdata = data;
	unsigned int blank;

	if (!evdata || (evdata->id != 0))
		return 0;

	if (val != MSM_DRM_EVENT_BLANK)
		return 0;
	logError(0, "%s %s: fts notifier begin!\n", tag, __func__);

	if (evdata->data && val == MSM_DRM_EVENT_BLANK && info) {
		blank = *(int *) (evdata->data);

		switch (blank) {
		case MSM_DRM_BLANK_POWERDOWN:
			if (info->sensor_sleep)
				break;

			if (info->aoi_notify_enabled)
				info->aoi_wake_on_suspend = true;
			else
				info->aoi_wake_on_suspend = false;

			if (info->aoi_wake_on_suspend)
				info->sensor_sleep = true;
			else
				queue_work(info->event_wq, &info->suspend_work);
			break;

		case MSM_DRM_BLANK_UNBLANK:
			if (!info->sensor_sleep)
				break;

			if (!info->resume_bit)
				queue_work(info->event_wq, &info->resume_work);

			if (info->aoi_wake_on_suspend)
				info->sensor_sleep = false;

			break;
		default:
			break;
		}
	}
	return NOTIFY_OK;
}
#endif

static struct notifier_block fts_noti_block = {
	.notifier_call = fts_fb_state_chg_callback,
};

static int fts_pinctrl_init(struct fts_ts_info *info)
{
	int retval;

	/* Get pinctrl if target uses pinctrl */
	info->ts_pinctrl = devm_pinctrl_get(info->dev);
	if (IS_ERR_OR_NULL(info->ts_pinctrl)) {
		retval = PTR_ERR(info->ts_pinctrl);
		logError(1, "Target does not use pinctrl %d\n", retval);
		goto err_pinctrl_get;
	}

	info->pinctrl_state_active
		= pinctrl_lookup_state(info->ts_pinctrl, PINCTRL_STATE_ACTIVE);
	if (IS_ERR_OR_NULL(info->pinctrl_state_active)) {
		retval = PTR_ERR(info->pinctrl_state_active);
		logError(1, "Can not lookup %s pinstate %d\n",
					PINCTRL_STATE_ACTIVE, retval);
		goto err_pinctrl_lookup;
	}

	info->pinctrl_state_suspend
		= pinctrl_lookup_state(info->ts_pinctrl, PINCTRL_STATE_SUSPEND);
	if (IS_ERR_OR_NULL(info->pinctrl_state_suspend)) {
		retval = PTR_ERR(info->pinctrl_state_suspend);
		logError(1, "Can not lookup %s pinstate %d\n",
					PINCTRL_STATE_SUSPEND, retval);
		goto err_pinctrl_lookup;
	}

	info->pinctrl_state_release
		= pinctrl_lookup_state(info->ts_pinctrl, PINCTRL_STATE_RELEASE);
	if (IS_ERR_OR_NULL(info->pinctrl_state_release)) {
		retval = PTR_ERR(info->pinctrl_state_release);
		logError(1, "Can not lookup %s pinstate %d\n",
					PINCTRL_STATE_RELEASE, retval);
	}

	return 0;

err_pinctrl_lookup:
	devm_pinctrl_put(info->ts_pinctrl);
err_pinctrl_get:
	info->ts_pinctrl = NULL;
	return retval;
}

static int fts_get_reg(struct fts_ts_info *info, bool get)
{
	int retval;
	const struct fts_i2c_platform_data *bdata = info->bdata;

	if (!get) {
		retval = 0;
		goto regulator_put;
	}

	if ((bdata->pwr_reg_name != NULL) && (*bdata->pwr_reg_name != 0)) {
		info->pwr_reg = regulator_get(info->dev,
					bdata->pwr_reg_name);
		if (IS_ERR(info->pwr_reg)) {
			logError(1, "%s %s: Failed to get power regulator\n",
				tag, __func__);
			retval = PTR_ERR(info->pwr_reg);
			goto regulator_put;
		}
	}

	if ((bdata->bus_reg_name != NULL) && (*bdata->bus_reg_name != 0)) {
		info->bus_reg = regulator_get(info->dev,
					bdata->bus_reg_name);
		if (IS_ERR(info->bus_reg)) {
			logError(1,
				"%s %s:Failed to get bus pullup regulator\n",
				tag, __func__);
			retval = PTR_ERR(info->bus_reg);
			goto regulator_put;
		}
	}

	return 0;

regulator_put:
	if (info->pwr_reg) {
		regulator_put(info->pwr_reg);
		info->pwr_reg = NULL;
	}

	if (info->bus_reg) {
		regulator_put(info->bus_reg);
		info->bus_reg = NULL;
	}

	return retval;
}

static int fts_enable_reg(struct fts_ts_info *info,
		bool enable)
{
	int retval;

	if (!enable) {
		retval = 0;
		goto disable_pwr_reg;
	}

	if (info->bus_reg) {
		retval = regulator_enable(info->bus_reg);
		if (retval < 0) {
			logError(1, "%s %s: Failed to enable bus regulator\n",
				tag, __func__);
			goto exit;
		}
	}

	if (info->pwr_reg) {
		retval = regulator_enable(info->pwr_reg);
		if (retval < 0) {
			logError(1, "%s %s: Failed to enable power regulator\n",
				tag, __func__);
			goto disable_bus_reg;
		}
	}

	return OK;

disable_pwr_reg:
	if (info->pwr_reg)
		regulator_disable(info->pwr_reg);

disable_bus_reg:
	if (info->bus_reg)
		regulator_disable(info->bus_reg);

exit:
	return retval;
}

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

static int fts_set_gpio(struct fts_ts_info *info)
{
	int retval;
	const struct fts_i2c_platform_data *bdata =
			info->bdata;

	retval = fts_gpio_setup(bdata->irq_gpio, true, 0, 0);
	if (retval < 0) {
		logError(1, "%s %s: Failed to configure irq GPIO\n",
			tag, __func__);
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
		msleep(20);
		gpio_set_value(bdata->reset_gpio, 1);
	}

	setResetGpio(bdata->reset_gpio);
	return OK;

err_gpio_reset:
	fts_gpio_setup(bdata->irq_gpio, false, 0, 0);
	setResetGpio(GPIO_NOT_DEFINED);
err_gpio_irq:
	return retval;
}

static int parse_dt(struct device *dev,
		struct fts_i2c_platform_data *bdata)
{
	int retval;
	const char *name;
	struct device_node *np = dev->of_node;

	bdata->irq_gpio = of_get_named_gpio_flags(np,
		"st,irq-gpio", 0, NULL);

	logError(0, "%s irq_gpio = %d\n", tag, bdata->irq_gpio);

	bdata->pwr_on_suspend =
		of_property_read_bool(np, "st,power_on_suspend");

	retval = of_property_read_string(np, "st,regulator_dvdd", &name);
	if (retval == -EINVAL)
		bdata->pwr_reg_name = NULL;
	else if (retval < 0)
		return retval;

	bdata->pwr_reg_name = name;
	logError(0, "%s pwr_reg_name = %s\n", tag, name);

	retval = of_property_read_string(np, "st,regulator_avdd", &name);
	if (retval == -EINVAL)
		bdata->bus_reg_name = NULL;
	else if (retval < 0)
		return retval;

	bdata->bus_reg_name = name;
	logError(0, "%s bus_reg_name = %s\n", tag, name);

	if (of_property_read_bool(np, "st,reset-gpio")) {
		bdata->reset_gpio = of_get_named_gpio_flags(np,
				"st,reset-gpio", 0, NULL);
		logError(0, "%s reset_gpio =%d\n", tag, bdata->reset_gpio);
	} else {
		bdata->reset_gpio = GPIO_NOT_DEFINED;
	}

	bdata->x_flip = of_property_read_bool(np, "st,x-flip");
	bdata->y_flip = of_property_read_bool(np, "st,y-flip");

	return OK;
}

static int fts_probe(struct i2c_client *client,
		const struct i2c_device_id *idp)
{
	struct fts_ts_info *info = NULL;
	int error = 0;
	struct device_node *dp = client->dev.of_node;
	int retval;
	int skip_5_1 = 0;

	logError(0, "%s %s: driver probe begin!\n", tag, __func__);

	logError(0, "%s SET I2C Functionality and Dev INFO:\n", tag);
	openChannel(client);
	logError(0, "%s driver ver. %s (built on)\n", tag, FTS_TS_DRV_VERSION);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		logError(1, "%s Unsupported I2C functionality\n", tag);
		error = -EIO;
		goto ProbeErrorExit_0;
	}

	info = kzalloc(sizeof(struct fts_ts_info), GFP_KERNEL);
	if (!info) {
		logError(1,
			"%s Out of memory, can't to allocate struct info!\n",
			tag);
		error = -ENOMEM;
		goto ProbeErrorExit_0;
	}

	info->client = client;

	i2c_set_clientdata(client, info);

	info->i2c_data = kmalloc(I2C_DATA_MAX_LEN, GFP_KERNEL);
	if (info->i2c_data == NULL) {
		error = -ENOMEM;
		goto ProbeErrorExit_0P1;
	}
	info->i2c_data_len = I2C_DATA_MAX_LEN;

	logError(0, "%s i2c address: %x\n", tag, client->addr);
	info->dev = &info->client->dev;
	if (dp) {
		info->bdata = devm_kzalloc(&client->dev,
				sizeof(struct fts_i2c_platform_data),
				GFP_KERNEL);
		if (!info->bdata) {
			logError(1, "%s ERROR:info.bdata kzalloc failed\n",
				tag);
			goto ProbeErrorExit_1;
		}
		parse_dt(&client->dev, info->bdata);
	}

	logError(0, "%s SET Regulators:\n", tag);
	retval = fts_get_reg(info, true);
	if (retval < 0) {
		logError(1, "%s ERROR: %s: Failed to get regulators\n",
			tag, __func__);
		goto ProbeErrorExit_1;
	}

	retval = fts_enable_reg(info, true);
	if (retval < 0) {
		logError(1, "%s %s: ERROR Failed to enable regulators\n",
			tag, __func__);
		goto ProbeErrorExit_2;
	}

	logError(0, "%s SET GPIOS:\n", tag);
	retval = fts_set_gpio(info);
	if (retval < 0) {
		logError(1, "%s %s: ERROR Failed to set up GPIO's\n",
			tag, __func__);
		goto ProbeErrorExit_2;
	}
	info->client->irq = gpio_to_irq(info->bdata->irq_gpio);

	retval = fts_pinctrl_init(info);
	if (!retval && info->ts_pinctrl) {
		/*
		 * Pinctrl handle is optional. If pinctrl handle is found
		 * let pins to be configured in active state. If not
		 * found continue further without error.
		 */
		retval = pinctrl_select_state(info->ts_pinctrl,
						info->pinctrl_state_active);
		if (retval < 0) {
			logError(1, "%s: Failed to select %s pinstate %d\n",
				__func__, PINCTRL_STATE_ACTIVE, retval);
		}
	}

	logError(0, "%s SET Auto Fw Update:\n", tag);
	info->fwu_workqueue = alloc_workqueue("fts-fwu-queue",
				WQ_UNBOUND|WQ_HIGHPRI|WQ_CPU_INTENSIVE, 1);
	if (!info->fwu_workqueue) {
		logError(1, "%s ERROR: Cannot create fwu work thread\n", tag);
		goto ProbeErrorExit_3;
	}

	INIT_DELAYED_WORK(&info->fwu_work, fts_fw_update_auto);

	logError(0, "%s SET Event Handler:\n", tag);
	wakeup_source_init(&info->wakeup_source, "fts_tp");
	info->event_wq = alloc_workqueue("fts-event-queue",
				WQ_UNBOUND|WQ_HIGHPRI|WQ_CPU_INTENSIVE, 1);
	if (!info->event_wq) {
		logError(1, "%s ERROR: Cannot create work thread\n", tag);
		error = -ENOMEM;
		goto ProbeErrorExit_4;
	}

	INIT_WORK(&info->work, fts_event_handler);

	INIT_WORK(&info->resume_work, fts_resume_work);
	INIT_WORK(&info->suspend_work, fts_suspend_work);

	logError(0, "%s SET Input Device Property:\n", tag);
	/* info->dev = &info->client->dev; */
	info->input_dev = input_allocate_device();
	if (!info->input_dev) {
		logError(1, "%s ERROR: No such input device defined!\n",
			tag);
		error = -ENODEV;
		goto ProbeErrorExit_5;
	}
	info->input_dev->dev.parent = &client->dev;
	info->input_dev->name = FTS_TS_DRV_NAME;
	snprintf(fts_ts_phys, sizeof(fts_ts_phys), "%s/input0",
		info->input_dev->name);
	info->input_dev->phys = fts_ts_phys;
	info->input_dev->id.bustype = BUS_I2C;
	info->input_dev->id.vendor = 0x0001;
	info->input_dev->id.product = 0x0002;
	info->input_dev->id.version = 0x0100;

	__set_bit(EV_SYN, info->input_dev->evbit);
	__set_bit(EV_KEY, info->input_dev->evbit);
	__set_bit(EV_ABS, info->input_dev->evbit);
	__set_bit(BTN_TOUCH, info->input_dev->keybit);
	__set_bit(BTN_TOOL_FINGER, info->input_dev->keybit);

	input_mt_init_slots(info->input_dev, TOUCH_ID_MAX, INPUT_MT_DIRECT);

	input_set_abs_params(info->input_dev, ABS_MT_POSITION_X,
			X_AXIS_MIN, X_AXIS_MAX, 0, 0);
	input_set_abs_params(info->input_dev, ABS_MT_POSITION_Y,
			Y_AXIS_MIN, Y_AXIS_MAX, 0, 0);
	input_set_abs_params(info->input_dev, ABS_MT_TOUCH_MAJOR,
			AREA_MIN, AREA_MAX, 0, 0);
	input_set_abs_params(info->input_dev, ABS_MT_TOUCH_MINOR,
			AREA_MIN, AREA_MAX, 0, 0);

#ifdef PHONE_GESTURE
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
	/* KEY associated to the touch screen buttons */
	input_set_capability(info->input_dev, EV_KEY, KEY_HOMEPAGE);
	input_set_capability(info->input_dev, EV_KEY, KEY_BACK);
	input_set_capability(info->input_dev, EV_KEY, KEY_MENU);
#endif

	mutex_init(&(info->input_report_mutex));

#ifdef PHONE_GESTURE
	mutex_init(&gestureMask_mutex);
#endif

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

	/* init hardware device */
	logError(0, "%s Device Initialization:\n", tag);
	error = fts_init(info);
	if (error < OK) {
		logError(1, "%s Cannot initialize the device ERROR %08X\n",
			tag, error);
		error = -ENODEV;
		goto ProbeErrorExit_6;
	}

	/*
	 * init feature switches (by default all the features
	 * are disable, if one feature want to be enabled from
	 * the start, set the corresponding value to 1)
	 */
	info->gesture_enabled = 0;
	info->glove_enabled = 0;
	info->charger_enabled = 0;
	info->stylus_enabled = 0;
	info->vr_enabled = 0;
	info->cover_enabled = 0;
	info->edge_rej_enabled = 0;
	info->corner_rej_enabled = 0;
	info->edge_palm_rej_enabled = 0;

	info->resume_bit = 1;
	info->notifier = fts_noti_block;

	logError(0, "%s SET Device File Nodes:\n", tag);
	/* sysfs stuff */
	info->attrs.attrs = fts_attr_group;
	error = sysfs_create_group(&client->dev.kobj, &info->attrs);
	if (error) {
		logError(1, "%s ERROR: Cannot create sysfs structure!\n", tag);
		error = -ENODEV;
		goto ProbeErrorExit_7;
	}

#ifdef SCRIPTLESS
	/* I2C cmd */
	if (fts_cmd_class == NULL)
		fts_cmd_class = class_create(THIS_MODULE, FTS_TS_DRV_NAME);
	info->i2c_cmd_dev = device_create(fts_cmd_class,
			NULL, DCHIP_ID_0, info, "fts_i2c");
	if (IS_ERR(info->i2c_cmd_dev)) {
		logError(1,
			"%s ERROR: Failed to create device for the sysfs!\n",
			tag);
		goto ProbeErrorExit_8;
	}

	dev_set_drvdata(info->i2c_cmd_dev, info);

	error = sysfs_create_group(&info->i2c_cmd_dev->kobj,
			&i2c_cmd_attr_group);
	if (error) {
		logError(1, "%s ERROR: Failed to create sysfs group!\n", tag);
		goto ProbeErrorExit_9;
	}
#endif

#ifdef DRIVER_TEST
	if (fts_cmd_class == NULL)
		fts_cmd_class = class_create(THIS_MODULE, FTS_TS_DRV_NAME);
	info->test_cmd_dev = device_create(fts_cmd_class,
			NULL, DCHIP_ID_0, info, "fts_driver_test");
	if (IS_ERR(info->test_cmd_dev)) {
		logError(1,
			"%s ERROR: Failed to create device for the sysfs!\n",
			tag);
		goto ProbeErrorExit_10;
	}

	dev_set_drvdata(info->test_cmd_dev, info);

	error = sysfs_create_group(&info->test_cmd_dev->kobj,
			&test_cmd_attr_group);
	if (error) {
		logError(1, "%s ERROR: Failed to create sysfs group!\n", tag);
		goto ProbeErrorExit_11;
	}
#endif

	if (fts_cmd_class == NULL)
		fts_cmd_class = class_create(THIS_MODULE, FTS_TS_DRV_NAME);
	info->aoi_cmd_dev = device_create(fts_cmd_class,
			NULL, DCHIP_ID_0, info, "touch_aoi");
	if (IS_ERR(info->aoi_cmd_dev)) {
		logError(1,
			"%s ERROR: Failed to create device for the sysfs\n",
			tag);
		goto ProbeErrorExit_10;
	}

	dev_set_drvdata(info->aoi_cmd_dev, info);

	error = sysfs_create_group(&info->aoi_cmd_dev->kobj,
			&aoi_cmd_attr_group);
	if (error) {
		logError(1, "%s ERROR: Failed to create sysfs group\n", tag);
		goto ProbeErrorExit_11;
	}

	queue_delayed_work(info->fwu_workqueue, &info->fwu_work,
			msecs_to_jiffies(EXP_FN_WORK_DELAY_MS));
	logError(1, "%s Probe Finished!\n", tag);

	info->event_mask = 0;

	return OK;

	/* error exit path */
#ifdef DRIVER_TEST
ProbeErrorExit_11:
#ifndef SCRIPTLESS
	device_destroy(fts_cmd_class, DCHIP_ID_0);
#endif

ProbeErrorExit_10:
#ifndef SCRIPTLESS
	sysfs_remove_group(&client->dev.kobj, &info->attrs);
#endif
#endif

#ifdef SCRIPTLESS
ProbeErrorExit_9:
	device_destroy(fts_cmd_class, DCHIP_ID_0);

ProbeErrorExit_8:
	sysfs_remove_group(&client->dev.kobj, &info->attrs);
#endif

ProbeErrorExit_7:
	/* fb_unregister_client(&info->notifier); */

ProbeErrorExit_6:
	input_unregister_device(info->input_dev);

ProbeErrorExit_5_1:
	if (skip_5_1 != 1)
		input_free_device(info->input_dev);

ProbeErrorExit_5:
	destroy_workqueue(info->event_wq);

ProbeErrorExit_4:
	destroy_workqueue(info->fwu_workqueue);
	wakeup_source_trash(&info->wakeup_source);

ProbeErrorExit_3:
	if (info->ts_pinctrl) {
		if (IS_ERR_OR_NULL(info->pinctrl_state_release)) {
			devm_pinctrl_put(info->ts_pinctrl);
			info->ts_pinctrl = NULL;
		} else {
			if (pinctrl_select_state(info->ts_pinctrl,
						info->pinctrl_state_release))
				logError(1, "%s:Failed to select %s pinstate\n",
					__func__, PINCTRL_STATE_RELEASE);
		}
	}
	fts_enable_reg(info, false);
	fts_gpio_setup(info->bdata->irq_gpio, false, 0, 0);
	fts_gpio_setup(info->bdata->reset_gpio, false, 0, 0);

ProbeErrorExit_2:
	fts_get_reg(info, false);

ProbeErrorExit_1:
	kfree(info->i2c_data);
ProbeErrorExit_0P1:
	kfree(info);

ProbeErrorExit_0:
	logError(1, "%s Probe Failed!\n", tag);

	return error;
}

static int fts_remove(struct i2c_client *client)
{
	struct fts_ts_info *info = i2c_get_clientdata(client);

#ifdef DRIVER_TEST
	sysfs_remove_group(&info->test_cmd_dev->kobj,
			&test_cmd_attr_group);
#endif

#ifdef SCRIPTLESS
	/* I2C cmd */
	sysfs_remove_group(&info->i2c_cmd_dev->kobj, &i2c_cmd_attr_group);
#endif

#if defined(SCRIPTLESS) || defined(DRIVER_TEST)
	device_destroy(fts_cmd_class, DCHIP_ID_0);
#endif

	/* sysfs stuff */
	sysfs_remove_group(&client->dev.kobj, &info->attrs);

	/* remove interrupt and event handlers */
	fts_interrupt_uninstall(info);

#if defined(CONFIG_FB_MSM)
	fb_unregister_client(&info->notifier);
#else
	msm_drm_unregister_client(&info->notifier);
#endif

	/* unregister the device */
	input_unregister_device(info->input_dev);

	/* input_free_device(info->input_dev ); */

	/* Empty the FIFO buffer */
	fts_command(info, FIFO_CMD_FLUSH);
	/* flushFIFO(); */

	/* Remove the work thread */
	destroy_workqueue(info->event_wq);
	/* wake_lock_destroy(&info->wakelock); */
	wakeup_source_trash(&info->wakeup_source);
	destroy_workqueue(info->fwu_workqueue);

	if (info->ts_pinctrl) {
		if (IS_ERR_OR_NULL(info->pinctrl_state_release)) {
			devm_pinctrl_put(info->ts_pinctrl);
			info->ts_pinctrl = NULL;
		} else {
			pinctrl_select_state(info->ts_pinctrl,
						info->pinctrl_state_release);
		}
	}
	fts_enable_reg(info, false);
	fts_gpio_setup(info->bdata->irq_gpio, false, 0, 0);
	fts_gpio_setup(info->bdata->reset_gpio, false, 0, 0);

	fts_get_reg(info, false);

	/* free all */
	kfree(info->i2c_data);
	kfree(info);

	return OK;
}

static const struct of_device_id fts_of_match_table[] = {
	{
		.compatible = "st,fts",
	},
	{},
};
static const struct i2c_device_id fts_device_id[] = {
	{FTS_TS_DRV_NAME, 0},
	{}
};

static struct i2c_driver fts_i2c_driver = {
	.driver = {
		.name = FTS_TS_DRV_NAME,
		.of_match_table = fts_of_match_table,
	},
	.probe = fts_probe,
	.remove = fts_remove,
	.id_table = fts_device_id,
};

static int __init fts_driver_init(void)
{
	return i2c_add_driver(&fts_i2c_driver);
}

static void __exit fts_driver_exit(void)
{
	i2c_del_driver(&fts_i2c_driver);
}

late_initcall(fts_driver_init);
module_exit(fts_driver_exit);

MODULE_DESCRIPTION("STMicroelectronics MultiTouch IC Driver");
MODULE_LICENSE("GPL v2");
