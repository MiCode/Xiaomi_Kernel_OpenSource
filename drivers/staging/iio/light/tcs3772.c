/*
 * A iio driver for the light sensor TCS3772.
 *
 * IIO Light driver for monitoring ambient light intensity in lux and proximity
 * ir.
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
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/regmap.h>
#include <linux/delay.h>
#include <linux/pm_runtime.h>
#include <linux/notifier.h>
#include <linux/regulator/consumer.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>

#define TCS3772_REG_CONFIGURE		0x80
#define TCS3772_REG_PPULSE		0x8E
#define TCS3772_REG_STATUS		0x93
#define TCS3772_REG_ALS_DATA_L		0x94
#define TCS3772_REG_ALS_DATA_H		0x95
#define TCS3772_REG_PROX_DATA_L		0x9C
#define TCS3772_REG_PROX_DATA_H		0x9D
#define TCS3772_REG_MAX			0x9D

#define CONFIGURE_SHTDWN_MASK		(BIT(0) | BIT(1) | BIT(2))
#define CONFIGURE_SHTDWN_EN		0x0

#define CONFIGURE_ALS_MASK		(BIT(0) | BIT(1))
#define CONFIGURE_ALS_MASK_PROX_ON	BIT(1)
#define CONFIGURE_ALS_EN		0x3

#define CONFIGURE_PROX_MASK		(BIT(0) | BIT(2))
#define CONFIGURE_PROX_MASK_ALS_ON	BIT(2)
#define CONFIGURE_PROX_EN		0x5

#define STATUS_ALS_VALID		0x1
#define STATUS_PROX_VALID		0x2

#define PPULSE_MASK			0xff
#define PPULSE_NUM			0x1

#define TCS3772_POLL_DELAY		100 /* mSec */
#define I2C_MAX_TIMEOUT			msecs_to_jiffies(20) /* 20 mSec */

#define TCS3772_N_CHANNELS		2

#define get_chan_num(type)		(type == IIO_LIGHT ? ALS : \
					type == IIO_PROXIMITY ? PROX : -EINVAL)

#define get_data_reg(type)		(type == IIO_LIGHT ? \
					TCS3772_REG_ALS_DATA_L : \
					type == IIO_PROXIMITY ? \
					TCS3772_REG_PROX_DATA_L : -EINVAL)
#define get_valid_mask(type)		(type == IIO_LIGHT ? \
					STATUS_ALS_VALID : \
					type == IIO_PROXIMITY ? \
					STATUS_PROX_VALID : -EINVAL)

enum channels {
	ALS,
	PROX,
};

enum channel_state {
	CHIP_POWER_OFF,
	CHIP_POWER_ON_CHAN_OFF,
	CHIP_POWER_ON_CHAN_ON,
};

struct tcs3772_chip {
	struct i2c_client		*client;
	struct i2c_device_id		*id;
	struct mutex			lock;
	struct regulator_bulk_data	*consumers;
	struct notifier_block		regulator_nb;
	wait_queue_head_t		i2c_wait_queue;
	int				i2c_xfer_ready;
	struct regmap			*regmap;

	u8				state[TCS3772_N_CHANNELS];
	int shutdown_complete;
};

/* regulators used by the device */
static struct regulator_bulk_data tcs3772_consumers[] = {
	{
		.supply = "vdd",
	},
};

/* device's regmap configuration for i2c communication */
/* non cacheable registers*/
bool tcs3772_volatile_reg(struct device *dev, unsigned int reg)
{
	return (reg >= TCS3772_REG_ALS_DATA_L) &&
		(reg <= TCS3772_REG_PROX_DATA_H);
}

static const struct reg_default tcs3772_reg_defaults[] = {
	{
		.reg = TCS3772_REG_CONFIGURE,
		.def = 0x00,
	},
	{
		.reg = TCS3772_REG_PPULSE,
		.def = 0x00,
	},
};

static const struct regmap_config tcs3772_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.volatile_reg = &tcs3772_volatile_reg,
	.max_register = TCS3772_REG_MAX,
	.reg_defaults = tcs3772_reg_defaults,
	.num_reg_defaults = 2,
	.cache_type = REGCACHE_RBTREE,
};

