/*
 * Support for mt9v113 Camera Sensor.
 *
 * Copyright (c) 2012 Intel Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kmod.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <media/v4l2-device.h>
#include <media/v4l2-chip-ident.h>

#include "mt9v113.h"

#define to_mt9v113_sensor(sd) container_of(sd, struct mt9v113_device, sd)

/*
 * TODO: use debug parameter to actually define when debug messages should
 * be printed.
 */
static int debug;
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "Debug level (0-1)");

/*
 * mt9v113_read_reg - read a register value through i2c
 * @client: i2c_client
 * @data_length: register data width, e.g.: 8bit/16bit/32bit
 * @reg: register address
 * @val: register read value
 *
 * The function return 0 for success, < 0 for err
 */
static int
mt9v113_read_reg(struct i2c_client *client, u16 data_length, u32 reg, u32 *val)
{
	int err;
	struct i2c_msg msg[2];
	unsigned char data[4];

	if (!client->adapter) {
		dev_err(&client->dev, "%s error, no client->adapter\n",
			__func__);
		return -ENODEV;
	}

	if (data_length != MISENSOR_8BIT && data_length != MISENSOR_16BIT
					 && data_length != MISENSOR_32BIT) {
		dev_err(&client->dev, "%s error, invalid data length\n",
			 __func__);
		return -EINVAL;
	}

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = MSG_LEN_OFFSET;
	msg[0].buf = data;

	/* high byte goes out first */
	data[0] = (u16) (reg >> 8);
	data[1] = (u16) (reg & 0xff);

	msg[1].addr = client->addr;
	msg[1].len = data_length;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = data;

	err = i2c_transfer(client->adapter, msg, 2);

	if (err >= 0) {
		*val = 0;
		/* high byte comes first */
		if (data_length == MISENSOR_8BIT)
			*val = data[0];
		else if (data_length == MISENSOR_16BIT)
			*val = data[1] + (data[0] << 8);
		else
			*val = data[3] + (data[2] << 8) +
			    (data[1] << 16) + (data[0] << 24);

		return 0;
	}

	dev_err(&client->dev, "read from offset 0x%x error %d", reg, err);
	return err;
}

/*
 * mt9v113_write_reg - write a register value through i2c
 * @client: i2c_client
 * @data_length: register data width, e.g.: 8bit/16bit/32bit
 * @reg: register address
 * @val: register write value
 *
 * The function return 0 for success, < 0 for err
 */
static int
mt9v113_write_reg(struct i2c_client *client, u16 data_length, u16 reg, u32 val)
{
	int num_msg;
	struct i2c_msg msg;
	unsigned char data[6] = {0};
	u16 *wreg;
	int retry = 0;

	if (!client->adapter) {
		dev_err(&client->dev, "%s error, no client->adapter\n",
			__func__);
		return -ENODEV;
	}

	if (data_length != MISENSOR_8BIT && data_length != MISENSOR_16BIT
					 && data_length != MISENSOR_32BIT) {
		dev_err(&client->dev, "%s error, invalid data_length\n",
			__func__);
		return -EINVAL;
	}

	memset(&msg, 0, sizeof(msg));

again:
	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 2 + data_length;
	msg.buf = data;

	/* high byte goes out first */
	wreg = (u16 *)data;
	*wreg = cpu_to_be16(reg);

	if (data_length == MISENSOR_8BIT) {
		data[2] = (u8)(val);
	} else if (data_length == MISENSOR_16BIT) {
		u16 *wdata = (u16 *)&data[2];
		*wdata = be16_to_cpu((u16)val);
	} else {
		/* MISENSOR_32BIT */
		u32 *wdata = (u32 *)&data[2];
		*wdata = be32_to_cpu(val);
	}

	num_msg = i2c_transfer(client->adapter, &msg, 1);

	if (num_msg >= 0)
		return 0;

	dev_err(&client->dev, "write error: wrote 0x%x to offset 0x%x error %d",
		val, reg, num_msg);
	if (retry <= I2C_RETRY_COUNT) {
		dev_dbg(&client->dev, "retrying... %d", retry);
		retry++;
		msleep(20);
		goto again;
	}

	return num_msg;
}

/**
 * misensor_rmw_reg - Read/Modify/Write a value to a register in the sensor
 * device
 * @client: i2c driver client structure
 * @data_length: 8/16/32-bits length
 * @reg: register address
 * @mask: masked out bits
 * @set: bits set
 *
 * Read/modify/write a value to a register in the  sensor device.
 * Returns zero if successful, or non-zero otherwise.
 */
static int
misensor_rmw_reg(struct i2c_client *client, u16 data_length, u16 reg,
		     u32 mask, u32 set)
{
	int err;
	u32 val;

	/* Exit when no mask */
	if (mask == 0)
		return 0;

	/* @mask must not exceed data length */
	switch (data_length) {
	case MISENSOR_8BIT:
		if (mask & ~0xff)
			return -EINVAL;
		break;
	case MISENSOR_16BIT:
		if (mask & ~0xffff)
			return -EINVAL;
		break;
	case MISENSOR_32BIT:
		break;
	default:
		/* Wrong @data_length */
		return -EINVAL;
	}

	err = mt9v113_read_reg(client, data_length, reg, &val);
	if (err) {
		dev_err(&client->dev, "misensor_rmw_reg error exit");
		return -EINVAL;
	}

	val &= ~mask;

	/*
	 * Perform the OR function if the @set exists.
	 * Shift @set value to target bit location. @set should set only
	 * bits included in @mask.
	 *
	 * REVISIT: This function expects @set to be non-shifted. Its shift
	 * value is then defined to be equal to mask's LSB position.
	 * How about to inform values in their right offset position and avoid
	 * this unneeded shift operation?
	 */
	set <<= ffs(mask) - 1;
	val |= set & mask;

	err = mt9v113_write_reg(client, data_length, reg, val);
	if (err) {
		dev_err(&client->dev, "misensor_rmw_reg error exit");
		return -EINVAL;
	}

	return 0;
}

static int __mt9v113_flush_reg_array(struct i2c_client *client,
				     struct mt9v113_write_ctrl *ctrl)
{
	struct i2c_msg msg;
	const int num_msg = 1;
	int ret;
	int retry = 0;

