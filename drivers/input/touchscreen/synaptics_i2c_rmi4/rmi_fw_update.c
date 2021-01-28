// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2012 Synaptics Incorporated.
 */

#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
/* #include <linux/input/synaptics_dsx.h> */
#include "SynaImage.h"
#include "include/synaptics_dsx_rmi4_i2c.h"
#include "tpd.h"

#define DEBUG_FW_UPDATE
#define SHOW_PROGRESS
#define FW_IMAGE_NAME "SynaImage.h"

#define CHECKSUM_OFFSET 0x00
#define BOOTLOADER_VERSION_OFFSET 0x07
#define IMAGE_SIZE_OFFSET 0x08
#define CONFIG_SIZE_OFFSET 0x0C
#define PRODUCT_ID_OFFSET 0x10
#define PRODUCT_INFO_OFFSET 0x1E
#define FW_IMAGE_OFFSET 0x100
#define PRODUCT_ID_SIZE 10

#define BOOTLOADER_ID_OFFSET 0
#define FLASH_PROPERTIES_OFFSET 2
#define BLOCK_SIZE_OFFSET 3
#define FW_BLOCK_COUNT_OFFSET 5

#define SYN_I2C_RETRY_TIMES 10
#define F01_STD_QUERY_LEN 21

#define REG_MAP (1 << 0)
#define UNLOCKED (1 << 1)
#define HAS_CONFIG_ID (1 << 2)
#define HAS_PERM_CONFIG (1 << 3)
#define HAS_BL_CONFIG (1 << 4)
#define HAS_DISP_CONFIG (1 << 5)
#define HAS_CTRL1 (1 << 6)

#define BLOCK_NUMBER_OFFSET 0
#define BLOCK_DATA_OFFSET 2

#define UI_CONFIG_AREA 0x00
#define PERM_CONFIG_AREA 0x01
#define BL_CONFIG_AREA 0x02
#define DISP_CONFIG_AREA 0x03
#define SENSOR_ID_VALID (1 << 31)

enum flash_command {
	CMD_WRITE_FW_BLOCK = 0x2,
	CMD_ERASE_ALL = 0x3,
	CMD_READ_CONFIG_BLOCK = 0x5,
	CMD_WRITE_CONFIG_BLOCK = 0x6,
	CMD_ERASE_CONFIG = 0x7,
	CMD_READ_SENSOR_ID = 0x8,
	CMD_ERASE_BL_CONFIG = 0x9,
	CMD_ERASE_DISP_CONFIG = 0xA,
	CMD_ENABLE_FLASH_PROG = 0xF,
};

enum glass_vendor {
	LENSON = 0,
	TPK = 1,
};

#define SLEEP_MODE_NORMAL (0x00)
#define SLEEP_MODE_SENSOR_SLEEP (0x01)
#define SLEEP_MODE_RESERVED0 (0x02)
#define SLEEP_MODE_RESERVED1 (0x03)

#define ENABLE_WAIT_MS (1 * 1000)
#define WRITE_WAIT_MS (3 * 1000)
#define ERASE_WAIT_MS (5 * 1000)

#define MIN_SLEEP_TIME_US 50
#define MAX_SLEEP_TIME_US 100

struct kobject *properties_kobj_fwupdate;

static ssize_t fwu_sysfs_show_image(struct file *data_file,
				    struct kobject *kobj,
				    struct bin_attribute *attributes, char *buf,
				    loff_t pos, size_t count);

static ssize_t fwu_sysfs_store_image(struct file *data_file,
				     struct kobject *kobj,
				     struct bin_attribute *attributes,
				     char *buf, loff_t pos, size_t count);

static int fwu_wait_for_idle(int timeout_ms);

struct image_header {
	unsigned int checksum;
	unsigned int image_size;
	unsigned int config_size;
	unsigned char options;
	unsigned char bootloader_version;
	unsigned char product_id[SYNAPTICS_RMI4_PRODUCT_ID_LENGTH + 1];
	unsigned char product_info[SYNAPTICS_RMI4_PRODUCT_INFO_SIZE];
};

struct pdt_properties {
	union {
		struct {
			unsigned char reserved_1 : 6;
			unsigned char has_bsr : 1;
			unsigned char reserved_2 : 1;
		} __packed;
		unsigned char data[1];
	};
};

struct f01_device_status {
	union {
		struct {
			unsigned char status_code : 4;
			unsigned char reserved : 2;
			unsigned char flash_prog : 1;
			unsigned char unconfigured : 1;
		} __packed;
		unsigned char data[1];
	};
};

struct f01_device_control {
	union {
		struct {
			unsigned char sleep_mode : 2;
			unsigned char nosleep : 1;
			unsigned char reserved : 2;
			unsigned char charger_connected : 1;
			unsigned char report_rate : 1;
			unsigned char configured : 1;
		} __packed;
		unsigned char data[1];
	};
};

struct f34_flash_control {
	union {
		struct {
			unsigned char command : 4;
			unsigned char status : 3;
			unsigned char program_enabled : 1;
		} __packed;
		unsigned char data[1];
	};
};

struct f34_flash_properties {
	union {
		struct {
			unsigned char regmap : 1;
			unsigned char unlocked : 1;
			unsigned char has_configid : 1;
			unsigned char has_perm_config : 1;
			unsigned char has_bl_config : 1;
			unsigned char has_display_config : 1;
			unsigned char has_blob_config : 1;
			unsigned char reserved : 1;
		} __packed;
		unsigned char data[1];
	};
};

struct synaptics_rmi4_fwu_handle {
	bool initialized;
	char product_id[SYNAPTICS_RMI4_PRODUCT_ID_LENGTH + 1];
	unsigned int image_size;
	unsigned int data_pos;
	unsigned int sensor_id;
	unsigned char intr_mask;
	unsigned char bootloader_id[2];
	unsigned char productinfo1;
	unsigned char productinfo2;
	unsigned char *ext_data_source;
	unsigned char *read_config_buf;
	const unsigned char *firmware_data;
	const unsigned char *config_data;
	unsigned short block_size;
	unsigned short fw_block_count;
	unsigned short config_block_count;
	unsigned short perm_config_block_count;
	unsigned short bl_config_block_count;
	unsigned short disp_config_block_count;
	unsigned short config_size;
	unsigned short config_area;
	unsigned short addr_f34_flash_control;
	unsigned short addr_f01_interrupt_register;
	struct synaptics_rmi4_fn_desc f01_fd;
	struct synaptics_rmi4_fn_desc f34_fd;
	struct synaptics_rmi4_exp_fn_ptr *fn_ptr;
	struct synaptics_rmi4_data *rmi4_data;
	struct f34_flash_control flash_control;
	struct f34_flash_properties flash_properties;
};

static struct bin_attribute dev_attr_data = {
	.attr = {

			.name = "data", .mode = (0644),
		},
	.size = 0,
	.read = fwu_sysfs_show_image,
	.write = fwu_sysfs_store_image,
};

