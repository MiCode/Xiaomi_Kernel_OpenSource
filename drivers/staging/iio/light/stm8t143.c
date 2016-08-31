/*
 * A iio driver for the light sensor STM8T143.
 *
 * IIO Light driver for monitoring proximity.
 *
 * Copyright (c) 2013, NVIDIA CORPORATION. All rights reserved.
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
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/of_gpio.h>


#define NUM_REGULATORS	1
#define NUM_CHANNELS	1

enum channels {
	PROX,
};

enum channel_state {
	CHIP_POWER_OFF,
	CHIP_POWER_ON,
};

struct stm8t143_chip {
	struct platform_device		*pdev;
	int				pout_gpio;
	int				tout_gpio;
	struct regulator_bulk_data	consumers[NUM_REGULATORS];

	u8				state[NUM_CHANNELS];
};


/* device's registration with iio to facilitate user operations */
static ssize_t stm8t143_chan_regulator_enable(
		struct iio_dev *indio_dev, uintptr_t private,
		struct iio_chan_spec const *chan,
		const char *buf, size_t len)
{
	u8 enable;
	int ret = 0;
	struct stm8t143_chip *chip = iio_priv(indio_dev);

	if (kstrtou8(buf, 10, &enable))
		return -EINVAL;

	if ((enable != 0) && (enable != 1))
		return -EINVAL;

	if (chan->type != IIO_PROXIMITY)
		return -EINVAL;

	if (enable == (chip->state[PROX] != CHIP_POWER_OFF))
		return 1;

	if (chip->consumers[0].supply == NULL)
		goto success;

	if (enable)
		ret = regulator_bulk_enable(1, chip->consumers);
	else
		ret = regulator_bulk_disable(1, chip->consumers);

	if (ret) {
		dev_err(&chip->pdev->dev,
			"devname:%s func:%s line:%d err:_stm8t143_register_read fails\n",
			chip->pdev->name, __func__, __LINE__);
		goto fail;
	}

success:
	chip->state[PROX] = enable;
fail:
	return ret ? ret : 1;
}

/*
 * chan_regulator_enable is used to enable regulators used by
 * particular channel.
 * chan_enable actually configures various registers to activate
 * a particular channel.
 */
static const struct iio_chan_spec_ext_info stm8t143_ext_info[] = {
	{
		.name = "regulator_enable",
		.shared = true,
		.write = stm8t143_chan_regulator_enable,
	},
	{
	},
};

static const struct iio_chan_spec stm8t143_channels[] = {
	{
		.type = IIO_PROXIMITY,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
		.ext_info = stm8t143_ext_info,
	},
};

static int stm8t143_read_raw(struct iio_dev *indio_dev,
	struct iio_chan_spec const *chan, int *val, int *val2, long mask)
{
	struct stm8t143_chip *chip = iio_priv(indio_dev);

	/* get_chan_num != -EINVAL <=> get_data_reg != -EINVAL */
	if (chan->type != IIO_PROXIMITY)
		return -EINVAL;

	if (chip->state[PROX] != CHIP_POWER_ON)
		return -EINVAL;

	*val = !gpio_get_value(chip->tout_gpio);
	pr_debug("proximity value:%d\n", *val);

	return IIO_VAL_INT;
}

static IIO_CONST_ATTR(vendor, "STMicroelectronics");
static IIO_CONST_ATTR(in_proximity_integration_time,
			"500000000"); /* 500 msec */
static IIO_CONST_ATTR(in_proximity_max_range, "20"); /* cm */
static IIO_CONST_ATTR(in_proximity_power_consumed,
			"38"); /* milli Watt */

static struct attribute *stm8t143_attrs[] = {
	&iio_const_attr_vendor.dev_attr.attr,
	&iio_const_attr_in_proximity_integration_time.dev_attr.attr,
	&iio_const_attr_in_proximity_max_range.dev_attr.attr,
	&iio_const_attr_in_proximity_power_consumed.dev_attr.attr,
	NULL
};

static struct attribute_group stm8t143_attr_group = {
	.name = "stm8t143",
	.attrs = stm8t143_attrs
};

/*
 * read_raw is used to report a channel's data to user
 * in non SI units
 */
static const struct iio_info stm8t143_iio_info = {
	.driver_module = THIS_MODULE,
	.read_raw = &stm8t143_read_raw,
	.attrs = &stm8t143_attr_group,
};

#ifdef CONFIG_PM_SLEEP
static int stm8t143_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);
	struct stm8t143_chip *chip = iio_priv(indio_dev);
	int ret = 0;

	if (chip->consumers[0].supply && chip->state[PROX])
		ret |= regulator_bulk_disable(1, chip->consumers);

	if (ret) {
		dev_err(&chip->pdev->dev,
			"devname:%s func:%s line:%d err:regulator_disable fails\n",
			chip->pdev->name, __func__, __LINE__);
		return ret;
	}

	return ret;
}

static int stm8t143_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);
	struct stm8t143_chip *chip = iio_priv(indio_dev);
	int ret = 0;

	if (chip->consumers[0].supply && chip->state[PROX])
		ret |= regulator_bulk_enable(1, chip->consumers);

	if (ret) {
		dev_err(&chip->pdev->dev,
			"devname:%s func:%s line:%d err:regulator_enable fails\n",
				chip->pdev->name, __func__, __LINE__);
		return ret;
	}

	return 0;
}

static SIMPLE_DEV_PM_OPS(stm8t143_pm_ops, stm8t143_suspend, stm8t143_resume);
#define STM8T143_PM_OPS (&stm8t143_pm_ops)
#else
#define STM8T143_PM_OPS NULL
#endif

