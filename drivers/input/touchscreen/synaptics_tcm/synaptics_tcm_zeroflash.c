/*
 * Synaptics TCM touchscreen driver
 *
 * Copyright (C) 2017-2018 Synaptics Incorporated. All rights reserved.
 *
 * Copyright (C) 2017-2018 Scott Lin <scott.lin@tw.synaptics.com>
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

#include <linux/gpio.h>
#include <linux/crc32.h>
#include <linux/firmware.h>
#include "synaptics_tcm_core.h"

/* add check F7A LCM by wanghan start */
extern bool lct_syna_verify_flag;
/* add check F7A LCM by wanghan end */

#define FW_IMAGE_NAME "synaptics/hdl_firmware.img"

#define BOOT_CONFIG_ID "BOOT_CONFIG"

#define F35_APP_CODE_ID "F35_APP_CODE"

#define APP_CONFIG_ID "APP_CONFIG"

#define DISP_CONFIG_ID "DISPLAY"

#define IMAGE_FILE_MAGIC_VALUE 0x4818472b

#define FLASH_AREA_MAGIC_VALUE 0x7c05e516

#define PDT_START_ADDR 0x00e9

#define PDT_END_ADDR 0x00ee

#define UBL_FN_NUMBER 0x35

#define F35_CTRL3_OFFSET 18

#define F35_CTRL7_OFFSET 22

#define F35_WRITE_FW_TO_PMEM_COMMAND 4

#define RESET_TO_HDL_DELAY_MS 0

#define DOWNLOAD_RETRY_COUNT 10

enum f35_error_code {
	SUCCESS = 0,
	UNKNOWN_FLASH_PRESENT,
	MAGIC_NUMBER_NOT_PRESENT,
	INVALID_BLOCK_NUMBER,
	BLOCK_NOT_ERASED,
	NO_FLASH_PRESENT,
	CHECKSUM_FAILURE,
	WRITE_FAILURE,
	INVALID_COMMAND,
	IN_DEBUG_MODE,
	INVALID_HEADER,
	REQUESTING_FIRMWARE,
	INVALID_CONFIGURATION,
	DISABLE_BLOCK_PROTECT_FAILURE,
};

enum config_download {
	HDL_INVALID = 0,
	HDL_TOUCH_CONFIG,
	HDL_DISPLAY_CONFIG,
	HDL_DISPLAY_CONFIG_TO_RAM,
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

struct rmi_f35_query {
	unsigned char version:4;
	unsigned char has_debug_mode:1;
	unsigned char has_data5:1;
	unsigned char has_query1:1;
	unsigned char has_query2:1;
	unsigned char chunk_size;
	unsigned char has_ctrl7:1;
	unsigned char has_host_download:1;
	unsigned char has_spi_master:1;
	unsigned char advanced_recovery_mode:1;
	unsigned char reserved:4;
} __packed;

struct rmi_f35_data {
	unsigned char error_code:5;
	unsigned char recovery_mode_forced:1;
	unsigned char nvm_programmed:1;
	unsigned char in_recovery:1;
} __packed;

struct rmi_pdt_entry {
	unsigned char query_base_addr;
	unsigned char command_base_addr;
	unsigned char control_base_addr;
	unsigned char data_base_addr;
	unsigned char intr_src_count:3;
	unsigned char reserved_1:2;
	unsigned char fn_version:2;
	unsigned char reserved_2:1;
	unsigned char fn_number;
} __packed;

struct rmi_addr {
	unsigned short query_base;
	unsigned short command_base;
	unsigned short control_base;
	unsigned short data_base;
};

struct firmware_status {
	unsigned short invalid_static_config:1;
	unsigned short need_disp_config:1;
	unsigned short need_app_config:1;
	unsigned short hdl_version:4;
	unsigned short reserved:9;
} __packed;

struct zeroflash_hcd {
	bool has_hdl;
	bool f35_ready;
	const unsigned char *image;
	unsigned char *buf;
	const struct firmware *fw_entry;
	struct work_struct config_work;
	struct work_struct firmware_work;
	struct workqueue_struct *workqueue;
	struct rmi_addr f35_addr;
	struct image_info image_info;
	struct firmware_status fw_status;
	struct syna_tcm_buffer out;
	struct syna_tcm_buffer resp;
	struct syna_tcm_hcd *tcm_hcd;
};

DECLARE_COMPLETION(zeroflash_remove_complete);

static struct zeroflash_hcd *zeroflash_hcd;

static int zeroflash_check_uboot(void)
{
	int retval;
	unsigned char fn_number;
	struct rmi_f35_query query;
	struct rmi_pdt_entry p_entry;
	struct syna_tcm_hcd *tcm_hcd = zeroflash_hcd->tcm_hcd;

	LOG_ENTRY();
	retval = syna_tcm_rmi_read(tcm_hcd,
			PDT_END_ADDR,
			&fn_number,
			sizeof(fn_number));
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to read RMI function number\n");
		return retval;
	}

