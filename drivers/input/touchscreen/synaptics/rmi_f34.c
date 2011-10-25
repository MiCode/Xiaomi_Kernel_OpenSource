/**
 *
 * Synaptics Register Mapped Interface (RMI4) Function $34 support for sensor
 * firmware reflashing.
 *
 * Copyright (c) 2007 - 2011, Synaptics Incorporated
 *
 */
/*
 * This file is licensed under the GPL2 license.
 *
 *#############################################################################
 * GPL
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 *#############################################################################
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/input.h>
#include <linux/sysfs.h>
#include <linux/math64.h>
#include "rmi_drvr.h"
#include "rmi_bus.h"
#include "rmi_sensor.h"
#include "rmi_function.h"
#include "rmi_f34.h"

/* data specific to fn $34 that needs to be kept around */
struct rmi_fn_34_data {
	unsigned char   status;
	unsigned char   cmd;
	unsigned short  bootloaderid;
	unsigned short  blocksize;
};


static ssize_t rmi_fn_34_status_show(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t rmi_fn_34_status_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);


static ssize_t rmi_fn_34_cmd_show(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t rmi_fn_34_cmd_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

static ssize_t rmi_fn_34_data_read(struct file *,
				struct kobject *kobj,
				struct bin_attribute *attributes,
				char *buf, loff_t pos, size_t count);

static ssize_t rmi_fn_34_data_write(struct file *,
				struct kobject *kobj,
				struct bin_attribute *attributes,
				char *buf, loff_t pos, size_t count);

static ssize_t rmi_fn_34_bootloaderid_show(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t rmi_fn_34_bootloaderid_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

static ssize_t rmi_fn_34_blocksize_show(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t rmi_fn_34_blocksize_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

/* define the device attributes using DEVICE_ATTR macros */
DEVICE_ATTR(status, 0444, rmi_fn_34_status_show, rmi_fn_34_status_store);  /* RO attr */
DEVICE_ATTR(cmd, 0664, rmi_fn_34_cmd_show, rmi_fn_34_cmd_store);     /* RW attr */
DEVICE_ATTR(bootloaderid, 0644, rmi_fn_34_bootloaderid_show, rmi_fn_34_bootloaderid_store); /* RW attr */
DEVICE_ATTR(blocksize, 0444, rmi_fn_34_blocksize_show, rmi_fn_34_blocksize_store);    /* RO attr */


struct bin_attribute dev_attr_data = {
	.attr = {
		.name = "data",
		.mode = 0644
	},
	.size = 0,
	.read = rmi_fn_34_data_read,
	.write = rmi_fn_34_data_write,
};

/* Helper fn to convert from processor specific data to our firmware specific endianness.
 * TODO: Should we use ntohs or something like that?
 */
void copyEndianAgnostic(unsigned char *dest, unsigned short src)
{
	dest[0] = src%0x100;
	dest[1] = src/0x100;
}

/*.
 * The interrupt handler for Fn $34.
 */
void FN_34_inthandler(struct rmi_function_info *rmifninfo,
	unsigned int assertedIRQs)
{
	unsigned int status;
	struct rmi_fn_34_data *fn34data = (struct rmi_fn_34_data *)rmifninfo->fndata;

	/* Read the Fn $34 status register to see whether the previous command executed OK */
	/* inform user space - through a sysfs param. */
	if (rmi_read_multiple(rmifninfo->sensor, rmifninfo->funcDescriptor.dataBaseAddr+3,
		(unsigned char *)&status, 1)) {
		printk(KERN_ERR "%s : Could not read status from 0x%x\n",
			__func__, rmifninfo->funcDescriptor.dataBaseAddr+3);
		status = 0xff; /* failure */
	}

	/* set a sysfs value that the user mode can read - only upper 4 bits are the status */
	fn34data->status = status & 0xf0; /* successful is $80, anything else is failure */
}
EXPORT_SYMBOL(FN_34_inthandler);

void FN_34_attention(struct rmi_function_info *rmifninfo)
{

}
EXPORT_SYMBOL(FN_34_attention);

int FN_34_config(struct rmi_function_info *rmifninfo)
{
	pr_debug("%s: RMI4 function $34 config\n", __func__);
	return 0;
}
EXPORT_SYMBOL(FN_34_config);


int FN_34_init(struct rmi_function_device *function_device)
{
	int retval = 0;
	unsigned char uData[2];
	struct rmi_function_info *rmifninfo = function_device->rfi;
	struct rmi_fn_34_data *fn34data;

	pr_debug("%s: RMI4 function $34 init\n", __func__);

	/* Here we will need to set up sysfs files for Bootloader ID and Block size */
	fn34data = kzalloc(sizeof(struct rmi_fn_34_data), GFP_KERNEL);
	if (!fn34data) {
		printk(KERN_ERR "%s: Error allocating memeory for rmi_fn_34_data.\n", __func__);
		return -ENOMEM;
	}
	rmifninfo->fndata = (void *)fn34data;

	/* set up sysfs file for Bootloader ID. */
	if (sysfs_create_file(&function_device->dev.kobj, &dev_attr_bootloaderid.attr) < 0) {
		printk(KERN_ERR "%s: Failed to create sysfs file for fn 34 bootloaderid.\n", __func__);
		return -ENODEV;
	}

	/* set up sysfs file for Block Size. */
	if (sysfs_create_file(&function_device->dev.kobj, &dev_attr_blocksize.attr) < 0) {
		printk(KERN_ERR "%s: Failed to create sysfs file for fn 34 blocksize.\n", __func__);
		return -ENODEV;
	}

	/* get the Bootloader ID and Block Size and store in the sysfs attributes. */
	retval = rmi_read_multiple(rmifninfo->sensor, rmifninfo->funcDescriptor.queryBaseAddr,
		uData, 2);
	if (retval) {
		printk(KERN_ERR "%s : Could not read bootloaderid from 0x%x\n",
			__func__, function_device->function->functionQueryBaseAddr);
		return retval;
	}
	/* need to convert from our firmware storage to processore specific data */
	fn34data->bootloaderid = (unsigned int)uData[0] + (unsigned int)uData[1]*0x100;

	retval = rmi_read_multiple(rmifninfo->sensor, rmifninfo->funcDescriptor.queryBaseAddr+3,
		uData, 2);
	if (retval) {
		printk(KERN_ERR "%s : Could not read block size from 0x%x\n",
			__func__, rmifninfo->funcDescriptor.queryBaseAddr+3);
		return retval;
	}
	/* need to convert from our firmware storage to processor specific data */
	fn34data->blocksize = (unsigned int)uData[0] + (unsigned int)uData[1]*0x100;

	/* set up sysfs file for status. */
	if (sysfs_create_file(&function_device->dev.kobj, &dev_attr_status.attr) < 0) {
		printk(KERN_ERR "%s: Failed to create sysfs file for fn 34 status.\n", __func__);
		return -ENODEV;
	}

	/* Also, sysfs will need to have a file set up to distinguish between commands - like
	Config write/read, Image write/verify.*/
	/* set up sysfs file for command code. */
	if (sysfs_create_file(&function_device->dev.kobj, &dev_attr_cmd.attr) < 0) {
		printk(KERN_ERR "%s: Failed to create sysfs file for fn 34 cmd.\n", __func__);
		return -ENODEV;
	}

	/* We will also need a sysfs file for the image/config block to write or read.*/
	/* set up sysfs bin file for binary data block. Since the image is already in our format
	there is no need to convert the data for endianess. */
	if (sysfs_create_bin_file(&function_device->dev.kobj, &dev_attr_data) < 0) {
		printk(KERN_ERR "%s: Failed to create sysfs file for fn 34 data.\n", __func__);
		return -ENODEV;
	}

	return retval;
}
EXPORT_SYMBOL(FN_34_init);

int FN_34_detect(struct rmi_function_info *rmifninfo,
	struct rmi_function_descriptor *fndescr, unsigned int interruptCount)
{
	int i;
	int InterruptOffset;
	int retval = 0;

	pr_debug("%s: RMI4 function $34 detect\n", __func__);
	if (rmifninfo->sensor == NULL) {
		printk(KERN_ERR "%s: NULL sensor passed in!", __func__);
		return -EINVAL;
	}

	/* Store addresses - used elsewhere to read data,
	* control, query, etc. */
	rmifninfo->funcDescriptor.queryBaseAddr = fndescr->queryBaseAddr;
	rmifninfo->funcDescriptor.commandBaseAddr = fndescr->commandBaseAddr;
	rmifninfo->funcDescriptor.controlBaseAddr = fndescr->controlBaseAddr;
	rmifninfo->funcDescriptor.dataBaseAddr = fndescr->dataBaseAddr;
	rmifninfo->funcDescriptor.interruptSrcCnt = fndescr->interruptSrcCnt;
	rmifninfo->funcDescriptor.functionNum = fndescr->functionNum;

	rmifninfo->numSources = fndescr->interruptSrcCnt;

	/* Need to get interrupt info to be used later when handling
	interrupts. */
	rmifninfo->interruptRegister = interruptCount/8;

	/* loop through interrupts for each source and or in a bit
	to the interrupt mask for each. */
	InterruptOffset = interruptCount % 8;

	for (i = InterruptOffset;
		i < ((fndescr->interruptSrcCnt & 0x7) + InterruptOffset);
		i++) {
			rmifninfo->interruptMask |= 1 << i;
	}

	return retval;
}
EXPORT_SYMBOL(FN_34_detect);

static ssize_t rmi_fn_34_bootloaderid_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rmi_function_device *fn = dev_get_drvdata(dev);
	struct rmi_fn_34_data *fn34data = (struct rmi_fn_34_data *)fn->rfi->fndata;

	return sprintf(buf, "%u\n", fn34data->bootloaderid);
}

static ssize_t rmi_fn_34_bootloaderid_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int error;
	unsigned long val;
	unsigned char uData[2];
	struct rmi_function_device *fn = dev_get_drvdata(dev);
	struct rmi_fn_34_data *fn34data = (struct rmi_fn_34_data *)fn->rfi->fndata;

	/* need to convert the string data to an actual value */
	error = strict_strtoul(buf, 10, &val);

	if (error)
		return error;

	fn34data->bootloaderid = val;

	/* Write the Bootloader ID key data back to the first two Block Data registers
	(F34_Flash_Data2.0 and F34_Flash_Data2.1).*/
	copyEndianAgnostic(uData, (unsigned short)val);
	error = rmi_write_multiple(fn->sensor, fn->function->functionDataBaseAddr,
		uData, 2);
	if (error) {
		printk(KERN_ERR "%s : Could not write bootloader id to 0x%x\n",
			__func__, fn->function->functionDataBaseAddr);
		return error;
	}

	return count;
}

static ssize_t rmi_fn_34_blocksize_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rmi_function_device *fn = dev_get_drvdata(dev);
	struct rmi_fn_34_data *fn34data = (struct rmi_fn_34_data *)fn->rfi->fndata;

	return sprintf(buf, "%u\n", fn34data->blocksize);
}

