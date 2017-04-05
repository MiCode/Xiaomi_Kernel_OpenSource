/*
 * Synaptics DSX touchscreen driver
 *
 * Copyright (c) 2014-2016, The Linux Foundation.  All rights reserved.
 *
 * Linux foundation chooses to take subject only to the GPLv2 license terms,
 * and distributes only under these terms.
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
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/input/synaptics_dsx_v2.h>
#include "synaptics_dsx_core.h"
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#if defined(CONFIG_SECURE_TOUCH)
#include <linux/pm_runtime.h>
#endif

#define SYN_I2C_RETRY_TIMES 10
#define RESET_DELAY 100
#define DSX_COORDS_ARR_SIZE	4

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
	struct i2c_client *i2c = to_i2c_client(rmi4_data->pdev->dev.parent);
	struct i2c_msg msg[] = {
		{
			.addr = i2c->addr,
			.flags = 0,
			.len = length + 1,
		}
	};

	mutex_lock(&rmi4_data->rmi4_io_ctrl_mutex);
	/*
	 * Reassign memory for write_buf in case length is greater than 32 bytes
	 */
	if (rmi4_data->write_buf_len < length + 1) {
		kfree(rmi4_data->write_buf);
		rmi4_data->write_buf = kzalloc(length + 1, GFP_KERNEL);
		if (!rmi4_data->write_buf) {
			rmi4_data->write_buf_len = 0;
			retval = -ENOMEM;
			goto exit;
		}
		rmi4_data->write_buf_len = length + 1;
	}

	/* Assign the write_buf of driver stucture to i2c_msg buf */
	msg[0].buf = rmi4_data->write_buf;

	retval = synaptics_rmi4_i2c_set_page(rmi4_data, addr);
	if (retval != PAGE_SELECT_LEN) {
		retval = -EIO;
		goto exit;
	}

	rmi4_data->write_buf[0] = addr & MASK_8BIT;
	memcpy(&rmi4_data->write_buf[1], &data[0], length);

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

