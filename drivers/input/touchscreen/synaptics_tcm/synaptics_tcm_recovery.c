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

#include "synaptics_tcm_core.h"

#define SET_UP_RECOVERY_MODE true

#define ENABLE_SYSFS_INTERFACE true

#define SYSFS_DIR_NAME "recovery"

#define IHEX_BUF_SIZE (2048 * 1024)

#define DATA_BUF_SIZE (512 * 1024)

#define IHEX_RECORD_SIZE 14

#define PDT_START_ADDR 0x00e9

#define UBL_FN_NUMBER 0x35

#define F35_CHUNK_SIZE 16

#define F35_CHUNK_SIZE_WORDS 8

#define F35_ERASE_ALL_WAIT_MS 5000

#define F35_ERASE_ALL_POLL_MS 100

#define F35_DATA5_OFFSET 5

#define F35_CTRL3_OFFSET 18

#define F35_RESET_COMMAND 16

#define F35_ERASE_ALL_COMMAND 3

#define F35_WRITE_CHUNK_COMMAND 2

#define F35_READ_FLASH_STATUS_COMMAND 1

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

struct recovery_hcd {
	bool set_up_recovery_mode;
	unsigned char chunk_buf[F35_CHUNK_SIZE + 3];
	unsigned char out_buf[3];
	unsigned char *ihex_buf;
	unsigned char *data_buf;
	unsigned int ihex_size;
	unsigned int ihex_records;
	unsigned int data_entries;
	struct kobject *sysfs_dir;
	struct rmi_addr f35_addr;
	struct syna_tcm_hcd *tcm_hcd;
};

DECLARE_COMPLETION(recovery_remove_complete);

static struct recovery_hcd *recovery_hcd;

static int recovery_do_recovery(void);

STORE_PROTOTYPE(recovery, recovery);

static struct device_attribute *attrs[] = {
	ATTRIFY(recovery),
};

static ssize_t recovery_sysfs_ihex_store(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count);

static struct bin_attribute bin_attr = {
	.attr = {
		.name = "ihex",
		.mode = 0220,
	},
	.size = 0,
	.write = recovery_sysfs_ihex_store,
};

static ssize_t recovery_sysfs_recovery_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned int input;
	struct syna_tcm_hcd *tcm_hcd = recovery_hcd->tcm_hcd;

	if (kstrtouint(buf, 10, &input))
		return -EINVAL;

	if (input == 1)
		recovery_hcd->set_up_recovery_mode = true;
	else if (input == 2)
		recovery_hcd->set_up_recovery_mode = false;
	else
		return -EINVAL;

	mutex_lock(&tcm_hcd->extif_mutex);

	if (recovery_hcd->ihex_size == 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to get ihex data\n");
		retval = -EINVAL;
		goto exit;
	}

	if (recovery_hcd->ihex_size % IHEX_RECORD_SIZE) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Invalid ihex data\n");
		retval = -EINVAL;
		goto exit;
	}

	recovery_hcd->ihex_records = recovery_hcd->ihex_size / IHEX_RECORD_SIZE;

	retval = recovery_do_recovery();
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to do recovery\n");
		goto exit;
	}

	retval = count;

exit:
	recovery_hcd->set_up_recovery_mode = SET_UP_RECOVERY_MODE;

	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}

static ssize_t recovery_sysfs_ihex_store(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count)
{
	int retval;
	struct syna_tcm_hcd *tcm_hcd = recovery_hcd->tcm_hcd;

	mutex_lock(&tcm_hcd->extif_mutex);

	retval = secure_memcpy(&recovery_hcd->ihex_buf[pos],
			IHEX_BUF_SIZE - pos,
			buf,
			count,
			count);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to copy ihex data\n");
		recovery_hcd->ihex_size = 0;
		goto exit;
	}

	recovery_hcd->ihex_size = pos + count;

	retval = count;

exit:
	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}

static int recovery_device_reset(void)
{
	int retval;
	unsigned char command;
	struct syna_tcm_hcd *tcm_hcd = recovery_hcd->tcm_hcd;
	const struct syna_tcm_board_data *bdata = tcm_hcd->hw_if->bdata;

	command = F35_RESET_COMMAND;

	retval = syna_tcm_rmi_write(tcm_hcd,
			recovery_hcd->f35_addr.control_base + F35_CTRL3_OFFSET,
			&command,
			sizeof(command));
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write F$35 command\n");
		return retval;
	}

	msleep(bdata->reset_delay_ms);

	return 0;
}

static int recovery_add_data_entry(unsigned char data)
{
	struct syna_tcm_hcd *tcm_hcd = recovery_hcd->tcm_hcd;

	if (recovery_hcd->data_entries >= DATA_BUF_SIZE) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Reached data buffer size limit\n");
		return -EINVAL;
	}

	recovery_hcd->data_buf[recovery_hcd->data_entries++] = data;

	return 0;
}

