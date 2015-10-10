/*
 *  Copyright (C) 2010,Imagis Technology Co. Ltd. All Rights Reserved.
 *  Copyright (C) 2015 XiaoMi, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <asm/unaligned.h>

#include <asm/io.h>
#include <mach/gpio.h>
#include <mach/vreg.h>
#include <linux/regulator/consumer.h>
#include <linux/err.h>

#include "ist30xx.h"
#include "ist30xx_update.h"
#include "ist30xx_tracking.h"

/******************************************************************************
 * Return value of Error
 * EPERM  : 1 (Operation not permitted)
 * ENOENT : 2 (No such file or directory)
 * EIO    : 5 (I/O error)
 * ENXIO  : 6 (No such device or address)
 * EINVAL : 22 (Invalid argument)
 *****************************************************************************/

const u32 pos_cmd = cpu_to_be32(CMD_GET_COORD);
struct i2c_msg pos_msg[READ_CMD_MSG_LEN] = {
	{
		.flags = 0,
		.len = IST30XX_ADDR_LEN,
		.buf = (u8 *)&pos_cmd,
	},
	{ .flags = I2C_M_RD, },
};

int ist30xx_get_position(struct i2c_client *client, u32 *buf, u16 len)
{
	int ret, i;

	pos_msg[0].addr = client->addr;
	pos_msg[1].addr = client->addr;
	pos_msg[1].len = len * IST30XX_DATA_LEN,
	pos_msg[1].buf = (u8 *)buf,

	ret = i2c_transfer(client->adapter, pos_msg, READ_CMD_MSG_LEN);
	if (ret != READ_CMD_MSG_LEN) {
		tsp_err("%s: i2c failed (%d)\n", __func__, ret);
		return -EIO;
	}

	for (i = 0; i < len; i++)
		buf[i] = cpu_to_be32(buf[i]);

	return 0;
}

int ist30xx_cmd_run_device(struct i2c_client *client, bool is_reset)
{
	int ret = -EIO;
	struct ist30xx_data *data = i2c_get_clientdata(client);

	if (is_reset == true) ist30xx_reset(data, false);
	ret = ist30xx_write_cmd(client, CMD_RUN_DEVICE, 0);

	ist30xx_tracking(data, TRACK_CMD_RUN_DEVICE);

	msleep(10);

	return ret;
}

int ist30xx_cmd_start_scan(struct i2c_client *client)
{
	int ret;
	struct ist30xx_data *data = i2c_get_clientdata(client);

	ret = ist30xx_write_cmd(client, CMD_START_SCAN, 0);

	ist30xx_tracking(data, TRACK_CMD_SCAN);

	msleep(100);

	data->status.noise_mode = true;

	return ret;
}

int ist30xx_cmd_calibrate(struct i2c_client *client)
{
	int ret = ist30xx_write_cmd(client, CMD_CALIBRATE, 0);
	struct ist30xx_data *data = i2c_get_clientdata(client);

	ist30xx_tracking(data, TRACK_CMD_CALIB);

	tsp_info("%s\n", __func__);

	msleep(100);

	return ret;
}

int ist30xx_cmd_check_calib(struct i2c_client *client)
{
	int ret = ist30xx_write_cmd(client, CMD_CHECK_CALIB, 0);
	struct ist30xx_data *data = i2c_get_clientdata(client);

	ist30xx_tracking(data, TRACK_CMD_CHECK_CALIB);

	tsp_info("*** Check Calibration cmd ***\n");

	msleep(20);

	return ret;
}

int ist30xx_cmd_update(struct i2c_client *client, int cmd)
{
	u32 val = (cmd == CMD_ENTER_FW_UPDATE ? CMD_FW_UPDATE_MAGIC : 0);
	int ret = ist30xx_write_cmd(client, cmd, val);
	struct ist30xx_data *data = i2c_get_clientdata(client);

	ist30xx_tracking(data, TRACK_CMD_FWUPDATE);

	msleep(10);

	return ret;
}

