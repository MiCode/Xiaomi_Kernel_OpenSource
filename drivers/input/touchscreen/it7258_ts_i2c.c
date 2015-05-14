/* drivers/input/touchscreen/it7258_ts_i2c.c
 *
 * Copyright (C) 2014 ITE Tech. Inc.
 *
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

#define MAX_BUFFER_SIZE			144
#define DEVICE_NAME			"IT7260"
#define SCREEN_X_RESOLUTION		320
#define SCREEN_Y_RESOLUTION		320
#define DEBUGFS_DIR_NAME		"ts_debug"

/* all commands writes go to this idx */
#define BUF_COMMAND			0x20
#define BUF_SYS_COMMAND			0x40
/* "device ready?" and "wake up please" and "read touch data" reads
 * go to this idx
 */
#define BUF_QUERY			0x80
/* most command response reads go to this idx */
#define BUF_RESPONSE			0xA0
#define BUF_SYS_RESPONSE		0xC0
/* reads of "point" go through here and produce 14 bytes of data */
#define BUF_POINT_INFO			0xE0

/* commands and their subcommands. when no subcommands exist, a zero
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
#define SYSFS_FW_UPLOAD_MODE_MANUAL	2
#define SYSFS_RESULT_FAIL		(-1)
#define SYSFS_RESULT_NOT_DONE		0
#define SYSFS_RESULT_SUCCESS		1
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

/* use this to include integers in commands */
#define CMD_UINT16(v)		((uint8_t)(v)) , ((uint8_t)((v) >> 8))

/* Function declarations */
static int fb_notifier_callback(struct notifier_block *self,
			unsigned long event, void *data);
static int IT7260_ts_resume(struct device *dev);
static int IT7260_ts_suspend(struct device *dev);

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

#define PD_FLAGS_DATA_TYPE_BITS		0xF0
/* other types (like chip-detected gestures) exist but we do not care */
#define PD_FLAGS_DATA_TYPE_TOUCH	0x00
/* set if pen touched, clear if finger(s) */
#define PD_FLAGS_NOT_PEN		0x08
/* a bit for each finger data that is valid (from lsb to msb) */
#define PD_FLAGS_HAVE_FINGERS		0x07
#define PD_PALM_FLAG_BIT		0x01
#define FD_PRESSURE_BITS		0x0F
#define FD_PRESSURE_NONE		0x00
#define FD_PRESSURE_HOVER		0x01
#define FD_PRESSURE_LIGHT		0x02
#define FD_PRESSURE_NORMAL		0x04
#define FD_PRESSURE_HIGH		0x08
#define FD_PRESSURE_HEAVY		0x0F

#define IT_VTG_MIN_UV		1800000
#define IT_VTG_MAX_UV		1800000
#define IT_I2C_VTG_MIN_UV	2600000
#define IT_I2C_VTG_MAX_UV	3300000

struct IT7260_ts_platform_data {
	u32 irqflags;
	u32 irq_gpio;
	u32 irq_gpio_flags;
	u32 reset_gpio;
	u32 reset_gpio_flags;
	bool wakeup;
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
#ifdef CONFIG_FB
	struct notifier_block fb_notif;
#endif
	struct dentry *dir;
};

static int8_t fwUploadResult;
static int8_t calibrationWasSuccessful;
static bool devicePresent;
static bool hadFingerDown;
static struct input_dev *input_dev;
static struct IT7260_ts_data *gl_ts;

#define LOGE(...)	pr_err(DEVICE_NAME ": " __VA_ARGS__)
#define LOGI(...)	printk(DEVICE_NAME ": " __VA_ARGS__)

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
static bool i2cReadNoReadyCheck(uint8_t bufferIndex, uint8_t *dataBuffer,
							uint16_t dataLength)
{
	struct i2c_msg msgs[2] = {
		{
			.addr = gl_ts->client->addr,
			.flags = I2C_M_NOSTART,
			.len = 1,
			.buf = &bufferIndex
		},
		{
			.addr = gl_ts->client->addr,
			.flags = I2C_M_RD,
			.len = dataLength,
			.buf = dataBuffer
		}
	};

	memset(dataBuffer, 0xFF, dataLength);

	return i2c_transfer(gl_ts->client->adapter, msgs, 2);
}

