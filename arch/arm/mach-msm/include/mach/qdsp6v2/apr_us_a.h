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

/* Max number of static located ports (bytes) */
#define USM_MAX_PORT_NUMBER_A 4

/* Parameter structures used in  USM_STREAM_CMD_SET_ENCDEC_PARAM command */
/* common declarations */
struct usm_cfg_common_a {
	u16 ch_cfg;
	u16 bits_per_sample;
	u32 sample_rate;
	u32 dev_id;
	u8 data_map[USM_MAX_PORT_NUMBER_A];
} __packed;

struct usm_stream_media_format_update {
	struct apr_hdr hdr;
	u32 format_id;
	/* <cfg_size> = sizeof(usm_cfg_common)+|transp_data| */
	u32 cfg_size;
	struct usm_cfg_common_a cfg_common;
	/* Transparent configuration data for specific encoder */
	u8  transp_data[USM_MAX_CFG_DATA_SIZE];
} __packed;

struct usm_encode_cfg_blk {
	u32 frames_per_buf;
	u32 format_id;
	/* <cfg_size> = sizeof(usm_cfg_common)+|transp_data| */
	u32 cfg_size;
	struct usm_cfg_common_a cfg_common;
	/* Transparent configuration data for specific encoder */
	u8  transp_data[USM_MAX_CFG_DATA_SIZE];
} __packed;

struct usm_stream_cmd_encdec_cfg_blk {
	struct apr_hdr hdr;
	u32 param_id;
	u32 param_size;
	struct usm_encode_cfg_blk enc_blk;
} __packed;
#endif /* __APR_US_A_H__ */
