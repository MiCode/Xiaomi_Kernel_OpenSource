/*
 * Synaptics DSX touchscreen driver
 *
 * Copyright (C) 2012-2015 Synaptics Incorporated. All rights reserved.
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
 *
 * INFORMATION CONTAINED IN THIS DOCUMENT IS PROVIDED "AS-IS," AND SYNAPTICS
 * EXPRESSLY DISCLAIMS ALL EXPRESS AND IMPLIED WARRANTIES, INCLUDING ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE,
 * AND ANY WARRANTIES OF NON-INFRINGEMENT OF ANY INTELLECTUAL PROPERTY RIGHTS.
 * IN NO EVENT SHALL SYNAPTICS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, PUNITIVE, OR CONSEQUENTIAL DAMAGES ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OF THE INFORMATION CONTAINED IN THIS DOCUMENT, HOWEVER CAUSED
 * AND BASED ON ANY THEORY OF LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, AND EVEN IF SYNAPTICS WAS ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE. IF A TRIBUNAL OF COMPETENT JURISDICTION DOES
 * NOT PERMIT THE DISCLAIMER OF DIRECT DAMAGES OR ANY OTHER DAMAGES, SYNAPTICS'
 * TOTAL CUMULATIVE LIABILITY TO ANY PARTY SHALL NOT EXCEED ONE HUNDRED U.S.
 * DOLLARS.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/types.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/input/synaptics_dsx_v2_6.h>
#include "synaptics_dsx_core.h"

#define SPI_READ 0x80
#define SPI_WRITE 0x00

#ifdef CONFIG_OF
static int parse_dt(struct device *dev, struct synaptics_dsx_board_data *bdata)
{
	int retval;
	u32 value;
	const char *name;
	struct property *prop;
	struct device_node *np = dev->of_node;

	bdata->irq_gpio = of_get_named_gpio_flags(np,
			"synaptics,irq-gpio", 0,
			(enum of_gpio_flags *)&bdata->irq_flags);

	retval = of_property_read_u32(np, "synaptics,irq-on-state",
			&value);
	if (retval < 0)
		bdata->irq_on_state = 0;
	else
		bdata->irq_on_state = value;

	retval = of_property_read_string(np, "synaptics,pwr-reg-name", &name);
	if (retval < 0)
		bdata->pwr_reg_name = NULL;
	else
		bdata->pwr_reg_name = name;

	retval = of_property_read_string(np, "synaptics,bus-reg-name", &name);
	if (retval < 0)
		bdata->bus_reg_name = NULL;
	else
		bdata->bus_reg_name = name;

	prop = of_find_property(np, "synaptics,power-gpio", NULL);
	if (prop && prop->length) {
		bdata->power_gpio = of_get_named_gpio_flags(np,
				"synaptics,power-gpio", 0, NULL);
		retval = of_property_read_u32(np, "synaptics,power-on-state",
				&value);
		if (retval < 0) {
			dev_err(dev, "%s: Unable to read synaptics,power-on-state property\n",
					__func__);
			return retval;
		} else {
			bdata->power_on_state = value;
		}
	} else {
		bdata->power_gpio = -1;
	}

	prop = of_find_property(np, "synaptics,power-delay-ms", NULL);
	if (prop && prop->length) {
		retval = of_property_read_u32(np, "synaptics,power-delay-ms",
				&value);
		if (retval < 0) {
			dev_err(dev, "%s: Unable to read synaptics,power-delay-ms property\n",
					__func__);
			return retval;
		} else {
			bdata->power_delay_ms = value;
		}
	} else {
		bdata->power_delay_ms = 0;
	}

	prop = of_find_property(np, "synaptics,reset-gpio", NULL);
	if (prop && prop->length) {
		bdata->reset_gpio = of_get_named_gpio_flags(np,
				"synaptics,reset-gpio", 0, NULL);
		retval = of_property_read_u32(np, "synaptics,reset-on-state",
				&value);
		if (retval < 0) {
			dev_err(dev, "%s: Unable to read synaptics,reset-on-state property\n",
					__func__);
			return retval;
		} else {
			bdata->reset_on_state = value;
		}
		retval = of_property_read_u32(np, "synaptics,reset-active-ms",
				&value);
		if (retval < 0) {
			dev_err(dev, "%s: Unable to read synaptics,reset-active-ms property\n",
					__func__);
			return retval;
		} else {
			bdata->reset_active_ms = value;
		}
	} else {
		bdata->reset_gpio = -1;
	}

	prop = of_find_property(np, "synaptics,reset-delay-ms", NULL);
	if (prop && prop->length) {
		retval = of_property_read_u32(np, "synaptics,reset-delay-ms",
				&value);
		if (retval < 0) {
			dev_err(dev, "%s: Unable to read synaptics,reset-delay-ms property\n",
					__func__);
			return retval;
		} else {
			bdata->reset_delay_ms = value;
		}
	} else {
		bdata->reset_delay_ms = 0;
	}

	prop = of_find_property(np, "synaptics,byte-delay-us", NULL);
	if (prop && prop->length) {
		retval = of_property_read_u32(np, "synaptics,byte-delay-us",
				&value);
		if (retval < 0) {
			dev_err(dev, "%s: Unable to read synaptics,byte-delay-us property\n",
					__func__);
			return retval;
		} else {
			bdata->byte_delay_us = value;
		}
	} else {
		bdata->byte_delay_us = 0;
	}

	prop = of_find_property(np, "synaptics,block-delay-us", NULL);
	if (prop && prop->length) {
		retval = of_property_read_u32(np, "synaptics,block-delay-us",
				&value);
		if (retval < 0) {
			dev_err(dev, "%s: Unable to read synaptics,block-delay-us property\n",
					__func__);
			return retval;
		} else {
			bdata->block_delay_us = value;
		}
	} else {
		bdata->block_delay_us = 0;
	}

	prop = of_find_property(np, "synaptics,max-y-for-2d", NULL);
	if (prop && prop->length) {
		retval = of_property_read_u32(np, "synaptics,max-y-for-2d",
				&value);
		if (retval < 0) {
			dev_err(dev, "%s: Unable to read synaptics,max-y-for-2d property\n",
					__func__);
			return retval;
		} else {
			bdata->max_y_for_2d = value;
		}
	} else {
		bdata->max_y_for_2d = -1;
	}

	prop = of_find_property(np, "synaptics,swap-axes", NULL);
	bdata->swap_axes = prop > 0 ? true : false;

	prop = of_find_property(np, "synaptics,x-flip", NULL);
	bdata->x_flip = prop > 0 ? true : false;

	prop = of_find_property(np, "synaptics,y-flip", NULL);
	bdata->y_flip = prop > 0 ? true : false;

	prop = of_find_property(np, "synaptics,ub-i2c-addr", NULL);
	if (prop && prop->length) {
		retval = of_property_read_u32(np, "synaptics,ub-i2c-addr",
				&value);
		if (retval < 0) {
			dev_err(dev, "%s: Unable to read synaptics,ub-i2c-addr property\n",
					__func__);
			return retval;
		} else {
			bdata->ub_i2c_addr = (unsigned short)value;
		}
	} else {
		bdata->ub_i2c_addr = -1;
	}

	prop = of_find_property(np, "synaptics,cap-button-codes", NULL);
	if (prop && prop->length) {
		bdata->cap_button_map->map = devm_kzalloc(dev,
				prop->length,
				GFP_KERNEL);
		if (!bdata->cap_button_map->map)
			return -ENOMEM;
		bdata->cap_button_map->nbuttons = prop->length / sizeof(u32);
		retval = of_property_read_u32_array(np,
				"synaptics,cap-button-codes",
				bdata->cap_button_map->map,
				bdata->cap_button_map->nbuttons);
		if (retval < 0) {
			bdata->cap_button_map->nbuttons = 0;
			bdata->cap_button_map->map = NULL;
		}
	} else {
		bdata->cap_button_map->nbuttons = 0;
		bdata->cap_button_map->map = NULL;
	}

	prop = of_find_property(np, "synaptics,vir-button-codes", NULL);
	if (prop && prop->length) {
		bdata->vir_button_map->map = devm_kzalloc(dev,
				prop->length,
				GFP_KERNEL);
		if (!bdata->vir_button_map->map)
			return -ENOMEM;
		bdata->vir_button_map->nbuttons = prop->length / sizeof(u32);
		bdata->vir_button_map->nbuttons /= 5;
		retval = of_property_read_u32_array(np,
				"synaptics,vir-button-codes",
				bdata->vir_button_map->map,
				bdata->vir_button_map->nbuttons * 5);
		if (retval < 0) {
			bdata->vir_button_map->nbuttons = 0;
			bdata->vir_button_map->map = NULL;
		}
	} else {
		bdata->vir_button_map->nbuttons = 0;
		bdata->vir_button_map->map = NULL;
	}

	return 0;
}
#endif

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
		mutex_unlock(&rmi4_data->rmi4_io_ctrl_mutex);
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
		retval = secure_memcpy(data, length, rxbuf, length, length);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to copy data\n",
					__func__);
		} else {
			retval = length;
		}
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
	retval = secure_memcpy(&txbuf[ADDRESS_WORD_LEN],
			xfer_count - ADDRESS_WORD_LEN, data, length, length);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to copy data\n",
				__func__);
		goto exit;
	}

	mutex_lock(&rmi4_data->rmi4_io_ctrl_mutex);

	retval = synaptics_rmi4_spi_set_page(rmi4_data, addr);
	if (retval != PAGE_SELECT_LEN) {
		mutex_unlock(&rmi4_data->rmi4_io_ctrl_mutex);
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

#ifdef CONFIG_OF
	if (spi->dev.of_node) {
		hw_if.board_data = devm_kzalloc(&spi->dev,
				sizeof(struct synaptics_dsx_board_data),
				GFP_KERNEL);
		if (!hw_if.board_data) {
			dev_err(&spi->dev,
					"%s: Failed to allocate memory for board data\n",
					__func__);
			return -ENOMEM;
		}
		hw_if.board_data->cap_button_map = devm_kzalloc(&spi->dev,
				sizeof(struct synaptics_dsx_button_map),
				GFP_KERNEL);
		if (!hw_if.board_data->cap_button_map) {
			dev_err(&spi->dev,
					"%s: Failed to allocate memory for 0D button map\n",
					__func__);
			return -ENOMEM;
		}
		hw_if.board_data->vir_button_map = devm_kzalloc(&spi->dev,
				sizeof(struct synaptics_dsx_button_map),
				GFP_KERNEL);
		if (!hw_if.board_data->vir_button_map) {
			dev_err(&spi->dev,
					"%s: Failed to allocate memory for virtual button map\n",
					__func__);
			return -ENOMEM;
		}
		parse_dt(&spi->dev, hw_if.board_data);
	}
#else
	hw_if.board_data = spi->dev.platform_data;
#endif

	hw_if.bus_access = &bus_access;

	spi->bits_per_word = 8;
	spi->mode = SPI_MODE_3;

	retval = spi_setup(spi);
	if (retval < 0) {
		dev_err(&spi->dev,
				"%s: Failed to perform SPI setup\n",
				__func__);
		return retval;
	}

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

#ifdef CONFIG_OF
static struct of_device_id synaptics_rmi4_of_match_table[] = {
	{
		.compatible = "synaptics,dsx-spi",
	},
	{},
};
MODULE_DEVICE_TABLE(of, synaptics_rmi4_of_match_table);
#else
#define synaptics_rmi4_of_match_table NULL
#endif

static struct spi_driver synaptics_rmi4_spi_driver = {
	.driver = {
		.name = SPI_DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = synaptics_rmi4_of_match_table,
	},
	.probe = synaptics_rmi4_spi_probe,
	.remove = synaptics_rmi4_spi_remove,
};


int synaptics_rmi4_bus_init_v26(void)
{
	return spi_register_driver(&synaptics_rmi4_spi_driver);
}
EXPORT_SYMBOL(synaptics_rmi4_bus_init_v26);

void synaptics_rmi4_bus_exit_v26(void)
{
	spi_unregister_driver(&synaptics_rmi4_spi_driver);

	return;
}
EXPORT_SYMBOL(synaptics_rmi4_bus_exit_v26);

MODULE_AUTHOR("Synaptics, Inc.");
MODULE_DESCRIPTION("Synaptics DSX SPI Bus Support Module");
MODULE_LICENSE("GPL v2");
