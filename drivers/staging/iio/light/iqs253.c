/*
 * A iio driver for the capacitive sensor IQS253.
 *
 * IIO Light driver for monitoring proximity.
 *
 * Copyright (c) 2014, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/irqchip/tegra.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/light/ls_sysfs.h>
#include <linux/iio/light/ls_dt.h>

/* registers */
#define SYSFLAGS		0x10
#define PROX_STATUS		0x31
#define TOUCH_STATUS		0x35
#define TARGET			0xC4
#define COMP0			0xC5
#define CH0_ATI_BASE		0xC8
#define CH1_ATI_BASE		0xC9
#define CH2_ATI_BASE		0xCA
#define CH0_PTH			0xCB
#define CH1_PTH			0xCC
#define CH2_PTH			0xCD
#define PROX_SETTINGS0		0xD1
#define PROX_SETTINGS1		0xD2
#define PROX_SETTINGS2		0xD3
#define PROX_SETTINGS3		0xD4
#define ACTIVE_CHAN		0xD5
#define LOW_POWER		0xD6
#define DYCAL_CHANS		0xD8
#define EVENT_MODE_MASK		0xD9
#define DEFAULT_COMMS_POINTER	0xDD

#define IQS253_PROD_ID		41

#define PROX_CH0		0x01
#define PROX_CH1		0x02
#define PROX_CH2		0x04

#define STYLUS_ONLY		PROX_CH0
#define PROXIMITY_ONLY		(PROX_CH1 | PROX_CH2)

#define CH0_COMPENSATION	0x55

#define PROX_TH_CH0		0x04 /* proximity threshold = 0.5 cm */
#define PROX_TH_CH1		0x01 /* proximity threshold = 2 cm */
#define PROX_TH_CH2		0x01 /* proximity threshold = 2 cm */

#define DISABLE_DYCAL		0x00

#define CH0_ATI_TH		0x17
#define CH1_ATI_TH		0x17
#define CH2_ATI_TH		0x19

#define EVENT_PROX_ONLY		0x01

#define PROX_SETTING_NORMAL	0x25
#define PROX_SETTING_STYLUS	0x26

#define ATI_IN_PROGRESS		0x04

#define NUM_REG 17

struct iqs253_chip {
	struct i2c_client	*client;
	const struct i2c_device_id	*id;
	u32			rdy_gpio;
	u32			wake_gpio;
	u32			sar_gpio;
	u32			mode;
	u32			value;
	struct regulator	*vddhi;
	u32			using_regulator;
	struct lightsensor_spec	*ls_spec;
	u32			stylus_inserted;
};

enum mode {
	MODE_NONE = -1,
	NORMAL_MODE,
	STYLUS_MODE,
	NUM_MODE
};

struct reg_val_pair {
	u8 reg;
	u8 val;
};

struct reg_val_pair reg_val_map[NUM_MODE][NUM_REG] = {
	{
		{ COMP0, CH0_COMPENSATION},
		{ CH0_ATI_BASE, CH0_ATI_TH},
		{ CH1_ATI_BASE, CH1_ATI_TH},
		{ CH2_ATI_BASE, CH2_ATI_TH},
		{ CH0_PTH, PROX_TH_CH0},
		{ CH1_PTH, PROX_TH_CH1},
		{ PROX_SETTINGS0, PROX_SETTING_NORMAL},
		{ ACTIVE_CHAN, PROXIMITY_ONLY | STYLUS_ONLY},
		{ DYCAL_CHANS, DISABLE_DYCAL},
		{ EVENT_MODE_MASK, EVENT_PROX_ONLY}
	},
	{
		{ COMP0, CH0_COMPENSATION},
		{ CH0_ATI_BASE, CH0_ATI_TH},
		{ CH1_ATI_BASE, CH1_ATI_TH},
		{ CH2_ATI_BASE, CH2_ATI_TH},
		{ CH2_PTH,  PROX_TH_CH2},
		{ PROX_SETTINGS0, PROX_SETTING_STYLUS},
		{ ACTIVE_CHAN, STYLUS_ONLY},
		{ DYCAL_CHANS, DISABLE_DYCAL},
		{ EVENT_MODE_MASK, EVENT_PROX_ONLY}
	},
};

static void iqs253_i2c_hand_shake(struct iqs253_chip *iqs253_chip)
{
	int retry_count = 10;
	do {
		gpio_direction_output(iqs253_chip->rdy_gpio, 0);
		mdelay(10);
		/* put to tristate */
		gpio_direction_input(iqs253_chip->rdy_gpio);
	} while (gpio_get_value(iqs253_chip->rdy_gpio) && retry_count--);
}

