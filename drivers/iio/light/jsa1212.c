/*
 * JSA1212 Ambient Light & Proximity Sensor Driver
 *
 * Copyright (c) 2014, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/acpi.h>
#include <linux/gpio/consumer.h>

#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/events.h>

/* JSA1212 reg address */
#define JSA1212_CONF_REG		0x01
#define JSA1212_INT_REG			0x02
#define JSA1212_PXS_LT_REG		0x03
#define JSA1212_PXS_HT_REG		0x04
#define JSA1212_ALS_TH1_REG		0x05
#define JSA1212_ALS_TH2_REG		0x06
#define JSA1212_ALS_TH3_REG		0x07
#define JSA1212_PXS_DATA_REG		0x08
#define JSA1212_ALS_DT1_REG		0x09
#define JSA1212_ALS_DT2_REG		0x0A
#define JSA1212_ALS_RNG_REG		0x0B

/* JSA1212 reg defaults */
#define JSA1212_CONF_REG_DEF		0x58
#define JSA1212_INT_REG_DEF		0x02
#define JSA1212_PXS_LT_REG_DEF		0x00
#define JSA1212_PXS_HT_REG_DEF		0xFF
#define JSA1212_ALS_TH1_REG_DEF		0x00
#define JSA1212_ALS_TH2_REG_DEF		0xF0
#define JSA1212_ALS_TH3_REG_DEF		0xFF
#define JSA1212_ALS_RNG_REG_DEF		0x00

/* JSA1212 reg masks */
#define JSA1212_CONF_MASK		0xFF
#define JSA1212_INT_MASK		0xFF
#define JSA1212_PXS_LT_MASK		0xFF
#define JSA1212_PXS_HT_MASK		0xFF
#define JSA1212_ALS_TH1_MASK		0xFF
#define JSA1212_ALS_TH2_LT_MASK		0x0F
#define JSA1212_ALS_TH2_HT_MASK		0xF0
#define JSA1212_ALS_TH3_MASK		0xFF
#define JSA1212_PXS_DATA_MASK		0xFF
#define JSA1212_ALS_DT1_MASK		0xFF
#define JSA1212_ALS_DT2_MASK		0x0F
#define JSA1212_ALS_RNG_MASK		0x07
#define JSA1212_REG_MASK		0xFF

/* JSA1212 CONF REG bits */
#define JSA1212_CONF_PXS_MASK		0x80
#define JSA1212_CONF_PXS_ENABLE		0x80
#define JSA1212_CONF_PXS_DISABLE	0x00
#define JSA1212_CONF_ALS_MASK		0x04
#define JSA1212_CONF_ALS_ENABLE		0x04
#define JSA1212_CONF_ALS_DISABLE	0x00

/* JSA1212 INT REG bits */
#define JSA1212_INT_CTRL_MASK		0x01
#define JSA1212_INT_CTRL_EITHER		0x00
#define JSA1212_INT_CTRL_BOTH		0x01
#define JSA1212_INT_ALS_PRST_MASK	0x06
#define JSA1212_INT_ALS_PRST_1CONV	0x00
#define JSA1212_INT_ALS_PRST_4CONV	0x02
#define JSA1212_INT_ALS_PRST_8CONV	0x04
#define JSA1212_INT_ALS_PRST_16CONV	0x06
#define JSA1212_INT_ALS_FLAG_MASK	0x08
#define JSA1212_INT_ALS_FLAG_CLR	0x00
#define JSA1212_INT_PXS_PRST_MASK	0x60
#define JSA1212_INT_PXS_PRST_1CONV	0x00
#define JSA1212_INT_PXS_PRST_4CONV	0x20
#define JSA1212_INT_PXS_PRST_8CONV	0x40
#define JSA1212_INT_PXS_PRST_16CONV	0x60
#define JSA1212_INT_PXS_FLAG_MASK	0x80
#define JSA1212_INT_PXS_FLAG_CLR	0x00

/* JSA1212 ALS RNG REG bits */
#define JSA1212_ALS_RNG_0_2048		0x00
#define JSA1212_ALS_RNG_0_1024		0x01
#define JSA1212_ALS_RNG_0_512		0x02
#define JSA1212_ALS_RNG_0_256		0x03
#define JSA1212_ALS_RNG_0_128		0x04

/* JSA1212 INT threshold range */
#define JSA1212_ALS_TH_MIN	0x0000
#define JSA1212_ALS_TH_MAX	0x0FFF
#define JSA1212_PXS_TH_MIN	0x00
#define JSA1212_PXS_TH_MAX	0xFF

#define JSA1212_I2C_RETRY	0x05
#define JSA1212_ALS_DELAY_MS	0xC8
#define JSA1212_PXS_DELAY_MS	0x64

enum jsa1212_op_mode {
	JSA1212_OPMODE_ALS_EN,
	JSA1212_OPMODE_PXS_EN,
	JSA1212_OPMODE_ALS_EV_EN,
	JSA1212_OPMODE_PXS_EV_EN,
};

enum jsa1212_cmd_id {
	JSA1212_CMD_POWEROFF,
	JSA1212_CMD_SUSPEND,
	JSA1212_CMD_RAW_ALS,
	JSA1212_CMD_RAW_PXS,
	JSA1212_CMD_ALS_EV_EN,
	JSA1212_CMD_ALS_EV_DIS,
	JSA1212_CMD_PXS_EV_EN,
	JSA1212_CMD_PXS_EV_DIS,
	JSA1212_CMD_RESUME,
};

