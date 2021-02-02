/*
 * Goodix GTx5 I2C Dirver
 * Hardware interface layer of touchdriver architecture.
 *
 * Copyright (C) 2015 - 2016 Goodix, Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 * Authors:  Yulong Cai <caiyulong@goodix.com>
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
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include "goodix_ts_core.h"
#include "goodix_cfg_bin.h"
#include <linux/input/mt.h>
#include <linux/input.h>
#include "../xiaomi/xiaomi_touch.h"

#define TS_DT_COMPATIBLE				"goodix,gt9886"
#define TS_DRIVER_NAME					"goodix_i2c"
#define I2C_MAX_TRANSFER_SIZE			256
#define TS_ADDR_LENGTH					2
#define TS_DOZE_ENABLE_RETRY_TIMES		10
#define TS_DOZE_DISABLE_RETRY_TIMES		10
#define TS_WAIT_CFG_READY_RETRY_TIMES	30
#define TS_WAIT_CMD_FREE_RETRY_TIMES	10

#define TS_REG_COORDS_BASE				0x824E
#define TS_REG_CMD						0x8040
#define TS_REG_REQUEST					0x8044
#define TS_REG_VERSION					0x8240
#define TS_REG_CFG_BASE					0x8050
#define TS_REG_DOZE_CTRL				0x30F0
#define TS_REG_DOZE_STAT				0x3100
#define TS_REG_ESD_TICK_R				0x3103

#define CFG_XMAX_OFFSET					(0x8052 - 0x8050)
#define CFG_YMAX_OFFSET					(0x8054 - 0x8050)

#define REQUEST_HANDLED					0x00
#define REQUEST_CONFIG					0x01
#define REQUEST_BAKREF					0x02
#define REQUEST_RESET					0x03
#define REQUEST_MAINCLK					0x04
#define REQUEST_IDLE					0x05

#define COMMAND_SLEEP					0x05
#define COMMAND_CLOSE_HID				0xaa
#define COMMAND_START_SEND_CFG			0x80
#define COMMAND_END_SEND_CFG			0x83
#define COMMAND_SEND_SMALL_CFG			0x81
#define COMMAND_SEND_CFG_PREPARE_OK		0x82
#define COMMAND_START_READ_CFG			0x86
#define COMMAND_READ_CFG_PREPARE_OK		0x85

#define BYTES_PER_COORD					8
#define TS_MAX_SENSORID					5
#define TS_CFG_MAX_LEN					1024
#define TS_CFG_HEAD_LEN					4
#define TS_CFG_BAG_NUM_INDEX			2
#define TS_CFG_BAG_START_INDEX			4
#if TS_CFG_MAX_LEN > GOODIX_CFG_MAX_SIZE
#error GOODIX_CFG_MAX_SIZE too small, please fix.
#endif

#define TS_DOZE_DISABLE_DATA			0xAA
#define TS_DOZE_CLOSE_OK_DATA			0xBB
#define TS_DOZE_ENABLE_DATA				0xCC
#define	TS_CMD_REG_READY				0xFF

#ifdef CONFIG_OF
int pre_event;

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

	r = of_property_read_u32(node, "goodix,panel-max-id",
				 &board_data->panel_max_id);
	if (r) {
		err = -ENOENT;
	} else {
		if (board_data->panel_max_id > GOODIX_MAX_TOUCH)
			board_data->panel_max_id = GOODIX_MAX_TOUCH;
	}

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

	r = of_property_read_u32(node, "goodix,panel-max-p",
				 &board_data->panel_max_p);
	if (r)
		err = -ENOENT;

	r = of_property_read_u32(node, "goodix,panel-max-fod",
				 &board_data->panel_max_overlapping_area);
	if (r)
		err = -ENOENT;

	board_data->swap_axis = of_property_read_bool(node, "goodix,swap-axis");

	board_data->x2x = of_property_read_bool(node, "goodix,x2x");

	board_data->y2y = of_property_read_bool(node, "goodix,y2y");

	return 0;
}

#ifdef USE_NEW_VERSION_EDGE_FILTER
/**
 * edge_filter_checksume_and_cmd - calcuate the checksum and save data as u8 array
 * @board_data - the value where parse data to saved in
 */
int edge_filter_checksume_and_cmd(struct goodix_ts_board_data *board_data)
{
	int i = 0;

	/* cmd parameters */
	board_data->edge_filter_params.length = 120;
	board_data->edge_filter_params_cmd_length = 2 + board_data->edge_filter_params.length + 4;
	if (board_data->edge_filter_params_cmd == NULL) {
		board_data->edge_filter_params_cmd =
			(u8 *) kmalloc(board_data->edge_filter_params_cmd_length, GFP_KERNEL);
		if (!board_data->edge_filter_params_cmd) {
			ts_err("malloc edge_filter_params_cmd error, exit!");
			return -EINVAL;
		}
	}

	/* add length */
	board_data->edge_filter_params_cmd[0] = (board_data->edge_filter_params.length >> 8) & 0xFF;
	board_data->edge_filter_params_cmd[1] = (board_data->edge_filter_params.length) & 0xFF;

	/* add parameters */
	for (i = 2; i < board_data->edge_filter_params.length + 2; i += 1) {
		if( i < 4 * GRIP_PARAMETER_NUM + 2){
			board_data->edge_filter_params_cmd[i] = board_data->edge_filter_params.cornerzone_filter[i -2];
		} else if (i < 2 * 4 * GRIP_PARAMETER_NUM + 2){
			board_data->edge_filter_params_cmd[i] = board_data->edge_filter_params.edgezone_filter[i - 4 * GRIP_PARAMETER_NUM - 2];
		} else if (i < 3 * 4 * GRIP_PARAMETER_NUM + 2){
			board_data->edge_filter_params_cmd[i] = board_data->edge_filter_params.deadzone_filter[i - 2 * 4 * GRIP_PARAMETER_NUM - 2];
		}
	}

	board_data->edge_filter_params.check_sum -= board_data->edge_filter_params.length;
	ts_log("check_sum = %d", board_data->edge_filter_params.check_sum);
	/* add check sum */
	board_data->edge_filter_params_cmd[i++] =
		(board_data->edge_filter_params.check_sum >> 24) & 0xFF;
	board_data->edge_filter_params_cmd[i++] =
		(board_data->edge_filter_params.check_sum >> 16) & 0xFF;
	board_data->edge_filter_params_cmd[i++] =
		(board_data->edge_filter_params.check_sum >> 8) & 0xFF;
	board_data->edge_filter_params_cmd[i++] =
		(board_data->edge_filter_params.check_sum) & 0xFF;

	ts_info("board_data->edge_filter_params_cmd: %*ph",
		2 + 4 * GRIP_PARAMETER_NUM,
		board_data->edge_filter_params_cmd);
	ts_info("board_data->edge_filter_params_cmd: %*ph",
		4 * GRIP_PARAMETER_NUM,
		(u8*)&(board_data->edge_filter_params_cmd[42]));
	ts_info("board_data->edge_filter_params_cmd: %*ph",
		4 * GRIP_PARAMETER_NUM + 4,
		(u8*)&(board_data->edge_filter_params_cmd[82]));

	return 0;
}
/**
 * parse_edge_filter_params - parse parameters from dts
 * @board_data - the value where parse data to saved in
 */
static int parse_edge_filter_params(struct device_node *node,
						struct goodix_ts_board_data *board_data)
{
	int r = 0, byte_len = 0;
//	int i = 0;

	/* edge filter corner size in game mode */
	r = of_property_read_u16_array(node, "goodix,edge-filter-corner-size",
				 board_data->edge_filter_corner_size, 3);
	if (r) {
		ts_err("Invalid goodix,edge-filter-corner-size");
		return -EINVAL;
	}
	ts_info("edge_filter_corner_size values: %d, %d, %d",
		board_data->edge_filter_corner_size[0],
		board_data->edge_filter_corner_size[1],
		board_data->edge_filter_corner_size[2]);

	/* edge filter parameter goodix,touch-deadzone-filter-ver*/
	if (of_find_property(node, "goodix,touch-deadzone-filter-ver", &byte_len)) {
		if ((byte_len / sizeof(u32)) != (GTP_PARAMETER_NUM * 4)) {
			ts_info("touch-deadzone-filter-ver parameters len in dts is wrong:%d", byte_len / sizeof(u32));
			return -EINVAL;
		}
		r = of_property_read_u32_array(node, "goodix,touch-deadzone-filter-ver",
				 board_data->edge_filter_params.deadzone_filter_ver, byte_len / sizeof(u32));
		if (r < 0) {
			ts_err("Invalid goodix,touch-deadzone-filter-ver");
			return -EINVAL;
		}
	}
/*	for(i = 0; i < GTP_PARAMETER_NUM * 4; i++){
		ts_info("touch-deadzone-filter-ver[%d] = %d", i, board_data->edge_filter_params.deadzone_filter_ver[i]);
	}*/

	/* edge filter parameter goodix,touch-deadzone-filter-hor*/
	if (of_find_property(node, "goodix,touch-deadzone-filter-hor", &byte_len)) {
		if ((byte_len / sizeof(u32)) != (GTP_PARAMETER_NUM * 4)) {
			ts_info("touch-deadzone-filter-hor parameters len in dts is wrong");
			return -EINVAL;
		}
		r = of_property_read_u32_array(node, "goodix,touch-deadzone-filter-hor",
					 board_data->edge_filter_params.deadzone_filter_hor, byte_len / sizeof(u32));
		if (r < 0) {
			ts_err("Invalid goodix,touch-deadzone-filter-hor");
			return -EINVAL;
		}
	}
/*	for(i = 0; i < GTP_PARAMETER_NUM * 4; i++){
		ts_info("touch-deadzone-filter-hor[%d] = %d", i, board_data->edge_filter_params.deadzone_filter_hor[i]);
	}*/

