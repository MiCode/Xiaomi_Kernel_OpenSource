/*
 * Synaptics RMI4 touchscreen driver
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/firmware.h>
#include <linux/string.h>
#include <linux/input/synaptics_dsx.h>
#include "synaptics_i2c_rmi4.h"

#define SHOW_PROGRESS
#define MAX_FIRMWARE_ID_LEN 10
#define FORCE_UPDATE false
#define INSIDE_FIRMWARE_UPDATE

#define FW_IMAGE_OFFSET 0x100
/* 0 to ignore flash block check to speed up flash time */
#define CHECK_FLASH_BLOCK_STATUS 1

#define REG_MAP (1 << 0)
#define UNLOCKED (1 << 1)
#define HAS_CONFIG_ID (1 << 2)
#define HAS_PERM_CONFIG (1 << 3)
#define HAS_BL_CONFIG (1 << 4)
#define HAS_DISP_CONFIG (1 << 5)
#define HAS_CTRL1 (1 << 6)

#define RMI4_INFO_MAX_LEN	200

#define RMI4_STORE_TS_INFO(buf, id, rev, fw_ver) \
		snprintf(buf, RMI4_INFO_MAX_LEN, \
			"controller\t= synaptics\n" \
			"model\t\t= %d rev %d\n" \
			"fw_ver\t\t= %d\n", id, rev, fw_ver)

enum falsh_config_area {
	UI_CONFIG_AREA = 0x00,
	PERM_CONFIG_AREA = 0x01,
	BL_CONFIG_AREA = 0x02,
	DISP_CONFIG_AREA = 0x03
};

enum flash_command {
	CMD_WRITE_FW_BLOCK		= 0x2,
	CMD_ERASE_ALL			= 0x3,
	CMD_WRITE_LOCKDOWN_BLOCK	= 0x4,
	CMD_READ_CONFIG_BLOCK	= 0x5,
	CMD_WRITE_CONFIG_BLOCK	= 0x6,
	CMD_ERASE_CONFIG		= 0x7,
	CMD_READ_SENSOR_ID		= 0x8,
	CMD_ERASE_BL_CONFIG		= 0x9,
	CMD_ERASE_DISP_CONFIG	= 0xA,
	CMD_ENABLE_FLASH_PROG	= 0xF
};

enum flash_area {
	NONE,
	UI_FIRMWARE,
	CONFIG_AREA,
	MISMATCH
};

enum image_file_option {
	OPTION_BUILD_INFO = 0,
	OPTION_CONTAIN_BOOTLOADER = 1,
};

enum flash_offset {
	OFFSET_BOOTLOADER_ID,
	OFFSET_FLASH_PROPERTIES,
	OFFSET_BLOCK_SIZE,
	OFFSET_FW_BLOCK_COUNT,
	OFFSET_BLOCK_NUMBER,
	OFFSET_BLOCK_DATA,
	OFFSET_FLASH_CONTROL,
	OFFSET_FLASH_STATUS
};

enum flash_update_mode {
	NORMAL = 1,
	FORCE = 2,
	LOCKDOWN = 8
};

#define SLEEP_MODE_NORMAL (0x00)
#define SLEEP_MODE_SENSOR_SLEEP (0x01)
#define SLEEP_MODE_RESERVED0 (0x02)
#define SLEEP_MODE_RESERVED1 (0x03)

#define ENABLE_WAIT_MS (1 * 1000)
#define WRITE_WAIT_MS (3 * 1000)
#define ERASE_WAIT_MS (5 * 1000)
#define RESET_WAIT_MS (500)

#define SLEEP_TIME_US 100

static int fwu_wait_for_idle(int timeout_ms);

struct image_header_data {
	union {
		struct {
			/* 0x00-0x0F */
			unsigned char file_checksum[4];
			unsigned char reserved_04;
			unsigned char reserved_05;
			unsigned char options_firmware_id:1;
			unsigned char options_contain_bootloader:1;
			unsigned char options_reserved:6;
			unsigned char bootloader_version;
			unsigned char firmware_size[4];
			unsigned char config_size[4];
			/* 0x10-0x1F */
			unsigned char product_id[SYNAPTICS_RMI4_PRODUCT_ID_SIZE];
			unsigned char pkg_id_lsb;
			unsigned char pkg_id_msb;
			unsigned char pkg_id_rev_lsb;
			unsigned char pkg_id_rev_msb;
			unsigned char product_info[SYNAPTICS_RMI4_PRODUCT_INFO_SIZE];
			/* 0x20-0x2F */
			unsigned char reserved_20_2f[0x10];
			/* 0x30-0x3F */
			unsigned char ds_firmware_id[0x10];
			/* 0x40-0x4F */
			unsigned char ds_customize_info[10];
			unsigned char reserved_4a_4f[6];
			/* 0x50-0x53*/
			unsigned char firmware_id[4];
		} __packed;
		unsigned char data[0x54];
	};
};

struct image_content {
	bool is_contain_build_info;
	unsigned int checksum;
	unsigned int image_size;
	unsigned int config_size;
	unsigned char options;
	unsigned char bootloader_version;
	unsigned char product_id[SYNAPTICS_RMI4_PRODUCT_ID_SIZE + 1];
	unsigned char product_info[SYNAPTICS_RMI4_PRODUCT_INFO_SIZE];
	u16 package_id;
	u16 package_revision_id;
	unsigned int firmware_id;
	const unsigned char *firmware_data;
	const unsigned char *config_data;
	const unsigned char *lockdown_data;
	unsigned short lockdown_block_count;
};

