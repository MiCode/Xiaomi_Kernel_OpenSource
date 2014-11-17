/*
 * LED flash driver for LM3642
 *
 * Copyright (c) 2010-2014 Intel Corporation. All Rights Reserved.
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
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/slab.h>

#include <media/lm3642.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>

#include <linux/atomisp.h>

struct lm3642_ctrl_id {
	struct v4l2_queryctrl qc;
	int (*s_ctrl) (struct v4l2_subdev *sd, __u32 val);
	int (*g_ctrl) (struct v4l2_subdev *sd, __s32 *val);
};

/* Registers */
#define LM3642_CONTROL_REG		0x09
#define LM3642_FLASH_CURRENT_SHIFT	0
#define LM3642_TORCH_CURRENT_SHIFT	4

#define LM3642_FEATURE_REG	0x08
#define LM3642_FLASH_TIMEOUT_SHIFT	0
#define LM3642_RAMP_SHIFT		3
#define LM3642_INDICATOR_CURRENT_SHIFT	6

#define LM3642_FLAGS_REG		0x0b
#define LM3642_FLAG_TIMEOUT		(1 << 0)
#define LM3642_FLAG_THERMAL_SHUTDOWN	(1 << 1)
#define LM3642_FLAG_LED_FAULT		(1 << 2)
#define LM3642_FLAG_OVP			(1 << 3)
#define LM3642_FLAG_UVLO		(1 << 4)
#define LM3642_FLAG_IVFM		(1 << 5)
#define LM3642_FLAG_UNUSED		(3 << 6)

#define LM3642_ENABLE_REG		0x0a
#define LM3642_MODE_SHIFT		0
#define LM3642_TORCH_EN_SHIFT		4
#define LM3642_FLASH_EN_SHIFT		5
#define LM3642_TX_EN_SHIFT		5
#define LM3642_IVFM_EN_SHIFT		7

struct lm3642 {
	struct v4l2_subdev sd;

	struct mutex power_lock;
	int power_count;

	unsigned int mode;
	int timeout;
	int ramp_time;
	u8 torch_current;
	u8 indicator_current;
	u8 flash_current;

	struct timer_list flash_off_delay;
	struct lm3642_platform_data *pdata;
};

#define to_lm3642(p_sd)	container_of(p_sd, struct lm3642, sd)

/* Return negative errno else zero on success */
static int lm3642_write(struct lm3642 *flash, u8 addr, u8 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&flash->sd);
	int ret;

	ret = i2c_smbus_write_byte_data(client, addr, val);

	dev_dbg(&client->dev, "Write Addr:%02X Val:%02X %s\n", addr, val,
		ret < 0 ? "fail" : "ok");

	return ret;
}

/* Return negative errno else a data byte received from the device. */
static int lm3642_read(struct lm3642 *flash, u8 addr)
{
	struct i2c_client *client = v4l2_get_subdevdata(&flash->sd);
	int ret;

	ret = i2c_smbus_read_byte_data(client, addr);

	dev_dbg(&client->dev, "Read Addr:%02X Val:%02X %s\n", addr, ret,
		ret < 0 ? "fail" : "ok");

	return ret;
}

/* -----------------------------------------------------------------------------
 * Hardware configuration
 */

static int lm3642_set_mode(struct lm3642 *flash, unsigned int mode)
{
	u8 val;
	int ret;

	val = (mode << LM3642_MODE_SHIFT) |
	      (flash->pdata->torch_en << LM3642_TORCH_EN_SHIFT) |
	      (flash->pdata->flash_en << LM3642_TORCH_EN_SHIFT) |
	      (flash->pdata->tx_en << LM3642_TX_EN_SHIFT) |
	      (flash->pdata->ivfm_en << LM3642_IVFM_EN_SHIFT);

	ret = lm3642_write(flash, LM3642_ENABLE_REG, val);
	if (ret == 0)
		flash->mode = mode;
	return ret;
}

static int lm3642_set_current(struct lm3642 *flash)
{
	u8 val;

	val = (flash->torch_current << LM3642_TORCH_CURRENT_SHIFT) |
	      (flash->flash_current << LM3642_FLASH_CURRENT_SHIFT);

	return lm3642_write(flash, LM3642_CONTROL_REG, val);
}

