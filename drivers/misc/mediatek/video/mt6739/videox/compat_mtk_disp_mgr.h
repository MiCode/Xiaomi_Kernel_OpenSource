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

struct compat_disp_session_config {
	compat_uint_t type;
	compat_uint_t device_id;
	compat_uint_t mode;
	compat_uint_t session_id;
	compat_uint_t user;
	compat_uint_t present_fence_idx;
	compat_uint_t dc_type;
	compat_int_t need_merge;
	compat_int_t tigger_mode;
};

struct compat_layer_dirty_roi {
	compat_ushort_t dirty_x;
	compat_ushort_t dirty_y;
	compat_ushort_t dirty_w;
	compat_ushort_t dirty_h;
};

struct compat_disp_input_config {
	compat_uptr_t src_base_addr;
	compat_uptr_t src_phy_addr;
	compat_uint_t buffer_source;
	compat_uint_t security;
	compat_uint_t src_fmt;
	compat_uint_t src_alpha;
	compat_uint_t dst_alpha;
	compat_uint_t yuv_range;

	compat_uint_t layer_rotation;
	compat_uint_t layer_type;
	compat_uint_t video_rotation;

	compat_uint_t next_buff_idx;

	compat_uint_t src_color_key;
	compat_uint_t frm_sequence;

	compat_uptr_t dirty_roi_addr;
	compat_ushort_t dirty_roi_num;

	compat_ushort_t src_pitch;
	compat_ushort_t src_offset_x, src_offset_y;
	compat_ushort_t src_width, src_height;
	compat_ushort_t tgt_offset_x, tgt_offset_y;
	compat_ushort_t tgt_width, tgt_height;

	u8 alpha_enable;
	u8 alpha;
	u8 sur_aen;
	u8 src_use_color_key;
	u8 layer_id;
	u8 layer_enable;

	u8 src_direct_link;
	u8 isTdshp;
	u8 identity;
	u8 connected_type;
};

struct compat_disp_output_config {
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
};

struct compat_disp_session_input_config {
	compat_uint_t setter;
	compat_uint_t session_id;
	compat_uint_t config_layer_num;
	struct compat_disp_input_config config[8];
};

struct compat_disp_present_fence_info {
	compat_uint_t session_id;
	compat_uint_t present_fence_fd;
	compat_uint_t present_fence_index;
};


struct compat_disp_session_vsync_config {
	compat_uint_t session_id;
	compat_uint_t vsync_cnt;
	compat_u64 vsync_ts;
	compat_uint_t lcm_fps;
};

struct compat_disp_session_layer_num_config {
	compat_uint_t session_id;
	compat_uint_t max_layer_num;
};



struct compat_disp_caps_info {
	compat_uint_t output_mode;
	compat_uint_t output_pass;
	compat_uint_t max_layer_num;
	compat_uint_t disp_feature;
	compat_uint_t is_support_frame_cfg_ioctl;
	compat_uint_t is_output_rotated;
	compat_uint_t fence_wait_supported;
};


struct compat_disp_buffer_info {
	compat_uint_t session_id;
	compat_uint_t layer_id;
	compat_uint_t layer_en;
	compat_int_t ion_fd;
	compat_uint_t cache_sync;
	compat_uint_t index;
	compat_int_t fence_fd;
	compat_uint_t interface_index;
	compat_int_t interface_fence_fd;
};

struct compat_disp_session_output_config {
	compat_uint_t session_id;
	struct compat_disp_output_config config;
};

struct compat_disp_frame_cfg_t {
	compat_uint_t setter;
	compat_uint_t session_id;

	/* input config */
	compat_uint_t input_layer_num;
	struct compat_disp_input_config input_cfg[8];
	compat_uint_t overlap_layer_num;

	/* constant layer */
	compat_uint_t const_layer_num;
	struct compat_disp_input_config const_layer[1];

	/* output config */
	compat_int_t output_en;
	struct compat_disp_output_config output_cfg;

	/* trigger config */
	compat_uint_t mode;
	compat_uint_t present_fence_idx;
	compat_uint_t tigger_mode;
	compat_uint_t user;
};

