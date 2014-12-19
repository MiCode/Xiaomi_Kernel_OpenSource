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
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/acpi.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/input/synaptics_dsx.h>
#include "synaptics_dsx_core.h"

#define SYNP_S7300_CHTCR_ID	"SYNP1000"

#define SYN_I2C_RETRY_TIMES 10

#define REPORT_ID_GET_BLOB 0x07
#define REPORT_ID_WRITE 0x09
#define REPORT_ID_READ_ADDRESS 0x0a
#define REPORT_ID_READ_DATA 0x0b
#define REPORT_ID_SET_RMI_MODE 0x0f

#define PREFIX_USAGE_PAGE_1BYTE 0x05
#define PREFIX_USAGE_PAGE_2BYTES 0x06
#define PREFIX_USAGE 0x09
#define PREFIX_REPORT_ID 0x85
#define PREFIX_REPORT_COUNT_1BYTE 0x95
#define PREFIX_REPORT_COUNT_2BYTES 0x96

#define USAGE_GET_BLOB 0xc5
#define USAGE_WRITE 0x02
#define USAGE_READ_ADDRESS 0x03
#define USAGE_READ_DATA 0x04
#define USAGE_SET_MODE 0x06

#define FEATURE_REPORT_TYPE 0x03

#define VENDOR_DEFINED_PAGE 0xff00

#define BLOB_REPORT_SIZE 256

#define RESET_COMMAND 0x01
#define GET_REPORT_COMMAND 0x02
#define SET_REPORT_COMMAND 0x03
#define SET_POWER_COMMAND 0x08

#define FINGER_MODE 0x00
#define RMI_MODE 0x02

#define DSX_HID_DEVICE_DESCRIPTOR_ADDR 0x0020
#define HID_WRITE_CMD_LEN 2
#define HID_READ_CMD_LEN 4
#define MASK_BLOB_SIZE 0x03
#define GPIO_MAX_RETRY 10

#define S7300_GPIO_DEF -1
#define S7300_POWER_DELAY_MS 100
#define S7300_RESET_DELAY_MS 160
#define S7300_RESET_ACTIVE_MS 20
#define S7300_BYTE_DELAY_US 20
#define S7300_BLOCK_DELAY_US 20
#define DSX_MAX_Y_FOR_2D -1 /* set to -1 if no virtual buttons */

static unsigned int cap_button_codes[] = {};
static unsigned int vir_button_codes[] = {};

static struct synaptics_dsx_button_map cap_button_map = {
	.nbuttons = ARRAY_SIZE(cap_button_codes),
	.map = cap_button_codes,
};

static struct synaptics_dsx_button_map vir_button_map = {
	.nbuttons = ARRAY_SIZE(vir_button_codes) / 5,
	.map = vir_button_codes,
};

struct hid_report_info {
	unsigned char get_blob_id;
	unsigned char write_id;
	unsigned char read_addr_id;
	unsigned char read_data_id;
	unsigned char set_mode_id;
	unsigned int blob_size;
};

static struct hid_report_info hid_report;

struct hid_device_descriptor {
	unsigned short device_descriptor_length;
	unsigned short format_version;
	unsigned short report_descriptor_length;
	unsigned short report_descriptor_index;
	unsigned short input_register_index;
	unsigned short input_report_max_length;
	unsigned short output_register_index;
	unsigned short output_report_max_length;
	unsigned short command_register_index;
	unsigned short data_register_index;
	unsigned short vendor_id;
	unsigned short product_id;
	unsigned short version_id;
	unsigned int reserved;
};

static struct hid_device_descriptor hid_dd;

struct i2c_rw_buffer {
	unsigned char *read;
	unsigned char *write;
	unsigned short read_size;
	unsigned short write_size;
};

static struct i2c_rw_buffer buffer;

