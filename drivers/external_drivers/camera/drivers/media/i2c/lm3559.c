/*
 * LED flash driver for LM3559
 *
 * Copyright (c) 2010-2012 Intel Corporation. All Rights Reserved.
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

#include <media/lm3559.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>

#include <linux/atomisp.h>

struct lm3559_ctrl_id {
	struct v4l2_queryctrl qc;
	int (*s_ctrl) (struct v4l2_subdev *sd, __u32 val);
	int (*g_ctrl) (struct v4l2_subdev *sd, __s32 *val);
};

/* Registers */

#define LM3559_MODE_SHIFT		0
#define LM3559_MODE_MASK		0x3

#define LM3559_TORCH_BRIGHTNESS_REG	0xa0
#define LM3559_TORCH_LED1_CURRENT_SHIFT	0
#define LM3559_TORCH_LED2_CURRENT_SHIFT	3

#define LM3559_INDICATOR_PRIVACY_REG	0x11

#define LM3559_INDICATOR_BACK_ON	3
#define LM3559_INDICATOR_CURRENT_SHIFT	0
#define LM3559_INDICATOR_LED1_SHIFT	4
#define LM3559_INDICATOR_LED2_SHIFT	5
#define LM3559_INDICATOR_BLINKING_PERIOD_SHIFT	5

#define LM3559_INDICATOR_PWM_PERIOD_REG	0x14
#define LM3559_INDICATOR_PWM_PERIOD_SHIFT	0


#define LM3559_INDICATOR_CURRENT_SHIFT	0

#define LM3559_ENABLE_REG	0x10

#define LM3559_FLASH_BRIGHTNESS_REG	0xb0
#define LM3559_FLASH_LED1_CURRENT_SHIFT	0
#define LM3559_FLASH_LED2_CURRENT_SHIFT	4
#define LM3559_FLASH_MAX_CURRENT	15
#define LM3560_FLASH_MAX_CURRENT	13
#define LM3560_TORCH_MAX_CURRENT	2

#define LM3559_FLASH_DURATION_REG	0xc0
#define LM3559_FLASH_TIMEOUT_SHIFT	0
#define LM3559_CURRENT_LIMIT_SHIFT	5

#define LM3559_FLAGS_REG		0xd0
#define LM3559_FLAG_TIMEOUT		(1 << 0)
#define LM3559_FLAG_THERMAL_SHUTDOWN	(1 << 1)
#define LM3559_FLAG_LED_FAULT		(1 << 2)
#define LM3559_FLAG_TX1_INTERRUPT	(1 << 3)
#define LM3559_FLAG_TX2_INTERRUPT	(1 << 4)
#define LM3559_FLAG_LED_THERMAL_FAULT	(1 << 5)
#define LM3559_FLAG_INPUT_FLASH_VOLTAGE_LOW		(1 << 6)
#define LM3559_FLAG_INPUT_VOLTAGE_LOW	(1 << 7)

#define LM3559_CONFIG_REG_1		0xe0
#define LM3559_CONFIG_REG_2		0xf0

#define LM3559_CONFIG_REG_1_INIT_SETTING	0x6c
#define LM3559_CONFIG_REG_2_INIT_SETTING	0x01
#define LM3559_CONFIG_REG_2_INIT_SETTING_LM3560	0x11
#define LM3559_GPIO_REG_INIT_SETTING		0x00

#define LM3559_ENVM_TX2_SHIFT		0
#define LM3559_ENVM_TX2_MASK		0x01
#define LM3559_TX2_POLARITY_SHIFT	6
#define LM3559_TX2_POLARITY_MASK	0x40

#define LM3559_GPIO_REG			0x20
#define LM3559_GPIO_DISABLE_TX2_SHIFT	3
#define LM3559_GPIO_DISABLE_TX2_MASK	(1 << LM3559_GPIO_DISABLE_TX2_SHIFT)

enum lm3559_hw_type {
	lm3559_hw_type_lm3559,
	lm3559_hw_type_lm3560,
};

struct privacy_indicator {
	u8 indicator_current;
	u8 blinking_period;
	u8 pwm_period;
	u8 led1_enable;
	u8 led2_enable;
	u8 back_on;
};
struct lm3559 {
	struct v4l2_subdev sd;

	struct mutex power_lock;
	int power_count;

