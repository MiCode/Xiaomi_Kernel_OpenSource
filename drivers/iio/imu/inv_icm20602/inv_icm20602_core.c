/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * Copyright (C) 2012 Invensense, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/jiffies.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kfifo.h>
#include <linux/spinlock.h>
#include <linux/spi/spi.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include "inv_icm20602_iio.h"
#include <linux/regulator/consumer.h>


static struct regulator *reg_ldo;

/* Attribute of icm20602 device init show */
static ssize_t inv_icm20602_init_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return 0;
}

static void inv_icm20602_def_config(struct inv_icm20602_state *st)
{
	struct icm20602_user_config *config = st->config;

	config->user_fps_in_ms = 20;
	config->gyro_lpf = INV_ICM20602_GYRO_LFP_92HZ;
	config->gyro_fsr = ICM20602_GYRO_FSR_1000DPS;
	config->acc_lpf = ICM20602_ACCLFP_99;
	config->acc_fsr = ICM20602_ACC_FSR_4G;
	config->gyro_accel_sample_rate = ICM20602_SAMPLE_RATE_200HZ;
	config->fifo_enabled = true;

}

static void inv_icm20602_load_config(struct inv_icm20602_state *st)
{
	struct icm20602_user_config *config = st->config;

	if (config->user_fps_in_ms == 0)
		inv_icm20602_def_config(st);
	config->fifo_enabled = true;

}

static ssize_t inv_icm20602_init_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int result = MPU_SUCCESS;
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct inv_icm20602_state *st = iio_priv(indio_dev);

	inv_icm20602_load_config(st);
	result |= icm20602_detect(st);
	result |= icm20602_init_device(st);
	icm20602_start_fifo(st);
	if (result)
		return result;

	return count;
}

static IIO_DEVICE_ATTR(
						inv_icm20602_init,
						0644,
						inv_icm20602_init_show,
						inv_icm20602_init_store,
						0);

/* Attribute of gyro lpf base on the enum inv_icm20602_gyro_temp_lpf_e */
static ssize_t inv_gyro_lpf_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct inv_icm20602_state *st = iio_priv(indio_dev);

	return snprintf(buf, 4, "%d\n", st->config->gyro_lpf);
}

static ssize_t inv_gyro_lpf_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct inv_icm20602_state *st = iio_priv(indio_dev);
	struct icm20602_user_config *config = st->config;
	int gyro_lpf;

	if (kstrtoint(buf, 10, &gyro_lpf))
		return -EINVAL;
	if (gyro_lpf > INV_ICM20602_GYRO_LFP_NUM)
		return -EINVAL;
	config->gyro_lpf = gyro_lpf;

	return count;
}
static IIO_DEVICE_ATTR(
						inv_icm20602_gyro_lpf,
						0644,
						inv_gyro_lpf_show,
						inv_gyro_lpf_store,
						0);

/* Attribute of gyro fsr base on enum inv_icm20602_gyro_temp_lpf_e */
static ssize_t inv_gyro_fsr_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct inv_icm20602_state *st = iio_priv(indio_dev);

	return snprintf(buf, 4, "%d\n", st->config->gyro_fsr);
}

static ssize_t inv_gyro_fsr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct inv_icm20602_state *st = iio_priv(indio_dev);
	struct icm20602_user_config *config = st->config;
	int gyro_fsr;

	if (kstrtoint(buf, 10, &gyro_fsr))
		return -EINVAL;
	if (gyro_fsr > ICM20602_GYRO_FSR_NUM)
		return -EINVAL;

	config->gyro_fsr = gyro_fsr;
	return count;
}

static IIO_DEVICE_ATTR(
						inv_icm20602_gyro_fsr,
						0644,
						inv_gyro_fsr_show,
						inv_gyro_fsr_store,
						0);

/* Attribute of self_test */
static ssize_t inv_self_test_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t inv_self_test_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct inv_icm20602_state *st = iio_priv(indio_dev);

	icm20602_self_test(st);
	return count;
}
static IIO_DEVICE_ATTR(
						inv_icm20602_self_test,
						0644,
						inv_self_test_show,
						inv_self_test_store,
						0);

/* Attribute of gyro fsr base on enum inv_icm20602_acc_fsr_e */
static ssize_t inv_gyro_acc_fsr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct inv_icm20602_state *st = iio_priv(indio_dev);

	return snprintf(buf, 4, "%d\n", st->config->acc_fsr);
}