static bool i2cWriteNoReadyCheck(uint8_t bufferIndex,
			const uint8_t *dataBuffer, uint16_t dataLength)
{
	uint8_t txbuf[257];
	struct i2c_msg msg = {
		.addr = gl_ts->client->addr,
		.flags = 0,
		.len = dataLength + 1,
		.buf = txbuf
	};

	/* just to be careful */
        BUG_ON(dataLength > sizeof(txbuf) - 1);

	txbuf[0] = bufferIndex;
	memcpy(txbuf + 1, dataBuffer, dataLength);

	return i2c_transfer(gl_ts->client->adapter, &msg, 1);
}

/*
 * Device is apparently always ready for i2c but not for actual
 * register reads/writes. This function ascertains it is ready
 * for that too. the results of this call often were ignored.
 */
static bool waitDeviceReady(bool forever, bool slowly)
{
	uint8_t ucQuery;
	uint32_t count = DEVICE_READY_MAX_WAIT;

	do {
		if (!i2cReadNoReadyCheck(BUF_QUERY, &ucQuery, sizeof(ucQuery)))
			ucQuery = CMD_STATUS_BUSY;

		if (slowly)
			mdelay(1000);
		if (!forever)
			count--;

	} while ((ucQuery & CMD_STATUS_BUSY) && count);

	return !ucQuery;
}

static bool i2cRead(uint8_t bufferIndex, uint8_t *dataBuffer,
						uint16_t dataLength)
{
	waitDeviceReady(false, false);
	return i2cReadNoReadyCheck(bufferIndex, dataBuffer, dataLength);
}

static bool i2cWrite(uint8_t bufferIndex, const uint8_t *dataBuffer,
							uint16_t dataLength)
{
	waitDeviceReady(false, false);
	return i2cWriteNoReadyCheck(bufferIndex, dataBuffer, dataLength);
}

static bool chipFirmwareReinitialize(uint8_t cmdOfChoice)
{
	uint8_t cmd[] = {cmdOfChoice};
	uint8_t rsp[2];

	if (!i2cWrite(BUF_COMMAND, cmd, sizeof(cmd)))
		return false;

	if (!i2cRead(BUF_RESPONSE, rsp, sizeof(rsp)))
		return false;

	/* a reply of two zero bytes signifies success */
	return !rsp[0] && !rsp[1];
}

static bool chipFirmwareUpgradeModeEnterExit(bool enter)
{
	uint8_t cmd[] = {CMD_FIRMWARE_UPGRADE, 0, 'I', 'T', '7', '2',
						'6', '0', 0x55, 0xAA};
	uint8_t resp[2];

	cmd[1] = enter ? FIRMWARE_MODE_ENTER : FIRMWARE_MODE_EXIT;
	if (!i2cWrite(BUF_COMMAND, cmd, sizeof(cmd)))
		return false;

	if (!i2cRead(BUF_RESPONSE, resp, sizeof(resp)))
		return false;

	/* a reply of two zero bytes signifies success */
	return !resp[0] && !resp[1];
}

static bool chipSetStartOffset(uint16_t offset)
{
	uint8_t cmd[] = {CMD_SET_START_OFFSET, 0, CMD_UINT16(offset)};
	uint8_t resp[2];

	if (!i2cWrite(BUF_COMMAND, cmd, 4))
		return false;


	if (!i2cRead(BUF_RESPONSE, resp, sizeof(resp)))
		return false;


	/* a reply of two zero bytes signifies success */
	return !resp[0] && !resp[1];
}


