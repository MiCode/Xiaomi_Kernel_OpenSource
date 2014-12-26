/*
 * Support for Aptina mt9e013 1080p HD camera sensor.
 *
 * Copyright (c) 2011 Intel Corporation. All Rights Reserved.
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
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/moduleparam.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/bitops.h>
#include <media/v4l2-device.h>
#include <media/v4l2-chip-ident.h>
#include <asm/intel_scu_ipc.h>
#include <asm/intel_mid_rpmsg.h>

#if defined(CONFIG_VIDEO_MT9E013_LEXINGTON)
#include "mt9e013_lexington.h"
#elif defined(CONFIG_VIDEO_MT9E013_ENZO)
#include "mt9e013_enzo.h"
#else /* CONFIG_VIDEO_MT9E013_BLACKBAY */
#include "mt9e013_blackbay.h"
#endif

#define to_mt9e013_sensor(sd) container_of(sd, struct mt9e013_device, sd)

#define HOME_POS 255
#define PR3_2_FW 0x0400
#define PR3_3_FW 0x0500

/* divides a by b using half up rounding and div/0 prevention
 * (result is 0 if b == 0) */
#define divsave_rounded(a, b)	(((b) != 0) ? (((a)+((b)>>1))/(b)) : (-1))

static int
mt9e013_read_reg(struct i2c_client *client, u16 len, u16 reg, u16 *val)
{
	u16 data[MT9E013_SHORT_MAX];
	int err, i;

	struct i2c_msg msg[2] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = I2C_MSG_LENGTH,
			.buf = (unsigned char *)&reg,
		}, {
			.addr = client->addr,
			.len = len,
			.flags = I2C_M_RD,
			.buf = (u8 *)data,
		}
	};

	reg = cpu_to_be16(reg);

	if (!client->adapter) {
		v4l2_err(client, "%s error, no client->adapter\n", __func__);
		return -ENODEV;
	}

	/* @len should be even when > 1 */
	if (len > MT9E013_BYTE_MAX) {
		v4l2_err(client, "%s error, invalid data length\n", __func__);
		return -EINVAL;
	}

	err = i2c_transfer(client->adapter, msg, 2);
	if (err != 2) {
		if (err >= 0)
			err = -EIO;
		goto error;
	}

	/* high byte comes first */
	if (len == MT9E013_8BIT) {
		*val = (u8)data[0];
	} else {
		/* 16-bit access is default when len > 1 */
		for (i = 0; i < (len >> 1); i++)
			val[i] = be16_to_cpu(data[i]);
	}

	return 0;

error:
	dev_err(&client->dev, "read from offset 0x%x error %d", reg, err);
	return err;
}

static int mt9e013_i2c_write(struct i2c_client *client, u16 len, u8 *data)
{
	struct i2c_msg msg;
	const int num_msg = 1;
	int ret;
	int retry = 0;

again:
	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = len;
	msg.buf = data;

	ret = i2c_transfer(client->adapter, &msg, 1);

	/*
	 * It is said that Rev 2 sensor needs some delay here otherwise
	 * registers do not seem to load correctly. But tests show that
	 * removing the delay would not cause any in-stablility issue and the
	 * delay will cause serious performance down, so, removed previous
	 * mdelay(1) here.
	 */

	if (ret == num_msg)
		return 0;

	if (retry <= I2C_RETRY_COUNT) {
		dev_dbg(&client->dev, "retrying i2c write transfer... %d",
			retry);
		retry++;
		msleep(20);
		goto again;
	}

	return ret;
}

static int
mt9e013_write_reg(struct i2c_client *client, u16 data_length, u16 reg, u16 val)
{
	int ret;
	unsigned char data[4] = {0};
	const u16 len = data_length + sizeof(u16); /* 16-bit address + data */

	if (!client->adapter) {
		v4l2_err(client, "%s error, no client->adapter\n", __func__);
		return -ENODEV;
	}

	if (data_length != MT9E013_8BIT && data_length != MT9E013_16BIT) {
		v4l2_err(client, "%s error, invalid data_length\n", __func__);
		return -EINVAL;
	}
	/* high byte goes out first */
	*(u16 *)data = cpu_to_be16(reg);
	switch (data_length) {
	case MT9E013_8BIT:
		data[2] = (u8)val;
		break;
	case MT9E013_16BIT:
		*(u16 *)&data[2] = cpu_to_be16(val);
		break;
	default:
		dev_err(&client->dev,
			"write error: invalid length type %d\n", data_length);
	}

	ret = mt9e013_i2c_write(client, len, data);
	if (ret)
		dev_err(&client->dev,
			"write error: wrote 0x%x to offset 0x%x error %d",
			val, reg, ret);

	return ret;
}


/**
 * mt9e013_rmw_reg - Read/Modify/Write a value to a register in the sensor
 * device
 * @client: i2c driver client structure
 * @data_length: 8/16-bits length
 * @reg: register address
 * @mask: masked out bits
 * @set: bits set
 *
 * Read/modify/write a value to a register in the  sensor device.
 * Returns zero if successful, or non-zero otherwise.
 */
