/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/slab.h>
#include <linux/of.h>

#include "hf_manager.h"

#define LSM6DSM_SECONDARY_NAME "lsm6dsm_secondary"

#define LSM6DSM_ASYNC_BUFFER_SIZE 8

#define SPI_WRITE_CMD(reg) (reg)
#define SPI_READ_CMD(reg) ((unsigned char)(reg & 0xFF) | 0x80)

/* fullscale 125dps ==> 4.375
 * 4.375 * rawdata = mdps
 * 4375 * rawdata = udps
 */
#define GYRO_SCALER (4375)

struct lsm6dsm_device {
	struct hf_device hf_dev;

	struct spi_message spi_msg;
	struct spi_transfer spi_xfer[2];

	unsigned char async_rx_buffer[LSM6DSM_ASYNC_BUFFER_SIZE];
	unsigned char async_tx_buffer[LSM6DSM_ASYNC_BUFFER_SIZE];

	uint32_t direction;
};

static struct sensor_info support_sensors[] = {
	{
		.sensor_type = SENSOR_TYPE_GYRO_SECONDARY,
		.gain = 1,
		.name = {'g', 'y', 'r', 'o'},
		.vendor = {'m', 't', 'k'},
	}
};

static int lsm6dsm_enable(struct hf_device *hfdev, int sensor_type, int en)
{
	int err = 0;
	unsigned char tx_buffer[2] = {0};
	struct spi_device *spi_dev = hf_device_get_private_data(hfdev);

	pr_debug("%s id:%d en:%d\n", __func__, sensor_type, en);
	if (en) {
		/* fullscale 125dps, OIS_EN_SPI2 */
		tx_buffer[0] = SPI_WRITE_CMD(0x70);
		tx_buffer[1] = 0x03;
		err = spi_write(spi_dev, tx_buffer, 2);
		/* down-sanpled 1khz @ 173hz cutoff, HP_EN_OIS */
		tx_buffer[0] = SPI_WRITE_CMD(0x71);
		tx_buffer[1] = 0x05;
		err = spi_write(spi_dev, tx_buffer, 2);
	} else {
		/* fullscale 125dps */
		tx_buffer[0] = SPI_WRITE_CMD(0x70);
		tx_buffer[1] = 0x02;
		err = spi_write(spi_dev, tx_buffer, 2);
		/* down-sanpled 1khz, 173hz cutoff */
		tx_buffer[0] = SPI_WRITE_CMD(0x71);
		tx_buffer[1] = 0x04;
		err = spi_write(spi_dev, tx_buffer, 2);
	}
	return err;
}

static int lsm6dsm_batch(struct hf_device *hfdev, int sensor_type,
		int64_t delay, int64_t latency)
{
	pr_debug("%s id:%d delay:%lld latency:%lld\n", __func__, sensor_type,
			delay, latency);
	return 0;
}

static void lsm6dsm_sample_complete(void *ctx)
{
	struct lsm6dsm_device *driver_dev = ctx;
	struct hf_manager *manager = driver_dev->hf_dev.manager;
	struct hf_manager_event event;
	int32_t data[3] = {0};

	data[0] = (int16_t)((driver_dev->async_rx_buffer[1] << 8) |
		(driver_dev->async_rx_buffer[0]));
	data[1] = (int16_t)((driver_dev->async_rx_buffer[3] << 8) |
		(driver_dev->async_rx_buffer[2]));
	data[2] = (int16_t)((driver_dev->async_rx_buffer[5] << 8) |
		(driver_dev->async_rx_buffer[4]));
	coordinate_map(driver_dev->direction, data);
	memset(&event, 0, sizeof(struct hf_manager_event));
	event.timestamp = ktime_get_boot_ns();
	event.sensor_type = SENSOR_TYPE_GYRO_SECONDARY;
	event.accurancy = SENSOR_ACCURANCY_HIGH;
	event.action = DATA_ACTION;
	event.word[0] = data[0] * GYRO_SCALER;
	event.word[1] = data[1] * GYRO_SCALER;
	event.word[2] = data[2] * GYRO_SCALER;
	manager->report(manager, &event);
	manager->complete(manager);
}