	if (ctrl->index == 0)
		return 0;

again:
	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 2 + ctrl->index;
	ctrl->buffer.addr = cpu_to_be16(ctrl->buffer.addr);
	msg.buf = (u8 *)&ctrl->buffer;

	ret = i2c_transfer(client->adapter, &msg, num_msg);
	if (ret != num_msg) {
		if (++retry <= I2C_RETRY_COUNT) {
			dev_dbg(&client->dev, "retrying... %d\n", retry);
			msleep(20);
			goto again;
		}
		dev_err(&client->dev, "%s: i2c transfer error\n", __func__);
		return -EIO;
	}

	ctrl->index = 0;

	/*
	 * REVISIT: Previously we had a delay after writing data to sensor.
	 * But it was removed as our tests have shown it is not necessary
	 * anymore.
	 */

	return 0;
}

static int __mt9v113_buf_reg_array(struct i2c_client *client,
				   struct mt9v113_write_ctrl *ctrl,
				   const struct misensor_reg *next)
{
	u16 *data16;
	u32 *data32;
	int err;

	/* Insufficient buffer? Let's flush and get more free space. */
	if (ctrl->index + next->length >= MT9V113_MAX_WRITE_BUF_SIZE) {
		err = __mt9v113_flush_reg_array(client, ctrl);
		if (err)
			return err;
	}

	switch (next->length) {
	case MISENSOR_8BIT:
		ctrl->buffer.data[ctrl->index] = (u8)next->val;
		break;
	case MISENSOR_16BIT:
		data16 = (u16 *)&ctrl->buffer.data[ctrl->index];
		*data16 = cpu_to_be16((u16)next->val);
		break;
	case MISENSOR_32BIT:
		data32 = (u32 *)&ctrl->buffer.data[ctrl->index];
		*data32 = cpu_to_be32(next->val);
		break;
	default:
		return -EINVAL;
	}

	/* When first item is added, we need to store its starting address */
	if (ctrl->index == 0)
		ctrl->buffer.addr = next->reg;

	ctrl->index += next->length;

	return 0;
}

static int
__mt9v113_write_reg_is_consecutive(struct i2c_client *client,
				   struct mt9v113_write_ctrl *ctrl,
				   const struct misensor_reg *next)
{
	if (ctrl->index == 0)
		return 1;

	return ctrl->buffer.addr + ctrl->index == next->reg;
}

/*
 * mt9v113_write_reg_array - Initializes a list of mt9v113 registers
 * @client: i2c driver client structure
 * @reglist: list of registers to be written
 * This function initializes a list of registers. When consecutive addresses
 * are found in a row on the list, this function creates a buffer and sends
 * consecutive data in a single i2c_transfer().
 *
 * __mt9v113_flush_reg_array, __mt9v113_buf_reg_array() and
 * __mt9v113_write_reg_is_consecutive() are internal functions to
 * mt9v113_write_reg_array() and should be not used anywhere else.
 *
 */
static int mt9v113_write_reg_array(struct i2c_client *client,
				const struct misensor_reg *reglist)
{
	const struct misensor_reg *next = reglist;
	struct mt9v113_write_ctrl ctrl;
	int err;

	ctrl.index = 0;
	for (; next->length != MISENSOR_TOK_TERM; next++) {
		switch (next->length & MISENSOR_TOK_MASK) {
		case MISENSOR_TOK_DELAY:
			err = __mt9v113_flush_reg_array(client, &ctrl);
			if (err)
				return err;
			msleep(next->val);
			break;
		case MISENSOR_TOK_RMW:
			err = __mt9v113_flush_reg_array(client, &ctrl);
			err |= misensor_rmw_reg(client,
						next->length &
							~MISENSOR_TOK_RMW,
						next->reg, next->val,
						next->val2);
			if (err) {
				dev_err(&client->dev, "%s read err. aborted\n",
					__func__);
				return -EINVAL;
			}
			break;
		default:
			/*
			 * If next address is not consecutive, data needs to be
			 * flushed before proceed.
			 */
			if (!__mt9v113_write_reg_is_consecutive(client, &ctrl,
								next)) {
				err = __mt9v113_flush_reg_array(client, &ctrl);
				if (err)
					return err;
			}
			err = __mt9v113_buf_reg_array(client, &ctrl, next);
			if (err) {
				dev_err(&client->dev, "%s: write aborted",
					__func__);
				return err;
			}
			break;
		}
	}

	err = __mt9v113_flush_reg_array(client, &ctrl);
	if (err)
		return err;
	return 0;
}

static int mt9v113_wait_standby(struct v4l2_subdev *sd, int flag)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int i = 100;
	int ret, status;

	while (i--) {
		ret = mt9v113_read_reg(client, MISENSOR_16BIT,
				       MT9V113_REG_STBY_CTRL, &status);
		if (ret) {
			dev_err(&client->dev, "err read SEQ_CMD: %d", ret);
			return -ret;
		}

		if (((status & STBY_CTRL_MASK_STBY_STAT)
			>> STBY_CTRL_BIT_STBY_STAT) == flag)
			return 0;
		msleep(20);
	}

	dev_err(&client->dev, "wait standby %d timeout.", flag);
	return -EBUSY;
}

static int mt9v113_set_suspend(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	ret = misensor_rmw_reg(client, MISENSOR_16BIT, MT9V113_REG_STBY_CTRL,
			       STBY_CTRL_MASK_STBY_REQ, 0x01);
	if (ret) {
		dev_err(&client->dev, "err set standby bit: %d", ret);
		return -EINVAL;
	}

	return mt9v113_wait_standby(sd, 1);
}

static int mt9v113_wait_refresh(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int i = 100;
	int ret, status;

	while (i--) {
		ret = mt9v113_write_reg(client, MISENSOR_16BIT,
					MT9V113_MCU_VAR_ADDR,
					MT9V113_VAR_SEQ_CMD);
		if (ret) {
			dev_err(&client->dev, "err Write VAR ADDR: %d", ret);
			return ret;
		}

		ret = mt9v113_read_reg(client, MISENSOR_16BIT,
					MT9V113_MCU_VAR_DATA0, &status);
		if (ret) {
			dev_err(&client->dev, "err read SEQ_CMD: %d", ret);
			return ret;
		}

		if (status == SEQ_CMD_RUN)
			return 0;
		msleep(20);
	}

	dev_err(&client->dev, "wait refresh timeout: 0x%x", status);
	return -EBUSY;
}

