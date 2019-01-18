/*
** =============================================================================
** Copyright (c) 2016  Texas Instruments Inc.
** Copyright (C) 2019 XiaoMi, Inc.
**
** This program is free software; you can redistribute it and/or modify it under
** the terms of the GNU General Public License as published by the Free Software
** Foundation; version 2.
**
** This program is distributed in the hope that it will be useful, but WITHOUT
** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
** FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
**
** File:
**     tiload.h
**
** Description:
**     header file for tiload.c
**
** =============================================================================
*/

#ifndef _TILOAD_H
#define _TILOAD_H

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#include "tas2557.h"

#define BPR_REG(book, page, reg)		(((book * 256 * 128) + \
						 (page * 128)) + reg)

/* typedefs required for the included header files */
struct BPR {
	unsigned char nBook;
	unsigned char nPage;
	unsigned char nRegister;
};

/* defines */
#define DEVICE_NAME     "tiload_node"

#define TILOAD_IOC_MAGIC   0xE0
#define TILOAD_IOMAGICNUM_GET			_IOR(TILOAD_IOC_MAGIC, 1, int)
#define TILOAD_IOMAGICNUM_SET			_IOW(TILOAD_IOC_MAGIC, 2, int)
#define TILOAD_BPR_READ					_IOR(TILOAD_IOC_MAGIC, 3, struct BPR)
#define TILOAD_BPR_WRITE				_IOW(TILOAD_IOC_MAGIC, 4, struct BPR)
#define TILOAD_IOCTL_SET_CHL			_IOW(TILOAD_IOC_MAGIC, 5, int)
#define TILOAD_IOCTL_SET_CONFIG			_IOW(TILOAD_IOC_MAGIC, 6, int)
#define TILOAD_IOCTL_SET_CALIBRATION	_IOW(TILOAD_IOC_MAGIC, 7, int)

#ifdef CONFIG_COMPAT
#define TILOAD_COMPAT_IOMAGICNUM_GET		_IOR(TILOAD_IOC_MAGIC, 1, compat_int_t)
#define TILOAD_COMPAT_IOMAGICNUM_SET		_IOW(TILOAD_IOC_MAGIC, 2, compat_int_t)
#define TILOAD_COMPAT_BPR_READ				_IOR(TILOAD_IOC_MAGIC, 3, struct BPR)
#define TILOAD_COMPAT_BPR_WRITE				_IOW(TILOAD_IOC_MAGIC, 4, struct BPR)
#define TILOAD_COMPAT_IOCTL_SET_CHL			_IOW(TILOAD_IOC_MAGIC, 5, compat_int_t)
#define TILOAD_COMPAT_IOCTL_SET_CONFIG		_IOW(TILOAD_IOC_MAGIC, 6, compat_int_t)
#define TILOAD_COMPAT_IOCTL_SET_CALIBRATION	_IOW(TILOAD_IOC_MAGIC, 7, compat_int_t)
#endif

int tiload_driver_init(struct tas2557_priv *pTAS2557);

#endif
