/* drivers/input/touchscreen/it7258_ts_i2c.c
 *
 * Copyright (C) 2014 ITE Tech. Inc.
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
#include <linux/wakelock.h>

#define MAX_BUFFER_SIZE			144
#define DEVICE_NAME			"IT7260"
#define SCREEN_X_RESOLUTION		320
#define SCREEN_Y_RESOLUTION		320

#define BUF_COMMAND			0x20 /* all commands writes go to this idx */
#define BUF_SYS_COMMAND			0x40
#define BUF_QUERY			0x80 /* "revice ready?" and "wake up please" and "read touch data" reads go to this idx */
#define BUF_RESPONSE			0xA0 /* most command responce reads go to this idx */
#define BUF_SYS_RESPONSE		0xC0
#define BUF_POINT_INFO			0xE0 /* reads of "point" go through here and produce 14 bytes of data */

/* commands and their subcommands. when no subcommands exist, a zero is send as the second byte */
#define CMD_IDENT_CHIP			0x00
#define CMD_READ_VERSIONS		0x01 /* VERSION_LENGTH bytes of data in response */
# define VER_FIRMWARE			0x00
# define VER_CONFIG			0x06
# define VERSION_LENGTH			10
#define CMD_PWR_CTL			0x04 /* subcommand is zero, next byte is power mode */
# define PWR_CTL_LOW_POWER_MODE		0x01 /* idle mode */
# define PWR_CTL_SLEEP_MODE		0x02 /* sleep mode */
#define CMD_UNKNOWN_7			0x07 /* command is not documented in the datasheet v1.0.0.7 */
#define CMD_FIRMWARE_REINIT_C		0x0C
#define CMD_CALIBRATE			0x13 /* needs to be followed by 4 bytes of zeroes */
#define CMD_FIRMWARE_UPGRADE		0x60
# define FIRMWARE_MODE_ENTER		0x00
# define FIRMWARE_MODE_EXIT		0x80
#define CMD_SET_START_OFFSET		0x61 /* address for FW read/write */
#define CMD_FW_WRITE			0x62 /* subcommand is number of bytes to write */
#define CMD_FW_READ			0x63 /* subcommand is number of bytes to read */
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
#define BT_INFO_NONE_BUT_DOWN		0x08 /* no new data but finder(s) still down */

/* use this to include integers in commands */
#define CMD_UINT16(v)		((uint8_t)(v)) , ((uint8_t)((v) >> 8))


struct FingerData {
	uint8_t xLo;
	uint8_t hi;
	uint8_t yLo;
	uint8_t pressure;
} __attribute__((packed));

struct PointData {
	uint8_t flags;
	uint8_t palm;
	struct FingerData fd[3];
} __attribute__((packed));

#define PD_FLAGS_DATA_TYPE_BITS		0xF0
/* other types (like chip-detected gestures) exist but we do not care */
#define PD_FLAGS_DATA_TYPE_TOUCH	0x00
#define PD_FLAGS_NOT_PEN		0x08 /* set if pen touched, clear if finger(s) */
#define PD_FLAGS_HAVE_FINGERS		0x07 /* a bit for each finger data that is valid (from lsb to msb) */
#define PD_PALM_FLAG_BIT		0x01
#define FD_PRESSURE_BITS		0x0F
#define FD_PRESSURE_NONE		0x00
#define FD_PRESSURE_HOVER		0x01
#define FD_PRESSURE_LIGHT		0x02
#define FD_PRESSURE_NORMAL		0x04
#define FD_PRESSURE_HIGH		0x08
#define FD_PRESSURE_HEAVY		0x0F

struct IT7260_ts_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
};

static int8_t fwUploadResult = SYSFS_RESULT_NOT_DONE;
static int8_t calibrationWasSuccessful = SYSFS_RESULT_NOT_DONE;
static bool devicePresent = false;
static DEFINE_MUTEX(sleepModeMutex);
static bool chipAwake = true;
static bool hadFingerDown = false;
static bool isDeviceSleeping = false;
static bool isDeviceSuspend = false;
static struct input_dev *input_dev;
static struct IT7260_ts_data *gl_ts;

#define LOGE(...)	pr_err(DEVICE_NAME ": " __VA_ARGS__)
#define LOGI(...)	printk(DEVICE_NAME ": " __VA_ARGS__)

/* internal use func - does not make sure chip is ready before read */
static bool i2cReadNoReadyCheck(uint8_t bufferIndex, uint8_t *dataBuffer, uint16_t dataLength)
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