static int iqs253_i2c_read_byte(struct iqs253_chip *chip, int reg)
{
	int ret = 0, retry_count = 10;
	do {
		iqs253_i2c_hand_shake(chip);
		ret = i2c_smbus_read_byte_data(chip->client, reg);
	} while (ret && retry_count--);
	return ret;
}

static int iqs253_i2c_write_byte(struct iqs253_chip *chip, int reg, int val)
{
	int ret = 0, retry_count = 10;
	do {
		iqs253_i2c_hand_shake(chip);
		ret = i2c_smbus_write_byte_data(chip->client, reg, val);
	} while (ret && retry_count--);
	return ret;
}

/* must call holding lock */
static int iqs253_set(struct iqs253_chip *iqs253_chip, int mode)
{
	int ret = 0, i;
	struct reg_val_pair *reg_val_pair_map;

	if ((mode != NORMAL_MODE) && (mode != STYLUS_MODE))
		return -EINVAL;

	reg_val_pair_map = reg_val_map[mode];

	for (i = 0; i < NUM_REG; i++) {
		if (!reg_val_pair_map[i].reg && !reg_val_pair_map[i].val)
			continue;

		ret = iqs253_i2c_write_byte(iqs253_chip,
					reg_val_pair_map[i].reg,
					reg_val_pair_map[i].val);
		if (ret) {
			dev_err(&iqs253_chip->client->dev,
				"iqs253 write val:%x to reg:%x failed\n",
				reg_val_pair_map[i].val,
				reg_val_pair_map[i].reg);
			return ret;
		}
	}

	/* wait for ATI to finish */
	do {
		ret = iqs253_i2c_read_byte(iqs253_chip, SYSFLAGS);
		mdelay(10);
	} while (ret & ATI_IN_PROGRESS);

	iqs253_chip->mode = mode;
	return 0;
}

/* device's registration with iio to facilitate user operations */
static ssize_t iqs253_chan_regulator_enable(
		struct iio_dev *indio_dev, uintptr_t private,
		struct iio_chan_spec const *chan,
		const char *buf, size_t len)
{
	int ret = 0;
	u8 enable;
	struct iqs253_chip *chip = iio_priv(indio_dev);

	if (chip->mode == STYLUS_MODE)
		return -EINVAL;

	if (kstrtou8(buf, 10, &enable))
		return -EINVAL;

	if ((enable != 0) && (enable != 1))
		return -EINVAL;

	if (chan->type != IIO_PROXIMITY)
		return -EINVAL;

	if (enable == chip->using_regulator)
		goto success;

	if (enable)
		ret = regulator_enable(chip->vddhi);
	else
		ret = regulator_disable(chip->vddhi);

	if (ret) {
		dev_err(&chip->client->dev,
		"idname:%s func:%s line:%d enable:%d regulator logic failed\n",
		chip->id->name, __func__, __LINE__, enable);
		goto fail;
	}

success:
	chip->using_regulator = enable;
	chip->mode = MODE_NONE;
fail:
	return ret ? ret : 1;
}

static ssize_t iqs253_chan_normal_mode_enable(
		struct iio_dev *indio_dev, uintptr_t private,
		struct iio_chan_spec const *chan,
		const char *buf, size_t len)
{
	int ret = 0;
	u8 enable;
	struct iqs253_chip *chip = iio_priv(indio_dev);

	if (chip->mode == STYLUS_MODE)
		return -EINVAL;

	if (kstrtou8(buf, 10, &enable))
		return -EINVAL;

	if ((enable != 0) && (enable != 1))
		return -EINVAL;

	if (chan->type != IIO_PROXIMITY)
		return -EINVAL;

	if (!chip->using_regulator)
		return -EINVAL;

	if (enable)
		ret = iqs253_set(chip, NORMAL_MODE);
	else
		chip->mode = MODE_NONE;

	return ret ? ret : 1;
}

/*
 * chan_regulator_enable is used to enable regulators used by
 * particular channel.
 * chan_enable actually configures various registers to activate
 * a particular channel.
 */
static const struct iio_chan_spec_ext_info iqs253_ext_info[] = {
	{
		.name = "regulator_enable",
		.write = iqs253_chan_regulator_enable,
	},
	{
		.name = "enable",
		.write = iqs253_chan_normal_mode_enable,
	},
	{
	},
};

static const struct iio_chan_spec iqs253_channels[] = {
	{
		.type = IIO_PROXIMITY,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
		.ext_info = iqs253_ext_info,
	},
};

