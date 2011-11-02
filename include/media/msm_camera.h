#ifndef __LINUX_MSM_CAMERA_H
#define __LINUX_MSM_CAMERA_H

#ifdef MSM_CAMERA_BIONIC
#include <sys/types.h>
#endif
#include <linux/types.h>
#include <linux/ioctl.h>
#include <linux/cdev.h>
#ifdef MSM_CAMERA_GCC
#include <time.h>
#else
#include <linux/time.h>
#endif

#ifdef __KERNEL__
#include <linux/ion.h>
#endif
#define MSM_CAM_IOCTL_MAGIC 'm'

#define MSM_CAM_IOCTL_GET_SENSOR_INFO \
	_IOR(MSM_CAM_IOCTL_MAGIC, 1, struct msm_camsensor_info *)

#define MSM_CAM_IOCTL_REGISTER_PMEM \
	_IOW(MSM_CAM_IOCTL_MAGIC, 2, struct msm_pmem_info *)

#define MSM_CAM_IOCTL_UNREGISTER_PMEM \
	_IOW(MSM_CAM_IOCTL_MAGIC, 3, unsigned)

#define MSM_CAM_IOCTL_CTRL_COMMAND \
	_IOW(MSM_CAM_IOCTL_MAGIC, 4, struct msm_ctrl_cmd *)

#define MSM_CAM_IOCTL_CONFIG_VFE  \
	_IOW(MSM_CAM_IOCTL_MAGIC, 5, struct msm_camera_vfe_cfg_cmd *)

#define MSM_CAM_IOCTL_GET_STATS \
	_IOR(MSM_CAM_IOCTL_MAGIC, 6, struct msm_camera_stats_event_ctrl *)

#define MSM_CAM_IOCTL_GETFRAME \
	_IOR(MSM_CAM_IOCTL_MAGIC, 7, struct msm_camera_get_frame *)

#define MSM_CAM_IOCTL_ENABLE_VFE \
	_IOW(MSM_CAM_IOCTL_MAGIC, 8, struct camera_enable_cmd *)

#define MSM_CAM_IOCTL_CTRL_CMD_DONE \
	_IOW(MSM_CAM_IOCTL_MAGIC, 9, struct camera_cmd *)

#define MSM_CAM_IOCTL_CONFIG_CMD \
	_IOW(MSM_CAM_IOCTL_MAGIC, 10, struct camera_cmd *)

#define MSM_CAM_IOCTL_DISABLE_VFE \
	_IOW(MSM_CAM_IOCTL_MAGIC, 11, struct camera_enable_cmd *)

#define MSM_CAM_IOCTL_PAD_REG_RESET2 \
	_IOW(MSM_CAM_IOCTL_MAGIC, 12, struct camera_enable_cmd *)

#define MSM_CAM_IOCTL_VFE_APPS_RESET \
	_IOW(MSM_CAM_IOCTL_MAGIC, 13, struct camera_enable_cmd *)

#define MSM_CAM_IOCTL_RELEASE_FRAME_BUFFER \
	_IOW(MSM_CAM_IOCTL_MAGIC, 14, struct camera_enable_cmd *)

#define MSM_CAM_IOCTL_RELEASE_STATS_BUFFER \
	_IOW(MSM_CAM_IOCTL_MAGIC, 15, struct msm_stats_buf *)

#define MSM_CAM_IOCTL_AXI_CONFIG \
	_IOW(MSM_CAM_IOCTL_MAGIC, 16, struct msm_camera_vfe_cfg_cmd *)

#define MSM_CAM_IOCTL_GET_PICTURE \
	_IOW(MSM_CAM_IOCTL_MAGIC, 17, struct msm_frame *)

#define MSM_CAM_IOCTL_SET_CROP \
	_IOW(MSM_CAM_IOCTL_MAGIC, 18, struct crop_info *)

#define MSM_CAM_IOCTL_PICT_PP \
	_IOW(MSM_CAM_IOCTL_MAGIC, 19, uint8_t *)

#define MSM_CAM_IOCTL_PICT_PP_DONE \
	_IOW(MSM_CAM_IOCTL_MAGIC, 20, struct msm_snapshot_pp_status *)

#define MSM_CAM_IOCTL_SENSOR_IO_CFG \
	_IOW(MSM_CAM_IOCTL_MAGIC, 21, struct sensor_cfg_data *)

#define MSM_CAM_IOCTL_FLASH_LED_CFG \
	_IOW(MSM_CAM_IOCTL_MAGIC, 22, unsigned *)

#define MSM_CAM_IOCTL_UNBLOCK_POLL_FRAME \
	_IO(MSM_CAM_IOCTL_MAGIC, 23)

#define MSM_CAM_IOCTL_CTRL_COMMAND_2 \
	_IOW(MSM_CAM_IOCTL_MAGIC, 24, struct msm_ctrl_cmd *)

#define MSM_CAM_IOCTL_AF_CTRL \
	_IOR(MSM_CAM_IOCTL_MAGIC, 25, struct msm_ctrl_cmt_t *)

#define MSM_CAM_IOCTL_AF_CTRL_DONE \
	_IOW(MSM_CAM_IOCTL_MAGIC, 26, struct msm_ctrl_cmt_t *)

#define MSM_CAM_IOCTL_CONFIG_VPE \
	_IOW(MSM_CAM_IOCTL_MAGIC, 27, struct msm_camera_vpe_cfg_cmd *)

#define MSM_CAM_IOCTL_AXI_VPE_CONFIG \
	_IOW(MSM_CAM_IOCTL_MAGIC, 28, struct msm_camera_vpe_cfg_cmd *)