static int recovery_add_padding(unsigned int *words)
{
	int retval;
	unsigned int padding;
	struct syna_tcm_hcd *tcm_hcd = recovery_hcd->tcm_hcd;

	padding = (F35_CHUNK_SIZE_WORDS - *words % F35_CHUNK_SIZE_WORDS);
	padding %= F35_CHUNK_SIZE_WORDS;

	while (padding) {
		retval = recovery_add_data_entry(0xff);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to add data entry\n");
			return retval;
		}

		retval = recovery_add_data_entry(0xff);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to add data entry\n");
			return retval;
		}

		(*words)++;
		padding--;
	}

	return 0;
}

static int recovery_parse_ihex(void)
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
	struct syna_tcm_hcd *tcm_hcd = recovery_hcd->tcm_hcd;

	words = 0;

	offset = 0;

	buf = recovery_hcd->ihex_buf;

	recovery_hcd->data_entries = 0;

	for (record = 0; record < recovery_hcd->ihex_records; record++) {
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
			if ((words % F35_CHUNK_SIZE_WORDS) == 0) {
				addr = (addrh << 8) + addrl;
				addr += offset;
				addr >>= 4;

				retval = recovery_add_data_entry(addr);
				if (retval < 0) {
					LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to add data entry\n");
					return retval;
				}

				retval = recovery_add_data_entry(addr >> 8);
				if (retval < 0) {
					LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to add data entry\n");
					return retval;
				}
			}

			retval = recovery_add_data_entry(data0);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to add data entry\n");
				return retval;
			}

			retval = recovery_add_data_entry(data1);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to add data entry\n");
				return retval;
			}

			words++;
		} else if (type == 0x02) {
			retval = recovery_add_padding(&words);
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
						"Failed to add padding\n");
				return retval;
			}

			offset = (data0 << 8) + data1;
			offset <<= 4;
		}
	}

	retval = recovery_add_padding(&words);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to add padding\n");
		return retval;
	}

	return 0;
}

static int recovery_check_status(void)
{
	int retval;
	unsigned char status;
	struct syna_tcm_hcd *tcm_hcd = recovery_hcd->tcm_hcd;

	retval = syna_tcm_rmi_read(tcm_hcd,
			recovery_hcd->f35_addr.data_base,
			&status,
			sizeof(status));
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to read status\n");
		return retval;
	}

	status = status & 0x1f;

	if (status != 0x00) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Recovery mode status = 0x%02x\n",
				status);
		return -EINVAL;
	}

	return 0;
}

static int recovery_write_flash(void)
{
	int retval;
	unsigned char *data_ptr;
	unsigned int chunk_buf_size;
	unsigned int chunk_data_size;
	unsigned int entries_written;
	unsigned int entries_to_write;
	struct syna_tcm_hcd *tcm_hcd = recovery_hcd->tcm_hcd;

	entries_written = 0;

	data_ptr = recovery_hcd->data_buf;

	chunk_buf_size = sizeof(recovery_hcd->chunk_buf);

	chunk_data_size = chunk_buf_size - 1;

	recovery_hcd->chunk_buf[chunk_buf_size - 1] = F35_WRITE_CHUNK_COMMAND;

	while (entries_written < recovery_hcd->data_entries) {
		entries_to_write = F35_CHUNK_SIZE + 2;

		retval = secure_memcpy(recovery_hcd->chunk_buf,
				chunk_buf_size - 1,
				data_ptr,
				recovery_hcd->data_entries - entries_written,
				entries_to_write);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to copy chunk data\n");
			return retval;
		}

		retval = syna_tcm_rmi_write(tcm_hcd,
				recovery_hcd->f35_addr.control_base,
				recovery_hcd->chunk_buf,
				chunk_buf_size);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to write chunk data\n");
			return retval;
		}

		data_ptr += entries_to_write;
		entries_written += entries_to_write;
	}

	retval = recovery_check_status();
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
			"Failed to get no error recovery mode status\n");
		return retval;
	}

	return 0;
}

