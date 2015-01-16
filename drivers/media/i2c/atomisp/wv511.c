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
#include <media/v4l2-device.h>
#include <asm/intel-mid.h>

#include "wv511.h"

#define INIT_FOCUS_POS 350

//#define WV_DEBUG 1
#define wv511_debug dev_err

static struct wv511_device wv511_dev;
static int wv511_i2c_write(struct i2c_client *client, u16 data)
{
	struct i2c_msg msg;
	const int num_msg = 1;
	int ret;
	u16 val;
#ifdef WV_DEBUG
	//struct i2c_client *client = v4l2_get_subdevdata(sd);
	wv511_debug(&client->dev, "_wv511_: %s: wr %x\n",__func__,data);
#endif

	val = cpu_to_be16(data);
	msg.addr = wv511_VCM_ADDR;
	msg.flags = 0;
	msg.len = wv511_16BIT;
	msg.buf = (u8 *)&val;

	ret = i2c_transfer(client->adapter, &msg, 1);


	return ret == num_msg ? 0 : -EIO;
}

int wv511_t_focus_vcm(struct v4l2_subdev *sd, u16 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = -EINVAL;
	u16 dac=0,s=0;

#ifdef WV_DEBUG
	wv511_debug(&client->dev, "_wv511_: %s: dec_value = %d slew =%x\n",__func__,val,wv511_dev.vcm_settings.slew_rate_setting);
#endif
	wv511_dev.vcm_settings.dac_code = (val & wv511_MAX_FOCUS_POS);
	dac = wv511_dev.vcm_settings.dac_code;
	s = wv511_dev.vcm_settings.slew_rate_setting;
	switch (wv511_dev.vcm_mode)
	{
	case wv511_DIRECT:
		ret = wv511_i2c_write(client,vcm_val(dac, VCM_DEFAULT_S));
		break;
	case wv511_LSC:
		ret = wv511_i2c_write(client, vcm_val(dac, s));
		break;
	}
	return ret;
}

int wv511_t_focus_abs(struct v4l2_subdev *sd, s32 value)
{
	int ret;

	value = min(value, wv511_MAX_FOCUS_POS);
	ret = wv511_t_focus_vcm(sd, wv511_MAX_FOCUS_POS - value);
	if (ret == 0) {
		wv511_dev.number_of_steps = value - wv511_dev.focus;
		wv511_dev.focus = value;
		getnstimeofday(&(wv511_dev.timestamp_t_focus_abs));
	}

	return ret;
}

int wv511_t_focus_rel(struct v4l2_subdev *sd, s32 value)
{
#ifdef WV_DEBUG
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	wv511_debug(&client->dev, "_wv511_: %s: wv511_t_focus_rel\n",__func__);
#endif

	return wv511_t_focus_abs(sd, wv511_dev.focus + value);
}

int wv511_q_focus_status(struct v4l2_subdev *sd, s32 *value)
{
	u32 status = 0;
	struct timespec temptime;
	const struct timespec timedelay = {
		0,
		min_t(u32, abs(wv511_dev.number_of_steps)*DELAY_PER_STEP_NS,
			DELAY_MAX_PER_STEP_NS),
	};

	ktime_get_ts(&temptime);

	temptime = timespec_sub(temptime, (wv511_dev.timestamp_t_focus_abs));

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

int wv511_q_focus_abs(struct v4l2_subdev *sd, s32 *value)
{
	s32 val;

	wv511_q_focus_status(sd, &val);

	if (val & ATOMISP_FOCUS_STATUS_MOVING)
		*value  = wv511_dev.focus - wv511_dev.number_of_steps;
	else
		*value  = wv511_dev.focus ;

	return 0;
}

int wv511_t_vcm_slew(struct v4l2_subdev *sd, s32 value)
{
	wv511_dev.vcm_settings.slew_rate_setting = value & 0xf;
	return 0;
}

int wv511_t_vcm_timing(struct v4l2_subdev *sd, s32 value)
{
#ifdef WV_DEBUG
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	wv511_debug(&client->dev, "_wv511_: nothing to do with %s: \n",__func__);
#endif
	return 0;
}

int wv511_vcm_init(struct v4l2_subdev *sd)
{
#ifdef WV_DEBUG
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	wv511_debug(&client->dev, "_wv511_: %s: wv511_vcm_init\n",__func__);
#endif

	/* set VCM to home position and vcm mode to direct*/
	wv511_dev.vcm_mode = wv511_DIRECT;
	wv511_dev.vcm_settings.slew_rate_setting = 0;
	wv511_dev.vcm_settings.dac_code = 0;

	return wv511_t_focus_abs(sd, INIT_FOCUS_POS);
}