static int mt9e013_rmw_reg(struct i2c_client *client, u16 data_length, u16 reg,
			   u16 mask, u16 set)
{
	int err;
	u16 val;

	/* Exit when no mask */
	if (mask == 0)
		return 0;

	/* @mask must not exceed data length */
	if (data_length == MT9E013_8BIT && mask & ~0xff)
		return -EINVAL;

	err = mt9e013_read_reg(client, data_length, reg, &val);
	if (err) {
		v4l2_err(client, "mt9e013_rmw_reg error exit, read failed\n");
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

	err = mt9e013_write_reg(client, data_length, reg, val);
	if (err) {
		v4l2_err(client, "mt9e013_rmw_reg error exit, write failed\n");
		return -EINVAL;
	}

	return 0;
}


/*
 * mt9e013_write_reg_array - Initializes a list of mt9e013 registers
 * @client: i2c driver client structure
 * @reglist: list of registers to be written
 *
 * This function initializes a list of registers. When consecutive addresses
 * are found in a row on the list, this function creates a buffer and sends
 * consecutive data in a single i2c_transfer().
 *
 * __mt9e013_flush_reg_array, __mt9e013_buf_reg_array() and
 * __mt9e013_write_reg_is_consecutive() are internal functions to
 * mt9e013_write_reg_array_fast() and should be not used anywhere else.
 *
 */

static int __mt9e013_flush_reg_array(struct i2c_client *client,
				     struct mt9e013_write_ctrl *ctrl)
{
	u16 size;

	if (ctrl->index == 0)
		return 0;

	size = sizeof(u16) + ctrl->index; /* 16-bit address + data */
	ctrl->buffer.addr = cpu_to_be16(ctrl->buffer.addr);
	ctrl->index = 0;

	return mt9e013_i2c_write(client, size, (u8 *)&ctrl->buffer);
}

static int __mt9e013_buf_reg_array(struct i2c_client *client,
				   struct mt9e013_write_ctrl *ctrl,
				   const struct mt9e013_reg *next)
{
	int size;
	u16 *data16;

	switch (next->type) {
	case MT9E013_8BIT:
		size = 1;
		ctrl->buffer.data[ctrl->index] = (u8)next->val;
		break;
	case MT9E013_16BIT:
		size = 2;
		data16 = (u16 *)&ctrl->buffer.data[ctrl->index];
		*data16 = cpu_to_be16((u16)next->val);
		break;
	default:
		return -EINVAL;
	}

	/* When first item is added, we need to store its starting address */
	if (ctrl->index == 0)
		ctrl->buffer.addr = next->reg.sreg;

	ctrl->index += size;

	/*
	 * Buffer cannot guarantee free space for u32? Better flush it to avoid
	 * possible lack of memory for next item.
	 */
	if (ctrl->index + sizeof(u16) >= MT9E013_MAX_WRITE_BUF_SIZE)
		return __mt9e013_flush_reg_array(client, ctrl);

	return 0;
}

static int
__mt9e013_write_reg_is_consecutive(struct i2c_client *client,
				   struct mt9e013_write_ctrl *ctrl,
				   const struct mt9e013_reg *next)
{
	if (ctrl->index == 0)
		return 1;

	return ctrl->buffer.addr + ctrl->index == next->reg.sreg;
}

static int mt9e013_write_reg_array(struct i2c_client *client,
				   const struct mt9e013_reg *reglist)
{
	const struct mt9e013_reg *next = reglist;
	struct mt9e013_write_ctrl ctrl;
	int err;

	ctrl.index = 0;
	for (; next->type != MT9E013_TOK_TERM; next++) {
		switch (next->type & MT9E013_TOK_MASK) {
		case MT9E013_TOK_DELAY:
			err = __mt9e013_flush_reg_array(client, &ctrl);
			if (err)
				return err;
			msleep(next->val);
			break;

		case MT9E013_RMW:
			err = __mt9e013_flush_reg_array(client, &ctrl);
			err |= mt9e013_rmw_reg(client,
					       next->type & ~MT9E013_RMW,
					       next->reg.sreg, next->val,
					       next->val2);
			if (err) {
				v4l2_err(client,
				"%s: rwm error,aborted\n", __func__);
				return err;
			}
			break;

		default:
			/*
			 * If next address is not consecutive, data needs to be
			 * flushed before proceed.
			 */
			if (!__mt9e013_write_reg_is_consecutive(client, &ctrl,
								next)) {
				err = __mt9e013_flush_reg_array(client, &ctrl);
				if (err)
					return err;
			}
			err = __mt9e013_buf_reg_array(client, &ctrl, next);
			if (err) {
				v4l2_err(client, "%s: write error, aborted\n",
					 __func__);
				return err;
			}
			break;
		}
	}

	return __mt9e013_flush_reg_array(client, &ctrl);
}

static int mt9e013_t_focus_abs(struct v4l2_subdev *sd, s32 value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mt9e013_device *dev = to_mt9e013_sensor(sd);
	int ret;

	value = min(value, MT9E013_MAX_FOCUS_POS);

	ret = mt9e013_write_reg(client, MT9E013_16BIT, MT9E013_VCM_CODE, value);
	if (ret == 0) {
		dev->number_of_steps = value - dev->focus;
		dev->focus = value;
		getnstimeofday(&(dev->timestamp_t_focus_abs));
	}
	return ret;
}

static int mt9e013_t_focus_rel(struct v4l2_subdev *sd, s32 value)
{
	struct mt9e013_device *dev = to_mt9e013_sensor(sd);
	return mt9e013_t_focus_abs(sd, dev->focus + value);
}

#define DELAY_PER_STEP_NS	1000000
#define DELAY_MAX_PER_STEP_NS	(1000000*40)
static int mt9e013_q_focus_status(struct v4l2_subdev *sd, s32 *value)
{
	u32 status = 0;
	struct mt9e013_device *dev = to_mt9e013_sensor(sd);
	struct timespec temptime;
	const struct timespec timedelay = {
		0,
		min((u32)abs(dev->number_of_steps)*DELAY_PER_STEP_NS,
			(u32)DELAY_MAX_PER_STEP_NS),
	};

	getnstimeofday(&temptime);

	temptime = timespec_sub(temptime, (dev->timestamp_t_focus_abs));

	if (timespec_compare(&temptime, &timedelay) <= 0) {
		status |= ATOMISP_FOCUS_STATUS_MOVING;
		status |= ATOMISP_FOCUS_HP_IN_PROGRESS;
	} else {
		status |= ATOMISP_FOCUS_STATUS_ACCEPTS_NEW_MOVE;
		status |= ATOMISP_FOCUS_HP_COMPLETE;
	}
	*value = status;
	return 0;
}

static int mt9e013_q_focus_abs(struct v4l2_subdev *sd, s32 *value)
{
	struct mt9e013_device *dev = to_mt9e013_sensor(sd);
	s32 val;

	mt9e013_q_focus_status(sd, &val);

	if (val & ATOMISP_FOCUS_STATUS_MOVING)
		*value  = dev->focus - dev->number_of_steps;
	else
		*value  = dev->focus;

	return 0;
}

static long mt9e013_set_exposure(struct v4l2_subdev *sd, u16 coarse_itg,
				 u16 fine_itg, u16 gain)

{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;
	u16 frame_length;
	struct mt9e013_device *dev = to_mt9e013_sensor(sd);

	mutex_lock(&dev->input_lock);

	ret = mt9e013_read_reg(client, MT9E013_16BIT,
			       MT9E013_FRAME_LENGTH_LINES, &frame_length);
	if (ret)
		goto out;

	/* enable group hold */
	ret = mt9e013_write_reg_array(client, mt9e013_param_hold);
	if (ret)
		goto out;

	/* set coarse integration time */
	ret = mt9e013_write_reg(client, MT9E013_16BIT,
			MT9E013_COARSE_INTEGRATION_TIME, coarse_itg);
	if (ret)
		goto out_disable;

	/* set fine integration time */
	ret = mt9e013_write_reg(client, MT9E013_16BIT,
			MT9E013_FINE_INTEGRATION_TIME, fine_itg);
	if (ret)
		goto out_disable;

	/* set global gain */
	ret = mt9e013_write_reg(client, MT9E013_16BIT,
			MT9E013_GLOBAL_GAIN, gain);

	if (ret)
		goto out_disable;
	dev->gain       = gain;
	dev->coarse_itg = coarse_itg;
	dev->fine_itg   = fine_itg;

out_disable:
	/* disable group hold */
	mt9e013_write_reg_array(client, mt9e013_param_update);
out:
	mutex_unlock(&dev->input_lock);

	return ret;
}

static long mt9e013_s_exposure(struct v4l2_subdev *sd,
			       struct atomisp_exposure *exposure)
{
	u16 coarse_itg, fine_itg, gain;

	coarse_itg = exposure->integration_time[0];
	fine_itg = exposure->integration_time[1];
	gain = exposure->gain[0];

	/* we should not accept the invalid value below */
	if (fine_itg == 0 || gain == 0) {
		struct i2c_client *client = v4l2_get_subdevdata(sd);
		v4l2_err(client, "%s: invalid value\n", __func__);
		return -EINVAL;
	}
	return mt9e013_set_exposure(sd, coarse_itg, fine_itg, gain);
}

static int mt9e013_read_reg_array(struct i2c_client *client, u16 size, u16 addr,
				  void *data)
{
	u8 *buf = data;
	u16 index;
	int ret = 0;

	for (index = 0; index + MT9E013_BYTE_MAX <= size;
	     index += MT9E013_BYTE_MAX) {
		ret = mt9e013_read_reg(client, MT9E013_BYTE_MAX, addr + index,
				       (u16 *)&buf[index]);
		if (ret)
			return ret;
	}

	if (size - index > 0)
		ret = mt9e013_read_reg(client, size - index, addr + index,
				       (u16 *)&buf[index]);

	return ret;
}

static unsigned long
mt9e013_otp_sum(struct v4l2_subdev *sd, u8 *buf, u16 start, u16 end)
{
	unsigned long sum = 0;
	u16 i;

	for (i = start; i <= end; i++)
		sum += buf[i];

	return sum;
}

static int mt9e013_otp_checksum(struct v4l2_subdev *sd, u8 *buf, int list_len,
				const struct mt9e013_otp_checksum_format *list)
{
	unsigned long sum;
	u8 checksum;
	int i;
	int zero_flag = 1;

	for (i = 0; i < list_len; i++) {
		sum = mt9e013_otp_sum(sd, buf, list[i].start, list[i].end);
		checksum = sum % MT9E013_OTP_MOD_CHECKSUM;
		if (buf[list[i].checksum] != checksum)
			return -EINVAL;
		/*
		 * Checksum must fail if whole data is 0.
		 * Clear zero_flag if data != 0 is found.
		 */
		if (unlikely(zero_flag && (sum > 0)))
			zero_flag = 0;
	}

	return !zero_flag ? 0 : -EINVAL;
}

static int
__mt9e013_otp_read(struct v4l2_subdev *sd, const struct mt9e013_reg *type,
		   void *buf)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;
	int retry = 100;
	u16 ready;

	ret = mt9e013_write_reg_array(client, type);
	if (ret) {
		v4l2_err(client, "%s: failed to prepare OTP memory\n",
			 __func__);
		return ret;
	}

	do {
		ret = mt9e013_read_reg(client, MT9E013_16BIT,
				       MT9E013_OTP_READY_REG, &ready);
		if (ret) {
			v4l2_err(client,
			"%s: failed to read OTP memory status\n", __func__);
			return ret;
		}
		if (ready & MT9E013_OTP_READY_REG_DONE)
			break;
	} while (--retry);

	if (!retry) {
		v4l2_err(client, "%s: OTP memory read timeout.\n", __func__);
		return -ETIMEDOUT;
	}

	if (!(ready & MT9E013_OTP_READY_REG_OK)) {
		v4l2_err(client, "%s: OTP memory was initialized with error\n",
			  __func__);
		return -EIO;
	}
	ret = mt9e013_read_reg_array(client, MT9E013_OTP_DATA_SIZE,
				     MT9E013_OTP_START_ADDR, buf);
	if (ret) {
		v4l2_err(client, "%s: failed to read OTP data\n", __func__);
		return ret;
	}

	if (MT9E013_OTP_CHECKSUM) {
		ret = mt9e013_otp_checksum(sd, buf,
				ARRAY_SIZE(mt9e013_otp_checksum_list),
				mt9e013_otp_checksum_list);
		if (ret) {
			v4l2_err(client, "%s: OTP checksum failed\n", __func__);
			return ret;
		}
	}

	return 0;
}

