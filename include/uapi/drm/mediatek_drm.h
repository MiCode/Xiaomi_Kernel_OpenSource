/*
 * Copyright (c) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#ifndef _UAPI_MEDIATEK_DRM_H
#define _UAPI_MEDIATEK_DRM_H

#include <drm/drm.h>

#define MTK_SUBMIT_NO_IMPLICIT   0x0 /* disable implicit sync */
#define MTK_SUBMIT_IN_FENCE   0x1 /* enable input fence */
#define MTK_SUBMIT_OUT_FENCE  0x2  /* enable output fence */

#define MTK_DRM_PROP_OVERLAP_LAYER_NUM  "OVERLAP_LAYER_NUM"
#define MTK_DRM_PROP_NEXT_BUFF_IDX  "NEXT_BUFF_IDX"
#define MTK_DRM_PROP_PRESENT_FENCE  "PRESENT_FENCE"


/**
 * User-desired buffer creation information structure.
 *
 * @size: user-desired memory allocation size.
 *	- this size value would be page-aligned internally.
 * @flags: user request for setting memory type or cache attributes.
 * @handle: returned a handle to created gem object.
 *	- this handle will be set by gem module of kernel side.
 */
struct drm_mtk_gem_create {
	uint64_t size;
	uint32_t flags;
	uint32_t handle;
};

/**
 * A structure for getting buffer offset.
 *
 * @handle: a pointer to gem object created.
 * @pad: just padding to be 64-bit aligned.
 * @offset: relatived offset value of the memory region allocated.
 *     - this value should be set by user.
 */
struct drm_mtk_gem_map_off {
	uint32_t handle;
	uint32_t pad;
	uint64_t offset;
};

/**
 * A structure for buffer submit.
 *
 * @type:
 * @session_id:
 * @layer_id:
 * @layer_en:
 * @fb_id:
 * @index:
 * @fence_fd:
 * @interface_index:
 * @interface_fence_fd:
 */
struct drm_mtk_gem_submit {
	uint32_t type;
	/* session */
	uint32_t session_id;
	/* layer */
	uint32_t layer_id;
	uint32_t layer_en;
	/* buffer */
	uint32_t fb_id;
	/* output */
	uint32_t index;
	int32_t fence_fd;
	uint32_t interface_index;
	int32_t interface_fence_fd;
};

/**
 * A structure for session create.
 *
 * @type:
 * @device_id:
 * @mode:
 * @session_id:
 */
struct drm_mtk_session {
	uint32_t type;
	/* device */
	uint32_t device_id;
	/* mode */
	uint32_t mode;
	/* output */
	uint32_t session_id;
};


#define DRM_MTK_GEM_CREATE		0x00
#define DRM_MTK_GEM_MAP_OFFSET		0x01
#define DRM_MTK_GEM_SUBMIT		0x02
#define DRM_MTK_SESSION_CREATE		0x03
#define DRM_MTK_SESSION_DESTROY		0x04
#define DRM_MTK_LAYERING_RULE           0x05
#define DRM_MTK_CRTC_GETFENCE           0x06
#define DRM_MTK_WAIT_REPAINT            0x07
#define DRM_MTK_GET_DISPLAY_CAPS	0x08
#define DRM_MTK_SET_DDP_MODE   0x09
#define DRM_MTK_GET_SESSION_INFO	0x0A

#define DRM_IOCTL_MTK_SET_DDP_MODE	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_SET_DDP_MODE, unsigned int)

enum MTK_DRM_SESSION_MODE {
	MTK_DRM_SESSION_INVALID = 0,
	/* single output */
	MTK_DRM_SESSION_DL,

	/* two ouputs */
	MTK_DRM_SESSION_DOUBLE_DL,
	MTK_DRM_SESSION_DC_MIRROR,

	/* three session at same time */
	MTK_DRM_SESSION_TRIPLE_DL,
	MTK_DRM_SESSION_NUM,
};

enum MTK_LAYERING_CAPS {
	MTK_LAYERING_OVL_ONLY = 0x00000001,
	MTK_MDP_RSZ_LAYER =		0x00000002,
	MTK_DISP_RSZ_LAYER =	0x00000004,
	MTK_MDP_ROT_LAYER =		0x00000008,
	MTK_MDP_HDR_LAYER =		0x00000010,
	MTK_NO_FBDC =			0x00000020,
	MTK_DMDP_RSZ_LAYER =		0x00000040,
};