static bool i2cWriteNoReadyCheck(uint8_t bufferIndex, const uint8_t *dataBuffer, uint16_t dataLength)
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
 * Device is apparently always ready for i2c but not for actual register reads/writes.
 * This function ascertains it is ready for that too. the results of this call often
 * were ignored.
 */
static bool waitDeviceReady(bool forever, bool slowly)
{
	uint8_t ucQuery;
	uint32_t count = DEVICE_READY_MAX_WAIT;

	do{
		if (!i2cReadNoReadyCheck(BUF_QUERY, &ucQuery, sizeof(ucQuery)))
			ucQuery = CMD_STATUS_BUSY;

		if (slowly)
			mdelay(1000);
		if (!forever)
			count--;

	}while((ucQuery & CMD_STATUS_BUSY) && count);

	return !ucQuery;
}

static bool i2cRead(uint8_t bufferIndex, uint8_t *dataBuffer, uint16_t dataLength)
{
	waitDeviceReady(false, false);
	return i2cReadNoReadyCheck(bufferIndex, dataBuffer, dataLength);
}

static bool i2cWrite(uint8_t bufferIndex, const uint8_t *dataBuffer, uint16_t dataLength)
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
	uint8_t cmd[] = {CMD_FIRMWARE_UPGRADE, 0, 'I', 'T', '7', '2', '6', '0', 0x55, 0xAA};
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
static bool chipFlashWriteAndVerify(unsigned int fwLength, const uint8_t *fwData, uint16_t writeStartOffset)
{
	uint32_t curDataOfst;

	for (curDataOfst = 0; curDataOfst < fwLength; curDataOfst += FW_WRITE_CHUNK_SIZE) {

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

		for (nRetries = 0; nRetries < FW_WRITE_RETRY_COUNT; nRetries++) {

			/* set write offset and write the data*/
			chipSetStartOffset(writeStartOffset + curDataOfst);
			i2cWrite(BUF_COMMAND, cmdWrite, 2 + curWriteSz);

			/* set offset and read the data back */
			chipSetStartOffset(writeStartOffset + curDataOfst);
			i2cWrite(BUF_COMMAND, cmdRead, sizeof(cmdRead));
			i2cRead(BUF_RESPONSE, bufRead, curWriteSz);

			/* verify. If success break out of retry loop */
			for (i = 0; i < curWriteSz && bufRead[i] == cmdWrite[i + 2]; i++);
			if (i == curWriteSz)
				break;
			LOGE("write of data offset %u failed on try %u at byte %u/%u\n", curDataOfst, nRetries, i, curWriteSz);
		}
		/* if we've failed after all the retries, tell the caller */
		if (nRetries == FW_WRITE_RETRY_COUNT)
			return false;
	}

	return true;
}

static bool chipFirmwareUpload(uint32_t fwLen, const uint8_t *fwData, uint32_t cfgLen, const uint8_t *cfgData)
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
	if (fwLen && fwData && !chipFlashWriteAndVerify(cfgLen, cfgData, CHIP_FLASH_SIZE - cfgLen)) {
		LOGE("failed to upload touch cfg data\n");
		goto out;
	}

	success = true;

out:
	return chipFirmwareUpgradeModeEnterExit(false) && chipFirmwareReinitialize(CMD_FIRMWARE_REINIT_6F) && success;
}

/*
 * both buffers should be VERSION_LENGTH in size,
 * but only a part of them is significant
 */
static bool chipGetVersions(uint8_t *verFw, uint8_t *verCfg, bool logIt)
{
	/* this code to get versions is reproduced as was written, but it does not make sense. Something here *PROBABLY IS* wrong */
	static const uint8_t cmdReadFwVer[] = {CMD_READ_VERSIONS, VER_FIRMWARE};
	static const uint8_t cmdReadCfgVer[] = {CMD_READ_VERSIONS, VER_CONFIG};
	bool ret = true;

	/* this structure is so that we definitely do all the calls, but still return a status in case anyone cares */
	ret = i2cWrite(BUF_COMMAND, cmdReadFwVer, sizeof(cmdReadFwVer)) && ret;
	ret = i2cRead(BUF_RESPONSE, verFw, VERSION_LENGTH) && ret;
	ret = i2cWrite(BUF_COMMAND, cmdReadCfgVer, sizeof(cmdReadCfgVer)) && ret;
	ret = i2cRead(BUF_RESPONSE, verCfg, VERSION_LENGTH) && ret;

	if (logIt)
		LOGI("current versions: fw@{%X,%X,%X,%X}, cfg@{%X,%X,%X,%X}\n",
			verFw[5], verFw[6], verFw[7], verFw[8],
			verCfg[1], verCfg[2], verCfg[3], verCfg[4]);

	return ret;
}