static void *mt9e013_otp_read(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	void *buf;
	int ret;

	buf = kmalloc(MT9E013_OTP_DATA_SIZE, GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	/*
	 * Try all banks in reverse order and return after first success.
	 * Last used bank has most up-to-date data.
	 */
	ret = __mt9e013_otp_read(sd, mt9e013_otp_type32, buf);
	if (!ret)
		return buf;
	ret = __mt9e013_otp_read(sd, mt9e013_otp_type31, buf);
	if (!ret)
		return buf;
	ret = __mt9e013_otp_read(sd, mt9e013_otp_type30, buf);

	/* Driver has failed to find valid data */
	if (ret) {
		v4l2_err(client, "%s: sensor found no valid OTP data\n",
			  __func__);
		kfree(buf);
		return ERR_PTR(ret);
	}

	return buf;
}

static u8 *mt9e013_fuseid_read(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u8 data[MT9E013_FUSEID_SIZE];
	u8 *fuseid;
	int ret, i;

	ret = mt9e013_read_reg_array(client, MT9E013_FUSEID_SIZE,
				     MT9E013_FUSEID_START_ADDR, &data);
	if (ret < 0) {
		v4l2_err(client, "%s: error reading FUSEID.\n", __func__);
		return ERR_PTR(ret);
	}

	fuseid = kmalloc(sizeof(*fuseid) * MT9E013_FUSEID_SIZE, GFP_KERNEL);

	if (!fuseid) {
		v4l2_err(client,
		"%s: no memory available when reading FUSEID.\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	/* FUSEID is in reverse order */
	for (i = 0; i < MT9E013_FUSEID_SIZE; i++)
		fuseid[i] = data[MT9E013_FUSEID_SIZE - i - 1];

	return fuseid;
}

static int mt9e013_g_priv_int_data(struct v4l2_subdev *sd,
				   struct v4l2_private_int_data *priv)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mt9e013_device *dev = to_mt9e013_sensor(sd);
	u8 __user *to = priv->data;
	u32 read_size = priv->size;
	int ret;

	/* No OTP data available on sensor */
	if (!dev->otp_data || !dev->fuseid)
		return -EIO;

	/* No need to copy data if size is 0 */
	if (!read_size)
		goto out;

	/* Correct read_size value only if bigger than maximum */
	if (read_size > MT9E013_OTP_DATA_SIZE)
		read_size = MT9E013_OTP_DATA_SIZE;

	ret = copy_to_user(to, dev->otp_data, read_size);
	if (ret) {
		v4l2_err(client, "%s: failed to copy OTP data to user\n",
			 __func__);
		return -EFAULT;
	}

	/* No room for FUSEID */
	if (priv->size <= MT9E013_OTP_DATA_SIZE)
		goto out;

	read_size = priv->size - MT9E013_OTP_DATA_SIZE;
	if (read_size > MT9E013_FUSEID_SIZE)
		read_size = MT9E013_FUSEID_SIZE;
	to += MT9E013_OTP_DATA_SIZE;

	ret = copy_to_user(to, dev->fuseid, read_size);
	if (ret) {
		v4l2_err(client, "%s: failed to copy FUSEID to user\n",
			 __func__);
		return -EFAULT;
	}

out:
	/* Return correct size */
	priv->size = MT9E013_OTP_DATA_SIZE + MT9E013_FUSEID_SIZE;

	return 0;
}

static long mt9e013_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	switch (cmd) {
	case ATOMISP_IOC_S_EXPOSURE:
		return mt9e013_s_exposure(sd, (struct atomisp_exposure *)arg);
	case ATOMISP_IOC_G_SENSOR_PRIV_INT_DATA:
		return mt9e013_g_priv_int_data(sd, arg);
	default:
		return -EINVAL;
	}
	return 0;
}

static int mt9e013_init_registers(struct v4l2_subdev *sd)
{
	int ret;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	ret  = mt9e013_write_reg_array(client, mt9e013_reset_register);
	ret |= mt9e013_write_reg_array(client, mt9e013_pll_timing);
	ret |= mt9e013_write_reg_array(client, mt9e013_raw_10);
	ret |= mt9e013_write_reg_array(client, mt9e013_mipi_config);
	ret |= mt9e013_write_reg_array(client, mt9e013_recommended_settings);
	ret |= mt9e013_write_reg_array(client, mt9e013_mipi_timing);
	ret |= mt9e013_write_reg_array(client, mt9e013_scaler);
	ret |= mt9e013_write_reg_array(client, mt9e013_init_vcm);

	return ret;
}

static int __mt9e013_init(struct v4l2_subdev *sd, u32 val)
{
	int ret;
	struct mt9e013_device *dev = to_mt9e013_sensor(sd);
	u8 fw_rev[16];

	/* set inital registers */
	ret = mt9e013_init_registers(sd);

	/*set VCM to home position */
	ret |= mt9e013_t_focus_abs(sd, HOME_POS);

	/* restore settings */
	mt9e013_res = mt9e013_res_preview;
	N_RES = N_RES_PREVIEW;

	/* The only way to detect whether this VCM maintains focus after putting
	 * the sensor into standby mode is by looking at the SCU FW version.
	 * PR3.3 or higher runs on FW version 05.00 or higher.
	 * We cannot distinguish between PR3.2 and PR3.25, so we need to be
	 * conservative. PR3.25 owners can change the comparison to compare
	 * to PR3_2_FW instead of PR3_3_FW for testing purposes.
	 */
	dev->keeps_focus_pos = false;
	ret |= rpmsg_send_generic_command(IPCMSG_FW_REVISION, 0, NULL, 0,
				       (u32 *)fw_rev, 4);
	if (ret == 0) {
		u16 fw_version = (fw_rev[15] << 8) | fw_rev[14];
		dev->keeps_focus_pos = fw_version >= PR3_3_FW;
	}
	if (!dev->keeps_focus_pos) {
		v4l2_warn(sd,
		"VCM does not maintain focus position in standby mode, using software workaround\n");
	}

	return ret;
}

static int mt9e013_init(struct v4l2_subdev *sd, u32 val)
{
	struct mt9e013_device *dev = to_mt9e013_sensor(sd);
	int ret;

	mutex_lock(&dev->input_lock);
	ret = __mt9e013_init(sd, val);
	mutex_unlock(&dev->input_lock);

	return ret;
}

static void mt9e013_uninit(struct v4l2_subdev *sd)
{
	struct mt9e013_device *dev = to_mt9e013_sensor(sd);

	dev->coarse_itg = 0;
	dev->fine_itg   = 0;
	dev->gain       = 0;
	dev->focus      = MT9E013_INVALID_CONFIG;
}

static int power_up(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mt9e013_device *dev = to_mt9e013_sensor(sd);
	int ret;

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
	msleep(20);

	return 0;

fail_clk:
	dev->platform_data->flisclk_ctrl(sd, 0);
fail_power:
	dev->platform_data->power_ctrl(sd, 0);
	dev_err(&client->dev, "sensor power-up failed\n");

	return ret;
}

static int power_down(struct v4l2_subdev *sd)
{
	struct mt9e013_device *dev = to_mt9e013_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

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

static int __mt9e013_s_power(struct v4l2_subdev *sd, int on)
{
	struct mt9e013_device *dev = to_mt9e013_sensor(sd);
	int ret;

	if (on == 0) {
		mt9e013_uninit(sd);
		ret = power_down(sd);
		dev->power = 0;
	} else {
		ret = power_up(sd);
		if (!ret) {
			dev->power = 1;
			/* init motor initial position */
			return __mt9e013_init(sd, 0);
		}
	}

	return ret;
}

static int mt9e013_s_power(struct v4l2_subdev *sd, int on)
{
	struct mt9e013_device *dev = to_mt9e013_sensor(sd);
	int ret;

	mutex_lock(&dev->input_lock);
	ret = __mt9e013_s_power(sd, on);
	mutex_unlock(&dev->input_lock);

	return ret;
}

static int mt9e013_g_chip_ident(struct v4l2_subdev *sd,
				struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!chip)
		return -EINVAL;

	v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_MT9E013, 0);

	return 0;
}

