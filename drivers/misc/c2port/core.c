/*
 *  Silicon Labs C2 port core Linux support
 *
 *  Copyright (c) 2007 Rodolfo Giometti <giometti@linux.it>
 *  Copyright (c) 2007 Eurotech S.p.A. <info@eurotech.it>
 *  Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/kmemcheck.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/idr.h>
#include <linux/sched.h>
#include <linux/slab.h>

#include <linux/c2port.h>

#define DRIVER_NAME             "c2port"
#define DRIVER_VERSION          "0.51.0"

static DEFINE_SPINLOCK(c2port_idr_lock);
static DEFINE_IDR(c2port_idr);

/*
 * Local variables
 */

static struct class *c2port_class;

/*
 * C2 registers & commands defines
 */

/* C2 registers */
#define C2PORT_DEVICEID		0x00
#define C2PORT_REVID		0x01
#define C2PORT_FPCTL		0x02
#define C2PORT_FPDAT		0xB4
#define C2PORT_XRAMD		0x84
#define C2PORT_XRAMAL		0xAD
#define C2PORT_XRAMAH		0xC7

/* C2 interface commands */
#define C2PORT_GET_VERSION	0x01
#define C2PORT_DEVICE_ERASE	0x03
#define C2PORT_BLOCK_READ	0x06
#define C2PORT_BLOCK_WRITE	0x07
#define C2PORT_PAGE_ERASE	0x08
#define C2PORT_DIRECT_READ	0x09
#define C2PORT_DIRECT_WRITE	0x0A
#define C2PORT_INDIRECT_READ	0x0B
#define C2PORT_INDIRECT_WRITE	0x0C

/* C2 status return codes */
#define C2PORT_INVALID_COMMAND	0x00
#define C2PORT_COMMAND_FAILED	0x02
#define C2PORT_COMMAND_OK	0x0d

/*
 * C2 port low level signal managements
 */

static void c2port_reset(struct c2port_device *dev)
{
	struct c2port_ops *ops = dev->ops;

	/* To reset the device we have to keep clock line low for at least
	 * 20us.
	 */
	local_irq_disable();
	ops->c2ck_set(dev, 0);
	udelay(25);
	ops->c2ck_set(dev, 1);
	local_irq_enable();

	udelay(1);
}

static void c2port_strobe_ck(struct c2port_device *dev)
{
	struct c2port_ops *ops = dev->ops;

	/* During hi-low-hi transition we disable local IRQs to avoid
	 * interructions since C2 port specification says that it must be
	 * shorter than 5us, otherwise the microcontroller may consider
	 * it as a reset signal!
	 */
	local_irq_disable();
	ops->c2ck_set(dev, 0);
	udelay(1);
	ops->c2ck_set(dev, 1);
	local_irq_enable();

	udelay(1);
}

/*
 * C2 port basic functions
 */

static void c2port_write_ar(struct c2port_device *dev, u8 addr)
{
	struct c2port_ops *ops = dev->ops;
	int i;

	/* START field */
	c2port_strobe_ck(dev);

	/* INS field (11b, LSB first) */
	ops->c2d_dir(dev, 0);
	ops->c2d_set(dev, 1);
	c2port_strobe_ck(dev);
	ops->c2d_set(dev, 1);
	c2port_strobe_ck(dev);

	/* ADDRESS field */
	for (i = 0; i < 8; i++) {
		ops->c2d_set(dev, addr & 0x01);
		c2port_strobe_ck(dev);

		addr >>= 1;
	}

	/* STOP field */
	ops->c2d_dir(dev, 1);
	c2port_strobe_ck(dev);
}

static int c2port_read_ar(struct c2port_device *dev, u8 *addr)
{
	struct c2port_ops *ops = dev->ops;
	int i;

	/* START field */
	c2port_strobe_ck(dev);

	/* INS field (10b, LSB first) */
	ops->c2d_dir(dev, 0);
	ops->c2d_set(dev, 0);
	c2port_strobe_ck(dev);
	ops->c2d_set(dev, 1);
	c2port_strobe_ck(dev);

	/* ADDRESS field */
	ops->c2d_dir(dev, 1);
	*addr = 0;
	for (i = 0; i < 8; i++) {
		*addr >>= 1;	/* shift in 8-bit ADDRESS field LSB first */

		c2port_strobe_ck(dev);
		if (ops->c2d_get(dev))
			*addr |= 0x80;
	}

	/* STOP field */
	c2port_strobe_ck(dev);

	return 0;
}

static int c2port_write_dr(struct c2port_device *dev, u8 data)
{
	struct c2port_ops *ops = dev->ops;
	int timeout, i;

	/* START field */
	c2port_strobe_ck(dev);

	/* INS field (01b, LSB first) */
	ops->c2d_dir(dev, 0);
	ops->c2d_set(dev, 1);
	c2port_strobe_ck(dev);
	ops->c2d_set(dev, 0);
	c2port_strobe_ck(dev);

	/* LENGTH field (00b, LSB first -> 1 byte) */
	ops->c2d_set(dev, 0);
	c2port_strobe_ck(dev);
	ops->c2d_set(dev, 0);
	c2port_strobe_ck(dev);

	/* DATA field */
	for (i = 0; i < 8; i++) {
		ops->c2d_set(dev, data & 0x01);
		c2port_strobe_ck(dev);

		data >>= 1;
	}

	/* WAIT field */
	ops->c2d_dir(dev, 1);
	timeout = 20;
	do {
		c2port_strobe_ck(dev);
		if (ops->c2d_get(dev))
			break;

		udelay(1);
	} while (--timeout > 0);
	if (timeout == 0)
		return -EIO;

	/* STOP field */
	c2port_strobe_ck(dev);

	return 0;
}

