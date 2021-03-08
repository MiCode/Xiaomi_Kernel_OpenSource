/*
 * Goodix I2C Module
 * Hardware interface layer of touchdriver architecture.
 *
 * Copyright (C) 2019 - 2020 Goodix, Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be a reference
 * to you, when you are integrating the GOODiX's CTP IC into your system,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include "goodix_ts_core.h"
#include "goodix_cfg_bin.h"

#define TS_DRIVER_NAME			"gtx8"
#define I2C_MAX_TRANSFER_SIZE		256
#define TS_ADDR_LENGTH			2
#define TS_DOZE_ENABLE_RETRY_TIMES	3
#define TS_DOZE_DISABLE_RETRY_TIMES	9
#define TS_WAIT_CFG_READY_RETRY_TIMES	30
#define TS_WAIT_CMD_FREE_RETRY_TIMES	10

#define TS_REG_COORDS_BASE		0x824E
#define TS_REG_CMD			0x8040
#define TS_REG_REQUEST			0x8044
#define TS_REG_VERSION			0x8240
#define TS_REG_CFG_BASE			0x8050
#define TS_REG_DOZE_CTRL		0x30F0
#define TS_REG_DOZE_STAT		0x3100
#define TS_REG_ESD_TICK_R		0x3103

#define CFG_XMAX_OFFSET			(0x8052 - 0x8050)
#define CFG_YMAX_OFFSET			(0x8054 - 0x8050)

#define REQUEST_HANDLED			0x00
#define REQUEST_CONFIG			0x01
#define REQUEST_BAKREF			0x02
#define REQUEST_RESET			0x03
#define REQUEST_RELOADFW		0x05
#define REQUEST_IDLE			0xff

#define COMMAND_SLEEP			0x05
#define COMMAND_CLOSE_HID		0xaa
#define COMMAND_START_SEND_CFG		0x80
#define COMMAND_END_SEND_CFG		0x83
#define COMMAND_SEND_SMALL_CFG		0x81
#define COMMAND_SEND_CFG_PREPARE_OK	0x82
#define COMMAND_START_READ_CFG		0x86
#define COMMAND_READ_CFG_PREPARE_OK	0x85
#define COMMAND_END_SEND_CFG_YS		0x7D

#define BYTES_PER_COORD			8
#define TS_MAX_SENSORID			5
#define TS_CFG_HEAD_LEN			4
#define TS_CFG_HEAD_LEN_YS		5
#define TS_CFG_BAG_NUM_INDEX		2
#define TS_CFG_BAG_START_INDEX		4

#define TS_DOZE_DISABLE_DATA		0xAA
#define TS_DOZE_CLOSE_OK_DATA		0xBB
#define TS_DOZE_ENABLE_DATA		0xCC
#define	TS_CMD_REG_READY		0xFF
#define TS_CMD_CFG_ERR			0x7E
#define TS_CMD_CFG_OK			0x7F

enum TS_SEND_CFG_REPLY {
	TS_CFG_REPLY_PKGS_ERR   = 0x01,
	TS_CFG_REPLY_CHKSUM_ERR = 0x02,
	TS_CFG_REPLY_DATA_ERR   = 0x03,
	TS_CFG_REPLY_DATA_EQU   = 0x07,
};

#define IRQ_HEAD_LEN_YS			8
#define IRQ_HEAD_LEN_NOR		2

int goodix_ts_core_init(void);
#ifdef CONFIG_OF
/**
 * goodix_parse_dt_resolution - parse resolution from dt
 * @node: devicetree node
 * @board_data: pointer to board data structure
 * return: 0 - no error, <0 error
 */
static int goodix_parse_dt_resolution(struct device_node *node,
		struct goodix_ts_board_data *board_data)
{
	int r, err;

	r = of_property_read_u32(node, "goodix,panel-max-x",
				 &board_data->panel_max_x);
	if (r)
		err = -ENOENT;

	r = of_property_read_u32(node, "goodix,panel-max-y",
				 &board_data->panel_max_y);
	if (r)
		err = -ENOENT;

	r = of_property_read_u32(node, "goodix,panel-max-w",
				 &board_data->panel_max_w);
	if (r)
		err = -ENOENT;

	r = of_property_read_u32(node, "goodix,panel-max-fod",
				 &board_data->panel_max_overlapping_area);
	if (r)
		err = -ENOENT;

	board_data->swap_axis = of_property_read_bool(node,
					"goodix,swap-axis");
	board_data->x2x = of_property_read_bool(node, "goodix,x2x");
	board_data->y2y = of_property_read_bool(node, "goodix,y2y");

	return 0;
}

/**
 * goodix_parse_dt - parse board data from dt
 * @dev: pointer to device
 * @board_data: pointer to board data structure
 * return: 0 - no error, <0 error
 */
static int goodix_parse_dt(struct device_node *node,
	struct goodix_ts_board_data *board_data)
{
	struct property *prop;
	int r;

	if (!board_data) {
		ts_err("invalid board data");
		return -EINVAL;
	}

	r = of_property_read_string(node, "goodix,fw-name",
				    &board_data->fw_name);
	if (r) {
		ts_err("can't get firmware name");
		board_data->fw_name = NULL;
	}

	r = of_get_named_gpio(node, "goodix,reset-gpio", 0);
	if (r < 0) {
		ts_err("invalid reset-gpio in dt: %d", r);
		return -EINVAL;
	}
	ts_info("get reset-gpio[%d] from dt", r);
	board_data->reset_gpio = r;

	r = of_get_named_gpio(node, "goodix,irq-gpio", 0);
	if (r < 0) {
		ts_err("invalid irq-gpio in dt: %d", r);
		return -EINVAL;
	}
	ts_info("get irq-gpio[%d] from dt", r);
	board_data->irq_gpio = r;

	r = of_property_read_u32(node, "goodix,irq-flags",
			&board_data->irq_flags);
	if (r) {
		ts_err("invalid irq-flags");
		return -EINVAL;
	}

	r = of_property_read_u32(node, "goodix,fod-lx",
			&board_data->fod_lx);
	if (r) {
		ts_err("invalid fod-lx");
		return -EINVAL;
	} else
		ts_info("get fod-lx:%d", board_data->fod_lx);

	r = of_property_read_u32(node, "goodix,fod-lx",
			&board_data->fod_lx);
	if (r) {
		ts_err("invalid fod-lx");
		return -EINVAL;
	} else
		ts_info("get fod-lx:%d", board_data->fod_lx);

	r = of_property_read_u32(node, "goodix,fod-ly",
			&board_data->fod_ly);
	if (r) {
		ts_err("invalid fod-ly");
		return -EINVAL;
	} else
		ts_info("get fod-ly:%d", board_data->fod_ly);

	r = of_property_read_u32(node, "goodix,fod-x-size",
			&board_data->fod_x_size);
	if (r) {
		ts_err("invalid fod-x-size");
		return -EINVAL;
	} else
		ts_info("get fod-x-size:%d", board_data->fod_x_size);

	r = of_property_read_u32(node, "goodix,fod-y-size",
			&board_data->fod_y_size);
	if (r) {
		ts_err("invalid fod-y-size");
		return -EINVAL;
	} else
		ts_info("get fod-y-size:%d", board_data->fod_y_size);

/*
	r = of_property_read_string(node, "goodix,avdd-name", &name_tmp);
	if (!r) {
		ts_info("avdd name form dt: %s", name_tmp);
		if (strlen(name_tmp) < sizeof(board_data->avdd_name))
			strncpy(board_data->avdd_name,
				name_tmp, sizeof(board_data->avdd_name));
		else
			ts_info("invalied avdd name length: %ld > %ld",
				strlen(name_tmp),
				sizeof(board_data->avdd_name));
	}
*/
	board_data->avdd_name = "vtouch";
	board_data->vddio_name = "iotouch";

	r = of_property_read_string(node, "goodix,cfg-name",
				    &board_data->cfg_bin_name);
	if (r) {
		ts_err("can't get cfg name");
		board_data->cfg_bin_name = NULL;
	}

	r = of_property_read_u32(node, "goodix,power-on-delay-us",
				&board_data->power_on_delay_us);
	if (!r) {
		/* 1000ms is too large, maybe you have pass a wrong value */
		if (board_data->power_on_delay_us > 1000 * 1000) {
			ts_err("Power on delay time exceed 1s, please check");
			board_data->power_on_delay_us = 0;
		}
	}

	r = of_property_read_u32(node, "goodix,power-off-delay-us",
				&board_data->power_off_delay_us);
	if (!r) {
		/* 1000ms is too large, maybe you have pass */
		if (board_data->power_off_delay_us > 1000 * 1000) {
			ts_err("Power off delay time exceed 1s, please check");
			board_data->power_off_delay_us = 0;
		}
	}

