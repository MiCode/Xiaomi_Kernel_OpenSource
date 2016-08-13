/* drivers/input/touchscreen/it7258_ts_i2c.c
 *
 * Copyright (C) 2014 ITE Tech. Inc.
 * Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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
#include <linux/string.h>

#define MAX_BUFFER_SIZE			144
#define DEVICE_NAME			"it7260"
#define SCREEN_X_RESOLUTION		320
#define SCREEN_Y_RESOLUTION		320
#define DEBUGFS_DIR_NAME		"ts_debug"
#define FW_NAME				"it7260_fw.bin"
#define CFG_NAME			"it7260_cfg.bin"
#define VER_BUFFER_SIZE			4
#define IT_FW_CHECK(x, y) \
	(((x)[0] < (y)->data[8]) || ((x)[1] < (y)->data[9]) || \
	((x)[2] < (y)->data[10]) || ((x)[3] < (y)->data[11]))
#define IT_CFG_CHECK(x, y) \
	(((x)[0] < (y)->data[(y)->size - 8]) || \
	((x)[1] < (y)->data[(y)->size - 7]) || \
	((x)[2] < (y)->data[(y)->size - 6]) || \
	((x)[3] < (y)->data[(y)->size - 5]))
#define IT7260_COORDS_ARR_SIZE		4

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
#define SUB_CMD_READ_FIRMWARE_VERSION	0x00
#define SUB_CMD_READ_CONFIG_VERSION	0x06
#define VERSION_LENGTH			10
/* subcommand is zero, next byte is power mode */
#define CMD_PWR_CTL			0x04
/* active mode */
#define PWR_CTL_ACTIVE_MODE		0x00
/* idle mode */
#define PWR_CTL_LOW_POWER_MODE		0x01
/* sleep mode */
#define PWR_CTL_SLEEP_MODE		0x02
#define WAIT_CHANGE_MODE		20
/* command is not documented in the datasheet v1.0.0.7 */
#define CMD_UNKNOWN_7			0x07
#define CMD_FIRMWARE_REINIT_C		0x0C
/* needs to be followed by 4 bytes of zeroes */
#define CMD_CALIBRATE			0x13
#define CMD_FIRMWARE_UPGRADE		0x60
#define SUB_CMD_ENTER_FW_UPGRADE_MODE	0x00
#define SUB_CMD_EXIT_FW_UPGRADE_MODE	0x80
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
#define DEVICE_READY_COUNT_MAX		500
#define DEVICE_READY_COUNT_20		20
#define IT_I2C_WAIT_10MS		10
#define IT_I2C_READ_RET			2
#define IT_I2C_WRITE_RET		1

/* result of reading with BUF_QUERY bits */
#define CMD_STATUS_BITS			0x07
#define CMD_STATUS_DONE			0x00
#define CMD_STATUS_BUSY			0x01
#define CMD_STATUS_ERROR		0x02
#define CMD_STATUS_NO_CONN		0x07
#define PT_INFO_BITS			0xF8
#define PT_INFO_YES			0x80

#define PD_FLAGS_DATA_TYPE_BITS		0xF0
/* other types (like chip-detected gestures) exist but we do not care */
#define PD_FLAGS_DATA_TYPE_TOUCH	0x00
/* a bit for each finger data that is valid (from lsb to msb) */
#define PD_FLAGS_HAVE_FINGERS		0x07
#define PD_PALM_FLAG_BIT		0x01
#define FD_PRESSURE_BITS		0x0F
#define FD_PRESSURE_NONE		0x00
#define FD_PRESSURE_LIGHT		0x02

#define IT_VTG_MIN_UV		1800000
#define IT_VTG_MAX_UV		1800000
#define IT_ACTIVE_LOAD_UA	15000
#define IT_I2C_VTG_MIN_UV	2600000
#define IT_I2C_VTG_MAX_UV	3300000
#define IT_I2C_ACTIVE_LOAD_UA	10000
#define DELAY_VTG_REG_EN	170

#define PINCTRL_STATE_ACTIVE	"pmx_ts_active"
#define PINCTRL_STATE_SUSPEND	"pmx_ts_suspend"
#define PINCTRL_STATE_RELEASE	"pmx_ts_release"

struct finger_data {
	uint8_t xLo;
	uint8_t hi;
	uint8_t yLo;
	uint8_t pressure;
}  __packed;

struct point_data {
	uint8_t flags;
	uint8_t palm;
	struct finger_data fd[3];
}  __packed;

struct it7260_ts_platform_data {
	u32 irq_gpio;
	u32 irq_gpio_flags;
	u32 reset_gpio;
	u32 reset_gpio_flags;
	bool wakeup;
	bool palm_detect_en;
	u16 palm_detect_keycode;
	const char *fw_name;
	const char *cfg_name;
	unsigned int panel_minx;
	unsigned int panel_miny;
	unsigned int panel_maxx;
	unsigned int panel_maxy;
	unsigned int disp_minx;
	unsigned int disp_miny;
	unsigned int disp_maxx;
	unsigned int disp_maxy;
	unsigned num_of_fingers;
	unsigned int reset_delay;
	unsigned int avdd_lpm_cur;
	bool low_reset;
};

struct it7260_ts_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	const struct it7260_ts_platform_data *pdata;
	struct regulator *vdd;
	struct regulator *avdd;
	bool device_needs_wakeup;
	bool suspended;
	bool fw_upgrade_result;
	bool cfg_upgrade_result;
	bool fw_cfg_uploading;
	struct work_struct work_pm_relax;
	bool calibration_success;
	bool had_finger_down;
	char fw_name[MAX_BUFFER_SIZE];
	char cfg_name[MAX_BUFFER_SIZE];
	struct mutex fw_cfg_mutex;
	u8 fw_ver[VER_BUFFER_SIZE];
	u8 cfg_ver[VER_BUFFER_SIZE];
#ifdef CONFIG_FB
	struct notifier_block fb_notif;
#endif
	struct dentry *dir;
	struct pinctrl *ts_pinctrl;
	struct pinctrl_state *pinctrl_state_active;
	struct pinctrl_state *pinctrl_state_suspend;
	struct pinctrl_state *pinctrl_state_release;
};

/* Function declarations */
static int fb_notifier_callback(struct notifier_block *self,
			unsigned long event, void *data);
static int it7260_ts_resume(struct device *dev);
static int it7260_ts_suspend(struct device *dev);

static int it7260_debug_suspend_set(void *_data, u64 val)
{
	struct it7260_ts_data *ts_data = _data;

	if (val)
		it7260_ts_suspend(&ts_data->client->dev);
	else
		it7260_ts_resume(&ts_data->client->dev);

	return 0;
}

static int it7260_debug_suspend_get(void *_data, u64 *val)
{
	struct it7260_ts_data *ts_data = _data;

	mutex_lock(&ts_data->input_dev->mutex);
	*val = ts_data->suspended;
	mutex_lock(&ts_data->input_dev->mutex);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_suspend_fops, it7260_debug_suspend_get,
				it7260_debug_suspend_set, "%lld\n");

/* internal use func - does not make sure chip is ready before read */
static int it7260_i2c_read_no_ready_check(struct it7260_ts_data *ts_data,
			uint8_t buf_index, uint8_t *buffer, uint16_t buf_len)
{
	int ret;
	struct i2c_msg msgs[2] = {
		{
			.addr = ts_data->client->addr,
			.flags = I2C_M_NOSTART,
			.len = 1,
			.buf = &buf_index
		},
		{
			.addr = ts_data->client->addr,
			.flags = I2C_M_RD,
			.len = buf_len,
			.buf = buffer
		}
	};

	memset(buffer, 0xFF, buf_len);

	ret = i2c_transfer(ts_data->client->adapter, msgs, 2);
	if (ret < 0)
		dev_err(&ts_data->client->dev, "i2c read failed %d\n", ret);

	return ret;
}

static int it7260_i2c_write_no_ready_check(struct it7260_ts_data *ts_data,
		uint8_t buf_index, const uint8_t *buffer, uint16_t buf_len)
{
	uint8_t txbuf[257];
	int ret;
	struct i2c_msg msg = {
		.addr = ts_data->client->addr,
		.flags = 0,
		.len = buf_len + 1,
		.buf = txbuf
	};

	/* just to be careful */
	if (buf_len > sizeof(txbuf) - 1) {
		dev_err(&ts_data->client->dev, "buf length is out of limit\n");
		return false;
	}

	txbuf[0] = buf_index;
	memcpy(txbuf + 1, buffer, buf_len);

	ret = i2c_transfer(ts_data->client->adapter, &msg, 1);
	if (ret < 0)
		dev_err(&ts_data->client->dev, "i2c write failed %d\n", ret);

	return ret;
}

/*
 * Device is apparently always ready for I2C communication but not for
 * actual register reads/writes. This function checks if it is ready
 * for that too. The results of this call often were ignored.
 * If forever is set to TRUE, then check the device's status until it
 * becomes ready with 500 retries at max. Otherwise retry 25 times only.
 * If slowly is set to TRUE, then add sleep of 50 ms in each retry,
 * otherwise don't sleep.
 */