static int mt9v113_refresh(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	/* Refresh sequencer Mode */
	ret = mt9v113_write_reg(client, MISENSOR_16BIT, MT9V113_MCU_VAR_ADDR,
				MT9V113_VAR_SEQ_CMD);
	if (ret) {
		dev_err(&client->dev, "err Write VAR ADDR: %d", ret);
		return ret;
	}

	ret = mt9v113_write_reg(client, MISENSOR_16BIT, MT9V113_MCU_VAR_DATA0,
				 SEQ_CMD_REFRESH_MODE);
	if (ret) {
		dev_err(&client->dev, "err refresh seq mode: %d", ret);
		return ret;
	}

	ret = mt9v113_wait_refresh(sd);
	if (ret)
		return ret;
	/* Refresh sequencer */
	ret = mt9v113_write_reg(client, MISENSOR_16BIT, MT9V113_MCU_VAR_ADDR,
				MT9V113_VAR_SEQ_CMD);
	if (ret) {
		dev_err(&client->dev, "err Write VAR ADDR: %d", ret);
		return ret;
	}

	ret = mt9v113_write_reg(client, MISENSOR_16BIT, MT9V113_MCU_VAR_DATA0,
				 SEQ_CMD_REFRESH);
	if (ret) {
		dev_err(&client->dev, "err refresh seq: %d", ret);
		return ret;
	}
	ret = mt9v113_wait_refresh(sd);
	if (ret)
		return ret;
	return 0;
}

static int mt9v113_wait_pll_lock(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int i = 10;
	int ret, status_pll;

	while (i--) {
		ret = mt9v113_read_reg(client, MISENSOR_16BIT,
			MT9V113_REG_PLL_CTRL, &status_pll);
		if (ret) {
			dev_err(&client->dev, "err read pll status: %d", ret);
			return -EINVAL;
		}
		if (status_pll & PLL_CTRL_MASK_PLL_STAT)
			return 0;
		usleep_range(8000, 12000);
	}

	dev_err(&client->dev, "pll can't lock, err: %d", ret);
	return -EBUSY;
}

static int mt9v113_init_pll(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	/* Bypass PLL */
	ret = misensor_rmw_reg(client, MISENSOR_16BIT, MT9V113_REG_PLL_CTRL,
			       PLL_CTRL_MASK_INIT_PLL, 0x1);
	if (ret)
		goto err;
	/* Disable PLL */
	ret = misensor_rmw_reg(client, MISENSOR_16BIT, MT9V113_REG_PLL_CTRL,
			       PLL_CTRL_MASK_EN_PLL, 0x0);
	if (ret)
		goto err;

	/*
	 * PLL Setting:
	 * Input: 19.2M
	 * M=35, N=2 P=0
	 * Target: 28M
	 * fbit: 224M
	 * fword: 28M
	 * sensor: 14M
	 */
	/* PLL Dividers: M=35, N=2 */
	ret = mt9v113_write_reg(client, MISENSOR_16BIT, MT9V113_REG_PLL_DIV,
				0x0223);
	if (ret)
		goto err;

	/* PLL P Divider = 0 */
	ret = mt9v113_write_reg(client, MISENSOR_16BIT, MT9V113_REG_PLL_P, 0x0);
	if (ret)
		goto err;

	/* PLL control: TEST_BYPASS on = 9291 */
	/* Arbitrary value from Aptina */
	ret = mt9v113_write_reg(client, MISENSOR_16BIT, MT9V113_REG_PLL_CTRL,
				0x244b);
	if (ret)
		goto err;

	/*
	 * wait PLL lock
	 * Aptina: min require 1ms delay
	 */
	usleep_range(1000, 2000);

	/* PLL control: PLL Enable on = 12363 */
	/* Abitrary value from Aptina */
	ret = mt9v113_write_reg(client, MISENSOR_16BIT, MT9V113_REG_PLL_CTRL,
				0x304b);
	if (ret)
		goto err;

	/* Poll PLL Lock state */
	ret = mt9v113_wait_pll_lock(sd);
	if (ret)
		return ret;

	/* PLL Bypass off */
	ret = misensor_rmw_reg(client, MISENSOR_16BIT, MT9V113_REG_PLL_CTRL,
			       PLL_CTRL_MASK_INIT_PLL, 0x0);
	if (ret)
		goto err;

	return 0;

err:
	dev_err(&client->dev, "reg pll access err: %d", ret);
	return ret;
}

static int mt9v113_wait_mipi_standby(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int i = 100;
	int ret, status;

	while (i--) {
		ret = mt9v113_read_reg(client, MISENSOR_16BIT,
				       MT9V113_REG_MIPI_STAT, &status);
		if (ret) {
			dev_err(&client->dev, "err read SEQ_CMD: %d", ret);
			return ret;
		}

		if (status & MIPI_STAT_MASK_MIPI_STBY_STAT)
			return 0;
		usleep_range(8000, 12000);
	}

	dev_err(&client->dev, "wait mipi standby %d timeout.", status);
	return -EBUSY;
}

static int mt9v113_mipi_standby(struct v4l2_subdev *sd, int state)
{
	int ret = 0;
	struct i2c_client *c = v4l2_get_subdevdata(sd);

	ret = misensor_rmw_reg(c, MISENSOR_16BIT, MT9V113_REG_MIPI_CTRL,
			       MIPI_CTRL_MASK_MIPI_STBY_REQ, state);
	if (ret)
		dev_err(&c->dev, "err set mipi standby bit: %d", ret);

	return ret;
}

