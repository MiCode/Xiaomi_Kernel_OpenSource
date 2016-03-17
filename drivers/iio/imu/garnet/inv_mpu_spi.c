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
#include <linux/list.h>
#include <linux/spi/spi.h>
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

#define INV_SPI_READ 0x80

static int inv_spi_write(const struct inv_mpu_state *st, u8 reg, u16 len,
				const u8 *data)
{
	struct spi_device *spi = to_spi_device(st->dev);
	u8 buf[256 + 1];

	if (len > 256)
		return -EINVAL;

	buf[0] = reg;
	memcpy(buf + 1, data, len);

	return spi_write(spi, buf, len + 1);
}

static int inv_spi_read(const struct inv_mpu_state *st, u8 reg, u16 len,
				u8 *data)
{
	struct spi_device *spi = to_spi_device(st->dev);
	struct spi_message msg;
	u8 d = reg | INV_SPI_READ;
	struct spi_transfer xfers[] = {
		{
			.tx_buf = &d,
			.bits_per_word = 8,
			.len = 1,
		},
		{
			.rx_buf = data,
			.bits_per_word = 8,
			.len = len,
		},
	};

	if (len > 256)
		return -EINVAL;

	spi_message_init(&msg);
	spi_message_add_tail(&xfers[0], &msg);
	spi_message_add_tail(&xfers[1], &msg);

	return spi_sync(spi, &msg);
}

static int inv_mpu_probe(struct spi_device *spi)
{
	const struct spi_device_id *id = spi_get_device_id(spi);
	struct mpu_platform_data *pdata;
	struct inv_mpu_state *st;
	struct iio_dev *indio_dev;
	int result;

	indio_dev = iio_device_alloc(sizeof(*st));
	if (indio_dev == NULL) {
		dev_err(&spi->dev, "memory allocation failed\n");
		result = -ENOMEM;
		goto out_no_free;
	}
	st = iio_priv(indio_dev);
	st->dev = &spi->dev;
	st->irq = spi->irq;
	st->write = inv_spi_write;
	st->read = inv_spi_read;
	mutex_init(&st->lock);
	mutex_init(&st->lock_cmd);
	wake_lock_init(&st->wake_lock, WAKE_LOCK_SUSPEND, "inv_mpu");
	INIT_LIST_HEAD(&st->sensors_list);
	spi_set_drvdata(spi, indio_dev);
	indio_dev->dev.parent = &spi->dev;
	indio_dev->name = "icm30628";
#ifdef CONFIG_OF
	result = inv_mpu_parse_dt(&spi->dev, &st->plat_data);
	if (result)
		goto out_init_free;
#else
	pdata = dev_get_platdata(&spi->dev);
	if (pdata == NULL)
		goto out_init_free;
	st->plat_data = *pdata;
#endif
	pdata = &st->plat_data;
	/* Configure Wake GPIO */
	if (gpio_is_valid(pdata->wake_gpio)) {
		result = gpio_request(pdata->wake_gpio, "inv_mpu-wake");
		if (result) {
			dev_err(&spi->dev, "cannot request wake gpio\n");
			goto out_init_free;
		}
		result = gpio_direction_output(pdata->wake_gpio, 0);
		if (result) {
			dev_err(&spi->dev, "cannot set wake gpio\n");
			goto out_gpio_free;
		}
	}
	/* Power on device */
	inv_init_power(st);
	result = inv_set_power_on(st);
	if (result < 0) {
		dev_err(&spi->dev,
			"power_on failed: %d\n", result);
		goto out_gpio_free;
	}
	dev_info(&spi->dev, "power on\n");

	/* Check chip type*/
	result = inv_check_chip_type(indio_dev, id->driver_data);
	if (result)
		goto out_power_off;

	result = inv_mpu_configure_ring(indio_dev);
	if (result) {
		dev_err(&spi->dev, "configure ring buffer failed\n");
		goto out_power_off;
	}

	result = inv_mpu_probe_trigger(indio_dev);
	if (result) {
		dev_err(&spi->dev, "trigger probe failed\n");
		goto out_unreg_ring;
	}

	result = iio_device_register(indio_dev);
	if (result) {
		dev_err(&spi->dev, "iio device register failed\n");
		goto out_remove_trigger;
	}

	result = inv_create_dmp_sysfs(indio_dev);
	if (result) {
		dev_err(&spi->dev, "create dmp sysfs failed\n");
		goto out_unreg_iio;
	}

	result = inv_sh_sensor_probe(st);
	if (result) {
		dev_err(&spi->dev, "sensor module init error\n");
		goto out_unreg_iio;
	}

	dev_info(&spi->dev, "%s is ready to go!\n", indio_dev->name);

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

static int inv_mpu_remove(struct spi_device *spi)
{
	struct iio_dev *indio_dev = spi_get_drvdata(spi);
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

	dev_info(&spi->dev, "inv-mpu-spi module removed.\n");

	return 0;
}

#ifdef CONFIG_PM
static int inv_mpu_suspend(struct device *dev)
{
	struct iio_dev *indio_dev = spi_get_drvdata(to_spi_device(dev));
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
	struct iio_dev *indio_dev = spi_get_drvdata(to_spi_device(dev));
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

static const struct spi_device_id inv_mpu_id[] = {
	{ "icm30628", ICM30628 },
	{ "sensorhub-v4", SENSORHUB_V4 },
	{ }
};
MODULE_DEVICE_TABLE(spi, inv_mpu_id);

static struct spi_driver inv_mpu_driver_spi = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "inv-mpu-spi",
		.pm = &inv_mpu_pm,
		.of_match_table = of_match_ptr(inv_mpu_of_match),
	},
	.probe = inv_mpu_probe,
	.remove = inv_mpu_remove,
	.id_table = inv_mpu_id,
};
module_spi_driver(inv_mpu_driver_spi);

MODULE_AUTHOR("InvenSense, Inc.");
MODULE_DESCRIPTION("InvenSense SensorHub SPI driver");
MODULE_LICENSE("GPL");