/* Config Register modes */
enum jsa1212_conf_mode {
	JSA1212_CONFMODE_DIS_ALL,
	JSA1212_CONFMODE_ALS_EN,
	JSA1212_CONFMODE_PXS_EN,
	JSA1212_CONFMODE_ALS_DIS,
	JSA1212_CONFMODE_PXS_DIS,
};

/* Interrrupt Register modes */
enum jsa1212_int_mode {
	JSA1212_INTMODE_ALS_ENABLE,
	JSA1212_INTMODE_ALS_DISABLE,
	JSA1212_INTMODE_PXS_ENABLE,
	JSA1212_INTMODE_PXS_DISABLE,
};

/* Threshold Register types */
enum jsa1212_threshold_id {
	JSA1212_THRESH_ALS,
	JSA1212_THRESH_PXS,
};

enum jsa1212_threshold_type {
	JSA1212_THRESH_LOW,
	JSA1212_THRESH_HIGH,
	JSA1212_THRESH_MAX,
};

struct jsa1212_data {
	struct i2c_client *client;
	struct mutex lock;
	u16 als_thresh[JSA1212_THRESH_MAX];
	u8 pxs_thresh[JSA1212_THRESH_MAX];
	u8 als_prst_val;
	u8 pxs_prst_val;
	u8 als_rng_idx;
	unsigned long flags;
	unsigned long state_flags; /* Caches chip state before suspend */
	int gpio_irq;
};

/* ALS range idx to val mapping */
unsigned int als_range_val[] = {2048, 1024, 512, 256, 128, 128, 128, 128};

static int jsa1212_reg_write(struct jsa1212_data *jsa1212_data, u8 reg,
				u8 in_data)
{
	int ret;

	ret = i2c_smbus_write_byte_data(jsa1212_data->client, reg, in_data);

	if (ret < 0)
		dev_err(&jsa1212_data->client->dev,
			"jsa1212 reg write (0x%02X - 0x%02X) error(%d)\n",
			reg, in_data, ret);
	return ret;
}

static int jsa1212_reg_read(struct jsa1212_data *jsa1212_data, u8 reg)
{
	int ret;

	ret = i2c_smbus_read_byte_data(jsa1212_data->client, reg);

	if (ret < 0)
		dev_err(&jsa1212_data->client->dev,
			"jsa1212 reg read (0x%02X) error(%d)\n",
			reg, ret);
	return ret;
}

static int jsa1212_reg_update(struct jsa1212_data *jsa1212_data, u8 reg,
				u8 bit_mask, u8 new_val)
{
	u8 old_cmd, new_cmd;
	int ret;

	ret = jsa1212_reg_read(jsa1212_data, reg);

	if (ret < 0) {
		dev_err(&jsa1212_data->client->dev,
				"%s: read reg (0X%02X) cmd failed\n",
				__func__, reg);
		return ret;
	}

	old_cmd = ret & JSA1212_REG_MASK;

	new_cmd = old_cmd & ~bit_mask;

	new_cmd |= new_val & bit_mask;

	if (new_cmd != old_cmd) {

		ret = jsa1212_reg_write(jsa1212_data, reg, new_cmd);

		if (ret < 0) {
			dev_err(&jsa1212_data->client->dev,
				"%s: write reg (0X%02X) cmd (0X%02X) failed\n",
				__func__, reg, new_cmd);
			return ret;
		 }
	}

	 return 0;
}

/* Sets configure register modes */
static int jsa1212_set_confmode(struct jsa1212_data *jsa1212_data,
				enum jsa1212_conf_mode confmode)
{
	int ret = -EINVAL;

	switch (confmode) {
	case JSA1212_CONFMODE_DIS_ALL:
		ret = jsa1212_reg_update(jsa1212_data, JSA1212_CONF_REG,
				JSA1212_CONF_ALS_MASK |
				JSA1212_CONF_PXS_MASK,
				JSA1212_CONF_ALS_DISABLE |
				JSA1212_CONF_PXS_DISABLE);
		if (ret < 0)
			goto set_confmode_err;

		clear_bit(JSA1212_OPMODE_PXS_EN, &jsa1212_data->flags);
		clear_bit(JSA1212_OPMODE_ALS_EN, &jsa1212_data->flags);

		break;
	case JSA1212_CONFMODE_ALS_EN:
		ret = jsa1212_reg_update(jsa1212_data, JSA1212_CONF_REG,
				JSA1212_CONF_ALS_MASK,
				JSA1212_CONF_ALS_ENABLE);
		if (ret < 0)
			goto set_confmode_err;

		set_bit(JSA1212_OPMODE_ALS_EN, &jsa1212_data->flags);

		break;
	case JSA1212_CONFMODE_ALS_DIS:
		ret = jsa1212_reg_update(jsa1212_data, JSA1212_CONF_REG,
				JSA1212_CONF_ALS_MASK,
				JSA1212_CONF_ALS_DISABLE);
		if (ret < 0)
			goto set_confmode_err;

		clear_bit(JSA1212_OPMODE_ALS_EN, &jsa1212_data->flags);

		break;
	case JSA1212_CONFMODE_PXS_EN:
		ret = jsa1212_reg_update(jsa1212_data, JSA1212_CONF_REG,
				JSA1212_CONF_PXS_MASK,
				JSA1212_CONF_PXS_ENABLE);
		if (ret < 0)
			goto set_confmode_err;

		set_bit(JSA1212_OPMODE_PXS_EN, &jsa1212_data->flags);

		break;
	case JSA1212_CONFMODE_PXS_DIS:
		ret = jsa1212_reg_update(jsa1212_data, JSA1212_CONF_REG,
				JSA1212_CONF_PXS_MASK,
				JSA1212_CONF_PXS_DISABLE);
		if (ret < 0)
			goto set_confmode_err;

		clear_bit(JSA1212_OPMODE_PXS_EN, &jsa1212_data->flags);

		break;

	default:
		break;
	}