static struct synaptics_dsx_board_data s7300_board_data = {
	.irq_flags = IRQ_TYPE_EDGE_FALLING | IRQF_ONESHOT,
	.irq_gpio = S7300_GPIO_DEF,
	.power_gpio = S7300_GPIO_DEF,
	.reset_gpio = S7300_GPIO_DEF,
	.power_delay_ms = S7300_POWER_DELAY_MS,
	.reset_delay_ms = S7300_RESET_DELAY_MS,
	.reset_active_ms = S7300_RESET_ACTIVE_MS,
	.byte_delay_us = S7300_BYTE_DELAY_US,
	.block_delay_us = S7300_BLOCK_DELAY_US,
	.max_y_for_2d = DSX_MAX_Y_FOR_2D,
	.cap_button_map = &cap_button_map,
	.vir_button_map = &vir_button_map,
	.device_descriptor_addr = DSX_HID_DEVICE_DESCRIPTOR_ADDR,
};

static int do_i2c_transfer(struct i2c_client *client, struct i2c_msg *msg)
{
	unsigned char retry;

	for (retry = 0; retry < SYN_I2C_RETRY_TIMES; retry++) {
		if (i2c_transfer(client->adapter, msg, 1) == 1)
			break;
		dev_err(&client->dev,
				"%s: I2C retry %d\n",
				__func__, retry + 1);
		msleep(20);
	}

	if (retry == SYN_I2C_RETRY_TIMES) {
		dev_err(&client->dev,
				"%s: I2C transfer over retry limit\n",
				__func__);
		return -EIO;
	}

	return 0;
}

static int check_buffer(unsigned char **buffer, unsigned short *buffer_size,
		unsigned short length)
{
	if (*buffer_size < length) {
		if (*buffer_size)
			kfree(*buffer);
		*buffer = kzalloc(length, GFP_KERNEL);
		if (!(*buffer))
			return -ENOMEM;
		*buffer_size = length;
	}

	return 0;
}

static int generic_read(struct i2c_client *client, unsigned short length)
{
	int retval;
	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = length,
		}
	};

	retval = check_buffer(&buffer.read, &buffer.read_size, length);
	if (retval) {
		dev_err(&client->dev, "%s: check_buffer error\n", __func__);
		return retval;
	}
	msg[0].buf = buffer.read;

	retval = do_i2c_transfer(client, msg);

	return retval;
}

static int generic_write(struct i2c_client *client, unsigned short length)
{
	int retval;
	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = length,
			.buf = buffer.write,
		}
	};

	retval = do_i2c_transfer(client, msg);

	return retval;
}

static void traverse_report_descriptor(unsigned int *index)
{
	unsigned char size;
	unsigned char *buf = buffer.read;

	size = buf[*index] & MASK_2BIT;
	switch (size) {
	case 0: /* 0 bytes */
		*index += 1;
		break;
	case 1: /* 1 byte */
		*index += 2;
		break;
	case 2: /* 2 bytes */
		*index += 3;
		break;
	case 3: /* 4 bytes */
		*index += 5;
		break;
	default:
		break;
	}

	return;
}

static void find_blob_size(unsigned int index)
{
	unsigned int ii = index;
	unsigned char *buf = buffer.read;

	while (ii < hid_dd.report_descriptor_length) {
		if (buf[ii] == PREFIX_REPORT_COUNT_1BYTE) {
			hid_report.blob_size = buf[ii + 1];
			return;
		} else if (buf[ii] == PREFIX_REPORT_COUNT_2BYTES) {
			hid_report.blob_size = buf[ii + 1] | (buf[ii + 2] << 8);
			return;
		}
		traverse_report_descriptor(&ii);
	}

	return;
}

static void find_reports(unsigned int index)
{
	unsigned int ii = index;
	unsigned char *buf = buffer.read;
	static unsigned int report_id_index;
	static unsigned char report_id;
	static unsigned short usage_page;

	if (buf[ii] == PREFIX_REPORT_ID) {
		report_id = buf[ii + 1];
		report_id_index = ii;
		return;
	}

	if (buf[ii] == PREFIX_USAGE_PAGE_1BYTE) {
		usage_page = buf[ii + 1];
		return;
	} else if (buf[ii] == PREFIX_USAGE_PAGE_2BYTES) {
		usage_page = buf[ii + 1] | (buf[ii + 2] << 8);
		return;
	}

	if ((usage_page == VENDOR_DEFINED_PAGE) && (buf[ii] == PREFIX_USAGE)) {
		switch (buf[ii + 1]) {
		case USAGE_GET_BLOB:
			hid_report.get_blob_id = report_id;
			find_blob_size(report_id_index);
			break;
		case USAGE_WRITE:
			hid_report.write_id = report_id;
			break;
		case USAGE_READ_ADDRESS:
			hid_report.read_addr_id = report_id;
			break;
		case USAGE_READ_DATA:
			hid_report.read_data_id = report_id;
			break;
		case USAGE_SET_MODE:
			hid_report.set_mode_id = report_id;
			break;
		default:
			break;
		}
	}

	return;
}

