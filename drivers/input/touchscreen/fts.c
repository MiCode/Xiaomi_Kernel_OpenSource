/*
 * fts.c
 *
 * FTS Capacitive touch screen controller (FingerTipS)
 *
 * Copyright (C) 2012, 2013 STMicroelectronics Limited.
 * Authors: AMS(Analog Mems Sensor)
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * *********************************************************************
 * Release 2.05      Date 8th Jan 2016
 * *********************************************************************
 * 1. Removed unnecessary  delay ... msleep()
 * 2. Included checks to validate test data in production test
 * 3. Included command to enable debug  message for prod_test from host(user space)
 */

#define DEBUG
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
#include <linux/wakelock.h>

#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include <linux/input/fts.h>

#include <linux/notifier.h>
#include <linux/fb.h>

#ifdef KERNEL_ABOVE_2_6_38
#include <linux/input/mt.h>
#endif

#ifdef CONFIG_JANUARY_BOOSTER
#include <linux/input/janeps_booster.h>
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#ifdef CONFIG_EXYNOS_TOUCH_DAEMON
#include <linux/exynos_touch_daemon.h>
extern struct exynos_touch_daemon_data exynos_touch_daemon_data;
#endif

#define	LINK_KOBJ_NAME	"tp"

#define FTS_INPUT_EVENT_START				0
#define FTS_INPUT_EVENT_SENSITIVE_MODE_OFF		0
#define FTS_INPUT_EVENT_SENSITIVE_MODE_ON		1
#define FTS_INPUT_EVENT_STYLUS_MODE_OFF			2
#define FTS_INPUT_EVENT_STYLUS_MODE_ON			3
#define FTS_INPUT_EVENT_WAKUP_MODE_OFF			4
#define FTS_INPUT_EVENT_WAKUP_MODE_ON			5
#define FTS_INPUT_EVENT_END				5

/*
 * Event installer helpers
 */
#define event_id(_e)     EVENTID_##_e
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

unsigned char tune_version_same = 0x0;

/* forward declarations */


static void fts_interrupt_enable(struct fts_ts_info *info);
static int fts_fw_upgrade(struct fts_ts_info *info, int mode, int fw_forceupdate, int crc_err);
static int fts_init_hw(struct fts_ts_info *info);
static int fts_init_flash_reload(struct fts_ts_info *info);
static int fts_command(struct fts_ts_info *info, unsigned char cmd);
static void fts_interrupt_set(struct fts_ts_info *info, int enable);
static int fts_systemreset(struct fts_ts_info *info);
extern int input_register_notifier_client(struct notifier_block *nb);
extern int input_unregister_notifier_client(struct notifier_block *nb);
static int fts_get_init_status(struct fts_ts_info *info);
static int fts_get_fw_version(struct fts_ts_info *info);
static int fts_chip_powercycle(struct fts_ts_info *info);
static int fts_chip_initialization(struct fts_ts_info *info);
static int fts_wait_controller_ready(struct fts_ts_info *info);
static int fts_enable_regulator(struct fts_ts_info *info, bool enable);
static void fts_set_wakeup_mode(struct fts_ts_info *info, bool enable);

void touch_callback(unsigned int status)
{
	/* Empty */
}
static int fts_write_reg(struct fts_ts_info *info, unsigned char *reg,
						 unsigned short len)
{
	struct i2c_msg xfer_msg[1];

	xfer_msg[0].addr = info->client->addr;
	xfer_msg[0].len = len;
	xfer_msg[0].flags = 0;
	xfer_msg[0].buf = reg;

	return (i2c_transfer(info->client->adapter, xfer_msg, 1) != 1);
}

static int fts_read_reg(struct fts_ts_info *info, unsigned char *reg, int cnum,
						unsigned char *buf, int num)
{
	struct i2c_msg xfer_msg[2];

	xfer_msg[0].addr = info->client->addr;
	xfer_msg[0].len = cnum;
	xfer_msg[0].flags = 0;
	xfer_msg[0].buf = reg;

	xfer_msg[1].addr = info->client->addr;
	xfer_msg[1].len = num;
	xfer_msg[1].flags = I2C_M_RD;
	xfer_msg[1].buf = buf;

	return (i2c_transfer(info->client->adapter, xfer_msg, 2) != 2);
}

static inline void fts_set_sensor_mode(struct fts_ts_info *info, int mode)
{
	if (!info)
		return ;
	mutex_lock(&info->fts_mode_mutex);
	info->mode = mode ;
	mutex_unlock(&info->fts_mode_mutex);
	return ;
}


static void swipe_gesture_control(char *data, char *reg)
{
	if (!reg)
		return ;

	if (data[0] & 0x01)
		reg[0] |= (1 << 7);
	else
		reg[0] &= ~(1 << 7);

	if (data[0] & 0x02)
		reg[1] |= (1);
	else
		reg[1] &= ~(1);

	if (data[0] & 0x04)
		reg[1] |= (1 << 1);
	else
		reg[1] &= ~(1 << 1);

	if (data[0] & 0x08)
		reg[1] |= (1 << 2);
	else
		reg[1] &= ~(1 << 2);
}


static void unicode_gesture_control(char *data, char *reg)
{
	if (data[0] & 0x01)
		reg[1] |= (1 << 5);
	else
		reg[1] &= ~(1 << 5);

	if (data[0] & 0x02)
		reg[0] |= (1 << 3);
	else
		reg[0] &= ~(1 << 3);

	if (data[0] & 0x04)
		reg[0] |= (1 << 6);
	else
		reg[0] &= ~(1 << 6);

	if (data[0] & 0x08)
		reg[0] |= (1 << 5);
	else
		reg[0] &= ~(1 << 5);

	if (data[0] & 0x10)
		reg[0] |= (1 << 4);
	else
		reg[0] &= ~(1 << 4);

	if (data[0] & 0x20)
		reg[1] |= (1 << 7);
	else
		reg[1] &= ~(1 << 7);

	if (data[0] & 0x80)
		reg[0] |= (1 << 2);
	else
		reg[0] &= ~(1 << 2);

	if (data[0] & 0x40)
		reg[2] |= (1);
	else
		reg[2] &= ~(1);
}
static void tap_gesture_control(char *data, char *reg)
{
	if (data[0])
		reg[0] |= (1 << 1);
	else
		reg[0] &= ~(1 << 1);
}

static int fts_set_gesture_reg(struct fts_ts_info *info, char *mode)
{
	int i;
	unsigned char reg[6] = {0xC1, 0x06};
	unsigned char regcmd[6] = {0xC2, 0x06, 0xFF, 0xFF, 0xFF, 0xFF};

	for (i = 0; i < 4; i++) {
		reg[i + 2] = *(mode + i);
	}

	fts_write_reg(info, regcmd, sizeof(regcmd));
	usleep_range(5000, 6000);
	fts_write_reg(info, reg, sizeof(reg));
	usleep_range(5000, 6000);

	return 0;
}

static void fts_set_gesture(struct fts_ts_info *info)
{
	int all = (info->gesture_mask[ALL_INDEX] & 0xC0) >> 6;
	char reg_data[4] = {0};

	if (all == 2) {
		swipe_gesture_control(&info->gesture_mask[SWIPE_INDEX], reg_data);
		unicode_gesture_control(&info->gesture_mask[UNICODE_INDEX], reg_data);
		tap_gesture_control(&info->gesture_mask[TAP_INDEX], reg_data);
		reg_data[3] = 0x0;
	} else if (all == 1) {
		reg_data[0] |= 0xFE;
		reg_data[1] |= 0xA7;
		reg_data[2] |= 0x1;
		reg_data[3] = 0x0;
	}

	fts_set_gesture_reg(info, reg_data);
}


static void fts_cover_on(struct fts_ts_info *info)
{
	int ret = 0;
	unsigned char regAdd[2] = {0xC1, 0x04};

	ret = fts_write_reg(info, regAdd, sizeof(regAdd));
	usleep_range(5000, 6000);
	if (ret)
		dev_err(info->dev, "fts cover on set failed\n");
}

static void fts_cover_off(struct fts_ts_info *info)
{
	int ret = 0;
	unsigned char regAdd[2] = {0xC2, 0x04};

	ret = fts_write_reg(info, regAdd, sizeof(regAdd));
	usleep_range(5000, 6000);
	if (ret)
		dev_err(info->dev, "fts cover off set failed\n");
}
static ssize_t fts_gesture_data_read(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	int count = snprintf(buf, PAGE_SIZE, "%u\n", info->gesture_value);

	info->gesture_value = GESTURE_ERROR;
	return count;
}

static ssize_t fts_fw_control_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret, mode;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	/* reading out firmware upgrade mode */
	sscanf(buf, "%d", &mode);

	info->fw_force = 1;
	ret = fts_fw_upgrade(info, mode, 0, 0);
	info->fw_force = 0;
	if (ret)
		dev_err(dev, "Unable to upgrade firmware\n");
	return count;
}


static ssize_t fts_gesture_control_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	int *p = (int *)info->gesture_mask;
	memcpy(buf, p, 4);

	return 4;
}

static ssize_t fts_gesture_control_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	int temp;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	const char *data = buf;

	if (data[2] == ALL_CTR) {
		info->gesture_disall = !data[0];
	} else if (data[2] == SWIPE_CTR) {
		info->gesture_mask[SWIPE_INDEX] = 0x0F & data[0];
		info->gesture_mask[ALL_INDEX] = 2 << 6;
	} else if (data[2] == UNICODE_CTR) {
		info->gesture_mask[UNICODE_INDEX] = 0xFF & data[0];
		info->gesture_mask[ALL_INDEX] = 2 << 6;
	} else if (data[2] == TAP_CTR) {
		info->gesture_mask[TAP_INDEX] = 0x01 & data[0];
		info->gesture_mask[ALL_INDEX] = 2 << 6;
	} else
		return -EIO ;

	temp = ((info->gesture_mask[SWIPE_INDEX] == 0x0F) &&
			   (info->gesture_mask[UNICODE_INDEX] == 0xFF) &&
			   (info->gesture_mask[TAP_INDEX] == 0x01));
	info->gesture_mask[ALL_INDEX] = (temp ? 1 : 2) << 6;

	return count;
}

static ssize_t fts_glove_control_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	return scnprintf(buf, PAGE_SIZE, "%04x\n", info->glove_bit);
}

static ssize_t fts_glove_control_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	/* reading out firmware upgrade mode */
	sscanf(buf, "%d", &info->glove_bit);
	if (info->glove_bit) {
		if (info->hover_bit) {
			info->hover_bit = 0;
			fts_command(info, HOVER_OFF);
		}

		ret = fts_command(info, GLOVE_ON);
		fts_set_sensor_mode(info, MODE_GLOVE);
	} else {
		ret = fts_command(info, GLOVE_OFF);
		fts_set_sensor_mode(info, MODE_NORMAL);
	}

	return count;
}

static ssize_t fts_cover_control_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	return scnprintf(buf, PAGE_SIZE, "%04x\n", info->cover_bit);
}

static ssize_t fts_cover_control_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	/* reading out firmware upgrade mode */
	sscanf(buf, "%d", &info->cover_bit);

	if (info->cover_bit) {
		if (info->resume_bit) {
			fts_command(info, GLOVE_OFF);
			fts_command(info, HOVER_OFF);
			fts_cover_on(info);
			usleep_range(5000, 6000);
			fts_command(info, FORCECALIBRATION);
		}

		fts_set_sensor_mode(info, MODE_COVER);
	} else {
		if (info->resume_bit) {
			fts_cover_off(info);
			usleep_range(5000, 6000);
			fts_command(info, FORCECALIBRATION);
		}

		fts_set_sensor_mode(info, MODE_NORMAL);

		if (info->glove_bit) {
			if (info->resume_bit) {
				fts_command(info, GLOVE_ON);
			}

			fts_set_sensor_mode(info, MODE_GLOVE);
		}

		if (info->hover_bit) {
			if (info->resume_bit) {
				fts_command(info, HOVER_ON);
			}

			fts_set_sensor_mode(info, MODE_HOVER);
		}
	}

	return count;
}


static ssize_t fts_sysfs_config_id_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);
	int error;

	error = fts_get_fw_version(info);

	error = snprintf(buf, PAGE_SIZE, "%s:%x:%s:%x\n", "86FTS", info->config_id, "FTM3BD54", info->fw_version);

	return error;
}
static ssize_t fts_sysfs_fwupdate_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	return sprintf(buf, "%d\n", info->fwupdate_stat);
}

static unsigned int le_to_uint(const unsigned char *ptr)
{
	return (unsigned int)ptr[0] + (unsigned int)ptr[1] * 0x100;
}
static unsigned int be_to_uint(const unsigned char *ptr)
{
	return (unsigned int)ptr[1] + (unsigned int)ptr[0] * 0x100;
}
static ssize_t fts_fw_test_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	const struct firmware *fw = NULL;
	unsigned char *data;
	unsigned int size;
	char *firmware_name = "st_fts.bin";
	int fw_version;
	int config_version;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);
	int retval;

	retval = request_firmware(&fw, firmware_name, info->dev);

	data = (unsigned char *)fw->data;
	size = fw->size;

	fw_version = le_to_uint(&data[FILE_FW_VER_OFFSET]);
	config_version = be_to_uint(&data[FILE_CONFIG_VER_OFFSET]);

	return 0;
}


static ssize_t fts_touch_debug_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	sscanf(buf, "%d", &info->touch_debug);

	return count;
}

unsigned char *cx2_h_thres = NULL;
unsigned char *cx2_v_thres = NULL;
unsigned char *cx2_min_thres = NULL;
unsigned char *cx2_max_thres = NULL;
unsigned int *self_ix2_min = NULL;
unsigned int *self_ix2_max  = NULL;
unsigned int *self_cx2_min  = NULL;
unsigned int *self_cx2_max  = NULL;
unsigned int mutual_raw_min;
unsigned int mutual_raw_max;
unsigned int mutual_raw_gap;
unsigned int self_raw_force_min ;
unsigned int self_raw_force_max ;
unsigned int self_raw_sense_min ;
unsigned int self_raw_sense_max ;

unsigned char *mskey_cx2_min = NULL;
unsigned char *mskey_cx2_max = NULL;
unsigned int mskey_raw_min_thres ;
unsigned int mskey_raw_max_thres ;
unsigned int sp_cx2_gap_node_count;
unsigned int sp_cx2_gap_node;
unsigned int *sp_cx2_gap_node_index = NULL;
static unsigned int debug_mode;

int check_sp_cx2_gap_node(int cx2_index)
{
	int index = 0;
	if (sp_cx2_gap_node_count == 0xFFFF)
		return -EPERM;

	for (index = 0; index < sp_cx2_gap_node_count; index++) {
		if (sp_cx2_gap_node_index[index] == cx2_index)
			return 0;
	}

	return -EPERM;
}

int read_compensation_event(struct fts_ts_info *info, unsigned int compensation_type, unsigned char *data)
{
	unsigned char regAdd[8];
	int error = 0;
	int retry = 0;
	int ret = 0;

	regAdd[0] = 0xB8;
	regAdd[1] = compensation_type;
	regAdd[2] = 0x00;
	fts_write_reg(info, regAdd, 3);
	msleep(10);

	for (retry = 0; retry <= READ_CNT; retry++) {
		regAdd[0] = READ_ONE_EVENT;
		error = fts_read_reg(info, regAdd, 1, data, FTS_EVENT_SIZE);
		if (error)
			return -ENODEV;

		if ((data[0] == EVENTID_COMP_DATA_READ) && (data[1] == compensation_type))
			break;
		else {
			msleep(10);
			if (retry == READ_CNT) {
				if (debug_mode)
					tp_log("FTS %s : TIMEOUT ,Cannot read completion event\n", __func__);
				ret = -1;
			}
		}
	}

	if (debug_mode)
		tp_log("%s exited \n", __func__);

	return ret;
}

static int fts_abs_math(int node1, int node2)
{
	int ret = 0;
	if (node1 >= node2)
		ret = node1 - node2;
	else
		ret = node2 - node1;

	return ret;
}

static int fts_initialization_test(struct fts_ts_info *info)
{
	int ret = 0;
	ret =  fts_chip_initialization(info);

	return ret;
}

static int fts_ito_test(struct fts_ts_info *info)
{
	unsigned char data[FTS_EVENT_SIZE];
	unsigned char event_id = 0;
	unsigned char tune_flag = 0;
	unsigned char retry = 0;
	unsigned char error;
	unsigned char regAdd = 0;
	unsigned int ito_check_status = 0;

	fts_systemreset(info);
	fts_wait_controller_ready(info);
	fts_command(info, SENSEOFF);
#ifdef PHONE_KEY
	fts_command(info, KEYOFF);
#endif
	fts_interrupt_set(info, INT_DISABLE);
	fts_command(info, FLUSHBUFFER);
	fts_command(info, ITO_CHECK);
	if (debug_mode)
		tp_log("fts ITO Check Start \n");

	for (retry = 0; retry < READ_CNT_ITO; retry++) {
		regAdd = READ_ONE_EVENT;
		error = fts_read_reg(info, &regAdd, sizeof(regAdd), data, FTS_EVENT_SIZE);
		if (error) {
			if (debug_mode)
				tp_log("fts_ito_test_show : i2C READ ERR , Cannot read device info\n");
			return -ENODEV;
		}

		if (debug_mode)
			tp_log("FTS ITO event : %02X %02X %02X %02X %02X %02X %02X %02X\n",
			data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);

		event_id = data[0];
		tune_flag = data[1];

		if ((event_id == 0x0F) && (tune_flag == 0x05)) {
			if ((data[2] == 0x00) && (data[3] == 0x00)) {
				ito_check_status = 0;
				if (debug_mode)
					tp_log("fts ITO check ok \n");
				break;
			} else {
				ito_check_status = 1;
				if (debug_mode)
					tp_log("fts ITO check fail \n");

				switch (data[2]) {
				case ERR_ITO_PANEL_OPEN_FORCE:
					if (debug_mode)
						tp_log("ITO Test result : Force channel [%d] open.\n",
						data[3]);
					break;
				case ERR_ITO_PANEL_OPEN_SENSE:
					if (debug_mode)
						tp_log("ITO Test result : Sense channel [%d] open.\n",
						data[3]);
					break;
				case ERR_ITO_F2G:
					if (debug_mode)
						tp_log("ITO Test result : Force channel [%d] short to GND.\n",
						data[3]);
					break;
				case ERR_ITO_S2G:
					if (debug_mode)
						tp_log("ITO Test result : Sense channel [%d] short to GND.\n",
						data[3]);
					break;
				case ERR_ITO_F2VDD:
					if (debug_mode)
						tp_log("ITO Test result : Force channel [%d] short to VDD.\n",
						data[3]);
					break;
				case ERR_ITO_S2VDD:
					if (debug_mode)
						tp_log("ITO Test result : Sense channel [%d] short to VDD.\n",
						data[3]);
					break;
				case ERR_ITO_P2P_FORCE:
					if (debug_mode)
						tp_log("ITO Test result : Force channel [%d] ,Pin to Pin short.\n",
						data[3]);
					break;
				case ERR_ITO_P2P_SENSE:
					if (debug_mode)
						tp_log("ITO Test result : Sense channel [%d] Pin to Pin short.\n",
						data[3]);
					break;
				default:
					break;
				}
				break;
			}
		} else {
			msleep(5);
			if (retry == READ_CNT_ITO)
				if (debug_mode)
					tp_log("Time over - wait for result of ITO test\n");
		}
	}

	fts_systemreset(info);
	fts_wait_controller_ready(info);
	fts_interrupt_set(info, INT_ENABLE);
	fts_command(info, SENSEON);
#ifdef PHONE_KEY
	fts_command(info, KEYON);
#endif
	fts_command(info, FORCECALIBRATION);

	return ito_check_status;
}