/* parses DT for gpio information */
static int stm8t143_gpio(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);
	struct stm8t143_chip *chip = iio_priv(indio_dev);
	int *pout_gpio = &chip->pout_gpio;
	int *tout_gpio = &chip->tout_gpio;
	int ret;

	*pout_gpio = of_get_named_gpio(np, "pout-gpio", 0);
	if (IS_ERR_VALUE(*pout_gpio)) {
		pr_err("could not pout gpio from DT");
		return *pout_gpio;
	}

	*tout_gpio = of_get_named_gpio(np, "tout-gpio", 0);
	if (IS_ERR_VALUE(*tout_gpio)) {
		pr_err("could not tout gpio from DT");
		return *tout_gpio;
	}

	/* essentially we are not considering the pout as a input at all */
	ret = gpio_request_one(*pout_gpio,
				GPIOF_OUT_INIT_LOW,
				"pout_gpio");
	if (ret < 0) {
		dev_err(&pdev->dev, "GPIO request for gpio %d failed %d\n",
				*pout_gpio, ret);
		return ret;
	}

	ret = gpio_request_one(*tout_gpio,
				GPIOF_IN,
				"tout_gpio");
	if (ret < 0) {
		dev_err(&pdev->dev, "GPIO request for gpio %d failed %d\n",
				*tout_gpio, ret);
		gpio_free(*pout_gpio);
		return ret;
	}
	return 0;
}

static void init_stm8t143_regulators(struct platform_device *pdev)
{
	int i, ret = 0;
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);
	struct stm8t143_chip *chip = iio_priv(indio_dev);

	struct regulator_bulk_data stm8t143_consumers[] = {
		{
			.supply = "vdd",
		},
	};

	if (!ARRAY_SIZE(stm8t143_consumers))
		return;

	for (i = 0; i < ARRAY_SIZE(stm8t143_consumers); i++) {
		chip->consumers[i].supply = stm8t143_consumers[i].supply;
		chip->consumers[i].consumer = stm8t143_consumers[i].consumer;
		chip->consumers[i].ret = stm8t143_consumers[i].ret;
	}

	ret = devm_regulator_bulk_get(&pdev->dev,
					ARRAY_SIZE(stm8t143_consumers),
					chip->consumers);
	if (ret) {
		dev_info(&pdev->dev,
			"devname:%s func:%s line:%d regulators not found\n",
			pdev->name, __func__, __LINE__);
		for (i = 0; i < ARRAY_SIZE(stm8t143_consumers); i++) {
			chip->consumers[i].supply = NULL;
			chip->consumers[i].consumer = NULL;
			chip->consumers[i].ret = 0;
		}
	}
}

static int stm8t143_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct stm8t143_chip *chip;
	struct iio_dev *indio_dev;

	indio_dev = iio_device_alloc(sizeof(*chip));
	if (indio_dev == NULL) {
		dev_err(&pdev->dev,
			"devname:%s func:%s line:%d err:iio_device_alloc fails\n",
			pdev->name, __func__, __LINE__);
		return -ENOMEM;
	}
	chip = iio_priv(indio_dev);
	platform_set_drvdata(pdev, indio_dev);

	ret = stm8t143_gpio(pdev);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"devname:%s func:%s line:%d err:gpio_init fails\n",
			pdev->name, __func__, __LINE__);
		goto err_gpio_irq_init;
	}

	init_stm8t143_regulators(pdev);

	indio_dev->info = &stm8t143_iio_info;
	indio_dev->channels = stm8t143_channels;
	indio_dev->num_channels = 1;
	indio_dev->name = pdev->name;
	indio_dev->dev.parent = &pdev->dev;
	indio_dev->modes = INDIO_DIRECT_MODE;
	ret = iio_device_register(indio_dev);
	if (ret) {
		dev_err(&pdev->dev,
			"devname:%s func:%s line:%d err:iio_device_register fails\n",
			pdev->name, __func__, __LINE__);
		goto err_iio_register;
	}

	chip->state[PROX] = CHIP_POWER_OFF;

	dev_info(&pdev->dev, "devname:%s func:%s line:%d probe success\n",
			pdev->name, __func__, __LINE__);

	return 0;

err_iio_register:
	gpio_free(chip->tout_gpio);
	gpio_free(chip->pout_gpio);
err_gpio_irq_init:
	iio_device_free(indio_dev);
	return ret;
}

static int stm8t143_remove(struct platform_device *pdev)
{
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);
	struct stm8t143_chip *chip = iio_priv(indio_dev);

	if (chip->consumers[0].supply && chip->state[PROX])
		regulator_bulk_disable(1, chip->consumers);

	gpio_free(chip->tout_gpio);
	gpio_free(chip->pout_gpio);

	iio_device_unregister(indio_dev);
	iio_device_free(indio_dev);
	return 0;
}

static void stm8t143_shutdown(struct platform_device *pdev)
{
	stm8t143_remove(pdev);
}

static const struct of_device_id stm8t143_of_match[] = {
	{ .compatible = "stm,stm8t143", },
	{ },
};
MODULE_DEVICE_TABLE(of, stm8t143_of_match);

static struct platform_driver stm8t143_driver = {
	.driver = {
		.name = "stm8t143",
		.owner = THIS_MODULE,
		.of_match_table = stm8t143_of_match,
		.pm = STM8T143_PM_OPS,
	},
	.probe = stm8t143_probe,
	.remove = stm8t143_remove,
	.shutdown = stm8t143_shutdown,
};

module_platform_driver(stm8t143_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("stm8t143 Driver");
MODULE_AUTHOR("Sri Krishna chowdary <schowdary@nvidia.com>");
