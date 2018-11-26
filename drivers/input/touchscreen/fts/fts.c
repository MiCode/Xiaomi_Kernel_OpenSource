/*
 * fts.c
 *
 * FTS Capacitive touch screen controller (FingerTipS)
 *
 * Copyright (C) 2016, STMicroelectronics Limited.
 * Copyright (C) 2018 XiaoMi, Inc.
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

#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>

#include <linux/notifier.h>
#ifdef CONFIG_DRM
#include <drm/drm_notifier.h>
#endif

#ifdef KERNEL_ABOVE_2_6_38
#include <linux/input/mt.h>
#endif
#ifdef CONFIG_TOUCHSCREEN_ST_DEBUG_FS
#include <linux/debugfs.h>
#endif
#include <linux/hwinfo.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/input/touch_common_info.h>
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
#ifdef CONFIG_INPUT_PRESS_NDT
#include "./../ndt_core.h"
#endif



#define LINK_KOBJ_NAME "tp"

#define event_id(_e)     EVENTID_##_e
#define handler_name(_h) fts_##_h##_event_handler

#define install_handler(_i, _evt, _hnd) \
do {_i->event_dispatch_table[event_id(_evt)] = handler_name(_hnd); } while (0)
#define WAIT_WITH_TIMEOUT(_info, _timeout, _command) \
do { \
	if (wait_for_completion_timeout(&_info->cmd_done, _timeout) == 0) {\
			dev_warn(_info->dev, "Waiting for %s command: timeout\n",\
			#_command);\
	} \
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
#if defined(SCRIPTLESS) || defined(DRIVER_TEST)
static struct class *fts_cmd_class;
#endif

extern chipInfo ftsInfo;
unsigned char tune_version_same;
char tag[8] = "[ FTS ]\0";
static u32 *typeOfComand;
static int numberParameters;
static int feature_feasibility = ERROR_OP_NOT_ALLOW;
#ifdef PHONE_GESTURE
static u8 mask[GESTURE_MASK_SIZE + 2];
extern struct mutex gestureMask_mutex;
#endif
#ifdef USE_NOISE_PARAM
static u8 noise_params[NOISE_PARAMETERS_SIZE] = {0};
#endif
static void fts_interrupt_enable(struct fts_ts_info *info);
static int fts_init_hw(struct fts_ts_info *info);
static int fts_mode_handler(struct fts_ts_info *info, int force);
static int fts_command(struct fts_ts_info *info, unsigned char cmd);
static struct fts_ts_info *fts_info;
static int fts_chip_autotune(struct fts_ts_info *info);
static int fts_enable_reg(struct fts_ts_info *fts_data, bool enable);
static int fts_flash_procedure(struct fts_ts_info *info, const char *fw_name, int force);
static const char *fts_get_limit(struct fts_ts_info *info);
#ifdef EDGEHOVER_FOR_VOLUME
static void fts_clear_point(struct fts_ts_info *info);
#endif
#ifdef CONFIG_INPUT_PRESS_NDT
bool aod_mode = false;
#endif

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
		input_mt_slot(info->input_dev, i);
		input_mt_report_slot_state(info->input_dev, type, 0);
		input_report_abs(info->input_dev, ABS_MT_TRACKING_ID, -1);
	}
    input_sync(info->input_dev);
    info->touch_id = 0;
}

static ssize_t fts_fwupdate_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	char *fw_name;
	int len = 0;
	int retval;

	len = strnlen(buf, count);
	fw_name = kzalloc(len + 1, GFP_KERNEL);
	if (fw_name == NULL)
		return -ENOMEM;

	if (count > 0) {
		strlcpy(fw_name, buf, len);
		if (fw_name[len - 1] == '\n')
			fw_name[len - 1] = 0;
		else
			fw_name[len] = 0;
	}

	log_error("%s firmware name :%s\n", tag, fw_name);
	retval = fts_flash_procedure(fts_info, fw_name, 1);
	fts_info->fwupdate_stat = retval;
	kfree(fw_name);
	fw_name = NULL;
	return count;
}

static ssize_t fts_fwupdate_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%08X\n", fts_info->fwupdate_stat);
}

static ssize_t fts_sysfs_config_id_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%x.%x\n", ftsInfo.u16_fwVer, ftsInfo.u16_cfgId);
}

#ifdef EDGEHOVER_FOR_VOLUME
static ssize_t fts_edge_value_store(struct device *dev, struct device_attribute *attr,
				    const char *buf, size_t count)
{
	sscanf(buf, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
			&fts_info->debug_abs, &fts_info->bdata->y_base, &fts_info->bdata->y_offset, &fts_info->bdata->y_skip, &fts_info->doubletap_interval, &fts_info->doubletap_distance, &fts_info->samp_interval_start, &fts_info->slide_thr_start, &fts_info->samp_interval, &fts_info->slide_thr, &fts_info->m_samp_interval, &fts_info->m_slide_thr, &fts_info->single_press_time_low, &fts_info->single_press_time_hi);
	return count;
}

static ssize_t fts_edge_value_show(struct device *dev, struct device_attribute *attr,
				   char *buf)
{
	return snprintf(buf, PAGE_SIZE, "debug_abs:%d\ny_base:%d,y_offset:%d,y_skip:%d\ndoubletap_interval:%d,doubletap_distance:%d\nsamp_interval_start:%d,slide_thr_start:%d,samp_interval:%d,slide_thr:%d\n,current_samp_interval:%d,current_slide_thr:%d\nsingle_press_time_low:%d,single_press_time_high:%d\n",
			fts_info->debug_abs, fts_info->bdata->y_base, fts_info->bdata->y_offset, fts_info->bdata->y_skip, fts_info->doubletap_interval, fts_info->doubletap_distance, fts_info->samp_interval_start, fts_info->slide_thr_start, fts_info->samp_interval, fts_info->slide_thr, fts_info->m_samp_interval, fts_info->m_slide_thr, fts_info->single_press_time_low, fts_info->single_press_time_hi);
}
#endif

static int check_feature_feasibility(struct fts_ts_info *info, unsigned int feature)
{
	int res = ERROR_OP_NOT_ALLOW;

	if (info->resume_bit == 0) {
		switch (feature) {
#ifdef PHONE_GESTURE

		case FEAT_GESTURE:
			res = OK;
			break;
#endif

		default:
			log_error("%s %s: Feature not allowed in this operating mode! ERROR %08X\n", tag, __func__, res);
			break;
		}
	} else {
		switch (feature) {
#ifdef PHONE_GESTURE

		case FEAT_GESTURE:
#endif
		case FEAT_GLOVE:
			res = OK;
			break;

		default:
			log_error("%s %s: Feature not allowed in this operating mode! ERROR %08X\n", tag, __func__, res);
			break;
		}
	}

	return res;
}

static ssize_t fts_feature_enable_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);
	char *p = (char *)buf;
	unsigned int temp;
	int res = OK;

	if ((count + 1) / 3 != 2) {
		log_error("%s fts_feature_enable: Number of parameter wrong! %zu > %d\n", tag, (count + 1) / 3, 2);
	} else {
		sscanf(p, "%02X ", &temp);
		p += 3;
		res = check_feature_feasibility(info, temp);

		if (res >= OK) {
			switch (temp) {
#ifdef PHONE_GESTURE

			case FEAT_GESTURE:
				sscanf(p, "%02X ", &info->gesture_enabled);
				log_error("%s fts_feature_enable: Gesture Enabled = %d\n", tag, info->gesture_enabled);
				break;
#endif

			case FEAT_GLOVE:
				sscanf(p, "%02X ", &info->glove_enabled);
				log_error("%s fts_feature_enable: Glove Enabled = %d\n", tag, info->glove_enabled);
				break;

			default:
				log_error("%s fts_feature_enable: Feature %02X not valid! ERROR %08X\n", tag, temp, ERROR_OP_NOT_ALLOW);
			}

			feature_feasibility = res;
		}
	}

	return count;
}

static ssize_t fts_feature_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	char buff[CMD_STR_LEN] = {0};
	int size = 6 * 2 + 1;
	u8 *all_strbuff = NULL;
	int count = 0, res;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	if (feature_feasibility >= OK)
		res = fts_mode_handler(info, 1);
	else {
		res = feature_feasibility;
		log_error("%s %s: Call before echo xx xx > feature_enable with a correct feature! ERROR %08X\n", tag,  __func__, res);
	}

	all_strbuff = (u8 *) kmalloc(size, GFP_KERNEL);

	if (all_strbuff != NULL) {
		memset(all_strbuff, 0, size);
		snprintf(buff, sizeof(buff), "%02X", 0xAA);
		strlcat(all_strbuff, buff, size);
		snprintf(buff, sizeof(buff), "%08X", res);
		strlcat(all_strbuff, buff, size);
		snprintf(buff, sizeof(buff), "%02X", 0xBB);
		strlcat(all_strbuff, buff, size);
		count = snprintf(buf, PAGE_SIZE, "%s\n", all_strbuff);
		kfree(all_strbuff);
	} else {
		log_error("%s fts_feature_enable_show: Unable to allocate all_strbuff! ERROR %08X\n", tag, ERROR_ALLOC);
	}

	feature_feasibility = ERROR_OP_NOT_ALLOW;
	return count;
}

#ifdef PHONE_GESTURE
static ssize_t fts_gesture_mask_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	char buff[CMD_STR_LEN] = {0};
	int size = 6 * 2 + 1;
	u8 *all_strbuff = NULL;
	int count = 0, res;

	if (mask[0] == 0) {
		res = ERROR_OP_NOT_ALLOW;
		log_error("%s %s: Call before echo enable/disable xx xx .... > gesture_mask with a correct number of parameters! ERROR %08X\n", tag, __func__, res);
	} else {
		if (mask[1] == FEAT_ENABLE || mask[1] == FEAT_DISABLE)
			res = updateGestureMask(&mask[2], mask[0], mask[1]);
		else
			res = ERROR_OP_NOT_ALLOW;

		if (res < OK) {
			log_error("%s fts_gesture_mask_store: ERROR %08X \n", tag, res);
		}
	}

	all_strbuff = (u8 *) kmalloc(size, GFP_KERNEL);

	if (all_strbuff != NULL) {
		memset(all_strbuff, 0, size);
		snprintf(buff, sizeof(buff), "%02X", 0xAA);
		strlcat(all_strbuff, buff, size);
		snprintf(buff, sizeof(buff), "%08X", res);
		strlcat(all_strbuff, buff, size);
		snprintf(buff, sizeof(buff), "%02X", 0xBB);
		strlcat(all_strbuff, buff, size);
		count = snprintf(buf, PAGE_SIZE, "%s\n", all_strbuff);
		kfree(all_strbuff);
	} else {
		log_error("%s fts_gesture_mask_show: Unable to allocate all_strbuff! ERROR %08X\n", tag, ERROR_ALLOC);
	}

	mask[0] = 0;
	return count;
}

static ssize_t fts_gesture_mask_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	char *p = (char *)buf;
	int n;
	unsigned int temp;

	if ((count + 1) / 3 > GESTURE_MASK_SIZE + 1) {
		log_error("%s fts_gesture_mask_store: Number of bytes of parameter wrong! %zu > (enable/disable + %d )\n", tag, (count + 1) / 3, GESTURE_MASK_SIZE);
		mask[0] = 0;
	} else {
		mask[0] = ((count + 1) / 3) - 1;

		for (n = 1; n <= (count + 1) / 3; n++) {
			sscanf(p, "%02X ", &temp);
			p += 3;
			mask[n] = (u8)temp;
			log_debug("%s mask[%d] = %02X\n", tag, n, mask[n]);
		}
	}

	return count;
}
#endif

static ssize_t stm_fts_cmd_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int n;
	char *p = (char *) buf;
	typeOfComand = (u32 *) kmalloc(8 * sizeof(u32), GFP_KERNEL);

	if (typeOfComand == NULL) {
		log_error("%s impossible to allocate typeOfComand!\n", tag);
		return 0;
	}

	memset(typeOfComand, 0, 8 * sizeof(u32));

	for (n = 0; n < (count + 1) / 3; n++) {
		sscanf(p, "%02X ", &typeOfComand[n]);
		p += 3;
		log_error("%s typeOfComand[%d] = %02X\n", tag, n, typeOfComand[n]);
	}

	numberParameters = n;
	log_error("%s Number of Parameters = %d\n", tag, numberParameters);
	return count;
}

static ssize_t stm_fts_cmd_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	char buff[CMD_STR_LEN] = {0};
	int res, j, doClean = 0, count = 0;
	int size = 6 * 2 + 1;
	u8 *all_strbuff = NULL;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);
	const char *limit_file_name = NULL;
	MutualSenseData compData;
	SelfSenseData comData;
	MutualSenseFrame frameMS;
	SelfSenseFrame frameSS;
	TestToDo todoDefault;

	todoDefault.MutualRaw = 1;
	todoDefault.MutualRawGap = 0;
	todoDefault.MutualCx1 = 0;
	todoDefault.MutualCx2 = 0;
	todoDefault.MutualCx2Adj = 0;
	todoDefault.MutualCxTotal = 1;
	todoDefault.MutualCxTotalAdj = 1;
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

	if (numberParameters >= 1 && typeOfComand != NULL) {
		res = fts_disableInterrupt();

		if (res < 0) {
			log_error("%s fts_disableInterrupt: ERROR %08X\n", tag, res);
			res = (res | ERROR_DISABLE_INTER);
			goto END;
		}
#ifdef CONFIG_DRM
		res = drm_unregister_client(&info->notifier);
#endif

		if (res < 0) {
			log_error("%s ERROR: unregister notifier failed!\n", tag);
			goto END;
		}

		switch (typeOfComand[0]) {
		case 0x01:
			res = production_test_ito();
			break;

		case 0x00:
			limit_file_name = fts_get_limit(info);
			if (ftsInfo.u32_mpPassFlag != INIT_MP) {
				log_error("%s MP Flag not set! %02x\n", tag, res);
				res = production_test_main(limit_file_name, 1, 1, &todoDefault, INIT_MP);
			} else {
				log_debug("%s MP Flag set! %02x\n", tag, res);
				res = production_test_main(limit_file_name, 1, 0, &todoDefault, INIT_MP);
			}
			break;
		case 0x02:
			res = fts_chip_autotune(info);
			break;

		case 0x12:
			log_error("%s Get 1 MS Key Frame\n", tag);
			res = getMSFrame2(MS_KEY, &frameMS);

			if (res < 0)
				log_error("%s Error while taking the MS Key frame... ERROR %02X\n", tag, res);
			else {
				log_debug("%s The frame size is %d words\n", tag, res);
				size = (res * (sizeof(short) * 2 + 1)) + 11;
				res = OK;
			}

			break;

		case 0x13:
			log_error("%s Get 1 MS Frame\n", tag);
			res = getMSFrame2(MS_TOUCH_ACTIVE, &frameMS);

			if (res < 0)
				log_error("%s Error while taking the MS frame... ERROR %02X\n", tag, res);
			else {
				log_debug("%s The frame size is %d words\n", tag, res);
				size = (res * (sizeof(short) * 2 + 1)) + 10;
				res = OK;
			}

			break;

		case 0x15:
			log_error("%s Get 1 SS Frame\n", tag);
			res = getSSFrame2(SS_TOUCH, &frameSS);

			if (res < OK)
				log_error("%s Error while taking the SS frame... ERROR %02X\n", tag, res);
			else {
				log_debug("%s The frame size is %d words\n", tag, res);
				size = (res * (sizeof(short) * 2 + 1)) + 10;
				res = OK;
			}

			break;

		case 0x14:
			log_error("%s Get MS Compensation Data\n", tag);
			res = readMutualSenseCompensationData(MS_TOUCH_ACTIVE, &compData);

			if (res < 0)
				log_error("%s Error reading MS compensation data ERROR %02X\n", tag, res);
			else {
				log_debug("%s MS Compensation Data Reading Finished!\n", tag);
				size = (compData.node_data_size * sizeof(u8)) * 3 + 1;
			}

			break;

		case 0x16:
			log_error("%s Get SS Compensation Data...\n", tag);
			res = readSelfSenseCompensationData(SS_TOUCH, &comData);

			if (res < 0)
				log_error("%s Error reading SS compensation data ERROR %02X\n", tag, res);
			else {
				log_debug("%s SS Compensation Data Reading Finished!\n", tag);
				size = ((comData.header.force_node + comData.header.sense_node) * 2 + 12) * sizeof(u8) * 2 + 1;
			}

			break;

		case 0x03:
			res = fts_system_reset();

			if (res >= OK)
				res = production_test_ms_raw(limit_file_name, 1, &todoDefault);

			break;

		case 0x04:
			res = fts_system_reset();

			if (res >= OK)
				res = production_test_ms_cx(limit_file_name, 1, &todoDefault);

			break;

		case 0x05:
			res = fts_system_reset();

			if (res >= OK)
				res = production_test_ss_raw(limit_file_name, 1, &todoDefault);

			break;

		case 0x06:
			res = fts_system_reset();

			if (res >= OK)
				res = production_test_ss_ix_cx(limit_file_name, 1, &todoDefault);

			break;

		case 0xF0:
		case 0xF1:
			doClean = (int)(typeOfComand[0] & 0x01);
			res = cleanUp(doClean);
			break;

		default:
			log_error("%s COMMAND NOT VALID!! Insert a proper value ...\n", tag);
			res = ERROR_OP_NOT_ALLOW;
			break;
		}

		if (typeOfComand[0] != 0xF0 || typeOfComand[0] != 0xF1) {
			doClean = fts_mode_handler(info, 0);
			doClean |= fts_enableInterrupt();
			if (doClean < OK)
				log_error("%s Cannot initialize the normal working mode of the device ERROR %08X\n", tag, doClean);
		}
	} else {
		log_error("%s NO COMMAND SPECIFIED!!! do: 'echo [cmd_code] [args] > stm_fts_cmd' before looking for result!\n", tag);
		res = ERROR_OP_NOT_ALLOW;
		if (typeOfComand != NULL) {
			kfree(typeOfComand);
			typeOfComand = NULL;
		}
		return count;
	}
#ifdef CONFIG_DRM
	if (drm_register_client(&info->notifier) < 0) {
		log_error("%s ERROR: register notifier failed!\n", tag);
	}
#endif
END:
	all_strbuff = (u8 *) kmalloc(size, GFP_KERNEL);
	memset(all_strbuff, 0, size);

	if (res >= OK) {
		switch (typeOfComand[0]) {
		case 0x12:
			snprintf(all_strbuff, size, "key_frame\n");
			for (j = 0; j < frameMS.node_data_size; j++) {
				if ((j + 1) % frameMS.header.sense_node)
					snprintf(buff, sizeof(buff), "%04d ", frameMS.node_data[j]);
				else
					snprintf(buff, sizeof(buff), "%04d\n", frameMS.node_data[j]);

				strlcat(all_strbuff, buff, size);
			}

			kfree(frameMS.node_data);
			break;

		case 0x13:
			snprintf(all_strbuff, size, "ms_frame\n");
			for (j = 0; j < frameMS.node_data_size; j++) {
				if ((j + 1) % frameMS.header.sense_node)
					snprintf(buff, sizeof(buff), "%04d ", frameMS.node_data[j]);
				else
					snprintf(buff, sizeof(buff), "%04d\n", frameMS.node_data[j]);

				strlcat(all_strbuff, buff, size);
			}

			kfree(frameMS.node_data);
			break;

		case 0x15:
			snprintf(all_strbuff, size, "ss_frame\n");
			for (j = 0; j < frameSS.header.force_node - 1; j++) {
				snprintf(buff, sizeof(buff), "%04d ", frameSS.force_data[j]);
				strlcat(all_strbuff, buff, size);
			}

			if (j == frameSS.header.force_node - 1) {
				snprintf(buff, sizeof(buff), "%04d\n", frameSS.force_data[j]);
				strlcat(all_strbuff, buff, size);
			}

			for (j = 0; j < frameSS.header.sense_node - 1; j++) {
				snprintf(buff, sizeof(buff), "%04d ", frameSS.sense_data[j]);
				strlcat(all_strbuff, buff, size);
			}

			if (j == frameSS.header.sense_node - 1) {
				snprintf(buff, sizeof(buff), "%04d\n", frameSS.sense_data[j]);
				strlcat(all_strbuff, buff, size);
			}

			kfree(frameSS.force_data);
			kfree(frameSS.sense_data);
			break;

		case 0x14:
			snprintf(buff, sizeof(buff), "%02X", (u8) compData.header.force_node);
			strlcat(all_strbuff, buff, size);
			snprintf(buff, sizeof(buff), "%02X", (u8) compData.header.sense_node);
			strlcat(all_strbuff, buff, size);
			snprintf(buff, sizeof(buff), "%02X", compData.cx1);
			strlcat(all_strbuff, buff, size);

			for (j = 0; j < compData.node_data_size; j++) {
				snprintf(buff, sizeof(buff), "%02X", *(compData.node_data + j));
				strlcat(all_strbuff, buff, size);
			}

			kfree(compData.node_data);
			break;

		case 0x16:
			snprintf(buff, sizeof(buff), "%02X", comData.header.force_node);
			strlcat(all_strbuff, buff, size);
			snprintf(buff, sizeof(buff), "%02X", comData.header.sense_node);
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
				snprintf(buff, sizeof(buff), "%02X", comData.ix2_fm[j]);
				strlcat(all_strbuff, buff, size);
			}

			for (j = 0; j < comData.header.sense_node; j++) {
				snprintf(buff, sizeof(buff), "%02X", comData.ix2_sn[j]);
				strlcat(all_strbuff, buff, size);
			}

			for (j = 0; j < comData.header.force_node; j++) {
				snprintf(buff, sizeof(buff), "%02X", comData.cx2_fm[j]);
				strlcat(all_strbuff, buff, size);
			}

			for (j = 0; j < comData.header.sense_node; j++) {
				snprintf(buff, sizeof(buff), "%02X", comData.cx2_sn[j]);
				strlcat(all_strbuff, buff, size);
			}

			kfree(comData.ix2_fm);
			kfree(comData.ix2_sn);
			kfree(comData.cx2_fm);
			kfree(comData.cx2_sn);
			break;

		default:
			snprintf(buff, sizeof(buff), "%02X", 0xAA);
			strlcat(all_strbuff, buff, size);
			snprintf(buff, sizeof(buff), "%08X", res);
			strlcat(all_strbuff, buff, size);
			snprintf(buff, sizeof(buff), "%02X", 0xBB);
			strlcat(all_strbuff, buff, size);
			break;
		}
	} else {
		snprintf(buff, sizeof(buff), "%02X", 0xAA);
		strlcat(all_strbuff, buff, size);
		snprintf(buff, sizeof(buff), "%08X", res);
		strlcat(all_strbuff, buff, size);
		snprintf(buff, sizeof(buff), "%02X", 0xBB);
		strlcat(all_strbuff, buff, size);
	}

	count = snprintf(buf, PAGE_SIZE, "%s\n", all_strbuff);
	numberParameters = 0;
	kfree(all_strbuff);
	if (typeOfComand != NULL) {
		kfree(typeOfComand);
		typeOfComand = NULL;
	}
	return count;
}

static ssize_t fts_panel_color_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct fts_ts_info *info = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%c\n",  info->lockdown_info[2]);
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
	ret = fts_get_lockdown_info(info->lockdown_info);

	if (ret != OK) {
		log_error("%s get lockdown info error\n", tag);
		return 0;
	}

	return snprintf(buf, PAGE_SIZE, "0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x\n",
			 info->lockdown_info[0], info->lockdown_info[1], info->lockdown_info[2], info->lockdown_info[3],
			 info->lockdown_info[4], info->lockdown_info[5], info->lockdown_info[6], info->lockdown_info[7]);
}

static ssize_t fts_strength_frame_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	char *p = (char *)buf;

	typeOfComand = (u32 *) kzalloc(8 * sizeof(u32), GFP_KERNEL);

	if (typeOfComand == NULL) {
		log_error("%s impossible to allocate typeOfComand!\n", tag);
		return 0;
	}
	sscanf(p, "%d", &typeOfComand[0]);
	log_error("%s %s: Type of Strength Frame selected: %d\n", tag, __func__, typeOfComand[0]);
	return count;
}

static ssize_t fts_selftest_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int res = 0, i = 0, count = 0, force_node = 0, sense_node = 0, pos = 0, last_pos = 0;
	MutualSenseFrame frameMS;
	char buff[20];
	res = fts_disableInterrupt();
	if (res < OK)
		goto END;

	res = getMSFrame2(MS_TOUCH_ACTIVE, &frameMS);
	if (res >= OK) {
		sense_node = frameMS.header.sense_node;
		force_node = frameMS.header.force_node;
	}
	for (i = 0; i < EXTERNAL_RELEASE_INFO_SIZE; i++) {
		if (i == 0) {
			pos += snprintf(buff + last_pos, PAGE_SIZE, "0x%02x", ftsInfo.u8_extReleaseInfo[i]);
			last_pos = pos;
		} else {
			pos += snprintf(buff + last_pos, PAGE_SIZE, "%02x", ftsInfo.u8_extReleaseInfo[i]);
			last_pos = pos;
		}
	}
	count = snprintf(buf, PAGE_SIZE, "Device address:,0x49\nChip Id:,0x%04x\nFw version:,0x%04x\nConfig version:,0x%04x\nChip serial number:,%s\nForce lines count:,%02d\nSense lines count:,%02d\n\n",
			ftsInfo.u16_ftsdId, ftsInfo.u16_fwVer, ftsInfo.u16_cfgId, buff, force_node, sense_node);
END:
	fts_enableInterrupt();
	return count;

}
static ssize_t fts_ms_raw_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int res = 0, count = 0, j = 0, sense_node = 0, force_node = 0, pos = 0, last_pos = 0;
	char *all_strbuff = NULL;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);
	MutualSenseFrame frameMS;

	res = fts_disableInterrupt();
	if (res < OK)
		goto END;
	all_strbuff = vmalloc(PAGE_SIZE);
	if (!all_strbuff) {
		log_error("%s %s alloc all_strbuff fail\n", tag, __func__);
		goto END;
	} else
		memset(all_strbuff, 0, PAGE_SIZE);
	res = getMSFrame2(MS_TOUCH_ACTIVE, &frameMS);
	fts_mode_handler(info, 1);
	sense_node = frameMS.header.sense_node;
	force_node = frameMS.header.force_node;
	pos += snprintf(all_strbuff + last_pos, PAGE_SIZE, "MsTouchRaw,%2d,%2d\n ,", force_node, sense_node);
	last_pos = pos;
	if (res >= OK) {
		/*
		 * print ms raw
		 * */
		for (j = 0; j < sense_node; j++)
			if ((j + 1) % sense_node) {
				pos += snprintf(all_strbuff + last_pos, PAGE_SIZE, "C%02d,", j);
				last_pos = pos;
			} else {
				pos += snprintf(all_strbuff + last_pos, PAGE_SIZE, "C%02d\nR00,", j);
				last_pos = pos;
			}
		for (j = 0; j < sense_node * force_node; j++) {
			if ((j + 1) % sense_node) {
				pos += snprintf(all_strbuff + last_pos, PAGE_SIZE, "%4d,", frameMS.node_data[j]);
				last_pos = pos;
			} else {
				if ((j + 1) / sense_node != force_node)
					pos += snprintf(all_strbuff + last_pos, PAGE_SIZE, "%4d\nR%02d,", frameMS.node_data[j], (j + 1) / sense_node);
				else
					pos += snprintf(all_strbuff + last_pos, PAGE_SIZE, "%4d\n", frameMS.node_data[j]);
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
static ssize_t fts_ms_cx_total_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int res = 0, pos = 0, last_pos = 0, count = 0, j = 0, sense_node = 0, force_node = 0;
	char *all_strbuff = NULL;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);
	MutualSenseData msCompData;
	u16 *total_cx = NULL;

	res = fts_disableInterrupt();
	if (res < OK)
		goto END;
	all_strbuff = vmalloc(PAGE_SIZE);
	if (!all_strbuff) {
		log_error("%s %s alloc all_strbuff fail\n", tag, __func__);
		goto END;
	} else
		memset(all_strbuff, 0, PAGE_SIZE);
	fts_mode_handler(info, 1);
	res = readMutualSenseCompensationData(MS_TOUCH_ACTIVE, &msCompData);

	if (res >= OK) {
		res = computeTotal(msCompData.node_data, msCompData.cx1, msCompData.header.force_node, msCompData.header.sense_node, CX1_WEIGHT, CX2_WEIGHT, &total_cx);
		sense_node = msCompData.header.sense_node;
		force_node = msCompData.header.force_node;
	}
	if (res >= OK) {
		/*
		 * print ms cx total
		 * */
		pos += snprintf(all_strbuff + last_pos, PAGE_SIZE, "MsTouchTotalCx,%2d,%2d\n ,", force_node, sense_node);
		last_pos = pos;
		for (j = 0; j < sense_node; j++)
			if ((j + 1) % sense_node) {
				pos += snprintf(all_strbuff + last_pos, PAGE_SIZE, "C%02d,", j);
				last_pos = pos;
			} else {
				pos += snprintf(all_strbuff + last_pos, PAGE_SIZE, "C%02d\nR00,", j);
				last_pos = pos;
			}
		for (j = 0; j < sense_node * force_node; j++) {
			if ((j + 1) % sense_node) {
				pos += snprintf(all_strbuff + last_pos, PAGE_SIZE, "%4d,", total_cx[j]);
				last_pos = pos;
			} else {
				if ((j + 1) / sense_node != force_node)
					pos += snprintf(all_strbuff + last_pos, PAGE_SIZE, "%4d\nR%02d,", total_cx[j], (j + 1) / sense_node);
				else
					pos += snprintf(all_strbuff + last_pos, PAGE_SIZE, "%4d\n", total_cx[j]);
				last_pos = pos;
			}
		}
		if (msCompData.node_data) {
			kfree(msCompData.node_data);
			msCompData.node_data = NULL;
		}
		if (total_cx) {
			kfree(total_cx);
			total_cx = NULL;
		}
	}

	count = snprintf(buf, PAGE_SIZE, "%s\n", all_strbuff);
	vfree(all_strbuff);
END:
	fts_enableInterrupt();
	return count;
}
static ssize_t fts_ms_cx_total_adjhor_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int res = 0, pos = 0, last_pos = 0, count = 0, j = 0, sense_node = 0, force_node = 0;
	char *all_strbuff = NULL;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);
	MutualSenseData msCompData;
	u16 *total_cx = NULL;
	u16 *total_adjhor = NULL;

	res = fts_disableInterrupt();
	if (res < OK)
		goto END;
	all_strbuff = vmalloc(PAGE_SIZE);
	if (!all_strbuff) {
		log_error("%s %s alloc all_strbuff fail\n", tag, __func__);
		goto END;
	} else
		memset(all_strbuff, 0, PAGE_SIZE);
	fts_mode_handler(info, 1);
	res = readMutualSenseCompensationData(MS_TOUCH_ACTIVE, &msCompData);

	if (res >= OK) {
		res = computeTotal(msCompData.node_data, msCompData.cx1, msCompData.header.force_node, msCompData.header.sense_node, CX1_WEIGHT, CX2_WEIGHT, &total_cx);
		sense_node = msCompData.header.sense_node;
		force_node = msCompData.header.force_node;
	}
	if (res >= OK) {
		res = computeAdjHorizTotal(total_cx, msCompData.header.force_node, msCompData.header.sense_node, &total_adjhor);
		if (res >= OK) {
			/*
			 * PRINT mx cx adjhor
			 * */
			pos += snprintf(all_strbuff + last_pos, PAGE_SIZE, "MsTouchTotalCxAdjHorizontal,%2d,%2d\n ,", force_node, sense_node - 1);
			last_pos = pos;
			for (j = 0; j < (sense_node - 1); j++)
				if ((j + 1) % (sense_node - 1)) {
					pos += snprintf(all_strbuff + last_pos, PAGE_SIZE, "C%02d,", j);
					last_pos = pos;
				} else {
					pos += snprintf(all_strbuff + last_pos, PAGE_SIZE, "C%02d\nR00,", j);
					last_pos = pos;
				}
			for (j = 0; j < (sense_node - 1) * force_node; j++) {
				if ((j + 1) % (sense_node - 1)) {
					pos += snprintf(all_strbuff + last_pos, PAGE_SIZE, "%4d,", total_adjhor[j]);
					last_pos = pos;
				} else {
					if ((j + 1) / (sense_node - 1) != force_node)
						pos += snprintf(all_strbuff + last_pos, PAGE_SIZE, "%4d\nR%02d,", total_adjhor[j], (j + 1) / (sense_node - 1));
					else
						pos += snprintf(all_strbuff + last_pos, PAGE_SIZE, "%4d\n", total_adjhor[j]);
					last_pos = pos;
				}
			}
		}
		if (msCompData.node_data) {
			kfree(msCompData.node_data);
			msCompData.node_data = NULL;
		}
		if (total_cx) {
			kfree(total_cx);
			total_cx = NULL;
		}
		if (total_adjhor) {
			kfree(total_adjhor);
			total_adjhor = NULL;
		}
	}
	count = snprintf(buf, PAGE_SIZE, "%s\n", all_strbuff);
	vfree(all_strbuff);
