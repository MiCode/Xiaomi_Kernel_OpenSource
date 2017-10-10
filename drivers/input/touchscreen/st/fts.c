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
#include <linux/fb.h>

#ifdef KERNEL_ABOVE_2_6_38
#include <linux/input/mt.h>
#endif

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

#define LINK_KOBJ_NAME "tp"

/*
 * Uncomment to use polling mode instead of interrupt mode.
 *
 */
/* #define FTS_USE_POLLING_MODE */

/*
 * Event installer helpers
 */
#define event_id(_e)	 EVENTID_##_e
#define handler_name(_h) fts_##_h##_event_handler

#define install_handler(_i, _evt, _hnd) \
do { \
	_i->event_dispatch_table[event_id(_evt)] = handler_name(_hnd); \
} while (0)

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

extern chipInfo ftsInfo;

unsigned char tune_version_same;

char tag[8] = "[ FTS ]\0";

static u32 *typeOfComand;
static int numberParameters;
static int feature_feasibility = ERROR_OP_NOT_ALLOW;
#ifdef PHONE_GESTURE
static u8 mask[GESTURE_MASK_SIZE+2];
#endif
static void fts_interrupt_enable(struct fts_ts_info *info);
static int fts_init_hw(struct fts_ts_info *info);
static int fts_mode_handler(struct fts_ts_info *info, int force);
static int fts_command(struct fts_ts_info *info, unsigned char cmd);
static void fts_unblank(struct fts_ts_info *info);
static int fts_chip_initialization(struct fts_ts_info *info);

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

/* force update firmware*/
static ssize_t fts_fw_control_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count){
	int ret, mode;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	/* reading out firmware upgrade mode */
	sscanf(buf, "%d", &mode);
#ifdef FTM3_CHIP
	ret = flashProcedure(PATH_FILE_FW, mode, !mode);
#else
	ret = flashProcedure(PATH_FILE_FW, mode, 1);
#endif
	info->fwupdate_stat = ret;

	if (ret < OK)
		logError(1,  "%s  %s :Unable to upgrade firmware\n", tag, __func__);
	return count;
}

static ssize_t fts_sysfs_config_id_show(struct device *dev, struct device_attribute *attr,
		char *buf) {
	int error;

	error = snprintf(buf, TSP_BUF_SIZE, "%x.%x\n", ftsInfo.u16_fwVer, ftsInfo.u16_cfgId);
	return error;
}

static ssize_t fts_sysfs_fwupdate_show(struct device *dev, struct device_attribute *attr,
		char *buf) {
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	/* fwupdate_stat: ERROR code Returned by flashProcedure. */
	return snprintf(buf, TSP_BUF_SIZE, "%08X\n", info->fwupdate_stat);
}

static ssize_t fts_fw_test_show(struct device *dev, struct device_attribute *attr,
		char *buf) {

	Firmware fw;
	int ret;

	fw.data = NULL;
	ret = readFwFile(PATH_FILE_FW, &fw, 0);

	if (ret < OK) {
	logError(1,  "%s: Error during reading FW file! ERROR %08X\n", tag, ret);
	} else
		logError(1,  "%s: fw_version = %04X, config_version = %04X, size = %d bytes\n",
			tag, fw.fw_ver, fw.config_id, fw.data_size);

	kfree(fw.data);
	return 0;
}

/* TODO: edit this function according to the features policy to allow during the screen on/off */
int check_feature_feasibility(struct fts_ts_info *info, unsigned int feature)
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
			logError(1, "%s %s: Feature not allowed in this operating mode! ERROR %08X\n", tag, __func__, res);
		break;

		}
	} else{
		switch (feature) {
#ifdef PHONE_GESTURE
		case FEAT_GESTURE:
#endif
		case FEAT_GLOVE:
		/* glove mode can only activate during sense on */
			res = OK;
		break;

		default:
			logError(1, "%s %s: Feature not allowed in this operating mode! ERROR %08X\n", tag, __func__, res);
		break;

		}
	}

	return res;

}

static ssize_t fts_feature_enable_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count) {
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);
	char *p = (char *)buf;
	unsigned int temp;
	int res = OK;

	if ((count + 1) / 3 != 2) {
		logError(1,  "%s fts_feature_enable: Number of parameter wrong! %d > %d\n",
			tag, (count + 1) / 3, 2);
	} else{
		sscanf(p, "%02X ", &temp);
		p += 3;
		res = check_feature_feasibility(info, temp);
		if (res >= OK) {
		switch (temp) {
#ifdef PHONE_GESTURE
		case FEAT_GESTURE:
			sscanf(p, "%02X ", &info->gesture_enabled);
			logError(1,  "%s fts_feature_enable: Gesture Enabled = %d\n", tag,
				info->gesture_enabled);
			break;
#endif
		case FEAT_GLOVE:
			sscanf(p, "%02X ", &info->glove_enabled);
			logError(1,  "%s fts_feature_enable: Glove Enabled = %d\n",
			tag, info->glove_enabled);

			break;

		default:
			logError(1,  "%s fts_feature_enable: Feature %02X not valid! ERROR %08X\n", tag, temp, ERROR_OP_NOT_ALLOW);

		}
		feature_feasibility = res;
		}

	}
	return count;
}

static ssize_t fts_feature_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char buff[CMD_STR_LEN] = {0};
	int size = 6 * 2;
	u8 *all_strbuff = NULL;
	int count = 0, res;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	if (feature_feasibility >= OK)
		res = fts_mode_handler(info, 1);
	else{
		res = feature_feasibility;
		logError(1,  "%s %s: Call before echo xx xx > feature_enable with a correct feature! ERROR %08X\n", tag, __func__, res);
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

		count = snprintf(buf, TSP_BUF_SIZE, "%s\n", all_strbuff);
		kfree(all_strbuff);
	} else{
		logError(1,  "%s fts_feature_enable_show: Unable to allocate all_strbuff! ERROR %08X\n", tag, ERROR_ALLOC);
	}

	feature_feasibility = ERROR_OP_NOT_ALLOW;
	return count;
}

#ifdef PHONE_GESTURE
static ssize_t fts_gesture_mask_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	char buff[CMD_STR_LEN] = {0};
		int size = 6 * 2;
		u8 *all_strbuff = NULL;
	int count = 0, res;

	if (mask[0] == 0) {
		res = ERROR_OP_NOT_ALLOW;
		logError(1, "%s %s: Call before echo enable/disable xx xx .... > gesture_mask with a correct number of parameters! ERROR %08X\n", tag, __func__, res);

	} else{
		res = fts_disableInterrupt();
		if (res >= OK) {
			if (mask[1] == FEAT_ENABLE)
				res = enableGesture(&mask[2], mask[0]);
			else{
				if (mask[1] == FEAT_DISABLE)
					res = disableGesture(&mask[2], mask[0]);
				else
					res = ERROR_OP_NOT_ALLOW;
			}
			if (res < OK) {
				logError(1,  "%s fts_gesture_mask_store: ERROR %08X\n", tag, res);
			}
		}
		res |= fts_enableInterrupt();
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

		count = snprintf(buf, TSP_BUF_SIZE, "%s\n", all_strbuff);
		kfree(all_strbuff);
	} else{
		logError(1,  "%s fts_gesture_mask_show: Unable to allocate all_strbuff! ERROR %08X\n", tag, ERROR_ALLOC);
	}

	mask[0] = 0;
	return count;
}

