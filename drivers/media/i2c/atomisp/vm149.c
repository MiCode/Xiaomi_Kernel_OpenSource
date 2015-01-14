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
#include <linux/mm.h>
#include <linux/kmod.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-device.h>
#include <asm/intel-mid.h>

#include "vm149.h"

static struct vm149_device vm149_dev;
static int vm149_i2c_write(struct i2c_client *client, u16 data)
{
	struct i2c_msg msg;
	const int num_msg = 1;
	int ret;
	u16 val;

	val = cpu_to_be16(data);
	msg.addr = VM149_VCM_ADDR;
	msg.flags = 0;
	msg.len = VM149_16BIT;
	msg.buf = (u8 *)&val;

	ret = i2c_transfer(client->adapter, &msg, 1);

	return ret == num_msg ? 0 : -EIO;
}

int vm149_vcm_power_up(struct v4l2_subdev *sd)
{
	int ret;

	/* Enable power */
	ret = vm149_dev.platform_data->power_ctrl(sd, 1);
	/* TODO:waiting time requested by VM149A(vcm) */
	usleep_range(12000, 12500);
	return ret;
}

int vm149_vcm_power_down(struct v4l2_subdev *sd)
{
	return vm149_dev.platform_data->power_ctrl(sd, 0);
}


int vm149_t_focus_vcm(struct v4l2_subdev *sd, u16 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = -EINVAL;
	u8 s = vm149_vcm_step_s(vm149_dev.vcm_settings.step_setting);

	ret = vm149_i2c_write(client,
				vm149_vcm_val(val, s));
	return ret;
}

int vm149_t_focus_abs(struct v4l2_subdev *sd, s32 value)
{
	int ret;

	value = min(value, VM149_MAX_FOCUS_POS);
	ret = vm149_t_focus_vcm(sd, VM149_MAX_FOCUS_POS - value);
	if (ret == 0) {
		vm149_dev.number_of_steps = value - vm149_dev.focus;
		vm149_dev.focus = value;
		getnstimeofday(&(vm149_dev.timestamp_t_focus_abs));
	}

	return ret;
}

int vm149_t_focus_rel(struct v4l2_subdev *sd, s32 value)
{

	return vm149_t_focus_abs(sd, vm149_dev.focus + value);
}

int vm149_q_focus_status(struct v4l2_subdev *sd, s32 *value)
{
	u32 status = 0;
	struct timespec temptime;
	const struct timespec timedelay = {
		0,
		min_t(u32, abs(vm149_dev.number_of_steps)*DELAY_PER_STEP_NS,
			DELAY_MAX_PER_STEP_NS),
	};

	ktime_get_ts(&temptime);

	temptime = timespec_sub(temptime, (vm149_dev.timestamp_t_focus_abs));

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

int vm149_q_focus_abs(struct v4l2_subdev *sd, s32 *value)
{
	s32 val;

	vm149_q_focus_status(sd, &val);

	if (val & ATOMISP_FOCUS_STATUS_MOVING)
		*value  = vm149_dev.focus - vm149_dev.number_of_steps;
	else
		*value  = vm149_dev.focus ;

	return 0;
}

int vm149_t_vcm_slew(struct v4l2_subdev *sd, s32 value)
{
	vm149_dev.vcm_settings.step_setting = value;

	return 0;
}

int vm149_t_vcm_timing(struct v4l2_subdev *sd, s32 value)
{

	return 0;
}

int vm149_vcm_init(struct v4l2_subdev *sd)
{

	/* set VCM to home position and vcm mode to direct*/
	vm149_dev.platform_data = camera_get_af_platform_data();
	return (NULL == vm149_dev.platform_data) ? -ENODEV : 0;

}