/* device's read/write functionality */
static int _tcs3772_register_read(struct tcs3772_chip *chip, int reg_l,
					int nreg, int *val)
{
	int ret;
	int temp, i = 0;

	if (!chip->regmap)
		return -ENODEV;

	ret = wait_event_timeout(chip->i2c_wait_queue,
					chip->i2c_xfer_ready, I2C_MAX_TIMEOUT);
	if (!ret) {
		dev_err(&chip->client->dev, "idname:%s func:%s line:%d " \
				"error_msg: device not ready for i2c xfer\n",
				chip->id->name, __func__, __LINE__);
		return -ETIMEDOUT;
	}

	mutex_lock(&chip->lock);
	*val = 0;
	while (i < nreg) {
		ret = regmap_read(chip->regmap, reg_l + i, &temp);
		if (ret)
			dev_err(&chip->client->dev,
					"idname:%s func:%s line:%d i:%d" \
					"error_msg:regmap_read fails\n",
					chip->id->name, __func__, __LINE__, i);
		*val |= temp << (i * 8);
		i++;
	}
	mutex_unlock(&chip->lock);
	return ret;
}

static int _tcs3772_register_write(struct tcs3772_chip *chip, int reg, int mask,
				int val)
{
	int ret;

	if (!chip->regmap)
		return -ENODEV;

	ret = wait_event_timeout(chip->i2c_wait_queue,
					chip->i2c_xfer_ready, I2C_MAX_TIMEOUT);
	if (!ret) {
		dev_err(&chip->client->dev, "idname:%s func:%s line:%d " \
				"error_msg: device not ready for i2c xfer\n",
				chip->id->name, __func__, __LINE__);
		return -ETIMEDOUT;
	}

	mutex_lock(&chip->lock);
	ret = regmap_update_bits(chip->regmap, reg, mask, val);
	if (ret)
		dev_err(&chip->client->dev, "idname:%s func:%s line:%d " \
				"error_msg:regmap_write fails\n",
				chip->id->name, __func__, __LINE__);
	mutex_unlock(&chip->lock);

	return ret;
}

/* sync the device's registers with cache after power up during resume */
static int _tcs3772_register_sync(struct tcs3772_chip *chip)
{
	int ret;

	if (!chip->regmap)
		return -ENODEV;

	ret = wait_event_timeout(chip->i2c_wait_queue,
					chip->i2c_xfer_ready, I2C_MAX_TIMEOUT);
	if (!ret) {
		dev_err(&chip->client->dev, "idname:%s func:%s line:%d " \
				"error_msg: device not ready for i2c xfer\n",
				chip->id->name, __func__, __LINE__);
		return -ETIMEDOUT;
	}

	mutex_lock(&chip->lock);
	regcache_mark_dirty(chip->regmap);
	ret = regcache_sync(chip->regmap);
	if (ret)
		dev_err(&chip->client->dev, "idname:%s func:%s line:%d " \
				"error_msg:regmap_write fails\n",
				chip->id->name, __func__, __LINE__);
	mutex_unlock(&chip->lock);

	return ret;
}

/* device's registration with iio to facilitate user operations */
static ssize_t tcs3772_chan_regulator_enable(struct iio_dev *indio_dev,
		uintptr_t private, struct iio_chan_spec const *chan,
		const char *buf, size_t len)
{
	u8 enable;
	int ret = 0;
	struct tcs3772_chip *chip = iio_priv(indio_dev);

	if (kstrtou8(buf, 10, &enable))
		return -EINVAL;

	if ((enable != 0) && (enable != 1))
		return -EINVAL;

	if (get_chan_num(chan->type) == -EINVAL)
		return -EINVAL;

	if (enable == (chip->state[get_chan_num(chan->type)] != CHIP_POWER_OFF))
		return 1;

	if (!chip->consumers)
		goto success;

	if (enable)
		ret = regulator_bulk_enable(1, chip->consumers);
	else
		ret = regulator_bulk_disable(1, chip->consumers);

	if (ret) {
		dev_err(&chip->client->dev, "idname:%s func:%s line:%d " \
				"error_msg:_tcs3772_register_read fails\n",
				chip->id->name, __func__, __LINE__);
		goto fail;
	}

success:
	chip->state[get_chan_num(chan->type)] = enable;
fail:
	return ret ? ret : 1;
}