static int
mt9e013_get_intg_factor(struct i2c_client *client,
			struct camera_mipi_info *info,
			const struct mt9e013_reg *reglist)
{
	unsigned int	vt_pix_clk_div;
	unsigned int	vt_sys_clk_div;
	unsigned int	pre_pll_clk_div;
	unsigned int	pll_multiplier;
	unsigned int	op_pix_clk_div;
	unsigned int	op_sys_clk_div;

    /* TODO: this should not be a constant but should be set by a call to
     * MSIC's driver to get the ext_clk that MSIC supllies to the sensor.
     */
	const int ext_clk_freq_mhz = 19200000;
	struct atomisp_sensor_mode_data buf;
	const struct mt9e013_reg *next = reglist;
	int vt_pix_clk_freq_mhz;
	u16 data[MT9E013_SHORT_MAX];

	unsigned int coarse_integration_time_min;
	unsigned int coarse_integration_time_max_margin;
	unsigned int fine_integration_time_min;
	unsigned int fine_integration_time_max_margin;
	unsigned int frame_length_lines;
	unsigned int line_length_pck;
	unsigned int read_mode;
	u16 value;
	int ret;

	if (info == NULL)
		return -EINVAL;

	memset(data, 0, MT9E013_SHORT_MAX * sizeof(u16));
	if (mt9e013_read_reg(client, 12, MT9E013_VT_PIX_CLK_DIV, data))
		return -EINVAL;
	vt_pix_clk_div = data[0];
	vt_sys_clk_div = data[1];
	pre_pll_clk_div = data[2];
	pll_multiplier = data[3];
	op_pix_clk_div = data[4];
	op_sys_clk_div = data[5];

	memset(data, 0, MT9E013_SHORT_MAX * sizeof(u16));
	if (mt9e013_read_reg(client, 4, MT9E013_FRAME_LENGTH_LINES, data))
		return -EINVAL;
	frame_length_lines = data[0];
	line_length_pck = data[1];

	memset(data, 0, MT9E013_SHORT_MAX * sizeof(u16));
	if (mt9e013_read_reg(client, 8, MT9E013_COARSE_INTG_TIME_MIN, data))
		return -EINVAL;
	coarse_integration_time_min = data[0];
	coarse_integration_time_max_margin = data[1];
	fine_integration_time_min = data[2];
	fine_integration_time_max_margin = data[3];

	memset(data, 0, MT9E013_SHORT_MAX * sizeof(u16));
	if (mt9e013_read_reg(client, 2, MT9E013_READ_MODE, data))
		return -EINVAL;
	read_mode = data[0];

	vt_pix_clk_freq_mhz = divsave_rounded(ext_clk_freq_mhz*pll_multiplier,
				pre_pll_clk_div*vt_sys_clk_div*vt_pix_clk_div);

	for (; next->type != MT9E013_TOK_TERM; next++) {
		if (next->type == MT9E013_16BIT) {
			if (next->reg.sreg == MT9E013_FINE_INTEGRATION_TIME) {
				buf.fine_integration_time_def = next->val;
				break;
			}
		}
	}

	/* something's wrong here, this mode does not have fine_igt set! */
	if (next->type == MT9E013_TOK_TERM)
		return -EINVAL;

	buf.coarse_integration_time_min = coarse_integration_time_min;
	buf.coarse_integration_time_max_margin =
					coarse_integration_time_max_margin;
	buf.fine_integration_time_min = fine_integration_time_min;
	buf.fine_integration_time_max_margin = fine_integration_time_max_margin;
	buf.vt_pix_clk_freq_mhz = vt_pix_clk_freq_mhz;
	buf.line_length_pck = line_length_pck;
	buf.frame_length_lines = frame_length_lines;
	buf.read_mode = read_mode;

	/* 1: normal 3:inc 2, 7:inc 4 addresses in X direction*/
	buf.binning_factor_x =
		(((read_mode & MT9E013_READ_MODE_X_ODD_INC) >> 6) + 1) / 2;

	/*
	 * 1:normal 3:inc 2, 7:inc 4, 15:inc 8, 31:inc 16, 63:inc 32 addresses
	 * in Y direction
	 */
	buf.binning_factor_y =
			((read_mode & MT9E013_READ_MODE_Y_ODD_INC) + 1) / 2;

	/* Get the cropping and output resolution to ISP for this mode. */
	ret = mt9e013_read_reg(client, MT9E013_16BIT,
				MT9E013_HORIZONTAL_START_H, &value);
	if (ret)
		return ret;
	buf.crop_horizontal_start = value;

	ret = mt9e013_read_reg(client, MT9E013_16BIT, MT9E013_VERTICAL_START_H,
				&value);
	if (ret)
		return ret;
	buf.crop_vertical_start = value;

	ret = mt9e013_read_reg(client, MT9E013_16BIT, MT9E013_HORIZONTAL_END_H,
				&value);
	if (ret)
		return ret;
	buf.crop_horizontal_end = value;

	ret = mt9e013_read_reg(client, MT9E013_16BIT, MT9E013_VERTICAL_END_H,
				&value);
	if (ret)
		return ret;
	buf.crop_vertical_end = value;

	ret = mt9e013_read_reg(client, MT9E013_16BIT,
				MT9E013_HORIZONTAL_OUTPUT_SIZE_H, &value);
	if (ret)
		return ret;
	buf.output_width = value;

	ret = mt9e013_read_reg(client, MT9E013_16BIT,
				MT9E013_VERTICAL_OUTPUT_SIZE_H, &value);
	if (ret)
		return ret;
	buf.output_height = value;

	memcpy(&info->data, &buf, sizeof(buf));

	return 0;
}

/* This returns the exposure time being used. This should only be used
   for filling in EXIF data, not for actual image processing. */
static int mt9e013_q_exposure(struct v4l2_subdev *sd, s32 *value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u16 coarse;
	int ret;

	/* the fine integration time is currently not calculated */
	ret = mt9e013_read_reg(client, MT9E013_16BIT,
			       MT9E013_COARSE_INTEGRATION_TIME, &coarse);
	*value = coarse;

	return ret;
}

static int mt9e013_test_pattern(struct v4l2_subdev *sd, s32 value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return mt9e013_write_reg(client, MT9E013_16BIT, 0x3070, value);
}