static int lm3642_set_feature(struct lm3642 *flash)
{
	u8 val;

	val = (flash->timeout << LM3642_FLASH_TIMEOUT_SHIFT) |
	      (flash->ramp_time << LM3642_RAMP_SHIFT) |
	      (flash->indicator_current << LM3642_INDICATOR_CURRENT_SHIFT);
	return lm3642_write(flash, LM3642_FEATURE_REG, val);
}

static int lm3642_set_enable(struct lm3642 *flash)
{
	u8 val;
	val = (flash->mode << LM3642_MODE_SHIFT) |
	      (flash->pdata->torch_en << LM3642_TORCH_EN_SHIFT) |
	      (flash->pdata->flash_en << LM3642_FLASH_EN_SHIFT) |
	      (flash->pdata->tx_en << LM3642_TX_EN_SHIFT) |
	      (flash->pdata->ivfm_en << LM3642_IVFM_EN_SHIFT);
	return lm3642_write(flash, LM3642_ENABLE_REG, val);
}

static void lm3642_flash_off_delay(long unsigned int arg)
{
	struct v4l2_subdev *sd = i2c_get_clientdata((struct i2c_client *)arg);
	struct lm3642 *flash = to_lm3642(sd);
	struct lm3642_platform_data *pdata = flash->pdata;

	gpio_set_value(pdata->gpio_strobe, 0);
}

static int lm3642_hw_strobe(struct i2c_client *client, bool strobe)
{
	int ret, timer_pending;
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct lm3642 *flash = to_lm3642(sd);
	struct lm3642_platform_data *pdata = flash->pdata;

	/*
	 * An abnormal high flash current is observed when strobe off the
	 * flash. Workaround here is firstly set flash current to lower level,
	 * wait a short moment, and then strobe off the flash.
	 */

	timer_pending = del_timer_sync(&flash->flash_off_delay);

	/* Flash off */
	if (!strobe) {
		/* set current to 70mA and wait a while */
		ret = lm3642_write(flash, LM3642_CONTROL_REG, 0);
		if (ret < 0)
			goto err;
		mod_timer(&flash->flash_off_delay,
			  jiffies + msecs_to_jiffies(LM3642_TIMER_DELAY));
		return 0;
	}

	/* Flash on */

	/*
	 * If timer is killed before run, flash is not strobe off,
	 * so must strobe off here
	 */
	if (timer_pending)
		gpio_set_value(pdata->gpio_strobe, 0);

	/* Restore flash current settings */
	ret = lm3642_set_current(flash);
	if (ret < 0)
		goto err;

	/* Strobe on Flash */
	gpio_set_value(pdata->gpio_strobe, 1);

	return 0;
err:
	dev_err(&client->dev, "failed to %s flash strobe (%d)\n",
		strobe ? "on" : "off", ret);
	return ret;
}

/* -----------------------------------------------------------------------------
 * V4L2 controls
 */

static int lm3642_read_status(struct lm3642 *flash)
{
	int ret;
	struct i2c_client *client = v4l2_get_subdevdata(&flash->sd);

	/* NOTE: reading register clear fault status */
	ret = lm3642_read(flash, LM3642_FLAGS_REG);
	if (ret < 0)
		return ret;

	/*
	 * Accordingly to datasheet we read back '1' in bit 6.
	 * Clear it first.
	 */
	ret &= ~LM3642_FLAG_UNUSED;
	if (ret > 0)
		dev_dbg(&client->dev, "LM3642 flag status: %02x\n", ret);

	return ret;
}

static int lm3642_s_flash_timeout(struct v4l2_subdev *sd, u32 val)
{
	struct lm3642 *flash = to_lm3642(sd);

	val = clamp(val, LM3642_MIN_TIMEOUT, LM3642_MAX_TIMEOUT);
	val = val / LM3642_TIMEOUT_STEPSIZE - 1;

	flash->timeout = val;

	return lm3642_set_feature(flash);
}