static int it7260_wait_device_ready(struct it7260_ts_data *ts_data,
					bool forever, bool slowly)
{
	uint8_t query;
	uint32_t count = DEVICE_READY_COUNT_20;
	int ret;

	if (ts_data->fw_cfg_uploading || forever)
		count = DEVICE_READY_COUNT_MAX;

	do {
		ret = it7260_i2c_read_no_ready_check(ts_data, BUF_QUERY, &query,
						sizeof(query));
		if (ret < 0 && ((query & CMD_STATUS_BITS)
						== CMD_STATUS_NO_CONN))
			continue;

		if ((query & CMD_STATUS_BITS) == CMD_STATUS_DONE)
			break;

		query = CMD_STATUS_BUSY;
		if (slowly)
			msleep(IT_I2C_WAIT_10MS);
	} while (--count);

	return ((!(query & CMD_STATUS_BITS)) ? 0 : -ENODEV);
}

static int it7260_i2c_read(struct it7260_ts_data *ts_data, uint8_t buf_index,
				uint8_t *buffer, uint16_t buf_len)
{
	int ret;

	ret = it7260_wait_device_ready(ts_data, false, false);
	if (ret < 0)
		return ret;

	return it7260_i2c_read_no_ready_check(ts_data, buf_index,
					buffer, buf_len);
}

static int it7260_i2c_write(struct it7260_ts_data *ts_data, uint8_t buf_index,
			const uint8_t *buffer, uint16_t buf_len)
{
	int ret;

	ret = it7260_wait_device_ready(ts_data, false, false);
	if (ret < 0)
		return ret;

	return it7260_i2c_write_no_ready_check(ts_data, buf_index,
					buffer, buf_len);
}

static int it7260_firmware_reinitialize(struct it7260_ts_data *ts_data,
						u8 command)
{
	uint8_t cmd[] = {command};
	uint8_t rsp[2];
	int ret;

	ret = it7260_i2c_write(ts_data, BUF_COMMAND, cmd, sizeof(cmd));
	if (ret != IT_I2C_WRITE_RET) {
		dev_err(&ts_data->client->dev,
			"failed to write fw reinit command %d\n", ret);
		return ret;
	}

	ret = it7260_i2c_read(ts_data, BUF_RESPONSE, rsp, sizeof(rsp));
	if (ret != IT_I2C_READ_RET) {
		dev_err(&ts_data->client->dev,
			"failed to read any response from chip %d\n", ret);
		return ret;
	}

	/* a reply of two zero bytes signifies success */
	if (rsp[0] == 0 && rsp[1] == 0)
		return 0;
	else
		return -EIO;
}

static int it7260_enter_exit_fw_ugrade_mode(struct it7260_ts_data *ts_data,
							bool enter)
{
	uint8_t cmd[] = {CMD_FIRMWARE_UPGRADE, 0, 'I', 'T', '7', '2',
						'6', '0', 0x55, 0xAA};
	uint8_t resp[2];
	int ret;

	cmd[1] = enter ? SUB_CMD_ENTER_FW_UPGRADE_MODE :
				SUB_CMD_EXIT_FW_UPGRADE_MODE;

	ret = it7260_i2c_write(ts_data, BUF_COMMAND, cmd, sizeof(cmd));
	if (ret != IT_I2C_WRITE_RET) {
		dev_err(&ts_data->client->dev,
			"failed to write CMD_FIRMWARE_UPGRADE %d\n", ret);
		return ret;
	}

	ret = it7260_i2c_read(ts_data, BUF_RESPONSE, resp, sizeof(resp));
	if (ret != IT_I2C_READ_RET) {
		dev_err(&ts_data->client->dev,
			"failed to read any response from chip %d\n", ret);
		return ret;
	}

	/* a reply of two zero bytes signifies success */
	if (resp[0] == 0 && resp[1] == 0)
		return 0;
	else
		return -EIO;
}

static int it7260_set_start_offset(struct it7260_ts_data *ts_data,
					uint16_t offset)
{
	uint8_t cmd[] = {CMD_SET_START_OFFSET, 0, ((uint8_t)(offset)),
				((uint8_t)((offset) >> 8))};
	uint8_t resp[2];
	int ret;

	ret = it7260_i2c_write(ts_data, BUF_COMMAND, cmd, 4);
	if (ret != IT_I2C_WRITE_RET) {
		dev_err(&ts_data->client->dev,
			"failed to write CMD_SET_START_OFFSET %d\n", ret);
		return ret;
	}


	ret = it7260_i2c_read(ts_data, BUF_RESPONSE, resp, sizeof(resp));
	if (ret != IT_I2C_READ_RET) {
		dev_err(&ts_data->client->dev,
			"failed to read any response from chip %d\n", ret);
		return ret;
	}

	/* a reply of two zero bytes signifies success */
	if (resp[0] == 0 && resp[1] == 0)
		return 0;
	else
		return -EIO;
}


/* write fw_length bytes from fw_data at chip offset wr_start_offset */
static int it7260_fw_flash_write_verify(struct it7260_ts_data *ts_data,
			unsigned int fw_length,	const uint8_t *fw_data,
			uint16_t wr_start_offset)
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
			it7260_set_start_offset(ts_data,
					wr_start_offset + cur_data_off);
			it7260_i2c_write(ts_data, BUF_COMMAND, cmd_write,
					cur_wr_size + 2);

			/* set offset and read the data back */
			it7260_set_start_offset(ts_data,
					wr_start_offset + cur_data_off);
			it7260_i2c_write(ts_data, BUF_COMMAND, cmd_read,
					sizeof(cmd_read));
			it7260_i2c_read(ts_data, BUF_RESPONSE, buf_read,
								cur_wr_size);

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
			dev_err(&ts_data->client->dev,
				"write of data offset %u failed on try %u at byte %u/%u\n",
				cur_data_off, retries, i, cur_wr_size);
			return -EIO;
		}
	}

	return 0;
}

/*
 * this code to get versions from the chip via i2c transactions, and save
 * them in driver data structure.
 */
static void it7260_get_chip_versions(struct it7260_ts_data *ts_data)
{
	static const u8 cmd_read_fw_ver[] = {CMD_READ_VERSIONS,
						SUB_CMD_READ_FIRMWARE_VERSION};
	static const u8 cmd_read_cfg_ver[] = {CMD_READ_VERSIONS,
						SUB_CMD_READ_CONFIG_VERSION};
	u8 ver_fw[VERSION_LENGTH], ver_cfg[VERSION_LENGTH];
	int ret;

	ret = it7260_i2c_write(ts_data, BUF_COMMAND, cmd_read_fw_ver,
					sizeof(cmd_read_fw_ver));
	if (ret == IT_I2C_WRITE_RET) {
		/*
		 * Sometimes, the controller may not respond immediately after
		 * writing the command, so wait for device to get ready.
		 */
		ret = it7260_wait_device_ready(ts_data, true, false);
		if (ret < 0)
			dev_err(&ts_data->client->dev,
				"failed to read chip status %d\n", ret);

		ret = it7260_i2c_read_no_ready_check(ts_data, BUF_RESPONSE,
					ver_fw, VERSION_LENGTH);
		if (ret == IT_I2C_READ_RET)
			memcpy(ts_data->fw_ver, ver_fw + (5 * sizeof(u8)),
					VER_BUFFER_SIZE * sizeof(u8));
		else
			dev_err(&ts_data->client->dev,
				"failed to read fw-ver from chip %d\n", ret);
	} else {
		dev_err(&ts_data->client->dev,
				"failed to write fw-read command %d\n", ret);
	}

	ret = it7260_i2c_write(ts_data, BUF_COMMAND, cmd_read_cfg_ver,
					sizeof(cmd_read_cfg_ver));
	if (ret == IT_I2C_WRITE_RET) {
		/*
		 * Sometimes, the controller may not respond immediately after
		 * writing the command, so wait for device to get ready.
		 */
		ret = it7260_wait_device_ready(ts_data, true, false);
		if (ret < 0)
			dev_err(&ts_data->client->dev,
				"failed to read chip status %d\n", ret);

		ret = it7260_i2c_read_no_ready_check(ts_data, BUF_RESPONSE,
					ver_cfg, VERSION_LENGTH);
		if (ret == IT_I2C_READ_RET)
			memcpy(ts_data->cfg_ver, ver_cfg + (1 * sizeof(u8)),
					VER_BUFFER_SIZE * sizeof(u8));
		else
			dev_err(&ts_data->client->dev,
				"failed to read cfg-ver from chip %d\n", ret);
	} else {
		dev_err(&ts_data->client->dev,
				"failed to write cfg-read command %d\n", ret);
	}

	dev_info(&ts_data->client->dev, "Current fw{%X.%X.%X.%X} cfg{%X.%X.%X.%X}\n",
		ts_data->fw_ver[0], ts_data->fw_ver[1], ts_data->fw_ver[2],
		ts_data->fw_ver[3], ts_data->cfg_ver[0], ts_data->cfg_ver[1],
		ts_data->cfg_ver[2], ts_data->cfg_ver[3]);
}