	/* get xyz resolutions */
	r = goodix_parse_dt_resolution(node, board_data);
	if (r < 0) {
		ts_err("Failed to parse resolutions:%d", r);
		return r;
	}

	/* key map */
	prop = of_find_property(node, "goodix,panel-key-map", NULL);
	if (prop && prop->length) {
		if (prop->length / sizeof(u32) > GOODIX_MAX_TP_KEY) {
			ts_err("Size of panel-key-map is invalid");
			return r;
		}

		board_data->panel_max_key = prop->length / sizeof(u32);
		board_data->tp_key_num = prop->length / sizeof(u32);
		r = of_property_read_u32_array(node,
				"goodix,panel-key-map",
				&board_data->panel_key_map[0],
				board_data->panel_max_key);
		if (r) {
			ts_err("failed get key map, %d", r);
			return r;
		}
	}

	/*get pen-enable switch and pen keys, must after "key map"*/
	board_data->pen_enable = of_property_read_bool(node,
					"goodix,pen-enable");
	if (board_data->pen_enable)
		ts_info("goodix pen enabled");

	ts_info("***key:%d, %d, %d, %d",
		board_data->panel_key_map[0], board_data->panel_key_map[1],
		board_data->panel_key_map[2], board_data->panel_key_map[3]);

	ts_debug("[DT]x:%d, y:%d, w:%d, p:%d", board_data->panel_max_x,
		 board_data->panel_max_y, board_data->panel_max_w,
		 board_data->panel_max_p);
	return 0;
}

/**
 * goodix_parse_customize_params - parse sensor independent params
 * @dev: pointer to device data
 * @board_data: board data
 * @sensor_id: sensor ID
 * return: 0 - read ok, < 0 - i2c transter error
*/
static int goodix_parse_customize_params(struct goodix_ts_device *dev, unsigned int sensor_id)
{
	struct device_node *node = dev->dev->of_node;
	struct goodix_ts_board_data *board_data = &dev->board_data;
	char of_node_name[24];
	int r;
	ts_info("goodix_parse_customize_params Enter");

	if (!board_data || !node)
		return -EINVAL;

	if (sensor_id > TS_MAX_SENSORID || node == NULL) {
		ts_err("Invalid sensor id");
		return -EINVAL;
	}

	/* parse sensor independent parameters */
	snprintf(of_node_name, sizeof(of_node_name), "sensor%u", sensor_id);
	node = of_find_node_by_name(dev->dev->of_node, of_node_name);
	if (!node) {
		ts_err("Child property[%s] not found", of_node_name);
		return -EINVAL;
	}
/*
	r = of_property_read_string(node, "goodix,fw-name",
				    &board_data->fw_name);
	if (r) {
		ts_err("can't get firmware name");
		board_data->fw_name = NULL;
	}
*/
	/* sensor independent resolutions */
	r = goodix_parse_dt_resolution(node, board_data);
	ts_info("goodix_parse_customize_params Exit");
	return r;
}
#endif

int goodix_i2c_test(struct goodix_ts_device *dev)
{
#define TEST_ADDR  0x4100
#define TEST_LEN   1
	struct i2c_client *client = to_i2c_client(dev->dev);
	unsigned char test_buf[TEST_LEN + 1], addr_buf[2];
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = !I2C_M_RD,
			.buf = &addr_buf[0],
			.len = TS_ADDR_LENGTH,
		}, {
			.addr = client->addr,
			.flags = I2C_M_RD,
			.buf = &test_buf[0],
			.len = TEST_LEN,
		}
	};

	msgs[0].buf[0] = (TEST_ADDR >> 8) & 0xFF;
	msgs[0].buf[1] = TEST_ADDR & 0xFF;

	if (likely(i2c_transfer(client->adapter, msgs, 2) == 2))
		return 0;

	/* test failed */
	return -EINVAL;
}

/* confirm current device is goodix or not.
 * If confirmed 0 will return.
 */
static int goodix_ts_dev_confirm(struct goodix_ts_device *ts_dev)
{
#define DEV_CONFIRM_RETRY 3
	int retry;

	for (retry = 0; retry < DEV_CONFIRM_RETRY; retry++) {
		gpio_direction_output(ts_dev->board_data.reset_gpio, 0);
		udelay(2000);
		gpio_direction_output(ts_dev->board_data.reset_gpio, 1);
		mdelay(5);
		if (!goodix_i2c_test(ts_dev)) {
			msleep(95);
			return 0;
		}
	}
	return -EINVAL;
}

/**
 * goodix_i2c_read_trans - read device register through i2c bus
 * @dev: pointer to device data
 * @addr: register address
 * @data: read buffer
 * @len: bytes to read
 * return: 0 - read ok, < 0 - i2c transter error
 */
int goodix_i2c_read_trans(struct goodix_ts_device *dev, unsigned int reg,
	unsigned char *data, unsigned int len)
{
	struct i2c_client *client = to_i2c_client(dev->dev);
	unsigned int transfer_length = 0;
	unsigned int pos = 0, address = reg;
	unsigned char get_buf[64], addr_buf[2];
	int retry, r = 0;
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = !I2C_M_RD,
			.buf = &addr_buf[0],
			.len = TS_ADDR_LENGTH,
		}, {
			.addr = client->addr,
			.flags = I2C_M_RD,
		}
	};

	if (likely(len < sizeof(get_buf))) {
		/* code optimize, use stack memory */
		msgs[1].buf = &get_buf[0];
	} else {
		msgs[1].buf = kzalloc(I2C_MAX_TRANSFER_SIZE < len
				   ? I2C_MAX_TRANSFER_SIZE : len, GFP_KERNEL);
		if (msgs[1].buf == NULL)
			return -ENOMEM;
	}

	while (pos != len) {
		if (unlikely(len - pos > I2C_MAX_TRANSFER_SIZE))
			transfer_length = I2C_MAX_TRANSFER_SIZE;
		else
			transfer_length = len - pos;

		msgs[0].buf[0] = (address >> 8) & 0xFF;
		msgs[0].buf[1] = address & 0xFF;
		msgs[1].len = transfer_length;

		for (retry = 0; retry < GOODIX_BUS_RETRY_TIMES; retry++) {
			if (likely(i2c_transfer(client->adapter,
						msgs, 2) == 2)) {
				memcpy(&data[pos], msgs[1].buf,
				       transfer_length);
				pos += transfer_length;
				address += transfer_length;
				break;
			}
			ts_info("I2c read retry[%d]:0x%x", retry + 1, reg);
			msleep(20);
		}
		if (unlikely(retry == GOODIX_BUS_RETRY_TIMES)) {
			ts_err("I2c read failed,dev:%02x,reg:%04x,size:%u",
			       client->addr, reg, len);
			r = -EBUS;
			goto read_exit;
		}
	}

read_exit:
	if (unlikely(len >= sizeof(get_buf)))
		kfree(msgs[1].buf);
	return r;
}

/**
 * goodix_i2c_write_trans - write device register through i2c bus
 * @dev: pointer to device data
 * @addr: register address
 * @data: write buffer
 * @len: bytes to write
 * return: 0 - write ok; < 0 - i2c transter error.
 */
int goodix_i2c_write_trans(struct goodix_ts_device *dev, unsigned int reg,
		unsigned char *data, unsigned int len)
{
	struct i2c_client *client = to_i2c_client(dev->dev);
	unsigned int pos = 0, transfer_length = 0;
	unsigned int address = reg;
	unsigned char put_buf[64];
	int retry, r = 0;
	struct i2c_msg msg = {
			.addr = client->addr,
			.flags = !I2C_M_RD,
	};

	if (likely(len + TS_ADDR_LENGTH < sizeof(put_buf))) {
		/* code optimize,use stack memory*/
		msg.buf = &put_buf[0];
	} else {
		msg.buf = kmalloc(I2C_MAX_TRANSFER_SIZE < len + TS_ADDR_LENGTH
				? I2C_MAX_TRANSFER_SIZE : len + TS_ADDR_LENGTH,
				GFP_KERNEL);
		if (msg.buf == NULL)
			return -ENOMEM;
	}

	while (pos != len) {
		if (unlikely(len - pos > I2C_MAX_TRANSFER_SIZE - TS_ADDR_LENGTH))
			transfer_length = I2C_MAX_TRANSFER_SIZE - TS_ADDR_LENGTH;
		else
			transfer_length = len - pos;

		msg.buf[0] = (unsigned char)((address >> 8) & 0xFF);
		msg.buf[1] = (unsigned char)(address & 0xFF);
		msg.len = transfer_length + 2;
		memcpy(&msg.buf[2], &data[pos], transfer_length);

		for (retry = 0; retry < GOODIX_BUS_RETRY_TIMES; retry++) {
			if (likely(i2c_transfer(client->adapter,
						&msg, 1) == 1)) {
				pos += transfer_length;
				address += transfer_length;
				break;
			}
			ts_debug("I2c write retry[%d]", retry + 1);
			msleep(20);
		}
		if (unlikely(retry == GOODIX_BUS_RETRY_TIMES)) {
			ts_err("I2c write failed,dev:%02x,reg:%04x,size:%u",
				client->addr, reg, len);
			r = -EBUS;
			goto write_exit;
		}
	}

write_exit:
	if (likely(len + TS_ADDR_LENGTH >= sizeof(put_buf)))
		kfree(msg.buf);
	return r;
}