struct compat_disp_session_info {
	compat_uint_t session_id;
	compat_uint_t maxLayerNum;
	compat_uint_t isHwVsyncAvailable;
	compat_uint_t displayType;
	compat_uint_t displayWidth;
	compat_uint_t displayHeight;
	compat_uint_t displayFormat;
	compat_uint_t displayMode;
	compat_uint_t vsyncFPS;
	compat_uint_t physicalWidth;
	compat_uint_t physicalHeight;
	compat_uint_t physicalWidthUm;	/* length: um, for more precise precision */
	compat_uint_t physicalHeightUm;	/* length: um, for more precise precision */
	compat_uint_t density;
	compat_uint_t isConnected;
	compat_uint_t isHDCPSupported;
	compat_uint_t isOVLDisabled;
	compat_uint_t is3DSupport;
	compat_uint_t const_layer_num;
	/* updateFPS: fps of HWC trigger display */
	/* notes: for better Accuracy, updateFPS = real_fps*100 */
	compat_uint_t updateFPS;
	compat_uint_t is_updateFPS_stable;
};

int _compat_ioctl_prepare_present_fence(struct file *file, unsigned long arg);
int _compat_ioctl_trigger_session(struct file *file, unsigned long arg);
int _compat_ioctl_destroy_session(struct file *file, unsigned long arg);
int _compat_ioctl_create_session(struct file *file, unsigned long arg);
int _compat_ioctl_get_info(struct file *file, unsigned long arg);
int _compat_ioctl_prepare_buffer(struct file *file, unsigned long arg, enum PREPARE_FENCE_TYPE type);
int _compat_ioctl_wait_vsync(struct file *file, unsigned long arg);
int _compat_ioctl_set_input_buffer(struct file *file, unsigned long arg);
int _compat_ioctl_get_display_caps(struct file *file, unsigned long arg);
int _compat_ioctl_get_vsync(struct file *file, unsigned long arg);
int _compat_ioctl_set_vsync(struct file *file, unsigned long arg);
int _compat_ioctl_set_output_buffer(struct file *file, unsigned long arg);
int _compat_ioctl_set_session_mode(struct file *file, unsigned long arg);
int _compat_ioctl_frame_config(struct file *file, unsigned long arg);


#define	COMPAT_DISP_IOCTL_CREATE_SESSION		DISP_IOW(201, struct compat_disp_session_config)
#define	COMPAT_DISP_IOCTL_DESTROY_SESSION		DISP_IOW(202, struct compat_disp_session_config)
#define	COMPAT_DISP_IOCTL_TRIGGER_SESSION		DISP_IOW(203, struct compat_disp_session_config)
#define	COMPAT_DISP_IOCTL_PREPARE_INPUT_BUFFER		DISP_IOW(204, struct compat_disp_buffer_info)
#define	COMPAT_DISP_IOCTL_PREPARE_OUTPUT_BUFFER		DISP_IOW(205, struct compat_disp_buffer_info)
#define	COMPAT_DISP_IOCTL_SET_INPUT_BUFFER		DISP_IOW(206, struct compat_disp_session_input_config)
#define	COMPAT_DISP_IOCTL_SET_OUTPUT_BUFFER		DISP_IOW(207, struct compat_disp_session_output_config)
#define	COMPAT_DISP_IOCTL_GET_SESSION_INFO		DISP_IOW(208, struct compat_disp_session_info)
#define	COMPAT_DISP_IOCTL_SET_SESSION_MODE		DISP_IOW(209, struct compat_disp_session_config)
#define	COMPAT_DISP_IOCTL_GET_SESSION_MODE		DISP_IOW(210, struct compat_disp_session_config)
#define	COMPAT_DISP_IOCTL_SET_SESSION_TYPE		DISP_IOW(211, struct compat_disp_session_config)
#define	COMPAT_DISP_IOCTL_GET_SESSION_TYPE		DISP_IOW(212, struct compat_disp_session_config)
#define	COMPAT_DISP_IOCTL_WAIT_FOR_VSYNC		DISP_IOW(213, struct compat_disp_session_vsync_config)
#define	COMPAT_DISP_IOCTL_SET_MAX_LAYER_NUM		DISP_IOW(214, struct compat_disp_session_layer_num_config)
#define	COMPAT_DISP_IOCTL_GET_VSYNC_FPS			DISP_IOW(215, compat_uint_t)
#define	COMPAT_DISP_IOCTL_SET_VSYNC_FPS			DISP_IOW(216, compat_uint_t)
#define	COMPAT_DISP_IOCTL_GET_PRESENT_FENCE		DISP_IOW(217, struct compat_disp_present_fence_info)
#define COMPAT_DISP_IOCTL_GET_IS_DRIVER_SUSPEND		DISP_IOW(218, compat_uint_t)
#define COMPAT_DISP_IOCTL_GET_DISPLAY_CAPS		DISP_IOW(219, struct compat_disp_caps_info)
#define	COMPAT_DISP_IOCTL_FRAME_CONFIG			DISP_IOW(220, struct compat_disp_session_output_config)


#endif
#endif /*_COMPAT_MTK_DISP_MGR_H_*/