struct pdt_properties {
	union {
		struct {
			unsigned char reserved_1:6;
			unsigned char has_bsr:1;
			unsigned char reserved_2:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f01_device_status {
	union {
		struct {
			unsigned char status_code:4;
			unsigned char reserved:2;
			unsigned char flash_prog:1;
			unsigned char unconfigured:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f01_device_control {
	union {
		struct {
			unsigned char sleep_mode:2;
			unsigned char nosleep:1;
			unsigned char reserved:2;
			unsigned char charger_connected:1;
			unsigned char report_rate:1;
			unsigned char configured:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f34_flash_control {
	union {
	/* version 0 */
		struct {
			unsigned char command_v0:4;
			unsigned char status:3;
			unsigned char program_enabled:1;
		} __packed;
	/* version 1 */
		struct {
			unsigned char command_v1:6;
			unsigned char reserved:2;
		} __packed;
		unsigned char data[1];
	};
};

struct f34_flash_status {
	union {
		struct {
			unsigned char status:6;
			unsigned char reserved:1;
			unsigned char program_enabled:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f34_flash_properties {
	union {
		struct {
			unsigned char regmap:1;
			unsigned char unlocked:1;
			unsigned char has_configid:1;
			unsigned char has_perm_config:1;
			unsigned char has_bl_config:1;
			unsigned char has_display_config:1;
			unsigned char has_blob_config:1;
			unsigned char reserved:1;
		} __packed;
		unsigned char data[1];
	};
};

struct synaptics_rmi4_fwu_handle {
	bool initialized;
	bool force_update;
	bool do_lockdown;
	bool interrupt_flag;
	bool polling_mode;
	char product_id[SYNAPTICS_RMI4_PRODUCT_ID_SIZE + 1];
	unsigned int image_size;
	unsigned int data_pos;
	unsigned char intr_mask;
	unsigned char bootloader_id[2];
	unsigned char productinfo1;
	unsigned char productinfo2;
	unsigned char *ext_data_source;
	unsigned char *read_config_buf;
	const unsigned char *firmware_data;
	const unsigned char *config_data;
	const unsigned char *lockdown_data;
	unsigned short block_size;
	unsigned short fw_block_count;
	unsigned short config_block_count;
	unsigned short lockdown_block_count;
	unsigned short perm_config_block_count;
	unsigned short bl_config_block_count;
	unsigned short disp_config_block_count;
	unsigned short config_size;
	unsigned short config_area;
	unsigned short addr_f01_interrupt_register;
	const unsigned char *data_buffer;
	struct synaptics_rmi4_fn_desc f01_fd;
	struct synaptics_rmi4_fn_desc f34_fd;
	struct synaptics_rmi4_exp_fn_ptr *fn_ptr;
	struct synaptics_rmi4_data *rmi4_data;
	struct f34_flash_properties flash_properties;
	struct workqueue_struct *fwu_workqueue;
	struct delayed_work fwu_work;
	char image_name[NAME_BUFFER_SIZE];
	struct image_content image_content;
	char *ts_info;
};

static struct synaptics_rmi4_fwu_handle *fwu;

DECLARE_COMPLETION(fwu_remove_complete);

static unsigned int extract_uint(const unsigned char *ptr)
{
	return (unsigned int)ptr[0] +
			(unsigned int)ptr[1] * 0x100 +
			(unsigned int)ptr[2] * 0x10000 +
			(unsigned int)ptr[3] * 0x1000000;
}

static unsigned int extract_uint_be(const unsigned char *ptr)
{
	return (unsigned int)ptr[3] +
			(unsigned int)ptr[2] * 0x100 +
			(unsigned int)ptr[1] * 0x10000 +
			(unsigned int)ptr[0] * 0x1000000;
}

static void synaptics_rmi4_update_debug_info(void)
{
	unsigned char pkg_id[4];
	unsigned int build_id;
	struct synaptics_rmi4_device_info *rmi;
	/* read device package id */
	fwu->fn_ptr->read(fwu->rmi4_data,
				fwu->f01_fd.query_base_addr + 17,
				pkg_id,
				sizeof(pkg_id));
	rmi = &(fwu->rmi4_data->rmi4_mod_info);

	build_id = (unsigned int)rmi->build_id[0] +
			(unsigned int)rmi->build_id[1] * 0x100 +
			(unsigned int)rmi->build_id[2] * 0x10000;

	RMI4_STORE_TS_INFO(fwu->ts_info, pkg_id[1] << 8 | pkg_id[0],
		pkg_id[3] << 8 | pkg_id[2], build_id);
}

static void parse_header(void)
{
	struct image_content *img = &fwu->image_content;
	struct image_header_data *data =
		(struct image_header_data *)fwu->data_buffer;
	img->checksum = extract_uint(data->file_checksum);
	img->bootloader_version = data->bootloader_version;
	img->image_size = extract_uint(data->firmware_size);
	img->config_size = extract_uint(data->config_size);
	memcpy(img->product_id, data->product_id,
		sizeof(data->product_id));
	img->product_id[sizeof(data->product_id)] = 0;

	img->product_id[sizeof(data->product_info)] = 0;
	memcpy(img->product_info, data->product_info,
		sizeof(data->product_info));

	img->is_contain_build_info =
		(data->options_firmware_id == (1 << OPTION_BUILD_INFO));

	if (img->is_contain_build_info) {
		img->package_id = (data->pkg_id_msb << 8) |
				data->pkg_id_lsb;
		img->package_revision_id = (data->pkg_id_rev_msb << 8) |
				data->pkg_id_rev_lsb;
		dev_info(&fwu->rmi4_data->i2c_client->dev,
			"%s Package ID %d Rev %d\n", __func__,
			img->package_id, img->package_revision_id);

		img->firmware_id = extract_uint(data->firmware_id);
		dev_info(&fwu->rmi4_data->i2c_client->dev,
			"%s Firwmare build id %d\n", __func__,
			img->firmware_id);
	}

	dev_dbg(&fwu->rmi4_data->i2c_client->dev,
		"Firwmare size %d, config size %d\n",
		img->image_size,
		img->config_size);

	/* get UI firmware offset */
	if (img->image_size)
		img->firmware_data = fwu->data_buffer + FW_IMAGE_OFFSET;
	/* get config offset*/
	if (img->config_size)
		img->config_data = fwu->data_buffer + FW_IMAGE_OFFSET +
				img->image_size;
	/* get lockdown offset*/
	switch (img->bootloader_version) {
	case 3:
	case 4:
		img->lockdown_block_count = 4;
		break;
	case 5:
	case 6:
		img->lockdown_block_count = 5;
		break;
	default:
		dev_warn(&fwu->rmi4_data->i2c_client->dev,
			"%s: Not support lockdown in " \
			"bootloader version V%d\n",
			__func__, img->bootloader_version);
		img->lockdown_data = NULL;
	}

	img->lockdown_data = fwu->data_buffer +
				FW_IMAGE_OFFSET -
				img->lockdown_block_count * fwu->block_size;

	fwu->lockdown_block_count = img->lockdown_block_count;
	fwu->lockdown_data = img->lockdown_data;
	fwu->config_data = img->config_data;
	fwu->firmware_data = img->firmware_data;
	return;
}

static int fwu_read_f01_device_status(struct f01_device_status *status)
{
	int retval;

	retval = fwu->fn_ptr->read(fwu->rmi4_data,
			fwu->f01_fd.data_base_addr,
			status->data,
			sizeof(status->data));
	if (retval < 0) {
		dev_err(&fwu->rmi4_data->i2c_client->dev,
				"%s: Failed to read F01 device status\n",
				__func__);
		return retval;
	}

	return 0;
}

static unsigned short fwu_get_address(enum flash_offset type)
{
	int offset;
	unsigned short addr = 0;
	struct i2c_client *i2c_client = fwu->rmi4_data->i2c_client;

	switch (type) {
	case OFFSET_BOOTLOADER_ID:
		offset = 0;
		addr = fwu->f34_fd.query_base_addr + offset;
		break;
	case OFFSET_FLASH_PROPERTIES:
		offset = ((fwu->f34_fd.version == 0) ? 2 : 1);
		addr = fwu->f34_fd.query_base_addr + offset;
		break;
	case OFFSET_BLOCK_SIZE:
		offset = ((fwu->f34_fd.version == 0) ? 3 : 2);
		addr = fwu->f34_fd.query_base_addr + offset;
		break;
	case OFFSET_FW_BLOCK_COUNT:
		offset = ((fwu->f34_fd.version == 0) ? 5 : 3);
		addr = fwu->f34_fd.query_base_addr + offset;
		break;
	case OFFSET_BLOCK_NUMBER:
		offset = 0;
		addr = fwu->f34_fd.data_base_addr + offset;
		break;
	case OFFSET_BLOCK_DATA:
		offset = ((fwu->f34_fd.version == 0) ? 2 : 1);
		addr = fwu->f34_fd.data_base_addr + offset;
		break;
	case OFFSET_FLASH_CONTROL:
		offset = ((fwu->f34_fd.version == 0) ?
			2 + (fwu->block_size) : 2);
		addr = fwu->f34_fd.data_base_addr + offset;
		break;
	case OFFSET_FLASH_STATUS:
		if (fwu->f34_fd.version == 1) {
			offset = 3;
			addr = fwu->f34_fd.data_base_addr + offset;
		} else if (fwu->f34_fd.version == 0) {
			dev_warn(&i2c_client->dev,
			"%s: F$34 version 0 does not contain " \
			"flash status register\n",
			__func__);
		}
		break;
	default:
		dev_err(&i2c_client->dev,
			"%s: Unknown flash offset (%d)\n",
			__func__, type);
		break;
	}
	return addr;
}

static int fwu_read_f34_queries(void)
{
	int retval;
	unsigned char count = 4;
	unsigned char buf[10];
	struct i2c_client *i2c_client = fwu->rmi4_data->i2c_client;

	retval = fwu->fn_ptr->read(fwu->rmi4_data,
			fwu_get_address(OFFSET_BOOTLOADER_ID),
			fwu->bootloader_id,
			sizeof(fwu->bootloader_id));
	if (retval < 0) {
		dev_err(&i2c_client->dev,
				"%s: Failed to read bootloader ID\n",
				__func__);
		return retval;
	}

	retval = fwu->fn_ptr->read(fwu->rmi4_data,
			fwu_get_address(OFFSET_FLASH_PROPERTIES),
			fwu->flash_properties.data,
			sizeof(fwu->flash_properties.data));
	if (retval < 0) {
		dev_err(&i2c_client->dev,
				"%s: Failed to read flash properties\n",
				__func__);
		return retval;
	}

	dev_info(&i2c_client->dev, "%s perm:%d, bl:%d, display:%d\n",
				__func__,
				fwu->flash_properties.has_perm_config,
				fwu->flash_properties.has_bl_config,
				fwu->flash_properties.has_display_config);

	if (fwu->flash_properties.has_perm_config)
		count += 2;

	if (fwu->flash_properties.has_bl_config)
		count += 2;

	if (fwu->flash_properties.has_display_config)
		count += 2;

	retval = fwu->fn_ptr->read(fwu->rmi4_data,
			fwu_get_address(OFFSET_BLOCK_SIZE),
			buf,
			2);
	if (retval < 0) {
		dev_err(&i2c_client->dev,
				"%s: Failed to read block size info\n",
				__func__);
		return retval;
	}

	batohs(&fwu->block_size, &(buf[0]));

	retval = fwu->fn_ptr->read(fwu->rmi4_data,
			fwu_get_address(OFFSET_FW_BLOCK_COUNT),
			buf,
			count);
	if (retval < 0) {
		dev_err(&i2c_client->dev,
			"%s: Failed to read block count info\n",
			__func__);
		return retval;
	}

	batohs(&fwu->fw_block_count, &(buf[0]));
	batohs(&fwu->config_block_count, &(buf[2]));

	count = 4;

	if (fwu->flash_properties.has_perm_config) {
		batohs(&fwu->perm_config_block_count, &(buf[count]));
		count += 2;
	}

	if (fwu->flash_properties.has_bl_config) {
		batohs(&fwu->bl_config_block_count, &(buf[count]));
		count += 2;
	}

	if (fwu->flash_properties.has_display_config)
		batohs(&fwu->disp_config_block_count, &(buf[count]));

	return 0;
}

static int fwu_read_interrupt_status(void)
{
	int retval;
	unsigned char interrupt_status;
	retval = fwu->fn_ptr->read(fwu->rmi4_data,
			fwu->addr_f01_interrupt_register,
			&interrupt_status,
			sizeof(interrupt_status));
	if (retval < 0) {
		dev_err(&fwu->rmi4_data->i2c_client->dev,
				"%s: Failed to read flash status\n",
				__func__);
		return retval;
	}
	return interrupt_status;
}

static int fwu_read_f34_flash_status(unsigned char *status)
{
	int retval;
	struct f34_flash_control flash_control;
	struct f34_flash_status flash_status;

	if (fwu->f34_fd.version == 1) {
		retval = fwu->fn_ptr->read(fwu->rmi4_data,
			fwu_get_address(OFFSET_FLASH_STATUS),
			flash_status.data,
			sizeof(flash_status.data));
		if (retval < 0) {
			dev_err(&fwu->rmi4_data->i2c_client->dev,
				"%s: Failed to read flash status\n",
				__func__);
			return -EIO;
		}
		*status = flash_status.status;
	} else {
		retval = fwu->fn_ptr->read(fwu->rmi4_data,
			fwu_get_address(OFFSET_FLASH_CONTROL),
			flash_control.data,
			sizeof(flash_control.data));
		if (retval < 0) {
			dev_err(&fwu->rmi4_data->i2c_client->dev,
				"%s: Failed to read flash status\n",
				__func__);
			return -EIO;
		}
		*status = flash_control.status;
	}
	return 0;
}

static int fwu_reset_device(void)
{
	int retval;

	dev_dbg(&fwu->rmi4_data->i2c_client->dev,
			"%s: Reset device\n",
			__func__);

	retval = fwu->rmi4_data->reset_device(fwu->rmi4_data);
	if (retval < 0) {
		dev_err(&fwu->rmi4_data->i2c_client->dev,
				"%s: Failed to reset core driver after reflash\n",
				__func__);
		return retval;
	}

	fwu->polling_mode = false;

	return 0;
}

static int fwu_write_f34_command(unsigned char cmd)
{
	int retval;
	struct f34_flash_control flash_control;

	flash_control.data[0] = cmd;
	fwu->interrupt_flag = false;
	retval = fwu->fn_ptr->write(fwu->rmi4_data,
			fwu_get_address(OFFSET_FLASH_CONTROL),
			flash_control.data,
			sizeof(flash_control.data));
	if (retval < 0) {
		dev_err(&fwu->rmi4_data->i2c_client->dev,
				"%s: Failed to write command 0x%02x\n",
				__func__, flash_control.data[0]);
		return retval;
	}
	return 0;
}

static int fwu_wait_for_idle(int timeout_ms)
{
	int count = 0;
	int timeout_count = ((timeout_ms * 1000) / SLEEP_TIME_US) + 1;
	do {
		if (fwu->interrupt_flag)
			return 0;
		if (fwu->polling_mode)
			if (fwu->intr_mask & fwu_read_interrupt_status())
				return 0;
		usleep_range(SLEEP_TIME_US, SLEEP_TIME_US + 1);
	} while (count++ < timeout_count);

	if (fwu->intr_mask & fwu_read_interrupt_status()) {
		fwu->polling_mode = true;
		dev_info(&fwu->rmi4_data->i2c_client->dev,
			"%s: Switch to polling mode\n",
			__func__);
		return 0;
	}

	dev_err(&fwu->rmi4_data->i2c_client->dev,
			"%s: Timed out waiting for idle status\n",
			__func__);

	return -ETIMEDOUT;
}

static enum flash_area fwu_go_nogo(void)
{
	int retval = 0;
	int index = 0;
	int deviceFirmwareID;
	int imageConfigID;
	int deviceConfigID;
	unsigned long imageFirmwareID;
	unsigned char firmware_id[4];
	unsigned char config_id[4];
	unsigned char pkg_id[4];
	char *strptr;
	char *imagePR = kzalloc(sizeof(MAX_FIRMWARE_ID_LEN), GFP_KERNEL);
	enum flash_area flash_area = NONE;
	struct i2c_client *i2c_client = fwu->rmi4_data->i2c_client;
	struct f01_device_status f01_device_status;
	struct image_content *img = &fwu->image_content;

	if (fwu->force_update) {
		flash_area = UI_FIRMWARE;
		goto exit;
	}

	if (img->is_contain_build_info) {
		/* if package id does not match, do not update firmware */
		fwu->fn_ptr->read(fwu->rmi4_data,
					fwu->f01_fd.query_base_addr + 17,
					pkg_id,
					sizeof(pkg_id));

		if (img->package_id != ((pkg_id[1] << 8) | pkg_id[0])) {
			flash_area = MISMATCH;
			goto exit;
		}
		if (img->package_revision_id !=
				((pkg_id[3] << 8) | pkg_id[2])) {
			flash_area = MISMATCH;
			goto exit;
		}
	}

	/* check firmware size */
	if (fwu->fw_block_count*fwu->block_size != img->image_size) {
		dev_err(&i2c_client->dev,
			"%s: firmware size of device (%d) != .img (%d)\n",
			__func__,
			fwu->config_block_count * fwu->block_size,
			img->image_size);
		flash_area = NONE;
		goto exit;
	}

	/* check config size */
	if (fwu->config_block_count*fwu->block_size != img->config_size) {
		dev_err(&i2c_client->dev,
			"%s: config size of device (%d) != .img (%d)\n",
			__func__,
			fwu->config_block_count * fwu->block_size,
			img->config_size);
		flash_area = NONE;
		goto exit;
	}

	retval = fwu_read_f01_device_status(&f01_device_status);
	if (retval < 0) {
		flash_area = NONE;
		goto exit;
	}

	/* Force update firmware when device is in bootloader mode */
	if (f01_device_status.flash_prog) {
		dev_info(&i2c_client->dev,
			"%s: In flash prog mode\n",
			__func__);
		flash_area = UI_FIRMWARE;
		goto exit;
	}

	/* device firmware id */
	retval = fwu->fn_ptr->read(fwu->rmi4_data,
				fwu->f01_fd.query_base_addr + 18,
				firmware_id,
				sizeof(firmware_id));
	if (retval < 0) {
		dev_err(&i2c_client->dev,
			"%s: Failed to read firmware ID (code %d).\n",
			__func__, retval);
		goto exit;
	}
	firmware_id[3] = 0;
	deviceFirmwareID = extract_uint(firmware_id);

	/* .img firmware id */
	if (img->is_contain_build_info) {
		dev_err(&i2c_client->dev,
			"%s: Image option contains build info.\n",
			__func__);
		imageFirmwareID = img->firmware_id;
	} else {
		if (!fwu->image_name) {
			dev_info(&i2c_client->dev,
				"%s: Unknown image file name\n",
				__func__);
			flash_area = UI_FIRMWARE;
			goto exit;
		}
		strptr = strnstr(fwu->image_name, "PR",
				sizeof(fwu->image_name));
		if (!strptr) {
			dev_err(&i2c_client->dev,
				"No valid PR number (PRxxxxxxx)" \
				"found in image file name...\n");
			goto exit;
		}

		strptr += 2;
		while (strptr[index] >= '0' && strptr[index] <= '9') {
			imagePR[index] = strptr[index];
			index++;
		}
		imagePR[index] = 0;

		retval = kstrtoul(imagePR, 10, &imageFirmwareID);
		if (retval ==  -EINVAL) {
			dev_err(&i2c_client->dev,
				"invalid image firmware id...\n");
			goto exit;
		}
	}

	dev_dbg(&i2c_client->dev,
			"Device firmware id %d, .img firmware id %d\n",
			deviceFirmwareID,
			(unsigned int)imageFirmwareID);
	if (imageFirmwareID > deviceFirmwareID) {
		flash_area = UI_FIRMWARE;
		goto exit;
	} else if (imageFirmwareID < deviceFirmwareID) {
		flash_area = NONE;
		dev_info(&i2c_client->dev,
			"%s: Img fw is older than device fw. Skip fw update.\n",
			__func__);
		goto exit;
	}

	/* device config id */
	retval = fwu->fn_ptr->read(fwu->rmi4_data,
				fwu->f34_fd.ctrl_base_addr,
				config_id,
				sizeof(config_id));
	if (retval < 0) {
		dev_err(&i2c_client->dev,
			"%s: Failed to read config ID (code %d).\n",
			__func__, retval);
		flash_area = NONE;
		goto exit;
	}
	deviceConfigID =  extract_uint_be(config_id);

	dev_dbg(&i2c_client->dev,
		"Device config ID 0x%02X, 0x%02X, 0x%02X, 0x%02X\n",
		config_id[0], config_id[1], config_id[2], config_id[3]);

	/* .img config id */
	dev_dbg(&i2c_client->dev,
			".img config ID 0x%02X, 0x%02X, 0x%02X, 0x%02X\n",
			fwu->config_data[0],
			fwu->config_data[1],
			fwu->config_data[2],
			fwu->config_data[3]);
	imageConfigID =  extract_uint_be(fwu->config_data);

	dev_dbg(&i2c_client->dev,
		"%s: Device config ID %d, .img config ID %d\n",
		__func__, deviceConfigID, imageConfigID);

	if (imageConfigID > deviceConfigID) {
		flash_area = CONFIG_AREA;
		goto exit;
	}
exit:
	kfree(imagePR);
	if (flash_area == MISMATCH)
		dev_info(&i2c_client->dev,
			"%s: Package ID indicates mismatch of firmware and" \
			" controller compatibility\n", __func__);
	else if (flash_area == NONE)
		dev_info(&i2c_client->dev,
			"%s: Nothing needs to be updated\n", __func__);
	else
		dev_info(&i2c_client->dev,
			"%s: Update %s block\n", __func__,
			flash_area == UI_FIRMWARE ? "UI FW" : "CONFIG");
	return flash_area;
}

static int fwu_scan_pdt(void)
{
	int retval;
	unsigned char ii;
	unsigned char intr_count = 0;
	unsigned char intr_off;
	unsigned char intr_src;
	unsigned short addr;
	bool f01found = false;
	bool f34found = false;
	struct synaptics_rmi4_fn_desc rmi_fd;

	dev_dbg(&fwu->rmi4_data->i2c_client->dev, "Scan PDT\n");

	for (addr = PDT_START; addr > PDT_END; addr -= PDT_ENTRY_SIZE) {
		retval = fwu->fn_ptr->read(fwu->rmi4_data,
					addr,
					(unsigned char *)&rmi_fd,
					sizeof(rmi_fd));
		if (retval < 0)
			return retval;

		if (rmi_fd.fn_number) {
			dev_dbg(&fwu->rmi4_data->i2c_client->dev,
					"%s: Found F%02x\n",
					__func__, rmi_fd.fn_number);
			switch (rmi_fd.fn_number) {
			case SYNAPTICS_RMI4_F01:
				f01found = true;
				fwu->f01_fd = rmi_fd;
				fwu->addr_f01_interrupt_register =
					fwu->f01_fd.data_base_addr + 1;
				break;
			case SYNAPTICS_RMI4_F34:
				f34found = true;
				fwu->f34_fd = rmi_fd;
				fwu->intr_mask = 0;
				intr_src = rmi_fd.intr_src_count;
				intr_off = intr_count % 8;
				for (ii = intr_off;
						ii < ((intr_src & MASK_3BIT) +
						intr_off);
						ii++)
					fwu->intr_mask |= 1 << ii;
				break;
			}
		} else
		break;

		intr_count += (rmi_fd.intr_src_count & MASK_3BIT);
	}

	if (!f01found || !f34found) {
		dev_err(&fwu->rmi4_data->i2c_client->dev,
				"%s: Failed to find both F01 and F34\n",
				__func__);
		return -EINVAL;
	}

	fwu_read_interrupt_status();
	return 0;
}

static int fwu_write_blocks(unsigned char *block_ptr, unsigned short block_cnt,
		unsigned char command)
{
	int retval;
	unsigned char flash_status;
	unsigned char block_offset[] = {0, 0};
	unsigned short block_num;
	unsigned short addr_block_data = fwu_get_address(OFFSET_BLOCK_DATA);
	unsigned short addr_block_num = fwu_get_address(OFFSET_BLOCK_NUMBER);
	struct i2c_client *i2c_client = fwu->rmi4_data->i2c_client;
#ifdef SHOW_PROGRESS
	unsigned int progress;
	unsigned char command_str[10];
	switch (command) {
	case CMD_WRITE_CONFIG_BLOCK:
		progress = 10;
		strlcpy(command_str, "config", 10);
		break;
	case CMD_WRITE_FW_BLOCK:
		progress = 100;
		strlcpy(command_str, "firmware", 10);
		break;
	case CMD_WRITE_LOCKDOWN_BLOCK:
		progress = 1;
		strlcpy(command_str, "lockdown", 10);
		break;
	default:
		progress = 1;
		strlcpy(command_str, "unknown", 10);
		break;
	}
#endif

	dev_dbg(&i2c_client->dev,
			"%s: Start to update %s blocks\n",
			__func__,
			command_str);
	retval = fwu->fn_ptr->write(fwu->rmi4_data,
			addr_block_num,
			block_offset,
			sizeof(block_offset));
	if (retval < 0) {
		dev_err(&i2c_client->dev,
				"%s: Failed to write to block number registers\n",
				__func__);
		return retval;
	}

	for (block_num = 0; block_num < block_cnt; block_num++) {
#ifdef SHOW_PROGRESS
		if (block_num % progress == 0)
			dev_info(&i2c_client->dev,
					"%s: update %s %3d / %3d\n",
					__func__,
					command_str,
					block_num, block_cnt);
#endif
		retval = fwu->fn_ptr->write(fwu->rmi4_data,
			addr_block_data,
			block_ptr,
			fwu->block_size);
		if (retval < 0) {
			dev_err(&i2c_client->dev,
				"%s: Failed to write block data (block %d)\n",
				__func__, block_num);
			return retval;
		}

		retval = fwu_write_f34_command(command);
		if (retval < 0) {
			dev_err(&i2c_client->dev,
					"%s: Failed to write command for block %d\n",
					__func__, block_num);
			return retval;
		}

		retval = fwu_wait_for_idle(WRITE_WAIT_MS);
		if (retval < 0) {
			dev_err(&i2c_client->dev,
					"%s: Failed to wait for idle status (block %d)\n",
					__func__, block_num);
			return retval;
		}

		#if CHECK_FLASH_BLOCK_STATUS
		retval = fwu_read_f34_flash_status(&flash_status);
		if (retval < 0) {
			dev_err(&i2c_client->dev,
					"%s: Failed to read flash status (block %d)\n",
					__func__, block_num);
			return retval;
		}
		if (flash_status != 0x00) {
			dev_err(&i2c_client->dev,
				"%s: Flash block %d failed, status 0x%02X\n",
				__func__, block_num, flash_status);
			return -EINVAL;
		}
		#endif
		block_ptr += fwu->block_size;
	}
#ifdef SHOW_PROGRESS
	dev_info(&i2c_client->dev,
			"%s: update %s %3d / %3d\n",
			__func__,
			command_str,
			block_cnt, block_cnt);
#endif
	return 0;
}

static int fwu_write_firmware(void)
{
	return fwu_write_blocks((unsigned char *)fwu->firmware_data,
		fwu->fw_block_count, CMD_WRITE_FW_BLOCK);
}

static int fwu_write_configuration(void)
{
	return fwu_write_blocks((unsigned char *)fwu->config_data,
		fwu->config_block_count, CMD_WRITE_CONFIG_BLOCK);
}

static int fwu_write_lockdown_block(void)
{
	return fwu_write_blocks((unsigned char *)fwu->lockdown_data,
		fwu->lockdown_block_count, CMD_WRITE_LOCKDOWN_BLOCK);
}

static int fwu_write_bootloader_id(void)
{
	int retval;

	dev_dbg(&fwu->rmi4_data->i2c_client->dev,
			"Write bootloader ID 0x%02X 0x%02X\n",
			fwu->bootloader_id[0],
			fwu->bootloader_id[1]);

	retval = fwu->fn_ptr->write(fwu->rmi4_data,
			fwu_get_address(OFFSET_BLOCK_DATA),
			fwu->bootloader_id,
			sizeof(fwu->bootloader_id));
	if (retval < 0) {
		dev_err(&fwu->rmi4_data->i2c_client->dev,
				"%s: Failed to write bootloader ID\n",
				__func__);
		return retval;
	}

	return 0;
}

static int fwu_enter_flash_prog(bool force)
{
	int retval;
	struct f01_device_status f01_device_status;
	struct f01_device_control f01_device_control;

	dev_dbg(&fwu->rmi4_data->i2c_client->dev, "Enter bootloader mode\n");

	retval = fwu_read_f01_device_status(&f01_device_status);
	if (retval < 0)
		return retval;

	if (force) {
		dev_info(&fwu->rmi4_data->i2c_client->dev,
			"%s: Force to enter flash prog mode\n",
			__func__);
	} else if (f01_device_status.flash_prog) {
		dev_info(&fwu->rmi4_data->i2c_client->dev,
				"%s: Already in flash prog mode\n",
				__func__);
		return 0;
	}

	retval = fwu_write_bootloader_id();
	if (retval < 0)
		return retval;

	retval = fwu_write_f34_command(CMD_ENABLE_FLASH_PROG);
	if (retval < 0)
		return retval;

	retval = fwu_wait_for_idle(ENABLE_WAIT_MS);
	if (retval < 0)
		return retval;

	retval = fwu_scan_pdt();
	if (retval < 0)
		return retval;

	retval = fwu_read_f01_device_status(&f01_device_status);
	if (retval < 0)
		return retval;

	if (!f01_device_status.flash_prog) {
		dev_err(&fwu->rmi4_data->i2c_client->dev,
				"%s: Not in flash prog mode\n",
				__func__);
		return -EINVAL;
	}

	retval = fwu_read_f34_queries();
	if (retval < 0)
		return retval;

	retval = fwu->fn_ptr->read(fwu->rmi4_data,
			fwu->f01_fd.ctrl_base_addr,
			f01_device_control.data,
			sizeof(f01_device_control.data));
	if (retval < 0) {
		dev_err(&fwu->rmi4_data->i2c_client->dev,
				"%s: Failed to read F01 device control\n",
				__func__);
		return retval;
	}

	f01_device_control.nosleep = true;
	f01_device_control.sleep_mode = SLEEP_MODE_NORMAL;

	retval = fwu->fn_ptr->write(fwu->rmi4_data,
			fwu->f01_fd.ctrl_base_addr,
			f01_device_control.data,
			sizeof(f01_device_control.data));
	if (retval < 0) {
		dev_err(&fwu->rmi4_data->i2c_client->dev,
				"%s: Failed to write F01 device control\n",
				__func__);
		return retval;
	}
	fwu->polling_mode = false;
	return retval;
}

static int fwu_do_write_config(void)
{
	int retval;

	retval = fwu_enter_flash_prog(false);
	if (retval < 0)
		return retval;

	dev_dbg(&fwu->rmi4_data->i2c_client->dev,
			"%s: Entered flash prog mode\n",
			__func__);

	if (fwu->config_area == PERM_CONFIG_AREA) {
		fwu->config_block_count = fwu->perm_config_block_count;
		goto write_config;
	}

	retval = fwu_write_bootloader_id();
	if (retval < 0)
		return retval;

	dev_dbg(&fwu->rmi4_data->i2c_client->dev,
			"%s: Bootloader ID written\n",
			__func__);

	switch (fwu->config_area) {
	case UI_CONFIG_AREA:
		retval = fwu_write_f34_command(CMD_ERASE_CONFIG);
		break;
	case BL_CONFIG_AREA:
		retval = fwu_write_f34_command(CMD_ERASE_BL_CONFIG);
		fwu->config_block_count = fwu->bl_config_block_count;
		break;
	case DISP_CONFIG_AREA:
		retval = fwu_write_f34_command(CMD_ERASE_DISP_CONFIG);
		fwu->config_block_count = fwu->disp_config_block_count;
		break;
	}
	if (retval < 0)
		return retval;

	dev_dbg(&fwu->rmi4_data->i2c_client->dev,
			"%s: Erase command written\n",
			__func__);

	retval = fwu_wait_for_idle(ERASE_WAIT_MS);
	if (retval < 0)
		return retval;

	dev_dbg(&fwu->rmi4_data->i2c_client->dev,
			"%s: Idle status detected\n",
			__func__);

write_config:
	retval = fwu_write_configuration();
	if (retval < 0)
		return retval;

	pr_notice("%s: Config written\n", __func__);

	return retval;
}

static int fwu_start_write_config(void)
{
	int retval;
	int block_count;

	switch (fwu->config_area) {
	case UI_CONFIG_AREA:
		block_count = fwu->config_block_count;
		break;
	case PERM_CONFIG_AREA:
		if (!fwu->flash_properties.has_perm_config)
			return -EINVAL;
		block_count = fwu->perm_config_block_count;
		break;
	case BL_CONFIG_AREA:
		if (!fwu->flash_properties.has_bl_config)
			return -EINVAL;
		block_count = fwu->bl_config_block_count;
		break;
	case DISP_CONFIG_AREA:
		if (!fwu->flash_properties.has_display_config)
			return -EINVAL;
		block_count = fwu->disp_config_block_count;
		break;
	default:
		return -EINVAL;
	}

	if (fwu->image_size == block_count*fwu->block_size) {
		dev_info(&fwu->rmi4_data->i2c_client->dev,
				"%s: write config from config file\n",
				__func__);
		fwu->config_data = fwu->data_buffer;
	} else {
		parse_header();
	}

	pr_notice("%s: Start of write config process\n", __func__);

	retval = fwu_do_write_config();
	if (retval < 0) {
		dev_err(&fwu->rmi4_data->i2c_client->dev,
				"%s: Failed to write config\n",
				__func__);
	}

	fwu->rmi4_data->reset_device(fwu->rmi4_data);

	pr_notice("%s: End of write config process\n", __func__);

	return retval;
}

static int fwu_do_write_lockdown(bool reset)
{
	int retval;

	pr_notice("%s: Start of lockdown process\n", __func__);

	retval = fwu_enter_flash_prog(false);
	if (retval < 0)
		return retval;

	dev_dbg(&fwu->rmi4_data->i2c_client->dev,
			"%s: Entered flash prog mode\n",
			__func__);

	if (fwu->flash_properties.unlocked == 0) {
		dev_err(&fwu->rmi4_data->i2c_client->dev,
			"%s: Device has been locked!\n",
			__func__);
		if (reset)
			goto exit;
		else
			return -EINVAL;
	}

	retval = fwu_write_lockdown_block();
	if (retval < 0)
		return retval;

	dev_dbg(&fwu->rmi4_data->i2c_client->dev,
			"%s:Lockdown device\n",
			__func__);

exit:
	if (reset)
		retval = fwu->rmi4_data->reset_device(fwu->rmi4_data);
	else
		retval = fwu_enter_flash_prog(true);

	if (retval < 0)
		return retval;

	pr_notice("%s: End of lockdown process\n", __func__);

	return retval;
}

static int fwu_start_write_lockdown(void)
{
	parse_header();
	return fwu_do_write_lockdown(true);
}

static int fwu_do_read_config(void)
{
	int retval;
	unsigned char block_offset[] = {0, 0};
	unsigned short block_num;
	unsigned short block_count;
	unsigned short index = 0;

	switch (fwu->config_area) {
	case UI_CONFIG_AREA:
		block_count = fwu->config_block_count;
		break;
	case PERM_CONFIG_AREA:
		if (!fwu->flash_properties.has_perm_config) {
			retval = -EINVAL;
			goto exit;
		}
		block_count = fwu->perm_config_block_count;
		break;
	case BL_CONFIG_AREA:
		if (!fwu->flash_properties.has_bl_config) {
			retval = -EINVAL;
			goto exit;
		}
		block_count = fwu->bl_config_block_count;
		break;
	case DISP_CONFIG_AREA:
		if (!fwu->flash_properties.has_display_config) {
			retval = -EINVAL;
			goto exit;
		}
		block_count = fwu->disp_config_block_count;
		break;
	default:
		retval = -EINVAL;
		goto exit;
	}

	fwu->config_size = fwu->block_size * block_count;

	kfree(fwu->read_config_buf);
	fwu->read_config_buf = kzalloc(fwu->config_size, GFP_KERNEL);

	block_offset[1] |= (fwu->config_area << 5);

	retval = fwu->fn_ptr->write(fwu->rmi4_data,
			fwu_get_address(OFFSET_BLOCK_NUMBER),
			block_offset,
			sizeof(block_offset));
	if (retval < 0) {
		dev_err(&fwu->rmi4_data->i2c_client->dev,
				"%s: Failed to write to block number registers\n",
				__func__);
		goto exit;
	}

	for (block_num = 0; block_num < block_count; block_num++) {
		retval = fwu_write_f34_command(CMD_READ_CONFIG_BLOCK);
		if (retval < 0) {
			dev_err(&fwu->rmi4_data->i2c_client->dev,
					"%s: Failed to write read config command\n",
					__func__);
			goto exit;
		}

		retval = fwu_wait_for_idle(WRITE_WAIT_MS);
		if (retval < 0) {
			dev_err(&fwu->rmi4_data->i2c_client->dev,
					"%s: Failed to wait for idle status\n",
					__func__);
			goto exit;
		}

		retval = fwu->fn_ptr->read(fwu->rmi4_data,
				fwu_get_address(OFFSET_BLOCK_DATA),
				&fwu->read_config_buf[index],
				fwu->block_size);
		if (retval < 0) {
			dev_err(&fwu->rmi4_data->i2c_client->dev,
					"%s: Failed to read block data (block %d)\n",
					__func__, block_num);
			goto exit;
		}

		index += fwu->block_size;
	}

exit:
	return retval;
}

static int fwu_do_reflash(void)
{
	int retval;
	unsigned char flash_status;

	if (fwu->do_lockdown) {
		retval = fwu_do_write_lockdown(false);
		if (retval < 0)
			dev_warn(&fwu->rmi4_data->i2c_client->dev,
			"%s: Skip lockdown process.\n",
			__func__);
	}
	retval = fwu_enter_flash_prog(false);
	if (retval < 0)
		return retval;
	dev_dbg(&fwu->rmi4_data->i2c_client->dev,
			"%s: Entered flash prog mode\n",
			__func__);

	retval = fwu_write_bootloader_id();
	if (retval < 0)
		return retval;

	dev_dbg(&fwu->rmi4_data->i2c_client->dev,
			"%s: Bootloader ID written\n",
			__func__);

	retval = fwu_write_f34_command(CMD_ERASE_ALL);
	if (retval < 0)
		return retval;

	dev_dbg(&fwu->rmi4_data->i2c_client->dev,
			"%s: Erase all command written\n",
			__func__);

	if (fwu->polling_mode)
		msleep(100);

	retval = fwu_wait_for_idle(ERASE_WAIT_MS);
	if (retval < 0)
		return retval;

	retval = fwu_read_f34_flash_status(&flash_status);
	if (retval < 0)
		return retval;
	if (flash_status != 0x00) {
		dev_err(&fwu->rmi4_data->i2c_client->dev,
				"%s: Erase all command failed, status 0x%02X\n",
				__func__, flash_status);
		return -EINVAL;
	}

	if (fwu->firmware_data) {
		retval = fwu_write_firmware();
		if (retval < 0)
			return retval;
		pr_notice("%s: Firmware programmed\n", __func__);
	}

	if (fwu->config_data) {
		retval = fwu_write_configuration();
		if (retval < 0)
			return retval;
		pr_notice("%s: Configuration programmed\n", __func__);
	}

	return retval;
}

static int fwu_start_reflash(void)
{
	int retval = 0;
	const struct firmware *fw_entry = NULL;
	struct f01_device_status f01_device_status;
	enum flash_area flash_area;

	pr_notice("%s: Start of reflash process\n", __func__);

	if (fwu->ext_data_source)
		dev_info(&fwu->rmi4_data->i2c_client->dev,
				"%s Load .img file from commandline.\n",
				__func__);
	else {
		if (strnlen(fwu->rmi4_data->fw_image_name,
				NAME_BUFFER_SIZE) == 0) {
			dev_err(&fwu->rmi4_data->i2c_client->dev,
				"Firmware image name not given, "\
				"skipping update\n");
			return 0;
		}

		if (strnlen(fwu->rmi4_data->fw_image_name, NAME_BUFFER_SIZE) ==
			NAME_BUFFER_SIZE) {
			dev_err(&fwu->rmi4_data->i2c_client->dev,
				"Firmware image name exceeds max length " \
				"(%d), skipping update\n", NAME_BUFFER_SIZE);
			return 0;
		}

		snprintf(fwu->image_name, NAME_BUFFER_SIZE, "%s",
			fwu->rmi4_data->fw_image_name);
		dev_info(&fwu->rmi4_data->i2c_client->dev,
			"%s: Requesting firmware image %s\n",
			__func__, fwu->image_name);

		retval = request_firmware(&fw_entry,
				fwu->image_name,
				&fwu->rmi4_data->i2c_client->dev);
		if (retval != 0) {
			dev_err(&fwu->rmi4_data->i2c_client->dev,
					"%s: Firmware image %s not available\n",
					__func__,
					fwu->image_name);
			return -EINVAL;
		}

		dev_dbg(&fwu->rmi4_data->i2c_client->dev,
				"%s: Firmware image size = %zu\n",
				__func__, fw_entry->size);

		fwu->data_buffer = fw_entry->data;
	}

	parse_header();
	flash_area = fwu_go_nogo();

	if (fwu->rmi4_data->sensor_sleep) {
		dev_err(&fwu->rmi4_data->i2c_client->dev,
			"%s: Sensor sleeping\n",
			__func__);
		retval = -ENODEV;
		goto exit;
	}
	fwu->rmi4_data->stay_awake = true;

	switch (flash_area) {
	case NONE:
	case MISMATCH:
		retval = 0;
		dev_info(&fwu->rmi4_data->i2c_client->dev,
		"%s: No need to do reflash.\n",
		__func__);
		goto exit;
	case UI_FIRMWARE:
		retval = fwu_do_reflash();
		break;
	case CONFIG_AREA:
		retval = fwu_do_write_config();
		break;
	default:
		dev_err(&fwu->rmi4_data->i2c_client->dev,
				"%s: Unknown flash area\n",
				__func__);
		retval = -EINVAL;
		goto exit;
	}

	if (retval < 0)
		dev_err(&fwu->rmi4_data->i2c_client->dev,
				"%s: Failed to do reflash\n",
				__func__);

	/* reset device */
	fwu_reset_device();

	/* check device status */
	retval = fwu_read_f01_device_status(&f01_device_status);
	if (retval < 0)
		goto exit;

	dev_info(&fwu->rmi4_data->i2c_client->dev, "Device is in %s mode\n",
		f01_device_status.flash_prog == 1 ? "bootloader" : "UI");
	if (f01_device_status.flash_prog)
		dev_info(&fwu->rmi4_data->i2c_client->dev, "Flash status %d\n",
				f01_device_status.status_code);

	if (f01_device_status.flash_prog) {
		dev_info(&fwu->rmi4_data->i2c_client->dev,
				"%s: Device is in flash prog mode 0x%02X\n",
				__func__, f01_device_status.status_code);
	}

exit:
	if (fw_entry)
		release_firmware(fw_entry);

	pr_notice("%s: End of reflash process\n", __func__);
	fwu->rmi4_data->stay_awake = false;
	return retval;
}

int synaptics_fw_updater(void)
{
	int retval;

	if (!fwu)
		return -ENODEV;

	if (!fwu->initialized)
		return -ENODEV;

	fwu->rmi4_data->fw_updating = true;
	if (fwu->rmi4_data->suspended == true) {
		fwu->rmi4_data->fw_updating = false;
		dev_err(&fwu->rmi4_data->i2c_client->dev,
			"Cannot start fw upgrade while device is in suspend\n");
		return -EBUSY;
	}

	fwu->config_area = UI_CONFIG_AREA;

	retval = fwu_start_reflash();
	fwu->rmi4_data->fw_updating = false;

	synaptics_rmi4_update_debug_info();

	return retval;
}
EXPORT_SYMBOL(synaptics_fw_updater);

static ssize_t fwu_sysfs_show_image(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count)
{
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	if (count < fwu->config_size) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Not enough space (%zu bytes) in buffer\n",
				__func__, count);
		return -EINVAL;
	}

	memcpy(buf, fwu->read_config_buf, fwu->config_size);

	return fwu->config_size;
}

static ssize_t fwu_sysfs_store_image(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count)
{
	memcpy((void *)(&fwu->ext_data_source[fwu->data_pos]),
			(const void *)buf,
			count);

	fwu->data_buffer = fwu->ext_data_source;
	fwu->data_pos += count;

	return count;
}

static ssize_t fwu_sysfs_image_name_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;
	char *strptr;

	if (count >= NAME_BUFFER_SIZE) {
		dev_err(&rmi4_data->i2c_client->dev,
			"Input over %d characters long\n", NAME_BUFFER_SIZE);
		return -EINVAL;
	}

	strptr = strnstr(buf, ".img",
			count);
	if (!strptr) {
		dev_err(&rmi4_data->i2c_client->dev,
			"Input is not valid .img file\n");
		return -EINVAL;
	}

	strlcpy(rmi4_data->fw_image_name, buf, count);
	return count;
}

static ssize_t fwu_sysfs_image_name_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	if (strnlen(fwu->rmi4_data->fw_image_name, NAME_BUFFER_SIZE) > 0)
		return snprintf(buf, PAGE_SIZE, "%s\n",
			fwu->rmi4_data->fw_image_name);
	else
		return snprintf(buf, PAGE_SIZE, "No firmware name given\n");
}

static ssize_t fwu_sysfs_force_reflash_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned int input;
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	if (sscanf(buf, "%u", &input) != 1) {
		retval = -EINVAL;
		goto exit;
	}

	if (input != 1) {
		retval = -EINVAL;
		goto exit;
	}
	if (LOCKDOWN)
		fwu->do_lockdown = true;

	fwu->force_update = true;
	retval = synaptics_fw_updater();
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to do reflash\n",
				__func__);
		goto exit;
	}

	retval = count;

exit:
	kfree(fwu->ext_data_source);
	fwu->ext_data_source = NULL;
	fwu->force_update = FORCE_UPDATE;
	fwu->do_lockdown = rmi4_data->board->do_lockdown;
	return retval;
}

static ssize_t fwu_sysfs_do_reflash_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned int input;
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	if (sscanf(buf, "%u", &input) != 1) {
		retval = -EINVAL;
		goto exit;
	}

	if (input & LOCKDOWN) {
		fwu->do_lockdown = true;
		input &= ~LOCKDOWN;
	}

	if ((input != NORMAL) && (input != FORCE)) {
		retval = -EINVAL;
		goto exit;
	}

	if (input == FORCE)
		fwu->force_update = true;

	retval = synaptics_fw_updater();
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to do reflash\n",
				__func__);
		goto exit;
	}