static int mt9v113_init_common(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	/*
	 * Soft reset and basic sys ctl
	 */
	ret = mt9v113_write_reg_array(client, mt9v113_reset);
	if (ret)
		return ret;

	/*
	 * PLL Init
	 */
	ret = mt9v113_init_pll(sd);
	if (ret) {
		dev_err(&client->dev, "err init pll: %d", ret);
		return ret;
	}
	/*
	 * Enable MIPI interface
	 */
	ret = mt9v113_write_reg(client, MISENSOR_16BIT, MT9V113_REG_MISC_CTRL,
				0x0018);
	if (ret) {
		dev_err(&client->dev, "err MIPI interface");
		return ret;
	}
	/*
	 * Take out of standby with MCU power up stop
	 * Arbitrary value from Aptina
	 */
	if (mt9v113_write_reg(client, MISENSOR_16BIT, MT9V113_REG_STBY_CTRL,
			      0x402c))
		dev_warn(&client->dev, "err leave standby");
	if (mt9v113_wait_standby(sd, 0))
		dev_warn(&client->dev, "err leave standby");

	/*
	 * MIPI setting
	 */
	/* Enable MIPI */
	ret = misensor_rmw_reg(client, MISENSOR_16BIT, MT9V113_REG_MIPI_CTRL,
			       MIPI_CTRL_MASK_EN_MIPI, 0x1);
	if (ret) {
		dev_err(&client->dev, "err Enable mipi: %d", ret);
		return ret;
	}

	/* Use IFP */
	ret = misensor_rmw_reg(client, MISENSOR_16BIT, MT9V113_REG_OFIFO_CTRL,
				OFIFO_CTRL_MASK_SENS_OUT, 0x0);
	if (ret) {
		dev_err(&client->dev, "err init mipi: %d", ret);
		return ret;
	}

	/* MIPI Stop EOF */
	ret = misensor_rmw_reg(client, MISENSOR_16BIT, MT9V113_REG_MIPI_CTRL,
			       MIPI_CTRL_MASK_MIPI_EOF_REQ, 0x1);
	if (ret) {
		dev_err(&client->dev, "err set mipi EOF bit: %d", ret);
		return ret;
	}

	/*
	 * REDUCE_IO_CURRENT
	 */
	ret = mt9v113_write_reg_array(client, mt9v113_reduce_current);
	if (ret) {
		dev_err(&client->dev, "err init io current: %d", ret);
		return ret;
	}

	/* core only tags defects, SOC will correct them. */
	/* From Aptina: reserved register */
	ret = misensor_rmw_reg(client, MISENSOR_16BIT, 0x31E0, 0x0002, 0x0);
	if (ret) {
		dev_err(&client->dev, "err reduce io current: %d", ret);
		return ret;
	}

	/*
	 * LSC 95%
	 */
	ret = mt9v113_write_reg_array(client, mt9v113_lsc_95);
	if (ret) {
		dev_err(&client->dev, "err set lsc: %d", ret);
		return ret;
	}

	/*
	 * COLOR PIPE CONTROL
	 */
	ret = mt9v113_write_reg(client, MISENSOR_16BIT,
				MT9V113_REG_COL_PIPE_CTL,
				MT9V113_REG_COL_PIPE_CTL_VAL);
	if (ret) {
		dev_err(&client->dev, "err set color pipe");
		return ret;
	}

	/*
	 * MT9V113 KERNEL CONFIG
	 * For Noise reduction
	 */
	ret = mt9v113_write_reg(client, MISENSOR_16BIT,
				MT9V113_REG_KERNEL_CONFIG,
				MT9V113_REG_KERNEL_CONFIG_VAL);
	if (ret) {
		dev_err(&client->dev, "err set kernel config");
		return ret;
	}

	/*
	 * AWB_CCM
	 */
	ret = mt9v113_write_reg_array(client, mt9v113_awb_ccm);
	if (ret) {
		dev_err(&client->dev, "err set awb & ccm: %d", ret);
		return ret;
	}

	/*
	 * CPIE_Calibration
	 */
	ret = mt9v113_write_reg_array(client, mt9v113_cpipe_calibration);
	if (ret) {
		dev_err(&client->dev, "err set cpipe_calibration: %d", ret);
		return ret;
	}

	/*
	 * CPIPE_Preference
	 */
	ret = mt9v113_write_reg_array(client, mt9v113_cpipe_perference);
	if (ret) {
		dev_err(&client->dev, "err set cpipe_perference: %d", ret);
		return ret;
	}

	return 0;
}

static int power_up(struct v4l2_subdev *sd)
{
	struct mt9v113_device *dev = to_mt9v113_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	if (NULL == dev->platform_data) {
		dev_err(&client->dev, "no camera_sensor_platform_data");
		return -ENODEV;
	}

	/* power control */
	ret = dev->platform_data->power_ctrl(sd, 1);
	if (ret)
		goto fail_power;

	/* flis clock control */
	ret = dev->platform_data->flisclk_ctrl(sd, 1);
	if (ret)
		goto fail_clk;

	/* gpio ctrl */
	ret = dev->platform_data->gpio_ctrl(sd, 1);
	if (ret)
		dev_err(&client->dev, "gpio failed 1\n");

	/*
	 * DS: 6000 EXTCLK is needed after HW reset
	 * EXTCLK: 19.2MHZ -> 6000 / 19.2E6 = 0.31mS
	 */
	usleep_range(310, 1000);

	return ret;

fail_clk:
	dev->platform_data->flisclk_ctrl(sd, 0);
fail_power:
	dev->platform_data->power_ctrl(sd, 0);
	dev_err(&client->dev, "sensor power-up failed\n");

	return ret;
}

static int power_down(struct v4l2_subdev *sd)
{
	struct mt9v113_device *dev = to_mt9v113_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	if (NULL == dev->platform_data) {
		dev_err(&client->dev, "no camera_sensor_platform_data");
		return -ENODEV;
	}

	ret = dev->platform_data->flisclk_ctrl(sd, 0);
	if (ret)
		dev_err(&client->dev, "flisclk failed\n");

	/* gpio ctrl */
	ret = dev->platform_data->gpio_ctrl(sd, 0);
	if (ret)
		dev_err(&client->dev, "gpio failed 1\n");

	/* power control */
	ret = dev->platform_data->power_ctrl(sd, 0);
	if (ret)
		dev_err(&client->dev, "vprog failed.\n");

	return ret;
}

static int mt9v113_s_power(struct v4l2_subdev *sd, int power)
{
	if (power == 0)
		return power_down(sd);

	if (power_up(sd))
		return -EINVAL;

	return mt9v113_init_common(sd);
}

