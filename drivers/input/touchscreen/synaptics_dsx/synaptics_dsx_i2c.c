/*
 * Synaptics DSX touchscreen driver
 *
 * Copyright (C) 2012 Synaptics Incorporated
 *
 * Copyright (C) 2012 Alexandra Chin <alexandra.chin@tw.synaptics.com>
 * Copyright (C) 2012 Scott Lin <scott.lin@tw.synaptics.com>
 * Copyright (C) 2016 XiaoMi, Inc.
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
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/input/synaptics_dsx.h>
#include "synaptics_dsx_core.h"

#define SYN_I2C_RETRY_TIMES 4

/*
#define I2C_BURST_LIMIT 255
*/
/*
#define XFER_MSGS_LIMIT 8
*/

static struct synaptics_dsx_hw_interface hw_if;

static struct platform_device *synaptics_dsx_i2c_device;

#ifdef CONFIG_OF
static void dump_dt(struct device *dev, struct synaptics_dsx_board_data *bdata)
{
	int i, j;
	char tmp[256] = {0};
	dev_dbg(dev, "START of device tree dump:\n");
	dev_dbg(dev, "power_gpio = %d\n", bdata->power_gpio);
	dev_dbg(dev, "reset_gpio = %d\n", bdata->reset_gpio);
	dev_dbg(dev, "irq_gpio = %d\n", bdata->irq_gpio);
	dev_dbg(dev, "power_on_state = %d\n", (int)bdata->power_on_state);
	dev_dbg(dev, "reset_on_state = %d\n", (int)bdata->reset_on_state);
	dev_dbg(dev, "power_delay_ms = %d\n", (int)bdata->power_delay_ms);
	dev_dbg(dev, "reset_delay_ms = %d\n", (int)bdata->reset_delay_ms);
	dev_dbg(dev, "reset_active_ms = %d\n", (int)bdata->reset_active_ms);
	dev_dbg(dev, "cut_off_power = %d\n", (int)bdata->cut_off_power);
	dev_dbg(dev, "use_charger_bit = %d\n", (int)bdata->use_charger_bit);
	dev_dbg(dev, "swap_axes = %d\n", (int)bdata->swap_axes);
	dev_dbg(dev, "x_flip = %d\n", (int)bdata->x_flip);
	dev_dbg(dev, "y_flip = %d\n", (int)bdata->y_flip);
	dev_dbg(dev, "ub_i2c_addr = %d\n", (int)bdata->ub_i2c_addr);
	dev_dbg(dev, "lockdown_area = %d\n", (int)bdata->lockdown_area);

	for (i = 0; i < bdata->tp_id_num; i++)
		snprintf(tmp, 256, "%s %d", tmp, bdata->tp_id_bytes[i]);
	dev_dbg(dev, "tp_id_bytes =%s\n", tmp);

	dev_dbg(dev, "config_array_size = %d\n", (int)bdata->config_array_size);
	for (i = 0; i < bdata->config_array_size; i++) {
		memset(tmp, 0, sizeof(tmp));
		for (j = 0; j < bdata->tp_id_num; j++)
			snprintf(tmp, 256, "%s 0x%0x", tmp, bdata->config_array[i].tp_ids[j]);
		dev_dbg(dev, "config[%d].tp_id =%s", i, tmp);

		dev_dbg(dev, "config[%d].fw_name = %s\n", i, bdata->config_array[i].fw_name);
	}
	dev_dbg(dev, "END of device tree dump\n");
}