static ssize_t sysfsUpgradeStore(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	const struct firmware *fw, *cfg;
	uint8_t verFw[10], verCfg[10];
	unsigned fwLen = 0, cfgLen = 0;
	bool manualUpgrade, success;
	int mode = 0;

	if (request_firmware(&fw, "it7260.fw", dev))
		LOGE("failed to get firmware for it7260\n");
	else
		fwLen = fw->size;


	if (request_firmware(&cfg, "it7260.cfg", dev))
		LOGE("failed to get config data for it7260\n");
	else
		cfgLen = cfg->size;

	sscanf(buf, "%d", &mode);
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
			success = chipFirmwareUpload(fwLen, fw->data, cfgLen, cfg->data);
			enable_irq(gl_ts->client->irq);

			fwUploadResult = success ? SYSFS_RESULT_SUCCESS : SYSFS_RESULT_FAIL;
			LOGI("upload %s\n", success ? "success" : "failed");
		}
		else {
			LOGI("firmware/config upgrade not needed\n");
		}
	}

	if (fwLen)
		release_firmware(fw);

	if (cfgLen)
		release_firmware(cfg);

	return count;
}

static ssize_t sysfsUpgradeShow(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d", fwUploadResult);
}

static ssize_t sysfsCalibrationShow(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d", calibrationWasSuccessful);
}

static bool chipSendCalibrationCmd(bool autoTuneOn)
{
	uint8_t cmdCalibrate[] = {CMD_CALIBRATE, 0, autoTuneOn ? 1 : 0, 0, 0};
	return i2cWrite(BUF_COMMAND, cmdCalibrate, sizeof(cmdCalibrate));
}

static ssize_t sysfsCalibrationStore(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	uint8_t resp;

	if (!chipSendCalibrationCmd(false))
		LOGE("failed to send calibration command\n");
	else {
		calibrationWasSuccessful = i2cRead(BUF_RESPONSE, &resp, sizeof(resp)) ? SYSFS_RESULT_SUCCESS : SYSFS_RESULT_FAIL;

		/* previous logic that was here never called chipFirmwareReinitialize() due to checking a guaranteed-not-null value against null. We now call it. Hopefully this is OK */
		if (!resp)
			LOGI("chipFirmwareReinitialize -> %s\n", chipFirmwareReinitialize(CMD_FIRMWARE_REINIT_6F) ? "success" : "fail");
	}

	return count;
}

static ssize_t sysfsPointShow(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t pointData[sizeof(struct PointData)];
	bool readSuccess;
	ssize_t ret;

	readSuccess = i2cReadNoReadyCheck(BUF_POINT_INFO, pointData, sizeof(pointData));
	ret = sprintf(buf, "point_show read ret[%d]--point[%x][%x][%x][%x][%x][%x][%x][%x][%x][%x][%x][%x][%x][%x]=\n",
		readSuccess, pointData[0],pointData[1],pointData[2],pointData[3],
		pointData[4],pointData[5],pointData[6],pointData[7],pointData[8],
		pointData[9],pointData[10],pointData[11],pointData[12],pointData[13]);


	LOGI("%s", buf);

	return ret;
}