#define MSM_CAM_IOCTL_STROBE_FLASH_CFG \
	_IOW(MSM_CAM_IOCTL_MAGIC, 29, uint32_t *)

#define MSM_CAM_IOCTL_STROBE_FLASH_CHARGE \
	_IOW(MSM_CAM_IOCTL_MAGIC, 30, uint32_t *)

#define MSM_CAM_IOCTL_STROBE_FLASH_RELEASE \
	_IO(MSM_CAM_IOCTL_MAGIC, 31)

#define MSM_CAM_IOCTL_FLASH_CTRL \
	_IOW(MSM_CAM_IOCTL_MAGIC, 32, struct flash_ctrl_data *)

#define MSM_CAM_IOCTL_ERROR_CONFIG \
	_IOW(MSM_CAM_IOCTL_MAGIC, 33, uint32_t *)

#define MSM_CAM_IOCTL_ABORT_CAPTURE \
	_IO(MSM_CAM_IOCTL_MAGIC, 34)

#define MSM_CAM_IOCTL_SET_FD_ROI \
	_IOW(MSM_CAM_IOCTL_MAGIC, 35, struct fd_roi_info *)

#define MSM_CAM_IOCTL_GET_CAMERA_INFO \
	_IOR(MSM_CAM_IOCTL_MAGIC, 36, struct msm_camera_info *)

#define MSM_CAM_IOCTL_UNBLOCK_POLL_PIC_FRAME \
	_IO(MSM_CAM_IOCTL_MAGIC, 37)

#define MSM_CAM_IOCTL_RELEASE_PIC_BUFFER \
	_IOW(MSM_CAM_IOCTL_MAGIC, 38, struct camera_enable_cmd *)

#define MSM_CAM_IOCTL_PUT_ST_FRAME \
	_IOW(MSM_CAM_IOCTL_MAGIC, 39, struct msm_camera_st_frame *)

#define MSM_CAM_IOCTL_GET_CONFIG_INFO \
	_IOR(MSM_CAM_IOCTL_MAGIC, 40, struct msm_cam_config_dev_info *)

#define MSM_CAM_IOCTL_V4L2_EVT_NOTIFY \
	_IOR(MSM_CAM_IOCTL_MAGIC, 41, struct v4l2_event *)

#define MSM_CAM_IOCTL_SET_MEM_MAP_INFO \
	_IOR(MSM_CAM_IOCTL_MAGIC, 42, struct msm_mem_map_info *)

#define MSM_CAM_IOCTL_ACTUATOR_IO_CFG \
	_IOW(MSM_CAM_IOCTL_MAGIC, 43, struct msm_actuator_cfg_data *)

#define MSM_CAM_IOCTL_MCTL_POST_PROC \
	_IOW(MSM_CAM_IOCTL_MAGIC, 44, struct msm_mctl_post_proc_cmd *)

#define MSM_CAM_IOCTL_RESERVE_FREE_FRAME \
	_IOW(MSM_CAM_IOCTL_MAGIC, 45, struct msm_cam_evt_divert_frame *)

#define MSM_CAM_IOCTL_RELEASE_FREE_FRAME \
	_IOR(MSM_CAM_IOCTL_MAGIC, 46, struct msm_cam_evt_divert_frame *)

struct msm_mctl_pp_cmd {
	int32_t  id;
	uint16_t length;
	void     *value;
};

struct msm_mctl_post_proc_cmd {
	int32_t type;
	struct msm_mctl_pp_cmd cmd;
};

#define MSM_CAMERA_LED_OFF  0
#define MSM_CAMERA_LED_LOW  1
#define MSM_CAMERA_LED_HIGH 2
#define MSM_CAMERA_LED_INIT 3
#define MSM_CAMERA_LED_RELEASE 4

#define MSM_CAMERA_STROBE_FLASH_NONE 0
#define MSM_CAMERA_STROBE_FLASH_XENON 1

#define MSM_MAX_CAMERA_SENSORS  5
#define MAX_SENSOR_NAME 32

#define MSM_MAX_CAMERA_CONFIGS 2

#define PP_SNAP  0x01
#define PP_RAW_SNAP ((0x01)<<1)
#define PP_PREV  ((0x01)<<2)
#define PP_MASK		(PP_SNAP|PP_RAW_SNAP|PP_PREV)

#define MSM_CAM_CTRL_CMD_DONE  0
#define MSM_CAM_SENSOR_VFE_CMD 1

/* Should be same as VIDEO_MAX_PLANES in videodev2.h */
#define MAX_PLANES 8

/*****************************************************
 *  structure
 *****************************************************/

/* define five type of structures for userspace <==> kernel
 * space communication:
 * command 1 - 2 are from userspace ==> kernel
 * command 3 - 4 are from kernel ==> userspace
 *
 * 1. control command: control command(from control thread),
 *                     control status (from config thread);
 */
struct msm_ctrl_cmd {
	uint16_t type;
	uint16_t length;
	void *value;
	uint16_t status;
	uint32_t timeout_ms;
	int resp_fd; /* FIXME: to be used by the kernel, pass-through for now */
	int vnode_id;  /* video dev id. Can we overload resp_fd? */
	uint32_t stream_type; /* used to pass value to qcamera server */
	int config_ident; /*used as identifier for config node*/
};

struct msm_cam_evt_msg {
	unsigned short type;	/* 1 == event (RPC), 0 == message (adsp) */
	unsigned short msg_id;
	unsigned int len;	/* size in, number of bytes out */
	uint32_t frame_id;
	void *data;
};

