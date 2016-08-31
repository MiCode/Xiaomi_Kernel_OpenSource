/*
 * A iio driver for the light sensor JSA-1127.
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
#include <linux/notifier.h>
#include <linux/regulator/consumer.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/bitops.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/iio/light/jsa1127.h>
#include <linux/random.h>

#define DEV_ERR(err_string) \
	dev_err(&chip->client->dev, \
		"idname:%s func:%s line:%d %s\n", \
		chip->id->name, __func__, __LINE__, err_string)

#define JSA1127_VENDOR			"Solteam-opto"

enum als_state {
	CHIP_POWER_OFF,
	CHIP_POWER_ON_ALS_OFF,
	CHIP_POWER_ON_ALS_ON,
};

#define JSA1127_OPMODE_CONTINUOUS		0x0C
#define JSA1127_ONE_TIME_INTEGRATION_OPMODE	0x04
#define JSA1127_CMD_START_INTERGATION		0x08
#define JSA1127_CMD_STOP_INTERGATION		0x30
#define JSA1127_CMD_STANDBY			0x8C
#define JSA1127_POWER_ON_DELAY			60 /* msec */

struct jsa1127_chip {
	struct i2c_client		*client;
	const struct i2c_device_id	*id;
	struct regulator		*regulator;

	int				rint;
	int				integration_time;
	int				noisy;

	struct workqueue_struct		*wq;
	struct delayed_work		dw;

	bool				use_internal_integration_timing;
	u8				als_state;
	u16				als_raw_value;
	u16				tint_coeff;
};

#define N_DATA_BYTES				2
#define RETRY_COUNT				3
#define JSA1127_RETRY_TIME			100 /* msec */
#define JSA1127_VALID_MASK			BIT(15)
#define JSA1127_DATA_MASK			(JSA1127_VALID_MASK - 1)
#define JSA1127_IS_DATA_VALID(val)		(val & JSA1127_VALID_MASK)
#define JSA1127_CONV_TO_DATA(val)		(val & JSA1127_DATA_MASK)
/* returns 0 on success, -errno on failure*/
static int jsa1127_try_update_als_reading_locked(struct jsa1127_chip *chip)
{
	int ret = 0;
	u16 val = 0;
	char buf[N_DATA_BYTES] = {0, 0};
	int retry_count = RETRY_COUNT;
	struct iio_dev *indio_dev = iio_priv_to_dev(chip);
	unsigned char rndnum;

	mutex_lock(&indio_dev->mlock);
	do {
		ret = i2c_master_recv(chip->client, buf, N_DATA_BYTES);
		if (ret != N_DATA_BYTES) {
			DEV_ERR("i2c_master_recv failed");
		} else {
			val = buf[1];
			val = (val << 8) | buf[0];
			if (JSA1127_IS_DATA_VALID(val)) {
				chip->als_raw_value = JSA1127_CONV_TO_DATA(val);
				if (chip->noisy) {
					get_random_bytes(&rndnum, 1);
					if (rndnum < 128)
						chip->als_raw_value++;
				}
				break;
			} else {
				msleep(JSA1127_RETRY_TIME);
				DEV_ERR("data invalid");
				ret = -EINVAL;
			}
		}
	} while (!JSA1127_IS_DATA_VALID(val) && (--retry_count));
	mutex_unlock(&indio_dev->mlock);

	return ret == N_DATA_BYTES ? 0 : ret;
}
#undef N_DATA_BYTES
#undef RETRY_COUNT
#undef JSA1127_RETRY_TIME
#undef JSA1127_VALID_MASK
#undef JSA1127_DATA_MASK
#undef JSA1127_IS_DATA_VALID
#undef JSA1127_CONV_TO_DATA