static int mt9v113_try_res(u32 *w, u32 *h)
{
	int i;

	/*
	 * The mode list is in ascending order. We're done as soon as
	 * we have found the first equal or bigger size.
	 */
	for (i = 0; i < N_RES; i++) {
		if ((mt9v113_res[i].width >= *w) &&
		    (mt9v113_res[i].height >= *h))
			break;
	}

	/*
	 * If no mode was found, it means we can provide only a smaller size.
	 * Returning the biggest one available in this case.
	 */
	if (i == N_RES)
		i--;

	*w = mt9v113_res[i].width;
	*h = mt9v113_res[i].height;

	return 0;
}

static struct mt9v113_res_struct *mt9v113_to_res(u32 w, u32 h)
{
	int  index;

	for (index = 0; index < N_RES; index++) {
		if ((mt9v113_res[index].width == w) &&
		    (mt9v113_res[index].height == h))
			break;
	}

	/* No mode found */
	if (index >= N_RES)
		return NULL;

	return &mt9v113_res[index];
}

static int mt9v113_try_mbus_fmt(struct v4l2_subdev *sd,
				struct v4l2_mbus_framefmt *fmt)
{
	return mt9v113_try_res(&fmt->width, &fmt->height);
}

static int mt9v113_res2size(unsigned int res, int *h_size, int *v_size)
{
	unsigned short hsize;
	unsigned short vsize;

	switch (res) {
	case MT9V113_RES_QCIF:
		hsize = MT9V113_RES_QCIF_SIZE_H;
		vsize = MT9V113_RES_QCIF_SIZE_V;
		break;
	case MT9V113_RES_QVGA:
		hsize = MT9V113_RES_QVGA_SIZE_H;
		vsize = MT9V113_RES_QVGA_SIZE_V;
		break;
	case MT9V113_RES_CIF:
		hsize = MT9V113_RES_CIF_SIZE_H;
		vsize = MT9V113_RES_CIF_SIZE_V;
		break;
	case MT9V113_RES_VGA:
		hsize = MT9V113_RES_VGA_SIZE_H;
		vsize = MT9V113_RES_VGA_SIZE_V;
		break;
	default:
		WARN(1, "%s: Resolution 0x%08x unknown\n", __func__, res);
		return -EINVAL;
	}

	if (h_size != NULL)
		*h_size = hsize;
	if (v_size != NULL)
		*v_size = vsize;
	return 0;
}

static int mt9v113_get_mbus_fmt(struct v4l2_subdev *sd,
				struct v4l2_mbus_framefmt *fmt)
{
	struct mt9v113_device *dev = to_mt9v113_sensor(sd);
	int width, height;
	int ret;

	ret = mt9v113_res2size(dev->res, &width, &height);
	if (ret)
		return ret;
	fmt->width = width;
	fmt->height = height;
	fmt->code = V4L2_MBUS_FMT_UYVY8_1X16;

	return 0;
}

static int mt9v113_set_mbus_fmt(struct v4l2_subdev *sd,
			      struct v4l2_mbus_framefmt *fmt)
{
	struct i2c_client *c = v4l2_get_subdevdata(sd);
	struct mt9v113_device *dev = to_mt9v113_sensor(sd);
	struct mt9v113_res_struct *res_index;
	u32 width = fmt->width;
	u32 height = fmt->height;
	int ret;

	mt9v113_try_res(&width, &height);
	res_index = mt9v113_to_res(width, height);

	/* Sanity check */
	if (unlikely(!res_index)) {
		WARN_ON(1);
		return -EINVAL;
	}

	switch (res_index->res) {
	case MT9V113_RES_QCIF:
		dev_info(&c->dev, "%s: set for qcif\n", __func__);
		ret = mt9v113_write_reg_array(c, mt9v113_qcif_init);
		break;
	case MT9V113_RES_QVGA:
		dev_info(&c->dev, "%s: set for qvga\n", __func__);
		ret = mt9v113_write_reg_array(c, mt9v113_qvga_init);
		break;
	case MT9V113_RES_CIF:
		dev_info(&c->dev, "%s: set for cif\n", __func__);
		ret = mt9v113_write_reg_array(c, mt9v113_cif_init);
		break;
	case MT9V113_RES_VGA:
		dev_info(&c->dev, "%s: set for vga\n", __func__);
		ret = mt9v113_write_reg_array(c, mt9v113_vga_init);
		break;
	default:
		dev_err(&c->dev, "set resolution: %d failed!\n",
			res_index->res);
		return -EINVAL;
	}

	if (ret)
		return ret;

	/* Limit max exposure if in video mode */
	ret = mt9v113_write_reg(c, MISENSOR_16BIT,
				MT9V113_MCU_VAR_ADDR,
				MT9V113_VAR_AE_MAX_INDEX);
	if (ret) {
		dev_err(&c->dev, "err Write VAR ADDR: %d", ret);
		return ret;
	}

	if (dev->run_mode == CI_MODE_VIDEO) {
		ret = mt9v113_write_reg(c, MISENSOR_16BIT,
				MT9V113_MCU_VAR_DATA0,
				MT9V113_AE_MAX_INDEX_0);
		/*
		 * Need to increase Flicker Detection sensitivity
		 * as keeping 30fps will have no enough exposure
		 * time
		 *
		 * DS: How to adjust Auto Flicker Detection Sensitivity:
		 * 1: Decrease the value of 0x000D to 2 or 1
		 * 2: Decrease 0x0010 to 3 or 2
		 * 3: Make the search range wider
		 */
		ret = mt9v113_write_reg_array(c, mt9v113_high_flicker);
		if (ret) {
			dev_err(&c->dev, "err set high flicker: %d", ret);
			return ret;
		}
	} else {
		ret = mt9v113_write_reg(c, MISENSOR_16BIT,
				MT9V113_MCU_VAR_DATA0,
				MT9V113_AE_MAX_INDEX_1);
	}

	if (ret) {
		dev_err(&c->dev, "err write ae_max_index: %d", ret);
		return ret;
	}

	dev->res = res_index->res;

	fmt->width = width;
	fmt->height = height;
	fmt->code = V4L2_MBUS_FMT_UYVY8_1X16;