static int fts_mutual_tune_data_test(struct fts_ts_info *info)
{
	unsigned char *mutual_cx_data = NULL;
	unsigned char *mutual_cx2_err = NULL;
	unsigned char cx1_num = 0;
	unsigned char regAdd[8];
	unsigned char buff_read[17];
	unsigned char data[FTS_EVENT_SIZE];
	unsigned int rx_num, tx_num;
	int i;
	int j = 0;
	int error = 0;
	int address_offset = 0;
	int start_tx_offset = 0;
	int cx2_zero_error_flag = 0;
	int cx2_adjacent_error_flag = 0;
	int cx2_min_max_error_flag = 0;
	int mutual_tune_error = 0;

	fts_interrupt_set(info, INT_DISABLE);
	fts_command(info, SENSEOFF);
#ifdef PHONE_KEY
	fts_command(info, KEYOFF);
#endif
	fts_command(info, FLUSHBUFFER);

	error = read_compensation_event(info, 0x02, data);
	if (error != 0)
		return -EPERM;

	regAdd[0] = 0xD0;
	regAdd[1] = 0x00;
	regAdd[2] = 0x50;
	fts_read_reg(info, regAdd, 3, &buff_read[0], 4);

	start_tx_offset = ((buff_read[2] << 8) | buff_read[1]);
	address_offset = start_tx_offset + 0x10;

	regAdd[0] = 0xD0;
	regAdd[1] = (unsigned char)((start_tx_offset & 0xFF00) >> 8);
	regAdd[2] = (unsigned char)(start_tx_offset & 0xFF);
	fts_read_reg(info, regAdd, 3, &buff_read[0], 17);

	tx_num = data[4];
	rx_num = data[5];
	cx1_num = buff_read[10];

	mutual_cx_data = (unsigned char *)kmalloc(((tx_num * rx_num) + 1), GFP_KERNEL);
	mutual_cx2_err = (unsigned char *)kmalloc(((tx_num * rx_num)), GFP_KERNEL);

	regAdd[0] = 0xD0;
	regAdd[1] = (unsigned char)((address_offset & 0xFF00) >> 8);
	regAdd[2] = (unsigned char)(address_offset & 0xFF);
	fts_read_reg(info, regAdd, 3, mutual_cx_data, ((tx_num * rx_num) + 1));

	{
		int cx2_sum = 0;
		for (j = 0; j <= tx_num; j++) {
			for (i = 0 ; i <= rx_num ; i++)
				cx2_sum += mutual_cx_data[(j * tx_num) + i];
		}

		if (cx2_sum == 0) {
			cx2_zero_error_flag = 1;
			if (debug_mode)
				tp_log("FTS %s: zero_error\n", __func__);
		}
	}

	{
		int hrzErrCnt = 0;
		int vrtErrCnt = 0;
		int node1, node2, threshold, delta;

		for (j = 0; j < tx_num; j++) {
			for (i = 0 ; i < rx_num-1 ; i++) {
				if (!check_sp_cx2_gap_node((j * rx_num) + i)) {
					threshold = sp_cx2_gap_node;
					if (debug_mode)
						tp_log("FTS %s:Horizontal check_sp_cx2_gap_node  tx = %d Rx = %d  threshold = %02X \n", __func__, j, i, threshold);
				} else
					threshold = cx2_h_thres[(j * rx_num) + i];

				node1 = mutual_cx_data[(j * rx_num) + i];
				node2 = mutual_cx_data[(j * rx_num) + i + 1];
				delta = fts_abs_math(node1, node2);

				if (delta > threshold) {
					mutual_cx2_err[(j * tx_num) + i + 1] += 0x01;
					hrzErrCnt++;
					if (debug_mode)
						tp_log("FTS %s: Error cx2_adjacent_error Horizontal threshold tx = %d Rx = %d delta = %02X threshold = %02X \n", __func__, j, i, delta, threshold);
				}
			}
		}

		for (j = 0; j < tx_num - 1; j++) {
			for (i = 0 ; i < rx_num; i++) {
				if (!check_sp_cx2_gap_node((j * rx_num) + i)) {
					threshold = sp_cx2_gap_node;
					if (debug_mode)
						tp_log("FTS %s:Vertical check_sp_cx2_gap_node  tx = %d Rx = %d threshold = %02X \n", __func__, j, i, threshold);
				} else
					threshold = cx2_v_thres[(j*rx_num)+i];

				node1 = mutual_cx_data[(j * rx_num) + i];
				node2 = mutual_cx_data[((j + 1) * rx_num) + i];
				delta = fts_abs_math(node1, node2);

				if (delta > threshold) {
					mutual_cx2_err[(j * tx_num) + i + 1] += 0x02;
					vrtErrCnt++;
					if (debug_mode)
						tp_log("FTS %s: Error cx2_adjacent_error Vertical threshold tx = %d Rx = %d delta = %02X threshold = %02X \n", __func__, j, i, delta, threshold);
				}
			}
		}

		if ((hrzErrCnt + vrtErrCnt) != 0) {
			cx2_adjacent_error_flag = 1;
			if (debug_mode)
				tp_log("FTS %s: cx2_adjacent_error : \n", __func__);
		}
	}

	{
		int minErrCnt = 0;
		int maxErrCnt = 0;

		for (j = 0; j < tx_num; j++) {
			for (i = 0; i < rx_num; i++) {
				if (mutual_cx_data[(j * rx_num) + i] < cx2_min_thres[(j * rx_num) + i])	{
					minErrCnt++;
					if (debug_mode)
						tp_log("FTS %s:Error Min error tx = %d Rx = %d mutual_cx_data= %04X cx2_min_thres = %04X \n", __func__, j, i, mutual_cx_data[(j*rx_num)+i], cx2_min_thres[(j*rx_num)+i]);
				}

				if (mutual_cx_data[(j * rx_num) + i] > cx2_max_thres[(j * rx_num) + i]) {
					maxErrCnt++;
					if (debug_mode)
						tp_log("FTS %s:Error Max error tx = %d Rx = %d mutual_cx_data= %04X cx2_max_thres = %04X \n", __func__, j, i, mutual_cx_data[(j*rx_num)+i], cx2_min_thres[(j*rx_num)+i]);
				}
			}
		}

		if ((minErrCnt+maxErrCnt) != 0) {
			cx2_min_max_error_flag = 1;
			if (debug_mode)
				tp_log("FTS %s: cx2_min_max_error\n", __func__);
		}
	}

	fts_interrupt_set(info, INT_ENABLE);
	fts_command(info, SENSEON);
#ifdef PHONE_KEY
	fts_command(info, KEYON);
#endif

	kfree(mutual_cx_data);
	kfree(mutual_cx2_err);

	if (cx2_min_max_error_flag || cx2_adjacent_error_flag || cx2_zero_error_flag) {
		mutual_tune_error = 1;
		if (debug_mode)
			tp_log("FTS %s: Mutual tune value Test:Failed\n", __func__);
	} else
		if (debug_mode)
			tp_log("FTS %s: Mutual tune value test Passed\n", __func__);

	return mutual_tune_error;
}

static int fts_self_tune_data_test(struct fts_ts_info *info)
{
	unsigned char *self_tune_data = NULL;
	unsigned int f_ix1_num = 0;
	unsigned int s_ix1_num = 0;
	unsigned int f_cx1_num = 0;
	unsigned int s_cx1_num = 0;
	unsigned char regAdd[8];
	unsigned char buff_read[17];
	unsigned char data[FTS_EVENT_SIZE];
	unsigned int rx_num, tx_num;
	int i = 0;
	int error = 0;
	int address_offset = 0;
	int start_tx_offset = 0;
	int self_tune_error = 0;

	fts_interrupt_set(info, INT_DISABLE);
	fts_command(info, SENSEOFF);
#ifdef PHONE_KEY
	fts_command(info, KEYOFF);
#endif
	fts_command(info, FLUSHBUFFER);

	error = read_compensation_event(info, 0x20, data);
	if (error != 0)
		return -EPERM;

	regAdd[0] = 0xD0;
	regAdd[1] = 0x00;
	regAdd[2] = 0x50;
	fts_read_reg(info, regAdd, 3, &buff_read[0], 4);

	start_tx_offset = ((buff_read[2] << 8) | buff_read[1]);
	address_offset = start_tx_offset + 0x10;

	regAdd[0] = 0xD0;
	regAdd[1] = (unsigned char)((start_tx_offset & 0xFF00) >> 8);
	regAdd[2] = (unsigned char)(start_tx_offset & 0xFF);
	fts_read_reg(info, regAdd, 3, &buff_read[0], 17);

	tx_num = data[4];
	rx_num = data[5];
	f_ix1_num = buff_read[10];
	s_ix1_num = buff_read[11];
	f_cx1_num = buff_read[12];
	s_cx1_num = buff_read[13];

	self_tune_data = (unsigned char *)kmalloc(((tx_num + rx_num) * 2 + 1), GFP_KERNEL);

	regAdd[0] = 0xD0;
	regAdd[1] = (unsigned char)((address_offset & 0xFF00) >> 8);
	regAdd[2] = (unsigned char)(address_offset & 0xFF);
	fts_read_reg(info, regAdd, 3, self_tune_data, ((tx_num + rx_num)*2+1));

	{
		int s_ix_force[tx_num];
		int s_ix_sense[rx_num];
		int minErrCnt = 0;
		int maxErrCnt = 0;

		for (i = 0; i < tx_num; i++)
			s_ix_force[i] = K_COFF * f_ix1_num +  L_COFF * self_tune_data[1+i];

		for (i = 0; i < rx_num; i++)
			s_ix_sense[i] = M_COFF * s_ix1_num + N_COFF * self_tune_data[1+tx_num+i];

		if (debug_mode)
			tp_log("fts_read_self_tune_show : probe6\n");

		for (i = 0; i < tx_num; i++) {
			if (s_ix_force[i] < self_ix2_min[i]) {
				minErrCnt++;
				if (debug_mode)
					tp_log("FTS %s:Error  Self min error force = %d s_ix_force[i]=%04X self_ix2_min[i]=%04X \n", __func__, i, s_ix_force[i], self_ix2_min[i]);
			}

			if (s_ix_force[i] > self_ix2_max[i]) {
				maxErrCnt++;
				if (debug_mode)
					tp_log("FTS %s:Error  Self max error force = %d s_ix_force[i]=%04X self_ix2_min[i]=%04X \n", __func__, i, s_ix_force[i], self_ix2_min[i]);
			}
		}

		for (i = tx_num; i < (tx_num + rx_num); i++) {
			if (s_ix_sense[i - tx_num] < self_ix2_min[i]) {
				minErrCnt++;
				if (debug_mode)
					tp_log("FTS %s:Error  Self min error sense = %d  s_ix_sense[i]=%04X self_ix2_min[i]=%04X \n", __func__, i, s_ix_sense[i-tx_num], self_ix2_min[i]);
			}

			if (s_ix_sense[i - tx_num] > self_ix2_max[i]) {
				maxErrCnt++;
				if (debug_mode)
					tp_log("FTS %s:Error  Self max error sense = %d s_ix_sense[i]=%04X self_ix2_min[i]=%04X \n", __func__, i, s_ix_sense[i-tx_num], self_ix2_min[i]);
			}
		}

		if ((minErrCnt + maxErrCnt) != 0) {
			self_tune_error = 1;
			if (debug_mode)
				tp_log("FTS %s: self tune data test Failed\n", __func__);
		} else
			if (debug_mode)
				tp_log("FTS %s: self tune data test Passes\n", __func__);
	}

	fts_interrupt_set(info, INT_ENABLE);
	fts_command(info, SENSEON);
#ifdef PHONE_KEY
	fts_command(info, KEYON);
#endif

	kfree(self_tune_data);

	return self_tune_error;
}

static int fts_mutual_raw_data_test(struct fts_ts_info *info)
{
	unsigned char *mutual_raw_data = NULL;
	unsigned char regAdd[8];
	unsigned char buff_read[17];
	unsigned char data[FTS_EVENT_SIZE];
	unsigned int rx_num, tx_num;
	unsigned int row, col = 0, address_offset = 0;
	int minErrCnt = 0;
	int maxErrCnt = 0;
	int m_raw_min = 0;
	int m_raw_max = 0;

	fts_interrupt_set(info, INT_DISABLE);
	fts_command(info, FLUSHBUFFER);
	fts_command(info, SENSEON);
#ifdef PHONE_KEY
	fts_command(info, KEYON);
#endif
	msleep(100);

	fts_command(info, SENSEOFF);

#ifdef PHONE_KEY
	fts_command(info, KEYOFF);
#endif
	msleep(50);

	read_compensation_event(info, 0x20, data);

	tx_num = data[4];
	rx_num = data[5];

	regAdd[0] = 0xD0;
	regAdd[1] = 0x00;
	regAdd[2] = 0x00;
	fts_read_reg(info, regAdd, 3, &buff_read[0], 4);

	address_offset = ((buff_read[2] << 8) | buff_read[1]) + (rx_num * 2);

	mutual_raw_data = (unsigned char *)kmalloc(((tx_num * rx_num) * 2 + 1), GFP_KERNEL);

	regAdd[0] = 0xD0;
	regAdd[1] = (unsigned char)((address_offset & 0xFF00) >> 8);
	regAdd[2] = (unsigned char)(address_offset & 0xFF);
	fts_read_reg(info, regAdd, 3, mutual_raw_data, ((tx_num * rx_num) * 2 + 1));

	m_raw_min = (mutual_raw_data[2]<<8) | mutual_raw_data[1];
	m_raw_max = (mutual_raw_data[2]<<8) | mutual_raw_data[1];

	for (row = 0; row < tx_num; row++) {
		int temp_mutual_raw = 0;
		for (col = 0; col < rx_num * 2;) {
			temp_mutual_raw = (mutual_raw_data[2 * row * rx_num + col + 2] << 8)|mutual_raw_data[2 * row * rx_num + col + 1];
			if (temp_mutual_raw < mutual_raw_min) {
				minErrCnt++;
				if (debug_mode)
					tp_log("FTS %s:Error Min error tx = %d Rx = %d: \n", __func__, row, col);
			}

			if (temp_mutual_raw > mutual_raw_max) {
				maxErrCnt++;
				if (debug_mode)
					tp_log("FTS %s:Error  Max error tx = %d Rx = %d: \n", __func__, row, col);
			}

			if (temp_mutual_raw < m_raw_min)
				m_raw_min = temp_mutual_raw;

			if (temp_mutual_raw > m_raw_max)
				m_raw_max = temp_mutual_raw;

			col = col+2;
		}
	}

	fts_interrupt_set(info, INT_ENABLE);
	fts_command(info, SENSEON);
#ifdef PHONE_KEY
	fts_command(info, KEYON);
#endif
	kfree(mutual_raw_data);

	if ((minErrCnt + maxErrCnt) != 0) {
		if (debug_mode)
			tp_log("FTS %s: Test Failed :mutual raw min/max error\n", __func__);
		return -EPERM;
	}

	if ((m_raw_max - m_raw_min) > mutual_raw_gap) {
		if (debug_mode) {
			tp_log("FTS %s:Error mutual_raw_gap = %d: \n", __func__, mutual_raw_gap);
			tp_log("FTS %s: Test Failed :mutual raw gap error\n", __func__);
		}
		return -ENOENT;
	}
	return 0;
}

static int fts_self_raw_data_test(struct fts_ts_info *info)
{
	unsigned char *self_raw_data_force = NULL;
	unsigned char *self_raw_data_sense = NULL;
	unsigned char regAdd[8];
	unsigned char buff_read[17];
	unsigned char data[FTS_EVENT_SIZE];
	unsigned int rx_num, tx_num;
	int i = 0;
	int address_offset_force = 0;
	int address_offset_sense = 0;
	int size_hex_force = 0;
	int size_hex_sense = 0;
	int minErrCnt = 0;
	int maxErrCnt = 0;

	fts_interrupt_set(info, INT_DISABLE);
	fts_command(info, FLUSHBUFFER);
	fts_command(info, SENSEON);
#ifdef PHONE_KEY
	fts_command(info, KEYON);
#endif
	msleep(100);

	fts_command(info, SENSEOFF);

#ifdef PHONE_KEY
	fts_command(info, KEYOFF);
#endif
	msleep(50);

	read_compensation_event(info, 0x20, data);

	tx_num = data[4];
	rx_num = data[5];
	size_hex_force = tx_num * 2;
	size_hex_sense = rx_num * 2;

	regAdd[0] = 0xD0;
	regAdd[1] = 0x00;
	regAdd[2] = 0x1A;
	fts_read_reg(info, regAdd, 3, &buff_read[0], 5);

	address_offset_force = ((buff_read[2] << 8) | buff_read[1]);
	address_offset_sense = ((buff_read[4] << 8) | buff_read[3]);

	self_raw_data_force = (unsigned char *)kmalloc((size_hex_force + 1), GFP_KERNEL);
	self_raw_data_sense = (unsigned char *)kmalloc((size_hex_sense + 1), GFP_KERNEL);

	regAdd[0] = 0xD0;
	regAdd[1] = (unsigned char)((address_offset_force & 0xFF00) >> 8);
	regAdd[2] = (unsigned char)(address_offset_force & 0xFF);
	fts_read_reg(info, regAdd, 3, self_raw_data_force, (size_hex_force + 1));

	for (i = 0; i < tx_num * 2;) {
		int temp_raw = 0;
		temp_raw = ((self_raw_data_force[i + 2] << 8) | self_raw_data_force[i + 1]);
		if (temp_raw < self_raw_force_min) {
			minErrCnt++;
			if (debug_mode)
				tp_log("FTS %s:Error Self raw min error force = %d raw_data = %d: \n", __func__, i, temp_raw);
		}

		if (temp_raw > self_raw_force_max) {
			maxErrCnt++;
			if (debug_mode)
				tp_log("FTS %s:Error Self raw max error force = %d raw_data = %d: \n", __func__, i, temp_raw);
		}
		i = i + 2;
	}

	regAdd[0] = 0xD0;
	regAdd[1] = (unsigned char)((address_offset_sense & 0xFF00) >> 8);
	regAdd[2] = (unsigned char)(address_offset_sense & 0xFF);
	fts_read_reg(info, regAdd, 3, self_raw_data_sense, (size_hex_sense + 1));

	for (i = 0; i < rx_num * 2;) {
		int temp_raw = 0;
		temp_raw = ((self_raw_data_sense[i + 2] << 8) | self_raw_data_sense[i + 1]);
		if (temp_raw < self_raw_sense_min) {
			minErrCnt++;
			if (debug_mode)
				tp_log("FTS %s:Error Self raw min error sense = %d sense_raw_data = %d: \n", __func__, i, temp_raw);
		}

		if (temp_raw > self_raw_sense_max) {
			maxErrCnt++;
			if (debug_mode)
				tp_log("FTS %s:Error Self raw max error sense = %d sense_raw_data = %d: \n", __func__, i, temp_raw);
		}
		i = i + 2;
	}

	fts_interrupt_set(info, INT_ENABLE);
	fts_command(info, SENSEON);
#ifdef PHONE_KEY
	fts_command(info, KEYON);
#endif
	kfree(self_raw_data_force);
	kfree(self_raw_data_sense);

	if ((minErrCnt + maxErrCnt) != 0) {
		if (debug_mode)
			tp_log("FTS %s: Test Failed :self raw min/max error\n", __func__);
		return -EPERM;
	}

	return 0;
}

