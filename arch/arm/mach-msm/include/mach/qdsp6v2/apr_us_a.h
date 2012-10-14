/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#ifndef __APR_US_A_H__
#define __APR_US_A_H__

#include "apr_us.h"

/* ======================================================================= */
/*  Session Level commands */
#define USM_SESSION_CMD_MEMORY_MAP			0x00012304
struct usm_stream_cmd_memory_map {
	struct apr_hdr	hdr;
	u32		buf_add;
	u32		buf_size;
	u16		mempool_id;
	u16		reserved;
} __packed;

#define USM_SESSION_CMD_MEMORY_UNMAP			0x00012305
struct usm_stream_cmd_memory_unmap {
	struct apr_hdr  hdr;
	u32             buf_add;
} __packed;

#define USM_DATA_CMD_READ				0x0001230E
struct usm_stream_cmd_read {
	struct apr_hdr  hdr;
	u32                 buf_add;
	u32                 buf_size;
	u32                 uid;
	u32                 counter;
} __packed;

#define USM_DATA_EVENT_READ_DONE			0x0001230F

#define USM_DATA_CMD_WRITE				0x00011273
struct usm_stream_cmd_write {
	struct apr_hdr hdr;
	u32 buf_add;
	u32 buf_size;
	u32 uid;
	u32 msw_ts;
	u32 lsw_ts;
	u32 flags;
} __packed;

#define USM_DATA_EVENT_WRITE_DONE			0x00011274

#endif /* __APR_US_A_H__ */