/* write fwLength bytes from fwData at chip offset writeStartOffset */
static bool chipFlashWriteAndVerify(unsigned int fwLength,
			const uint8_t *fwData, uint16_t writeStartOffset)
{
	uint32_t curDataOfst;

	for (curDataOfst = 0; curDataOfst < fwLength;
				curDataOfst += FW_WRITE_CHUNK_SIZE) {

		uint8_t cmdWrite[2 + FW_WRITE_CHUNK_SIZE] = {CMD_FW_WRITE};
		uint8_t bufRead[FW_WRITE_CHUNK_SIZE];
		uint8_t cmdRead[2] = {CMD_FW_READ};
		unsigned i, nRetries;
		uint32_t curWriteSz;

		/* figure out how much to write */
		curWriteSz = fwLength - curDataOfst;
		if (curWriteSz > FW_WRITE_CHUNK_SIZE)
			curWriteSz = FW_WRITE_CHUNK_SIZE;

		/* prepare the write command */
		cmdWrite[1] = curWriteSz;
		for (i = 0; i < curWriteSz; i++)
			cmdWrite[i + 2] = fwData[curDataOfst + i];

		/* prepare the read command */
		cmdRead[1] = curWriteSz;

		for (nRetries = 0; nRetries < FW_WRITE_RETRY_COUNT;
							nRetries++) {

			/* set write offset and write the data*/
			chipSetStartOffset(writeStartOffset + curDataOfst);
			i2cWrite(BUF_COMMAND, cmdWrite, 2 + curWriteSz);

			/* set offset and read the data back */
			chipSetStartOffset(writeStartOffset + curDataOfst);
			i2cWrite(BUF_COMMAND, cmdRead, sizeof(cmdRead));
			i2cRead(BUF_RESPONSE, bufRead, curWriteSz);

			/* verify. If success break out of retry loop */
			i = 0;
			while (i < curWriteSz && bufRead[i] == cmdWrite[i + 2])
				i++;
			if (i == curWriteSz)
				break;
			pr_err("write of data offset %u failed on try %u at byte %u/%u\n",
				curDataOfst, nRetries, i, curWriteSz);
		}
		/* if we've failed after all the retries, tell the caller */
		if (nRetries == FW_WRITE_RETRY_COUNT)
			return false;
	}

	return true;
}

static bool chipFirmwareUpload(uint32_t fwLen, const uint8_t *fwData,
				uint32_t cfgLen, const uint8_t *cfgData)
{
	bool success = false;

	/* enter fw upload mode */
	if (!chipFirmwareUpgradeModeEnterExit(true))
		return false;

	/* flash the firmware if requested */
	if (fwLen && fwData && !chipFlashWriteAndVerify(fwLen, fwData, 0)) {
		LOGE("failed to upload touch firmware\n");
		goto out;
	}

	/* flash config data if requested */
	if (fwLen && fwData && !chipFlashWriteAndVerify(cfgLen, cfgData,
						CHIP_FLASH_SIZE - cfgLen)) {
		LOGE("failed to upload touch cfg data\n");
		goto out;
	}

	success = true;

out:
	return chipFirmwareUpgradeModeEnterExit(false) &&
		chipFirmwareReinitialize(CMD_FIRMWARE_REINIT_6F) && success;
}


/* both buffers should be VERSION_LENGTH in size,
 * but only a part of them is significant
 */
static bool chipGetVersions(uint8_t *verFw, uint8_t *verCfg, bool logIt)
{
	/* this code to get versions is reproduced as was written, but it does
	 * not make sense. Something here *PROBABLY IS* wrong
	 */
	static const uint8_t cmdReadFwVer[] = {CMD_READ_VERSIONS, VER_FIRMWARE};
	static const uint8_t cmdReadCfgVer[] = {CMD_READ_VERSIONS, VER_CONFIG};
	bool ret = true;

	/* this structure is so that we definitely do all the calls, but still
	 * return a status in case anyone cares
	 */
	ret = i2cWrite(BUF_COMMAND, cmdReadFwVer, sizeof(cmdReadFwVer)) && ret;
	ret = i2cRead(BUF_RESPONSE, verFw, VERSION_LENGTH) && ret;
	ret = i2cWrite(BUF_COMMAND, cmdReadCfgVer,
					sizeof(cmdReadCfgVer)) && ret;
	ret = i2cRead(BUF_RESPONSE, verCfg, VERSION_LENGTH) && ret;

	if (logIt)
		LOGI("current versions: fw@{%X,%X,%X,%X}, cfg@{%X,%X,%X,%X}\n",
			verFw[5], verFw[6], verFw[7], verFw[8],
			verCfg[1], verCfg[2], verCfg[3], verCfg[4]);

	return ret;
}

static int IT7260_ts_chipLowPowerMode(bool low)
{
	static const uint8_t cmdGoSleep[] = {CMD_PWR_CTL,
					0x00, PWR_CTL_SLEEP_MODE};
	uint8_t dummy;

	if (low)
		i2cWriteNoReadyCheck(BUF_COMMAND, cmdGoSleep,
					sizeof(cmdGoSleep));
	else
		i2cReadNoReadyCheck(BUF_QUERY, &dummy, sizeof(dummy));

	return 0;
}