static int fts_mskey_tune_data_test(struct fts_ts_info *info)
{
	unsigned char *mskey_cx2 = NULL;
	unsigned char cx1_num = 0;
	unsigned char regAdd[8];
	unsigned char buff_read[17];
	unsigned char data[FTS_EVENT_SIZE];
	unsigned int rx_num, tx_num, mskeyNum = 0;
	int i = 0;
	int error = 0;
	int address_offset = 0;
	int start_tx_offset = 0;
	int key_cnt = 0;
	int mskey_tune_error = 0;

	fts_interrupt_set(info, INT_DISABLE);
	fts_command(info, SENSEOFF);
#ifdef PHONE_KEY
	fts_command(info, KEYOFF);
#endif
	fts_command(info, FLUSHBUFFER);

	error = read_compensation_event(info, 0x10, data);
	if (error != 0)
		return -EPERM;

	regAdd[0] = 0xD0;
	regAdd[1] = 0x00;
	regAdd[2] = 0x50;
	fts_read_reg(info, regAdd, 3, &buff_read[0], 4);

	start_tx_offset = ((buff_read[2] << 8) | buff_read[1]);
	address_offset  = start_tx_offset + 0x10;

	regAdd[0] = 0xD0;
	regAdd[1] = (unsigned char)((start_tx_offset & 0xFF00) >> 8);
	regAdd[2] = (unsigned char)(start_tx_offset & 0xFF);
	fts_read_reg(info, regAdd, 3, &buff_read[0], 17);

	tx_num = data[4];
	rx_num = data[5];
	cx1_num = buff_read[10];

	if (tx_num > rx_num)
		mskeyNum = tx_num;
	else
		mskeyNum = rx_num;

	mskey_cx2 = (unsigned char *)kmalloc((mskeyNum + 1), GFP_KERNEL);

	regAdd[0] = 0xD0;
	regAdd[1] = (unsigned char)((address_offset & 0xFF00) >> 8);
	regAdd[2] = (unsigned char)(address_offset & 0xFF);
	fts_read_reg(info, regAdd, 3, mskey_cx2, (mskeyNum + 1));

	{
		int minErrCnt = 0;
		int maxErrCnt = 0;
		for (i = 0; i < key_cnt; i++) {
			if (mskey_cx2[i] < mskey_cx2_min[i]) {
				minErrCnt++;
				if (debug_mode)
					tp_log("FTS %s:Error mskey_cx2 min error i = %d mskey_cx2[i] = %04X  mskey_cx2_min[i] = %04X \n", __func__, i, mskey_cx2[i], mskey_cx2_min[i]);
			}

			if (mskey_cx2[i] > mskey_cx2_max[i]) {
				maxErrCnt++;
				if (debug_mode)
					tp_log("FTS %s:Error mskey_cx2 min error i = %d mskey_cx2[i] = %04X  mskey_cx2_max[i] = %04X \n", __func__, i, mskey_cx2[i], mskey_cx2_max[i]);
			}
		}

		if ((minErrCnt + maxErrCnt) != 0)
			mskey_tune_error = 1;
	}

	fts_interrupt_set(info, INT_ENABLE);
	fts_command(info, SENSEON);
#ifdef PHONE_KEY
	fts_command(info, KEYON);
#endif
	kfree(mskey_cx2);

	return mskey_tune_error;
}

static int fts_mskey_raw_data_test(struct fts_ts_info *info)
{
	unsigned char *mskey_raw_data = NULL;
	unsigned char regAdd[8];
	unsigned char buff_read[17];
	unsigned char data[FTS_EVENT_SIZE];
	unsigned int rx_num, tx_num, mskeyNum = 0;
	int i = 0;
	int error = 0;
	int address_offset = 0;
	int size_hex = 0;
	int mskey_raw_error = 0;

	fts_interrupt_set(info, INT_DISABLE);
	fts_command(info, SENSEON);
#ifdef PHONE_KEY
	fts_command(info, KEYON);
#endif
	fts_command(info, FLUSHBUFFER);

	error = read_compensation_event(info, 0x10, data);
	if (error != 0)
		return -EPERM;

	tx_num = data[4];
	rx_num = data[5];

	if (tx_num > rx_num)
		mskeyNum = tx_num;
	else
		mskeyNum = rx_num;

	size_hex = (mskeyNum) * 2 ;

	regAdd[0] = 0xD0;
	regAdd[1] = 0x00;
	regAdd[2] = 0x32;
	fts_read_reg(info, regAdd, 3, &buff_read[0], 4);

	address_offset = ((buff_read[2] << 8) | buff_read[1]) + (rx_num * 2);

	mskey_raw_data = (unsigned char *)kmalloc((size_hex + 1), GFP_KERNEL);

	regAdd[0] = 0xD0;
	regAdd[1] = (unsigned char)((address_offset & 0xFF00) >> 8);
	regAdd[2] = (unsigned char)(address_offset & 0xFF);
	fts_read_reg(info, regAdd, 3, mskey_raw_data, (size_hex + 1));

	{
		int minErrCnt = 0;
		int maxErrCnt = 0;
		int mskey_raw;
		for (i = 0; i < mskeyNum; i++) {
			mskey_raw = mskey_raw_data[2 * i + 2] << 8 | mskey_raw_data[i * 2 + 1];
			if (mskey_raw < mskey_raw_min_thres) {
				minErrCnt++;
				if (debug_mode)
					tp_log("FTS %s:Error mskey_raw min error i = %d mskey_raw[i] = %04X  mskey_raw_min_thres[i] = %04X \n", __func__, i, mskey_raw, mskey_raw_min_thres);
			}

			if (mskey_raw > mskey_raw_max_thres) {
				maxErrCnt++;
				if (debug_mode)
					tp_log("FTS %s:Error mskey_raw max error i = %d mskey_raw[i] = %04X  mskey_raw_max_thres[i] = %04X \n", __func__, i, mskey_raw, mskey_raw_max_thres);
			}
		}

		if ((minErrCnt + maxErrCnt) != 0) {
			mskey_raw_error = 1;
		}
	}

	fts_interrupt_set(info, INT_ENABLE);
	kfree(mskey_raw_data);

	return mskey_raw_error;
}

static ssize_t fts_production_test_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	sscanf(buf, "%d", &debug_mode);

	return count;
}

static ssize_t fts_production_test_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);
	char buff[CMD_STR_LEN] = {0};
	const struct firmware *fw_config = NULL;
	unsigned char *configData = NULL;
	unsigned int config_size = 0;
	static unsigned int tx_num, rx_num, i, ret;

	/*initialize the test values*/
	mutual_raw_min = 0xFFFF;
	mutual_raw_max = 0xFFFF;
	mutual_raw_gap = 0xFFFf;
	self_raw_force_min = 0xFFFF;
	self_raw_force_max = 0xFFFF;
	self_raw_sense_min = 0xFFFF;
	self_raw_sense_max = 0xFFFF;
	mskey_raw_min_thres = 0xFFFF;
	mskey_raw_max_thres = 0xFFFF;
	sp_cx2_gap_node_count = 0xFFFF;
	sp_cx2_gap_node = 0xFFFF;

	memset(Out_buff, 0x00, ARRAY_SIZE(Out_buff));

	ret = request_firmware(&fw_config, "fts_threshold.bin", info->dev);
	if (ret) {
		if (debug_mode)
			tp_log("FTS Unable to open Threshold file '%s'\n", "fts_threshold.bin");
		return -EPERM;
	}
	configData = (unsigned char *)fw_config->data;
	config_size = fw_config->size;

	if (config_size > 0) {
		if (debug_mode)
			tp_log("%s, config size = %d\n", __func__, config_size);
	} else {
		if (debug_mode)
			tp_log("%s, config size is invalid", __func__);

		return -EPERM;
	}

	for (i = 0; i < config_size;) {
		if ((configData[i] == 0xAA) && (configData[i+1] == 0x55)) {
			i = i + 2;
			if (debug_mode)
				tp_log("%s,0xaa0x55 configData[i] = %d\n", __func__, configData[i]);
			switch (configData[i++]) {
			case 0x00:
				tx_num = configData[i++];
				rx_num = configData[i++];
				if (debug_mode)
					tp_log("%s,0xaa0x55 00 configData[i] = %d\n", __func__, configData[i]);
				break;
			case 0x01:
				{
					unsigned int cx2_h_thres_size = 0, cx2_h_thres_index = 0;
					int tx, rx;
					tx = configData[i++];
					rx = configData[i++];
					cx2_h_thres_size = tx * rx;
					cx2_h_thres = (unsigned char *)kmalloc(cx2_h_thres_size, GFP_KERNEL);
					while (cx2_h_thres_index < cx2_h_thres_size)
						cx2_h_thres[cx2_h_thres_index++] = configData[i];
						i++;
					if (debug_mode)
					tp_log("%s,0xaa0x55 01 configData[i] = %d\n", __func__, configData[i]);

					break;
				}
			case 0x02:
				{
					unsigned int cx2_v_thres_size = 0, cx2_v_thres_index = 0;
					int tx, rx;
					tx = configData[i++];
					rx = configData[i++];
					cx2_v_thres_size = tx * rx;
					cx2_v_thres = (unsigned char *)kmalloc(cx2_v_thres_size, GFP_KERNEL);

					while (cx2_v_thres_index < cx2_v_thres_size)
						cx2_v_thres[cx2_v_thres_index++] = configData[i];

					i++;
					if (debug_mode)
						tp_log("%s,0xaa0x55 02 configData[i] = %d\n", __func__, configData[i]);

					break;
				}
			case 0x03:
				{
					unsigned int cx2_min_thres_size = 0, cx2_min_thres_index = 0;
					cx2_min_thres_size = tx_num * rx_num;
					cx2_min_thres = (unsigned char *)kmalloc(cx2_min_thres_size, GFP_KERNEL);

					while (cx2_min_thres_index < cx2_min_thres_size)
						cx2_min_thres[cx2_min_thres_index++] = configData[i];

					i++;
					if (debug_mode)
						tp_log("%s,0xaa0x55 03 configData[i] = %d\n", __func__, configData[i]);

					break;
				}
			case 0x04:
				{
					unsigned int cx2_max_thres_size = 0, cx2_max_thres_index = 0;
					cx2_max_thres_size = tx_num * rx_num;
					cx2_max_thres = (unsigned char *)kmalloc(cx2_max_thres_size, GFP_KERNEL);

					while (cx2_max_thres_index < cx2_max_thres_size)
						cx2_max_thres[cx2_max_thres_index++] = configData[i];

					i++;
					if (debug_mode)
						tp_log("%s,0xaa0x55 04 configData[i] = %d\n", __func__, configData[i]);

					break;
				}
			case 0x05:
				{
					unsigned int self_ix2_min_size = 0, self_ix2_min_index = 0;
					self_ix2_min_size = (tx_num + rx_num);
					self_ix2_min = (unsigned int *)kmalloc(self_ix2_min_size * sizeof(unsigned int), GFP_KERNEL);

					while (self_ix2_min_index < tx_num)
						self_ix2_min[self_ix2_min_index++] = configData[i]<<8|configData[i+1];

					i = i + 2;

					while (self_ix2_min_index < self_ix2_min_size)
						self_ix2_min[self_ix2_min_index++] = configData[i]<<8|configData[i+1];

					i = i + 2;
					if (debug_mode)
						tp_log("%s,0xaa0x55 05 configData[i] = %d\n", __func__, configData[i]);

					break;
				}
			case 0x06:
				{
					unsigned int self_ix2_max_size = 0, self_ix2_max_index = 0;
					self_ix2_max_size = (tx_num + rx_num);
					self_ix2_max = (unsigned int *)kmalloc(self_ix2_max_size * sizeof(unsigned int), GFP_KERNEL);

					while (self_ix2_max_index < tx_num)
						self_ix2_max[self_ix2_max_index++] = configData[i]<<8|configData[i+1];

					i = i + 2;

					while (self_ix2_max_index < self_ix2_max_size)
						self_ix2_max[self_ix2_max_index++] = configData[i]<<8|configData[i+1];

					i = i + 2;
					if (debug_mode)
						tp_log("%s,0xaa0x55 06 configData[i] = %d\n", __func__, configData[i]);

					break;
				}
			case 0x07:
				{
					unsigned int self_cx2_min_size = 0, self_cx2_min_index = 0;
					self_cx2_min_size = (tx_num + rx_num);
					self_cx2_min = (unsigned int *)kmalloc(self_cx2_min_size * sizeof(unsigned int), GFP_KERNEL);

					while (self_cx2_min_index < tx_num)
						self_cx2_min[self_cx2_min_index++] = configData[i]<<8|configData[i+1];

					i = i + 2;

					while (self_cx2_min_index < self_cx2_min_size)
						self_cx2_min[self_cx2_min_index++] = configData[i]<<8|configData[i+1];

					i = i + 2;
					if (debug_mode)
						tp_log("%s,0xaa0x55 07 configData[i] = %d\n", __func__, configData[i]);

					break;
				}
			case 0x08:
				{
					unsigned int self_cx2_max_size = 0, self_cx2_max_index = 0;
					self_cx2_max_size = (tx_num + rx_num);
					self_cx2_max = (unsigned int *)kmalloc(self_cx2_max_size * sizeof(unsigned int), GFP_KERNEL);

					while (self_cx2_max_index < tx_num)
						self_cx2_max[self_cx2_max_index++] = configData[i]<<8|configData[i+1];

					i = i + 2;

					while (self_cx2_max_index < self_cx2_max_size)
						self_cx2_max[self_cx2_max_index++] = configData[i]<<8|configData[i+1];

					i = i + 2;

					if (debug_mode)
						tp_log("%s,0xaa0x55 08 configData[i] = %d\n", __func__, configData[i]);

					break;
				}
			}

			if (debug_mode)
				tp_log("%s,0xaa0x55 switch exit configData[i] = %d\n", __func__, configData[i]);
		} else if ((configData[i] == 0xAA) && (configData[i+1] == 0x56)) {
			i = i + 2;
			if (debug_mode)
				tp_log("%s, 0xaa0x56 configData[i] = %d\n", __func__, configData[i]);

			switch (configData[i]) {
			case 0x01:
				mutual_raw_min = (configData[i+1]<<8)|configData[i+2];
				break;
			case 0x02:
				mutual_raw_max = (configData[i+1]<<8)|configData[i+2];
				break;
			case 0x03:
				mutual_raw_gap = (configData[i+1]<<8)|configData[i+2];
				break;
			case 0x04:
				self_raw_force_min = (configData[i+1]<<8)|configData[i+2];
				break;
			case 0x05:
				self_raw_force_max = (configData[i+1]<<8)|configData[i+2];
				break;
			case 0x06:
				self_raw_sense_min = (configData[i+1]<<8)|configData[i+2];
				break;
			case 0x07:
				self_raw_sense_max = (configData[i+1]<<8)|configData[i+2];
				break;
			case 0x08:
				mskey_raw_min_thres = (configData[i+1]<<8)|configData[i+2];
				break;
			case 0x09:
				mskey_raw_max_thres = (configData[i+1]<<8)|configData[i+2];
				break;
			}
			i = i + 2;
		} else if ((configData[i] == 0xAA) && (configData[i+1] == 0x57)) {
			int node_index = 0;
			int tx, rx;
			i = i + 2;
			if (debug_mode)
				tp_log("%s,0xaa0x57 configData[i] \n", __func__);
			tx = configData[i++];
			rx = configData[i++];
			sp_cx2_gap_node_count = configData[i++];
			sp_cx2_gap_node = configData[i++];
			sp_cx2_gap_node_index = (unsigned int *)kmalloc(sp_cx2_gap_node_count, GFP_KERNEL);
			for (node_index = 0; node_index < sp_cx2_gap_node_count; node_index++) {
				sp_cx2_gap_node_index[node_index] = ((unsigned int)configData[i]*rx) + (unsigned int)configData[i + 1];
				i = i + 2;
			}
		} else {
			i++;
			if (debug_mode)
				tp_log("%s,Data not matching %d\n", __func__, configData[i]);
		}
	}

	snprintf(buff, sizeof(buff), "Production Test Results\n");
	strncat(Out_buff, buff, 512);

	snprintf(buff, sizeof(buff), "Initialization TEST :");
	strncat(Out_buff, buff, 512);
	if (fts_initialization_test(info) >= 0) {
		snprintf(buff, sizeof(buff), "PASSED\n");
		strncat(Out_buff, buff, 512);
	} else {
		snprintf(buff, sizeof(buff), "FAILED\n");
		strncat(Out_buff, buff, 512);
	}

	snprintf(buff, sizeof(buff), "ITO TEST:");
	strncat(Out_buff, buff, 512);
	if (fts_ito_test(info) == 0) {
		snprintf(buff, sizeof(buff), "PASSED\n");
		strncat(Out_buff, buff, 512);
	} else {
		snprintf(buff, sizeof(buff), "FAILED\n");
		strncat(Out_buff, buff, 512);
	}

	snprintf(buff, sizeof(buff), "Mutual tune data TEST:");
	strncat(Out_buff, buff, 512);
	if ((cx2_h_thres != NULL) && (cx2_v_thres != NULL) && (cx2_min_thres != NULL) && (cx2_max_thres != NULL)) {
		if (fts_mutual_tune_data_test(info) == 0)
			snprintf(buff, sizeof(buff), "PASSED\n");
		else
			snprintf(buff, sizeof(buff), "FAILED \n");

		strncat(Out_buff, buff, 512);
	} else {
		snprintf(buff, sizeof(buff), "Test Data Not Entered \n");
		strncat(Out_buff, buff, 512);
	}

	snprintf(buff, sizeof(buff), "Self tune data TEST:");
	strncat(Out_buff, buff, 512);
	if ((self_ix2_min != NULL) && (self_ix2_max != NULL)) {
		if (fts_self_tune_data_test(info) == 0)
			snprintf(buff, sizeof(buff), "PASSED \n");
		else
			snprintf(buff, sizeof(buff), "FAILED \n");

		strncat(Out_buff, buff, 512);
	} else {
		snprintf(buff, sizeof(buff), "Test Data Not Entered \n");
		strncat(Out_buff, buff, 512);
	}

	snprintf(buff, sizeof(buff), "Mutual raw data TEST:");
	strncat(Out_buff, buff, 512);
	if ((mutual_raw_min != 0xFFFF) && (mutual_raw_max != 0xFFFF) && (mutual_raw_gap != 0xFFFF)) {
		if (fts_mutual_raw_data_test(info) == 0)
			snprintf(buff, sizeof(buff), "PASSED \n");
		else
			snprintf(buff, sizeof(buff), "FAILED \n");

		strncat(Out_buff, buff, 512);
	} else {
		snprintf(buff, sizeof(buff), "Test Data Not Entered \n");
		strncat(Out_buff, buff, 512);
	}

	snprintf(buff, sizeof(buff), "Self raw data TEST:");
	strncat(Out_buff, buff, 512);
	if ((self_raw_force_min != 0xFFFF) && (self_raw_force_max != 0xFFFF) && (self_raw_sense_min != 0xFFFF) && (self_raw_sense_max != 0xFFFF)) {
		if (fts_self_raw_data_test(info) == 0)
			snprintf(buff, sizeof(buff), "PASSED \n");
		else
			snprintf(buff, sizeof(buff), "FAILED \n");

		strncat(Out_buff, buff, 512);
	} else {
		snprintf(buff, sizeof(buff), "Test Data Not Entered\n");
		strncat(Out_buff, buff, 512);
	}

	snprintf(buff, sizeof(buff), "Mskey tune data TEST:");
	strncat(Out_buff, buff, 512);
	if ((mskey_cx2_min != NULL) && (mskey_cx2_max != NULL)) {
		if (fts_mskey_tune_data_test(info) == 0)
			snprintf(buff, sizeof(buff), "PASSED \n");
		else
			snprintf(buff, sizeof(buff), "FAILED \n");

		strncat(Out_buff, buff, 512);
	} else {
		snprintf(buff, sizeof(buff), "Test Data Not Entered\n");
		strncat(Out_buff, buff, 512);
	}

	snprintf(buff, sizeof(buff), "Mskey raw data TEST:");
	strncat(Out_buff, buff, 512);
	if ((mskey_raw_min_thres != 0xFFFF) && (mskey_raw_max_thres != 0xFFFF)) {
		if (fts_mskey_raw_data_test(info) == 0)
			snprintf(buff, sizeof(buff), "PASSED \n");
		else
			snprintf(buff, sizeof(buff), "FAILED \n");

		strncat(Out_buff, buff, 512);
	} else {
		snprintf(buff, sizeof(buff), "Test Data Not Entered\n");
		strncat(Out_buff, buff, 512);
	}

	release_firmware(fw_config);
	kfree(cx2_h_thres);
	kfree(cx2_v_thres);
	kfree(cx2_min_thres);
	kfree(cx2_max_thres);
	kfree(self_ix2_min);
	kfree(self_ix2_max);
	kfree(self_cx2_min);
	kfree(self_cx2_max);
	kfree(mskey_cx2_min);
	kfree(mskey_cx2_max);
	kfree(sp_cx2_gap_node_index);

	return snprintf(buf, TSP_BUF_SIZE, "%s\n", Out_buff);
}

static ssize_t fts_initialization_test_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	char result = 0;
	char ret  = 0;
	char buff[CMD_STR_LEN] = {0};
	char all_strbuff[CMD_STR_LEN] = {0};
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);
	int size = 0;
	size = sizeof(all_strbuff);

	snprintf(buff, sizeof(buff), "%02X", 0xAA);
	strncat(all_strbuff, buff, size);

	ret =  fts_chip_initialization(info);

	if (!ret)
		result = 0x00;
	else
		result = 0x01;

	snprintf(buff, sizeof(buff), "%02X", result);
	strncat(all_strbuff, buff, size);

	snprintf(buff, sizeof(buff), "%02X", 0xBB);
	strncat(all_strbuff, buff, size);

	return snprintf(buf, TSP_BUF_SIZE, "%s\n", all_strbuff);
}