static int c2port_read_dr(struct c2port_device *dev, u8 *data)
{
	struct c2port_ops *ops = dev->ops;
	int timeout, i;

	/* START field */
	c2port_strobe_ck(dev);

	/* INS field (00b, LSB first) */
	ops->c2d_dir(dev, 0);
	ops->c2d_set(dev, 0);
	c2port_strobe_ck(dev);
	ops->c2d_set(dev, 0);
	c2port_strobe_ck(dev);

	/* LENGTH field (00b, LSB first -> 1 byte) */
	ops->c2d_set(dev, 0);
	c2port_strobe_ck(dev);
	ops->c2d_set(dev, 0);
	c2port_strobe_ck(dev);

	/* WAIT field */
	ops->c2d_dir(dev, 1);
	timeout = 20;
	do {
		c2port_strobe_ck(dev);
		if (ops->c2d_get(dev))
			break;

		udelay(1);
	} while (--timeout > 0);
	if (timeout == 0)
		return -EIO;

	/* DATA field */
	*data = 0;
	for (i = 0; i < 8; i++) {
		*data >>= 1;	/* shift in 8-bit DATA field LSB first */

		c2port_strobe_ck(dev);
		if (ops->c2d_get(dev))
			*data |= 0x80;
	}

	/* STOP field */
	c2port_strobe_ck(dev);

	return 0;
}

static int c2port_poll_in_busy(struct c2port_device *dev)
{
	u8 addr;
	int ret, timeout = 20;

	do {
		ret = (c2port_read_ar(dev, &addr));
		if (ret < 0)
			return -EIO;

		if (!(addr & 0x02))
			break;

		udelay(1);
	} while (--timeout > 0);
	if (timeout == 0)
		return -EIO;

	return 0;
}

static int c2port_poll_out_ready(struct c2port_device *dev)
{
	u8 addr;
	int ret, timeout = 10000; /* erase flash needs long time... */

	do {
		ret = (c2port_read_ar(dev, &addr));
		if (ret < 0)
			return -EIO;

		if (addr & 0x01)
			break;

		udelay(1);
	} while (--timeout > 0);
	if (timeout == 0)
		return -EIO;

	return 0;
}

/*
 * sysfs methods
 */

static ssize_t c2port_show_name(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct c2port_device *c2dev = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", c2dev->name);
}

static ssize_t c2port_show_flash_blocks_num(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct c2port_device *c2dev = dev_get_drvdata(dev);
	struct c2port_ops *ops = c2dev->ops;

	return sprintf(buf, "%d\n", ops->blocks_num);
}

static ssize_t c2port_show_flash_block_size(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct c2port_device *c2dev = dev_get_drvdata(dev);
	struct c2port_ops *ops = c2dev->ops;

	return sprintf(buf, "%d\n", ops->block_size);
}

static ssize_t c2port_show_flash_size(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct c2port_device *c2dev = dev_get_drvdata(dev);
	struct c2port_ops *ops = c2dev->ops;

	return sprintf(buf, "%d\n", ops->blocks_num * ops->block_size);
}

static ssize_t c2port_show_access(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct c2port_device *c2dev = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", c2dev->access);
}

static ssize_t c2port_store_access(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct c2port_device *c2dev = dev_get_drvdata(dev);
	struct c2port_ops *ops = c2dev->ops;
	int status, ret;

	ret = sscanf(buf, "%d", &status);
	if (ret != 1)
		return -EINVAL;

	mutex_lock(&c2dev->mutex);

	c2dev->access = !!status;

	/* If access is "on" clock should be HIGH _before_ setting the line
	 * as output and data line should be set as INPUT anyway */
	if (c2dev->access)
		ops->c2ck_set(c2dev, 1);
	ops->access(c2dev, c2dev->access);
	if (c2dev->access)
		ops->c2d_dir(c2dev, 1);

	mutex_unlock(&c2dev->mutex);

	return count;
}

static ssize_t c2port_store_reset(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct c2port_device *c2dev = dev_get_drvdata(dev);

	/* Check the device access status */
	if (!c2dev->access)
		return -EBUSY;

	mutex_lock(&c2dev->mutex);

	c2port_reset(c2dev);
	c2dev->flash_access = 0;

	mutex_unlock(&c2dev->mutex);

	return count;
}

static ssize_t __c2port_show_dev_id(struct c2port_device *dev, char *buf)
{
	u8 data;
	int ret;

	/* Select DEVICEID register for C2 data register accesses */
	c2port_write_ar(dev, C2PORT_DEVICEID);

	/* Read and return the device ID register */
	ret = c2port_read_dr(dev, &data);
	if (ret < 0)
		return ret;

	return sprintf(buf, "%d\n", data);
}