#define N_MSG_SEND	1
static int jsa1127_send_cmd_locked(struct jsa1127_chip *chip, char command)
{
	int ret = -EAGAIN;
	struct iio_dev *indio_dev = iio_priv_to_dev(chip);
	char cmd[N_MSG_SEND];
	cmd[0] = command;
	mutex_lock(&indio_dev->mlock);
	while (ret == -EAGAIN)
		ret = i2c_master_send(chip->client, cmd, N_MSG_SEND);
	if (ret != N_MSG_SEND)
		dev_err(&chip->client->dev,
			"idname:%s func:%s line:%d i2c_master_send fails\n",
			chip->id->name, __func__, __LINE__);
	mutex_unlock(&indio_dev->mlock);

	return ret == N_MSG_SEND ? 0 : ret;
}
#undef N_MSG_SEND

/* device's registration with iio to facilitate user operations */
static ssize_t jsa1127_chan_regulator_enable(struct iio_dev *indio_dev,
		uintptr_t priv, struct iio_chan_spec const *chan,
		const char *buf, size_t len)
{
	u8 enable;
	int ret = 0;
	struct jsa1127_chip *chip = iio_priv(indio_dev);

	if (kstrtou8(buf, 10, &enable))
		return -EINVAL;

	if ((enable != 0) && (enable != 1))
		return -EINVAL;

	if (chan->type != IIO_LIGHT)
		return -EINVAL;

	if (enable == (chip->als_state != CHIP_POWER_OFF))
		goto success;

	if (!chip->regulator)
		goto success;

	if (enable)
		ret = regulator_enable(chip->regulator);
	else
		ret = regulator_disable(chip->regulator);

	if (ret) {
		dev_err(&chip->client->dev,
		"idname:%s func:%s line:%d _jsa1127_register_read fails\n",
		chip->id->name, __func__, __LINE__);
		goto fail;
	}

success:
	chip->als_state = enable;
fail:
	return ret ? ret : 1;
}

static ssize_t jsa1127_chan_enable(struct iio_dev *indio_dev,
		uintptr_t priv, struct iio_chan_spec const *chan,
		const char *buf, size_t len)
{
	u8 enable;
	int ret = 0;
	struct jsa1127_chip *chip = iio_priv(indio_dev);

	if (kstrtou8(buf, 10, &enable))
		return -EINVAL;

	if ((enable != 0) && (enable != 1))
		return -EINVAL;

	if (chip->als_state == CHIP_POWER_OFF) {
		dev_err(&chip->client->dev,
			"idname:%s func:%s line:%d please enable regulator first\n",
			chip->id->name, __func__, __LINE__);
		return -EINVAL;
	}

	if (chan->type != IIO_LIGHT)
		return -EINVAL;

	if (enable) {
		chip->als_raw_value = 0;
		chip->als_state = CHIP_POWER_ON_ALS_ON;
		queue_delayed_work(chip->wq, &chip->dw, JSA1127_POWER_ON_DELAY);
	} else {
		cancel_delayed_work_sync(&chip->dw);
		chip->als_state = CHIP_POWER_ON_ALS_OFF;
	}

	return ret ? ret : 1;
}

/* chan_regulator_enable is used to enable regulators used by
 * particular channel.
 * chan_enable actually configures various registers to activate
 * a particular channel.
 */
static const struct iio_chan_spec_ext_info jsa1127_ext_info[] = {
	{
		.name = "regulator_enable",
		.write = jsa1127_chan_regulator_enable,
	},
	{
		.name = "enable",
		.write = jsa1127_chan_enable,
	},
	{
	},
};

static const struct iio_chan_spec jsa1127_channels[] = {
	{
		.type = IIO_LIGHT,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
		.ext_info = jsa1127_ext_info,
	},
};

static int jsa1127_read_raw(struct iio_dev *indio_dev,
	struct iio_chan_spec const *chan, int *val, int *val2, long mask)
{
	struct jsa1127_chip *chip = iio_priv(indio_dev);
	int ret = -EINVAL;