	LOGD(tcm_hcd->pdev->dev.parent,
			"Found F$%02x\n",
			fn_number);

	if (fn_number != UBL_FN_NUMBER) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to find F$35\n");
		return -ENODEV;
	}

	if (zeroflash_hcd->f35_ready)
		return 0;

	retval = syna_tcm_rmi_read(tcm_hcd,
			PDT_START_ADDR,
			(unsigned char *)&p_entry,
			sizeof(p_entry));
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to read PDT entry\n");
		return retval;
	}

	zeroflash_hcd->f35_addr.query_base = p_entry.query_base_addr;
	zeroflash_hcd->f35_addr.command_base = p_entry.command_base_addr;
	zeroflash_hcd->f35_addr.control_base = p_entry.control_base_addr;
	zeroflash_hcd->f35_addr.data_base = p_entry.data_base_addr;

	retval = syna_tcm_rmi_read(tcm_hcd,
			zeroflash_hcd->f35_addr.query_base,
			(unsigned char *)&query,
			sizeof(query));
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to read F$35 query\n");
		return retval;
	}

	zeroflash_hcd->f35_ready = true;

	if (query.has_query2 && query.has_ctrl7 && query.has_host_download) {
		zeroflash_hcd->has_hdl = true;
	} else {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Host download not supported\n");
		zeroflash_hcd->has_hdl = false;
		return -ENODEV;
	}

	LOG_DONE();
	return 0;
}