static int iqs253_read_raw(struct iio_dev *indio_dev,
	struct iio_chan_spec const *chan, int *val, int *val2, long mask)
{
	struct iqs253_chip *chip = iio_priv(indio_dev);
	int ret;

	if (chip->mode != NORMAL_MODE)
		return -EINVAL;

	if (chan->type != IIO_PROXIMITY)
		return -EINVAL;

	ret = iqs253_i2c_read_byte(chip, PROX_STATUS);
	chip->value = -1;
	if (ret >= 0) {
		if ((ret >= 0) && (chip->mode == NORMAL_MODE)) {
			ret = ret & PROXIMITY_ONLY;
			/*
			 * if both channel detect proximity => distance = 0;
			 * if one channel detects proximity => distance = 1;
			 * if no channel detects proximity => distance = 2;
			 */
			chip->value = (ret == (PROX_CH1 | PROX_CH2)) ? 0 :
								ret ? 1 : 2;
		}
	}
	if (chip->value == -1)
		return -EINVAL;

	*val = chip->value; /* cm */

	/* provide input to SAR */
	if (chip->value)
		gpio_direction_output(chip->sar_gpio, 0);
	else
		gpio_direction_output(chip->sar_gpio, 1);

	return IIO_VAL_INT;
}

static IIO_CONST_ATTR(vendor, "Azoteq");
static IIO_CONST_ATTR(in_proximity_integration_time,
			"16000000"); /* 16 msec */
static IIO_CONST_ATTR(in_proximity_max_range, "2"); /* cm */
static IIO_CONST_ATTR(in_proximity_power_consumed, "1.67"); /* mA */

static struct attribute *iqs253_attrs[] = {
	&iio_const_attr_vendor.dev_attr.attr,
	&iio_const_attr_in_proximity_integration_time.dev_attr.attr,
	&iio_const_attr_in_proximity_max_range.dev_attr.attr,
	&iio_const_attr_in_proximity_power_consumed.dev_attr.attr,
	NULL
};

static struct attribute_group iqs253_attr_group = {
	.name = "iqs253",
	.attrs = iqs253_attrs
};

static struct iio_info iqs253_iio_info = {
	.driver_module = THIS_MODULE,
	.read_raw = &iqs253_read_raw,
	.attrs = &iqs253_attr_group,
};

#ifdef CONFIG_PM_SLEEP
static int iqs253_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct iqs253_chip *chip = iio_priv(indio_dev);
	int ret = 0;

	if (!chip->using_regulator)
		ret = regulator_enable(chip->vddhi);

	if (ret) {
		dev_err(&chip->client->dev,
		"idname:%s func:%s line:%d regulator enable fails\n",
		chip->id->name, __func__, __LINE__);
		return ret;
	}

	ret = iqs253_set(chip, STYLUS_MODE);
	if (ret) {
		dev_err(&chip->client->dev,
		"idname:%s func:%s line:%d can not enable stylus mode\n",
		chip->id->name, __func__, __LINE__);
		return ret;
	}
	return tegra_pm_irq_set_wake(tegra_gpio_to_wake(chip->wake_gpio), 1);
}

static int iqs253_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct iqs253_chip *chip = iio_priv(indio_dev);
	int ret = 0;

	if (chip->using_regulator) {
		ret = iqs253_set(chip, NORMAL_MODE);
	} else {
		chip->mode = MODE_NONE;
		ret = regulator_disable(chip->vddhi);
	}

	if (ret) {
		dev_err(&chip->client->dev,
		"idname:%s func:%s line:%d regulator enable fails\n",
		chip->id->name, __func__, __LINE__);
		return ret;
	}

	return ret;
}

static SIMPLE_DEV_PM_OPS(iqs253_pm_ops, iqs253_suspend, iqs253_resume);
#define IQS253_PM_OPS (&iqs253_pm_ops)
#else
#define IQS253_PM_OPS NULL
#endif

