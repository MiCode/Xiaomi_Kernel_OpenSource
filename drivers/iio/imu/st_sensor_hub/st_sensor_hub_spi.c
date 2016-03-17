/*
 * STMicroelectronics st_sensor_hub spi driver
 *
 * Copyright 2014 STMicroelectronics Inc.
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * Denis Ciocca <denis.ciocca@st.com>
 *
 * Licensed under the GPL-2.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/iio/iio.h>
#include <linux/of.h>

#include <linux/platform_data/st_hub_pdata.h>

#include "st_sensor_hub.h"

static int st_hub_spi_read(struct device *dev, size_t len, u8 *data)
{
	int err;
	struct spi_device *spi = to_spi_device(dev);
	struct st_hub_data *hdata = spi_get_drvdata(spi);

	struct spi_transfer xfers = {
		.rx_buf = hdata->tb.rx_buf,
		.bits_per_word = 8,
		.len = len,
	};

	if (len >= ST_HUB_MAX_RX_BUFFER) {
		dev_err(dev,
			"SPI read transfer not possible. Buffer too small.\n");
		return -ENOMEM;
	}

	mutex_lock(&hdata->tb.buf_lock);

	err = spi_sync_transfer(spi, &xfers, 1);
	if (err)
		goto spi_read_error;

	memcpy(data, hdata->tb.rx_buf, len * sizeof(u8));
	mutex_unlock(&hdata->tb.buf_lock);

	return len;

spi_read_error:
	mutex_unlock(&hdata->tb.buf_lock);
	return err;
}

static int st_hub_spi_write(struct device *dev, size_t len, u8 *data)
{
	int err;
	struct spi_device *spi = to_spi_device(dev);
	struct st_hub_data *hdata = spi_get_drvdata(spi);

	struct spi_transfer xfers = {
		.tx_buf = hdata->tb.tx_buf,
		.bits_per_word = 8,
		.len = len,
	};

	if (len >= ST_HUB_MAX_TX_BUFFER) {
		dev_err(dev,
			"SPI write transfer not possible. Buffer too small.\n");
		return -ENOMEM;
	}

	mutex_lock(&hdata->tb.buf_lock);
	memcpy(hdata->tb.tx_buf, data, len * sizeof(u8));

	err = spi_sync_transfer(to_spi_device(dev), &xfers, 1);
	mutex_unlock(&hdata->tb.buf_lock);

	return err;
}

static const struct st_hub_transfer_function st_hub_tf_spi = {
	.read = st_hub_spi_read,
	.write = st_hub_spi_write,
	.read_rl = st_hub_spi_read,
	.write_rl = st_hub_spi_write,
};

static char *st_sensor_hub_spi_get_name(struct device *dev)
{
	struct spi_device *spi = to_spi_device(dev);

	return spi->modalias;
}

static int st_sensor_hub_spi_probe(struct spi_device *spi)
{
	int err;
	struct st_hub_data *hdata;

	hdata = kzalloc(sizeof(*hdata), GFP_KERNEL);
	if (!hdata)
		return -ENOMEM;

	hdata->irq = spi->irq;
	hdata->dev = &spi->dev;
	hdata->tf = &st_hub_tf_spi;
	hdata->get_dev_name = &st_sensor_hub_spi_get_name;
	spi_set_drvdata(spi, hdata);

	err = st_sensor_hub_common_probe(hdata);
	if (err < 0)
		goto st_hub_free_hdata;

	return 0;

st_hub_free_hdata:
	kfree(hdata);
	return err;
}

static int st_sensor_hub_spi_remove(struct spi_device *spi)
{
	struct st_hub_data *hdata = spi_get_drvdata(spi);

	st_sensor_hub_common_remove(hdata);

	kfree(hdata);
	return 0;
}

#ifdef CONFIG_PM
static int st_sensor_hub_spi_suspend(struct device *dev)
{
	struct st_hub_data *hdata = spi_get_drvdata(to_spi_device(dev));

	return st_sensor_hub_common_suspend(hdata);
}

static int st_sensor_hub_spi_resume(struct device *dev)
{
	struct st_hub_data *hdata = spi_get_drvdata(to_spi_device(dev));

	return st_sensor_hub_common_resume(hdata);
}

static const struct dev_pm_ops st_sensor_hub_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(st_sensor_hub_spi_suspend, \
						st_sensor_hub_spi_resume)
};

#define ST_SENSOR_HUB_PM_OPS		(&st_sensor_hub_pm_ops)
#else /* CONFIG_PM */
#define ST_SENSOR_HUB_PM_OPS		NULL
#endif /* CONFIG_PM */

static const struct spi_device_id st_sensor_hub_id_table[] = {
	{ LIS332EB_DEV_NAME },
	{ },
};
MODULE_DEVICE_TABLE(spi, st_sensor_hub_id_table);

#ifdef CONFIG_OF
static const struct of_device_id st_sensor_hub_of_match[] = {
	{ .compatible = CONCATENATE_STRING("st,", LIS332EB_DEV_NAME) },
	{ }
};
MODULE_DEVICE_TABLE(of, st_sensor_hub_of_match);
#endif /* CONFIG_OF */

static struct spi_driver st_sensor_hub_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "st-sensor-hub-spi",
		.pm = ST_SENSOR_HUB_PM_OPS,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(st_sensor_hub_of_match),
#endif /* CONFIG_OF */
	},
	.probe = st_sensor_hub_spi_probe,
	.remove = st_sensor_hub_spi_remove,
	.id_table = st_sensor_hub_id_table,
};
module_spi_driver(st_sensor_hub_driver);

MODULE_AUTHOR("Denis Ciocca <denis.ciocca@st.com>");
MODULE_DESCRIPTION("STMicroelectronics sensor-hub spi driver");
MODULE_LICENSE("GPL v2");