static ssize_t fts_ito_test_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	unsigned char data[FTS_EVENT_SIZE];
	unsigned char event_id = 0;
	unsigned char tune_flag = 0;
	unsigned char retry = 0;
	unsigned char error;
	unsigned char regAdd = 0;
	unsigned int ito_check_status = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);
	char buff[CMD_STR_LEN] = {0};
	char all_strbuff[CMD_STR_LEN] = {0};
	int size = 0;
	size = sizeof(all_strbuff);

	fts_systemreset(info);
	fts_wait_controller_ready(info);
	fts_command(info, SENSEOFF);
#ifdef PHONE_KEY
	fts_command(info, KEYOFF);
#endif
	fts_interrupt_set(info, INT_DISABLE);
	fts_command(info, FLUSHBUFFER);
	fts_command(info, ITO_CHECK);

	for (retry = 0; retry <= READ_CNT_ITO; retry++) {
		regAdd = READ_ONE_EVENT;
		error = fts_read_reg(info, &regAdd, sizeof(regAdd), data, FTS_EVENT_SIZE);
		if (error)
			return -ENODEV;

		event_id = data[0];
		tune_flag = data[1];

		if ((event_id == 0x0F) && (tune_flag == 0x05)) {
			if ((data[2] == 0x00) && (data[3] == 0x00)) {
				ito_check_status = 0;
				break;
			} else {
				ito_check_status = 1;

				switch (data[2]) {
				case ERR_ITO_PANEL_OPEN_FORCE:
					dev_err(info->dev, "ITO Test result : Force channel [%d] open.\n",
						data[3]);
					break;
				case ERR_ITO_PANEL_OPEN_SENSE:
					dev_err(info->dev, "ITO Test result : Sense channel [%d] open.\n",
						data[3]);
					break;
				case ERR_ITO_F2G:
					dev_err(info->dev, "ITO Test result : Force channel [%d] short to GND.\n",
						data[3]);
					break;
				case ERR_ITO_S2G:
					dev_err(info->dev, "ITO Test result : Sense channel [%d] short to GND.\n",
						data[3]);
					break;
				case ERR_ITO_F2VDD:
					dev_err(info->dev, "ITO Test result : Force channel [%d] short to VDD.\n",
						data[3]);
					break;
				case ERR_ITO_S2VDD:
					dev_err(info->dev, "ITO Test result : Sense channel [%d] short to VDD.\n",
						data[3]);
					break;
				case ERR_ITO_P2P_FORCE:
					dev_err(info->dev, "ITO Test result : Force channel [%d] ,Pin to Pin short.\n",
						data[3]);
					break;
				case ERR_ITO_P2P_SENSE:
					dev_err(info->dev, "ITO Test result : Sense channel [%d] Pin to Pin short.\n",
						data[3]);
					break;
				default:
					break;
				}

				break;
			}
		} else
			msleep(5);
	}

	fts_systemreset(info);
	fts_wait_controller_ready(info);
	fts_command(info, SENSEON);
#ifdef PHONE_KEY
	fts_command(info, KEYON);
#endif
	fts_command(info, FORCECALIBRATION);

	fts_command(info, FLUSHBUFFER);
	fts_interrupt_set(info, INT_ENABLE);

	snprintf(buff, sizeof(buff), "%02X", 0xAA);
	strncat(all_strbuff, buff, size);

	snprintf(buff, sizeof(buff), "%02X", ito_check_status);
	strncat(all_strbuff, buff, size);

	snprintf(buff, sizeof(buff), "%02X", 0xBB);
	strncat(all_strbuff, buff, size);

	return snprintf(buf, TSP_BUF_SIZE, "%s\n", all_strbuff);
}

static ssize_t fts_read_mutual_tune_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);
	unsigned char *all_strbuff = NULL;
	unsigned char *mutual_cx_data = NULL;
	unsigned char cx1_num = 0;
	char buff[CMD_STR_LEN] = {0};
	unsigned char regAdd[8];
	unsigned char buff_read[17];
	unsigned char data[FTS_EVENT_SIZE];
	unsigned int rx_num, tx_num;
	int count = 0;
	int j = 0;
	int error = 0;
	int address_offset = 0;
	int start_tx_offset = 0;
	int retry = 0;
	int size = 0;

	fts_interrupt_set(info, INT_DISABLE);
	fts_command(info, SENSEOFF);
#ifdef PHONE_KEY
	fts_command(info, KEYOFF);
#endif
	fts_command(info, FLUSHBUFFER);

	regAdd[0] = 0xB8;
	regAdd[1] = 0x02;
	regAdd[2] = 0x00;
	fts_write_reg(info, regAdd, 3);

	for (retry = 0; retry <= READ_CNT; retry++) {
		regAdd[0] = READ_ONE_EVENT;
		error = fts_read_reg(info, regAdd, 1, data, FTS_EVENT_SIZE);
		if (error)
			return -ENODEV;

		if ((data[0] == EVENTID_COMP_DATA_READ) && (data[1] == 0x02))
			break;
		else
			msleep(10);
	}

	regAdd[0] = 0xD0;
	regAdd[1] = 0x00;
	regAdd[2] = 0x50;
	fts_read_reg(info, regAdd, 3, &buff_read[0], 4);

	start_tx_offset = ((buff_read[2] << 8) | buff_read[1]);
	address_offset = start_tx_offset + 0x10;

	regAdd[0] = 0xD0;
	regAdd[1] = (unsigned char)((start_tx_offset & 0xFF00) >> 8);
	regAdd[2] = (unsigned char)(start_tx_offset & 0xFF);
	fts_read_reg(info, regAdd, 3, &buff_read[0], 17);

	tx_num = data[4];
	rx_num = data[5];
	cx1_num = buff_read[10];

	all_strbuff = (unsigned char *)kmalloc(((tx_num * rx_num) + 5) * 2, GFP_KERNEL);
	mutual_cx_data = (unsigned char *)kmalloc(((tx_num * rx_num) + 1), GFP_KERNEL);

	memset(all_strbuff, 0, sizeof(char) * ((tx_num * rx_num + 5) * 2));
	size = ((tx_num * rx_num + 5) * 2);

	snprintf(buff, sizeof(buff), "%02X", 0xAA);
	strncat(all_strbuff, buff, size);

	snprintf(buff, sizeof(buff), "%02X", tx_num);
	strncat(all_strbuff, buff, size);

	snprintf(buff, sizeof(buff), "%02X", rx_num);
	strncat(all_strbuff, buff, size);

	snprintf(buff, sizeof(buff), "%02X", cx1_num);
	strncat(all_strbuff, buff, size);

	regAdd[0] = 0xD0;
	regAdd[1] = (unsigned char)((address_offset & 0xFF00) >> 8);
	regAdd[2] = (unsigned char)(address_offset & 0xFF);
	fts_read_reg(info, regAdd, 3, mutual_cx_data, ((tx_num * rx_num) + 1));

	for (j = 1; j < ((tx_num * rx_num) + 1); j++) {
			snprintf(buff, sizeof(buff), "%02X", mutual_cx_data[j]);
			strncat(all_strbuff, buff, size);
	}

	snprintf(buff, sizeof(buff), "%02X", 0xBB);
	strncat(all_strbuff, buff, size);

	fts_command(info, SENSEON);
#ifdef PHONE_KEY
	fts_command(info, KEYON);
#endif
	fts_command(info, FORCECALIBRATION);
	fts_command(info, FLUSHBUFFER);
	fts_interrupt_set(info, INT_ENABLE);

	count = snprintf(buf, TSP_BUF_SIZE, "%s\n", all_strbuff);
	kfree(all_strbuff);
	kfree(mutual_cx_data);

	return count;
}

static ssize_t fts_read_self_tune_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);
	unsigned char *all_strbuff = NULL;
	unsigned char *self_tune_data = NULL;
	unsigned char f_ix1_num = 0;
	unsigned char s_ix1_num = 0;
	unsigned char f_cx1_num = 0;
	unsigned char s_cx1_num = 0;
	char buff[CMD_STR_LEN] = {0};
	unsigned char regAdd[8];
	unsigned char buff_read[17];
	unsigned char data[FTS_EVENT_SIZE];
	unsigned int rx_num, tx_num;
	int count = 0;
	int j = 0;
	int error = 0;
	int address_offset = 0;
	int start_tx_offset = 0;
	int retry = 0;
	int size = 0;

	fts_interrupt_set(info, INT_DISABLE);
	fts_command(info, SENSEOFF);
#ifdef PHONE_KEY
	fts_command(info, KEYOFF);
#endif
	fts_command(info, FLUSHBUFFER);

	regAdd[0] = 0xB8;
	regAdd[1] = 0x20;
	regAdd[2] = 0x00;
	fts_write_reg(info, regAdd, 3);

	for (retry = 0; retry <= READ_CNT; retry++) {
		regAdd[0] = READ_ONE_EVENT;
		error = fts_read_reg(info, regAdd, 1, data, FTS_EVENT_SIZE);
		if (error)
			return -ENODEV;

		if ((data[0] == EVENTID_COMP_DATA_READ) && (data[1] == 0x20))
			break;
		else
			msleep(10);
	}

	regAdd[0] = 0xD0;
	regAdd[1] = 0x00;
	regAdd[2] = 0x50;
	fts_read_reg(info,regAdd, 3, &buff_read[0], 4);

	start_tx_offset = ((buff_read[2] << 8) | buff_read[1]);
	address_offset  = start_tx_offset + 0x10;

	regAdd[0] = 0xD0;
	regAdd[1] = (unsigned char)((start_tx_offset & 0xFF00) >> 8);
	regAdd[2] = (unsigned char)(start_tx_offset & 0xFF);
	fts_read_reg(info, regAdd, 3, &buff_read[0], 17);

	tx_num = data[4];
	rx_num = data[5];

	f_ix1_num = buff_read[10];
	s_ix1_num = buff_read[11];
	f_cx1_num = buff_read[12];
	s_cx1_num = buff_read[13];

	all_strbuff = (unsigned char *)kmalloc(((tx_num + rx_num) * 2 + 8) * 2, GFP_KERNEL);
	self_tune_data = (unsigned char *)kmalloc(((tx_num + rx_num) * 2 + 1), GFP_KERNEL);

	memset(all_strbuff, 0, sizeof(char) * (((tx_num + rx_num) * 2 + 8) * 2));
	size = (((tx_num + rx_num) * 2 + 8) * 2);

	snprintf(buff, sizeof(buff), "%02X", 0xAA);
	strncat(all_strbuff, buff, size);

	snprintf(buff, sizeof(buff), "%02X", tx_num);
	strncat(all_strbuff, buff, size);

	snprintf(buff, sizeof(buff), "%02X", rx_num);
	strncat(all_strbuff, buff, size);

	snprintf(buff, sizeof(buff), "%02X", f_ix1_num);
	strncat(all_strbuff, buff, size);

	snprintf(buff, sizeof(buff), "%02X", s_ix1_num);
	strncat(all_strbuff, buff, size);

	snprintf(buff, sizeof(buff), "%02X", f_cx1_num);
	strncat(all_strbuff, buff, size);

	snprintf(buff, sizeof(buff), "%02X", s_cx1_num);
	strncat(all_strbuff, buff, size);

	regAdd[0] = 0xD0;
	regAdd[1] = (unsigned char)((address_offset & 0xFF00) >> 8);
	regAdd[2] = (unsigned char)(address_offset & 0xFF);
	fts_read_reg(info, regAdd, 3, self_tune_data, ((tx_num + rx_num) * 2 + 1));

	for (j = 1; j < ((tx_num + rx_num) * 2 + 1); j++) {
			snprintf(buff, sizeof(buff), "%02X", self_tune_data[j]);
			strncat(all_strbuff, buff, size);
	}

	snprintf(buff, sizeof(buff), "%02X", 0xBB);
	strncat(all_strbuff, buff, size);

	fts_interrupt_set(info, INT_ENABLE);
	fts_command(info, SENSEON);
#ifdef PHONE_KEY
	fts_command(info, KEYON);
#endif
	count = snprintf(buf, TSP_BUF_SIZE, "%s\n", all_strbuff);

	kfree(all_strbuff);
	kfree(self_tune_data);

	return count;
}

static ssize_t fts_read_mutual_raw_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);
	unsigned char *all_strbuff = NULL;
	unsigned char *mutual_raw_data = NULL;
	char buff[CMD_STR_LEN] = {0};
	unsigned char regAdd[8];
	unsigned char buff_read[17];
	unsigned char data[FTS_EVENT_SIZE];
	unsigned int rx_num, tx_num;
	int count = 0;
	int j = 0;
	int error = 0;
	int address_offset = 0;
	int retry = 0;
	int size = 0;
	int size_hex = 0;

	fts_interrupt_set(info, INT_DISABLE);
	fts_command(info, SENSEON);
#ifdef PHONE_KEY
	fts_command(info, KEYON);
#endif
	fts_command(info, FLUSHBUFFER);

	regAdd[0] = 0xB8;
	regAdd[1] = 0x20;
	regAdd[2] = 0x00;
	fts_write_reg(info, regAdd, 3);

	for (retry = 0; retry <= READ_CNT; retry++) {
		regAdd[0] = READ_ONE_EVENT;
		error = fts_read_reg(info, regAdd, 1, data, FTS_EVENT_SIZE);
		if (error)
			return -ENODEV;

		if ((data[0] == EVENTID_COMP_DATA_READ) && (data[1] == 0x20))
			break;
		else
			msleep(10);
	}

	tx_num = data[4];
	rx_num = data[5];
	size = (((tx_num * rx_num) * 2 + 4) * 2);
	size_hex = (tx_num * rx_num) * 2;

	regAdd[0] = 0xD0;
	regAdd[1] = 0x00;
	regAdd[2] = 0x00;
	fts_read_reg(info, regAdd,3, &buff_read[0], 4);

	address_offset = ((buff_read[2] << 8) | buff_read[1]) + (rx_num * 2);

	all_strbuff = (unsigned char *)kmalloc(size, GFP_KERNEL);
	mutual_raw_data = (unsigned char *)kmalloc((size_hex + 1), GFP_KERNEL);

	memset(all_strbuff, 0, sizeof(char) * size);

	snprintf(buff, sizeof(buff), "%02X", 0xAA);
	strncat(all_strbuff, buff, size);

	snprintf(buff, sizeof(buff), "%02X", tx_num);
	strncat(all_strbuff, buff, size);

	snprintf(buff, sizeof(buff), "%02X", rx_num);
	strncat(all_strbuff, buff, size);

	regAdd[0] = 0xD0;
	regAdd[1] = (unsigned char)((address_offset & 0xFF00) >> 8);
	regAdd[2] = (unsigned char)(address_offset & 0xFF);
	fts_read_reg(info, regAdd, 3, mutual_raw_data, (size_hex + 1));

	for (j = 1; j < (size_hex + 1); j++) {
		snprintf(buff, sizeof(buff), "%02X", mutual_raw_data[j]);
		strncat(all_strbuff, buff, size);
	}

	snprintf(buff, sizeof(buff), "%02X", 0xBB);
	strncat(all_strbuff, buff, size);

	fts_interrupt_set(info, INT_ENABLE);
	count = snprintf(buf, TSP_BUF_SIZE, "%s\n", all_strbuff);

	kfree(all_strbuff);
	kfree(mutual_raw_data);

	return count;
}

static ssize_t fts_read_self_raw_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);
	unsigned char *all_strbuff = NULL;
	unsigned char *self_raw_data_force = NULL;
	unsigned char *self_raw_data_sense = NULL;
	char buff[CMD_STR_LEN] = {0};
	unsigned char regAdd[8];
	unsigned char buff_read[17];
	unsigned char data[FTS_EVENT_SIZE];
	unsigned int rx_num, tx_num;
	int count = 0;
	int j = 0;
	int error = 0;
	int address_offset_force = 0;
	int address_offset_sense = 0;
	int retry = 0;
	int size = 0;
	int size_hex_force = 0;
	int size_hex_sense = 0;

	fts_interrupt_set(info, INT_DISABLE);
	fts_command(info, SENSEON);
#ifdef PHONE_KEY
	fts_command(info, KEYON);
#endif
	fts_command(info, FLUSHBUFFER);

	regAdd[0] = 0xB8;
	regAdd[1] = 0x20;
	regAdd[2] = 0x00;
	fts_write_reg(info, regAdd, 3);

	for (retry = 0; retry <= READ_CNT; retry++) {
		regAdd[0] = READ_ONE_EVENT;
		error = fts_read_reg(info, regAdd, 1, data, FTS_EVENT_SIZE);
		if (error)
			return -ENODEV;

		if ((data[0] == EVENTID_COMP_DATA_READ) && (data[1] == 0x20))
			break;
		else
			msleep(10);
	}

	tx_num = data[4];
	rx_num = data[5];

	size = (((tx_num * rx_num) * 2 + 4) * 2);
	size_hex_force = tx_num * 2;
	size_hex_sense = rx_num * 2;

	regAdd[0] = 0xD0;
	regAdd[1] = 0x00;
	regAdd[2] = 0x1A;
	fts_read_reg(info, regAdd, 3, &buff_read[0], 5);

	address_offset_force = ((buff_read[2] << 8) | buff_read[1]);
	address_offset_sense = ((buff_read[4] << 8) | buff_read[3]);

	all_strbuff = (unsigned char *)kmalloc(size, GFP_KERNEL);
	self_raw_data_force = (unsigned char *)kmalloc((size_hex_force + 1), GFP_KERNEL);
	self_raw_data_sense = (unsigned char *)kmalloc((size_hex_sense + 1), GFP_KERNEL);

	memset(all_strbuff, 0, sizeof(char)*(((tx_num * rx_num) * 2 + 4) * 2));

	snprintf(buff, sizeof(buff), "%02X", 0xAA);
	strncat(all_strbuff, buff, size);

	snprintf(buff, sizeof(buff), "%02X", tx_num);
	strncat(all_strbuff, buff, size);

	snprintf(buff, sizeof(buff), "%02X", rx_num);
	strncat(all_strbuff, buff, size);

	regAdd[0] = 0xD0;
	regAdd[1] = (unsigned char)((address_offset_force & 0xFF00) >> 8);
	regAdd[2] = (unsigned char)(address_offset_force & 0xFF);
	fts_read_reg(info, regAdd, 3, self_raw_data_force, (size_hex_force + 1));

	for (j = 1; j < (size_hex_force+1); j++) {
			snprintf(buff, sizeof(buff), "%02X", self_raw_data_force[j]);
			strncat(all_strbuff, buff, size);
	}


	regAdd[0] = 0xD0;
	regAdd[1] = (unsigned char)((address_offset_sense & 0xFF00) >> 8);
	regAdd[2] = (unsigned char)(address_offset_sense & 0xFF);
	fts_read_reg(info, regAdd, 3, self_raw_data_sense, (size_hex_sense + 1));

	for (j = 1; j < (size_hex_sense + 1); j++) {
			snprintf(buff, sizeof(buff), "%02X", self_raw_data_sense[j]);
			strncat(all_strbuff, buff, size);
	}

	snprintf(buff, sizeof(buff), "%02X", 0xBB);
	strncat(all_strbuff, buff, size);

	fts_interrupt_set(info, INT_ENABLE);
	count = snprintf(buf, TSP_BUF_SIZE, "%s\n", all_strbuff);

	kfree(all_strbuff);
	kfree(self_raw_data_force);
	kfree(self_raw_data_sense);

	return count;

}

static ssize_t fts_read_mutual_strength_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);
	unsigned char *all_strbuff = NULL;
	unsigned char *mutual_strength_data = NULL;
	char buff[CMD_STR_LEN] = {0};
	unsigned char regAdd[8];
	unsigned char buff_read[17];
	unsigned char data[FTS_EVENT_SIZE];
	unsigned int rx_num, tx_num;
	int count = 0;
	int j = 0;
	int error = 0;
	int address_offset = 0;
	int retry = 0;
	int size = 0;
	int size_hex = 0;

	fts_interrupt_set(info, INT_DISABLE);
	fts_command(info, SENSEON);