struct drm_mtk_layer_config {
	uint32_t ovl_id;
	uint32_t src_fmt;
	int dataspace;
	uint32_t dst_offset_x, dst_offset_y;
	uint32_t dst_width, dst_height;
	int ext_sel_layer;
	uint32_t src_offset_x, src_offset_y;
	uint32_t src_width, src_height;
	uint32_t layer_caps;
	uint32_t clip; /* drv internal use */
	__u8 compress;
};

struct drm_mtk_layering_info {
	struct drm_mtk_layer_config *input_config[2];
	int disp_mode[2];
	int layer_num[2];
	int gles_head[2];
	int gles_tail[2];
	int hrt_num;
	/* res_idx: SF/HWC selects which resolution to use */
	int res_idx;
	uint32_t hrt_weight;
	uint32_t hrt_idx;
};

/**
 * A structure for fence retrival.
 *
 * @crtc_id:
 * @fence_fd:
 * @fence_idx:
 */
struct drm_mtk_fence {
	/* input */
	uint32_t crtc_id; /**< Id */

	/* output */
	int32_t fence_fd;
	/* device */
	uint32_t fence_idx;
};

enum DRM_REPAINT_TYPE {
	DRM_WAIT_FOR_REPAINT,
	DRM_REPAINT_FOR_ANTI_LATENCY,
	DRM_REPAINT_FOR_SWITCH_DECOUPLE,
	DRM_REPAINT_FOR_SWITCH_DECOUPLE_MIRROR,
	DRM_REPAINT_FOR_IDLE,
	DRM_REPAINT_TYPE_NUM,
};

enum MTK_DRM_DISP_FEATURE {
	DRM_DISP_FEATURE_HRT = 0x00000001,
	DRM_DISP_FEATURE_DISP_SELF_REFRESH = 0x00000002,
	DRM_DISP_FEATURE_RPO = 0x00000004,
	DRM_DISP_FEATURE_FORCE_DISABLE_AOD = 0x00000008,
	DRM_DISP_FEATURE_OUTPUT_ROTATED = 0x00000010,
	DRM_DISP_FEATURE_THREE_SESSION = 0x00000020,
	DRM_DISP_FEATURE_FBDC = 0x00000040,
};

struct mtk_drm_disp_caps_info {
	unsigned int disp_feature_flag;
	int lcm_degree; /* for rotate180 */
	unsigned int rsz_in_max[2]; /* for RPO { width, height } */

	/* for WCG */
	int lcm_color_mode;
	unsigned int max_luminance;
	unsigned int average_luminance;
	unsigned int min_luminance;
};

struct drm_mtk_session_info {
	unsigned int session_id;
	unsigned int vsyncFPS;
	unsigned int physicalWidthUm;
	unsigned int physicalHeightUm;
};

#define DRM_IOCTL_MTK_GEM_CREATE	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_GEM_CREATE, struct drm_mtk_gem_create)

#define DRM_IOCTL_MTK_GEM_MAP_OFFSET	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_GEM_MAP_OFFSET, struct drm_mtk_gem_map_off)

#define DRM_IOCTL_MTK_GEM_SUBMIT	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_GEM_SUBMIT, struct drm_mtk_gem_submit)

#define DRM_IOCTL_MTK_SESSION_CREATE	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_SESSION_CREATE, struct drm_mtk_session)

#define DRM_IOCTL_MTK_SESSION_DESTROY	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_SESSION_DESTROY, struct drm_mtk_session)

#define DRM_IOCTL_MTK_LAYERING_RULE	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_LAYERING_RULE, struct drm_mtk_layering_info)

#define DRM_IOCTL_MTK_CRTC_GETFENCE	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_CRTC_GETFENCE, struct drm_mtk_fence)

#define DRM_IOCTL_MTK_WAIT_REPAINT	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_WAIT_REPAINT, unsigned int)

#define DRM_IOCTL_MTK_GET_DISPLAY_CAPS	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_GET_DISPLAY_CAPS, struct mtk_drm_disp_caps_info)

#define DRM_IOCTL_MTK_SET_DDP_MODE     DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_SET_DDP_MODE, unsigned int)

#define DRM_IOCTL_MTK_GET_SESSION_INFO     DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_GET_SESSION_INFO, struct drm_mtk_session_info)

#define MTK_DRM_ADVANCE
#define MTK_DRM_FORMAT_DIM		fourcc_code('D', ' ', '0', '0')
#endif /* _UAPI_MEDIATEK_DRM_H */