int ist30xx_cmd_reg(struct i2c_client *client, int cmd)
{
	int ret = ist30xx_write_cmd(client, cmd, 0);
	struct ist30xx_data *data = i2c_get_clientdata(client);

	if (cmd == CMD_ENTER_REG_ACCESS) {
		ist30xx_tracking(data, TRACK_CMD_ENTER_REG);
		msleep(100);
	} else if (cmd == CMD_EXIT_REG_ACCESS) {
		ist30xx_tracking(data, TRACK_CMD_EXIT_REG);
		msleep(10);
	}

	return ret;
}


int ist30xx_read_cmd(struct i2c_client *client, u32 cmd, u32 *buf)
{
	int ret;
	u32 le_reg = cpu_to_be32(cmd);

	struct i2c_msg msg[READ_CMD_MSG_LEN] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = IST30XX_ADDR_LEN,
			.buf = (u8 *)&le_reg,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = IST30XX_DATA_LEN,
			.buf = (u8 *)buf,
		},
	};

	ret = i2c_transfer(client->adapter, msg, READ_CMD_MSG_LEN);
	if (ret != READ_CMD_MSG_LEN) {
		tsp_err("%s: i2c failed (%d), cmd: %x\n", __func__, ret, cmd);
		return -EIO;
	}
	*buf = cpu_to_be32(*buf);

	return 0;
}


int ist30xx_write_cmd(struct i2c_client *client, u32 cmd, u32 val)
{
	int ret;
	struct i2c_msg msg;
	u8 msg_buf[IST30XX_ADDR_LEN + IST30XX_DATA_LEN];

	put_unaligned_be32(cmd, msg_buf);
	put_unaligned_be32(val, msg_buf + IST30XX_ADDR_LEN);

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = IST30XX_ADDR_LEN + IST30XX_DATA_LEN;
	msg.buf = msg_buf;

	ret = i2c_transfer(client->adapter, &msg, WRITE_CMD_MSG_LEN);
	if (ret != WRITE_CMD_MSG_LEN) {
		tsp_err("%s: i2c failed (%d), cmd: %x(%x)\n", __func__, ret, cmd, val);
		return -EIO;
	}

	return 0;
}

#define IST_VTG_MIN_UV          2600000
#define IST_VTG_MAX_UV          3300000
#define IST_I2C_VTG_MIN_UV      1800000
#define IST_I2C_VTG_MAX_UV      1800000

static int ts_power_on(struct ist30xx_data *data, bool on)
{
	int rc;

	if (!on)
		goto power_off;

	rc = regulator_enable(data->vdd);
	if (rc) {
		tsp_err("Regulator vdd enable failed rc=%d\n", rc);
		return rc;
	}

	rc = regulator_enable(data->vcc_i2c);
	if (rc) {
		tsp_err("Regulator vcc_i2c enable failed rc=%d\n", rc);
		regulator_disable(data->vdd);
	}

	return rc;

power_off:
	rc = regulator_disable(data->vdd);
	if (rc) {
		tsp_err("Regulator vdd disable failed rc=%d\n", rc);
		return rc;
	}

	rc = regulator_disable(data->vcc_i2c);
	if (rc) {
		tsp_err("Regulator vcc_i2c disable failed rc=%d\n", rc);
		regulator_enable(data->vdd);
	}

	return rc;
}