	return ret;

set_confmode_err:
	dev_err(&jsa1212_data->client->dev,
		"jsa1212 set conf mode (%d)\n failed", confmode);

	return ret;
}

static int jsa1212_set_thresh(struct jsa1212_data *jsa1212_data,
			enum jsa1212_threshold_id thresh_id,
			enum jsa1212_threshold_type thresh_type,
			u16 val)
{
	int ret;
	u8 reg1 = 0x00, mask1, val1;
	u8 reg2 = 0x00, mask2, val2;

	switch (thresh_id) {
	case JSA1212_THRESH_ALS:
		switch (thresh_type) {
		case JSA1212_THRESH_LOW:
			reg1 = JSA1212_ALS_TH1_REG;
			mask1 = JSA1212_ALS_TH1_MASK;
			val1 = val & mask1;
			reg2 = JSA1212_ALS_TH2_REG; /* Handles upper 4 bits */
			mask2 = JSA1212_ALS_TH2_LT_MASK;
			val2 = val >> 8 & mask2;
			break;
		case JSA1212_THRESH_HIGH:
			reg1 = JSA1212_ALS_TH3_REG;
			mask1 = JSA1212_ALS_TH3_MASK;
			val1 = val >> 4 & mask1;
			reg2 = JSA1212_ALS_TH2_REG;
			mask2 = JSA1212_ALS_TH2_HT_MASK;
			val2 = val << 4 & mask2;
			break;
		default:
			ret = -EINVAL;
					break;
		}
		break;
	case JSA1212_THRESH_PXS:
		switch (thresh_type) {
		case JSA1212_THRESH_LOW:
			reg1 = JSA1212_PXS_LT_REG;
			mask1 = JSA1212_PXS_LT_MASK;
			val1 = val & mask1;
			break;
		case JSA1212_THRESH_HIGH:
			reg1 = JSA1212_PXS_HT_REG;
			mask1 = JSA1212_PXS_HT_MASK;
			val1 = val & mask1;
			break;
		default:
			ret = -EINVAL;
			break;
		}
			break;
	default:
		ret = -EINVAL;
		break;
	}

	if (reg1) {
		ret = jsa1212_reg_update(jsa1212_data, reg1, mask1, val1);
		if (ret < 0) {
			dev_err(&jsa1212_data->client->dev,
				"%s failed reg (0X%02X) val (0X%02X)\n",
				__func__, reg1, val1);
			return ret;
		}
	}

	if (reg2) {
		ret = jsa1212_reg_update(jsa1212_data, reg2, mask2, val2);
		if (ret < 0) {
			dev_err(&jsa1212_data->client->dev,
				"%s failed reg (0X%02X) val (0X%02X)\n",
				__func__, reg2, val2);
			return ret;
		}
	}

	return ret;
}

/*
 * In JSA1212 device, Interrupt enabling involves setting threshold registers
 * and enabling the interrupt modes in interrupt control register. This
 * function handles this functionality.
 */
static int jsa1212_set_intmode(struct jsa1212_data *jsa1212_data,
				enum jsa1212_int_mode intmode)
{
	int ret;
	u8 mask, cmd;