/**
 * goodix_set_i2c_doze_mode - disable or enable doze mode
 * @dev: pointer to device data
 * @enable: true/flase
 * return: 0 - ok; < 0 - error.
 * This func must be used in pairs, when you disable doze
 * mode, then you must enable it again.
 * Between set_doze_false and set_doze_true, do not reset
 * IC!
 */
static int goodix_set_i2c_doze_mode(struct goodix_ts_device *dev, int enable)
{
	static DEFINE_MUTEX(doze_mode_lock);
	static int doze_mode_set_count;
	int result = -EINVAL;
	int i;
	u8 w_data, r_data;

	if (dev->ic_type != IC_TYPE_NORMANDY)
		return 0;

	mutex_lock(&doze_mode_lock);

	if (enable) {
		if (doze_mode_set_count != 0)
			doze_mode_set_count--;

		/*when count equal 0, allow ic enter doze mode*/
		if (doze_mode_set_count == 0) {
			w_data = TS_DOZE_ENABLE_DATA;
			for (i = 0; i < TS_DOZE_ENABLE_RETRY_TIMES; i++) {
				result = goodix_i2c_write_trans(dev,
						TS_REG_DOZE_CTRL, &w_data, 1);
				if (!result) {
					result = 0;
					goto exit;
				}
				usleep_range(1000, 1100);
			}
			if (i >= TS_DOZE_ENABLE_RETRY_TIMES)
				ts_err("i2c doze mode enable failed");
		} else {
			/*ts_info("doze count not euqal 0,
			 * so skip doze mode enable");
			 */
			result = 0;
			goto exit;
		}
	} else {
		doze_mode_set_count++;

		if (doze_mode_set_count == 1) {
			w_data = TS_DOZE_DISABLE_DATA;
			goodix_i2c_write_trans(dev, TS_REG_DOZE_CTRL,
					       &w_data, 1);
			usleep_range(1000, 1100);
			for (i = 0; i < TS_DOZE_DISABLE_RETRY_TIMES; i++) {
				goodix_i2c_read_trans(dev,
						TS_REG_DOZE_STAT, &r_data, 1);
				if (TS_DOZE_CLOSE_OK_DATA == r_data) {
					result = 0;
					goto exit;
				} else if (0xAA != r_data) {
					w_data = TS_DOZE_DISABLE_DATA;
					goodix_i2c_write_trans(dev,
						TS_REG_DOZE_CTRL, &w_data, 1);
				}
				usleep_range(10000, 10100);
			}
			ts_err("doze mode disable FAILED");
		} else {
			result = 0;
			goto exit;
		}
	}

exit:
	mutex_unlock(&doze_mode_lock);
	return result;
}

/**
 * goodix_i2c_write - write device register through i2c bus
 * @dev: pointer to device data
 * @addr: register address
 * @data: write buffer
 * @len: bytes to write
 * return: 0 - write ok; < 0 - i2c transter error.
 */
int goodix_i2c_write(struct goodix_ts_device *dev, unsigned int reg,
		unsigned char *data, unsigned int len)
{
	int r = -EINVAL;

	if (goodix_set_i2c_doze_mode(dev, false)) {
		ts_err(" faild disable doze, i2c write:0x%04x", reg);
		goto exit;
	}
	r = goodix_i2c_write_trans(dev, reg, data, len);

exit:
	if (goodix_set_i2c_doze_mode(dev, true))
		ts_err("failed enable doze write:0x%04x", reg);

	return r;
}

/**
 * goodix_i2c_read - read device register through i2c bus
 * @dev: pointer to device data
 * @addr: register address
 * @data: read buffer
 * @len: bytes to read
 * return: 0 - read ok, < 0 - i2c transter error
 */
int goodix_i2c_read(struct goodix_ts_device *dev, unsigned int reg,
	unsigned char *data, unsigned int len)
{
	int r = -EINVAL;

	if (goodix_set_i2c_doze_mode(dev, false)) {
		ts_err("failed disable doze:0x%04x", reg);
		goto exit;
	}
	r = goodix_i2c_read_trans(dev, reg, data, len);

exit:
	if (goodix_set_i2c_doze_mode(dev, true))
		ts_err("failed enable doze :0x%04x", reg);

	return r;
}

/**
 * goodix_i2c_write_trans_once
 * write device register through i2c bus, no retry
 * @dev: pointer to device data
 * @addr: register address
 * @data: write buffer
 * @len: bytes to write
 * return: 0 - write ok; < 0 - i2c transter error.
 */
int goodix_i2c_write_trans_once(struct goodix_ts_device *dev, unsigned int reg,
		unsigned char *data, unsigned int len)
{
	struct i2c_client *client = to_i2c_client(dev->dev);
	unsigned int pos = 0, transfer_length = 0;
	unsigned int address = reg;
	unsigned char put_buf[64];
	struct i2c_msg msg = {
			.addr = client->addr,
			.flags = !I2C_M_RD,
	};

	if (likely(len + TS_ADDR_LENGTH < sizeof(put_buf))) {
		/* code optimize,use stack memory*/
		msg.buf = &put_buf[0];
	} else {
		msg.buf = kmalloc(I2C_MAX_TRANSFER_SIZE < len + TS_ADDR_LENGTH
				? I2C_MAX_TRANSFER_SIZE : len + TS_ADDR_LENGTH,
				GFP_KERNEL);
		if (msg.buf == NULL) {
			ts_err("Malloc failed");
			return -ENOMEM;
		}
	}

	while (pos != len) {
		if (unlikely(len - pos > I2C_MAX_TRANSFER_SIZE - TS_ADDR_LENGTH))
			transfer_length = I2C_MAX_TRANSFER_SIZE - TS_ADDR_LENGTH;
		else
			transfer_length = len - pos;

		msg.buf[0] = (unsigned char)((address >> 8) & 0xFF);
		msg.buf[1] = (unsigned char)(address & 0xFF);
		msg.len = transfer_length + 2;
		memcpy(&msg.buf[2], &data[pos], transfer_length);

		i2c_transfer(client->adapter, &msg, 1);
		pos += transfer_length;
		address += transfer_length;
	}

	if (likely(len + TS_ADDR_LENGTH >= sizeof(put_buf)))
		kfree(msg.buf);
	return 0;
}

static void goodix_cmd_init(struct goodix_ts_device *dev,
			    struct goodix_ts_cmd *ts_cmd,
			    u8 cmds, u16 cmd_data, u32 reg_addr)
{
	u16 checksum = 0;
	ts_cmd->initialized = false;
	if (!reg_addr || !cmds)
		return;

	if (dev->ic_type == IC_TYPE_YELLOWSTONE) {
		ts_cmd->cmd_reg = reg_addr;
		ts_cmd->length = 5;
		ts_cmd->cmds[0] = cmds;
		ts_cmd->cmds[1] = (cmd_data >> 8) & 0xFF;
		ts_cmd->cmds[2] = cmd_data & 0xFF;
		checksum = ts_cmd->cmds[0] + ts_cmd->cmds[1] +
			ts_cmd->cmds[2];
		ts_cmd->cmds[3] = (checksum >> 8) & 0xFF;
		ts_cmd->cmds[4] = checksum & 0xFF;
		ts_cmd->initialized = true;		
	} else if (dev->ic_type == IC_TYPE_NORMANDY) {
		ts_cmd->cmd_reg = reg_addr;
		ts_cmd->length = 3;
		ts_cmd->cmds[0] = cmds;
		ts_cmd->cmds[1] = cmd_data & 0xFF;
		ts_cmd->cmds[2] = 0 - cmds - cmd_data;
		ts_cmd->initialized = true;
	} else {
		ts_err("unsupported ic type");
	}
}

/**
 * goodix_send_command - seng cmd to firmware
 *
 * @dev: pointer to device
 * @cmd: pointer to command struct which cotain command data
 * Returns 0 - succeed,<0 - failed
 */
int goodix_send_command(struct goodix_ts_device *dev,
		struct goodix_ts_cmd *cmd)
{
	int ret;

	if (!cmd || !cmd->initialized)
		return -EINVAL;

	ret = goodix_i2c_write(dev, cmd->cmd_reg, cmd->cmds, cmd->length);

	return ret;
}