static int zeroflash_parse_fw_image(void)
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
	struct syna_tcm_hcd *tcm_hcd = zeroflash_hcd->tcm_hcd;
	const unsigned char *image;
	const unsigned char *content;

	LOG_ENTRY();
	image = zeroflash_hcd->image;
	image_info = &zeroflash_hcd->image_info;
	header = (struct image_header *)image;

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

		if (0 == strncmp((char *)descriptor->id_string,
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
		} else if (0 == strncmp((char *)descriptor->id_string,
					F35_APP_CODE_ID,
					strlen(F35_APP_CODE_ID))) {
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
		} else if (0 == strncmp((char *)descriptor->id_string,
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
			image_info->packrat_number = le4_to_uint(&content[14]);
			LOGD(tcm_hcd->pdev->dev.parent,
					"Application config size = %d\n",
					length);
			LOGD(tcm_hcd->pdev->dev.parent,
					"Application config flash address = 0x%08x\n",
					flash_addr);
		} else if (0 == strncmp((char *)descriptor->id_string,
					DISP_CONFIG_ID,
					strlen(DISP_CONFIG_ID))) {
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

	LOG_DONE();
	return 0;
}

static int zeroflash_get_fw_image(void)
{
	int retval;
	struct syna_tcm_hcd *tcm_hcd = zeroflash_hcd->tcm_hcd;

	LOG_ENTRY();
	if (zeroflash_hcd->fw_entry != NULL)
		return 0;

	do {
		retval = request_firmware(&zeroflash_hcd->fw_entry,
				FW_IMAGE_NAME,
				tcm_hcd->pdev->dev.parent);
		if (retval < 0) {
			LOGD(tcm_hcd->pdev->dev.parent,
					"Failed to request %s\n",
					FW_IMAGE_NAME);
			msleep(100);
		} else {
			break;
		}
	} while (1);

	LOGD(tcm_hcd->pdev->dev.parent,
			"Firmware image size = %d\n",
			(unsigned int)zeroflash_hcd->fw_entry->size);

	zeroflash_hcd->image = zeroflash_hcd->fw_entry->data;

	retval = zeroflash_parse_fw_image();
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to parse firmware image\n");
		release_firmware(zeroflash_hcd->fw_entry);
		zeroflash_hcd->fw_entry = NULL;
		zeroflash_hcd->image = NULL;
		return retval;
	}

	LOG_DONE();
	return 0;
}

static void zeroflash_download_config(void)
{
	struct firmware_status *fw_status;
	struct syna_tcm_hcd *tcm_hcd = zeroflash_hcd->tcm_hcd;

	LOG_ENTRY();
	fw_status = &zeroflash_hcd->fw_status;

	if (!fw_status->need_app_config && !fw_status->need_disp_config) {
		if (atomic_read(&tcm_hcd->helper.task) == HELP_NONE) {
			atomic_set(&tcm_hcd->helper.task,
					HELP_SEND_RESET_NOTIFICATION);
			queue_work(tcm_hcd->helper.workqueue,
					&tcm_hcd->helper.work);
		}
		atomic_set(&tcm_hcd->host_downloading, 0);
		wake_up_interruptible(&tcm_hcd->hdl_wq);
		return;
	}

	queue_work(zeroflash_hcd->workqueue, &zeroflash_hcd->config_work);

	LOG_DONE();
	return;
}

static void zeroflash_download_firmware(void)
{
	LOG_ENTRY();
	queue_work(zeroflash_hcd->workqueue, &zeroflash_hcd->firmware_work);
	LOG_DONE();

	return;
}

static int zeroflash_download_disp_config(void)
{
	int retval;
	unsigned char response_code;
	struct image_info *image_info;
	struct syna_tcm_hcd *tcm_hcd = zeroflash_hcd->tcm_hcd;
	static unsigned int retry_count;

	LOG_ENTRY();
	LOGN(tcm_hcd->pdev->dev.parent,
			"Downloading display config\n");

	image_info = &zeroflash_hcd->image_info;

	if (image_info->disp_config.size == 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"No display config in image file\n");
		return -EINVAL;
	}

	LOCK_BUFFER(zeroflash_hcd->out);

	retval = syna_tcm_alloc_mem(tcm_hcd,
			&zeroflash_hcd->out,
			image_info->disp_config.size + 2);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for zeroflash_hcd->out.buf\n");
		goto unlock_out;
	}

	switch (zeroflash_hcd->fw_status.hdl_version) {
	case 0:
		zeroflash_hcd->out.buf[0] = 1;
		zeroflash_hcd->out.buf[1] = HDL_DISPLAY_CONFIG_TO_RAM;
		break;
	case 1:
		zeroflash_hcd->out.buf[0] = 2;
		zeroflash_hcd->out.buf[1] = HDL_DISPLAY_CONFIG;
		break;
	default:
		retval = -EINVAL;
		LOGE(tcm_hcd->pdev->dev.parent,
				"Invalid HDL version (%d)\n",
				zeroflash_hcd->fw_status.hdl_version);
		goto unlock_out;
	}

	retval = secure_memcpy(&zeroflash_hcd->out.buf[2],
			zeroflash_hcd->out.buf_size - 2,
			image_info->disp_config.data,
			image_info->disp_config.size,
			image_info->disp_config.size);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to copy display config data\n");
		goto unlock_out;
	}

	zeroflash_hcd->out.data_length = image_info->disp_config.size + 2;

	LOCK_BUFFER(zeroflash_hcd->resp);

	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_DOWNLOAD_CONFIG,
			zeroflash_hcd->out.buf,
			zeroflash_hcd->out.data_length,
			&zeroflash_hcd->resp.buf,
			&zeroflash_hcd->resp.buf_size,
			&zeroflash_hcd->resp.data_length,
			&response_code,
			0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_DOWNLOAD_CONFIG));
		if (response_code != STATUS_ERROR)
			goto unlock_resp;
		retry_count++;
		if (DOWNLOAD_RETRY_COUNT && retry_count > DOWNLOAD_RETRY_COUNT)
			goto unlock_resp;
	} else {
		retry_count = 0;
	}

	retval = secure_memcpy((unsigned char *)&zeroflash_hcd->fw_status,
			sizeof(zeroflash_hcd->fw_status),
			zeroflash_hcd->resp.buf,
			zeroflash_hcd->resp.buf_size,
			sizeof(zeroflash_hcd->fw_status));
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to copy firmware status\n");
		goto unlock_resp;
	}

	LOGN(tcm_hcd->pdev->dev.parent,
			"Display config downloaded\n");

	retval = 0;

unlock_resp:
	UNLOCK_BUFFER(zeroflash_hcd->resp);

unlock_out:
	UNLOCK_BUFFER(zeroflash_hcd->out);

	LOG_DONE();
	return retval;
}

