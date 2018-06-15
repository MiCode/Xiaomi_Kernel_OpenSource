/* Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
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
#ifndef __APR_US_H__
#define __APR_US_H__

#include <ipc/apr.h>

/* ======================================================================= */
/*  Session Level commands */

#define USM_SESSION_CMD_RUN				0x00012306
struct usm_stream_cmd_run {
	struct apr_hdr hdr;
	u32            flags;
	u32            msw_ts;
	u32            lsw_ts;
} __packed;

/* Stream level commands */
#define USM_STREAM_CMD_OPEN_READ			0x00012309
struct usm_stream_cmd_open_read {
	struct apr_hdr hdr;
	u32            uMode;
	u32            src_endpoint;
	u32            pre_proc_top;
	u32            format;
} __packed;

#define USM_STREAM_CMD_OPEN_WRITE			0x00011271
struct usm_stream_cmd_open_write {
	struct apr_hdr hdr;
	u32            format;
} __packed;


#define USM_STREAM_CMD_CLOSE				0x0001230A

#define USM_STREAM_CMD_SET_PARAM			0x00012731
struct usm_stream_cmd_set_param {
	struct apr_hdr hdr;
	u32            buf_addr_lsw;
	u32            buf_addr_msw;
	u32            mem_map_handle;
	u32            buf_size;
	u32            module_id;
	u32            param_id;
} __packed;

#define USM_STREAM_CMD_GET_PARAM			0x00012732
struct usm_stream_cmd_get_param {
	struct apr_hdr hdr;
	u32            buf_addr_lsw;
	u32            buf_addr_msw;
	u32            mem_map_handle;
	u32            buf_size;
	u32            module_id;
	u32            param_id;
} __packed;

/* Encoder configuration definitions */
#define USM_STREAM_CMD_SET_ENC_PARAM			0x0001230B
/* Decoder configuration definitions */
#define USM_DATA_CMD_MEDIA_FORMAT_UPDATE		0x00011272

/* Encoder/decoder configuration block */
#define USM_PARAM_ID_ENCDEC_ENC_CFG_BLK			0x0001230D

/* Max number of static located ports (bytes) */
#define USM_MAX_PORT_NUMBER 8

/* Max number of static located transparent data (bytes) */
#define USM_MAX_CFG_DATA_SIZE 100

/* Parameter structures used in  USM_STREAM_CMD_SET_ENCDEC_PARAM command */
/* common declarations */
struct usm_cfg_common {
	u16 ch_cfg;
	u16 bits_per_sample;
	u32 sample_rate;
	u32 dev_id;
	u8 data_map[USM_MAX_PORT_NUMBER];
} __packed;

struct us_encdec_cfg {
	u32 format_id;
	struct usm_cfg_common cfg_common;
	u16 params_size;
	u8 *params;
} __packed;

/* Start/stop US signal detection */
#define USM_SESSION_CMD_SIGNAL_DETECT_MODE		0x00012719

struct usm_session_cmd_detect_info {
	struct apr_hdr hdr;
	u32 detect_mode;
	u32 skip_interval;
	u32 algorithm_cfg_size;
} __packed;

/* US signal detection result */
#define USM_SESSION_EVENT_SIGNAL_DETECT_RESULT		0x00012720

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

struct usm_stream_media_format_update {
	struct apr_hdr hdr;
	u32 format_id;
	/* <cfg_size> = sizeof(usm_cfg_common)+|transp_data| */
	u32 cfg_size;
	struct usm_cfg_common cfg_common;
	/* Transparent configuration data for specific encoder */
	u8  transp_data[USM_MAX_CFG_DATA_SIZE];
} __packed;

struct usm_encode_cfg_blk {
	u32 frames_per_buf;
	u32 format_id;
	/* <cfg_size> = sizeof(usm_cfg_common)+|transp_data| */
	u32 cfg_size;
	struct usm_cfg_common cfg_common;
	/* Transparent configuration data for specific encoder */
	u8  transp_data[USM_MAX_CFG_DATA_SIZE];
} __packed;

struct usm_stream_cmd_encdec_cfg_blk {
	struct apr_hdr hdr;
	u32 param_id;
	u32 param_size;
	struct usm_encode_cfg_blk enc_blk;
} __packed;

#endif /* __APR_US_H__ */
