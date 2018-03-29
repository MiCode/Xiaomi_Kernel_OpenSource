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

#ifndef __DISP_SESSION_H
#define __DISP_SESSION_H

#define DISP_SESSION_DEVICE	"mtk_disp_mgr"


#define DISP_NO_ION_FD                 ((int)(~0U>>1))
#define DISP_NO_USE_LAEYR_ID           ((int)(~0U>>1))

#define MAX_INPUT_CONFIG		4
#define MAKE_DISP_FORMAT_ID(id, bpp)  (((id) << 8) | (bpp))
#define DISP_SESSION_MODE(id) (((id)>>24)&0xff)
#define DISP_SESSION_TYPE(id) (((id)>>16)&0xff)
#define DISP_SESSION_DEV(id) ((id)&0xff)
#define MAKE_DISP_SESSION(type, dev) (unsigned int)((type) << 16 | (dev))



/* /============================================================================= */
/* structure declarations */
/* /=========================== */

typedef enum {
	DISP_IF_TYPE_DBI = 0,
	DISP_IF_TYPE_DPI,
	DISP_IF_TYPE_DSI0,
	DISP_IF_TYPE_DSI1,
	DISP_IF_TYPE_DSIDUAL,
	DISP_IF_HDMI = 7,
	DISP_IF_HDMI_SMARTBOOK,
	DISP_IF_MHL
} DISP_IF_TYPE;

typedef enum {
	DISP_IF_FORMAT_RGB565 = 0,
	DISP_IF_FORMAT_RGB666,
	DISP_IF_FORMAT_RGB888
} DISP_IF_FORMAT;

typedef enum {
	DISP_IF_MODE_VIDEO = 0,
	DISP_IF_MODE_COMMAND
} DISP_IF_MODE;


typedef enum {
	DISP_ORIENTATION_0 = 0,
	DISP_ORIENTATION_90 = 1,
	DISP_ORIENTATION_180 = 2,
	DISP_ORIENTATION_270 = 3,
} DISP_ORIENTATION;

typedef enum {
	DISP_FORMAT_UNKNOWN = 0,

	DISP_FORMAT_RGB565 = MAKE_DISP_FORMAT_ID(1, 2),
	DISP_FORMAT_RGB888 = MAKE_DISP_FORMAT_ID(2, 3),
	DISP_FORMAT_BGR888 = MAKE_DISP_FORMAT_ID(3, 3),
	DISP_FORMAT_ARGB8888 = MAKE_DISP_FORMAT_ID(4, 4),
	DISP_FORMAT_ABGR8888 = MAKE_DISP_FORMAT_ID(5, 4),
	DISP_FORMAT_RGBA8888 = MAKE_DISP_FORMAT_ID(6, 4),
	DISP_FORMAT_BGRA8888 = MAKE_DISP_FORMAT_ID(7, 4),
	DISP_FORMAT_YUV422 = MAKE_DISP_FORMAT_ID(8, 2),
	DISP_FORMAT_XRGB8888 = MAKE_DISP_FORMAT_ID(9, 4),
	DISP_FORMAT_XBGR8888 = MAKE_DISP_FORMAT_ID(10, 4),
	DISP_FORMAT_RGBX8888 = MAKE_DISP_FORMAT_ID(11, 4),
	DISP_FORMAT_BGRX8888 = MAKE_DISP_FORMAT_ID(12, 4),
	DISP_FORMAT_UYVY = MAKE_DISP_FORMAT_ID(13, 2),
	DISP_FORMAT_YUV420_P = MAKE_DISP_FORMAT_ID(14, 2),
	DISP_FORMAT_YV12 = MAKE_DISP_FORMAT_ID(16, 1),	/* BPP = 1.5 */
	DISP_FORMAT_BPP_MASK = 0xFF,
} DISP_FORMAT;

typedef enum {
	DISP_LAYER_2D = 0,
	DISP_LAYER_3D_SBS_0 = 0x1,
	DISP_LAYER_3D_SBS_90 = 0x2,
	DISP_LAYER_3D_SBS_180 = 0x3,
	DISP_LAYER_3D_SBS_270 = 0x4,
	DISP_LAYER_3D_TAB_0 = 0x10,
	DISP_LAYER_3D_TAB_90 = 0x20,
	DISP_LAYER_3D_TAB_180 = 0x30,
	DISP_LAYER_3D_TAB_270 = 0x40,
} DISP_LAYER_TYPE;