END:
	fts_enableInterrupt();
	return count;
}
static ssize_t fts_ms_cx_total_adjvert_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int res = 0, pos = 0, last_pos = 0, count = 0, j = 0, sense_node = 0, force_node = 0;
	char *all_strbuff = NULL;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);
	MutualSenseData msCompData;
	u16 *total_cx = NULL;
	u16 *total_adjvert = NULL;

	res = fts_disableInterrupt();
	if (res < OK)
		goto END;
	all_strbuff = vmalloc(PAGE_SIZE);
	if (!all_strbuff) {
		log_error("%s %s alloc all_strbuff fail\n", tag, __func__);
		goto END;
	} else
		memset(all_strbuff, 0, PAGE_SIZE);
	fts_mode_handler(info, 1);
	res = readMutualSenseCompensationData(MS_TOUCH_ACTIVE, &msCompData);

	if (res >= OK) {
		res = computeTotal(msCompData.node_data, msCompData.cx1, msCompData.header.force_node, msCompData.header.sense_node, CX1_WEIGHT, CX2_WEIGHT, &total_cx);
		sense_node = msCompData.header.sense_node;
		force_node = msCompData.header.force_node;
	}
	if (res >= OK) {
		res = computeAdjVertTotal(total_cx, msCompData.header.force_node, msCompData.header.sense_node, &total_adjvert);
		if (res >= OK) {
			/*
			 * PRINT ms cx adjvert
			 * */
			pos += snprintf(all_strbuff + last_pos, PAGE_SIZE, "MsTouchTotalCxAdjVertical,%2d,%2d\n ,", force_node - 1, sense_node);
			last_pos = pos;
			for (j = 0; j < sense_node; j++)
				if ((j + 1) % sense_node) {
					pos += snprintf(all_strbuff + last_pos, PAGE_SIZE, "C%02d,", j);
					last_pos = pos;
				} else {
					pos += snprintf(all_strbuff + last_pos, PAGE_SIZE, "C%02d\nR00,", j);
					last_pos = pos;
				}
			for (j = 0; j < sense_node * (force_node - 1); j++) {
				if ((j + 1) % sense_node) {
					pos += snprintf(all_strbuff + last_pos, PAGE_SIZE, "%4d,", total_adjvert[j]);
					last_pos = pos;
				} else {
					if ((j + 1) / sense_node != (force_node - 1))
						pos += snprintf(all_strbuff + last_pos, PAGE_SIZE, "%4d\nR%02d,", total_adjvert[j], (j + 1) / sense_node);
					else
						pos += snprintf(all_strbuff + last_pos, PAGE_SIZE, "%4d\n", total_adjvert[j]);
					last_pos = pos;
				}
			}
		}
		if (msCompData.node_data) {
			kfree(msCompData.node_data);
			msCompData.node_data = NULL;
		}
		if (total_cx) {
			kfree(total_cx);
			total_cx = NULL;
		}
		if (total_adjvert) {
			kfree(total_adjvert);
			total_adjvert = NULL;
		}
	}
	count = snprintf(buf, PAGE_SIZE, "%s\n", all_strbuff);
	vfree(all_strbuff);