static struct device_attribute attrs[] = {
/*
 *	__ATTR(doreflash, (0444), synaptics_rmi4_show_error,
 *	       fwu_sysfs_do_reflash_store),
 *
 *	__ATTR(writeconfig, (0444), synaptics_rmi4_show_error,
 *	       fwu_sysfs_write_config_store),
 *	__ATTR(readconfig, (0444), synaptics_rmi4_show_error,
 *	       fwu_sysfs_read_config_store),
 *
 *	__ATTR(configarea, 0222,
 *	       synaptics_rmi4_show_error,
 *	       fwu_sysfs_config_area_store),
 *	__ATTR(imagesize, 0222,
 *	       synaptics_rmi4_show_error,
 *	       fwu_sysfs_image_size_store),
 *
 *	__ATTR(blocksize, 0444, fwu_sysfs_block_size_show,
 *	       synaptics_rmi4_store_error),
 *	__ATTR(fwblockcount, 0444, fwu_sysfs_firmware_block_count_show,
 *	       synaptics_rmi4_store_error),
 *	__ATTR(configblockcount, 0444,
 *	       fwu_sysfs_configuration_block_count_show,
 *	       synaptics_rmi4_store_error),
 *	__ATTR(permconfigblockcount, 0444,
 *	       fwu_sysfs_perm_config_block_count_show,
 *	       synaptics_rmi4_store_error),
 *	__ATTR(blconfigblockcount, 0444,
 *	       fwu_sysfs_bl_config_block_count_show,
 *	       synaptics_rmi4_store_error),
 *	__ATTR(dispconfigblockcount, 0444,
 *	       fwu_sysfs_disp_config_block_count_show,
 *	       synaptics_rmi4_store_error),
 */
};



static struct synaptics_rmi4_fwu_handle *fwu;

static struct completion remove_complete;

static unsigned int extract_uint(const unsigned char *ptr)
{
	return (unsigned int)ptr[0] + (unsigned int)ptr[1] * 0x100 +
	       (unsigned int)ptr[2] * 0x10000 +
	       (unsigned int)ptr[3] * 0x1000000;
}

static void parse_header(struct image_header *header,
			 const unsigned char *fw_image)
{
	header->checksum = extract_uint(&fw_image[CHECKSUM_OFFSET]);
	header->bootloader_version = fw_image[BOOTLOADER_VERSION_OFFSET];
	header->image_size = extract_uint(&fw_image[IMAGE_SIZE_OFFSET]);
	header->config_size = extract_uint(&fw_image[CONFIG_SIZE_OFFSET]);
	memcpy(header->product_id, &fw_image[PRODUCT_ID_OFFSET],
	       SYNAPTICS_RMI4_PRODUCT_ID_LENGTH);
	header->product_id[SYNAPTICS_RMI4_PRODUCT_ID_LENGTH] = 0;
	memcpy(header->product_info, &fw_image[PRODUCT_INFO_OFFSET],
	       SYNAPTICS_RMI4_PRODUCT_INFO_SIZE);

#ifdef DEBUG_FW_UPDATE
	dev_info(&fwu->rmi4_data->i2c_client->dev,
		 "Firwmare size %d, config size %d\n", header->image_size,
		 header->config_size);
#endif
}

static int fwu_check_version(void)
{
	int retval;
	unsigned char firmware_id[4];
	unsigned char config_id[4];
	struct i2c_client *i2c_client = fwu->rmi4_data->i2c_client;

	/* device firmware id */
	retval = fwu->fn_ptr->read(fwu->rmi4_data,
				   fwu->f01_fd.query_base_addr + 18,
				   firmware_id, sizeof(firmware_id));
	if (retval < 0) {
		TPD_DMESG("Failed to read firmware ID (code %d).\n", retval);
		return retval;
	}
	firmware_id[3] = 0;

	TPD_DMESG("Device firmware ID%d\n", extract_uint(firmware_id));

	/* device config id */
	retval = fwu->fn_ptr->read(fwu->rmi4_data, fwu->f34_fd.ctrl_base_addr,
				   config_id, sizeof(config_id));
	if (retval < 0) {
		dev_info(&i2c_client->dev,
			"Failed to read config ID (code %d).\n", retval);
		return retval;
	}

	TPD_DMESG("Device config ID 0x%02X, 0x%02X, 0x%02X, 0x%02X\n",
		  config_id[0], config_id[1], config_id[2], config_id[3]);

	/* .img config id */
	TPD_DMESG(".img config ID 0x%02X, 0x%02X, 0x%02X, 0x%02X\n",
		  fwu->config_data[0], fwu->config_data[1], fwu->config_data[2],
		  fwu->config_data[3]);

	if (config_id[0] == fwu->config_data[0] &&
	    config_id[1] == fwu->config_data[1] &&
	    config_id[2] == fwu->config_data[2] &&
	    config_id[3] == fwu->config_data[3]) {
		TPD_DMESG(
			"Both the device and .img is the same version, no need to update!\n");
		return -1;
	}
	return 0;
}

static int fwu_read_f01_device_status(struct f01_device_status *status)
{
	int retval;

	retval = fwu->fn_ptr->read(fwu->rmi4_data, fwu->f01_fd.data_base_addr,
				   status->data, sizeof(status->data));
	if (retval < 0) {
		TPD_DMESG("%s: Failed to read F01 device status\n", __func__);
		return retval;
	}

	return 0;
}