static int goodix_read_version(struct goodix_ts_device *dev,
		struct goodix_ts_version *version)
{
	u8 buffer[GOODIX_PID_MAX_LEN + 1];
	u8 temp_buf[256];
	u16 checksum = 0;
	u8 pid_read_len = dev->reg.pid_len;
	u8 vid_read_len = dev->reg.vid_len;
	u8 sensor_id_mask = dev->reg.sensor_id_mask;
	int r;

	if (!version) {
		ts_err("pointer of version is NULL");
		return -EINVAL;
	}
	version->valid = false;

	/*check reg info valid*/
	if (!dev->reg.pid || !dev->reg.sensor_id || !dev->reg.vid) {
		ts_err("reg is NULL, pid:0x%04x, vid:0x%04x, sensor_id:0x%04x",
			dev->reg.pid, dev->reg.vid, dev->reg.sensor_id);
		return -EINVAL;
	}
	if (!pid_read_len || pid_read_len > GOODIX_PID_MAX_LEN ||
	    !vid_read_len || vid_read_len > GOODIX_VID_MAX_LEN) {
		ts_err("invalied pid vid length, pid_len:%d, vid_len:%d",
			pid_read_len, vid_read_len);
		return -EINVAL;
	}

	/*disable doze mode, just valid for normandy
	 * this func must be used in pairs
	 */
	if (goodix_set_i2c_doze_mode(dev, false)) {
		ts_err("failed disable doze");
		r = -EINVAL;
		goto exit;
	}

	/*check checksum*/
	if (dev->reg.version_base && dev->reg.version_len < sizeof(temp_buf)) {
		r = goodix_i2c_read(dev, dev->reg.version_base,
				temp_buf, dev->reg.version_len);
		if (r < 0) {
			ts_err("Read version base failed, reg:0x%02x, len:%d",
				dev->reg.version_base, dev->reg.version_len);
			if (version)
				version->valid = false;
			goto exit;
		}

		if (dev->ic_type == IC_TYPE_YELLOWSTONE)
			checksum = checksum_u8_ys(temp_buf, dev->reg.version_len);
		else
			checksum = checksum_u8(temp_buf, dev->reg.version_len);
		if (checksum) {
			ts_err("checksum error:0x%02x, base:0x%02x, len:%d",
			       checksum, dev->reg.version_base,
			       dev->reg.version_len);
			ts_err("%*ph", (int)(dev->reg.version_len / 2),
			       temp_buf);
			ts_err("%*ph", (int)(dev->reg.version_len -
						dev->reg.version_len / 2),
				&temp_buf[dev->reg.version_len / 2]);

			if (version)
				version->valid = false;
			r = -EINVAL;
			goto exit;
		}
	}

	/*read pid*/
	memset(buffer, 0, sizeof(buffer));
	memset(version->pid, 0, sizeof(version->pid));
	r = goodix_i2c_read(dev, dev->reg.pid, buffer, pid_read_len);
	if (r < 0) {
		ts_err("Read pid failed");
		if (version)
			version->valid = false;
		goto exit;
	}

	/* check pid is digit or not, current we only support digital pid */
	if (!isdigit(buffer[0]) || !isdigit(buffer[1])) {
		ts_err("pid not digit: 0x%x,0x%x", buffer[0], buffer[1]);
		r = -EINVAL;
		goto exit;
	}

	memcpy(version->pid, buffer, pid_read_len);

	/*read vid*/
	memset(buffer, 0, sizeof(buffer));
	memset(version->vid, 0, sizeof(version->vid));
	r = goodix_i2c_read(dev, dev->reg.vid, buffer, vid_read_len);
	if (r < 0) {
		ts_err("Read vid failed");
		if (version)
			version->valid = false;
		goto exit;
	}
	memcpy(version->vid, buffer, vid_read_len);

	/*read sensor_id*/
	memset(buffer, 0, sizeof(buffer));
	r = goodix_i2c_read(dev, dev->reg.sensor_id, buffer, 1);
	if (r < 0) {
		ts_err("Read sensor_id failed");
		if (version)
			version->valid = false;
		goto exit;
	}
	if (sensor_id_mask != 0) {
		version->sensor_id = buffer[0] & sensor_id_mask;
		ts_info("sensor_id_mask:0x%02x, sensor_id:0x%02x",
			sensor_id_mask, version->sensor_id);
	} else {
		version->sensor_id = buffer[0];
	}

	version->valid = true;

	goodix_parse_customize_params(dev, version->sensor_id);

	ts_info("PID:%s,SensorID:%d, VID:%*ph", version->pid,
		version->sensor_id, (int)sizeof(version->vid), version->vid);
exit:
	/*enable doze mode, just valid for normandy
	 * this func must be used in pairs
	 */
	goodix_set_i2c_doze_mode(dev, true);

	return r;
}

static int goodix_wait_cfg_cmd_ready(struct goodix_ts_device *dev,
			u8 right_cmd, u8 send_cmd)
{
	int try_times = 0;
	u8 cmd_flag = 0;
	u8 cmd_buf[3] = {0};
	u16 command_reg = dev->reg.command;
	struct goodix_ts_cmd ts_cmd;

	goodix_cmd_init(dev, &ts_cmd, send_cmd, 0, command_reg);

	for (try_times = 0; try_times < TS_WAIT_CFG_READY_RETRY_TIMES;
	     try_times++) {
		if (goodix_i2c_read(dev, command_reg, cmd_buf, 3)) {
			ts_err("Read cmd_reg error");
			return -EINVAL;
		}
		cmd_flag = cmd_buf[0];
		if (cmd_flag == right_cmd) {
			return 0;
		} else if (cmd_flag != send_cmd) {
			ts_err("failed cmd_reg:0x%X, 0x%X, 0x%X",
			       cmd_buf[0], cmd_buf[1], cmd_buf[2]);
			if (goodix_send_command(dev, &ts_cmd)) {
				ts_err("Resend cmd 0x%02X FAILED", send_cmd);
				return -EINVAL;
			}
		}
		usleep_range(10000, 11000);
	}

	return -EINVAL;
}

static int _do_goodix_send_config(struct goodix_ts_device *dev,
		struct goodix_ts_config *config)
{
	int r = 0;
	int try_times = 0;
	u8 buf[3] = {0};
	u16 command_reg = dev->reg.command;
	u16 cfg_reg = dev->reg.cfg_addr;
	struct goodix_ts_cmd ts_cmd;

	/*1. Inquire command_reg until it's free*/
	for (try_times = 0; try_times < TS_WAIT_CMD_FREE_RETRY_TIMES;
	     try_times++) {
		if (!goodix_i2c_read(dev, command_reg, buf, 1) &&
		    buf[0] == TS_CMD_REG_READY)
			break;
		usleep_range(10000, 11000);
	}
	if (try_times >= TS_WAIT_CMD_FREE_RETRY_TIMES) {
		ts_err("failed send cfg, reg:0x%04x is not 0xff",
			command_reg);
		r = -EINVAL;
		goto exit;
	}

	/*2. send "start write cfg" command*/
	goodix_cmd_init(dev, &ts_cmd, COMMAND_START_SEND_CFG,
			 0, dev->reg.command);
	if (goodix_send_command(dev, &ts_cmd)) {
		ts_err("failed send cfg, COMMAND_START_SEND_CFG ERROR");
		r = -EINVAL;
		goto exit;
	}

	/*3. wait ic set command_reg to 0x82*/
	if (goodix_wait_cfg_cmd_ready(dev, COMMAND_SEND_CFG_PREPARE_OK,
				      COMMAND_START_SEND_CFG)) {
		ts_err("failed send cfg, reg:0x%04x is not 0x82",
			command_reg);
		r = -EINVAL;
		goto exit;
	}

	/*4. write cfg*/
	if (goodix_i2c_write(dev, cfg_reg, config->data, config->length)) {
		ts_err("Send cfg FAILED, write cfg to fw ERROR");
		r = -EINVAL;
		goto exit;
	}

	/*5. send "end send cfg" command*/
	goodix_cmd_init(dev, &ts_cmd, COMMAND_END_SEND_CFG,
			 0, dev->reg.command);
	if (goodix_send_command(dev, &ts_cmd)) {
		ts_err("failed send cfg, COMMAND_END_SEND_CFG ERROR");
		r = -EINVAL;
		goto exit;
	}