#ifdef PHONE_KEY
	fts_command(info, KEYON);
#endif
	fts_command(info, FLUSHBUFFER);

	regAdd[0] = 0xB8;
	regAdd[1] = 0x20;
	regAdd[2] = 0x00;
	fts_write_reg(info, regAdd, 3);

	for (retry = 0; retry <= READ_CNT; retry++) {
		regAdd[0] = READ_ONE_EVENT;
		error = fts_read_reg(info, regAdd, 1, data, FTS_EVENT_SIZE);
		if (error)
			return -ENODEV;

		if ((data[0] == EVENTID_COMP_DATA_READ) && (data[1] == 0x20))
			break;
		else
			msleep(10);
	}

	tx_num = data[4];
	rx_num = data[5];
	size = (((tx_num * rx_num) * 2 + 4) * 2);
	size_hex = (tx_num * rx_num) * 2;

	regAdd[0] = 0xD0;
	regAdd[1] = 0x00;
	regAdd[2] = 0x04;
	fts_read_reg(info, regAdd,3, &buff_read[0], 4);

	address_offset = ((buff_read[2] << 8) | buff_read[1]) + (rx_num * 2);

	all_strbuff = (unsigned char *)kmalloc(size, GFP_KERNEL);
	mutual_strength_data = (unsigned char *)kmalloc((size_hex + 1), GFP_KERNEL);

	memset(all_strbuff, 0, sizeof(char) * size);

	snprintf(buff, sizeof(buff), "%02X", 0xAA);
	strncat(all_strbuff, buff, size);

	snprintf(buff, sizeof(buff), "%02X", tx_num);
	strncat(all_strbuff, buff, size);

	snprintf(buff, sizeof(buff), "%02X", rx_num);
	strncat(all_strbuff, buff, size);

	regAdd[0] = 0xD0;
	regAdd[1] = (unsigned char)((address_offset & 0xFF00) >> 8);
	regAdd[2] = (unsigned char)(address_offset & 0xFF);
	fts_read_reg(info, regAdd, 3, mutual_strength_data, (size_hex + 1));

	for (j = 1; j < (size_hex+1); j++) {
		snprintf(buff, sizeof(buff), "%02X", mutual_strength_data[j]);
		strncat(all_strbuff, buff, size);
	}

	snprintf(buff, sizeof(buff), "%02X", 0xBB);
	strncat(all_strbuff, buff, size);

	fts_interrupt_set(info, INT_ENABLE);
	count = snprintf(buf, TSP_BUF_SIZE, "%s\n", all_strbuff);

	kfree(all_strbuff);
	kfree(mutual_strength_data);

	return count;
}

static ssize_t fts_read_self_strength_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);
	unsigned char *all_strbuff = NULL;
	unsigned char *self_strength_data_force = NULL;
	unsigned char *self_strength_data_sense = NULL;
	char buff[CMD_STR_LEN] = {0};
	unsigned char regAdd[8];
	unsigned char buff_read[17];
	unsigned char data[FTS_EVENT_SIZE];
	unsigned int rx_num, tx_num;
	int count = 0;
	int j = 0;
	int error = 0;
	int address_offset_force = 0;
	int address_offset_sense = 0;
	int retry = 0;
	int size = 0;
	int size_hex_force = 0;
	int size_hex_sense = 0;

	fts_interrupt_set(info, INT_DISABLE);
	fts_command(info, SENSEON);
#ifdef PHONE_KEY
	fts_command(info, KEYON);
#endif
	fts_command(info, FLUSHBUFFER);

	regAdd[0] = 0xB8;
	regAdd[1] = 0x20;
	regAdd[2] = 0x00;
	fts_write_reg(info, regAdd, 3);

	for (retry = 0; retry <= READ_CNT; retry++) {
		regAdd[0] = READ_ONE_EVENT;
		error = fts_read_reg(info, regAdd, 1, data, FTS_EVENT_SIZE);
		if (error)
			return -ENODEV;

		if ((data[0] == EVENTID_COMP_DATA_READ) && (data[1] == 0x20))
			break;
		else
			msleep(10);
	}

	tx_num = data[4];
	rx_num = data[5];

	size = (((tx_num * rx_num) * 2 + 4) * 2);
	size_hex_force = tx_num * 2;
	size_hex_sense = rx_num * 2;

	regAdd[0] = 0xD0;
	regAdd[1] = 0x00;
	regAdd[2] = 0x22;
	fts_read_reg(info, regAdd, 3, &buff_read[0], 5);

	address_offset_force = ((buff_read[2] << 8) | buff_read[1]);
	address_offset_sense = ((buff_read[4] << 8) | buff_read[3]);

	all_strbuff = (unsigned char *)kmalloc(size, GFP_KERNEL);
	self_strength_data_force = (unsigned char *)kmalloc((size_hex_force + 1), GFP_KERNEL);
	self_strength_data_sense = (unsigned char *)kmalloc((size_hex_sense + 1), GFP_KERNEL);

	memset(all_strbuff, 0, sizeof(char) * (((tx_num * rx_num) * 2 + 4) * 2));

	snprintf(buff, sizeof(buff), "%02X", 0xAA);
	strncat(all_strbuff, buff, size);

	snprintf(buff, sizeof(buff), "%02X", tx_num);
	strncat(all_strbuff, buff, size);

	snprintf(buff, sizeof(buff), "%02X", rx_num);
	strncat(all_strbuff, buff, size);


	regAdd[0] = 0xD0;
	regAdd[1] = (unsigned char)((address_offset_force & 0xFF00) >> 8);
	regAdd[2] = (unsigned char)(address_offset_force & 0xFF);
	fts_read_reg(info, regAdd, 3, self_strength_data_force, (size_hex_force + 1));

	for (j = 1; j < (size_hex_force + 1); j++) {
			snprintf(buff, sizeof(buff), "%02X", self_strength_data_force[j]);
			strncat(all_strbuff, buff, size);
	}

	regAdd[0] = 0xD0;
	regAdd[1] = (unsigned char)((address_offset_sense & 0xFF00) >> 8);
	regAdd[2] = (unsigned char)(address_offset_sense & 0xFF);
	fts_read_reg(info, regAdd, 3, self_strength_data_sense, (size_hex_sense + 1));

	for (j = 1; j < (size_hex_sense+1); j++) {
			snprintf(buff, sizeof(buff), "%02X", self_strength_data_sense[j]);
			strncat(all_strbuff, buff, size);
	}

	snprintf(buff, sizeof(buff), "%02X", 0xBB);
	strncat(all_strbuff, buff, size);

	fts_interrupt_set(info, INT_ENABLE);
	count = snprintf(buf, TSP_BUF_SIZE, "%s\n", all_strbuff);

	kfree(all_strbuff);
	kfree(self_strength_data_force);
	kfree(self_strength_data_sense);

	return count;

}

static DEVICE_ATTR(fwupdate, (S_IRUGO|S_IWUSR|S_IWGRP), fts_sysfs_fwupdate_show, fts_fw_control_store);
/**/
static DEVICE_ATTR(gesture_control, (S_IRUGO|S_IWUSR|S_IWGRP), fts_gesture_control_show, fts_gesture_control_store);
static DEVICE_ATTR(gesture_data, S_IRUGO, fts_gesture_data_read, NULL);
static DEVICE_ATTR(glove_control, (S_IRUGO|S_IWUSR|S_IWGRP), fts_glove_control_show, fts_glove_control_store);
/**/
static DEVICE_ATTR(appid, (S_IRUGO), fts_sysfs_config_id_show, NULL);
static DEVICE_ATTR(update_test, (S_IRUGO), fts_fw_test_show, NULL);
static DEVICE_ATTR(cover_control, (S_IRUGO|S_IWUSR|S_IWGRP), fts_cover_control_show, fts_cover_control_store);

/** factory test */
static DEVICE_ATTR(init_test, (S_IRUGO), fts_initialization_test_show, NULL);
static DEVICE_ATTR(ito_test, (S_IRUGO), fts_ito_test_show, NULL);
static DEVICE_ATTR(touch_debug, (S_IWUSR|S_IWGRP), NULL, fts_touch_debug_store);
static DEVICE_ATTR(read_mutual_tune, (S_IRUGO), fts_read_mutual_tune_show, NULL);
static DEVICE_ATTR(read_self_tune, (S_IRUGO), fts_read_self_tune_show, NULL);
static DEVICE_ATTR(read_mutual_raw, (S_IRUGO), fts_read_mutual_raw_show, NULL);
static DEVICE_ATTR(read_self_raw, (S_IRUGO), fts_read_self_raw_show, NULL);
static DEVICE_ATTR(read_mutual_strength, (S_IRUGO), fts_read_mutual_strength_show, NULL);
static DEVICE_ATTR(read_self_strength, (S_IRUGO), fts_read_self_strength_show, NULL);


static DEVICE_ATTR(prod_test, (S_IRUGO|S_IWUSR|S_IWGRP), fts_production_test_show, fts_production_test_store);

static struct attribute *fts_attr_group[] = {
	&dev_attr_fwupdate.attr,
	&dev_attr_gesture_control.attr,
	&dev_attr_gesture_data.attr,
	&dev_attr_glove_control.attr,
	&dev_attr_appid.attr,
	&dev_attr_update_test.attr,
	&dev_attr_cover_control.attr,

	&dev_attr_init_test.attr,
	&dev_attr_ito_test.attr,
	&dev_attr_touch_debug.attr,
	&dev_attr_read_mutual_tune.attr,
	&dev_attr_read_self_tune.attr,
	&dev_attr_read_mutual_raw.attr,
	&dev_attr_read_self_raw.attr,
	&dev_attr_read_mutual_strength.attr,
	&dev_attr_read_self_strength.attr,
	&dev_attr_prod_test.attr,
	NULL,
};

static int fts_read_reg(struct fts_ts_info *info, unsigned char *reg, int cnum,
						unsigned char *buf, int num);

static int fts_write_reg(struct fts_ts_info *info, unsigned char *reg,
						 unsigned short len);


static ssize_t fts_i2c_wr_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);
	int i ;
	char buff[16];
	memset(Out_buff, 0x00, ARRAY_SIZE(Out_buff));
	if (byte_count_read == 0) {
		snprintf(Out_buff, sizeof(Out_buff), "{FAILED}");
		return snprintf(buf, TSP_BUF_SIZE, "{%s}\n", Out_buff);
	}

	snprintf(buff, sizeof(buff), "{");
	strncat(Out_buff, buff, 512);
	for (i = 0; i < (byte_count_read + 2); i++) {
		if ((i == 0)) {
			char temp_byte_count_read = (byte_count_read >> 8) & 0xFF;
			snprintf(buff, sizeof(buff), "%02X", temp_byte_count_read);
		} else if (i == 1) {
			char temp_byte_count_read = (byte_count_read) & 0xFF;
			snprintf(buff, sizeof(buff), "%02X", temp_byte_count_read);

		} else
			snprintf(buff, sizeof(buff), "%02X", info->cmd_wr_result[i-2]);

		strncat(Out_buff, buff, 512);
		if (i < (byte_count_read + 1)) {
			snprintf(buff, sizeof(buff), " ");
			strncat(Out_buff, buff, 512);
		}
	}

	snprintf(buff, sizeof(buff), "}");
	strncat(Out_buff, buff, 512);
	return snprintf(buf, TSP_BUF_SIZE, "%s\n", Out_buff);
}

static ssize_t fts_i2c_wr_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);
	unsigned char pAddress[8] = {0};
	unsigned int byte_count = 0;
	int i;
	unsigned int data[8] = {0};

	memset(info->cmd_wr_result, 0x00, ARRAY_SIZE(info->cmd_wr_result));
	sscanf(buf, "%x %x %x %x %x %x %x %x ", (data + 7), (data), (data + 1), (data + 2), (data + 3), (data + 4), (data + 5), (data + 6));

	byte_count = data[7];

	for (i = 0; i < 7; i++)
		pAddress[i] = (unsigned char)data[i];

	byte_count_read = data[byte_count - 1];

	ret = fts_write_reg(info, pAddress, 3);
	msleep(20);
	ret = fts_read_reg(info, &pAddress[3], (byte_count - 4), info->cmd_wr_result, byte_count_read);

	if (ret)
		dev_err(dev, "Unable to read register \n");

	return count;
}

static ssize_t fts_i2c_read_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);
	int i;
	char buff[16];

	memset(Out_buff, 0x00, ARRAY_SIZE(Out_buff));
	if (byte_count_read == 0) {
		snprintf(Out_buff, sizeof(Out_buff), "{FAILED}");
		return snprintf(buf, TSP_BUF_SIZE, "{%s}\n", Out_buff);
	}

	snprintf(buff, sizeof(buff), "{");
	strncat(Out_buff, buff, 512);
	for (i = 0; i < (byte_count_read + 2); i++) {
		if ((i == 0)) {
			char temp_byte_count_read = (byte_count_read >> 8) & 0xFF;
			snprintf(buff, sizeof(buff), "%02X", temp_byte_count_read);
		} else if (i == 1) {
			char temp_byte_count_read = (byte_count_read) & 0xFF;
			snprintf(buff, sizeof(buff), "%02X", temp_byte_count_read);
		} else
			snprintf(buff, sizeof(buff), "%02X", info->cmd_read_result[i-2]);

		strncat(Out_buff, buff, 512);
		if (i < (byte_count_read + 1)) {
			snprintf(buff, sizeof(buff), " ");
			strncat(Out_buff, buff, 512);
		}
	}

	snprintf(buff, sizeof(buff), "}");
	strncat(Out_buff, buff, 512);

	return snprintf(buf, TSP_BUF_SIZE, "%s\n", Out_buff);
}

static ssize_t fts_i2c_read_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);
	unsigned char pAddress[8] = {0};
	unsigned int byte_count = 0;
	int i;
	unsigned int data[8] = {0};

	byte_count_read = 0;
	memset(info->cmd_read_result, 0x00, ARRAY_SIZE(info->cmd_read_result));
	sscanf(buf, "%x %x %x %x %x %x %x %x ", (data + 7), (data), (data + 1), (data + 2), (data + 3), (data + 4), (data + 5), (data + 6));
	byte_count = data[7];


	if (byte_count > 7)
		return count;

	for (i = 0; i < byte_count; i++)
		pAddress[i] = (unsigned char)data[i];

	byte_count_read = data[byte_count-1];

	ret = fts_read_reg(info, pAddress, (byte_count - 1), info->cmd_read_result, byte_count_read);
	if (ret)
		dev_err(dev, "Unable to read register\n");

	return count;
}


static ssize_t fts_i2c_write_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);
	return snprintf(buf, TSP_BUF_SIZE, "%s", info->cmd_write_result);

}

static ssize_t fts_i2c_write_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);
	unsigned short byte_count = 0;
	int i ;

	memset(pAddress_i2c, 0x00, ARRAY_SIZE(pAddress_i2c));
	memset(info->cmd_write_result, 0x00, ARRAY_SIZE(info->cmd_write_result));
	sscanf(buf, "%x ", data);
	byte_count = data[0];

	if (byte_count <= 512) {
		for (i = 1; i <= byte_count ; i++)
			sscanf(&buf[3 * i] , "%x ", (data + (i - 1)));
	} else
		snprintf(info->cmd_write_result, sizeof(info->cmd_write_result), "{Write NOT OK}\n");

	for (i = 0; i < byte_count; i++)
		pAddress_i2c[i] = (unsigned char)data[i];

	if ((pAddress_i2c[0] == 0xb3) && (pAddress_i2c[3] == 0xb1)) {
		ret = fts_write_reg(info, pAddress_i2c, 3);
		msleep(20);
		ret = fts_write_reg(info, &pAddress_i2c[3], byte_count-3);
	} else
		ret = fts_write_reg(info, pAddress_i2c, byte_count);

	if (ret)
		snprintf(info->cmd_write_result, sizeof(info->cmd_write_result), "{Write NOT OK}\n");
	else
		snprintf(info->cmd_write_result, sizeof(info->cmd_write_result), "{Write OK}\n");

	return count;
}

static DEVICE_ATTR(iread, (S_IWUSR|S_IWGRP), NULL, fts_i2c_read_store);
static DEVICE_ATTR(iread_result, (S_IWUSR|S_IWGRP), fts_i2c_read_show, NULL);
static DEVICE_ATTR(iwr, (S_IWUSR|S_IWGRP), NULL, fts_i2c_wr_store);
static DEVICE_ATTR(iwr_result, (S_IWUSR|S_IWGRP), fts_i2c_wr_show, NULL);
static DEVICE_ATTR(iwrite, (S_IWUSR|S_IWGRP), NULL, fts_i2c_write_store);
static DEVICE_ATTR(iwrite_result, (S_IWUSR|S_IWGRP), fts_i2c_write_show, NULL);


static struct attribute *i2c_cmd_attributes[] = {
	&dev_attr_iread.attr,
	&dev_attr_iread_result.attr,
	&dev_attr_iwr.attr,
	&dev_attr_iwr_result.attr,
	&dev_attr_iwrite.attr,
	&dev_attr_iwrite_result.attr,
	NULL,
};