struct msm_pp_frame_sp {
	/* phy addr of the buffer */
	unsigned long  phy_addr;
	uint32_t       y_off;
	uint32_t       cbcr_off;
	/* buffer length */
	uint32_t       length;
	int32_t        fd;
	uint32_t       addr_offset;
	/* mapped addr */
	unsigned long  vaddr;
};

struct msm_pp_frame_mp {
	/* phy addr of the plane */
	unsigned long  phy_addr;
	/* offset of plane data */
	uint32_t       data_offset;
	/* plane length */
	uint32_t       length;
	int32_t        fd;
	uint32_t       addr_offset;
	/* mapped addr */
	unsigned long  vaddr;
};

struct msm_pp_frame {
	uint32_t       handle; /* stores vb cookie */
	uint32_t       frame_id;
	int            path;
	unsigned short image_type;
	unsigned short num_planes; /* 1 for sp */
	struct timeval timestamp;
	union {
		struct msm_pp_frame_sp sp;
		struct msm_pp_frame_mp mp[MAX_PLANES];
	};
};

struct msm_cam_evt_divert_frame {
	unsigned short image_mode;
	unsigned short op_mode;
	unsigned short inst_idx;
	unsigned short node_idx;
	struct msm_pp_frame frame;
	int            do_pp;
};

struct msm_mctl_pp_cmd_ack_event {
	uint32_t cmd;        /* VPE_CMD_ZOOM? */
	int      status;     /* 0 done, < 0 err */
	uint32_t cookie;     /* daemon's cookie */
};

struct msm_mctl_pp_event_info {
	int32_t  event;
	union {
		struct msm_mctl_pp_cmd_ack_event ack;
	};
};

struct msm_isp_event_ctrl {
	unsigned short resptype;
	union {
		struct msm_cam_evt_msg isp_msg;
		struct msm_ctrl_cmd ctrl;
		struct msm_cam_evt_divert_frame div_frame;
		struct msm_mctl_pp_event_info pp_event_info;
	} isp_data;
};

#define MSM_CAM_RESP_CTRL              0
#define MSM_CAM_RESP_STAT_EVT_MSG      1
#define MSM_CAM_RESP_STEREO_OP_1       2
#define MSM_CAM_RESP_STEREO_OP_2       3
#define MSM_CAM_RESP_V4L2              4
#define MSM_CAM_RESP_DIV_FRAME_EVT_MSG 5
#define MSM_CAM_RESP_DONE_EVENT        6
#define MSM_CAM_RESP_MCTL_PP_EVENT     7
#define MSM_CAM_RESP_MAX               8

#define MSM_CAM_APP_NOTIFY_EVENT  0

/* this one is used to send ctrl/status up to config thread */

struct msm_stats_event_ctrl {
	/* 0 - ctrl_cmd from control thread,
	 * 1 - stats/event kernel,
	 * 2 - V4L control or read request */
	int resptype;
	int timeout_ms;
	struct msm_ctrl_cmd ctrl_cmd;
	/* struct  vfe_event_t  stats_event; */
	struct msm_cam_evt_msg stats_event;
};

/* 2. config command: config command(from config thread); */
struct msm_camera_cfg_cmd {
	/* what to config:
	 * 1 - sensor config, 2 - vfe config */
	uint16_t cfg_type;

	/* sensor config type */
	uint16_t cmd_type;
	uint16_t queue;
	uint16_t length;
	void *value;
};

#define CMD_GENERAL			0
#define CMD_AXI_CFG_OUT1		1
#define CMD_AXI_CFG_SNAP_O1_AND_O2	2
#define CMD_AXI_CFG_OUT2		3
#define CMD_PICT_T_AXI_CFG		4
#define CMD_PICT_M_AXI_CFG		5
#define CMD_RAW_PICT_AXI_CFG		6

#define CMD_FRAME_BUF_RELEASE		7
#define CMD_PREV_BUF_CFG		8
#define CMD_SNAP_BUF_RELEASE		9
#define CMD_SNAP_BUF_CFG		10
#define CMD_STATS_DISABLE		11
#define CMD_STATS_AEC_AWB_ENABLE	12
#define CMD_STATS_AF_ENABLE		13
#define CMD_STATS_AEC_ENABLE		14
#define CMD_STATS_AWB_ENABLE		15
#define CMD_STATS_ENABLE  		16

#define CMD_STATS_AXI_CFG		17
#define CMD_STATS_AEC_AXI_CFG		18
#define CMD_STATS_AF_AXI_CFG 		19
#define CMD_STATS_AWB_AXI_CFG		20
#define CMD_STATS_RS_AXI_CFG		21
#define CMD_STATS_CS_AXI_CFG		22
#define CMD_STATS_IHIST_AXI_CFG		23
#define CMD_STATS_SKIN_AXI_CFG		24

#define CMD_STATS_BUF_RELEASE		25
#define CMD_STATS_AEC_BUF_RELEASE	26
#define CMD_STATS_AF_BUF_RELEASE	27
#define CMD_STATS_AWB_BUF_RELEASE	28
#define CMD_STATS_RS_BUF_RELEASE	29
#define CMD_STATS_CS_BUF_RELEASE	30
#define CMD_STATS_IHIST_BUF_RELEASE	31
#define CMD_STATS_SKIN_BUF_RELEASE	32

#define UPDATE_STATS_INVALID		33
#define CMD_AXI_CFG_SNAP_GEMINI		34
#define CMD_AXI_CFG_SNAP		35
#define CMD_AXI_CFG_PREVIEW		36
#define CMD_AXI_CFG_VIDEO		37