static ssize_t c2port_show_dev_id(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct c2port_device *c2dev = dev_get_drvdata(dev);
	ssize_t ret;

	/* Check the device access status */
	if (!c2dev->access)
		return -EBUSY;

	mutex_lock(&c2dev->mutex);
	ret = __c2port_show_dev_id(c2dev, buf);
	mutex_unlock(&c2dev->mutex);

	if (ret < 0)
		dev_err(dev, "cannot read from %s\n", c2dev->name);

	return ret;
}

static ssize_t __c2port_show_rev_id(struct c2port_device *dev, char *buf)
{
	u8 data;
	int ret;

	/* Select REVID register for C2 data register accesses */
	c2port_write_ar(dev, C2PORT_REVID);

	/* Read and return the revision ID register */
	ret = c2port_read_dr(dev, &data);
	if (ret < 0)
		return ret;

	return sprintf(buf, "%d\n", data);
}

static ssize_t c2port_show_rev_id(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct c2port_device *c2dev = dev_get_drvdata(dev);
	ssize_t ret;

	/* Check the device access status */
	if (!c2dev->access)
		return -EBUSY;

	mutex_lock(&c2dev->mutex);
	ret = __c2port_show_rev_id(c2dev, buf);
	mutex_unlock(&c2dev->mutex);

	if (ret < 0)
		dev_err(c2dev->dev, "cannot read from %s\n", c2dev->name);

	return ret;
}

static ssize_t c2port_show_flash_access(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct c2port_device *c2dev = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", c2dev->flash_access);
}

static ssize_t __c2port_store_flash_access(struct c2port_device *dev,
						int status)
{
	int ret;

	/* Check the device access status */
	if (!dev->access)
		return -EBUSY;

	dev->flash_access = !!status;

	/* If flash_access is off we have nothing to do... */
	if (dev->flash_access == 0)
		return 0;

	/* Target the C2 flash programming control register for C2 data
	 * register access */
	c2port_write_ar(dev, C2PORT_FPCTL);

	/* Write the first keycode to enable C2 Flash programming */
	ret = c2port_write_dr(dev, 0x02);
	if (ret < 0)
		return ret;

	/* Write the second keycode to enable C2 Flash programming */
	ret = c2port_write_dr(dev, 0x01);
	if (ret < 0)
		return ret;

	/* Delay for at least 20ms to ensure the target is ready for
	 * C2 flash programming */
	mdelay(25);

	return 0;
}

static ssize_t c2port_store_flash_access(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct c2port_device *c2dev = dev_get_drvdata(dev);
	int status;
	ssize_t ret;

	ret = sscanf(buf, "%d", &status);
	if (ret != 1)
		return -EINVAL;

	mutex_lock(&c2dev->mutex);
	ret = __c2port_store_flash_access(c2dev, status);
	mutex_unlock(&c2dev->mutex);

	if (ret < 0) {
		dev_err(c2dev->dev, "cannot enable %s flash programming\n",
			c2dev->name);
		return ret;
	}

	return count;
}

int mcu_halted(struct c2port_device *c2dev)
{
	u8 status;
	c2port_write_ar(c2dev, C2PORT_FPCTL);
	c2port_read_dr(c2dev, &status);
	if (status & 0x01)
		return 1;
	return 0;
}

static ssize_t c2port_show_ram_access(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct c2port_device *c2dev = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", mcu_halted(c2dev));
}

static ssize_t c2port_store_ram_access(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	ssize_t ret;
	int status;
	struct c2port_device *c2dev = dev_get_drvdata(dev);
	ret = sscanf(buf, "%d", &status);
	if (ret != 1) {
		dev_err(dev, "Invalid access parameter!\n");
		return -EINVAL;
	}
	mutex_lock(&c2dev->mutex);
	c2port_write_ar(c2dev, C2PORT_FPCTL);
	/* Halt device */
	if (status == 1)
		c2port_write_dr(c2dev, 0x01);
	else
		c2port_write_dr(c2dev, 0x00);
	mutex_unlock(&c2dev->mutex);
	return count;
}

static ssize_t c2port_store_rest_halt_access(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct c2port_device *c2dev = dev_get_drvdata(dev);

	c2port_reset(c2dev);
	c2port_write_ar(c2dev, C2PORT_FPCTL);
	c2port_write_dr(c2dev, 0x02);
	c2port_write_dr(c2dev, 0x04);
	c2port_write_dr(c2dev, 0x01);

	return count;
}

