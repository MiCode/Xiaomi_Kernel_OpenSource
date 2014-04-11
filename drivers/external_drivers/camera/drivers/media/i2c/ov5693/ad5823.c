#include <asm/intel-mid.h>
#include <linux/bitops.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/kmod.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-device.h>

#include "ad5823.h"

static struct ad5823_device ad5823_dev;
static int ad5823_i2c_write(struct i2c_client *client, u8 reg, u8 val)
{
	struct i2c_msg msg;
	u8 buf[2];
	buf[0] = reg;
	buf[1] = val;
	msg.addr = AD5823_VCM_ADDR;
	msg.flags = 0;
	msg.len = AD5823_REG_LENGTH + AD5823_8BIT;
	msg.buf = &buf[0];

	if (i2c_transfer(client->adapter, &msg, 1) != 1)
		return -EIO;
	return 0;
}

static int ad5823_i2c_read(struct i2c_client *client, u8 reg, u8 *val)
{
	struct i2c_msg msg[2];
	u8 buf[2];
	buf[0] = reg;
	buf[1] = 0;

	msg[0].addr = AD5823_VCM_ADDR;
	msg[0].flags = 0;
	msg[0].len = AD5823_REG_LENGTH;
	msg[0].buf = &buf[0];

	msg[1].addr = AD5823_VCM_ADDR;
	msg[1].flags = I2C_M_RD;
	msg[1].len = AD5823_8BIT;
	msg[1].buf = &buf[1];
	*val = 0;
	if (i2c_transfer(client->adapter, msg, 2) != 2)
		return -EIO;
	*val = buf[1];
	return 0;
}

int ad5823_vcm_power_up(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = -EINVAL;
	u8 vcm_mode_reg_val[4] = {
		AD5823_ARC_RES0,
		AD5823_ARC_RES1,
		AD5823_ARC_RES2,
		AD5823_ESRC
	};

	/* Enable power */
	if (ad5823_dev.platform_data) {
		ret = ad5823_dev.platform_data->power_ctrl(sd, 1);
		if (ret)
			return ret;
	}
	/*
	 * waiting time requested by AD5823(vcm)
	 */
	usleep_range(1000, 2000);

	/*
	 * Set vcm ringing control mode.
	 */
	if (ad5823_dev.vcm_mode != AD5823_DIRECT) {
		ret = ad5823_i2c_write(client, AD5823_REG_VCM_CODE_MSB,
						AD5823_RING_CTRL_ENABLE);
		if (ret)
			return ret;

		ret = ad5823_i2c_write(client, AD5823_REG_MODE,
					vcm_mode_reg_val[ad5823_dev.vcm_mode]);
		if (ret)
			return ret;
	} else {
		ret = ad5823_i2c_write(client, AD5823_REG_VCM_CODE_MSB,
						AD5823_RING_CTRL_DISABLE);
		if (ret)
			return ret;
	}

	return ret;
}

int ad5823_vcm_power_down(struct v4l2_subdev *sd)
{
	int ret = -ENODEV;

	if (ad5823_dev.platform_data)
		ret = ad5823_dev.platform_data->power_ctrl(sd, 0);

	return ret;
}


int ad5823_t_focus_vcm(struct v4l2_subdev *sd, u16 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = -EINVAL;
	u8 vcm_code;

	ret = ad5823_i2c_read(client, AD5823_REG_VCM_CODE_MSB, &vcm_code);
	if (ret)
		return ret;

	/* set reg VCM_CODE_MSB Bit[1:0] */
	vcm_code = (vcm_code & VCM_CODE_MSB_MASK) | ((val >> 8) & ~VCM_CODE_MSB_MASK);
	ret = ad5823_i2c_write(client, AD5823_REG_VCM_CODE_MSB, vcm_code);
	if (ret)
		return ret;

	/* set reg VCM_CODE_LSB Bit[7:0] */
	ret = ad5823_i2c_write(client, AD5823_REG_VCM_CODE_LSB, (val & 0xff));
	if (ret)
		return ret;

	/* set required vcm move time */
	vcm_code = AD5823_RESONANCE_PERIOD / AD5823_RESONANCE_COEF
		   - AD5823_HIGH_FREQ_RANGE;
	ret = ad5823_i2c_write(client, AD5823_REG_VCM_MOVE_TIME, vcm_code);

	return ret;
}

int ad5823_t_focus_abs(struct v4l2_subdev *sd, s32 value)
{
	int ret;

	value = min(value, AD5823_MAX_FOCUS_POS);
	ret = ad5823_t_focus_vcm(sd, AD5823_MAX_FOCUS_POS - value);
	if (ret == 0) {
		ad5823_dev.number_of_steps = value - ad5823_dev.focus;
		ad5823_dev.focus = value;
		ktime_get_ts(&ad5823_dev.timestamp_t_focus_abs);
	}

	return ret;
}

int ad5823_t_focus_rel(struct v4l2_subdev *sd, s32 value)
{
	return ad5823_t_focus_abs(sd, ad5823_dev.focus + value);
}

int ad5823_q_focus_status(struct v4l2_subdev *sd, s32 *value)
{
	u32 status = 0;
	struct timespec temptime;
	const struct timespec timedelay = {
		0,
		min_t(u32, abs(ad5823_dev.number_of_steps)*DELAY_PER_STEP_NS,
			DELAY_MAX_PER_STEP_NS),
	};

	ktime_get_ts(&temptime);

	temptime = timespec_sub(temptime, (ad5823_dev.timestamp_t_focus_abs));

	if (timespec_compare(&temptime, &timedelay) <= 0)
		status = ATOMISP_FOCUS_STATUS_MOVING
			| ATOMISP_FOCUS_HP_IN_PROGRESS;
	else
		status = ATOMISP_FOCUS_STATUS_ACCEPTS_NEW_MOVE
			| ATOMISP_FOCUS_HP_COMPLETE;

	*value = status;

	return 0;
}

int ad5823_q_focus_abs(struct v4l2_subdev *sd, s32 *value)
{
	s32 val;

	ad5823_q_focus_status(sd, &val);

	if (val & ATOMISP_FOCUS_STATUS_MOVING)
		*value  = ad5823_dev.focus - ad5823_dev.number_of_steps;
	else
		*value  = ad5823_dev.focus ;

	return 0;
}

int ad5823_t_vcm_slew(struct v4l2_subdev *sd, s32 value)
{
	return 0;
}

int ad5823_t_vcm_timing(struct v4l2_subdev *sd, s32 value)
{
	return 0;
}

int ad5823_vcm_init(struct v4l2_subdev *sd)
{
	/* set vcm mode to ARC RES0.5 */
	ad5823_dev.vcm_mode = AD5823_ARC_RES1;
	ad5823_dev.platform_data = camera_get_af_platform_data();
	return ad5823_dev.platform_data ? 0 : -ENODEV;
}

