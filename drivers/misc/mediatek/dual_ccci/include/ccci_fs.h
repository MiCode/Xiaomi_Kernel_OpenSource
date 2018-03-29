/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *   ccci_fs.h
 *
 * Project:
 * --------
 *   ALPS
 *
 * Description:
 * ------------
 *   MT65XX CCCI FS Proxy Driver
 *
 ****************************************************************************/

#ifndef __CCCI_FS_H__
#define __CCCI_FS_H__
/*
 * define IOCTL commands
 */
#define CCCI_FS_IOC_MAGIC 'K'
#define CCCI_FS_IOCTL_GET_INDEX _IO(CCCI_FS_IOC_MAGIC, 1)
#define CCCI_FS_IOCTL_SEND      _IOR(CCCI_FS_IOC_MAGIC, 2, unsigned int)

#define  CCCI_FS_MAX_BUF_SIZE   (16384)
#define  CCCI_FS_MAX_BUFFERS    (5)
struct fs_stream_msg_t {
	unsigned length;
	unsigned index;
};

struct fs_stream_buffer_t {
	unsigned fs_ops;
	unsigned char buffer[CCCI_FS_MAX_BUF_SIZE];
};

#define CCCI_FS_SMEM_SIZE (sizeof(struct fs_stream_buffer_t) * CCCI_FS_MAX_BUFFERS)

#endif				/*  __CCCI_FS_H__ */