	retval = count;

exit:
	kfree(fwu->ext_data_source);
	fwu->ext_data_source = NULL;
	fwu->force_update = FORCE_UPDATE;
	fwu->do_lockdown = rmi4_data->board->do_lockdown;
	return retval;
}

static ssize_t fwu_sysfs_write_lockdown_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned int input;
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	if (sscanf(buf, "%u", &input) != 1) {
		retval = -EINVAL;
		goto exit;
	}

	if (input != 1) {
		retval = -EINVAL;
		goto exit;
	}

	retval = fwu_start_write_lockdown();
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to write lockdown block\n",
				__func__);
		goto exit;
	}

	retval = count;

exit:
	kfree(fwu->ext_data_source);
	fwu->ext_data_source = NULL;
	fwu->force_update = FORCE_UPDATE;
	fwu->do_lockdown = rmi4_data->board->do_lockdown;
	return retval;
}

static ssize_t fwu_sysfs_write_config_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned int input;
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	if (sscanf(buf, "%u", &input) != 1) {
		retval = -EINVAL;
		goto exit;
	}

	if (input != 1) {
		retval = -EINVAL;
		goto exit;
	}

	retval = fwu_start_write_config();
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to write config\n",
				__func__);
		goto exit;
	}

	retval = count;

exit:
	kfree(fwu->ext_data_source);
	fwu->ext_data_source = NULL;
	return retval;
}

