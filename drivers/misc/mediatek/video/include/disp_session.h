/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Joey Pan <joey.pan@mediatek.com>
 */

#ifndef __DISP_SESSION_H
#define __DISP_SESSION_H
#include <linux/types.h>

#define DISP_SESSION_DEVICE	"mtk_disp_mgr"

#define DISP_NO_ION_FD                 ((int)(~0U>>1))
#define DISP_NO_USE_LAEYR_ID           ((int)(~0U>>1))

#define MAKE_DISP_FORMAT_ID(id, bpp)  (((id) << 8) | (bpp))
#define DISP_SESSION_MODE(id) (((id)>>24)&0xff)
#define DISP_SESSION_TYPE(id) (((id)>>16)&0xff)
#define DISP_SESSION_DEV(id) ((id)&0xff)
#define MAKE_DISP_SESSION(type, dev) (unsigned int)((type)<<16 | (dev))

#define RSZ_RES_LIST_NUM 8

/* /=========================== */
/* structure declarations */
/* /=========================== */

enum DISP_IF_TYPE {
	DISP_IF_TYPE_DBI = 0,
	DISP_IF_TYPE_DPI,
	DISP_IF_TYPE_DSI0,
	DISP_IF_TYPE_DSI1,
	DISP_IF_TYPE_DSIDUAL,
	DISP_IF_HDMI = 7,
	DISP_IF_HDMI_SMARTBOOK,
	DISP_IF_MHL,
	DISP_IF_EPD,
	DISP_IF_SLIMPORT
};

enum DISP_IF_FORMAT {
	DISP_IF_FORMAT_RGB565 = 0,
	DISP_IF_FORMAT_RGB666,
	DISP_IF_FORMAT_RGB888
};

enum DISP_IF_MODE {
	DISP_IF_MODE_VIDEO = 0,
	DISP_IF_MODE_COMMAND
};


enum DISP_ORIENTATION {
	DISP_ORIENTATION_0 = 0,
	DISP_ORIENTATION_90 = 1,
	DISP_ORIENTATION_180 = 2,
	DISP_ORIENTATION_270 = 3,
};

enum DISP_FORMAT {
	DISP_FORMAT_UNKNOWN = 0,

	DISP_FORMAT_RGB565 =	MAKE_DISP_FORMAT_ID(1, 2),
	DISP_FORMAT_RGB888 =	MAKE_DISP_FORMAT_ID(2, 3),
	DISP_FORMAT_BGR888 =	MAKE_DISP_FORMAT_ID(3, 3),
	DISP_FORMAT_ARGB8888 =	MAKE_DISP_FORMAT_ID(4, 4),
	DISP_FORMAT_ABGR8888 =	MAKE_DISP_FORMAT_ID(5, 4),
	DISP_FORMAT_RGBA8888 =	MAKE_DISP_FORMAT_ID(6, 4),
	DISP_FORMAT_BGRA8888 =	MAKE_DISP_FORMAT_ID(7, 4),
	DISP_FORMAT_YUV422 =	MAKE_DISP_FORMAT_ID(8, 2),
	DISP_FORMAT_XRGB8888 =	MAKE_DISP_FORMAT_ID(9, 4),
	DISP_FORMAT_XBGR8888 =	MAKE_DISP_FORMAT_ID(10, 4),
	DISP_FORMAT_RGBX8888 =	MAKE_DISP_FORMAT_ID(11, 4),
	DISP_FORMAT_BGRX8888 =	MAKE_DISP_FORMAT_ID(12, 4),
	DISP_FORMAT_UYVY =	MAKE_DISP_FORMAT_ID(13, 2),
	DISP_FORMAT_YUV420_P =	MAKE_DISP_FORMAT_ID(14, 2),
	DISP_FORMAT_YV12 =	MAKE_DISP_FORMAT_ID(16, 1), /* BPP = 1.5 */
	DISP_FORMAT_PARGB8888 =	MAKE_DISP_FORMAT_ID(17, 4),
	DISP_FORMAT_PABGR8888 =	MAKE_DISP_FORMAT_ID(18, 4),
	DISP_FORMAT_PRGBA8888 =	MAKE_DISP_FORMAT_ID(19, 4),
	DISP_FORMAT_PBGRA8888 =	MAKE_DISP_FORMAT_ID(20, 4),
	DISP_FORMAT_DIM =	MAKE_DISP_FORMAT_ID(21, 0),
	DISP_FORMAT_RGBA1010102 =	MAKE_DISP_FORMAT_ID(22, 4),
	DISP_FORMAT_PRGBA1010102 =	MAKE_DISP_FORMAT_ID(23, 4),
	DISP_FORMAT_RGBA_FP16 =		MAKE_DISP_FORMAT_ID(24, 8),
	DISP_FORMAT_PRGBA_FP16 =	MAKE_DISP_FORMAT_ID(25, 8),
	DISP_FORMAT_NUM =	MAKE_DISP_FORMAT_ID(26, 0),
	DISP_FORMAT_BPP_MASK =	0xFF,
};