static int parse_report_descriptor(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned int ii = 0;
	unsigned char *buf;
	struct i2c_client *i2c = to_i2c_client(rmi4_data->pdev->dev.parent);

	buffer.write[0] = hid_dd.report_descriptor_index & MASK_8BIT;
	buffer.write[1] = hid_dd.report_descriptor_index >> 8;
	retval = generic_write(i2c, 2);
	if (retval < 0)
		return retval;
	retval = generic_read(i2c, hid_dd.report_descriptor_length);
	if (retval < 0)
		return retval;

	buf = buffer.read;

	hid_report.get_blob_id = REPORT_ID_GET_BLOB;
	hid_report.write_id = REPORT_ID_WRITE;
	hid_report.read_addr_id = REPORT_ID_READ_ADDRESS;
	hid_report.read_data_id = REPORT_ID_READ_DATA;
	hid_report.set_mode_id = REPORT_ID_SET_RMI_MODE;
	hid_report.blob_size = BLOB_REPORT_SIZE;

	while (ii < hid_dd.report_descriptor_length) {
		find_reports(ii);
		traverse_report_descriptor(&ii);
	}

	return 0;
}

static int switch_to_rmi(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	struct i2c_client *i2c = to_i2c_client(rmi4_data->pdev->dev.parent);

	mutex_lock(&rmi4_data->rmi4_io_ctrl_mutex);

	retval = check_buffer(&buffer.write, &buffer.write_size, 11);
	if (retval) {
		mutex_unlock(&rmi4_data->rmi4_io_ctrl_mutex);
		dev_err(&i2c->dev, "%s: check_buffer error\n", __func__);
		return retval;
	}

	/* set rmi mode */
	buffer.write[0] = hid_dd.command_register_index & MASK_8BIT;
	buffer.write[1] = hid_dd.command_register_index >> 8;
	buffer.write[2] = (FEATURE_REPORT_TYPE << 4) | hid_report.set_mode_id;
	buffer.write[3] = SET_REPORT_COMMAND;
	buffer.write[4] = hid_report.set_mode_id;
	buffer.write[5] = hid_dd.data_register_index & MASK_8BIT;
	buffer.write[6] = hid_dd.data_register_index >> 8;
	buffer.write[7] = 0x04;
	buffer.write[8] = 0x00;
	buffer.write[9] = hid_report.set_mode_id;
	buffer.write[10] = RMI_MODE;

	retval = generic_write(i2c, 11);

	mutex_unlock(&rmi4_data->rmi4_io_ctrl_mutex);

	return retval;
}

static int check_report_mode(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned short report_size;
	struct i2c_client *i2c = to_i2c_client(rmi4_data->pdev->dev.parent);

	mutex_lock(&rmi4_data->rmi4_io_ctrl_mutex);

	retval = check_buffer(&buffer.write, &buffer.write_size, 7);
	if (retval) {
		dev_err(&i2c->dev, "%s: check_buffer error\n", __func__);
		goto exit;
	}

	buffer.write[0] = hid_dd.command_register_index & MASK_8BIT;
	buffer.write[1] = hid_dd.command_register_index >> 8;
	buffer.write[2] = (FEATURE_REPORT_TYPE << 4) | hid_report.set_mode_id;
	buffer.write[3] = GET_REPORT_COMMAND;
	buffer.write[4] = hid_report.set_mode_id;
	buffer.write[5] = hid_dd.data_register_index & MASK_8BIT;
	buffer.write[6] = hid_dd.data_register_index >> 8;

	retval = generic_write(i2c, 7);
	if (retval < 0)
		goto exit;

	retval = generic_read(i2c, 2);
	if (retval < 0)
		goto exit;

	report_size = (buffer.read[1] << 8) | buffer.read[0];

	retval = generic_write(i2c, 7);
	if (retval < 0)
		goto exit;

	retval = generic_read(i2c, report_size);
	if (retval < 0)
		goto exit;

	retval = buffer.read[3];
	dev_dbg(rmi4_data->pdev->dev.parent,
			"%s: Report mode = %d\n",
			__func__, retval);

exit:
	mutex_unlock(&rmi4_data->rmi4_io_ctrl_mutex);

	return retval;
}

