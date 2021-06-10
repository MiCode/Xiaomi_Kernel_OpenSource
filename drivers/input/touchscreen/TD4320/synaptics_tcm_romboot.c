/*
 * Synaptics TCM touchscreen driver
 *
 * Copyright (C) 2017-2018 Synaptics Incorporated. All rights reserved.
 *
 * Copyright (C) 2017-2018 Scott Lin <scott.lin@tw.synaptics.com>
 *
 * Copyright (C) 2017-2018 Leon Tu <leon.tu@tw.synaptics.com>
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

#include <linux/crc32.h>
#include <linux/firmware.h>
#include <linux/spi/spi.h>
#include "synaptics_tcm_core.h"

#define SYSFS_DIR_NAME "romboot"

#define BOOT_CONFIG_ID "BOOT_CONFIG"

#define ROMBOOT_APP_CODE_ID "ROMBOOT_APP_CODE"

#define APP_CONFIG_ID "APP_CONFIG"

#define DISP_CONFIG_ID "DISPLAY"

#define IMAGE_FILE_MAGIC_VALUE 0x4818472b

#define FLASH_AREA_MAGIC_VALUE 0x7c05e516

#define BINARY_FILE_MAGIC_VALUE 0xaa55

#define FW_IMAGE_NAME "synaptics/hdl_firmware.img"

#define IMAGE_BUF_SIZE (512 * 1024)

#define IHEX_BUF_SIZE (2048 * 1024)

#define DATA_BUF_SIZE (512 * 256)

#define IHEX_RECORD_SIZE 14

#define ENABLE_SYSFS_INTERFACE true

#define RESERVED_BYTES 14

#define FLASH_PAGE_SIZE 256

#define STATUS_CHECK_US_MIN 5000

#define STATUS_CHECK_US_MAX 10000

#define STATUS_CHECK_RETRY 50

struct area_descriptor {
	unsigned char magic_value[4];
	unsigned char id_string[16];
	unsigned char flags[4];
	unsigned char flash_addr_words[4];
	unsigned char length[4];
	unsigned char checksum[4];
};

struct block_data {
	const unsigned char *data;
	unsigned int size;
	unsigned int flash_addr;
};

struct image_info {
	unsigned int packrat_number;
	struct block_data boot_config;
	struct block_data app_firmware;
	struct block_data app_config;
	struct block_data disp_config;
};

struct image_header {
	unsigned char magic_value[4];
	unsigned char num_of_areas[4];
};

struct romboot_hcd {
	bool has_rom_flash;
	bool force_update;
	const unsigned char *image;
	struct image_info image_info;
	unsigned char *image_buf;
	unsigned char *ihex_buf;
	unsigned char *data_buf;
	unsigned long int image_size;
	unsigned long int ihex_size;
	unsigned int ihex_records;
	unsigned int data_entries;
	unsigned int page_size;
	unsigned int write_block_size;
	unsigned int max_write_payload_size;
	const struct firmware *fw_entry;
	struct work_struct romboot_work;
	struct mutex romboot_mutex;
	struct kobject *sysfs_dir;
	struct workqueue_struct *workqueue;
	struct syna_tcm_buffer out;
	struct syna_tcm_buffer resp;
	struct syna_tcm_hcd *tcm_hcd;
};

enum flash_command {
	JEDEC_PAGE_PROGRAM = 0x02,
	JEDEC_READ_STATUS = 0x05,
	JEDEC_WRITE_ENABLE = 0x06,
	JEDEC_CHIP_ERASE = 0xc7,
};

struct flash_param {
	unsigned char spi_param;
	unsigned char clk_div;
	unsigned char mode;
	unsigned short read_size;
	unsigned char jedec_cmd;
} __packed;

DECLARE_COMPLETION(romboot_remove_complete);

static struct romboot_hcd *romboot_hcd;

static int romboot_get_fw_image(void);

STORE_PROTOTYPE(romboot, download)

STORE_PROTOTYPE(romboot, program)

static struct device_attribute *attrs[] = {
	ATTRIFY(download),
	ATTRIFY(program),
};

static void romboot_do_download(struct syna_tcm_hcd *tcm_hcd);

static void romboot_do_program(struct syna_tcm_hcd *tcm_hcd);

static ssize_t romboot_sysfs_image_store(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count);

static ssize_t romboot_sysfs_ihex_store(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count);

static struct bin_attribute bin_attrs[] = {
	{
		.attr = {
			.name = "img",
			.mode = 0220,
		},
		.size = 0,
		.write = romboot_sysfs_image_store,
	},
	{
		.attr = {
			.name = "ihex",
			.mode = 0220,
		},
		.size = 0,
		.write = romboot_sysfs_ihex_store,
	},
};

static ssize_t romboot_sysfs_download_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned int input;
	struct syna_tcm_hcd *tcm_hcd = romboot_hcd->tcm_hcd;

	if (kstrtouint(buf, 0, &input) != 0)
		return -EINVAL;

	if (input)
		romboot_do_download(tcm_hcd);

	if (romboot_hcd->fw_entry) {
		release_firmware(romboot_hcd->fw_entry);
		romboot_hcd->fw_entry = NULL;
	}

	romboot_hcd->image = NULL;
	romboot_hcd->image_size = 0;

	return retval;
}

static ssize_t romboot_sysfs_program_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned int input;
	struct syna_tcm_hcd *tcm_hcd = romboot_hcd->tcm_hcd;

	if (kstrtouint(buf, 0, &input) != 0)
		return -EINVAL;

	if (input)
		romboot_do_program(tcm_hcd);

	if (romboot_hcd->fw_entry) {
		release_firmware(romboot_hcd->fw_entry);
		romboot_hcd->fw_entry = NULL;
	}

	romboot_hcd->image = NULL;
	romboot_hcd->image_size = 0;

	return retval;
}

static ssize_t romboot_sysfs_image_store(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count)
{
	int retval;

	struct syna_tcm_hcd *tcm_hcd = romboot_hcd->tcm_hcd;

	mutex_lock(&tcm_hcd->extif_mutex);

	retval = secure_memcpy(&romboot_hcd->image_buf[pos],
			IMAGE_BUF_SIZE - pos,
			buf,
			count,
			count);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to copy firmware image data\n");
		romboot_hcd->image_size = 0;
		goto exit;
	}

	romboot_hcd->image_size = pos + count;

	retval = count;

exit:
	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}


static ssize_t romboot_sysfs_ihex_store(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count)
{
	int retval;
	struct syna_tcm_hcd *tcm_hcd = romboot_hcd->tcm_hcd;

	mutex_lock(&tcm_hcd->extif_mutex);

	retval = secure_memcpy(&romboot_hcd->ihex_buf[pos],
			IHEX_BUF_SIZE - pos,
			buf,
			count,
			count);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to copy ihex data\n");
		romboot_hcd->ihex_size = 0;
		goto exit;
	}

	romboot_hcd->ihex_size = pos + count;

	retval = count;

exit:
	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}

static int romboot_parse_fw_image(void)
{
	unsigned int idx;
	unsigned int addr;
	unsigned int offset;
	unsigned int length;
	unsigned int checksum;
	unsigned int flash_addr;
	unsigned int magic_value;
	unsigned int num_of_areas;
	struct image_header *header;
	struct image_info *image_info;
	struct area_descriptor *descriptor;
	struct syna_tcm_hcd *tcm_hcd = romboot_hcd->tcm_hcd;
	const unsigned char *image;
	const unsigned char *content;

	image = romboot_hcd->image;
	image_info = &romboot_hcd->image_info;
	header = (struct image_header *)image;

	magic_value = le4_to_uint(header->magic_value);
	if ((magic_value & 0xffff) == BINARY_FILE_MAGIC_VALUE) {
		LOGN(tcm_hcd->pdev->dev.parent,
				"use binary file\n");
		image_info->app_firmware.size = romboot_hcd->image_size;
		image_info->app_firmware.data = romboot_hcd->image;
		LOGN(tcm_hcd->pdev->dev.parent,
				"Application firmware size = %ld\n",
				romboot_hcd->image_size);
		return 0;
	} else if (magic_value != IMAGE_FILE_MAGIC_VALUE) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Invalid image file magic value\n");
		return -EINVAL;
	}

	memset(image_info, 0x00, sizeof(*image_info));

	offset = sizeof(*header);
	num_of_areas = le4_to_uint(header->num_of_areas);

	for (idx = 0; idx < num_of_areas; idx++) {
		addr = le4_to_uint(image + offset);
		descriptor = (struct area_descriptor *)(image + addr);
		offset += 4;

		magic_value = le4_to_uint(descriptor->magic_value);
		if (magic_value != FLASH_AREA_MAGIC_VALUE)
			continue;

		length = le4_to_uint(descriptor->length);
		content = (unsigned char *)descriptor + sizeof(*descriptor);
		flash_addr = le4_to_uint(descriptor->flash_addr_words) * 2;
		checksum = le4_to_uint(descriptor->checksum);

		if (strncmp((char *)descriptor->id_string,
				BOOT_CONFIG_ID,
				strlen(BOOT_CONFIG_ID)) == 0) {
			if (checksum != (crc32(~0, content, length) ^ ~0)) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Boot config checksum error\n");
				return -EINVAL;
			}
			image_info->boot_config.size = length;
			image_info->boot_config.data = content;
			image_info->boot_config.flash_addr = flash_addr;
			LOGD(tcm_hcd->pdev->dev.parent,
					"Boot config size = %d\n",
					length);
			LOGD(tcm_hcd->pdev->dev.parent,
					"Boot config flash address = 0x%08x\n",
					flash_addr);
		} else if (strncmp((char *)descriptor->id_string,
				ROMBOOT_APP_CODE_ID,
				strlen(ROMBOOT_APP_CODE_ID)) == 0) {
			if (checksum != (crc32(~0, content, length) ^ ~0)) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Application firmware checksum error\n");
				return -EINVAL;
			}
			image_info->app_firmware.size = length;
			image_info->app_firmware.data = content;
			image_info->app_firmware.flash_addr = flash_addr;
			LOGD(tcm_hcd->pdev->dev.parent,
					"Application firmware size = %d\n",
					length);
			LOGD(tcm_hcd->pdev->dev.parent,
					"Application firmware flash address = 0x%08x\n",
					flash_addr);
		} else if (strncmp((char *)descriptor->id_string,
				APP_CONFIG_ID,
				strlen(APP_CONFIG_ID)) == 0) {
			if (checksum != (crc32(~0, content, length) ^ ~0)) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Application config checksum error\n");
				return -EINVAL;
			}
			image_info->app_config.size = length;
			image_info->app_config.data = content;
			image_info->app_config.flash_addr = flash_addr;
			image_info->packrat_number = le4_to_uint(&content[14]);
			LOGD(tcm_hcd->pdev->dev.parent,
					"Application config size = %d\n",
					length);
			LOGD(tcm_hcd->pdev->dev.parent,
					"Application config flash address = 0x%08x\n",
					flash_addr);
		} else if (strncmp((char *)descriptor->id_string,
				DISP_CONFIG_ID,
				strlen(DISP_CONFIG_ID)) == 0) {
			if (checksum != (crc32(~0, content, length) ^ ~0)) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Display config checksum error\n");
				return -EINVAL;
			}
			image_info->disp_config.size = length;
			image_info->disp_config.data = content;
			image_info->disp_config.flash_addr = flash_addr;
			LOGD(tcm_hcd->pdev->dev.parent,
					"Display config size = %d\n",
					length);
			LOGD(tcm_hcd->pdev->dev.parent,
					"Display config flash address = 0x%08x\n",
					flash_addr);
		}
	}

	return 0;
}

static int romboot_get_fw_image(void)
{
	int retval;
	struct syna_tcm_hcd *tcm_hcd = romboot_hcd->tcm_hcd;

	if (romboot_hcd->image == NULL) {
		retval = request_firmware(&romboot_hcd->fw_entry,
				FW_IMAGE_NAME,
				tcm_hcd->pdev->dev.parent);
		if (retval < 0) {
			LOGD(tcm_hcd->pdev->dev.parent,
					"Failed to request %s\n",
					FW_IMAGE_NAME);
			return retval;
		}

		LOGD(tcm_hcd->pdev->dev.parent,
				"Firmware image size = %d\n",
				(unsigned int)romboot_hcd->fw_entry->size);

		romboot_hcd->image = romboot_hcd->fw_entry->data;
	}
	retval = romboot_parse_fw_image();
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to parse firmware image\n");
		release_firmware(romboot_hcd->fw_entry);
		romboot_hcd->fw_entry = NULL;
		romboot_hcd->image = NULL;
		return retval;
	}

	return 0;
}

static void romboot_do_download(struct syna_tcm_hcd *tcm_hcd)
{
	int retval;
	unsigned char *out_buf = NULL;
	unsigned char *resp_buf = NULL;
	unsigned int resp_buf_size;
	unsigned int resp_length;
	unsigned int data_size_blocks;
	unsigned int image_size;

	LOGN(tcm_hcd->pdev->dev.parent,
			"%s\n", __func__);

	resp_buf = NULL;
	resp_buf_size = 0;


	mutex_lock(&tcm_hcd->extif_mutex);

	pm_stay_awake(&tcm_hcd->pdev->dev);

	mutex_lock(&romboot_hcd->romboot_mutex);


	if (tcm_hcd->id_info.mode != MODE_ROMBOOTLOADER) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Not in romboot\n");
		goto exit;
	}

	if (romboot_hcd->image_size != 0)
		romboot_hcd->image = romboot_hcd->image_buf;

	retval = romboot_get_fw_image();
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to request romboot.img\n");
		goto exit;
	}

	image_size = (unsigned int)romboot_hcd->image_info.app_firmware.size;

	LOGD(tcm_hcd->pdev->dev.parent,
			"image_size = %d\n",
			image_size);

	data_size_blocks = image_size / 16;

	out_buf = kzalloc(image_size + RESERVED_BYTES,
			GFP_KERNEL);

	memset(out_buf, 0x00, RESERVED_BYTES);

	out_buf[0] = romboot_hcd->image_info.app_firmware.size >> 16;

	retval = secure_memcpy(&out_buf[RESERVED_BYTES],
			romboot_hcd->image_info.app_firmware.size,
			romboot_hcd->image_info.app_firmware.data,
			romboot_hcd->image_info.app_firmware.size,
			romboot_hcd->image_info.app_firmware.size);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to copy payload\n");
		goto exit;
	}

	LOGD(tcm_hcd->pdev->dev.parent,
			"data_size_blocks: %d\n",
			data_size_blocks);

	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_ROMBOOT_DOWNLOAD,
			out_buf,
			image_size + RESERVED_BYTES,
			&resp_buf,
			&resp_buf_size,
			&resp_length,
			NULL,
			20);
	if (retval < 0) {
		if (retval == -ETIME) {
			retval = tcm_hcd->read_message(tcm_hcd,
					NULL,
					0);
			if (retval < 0 || tcm_hcd->in.buf[1] != STATUS_OK) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to wait for HOSTDOWNLOAD response");
				goto exit;
			} else {
				LOGN(tcm_hcd->pdev->dev.parent,
						"written blocks = %d",
						tcm_hcd->in.buf[4] |
						tcm_hcd->in.buf[5] << 8);
			}
		} else {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to write command HOSTDOWNLOAD");
			goto exit;
		}
	}

	retval = tcm_hcd->switch_mode(tcm_hcd, FW_MODE_BOOTLOADER);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to switch to bootloader");
		goto exit;
	}

exit:

	mutex_unlock(&romboot_hcd->romboot_mutex);

	pm_relax(&tcm_hcd->pdev->dev);

	mutex_unlock(&tcm_hcd->extif_mutex);

	kfree(out_buf);
}

static int romboot_add_data_entry(unsigned char data)
{
	struct syna_tcm_hcd *tcm_hcd = romboot_hcd->tcm_hcd;

	if (romboot_hcd->data_entries >= DATA_BUF_SIZE) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Reached data buffer size limit\n");
		return -EINVAL;
	}

	romboot_hcd->data_buf[romboot_hcd->data_entries++] = data;

	return 0;
}

static int romboot_parse_ihex(void)
{
	int retval;
	unsigned char colon;
	unsigned char *buf;
	unsigned int addr;
	unsigned int type;
	unsigned int addrl;
	unsigned int addrh;
	unsigned int data0;
	unsigned int data1;
	unsigned int count;
	unsigned int words;
	unsigned int offset;
	unsigned int record;
	struct syna_tcm_hcd *tcm_hcd = romboot_hcd->tcm_hcd;
	struct image_info *image_info = &romboot_hcd->image_info;

	words = 0;

	offset = 0;

	buf = romboot_hcd->ihex_buf;

	romboot_hcd->data_entries = 0;

	romboot_hcd->ihex_records = romboot_hcd->ihex_size / IHEX_RECORD_SIZE;

	memset(romboot_hcd->data_buf, 0xff, DATA_BUF_SIZE);

	for (record = 0; record < romboot_hcd->ihex_records; record++) {
		buf[(record + 1) * IHEX_RECORD_SIZE - 1] = 0x00;
		retval = sscanf(&buf[record * IHEX_RECORD_SIZE],
				"%c%02x%02x%02x%02x%02x%02x",
				&colon,
				&count,
				&addrh,
				&addrl,
				&type,
				&data0,
				&data1);
		if (retval != 7) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to read ihex record\n");
			return -EINVAL;
		}

		if (type == 0x00) {
			addr = (addrh << 8) + addrl;
			addr += offset;

			romboot_hcd->data_entries = addr;

			retval = romboot_add_data_entry(data0);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to add data entry\n");
				return retval;
			}

			retval = romboot_add_data_entry(data1);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to add data entry\n");
				return retval;
			}

			words++;
		} else if (type == 0x02) {
			offset = (data0 << 8) + data1;
			offset <<= 4;

			romboot_hcd->data_entries = offset;
		}
	}

	image_info->app_firmware.size = DATA_BUF_SIZE;
	image_info->app_firmware.data = romboot_hcd->data_buf;

	return 0;
}

static int romboot_flash_command(struct syna_tcm_hcd *tcm_hcd,
		unsigned char flash_command, unsigned char *out,
		unsigned int out_size, unsigned char *in,
		unsigned int in_size)
{
	int retval;
	unsigned char *payld_buf = NULL;
	unsigned char *resp_buf = NULL;
	unsigned int resp_buf_size;
	unsigned int resp_length;
	struct flash_param flash_param = {1, 0x19, 0, 0, 0};

	flash_param.read_size = in_size;

	flash_param.jedec_cmd = flash_command;

	resp_buf = NULL;
	resp_buf_size = 0;

	payld_buf = kzalloc(sizeof(flash_param) + out_size,
			GFP_KERNEL);

	memcpy(payld_buf, &flash_param, sizeof(flash_param));

	memcpy(payld_buf + sizeof(flash_param), out, out_size);

	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_SPI_MASTER_WRITE_THEN_READ_EXTENDED,
			payld_buf,
			sizeof(flash_param) + out_size,
			&resp_buf,
			&resp_buf_size,
			&resp_length,
			NULL,
			20);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command CMD_SPI_MASTER_WRITE_THEN_READ_EXTENDED");
		goto exit;
	}

	if (in_size && (in_size <= resp_length))
		memcpy(in, resp_buf, in_size);

exit:
	kfree(payld_buf);
	kfree(resp_buf);
	return retval;
}

static int romboot_flash_status(struct syna_tcm_hcd *tcm_hcd)
{
	int retval;
	int idx;
	unsigned char status;

	for (idx = 0; idx < STATUS_CHECK_RETRY; idx++) {
		retval = romboot_flash_command(tcm_hcd, JEDEC_READ_STATUS,
				NULL, 0, &status, sizeof(status));
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to write JEDEC_READ_STATUS");
			return retval;
		}

		usleep_range(STATUS_CHECK_US_MIN, STATUS_CHECK_US_MAX);

		if (!status)
			break;
	}

	if (status)
		retval = -EIO;
	else
		retval = status;

	return retval;
}

static int romboot_flash_erase(struct syna_tcm_hcd *tcm_hcd)
{
	int retval;

		LOGE(tcm_hcd->pdev->dev.parent,
				"%s", __func__);

	retval = romboot_flash_command(tcm_hcd, JEDEC_WRITE_ENABLE,
			NULL, 0, NULL, 0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write JEDEC_WRITE_ENABLE");
		return retval;
	}

	retval = romboot_flash_command(tcm_hcd, JEDEC_CHIP_ERASE,
			NULL, 0, NULL, 0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write JEDEC_CHIP_ERASE");
		return retval;
	}

	retval = romboot_flash_status(tcm_hcd);
	if (retval < 0)
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to get correct status: %d", retval);

	return retval;
}

static int romboot_flash_program(struct syna_tcm_hcd *tcm_hcd,
		unsigned char *image_buf, unsigned int image_size)
{
	int retval;
	int idx;
	unsigned short img_header;
	unsigned int pages;
	unsigned char buf[FLASH_PAGE_SIZE + 3];

	img_header = image_buf[0] | image_buf[1] << 8;
	if ((image_size % FLASH_PAGE_SIZE) ||
			(img_header != BINARY_FILE_MAGIC_VALUE)) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Wrong image file");
		LOGE(tcm_hcd->pdev->dev.parent,
				"image_size = %d, img_header = 0x%04x",
				image_size, img_header);
		return -EINVAL;
	}

	pages = image_size / FLASH_PAGE_SIZE;

	for (idx = 0; idx < pages; idx++) {
		retval = romboot_flash_command(tcm_hcd, JEDEC_WRITE_ENABLE,
				NULL, 0, NULL, 0);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to write JEDEC_WRITE_ENABLE");
			return retval;
		}

		buf[0] = FLASH_PAGE_SIZE * idx >> 16;
		buf[1] = FLASH_PAGE_SIZE * idx >> 8;
		buf[2] = FLASH_PAGE_SIZE * idx;

		memcpy(buf + 3, image_buf + FLASH_PAGE_SIZE * idx,
				FLASH_PAGE_SIZE);

		retval = romboot_flash_command(tcm_hcd, JEDEC_PAGE_PROGRAM,
				buf, sizeof(buf), NULL, 0);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to write JEDEC_READ_STATUS");
			return retval;
		}

		retval = romboot_flash_status(tcm_hcd);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to get correct status: %d",
					retval);
			return retval;
		}

	}

	return retval;
}

static void romboot_do_program(struct syna_tcm_hcd *tcm_hcd)
{
	int retval;
	unsigned int image_size;
	unsigned char *out_buf = NULL;

	LOGN(tcm_hcd->pdev->dev.parent,
			"%s\n", __func__);

	mutex_lock(&tcm_hcd->extif_mutex);

	pm_stay_awake(&tcm_hcd->pdev->dev);

	mutex_lock(&romboot_hcd->romboot_mutex);

	if (tcm_hcd->id_info.mode != MODE_ROMBOOTLOADER) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Not in romboot\n");
		goto do_program_exit;
	}

	retval = romboot_parse_ihex();
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to parse ihex data\n");
		goto do_program_exit;
	}

	image_size = (unsigned int)romboot_hcd->image_info.app_firmware.size;

	out_buf = kzalloc(image_size, GFP_KERNEL);

	retval = secure_memcpy(out_buf,
			romboot_hcd->image_info.app_firmware.size,
			romboot_hcd->image_info.app_firmware.data,
			romboot_hcd->image_info.app_firmware.size,
			romboot_hcd->image_info.app_firmware.size);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to copy payload\n");
		goto do_program_exit;
	}

	LOGD(tcm_hcd->pdev->dev.parent,
			"image_size = %d\n",
			image_size);

	retval = romboot_flash_erase(tcm_hcd);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to erase chip");
		goto do_program_exit;
	}

	retval = romboot_flash_program(tcm_hcd, out_buf, image_size);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to program chip");
		goto do_program_exit;
	}

	retval = tcm_hcd->reset(tcm_hcd, true, true);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to reset");
	}

do_program_exit:

	mutex_unlock(&romboot_hcd->romboot_mutex);

	pm_relax(&tcm_hcd->pdev->dev);

	mutex_unlock(&tcm_hcd->extif_mutex);

	kfree(out_buf);
}

static void romboot_download_firmware(void)
{
	queue_work(romboot_hcd->workqueue, &romboot_hcd->romboot_work);
}

static void romboot_download_work(struct work_struct *work)
{
	struct syna_tcm_hcd *tcm_hcd = romboot_hcd->tcm_hcd;

	atomic_set(&tcm_hcd->host_downloading, 1);
	romboot_do_download(tcm_hcd);
}

static int romboot_init(struct syna_tcm_hcd *tcm_hcd)
{
	int retval;
	int idx;

	romboot_hcd = kzalloc(sizeof(*romboot_hcd), GFP_KERNEL);
	if (!romboot_hcd) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for romboot_hcd\n");
		return -ENOMEM;
	}

	romboot_hcd->image_buf = kzalloc(IMAGE_BUF_SIZE, GFP_KERNEL);
		if (!romboot_hcd->image_buf) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to allocate memory for romboot_hcd->image_buf\n");
			goto err_allocate_memory;
	}

	romboot_hcd->ihex_buf = kzalloc(IHEX_BUF_SIZE, GFP_KERNEL);
	if (!romboot_hcd->ihex_buf) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for romboot_hcd->ihex_buf\n");
		goto err_allocate_ihex_buf;
	}

	romboot_hcd->data_buf = kzalloc(DATA_BUF_SIZE, GFP_KERNEL);
	if (!romboot_hcd->data_buf) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for romboot_hcd->data_buf\n");
		goto err_allocate_data_buf;
	}

	romboot_hcd->tcm_hcd = tcm_hcd;

	mutex_init(&romboot_hcd->romboot_mutex);

	INIT_BUFFER(romboot_hcd->out, false);
	INIT_BUFFER(romboot_hcd->resp, false);

	romboot_hcd->workqueue =
			create_singlethread_workqueue("syna_tcm_romboot");
	INIT_WORK(&romboot_hcd->romboot_work,
			romboot_download_work);

	romboot_hcd->sysfs_dir = kobject_create_and_add(SYSFS_DIR_NAME,
			tcm_hcd->sysfs_dir);
	if (!romboot_hcd->sysfs_dir) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to create sysfs directory\n");
		retval = -EINVAL;
		goto err_sysfs_create_dir;
	}

	for (idx = 0; idx < ARRAY_SIZE(attrs); idx++) {
		retval = sysfs_create_file(romboot_hcd->sysfs_dir,
				&(*attrs[idx]).attr);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to create sysfs file\n");
			goto err_sysfs_create_file;
		}
	}

	retval = sysfs_create_bin_file(romboot_hcd->sysfs_dir, &bin_attrs[0]);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to create sysfs bin file\n");
		goto err_sysfs_create_bin_file;
	}

	retval = sysfs_create_bin_file(romboot_hcd->sysfs_dir, &bin_attrs[1]);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to create sysfs hex file\n");
		goto err_sysfs_create_hex_file;
	}

	return 0;

err_sysfs_create_hex_file:
	sysfs_remove_bin_file(romboot_hcd->sysfs_dir, &bin_attrs[0]);

err_sysfs_create_bin_file:
	idx = ARRAY_SIZE(attrs);
	for (idx--; idx >= 0; idx--)
		sysfs_remove_file(romboot_hcd->sysfs_dir, &(*attrs[idx]).attr);

err_sysfs_create_file:
	kobject_put(romboot_hcd->sysfs_dir);

err_sysfs_create_dir:
	kfree(romboot_hcd->data_buf);

err_allocate_data_buf:
	kfree(romboot_hcd->ihex_buf);

err_allocate_ihex_buf:
	kfree(romboot_hcd->image_buf);

err_allocate_memory:
	RELEASE_BUFFER(romboot_hcd->resp);
	RELEASE_BUFFER(romboot_hcd->out);

	kfree(romboot_hcd);
	romboot_hcd = NULL;

	return retval;

}

static int romboot_remove(struct syna_tcm_hcd *tcm_hcd)
{
	int idx;

	if (!romboot_hcd)
		goto exit;

	if (romboot_hcd->fw_entry)
		release_firmware(romboot_hcd->fw_entry);

	if (true == ENABLE_SYSFS_INTERFACE) {
		sysfs_remove_bin_file(romboot_hcd->sysfs_dir, &bin_attrs[1]);
		sysfs_remove_bin_file(romboot_hcd->sysfs_dir, &bin_attrs[0]);

		for (idx = 0; idx < ARRAY_SIZE(attrs); idx++) {
			sysfs_remove_file(romboot_hcd->sysfs_dir,
					&(*attrs[idx]).attr);
		}

		kobject_put(romboot_hcd->sysfs_dir);
	}

	cancel_work_sync(&romboot_hcd->romboot_work);
	flush_workqueue(romboot_hcd->workqueue);
	destroy_workqueue(romboot_hcd->workqueue);

	RELEASE_BUFFER(romboot_hcd->resp);
	RELEASE_BUFFER(romboot_hcd->out);

	kfree(romboot_hcd->image_buf);
	kfree(romboot_hcd->data_buf);
	kfree(romboot_hcd->ihex_buf);
	kfree(romboot_hcd);
	romboot_hcd = NULL;

exit:
	complete(&romboot_remove_complete);

	return 0;
}

static int romboot_syncbox(struct syna_tcm_hcd *tcm_hcd)
{
	if (!romboot_hcd)
		return 0;

	switch (tcm_hcd->report.id) {
	case REPORT_ROMBOOT:
		romboot_download_firmware();
		break;
	default:
		break;
	}

	return 0;
}

static int romboot_reset(struct syna_tcm_hcd *tcm_hcd)
{
	int retval;

	if (!romboot_hcd) {
		retval = romboot_init(tcm_hcd);
		return retval;
	}

	return 0;
}

static struct syna_tcm_module_cb romboot_module = {
	.type = TCM_ROMBOOT,
	.init = romboot_init,
	.remove = romboot_remove,
	.syncbox = romboot_syncbox,
	.asyncbox = NULL,
	.reset = romboot_reset,
	.suspend = NULL,
	.resume = NULL,
	.early_suspend = NULL,
};

static int __init romboot_module_init(void)
{
	return syna_tcm_add_module(&romboot_module, true);
}

static void __exit romboot_module_exit(void)
{
	syna_tcm_add_module(&romboot_module, false);

	wait_for_completion(&romboot_remove_complete);
}

module_init(romboot_module_init);
module_exit(romboot_module_exit);

MODULE_AUTHOR("Synaptics, Inc.");
MODULE_DESCRIPTION("Synaptics TCM Romboot Module");
MODULE_LICENSE("GPL v2");