	switch (intmode) {
	case JSA1212_INTMODE_ALS_ENABLE:
		mask = JSA1212_INT_CTRL_MASK | JSA1212_INT_ALS_PRST_MASK;
		cmd = JSA1212_INT_CTRL_EITHER | jsa1212_data->als_prst_val;

		ret = jsa1212_set_thresh(jsa1212_data, JSA1212_THRESH_ALS,
				JSA1212_THRESH_LOW,
				jsa1212_data->als_thresh[JSA1212_THRESH_LOW]);
		if (ret < 0)
			goto set_intmode_err;

		ret = jsa1212_set_thresh(jsa1212_data, JSA1212_THRESH_ALS,
				JSA1212_THRESH_HIGH,
				jsa1212_data->als_thresh[JSA1212_THRESH_HIGH]);
		if (ret < 0)
			goto set_intmode_err;


		ret = jsa1212_reg_update(jsa1212_data, JSA1212_INT_REG,
					mask, cmd);
		if (ret < 0)
			goto set_intmode_err;
		break;
	case JSA1212_INTMODE_ALS_DISABLE:
		mask = JSA1212_INT_ALS_PRST_MASK;
		cmd = JSA1212_INT_ALS_PRST_4CONV;

		ret = jsa1212_set_thresh(jsa1212_data, JSA1212_THRESH_ALS,
					JSA1212_THRESH_LOW,
					JSA1212_ALS_TH_MIN);
		if (ret < 0)
			goto set_intmode_err;

		ret = jsa1212_set_thresh(jsa1212_data, JSA1212_THRESH_ALS,
					JSA1212_THRESH_HIGH,
					JSA1212_ALS_TH_MAX);
		if (ret < 0)
			goto set_intmode_err;

		ret = jsa1212_reg_update(jsa1212_data, JSA1212_INT_REG,
					mask, cmd);
		if (ret < 0)
			goto set_intmode_err;

		break;
	case JSA1212_INTMODE_PXS_ENABLE:
		mask = JSA1212_INT_CTRL_MASK | JSA1212_INT_PXS_PRST_MASK;
		cmd = JSA1212_INT_CTRL_EITHER | jsa1212_data->pxs_prst_val;

		ret = jsa1212_set_thresh(jsa1212_data, JSA1212_THRESH_PXS,
				JSA1212_THRESH_LOW,
				jsa1212_data->pxs_thresh[JSA1212_THRESH_LOW]);
		if (ret < 0)
			goto set_intmode_err;

		ret = jsa1212_set_thresh(jsa1212_data, JSA1212_THRESH_PXS,
				JSA1212_THRESH_HIGH,
				jsa1212_data->pxs_thresh[JSA1212_THRESH_HIGH]);
		if (ret < 0)
			goto set_intmode_err;

		ret = jsa1212_reg_update(jsa1212_data, JSA1212_INT_REG,
					mask, cmd);
		if (ret < 0)
			goto set_intmode_err;
		break;
	case JSA1212_INTMODE_PXS_DISABLE:
		mask = JSA1212_INT_PXS_PRST_MASK;
		cmd = JSA1212_INT_PXS_PRST_1CONV;

		ret = jsa1212_set_thresh(jsa1212_data, JSA1212_THRESH_PXS,
					JSA1212_THRESH_LOW,
					JSA1212_PXS_TH_MIN);
		if (ret < 0)
			goto set_intmode_err;

		ret = jsa1212_set_thresh(jsa1212_data, JSA1212_THRESH_PXS,
					JSA1212_THRESH_HIGH,
					JSA1212_PXS_TH_MAX);
		if (ret < 0)
			goto set_intmode_err;

		ret = jsa1212_reg_update(jsa1212_data, JSA1212_INT_REG,
					mask, cmd);
		if (ret < 0)
			goto set_intmode_err;

		break;
	default:
		ret = -EINVAL;
	}

	return ret;

set_intmode_err:
	dev_err(&jsa1212_data->client->dev,
			"jsa1212 set int mode (%d)\n failed", intmode);
	return ret;
}

static int jsa1212_send_cmd(struct jsa1212_data *jsa1212_data,
			enum jsa1212_cmd_id cmd_id)
{
	int ret = -EINVAL;
	u8 suspend_flags;

	switch (cmd_id) {
	case JSA1212_CMD_POWEROFF:
	case JSA1212_CMD_SUSPEND:
		suspend_flags = jsa1212_data->flags;

		ret = jsa1212_set_confmode(jsa1212_data,
					JSA1212_CONFMODE_DIS_ALL);
		if (ret < 0)
			goto send_cmd_err;

		jsa1212_data->state_flags = suspend_flags;

		break;
	case JSA1212_CMD_RAW_ALS:
		ret = jsa1212_set_confmode(jsa1212_data,
					JSA1212_CONFMODE_ALS_EN);
		if (ret < 0)
			goto send_cmd_err;

		break;
	case JSA1212_CMD_RAW_PXS:
		ret = jsa1212_set_confmode(jsa1212_data,
					JSA1212_CONFMODE_PXS_EN);
		if (ret < 0)
			goto send_cmd_err;

		break;
	case JSA1212_CMD_ALS_EV_EN:
		ret = jsa1212_set_intmode(jsa1212_data,
					JSA1212_INTMODE_ALS_ENABLE);
		if (ret < 0)
			goto send_cmd_err;

		ret = jsa1212_set_confmode(jsa1212_data,
					JSA1212_CONFMODE_ALS_EN);
		if (ret < 0)
			goto send_cmd_err;

		set_bit(JSA1212_OPMODE_ALS_EV_EN, &jsa1212_data->flags);

		break;
	case JSA1212_CMD_ALS_EV_DIS:

		ret = jsa1212_set_confmode(jsa1212_data,
					JSA1212_CONFMODE_ALS_DIS);
		if (ret < 0)
			goto send_cmd_err;

		ret = jsa1212_set_intmode(jsa1212_data,
					JSA1212_INTMODE_ALS_DISABLE);
		if (ret < 0)
			goto send_cmd_err;

		clear_bit(JSA1212_OPMODE_ALS_EV_EN, &jsa1212_data->flags);

		break;
	case JSA1212_CMD_PXS_EV_EN:
		ret = jsa1212_set_intmode(jsa1212_data,
					JSA1212_INTMODE_PXS_ENABLE);
		if (ret < 0)
			goto send_cmd_err;

		ret = jsa1212_set_confmode(jsa1212_data,
					JSA1212_CONFMODE_PXS_EN);
		if (ret < 0)
			goto send_cmd_err;

		set_bit(JSA1212_OPMODE_PXS_EV_EN, &jsa1212_data->flags);

		break;
	case JSA1212_CMD_PXS_EV_DIS:

		ret = jsa1212_set_confmode(jsa1212_data,
					JSA1212_CONFMODE_PXS_DIS);
		if (ret < 0)
			goto send_cmd_err;

		ret = jsa1212_set_intmode(jsa1212_data,
					JSA1212_INTMODE_PXS_DISABLE);
		if (ret < 0)
			goto send_cmd_err;

		clear_bit(JSA1212_OPMODE_PXS_EV_EN, &jsa1212_data->flags);

		break;
	case JSA1212_CMD_RESUME:
		if (test_bit(JSA1212_OPMODE_ALS_EN,
				&jsa1212_data->state_flags)) {

			ret = jsa1212_set_confmode(jsa1212_data,
						JSA1212_CONFMODE_ALS_EN);
			if (ret < 0)
				goto send_cmd_err;
		}

		if (test_bit(JSA1212_OPMODE_PXS_EN,
				&jsa1212_data->state_flags)) {

			ret = jsa1212_set_confmode(jsa1212_data,
						JSA1212_CONFMODE_PXS_EN);

			if (ret < 0)
				goto send_cmd_err;
		}

		jsa1212_data->state_flags = 0;

		break;

	default:
		break;
	}