static int hid_i2c_init(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	struct i2c_client *i2c = to_i2c_client(rmi4_data->pdev->dev.parent);
	const struct synaptics_dsx_board_data *bdata =
			rmi4_data->hw_if->board_data;
	int wait_times = 100;

	mutex_lock(&rmi4_data->rmi4_io_ctrl_mutex);

	retval = check_buffer(&buffer.write, &buffer.write_size, 6);
	if (retval)
		goto exit;

	/* read device descriptor */
	buffer.write[0] = bdata->device_descriptor_addr & MASK_8BIT;
	buffer.write[1] = bdata->device_descriptor_addr >> 8;
	retval = generic_write(i2c, 2);
	if (retval < 0)
		goto exit;
	retval = generic_read(i2c, sizeof(hid_dd));
	if (retval < 0)
		goto exit;
	memcpy((unsigned char *)&hid_dd, buffer.read, sizeof(hid_dd));

	retval = parse_report_descriptor(rmi4_data);
	if (retval < 0)
		goto exit;

	/* set power */
	buffer.write[0] = hid_dd.command_register_index & MASK_8BIT;
	buffer.write[1] = hid_dd.command_register_index >> 8;
	buffer.write[2] = 0x00;
	buffer.write[3] = SET_POWER_COMMAND;
	retval = generic_write(i2c, 4);
	if (retval < 0)
		goto exit;

	/* reset */
	buffer.write[0] = hid_dd.command_register_index & MASK_8BIT;
	buffer.write[1] = hid_dd.command_register_index >> 8;
	buffer.write[2] = 0x00;
	buffer.write[3] = RESET_COMMAND;
	retval = generic_write(i2c, 4);
	if (retval < 0)
		goto exit;

	while (wait_times-- > 0 && gpio_get_value(bdata->irq_gpio))
		msleep(20);

	retval = generic_read(i2c, hid_dd.input_report_max_length);
	if (retval < 0)
		goto exit;

	/* get blob */
	buffer.write[0] = hid_dd.command_register_index & MASK_8BIT;
	buffer.write[1] = hid_dd.command_register_index >> 8;
	buffer.write[2] = (FEATURE_REPORT_TYPE << 4) | hid_report.get_blob_id;
	buffer.write[3] = 0x02;
	buffer.write[4] = hid_dd.data_register_index & MASK_8BIT;
	buffer.write[5] = hid_dd.data_register_index >> 8;

	retval = generic_write(i2c, 6);
	if (retval < 0)
		goto exit;

	msleep(20);

	retval = generic_read(i2c, hid_report.blob_size + 3);
	if (retval < 0)
		goto exit;

exit:
	mutex_unlock(&rmi4_data->rmi4_io_ctrl_mutex);

	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to initialize HID/I2C interface\n",
				__func__);
		return retval;
	}

	retval = switch_to_rmi(rmi4_data);

	return retval;
}

