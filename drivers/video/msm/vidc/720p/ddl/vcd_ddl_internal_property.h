/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
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
#ifndef _VCD_DDL_INTERNAL_PROPERTY_H_
#define _VCD_DDL_INTERNAL_PROPERTY_H_
#include <media/msm/vcd_api.h>

#define VCD_EVT_RESP_DDL_BASE          0x3000
#define VCD_EVT_RESP_DEVICE_INIT       (VCD_EVT_RESP_DDL_BASE + 0x1)
#define VCD_EVT_RESP_OUTPUT_REQ        (VCD_EVT_RESP_DDL_BASE + 0x2)
#define VCD_EVT_RESP_EOS_DONE          (VCD_EVT_RESP_DDL_BASE + 0x3)
#define VCD_EVT_RESP_TRANSACTION_PENDING (VCD_EVT_RESP_DDL_BASE + 0x4)

#define VCD_S_DDL_ERR_BASE     0x90000000
#define VCD_ERR_MAX_NO_CODEC   (VCD_S_DDL_ERR_BASE + 0x1)
#define VCD_ERR_CLIENT_PRESENT (VCD_S_DDL_ERR_BASE + 0x2)
#define VCD_ERR_CLIENT_FATAL   (VCD_S_DDL_ERR_BASE + 0x3)

#define VCD_I_CUSTOM_BASE  (VCD_I_RESERVED_BASE)
#define VCD_I_RC_LEVEL_CONFIG (VCD_I_CUSTOM_BASE + 0x1)
#define VCD_I_FRAME_LEVEL_RC (VCD_I_CUSTOM_BASE + 0x2)
#define VCD_I_ADAPTIVE_RC    (VCD_I_CUSTOM_BASE + 0x3)
#define VCD_I_CUSTOM_DDL_BASE  (VCD_I_RESERVED_BASE + 0x100)
#define DDL_I_INPUT_BUF_REQ  (VCD_I_CUSTOM_DDL_BASE + 0x1)
#define DDL_I_OUTPUT_BUF_REQ (VCD_I_CUSTOM_DDL_BASE + 0x2)
#define DDL_I_DPB       (VCD_I_CUSTOM_DDL_BASE + 0x3)
#define DDL_I_DPB_RELEASE    (VCD_I_CUSTOM_DDL_BASE + 0x4)
#define DDL_I_DPB_RETRIEVE  (VCD_I_CUSTOM_DDL_BASE + 0x5)
#define DDL_I_REQ_OUTPUT_FLUSH   (VCD_I_CUSTOM_DDL_BASE + 0x6)
#define DDL_I_SEQHDR_ALIGN_BYTES (VCD_I_CUSTOM_DDL_BASE + 0x7)
#define DDL_I_SEQHDR_PRESENT (VCD_I_CUSTOM_DDL_BASE + 0xb)
#define DDL_I_CAPABILITY    (VCD_I_CUSTOM_DDL_BASE + 0x8)
#define DDL_I_FRAME_PROC_UNITS    (VCD_I_CUSTOM_DDL_BASE + 0x9)

struct vcd_property_rc_level {
	u32 frame_level_rc;
	u32 mb_level_rc;
};

struct vcd_property_frame_level_rc_params {
	u32 reaction_coeff;
};

struct vcd_property_adaptive_rc_params {
	u32 dark_region_as_flag;
	u32 smooth_region_as_flag;
	u32 static_region_as_flag;
	u32 activity_region_flag;
};

struct ddl_frame_data_tag;

struct ddl_property_dec_pic_buffers {
	struct ddl_frame_data_tag *dec_pic_buffers;
	u32 no_of_dec_pic_buf;
};

struct ddl_property_capability {
	u32 max_num_client;
	u32 general_command_depth;
	u32 frame_command_depth;
	u32 exclusive;
	u32   ddl_time_out_in_ms;
};

#endif