typedef enum {
	/* normal memory */
	DISP_NORMAL_BUFFER = 0,
	/* normal memory but should not be dumpped within screenshot */
	DISP_PROTECT_BUFFER = 1,
	/* secure memory */
	DISP_SECURE_BUFFER = 2,
	DISP_SECURE_BUFFER_SHIFT = 0x10002
} DISP_BUFFER_TYPE;

typedef enum {
	/* ion buffer */
	DISP_BUFFER_ION = 0,
	/* dim layer, const alpha */
	DISP_BUFFER_ALPHA = 1,
	/* mva buffer */
	DISP_BUFFER_MVA = 2,
} DISP_BUFFER_SOURCE;

typedef enum {
	DISP_ALPHA_ONE = 0,
	DISP_ALPHA_SRC = 1,
	DISP_ALPHA_SRC_INVERT = 2,
	DISP_ALPHA_INVALID = 3,
} DISP_ALPHA_TYPE;

typedef enum {
	DISP_SESSION_PRIMARY = 1,
	DISP_SESSION_EXTERNAL = 2,
	DISP_SESSION_MEMORY = 3
} DISP_SESSION_TYPE;

typedef enum {
	DISP_YUV_BT601_FULL = 0,
	DISP_YUV_BT601	= 1,
	DISP_YUV_BT709  = 2
} DISP_YUV_RANGE_ENUM;

typedef enum {
	DISP_INVALID_SESSION_MODE = 0,
	/* single output */
	DISP_SESSION_DIRECT_LINK_MODE = 1,
	DISP_SESSION_DECOUPLE_MODE = 2,

	/* two ouputs */
	DISP_SESSION_DIRECT_LINK_MIRROR_MODE = 3,
	DISP_SESSION_DECOUPLE_MIRROR_MODE = 4,
} DISP_MODE;

typedef enum {
	DISP_OUTPUT_UNKNOWN				= 0,
	DISP_OUTPUT_MEMORY				= 1,
	DISP_OUTPUT_DECOUPLE			= 2,
	DISP_OUTPUT_DIRECT_LINK			= 3,
	DISP_OUTPUT_DIRECT_LINK_MIRROR	= 4,
} DISP_OUTPUT_TYPE;



typedef struct disp_session_config_t {
	unsigned int type;
	unsigned int device_id;
	unsigned int mode;
	unsigned int session_id;
	unsigned int present_fence_idx;
	unsigned int output_type;
} disp_session_config;

typedef struct {
	unsigned int session_id;
	unsigned int vsync_cnt;
	uint64_t vsync_ts;
} disp_session_vsync_config;

typedef struct disp_input_config_t {
	unsigned int layer_id;
	unsigned int layer_enable;
	unsigned int buffer_source;
	void *src_base_addr;
	void *src_phy_addr;
	unsigned int src_direct_link;
	unsigned int src_fmt;
	unsigned int src_use_color_key;
	unsigned int src_color_key;
	unsigned int src_pitch;
	unsigned int src_offset_x, src_offset_y;
	unsigned int src_width, src_height;

	unsigned int tgt_offset_x, tgt_offset_y;
	unsigned int tgt_width, tgt_height;
	unsigned int layer_rotation;
	unsigned int layer_type;
	unsigned int video_rotation;

	unsigned int isTdshp;	/* set to 1, will go through tdshp first, then layer blending, then to color */

	unsigned int next_buff_idx;
	int identity;
	int connected_type;
	unsigned int security;
	unsigned int alpha_enable;
	unsigned int alpha;
	unsigned int sur_aen;
	unsigned int src_alpha;
	unsigned int dst_alpha;
	unsigned int frm_sequence;
	unsigned int yuv_range;

	unsigned int fps;
	int64_t timestamp;
} disp_input_config;

typedef struct disp_output_config_t {
	void *va;
	void *pa;
	unsigned int fmt;
	unsigned int x;
	unsigned int y;
	unsigned int width;
	unsigned int height;
	unsigned int pitch;
	unsigned int pitchUV;
	unsigned int security;
	unsigned int buff_idx;
	unsigned int interface_idx;
	unsigned int frm_sequence;
} disp_output_config;

typedef struct disp_session_input_config_t {
	unsigned int session_id;
	unsigned int config_layer_num;
	disp_input_config config[MAX_INPUT_CONFIG];
} disp_session_input_config;

typedef struct disp_session_output_config_t {
	unsigned int session_id;
	disp_output_config config;
} disp_session_output_config;

typedef struct disp_session_layer_num_config_t {
	unsigned int session_id;
	unsigned int max_layer_num;
} disp_session_layer_num_config;

