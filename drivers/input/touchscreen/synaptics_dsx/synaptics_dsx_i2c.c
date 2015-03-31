/*
 * Synaptics DSX touchscreen driver
 *
 * Copyright (C) 2012 Synaptics Incorporated
 *
 * Copyright (C) 2012 Alexandra Chin <alexandra.chin@tw.synaptics.com>
 * Copyright (C) 2012 Scott Lin <scott.lin@tw.synaptics.com>
 * Copyright (C) 2015 XiaoMi, Inc.
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
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/input/synaptics_dsx.h>
#include "synaptics_dsx_core.h"
#include <linux/of_gpio.h>

#define SYN_I2C_RETRY_TIMES 10

static int synaptics_rmi4_i2c_set_page(struct synaptics_rmi4_data *rmi4_data,
		unsigned short addr)
{
	int retval;
	unsigned char retry;
	unsigned char buf[PAGE_SELECT_LEN];
	unsigned char page;
	struct i2c_client *i2c = to_i2c_client(rmi4_data->pdev->dev.parent);

	page = ((addr >> 8) & MASK_8BIT);
	if (page != rmi4_data->current_page) {
		buf[0] = MASK_8BIT;
		buf[1] = page;
		for (retry = 0; retry < SYN_I2C_RETRY_TIMES; retry++) {
			retval = i2c_master_send(i2c, buf, PAGE_SELECT_LEN);
			if (retval != PAGE_SELECT_LEN) {
				dev_err(rmi4_data->pdev->dev.parent,
						"%s: I2C retry %d\n",
						__func__, retry + 1);
				msleep(20);
			} else {
				rmi4_data->current_page = page;
				break;
			}
		}
	} else {
		retval = PAGE_SELECT_LEN;
	}

	return retval;
}

static int synaptics_rmi4_i2c_read(struct synaptics_rmi4_data *rmi4_data,
		unsigned short addr, unsigned char *data, unsigned short length)
{
	int retval;
	unsigned char retry;
	unsigned char buf;
	struct i2c_client *i2c = to_i2c_client(rmi4_data->pdev->dev.parent);
	struct i2c_msg msg[] = {
		{
			.addr = i2c->addr,
			.flags = 0,
			.len = 1,
			.buf = &buf,
		},
		{
			.addr = i2c->addr,
			.flags = I2C_M_RD,
			.len = length,
			.buf = data,
		},
	};

	buf = addr & MASK_8BIT;

	mutex_lock(&rmi4_data->rmi4_io_ctrl_mutex);

	retval = synaptics_rmi4_i2c_set_page(rmi4_data, addr);
	if (retval != PAGE_SELECT_LEN) {
		retval = -EIO;
		goto exit;
	}

	for (retry = 0; retry < SYN_I2C_RETRY_TIMES; retry++) {
		if (i2c_transfer(i2c->adapter, msg, 2) == 2) {
			retval = length;
			break;
		}
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: I2C retry %d\n",
				__func__, retry + 1);
		msleep(20);
	}

	if (retry == SYN_I2C_RETRY_TIMES) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: I2C read over retry limit\n",
				__func__);
		retval = -EIO;
	}

exit:
	mutex_unlock(&rmi4_data->rmi4_io_ctrl_mutex);

	return retval;
}

static int synaptics_rmi4_i2c_write(struct synaptics_rmi4_data *rmi4_data,
		unsigned short addr, unsigned char *data, unsigned short length)
{
	int retval;
	unsigned char retry;
	unsigned char buf[length + 1];
	struct i2c_client *i2c = to_i2c_client(rmi4_data->pdev->dev.parent);
	struct i2c_msg msg[] = {
		{
			.addr = i2c->addr,
			.flags = 0,
			.len = length + 1,
			.buf = buf,
		}
	};

	mutex_lock(&rmi4_data->rmi4_io_ctrl_mutex);

	retval = synaptics_rmi4_i2c_set_page(rmi4_data, addr);
	if (retval != PAGE_SELECT_LEN) {
		retval = -EIO;
		goto exit;
	}

	buf[0] = addr & MASK_8BIT;
	memcpy(&buf[1], &data[0], length);

	for (retry = 0; retry < SYN_I2C_RETRY_TIMES; retry++) {
		if (i2c_transfer(i2c->adapter, msg, 1) == 1) {
			retval = length;
			break;
		}
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: I2C retry %d\n",
				__func__, retry + 1);
		msleep(20);
	}

	if (retry == SYN_I2C_RETRY_TIMES) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: I2C write over retry limit\n",
				__func__);
		retval = -EIO;
	}

exit:
	mutex_unlock(&rmi4_data->rmi4_io_ctrl_mutex);

	return retval;
}

static struct synaptics_dsx_bus_access bus_access = {
	.type = BUS_I2C,
	.read = synaptics_rmi4_i2c_read,
	.write = synaptics_rmi4_i2c_write,
};

static struct platform_device *synaptics_dsx_i2c_device;

static void synaptics_rmi4_i2c_dev_release(struct device *dev)
{
	kfree(synaptics_dsx_i2c_device);

	return;
}

static void synaptics_rmi4_para_dump(struct device *dev,
				struct synaptics_dsx_board_data *bdata)
{
	int i;

	dev_info(dev, " id = %d\n", bdata->id);
	dev_info(dev, " power_gpio = %d\n", bdata->power_gpio);
	dev_info(dev, " reset_gpio = %d\n", bdata->reset_gpio);
	dev_info(dev, " irq_gpio = %d\n", bdata->irq_gpio);
	dev_info(dev, " x_flip = %d\n", (int)bdata->x_flip);
	dev_info(dev, " y_flip = %d\n", (int)bdata->y_flip);
	dev_info(dev, "swap_axes = %d\n", (int)bdata->swap_axes);
	dev_info(dev, "power_on_state = %d\n", (int)bdata->power_on_state);
	dev_info(dev, "reset_on_state = %d\n", (int)bdata->reset_on_state);
	dev_info(dev, "panel_x = %d\n", (int)bdata->panel_x);
	dev_info(dev, "panel_y = %d\n", (int)bdata->panel_y);
	dev_info(dev, "power_delay_ms = %d\n", (int)bdata->power_delay_ms);
	dev_info(dev, "reset_delay_ms = %d\n", (int)bdata->reset_delay_ms);
	dev_info(dev, "reset_active_ms = %d\n", (int)bdata->reset_active_ms);

	for (i = 0; i < bdata->cap_button_map->nbuttons; i++)
		dev_info(dev, "key[%d] = %d\n", i, bdata->cap_button_map->map[i]);
}

static int synaptics_rmi4_parse_dt(struct device *dev,
				struct synaptics_dsx_board_data *bdata)
{
	int rc, i;
	u32 temp_val;
	struct device_node *np = dev->of_node;
	int key_map[3];

	bdata->power_gpio = of_get_named_gpio_flags(np, "synaptics,power-gpio",
				0, &bdata->power_gpio_flags);
	if (bdata->power_gpio < 0)
		return bdata->power_gpio;

	bdata->reset_gpio = of_get_named_gpio_flags(np, "synaptics,reset-gpio",
				0, &bdata->reset_gpio_flags);
	if (bdata->reset_gpio < 0)
		return bdata->reset_gpio;

	bdata->irq_gpio = of_get_named_gpio_flags(np, "synaptics,irq-gpio",
				0, &bdata->irq_gpio_flags);
	if (bdata->irq_gpio < 0)
		return bdata->irq_gpio;

	rc = of_property_read_string(np, "synaptics,fw-name",
			&bdata->fw_name);
	if (rc  && (rc  != -EINVAL)) {
		dev_err(dev, "Unable to read fw name\n");
		return rc ;
	}

	rc = of_property_read_u32(np, "synaptics,id", &temp_val);
	if (rc) {
		dev_err(dev, "can't read id!\n");
		return rc;
	} else
		bdata->id = (bool)temp_val;

	rc = of_property_read_u32(np, "synaptics,x-flip", &temp_val);
	if (rc) {
		dev_err(dev, "can't read x-flip!\n");
		return rc;
	} else
		bdata->x_flip = (bool)temp_val;
	rc = of_property_read_u32(np, "synaptics,y-flip", &temp_val);
	if (rc) {
		dev_err(dev, "can't read y-flip!\n");
		return rc;
	} else
		bdata->y_flip = (bool)temp_val;
	rc = of_property_read_u32(np, "synaptics,swap-axes", &temp_val);
	if (rc) {
		dev_err(dev, "can't read swap-axes!\n");
		return rc;
	} else
		bdata->swap_axes = (bool)temp_val;

	rc = of_property_read_u32(np, "synaptics,power-on-state", &bdata->power_on_state);
	if (rc) {
		dev_err(dev, "can't read power-on-state!\n");
		return rc;
	}
	rc = of_property_read_u32(np, "synaptics,reset-on-state", &bdata->reset_on_state);
	if (rc) {
		dev_err(dev, "can't read reset-on-state!\n");
		return rc;
	}

	rc = of_property_read_u32(np, "synaptics,irqflags", &temp_val);
	if (rc) {
		dev_err(dev, "can't read irqflags!\n");
		return rc;
	} else
		bdata->irq_flags = (unsigned long)temp_val;

	rc = of_property_read_u32(np, "synaptics,panel-x", &bdata->panel_x);
	if (rc) {
		dev_err(dev, "can't read panel-x!\n");
		return rc;
	}
	rc = of_property_read_u32(np, "synaptics,panel-y", &bdata->panel_y);
	if (rc) {
		dev_err(dev, "can't read panel-y!\n");
		return rc;
	}

	rc = of_property_read_u32(np, "synaptics,max-major", &bdata->max_major);
	if (rc) {
		dev_err(dev, "can't read max major!\n");
		return rc;
	}

	rc = of_property_read_u32(np, "synaptics,max-minor", &bdata->max_minor);
	if (rc) {
		dev_err(dev, "can't read max major!\n");
		return rc;
	}

	rc = of_property_read_u32(np, "synaptics,max-finger-num", &bdata->max_finger_num);
	if (rc) {
		dev_err(dev, "can't read max major!\n");
		return rc;
	}

	rc = of_property_read_u32(np, "synaptics,power-delay-ms", &bdata->power_delay_ms);
	if (rc) {
		dev_err(dev, "can't read power-delay-ms!\n");
		return rc;
	}
	rc = of_property_read_u32(np, "synaptics,reset-delay-ms", &bdata->reset_delay_ms);
	if (rc) {
		dev_err(dev, "can't read reset-delay-ms!\n");
		return rc;
	}
	rc = of_property_read_u32(np, "synaptics,reset-active-ms", &bdata->reset_active_ms);
	if (rc) {
		dev_err(dev, "can't read reset-active-ms!\n");
		return rc;
	}

	bdata->cap_button_map = devm_kzalloc(dev,
					sizeof(struct synaptics_dsx_cap_button_map), GFP_KERNEL);
	if (bdata->cap_button_map == NULL)
		return -ENOMEM;

	rc = of_property_read_u32(np, "synaptics,key-num", &temp_val);
	if (rc) {
		dev_err(dev, "can't read key-num!\n");
		return rc;
	} else
		bdata->cap_button_map->nbuttons = (u8)temp_val;

	bdata->cap_button_map->map = devm_kzalloc(dev,
						sizeof(unsigned char) * temp_val, GFP_KERNEL);
	if (bdata->cap_button_map->map == NULL)
		return -ENOMEM;

	rc = of_property_read_u32_array(np, "synaptics,key-map", key_map, temp_val);
	if (rc) {
		dev_err(dev, "can't get key-map!\n");
		return rc;
	}

	for (i = 0; i < bdata->cap_button_map->nbuttons; i++)
		bdata->cap_button_map->map[i] = (unsigned char)key_map[i];

	synaptics_rmi4_para_dump(dev, bdata);

	return 0;
}

static int synaptics_rmi4_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *dev_id)
{
	int retval;
	struct synaptics_dsx_hw_interface *hw_if;

	if (!i2c_check_functionality(client->adapter,
			I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&client->dev,
				"%s: SMBus byte data commands not supported by host\n",
				__func__);
		return -EIO;
	}

	synaptics_dsx_i2c_device = kzalloc(
			sizeof(struct platform_device),
			GFP_KERNEL);
	if (!synaptics_dsx_i2c_device) {
		dev_err(&client->dev,
				"%s: Failed to allocate memory for synaptics_dsx_i2c_device\n",
				__func__);
		return -ENOMEM;
	}

	hw_if = devm_kzalloc(&client->dev,
			sizeof(struct synaptics_dsx_hw_interface), GFP_KERNEL);
	if (!hw_if) {
		dev_err(&client->dev,
				"%s: Failed to allocate memory for hw_if\n",
				__func__);
		return -ENOMEM;
	}

	hw_if->board_data = client->dev.platform_data;
	hw_if->bus_access = &bus_access;


	if (client->dev.of_node) {
		hw_if->board_data = devm_kzalloc(&client->dev,
				sizeof(struct synaptics_dsx_board_data), GFP_KERNEL);
		if (!hw_if->board_data) {
			dev_err(&client->dev, "Failed to allocate memory!\n");
			return -ENOMEM;
		}

		hw_if->board_data->cap_button_map = devm_kzalloc(&client->dev,
				sizeof(struct synaptics_dsx_cap_button_map),
				GFP_KERNEL);
		if (!hw_if->board_data->cap_button_map) {
			dev_err(&client->dev,
					"%s: Failed to allocate memory for 0D button map\n",
					__func__);
			return -ENOMEM;
		}

		hw_if->board_data->vir_button_map = devm_kzalloc(&client->dev,
				sizeof(struct synaptics_dsx_cap_button_map),
				GFP_KERNEL);
		if (!hw_if->board_data->vir_button_map) {
			dev_err(&client->dev,
					"%s: Failed to allocate memory for virtual button map\n",
					__func__);
			return -ENOMEM;
		}

		retval = synaptics_rmi4_parse_dt(&client->dev, hw_if->board_data);
		if (retval)
			return -EINVAL;
	}

	synaptics_dsx_i2c_device->name = PLATFORM_DRIVER_NAME;
	synaptics_dsx_i2c_device->id = hw_if->board_data->id;
	synaptics_dsx_i2c_device->num_resources = 0;
	synaptics_dsx_i2c_device->dev.parent = &client->dev;
	synaptics_dsx_i2c_device->dev.platform_data = hw_if;
	synaptics_dsx_i2c_device->dev.release = synaptics_rmi4_i2c_dev_release;

	retval = platform_device_register(synaptics_dsx_i2c_device);
	if (retval) {
		dev_err(&client->dev,
				"%s: Failed to register platform device\n",
				__func__);
		return -ENODEV;
	}

	return 0;
}

static int synaptics_rmi4_i2c_remove(struct i2c_client *client)
{
	platform_device_unregister(synaptics_dsx_i2c_device);

	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id synaptics_rmi4_match_table[] = {
	{ .compatible = "synaptics,rmi4",},
	{ },
};
#else
#define synaptics_rmi4_match_table NULL
#endif

static const struct i2c_device_id synaptics_rmi4_id_table[] = {
	{I2C_DRIVER_NAME, 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, synaptics_rmi4_id_table);

static struct i2c_driver synaptics_rmi4_i2c_driver = {
	.driver = {
		.name = I2C_DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = synaptics_rmi4_match_table,
	},
	.probe = synaptics_rmi4_i2c_probe,
	.remove = __devexit_p(synaptics_rmi4_i2c_remove),
	.id_table = synaptics_rmi4_id_table,
};

int synaptics_rmi4_bus_init(void)
{
	return i2c_add_driver(&synaptics_rmi4_i2c_driver);
}
EXPORT_SYMBOL(synaptics_rmi4_bus_init);

void synaptics_rmi4_bus_exit(void)
{
	i2c_del_driver(&synaptics_rmi4_i2c_driver);

	return;
}
EXPORT_SYMBOL(synaptics_rmi4_bus_exit);

MODULE_AUTHOR("Synaptics, Inc.");
MODULE_DESCRIPTION("Synaptics DSX I2C Bus Support Module");
MODULE_LICENSE("GPL v2");