static int lm3642_g_flash_timeout(struct v4l2_subdev *sd, s32 *val)
{
	struct lm3642 *flash = to_lm3642(sd);

	*val = (u32)(flash->timeout + 1) * LM3642_TIMEOUT_STEPSIZE;

	return 0;
}

static int lm3642_s_flash_intensity(struct v4l2_subdev *sd, u32 intensity)
{
	struct lm3642 *flash = to_lm3642(sd);

	intensity = LM3642_CLAMP_PERCENTAGE(intensity);
	intensity = LM3642_PERCENT_TO_VALUE(intensity, LM3642_FLASH_STEP);

	flash->flash_current = intensity;

	return lm3642_set_current(flash);
}

static int lm3642_g_flash_intensity(struct v4l2_subdev *sd, s32 *val)
{
	struct lm3642 *flash = to_lm3642(sd);

	*val = LM3642_VALUE_TO_PERCENT((u32)flash->flash_current,
			LM3642_FLASH_STEP);

	return 0;
}

static int lm3642_s_torch_intensity(struct v4l2_subdev *sd, u32 intensity)
{
	struct lm3642 *flash = to_lm3642(sd);

	intensity = LM3642_CLAMP_PERCENTAGE(intensity);
	intensity = LM3642_PERCENT_TO_VALUE(intensity, LM3642_TORCH_STEP);

	flash->torch_current = intensity;

	return lm3642_set_current(flash);
}

static int lm3642_g_torch_intensity(struct v4l2_subdev *sd, s32 *val)
{
	struct lm3642 *flash = to_lm3642(sd);

	*val = LM3642_VALUE_TO_PERCENT((u32)flash->torch_current,
			LM3642_TORCH_STEP);

	return 0;
}

static int lm3642_s_indicator_intensity(struct v4l2_subdev *sd, u32 intensity)
{
	struct lm3642 *flash = to_lm3642(sd);

	intensity = LM3642_CLAMP_PERCENTAGE(intensity);
	intensity = LM3642_PERCENT_TO_VALUE(intensity, LM3642_INDICATOR_STEP);

	flash->indicator_current = intensity;

	return lm3642_set_feature(flash);
}

static int lm3642_g_indicator_intensity(struct v4l2_subdev *sd, s32 *val)
{
	struct lm3642 *flash = to_lm3642(sd);

	*val = LM3642_VALUE_TO_PERCENT((u32)flash->indicator_current,
			LM3642_INDICATOR_STEP);

	return 0;
}

static int lm3642_s_flash_strobe(struct v4l2_subdev *sd, u32 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return lm3642_hw_strobe(client, val);
}

static int lm3642_s_flash_mode(struct v4l2_subdev *sd, u32 new_mode)
{
	struct lm3642 *flash = to_lm3642(sd);
	unsigned int mode;

	switch (new_mode) {
	case ATOMISP_FLASH_MODE_OFF:
		mode = LM3642_MODE_SHUTDOWN;
		break;
	case ATOMISP_FLASH_MODE_FLASH:
		mode = LM3642_MODE_FLASH;
		break;
	case ATOMISP_FLASH_MODE_INDICATOR:
		mode = LM3642_MODE_INDICATOR;
		break;
	case ATOMISP_FLASH_MODE_TORCH:
		mode = LM3642_MODE_TORCH;
		break;
	default:
		return -EINVAL;
	}

	return lm3642_set_mode(flash, mode);
}

static int lm3642_g_flash_mode(struct v4l2_subdev *sd, s32 * val)
{
	struct lm3642 *flash = to_lm3642(sd);
	*val = flash->mode;
	return 0;
}

static int lm3642_g_flash_status(struct v4l2_subdev *sd, s32 *val)
{
	struct lm3642 *flash = to_lm3642(sd);
	int value;

	value = lm3642_read_status(flash);
	if (value < 0)
		return value;

	if (value & LM3642_FLAG_TIMEOUT)
		*val = ATOMISP_FLASH_STATUS_TIMEOUT;
	else if (value > 0)
		*val = ATOMISP_FLASH_STATUS_HW_ERROR;
	else
		*val = ATOMISP_FLASH_STATUS_OK;

	return 0;
}