#define CMD_STATS_IHIST_ENABLE 38
#define CMD_STATS_RS_ENABLE 39
#define CMD_STATS_CS_ENABLE 40
#define CMD_VPE 41
#define CMD_AXI_CFG_VPE 42
#define CMD_AXI_CFG_ZSL 43
#define CMD_AXI_CFG_SNAP_VPE 44
#define CMD_AXI_CFG_SNAP_THUMB_VPE 45
#define CMD_AXI_CFG_VIDEO_ALL_CHNLS  46
#define CMD_AXI_CFG_ZSL_ALL_CHNLS  47
#define CMD_CONFIG_PING_ADDR 48
#define CMD_CONFIG_PONG_ADDR 49
#define CMD_CONFIG_FREE_BUF_ADDR 50

/* vfe config command: config command(from config thread)*/
struct msm_vfe_cfg_cmd {
	int cmd_type;
	uint16_t length;
	void *value;
};

struct msm_vpe_cfg_cmd {
	int cmd_type;
	uint16_t length;
	void *value;
};

#define MAX_CAMERA_ENABLE_NAME_LEN 32
struct camera_enable_cmd {
	char name[MAX_CAMERA_ENABLE_NAME_LEN];
};

#define MSM_PMEM_OUTPUT1		0
#define MSM_PMEM_OUTPUT2		1
#define MSM_PMEM_OUTPUT1_OUTPUT2	2
#define MSM_PMEM_THUMBNAIL		3
#define MSM_PMEM_MAINIMG		4
#define MSM_PMEM_RAW_MAINIMG		5
#define MSM_PMEM_AEC_AWB		6
#define MSM_PMEM_AF			7
#define MSM_PMEM_AEC			8
#define MSM_PMEM_AWB			9
#define MSM_PMEM_RS			10
#define MSM_PMEM_CS			11
#define MSM_PMEM_IHIST			12
#define MSM_PMEM_SKIN			13
#define MSM_PMEM_VIDEO			14
#define MSM_PMEM_PREVIEW		15
#define MSM_PMEM_VIDEO_VPE		16
#define MSM_PMEM_C2D			17
#define MSM_PMEM_MAINIMG_VPE    18
#define MSM_PMEM_THUMBNAIL_VPE  19
#define MSM_PMEM_MAX            20

#define STAT_AEAW			0
#define STAT_AEC			1
#define STAT_AF				2
#define STAT_AWB			3
#define STAT_RS				4
#define STAT_CS				5
#define STAT_IHIST			6
#define STAT_SKIN			7
#define STAT_MAX			8

#define FRAME_PREVIEW_OUTPUT1		0
#define FRAME_PREVIEW_OUTPUT2		1
#define FRAME_SNAPSHOT			2
#define FRAME_THUMBNAIL			3
#define FRAME_RAW_SNAPSHOT		4
#define FRAME_MAX			5

struct msm_pmem_info {
	int type;
	int fd;
	void *vaddr;
	uint32_t offset;
	uint32_t len;
	uint32_t y_off;
	uint32_t cbcr_off;
	uint8_t active;
};

struct outputCfg {
	uint32_t height;
	uint32_t width;

	uint32_t window_height_firstline;
	uint32_t window_height_lastline;
};

#define OUTPUT_1	0
#define OUTPUT_2	1
#define OUTPUT_1_AND_2            2   /* snapshot only */
#define OUTPUT_1_AND_3            3   /* video */
#define CAMIF_TO_AXI_VIA_OUTPUT_2 4
#define OUTPUT_1_AND_CAMIF_TO_AXI_VIA_OUTPUT_2 5
#define OUTPUT_2_AND_CAMIF_TO_AXI_VIA_OUTPUT_1 6
#define OUTPUT_1_2_AND_3 7
#define OUTPUT_ALL_CHNLS 8
#define OUTPUT_ZSL_ALL_CHNLS 9
#define LAST_AXI_OUTPUT_MODE_ENUM  OUTPUT_ZSL_ALL_CHNLS

#define MSM_FRAME_PREV_1	0
#define MSM_FRAME_PREV_2	1
#define MSM_FRAME_ENC		2

#define OUTPUT_TYPE_P    (1<<0)
#define OUTPUT_TYPE_T    (1<<1)
#define OUTPUT_TYPE_S    (1<<2)
#define OUTPUT_TYPE_V    (1<<3)
#define OUTPUT_TYPE_L    (1<<4)
#define OUTPUT_TYPE_ST_L (1<<5)
#define OUTPUT_TYPE_ST_R (1<<6)
#define OUTPUT_TYPE_ST_D (1<<7)

struct fd_roi_info {
	void *info;
	int info_len;
};

struct msm_mem_map_info {
	uint32_t cookie;
	uint32_t length;
	uint32_t mem_type;
};

#define MSM_MEM_MMAP		0
#define MSM_MEM_USERPTR		1
#define MSM_PLANE_MAX		8
#define MSM_PLANE_Y			0
#define MSM_PLANE_UV		1

struct msm_frame {
	struct timespec ts;
	int path;
	int type;
	unsigned long buffer;
	uint32_t phy_offset;
	uint32_t y_off;
	uint32_t cbcr_off;
	int fd;

	void *cropinfo;
	int croplen;
	uint32_t error_code;
	struct fd_roi_info roi_info;
	uint32_t frame_id;
	int stcam_quality_ind;
	uint32_t stcam_conv_value;

	struct ion_allocation_data ion_alloc;
	struct ion_fd_data fd_data;
};