	return ret;

send_cmd_err:
	dev_err(&jsa1212_data->client->dev,
			"jsa1212 send cmd %d failed\n", cmd_id);
	return ret;
}

static int jsa1212_read_data(struct jsa1212_data *data,
				struct iio_chan_spec const *chan,
				unsigned int *val)
{
	int ret;
	u8 reg;
	unsigned char buf[2];

	switch (chan->type) {
	case IIO_LIGHT:
		reg = JSA1212_ALS_DT1_REG;

		/*Read first 8 bits of 12 bit data*/
		ret = jsa1212_reg_read(data, reg);

		if (ret < 0)
			goto data_read_error;

		buf[0] = ret & JSA1212_ALS_DT1_MASK;

		reg = JSA1212_ALS_DT2_REG;

		/*Read next 4 bits of 12 bit data*/
		ret = jsa1212_reg_read(data, reg);

		if (ret < 0)
			goto data_read_error;

		buf[1] = ret & JSA1212_ALS_DT2_MASK;

		*val = buf[0] | (buf[1] << 8);

		if (!test_bit(JSA1212_OPMODE_ALS_EV_EN, &data->flags)) {
			ret = jsa1212_set_confmode(data,
					JSA1212_CONFMODE_ALS_DIS);
			if (ret < 0)
				goto data_read_error;
		}
		break;
	case IIO_PROXIMITY:
		reg = JSA1212_PXS_DATA_REG;

		/*Read out all data*/
		ret = jsa1212_reg_read(data, reg);

		if (ret < 0)
			goto data_read_error;

		*val = ret & JSA1212_PXS_DATA_MASK;

		if (!test_bit(JSA1212_OPMODE_PXS_EV_EN, &data->flags)) {
			ret = jsa1212_set_confmode(data,
					JSA1212_CONFMODE_PXS_DIS);
			if (ret < 0)
				goto data_read_error;
		}

		break;
	default:
		return -EINVAL;
	}

	return ret;

data_read_error:
	dev_err(&data->client->dev,
		"jsa1212 data read reg (0X%02X)\n", reg);

	return ret;
}


static int jsa1212_read_channel(struct jsa1212_data *jsa1212_data,
				struct iio_chan_spec const *chan, int *val)
{
	int ret, delay = 0;
	enum jsa1212_cmd_id cmd_id;

	switch (chan->type) {
	case IIO_LIGHT:
		cmd_id = JSA1212_CMD_RAW_ALS;
		delay = JSA1212_ALS_DELAY_MS;
		break;
	case IIO_PROXIMITY:
		cmd_id = JSA1212_CMD_RAW_PXS;
		delay = JSA1212_PXS_DELAY_MS;
		break;
	default:
		ret = -EINVAL;
		goto read_chan_err;
	}

	ret = jsa1212_send_cmd(jsa1212_data, cmd_id);
	if (ret < 0)
		goto read_chan_err;

	/* Delay for data output */
	msleep(delay);

	ret = jsa1212_read_data(jsa1212_data, chan, val);
	if (ret < 0)
		goto read_chan_err;

	return ret;

read_chan_err:
	dev_err(&jsa1212_data->client->dev,
		"jsa1212 read channel scan id %d failed\n", chan->scan_index);
	return ret;
}


static int jsa1212_read_raw(struct iio_dev *indio_dev,
				struct iio_chan_spec const *chan,
				int *val, int *val2, long mask)
{
	int ret;
	struct jsa1212_data *data = iio_priv(indio_dev);

	mutex_lock(&data->lock);

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		ret = jsa1212_read_channel(data, chan, val);
		ret = ret < 0 ? ret : IIO_VAL_INT;
		break;
	case IIO_CHAN_INFO_SCALE:
		switch (chan->type) {
		case IIO_LIGHT:
			*val = als_range_val[data->als_rng_idx];
			*val2 = BIT(12); /* Max 12 bit value */
			ret = IIO_VAL_FRACTIONAL;
			break;
		case IIO_PROXIMITY:
		default:
			ret = -EINVAL;
			break;
		}
	default:
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&data->lock);

	return ret;
}