	/* edge filter parameter goodix,touch-edgezone-filter-ver*/
	if (of_find_property(node, "goodix,touch-edgezone-filter-ver", &byte_len)) {
		if ((byte_len / sizeof(u32)) != (GTP_PARAMETER_NUM * 4)) {
			ts_info("touch-edgezone-filter-ver parameters len in dts is wrong");
			return -EINVAL;
		}
		r = of_property_read_u32_array(node, "goodix,touch-edgezone-filter-ver",
					 board_data->edge_filter_params.edgezone_filter_ver, byte_len / sizeof(u32));
		if (r < 0) {
			ts_err("Invalid goodix,touch-edgezone-filter-ver");
			return -EINVAL;
		}
	}
/*	for(i = 0; i < GTP_PARAMETER_NUM * 4; i++){
		ts_info("touch-edgezone-filter-ver[%d] = %d", i, board_data->edge_filter_params.edgezone_filter_ver[i]);
	}*/

	/* edge filter parameter goodix,touch-edgezone-filter-hor*/
	if (of_find_property(node, "goodix,touch-edgezone-filter-hor", &byte_len)) {
		if ((byte_len / sizeof(u32)) != (GTP_PARAMETER_NUM * 4)) {
			ts_info("touch-edgezone-filter-hor parameters len in dts is wrong");
			return -EINVAL;
		}
		r = of_property_read_u32_array(node, "goodix,touch-edgezone-filter-hor",
					 board_data->edge_filter_params.edgezone_filter_hor, byte_len / sizeof(u32));
		if (r < 0) {
			ts_err("Invalid goodix,touch-edgezone-filter-hor");
			return -EINVAL;
		}
	}
/*	for(i = 0; i < GTP_PARAMETER_NUM * 4; i++){
		ts_info("touch-edgezone-filter-hor[%d] = %d", i, board_data->edge_filter_params.edgezone_filter_hor[i]);
	}*/
	/* edge filter parameter goodix,touch-cornerzone-filter-ver*/
	if (of_find_property(node, "goodix,touch-cornerzone-filter-ver", &byte_len)) {
		if ((byte_len / sizeof(u32)) != (GTP_PARAMETER_NUM * 4)) {
			ts_info("touch-cornerzone-filter-ver parameters len in dts is wrong");
			return -EINVAL;
		}
		r = of_property_read_u32_array(node, "goodix,touch-cornerzone-filter-ver",
					 board_data->edge_filter_params.cornerzone_filter_ver, byte_len / sizeof(u32));
		if (r < 0) {
			ts_err("Invalid goodix,touch-cornerzone-filter-ver");
			return -EINVAL;
		}
	}
/*	for(i = 0; i < GTP_PARAMETER_NUM * 4; i++){
		ts_info("touch-cornerzone-filter-ver[%d] = %d", i, board_data->edge_filter_params.cornerzone_filter_ver[i]);
	}*/

	/* edge filter parameter goodix,touch-cornerzone-filter-hor1*/
	if (of_find_property(node, "goodix,touch-cornerzone-filter-hor1", &byte_len)) {
		if ((byte_len / sizeof(u32)) != (GTP_PARAMETER_NUM * 4)) {
			ts_info("touch-cornerzone-filter-hor1 parameters len in dts is wrong");
			return -EINVAL;
		}
		r = of_property_read_u32_array(node, "goodix,touch-cornerzone-filter-hor1",
					board_data->edge_filter_params.cornerzone_filter_hor1, byte_len / sizeof(u32));
		if (r < 0) {
			ts_err("Invalid goodix,touch-cornerzone-filter-hor1");
			return -EINVAL;
		}
	}
/*	for(i = 0; i < GTP_PARAMETER_NUM * 4; i++){
		ts_info("touch-cornerzone-filter-hor1[%d] = %d", i, board_data->edge_filter_params.cornerzone_filter_hor1[i]);
	}*/

	/* edge filter parameter goodix,touch-cornerzone-filter-hor2*/
	if (of_find_property(node, "goodix,touch-cornerzone-filter-hor2", &byte_len)) {
		if ((byte_len / sizeof(u32)) != (GTP_PARAMETER_NUM * 4)) {
			ts_info("touch-cornerzone-filter-hor2 parameters len in dts is wrong");
			return -EINVAL;
		}
		r = of_property_read_u32_array(node, "goodix,touch-cornerzone-filter-hor2",
				 board_data->edge_filter_params.cornerzone_filter_hor2, byte_len / sizeof(u32));
		if (r < 0) {
			ts_err("Invalid goodix,touch-cornerzone-filter-hor2");
			return -EINVAL;
		}
	}
/*	for(i = 0; i < GTP_PARAMETER_NUM * 4; i++){
		ts_info("touch-cornerzone-filter-hor2[%d] = %d", i, board_data->edge_filter_params.cornerzone_filter_hor2[i]);
	}*/

	return edge_filter_checksume_and_cmd(board_data);
}
#endif

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

	ts_err("enter::%s\n", __func__);
	if (!board_data) {
		ts_err("Invalid board data");
		return -EINVAL;
	}

	r = of_get_named_gpio(node, "goodix,reset-gpio", 0);
	if (r < 0) {
		ts_err("Invalid reset-gpio in dt: %d", r);
		return -EINVAL;
	} else {
		ts_info("Parse reset-gpio[%d] from dt", r);
		board_data->reset_gpio = r;
	}

	r = of_get_named_gpio(node, "goodix,irq-gpio", 0);
	if (r < 0) {
		ts_err("Invalid irq-gpio in dt: %d", r);
		return -EINVAL;
	} else {
		ts_info("Parse irq-gpio[%d] from dt", r);
		board_data->irq_gpio = r;
	}

	r = of_property_read_u32(node, "goodix,irq-flags",
				 &board_data->irq_flags);
	if (r) {
		ts_err("Invalid irq-flags");
		return -EINVAL;
	}

	board_data->avdd_name = "vtouch";
	board_data->vddio_name = "iotouch";
	r = of_property_read_u32(node, "vtouch-load", &board_data->avdd_load);
	if (r) {
		ts_err("can't get vtouch-load");
		board_data->avdd_load = 0;
	}

	r = of_property_read_string(node, "goodix,cfg-name",
				    &board_data->cfg_bin_name);
	if (r) {
		ts_err("can't get cfg name");
		board_data->cfg_bin_name = NULL;
	}

	r = of_property_read_u32(node, "goodix,power-on-delay-us",
				 &board_data->power_on_delay_us);
	if (!r) {
		/* 1000ms is too large, maybe you have pass
		 * a wrong value */
		if (board_data->power_on_delay_us > 1000 * 1000) {
			ts_err("Power on delay time exceed 1s, please check");
			board_data->power_on_delay_us = 0;
		}
	}

	r = of_property_read_u32(node, "goodix,power-off-delay-us",
				 &board_data->power_off_delay_us);
	if (!r) {
		/* 1000ms is too large, maybe you have pass
		 * a wrong value */
		if (board_data->power_off_delay_us > 1000 * 1000) {
			ts_err("Power off delay time exceed 1s, please check");
			board_data->power_off_delay_us = 0;
		}
	}

#ifdef USE_NEW_VERSION_EDGE_FILTER
	parse_edge_filter_params(node, board_data);
#endif

	/* get xyz resolutions */
	r = goodix_parse_dt_resolution(node, board_data);
	if (r < 0) {
		ts_err("Failed to parse resolutions:%d", r);
		return r;
	}

	/* key map */
	prop = of_find_property(node, "goodix,panel-key-map", NULL);
	if (prop && prop->length) {
		if (prop->length / sizeof(u32) > GOODIX_MAX_KEY) {
			ts_err("Size of panel-key-map is invalid");
			return r;
		}

		board_data->panel_max_key = prop->length / sizeof(u32);
		board_data->tp_key_num = prop->length / sizeof(u32);
		r = of_property_read_u32_array(node,
					       "goodix,panel-key-map",
					       &board_data->panel_key_map[0],
					       board_data->panel_max_key);
		if (r)
			return r;
	}

	/*get pen-enable switch and pen keys, must after "key map" */
	board_data->pen_enable =
	    of_property_read_bool(node, "goodix,pen-enable");
	if (board_data->pen_enable) {
		prop = of_find_property(node, "goodix,key-of-pen", NULL);
		if (prop && prop->length) {
			if (prop->length / sizeof(u32) > GOODIX_PEN_MAX_KEY) {
				ts_err("Size of key-of-pen is invalid");
				return r;
			}
			r = of_property_read_u32_array(node,
						       "goodix,key-of-pen",
						       &board_data->panel_key_map
						       [board_data->panel_max_key],
						       prop->length /
						       sizeof(u32));
			if (r)
				return r;
			board_data->panel_max_key +=
			    (prop->length / sizeof(u32));
		}
	}
	ts_info("***key:%d, %d, %d, %d, %d",
		board_data->panel_key_map[0],
		board_data->panel_key_map[1],
		board_data->panel_key_map[2],
		board_data->panel_key_map[3], board_data->panel_key_map[4]);
	/*add end */

	ts_debug("[DT]id:%d, x:%d, y:%d, w:%d, p:%d",
		 board_data->panel_max_id,
		 board_data->panel_max_x,
		 board_data->panel_max_y,
		 board_data->panel_max_w, board_data->panel_max_p);
	return 0;
}