static int zeroflash_download_app_config(void)
{
	int retval;
	unsigned char padding;
	unsigned char response_code;
	struct image_info *image_info;
	struct syna_tcm_hcd *tcm_hcd = zeroflash_hcd->tcm_hcd;
	static unsigned int retry_count;

	LOG_ENTRY();
	LOGN(tcm_hcd->pdev->dev.parent,
			"Downloading application config\n");

	image_info = &zeroflash_hcd->image_info;

	if (image_info->app_config.size == 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"No application config in image file\n");
		return -EINVAL;
	}

	padding = image_info->app_config.size % 8;
	if (padding)
		padding = 8 - padding;

	LOCK_BUFFER(zeroflash_hcd->out);

	retval = syna_tcm_alloc_mem(tcm_hcd,
			&zeroflash_hcd->out,
			image_info->app_config.size + 2 + padding);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for zeroflash_hcd->out.buf\n");
		goto unlock_out;
	}

	switch (zeroflash_hcd->fw_status.hdl_version) {
	case 0:
		zeroflash_hcd->out.buf[0] = 1;
		break;
	case 1:
		zeroflash_hcd->out.buf[0] = 2;
		break;
	default:
		retval = -EINVAL;
		LOGE(tcm_hcd->pdev->dev.parent,
				"Invalid HDL version (%d)\n",
				zeroflash_hcd->fw_status.hdl_version);
		goto unlock_out;
	}

	zeroflash_hcd->out.buf[1] = HDL_TOUCH_CONFIG;

	retval = secure_memcpy(&zeroflash_hcd->out.buf[2],
			zeroflash_hcd->out.buf_size - 2,
			image_info->app_config.data,
			image_info->app_config.size,
			image_info->app_config.size);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to copy application config data\n");
		goto unlock_out;
	}

	zeroflash_hcd->out.data_length = image_info->app_config.size + 2;
	zeroflash_hcd->out.data_length += padding;

	LOCK_BUFFER(zeroflash_hcd->resp);

	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_DOWNLOAD_CONFIG,
			zeroflash_hcd->out.buf,
			zeroflash_hcd->out.data_length,
			&zeroflash_hcd->resp.buf,
			&zeroflash_hcd->resp.buf_size,
			&zeroflash_hcd->resp.data_length,
			&response_code,
			0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_DOWNLOAD_CONFIG));
		if (response_code != STATUS_ERROR)
			goto unlock_resp;
		retry_count++;
		if (DOWNLOAD_RETRY_COUNT && retry_count > DOWNLOAD_RETRY_COUNT)
			goto unlock_resp;
	} else {
		retry_count = 0;
	}

	retval = secure_memcpy((unsigned char *)&zeroflash_hcd->fw_status,
			sizeof(zeroflash_hcd->fw_status),
			zeroflash_hcd->resp.buf,
			zeroflash_hcd->resp.buf_size,
			sizeof(zeroflash_hcd->fw_status));
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to copy firmware status\n");
		goto unlock_resp;
	}

	LOGN(tcm_hcd->pdev->dev.parent,
			"Application config downloaded\n");

	retval = 0;

unlock_resp:
	UNLOCK_BUFFER(zeroflash_hcd->resp);

unlock_out:
	UNLOCK_BUFFER(zeroflash_hcd->out);

	LOG_DONE();
	return retval;
}

static void zeroflash_download_config_work(struct work_struct *work)
{
	int retval;
	struct syna_tcm_hcd *tcm_hcd = zeroflash_hcd->tcm_hcd;

	LOG_ENTRY();
	retval = zeroflash_get_fw_image();
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to get firmware image\n");
		return;
	}

	LOGN(tcm_hcd->pdev->dev.parent,
			"Start of config download\n");

	if (zeroflash_hcd->fw_status.need_app_config) {
		retval = zeroflash_download_app_config();
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to download application config\n");
			return;
		}
		goto exit;
	}

	if (zeroflash_hcd->fw_status.need_disp_config) {
		retval = zeroflash_download_disp_config();
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to download display config\n");
			return;
		}
		goto exit;
	}

exit:
	LOGN(tcm_hcd->pdev->dev.parent,
			"End of config download\n");

	zeroflash_download_config();

	LOG_DONE();
	return;
}

