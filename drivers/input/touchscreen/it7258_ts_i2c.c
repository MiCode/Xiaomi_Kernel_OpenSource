/* drivers/input/touchscreen/it7258_ts_i2c.c
 *
 * Copyright (C) 2014 ITE Tech. Inc.
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include <linux/fb.h>
#include <linux/debugfs.h>
#include <linux/input/mt.h>

#define MAX_BUFFER_SIZE			144
#define DEVICE_NAME			"IT7260"
#define SCREEN_X_RESOLUTION		320
#define SCREEN_Y_RESOLUTION		320
#define DEBUGFS_DIR_NAME		"ts_debug"

/* all commands writes go to this idx */
#define BUF_COMMAND			0x20
#define BUF_SYS_COMMAND			0x40
/*
 * "device ready?" and "wake up please" and "read touch data" reads
 * go to this idx
 */
#define BUF_QUERY			0x80
/* most command response reads go to this idx */
#define BUF_RESPONSE			0xA0
#define BUF_SYS_RESPONSE		0xC0
/* reads of "point" go through here and produce 14 bytes of data */
#define BUF_POINT_INFO			0xE0

/*
 * commands and their subcommands. when no subcommands exist, a zero
 * is send as the second byte
 */
#define CMD_IDENT_CHIP			0x00
/* VERSION_LENGTH bytes of data in response */
#define CMD_READ_VERSIONS		0x01
#define VER_FIRMWARE			0x00
#define VER_CONFIG			0x06
#define VERSION_LENGTH			10
/* subcommand is zero, next byte is power mode */
#define CMD_PWR_CTL			0x04
/* idle mode */
#define PWR_CTL_LOW_POWER_MODE		0x01
/* sleep mode */
#define PWR_CTL_SLEEP_MODE		0x02
/* command is not documented in the datasheet v1.0.0.7 */
#define CMD_UNKNOWN_7			0x07
#define CMD_FIRMWARE_REINIT_C		0x0C
/* needs to be followed by 4 bytes of zeroes */
#define CMD_CALIBRATE			0x13
#define CMD_FIRMWARE_UPGRADE		0x60
#define FIRMWARE_MODE_ENTER		0x00
#define FIRMWARE_MODE_EXIT		0x80
/* address for FW read/write */
#define CMD_SET_START_OFFSET		0x61
/* subcommand is number of bytes to write */
#define CMD_FW_WRITE			0x62
/* subcommand is number of bytes to read */
#define CMD_FW_READ			0x63
#define CMD_FIRMWARE_REINIT_6F		0x6F

#define FW_WRITE_CHUNK_SIZE		128
#define FW_WRITE_RETRY_COUNT		4
#define CHIP_FLASH_SIZE			0x8000
#define DEVICE_READY_MAX_WAIT		500

/* result of reading with BUF_QUERY bits */
#define CMD_STATUS_BITS			0x07
#define CMD_STATUS_DONE			0x00
#define CMD_STATUS_BUSY			0x01
#define CMD_STATUS_ERROR		0x02
#define PT_INFO_BITS			0xF8
#define BT_INFO_NONE			0x00
#define PT_INFO_YES			0x80
/* no new data but finder(s) still down */
#define BT_INFO_NONE_BUT_DOWN		0x08

#define PD_FLAGS_DATA_TYPE_BITS		0xF0
/* other types (like chip-detected gestures) exist but we do not care */
#define PD_FLAGS_DATA_TYPE_TOUCH	0x00
/* a bit for each finger data that is valid (from lsb to msb) */
#define PD_FLAGS_HAVE_FINGERS		0x07
/* number of finger supported */
#define PD_FINGERS_SUPPORTED		3
#define PD_PALM_FLAG_BIT		0x01
#define FD_PRESSURE_BITS		0x0F
#define FD_PRESSURE_NONE		0x00
#define FD_PRESSURE_LIGHT		0x02

#define IT_VTG_MIN_UV		1800000
#define IT_VTG_MAX_UV		1800000
#define IT_I2C_VTG_MIN_UV	2600000
#define IT_I2C_VTG_MAX_UV	3300000

struct FingerData {
	uint8_t xLo;
	uint8_t hi;
	uint8_t yLo;
	uint8_t pressure;
}  __packed;

struct PointData {
	uint8_t flags;
	uint8_t palm;
	struct FingerData fd[3];
}  __packed;

struct IT7260_ts_platform_data {
	u32 irqflags;
	u32 irq_gpio;
	u32 irq_gpio_flags;
	u32 reset_gpio;
	u32 reset_gpio_flags;
	bool wakeup;
	bool palm_detect_en;
	u16 palm_detect_keycode;
};

struct IT7260_ts_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	const struct IT7260_ts_platform_data *pdata;
	struct regulator *vdd;
	struct regulator *avdd;
	bool device_needs_wakeup;
	bool suspended;
	struct work_struct work_pm_relax;
	bool fw_upgrade_result;
	bool calibration_success;
	bool had_finger_down;
#ifdef CONFIG_FB
	struct notifier_block fb_notif;
#endif
	struct dentry *dir;
};

/* Function declarations */
static int fb_notifier_callback(struct notifier_block *self,
			unsigned long event, void *data);
static int IT7260_ts_resume(struct device *dev);
static int IT7260_ts_suspend(struct device *dev);

static struct IT7260_ts_data *gl_ts;

static int IT7260_debug_suspend_set(void *_data, u64 val)
{
	if (val)
		IT7260_ts_suspend(&gl_ts->client->dev);
	else
		IT7260_ts_resume(&gl_ts->client->dev);

	return 0;
}