enum msm_st_frame_packing {
	SIDE_BY_SIDE_HALF,
	SIDE_BY_SIDE_FULL,
	TOP_DOWN_HALF,
	TOP_DOWN_FULL,
};

struct msm_st_crop {
	uint32_t in_w;
	uint32_t in_h;
	uint32_t out_w;
	uint32_t out_h;
};

struct msm_st_half {
	uint32_t buf_y_off;
	uint32_t buf_cbcr_off;
	uint32_t buf_y_stride;
	uint32_t buf_cbcr_stride;
	uint32_t pix_x_off;
	uint32_t pix_y_off;
	struct msm_st_crop stCropInfo;
};

struct msm_st_frame {
	struct msm_frame buf_info;
	int type;
	enum msm_st_frame_packing packing;
	struct msm_st_half L;
	struct msm_st_half R;
	int frame_id;
};

#define MSM_CAMERA_ERR_MASK (0xFFFFFFFF & 1)

struct stats_buff {
	unsigned long buff;
	int fd;
};

struct msm_stats_buf {
	uint8_t awb_ymin;
	struct stats_buff aec;
	struct stats_buff awb;
	struct stats_buff af;
	struct stats_buff ihist;
	struct stats_buff rs;
	struct stats_buff cs;
	struct stats_buff skin;
	int type;
	uint32_t status_bits;
	unsigned long buffer;
	int fd;
	uint32_t frame_id;
};
#define MSM_V4L2_EXT_CAPTURE_MODE_DEFAULT 0
/* video capture mode in VIDIOC_S_PARM */
#define MSM_V4L2_EXT_CAPTURE_MODE_PREVIEW \
	(MSM_V4L2_EXT_CAPTURE_MODE_DEFAULT+1)
/* extendedmode for video recording in VIDIOC_S_PARM */
#define MSM_V4L2_EXT_CAPTURE_MODE_VIDEO \
	(MSM_V4L2_EXT_CAPTURE_MODE_DEFAULT+2)
/* extendedmode for the full size main image in VIDIOC_S_PARM */
#define MSM_V4L2_EXT_CAPTURE_MODE_MAIN (MSM_V4L2_EXT_CAPTURE_MODE_DEFAULT+3)
/* extendedmode for the thumb nail image in VIDIOC_S_PARM */
#define MSM_V4L2_EXT_CAPTURE_MODE_THUMBNAIL \
	(MSM_V4L2_EXT_CAPTURE_MODE_DEFAULT+4)
#define MSM_V4L2_EXT_CAPTURE_MODE_RAW \
	(MSM_V4L2_EXT_CAPTURE_MODE_DEFAULT+5)
#define MSM_V4L2_EXT_CAPTURE_MODE_MAX (MSM_V4L2_EXT_CAPTURE_MODE_DEFAULT+6)


#define MSM_V4L2_PID_MOTION_ISO              V4L2_CID_PRIVATE_BASE
#define MSM_V4L2_PID_EFFECT                 (V4L2_CID_PRIVATE_BASE+1)
#define MSM_V4L2_PID_HJR                    (V4L2_CID_PRIVATE_BASE+2)
#define MSM_V4L2_PID_LED_MODE               (V4L2_CID_PRIVATE_BASE+3)
#define MSM_V4L2_PID_PREP_SNAPSHOT          (V4L2_CID_PRIVATE_BASE+4)
#define MSM_V4L2_PID_EXP_METERING           (V4L2_CID_PRIVATE_BASE+5)
#define MSM_V4L2_PID_ISO                    (V4L2_CID_PRIVATE_BASE+6)
#define MSM_V4L2_PID_CAM_MODE               (V4L2_CID_PRIVATE_BASE+7)
#define MSM_V4L2_PID_LUMA_ADAPTATION	    (V4L2_CID_PRIVATE_BASE+8)
#define MSM_V4L2_PID_BEST_SHOT              (V4L2_CID_PRIVATE_BASE+9)
#define MSM_V4L2_PID_FOCUS_MODE	            (V4L2_CID_PRIVATE_BASE+10)
#define MSM_V4L2_PID_BL_DETECTION           (V4L2_CID_PRIVATE_BASE+11)
#define MSM_V4L2_PID_SNOW_DETECTION         (V4L2_CID_PRIVATE_BASE+12)
#define MSM_V4L2_PID_CTRL_CMD               (V4L2_CID_PRIVATE_BASE+13)
#define MSM_V4L2_PID_EVT_SUB_INFO           (V4L2_CID_PRIVATE_BASE+14)
#define MSM_V4L2_PID_STROBE_FLASH           (V4L2_CID_PRIVATE_BASE+15)
#define MSM_V4L2_PID_MMAP_ENTRY             (V4L2_CID_PRIVATE_BASE+16)
#define MSM_V4L2_PID_MMAP_INST              (V4L2_CID_PRIVATE_BASE+17)
#define MSM_V4L2_PID_MAX                    MSM_V4L2_PID_MMAP_INST

/* camera operation mode for video recording - two frame output queues */
#define MSM_V4L2_CAM_OP_DEFAULT         0
/* camera operation mode for video recording - two frame output queues */
#define MSM_V4L2_CAM_OP_PREVIEW         (MSM_V4L2_CAM_OP_DEFAULT+1)
/* camera operation mode for video recording - two frame output queues */
#define MSM_V4L2_CAM_OP_VIDEO           (MSM_V4L2_CAM_OP_DEFAULT+2)
/* camera operation mode for standard shapshot - two frame output queues */
#define MSM_V4L2_CAM_OP_CAPTURE         (MSM_V4L2_CAM_OP_DEFAULT+3)
/* camera operation mode for zsl shapshot - three output queues */
#define MSM_V4L2_CAM_OP_ZSL             (MSM_V4L2_CAM_OP_DEFAULT+4)
/* camera operation mode for raw snapshot - one frame output queue */
#define MSM_V4L2_CAM_OP_RAW             (MSM_V4L2_CAM_OP_DEFAULT+5)