static ssize_t sysfsUpgradeStore(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	const struct firmware *fw, *cfg;
	uint8_t verFw[10], verCfg[10];
	unsigned fwLen = 0, cfgLen = 0;
	bool manualUpgrade, success;
	int mode = 0, ret;

	ret = request_firmware(&fw, "it7260.fw", dev);
	if (ret)
		LOGE("failed to get firmware for it7260\n");
	else
		fwLen = fw->size;

	ret = request_firmware(&cfg, "it7260.cfg", dev);
	if (ret)
		LOGE("failed to get config data for it7260\n");
	else
		cfgLen = cfg->size;

	ret = sscanf(buf, "%d", &mode);
	manualUpgrade = mode == SYSFS_FW_UPLOAD_MODE_MANUAL;
	LOGI("firmware found %ub of fw and %ub of config in %s mode\n",
		fwLen, cfgLen, manualUpgrade ? "manual" : "normal");

	chipGetVersions(verFw, verCfg, true);

	fwUploadResult = SYSFS_RESULT_NOT_DONE;
	if (fwLen && cfgLen) {
		if (manualUpgrade || (verFw[5] < fw->data[8] || verFw[6] <
			fw->data[9] || verFw[7] < fw->data[10] || verFw[8] <
			fw->data[11]) || (verCfg[1] < cfg->data[cfgLen - 8]
			|| verCfg[2] < cfg->data[cfgLen - 7] || verCfg[3] <
			cfg->data[cfgLen - 6] ||
			verCfg[4] < cfg->data[cfgLen - 5])){
			LOGI("firmware/config will be upgraded\n");
			disable_irq(gl_ts->client->irq);
			success = chipFirmwareUpload(fwLen, fw->data, cfgLen,
								cfg->data);
			enable_irq(gl_ts->client->irq);

			fwUploadResult = success ?
				SYSFS_RESULT_SUCCESS : SYSFS_RESULT_FAIL;
			LOGI("upload %s\n", success ? "success" : "failed");
		} else {
			LOGI("firmware/config upgrade not needed\n");
		}
	}

	if (fwLen)
		release_firmware(fw);

	if (cfgLen)
		release_firmware(cfg);

	return count;
}

static ssize_t sysfsUpgradeShow(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_BUFFER_SIZE, "%d", fwUploadResult);
}

static ssize_t sysfsCalibrationShow(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_BUFFER_SIZE, "%d", calibrationWasSuccessful);
}

static bool chipSendCalibrationCmd(bool autoTuneOn)
{
	uint8_t cmdCalibrate[] = {CMD_CALIBRATE, 0, autoTuneOn ? 1 : 0, 0, 0};
	return i2cWrite(BUF_COMMAND, cmdCalibrate, sizeof(cmdCalibrate));
}

static ssize_t sysfsCalibrationStore(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	uint8_t resp;

	if (!chipSendCalibrationCmd(false))
		LOGE("failed to send calibration command\n");
	else {
		calibrationWasSuccessful =
			i2cRead(BUF_RESPONSE, &resp, sizeof(resp))
			? SYSFS_RESULT_SUCCESS : SYSFS_RESULT_FAIL;

		/* previous logic that was here never called
		 * chipFirmwareReinitialize() due to checking a
		 * guaranteed-not-null value against null. We now
		 * call it. Hopefully this is OK
		 */
		if (!resp)
			LOGI("chipFirmwareReinitialize -> %s\n",
			chipFirmwareReinitialize(CMD_FIRMWARE_REINIT_6F)
			? "success" : "fail");
	}

	return count;
}

static ssize_t sysfsPointShow(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	uint8_t pointData[sizeof(struct PointData)];
	bool readSuccess;
	ssize_t ret;

	readSuccess = i2cReadNoReadyCheck(BUF_POINT_INFO, pointData,
							sizeof(pointData));
	ret = snprintf(buf, MAX_BUFFER_SIZE,
		"point_show read ret[%d]--point[%x][%x][%x][%x][%x][%x][%x][%x][%x][%x][%x][%x][%x][%x]=\n",
		readSuccess, pointData[0], pointData[1], pointData[2],
		pointData[3], pointData[4], pointData[5], pointData[6],
		pointData[7], pointData[8], pointData[9], pointData[10],
		pointData[11], pointData[12], pointData[13]);

	LOGI("%s", buf);

	return ret;
}