END:
	fts_enableInterrupt();
	return count;
}
static ssize_t fts_ss_ix_total_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int res = 0, pos = 0, last_pos = 0, count = 0, j = 0, sense_node = 0, force_node = 0, trows = 0, tcolumns = 0;
	char *all_strbuff = NULL;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);
	SelfSenseData ssCompData;
	int *ix1_w = NULL;
	int *ix2_w = NULL;
	u16 *total_ix_f = NULL;
	u16 *total_ix_s = NULL;

	res = fts_disableInterrupt();
	if (res < OK)
		goto END;
	all_strbuff = vmalloc(PAGE_SIZE);
	if (!all_strbuff) {
		log_error("%s %s alloc all_strbuff fail\n", tag, __func__);
		goto END;
	} else
		memset(all_strbuff, 0, PAGE_SIZE);
	fts_mode_handler(info, 1);
	res = parseProductionTestLimits(LIMITS_FILE, SS_IX1_FORCE_W, &ix1_w, &trows, &tcolumns);

	if (res < 0 || (trows != 1 || tcolumns != 1)) {
		log_error("%s production_test_data: parseProductionTestLimits SS_IX1_FORCE_W failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_IX_CX);
		goto END;
	}

	res = parseProductionTestLimits(LIMITS_FILE, SS_IX2_FORCE_W, &ix2_w, &trows, &tcolumns);

	if (res < 0 || (trows != 1 || tcolumns != 1)) {
		log_error("%s production_test_data: parseProductionTestLimits SS_IX1_FORCE_W failed... ERROR %02X\n", tag, ERROR_PROD_TEST_DATA_SS_IX_CX);
		goto END;
	}
	res = readSelfSenseCompensationData(SS_TOUCH, &ssCompData);
	if (res >= OK) {
		sense_node = ssCompData.header.sense_node;
		force_node = ssCompData.header.force_node;
		res = computeTotal(ssCompData.ix2_fm, ssCompData.f_ix1, ssCompData.header.force_node, 1, *ix1_w, *ix2_w, &total_ix_f);
		/*
		 * print force ix total
		 * */
		if (res >= 0) {
			pos += snprintf(all_strbuff + last_pos, PAGE_SIZE, "SsTouchForceTotalIx,%2d,1\n ,C00\n", force_node);
			last_pos = pos;
			for (j = 0; j < force_node; j++) {
				pos += snprintf(all_strbuff + last_pos, PAGE_SIZE, "R%02d,%4d\n", j, total_ix_f[j]);
				last_pos = pos;
			}
			}
		res = computeTotal(ssCompData.ix2_sn, ssCompData.s_ix1, 1, ssCompData.header.sense_node, *ix1_w, *ix2_w, &total_ix_s);
		if (res >= OK) {
			/*
			 * print sense ix total
			 * */
			pos += snprintf(all_strbuff + last_pos, PAGE_SIZE, "\nSsTouchSenseTotalIx,1,%2d\n ,", sense_node);
			last_pos = pos;
			for (j = 0; j < sense_node - 1; j++) {
				pos += snprintf(all_strbuff + last_pos, PAGE_SIZE, "C%02d,", j);
				last_pos = pos;
			}
			pos += snprintf(all_strbuff + last_pos, PAGE_SIZE, "C%02d\nR00,", sense_node - 1);
			last_pos = pos;
			for (j = 0; j < sense_node - 1; j++) {
				pos += snprintf(all_strbuff + last_pos, PAGE_SIZE, "%4d,", total_ix_s[j]);
				last_pos = pos;
			}
			pos += snprintf(all_strbuff + last_pos, PAGE_SIZE, "%4d\n", total_ix_s[sense_node - 1]);
			last_pos = pos;
			}
			if (ssCompData.ix2_fm != NULL) {
				kfree(ssCompData.ix2_fm);
				ssCompData.ix2_fm = NULL;
			}

			if (ssCompData.ix2_sn != NULL) {
				kfree(ssCompData.ix2_sn);
				ssCompData.ix2_sn = NULL;
			}

			if (ssCompData.cx2_fm != NULL) {
				kfree(ssCompData.cx2_fm);
				ssCompData.cx2_fm = NULL;
			}

			if (ssCompData.cx2_sn != NULL) {
				kfree(ssCompData.cx2_sn);
				ssCompData.cx2_sn = NULL;
			}
			if (total_ix_f) {
				kfree(total_ix_f);
				total_ix_f = NULL;
			}
			if (total_ix_s) {
				kfree(total_ix_s);
				total_ix_s = NULL;
			}
	}
	count = snprintf(buf, PAGE_SIZE, "%s\n", all_strbuff);
	vfree(all_strbuff);
END:
	fts_enableInterrupt();
	return count;
}

static ssize_t fts_strength_frame_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	MutualSenseFrame frame;
	int res = 0, count = 0, j = 0, size = 0, type = 0;
	char *all_strbuff = NULL;
	char buff[CMD_STR_LEN] = {0};
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);
	frame.node_data = NULL;
	res = fts_disableInterrupt();

	if (res < OK)
		goto END;
	if (typeOfComand == NULL) {
		log_error("%s %sdo not echo to command\n", tag, __func__);
		return count;
	}
/*
	res = senseOn();

#ifdef PHONE_KEY
	res = keyOn();
#endif

	if (res < OK) {
		log_error("%s %s: could not start scanning! ERROR %08X\n", tag, __func__, res);
		goto END;
	}

	mdelay(WAIT_FOR_FRESH_FRAMES);
	res = senseOff();

#ifdef PHONE_KEY
	res = keyOff();
#endif
	if (res < OK) {
		log_error("%s %s: could not finish scanning! ERROR %08X \n", tag, __func__, res);
		goto END;
	}

	mdelay(WAIT_AFTER_SENSEOFF);
	flushFIFO();
	*/
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
		log_error("%s %s: Strength type %d not valid! ERROR %08X\n", tag, __func__, typeOfComand[0], ERROR_OP_NOT_ALLOW);
		res = ERROR_OP_NOT_ALLOW;
		goto END;
	}
	res = getMSFrame(type, &frame, 0);

	if (res < OK) {
		log_error("%s %s: could not get the frame! ERROR %08X \n", tag, __func__, res);
		goto END;
	} else {
		if (typeOfComand[0] == 1)
			size = (res * 5) + 11;
		else if (typeOfComand[0] == 2)
			size = (res * 5) + 12;
		log_debug("%s The frame size is %d words\n", tag, res);
		res = OK;
	}

END:
	/*
	flushFIFO();
	*/
	fts_mode_handler(info, 1);
	all_strbuff = (char *)kmalloc(size * sizeof(char), GFP_KERNEL);

	if (all_strbuff != NULL) {
		memset(all_strbuff, 0, size);
		if (typeOfComand[0] == 1)
			snprintf(all_strbuff, size, "ms_differ\n");
		else if (typeOfComand[0] == 2)
			snprintf(all_strbuff, size, "key_differ\n");
		if (res >= OK) {
			for (j = 0; j < frame.node_data_size; j++) {
				if ((j + 1) % frame.header.sense_node)
					snprintf(buff, sizeof(buff), "%4d ", frame.node_data[j]);
				else
					snprintf(buff, sizeof(buff), "%4d\n", frame.node_data[j]);

				strlcat(all_strbuff, buff, size);
			}

			kfree(frame.node_data);
		}

		count = snprintf(buf, PAGE_SIZE, "%s\n", all_strbuff);
		kfree(all_strbuff);
	} else {
		log_error("%s %s: Unable to allocate all_strbuff! ERROR %08X\n", tag, __func__, ERROR_ALLOC);
	}

	fts_enableInterrupt();
	if (typeOfComand != NULL) {
		kfree(typeOfComand);
		typeOfComand = NULL;
	}
	return count;
}

#ifdef CONFIG_FTS_TOUCH_COUNT_DUMP
static ssize_t fts_touch_suspend_notify_show(struct device *dev, struct device_attribute *attr,
				   char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", fts_info->sensor_sleep);
}
#endif

static DEVICE_ATTR(fwupdate, (S_IRUGO | S_IWUSR | S_IWGRP), fts_fwupdate_show, fts_fwupdate_store);
static DEVICE_ATTR(appid, (S_IRUGO), fts_sysfs_config_id_show, NULL);
static DEVICE_ATTR(stm_fts_cmd, (S_IRUGO | S_IWUSR | S_IWGRP), stm_fts_cmd_show, stm_fts_cmd_store);
static DEVICE_ATTR(feature_enable, (S_IRUGO | S_IWUSR | S_IWGRP), fts_feature_enable_show, fts_feature_enable_store);
#ifdef PHONE_GESTURE
static DEVICE_ATTR(gesture_mask, (S_IRUGO | S_IWUSR | S_IWGRP), fts_gesture_mask_show, fts_gesture_mask_store);
#endif
#ifdef EDGEHOVER_FOR_VOLUME
static DEVICE_ATTR(edge_value, (S_IRUGO | S_IWUSR | S_IWGRP), fts_edge_value_show, fts_edge_value_store);
#endif
static DEVICE_ATTR(panel_vendor, (S_IRUGO), fts_panel_vendor_show, NULL);
static DEVICE_ATTR(panel_color, (S_IRUGO), fts_panel_color_show, NULL);
static DEVICE_ATTR(lockdown_info, (S_IRUGO), fts_lockdown_info_show, NULL);
static DEVICE_ATTR(strength_frame, (S_IRUGO | S_IWUSR | S_IWGRP), fts_strength_frame_show, fts_strength_frame_store);
static DEVICE_ATTR(selftest_info, (S_IRUGO), fts_selftest_info_show, NULL);
static DEVICE_ATTR(ms_raw, (S_IRUGO), fts_ms_raw_show, NULL);
static DEVICE_ATTR(ms_cx_total, (S_IRUGO), fts_ms_cx_total_show, NULL);
static DEVICE_ATTR(ms_cx_adjhor, (S_IRUGO), fts_ms_cx_total_adjhor_show, NULL);
static DEVICE_ATTR(ms_cx_adjvert, (S_IRUGO), fts_ms_cx_total_adjvert_show, NULL);
static DEVICE_ATTR(ss_ix_total, (S_IRUGO), fts_ss_ix_total_show, NULL);
static struct attribute *fts_attr_group[] = {
	&dev_attr_fwupdate.attr,
	&dev_attr_appid.attr,
	&dev_attr_stm_fts_cmd.attr,
	&dev_attr_feature_enable.attr,
#ifdef PHONE_GESTURE
	&dev_attr_gesture_mask.attr,
#endif
#ifdef EDGEHOVER_FOR_VOLUME
	&dev_attr_edge_value.attr,
#endif
	&dev_attr_panel_vendor.attr,
	&dev_attr_panel_color.attr,
	&dev_attr_lockdown_info.attr,
	&dev_attr_strength_frame.attr,
	&dev_attr_selftest_info.attr,
	&dev_attr_ms_raw.attr,
	&dev_attr_ms_cx_total.attr,
	&dev_attr_ms_cx_adjhor.attr,
	&dev_attr_ms_cx_adjvert.attr,
	&dev_attr_ss_ix_total.attr,
	NULL,
};

#ifdef CONFIG_FTS_TOUCH_COUNT_DUMP
static DEVICE_ATTR(touch_suspend_notify, (S_IRUGO | S_IRGRP), fts_touch_suspend_notify_show, NULL);
#endif

