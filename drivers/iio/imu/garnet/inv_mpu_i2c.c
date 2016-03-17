/*
* Copyright (C) 2012 Invensense, Inc.
 * Copyright (C) 2016 XiaoMi, Inc.
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
#include <linux/i2c.h>
#include <linux/list.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/jiffies.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kfifo.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/wakelock.h>
#include <linux/gpio.h>
#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/sysfs.h>

#include "inv_mpu_iio.h"
#ifdef CONFIG_OF
#  include "inv_mpu_dts.h"
#endif
#include "inv_sh_sensor.h"

/**
 *  inv_i2c_write() - Write several bytes to a device register through i2c
 *  @st:	Device driver instance.
 *  @reg:	Device register to be written to.
 *  @len:    length of the write operation.
 *  @data:	pointer to the data to be written.
 *  NOTE: The size of write cannot exceed 256 bytes.
 */
static int inv_i2c_write(const struct inv_mpu_state *st, u8 reg, u16 len,
				const u8 *data)
{
	struct i2c_client *client = to_i2c_client(st->dev);
	u8 buf[256 + 1];
	struct i2c_msg msgs[1];
	int ret;

	if (!data || len > 256)
		return -EINVAL;

	buf[0] = reg;
	memcpy(buf + 1, data, len);

	/* write message */
	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].buf = buf;
	msgs[0].len = len + 1;
	ret = i2c_transfer(client->adapter, msgs, 1);

	if (ret != 1) {
		if (ret >= 0)
			ret = -EIO;
		return ret;
	}

	return 0;
}

static int inv_i2c_read(const struct inv_mpu_state *st, u8 reg, u16 len,
			u8 *data)
{
	struct i2c_client *client = to_i2c_client(st->dev);
	struct i2c_msg msgs[2];
	int ret;

	if (!data || len > 256)
		return -EINVAL;

	msgs[0].addr = client->addr;
	msgs[0].flags = 0;	/* write */
	msgs[0].buf = &reg;
	msgs[0].len = 1;

	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].buf = data;
	msgs[1].len = len;

	ret = i2c_transfer(client->adapter, msgs, 2);

	if (ret < 2) {
		if (ret >= 0)
			ret = -EIO;
	} else
		ret = 0;

	return ret;
}

static int inv_mpu_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct mpu_platform_data *pdata;
	struct inv_mpu_state *st;
	struct iio_dev *indio_dev;
	int result;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		result = -ENOSYS;
		dev_err(&client->dev, "i2c functionality missing\n");
		goto out_no_free;
	}
	indio_dev = iio_device_alloc(sizeof(*st));
	if (indio_dev == NULL) {
		dev_err(&client->dev, "memory allocation failed\n");
		result = -ENOMEM;
		goto out_no_free;
	}
	st = iio_priv(indio_dev);
	st->dev = &client->dev;
	st->irq = client->irq;
	st->write = inv_i2c_write;
	st->read = inv_i2c_read;
	mutex_init(&st->lock);
	mutex_init(&st->lock_cmd);
	wake_lock_init(&st->wake_lock, WAKE_LOCK_SUSPEND, "inv_mpu");
	INIT_LIST_HEAD(&st->sensors_list);
	i2c_set_clientdata(client, indio_dev);
	indio_dev->dev.parent = &client->dev;
	indio_dev->name = "icm30628";
#ifdef CONFIG_OF
	result = inv_mpu_parse_dt(&client->dev, &st->plat_data);
	if (result)
		goto out_init_free;
#else
	pdata = dev_get_platdata(&client->dev);
	if (pdata == NULL)
		goto out_init_free;
	st->plat_data = *pdata;