#if defined(CONFIG_SECURE_TOUCH)
static int synaptics_rmi4_clk_prepare_enable(
		struct synaptics_rmi4_data *rmi4_data)
{
	int ret;
	ret = clk_prepare_enable(rmi4_data->iface_clk);
	if (ret) {
		dev_err(rmi4_data->pdev->dev.parent,
			"error on clk_prepare_enable(iface_clk):%d\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(rmi4_data->core_clk);
	if (ret) {
		clk_disable_unprepare(rmi4_data->iface_clk);
		dev_err(rmi4_data->pdev->dev.parent,
			"error clk_prepare_enable(core_clk):%d\n", ret);
	}
	return ret;
}

static void synaptics_rmi4_clk_disable_unprepare(
		struct synaptics_rmi4_data *rmi4_data)
{
	clk_disable_unprepare(rmi4_data->core_clk);
	clk_disable_unprepare(rmi4_data->iface_clk);
}

static int synaptics_rmi4_i2c_get(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	struct i2c_client *i2c = to_i2c_client(rmi4_data->pdev->dev.parent);

	mutex_lock(&rmi4_data->rmi4_io_ctrl_mutex);
	retval = pm_runtime_get_sync(i2c->adapter->dev.parent);
	if (retval >= 0) {
		retval = synaptics_rmi4_clk_prepare_enable(rmi4_data);
		if (retval)
			pm_runtime_put_sync(i2c->adapter->dev.parent);
	}
	mutex_unlock(&rmi4_data->rmi4_io_ctrl_mutex);

	return retval;
}

static void synaptics_rmi4_i2c_put(struct synaptics_rmi4_data *rmi4_data)
{
	struct i2c_client *i2c = to_i2c_client(rmi4_data->pdev->dev.parent);

	mutex_lock(&rmi4_data->rmi4_io_ctrl_mutex);
	synaptics_rmi4_clk_disable_unprepare(rmi4_data);
	pm_runtime_put_sync(i2c->adapter->dev.parent);
	mutex_unlock(&rmi4_data->rmi4_io_ctrl_mutex);
}
#endif

static struct synaptics_dsx_bus_access bus_access = {
	.type = BUS_I2C,
	.read = synaptics_rmi4_i2c_read,
	.write = synaptics_rmi4_i2c_write,
#if defined(CONFIG_SECURE_TOUCH)
	.get = synaptics_rmi4_i2c_get,
	.put = synaptics_rmi4_i2c_put,
#endif
};

static struct synaptics_dsx_hw_interface hw_if;

static struct platform_device *synaptics_dsx_i2c_device;

static void synaptics_rmi4_i2c_dev_release(struct device *dev)
{
	kfree(synaptics_dsx_i2c_device);

	return;
}
#ifdef CONFIG_OF
int synaptics_dsx_get_dt_coords(struct device *dev, char *name,
				struct synaptics_dsx_board_data *pdata,
				struct device_node *node)
{
	u32 coords[DSX_COORDS_ARR_SIZE];
	struct property *prop;
	struct device_node *np = (node == NULL) ? (dev->of_node) : (node);
	int coords_size, rc;

	prop = of_find_property(np, name, NULL);
	if (!prop)
		return -EINVAL;
	if (!prop->value)
		return -ENODATA;

	coords_size = prop->length / sizeof(u32);
	if (coords_size != DSX_COORDS_ARR_SIZE) {
		dev_err(dev, "invalid %s\n", name);
		return -EINVAL;
	}

	rc = of_property_read_u32_array(np, name, coords, coords_size);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read %s\n", name);
		return rc;
	}

	if (strcmp(name, "synaptics,panel-coords") == 0) {
		pdata->panel_minx = coords[0];
		pdata->panel_miny = coords[1];
		pdata->panel_maxx = coords[2];
		pdata->panel_maxy = coords[3];
	} else if (strcmp(name, "synaptics,display-coords") == 0) {
		pdata->disp_minx = coords[0];
		pdata->disp_miny = coords[1];
		pdata->disp_maxx = coords[2];
		pdata->disp_maxy = coords[3];
	} else {
		dev_err(dev, "unsupported property %s\n", name);
		return -EINVAL;
	}

	return 0;
}

static int synaptics_dsx_parse_dt(struct device *dev,
				struct synaptics_dsx_board_data *rmi4_pdata)
{
	struct device_node *np = dev->of_node;
	struct property *prop;
	u32 temp_val, num_buttons;
	u32 button_map[MAX_NUMBER_OF_BUTTONS];
	int rc, i;

	rmi4_pdata->x_flip = of_property_read_bool(np, "synaptics,x-flip");
	rmi4_pdata->y_flip = of_property_read_bool(np, "synaptics,y-flip");

	rmi4_pdata->disable_gpios = of_property_read_bool(np,
			"synaptics,disable-gpios");

	rmi4_pdata->bypass_packrat_id_check = of_property_read_bool(np,
			"synaptics,bypass-packrat-id-check");

	rmi4_pdata->resume_in_workqueue = of_property_read_bool(np,
			"synaptics,resume-in-workqueue");

	rmi4_pdata->reset_delay_ms = RESET_DELAY;
	rc = of_property_read_u32(np, "synaptics,reset-delay-ms", &temp_val);
	if (!rc)
		rmi4_pdata->reset_delay_ms = temp_val;
	else if (rc != -EINVAL) {
		dev_err(dev, "Unable to read reset delay\n");
		return rc;
	}

	rc = of_property_read_u32(np, "synaptics,config-id",
					&rmi4_pdata->config_id);
	if (rc && (rc != -EINVAL))
		dev_err(dev, "Unable to read config id from DT\n");

	rmi4_pdata->fw_name = "PRXXX_fw.img";
	rc = of_property_read_string(np, "synaptics,fw-name",
					&rmi4_pdata->fw_name);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read fw name\n");
		return rc;
	}

	/* reset, irq gpio info */
	rmi4_pdata->reset_gpio = of_get_named_gpio_flags(np,
			"synaptics,reset-gpio", 0, &rmi4_pdata->reset_flags);
	rmi4_pdata->irq_gpio = of_get_named_gpio_flags(np,
			"synaptics,irq-gpio", 0, &rmi4_pdata->irq_flags);