	unsigned int mode;
	int timeout;
	u8 torch_current;
	u8 indicator_current;
	u8 flash_current;

	struct privacy_indicator indicator;
	struct timer_list flash_off_delay;
	struct lm3559_platform_data *pdata;
	enum lm3559_hw_type hw_type;
};

#define to_lm3559(p_sd)	container_of(p_sd, struct lm3559, sd)

/* Return negative errno else zero on success */
static int lm3559_write(struct lm3559 *flash, u8 addr, u8 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&flash->sd);
	int ret;

	ret = i2c_smbus_write_byte_data(client, addr, val);

	dev_dbg(&client->dev, "Write Addr:%02X Val:%02X %s\n", addr, val,
		ret < 0 ? "fail" : "ok");

	return ret;
}

/* Return negative errno else a data byte received from the device. */
static int lm3559_read(struct lm3559 *flash, u8 addr)
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

static int lm3559_set_mode(struct lm3559 *flash, unsigned int mode)
{
	u8 val;
	int ret;

	val = lm3559_read(flash, LM3559_ENABLE_REG);
	val &= ~LM3559_MODE_MASK;
	val |= (mode & LM3559_MODE_MASK) << LM3559_MODE_SHIFT;

	ret = lm3559_write(flash, LM3559_ENABLE_REG, val);
	if (ret == 0)
		flash->mode = mode;
	return ret;
}

static int lm3559_set_indicator(struct lm3559 *flash)
{
	int ret;
	u8 val;

	/* Clear flags register. */
	lm3559_read(flash, LM3559_FLAGS_REG);
	val = (flash->indicator.indicator_current
			<< LM3559_INDICATOR_CURRENT_SHIFT) |
		(flash->indicator.led1_enable << LM3559_INDICATOR_LED1_SHIFT) |
		(flash->indicator.led2_enable << LM3559_INDICATOR_LED2_SHIFT) |
		(flash->indicator.back_on << LM3559_INDICATOR_BACK_ON) |
		(flash->indicator.blinking_period
			<< LM3559_INDICATOR_BLINKING_PERIOD_SHIFT);
	ret = lm3559_write(flash, LM3559_INDICATOR_PRIVACY_REG, val);
	if (ret)
		return ret;
	val = flash->indicator.pwm_period << LM3559_INDICATOR_PWM_PERIOD_SHIFT;
	ret = lm3559_write(flash, LM3559_INDICATOR_PWM_PERIOD_REG, val);

	return ret;
}
static int lm3559_set_torch(struct lm3559 *flash)
{
	u8 val;

	/* Clear flags register. */
	lm3559_read(flash, LM3559_FLAGS_REG);
	val = (flash->torch_current << LM3559_TORCH_LED1_CURRENT_SHIFT) |
	      (flash->torch_current << LM3559_TORCH_LED2_CURRENT_SHIFT);

	return lm3559_write(flash, LM3559_TORCH_BRIGHTNESS_REG, val);
}

static int lm3559_set_flash(struct lm3559 *flash)
{
	u8 val;

	/* Clear flags register. */
	lm3559_read(flash, LM3559_FLAGS_REG);
	val = (flash->flash_current << LM3559_FLASH_LED1_CURRENT_SHIFT) |
		(flash->flash_current << LM3559_FLASH_LED2_CURRENT_SHIFT);

	return lm3559_write(flash, LM3559_FLASH_BRIGHTNESS_REG, val);
}

static int lm3559_set_duration(struct lm3559 *flash)
{
	u8 val;

	val = (flash->timeout << LM3559_FLASH_TIMEOUT_SHIFT) |
	      (flash->pdata->current_limit << LM3559_CURRENT_LIMIT_SHIFT);

	return lm3559_write(flash, LM3559_FLASH_DURATION_REG, val);
}