enum DISP_LAYER_TYPE {
	DISP_LAYER_2D = 0,
	DISP_LAYER_3D_SBS_0 = 0x1,
	DISP_LAYER_3D_SBS_90 = 0x2,
	DISP_LAYER_3D_SBS_180 = 0x3,
	DISP_LAYER_3D_SBS_270 = 0x4,
	DISP_LAYER_3D_TAB_0 = 0x10,
	DISP_LAYER_3D_TAB_90 = 0x20,
	DISP_LAYER_3D_TAB_180 = 0x30,
	DISP_LAYER_3D_TAB_270 = 0x40,
};

enum DISP_BUFFER_TYPE {
	/* normal memory */
	DISP_NORMAL_BUFFER = 0,
	/* normal memory but should not be dumpped within screenshot */
	DISP_PROTECT_BUFFER = 1,
	/* secure memory */
	DISP_SECURE_BUFFER = 2,
	DISP_SECURE_BUFFER_SHIFT = 0x10002
};

enum DISP_BUFFER_SOURCE {
	/* ion buffer */
	DISP_BUFFER_ION = 0,
	/* dim layer, const alpha */
	DISP_BUFFER_ALPHA = 1,
	/* mva buffer */
	DISP_BUFFER_MVA = 2,
};

enum DISP_ALPHA_TYPE {
	DISP_ALPHA_ONE = 0,
	DISP_ALPHA_SRC = 1,
	DISP_ALPHA_SRC_INVERT = 2,
	DISP_ALPHA_INVALID = 3,
};

enum DISP_SESSION_TYPE {
	DISP_SESSION_PRIMARY = 1,
	DISP_SESSION_EXTERNAL = 2,
	DISP_SESSION_MEMORY = 3
};

enum DISP_YUV_RANGE_ENUM {
	DISP_YUV_BT601_FULL = 0,
	DISP_YUV_BT601 = 1,
	DISP_YUV_BT709 = 2
};

enum DISP_MODE {
	DISP_INVALID_SESSION_MODE = 0,
	/* single output */
	DISP_SESSION_DIRECT_LINK_MODE = 1,
	DISP_SESSION_DECOUPLE_MODE = 2,

	/* two ouputs */
	DISP_SESSION_DIRECT_LINK_MIRROR_MODE = 3,
	DISP_SESSION_DECOUPLE_MIRROR_MODE = 4,

	DISP_SESSION_RDMA_MODE,
	DISP_SESSION_DUAL_DIRECT_LINK_MODE,
	DISP_SESSION_DUAL_DECOUPLE_MODE,
	DISP_SESSION_DUAL_RDMA_MODE,
	/* three session at same time */
	DISP_SESSION_TRIPLE_DIRECT_LINK_MODE,
	DISP_SESSION_MODE_NUM,

};