#define MSM_V4L2_VID_CAP_TYPE	0
#define MSM_V4L2_STREAM_ON		1
#define MSM_V4L2_STREAM_OFF		2
#define MSM_V4L2_SNAPSHOT		3
#define MSM_V4L2_QUERY_CTRL		4
#define MSM_V4L2_GET_CTRL		5
#define MSM_V4L2_SET_CTRL		6
#define MSM_V4L2_QUERY			7
#define MSM_V4L2_GET_CROP		8
#define MSM_V4L2_SET_CROP		9
#define MSM_V4L2_OPEN			10
#define MSM_V4L2_CLOSE			11
#define MSM_V4L2_SET_CTRL_CMD	12
#define MSM_V4L2_EVT_SUB_MASK	13
#define MSM_V4L2_MAX			14
#define V4L2_CAMERA_EXIT		43

struct crop_info {
	void *info;
	int len;
};

struct msm_postproc {
	int ftnum;
	struct msm_frame fthumnail;
	int fmnum;
	struct msm_frame fmain;
};

struct msm_snapshot_pp_status {
	void *status;
};

#define CFG_SET_MODE			0
#define CFG_SET_EFFECT			1
#define CFG_START			2
#define CFG_PWR_UP			3
#define CFG_PWR_DOWN			4
#define CFG_WRITE_EXPOSURE_GAIN		5
#define CFG_SET_DEFAULT_FOCUS		6
#define CFG_MOVE_FOCUS			7
#define CFG_REGISTER_TO_REAL_GAIN	8
#define CFG_REAL_TO_REGISTER_GAIN	9
#define CFG_SET_FPS			10
#define CFG_SET_PICT_FPS		11
#define CFG_SET_BRIGHTNESS		12
#define CFG_SET_CONTRAST		13
#define CFG_SET_ZOOM			14
#define CFG_SET_EXPOSURE_MODE		15
#define CFG_SET_WB			16
#define CFG_SET_ANTIBANDING		17
#define CFG_SET_EXP_GAIN		18
#define CFG_SET_PICT_EXP_GAIN		19
#define CFG_SET_LENS_SHADING		20
#define CFG_GET_PICT_FPS		21
#define CFG_GET_PREV_L_PF		22
#define CFG_GET_PREV_P_PL		23
#define CFG_GET_PICT_L_PF		24
#define CFG_GET_PICT_P_PL		25
#define CFG_GET_AF_MAX_STEPS		26
#define CFG_GET_PICT_MAX_EXP_LC		27
#define CFG_SEND_WB_INFO    28
#define CFG_SENSOR_INIT    29
#define CFG_GET_3D_CALI_DATA 30
#define CFG_GET_CALIB_DATA		31
#define CFG_GET_OUTPUT_INFO		32
#define CFG_GET_EEPROM_DATA		33
#define CFG_SET_ACTUATOR_INFO		34
#define CFG_GET_ACTUATOR_INFO		35
#define CFG_MAX			36


#define MOVE_NEAR	0
#define MOVE_FAR	1

#define SENSOR_PREVIEW_MODE		0
#define SENSOR_SNAPSHOT_MODE		1
#define SENSOR_RAW_SNAPSHOT_MODE	2
#define SENSOR_HFR_60FPS_MODE 3
#define SENSOR_HFR_90FPS_MODE 4
#define SENSOR_HFR_120FPS_MODE 5

#define SENSOR_QTR_SIZE			0
#define SENSOR_FULL_SIZE		1
#define SENSOR_QVGA_SIZE		2
#define SENSOR_INVALID_SIZE		3

#define CAMERA_EFFECT_OFF		0
#define CAMERA_EFFECT_MONO		1
#define CAMERA_EFFECT_NEGATIVE		2
#define CAMERA_EFFECT_SOLARIZE		3
#define CAMERA_EFFECT_SEPIA		4
#define CAMERA_EFFECT_POSTERIZE		5
#define CAMERA_EFFECT_WHITEBOARD	6
#define CAMERA_EFFECT_BLACKBOARD	7
#define CAMERA_EFFECT_AQUA		8
#define CAMERA_EFFECT_EMBOSS		9
#define CAMERA_EFFECT_SKETCH		10
#define CAMERA_EFFECT_NEON		11
#define CAMERA_EFFECT_MAX		12

/* QRD */
#define CAMERA_EFFECT_BW		10
#define CAMERA_EFFECT_BLUISH	12
#define CAMERA_EFFECT_REDDISH	13
#define CAMERA_EFFECT_GREENISH	14

/* QRD */
#define CAMERA_ANTIBANDING_OFF		0
#define CAMERA_ANTIBANDING_50HZ		2
#define CAMERA_ANTIBANDING_60HZ		1
#define CAMERA_ANTIBANDING_AUTO		3

#define CAMERA_CONTRAST_LV0			0
#define CAMERA_CONTRAST_LV1			1
#define CAMERA_CONTRAST_LV2			2
#define CAMERA_CONTRAST_LV3			3
#define CAMERA_CONTRAST_LV4			4
#define CAMERA_CONTRAST_LV5			5
#define CAMERA_CONTRAST_LV6			6
#define CAMERA_CONTRAST_LV7			7
#define CAMERA_CONTRAST_LV8			8
#define CAMERA_CONTRAST_LV9			9