static int recovery_poll_erase_completion(void)
{
	int retval;
	unsigned char status;
	unsigned char command;
	unsigned char data_base;
	unsigned int timeout;
	struct syna_tcm_hcd *tcm_hcd = recovery_hcd->tcm_hcd;

	timeout = F35_ERASE_ALL_WAIT_MS;

	data_base = recovery_hcd->f35_addr.data_base;

	do {
		command = F35_READ_FLASH_STATUS_COMMAND;

		retval = syna_tcm_rmi_write(tcm_hcd,
				recovery_hcd->f35_addr.command_base,
				&command,
				sizeof(command));
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to write F$35 command\n");
			return retval;
		}

		do {
			retval = syna_tcm_rmi_read(tcm_hcd,
					recovery_hcd->f35_addr.command_base,
					&command,
					sizeof(command));
			if (retval < 0) {
				LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to read command status\n");
				return retval;
			}

			if (command == 0x00)
				break;

			if (timeout == 0)
				break;

			msleep(F35_ERASE_ALL_POLL_MS);
			timeout -= F35_ERASE_ALL_POLL_MS;
		} while (true);

		if (command != 0 && timeout == 0) {
			retval = -EINVAL;
			goto exit;
		}

		retval = syna_tcm_rmi_read(tcm_hcd,
				data_base + F35_DATA5_OFFSET,
				&status,
				sizeof(status));
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to read flash status\n");
			return retval;
		}

		if ((status & 0x01) == 0x00)
			break;

		if (timeout == 0) {
			retval = -EINVAL;
			goto exit;
		}

		msleep(F35_ERASE_ALL_POLL_MS);
		timeout -= F35_ERASE_ALL_POLL_MS;
	} while (true);

	retval = 0;

exit:
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to get erase completion\n");
	}

	return retval;
}

static int recovery_erase_flash(void)
{
	int retval;
	unsigned char command;
	struct syna_tcm_hcd *tcm_hcd = recovery_hcd->tcm_hcd;

	command = F35_ERASE_ALL_COMMAND;

	retval = syna_tcm_rmi_write(tcm_hcd,
			recovery_hcd->f35_addr.control_base + F35_CTRL3_OFFSET,
			&command,
			sizeof(command));
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write F$35 command\n");
		return retval;
	}

	if (recovery_hcd->f35_addr.command_base) {
		retval = recovery_poll_erase_completion();
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to wait for erase completion\n");
			return retval;
		}
	} else {
		msleep(F35_ERASE_ALL_WAIT_MS);
	}

	retval = recovery_check_status();
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
			"Failed to get no error recovery mode status\n");
		return retval;
	}

	return 0;
}

static int recovery_set_up_recovery_mode(void)
{
	int retval;
	struct syna_tcm_hcd *tcm_hcd = recovery_hcd->tcm_hcd;
	const struct syna_tcm_board_data *bdata = tcm_hcd->hw_if->bdata;

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

	retval = tcm_hcd->write_message(tcm_hcd,
			recovery_hcd->out_buf[0],
			&recovery_hcd->out_buf[1],
			2,
			NULL,
			NULL,
			NULL,
			NULL,
			0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_REBOOT_TO_ROM_BOOTLOADER));
		return retval;
	}

	msleep(bdata->reset_delay_ms);

	return 0;
}

static int recovery_do_recovery(void)
{
	int retval;
	struct rmi_pdt_entry p_entry;
	struct syna_tcm_hcd *tcm_hcd = recovery_hcd->tcm_hcd;

	retval = recovery_parse_ihex();
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to parse ihex data\n");
		return retval;
	}

	if (recovery_hcd->set_up_recovery_mode) {
		retval = recovery_set_up_recovery_mode();
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to set up recovery mode\n");
			return retval;
		}
	}

	tcm_hcd->update_watchdog(tcm_hcd, false);

	retval = syna_tcm_rmi_read(tcm_hcd,
			PDT_START_ADDR,
			(unsigned char *)&p_entry,
			sizeof(p_entry));
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to read PDT entry\n");
		return retval;
	}

	if (p_entry.fn_number != UBL_FN_NUMBER) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to find F$35\n");
		return -ENODEV;
	}

	recovery_hcd->f35_addr.query_base = p_entry.query_base_addr;
	recovery_hcd->f35_addr.command_base = p_entry.command_base_addr;
	recovery_hcd->f35_addr.control_base = p_entry.control_base_addr;
	recovery_hcd->f35_addr.data_base = p_entry.data_base_addr;

	LOGN(tcm_hcd->pdev->dev.parent,
			"Start of recovery\n");

	retval = recovery_erase_flash();
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to erase flash\n");
		return retval;
	}

	LOGN(tcm_hcd->pdev->dev.parent,
			"Flash erased\n");

	retval = recovery_write_flash();
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write to flash\n");
		return retval;
	}

	LOGN(tcm_hcd->pdev->dev.parent,
			"Flash written\n");

	retval = recovery_device_reset();
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to do reset\n");
		return retval;
	}

	LOGN(tcm_hcd->pdev->dev.parent,
			"End of recovery\n");

	if (recovery_hcd->set_up_recovery_mode)
		return 0;

	tcm_hcd->update_watchdog(tcm_hcd, true);

	retval = tcm_hcd->enable_irq(tcm_hcd, true, NULL);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to enable interrupt\n");
		return retval;
	}

	retval = tcm_hcd->identify(tcm_hcd, true);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to do identification\n");
		return retval;
	}

	if (tcm_hcd->id_info.mode != MODE_APPLICATION) {
		retval = tcm_hcd->switch_mode(tcm_hcd, FW_MODE_APPLICATION);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to run application firmware\n");
			return retval;
		}
	}

	return 0;
}