/**
 * goodix_parse_customize_params - parse sensor independent params
 * @dev: pointer to device data
 * @board_data: board data
 * @sensor_id: sensor ID
 * return: 0 - read ok, < 0 - i2c transter error
*/
static int goodix_parse_customize_params(struct goodix_ts_device *dev, struct goodix_ts_board_data
					 *board_data, unsigned int sensor_id)
{
	struct device_node *node = dev->dev->of_node;
	char of_node_name[24];
	int r;

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

	r = of_property_read_string(node, "goodix,fw-name",
				    &board_data->fw_name);
	if (r) {
		ts_err("can't get firmware name");
		board_data->fw_name = NULL;
	}

	/* sensor independent resolutions */
	r = goodix_parse_dt_resolution(node, board_data);
	return r;
}
#endif

#ifdef CONFIG_ACPI
static int goodix_parse_acpi(struct acpi_device *dev,
			     struct goodix_ts_board_data *bdata)
{
	return 0;
}

static int goodix_parse_acpi_cfg(struct acpi_device *dev,
				 char *cfg_type,
				 struct goodix_ts_config *config,
				 unsigned int sensor_id)
{
	return 0;
}
#endif

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
	unsigned char get_buf[32], addr_buf[2];
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
				      ? I2C_MAX_TRANSFER_SIZE : len,
				      GFP_KERNEL);
		if (msgs[1].buf == NULL) {
			ts_err("Malloc failed");
			return -ENOMEM;
		}
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
			if (likely(i2c_transfer(client->adapter, msgs, 2) == 2)) {
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
		/* code optimize,use stack memory */
		msg.buf = &put_buf[0];
	} else {
		msg.buf = kmalloc(I2C_MAX_TRANSFER_SIZE < len + TS_ADDR_LENGTH
				  ? I2C_MAX_TRANSFER_SIZE : len +
				  TS_ADDR_LENGTH, GFP_KERNEL);
		if (msg.buf == NULL) {
			ts_err("Malloc failed");
			return -ENOMEM;
		}
	}

	while (pos != len) {
		if (unlikely
		    (len - pos > I2C_MAX_TRANSFER_SIZE - TS_ADDR_LENGTH))
			transfer_length =
			    I2C_MAX_TRANSFER_SIZE - TS_ADDR_LENGTH;
		else
			transfer_length = len - pos;

		msg.buf[0] = (unsigned char)((address >> 8) & 0xFF);
		msg.buf[1] = (unsigned char)(address & 0xFF);
		msg.len = transfer_length + 2;
		memcpy(&msg.buf[2], &data[pos], transfer_length);

		for (retry = 0; retry < GOODIX_BUS_RETRY_TIMES; retry++) {
			if (likely(i2c_transfer(client->adapter, &msg, 1) == 1)) {
				pos += transfer_length;
				address += transfer_length;
				break;
			}
			ts_info("I2c write retry[%d]", retry + 1);
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
int goodix_set_i2c_doze_mode(struct goodix_ts_device *dev, int enable)
{
	int result = -EINVAL;
	int i;
	u8 w_data, r_data;

	if (dev->ic_type != IC_TYPE_NORMANDY)
		return 0;
	mutex_lock(&dev->doze_mode_lock);
	if (enable) {
		if (dev->doze_mode_set_count != 0)
			dev->doze_mode_set_count--;
		/*when count equal 0, allow ic enter doze mode */
		if (dev->doze_mode_set_count == 0) {
			w_data = TS_DOZE_ENABLE_DATA;
			for (i = 0; i < TS_DOZE_ENABLE_RETRY_TIMES; i++) {
				result =
				    goodix_i2c_write_trans(dev,
							   TS_REG_DOZE_CTRL,
							   &w_data, 1);
				if (!result) {
					result = 0;
					goto exit;
				}
				usleep_range(1000, 1100);
			}
			if (i >= TS_DOZE_ENABLE_RETRY_TIMES)
				ts_err
				    ("i2c doze mode enable failed, i2c write fail");
		} else {
			/*ts_info("doze count not euqal 0, so skip doze mode enable"); */
			result = 0;
			goto exit;
		}
	} else {
		dev->doze_mode_set_count++;
		if (dev->doze_mode_set_count == 1) {
			w_data = TS_DOZE_DISABLE_DATA;
			goodix_i2c_write_trans(dev, TS_REG_DOZE_CTRL, &w_data,
					       1);
			usleep_range(1000, 1100);
			for (i = 0; i < TS_DOZE_DISABLE_RETRY_TIMES; i++) {
				goodix_i2c_read_trans(dev, TS_REG_DOZE_STAT,
						      &r_data, 1);
				if (TS_DOZE_CLOSE_OK_DATA == r_data) {
					result = 0;
					goto exit;
				} else if (0xAA != r_data) {
					w_data = TS_DOZE_DISABLE_DATA;
					goodix_i2c_write_trans(dev,
							       TS_REG_DOZE_CTRL,
							       &w_data, 1);
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
	mutex_unlock(&dev->doze_mode_lock);
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
	int r;

	if (dev->ic_type == IC_TYPE_NORMANDY) {
		if (goodix_set_i2c_doze_mode(dev, false) != 0)
			ts_err
			    ("gtx8 i2c write:0x%04x ERROR, disable doze mode FAILED",
			     reg);
	}

	r = goodix_i2c_write_trans(dev, reg, data, len);

	if (dev->ic_type == IC_TYPE_NORMANDY) {
		if (goodix_set_i2c_doze_mode(dev, true) != 0)
			ts_err
			    ("gtx8 i2c write:0x%04x ERROR, enable doze mode FAILED",
			     reg);
	}
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
	int r;

	if (dev->ic_type == IC_TYPE_NORMANDY) {
		if (goodix_set_i2c_doze_mode(dev, false) != 0)
			ts_err
			    ("gtx8 i2c read:0x%04x ERROR, disable doze mode FAILED",
			     reg);
	}
	r = goodix_i2c_read_trans(dev, reg, data, len);
	if (dev->ic_type == IC_TYPE_NORMANDY) {
		if (goodix_set_i2c_doze_mode(dev, true) != 0)
			ts_err
			    ("gtx8 i2c read:0x%04x ERROR, enable doze mode FAILED",
			     reg);
	}
	return r;
}

/**
 * goodix_i2c_write_trans_once -
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
		/* code optimize,use stack memory */
		msg.buf = &put_buf[0];
	} else {
		msg.buf = kmalloc(I2C_MAX_TRANSFER_SIZE < len + TS_ADDR_LENGTH
				  ? I2C_MAX_TRANSFER_SIZE : len +
				  TS_ADDR_LENGTH, GFP_KERNEL);
		if (msg.buf == NULL) {
			ts_err("Malloc failed");
			return -ENOMEM;
		}
	}

	while (pos != len) {
		if (unlikely
		    (len - pos > I2C_MAX_TRANSFER_SIZE - TS_ADDR_LENGTH))
			transfer_length =
			    I2C_MAX_TRANSFER_SIZE - TS_ADDR_LENGTH;
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

static void goodix_cmds_init(struct goodix_ts_cmd *ts_cmd,
			     u8 cmds, u8 cmd_data, u32 reg_addr)
{
	if (reg_addr) {
		ts_cmd->cmd_reg = reg_addr;
		ts_cmd->length = 3;
		ts_cmd->cmds[0] = cmds;
		ts_cmd->cmds[1] = cmd_data;
		ts_cmd->cmds[2] = 0 - cmds - cmd_data;
		ts_cmd->initialized = true;
	} else {
		ts_cmd->initialized = false;
	}
}

/**
 * goodix_send_command - seng cmd to firmware
 *
 * @dev: pointer to device
 * @cmd: pointer to command struct which cotain command data
 * Returns 0 - succeed,<0 - failed
 */
int goodix_send_command(struct goodix_ts_device *dev, struct goodix_ts_cmd *cmd)
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
	u8 buffer[12];
	u8 temp_buf[256], checksum;
	int r;
	u8 pid_read_len = dev->reg.pid_len;
	u8 vid_read_len = dev->reg.vid_len;
	u8 sensor_id_mask = dev->reg.sensor_id_mask;
#define IS_CHAR(c)	(((c) >= 'A' && (c) <= 'Z')\
		|| ((c) >= 'a' && (c) <= 'z')\
		|| ((c) >= '0' && (c) <= '9'))

	if (!version) {
		ts_err("pointer of version is NULL");
		return -EINVAL;
	}
	version->valid = false;

	/*check reg info valid */
	if (!dev->reg.pid || !dev->reg.sensor_id || !dev->reg.vid) {
		ts_err("reg is NULL, pid:0x%04x, vid:0x%04x, sensor_id:0x%04x",
		       dev->reg.pid, dev->reg.vid, dev->reg.sensor_id);
		return -EINVAL;
	}
	if (!pid_read_len || pid_read_len > GOODIX_PID_MAX_LEN || !vid_read_len
	    || vid_read_len > GOODIX_VID_MAX_LEN) {
		ts_err
		    ("pid vid read len ERROR, pid_read_len:%d, vid_read_len:%d",
		     pid_read_len, vid_read_len);
		return -EINVAL;
	}

	/*disable doze mode, just valid for normandy
	 * this func must be used in pairs*/
	goodix_set_i2c_doze_mode(dev, false);

	/*check checksum */
	if (dev->reg.version_base && dev->reg.version_len < 256) {
		r = goodix_i2c_read(dev, dev->reg.version_base,
				    temp_buf, dev->reg.version_len);

		if (r < 0) {
			ts_err("Read version base failed, reg:0x%02x, len:%d",
			       dev->reg.version_base, dev->reg.version_len);
			if (version)
				version->valid = false;
			goto exit;
		}

		checksum = checksum_u8(temp_buf, dev->reg.version_len);
		if (checksum) {
			ts_err("checksum error:0x%02x, base:0x%02x, len:%d",
			       checksum, dev->reg.version_base,
			       dev->reg.version_len);
			ts_err("%*ph", (int)(dev->reg.version_len / 2),
			       temp_buf);
			ts_err("%*ph",
			       (int)(dev->reg.version_len -
				     dev->reg.version_len / 2),
			       &temp_buf[dev->reg.version_len / 2]);

			if (version)
				version->valid = false;
			r = -EINVAL;
			goto exit;
		}
	}

	/*read pid */
	memset(buffer, 0, sizeof(buffer));
	memset(version->pid, 0, sizeof(version->pid));
	r = goodix_i2c_read(dev, dev->reg.pid, buffer, pid_read_len);
	if (r < 0) {
		ts_err("Read pid failed");
		if (version)
			version->valid = false;
		goto exit;
	}
	memcpy(version->pid, buffer, pid_read_len);

	/*read vid */
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
	ts_info("PID:%s, VID:0x%02x 0x%02x 0x%02x 0x%02x",
		version->pid,
		version->vid[0],
		version->vid[1], version->vid[2], version->vid[3]);
	/*read sensor_id */
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

	r = goodix_i2c_read(dev, dev->reg.vid, buffer, vid_read_len);
	memcpy(version->vid, buffer, vid_read_len);
	ts_info("PID:%s,SensorID:%d, VID:%*ph",
		version->pid,
		version->sensor_id, (int)sizeof(version->vid), version->vid);

exit:
	/*enable doze mode, just valid for normandy
	 * this func must be used in pairs*/
	goodix_set_i2c_doze_mode(dev, true);

	return r;
}

static int goodix_wait_cfg_cmd_ready(struct goodix_ts_device *dev,
				     u8 right_cmd, u8 send_cmd)
{
	int try_times = 0;
	u8 cmd_flag = 0;
	u8 cmd_buf[3] = { 0 };
	u16 command_reg = dev->reg.command;
	struct goodix_ts_cmd ts_cmd;

	goodix_cmds_init(&ts_cmd, send_cmd, 0, command_reg);

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
			ts_err
			    ("Read cmd_reg data abnormal,return:0x%02X, 0x%02X, 0x%02X, send again",
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

static int goodix_send_small_config(struct goodix_ts_device *dev,
				    struct goodix_ts_config *config)
{
	int r = 0;
	int try_times = 0;
	u8 buf = 0;
	u16 command_reg = dev->reg.command;
	u16 cfg_reg = dev->reg.cfg_addr;
	struct goodix_ts_cmd ts_cmd;

	/*1. Inquire command_reg until it's free */
	for (try_times = 0; try_times < TS_WAIT_CMD_FREE_RETRY_TIMES;
	     try_times++) {
		if (!goodix_i2c_read(dev, command_reg, &buf, 1)
		    && buf == TS_CMD_REG_READY)
			break;
		usleep_range(10000, 11000);
	}
	if (try_times >= TS_WAIT_CMD_FREE_RETRY_TIMES) {
		ts_err
		    ("Send small cfg FAILED, before send, reg:0x%04x is not 0xff",
		     command_reg);
		r = -EINVAL;
		goto exit;
	}

	/*2. write cfg data */
	if (goodix_i2c_write(dev, cfg_reg, config->data, config->length)) {
		ts_err("Send small cfg FAILED, write cfg to fw ERROR");
		r = -EINVAL;
		goto exit;
	}

	/*3. send 0x81 command */
	goodix_cmds_init(&ts_cmd, COMMAND_SEND_SMALL_CFG, 0, dev->reg.command);
	if (goodix_send_command(dev, &ts_cmd)) {
		ts_err
		    ("Send large cfg FAILED, send COMMAND_SEND_SMALL_CFG ERROR");
		r = -EINVAL;
		goto exit;
	}

	r = 0;
	ts_info("send small cfg SUCCESS");

exit:
	return r;
}

static int goodix_send_large_config(struct goodix_ts_device *dev,
				    struct goodix_ts_config *config)
{
	int r = 0;
	int try_times = 0;
	u8 buf = 0;
	u16 command_reg = dev->reg.command;
	u16 cfg_reg = dev->reg.cfg_addr;
	struct goodix_ts_cmd ts_cmd;

	/*1. Inquire command_reg until it's free */
	for (try_times = 0; try_times < TS_WAIT_CMD_FREE_RETRY_TIMES;
	     try_times++) {
		if (!goodix_i2c_read(dev, command_reg, &buf, 1)
		    && buf == TS_CMD_REG_READY)
			break;
		usleep_range(10000, 11000);
	}
	if (try_times >= TS_WAIT_CMD_FREE_RETRY_TIMES) {
		ts_err
		    ("Send large cfg FAILED, before send, reg:0x%04x is not 0xff",
		     command_reg);
		r = -EINVAL;
		goto exit;
	}

	/*2. send "start write cfg" command */
	goodix_cmds_init(&ts_cmd, COMMAND_START_SEND_CFG, 0, dev->reg.command);
	if (goodix_send_command(dev, &ts_cmd)) {
		ts_err
		    ("Send large cfg FAILED, send COMMAND_START_SEND_CFG ERROR");
		r = -EINVAL;
		goto exit;
	}

	/*3. wait ic set command_reg to 0x82 */
	if (goodix_wait_cfg_cmd_ready
	    (dev, COMMAND_SEND_CFG_PREPARE_OK, COMMAND_START_SEND_CFG)) {
		ts_err("Send large cfg FAILED, reg:0x%04x is not 0x82",
		       command_reg);
		r = -EINVAL;
		goto exit;
	}

	/*4. write cfg */
	if (goodix_i2c_write(dev, cfg_reg, config->data, config->length)) {
		ts_err("Send large cfg FAILED, write cfg to fw ERROR");
		r = -EINVAL;
		goto exit;
	}

	/*5. send "end send cfg" command */
	goodix_cmds_init(&ts_cmd, COMMAND_END_SEND_CFG, 0, dev->reg.command);
	if (goodix_send_command(dev, &ts_cmd)) {
		ts_err
		    ("Send large cfg FAILED, send COMMAND_END_SEND_CFG ERROR");
		r = -EINVAL;
		goto exit;
	}

	/*6. wait ic set command_reg to 0xff */
	for (try_times = 0; try_times < TS_WAIT_CMD_FREE_RETRY_TIMES;
	     try_times++) {
		if (!goodix_i2c_read(dev, command_reg, &buf, 1)
		    && buf == TS_CMD_REG_READY)
			break;
		usleep_range(10000, 11000);
	}
	if (try_times >= TS_WAIT_CMD_FREE_RETRY_TIMES) {
		ts_err
		    ("Send large cfg FAILED, after send, reg:0x%04x is not 0xff",
		     command_reg);
		r = -EINVAL;
		goto exit;
	}

	ts_info("Send large cfg SUCCESS");
	r = 0;

exit:
	return r;
}

static int goodix_check_cfg_valid(struct goodix_ts_device *dev, u8 *cfg,
				  u32 length)
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

	if (dev->ic_type == IC_TYPE_NANJING) {
		/*check configuration head checksum */
		checksum = 0;
		for (i = 0; i < 3; i++)
			checksum += cfg[i];

		if (checksum != 0) {
			ts_err
			    ("cfg head checksum ERROR, ic type:nanjing, checksum:0x%02x",
			     checksum);
			ret = -EINVAL;
			goto exit;
		}
		bag_num = cfg[1];
		bag_start = 3;
	} else if (dev->ic_type == IC_TYPE_NORMANDY) {
		checksum = 0;
		for (i = 0; i < TS_CFG_HEAD_LEN; i++)
			checksum += cfg[i];
		if (checksum != 0) {
			ts_err
			    ("cfg head checksum ERROR, ic type:normandy, checksum:0x%02x",
			     checksum);
			ret = -EINVAL;
			goto exit;
		}
		bag_num = cfg[TS_CFG_BAG_NUM_INDEX];
		bag_start = TS_CFG_BAG_START_INDEX;
	} else {
		ts_err("cfg check FAILED, unkonw ic_type");
		ret = -EINVAL;
		goto exit;
	}

	ts_info("cfg bag_num:%d, cfg length:%d", bag_num, length);

	/*check each bag's checksum */
	for (j = 0; j < bag_num; j++) {
		if (bag_start >= length - 1) {
			ts_err("ERROR, overflow!!bag_start:%d, cfg_len:%d",
			       bag_start, length);
			ret = -EINVAL;
			goto exit;
		}

		bag_end = bag_start + cfg[bag_start + 1] + 3;
		if ((j == 0) && (dev->ic_type == IC_TYPE_NANJING))
			/*the first bag of nanjing cfg is different! */
			bag_end = 336;

		checksum = 0;
		if (bag_end > length) {
			ts_err
			    ("ERROR, overflow!!bag:%d, bag_start:%d,  bag_end:%d, cfg length:%d",
			     j, bag_start, bag_end, length);
			ret = -EINVAL;
			goto exit;
		}
		for (i = bag_start; i < bag_end; i++)
			checksum += cfg[i];
		if (checksum != 0) {
			ts_err("cfg INVALID, bag:%d checksum ERROR:0x%02x", j,
			       checksum);
			ret = -EINVAL;
			goto exit;
		}
		bag_start = bag_end;
	}

	ret = 0;
	ts_info("configuration check SUCCESS");

exit:
	return ret;
}

static int goodix_send_config(struct goodix_ts_device *dev,
			      struct goodix_ts_config *config)
{
	int r = 0;

	/*check reg valid */
/*	if (!dev->reg.cfg_addr) {
		ts_err("cfg register is NULL");
		return -EINVAL;
	}*/
	if (!config || !*(config->data)) {
		ts_err("Null config data");
		return -EINVAL;
	}

	/*check configuration valid */
	r = goodix_check_cfg_valid(dev, config->data, config->length);
	if (r != 0) {
		ts_err("cfg check FAILED");
		return -EINVAL;
	}

	ts_info("ver:%02xh,size:%d", config->data[0], config->length);

	mutex_lock(&config->lock);

	if (dev->ic_type == IC_TYPE_NANJING)
		r = goodix_send_large_config(dev, config);
	else if (dev->ic_type == IC_TYPE_NORMANDY) {
		/*disable doze mode */
		goodix_set_i2c_doze_mode(dev, false);

		if (config->length > 32)
			r = goodix_send_large_config(dev, config);
		else
			r = goodix_send_small_config(dev, config);

		/*enable doze mode */
		goodix_set_i2c_doze_mode(dev, true);
	}

	if (r != 0)
		ts_err("send_cfg FAILED, ic_type:%d, cfg_len:%d",
		       dev->ic_type, config->length);

	mutex_unlock(&config->lock);
	return r;
}

#if 0
/**
 * goodix_send_config - send config data to device.
 * @dev: pointer to device
 * @config: pointer to config data struct to be send
 * @return: 0 - succeed, < 0 - failed
 */
static int goodix_send_config(struct goodix_ts_device *dev,
			      struct goodix_ts_config *config)
{
	int r = 0;

	if (!config || !config->data) {
		ts_err("Null config data");
		return -EINVAL;
	}

	ts_info("Send %s,ver:%02xh,size:%d",
		config->name, config->data[0], config->length);

	mutex_lock(&config->lock);
	r = goodix_i2c_write(dev, config->reg_base,
			     config->data, config->length);
	if (r)
		goto exit;

	/* make sure the firmware accept the config data */
	if (config->delay)
		msleep(config->delay);
exit:
	mutex_unlock(&config->lock);
	return r;
}
#endif

/**
 * goodix_close_hidi2c_mode
 *   Called by touch core module when bootup
 * @ts_dev: pointer to touch device
 * return: 0 - no error, <0 error
*/
static int goodix_close_hidi2c_mode(struct goodix_ts_device *ts_dev)
{
	int r = 0;
	int try_times;
	int j;
	unsigned char buffer[1];
	unsigned char reg_sta;
	struct goodix_ts_cmd ts_cmd;

	for (try_times = 0; try_times < 10; try_times++) {
		if (goodix_i2c_read(ts_dev, 0x8040, &reg_sta, 1) != 0)
			continue;
		else if (reg_sta == 0xff)
			break;
		usleep_range(10000, 11000);
	}
	if (try_times >= 10) {
		ts_info
		    ("goodix_close_hidi2c_mode FAILED, 0x8040 is not equal to 0xff");
		return -EINVAL;
	}

	goodix_cmds_init(&ts_cmd, COMMAND_CLOSE_HID, 0, 0x8040);
	for (try_times = 0; try_times < 3; try_times++) {
		if (ts_cmd.initialized) {
			r = goodix_send_command(ts_dev, &ts_cmd);
			if (r)
				continue;

			usleep_range(100000, 110000);

			/*read 0x8040, if it's not 0xFF,continue */
			for (j = 0; j < 3; j++) {
				if (goodix_i2c_read(ts_dev, 0x8040, buffer, 1)
				    != 0)
					continue;
				else {
					if (buffer[0] != 0xFF) {
						ts_info
						    ("try_times:%d:%d, read 0x8040:0x%02x",
						     try_times, j, buffer[0]);
						usleep_range(10000, 11000);
						continue;
					} else
						goto exit;
				}
			}
		}
	}

exit:
	if (try_times >= 3) {
		ts_info("close hid_i2c mode FAILED");
		r = -EINVAL;
	} else {
		ts_info("close hid_i2c mode SUCCESS");
		r = 0;
	}
	return r;
}

/* success return config length else return -1 */
static int _goodix_do_read_config(struct goodix_ts_device *dev,
				  u32 base_addr, u8 *buf)
{
	int sub_bags = 0;
	int offset = 0;
	int subbag_len;
	u8 checksum;
	int i;
	int ret;

	/*disable doze mode */
	if (dev->ic_type == IC_TYPE_NORMANDY)
		goodix_set_i2c_doze_mode(dev, false);

	ret = goodix_i2c_read(dev, base_addr, buf, TS_CFG_HEAD_LEN);
	if (ret)
		goto err_out;

	if (dev->ic_type == IC_TYPE_NANJING) {
		offset = 3;
		sub_bags = buf[1];
		checksum = checksum_u8(buf, 3);
	} else {
		offset = TS_CFG_BAG_START_INDEX;
		sub_bags = buf[TS_CFG_BAG_NUM_INDEX];
		checksum = checksum_u8(buf, TS_CFG_HEAD_LEN);
	}
	if (checksum) {
		ts_err("Config head checksum err:0x%x,data:%*ph",
		       checksum, TS_CFG_HEAD_LEN, buf);
		ret = -EINVAL;
		goto err_out;
	}

	ts_info("config_version:%u, vub_bags:%u", buf[0], sub_bags);
	for (i = 0; i < sub_bags; i++) {
		/* read sub head [0]: sub bag num, [1]: sub bag length */
		ret = goodix_i2c_read(dev, base_addr + offset, buf + offset, 2);
		if (ret)
			goto err_out;

		/* read sub bag data */
		if (dev->ic_type == IC_TYPE_NANJING && i == 0)
			subbag_len = buf[offset + 1] + 256;
		else
			subbag_len = buf[offset + 1];

		ts_info("sub bag num:%u,sub bag length:%u", buf[offset],
			subbag_len);
		ret =
		    goodix_i2c_read(dev, base_addr + offset + 2,
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
		ts_debug("sub bag %d, data:%*ph", buf[offset],
			 buf[offset + 1] + 3, buf + offset);
	}
	ret = offset;

err_out:
	/*enable doze mode */
	if (dev->ic_type == IC_TYPE_NORMANDY)
		goodix_set_i2c_doze_mode(dev, true);

	return ret;
}

/* success return config_len, <= 0 failed */
static int goodix_read_config(struct goodix_ts_device *dev,
			      u8 *config_data, u32 config_len)
{
	struct goodix_ts_cmd ts_cmd;
	u8 cmd_flag;
	u32 cmd_reg = dev->reg.command;
	int r = 0;
	int i;

	if (!config_data || config_len > TS_CFG_MAX_LEN) {
		ts_err("Illegal params");
		return -EINVAL;
	}
	if (!dev->reg.command) {
		ts_err("command register ERROR:0x%04x", dev->reg.command);
		return -EINVAL;
	}

	/*disable doze mode */
	if (dev->ic_type == IC_TYPE_NORMANDY)
		goodix_set_i2c_doze_mode(dev, false);

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
	goodix_cmds_init(&ts_cmd, COMMAND_START_READ_CFG, 0, cmd_reg);
	r = goodix_send_command(dev, &ts_cmd);
	if (r) {
		ts_err("Failed send read config command");
		goto exit;
	}
	/* wait for config data ready */
	if (goodix_wait_cfg_cmd_ready
	    (dev, COMMAND_READ_CFG_PREPARE_OK, COMMAND_START_READ_CFG)) {
		ts_err("Wait for config data ready timeout");
		r = -EAGAIN;
		goto exit;
	}
	if (config_len) {
		r = goodix_i2c_read(dev, cmd_reg + 16, config_data, config_len);
		if (r)
			ts_err("Failed read config data");
		else
			r = config_len;
	} else {
		r = _goodix_do_read_config(dev, cmd_reg + 16, config_data);
		if (r < 0)
			ts_err("Failed read config data");
	}
	if (r > 0)
		ts_info("success read config, len:%d", r);
	/* clear command */
	goodix_cmds_init(&ts_cmd, TS_CMD_REG_READY, 0, cmd_reg);
	goodix_send_command(dev, &ts_cmd);

	/*enable doze mode */
	if (dev->ic_type == IC_TYPE_NORMANDY)
		goodix_set_i2c_doze_mode(dev, true);

exit:
	return r;
}

/**
 * goodix_hw_init - hardware initialize
 *   Called by touch core module when bootup
 * @ts_dev: pointer to touch device
 * return: 0 - no error, <0 error
*/
static int goodix_hw_init(struct goodix_ts_device *ts_dev)
{
	int r;

	BUG_ON(!ts_dev);
	ts_info("goodix_hw_init");
	/* goodix_hw_init may be called many times */
	if (!ts_dev->normal_cfg) {
		ts_dev->normal_cfg = devm_kzalloc(ts_dev->dev,
						  sizeof(*ts_dev->normal_cfg),
						  GFP_KERNEL);
		if (!ts_dev->normal_cfg) {
			ts_err("Failed to alloc memory for normal cfg");
			return -ENOMEM;
		}
		mutex_init(&ts_dev->normal_cfg->lock);
	}
	if (!ts_dev->highsense_cfg) {
		ts_dev->highsense_cfg = devm_kzalloc(ts_dev->dev,
						     sizeof
						     (*ts_dev->highsense_cfg),
						     GFP_KERNEL);
		if (!ts_dev->highsense_cfg) {
			ts_err("Failed to alloc memory for high sense cfg");
			return -ENOMEM;
		}
		mutex_init(&ts_dev->highsense_cfg->lock);
	}

	/*for Nanjing IC, close HID_I2C mode when driver is probed */
	if (ts_dev->ic_type == IC_TYPE_NANJING) {
		r = goodix_close_hidi2c_mode(ts_dev);
		if (r < 0)
			ts_info("close hid i2c mode FAILED");
	}

	/* read chip version: PID/VID/sensor ID,etc. */
	r = goodix_read_version(ts_dev, &ts_dev->chip_version);
	if (r < 0)
		return r;

	/* devicetree property like resolution(panel_max_xxx)
	 * may be different between sensors, here we try to parse
	 * parameters form sensor child node */
	r = goodix_parse_customize_params(ts_dev,
					  ts_dev->board_data,
					  ts_dev->chip_version.sensor_id);
	if (r < 0)
		ts_info("Cann't find customized parameters");

	ts_dev->normal_cfg->delay = 500;
	/* send normal-cfg to firmware */
	r = goodix_send_config(ts_dev, ts_dev->normal_cfg);

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
	u8 data[2] = { 0x00 };
	int r = 0;

	ts_info("HW reset");
	ts_info("ic_type = %d\n", dev->ic_type);
	dev->ic_type = IC_TYPE_NORMANDY;
	if (dev->ic_type == IC_TYPE_NORMANDY) {
		ts_info("touchpanel normandy reset");
		//gpio_direction_output(dev->board_data->reset_gpio, 1);
		//msleep(10);
		gpio_direction_output(dev->board_data->reset_gpio, 0);
		udelay(2000);
		gpio_direction_output(dev->board_data->reset_gpio, 1);
		msleep(100);
	} else if (dev->ic_type == IC_TYPE_NANJING) {
		ts_info("nanjing reset");

		/*close watch dog */
		data[0] = 0;
		goodix_i2c_write(dev, 0x40b0, data, 1);
		msleep(10);

		/*soft reset */
		data[0] = 1;
		goodix_i2c_write_trans_once(dev, 0x4180, data, 1);
		msleep(250);
		goodix_close_hidi2c_mode(dev);

		/*clear coor_reg */
		data[0] = 0;
		data[1] = 0;
		goodix_i2c_write(dev, 0x824d, data, 2);
	}

	/*init static esd */
	data[0] = GOODIX_ESD_TICK_WRITE_DATA;
	if (dev->ic_type == IC_TYPE_NANJING) {
		r = goodix_i2c_write(dev, 0x8043, data, 1);
		if (r < 0)
			ts_err
			    ("nanjing reset, init static esd FAILED, i2c wirte ERROR");
	}

	/*init dynamic esd */
	if (dev->reg.esd) {
		r = goodix_i2c_write_trans(dev, dev->reg.esd, data, 1);
		if (r < 0)
			ts_err
			    ("IC reset, init dynamic esd FAILED, i2c write ERROR");
	} else
		ts_info("reg.esd is NULL, skip dynamic esd init");

	return 0;
}

/**
 * goodix_request_handler - handle firmware request
 *
 * @dev: pointer to touch device
 * @request_data: requset information
 * Returns 0 - succeed,<0 - failed
 */
static int goodix_request_handler(struct goodix_ts_device *dev,
				  struct goodix_request_data *request_data)
{
	unsigned char buffer[1];
	int r;

	if (dev->reg.fw_request != 0x6F6D) {
		ts_info("firmware reg is wrong!\n");
		dev->reg.fw_request = 0x6F6D;
	}
	r = goodix_i2c_read_trans(dev, dev->reg.fw_request, buffer, 1);	/*TS_REG_REQUEST */
	if (r < 0)
		return r;

	switch (buffer[0]) {
	case REQUEST_CONFIG:
		ts_info("HW request config");
		goodix_send_config(dev, dev->normal_cfg);
		goto clear_requ;
		break;
	case REQUEST_BAKREF:
		ts_info("HW request bakref");
		goto clear_requ;
		break;
	case REQUEST_RESET:
		ts_info("HW requset reset");
		goto clear_requ;
		break;
	case REQUEST_MAINCLK:
		ts_info("HW request mainclk");
		goto clear_requ;
		break;
	default:
		ts_info("Unknown hw request:%d", buffer[0]);
		return 0;
	}

clear_requ:
	buffer[0] = 0x00;
	r = goodix_i2c_write_trans(dev, dev->reg.fw_request, buffer, 1);	/*TS_REG_REQUEST */
	return r;
}

/*goodix_swap_coords - swap coord
 */

static void goodix_swap_coords(struct goodix_ts_device *dev,
			       struct goodix_ts_coords *coords, int touch_num)
{
	int i, temp;
	struct goodix_ts_board_data *bdata = dev->board_data;
	for (i = 0; i < touch_num; i++) {
		if (bdata->swap_axis) {
			temp = coords->x;
			coords->x = coords->y;
			coords->y = temp;
		}
		if (bdata->x2x)
			coords->x = bdata->panel_max_x - coords->x;
		if (bdata->y2y)
			coords->y = bdata->panel_max_y - coords->y;
		coords++;
	}
}

static int goodix_remap_trace_id(struct goodix_ts_device *dev,
				 u8 *coor_buf, u32 coor_buf_len, int touch_num)
{
	static u8 remap_array[20] = { 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff
	};
	int i, j;
	int offset = 0;
	bool need_leave = false;
	bool need_add = false;
	u8 temp_buf[BYTES_PER_COORD] = { 0x00 };
	u8 *small;
	bool need_swap = false;
	int max_touch_num = dev->board_data->panel_max_id;

	if (touch_num > dev->board_data->panel_max_id) {
		ts_err("touch num error, trace id no remap:%d", touch_num);
		return 0;
	}

	if (!coor_buf || coor_buf_len > (2 + BYTES_PER_COORD * max_touch_num)) {
		ts_err("touch data buff error, !coor_buf:%d, len:%d", !coor_buf,
		       coor_buf_len);
		return 0;
	}

	/*clear and reset remap_array */
	if (touch_num == 0) {
		for (i = 0; i < sizeof(remap_array); i++)
			remap_array[i] = 0xff;
		return 0;
	}

	/*find and add new point */
	offset = 0;
	for (i = 0; i < touch_num; i++) {
		need_add = true;
		for (j = 0; j < sizeof(remap_array); j++) {
			if (coor_buf[offset] == remap_array[j]) {
				need_add = false;
				break;
			}
		}
		if (need_add == true) {
			for (j = 0; j < sizeof(remap_array); j++) {
				if (remap_array[j] == 0xff) {
					remap_array[j] = coor_buf[offset];
					break;
				}
			}
		}
		offset += BYTES_PER_COORD;
	}

	/*scan remap_array, find and remove leave point */
	for (i = 0; i < sizeof(remap_array); i++) {
		if (remap_array[i] == 0xff)
			continue;
		else {
			need_leave = true;
			offset = 0;
			for (j = 0; j < touch_num; j++) {
				if (remap_array[i] == coor_buf[offset]) {
					need_leave = false;
					break;
				}
				offset += BYTES_PER_COORD;
			}
			if (need_leave == true) {
				/*ts_info("---leave, trace id:%d:%d", remap_array[i], i); */
				remap_array[i] = 0xff;
			}
		}
	}

	/*remap trace id */
	offset = 0;
	for (i = 0; i < touch_num; i++) {
		/*do not remap pen's trace ID */
		if (coor_buf[offset] >= 0x80) {
			offset += BYTES_PER_COORD;
			continue;
		} else {
			for (j = 0; j < sizeof(remap_array); j++) {
				if (remap_array[j] == coor_buf[offset]) {
					/*ts_info("***remap, %d--->%d", coor_buf[offset], j); */
					coor_buf[offset] = j;
					break;
				}
			}
			if (j >= sizeof(remap_array)) {
				ts_err("remap ERROR!!trace id:%d",
				       coor_buf[offset]);
				ts_err("remap_array:%*ph",
				       (int)sizeof(remap_array), remap_array);
			}
			offset += BYTES_PER_COORD;
		}

	}

	/*for (i = 0; i < touch_num; i++) {
	   ts_info("remap data%d:0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x",
	   i, coor_buf[i * 8], coor_buf[i * 8 + 1],
	   coor_buf[i * 8 + 2], coor_buf[i * 8 + 3],
	   coor_buf[i * 8 + 4], coor_buf[i * 8 + 5],
	   coor_buf[i * 8 + 6], coor_buf[i * 8 + 7]);
	   } */

	/*realign coor data by new trace ID */
	for (i = 0; i < touch_num - 1; i++) {
		small = &coor_buf[BYTES_PER_COORD * i];
		need_swap = false;
		for (j = i + 1; j < touch_num; j++) {
			if (coor_buf[BYTES_PER_COORD * j] < *small) {
				need_swap = true;
				small = &coor_buf[BYTES_PER_COORD * j];
			}
		}
		/*swap */
		if (need_swap) {
			memcpy(temp_buf, small, BYTES_PER_COORD);
			memcpy(small,
			       &coor_buf[BYTES_PER_COORD * i], BYTES_PER_COORD);
			memcpy(&coor_buf[BYTES_PER_COORD * i],
			       temp_buf, BYTES_PER_COORD);
		}
	}

	/*for (i = 0; i < touch_num; i++) {
	   ts_info("final data%d:0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x",
	   i, coor_buf[i * 8], coor_buf[i * 8 + 1],
	   coor_buf[i * 8 + 2], coor_buf[i * 8 + 3],
	   coor_buf[i * 8 + 4], coor_buf[i * 8 + 5],
	   coor_buf[i * 8 + 6], coor_buf[i * 8 + 7]);
	   } */

	return 0;
}

/**
 * goodix_event_handler - handle firmware event
 *
 * @dev: pointer to touch device
 * @ts_event: pointer to touch event structure
 * Returns 0 - succeed,<0 - failed
 */

static int goodix_touch_handler(struct goodix_ts_device *dev,
				struct goodix_ts_event *ts_event,
				u8 *pre_buf, u32 pre_buf_len)
{
	struct goodix_touch_data *touch_data = &ts_event->event_data.touch_data;
	struct goodix_ts_coords *coords = &(touch_data->coords[0]);
	int max_touch_num = dev->board_data->panel_max_id;
	unsigned char buffer[4 + BYTES_PER_COORD * max_touch_num];
	unsigned char coord_sta;
	int touch_num = 0, i, r;
	unsigned char chksum = 0;

	if (!pre_buf || pre_buf_len != (4 + BYTES_PER_COORD)) {
		r = -EINVAL;
		return r;
	}

	/*copy data to buffer */
	memcpy(buffer, pre_buf, pre_buf_len);

	/* buffer[1]: touch state */
	coord_sta = buffer[1];

	touch_num = coord_sta & 0x0F;

	if (unlikely(touch_num > max_touch_num)) {
		touch_num = -EINVAL;
		goto exit_clean_sta;
	} else if (unlikely(touch_num > 1)) {
		r = goodix_i2c_read_trans(dev, dev->reg.coor + 4 + BYTES_PER_COORD,	/*TS_REG_COORDS_BASE */
					  &buffer[4 + BYTES_PER_COORD],
					  (touch_num - 1) * BYTES_PER_COORD);
		if (unlikely(r < 0))
			goto exit_clean_sta;
	}

	/* touch_num * BYTES_PER_COORD + 1(touch event state)
	 * + 1(checksum) + 1(key value) */
	if (dev->ic_type == IC_TYPE_NANJING) {
		chksum = checksum_u8(&buffer[1],
				     touch_num * BYTES_PER_COORD + 3);
	} else {
		chksum = checksum_u8(&buffer[0],
				     touch_num * BYTES_PER_COORD + 4);
	}
	if (unlikely(chksum != 0)) {
		ts_err("Checksum error:%X, ic_type:%d", chksum, dev->ic_type);
		r = -EINVAL;
		goto exit_clean_sta;
	}

	touch_data->have_key = false;	/*clear variable */
	touch_data->key_value = 0;	/*clear variable */
	touch_data->have_key = (coord_sta >> 4) & 0x01;
	if (unlikely(touch_data->have_key)) {
		touch_data->key_value = buffer[touch_num * BYTES_PER_COORD + 2];
		if (dev->board_data->pen_enable)
			touch_data->key_value = (touch_data->key_value & 0x0f) |
			    ((touch_data->key_value & 0xf0) >> (4 -
								dev->board_data->tp_key_num));
	}

	/*ts_info("$$$$$$coord_sta:0x%02x, have_key:%d, key_value:0x%02x",
	   coord_sta, touch_data->have_key, touch_data->key_value); */

	/*for (i = 0; i < touch_num; i++) {
	   ts_info("raw coor data%d:0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x",
	   i, buffer[i * 8 + 2], buffer[i * 8 + 3],
	   buffer[i * 8 + 4], buffer[i * 8 + 5],
	   buffer[i * 8 + 6], buffer[i * 8 + 7],
	   buffer[i * 8 + 8], buffer[i * 8 + 9]);
	   } */

	/*add end */

	/*remap trace id */
	if (dev->ic_type == IC_TYPE_NANJING)
		goodix_remap_trace_id(dev, &buffer[2],
				      2 + BYTES_PER_COORD * max_touch_num,
				      touch_num);

	/*clear buffer */
	memset(touch_data->coords, 0x00, sizeof(touch_data->coords));
	memset(touch_data->pen_coords, 0x00, sizeof(touch_data->pen_coords));

	if (likely(touch_num >= 1)) {
		/*"0 ~ touch_num - 2" is finger, "touch_num - 1" may be a finger or a pen */
		/*process "0 ~ touch_num -2" */
		for (i = 0; i < touch_num - 1; i++) {
			coords->id = buffer[i * BYTES_PER_COORD + 2] & 0x0f;
			coords->x = buffer[i * BYTES_PER_COORD + 3] |
			    (buffer[i * BYTES_PER_COORD + 4] << 8);
			coords->y = buffer[i * BYTES_PER_COORD + 5] |
			    (buffer[i * BYTES_PER_COORD + 6] << 8);
			coords->w = buffer[i * BYTES_PER_COORD + 7];
			coords->p = coords->w;
			coords->overlapping_area = buffer[8];
			coords->area = buffer[i * BYTES_PER_COORD + 9];
			coords++;
		}

		/*process "touch_num - 1", it may be a finger or a pen */
		/*it's a pen */
		i = touch_num - 1;
		//ts_err("%s:i=%d touch_num=%d\n",__func__,i,touch_num);
		if (unlikely(touch_num >= 1 && buffer[i * BYTES_PER_COORD + 2] >= 0x80)) {
			if (dev->board_data->pen_enable) {
				touch_data->pen_down = true;
				/*change pen's trace ID, let it equal to "panel_max_id - 1"*/
				/*touch_data->pen_coords[0].id = dev->board_data->panel_max_id - 1;*/
				touch_data->pen_coords[0].id = dev->board_data->panel_max_id * 2;
				touch_data->pen_coords[0].x = buffer[i * BYTES_PER_COORD + 3] |
					(buffer[i * BYTES_PER_COORD + 4] << 8);
				touch_data->pen_coords[0].y = buffer[i * BYTES_PER_COORD + 5] |
					(buffer[i * BYTES_PER_COORD + 6] << 8);
				touch_data->pen_coords[0].w = buffer[i * BYTES_PER_COORD + 7];
				touch_data->pen_coords[0].p = touch_data->pen_coords[0].w;
			} else {
				coords->id = buffer[i * BYTES_PER_COORD + 2] & 0x0f;
				coords->x = buffer[i * BYTES_PER_COORD + 3] |
									(buffer[i * BYTES_PER_COORD + 4] << 8);
				coords->y = buffer[i * BYTES_PER_COORD + 5] |
									(buffer[i * BYTES_PER_COORD + 6] << 8);
				coords->w = buffer[i * BYTES_PER_COORD + 7];
				coords->p = coords->w;
				coords->overlapping_area = buffer[8];
				coords->area = buffer[i * BYTES_PER_COORD + 9];
				/*ts_debug("EF:[%d](%d, %d)", coords->id, coords->x, coords->y);*/
				if (touch_data->pen_down == true) {
					touch_data->pen_down = false;
					/*ts_info("***pen leave");*/
				}
			}
		} else {/*it's a finger */
			coords->id = buffer[i * BYTES_PER_COORD + 2] & 0x0f;
			coords->x = buffer[i * BYTES_PER_COORD + 3] |
			    (buffer[i * BYTES_PER_COORD + 4] << 8);
			coords->y = buffer[i * BYTES_PER_COORD + 5] |
			    (buffer[i * BYTES_PER_COORD + 6] << 8);
			coords->w = buffer[i * BYTES_PER_COORD + 7];
			coords->p = coords->w;
			coords->overlapping_area = buffer[8];
			coords->area = buffer[i * BYTES_PER_COORD + 9];
			/*ts_debug("EF:[%d](%d, %d)", coords->id, coords->x, coords->y); */
			if (touch_data->pen_down == true) {
				touch_data->pen_down = false;
				/*ts_info("***pen leave"); */
			}
		}
	}
	/*swap coord */
	goodix_swap_coords(dev, &touch_data->coords[0], touch_num);
	goodix_swap_coords(dev, &touch_data->pen_coords[0], 1);

	touch_data->touch_num = touch_num;
	/* mark this event as touch event */
	ts_event->event_type = EVENT_TOUCH;
	r = 0;
exit_clean_sta:
	/* handshake */
	/*buffer[0] = 0x00; */
	/*goodix_i2c_write_trans(dev, dev->reg.coor, buffer, 1); */
	return r;
}

static int goodix_event_handler(struct goodix_ts_device *dev,
				struct goodix_ts_event *ts_event)
{
	unsigned char pre_buf[4 + BYTES_PER_COORD];
	unsigned char event_sta;
	struct i2c_client *client = to_i2c_client(dev->dev);
	struct goodix_ts_core *core_data = i2c_get_clientdata(client);
	int r;

	ts_log("goodix_event_handler enter");
	/*read big palm event if touch palm sensor was enabled*/
	if (core_data->palm_sensor_enabled) {
		r = goodix_i2c_read_trans(dev, dev->reg.palm_reg, pre_buf, 1);
		if (unlikely(r < 0))
			return r;
		ts_log("%s palm event reg addr 0x%04x, return 0x%02x\n", __func__, dev->reg.palm_reg, pre_buf[0]);
		if ((pre_buf[0] & 0xf0) == 0x20) {
			ts_info("%s palm event detected, inform touch psensor\n", __func__);
			core_data->palm_event = true;
			update_palm_sensor_value(1);
			return r;
		}
	}

	/*memset(pre_buf, 0, sizeof(pre_buf)); */
	r = goodix_i2c_read_trans(dev, dev->reg.coor,
				  pre_buf, 4 + BYTES_PER_COORD);
	if (unlikely(r < 0))
		return r;

	/* buffer[0]: event state */
	core_data->event_status = pre_buf[0];
	event_sta = pre_buf[0];
	ts_log("IRQ event type:%02x", event_sta);
	if (likely((event_sta & GOODIX_TOUCH_EVENT) == GOODIX_TOUCH_EVENT)) {
		/*handle touch event */
		goodix_touch_handler(dev, ts_event, pre_buf, 4 + BYTES_PER_COORD);
	} else
	    if (unlikely
		((event_sta & GOODIX_REQUEST_EVENT) == GOODIX_REQUEST_EVENT)) {
		/* handle request event */
		ts_event->event_type = EVENT_REQUEST;
		goodix_request_handler(dev, &ts_event->event_data.request_data);
	} else if ((event_sta & GOODIX_GESTURE_EVENT) == GOODIX_GESTURE_EVENT) {
		/* handle gesture event */
		ts_info("Gesture event");
	} else if ((event_sta & GOODIX_HOTKNOT_EVENT) == GOODIX_HOTKNOT_EVENT) {
		/* handle hotknot event */
		ts_info("Hotknot event");
	} else {
		ts_info("unknow event type:%02x", event_sta);
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

	goodix_cmds_init(&sleep_cmd, COMMAND_SLEEP, 0, dev->reg.command);
	if (sleep_cmd.initialized) {
		r = goodix_send_command(dev, &sleep_cmd);
		if (!r)
			ts_info("Chip in sleep mode");
	} else
		ts_err("Uninitialized sleep command");

	return r;
}

/**
 * goodix_hw_resume - Let touch deivce stay in active  mode.
 * @dev: pointer to goodix touch device
 * @return: 0 - succeed, < 0 - failed
 */
static int goodix_hw_resume(struct goodix_ts_device *dev)
{
	int r = 0, i, retry = GOODIX_BUS_RETRY_TIMES;
	u8 temp_buf[256], checksum;
	u8 data[2] = { 0x00 };

	for (; retry > 0; retry--) {
		/*resume IC */
		if (dev->ic_type == IC_TYPE_NORMANDY)
			goodix_hw_reset(dev);
		else if (dev->ic_type == IC_TYPE_NANJING) {
			/*1. read 0x8000 to resume nanjing */
			goodix_i2c_read(dev, 0x8000, data, 1);
			msleep(150);
			/*2. check resume success or not */
			for (i = 0; i < 10; i++) {
				r = goodix_i2c_read(dev, dev->reg.command, data,
						    1);
				if (!r && data[0] == 0xff)
					break;
				msleep(20);
			}

			if (i >= 10) {
				ts_err
				    ("resume nanjing failed, after read 0x8000, 0x8040 not equal 0xff");
				continue;
			}
			/*3. close hid i2c */
			goodix_close_hidi2c_mode(dev);

			/*4. clear coor */
			data[0] = 0;
			data[1] = 0;
			goodix_i2c_write(dev, 0x824d, data, 2);
		}

		/*read version and check checksum */
		if (dev->reg.version_base && dev->reg.version_len < 256) {
			r = goodix_i2c_read(dev, dev->reg.version_base,
					    temp_buf, dev->reg.version_len);
			if (r < 0)
				continue;

			checksum = checksum_u8(temp_buf, dev->reg.version_len);
			if (!checksum) {
				ts_info("read version SUCCESS");
				break;
			}
		}
	}
	return r;
}

static int goodix_esd_check(struct goodix_ts_device *dev)
{
	int r;
	u8 data = 0;

	if (dev->reg.esd == 0) {
		ts_err("esd reg is NULL");
		return 0;
	}

	/*check dynamic esd */
	if (dev->ic_type == IC_TYPE_NORMANDY)
		r = dev->hw_ops->read_trans(dev, TS_REG_ESD_TICK_R, &data, 1);
	else
		r = dev->hw_ops->read_trans(dev, dev->reg.esd, &data, 1);

	if (r < 0 || (data == GOODIX_ESD_TICK_WRITE_DATA)) {
		ts_info("dynamic esd occur, r:%d, data:0x%02x", r, data);
		r = -EINVAL;
		goto exit;
	}

	/*check static esd */
	if (dev->ic_type == IC_TYPE_NANJING) {
		r = dev->hw_ops->read_trans(dev, 0x8043, &data, 1);

		if (r < 0 || (data != 0xaa)) {
			ts_info("static esd occur, r:%d, data:0x%02x", r, data);
			r = -EINVAL;
			goto exit;
		}
	}

exit:
	return r;
}

/* hardware opeation funstions */
static const struct goodix_ts_hw_ops hw_i2c_ops = {
	.init = goodix_hw_init,
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
	kfree(goodix_pdev);
	return;
}

static int goodix_i2c_probe(struct i2c_client *client,
			    const struct i2c_device_id *dev_id)
{
	struct goodix_ts_device *ts_device = NULL;
	struct goodix_ts_board_data *ts_bdata = NULL;
	int r = 0;

	ts_err("enter::%s\n", __func__);

	r = i2c_check_functionality(client->adapter, I2C_FUNC_I2C);
	if (!r)
		return -EIO;

	/* board data */
	ts_bdata = devm_kzalloc(&client->dev,
				sizeof(struct goodix_ts_board_data),
				GFP_KERNEL);
	if (!ts_bdata)
		return -ENOMEM;

	if (IS_ENABLED(CONFIG_OF) && client->dev.of_node) {
		/* parse devicetree property */
		r = goodix_parse_dt(client->dev.of_node, ts_bdata);
		if (r < 0)
			return r;
	}
#ifdef CONFIG_ACPI
	else if (ACPI_COMPANION(&client->dev)) {
		r = goodix_parse_acpi(&client->dev, ts_bdata);
		if (r < 0)
			return r;
	}
#endif
	else {
		/* use platform data */
		devm_kfree(&client->dev, ts_bdata);
		ts_bdata = client->dev.platform_data;
	}

	if (!ts_bdata)
		return -ENODEV;

	ts_device = devm_kzalloc(&client->dev,
				 sizeof(struct goodix_ts_device), GFP_KERNEL);
	if (!ts_device)
		return -ENOMEM;

	ts_device->name = "GTx5 TouchDevcie";
	ts_device->dev = &client->dev;
	ts_device->board_data = ts_bdata;
	ts_device->hw_ops = &hw_i2c_ops;

	/* ts core device */
	goodix_pdev = kzalloc(sizeof(struct platform_device), GFP_KERNEL);
	if (!goodix_pdev)
		return -ENOMEM;

	goodix_pdev->name = GOODIX_CORE_DRIVER_NAME;
	goodix_pdev->id = 0;
	goodix_pdev->num_resources = 0;
	/*
	 * you could find this platform dev in
	 * /sys/devices/platfrom/goodix_ts.0
	 * goodix_pdev->dev.parent = &client->dev;
	 */
	goodix_pdev->dev.platform_data = ts_device;
	goodix_pdev->dev.release = goodix_pdev_release;

	/* register platform device, then the goodix_ts_core
	 * module will probe the touch deivce. */
	r = platform_device_register(goodix_pdev);

	ts_info("goodix9886_i2c_probe OUT");

	return r;
}

static int goodix_i2c_remove(struct i2c_client *client)
{
	platform_device_unregister(goodix_pdev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id i2c_matchs[] = {
	{.compatible = TS_DT_COMPATIBLE,},
	{},
};

MODULE_DEVICE_TABLE(of, i2c_matchs);
#endif

#ifdef CONFIG_ACPI
static const struct acpi_device_id acpi_matchs[] = {
	{.id = "PNPxxx"},
	{},
};

MODULE_DEVICE_TABLE(acpi, acpi_matchs);
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
#ifdef CONFIG_ACPI
		   .acpi_match_table = acpi_matchs,
#endif
		   },
	.probe = goodix_i2c_probe,
	.remove = goodix_i2c_remove,
	.id_table = i2c_id_table,
};

static int __init goodix_i2c_init(void)
{
	ts_info("GTx5xx HW layer init");
	return i2c_add_driver(&goodix_i2c_driver);
}

static void __exit goodix_i2c_exit(void)
{
	i2c_del_driver(&goodix_i2c_driver);
	return;
}

module_init(goodix_i2c_init);
module_exit(goodix_i2c_exit);

MODULE_DESCRIPTION("Goodix GTx5 Touchscreen Hardware Module");
MODULE_AUTHOR("Goodix, Inc.");
MODULE_LICENSE("GPL v2");