static int mt9e013_v_flip(struct v4l2_subdev *sd, s32 value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	if (value > 1)
		return -EINVAL;

	ret = mt9e013_write_reg_array(client, mt9e013_param_hold);
	if (ret)
		return ret;
	ret = mt9e013_rmw_reg(client, MT9E013_16BIT & ~MT9E013_RMW,
			       0x3040, 0x8000, value);
	if (ret)
		return ret;
	return mt9e013_write_reg_array(client, mt9e013_param_update);
}


static int mt9e013_t_vcm_slew(struct v4l2_subdev *sd, s32 value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (value > MT9E013_VCM_SLEW_STEP_MAX)
		return -EINVAL;

	return mt9e013_rmw_reg(client, MT9E013_16BIT, MT9E013_VCM_SLEW_STEP,
				MT9E013_VCM_SLEW_STEP_MASK, value);
}

static int mt9e013_t_vcm_timing(struct v4l2_subdev *sd, s32 value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	/* Max 16 bits */
	if (value > MT9E013_VCM_SLEW_TIME_MAX)
		return -EINVAL;

	return mt9e013_write_reg(client, MT9E013_16BIT, MT9E013_VCM_SLEW_TIME,
				 value);
}

static int mt9e013_g_focal(struct v4l2_subdev *sd, s32 *val)
{
	*val = (MT9E013_FOCAL_LENGTH_NUM << 16) | MT9E013_FOCAL_LENGTH_DEM;
	return 0;
}

static int mt9e013_g_fnumber(struct v4l2_subdev *sd, s32 *val)
{
	/*const f number for mt9e013*/
	*val = (MT9E013_F_NUMBER_DEFAULT_NUM << 16) | MT9E013_F_NUMBER_DEM;
	return 0;
}

static int mt9e013_g_fnumber_range(struct v4l2_subdev *sd, s32 *val)
{
	*val = (MT9E013_F_NUMBER_DEFAULT_NUM << 24) |
		(MT9E013_F_NUMBER_DEM << 16) |
		(MT9E013_F_NUMBER_DEFAULT_NUM << 8) | MT9E013_F_NUMBER_DEM;
	return 0;
}

static int mt9e013_g_bin_factor_x(struct v4l2_subdev *sd, s32 *val)
{
	struct mt9e013_device *dev = to_mt9e013_sensor(sd);

	*val = mt9e013_res[dev->fmt_idx].bin_factor_x;

	return 0;
}

static int mt9e013_g_bin_factor_y(struct v4l2_subdev *sd, s32 *val)
{
	struct mt9e013_device *dev = to_mt9e013_sensor(sd);

	*val = mt9e013_res[dev->fmt_idx].bin_factor_y;

	return 0;
}

static struct mt9e013_control mt9e013_controls[] = {
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
		.query = mt9e013_q_exposure,
	},
	{
		.qc = {
			.id = V4L2_CID_TEST_PATTERN,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "Test pattern",
			.minimum = 0,
			.maximum = 0xffff,
			.step = 1,
			.default_value = 0,
		},
		.tweak = mt9e013_test_pattern,
	},
	{
		.qc = {
			.id = V4L2_CID_VFLIP,
			.type = V4L2_CTRL_TYPE_BOOLEAN,
			.name = "Flip",
			.minimum = 0,
			.maximum = 1,
			.step = 1,
			.default_value = 0,
		},
		.tweak = mt9e013_v_flip,
	},
	{
		.qc = {
			.id = V4L2_CID_FOCUS_ABSOLUTE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "focus move absolute",
			.minimum = 0,
			.maximum = MT9E013_MAX_FOCUS_POS,
			.step = 1,
			.default_value = 0,
			.flags = 0,
		},
		.tweak = mt9e013_t_focus_abs,
		.query = mt9e013_q_focus_abs,
	},
	{
		.qc = {
			.id = V4L2_CID_FOCUS_RELATIVE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "focus move relative",
			.minimum = MT9E013_MAX_FOCUS_NEG,
			.maximum = MT9E013_MAX_FOCUS_POS,
			.step = 1,
			.default_value = 0,
			.flags = 0,
		},
		.tweak = mt9e013_t_focus_rel,
	},
	{
		.qc = {
			.id = V4L2_CID_FOCUS_STATUS,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "focus status",
			.minimum = 0,
			.maximum = 100, /* allow enum to grow in the future */
			.step = 1,
			.default_value = 0,
			.flags = 0,
		},
		.query = mt9e013_q_focus_status,
	},
	{
		.qc = {
			.id = V4L2_CID_VCM_SLEW,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "vcm slew",
			.minimum = 0,
			.maximum = MT9E013_VCM_SLEW_STEP_MAX,
			.step = 1,
			.default_value = 0,
			.flags = 0,
		},
		.tweak = mt9e013_t_vcm_slew,
	},
	{
		.qc = {
			.id = V4L2_CID_VCM_TIMEING,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "vcm step time",
			.minimum = 0,
			.maximum = MT9E013_VCM_SLEW_TIME_MAX,
			.step = 1,
			.default_value = 0,
			.flags = 0,
		},
		.tweak = mt9e013_t_vcm_timing,
	},
	{
		.qc = {
			.id = V4L2_CID_FOCAL_ABSOLUTE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "focal length",
			.minimum = MT9E013_FOCAL_LENGTH_DEFAULT,
			.maximum = MT9E013_FOCAL_LENGTH_DEFAULT,
			.step = 0x01,
			.default_value = MT9E013_FOCAL_LENGTH_DEFAULT,
			.flags = 0,
		},
		.query = mt9e013_g_focal,
	},
	{
		.qc = {
			.id = V4L2_CID_FNUMBER_ABSOLUTE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "f-number",
			.minimum = MT9E013_F_NUMBER_DEFAULT,
			.maximum = MT9E013_F_NUMBER_DEFAULT,
			.step = 0x01,
			.default_value = MT9E013_F_NUMBER_DEFAULT,
			.flags = 0,
		},
		.query = mt9e013_g_fnumber,
	},
	{
		.qc = {
			.id = V4L2_CID_FNUMBER_RANGE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "f-number range",
			.minimum = MT9E013_F_NUMBER_RANGE,
			.maximum =  MT9E013_F_NUMBER_RANGE,
			.step = 0x01,
			.default_value = MT9E013_F_NUMBER_RANGE,
			.flags = 0,
		},
		.query = mt9e013_g_fnumber_range,
	},
	{
		.qc = {
			.id = V4L2_CID_BIN_FACTOR_HORZ,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "horizontal binning factor",
			.minimum = 0,
			.maximum = MT9E013_BIN_FACTOR_MAX,
			.step = 1,
			.default_value = 0,
			.flags = 0,
		},
		.query = mt9e013_g_bin_factor_x,
	},
	{
		.qc = {
			.id = V4L2_CID_BIN_FACTOR_VERT,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "vertical binning factor",
			.minimum = 0,
			.maximum = MT9E013_BIN_FACTOR_MAX,
			.step = 1,
			.default_value = 0,
			.flags = 0,
		},
		.query = mt9e013_g_bin_factor_y,
	},
};
#define N_CONTROLS (ARRAY_SIZE(mt9e013_controls))

static struct mt9e013_control *mt9e013_find_control(u32 id)
{
	int i;

	for (i = 0; i < N_CONTROLS; i++)
		if (mt9e013_controls[i].qc.id == id)
			return &mt9e013_controls[i];
	return NULL;
}

static int mt9e013_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
	struct mt9e013_device *dev = to_mt9e013_sensor(sd);
	struct mt9e013_control *ctrl = mt9e013_find_control(qc->id);

	if (ctrl == NULL)
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	*qc = ctrl->qc;
	mutex_unlock(&dev->input_lock);

	return 0;
}

/* mt9e013 control set/get */
static int mt9e013_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct mt9e013_device *dev = to_mt9e013_sensor(sd);
	struct mt9e013_control *s_ctrl;
	int ret;

	if (!ctrl)
		return -EINVAL;

	s_ctrl = mt9e013_find_control(ctrl->id);
	if ((s_ctrl == NULL) || (s_ctrl->query == NULL))
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	ret = s_ctrl->query(sd, &ctrl->value);
	mutex_unlock(&dev->input_lock);

	return ret;
}