static ssize_t inv_gyro_acc_fsr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct inv_icm20602_state *st = iio_priv(indio_dev);
	struct icm20602_user_config *config = st->config;
	int acc_fsr;

	if (kstrtoint(buf, 10, &acc_fsr))
		return -EINVAL;
	if (acc_fsr > ICM20602_ACC_FSR_NUM)
		return -EINVAL;

	config->acc_fsr = acc_fsr;
	return count;
}
static IIO_DEVICE_ATTR(
						inv_icm20602_acc_fsr,
						0644,
						inv_gyro_acc_fsr_show,
						inv_gyro_acc_fsr_store,
						0);

/* Attribute of gyro fsr base on enum inv_icm20602_acc_lpf_e */
static ssize_t inv_gyro_acc_lpf_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct inv_icm20602_state *st = iio_priv(indio_dev);

	return snprintf(buf, 4, "%d\n", st->config->acc_lpf);
}

static ssize_t inv_gyro_acc_lpf_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct inv_icm20602_state *st = iio_priv(indio_dev);
	struct icm20602_user_config *config = st->config;
	int acc_lpf;

	if (kstrtoint(buf, 10, &acc_lpf))
		return -EINVAL;
	if (acc_lpf > ICM20602_ACCLPF_NUM)
		return -EINVAL;

	config->acc_fsr = acc_lpf;
	return count;
}
static IIO_DEVICE_ATTR(
						inv_icm20602_acc_lpf,
						0644,
						inv_gyro_acc_lpf_show,
						inv_gyro_acc_lpf_store,
						0);

/* Attribute of user_fps_in_ms */
static ssize_t inv_user_fps_in_ms_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct inv_icm20602_state *st = iio_priv(indio_dev);

	return snprintf(buf, 4, "%d\n", st->config->user_fps_in_ms);
}

static ssize_t inv_user_fps_in_ms_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct inv_icm20602_state *st = iio_priv(indio_dev);
	struct icm20602_user_config *config = st->config;
	int user_fps_in_ms;

	if (kstrtoint(buf, 10, &user_fps_in_ms))
		return -EINVAL;
	if (user_fps_in_ms < 10)
		return -EINVAL;

	config->user_fps_in_ms = user_fps_in_ms;
	return count;
}
static IIO_DEVICE_ATTR(
						inv_user_fps_in_ms,
						0644,
						inv_user_fps_in_ms_show,
						inv_user_fps_in_ms_store,
						0);

/* Attribute of gyro_accel_sample_rate */
static ssize_t inv_sampling_frequency_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct inv_icm20602_state *st = iio_priv(indio_dev);

	return snprintf(buf, 4, "%d\n", st->config->gyro_accel_sample_rate);
}

static ssize_t inv_sampling_frequency_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct inv_icm20602_state *st = iio_priv(indio_dev);
	struct icm20602_user_config *config = st->config;
	int gyro_accel_sample_rate;

	if (kstrtoint(buf, 10, &gyro_accel_sample_rate))
		return -EINVAL;
	if (gyro_accel_sample_rate < 10)
		return -EINVAL;

	config->gyro_accel_sample_rate = gyro_accel_sample_rate;
	return count;
}
static IIO_DEV_ATTR_SAMP_FREQ(
						0644,
						inv_sampling_frequency_show,
						inv_sampling_frequency_store);

static struct attribute *inv_icm20602_attributes[] = {
	&iio_dev_attr_inv_icm20602_init.dev_attr.attr,

	&iio_dev_attr_inv_icm20602_gyro_fsr.dev_attr.attr,
	&iio_dev_attr_inv_icm20602_gyro_lpf.dev_attr.attr,

	&iio_dev_attr_inv_icm20602_self_test.dev_attr.attr,

	&iio_dev_attr_inv_icm20602_acc_fsr.dev_attr.attr,
	&iio_dev_attr_inv_icm20602_acc_lpf.dev_attr.attr,

	&iio_dev_attr_sampling_frequency.dev_attr.attr,

	&iio_dev_attr_inv_user_fps_in_ms.dev_attr.attr,
	NULL,
};

#define INV_ICM20602_CHAN(_type, _channel2, _index)                    \
{                                                             \
	.type = _type,                                        \
	.modified = 1,                                        \
	.channel2 = _channel2,                                \
	.info_mask_shared_by_type =  BIT(IIO_CHAN_INFO_SCALE), \
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),         \
	.scan_index = _index,                                 \
	.scan_type = {                                        \
			.sign = 's',                          \
			.realbits = 16,                       \
			.storagebits = 16,                    \
			.shift = 0,                          \
			.endianness = IIO_BE,                 \
	},                                       \
}