static int jsa1212_read_thresh(struct iio_dev *indio_dev,
		const struct iio_chan_spec *chan, enum iio_event_type type,
		enum iio_event_direction dir, enum iio_event_info info,
		int *val, int *val2)
{
	struct jsa1212_data *data = iio_priv(indio_dev);
	int ret = IIO_VAL_INT;

	mutex_lock(&data->lock);

	switch (chan->type) {
	case IIO_LIGHT:
		switch (dir) {
		case IIO_EV_DIR_RISING:
			*val = data->als_thresh[JSA1212_THRESH_HIGH];
			break;
		case IIO_EV_DIR_FALLING:
			*val = data->als_thresh[JSA1212_THRESH_LOW];
			break;
		default:
			ret = -EINVAL;
			goto read_thresh_err;
		}
		break;
	case IIO_PROXIMITY:
		switch (dir) {
		case IIO_EV_DIR_RISING:
			*val = data->pxs_thresh[JSA1212_THRESH_HIGH];
			break;
		case IIO_EV_DIR_FALLING:
			*val = data->pxs_thresh[JSA1212_THRESH_LOW];
			break;
		default:
			ret = -EINVAL;
			goto read_thresh_err;
		}
		break;
	default:
		ret = -EINVAL;
	}

read_thresh_err:
	mutex_unlock(&data->lock);

	return ret;
}

static int jsa1212_write_thresh(struct iio_dev *indio_dev,
		const struct iio_chan_spec *chan, enum iio_event_type type,
		enum iio_event_direction dir, enum iio_event_info info, int val,
		int val2)
{
	struct jsa1212_data *data = iio_priv(indio_dev);
	int ret = IIO_VAL_INT;

	mutex_lock(&data->lock);
	switch (chan->type) {
	case IIO_LIGHT:
		switch (dir) {
		case IIO_EV_DIR_RISING:
			data->als_thresh[JSA1212_THRESH_HIGH] = val;
			break;
		case IIO_EV_DIR_FALLING:
			data->als_thresh[JSA1212_THRESH_LOW] = val;
			break;
		default:
			ret = -EINVAL;
			goto write_thresh_err;
		}
		break;
	case IIO_PROXIMITY:
		switch (dir) {
		case IIO_EV_DIR_RISING:
			data->pxs_thresh[JSA1212_THRESH_HIGH] = val;
			break;
		case IIO_EV_DIR_FALLING:
			data->pxs_thresh[JSA1212_THRESH_LOW] = val;
			break;
		default:
			ret = -EINVAL;
			goto write_thresh_err;
		}
		break;
	default:
		ret = -EINVAL;
	}

write_thresh_err:
	mutex_unlock(&data->lock);
	return ret;
}

static int jsa1212_read_event_config(struct iio_dev *indio_dev,
		const struct iio_chan_spec *chan,
		enum iio_event_type type,
		enum iio_event_direction dir)
{
	struct jsa1212_data *data = iio_priv(indio_dev);
	int event_en;

	mutex_lock(&data->lock);

	switch (chan->type) {
	case IIO_LIGHT:
		event_en = test_bit(JSA1212_OPMODE_ALS_EV_EN, &data->flags);
		break;
	case IIO_PROXIMITY:
		event_en = test_bit(JSA1212_OPMODE_PXS_EV_EN, &data->flags);
		break;
	default:
		event_en = -EINVAL;
		break;
	}

	mutex_unlock(&data->lock);

	return event_en;
}

static int jsa1212_write_event_config(struct iio_dev *indio_dev,
		const struct iio_chan_spec *chan, enum iio_event_type type,
		enum iio_event_direction dir, int state)
{
	struct jsa1212_data *data = iio_priv(indio_dev);
	int ret;
	enum jsa1212_cmd_id cmd_id;

	mutex_lock(&data->lock);

	switch (chan->type) {
	case IIO_LIGHT:
		cmd_id = state ? JSA1212_CMD_ALS_EV_EN :
				JSA1212_CMD_ALS_EV_DIS;
		break;
	case IIO_PROXIMITY:
		cmd_id = state ? JSA1212_CMD_PXS_EV_EN :
				JSA1212_CMD_PXS_EV_DIS;
		break;
	default:
		ret = -EINVAL;
		goto event_unlock;
	}

	ret = jsa1212_send_cmd(data, cmd_id);

	if (ret < 0) {
		dev_err(&data->client->dev,
			"write event failed channel (%d) cmd (%d)",
			chan->type, cmd_id);
		goto event_unlock;
	}

event_unlock:
	mutex_unlock(&data->lock);

	return ret;
}