static ssize_t fwu_sysfs_read_config_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned int input;
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	if (input != 1)
		return -EINVAL;

	retval = fwu_do_read_config();
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to read config\n",
				__func__);
		return retval;
	}

	return count;
}

static ssize_t fwu_sysfs_config_area_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned short config_area;
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	retval = kstrtou16(buf, 10, &config_area);
	if (retval)
		return retval;

	if (config_area < 0x00 || config_area > 0x03) {
		dev_err(&rmi4_data->i2c_client->dev,
			"%s: Incorrect value of config_area\n",
			 __func__);
		return -EINVAL;
	}

	fwu->config_area = config_area;

	return count;
}

static ssize_t fwu_sysfs_image_size_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned long size;
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	retval = kstrtoul(buf, 10, &size);
	if (retval)
		return retval;

	fwu->image_size = size;
	fwu->data_pos = 0;

	kfree(fwu->ext_data_source);
	fwu->ext_data_source = kzalloc(fwu->image_size, GFP_KERNEL);
	if (!fwu->ext_data_source) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to alloc mem for image data\n",
				__func__);
		return -ENOMEM;
	}

	return count;
}

static ssize_t fwu_sysfs_block_size_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", fwu->block_size);
}

static ssize_t fwu_sysfs_firmware_block_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", fwu->fw_block_count);
}