enum DISP_SESSION_USER {
	SESSION_USER_INVALID = -1,
	SESSION_USER_HWC = 0,
	SESSION_USER_GUIEXT = 1,
	SESSION_USER_AEE = 2,
	SESSION_USER_PANDISP = 3,
	SESSION_USER_CNT,
};

enum DISP_DC_TYPE {
	DISP_OUTPUT_UNKNOWN = 0,
	DISP_OUTPUT_MEMORY = 1,
	DISP_OUTPUT_DECOUPLE = 2,
};

enum EXTD_TRIGGER_MODE {
	TRIGGER_NORMAL = 0,
	TRIGGER_SUSPEND = 1,
	TRIGGER_RESUME = 2,
	TRIGGER_MODE_MAX_NUM,
};

struct disp_session_config {
	enum DISP_SESSION_TYPE type;
	unsigned int device_id;
	enum DISP_MODE mode;
	unsigned int session_id;
	enum DISP_SESSION_USER user;
	unsigned int present_fence_idx;
	enum DISP_DC_TYPE dc_type;
	int need_merge;
	enum EXTD_TRIGGER_MODE tigger_mode;
};

struct disp_session_vsync_config {
	unsigned int session_id;
	unsigned int vsync_cnt;
	unsigned long long vsync_ts;
	int lcm_fps;
};

struct layer_dirty_roi {
	__u16 dirty_x;
	__u16 dirty_y;
	__u16 dirty_w;
	__u16 dirty_h;
};

struct disp_input_config {
	void *src_base_addr;
	void *src_phy_addr;
	enum DISP_BUFFER_SOURCE buffer_source;
	enum DISP_BUFFER_TYPE security;
	enum DISP_FORMAT src_fmt;
	enum DISP_ALPHA_TYPE src_alpha;
	enum DISP_ALPHA_TYPE dst_alpha;
	enum DISP_YUV_RANGE_ENUM yuv_range;
	int dataspace;

	enum DISP_ORIENTATION layer_rotation;
	enum DISP_LAYER_TYPE layer_type;
	enum DISP_ORIENTATION video_rotation;

	__u32 next_buff_idx;
	/* fence to be waited before using this buffer. -1 if invalid */
	int src_fence_fd;
	/* fence struct of src_fence_fd, used in kernel */
	void *src_fence_struct;

	__u32 src_color_key;
	__u32 frm_sequence;
	__u32 dim_color;

	void *dirty_roi_addr;
	__u16 dirty_roi_num;

	__u16 src_v_pitch;
	__u16 src_pitch;
	__u16 src_offset_x, src_offset_y;
	__u16 src_width, src_height;
	__u16 tgt_offset_x, tgt_offset_y;
	__u16 tgt_width, tgt_height;

	__u8 alpha_enable;
	__u8 alpha;
	__u8 sur_aen;
	__u8 src_use_color_key;
	__u8 layer_id;
	__u8 layer_enable;
	__u8 src_direct_link;

	__u8 isTdshp;
	__u8 identity;
	__u8 connected_type;
	__s8 ext_sel_layer;

	__u8 compress;
};

struct disp_output_config {
	void *va;
	void *pa;
	enum DISP_FORMAT fmt;
	unsigned int x;
	unsigned int y;
	unsigned int width;
	unsigned int height;
	unsigned int pitch;
	unsigned int pitchUV;
	enum DISP_BUFFER_TYPE security;
	unsigned int buff_idx;
	unsigned int interface_idx;
	/* fence to be waited before using this buffer. -1 if invalid */
	int src_fence_fd;
	/* fence struct of src_fence_fd, used in kernel */
	void *src_fence_struct;
	unsigned int frm_sequence;
};

struct disp_ccorr_config {
	bool is_dirty;
	int mode;
	int color_matrix[16];
	bool featureFlag;
};

struct disp_session_input_config {
	enum DISP_SESSION_USER setter;
	unsigned int session_id;
	unsigned int config_layer_num;
	struct disp_input_config config[12];
	struct disp_ccorr_config ccorr_config;
};

struct disp_session_output_config {
	unsigned int session_id;
	struct disp_output_config config;
};