	return 0;
}

static int mt9v113_g_focal(struct v4l2_subdev *sd, s32 *val)
{
	/* const focal length for MT9V113 */
	*val = (MT9V113_FOCAL_LENGTH_NUM << 16) | MT9V113_FOCAL_LENGTH_DEM;
	return 0;
}

static int mt9v113_g_fnumber(struct v4l2_subdev *sd, s32 *val)
{
	/* const f-number for MT9V113 */
	*val = (MT9V113_F_NUMBER_DEFAULT_NUM << 16) | MT9V113_F_NUMBER_DEM;
	return 0;
}

static int mt9v113_g_fnumber_range(struct v4l2_subdev *sd, s32 *val)
{
	*val = (MT9V113_F_NUMBER_DEFAULT_NUM << 24) |
		(MT9V113_F_NUMBER_DEM << 16) |
		(MT9V113_F_NUMBER_DEFAULT_NUM << 8) | MT9V113_F_NUMBER_DEM;
	return 0;
}

/* read shutter, in number of line period */
static int mt9v113_get_shutter(struct v4l2_subdev *sd, s32 *shutter)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mt9v113_device *dev = to_mt9v113_sensor(sd);
	u32 inte_time, row_time;
	int ret, i;

	/* read integration time */
	ret = mt9v113_write_reg(client, MISENSOR_16BIT,
				MT9V113_MCU_VAR_ADDR,
				MT9V113_VAR_INTEGRATION_TIME);
	if (ret) {
		dev_err(&client->dev, "err Write VAR ADDR: %d", ret);
		return ret;
	}

	ret = mt9v113_read_reg(client, MISENSOR_16BIT,
				MT9V113_MCU_VAR_DATA0,
				&inte_time);
	if (ret) {
		dev_err(&client->dev,
				"err read integration time: %d", ret);
		return ret;
	}

	/* get row time */
	for (i = 0; i < N_RES; i++) {
		if (mt9v113_res[i].res == dev->res) {
			row_time = mt9v113_res[i].row_time;
			break;
		}
	}
	if (i == N_RES)	{
		dev_err(&client->dev,
				"err get row  time: %d", ret);
		return -EINVAL;
	}

	/* return exposure value is in units of 100us */
	*shutter = inte_time * row_time / 100;

	return 0;
}

/*
 * This returns the exposure compensation value, which is expressed in
 * terms of EV. The default EV value is 0, and driver don't support
 * adjust EV value.
 */
static int mt9v113_get_exposure_bias(struct v4l2_subdev *sd, s32 *value)
{
	*value = 0;

	return 0;
}

/*
 * This returns ISO sensitivity.
 */
static int mt9v113_get_iso(struct v4l2_subdev *sd, s32 *value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u32 ae_gain, ae_d_gain;
	int ret;

	/* read ae virtual gain */
	ret = mt9v113_write_reg(client, MISENSOR_16BIT,
				MT9V113_MCU_VAR_ADDR,
				MT9V113_VAR_AE_GAIN);
	if (ret) {
		dev_err(&client->dev, "err Write VAR ADDR: %d", ret);
		return ret;
	}

	ret = mt9v113_read_reg(client, MISENSOR_16BIT,
				MT9V113_MCU_VAR_DATA0,
				&ae_gain);
	if (ret) {
		dev_err(&client->dev,
				"err read ae virtual gain: %d", ret);
		return ret;
	}

	/* read ae_d_gain */
	ret = mt9v113_write_reg(client, MISENSOR_16BIT,
				MT9V113_MCU_VAR_ADDR,
				MT9V113_VAR_AE_D_GAIN);
	if (ret) {
		dev_err(&client->dev, "err Write VAR ADDR: %d", ret);
		return ret;
	}

	ret = mt9v113_read_reg(client, MISENSOR_16BIT,
				MT9V113_MCU_VAR_DATA0,
				&ae_d_gain);
	if (ret) {
		dev_err(&client->dev,
				"err read ae_d_gain: %d", ret);
		return ret;
	}

	*value = ((ae_gain * 25) >> 4) + (((ae_d_gain - 128) * 200) >> 7);

	return 0;
}

/*
 * More will be added in future
 */
static struct mt9v113_control mt9v113_controls[] = {
	{
		.qc = {
			.id = V4L2_CID_EXPOSURE_ABSOLUTE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "exposure",
			.minimum = 0x0,
			.maximum = 0xffff,
			.step = 0x01,
			.default_value = 0x00,
			.flags = 0,
		},
		.query = mt9v113_get_shutter,
	},
	{
		.qc = {
			.id = V4L2_CID_AUTO_EXPOSURE_BIAS,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "exposure bias",
			.minimum = 0x0,
			.maximum = 0xffff,
			.step = 0x01,
			.default_value = 0x00,
			.flags = 0,
		},
		.query = mt9v113_get_exposure_bias,
	},
	{
		.qc = {
			.id = V4L2_CID_ISO_SENSITIVITY,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "iso",
			.minimum = 0x0,
			.maximum = 0xffff,
			.step = 0x01,
			.default_value = 0x00,
			.flags = 0,
		},
		.query = mt9v113_get_iso,
	},
	{
		.qc = {
			.id = V4L2_CID_FOCAL_ABSOLUTE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "focal length",
			.minimum = 0,
			.maximum = MT9V113_FOCAL_LENGTH_DEFAULT,
			.step = 0x01,
			.default_value = MT9V113_FOCAL_LENGTH_DEFAULT,
			.flags = 0,
		},
		.query = mt9v113_g_focal,
	},
	{
		.qc = {
			.id = V4L2_CID_FNUMBER_ABSOLUTE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "f-number",
			.minimum = 0,
			.maximum = MT9V113_F_NUMBER_DEFAULT,
			.step = 0x01,
			.default_value = MT9V113_F_NUMBER_DEFAULT,
			.flags = 0,
		},
		.query = mt9v113_g_fnumber,
	},
	{
		.qc = {
			.id = V4L2_CID_FNUMBER_RANGE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "f-number range",
			.minimum = 0,
			.maximum =  MT9V113_F_NUMBER_RANGE,
			.step = 0x01,
			.default_value = MT9V113_F_NUMBER_RANGE,
			.flags = 0,
		},
		.query = mt9v113_g_fnumber_range,
	},
};