static ssize_t __c2port_write_flash_erase(struct c2port_device *dev)
{
	u8 status;
	int ret;

	/* Target the C2 flash programming data register for C2 data register
	 * access.
	 */
	c2port_write_ar(dev, C2PORT_FPDAT);

	/* Send device erase command */
	c2port_write_dr(dev, C2PORT_DEVICE_ERASE);

	/* Wait for input acknowledge */
	ret = c2port_poll_in_busy(dev);
	if (ret < 0)
		return ret;

	/* Should check status before starting FLASH access sequence */

	/* Wait for status information */
	ret = c2port_poll_out_ready(dev);
	if (ret < 0)
		return ret;

	/* Read flash programming interface status */
	ret = c2port_read_dr(dev, &status);
	if (ret < 0)
		return ret;
	if (status != C2PORT_COMMAND_OK)
		return -EBUSY;

	/* Send a three-byte arming sequence to enable the device erase.
	 * If the sequence is not received correctly, the command will be
	 * ignored.
	 * Sequence is: 0xde, 0xad, 0xa5.
	 */
	c2port_write_dr(dev, 0xde);
	ret = c2port_poll_in_busy(dev);
	if (ret < 0)
		return ret;
	c2port_write_dr(dev, 0xad);
	ret = c2port_poll_in_busy(dev);
	if (ret < 0)
		return ret;
	c2port_write_dr(dev, 0xa5);
	ret = c2port_poll_in_busy(dev);
	if (ret < 0)
		return ret;

	ret = c2port_poll_out_ready(dev);
	if (ret < 0)
		return ret;

	return 0;
}

static ssize_t c2port_store_flash_erase(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct c2port_device *c2dev = dev_get_drvdata(dev);
	int ret;

	/* Check the device and flash access status */
	if (!c2dev->access || !c2dev->flash_access)
		return -EBUSY;

	mutex_lock(&c2dev->mutex);
	ret = __c2port_write_flash_erase(c2dev);
	mutex_unlock(&c2dev->mutex);

	if (ret < 0) {
		dev_err(c2dev->dev, "cannot erase %s flash\n", c2dev->name);
		return ret;
	}

	return count;
}

static ssize_t __c2port_read_flash_data(struct c2port_device *dev,
				char *buffer, loff_t offset, size_t count)
{
	struct c2port_ops *ops = dev->ops;
	u8 status, nread = 128;
	int i, ret;

	/* Check for flash end */
	if (offset >= ops->block_size * ops->blocks_num)
		return 0;

	if (ops->block_size * ops->blocks_num - offset < nread)
		nread = ops->block_size * ops->blocks_num - offset;
	if (count < nread)
		nread = count;
	if (nread == 0)
		return nread;

	/* Target the C2 flash programming data register for C2 data register
	 * access */
	c2port_write_ar(dev, C2PORT_FPDAT);

	/* Send flash block read command */
	c2port_write_dr(dev, C2PORT_BLOCK_READ);

	/* Wait for input acknowledge */
	ret = c2port_poll_in_busy(dev);
	if (ret < 0)
		return ret;

	/* Should check status before starting FLASH access sequence */

	/* Wait for status information */
	ret = c2port_poll_out_ready(dev);
	if (ret < 0)
		return ret;

	/* Read flash programming interface status */
	ret = c2port_read_dr(dev, &status);
	if (ret < 0)
		return ret;
	if (status != C2PORT_COMMAND_OK)
		return -EBUSY;

	/* Send address high byte */
	c2port_write_dr(dev, offset >> 8);
	ret = c2port_poll_in_busy(dev);
	if (ret < 0)
		return ret;

	/* Send address low byte */
	c2port_write_dr(dev, offset & 0x00ff);
	ret = c2port_poll_in_busy(dev);
	if (ret < 0)
		return ret;

	/* Send address block size */
	c2port_write_dr(dev, nread);
	ret = c2port_poll_in_busy(dev);
	if (ret < 0)
		return ret;

	/* Should check status before reading FLASH block */

	/* Wait for status information */
	ret = c2port_poll_out_ready(dev);
	if (ret < 0)
		return ret;

	/* Read flash programming interface status */
	ret = c2port_read_dr(dev, &status);
	if (ret < 0)
		return ret;
	if (status != C2PORT_COMMAND_OK)
		return -EBUSY;

	/* Read flash block */
	for (i = 0; i < nread; i++) {
		ret = c2port_poll_out_ready(dev);
		if (ret < 0)
			return ret;

		ret = c2port_read_dr(dev, buffer+i);
		if (ret < 0)
			return ret;
	}

	return nread;
}

static ssize_t c2port_read_flash_data(struct file *filp, struct kobject *kobj,
				struct bin_attribute *attr,
				char *buffer, loff_t offset, size_t count)
{
	struct c2port_device *c2dev =
			dev_get_drvdata(container_of(kobj,
						struct device, kobj));
	ssize_t ret;

	/* Check the device and flash access status */
	if (!c2dev->access || !c2dev->flash_access)
		return -EBUSY;

	mutex_lock(&c2dev->mutex);
	ret = __c2port_read_flash_data(c2dev, buffer, offset, count);
	mutex_unlock(&c2dev->mutex);

	if (ret < 0)
		dev_err(c2dev->dev, "cannot read %s flash\n", c2dev->name);

	return ret;
}