static const struct iio_event_spec jsa1212_als_event_spec[] = {
	{
		.type = IIO_EV_TYPE_THRESH,
		.dir = IIO_EV_DIR_RISING,
		.mask_separate = BIT(IIO_EV_INFO_VALUE),
	},
	{
		.type = IIO_EV_TYPE_THRESH,
		.dir = IIO_EV_DIR_FALLING,
		.mask_separate = BIT(IIO_EV_INFO_VALUE),
	},
	{
		.type = IIO_EV_TYPE_THRESH,
		.dir = IIO_EV_DIR_EITHER,
		.mask_separate = BIT(IIO_EV_INFO_ENABLE),
	},

};

static const struct iio_event_spec jsa1212_pxs_event_spec[] = {
	{
		.type = IIO_EV_TYPE_THRESH,
		.dir = IIO_EV_DIR_RISING,
		.mask_separate = BIT(IIO_EV_INFO_VALUE),
	},
	{
		.type = IIO_EV_TYPE_THRESH,
		.dir = IIO_EV_DIR_FALLING,
		.mask_separate = BIT(IIO_EV_INFO_VALUE),
	},
	{
		.type = IIO_EV_TYPE_THRESH,
		.dir = IIO_EV_DIR_EITHER,
		.mask_separate = BIT(IIO_EV_INFO_ENABLE),
	},
};

static const struct iio_chan_spec jsa1212_channels[] = {
	{
		.type = IIO_LIGHT,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |
			BIT(IIO_CHAN_INFO_SCALE),
		.event_spec = jsa1212_als_event_spec,
		.num_event_specs = ARRAY_SIZE(jsa1212_als_event_spec),
	},
	{
		.type = IIO_PROXIMITY,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
		.event_spec = jsa1212_pxs_event_spec,
		.num_event_specs = ARRAY_SIZE(jsa1212_pxs_event_spec),
	}
};

static const struct iio_info jsa1212_info_no_irq = {
	.driver_module		= THIS_MODULE,
	.read_raw		= &jsa1212_read_raw,
};

static const struct iio_info jsa1212_info = {
	.driver_module		= THIS_MODULE,
	.read_raw		= &jsa1212_read_raw,
	.read_event_value	= &jsa1212_read_thresh,
	.write_event_value	= &jsa1212_write_thresh,
	.read_event_config	= &jsa1212_read_event_config,
	.write_event_config	= &jsa1212_write_event_config,

};

static irqreturn_t jsa1212_interrupt_handler(int irq, void *private)
{
	struct iio_dev *dev_info = private;
	struct jsa1212_data *data = iio_priv(dev_info);
	int ret;
	u8 int_data;

	ret = jsa1212_reg_read(data, JSA1212_INT_REG);
	if (ret < 0) {
		dev_err(&data->client->dev,
			"irq read int reg failed\n");
		goto irq_done;
	}

	int_data = ret & JSA1212_INT_MASK;

	if (int_data & JSA1212_INT_ALS_FLAG_MASK)
		iio_push_event(dev_info,
			       IIO_UNMOD_EVENT_CODE(IIO_LIGHT, 0,
				IIO_EV_TYPE_THRESH,
				IIO_EV_DIR_EITHER),
				iio_get_time_ns());

	if (int_data & JSA1212_INT_PXS_FLAG_MASK)
		iio_push_event(dev_info,
			       IIO_UNMOD_EVENT_CODE(IIO_PROXIMITY, 0,
				IIO_EV_TYPE_THRESH,
				IIO_EV_DIR_EITHER),
				iio_get_time_ns());

	ret = jsa1212_reg_update(data, JSA1212_INT_REG,
				JSA1212_INT_ALS_FLAG_MASK|
				JSA1212_INT_PXS_FLAG_MASK,
				JSA1212_INT_ALS_FLAG_CLR|
				JSA1212_INT_PXS_FLAG_CLR);
	if (ret < 0) {
		dev_err(&data->client->dev,
			"irq write int reg failed\n");
		goto irq_done;
	}


irq_done:
	return IRQ_HANDLED;
}

static int jsa1212_chip_init(struct jsa1212_data *data)
{
	int ret;

	ret = jsa1212_reg_write(data, JSA1212_CONF_REG, JSA1212_CONF_REG_DEF);
	if (ret < 0)
		goto chip_init_err;

	ret = jsa1212_reg_write(data, JSA1212_INT_REG, JSA1212_INT_REG_DEF);
	if (ret < 0)
		goto chip_init_err;

	data->als_thresh[JSA1212_THRESH_LOW] = JSA1212_ALS_TH_MIN;
	data->als_thresh[JSA1212_THRESH_HIGH] = JSA1212_ALS_TH_MAX;
	data->pxs_thresh[JSA1212_THRESH_LOW] = JSA1212_PXS_TH_MIN;
	data->pxs_thresh[JSA1212_THRESH_HIGH] = JSA1212_PXS_TH_MAX;
	data->als_prst_val = JSA1212_INT_ALS_PRST_1CONV;
	data->pxs_prst_val = JSA1212_INT_PXS_PRST_1CONV;
	data->flags = 0x00;
	data->als_rng_idx = JSA1212_ALS_RNG_0_2048;
	data->gpio_irq = -1;

	return 0;

chip_init_err:
	dev_err(&data->client->dev, "Chip init err\n");
	return ret;
}

static int jsa1212_acpi_gpio_probe(struct i2c_client *client,
				struct jsa1212_data *data)
{
	const struct acpi_device_id *id;
	struct device *dev;
	struct gpio_desc *gpio;
	int ret;