static const struct lm3642_ctrl_id lm3642_ctrls[] = {
	s_ctrl_id_entry_integer(V4L2_CID_FLASH_TIMEOUT,
				"Flash Timeout",
				0,
				LM3642_MAX_TIMEOUT,
				1,
				LM3642_DEFAULT_TIMEOUT,
				0,
				lm3642_s_flash_timeout,
				lm3642_g_flash_timeout),
	s_ctrl_id_entry_integer(V4L2_CID_FLASH_INTENSITY,
				"Flash Intensity",
				LM3642_MIN_PERCENT,
				LM3642_MAX_PERCENT,
				1,
				LM3642_FLASH_DEFAULT_BRIGHTNESS,
				0,
				lm3642_s_flash_intensity,
				lm3642_g_flash_intensity),
	s_ctrl_id_entry_integer(V4L2_CID_FLASH_TORCH_INTENSITY,
				"Torch Intensity",
				LM3642_MIN_PERCENT,
				LM3642_MAX_PERCENT,
				1,
				LM3642_TORCH_DEFAULT_BRIGHTNESS,
				0,
				lm3642_s_torch_intensity,
				lm3642_g_torch_intensity),
	s_ctrl_id_entry_integer(V4L2_CID_FLASH_INDICATOR_INTENSITY,
				"Indicator Intensity",
				LM3642_MIN_PERCENT,
				LM3642_MAX_PERCENT,
				1,
				LM3642_INDICATOR_DEFAULT_BRIGHTNESS,
				0,
				lm3642_s_indicator_intensity,
				lm3642_g_indicator_intensity),
	s_ctrl_id_entry_boolean(V4L2_CID_FLASH_STROBE,
				"Flash Strobe",
				0,
				0,
				lm3642_s_flash_strobe,
				NULL),
	s_ctrl_id_entry_integer(V4L2_CID_FLASH_MODE,
				"Flash Mode",
				0,   /* don't assume any enum ID is first */
				100, /* enum value, may get extended */
				1,
				ATOMISP_FLASH_MODE_OFF,
				0,
				lm3642_s_flash_mode,
				lm3642_g_flash_mode),
	s_ctrl_id_entry_integer(V4L2_CID_FLASH_STATUS,
				"Flash Status",
				0,   /* don't assume any enum ID is first */
				100, /* enum value, may get extended */
				1,
				ATOMISP_FLASH_STATUS_OK,
				0,
				NULL,
				lm3642_g_flash_status),
};

static const struct lm3642_ctrl_id *find_ctrl_id(unsigned int id)
{
	int i;
	int num;

	num = ARRAY_SIZE(lm3642_ctrls);
	for (i = 0; i < num; i++) {
		if (lm3642_ctrls[i].qc.id == id)
			return &lm3642_ctrls[i];
	}

	return NULL;
}

static int lm3642_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
	int num;

	if (!qc)
		return -EINVAL;

	num = ARRAY_SIZE(lm3642_ctrls);
	if (qc->id >= num)
		return -EINVAL;

	*qc = lm3642_ctrls[qc->id].qc;

	return 0;
}

static int lm3642_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	const struct lm3642_ctrl_id *s_ctrl;

	if (!ctrl)
		return -EINVAL;

	s_ctrl = find_ctrl_id(ctrl->id);
	if (!s_ctrl)
		return -EINVAL;

	return s_ctrl->s_ctrl(sd, ctrl->value);
}

static int lm3642_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	const struct lm3642_ctrl_id *s_ctrl;

	if (!ctrl)
		return -EINVAL;

	s_ctrl = find_ctrl_id(ctrl->id);
	if (s_ctrl == NULL)
		return -EINVAL;

	return s_ctrl->g_ctrl(sd, &ctrl->value);
}

/* -----------------------------------------------------------------------------
 * V4L2 subdev core operations
 */