static int lm3559_set_config(struct lm3559 *flash)
{
	int ret;
	u8 val;

	val = LM3559_CONFIG_REG_1_INIT_SETTING & ~LM3559_TX2_POLARITY_MASK;
	val |= flash->pdata->tx2_polarity << LM3559_TX2_POLARITY_SHIFT;
	ret = lm3559_write(flash, LM3559_CONFIG_REG_1, val);
	if (ret)
		return ret;

	if (flash->hw_type == lm3559_hw_type_lm3560)
		val = LM3559_CONFIG_REG_2_INIT_SETTING_LM3560 &
		      ~LM3559_ENVM_TX2_MASK;
	else
		val = LM3559_CONFIG_REG_2_INIT_SETTING & ~LM3559_ENVM_TX2_MASK;
	val |= flash->pdata->envm_tx2 << LM3559_ENVM_TX2_SHIFT;
	ret = lm3559_write(flash, LM3559_CONFIG_REG_2, val);
	if (ret)
		return ret;

	val = LM3559_GPIO_REG_INIT_SETTING & ~LM3559_GPIO_DISABLE_TX2_MASK;
	val |= flash->pdata->disable_tx2 << LM3559_GPIO_DISABLE_TX2_SHIFT;
	return lm3559_write(flash, LM3559_GPIO_REG, val);
}

/* -----------------------------------------------------------------------------
 * Hardware trigger
 */

static void lm3559_flash_off_delay(long unsigned int arg)
{
	struct v4l2_subdev *sd = i2c_get_clientdata((struct i2c_client *)arg);
	struct lm3559 *flash = to_lm3559(sd);
	struct lm3559_platform_data *pdata = flash->pdata;

	gpio_set_value(pdata->gpio_strobe, 0);
}