static ssize_t fwu_sysfs_configuration_block_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", fwu->config_block_count);
}

static ssize_t fwu_sysfs_perm_config_block_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", fwu->perm_config_block_count);
}

static ssize_t fwu_sysfs_bl_config_block_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", fwu->bl_config_block_count);
}

static ssize_t fwu_sysfs_disp_config_block_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", fwu->disp_config_block_count);
}

static ssize_t fwu_sysfs_config_id_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char config_id[4];
	/* device config id */
	fwu->fn_ptr->read(fwu->rmi4_data,
				fwu->f34_fd.ctrl_base_addr,
				config_id,
				sizeof(config_id));

	return snprintf(buf, PAGE_SIZE, "%d.%d.%d.%d\n",
		config_id[0], config_id[1], config_id[2], config_id[3]);
}

static ssize_t fwu_sysfs_package_id_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char pkg_id[4];
	/* read device package id */
	fwu->fn_ptr->read(fwu->rmi4_data,
				fwu->f01_fd.query_base_addr + 17,
				pkg_id,
				sizeof(pkg_id));

	return snprintf(buf, PAGE_SIZE, "%d rev %d\n",
		(pkg_id[1] << 8) | pkg_id[0],
		(pkg_id[3] << 8) | pkg_id[2]);
}