static int fwu_read_f34_queries(void)
{
	int retval;
	unsigned char count = 4;
	unsigned char buf[10];
	struct i2c_client *i2c_client = fwu->rmi4_data->i2c_client;

	retval = fwu->fn_ptr->read(fwu->rmi4_data, fwu->f34_fd.query_base_addr +
							   BOOTLOADER_ID_OFFSET,
				   fwu->bootloader_id,
				   sizeof(fwu->bootloader_id));
	if (retval < 0) {
		TPD_DMESG("%s: Failed to read bootloader ID\n", __func__);
		return retval;
	}

	retval = fwu->fn_ptr->read(
		fwu->rmi4_data,
		fwu->f34_fd.query_base_addr + FLASH_PROPERTIES_OFFSET,
		fwu->flash_properties.data, sizeof(fwu->flash_properties.data));
	if (retval < 0) {
		dev_info(&i2c_client->dev,
			"%s: Failed to read flash properties\n", __func__);
		return retval;
	}

	dev_info(&i2c_client->dev, "%s perm:%d, bl%d, display:%d\n", __func__,
		 fwu->flash_properties.has_perm_config,
		 fwu->flash_properties.has_bl_config,
		 fwu->flash_properties.has_display_config);

	if (fwu->flash_properties.has_perm_config)
		count += 2;

	if (fwu->flash_properties.has_bl_config)
		count += 2;

	if (fwu->flash_properties.has_display_config)
		count += 2;

	retval = fwu->fn_ptr->read(fwu->rmi4_data, fwu->f34_fd.query_base_addr +
							   BLOCK_SIZE_OFFSET,
				   buf, 2);
	if (retval < 0) {
		dev_info(&i2c_client->dev,
			"%s: Failed to read block size info\n", __func__);
		return retval;
	}

	batohs(&fwu->block_size, &(buf[0]));

	retval =
		fwu->fn_ptr->read(fwu->rmi4_data, fwu->f34_fd.query_base_addr +
							  FW_BLOCK_COUNT_OFFSET,
				  buf, count);
	if (retval < 0) {
		dev_info(&i2c_client->dev,
			"%s: Failed to read block count info\n", __func__);
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

	fwu->addr_f34_flash_control = fwu->f34_fd.data_base_addr +
				      BLOCK_DATA_OFFSET + fwu->block_size;
	return 0;
}

static int fwu_read_interrupt_status(void)
{
	int retval;
	unsigned char interrupt_status;

	retval = fwu->fn_ptr->read(fwu->rmi4_data,
				   fwu->addr_f01_interrupt_register,
				   &interrupt_status, sizeof(interrupt_status));
	if (retval < 0) {
		TPD_DMESG("%s: Failed to read flash status\n", __func__);
		return retval;
	}
	return interrupt_status;
}

static int fwu_read_f34_flash_status(void)
{
	int retval;

	retval = fwu->fn_ptr->read(fwu->rmi4_data, fwu->addr_f34_flash_control,
				   fwu->flash_control.data,
				   sizeof(fwu->flash_control.data));
	if (retval < 0) {
		dev_info(&fwu->rmi4_data->i2c_client->dev,
			"%s: Failed to read flash status\n", __func__);
		return retval;
	}

	return fwu->flash_control.data[0];
}

static int fwu_reset_device(void)
{
	int retval;
	unsigned char reset = 0x01;

#ifdef DEBUG_FW_UPDATE
	dev_info(&fwu->rmi4_data->i2c_client->dev, "Reset device\n");
#endif

	retval = fwu->fn_ptr->write(fwu->rmi4_data, fwu->f01_fd.cmd_base_addr,
				    &reset, sizeof(reset));
	if (retval < 0) {
		dev_info(&fwu->rmi4_data->i2c_client->dev,
			"%s: Failed to reset device (addr : 0x%02x)\n",
			__func__, fwu->f01_fd.cmd_base_addr);
		return retval;
	}

	fwu_wait_for_idle(WRITE_WAIT_MS);

	retval = fwu->rmi4_data->reset_device(fwu->rmi4_data);
	if (retval < 0) {
		dev_info(&fwu->rmi4_data->i2c_client->dev,
			"%s: Failed to reset core driver after reflash\n",
			__func__);
		return retval;
	}
	return 0;
}

static int fwu_write_f34_command(unsigned char cmd)
{
	int retval;

	retval = fwu->fn_ptr->write(fwu->rmi4_data, fwu->addr_f34_flash_control,
				    &cmd, sizeof(cmd));
	if (retval < 0) {
		TPD_DMESG("%s: Failed to write command 0x%02x\n", __func__,
			  cmd);
		return retval;
	}
	return 0;
}

static unsigned char fwu_check_flash_status(void)
{
	fwu_read_f34_flash_status();
	return fwu->flash_control.status;
}

static int fwu_wait_for_idle(int timeout_ms)
{
	int count = 0;
	int timeout_count = ((timeout_ms * 1000) / MAX_SLEEP_TIME_US) + 1;

	do {
		if (fwu_read_interrupt_status() > 0)
			/* if(     (fwu_read_f34_flash_status())&0x7f==0x00) */
			return 0;

		usleep_range(MIN_SLEEP_TIME_US, MAX_SLEEP_TIME_US);
		count++;
	} while (count < timeout_count);

	TPD_DMESG("%s: Timed out waiting for idle status\n", __func__);

	return -ETIMEDOUT;
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

#ifdef DEBUG_FW_UPDATE
	dev_info(&fwu->rmi4_data->i2c_client->dev, "Scan PDT\n");
#endif

	for (addr = PDT_START; addr > PDT_END; addr -= PDT_ENTRY_SIZE) {
		retval = fwu->fn_ptr->read(fwu->rmi4_data, addr,
					   (unsigned char *)&rmi_fd,
					   sizeof(rmi_fd));
		if (retval < 0)
			return retval;

		if (rmi_fd.fn_number) {
			TPD_DMESG("%s: Found F%02x\n", __func__,
				  rmi_fd.fn_number);
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
				     ii < ((intr_src & MASK_3BIT) + intr_off);
				     ii++)
					fwu->intr_mask |= 1 << ii;
				break;
			}
		} else
			break;

		intr_count += (rmi_fd.intr_src_count & MASK_3BIT);
	}

	if (!f01found || !f34found) {
		dev_info(&fwu->rmi4_data->i2c_client->dev,
			"%s: Failed to find both F01 and F34\n", __func__);
		return -EINVAL;
	}

	fwu_read_interrupt_status();
	return 0;
}

static int fwu_write_blocks(unsigned char *block_ptr, unsigned short block_cnt,
			    unsigned char command)
{
	int retval;
	unsigned char block_offset[] = {0, 0};
	unsigned short block_num;
#ifdef SHOW_PROGRESS
	unsigned int progress = (command == CMD_WRITE_CONFIG_BLOCK) ? 10 : 100;
#endif
	retval = fwu->fn_ptr->write(fwu->rmi4_data, fwu->f34_fd.data_base_addr +
							    BLOCK_NUMBER_OFFSET,
				    block_offset, sizeof(block_offset));
	/* TPD_DMESG("write block number"); */
	if (retval < 0) {
		dev_info(&fwu->rmi4_data->i2c_client->dev,
			"%s: Failed to write to block number registers\n",
			__func__);
		return retval;
	}

	for (block_num = 0; block_num < block_cnt; block_num++) {
#ifdef SHOW_PROGRESS
		if (block_num % progress == 0) {
			TPD_DMESG("%s: update %s %3d / %3d\n", __func__,
				  command == CMD_WRITE_CONFIG_BLOCK
					  ? "config"
					  : "firmware",
				  block_num, block_cnt);
		}
#endif
		/* TPD_DMESG("before write BLOCK_DATA_OFFSET"); */
		retval = fwu->fn_ptr->write(fwu->rmi4_data,
					    fwu->f34_fd.data_base_addr +
						    BLOCK_DATA_OFFSET,
					    block_ptr, fwu->block_size);
		if (retval < 0) {
			TPD_DMESG("%s: Failed to write block data (block %d)\n",
				  __func__, block_num);
			return retval;
		}

		retval = fwu_write_f34_command(command);
		if (retval < 0) {
			dev_info(&fwu->rmi4_data->i2c_client->dev,
				"%s: Failed to write command for block %d\n",
				__func__, block_num);
			return retval;
		}

		retval = fwu_wait_for_idle(WRITE_WAIT_MS);
		if (retval < 0) {
			dev_info(&fwu->rmi4_data->i2c_client->dev,
				"%s: Failed to wait for idle status (block %d)\n",
				__func__, block_num);
			return retval;
		}

		retval = fwu_check_flash_status();
		if (retval != 0) {
			dev_info(&fwu->rmi4_data->i2c_client->dev,
				"%s: Flash block %d status %d\n", __func__,
				block_num, retval);
			return -1;
		}
		block_ptr += fwu->block_size;
	}
#ifdef SHOW_PROGRESS
	dev_info(&fwu->rmi4_data->i2c_client->dev, "%s: update %s %3d / %3d\n",
		 __func__,
		 command == CMD_WRITE_CONFIG_BLOCK ? "config" : "firmware",
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
				fwu->config_block_count,
				CMD_WRITE_CONFIG_BLOCK);
}