static ssize_t fts_gesture_mask_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	char *p = (char *)buf;
	int n;
	unsigned int temp;

	if ((count + 1) / 3 > GESTURE_MASK_SIZE+1) {
		logError(1,  "%s fts_gesture_mask_store: Number of bytes of parameter wrong! %d > (enable/disable + %d )\n", tag, (count + 1) / 3, GESTURE_MASK_SIZE);
		mask[0] = 0;
	} else {
		mask[0] = ((count + 1) / 3) - 1;
		for (n = 1; n <= (count + 1) / 3; n++) {
			sscanf(p, "%02X ", &temp);
			p += 3;
			mask[n] = (u8)temp;
			logError(1,  "%s mask[%d] = %02X\n", tag, n, mask[n]);

		}

	}

	return count;
}
#endif

/************************ PRODUCTION TEST **********************************/
static ssize_t stm_fts_cmd_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count) {
	int n;
	char *p = (char *) buf;

	typeOfComand = (u32 *) kmalloc(8 * sizeof (u32), GFP_KERNEL);
	if (typeOfComand == NULL) {
		logError(1,  "%s impossible to allocate typeOfComand!\n", tag);
		return count;
	}
	memset(typeOfComand, 0, 8 * sizeof (u32));

	logError(1,  "%s\n", tag);
	for (n = 0; n < (count + 1) / 3; n++) {
		sscanf(p, "%02X ", &typeOfComand[n]);
		p += 3;
		logError(1,  "%s typeOfComand[%d] = %02X\n", tag, n, typeOfComand[n]);

	}

	numberParameters = n;
	logError(1,  "%s Number of Parameters = %d\n", tag, numberParameters);
	return count;
}

static ssize_t stm_fts_cmd_show(struct device *dev, struct device_attribute *attr,
		char *buf) {
	char buff[CMD_STR_LEN] = {0};
	int res, j, doClean = 0, count;

	int size = 6 * 2;
	u8 *all_strbuff = NULL;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	MutualSenseData compData;
	SelfSenseData comData;
	MutualSenseFrame frameMS;
	SelfSenseFrame frameSS;

	/*  struct used for defining which test
	 *perform during the production test
	 */
	TestToDo todoDefault;

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

	if (numberParameters >= 1 && typeOfComand != NULL) {
		res = fts_disableInterrupt();
		if (res < 0) {
			logError(0, "%s fts_disableInterrupt: ERROR %08X\n", tag, res);
			res = (res | ERROR_DISABLE_INTER);
			goto END;
		}

	res = fb_unregister_client(&info->notifier);
		if (res < 0) {
			logError(1,  "%s ERROR: unregister notifier failed!\n", tag);
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
				res = production_test_main(LIMITS_FILE, 1, 1, &todoDefault, INIT_MP);
	} else{
	   logError(0, "%s MP Flag set!\n", tag, res);
	   res = production_test_main(LIMITS_FILE, 1, 0, &todoDefault, INIT_MP);
	}
	break;
			/*read mutual raw*/
		case 0x13:
			logError(0, "%s Get 1 MS Frame\n", tag);
			/* res = getMSFrame(ADDR_RAW_TOUCH, &frame, 0); */
			res = getMSFrame2(MS_TOUCH_ACTIVE, &frameMS);
			if (res < 0) {
				logError(0, "%s Error while taking the MS frame... ERROR %02X\n", tag, res);

			} else {
				logError(0, "%s The frame size is %d words\n", tag, res);
				size = (res * sizeof (short) + 8)*2;
				/* set res to OK because if getMSFrame is
				 * successful res = number of words read
				 */
				res = OK;
			}
	break;
	/*read self raw*/
		case 0x15:
			logError(0, "%s Get 1 SS Frame\n", tag);
			res = getSSFrame2(SS_TOUCH, &frameSS);

			if (res < OK) {
				logError(0, "%s Error while taking the SS frame... ERROR %02X\n", tag, res);

			} else {
				logError(0, "%s The frame size is %d words\n", tag, res);
				size = (res * sizeof (short) + 8)*2+1;
				/* set res to OK because if getMSFrame is
				 * successful res = number of words read
				 */
				res = OK;
			}

			break;

		case 0x14: /*read mutual comp data */
			logError(0, "%s Get MS Compensation Data\n", tag);
			res = readMutualSenseCompensationData(MS_TOUCH_ACTIVE, &compData);

			if (res < 0) {
				logError(0, "%s Error reading MS compensation data ERROR %02X\n", tag, res);
			} else {
				logError(0, "%s MS Compensation Data Reading Finished!\n", tag);
				size = ((compData.node_data_size + 9) * sizeof (u8))*2;
			}
			break;

		case 0x16: /* read self comp data */
			logError(0, "%s Get SS Compensation Data...\n", tag);
			res = readSelfSenseCompensationData(SS_TOUCH, &comData);
			if (res < 0) {
				logError(0, "%s Error reading SS compensation data ERROR %02X\n", tag, res);
			} else {
				logError(0, "%s SS Compensation Data Reading Finished!\n", tag);
				size = ((comData.header.force_node + comData.header.sense_node)*2 + 12) * sizeof (u8)*2;
			}
			break;

		case 0x03: /* MS Raw DATA TEST */
			res = fts_system_reset();
			if (res >= OK)
				res = production_test_ms_raw(LIMITS_FILE, 1, &todoDefault);
			break;

		case 0x04: /* MS CX DATA TEST */
			res = fts_system_reset();
			if (res >= OK)
				res = production_test_ms_cx(LIMITS_FILE, 1, &todoDefault);
			break;

		case 0x05: /* SS RAW DATA TEST */
			res = fts_system_reset();
			if (res >= OK)
				res = production_test_ss_raw(LIMITS_FILE, 1, &todoDefault);
			break;

		case 0x06: /* SS IX CX DATA TEST */
			res = fts_system_reset();
			if (res >= OK)
				res = production_test_ss_ix_cx(LIMITS_FILE, 1, &todoDefault);
			break;

		case 0xF0:
		case 0xF1: /* TOUCH ENABLE/DISABLE */
			doClean = (int) (typeOfComand[0]&0x01);
			res = cleanUp(doClean);

			break;

		default:
			logError(1,  "%s COMMAND NOT VALID!! Insert a proper value ...\n", tag);
			res = ERROR_OP_NOT_ALLOW;
			break;
		}

		doClean = fts_enableInterrupt();
		if (doClean < 0) {
			logError(0, "%s fts_enableInterrupt: ERROR %08X\n", tag, (doClean|ERROR_ENABLE_INTER));
		}
	} else {
		logError(1,  "%s NO COMMAND SPECIFIED!!! do: 'echo [cmd_code] [args] > stm_fts_cmd' before looking for result!\n", tag);
		res = ERROR_OP_NOT_ALLOW;
		typeOfComand = NULL;

	}

		if (fb_register_client(&info->notifier) < 0) {
			logError(1,  "%s ERROR: register notifier failed!\n", tag);
		}

