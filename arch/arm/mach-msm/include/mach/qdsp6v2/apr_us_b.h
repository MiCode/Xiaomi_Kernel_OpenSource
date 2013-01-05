/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#ifndef __APR_US_B_H__
#define __APR_US_B_H__

#include "apr_us.h"

/* ======================================================================= */
/*  Session Level commands */
#define USM_CMD_SHARED_MEM_MAP_REGION		0x00012728
struct usm_cmd_memory_map_region {
	struct apr_hdr hdr;
	u16            mempool_id;
	u16            num_regions;
	u32            flags;
	u32            shm_addr_lsw;
	u32            shm_addr_msw;
	u32            mem_size_bytes;
} __packed;

#define USM_CMDRSP_SHARED_MEM_MAP_REGION	0x00012729
struct usm_cmdrsp_memory_map_region {
	u32            mem_map_handle;
} __packed;

#define USM_CMD_SHARED_MEM_UNMAP_REGION         0x0001272A
struct usm_cmd_memory_unmap_region {
	struct apr_hdr hdr;
	u32            mem_map_handle;
} __packed;

#define USM_DATA_CMD_READ			0x00012724
struct usm_stream_cmd_read {
	struct apr_hdr hdr;
	u32            buf_addr_lsw;
	u32            buf_addr_msw;
	u32            mem_map_handle;
	u32            buf_size;
	u32            seq_id;
	u32            counter;
} __packed;

#define USM_DATA_EVENT_READ_DONE		0x00012725

#define USM_DATA_CMD_WRITE			0x00012726
struct usm_stream_cmd_write {
	struct apr_hdr hdr;
	u32            buf_addr_lsw;
	u32            buf_addr_msw;
	u32            mem_map_handle;
	u32            buf_size;
	u32            seq_id;
	u32            res0;
	u32            res1;
	u32            res2;
} __packed;

#define USM_DATA_EVENT_WRITE_DONE		0x00012727

#endif /* __APR_US_B_H__ */