static int ts_power_init(struct ist30xx_data *data, bool on)
{
	int rc;

	if (!on)
		goto pwr_deinit;

	data->vdd = regulator_get(&data->client->dev, "vdd");
	if (IS_ERR(data->vdd)) {
		rc = PTR_ERR(data->vdd);
		tsp_err("Regulator get failed vdd rc=%d\n", rc);
		return rc;
	}

	if (regulator_count_voltages(data->vdd) > 0) {
		rc = regulator_set_voltage(data->vdd, IST_VTG_MIN_UV,
					   IST_VTG_MAX_UV);
		if (rc) {
			tsp_err("Regulator set_vtg failed vdd rc=%d\n", rc);
			goto reg_vdd_put;
		}
	}

	data->vcc_i2c = regulator_get(&data->client->dev, "vcc_i2c");
	if (IS_ERR(data->vcc_i2c)) {
		rc = PTR_ERR(data->vcc_i2c);
		tsp_err("Regulator get failed vcc_i2c rc=%d\n", rc);
		goto reg_vdd_set_vtg;
	}

	if (regulator_count_voltages(data->vcc_i2c) > 0) {
		rc = regulator_set_voltage(data->vcc_i2c, IST_I2C_VTG_MIN_UV,
					   IST_I2C_VTG_MAX_UV);
		if (rc) {
			tsp_err("Regulator set_vtg failed vcc_i2c rc=%d\n", rc);
			goto reg_vcc_i2c_put;
		}
	}

	return 0;

reg_vcc_i2c_put:
	regulator_put(data->vcc_i2c);
reg_vdd_set_vtg:
	if (regulator_count_voltages(data->vdd) > 0)
		regulator_set_voltage(data->vdd, 0, IST_VTG_MAX_UV);
reg_vdd_put:
	regulator_put(data->vdd);
	return rc;

pwr_deinit:
	if (regulator_count_voltages(data->vdd) > 0)
		regulator_set_voltage(data->vdd, 0, IST_VTG_MAX_UV);

	regulator_put(data->vdd);

	if (regulator_count_voltages(data->vcc_i2c) > 0)
		regulator_set_voltage(data->vcc_i2c, 0, IST_I2C_VTG_MAX_UV);

	regulator_put(data->vcc_i2c);
	return 0;
}


static void ts_power_enable(struct ist30xx_data *data, int en)
{
	if (en) {
		/* VDD 3V enable */
		//ts_power_init(data, true);
		ts_power_on(data, true);
	} else {
		/* VDD 3V disable */
		ts_power_on(data, false);
		//ts_power_init(data, false);
	}
}

int ist30xx_power_on(struct ist30xx_data *data, bool download)
{
	if (data->status.power != 1) {
		tsp_info("%s()\n", __func__);
		/* VDD enable */
		/* VDDIO enable */
		ist30xx_tracking(data, TRACK_PWR_ON);
		ts_power_enable(data, 1);
		msleep(10);
		data->status.power = 1;
		gpio_set_value_cansleep(data->pdata->reset_gpio, 1);
		if (download)
			msleep(6);
		else
			msleep(100);
	}

	return 0;
}


int ist30xx_power_off(struct ist30xx_data *data)
{
	if (data->status.power != 0) {
		tsp_info("%s()\n", __func__);
		/* VDDIO disable */
		/* VDD disable */
		ist30xx_tracking(data, TRACK_PWR_OFF);
		gpio_set_value_cansleep(data->pdata->reset_gpio, 0);
		ts_power_enable(data, 0);
		msleep(50);
		data->status.power = 0;
		data->status.noise_mode = false;
	}

	return 0;
}


int ist30xx_reset(struct ist30xx_data *data, bool download)
{
	tsp_info("%s()\n", __func__);
	ist30xx_power_off(data);
	msleep(10);
	ist30xx_power_on(data, download);

	data->status.power = 1;
	return 0;
}


int ist30xx_internal_suspend(struct ist30xx_data *data)
{
	ist30xx_power_off(data);
	return 0;
}


int ist30xx_internal_resume(struct ist30xx_data *data)
{
	ist30xx_power_on(data, false);
	ist30xx_cmd_run_device(data->client, false);
	return 0;
}


int ist30xx_init_system(struct ist30xx_data *data)
{
	int ret;

	ret = ts_power_init(data, true);
	if (ret) {
		tsp_err("%s: ist30xx_power_init (%d)\n", __func__, ret);
		return -EIO;
	}

	// TODO : place additional code here.
	ret = ist30xx_power_on(data, false);
	if (ret) {
		tsp_err("%s: ist30xx_power_on failed (%d)\n", __func__, ret);
		return -EIO;
	}

	return 0;
}