	if (chan->type != IIO_LIGHT)
		return -EINVAL;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		if (chip->als_state != CHIP_POWER_ON_ALS_ON)
			return -EINVAL;

		if (chip->als_raw_value != -EINVAL) {
			*val = chip->als_raw_value;
			ret = IIO_VAL_INT;
		}

		queue_delayed_work(chip->wq, &chip->dw, 0);
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}


/* integration time in msec corresponding to resistor RINT */
static ssize_t jsa1127_integration_time(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct jsa1127_chip *chip = iio_priv(indio_dev);

	return sprintf(buf, "%d\n", chip->integration_time);
}

/* max detection range in lux corresponding to ressistor RINT
 * units = lux */
static ssize_t jsa1127_max_range(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct jsa1127_chip *chip = iio_priv(indio_dev);
	int max_range = 0;

	if (chip->rint == 50)
		max_range = 109000;
	else if (chip->rint == 100)
		max_range = 54000;
	else if (chip->rint == 200)
		max_range = 27000;
	else if (chip->rint == 400)
		max_range = 13000;
	else if (chip->rint == 800)
		max_range = 6500;
	else
		DEV_ERR("invalid RINT");
	return sprintf(buf, "%d\n", max_range);
}


/* resolution in lux/count corresponding to ressistor RINT
 * units = mLux/lsb */
static ssize_t jsa1127_resolution(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct jsa1127_chip *chip = iio_priv(indio_dev);
	int resolution = 0;

	if (chip->rint == 50)
		resolution = 3330;
	else if (chip->rint == 100)
		resolution = 1670;
	else if (chip->rint == 200)
		resolution = 830;
	else if (chip->rint == 400)
		resolution = 420;
	else if (chip->rint == 800)
		resolution = 210;
	else
		DEV_ERR("invalid RINT");
	return sprintf(buf, "%d\n", resolution * chip->tint_coeff);
}

#define JSA1127_POWER_CONSUMED		"1.65" /* mWatt */

static IIO_CONST_ATTR(vendor, JSA1127_VENDOR);
static IIO_DEVICE_ATTR(in_illuminance_resolution, S_IRUGO,
			jsa1127_resolution, NULL, 0);
static IIO_DEVICE_ATTR(in_illuminance_integration_time, S_IRUGO,
			jsa1127_integration_time, NULL, 0);
static IIO_CONST_ATTR(in_illuminance_power_consumed, JSA1127_POWER_CONSUMED);
static IIO_DEVICE_ATTR(in_illuminance_max_range, S_IRUGO,
			jsa1127_max_range, NULL, 0);

static struct attribute *jsa1127_attributes[] = {
	&iio_const_attr_vendor.dev_attr.attr,
	&iio_dev_attr_in_illuminance_resolution.dev_attr.attr,
	&iio_dev_attr_in_illuminance_integration_time.dev_attr.attr,
	&iio_const_attr_in_illuminance_power_consumed.dev_attr.attr,
	&iio_dev_attr_in_illuminance_max_range.dev_attr.attr,
	NULL,
};

static const struct attribute_group jsa1127_attr_group = {
	.attrs = jsa1127_attributes,
};

/* read_raw is used to report a channel's data to user
 * in non SI units
 */
static const struct iio_info jsa1127_iio_info = {
	.attrs = &jsa1127_attr_group,
	.driver_module = THIS_MODULE,
	.read_raw = jsa1127_read_raw,
};