static int synaptics_rmi4_i2c_read(struct synaptics_rmi4_data *rmi4_data,
		unsigned short addr, unsigned char *data, unsigned short length)
{
	int retval;
	unsigned char retry;
	unsigned char recover = 1;
	unsigned short report_length;
	struct i2c_client *i2c = to_i2c_client(rmi4_data->pdev->dev.parent);
	struct i2c_msg msg[] = {
		{
			.addr = i2c->addr,
			.flags = 0,
			.len = hid_dd.output_report_max_length + 2,
		},
		{
			.addr = i2c->addr,
			.flags = I2C_M_RD,
			.len = length + 4,
		},
	};

recover:
	mutex_lock(&rmi4_data->rmi4_io_ctrl_mutex);

	retval = check_buffer(&buffer.write, &buffer.write_size,
			hid_dd.output_report_max_length + 2);
	if (retval) {
		mutex_unlock(&rmi4_data->rmi4_io_ctrl_mutex);
		dev_err(&i2c->dev, "%s: check_buffer error\n", __func__);
		return retval;
	}
	msg[0].buf = buffer.write;
	buffer.write[0] = hid_dd.output_register_index & MASK_8BIT;
	buffer.write[1] = hid_dd.output_register_index >> 8;
	buffer.write[2] = hid_dd.output_report_max_length & MASK_8BIT;
	buffer.write[3] = hid_dd.output_report_max_length >> 8;
	buffer.write[4] = hid_report.read_addr_id;
	buffer.write[5] = 0x00;
	buffer.write[6] = addr & MASK_8BIT;
	buffer.write[7] = addr >> 8;
	buffer.write[8] = length & MASK_8BIT;
	buffer.write[9] = length >> 8;

	retval = check_buffer(&buffer.read, &buffer.read_size, length + 4);
	if (retval) {
		dev_err(&i2c->dev, "%s: check_buffer error\n", __func__);
		mutex_unlock(&rmi4_data->rmi4_io_ctrl_mutex);
		if (buffer.write_size)
			kfree(buffer.write);
		return retval;
	}
	msg[1].buf = buffer.read;

	retval = do_i2c_transfer(i2c, &msg[0]);
	if (retval != 0)
		goto exit;

	retry = 0;
	do {
		retval = do_i2c_transfer(i2c, &msg[1]);
		if (retval == 0)
			retval = length;
		else
			goto exit;

		report_length = (buffer.read[1] << 8) | buffer.read[0];
		if (report_length == hid_dd.input_report_max_length) {
			memcpy(&data[0], &buffer.read[4], length);
			goto exit;
		}

		msleep(20);
		retry++;
	} while (retry < SYN_I2C_RETRY_TIMES);

	dev_err(rmi4_data->pdev->dev.parent,
			"%s: Failed to receive read report\n",
			__func__);
	retval = -EIO;

exit:
	mutex_unlock(&rmi4_data->rmi4_io_ctrl_mutex);

	if ((retval != length) && (recover == 1)) {
		recover = 0;
		if (check_report_mode(rmi4_data) != RMI_MODE) {
			retval = hid_i2c_init(rmi4_data);
			if (retval == 0)
				goto recover;
		}
	}

	return retval;
}

static int synaptics_rmi4_i2c_write(struct synaptics_rmi4_data *rmi4_data,
		unsigned short addr, unsigned char *data, unsigned short length)
{
	int retval;
	unsigned char recover = 1;
	unsigned char msg_length;
	struct i2c_client *i2c = to_i2c_client(rmi4_data->pdev->dev.parent);
	struct i2c_msg msg[] = {
		{
			.addr = i2c->addr,
			.flags = 0,
		}
	};

	if ((length + 10) < (hid_dd.output_report_max_length + 2))
		msg_length = hid_dd.output_report_max_length + 2;
	else
		msg_length = length + 10;

recover:
	mutex_lock(&rmi4_data->rmi4_io_ctrl_mutex);

	retval = check_buffer(&buffer.write, &buffer.write_size, msg_length);
	if (retval) {
		mutex_unlock(&rmi4_data->rmi4_io_ctrl_mutex);
		dev_err(&i2c->dev, "%s: check_buffer error\n", __func__);
		return retval;
	}
	msg[0].len = msg_length;
	msg[0].buf = buffer.write;
	buffer.write[0] = hid_dd.output_register_index & MASK_8BIT;
	buffer.write[1] = hid_dd.output_register_index >> 8;
	buffer.write[2] = hid_dd.output_report_max_length & MASK_8BIT;
	buffer.write[3] = hid_dd.output_report_max_length >> 8;
	buffer.write[4] = hid_report.write_id;
	buffer.write[5] = 0x00;
	buffer.write[6] = addr & MASK_8BIT;
	buffer.write[7] = addr >> 8;
	buffer.write[8] = length & MASK_8BIT;
	buffer.write[9] = length >> 8;
	memcpy(&buffer.write[10], &data[0], length);

	retval = do_i2c_transfer(i2c, msg);
	if (retval == 0)
		retval = length;

	mutex_unlock(&rmi4_data->rmi4_io_ctrl_mutex);

	if ((retval != length) && (recover == 1)) {
		recover = 0;
		if (check_report_mode(rmi4_data) != RMI_MODE) {
			retval = hid_i2c_init(rmi4_data);
			if (retval == 0)
				goto recover;
		}
	}

	return retval;
}