struct disp_session_layer_num_config {
	unsigned int session_id;
	unsigned int max_layer_num;
};

struct disp_frame_cfg_t {
	enum DISP_SESSION_USER setter;
	unsigned int session_id;

	/* input config */
	unsigned int input_layer_num;
	struct disp_input_config input_cfg[12];
	unsigned int overlap_layer_num;

	/* constant layer */
	unsigned int const_layer_num;
	struct disp_input_config const_layer[1];

	/* output config */
	int output_en;
	struct disp_output_config output_cfg;

	/* trigger config */
	enum DISP_MODE mode;
	unsigned int present_fence_idx;
	int prev_present_fence_fd;
	void *prev_present_fence_struct;
	enum EXTD_TRIGGER_MODE tigger_mode;
	enum DISP_SESSION_USER user;

	/* ccorr config */
	struct disp_ccorr_config ccorr_config;

	/* res_idx: SF/HWC selects which resolution to use */
	int res_idx;
	unsigned int hrt_weight;
	unsigned int hrt_idx;

	/* for panel HBM (High Backlight Mode) control */
	bool hbm_en;
	/*DynFPS*/
	int active_config;
};

struct disp_session_info {
	unsigned int session_id;
	unsigned int maxLayerNum;
	unsigned int isHwVsyncAvailable;
	enum DISP_IF_TYPE displayType;
	unsigned int displayWidth;
	unsigned int displayHeight;
	unsigned int displayFormat;
	enum DISP_IF_MODE displayMode;
	unsigned int vsyncFPS;
	unsigned int physicalWidth;
	unsigned int physicalHeight;
	/* length: um, for more precise precision */
	unsigned int physicalWidthUm;
	/* length: um, for more precise precision */
	unsigned int physicalHeightUm;
	unsigned int density;
	unsigned int isConnected;
	unsigned int isHDCPSupported;
	unsigned int isOVLDisabled;
	unsigned int is3DSupport;
	unsigned int const_layer_num;
	/* updateFPS: fps of HWC trigger display */
	/* notes: for better Accuracy, updateFPS = real_fps*100 */
	unsigned int updateFPS;
	unsigned int is_updateFPS_stable;
};

struct disp_buffer_info {
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
};

struct disp_present_fence {
	/* input */
	unsigned int session_id;
	/* output */
	int present_fence_fd;
	unsigned int present_fence_index;
};

struct disp_present_fence_info {
	/* Session */
	unsigned int session_id;

	/* Output */
	unsigned int index;
	int fence_fd;
};

enum DISP_CAP_OUTPUT_MODE {
	DISP_OUTPUT_CAP_DIRECT_LINK = 0,
	DISP_OUTPUT_CAP_DECOUPLE,
	DISP_OUTPUT_CAP_SWITCHABLE,
};

enum DISP_CAP_OUTPUT_PASS {
	DISP_OUTPUT_CAP_SINGLE_PASS = 0,
	DISP_OUTPUT_CAP_MULTI_PASS,
};

enum DISP_FEATURE {
	DISP_FEATURE_TIME_SHARING = 0x00000001,
	DISP_FEATURE_HRT = 0x00000002,
	DISP_FEATURE_PARTIAL = 0x00000004,
	DISP_FEATURE_FENCE_WAIT = 0x00000008,
	DISP_FEATURE_RSZ = 0x00000010,
	DISP_FEATURE_NO_PARGB = 0x00000020,
	DISP_FEATURE_DISP_SELF_REFRESH = 0x00000040,
	DISP_FEATURE_RPO = 0x00000080,
	DISP_FEATURE_FBDC = 0x00000100,
	DISP_FEATURE_FORCE_DISABLE_AOD = 0x00000200,
	DISP_FEATURE_ARR = 0x00000400,
	DISP_FEATURE_DYNFPS = 0x00000800
};