static int fts_command(struct fts_ts_info *info, unsigned char cmd)
{
	unsigned char regAdd;
	int ret = 0;
	regAdd = cmd;
	ret = fts_writeCmd(&regAdd, sizeof(regAdd));
	log_debug("%s Issued command 0x%02x, return value %08X\n", tag, cmd, ret);
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


static inline unsigned char *fts_next_event(unsigned char *evt)
{
	evt += FIFO_EVENT_SIZE;
	return (evt[-1] & 0x1F) ? evt : NULL;
}

static unsigned char *fts_nop_event_handler(struct fts_ts_info *info, unsigned char *event)
{
	log_debug("%s %s Doing nothing for event = %02X %02X %02X %02X %02X %02X %02X %02X\n", tag, __func__, event[0], event[1], event[2], event[3], event[4], event[5], event[6], event[7]);
	return fts_next_event(event);
}

#if defined(CONFIG_INPUT_PRESS_NEXTINPUT) || defined(CONFIG_INPUT_PRESS_NDT)
static int fts_infod;
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
#endif
bool finger_report_flag = false;
static unsigned char *fts_enter_pointer_event_handler(struct fts_ts_info *info, unsigned char *event)
{
	unsigned char touchId, touchcount;
	int x, y, z;
#ifndef CONFIG_INPUT_PRESS_NDT
	if (!info->resume_bit)
		goto no_report;
#endif

	touchId = event[1] & 0x0F;
	touchcount = (event[1] & 0xF0) >> 4;
	__set_bit(touchId, &info->touch_id);
	x = (event[2] << 4) | (event[4] & 0xF0) >> 4;
	y = (event[3] << 4) | (event[4] & 0x0F);
#ifdef CONFIG_INPUT_PRESS_NDT
	z = ndt_get_pressure();
#else
	z = (event[5] & 0x3F);
#endif
	if (x == X_AXIS_MAX)
		x--;

	if (y == Y_AXIS_MAX)
		y--;

#if defined(CONFIG_INPUT_PRESS_NEXTINPUT) || defined(CONFIG_INPUT_PRESS_NDT)
	if (fts_is_in_fodarea(x, y)) {
		if (!finger_report_flag) {
			log_error("%s %s: finger down in the fod area\n", tag, __func__);
			finger_report_flag = true;
		}
		input_report_key(info->input_dev, KEY_INFO, 1);
		input_sync(info->input_dev);
		fts_infod |= BIT(touchId);
	}
	else
		fts_infod &= ~BIT(touchId);
	if (!info->resume_bit && !aod_mode)
		aod_mode = true;
#endif
	input_mt_slot(info->input_dev, touchId);
	input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 1);
	log_debug("%s  %s : TouchID = %d,Touchcount = %d\n", tag, __func__, touchId, touchcount);

	if (touchcount == 1) {
		input_report_key(info->input_dev, BTN_TOUCH, 1);
		input_report_key(info->input_dev, BTN_TOOL_FINGER, 1);
	}
#ifdef CONFIG_INPUT_PRESS_NDT
	if (!aod_mode) {
		input_report_abs(info->input_dev, ABS_MT_POSITION_X, x);
		input_report_abs(info->input_dev, ABS_MT_POSITION_Y, y);
		input_report_abs(info->input_dev, ABS_MT_TOUCH_MAJOR, z);
		input_report_abs(info->input_dev, ABS_MT_TOUCH_MINOR, z);
	} else {
		input_report_abs(info->input_dev, ABS_MT_POSITION_X, 0);
		input_report_abs(info->input_dev, ABS_MT_POSITION_Y, 0);
		input_report_abs(info->input_dev, ABS_MT_TOUCH_MAJOR, 0);
		input_report_abs(info->input_dev, ABS_MT_TOUCH_MINOR, 0);
	}
#else
		input_report_abs(info->input_dev, ABS_MT_POSITION_X, x);
		input_report_abs(info->input_dev, ABS_MT_POSITION_Y, y);
		input_report_abs(info->input_dev, ABS_MT_TOUCH_MAJOR, z);
		input_report_abs(info->input_dev, ABS_MT_TOUCH_MINOR, z);
#endif

	input_report_abs(info->input_dev, ABS_MT_PRESSURE, z);
	log_debug("%s  %s :  Event 0x%02x - ID[%d], (x, y, z) = (%3d, %3d, %3d)\n", tag, __func__, *event, touchId, x, y, z);
#ifndef CONFIG_INPUT_PRESS_NDT
no_report:
#endif
	return fts_next_event(event);
}

static unsigned char *fts_leave_pointer_event_handler(struct fts_ts_info *info, unsigned char *event)
{
	unsigned char touchId, touchcount;
	int x;
#if defined(CONFIG_INPUT_PRESS_NEXTINPUT) || defined(CONFIG_INPUT_PRESS_NDT)
	int y;
#endif

	touchId = event[1] & 0x0F;
	touchcount = (event[1] & 0xF0) >> 4;
	__clear_bit(touchId, &info->touch_id);
	x = (event[2] << 4) | (event[4] & 0xF0) >> 4;
#if defined(CONFIG_INPUT_PRESS_NEXTINPUT) || defined(CONFIG_INPUT_PRESS_NDT)
	y = (event[3] << 4) | (event[4] & 0x0F);
#endif
	input_mt_slot(info->input_dev, touchId);
	input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 0);
	log_debug("%s  %s : TouchID = %d, Touchcount = %d\n", tag, __func__, touchId, touchcount);

	if (touchcount == 0) {
		input_report_key(info->input_dev, BTN_TOUCH, 0);
		input_report_key(info->input_dev, BTN_TOOL_FINGER, 0);
	}

	input_report_abs(info->input_dev, ABS_MT_TRACKING_ID, -1);
#ifdef EDGEHOVER_FOR_VOLUME
	if ((x > 80) && (x < 1360)) {
		if ((fts_info->volume_type == INPUT_EVENT_DOUBLE_TAP_FOR_VOLUME) && !fts_info->judgment_mode) {
			fts_info->judgment_mode = true;
			if (fts_info->volume_flag) {
				input_report_key(info->input_dev, KEY_VOLUMEDOWN + fts_info->volume_flag - 1, 0);
				fts_info->volume_flag = 0;
			}
		}
		if (fts_info->volume_type == INPUT_EVENT_SLIDE_FOR_VOLUME) {
			fts_info->m_slide_thr = fts_info->slide_thr_start;
			fts_info->m_samp_interval = fts_info->samp_interval_start;
			fts_info->m_sampdura = 0;
			fts_clear_point(info);
		}
		if (((fts_info->volume_type == INPUT_EVENT_LONG_SINGLE_TAP_FOR_VOLUME) || (fts_info->volume_type == INPUT_EVENT_SINGLE_TAP_FOR_VOLUME)) && !fts_info->judgment_mode) {
			fts_info->judgment_mode = true;
			if (fts_info->volume_flag) {
				input_report_key(info->input_dev, KEY_VOLUMEDOWN + fts_info->volume_flag - 1, 0);
				fts_info->volume_flag = 0;
			}
		}
	}
#endif
	log_debug("%s  %s : Event 0x%02x - release ID[%d]\n", tag, __func__, event[0], touchId);
#if defined(CONFIG_INPUT_PRESS_NEXTINPUT) || defined(CONFIG_INPUT_PRESS_NDT)
	fts_infod &= ~BIT(touchId);
	if (fts_info->resume_bit && aod_mode)
		aod_mode = false;
	input_report_key(info->input_dev, KEY_INFO, 0);
	input_sync(info->input_dev);
	finger_report_flag = false;
#endif
	return fts_next_event(event);
}

#define fts_motion_pointer_event_handler fts_enter_pointer_event_handler

#ifdef EDGEHOVER_FOR_VOLUME
static bool fts_is_volume(int x_pos, int y_pos)
{
	if (x_pos == fts_info->bdata->x_base) {
		if ((y_pos < (fts_info->bdata->y_base + fts_info->bdata->y_offset)) && (y_pos >= fts_info->bdata->y_base)) {
			fts_info->volume_flag = 2;
			return true;
		} else if ((y_pos >= (fts_info->bdata->y_base + fts_info->bdata->y_offset + fts_info->bdata->y_skip)) && (y_pos < (fts_info->bdata->y_base + fts_info->bdata->y_offset * 2 + fts_info->bdata->y_skip))) {
			fts_info->volume_flag = 1;
			return true;
		} else
			return false;
	} else
		return false;
}

static void fts_add_point(struct fts_ts_info *info, int current_y)
{
	if (info->point_index < 2) {
		info->point[info->point_index] = current_y;
		info->point_index++;
	}
}

static void fts_clear_point(struct fts_ts_info *info)
{
	int i;
	for (i = 0; i < 2; i++)
		info->point[i] = 0;
	info->point_index = 0;
}

static int fts_point_size(struct fts_ts_info *info)
{
	int i, size = 0;
	for (i = 0; i < 2; i++)
		if (info->point[i] != 0)
			size++;
	return size;
}

static int fts_abs(int value)
{
	return value > 0 ? value : (0 - value);
}

static void fts_change_slide_to_volume(struct fts_ts_info *info)
{
	int volumechange, i;
	struct timespec ts;

	volumechange = (info->point[0] - info->point[1]) / fts_info->m_slide_thr;
	if (volumechange) {
		if (fts_info->m_slide_thr != fts_info->slide_thr) {
			fts_info->m_slide_thr = fts_info->slide_thr;
			fts_info->m_samp_interval = fts_info->samp_interval;
		}
		getnstimeofday(&ts);
		fts_info->pre_judgment_time = timespec_to_ns(&ts);
	}
	for (i = 0; i < fts_abs(volumechange); i++)
		fts_input_report_key(info, volumechange > 0 ? KEY_VOLUMEUP : KEY_VOLUMEDOWN);
}

static unsigned char *fts_edge_enter_pointer_event_handler(struct fts_ts_info *info, unsigned char *event)
{
	int x, y;
	struct timespec ts;
	long long adjust_time_ns;


	if (!info->bdata->side_volume)
		goto no_report;

	x = (event[2] << 4) | (event[4] & 0xF0) >> 4;
	y = (event[3] << 4) | (event[4] & 0x0F);
	switch (fts_info->volume_type) {
	case INPUT_EVENT_DOUBLE_TAP_FOR_VOLUME:
		if (fts_is_volume(x, y)) {
			getnstimeofday(&ts);
			adjust_time_ns = timespec_to_ns(&ts);
			if ((adjust_time_ns - fts_info->pre_judgment_time) > 3000000000) {
				log_error("%s, don't touch over 3s,enter adjust mode\n", tag);
				fts_info->judgment_mode = true;
			}
			fts_info->pre_judgment_time = adjust_time_ns;
			if (!fts_info->judgment_mode) {
				input_report_key(info->input_dev, KEY_VOLUMEDOWN + fts_info->volume_flag - 1, 1);
			}
		} else if (fts_info->volume_flag) {
			input_report_key(info->input_dev, KEY_VOLUMEDOWN + fts_info->volume_flag - 1, 0);
			fts_info->volume_flag = 0;
		}
		break;
	case INPUT_EVENT_SLIDE_FOR_VOLUME:
		getnstimeofday(&ts);
		adjust_time_ns = timespec_to_ns(&ts);
		if ((adjust_time_ns - fts_info->pre_judgment_time) > 3000000000) {
			log_error("%s, don't touch over 3s,enter adjust mode\n", tag);
			fts_info->m_slide_thr = fts_info->slide_thr_start;
			fts_info->m_samp_interval = fts_info->samp_interval_start;
		}
		if (++fts_info->m_sampdura >= fts_info->m_samp_interval) {
			fts_add_point(info, y);
			fts_info->m_sampdura = 0;
		}
		if (fts_point_size(info) >= 2) {
			fts_info->m_sampdura = 0;
			fts_change_slide_to_volume(info);
			fts_clear_point(info);
			fts_add_point(info, y);
		}
		break;
	case INPUT_EVENT_SINGLE_TAP_FOR_VOLUME:
		/*
		if (fts_is_volume(x, y)) {
			input_report_key(info->input_dev, KEY_VOLUMEDOWN + fts_info->volume_flag - 1, 1);
		} else if (fts_info->volume_flag) {
			input_report_key(info->input_dev, KEY_VOLUMEDOWN + fts_info->volume_flag - 1, 0);
			fts_info->volume_flag = 0;
		}
		break;
		*/
	case INPUT_EVENT_LONG_SINGLE_TAP_FOR_VOLUME:
		if (fts_is_volume(x, y)) {
			getnstimeofday(&ts);
			if (fts_info->down_time_ns == 0)
				fts_info->down_time_ns = timespec_to_ns(&ts);
			if ((fts_info->down_time_ns - fts_info->pre_judgment_time) > 3000000000) {
				log_error("%s, don't touch over 3s,enter adjust mode\n", tag);
				fts_info->judgment_mode = true;
			}
			fts_info->pre_judgment_time = fts_info->down_time_ns;
			if (!info->judgment_mode) {
				input_report_key(info->input_dev, KEY_VOLUMEDOWN + fts_info->volume_flag - 1, 1);
			}
		} else if (fts_info->volume_flag) {
			input_report_key(info->input_dev, KEY_VOLUMEDOWN + fts_info->volume_flag - 1, 0);
			fts_info->volume_flag = 0;
		}
		break;
	default:
		goto no_report;
	}
	if (fts_info->debug_abs) {
		input_mt_slot(info->input_dev, 9);
		input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 1);

		input_report_key(info->input_dev, BTN_TOUCH, 1);
		input_report_key(info->input_dev, BTN_TOOL_FINGER, 1);

		input_report_abs(info->input_dev, ABS_MT_POSITION_X, x);
		input_report_abs(info->input_dev, ABS_MT_POSITION_Y, y);
		input_report_abs(info->input_dev, ABS_MT_TOUCH_MAJOR, 1);
		input_report_abs(info->input_dev, ABS_MT_TOUCH_MINOR, 1);
		input_report_abs(info->input_dev, ABS_MT_PRESSURE, 1);
	}
no_report:
	return fts_next_event(event);
}

static unsigned char *fts_edge_leave_pointer_event_handler(struct fts_ts_info *info, unsigned char *event)
{
	int x, y;
	struct timespec ts;
	long long time_ns;

	if (!info->bdata->side_volume)
		goto no_report;
	x = (event[2] << 4) | (event[4] & 0xF0) >> 4;
	y = (event[3] << 4) | (event[4] & 0x0F);
	switch (fts_info->volume_type) {
	case INPUT_EVENT_DOUBLE_TAP_FOR_VOLUME:
		if (fts_is_volume(x, y)) {
			getnstimeofday(&ts);
			time_ns = timespec_to_ns(&ts);
			if (info->judgment_mode) {
				if ((time_ns - info->pre_doubletap_time) < fts_info->doubletap_interval * 1000000 && (time_ns - info->pre_doubletap_time) > 30000000
						&& fts_abs(y - info->pre_y) < fts_info->doubletap_distance) {
					log_error("%s, double tap volume find\n", tag);
					fts_input_report_key(info, KEY_VOLUMEDOWN + fts_info->volume_flag - 1);
					fts_info->judgment_mode = false;
				}
				info->pre_doubletap_time = time_ns;
				info->pre_y = y;
			} else {
				input_report_key(info->input_dev, KEY_VOLUMEDOWN + fts_info->volume_flag - 1, 0);
				fts_info->volume_flag = 0;
			}
		}
		break;
	case INPUT_EVENT_SLIDE_FOR_VOLUME:
		fts_info->m_sampdura = 0;
		fts_clear_point(info);
		break;
	case INPUT_EVENT_SINGLE_TAP_FOR_VOLUME:
/*		if (fts_is_volume(x, y)) {
			input_report_key(info->input_dev, KEY_VOLUMEDOWN + fts_info->volume_flag - 1, 0);
			fts_info->volume_flag = 0;
		}
		break;
		*/
	case INPUT_EVENT_LONG_SINGLE_TAP_FOR_VOLUME:
		if (fts_is_volume(x, y)) {
			getnstimeofday(&ts);
			time_ns = timespec_to_ns(&ts);
			if (info->judgment_mode) {
				if ((time_ns - fts_info->down_time_ns) > fts_info->single_press_time_low * 1000000 && (time_ns - fts_info->down_time_ns) < fts_info->single_press_time_hi * 1000000) {
					log_error("%s, single tap volume find\n", tag);
					fts_input_report_key(info, KEY_INFO);
					fts_info->judgment_mode = false;
				}
			} else {
				input_report_key(info->input_dev, KEY_VOLUMEDOWN + fts_info->volume_flag - 1, 0);
				fts_info->volume_flag = 0;
			}
			fts_info->down_time_ns = 0;
		}
		break;
	default:
		goto no_report;

	}
	if (fts_info->debug_abs) {
		input_mt_slot(info->input_dev, 9);
		input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 0);

		input_report_key(info->input_dev, BTN_TOUCH, 0);
		input_report_key(info->input_dev, BTN_TOOL_FINGER, 0);

		input_report_abs(info->input_dev, ABS_MT_TRACKING_ID, -1);
	}
no_report:
	return fts_next_event(event);
}