static int lm3559_hw_strobe(struct i2c_client *client, bool strobe)
{
	int ret, timer_pending;
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct lm3559 *flash = to_lm3559(sd);
	struct lm3559_platform_data *pdata = flash->pdata;

	/*
	 * An abnormal high flash current is observed on lm3554 when
	 * strobe off the flash. Workaround here is firstly set flash
	 * current to lower level, wait a short moment, and then strobe
	 * off the flash.
	 * FIXME: The same issue exists with lm3559?
	 */

	timer_pending = del_timer_sync(&flash->flash_off_delay);

	/* Flash off */
	if (!strobe) {
		/* set current to 70mA and wait a while */
		ret = lm3559_write(flash, LM3559_FLASH_BRIGHTNESS_REG, 0);
		if (ret < 0)
			goto err;
		mod_timer(&flash->flash_off_delay,
			  jiffies + msecs_to_jiffies(LM3559_TIMER_DELAY));
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
	ret = lm3559_set_flash(flash);
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

static int lm3559_read_status(struct lm3559 *flash)
{
	int ret;
	struct i2c_client *client = v4l2_get_subdevdata(&flash->sd);

	/* NOTE: reading register clear fault status */
	ret = lm3559_read(flash, LM3559_FLAGS_REG);
	if (ret < 0)
		return ret;

	/*
	 * Do not take TX1/TX2 signal as an error
	 * because MSIC will not turn off flash, but turn to
	 * torch mode according to gsm modem signal by hardware.
	 */
	ret &= ~(LM3559_FLAG_TX1_INTERRUPT | LM3559_FLAG_TX2_INTERRUPT);

	if (ret > 0)
		dev_dbg(&client->dev, "LM3559 flag status: %02x\n", ret);

	return ret;
}

static int lm3559_s_flash_timeout(struct v4l2_subdev *sd, u32 val)
{
	struct lm3559 *flash = to_lm3559(sd);

	val = clamp(val, LM3559_MIN_TIMEOUT, LM3559_MAX_TIMEOUT);
	val = val / LM3559_TIMEOUT_STEPSIZE - 1;

	flash->timeout = val;

	return lm3559_set_duration(flash);
}

static int lm3559_g_flash_timeout(struct v4l2_subdev *sd, s32 *val)
{
	struct lm3559 *flash = to_lm3559(sd);

	*val = (u32)(flash->timeout + 1) * LM3559_TIMEOUT_STEPSIZE;

	return 0;
}

static int lm3559_s_flash_intensity(struct v4l2_subdev *sd, u32 intensity)
{
	struct lm3559 *flash = to_lm3559(sd);
	unsigned int limit = flash->pdata->flash_current_limit;

	if (limit == 0)
		limit = LM3559_FLASH_MAX_CURRENT;

	if (flash->hw_type == lm3559_hw_type_lm3560 &&
	    limit > LM3560_FLASH_MAX_CURRENT)
		limit = LM3560_FLASH_MAX_CURRENT;

	intensity = LM3559_CLAMP_PERCENTAGE(intensity);
	intensity = intensity * limit / LM3559_MAX_PERCENT;
	flash->flash_current = intensity;

	return lm3559_set_flash(flash);
}

static int lm3559_g_flash_intensity(struct v4l2_subdev *sd, s32 *val)
{
	struct lm3559 *flash = to_lm3559(sd);
	unsigned int limit = flash->pdata->flash_current_limit;

	if (limit == 0)
		limit = LM3559_FLASH_MAX_CURRENT;

	if (flash->hw_type == lm3559_hw_type_lm3560 &&
	    limit > LM3560_FLASH_MAX_CURRENT)
		limit = LM3560_FLASH_MAX_CURRENT;

	*val = flash->flash_current * LM3559_MAX_PERCENT / limit;

	return 0;
}

static int lm3559_s_torch_intensity(struct v4l2_subdev *sd, u32 intensity)
{
	struct lm3559 *flash = to_lm3559(sd);

	intensity = LM3559_CLAMP_PERCENTAGE(intensity);
	intensity = LM3559_PERCENT_TO_VALUE(intensity, LM3559_TORCH_STEP);

	if (flash->hw_type == lm3559_hw_type_lm3560 &&
	   intensity > LM3560_TORCH_MAX_CURRENT)
		intensity = LM3560_TORCH_MAX_CURRENT;

	flash->torch_current = intensity;

	return lm3559_set_torch(flash);
}

static int lm3559_g_torch_intensity(struct v4l2_subdev *sd, s32 *val)
{
	struct lm3559 *flash = to_lm3559(sd);

	*val = LM3559_VALUE_TO_PERCENT((u32)flash->torch_current,
			LM3559_TORCH_STEP);

	return 0;
}

static int lm3559_s_indicator_intensity(struct v4l2_subdev *sd, u32 intensity)
{
	struct lm3559 *flash = to_lm3559(sd);

	intensity = LM3559_CLAMP_PERCENTAGE(intensity);
	intensity = LM3559_PERCENT_TO_VALUE(intensity, LM3559_INDICATOR_STEP);

	flash->indicator.indicator_current = intensity;

	return lm3559_set_indicator(flash);
}

static int lm3559_g_indicator_intensity(struct v4l2_subdev *sd, s32 *val)
{
	struct lm3559 *flash = to_lm3559(sd);

	*val = LM3559_VALUE_TO_PERCENT(
		(u32)flash->indicator.indicator_current, LM3559_INDICATOR_STEP);

	return 0;
}

static int lm3559_s_flash_strobe(struct v4l2_subdev *sd, u32 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return lm3559_hw_strobe(client, val);
}

static int lm3559_s_flash_mode(struct v4l2_subdev *sd, u32 new_mode)
{
	struct lm3559 *flash = to_lm3559(sd);
	unsigned int mode;

	switch (new_mode) {
	case ATOMISP_FLASH_MODE_OFF:
		mode = LM3559_MODE_SHUTDOWN;
		break;
	case ATOMISP_FLASH_MODE_FLASH:
		mode = LM3559_MODE_FLASH;
		break;
	case ATOMISP_FLASH_MODE_INDICATOR:
		mode = LM3559_MODE_INDICATOR;
		break;
	case ATOMISP_FLASH_MODE_TORCH:
		mode = LM3559_MODE_TORCH;
		break;
	default:
		return -EINVAL;
	}

	return lm3559_set_mode(flash, mode);
}

static int lm3559_g_flash_mode(struct v4l2_subdev *sd, s32 *val)
{
	struct lm3559 *flash = to_lm3559(sd);
	*val = flash->mode;
	return 0;
}

static int lm3559_g_flash_status(struct v4l2_subdev *sd, s32 *val)
{
	struct lm3559 *flash = to_lm3559(sd);
	int value;

	value = lm3559_read_status(flash);
	if (value < 0)
		return value;

	if (value & LM3559_FLAG_TIMEOUT)
		*val = ATOMISP_FLASH_STATUS_TIMEOUT;
	else if (value > 0)
		*val = ATOMISP_FLASH_STATUS_HW_ERROR;
	else
		*val = ATOMISP_FLASH_STATUS_OK;

	return 0;
}

static int lm3559_g_flash_status_register(struct v4l2_subdev *sd, s32 *val)
{
	struct lm3559 *flash = to_lm3559(sd);
	int ret;

	ret = lm3559_read(flash, LM3559_FLAGS_REG);

	if (ret < 0)
		return ret;

	*val = ret;
	return 0;
}

static const struct lm3559_ctrl_id lm3559_ctrls[] = {
	s_ctrl_id_entry_integer(V4L2_CID_FLASH_TIMEOUT,
				"Flash Timeout",
				0,
				LM3559_MAX_TIMEOUT,
				1,
				LM3559_DEFAULT_TIMEOUT,
				0,
				lm3559_s_flash_timeout,
				lm3559_g_flash_timeout),
	s_ctrl_id_entry_integer(V4L2_CID_FLASH_INTENSITY,
				"Flash Intensity",
				LM3559_MIN_PERCENT,
				LM3559_MAX_PERCENT,
				1,
				LM3559_FLASH_DEFAULT_BRIGHTNESS,
				0,
				lm3559_s_flash_intensity,
				lm3559_g_flash_intensity),
	s_ctrl_id_entry_integer(V4L2_CID_FLASH_TORCH_INTENSITY,
				"Torch Intensity",
				LM3559_MIN_PERCENT,
				LM3559_MAX_PERCENT,
				1,
				LM3559_TORCH_DEFAULT_BRIGHTNESS,
				0,
				lm3559_s_torch_intensity,
				lm3559_g_torch_intensity),
	s_ctrl_id_entry_integer(V4L2_CID_FLASH_INDICATOR_INTENSITY,
				"Indicator Intensity",
				LM3559_MIN_PERCENT,
				LM3559_MAX_PERCENT,
				1,
				LM3559_INDICATOR_DEFAULT_BRIGHTNESS,
				0,
				lm3559_s_indicator_intensity,
				lm3559_g_indicator_intensity),
	s_ctrl_id_entry_boolean(V4L2_CID_FLASH_STROBE,
				"Flash Strobe",
				0,
				0,
				lm3559_s_flash_strobe,
				NULL),
	s_ctrl_id_entry_integer(V4L2_CID_FLASH_MODE,
				"Flash Mode",
				0,   /* don't assume any enum ID is first */
				100, /* enum value, may get extended */
				1,
				ATOMISP_FLASH_MODE_OFF,
				0,
				lm3559_s_flash_mode,
				lm3559_g_flash_mode),
	s_ctrl_id_entry_integer(V4L2_CID_FLASH_STATUS,
				"Flash Status",
				0,   /* don't assume any enum ID is first */
				100, /* enum value, may get extended */
				1,
				ATOMISP_FLASH_STATUS_OK,
				0,
				NULL,
				lm3559_g_flash_status),
	s_ctrl_id_entry_integer(V4L2_CID_FLASH_STATUS_REGISTER,
				"Flash Status Register",
				0,   /* don't assume any enum ID is first */
				100, /* enum value, may get extended */
				1,
				0,
				0,
				NULL,
				lm3559_g_flash_status_register),
};

static const struct lm3559_ctrl_id *find_ctrl_id(unsigned int id)
{
	int i;
	int num;

	num = ARRAY_SIZE(lm3559_ctrls);
	for (i = 0; i < num; i++) {
		if (lm3559_ctrls[i].qc.id == id)
			return &lm3559_ctrls[i];
	}

	return NULL;
}

static int lm3559_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
	int num;

	if (!qc)
		return -EINVAL;

	num = ARRAY_SIZE(lm3559_ctrls);
	if (qc->id >= num)
		return -EINVAL;

	*qc = lm3559_ctrls[qc->id].qc;

	return 0;
}

static int lm3559_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	const struct lm3559_ctrl_id *s_ctrl;

	if (!ctrl)
		return -EINVAL;

	s_ctrl = find_ctrl_id(ctrl->id);
	if (!s_ctrl)
		return -EINVAL;

	return s_ctrl->s_ctrl(sd, ctrl->value);
}