static int it7260_cfg_upload(struct it7260_ts_data *ts_data, bool force)
{
	const struct firmware *cfg = NULL;
	int ret;
	bool cfg_upgrade = false;
	struct device *dev = &ts_data->client->dev;

	ret = request_firmware(&cfg, ts_data->cfg_name, dev);
	if (ret) {
		dev_err(dev, "failed to get config data %s for it7260 %d\n",
					ts_data->cfg_name, ret);
		return ret;
	}

	/*
	 * This compares the cfg version number from chip and the cfg
	 * data file. IT flashes only when version of cfg data file is
	 * greater than that of chip or if it is set for force cfg upgrade.
	 */
	if (force)
		cfg_upgrade = true;
	else if (IT_CFG_CHECK(ts_data->cfg_ver, cfg))
		cfg_upgrade = true;

	if (!cfg_upgrade) {
		dev_err(dev, "CFG upgrade not required ...\n");
		dev_info(dev,
			"Chip CFG : %X.%X.%X.%X Binary CFG : %X.%X.%X.%X\n",
			ts_data->cfg_ver[0], ts_data->cfg_ver[1],
			ts_data->cfg_ver[2], ts_data->cfg_ver[3],
			cfg->data[cfg->size - 8], cfg->data[cfg->size - 7],
			cfg->data[cfg->size - 6], cfg->data[cfg->size - 5]);
		ret = -EFAULT;
		goto out;
	} else {
		dev_info(dev, "Config upgrading...\n");

		disable_irq(ts_data->client->irq);
		/* enter cfg upload mode */
		ret = it7260_enter_exit_fw_ugrade_mode(ts_data, true);
		if (ret < 0) {
			dev_err(dev, "Can't enter cfg upgrade mode %d\n", ret);
			enable_irq(ts_data->client->irq);
			goto out;
		}
		/* flash config data if requested */
		ret  = it7260_fw_flash_write_verify(ts_data, cfg->size,
					cfg->data, CHIP_FLASH_SIZE - cfg->size);
		if (ret < 0) {
			dev_err(dev,
				"failed to upgrade touch cfg data %d\n", ret);
			ret = it7260_enter_exit_fw_ugrade_mode(ts_data, false);
			if (ret < 0)
				dev_err(dev,
					"Can't exit cfg upgrade mode%d\n", ret);

			ret = it7260_firmware_reinitialize(ts_data,
						CMD_FIRMWARE_REINIT_6F);
			if (ret < 0)
				dev_err(dev, "Can't reinit cfg %d\n", ret);

			ret = -EIO;
			enable_irq(ts_data->client->irq);
			goto out;
		} else {
			memcpy(ts_data->cfg_ver, cfg->data +
					(cfg->size - 8 * sizeof(u8)),
					VER_BUFFER_SIZE * sizeof(u8));
			dev_info(dev, "CFG upgrade is success. New cfg ver: %X.%X.%X.%X\n",
				ts_data->cfg_ver[0], ts_data->cfg_ver[1],
				ts_data->cfg_ver[2], ts_data->cfg_ver[3]);

		}
		enable_irq(ts_data->client->irq);
	}

out:
	release_firmware(cfg);

	return ret;
}

static int it7260_fw_upload(struct it7260_ts_data *ts_data, bool force)
{
	const struct firmware *fw = NULL;
	int ret;
	bool fw_upgrade = false;
	struct device *dev = &ts_data->client->dev;

	ret = request_firmware(&fw, ts_data->fw_name, dev);
	if (ret) {
		dev_err(dev, "failed to get firmware %s for it7260 %d\n",
					ts_data->fw_name, ret);
		return ret;
	}

	/*
	 * This compares the fw version number from chip and the fw data
	 * file. It flashes only when version of fw data file is greater
	 * than that of chip or it it is set for force fw upgrade.
	 */
	if (force)
		fw_upgrade = true;
	else if (IT_FW_CHECK(ts_data->fw_ver, fw))
		fw_upgrade = true;

	if (!fw_upgrade) {
		dev_err(dev, "FW upgrade not required ...\n");
		dev_info(dev, "Chip FW : %X.%X.%X.%X Binary FW : %X.%X.%X.%X\n",
			ts_data->fw_ver[0], ts_data->fw_ver[1],
			ts_data->fw_ver[2], ts_data->fw_ver[3],
			fw->data[8], fw->data[9], fw->data[10], fw->data[11]);
		ret = -EFAULT;
		goto out;
	} else {
		dev_info(dev, "Firmware upgrading...\n");

		disable_irq(ts_data->client->irq);
		/* enter fw upload mode */
		ret = it7260_enter_exit_fw_ugrade_mode(ts_data, true);
		if (ret < 0) {
			dev_err(dev, "Can't enter fw upgrade mode %d\n", ret);
			enable_irq(ts_data->client->irq);
			goto out;
		}
		/* flash the firmware if requested */
		ret = it7260_fw_flash_write_verify(ts_data, fw->size,
							fw->data, 0);
		if (ret < 0) {
			dev_err(dev,
				"failed to upgrade touch firmware %d\n", ret);
			ret = it7260_enter_exit_fw_ugrade_mode(ts_data, false);
			if (ret < 0)
				dev_err(dev,
					"Can't exit fw upgrade mode %d\n", ret);

			ret = it7260_firmware_reinitialize(ts_data,
						CMD_FIRMWARE_REINIT_6F);
			if (ret < 0)
				dev_err(dev, "Can't reinit firmware %d\n", ret);

			ret = -EIO;
			enable_irq(ts_data->client->irq);
			goto out;
		} else {
			memcpy(ts_data->fw_ver, fw->data + (8 * sizeof(u8)),
					VER_BUFFER_SIZE * sizeof(u8));
			dev_info(dev, "FW upgrade is success. New fw ver: %X.%X.%X.%X\n",
					ts_data->fw_ver[0], ts_data->fw_ver[1],
					ts_data->fw_ver[2], ts_data->fw_ver[3]);
		}
		enable_irq(ts_data->client->irq);
	}

out:
	release_firmware(fw);

	return ret;
}

static int it7260_ts_chip_low_power_mode(struct it7260_ts_data *ts_data,
					const u8 sleep_type)
{
	const uint8_t cmd_sleep[] = {CMD_PWR_CTL, 0x00, sleep_type};
	uint8_t dummy;

	if (sleep_type)
		it7260_i2c_write_no_ready_check(ts_data, BUF_COMMAND, cmd_sleep,
					sizeof(cmd_sleep));
	else
		it7260_i2c_read_no_ready_check(ts_data, BUF_QUERY, &dummy,
						sizeof(dummy));

	msleep(WAIT_CHANGE_MODE);
	return 0;
}

static ssize_t sysfs_fw_upgrade_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct it7260_ts_data *ts_data = dev_get_drvdata(dev);
	int mode = 0, ret;

	if (ts_data->suspended) {
		dev_err(dev, "Device is suspended, can't flash fw!!!\n");
		return -EBUSY;
	}

	ret = kstrtoint(buf, 10, &mode);
	if (ret) {
		dev_err(dev, "failed to read input for sysfs\n");
		return -EINVAL;
	}

	mutex_lock(&ts_data->fw_cfg_mutex);
	if (mode == 1) {
		ts_data->fw_cfg_uploading = true;
		ret = it7260_fw_upload(ts_data, false);
		if (ret) {
			dev_err(dev, "Failed to flash fw: %d", ret);
			ts_data->fw_upgrade_result = false;
		 } else {
			ts_data->fw_upgrade_result = true;
		}
		ts_data->fw_cfg_uploading = false;
	}
	mutex_unlock(&ts_data->fw_cfg_mutex);

	return count;
}

static ssize_t sysfs_cfg_upgrade_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct it7260_ts_data *ts_data = dev_get_drvdata(dev);
	int mode = 0, ret;

	if (ts_data->suspended) {
		dev_err(dev, "Device is suspended, can't flash cfg!!!\n");
		return -EBUSY;
	}

	ret = kstrtoint(buf, 10, &mode);
	if (ret) {
		dev_err(dev, "failed to read input for sysfs\n");
		return -EINVAL;
	}

	mutex_lock(&ts_data->fw_cfg_mutex);
	if (mode == 1) {
		ts_data->fw_cfg_uploading = true;
		ret = it7260_cfg_upload(ts_data, false);
		if (ret) {
			dev_err(dev, "Failed to flash cfg: %d", ret);
			ts_data->cfg_upgrade_result = false;
		} else {
			ts_data->cfg_upgrade_result = true;
		}
		ts_data->fw_cfg_uploading = false;
	}
	mutex_unlock(&ts_data->fw_cfg_mutex);

	return count;
}

static ssize_t sysfs_fw_upgrade_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct it7260_ts_data *ts_data = dev_get_drvdata(dev);

	return scnprintf(buf, MAX_BUFFER_SIZE, "%d\n",
				ts_data->fw_upgrade_result);
}

static ssize_t sysfs_cfg_upgrade_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct it7260_ts_data *ts_data = dev_get_drvdata(dev);

	return scnprintf(buf, MAX_BUFFER_SIZE, "%d\n",
				ts_data->cfg_upgrade_result);
}

static ssize_t sysfs_force_fw_upgrade_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct it7260_ts_data *ts_data = dev_get_drvdata(dev);
	int mode = 0, ret;

	if (ts_data->suspended) {
		dev_err(dev, "Device is suspended, can't flash fw!!!\n");
		return -EBUSY;
	}

	ret = kstrtoint(buf, 10, &mode);
	if (ret) {
		dev_err(dev, "failed to read input for sysfs\n");
		return -EINVAL;
	}

	mutex_lock(&ts_data->fw_cfg_mutex);
	if (mode == 1) {
		ts_data->fw_cfg_uploading = true;
		ret = it7260_fw_upload(ts_data, true);
		if (ret) {
			dev_err(dev, "Failed to force flash fw: %d", ret);
			ts_data->fw_upgrade_result = false;
		} else {
			ts_data->fw_upgrade_result = true;
		}
		ts_data->fw_cfg_uploading = false;
	}
	mutex_unlock(&ts_data->fw_cfg_mutex);

	return count;
}