static ssize_t tcs3772_chan_enable(struct iio_dev *indio_dev,
		uintptr_t private, struct iio_chan_spec const *chan,
		const char *buf, size_t len)
{
	u8 enable;
	int ret;
	struct tcs3772_chip *chip = iio_priv(indio_dev);
	int state = chip->state[get_chan_num(chan->type)];

	if (kstrtou8(buf, 10, &enable))
		return -EINVAL;

	if ((enable != 0) && (enable != 1))
		return -EINVAL;

	if (get_chan_num(chan->type) == -EINVAL)
		return -EINVAL;

	if (state == CHIP_POWER_OFF) {
		dev_err(&chip->client->dev, "idname:%s func:%s line:%d " \
				"error_msg:please enable regulator first\n",
				chip->id->name, __func__, __LINE__);
		return -EINVAL;
	}

	if (!((enable && (state == CHIP_POWER_ON_CHAN_OFF)) ||
		(!enable && (state == CHIP_POWER_ON_CHAN_ON))))
		return -EINVAL;

	/* a small optimization*/
	if (enable == (state - 1))
		goto success;

	if (chan->type == IIO_LIGHT) {
		if (enable) {
			ret = _tcs3772_register_write(chip,
							TCS3772_REG_CONFIGURE,
							CONFIGURE_ALS_MASK,
							CONFIGURE_ALS_EN);
			if (ret)
				return ret;
		} else {
			if (chip->state[PROX] == CHIP_POWER_ON_CHAN_ON) {
				ret = _tcs3772_register_write(chip,
						TCS3772_REG_CONFIGURE,
						CONFIGURE_ALS_MASK_PROX_ON,
						!CONFIGURE_ALS_EN);
				if (ret)
					return ret;
			} else {
				ret = _tcs3772_register_write(chip,
							TCS3772_REG_CONFIGURE,
							CONFIGURE_ALS_MASK,
							!CONFIGURE_ALS_EN);
				if (ret)
					return ret;
			}
		}
	} else {
	/* chan->type == IIO_PROXIMITY */
		if (enable) {
			ret = _tcs3772_register_write(chip,
							TCS3772_REG_CONFIGURE,
							CONFIGURE_PROX_MASK,
							CONFIGURE_PROX_EN);
			if (ret)
				return ret;
			ret = _tcs3772_register_write(chip, TCS3772_REG_PPULSE,
							PPULSE_MASK,
							PPULSE_NUM);
			if (ret)
				return ret;
		} else {
			if (chip->state[ALS] == CHIP_POWER_ON_CHAN_ON) {
				ret = _tcs3772_register_write(chip,
						TCS3772_REG_CONFIGURE,
						CONFIGURE_PROX_MASK_ALS_ON,
						!CONFIGURE_PROX_EN);
				if (ret)
					return ret;
			} else {
				ret = _tcs3772_register_write(chip,
							TCS3772_REG_CONFIGURE,
							CONFIGURE_PROX_MASK,
							!CONFIGURE_PROX_EN);
				if (ret)
					return ret;
			}
		}
	}

success:
	/* success on enable = 1 => state = CHIP_POWER_ON_CHAN_ON (2)
	 * success on enable = 0 => state = CHIP_POWER_ON_CHAN_OFF (1)
	 * from enum channel_state. Hence a small optimization */
	chip->state[get_chan_num(chan->type)] = enable + 1;
	return ret ? ret : 1;
}

/* chan_regulator_enable is used to enable regulators used by
 * particular channel.
 * chan_enable actually configures various registers to activate
 * a particular channel.
 */
static const struct iio_chan_spec_ext_info tcs3772_ext_info[] = {
	{
		.name = "regulator_enable",
		.shared = true,
		.write = tcs3772_chan_regulator_enable,
	},
	{
		.name = "enable",
		.shared = true,
		.write = tcs3772_chan_enable,
	},
	{
	},
};

static const struct iio_chan_spec tcs3772_channels[] = {
	{
		.type = IIO_LIGHT,
		.info_mask = IIO_CHAN_INFO_RAW_SEPARATE_BIT,
		.ext_info = tcs3772_ext_info,
	},
	{
		.type = IIO_PROXIMITY,
		.info_mask = IIO_CHAN_INFO_RAW_SEPARATE_BIT,
		.ext_info = tcs3772_ext_info,
	},
};

static int tcs3772_read_raw(struct iio_dev *indio_dev,
	struct iio_chan_spec const *chan, int *val, int *val2, long mask)
{
	struct tcs3772_chip *chip = iio_priv(indio_dev);
	int ret;
	int value;

	/* get_chan_num != -EINVAL <=> get_data_reg != -EINVAL */
	if (get_chan_num(chan->type) == -EINVAL)
		return -EINVAL;

	if (chip->state[get_chan_num(chan->type)] != CHIP_POWER_ON_CHAN_ON)
		return -EINVAL;