#define fts_edge_motion_pointer_event_handler fts_edge_enter_pointer_event_handler
#endif
#ifdef PHONE_KEY
static unsigned char *fts_key_status_event_handler(struct fts_ts_info *info, unsigned char *event)
{
	int i;
	bool curr_state, new_state;
	log_debug("%s %s Received event %02X %02X %02X %02X %02X %02X %02X %02X\n", tag, __func__, event[0], event[1], event[2], event[3], event[4], event[5], event[6], event[7]);
	for (i = 0; i < info->bdata->nbuttons; i++) {
		curr_state = test_bit(i, &info->bdata->keystates);
		new_state = test_bit(i, (unsigned long *)&event[2]);
		if (curr_state ^ new_state) {
			input_report_key(info->input_dev, info->bdata->key_code[i], !!(event[2] & (1 << i)));
			input_sync(info->input_dev);
		}
	}
	info->bdata->keystates = event[2];
	return fts_next_event(event);
}
#endif

#ifdef PHONE_PALM
static unsigned char *fts_palm_status_event_handler(struct fts_ts_info *info, unsigned char *event)
{
	log_debug("%s %s Received event %02X %02X %02X %02X %02X %02X %02X %02X\n", tag, __func__, event[0], event[1], event[2], event[3], event[4], event[5], event[6], event[7]);
	if (event[1] == 0xE5 && info->palm_enabled)
		fts_input_report_key(info, KEY_SLEEP);
	return fts_next_event(event);
}
#endif
static unsigned char *fts_error_event_handler(struct fts_ts_info *info, unsigned char *event)
{
	int error = 0;
	log_debug("%s %s Received event %02X %02X %02X %02X %02X %02X %02X %02X\n", tag, __func__, event[0], event[1], event[2], event[3], event[4], event[5], event[6], event[7]);

	switch (event[1]) {
	case EVENT_TYPE_ESD_ERROR:
		release_all_touches(info);
		fts_chip_powercycle(info);
		error = fts_system_reset();
		error |= fts_mode_handler(info, 0);
		error |= fts_enableInterrupt();

		if (error < OK)
			log_error("%s %s Cannot restore the device ERROR %08X\n", tag, __func__, error);

		break;

	case EVENT_TYPE_WATCHDOG_ERROR:
			dumpErrorInfo();
			release_all_touches(info);
			error = fts_system_reset();
			error |= fts_mode_handler(info, 0);
			error |= fts_enableInterrupt();

			if (error < OK)
				log_error("%s %s Cannot reset the device ERROR %08X\n", tag, __func__, error);
		break;
	}

	return fts_next_event(event);
}

static unsigned char *fts_controller_ready_event_handler(struct fts_ts_info *info, unsigned char *event)
{
	int error;
	log_debug("%s %s Received event 0x%02x\n", tag, __func__, event[0]);
	release_all_touches(info);
	setSystemResettedUp(1);
	setSystemResettedDown(1);
	error = fts_mode_handler(info, 0);

	if (error < OK)
		log_error("%s %s Cannot restore the device status ERROR %08X\n", tag, __func__, error);

	return fts_next_event(event);
}

static unsigned char *fts_status_event_handler(struct fts_ts_info *info, unsigned char *event)
{
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
		log_error("%s %s Received unhandled status event = %02X %02X %02X %02X %02X %02X %02X %02X\n", tag, __func__, event[0], event[1], event[2], event[3], event[4], event[5], event[6], event[7]);
		break;
	}

	return fts_next_event(event);
}

#ifdef PHONE_GESTURE
static unsigned char *fts_gesture_event_handler(struct fts_ts_info *info, unsigned char *event)
{
	unsigned char touchId;
	int value;
	char ch[64] = {0x0,};

	if (!info->gesture_enabled)
		return fts_next_event(event);
	log_debug("%s  gesture  event	data: %02X %02X %02X %02X %02X %02X %02X %02X\n", tag, event[0], event[1], event[2], event[3], event[4], event[5], event[6], event[7]);

	if (event[1] == 0x03)
		log_error("%s %s: Gesture ID %02X enable_status = %02X\n", tag, __func__, event[2], event[3]);

	if (event[1] == EVENT_TYPE_ENB && event[2] == 0x00) {
		switch (event[3]) {
		case GESTURE_ENABLE:
			log_error("%s %s: Gesture Enabled! res = %02X\n", tag, __func__, event[4]);
			break;

		case GESTURE_DISABLE:
			log_error("%s %s: Gesture Disabled! res = %02X\n", tag, __func__, event[4]);
			break;

		default:
			log_error("%s %s: Event not Valid!\n", tag, __func__);
		}
	}

	touchId = 0;
	__set_bit(touchId, &info->touch_id);

	if (event[0] == EVENTID_GESTURE && (event[1] == EVENT_TYPE_GESTURE_DTC1 || event[1] == EVENT_TYPE_GESTURE_DTC2)) {
		switch (event[2]) {
		case GES_ID_DBLTAP:
			value = KEY_WAKEUP;
			info->dbclick_count++;
			log_debug("%s %s: double tap !\n", tag, __func__);
			snprintf(ch, sizeof(ch), "%d", info->dbclick_count);
			break;

		case GES_ID_AT:
			value = KEY_WWW;
			log_debug("%s %s: @ !\n", tag, __func__);
			break;

		case GES_ID_C:
			value = KEY_C;
			log_debug("%s %s: C !\n", tag, __func__);
			break;

		case GES_ID_E:
			value = KEY_E;
			log_debug("%s %s: e !\n", tag, __func__);
			break;

		case GES_ID_F:
			value = KEY_F;
			log_debug("%s %s: F !\n", tag, __func__);
			break;

		case GES_ID_L:
			value = KEY_L;
			log_debug("%s %s: L !\n", tag, __func__);
			break;

		case GES_ID_M:
			value = KEY_M;
			log_debug("%s %s: M !\n", tag, __func__);
			break;

		case GES_ID_O:
			value = KEY_O;
			log_debug("%s %s: O !\n", tag, __func__);
			break;

		case GES_ID_S:
			value = KEY_S;
			log_debug("%s %s: S !\n", tag, __func__);
			break;

		case GES_ID_V:
			value = KEY_V;
			log_debug("%s %s:  V !\n", tag, __func__);
			break;

		case GES_ID_W:
			value = KEY_W;
			log_debug("%s %s:  W !\n", tag, __func__);
			break;

		case GES_ID_Z:
			value = KEY_Z;
			log_debug("%s %s:  Z !\n", tag, __func__);
			break;

		case GES_ID_HFLIP_L2R:
			value = KEY_RIGHT;
			log_debug("%s %s:  -> !\n", tag, __func__);
			break;

		case GES_ID_HFLIP_R2L:
			value = KEY_LEFT;
			log_debug("%s %s:  <- !\n", tag, __func__);
			break;

		case GES_ID_VFLIP_D2T:
			value = KEY_UP;
			log_debug("%s %s:  UP !\n", tag, __func__);
			break;

		case GES_ID_VFLIP_T2D:
			value = KEY_DOWN;
			log_debug("%s %s:  DOWN !\n", tag, __func__);
			break;

		case GES_ID_CUST1:
			value = KEY_F1;
			log_debug("%s %s:  F1 !\n", tag, __func__);
			break;

		case GES_ID_CUST2:
			value = KEY_F1;
			log_debug("%s %s:  F2 !\n", tag, __func__);
			break;

		case GES_ID_CUST3:
			value = KEY_F3;
			log_debug("%s %s:  F3 !\n", tag, __func__);
			break;

		case GES_ID_CUST4:
			value = KEY_F1;
			log_debug("%s %s:  F4 !\n", tag, __func__);
			break;

		case GES_ID_CUST5:
			value = KEY_F1;
			log_debug("%s %s:  F5 !\n", tag, __func__);
			break;

		case GES_ID_LEFTBRACE:
			value = KEY_LEFTBRACE;
			log_debug("%s %s:  < !\n", tag, __func__);
			break;

		case GES_ID_RIGHTBRACE:
			value = KEY_RIGHTBRACE;
			log_debug("%s %s:  > !\n", tag, __func__);
			break;

		default:
			log_debug("%s %s:  No valid GestureID!\n", tag, __func__);
			goto gesture_done;
		}

		fts_input_report_key(info, value);
gesture_done:
		__clear_bit(touchId, &info->touch_id);
	}

	return fts_next_event(event);
}
#endif

#define fts_motion_pointer_event_handler fts_enter_pointer_event_handler

static void fts_event_handler(struct work_struct *work)
{
	struct fts_ts_info *info;
	int error, error1;
	int left_events;
	unsigned char regAdd;
	unsigned char data[FIFO_EVENT_SIZE * (FIFO_DEPTH)] = {0};
	unsigned char *event = NULL;
	unsigned char eventId;
	event_dispatch_handler_t event_handler;
	info = container_of(work, struct fts_ts_info, work);
	regAdd = FIFO_CMD_READONE;
	error = fts_readCmd(&regAdd, sizeof(regAdd), data, FIFO_EVENT_SIZE);

	if (!error) {
		left_events = data[7] & 0x1F;

		if ((left_events > 0) && (left_events < FIFO_DEPTH)) {
			regAdd = FIFO_CMD_READALL;
			error1 = fts_readCmd(&regAdd, sizeof(regAdd), &data[FIFO_EVENT_SIZE], left_events * FIFO_EVENT_SIZE);

			if (error1)
				data[7] &= 0xE0;
		}

		event = data;

		do {
			eventId = *event;
			event_handler = info->event_dispatch_table[eventId];

			if (eventId < EVENTID_LAST) {
				event = event_handler(info, (event));
			} else {
				event = fts_next_event(event);
			}

		} while (event);
		input_sync(info->input_dev);
	}

	fts_interrupt_enable(info);
}

static int cx_crc_check(void)
{
	unsigned char regAdd1[3] = {FTS_CMD_HW_REG_R, ADDR_CRC_BYTE0, ADDR_CRC_BYTE1};
	unsigned char val[2];
	unsigned char crc_status = 0;
	int res = 0;
#ifndef FTM3_CHIP
	u8 cmd[4] = { FTS_CMD_HW_REG_W, 0x00, 0x00, SYSTEM_RESET_VALUE };
	int event_to_search[2] = {(int)EVENTID_ERROR_EVENT, (int)EVENT_TYPE_CHECKSUM_ERROR} ;
	u8 readData[FIFO_EVENT_SIZE];
#endif
	res = fts_readCmd(regAdd1, sizeof(regAdd1), val, 2);

	if (res < OK) {
		log_error("%s %s Cannot read crc status ERROR %08X\n", tag, __func__, res);
		return res;
	}

	crc_status = val[1] & CRC_MASK;

	if (crc_status != OK) {
		log_error("%s %s CRC ERROR = %X\n", tag, __func__, crc_status);
		return crc_status;
	}

#ifndef FTM3_CHIP
	log_debug("%s %s: Verifying if Config CRC Error...\n", tag, __func__);
	u16ToU8_be(SYSTEM_RESET_ADDRESS, &cmd[1]);
	res = fts_writeCmd(cmd, 4);

	if (res < OK) {
		log_error("%s %s Cannot send system resest command ERROR %08X\n", tag, __func__, res);
		return res;
	}

	setSystemResettedDown(1);
	setSystemResettedUp(1);
	res = pollForEvent(event_to_search, 2, readData, GENERAL_TIMEOUT);

	if (res < OK) {
		log_error("%s cx_crc_check: No Config CRC Found!\n", tag);
	} else {
		if (readData[2] == CRC_CONFIG_SIGNATURE || readData[2] == CRC_CONFIG) {
			log_error("%s cx_crc_check: CRC Error for config found! CRC ERROR = %02X\n", tag, readData[2]);
			return readData[2];
		}
	}

#endif
	return OK;
}

static const char *fts_get_config(struct fts_ts_info *info)
{
	struct fts_i2c_platform_data *pdata = info->bdata;
	int i = 0, ret = 0;

	ret = fts_get_lockdown_info(info->lockdown_info);

	if (ret < OK) {
		log_error("%s can't read lockdown info", tag);
		return PATH_FILE_FW;
	}

	ret |= fts_enableInterrupt();

	for (i = 0; i < pdata->config_array_size; i++) {
		if ((info->lockdown_info[0] == pdata->config_array[i].tp_vendor))
			break;
	}

	if (i >= pdata->config_array_size) {
		log_error("%s can't find right config", tag);
		return PATH_FILE_FW;
	}

	log_error("%s Choose config %d: %s", tag, i, pdata->config_array[i].fts_cfg_name);
	pdata->current_index = i;
#ifdef CONFIG_FTS_TOUCH_COUNT_DUMP
	if (pdata->dump_click_count) {
		info->current_clicknum_file = kzalloc(TOUCH_COUNT_FILE_MAXSIZE, GFP_KERNEL);
		strlcpy(info->current_clicknum_file, pdata->config_array[i].clicknum_file_name, TOUCH_COUNT_FILE_MAXSIZE);
	}
#endif
	return pdata->config_array[i].fts_cfg_name;
}

static const char *fts_get_limit(struct fts_ts_info *info)
{
	struct fts_i2c_platform_data *pdata = info->bdata;
	int i = 0, ret = 0;

	ret = fts_get_lockdown_info(info->lockdown_info);

	if (ret < OK) {
		log_error("%s can't read lockdown info", tag);
		return LIMITS_FILE;
	}

	ret |= fts_enableInterrupt();

	for (i = 0; i < pdata->config_array_size; i++) {
		if ((info->lockdown_info[0] == pdata->config_array[i].tp_vendor))
			break;
	}

	if (i >= pdata->config_array_size) {
		log_error("%s can't find right limit", tag);
		return LIMITS_FILE;
	}

	log_error("%s Choose limit file %d: %s", tag, i, pdata->config_array[i].fts_limit_name);
	pdata->current_index = i;
	return pdata->config_array[i].fts_limit_name;
}

static int fts_flash_procedure(struct fts_ts_info *info, const char *fw_name, int force)
{
#ifndef FTM3_CHIP
	u8 cmd[4] = { FTS_CMD_HW_REG_W, 0x00, 0x00, SYSTEM_RESET_VALUE };
	int event_to_search[2] = {(int)EVENTID_ERROR_EVENT, (int)EVENT_TYPE_CHECKSUM_ERROR} ;
	u8 readData[FIFO_EVENT_SIZE];
	int flag_init = 0;
#endif
	int retval = 0;
	int retval1 = 0;
	int ret = 0;
	int crc_status = 0;
	int error = 0;

	log_error("%s Fw Auto Update is starting...\n", tag);
	ret = cx_crc_check();

	if (ret > OK && ftsInfo.u16_fwVer == 0x0000) {
		log_error("%s %s: CRC Error or NO FW!\n", tag, __func__);
		crc_status = 1;
	} else {
		crc_status = 0;
		log_error("%s %s: NO CRC Error or Impossible to read CRC register!\n", tag, __func__);
	}
	if (fw_name == NULL) {
		fw_name = fts_get_config(info);
		if (fw_name == NULL) {
			log_error("%s not found mached config!", tag);
			goto END;
		}
	}
	if (force)
		retval = flashProcedure(fw_name, 1, 1);
	else
		retval = flashProcedure(fw_name, crc_status, 1);

	if ((retval & 0xFF000000) == ERROR_FLASH_PROCEDURE) {
		log_error("%s %s: firmware update failed and retry! ERROR %08X\n", tag, __func__, retval);
		fts_chip_powercycle(info);
		if (force)
			retval1 = flashProcedure(fw_name, 1, 1);
		else
			retval1 = flashProcedure(fw_name, crc_status, 1);

		if ((retval1 & 0xFF000000) == ERROR_FLASH_PROCEDURE) {
			log_error("%s %s: firmware update failed again!  ERROR %08X\n", tag, __func__, retval1);
		}
	}

#ifndef FTM3_CHIP
	log_error("%s %s: Verifying if CX CRC Error...%02x\n", tag, __func__, ret);
	u16ToU8_be(SYSTEM_RESET_ADDRESS, &cmd[1]);
	ret = fts_writeCmd(cmd, 4);

	if (ret < OK) {
		log_error("%s %s Cannot send system reset command ERROR %08X\n", tag, __func__, ret);
	} else {
		setSystemResettedDown(1);
		setSystemResettedUp(1);
		ret = pollForEvent(event_to_search, 2, readData, GENERAL_TIMEOUT);

		if (ret < OK)
			log_error("%s %s: No CX CRC Found!\n", tag, __func__);
		else {
			if (readData[2] == CRC_CX_MEMORY) {
				log_error("%s %s: CRC Error for CX found! CRC ERROR = %02X\n\n", tag, __func__, readData[2]);
				flag_init = 1;
			}
		}
	}

#endif
END:
	if (((ftsInfo.u32_mpPassFlag != INIT_MP) && (ftsInfo.u32_mpPassFlag != INIT_FIELD))
#ifndef FTM3_CHIP
	    || flag_init == 1
#endif
		|| ftsInfo.u8_msScrConfigTuneVer != ftsInfo.u8_msScrCxmemTuneVer
		|| ftsInfo.u8_ssTchConfigTuneVer != ftsInfo.u8_ssTchCxmemTuneVer
	   )
		ret = ERROR_GET_INIT_STATUS;
	else
		ret = OK;

	if (ret == ERROR_GET_INIT_STATUS) {
		log_error("%s do chip initialization,init_flag:0x%x\n", tag, ftsInfo.u32_mpPassFlag);
		error = fts_chip_autotune(info);

		if (error < OK) {
			log_error("%s %s Cannot initialize the chip ERROR %08X\n", tag, __func__, error);
		}
	}

	error = fts_init_hw(info);

	if (error < OK)
		log_error("%s Cannot initialize the hardware device ERROR %08X\n", tag, error);

	log_error("%s Fw Auto Update Finished!\n", tag);
	return error;

}