static int parse_dt(struct device *dev, struct synaptics_dsx_board_data *bdata)
{
	int retval;
	u32 value;
	const char *name;
	struct synaptics_dsx_config_info *config_info;
	struct property *prop;
	struct device_node *temp, *np = dev->of_node;

	bdata->irq_gpio = of_get_named_gpio_flags(np,
			"synaptics,irq-gpio", 0, NULL);
	if (bdata->irq_gpio < 0)
		return -EINVAL;

	retval = of_property_read_u32(np, "synaptics,irq-on-state",
			&value);
	if (retval < 0)
		bdata->irq_on_state = 0;
	else
		bdata->irq_on_state = value;

	retval = of_property_read_u32(np, "synaptics,irq-flags", &value);
	if (retval < 0)
		return retval;
	else
		bdata->irq_flags = value;

	retval = of_property_read_u32(np, "synaptics,panel-x", &bdata->panel_x);
	if (retval < 0)
		return retval;

	retval = of_property_read_u32(np, "synaptics,panel-y", &bdata->panel_y);
	if (retval < 0)
		return retval;

	retval = of_property_read_u32(np, "synaptics,max-major", &bdata->max_major);
	if (retval < 0)
		return retval;

	retval = of_property_read_u32(np, "synaptics,max-minor", &bdata->max_minor);
	if (retval < 0)
		return retval;

	retval = of_property_read_u32(np, "synaptics,max-finger-num", &bdata->max_finger_num);
	if (retval < 0)
		return retval;

	retval = of_property_read_string(np, "synaptics,pwr-reg-name", &name);
	if (retval == -EINVAL)
		bdata->pwr_reg_name = NULL;
	else if (retval < 0)
		return retval;
	else
		bdata->pwr_reg_name = name;

	retval = of_property_read_string(np, "synaptics,bus-reg-name", &name);
	if (retval == -EINVAL)
		bdata->bus_reg_name = NULL;
	else if (retval < 0)
		return retval;
	else
		bdata->bus_reg_name = name;

	retval = of_property_read_string(np, "synaptics,power-gpio-name", &name);
	if (retval == -EINVAL)
		bdata->power_gpio_name = NULL;
	else if (retval < 0)
		return retval;
	else
		bdata->power_gpio_name = name;

	retval = of_property_read_string(np, "synaptics,reset-gpio-name", &name);
	if (retval == -EINVAL)
		bdata->reset_gpio_name = NULL;
	else if (retval < 0)
		return retval;
	else
		bdata->reset_gpio_name = name;

	retval = of_property_read_string(np, "synaptics,irq-gpio-name", &name);
	if (retval == -EINVAL)
		bdata->irq_gpio_name = NULL;
	else if (retval < 0)
		return retval;
	else
		bdata->irq_gpio_name = name;

	bdata->cut_off_power = of_property_read_bool(np, "synaptics,cut-off-power");

	bdata->use_charger_bit = of_property_read_bool(np, "synaptics,use-charger-bit");

	bdata->power_gpio = of_get_named_gpio_flags(np,
			"synaptics,power-gpio", 0, NULL);
	if (bdata->power_gpio >= 0) {
		retval = of_property_read_u32(np, "synaptics,power-on-state",
				&value);
		if (retval < 0)
			return retval;
		else
			bdata->power_on_state = value;
	} else
		bdata->power_gpio = -1;

	retval = of_property_read_u32(np, "synaptics,power-delay-ms",
			&value);
	if (retval < 0)
		bdata->power_delay_ms = 0;	/* No power delay by default */
	else
		bdata->power_delay_ms = value;

	bdata->reset_gpio = of_get_named_gpio_flags(np,
			"synaptics,reset-gpio", 0, NULL);
	if (bdata->reset_gpio >= 0) {
		retval = of_property_read_u32(np, "synaptics,reset-on-state",
				&value);
		if (retval < 0)
			return retval;
		else
			bdata->reset_on_state = value;

		retval = of_property_read_u32(np, "synaptics,reset-active-ms",
				&value);
		if (retval < 0)
			return retval;
		else
			bdata->reset_active_ms = value;
	} else
		bdata->reset_gpio = -1;

	retval = of_property_read_u32(np, "synaptics,reset-delay-ms",
			&value);
	if (retval < 0)
		bdata->reset_delay_ms = 0;	/* No reset delay by default */
	else
		bdata->reset_delay_ms = value;

	retval = of_property_read_u32(np, "synaptics,max-y-for-2d",
			&value);
	if (retval < 0)
		bdata->max_y_for_2d = -1;
	else
		bdata->max_y_for_2d = value;

	bdata->swap_axes = of_property_read_bool(np, "synaptics,swap-axes");

	bdata->x_flip = of_property_read_bool(np, "synaptics,x-flip");

	bdata->y_flip = of_property_read_bool(np, "synaptics,y-flip");

	retval = of_property_read_u32(np, "synaptics,ub-i2c-addr",
			&value);
	if (retval < 0)
		bdata->ub_i2c_addr = -1;
	else
		bdata->ub_i2c_addr = (unsigned short)value;

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

	if (of_property_read_bool(np, "synaptics,product-id-as-lockdown"))
		bdata->lockdown_area = LOCKDOWN_AREA_PRODUCT_ID;
	else if (of_property_read_bool(np, "synaptics,guest-serialization-as-lockdown"))
		bdata->lockdown_area = LOCKDOWN_AREA_GUEST_SERIALIZATION;
	else
		bdata->lockdown_area = LOCKDOWN_AREA_UNKNOWN;

	prop = of_find_property(np, "synaptics,tp-id-byte", NULL);
	if (prop && prop->length) {
		bdata->tp_id_bytes = devm_kzalloc(dev,
				prop->length,
				GFP_KERNEL);
		if (!bdata->tp_id_bytes)
			return -ENOMEM;
		bdata->tp_id_num = prop->length / sizeof(u8);
		retval = of_property_read_u8_array(np,
				"synaptics,tp-id-byte",
				bdata->tp_id_bytes,
				bdata->tp_id_num);
		if (retval < 0) {
			bdata->tp_id_num = 0;
			bdata->tp_id_bytes = NULL;
		}
	} else {
		dev_err(dev, "Don't know which byte of lockdown info to distinguish TP\n");
		bdata->tp_id_num = 0;
		bdata->tp_id_bytes = NULL;
	}

	retval = of_property_read_u32(np, "synaptics,config-array-size",
		&bdata->config_array_size);
	if (retval < 0) {
		dev_err(dev, "Cannot get config array size\n");
		return retval;
	}

	bdata->config_array = devm_kzalloc(dev, bdata->config_array_size *
					sizeof(struct synaptics_dsx_config_info), GFP_KERNEL);
	if (!bdata->config_array) {
		dev_err(dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	config_info = bdata->config_array;
	for_each_child_of_node(np, temp) {
		prop = of_find_property(temp, "synaptics,tp-id", NULL);
		if (prop && prop->length) {
			if (bdata->tp_id_num != prop->length / sizeof(u8)) {
				dev_err(dev, "Invalid TP id length\n");
				return -EINVAL;
			}
			config_info->tp_ids = devm_kzalloc(dev,
					prop->length,
					GFP_KERNEL);
			if (!config_info->tp_ids)
				return -ENOMEM;
			retval = of_property_read_u8_array(temp,
					"synaptics,tp-id",
					config_info->tp_ids,
					bdata->tp_id_num);
			if (retval < 0) {
				dev_err(dev, "Error reading TP id\n");
				return -EINVAL;
			}
		} else if (bdata->tp_id_num == 0) {
			/* No TP id indicated, skip */
			config_info->tp_ids = NULL;
		} else {
			dev_err(dev, "Cannot find TP id\n");
			return -EINVAL;
		}

		retval = of_property_read_string(temp, "synaptics,fw-name",
			&config_info->fw_name);
		if (retval && (retval != -EINVAL)) {
			dev_err(dev, "Unable to read firmware name\n");
			return retval;
		}
		config_info++;
	};

	dump_dt(dev, bdata);
	return 0;
}
#endif

static void synaptics_rmi4_i2c_check_addr(struct synaptics_rmi4_data *rmi4_data,
		struct i2c_client *i2c)
{
	if (hw_if.board_data->ub_i2c_addr == -1)
		return;

	if (hw_if.board_data->i2c_addr == i2c->addr)
		hw_if.board_data->i2c_addr = hw_if.board_data->ub_i2c_addr;
	else
		hw_if.board_data->i2c_addr = i2c->addr;

	return;
}

static int synaptics_rmi4_i2c_set_page(struct synaptics_rmi4_data *rmi4_data,
		unsigned short addr)
{
	int retval = -EINVAL;
	unsigned char retry;
	unsigned char buf[PAGE_SELECT_LEN];
	unsigned char page;
	struct i2c_client *i2c = to_i2c_client(rmi4_data->pdev->dev.parent);
	struct i2c_msg msg[1];

	msg[0].addr = hw_if.board_data->i2c_addr;
	msg[0].flags = 0;
	msg[0].len = PAGE_SELECT_LEN;
	msg[0].buf = buf;

	page = ((addr >> 8) & MASK_8BIT);
	buf[0] = MASK_8BIT;
	buf[1] = page;

	if (page != rmi4_data->current_page) {
		for (retry = 0; retry < SYN_I2C_RETRY_TIMES; retry++) {
			if (i2c_transfer(i2c->adapter, msg, 1) == 1) {
				rmi4_data->current_page = page;
				retval = PAGE_SELECT_LEN;
				break;
			}
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: I2C retry %d\n",
					__func__, retry + 1);
			msleep(5);

			if (retry == SYN_I2C_RETRY_TIMES / 2) {
				synaptics_rmi4_i2c_check_addr(rmi4_data, i2c);
				msg[0].addr = hw_if.board_data->i2c_addr;
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
#ifdef I2C_BURST_LIMIT
	unsigned char ii;
	unsigned char rd_msgs = ((length - 1) / I2C_BURST_LIMIT) + 1;
#else
	unsigned char rd_msgs = 1;
#endif
	unsigned char index = 0;
	unsigned char xfer_msgs;
	unsigned char remaining_msgs;
	unsigned short i2c_addr;
	unsigned short data_offset = 0;
	unsigned short remaining_length = length;
	struct i2c_client *i2c = to_i2c_client(rmi4_data->pdev->dev.parent);
	struct i2c_adapter *adap = i2c->adapter;
	struct i2c_msg msg[rd_msgs + 1];

	mutex_lock(&rmi4_data->rmi4_io_ctrl_mutex);

	retval = synaptics_rmi4_i2c_set_page(rmi4_data, addr);
	if (retval != PAGE_SELECT_LEN) {
		retval = -EIO;
		goto exit;
	}

	msg[0].addr = hw_if.board_data->i2c_addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &buf;

#ifdef I2C_BURST_LIMIT
	for (ii = 0; ii < (rd_msgs - 1); ii++) {
		msg[ii + 1].addr = hw_if.board_data->i2c_addr;
		msg[ii + 1].flags = I2C_M_RD;
		msg[ii + 1].len = I2C_BURST_LIMIT;
		msg[ii + 1].buf = &data[data_offset];
		data_offset += I2C_BURST_LIMIT;
		remaining_length -= I2C_BURST_LIMIT;
	}
#endif

	msg[rd_msgs].addr = hw_if.board_data->i2c_addr;
	msg[rd_msgs].flags = I2C_M_RD;
	msg[rd_msgs].len = remaining_length;
	msg[rd_msgs].buf = &data[data_offset];

	buf = addr & MASK_8BIT;

	remaining_msgs = rd_msgs + 1;

	while (remaining_msgs) {
#ifdef XFER_MSGS_LIMIT
		if (remaining_msgs > XFER_MSGS_LIMIT)
			xfer_msgs = XFER_MSGS_LIMIT;
		else
			xfer_msgs = remaining_msgs;
#else
		xfer_msgs = remaining_msgs;
#endif
		for (retry = 0; retry < SYN_I2C_RETRY_TIMES; retry++) {
			retval = i2c_transfer(adap, &msg[index], xfer_msgs);
			if (retval == xfer_msgs)
				break;

			dev_err(rmi4_data->pdev->dev.parent,
					"%s: I2C retry %d\n",
					__func__, retry + 1);
			msleep(5);

			if (retry == SYN_I2C_RETRY_TIMES / 2) {
				synaptics_rmi4_i2c_check_addr(rmi4_data, i2c);
				i2c_addr = hw_if.board_data->i2c_addr;
				msg[0].addr = i2c_addr;
#ifdef I2C_BURST_LIMIT
				for (ii = 0; ii < (rd_msgs - 1); ii++)
					msg[ii + 1].addr = i2c_addr;
#endif
				msg[rd_msgs].addr = i2c_addr;
			}
		}

		if (retry == SYN_I2C_RETRY_TIMES) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: I2C read over retry limit\n",
					__func__);
			retval = -EIO;
			goto exit;
		}

		remaining_msgs -= xfer_msgs;
		index += xfer_msgs;
	}

	retval = length;

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
	struct i2c_msg msg[1];

	mutex_lock(&rmi4_data->rmi4_io_ctrl_mutex);

	retval = synaptics_rmi4_i2c_set_page(rmi4_data, addr);
	if (retval != PAGE_SELECT_LEN) {
		retval = -EIO;
		goto exit;
	}

	msg[0].addr = hw_if.board_data->i2c_addr;
	msg[0].flags = 0;
	msg[0].len = length + 1;
	msg[0].buf = buf;

	buf[0] = addr & MASK_8BIT;
	retval = secure_memcpy(&buf[1], length, &data[0], length, length);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to copy data\n",
				__func__);
		goto exit;
	}

	for (retry = 0; retry < SYN_I2C_RETRY_TIMES; retry++) {
		if (i2c_transfer(i2c->adapter, msg, 1) == 1) {
			retval = length;
			break;
		}
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: I2C retry %d\n",
				__func__, retry + 1);
		msleep(5);

		if (retry == SYN_I2C_RETRY_TIMES / 2) {
			synaptics_rmi4_i2c_check_addr(rmi4_data, i2c);
			msg[0].addr = hw_if.board_data->i2c_addr;
		}
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

static void synaptics_rmi4_i2c_dev_release(struct device *dev)
{
	kfree(synaptics_dsx_i2c_device);

	return;
}

static int synaptics_rmi4_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *dev_id)
{
	int retval;

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

#ifdef CONFIG_OF
	if (client->dev.of_node) {
		hw_if.board_data = devm_kzalloc(&client->dev,
				sizeof(struct synaptics_dsx_board_data),
				GFP_KERNEL);
		if (!hw_if.board_data) {
			dev_err(&client->dev,
					"%s: Failed to allocate memory for board data\n",
					__func__);
			return -ENOMEM;
		}
		hw_if.board_data->cap_button_map = devm_kzalloc(&client->dev,
				sizeof(struct synaptics_dsx_button_map),
				GFP_KERNEL);
		if (!hw_if.board_data->cap_button_map) {
			dev_err(&client->dev,
					"%s: Failed to allocate memory for 0D button map\n",
					__func__);
			return -ENOMEM;
		}
		hw_if.board_data->vir_button_map = devm_kzalloc(&client->dev,
				sizeof(struct synaptics_dsx_button_map),
				GFP_KERNEL);
		if (!hw_if.board_data->vir_button_map) {
			dev_err(&client->dev,
					"%s: Failed to allocate memory for virtual button map\n",
					__func__);
			return -ENOMEM;
		}
		parse_dt(&client->dev, hw_if.board_data);
	}
#else
	hw_if.board_data = client->dev.platform_data;
#endif

	hw_if.bus_access = &bus_access;
	hw_if.board_data->i2c_addr = client->addr;

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
static struct of_device_id synaptics_rmi4_of_match_table[] = {
	{
		.compatible = "synaptics,dsx-i2c",
	},
	{},
};
MODULE_DEVICE_TABLE(of, synaptics_rmi4_of_match_table);
#else
#define synaptics_rmi4_of_match_table NULL
#endif

static struct i2c_driver synaptics_rmi4_i2c_driver = {
	.driver = {
		.name = I2C_DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = synaptics_rmi4_of_match_table,
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
