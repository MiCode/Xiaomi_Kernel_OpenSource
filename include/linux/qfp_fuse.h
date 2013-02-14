/*
 * Copyright (c) 2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _QFP_FUSE_H_
#define _QFP_FUSE_H_

#include <linux/types.h>
#include <linux/ioctl.h>

#define QFP_FUSE_IOC_MAGIC          0x92

#define QFP_FUSE_IOC_WRITE          _IO(QFP_FUSE_IOC_MAGIC, 1)
#define QFP_FUSE_IOC_READ           _IO(QFP_FUSE_IOC_MAGIC, 2)


/*
 * This structure is used to exchange the fuse parameters with the user
 * space application. The pointer to this structure is passed to the ioctl
 * function.
 * offset   = offset from the QFPROM base for the data to be read/written.
 * size     = number of 32-bit words to be read/written.
 * data     = pointer to the 32 bit word denoting userspace data.
 */
struct qfp_fuse_req {
	u32 offset;
	u32 size;
	u32 *data;
};

#endif