static int lm3559_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	const struct lm3559_ctrl_id *s_ctrl;

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
static int lm3559_setup(struct lm3559 *flash)
{
	struct i2c_client *client = v4l2_get_subdevdata(&flash->sd);
	unsigned int flash_current_limit = flash->pdata->flash_current_limit;
	int ret;

	/* clear the flags register */
	ret = lm3559_read(flash, LM3559_FLAGS_REG);
	if (ret < 0)
		return ret;

	dev_dbg(&client->dev, "Fault info: %02x\n", ret);

	ret = lm3559_set_config(flash);
	if (ret < 0)
		return ret;

	flash->timeout = LM3559_DEFAULT_TIMEOUT_SETTING;
	ret = lm3559_set_duration(flash);
	if (ret < 0)
		return ret;

	flash->torch_current = LM3559_TORCH_DEFAULT;
	ret = lm3559_set_torch(flash);
	if (ret < 0)
		return ret;

	if (flash_current_limit == 0)
		flash_current_limit = LM3559_FLASH_MAX_CURRENT;
	flash->flash_current = LM3559_FLASH_DEFAULT_BRIGHTNESS *
				flash_current_limit / LM3559_MAX_PERCENT;
	ret = lm3559_set_flash(flash);
	if (ret < 0)
		return ret;

	/* read status */
	ret = lm3559_read_status(flash);
	if (ret < 0)
		return ret;

	return ret ? -EIO : 0;
}