static void fts_fw_update_auto(struct work_struct *work)
{
	struct delayed_work *fwu_work = container_of(work, struct delayed_work, work);
	struct fts_ts_info *info = container_of(fwu_work, struct fts_ts_info, fwu_work);

#ifdef CONFIG_INPUT_PRESS_NDT
	ndt_update_fw(false, "ndt_fw.bin");
#endif
	fts_flash_procedure(info, NULL, 0);
#ifdef CONFIG_DRM
	drm_register_client(&info->notifier);
#endif
}

static int fts_chip_autotune(struct fts_ts_info *info)
{
	int ret2 = 0;
	int retry;
	int initretrycnt = 0;
	TestToDo todoDefault;
	const char *limit_file_name = NULL;

	todoDefault.MutualRaw = 1;
	todoDefault.MutualRawGap = 0;
	todoDefault.MutualCx1 = 0;
	todoDefault.MutualCx2 = 0;
	todoDefault.MutualCx2Adj = 0;
	todoDefault.MutualCxTotal = 1;
	todoDefault.MutualCxTotalAdj = 1;
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

	limit_file_name = fts_get_limit(info);
	for (retry = 0; retry <= INIT_FLAG_CNT; retry++) {
		ret2 = production_test_main(limit_file_name, 1, 1, &todoDefault, INIT_FIELD);

		if (ret2 == OK)
			break;

		initretrycnt++;
		log_error("%s initialization cycle count = %04d - ERROR %08X\n", tag, initretrycnt, ret2);
		fts_chip_powercycle(info);
	}

	return ret2;
}

#ifdef FTS_USE_POLLING_MODE
static enum hrtimer_restart fts_timer_func(struct hrtimer *timer)
{
	struct fts_ts_info *info = container_of(timer, struct fts_ts_info, timer);
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
	info->event_dispatch_table = kzalloc(sizeof(event_dispatch_handler_t) * EVENTID_LAST, GFP_KERNEL);

	if (!info->event_dispatch_table) {
		log_error("%s OOM allocating event dispatch table\n", tag);
		return -ENOMEM;
	}

	for (i = 0; i < EVENTID_LAST; i++)
		info->event_dispatch_table[i] = fts_nop_event_handler;

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
#ifdef EDGEHOVER_FOR_VOLUME
	install_handler(info, EDGE_ENTER_POINTER, edge_enter_pointer);
	install_handler(info, EDGE_LEAVE_POINTER, edge_leave_pointer);
	install_handler(info, EDGE_MOTION_POINTER, edge_motion_pointer);
#endif
#ifdef PHONE_PALM
	install_handler(info, PALM, palm_status);
#endif
	error = fts_disableInterrupt();
#ifdef FTS_USE_POLLING_MODE
	log_debug("%s Polling Mode\n", tag);
	hrtimer_init(&info->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	info->timer.function = fts_timer_func;
	hrtimer_start(&info->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
#else
	log_debug("%s Interrupt Mode\n", tag);

	if (request_irq(info->client->irq, fts_interrupt_handler, info->bdata->irq_flags, info->client->name, info)) {
		log_error("%s Request irq failed\n", tag);
		kfree(info->event_dispatch_table);
		error = -EBUSY;
	}

#endif
	return error;
}

static void fts_interrupt_uninstall(struct fts_ts_info *info)
{
	fts_disableInterrupt();

	if (info->event_dispatch_table != NULL)
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
}

static int fts_init(struct fts_ts_info *info)
{
	int error;
	error = fts_system_reset();

	if (error < OK && error != (ERROR_TIMEOUT | ERROR_SYSTEM_RESET_FAIL)) {
		log_error("%s Cannot reset the device! ERROR %08X\n", tag, error);
		return error;
	} else {
		if (error == (ERROR_TIMEOUT | ERROR_SYSTEM_RESET_FAIL)) {
			log_error("%s Setting default Chip INFO!\n", tag);
			defaultChipInfo(0);
		} else {
			error = readChipInfo(0);

			if (error < OK)
				log_error("%s Cannot read Chip Info! ERROR %08X\n", tag, error);
		}
	}

	error = fts_interrupt_install(info);

	if (error != OK)
		log_error("%s Init (1) error (ERROR  = %08X)\n", __func__,  error);

	return error;
}

int fts_chip_powercycle(struct fts_ts_info *info)
{
	log_error("%s %s: Power Cycle Starting...\n", tag, __func__);

	fts_enable_reg(info, 0);
	if (info->bdata->reset_gpio != GPIO_NOT_DEFINED)
		gpio_set_value(info->bdata->reset_gpio, 0);
	else
		mdelay(300);

	fts_enable_reg(info, 1);

	if (info->bdata->reset_gpio != GPIO_NOT_DEFINED) {
		mdelay(10);
		gpio_set_value(info->bdata->reset_gpio, 1);
	}

	release_all_touches(info);
	log_error("%s %s: Power Cycle Finished!\n", tag, __func__);
	setSystemResettedUp(1);
	setSystemResettedDown(1);
	return OK;
}

static int fts_init_hw(struct fts_ts_info *info)
{
	int error = 0;
	error = cleanUp(0);
	error |= fts_mode_handler(info, 0);
	error |= fts_enableInterrupt();

	if (error < OK)
		log_error("%s Init Hw error (ERROR = %08X)\n", tag, error);

	return error;
}


static int fts_mode_handler(struct fts_ts_info *info, int force)
{
	int res = OK;
	int ret = OK;
	u8 cmd[2] = { 0x93, 0x01};
	log_debug("%s %s: Mode Handler starting...\n", tag, __func__);

	switch (info->resume_bit) {
	case 0:
		log_debug("%s %s: Screen OFF...\n", tag, __func__);
		res |= fts_command(info, FTS_CMD_MS_MT_SENSE_OFF);
#ifdef PHONE_KEY
		log_debug("%s %s: Key OFF! \n", tag, __func__);
		res |= fts_command(info, FTS_CMD_MS_KEY_OFF);
#endif
		info->mode = MODE_SENSEOFF;
#ifdef PHONE_GESTURE
#ifndef CONFIG_INPUT_PRESS_NDT
		if (info->gesture_enabled == 1) {
#endif
			log_debug("%s %s: enter in gesture mode !\n", tag, __func__);
			ret = enterGestureMode(isSystemResettedDown());

			if (ret >= OK)
				info->mode = MODE_GESTURE;
			else
				log_error("%s %s: enterGestureMode failed! ERROR %08X recovery in senseOff...\n", tag, __func__, ret);

			res |= ret;
#ifndef CONFIG_INPUT_PRESS_NDT
		}
#endif

#endif
#ifndef CONFIG_INPUT_PRESS_NDT
		if (info->mode != MODE_GESTURE || info->gesture_enabled == 0) {
			fts_disableInterrupt();
		}
#endif

		setSystemResettedDown(0);
		break;

	case 1:
		info->mode = MODE_NORMAL;
		if (info->glove_enabled == FEAT_ENABLE || force == 1) {
			if (isSystemResettedUp() || force == 1) {
				log_debug("%s %s: Glove Mode setting...\n", tag, __func__);
				ret = featureEnableDisable(info->glove_enabled, FEAT_GLOVE);

				if (ret < OK)
					log_error("%s %s: error during setting GLOVE_MODE! ERROR %08X\n", tag, __func__, ret);

				res |= ret;
			}

			if (ret >= OK && info->glove_enabled == FEAT_ENABLE) {
				info->mode = MODE_GLOVE;
				log_error("%s %s: GLOVE_MODE Enabled!\n", tag, __func__);
			} else {
				log_error("%s %s: GLOVE_MODE Disabled!\n", tag, __func__);
			}
		}

		log_debug("%s %s: Sense ON without calibration!\n", tag, __func__);
		/*
		res |= fts_command(info, FTS_CMD_MS_MT_SENSE_ON);
		*/
		res |= fts_writeCmd(cmd, 2);
		if (res < OK) {
			log_error("%s %s Cannot send sense on command ERROR %08X\n", tag, __func__, res);
		}
#ifdef PHONE_KEY
		log_debug("%s %s: Key ON!\n", tag, __func__);
		res |= fts_command(info, FTS_CMD_MS_KEY_ON);
#endif

		setSystemResettedUp(0);
		break;

	default:
		log_error("%s %s: invalid resume_bit value = %d! ERROR %08X\n", tag, __func__, info->resume_bit, ERROR_OP_NOT_ALLOW);
		res = ERROR_OP_NOT_ALLOW;
	}

	log_debug("%s %s: Mode Handler finished! res = %08X\n", tag, __func__, res);
	return res;
}

static void fts_resume_work(struct work_struct *work)
{
    struct fts_ts_info *info;

    info = container_of(work, struct fts_ts_info, resume_work);
    info->resume_bit = 1;
#ifdef CONFIG_INPUT_PRESS_NDT
	if (ndt_is_pressed() && aod_mode)
		aod_mode = false;
#endif
#ifdef USE_NOISE_PARAM
	readNoiseParameters(noise_params);
#endif
	fts_system_reset();
#ifdef USE_NOISE_PARAM
	writeNoiseParameters(noise_params);
#endif
	release_all_touches(info);
	fts_mode_handler(info, 0);
	info->sensor_sleep = false;
	fts_enableInterrupt();
}

static void fts_suspend_work(struct work_struct *work)
{
	struct fts_ts_info *info;

	info = container_of(work, struct fts_ts_info, suspend_work);
#ifdef EDGEHOVER_FOR_VOLUME
	if (info->volume_type == INPUT_EVENT_DOUBLE_TAP_FOR_VOLUME) {
		fts_info->volume_flag = 0;
		fts_info->judgment_mode = true;
	} else if (info->volume_type == INPUT_EVENT_SLIDE_FOR_VOLUME) {
		fts_info->m_sampdura = 0;
		fts_clear_point(info);
	} else if (info->volume_type == INPUT_EVENT_LONG_SINGLE_TAP_FOR_VOLUME || info->volume_type == INPUT_EVENT_SINGLE_TAP_FOR_VOLUME) {
		fts_info->volume_flag = 0;
		fts_info->judgment_mode = true;
	}
#endif
	info->resume_bit = 0;
	fts_mode_handler(info, 0);
	release_all_touches(info);
	info->sensor_sleep = true;
#ifdef CONFIG_FTS_TOUCH_COUNT_DUMP
	sysfs_notify(&fts_info->fts_touch_dev->kobj, NULL, "touch_suspend_notify");
#endif
}

#ifdef CONFIG_DRM
static int fts_drm_state_chg_callback(struct notifier_block *nb, unsigned long val, void *data)
{
	struct fts_ts_info *info = container_of(nb, struct fts_ts_info, notifier);
	struct drm_notify_data *evdata = data;
	unsigned int blank;

	if (val != DRM_EVENT_BLANK)
		return 0;

	if (evdata && evdata->data && val == DRM_EVENT_BLANK && info) {
		blank = *(int *)(evdata->data);

		switch (blank) {
		case DRM_BLANK_POWERDOWN:
			if (info->sensor_sleep)
				break;

			log_error("%s %s: DRM_BLANK_POWERDOWN\n", tag, __func__);
			queue_work(info->event_wq, &info->suspend_work);
			break;

		case DRM_BLANK_UNBLANK:
			if (!info->sensor_sleep)
				break;

			log_error("%s %s: DRM_BLANK_UNBLANK\n", tag, __func__);
			queue_work(info->event_wq, &info->resume_work);
			break;

		default:
			break;
		}
	}

	return NOTIFY_OK;
}

static struct notifier_block fts_noti_block = {
	.notifier_call = fts_drm_state_chg_callback,
};
#endif

static int fts_get_reg(struct fts_ts_info *fts_data, bool get)
{
	int retval;
	const struct fts_i2c_platform_data *bdata = fts_data->bdata;

	if (!get) {
		retval = 0;
		goto regulator_put;
	}

	if ((bdata->pwr_reg_name != NULL) && (*bdata->pwr_reg_name != 0)) {
		fts_data->pwr_reg = regulator_get(fts_data->dev, bdata->pwr_reg_name);

		if (IS_ERR(fts_data->pwr_reg)) {
			log_error("%s %s: Failed to get power regulator\n", tag, __func__);
			retval = PTR_ERR(fts_data->pwr_reg);
			goto regulator_put;
		}
	}

	if ((bdata->bus_reg_name != NULL) && (*bdata->bus_reg_name != 0)) {
		fts_data->bus_reg = regulator_get(fts_data->dev, bdata->bus_reg_name);

		if (IS_ERR(fts_data->bus_reg)) {
			log_error("%s %s: Failed to get bus pullup regulator\n", tag, __func__);
			retval = PTR_ERR(fts_data->bus_reg);
			goto regulator_put;
		}
	}

	return 0;
regulator_put:

	if (fts_data->pwr_reg) {
		regulator_put(fts_data->pwr_reg);
		fts_data->pwr_reg = NULL;
	}

	if (fts_data->bus_reg) {
		regulator_put(fts_data->bus_reg);
		fts_data->bus_reg = NULL;
	}

	return retval;
}

static int fts_enable_reg(struct fts_ts_info *fts_data, bool enable)
{
	int retval;

	if (!enable) {
		retval = 0;
		goto disable_bus_reg;
	}

	if (fts_data->pwr_reg) {
		retval = regulator_enable(fts_data->pwr_reg);

		if (retval < 0) {
			log_error("%s %s: Failed to enable power regulator\n", tag, __func__);
			goto exit;
		}
	}
	mdelay(2);
	if (fts_data->bus_reg) {
		retval = regulator_enable(fts_data->bus_reg);

		if (retval < 0) {
			log_error("%s %s: Failed to enable bus regulator\n", tag, __func__);
			goto disable_pwr_reg;
		}
	}


	return OK;

disable_bus_reg:

	if (fts_data->bus_reg)
		regulator_disable(fts_data->bus_reg);

disable_pwr_reg:

	if (fts_data->pwr_reg)
		regulator_disable(fts_data->pwr_reg);
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
			log_error("%s %s: Failed to get gpio %d (code: %d)", tag, __func__, gpio, retval);
			return retval;
		}

		if (dir == 0)
			retval = gpio_direction_input(gpio);
		else
			retval = gpio_direction_output(gpio, state);

		if (retval) {
			log_error("%s %s: Failed to set gpio %d direction", tag, __func__, gpio);
			return retval;
		}
	} else {
		gpio_free(gpio);
	}

	return retval;
}

static int fts_set_gpio(struct fts_ts_info *fts_data)
{
	int retval;
	const struct fts_i2c_platform_data *bdata = fts_data->bdata;
	retval = fts_gpio_setup(bdata->irq_gpio, true, 0, 0);

	if (retval < 0) {
		log_error("%s %s: Failed to configure irq GPIO\n", tag, __func__);
		goto err_gpio_irq;
	}

	if (bdata->reset_gpio >= 0) {
		retval = fts_gpio_setup(bdata->reset_gpio, true, 1, 0);

		if (retval < 0) {
			log_error("%s %s: Failed to configure reset GPIO\n", tag, __func__);
			goto err_gpio_reset;
		}
	}

	setResetGpio(bdata->reset_gpio);
	return OK;
err_gpio_reset:
	fts_gpio_setup(bdata->irq_gpio, false, 0, 0);
	setResetGpio(GPIO_NOT_DEFINED);
err_gpio_irq:
	return retval;
}

