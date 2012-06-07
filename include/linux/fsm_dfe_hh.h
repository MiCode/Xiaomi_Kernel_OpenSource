/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _FSM_DFE_HH_H_
#define _FSM_DFE_HH_H_

#include <linux/ioctl.h>

/*
 * Device interface
 */

#define DFE_HH_DEVICE_NAME		"dfe_hh"

/*
 * IOCTL interface
 */

enum {
	DFE_IOCTL_COMMAND_CODE_WRITE,
	DFE_IOCTL_COMMAND_CODE_WRITE_WITH_MASK,
};

struct dfe_write_register_param {
	unsigned int offset;
	unsigned int value;
};

struct dfe_write_register_mask_param {
	unsigned int offset;
	unsigned int value;
	unsigned int mask;
};

struct dfe_read_write_array_param {
	unsigned int offset;
	unsigned int num; /* number of 16 bit registers */
	unsigned int *pArray;
};

struct dfe_command_entry {
	unsigned int code;
	unsigned int offset;
	unsigned int value;
	unsigned int mask; /* DFE_IOCTL_COMMAND_CODE_WRITE_WITH_MASK only */
};

struct dfe_command_param {
	unsigned int num;
	struct dfe_command_entry *pEntry;
};

#define DFE_IOCTL_MAGIC				'h'
#define DFE_IOCTL_READ_REGISTER \
	_IOC(_IOC_READ, DFE_IOCTL_MAGIC, 0x01, \
		sizeof(unsigned int *))
#define DFE_IOCTL_WRITE_REGISTER \
	_IOC(_IOC_WRITE, DFE_IOCTL_MAGIC, 0x02, \
		sizeof(struct dfe_write_register_param *))
#define DFE_IOCTL_WRITE_REGISTER_WITH_MASK \
	_IOC(_IOC_WRITE, DFE_IOCTL_MAGIC, 0x03, \
		sizeof(struct dfe_write_register_mask_param *))
#define DFE_IOCTL_READ_REGISTER_ARRAY \
	_IOC(_IOC_READ, DFE_IOCTL_MAGIC, 0x04, \
		sizeof(struct dfe_read_write_array_param *))
#define DFE_IOCTL_WRITE_REGISTER_ARRAY \
	_IOC(_IOC_WRITE, DFE_IOCTL_MAGIC, 0x05, \
		sizeof(struct dfe_read_write_array_param *))
#define DFE_IOCTL_COMMAND \
	_IOC(_IOC_WRITE, DFE_IOCTL_MAGIC, 0x10, \
		sizeof(struct dfe_command_param *))

#endif /* _FSM_DFE_HH_H_ */