static struct attribute_group i2c_cmd_attr_group = {
	.attrs = i2c_cmd_attributes,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void fts_early_suspend(struct early_suspend *h)
{
	struct fts_ts_info *info;
	struct device *dev;

	info = container_of(h, struct fts_ts_info, early_suspend);
	dev = &info->client->dev;
	dev_info(dev, "FTS Early Suspend entered\n");
	if (fts_suspend(info->client, PMSG_SUSPEND))
		dev_err(&info->client->dev, "Early suspend failed\n");
	dev_info(dev, "FTS Early Suspended\n");
}

static void fts_late_resume(struct early_suspend *h)
{
	struct fts_ts_info *info;
	struct device *dev;

	info = container_of(h, struct fts_ts_info, early_suspend);
	dev = &info->client->dev;
	dev_info(dev, "FTS Early Resume entered\n");
	if (fts_resume(info->client))
		dev_err(&info->client->dev, "Late resume failed\n");
	dev_info(dev, "FTS Early Resumed\n");
}
#endif /* CONFIG_HAS_EARLYSUSPEND */

static int fts_command(struct fts_ts_info *info, unsigned char cmd)
{
	unsigned char regAdd = cmd;
	int ret;

	ret = fts_write_reg(info, &regAdd, sizeof(regAdd));

	return ret;
}

static int fts_systemreset(struct fts_ts_info *info)
{
	int ret;
	unsigned char regAdd[4] = {0xB6, 0x00, 0x23, 0x01};

	dev_dbg(info->dev, "Doing a system reset\n");

	ret = fts_write_reg(info, regAdd, sizeof(regAdd));

	usleep_range(5000, 6000);

	return ret;
}

static int fts_get_fw_version(struct fts_ts_info *info)
{
	unsigned char val[8];
	unsigned char regAdd[3] = {0xB6, 0x00, 0x07};
	unsigned char regAdd2[4] = {0xB2, 0x00, 0x01, 0x08};
	unsigned char regAdd3 = READ_ONE_EVENT;
	int error;

	fts_interrupt_set(info, INT_DISABLE);
	msleep(10);
	fts_command(info, FLUSHBUFFER);
	error = fts_read_reg(info, regAdd, sizeof(regAdd), val, sizeof(val));
	/*check for chip id*/
	if ((val[1] != FTS_ID0) || (val[2] != FTS_ID1)) {
		dev_err(info->dev,
			"Wrong version id (read 0x%02x%02x, expected 0x%02x%02x)\n",
				val[1], val[2], FTS_ID0, FTS_ID1);
		return -ENODEV;
	} else {
		info->fw_version = (val[5] << 8) | val[4];
	}

	fts_write_reg(info, regAdd2, sizeof(regAdd2));
	error = fts_read_reg(info, &regAdd3, sizeof(regAdd3), val, FTS_EVENT_SIZE);
	if (error) {
		info->config_id = 0;
		return -ENODEV;
	} else {
		info->config_id = (val[3] << 8) | val[4];
		pr_err("!!!read 0xAA result: val[0]:%x,val[1]:%x,val[2]:%x,val[3]:%x,val[4]:%x,val[5]:%x,val[6]:%x,val[7]:%x \n", val[0], val[1], val[2], val[3], val[4], val[5], val[6], val[7]);
	}

	fts_interrupt_set(info, INT_ENABLE);

	return 0;
}

static int fts_flash_status(struct fts_ts_info *info,
				int timeout, unsigned int steps)
{
	int ret, status;
	unsigned char data;
	unsigned char regAdd[2];

	do {
		regAdd[0] = FLASH_READ_STATUS;
		regAdd[1] = 0;

		msleep(20);

		ret = fts_read_reg(info, regAdd, sizeof(regAdd), &data, sizeof(data));
		if (ret)
			status = FLASH_STATUS_UNKNOWN;
		else
			status = (data & 0x01) ? FLASH_STATUS_BUSY : FLASH_STATUS_READY;

		if (status == FLASH_STATUS_BUSY) {
			timeout -= steps;
			msleep(steps);
		}

	} while ((status == FLASH_STATUS_BUSY) && (timeout));

	return status;
}


static int fts_flash_unlock(struct fts_ts_info *info)
{
	int ret;
	unsigned char regAdd[4] = {FLASH_UNLOCK, FLASH_UNLOCK_CODE_0, FLASH_UNLOCK_CODE_1, 0x00};

	ret = fts_write_reg(info, regAdd, sizeof(regAdd));

	if (ret)
		dev_err(info->dev, "Cannot unlock flash\n");
	else
		dev_dbg(info->dev, "Flash unlocked\n");

	return ret;
}

static int fts_flash_load(struct fts_ts_info *info,
			int cmd, int address, const char *data, int size)
{
	int ret;
	unsigned char *cmd_buf;
	unsigned int loaded;

	cmd_buf = kmalloc(FLASH_LOAD_COMMAND_SIZE, GFP_KERNEL);
	if (cmd_buf == NULL) {
		dev_err(info->dev, "Out of memory when programming flash\n");
		return -ENOMEM;
	}

	loaded = 0;
	while (loaded < size) {
		cmd_buf[0] = cmd;
		cmd_buf[1] = (address >> 8) & 0xFF;
		cmd_buf[2] = (address) & 0xFF;

		memcpy(&cmd_buf[3], data, FLASH_LOAD_CHUNK_SIZE);
		ret = fts_write_reg(info, cmd_buf, FLASH_LOAD_COMMAND_SIZE);
		if (ret) {
			dev_err(info->dev, "Cannot load firmware in RAM\n");
			break;
		}

		data += FLASH_LOAD_CHUNK_SIZE;
		loaded += FLASH_LOAD_CHUNK_SIZE;
		address += FLASH_LOAD_CHUNK_SIZE;
	}

	kfree(cmd_buf);

	return (loaded == size) ? 0 : -1;
}


static int fts_flash_erase(struct fts_ts_info *info, int cmd)
{
	int ret;
	unsigned char regAdd = cmd;

	ret = fts_write_reg(info, &regAdd, sizeof(regAdd));

	if (ret)
		dev_err(info->dev, "Cannot erase flash\n");
	else
		dev_dbg(info->dev, "Flash erased\n");

	return ret;
}


static int fts_flash_program(struct fts_ts_info *info, int cmd)
{
	int ret;
	unsigned char regAdd = cmd;

	ret = fts_write_reg(info, &regAdd, sizeof(regAdd));

	if (ret)
		dev_err(info->dev, "Cannot program flash\n");
	else
		dev_dbg(info->dev, "Flash programmed\n");

	return ret;
}


static int fts_fw_upgrade(struct fts_ts_info *info, int mode, int fw_forceupdate, int crc_err)
{
	int ret;
	const struct firmware *fw = NULL;
	unsigned char *data = NULL;
	unsigned int size;
	int updata_loop = 0;
	int status, fw_ver = 0, config_ver = 0;
	int program_command, erase_command, load_command, load_address = 0;

	info->fwupdate_stat = 1;

	ret = request_firmware(&fw, info->bdata->fw_name, info->dev);
	if (ret)
		return ret;

	if (fw->size == 0) {
		dev_err(info->dev, "Wrong firmware file '%s'\n", info->bdata->fw_name);
		goto fw_done;
	}

	data = (unsigned char *)fw->data;
	size = fw->size;
	fw_ver = le_to_uint(&data[FILE_FW_VER_OFFSET]);
	config_ver = be_to_uint(&data[FILE_CONFIG_VER_OFFSET]);

	ret = fts_get_fw_version(info);
	if (ret)
		dev_err(info->dev, "%s: can not get fw version!\n", __func__);

	dev_err(info->dev, "%s: tp:fw_version = %x, config_id = %x. bin: fw_ver = %x, config_ver = %x\n", __func__,
		info->fw_version, info->config_id, fw_ver, config_ver);

	if (fw_ver != info->fw_version || config_ver != info->config_id || fw_forceupdate == 1)
		mode = MODE_RELEASE_AND_CONFIG_128;
	else {
		info->fwupdate_stat = 0;
		dev_err(info->dev, "%s: no need to update", __func__);
		return 0;
	}

fts_updat:
	dev_dbg(info->dev, "Flash programming...\n");
	ret = fts_systemreset(info);
	if (ret) {
		dev_warn(info->dev, "Cannot reset the device 00\n");
		goto fw_done;
	}
	fts_wait_controller_ready(info);

	switch (mode) {
	case MODE_CONFIG_ONLY:
		program_command = FLASH_PROGRAM;
		erase_command = FLASH_ERASE;
		load_command = FLASH_LOAD_FIRMWARE_UPPER_64K;
		load_address = FLASH_LOAD_INFO_BLOCK_OFFSET;
		break;
	case MODE_RELEASE_AND_CONFIG_128:
		/* skip 32 bytes header */
		data += 32;
		size = size - 32;
		/* fall throug */
	case MODE_RELEASE_ONLY:
		program_command = FLASH_PROGRAM;
		erase_command = FLASH_ERASE;
		load_command = FLASH_LOAD_FIRMWARE_LOWER_64K;
		load_address = FLASH_LOAD_FIRMWARE_OFFSET;
		break;
	default:
		/* should never be here, already checked mode value before */
		break;
	}

	dev_info(info->dev, "1) checking for status.\n");
	status = fts_flash_status(info, 1000, 100);
	if ((status == FLASH_STATUS_UNKNOWN) || (status == FLASH_STATUS_BUSY)) {
		dev_err(info->dev, "Wrong flash status 1\n");
		goto fw_done;
	}

	dev_info(info->dev, "2) unlock the flash.\n");
	ret = fts_flash_unlock(info);
	if (ret) {
		dev_err(info->dev, "Cannot unlock the flash device\n");
		goto fw_done;
	}

	/* wait for a while */
	status = fts_flash_status(info, 3000, 100);
	if ((status == FLASH_STATUS_UNKNOWN) || (status == FLASH_STATUS_BUSY)) {
		dev_err(info->dev, "Wrong flash status 2\n");
		goto fw_done;
	}

	dev_info(info->dev, "3) load the program.\n");
	if (load_command == FLASH_LOAD_FIRMWARE_LOWER_64K) {
		ret = fts_flash_load(info, load_command, load_address, data, FLASH_SIZE_F0_CMD);
		load_command = FLASH_LOAD_FIRMWARE_UPPER_64K;
		if ((crc_err == 0) && (size == (FLASH_SIZE_FW_CONFIG + FLASH_SIZE_CXMEM)))
			size = size - FLASH_SIZE_CXMEM;

		ret = fts_flash_load(info, load_command, load_address, (data+FLASH_SIZE_F0_CMD), (size-FLASH_SIZE_F0_CMD));
	} else
		ret = fts_flash_load(info, load_command, load_address, data, size);
	if (ret) {
		dev_err(info->dev,
			"Cannot load program to for the flash device\n");
		goto fw_done;
	}

	/* wait for a while */
	status = fts_flash_status(info, 3000, 100);
	if ((status == FLASH_STATUS_UNKNOWN) || (status == FLASH_STATUS_BUSY)) {
		dev_err(info->dev, "Wrong flash status 3\n");
		goto fw_done;
	}

	dev_info(info->dev, "4) erase the flash.\n");
	ret = fts_flash_erase(info, erase_command);
	if (ret) {
		dev_err(info->dev, "Cannot erase the flash device\n");
		goto fw_done;
	}

	/* wait for a while */
	dev_info(info->dev, "5) checking for status.\n");
	status = fts_flash_status(info, 3000, 100);
	if ((status == FLASH_STATUS_UNKNOWN) || (status == FLASH_STATUS_BUSY)) {
		dev_err(info->dev, "Wrong flash status 4\n");
		goto fw_done;
	}

	dev_info(info->dev, "6) program the flash.\n");
	ret = fts_flash_program(info, program_command);
	if (ret) {
		dev_err(info->dev, "Cannot program the flash device\n");
		goto fw_done;
	}

	/* wait for a while */
	status = fts_flash_status(info, 3000, 100);
	if ((status == FLASH_STATUS_UNKNOWN) || (status == FLASH_STATUS_BUSY)) {
		dev_err(info->dev, "Wrong flash status 5\n");
		goto fw_done;
	}

	dev_info(info->dev, "Flash programming: done.\n");

	dev_info(info->dev, "Perform a system reset\n");
	ret = fts_systemreset(info);
	if (ret) {
		dev_warn(info->dev, "Cannot reset the device\n");
		goto fw_done;
	}
	fts_wait_controller_ready(info);
	fts_interrupt_set(info, INT_ENABLE);
	ret = fts_get_fw_version(info);
	if (ret) {
		dev_warn(info->dev, "Cannot retrieve firmware version\n");
		goto fw_done;
	}

	dev_err(info->dev, "%s: tp:fw_version = %x, config_id = %x. bin: fw_ver = %x, config_ver = %x\n", __func__,
			info->fw_version, info->config_id, fw_ver, config_ver);

	if (fw_ver == info->fw_version && config_ver == info->config_id) {
		info->fwupdate_stat = 2;
		dev_err(info->dev, "%s: firmware update OK!", __func__);
	} else {
		if (updata_loop < 2) {
			updata_loop++;
			mode = MODE_RELEASE_ONLY;
			dev_err(info->dev, "%s: firmware updata failed, update again %d******************\n", __func__, updata_loop);
			goto fts_updat;
		}
		dev_err(info->dev, "%s: firmware update failed!", __func__);
		ret = -1;
	}

	dev_info(info->dev, "New firmware version 0x%04x installed\n", info->fw_version);

fw_done:
	release_firmware(fw);

	return ret;
}


static void fts_interrupt_set(struct fts_ts_info *info, int enable)
{
	unsigned char regAdd[4] = { 0xB6, 0x00, 0x1C, enable };

	fts_write_reg(info, regAdd, 4);
}

static inline unsigned char *fts_next_event(unsigned char *evt)
{
	/* Nothing to do with this event, moving to the next one */
	evt += FTS_EVENT_SIZE;

	/* the previous one was the last event ?  */
	return (evt[-1] & 0x1F) ? evt : NULL;
}


/* EventId : 0x00 */
static unsigned char *fts_nop_event_handler(struct fts_ts_info *info,
			unsigned char *event)
{
	return fts_next_event(event);
}


/* EventId : 0x03 */
static unsigned char *fts_enter_pointer_event_handler(struct fts_ts_info *info,
			unsigned char *event)
{
	unsigned char touchId, touchcount;
	int x;
	int y;
	int z;

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

#ifdef CONFIG_EXYNOS_TOUCH_DAEMON
	if (exynos_touch_daemon_data.record == 1) {
		if (exynos_touch_daemon_data.tp.count < TOUCHPOINT) {
			exynos_touch_daemon_data.tp.x[exynos_touch_daemon_data.tp.count] = x;
			exynos_touch_daemon_data.tp.y[exynos_touch_daemon_data.tp.count] = y;
			exynos_touch_daemon_data.tp.wx[exynos_touch_daemon_data.tp.count] = z;
			exynos_touch_daemon_data.tp.wy[exynos_touch_daemon_data.tp.count] = z;
			exynos_touch_daemon_data.tp.count++;
		} else {
			printk("%s: Recordable touch point exceeds %d\n", __func__, TOUCHPOINT);
			exynos_touch_daemon_data.record = 0;
		}
	}
#endif

	input_mt_slot(info->input_dev, touchId);
	input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 1);

#ifdef CONFIG_JANUARY_BOOSTER
	if (touchcount >= 1) {
		janeps_input_report(PRESS, x, y);
	}
#endif

	if (touchcount == 1) {
		input_report_key(info->input_dev, BTN_TOUCH, 1);
		input_report_key(info->input_dev, BTN_TOOL_FINGER, 1);
	}

	input_report_abs(info->input_dev, ABS_MT_POSITION_X, x);
	input_report_abs(info->input_dev, ABS_MT_POSITION_Y, y);
	input_report_abs(info->input_dev, ABS_MT_TOUCH_MAJOR, z);
	input_report_abs(info->input_dev, ABS_MT_TOUCH_MINOR, z);
	input_report_abs(info->input_dev, ABS_MT_PRESSURE, z);

no_report:
	return fts_next_event(event);
}


/* EventId : 0x04 */
static unsigned char *fts_leave_pointer_event_handler(struct fts_ts_info *info,
			unsigned char *event)
{
	unsigned char touchId, touchcount;
#ifdef CONFIG_JANUARY_BOOSTER
	int x;
	int y;

	x = (event[2] << 4) | (event[4] & 0xF0) >> 4;
	y = (event[3] << 4) | (event[4] & 0x0F);

	if (x == X_AXIS_MAX)
		x--;
	if (y == Y_AXIS_MAX)
		y--;
#endif

	touchId = event[1] & 0x0F;
	touchcount = (event[1] & 0xF0) >> 4;
	__clear_bit(touchId, &info->touch_id);

	input_mt_slot(info->input_dev, touchId);
	input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 0);
	if (touchcount == 0) {
		input_report_key(info->input_dev, BTN_TOUCH, 0);
		input_report_key(info->input_dev, BTN_TOOL_FINGER, 0);
#ifdef CONFIG_EXYNOS_TOUCH_DAEMON
		if (exynos_touch_daemon_data.record == 1) {
			printk("%s Touch point recording is completed ad %d points\n", __func__, exynos_touch_daemon_data.tp.count);
			exynos_touch_daemon_data.record = 0;
		}
#endif
#ifdef CONFIG_JANUARY_BOOSTER
		janeps_input_report(RELEASE, x, y);
#endif
	}

	return fts_next_event(event);
}

/* EventId : 0x05 */
#define fts_motion_pointer_event_handler fts_enter_pointer_event_handler


/* EventId : 0x09 */
#define fts_hover_motion_pointer_event_handler fts_hover_enter_pointer_event_handler

/* EventId : 0x0E */
static unsigned char *fts_button_status_event_handler(struct fts_ts_info *info,
			unsigned char *event)
{
	int i;
	unsigned int buttons, changed;

	dev_dbg(info->dev, "Received event 0x%02x\n", event[0]);

	/* get current buttons status */
	buttons = event[1] | (event[2] << 8);

	/* check what is changed */
	changed = buttons ^ info->buttons;

	for (i = 0; i < 16; i++)
		if (changed & (1 << i))
			input_report_key(info->input_dev, BTN_0 + i, (!(info->buttons & (1 << i))));

	/* save current button status */
	info->buttons = buttons;

	dev_dbg(info->dev, "Event 0x%02x -  SS = 0x%02x, MS = 0x%02x\n",
			event[0], event[1], event[2]);

	return fts_next_event(event);
}


/* EventId : 0x0F */
static unsigned char *fts_error_event_handler(struct fts_ts_info *info,
			unsigned char *event)
{
	int error;
	int i;
	dev_dbg(info->dev, "Received event 0x%02x\n", event[0]);

	if (event[1] == 0x0a) {
		if (info->bus_reg) {
			error = regulator_disable(info->bus_reg);
			if (error < 0) {
				dev_err(info->dev,
					"%s: Failed to enable bus pullup regulator\n", __func__);
			}
		}
		if (info->pwr_reg) {
			error = regulator_disable(info->pwr_reg);
			if (error < 0) {
				dev_err(info->dev,
					"%s: Failed to enable power regulator\n", __func__);
			}
		}

		msleep(300);
		if (info->pwr_reg)
		    error = regulator_enable(info->pwr_reg);

		if (info->bus_reg)
			error = regulator_enable(info->bus_reg);
		msleep(300);

		for (i = 0; i < TOUCH_ID_MAX; i++) {
			input_mt_slot(info->input_dev, i);
			input_mt_report_slot_state(info->input_dev,
					(i < FINGER_MAX) ? MT_TOOL_FINGER : MT_TOOL_PEN, 0);
		}
		input_sync(info->input_dev);

		error = fts_systemreset(info);
		fts_wait_controller_ready(info);
		error += fts_command(info, FORCECALIBRATION);
		error += fts_command(info, SENSEON);
#ifdef PHONE_KEY
		error += fts_command(info, KEYON);
#endif
		error += fts_command(info, FLUSHBUFFER);
		fts_interrupt_set(info, INT_ENABLE);
		if (error)
			dev_err(info->dev, "%s: Cannot reset device\n", __func__);
		if (event[2] >= 0x80) {
			dev_err(info->dev, "ESD or Low battery at gesture mode recovery\n");
			fts_set_gesture(info);
			fts_command(info, ENTER_GESTURE_MODE);
			fts_set_sensor_mode(info, MODE_GESTURE);
			info->gesture_enable = 1;
		}
	}

	return fts_next_event(event);
}


/* EventId : 0x10 */
static unsigned char *fts_controller_ready_event_handler(
			struct fts_ts_info *info, unsigned char *event)
{
	dev_dbg(info->dev, "Received event 0x%02x\n", event[0]);
	info->touch_id = 0;
	info->buttons = 0;
	input_sync(info->input_dev);
	return fts_next_event(event);
}


/* EventId : 0x16 */
static unsigned char *fts_status_event_handler(
			struct fts_ts_info *info, unsigned char *event)
{
	dev_dbg(info->dev, "Received event 0x%02x\n", event[0]);

	switch (event[1]) {
	case FTS_STATUS_MUTUAL_TUNE:
	case FTS_STATUS_SELF_TUNE:
	case FTS_FORCE_CAL_SELF_MUTUAL:
		complete(&info->cmd_done);
		break;

	case FTS_FLASH_WRITE_CONFIG:
	case FTS_FLASH_WRITE_COMP_MEMORY:
	case FTS_FORCE_CAL_SELF:
	case FTS_WATER_MODE_ON:
	case FTS_WATER_MODE_OFF:
	default:
		dev_err(info->dev,
			"Received unhandled status event = 0x%02x\n", event[1]);
		break;
	}

	return fts_next_event(event);
}


/* EventId : 0x05 */
#define fts_motion_pointer_event_handler fts_enter_pointer_event_handler

static unsigned char *fts_pen_enter_event_handler(
			struct fts_ts_info *info, unsigned char *event)
{
	unsigned char touchId;
	int x, y, z;
	int eraser, barrel;

	dev_dbg(info->dev, "Received event 0x%02x\n", event[0]);

	/* always use last position as touchId */
	touchId = TOUCH_ID_MAX;

	__set_bit(touchId, &info->touch_id);

	x = (event[2] << 4) | (event[4] & 0xF0) >> 4;
	y = (event[3] << 4) | (event[4] & 0x0F);
	z = (event[5] & 0xFF);

	eraser = (event[1] * 0x80) >> 7;
	barrel = (event[1] * 0x40) >> 6;

	if (x == X_AXIS_MAX)
		x--;

	if (y == Y_AXIS_MAX)
		y--;

	input_mt_slot(info->input_dev, touchId);
	input_report_abs(info->input_dev, ABS_MT_TRACKING_ID, touchId);
	input_report_abs(info->input_dev, ABS_MT_POSITION_X, x);
	input_report_abs(info->input_dev, ABS_MT_POSITION_Y, y);
	input_report_abs(info->input_dev, ABS_MT_TOUCH_MAJOR, z);
	input_report_abs(info->input_dev, ABS_MT_TOUCH_MINOR, z);
	input_report_abs(info->input_dev, ABS_MT_PRESSURE, z);

	input_report_key(info->input_dev, BTN_STYLUS, eraser);
	input_report_key(info->input_dev, BTN_STYLUS2, barrel);
	input_mt_report_slot_state(info->input_dev, MT_TOOL_PEN, 1);

	return fts_next_event(event);
}


