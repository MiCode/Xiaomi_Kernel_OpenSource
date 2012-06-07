/* Copyright (c) 2009-2010, Code Aurora Forum. All rights reserved.
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

#ifndef __RMT_STORAGE_SERVER_H
#define __RMT_STORAGE_SERVER_H

#include <linux/types.h>
#include <linux/ioctl.h>

#define RMT_STORAGE_OPEN              0
#define RMT_STORAGE_WRITE             1
#define RMT_STORAGE_CLOSE             2
#define RMT_STORAGE_SEND_USER_DATA    3
#define RMT_STORAGE_READ              4
#define RMT_STORAGE_NOOP              255

#define RMT_STORAGE_MAX_IOVEC_XFR_CNT 5
#define MAX_NUM_CLIENTS 10
#define MAX_RAMFS_TBL_ENTRIES 3
#define RAMFS_BLOCK_SIZE		512


enum {
	RMT_STORAGE_NO_ERROR = 0,	/* Success */
	RMT_STORAGE_ERROR_PARAM,	/* Invalid parameters */
	RMT_STORAGE_ERROR_PIPE,		/* RPC pipe failure */
	RMT_STORAGE_ERROR_UNINIT,	/* Server is not initalized */
	RMT_STORAGE_ERROR_BUSY,		/* Device busy */
	RMT_STORAGE_ERROR_DEVICE	/* Remote storage device */
} rmt_storage_status;

struct rmt_storage_iovec_desc {
	uint32_t sector_addr;
	uint32_t data_phy_addr;
	uint32_t num_sector;
};

#define MAX_PATH_NAME 32
struct rmt_storage_event {
	uint32_t id;		/* Event ID */
	uint32_t sid;		/* Storage ID */
	uint32_t handle;	/* Client handle */
	char path[MAX_PATH_NAME];
	struct rmt_storage_iovec_desc xfer_desc[RMT_STORAGE_MAX_IOVEC_XFR_CNT];
	uint32_t xfer_cnt;
	uint32_t usr_data;
};

struct rmt_storage_send_sts {
	uint32_t err_code;
	uint32_t data;
	uint32_t handle;
	uint32_t xfer_dir;
};

struct rmt_shrd_mem_param {
	uint32_t sid;		/* Storage ID */
	uint32_t start;		/* Physical memory address */
	uint32_t size;		/* Physical memory size */
	void *base;		/* Virtual user-space memory address */
};

#define RMT_STORAGE_IOCTL_MAGIC (0xC2)

#define RMT_STORAGE_SHRD_MEM_PARAM \
	_IOWR(RMT_STORAGE_IOCTL_MAGIC, 0, struct rmt_shrd_mem_param)

#define RMT_STORAGE_WAIT_FOR_REQ \
	_IOR(RMT_STORAGE_IOCTL_MAGIC, 1, struct rmt_storage_event)

#define RMT_STORAGE_SEND_STATUS \
	_IOW(RMT_STORAGE_IOCTL_MAGIC, 2, struct rmt_storage_send_sts)
#endif