static int recovery_init(struct syna_tcm_hcd *tcm_hcd)
{
	int retval;
	int idx;

	recovery_hcd = kzalloc(sizeof(*recovery_hcd), GFP_KERNEL);
	if (!recovery_hcd) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for recovery_hcd\n");
		return -ENOMEM;
	}

	recovery_hcd->ihex_buf = kzalloc(IHEX_BUF_SIZE, GFP_KERNEL);
	if (!recovery_hcd->ihex_buf) {
		LOGE(tcm_hcd->pdev->dev.parent,
			"Failed to allocate memory for ihex_buf\n");
		goto err_allocate_ihex_buf;
	}

	recovery_hcd->data_buf = kzalloc(DATA_BUF_SIZE, GFP_KERNEL);
	if (!recovery_hcd->data_buf) {
		LOGE(tcm_hcd->pdev->dev.parent,
			"Failed to allocate memory for data_buf\n");
		goto err_allocate_data_buf;
	}

	recovery_hcd->tcm_hcd = tcm_hcd;

	recovery_hcd->set_up_recovery_mode = SET_UP_RECOVERY_MODE;

	recovery_hcd->out_buf[0] = CMD_REBOOT_TO_ROM_BOOTLOADER;
	recovery_hcd->out_buf[1] = 0;
	recovery_hcd->out_buf[2] = 0;

	if (!ENABLE_SYSFS_INTERFACE)
		return 0;

	recovery_hcd->sysfs_dir = kobject_create_and_add(SYSFS_DIR_NAME,
			tcm_hcd->sysfs_dir);
	if (!recovery_hcd->sysfs_dir) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to create sysfs directory\n");
		retval = -EINVAL;
		goto err_sysfs_create_dir;
	}

	for (idx = 0; idx < ARRAY_SIZE(attrs); idx++) {
		retval = sysfs_create_file(recovery_hcd->sysfs_dir,
				&(*attrs[idx]).attr);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to create sysfs file\n");
			goto err_sysfs_create_file;
		}
	}

	retval = sysfs_create_bin_file(recovery_hcd->sysfs_dir, &bin_attr);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to create sysfs bin file\n");
		goto err_sysfs_create_bin_file;
	}

	return 0;

err_sysfs_create_bin_file:
err_sysfs_create_file:
	for (idx--; idx >= 0; idx--)
		sysfs_remove_file(recovery_hcd->sysfs_dir, &(*attrs[idx]).attr);

	kobject_put(recovery_hcd->sysfs_dir);

err_sysfs_create_dir:
	kfree(recovery_hcd->data_buf);
err_allocate_data_buf:
	kfree(recovery_hcd->ihex_buf);
err_allocate_ihex_buf:
	kfree(recovery_hcd);
	recovery_hcd = NULL;

	return retval;
}

static int recovery_remove(struct syna_tcm_hcd *tcm_hcd)
{
	int idx;

	if (!recovery_hcd)
		goto exit;

	if (ENABLE_SYSFS_INTERFACE) {
		sysfs_remove_bin_file(recovery_hcd->sysfs_dir, &bin_attr);

		for (idx = 0; idx < ARRAY_SIZE(attrs); idx++) {
			sysfs_remove_file(recovery_hcd->sysfs_dir,
					&(*attrs[idx]).attr);
		}

		kobject_put(recovery_hcd->sysfs_dir);
	}

	kfree(recovery_hcd->data_buf);
	kfree(recovery_hcd->ihex_buf);
	kfree(recovery_hcd);
	recovery_hcd = NULL;

exit:
	complete(&recovery_remove_complete);

	return 0;
}

static int recovery_reset(struct syna_tcm_hcd *tcm_hcd)
{
	int retval;

	if (!recovery_hcd) {
		retval = recovery_init(tcm_hcd);
		return retval;
	}

	return 0;
}

static struct syna_tcm_module_cb recovery_module = {
	.type = TCM_RECOVERY,
	.init = recovery_init,
	.remove = recovery_remove,
	.syncbox = NULL,
	.asyncbox = NULL,
	.reset = recovery_reset,
	.suspend = NULL,
	.resume = NULL,
	.early_suspend = NULL,
};

static int __init recovery_module_init(void)
{
	return syna_tcm_add_module(&recovery_module, true);
}

static void __exit recovery_module_exit(void)
{
	syna_tcm_add_module(&recovery_module, false);

	wait_for_completion(&recovery_remove_complete);
}

module_init(recovery_module_init);
module_exit(recovery_module_exit);

MODULE_AUTHOR("Synaptics, Inc.");
MODULE_DESCRIPTION("Synaptics TCM Recovery Module");
MODULE_LICENSE("GPL v2");