static ssize_t sysfs_force_cfg_upgrade_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct it7260_ts_data *ts_data = dev_get_drvdata(dev);
	int mode = 0, ret;

	if (ts_data->suspended) {
		dev_err(dev, "Device is suspended, can't flash cfg!!!\n");
		return -EBUSY;
	}

	ret = kstrtoint(buf, 10, &mode);
	if (ret) {
		dev_err(dev, "failed to read input for sysfs\n");
		return -EINVAL;
	}

	mutex_lock(&ts_data->fw_cfg_mutex);
	if (mode == 1) {
		ts_data->fw_cfg_uploading = true;
		ret = it7260_cfg_upload(ts_data, true);
		if (ret) {
			dev_err(dev, "Failed to force flash cfg: %d", ret);
			ts_data->cfg_upgrade_result = false;
		} else {
			ts_data->cfg_upgrade_result = true;
		}
		ts_data->fw_cfg_uploading = false;
	}
	mutex_unlock(&ts_data->fw_cfg_mutex);

	return count;
}

static ssize_t sysfs_force_fw_upgrade_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct it7260_ts_data *ts_data = dev_get_drvdata(dev);

	return snprintf(buf, MAX_BUFFER_SIZE, "%d", ts_data->fw_upgrade_result);
}

static ssize_t sysfs_force_cfg_upgrade_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct it7260_ts_data *ts_data = dev_get_drvdata(dev);

	return snprintf(buf, MAX_BUFFER_SIZE, "%d",
				ts_data->cfg_upgrade_result);
}

static ssize_t sysfs_calibration_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct it7260_ts_data *ts_data = dev_get_drvdata(dev);

	return scnprintf(buf, MAX_BUFFER_SIZE, "%d\n",
				ts_data->calibration_success);
}

static int it7260_ts_send_calibration_cmd(struct it7260_ts_data *ts_data,
						bool auto_tune_on)
{
	uint8_t cmd_calibrate[] = {CMD_CALIBRATE, 0,
					auto_tune_on ? 1 : 0, 0, 0};

	return it7260_i2c_write(ts_data, BUF_COMMAND, cmd_calibrate,
					sizeof(cmd_calibrate));
}

static ssize_t sysfs_calibration_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct it7260_ts_data *ts_data = dev_get_drvdata(dev);
	uint8_t resp;
	int ret;

	ret = it7260_ts_send_calibration_cmd(ts_data, false);
	if (ret < 0) {
		dev_err(dev, "failed to send calibration command\n");
	} else {
		ret = it7260_i2c_read(ts_data, BUF_RESPONSE, &resp,
							sizeof(resp));
		if (ret == IT_I2C_READ_RET)
			ts_data->calibration_success = true;

		/*
		 * previous logic that was here never called
		 * it7260_firmware_reinitialize() due to checking a
		 * guaranteed-not-null value against null. We now
		 * call it. Hopefully this is OK
		 */
		if (!resp)
			dev_dbg(dev, "it7260_firmware_reinitialize-> %s\n",
				it7260_firmware_reinitialize(ts_data,
				CMD_FIRMWARE_REINIT_6F) ? "success" : "fail");
	}

	return count;
}

static ssize_t sysfs_point_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct it7260_ts_data *ts_data = dev_get_drvdata(dev);
	uint8_t pt_data[sizeof(struct point_data)];
	int readSuccess;
	ssize_t ret;

	readSuccess = it7260_i2c_read_no_ready_check(ts_data, BUF_POINT_INFO,
					pt_data, sizeof(pt_data));

	if (readSuccess == IT_I2C_READ_RET) {
		ret = scnprintf(buf, MAX_BUFFER_SIZE,
			"point_show read ret[%d]--point[%x][%x][%x][%x][%x][%x][%x][%x][%x][%x][%x][%x][%x][%x]\n",
			readSuccess, pt_data[0], pt_data[1],
			pt_data[2], pt_data[3], pt_data[4],
			pt_data[5], pt_data[6], pt_data[7],
			pt_data[8], pt_data[9], pt_data[10],
			pt_data[11], pt_data[12], pt_data[13]);
	} else {
		 ret = scnprintf(buf, MAX_BUFFER_SIZE,
			"failed to read point data\n");
	}
	dev_dbg(dev, "%s", buf);

	return ret;
}

static ssize_t sysfs_version_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct it7260_ts_data *ts_data = dev_get_drvdata(dev);

	return scnprintf(buf, MAX_BUFFER_SIZE,
			"fw{%X.%X.%X.%X} cfg{%X.%X.%X.%X}\n",
			ts_data->fw_ver[0], ts_data->fw_ver[1],
			ts_data->fw_ver[2], ts_data->fw_ver[3],
			ts_data->cfg_ver[0], ts_data->cfg_ver[1],
			ts_data->cfg_ver[2], ts_data->cfg_ver[3]);
}

static ssize_t sysfs_sleep_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct it7260_ts_data *ts_data = dev_get_drvdata(dev);
	/*
	 * The usefulness of this was questionable at best - we were at least
	 * leaking a byte of kernel data (by claiming to return a byte but not
	 * writing to buf. To fix this now we actually return the sleep status
	 */
	*buf = ts_data->suspended ? '1' : '0';

	return 1;
}

static ssize_t sysfs_sleep_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct it7260_ts_data *ts_data = dev_get_drvdata(dev);
	int go_to_sleep, ret;

	ret = kstrtoint(buf, 10, &go_to_sleep);

	/* (ts_data->suspended == true && goToSleepVal > 0) means
	 * device is already suspended and you want it to be in sleep,
	 * (ts_data->suspended == false && goToSleepVal == 0) means
	 * device is already active and you also want it to be active.
	 */
	if ((ts_data->suspended && go_to_sleep > 0) ||
			(!ts_data->suspended && go_to_sleep == 0))
		dev_err(dev, "duplicate request to %s chip\n",
			go_to_sleep ? "sleep" : "wake");
	else if (go_to_sleep) {
		disable_irq(ts_data->client->irq);
		it7260_ts_chip_low_power_mode(ts_data, PWR_CTL_SLEEP_MODE);
		dev_dbg(dev, "touch is going to sleep...\n");
	} else {
		it7260_ts_chip_low_power_mode(ts_data, PWR_CTL_ACTIVE_MODE);
		enable_irq(ts_data->client->irq);
		dev_dbg(dev, "touch is going to wake!\n");
	}
	ts_data->suspended = go_to_sleep;

	return count;
}

static ssize_t sysfs_cfg_name_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct it7260_ts_data *ts_data = dev_get_drvdata(dev);
	char *strptr;

	if (count >= MAX_BUFFER_SIZE) {
		dev_err(dev, "Input over %d chars long\n", MAX_BUFFER_SIZE);
		return -EINVAL;
	}

	strptr = strnstr(buf, ".bin", count);
	if (!strptr) {
		dev_err(dev, "Input is invalid cfg file\n");
		return -EINVAL;
	}

	strlcpy(ts_data->cfg_name, buf, count);

	return count;
}

static ssize_t sysfs_cfg_name_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct it7260_ts_data *ts_data = dev_get_drvdata(dev);

	if (strnlen(ts_data->cfg_name, MAX_BUFFER_SIZE) > 0)
		return scnprintf(buf, MAX_BUFFER_SIZE, "%s\n",
				ts_data->cfg_name);
	else
		return scnprintf(buf, MAX_BUFFER_SIZE,
			"No config file name given\n");
}

static ssize_t sysfs_fw_name_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct it7260_ts_data *ts_data = dev_get_drvdata(dev);
	char *strptr;

	if (count >= MAX_BUFFER_SIZE) {
		dev_err(dev, "Input over %d chars long\n", MAX_BUFFER_SIZE);
		return -EINVAL;
	}

	strptr = strnstr(buf, ".bin", count);
	if (!strptr) {
		dev_err(dev, "Input is invalid fw file\n");
		return -EINVAL;
	}

	strlcpy(ts_data->fw_name, buf, count);
	return count;
}

static ssize_t sysfs_fw_name_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct it7260_ts_data *ts_data = dev_get_drvdata(dev);

	if (strnlen(ts_data->fw_name, MAX_BUFFER_SIZE) > 0)
		return scnprintf(buf, MAX_BUFFER_SIZE, "%s\n",
			ts_data->fw_name);
	else
		return scnprintf(buf, MAX_BUFFER_SIZE,
			"No firmware file name given\n");
}

static DEVICE_ATTR(version, S_IRUGO | S_IWUSR,
			sysfs_version_show, NULL);
static DEVICE_ATTR(sleep, S_IRUGO | S_IWUSR,
			sysfs_sleep_show, sysfs_sleep_store);
static DEVICE_ATTR(calibration, S_IRUGO | S_IWUSR,
			sysfs_calibration_show, sysfs_calibration_store);