#define CAMERA_BRIGHTNESS_LV0			0
#define CAMERA_BRIGHTNESS_LV1			1
#define CAMERA_BRIGHTNESS_LV2			2
#define CAMERA_BRIGHTNESS_LV3			3
#define CAMERA_BRIGHTNESS_LV4			4
#define CAMERA_BRIGHTNESS_LV5			5
#define CAMERA_BRIGHTNESS_LV6			6
#define CAMERA_BRIGHTNESS_LV7			7
#define CAMERA_BRIGHTNESS_LV8			8


#define CAMERA_SATURATION_LV0			0
#define CAMERA_SATURATION_LV1			1
#define CAMERA_SATURATION_LV2			2
#define CAMERA_SATURATION_LV3			3
#define CAMERA_SATURATION_LV4			4
#define CAMERA_SATURATION_LV5			5
#define CAMERA_SATURATION_LV6			6
#define CAMERA_SATURATION_LV7			7
#define CAMERA_SATURATION_LV8			8

#define CAMERA_SHARPNESS_LV0		0
#define CAMERA_SHARPNESS_LV1		3
#define CAMERA_SHARPNESS_LV2		6
#define CAMERA_SHARPNESS_LV3		9
#define CAMERA_SHARPNESS_LV4		12
#define CAMERA_SHARPNESS_LV5		15
#define CAMERA_SHARPNESS_LV6		18
#define CAMERA_SHARPNESS_LV7		21
#define CAMERA_SHARPNESS_LV8		24
#define CAMERA_SHARPNESS_LV9		27
#define CAMERA_SHARPNESS_LV10		30

#define CAMERA_SETAE_AVERAGE		0
#define CAMERA_SETAE_CENWEIGHT	1

#define CFG_SET_SATURATION		30
#define CFG_SET_SHARPNESS			31
#define CFG_SET_TOUCHAEC            32
#define CFG_SET_AUTO_FOCUS          33
#define CFG_SET_AUTOFLASH 34
/* QRD */
#define CFG_SET_EXPOSURE_COMPENSATION 35

#define  CAMERA_WB_AUTO               1 /* This list must match aeecamera.h */
#define  CAMERA_WB_CUSTOM             2
#define  CAMERA_WB_INCANDESCENT       3
#define  CAMERA_WB_FLUORESCENT        4
#define  CAMERA_WB_DAYLIGHT           5
#define  CAMERA_WB_CLOUDY_DAYLIGHT    6
#define  CAMERA_WB_TWILIGHT           7
#define  CAMERA_WB_SHADE              8

#define CAMERA_EXPOSURE_COMPENSATION_LV0			12
#define CAMERA_EXPOSURE_COMPENSATION_LV1			6
#define CAMERA_EXPOSURE_COMPENSATION_LV2			0
#define CAMERA_EXPOSURE_COMPENSATION_LV3			-6
#define CAMERA_EXPOSURE_COMPENSATION_LV4			-12


struct sensor_pict_fps {
	uint16_t prevfps;
	uint16_t pictfps;
};

struct exp_gain_cfg {
	uint16_t gain;
	uint32_t line;
};

struct focus_cfg {
	int32_t steps;
	int dir;
};

struct fps_cfg {
	uint16_t f_mult;
	uint16_t fps_div;
	uint32_t pict_fps_div;
};
struct wb_info_cfg {
	uint16_t red_gain;
	uint16_t green_gain;
	uint16_t blue_gain;
};
struct sensor_3d_exp_cfg {
	uint16_t gain;
	uint32_t line;
	uint16_t r_gain;
	uint16_t b_gain;
	uint16_t gr_gain;
	uint16_t gb_gain;
	uint16_t gain_adjust;
};
struct sensor_3d_cali_data_t{
	unsigned char left_p_matrix[3][4][8];
	unsigned char right_p_matrix[3][4][8];
	unsigned char square_len[8];
	unsigned char focal_len[8];
	unsigned char pixel_pitch[8];
	uint16_t left_r;
	uint16_t left_b;
	uint16_t left_gb;
	uint16_t left_af_far;
	uint16_t left_af_mid;
	uint16_t left_af_short;
	uint16_t left_af_5um;
	uint16_t left_af_50up;
	uint16_t left_af_50down;
	uint16_t right_r;
	uint16_t right_b;
	uint16_t right_gb;
	uint16_t right_af_far;
	uint16_t right_af_mid;
	uint16_t right_af_short;
	uint16_t right_af_5um;
	uint16_t right_af_50up;
	uint16_t right_af_50down;
};
struct sensor_init_cfg {
	uint8_t prev_res;
	uint8_t pict_res;
};

struct sensor_calib_data {
	/* Color Related Measurements */
	uint16_t r_over_g;
	uint16_t b_over_g;
	uint16_t gr_over_gb;

	/* Lens Related Measurements */
	uint16_t macro_2_inf;
	uint16_t inf_2_macro;
	uint16_t stroke_amt;
	uint16_t af_pos_1m;
	uint16_t af_pos_inf;
};

enum msm_sensor_resolution_t {
	MSM_SENSOR_RES_FULL,
	MSM_SENSOR_RES_QTR,
	MSM_SENSOR_RES_2,
	MSM_SENSOR_RES_3,
	MSM_SENSOR_RES_4,
	MSM_SENSOR_RES_5,
	MSM_SENSOR_RES_6,
	MSM_SENSOR_RES_7,
	MSM_SENSOR_INVALID_RES,
};