END: /* here start the reporting phase, assembling the data to send in the file node */
	all_strbuff = (u8 *) kmalloc(size, GFP_KERNEL);
	memset(all_strbuff, 0, size);

	snprintf(buff, sizeof(buff), "%02X", 0xAA);
	strlcat(all_strbuff, buff, size);

	snprintf(buff, sizeof(buff), "%08X", res);
	strlcat(all_strbuff, buff, size);

	if (res >= OK) {
		/*all the other cases are already fine printing only the res.*/
		switch (typeOfComand[0]) {
		case 0x13:
			snprintf(buff, sizeof (buff), "%02X", (u8) frameMS.header.force_node);
			strlcat(all_strbuff, buff, size);

		snprintf(buff, sizeof(buff), "%02X", (u8) frameMS.header.sense_node);
		strlcat(all_strbuff, buff, size);

		for (j = 0; j < frameMS.node_data_size; j++) {
			snprintf(buff, sizeof(buff), "%04X", frameMS.node_data[j]);
			strlcat(all_strbuff, buff, size);
		}

		kfree(frameMS.node_data);
		break;

		case 0x15:
			snprintf(buff, sizeof(buff), "%02X", (u8) frameSS.header.force_node);
			strlcat(all_strbuff, buff, size);

			snprintf(buff, sizeof(buff), "%02X", (u8) frameSS.header.sense_node);
			strlcat(all_strbuff, buff, size);

			/* Copying self raw data Force */
			for (j = 0; j < frameSS.header.force_node; j++) {
				snprintf(buff, sizeof(buff), "%04X", frameSS.force_data[j]);
				strlcat(all_strbuff, buff, size);
			}

			/* Copying self raw data Sense */
			for (j = 0; j < frameSS.header.sense_node; j++) {
				snprintf(buff, sizeof(buff), "%04X", frameSS.sense_data[j]);
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

			/* Cpying CX1 value */
			snprintf(buff, sizeof(buff), "%02X", compData.cx1);
			strlcat(all_strbuff, buff, size);

			/* Copying CX2 values */
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

			/* Copying IX2 Force */
			for (j = 0; j < comData.header.force_node; j++) {
				snprintf(buff, sizeof(buff), "%02X", comData.ix2_fm[j]);
				strlcat(all_strbuff, buff, size);
			}

			/* Copying IX2 Sense */
			for (j = 0; j < comData.header.sense_node; j++) {
				snprintf(buff, sizeof(buff), "%02X", comData.ix2_sn[j]);
				strlcat(all_strbuff, buff, size);
			}

			/* Copying CX2 Force */
			for (j = 0; j < comData.header.force_node; j++) {
				snprintf(buff, sizeof(buff), "%02X", comData.cx2_fm[j]);
				strlcat(all_strbuff, buff, size);
			}

		/* Copying CX2 Sense */
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
			break;

		}
	}

	snprintf(buff, sizeof(buff), "%02X", 0xBB);
	strlcat(all_strbuff, buff, size);

	count = snprintf(buf, TSP_BUF_SIZE, "%s\n", all_strbuff);
	numberParameters = 0;  /* need to reset the number of parameters
				* in order to wait the next command, comment
				*if you want to repeat the last command sent
				*just doing a cat
				*/
	/* logError(0,"%s numberParameters = %d\n", tag, numberParameters); */
	kfree(all_strbuff);

	kfree(typeOfComand);
	return count;

}

static DEVICE_ATTR(fwupdate, (S_IRUGO | S_IWUSR | S_IWGRP), fts_sysfs_fwupdate_show, fts_fw_control_store);
static DEVICE_ATTR(appid, (S_IRUGO), fts_sysfs_config_id_show, NULL);
static DEVICE_ATTR(fw_file_test, (S_IRUGO), fts_fw_test_show, NULL);
static DEVICE_ATTR(stm_fts_cmd, (S_IRUGO | S_IWUSR | S_IWGRP), stm_fts_cmd_show, stm_fts_cmd_store);
static DEVICE_ATTR(feature_enable, (S_IRUGO | S_IWUSR | S_IWGRP), fts_feature_enable_show, fts_feature_enable_store);
#ifdef PHONE_GESTURE
static DEVICE_ATTR(gesture_mask, (S_IRUGO | S_IWUSR | S_IWGRP), fts_gesture_mask_show, fts_gesture_mask_store);
#endif
/*  /sys/devices/soc.0/f9928000.i2c/i2c-6/6-0049 */
static struct attribute *fts_attr_group[] = {
	&dev_attr_fwupdate.attr,
	&dev_attr_appid.attr,
	&dev_attr_fw_file_test.attr,
	/* &dev_attr_touch_debug.attr, */
	&dev_attr_stm_fts_cmd.attr,
	&dev_attr_feature_enable.attr,
#ifdef PHONE_GESTURE
	&dev_attr_gesture_mask.attr,
#endif
	NULL,
};

static int fts_command(struct fts_ts_info *info, unsigned char cmd)
{
	unsigned char regAdd;
	int ret;

	regAdd = cmd;

	ret = fts_writeCmd(&regAdd, sizeof (regAdd)); /* 0 = ok */

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
static unsigned char *fts_nop_event_handler(struct fts_ts_info *info,
		unsigned char *event) {
	/*  logError(1,  "%s %s Doing nothing for event = %02X %02X %02X %02X %02X %02X %02X %02X\n",
	* tag, __func__, event[0], event[1], event[2], event[3], event[4], event[5], event[6], event[7]);
	*/
	return fts_next_event(event);
}

/* EventId : 0x03 */
static unsigned char *fts_enter_pointer_event_handler(struct fts_ts_info *info,
		unsigned char *event) {
	unsigned char touchId, touchcount;
	int x, y, z;

	if (!info->resume_bit)
		goto no_report;

	touchId = event[1] & 0x0F;
	touchcount = (event[1] & 0xF0) >> 4;

	__set_bit(touchId, &info->touch_id);

	x = (event[2] << 4) | (event[4] & 0xF0) >> 4;
	y = (event[3] << 4) | (event[4] & 0x0F);
	z = (event[5] & 0x3F);

	if (x == X_AXIS_MAX)
		x--;

	if (y == Y_AXIS_MAX)
		y--;

	input_mt_slot(info->input_dev, touchId);
	input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 1);
	logError(0, "%s  %s : TouchID = %d,Touchcount = %d\n", tag, __func__, touchId, touchcount);
	if (touchcount == 1) {
		input_report_key(info->input_dev, BTN_TOUCH, 1);
		input_report_key(info->input_dev, BTN_TOOL_FINGER, 1);
	}
	/* input_report_abs(info->input_dev, ABS_MT_TRACKING_ID, touchId); */
	input_report_abs(info->input_dev, ABS_MT_POSITION_X, x);
	input_report_abs(info->input_dev, ABS_MT_POSITION_Y, y);
	input_report_abs(info->input_dev, ABS_MT_TOUCH_MAJOR, z);
	input_report_abs(info->input_dev, ABS_MT_TOUCH_MINOR, z);
	input_report_abs(info->input_dev, ABS_MT_PRESSURE, z);
	logError(0, "%s  %s :  Event 0x%02x - ID[%d], (x, y, z) = (%3d, %3d, %3d)\n", tag, __func__, *event, touchId, x, y, z);

no_report:
	return fts_next_event(event);
}

/* EventId : 0x04 */
static unsigned char *fts_leave_pointer_event_handler(struct fts_ts_info *info,
		unsigned char *event) {
	unsigned char touchId, touchcount;

	touchId = event[1] & 0x0F;
	touchcount = (event[1] & 0xF0) >> 4;

	__clear_bit(touchId, &info->touch_id);

	input_mt_slot(info->input_dev, touchId);
	input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 0);
	logError(0, "%s  %s : TouchID = %d, Touchcount = %d\n", tag, __func__, touchId, touchcount);
	if (touchcount == 0) {
		input_report_key(info->input_dev, BTN_TOUCH, 0);
		input_report_key(info->input_dev, BTN_TOOL_FINGER, 0);
	}

	input_report_abs(info->input_dev, ABS_MT_TRACKING_ID, -1);
	logError(0, "%s  %s : Event 0x%02x - release ID[%d]\n", tag, __func__, event[0], touchId);

	return fts_next_event(event);
}

/* EventId : 0x05 */
#define fts_motion_pointer_event_handler fts_enter_pointer_event_handler