static int fts_unset_gpio(struct fts_ts_info *fts_data)
{
	const struct fts_i2c_platform_data *bdata = fts_data->bdata;

	fts_gpio_setup(bdata->irq_gpio, false, 0, 0);
	fts_gpio_setup(bdata->reset_gpio, false, 0, 0);
	return OK;
}

static int fts_pinctrl_init(struct fts_ts_info *info)
{
	int retval = 0;
	/* Get pinctrl if target uses pinctrl */
	info->ts_pinctrl = devm_pinctrl_get(info->dev);

	if (IS_ERR_OR_NULL(info->ts_pinctrl)) {
		retval = PTR_ERR(info->ts_pinctrl);
		dev_err(info->dev,
			"Target does not use pinctrl %d\n",
			retval);
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

static int parse_dt(struct device *dev, struct fts_i2c_platform_data *bdata)
{
	int retval;
	const char *name;
	struct device_node *temp, *np = dev->of_node;
	struct fts_config_info *config_info;
	u32 temp_val;
	bdata->irq_gpio = of_get_named_gpio_flags(np, "fts,irq-gpio", 0, NULL);
	log_debug("%s irq_gpio = %d\n", tag, bdata->irq_gpio);
	retval = of_property_read_string(np, "fts,pwr-reg-name", &name);

	if (retval == -EINVAL)
		bdata->pwr_reg_name = NULL;
	else if (retval < 0)
		return retval;
	else {
		bdata->pwr_reg_name = name;
		log_debug("%s pwr_reg_name = %s\n", tag, name);
	}

	retval = of_property_read_string(np, "fts,bus-reg-name", &name);

	if (retval == -EINVAL)
		bdata->bus_reg_name = NULL;
	else if (retval < 0)
		return retval;
	else {
		bdata->bus_reg_name = name;
		log_debug("%s bus_reg_name = %s\n", tag, name);
	}

	if (of_property_read_bool(np, "fts,reset-gpio-enable")) {
		bdata->reset_gpio = of_get_named_gpio_flags(np, "fts,reset-gpio", 0, NULL);
		log_debug("%s reset_gpio =%d\n", tag, bdata->reset_gpio);
	} else {
		bdata->reset_gpio = GPIO_NOT_DEFINED;
	}

	retval = of_property_read_u32(np, "fts,irq-flags", &temp_val);
	if (retval < 0)
		return retval;
	else
		bdata->irq_flags = temp_val;
#ifdef CONFIG_FTS_TOUCH_COUNT_DUMP
	bdata->dump_click_count = of_property_read_bool(np, "fts,dump-click-count");
#endif

	retval = of_property_read_u32(np, "fts,config-array-size", (u32 *)&bdata->config_array_size);

	if (retval) {
		log_error("%s Unable to get array size\n", tag);
		return retval;
	}

	bdata->config_array = devm_kzalloc(dev, bdata->config_array_size *
					   sizeof(struct fts_config_info), GFP_KERNEL);

	if (!bdata->config_array) {
		log_error("%s Unable to allocate memory\n", tag);
		return -ENOMEM;
	}

	config_info = bdata->config_array;
	for_each_child_of_node(np, temp) {
		retval = of_property_read_u32(temp, "fts,tp-vendor", &temp_val);

		if (retval) {
			log_error("%s Unable to read tp vendor\n", tag);
		} else {
			config_info->tp_vendor = (u8)temp_val;
			log_debug("%s %s:tp vendor: %u", tag, __func__, config_info->tp_vendor);
		}
		retval = of_property_read_u32(temp, "fts,tp-color", &temp_val);

		if (retval) {
			log_error("%s Unable to read tp color\n", tag);
		} else {
			config_info->tp_color = (u8)temp_val;
			log_debug("%s %s:tp color: %u", tag, __func__, config_info->tp_color);
		}

		retval = of_property_read_u32(temp, "fts,tp-hw-version", &temp_val);

		if (retval) {
			log_error("%s Unable to read tp hw version\n", tag);
		} else {
			config_info->tp_hw_version = (u8)temp_val;
			log_debug("%s %s:tp color: %u", tag, __func__, config_info->tp_hw_version);
		}

		retval = of_property_read_string(temp, "fts,fw-name",
						 &config_info->fts_cfg_name);

		if (retval && (retval != -EINVAL)) {
			log_error("%s Unable to read cfg name\n", tag);
		} else {
			log_debug("%s %s:fw_name: %s", tag, __func__, config_info->fts_cfg_name);
		}
		retval = of_property_read_string(temp, "fts,limit-name",
						 &config_info->fts_limit_name);

		if (retval && (retval != -EINVAL)) {
			log_error("%s Unable to read limit name\n", tag);
		} else {
			log_debug("%s %s:limit_name: %s", tag, __func__, config_info->fts_limit_name);
		}
#ifdef CONFIG_FTS_TOUCH_COUNT_DUMP
		if (bdata->dump_click_count) {
			retval = of_property_read_string(temp, "fts,clicknum-file-name",
				&config_info->clicknum_file_name);
			if (retval && (retval != -EINVAL)) {
				dev_err(dev, "Unable to read click count file name\n");
			} else
				dev_err(dev, "%s\n", config_info->clicknum_file_name);
		}
#endif

		config_info++;
	}
#ifdef PHONE_KEY
	retval = of_property_read_u32(np, "fts,key-num", &temp_val);
	if (retval) {
		log_error("%s Unable to read key num\n", tag);
		return retval;
	} else
		bdata->nbuttons = temp_val;

	if (bdata->nbuttons != 0) {
		bdata->key_code = devm_kzalloc(dev,
					sizeof(int) * bdata->nbuttons, GFP_KERNEL);
		if (!bdata->key_code)
			return -ENOMEM;
		retval = of_property_read_u32_array(np, "fts,key-codes",
						bdata->key_code, bdata->nbuttons);
		if (retval) {
			log_error("%s Unable to read key codes\n", tag);
			return retval;
		}
	}
#endif
#ifdef EDGEHOVER_FOR_VOLUME
	bdata->side_volume = of_property_read_bool(np, "fts,side-volume");
	if (bdata->side_volume) {
		retval = of_property_read_u32(np, "fts,x-base", &temp_val);
		if (retval)
			log_error("%s Unable to read x base value\n", tag);
		else
			bdata->x_base = temp_val;

		retval = of_property_read_u32(np, "fts,y-base", &temp_val);
		if (retval)
			log_error("%s Unable to read y base value\n", tag);
		else
			bdata->y_base = temp_val;

		retval = of_property_read_u32(np, "fts,y-offset", &temp_val);
		if (retval)
			log_error("%s Unable to read y offset value\n", tag);
		else
			bdata->y_offset = temp_val;

		retval = of_property_read_u32(np, "fts,y-skip", &temp_val);
		if (retval)
			log_error("%s Unable to read y skip value\n", tag);
		else
			bdata->y_skip = temp_val;
	}
#endif
	return OK;
}

static void fts_switch_mode_work(struct work_struct *work)
{
	struct fts_mode_switch *ms = container_of(work, struct fts_mode_switch, switch_mode_work);


	struct fts_ts_info *info = ms->info;
	unsigned char value = ms->mode;
	static const char *fts_gesture_on = "01 02";
	char *gesture_result;
	int size = 6 * 2 + 1;
	char ch[16] = {0x0,};

	log_error("%s %s mode:%d\n", tag, __func__, value);
	if (value >= INPUT_EVENT_WAKUP_MODE_OFF && value <= INPUT_EVENT_WAKUP_MODE_ON) {
		info->gesture_enabled = value - INPUT_EVENT_WAKUP_MODE_OFF;
		if (info->gesture_enabled) {
			gesture_result = (u8 *) kzalloc(size, GFP_KERNEL);
			if (gesture_result != NULL) {
				fts_gesture_mask_store(info->dev, NULL, fts_gesture_on, strlen(fts_gesture_on));
				fts_gesture_mask_show(info->dev, NULL, gesture_result);
				if (strncmp("AA00000000BB", gesture_result, size - 1))
					log_error("%s %s:store gesture mask error\n", tag, __func__);
				kfree(gesture_result);
				gesture_result = NULL;
			}
		}
			snprintf(ch, sizeof(ch), "%s", (value - INPUT_EVENT_WAKUP_MODE_OFF) ? "enabled" : "disabled");
	} else if (value >= INPUT_EVENT_COVER_MODE_OFF && value <= INPUT_EVENT_COVER_MODE_ON) {
		info->glove_enabled = value - INPUT_EVENT_COVER_MODE_OFF;
		fts_mode_handler(info, 1);
	}
#ifdef EDGEHOVER_FOR_VOLUME
	if (value >= INPUT_EVENT_SLIDE_FOR_VOLUME && value <= INPUT_EVENT_LONG_SINGLE_TAP_FOR_VOLUME) {
		info->volume_type = value;
		if (fts_info->volume_type == INPUT_EVENT_SINGLE_TAP_FOR_VOLUME) {
			fts_info->single_press_time_low = 30;
			fts_info->single_press_time_hi = 800;
		} else if (fts_info->volume_type == INPUT_EVENT_LONG_SINGLE_TAP_FOR_VOLUME) {
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

static int fts_input_event(struct input_dev *dev, unsigned int type, unsigned int code, int value)
{
	struct fts_ts_info *info = input_get_drvdata(dev);
	struct fts_mode_switch *ms;

	if (type == EV_SYN && code == SYN_CONFIG) {
		log_error("%s %s:set input event value = %d\n", tag, __func__, value);

		if (value >= INPUT_EVENT_START && value <= INPUT_EVENT_END) {
			ms = (struct fts_mode_switch *)kmalloc(sizeof(struct fts_mode_switch), GFP_ATOMIC);

			if (ms != NULL) {
				ms->info = info;
				ms->mode = (unsigned char)value;
				INIT_WORK(&ms->switch_mode_work, fts_switch_mode_work);
				schedule_work(&ms->switch_mode_work);
			} else {
				log_error("%s %s:failed in allocating memory for switching mode\n", tag, __func__);
				return -ENOMEM;
			}
		} else {
			log_error("%s %s:Invalid event value\n", tag, __func__);
			return -EINVAL;
		}
	}

	return 0;
}

static int fts_short_open_test(void)
{
	int ret;
	char buf[13] = {0};
	ret = stm_fts_cmd_store(fts_info->dev, NULL, "01", 2);

	if (ret == 0)
		return FTS_RESULT_INVALID;

	stm_fts_cmd_show(fts_info->dev, NULL, buf);

	if (!strncmp("AA00000000BB", buf, 12))
		return FTS_RESULT_PASS;
	else
		return FTS_RESULT_FAIL;
}

static int fts_i2c_test(void)
{
	int retry = 0;
	u8 data[CHIP_INFO_SIZE + 3];

	if (fts_info == NULL)
		return FTS_RESULT_INVALID;

	while (retry < 5) {
		if (readCmdU16(FTS_CMD_FRAMEBUFFER_R, ADDR_FRAMEBUFFER_DATA, data, CHIP_INFO_SIZE + 3, DUMMY_FRAMEBUFFER) < 0)
			log_error("%s readChipInfo: ERROR %02X\n", tag, ERROR_I2C_R);
		else
			break;

		msleep(20);
		retry++;
	}

	if (retry >= 5)
		return FTS_RESULT_FAIL;
	else
		return FTS_RESULT_PASS;
}

static ssize_t fts_selftest_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
	char tmp[5] = {0};
	int cnt;

	if (*pos != 0)
		return 0;
	cnt = snprintf(tmp, sizeof(fts_info->result_type), "%d\n", fts_info->result_type);
	if (copy_to_user(buf, tmp, strlen(tmp))) {
		return -EFAULT;
	}
	*pos += cnt;
	return cnt;
}

static ssize_t fts_selftest_write(struct file *file, const char __user *buf, size_t count, loff_t *pos)
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
	.read		= fts_selftest_read,
	.write		= fts_selftest_write,
};

static ssize_t fts_datadump_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
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

	ret = stm_fts_cmd_store(fts_info->dev, NULL, "13", 2);
	if (ret == 0)
		goto out;
	cnt1 = stm_fts_cmd_show(fts_info->dev, NULL, tmp);
	if (cnt1 == 0) {
		ret = 0;
		goto out;
	}
	ret = stm_fts_cmd_store(fts_info->dev, NULL, "15", 2);
	if (ret == 0)
		goto out;
	cnt2 = stm_fts_cmd_show(fts_info->dev, NULL, tmp + cnt1);
	if (cnt2 == 0) {
		ret = 0;
		goto out;
	}
	ret = fts_strength_frame_store(fts_info->dev, NULL, "1", 1);
	if (ret == 0)
		goto out;
	cnt3 = fts_strength_frame_show(fts_info->dev, NULL, tmp + cnt1 + cnt2);
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
	.read		= fts_datadump_read,
};

#define TP_INFO_MAX_LENGTH 50
static ssize_t fts_fw_version_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
	int cnt = 0, ret = 0;
	char tmp[TP_INFO_MAX_LENGTH];

	if (*pos != 0)
		return 0;

	cnt = snprintf(tmp, TP_INFO_MAX_LENGTH, "%x.%x\n", ftsInfo.u16_fwVer, ftsInfo.u16_cfgId);
	ret = copy_to_user(buf, tmp, cnt);
	*pos += cnt;
	if (ret != 0)
		return 0;
	else
		return cnt;
}

static const struct file_operations fts_fw_version_ops = {
	.read		= fts_fw_version_read,
};

static ssize_t fts_lockdown_info_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
	int cnt = 0, ret = 0;
	char tmp[TP_INFO_MAX_LENGTH];

	if (*pos != 0)
		return 0;

	ret = fts_get_lockdown_info(fts_info->lockdown_info);
	if (ret != OK) {
		log_error("%s %s get lockdown info error\n", tag, __func__);
		goto out;
	}

	cnt = snprintf(tmp, TP_INFO_MAX_LENGTH, "0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x\n",
			 fts_info->lockdown_info[0], fts_info->lockdown_info[1], fts_info->lockdown_info[2], fts_info->lockdown_info[3],
			 fts_info->lockdown_info[4], fts_info->lockdown_info[5], fts_info->lockdown_info[6], fts_info->lockdown_info[7]);
	ret = copy_to_user(buf, tmp, cnt);
out:
	*pos += cnt;
	if (ret != 0)
		return 0;
	else
		return cnt;
}

static const struct file_operations fts_lockdown_info_ops = {
	.read		= fts_lockdown_info_read,
};

#ifdef CONFIG_PM
static int fts_pm_suspend(struct device *dev)
{
	struct fts_ts_info *info = dev_get_drvdata(dev);
#ifndef CONFIG_INPUT_PRESS_NDT
	if (device_may_wakeup(dev) && info->gesture_enabled) {
		log_error("%s enable touch irq wake\n", tag);
		enable_irq_wake(info->client->irq);
	}
#else
	enable_irq_wake(info->client->irq);
#endif

	return 0;

}

static int fts_pm_resume(struct device *dev)
{
	struct fts_ts_info *info = dev_get_drvdata(dev);

#ifndef CONFIG_INPUT_PRESS_NDT
	if (device_may_wakeup(dev) && info->gesture_enabled) {
		log_error("%s disable touch irq wake\n", tag);
		disable_irq_wake(info->client->irq);
	}
#else
	disable_irq_wake(info->client->irq);
#endif
	return 0;
}

static const struct dev_pm_ops fts_dev_pm_ops = {
	.suspend = fts_pm_suspend,
	.resume  = fts_pm_resume,
};
#endif