	if (!client)
		return -EINVAL;

	dev = &client->dev;

	if (!ACPI_HANDLE(dev))
		return -ENODEV;

	id = acpi_match_device(dev->driver->acpi_match_table, dev);

	if (!id)
		return -ENODEV;

	/* ALS/proximity event gpio interrupt pin */
	gpio = devm_gpiod_get_index(dev, "jsa1212_int", 0);

	if (IS_ERR(gpio)) {
		dev_err(dev, "acpi gpio get index failed\n");
		return PTR_ERR(gpio);
	}

	ret = gpiod_direction_input(gpio);

	if (ret)
		return ret;

	ret = gpiod_to_irq(gpio);

	if (ret < 0)
		return ret;

	data->gpio_irq = ret;

	/* update client irq if invalid */
	if (client->irq < 0)
		client->irq = data->gpio_irq;

	dev_dbg(dev, "gpio probe sucess gpio:%d irq:%d\n",
				desc_to_gpio(gpio), data->gpio_irq);

	return 0;
}

static int jsa1212_probe(struct i2c_client *client,
			     const struct i2c_device_id *id)
{
	struct jsa1212_data *jsa1212_data;
	struct iio_dev *indio_dev;
	int ret;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -ENODEV;

	indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*jsa1212_data));

	if (!indio_dev)
		return -ENOMEM;

	jsa1212_data = iio_priv(indio_dev);

	i2c_set_clientdata(client, indio_dev);

	jsa1212_data->client = client;
	mutex_init(&jsa1212_data->lock);
	indio_dev->dev.parent = &client->dev;
	indio_dev->channels = jsa1212_channels;
	indio_dev->num_channels = ARRAY_SIZE(jsa1212_channels);
	indio_dev->name = id->name;
	indio_dev->modes = INDIO_DIRECT_MODE;

	ret = jsa1212_chip_init(jsa1212_data);

	if (ret < 0)
		return ret;

	ret = jsa1212_acpi_gpio_probe(client, jsa1212_data);

	if (ret)
		dev_info(&client->dev, "acpi gpio probe failed (%d)\n", ret);

	if (client->irq > 0) {
		indio_dev->info = &jsa1212_info;

		ret = devm_request_threaded_irq(&client->dev, client->irq,
				NULL, jsa1212_interrupt_handler,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"jsa1212_thresh_event", indio_dev);
		if (ret) {
			dev_err(&client->dev, "request irq (%d) failed\n",
					client->irq);
			return ret;
		}
	} else
		indio_dev->info = &jsa1212_info_no_irq;

	ret = devm_iio_device_register(&client->dev, indio_dev);
	if (ret) {
		dev_err(&client->dev, "%s: regist device failed\n", __func__);
		return -ENODEV;
	}

	return 0;
}

static int jsa1212_remove(struct i2c_client *client)
{
	struct jsa1212_data *data;
	int ret;

	data = iio_priv(i2c_get_clientdata(client));

	mutex_lock(&data->lock);

	ret = jsa1212_send_cmd(data, JSA1212_CMD_POWEROFF);

	if (ret < 0)
		dev_err(&client->dev, "send shutdown cmd failed\n");

	mutex_unlock(&data->lock);

	return ret;
}

#ifdef CONFIG_PM_SLEEP
static int jsa1212_suspend(struct device *dev)
{
	int ret;
	struct jsa1212_data *data;

	data = iio_priv(i2c_get_clientdata(to_i2c_client(dev)));

	mutex_lock(&data->lock);

	ret = jsa1212_send_cmd(data, JSA1212_CMD_SUSPEND);

	if (ret < 0)
		dev_err(dev, "send shutdown cmd failed\n");

	mutex_unlock(&data->lock);

	return ret;
}

static int jsa1212_resume(struct device *dev)
{
	int ret;
	struct jsa1212_data *data;

	data = iio_priv(i2c_get_clientdata(to_i2c_client(dev)));

	mutex_lock(&data->lock);

	ret = jsa1212_send_cmd(data, JSA1212_CMD_RESUME);

	if (ret < 0)
		dev_err(dev, "send resume cmd failed\n");

	mutex_unlock(&data->lock);

	return ret;
}
#endif

static SIMPLE_DEV_PM_OPS(jsa1212_pm_ops, jsa1212_suspend, jsa1212_resume);

static const struct acpi_device_id jsa1212_acpi_match[] = {
	{"JSA1212", 0},
	{ },
};

static const struct i2c_device_id jsa1212_id[] = {
	{ "jsa1212", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, jsa1212_id);

static struct i2c_driver jsa1212_driver = {
	.driver = {
		.name	= "jsa1212",
		.pm	= &jsa1212_pm_ops,
		.owner	= THIS_MODULE,
		.acpi_match_table = ACPI_PTR(jsa1212_acpi_match),
	},
	.probe		= jsa1212_probe,
	.remove		= jsa1212_remove,
	.id_table	= jsa1212_id,
};
module_i2c_driver(jsa1212_driver);

MODULE_AUTHOR("Sathya Kuppuswamy <sathyanarayanan.kuppuswamy@linux.intel.com>");
MODULE_DESCRIPTION("JSA1212 proximity/ambient light sensor driver");
MODULE_LICENSE("GPL v2");