static DEVICE_ATTR(fw_update, S_IRUGO | S_IWUSR,
			sysfs_fw_upgrade_show, sysfs_fw_upgrade_store);
static DEVICE_ATTR(cfg_update, S_IRUGO | S_IWUSR,
			sysfs_cfg_upgrade_show, sysfs_cfg_upgrade_store);
static DEVICE_ATTR(point, S_IRUGO | S_IWUSR,
			sysfs_point_show, NULL);
static DEVICE_ATTR(fw_name, S_IRUGO | S_IWUSR,
			sysfs_fw_name_show, sysfs_fw_name_store);
static DEVICE_ATTR(cfg_name, S_IRUGO | S_IWUSR,
			sysfs_cfg_name_show, sysfs_cfg_name_store);
static DEVICE_ATTR(force_fw_update, S_IRUGO | S_IWUSR,
			sysfs_force_fw_upgrade_show,
			sysfs_force_fw_upgrade_store);
static DEVICE_ATTR(force_cfg_update, S_IRUGO | S_IWUSR,
			sysfs_force_cfg_upgrade_show,
			sysfs_force_cfg_upgrade_store);

static struct attribute *it7260_attributes[] = {
	&dev_attr_version.attr,
	&dev_attr_sleep.attr,
	&dev_attr_calibration.attr,
	&dev_attr_fw_update.attr,
	&dev_attr_cfg_update.attr,
	&dev_attr_point.attr,
	&dev_attr_fw_name.attr,
	&dev_attr_cfg_name.attr,
	&dev_attr_force_fw_update.attr,
	&dev_attr_force_cfg_update.attr,
	NULL
};

static const struct attribute_group it7260_attr_group = {
	.attrs = it7260_attributes,
};

static void it7260_ts_release_all(struct it7260_ts_data *ts_data)
{
	int finger;

	for (finger = 0; finger < ts_data->pdata->num_of_fingers; finger++) {
		input_mt_slot(ts_data->input_dev, finger);
		input_mt_report_slot_state(ts_data->input_dev,
				MT_TOOL_FINGER, 0);
	}

	input_report_key(ts_data->input_dev, BTN_TOUCH, 0);
	input_sync(ts_data->input_dev);
}

