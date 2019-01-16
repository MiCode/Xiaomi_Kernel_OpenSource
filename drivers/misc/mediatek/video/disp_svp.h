/**
 * NOTICE:
 * MUST BE consistent with bionic/libc/kernel/common/linux/disp_svp.h
 */
#ifndef __DISP_SVP_H
#define __DISP_SVP_H

#define DISP_NO_ION_FD                 ((int)(~0U>>1))
#define DISP_NO_USE_LAEYR_ID           ((int)(~0U>>1))

#define MAX_INPUT_CONFIG		4
#define MAKE_DISP_FORMAT_ID(id, bpp)  (((id) << 8) | (bpp))

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
	DISP_FORMAT_XRGB8888 = MAKE_DISP_FORMAT_ID(8, 4),
	DISP_FORMAT_XBGR8888 = MAKE_DISP_FORMAT_ID(9, 4),
	/* Packed YUV Formats */
	DISP_FORMAT_YUV444 = MAKE_DISP_FORMAT_ID(10, 3),
	DISP_FORMAT_YVYU = MAKE_DISP_FORMAT_ID(11, 2),	/* Same as UYVY, but replace Y/U/V */
	DISP_FORMAT_VYUY = MAKE_DISP_FORMAT_ID(12, 2),
	DISP_FORMAT_UYVY = MAKE_DISP_FORMAT_ID(13, 2),
	DISP_FORMAT_Y422 = MAKE_DISP_FORMAT_ID(13, 2),
	DISP_FORMAT_YUYV = MAKE_DISP_FORMAT_ID(14, 2),	/* Same as UYVY but replace U/V */
	DISP_FORMAT_YUY2 = MAKE_DISP_FORMAT_ID(14, 2),
	DISP_FORMAT_YUV422 = MAKE_DISP_FORMAT_ID(14, 2),	/* Will be removed */
	DISP_FORMAT_GREY = MAKE_DISP_FORMAT_ID(15, 1),	/* Single Y plane */
	DISP_FORMAT_Y800 = MAKE_DISP_FORMAT_ID(15, 1),
	DISP_FORMAT_Y8 = MAKE_DISP_FORMAT_ID(15, 1),
	/* Planar YUV Formats */
	DISP_FORMAT_YV12 = MAKE_DISP_FORMAT_ID(16, 1),	/* BPP = 1.5 */
	DISP_FORMAT_I420 = MAKE_DISP_FORMAT_ID(17, 1),	/* Same as YV12 but replace U/V */
	DISP_FORMAT_IYUV = MAKE_DISP_FORMAT_ID(17, 1),
	DISP_FORMAT_NV12 = MAKE_DISP_FORMAT_ID(18, 1),	/* BPP = 1.5 */
	DISP_FORMAT_NV21 = MAKE_DISP_FORMAT_ID(19, 1),	/* Same as NV12 but replace U/V */

	DISP_FORMAT_BPP_MASK = 0xFFFF,
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
	/* secure memory */
	DISP_SECURE_BUFFER = 1,
	/* normal memory but should not be dumpped within screenshot */
	DISP_PROTECT_BUFFER = 2,
	DISP_SECURE_BUFFER_SHIFT = 0x10001
} DISP_BUFFER_TYPE;

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
	/* single output */
	DISP_SESSION_DIRECT_LINK_MODE = 1,
	DISP_SESSION_DECOUPLE_MODE = 2,

	/* two ouputs */
	DISP_SESSION_DIRECT_LINK_MIRROR_MODE = 3,
	DISP_SESSION_DECOUPLE_MIRROR_MODE = 4,
} DISP_MODE;

typedef struct disp_session_config_t {
	DISP_SESSION_TYPE type;
	unsigned int device_id;
	DISP_MODE mode;
	unsigned int session_id;
} disp_session_config;

typedef struct {
	unsigned int session_id;
	unsigned int vsync_cnt;
	long int vsync_ts;
} disp_session_vsync_config;

typedef struct disp_input_config_t {
	unsigned int layer_id;
	unsigned int layer_enable;

	void *src_base_addr;
	void *src_phy_addr;
	unsigned int src_direct_link;
	DISP_FORMAT src_fmt;
	unsigned int src_use_color_key;
	unsigned int src_color_key;
	unsigned int src_pitch;
	unsigned int src_offset_x, src_offset_y;
	unsigned int src_width, src_height;

	unsigned int tgt_offset_x, tgt_offset_y;
	unsigned int tgt_width, tgt_height;
	DISP_ORIENTATION layer_rotation;
	DISP_LAYER_TYPE layer_type;
	DISP_ORIENTATION video_rotation;

	unsigned int isTdshp;	/* set to 1, will go through tdshp first, then layer blending, then to color */

	unsigned int next_buff_idx;
	int identity;
	int connected_type;
	DISP_BUFFER_TYPE security;
	unsigned int alpha_enable;
	unsigned int alpha;
	unsigned int sur_aen;
	DISP_ALPHA_TYPE src_alpha;
	DISP_ALPHA_TYPE dst_alpha;
} disp_input_config;

typedef struct disp_output_config_t {
	unsigned int va;
	unsigned int pa;
	DISP_FORMAT fmt;
	unsigned int x;
	unsigned int y;
	unsigned int width;
	unsigned int height;
	unsigned int pitch;
	unsigned int pitchUV;
	DISP_BUFFER_TYPE security;
	unsigned int buff_idx;
} disp_output_config;

#define MAX_INPUT_CONFIG 4

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
} disp_buffer_info;
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


#endif				/* __DISP_SVP_H */