static int synaptics_rmi4_debug_dump_info(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", fwu->ts_info);

	return 0;
}

static int debugfs_dump_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, synaptics_rmi4_debug_dump_info,
			inode->i_private);
}

static const struct file_operations debug_dump_info_fops = {
	.owner		= THIS_MODULE,
	.open		= debugfs_dump_info_open,
	.read		= seq_read,
	.release	= single_release,
};

static void synaptics_rmi4_fwu_attn(struct synaptics_rmi4_data *rmi4_data,
		unsigned char intr_mask)
{
	if (!fwu)
		return;

	if (fwu->intr_mask & intr_mask)
		fwu->interrupt_flag = true;

	return;
}

static struct bin_attribute dev_attr_data = {
	.attr = {
		.name = "data",
		.mode = (S_IRUGO | S_IWUSR | S_IWGRP),
	},
	.size = 0,
	.read = fwu_sysfs_show_image,
	.write = fwu_sysfs_store_image,
};

static struct device_attribute attrs[] = {
	__ATTR(fw_name, S_IRUGO | S_IWUSR | S_IWGRP,
			fwu_sysfs_image_name_show,
			fwu_sysfs_image_name_store),
	__ATTR(force_update_fw, S_IWUSR | S_IWGRP,
			NULL,
			fwu_sysfs_force_reflash_store),
	__ATTR(update_fw, S_IWUSR | S_IWGRP,
			NULL,
			fwu_sysfs_do_reflash_store),
	__ATTR(writeconfig, S_IWUSR | S_IWGRP,
			NULL,
			fwu_sysfs_write_config_store),
	__ATTR(writelockdown, S_IWUSR | S_IWGRP,
			NULL,
			fwu_sysfs_write_lockdown_store),
	__ATTR(readconfig, S_IWUSR | S_IWGRP,
			NULL,
			fwu_sysfs_read_config_store),
	__ATTR(configarea, S_IWUSR | S_IWGRP,
			NULL,
			fwu_sysfs_config_area_store),
	__ATTR(imagesize, S_IWUSR | S_IWGRP,
			NULL,
			fwu_sysfs_image_size_store),
	__ATTR(blocksize, S_IRUGO,
			fwu_sysfs_block_size_show,
			synaptics_rmi4_store_error),
	__ATTR(fwblockcount, S_IRUGO,
			fwu_sysfs_firmware_block_count_show,
			synaptics_rmi4_store_error),
	__ATTR(configblockcount, S_IRUGO,
			fwu_sysfs_configuration_block_count_show,
			synaptics_rmi4_store_error),
	__ATTR(permconfigblockcount, S_IRUGO,
			fwu_sysfs_perm_config_block_count_show,
			synaptics_rmi4_store_error),
	__ATTR(blconfigblockcount, S_IRUGO,
			fwu_sysfs_bl_config_block_count_show,
			synaptics_rmi4_store_error),
	__ATTR(dispconfigblockcount, S_IRUGO,
			fwu_sysfs_disp_config_block_count_show,
			synaptics_rmi4_store_error),
	__ATTR(config_id, S_IRUGO,
			fwu_sysfs_config_id_show,
			synaptics_rmi4_store_error),
	__ATTR(package_id, S_IRUGO,
			fwu_sysfs_package_id_show,
			synaptics_rmi4_store_error),
};