static int __lm3559_s_power(struct lm3559 *flash, int power)
{
	struct lm3559_platform_data *pdata = flash->pdata;
	int ret;

	ret = gpio_request(pdata->gpio_reset, "flash reset");
	if (ret < 0)
		return ret;

	ret = gpio_direction_output(pdata->gpio_reset, power);
	if (ret < 0) {
		gpio_free(pdata->gpio_reset);
		return ret;
	}
	gpio_set_value(pdata->gpio_reset, power);
	gpio_free(pdata->gpio_reset);
	usleep_range(100, 100);

	if (power) {
		/* Setup default values. This makes sure that the chip
		 * is in a known state.
		 */
		ret = lm3559_setup(flash);
		if (ret < 0) {
			__lm3559_s_power(flash, 0);
			return ret;
		}
	}

	return 0;
}

static int lm3559_s_power(struct v4l2_subdev *sd, int power)
{
	struct lm3559 *flash = to_lm3559(sd);
	int ret = 0;

	mutex_lock(&flash->power_lock);

	if (flash->power_count == !power) {
		ret = __lm3559_s_power(flash, !!power);
		if (ret < 0)
			goto done;
	}

	flash->power_count += power ? 1 : -1;
	WARN_ON(flash->power_count < 0);

done:
	mutex_unlock(&flash->power_lock);
	return ret;
}

static const struct v4l2_subdev_core_ops lm3559_core_ops = {
	.queryctrl = lm3559_queryctrl,
	.g_ctrl = lm3559_g_ctrl,
	.s_ctrl = lm3559_s_ctrl,
	.s_power = lm3559_s_power,
};

static const struct v4l2_subdev_ops lm3559_ops = {
	.core = &lm3559_core_ops,
};

static int lm3559_detect(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct i2c_adapter *adapter = client->adapter;
	struct lm3559 *flash = to_lm3559(sd);
	int ret;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&client->dev, "lm3559_detect i2c error\n");
		return -ENODEV;
	}

	/* Make sure the power is initially off to ensure chip is resetted */
	__lm3559_s_power(flash, 0);

	/* Power up the flash driver, resetting and initializing it. */
	ret = lm3559_s_power(&flash->sd, 1);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to power on lm3559 LED flash\n");
	} else {
		dev_dbg(&client->dev, "Successfully detected lm3559 LED flash\n");
		lm3559_s_power(&flash->sd, 0);
	}

	return ret;
}

static int lm3559_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	return lm3559_s_power(sd, 1);
}

static int lm3559_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	return lm3559_s_power(sd, 0);
}

static const struct v4l2_subdev_internal_ops lm3559_internal_ops = {
	.registered = lm3559_detect,
	.open = lm3559_open,
	.close = lm3559_close,
};

/* -----------------------------------------------------------------------------
 *  I2C driver
 */
#ifdef CONFIG_PM

static int lm3559_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	struct lm3559 *flash = to_lm3559(subdev);
	int rval;

	if (flash->power_count == 0)
		return 0;

	rval = __lm3559_s_power(flash, 0);

	dev_dbg(&client->dev, "Suspend %s\n", rval < 0 ? "failed" : "ok");

	return rval;
}

static int lm3559_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	struct lm3559 *flash = to_lm3559(subdev);
	int rval;

	if (flash->power_count == 0)
		return 0;

	rval = __lm3559_s_power(flash, 1);

	dev_dbg(&client->dev, "Resume %s\n", rval < 0 ? "fail" : "ok");

	return rval;
}