static int fwu_write_bootloader_id(void)
{
	int retval;

#ifdef DEBUG_FW_UPDATE
	dev_info(&fwu->rmi4_data->i2c_client->dev, "Write bootloader ID\n");
#endif
	retval = fwu->fn_ptr->write(
		fwu->rmi4_data, fwu->f34_fd.data_base_addr + BLOCK_DATA_OFFSET,
		fwu->bootloader_id, sizeof(fwu->bootloader_id));
	TPD_DMESG("bootloader ID=%s\n", fwu->bootloader_id);
	TPD_DMESG("f34_data_addr=%x\n",
		  fwu->f34_fd.data_base_addr + BLOCK_DATA_OFFSET);

	if (retval < 0) {
		TPD_DMESG("%s: Failed to write bootloader ID\n", __func__);
		return retval;
	}

	return 0;
}

static int fwu_enter_flash_prog(void)
{
	int retval;
	struct f01_device_status f01_device_status;
	struct f01_device_control f01_device_control;

#ifdef DEBUG_FW_UPDATE
	dev_info(&fwu->rmi4_data->i2c_client->dev, "Enter bootloader mode\n");
#endif
	retval = fwu_read_f01_device_status(&f01_device_status);
	if (retval < 0)
		return retval;

	if (f01_device_status.flash_prog) {
		TPD_DMESG("%s: Already in flash prog mode\n", __func__);
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

	retval = fwu_read_f01_device_status(&f01_device_status);
	if (retval < 0)
		return retval;

	if (!f01_device_status.flash_prog) {
		dev_info(&fwu->rmi4_data->i2c_client->dev,
			"%s: Program enabled bit not set\n", __func__);
		return -EINVAL;
	}

	retval = fwu_scan_pdt();
	if (retval < 0)
		return retval;

	retval = fwu_read_f01_device_status(&f01_device_status);
	if (retval < 0)
		return retval;

	if (!f01_device_status.flash_prog) {
		dev_info(&fwu->rmi4_data->i2c_client->dev,
			"%s: Not in flash prog mode\n", __func__);
		return -EINVAL;
	}

	retval = fwu_read_f34_queries();
	if (retval < 0)
		return retval;

	retval = fwu->fn_ptr->read(fwu->rmi4_data, fwu->f01_fd.ctrl_base_addr,
				   f01_device_control.data,
				   sizeof(f01_device_control.data));
	if (retval < 0) {
		TPD_DMESG("%s: Failed to read F01 device control\n", __func__);
		return retval;
	}

	f01_device_control.nosleep = true;
	f01_device_control.sleep_mode = SLEEP_MODE_NORMAL;

	retval = fwu->fn_ptr->write(fwu->rmi4_data, fwu->f01_fd.ctrl_base_addr,
				    f01_device_control.data,
				    sizeof(f01_device_control.data));
	if (retval < 0) {
		TPD_DMESG("%s: Failed to write F01 device control\n", __func__);
		return retval;
	}

	return retval;
}

static int fwu_do_reflash(void)
{
	int retval;

	retval = fwu_enter_flash_prog();
	if (retval < 0)
		return retval;

	TPD_DMESG("%s: Entered flash prog mode\n", __func__);

	retval = fwu_write_bootloader_id();
	if (retval < 0)
		return retval;

	TPD_DMESG("%s: Bootloader ID written\n", __func__);

	retval = fwu_write_f34_command(CMD_ERASE_ALL);
	if (retval < 0)
		return retval;

	TPD_DMESG("%s: Erase all command written\n", __func__);

	/* while (1); */

	retval = fwu_wait_for_idle(ERASE_WAIT_MS);
	if (retval < 0)
		return retval;

	TPD_DMESG("%s: Idle status detected\n", __func__);

	if (fwu->firmware_data) {
		retval = fwu_write_firmware();
		mdelay(100);
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

char buf[] = {
	/* #inlcude ".h" */
};

static int fwu_read_sensor_id(void)
{
	int retval;
	unsigned char sensor_id[2];
	unsigned char snsrid_pinmux[] = {0x05, 0x00};
	unsigned char snsrid_pullupmux[] = {0x05, 0x00};

	if ((fwu->sensor_id & SENSOR_ID_VALID) != 0)
		return 0;

	retval = fwu_enter_flash_prog();
	if (retval < 0)
		goto reset;

	retval = fwu->fn_ptr->write(fwu->rmi4_data, fwu->f34_fd.data_base_addr +
							    BLOCK_DATA_OFFSET,
				    snsrid_pinmux, sizeof(snsrid_pinmux));
	if (retval < 0) {
		TPD_DMESG("%s: Failed to write sensor ID\n", __func__);
		goto reset;
	}

	retval = fwu->fn_ptr->write(fwu->rmi4_data,
				    fwu->f34_fd.data_base_addr +
					    BLOCK_DATA_OFFSET + 2,
				    snsrid_pullupmux, sizeof(snsrid_pullupmux));
	if (retval < 0) {
		TPD_DMESG("%s: Failed to write sensor ID\n", __func__);
		goto reset;
	}

	retval = fwu_write_f34_command(CMD_READ_SENSOR_ID);
	if (retval < 0)
		goto reset;

	retval =
		fwu->fn_ptr->read(fwu->rmi4_data, fwu->f34_fd.data_base_addr +
							  BLOCK_DATA_OFFSET + 4,
				  sensor_id, sizeof(sensor_id));

	if (retval < 0) {
		TPD_DMESG("%s: Failed to read sensor ID\n", __func__);
		goto reset;
	}

	fwu->sensor_id = sensor_id[0] + (sensor_id[1] << 8);
	fwu->sensor_id |= SENSOR_ID_VALID;

#ifdef DEBUG_FW_UPDATE
	TPD_DMESG("Read sensor ID %u\n", fwu->sensor_id & ~SENSOR_ID_VALID);
#endif
reset:
	fwu_reset_device();
	return retval;
}

static int fwu_start_reflash(void)
{
	int retval;
	struct image_header header;
	const unsigned char *fw_image;
	const struct firmware *fw_entry = NULL;
	struct f01_device_status f01_device_status;

	TPD_DMESG("%s: Start of reflash process\n", __func__);
	/* mt65xx_eint_mask(CUST_EINT_TOUCH_PANEL_NUM); */

	if (fwu->ext_data_source)
		fw_image = fwu->ext_data_source;
	else {
		TPD_DMESG("%s: Requesting firmware image %s\n", __func__,
			  FW_IMAGE_NAME);

		retval = fwu_read_sensor_id();
		if (retval < 0)
			return -EINVAL;

		switch (fwu->sensor_id & 0xFFFF) {
		case LENSON:
			fw_image = synaImage;
			break;
		case TPK:
			fw_image = synaImage2;
			break;
		default:
			TPD_DMESG("%s: Unknown touch sensor id: %d\n", __func__,
				  (u16)fwu->sensor_id);
			return -EINVAL;
		}
		TPD_DMESG("%s: Sensor ID=%d\n", __func__, (u16)fwu->sensor_id);
	}

	parse_header(&header, fw_image);

	if (header.image_size)
		fwu->firmware_data = fw_image + FW_IMAGE_OFFSET;
	if (header.config_size)
		fwu->config_data =
			fw_image + FW_IMAGE_OFFSET + header.image_size;

	fwu->fn_ptr->enable(fwu->rmi4_data, false);

	retval = fwu_check_version();

	if (retval < 0)
		goto exit;

	retval = fwu_do_reflash();
	if (retval < 0)
		TPD_DMESG("%s: Failed to do reflash\n", __func__);

	msleep(50);
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
			 "%s: Device is in flash prog mode 0x%02X\n", __func__,
			 f01_device_status.status_code);
		retval = 0;
		goto exit;
	}
	fwu->fn_ptr->enable(fwu->rmi4_data, true);
	if (fw_entry)
		release_firmware(fw_entry);

	pr_notice("%s: End of reflash process\n", __func__);
exit:
	/* mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM); */
	return retval;
}

int synaptics_fw_updater(unsigned char *fw_data)
{
	int retval;

	if (!fwu)
		return -ENODEV;

	if (!fwu->initialized)
		return -ENODEV;

	fwu->ext_data_source = fw_data;
	fwu->config_area = UI_CONFIG_AREA;

	retval = fwu_start_reflash();

	return retval;
}
EXPORT_SYMBOL(synaptics_fw_updater);

static ssize_t fwu_sysfs_show_image(struct file *data_file,
				    struct kobject *kobj,
				    struct bin_attribute *attributes, char *buf,
				    loff_t pos, size_t count)
{
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	if (count < fwu->config_size) {
		dev_info(&rmi4_data->i2c_client->dev,
			"%s: Not enough space (%zu bytes) in buffer\n",
			__func__, count);
		return -EINVAL;
	}

	memcpy(buf, fwu->read_config_buf, fwu->config_size);

	return fwu->config_size;
}

static ssize_t fwu_sysfs_store_image(struct file *data_file,
				     struct kobject *kobj,
				     struct bin_attribute *attributes,
				     char *buf, loff_t pos, size_t count)
{
	memcpy((void *)(&fwu->ext_data_source[fwu->data_pos]),
	       (const void *)buf, count);

	fwu->data_pos += count;

	return count;
}

static void synaptics_rmi4_fwu_attn(struct i2c_client *client,
				 unsigned char intr_mask)
{
	if (fwu->intr_mask & intr_mask)
		fwu_read_f34_flash_status();
}

static int synaptics_rmi4_i2c_read(struct synaptics_rmi4_data *rmi4_data,
				   unsigned short addr, unsigned char *data,
				   unsigned short length)
{
	return tpd_i2c_read_data(rmi4_data->i2c_client, addr, data, length);


}


static int synaptics_rmi4_i2c_write(struct synaptics_rmi4_data *rmi4_data,
				    unsigned short addr, unsigned char *data,
				    unsigned short length)
{
	return tpd_i2c_write_data(rmi4_data->i2c_client, addr, data, length);


}

static int synaptics_rmi4_irq_enable(struct synaptics_rmi4_data *rmi4_data,
				     bool enable)
{
	int retval = 0;
	unsigned char intr_status;


	if (enable) {
		if (rmi4_data->irq_enabled)
			return retval;

		/* Clear interrupts first */
		retval = synaptics_rmi4_i2c_read(
			rmi4_data, rmi4_data->f01_data_base_addr + 1,
			&intr_status, rmi4_data->num_of_intr_regs);
		if (retval < 0)
			return retval;



		rmi4_data->irq_enabled = true;
	} else {
		if (rmi4_data->irq_enabled) {
			disable_irq(rmi4_data->irq);
			free_irq(rmi4_data->irq, rmi4_data);
			rmi4_data->irq_enabled = false;
		}
	}

	return retval;
}

static int synaptics_rmi4_alloc_fh(struct synaptics_rmi4_fn **fhandler,
				   struct synaptics_rmi4_fn_desc *rmi_fd,
				   int page_number)
{
	if (!(*fhandler))
		*fhandler = kmalloc(sizeof(**fhandler), GFP_KERNEL);

	if (!(*fhandler))
		return -ENOMEM;

	(*fhandler)->full_addr.data_base =
		(rmi_fd->data_base_addr | (page_number << 8));
	(*fhandler)->full_addr.ctrl_base =
		(rmi_fd->ctrl_base_addr | (page_number << 8));
	(*fhandler)->full_addr.cmd_base =
		(rmi_fd->cmd_base_addr | (page_number << 8));
	(*fhandler)->full_addr.query_base =
		(rmi_fd->query_base_addr | (page_number << 8));

	return 0;
}

static int synaptics_rmi4_f11_init(struct synaptics_rmi4_data *rmi4_data,
				   struct synaptics_rmi4_fn *fhandler,
				   struct synaptics_rmi4_fn_desc *fd,
				   unsigned int intr_count)
{
	int retval;
	unsigned char ii;
	unsigned char intr_offset;
	unsigned char abs_data_size;
	unsigned char abs_data_blk_size;
	unsigned char query[F11_STD_QUERY_LEN];
	unsigned char control[F11_STD_CTRL_LEN];

	fhandler->fn_number = fd->fn_number;
	fhandler->num_of_data_sources = fd->intr_src_count;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
					 fhandler->full_addr.query_base, query,
					 sizeof(query));
	if (retval < 0)
		return retval;

	/* Maximum number of fingers supported */
	if ((query[1] & MASK_3BIT) <= 4)
		fhandler->num_of_data_points = (query[1] & MASK_3BIT) + 1;
	else if ((query[1] & MASK_3BIT) == 5)
		fhandler->num_of_data_points = 10;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
					 fhandler->full_addr.ctrl_base, control,
					 sizeof(control));
	if (retval < 0)
		return retval;

	/* Maximum x and y */
	rmi4_data->sensor_max_x = ((control[6] & MASK_8BIT) << 0) |
				  ((control[7] & MASK_4BIT) << 8);
	rmi4_data->sensor_max_y = ((control[8] & MASK_8BIT) << 0) |
				  ((control[9] & MASK_4BIT) << 8);
	dev_dbg(&rmi4_data->i2c_client->dev,
		"%s: Function %02x max x = %d max y = %d\n", __func__,
		fhandler->fn_number, rmi4_data->sensor_max_x,
		rmi4_data->sensor_max_y);

	fhandler->intr_reg_num = (intr_count + 7) / 8;
	if (fhandler->intr_reg_num != 0)
		fhandler->intr_reg_num -= 1;

	/* Set an enable bit for each data source */
	intr_offset = intr_count % 8;
	fhandler->intr_mask = 0;
	for (ii = intr_offset;
	     ii < ((fd->intr_src_count & MASK_3BIT) + intr_offset); ii++)
		fhandler->intr_mask |= 1 << ii;

	abs_data_size = query[5] & MASK_2BIT;
	abs_data_blk_size = 3 + (2 * (abs_data_size == 0 ? 1 : 0));
	fhandler->size_of_data_register_block = abs_data_blk_size;

	return retval;
}