static ssize_t sysfsPointStore(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t sysfsStatusShow(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_BUFFER_SIZE, "%d\n", devicePresent ? 1 : 0);
}

static ssize_t sysfsStatusStore(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	uint8_t verFw[10], verCfg[10];

	chipGetVersions(verFw, verCfg, true);

	return count;
}

static ssize_t sysfsVersionShow(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	uint8_t verFw[10], verCfg[10];

	chipGetVersions(verFw, verCfg, false);
	return snprintf(buf, MAX_BUFFER_SIZE, "%x,%x,%x,%x # %x,%x,%x,%x\n",
			verFw[5], verFw[6], verFw[7], verFw[8],
			verCfg[1], verCfg[2], verCfg[3], verCfg[4]);
}

static ssize_t sysfsVersionStore(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
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

	ret = sscanf(buf, "%d", &go_to_sleep);

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

static DEVICE_ATTR(status, S_IRUGO|S_IWUSR|S_IWGRP,
				sysfsStatusShow, sysfsStatusStore);
static DEVICE_ATTR(version, S_IRUGO|S_IWUSR|S_IWGRP,
				sysfsVersionShow, sysfsVersionStore);
static DEVICE_ATTR(sleep, S_IRUGO|S_IWUSR|S_IWGRP,
				sysfsSleepShow, sysfsSleepStore);

static struct attribute *it7260_attrstatus[] = {
	&dev_attr_status.attr,
	&dev_attr_version.attr,
	&dev_attr_sleep.attr,
	NULL
};

static const struct attribute_group it7260_attrstatus_group = {
	.attrs = it7260_attrstatus,
};

static DEVICE_ATTR(calibration, S_IRUGO|S_IWUSR|S_IWGRP,
			sysfsCalibrationShow, sysfsCalibrationStore);
static DEVICE_ATTR(upgrade, S_IRUGO|S_IWUSR|S_IWGRP,
			sysfsUpgradeShow, sysfsUpgradeStore);
static DEVICE_ATTR(point, S_IRUGO|S_IWUSR|S_IWGRP,
			sysfsPointShow, sysfsPointStore);

static struct attribute *it7260_attributes[] = {
	&dev_attr_calibration.attr,
	&dev_attr_upgrade.attr,
	&dev_attr_point.attr,
	NULL
};

static const struct attribute_group it7260_attr_group = {
	.attrs = it7260_attributes,
};

static void chipExternalCalibration(bool autoTuneEnabled)
{
	uint8_t resp[2];

	LOGI("sent calibration command -> %d\n",
			chipSendCalibrationCmd(autoTuneEnabled));
	waitDeviceReady(true, true);
	i2cReadNoReadyCheck(BUF_RESPONSE, resp, sizeof(resp));
	chipFirmwareReinitialize(CMD_FIRMWARE_REINIT_C);
}

void sendCalibrationCmd(void)
{
	chipExternalCalibration(false);
}
EXPORT_SYMBOL(sendCalibrationCmd);

static void IT7260_ts_release_all(void)
{
	input_report_key(gl_ts->input_dev, BTN_TOUCH, 0);
	input_mt_sync(gl_ts->input_dev);
	input_sync(gl_ts->input_dev);
}

static void readFingerData(uint16_t *xP, uint16_t *yP, uint8_t *pressureP,
						const struct FingerData *fd)
{
	uint16_t x = fd->xLo;
	uint16_t y = fd->yLo;

	x += ((uint16_t)(fd->hi & 0x0F)) << 8;
	y += ((uint16_t)(fd->hi & 0xF0)) << 4;

	if (xP)
		*xP = x;
	if (yP)
		*yP = y;
	if (pressureP)
		*pressureP = fd->pressure & FD_PRESSURE_BITS;
}

static irqreturn_t IT7260_ts_threaded_handler(int irq, void *devid)
{
	struct PointData pointData;
	uint8_t devStatus;
	uint8_t pressure = FD_PRESSURE_NONE;
	uint16_t x, y;

	/* This code adds the touch-to-wake functioanlity to the ITE tech
	 * driver. When the device is in suspend driver, it sends the
	 * KEY_WAKEUP event to wake the device. The pm_stay_awake() call
	 * tells the pm core to stay awake untill the CPU cores up already. The
	 * schedule_work() call schedule a work that tells the pm core to relax
	 * once the CPU cores are up.
	 */
	if (gl_ts->device_needs_wakeup) {
		pm_stay_awake(&gl_ts->client->dev);
		gl_ts->device_needs_wakeup = false;
		input_report_key(gl_ts->input_dev, KEY_WAKEUP, 1);
		input_sync(gl_ts->input_dev);
		input_report_key(gl_ts->input_dev, KEY_WAKEUP, 0);
		input_sync(gl_ts->input_dev);
		schedule_work(&gl_ts->work_pm_relax);
	}

	/* verify there is point data to read & it is readable and valid */
	i2cReadNoReadyCheck(BUF_QUERY, &devStatus, sizeof(devStatus));
	if (!((devStatus & PT_INFO_BITS) & PT_INFO_YES)) {
		return IRQ_HANDLED;
	}
	if (!i2cReadNoReadyCheck(BUF_POINT_INFO, (void *)&pointData,
						sizeof(pointData))) {
		dev_err(&gl_ts->client->dev,
			"readTouchDataPoint() failed to read point data buffer\n");
		return IRQ_HANDLED;
	}
	if ((pointData.flags & PD_FLAGS_DATA_TYPE_BITS) !=
					PD_FLAGS_DATA_TYPE_TOUCH) {
		dev_err(&gl_ts->client->dev,
			"readTouchDataPoint() dropping non-point data of type 0x%02X\n",
							pointData.flags);
		return IRQ_HANDLED;
	}

	if ((pointData.flags & PD_FLAGS_HAVE_FINGERS) & 1)
		readFingerData(&x, &y, &pressure, pointData.fd);

	if (pressure >= FD_PRESSURE_LIGHT) {
		if (!hadFingerDown)
			hadFingerDown = true;

		readFingerData(&x, &y, &pressure, pointData.fd);

		input_report_key(gl_ts->input_dev, BTN_TOUCH, 1);
		input_report_abs(gl_ts->input_dev, ABS_MT_POSITION_X, x);
		input_report_abs(gl_ts->input_dev, ABS_MT_POSITION_Y, y);
		input_mt_sync(gl_ts->input_dev);
		input_sync(gl_ts->input_dev);


	} else if (hadFingerDown) {
		hadFingerDown = false;

		input_report_key(gl_ts->input_dev, BTN_TOUCH, 0);
		input_mt_sync(gl_ts->input_dev);
		input_sync(gl_ts->input_dev);
	}

	return IRQ_HANDLED;
}

static void IT7260_ts_work_func(struct work_struct *work)
{
	pm_relax(&gl_ts->client->dev);
}

static bool chipIdentifyIT7260(void)
{
	static const uint8_t cmdIdent[] = {CMD_IDENT_CHIP};
	static const uint8_t expectedID[] = {0x0A, 'I', 'T', 'E', '7',
							'2', '6', '0'};
	uint8_t chipID[10] = {0,};

	waitDeviceReady(true, false);

	if (!i2cWriteNoReadyCheck(BUF_COMMAND, cmdIdent, sizeof(cmdIdent))) {
		LOGE("i2cWrite() failed\n");
		return false;
	}

	waitDeviceReady(true, false);

	if (!i2cReadNoReadyCheck(BUF_RESPONSE, chipID, sizeof(chipID))) {
		LOGE("i2cRead() failed\n");
		return false;
	}
	LOGI("chipIdentifyIT7260 read id: %02X %c%c%c%c%c%c%ci %c%c\n",
		chipID[0], chipID[1], chipID[2], chipID[3], chipID[4],
		chipID[5], chipID[6], chipID[7], chipID[8], chipID[9]);

	if (memcmp(chipID, expectedID, sizeof(expectedID)))
		return false;

	if (chipID[8] == '5' && chipID[9] == '6')
		LOGI("rev BX3 found\n");
	else if (chipID[8] == '6' && chipID[9] == '6')
		LOGI("rev BX4 found\n");
	else
		LOGI("unknown revision (0x%02X 0x%02X) found\n",
						chipID[8], chipID[9]);

	return true;
}

static int IT7260_ts_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	static const uint8_t cmdStart[] = {CMD_UNKNOWN_7};
	struct IT7260_ts_platform_data *pdata;
	uint8_t rsp[2];
	int ret = -1;
	int rc;
	struct dentry *temp;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		LOGE("need I2C_FUNC_I2C\n");
		ret = -ENODEV;
		goto err_out;
	}

	if (!client->irq) {
		LOGE("need IRQ\n");
		ret = -ENODEV;
		goto err_out;
	}
	gl_ts = kzalloc(sizeof(*gl_ts), GFP_KERNEL);
	if (!gl_ts) {
		ret = -ENOMEM;
		goto err_out;
	}

	gl_ts->client = client;
	i2c_set_clientdata(client, gl_ts);

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
			sizeof(struct IT7260_ts_platform_data), GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}
	} else
		pdata = client->dev.platform_data;

	if (!pdata) {
		dev_err(&client->dev, "Invalid pdata\n");
		return -EINVAL;
	}
	gl_ts->pdata = pdata;
	if (sysfs_create_group(&(client->dev.kobj), &it7260_attrstatus_group)) {
		dev_err(&client->dev, "failed to register sysfs #1\n");
		goto err_sysfs_grp_create_1;
	}

	gl_ts->vdd = regulator_get(&gl_ts->client->dev, "vdd");
	if (IS_ERR(gl_ts->vdd)) {
		dev_err(&gl_ts->client->dev,
				"Regulator get failed vdd\n");
		gl_ts->vdd = NULL;
	} else {
		rc = regulator_set_voltage(gl_ts->vdd,
				IT_VTG_MIN_UV, IT_VTG_MAX_UV);
		if (rc)
			dev_err(&gl_ts->client->dev,
				"Regulator set_vtg failed vdd\n");
	}

	gl_ts->avdd = regulator_get(&gl_ts->client->dev, "avdd");
	if (IS_ERR(gl_ts->avdd)) {
		dev_err(&gl_ts->client->dev,
				"Regulator get failed avdd\n");
		gl_ts->avdd = NULL;
	} else {
		rc = regulator_set_voltage(gl_ts->avdd, IT_I2C_VTG_MIN_UV,
							IT_I2C_VTG_MAX_UV);
		if (rc)
			dev_err(&gl_ts->client->dev,
				"Regulator get failed avdd\n");
	}

	if (gl_ts->vdd) {
		rc = regulator_enable(gl_ts->vdd);
		if (rc) {
			dev_err(&gl_ts->client->dev,
				"Regulator vdd enable failed rc=%d\n", rc);
			return rc;
		}
	}

	if (gl_ts->avdd) {
		rc = regulator_enable(gl_ts->avdd);
		if (rc) {
			dev_err(&gl_ts->client->dev,
				"Regulator avdd enable failed rc=%d\n", rc);
			return rc;
		}
	}

	/* reset gpio info */
	pdata->reset_gpio = of_get_named_gpio_flags(client->dev.of_node,
					"ite,reset-gpio", 0,
					&pdata->reset_gpio_flags);
	if (gpio_is_valid(pdata->reset_gpio)) {
		if (gpio_request(pdata->reset_gpio, "ite_reset_gpio"))
			dev_err(&gl_ts->client->dev,
				"gpio_request failed for reset GPIO\n");
		if (gpio_direction_output(pdata->reset_gpio, 0))
			dev_err(&gl_ts->client->dev,
				"gpio_direction_output for reset GPIO\n");
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

	if (!chipIdentifyIT7260()) {
		LOGI("chipIdentifyIT7260 FAIL");
		goto err_ident_fail_or_input_alloc;
	}

	input_dev = input_allocate_device();
	if (!input_dev) {
		LOGE("failed to allocate input device\n");
		ret = -ENOMEM;
		goto err_ident_fail_or_input_alloc;
	}
	gl_ts->input_dev = input_dev;

	input_dev->name = DEVICE_NAME;
	input_dev->phys = "I2C";
	input_dev->id.bustype = BUS_I2C;
	input_dev->id.vendor = 0x0001;
	input_dev->id.product = 0x7260;
	set_bit(EV_SYN, input_dev->evbit);
	set_bit(EV_KEY, input_dev->evbit);
	set_bit(EV_ABS, input_dev->evbit);
	set_bit(INPUT_PROP_DIRECT,input_dev->propbit);
	set_bit(BTN_TOUCH, input_dev->keybit);
	set_bit(KEY_SLEEP,input_dev->keybit);
	set_bit(KEY_POWER,input_dev->keybit);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0,
				SCREEN_X_RESOLUTION, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0,
				SCREEN_Y_RESOLUTION, 0, 0);
	input_set_drvdata(gl_ts->input_dev, gl_ts);

	if (pdata->wakeup) {
		set_bit(KEY_WAKEUP, gl_ts->input_dev->keybit);
		INIT_WORK(&gl_ts->work_pm_relax, IT7260_ts_work_func);
		device_init_wakeup(&client->dev, pdata->wakeup);
	}

	if (input_register_device(input_dev)) {
		LOGE("failed to register input device\n");
		goto err_input_register;
	}

	if (request_threaded_irq(client->irq, NULL, IT7260_ts_threaded_handler,
		IRQF_TRIGGER_LOW | IRQF_ONESHOT, client->name, gl_ts)) {
		dev_err(&client->dev, "request_irq failed\n");
		goto err_irq_reg;
	}

	if (sysfs_create_group(&(client->dev.kobj), &it7260_attr_group)) {
		dev_err(&client->dev, "failed to register sysfs #2\n");
		goto err_sysfs_grp_create_2;
	}

