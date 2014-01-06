/*
 * Synaptics DSX touchscreen driver
 *
 * Copyright (C) 2012 Synaptics Incorporated
 *
 * Copyright (C) 2012 Alexandra Chin <alexandra.chin@tw.synaptics.com>
 * Copyright (C) 2012 Scott Lin <scott.lin@tw.synaptics.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/input/synaptics_dsx.h>
#include "synaptics_dsx_core.h"

#define SPI_READ 0x80
#define SPI_WRITE 0x00

static int synaptics_rmi4_spi_set_page(struct synaptics_rmi4_data *rmi4_data,
		unsigned short addr)
{
	int retval;
	unsigned int index;
	unsigned int xfer_count = PAGE_SELECT_LEN + 1;
	unsigned char txbuf[xfer_count];
	unsigned char page;
	struct spi_message msg;
	struct spi_transfer xfers[xfer_count];
	struct spi_device *spi = to_spi_device(rmi4_data->pdev->dev.parent);
	const struct synaptics_dsx_board_data *bdata =
			rmi4_data->hw_if->board_data;

	page = ((addr >> 8) & ~MASK_7BIT);
	if (page != rmi4_data->current_page) {
		spi_message_init(&msg);

		txbuf[0] = SPI_WRITE;
		txbuf[1] = MASK_8BIT;
		txbuf[2] = page;

		for (index = 0; index < xfer_count; index++) {
			memset(&xfers[index], 0, sizeof(struct spi_transfer));
			xfers[index].len = 1;
			xfers[index].delay_usecs = bdata->byte_delay_us;
			xfers[index].tx_buf = &txbuf[index];
			spi_message_add_tail(&xfers[index], &msg);
		}

		if (bdata->block_delay_us)
			xfers[index - 1].delay_usecs = bdata->block_delay_us;

		retval = spi_sync(spi, &msg);
		if (retval == 0) {
			rmi4_data->current_page = page;
			retval = PAGE_SELECT_LEN;
		} else {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to complete SPI transfer, error = %d\n",
					__func__, retval);
		}
	} else {
		retval = PAGE_SELECT_LEN;
	}

	return retval;
}

static int synaptics_rmi4_spi_read(struct synaptics_rmi4_data *rmi4_data,
		unsigned short addr, unsigned char *data, unsigned short length)
{
	int retval;
	unsigned int index;
	unsigned int xfer_count = length + ADDRESS_WORD_LEN;
	unsigned char txbuf[ADDRESS_WORD_LEN];
	unsigned char *rxbuf = NULL;
	struct spi_message msg;
	struct spi_transfer *xfers = NULL;
	struct spi_device *spi = to_spi_device(rmi4_data->pdev->dev.parent);
	const struct synaptics_dsx_board_data *bdata =
			rmi4_data->hw_if->board_data;

	spi_message_init(&msg);

	xfers = kcalloc(xfer_count, sizeof(struct spi_transfer), GFP_KERNEL);
	if (!xfers) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to allocate memory for xfers\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}

	txbuf[0] = (addr >> 8) | SPI_READ;
	txbuf[1] = addr & MASK_8BIT;

	rxbuf = kmalloc(length, GFP_KERNEL);
	if (!rxbuf) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to allocate memory for rxbuf\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}

	mutex_lock(&rmi4_data->rmi4_io_ctrl_mutex);

	retval = synaptics_rmi4_spi_set_page(rmi4_data, addr);
	if (retval != PAGE_SELECT_LEN) {
		retval = -EIO;
		goto exit;
	}

	for (index = 0; index < xfer_count; index++) {
		xfers[index].len = 1;
		xfers[index].delay_usecs = bdata->byte_delay_us;
		if (index < ADDRESS_WORD_LEN)
			xfers[index].tx_buf = &txbuf[index];
		else
			xfers[index].rx_buf = &rxbuf[index - ADDRESS_WORD_LEN];
		spi_message_add_tail(&xfers[index], &msg);
	}

	if (bdata->block_delay_us)
		xfers[index - 1].delay_usecs = bdata->block_delay_us;

	retval = spi_sync(spi, &msg);
	if (retval == 0) {
		retval = length;
		memcpy(data, rxbuf, length);
	} else {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to complete SPI transfer, error = %d\n",
				__func__, retval);
	}

	mutex_unlock(&rmi4_data->rmi4_io_ctrl_mutex);

exit:
	kfree(rxbuf);
	kfree(xfers);

	return retval;
}

static int synaptics_rmi4_spi_write(struct synaptics_rmi4_data *rmi4_data,
		unsigned short addr, unsigned char *data, unsigned short length)
{
	int retval;
	unsigned int index;
	unsigned int xfer_count = length + ADDRESS_WORD_LEN;
	unsigned char *txbuf = NULL;
	struct spi_message msg;
	struct spi_transfer *xfers = NULL;
	struct spi_device *spi = to_spi_device(rmi4_data->pdev->dev.parent);
	const struct synaptics_dsx_board_data *bdata =
			rmi4_data->hw_if->board_data;

	spi_message_init(&msg);

	xfers = kcalloc(xfer_count, sizeof(struct spi_transfer), GFP_KERNEL);
	if (!xfers) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to allocate memory for xfers\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}

	txbuf = kmalloc(xfer_count, GFP_KERNEL);
	if (!txbuf) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to allocate memory for txbuf\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}

	txbuf[0] = (addr >> 8) & ~SPI_READ;
	txbuf[1] = addr & MASK_8BIT;
	memcpy(&txbuf[ADDRESS_WORD_LEN], data, length);

	mutex_lock(&rmi4_data->rmi4_io_ctrl_mutex);

	retval = synaptics_rmi4_spi_set_page(rmi4_data, addr);
	if (retval != PAGE_SELECT_LEN) {
		retval = -EIO;
		goto exit;
	}

	for (index = 0; index < xfer_count; index++) {
		xfers[index].len = 1;
		xfers[index].delay_usecs = bdata->byte_delay_us;
		xfers[index].tx_buf = &txbuf[index];
		spi_message_add_tail(&xfers[index], &msg);
	}

	if (bdata->block_delay_us)
		xfers[index - 1].delay_usecs = bdata->block_delay_us;

	retval = spi_sync(spi, &msg);
	if (retval == 0) {
		retval = length;
	} else {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to complete SPI transfer, error = %d\n",
				__func__, retval);
	}

	mutex_unlock(&rmi4_data->rmi4_io_ctrl_mutex);

exit:
	kfree(txbuf);
	kfree(xfers);

	return retval;
}

static struct synaptics_dsx_bus_access bus_access = {
	.type = BUS_SPI,
	.read = synaptics_rmi4_spi_read,
	.write = synaptics_rmi4_spi_write,
};

static struct synaptics_dsx_hw_interface hw_if;

static struct platform_device *synaptics_dsx_spi_device;

static void synaptics_rmi4_spi_dev_release(struct device *dev)
{
	kfree(synaptics_dsx_spi_device);

	return;
}

static int synaptics_rmi4_spi_probe(struct spi_device *spi)
{
	int retval;

	if (spi->master->flags & SPI_MASTER_HALF_DUPLEX) {
		dev_err(&spi->dev,
				"%s: Full duplex not supported by host\n",
				__func__);
		return -EIO;
	}

	synaptics_dsx_spi_device = kzalloc(
			sizeof(struct platform_device),
			GFP_KERNEL);
	if (!synaptics_dsx_spi_device) {
		dev_err(&spi->dev,
				"%s: Failed to allocate memory for synaptics_dsx_spi_device\n",
				__func__);
		return -ENOMEM;
	}

	spi->bits_per_word = 8;
	spi->mode = SPI_MODE_3;

	retval = spi_setup(spi);
	if (retval < 0) {
		dev_err(&spi->dev,
				"%s: Failed to perform SPI setup\n",
				__func__);
		return retval;
	}

	hw_if.board_data = spi->dev.platform_data;
	hw_if.bus_access = &bus_access;

	synaptics_dsx_spi_device->name = PLATFORM_DRIVER_NAME;
	synaptics_dsx_spi_device->id = 0;
	synaptics_dsx_spi_device->num_resources = 0;
	synaptics_dsx_spi_device->dev.parent = &spi->dev;
	synaptics_dsx_spi_device->dev.platform_data = &hw_if;
	synaptics_dsx_spi_device->dev.release = synaptics_rmi4_spi_dev_release;

	retval = platform_device_register(synaptics_dsx_spi_device);
	if (retval) {
		dev_err(&spi->dev,
				"%s: Failed to register platform device\n",
				__func__);
		return -ENODEV;
	}

	return 0;
}

static int synaptics_rmi4_spi_remove(struct spi_device *spi)
{
	platform_device_unregister(synaptics_dsx_spi_device);

	return 0;
}

static struct spi_driver synaptics_rmi4_spi_driver = {
	.driver = {
		.name = SPI_DRIVER_NAME,
		.owner = THIS_MODULE,
	},
	.probe = synaptics_rmi4_spi_probe,
	.remove = __devexit_p(synaptics_rmi4_spi_remove),
};


int synaptics_rmi4_bus_init(void)
{
	return spi_register_driver(&synaptics_rmi4_spi_driver);
}
EXPORT_SYMBOL(synaptics_rmi4_bus_init);

void synaptics_rmi4_bus_exit(void)
{
	spi_unregister_driver(&synaptics_rmi4_spi_driver);

	return;
}
EXPORT_SYMBOL(synaptics_rmi4_bus_exit);

MODULE_AUTHOR("Synaptics, Inc.");
MODULE_DESCRIPTION("Synaptics DSX SPI Bus Support Module");
MODULE_LICENSE("GPL v2");