static ssize_t sysfsPointStore(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t sysfsStatusShow(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", devicePresent ? 1 : 0);
}

static ssize_t sysfsStatusStore(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	uint8_t verFw[10], verCfg[10];

	chipGetVersions(verFw, verCfg, true);

	return count;
}

static ssize_t sysfsVersionShow(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t verFw[10], verCfg[10];

	chipGetVersions(verFw, verCfg, false);
	return sprintf(buf, "%x,%x,%x,%x # %x,%x,%x,%x\n",verFw[5], verFw[6], verFw[7], verFw[8], verCfg[1], verCfg[2], verCfg[3], verCfg[4]);
}

static ssize_t sysfsVersionStore(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t sysfsSleepShow(struct device *dev, struct device_attribute *attr, char *buf)
{
	/*
	 * The usefulness of this was questionable at best - we were at least leaking a byte of
	 * kernel data (by claiming to return a byte but not writing to buf. To fix this now
	 * we actually return the sleep status
	 */
	if (!mutex_lock_interruptible(&sleepModeMutex)) {
		*buf = chipAwake ? '1' : '0';
		mutex_unlock(&sleepModeMutex);
		return 1;
	}
	else
		return -EINTR;
}

static ssize_t sysfsSleepStore(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	static const uint8_t cmdGoSleep[] = {CMD_PWR_CTL, 0x00, PWR_CTL_SLEEP_MODE};
	int goToSleepVal;
	bool goToWake;
	uint8_t dummy;

	sscanf(buf, "%d", &goToSleepVal);
	goToWake = !goToSleepVal;	/* convert to bool of proper polarity */

	if (!mutex_lock_interruptible(&sleepModeMutex)) {
		if ((chipAwake && goToWake) || (!chipAwake && !goToWake))
			LOGE("duplicate request to %s chip\n", goToWake ? "wake" : "sleep");
		else if (goToWake) {
			i2cReadNoReadyCheck(BUF_QUERY, &dummy, sizeof(dummy));
			enable_irq(gl_ts->client->irq);
			LOGI("touch is going to wake!\n\n");
		} else {
			disable_irq(gl_ts->client->irq);
			i2cWriteNoReadyCheck(BUF_COMMAND, cmdGoSleep, sizeof(cmdGoSleep));
			LOGI("touch is going to sleep...\n\n");
		}
		chipAwake = goToWake;
		mutex_unlock(&sleepModeMutex);
		return count;
	} else
		return -EINTR;
}


static DEVICE_ATTR(status, S_IRUGO|S_IWUSR|S_IWGRP, sysfsStatusShow, sysfsStatusStore);
static DEVICE_ATTR(version, S_IRUGO|S_IWUSR|S_IWGRP, sysfsVersionShow, sysfsVersionStore);
static DEVICE_ATTR(sleep, S_IRUGO|S_IWUSR|S_IWGRP, sysfsSleepShow, sysfsSleepStore);

static struct attribute *it7260_attrstatus[] = {
	&dev_attr_status.attr,
	&dev_attr_version.attr,
	&dev_attr_sleep.attr,
	NULL
};

static const struct attribute_group it7260_attrstatus_group = {
	.attrs = it7260_attrstatus,
};

static DEVICE_ATTR(calibration, S_IRUGO|S_IWUSR|S_IWGRP, sysfsCalibrationShow, sysfsCalibrationStore);
static DEVICE_ATTR(upgrade, S_IRUGO|S_IWUSR|S_IWGRP, sysfsUpgradeShow, sysfsUpgradeStore);
static DEVICE_ATTR(point, S_IRUGO|S_IWUSR|S_IWGRP, sysfsPointShow, sysfsPointStore);


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

	LOGI("sent calibration command -> %d\n", chipSendCalibrationCmd(autoTuneEnabled));
	waitDeviceReady(true, true);
	i2cReadNoReadyCheck(BUF_RESPONSE, resp, sizeof(resp));
	chipFirmwareReinitialize(CMD_FIRMWARE_REINIT_C);
}

void sendCalibrationCmd(void)
{
	chipExternalCalibration(false);
}
EXPORT_SYMBOL(sendCalibrationCmd);

static void readFingerData(uint16_t *xP, uint16_t *yP, uint8_t *pressureP, const struct FingerData *fd)
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

static void readTouchDataPoint(void)
{
	struct PointData pointData;
	uint8_t devStatus;
	uint8_t pressure = FD_PRESSURE_NONE;
	uint16_t x, y;

	/* verify there is point data to read & it is readable and valid */
	i2cReadNoReadyCheck(BUF_QUERY, &devStatus, sizeof(devStatus));
	if (!((devStatus & PT_INFO_BITS) & PT_INFO_YES)) {
		LOGE("readTouchDataPoint() called when no data available (0x%02X)\n", devStatus);
		return;
	}
	if (!i2cReadNoReadyCheck(BUF_POINT_INFO, (void*)&pointData, sizeof(pointData))) {
		LOGE("readTouchDataPoint() failed to read point data buffer\n");
		return;
	}
	if ((pointData.flags & PD_FLAGS_DATA_TYPE_BITS) != PD_FLAGS_DATA_TYPE_TOUCH) {
		LOGE("readTouchDataPoint() dropping non-point data of type 0x%02X\n", pointData.flags);
		return;
	}

	if ((pointData.flags & PD_FLAGS_HAVE_FINGERS) & 1)
		readFingerData(&x, &y, &pressure, pointData.fd);

	if (pressure >= FD_PRESSURE_LIGHT) {

		if (!hadFingerDown)
			hadFingerDown = true;

		readFingerData(&x, &y, &pressure, pointData.fd);

		input_report_abs(gl_ts->input_dev, ABS_X, x);
		input_report_abs(gl_ts->input_dev, ABS_Y, y);
		input_report_key(gl_ts->input_dev, BTN_TOUCH, 1);
		input_sync(gl_ts->input_dev);

	} else if (hadFingerDown) {
		hadFingerDown = false;

		input_report_key(gl_ts->input_dev, BTN_TOUCH, 0);
		input_sync(gl_ts->input_dev);
	}

}

static irqreturn_t IT7260_ts_threaded_handler(int irq, void *devid)
{
	smp_rmb();
	if (isDeviceSleeping) {
		smp_wmb();
	} else {
		readTouchDataPoint();
	}

	return IRQ_HANDLED;
}

static bool chipIdentifyIT7260(void)
{
	static const uint8_t cmdIdent[] = {CMD_IDENT_CHIP};
	static const uint8_t expectedID[] = {0x0A, 'I', 'T', 'E', '7', '2', '6', '0'};
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
	pr_info("chipIdentifyIT7260 read id: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
		chipID[0], chipID[1], chipID[2], chipID[3], chipID[4],
		chipID[5], chipID[6], chipID[7], chipID[8], chipID[9]);

	if (memcmp(chipID, expectedID, sizeof(expectedID)))
		return false;

	if (chipID[8] == '5' && chipID[9] == '6')
		LOGI("rev BX3 found\n");
	else if (chipID[8] == '6' && chipID[9] == '6')
		LOGI("rev BX4 found\n");
	else
		LOGI("unknown revision (0x%02X 0x%02X) found\n", chipID[8], chipID[9]);

	return true;
}

static int IT7260_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	static const uint8_t cmdStart[] = {CMD_UNKNOWN_7};
	struct IT7260_i2c_platform_data *pdata;
	uint8_t rsp[2];
	int ret = -1;

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
	pdata = client->dev.platform_data;

	if (sysfs_create_group(&(client->dev.kobj), &it7260_attrstatus_group)) {
		dev_err(&client->dev, "failed to register sysfs #1\n");
		goto err_sysfs_grp_create_1;
	}

	if (!chipIdentifyIT7260()) {
		LOGI ("chipIdentifyIT7260 FAIL");
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
	set_bit(KEY_WAKEUP,input_dev->keybit);
	set_bit(KEY_POWER,input_dev->keybit);
	input_set_abs_params(input_dev, ABS_X, 0, SCREEN_X_RESOLUTION, 0, 0);
	input_set_abs_params(input_dev, ABS_Y, 0, SCREEN_Y_RESOLUTION, 0, 0);

	if (input_register_device(input_dev)) {
		LOGE("failed to register input device\n");
		goto err_input_register;
	}

	if (request_threaded_irq(client->irq, NULL, IT7260_ts_threaded_handler, IRQF_TRIGGER_LOW | IRQF_ONESHOT, client->name, gl_ts)) {
		dev_err(&client->dev, "request_irq failed\n");
		goto err_irq_reg;
	}

	if (sysfs_create_group(&(client->dev.kobj), &it7260_attr_group)) {
		dev_err(&client->dev, "failed to register sysfs #2\n");
		goto err_sysfs_grp_create_2;
	}
	
	devicePresent = true;

	i2cWriteNoReadyCheck(BUF_COMMAND, cmdStart, sizeof(cmdStart));
	mdelay(10);
	i2cReadNoReadyCheck(BUF_RESPONSE, rsp, sizeof(rsp));
	mdelay(10);

	return 0;

err_sysfs_grp_create_2:
	free_irq(client->irq, gl_ts);

err_irq_reg:
	input_unregister_device(input_dev);
	input_dev = NULL;

err_input_register:
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
	devicePresent = false;
	return 0;
}

static const struct i2c_device_id IT7260_ts_id[] = {
	{ DEVICE_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, IT7260_ts_id);

static const struct of_device_id IT7260_match_table[] = {
	{ .compatible = "ITE,IT7260_ts",},
	{},
};

static int IT7260_ts_resume(struct i2c_client *i2cdev)
{
	isDeviceSuspend	= false;
    return 0;
}

static int IT7260_ts_suspend(struct i2c_client *i2cdev, pm_message_t pmesg)
{
	isDeviceSuspend = true;
    return 0;
}

static struct i2c_driver IT7260_ts_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = DEVICE_NAME,
		.of_match_table = IT7260_match_table,
	},
	.probe = IT7260_ts_probe,
	.remove = IT7260_ts_remove,
	.id_table = IT7260_ts_id,
	.resume   = IT7260_ts_resume,
	.suspend = IT7260_ts_suspend,
};

module_i2c_driver(IT7260_ts_driver);

MODULE_DESCRIPTION("IT7260 Touchscreen Driver");
MODULE_LICENSE("GPL v2");