	rc = synaptics_dsx_get_dt_coords(dev, "synaptics,display-coords",
				rmi4_pdata, NULL);
	if (rc && (rc != -EINVAL))
		return rc;

	rc = synaptics_dsx_get_dt_coords(dev, "synaptics,panel-coords",
				rmi4_pdata, NULL);
	if (rc && (rc != -EINVAL))
		return rc;

	rmi4_pdata->detect_device = of_property_read_bool(np,
				"synaptics,detect-device");

	if (rmi4_pdata->detect_device)
		return 0;

	prop = of_find_property(np, "synaptics,button-map", NULL);
	if (prop) {
		num_buttons = prop->length / sizeof(temp_val);

		rmi4_pdata->cap_button_map = devm_kzalloc(dev,
			sizeof(*rmi4_pdata->cap_button_map),
			GFP_KERNEL);
		if (!rmi4_pdata->cap_button_map)
			return -ENOMEM;

		rmi4_pdata->cap_button_map->map = devm_kzalloc(dev,
			sizeof(*rmi4_pdata->cap_button_map->map) *
			MAX_NUMBER_OF_BUTTONS, GFP_KERNEL);
		if (!rmi4_pdata->cap_button_map->map)
			return -ENOMEM;

		if (num_buttons <= MAX_NUMBER_OF_BUTTONS) {
			rc = of_property_read_u32_array(np,
				"synaptics,button-map", button_map,
				num_buttons);
			if (rc) {
				dev_err(dev, "Unable to read key codes\n");
				return rc;
			}
			for (i = 0; i < num_buttons; i++)
				rmi4_pdata->cap_button_map->map[i] =
					button_map[i];
			rmi4_pdata->cap_button_map->nbuttons =
				num_buttons;
		} else {
			return -EINVAL;
		}
	}
	return 0;
}
#else
static inline int synaptics_dsx_parse_dt(struct device *dev,
				struct synaptics_dsx_board_data *rmi4_pdata)
{
	return 0;
}
#endif

static int synaptics_rmi4_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *dev_id)
{
	int retval;
	struct synaptics_dsx_board_data *platform_data;

	if (!i2c_check_functionality(client->adapter,
			I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&client->dev,
				"%s: SMBus byte data commands not supported by host\n",
				__func__);
		return -EIO;
	}

	if (client->dev.of_node) {
		platform_data = devm_kzalloc(&client->dev,
			sizeof(struct synaptics_dsx_board_data),
			GFP_KERNEL);
		if (!platform_data) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}

		retval = synaptics_dsx_parse_dt(&client->dev, platform_data);
		if (retval)
			return retval;
	} else {
		platform_data = client->dev.platform_data;
	}

	if (!platform_data) {
		dev_err(&client->dev,
				"%s: No platform data found\n",
				__func__);
		return -EINVAL;
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

	hw_if.board_data = platform_data;
	hw_if.bus_access = &bus_access;

	synaptics_dsx_i2c_device->name = PLATFORM_DRIVER_NAME;
	synaptics_dsx_i2c_device->id = 0;
	synaptics_dsx_i2c_device->num_resources = 0;
	synaptics_dsx_i2c_device->dev.parent = &client->dev;
	synaptics_dsx_i2c_device->dev.platform_data = &hw_if;
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

static const struct i2c_device_id synaptics_rmi4_id_table[] = {
	{I2C_DRIVER_NAME, 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, synaptics_rmi4_id_table);

#ifdef CONFIG_OF
static struct of_device_id dsx_match_table[] = {
	{ .compatible = "synaptics,dsx",},
	{ },
};
#else
#define dsx_match_table NULL
#endif

static struct i2c_driver synaptics_rmi4_i2c_driver = {
	.driver = {
		.name = I2C_DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = dsx_match_table,
	},
	.probe = synaptics_rmi4_i2c_probe,
	.remove = synaptics_rmi4_i2c_remove,
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