static struct synaptics_dsx_bus_access bus_access = {
	.type = BUS_I2C,
	.read = synaptics_rmi4_i2c_read,
	.write = synaptics_rmi4_i2c_write,
};

static struct synaptics_dsx_hw_interface hw_if;

static struct platform_device *synaptics_dsx_i2c_device;

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

	if (!strncmp(SYNP_S7300_CHTCR_ID, client->name,
			strlen(SYNP_S7300_CHTCR_ID))) {
		struct gpio_desc *gpio;

		hw_if.board_data = &s7300_board_data;
		s7300_board_data.irq = client->irq;

		gpio = devm_gpiod_get_index(&client->dev, "synaptic_irq", 0);
		if (!IS_ERR(gpio)) {
			s7300_board_data.irq_gpio = desc_to_gpio(gpio);
			s7300_board_data.irq =
				gpio_to_irq(s7300_board_data.irq_gpio);
		} else
			dev_err(&client->dev, "Failed to get irq gpio\n");

		gpio = devm_gpiod_get_index(&client->dev, "synaptic_reset", 1);
		if (!IS_ERR(gpio))
			s7300_board_data.reset_gpio = desc_to_gpio(gpio);
		else
			dev_err(&client->dev, "Failed to get reset gpio\n");

		dev_info(&client->dev, "addr 0x%02x,reset %d,irq %d\n",
			client->addr, s7300_board_data.reset_gpio,
			s7300_board_data.irq_gpio);
	} else
		dev_info(&client->dev, "%s %s\n", client->name, dev_id->name);

	hw_if.bus_access = &bus_access;
	hw_if.bl_hw_init = switch_to_rmi;
	hw_if.ui_hw_init = hid_i2c_init;

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
	if (buffer.read_size)
		kfree(buffer.read);

	if (buffer.write_size)
		kfree(buffer.write);

	platform_device_unregister(synaptics_dsx_i2c_device);

	return 0;
}

static const struct i2c_device_id synaptics_rmi4_id_table[] = {
	{ HID_I2C_DRIVER_NAME, 0 },
	{ SYNP_S7300_CHTCR_ID, 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, synaptics_rmi4_id_table);

#ifdef CONFIG_ACPI
static struct acpi_device_id acpi_match[] = {
	{ SYNP_S7300_CHTCR_ID, 0 },
	{ },
};
MODULE_DEVICE_TABLE(acpi, acpi_match);
#endif

static struct i2c_driver synaptics_rmi4_i2c_driver = {
	.driver = {
		.name = HID_I2C_DRIVER_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_ACPI
		.acpi_match_table = ACPI_PTR(acpi_match),
#endif
	},
	.probe = synaptics_rmi4_i2c_probe,
	.remove = synaptics_rmi4_i2c_remove,
	.id_table = synaptics_rmi4_id_table,
};

int synaptics_rmi4_bus_hid_i2c_init(void)
{
	return i2c_add_driver(&synaptics_rmi4_i2c_driver);
}
EXPORT_SYMBOL(synaptics_rmi4_bus_hid_i2c_init);

void synaptics_rmi4_bus_hid_i2c_exit(void)
{
	i2c_del_driver(&synaptics_rmi4_i2c_driver);
	return;
}
EXPORT_SYMBOL(synaptics_rmi4_bus_hid_i2c_exit);

MODULE_AUTHOR("Synaptics, Inc.");
MODULE_DESCRIPTION("Synaptics DSX HID I2C Bus Support Module");
MODULE_LICENSE("GPL v2");