#ifdef CONFIG_TOUCHSCREEN_ST_DEBUG_FS
static void tpdbg_shutdown(struct fts_ts_info *info, bool sleep)
{
	if (sleep) {
		fts_command(info, FTS_CMD_MS_MT_SENSE_OFF);
#ifdef PHONE_KEY
		fts_command(info, FTS_CMD_MS_KEY_OFF);
#endif
	} else {
		fts_command(info, FTS_CMD_MS_MT_SENSE_ON);
#ifdef PHONE_KEY
		fts_command(info, FTS_CMD_MS_KEY_ON);
#endif
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

static ssize_t tpdbg_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
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

static ssize_t tpdbg_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
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

static int fts_probe(struct i2c_client *client, const struct i2c_device_id *idp)
{
	struct fts_ts_info *info = NULL;
	char fts_ts_phys[64];
	int error = 0;
	struct device_node *dp = client->dev.of_node;
	int retval = 0;
#ifdef PHONE_KEY
	int i = 0;
#endif
	u8 *tp_maker;
	openChannel(client);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		log_error("%s Unsupported I2C functionality\n", tag);
		error = -EIO;
		goto ProbeErrorExit_0;
	}

	info = kzalloc(sizeof(struct fts_ts_info), GFP_KERNEL);

	if (!info) {
		log_error("%s Out of memory... Impossible to allocate struct info!\n", tag);
		error = -ENOMEM;
		goto ProbeErrorExit_0;
	}

	info->client = client;
	i2c_set_clientdata(client, info);
	info->dev = &info->client->dev;

	if (dp) {
		info->bdata = devm_kzalloc(&client->dev, sizeof(struct fts_i2c_platform_data), GFP_KERNEL);

		if (!info->bdata) {
			log_error("%s ERROR:info.bdata kzalloc failed\n", tag);
			goto ProbeErrorExit_1;
		}

		parse_dt(&client->dev, info->bdata);
	}

	retval = fts_get_reg(info, true);

	if (retval < 0) {
		log_error("%s ERROR: %s: Failed to get regulators\n", tag, __func__);
		goto ProbeErrorExit_1;
	}

	retval = fts_enable_reg(info, true);

	if (retval < 0) {
		log_error("%s %s: ERROR Failed to enable regulators\n", tag, __func__);
		goto ProbeErrorExit_2;
	}

	retval = fts_set_gpio(info);

	if (retval < 0) {
		log_error("%s %s: ERROR Failed to set up GPIO's\n", tag, __func__);
		goto ProbeErrorExit_3;
	}

	error = fts_pinctrl_init(info);

	if (!error && info->ts_pinctrl) {
		error = pinctrl_select_state(info->ts_pinctrl, info->pinctrl_state_active);

		if (error < 0) {
			dev_err(&client->dev,
				"%s: Failed to select %s pinstate %d\n",
				__func__, PINCTRL_STATE_ACTIVE, error);
		}
	} else {
		dev_err(&client->dev,
			"%s: Failed to init pinctrl\n",
			__func__);
	}

	info->client->irq = gpio_to_irq(info->bdata->irq_gpio);
	info->fwu_workqueue = create_singlethread_workqueue("fts-fwu-queue");

	if (!info->fwu_workqueue) {
		log_error("%s ERROR: Cannot create fwu work thread\n", tag);
		goto ProbeErrorExit_3_1;
	}

	INIT_DELAYED_WORK(&info->fwu_work, fts_fw_update_auto);
	info->event_wq = create_singlethread_workqueue("fts-event-queue");

	if (!info->event_wq) {
		log_error("%s ERROR: Cannot create work thread\n", tag);
		error = -ENOMEM;
		goto ProbeErrorExit_4;
	}

	INIT_WORK(&info->work, fts_event_handler);
	INIT_WORK(&info->resume_work, fts_resume_work);
	INIT_WORK(&info->suspend_work, fts_suspend_work);
	info->dev = &info->client->dev;
	info->input_dev = input_allocate_device();

	if (!info->input_dev) {
		log_error("%s ERROR: No such input device defined!\n", tag);
		error = -ENODEV;
		goto ProbeErrorExit_5;
	}

	info->input_dev->dev.parent = &client->dev;
	info->input_dev->name = FTS_TS_DRV_NAME;
	snprintf(fts_ts_phys, sizeof(fts_ts_phys), "%s/input0", info->input_dev->name);
	info->input_dev->phys = fts_ts_phys;
	info->input_dev->id.bustype = BUS_I2C;
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
	input_mt_init_slots(info->input_dev, TOUCH_ID_MAX, INPUT_MT_DIRECT);
	input_set_abs_params(info->input_dev, ABS_MT_POSITION_X, X_AXIS_MIN, X_AXIS_MAX, 0, 0);
	input_set_abs_params(info->input_dev, ABS_MT_POSITION_Y, Y_AXIS_MIN, Y_AXIS_MAX, 0, 0);
	input_set_abs_params(info->input_dev, ABS_MT_TOUCH_MAJOR, AREA_MIN, AREA_MAX, 0, 0);
	input_set_abs_params(info->input_dev, ABS_MT_TOUCH_MINOR, AREA_MIN, AREA_MAX, 0, 0);
	input_set_abs_params(info->input_dev, ABS_MT_PRESSURE, PRESSURE_MIN, PRESSURE_MAX, 0, 0);
#ifdef PHONE_GESTURE
	input_set_capability(info->input_dev, EV_KEY, KEY_WAKEUP);
	mutex_init(&gestureMask_mutex);
#endif
#ifdef PHONE_PALM
	input_set_capability(info->input_dev, EV_KEY, KEY_SLEEP);
	info->palm_enabled = true;
#endif
#ifdef PHONE_KEY
	for (i = 0; i < info->bdata->nbuttons; i++)
		input_set_capability(info->input_dev, EV_KEY, info->bdata->key_code[i]);
#endif
#ifdef EDGEHOVER_FOR_VOLUME
	input_set_capability(info->input_dev, EV_KEY, KEY_VOLUMEUP);
	input_set_capability(info->input_dev, EV_KEY, KEY_VOLUMEDOWN);
	input_set_capability(info->input_dev, EV_KEY, KEY_INFO);
	info->judgment_mode = true;
	info->volume_type = INPUT_EVENT_DOUBLE_TAP_FOR_VOLUME;
	info->slide_thr_start = 200;
	info->samp_interval_start = 12;
	info->slide_thr = 40;
	info->samp_interval = 4;
	info->m_slide_thr = info->slide_thr_start;
	info->m_samp_interval = info->samp_interval_start;
	info->doubletap_interval = 800;
	info->doubletap_distance = 80;
	info->single_press_time_low = 30;
	info->single_press_time_hi = 500;
#endif
#ifdef CONFIG_INPUT_PRESS_NDT
	input_set_capability(info->input_dev, EV_KEY, KEY_INFO);
#endif
	mutex_init(&(info->input_report_mutex));
	error = input_register_device(info->input_dev);

	if (error) {
		log_error("%s ERROR: No such input device\n", tag);
		error = -ENODEV;
		goto ProbeErrorExit_5_1;
	}

	info->touch_id = 0;
	error = fts_init(info);

	if (error < OK) {
		log_error("%s Cannot initialize the device ERROR %08X\n", tag, error);
		error = -ENODEV;
		goto ProbeErrorExit_6;
	}

	info->gesture_enabled = 0;
	info->glove_enabled = 0;
	info->resume_bit = 1;
	info->notifier = fts_noti_block;
	info->attrs.attrs = fts_attr_group;
	error = sysfs_create_group(&client->dev.kobj, &info->attrs);

	if (error) {
		log_error("%s ERROR: Cannot create sysfs structure!\n", tag);
		error = -ENODEV;
		goto ProbeErrorExit_7;
	}

	update_hardware_info(TYPE_TOUCH, 4);

	error = fts_get_lockdown_info(info->lockdown_info);

	if (error < OK)
		log_error("%s can't get lockdown info", tag);
	else {
		log_error("%s Lockdown:0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x\n", tag,
				info->lockdown_info[0], info->lockdown_info[1], info->lockdown_info[2], info->lockdown_info[3],
				info->lockdown_info[4], info->lockdown_info[5], info->lockdown_info[6], info->lockdown_info[7]);
		update_hardware_info(TYPE_TP_MAKER, info->lockdown_info[0] - 0x30);
	}

	tp_maker = kzalloc(20, GFP_KERNEL);
	if (tp_maker == NULL)
		log_error("%s fail to alloc vendor name memory\n", tag);
	else {
		kfree(tp_maker);
		tp_maker = NULL;
	}
	info->dbclick_count = 0;
	dev_set_drvdata(&client->dev, info);
	device_init_wakeup(&client->dev, 1);
#ifdef CONFIG_TOUCHSCREEN_ST_DEBUG_FS
	info->debugfs = debugfs_create_dir("tp_debug", NULL);
	if (info->debugfs) {
		debugfs_create_file("switch_state", 0660, info->debugfs, info, &tpdbg_operations);
	}
#endif
	fts_info = info;
#ifdef SCRIPTLESS

	if (fts_cmd_class == NULL)
		fts_cmd_class = class_create(THIS_MODULE, FTS_TS_DRV_NAME);

	info->i2c_cmd_dev = device_create(fts_cmd_class, NULL, DCHIP_ID_0, info, "fts_i2c");

	if (IS_ERR(info->i2c_cmd_dev)) {
		log_error("%s ERROR: Failed to create device for the sysfs!\n", tag);
		goto ProbeErrorExit_8;
	}

	dev_set_drvdata(info->i2c_cmd_dev, info);
	error = sysfs_create_group(&info->i2c_cmd_dev->kobj, &i2c_cmd_attr_group);

	if (error) {
		log_error("%s ERROR: Failed to create sysfs group!\n", tag);
		goto ProbeErrorExit_9;
	}

#endif
#ifdef DRIVER_TEST

	if (fts_cmd_class == NULL)
		fts_cmd_class = class_create(THIS_MODULE, FTS_TS_DRV_NAME);

	info->test_cmd_dev = device_create(fts_cmd_class, NULL, DCHIP_ID_1, info, "fts_driver_test");

	if (IS_ERR(info->test_cmd_dev)) {
		log_error("%s ERROR: Failed to create device for the sysfs!\n", tag);
		goto ProbeErrorExit_10;
	}

	dev_set_drvdata(info->test_cmd_dev, info);
	error = sysfs_create_group(&info->test_cmd_dev->kobj,  &test_cmd_attr_group);

	if (error) {
		log_error("%s ERROR: Failed to create sysfs group!\n", tag);
		goto ProbeErrorExit_11;
	}

#endif
#ifdef CONFIG_FTS_TOUCH_COUNT_DUMP
	if (info->fts_tp_class == NULL)
		info->fts_tp_class = class_create(THIS_MODULE, "touch");
	info->fts_touch_dev = device_create(info->fts_tp_class, NULL, DCHIP_ID_0, info, "touch_suspend_notify");

	if (IS_ERR(info->fts_touch_dev)) {
		log_error("%s ERROR: Failed to create device for the sysfs!\n", tag);
		goto ProbeErrorExit_12;
	}

	dev_set_drvdata(info->fts_touch_dev, info);
	error = sysfs_create_file(&info->fts_touch_dev->kobj, &dev_attr_touch_suspend_notify.attr);

	if (error) {
		log_error("%s ERROR: Failed to create sysfs group!\n", tag);
		goto ProbeErrorExit_13;
	}
#endif
	info->tp_selftest_proc = proc_create("tp_selftest", 0, NULL, &fts_selftest_ops);
	info->tp_data_dump_proc = proc_create("tp_data_dump", 0, NULL, &fts_datadump_ops);
	info->tp_fw_version_proc = proc_create("tp_fw_version", 0, NULL, &fts_fw_version_ops);
	info->tp_lockdown_info_proc = proc_create("tp_lockdown_info", 0, NULL, &fts_lockdown_info_ops);
	queue_delayed_work(info->fwu_workqueue, &info->fwu_work, msecs_to_jiffies(EXP_FN_WORK_DELAY_MS));

	return OK;
#ifdef CONFIG_FTS_TOUCH_COUNT_DUMP
ProbeErrorExit_13:
	device_destroy(info->fts_tp_class, DCHIP_ID_0);
	class_destroy(info->fts_tp_class);
	info->fts_tp_class = NULL;
#endif
#ifdef CONFIG_FTS_TOUCH_COUNT_DUMP
#ifdef DRIVER_TEST
ProbeErrorExit_12:
	sysfs_remove_group(&info->test_cmd_dev->kobj,  &test_cmd_attr_group);
#endif
#endif
#ifdef DRIVER_TEST
ProbeErrorExit_11:
	device_destroy(fts_cmd_class, DCHIP_ID_1);
#ifndef SCRIPTLESS
	class_destroy(fts_cmd_class);
	fts_cmd_class = NULL;
#endif
ProbeErrorExit_10:
#ifdef SCRIPTLESS
	sysfs_remove_group(&info->i2c_cmd_dev->kobj, &i2c_cmd_attr_group);
#endif
#endif
#ifdef SCRIPTLESS
ProbeErrorExit_9:
	device_destroy(fts_cmd_class, DCHIP_ID_0);
	class_destroy(fts_cmd_class);
	fts_cmd_class = NULL;
ProbeErrorExit_8:
	sysfs_remove_group(&client->dev.kobj, &info->attrs);
#endif
ProbeErrorExit_7:
#ifdef CONFIG_DRM
	drm_unregister_client(&info->notifier);
#endif
ProbeErrorExit_6:
	input_unregister_device(info->input_dev);
ProbeErrorExit_5_1:
ProbeErrorExit_5:
	destroy_workqueue(info->event_wq);
ProbeErrorExit_4:
	destroy_workqueue(info->fwu_workqueue);
ProbeErrorExit_3_1:
	fts_unset_gpio(info);
ProbeErrorExit_3:
	fts_enable_reg(info, false);
ProbeErrorExit_2:
	fts_get_reg(info, false);
ProbeErrorExit_1:
	kfree(info);
ProbeErrorExit_0:
	log_error("%s Probe Failed!\n", tag);
	return error;
}

static int fts_remove(struct i2c_client *client)
{
	struct fts_ts_info *info = i2c_get_clientdata(client);

	if (info->tp_lockdown_info_proc)
		proc_remove(info->tp_lockdown_info_proc);
	if (info->tp_fw_version_proc)
		proc_remove(info->tp_fw_version_proc);
	if (info->tp_data_dump_proc)
		proc_remove(info->tp_data_dump_proc);
	if (info->tp_selftest_proc)
		proc_remove(info->tp_selftest_proc);
#ifdef CONFIG_FTS_TOUCH_COUNT_DUMP
	if (info->bdata->dump_click_count && !info->current_clicknum_file) {
		kfree(info->current_clicknum_file);
		info->current_clicknum_file = NULL;
	}
#endif
#ifdef CONFIG_FTS_TOUCH_COUNT_DUMP
	sysfs_remove_file(&info->fts_touch_dev->kobj, &dev_attr_touch_suspend_notify.attr);
	device_destroy(info->fts_tp_class, DCHIP_ID_0);
	class_destroy(info->fts_tp_class);
	info->fts_tp_class = NULL;
#endif
#ifdef DRIVER_TEST
	sysfs_remove_group(&info->test_cmd_dev->kobj, &test_cmd_attr_group);
#endif
#ifdef SCRIPTLESS
	sysfs_remove_group(&info->i2c_cmd_dev->kobj, &i2c_cmd_attr_group);
#endif
#if defined(SCRIPTLESS) || defined(DRIVER_TEST)
#ifdef SCRIPTLESS
	device_destroy(fts_cmd_class, DCHIP_ID_0);
#endif
#ifdef DRIVER_TEST
	device_destroy(fts_cmd_class, DCHIP_ID_1);
#endif
	class_destroy(fts_cmd_class);
	fts_cmd_class = NULL;
#endif
	sysfs_remove_group(&client->dev.kobj, &info->attrs);
	fts_interrupt_uninstall(info);
#ifdef CONFIG_DRM
	drm_unregister_client(&info->notifier);
#endif
	input_unregister_device(info->input_dev);
	fts_command(info, FIFO_CMD_FLUSH);
	destroy_workqueue(info->event_wq);
	destroy_workqueue(info->fwu_workqueue);
	fts_unset_gpio(info);
	fts_enable_reg(info, false);
	fts_get_reg(info, false);
	fts_info = NULL;
	kfree(info);
	return OK;
}

static struct of_device_id fts_of_match_table[] = {
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
#ifdef CONFIG_PM
		.pm = &fts_dev_pm_ops,
#endif
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

MODULE_DESCRIPTION("STMicroelectronics MultiTouch IC Driver");
MODULE_AUTHOR("STMicroelectronics, Inc.");
MODULE_LICENSE("GPL");

late_initcall(fts_driver_init);
module_exit(fts_driver_exit);