static int mt9e013_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct mt9e013_device *dev = to_mt9e013_sensor(sd);
	struct mt9e013_control *octrl = mt9e013_find_control(ctrl->id);
	int ret;

	if ((octrl == NULL) || (octrl->tweak == NULL))
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	ret = octrl->tweak(sd, ctrl->value);
	mutex_unlock(&dev->input_lock);

	return ret;
}

/*
 * distance - calculate the distance
 * @res: resolution
 * @w: width
 * @h: height
 *
 * Get the gap between resolution and w/h.
 * res->width/height smaller than w/h wouldn't be considered.
 * Returns the value of gap or -1 if fail.
 */
static int distance(struct mt9e013_resolution const *res, const u32 w,
		    const u32 h, const s32 m)
{
	u32 w_ratio = ((res->width<<13)/w);
	u32 h_ratio = ((res->height<<13)/h);
	s32 match   = abs(((w_ratio<<13)/h_ratio) - ((s32)8192));

	if ((w_ratio < (s32)8192) || (h_ratio < (s32)8192)  || (match > m))
		return -1;

	return w_ratio + h_ratio;
}


/*
 * Tune this value so that the DVS resolutions get selected properly,
 * but make sure 16:9 does not match 4:3
 */
#define LARGEST_ALLOWED_RATIO_MISMATCH 140

/*
 * Returns the nearest higher resolution index.
 * @w: width
 * @h: height
 * matching is done based on enveloping resolution and
 * aspect ratio. If the aspect ratio cannot be matched
 * to any index, the search is done again using envelopel
 * matching only. if no match can be found again, -1 is
 * returned.
 */
static int nearest_resolution_index(int w, int h)
{
	int i, j;
	int idx = -1;
	int dist;
	int min_dist = INT_MAX;
	struct mt9e013_resolution *tmp_res = NULL;
	s32 m = LARGEST_ALLOWED_RATIO_MISMATCH;

#ifdef CONFIG_VIDEO_MT9E013_ENZO
	/*
	 * For 1080p or 1088p resolutions, we must select index 6.
	 * This is an exception. If we let algorithm to run, this case
	 * will fail.
	 */
	if (h == 1080 || h == 1088)
		return 6;
#endif

	for (j = 0; j < 2; ++j) {
		for (i = 0; i < N_RES; i++) {
			tmp_res = &mt9e013_res[i];
			dist = distance(tmp_res, w, h, m);
			if (dist == -1)
				continue;
			if (dist < min_dist) {
				min_dist = dist;
				idx = i;
			}
		}
		if (idx != -1)
			break;
		m = LONG_MAX;
	}
	return idx;
}

static int get_resolution_index(int w, int h)
{
	int i;

	for (i = 0; i < N_RES; i++) {
		if (w != mt9e013_res[i].width)
			continue;
		if (h != mt9e013_res[i].height)
			continue;
		/* Found it */
		return i;
	}
	return -1;
}

static int __mt9e013_try_mbus_fmt(struct v4l2_subdev *sd,
				struct v4l2_mbus_framefmt *fmt)
{
	int idx;

	if (!fmt)
		return -EINVAL;

	if (fmt->width > MT9E013_RES_WIDTH_MAX ||
	    fmt->height > MT9E013_RES_HEIGHT_MAX) {
		fmt->width = MT9E013_RES_WIDTH_MAX;
		fmt->height = MT9E013_RES_HEIGHT_MAX;
	} else {
		idx = nearest_resolution_index(fmt->width, fmt->height);

		/*
		 * nearest_resolution_index() doesn't return smaller
		 * resolutions. If it fails, it means the requested
		 * resolution is higher than we can support.
		 * Fallback to highest possible resolution in this case.
		 */
		if (idx == -1)
			idx = N_RES - 1;

		fmt->width = mt9e013_res[idx].width;
		fmt->height = mt9e013_res[idx].height;
	}

	fmt->code = V4L2_MBUS_FMT_SGRBG10_1X10;

	return 0;
}

static int mt9e013_try_mbus_fmt(struct v4l2_subdev *sd,
				struct v4l2_mbus_framefmt *fmt)
{
	struct mt9e013_device *dev = to_mt9e013_sensor(sd);
	int ret;

	mutex_lock(&dev->input_lock);
	ret = __mt9e013_try_mbus_fmt(sd, fmt);
	mutex_unlock(&dev->input_lock);

	return ret;
}

static int mt9e013_get_mbus_format_code(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mt9e013_device *dev = to_mt9e013_sensor(sd);
	u16 reg;

	if (!dev->power)
		return -EIO;

	/*
	 * Get the order of color pixel readout.
	 * Change with mirror and flip.
	 */
	if (mt9e013_read_reg(client, MT9E013_8BIT,
			MT9E013_PIXEL_ORDER, &reg)) {
		return -EIO;
	}
	switch (reg) {
	case MT9E013_PIXEL_ORDER0:
		return V4L2_MBUS_FMT_SGRBG10_1X10;
	case MT9E013_PIXEL_ORDER1:
		return V4L2_MBUS_FMT_SRGGB10_1X10;
	case MT9E013_PIXEL_ORDER2:
		return V4L2_MBUS_FMT_SBGGR10_1X10;
	case MT9E013_PIXEL_ORDER3:
		return V4L2_MBUS_FMT_SGBRG10_1X10;
	default:
		return -EINVAL;
	}
}

static int mt9e013_s_mbus_fmt(struct v4l2_subdev *sd,
			      struct v4l2_mbus_framefmt *fmt)
{
	struct mt9e013_device *dev = to_mt9e013_sensor(sd);
	const struct mt9e013_reg *mt9e013_def_reg;
	struct camera_mipi_info *mt9e013_info = NULL;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	mt9e013_info = v4l2_get_subdev_hostdata(sd);
	if (mt9e013_info == NULL)
		return -EINVAL;

	mutex_lock(&dev->input_lock);

	ret = __mt9e013_try_mbus_fmt(sd, fmt);
	if (ret) {
		mutex_unlock(&dev->input_lock);
		v4l2_err(sd, "try fmt fail\n");
		return ret;
	}

	dev->fmt_idx = get_resolution_index(fmt->width, fmt->height);

	/* Sanity check */
	if (unlikely(dev->fmt_idx == -1)) {
		mutex_unlock(&dev->input_lock);
		v4l2_err(sd, "get resolution fail\n");
		return -EINVAL;
	}

	mt9e013_def_reg = mt9e013_res[dev->fmt_idx].regs;

	ret = mt9e013_write_reg_array(client, mt9e013_def_reg);
	if (ret) {
		mutex_unlock(&dev->input_lock);
		return -EINVAL;
	}

	fmt->code = mt9e013_get_mbus_format_code(sd);
	if (fmt->code < 0) {
		mutex_unlock(&dev->input_lock);
		return fmt->code;
	}
	dev->fps = mt9e013_res[dev->fmt_idx].fps;
	dev->pixels_per_line = mt9e013_res[dev->fmt_idx].pixels_per_line;
	dev->lines_per_frame = mt9e013_res[dev->fmt_idx].lines_per_frame;
	dev->coarse_itg = 0;
	dev->fine_itg = 0;
	dev->gain = 0;

	ret = mt9e013_get_intg_factor(client, mt9e013_info, mt9e013_def_reg);
	mutex_unlock(&dev->input_lock);
	if (ret) {
		v4l2_err(sd, "failed to get integration_factor\n");
		return -EINVAL;
	}

	return 0;
}

static int mt9e013_g_mbus_fmt(struct v4l2_subdev *sd,
			      struct v4l2_mbus_framefmt *fmt)
{
	struct mt9e013_device *dev = to_mt9e013_sensor(sd);

	if (!fmt)
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	fmt->width = mt9e013_res[dev->fmt_idx].width;
	fmt->height = mt9e013_res[dev->fmt_idx].height;
	fmt->code = mt9e013_get_mbus_format_code(sd);
	mutex_unlock(&dev->input_lock);