#define N_CONTROLS (ARRAY_SIZE(mt9v113_controls))

static struct mt9v113_control *mt9v113_find_control(__u32 id)
{
	int i;

	for (i = 0; i < N_CONTROLS; i++) {
		if (mt9v113_controls[i].qc.id == id)
			return &mt9v113_controls[i];
	}
	return NULL;
}

static int mt9v113_detect(struct mt9v113_device *dev, struct i2c_client *client)
{
	struct i2c_adapter *adapter = client->adapter;
	u32 retvalue;

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "%s: i2c error", __func__);
		return -ENODEV;
	}
	/*
	 * Read Device ID
	 * SYSCTL: 0x0
	 * contains the MT9V113 device ID Number, 0x2280
	 * Read-Only
	 */
	mt9v113_read_reg(client, MISENSOR_16BIT, MT9V113_REG_CHIPID, &retvalue);
	dev->real_model_id = retvalue;

	if (retvalue != V4L2_IDENT_MT9V113) {
		dev_err(&client->dev, "%s: failed: client->addr = 0x%x\n",
			__func__, client->addr);
		dev_err(&client->dev, "%s: bad device id: 0x%x\n",
			__func__, retvalue);
		return -ENODEV;
	}

	return 0;
}

static int
mt9v113_s_config(struct v4l2_subdev *sd, int irq, void *platform_data)
{
	struct mt9v113_device *dev = to_mt9v113_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	if (NULL == platform_data)
		return -ENODEV;

	dev->platform_data =
	    (struct camera_sensor_platform_data *)platform_data;

	if (dev->platform_data->platform_init) {
		ret = dev->platform_data->platform_init(client);
		if (ret) {
			dev_err(&client->dev, "mt9v113 platform init err\n");
			return ret;
		}
	}
	ret = mt9v113_s_power(sd, 1);
	if (ret) {
		dev_err(&client->dev, "mt9v113 power-up err");
		return ret;
	}

	/*
	 * Soft reset and basic sys ctl
	 */
	ret = mt9v113_write_reg_array(client, mt9v113_reset);
	if (ret)
		return ret;

	/* config & detect sensor */
	ret = mt9v113_detect(dev, client);
	if (ret) {
		dev_err(&client->dev, "mt9v113_detect err s_config.\n");
		goto fail_detect;
	}

	ret = dev->platform_data->csi_cfg(sd, 1);
	if (ret)
		goto fail_csi_cfg;


	ret = mt9v113_s_power(sd, 0);
	if (ret) {
		dev_err(&client->dev, "mt9v113 power down err");
		return ret;
	}

	return 0;

fail_csi_cfg:
	dev->platform_data->csi_cfg(sd, 0);
fail_detect:
	mt9v113_s_power(sd, 0);
	dev_err(&client->dev, "sensor power-gating failed\n");
	return ret;
}

static int mt9v113_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
	struct mt9v113_control *ctrl = mt9v113_find_control(qc->id);

	if (ctrl == NULL)
		return -EINVAL;
	*qc = ctrl->qc;
	return 0;
}

static int mt9v113_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct mt9v113_control *octrl = mt9v113_find_control(ctrl->id);
	int ret;

	if (octrl == NULL)
		return -EINVAL;

	ret = octrl->query(sd, &ctrl->value);
	if (ret < 0)
		return ret;

	return 0;
}

static int mt9v113_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct mt9v113_control *octrl = mt9v113_find_control(ctrl->id);
	int ret;

	if (!octrl || !octrl->tweak)
		return -EINVAL;

	ret = octrl->tweak(sd, ctrl->value);
	if (ret < 0)
		return ret;

	return 0;
}

static int mt9v113_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret = 0;
	struct i2c_client *c = v4l2_get_subdevdata(sd);
	if (enable) {

		/* Make sure MCU will be turn on after LSC */
		/* Arbitrary value from Aptina */
		if (mt9v113_write_reg(c, MISENSOR_16BIT, MT9V113_REG_STBY_CTRL,
				      0x0028))
			dev_err(&c->dev, "err standby control");

		ret = mt9v113_refresh(sd);
		if (ret)
			return ret;

		ret = mt9v113_mipi_standby(sd, 0);
		if (ret)
			return ret;

	} else {
		ret = mt9v113_mipi_standby(sd, 1);
		if (ret)
			return ret;

		ret = mt9v113_wait_mipi_standby(sd);
		if (ret)
			return ret;

	}

	return ret;
}

static int
mt9v113_enum_framesizes(struct v4l2_subdev *sd, struct v4l2_frmsizeenum *fsize)
{
	unsigned int index = fsize->index;

	if (index >= N_RES)
		return -EINVAL;

	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	fsize->discrete.width = mt9v113_res[index].width;
	fsize->discrete.height = mt9v113_res[index].height;

	return 0;
}

static int mt9v113_enum_frameintervals(struct v4l2_subdev *sd,
				       struct v4l2_frmivalenum *fival)
{
	unsigned int index = fival->index;
	int i;

	if (index >= N_RES)
		return -EINVAL;

	/* find out the first equal or bigger size */
	for (i = 0; i < N_RES; i++) {
		if ((mt9v113_res[i].width >= fival->width) &&
		    (mt9v113_res[i].height >= fival->height))
			break;
	}
	if (i == N_RES)
		i--;

	index = i;

	fival->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	fival->discrete.numerator = 1;
	fival->discrete.denominator = mt9v113_res[index].fps;

	return 0;
}

static int
mt9v113_g_chip_ident(struct v4l2_subdev *sd, struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_MT9V113, 0);
}

static int mt9v113_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_fh *fh,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index)
		return -EINVAL;
	code->code = V4L2_MBUS_FMT_UYVY8_1X16;

	return 0;
}

static int mt9v113_enum_frame_size(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh,
	struct v4l2_subdev_frame_size_enum *fse)
{

	unsigned int index = fse->index;


	if (index >= N_RES)
		return -EINVAL;

	fse->min_width = mt9v113_res[index].width;
	fse->min_height = mt9v113_res[index].height;
	fse->max_width = mt9v113_res[index].width;
	fse->max_height = mt9v113_res[index].height;