/*
 * synaptics_rmi4_f12_init()
 *
 * Called by synaptics_rmi4_query_device().
 *
 * This function parses information from the Function 12 registers and
 * determines the number of fingers supported, offset to the data1
 * register, x and y data ranges, offset to the associated interrupt
 * status register, interrupt bit mask, and allocates memory resources
 * for finger data acquisition.
 */
static int synaptics_rmi4_f12_init(struct synaptics_rmi4_data *rmi4_data,
				   struct synaptics_rmi4_fn *fhandler,
				   struct synaptics_rmi4_fn_desc *fd,
				   unsigned int intr_count)
{
	int retval;
	unsigned char ii;
	unsigned char intr_offset;
	unsigned char ctrl_8_offset;
	unsigned char ctrl_23_offset;
	struct synaptics_rmi4_f12_query_5 query_5;
	struct synaptics_rmi4_f12_query_8 query_8;
	struct synaptics_rmi4_f12_ctrl_8 ctrl_8;
	struct synaptics_rmi4_f12_ctrl_23 ctrl_23;
	struct synaptics_rmi4_f12_finger_data *finger_data_list;

	fhandler->fn_number = fd->fn_number;
	fhandler->num_of_data_sources = fd->intr_src_count;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
					 fhandler->full_addr.query_base + 5,
					 query_5.data, sizeof(query_5.data));
	if (retval < 0)
		return retval;

	ctrl_8_offset = query_5.ctrl0_is_present + query_5.ctrl1_is_present +
			query_5.ctrl2_is_present + query_5.ctrl3_is_present +
			query_5.ctrl4_is_present + query_5.ctrl5_is_present +
			query_5.ctrl6_is_present + query_5.ctrl7_is_present;

	ctrl_23_offset = ctrl_8_offset + query_5.ctrl8_is_present +
			 query_5.ctrl9_is_present + query_5.ctrl10_is_present +
			 query_5.ctrl11_is_present + query_5.ctrl12_is_present +
			 query_5.ctrl13_is_present + query_5.ctrl14_is_present +
			 query_5.ctrl15_is_present + query_5.ctrl16_is_present +
			 query_5.ctrl17_is_present + query_5.ctrl18_is_present +
			 query_5.ctrl19_is_present + query_5.ctrl20_is_present +
			 query_5.ctrl21_is_present + query_5.ctrl22_is_present;

	retval = synaptics_rmi4_i2c_read(
		rmi4_data, fhandler->full_addr.ctrl_base + ctrl_23_offset,
		ctrl_23.data, sizeof(ctrl_23.data));
	if (retval < 0)
		return retval;

	/* Maximum number of fingers supported */
	fhandler->num_of_data_points = ctrl_23.max_reported_objects;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
					 fhandler->full_addr.query_base + 8,
					 query_8.data, sizeof(query_8.data));
	if (retval < 0)
		return retval;

	/* Determine the presence of the Data0 register */
	fhandler->data1_offset = query_8.data0_is_present;

	retval = synaptics_rmi4_i2c_read(
		rmi4_data, fhandler->full_addr.ctrl_base + ctrl_8_offset,
		ctrl_8.data, sizeof(ctrl_8.data));
	if (retval < 0)
		return retval;

	/* Maximum x and y */
	rmi4_data->sensor_max_x =
		((unsigned short)ctrl_8.max_x_coord_lsb << 0) |
		((unsigned short)ctrl_8.max_x_coord_msb << 8);
	rmi4_data->sensor_max_y =
		((unsigned short)ctrl_8.max_y_coord_lsb << 0) |
		((unsigned short)ctrl_8.max_y_coord_msb << 8);
	dev_dbg(&rmi4_data->i2c_client->dev,
		"%s: Function %02x max x = %d max y = %d\n", __func__,
		fhandler->fn_number, rmi4_data->sensor_max_x,
		rmi4_data->sensor_max_y);

	rmi4_data->num_of_rx = ctrl_8.num_of_rx;
	rmi4_data->num_of_tx = ctrl_8.num_of_tx;

	fhandler->intr_reg_num = (intr_count + 7) / 8;
	if (fhandler->intr_reg_num != 0)
		fhandler->intr_reg_num -= 1;

	/* Set an enable bit for each data source */
	intr_offset = intr_count % 8;
	fhandler->intr_mask = 0;
	for (ii = intr_offset;
	     ii < ((fd->intr_src_count & MASK_3BIT) + intr_offset); ii++)
		fhandler->intr_mask |= 1 << ii;

	/* Allocate memory for finger data storage space */
	fhandler->data_size = fhandler->num_of_data_points *
			      sizeof(struct synaptics_rmi4_f12_finger_data);
	finger_data_list = kmalloc(fhandler->data_size, GFP_KERNEL);
	fhandler->data = (void *)finger_data_list;

	return retval;
}