static void synaptics_rmi4_fwu_work(struct work_struct *work)
{
	fwu_start_reflash();
}

static int synaptics_rmi4_fwu_init(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char attr_count;
	struct pdt_properties pdt_props;
	struct dentry *temp;

	fwu = kzalloc(sizeof(*fwu), GFP_KERNEL);
	if (!fwu) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to alloc mem for fwu\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}

	fwu->fn_ptr = kzalloc(sizeof(*(fwu->fn_ptr)), GFP_KERNEL);
	if (!fwu->fn_ptr) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to alloc mem for fn_ptr\n",
				__func__);
		retval = -ENOMEM;
		goto exit_free_fwu;
	}

	fwu->rmi4_data = rmi4_data;
	fwu->fn_ptr->read = rmi4_data->i2c_read;
	fwu->fn_ptr->write = rmi4_data->i2c_write;
	fwu->fn_ptr->enable = rmi4_data->irq_enable;

	retval = fwu->fn_ptr->read(rmi4_data,
			PDT_PROPS,
			pdt_props.data,
			sizeof(pdt_props.data));
	if (retval < 0) {
		dev_dbg(&rmi4_data->i2c_client->dev,
				"%s: Failed to read PDT properties, assuming 0x00\n",
				__func__);
		goto exit_free_mem;
	} else if (pdt_props.has_bsr) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Reflash for LTS not currently supported\n",
				__func__);
		retval = -EINVAL;
		goto exit_free_mem;
	}

	retval = fwu_scan_pdt();
	if (retval < 0)
		goto exit_free_mem;

	fwu->productinfo1 = rmi4_data->rmi4_mod_info.product_info[0];
	fwu->productinfo2 = rmi4_data->rmi4_mod_info.product_info[1];

	memcpy(fwu->product_id, rmi4_data->rmi4_mod_info.product_id_string,
			SYNAPTICS_RMI4_PRODUCT_ID_SIZE);
	fwu->product_id[SYNAPTICS_RMI4_PRODUCT_ID_SIZE] = 0;

	dev_dbg(&rmi4_data->i2c_client->dev,
			"%s: F01 product info: 0x%04x 0x%04x\n",
			__func__, fwu->productinfo1, fwu->productinfo2);
	dev_dbg(&rmi4_data->i2c_client->dev,
			"%s: F01 product ID: %s\n",
			__func__, fwu->product_id);

	retval = fwu_read_f34_queries();
	if (retval < 0)
		goto exit_free_mem;

	fwu->initialized = true;
	fwu->force_update = FORCE_UPDATE;
	fwu->do_lockdown = rmi4_data->board->do_lockdown;
	fwu->initialized = true;
	fwu->polling_mode = false;

	retval = sysfs_create_bin_file(&rmi4_data->i2c_client->dev.kobj,
			&dev_attr_data);
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to create sysfs bin file\n",
				__func__);
		goto exit_free_mem;
	}

	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++) {
		retval = sysfs_create_file(&rmi4_data->i2c_client->dev.kobj,
				&attrs[attr_count].attr);
		if (retval < 0) {
			dev_err(&rmi4_data->i2c_client->dev,
					"%s: Failed to create sysfs attributes\n",
					__func__);
			retval = -ENODEV;
			goto exit_remove_attrs;
		}
	}

	temp = debugfs_create_file("dump_info", S_IRUSR | S_IWUSR,
			fwu->rmi4_data->dir, fwu->rmi4_data,
			&debug_dump_info_fops);
	if (temp == NULL || IS_ERR(temp)) {
		dev_err(&rmi4_data->i2c_client->dev,
			"%s: Failed to create debugfs dump info file\n",
			__func__);
		retval = PTR_ERR(temp);
		goto exit_remove_attrs;
	}

	fwu->ts_info = kzalloc(RMI4_INFO_MAX_LEN, GFP_KERNEL);
	if (!fwu->ts_info) {
		dev_err(&rmi4_data->i2c_client->dev, "Not enough memory\n");
		goto exit_free_ts_info;
	}

	synaptics_rmi4_update_debug_info();