	if (dev->ic_type == IC_TYPE_YELLOWSTONE) {
		/*6. wait 0x7f or 0x7e */
		for (try_times = 0; try_times < TS_WAIT_CMD_FREE_RETRY_TIMES;
		     try_times++) {
			r = goodix_i2c_read(dev, command_reg, buf, 3);
			if (!r && (buf[0] == TS_CMD_CFG_ERR ||
				   buf[0] == TS_CMD_CFG_OK))
				break;
			usleep_range(10000, 11000);
		}
		ts_info("send config result: %*ph", 3, buf);
		/* set 0x7D to end send config process */
		goodix_cmd_init(dev, &ts_cmd, COMMAND_END_SEND_CFG_YS,
				 0, dev->reg.command);
		if (goodix_send_command(dev, &ts_cmd)) {
			ts_err("failed send cfg end cmd");
			r = -EINVAL;
			goto exit;
		}

		if (try_times >= TS_WAIT_CMD_FREE_RETRY_TIMES) {
			ts_err("failed get result");
			r = -EINVAL;
			goto exit;
		}
		if (buf[0] == TS_CMD_CFG_ERR) {
			if (buf[2] != TS_CFG_REPLY_DATA_EQU)
				ts_err("failed send cfg");
			else
				ts_info("config data equal with flash");
			r = -EINVAL;
			goto exit;
		}
	} else {
		/*6. wait ic set command_reg to 0xff*/
		for (try_times = 0; try_times < TS_WAIT_CMD_FREE_RETRY_TIMES;
		     try_times++) {
			if (!goodix_i2c_read(dev, command_reg, buf, 1) &&
			    buf[0] == TS_CMD_REG_READY)
				break;
			usleep_range(10000, 11000);
		}
		if (try_times >= TS_WAIT_CMD_FREE_RETRY_TIMES) {
			ts_err("failed send cfg, reg:0x%04x is 0x%x not 0xff",
				command_reg, buf[0]);
			r = -EINVAL;
			goto exit;
		}
	}

	ts_info("Send cfg SUCCESS");
	r = 0;

exit:
	return r;
}

/*static int goodix_check_cfg_valid(struct goodix_ts_device *dev, u8 *cfg, u32 length)
{
	int ret;
	u8 bag_num;
	u8 checksum;
	int i, j;
	int bag_start = 0;
	int bag_end = 0;

	if (!cfg || length < TS_CFG_HEAD_LEN) {
		ts_err("cfg is INVALID, len:%d", length);
		ret = -EINVAL;
		goto exit;
	}

	checksum = 0;
	for (i = 0; i < TS_CFG_HEAD_LEN; i++)
		checksum += cfg[i];
	if (checksum != 0) {
		ts_err("cfg head checksum ERROR, checksum:0x%02x",
			checksum);
		ret = -EINVAL;
		goto exit;
	}
	bag_num = cfg[TS_CFG_BAG_NUM_INDEX];
	bag_start = TS_CFG_BAG_START_INDEX;

	ts_info("cfg bag_num:%d, cfg length:%d", bag_num, length);
	for (j = 0; j < bag_num; j++) {
		if (bag_start >= length - 1) {
			ts_err("ERROR, overflow!!bag_start:%d, cfg_len:%d",
				bag_start, length);
			ret = -EINVAL;
			goto exit;
		}

		bag_end = bag_start + cfg[bag_start + 1] + 3;

		checksum = 0;
		if (bag_end > length) {
			ts_err("ERROR, overflow!!bag:%d, bag_start:%d,"
				"bag_end:%d, cfg length:%d",
				j, bag_start, bag_end, length);
			ret = -EINVAL;
			goto exit;
		}
		for (i = bag_start; i < bag_end; i++)
			checksum += cfg[i];
		if (checksum != 0) {
			ts_err("cfg INVALID, bag:%d checksum ERROR:0x%02x",
			       j, checksum);
			ret = -EINVAL;
			goto exit;
		}
		bag_start = bag_end;
	}

	ret = 0;
	ts_info("configuration check SUCCESS");

exit:
	return ret;
}*/

static int goodix_send_config(struct goodix_ts_device *dev,
		struct goodix_ts_config *config)
{
	int r = 0;

	if (!config || !config->initialized) {
		ts_err("invalid config data");
		return -EINVAL;
	}

	/*check configuration valid*/
	/* TODO remove this
	r = goodix_check_cfg_valid(dev, config->data, config->length);
	if (r != 0) {
	 	ts_err("cfg check FAILED");
	 	return -EINVAL;
	}
	*/
	ts_info("ver:%02xh,size:%d", config->data[0], config->length);
	mutex_lock(&config->lock);

	/*disable doze mode*/
	if (!goodix_set_i2c_doze_mode(dev, false)) {
		r = _do_goodix_send_config(dev, config);
	} else {
		ts_err("failed disable doze[abort]");
		r = -EINVAL;
	}
	/*enable doze mode*/
	goodix_set_i2c_doze_mode(dev, true);

	mutex_unlock(&config->lock);
	return r;
}

/* success return config length else return -1 */
static int goodix_read_config_ys(struct goodix_ts_device *dev,
				u8 *buf)
{
	u32 cfg_addr = dev->reg.cfg_addr;
	int sub_bags = 0;
	int offset = 0;
	int subbag_len;
	u16 checksum;
	int i;
	int ret;

	ret = goodix_i2c_read(dev, cfg_addr, buf, TS_CFG_HEAD_LEN_YS);
	if (ret)
		goto err_out;

	offset = TS_CFG_HEAD_LEN_YS;
	sub_bags = buf[TS_CFG_BAG_NUM_INDEX];
	checksum = checksum_u8_ys(buf, TS_CFG_HEAD_LEN_YS);
	if (checksum) {
		ts_err("Config head checksum err:0x%x,data:%*ph",
				checksum, TS_CFG_HEAD_LEN_YS, buf);
		ret = -EINVAL;
		goto err_out;
	}

	ts_info("config_version:%u, vub_bags:%u", buf[0], sub_bags);
	for (i = 0; i < sub_bags; i++) {
		/* read sub head [0]: sub bag num, [1]: sub bag length */
		ret = goodix_i2c_read(dev, cfg_addr + offset, buf + offset, 2);
		if (ret)
			goto err_out;

		/* read sub bag data */
		subbag_len = buf[offset + 1];

		ts_debug("sub bag num:%u,sub bag length:%u",
			 buf[offset], subbag_len);
		ret = goodix_i2c_read(dev, cfg_addr + offset + 2,
				      buf + offset + 2, subbag_len + 2);
		if (ret)
			goto err_out;
		checksum = checksum_u8_ys(buf + offset, subbag_len + 4);
		if (checksum) {
			ts_err("sub bag checksum err:0x%x", checksum);
			ret = -EINVAL;
			goto err_out;
		}
		offset += subbag_len + 4;
		ts_debug("sub bag %d, data:%*ph",
			 buf[offset], buf[offset + 1] + 4, buf + offset);
	}
	ret = offset;

err_out:
	return ret;
}

/* success return config length else return -1 */
static int goodix_read_config_nor(struct goodix_ts_device *dev,
				u8 *buf)
{
	u32 cfg_addr = dev->reg.cfg_addr;
	int sub_bags = 0;
	int offset = 0;
	int subbag_len;
	u8 checksum;
	int i;
	int ret;

	/*disable doze mode*/
	if (goodix_set_i2c_doze_mode(dev, false)) {
		ts_err("failed disable doze mode[abort]");
		ret = -EINVAL;
		goto err_out;
	}

	ret = goodix_i2c_read(dev, cfg_addr, buf, TS_CFG_HEAD_LEN);
	if (ret)
		goto err_out;

	offset = TS_CFG_BAG_START_INDEX;
	sub_bags = buf[TS_CFG_BAG_NUM_INDEX];
	checksum = checksum_u8(buf, TS_CFG_HEAD_LEN);
	if (checksum) {
		ts_err("Config head checksum err:0x%x,data:%*ph",
				checksum, TS_CFG_HEAD_LEN, buf);
		ret = -EINVAL;
		goto err_out;
	}

	ts_info("config_version:%u, vub_bags:%u", buf[0], sub_bags);
	for (i = 0; i < sub_bags; i++) {
		/* read sub head [0]: sub bag num, [1]: sub bag length */
		ret = goodix_i2c_read(dev, cfg_addr + offset, buf + offset, 2);
		if (ret)
			goto err_out;

		/* read sub bag data */
		subbag_len = buf[offset + 1];

		ts_debug("sub bag num:%u,sub bag length:%u",
			 buf[offset], subbag_len);
		ret = goodix_i2c_read(dev, cfg_addr + offset + 2,
				      buf + offset + 2, subbag_len + 1);
		if (ret)
			goto err_out;
		checksum = checksum_u8(buf + offset, subbag_len + 3);
		if (checksum) {
			ts_err("sub bag checksum err:0x%x", checksum);
			ret = -EINVAL;
			goto err_out;
		}
		offset += subbag_len + 3;
		ts_debug("sub bag %d, data:%*ph",
			 buf[offset], buf[offset + 1] + 3, buf + offset);
	}
	ret = offset;

err_out:
	/*enable doze mode*/
	goodix_set_i2c_doze_mode(dev, true);

	return ret;
}