static int iqs253_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int ret;
	struct iqs253_chip *iqs253_chip;
	struct iio_dev *indio_dev;
	int rdy_gpio = -1, wake_gpio = -1, sar_gpio = -1;

	rdy_gpio = of_get_named_gpio(client->dev.of_node, "rdy-gpio", 0);
	if (rdy_gpio == -EPROBE_DEFER)
		return -EPROBE_DEFER;

	if (!gpio_is_valid(rdy_gpio))
		return -EINVAL;

	wake_gpio = of_get_named_gpio(client->dev.of_node, "wake-gpio", 0);
	if (wake_gpio == -EPROBE_DEFER)
		return -EPROBE_DEFER;

	if (!gpio_is_valid(wake_gpio))
		return -EINVAL;

	sar_gpio = of_get_named_gpio(client->dev.of_node, "sar-gpio", 0);
	if (sar_gpio == -EPROBE_DEFER)
		return -EPROBE_DEFER;

	ret = gpio_request_one(sar_gpio, GPIOF_OUT_INIT_LOW, NULL);
	if (ret < 0)
		return -EINVAL;

	indio_dev = iio_device_alloc(sizeof(*iqs253_chip));
	if (!indio_dev)
		return -ENOMEM;

	i2c_set_clientdata(client, indio_dev);
	iqs253_chip = iio_priv(indio_dev);

	iqs253_chip->ls_spec = of_get_ls_spec(&client->dev);
	if (!iqs253_chip->ls_spec) {
		dev_err(&client->dev,
			"devname:%s func:%s line:%d invalid meta data\n",
			id->name, __func__, __LINE__);
		return -ENODATA;
	}

	fill_ls_attrs(iqs253_chip->ls_spec, iqs253_attrs);
	indio_dev->info = &iqs253_iio_info;
	indio_dev->channels = iqs253_channels;
	indio_dev->num_channels = 1;
	indio_dev->name = id->name;
	indio_dev->dev.parent = &client->dev;
	indio_dev->modes = INDIO_DIRECT_MODE;
	ret = iio_device_register(indio_dev);
	if (ret) {
		dev_err(&client->dev,
			"devname:%s func:%s line:%d iio_device_register fail\n",
			id->name, __func__, __LINE__);
		goto err_iio_register;
	}

	iqs253_chip->client = client;
	iqs253_chip->id = id;
	iqs253_chip->mode = MODE_NONE;
	iqs253_chip->vddhi = devm_regulator_get(&client->dev, "vddhi");
	if (IS_ERR(iqs253_chip->vddhi)) {
		dev_err(&client->dev,
			"devname:%s func:%s regulator vddhi not found\n",
			id->name, __func__);
		goto err_regulator_get;
	}

	ret = gpio_request(rdy_gpio, "iqs253");
	if (ret) {
			dev_err(&client->dev,
			"devname:%s func:%s regulator vddhi not found\n",
			id->name, __func__);
		goto err_gpio_request;
	}
	iqs253_chip->rdy_gpio = rdy_gpio;
	iqs253_chip->wake_gpio = wake_gpio;
	iqs253_chip->sar_gpio = sar_gpio;

	ret = regulator_enable(iqs253_chip->vddhi);
	if (ret) {
			dev_err(&client->dev,
			"devname:%s func:%s regulator enable failed\n",
			id->name, __func__);
		goto err_gpio_request;
	}

	ret = iqs253_i2c_read_byte(iqs253_chip, 0);
	if (ret != IQS253_PROD_ID) {
			dev_err(&client->dev,
			"devname:%s func:%s device not present\n",
			id->name, __func__);
		goto err_gpio_request;

	}

	ret = regulator_disable(iqs253_chip->vddhi);
	if (ret) {
			dev_err(&client->dev,
			"devname:%s func:%s regulator disable failed\n",
			id->name, __func__);
		goto err_gpio_request;
	}

	dev_info(&client->dev, "devname:%s func:%s line:%d probe success\n",
			id->name, __func__, __LINE__);

	return 0;

err_gpio_request:
err_regulator_get:
	iio_device_unregister(indio_dev);
err_iio_register:
	iio_device_free(indio_dev);

	dev_err(&client->dev, "devname:%s func:%s line:%d probe failed\n",
			id->name, __func__, __LINE__);
	return ret;
}

static int iqs253_remove(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct iqs253_chip *chip = iio_priv(indio_dev);
	gpio_free(chip->rdy_gpio);
	iio_device_unregister(indio_dev);
	iio_device_free(indio_dev);
	return 0;
}

static void iqs253_shutdown(struct i2c_client *client)
{

}

static const struct i2c_device_id iqs253_id[] = {
	{"iqs253", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, iqs253_id);

static const struct of_device_id iqs253_of_match[] = {
	{ .compatible = "azoteq,iqs253", },
	{ },
};
MODULE_DEVICE_TABLE(of, iqs253_of_match);

static struct i2c_driver iqs253_driver = {
	.class = I2C_CLASS_HWMON,
	.driver = {
		.name = "iqs253",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(iqs253_of_match),
	},
	.probe = iqs253_probe,
	.remove = iqs253_remove,
	.shutdown = iqs253_shutdown,
	.id_table = iqs253_id,
};

module_i2c_driver(iqs253_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("IQS253 Driver");
MODULE_AUTHOR("Sri Krishna chowdary <schowdary@nvidia.com>");
