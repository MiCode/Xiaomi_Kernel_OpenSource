/*
 * Synaptics TCM touchscreen driver
 *
 * Copyright (C) 2017-2019 Synaptics Incorporated. All rights reserved.
 *
 * Copyright (C) 2017-2019 Scott Lin <scott.lin@tw.synaptics.com>
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
#include "synaptics_tcm_core.h"

#define STARTUP_REFLASH

#define FORCE_REFLASH false

#define ENABLE_SYSFS_INTERFACE true

#define SYSFS_DIR_NAME "reflash"

#define CUSTOM_DIR_NAME "custom"

#define FW_IMAGE_NAME "synaptics_firmware.img"

#define BOOT_CONFIG_ID "BOOT_CONFIG"

#define APP_CODE_ID "APP_CODE"

#define PROD_TEST_ID "APP_PROD_TEST"

#define APP_CONFIG_ID "APP_CONFIG"

#define DISP_CONFIG_ID "DISPLAY"

#define FB_READY_COUNT 2

#define FB_READY_WAIT_MS 100

#define FB_READY_TIMEOUT_S 80

#define IMAGE_FILE_MAGIC_VALUE 0x4818472b

#define FLASH_AREA_MAGIC_VALUE 0x7c05e516

#define BOOT_CONFIG_SIZE 8

#define BOOT_CONFIG_SLOTS 16

#define IMAGE_BUF_SIZE (512 * 1024)

#define ERASE_FLASH_DELAY_MS 500

#define WRITE_FLASH_DELAY_MS 20

#define REFLASH (1 << 0)

#define FORCE_UPDATE (1 << 1)

#define APP_CFG_UPDATE (1 << 2)

#define DISP_CFG_UPDATE (1 << 3)

#define BOOT_CFG_UPDATE (1 << 4)

#define BOOT_CFG_LOCKDOWN (1 << 5)

#define reflash_write(p_name) \
static int reflash_write_##p_name(void) \
{ \
	int retval; \
	unsigned int size; \
	unsigned int flash_addr; \
	struct syna_tcm_hcd *tcm_hcd = reflash_hcd->tcm_hcd; \
	const unsigned char *data; \
\
	data = reflash_hcd->image_info.p_name.data; \
	size = reflash_hcd->image_info.p_name.size; \
	flash_addr = reflash_hcd->image_info.p_name.flash_addr; \
\
	retval = reflash_write_flash(flash_addr, data, size); \
	if (retval < 0) { \
		LOGE(tcm_hcd->pdev->dev.parent, \
				"Failed to write to flash\n"); \
		return retval; \
	} \
\
	return 0; \
}

#define reflash_erase(p_name) \
static int reflash_erase_##p_name(void) \
{ \
	int retval; \
	unsigned int size; \
	unsigned int flash_addr; \
	unsigned int page_start; \
	unsigned int page_count; \
	struct syna_tcm_hcd *tcm_hcd = reflash_hcd->tcm_hcd; \
\
	flash_addr = reflash_hcd->image_info.p_name.flash_addr; \
\
	page_start = flash_addr / reflash_hcd->page_size; \
\
	size = reflash_hcd->image_info.p_name.size; \
	page_count = ceil_div(size, reflash_hcd->page_size); \
\
	LOGD(tcm_hcd->pdev->dev.parent, \
			"Page start = %d\n", \
			page_start); \
\
	LOGD(tcm_hcd->pdev->dev.parent, \
			"Page count = %d\n", \
			page_count); \
\
	retval = reflash_erase_flash(page_start, page_count); \
	if (retval < 0) { \
		LOGE(tcm_hcd->pdev->dev.parent, \
				"Failed to erase flash pages\n"); \
		return retval; \
	} \
\
	return 0; \
}

#define reflash_update(p_name) \
static int reflash_update_##p_name(bool reset) \
{ \
	int retval; \
	struct syna_tcm_hcd *tcm_hcd = reflash_hcd->tcm_hcd; \
\
	retval = reflash_set_up_flash_access(); \
	if (retval < 0) { \
		LOGE(tcm_hcd->pdev->dev.parent, \
				"Failed to set up flash access\n"); \
		return retval; \
	} \
\
	tcm_hcd->update_watchdog(tcm_hcd, false); \
\
	retval = reflash_check_##p_name(); \
	if (retval < 0) { \
		LOGE(tcm_hcd->pdev->dev.parent, \
				"Failed "#p_name" partition check\n"); \
		reset = true; \
		goto reset; \
	} \
\
	retval = reflash_erase_##p_name(); \
	if (retval < 0) { \
		LOGE(tcm_hcd->pdev->dev.parent, \
				"Failed to erase "#p_name" partition\n"); \
		reset = true; \
		goto reset; \
	} \
\
	LOGN(tcm_hcd->pdev->dev.parent, \
			"Partition erased ("#p_name")\n"); \
\
	retval = reflash_write_##p_name(); \
	if (retval < 0) { \
		LOGE(tcm_hcd->pdev->dev.parent, \
				"Failed to write "#p_name" partition\n"); \
		reset = true; \
		goto reset; \
	} \
\
	LOGN(tcm_hcd->pdev->dev.parent, \
			"Partition written ("#p_name")\n"); \
\
	retval = 0; \
\
reset: \
	if (!reset) \
		goto exit; \
\
	if (tcm_hcd->reset(tcm_hcd, false, true) < 0) { \
		LOGE(tcm_hcd->pdev->dev.parent, \
				"Failed to do reset\n"); \
	} \
\
exit: \
	tcm_hcd->update_watchdog(tcm_hcd, true); \
\
	return retval; \
}

#define reflash_show_data() \
{ \
	LOCK_BUFFER(reflash_hcd->read); \
\
	readlen = MIN(count, reflash_hcd->read.data_length - pos); \
\
	retval = secure_memcpy(buf, \
			count, \
			&reflash_hcd->read.buf[pos], \
			reflash_hcd->read.buf_size - pos, \
			readlen); \
	if (retval < 0) { \
		LOGE(tcm_hcd->pdev->dev.parent, \
			"Failed to copy read data\n"); \
	} else { \
		retval = readlen; \
	} \
\
	UNLOCK_BUFFER(reflash_hcd->read); \
}

enum update_area {
	NONE = 0,
	FIRMWARE_CONFIG,
	CONFIG_ONLY,
};

struct app_config_header {
	unsigned short magic_value[4];
	unsigned char checksum[4];
	unsigned char length[2];
	unsigned char build_id[4];
	unsigned char customer_config_id[16];
};

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
	struct block_data boot_config;
	struct block_data app_firmware;
	struct block_data prod_test_firmware;
	struct block_data app_config;
	struct block_data disp_config;
};

struct image_header {
	unsigned char magic_value[4];
	unsigned char num_of_areas[4];
};

struct boot_config {
	union {
		unsigned char i2c_address;
		struct {
			unsigned char cpha:1;
			unsigned char cpol:1;
			unsigned char word0_b2__7:6;
		} __packed;
	};
	unsigned char attn_polarity:1;
	unsigned char attn_drive:2;
	unsigned char attn_pullup:1;
	unsigned char word0_b12__14:3;
	unsigned char used:1;
	unsigned short customer_part_id;
	unsigned short boot_timeout;
	unsigned short continue_on_reset:1;
	unsigned short word3_b1__15:15;
} __packed;

struct reflash_hcd {
	bool force_update;
	bool disp_cfg_update;
	const unsigned char *image;
	unsigned char *image_buf;
	unsigned int image_size;
	unsigned int page_size;
	unsigned int write_block_size;
	unsigned int max_write_payload_size;
	const struct firmware *fw_entry;
	struct mutex reflash_mutex;
	struct kobject *sysfs_dir;
	struct kobject *custom_dir;
	struct work_struct work;
	struct workqueue_struct *workqueue;
	struct image_info image_info;
	struct syna_tcm_buffer out;
	struct syna_tcm_buffer resp;
	struct syna_tcm_buffer read;
	struct syna_tcm_hcd *tcm_hcd;
};

DECLARE_COMPLETION(reflash_remove_complete);

static struct reflash_hcd *reflash_hcd;

static int reflash_get_fw_image(void);

static int reflash_read_data(enum flash_area area, bool run_app_firmware,
		struct syna_tcm_buffer *output);

static int reflash_update_custom_otp(const unsigned char *data,
		unsigned int offset, unsigned int datalen);

static int reflash_update_custom_lcm(const unsigned char *data,
		unsigned int offset, unsigned int datalen);

static int reflash_update_custom_oem(const unsigned char *data,
		unsigned int offset, unsigned int datalen);

static int reflash_update_boot_config(bool lock);

static int reflash_update_app_config(bool reset);

static int reflash_update_disp_config(bool reset);

static int reflash_do_reflash(void);

STORE_PROTOTYPE(reflash, reflash);

static struct device_attribute *attrs[] = {
	ATTRIFY(reflash),
};

static ssize_t reflash_sysfs_image_store(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count);

static ssize_t reflash_sysfs_lockdown_show(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count);

static ssize_t reflash_sysfs_lockdown_store(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count);

static ssize_t reflash_sysfs_lcm_show(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count);

static ssize_t reflash_sysfs_lcm_store(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count);

static ssize_t reflash_sysfs_oem_show(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count);

static ssize_t reflash_sysfs_oem_store(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count);

static struct bin_attribute bin_attrs[] = {
	{
		.attr = {
			.name = "image",
			.mode = 0220,
		},
		.size = 0,
		.write = reflash_sysfs_image_store,
	},
	{
		.attr = {
			.name = "lockdown",
			.mode = 0664,
		},
		.size = 0,
		.read = reflash_sysfs_lockdown_show,
		.write = reflash_sysfs_lockdown_store,
	},
	{
		.attr = {
			.name = "lcm",
			.mode = 0664,
		},
		.size = 0,
		.read = reflash_sysfs_lcm_show,
		.write = reflash_sysfs_lcm_store,
	},
	{
		.attr = {
			.name = "oem",
			.mode = 0664,
		},
		.size = 0,
		.read = reflash_sysfs_oem_show,
		.write = reflash_sysfs_oem_store,
	},
};

static ssize_t reflash_sysfs_reflash_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned int input;
	struct syna_tcm_hcd *tcm_hcd = reflash_hcd->tcm_hcd;

	if (kstrtouint(buf, 10, &input))
		return -EINVAL;

	mutex_lock(&tcm_hcd->extif_mutex);

	pm_stay_awake(&tcm_hcd->pdev->dev);

	mutex_lock(&reflash_hcd->reflash_mutex);

	if (reflash_hcd->image_size != 0)
		reflash_hcd->image = reflash_hcd->image_buf;

	reflash_hcd->force_update = input & FORCE_UPDATE ? true : false;

	if (input & REFLASH || input & FORCE_UPDATE) {
		retval = reflash_do_reflash();
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to do reflash\n");
			goto exit;
		}
	}

	if ((input & ~(REFLASH | FORCE_UPDATE)) == 0) {
		retval = count;
		goto exit;
	}

	retval = reflash_get_fw_image();
	if (retval < 0) {
		LOGD(tcm_hcd->pdev->dev.parent,
				"Failed to get firmware image\n");
		goto exit;
	}

	if (input & BOOT_CFG_LOCKDOWN) {
		retval = reflash_update_boot_config(true);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to lockdown boot config\n");
			goto exit;
		}
	} else if (input & BOOT_CFG_UPDATE) {
		retval = reflash_update_boot_config(false);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to update boot config\n");
			goto exit;
		}
	}

	if (input & REFLASH || input & FORCE_UPDATE) {
		retval = count;
		goto exit;
	}

	if (input & DISP_CFG_UPDATE) {
		if (input & APP_CFG_UPDATE)
			retval = reflash_update_disp_config(false);
		else
			retval = reflash_update_disp_config(true);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to reflash display config\n");
			goto exit;
		}
	}

	if (input & APP_CFG_UPDATE) {
		retval = reflash_update_app_config(true);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to reflash application config\n");
			goto exit;
		}
	}

	retval = count;

exit:
	if (reflash_hcd->fw_entry) {
		release_firmware(reflash_hcd->fw_entry);
		reflash_hcd->fw_entry = NULL;
	}

	reflash_hcd->image = NULL;
	reflash_hcd->image_size = 0;
	reflash_hcd->force_update = FORCE_REFLASH;

	mutex_unlock(&reflash_hcd->reflash_mutex);

	pm_relax(&tcm_hcd->pdev->dev);

	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}

static ssize_t reflash_sysfs_image_store(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count)
{
	int retval;
	struct syna_tcm_hcd *tcm_hcd = reflash_hcd->tcm_hcd;

	mutex_lock(&tcm_hcd->extif_mutex);

	retval = secure_memcpy(&reflash_hcd->image_buf[pos],
			IMAGE_BUF_SIZE - pos,
			buf,
			count,
			count);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to copy firmware image data\n");
		reflash_hcd->image_size = 0;
		goto exit;
	}

	reflash_hcd->image_size = pos + count;

	retval = count;

exit:
	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}

static ssize_t reflash_sysfs_lockdown_show(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count)
{
	int retval;
	unsigned int readlen;
	struct syna_tcm_hcd *tcm_hcd = reflash_hcd->tcm_hcd;

	mutex_lock(&tcm_hcd->extif_mutex);

	mutex_lock(&reflash_hcd->reflash_mutex);

	retval = reflash_read_data(CUSTOM_OTP, true, NULL);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to read lockdown data\n");
		goto exit;
	}

	reflash_show_data();

exit:
	mutex_unlock(&reflash_hcd->reflash_mutex);

	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}

static ssize_t reflash_sysfs_lockdown_store(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count)
{
	int retval;
	struct syna_tcm_hcd *tcm_hcd = reflash_hcd->tcm_hcd;

	mutex_lock(&tcm_hcd->extif_mutex);

	pm_stay_awake(&tcm_hcd->pdev->dev);

	mutex_lock(&reflash_hcd->reflash_mutex);

	retval = reflash_update_custom_otp(buf, pos, count);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to update custom OTP data\n");
		goto exit;
	}

	retval = count;

exit:
	mutex_unlock(&reflash_hcd->reflash_mutex);

	pm_relax(&tcm_hcd->pdev->dev);

	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}

static ssize_t reflash_sysfs_lcm_show(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count)
{
	int retval;
	unsigned int readlen;
	struct syna_tcm_hcd *tcm_hcd = reflash_hcd->tcm_hcd;

	mutex_lock(&tcm_hcd->extif_mutex);

	mutex_lock(&reflash_hcd->reflash_mutex);

	retval = reflash_read_data(CUSTOM_LCM, true, NULL);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to read LCM data\n");
		goto exit;
	}

	reflash_show_data();

exit:
	mutex_unlock(&reflash_hcd->reflash_mutex);

	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}

static ssize_t reflash_sysfs_lcm_store(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count)
{
	int retval;
	struct syna_tcm_hcd *tcm_hcd = reflash_hcd->tcm_hcd;

	mutex_lock(&tcm_hcd->extif_mutex);

	pm_stay_awake(&tcm_hcd->pdev->dev);

	mutex_lock(&reflash_hcd->reflash_mutex);

	retval = reflash_update_custom_lcm(buf, pos, count);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to update custom LCM data\n");
		goto exit;
	}

	retval = count;

exit:
	mutex_unlock(&reflash_hcd->reflash_mutex);

	pm_relax(&tcm_hcd->pdev->dev);

	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}

static ssize_t reflash_sysfs_oem_show(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count)
{
	int retval;
	unsigned int readlen;
	struct syna_tcm_hcd *tcm_hcd = reflash_hcd->tcm_hcd;

	mutex_lock(&tcm_hcd->extif_mutex);

	mutex_lock(&reflash_hcd->reflash_mutex);

	retval = reflash_read_data(CUSTOM_OEM, true, NULL);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to read OEM data\n");
		goto exit;
	}

	reflash_show_data();

exit:
	mutex_unlock(&reflash_hcd->reflash_mutex);

	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}

static ssize_t reflash_sysfs_oem_store(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count)
{
	int retval;
	struct syna_tcm_hcd *tcm_hcd = reflash_hcd->tcm_hcd;

	mutex_lock(&tcm_hcd->extif_mutex);

	pm_stay_awake(&tcm_hcd->pdev->dev);

	mutex_lock(&reflash_hcd->reflash_mutex);

	retval = reflash_update_custom_oem(buf, pos, count);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to update custom OEM data\n");
		goto exit;
	}

	retval = count;

exit:
	mutex_unlock(&reflash_hcd->reflash_mutex);

	pm_relax(&tcm_hcd->pdev->dev);

	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}

static int reflash_set_up_flash_access(void)
{
	int retval;
	unsigned int temp;
	struct syna_tcm_hcd *tcm_hcd = reflash_hcd->tcm_hcd;

	retval = tcm_hcd->identify(tcm_hcd, true);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to do identification\n");
		return retval;
	}

	if (tcm_hcd->id_info.mode == MODE_APPLICATION) {
		retval = tcm_hcd->switch_mode(tcm_hcd, FW_MODE_BOOTLOADER);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to enter bootloader mode\n");
			return retval;
		}
	}

	temp = tcm_hcd->boot_info.write_block_size_words;
	reflash_hcd->write_block_size = temp * 2;

	temp = le2_to_uint(tcm_hcd->boot_info.erase_page_size_words);
	reflash_hcd->page_size = temp * 2;

	temp = le2_to_uint(tcm_hcd->boot_info.max_write_payload_size);
	reflash_hcd->max_write_payload_size = temp;

	LOGD(tcm_hcd->pdev->dev.parent,
			"Write block size = %d\n",
			reflash_hcd->write_block_size);

	LOGD(tcm_hcd->pdev->dev.parent,
			"Page size = %d\n",
			reflash_hcd->page_size);

	LOGD(tcm_hcd->pdev->dev.parent,
			"Max write payload size = %d\n",
			reflash_hcd->max_write_payload_size);

	if (reflash_hcd->write_block_size > (tcm_hcd->wr_chunk_size - 5)) {
		LOGE(tcm_hcd->pdev->dev.parent,
			"Write size greater than available chunk space\n");
		return -EINVAL;
	}

	return 0;
}

static int reflash_parse_fw_image(void)
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
	struct syna_tcm_hcd *tcm_hcd = reflash_hcd->tcm_hcd;
	const unsigned char *image;
	const unsigned char *content;

	image = reflash_hcd->image;
	image_info = &reflash_hcd->image_info;
	header = (struct image_header *)image;

	reflash_hcd->disp_cfg_update = false;

	magic_value = le4_to_uint(header->magic_value);
	if (magic_value != IMAGE_FILE_MAGIC_VALUE) {
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

		if (!memcmp((char *)descriptor->id_string,
				BOOT_CONFIG_ID,
				strlen(BOOT_CONFIG_ID))) {
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
		} else if (!memcmp((char *)descriptor->id_string,
				APP_CODE_ID,
				strlen(APP_CODE_ID))) {
			if (checksum != (crc32(~0, content, length) ^ ~0)) {
				LOGE(tcm_hcd->pdev->dev.parent,
					"APP firmware checksum error\n");
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
		} else if (!memcmp((char *)descriptor->id_string,
				PROD_TEST_ID,
				strlen(PROD_TEST_ID))) {
			if (checksum != (crc32(~0, content, length) ^ ~0)) {
				LOGE(tcm_hcd->pdev->dev.parent,
					"Production test checksum error\n");
				return -EINVAL;
			}
			image_info->prod_test_firmware.size = length;
			image_info->prod_test_firmware.data = content;
			image_info->prod_test_firmware.flash_addr = flash_addr;
			LOGD(tcm_hcd->pdev->dev.parent,
				"Production test firmware size = %d\n",
				length);
			LOGD(tcm_hcd->pdev->dev.parent,
				"Production test flash address = 0x%08x\n",
				flash_addr);
		} else if (!memcmp((char *)descriptor->id_string,
				APP_CONFIG_ID,
				strlen(APP_CONFIG_ID))) {
			if (checksum != (crc32(~0, content, length) ^ ~0)) {
				LOGE(tcm_hcd->pdev->dev.parent,
					"Application config checksum error\n");
				return -EINVAL;
			}
			image_info->app_config.size = length;
			image_info->app_config.data = content;
			image_info->app_config.flash_addr = flash_addr;
			LOGD(tcm_hcd->pdev->dev.parent,
				"Application config size = %d\n",
				length);
			LOGD(tcm_hcd->pdev->dev.parent,
				"Application config flash address = 0x%08x\n",
				flash_addr);
		} else if (!memcmp((char *)descriptor->id_string,
				DISP_CONFIG_ID,
				strlen(DISP_CONFIG_ID))) {
			if (checksum != (crc32(~0, content, length) ^ ~0)) {
				LOGE(tcm_hcd->pdev->dev.parent,
					"Display config checksum error\n");
				return -EINVAL;
			}
			reflash_hcd->disp_cfg_update = true;
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

static int reflash_get_fw_image(void)
{
	int retval;
	struct syna_tcm_hcd *tcm_hcd = reflash_hcd->tcm_hcd;

	if (reflash_hcd->image == NULL) {
		retval = request_firmware(&reflash_hcd->fw_entry, FW_IMAGE_NAME,
				tcm_hcd->pdev->dev.parent);
		if (retval < 0) {
			LOGD(tcm_hcd->pdev->dev.parent,
					"Failed to request %s\n",
					FW_IMAGE_NAME);
			return retval;
		}

		LOGD(tcm_hcd->pdev->dev.parent,
				"Firmware image size = %d\n",
				(unsigned int)reflash_hcd->fw_entry->size);

		reflash_hcd->image = reflash_hcd->fw_entry->data;
		reflash_hcd->image_size = reflash_hcd->fw_entry->size;
	}

	retval = reflash_parse_fw_image();
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to parse firmware image\n");
		return retval;
	}

	return 0;
}

static enum update_area reflash_compare_id_info(void)
{
	enum update_area update_area;
	unsigned int idx;
	unsigned int image_fw_id;
	unsigned int device_fw_id;
	unsigned char *image_config_id;
	unsigned char *device_config_id;
	struct app_config_header *header;
	struct syna_tcm_hcd *tcm_hcd = reflash_hcd->tcm_hcd;
	const unsigned char *app_config_data;

	update_area = NONE;

	if (reflash_hcd->image_info.app_config.size < sizeof(*header)) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Invalid application config in image file\n");
		goto exit;
	}

	app_config_data = reflash_hcd->image_info.app_config.data;
	header = (struct app_config_header *)app_config_data;

	if (reflash_hcd->force_update) {
		update_area = FIRMWARE_CONFIG;
		goto exit;
	}

	if (tcm_hcd->id_info.mode != MODE_APPLICATION) {
		update_area = FIRMWARE_CONFIG;
		goto exit;
	}

	image_fw_id = le4_to_uint(header->build_id);
	device_fw_id = tcm_hcd->packrat_number;

	if (image_fw_id > device_fw_id) {
		LOGN(tcm_hcd->pdev->dev.parent,
			"Image firmware ID newer than device firmware ID\n");
		update_area = FIRMWARE_CONFIG;
		goto exit;
	} else if (image_fw_id < device_fw_id) {
		LOGN(tcm_hcd->pdev->dev.parent,
			"Image firmware ID older than device firmware ID\n");
		update_area = NONE;
		goto exit;
	}

	image_config_id = header->customer_config_id;
	device_config_id = tcm_hcd->app_info.customer_config_id;

	for (idx = 0; idx < 16; idx++) {
		if (image_config_id[idx] > device_config_id[idx]) {
			LOGN(tcm_hcd->pdev->dev.parent,
				"Image config ID newer than device's ID\n");
			update_area = CONFIG_ONLY;
			goto exit;
		} else if (image_config_id[idx] < device_config_id[idx]) {
			LOGN(tcm_hcd->pdev->dev.parent,
				"Image config ID older than device's ID\n");
			update_area = NONE;
			goto exit;
		}
	}

	update_area = NONE;

exit:
	if (update_area == NONE)
		LOGD(tcm_hcd->pdev->dev.parent, "No need to do reflash\n");
	else
		LOGD(tcm_hcd->pdev->dev.parent,
				"Updating %s\n",
				update_area == FIRMWARE_CONFIG ?
				"firmware and config" :
				"config only");

	return update_area;
}

static int reflash_read_flash(unsigned int address, unsigned char *data,
		unsigned int datalen)
{
	int retval;
	unsigned int length_words;
	unsigned int flash_addr_words;
	struct syna_tcm_hcd *tcm_hcd = reflash_hcd->tcm_hcd;

	LOCK_BUFFER(reflash_hcd->out);

	retval = syna_tcm_alloc_mem(tcm_hcd,
			&reflash_hcd->out,
			6);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
			"Failed to allocate memory for reflash_hcd->out.buf\n");
		UNLOCK_BUFFER(reflash_hcd->out);
		return retval;
	}

	length_words = datalen / 2;
	flash_addr_words = address / 2;

	reflash_hcd->out.buf[0] = (unsigned char)flash_addr_words;
	reflash_hcd->out.buf[1] = (unsigned char)(flash_addr_words >> 8);
	reflash_hcd->out.buf[2] = (unsigned char)(flash_addr_words >> 16);
	reflash_hcd->out.buf[3] = (unsigned char)(flash_addr_words >> 24);
	reflash_hcd->out.buf[4] = (unsigned char)length_words;
	reflash_hcd->out.buf[5] = (unsigned char)(length_words >> 8);

	LOCK_BUFFER(reflash_hcd->resp);

	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_READ_FLASH,
			reflash_hcd->out.buf,
			6,
			&reflash_hcd->resp.buf,
			&reflash_hcd->resp.buf_size,
			&reflash_hcd->resp.data_length,
			NULL,
			0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_READ_FLASH));
		UNLOCK_BUFFER(reflash_hcd->resp);
		UNLOCK_BUFFER(reflash_hcd->out);
		return retval;
	}

	UNLOCK_BUFFER(reflash_hcd->out);

	if (reflash_hcd->resp.data_length != datalen) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to read requested length\n");
		UNLOCK_BUFFER(reflash_hcd->resp);
		return -EIO;
	}

	retval = secure_memcpy(data,
			datalen,
			reflash_hcd->resp.buf,
			reflash_hcd->resp.buf_size,
			datalen);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to copy read data\n");
		UNLOCK_BUFFER(reflash_hcd->resp);
		return retval;
	}

	UNLOCK_BUFFER(reflash_hcd->resp);

	return 0;
}

static int reflash_read_data(enum flash_area area, bool run_app_firmware,
		struct syna_tcm_buffer *output)
{
	int retval;
	unsigned int temp;
	unsigned int addr;
	unsigned int length;
	struct syna_tcm_app_info *app_info;
	struct syna_tcm_boot_info *boot_info;
	struct syna_tcm_hcd *tcm_hcd = reflash_hcd->tcm_hcd;

	switch (area) {
	case CUSTOM_LCM:
	case CUSTOM_OEM:
	case PPDT:
		retval = tcm_hcd->get_data_location(tcm_hcd,
				area,
				&addr,
				&length);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to get data location\n");
			return retval;
		}
		break;
	default:
		break;
	}

	retval = reflash_set_up_flash_access();
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to set up flash access\n");
		return retval;
	}

	app_info = &tcm_hcd->app_info;
	boot_info = &tcm_hcd->boot_info;

	switch (area) {
	case BOOT_CONFIG:
		temp = le2_to_uint(boot_info->boot_config_start_block);
		addr = temp * reflash_hcd->write_block_size;
		length = BOOT_CONFIG_SIZE * BOOT_CONFIG_SLOTS;
		break;
	case APP_CONFIG:
		temp = le2_to_uint(app_info->app_config_start_write_block);
		addr = temp * reflash_hcd->write_block_size;
		length = le2_to_uint(app_info->app_config_size);
		break;
	case DISP_CONFIG:
		temp = le4_to_uint(boot_info->display_config_start_block);
		addr = temp * reflash_hcd->write_block_size;
		temp = le2_to_uint(boot_info->display_config_length_blocks);
		length = temp * reflash_hcd->write_block_size;
		break;
	case CUSTOM_OTP:
		temp = le2_to_uint(boot_info->custom_otp_start_block);
		addr = temp * reflash_hcd->write_block_size;
		temp = le2_to_uint(boot_info->custom_otp_length_blocks);
		length = temp * reflash_hcd->write_block_size;
		break;
	case CUSTOM_LCM:
	case CUSTOM_OEM:
	case PPDT:
		addr *= reflash_hcd->write_block_size;
		length *= reflash_hcd->write_block_size;
		break;
	default:
		LOGE(tcm_hcd->pdev->dev.parent,
				"Invalid data area\n");
		retval = -EINVAL;
		goto run_app_firmware;
	}

	if (addr == 0 || length == 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Data area unavailable\n");
		retval = -EINVAL;
		goto run_app_firmware;
	}

	LOCK_BUFFER(reflash_hcd->read);

	retval = syna_tcm_alloc_mem(tcm_hcd,
			&reflash_hcd->read,
			length);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
			"Failed to allocate memory for read.buf\n");
		UNLOCK_BUFFER(reflash_hcd->read);
		goto run_app_firmware;
	}

	retval = reflash_read_flash(addr, reflash_hcd->read.buf, length);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to read from flash\n");
		UNLOCK_BUFFER(reflash_hcd->read);
		goto run_app_firmware;
	}

	reflash_hcd->read.data_length = length;

	if (output != NULL) {
		retval = syna_tcm_alloc_mem(tcm_hcd,
				output,
				length);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for output->buf\n");
			UNLOCK_BUFFER(reflash_hcd->read);
			goto run_app_firmware;
		}

		retval = secure_memcpy(output->buf,
				output->buf_size,
				reflash_hcd->read.buf,
				reflash_hcd->read.buf_size,
				length);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to copy read data\n");
			UNLOCK_BUFFER(reflash_hcd->read);
			goto run_app_firmware;
		}

		output->data_length = length;
	}

	UNLOCK_BUFFER(reflash_hcd->read);

	retval = 0;

run_app_firmware:
	if (!run_app_firmware)
		goto exit;

	if (tcm_hcd->switch_mode(tcm_hcd, FW_MODE_APPLICATION) < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to run application firmware\n");
	}

exit:
	return retval;
}

static int reflash_check_boot_config(void)
{
	unsigned int temp;
	unsigned int image_addr;
	unsigned int device_addr;
	struct syna_tcm_hcd *tcm_hcd = reflash_hcd->tcm_hcd;

	if (reflash_hcd->image_info.boot_config.size < BOOT_CONFIG_SIZE) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"No valid boot config in image file\n");
		return -EINVAL;
	}

	image_addr = reflash_hcd->image_info.boot_config.flash_addr;

	temp = le2_to_uint(tcm_hcd->boot_info.boot_config_start_block);
	device_addr = temp * reflash_hcd->write_block_size;

	if (image_addr != device_addr) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Flash address mismatch\n");
		return -EINVAL;
	}

	return 0;
}

static int reflash_check_app_config(void)
{
	unsigned int temp;
	unsigned int image_addr;
	unsigned int image_size;
	unsigned int device_addr;
	unsigned int device_size;
	struct syna_tcm_hcd *tcm_hcd = reflash_hcd->tcm_hcd;

	if (reflash_hcd->image_info.app_config.size == 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"No application config in image file\n");
		return -EINVAL;
	}

	image_addr = reflash_hcd->image_info.app_config.flash_addr;
	image_size = reflash_hcd->image_info.app_config.size;

	temp = le2_to_uint(tcm_hcd->app_info.app_config_start_write_block);
	device_addr = temp * reflash_hcd->write_block_size;
	device_size = le2_to_uint(tcm_hcd->app_info.app_config_size);

	if (device_addr == 0 && device_size == 0)
		return 0;

	if (image_addr != device_addr) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Flash address mismatch\n");
		return -EINVAL;
	}

	if (image_size != device_size) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Config size mismatch\n");
		return -EINVAL;
	}

	return 0;
}

static int reflash_check_disp_config(void)
{
	unsigned int temp;
	unsigned int image_addr;
	unsigned int image_size;
	unsigned int device_addr;
	unsigned int device_size;
	struct syna_tcm_hcd *tcm_hcd = reflash_hcd->tcm_hcd;

	if (reflash_hcd->image_info.disp_config.size == 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"No display config in image file\n");
		return -EINVAL;
	}

	image_addr = reflash_hcd->image_info.disp_config.flash_addr;
	image_size = reflash_hcd->image_info.disp_config.size;

	temp = le4_to_uint(tcm_hcd->boot_info.display_config_start_block);
	device_addr = temp * reflash_hcd->write_block_size;

	temp = le2_to_uint(tcm_hcd->boot_info.display_config_length_blocks);
	device_size = temp * reflash_hcd->write_block_size;

	if (image_addr != device_addr) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Flash address mismatch\n");
		return -EINVAL;
	}

	if (image_size != device_size) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Config size mismatch\n");
		return -EINVAL;
	}

	return 0;
}

static int reflash_check_prod_test_firmware(void)
{
	struct syna_tcm_hcd *tcm_hcd = reflash_hcd->tcm_hcd;

	if (reflash_hcd->image_info.prod_test_firmware.size == 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"No production test firmware in image file\n");
		return -EINVAL;
	}

	return 0;
}

static int reflash_check_app_firmware(void)
{
	struct syna_tcm_hcd *tcm_hcd = reflash_hcd->tcm_hcd;

	if (reflash_hcd->image_info.app_firmware.size == 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"No application firmware in image file\n");
		return -EINVAL;
	}

	return 0;
}

static int reflash_write_flash(unsigned int address, const unsigned char *data,
		unsigned int datalen)
{
	int retval;
	unsigned int offset;
	unsigned int w_length;
	unsigned int xfer_length;
	unsigned int remaining_length;
	unsigned int flash_address;
	unsigned int block_address;
	struct syna_tcm_hcd *tcm_hcd = reflash_hcd->tcm_hcd;

	w_length = tcm_hcd->wr_chunk_size - 5;

	w_length = w_length - (w_length % reflash_hcd->write_block_size);

	w_length = MIN(w_length, reflash_hcd->max_write_payload_size);

	offset = 0;

	remaining_length = datalen;

	LOCK_BUFFER(reflash_hcd->out);
	LOCK_BUFFER(reflash_hcd->resp);

	while (remaining_length) {
		if (remaining_length > w_length)
			xfer_length = w_length;
		else
			xfer_length = remaining_length;

		retval = syna_tcm_alloc_mem(tcm_hcd,
				&reflash_hcd->out,
				xfer_length + 2);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for out.buf\n");
			UNLOCK_BUFFER(reflash_hcd->resp);
			UNLOCK_BUFFER(reflash_hcd->out);
			return retval;
		}

		flash_address = address + offset;
		block_address = flash_address / reflash_hcd->write_block_size;
		reflash_hcd->out.buf[0] = (unsigned char)block_address;
		reflash_hcd->out.buf[1] = (unsigned char)(block_address >> 8);

		retval = secure_memcpy(&reflash_hcd->out.buf[2],
				reflash_hcd->out.buf_size - 2,
				&data[offset],
				datalen - offset,
				xfer_length);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to copy write data\n");
			UNLOCK_BUFFER(reflash_hcd->resp);
			UNLOCK_BUFFER(reflash_hcd->out);
			return retval;
		}

		retval = tcm_hcd->write_message(tcm_hcd,
				CMD_WRITE_FLASH,
				reflash_hcd->out.buf,
				xfer_length + 2,
				&reflash_hcd->resp.buf,
				&reflash_hcd->resp.buf_size,
				&reflash_hcd->resp.data_length,
				NULL,
				WRITE_FLASH_DELAY_MS);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to write command %s\n",
					STR(CMD_WRITE_FLASH));
			LOGE(tcm_hcd->pdev->dev.parent,
					"Flash address = 0x%08x\n",
					flash_address);
			LOGE(tcm_hcd->pdev->dev.parent,
					"Data length = %d\n",
					xfer_length);
			UNLOCK_BUFFER(reflash_hcd->resp);
			UNLOCK_BUFFER(reflash_hcd->out);
			return retval;
		}

		offset += xfer_length;
		remaining_length -= xfer_length;
	}

	UNLOCK_BUFFER(reflash_hcd->resp);
	UNLOCK_BUFFER(reflash_hcd->out);

	return 0;
}

reflash_write(app_config)

reflash_write(disp_config)

reflash_write(prod_test_firmware)

reflash_write(app_firmware)

static int reflash_erase_flash(unsigned int page_start, unsigned int page_count)
{
	int retval;
	unsigned char out_buf[2];
	struct syna_tcm_hcd *tcm_hcd = reflash_hcd->tcm_hcd;

	out_buf[0] = (unsigned char)page_start;
	out_buf[1] = (unsigned char)page_count;

	LOCK_BUFFER(reflash_hcd->resp);

	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_ERASE_FLASH,
			out_buf,
			sizeof(out_buf),
			&reflash_hcd->resp.buf,
			&reflash_hcd->resp.buf_size,
			&reflash_hcd->resp.data_length,
			NULL,
			ERASE_FLASH_DELAY_MS);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_ERASE_FLASH));
		UNLOCK_BUFFER(reflash_hcd->resp);
		return retval;
	}

	UNLOCK_BUFFER(reflash_hcd->resp);

	return 0;
}

reflash_erase(app_config)

reflash_erase(disp_config)

reflash_erase(prod_test_firmware)

reflash_erase(app_firmware)

static int reflash_update_custom_otp(const unsigned char *data,
		unsigned int offset, unsigned int datalen)
{
	int retval;
	unsigned int temp;
	unsigned int addr;
	unsigned int length;
	struct syna_tcm_hcd *tcm_hcd = reflash_hcd->tcm_hcd;

	retval = reflash_set_up_flash_access();
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to set up flash access\n");
		return retval;
	}

	tcm_hcd->update_watchdog(tcm_hcd, false);

	temp = le2_to_uint(tcm_hcd->boot_info.custom_otp_start_block);
	addr = temp * reflash_hcd->write_block_size;

	temp = le2_to_uint(tcm_hcd->boot_info.custom_otp_length_blocks);
	length = temp * reflash_hcd->write_block_size;

	if (addr == 0 || length == 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Data area unavailable\n");
		retval = -EINVAL;
		goto run_app_firmware;
	}

	if (datalen + offset > length) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Invalid data length\n");
		retval = -EINVAL;
		goto run_app_firmware;
	}

	retval = reflash_write_flash(addr + offset,
			data,
			datalen);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write to flash\n");
		goto run_app_firmware;
	}

	retval = 0;

run_app_firmware:
	if (tcm_hcd->switch_mode(tcm_hcd, FW_MODE_APPLICATION) < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to run application firmware\n");
	}

	tcm_hcd->update_watchdog(tcm_hcd, true);

	return retval;
}

static int reflash_update_custom_lcm(const unsigned char *data,
		unsigned int offset, unsigned int datalen)
{
	int retval;
	unsigned int addr;
	unsigned int length;
	unsigned int page_start;
	unsigned int page_count;
	struct syna_tcm_hcd *tcm_hcd = reflash_hcd->tcm_hcd;

	retval = tcm_hcd->get_data_location(tcm_hcd,
			CUSTOM_LCM,
			&addr,
			&length);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to get data location\n");
		return retval;
	}

	retval = reflash_set_up_flash_access();
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to set up flash access\n");
		return retval;
	}

	tcm_hcd->update_watchdog(tcm_hcd, false);

	addr *= reflash_hcd->write_block_size;
	length *= reflash_hcd->write_block_size;

	if (addr == 0 || length == 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Data area unavailable\n");
		retval = -EINVAL;
		goto run_app_firmware;
	}

	if (datalen + offset > length) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Invalid data length\n");
		retval = -EINVAL;
		goto run_app_firmware;
	}

	if (offset == 0) {
		page_start = addr / reflash_hcd->page_size;

		page_count = ceil_div(length, reflash_hcd->page_size);

		retval = reflash_erase_flash(page_start, page_count);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to erase flash pages\n");
			goto run_app_firmware;
		}
	}

	retval = reflash_write_flash(addr + offset,
			data,
			datalen);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write to flash\n");
		goto run_app_firmware;
	}

	retval = 0;

run_app_firmware:
	if (tcm_hcd->switch_mode(tcm_hcd, FW_MODE_APPLICATION) < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to run application firmware\n");
	}

	tcm_hcd->update_watchdog(tcm_hcd, true);

	return retval;
}

static int reflash_update_custom_oem(const unsigned char *data,
		unsigned int offset, unsigned int datalen)
{
	int retval;
	unsigned int addr;
	unsigned int length;
	unsigned int page_start;
	unsigned int page_count;
	struct syna_tcm_hcd *tcm_hcd = reflash_hcd->tcm_hcd;

	retval = tcm_hcd->get_data_location(tcm_hcd,
			CUSTOM_OEM,
			&addr,
			&length);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to get data location\n");
		return retval;
	}

	retval = reflash_set_up_flash_access();
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to set up flash access\n");
		return retval;
	}

	tcm_hcd->update_watchdog(tcm_hcd, false);

	addr *= reflash_hcd->write_block_size;
	length *= reflash_hcd->write_block_size;

	if (addr == 0 || length == 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Data area unavailable\n");
		retval = -EINVAL;
		goto run_app_firmware;
	}

	if (datalen + offset > length) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Invalid data length\n");
		retval = -EINVAL;
		goto run_app_firmware;
	}

	if (offset == 0) {
		page_start = addr / reflash_hcd->page_size;

		page_count = ceil_div(length, reflash_hcd->page_size);

		retval = reflash_erase_flash(page_start, page_count);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to erase flash pages\n");
			goto run_app_firmware;
		}
	}

	retval = reflash_write_flash(addr + offset,
			data,
			datalen);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write to flash\n");
		goto run_app_firmware;
	}

	retval = 0;

run_app_firmware:
	if (tcm_hcd->switch_mode(tcm_hcd, FW_MODE_APPLICATION) < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to run application firmware\n");
	}

	tcm_hcd->update_watchdog(tcm_hcd, true);

	return retval;
}

static int reflash_update_boot_config(bool lock)
{
	int retval;
	unsigned char slot_used;
	unsigned int idx;
	unsigned int addr;
	struct boot_config *data;
	struct boot_config *last_slot;
	struct syna_tcm_hcd *tcm_hcd = reflash_hcd->tcm_hcd;

	retval = reflash_set_up_flash_access();
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to set up flash access\n");
		return retval;
	}

	tcm_hcd->update_watchdog(tcm_hcd, false);

	retval = reflash_check_boot_config();
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed boot_config partition check\n");
		goto reset;
	}

	retval = reflash_read_data(BOOT_CONFIG, false, NULL);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to read boot config\n");
		goto reset;
	}

	LOCK_BUFFER(reflash_hcd->read);

	data = (struct boot_config *)reflash_hcd->read.buf;
	last_slot = data + (BOOT_CONFIG_SLOTS - 1);
	slot_used = tcm_hcd->id_info.mode == MODE_TDDI_BOOTLOADER ? 0 : 1;

	if (last_slot->used == slot_used) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Boot config already locked down\n");
		UNLOCK_BUFFER(reflash_hcd->read);
		goto reset;
	}

	if (lock) {
		idx = BOOT_CONFIG_SLOTS - 1;
	} else {
		for (idx = 0; idx < BOOT_CONFIG_SLOTS; idx++) {
			if (data->used == slot_used) {
				data++;
				continue;
			} else {
				break;
			}
		}
	}

	UNLOCK_BUFFER(reflash_hcd->read);

	if (idx == BOOT_CONFIG_SLOTS) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"No free boot config slot available\n");
		goto reset;
	}

	addr += idx * BOOT_CONFIG_SIZE;

	retval = reflash_write_flash(addr,
			reflash_hcd->image_info.boot_config.data,
			BOOT_CONFIG_SIZE);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write to flash\n");
		goto reset;
	}

	LOGN(tcm_hcd->pdev->dev.parent,
			"Slot %d updated with new boot config\n",
			idx);

	retval = 0;

reset:
	if (tcm_hcd->reset(tcm_hcd, false, true) < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to do reset\n");
	}

	tcm_hcd->update_watchdog(tcm_hcd, true);

	return retval;
}

reflash_update(app_config)

reflash_update(disp_config)

reflash_update(prod_test_firmware)

reflash_update(app_firmware)

static int reflash_do_reflash(void)
{
	int retval;
	enum update_area update_area;
	struct syna_tcm_hcd *tcm_hcd = reflash_hcd->tcm_hcd;

	retval = reflash_get_fw_image();
	if (retval < 0) {
		LOGD(tcm_hcd->pdev->dev.parent,
				"Failed to get firmware image\n");
		goto exit;
	}

	LOGD(tcm_hcd->pdev->dev.parent,
			"Start of reflash\n");

	atomic_set(&tcm_hcd->firmware_flashing, 1);

	update_area = reflash_compare_id_info();

	switch (update_area) {
	case FIRMWARE_CONFIG:
		retval = reflash_update_app_firmware(false);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to reflash application firmware\n");
			goto exit;
		}
		memset(&tcm_hcd->app_info, 0x00, sizeof(tcm_hcd->app_info));
		if (tcm_hcd->features.dual_firmware) {
			retval = reflash_update_prod_test_firmware(false);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to reflash production test\n");
				goto exit;
			}
		}
	case CONFIG_ONLY:
		if (reflash_hcd->disp_cfg_update) {
			retval = reflash_update_disp_config(false);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to reflash display config\n");
				goto exit;
			}
		}
		retval = reflash_update_app_config(true);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to reflash application config\n");
			goto exit;
		}
		break;
	case NONE:
	default:
		break;
	}

	LOGD(tcm_hcd->pdev->dev.parent,
			"End of reflash\n");

	retval = 0;

exit:
	if (reflash_hcd->fw_entry) {
		release_firmware(reflash_hcd->fw_entry);
		reflash_hcd->fw_entry = NULL;
		reflash_hcd->image = NULL;
		reflash_hcd->image_size = 0;
	}

	atomic_set(&tcm_hcd->firmware_flashing, 0);
	wake_up_interruptible(&tcm_hcd->reflash_wq);
	return retval;
}

#ifdef STARTUP_REFLASH
static void reflash_startup_work(struct work_struct *work)
{
	int retval;
#if defined(CONFIG_DRM) || defined(CONFIG_FB)
	unsigned int timeout;
#endif
	struct syna_tcm_hcd *tcm_hcd = reflash_hcd->tcm_hcd;

#if defined(CONFIG_DRM) || defined(CONFIG_FB)
	timeout = FB_READY_TIMEOUT_S * 1000 / FB_READY_WAIT_MS;

	while (tcm_hcd->fb_ready != FB_READY_COUNT - 1) {
		if (timeout == 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Timed out waiting for FB ready\n");
			return;
		}
		msleep(FB_READY_WAIT_MS);
		timeout--;
	}
#endif

	pm_stay_awake(&tcm_hcd->pdev->dev);

	mutex_lock(&reflash_hcd->reflash_mutex);

	retval = reflash_do_reflash();
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to do reflash\n");
	}

	mutex_unlock(&reflash_hcd->reflash_mutex);

	pm_relax(&tcm_hcd->pdev->dev);
}
#endif

static int reflash_init(struct syna_tcm_hcd *tcm_hcd)
{
	int retval;
	int idx;

	reflash_hcd = kzalloc(sizeof(*reflash_hcd), GFP_KERNEL);
	if (!reflash_hcd) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for reflash_hcd\n");
		return -ENOMEM;
	}

	reflash_hcd->image_buf = kzalloc(IMAGE_BUF_SIZE, GFP_KERNEL);
	if (!reflash_hcd->image_buf) {
		LOGE(tcm_hcd->pdev->dev.parent,
			"Failed to allocate memory for image_buf\n");
		goto err_allocate_memory;
	}

	reflash_hcd->tcm_hcd = tcm_hcd;

	reflash_hcd->force_update = FORCE_REFLASH;

	mutex_init(&reflash_hcd->reflash_mutex);

	INIT_BUFFER(reflash_hcd->out, false);
	INIT_BUFFER(reflash_hcd->resp, false);
	INIT_BUFFER(reflash_hcd->read, false);

#ifdef STARTUP_REFLASH
	reflash_hcd->workqueue =
			create_singlethread_workqueue("syna_tcm_reflash");
	INIT_WORK(&reflash_hcd->work, reflash_startup_work);
	queue_work(reflash_hcd->workqueue, &reflash_hcd->work);
#endif

	if (!ENABLE_SYSFS_INTERFACE)
		return 0;

	reflash_hcd->sysfs_dir = kobject_create_and_add(SYSFS_DIR_NAME,
			tcm_hcd->sysfs_dir);
	if (!reflash_hcd->sysfs_dir) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to create sysfs directory\n");
		retval = -EINVAL;
		goto err_sysfs_create_dir;
	}

	for (idx = 0; idx < ARRAY_SIZE(attrs); idx++) {
		retval = sysfs_create_file(reflash_hcd->sysfs_dir,
				&(*attrs[idx]).attr);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to create sysfs file\n");
			goto err_sysfs_create_file;
		}
	}

	retval = sysfs_create_bin_file(reflash_hcd->sysfs_dir, &bin_attrs[0]);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to create sysfs bin file\n");
		goto err_sysfs_create_bin_file;
	}

	reflash_hcd->custom_dir = kobject_create_and_add(CUSTOM_DIR_NAME,
			reflash_hcd->sysfs_dir);
	if (!reflash_hcd->custom_dir) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to create custom sysfs directory\n");
		retval = -EINVAL;
		goto err_custom_sysfs_create_dir;
	}

	for (idx = 1; idx < ARRAY_SIZE(bin_attrs); idx++) {
		retval = sysfs_create_bin_file(reflash_hcd->custom_dir,
				&bin_attrs[idx]);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to create sysfs bin file\n");
			goto err_custom_sysfs_create_bin_file;
		}
	}

	tcm_hcd->read_flash_data = reflash_read_data;

	return 0;

err_custom_sysfs_create_bin_file:
	for (idx--; idx > 0; idx--)
		sysfs_remove_bin_file(reflash_hcd->custom_dir, &bin_attrs[idx]);

	kobject_put(reflash_hcd->custom_dir);

	idx = ARRAY_SIZE(attrs);

err_custom_sysfs_create_dir:
	sysfs_remove_bin_file(reflash_hcd->sysfs_dir, &bin_attrs[0]);

err_sysfs_create_bin_file:
err_sysfs_create_file:
	for (idx--; idx >= 0; idx--)
		sysfs_remove_file(reflash_hcd->sysfs_dir, &(*attrs[idx]).attr);

	kobject_put(reflash_hcd->sysfs_dir);

err_sysfs_create_dir:
err_allocate_memory:
	kfree(reflash_hcd->image_buf);

	RELEASE_BUFFER(reflash_hcd->read);
	RELEASE_BUFFER(reflash_hcd->resp);
	RELEASE_BUFFER(reflash_hcd->out);

	kfree(reflash_hcd);
	reflash_hcd = NULL;

	return retval;
}

static int reflash_remove(struct syna_tcm_hcd *tcm_hcd)
{
	int idx;

	if (!reflash_hcd)
		goto exit;

	tcm_hcd->read_flash_data = NULL;

	if (ENABLE_SYSFS_INTERFACE) {
		for (idx = 1; idx < ARRAY_SIZE(bin_attrs); idx++) {
			sysfs_remove_bin_file(reflash_hcd->custom_dir,
					&bin_attrs[idx]);
		}

		kobject_put(reflash_hcd->custom_dir);

		sysfs_remove_bin_file(reflash_hcd->sysfs_dir, &bin_attrs[0]);

		for (idx = 0; idx < ARRAY_SIZE(attrs); idx++) {
			sysfs_remove_file(reflash_hcd->sysfs_dir,
					&(*attrs[idx]).attr);
		}

		kobject_put(reflash_hcd->sysfs_dir);
	}

#ifdef STARTUP_REFLASH
	cancel_work_sync(&reflash_hcd->work);
	flush_workqueue(reflash_hcd->workqueue);
	destroy_workqueue(reflash_hcd->workqueue);
#endif

	kfree(reflash_hcd->image_buf);

	RELEASE_BUFFER(reflash_hcd->read);
	RELEASE_BUFFER(reflash_hcd->resp);
	RELEASE_BUFFER(reflash_hcd->out);

	kfree(reflash_hcd);
	reflash_hcd = NULL;

exit:
	complete(&reflash_remove_complete);

	return 0;
}

static int reflash_reset(struct syna_tcm_hcd *tcm_hcd)
{
	int retval;

	if (!reflash_hcd) {
		retval = reflash_init(tcm_hcd);
		return retval;
	}

	return 0;
}

static struct syna_tcm_module_cb reflash_module = {
	.type = TCM_REFLASH,
	.init = reflash_init,
	.remove = reflash_remove,
	.syncbox = NULL,
	.asyncbox = NULL,
	.reset = reflash_reset,
	.suspend = NULL,
	.resume = NULL,
	.early_suspend = NULL,
};

static int __init reflash_module_init(void)
{
	return syna_tcm_add_module(&reflash_module, true);
}

static void __exit reflash_module_exit(void)
{
	syna_tcm_add_module(&reflash_module, false);

	wait_for_completion(&reflash_remove_complete);
}

module_init(reflash_module_init);
module_exit(reflash_module_exit);

MODULE_AUTHOR("Synaptics, Inc.");
MODULE_DESCRIPTION("Synaptics TCM Reflash Module");
MODULE_LICENSE("GPL v2");