static ssize_t __c2port_write_flash_data(struct c2port_device *dev,
				char *buffer, loff_t offset, size_t count)
{
	struct c2port_ops *ops = dev->ops;
	u8 status, nwrite = 128;
	int i, ret;

	if (nwrite > count)
		nwrite = count;
	if (ops->block_size * ops->blocks_num - offset < nwrite)
		nwrite = ops->block_size * ops->blocks_num - offset;

	/* Check for flash end */
	if (offset >= ops->block_size * ops->blocks_num)
		return -EINVAL;

	/* Target the C2 flash programming data register for C2 data register
	 * access */
	c2port_write_ar(dev, C2PORT_FPDAT);

	/* Send flash block write command */
	c2port_write_dr(dev, C2PORT_BLOCK_WRITE);

	/* Wait for input acknowledge */
	ret = c2port_poll_in_busy(dev);
	if (ret < 0)
		return ret;

	/* Should check status before starting FLASH access sequence */

	/* Wait for status information */
	ret = c2port_poll_out_ready(dev);
	if (ret < 0)
		return ret;

	/* Read flash programming interface status */
	ret = c2port_read_dr(dev, &status);
	if (ret < 0)
		return ret;
	if (status != C2PORT_COMMAND_OK)
		return -EBUSY;

	/* Send address high byte */
	c2port_write_dr(dev, offset >> 8);
	ret = c2port_poll_in_busy(dev);
	if (ret < 0)
		return ret;

	/* Send address low byte */
	c2port_write_dr(dev, offset & 0x00ff);
	ret = c2port_poll_in_busy(dev);
	if (ret < 0)
		return ret;

	/* Send address block size */
	c2port_write_dr(dev, nwrite);
	ret = c2port_poll_in_busy(dev);
	if (ret < 0)
		return ret;

	/* Should check status before writing FLASH block */

	/* Wait for status information */
	ret = c2port_poll_out_ready(dev);
	if (ret < 0)
		return ret;

	/* Read flash programming interface status */
	ret = c2port_read_dr(dev, &status);
	if (ret < 0)
		return ret;
	if (status != C2PORT_COMMAND_OK)
		return -EBUSY;

	/* Write flash block */
	for (i = 0; i < nwrite; i++) {
		ret = c2port_write_dr(dev, *(buffer+i));
		if (ret < 0)
			return ret;

		ret = c2port_poll_in_busy(dev);
		if (ret < 0)
			return ret;

	}

	/* Wait for last flash write to complete */
	ret = c2port_poll_out_ready(dev);
	if (ret < 0)
		return ret;

	return nwrite;
}

static ssize_t c2port_write_flash_data(struct file *filp, struct kobject *kobj,
				struct bin_attribute *attr,
				char *buffer, loff_t offset, size_t count)
{
	struct c2port_device *c2dev =
			dev_get_drvdata(container_of(kobj,
						struct device, kobj));
	int ret;

	/* Check the device access status */
	if (!c2dev->access || !c2dev->flash_access)
		return -EBUSY;

	mutex_lock(&c2dev->mutex);
	ret = __c2port_write_flash_data(c2dev, buffer, offset, count);
	mutex_unlock(&c2dev->mutex);

	if (ret < 0)
		dev_err(c2dev->dev, "cannot write %s flash\n", c2dev->name);

	return ret;
}

/* helper function to implement ram/sfr/xram read/write */
/* is_8bit is used for detect overflow in address format*/
static inline bool is_8bit(u16 word)
{
	if (word & 0xff00)
		return false;
	return true;
}

static inline bool addr_valid8bit(u16 addr, u16 size)
{
	return is_8bit(addr) && is_8bit(size) &&
		is_8bit(addr + size - 1);
}
/* Get address space size */
static ssize_t get_ram_size(struct c2port_device *dev)
{
	return dev->ops->ram_size;
}

static ssize_t get_sfr_size(struct c2port_device *dev)
{
	return dev->ops->sfr_size;
}

static ssize_t get_xram_size(struct c2port_device *dev)
{
	return dev->ops->xram_size;
}

/* calculate how many bytes to read/write */
static ssize_t calc_bytes(loff_t start, u16 mem_size, u16 expected_size)
{
	if (start > mem_size)
		return 0;
	if (start + expected_size > mem_size)
		return mem_size - start;
	return expected_size;
}