/* success return config_len, <= 0 failed */
static int goodix_read_config(struct goodix_ts_device *dev,
			      u8 *config_data)
{
	struct goodix_ts_cmd ts_cmd;
	u8 cmd_flag;
	u32 cmd_reg = dev->reg.command;
	int r = 0;
	int i;

	if (!config_data) {
		ts_err("Illegal params");
		return -EINVAL;
	}
	if (!dev->reg.command) {
		ts_err("command register ERROR:0x%04x", dev->reg.command);
		return -EINVAL;
	}

	/*disable doze mode*/
	if (goodix_set_i2c_doze_mode(dev, false)) {
		ts_err("failed disabled doze[abort]");
		r = -EINVAL;
		goto exit;
	}

	/* wait for IC in IDLE state */
	for (i = 0; i < TS_WAIT_CMD_FREE_RETRY_TIMES; i++) {
		cmd_flag = 0;
		r = goodix_i2c_read(dev, cmd_reg, &cmd_flag, 1);
		if (r < 0 || cmd_flag == TS_CMD_REG_READY)
			break;
		usleep_range(10000, 11000);
	}
	if (cmd_flag != TS_CMD_REG_READY) {
		ts_err("Wait for IC ready IDEL state timeout:addr 0x%x\n",
		       cmd_reg);
		r = -EAGAIN;
		goto exit;
	}
	/* 0x86 read config command */
	goodix_cmd_init(dev, &ts_cmd, COMMAND_START_READ_CFG,
			 0, cmd_reg);
	r = goodix_send_command(dev, &ts_cmd);
	if (r) {
		ts_err("Failed send read config command");
		goto exit;
	}
	/* wait for config data ready */
	if (goodix_wait_cfg_cmd_ready(dev, COMMAND_READ_CFG_PREPARE_OK,
				      COMMAND_START_READ_CFG)) {
		ts_err("Wait for config data ready timeout");
		r = -EAGAIN;
		goto exit;
	}

	if (dev->ic_type == IC_TYPE_YELLOWSTONE)
		r = goodix_read_config_ys(dev, config_data);
	else
		r = goodix_read_config_nor(dev, config_data);
	if (r <= 0)
		ts_err("Failed read config data");

	/* clear command */
	goodix_cmd_init(dev, &ts_cmd, TS_CMD_REG_READY, 0, cmd_reg);
	goodix_send_command(dev, &ts_cmd);

exit:
	/*enable doze mode*/
	goodix_set_i2c_doze_mode(dev, true);

	return r;
}

/**
 * goodix_hw_reset - reset device
 *
 * @dev: pointer to touch device
 * Returns 0 - succeed,<0 - failed
 */
int goodix_hw_reset(struct goodix_ts_device *dev)
{
	u8 data[2] = {0x00};
	int r = 0;

	ts_info("HW reset");

	gpio_direction_output(dev->board_data.reset_gpio, 0);
	udelay(2000);
	gpio_direction_output(dev->board_data.reset_gpio, 1);
	msleep(100);

	/*init dynamic esd*/
	if (dev->reg.esd) {
		r = goodix_i2c_write_trans(dev, dev->reg.esd, data, 1);
		if (r < 0)
			ts_err("IC reset, init dynamic esd FAILED");
	} else {
		ts_info("reg.esd is NULL, skip dynamic esd init");
	}

	return 0;
}

/**
 * goodix_request_handler - handle firmware request
 *
 * @dev: pointer to touch device
 * @request_data: requset information
 * Returns 0 - succeed,<0 - failed
 */
static int goodix_request_handler(struct goodix_ts_device *dev)
{
	unsigned char buffer[1];
	int r;

	r = goodix_i2c_read_trans(dev, dev->reg.fw_request, buffer, 1);
	if (r < 0)
		return r;

	switch (buffer[0]) {
	case REQUEST_CONFIG:
		ts_info("HW request config");
		r = goodix_send_config(dev, &(dev->normal_cfg));
		if (r != 0)
			ts_info("request config, send config faild");
	break;
	case REQUEST_BAKREF:
		ts_info("HW request bakref");
	break;
	case REQUEST_RESET:
		ts_info("HW requset reset");
		r = goodix_hw_reset(dev);
		if (r != 0)
			ts_info("request reset, reset faild");
	break;
	case REQUEST_RELOADFW:
		ts_info("HW request reload fw");
		goodix_do_fw_update(UPDATE_MODE_FORCE|UPDATE_MODE_SRC_REQUEST);
	break;
	case REQUEST_IDLE:
		ts_info("HW request idle");
	break;
	default:
		ts_info("Unknown hw request:%d", buffer[0]);
	break;
	}

	buffer[0] = 0x00;
	r = goodix_i2c_write_trans(dev, dev->reg.fw_request, buffer, 1);
	return r;
}

static void goodix_swap_coords(struct goodix_ts_device *dev,
		unsigned int *coor_x, unsigned int *coor_y)
{
	unsigned int temp;
	struct goodix_ts_board_data *bdata = &dev->board_data;

	if (bdata->swap_axis) {
		temp = *coor_x;
		*coor_x = *coor_y;
		*coor_y = temp;
	}
	if (bdata->x2x)
		*coor_x = bdata->panel_max_x - *coor_x;
	if (bdata->y2y)
		*coor_y = bdata->panel_max_y - *coor_y;
}

#define GOODIX_KEY_STATE 	0x10
static void goodix_parse_finger_nor(struct goodix_ts_device *dev,
	struct goodix_touch_data *touch_data, unsigned char *buf, int touch_num)
{
	unsigned int id = 0, x = 0, y = 0, w = 0;
	static u8 pre_key_map;
	u8 cur_key_map = 0;
	static u32 pre_finger_map;
	u32 cur_finger_map = 0;
	u8 *coor_data;
	int i;

	coor_data = &buf[IRQ_HEAD_LEN_NOR];
	for (i = 0; i < touch_num; i++) {
		id = coor_data[0];
		if(id >= GOODIX_MAX_TOUCH){
			ts_info("invaild finger id =%d", id);
			break;
		}
		x = le16_to_cpup((__be16 *) (coor_data + 1));
		y = le16_to_cpup((__be16 *) (coor_data + 3));
		w = coor_data[5];
		goodix_swap_coords(dev, &x, &y);
		touch_data->coords[id].status = TS_TOUCH;
		touch_data->coords[id].x = x;
		touch_data->coords[id].y = y;
		touch_data->coords[id].w = w;
		cur_finger_map |= (1 << id);
		coor_data += BYTES_PER_COORD;
	}

	/* process finger release */
	for (i = 0; i < GOODIX_MAX_TOUCH; i++) {
		if (cur_finger_map & (1 << i))
			continue;
		if (pre_finger_map & (1 << i))
			touch_data->coords[i].status = TS_RELEASE;
	}
	pre_finger_map = cur_finger_map;
	touch_data->touch_num = touch_num;

	if (buf[1] & GOODIX_KEY_STATE) {
		/* have key */
		cur_key_map = buf[touch_num * BYTES_PER_COORD + 2] & 0x0F;
		for (i = 0; i < GOODIX_MAX_TP_KEY; i++) {
			if (cur_key_map & (1 << i)) {
				touch_data->keys[i].status = TS_TOUCH;
				touch_data->keys[i].code =
					dev->board_data.panel_key_map[i];
			}
		}
	}
	/* process key release */
	for (i = 0; i < GOODIX_MAX_TP_KEY; i++) {
		if (cur_key_map & (1 << i) || !(pre_key_map & (1 << i)))
			continue;
		touch_data->keys[i].status = TS_RELEASE;
		touch_data->keys[i].code = dev->board_data.panel_key_map[i];
	}
	pre_key_map = cur_key_map;
}

static void goodix_parse_finger_ys(struct goodix_ts_device *dev,
	struct goodix_touch_data *touch_data, unsigned char *buf, int touch_num)
{
	unsigned int id = 0, x = 0, y = 0, w = 0, overlapping_area = 0;
	static u32 pre_finger_map;
	u32 cur_finger_map = 0;
	u8 *coor_data;
	int i;

	coor_data = &buf[IRQ_HEAD_LEN_YS];
	overlapping_area = coor_data[BYTES_PER_COORD*touch_num];
	for (i = 0; i < touch_num; i++) {
		id = (coor_data[0] >> 4) & 0x0F;
		if(id >= GOODIX_MAX_TOUCH){
			ts_info("invaild finger id =%d", id);
			break;
		}
		x = be16_to_cpup((__be16 *)(coor_data + 2));
		y = be16_to_cpup((__be16 *)(coor_data + 4));
		w = be16_to_cpup((__be16 *)(coor_data + 6));
		goodix_swap_coords(dev, &x, &y);
		touch_data->coords[id].status = TS_TOUCH;
		touch_data->coords[id].x = x;
		touch_data->coords[id].y = y;
		touch_data->coords[id].w = w;
		touch_data->coords[id].overlapping_area = overlapping_area;
		cur_finger_map |= (1 << id);
		coor_data += BYTES_PER_COORD;
	}