static ssize_t rmi_fn_34_blocksize_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	/* Block Size is RO so we shouldn't do anything if the
	user space writes to the sysfs file. */

	return -EPERM;
}

static ssize_t rmi_fn_34_status_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rmi_function_device *fn = dev_get_drvdata(dev);
	struct rmi_fn_34_data *fn34data = (struct rmi_fn_34_data *)fn->rfi->fndata;

	return sprintf(buf, "%u\n", fn34data->status);
}

static ssize_t rmi_fn_34_status_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	/* Status is RO so we shouldn't do anything if the user
	app writes to the sysfs file. */
	return -EPERM;
}

static ssize_t rmi_fn_34_cmd_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rmi_function_device *fn = dev_get_drvdata(dev);
	struct rmi_fn_34_data *fn34data = (struct rmi_fn_34_data *)fn->rfi->fndata;

	return sprintf(buf, "%u\n", fn34data->cmd);
}

static ssize_t rmi_fn_34_cmd_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct rmi_function_device *fn = dev_get_drvdata(dev);
	struct rmi_fn_34_data *fn34data = (struct rmi_fn_34_data *)fn->rfi->fndata;
	unsigned long val;
	unsigned char cmd;
	int error;

	/* need to convert the string data to an actual value */
	error = strict_strtoul(buf, 10, &val);

	if (error)
		return error;

	fn34data->cmd = val;

	/* determine the proper command to issue.
	*/
	switch (val) {
	case ENABLE_FLASH_PROG:
		/* Issue a Flash Program Enable ($0F) command to the Flash Command
		(F34_Flash_Data3, bits 3:0) field.*/
		cmd = 0x0F;
		error = rmi_write_multiple(fn->sensor, fn->function->functionDataBaseAddr+3,
			(unsigned char *)&cmd, 1);
		if (error) {
			printk(KERN_ERR "%s : Could not write Flash Program Enable cmd to 0x%x\n",
				__func__, fn->function->functionDataBaseAddr+3);
			return error;
		}
		break;

	case ERASE_ALL:
		/* Issue a Erase All ($03) command to the Flash Command
		(F34_Flash_Data3, bits 3:0) field.*/
		cmd = 0x03;
		error = rmi_write_multiple(fn->sensor, fn->function->functionDataBaseAddr+3,
			(unsigned char *)&cmd, 1);
		if (error) {
			printk(KERN_ERR "%s : Could not write Erase All cmd to 0x%x\n",
				__func__, fn->function->functionDataBaseAddr+3);
			return error;
		}
		break;

	case ERASE_CONFIG:
		/* Issue a Erase Configuration ($07) command to the Flash Command
		(F34_Flash_Data3, bits 3:0) field.*/
		cmd = 0x07;
		error = rmi_write_multiple(fn->sensor, fn->function->functionDataBaseAddr+3,
			(unsigned char *)&cmd, 1);
		if (error) {
			printk(KERN_ERR "%s : Could not write Erase Configuration cmd to 0x%x\n",
				__func__, fn->function->functionDataBaseAddr+3);
			return error;
		}
		break;

	case WRITE_FW_BLOCK:
		/* Issue a Write Firmware Block ($02) command to the Flash Command
		(F34_Flash_Data3, bits 3:0) field.*/
		cmd = 0x02;
		error = rmi_write_multiple(fn->sensor, fn->function->functionDataBaseAddr+3,
			(unsigned char *)&cmd, 1);
		if (error) {
			printk(KERN_ERR "%s : Could not write Write Firmware Block cmd to 0x%x\n",
				__func__, fn->function->functionDataBaseAddr+3);
		return error;
		}
		break;

	case WRITE_CONFIG_BLOCK:
		/* Issue a Write Config Block ($06) command to the Flash Command
		(F34_Flash_Data3, bits 3:0) field.*/
		cmd = 0x06;
		error = rmi_write_multiple(fn->sensor, fn->function->functionDataBaseAddr+3,
			(unsigned char *)&cmd, 1);
		if (error) {
			printk(KERN_ERR "%s : Could not write Write Config Block cmd to 0x%x\n",
				__func__, fn->function->functionDataBaseAddr+3);
			return error;
		}
		break;

	case READ_CONFIG_BLOCK:
		/* Issue a Read Config Block ($05) command to the Flash Command
		(F34_Flash_Data3, bits 3:0) field.*/
		cmd = 0x05;
		error = rmi_write_multiple(fn->sensor, fn->function->functionDataBaseAddr+3,
			(unsigned char *)&cmd, 1);
		if (error) {
			printk(KERN_ERR "%s : Could not write Read Config Block cmd to 0x%x\n",
				__func__, fn->function->functionDataBaseAddr+3);
			return error;
		}
		break;

	case DISABLE_FLASH_PROG:
		/* Issue a reset command ($01) - this will reboot the sensor and ATTN will now go to
		the Fn $01 instead of the Fn $34 since the sensor will no longer be in Flash mode. */
		cmd = 0x01;
		/*if ((error = rmi_write_multiple(fn->sensor, fn->sensor->sensorCommandBaseAddr,
			(unsigned char *)&cmd, 1))) {
			printk(KERN_ERR "%s : Could not write Reset cmd to 0x%x\n",
				__func__, fn->sensor->sensorCommandBaseAddr);
		return error;
		}*/
		break;

	default:
		pr_debug("%s: RMI4 function $34 - unknown command.\n", __func__);
		break;
	}

	return count;
}