static int IT7260_debug_suspend_get(void *_data, u64 *val)
{
	*val = gl_ts->suspended;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_suspend_fops, IT7260_debug_suspend_get,
				IT7260_debug_suspend_set, "%lld\n");

/* internal use func - does not make sure chip is ready before read */
static bool IT7260_i2cReadNoReadyCheck(uint8_t buf_index, uint8_t *buffer,
							uint16_t buf_len)
{
	struct i2c_msg msgs[2] = {
		{
			.addr = gl_ts->client->addr,
			.flags = I2C_M_NOSTART,
			.len = 1,
			.buf = &buf_index
		},
		{
			.addr = gl_ts->client->addr,
			.flags = I2C_M_RD,
			.len = buf_len,
			.buf = buffer
		}
	};

	memset(buffer, 0xFF, buf_len);

	return i2c_transfer(gl_ts->client->adapter, msgs, 2);
}

static bool IT7260_i2cWriteNoReadyCheck(uint8_t buf_index,
			const uint8_t *buffer, uint16_t buf_len)
{
	uint8_t txbuf[257];
	struct i2c_msg msg = {
		.addr = gl_ts->client->addr,
		.flags = 0,
		.len = buf_len + 1,
		.buf = txbuf
	};

	/* just to be careful */
	if (buf_len > sizeof(txbuf) - 1) {
		dev_err(&gl_ts->client->dev, "buf length is out of limit\n");
		return false;
	}

	txbuf[0] = buf_index;
	memcpy(txbuf + 1, buffer, buf_len);

	return i2c_transfer(gl_ts->client->adapter, &msg, 1);
}

/*
 * Device is apparently always ready for i2c but not for actual
 * register reads/writes. This function ascertains it is ready
 * for that too. the results of this call often were ignored.
 */
static bool IT7260_waitDeviceReady(bool forever, bool slowly)
{
	uint8_t query;
	uint32_t count = DEVICE_READY_MAX_WAIT;

	do {
		if (!IT7260_i2cReadNoReadyCheck(BUF_QUERY, &query,
						sizeof(query)))
			query = CMD_STATUS_BUSY;

		if (slowly)
			mdelay(1000);
		if (!forever)
			count--;

	} while ((query & CMD_STATUS_BUSY) && count);

	return !query;
}

static bool IT7260_i2cRead(uint8_t buf_index, uint8_t *buffer,
						uint16_t buf_len)
{
	IT7260_waitDeviceReady(false, false);
	return IT7260_i2cReadNoReadyCheck(buf_index, buffer, buf_len);
}

static bool IT7260_i2cWrite(uint8_t buf_index, const uint8_t *buffer,
							uint16_t buf_len)
{
	IT7260_waitDeviceReady(false, false);
	return IT7260_i2cWriteNoReadyCheck(buf_index, buffer, buf_len);
}

static bool IT7260_chipFirmwareReinitialize(uint8_t command)
{
	uint8_t cmd[] = {command};
	uint8_t rsp[2];

	if (!IT7260_i2cWrite(BUF_COMMAND, cmd, sizeof(cmd)))
		return false;

	if (!IT7260_i2cRead(BUF_RESPONSE, rsp, sizeof(rsp)))
		return false;

	/* a reply of two zero bytes signifies success */
	return !rsp[0] && !rsp[1];
}

static bool IT7260_chipFirmwareUpgradeModeEnterExit(bool enter)
{
	uint8_t cmd[] = {CMD_FIRMWARE_UPGRADE, 0, 'I', 'T', '7', '2',
						'6', '0', 0x55, 0xAA};
	uint8_t resp[2];

	cmd[1] = enter ? FIRMWARE_MODE_ENTER : FIRMWARE_MODE_EXIT;
	if (!IT7260_i2cWrite(BUF_COMMAND, cmd, sizeof(cmd)))
		return false;

	if (!IT7260_i2cRead(BUF_RESPONSE, resp, sizeof(resp)))
		return false;

	/* a reply of two zero bytes signifies success */
	return !resp[0] && !resp[1];
}

static bool IT7260_chipSetStartOffset(uint16_t offset)
{
	uint8_t cmd[] = {CMD_SET_START_OFFSET, 0, ((uint8_t)(offset)),
				((uint8_t)((offset) >> 8))};
	uint8_t resp[2];

	if (!IT7260_i2cWrite(BUF_COMMAND, cmd, 4))
		return false;


	if (!IT7260_i2cRead(BUF_RESPONSE, resp, sizeof(resp)))
		return false;


	/* a reply of two zero bytes signifies success */
	return !resp[0] && !resp[1];
}