#if defined(CONFIG_FB)
	gl_ts->fb_notif.notifier_call = fb_notifier_callback;

	ret = fb_register_client(&gl_ts->fb_notif);
	if (ret)
		dev_err(&client->dev, "Unable to register fb_notifier: %d\n",
									ret);
#endif
	
	devicePresent = true;

	i2cWriteNoReadyCheck(BUF_COMMAND, cmdStart, sizeof(cmdStart));
	mdelay(10);
	i2cReadNoReadyCheck(BUF_RESPONSE, rsp, sizeof(rsp));
	mdelay(10);

	gl_ts->dir = debugfs_create_dir(DEBUGFS_DIR_NAME, NULL);
	if (gl_ts->dir == NULL || IS_ERR(gl_ts->dir)) {
		dev_err(&client->dev,
			"%s: Failed to create debugfs directory, rc = %ld\n",
			__func__, PTR_ERR(gl_ts->dir));
		ret = PTR_ERR(gl_ts->dir);
		goto err_create_debugfs_dir;
	}

	temp = debugfs_create_file("suspend", S_IRUSR | S_IWUSR, gl_ts->dir,
					gl_ts, &debug_suspend_fops);
	if (temp == NULL || IS_ERR(temp)) {
		dev_err(&client->dev,
			"%s: Failed to create suspend debugfs file, rc = %ld\n",
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

err_sysfs_grp_create_2:
	free_irq(client->irq, gl_ts);

err_irq_reg:
	input_unregister_device(input_dev);
	input_dev = NULL;

err_input_register:
	if (pdata->wakeup) {
		cancel_work_sync(&gl_ts->work_pm_relax);
		device_init_wakeup(&client->dev, false);
	}
	if (input_dev)
		input_free_device(input_dev);

err_ident_fail_or_input_alloc:
	sysfs_remove_group(&(client->dev.kobj), &it7260_attrstatus_group);

err_sysfs_grp_create_1:
	kfree(gl_ts);

err_out:
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
	if (gl_ts->pdata->wakeup) {
		cancel_work_sync(&gl_ts->work_pm_relax);
		device_init_wakeup(&client->dev, false);
	}
	devicePresent = false;
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

	if (device_may_wakeup(dev)) {
		if (gl_ts->device_needs_wakeup) {
			gl_ts->device_needs_wakeup = false;
			disable_irq_wake(gl_ts->client->irq);
		}
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

	if (device_may_wakeup(dev)) {
		if (!gl_ts->device_needs_wakeup) {
			gl_ts->device_needs_wakeup = true;
			enable_irq_wake(gl_ts->client->irq);
		}
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