struct disp_caps_info {
	enum DISP_CAP_OUTPUT_MODE output_mode;
	enum DISP_CAP_OUTPUT_PASS output_pass;
	unsigned int max_layer_num;
	unsigned int disp_feature;
	int is_support_frame_cfg_ioctl;
	int is_output_rotated;
	int lcm_degree;

	/*
	 * resizer input resolution list
	 * format:
	 *   sequence from big resolution(LCM resolution) to small
	 *   portrait {width, height, rsz layer cnt to use}
	 * ex:
	 *   { 1440, 2560, 0},
	 *   { 1080, 1920, 1},
	 *   ...
	 */
	unsigned int rsz_in_res_list[RSZ_RES_LIST_NUM][3];
	unsigned int rsz_list_length;
	/* portrait { width, height } */
	unsigned int rsz_in_max[2];

	/* is_support_three_session:
	 *  1: support three session at same time
	 *  0: not support three session at same time
	 */
	int is_support_three_session;
	int lcm_color_mode;
	unsigned int max_luminance;
	unsigned int average_luminance;
	unsigned int min_luminance;
};

struct disp_session_buf_info {
	unsigned int session_id;
	unsigned int buf_hnd[3];
};

enum LAYERING_CAPS {
	LAYERING_OVL_ONLY =	0x00000001,
	MDP_RSZ_LAYER =		0x00000002,
	DISP_RSZ_LAYER =	0x00000004,
	MDP_ROT_LAYER =		0x00000008,
	MDP_HDR_LAYER =		0x00000010,
	NO_FBDC =		0x00000020,
};

struct layer_config {
	unsigned int ovl_id;
	enum DISP_FORMAT src_fmt;
	int dataspace;
	unsigned int dst_offset_x, dst_offset_y;
	unsigned int dst_width, dst_height;
	int ext_sel_layer;
	unsigned int src_offset_x, src_offset_y;
	unsigned int src_width, src_height;
	unsigned int layer_caps;
	unsigned int clip; /* drv internal use */
	__u8 compress;
};

struct disp_layer_info {
	struct layer_config *input_config[2];
	int disp_mode[2];
	int layer_num[2];
	int gles_head[2];
	int gles_tail[2];
	int hrt_num;
	/* res_idx: SF/HWC selects which resolution to use */
	int res_idx;
	unsigned int hrt_weight;
	unsigned int hrt_idx;

	/*DynFPS*/
	int active_config_id[2];
};

enum DISP_SCENARIO {
	DISP_SCENARIO_NORMAL,
	DISP_SCENARIO_SELF_REFRESH,
	DISP_SCENARIO_FORCE_DC,
	DISP_SCENARIO_NUM,
};
struct disp_scenario_config_t {
	unsigned int session_id;
	unsigned int scenario;
};

enum DISP_UT_ERROR {
	DISP_UT_ERROR_OVL = 0x00000001,
	DISP_UT_ERROR_WDMA = 0x00000002,
	DISP_UT_ERROR_RDMA = 0x00000004,
	DISP_UT_ERROR_CMDQ_TIMEOUT = 0x00000008,
};

enum DISP_SELF_REFRESH_TYPE {
	WAIT_FOR_REFRESH,
	REFRESH_FOR_ANTI_LATENCY2,
	REFRESH_FOR_SWITCH_DECOUPLE,
	REFRESH_FOR_SWITCH_DECOUPLE_MIRROR,
	REFRESH_FOR_IDLE,
	REFRESH_TYPE_NUM,
};
struct dynamic_fps_levels {
	unsigned int fps_level_num;
	unsigned int fps_levels[10];
};

/*DynFPS start*/
#define MULTI_CONFIG_NUM 2
struct dyn_config_info {
	unsigned int vsyncFPS;
	unsigned int vact_timing_fps;/*active timing fps*/
	unsigned int width;
	unsigned int height;
};

/*only primary_display support*/
struct multi_configs {
	unsigned int config_num;
	struct dyn_config_info dyn_cfgs[MULTI_CONFIG_NUM];
};
/*DynFPS end*/