static void synaptics_rmi4_f1a_kfree(struct synaptics_rmi4_fn *fhandler)
{
	struct synaptics_rmi4_f1a_handle *f1a = fhandler->data;

	if (f1a) {
		kfree(f1a->button_data_buffer);
		kfree(f1a->button_map);
		kfree(f1a);
		fhandler->data = NULL;
	}
}

static int synaptics_rmi4_f1a_alloc_mem(struct synaptics_rmi4_data *rmi4_data,
					struct synaptics_rmi4_fn *fhandler)
{
	int retval;
	struct synaptics_rmi4_f1a_handle *f1a;

	f1a = kzalloc(sizeof(*f1a), GFP_KERNEL);
	if (!f1a)

		return -ENOMEM;


	fhandler->data = (void *)f1a;

	retval = synaptics_rmi4_i2c_read(
		rmi4_data, fhandler->full_addr.query_base,
		f1a->button_query.data, sizeof(f1a->button_query.data));
	if (retval < 0) {
		dev_info(&rmi4_data->i2c_client->dev,
			"%s: Failed to read query registers\n", __func__);
		return retval;
	}

	f1a->button_count = f1a->button_query.max_button_count + 1;
	f1a->button_bitmask_size = (f1a->button_count + 7) / 8;

	f1a->button_data_buffer =
		kcalloc(f1a->button_bitmask_size,
			sizeof(*(f1a->button_data_buffer)), GFP_KERNEL);
	if (!f1a->button_data_buffer)

		return -ENOMEM;


	f1a->button_map = kcalloc(f1a->button_count, sizeof(*(f1a->button_map)),
				  GFP_KERNEL);
	if (!f1a->button_map)

		return -ENOMEM;


	return 0;
}

static int synaptics_rmi4_f1a_button_map(struct synaptics_rmi4_data *rmi4_data,
					 struct synaptics_rmi4_fn *fhandler)
{


	return 0;
}

static int synaptics_rmi4_f1a_init(struct synaptics_rmi4_data *rmi4_data,
				   struct synaptics_rmi4_fn *fhandler,
				   struct synaptics_rmi4_fn_desc *fd,
				   unsigned int intr_count)
{
	int retval;
	unsigned char ii;
	unsigned short intr_offset;

	fhandler->fn_number = fd->fn_number;
	fhandler->num_of_data_sources = fd->intr_src_count;

	fhandler->intr_reg_num = (intr_count + 7) / 8;
	if (fhandler->intr_reg_num != 0)
		fhandler->intr_reg_num -= 1;

	/* Set an enable bit for each data source */
	intr_offset = intr_count % 8;
	fhandler->intr_mask = 0;
	for (ii = intr_offset;
	     ii < ((fd->intr_src_count & MASK_3BIT) + intr_offset); ii++)
		fhandler->intr_mask |= 1 << ii;

	retval = synaptics_rmi4_f1a_alloc_mem(rmi4_data, fhandler);
	if (retval < 0)
		goto error_exit;

	retval = synaptics_rmi4_f1a_button_map(rmi4_data, fhandler);
	if (retval < 0)
		goto error_exit;

	rmi4_data->button_0d_enabled = 1;

	return 0;

error_exit:
	synaptics_rmi4_f1a_kfree(fhandler);

	return retval;
}

/*
 * synaptics_rmi4_query_device()
 *
 * Called by synaptics_rmi4_probe().
 *
 * This function scans the page description table, records the offsets
 * to the register types of Function $01, sets up the function handlers
 * for Function $11 and Function $12, determines the number of interrupt
 * sources from the sensor, adds valid Functions with data inputs to the
 * Function linked list, parses information from the query registers of
 * Function $01, and enables the interrupt sources from the valid Functions
 * with data inputs.
 */