typedef struct disp_session_info_t {
	unsigned int session_id;
	unsigned int maxLayerNum;
	unsigned int isHwVsyncAvailable;
	DISP_IF_TYPE displayType;
	unsigned int displayWidth;
	unsigned int displayHeight;
	unsigned int displayFormat;
	DISP_IF_MODE displayMode;
	unsigned int vsyncFPS;
	unsigned int physicalWidth;
	unsigned int physicalHeight;
	unsigned int isConnected;
	unsigned int isHDCPSupported;
	unsigned int isOVLDisabled;
} disp_session_info;

typedef struct disp_buffer_info_t {
	/* Session */
	unsigned int session_id;
	/* Input */
	unsigned int layer_id;
	unsigned int layer_en;
	int ion_fd;
	unsigned int cache_sync;
	/* Output */
	unsigned int index;
	int fence_fd;
	unsigned int interface_index;
	int interface_fence_fd;
} disp_buffer_info;

typedef struct disp_present_fence_info_t {
	/* input */
	unsigned int session_id;
	/* output */
	unsigned int present_fence_fd;
	unsigned int present_fence_index;
} disp_present_fence;

typedef struct disp_present_fence_t {
	/* Session */
	unsigned int session_id;

	/* Output */
	unsigned int index;
	int fence_fd;
} disp_present_fence_info;

typedef enum {
	DISP_OUTPUT_CAP_DIRECT_LINK = 0,
	DISP_OUTPUT_CAP_DECOUPLE,
	DISP_OUTPUT_CAP_SWITCHABLE,
} DISP_CAP_OUTPUT_MODE;

typedef enum {
	DISP_OUTPUT_CAP_SINGLE_PASS = 0,
	DISP_OUTPUT_CAP_MULTI_PASS,
} DISP_CAP_OUTPUT_PASS;

typedef struct disp_caps_t {
	DISP_CAP_OUTPUT_MODE output_mode;
	DISP_CAP_OUTPUT_PASS output_pass;
	unsigned int max_layer_num;
} disp_caps_info;


enum DISP_SCENARIO {
	DISP_SCENARIO_NORMAL,
	DISP_SCENARIO_SELF_REFRESH,
};
struct disp_scenario_config_t {
	unsigned int session_id;
	unsigned int scenario;
};

/* IOCTL commands. */
#define DISP_IOW(num, dtype)     _IOW('O', num, dtype)
#define DISP_IOR(num, dtype)     _IOR('O', num, dtype)
#define DISP_IOWR(num, dtype)    _IOWR('O', num, dtype)
#define DISP_IO(num)             _IO('O', num)


#define	DISP_IOCTL_CREATE_SESSION				DISP_IOW(201, disp_session_config)
#define	DISP_IOCTL_DESTROY_SESSION				DISP_IOW(202, disp_session_config)
#define	DISP_IOCTL_TRIGGER_SESSION				DISP_IOW(203, disp_session_config)
#define DISP_IOCTL_PREPARE_INPUT_BUFFER			DISP_IOW(204, disp_buffer_info)
#define DISP_IOCTL_PREPARE_OUTPUT_BUFFER		DISP_IOW(205, disp_buffer_info)
#define DISP_IOCTL_SET_INPUT_BUFFER			DISP_IOW(206, disp_session_input_config)
#define DISP_IOCTL_SET_OUTPUT_BUFFER			DISP_IOW(207, disp_session_output_config)
#define	DISP_IOCTL_GET_SESSION_INFO				DISP_IOW(208, disp_session_info)


#define	DISP_IOCTL_SET_SESSION_MODE			    DISP_IOW(209, disp_session_config)
#define	DISP_IOCTL_GET_SESSION_MODE			    DISP_IOW(210, disp_session_config)
#define	DISP_IOCTL_SET_SESSION_TYPE				DISP_IOW(211, disp_session_config)
#define	DISP_IOCTL_GET_SESSION_TYPE				DISP_IOW(212, disp_session_config)
#define	DISP_IOCTL_WAIT_FOR_VSYNC				DISP_IOW(213, disp_session_vsync_config)
#define	DISP_IOCTL_SET_MAX_LAYER_NUM			DISP_IOW(214, disp_session_layer_num_config)
#define	DISP_IOCTL_SET_VSYNC_FPS				DISP_IOW(215, unsigned int)
#define	DISP_IOCTL_GET_PRESENT_FENCE			DISP_IOW(216, disp_present_fence)

#define	DISP_IOCTL_SET_SCENARIO				DISP_IOW(222, struct disp_scenario_config_t)

#endif				/* __DISP_SESSION_H */