/* Send target address to C2 port, format: 1byte addr, 1byte size */
static ssize_t send_addr_ram(struct c2port_device *dev, u16 addr, u16 size)
{
	int ret;

	/* Overflow dectction */
	if (!addr_valid8bit(addr, size)) {
		dev_err(dev->dev, "Address/size overflow!\n");
		return -EINVAL;
	}

	/* Write start address */
	c2port_write_dr(dev, addr);
	ret = c2port_poll_in_busy(dev);
	if (ret < 0)
		return ret;

	/* Send address block size */
	c2port_write_dr(dev, size);
	ret = c2port_poll_in_busy(dev);
	return ret;
}
/* Externl RAM address format: 2byte addr, size not used for XRAM */
/* Address is increased automatically when write/read to XRAMD */
static ssize_t send_addr_xram(struct c2port_device *dev, u16 addr, u16 size)
{
	int ret;

	/* Write start address */
	/* Low byte */
	c2port_write_ar(dev, C2PORT_XRAMAL);
	c2port_write_dr(dev, (u8) addr);
	ret = c2port_poll_in_busy(dev);
	if (ret < 0)
		return ret;
	/* High byte */
	c2port_write_ar(dev, C2PORT_XRAMAH);
	c2port_write_dr(dev, (u8) (addr >> 8));
	ret = c2port_poll_in_busy(dev);
	if (ret < 0)
		return ret;

	return 0;
}
/* Write FPDAT command, XRAM manipulation doesn't need it */
static ssize_t write_command(struct c2port_device *c2dev, u8 command)
{
	int ret;
	u8 status;

	c2port_write_ar(c2dev, C2PORT_FPDAT);
	c2port_write_dr(c2dev, command);

	/* Wait for command status */
	ret = c2port_poll_in_busy(c2dev);
	if (ret < 0)
		return ret;

	/* Wait for status information */
	ret = c2port_poll_out_ready(c2dev);
	if (ret < 0)
		return ret;

	/* Read command status */
	ret = c2port_read_dr(c2dev, &status);
	if (ret < 0)
		return ret;
	if (status != C2PORT_COMMAND_OK)
		return -EBUSY;
	return 0;
}
/* Read bytes for SFR/internal memory */
static ssize_t read_bytes_common(struct c2port_device *dev, char *buffer,
						 u16 nread)
{
	int i;
	u8 ret;
	/* Read flash block */
	for (i = 0; i < nread; i++) {
		ret = c2port_poll_out_ready(dev);
		if (ret < 0)
			return ret;

		ret = c2port_read_dr(dev, buffer + i);
		if (ret < 0)
			return ret;
	}
	return nread;
}

static ssize_t write_bytes_common(struct c2port_device *dev, char *buffer,
							u16 nwrite)
{
	int i;
	int ret;

	for (i = 0; i < nwrite; i++) {
		ret = c2port_write_dr(dev, *(buffer + i));
		if (ret < 0)
			return ret;

		ret = c2port_poll_in_busy(dev);
		if (ret < 0)
			return ret;
	}
	return nwrite;
}

static ssize_t read_bytes_xram(struct c2port_device *dev, char *buffer,
							u16 nread)
{
	int i;
	u8 ret;
	/* Switch to read/write of XRAMD */
	c2port_write_ar(dev, C2PORT_XRAMD);

	for (i = 0; i < nread; i++) {
		/* No need to poll out ready */
		ret = c2port_read_dr(dev, buffer + i);
		if (ret < 0)
			return ret;
	}
	return nread;
}

static ssize_t write_bytes_xram(struct c2port_device *dev, char *buffer,
							u16 nwrite)
{
	int i;
	int ret;
	/* Switch to read/write of XRAMD */
	c2port_write_ar(dev, C2PORT_XRAMD);

	for (i = 0; i < nwrite; i++) {
		ret = c2port_write_dr(dev, *(buffer + i));
		if (ret < 0)
			return ret;
		/* No needn to poll in busy */
	}
	return nwrite;
}

/* desciption for every operation */
struct c2port_internal_operation {
	/* address access type */
	ssize_t (*get_addr_size)(struct c2port_device *dev);
	/* Which command to write to FPDAT register */
	u8 command;
	/* Send address and how many bytes/blocks to read/write */
	ssize_t (*send_addr_and_count) (struct c2port_device *dev,
					u16 start_addr, u16 count);
	/* functions to execute read/write */
	ssize_t (*operation) (struct c2port_device *dev, char *buffer,
				u16 count);
};

/* Operations for manipulating internal ram */
static struct c2port_internal_operation read_iram_ops = {
	.get_addr_size = get_ram_size,
	.command = C2PORT_INDIRECT_READ,
	.send_addr_and_count = send_addr_ram,
	.operation = read_bytes_common
};

static struct c2port_internal_operation write_iram_ops = {
	.get_addr_size = get_ram_size,
	.command = C2PORT_INDIRECT_WRITE,
	.send_addr_and_count = send_addr_ram,
	.operation = write_bytes_common
};

/* Operations for manipulating special function registers */
static struct c2port_internal_operation read_sfr_ops = {
	.get_addr_size = get_sfr_size,
	.command = C2PORT_DIRECT_READ,
	.send_addr_and_count = send_addr_ram,
	.operation = read_bytes_common
};

static struct c2port_internal_operation write_sfr_ops = {
	.get_addr_size = get_sfr_size,
	.command = C2PORT_DIRECT_WRITE,
	.send_addr_and_count = send_addr_ram,
	.operation = write_bytes_common
};

/* Operations for manipulating XRAM and USB FIFO */
static struct c2port_internal_operation read_xram_ops = {
	.get_addr_size = get_xram_size,
	/* No FPDAT operation */
	.command = 0,
	.send_addr_and_count = send_addr_xram,
	.operation = read_bytes_xram
};

static struct c2port_internal_operation write_xram_ops = {
	.get_addr_size = get_xram_size,
	/* No FPDAT operation */
	.command = 0,
	.send_addr_and_count = send_addr_xram,
	.operation = write_bytes_xram
};