static irqreturn_t it7260_ts_threaded_handler(int irq, void *devid)
{
	struct point_data pt_data;
	struct it7260_ts_data *ts_data = devid;
	struct input_dev *input_dev = ts_data->input_dev;
	u8 dev_status, finger, touch_count = 0, finger_status;
	u8 pressure = FD_PRESSURE_NONE;
	u16 x, y;
	bool palm_detected;
	int ret;

	/* verify there is point data to read & it is readable and valid */
	ret = it7260_i2c_read_no_ready_check(ts_data, BUF_QUERY, &dev_status,
						sizeof(dev_status));
	if (ret == IT_I2C_READ_RET)
		if (!((dev_status & PT_INFO_BITS) & PT_INFO_YES))
			return IRQ_HANDLED;
	ret = it7260_i2c_read_no_ready_check(ts_data, BUF_POINT_INFO,
				(void *)&pt_data, sizeof(pt_data));
	if (ret != IT_I2C_READ_RET) {
		dev_err(&ts_data->client->dev,
			"failed to read point data buffer\n");
		return IRQ_HANDLED;
	}

	/* Check if controller moves from idle to active state */
	if ((pt_data.flags & PD_FLAGS_DATA_TYPE_BITS) !=
					PD_FLAGS_DATA_TYPE_TOUCH) {
		/*
		 * This code adds the touch-to-wake functionality to the ITE
		 * tech driver. When user puts a finger on touch controller in
		 * idle state, the controller moves to active state and driver
		 * sends the KEY_WAKEUP event to wake the device. The
		 * pm_stay_awake() call tells the pm core to stay awake until
		 * the CPU cores are up already. The schedule_work() call
		 * schedule a work that tells the pm core to relax once the CPU
		 * cores are up.
		 */
		if (ts_data->device_needs_wakeup) {
			pm_stay_awake(&ts_data->client->dev);
			input_report_key(input_dev, KEY_WAKEUP, 1);
			input_sync(input_dev);
			input_report_key(input_dev, KEY_WAKEUP, 0);
			input_sync(input_dev);
			schedule_work(&ts_data->work_pm_relax);
			return IRQ_HANDLED;
		}
	}

	palm_detected = pt_data.palm & PD_PALM_FLAG_BIT;
	if (palm_detected && ts_data->pdata->palm_detect_en) {
		input_report_key(input_dev,
				ts_data->pdata->palm_detect_keycode, 1);
		input_sync(input_dev);
		input_report_key(input_dev,
				ts_data->pdata->palm_detect_keycode, 0);
		input_sync(input_dev);
	}

	for (finger = 0; finger < ts_data->pdata->num_of_fingers; finger++) {
		finger_status = pt_data.flags & (0x01 << finger);

		input_mt_slot(input_dev, finger);
		input_mt_report_slot_state(input_dev, MT_TOOL_FINGER,
					finger_status != 0);

		x = pt_data.fd[finger].xLo +
			(((u16)(pt_data.fd[finger].hi & 0x0F)) << 8);
		y = pt_data.fd[finger].yLo +
			(((u16)(pt_data.fd[finger].hi & 0xF0)) << 4);

		pressure = pt_data.fd[finger].pressure & FD_PRESSURE_BITS;

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

static void it7260_ts_work_func(struct work_struct *work)
{
	struct it7260_ts_data *ts_data = container_of(work,
				struct it7260_ts_data, work_pm_relax);

	pm_relax(&ts_data->client->dev);
}

static int it7260_ts_chip_identify(struct it7260_ts_data *ts_data)
{
	static const uint8_t cmd_ident[] = {CMD_IDENT_CHIP};
	static const uint8_t expected_id[] = {0x0A, 'I', 'T', 'E', '7',
							'2', '6', '0'};
	uint8_t chip_id[10] = {0,};
	int ret;

	/*
	 * Sometimes, the controller may not respond immediately after
	 * writing the command, so wait for device to get ready.
	 * FALSE means to retry 20 times at max to read the chip status.
	 * TRUE means to add delay in each retry.
	 */
	ret = it7260_wait_device_ready(ts_data, false, true);
	if (ret < 0) {
		dev_err(&ts_data->client->dev,
			"failed to read chip status %d\n", ret);
		return ret;
	}

	ret = it7260_i2c_write_no_ready_check(ts_data, BUF_COMMAND, cmd_ident,
							sizeof(cmd_ident));
	if (ret != IT_I2C_WRITE_RET) {
		dev_err(&ts_data->client->dev,
			"failed to write CMD_IDENT_CHIP %d\n", ret);
		return ret;
	}

	/*
	 * Sometimes, the controller may not respond immediately after
	 * writing the command, so wait for device to get ready.
	 * TRUE means to retry 500 times at max to read the chip status.
	 * FALSE means to avoid unnecessary delays in each retry.
	 */
	ret = it7260_wait_device_ready(ts_data, true, false);
	if (ret < 0) {
		dev_err(&ts_data->client->dev,
			"failed to read chip status %d\n", ret);
		return ret;
	}


	ret = it7260_i2c_read_no_ready_check(ts_data, BUF_RESPONSE, chip_id,
							sizeof(chip_id));
	if (ret != IT_I2C_READ_RET) {
		dev_err(&ts_data->client->dev,
			"failed to read chip-id %d\n", ret);
		return ret;
	}
	dev_info(&ts_data->client->dev,
		"it7260_ts_chip_identify read id: %02X %c%c%c%c%c%c%c %c%c\n",
		chip_id[0], chip_id[1], chip_id[2], chip_id[3], chip_id[4],
		chip_id[5], chip_id[6], chip_id[7], chip_id[8], chip_id[9]);

	if (memcmp(chip_id, expected_id, sizeof(expected_id)))
		return -EINVAL;

	if (chip_id[8] == '5' && chip_id[9] == '6')
		dev_info(&ts_data->client->dev, "rev BX3 found\n");
	else if (chip_id[8] == '6' && chip_id[9] == '6')
		dev_info(&ts_data->client->dev, "rev BX4 found\n");
	else
		dev_info(&ts_data->client->dev, "unknown revision (0x%02X 0x%02X) found\n",
						chip_id[8], chip_id[9]);

	return 0;
}

static int reg_set_optimum_mode_check(struct regulator *reg, int load_uA)
{
	return (regulator_count_voltages(reg) > 0) ?
		regulator_set_optimum_mode(reg, load_uA) : 0;
}

static int it7260_regulator_configure(struct it7260_ts_data *ts_data, bool on)
{
	int retval;

	if (on == false)
		goto hw_shutdown;

	ts_data->vdd = devm_regulator_get(&ts_data->client->dev, "vdd");
	if (IS_ERR(ts_data->vdd)) {
		dev_err(&ts_data->client->dev,
				"%s: Failed to get vdd regulator\n", __func__);
		return PTR_ERR(ts_data->vdd);
	}

	if (regulator_count_voltages(ts_data->vdd) > 0) {
		retval = regulator_set_voltage(ts_data->vdd,
			IT_VTG_MIN_UV, IT_VTG_MAX_UV);
		if (retval) {
			dev_err(&ts_data->client->dev,
				"regulator set_vtg failed retval =%d\n",
				retval);
			goto err_set_vtg_vdd;
		}
	}

	ts_data->avdd = devm_regulator_get(&ts_data->client->dev, "avdd");
	if (IS_ERR(ts_data->avdd)) {
		dev_err(&ts_data->client->dev,
				"%s: Failed to get i2c regulator\n", __func__);
		retval = PTR_ERR(ts_data->avdd);
		goto err_get_vtg_i2c;
	}

	if (regulator_count_voltages(ts_data->avdd) > 0) {
		retval = regulator_set_voltage(ts_data->avdd,
			IT_I2C_VTG_MIN_UV, IT_I2C_VTG_MAX_UV);
		if (retval) {
			dev_err(&ts_data->client->dev,
				"reg set i2c vtg failed retval =%d\n",
				retval);
		goto err_set_vtg_i2c;
		}
	}

	return 0;

err_set_vtg_i2c:
err_get_vtg_i2c:
	if (regulator_count_voltages(ts_data->vdd) > 0)
		regulator_set_voltage(ts_data->vdd, 0, IT_VTG_MAX_UV);
err_set_vtg_vdd:
	return retval;

hw_shutdown:
	if (regulator_count_voltages(ts_data->vdd) > 0)
		regulator_set_voltage(ts_data->vdd, 0, IT_VTG_MAX_UV);
	if (regulator_count_voltages(ts_data->avdd) > 0)
		regulator_set_voltage(ts_data->avdd, 0, IT_I2C_VTG_MAX_UV);
	return 0;
};

static int it7260_power_on(struct it7260_ts_data *ts_data, bool on)
{
	int retval;

	if (on == false)
		goto power_off;

	retval = reg_set_optimum_mode_check(ts_data->vdd,
		IT_ACTIVE_LOAD_UA);
	if (retval < 0) {
		dev_err(&ts_data->client->dev,
			"Regulator vdd set_opt failed rc=%d\n",
			retval);
		return retval;
	}

	retval = regulator_enable(ts_data->vdd);
	if (retval) {
		dev_err(&ts_data->client->dev,
			"Regulator vdd enable failed rc=%d\n",
			retval);
		goto error_reg_en_vdd;
	}

	retval = reg_set_optimum_mode_check(ts_data->avdd,
		IT_I2C_ACTIVE_LOAD_UA);
	if (retval < 0) {
		dev_err(&ts_data->client->dev,
			"Regulator avdd set_opt failed rc=%d\n",
			retval);
		goto error_reg_opt_i2c;
	}

	retval = regulator_enable(ts_data->avdd);
	if (retval) {
		dev_err(&ts_data->client->dev,
			"Regulator avdd enable failed rc=%d\n",
			retval);
		goto error_reg_en_avdd;
	}

	return 0;

error_reg_en_avdd:
	reg_set_optimum_mode_check(ts_data->avdd, 0);
error_reg_opt_i2c:
	regulator_disable(ts_data->vdd);
error_reg_en_vdd:
	reg_set_optimum_mode_check(ts_data->vdd, 0);
	return retval;

power_off:
	reg_set_optimum_mode_check(ts_data->vdd, 0);
	regulator_disable(ts_data->vdd);
	reg_set_optimum_mode_check(ts_data->avdd, 0);
	regulator_disable(ts_data->avdd);

	return 0;
}

static int it7260_gpio_configure(struct it7260_ts_data *ts_data, bool on)
{
	int retval = 0;

	if (on) {
		if (gpio_is_valid(ts_data->pdata->irq_gpio)) {
			/* configure touchscreen irq gpio */
			retval = gpio_request(ts_data->pdata->irq_gpio,
					"ite_irq_gpio");
			if (retval) {
				dev_err(&ts_data->client->dev,
					"unable to request irq gpio [%d]\n",
					retval);
				goto err_irq_gpio_req;
			}

			retval = gpio_direction_input(ts_data->pdata->irq_gpio);
			if (retval) {
				dev_err(&ts_data->client->dev,
					"unable to set direction for irq gpio [%d]\n",
					retval);
				goto err_irq_gpio_dir;
			}
		} else {
			dev_err(&ts_data->client->dev,
				"irq gpio not provided\n");
				goto err_irq_gpio_req;
		}

		if (gpio_is_valid(ts_data->pdata->reset_gpio)) {
			/* configure touchscreen reset out gpio */
			retval = gpio_request(ts_data->pdata->reset_gpio,
					"ite_reset_gpio");
			if (retval) {
				dev_err(&ts_data->client->dev,
					"unable to request reset gpio [%d]\n",
					retval);
					goto err_reset_gpio_req;
			}

			retval = gpio_direction_output(
					ts_data->pdata->reset_gpio, 1);
			if (retval) {
				dev_err(&ts_data->client->dev,
					"unable to set direction for reset gpio [%d]\n",
					retval);
				goto err_reset_gpio_dir;
			}

			if (ts_data->pdata->low_reset)
				gpio_set_value(ts_data->pdata->reset_gpio, 0);
			else
				gpio_set_value(ts_data->pdata->reset_gpio, 1);

			msleep(ts_data->pdata->reset_delay);
		} else {
			dev_err(&ts_data->client->dev,
				"reset gpio not provided\n");
				goto err_reset_gpio_req;
		}
	} else {
		if (gpio_is_valid(ts_data->pdata->irq_gpio))
			gpio_free(ts_data->pdata->irq_gpio);
		if (gpio_is_valid(ts_data->pdata->reset_gpio)) {
			/*
			 * This is intended to save leakage current
			 * only. Even if the call(gpio_direction_input)
			 * fails, only leakage current will be more but
			 * functionality will not be affected.
			 */
			retval = gpio_direction_input(
					ts_data->pdata->reset_gpio);
			if (retval) {
				dev_err(&ts_data->client->dev,
					"unable to set direction for gpio reset [%d]\n",
					retval);
			}
			gpio_free(ts_data->pdata->reset_gpio);
		}
	}

	return 0;

err_reset_gpio_dir:
	if (gpio_is_valid(ts_data->pdata->reset_gpio))
		gpio_free(ts_data->pdata->reset_gpio);
err_reset_gpio_req:
err_irq_gpio_dir:
	if (gpio_is_valid(ts_data->pdata->irq_gpio))
		gpio_free(ts_data->pdata->irq_gpio);
err_irq_gpio_req:
	return retval;
}

#if CONFIG_OF
static int it7260_get_dt_coords(struct device *dev, char *name,
				struct it7260_ts_platform_data *pdata)
{
	u32 coords[IT7260_COORDS_ARR_SIZE];
	struct property *prop;
	struct device_node *np = dev->of_node;
	int coords_size, rc;

	prop = of_find_property(np, name, NULL);
	if (!prop)
		return -EINVAL;
	if (!prop->value)
		return -ENODATA;

	coords_size = prop->length / sizeof(u32);
	if (coords_size != IT7260_COORDS_ARR_SIZE) {
		dev_err(dev, "invalid %s\n", name);
		return -EINVAL;
	}

	rc = of_property_read_u32_array(np, name, coords, coords_size);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read %s\n", name);
		return rc;
	}

	if (strcmp(name, "ite,panel-coords") == 0) {
		pdata->panel_minx = coords[0];
		pdata->panel_miny = coords[1];
		pdata->panel_maxx = coords[2];
		pdata->panel_maxy = coords[3];

		if (pdata->panel_maxx == 0 || pdata->panel_minx > 0)
			rc = -EINVAL;
		else if (pdata->panel_maxy == 0 || pdata->panel_miny > 0)
			rc = -EINVAL;

		if (rc) {
			dev_err(dev, "Invalid panel resolution %d\n", rc);
			return rc;
		}
	} else if (strcmp(name, "ite,display-coords") == 0) {
		pdata->disp_minx = coords[0];
		pdata->disp_miny = coords[1];
		pdata->disp_maxx = coords[2];
		pdata->disp_maxy = coords[3];
	} else {
		dev_err(dev, "unsupported property %s\n", name);
		return -EINVAL;
	}

	return 0;
}

static int it7260_parse_dt(struct device *dev,
				struct it7260_ts_platform_data *pdata)
{
	struct it7260_ts_data *ts_data = dev_get_drvdata(dev);
	struct device_node *np = dev->of_node;
	u32 temp_val;
	int rc;

	/* reset, irq gpio info */
	pdata->reset_gpio = of_get_named_gpio_flags(np,
			"ite,reset-gpio", 0, &pdata->reset_gpio_flags);
	pdata->irq_gpio = of_get_named_gpio_flags(np,
			"ite,irq-gpio", 0, &pdata->irq_gpio_flags);

	rc = of_property_read_u32(np, "ite,num-fingers", &temp_val);
	if (!rc)
		pdata->num_of_fingers = temp_val;
	else if (rc != -EINVAL) {
		dev_err(dev, "Unable to read reset delay\n");
		return rc;
	}

	pdata->wakeup = of_property_read_bool(np, "ite,wakeup");
	pdata->palm_detect_en = of_property_read_bool(np, "ite,palm-detect-en");
	if (pdata->palm_detect_en) {
		rc = of_property_read_u32(np, "ite,palm-detect-keycode",
							&temp_val);
		if (!rc) {
			pdata->palm_detect_keycode = temp_val;
		} else {
			dev_err(dev, "Unable to read palm-detect-keycode\n");
			return rc;
		}
	}

	rc = of_property_read_string(np, "ite,fw-name", &pdata->fw_name);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read fw image name %d\n", rc);
		return rc;
	}

	rc = of_property_read_string(np, "ite,cfg-name", &pdata->cfg_name);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read cfg image name %d\n", rc);
		return rc;
	}

	snprintf(ts_data->fw_name, MAX_BUFFER_SIZE, "%s",
		(pdata->fw_name != NULL) ? pdata->fw_name : FW_NAME);
	snprintf(ts_data->cfg_name, MAX_BUFFER_SIZE, "%s",
		(pdata->cfg_name != NULL) ? pdata->cfg_name : CFG_NAME);

	rc = of_property_read_u32(np, "ite,reset-delay", &temp_val);
	if (!rc)
		pdata->reset_delay = temp_val;
	else if (rc != -EINVAL) {
		dev_err(dev, "Unable to read reset delay\n");
		return rc;
	}

	rc = of_property_read_u32(np, "ite,avdd-lpm-cur", &temp_val);
	if (!rc) {
		pdata->avdd_lpm_cur = temp_val;
	} else if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read avdd lpm current value %d\n", rc);
		return rc;
	}

	pdata->low_reset = of_property_read_bool(np, "ite,low-reset");

	rc = it7260_get_dt_coords(dev, "ite,display-coords", pdata);
	if (rc && (rc != -EINVAL))
		return rc;

	rc = it7260_get_dt_coords(dev, "ite,panel-coords", pdata);
	if (rc && (rc != -EINVAL))
		return rc;

	return 0;
}
#else
static inline int it7260_ts_parse_dt(struct device *dev,
				struct it7260_ts_platform_data *pdata)
{
	return 0;
}
#endif