static int synaptics_rmi4_query_device(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char ii;
	unsigned char page_number;
	unsigned char intr_count = 0;
	unsigned char data_sources = 0;
	unsigned char f01_query[F01_STD_QUERY_LEN];
	unsigned short pdt_entry_addr;
	unsigned short intr_addr;
	struct synaptics_rmi4_f01_device_status status;
	struct synaptics_rmi4_fn_desc rmi_fd;
	struct synaptics_rmi4_fn *fhandler;
	struct synaptics_rmi4_device_info *rmi;

	rmi = &(rmi4_data->rmi4_mod_info);

	INIT_LIST_HEAD(&rmi->support_fn_list);

	/* Scan the page description tables of the pages to service */
	for (page_number = 0; page_number < PAGES_TO_SERVICE; page_number++) {
		for (pdt_entry_addr = PDT_START; pdt_entry_addr > PDT_END;
		     pdt_entry_addr -= PDT_ENTRY_SIZE) {
			pdt_entry_addr |= (page_number << 8);

			retval = synaptics_rmi4_i2c_read(
				rmi4_data, pdt_entry_addr,
				(unsigned char *)&rmi_fd, sizeof(rmi_fd));
			if (retval < 0)
				return retval;

			fhandler = NULL;

			if (rmi_fd.fn_number == 0) {
				TPD_DMESG("%s: Reached end of PDT\n", __func__);
				break;
			}

			TPD_DMESG("%s: F%02x found (page %d)\n", __func__,
				  rmi_fd.fn_number, page_number);

			switch (rmi_fd.fn_number) {
			case SYNAPTICS_RMI4_F01:
				rmi4_data->f01_query_base_addr =
					rmi_fd.query_base_addr;
				rmi4_data->f01_ctrl_base_addr =
					rmi_fd.ctrl_base_addr;
				rmi4_data->f01_data_base_addr =
					rmi_fd.data_base_addr;
				rmi4_data->f01_cmd_base_addr =
					rmi_fd.cmd_base_addr;

				retval = synaptics_rmi4_i2c_read(
					rmi4_data,
					rmi4_data->f01_data_base_addr,
					status.data, sizeof(status.data));
				if (retval < 0)
					return retval;

				if (status.flash_prog == 1) {
					TPD_DMESG(
						"%s: In flash prog mode, status = 0x%02x\n",
						__func__, status.status_code);
					goto flash_prog_mode;
				}
				break;
			case SYNAPTICS_RMI4_F11:
				if (rmi_fd.intr_src_count == 0)
					break;

				retval = synaptics_rmi4_alloc_fh(
					&fhandler, &rmi_fd, page_number);
				if (retval < 0) {
					dev_info(&rmi4_data->i2c_client->dev,
						"%s: Failed to alloc for F%d\n",
						__func__, rmi_fd.fn_number);
					return retval;
				}

				retval = synaptics_rmi4_f11_init(
					rmi4_data, fhandler, &rmi_fd,
					intr_count);
				if (retval < 0)
					return retval;
				break;
			case SYNAPTICS_RMI4_F12:
				if (rmi_fd.intr_src_count == 0)
					break;

				retval = synaptics_rmi4_alloc_fh(
					&fhandler, &rmi_fd, page_number);
				if (retval < 0) {
					dev_info(&rmi4_data->i2c_client->dev,
						"%s: Failed to alloc for F%d\n",
						__func__, rmi_fd.fn_number);
					return retval;
				}

				retval = synaptics_rmi4_f12_init(
					rmi4_data, fhandler, &rmi_fd,
					intr_count);
				if (retval < 0)
					return retval;
				break;
			case SYNAPTICS_RMI4_F1A:
				if (rmi_fd.intr_src_count == 0)
					break;

				retval = synaptics_rmi4_alloc_fh(
					&fhandler, &rmi_fd, page_number);
				if (retval < 0) {
					dev_info(&rmi4_data->i2c_client->dev,
						"%s: Failed to alloc for F%d\n",
						__func__, rmi_fd.fn_number);
					return retval;
				}

				retval = synaptics_rmi4_f1a_init(
					rmi4_data, fhandler, &rmi_fd,
					intr_count);
				if (retval < 0)
					return retval;
				break;
#if PROXIMITY
			case SYNAPTICS_RMI4_F51:
				if (rmi_fd.intr_src_count == 0)
					break;

				retval = synaptics_rmi4_alloc_fh(
					&fhandler, &rmi_fd, page_number);
				if (retval < 0) {
					dev_info(&rmi4_data->i2c_client->dev,
						"%s: Failed to alloc for F%d\n",
						__func__, rmi_fd.fn_number);
					return retval;
				}

				retval = synaptics_rmi4_f51_init(
					rmi4_data, fhandler, &rmi_fd,
					intr_count);
				if (retval < 0)
					return retval;
				break;
#endif
			}

			/* Accumulate the interrupt count */
			intr_count += (rmi_fd.intr_src_count & MASK_3BIT);

			if (fhandler && rmi_fd.intr_src_count)
				list_add_tail(&fhandler->link,
					      &rmi->support_fn_list);
		}
	}

flash_prog_mode:
	rmi4_data->num_of_intr_regs = (intr_count + 7) / 8;
	TPD_DMESG("%s: Number of interrupt registers = %d\n", __func__,
		  rmi4_data->num_of_intr_regs);

	retval = synaptics_rmi4_i2c_read(rmi4_data,
					 rmi4_data->f01_query_base_addr,
					 f01_query, sizeof(f01_query));
	if (retval < 0)
		return retval;

	/* RMI Version 4.0 currently supported */
	rmi->version_major = 4;
	rmi->version_minor = 0;

	rmi->manufacturer_id = f01_query[0];
	rmi->product_props = f01_query[1];
	rmi->product_info[0] = f01_query[2] & MASK_7BIT;
	rmi->product_info[1] = f01_query[3] & MASK_7BIT;
	rmi->date_code[0] = f01_query[4] & MASK_5BIT;
	rmi->date_code[1] = f01_query[5] & MASK_4BIT;
	rmi->date_code[2] = f01_query[6] & MASK_5BIT;
	rmi->tester_id =
		((f01_query[7] & MASK_7BIT) << 8) | (f01_query[8] & MASK_7BIT);
	rmi->serial_number =
		((f01_query[9] & MASK_7BIT) << 8) | (f01_query[10] & MASK_7BIT);
	memcpy(rmi->product_id_string, &f01_query[11], 10);

	if (rmi->manufacturer_id != 1) {
		dev_info(&rmi4_data->i2c_client->dev,
			"%s: Non-Synaptics device found, manufacturer ID = %d\n",
			__func__, rmi->manufacturer_id);
	}

	memset(rmi4_data->intr_mask, 0x00, sizeof(rmi4_data->intr_mask));

	/*
	 * Map out the interrupt bit masks for the interrupt sources
	 * from the registered function handlers.
	 */
	list_for_each_entry(fhandler, &rmi->support_fn_list, link)
		data_sources += fhandler->num_of_data_sources;
	if (data_sources) {
		list_for_each_entry(fhandler, &rmi->support_fn_list, link) {
			if (fhandler->num_of_data_sources)
				rmi4_data->intr_mask[fhandler->intr_reg_num] |=
					fhandler->intr_mask;
		}
	}

	/* Enable the interrupt sources */
	for (ii = 0; ii < rmi4_data->num_of_intr_regs; ii++) {
		if (rmi4_data->intr_mask[ii] != 0x00) {
			TPD_DMESG("%s: Interrupt enable mask %d = 0x%02x\n",
				  __func__, ii, rmi4_data->intr_mask[ii]);
			intr_addr = rmi4_data->f01_ctrl_base_addr + 1 + ii;
			retval = synaptics_rmi4_i2c_write(
				rmi4_data, intr_addr,
				&(rmi4_data->intr_mask[ii]),
				sizeof(rmi4_data->intr_mask[ii]));
			if (retval < 0)
				return retval;
		}
	}

	return 0;
}