/* EventId : 0x24 */
static unsigned char *fts_pen_leave_event_handler(
			struct fts_ts_info *info, unsigned char *event)
{
	unsigned char touchId;

	dev_dbg(info->dev, "Received event 0x%02x\n", event[0]);

	/* always use last position as touchId */
	touchId = TOUCH_ID_MAX;

	__clear_bit(touchId, &info->touch_id);

	input_report_key(info->input_dev, BTN_STYLUS, 0);
	input_report_key(info->input_dev, BTN_STYLUS2, 0);

	input_mt_slot(info->input_dev, touchId);
	input_report_abs(info->input_dev, ABS_MT_TRACKING_ID, -1);

	dev_dbg(info->dev,
		"Event 0x%02x - release ID[%d]\n",
		event[0], touchId);

	return fts_next_event(event);
}


/* EventId : 0x25 */
#define fts_pen_motion_event_handler fts_pen_enter_event_handler

static void fts_event_handler(struct work_struct *work)
{
	struct fts_ts_info *info;
	int error, error1;
	int left_events;
	unsigned char regAdd;
	unsigned char data[FTS_EVENT_SIZE * (FTS_FIFO_MAX)] = {0};
	unsigned char *event = NULL;
	unsigned char eventId;
	event_dispatch_handler_t event_handler;

	info = container_of(work, struct fts_ts_info, work);
	/*
	 * to avoid reading all FIFO, we read the first event and
	 * then check how many events left in the FIFO
	 */

	wake_lock_timeout(&info->wakelock, HZ);
	regAdd = READ_ONE_EVENT;
	error = fts_read_reg(info, &regAdd,
			sizeof(regAdd), data, FTS_EVENT_SIZE);

	if (!error) {

		left_events = data[7] & 0x1F;
		if ((left_events > 0) && (left_events < FTS_FIFO_MAX)) {
			/*
			 * Read remaining events.
			 */
			regAdd = READ_ALL_EVENT;
			error1 = fts_read_reg(info, &regAdd, sizeof(regAdd),
						&data[FTS_EVENT_SIZE],
						left_events * FTS_EVENT_SIZE);

			/*
			 * Got an error reading remining events,
			 * process at least * the first one that was
			 * raeding fine.
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

	fts_interrupt_enable(info);
}

static int cx_crc_check(struct fts_ts_info *info)
{
	unsigned char regAdd1[3] = {0xB6, 0x00, 0x86};
	unsigned char val[4];
	unsigned char crc_status;
	unsigned int error;

	error = fts_read_reg(info, regAdd1, sizeof(regAdd1), val, sizeof(val));
	if (error) {
		dev_err(info->dev, "Cannot read crc status\n");
		return -ENOENT;
	}

	crc_status = val[1] & 0x02;
	if (crc_status != 0) {
		tp_log("%s:fts CRC status = %d \n", __func__, crc_status);
	}

		return crc_status;
}
static void fts_fw_update_auto(struct work_struct *work)
{
	int retval;
	int retval1;
	int ret;
	int error = 0;
	struct fts_ts_info *info;
	struct delayed_work *fwu_work = container_of(work, struct delayed_work, work);
	int crc_status = 0;

	info = container_of(fwu_work, struct fts_ts_info, fwu_work);

	if ((cx_crc_check(info)) != 0) {
		tp_log("%s: CRC Error 128 K firmware update \n", __func__);
		crc_status = 1;
	} else {
		crc_status = 0;
		tp_log("%s:NO CRC Error 124 K firmware update \n", __func__);
	}
	/*check firmware*/
	info->fw_force = 0;
	retval = fts_fw_upgrade(info, 0, 0, crc_status);
	if (retval) {
		tp_log("%s: firmware update failed and retry!\n", __func__);
		fts_chip_powercycle(info);
		fts_systemreset(info);
		fts_wait_controller_ready(info);

		retval1 = fts_fw_upgrade(info, 0, 0, crc_status);
		if (retval1) {
			dev_err(info->dev, "%s: firwmare update failed again!\n", __func__);
			return ;
		}
	}

	ret = fts_get_init_status(info);
	error = fts_command(info, FORCECALIBRATION);
	error += fts_command(info, SENSEON);
	error += fts_command(info, FLUSHBUFFER);
	if (ret != 0)
		fts_chip_initialization(info);
}

static int fts_chip_initialization(struct fts_ts_info *info)
{
	int ret2 = 0;
	int retry;
	int error;
	int initretrycnt = 0;

	for (retry = 0; retry <= INIT_FLAG_CNT; retry++) {
		fts_chip_powercycle(info);
		fts_systemreset(info);
		fts_wait_controller_ready(info);
		ret2 = fts_init_flash_reload(info);
		if (ret2 == 0)
			break;
		initretrycnt++;
		dev_dbg(info->dev, "initialization cycle count = %04d\n", initretrycnt);
	}
	if (ret2 != 0) {
		dev_dbg(info->dev, "fts initialization 3 times error\n");
		error = fts_systemreset(info);
		fts_wait_controller_ready(info);
		error += fts_command(info, SENSEON);
		error += fts_command(info, FORCECALIBRATION);
#ifdef PHONE_KEY
			fts_command(info, KEYON);
#endif
		error += fts_command(info, FLUSHBUFFER);
		fts_interrupt_set(info, INT_ENABLE);
		if (error)
			tp_log("%s: Cannot reset the device----------\n", __func__);
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

static int fts_wait_controller_ready(struct fts_ts_info *info)
{
	unsigned int retry = 0, error = 0;

	unsigned char regAdd[8];
	unsigned char data[FTS_EVENT_SIZE];
	/* Read controller ready event*/
	for (retry = 0; retry <= CNRL_RDY_CNT; retry++) {
		regAdd[0] = READ_ONE_EVENT;
		error = fts_read_reg(info, regAdd, 1, data, FTS_EVENT_SIZE);
		if (error) {
			tp_log("%s : I2c READ ERROR : Cannot read device info\n", __func__);
			return -ENODEV;
		}

		if (data[0] == EVENTID_CONTROLLER_READY)
			break;
		else {
			msleep(10);
			if (retry == CNRL_RDY_CNT) {
				tp_log("%s : TIMEOUT ,Cannot read controller ready event after system reset\n", __func__);
				return -ENODEV;
			}
		}
	}
	return 0;
}
static int fts_interrupt_install(struct fts_ts_info *info)
{
	int i, error = 0;
	const struct fts_i2c_platform_data *bdata = info->bdata;

	info->event_dispatch_table = kzalloc(sizeof(event_dispatch_handler_t) * EVENTID_LAST, GFP_KERNEL);

	if (!info->event_dispatch_table) {
		dev_err(info->dev, "OOM allocating event dispatch table\n");
		return -ENOMEM;
	}

	for (i = 0; i < EVENTID_LAST; i++)
		info->event_dispatch_table[i] = fts_nop_event_handler;

	install_handler(info, ENTER_POINTER, enter_pointer);

	install_handler(info, LEAVE_POINTER, leave_pointer);
	install_handler(info, MOTION_POINTER, motion_pointer);

	install_handler(info, BUTTON_STATUS, button_status);

	install_handler(info, ERROR, error);
	install_handler(info, CONTROLLER_READY, controller_ready);
	install_handler(info, STATUS, status);



	install_handler(info, PEN_ENTER, pen_enter);
	install_handler(info, PEN_LEAVE, pen_leave);
	install_handler(info, PEN_MOTION, pen_motion);

	/* disable interrupts in any case */
	fts_interrupt_set(info, INT_DISABLE);

#ifdef FTS_USE_POLLING_MODE
	dev_dbg(info->dev, "Polling Mode\n");
	hrtimer_init(&info->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	info->timer.function = fts_timer_func;
	hrtimer_start(&info->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
#else
	dev_dbg(info->dev, "Interrupt Mode\n");
	error = request_threaded_irq(info->client->irq, NULL, fts_interrupt_handler,
			bdata->irq_flags, info->client->name, info);
	if (error) {
		dev_err(info->dev, "Request irq failed\n");
		kfree(info->event_dispatch_table);
	} else
		fts_interrupt_set(info, INT_ENABLE);
#endif

	return error;
}


static void fts_interrupt_uninstall(struct fts_ts_info *info)
{
	fts_interrupt_set(info, INT_DISABLE);

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

static int fts_init(struct fts_ts_info *info)
{
	int error;
	static int retry_count;

retry:
	error = fts_systemreset(info);
	if (error) {
		dev_err(info->dev,
			"Cannot reset the device\n");
		if (retry_count < 6) {
			retry_count++;
			msleep(100);
			goto retry;
		}

		return -ENODEV;
	}
	fts_wait_controller_ready(info);
	/* check for chip id */
	error = fts_get_fw_version(info);
	if (error) {
		dev_err(info->dev,
			"Cannot initiliaze, wrong device id\n");
		return -ENODEV;
	}

	error = fts_interrupt_install(info);

	if (error)
		dev_err(info->dev, "Init (1) error (#errors = %d)\n", error);

	return error ? -ENODEV : 0;
}

static int fts_chip_powercycle(struct fts_ts_info *info)
{
		int error;
		int i;

		if (info->bus_reg) {
			error = regulator_disable(info->bus_reg);
			if (error < 0)
				dev_err(info->dev, "%s: Failed to enable bus pull-up regulator\n", __func__);
		}

		if (info->pwr_reg) {
			error = regulator_disable(info->pwr_reg);
			if (error < 0)
				dev_err(info->dev, "%s: Failed to enable power regulator\n", __func__);
		}
		msleep(300);

		if (info->pwr_reg)
			error = regulator_enable(info->pwr_reg);

		if (info->bus_reg)
			error = regulator_enable(info->bus_reg);
		msleep(300);

		gpio_set_value(info->bdata->reset_gpio, 0);
		msleep(10);
		gpio_set_value(info->bdata->reset_gpio, 1);
		msleep(500);


		for (i = 0; i < TOUCH_ID_MAX; i++) {
			input_mt_slot(info->input_dev, i);
			input_mt_report_slot_state(info->input_dev,
					(i < FINGER_MAX) ? MT_TOOL_FINGER : MT_TOOL_PEN, 0);
		}

		input_sync(info->input_dev);

		return error;
}


static int fts_get_init_status(struct fts_ts_info *info)
{
	unsigned char data[FTS_EVENT_SIZE];
	unsigned char regAdd[4] = {0xB2, 0x07, 0x29, 0x04};
	unsigned char buff_read[17];
	unsigned char regAdd1 = 0;
	unsigned char event_id = 0;
	unsigned char ms_tune_version      = 0;
	unsigned char chip_ms_tune_version = 0;
	unsigned char ss_tune_version = 0;
	unsigned char chip_ss_tune_version = 0;
	unsigned char error ;
	unsigned char retry ;
	int address_offset = 0, start_tx_offset = 0;
	fts_interrupt_set(info, INT_DISABLE);
	fts_command(info, FLUSHBUFFER);

	fts_write_reg(info, regAdd, sizeof(regAdd));
	for (retry = 0; retry <= 40; retry++) {
		regAdd1 = READ_ONE_EVENT;
		error = fts_read_reg(info, &regAdd1, sizeof(regAdd), data, FTS_EVENT_SIZE);
		if (error) {
			dev_err(info->dev, "Cannot read device info\n");
			return -EPERM;
		}

		event_id = data[0];
		if (event_id == 0x12) {
			ms_tune_version = data[3];
			break;
		} else
			msleep(10);
	}

	/* Request Compensation Data*/
	regAdd[0] = 0xB8;
	regAdd[1] = 0x02;
	regAdd[2] = 0x00;
	fts_write_reg(info, regAdd, 3);
	msleep(100);
	/* Read completion event*/
	for (retry = 0; retry <= READ_CNT; retry++)
	{
		regAdd[0] = READ_ONE_EVENT;
		error = fts_read_reg(info, regAdd, 1, data, FTS_EVENT_SIZE);
		if (error) {
			tp_log("fts_read_mutual_tune_show : I2c READ ERROR : Cannot read device info\n");
			return -ENODEV;
		}
		printk("FTS fts status event: %02X %02X %02X %02X %02X %02X %02X %02X\n",
				data[0], data[1], data[2], data[3],
				data[4], data[5], data[6], data[7]);

		if ((data[0] == EVENTID_COMP_DATA_READ) && (data[1] == 0x02))
			break;
		else {
			msleep(100);
			if (retry == READ_CNT)
				tp_log("fts_read_mutual_tune_show : TIMEOUT ,Cannot read completion event for MS Touch compensation \n");
		}
	}

	regAdd[0] = 0xD0;
	regAdd[1] = 0x00;
	regAdd[2] = 0x50;
	fts_read_reg(info, regAdd, 3, &buff_read[0], 4);
	printk("FTS Read Offset Address for Compensation1: %02X %02X %02X %02X\n", buff_read[0], buff_read[1], buff_read[2], buff_read[3]);
	start_tx_offset = ((buff_read[2]<<8) | buff_read[1]);
	address_offset  = start_tx_offset + 0x10;

	regAdd[0] = 0xD0;
	regAdd[1] = (unsigned char)((start_tx_offset & 0xFF00) >> 8);
	regAdd[2] = (unsigned char)(start_tx_offset & 0xFF);
	fts_read_reg(info, regAdd, 3, &buff_read[0], 17);

	printk("FTS  Read Offset Address for f_cnt and s_cnt: %02X %02X %02X %02X  %02X %02X %02X %02X\n", buff_read[0], buff_read[1], buff_read[2], buff_read[3], buff_read[4], buff_read[5], buff_read[6], buff_read[7]);

	chip_ms_tune_version = buff_read[9];
	if (chip_ms_tune_version == ms_tune_version)
		tune_version_same = 0x1;
	else {
		tune_version_same = 0x0;
		tp_log("fts MS Tune version not the same\n");
		goto  exit_init ;
	}

	regAdd[0] = 0xB2;
	regAdd[1] = 0x07;
	regAdd[2] = 0x4E;
	regAdd[3] = 0x04;
	fts_write_reg(info, regAdd, sizeof(regAdd));
	for (retry = 0; retry <= 40; retry++) {
		regAdd1 = READ_ONE_EVENT;
		error = fts_read_reg(info, &regAdd1, sizeof(regAdd), data, FTS_EVENT_SIZE);
		if (error) {
			dev_err(info->dev, "Cannot read device info\n");
			return -EPERM;
		}

		event_id = data[0];
		if (event_id == 0x12) {
			ss_tune_version = data[3];
			break;
		} else
			msleep(10);
	}

	/* Request Compensation Data*/
	regAdd[0] = 0xB8;
	regAdd[1] = 0x20;
	regAdd[2] = 0x00;
	fts_write_reg(info, regAdd, 3);

	for (retry = 0; retry <= READ_CNT; retry++) {
		regAdd[0] = READ_ONE_EVENT;
		error = fts_read_reg(info, regAdd, 1, data, FTS_EVENT_SIZE);
		if (error) {
			tp_log("%s : I2c READ ERROR : Cannot read device info\n", __func__);
			return -ENODEV;
		}
		printk("FTS fts status event: %02X %02X %02X %02X %02X %02X %02X %02X\n",
				data[0], data[1], data[2], data[3],
				data[4], data[5], data[6], data[7]);

		if ((data[0] == EVENTID_COMP_DATA_READ) && (data[1] == 0x20))
			break;
		else {
			msleep(10);
			if (retry == READ_CNT)
				tp_log("%s : TIMEOUT ,Cannot read completion event for SS Touch compensation \n", __func__);
		}
	}

	/* Read Offset Address for Compensation*/
	regAdd[0] = 0xD0;
	regAdd[1] = 0x00;
	regAdd[2] = 0x50;
	fts_read_reg(info, regAdd, 3, &buff_read[0], 4);
	start_tx_offset = ((buff_read[2]<<8) | buff_read[1]);
	address_offset  = start_tx_offset + 0x10;

	regAdd[0] = 0xD0;
	regAdd[1] = (unsigned char)((start_tx_offset & 0xFF00) >> 8);
	regAdd[2] = (unsigned char)(start_tx_offset & 0xFF);
	fts_read_reg(info, regAdd, 3, &buff_read[0], 17);

	printk("FTS  Read Offset Address for f_cnt and s_cnt: %02X %02X %02X %02X  %02X %02X %02X %02X\n", buff_read[0], buff_read[1], buff_read[2], buff_read[3], buff_read[4], buff_read[5], buff_read[6], buff_read[7]);

	chip_ss_tune_version = buff_read[9];

	if (chip_ss_tune_version == ss_tune_version)
		tune_version_same = 0x1;
	else {
		tune_version_same = 0x0;
		tp_log("fts SS Tune version not the same\n");
	}
exit_init:
	fts_interrupt_set(info, INT_ENABLE);

	if (tune_version_same == 0) {
		tp_log("fts initialization status error\n");
		return -EPERM;
	} else {
		tp_log("fts initialization status OK\n");
		return 0;
	}
}

static int fts_init_flash_reload(struct fts_ts_info *info)
{
	unsigned char data[FTS_EVENT_SIZE];
	int retry, error = 0;
	unsigned char event_id = 0;
	unsigned char tune_flag = 0;
	char init_check_error = 2;
	unsigned char regAdd = 0;

	fts_interrupt_set(info, INT_DISABLE);
	fts_command(info, FLUSHBUFFER);
	fts_command(info, INIT_CMD);


	for (retry = 0; retry <= READ_CNT_INIT; retry++) {
		regAdd = READ_ONE_EVENT;
		error = fts_read_reg(info, &regAdd, sizeof(regAdd), data, FTS_EVENT_SIZE);
		if (error) {
			dev_err(info->dev, "Cannot read device info\n");
			return -ENODEV;
		}

		event_id = data[0];
		tune_flag = data[1];

		if ((event_id == 0x16) && (tune_flag == 0x07)) {
			if (data[2] == 0x00) {
				init_check_error = 0;
				printk("fts initialization passed \n");
			} else {
				init_check_error = 1;
				printk("fts initialization failed \n");
			}

			break;

		} else if (retry == READ_CNT_INIT)
			init_check_error = 2;
		else
			msleep(50);
	}


	if (init_check_error != 0) {
		if (init_check_error == 2)
			printk("fts mutual initialization timeout \n");
		return -EPERM;
	}
	error += fts_command(info, SENSEON);
#ifdef PHONE_KEY
	error += fts_command(info, KEYON);
#endif
	error += fts_command(info, FORCECALIBRATION);
	error += fts_command(info, FLUSHBUFFER);
	fts_interrupt_set(info, INT_ENABLE);

	if (error != 0) {
		dev_err(info->dev, "Init (2) error (#errors = %d)\n", error);
		return -ENODEV ;
	} else
		return 0;
}

static int fts_init_hw(struct fts_ts_info *info)
{
	int error = 0;

	error += fts_command(info, SENSEON);
#ifdef PHONE_KEY
	error += fts_command(info, KEYON);
#endif
	error += fts_command(info, FORCECALIBRATION);
	error += fts_command(info, FLUSHBUFFER);
	fts_interrupt_set(info, INT_ENABLE);

	if (error)
		dev_err(info->dev, "Init (2) error (#errors = %d)\n", error);

	return error ? -ENODEV : 0;
}

static void fts_release_fingers(struct fts_ts_info *info)
{
	int i;

	for (i = 0; i < TOUCH_ID_MAX; i++) {
		input_mt_slot(info->input_dev, i);
		input_mt_report_slot_state(info->input_dev, (i < FINGER_MAX) ? MT_TOOL_FINGER : MT_TOOL_PEN, 0);
	}

	input_sync(info->input_dev);
}

static void fts_ts_suspend(struct fts_ts_info *info)
{
	if (info->sensor_sleep)
		return ;

	disable_irq_nosync(info->client->irq);
	/* Release all buttons */
	info->buttons = 0;
	info->resume_bit = 0;

	fts_release_fingers(info);

	if (info->enable_gesture_mode)
		fts_set_wakeup_mode(info, true);
	else
		fts_command(info, SENSEOFF);

	info->sensor_sleep = true;
}

static void fts_ts_resume(struct fts_ts_info *info)
{
	if (!info->sensor_sleep)
		return ;

	fts_systemreset(info);
	fts_wait_controller_ready(info);

	fts_command(info, SENSEON);
	msleep(10);
	fts_interrupt_set(info, INT_ENABLE);

	info->resume_bit = 1;
	info->sensor_sleep = false;

	enable_irq(info->client->irq);
}

#ifdef CONFIG_FB
static int fts_fb_state_chg_callback(struct notifier_block *nb, unsigned long val, void *data)
{
	struct fts_ts_info *info = container_of(nb, struct fts_ts_info, fb_notifier);
	struct fb_event *evdata = data;
	unsigned int blank;

	if (evdata && evdata->data && val == FB_EVENT_BLANK && info) {
		blank = *(int *)(evdata->data);

		switch (blank) {
		case FB_BLANK_POWERDOWN:
			fts_ts_suspend(info);
			break;
		case FB_BLANK_UNBLANK:
			fts_ts_resume(info);
			break;
		default:
			break;
		}
	}

	return NOTIFY_OK;
}
#endif

static void fts_set_wakeup_mode(struct fts_ts_info *info, bool enable)
{
	u8 reg[10] = {0xc3, 0x01, 0x02, 0x00, 0x00, 0x00};

	if (enable) {
		fts_write_reg(info, reg, sizeof(reg));
		msleep(100);
		fts_command(info, ENTER_GESTURE_MODE);
	} else {
		fts_command(info, SENSEOFF);
	}
}

static void fts_wakeup_mode_switch(struct fts_ts_info *info, u8 val)
{
	if (info->sensor_sleep) {
		if (info->enable_gesture_mode == 0 && val != 0) {
			info->enable_gesture_mode = (u8)val;
			fts_set_wakeup_mode(info, true);
		} else if (info->enable_gesture_mode != 0 && val == 0) {
			info->enable_gesture_mode = (u8)val;
			fts_set_wakeup_mode(info, false);
		}
	} else
		info->enable_gesture_mode = (u8)val;
}

static void fts_switch_mode_work(struct work_struct *work)
{
	struct fts_mode_switch *ms = container_of(work, struct fts_mode_switch, switch_mode_work);
	struct fts_ts_info *info = ms->data;
	u8 value = ms->mode;

	if (value == FTS_INPUT_EVENT_SENSITIVE_MODE_ON ||
				value == FTS_INPUT_EVENT_SENSITIVE_MODE_OFF)
		dev_err(info->dev, "glove mode\n");
	else if (value == FTS_INPUT_EVENT_STYLUS_MODE_ON ||
				value == FTS_INPUT_EVENT_STYLUS_MODE_OFF)
		dev_err(info->dev, "stylus mode\n");
	else if (value == FTS_INPUT_EVENT_WAKUP_MODE_ON ||
				value == FTS_INPUT_EVENT_WAKUP_MODE_OFF) {
		dev_err(info->dev, "wakeup mode\n");
		fts_wakeup_mode_switch(info, value - FTS_INPUT_EVENT_WAKUP_MODE_OFF);
	}

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
		if (value >= FTS_INPUT_EVENT_START && value <= FTS_INPUT_EVENT_END) {
			ms = (struct fts_mode_switch *)kmalloc(sizeof(struct fts_mode_switch), GFP_ATOMIC);
			if (ms != NULL) {
				ms->data = info;
				ms->mode = (u8)value;
				INIT_WORK(&ms->switch_mode_work, fts_switch_mode_work);
				schedule_work(&ms->switch_mode_work);
			} else {
				dev_err(info->dev, "Failed to allocating memory for fts_mode_swotch\n");
				return -ENOMEM;
			}
		}
	}

	return 0;
}

static int fts_set_input_dev(struct fts_ts_info *info)
{
	int retval = 0;

	info->input_dev = input_allocate_device();
	if (!info->input_dev) {
		dev_err(info->dev, "Input allocate device failed\n");
		return -ENOMEM;
	}

	info->input_dev->dev.parent = info->dev;
	info->input_dev->name = FTS_TS_DRV_NAME;
	info->input_dev->phys = FTS_TS_PHYS_NAME;
	info->input_dev->id.bustype = BUS_I2C;
	info->input_dev->id.vendor = 0x0001;
	info->input_dev->id.product = 0x0002;
	info->input_dev->id.version = 0x0100;
	info->input_dev->event = fts_input_event;

	__set_bit(EV_SYN, info->input_dev->evbit);
	__set_bit(EV_KEY, info->input_dev->evbit);
	__set_bit(EV_ABS, info->input_dev->evbit);
	__set_bit(BTN_TOUCH, info->input_dev->keybit);
	__set_bit(BTN_TOOL_FINGER, info->input_dev->keybit);

	input_mt_init_slots(info->input_dev, TOUCH_ID_MAX, INPUT_MT_DIRECT);

	input_set_abs_params(info->input_dev, ABS_MT_TRACKING_ID,
					 0, FINGER_MAX, 0, 0);
	input_set_abs_params(info->input_dev, ABS_MT_POSITION_X,
					 X_AXIS_MIN, X_AXIS_MAX, 0, 0);
	input_set_abs_params(info->input_dev, ABS_MT_POSITION_Y,
					 Y_AXIS_MIN, Y_AXIS_MAX, 0, 0);
	input_set_abs_params(info->input_dev, ABS_MT_TOUCH_MAJOR,
					 AREA_MIN, AREA_MAX, 0, 0);
	input_set_abs_params(info->input_dev, ABS_MT_TOUCH_MINOR,
					AREA_MIN, AREA_MAX, 0, 0);

	input_set_capability(info->input_dev, EV_KEY, KEY_WAKEUP);

	input_set_drvdata(info->input_dev, info);

	/* register the multi-touch input device */
	retval = input_register_device(info->input_dev);
	if (retval) {
		dev_err(info->dev, "Input register device failed %d\n", retval);
		goto err_register_device;
	}

	return 0;

err_register_device:
	input_free_device(info->input_dev);
	info->input_dev = NULL;

	return retval;
}

static int fts_get_regulator(struct fts_ts_info *info, bool get)
{
	int retval = 0;
	const struct fts_i2c_platform_data *bdata = info->bdata;

	if (!get)
		goto regulator_put;

	if ((bdata->pwr_reg_name != NULL) && (*bdata->pwr_reg_name != 0)) {
		info->pwr_reg = regulator_get(info->dev,
				bdata->pwr_reg_name);
		if (IS_ERR(info->pwr_reg)) {
			dev_err(info->dev,
					"%s: Failed to get power regulator\n",
					__func__);
			retval = PTR_ERR(info->pwr_reg);
			goto regulator_put;
		}
	}

	if ((bdata->bus_reg_name != NULL) && (*bdata->bus_reg_name != 0)) {
		info->bus_reg = regulator_get(info->dev,
				bdata->bus_reg_name);
		if (IS_ERR(info->bus_reg)) {
			dev_err(info->dev,
					"%s: Failed to get bus pullup regulator\n",
					__func__);
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

static int fts_enable_regulator(struct fts_ts_info *info, bool enable)
{
	int retval;

	if (!enable) {
		retval = 0;
		goto disable_pwr_reg;
	}

	if (info->bus_reg) {
		retval = regulator_enable(info->bus_reg);
		if (retval < 0) {
			dev_err(info->dev,
					"%s: Failed to enable bus pullup regulator\n",
					__func__);
			goto exit;
		}
	}

	if (info->pwr_reg) {
		retval = regulator_enable(info->pwr_reg);
		if (retval < 0) {
			dev_err(info->dev,
					"%s: Failed to enable power regulator\n",
					__func__);
			goto disable_bus_reg;
		}
	}

	return 0;

disable_pwr_reg:
	if (info->pwr_reg)
		regulator_disable(info->pwr_reg);

disable_bus_reg:
	if (info->bus_reg)
		regulator_disable(info->bus_reg);

exit:
	return retval;
}
static int fts_gpio_setup(int gpio, bool config, int dir, int state, const char *label)
{
	int retval = 0;

	if (config) {
		retval = gpio_request(gpio, label);
		if (retval) {
			pr_err("%s: Failed to get gpio %d (code: %d)",
							__func__, gpio, retval);
			return retval;
		}

		if (dir == 0)
			retval = gpio_direction_input(gpio);
		else
			retval = gpio_direction_output(gpio, state);
		if (retval) {
			pr_err("%s: Failed to set gpio %d direction",
							__func__, gpio);
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
	const struct fts_i2c_platform_data *bdata = info->bdata;

	retval = fts_gpio_setup(bdata->irq_gpio, true, 0, 0, bdata->irq_gpio_name);
	if (retval < 0) {
		dev_err(info->dev,
				"%s: Failed to configure attention GPIO\n",
				__func__);
		goto err_gpio_irq;
	}

	if (bdata->reset_gpio >= 0) {
		retval = fts_gpio_setup(bdata->reset_gpio, true, 1, 0, bdata->reset_gpio_name);
		if (retval < 0) {
			dev_err(info->dev,
					"%s: Failed to configure reset GPIO\n",
					__func__);
			goto err_gpio_reset;
		}
	}

	if (bdata->reset_gpio >= 0) {
		gpio_set_value(bdata->reset_gpio, 0);
		usleep_range(10000, 11000);
		gpio_set_value(bdata->reset_gpio, 1);
		msleep(70);
	}

	return 0;

err_gpio_reset:
	fts_gpio_setup(bdata->irq_gpio, false, 0, 0, NULL);
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

#ifdef CONFIG_OF
static void dump_dt(struct device *dev, struct fts_i2c_platform_data *bdata)
{
	dev_info(dev, "START of device tree dump:\n");
	dev_info(dev, "reset_gpio = %d\n", bdata->reset_gpio);
	dev_info(dev, "irq_gpio = %d\n", bdata->irq_gpio);
	dev_info(dev, "pwr_reg_name = %s\n", bdata->pwr_reg_name);
	dev_info(dev, "bus_reg_name = %s\n", bdata->bus_reg_name);
	dev_info(dev, "irq_gpio_name = %s\n", bdata->irq_gpio_name);
	dev_info(dev, "reset_gpio_name = %s\n", bdata->reset_gpio_name);
	dev_info(dev, "fw_name = %s\n", bdata->fw_name);
	dev_info(dev, "END of device tree dump\n");
}

static int parse_dt(struct device *dev, struct fts_i2c_platform_data *bdata)
{
	int retval;
	u32 value;
	const char *name;
	struct device_node *np = dev->of_node;

	bdata->irq_gpio = of_get_named_gpio_flags(np,
			"fts,irq-gpio", 0, NULL);
	if (bdata->irq_gpio < 0)
		return -EINVAL;

	retval = of_property_read_u32(np, "fts,irq-flags", &value);
	if (retval < 0)
		return retval;
	else
		bdata->irq_flags = value;


	retval = of_property_read_string(np, "fts,pwr-reg-name", &name);
	if (retval == -EINVAL)
		bdata->pwr_reg_name = NULL;
	else if (retval < 0)
		return retval;
	else
		bdata->pwr_reg_name = name;

	retval = of_property_read_string(np, "fts,bus-reg-name", &name);
	if (retval == -EINVAL)
		bdata->bus_reg_name = NULL;
	else if (retval < 0)
		return retval;
	else
		bdata->bus_reg_name = name;

	retval = of_property_read_string(np, "fts,reset-gpio-name", &name);
	if (retval == -EINVAL)
		bdata->reset_gpio_name = NULL;
	else if (retval < 0)
		return retval;
	else
		bdata->reset_gpio_name = name;

	retval = of_property_read_string(np, "fts,irq-gpio-name", &name);
	if (retval == -EINVAL)
		bdata->irq_gpio_name = NULL;
	else if (retval < 0)
		return retval;
	else
		bdata->irq_gpio_name = name;

	bdata->reset_gpio = of_get_named_gpio_flags(np,
			"fts,reset-gpio", 0, NULL);
	if (bdata->reset_gpio < 0)
		bdata->reset_gpio = -1;

	retval = of_property_read_string(np, "fts,fw-name", &name);
	if (retval == -EINVAL)
		bdata->fw_name = NULL;
	else if (retval < 0)
		return retval;
	else
		bdata->fw_name = name;

	dump_dt(dev, bdata);

	return 0;
}
#else
static int parse_dt(struct device *dev, struct fts_i2c_platform_data *bdata)
{
	return -ENODEV;
}
#endif

static int fts_probe(struct i2c_client *client, const struct i2c_device_id *idp)
{
	struct fts_ts_info *info = NULL;
	int error = 0;

	dev_info(&client->dev, "driver ver. 12%s\n", FTS_TS_DRV_VERSION);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "Unsupported I2C functionality\n");
		error = -EIO;
		goto err_exit;
	}

	info = kzalloc(sizeof(struct fts_ts_info), GFP_KERNEL);
	if (!info) {
		dev_err(&client->dev, "Out of memory\n");
		error = -ENOMEM;
		goto err_exit;
	}

	info->client = client;
	i2c_set_clientdata(client, info);
	info->dev = &info->client->dev;

#ifdef CONFIG_OF
	if (client->dev.of_node) {
		info->bdata = devm_kzalloc(&client->dev, sizeof(struct fts_i2c_platform_data), GFP_KERNEL);
		if (!info->bdata) {
			dev_err(&client->dev,
					"%s: Failed to allocate memory for board data\n",
					__func__);
			goto err_free_info;
		}

		parse_dt(&client->dev, info->bdata);
	}
#else
	info->bdata = client->dev.platform_data;
#endif

	error = fts_get_regulator(info, true);
	if (error < 0) {
		dev_err(&client->dev,
				"%s: Failed to get regulators\n",
				__func__);
		goto err_free_info;
	}

	error = fts_enable_regulator(info, true);
	if (error < 0) {
		dev_err(&client->dev,
				"%s: Failed to enable regulators\n",
				__func__);
		goto err_free_regulator;
	}

	error = fts_set_gpio(info);
	if (error < 0) {
		dev_err(&client->dev,
				"%s: Failed to set up GPIO's\n",
				__func__);
		goto err_disable_regulator;
	}

	error = fts_pinctrl_init(info);
	if (!error && info->ts_pinctrl) {
		error = pinctrl_select_state(info->ts_pinctrl,
				info->pinctrl_state_active);
		if (error < 0) {
			dev_err(&client->dev,
					"%s: Failed to select %s pinstate %d\n",
					__func__, PINCTRL_STATE_ACTIVE, error);
			goto err_free_gpio;
		}
	} else {
		dev_err(&client->dev,
				"%s: Failed to init pinctrl\n",
				__func__);
		goto err_free_gpio;
	}


	info->client->irq = gpio_to_irq(info->bdata->irq_gpio);

	INIT_DELAYED_WORK(&info->fwu_work, fts_fw_update_auto);
	wake_lock_init(&info->wakelock, WAKE_LOCK_SUSPEND, "fts_tp");

	info->event_wq = create_singlethread_workqueue("fts-event-queue");
	if (!info->event_wq) {
		dev_err(&client->dev, "Cannot create work thread\n");
		error = -ENOMEM;
		goto err_free_pinctrl;
	}

	INIT_WORK(&info->work, fts_event_handler);

	info->dev = &info->client->dev;

	error = fts_set_input_dev(info);
	if (error) {
		dev_err(&client->dev, "Set input device failed\n");
		goto err_destroy_event_wq;
	}

	/* track slots */
	info->touch_id = 0;

	/* track buttons */
	info->buttons = 0;

	init_completion(&info->cmd_done);

	/* init hardware device */
	error = fts_init(info);
	if (error) {
		dev_err(info->dev, "Cannot initialize the device\n");
		error = -ENODEV;
		goto err_free_input_dev;
	}

	error = fts_init_hw(info);
	if (error) {
		dev_err(info->dev, "Cannot initialize the hardware device\n");
		error = -ENODEV;
		goto err_uninstall_interrupt;
	}

	info->resume_bit = 1;
	info->gesture_disall = 1;
	info->gesture_value  = 0;

	mutex_init(&info->fts_mode_mutex);

#ifdef CONFIG_FB
	info->fb_notifier.notifier_call = fts_fb_state_chg_callback;
	error = fb_register_client(&info->fb_notifier);
	if (error) {
		dev_err(&client->dev,
				"%s: Failed to register fb notifier.\n",
				__func__);
		goto err_uninstall_interrupt;
	}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	info->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	info->early_suspend.suspend = fts_early_suspend;
	info->early_suspend.resume = fts_late_resume;
	register_early_suspend(&info->early_suspend);
#endif

#ifdef CONFIG_EXYNOS_TOUCH_DAEMON
#ifdef CONFIG_TOUCHSCREEN_FTS
	exynos_touch_daemon_data.touchdata = info;
#endif
#endif

	/* sysfs stuff */
	info->attrs.attrs = fts_attr_group;
	error = sysfs_create_group(&client->dev.kobj, &info->attrs);
	if (error) {
		dev_err(info->dev, "Cannot create sysfs structure\n");
		goto err_free_notifier;
	}

	error = sysfs_create_group(&info->dev->kobj, &i2c_cmd_attr_group);
	if (error) {
		dev_err(info->dev, "FTS Failed to create sysfs group\n");
		goto err_remove_sysfs_group;
	}

	device_init_wakeup(&client->dev, 1);

	schedule_delayed_work(&info->fwu_work, msecs_to_jiffies(EXP_FN_WORK_DELAY_MS));

	return 0;

err_remove_sysfs_group:
	sysfs_remove_group(&client->dev.kobj, &info->attrs);

err_free_notifier:
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&info->early_suspend);
#endif

#ifdef CONFIG_FB
	fb_unregister_client(&info->fb_notifier);
#endif

err_uninstall_interrupt:
	fts_interrupt_uninstall(info);

err_free_input_dev:
	input_unregister_device(info->input_dev);
	info->input_dev = NULL;

err_destroy_event_wq:
	wake_lock_destroy(&info->wakelock);

	destroy_workqueue(info->event_wq);

err_free_pinctrl:
	if (info->ts_pinctrl)
		devm_pinctrl_put(info->ts_pinctrl);

err_free_gpio:
	fts_gpio_setup(info->bdata->irq_gpio, false, 0, 0, NULL);
	if (info->bdata->reset_gpio >= 0)
		fts_gpio_setup(info->bdata->reset_gpio, false, 0, 0, NULL);

err_disable_regulator:
	fts_enable_regulator(info, false);

err_free_regulator:
	fts_get_regulator(info, false);

err_free_info:
	kfree(info);

err_exit:
	dev_err(&client->dev, "Probe failed.\n");

	return error;
}

static int fts_remove(struct i2c_client *client)
{
	struct fts_ts_info *info = i2c_get_clientdata(client);

	sysfs_remove_group(&client->dev.kobj, &info->attrs);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&info->early_suspend);
#endif

#ifdef CONFIG_FB
	fb_unregister_client(&info->fb_notifier);
#endif

	fts_interrupt_uninstall(info);

	fts_command(info, FLUSHBUFFER);

	destroy_workqueue(info->event_wq);

	sysfs_remove_group(&info->dev->kobj, &i2c_cmd_attr_group);

	input_unregister_device(info->input_dev);

	kfree(info);

	return 0;
}


static int fts_suspend(struct device *dev)
{
	struct fts_ts_info *info = dev_get_drvdata(dev);

	if (device_may_wakeup(dev) && info->enable_gesture_mode) {
		dev_info(info->dev, "suspend: in wakeup mode, enable touch irq wake\n");
		enable_irq_wake(info->client->irq);
	}

	return 0;
}

static int fts_resume(struct device *dev)
{
	struct fts_ts_info *info = dev_get_drvdata(dev);

	if (device_may_wakeup(dev) && info->enable_gesture_mode) {
		dev_info(info->dev, "resume: in wakeup mode, disable touch irq wake\n");
		disable_irq_wake(info->client->irq);
	}

	return 0;
}

static const struct dev_pm_ops fts_pm_ops = {
	.suspend = fts_suspend,
	.resume = fts_resume,
};

static struct of_device_id fts_of_match_table[] = {
	{
		.compatible = FTS_TS_OF_NAME,
	},
};

static const struct i2c_device_id fts_device_id[] = {
	{FTS_TS_DRV_NAME, 0},
};

static struct i2c_driver fts_i2c_driver = {
	.driver = {
		.name = FTS_TS_DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = fts_of_match_table,
		.pm = &fts_pm_ops,
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
MODULE_AUTHOR("Tao Jun <taojun@xiaomi.com>");
MODULE_LICENSE("GPL");

late_initcall(fts_driver_init);
module_exit(fts_driver_exit);