static const struct iio_chan_spec inv_icm20602_channels[] = {
	IIO_CHAN_SOFT_TIMESTAMP(INV_ICM20602_SCAN_TIMESTAMP),

	{
		.type = IIO_TEMP,
		.info_mask_separate =  BIT(IIO_CHAN_INFO_RAW)
				| BIT(IIO_CHAN_INFO_OFFSET)
				| BIT(IIO_CHAN_INFO_SCALE),
		.scan_index = INV_ICM20602_SCAN_TEMP,
		.channel2 = IIO_MOD_X,
		.scan_type = {
				.sign = 's',
				.realbits = 16,
				.storagebits = 16,
				.shift = 0,
				.endianness = IIO_BE,
			     },
	},
	INV_ICM20602_CHAN(IIO_ANGL_VEL, IIO_MOD_X, INV_ICM20602_SCAN_GYRO_X),
	INV_ICM20602_CHAN(IIO_ANGL_VEL, IIO_MOD_Y, INV_ICM20602_SCAN_GYRO_Y),
	INV_ICM20602_CHAN(IIO_ANGL_VEL, IIO_MOD_Z, INV_ICM20602_SCAN_GYRO_Z),

	INV_ICM20602_CHAN(IIO_ACCEL, IIO_MOD_X, INV_ICM20602_SCAN_ACCL_X),
	INV_ICM20602_CHAN(IIO_ACCEL, IIO_MOD_Y, INV_ICM20602_SCAN_ACCL_Y),
	INV_ICM20602_CHAN(IIO_ACCEL, IIO_MOD_Z, INV_ICM20602_SCAN_ACCL_Z),
};

static const struct attribute_group inv_icm20602_attribute_group = {
	.attrs = inv_icm20602_attributes,
};

static const struct iio_info icm20602_info = {
	.driver_module = THIS_MODULE,
	.read_raw = NULL,
	.write_raw = NULL,
	.attrs = &inv_icm20602_attribute_group,
	.validate_trigger = inv_icm20602_validate_trigger,
};

static int icm20602_ldo_work(struct inv_icm20602_state *st, bool enable)
{
	int ret = 0;

	if (enable) {
		ret = regulator_set_voltage(reg_ldo,
			ICM20602_LDO_VTG_MIN_UV, ICM20602_LDO_VTG_MAX_UV);
		if (ret)
			pr_err("Failed to request LDO voltage.\n");

		ret = regulator_enable(reg_ldo);
		if (ret)
			pr_err("Failed to enable LDO %d\n", ret);
	} else {
		ret = regulator_disable(reg_ldo);
		regulator_set_load(reg_ldo, 0);
		if (ret)
			pr_err("Failed to disable LDO %d\n", ret);
	}

	return MPU_SUCCESS;
}

static int icm20602_init_regulators(struct inv_icm20602_state *st)
{
	struct regulator *reg;
	reg = regulator_get(&st->client->dev, "vdd-ldo");
	if (IS_ERR_OR_NULL(reg)) {
		pr_err("Unable to get regulator for LDO\n");
		return -MPU_FAIL;
	}

	reg_ldo = reg;

	return MPU_SUCCESS;
}

static int of_populate_icm20602_dt(struct inv_icm20602_state *st)
{
	int result = MPU_SUCCESS;

	/* use client device irq */
	st->gpio = of_get_named_gpio(st->client->dev.of_node,
			"invensense,icm20602-gpio", 0);
	result = gpio_is_valid(st->gpio);
	if (!result) {
		pr_err("gpio_is_valid %d failed\n", st->gpio);
		return -MPU_FAIL;
	}

	result = gpio_request(st->gpio, "icm20602-irq");
	if (result) {
		pr_err("gpio_request failed\n");
		return -MPU_FAIL;
	}

	result = gpio_direction_input(st->gpio);
	if (result) {
		pr_err("gpio_direction_input failed\n");
		return -MPU_FAIL;
	}

	st->client->irq = gpio_to_irq(st->gpio);
	if (st->client->irq < 0) {
		pr_err("gpio_to_irq failed\n");
		return -MPU_FAIL;
	}

	return MPU_SUCCESS;
}

/*
 *  inv_icm20602_probe() - probe function.
 *  @client:          i2c client.
 *  @id:              i2c device id.
 *
 *  Returns 0 on success, a negative error code otherwise.
 *  The I2C address of the ICM-20602 is 0x68 or 0x69
 *  depending upon the value driven on AD0 pin.
 */