	return fmt->code < 0 ? fmt->code : 0;
}

static int mt9e013_detect(struct i2c_client *client, u16 *id, u8 *revision)
{
	struct i2c_adapter *adapter = client->adapter;
	u16 reg;

	/* i2c check */
	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C))
		return -ENODEV;

	/* check sensor chip model and revision IDs */
	if (mt9e013_read_reg(client, MT9E013_16BIT, MT9E013_SC_CMMN_CHIP_ID,
				id)) {
		v4l2_err(client, "Reading sensor_id error.\n");
		return -ENODEV;
	}

	if (*id != MT9E013_ID && *id != MT9E013_ID2) {
		v4l2_err(client,
			"sensor ID error, sensor_id = 0x%x\n", *id);
		return -ENODEV;
	}

	if (mt9e013_read_reg(client, MT9E013_8BIT, MT9E013_SC_CMMN_REV_ID,
				&reg)) {
		v4l2_err(client, "Reading sensor_rev_id error.\n");
		return -ENODEV;
	}
	*revision = (u8)reg;

	return 0;
}

/*
 * mt9e013 stream on/off
 */
static int mt9e013_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mt9e013_device *dev = to_mt9e013_sensor(sd);

	mutex_lock(&dev->input_lock);
	if (enable) {
		if (!dev->keeps_focus_pos) {
			struct mt9e013_reg mt9e013_stream_enable[] = {
				mt9e013_streaming[0],
				/* VCM_NEW_CODE */
				{MT9E013_16BIT, {0x30F2}, 0x0000},
				INIT_VCM_CONTROL,
				/* VCM_NEW_CODE */
				{MT9E013_16BIT, {0x30F2}, 0x0000},
				{MT9E013_TOK_DELAY, {0}, 60},
				{MT9E013_TOK_TERM, {0}, 0}
			};

			mt9e013_stream_enable[1].val = dev->focus + 1;
			mt9e013_stream_enable[3].val = dev->focus;

			ret = mt9e013_write_reg_array(client,
							mt9e013_stream_enable);
		} else {
			ret = mt9e013_write_reg_array(client,
							mt9e013_streaming);
		}

		if (ret != 0) {
			mutex_unlock(&dev->input_lock);
			v4l2_err(client, "write_reg_array err\n");
			return ret;
		}
		dev->streaming = 1;
	} else {

		ret = mt9e013_write_reg_array(client, mt9e013_soft_standby);
		if (ret != 0) {
			mutex_unlock(&dev->input_lock);
			v4l2_err(client, "write_reg_array err\n");
			return ret;
		}
		dev->streaming = 0;
	}

	/* restore settings */
	mt9e013_res = mt9e013_res_preview;
	N_RES = N_RES_PREVIEW;
	mutex_unlock(&dev->input_lock);

	return 0;
}

/*
 * mt9e013 enum frame size, frame intervals
 */
static int mt9e013_enum_framesizes(struct v4l2_subdev *sd,
				   struct v4l2_frmsizeenum *fsize)
{
	struct mt9e013_device *dev = to_mt9e013_sensor(sd);
	unsigned int index = fsize->index;

	if (index >= N_RES)
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	fsize->discrete.width = mt9e013_res[index].width;
	fsize->discrete.height = mt9e013_res[index].height;
	fsize->reserved[0] = mt9e013_res[index].used;
	mutex_unlock(&dev->input_lock);

	return 0;
}

static int mt9e013_enum_frameintervals(struct v4l2_subdev *sd,
				       struct v4l2_frmivalenum *fival)
{
	struct mt9e013_device *dev = to_mt9e013_sensor(sd);
	unsigned int index = fival->index;

	if (index >= N_RES)
		return -EINVAL;

	mutex_lock(&dev->input_lock);

	/*
	 * Since the isp will donwscale the resolution to the right size,
	 * find the nearest one that will allow the isp to do so important
	 * to ensure that the resolution requested is padded correctly by
	 * the requester, which is the atomisp driver in this case.
	 */
	index = nearest_resolution_index(fival->width, fival->height);

	if (-1 == index) {
		mutex_unlock(&dev->input_lock);
		return -EINVAL;
	}

	fival->type = V4L2_FRMIVAL_TYPE_DISCRETE;
/*	fival->width = mt9e013_res[index].width;
	fival->height = mt9e013_res[index].height; */
	fival->discrete.numerator = 1;
	fival->discrete.denominator = mt9e013_res[index].fps;

	mutex_unlock(&dev->input_lock);

	return 0;
}

static int mt9e013_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned int index,
				 enum v4l2_mbus_pixelcode *code)
{
	struct mt9e013_device *dev = to_mt9e013_sensor(sd);

	mutex_lock(&dev->input_lock);
	*code = mt9e013_get_mbus_format_code(sd);
	mutex_unlock(&dev->input_lock);

	return *code < 0 ? *code : 0;
}

static int mt9e013_s_config(struct v4l2_subdev *sd,
			    int irq, void *pdata)
{
	struct mt9e013_device *dev = to_mt9e013_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u8 sensor_revision;
	u16 sensor_id;
	int ret;
	void *otp_data;
	void *fuseid;

	if (pdata == NULL)
		return -ENODEV;

	dev->platform_data = pdata;

	mutex_lock(&dev->input_lock);

	/*
	 * The initial state of physical power is unknown
	 * so first power down it to make it to a known
	 * state, and then start the power up sequence
	 */
	power_down(sd);
	msleep(20);

	ret = __mt9e013_s_power(sd, 1);
	if (ret) {
		v4l2_err(client, "mt9e013 power-up err.\n");
		mutex_unlock(&dev->input_lock);
		return ret;
	}

	ret = dev->platform_data->csi_cfg(sd, 1);
	if (ret)
		goto fail_csi_cfg;

	/* config & detect sensor */
	ret = mt9e013_detect(client, &sensor_id, &sensor_revision);
	if (ret) {
		v4l2_err(client, "mt9e013_detect err s_config.\n");
		goto fail_detect;
	}

	dev->sensor_id = sensor_id;
	dev->sensor_revision = sensor_revision;

	/* Read sensor's OTP data */
	otp_data = mt9e013_otp_read(sd);
	if (!IS_ERR(otp_data))
		dev->otp_data = otp_data;

	fuseid = mt9e013_fuseid_read(sd);
	if (!IS_ERR(fuseid))
		dev->fuseid = fuseid;

	/* power off sensor */
	ret = __mt9e013_s_power(sd, 0);
	mutex_unlock(&dev->input_lock);
	if (ret) {
		v4l2_err(client, "mt9e013 power-down err.\n");
		return ret;
	}

	return 0;

fail_detect:
	dev->platform_data->csi_cfg(sd, 0);
fail_csi_cfg:
	__mt9e013_s_power(sd, 0);
	mutex_unlock(&dev->input_lock);
	dev_err(&client->dev, "sensor power-gating failed\n");
	return ret;
}

static int
mt9e013_enum_mbus_code(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_mbus_code_enum *code)
{
	struct mt9e013_device *dev = to_mt9e013_sensor(sd);

	if (code->index >= MAX_FMTS)
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	code->code = mt9e013_get_mbus_format_code(sd);
	mutex_unlock(&dev->input_lock);

	return code->code < 0 ? code->code : 0;
}

static int
mt9e013_enum_frame_size(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
			struct v4l2_subdev_frame_size_enum *fse)
{
	struct mt9e013_device *dev = to_mt9e013_sensor(sd);
	int index = fse->index;

	if (index >= N_RES)
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	fse->min_width = mt9e013_res[index].width;
	fse->min_height = mt9e013_res[index].height;
	fse->max_width = mt9e013_res[index].width;
	fse->max_height = mt9e013_res[index].height;
	mutex_unlock(&dev->input_lock);

	return 0;
}