struct msm_sensor_output_info_t {
	uint16_t x_output;
	uint16_t y_output;
	uint16_t line_length_pclk;
	uint16_t frame_length_lines;
	uint32_t vt_pixel_clk;
	uint32_t op_pixel_clk;
	uint16_t binning_factor;
};

struct sensor_output_info_t {
	struct msm_sensor_output_info_t *output_info;
	uint16_t num_info;
};

struct sensor_eeprom_data_t {
	void *eeprom_data;
	uint16_t index;
};

struct mirror_flip {
	int32_t x_mirror;
	int32_t y_flip;
};

struct cord {
	uint32_t x;
	uint32_t y;
};

struct sensor_cfg_data {
	int cfgtype;
	int mode;
	int rs;
	uint8_t max_steps;

	union {
		int8_t effect;
		uint8_t lens_shading;
		uint16_t prevl_pf;
		uint16_t prevp_pl;
		uint16_t pictl_pf;
		uint16_t pictp_pl;
		uint32_t pict_max_exp_lc;
		uint16_t p_fps;
		struct sensor_init_cfg init_info;
		struct sensor_pict_fps gfps;
		struct exp_gain_cfg exp_gain;
		struct focus_cfg focus;
		struct fps_cfg fps;
		struct wb_info_cfg wb_info;
		struct sensor_3d_exp_cfg sensor_3d_exp;
		struct sensor_calib_data calib_info;
		struct sensor_output_info_t output_info;
		struct sensor_eeprom_data_t eeprom_data;
		/* QRD */
		uint16_t antibanding;
		uint8_t contrast;
		uint8_t saturation;
		uint8_t sharpness;
		int8_t brightness;
		int ae_mode;
		uint8_t wb_val;
		int8_t exp_compensation;
		struct cord aec_cord;
		int is_autoflash;
		struct mirror_flip mirror_flip;
	} cfg;
};

struct msm_actuator_move_params_t {
	int8_t dir;
	int32_t num_steps;
};

struct msm_actuator_set_info_t {
	uint32_t total_steps;
	uint16_t gross_steps;
	uint16_t fine_steps;
};

struct msm_actuator_get_info_t {
	uint32_t focal_length_num;
	uint32_t focal_length_den;
	uint32_t f_number_num;
	uint32_t f_number_den;
	uint32_t f_pix_num;
	uint32_t f_pix_den;
	uint32_t total_f_dist_num;
	uint32_t total_f_dist_den;
};

struct msm_actuator_cfg_data {
	int cfgtype;
	uint8_t is_af_supported;
	union {
		struct msm_actuator_move_params_t move;
		struct msm_actuator_set_info_t set_info;
		struct msm_actuator_get_info_t get_info;
	} cfg;
};

struct sensor_large_data {
	int cfgtype;
	union {
		struct sensor_3d_cali_data_t sensor_3d_cali_data;
	} data;
};

enum sensor_type_t {
	BAYER,
	YUV,
	JPEG_SOC,
};

enum flash_type {
	LED_FLASH,
	STROBE_FLASH,
};

enum strobe_flash_ctrl_type {
	STROBE_FLASH_CTRL_INIT,
	STROBE_FLASH_CTRL_CHARGE,
	STROBE_FLASH_CTRL_RELEASE
};

struct strobe_flash_ctrl_data {
	enum strobe_flash_ctrl_type type;
	int charge_en;
};

struct msm_camera_info {
	int num_cameras;
	uint8_t has_3d_support[MSM_MAX_CAMERA_SENSORS];
	uint8_t is_internal_cam[MSM_MAX_CAMERA_SENSORS];
	uint32_t s_mount_angle[MSM_MAX_CAMERA_SENSORS];
	const char *video_dev_name[MSM_MAX_CAMERA_SENSORS];
	enum sensor_type_t sensor_type[MSM_MAX_CAMERA_SENSORS];

};

struct msm_cam_config_dev_info {
	int num_config_nodes;
	const char *config_dev_name[MSM_MAX_CAMERA_CONFIGS];
	int config_dev_id[MSM_MAX_CAMERA_CONFIGS];
};

struct flash_ctrl_data {
	int flashtype;
	union {
		int led_state;
		struct strobe_flash_ctrl_data strobe_ctrl;
	} ctrl_data;
};

#define GET_NAME			0
#define GET_PREVIEW_LINE_PER_FRAME	1
#define GET_PREVIEW_PIXELS_PER_LINE	2
#define GET_SNAPSHOT_LINE_PER_FRAME	3
#define GET_SNAPSHOT_PIXELS_PER_LINE	4
#define GET_SNAPSHOT_FPS		5
#define GET_SNAPSHOT_MAX_EP_LINE_CNT	6

struct msm_camsensor_info {
	char name[MAX_SENSOR_NAME];
	uint8_t flash_enabled;
	int8_t total_steps;
	uint8_t support_3d;
};

#define V4L2_SINGLE_PLANE	0
#define V4L2_MULTI_PLANE_Y	0
#define V4L2_MULTI_PLANE_CBCR	1
#define V4L2_MULTI_PLANE_CB	1
#define V4L2_MULTI_PLANE_CR	2

struct plane_data {
	int plane_id;
	uint32_t offset;
	unsigned long size;
};

struct img_plane_info {
	uint32_t width;
	uint32_t height;
	uint32_t pixelformat;
	uint8_t buffer_type; /*Single/Multi planar*/
	uint8_t output_port;
	uint32_t ext_mode;
	uint8_t num_planes;
	struct plane_data plane[MAX_PLANES];
	uint32_t sp_y_offset;
	uint8_t vpe_can_use;
};

#endif /* __LINUX_MSM_CAMERA_H */