	/* process finger release */
	for (i = 0; i < GOODIX_MAX_TOUCH; i++) {
		if (cur_finger_map & (1 << i))
			continue;
		if (pre_finger_map & (1 << i))
			touch_data->coords[i].status = TS_RELEASE;
	}
	pre_finger_map = cur_finger_map;
	touch_data->touch_num = touch_num;
}

static unsigned int goodix_pen_btn_code[] = {BTN_STYLUS, BTN_STYLUS2};
static void goodix_parse_pen_nor(struct goodix_ts_device *dev,
	struct goodix_pen_data *pen_data, unsigned char *buf, int touch_num)
{
	unsigned int id = 0;
	static u8 pre_key_map;
	u8 cur_key_map = 0;
	static u32 pre_pen_status;
	u32 cur_pen_status = 0;
	u8 *coor_data;
	int i;

	coor_data = &buf[2];
	for (i = 0; i < touch_num; i++) {
		/* search for pen coordinate */
		id = coor_data[0];
		if (id < 0x80) {
			coor_data += BYTES_PER_COORD;
			continue;
		}
		pen_data->coords.x = le16_to_cpup((__be16 *)(coor_data + 1));
		pen_data->coords.y = le16_to_cpup((__be16 *)(coor_data + 3));
		pen_data->coords.p = le16_to_cpup((__be16 *)(coor_data + 5));
		goodix_swap_coords(dev, &pen_data->coords.x,
				   &pen_data->coords.y);
		pen_data->coords.status = TS_TOUCH;
		pen_data->coords.tool_type = BTN_TOOL_PEN;
		cur_pen_status = 1;
		/* currently only support one stylus */
		break;
	}
	if (!cur_pen_status && pre_pen_status) {
		pen_data->coords.status = TS_RELEASE;
	}
	pre_pen_status = cur_pen_status;

	/* process pen button */
	if (buf[1] & GOODIX_KEY_STATE) {
		cur_key_map = (buf[touch_num * BYTES_PER_COORD + 2] >> 4) & 0x0F;
		for (i = 0; i < GOODIX_MAX_PEN_KEY; i++) {
			if (!(cur_key_map & (1 << i)))
				continue;
			pen_data->keys[i].status = TS_TOUCH;
			pen_data->keys[i].code = goodix_pen_btn_code[i];
		}
	}
	for (i = 0; i < GOODIX_MAX_PEN_KEY; i++) {
		if (cur_key_map & (1 << i) || !(pre_key_map & (1 << i)))
			continue;
		pen_data->keys[i].status = TS_RELEASE;
		pen_data->keys[i].code = goodix_pen_btn_code[i];
	}
	pre_key_map = cur_key_map;
}

static void goodix_parse_pen_ys(struct goodix_ts_device *dev,
	struct goodix_pen_data *pen_data, unsigned char *buf, int touch_num)
{
	ts_info("unsupported");
}

static int goodix_touch_handler_ys(struct goodix_ts_device *dev,
		struct goodix_ts_event *ts_event,
		u8 *pre_buf, u32 pre_buf_len)
{
	struct goodix_touch_data *touch_data = &ts_event->touch_data;
	struct goodix_pen_data *pen_data = &ts_event->pen_data;
	static u8 buffer[IRQ_HEAD_LEN_YS +
			 BYTES_PER_COORD * GOODIX_MAX_TOUCH + 2];
	int touch_num = 0, r = -EINVAL;
	u8 point_type = 0;
	u16 chksum = 0;

	static u8 pre_finger_num = 0;
	static u8 pre_pen_num = 0;

	/* clean event buffer */
	memset(ts_event, 0, sizeof(*ts_event));
	/* copy pre-data to buffer */
	memcpy(buffer, pre_buf, pre_buf_len);

	touch_num = buffer[2] & 0x0F;

	if (unlikely(touch_num > GOODIX_MAX_TOUCH)) {
		touch_num = -EINVAL;
		goto exit_clean_sta;
	}
	if (unlikely(touch_num > 1)) {
		r = goodix_i2c_read_trans(dev,
				dev->reg.coor + pre_buf_len,
				&buffer[pre_buf_len],
				(touch_num - 1) * BYTES_PER_COORD);
		if (unlikely(r < 0))
			goto exit_clean_sta;
	}

	if (touch_num > 0) {
		chksum = checksum_u8_ys(&buffer[IRQ_HEAD_LEN_YS],
					touch_num * BYTES_PER_COORD + 2);
		if (unlikely(chksum != 0)) {
			ts_debug("checksum error:%x", chksum);
			r = -EINVAL;
			goto exit_clean_sta;
		}
	}
	point_type = buffer[(touch_num - 1) * BYTES_PER_COORD + IRQ_HEAD_LEN_YS];
	if (touch_num >= 1 && (point_type == 3 || point_type == 1)) {
		if (pre_finger_num) {
			ts_event->event_type = EVENT_TOUCH;
			goodix_parse_finger_ys(dev, touch_data, buffer, 0);
			pre_finger_num = 0;
		} else {
			pre_pen_num = 1;
			ts_event->event_type = EVENT_PEN;
			goodix_parse_pen_ys(dev, pen_data, buffer, touch_num);
		}
	} else {
		if (pre_pen_num) {
			ts_event->event_type = EVENT_PEN;
			goodix_parse_pen_ys(dev, pen_data, buffer, 0);
			pre_pen_num = 0;
		} else {
			ts_event->event_type = EVENT_TOUCH;
			goodix_parse_finger_ys(dev, touch_data,
					    buffer, touch_num);
			pre_finger_num = touch_num;
		}
	}

	/* process custom info */
	if (buffer[3] & 0x01) {
		ts_debug("TODO add custom info process function");
	}
exit_clean_sta:
	return r;
}

static int goodix_touch_handler_nor(struct goodix_ts_device *dev,
		struct goodix_ts_event *ts_event,
		u8 *pre_buf, u32 pre_buf_len)
{
	struct goodix_touch_data *touch_data = &ts_event->touch_data;
	struct goodix_pen_data *pen_data = &ts_event->pen_data;
	static u8 buffer[IRQ_HEAD_LEN_NOR +
			 BYTES_PER_COORD * GOODIX_MAX_TOUCH + 2];
	int touch_num = 0, r = -EINVAL;
	unsigned char chksum = 0;

	static u8 pre_finger_num = 0;
	static u8 pre_pen_num = 0;

	/* clean event buffer */
	memset(ts_event, 0, sizeof(*ts_event));
	/* copy pre-data to buffer */
	memcpy(buffer, pre_buf, pre_buf_len);

	touch_num = buffer[1] & 0x0F;

	if (unlikely(touch_num > GOODIX_MAX_TOUCH)) {
		touch_num = -EINVAL;
		goto exit_clean_sta;
	}
	if (unlikely(touch_num > 1)) {
		r = goodix_i2c_read_trans(dev,
				dev->reg.coor + pre_buf_len,
				&buffer[pre_buf_len],
				(touch_num - 1) * BYTES_PER_COORD);
		if (unlikely(r < 0))
			goto exit_clean_sta;
	}

	chksum = checksum_u8(&buffer[0], touch_num * BYTES_PER_COORD + 4);
	if (unlikely(chksum != 0)) {
		ts_debug("checksum error:%X, ic_type:%d", chksum, dev->ic_type);
		r = -EINVAL;
		goto exit_clean_sta;
	}

	if (touch_num >= 1 &&
	    buffer[(touch_num - 1) * BYTES_PER_COORD + 2] >= 0x80) {
		if (pre_finger_num) {
			ts_event->event_type = EVENT_TOUCH;
			goodix_parse_finger_nor(dev, touch_data, buffer, 0);
			pre_finger_num = 0;
		} else {
			pre_pen_num = 1;
			ts_event->event_type = EVENT_PEN;
			goodix_parse_pen_nor(dev, pen_data, buffer, touch_num);
		}
	} else {
		if (pre_pen_num) {
			ts_event->event_type = EVENT_PEN;
			goodix_parse_pen_nor(dev, pen_data, buffer, 0);
			pre_pen_num = 0;
		} else {
			ts_event->event_type = EVENT_TOUCH;
			goodix_parse_finger_nor(dev, touch_data,
					    buffer, touch_num);
			pre_finger_num = touch_num;
		}
	}
exit_clean_sta:
	return r;
}

static int goodix_event_handler(struct goodix_ts_device *dev,
		struct goodix_ts_event *ts_event)
{
	int pre_read_len;
	u8 pre_buf[32];
	u8 event_sta;
	struct i2c_client *client = to_i2c_client(dev->dev);
	struct goodix_ts_core *core_data = i2c_get_clientdata(client);
	int r;