#ifdef INSIDE_FIRMWARE_UPDATE
	fwu->fwu_workqueue = create_singlethread_workqueue("fwu_workqueue");
	INIT_DELAYED_WORK(&fwu->fwu_work, synaptics_rmi4_fwu_work);
	queue_delayed_work(fwu->fwu_workqueue,
			&fwu->fwu_work,
			msecs_to_jiffies(1000));
#endif

	return 0;
exit_free_ts_info:
	debugfs_remove(temp);
exit_remove_attrs:
	for (attr_count--; attr_count >= 0; attr_count--) {
		sysfs_remove_file(&rmi4_data->input_dev->dev.kobj,
				&attrs[attr_count].attr);
	}

	sysfs_remove_bin_file(&rmi4_data->input_dev->dev.kobj, &dev_attr_data);

exit_free_mem:
	kfree(fwu->fn_ptr);

exit_free_fwu:
	kfree(fwu);
	fwu = NULL;

exit:
	return retval;
}

static void synaptics_rmi4_fwu_remove(struct synaptics_rmi4_data *rmi4_data)
{
	unsigned char attr_count;

	sysfs_remove_bin_file(&rmi4_data->input_dev->dev.kobj, &dev_attr_data);

	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++) {
		sysfs_remove_file(&rmi4_data->input_dev->dev.kobj,
				&attrs[attr_count].attr);
	}

	kfree(fwu->read_config_buf);
	kfree(fwu->fn_ptr);
	kfree(fwu);

	complete(&fwu_remove_complete);

	return;
}

static int __init rmi4_fw_update_module_init(void)
{
	synaptics_rmi4_new_function(RMI_FW_UPDATER, true,
			synaptics_rmi4_fwu_init,
			synaptics_rmi4_fwu_remove,
			synaptics_rmi4_fwu_attn);
	return 0;
}

static void __exit rmi4_fw_update_module_exit(void)
{
	synaptics_rmi4_new_function(RMI_FW_UPDATER, false,
			synaptics_rmi4_fwu_init,
			synaptics_rmi4_fwu_remove,
			synaptics_rmi4_fwu_attn);
	wait_for_completion(&fwu_remove_complete);
	return;
}

module_init(rmi4_fw_update_module_init);
module_exit(rmi4_fw_update_module_exit);

MODULE_AUTHOR("Synaptics, Inc.");
MODULE_DESCRIPTION("RMI4 FW Update Module");
MODULE_LICENSE("GPL v2");