/* write fw_length bytes from fw_data at chip offset wr_start_offset */
static bool IT7260_chipFlashWriteAndVerify(unsigned int fw_length,
			const uint8_t *fw_data, uint16_t wr_start_offset)
{
	uint32_t cur_data_off;

	for (cur_data_off = 0; cur_data_off < fw_length;
				cur_data_off += FW_WRITE_CHUNK_SIZE) {

		uint8_t cmd_write[2 + FW_WRITE_CHUNK_SIZE] = {CMD_FW_WRITE};
		uint8_t buf_read[FW_WRITE_CHUNK_SIZE];
		uint8_t cmd_read[2] = {CMD_FW_READ};
		unsigned i, retries;
		uint32_t cur_wr_size;

		/* figure out how much to write */
		cur_wr_size = fw_length - cur_data_off;
		if (cur_wr_size > FW_WRITE_CHUNK_SIZE)
			cur_wr_size = FW_WRITE_CHUNK_SIZE;

		/* prepare the write command */
		cmd_write[1] = cur_wr_size;
		for (i = 0; i < cur_wr_size; i++)
			cmd_write[i + 2] = fw_data[cur_data_off + i];

		/* prepare the read command */
		cmd_read[1] = cur_wr_size;

		for (retries = 0; retries < FW_WRITE_RETRY_COUNT;
							retries++) {

			/* set write offset and write the data */
			IT7260_chipSetStartOffset(
					wr_start_offset + cur_data_off);
			IT7260_i2cWrite(BUF_COMMAND, cmd_write,
					cur_wr_size + 2);

			/* set offset and read the data back */
			IT7260_chipSetStartOffset(
					wr_start_offset + cur_data_off);
			IT7260_i2cWrite(BUF_COMMAND, cmd_read,
					sizeof(cmd_read));
			IT7260_i2cRead(BUF_RESPONSE, buf_read, cur_wr_size);

			/* verify. If success break out of retry loop */
			i = 0;
			while (i < cur_wr_size &&
					buf_read[i] == cmd_write[i + 2])
				i++;
			if (i == cur_wr_size)
				break;
		}
		/* if we've failed after all the retries, tell the caller */
		if (retries == FW_WRITE_RETRY_COUNT) {
			dev_err(&gl_ts->client->dev,
				"write of data offset %u failed on try %u at byte %u/%u\n",
				cur_data_off, retries, i, cur_wr_size);
			return false;
		}
	}

	return true;
}

static bool IT7260_chipFirmwareUpload(uint32_t fw_len, const uint8_t *fw_data,
				uint32_t cfg_len, const uint8_t *cfg_data)
{
	bool success = false;

	/* enter fw upload mode */
	if (!IT7260_chipFirmwareUpgradeModeEnterExit(true))
		return false;

	/* flash the firmware if requested */
	if (fw_len && fw_data && !IT7260_chipFlashWriteAndVerify(fw_len,
					fw_data, 0)) {
		dev_err(&gl_ts->client->dev, "failed to upload touch firmware\n");
		goto out;
	}

	/* flash config data if requested */
	if (cfg_len && cfg_data && !IT7260_chipFlashWriteAndVerify(cfg_len,
					cfg_data, CHIP_FLASH_SIZE - cfg_len)) {
		dev_err(&gl_ts->client->dev, "failed to upload touch cfg data\n");
		goto out;
	}

	success = true;

out:
	return IT7260_chipFirmwareUpgradeModeEnterExit(false) &&
		IT7260_chipFirmwareReinitialize(CMD_FIRMWARE_REINIT_6F) &&
		success;
}


/*
 * both buffers should be VERSION_LENGTH in size,
 * but only a part of them is significant
 */
static bool IT7260_chipGetVersions(uint8_t *ver_fw, uint8_t *ver_cfg,
								bool log_it)
{
	/*
	 * this code to get versions is reproduced as was written, but it does
	 * not make sense. Something here *PROBABLY IS* wrong
	 */
	static const uint8_t cmd_read_fw_ver[] = {CMD_READ_VERSIONS,
								VER_FIRMWARE};
	static const uint8_t cmd_read_cfg_ver[] = {CMD_READ_VERSIONS,
								VER_CONFIG};
	bool ret = true;

	/*
	 * this structure is so that we definitely do all the calls, but still
	 * return a status in case anyone cares
	 */
	ret = IT7260_i2cWrite(BUF_COMMAND, cmd_read_fw_ver,
					sizeof(cmd_read_fw_ver)) && ret;
	ret = IT7260_i2cRead(BUF_RESPONSE, ver_fw, VERSION_LENGTH) && ret;
	ret = IT7260_i2cWrite(BUF_COMMAND, cmd_read_cfg_ver,
					sizeof(cmd_read_cfg_ver)) && ret;
	ret = IT7260_i2cRead(BUF_RESPONSE, ver_cfg, VERSION_LENGTH) && ret;

	if (log_it)
		dev_info(&gl_ts->client->dev,
			"current versions: fw@{%X,%X,%X,%X}, cfg@{%X,%X,%X,%X}\n",
			ver_fw[5], ver_fw[6], ver_fw[7], ver_fw[8],
			ver_cfg[1], ver_cfg[2], ver_cfg[3], ver_cfg[4]);

	return ret;
}

static int IT7260_ts_chipLowPowerMode(bool low)
{
	static const uint8_t cmd_sleep[] = {CMD_PWR_CTL,
					0x00, PWR_CTL_SLEEP_MODE};
	uint8_t dummy;

	if (low)
		IT7260_i2cWriteNoReadyCheck(BUF_COMMAND, cmd_sleep,
					sizeof(cmd_sleep));
	else
		IT7260_i2cReadNoReadyCheck(BUF_QUERY, &dummy, sizeof(dummy));

	return 0;
}