static int it7260_ts_pinctrl_init(struct it7260_ts_data *ts_data)
{
	int retval;

	/* Get pinctrl if target uses pinctrl */
	ts_data->ts_pinctrl = devm_pinctrl_get(&(ts_data->client->dev));
	if (IS_ERR_OR_NULL(ts_data->ts_pinctrl)) {
		retval = PTR_ERR(ts_data->ts_pinctrl);
		dev_dbg(&ts_data->client->dev,
			"Target does not use pinctrl %d\n", retval);
		goto err_pinctrl_get;
	}

	ts_data->pinctrl_state_active
		= pinctrl_lookup_state(ts_data->ts_pinctrl,
				PINCTRL_STATE_ACTIVE);
	if (IS_ERR_OR_NULL(ts_data->pinctrl_state_active)) {
		retval = PTR_ERR(ts_data->pinctrl_state_active);
		dev_err(&ts_data->client->dev,
			"Can not lookup %s pinstate %d\n",
			PINCTRL_STATE_ACTIVE, retval);
		goto err_pinctrl_lookup;
	}

	ts_data->pinctrl_state_suspend
		= pinctrl_lookup_state(ts_data->ts_pinctrl,
			PINCTRL_STATE_SUSPEND);
	if (IS_ERR_OR_NULL(ts_data->pinctrl_state_suspend)) {
		retval = PTR_ERR(ts_data->pinctrl_state_suspend);
		dev_err(&ts_data->client->dev,
			"Can not lookup %s pinstate %d\n",
			PINCTRL_STATE_SUSPEND, retval);
		goto err_pinctrl_lookup;
	}

	ts_data->pinctrl_state_release
		= pinctrl_lookup_state(ts_data->ts_pinctrl,
			PINCTRL_STATE_RELEASE);
	if (IS_ERR_OR_NULL(ts_data->pinctrl_state_release)) {
		retval = PTR_ERR(ts_data->pinctrl_state_release);
		dev_dbg(&ts_data->client->dev,
			"Can not lookup %s pinstate %d\n",
			PINCTRL_STATE_RELEASE, retval);
	}

	return 0;

err_pinctrl_lookup:
	devm_pinctrl_put(ts_data->ts_pinctrl);
err_pinctrl_get:
	ts_data->ts_pinctrl = NULL;
	return retval;
}

static int it7260_ts_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	static const uint8_t cmd_start[] = {CMD_UNKNOWN_7};
	struct it7260_ts_data *ts_data;
	struct it7260_ts_platform_data *pdata;
	uint8_t rsp[2];
	int ret = -1, err;
	struct dentry *temp;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "need I2C_FUNC_I2C\n");
		return -ENODEV;
	}

	ts_data = devm_kzalloc(&client->dev, sizeof(*ts_data), GFP_KERNEL);
	if (!ts_data)
		return -ENOMEM;

	ts_data->client = client;
	i2c_set_clientdata(client, ts_data);

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata)
			return -ENOMEM;

		ret = it7260_parse_dt(&client->dev, pdata);
		if (ret)
			return ret;
	} else {
		pdata = client->dev.platform_data;
	}

	if (!pdata) {
		dev_err(&client->dev, "No platform data found\n");
		return -ENOMEM;
	}

	ts_data->pdata = pdata;

	ret = it7260_regulator_configure(ts_data, true);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to configure regulators\n");
		goto err_reg_configure;
	}

	ret = it7260_power_on(ts_data, true);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to power on\n");
		goto err_power_device;
	}

	/*
	 * After enabling regulators, controller needs a delay to come to
	 * an active state.
	 */
	msleep(DELAY_VTG_REG_EN);

	ret = it7260_ts_pinctrl_init(ts_data);
	if (!ret && ts_data->ts_pinctrl) {
		/*
		 * Pinctrl handle is optional. If pinctrl handle is found
		 * let pins to be configured in active state. If not
		 * found continue further without error.
		 */
		ret = pinctrl_select_state(ts_data->ts_pinctrl,
					ts_data->pinctrl_state_active);
		if (ret < 0) {
			dev_err(&ts_data->client->dev,
				"failed to select pin to active state %d",
				ret);
		}
	} else {
		ret = it7260_gpio_configure(ts_data, true);
		if (ret < 0) {
			dev_err(&client->dev, "Failed to configure gpios\n");
			goto err_gpio_config;
		}
	}

	ret = it7260_ts_chip_identify(ts_data);
	if (ret) {
		dev_err(&client->dev, "Failed to identify chip %d!!!", ret);
		goto err_identification_fail;
	}

	it7260_get_chip_versions(ts_data);

	ts_data->input_dev = input_allocate_device();
	if (!ts_data->input_dev) {
		dev_err(&client->dev, "failed to allocate input device\n");
		ret = -ENOMEM;
		goto err_input_alloc;
	}

	/* Initialize mutex for fw and cfg upgrade */
	mutex_init(&ts_data->fw_cfg_mutex);

	ts_data->input_dev->name = DEVICE_NAME;
	ts_data->input_dev->phys = "I2C";
	ts_data->input_dev->id.bustype = BUS_I2C;
	ts_data->input_dev->id.vendor = 0x0001;
	ts_data->input_dev->id.product = 0x7260;
	set_bit(EV_SYN, ts_data->input_dev->evbit);
	set_bit(EV_KEY, ts_data->input_dev->evbit);
	set_bit(EV_ABS, ts_data->input_dev->evbit);
	set_bit(INPUT_PROP_DIRECT, ts_data->input_dev->propbit);
	set_bit(BTN_TOUCH, ts_data->input_dev->keybit);
	input_set_abs_params(ts_data->input_dev, ABS_MT_POSITION_X,
		ts_data->pdata->disp_minx, ts_data->pdata->disp_maxx, 0, 0);
	input_set_abs_params(ts_data->input_dev, ABS_MT_POSITION_Y,
		ts_data->pdata->disp_miny, ts_data->pdata->disp_maxy, 0, 0);
	input_mt_init_slots(ts_data->input_dev,
					ts_data->pdata->num_of_fingers, 0);

	input_set_drvdata(ts_data->input_dev, ts_data);

	if (pdata->wakeup) {
		set_bit(KEY_WAKEUP, ts_data->input_dev->keybit);
		INIT_WORK(&ts_data->work_pm_relax, it7260_ts_work_func);
		device_init_wakeup(&client->dev, pdata->wakeup);
	}

	if (pdata->palm_detect_en)
		set_bit(ts_data->pdata->palm_detect_keycode,
					ts_data->input_dev->keybit);

	if (input_register_device(ts_data->input_dev)) {
		dev_err(&client->dev, "failed to register input device\n");
		goto err_input_register;
	}

	if (request_threaded_irq(client->irq, NULL, it7260_ts_threaded_handler,
		IRQF_TRIGGER_LOW | IRQF_ONESHOT, client->name, ts_data)) {
		dev_err(&client->dev, "request_irq failed\n");
		goto err_irq_reg;
	}

	if (sysfs_create_group(&(client->dev.kobj), &it7260_attr_group)) {
		dev_err(&client->dev, "failed to register sysfs #2\n");
		goto err_sysfs_grp_create;
	}

#if defined(CONFIG_FB)
	ts_data->fb_notif.notifier_call = fb_notifier_callback;

	ret = fb_register_client(&ts_data->fb_notif);
	if (ret)
		dev_err(&client->dev, "Unable to register fb_notifier %d\n",
					ret);