	if (dev->ic_type == IC_TYPE_YELLOWSTONE)
		pre_read_len = IRQ_HEAD_LEN_YS + BYTES_PER_COORD + 2;
	else
		pre_read_len = IRQ_HEAD_LEN_NOR + BYTES_PER_COORD + 2;
	r = goodix_i2c_read_trans(dev, dev->reg.coor,
				  pre_buf, pre_read_len);
	if (unlikely(r < 0))
		return r;

	if (dev->ic_type == IC_TYPE_YELLOWSTONE &&
	    checksum_u8_ys(pre_buf, IRQ_HEAD_LEN_YS)) {
		ts_debug("irq head checksum error %*ph",
			IRQ_HEAD_LEN_YS, pre_buf);
		return -EINVAL;
	}
	/* buffer[0]: event state */
	event_sta = pre_buf[0];
	core_data->event_status = pre_buf[0];
	ts_info("event status is: 0x%02X", pre_buf[0]);
	if (likely((event_sta & GOODIX_TOUCH_EVENT) == GOODIX_TOUCH_EVENT)) {
		/* handle touch event */
		if (dev->ic_type == IC_TYPE_YELLOWSTONE)
			goodix_touch_handler_ys(dev, ts_event, pre_buf,
				     		pre_read_len);
		else
			goodix_touch_handler_nor(dev, ts_event, pre_buf,
				     		 pre_read_len);
	} else if (unlikely((event_sta & GOODIX_REQUEST_EVENT) ==
			     GOODIX_REQUEST_EVENT)) {
		/* handle request event */
		ts_event->event_type = EVENT_REQUEST;
		goodix_request_handler(dev);
	} else if ((event_sta & GOODIX_GESTURE_EVENT) ==
		   GOODIX_GESTURE_EVENT) {
		/* handle gesture event */
		ts_debug("Gesture event");
	} else if ((event_sta & GOODIX_HOTKNOT_EVENT) ==
		   GOODIX_HOTKNOT_EVENT) {
		/* handle hotknot event */
		ts_debug("Hotknot event");
	} else {
		ts_debug("unknow event type:0x%x", event_sta);
		r = -EINVAL;
	}

	return r;
}

/**
 * goodix_hw_suspend - Let touch deivce stay in lowpower mode.
 * @dev: pointer to goodix touch device
 * @return: 0 - succeed, < 0 - failed
 */
static int goodix_hw_suspend(struct goodix_ts_device *dev)
{
	struct goodix_ts_cmd sleep_cmd;
	int r = 0;

	goodix_cmd_init(dev, &sleep_cmd, COMMAND_SLEEP,
			 0, dev->reg.command);
	if (sleep_cmd.initialized) {
		r = goodix_send_command(dev, &sleep_cmd);
		if (!r)
			ts_info("Chip in sleep mode");
	} else {
		ts_err("Uninitialized sleep command");
	}
	return r;
}

/**
 * goodix_hw_resume - Let touch deivce stay in active  mode.
 * @dev: pointer to goodix touch device
 * @return: 0 - succeed, < 0 - failed
 */
static int goodix_hw_resume(struct goodix_ts_device *dev)
{
	goodix_hw_reset(dev);

	return 0;
}

static int goodix_esd_check(struct goodix_ts_device *dev)
{
	int r;
	u8 data = 0;

	if (dev->reg.esd == 0) {
		ts_err("esd reg is NULL");
		return 0;
	}

	/*check dynamic esd*/
	r = dev->hw_ops->read_trans(dev, TS_REG_ESD_TICK_R, &data, 1);

	if (r < 0 || (data == GOODIX_ESD_TICK_WRITE_DATA)) {
		ts_info("dynamic esd occur, r:%d, data:0x%02x", r, data);
		r = -EINVAL;
		goto exit;
	}

exit:
	return r;
}

/* hardware opeation funstions */
static const struct goodix_ts_hw_ops hw_i2c_ops = {
	.dev_confirm = goodix_ts_dev_confirm,
	.read = goodix_i2c_read,
	.write = goodix_i2c_write,
	.read_trans = goodix_i2c_read_trans,
	.write_trans = goodix_i2c_write_trans,
	.reset = goodix_hw_reset,
	.event_handler = goodix_event_handler,
	.send_config = goodix_send_config,
	.read_config = goodix_read_config,
	.send_cmd = goodix_send_command,
	.read_version = goodix_read_version,
	.suspend = goodix_hw_suspend,
	.resume = goodix_hw_resume,
	.check_hw = goodix_esd_check,
};

static struct platform_device *goodix_pdev;

static void goodix_pdev_release(struct device *dev)
{
	ts_info("goodix pdev released");
}

static int goodix_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *dev_id)
{
	struct goodix_ts_device *ts_device = NULL;
	int r = 0;

	ts_info("goodix_i2c_probe IN");

	r = i2c_check_functionality(client->adapter,
		I2C_FUNC_I2C);
	if (!r)
		return -EIO;

	/* ts device data */
	ts_device = devm_kzalloc(&client->dev,
		sizeof(struct goodix_ts_device), GFP_KERNEL);
	if (!ts_device)
		return -ENOMEM;

	if (IS_ENABLED(CONFIG_OF) && client->dev.of_node) {
		/* parse devicetree property */
		r = goodix_parse_dt(client->dev.of_node,
				    &ts_device->board_data);
		if (r < 0) {
			ts_err("failed parse device info form dts, %d", r);
			return -EINVAL;
		}
	} else {
		ts_err("no valid device tree node found");
		return -ENODEV;
	}

	ts_device->name = "Goodix TouchDevcie";
	ts_device->dev = &client->dev;
	ts_device->hw_ops = &hw_i2c_ops;

	/* ts core device */
	goodix_pdev = kzalloc(sizeof(struct platform_device), GFP_KERNEL);
	if (!goodix_pdev)
		return -ENOMEM;

	goodix_pdev->name = GOODIX_CORE_DRIVER_NAME;
	goodix_pdev->id = 0;
	goodix_pdev->num_resources = 0;
	/*
	 * you can find this platform dev in
	 * /sys/devices/platfrom/goodix_ts.0
	 * goodix_pdev->dev.parent = &client->dev;
	 */
	goodix_pdev->dev.platform_data = ts_device;
	goodix_pdev->dev.release = goodix_pdev_release;

	/* register platform device, then the goodix_ts_core
	 * module will probe the touch deivce.
	 */
	r = platform_device_register(goodix_pdev);
	if (r) {
		ts_err("failed register goodix platform device, %d", r);
		goto err_pdev;
	}
	r = goodix_ts_core_init();
	if (r) {
		ts_err("failed register platform driver, %d", r);
		goto err_pdriver;
	}
	ts_info("i2c probe out");
	return r;

err_pdriver:
	platform_device_unregister(goodix_pdev);
err_pdev:
	kfree(goodix_pdev);
	goodix_pdev = NULL;
	ts_info("i2c probe out, %d", r);
	return r;
}

static int goodix_i2c_remove(struct i2c_client *client)
{
	if (goodix_pdev) {
		platform_device_unregister(goodix_pdev);
		kfree(goodix_pdev);
		goodix_pdev = NULL;
	}

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id i2c_matchs[] = {
	{.compatible = "goodix,gt9896",},
	{.compatible = "goodix,gt9886",},
	{.compatible = "goodix,gt9889",},
	{.compatible = "goodix,gt5863",},
	{},
};
MODULE_DEVICE_TABLE(of, i2c_matchs);
#endif

static const struct i2c_device_id i2c_id_table[] = {
	{TS_DRIVER_NAME, 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, i2c_id_table);

static struct i2c_driver goodix_i2c_driver = {
	.driver = {
		.name = TS_DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(i2c_matchs),
	},
	.probe = goodix_i2c_probe,
	.remove = goodix_i2c_remove,
	.id_table = i2c_id_table,
};

/* release manully when prob failed */
void goodix_ts_dev_release(void)
{
	if (goodix_pdev) {
		platform_device_unregister(goodix_pdev);
		kfree(goodix_pdev);
		goodix_pdev = NULL;
	}
	i2c_del_driver(&goodix_i2c_driver);
}

static int __init goodix_i2c_init(void)
{
	ts_info("Goodix driver init");
	return i2c_add_driver(&goodix_i2c_driver);
}

static void __exit goodix_i2c_exit(void)
{
	i2c_del_driver(&goodix_i2c_driver);
	ts_info("Goodix driver exit");
}

late_initcall(goodix_i2c_init);
module_exit(goodix_i2c_exit);

MODULE_DESCRIPTION("Goodix Touchscreen Hardware Module");
MODULE_AUTHOR("Goodix, Inc.");
MODULE_LICENSE("GPL v2");