/* General framework manipulating RAM/SFR/XRAM */
static ssize_t c2port_operation_execute(struct c2port_internal_operation *c2op,
					struct file *filp, struct kobject *kobj,
					char *buffer, loff_t offset,
					size_t count)
{
	struct c2port_device *c2dev = dev_get_drvdata(container_of(kobj,
						struct  device, kobj));
	ssize_t ret;
	u16 nreadwrite = (u16)count;

	if (!c2dev->access || !mcu_halted(c2dev))
		return -EBUSY;

	mutex_lock(&c2dev->mutex);

	nreadwrite = calc_bytes(offset, c2op->get_addr_size(c2dev), nreadwrite);
	if (nreadwrite == 0) {
		ret = 0;
		goto out;
	}
	/* Send command to FPDAT */
	if (c2op->command != 0) {
		ret = write_command(c2dev, c2op->command);
		if (ret < 0) {
			dev_err(c2dev->dev, "cannot write %s C2 command\n",
					c2dev->name);
			goto out;
		}
	}

	/* Send address and count */
	ret = c2op->send_addr_and_count(c2dev, offset, nreadwrite);
	if (ret < 0) {
		dev_err(c2dev->dev, "cannot write %s C2 ram adress: 0x%02x\n",
				c2dev->name, (int)offset);
		goto out;
	}
	/* do read/write */
	ret = c2op->operation(c2dev, buffer, nreadwrite);
	if (ret < 0)
		dev_err(c2dev->dev, "cannot manipulate %s C2 ram data\n",
				c2dev->name);
out:
	mutex_unlock(&c2dev->mutex);
	return ret;
}

static ssize_t c2port_read_internal_ram(struct file *filp, struct kobject *kobj,
					struct bin_attribute *attr,
					char *buffer, loff_t offset,
					size_t count)
{
	return c2port_operation_execute(&read_iram_ops, filp, kobj, buffer,
					offset, count);
}

static ssize_t c2port_write_internal_ram(struct file *filp,
					 struct kobject *kobj,
					 struct bin_attribute *attr,
					 char *buffer, loff_t offset,
					 size_t count)
{
	return c2port_operation_execute(&write_iram_ops, filp, kobj, buffer,
					offset, count);
}

static ssize_t c2port_read_sfr(struct file *filp, struct kobject *kobj,
				struct bin_attribute *attr,
				char *buffer, loff_t offset, size_t count)
{
	return c2port_operation_execute(&read_sfr_ops, filp, kobj, buffer,
					offset, count);
}

static ssize_t c2port_write_sfr(struct file *filp, struct kobject *kobj,
				struct bin_attribute *attr,
				char *buffer, loff_t offset, size_t count)
{
	return c2port_operation_execute(&write_sfr_ops, filp, kobj, buffer,
					offset, count);
}

static ssize_t c2port_read_xram(struct file *filp, struct kobject *kobj,
				struct bin_attribute *attr,
				char *buffer, loff_t offset, size_t count)
{
	return c2port_operation_execute(&read_xram_ops, filp, kobj, buffer,
					offset, count);
}

static ssize_t c2port_write_xram(struct file *filp, struct kobject *kobj,
				 struct bin_attribute *attr,
				 char *buffer, loff_t offset, size_t count)
{
	return c2port_operation_execute(&write_xram_ops, filp, kobj, buffer,
					offset, count);
}

/*
 * Class attributes
 */

static struct device_attribute c2port_attrs[] = {
	__ATTR(name, 0444, c2port_show_name, NULL),
	__ATTR(flash_blocks_num, 0444, c2port_show_flash_blocks_num, NULL),
	__ATTR(flash_block_size, 0444, c2port_show_flash_block_size, NULL),
	__ATTR(flash_size, 0444, c2port_show_flash_size, NULL),
	__ATTR(access, 0644, c2port_show_access, c2port_store_access),
	__ATTR(reset, 0200, NULL, c2port_store_reset),
	__ATTR(dev_id, 0444, c2port_show_dev_id, NULL),
	__ATTR(rev_id, 0444, c2port_show_rev_id, NULL),

	__ATTR(flash_access, 0644, c2port_show_flash_access,
					c2port_store_flash_access),
	__ATTR(flash_erase, 0200, NULL, c2port_store_flash_erase),
	/* before read/write to ram/sfr/xram, should halt mcu */
	__ATTR(ram_access, 0644, c2port_show_ram_access,
					c2port_store_ram_access),
	/* reset the device and then halt it */
	__ATTR(reset_halt, 0200, NULL,
						c2port_store_rest_halt_access),
	__ATTR_NULL,
};

static struct bin_attribute c2port_bin_attrs = {
	.attr	= {
		.name	= "flash_data",
		.mode	= 0644
	},
	.read	= c2port_read_flash_data,
	.write	= c2port_write_flash_data,
	/* .size is computed at run-time */
};

/* Add supports for ram read/write */
static struct bin_attribute c2port_bin_ram_attrs = {
	.attr = {
		.name = "internal_ram",
		.mode = 0644},
	.read = c2port_read_internal_ram,
	.write = c2port_write_internal_ram
};

/* Add supports for sfr read/write */
static struct bin_attribute c2port_bin_sfr_attrs = {
	.attr = {
		 .name = "sfr",
		 .mode = 0644},
	.read = c2port_read_sfr,
	.write = c2port_write_sfr
};

/* Add supports for xram read/write */
static struct bin_attribute c2port_bin_xram_attrs = {
	.attr = {
		 .name = "xram",
		 .mode = 0644},
	.read = c2port_read_xram,
	.write = c2port_write_xram
};