/* Put device into known state. */
static int lm3642_setup(struct lm3642 *flash)
{
	struct i2c_client *client = v4l2_get_subdevdata(&flash->sd);
	int ret;

	/* clear the flags register */
	ret = lm3642_read(flash, LM3642_FLAGS_REG);
	if (ret < 0)
		return ret;

	dev_dbg(&client->dev, "Fault info: %02x\n", ret);

	ret = lm3642_set_enable(flash);
	if (ret < 0)
		return ret;

	ret = lm3642_set_feature(flash);
	if (ret < 0)
		return ret;

	ret = lm3642_set_current(flash);
	if (ret < 0)
		return ret;

	/* read status */
	ret = lm3642_read_status(flash);
	if (ret < 0)
		return ret;

	return ret ? -EIO : 0;
}

static int __lm3642_s_power(struct lm3642 *flash, int power)
{
	return 0;
}

static int lm3642_s_power(struct v4l2_subdev *sd, int power)
{
	struct lm3642 *flash = to_lm3642(sd);
	int ret = 0;

	mutex_lock(&flash->power_lock);

	if (flash->power_count == !power) {
		ret = __lm3642_s_power(flash, !!power);
		if (ret < 0)
			goto done;
	}

	flash->power_count += power ? 1 : -1;
	WARN_ON(flash->power_count < 0);

done:
	mutex_unlock(&flash->power_lock);
	return ret;
}

static const struct v4l2_subdev_core_ops lm3642_core_ops = {
	.queryctrl = lm3642_queryctrl,
	.g_ctrl = lm3642_g_ctrl,
	.s_ctrl = lm3642_s_ctrl,
	.s_power = lm3642_s_power,
};

static const struct v4l2_subdev_ops lm3642_ops = {
	.core = &lm3642_core_ops,
};

static int lm3642_detect(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct i2c_adapter *adapter = client->adapter;
	struct lm3642 *flash = to_lm3642(sd);
	int ret;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&client->dev, "lm3642_detect i2c error\n");
		return -ENODEV;
	}

	/* Power up the flash driver and reset it */
	ret = lm3642_s_power(&flash->sd, 1);
	if (ret < 0)
		return ret;

	/* Setup default values. This makes sure that the chip is in a known
	 * state.
	 */
	ret = lm3642_setup(flash);
	if (ret < 0)
		goto fail;

	dev_dbg(&client->dev, "Successfully detected lm3642 LED flash\n");
	lm3642_s_power(&flash->sd, 0);
	return 0;

fail:
	lm3642_s_power(&flash->sd, 0);
	return ret;
}

static int lm3642_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	return lm3642_s_power(sd, 1);
}

static int lm3642_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	return lm3642_s_power(sd, 0);
}

static const struct v4l2_subdev_internal_ops lm3642_internal_ops = {
	.registered = lm3642_detect,
	.open = lm3642_open,
	.close = lm3642_close,
};

/* -----------------------------------------------------------------------------
 *  I2C driver
 */
#ifdef CONFIG_PM

static int lm3642_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	struct lm3642 *flash = to_lm3642(subdev);
	int rval;

	if (flash->power_count == 0)
		return 0;

	rval = __lm3642_s_power(flash, 0);

	dev_dbg(&client->dev, "Suspend %s\n", rval < 0 ? "failed" : "ok");

	return rval;
}

static int lm3642_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	struct lm3642 *flash = to_lm3642(subdev);
	int rval;

	if (flash->power_count == 0)
		return 0;

	rval = __lm3642_s_power(flash, 1);

	dev_dbg(&client->dev, "Resume %s\n", rval < 0 ? "fail" : "ok");

	return rval;
}

#else

#define lm3642_suspend NULL
#define lm3642_resume  NULL

#endif /* CONFIG_PM */

static int lm3642_gpio_init(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct lm3642 *flash = to_lm3642(sd);
	struct lm3642_platform_data *pdata = flash->pdata;
	int ret;

	ret = gpio_request(pdata->gpio_strobe, "flash");
	if (ret < 0)
		return ret;

	ret = gpio_direction_output(pdata->gpio_strobe, 0);
	if (ret < 0)
		goto err_gpio_flash;

	ret = gpio_request(pdata->gpio_torch, "torch");
	if (ret < 0)
		goto err_gpio_flash;

	ret = gpio_direction_output(pdata->gpio_torch, 0);
	if (ret < 0)
		goto err_gpio_torch;

	return 0;

err_gpio_torch:
	gpio_free(pdata->gpio_torch);
err_gpio_flash:
	gpio_free(pdata->gpio_strobe);
	return ret;
}