static int zeroflash_download_app_fw(void)
{
	int retval;
	unsigned char command;
	struct image_info *image_info;
	struct syna_tcm_hcd *tcm_hcd = zeroflash_hcd->tcm_hcd;
#if RESET_TO_HDL_DELAY_MS
	const struct syna_tcm_board_data *bdata = tcm_hcd->hw_if->bdata;
#endif

	LOG_ENTRY();
	LOGN(tcm_hcd->pdev->dev.parent,
			"Downloading application firmware\n");

	image_info = &zeroflash_hcd->image_info;

	if (image_info->app_firmware.size == 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"No application firmware in image file\n");
		return -EINVAL;
	}

	LOCK_BUFFER(zeroflash_hcd->out);

	retval = syna_tcm_alloc_mem(tcm_hcd,
			&zeroflash_hcd->out,
			image_info->app_firmware.size);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for zeroflash_hcd->out.buf\n");
		UNLOCK_BUFFER(zeroflash_hcd->out);
		return retval;
	}

	retval = secure_memcpy(zeroflash_hcd->out.buf,
			zeroflash_hcd->out.buf_size,
			image_info->app_firmware.data,
			image_info->app_firmware.size,
			image_info->app_firmware.size);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to copy application firmware data\n");
		UNLOCK_BUFFER(zeroflash_hcd->out);
		return retval;
	}

	zeroflash_hcd->out.data_length = image_info->app_firmware.size;

	command = F35_WRITE_FW_TO_PMEM_COMMAND;

#if RESET_TO_HDL_DELAY_MS
	gpio_set_value(bdata->reset_gpio, bdata->reset_on_state);
	msleep(bdata->reset_active_ms);
	gpio_set_value(bdata->reset_gpio, !bdata->reset_on_state);
	mdelay(RESET_TO_HDL_DELAY_MS);
#endif

	retval = syna_tcm_rmi_write(tcm_hcd,
			zeroflash_hcd->f35_addr.control_base + F35_CTRL3_OFFSET,
			&command,
			sizeof(command));
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write F$35 command\n");
		UNLOCK_BUFFER(zeroflash_hcd->out);
		return retval;
	}

	retval = syna_tcm_rmi_write(tcm_hcd,
			zeroflash_hcd->f35_addr.control_base + F35_CTRL7_OFFSET,
			zeroflash_hcd->out.buf,
			zeroflash_hcd->out.data_length);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write application firmware data\n");
		UNLOCK_BUFFER(zeroflash_hcd->out);
		return retval;
	}

	UNLOCK_BUFFER(zeroflash_hcd->out);

	LOGN(tcm_hcd->pdev->dev.parent,
			"Application firmware downloaded\n");

	LOG_DONE();
	return 0;
}

static void zeroflash_download_firmware_work(struct work_struct *work)
{
	int retval;
	struct rmi_f35_data data;
	struct syna_tcm_hcd *tcm_hcd = zeroflash_hcd->tcm_hcd;
	static unsigned int retry_count;

	LOG_ENTRY();
	retval = zeroflash_check_uboot();
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Microbootloader support unavailable\n");
		goto exit;
	}

	atomic_set(&tcm_hcd->host_downloading, 1);

	retval = syna_tcm_rmi_read(tcm_hcd,
			zeroflash_hcd->f35_addr.data_base,
			(unsigned char *)&data,
			sizeof(data));
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to read F$35 data\n");
		goto exit;
	}

	if (data.error_code != REQUESTING_FIRMWARE) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Microbootloader error code = 0x%02x\n",
				data.error_code);
		if (data.error_code != CHECKSUM_FAILURE) {
			retval = -EIO;
			goto exit;
		} else {
			retry_count++;
		}
	} else {
		retry_count = 0;
	}

	retval = zeroflash_get_fw_image();
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to get firmware image\n");
		goto exit;
	}

	LOGN(tcm_hcd->pdev->dev.parent,
			"Start of firmware download\n");

	retval = zeroflash_download_app_fw();
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to download application firmware\n");
		goto exit;
	}

	LOGN(tcm_hcd->pdev->dev.parent,
			"End of firmware download\n");

exit:
	if (retval < 0)
		retry_count++;

	if (DOWNLOAD_RETRY_COUNT && retry_count > DOWNLOAD_RETRY_COUNT) {
		retval = tcm_hcd->enable_irq(tcm_hcd, false, true);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to disable interrupt\n");
		}
	} else {
		retval = tcm_hcd->enable_irq(tcm_hcd, true, NULL);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to enable interrupt\n");
		}
	}

	LOG_DONE();
	return;
}