static void jsa1127_work_func(struct work_struct *ws)
{
	struct jsa1127_chip *chip = container_of(ws,
					struct jsa1127_chip, dw.work);
	int ret = 0;

	if (chip->als_state != CHIP_POWER_ON_ALS_ON)
		return;

	if (chip->use_internal_integration_timing) {
		ret = jsa1127_send_cmd_locked(chip, JSA1127_OPMODE_CONTINUOUS);
		if (ret)
			goto fail;
		msleep(chip->integration_time);
	} else {
		ret = jsa1127_send_cmd_locked(chip,
					JSA1127_ONE_TIME_INTEGRATION_OPMODE);
		if (ret)
			goto fail;
		ret = jsa1127_send_cmd_locked(chip,
						JSA1127_CMD_START_INTERGATION);
		if (ret)
			goto fail;
		msleep(chip->integration_time);
		ret = jsa1127_send_cmd_locked(chip,
						JSA1127_CMD_STOP_INTERGATION);
		if (ret)
			goto fail;
	}
	ret = jsa1127_try_update_als_reading_locked(chip);
	if (ret)
		goto fail;
	jsa1127_send_cmd_locked(chip, JSA1127_CMD_STANDBY);
	return;
fail:
	chip->als_raw_value = -EINVAL;
}

#if 0
static int jsa1127_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct jsa1127_chip *chip = iio_priv(indio_dev);
	int ret = 0;

	if (chip->regulator && (chip->als_state != CHIP_POWER_OFF))
		ret = regulator_disable(chip->regulator);

	if (ret) {
		DEV_ERR("regulator_disable fails");
		return ret;
	}

	if (!chip->regulator || regulator_is_enabled(chip->regulator))
		jsa1127_send_cmd_locked(chip, JSA1127_CMD_STANDBY);
	return ret;
}

static int jsa1127_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct jsa1127_chip *chip = iio_priv(indio_dev);
	int ret = 0;

	if (chip->als_state == CHIP_POWER_OFF)
		return 0;

	ret = regulator_enable(chip->regulator);
	if (ret) {
		DEV_ERR("regulator_bulk_enable fails");
		return ret;
	}

	mutex_lock(&indio_dev->mlock);
	queue_delayed_work(chip->wq, &chip->dw, 0);
	mutex_unlock(&indio_dev->mlock);
	return ret;
}

static SIMPLE_DEV_PM_OPS(jsa1127_pm_ops, jsa1127_suspend, jsa1127_resume);
#define JSA1127_PM_OPS (&jsa1127_pm_ops)
#else
#define JSA1127_PM_OPS NULL
#endif

/* device's i2c registration */
static int jsa1127_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int ret = 0;
	struct jsa1127_chip *chip;
	struct iio_dev *indio_dev;
	struct jsa1127_platform_data *jsa1127_platform_data;
	u32 rint = UINT_MAX, use_internal_integration_timing = UINT_MAX;
	u32 integration_time = UINT_MAX;
	u32 tint = UINT_MAX;
	u32 noisy = UINT_MAX;

	if (client->dev.of_node) {
		of_property_read_u32(client->dev.of_node,
				"solteam-opto,rint", &rint);
		of_property_read_u32(client->dev.of_node,
				"solteam-opto,integration-time",
				&integration_time);
		of_property_read_u32(client->dev.of_node,
				"solteam-opto,use-internal-integration-timing",
				&use_internal_integration_timing);
		of_property_read_u32(client->dev.of_node,
				"solteam-opto,tint-coeff", &tint);
		of_property_read_u32(client->dev.of_node,
				"solteam-opto,noisy", &noisy);
	} else {
		jsa1127_platform_data = client->dev.platform_data;
		rint = jsa1127_platform_data->rint;
		integration_time = jsa1127_platform_data->integration_time;
		use_internal_integration_timing =
			jsa1127_platform_data->use_internal_integration_timing;
		tint =
			jsa1127_platform_data->tint_coeff;
		noisy =
			jsa1127_platform_data->noisy;
	}

	if ((rint == UINT_MAX) ||
		(use_internal_integration_timing == UINT_MAX) ||
		(rint%50 != 0) || (tint == UINT_MAX)) {
		pr_err("func:%s failed due to invalid platform data", __func__);
		return -EINVAL;
	}

	indio_dev = iio_device_alloc(sizeof(*chip));
	if (indio_dev == NULL) {
		dev_err(&client->dev,
			"idname:%s func:%s line:%d iio_allocate_device fails\n",
			id->name, __func__, __LINE__);
		return -ENOMEM;
	}
	chip = iio_priv(indio_dev);

	chip->rint = rint;
	chip->integration_time = integration_time;
	chip->use_internal_integration_timing = use_internal_integration_timing;
	chip->tint_coeff = tint;
	chip->noisy = noisy;

	i2c_set_clientdata(client, indio_dev);
	chip->client = client;

	indio_dev->info = &jsa1127_iio_info;
	indio_dev->channels = jsa1127_channels;
	indio_dev->num_channels = 1;
	indio_dev->name = id->name;
	indio_dev->dev.parent = &client->dev;
	indio_dev->modes = INDIO_DIRECT_MODE;
	ret = iio_device_register(indio_dev);
	if (ret) {
		dev_err(&client->dev,
			"idname:%s func:%s line:%d iio_device_register fails\n",
			id->name, __func__, __LINE__);
		goto free_iio_dev;
	}

	chip->wq = alloc_workqueue(id->name, WQ_FREEZABLE |
					WQ_NON_REENTRANT | WQ_UNBOUND, 1);
	INIT_DELAYED_WORK(&chip->dw, jsa1127_work_func);

	chip->regulator = devm_regulator_get(&client->dev, "vdd");
	if (IS_ERR(chip->regulator)) {
		dev_info(&client->dev,
			"idname:%s func:%s line:%d regulator not found.\n"
			"Assuming regulator is not needed\n",
			id->name, __func__, __LINE__);
		chip->regulator = NULL;
		goto finish;
	}

	if (regulator_is_enabled(chip->regulator))
		jsa1127_send_cmd_locked(chip, JSA1127_CMD_STANDBY);
	if (ret)
		goto destroy_wq;