#ifdef PHONE_KEY
/* EventId : 0x0E */
static unsigned char *fts_key_status_event_handler(struct fts_ts_info *info, unsigned char *event)
{
	int value;
	logError(0, "%s %s Received event %02X %02X %02X %02X %02X %02X %02X %02X\n", tag, __func__, event[0], event[1], event[2], event[3], event[4], event[5], event[6], event[7]);
	/* TODO: the customer should handle the events coming from the keys according his needs (this is an example that report only the single pressure of one key at time) */
	if (event[2] != 0) {			/* event[2] contain the bitmask of the keys that are actually pressed */
		switch (event[2]) {
		case KEY1:
			value = KEY_HOMEPAGE;
			logError(0, "%s %s: Button HOME !\n", tag, __func__);
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
			logError(0, "%s %s:  No valid Button ID or more than one key pressed!\n", tag, __func__);
			goto done;
		}

			fts_input_report_key(info, value);
	} else{
		logError(0, "%s %s: All buttons released!\n", tag, __func__);
	}
done:
	return fts_next_event(event);
}
#endif

/* EventId : 0x0F */
static unsigned char *fts_error_event_handler(struct fts_ts_info *info,
		unsigned char *event) {
	int error = 0, i = 0;
	logError(0, "%s %s Received event 0x%02x 0x%02x\n", tag, __func__, event[0], event[1]);

	switch (event[1]) {
	case EVENT_TYPE_ESD_ERROR:
	{
		for (i = 0; i < TOUCH_ID_MAX; i++) {
			input_mt_slot(info->input_dev, i);
			input_mt_report_slot_state(info->input_dev, (i < FINGER_MAX) ? MT_TOOL_FINGER : MT_TOOL_PEN, 0);
		}
		input_sync(info->input_dev);

		fts_chip_powercycle(info);

	error = fts_system_reset();
	error |= fts_mode_handler(info, 0);
	error |= fts_enableInterrupt();
		if (error < OK) {
			logError(1,  "%s %s Cannot restore the device ERROR %08X\n", tag, __func__, error);
		}
	}
	break;
	case EVENT_TYPE_WATCHDOG_ERROR: /* watch dog timer */
	{
		if (event[2] == 0) {
			/* before reset clear all slot */
			for (i = 0; i < TOUCH_ID_MAX; i++) {
				input_mt_slot(info->input_dev, i);
				input_mt_report_slot_state(info->input_dev,
						(i < FINGER_MAX) ? MT_TOOL_FINGER : MT_TOOL_PEN, 0);
			}
			input_sync(info->input_dev);
			error = fts_system_reset();
			error |= fts_mode_handler(info, 0);
			error |= fts_enableInterrupt();
			if (error < OK) {
				logError(1,  "%s %s Cannot reset the device ERROR %08X\n", tag, __func__, error);
			}
		}
	}
	break;

	}
	return fts_next_event(event);
}

/* EventId : 0x10 */
static unsigned char *fts_controller_ready_event_handler(
		struct fts_ts_info *info, unsigned char *event) {
	int error;
	logError(0, "%s %s Received event 0x%02x\n", tag, __func__, event[0]);
	info->touch_id = 0;
	input_sync(info->input_dev);
	setSystemResettedUp(1);
	setSystemResettedDown(1);
	error = fts_mode_handler(info, 0);
	if (error < OK) {
		  logError(1,  "%s %s Cannot restore the device status ERROR %08X\n", tag, __func__, error);
	}
	return fts_next_event(event);
}

/* EventId : 0x16 */
static unsigned char *fts_status_event_handler(
		struct fts_ts_info *info, unsigned char *event) {
	/* logError(1,  "%s Received event 0x%02x\n", tag, event[0]); */

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
		logError(0,
			"%s %s Received unhandled status event = %02X %02X %02X %02X %02X %02X %02X %02X\n",
			tag, __func__, event[0], event[1], event[2],
			event[3], event[4], event[5], event[6], event[7]);
	break;
	}

	return fts_next_event(event);
}

#ifdef PHONE_GESTURE
static unsigned char *fts_gesture_event_handler(struct fts_ts_info *info, unsigned char *event)
{
	unsigned char touchId;
	int value;

	logError(0, "%s  gesture  event	data: %02X %02X %02X %02X %02X %02X %02X %02X\n", tag, event[0], event[1], event[2], event[3], event[4], event[5], event[6], event[7]);

	if (event[1] == 0x03) {

		logError(1,  "%s %s: Gesture ID %02X enable_status = %02X\n", tag, __func__, event[2], event[3]);

	}
	if (event[1] == EVENT_TYPE_ENB && event[2] == 0x00) {
		switch (event[3]) {
		case GESTURE_ENABLE:
			logError(1,  "%s %s: Gesture Enabled! res = %02X\n", tag, __func__, event[4]);
			break;

		case GESTURE_DISABLE:
			logError(1,  "%s %s: Gesture Disabled! res = %02X\n", tag, __func__, event[4]);
			break;

		default:
			logError(1,  "%s %s: Event not Valid!\n", tag, __func__);

		}

	}

	/* always use touchId zero */
	touchId = 0;
	__set_bit(touchId, &info->touch_id);

	if (event[0] == EVENTID_GESTURE && (event[1] == EVENT_TYPE_GESTURE_DTC1 || event[1] == EVENT_TYPE_GESTURE_DTC2)) {

		switch (event[2]) {
		case GES_ID_DBLTAP:
			value = KEY_WAKEUP;
			logError(0, "%s %s: double tap !\n", tag, __func__);
		break;

		case GES_ID_AT:
			value = KEY_WWW;
			logError(0, "%s %s: @ !\n", tag, __func__);
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
			logError(0, "%s %s:  No valid GestureID!\n", tag, __func__);
			goto gesture_done;

		}

		fts_input_report_key(info, value);

	gesture_done:
		/* Done with gesture event, clear bit. */
		__clear_bit(touchId, &info->touch_id);
	}

	return fts_next_event(event);
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
	int error, error1;
	int left_events;
	unsigned char regAdd;
	unsigned char data[FIFO_EVENT_SIZE * (FIFO_DEPTH)] = {0};
	unsigned char *event = NULL;
	unsigned char eventId;
	event_dispatch_handler_t event_handler;

	info = container_of(work, struct fts_ts_info, work);
	/*
	 * to avoid reading all FIFO, we read the first event and
	 * then check how many events left in the FIFO
	 */


	regAdd = FIFO_CMD_READONE;
	error = fts_readCmd(&regAdd,
			sizeof (regAdd), data, FIFO_EVENT_SIZE);

	if (!error) {

		left_events = data[7] & 0x1F;
		if ((left_events > 0) && (left_events < FIFO_DEPTH)) {
			/*
			 * Read remaining events.
			 */
			regAdd = FIFO_CMD_READALL;

			error1 = fts_readCmd(&regAdd, sizeof (regAdd),
					&data[FIFO_EVENT_SIZE],
					left_events * FIFO_EVENT_SIZE);
			/*
			 * Got an error reading remaining events,
			 * process at least * the first one that was
			 * reading fine.
			 */
			if (error1)
				data[7] &= 0xE0;
		}

		/* At least one event is available */
		event = data;
		do {
			eventId = *event;
			event_handler = info->event_dispatch_table[eventId];

			if (eventId < EVENTID_LAST) {
				event = event_handler(info, (event));
			} else {
				event = fts_next_event(event);
			}
			input_sync(info->input_dev);
		} while (event);
	}

	/*
	 * re-enable interrupts
	 */
	fts_interrupt_enable(info);
}