static int synaptics_rmi4_reset_device(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char command = 0x01;
	/* struct synaptics_rmi4_fn *fhandler; */
	struct synaptics_rmi4_device_info *rmi;

	rmi = &(rmi4_data->rmi4_mod_info);

	retval = synaptics_rmi4_i2c_write(rmi4_data,
					  rmi4_data->f01_cmd_base_addr,
					  &command, sizeof(command));
	if (retval < 0) {
		dev_info(&rmi4_data->i2c_client->dev,
			"%s: Failed to issue reset command, error = %d\n",
			__func__, retval);
		return retval;
	}

	msleep(100);


	retval = synaptics_rmi4_query_device(rmi4_data);
	if (retval < 0) {
		dev_info(&rmi4_data->i2c_client->dev,
			"%s: Failed to query device\n", __func__);
		return retval;
	}

	return 0;
}

static int synaptics_rmi4_fwu_init(struct i2c_client *client)
{
	int retval;
	unsigned char attr_count;
	struct pdt_properties pdt_props;

	TPD_DMESG("%s:enter\n", __func__);

	fwu = kzalloc(sizeof(*fwu), GFP_KERNEL);
	if (!fwu)

		goto exit;


	fwu->rmi4_data =
		kzalloc(sizeof(struct synaptics_rmi4_data), GFP_KERNEL);
	if (!fwu->rmi4_data) {

		retval = -ENOMEM;
		goto exit_free_fwu;
	}

	fwu->fn_ptr = kzalloc(sizeof(*(fwu->fn_ptr)), GFP_KERNEL);
	if (!fwu->fn_ptr) {

		retval = -ENOMEM;
		goto exit_free_rmi4;
	}

	fwu->rmi4_data->input_dev = tpd->dev;
	fwu->rmi4_data->i2c_client = client;
	fwu->fn_ptr->read = synaptics_rmi4_i2c_read;
	fwu->fn_ptr->write = synaptics_rmi4_i2c_write;
	fwu->fn_ptr->enable = synaptics_rmi4_irq_enable;
	fwu->rmi4_data->reset_device = synaptics_rmi4_reset_device;

	retval = synaptics_rmi4_query_device(fwu->rmi4_data);
	if (retval < 0)
		goto exit_free_mem;

	mutex_init(&(fwu->rmi4_data->rmi4_io_ctrl_mutex));

	retval = fwu->fn_ptr->read(fwu->rmi4_data, PDT_PROPS, pdt_props.data,
				   sizeof(pdt_props.data));
	if (retval < 0) {
		TPD_DMESG("%s: Failed to read PDT properties, assuming 0x00\n",
			  __func__);
	} else if (pdt_props.has_bsr) {
		TPD_DMESG("%s: Reflash for LTS not currently supported\n",
			  __func__);
		goto exit_free_mem;
	}

	retval = fwu_scan_pdt();
	if (retval < 0)
		goto exit_free_mem;


	retval = fwu_read_f34_queries();
	if (retval < 0)
		goto exit_free_mem;

	TPD_DMESG(
		"query_base_addr=0x%x, data_base_addr=0x%x, addr_f34_flash_control=0x%x, bootloader_id=%s\n",
		fwu->f34_fd.query_base_addr, fwu->f34_fd.data_base_addr,
		fwu->addr_f34_flash_control, fwu->bootloader_id);

	fwu->initialized = true;
	/* fwu->ext_data_source = synaImage; */
	/* fwu->config_area = UI_CONFIG_AREA; */

	properties_kobj_fwupdate =
		kobject_create_and_add("fwupdate", properties_kobj_synap);

	if (!properties_kobj_fwupdate) {
		dev_info(&client->dev, "%s: Failed to create sysfs directory\n",
			__func__);
		goto err_sysfs_dir;
	}
	retval =
		sysfs_create_bin_file(properties_kobj_fwupdate, &dev_attr_data);
	if (retval < 0) {
		dev_info(&client->dev, "%s: Failed to create sysfs bin file\n",
			__func__);
		goto exit_free_mem;
	}

	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++) {
		/* retval = sysfs_create_file(&tpd->dev->dev.kobj,*/
		 /* &attrs[attr_count].attr); */
		retval = sysfs_create_file(properties_kobj_fwupdate,
					   &attrs[attr_count].attr);
		if (retval < 0) {
			dev_info(&client->dev,
				"%s: Failed to create sysfs attributes\n",
				__func__);
			retval = -ENODEV;
			goto exit_remove_attrs;
		}
	}


	return 0;

exit_remove_attrs:
	for (attr_count--; attr_count >= 0; attr_count--)
		sysfs_remove_file(properties_kobj_fwupdate,
				  &attrs[attr_count].attr);

	sysfs_remove_bin_file(properties_kobj_fwupdate, &dev_attr_data);

err_sysfs_dir:

exit_free_mem:
	kfree(fwu->fn_ptr);

exit_free_rmi4:
	kfree(fwu->rmi4_data);

exit_free_fwu:
	kfree(fwu);

exit:
	return 0;
}

static void synaptics_rmi4_fwu_remove(struct i2c_client *client)
{
	unsigned char attr_count;

	sysfs_remove_bin_file(&fwu->rmi4_data->input_dev->dev.kobj,
			      &dev_attr_data);

	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++)
		sysfs_remove_file(&fwu->rmi4_data->input_dev->dev.kobj,
				  &attrs[attr_count].attr);

	kfree(fwu->fn_ptr);
	kfree(fwu);

	complete(&remove_complete);
}

static int __init rmi4_fw_update_module_init(void)
{
	synaptics_rmi4_new_function(
		RMI_FW_UPDATER, true, synaptics_rmi4_fwu_init,
		synaptics_rmi4_fwu_remove, synaptics_rmi4_fwu_attn);
	return 0;
}

static void __exit rmi4_fw_update_module_exit(void)
{
	init_completion(&remove_complete);
	synaptics_rmi4_new_function(
		RMI_FW_UPDATER, false, synaptics_rmi4_fwu_init,
		synaptics_rmi4_fwu_remove, synaptics_rmi4_fwu_attn);
	wait_for_completion(&remove_complete);
}
module_init(rmi4_fw_update_module_init);
module_exit(rmi4_fw_update_module_exit);

MODULE_AUTHOR("Synaptics, Inc.");
MODULE_DESCRIPTION("RMI4 FW Update Module");
MODULE_LICENSE("GPL");
MODULE_VERSION(SYNAPTICS_RMI4_DRIVER_VERSION);