finish:
	chip->als_state = CHIP_POWER_OFF;
	chip->id = id;
	dev_info(&client->dev, "idname:%s func:%s line:%d probe success\n",
			id->name, __func__, __LINE__);
	return 0;

destroy_wq:
	destroy_workqueue(chip->wq);
	iio_device_unregister(indio_dev);
free_iio_dev:
	iio_device_free(indio_dev);
	return ret;
}

/* no need for any additional shutdown flag as
 * by now workqueue will not schedule */
static void jsa1127_shutdown(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct jsa1127_chip *chip = iio_priv(indio_dev);
	int ret;

	if (chip->regulator && (chip->als_state != CHIP_POWER_OFF))
		regulator_disable(chip->regulator);

	if (!chip->regulator || regulator_is_enabled(chip->regulator))
		ret = jsa1127_send_cmd_locked(chip, JSA1127_CMD_STANDBY);

	destroy_workqueue(chip->wq);
	iio_device_unregister(indio_dev);
	iio_device_free(indio_dev);
}

static int jsa1127_remove(struct i2c_client *client)
{
	jsa1127_shutdown(client);
	return 0;
}
#undef SEND

static const struct i2c_device_id jsa1127_id[] = {
	{"jsa1127", 0},
	{ }
};
MODULE_DEVICE_TABLE(i2c, jsa1127_id);

static const struct of_device_id jsa1127_of_match[] = {
	{ .compatible = "solteam-opto,jsa1127", },
	{ },
};
MODULE_DEVICE_TABLE(of, jsa1127_of_match);

static struct i2c_driver jsa1127_driver = {
	.class = I2C_CLASS_HWMON,
	.driver = {
		.name = JSA1127_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(jsa1127_of_match),
		.pm = JSA1127_PM_OPS,
	},
	.id_table = jsa1127_id,
	.probe = jsa1127_probe,
	.remove = jsa1127_remove,
	.shutdown = jsa1127_shutdown,
};
module_i2c_driver(jsa1127_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("jsa1127 Driver");
MODULE_AUTHOR("Sri Krishna chowdary <schowdary@nvidia.com>");