static int cx_crc_check(void)
{
	unsigned char regAdd1[3] = {FTS_CMD_HW_REG_R, ADDR_CRC_BYTE0, ADDR_CRC_BYTE1};
	unsigned char val = 0;
	unsigned char crc_status;
	unsigned int error;

	error = fts_readCmd(regAdd1, sizeof (regAdd1), &val, 1);
	if (error < OK) {
		logError(1,  "%s %s Cannot read crc status ERROR %08X\n", tag, __func__, error);
		return error;
	}

	crc_status = val & CRC_MASK;
	if (crc_status != OK) { /* CRC error if crc_status!= 0 */
		logError(1,  "%s %s CRC ERROR = %X\n", tag, __func__, crc_status);
	}

	return crc_status; /* return OK if no CRC error, or a number >OK if crc error */
}

static void fts_fw_update_auto(struct work_struct *work)
{
	int retval = 0;
	int retval1 = 0;
	int ret;
	struct fts_ts_info *info;
	struct delayed_work *fwu_work = container_of(work, struct delayed_work, work);
	int crc_status = 0;
	int error = 0;
	info = container_of(fwu_work, struct fts_ts_info, fwu_work);

	logError(1,  "%s Fw Auto Update is starting...\n", tag);

	/* check CRC status */
	ret = cx_crc_check();
	if (ret > OK && ftsInfo.u16_fwVer == 0x0000) {
		logError(1,  "%s %s: CRC Error or NO FW!\n", tag, __func__);
		crc_status = 1;
	} else {
		crc_status = 0;
		logError(1,  "%s %s: NO CRC Error or Impossible to read CRC register!\n", tag, __func__);
	}
#ifdef FTM3_CHIP
	retval = flashProcedure(PATH_FILE_FW, crc_status, !crc_status);
#else
	retval = flashProcedure(PATH_FILE_FW, crc_status, 1);
#endif
	if ((retval & 0xFF000000) == ERROR_FLASH_PROCEDURE) {
		logError(1,  "%s %s: firmware update failed and retry! ERROR %08X\n", tag, __func__, retval);
		fts_chip_powercycle(info); /* power reset */
#ifdef FTM3_CHIP
		retval1 = flashProcedure(PATH_FILE_FW, crc_status, !crc_status);
#else
		retval1 = flashProcedure(PATH_FILE_FW, crc_status, 1);
#endif
		if ((retval1 & 0xFF000000) == ERROR_FLASH_PROCEDURE) {
			logError(1,  "%s %s: firmware update failed again!  ERROR %08X\n", tag, __func__, retval1);
			logError(1,  "%s Fw Auto Update Failed!\n", tag);
			/* return; */
		}
	}

	if ((ftsInfo.u32_mpPassFlag != INIT_MP) && (ftsInfo.u32_mpPassFlag != INIT_FIELD))
		ret = ERROR_GET_INIT_STATUS;
	else
		ret = OK;

	if (ret == ERROR_GET_INIT_STATUS) { /* initialization status not correct or after FW complete update, do initialization. */
		error = fts_chip_initialization(info);
		if (error < OK) {
			logError(1,  "%s %s Cannot initialize the chip ERROR %08X\n", tag, __func__, error);
		}
	}
		error = fts_init_hw(info);
		if (error < OK) {
			logError(1,  "%s Cannot initialize the hardware device ERROR %08X\n", tag, error);
		}

	logError(1,  "%s Fw Auto Update Finished!\n", tag);
}