	return 0;
}

static struct v4l2_mbus_framefmt *
__mt9v113_get_pad_format(struct mt9v113_device *sensor,
			 struct v4l2_subdev_fh *fh, unsigned int pad,
			 enum v4l2_subdev_format_whence which)
{
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->sd);

	if (pad != 0) {
		dev_err(&client->dev,  "%s err. pad %x\n", __func__, pad);
		return NULL;
	}

	switch (which) {
	case V4L2_SUBDEV_FORMAT_TRY:
		return v4l2_subdev_get_try_format(fh, pad);
	case V4L2_SUBDEV_FORMAT_ACTIVE:
		return &sensor->format;
	default:
		return NULL;
	}
}

static int
mt9v113_get_pad_format(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_format *fmt)
{
	struct mt9v113_device *snr = to_mt9v113_sensor(sd);
	struct v4l2_mbus_framefmt *format =
			__mt9v113_get_pad_format(snr, fh, fmt->pad, fmt->which);

	if (format == NULL)
		return -EINVAL;
	fmt->format = *format;

	return 0;
}

static int
mt9v113_set_pad_format(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_format *fmt)
{
	struct mt9v113_device *snr = to_mt9v113_sensor(sd);
	struct v4l2_mbus_framefmt *format =
			__mt9v113_get_pad_format(snr, fh, fmt->pad, fmt->which);

	if (format == NULL)
		return -EINVAL;

	if (fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE)
		snr->format = fmt->format;

	return 0;
}

static int mt9v113_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	struct mt9v113_device *snr = to_mt9v113_sensor(sd);

	if (param->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	snr->run_mode = param->parm.capture.capturemode;

	return 0;
}

static int mt9v113_g_skip_frames(struct v4l2_subdev *sd, u32 *frames)
{
	int index;
	struct mt9v113_device *snr = to_mt9v113_sensor(sd);

	if (frames == NULL)
		return -EINVAL;

	for (index = 0; index < N_RES; index++) {
		if (mt9v113_res[index].res == snr->res)
			break;
	}

	if (index >= N_RES)
		return -EINVAL;

	*frames = mt9v113_res[index].skip_frames;

	return 0;
}
static const struct v4l2_subdev_video_ops mt9v113_video_ops = {
	.try_mbus_fmt = mt9v113_try_mbus_fmt,
	.s_mbus_fmt = mt9v113_set_mbus_fmt,
	.g_mbus_fmt = mt9v113_get_mbus_fmt,
	.s_stream = mt9v113_s_stream,
	.s_parm = mt9v113_s_parm,
	.enum_framesizes = mt9v113_enum_framesizes,
	.enum_frameintervals = mt9v113_enum_frameintervals,
};

static struct v4l2_subdev_sensor_ops mt9v113_sensor_ops = {
	.g_skip_frames	= mt9v113_g_skip_frames,
};

static const struct v4l2_subdev_core_ops mt9v113_core_ops = {
	.g_chip_ident = mt9v113_g_chip_ident,
	.queryctrl = mt9v113_queryctrl,
	.g_ctrl = mt9v113_g_ctrl,
	.s_ctrl = mt9v113_s_ctrl,
	.s_power = mt9v113_s_power,
};

static const struct v4l2_subdev_pad_ops mt9v113_pad_ops = {
	.enum_mbus_code = mt9v113_enum_mbus_code,
	.enum_frame_size = mt9v113_enum_frame_size,
	.get_fmt = mt9v113_get_pad_format,
	.set_fmt = mt9v113_set_pad_format,
};

static const struct v4l2_subdev_ops mt9v113_ops = {
	.core = &mt9v113_core_ops,
	.video = &mt9v113_video_ops,
	.pad = &mt9v113_pad_ops,
	.sensor = &mt9v113_sensor_ops,
};

static const struct media_entity_operations mt9v113_entity_ops;


static int mt9v113_remove(struct i2c_client *client)
{
	struct mt9v113_device *dev;
	struct v4l2_subdev *sd = i2c_get_clientdata(client);

	dev = container_of(sd, struct mt9v113_device, sd);
	dev->platform_data->csi_cfg(sd, 0);
	if (dev->platform_data->platform_deinit)
		dev->platform_data->platform_deinit();
	v4l2_device_unregister_subdev(sd);
	media_entity_cleanup(&dev->sd.entity);
	kfree(dev);
	return 0;
}

static int mt9v113_probe(struct i2c_client *client,
		       const struct i2c_device_id *id)
{
	struct mt9v113_device *dev;
	int ret;

	/* Setup sensor configuration structure */
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		dev_err(&client->dev, "out of memory\n");
		return -ENOMEM;
	}

	v4l2_i2c_subdev_init(&dev->sd, client, &mt9v113_ops);
	if (client->dev.platform_data) {
		ret = mt9v113_s_config(&dev->sd, client->irq,
				       client->dev.platform_data);
		if (ret) {
			v4l2_device_unregister_subdev(&dev->sd);
			kfree(dev);
			return ret;
		}
	}

	/*TODO add format code here*/
	dev->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	dev->pad.flags = MEDIA_PAD_FL_SOURCE;

	/* REVISIT: Do we need media controller? */
	ret = media_entity_init(&dev->sd.entity, 1, &dev->pad, 0);
	if (ret) {
		mt9v113_remove(client);
		return ret;
	}

	/* set res index to be invalid */
	dev->res = -1;
	dev->run_mode = CI_MODE_PREVIEW;

	return 0;
}


MODULE_DEVICE_TABLE(i2c, mt9v113_id);

static struct i2c_driver mt9v113_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "mt9v113"
	},
	.probe = mt9v113_probe,
	.remove = __exit_p(mt9v113_remove),
	.id_table = mt9v113_id,
};

static __init int init_mt9v113(void)
{
	return i2c_add_driver(&mt9v113_driver);
}

static __exit void exit_mt9v113(void)
{
	i2c_del_driver(&mt9v113_driver);
}

module_init(init_mt9v113);
module_exit(exit_mt9v113);

MODULE_AUTHOR("Tao Jing <jing.tao@intel.com>");
MODULE_LICENSE("GPL");