/*
 * Exported functions
 */

struct c2port_device *c2port_device_register(char *name,
					struct c2port_ops *ops, void *devdata)
{
	struct c2port_device *c2dev;
	int ret;

	if (unlikely(!ops) || unlikely(!ops->access) || \
		unlikely(!ops->c2d_dir) || unlikely(!ops->c2ck_set) || \
		unlikely(!ops->c2d_get) || unlikely(!ops->c2d_set))
		return ERR_PTR(-EINVAL);

	c2dev = kmalloc(sizeof(struct c2port_device), GFP_KERNEL);
	kmemcheck_annotate_bitfield(c2dev, flags);
	if (unlikely(!c2dev))
		return ERR_PTR(-ENOMEM);

	idr_preload(GFP_KERNEL);
	spin_lock_irq(&c2port_idr_lock);
	ret = idr_alloc(&c2port_idr, c2dev, 0, 0, GFP_NOWAIT);
	spin_unlock_irq(&c2port_idr_lock);
	idr_preload_end();

	if (ret < 0)
		goto error_idr_alloc;
	c2dev->id = ret;

	c2dev->dev = device_create(c2port_class, NULL, 0, c2dev,
				   "c2port%d", c2dev->id);
	if (unlikely(IS_ERR(c2dev->dev))) {
		ret = PTR_ERR(c2dev->dev);
		goto error_device_create;
	}
	dev_set_drvdata(c2dev->dev, c2dev);

	strncpy(c2dev->name, name, C2PORT_NAME_LEN);
	c2dev->ops = ops;
	mutex_init(&c2dev->mutex);

	/* Create binary file */
	c2port_bin_attrs.size = ops->blocks_num * ops->block_size;
	ret = device_create_bin_file(c2dev->dev, &c2port_bin_attrs);
	if (unlikely(ret))
		goto error_device_create_bin_file;

	/* Create binary file for ram read/write */
	if (ops->sfr_size) {
		c2port_bin_ram_attrs.size = ops->ram_size;
		ret = device_create_bin_file(c2dev->dev, &c2port_bin_ram_attrs);
		if (unlikely(ret))
			goto error_device_create_bin_file;
	}

	/* Create binary file for sfr read/write */
	if (ops->sfr_size) {
		c2port_bin_sfr_attrs.size = ops->sfr_size;
		ret = device_create_bin_file(c2dev->dev, &c2port_bin_sfr_attrs);
		if (unlikely(ret))
			goto error_device_create_bin_file;
	}

	/* Create binary file for xram read/write */
	if (ops->xram_size) {
		c2port_bin_xram_attrs.size = ops->xram_size;
		ret = device_create_bin_file(c2dev->dev,
				&c2port_bin_xram_attrs);
		if (unlikely(ret))
			goto error_device_create_bin_file;
	}

	/* By default C2 port access is off */
	c2dev->access = c2dev->flash_access = 0;
	ops->access(c2dev, 0);

	dev_info(c2dev->dev, "C2 port %s added\n", name);
	dev_info(c2dev->dev, "%s flash has %d blocks x %d bytes "
				"(%d bytes total)\n",
				name, ops->blocks_num, ops->block_size,
				ops->blocks_num * ops->block_size);

	return c2dev;

error_device_create_bin_file:
	device_destroy(c2port_class, 0);

error_device_create:
	spin_lock_irq(&c2port_idr_lock);
	idr_remove(&c2port_idr, c2dev->id);
	spin_unlock_irq(&c2port_idr_lock);

error_idr_alloc:
	kfree(c2dev);

	return ERR_PTR(ret);
}
EXPORT_SYMBOL(c2port_device_register);

void c2port_device_unregister(struct c2port_device *c2dev)
{
	if (!c2dev)
		return;

	dev_info(c2dev->dev, "C2 port %s removed\n", c2dev->name);

	device_remove_bin_file(c2dev->dev, &c2port_bin_attrs);
	spin_lock_irq(&c2port_idr_lock);
	idr_remove(&c2port_idr, c2dev->id);
	spin_unlock_irq(&c2port_idr_lock);

	device_destroy(c2port_class, c2dev->id);

	kfree(c2dev);
}
EXPORT_SYMBOL(c2port_device_unregister);

/*
 * Module stuff
 */

static int __init c2port_init(void)
{
	printk(KERN_INFO "Silicon Labs C2 port support v. " DRIVER_VERSION
		" - (C) 2007 Rodolfo Giometti\n");

	c2port_class = class_create(THIS_MODULE, "c2port");
	if (IS_ERR(c2port_class)) {
		printk(KERN_ERR "c2port: failed to allocate class\n");
		return PTR_ERR(c2port_class);
	}
	c2port_class->dev_attrs = c2port_attrs;

	return 0;
}

static void __exit c2port_exit(void)
{
	class_destroy(c2port_class);
}

module_init(c2port_init);
module_exit(c2port_exit);

MODULE_AUTHOR("Rodolfo Giometti <giometti@linux.it>");
MODULE_DESCRIPTION("Silicon Labs C2 port support v. " DRIVER_VERSION);
MODULE_LICENSE("GPL");