/* IOCTL commands. */
#define DISP_IOW(num, dtype)     _IOW('O', num, dtype)
#define DISP_IOR(num, dtype)     _IOR('O', num, dtype)
#define DISP_IOWR(num, dtype)    _IOWR('O', num, dtype)
#define DISP_IO(num)             _IO('O', num)


#define	DISP_IOCTL_CREATE_SESSION	\
	DISP_IOW(201, struct disp_session_config)
#define	DISP_IOCTL_DESTROY_SESSION	\
	DISP_IOW(202, struct disp_session_config)
#define	DISP_IOCTL_TRIGGER_SESSION	\
	DISP_IOW(203, struct disp_session_config)
#define	DISP_IOCTL_PREPARE_INPUT_BUFFER	\
	DISP_IOW(204, struct disp_buffer_info)
#define	DISP_IOCTL_PREPARE_OUTPUT_BUFFER	\
	DISP_IOW(205, struct disp_buffer_info)
#define	DISP_IOCTL_SET_INPUT_BUFFER	\
	DISP_IOW(206, struct disp_session_input_config)
#define	DISP_IOCTL_SET_OUTPUT_BUFFER	\
	DISP_IOW(207, struct disp_session_output_config)
#define	DISP_IOCTL_GET_SESSION_INFO	\
	DISP_IOW(208, struct disp_session_info)


#define	DISP_IOCTL_SET_SESSION_MODE	\
	DISP_IOW(209, struct disp_session_config)
#define	DISP_IOCTL_GET_SESSION_MODE	\
	DISP_IOW(210, struct disp_session_config)
#define	DISP_IOCTL_SET_SESSION_TYPE	\
	DISP_IOW(211, struct disp_session_config)
#define	DISP_IOCTL_GET_SESSION_TYPE	\
	DISP_IOW(212, struct disp_session_config)
#define	DISP_IOCTL_WAIT_FOR_VSYNC	\
	DISP_IOW(213, struct disp_session_vsync_config)
#define	DISP_IOCTL_SET_MAX_LAYER_NUM	\
	DISP_IOW(214, struct disp_session_layer_num_config)
#define	DISP_IOCTL_GET_VSYNC_FPS	\
	DISP_IOW(215, unsigned int)
#define	DISP_IOCTL_SET_VSYNC_FPS	\
	DISP_IOW(216, unsigned int)
#define	DISP_IOCTL_GET_PRESENT_FENCE	\
	DISP_IOW(217, struct disp_present_fence)

#define DISP_IOCTL_GET_IS_DRIVER_SUSPEND	\
	DISP_IOW(218, unsigned int)
#define DISP_IOCTL_GET_DISPLAY_CAPS	\
	DISP_IOW(219, struct disp_caps_info)
#define DISP_IOCTL_INSERT_SESSION_BUFFERS	\
	DISP_IOW(220, struct disp_session_buf_info)
#define	DISP_IOCTL_FRAME_CONFIG	\
	DISP_IOW(221, struct disp_session_output_config)
#define DISP_IOCTL_QUERY_VALID_LAYER	\
	DISP_IOW(222, struct disp_layer_info)
#define	DISP_IOCTL_SET_SCENARIO	\
	DISP_IOW(223, struct disp_scenario_config_t)
#define	DISP_IOCTL_WAIT_ALL_JOBS_DONE	\
	DISP_IOW(224, unsigned int)
#define	DISP_IOCTL_SCREEN_FREEZE	\
	DISP_IOW(225, unsigned int)
#define DISP_IOCTL_GET_UT_RESULT	\
	DISP_IOW(226, unsigned int)
#define DISP_IOCTL_WAIT_DISP_SELF_REFRESH	\
	DISP_IOW(227, unsigned int)
#define DISP_IOCTL_GET_MULTI_CONFIGS \
	DISP_IOR(231, struct multi_configs)
#ifdef __KERNEL__

int disp_mgr_get_session_info(struct disp_session_info *info);

#endif



#endif				/* __DISP_SESSION_H */