	do {
		ret = _tcs3772_register_read(chip, TCS3772_REG_STATUS, 1,
						&value);
		if (ret) {
			dev_err(&chip->client->dev,
				"idname:%s func:%s line:%d " \
				"error_msg:_tcs3772_register_read fails\n",
				chip->id->name, __func__, __LINE__);
			return ret;
		}
		msleep(TCS3772_POLL_DELAY);
	} while (!(value | get_valid_mask(chan->type)));

	ret = _tcs3772_register_read(chip, get_data_reg(chan->type), 2, &value);
	if (ret)
		dev_err(&chip->client->dev, "idname:%s func:%s line:%d " \
			"error_msg:_tcs3772_register_read fails\n",
			chip->id->name, __func__, __LINE__);

	if (!ret) {
		*val = value;
		ret = IIO_VAL_INT;
	}
	return ret;
}

/* read_raw is used to report a channel's data to user
 * in non SI units
 */
static const struct iio_info tcs3772_iio_info = {
	.driver_module = THIS_MODULE,
	.read_raw = &tcs3772_read_raw,
};

/* chip's power management helpers */
static int tcs3772_activate_standby_mode(struct tcs3772_chip *chip)
{
	int ret;
	ret = _tcs3772_register_write(chip, TCS3772_REG_CONFIGURE,
					CONFIGURE_SHTDWN_MASK,
					CONFIGURE_SHTDWN_EN);
	return 0;
}

/* this detects the regulator enable/disable event and puts
 * the device to low power state if this device does not use the regulator */
static int tcs3772_power_manager(struct notifier_block *regulator_nb,
				unsigned long event, void *v)
{
	struct tcs3772_chip *chip;

	chip = container_of(regulator_nb, struct tcs3772_chip, regulator_nb);

	if (event & (REGULATOR_EVENT_POST_ENABLE |
			REGULATOR_EVENT_OUT_POSTCHANGE)) {
		chip->i2c_xfer_ready = 1;
		tcs3772_activate_standby_mode(chip);
	} else if (event & (REGULATOR_EVENT_DISABLE |
			REGULATOR_EVENT_FORCE_DISABLE)) {
		chip->i2c_xfer_ready = 0;
	}
	return NOTIFY_OK;
}

#ifdef CONFIG_PM_SLEEP
static int tcs3772_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct tcs3772_chip *chip = iio_priv(indio_dev);
	int ret = 0, i;

	if (!chip->consumers)
		return 0;

	/* assumes all other devices stop using this regulator */
	for (i = 0; i < TCS3772_N_CHANNELS; i++)
		if (chip->state[i] != CHIP_POWER_OFF)
			ret |= regulator_bulk_disable(1, chip->consumers);

	if (ret) {
		dev_err(&chip->client->dev, "idname:%s func:%s line:%d " \
				"error_msg:regulator_bulk_disable fails\n",
				chip->id->name, __func__, __LINE__);
		return ret;
	}

	if (regulator_is_enabled(chip->consumers[0].consumer))
		ret = tcs3772_activate_standby_mode(chip);
	if (ret)
		dev_err(&chip->client->dev, "idname:%s func:%s line:%d " \
				"error_msg:tcs3772_activate_standby fails\n",
				chip->id->name, __func__, __LINE__);
	return ret;
}

static int tcs3772_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct tcs3772_chip *chip = iio_priv(indio_dev);
	int ret = 0, i;

	for (i = 0; i < TCS3772_N_CHANNELS; i++)
		if (chip->state[i] != CHIP_POWER_OFF)
			ret |= regulator_bulk_enable(1, tcs3772_consumers);

	if (ret) {
		dev_err(&chip->client->dev, "idname:%s func:%s line:%d " \
				"error_msg:regulator_bulk_enable fails\n",
				chip->id->name, __func__, __LINE__);
		return ret;
	}

	if (chip->state[ALS] == CHIP_POWER_ON_CHAN_ON ||
		chip->state[PROX] == CHIP_POWER_ON_CHAN_ON) {
			ret = _tcs3772_register_sync(chip);
			if (ret)
				dev_err(&chip->client->dev,
					"idname:%s func:%s line:%d " \
					"error_msg:restore_state fails\n",
					chip->id->name, __func__, __LINE__);
	}
	return ret;
}

static SIMPLE_DEV_PM_OPS(tcs3772_pm_ops, tcs3772_suspend, tcs3772_resume);
#define TCS3772_PM_OPS (&tcs3772_pm_ops)
#else
#define TCS3772_PM_OPS NULL
#endif

