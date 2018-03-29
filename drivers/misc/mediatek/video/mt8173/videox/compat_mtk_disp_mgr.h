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

#ifndef _COMPAT_MTK_DISP_MGR_H_
#define _COMPAT_MTK_DISP_MGR_H_
#include <linux/uaccess.h>
#include <linux/compat.h>
#include "mtk_disp_mgr.h"

#include "disp_session.h"

#ifdef CONFIG_COMPAT
typedef struct compat_disp_input_config_t {
	compat_uint_t layer_id;
	compat_uint_t layer_enable;
	compat_uint_t buffer_source;
	compat_uptr_t src_base_addr;
	compat_uptr_t src_phy_addr;
	compat_uint_t src_direct_link;
	compat_uint_t src_fmt;
	compat_uint_t src_use_color_key;
	compat_uint_t src_color_key;
	compat_uint_t src_pitch;
	compat_uint_t src_offset_x, src_offset_y;
	compat_uint_t src_width, src_height;

	compat_uint_t tgt_offset_x, tgt_offset_y;
	compat_uint_t tgt_width, tgt_height;
	compat_uint_t layer_rotation;
	compat_uint_t layer_type;
	compat_uint_t video_rotation;

	compat_uint_t isTdshp;

	compat_uint_t next_buff_idx;
	compat_int_t identity;
	compat_int_t connected_type;
	compat_uint_t security;
	compat_uint_t alpha_enable;
	compat_uint_t alpha;
	compat_uint_t sur_aen;
	compat_uint_t src_alpha;
	compat_uint_t dst_alpha;
	compat_uint_t frm_sequence;
	compat_uint_t yuv_range;
	compat_uint_t fps;
	compat_u64 timestamp;
} compat_disp_input_config;

typedef struct compat_disp_output_config_t {
	compat_uptr_t va;
	compat_uptr_t pa;
	compat_uint_t fmt;
	compat_uint_t x;
	compat_uint_t y;
	compat_uint_t width;
	compat_uint_t height;
	compat_uint_t pitch;
	compat_uint_t pitchUV;
	compat_uint_t security;
	compat_uint_t buff_idx;
	compat_uint_t interface_idx;
	compat_uint_t frm_sequence;
} compat_disp_output_config;

typedef struct compat_disp_session_input_config_t {
	compat_uint_t session_id;
	compat_uint_t config_layer_num;
	compat_disp_input_config config[MAX_INPUT_CONFIG];
} compat_disp_session_input_config;

typedef struct compat_disp_session_output_config_t {
	compat_uint_t session_id;
	compat_disp_output_config config;
} compat_disp_session_output_config;

int _compat_ioctl_set_input_buffer(struct file *file, unsigned long arg);
int _compat_ioctl_set_output_buffer(struct file *file, unsigned long arg);

#define	COMPAT_DISP_IOCTL_SET_INPUT_BUFFER			DISP_IOW(206, compat_disp_session_input_config)
#define	COMPAT_DISP_IOCTL_SET_OUTPUT_BUFFER			DISP_IOW(207, compat_disp_session_output_config)
#endif
#endif /*_COMPAT_MTK_DISP_MGR_H_*/