#endif
	
	it7260_i2c_write_no_ready_check(ts_data, BUF_COMMAND, cmd_start,
							sizeof(cmd_start));
	msleep(pdata->reset_delay);
	it7260_i2c_read_no_ready_check(ts_data, BUF_RESPONSE, rsp, sizeof(rsp));
	msleep(pdata->reset_delay);

	ts_data->dir = debugfs_create_dir(DEBUGFS_DIR_NAME, NULL);
	if (ts_data->dir == NULL || IS_ERR(ts_data->dir)) {
		dev_err(&client->dev,
			"%s: Failed to create debugfs directory, ret = %ld\n",
			__func__, PTR_ERR(ts_data->dir));
		ret = PTR_ERR(ts_data->dir);
		goto err_create_debugfs_dir;
	}

	temp = debugfs_create_file("suspend", S_IRUSR | S_IWUSR, ts_data->dir,
					ts_data, &debug_suspend_fops);
	if (temp == NULL || IS_ERR(temp)) {
		dev_err(&client->dev,
			"%s: Failed to create suspend debugfs file, ret = %ld\n",
			__func__, PTR_ERR(temp));
		ret = PTR_ERR(temp);
		goto err_create_debugfs_file;
	}

	return 0;

err_create_debugfs_file:
	debugfs_remove_recursive(ts_data->dir);
err_create_debugfs_dir:
#if defined(CONFIG_FB)
	if (fb_unregister_client(&ts_data->fb_notif))
		dev_err(&client->dev, "Error occurred while unregistering fb_notifier.\n");
#endif
	sysfs_remove_group(&(client->dev.kobj), &it7260_attr_group);

err_sysfs_grp_create:
	free_irq(client->irq, ts_data);

err_irq_reg:
	input_unregister_device(ts_data->input_dev);

err_input_register:
	if (pdata->wakeup) {
		cancel_work_sync(&ts_data->work_pm_relax);
		device_init_wakeup(&client->dev, false);
	}
	if (ts_data->input_dev)
		input_free_device(ts_data->input_dev);
	ts_data->input_dev = NULL;

err_input_alloc:
err_identification_fail:
	if (ts_data->ts_pinctrl) {
		if (IS_ERR_OR_NULL(ts_data->pinctrl_state_release)) {
			devm_pinctrl_put(ts_data->ts_pinctrl);
			ts_data->ts_pinctrl = NULL;
		} else {
			err = pinctrl_select_state(ts_data->ts_pinctrl,
					ts_data->pinctrl_state_release);
			if (err)
				dev_err(&ts_data->client->dev,
					"failed to select relase pinctrl state %d\n",
					err);
		}
	} else {
		if (gpio_is_valid(pdata->reset_gpio))
			gpio_free(pdata->reset_gpio);
		if (gpio_is_valid(pdata->irq_gpio))
			gpio_free(pdata->irq_gpio);
	}

err_gpio_config:
	it7260_power_on(ts_data, false);

err_power_device:
	it7260_regulator_configure(ts_data, false);

err_reg_configure:
	return ret;
}

static int it7260_ts_remove(struct i2c_client *client)
{
	struct it7260_ts_data *ts_data = i2c_get_clientdata(client);
	int ret;

	debugfs_remove_recursive(ts_data->dir);
#if defined(CONFIG_FB)
	if (fb_unregister_client(&ts_data->fb_notif))
		dev_err(&client->dev, "Error occurred while unregistering fb_notifier.\n");
#endif
	sysfs_remove_group(&(client->dev.kobj), &it7260_attr_group);
	free_irq(client->irq, ts_data);
	input_unregister_device(ts_data->input_dev);
	if (ts_data->input_dev)
		input_free_device(ts_data->input_dev);
	ts_data->input_dev = NULL;
	if (ts_data->pdata->wakeup) {
		cancel_work_sync(&ts_data->work_pm_relax);
		device_init_wakeup(&client->dev, false);
	}
	if (ts_data->ts_pinctrl) {
		if (IS_ERR_OR_NULL(ts_data->pinctrl_state_release)) {
			devm_pinctrl_put(ts_data->ts_pinctrl);
			ts_data->ts_pinctrl = NULL;
		} else {
			ret = pinctrl_select_state(ts_data->ts_pinctrl,
					ts_data->pinctrl_state_release);
			if (ret)
				dev_err(&ts_data->client->dev,
					"failed to select relase pinctrl state %d\n",
					ret);
		}
	} else {
		if (gpio_is_valid(ts_data->pdata->reset_gpio))
			gpio_free(ts_data->pdata->reset_gpio);
		if (gpio_is_valid(ts_data->pdata->irq_gpio))
			gpio_free(ts_data->pdata->irq_gpio);
	}
	it7260_power_on(ts_data, false);
	it7260_regulator_configure(ts_data, false);

	return 0;
}

#if defined(CONFIG_FB)
static int fb_notifier_callback(struct notifier_block *self,
			unsigned long event, void *data)
{
	struct it7260_ts_data *ts_data = container_of(self,
					struct it7260_ts_data, fb_notif);
	struct fb_event *evdata = data;
	int *blank;

	if (evdata && evdata->data && ts_data && ts_data->client) {
		if (event == FB_EVENT_BLANK) {
			blank = evdata->data;
			if (*blank == FB_BLANK_UNBLANK)
				it7260_ts_resume(&(ts_data->client->dev));
			else if (*blank == FB_BLANK_POWERDOWN ||
					*blank == FB_BLANK_VSYNC_SUSPEND)
				it7260_ts_suspend(&(ts_data->client->dev));
		}
	}

	return 0;
}
#endif

#ifdef CONFIG_PM
static int it7260_ts_resume(struct device *dev)
{
	struct it7260_ts_data *ts_data = dev_get_drvdata(dev);
	int retval;

	if (device_may_wakeup(dev)) {
		if (ts_data->device_needs_wakeup) {
			/* Set active current for the avdd regulator */
			if (ts_data->pdata->avdd_lpm_cur) {
				retval = reg_set_optimum_mode_check(
						ts_data->avdd,
						IT_I2C_ACTIVE_LOAD_UA);
				if (retval < 0)
					dev_err(dev, "Regulator avdd set_opt failed at resume rc=%d\n",
					retval);
			}

			ts_data->device_needs_wakeup = false;
			disable_irq_wake(ts_data->client->irq);
		}
		return 0;
	}

	if (ts_data->ts_pinctrl) {
		retval = pinctrl_select_state(ts_data->ts_pinctrl,
				ts_data->pinctrl_state_active);
		if (retval < 0) {
			dev_err(dev, "Cannot get default pinctrl state %d\n",
				retval);
			goto err_pinctrl_select_suspend;
		}
	}

	enable_irq(ts_data->client->irq);
	ts_data->suspended = false;
	return 0;

err_pinctrl_select_suspend:
	return retval;
}

static int it7260_ts_suspend(struct device *dev)
{
	struct it7260_ts_data *ts_data = dev_get_drvdata(dev);
	int retval;

	if (ts_data->fw_cfg_uploading) {
		dev_dbg(dev, "Fw/cfg uploading. Can't go to suspend.\n");
		return -EBUSY;
	}

	if (device_may_wakeup(dev)) {
		if (!ts_data->device_needs_wakeup) {
			/* put the device in low power idle mode */
			it7260_ts_chip_low_power_mode(ts_data,
						PWR_CTL_LOW_POWER_MODE);

			/* Set lpm current for avdd regulator */
			if (ts_data->pdata->avdd_lpm_cur) {
				retval = reg_set_optimum_mode_check(
						ts_data->avdd,
						ts_data->pdata->avdd_lpm_cur);
				if (retval < 0)
					dev_err(dev, "Regulator avdd set_opt failed at suspend rc=%d\n",
						retval);
			}

			ts_data->device_needs_wakeup = true;
			enable_irq_wake(ts_data->client->irq);
		}
		return 0;
	}

	disable_irq(ts_data->client->irq);

	it7260_ts_release_all(ts_data);

	if (ts_data->ts_pinctrl) {
		retval = pinctrl_select_state(ts_data->ts_pinctrl,
				ts_data->pinctrl_state_suspend);
		if (retval < 0) {
			dev_err(dev, "Cannot get idle pinctrl state %d\n",
				retval);
			goto err_pinctrl_select_suspend;
		}
	}

	ts_data->suspended = true;

	return 0;

err_pinctrl_select_suspend:
	return retval;
}

static const struct dev_pm_ops it7260_ts_dev_pm_ops = {
	.suspend = it7260_ts_suspend,
	.resume  = it7260_ts_resume,
};
#else
static int it7260_ts_resume(struct device *dev)
{
	return 0;
}

static int it7260_ts_suspend(struct device *dev)
{
	return 0;
}
#endif

static const struct i2c_device_id it7260_ts_id[] = {
	{ DEVICE_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, it7260_ts_id);

static const struct of_device_id it7260_match_table[] = {
	{ .compatible = "ite,it7260_ts",},
	{},
};

static struct i2c_driver it7260_ts_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = DEVICE_NAME,
		.of_match_table = it7260_match_table,
#ifdef CONFIG_PM
		.pm = &it7260_ts_dev_pm_ops,
#endif
	},
	.probe = it7260_ts_probe,
	.remove = it7260_ts_remove,
	.id_table = it7260_ts_id,
};

module_i2c_driver(it7260_ts_driver);

MODULE_DESCRIPTION("it7260 Touchscreen Driver");
MODULE_LICENSE("GPL v2");
