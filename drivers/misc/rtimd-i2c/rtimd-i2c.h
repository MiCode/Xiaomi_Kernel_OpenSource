/*
 * File name: rtimd-i2c.h
 *
 * Description : RAONTECH Micro Display I2C driver.
 *
 * Copyright (C) (2017, RAONTECH)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __RTIMD_I2C_H__
#define __RTIMD_I2C_H__

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#include <linux/ioctl.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/atomic.h>
#include <linux/i2c.h>
#include <linux/list.h>
#include <linux/jiffies.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/gpio.h>
#include <linux/compat.h>
#include <linux/ctype.h>

#define RMDERR(fmt, args...) \
	pr_err("RTI_MD: %s(): " fmt, __func__, ## args)

// pr_debug was not output msg......
#define RMDDBG(fmt, args...) \
	pr_err("RTI_MD: %s(): " fmt, __func__, ## args)


/* device file name */
#define RTI_MD_DEV_NAME		"rtimd-i2c"

/* device number */
#define RTI_MD_MAJOR_NR         460	  /* MAJOR No. */
#define RTI_MD_MINOR_NR         200       /* MINOR No. */

#define MAX_RTIMD_REG_DATA_SIZE		PAGE_SIZE /* 4KB Kernel page size */

/* RDC Control Block */
struct RTIMD_CB_T {
	atomic_t open_flag; /* to open only once */

	uint8_t	*read_buf;
	uint8_t	*write_buf;

	int bus_num;
	struct i2c_adapter *adap;
};

#define RTIMD_SINGLE_IO_MODE	1
#define RTIMD_BURST_IO_MODE		0

/*
 * NOTE: Force align to 64-bit to compat 32-bit application.
 */
struct RTIMD_SINGLE_WRITE_REG_T {
	uint32_t reg_addr;
	uint8_t bus_num;
	uint8_t slave_addr;
	uint8_t reg_size;
	uint8_t data;
};

struct RTIMD_BURST_WRITE_REG_T {
	uint64_t wbuf_addr;

	uint8_t bus_num;
	uint8_t slave_addr;
	uint16_t wsize;
	uint32_t pad;
};

struct RTIMD_SINGLE_READ_REG_T {
	uint64_t rbuf_addr;

	uint32_t reg_addr;
	uint8_t bus_num;
	uint8_t slave_addr;
	uint8_t reg_size;
	uint8_t pad;
};

struct RTIMD_BURST_READ_REG_T {
	uint64_t wbuf_addr;
	uint64_t rbuf_addr;

	uint16_t wsize;
	uint16_t rsize;
	uint8_t bus_num;
	uint8_t slave_addr;
	uint16_t pad;
};

#define RTIMD_IOC_MAGIC	'R'

#define IOCTL_RTIMD_SINGLE_READ		_IOWR(RTIMD_IOC_MAGIC, 1, struct RTIMD_SINGLE_READ_REG_T)
#define IOCTL_RTIMD_BURST_READ		_IOWR(RTIMD_IOC_MAGIC, 2, struct RTIMD_BURST_READ_REG_T)
#define IOCTL_RTIMD_SINGLE_WRITE	_IOWR(RTIMD_IOC_MAGIC, 3, struct RTIMD_SINGLE_WRITE_REG_T)
#define IOCTL_RTIMD_BURST_WRITE		_IOWR(RTIMD_IOC_MAGIC, 4, struct RTIMD_BURST_WRITE_REG_T)

#endif /* __RTIMD_I2C_H__ */

