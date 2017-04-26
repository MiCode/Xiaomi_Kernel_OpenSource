/*
** =============================================================================
** Copyright (c) 2016  Texas Instruments Inc.
**
** This program is free software; you can redistribute it and/or modify it under
** the terms of the GNU General Public License as published by the Free Software
** Foundation; version 2.
**
** This program is distributed in the hope that it will be useful, but WITHOUT
** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
** FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License along with
** this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
** Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

#include "tas2555.h"

#define BPR_REG(book, page, reg)		(((book * 256 * 128) + \
						 (page * 128)) + reg)

/* typedefs required for the included header files */
typedef char *string;

typedef struct {
	unsigned char nBook;
	unsigned char nPage;
	unsigned char nRegister;
} BPR;

/* defines */
#define DEVICE_NAME     "tiload_node"
#define TILOAD_IOC_MAGIC   0xE0
#define TILOAD_IOMAGICNUM_GET  _IOR(TILOAD_IOC_MAGIC, 1, int)
#define TILOAD_IOMAGICNUM_SET  _IOW(TILOAD_IOC_MAGIC, 2, int)
#define TILOAD_BPR_READ _IOR(TILOAD_IOC_MAGIC, 3, BPR)
#define TILOAD_BPR_WRITE _IOW(TILOAD_IOC_MAGIC, 4, BPR)
#define TILOAD_IOCTL_SET_MODE _IOW(TILOAD_IOC_MAGIC, 5, int)
#define TILOAD_IOCTL_SET_CALIBRATION _IOW(TILOAD_IOC_MAGIC, 6, int)


int tiload_driver_init(struct tas2555_priv *pTAS2555);

#endif