static int lm3642_gpio_uninit(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct lm3642 *flash = to_lm3642(sd);
	struct lm3642_platform_data *pdata = flash->pdata;
	int ret;

	ret = gpio_direction_output(pdata->gpio_torch, 0);
	if (ret < 0)
		return ret;

	ret = gpio_direction_output(pdata->gpio_strobe, 0);
	if (ret < 0)
		return ret;

	gpio_free(pdata->gpio_torch);

	gpio_free(pdata->gpio_strobe);

	return 0;
}

static int lm3642_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	int err;
	struct lm3642 *flash;

	if (client->dev.platform_data == NULL) {
		dev_err(&client->dev, "no platform data\n");
		return -ENODEV;
	}

	flash = kzalloc(sizeof(*flash), GFP_KERNEL);
	if (!flash) {
		dev_err(&client->dev, "out of memory\n");
		return -ENOMEM;
	}

	flash->pdata = client->dev.platform_data;

	v4l2_i2c_subdev_init(&flash->sd, client, &lm3642_ops);
	flash->sd.internal_ops = &lm3642_internal_ops;
	flash->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	flash->mode = ATOMISP_FLASH_MODE_OFF;
	flash->timeout = LM3642_DEFAULT_TIMEOUT;
	flash->ramp_time = LM3642_DEFAULT_RAMP_TIME;
	flash->indicator_current = LM3642_DEFAULT_INDICATOR_CURRENT;
	flash->flash_current = LM3642_DEFAULT_FLASH_CURRENT;

	err = media_entity_init(&flash->sd.entity, 0, NULL, 0);
	if (err) {
		dev_err(&client->dev, "error initialize a media entity.\n");
		goto fail1;
	}

	flash->sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV_FLASH;

	mutex_init(&flash->power_lock);

	setup_timer(&flash->flash_off_delay, lm3642_flash_off_delay,
		    (unsigned long)client);

	err = lm3642_gpio_init(client);
	if (err) {
		dev_err(&client->dev, "gpio request/direction_output fail");
		goto fail2;
	}

	return 0;
fail2:
	media_entity_cleanup(&flash->sd.entity);
fail1:
	v4l2_device_unregister_subdev(&flash->sd);
	kfree(flash);

	return err;
}

static int lm3642_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct lm3642 *flash = to_lm3642(sd);
	int ret;

	media_entity_cleanup(&flash->sd.entity);
	v4l2_device_unregister_subdev(sd);

	del_timer_sync(&flash->flash_off_delay);

	ret = lm3642_gpio_uninit(client);
	if (ret < 0)
		goto fail;

	kfree(flash);

	return 0;
fail:
	dev_err(&client->dev, "gpio request/direction_output fail");
	return ret;
}

static const struct i2c_device_id lm3642_id[] = {
	{LM3642_NAME, 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, lm3642_id);

static const struct dev_pm_ops lm3642_pm_ops = {
	.suspend = lm3642_suspend,
	.resume = lm3642_resume,
};

static struct i2c_driver lm3642_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = LM3642_NAME,
		.pm   = &lm3642_pm_ops,
	},
	.probe = lm3642_probe,
	.remove = lm3642_remove,
	.id_table = lm3642_id,
};

static __init int init_lm3642(void)
{
	return i2c_add_driver(&lm3642_driver);
}

static __exit void exit_lm3642(void)
{
	i2c_del_driver(&lm3642_driver);
}

module_init(init_lm3642);
module_exit(exit_lm3642);
MODULE_AUTHOR("Shuguang Gong <Shuguang.Gong@intel.com>");
MODULE_DESCRIPTION("LED flash driver for LM3642");
MODULE_LICENSE("GPL");