static struct v4l2_mbus_framefmt *
__mt9e013_get_pad_format(struct mt9e013_device *sensor,
			 struct v4l2_subdev_fh *fh, unsigned int pad,
			 enum v4l2_subdev_format_whence which)
{
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->sd);

	if (pad != 0) {
		v4l2_err(client, "%s err. pad %x\n", __func__, pad);
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
mt9e013_get_pad_format(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_format *fmt)
{
	struct mt9e013_device *dev = to_mt9e013_sensor(sd);
	struct v4l2_mbus_framefmt *format =
			__mt9e013_get_pad_format(dev, fh, fmt->pad, fmt->which);

	if (format == NULL)
		return -EINVAL;
	fmt->format = *format;

	return 0;
}

static int
mt9e013_set_pad_format(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_format *fmt)
{
	struct mt9e013_device *dev = to_mt9e013_sensor(sd);
	struct v4l2_mbus_framefmt *format =
			__mt9e013_get_pad_format(dev, fh, fmt->pad, fmt->which);

	if (format == NULL)
		return -EINVAL;
	if (fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE)
		dev->format = fmt->format;

	return 0;
}

static int
mt9e013_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	struct mt9e013_device *dev = to_mt9e013_sensor(sd);

	mutex_lock(&dev->input_lock);

	if (dev->streaming) {
		mutex_unlock(&dev->input_lock);
		return -EBUSY;
	}

	dev->run_mode = param->parm.capture.capturemode;

	switch (dev->run_mode) {
	case CI_MODE_VIDEO:
		mt9e013_res = mt9e013_res_video;
		N_RES = N_RES_VIDEO;
		break;
	case CI_MODE_STILL_CAPTURE:
		mt9e013_res = mt9e013_res_still;
		N_RES = N_RES_STILL;
		break;
	default:
		mt9e013_res = mt9e013_res_preview;
		N_RES = N_RES_PREVIEW;
	}

	/* Reset sensor mode */
	dev->fmt_idx = 0;
	dev->fps = mt9e013_res[dev->fmt_idx].fps;
	dev->pixels_per_line = mt9e013_res[dev->fmt_idx].pixels_per_line;
	dev->lines_per_frame = mt9e013_res[dev->fmt_idx].lines_per_frame;
	dev->coarse_itg = 0;
	dev->fine_itg = 0;
	dev->gain = 0;

	mutex_unlock(&dev->input_lock);
	return 0;
}

static int
mt9e013_g_frame_interval(struct v4l2_subdev *sd,
				struct v4l2_subdev_frame_interval *interval)
{
	struct mt9e013_device *dev = to_mt9e013_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u16 lines_per_frame;

	mutex_lock(&dev->input_lock);

	/*
	 * if no specific information to calculate the fps,
	 * just used the value in sensor settings
	 */
	if (!dev->pixels_per_line || !dev->lines_per_frame) {
		interval->interval.numerator = 1;
		interval->interval.denominator = dev->fps;
		mutex_unlock(&dev->input_lock);
		return 0;
	}

	/*
	 * DS: if coarse_integration_time is set larger than
	 * lines_per_frame the frame_size will be expanded to
	 * coarse_integration_time+1
	 */
	if (dev->coarse_itg > dev->lines_per_frame) {
		if (dev->coarse_itg == 0xFFFF) {
			/*
			 * we can not add 1 according to ds, as this will
			 * cause over flow
			 */
			v4l2_warn(client, "%s: abnormal coarse_itg:0x%x\n",
				  __func__, dev->coarse_itg);
			lines_per_frame = dev->coarse_itg;
		} else
			lines_per_frame = dev->coarse_itg + 1;
	} else
		lines_per_frame = dev->lines_per_frame;

	interval->interval.numerator = dev->pixels_per_line *
					lines_per_frame;
	interval->interval.denominator = MT9E013_MCLK * 1000000;

	mutex_unlock(&dev->input_lock);

	return 0;
}

static int mt9e013_g_skip_frames(struct v4l2_subdev *sd, u32 *frames)
{
	struct mt9e013_device *dev = to_mt9e013_sensor(sd);

	if (frames == NULL)
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	*frames = mt9e013_res[dev->fmt_idx].skip_frames;
	mutex_unlock(&dev->input_lock);

	return 0;
}
static const struct v4l2_subdev_video_ops mt9e013_video_ops = {
	.s_stream = mt9e013_s_stream,
	.enum_framesizes = mt9e013_enum_framesizes,
	.enum_frameintervals = mt9e013_enum_frameintervals,
	.enum_mbus_fmt = mt9e013_enum_mbus_fmt,
	.try_mbus_fmt = mt9e013_try_mbus_fmt,
	.g_mbus_fmt = mt9e013_g_mbus_fmt,
	.s_mbus_fmt = mt9e013_s_mbus_fmt,
	.s_parm = mt9e013_s_parm,
	.g_frame_interval = mt9e013_g_frame_interval,
};

static struct v4l2_subdev_sensor_ops mt9e013_sensor_ops = {
	.g_skip_frames	= mt9e013_g_skip_frames,
};

static const struct v4l2_subdev_core_ops mt9e013_core_ops = {
	.g_chip_ident = mt9e013_g_chip_ident,
	.queryctrl = mt9e013_queryctrl,
	.g_ctrl = mt9e013_g_ctrl,
	.s_ctrl = mt9e013_s_ctrl,
	.s_power = mt9e013_s_power,
	.ioctl = mt9e013_ioctl,
	.init = mt9e013_init,
};

/* REVISIT: Do we need pad operations? */
static const struct v4l2_subdev_pad_ops mt9e013_pad_ops = {
	.enum_mbus_code = mt9e013_enum_mbus_code,
	.enum_frame_size = mt9e013_enum_frame_size,
	.get_fmt = mt9e013_get_pad_format,
	.set_fmt = mt9e013_set_pad_format,
};

static const struct v4l2_subdev_ops mt9e013_ops = {
	.core = &mt9e013_core_ops,
	.video = &mt9e013_video_ops,
	.pad = &mt9e013_pad_ops,
	.sensor = &mt9e013_sensor_ops,
};

static const struct media_entity_operations mt9e013_entity_ops = {
	.link_setup = NULL,
};

static int mt9e013_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct mt9e013_device *dev = to_mt9e013_sensor(sd);

	dev->platform_data->csi_cfg(sd, 0);
	v4l2_device_unregister_subdev(sd);
	kfree(dev->otp_data);
	kfree(dev->fuseid);
	kfree(dev);

	return 0;
}

static int mt9e013_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct mt9e013_device *dev;
	int ret;

	/* allocate sensor device & init sub device */
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		v4l2_err(client, "%s: out of memory\n", __func__);
		return -ENOMEM;
	}

	mutex_init(&dev->input_lock);

	dev->fmt_idx = 0;
	v4l2_i2c_subdev_init(&(dev->sd), client, &mt9e013_ops);

	if (client->dev.platform_data) {
		ret = mt9e013_s_config(&dev->sd, client->irq,
				       client->dev.platform_data);
		if (ret) {
			v4l2_device_unregister_subdev(&dev->sd);
			kfree(dev);
			return ret;
		}
	}

	dev->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	dev->pad.flags = MEDIA_PAD_FL_SOURCE;
	dev->sd.entity.ops = &mt9e013_entity_ops;
	dev->sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;
	dev->format.code = V4L2_MBUS_FMT_SGRBG10_1X10;

	ret = media_entity_init(&dev->sd.entity, 1, &dev->pad, 0);
	if (ret) {
		mt9e013_remove(client);
		return ret;
	}

	return 0;
}

static const struct i2c_device_id mt9e013_id[] = {
	{MT9E013_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, mt9e013_id);

static struct i2c_driver mt9e013_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = MT9E013_NAME,
	},
	.probe = mt9e013_probe,
	.remove = mt9e013_remove,
	.id_table = mt9e013_id,
};

static __init int init_mt9e013(void)
{
	return i2c_add_driver(&mt9e013_driver);
}

static __exit void exit_mt9e013(void)
{
	i2c_del_driver(&mt9e013_driver);
}

module_init(init_mt9e013);
module_exit(exit_mt9e013);

MODULE_DESCRIPTION("A low-level driver for Aptina MT9E013 sensors");
MODULE_LICENSE("GPL");