static ssize_t sysfsUpgradeStore(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	const struct firmware *fw, *cfg;
	uint8_t ver_fw[10], ver_cfg[10];
	unsigned fw_len = 0, cfg_len = 0;
	bool success;
	int mode = 0, ret;

	ret = request_firmware(&fw, "it7260.fw", dev);
	if (ret)
		dev_dbg(dev, "failed to get firmware for it7260 %d\n", ret);
	else
		fw_len = fw->size;

	ret = request_firmware(&cfg, "it7260.cfg", dev);
	if (ret)
		dev_dbg(dev, "failed to get config data for it7260 %d\n", ret);
	else
		cfg_len = cfg->size;

	ret = kstrtoint(buf, 10, &mode);

	IT7260_chipGetVersions(ver_fw, ver_cfg, true);

	gl_ts->fw_upgrade_result = false;
	if (fw_len && cfg_len) {
		if ((mode > 0) && ((ver_fw[5] < fw->data[8] ||
				ver_fw[6] < fw->data[9] ||
				ver_fw[7] < fw->data[10] ||
				ver_fw[8] < fw->data[11]) ||
				(ver_cfg[1] < cfg->data[cfg_len - 8] ||
				ver_cfg[2] < cfg->data[cfg_len - 7] ||
				ver_cfg[3] < cfg->data[cfg_len - 6] ||
				ver_cfg[4] < cfg->data[cfg_len - 5]))) {
			dev_info(dev, "firmware/config will be upgraded\n");
			disable_irq(gl_ts->client->irq);
			/* upgrade the fw and cfg */
			success = IT7260_chipFirmwareUpload(fw_len, fw->data,
						cfg_len, cfg->data);
			enable_irq(gl_ts->client->irq);

			gl_ts->fw_upgrade_result = success;
			dev_info(dev, "fw/cfg upload %s\n",
					success ? "success" : "failed");
		} else {
			dev_info(dev, "firmware/config upgrade not needed\n");
		}
	}

	if (fw_len)
		release_firmware(fw);

	if (cfg_len)
		release_firmware(cfg);

	return count;
}

static ssize_t sysfsUpgradeShow(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_BUFFER_SIZE, "%d", gl_ts->fw_upgrade_result);
}

static ssize_t sysfsCalibrationShow(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_BUFFER_SIZE, "%d", gl_ts->calibration_success);
}

static bool IT7260_chipSendCalibrationCmd(bool auto_tune_on)
{
	uint8_t cmd_calibrate[] = {CMD_CALIBRATE, 0,
					auto_tune_on ? 1 : 0, 0, 0};
	return IT7260_i2cWrite(BUF_COMMAND, cmd_calibrate,
					sizeof(cmd_calibrate));
}