static int inv_icm20602_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct inv_icm20602_state *st;
	struct iio_dev *indio_dev;
	int result = MPU_SUCCESS;

	indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*st));
	if (indio_dev == NULL) {
		result =  -ENOMEM;
		goto out_remove_trigger;
	}
	st = iio_priv(indio_dev);
	st->client = client;
	st->interface = ICM20602_I2C;

	pr_debug("i2c address is %x\n", client->addr);
	result = of_populate_icm20602_dt(st);
	if (result)
		return result;

	st->config = kzalloc(sizeof(struct icm20602_user_config), GFP_ATOMIC);
	if (st->config == NULL)
		return -ENOMEM;

	icm20602_init_reg_map();

	i2c_set_clientdata(client, indio_dev);

	dev_set_drvdata(&client->dev, indio_dev);
	indio_dev->dev.parent = &client->dev;
	indio_dev->name = ICM20602_DEV_NAME;
	indio_dev->channels = inv_icm20602_channels;
	indio_dev->num_channels = ARRAY_SIZE(inv_icm20602_channels);

	indio_dev->info = &icm20602_info;
	indio_dev->modes = INDIO_BUFFER_TRIGGERED;
	result = iio_triggered_buffer_setup(indio_dev,
		inv_icm20602_irq_handler, inv_icm20602_read_fifo_fn, NULL);
	if (result) {
		dev_err(&st->client->dev, " configure buffer fail %d\n",
				result);
		goto out_remove_trigger;
	}
	icm20602_init_regulators(st);
	icm20602_ldo_work(st, true);

	result = inv_icm20602_probe_trigger(indio_dev);
	if (result) {
		dev_err(&st->client->dev, "trigger probe fail %d\n", result);
		goto out_unreg_ring;
	}

	INIT_KFIFO(st->timestamps);
	spin_lock_init(&st->time_stamp_lock);
	result = iio_device_register(indio_dev);
	if (result) {
		dev_err(&st->client->dev, "IIO register fail %d\n", result);
		goto out_remove_trigger;
	}

	return 0;

out_remove_trigger:
	inv_icm20602_remove_trigger(st);
out_unreg_ring:
	iio_triggered_buffer_cleanup(indio_dev);
	iio_device_free(indio_dev);
	gpio_free(st->gpio);

	return 0;
}

static int inv_icm20602_remove(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct inv_icm20602_state *st = iio_priv(indio_dev);

	gpio_free(st->gpio);
	iio_device_unregister(indio_dev);
	inv_icm20602_remove_trigger(st);
	iio_triggered_buffer_cleanup(indio_dev);
	iio_device_free(indio_dev);

	return 0;
}

static int inv_icm20602_suspend(struct device *dev)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct inv_icm20602_state *st = iio_priv(indio_dev);

	icm20602_stop_fifo(st);
	icm20602_ldo_work(st, false);
	return 0;
}

static int inv_icm20602_resume(struct device *dev)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct inv_icm20602_state *st = iio_priv(indio_dev);

	icm20602_ldo_work(st, true);
	icm20602_detect(st);
	icm20602_init_device(st);
	icm20602_start_fifo(st);

	return 0;
}

static const struct dev_pm_ops icm20602_pm_ops = {
	.resume		=	inv_icm20602_resume,
	.suspend	=	inv_icm20602_suspend,
};

static const struct of_device_id icm20602_match_table[] = {
	{.compatible = "invensense,icm20602"},
	{}
};
MODULE_DEVICE_TABLE(of, icm20602_match_table);

static const struct i2c_device_id inv_icm20602_id[] = {
	{"icm20602", 0},
	{}
};

static struct i2c_driver icm20602_i2c_driver = {
	.probe		=	inv_icm20602_probe,
	.remove		=	inv_icm20602_remove,
	.id_table	=	inv_icm20602_id,
	.driver = {
		.owner	=	THIS_MODULE,
		.name	=	"inv-icm20602",
		.of_match_table = icm20602_match_table,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
		.pm = &icm20602_pm_ops,
	},
};

static int __init inv_icm20602_init(void)
{
	return i2c_add_driver(&icm20602_i2c_driver);
}
module_init(inv_icm20602_init);

static void __exit inv_icm20602_exit(void)
{
	i2c_del_driver(&icm20602_i2c_driver);
}
module_exit(inv_icm20602_exit);

MODULE_DESCRIPTION("icm20602 IMU driver");
MODULE_LICENSE("GPL v2");