#endif
	pdata = &st->plat_data;
	/* Configure Wake GPIO */
	if (gpio_is_valid(pdata->wake_gpio)) {
		result = gpio_request(pdata->wake_gpio, "inv_mpu-wake");
		if (result) {
			dev_err(&client->dev, "cannot request wake gpio\n");
			goto out_init_free;
		}
		result = gpio_direction_output(pdata->wake_gpio, 0);
		if (result) {
			dev_err(&client->dev, "cannot set wake gpio\n");
			goto out_gpio_free;
		}
	}
	/* Power on device */
	inv_init_power(st);
	result = inv_set_power_on(st);
	if (result < 0) {
		dev_err(&client->dev,
			"power_on failed: %d\n", result);
		goto out_gpio_free;
	}
	dev_info(&client->dev, "power on\n");
	/* Check chip type */
	result = inv_check_chip_type(indio_dev, id->driver_data);
	if (result)
		goto out_power_off;

	result = inv_mpu_configure_ring(indio_dev);
	if (result) {
		dev_err(&client->dev, "configure ring buffer failed\n");
		goto out_power_off;
	}

	result = inv_mpu_probe_trigger(indio_dev);
	if (result) {
		dev_err(&client->dev, "trigger probe failed\n");
		goto out_unreg_ring;
	}

	result = iio_device_register(indio_dev);
	if (result) {
		dev_err(&client->dev, "iio device register failed\n");
		goto out_remove_trigger;
	}

	result = inv_create_dmp_sysfs(indio_dev);
	if (result) {
		dev_err(&client->dev, "create dmp sysfs failed\n");
		goto out_unreg_iio;
	}

	result = inv_sh_sensor_probe(st);
	if (result) {
		dev_err(&client->dev, "sensor module init error\n");
		goto out_unreg_iio;
	}

	dev_info(&client->dev, "%s is ready to go!\n", indio_dev->name);

	return 0;
out_unreg_iio:
	iio_device_unregister(indio_dev);
out_remove_trigger:
	inv_mpu_remove_trigger(indio_dev);
out_unreg_ring:
	inv_mpu_unconfigure_ring(indio_dev);
out_power_off:
	inv_set_power_off(st);
out_gpio_free:
	if (gpio_is_valid(pdata->wake_gpio))
		gpio_free(pdata->wake_gpio);
out_init_free:
	wake_lock_destroy(&st->wake_lock);
	mutex_destroy(&st->lock);
	mutex_destroy(&st->lock_cmd);
	iio_device_free(indio_dev);
out_no_free:
	return result;
}

static int inv_mpu_remove(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct inv_mpu_state *st = iio_priv(indio_dev);

	inv_sh_sensor_remove(st);
	iio_device_unregister(indio_dev);
	inv_mpu_remove_trigger(indio_dev);
	inv_mpu_unconfigure_ring(indio_dev);
	inv_set_power_off(st);
	if (gpio_is_valid(st->plat_data.wake_gpio))
		gpio_free(st->plat_data.wake_gpio);
	wake_lock_destroy(&st->wake_lock);
	mutex_destroy(&st->lock);
	mutex_destroy(&st->lock_cmd);
	iio_device_free(indio_dev);

	dev_info(&client->dev, "inv-mpu-i2c module removed.\n");

	return 0;
}

#ifdef CONFIG_PM
static int inv_mpu_suspend(struct device *dev)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(to_i2c_client(dev));
	struct inv_mpu_state *st = iio_priv(indio_dev);
	int ret;

	dev_dbg(dev, "%s inv_mpu_suspend\n", st->hw->name);

	ret = inv_proto_set_power(st, false);
	enable_irq_wake(st->irq);
	disable_irq(st->irq);

	return ret;
}

static int inv_mpu_resume(struct device *dev)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(to_i2c_client(dev));
	struct inv_mpu_state *st = iio_priv(indio_dev);
	int ret;

	dev_dbg(dev, "%s inv_mpu_resume\n", st->hw->name);

	ret = inv_proto_set_power(st, true);
	enable_irq(st->irq);
	disable_irq_wake(st->irq);

	return ret;
}
#endif /* CONFIG_PM */

static SIMPLE_DEV_PM_OPS(inv_mpu_pm, inv_mpu_suspend, inv_mpu_resume);

static const struct of_device_id inv_mpu_of_match[] = {
	{ .compatible = "inven,icm30628", },
	{ .compatible = "inven,sensorhub-v4", },
	{ }
};
MODULE_DEVICE_TABLE(of, inv_mpu_of_match);

static const struct i2c_device_id inv_mpu_id[] = {
	{ "icm30628", ICM30628 },
	{ "sensorhub-v4", SENSORHUB_V4 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, inv_mpu_id);

static struct i2c_driver inv_mpu_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "inv-mpu-i2c",
		.pm = &inv_mpu_pm,
		.of_match_table = of_match_ptr(inv_mpu_of_match),
	},
	.probe = inv_mpu_probe,
	.remove = inv_mpu_remove,
	.id_table = inv_mpu_id,
};
module_i2c_driver(inv_mpu_driver);

MODULE_AUTHOR("InvenSense, Inc.");
MODULE_DESCRIPTION("InvenSense SensorHub I2C driver");
MODULE_LICENSE("GPL");