static int fts_chip_initialization(struct fts_ts_info *info)
{
	int ret2 = 0;
	int retry;
	int initretrycnt = 0;

	/* initialization error, retry initialization */
	for (retry = 0; retry <= INIT_FLAG_CNT; retry++) {
		ret2 = production_test_initialization();
		if (ret2 == OK) {
				ret2 = save_mp_flag(INIT_FIELD);
		if (ret2 == OK)
			break;
	}
		initretrycnt++;
		logError(1,  "%s initialization cycle count = %04d - ERROR %08X\n", tag, initretrycnt, ret2);
	fts_chip_powercycle(info);
	}
	if (ret2 < OK) { /* initialization error */
		logError(1,  "%s fts initialization failed 3 times\n", tag);
	}

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

	info->event_dispatch_table = kzalloc(
			sizeof (event_dispatch_handler_t) * EVENTID_LAST, GFP_KERNEL);

	if (!info->event_dispatch_table) {
		logError(1,  "%s OOM allocating event dispatch table\n", tag);
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

	/* disable interrupts in any case */
	error = fts_disableInterrupt();

#ifdef FTS_USE_POLLING_MODE
	logError(1,  "%s Polling Mode\n");
	hrtimer_init(&info->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	info->timer.function = fts_timer_func;
	hrtimer_start(&info->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
#else
	logError(1,  "%s Interrupt Mode\n", tag);
	if (request_irq(info->client->irq, fts_interrupt_handler,
			IRQF_TRIGGER_LOW, info->client->name,
			info)) {
		logError(1,  "%s Request irq failed\n", tag);
		kfree(info->event_dispatch_table);
		error = -EBUSY;
	} /*else {
		error = fts_enableInterrupt();
	}*/
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
	hrtimer_start(&info->timer,
			ktime_set(0, 10000000), HRTIMER_MODE_REL);
#else
	enable_irq(info->client->irq);
#endif
}

/*
static void fts_interrupt_disable(struct fts_ts_info *info)
{
#ifdef FTS_USE_POLLING_MODE
	hrtimer_cancel(&info->timer);
#else
	disable_irq(info->client->irq);
#endif
}
*/

static int fts_init(struct fts_ts_info *info)
{
	int error;

	error = fts_system_reset();
	if (error < OK && error != (ERROR_TIMEOUT | ERROR_SYSTEM_RESET_FAIL)) {
		logError(1,  "%s Cannot reset the device! ERROR %08X\n", tag, error);
		return error;
	}
	if (error == (ERROR_TIMEOUT | ERROR_SYSTEM_RESET_FAIL)) {
		logError(1,  "%s Setting default Chip INFO!\n", tag);
		defaultChipInfo(0);
	} else {
		error = readChipInfo(0);			/* system reset OK */
		if (error < OK) {
			 logError(1,  "%s Cannot read Chip Info! ERROR %08X\n", tag, error);
			}
	}

	error = fts_interrupt_install(info);

	if (error != OK) {
		logError(1,  "%s Init (1) error (ERROR  = %08X)\n", error);
		return error;
	}

	fts_unblank(info);

	return error;
}

int fts_chip_powercycle(struct fts_ts_info *info)
{
	int error, i;

	logError(1,  "%s %s: Power Cycle Starting...\n", tag, __func__);
	if (info->pwr_reg) {
		error = regulator_disable(info->pwr_reg);
		if (error < 0) {
			logError(1,  "%s %s: Failed to disable DVDD regulator\n", tag, __func__);
		}
	}

	if (info->bus_reg) {
		error = regulator_disable(info->bus_reg);
		if (error < 0) {
			logError(1,  "%s %s: Failed to disable AVDD regulator\n", tag, __func__);
		}
	}

	if (info->bdata->reset_gpio != GPIO_NOT_DEFINED)
		gpio_set_value(info->bdata->reset_gpio, 0);

	msleep(300);
	if (info->pwr_reg) {
		error = regulator_enable(info->bus_reg);
		if (error < 0) {
			logError(1,  "%s %s: Failed to enable AVDD regulator\n", tag, __func__);
		}
	}

	if (info->bus_reg) {
		error = regulator_enable(info->pwr_reg);
		if (error < 0) {
			logError(1,  "%s %s: Failed to enable DVDD regulator\n", tag, __func__);
		}
	}
	msleep(300); /* time needed by the regulators for reaching the regime values */

	if (info->bdata->reset_gpio != GPIO_NOT_DEFINED) {
		msleep(10); /* time to wait before bring up the reset gpio after the power up of the regulators */
		gpio_set_value(info->bdata->reset_gpio, 1);
		/* msleep(300); */
	}

	/* before reset clear all slot */
	for (i = 0; i < TOUCH_ID_MAX; i++) {
		input_mt_slot(info->input_dev, i);
		input_mt_report_slot_state(info->input_dev,
				(i < FINGER_MAX) ? MT_TOOL_FINGER : MT_TOOL_PEN, 0);
	}
	input_sync(info->input_dev);

	logError(1,  "%s %s: Power Cycle Finished! ERROR CODE = %08x\n", tag, __func__, error);
	setSystemResettedUp(1);
	setSystemResettedDown(1);
	return error;
}

int fts_chip_powercycle2(struct fts_ts_info *info, unsigned long sleep)
{
	int error, i;

	logError(1,  "%s %s: Power Cycle Starting...\n", tag, __func__);
	if (info->pwr_reg) {
		error = regulator_disable(info->pwr_reg);
		if (error < 0) {
			logError(1,  "%s %s: Failed to disable DVDD regulator\n", tag, __func__);
		}
	}

	if (info->bus_reg) {
		error = regulator_disable(info->bus_reg);
		if (error < 0) {
			logError(1,  "%s %s: Failed to disable AVDD regulator\n", tag, __func__);
		}
	}

	if (info->bdata->reset_gpio != GPIO_NOT_DEFINED)
		gpio_set_value(info->bdata->reset_gpio, 0);

	msleep(sleep);
	if (info->pwr_reg) {
		error = regulator_enable(info->bus_reg);
		if (error < 0) {
			logError(1,  "%s %s: Failed to enable AVDD regulator\n", tag, __func__);
		}
	}

	if (info->bus_reg) {
		error = regulator_enable(info->pwr_reg);
		if (error < 0) {
			logError(1,  "%s %s: Failed to enable DVDD regulator\n", tag, __func__);
		}
	}
	msleep(500); /*time needed by the regulators for reaching the regime values */

	if (info->bdata->reset_gpio != GPIO_NOT_DEFINED) {
		msleep(10); /*time to wait before bring up the reset gpio after the power up of the regulators */
		gpio_set_value(info->bdata->reset_gpio, 1);
		/* msleep(300); */
	}

	/* before reset clear all slot */
	for (i = 0; i < TOUCH_ID_MAX; i++) {
		input_mt_slot(info->input_dev, i);
		input_mt_report_slot_state(info->input_dev,
				(i < FINGER_MAX) ? MT_TOOL_FINGER : MT_TOOL_PEN, 0);
	}
	input_sync(info->input_dev);

	logError(1,  "%s %s: Power Cycle Finished! ERROR CODE = %08x\n", tag, __func__, error);
	setSystemResettedUp(1);
	setSystemResettedDown(1);
	return error;
}

static int fts_init_hw(struct fts_ts_info *info)
{
	int error = 0;

	error = cleanUp(1);
	if (error < OK)
		logError(1,  "%s Init (2) error (ERROR = %08X)\n", tag, error);

	info->mode = MODE_NORMAL;

	return error;
}

	/*
	* TODO: change this function according with the needs of
	*customer in temrs of feature to enable/disable
	*/
static int fts_mode_handler(struct fts_ts_info *info, int force)
{
	int res = OK;
	int ret = OK;

	logError(0, "%s %s: Mode Handler starting...\n", tag, __func__);
	switch (info->resume_bit) {
	case 0:/* screen down */
	logError(0, "%s %s: Screen OFF...\n", tag, __func__);
#ifdef PHONE_GESTURE
	if (info->gesture_enabled == 1) {
		logError(0, "%s %s: enter in gesture mode !\n", tag, __func__);
		res = enterGestureMode(isSystemResettedDown());
		if (res >= OK) {

			info->mode = MODE_GESTURE;
			/* return OK; */
		} else {
			logError(1,  "%s %s: enterGestureMode failed! ERROR %08X recovery in senseOff...\n", tag, __func__, res);
		}
	}
#endif
	if (info->mode != MODE_GESTURE || info->gesture_enabled == 0) {
	logError(0, "%s %s: Sense OFF!\n", tag, __func__);
	res |= fts_command(info, FTS_CMD_MS_MT_SENSE_OFF);		/* we need to use fts_command for speed reason (no need to check echo in this case and interrupt can be enabled) */
#ifdef PHONE_KEY
	logError(0, "%s %s: Key OFF!\n", tag, __func__);
	res |= fts_command(info, FTS_CMD_MS_KEY_OFF);
#endif

	info->mode = MODE_SENSEOFF;

	}
	setSystemResettedDown(0);
	break;

	case 1:	/* screen up */
		logError(0, "%s %s: Screen ON...\n", tag, __func__);
		logError(0, "%s %s: Sense ON!\n", tag, __func__);
		res |= fts_command(info, FTS_CMD_MS_MT_SENSE_ON);
#ifdef PHONE_KEY
		logError(0, "%s %s: Key ON!\n", tag, __func__);
		res |= fts_command(info, FTS_CMD_MS_KEY_ON);
#endif
		info->mode = MODE_NORMAL;

		if (info->glove_enabled == FEAT_ENABLE || force == 1) {
			if (isSystemResettedUp() || force == 1) {
				logError(0, "%s %s: Glove Mode setting...\n", tag, __func__);
				ret = featureEnableDisable(info->glove_enabled, FEAT_GLOVE);
				if (ret < OK) {
					logError(1,  "%s %s: error during setting GLOVE_MODE! ERROR %08X\n", tag, __func__, ret);
				}
				res |= ret;
			}
			if (ret >= OK && info->glove_enabled == FEAT_ENABLE) {
					info->mode = MODE_GLOVE;
					logError(1,  "%s %s: GLOVE_MODE Enabled!\n", tag, __func__);
			} else{
				logError(1,  "%s %s: GLOVE_MODE Disabled!\n", tag, __func__);
			}
		}

	setSystemResettedUp(0);
	break;

	default:
			logError(1,  "%s %s: invalid resume_bit value = %d! ERROR %08X\n", tag, __func__, info->resume_bit, ERROR_OP_NOT_ALLOW);
			res = ERROR_OP_NOT_ALLOW;
	}

	logError(0, "%s %s: Mode Handler finished! res = %08X\n", tag, __func__, res);
	return res;

}

static int fts_fb_state_chg_callback(struct notifier_block *nb, unsigned long val, void *data)
{
	struct fts_ts_info *info = container_of(nb, struct fts_ts_info, notifier);
	struct fb_event *evdata = data;
	int i;
	unsigned int blank;

	if (val != FB_EVENT_BLANK)
		return 0;

	logError(0, "%s %s: fts notifier begin!\n", tag, __func__);

	if (evdata && evdata->data && val == FB_EVENT_BLANK && info) {

		blank = *(int *) (evdata->data);

		switch (blank) {
		case FB_BLANK_POWERDOWN:
			if (info->sensor_sleep)
				break;

			logError(0, "%s %s: FB_BLANK_POWERDOWN\n", tag, __func__);

			/* Release all slots */
			for (i = 0; i < TOUCH_ID_MAX; i++) {
				input_mt_slot(info->input_dev, i);
				input_mt_report_slot_state(info->input_dev,
					(i < FINGER_MAX) ? MT_TOOL_FINGER : MT_TOOL_PEN, 0);
			}
			input_sync(info->input_dev);

			info->resume_bit = 0;

			fts_mode_handler(info, 0);

			info->sensor_sleep = true;

			fts_disableInterrupt();

			break;

		case FB_BLANK_UNBLANK:
			if (!info->sensor_sleep)
				break;

			logError(0, "%s %s: FB_BLANK_UNBLANK\n", tag, __func__);

			for (i = 0; i < TOUCH_ID_MAX; i++) {
				input_mt_slot(info->input_dev, i);
				input_mt_report_slot_state(info->input_dev,
						(i < FINGER_MAX) ? MT_TOOL_FINGER : MT_TOOL_PEN, 0);
			}
			input_sync(info->input_dev);

			info->resume_bit = 1;

			fts_mode_handler(info, 0);

			info->sensor_sleep = false;

			fts_enableInterrupt();
			break;
		default:
			break;

		}
	}
	return NOTIFY_OK;

}

static void fts_unblank(struct fts_ts_info *info)
{
	int i;

	for (i = 0; i < TOUCH_ID_MAX; i++) {
		input_mt_slot(info->input_dev, i);
		input_mt_report_slot_state(info->input_dev,
			(i < FINGER_MAX) ? MT_TOOL_FINGER : MT_TOOL_PEN, 0);
	}
	input_sync(info->input_dev);

	info->resume_bit = 1;

	fts_mode_handler(info, 0);

	info->sensor_sleep = false;

	fts_enableInterrupt();
}

static struct notifier_block fts_noti_block = {
	.notifier_call = fts_fb_state_chg_callback,
};

static int fts_get_reg(struct fts_ts_info *rmi4_data,
		bool get) {
	int retval;
	const struct fts_i2c_platform_data *bdata =
			rmi4_data->bdata;

	if (!get) {
		retval = 0;
		goto regulator_put;
	}

	if ((bdata->pwr_reg_name != NULL) && (*bdata->pwr_reg_name != 0)) {
		rmi4_data->pwr_reg = regulator_get(rmi4_data->dev,
				bdata->pwr_reg_name);
		if (IS_ERR(rmi4_data->pwr_reg)) {
			logError(1,  "%s %s: Failed to get power regulator\n", tag,
					__func__);
			retval = PTR_ERR(rmi4_data->pwr_reg);
			goto regulator_put;
		}
	}

	if ((bdata->bus_reg_name != NULL) && (*bdata->bus_reg_name != 0)) {
		rmi4_data->bus_reg = regulator_get(rmi4_data->dev,
				bdata->bus_reg_name);
		if (IS_ERR(rmi4_data->bus_reg)) {
			logError(1,  "%s %s: Failed to get bus pullup regulator\n", tag,
					__func__);
			retval = PTR_ERR(rmi4_data->bus_reg);
			goto regulator_put;
		}
	}

	return 0;

regulator_put:
	if (rmi4_data->pwr_reg) {
		regulator_put(rmi4_data->pwr_reg);
		rmi4_data->pwr_reg = NULL;
	}

	if (rmi4_data->bus_reg) {
		regulator_put(rmi4_data->bus_reg);
		rmi4_data->bus_reg = NULL;
	}

	return retval;
}

static int fts_enable_reg(struct fts_ts_info *rmi4_data,
		bool enable) {
	int retval;

	if (!enable) {
		retval = 0;
		goto disable_pwr_reg;
	}

	if (rmi4_data->bus_reg) {
		retval = regulator_enable(rmi4_data->bus_reg);
		if (retval < 0) {
			logError(1,  "%s %s: Failed to enable bus regulator\n", tag,
					__func__);
			goto exit;
		}
	}

	if (rmi4_data->pwr_reg) {
		retval = regulator_enable(rmi4_data->pwr_reg);
		if (retval < 0) {
			logError(1,  "%s %s: Failed to enable power regulator\n", tag,
					__func__);
			goto disable_bus_reg;
		}
	}

	return OK;

disable_pwr_reg:
	if (rmi4_data->pwr_reg)
		regulator_disable(rmi4_data->pwr_reg);

disable_bus_reg:
	if (rmi4_data->bus_reg)
		regulator_disable(rmi4_data->bus_reg);

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
			logError(1,  "%s %s: Failed to get gpio %d (code: %d)", tag,
					__func__, gpio, retval);
			return retval;
		}

		if (dir == 0)
			retval = gpio_direction_input(gpio);
		else
			retval = gpio_direction_output(gpio, state);
		if (retval) {
			logError(1,  "%s %s: Failed to set gpio %d direction", tag,
					__func__, gpio);
			return retval;
		}
	} else {
		gpio_free(gpio);
	}

	return retval;
}

static int fts_set_gpio(struct fts_ts_info *rmi4_data)
{
	int retval;
	const struct fts_i2c_platform_data *bdata =
			rmi4_data->bdata;

	retval = fts_gpio_setup(bdata->irq_gpio, true, 0, 0);
	if (retval < 0) {
		logError(1,  "%s %s: Failed to configure irq GPIO\n", tag, __func__);
		goto err_gpio_irq;
	}

	if (bdata->reset_gpio >= 0) {
		retval = fts_gpio_setup(bdata->reset_gpio, true, 1, 0);
		if (retval < 0) {
			logError(1,  "%s %s: Failed to configure reset GPIO\n", tag, __func__);
			goto err_gpio_reset;
		}
	}
	if (bdata->reset_gpio >= 0) {
		gpio_set_value(bdata->reset_gpio, 0);
		msleep(10);
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

static int parse_dt(struct device *dev, struct fts_i2c_platform_data *bdata)
{
	int retval;
	const char *name;
	struct device_node *np = dev->of_node;

	bdata->irq_gpio = of_get_named_gpio_flags(np,
			"st,irq-gpio", 0, NULL);

	logError(0, "%s irq_gpio = %d\n", tag, bdata->irq_gpio);

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

	return OK;
}

static int fts_probe(struct i2c_client *client,
		const struct i2c_device_id *idp) {
	struct fts_ts_info *info = NULL;
	char fts_ts_phys[64];
	int error = 0;
	struct device_node *dp = client->dev.of_node;
	int retval;

	logError(1,  "%s %s: driver probe begin!\n", tag, __func__);

	logError(1,  "%s SET I2C Functionality and Dev INFO:\n", tag);
	openChannel(client);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		logError(1,  "%s Unsupported I2C functionality\n", tag);
		error = -EIO;
		goto ProbeErrorExit_0;
	}

	info = kzalloc(sizeof (struct fts_ts_info), GFP_KERNEL);
	if (!info) {
		logError(1,  "%s Out of memory... Impossible to allocate struct info!\n", tag);
		error = -ENOMEM;
		goto ProbeErrorExit_0;
	}

	info->client = client;

	i2c_set_clientdata(client, info);
	logError(1,  "%s i2c address: %x\n", tag, client->addr);
	info->dev = &info->client->dev;
	if (dp) {
		info->bdata = devm_kzalloc(&client->dev, sizeof (struct fts_i2c_platform_data), GFP_KERNEL);
		if (!info->bdata) {
			logError(1,  "%s ERROR:info.bdata kzalloc failed\n", tag);
			goto ProbeErrorExit_1;
		}
		parse_dt(&client->dev, info->bdata);
	}

	logError(1,  "%s SET Regulators:\n", tag);
	retval = fts_get_reg(info, true);
	if (retval < 0) {
		logError(1,  "%s ERROR: %s: Failed to get regulators\n", tag, __func__);
		goto ProbeErrorExit_1;
	}

	retval = fts_enable_reg(info, true);
	if (retval < 0) {
		logError(1,  "%s %s: ERROR Failed to enable regulators\n", tag, __func__);
		goto ProbeErrorExit_2;
	}

	logError(1,  "%s SET GPIOS:\n", tag);
	retval = fts_set_gpio(info);
	if (retval < 0) {
		logError(1,  "%s %s: ERROR Failed to set up GPIO's\n", tag, __func__);
		goto ProbeErrorExit_2;
	}
	info->client->irq = gpio_to_irq(info->bdata->irq_gpio);

	logError(1,  "%s SET Auto Fw Update:\n", tag);
	info->fwu_workqueue = create_singlethread_workqueue("fts-fwu-queue");
	if (!info->fwu_workqueue) {
		logError(1,  "%s ERROR: Cannot create fwu work thread\n", tag);
		goto ProbeErrorExit_3;
	}
	INIT_DELAYED_WORK(&info->fwu_work, fts_fw_update_auto);

	logError(1,  "%s SET Event Handler:\n", tag);
	info->event_wq = create_singlethread_workqueue("fts-event-queue");
	if (!info->event_wq) {
		logError(1,  "%s ERROR: Cannot create work thread\n", tag);
		error = -ENOMEM;
		goto ProbeErrorExit_4;
	}

	INIT_WORK(&info->work, fts_event_handler);

	logError(1,  "%s SET Input Device Property:\n", tag);
	info->dev = &info->client->dev;
	info->input_dev = input_allocate_device();
	if (!info->input_dev) {
		logError(1,  "%s ERROR: No such input device defined!\n", tag);
		error = -ENODEV;
		goto ProbeErrorExit_5;
	}
	info->input_dev->dev.parent = &client->dev;
	info->input_dev->name = FTS_TS_DRV_NAME;
	snprintf(fts_ts_phys, sizeof (fts_ts_phys), "%s/input0",
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

	/* input_mt_init_slots(info->input_dev, TOUCH_ID_MAX); */

	/* input_set_abs_params(info->input_dev, ABS_MT_TRACKING_ID, 0, FINGER_MAX, 0, 0); */
	input_set_abs_params(info->input_dev, ABS_MT_POSITION_X,
			X_AXIS_MIN, X_AXIS_MAX, 0, 0);
	input_set_abs_params(info->input_dev, ABS_MT_POSITION_Y,
			Y_AXIS_MIN, Y_AXIS_MAX, 0, 0);
	input_set_abs_params(info->input_dev, ABS_MT_TOUCH_MAJOR,
			AREA_MIN, AREA_MAX, 0, 0);
	input_set_abs_params(info->input_dev, ABS_MT_TOUCH_MINOR,
			AREA_MIN, AREA_MAX, 0, 0);
	input_set_abs_params(info->input_dev, ABS_MT_PRESSURE,
			PRESSURE_MIN, PRESSURE_MAX, 0, 0);

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

	/* register the multi-touch input device */
	error = input_register_device(info->input_dev);
	if (error) {
		logError(1,  "%s ERROR: No such input device\n", tag);
		error = -ENODEV;
		goto ProbeErrorExit_5_1;
	}

	/* track slots */
	info->touch_id = 0;

	/* init hardware device */
	logError(1,  "%s Device Initialization:\n", tag);
	error = fts_init(info);
	if (error < OK) {
		logError(1,  "%s Cannot initialize the device ERROR %08X\n", tag, error);
		error = -ENODEV;
		goto ProbeErrorExit_6;
	}

	info->gesture_enabled = 0;
	info->glove_enabled = 0;
	info->resume_bit = 1;
	info->notifier = fts_noti_block;
	error = fb_register_client(&info->notifier);
	if (error) {
		logError(1,  "%s ERROR: register notifier failed!\n", tag);
		goto ProbeErrorExit_6;
	}

	logError(1,  "%s SET Device File Nodes:\n", tag);
	/* sysfs stuff */
	info->attrs.attrs = fts_attr_group;
	error = sysfs_create_group(&client->dev.kobj, &info->attrs);
	if (error) {
		logError(1,  "%s ERROR: Cannot create sysfs structure!\n", tag);
		error = -ENODEV;
		goto ProbeErrorExit_7;
	}

#ifdef SCRIPTLESS
	/*I2C cmd*/
	if (fts_cmd_class == NULL)
		fts_cmd_class = class_create(THIS_MODULE, FTS_TS_DRV_NAME);
	info->i2c_cmd_dev = device_create(fts_cmd_class,
			NULL, DCHIP_ID_0, info, "fts_i2c");
	if (IS_ERR(info->i2c_cmd_dev)) {
		logError(1,  "%s ERROR: Failed to create device for the sysfs!\n", tag);
		goto ProbeErrorExit_8;
	}

	dev_set_drvdata(info->i2c_cmd_dev, info);

	error = sysfs_create_group(&info->i2c_cmd_dev->kobj,
			&i2c_cmd_attr_group);
	if (error) {
		logError(1,  "%s ERROR: Failed to create sysfs group!\n", tag);
		goto ProbeErrorExit_9;
	}

#endif

#ifdef DRIVER_TEST
	if (fts_cmd_class == NULL)
		fts_cmd_class = class_create(THIS_MODULE, FTS_TS_DRV_NAME);
	info->test_cmd_dev = device_create(fts_cmd_class,
			NULL, DCHIP_ID_0, info, "fts_driver_test");
	if (IS_ERR(info->test_cmd_dev)) {
		logError(1,  "%s ERROR: Failed to create device for the sysfs!\n", tag);
		goto ProbeErrorExit_10;
	}

	dev_set_drvdata(info->test_cmd_dev, info);

	error = sysfs_create_group(&info->test_cmd_dev->kobj,
			&test_cmd_attr_group);
	if (error) {
		logError(1,  "%s ERROR: Failed to create sysfs group!\n", tag);
		goto ProbeErrorExit_11;
	}

#endif
	/*if wanna auto-update FW when probe,
	 * please don't comment the following code
	 */

	/* queue_delayed_work(info->fwu_workqueue, &info->fwu_work,
	 * msecs_to_jiffies(EXP_FN_WORK_DELAY_MS));
	 */
	logError(1,  "%s Probe Finished!\n", tag);
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
	fb_unregister_client(&info->notifier);

ProbeErrorExit_6:
	input_unregister_device(info->input_dev);

ProbeErrorExit_5_1:
	/* intput_free_device(info->input_dev ); */

	ProbeErrorExit_5:
			destroy_workqueue(info->event_wq);

ProbeErrorExit_4:
	destroy_workqueue(info->fwu_workqueue);

ProbeErrorExit_3:
	fts_enable_reg(info, false);

ProbeErrorExit_2:
	fts_get_reg(info, false);

ProbeErrorExit_1:
	kfree(info);

ProbeErrorExit_0:
	logError(1,  "%s Probe Failed!\n", tag);

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
	/*I2C cmd*/
	sysfs_remove_group(&info->i2c_cmd_dev->kobj,
			&i2c_cmd_attr_group);

#endif

#if defined(SCRIPTLESS) || defined(DRIVER_TEST)
	device_destroy(fts_cmd_class, DCHIP_ID_0);
#endif

	/* sysfs stuff */
	sysfs_remove_group(&client->dev.kobj, &info->attrs);

	/* remove interrupt and event handlers */
	fts_interrupt_uninstall(info);

	fb_unregister_client(&info->notifier);

	/* unregister the device */
	input_unregister_device(info->input_dev);

	/* intput_free_device(info->input_dev ); */

	/* Empty the FIFO buffer */
	fts_command(info, FIFO_CMD_FLUSH);
	/* flushFIFO(); */

	/* Remove the work thread */
	destroy_workqueue(info->event_wq);
	destroy_workqueue(info->fwu_workqueue);

	fts_enable_reg(info, false);
	fts_get_reg(info, false);

	/* free all */
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