static int lsm6dsm_sample(struct hf_device *hfdev)
{
	struct spi_device *spi_dev = hf_device_get_private_data(hfdev);
	struct lsm6dsm_device *driver_dev = spi_get_drvdata(spi_dev);

	spi_message_init(&driver_dev->spi_msg);

	driver_dev->spi_msg.context = driver_dev;
	driver_dev->spi_msg.complete = lsm6dsm_sample_complete;

	memset(driver_dev->spi_xfer, 0, sizeof(driver_dev->spi_xfer));

	memset(driver_dev->async_tx_buffer, 0,
		sizeof(driver_dev->async_tx_buffer));
	memset(driver_dev->async_rx_buffer, 0,
		sizeof(driver_dev->async_rx_buffer));

	driver_dev->async_tx_buffer[0] = SPI_READ_CMD(0x22);
	driver_dev->spi_xfer[0].len = 1;
	driver_dev->spi_xfer[0].tx_buf = driver_dev->async_tx_buffer;
	spi_message_add_tail(&driver_dev->spi_xfer[0], &driver_dev->spi_msg);

	driver_dev->spi_xfer[1].len = 6;
	driver_dev->spi_xfer[1].rx_buf = driver_dev->async_rx_buffer;
	spi_message_add_tail(&driver_dev->spi_xfer[1], &driver_dev->spi_msg);

	return spi_async(spi_dev, &driver_dev->spi_msg);
}

static int lsm6dsm_init_device(struct spi_device *spi_dev)
{
	int err = 0;
	unsigned char tx_buffer[2] = {0};
	unsigned char rx_buffer[2] = {0};

	tx_buffer[0] = SPI_READ_CMD(0x0f);
	err = spi_write_then_read(spi_dev, tx_buffer, 1, rx_buffer, 1);

	/* fullscale 125dps */
	tx_buffer[0] = SPI_WRITE_CMD(0x70);
	tx_buffer[1] = 0x02;
	err = spi_write(spi_dev, tx_buffer, 2);

	/* down-sanpled 1khz, 173hz cutoff */
	tx_buffer[0] = SPI_WRITE_CMD(0x71);
	tx_buffer[1] = 0x04;
	err = spi_write(spi_dev, tx_buffer, 2);
	return err;
}

static int lsm6dsm_probe(struct spi_device *spi_dev)
{
	int err = 0;
	struct lsm6dsm_device *driver_dev = NULL;

	spi_dev->mode = SPI_MODE_0;
	spi_dev->bits_per_word = 8;
	spi_dev->max_speed_hz = 10 * 1000 * 1000;

	err = lsm6dsm_init_device(spi_dev);
	if (err < 0) {
		pr_err("%s init device fail\n", __func__);
		goto init_fail;
	}

	driver_dev = kzalloc(sizeof(*driver_dev), GFP_KERNEL);
	if (!driver_dev) {
		err = -ENOMEM;
		goto init_fail;
	}

	if (of_property_read_u32(spi_dev->dev.of_node,
		"direction", &driver_dev->direction)) {
		pr_err("%s get direction dts fail\n", __func__);
		goto dts_fail;
	}

	driver_dev->hf_dev.dev_name = LSM6DSM_SECONDARY_NAME;
	driver_dev->hf_dev.device_poll = HF_DEVICE_IO_POLLING;
	driver_dev->hf_dev.device_bus = HF_DEVICE_IO_ASYNC;
	driver_dev->hf_dev.support_list = support_sensors;
	driver_dev->hf_dev.support_size = ARRAY_SIZE(support_sensors);
	driver_dev->hf_dev.enable = lsm6dsm_enable;
	driver_dev->hf_dev.batch = lsm6dsm_batch;
	driver_dev->hf_dev.sample = lsm6dsm_sample;
	hf_device_set_private_data(&driver_dev->hf_dev, spi_dev);
	err = hf_device_register_manager_create(&driver_dev->hf_dev);
	if (err < 0) {
		pr_err("%s hf_manager_create fail\n", __func__);
		err = -1;
		goto create_manager_fail;
	}
	spi_set_drvdata(spi_dev, driver_dev);
	return 0;

create_manager_fail:
dts_fail:
	kfree(driver_dev);
init_fail:
	return err;
}

static int lsm6dsm_remove(struct spi_device *spi_dev)
{
	struct lsm6dsm_device *driver_dev = spi_get_drvdata(spi_dev);

	hf_manager_destroy(driver_dev->hf_dev.manager);
	kfree(driver_dev);
	return 0;
}

static const struct of_device_id lsm6dsm_ids[] = {
	{.compatible = "mediatek,lsm6dsm_secondary"},
	{},
};

static struct spi_driver lsm6dsm_driver = {
	.driver = {
		.name = LSM6DSM_SECONDARY_NAME,
		.bus = &spi_bus_type,
		.owner = THIS_MODULE,
		.of_match_table = lsm6dsm_ids,
	},
	.probe = lsm6dsm_probe,
	.remove = lsm6dsm_remove,
};

module_spi_driver(lsm6dsm_driver);

MODULE_AUTHOR("Mediatek");
MODULE_DESCRIPTION("lsm6dsm secondary driver");
MODULE_LICENSE("GPL");
