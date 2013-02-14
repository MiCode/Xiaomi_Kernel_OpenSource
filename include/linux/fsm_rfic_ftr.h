/* Copyright (c) 2010-2012, The Linux Foundation. All rights reserved.
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

#ifndef _FSM_RFIC_FTR_H_
#define _FSM_RFIC_FTR_H_

#include <linux/ioctl.h>

/*
 * Device interface
 */

#define RFIC_FTR_DEVICE_NAME		"rfic_ftr"

/*
 * IOCTL interface
 */

/*
    Macro to associate the "bus" and "address" pair when accessing the RFIC.
    Using a 32 bit address, reserve the upper 8 bits for the bus value, and
    the lower 24 bits for the address.
 */
#define RFIC_FTR_ADDR(bus, addr)	(((bus&0x03)<<24)|(addr&0xFFFFFF))
#define RFIC_FTR_GET_ADDR(busAddr)	(busAddr&0xFFFFFF)
#define RFIC_FTR_GET_BUS(busAddr)	((busAddr>>24)&0x03)

struct rfic_write_register_param {
	unsigned int rficAddr;
	unsigned int value;
};

struct rfic_write_register_mask_param {
	unsigned int rficAddr;
	unsigned int value;
	unsigned int mask;
};

struct rfic_grfc_param {
	unsigned int grfcId;
	unsigned int maskValue;
	unsigned int ctrlValue;
};

#define RFIC_IOCTL_MAGIC				'f'
#define RFIC_IOCTL_READ_REGISTER \
	_IOC(_IOC_READ, RFIC_IOCTL_MAGIC, 0x01, \
		sizeof(unsigned int *))
#define RFIC_IOCTL_WRITE_REGISTER \
	_IOC(_IOC_WRITE, RFIC_IOCTL_MAGIC, 0x02, \
		sizeof(struct rfic_write_register_param *))
#define RFIC_IOCTL_WRITE_REGISTER_WITH_MASK \
	_IOC(_IOC_WRITE, RFIC_IOCTL_MAGIC, 0x03, \
		sizeof(struct rfic_write_register_mask_param *))
#define RFIC_IOCTL_GET_GRFC \
	_IOC(_IOC_WRITE, RFIC_IOCTL_MAGIC, 0x10, \
		sizeof(struct rfic_grfc_param *))
#define RFIC_IOCTL_SET_GRFC \
	_IOC(_IOC_WRITE, RFIC_IOCTL_MAGIC, 0x11, \
		sizeof(struct rfic_grfc_param *))

#endif /* _FSM_RFIC_FTR_H_ */