static ssize_t sysfsCalibrationStore(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	uint8_t resp;

	if (!IT7260_chipSendCalibrationCmd(false)) {
		dev_err(dev, "failed to send calibration command\n");
	} else {
		gl_ts->calibration_success =
			IT7260_i2cRead(BUF_RESPONSE, &resp, sizeof(resp));

		/*
		 * previous logic that was here never called
		 * IT7260_chipFirmwareReinitialize() due to checking a
		 * guaranteed-not-null value against null. We now
		 * call it. Hopefully this is OK
		 */
		if (!resp)
			dev_dbg(dev, "IT7260_chipFirmwareReinitialize -> %s\n",
			IT7260_chipFirmwareReinitialize(CMD_FIRMWARE_REINIT_6F)
			? "success" : "fail");
	}

	return count;
}
`
static ssize_t sysfsPointShow(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	uint8_t point_data[sizeof(struct PointData)];
	bool readSuccess;
	ssize_t ret;

	readSuccess = IT7260_i2cReadNoReadyCheck(BUF_POINT_INFO, point_data,
							sizeof(point_data));
	ret = snprintf(buf, MAX_BUFFER_SIZE,
		"point_show read ret[%d]--point[%x][%x][%x][%x][%x][%x][%x][%x][%x][%x][%x][%x][%x][%x]=\n",
		readSuccess, point_data[0], point_data[1], point_data[2],
		point_data[3], point_data[4], point_data[5], point_data[6],
		point_data[7], point_data[8], point_data[9], point_data[10],
		point_data[11], point_data[12], point_data[13]);

	dev_dbg(dev, "%s", buf);

	return ret;
}

static ssize_t sysfsPointStore(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t sysfsVersionShow(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	uint8_t ver_fw[10], ver_cfg[10];

	IT7260_chipGetVersions(ver_fw, ver_cfg, false);
	return snprintf(buf, MAX_BUFFER_SIZE,
			"fw{%x,%x,%x,%x} # cfg{%x,%x,%x,%x}\n",
			ver_fw[5], ver_fw[6], ver_fw[7], ver_fw[8],
			ver_cfg[1], ver_cfg[2], ver_cfg[3], ver_cfg[4]);
}

static ssize_t sysfsSleepShow(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	/*
	 * The usefulness of this was questionable at best - we were at least
	 * leaking a byte of kernel data (by claiming to return a byte but not
	 * writing to buf. To fix this now we actually return the sleep status
	 */
	*buf = gl_ts->suspended ? '1' : '0';
	return 1;
}

static ssize_t sysfsSleepStore(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int go_to_sleep, ret;

	ret = kstrtoint(buf, 10, &goToSleepVal);

	/* (gl_ts->suspended == true && goToSleepVal > 0) means
	 * device is already suspended and you want it to be in sleep,
	 * (gl_ts->suspended == false && goToSleepVal == 0) means
	 * device is already active and you also want it to be active.
	 */
	if ((gl_ts->suspended && go_to_sleep > 0) ||
			(!gl_ts->suspended && go_to_sleep == 0))
		dev_err(dev, "duplicate request to %s chip\n",
			go_to_sleep ? "sleep" : "wake");
	else if (go_to_sleep) {
		disable_irq(gl_ts->client->irq);
		IT7260_ts_chipLowPowerMode(true);
		dev_dbg(dev, "touch is going to sleep...\n");
	} else {
		IT7260_ts_chipLowPowerMode(false);
		enable_irq(gl_ts->client->irq);
		dev_dbg(dev, "touch is going to wake!\n");
	}
	gl_ts->suspended = go_to_sleep;

	return count;
}

static DEVICE_ATTR(version, S_IRUGO | S_IWUSR,
			sysfsVersionShow, NULL);
static DEVICE_ATTR(sleep, S_IRUGO | S_IWUSR,
			sysfsSleepShow, sysfsSleepStore);
static DEVICE_ATTR(calibration, S_IRUGO|S_IWUSR|S_IWGRP,
			sysfsCalibrationShow, sysfsCalibrationStore);
static DEVICE_ATTR(upgrade, S_IRUGO|S_IWUSR|S_IWGRP,
			sysfsUpgradeShow, sysfsUpgradeStore);
static DEVICE_ATTR(point, S_IRUGO|S_IWUSR|S_IWGRP,
			sysfsPointShow, sysfsPointStore);

static struct attribute *it7260_attributes[] = {
	&dev_attr_version.attr,
	&dev_attr_sleep.attr,
	&dev_attr_calibration.attr,
	&dev_attr_upgrade.attr,
	&dev_attr_point.attr,
	NULL
};

static const struct attribute_group it7260_attr_group = {
	.attrs = it7260_attributes,
};

static void IT7260_chipExternalCalibration(bool autoTuneEnabled)
{
	uint8_t resp[2];

	dev_dbg(&gl_ts->client->dev, "sent calibration command -> %d\n",
			IT7260_chipSendCalibrationCmd(autoTuneEnabled));
	IT7260_waitDeviceReady(true, true);
	IT7260_i2cReadNoReadyCheck(BUF_RESPONSE, resp, sizeof(resp));
	IT7260_chipFirmwareReinitialize(CMD_FIRMWARE_REINIT_C);
}

void IT7260_sendCalibrationCmd(void)
{
	IT7260_chipExternalCalibration(false);
}
EXPORT_SYMBOL(IT7260_sendCalibrationCmd);

static void IT7260_ts_release_all(void)
{
	int finger;

	for (finger = 0; finger < PD_FINGERS_SUPPORTED; finger++) {
		input_mt_slot(gl_ts->input_dev, finger);
		input_mt_report_slot_state(gl_ts->input_dev,
				MT_TOOL_FINGER, 0);
	}

	input_report_key(gl_ts->input_dev, BTN_TOUCH, 0);
	input_sync(gl_ts->input_dev);
}

static irqreturn_t IT7260_ts_threaded_handler(int irq, void *devid)
{
	struct PointData point_data;
	struct input_dev *input_dev = gl_ts->input_dev;
	u8 dev_status, finger, touch_count = 0, finger_status;
	u8 pressure = FD_PRESSURE_NONE;
	u16 x, y;
	bool palm_detected;

	/*
	 * This code adds the touch-to-wake functionality to the ITE tech
	 * driver. When the device is in suspend, driver sends the
	 * KEY_WAKEUP event to wake the device. The pm_stay_awake() call
	 * tells the pm core to stay awake until the CPU cores up already. The
	 * schedule_work() call schedule a work that tells the pm core to relax
	 * once the CPU cores are up.
	 */
	if (gl_ts->device_needs_wakeup) {
		pm_stay_awake(&gl_ts->client->dev);
		gl_ts->device_needs_wakeup = false;
		input_report_key(input_dev, KEY_WAKEUP, 1);
		input_sync(input_dev);
		input_report_key(input_dev, KEY_WAKEUP, 0);
		input_sync(input_dev);
		schedule_work(&gl_ts->work_pm_relax);
	}

	/* verify there is point data to read & it is readable and valid */
	IT7260_i2cReadNoReadyCheck(BUF_QUERY, &dev_status, sizeof(dev_status));
	if (!((dev_status & PT_INFO_BITS) & PT_INFO_YES))
		return IRQ_HANDLED;
	if (!IT7260_i2cReadNoReadyCheck(BUF_POINT_INFO, (void *)&point_data,
						sizeof(point_data))) {
		dev_err(&gl_ts->client->dev,
			"failed to read point data buffer\n");
		return IRQ_HANDLED;
	}
	if ((point_data.flags & PD_FLAGS_DATA_TYPE_BITS) !=
					PD_FLAGS_DATA_TYPE_TOUCH) {
		dev_err(&gl_ts->client->dev,
			"dropping non-point data of type 0x%02X\n",
							point_data.flags);
		return IRQ_HANDLED;
	}

	palm_detected = point_data.palm & PD_PALM_FLAG_BIT;
	if (palm_detected && gl_ts->pdata->palm_detect_en) {
		input_report_key(input_dev,
				gl_ts->pdata->palm_detect_keycode, 1);
		input_sync(input_dev);
		input_report_key(input_dev,
				gl_ts->pdata->palm_detect_keycode, 0);
		input_sync(input_dev);
	}

	for (finger = 0; finger < PD_FINGERS_SUPPORTED; finger++) {
		finger_status = point_data.flags & (0x01 << finger);

		input_mt_slot(input_dev, finger);
		input_mt_report_slot_state(input_dev, MT_TOOL_FINGER,
					finger_status != 0);

		x = point_data.fd[finger].xLo +
			(((u16)(point_data.fd[finger].hi & 0x0F)) << 8);
		y = point_data.fd[finger].yLo +
			(((u16)(point_data.fd[finger].hi & 0xF0)) << 4);

		pressure = point_data.fd[finger].pressure & FD_PRESSURE_BITS;

		if (finger_status) {
			if (pressure >= FD_PRESSURE_LIGHT) {
				input_report_key(input_dev, BTN_TOUCH, 1);
				input_report_abs(input_dev,
							ABS_MT_POSITION_X, x);
				input_report_abs(input_dev,
							ABS_MT_POSITION_Y, y);
				touch_count++;
			}
		}
	}

	input_report_key(input_dev, BTN_TOUCH, touch_count > 0);
	input_sync(input_dev);

	return IRQ_HANDLED;
}

static void IT7260_ts_work_func(struct work_struct *work)
{
	pm_relax(&gl_ts->client->dev);
}

static bool IT7260_chipIdentify(void)
{
	static const uint8_t cmd_ident[] = {CMD_IDENT_CHIP};
	static const uint8_t expected_id[] = {0x0A, 'I', 'T', 'E', '7',
							'2', '6', '0'};
	uint8_t chip_id[10] = {0,};

	IT7260_waitDeviceReady(true, false);

	if (!IT7260_i2cWriteNoReadyCheck(BUF_COMMAND, cmd_ident,
							sizeof(cmd_ident))) {
		dev_err(&gl_ts->client->dev, "failed to write CMD_IDENT_CHIP\n");
		return false;
	}

	IT7260_waitDeviceReady(true, false);

	if (!IT7260_i2cReadNoReadyCheck(BUF_RESPONSE, chip_id,
							sizeof(chip_id))) {
		dev_err(&gl_ts->client->dev, "failed to read chip-id\n");
		return false;
	}
	dev_info(&gl_ts->client->dev,
		"IT7260_chipIdentify read id: %02X %c%c%c%c%c%c%c %c%c\n",
		chip_id[0], chip_id[1], chip_id[2], chip_id[3], chip_id[4],
		chip_id[5], chip_id[6], chip_id[7], chip_id[8], chip_id[9]);

	if (memcmp(chip_id, expected_id, sizeof(expected_id)))
		return false;

	if (chip_id[8] == '5' && chip_id[9] == '6')
		dev_info(&gl_ts->client->dev, "rev BX3 found\n");
	else if (chip_id[8] == '6' && chip_id[9] == '6')
		dev_info(&gl_ts->client->dev, "rev BX4 found\n");
	else
		dev_info(&gl_ts->client->dev, "unknown revision (0x%02X 0x%02X) found\n",
						chip_id[8], chip_id[9]);

	return true;
}

static int IT7260_ts_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	static const uint8_t cmd_start[] = {CMD_UNKNOWN_7};
	struct IT7260_ts_platform_data *pdata;
	uint8_t rsp[2];
	int ret = -1;
	u32 temp_val;
	struct dentry *temp;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "need I2C_FUNC_I2C\n");
		return -ENODEV;
	}

	gl_ts = devm_kzalloc(&client->dev, sizeof(*gl_ts), GFP_KERNEL);
	if (!gl_ts)
		return -ENOMEM;

	gl_ts->client = client;
	i2c_set_clientdata(client, gl_ts);

	if (client->dev.platform_data == NULL)
		return -ENODEV;

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata)
			return -ENOMEM;
	} else {
		pdata = client->dev.platform_data;
	}

	if (!pdata)
		return -ENOMEM;

	gl_ts->pdata = pdata;

	gl_ts->vdd = devm_regulator_get(&gl_ts->client->dev, "vdd");
	if (IS_ERR(gl_ts->vdd)) {
		dev_err(&client->dev,
				"Regulator get failed vdd\n");
		gl_ts->vdd = NULL;
	} else {
		ret = regulator_set_voltage(gl_ts->vdd,
				IT_VTG_MIN_UV, IT_VTG_MAX_UV);
		if (ret) {
			dev_err(&client->dev,
				"Regulator set_vtg failed vdd %d\n", ret);
			return ret;
		}
	}

	gl_ts->avdd = devm_regulator_get(&gl_ts->client->dev, "avdd");
	if (IS_ERR(gl_ts->avdd)) {
		dev_err(&client->dev,
				"Regulator get failed avdd\n");
		gl_ts->avdd = NULL;
	} else {
		ret = regulator_set_voltage(gl_ts->avdd, IT_I2C_VTG_MIN_UV,
							IT_I2C_VTG_MAX_UV);
		if (ret) {
			dev_err(&client->dev,
				"Regulator get failed avdd %d\n", ret);
			return ret;
		}
	}

	if (gl_ts->vdd) {
		ret = regulator_enable(gl_ts->vdd);
		if (ret) {
			dev_err(&gl_ts->client->dev,
				"Regulator vdd enable failed ret=%d\n", ret);
			return ret;
		}
	}

	if (gl_ts->avdd) {
		ret = regulator_enable(gl_ts->avdd);
		if (ret) {
			dev_err(&gl_ts->client->dev,
				"Regulator avdd enable failed ret=%d\n", ret);
			return ret;
		}
	}

	/* reset gpio info */
	pdata->reset_gpio = of_get_named_gpio_flags(client->dev.of_node,
					"ite,reset-gpio", 0,
					&pdata->reset_gpio_flags);
	if (gpio_is_valid(pdata->reset_gpio)) {
		if (gpio_request(pdata->reset_gpio, "ite_reset_gpio")) {
			dev_err(&client->dev,
				"gpio_request failed for reset GPIO\n");
			return -EINVAL;
		}
		if (gpio_direction_output(pdata->reset_gpio, 0)) {
			dev_err(&client->dev,
				"gpio_direction_output for reset GPIO\n");
			return -EINVAL;
		}
		dev_dbg(&gl_ts->client->dev, "Reset GPIO %d\n",
							pdata->reset_gpio);
	} else {
		return pdata->reset_gpio;
	}

	/* irq gpio info */
	pdata->irq_gpio = of_get_named_gpio_flags(client->dev.of_node,
				"ite,irq-gpio", 0, &pdata->irq_gpio_flags);
	if (gpio_is_valid(pdata->irq_gpio)) {
		dev_dbg(&gl_ts->client->dev, "IRQ GPIO %d, IRQ # %d\n",
				pdata->irq_gpio, gpio_to_irq(pdata->irq_gpio));
	} else {
		return pdata->irq_gpio;
	}

	pdata->wakeup = of_property_read_bool(client->dev.of_node,
						"ite,wakeup");
	pdata->palm_detect_en = of_property_read_bool(client->dev.of_node,
						"ite,palm-detect-en");
	if (pdata->palm_detect_en) {
		ret = of_property_read_u32(client->dev.of_node,
					"ite,palm-detect-keycode", &temp_val);
		if (!ret) {
			pdata->palm_detect_keycode = temp_val;
		} else {
			dev_err(&client->dev,
				"Unable to read palm-detect-keycode\n");
			return ret;
		}
	}

	if (!IT7260_chipIdentify()) {
		dev_err(&client->dev, "Failed to identify chip!!!");
		goto err_identification_fail;
	}

	gl_ts->input_dev = input_allocate_device();
	if (!gl_ts->input_dev) {
		dev_err(&client->dev, "failed to allocate input device\n");
		ret = -ENOMEM;
		goto err_input_alloc;
	}

	gl_ts->input_dev->name = DEVICE_NAME;
	gl_ts->input_dev->phys = "I2C";
	gl_ts->input_dev->id.bustype = BUS_I2C;
	gl_ts->input_dev->id.vendor = 0x0001;
	gl_ts->input_dev->id.product = 0x7260;
	set_bit(EV_SYN, gl_ts->input_dev->evbit);
	set_bit(EV_KEY, gl_ts->input_dev->evbit);
	set_bit(EV_ABS, gl_ts->input_dev->evbit);
	set_bit(INPUT_PROP_DIRECT, gl_ts->input_dev->propbit);
	set_bit(BTN_TOUCH, gl_ts->input_dev->keybit);
	input_set_abs_params(gl_ts->input_dev, ABS_MT_POSITION_X, 0,
				SCREEN_X_RESOLUTION, 0, 0);
	input_set_abs_params(gl_ts->input_dev, ABS_MT_POSITION_Y, 0,
				SCREEN_Y_RESOLUTION, 0, 0);
	input_set_drvdata(gl_ts->input_dev, gl_ts);
	input_mt_init_slots(gl_ts->input_dev, PD_FINGERS_SUPPORTED, 0);

	if (pdata->wakeup) {
		set_bit(KEY_WAKEUP, gl_ts->input_dev->keybit);
		INIT_WORK(&gl_ts->work_pm_relax, IT7260_ts_work_func);
		device_init_wakeup(&client->dev, pdata->wakeup);
	}

	if (pdata->palm_detect_en)
		set_bit(gl_ts->pdata->palm_detect_keycode,
					gl_ts->input_dev->keybit);

	if (input_register_device(gl_ts->input_dev)) {
		dev_err(&client->dev, "failed to register input device\n");
		goto err_input_register;
	}

	if (request_threaded_irq(client->irq, NULL, IT7260_ts_threaded_handler,
		IRQF_TRIGGER_LOW | IRQF_ONESHOT, client->name, gl_ts)) {
		dev_err(&client->dev, "request_irq failed\n");
		goto err_irq_reg;
	}

	if (sysfs_create_group(&(client->dev.kobj), &it7260_attr_group)) {
		dev_err(&client->dev, "failed to register sysfs #2\n");
		goto err_sysfs_grp_create;
	}

#if defined(CONFIG_FB)
	gl_ts->fb_notif.notifier_call = fb_notifier_callback;

	ret = fb_register_client(&gl_ts->fb_notif);
	if (ret)
		dev_err(&client->dev, "Unable to register fb_notifier: %d\n",
					ret);
#endif
	
	IT7260_i2cWriteNoReadyCheck(BUF_COMMAND, cmd_start, sizeof(cmd_start));
	mdelay(10);
	IT7260_i2cReadNoReadyCheck(BUF_RESPONSE, rsp, sizeof(rsp));
	mdelay(10);

	gl_ts->dir = debugfs_create_dir(DEBUGFS_DIR_NAME, NULL);
	if (gl_ts->dir == NULL || IS_ERR(gl_ts->dir)) {
		dev_err(&client->dev,
			"%s: Failed to create debugfs directory, ret = %ld\n",
			__func__, PTR_ERR(gl_ts->dir));
		ret = PTR_ERR(gl_ts->dir);
		goto err_create_debugfs_dir;
	}

	temp = debugfs_create_file("suspend", S_IRUSR | S_IWUSR, gl_ts->dir,
					gl_ts, &debug_suspend_fops);
	if (temp == NULL || IS_ERR(temp)) {
		dev_err(&client->dev,
			"%s: Failed to create suspend debugfs file, ret = %ld\n",
			__func__, PTR_ERR(temp));
		ret = PTR_ERR(temp);
		goto err_create_debugfs_file;
	}

	return 0;

err_create_debugfs_file:
	debugfs_remove_recursive(gl_ts->dir);
err_create_debugfs_dir:
#if defined(CONFIG_FB)
	if (fb_unregister_client(&gl_ts->fb_notif))
		dev_err(&client->dev, "Error occurred while unregistering fb_notifier.\n");
#endif
	sysfs_remove_group(&(client->dev.kobj), &it7260_attr_group);

err_sysfs_grp_create:
	free_irq(client->irq, gl_ts);

err_irq_reg:
	input_unregister_device(gl_ts->input_dev);

err_input_register:
	if (pdata->wakeup) {
		cancel_work_sync(&gl_ts->work_pm_relax);
		device_init_wakeup(&client->dev, false);
	}
	if (gl_ts->input_dev)
		input_free_device(gl_ts->input_dev);
	gl_ts->input_dev = NULL;

err_input_alloc:
err_identification_fail:
	if (gpio_is_valid(pdata->reset_gpio))
		gpio_free(pdata->reset_gpio);
	if (gpio_is_valid(pdata->irq_gpio))
		gpio_free(pdata->irq_gpio);

	regulator_disable(gl_ts->vdd);
	regulator_disable(gl_ts->avdd);
	regulator_put(gl_ts->vdd);
	regulator_put(gl_ts->avdd);

	return ret;
}

static int IT7260_ts_remove(struct i2c_client *client)
{
	debugfs_remove_recursive(gl_ts->dir);
#if defined(CONFIG_FB)
	if (fb_unregister_client(&gl_ts->fb_notif))
		dev_err(&client->dev, "Error occurred while unregistering fb_notifier.\n");
#endif
	sysfs_remove_group(&(client->dev.kobj), &it7260_attr_group);
	free_irq(client->irq, gl_ts);
	input_unregister_device(gl_ts->input_dev);
	if (gl_ts->input_dev)
		input_free_device(gl_ts->input_dev);
	gl_ts->input_dev = NULL;
	if (gl_ts->pdata->wakeup) {
		cancel_work_sync(&gl_ts->work_pm_relax);
		device_init_wakeup(&client->dev, false);
	}
	if (gpio_is_valid(gl_ts->pdata->reset_gpio))
		gpio_free(gl_ts->pdata->reset_gpio);
	if (gpio_is_valid(gl_ts->pdata->irq_gpio))
		gpio_free(gl_ts->pdata->irq_gpio);
	regulator_disable(gl_ts->vdd);
	regulator_disable(gl_ts->avdd);
	regulator_put(gl_ts->vdd);
	regulator_put(gl_ts->avdd);
	return 0;
}

#if defined(CONFIG_FB)
static int fb_notifier_callback(struct notifier_block *self,
			unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;

	if (evdata && evdata->data && gl_ts && gl_ts->client) {
		if (event == FB_EVENT_BLANK) {
			blank = evdata->data;
			if (*blank == FB_BLANK_UNBLANK)
				IT7260_ts_resume(&(gl_ts->client->dev));
			else if (*blank == FB_BLANK_POWERDOWN ||
					*blank == FB_BLANK_VSYNC_SUSPEND)
				IT7260_ts_suspend(&(gl_ts->client->dev));
		}
	}

	return 0;
}
#endif

#ifdef CONFIG_PM
static int IT7260_ts_resume(struct device *dev)
{
	if (!gl_ts->suspended) {
		dev_info(dev, "Already in resume state\n");
		return 0;
	}

	if (device_may_wakeup(dev) && (gl_ts->device_needs_wakeup)) {
		gl_ts->device_needs_wakeup = false;
		disable_irq_wake(gl_ts->client->irq);
		return 0;
	}

	/* put the device in active power mode */
	IT7260_ts_chipLowPowerMode(false);

	enable_irq(gl_ts->client->irq);
	gl_ts->suspended = false;
	return 0;
}

static int IT7260_ts_suspend(struct device *dev)
{
	if (gl_ts->suspended) {
		dev_info(dev, "Already in suspend state\n");
		return 0;
	}

	if (device_may_wakeup(dev) && (!gl_ts->device_needs_wakeup)) {
		gl_ts->device_needs_wakeup = true;
		enable_irq_wake(gl_ts->client->irq);
		return 0;
	}

	disable_irq(gl_ts->client->irq);

	/* put the device in low power mode */
	IT7260_ts_chipLowPowerMode(true);

	IT7260_ts_release_all();
	gl_ts->suspended = true;

	return 0;
}

static const struct dev_pm_ops IT7260_ts_dev_pm_ops = {
	.suspend = IT7260_ts_suspend,
	.resume  = IT7260_ts_resume,
};
#else
static int IT7260_ts_resume(struct device *dev)
{
	return 0;
}

static int IT7260_ts_suspend(struct device *dev)
{
	return 0;
}
#endif

static const struct i2c_device_id IT7260_ts_id[] = {
	{ DEVICE_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, IT7260_ts_id);

static const struct of_device_id IT7260_match_table[] = {
	{ .compatible = "ite,it7260_ts",},
	{},
};

static struct i2c_driver IT7260_ts_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = DEVICE_NAME,
		.of_match_table = IT7260_match_table,
#ifdef CONFIG_PM
		.pm = &IT7260_ts_dev_pm_ops,
#endif
	},
	.probe = IT7260_ts_probe,
	.remove = IT7260_ts_remove,
	.id_table = IT7260_ts_id,
};

module_i2c_driver(IT7260_ts_driver);

MODULE_DESCRIPTION("IT7260 Touchscreen Driver");
MODULE_LICENSE("GPL v2");