static int zeroflash_init(struct syna_tcm_hcd *tcm_hcd)
{
	LOG_ENTRY();
	zeroflash_hcd = kzalloc(sizeof(*zeroflash_hcd), GFP_KERNEL);
	if (!zeroflash_hcd) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for zeroflash_hcd\n");
		return -ENOMEM;
	}

	zeroflash_hcd->tcm_hcd = tcm_hcd;

	INIT_BUFFER(zeroflash_hcd->out, false);
	INIT_BUFFER(zeroflash_hcd->resp, false);

	zeroflash_hcd->workqueue =
		create_singlethread_workqueue("syna_tcm_zeroflash");
	INIT_WORK(&zeroflash_hcd->config_work,
			zeroflash_download_config_work);
	INIT_WORK(&zeroflash_hcd->firmware_work,
			zeroflash_download_firmware_work);

	if (tcm_hcd->init_okay == false &&
			tcm_hcd->hw_if->bus_io->type == BUS_SPI)
		zeroflash_download_firmware();

	LOG_DONE();
	return 0;
}

static int zeroflash_remove(struct syna_tcm_hcd *tcm_hcd)
{
	LOG_ENTRY();
	if (!zeroflash_hcd)
		goto exit;

	if (zeroflash_hcd->fw_entry)
		release_firmware(zeroflash_hcd->fw_entry);

	cancel_work_sync(&zeroflash_hcd->config_work);
	cancel_work_sync(&zeroflash_hcd->firmware_work);
	flush_workqueue(zeroflash_hcd->workqueue);
	destroy_workqueue(zeroflash_hcd->workqueue);

	RELEASE_BUFFER(zeroflash_hcd->resp);
	RELEASE_BUFFER(zeroflash_hcd->out);

	kfree(zeroflash_hcd);
	zeroflash_hcd = NULL;

exit:
	complete(&zeroflash_remove_complete);

	LOG_DONE();
	return 0;
}

static int zeroflash_syncbox(struct syna_tcm_hcd *tcm_hcd)
{
	int retval;
	unsigned char *fw_status;

	LOG_ENTRY();
	if (!zeroflash_hcd)
		return 0;

	switch (tcm_hcd->report.id) {
	case REPORT_STATUS:
		fw_status = (unsigned char *)&zeroflash_hcd->fw_status;
		retval = secure_memcpy(fw_status,
				sizeof(zeroflash_hcd->fw_status),
				tcm_hcd->report.buffer.buf,
				tcm_hcd->report.buffer.buf_size,
				sizeof(zeroflash_hcd->fw_status));
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to copy firmware status\n");
			return retval;
		}
		zeroflash_download_config();
		break;
	case REPORT_HDL:
		retval = tcm_hcd->enable_irq(tcm_hcd, false, true);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to disable interrupt\n");
			return retval;
		}
		zeroflash_download_firmware();
		break;
	default:
		break;
	}

	LOG_DONE();
	return 0;
}

static int zeroflash_reset(struct syna_tcm_hcd *tcm_hcd)
{
	int retval;

	LOG_ENTRY();
	if (!zeroflash_hcd) {
		retval = zeroflash_init(tcm_hcd);
		return retval;
	}

	LOG_DONE();
	return 0;
}

static struct syna_tcm_module_cb zeroflash_module = {
	.type = TCM_ZEROFLASH,
	.init = zeroflash_init,
	.remove = zeroflash_remove,
	.syncbox = zeroflash_syncbox,
	.asyncbox = NULL,
	.reset = zeroflash_reset,
	.suspend = NULL,
	.resume = NULL,
	.early_suspend = NULL,
};

static int __init zeroflash_module_init(void)
{
	int retval;
	LOG_ENTRY();
	/* add check F7A LCM by wanghan start */
	if(!lct_syna_verify_flag)
		return -ENODEV;
	/* add check F7A LCM by wanghan end */
	LOGV("__init zeroflash module\n");
	retval = syna_tcm_add_module(&zeroflash_module, true);
	if(retval) {
		LOGV("syna_tcm_add_module failed! retval = %d\n", retval);
	}
	LOG_DONE();
	return retval;
}

static void __exit zeroflash_module_exit(void)
{
	LOG_ENTRY();
	syna_tcm_add_module(&zeroflash_module, false);

	wait_for_completion(&zeroflash_remove_complete);

	LOG_DONE();
	return;
}

module_init(zeroflash_module_init);
module_exit(zeroflash_module_exit);

MODULE_AUTHOR("Synaptics, Inc.");
MODULE_DESCRIPTION("Synaptics TCM Zeroflash Module");
MODULE_LICENSE("GPL v2");