static ssize_t rmi_fn_34_data_read(struct file * filp,
				struct kobject *kobj,
				struct bin_attribute *attributes,
				char *buf, loff_t pos, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct rmi_function_device *fn = dev_get_drvdata(dev);
	int error;

	/* TODO: add check for count to verify it's the correct blocksize */

	/* read the data from flash into buf. */
	/* the app layer will be blocked at reading from the sysfs file. */
	/* when we return the count (or error if we fail) the app will resume. */
	error = rmi_read_multiple(fn->sensor, fn->function->functionDataBaseAddr+pos,
		(unsigned char *)buf, count);
	if (error) {
		printk(KERN_ERR "%s : Could not read data from 0x%llx\n",
			__func__, fn->function->functionDataBaseAddr+pos);
		return error;
	}

	return count;
}

static ssize_t rmi_fn_34_data_write(struct file *filp,
				struct kobject *kobj,
				struct bin_attribute *attributes,
				char *buf, loff_t pos, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct rmi_function_device *fn = dev_get_drvdata(dev);
	struct rmi_fn_34_data *fn34data = (struct rmi_fn_34_data *)fn->rfi->fndata;
	unsigned int blocknum;
	int error;

	/* write the data from buf to flash. */
	/* the app layer will be blocked at writing to the sysfs file. */
	/* when we return the count (or error if we fail) the app will resume. */

	/* TODO: Add check on count - if non-zero veriy it's the correct blocksize */

	/* Verify that the byte offset is always aligned on a block boundary and if not
	return an error.  We can't just use the mod operator % and do a (pos % fn34data->blocksize) because of a gcc
	bug that results in undefined symbols.  So we have to compute it the hard
	way.  Grumble. */
	unsigned int remainder;
	div_u64_rem(pos, fn34data->blocksize, &remainder);
	if (remainder) {
		printk(KERN_ERR "%s : Invalid byte offset of %llx leads to invalid block number.\n",
			__func__, pos);
		return -EINVAL;
	}

	/* Compute the block number using the byte offset (pos) and the block size.
	once again, we can't just do a divide due to a gcc bug. */
	blocknum = div_u64(pos, fn34data->blocksize);

	/* Write the block number first */
	error = rmi_write_multiple(fn->sensor, fn->function->functionDataBaseAddr,
		(unsigned char *)&blocknum, 2);
	if (error) {
		printk(KERN_ERR "%s : Could not write block number to 0x%x\n",
			__func__, fn->function->functionDataBaseAddr);
		return error;
	}

	/* Write the data block - only if the count is non-zero  */
	if (count) {
		error = rmi_write_multiple(fn->sensor, fn->function->functionDataBaseAddr+2,
			(unsigned char *)buf, count);
		if (error) {
			printk(KERN_ERR "%s : Could not write block data to 0x%x\n",
				__func__, fn->function->functionDataBaseAddr+2);
			return error;
		}
	}

	return count;
}