#else

#define lm3559_suspend NULL
#define lm3559_resume  NULL

#endif /* CONFIG_PM */

static int lm3559_gpio_init(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct lm3559 *flash = to_lm3559(sd);
	struct lm3559_platform_data *pdata = flash->pdata;
	int ret;

	ret = gpio_request(pdata->gpio_strobe, "flash");
	if (ret < 0)
		return ret;

	ret = gpio_direction_output(pdata->gpio_strobe, 0);
	if (ret < 0)
		goto err_gpio_flash;

	return 0;

err_gpio_flash:
	gpio_free(pdata->gpio_strobe);
	return ret;
}

static int lm3559_gpio_uninit(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct lm3559 *flash = to_lm3559(sd);
	struct lm3559_platform_data *pdata = flash->pdata;
	int ret;

	ret = gpio_direction_output(pdata->gpio_strobe, 0);
	if (ret < 0)
		return ret;

	gpio_free(pdata->gpio_strobe);

	return 0;
}

static int lm3559_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	int err;
	struct lm3559 *flash;

	if (client->dev.platform_data == NULL) {
		dev_err(&client->dev, "no platform data\n");
		return -ENODEV;
	}

	flash = kzalloc(sizeof(*flash), GFP_KERNEL);
	if (!flash) {
		dev_err(&client->dev, "out of memory\n");
		return -ENOMEM;
	}

	flash->hw_type = id->driver_data;
	flash->pdata = client->dev.platform_data;

	v4l2_i2c_subdev_init(&flash->sd, client, &lm3559_ops);
	flash->sd.internal_ops = &lm3559_internal_ops;
	flash->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	flash->mode = ATOMISP_FLASH_MODE_OFF;

	err = media_entity_init(&flash->sd.entity, 0, NULL, 0);
	if (err) {
		dev_err(&client->dev, "error initialize a media entity.\n");
		goto fail1;
	}

	flash->sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV_FLASH;

	mutex_init(&flash->power_lock);

	setup_timer(&flash->flash_off_delay, lm3559_flash_off_delay,
		    (unsigned long)client);

	err = lm3559_gpio_init(client);
	if (err) {
		dev_err(&client->dev, "gpio request/direction_output fail");
		goto fail2;
	}

	if (flash->hw_type == lm3559_hw_type_lm3560 &&
	    (flash->pdata->flash_current_limit == 0 ||
	     flash->pdata->flash_current_limit > LM3560_FLASH_MAX_CURRENT))
		flash->pdata->flash_current_limit = LM3560_FLASH_MAX_CURRENT;

	return 0;
fail2:
	media_entity_cleanup(&flash->sd.entity);
fail1:
	v4l2_device_unregister_subdev(&flash->sd);
	kfree(flash);

	return err;
}

static int lm3559_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct lm3559 *flash = to_lm3559(sd);
	int ret;

	media_entity_cleanup(&flash->sd.entity);
	v4l2_device_unregister_subdev(sd);

	del_timer_sync(&flash->flash_off_delay);

	ret = lm3559_gpio_uninit(client);
	if (ret < 0)
		goto fail;

	kfree(flash);

	return 0;
fail:
	dev_err(&client->dev, "gpio request/direction_output fail");
	return ret;
}

static const struct i2c_device_id lm3559_id[] = {
	{ LM3559_NAME, lm3559_hw_type_lm3559 },
	{ LM3560_NAME, lm3559_hw_type_lm3560 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, lm3559_id);

static const struct dev_pm_ops lm3559_pm_ops = {
	.suspend = lm3559_suspend,
	.resume = lm3559_resume,
};

static struct i2c_driver lm3559_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = LM3559_NAME,
		.pm   = &lm3559_pm_ops,
	},
	.probe = lm3559_probe,
	.remove = lm3559_remove,
	.id_table = lm3559_id,
};

static __init int init_lm3559(void)
{
	return i2c_add_driver(&lm3559_driver);
}

static __exit void exit_lm3559(void)
{
	i2c_del_driver(&lm3559_driver);
}

module_init(init_lm3559);
module_exit(exit_lm3559);
MODULE_AUTHOR("Shenbo Huang <shenbo.huang@intel.com>");
MODULE_DESCRIPTION("LED flash driver for LM3559");
MODULE_LICENSE("GPL");