/* device's i2c registration */
static int tcs3772_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int ret = 0;
	struct tcs3772_chip *chip;
	struct iio_dev *indio_dev;
	struct regmap *regmap;

	indio_dev = iio_device_alloc(sizeof(*chip));
	if (indio_dev == NULL) {
		dev_err(&client->dev, "idname:%s func:%s line:%d " \
			"error_msg:iio_device_alloc fails\n",
			id->name, __func__, __LINE__);
		return -ENOMEM;
	}
	chip = iio_priv(indio_dev);

	i2c_set_clientdata(client, indio_dev);
	chip->client = client;
	mutex_init(&chip->lock);

	regmap = devm_regmap_init_i2c(client, &tcs3772_regmap_config);
	if (IS_ERR_OR_NULL(regmap)) {
		dev_err(&client->dev, "idname:%s func:%s line:%d " \
		"error_msg:devm_regmap_init_i2c fails\n",
		id->name, __func__, __LINE__);
		return -ENOMEM;
	}
	chip->regmap = regmap;

	ret = devm_regulator_bulk_get(&client->dev,
					ARRAY_SIZE(tcs3772_consumers),
					tcs3772_consumers);
	if (ret)
		dev_err(&client->dev, "idname:%s func:%s line:%d " \
			"error_msg:regulator_get fails\n",
			id->name, __func__, __LINE__);
	else
		chip->consumers = tcs3772_consumers;

	if (chip->consumers) {
		chip->regulator_nb.notifier_call = tcs3772_power_manager;
		ret = regulator_register_notifier(chip->consumers[0].consumer,
						&chip->regulator_nb);
		if (ret)
			dev_err(&client->dev, "idname:%s func:%s line:%d " \
				"error_msg:regulator_register_notifier fails\n",
				id->name, __func__, __LINE__);
	}

	indio_dev->info = &tcs3772_iio_info;
	indio_dev->channels = tcs3772_channels;
	indio_dev->num_channels = 2;
	indio_dev->name = id->name;
	indio_dev->dev.parent = &client->dev;
	indio_dev->modes = INDIO_DIRECT_MODE;
	ret = iio_device_register(indio_dev);
	if (ret) {
		dev_err(&client->dev, "idname:%s func:%s line:%d " \
			"error_msg:iio_device_register fails\n",
			id->name, __func__, __LINE__);
		goto err_iio_free;
	}

	init_waitqueue_head(&chip->i2c_wait_queue);
	chip->state[ALS] = CHIP_POWER_OFF;
	chip->state[PROX] = CHIP_POWER_OFF;

	if (regulator_is_enabled(chip->consumers[0].consumer)) {
		chip->i2c_xfer_ready = 1;
		tcs3772_activate_standby_mode(chip);
	}

	dev_info(&client->dev, "idname:%s func:%s line:%d " \
			"probe success\n",
			id->name, __func__, __LINE__);

	return 0;

err_iio_free:
	mutex_destroy(&chip->lock);
	iio_device_free(indio_dev);
	return ret;
}

static void tcs3772_shutdown(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct tcs3772_chip *chip = iio_priv(indio_dev);
	mutex_lock(&chip->lock);
	chip->shutdown_complete = 1;
	mutex_unlock(&chip->lock);
}

static int tcs3772_remove(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct tcs3772_chip *chip = iio_priv(indio_dev);

	mutex_destroy(&chip->lock);
	iio_device_unregister(indio_dev);
	iio_device_free(indio_dev);
	return 0;
}

static const struct i2c_device_id tcs3772_id[] = {
	{"tcs3772", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, tcs3772_id);

static const struct of_device_id tcs3772_of_match[] = {
	{ .compatible = "taos,tcs3772", },
	{ },
};
MODULE_DEVICE_TABLE(of, tcs3772_of_match);

static struct i2c_driver tcs3772_driver = {
	.class = I2C_CLASS_HWMON,
	.driver = {
		.name = "tcs3772",
		.owner = THIS_MODULE,
		.of_match_table = tcs3772_of_match,
		.pm = TCS3772_PM_OPS,
	},
	.id_table = tcs3772_id,
	.probe = tcs3772_probe,
	.remove = tcs3772_remove,
	.shutdown = tcs3772_shutdown,
};

static int __init tcs3772_init(void)
{
	return i2c_add_driver(&tcs3772_driver);
}

static void __exit tcs3772_exit(void)
{
	i2c_del_driver(&tcs3772_driver);
}

module_init(tcs3772_init);
module_exit(tcs3772_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("tcs3772 Driver");
MODULE_AUTHOR("Sri Krishna chowdary <schowdary@nvidia.com>");
