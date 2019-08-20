#ifndef __UAPI_MSM_CAMERA_H
#define __UAPI_MSM_CAMERA_H

#define CAM_API_V1

#include <linux/videodev2.h>
#include <linux/types.h>
#include <linux/ioctl.h>
#include <linux/media.h>

#include <linux/msm_ion.h>

#define BIT(nr)   (1UL << (nr))

#define MSM_CAM_IOCTL_MAGIC 'm'

#define MAX_SERVER_PAYLOAD_LENGTH 8192

#define MSM_CAM_IOCTL_GET_SENSOR_INFO \
	_IOR(MSM_CAM_IOCTL_MAGIC, 1, struct msm_camsensor_info *)

#define MSM_CAM_IOCTL_REGISTER_PMEM \
	_IOW(MSM_CAM_IOCTL_MAGIC, 2, struct msm_pmem_info *)

#define MSM_CAM_IOCTL_UNREGISTER_PMEM \
	_IOW(MSM_CAM_IOCTL_MAGIC, 3, unsigned int)

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
	_IOW(MSM_CAM_IOCTL_MAGIC, 22, unsigned int *)

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

#define MSM_CAM_IOCTL_V4L2_EVT_NOTIFY \
	_IOW(MSM_CAM_IOCTL_MAGIC, 40, struct v4l2_event_and_payload)

#define MSM_CAM_IOCTL_SET_MEM_MAP_INFO \
	_IOR(MSM_CAM_IOCTL_MAGIC, 41, struct msm_mem_map_info *)

#define MSM_CAM_IOCTL_ACTUATOR_IO_CFG \
	_IOW(MSM_CAM_IOCTL_MAGIC, 42, struct msm_actuator_cfg_data *)

#define MSM_CAM_IOCTL_MCTL_POST_PROC \
	_IOW(MSM_CAM_IOCTL_MAGIC, 43, struct msm_mctl_post_proc_cmd *)

#define MSM_CAM_IOCTL_RESERVE_FREE_FRAME \
	_IOW(MSM_CAM_IOCTL_MAGIC, 44, struct msm_cam_evt_divert_frame *)

#define MSM_CAM_IOCTL_RELEASE_FREE_FRAME \
	_IOR(MSM_CAM_IOCTL_MAGIC, 45, struct msm_cam_evt_divert_frame *)

#define MSM_CAM_IOCTL_PICT_PP_DIVERT_DONE \
	_IOR(MSM_CAM_IOCTL_MAGIC, 46, struct msm_pp_frame *)

#define MSM_CAM_IOCTL_SENSOR_V4l2_S_CTRL \
	_IOR(MSM_CAM_IOCTL_MAGIC, 47, struct v4l2_control)

#define MSM_CAM_IOCTL_SENSOR_V4l2_QUERY_CTRL \
	_IOR(MSM_CAM_IOCTL_MAGIC, 48, struct v4l2_queryctrl)

#define MSM_CAM_IOCTL_GET_KERNEL_SYSTEM_TIME \
	_IOW(MSM_CAM_IOCTL_MAGIC, 49, struct timeval *)

#define MSM_CAM_IOCTL_SET_VFE_OUTPUT_TYPE \
	_IOW(MSM_CAM_IOCTL_MAGIC, 50, uint32_t *)

#define MSM_CAM_IOCTL_MCTL_DIVERT_DONE \
	_IOR(MSM_CAM_IOCTL_MAGIC, 51, struct msm_cam_evt_divert_frame *)

#define MSM_CAM_IOCTL_GET_ACTUATOR_INFO \
	_IOW(MSM_CAM_IOCTL_MAGIC, 52, struct msm_actuator_cfg_data *)

#define MSM_CAM_IOCTL_EEPROM_IO_CFG \
	_IOW(MSM_CAM_IOCTL_MAGIC, 53, struct msm_eeprom_cfg_data *)

#define MSM_CAM_IOCTL_ISPIF_IO_CFG \
	_IOR(MSM_CAM_IOCTL_MAGIC, 54, struct ispif_cfg_data *)

#define MSM_CAM_IOCTL_STATS_REQBUF \
	_IOR(MSM_CAM_IOCTL_MAGIC, 55, struct msm_stats_reqbuf *)

#define MSM_CAM_IOCTL_STATS_ENQUEUEBUF \
	_IOR(MSM_CAM_IOCTL_MAGIC, 56, struct msm_stats_buf_info *)

#define MSM_CAM_IOCTL_STATS_FLUSH_BUFQ \
	_IOR(MSM_CAM_IOCTL_MAGIC, 57, struct msm_stats_flush_bufq *)

#define MSM_CAM_IOCTL_SET_MCTL_SDEV \
	_IOW(MSM_CAM_IOCTL_MAGIC, 58, struct msm_mctl_set_sdev_data *)

#define MSM_CAM_IOCTL_UNSET_MCTL_SDEV \
	_IOW(MSM_CAM_IOCTL_MAGIC, 59, struct msm_mctl_set_sdev_data *)

#define MSM_CAM_IOCTL_GET_INST_HANDLE \
	_IOR(MSM_CAM_IOCTL_MAGIC, 60, uint32_t *)

#define MSM_CAM_IOCTL_STATS_UNREG_BUF \
	_IOR(MSM_CAM_IOCTL_MAGIC, 61, struct msm_stats_flush_bufq *)

#define MSM_CAM_IOCTL_CSIC_IO_CFG \
	_IOWR(MSM_CAM_IOCTL_MAGIC, 62, struct csic_cfg_data *)

#define MSM_CAM_IOCTL_CSID_IO_CFG \
	_IOWR(MSM_CAM_IOCTL_MAGIC, 63, struct csid_cfg_data *)

#define MSM_CAM_IOCTL_CSIPHY_IO_CFG \
	_IOR(MSM_CAM_IOCTL_MAGIC, 64, struct csiphy_cfg_data *)

#define MSM_CAM_IOCTL_OEM \
	_IOW(MSM_CAM_IOCTL_MAGIC, 65, struct sensor_cfg_data *)

#define MSM_CAM_IOCTL_AXI_INIT \
	_IOWR(MSM_CAM_IOCTL_MAGIC, 66, uint8_t *)

#define MSM_CAM_IOCTL_AXI_RELEASE \
	_IO(MSM_CAM_IOCTL_MAGIC, 67)

struct v4l2_event_and_payload {
	struct v4l2_event evt;
	uint32_t payload_length;
	uint32_t transaction_id;
	void *payload;
};

struct msm_stats_reqbuf {
	int num_buf;		/* how many buffers requested */
	int stats_type;	/* stats type */
};

struct msm_stats_flush_bufq {
	int stats_type;	/* enum msm_stats_enum_type */
};

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
#define MAX_CAM_NAME_SIZE 32
#define MAX_ACT_MOD_NAME_SIZE 32
#define MAX_ACT_NAME_SIZE 32
#define NUM_ACTUATOR_DIR 2
#define MAX_ACTUATOR_SCENARIO 8
#define MAX_ACTUATOR_REGION 5
#define MAX_ACTUATOR_INIT_SET 12
#define MAX_ACTUATOR_TYPE_SIZE 32
#define MAX_ACTUATOR_REG_TBL_SIZE 8


#define MSM_MAX_CAMERA_CONFIGS 2

#define PP_SNAP  0x01
#define PP_RAW_SNAP ((0x01)<<1)
#define PP_PREV  ((0x01)<<2)
#define PP_THUMB ((0x01)<<3)
#define PP_MASK		(PP_SNAP|PP_RAW_SNAP|PP_PREV|PP_THUMB)

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
	int queue_idx;
	uint32_t evt_id;
	uint32_t stream_type; /* used to pass value to qcamera server */
	int config_ident; /*used as identifier for config node*/
};

struct msm_cam_evt_msg {
	unsigned short type;	/* 1 == event (RPC), 0 == message (adsp) */
	unsigned short msg_id;
	unsigned int len;	/* size in, number of bytes out */
	uint32_t frame_id;
	void *data;
	struct timespec timestamp;
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
	unsigned short buf_idx;
	int            path;
	unsigned short image_type;
	unsigned short num_planes; /* 1 for sp */
	struct timeval timestamp;
	union {
		struct msm_pp_frame_sp sp;
		struct msm_pp_frame_mp mp[MAX_PLANES];
	};
	int node_type;
	uint32_t inst_handle;
};

struct msm_pp_crop {
	uint32_t  src_x;
	uint32_t  src_y;
	uint32_t  src_w;
	uint32_t  src_h;
	uint32_t  dst_x;
	uint32_t  dst_y;
	uint32_t  dst_w;
	uint32_t  dst_h;
	uint8_t update_flag;
};

struct msm_mctl_pp_frame_cmd {
	uint32_t cookie;
	uint8_t  vpe_output_action;
	struct msm_pp_frame src_frame;
	struct msm_pp_frame dest_frame;
	struct msm_pp_crop crop;
	int path;
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
#define MSM_CAM_APP_NOTIFY_ERROR_EVENT  1

/* this one is used to send ctrl/status up to config thread */

struct msm_stats_event_ctrl {
	/* 0 - ctrl_cmd from control thread,
	 * 1 - stats/event kernel,
	 * 2 - V4L control or read request
	 */
	int resptype;
	int timeout_ms;
	struct msm_ctrl_cmd ctrl_cmd;
	/* struct  vfe_event_t  stats_event; */
	struct msm_cam_evt_msg stats_event;
};

/* 2. config command: config command(from config thread); */
struct msm_camera_cfg_cmd {
	/* what to config:
	 * 1 - sensor config, 2 - vfe config
	 */
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
#define CMD_STATS_ENABLE		16

#define CMD_STATS_AXI_CFG		17
#define CMD_STATS_AEC_AXI_CFG		18
#define CMD_STATS_AF_AXI_CFG		19
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

#define CMD_CONFIG_PING_ADDR 46
#define CMD_CONFIG_PONG_ADDR 47
#define CMD_CONFIG_FREE_BUF_ADDR 48
#define CMD_AXI_CFG_ZSL_ALL_CHNLS 49
#define CMD_AXI_CFG_VIDEO_ALL_CHNLS 50
#define CMD_VFE_BUFFER_RELEASE 51
#define CMD_VFE_PROCESS_IRQ 52
#define CMD_STATS_BG_ENABLE 53
#define CMD_STATS_BF_ENABLE 54
#define CMD_STATS_BHIST_ENABLE 55
#define CMD_STATS_BG_BUF_RELEASE 56
#define CMD_STATS_BF_BUF_RELEASE 57
#define CMD_STATS_BHIST_BUF_RELEASE 58
#define CMD_VFE_PIX_SOF_COUNT_UPDATE 59
#define CMD_VFE_COUNT_PIX_SOF_ENABLE 60
#define CMD_STATS_BE_ENABLE 61
#define CMD_STATS_BE_BUF_RELEASE 62

#define CMD_AXI_CFG_PRIM               BIT(8)
#define CMD_AXI_CFG_PRIM_ALL_CHNLS     BIT(9)
#define CMD_AXI_CFG_SEC                BIT(10)
#define CMD_AXI_CFG_SEC_ALL_CHNLS      BIT(11)
#define CMD_AXI_CFG_TERT1              BIT(12)
#define CMD_AXI_CFG_TERT2              BIT(13)

#define CMD_AXI_START  0xE1
#define CMD_AXI_STOP   0xE2
#define CMD_AXI_RESET  0xE3
#define CMD_AXI_ABORT  0xE4



#define AXI_CMD_PREVIEW      BIT(0)
#define AXI_CMD_CAPTURE      BIT(1)
#define AXI_CMD_RECORD       BIT(2)
#define AXI_CMD_ZSL          BIT(3)
#define AXI_CMD_RAW_CAPTURE  BIT(4)
#define AXI_CMD_LIVESHOT     BIT(5)

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
#define MSM_PMEM_BAYER_GRID		20
#define MSM_PMEM_BAYER_FOCUS	21
#define MSM_PMEM_BAYER_HIST		22
#define MSM_PMEM_BAYER_EXPOSURE 23
#define MSM_PMEM_MAX            24

#define STAT_AEAW			0
#define STAT_AEC			1
#define STAT_AF				2
#define STAT_AWB			3
#define STAT_RS				4
#define STAT_CS				5
#define STAT_IHIST			6
#define STAT_SKIN			7
#define STAT_BG				8
#define STAT_BF				9
#define STAT_BE				10
#define STAT_BHIST			11
#define STAT_MAX			12

#define FRAME_PREVIEW_OUTPUT1		0
#define FRAME_PREVIEW_OUTPUT2		1
#define FRAME_SNAPSHOT			2
#define FRAME_THUMBNAIL			3
#define FRAME_RAW_SNAPSHOT		4
#define FRAME_MAX			5

enum msm_stats_enum_type {
	MSM_STATS_TYPE_AEC, /* legacy based AEC */
	MSM_STATS_TYPE_AF,  /* legacy based AF */
	MSM_STATS_TYPE_AWB, /* legacy based AWB */
	MSM_STATS_TYPE_RS,  /* legacy based RS */
	MSM_STATS_TYPE_CS,  /* legacy based CS */
	MSM_STATS_TYPE_IHIST,   /* legacy based HIST */
	MSM_STATS_TYPE_SKIN,    /* legacy based SKIN */
	MSM_STATS_TYPE_BG,  /* Bayer Grids */
	MSM_STATS_TYPE_BF,  /* Bayer Focus */
	MSM_STATS_TYPE_BE,  /* Bayer Exposure*/
	MSM_STATS_TYPE_BHIST,   /* Bayer Hist */
	MSM_STATS_TYPE_AE_AW,   /* legacy stats for vfe 2.x*/
	MSM_STATS_TYPE_COMP, /* Composite stats */
	MSM_STATS_TYPE_MAX  /* MAX */
};

struct msm_stats_buf_info {
	int type; /* msm_stats_enum_type */
	int fd;
	void *vaddr;
	uint32_t offset;
	uint32_t len;
	uint32_t y_off;
	uint32_t cbcr_off;
	uint32_t planar0_off;
	uint32_t planar1_off;
	uint32_t planar2_off;
	uint8_t active;
	int buf_idx;
};

struct msm_pmem_info {
	int type;
	int fd;
	void *vaddr;
	uint32_t offset;
	uint32_t len;
	uint32_t y_off;
	uint32_t cbcr_off;
	uint32_t planar0_off;
	uint32_t planar1_off;
	uint32_t planar2_off;
	uint8_t active;
};

struct outputCfg {
	uint32_t height;
	uint32_t width;

	uint32_t window_height_firstline;
	uint32_t window_height_lastline;
};

#define VIDEO_NODE 0
#define MCTL_NODE 1

#define OUTPUT_1	0
#define OUTPUT_2	1
#define OUTPUT_1_AND_2            2   /* snapshot only */
#define OUTPUT_1_AND_3            3   /* video */
#define CAMIF_TO_AXI_VIA_OUTPUT_2 4
#define OUTPUT_1_AND_CAMIF_TO_AXI_VIA_OUTPUT_2 5
#define OUTPUT_2_AND_CAMIF_TO_AXI_VIA_OUTPUT_1 6
#define OUTPUT_1_2_AND_3 7
#define OUTPUT_ALL_CHNLS 8
#define OUTPUT_VIDEO_ALL_CHNLS 9
#define OUTPUT_ZSL_ALL_CHNLS 10
#define LAST_AXI_OUTPUT_MODE_ENUM OUTPUT_ZSL_ALL_CHNLS

#define OUTPUT_PRIM              BIT(8)
#define OUTPUT_PRIM_ALL_CHNLS    BIT(9)
#define OUTPUT_SEC               BIT(10)
#define OUTPUT_SEC_ALL_CHNLS     BIT(11)
#define OUTPUT_TERT1             BIT(12)
#define OUTPUT_TERT2             BIT(13)



#define MSM_FRAME_PREV_1	0
#define MSM_FRAME_PREV_2	1
#define MSM_FRAME_ENC		2

#define OUTPUT_TYPE_P    BIT(0)
#define OUTPUT_TYPE_T    BIT(1)
#define OUTPUT_TYPE_S    BIT(2)
#define OUTPUT_TYPE_V    BIT(3)
#define OUTPUT_TYPE_L    BIT(4)
#define OUTPUT_TYPE_ST_L BIT(5)
#define OUTPUT_TYPE_ST_R BIT(6)
#define OUTPUT_TYPE_ST_D BIT(7)
#define OUTPUT_TYPE_R    BIT(8)
#define OUTPUT_TYPE_R1   BIT(9)
#define OUTPUT_TYPE_SAEC   BIT(10)
#define OUTPUT_TYPE_SAFC   BIT(11)
#define OUTPUT_TYPE_SAWB   BIT(12)
#define OUTPUT_TYPE_IHST   BIT(13)
#define OUTPUT_TYPE_CSTA   BIT(14)

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
	uint32_t planar0_off;
	uint32_t planar1_off;
	uint32_t planar2_off;
	int fd;

	void *cropinfo;
	int croplen;
	uint32_t error_code;
	struct fd_roi_info roi_info;
	uint32_t frame_id;
	int stcam_quality_ind;
	uint32_t stcam_conv_value;
	int ion_dev_fd;
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
	uint32_t buf_p0_off;
	uint32_t buf_p1_off;
	uint32_t buf_p0_stride;
	uint32_t buf_p1_stride;
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
	struct stats_buff be;
	struct stats_buff ihist;
	struct stats_buff rs;
	struct stats_buff cs;
	struct stats_buff skin;
	int type;
	uint32_t status_bits;
	unsigned long buffer;
	int fd;
	int length;
	struct ion_handle *handle;
	uint32_t frame_id;
	int buf_idx;
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
/* ISP_PIX_OUTPUT1: no pp, directly send output1 buf to user */
#define MSM_V4L2_EXT_CAPTURE_MODE_ISP_PIX_OUTPUT1 \
	(MSM_V4L2_EXT_CAPTURE_MODE_DEFAULT+5)
/* ISP_PIX_OUTPUT2: no pp, directly send output2 buf to user */
#define MSM_V4L2_EXT_CAPTURE_MODE_ISP_PIX_OUTPUT2 \
	(MSM_V4L2_EXT_CAPTURE_MODE_DEFAULT+6)
/* raw image type */
#define MSM_V4L2_EXT_CAPTURE_MODE_RAW \
	(MSM_V4L2_EXT_CAPTURE_MODE_DEFAULT+7)
/* RDI dump */
#define MSM_V4L2_EXT_CAPTURE_MODE_RDI \
	(MSM_V4L2_EXT_CAPTURE_MODE_DEFAULT+8)
/* RDI dump 1 */
#define MSM_V4L2_EXT_CAPTURE_MODE_RDI1 \
	(MSM_V4L2_EXT_CAPTURE_MODE_DEFAULT+9)
/* RDI dump 2 */
#define MSM_V4L2_EXT_CAPTURE_MODE_RDI2 \
	(MSM_V4L2_EXT_CAPTURE_MODE_DEFAULT+10)
#define MSM_V4L2_EXT_CAPTURE_MODE_AEC \
	(MSM_V4L2_EXT_CAPTURE_MODE_DEFAULT+11)
#define MSM_V4L2_EXT_CAPTURE_MODE_AWB \
	(MSM_V4L2_EXT_CAPTURE_MODE_DEFAULT+12)
#define MSM_V4L2_EXT_CAPTURE_MODE_AF \
	(MSM_V4L2_EXT_CAPTURE_MODE_DEFAULT+13)
#define MSM_V4L2_EXT_CAPTURE_MODE_IHIST \
	(MSM_V4L2_EXT_CAPTURE_MODE_DEFAULT+14)
#define MSM_V4L2_EXT_CAPTURE_MODE_CS \
	(MSM_V4L2_EXT_CAPTURE_MODE_DEFAULT+15)
#define MSM_V4L2_EXT_CAPTURE_MODE_RS \
	(MSM_V4L2_EXT_CAPTURE_MODE_DEFAULT+16)
#define MSM_V4L2_EXT_CAPTURE_MODE_CSTA \
	(MSM_V4L2_EXT_CAPTURE_MODE_DEFAULT+17)
#define MSM_V4L2_EXT_CAPTURE_MODE_V2X_LIVESHOT \
	(MSM_V4L2_EXT_CAPTURE_MODE_DEFAULT+18)
#define MSM_V4L2_EXT_CAPTURE_MODE_MAX (MSM_V4L2_EXT_CAPTURE_MODE_DEFAULT+19)


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
#define MSM_V4L2_PID_INST_HANDLE            (V4L2_CID_PRIVATE_BASE+16)
#define MSM_V4L2_PID_MMAP_INST              (V4L2_CID_PRIVATE_BASE+17)
#define MSM_V4L2_PID_PP_PLANE_INFO          (V4L2_CID_PRIVATE_BASE+18)
#define MSM_V4L2_PID_MAX                    MSM_V4L2_PID_PP_PLANE_INFO

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
/* camera operation mode for jpeg snapshot - one frame output queue */
#define MSM_V4L2_CAM_OP_JPEG_CAPTURE    (MSM_V4L2_CAM_OP_DEFAULT+6)


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
#define MSM_V4L2_PRIVATE_CMD    14
#define MSM_V4L2_MAX			15
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
#define CFG_GET_EEPROM_INFO		33
#define CFG_GET_EEPROM_DATA		34
#define CFG_SET_ACTUATOR_INFO		35
#define CFG_GET_ACTUATOR_INFO           36
/* TBD: QRD */
#define CFG_SET_SATURATION            37
#define CFG_SET_SHARPNESS             38
#define CFG_SET_TOUCHAEC              39
#define CFG_SET_AUTO_FOCUS            40
#define CFG_SET_AUTOFLASH             41
#define CFG_SET_EXPOSURE_COMPENSATION 42
#define CFG_SET_ISO                   43
#define CFG_START_STREAM              44
#define CFG_STOP_STREAM               45
#define CFG_GET_CSI_PARAMS            46
#define CFG_POWER_UP                  47
#define CFG_POWER_DOWN                48
#define CFG_WRITE_I2C_ARRAY           49
#define CFG_READ_I2C_ARRAY            50
#define CFG_PCLK_CHANGE               51
#define CFG_CONFIG_VREG_ARRAY         52
#define CFG_CONFIG_CLK_ARRAY          53
#define CFG_GPIO_OP                   54
#define CFG_MAX                       55


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
#define CAMERA_EFFECT_FADED		12
#define CAMERA_EFFECT_VINTAGECOOL	13
#define CAMERA_EFFECT_VINTAGEWARM	14
#define CAMERA_EFFECT_ACCENT_BLUE       15
#define CAMERA_EFFECT_ACCENT_GREEN      16
#define CAMERA_EFFECT_ACCENT_ORANGE     17
#define CAMERA_EFFECT_MAX               18

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

enum msm_v4l2_saturation_level {
	MSM_V4L2_SATURATION_L0,
	MSM_V4L2_SATURATION_L1,
	MSM_V4L2_SATURATION_L2,
	MSM_V4L2_SATURATION_L3,
	MSM_V4L2_SATURATION_L4,
	MSM_V4L2_SATURATION_L5,
	MSM_V4L2_SATURATION_L6,
	MSM_V4L2_SATURATION_L7,
	MSM_V4L2_SATURATION_L8,
	MSM_V4L2_SATURATION_L9,
	MSM_V4L2_SATURATION_L10,
};

enum msm_v4l2_contrast_level {
	MSM_V4L2_CONTRAST_L0,
	MSM_V4L2_CONTRAST_L1,
	MSM_V4L2_CONTRAST_L2,
	MSM_V4L2_CONTRAST_L3,
	MSM_V4L2_CONTRAST_L4,
	MSM_V4L2_CONTRAST_L5,
	MSM_V4L2_CONTRAST_L6,
	MSM_V4L2_CONTRAST_L7,
	MSM_V4L2_CONTRAST_L8,
	MSM_V4L2_CONTRAST_L9,
	MSM_V4L2_CONTRAST_L10,
};


enum msm_v4l2_exposure_level {
	MSM_V4L2_EXPOSURE_N2,
	MSM_V4L2_EXPOSURE_N1,
	MSM_V4L2_EXPOSURE_D,
	MSM_V4L2_EXPOSURE_P1,
	MSM_V4L2_EXPOSURE_P2,
};

enum msm_v4l2_sharpness_level {
	MSM_V4L2_SHARPNESS_L0,
	MSM_V4L2_SHARPNESS_L1,
	MSM_V4L2_SHARPNESS_L2,
	MSM_V4L2_SHARPNESS_L3,
	MSM_V4L2_SHARPNESS_L4,
	MSM_V4L2_SHARPNESS_L5,
	MSM_V4L2_SHARPNESS_L6,
};

enum msm_v4l2_expo_metering_mode {
	MSM_V4L2_EXP_FRAME_AVERAGE,
	MSM_V4L2_EXP_CENTER_WEIGHTED,
	MSM_V4L2_EXP_SPOT_METERING,
};

enum msm_v4l2_iso_mode {
	MSM_V4L2_ISO_AUTO = 0,
	MSM_V4L2_ISO_DEBLUR,
	MSM_V4L2_ISO_100,
	MSM_V4L2_ISO_200,
	MSM_V4L2_ISO_400,
	MSM_V4L2_ISO_800,
	MSM_V4L2_ISO_1600,
};

enum msm_v4l2_wb_mode {
	MSM_V4L2_WB_OFF,
	MSM_V4L2_WB_AUTO,
	MSM_V4L2_WB_CUSTOM,
	MSM_V4L2_WB_INCANDESCENT,
	MSM_V4L2_WB_FLUORESCENT,
	MSM_V4L2_WB_DAYLIGHT,
	MSM_V4L2_WB_CLOUDY_DAYLIGHT,
};

enum msm_v4l2_special_effect {
	MSM_V4L2_EFFECT_OFF,
	MSM_V4L2_EFFECT_MONO,
	MSM_V4L2_EFFECT_NEGATIVE,
	MSM_V4L2_EFFECT_SOLARIZE,
	MSM_V4L2_EFFECT_SEPIA,
	MSM_V4L2_EFFECT_POSTERAIZE,
	MSM_V4L2_EFFECT_WHITEBOARD,
	MSM_V4L2_EFFECT_BLACKBOARD,
	MSM_V4L2_EFFECT_AQUA,
	MSM_V4L2_EFFECT_EMBOSS,
	MSM_V4L2_EFFECT_SKETCH,
	MSM_V4L2_EFFECT_NEON,
	MSM_V4L2_EFFECT_MAX,
};

enum msm_v4l2_power_line_frequency {
	MSM_V4L2_POWER_LINE_OFF,
	MSM_V4L2_POWER_LINE_60HZ,
	MSM_V4L2_POWER_LINE_50HZ,
	MSM_V4L2_POWER_LINE_AUTO,
};

#define CAMERA_ISO_TYPE_AUTO           0
#define CAMEAR_ISO_TYPE_HJR            1
#define CAMEAR_ISO_TYPE_100            2
#define CAMERA_ISO_TYPE_200            3
#define CAMERA_ISO_TYPE_400            4
#define CAMEAR_ISO_TYPE_800            5
#define CAMERA_ISO_TYPE_1600           6

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
struct sensor_3d_cali_data_t {
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

struct msm_sensor_exp_gain_info_t {
	uint16_t coarse_int_time_addr;
	uint16_t global_gain_addr;
	uint16_t vert_offset;
};

struct msm_sensor_output_reg_addr_t {
	uint16_t x_output;
	uint16_t y_output;
	uint16_t line_length_pclk;
	uint16_t frame_length_lines;
};

struct sensor_driver_params_type {
	struct msm_camera_i2c_reg_setting *init_settings;
	uint16_t init_settings_size;
	struct msm_camera_i2c_reg_setting *mode_settings;
	uint16_t mode_settings_size;
	struct msm_sensor_output_reg_addr_t *sensor_output_reg_addr;
	struct msm_camera_i2c_reg_setting *start_settings;
	struct msm_camera_i2c_reg_setting *stop_settings;
	struct msm_camera_i2c_reg_setting *groupon_settings;
	struct msm_camera_i2c_reg_setting *groupoff_settings;
	struct msm_sensor_exp_gain_info_t *sensor_exp_gain_info;
	struct msm_sensor_output_info_t *output_info;
};

struct mirror_flip {
	int32_t x_mirror;
	int32_t y_flip;
};

struct cord {
	uint32_t x;
	uint32_t y;
};

struct msm_eeprom_data_t {
	void *eeprom_data;
	uint16_t index;
};

struct msm_camera_csid_vc_cfg {
	uint8_t cid;
	uint8_t dt;
	uint8_t decode_format;
};

struct csi_lane_params_t {
	uint16_t csi_lane_assign;
	uint8_t csi_lane_mask;
	uint8_t csi_if;
	uint8_t csid_core[2];
	uint8_t csi_phy_sel;
};

struct msm_camera_csid_lut_params {
	uint8_t num_cid;
	struct msm_camera_csid_vc_cfg *vc_cfg;
};

struct msm_camera_csid_params {
	uint8_t lane_cnt;
	uint16_t lane_assign;
	uint8_t phy_sel;
	uint32_t topology;
	struct msm_camera_csid_lut_params lut_params;
};

struct msm_camera_csiphy_params {
	uint8_t lane_cnt;
	uint8_t settle_cnt;
	uint16_t lane_mask;
	uint8_t combo_mode;
	uint8_t csid_core;
	uint64_t data_rate;
};

struct msm_camera_csi2_params {
	struct msm_camera_csid_params csid_params;
	struct msm_camera_csiphy_params csiphy_params;
};

enum msm_camera_csi_data_format {
	CSI_8BIT,
	CSI_10BIT,
	CSI_12BIT,
};

struct msm_camera_csi_params {
	enum msm_camera_csi_data_format data_format;
	uint8_t lane_cnt;
	uint8_t lane_assign;
	uint8_t settle_cnt;
	uint8_t dpcm_scheme;
};

enum csic_cfg_type_t {
	CSIC_INIT,
	CSIC_CFG,
};

struct csic_cfg_data {
	enum csic_cfg_type_t cfgtype;
	struct msm_camera_csi_params *csic_params;
};

enum csid_cfg_type_t {
	CSID_INIT,
	CSID_CFG,
	CSID_SECCAM_TOPOLOGY,
	CSID_SECCAM_RESET,
};

struct csid_cfg_data {
	enum csid_cfg_type_t cfgtype;
	union {
		uint32_t csid_version;
		struct msm_camera_csid_params *csid_params;
	} cfg;
};

enum csiphy_cfg_type_t {
	CSIPHY_INIT,
	CSIPHY_CFG,
};

struct csiphy_cfg_data {
	enum csiphy_cfg_type_t cfgtype;
	struct msm_camera_csiphy_params *csiphy_params;
};

#define CSI_EMBED_DATA 0x12
#define CSI_RESERVED_DATA_0 0x13
#define CSI_YUV422_8  0x1E
#define CSI_RAW8    0x2A
#define CSI_RAW10   0x2B
#define CSI_RAW12   0x2C

#define CSI_DECODE_6BIT 0
#define CSI_DECODE_8BIT 1
#define CSI_DECODE_10BIT 2
#define CSI_DECODE_DPCM_10_8_10 5

#define ISPIF_STREAM(intf, action, vfe) (((intf)<<ISPIF_S_STREAM_SHIFT)+\
	(action)+((vfe)<<ISPIF_VFE_INTF_SHIFT))
#define ISPIF_ON_FRAME_BOUNDARY   (0x01 << 0)
#define ISPIF_OFF_FRAME_BOUNDARY  (0x01 << 1)
#define ISPIF_OFF_IMMEDIATELY     (0x01 << 2)
#define ISPIF_S_STREAM_SHIFT      4
#define ISPIF_VFE_INTF_SHIFT      12

#define PIX_0 (0x01 << 0)
#define RDI_0 (0x01 << 1)
#define PIX_1 (0x01 << 2)
#define RDI_1 (0x01 << 3)
#define RDI_2 (0x01 << 4)

enum msm_ispif_vfe_intf {
	VFE0,
	VFE1,
	VFE_MAX,
};

enum msm_ispif_intftype {
	PIX0,
	RDI0,
	PIX1,
	RDI1,
	RDI2,
	INTF_MAX,
};

enum msm_ispif_vc {
	VC0,
	VC1,
	VC2,
	VC3,
};

enum msm_ispif_cid {
	CID0,
	CID1,
	CID2,
	CID3,
	CID4,
	CID5,
	CID6,
	CID7,
	CID8,
	CID9,
	CID10,
	CID11,
	CID12,
	CID13,
	CID14,
	CID15,
};

struct msm_ispif_params {
	uint8_t intftype;
	uint16_t cid_mask;
	uint8_t csid;
	uint8_t vfe_intf;
};

struct msm_ispif_params_list {
	uint32_t len;
	struct msm_ispif_params params[4];
};

enum ispif_cfg_type_t {
	ISPIF_INIT,
	ISPIF_SET_CFG,
	ISPIF_SET_ON_FRAME_BOUNDARY,
	ISPIF_SET_OFF_FRAME_BOUNDARY,
	ISPIF_SET_OFF_IMMEDIATELY,
	ISPIF_RELEASE,
};

struct ispif_cfg_data {
	enum ispif_cfg_type_t cfgtype;
	union {
		uint32_t csid_version;
		int cmd;
		struct msm_ispif_params_list ispif_params;
	} cfg;
};

enum msm_camera_i2c_reg_addr_type {
	MSM_CAMERA_I2C_BYTE_ADDR = 1,
	MSM_CAMERA_I2C_WORD_ADDR,
	MSM_CAMERA_I2C_3B_ADDR,
	MSM_CAMERA_I2C_DWORD_ADDR,
};
#define MSM_CAMERA_I2C_DWORD_ADDR MSM_CAMERA_I2C_DWORD_ADDR

struct msm_camera_i2c_reg_array {
	uint16_t reg_addr;
	uint16_t reg_data;
};

enum msm_camera_i2c_data_type {
	MSM_CAMERA_I2C_BYTE_DATA = 1,
	MSM_CAMERA_I2C_WORD_DATA,
	MSM_CAMERA_I2C_SET_BYTE_MASK,
	MSM_CAMERA_I2C_UNSET_BYTE_MASK,
	MSM_CAMERA_I2C_SET_WORD_MASK,
	MSM_CAMERA_I2C_UNSET_WORD_MASK,
	MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA,
};

struct msm_camera_i2c_reg_setting {
	struct msm_camera_i2c_reg_array *reg_setting;
	uint16_t size;
	enum msm_camera_i2c_reg_addr_type addr_type;
	enum msm_camera_i2c_data_type data_type;
	uint16_t delay;
};

enum oem_setting_type {
	I2C_READ = 1,
	I2C_WRITE,
	GPIO_OP,
	EEPROM_READ,
	VREG_SET,
	CLK_SET,
};

struct sensor_oem_setting {
	enum oem_setting_type type;
	void *data;
};

enum camera_vreg_type {
	REG_LDO,
	REG_VS,
	REG_GPIO,
};

enum msm_camera_vreg_name_t {
	CAM_VDIG,
	CAM_VIO,
	CAM_VANA,
	CAM_VAF,
	CAM_VREG_MAX,
};

struct msm_camera_csi_lane_params {
	uint16_t csi_lane_assign;
	uint16_t csi_lane_mask;
};

struct camera_vreg_t {
	const char *reg_name;
	int min_voltage;
	int max_voltage;
	int op_mode;
	uint32_t delay;
};

struct msm_camera_vreg_setting {
	struct camera_vreg_t *cam_vreg;
	uint16_t num_vreg;
	uint8_t enable;
};

struct msm_cam_clk_info {
	const char *clk_name;
	long clk_rate;
	uint32_t delay;
};

struct msm_cam_clk_setting {
	struct msm_cam_clk_info *clk_info;
	uint16_t num_clk_info;
	uint8_t enable;
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
		uint8_t iso_type;
		struct sensor_init_cfg init_info;
		struct sensor_pict_fps gfps;
		struct exp_gain_cfg exp_gain;
		struct focus_cfg focus;
		struct fps_cfg fps;
		struct wb_info_cfg wb_info;
		struct sensor_3d_exp_cfg sensor_3d_exp;
		struct sensor_calib_data calib_info;
		struct sensor_output_info_t output_info;
		struct msm_eeprom_data_t eeprom_data;
		struct csi_lane_params_t csi_lane_params;
		/* QRD */
		uint16_t antibanding;
		uint8_t contrast;
		uint8_t saturation;
		uint8_t sharpness;
		int8_t brightness;
		int ae_mode;
		uint8_t wb_val;
		int8_t exp_compensation;
		uint32_t pclk;
		struct cord aec_cord;
		int is_autoflash;
		struct mirror_flip mirror_flip;
		void *setting;
	} cfg;
};

enum gpio_operation_type {
	GPIO_REQUEST,
	GPIO_FREE,
	GPIO_SET_DIRECTION_OUTPUT,
	GPIO_SET_DIRECTION_INPUT,
	GPIO_GET_VALUE,
	GPIO_SET_VALUE,
};

struct msm_cam_gpio_operation {
	enum gpio_operation_type op_type;
	unsigned int address;
	int value;
	const char *tag;
};

struct damping_params_t {
	uint32_t damping_step;
	uint32_t damping_delay;
	uint32_t hw_params;
};

enum actuator_type {
	ACTUATOR_VCM,
	ACTUATOR_PIEZO,
	ACTUATOR_HVCM,
	ACTUATOR_BIVCM,
};

enum msm_actuator_data_type {
	MSM_ACTUATOR_BYTE_DATA = 1,
	MSM_ACTUATOR_WORD_DATA,
};

enum msm_actuator_addr_type {
	MSM_ACTUATOR_BYTE_ADDR = 1,
	MSM_ACTUATOR_WORD_ADDR,
};

enum msm_actuator_write_type {
	MSM_ACTUATOR_WRITE_HW_DAMP,
	MSM_ACTUATOR_WRITE_DAC,
	MSM_ACTUATOR_WRITE,
	MSM_ACTUATOR_WRITE_DIR_REG,
	MSM_ACTUATOR_POLL,
	MSM_ACTUATOR_READ_WRITE,
};

struct msm_actuator_reg_params_t {
	enum msm_actuator_write_type reg_write_type;
	uint32_t hw_mask;
	uint16_t reg_addr;
	uint16_t hw_shift;
	uint16_t data_type;
	uint16_t addr_type;
	uint16_t reg_data;
	uint16_t delay;
};

struct reg_settings_t {
	uint16_t reg_addr;
	uint16_t reg_data;
};

struct region_params_t {
	/* [0] = ForwardDirection Macro boundary
	 *  [1] = ReverseDirection Inf boundary
	 */
	uint16_t step_bound[2];
	uint16_t code_per_step;
};

struct msm_actuator_move_params_t {
	int8_t dir;
	int8_t sign_dir;
	int16_t dest_step_pos;
	int32_t num_steps;
	struct damping_params_t __user *ringing_params;
};

struct msm_actuator_tuning_params_t {
	int16_t initial_code;
	uint16_t pwd_step;
	uint16_t region_size;
	uint32_t total_steps;
	struct region_params_t __user *region_params;
};

struct msm_actuator_params_t {
	enum actuator_type act_type;
	uint8_t reg_tbl_size;
	uint16_t data_size;
	uint16_t init_setting_size;
	uint32_t i2c_addr;
	enum msm_actuator_addr_type i2c_addr_type;
	enum msm_actuator_data_type i2c_data_type;
	struct msm_actuator_reg_params_t __user *reg_tbl_params;
	struct reg_settings_t __user *init_settings;
};

struct msm_actuator_set_info_t {
	struct msm_actuator_params_t actuator_params;
	struct msm_actuator_tuning_params_t af_tuning_params;
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
	uint32_t hor_view_angle_num;
	uint32_t hor_view_angle_den;
	uint32_t ver_view_angle_num;
	uint32_t ver_view_angle_den;
};

enum af_camera_name {
	ACTUATOR_MAIN_CAM_0,
	ACTUATOR_MAIN_CAM_1,
	ACTUATOR_MAIN_CAM_2,
	ACTUATOR_MAIN_CAM_3,
	ACTUATOR_MAIN_CAM_4,
	ACTUATOR_MAIN_CAM_5,
	ACTUATOR_WEB_CAM_0,
	ACTUATOR_WEB_CAM_1,
	ACTUATOR_WEB_CAM_2,
};

struct msm_actuator_cfg_data {
	int cfgtype;
	uint8_t is_af_supported;
	union {
		struct msm_actuator_move_params_t move;
		struct msm_actuator_set_info_t set_info;
		struct msm_actuator_get_info_t get_info;
		enum af_camera_name cam_name;
	} cfg;
};

struct msm_eeprom_support {
	uint16_t is_supported;
	uint16_t size;
	uint16_t index;
	uint16_t qvalue;
};

struct msm_calib_wb {
	uint16_t r_over_g;
	uint16_t b_over_g;
	uint16_t gr_over_gb;
};

struct msm_calib_af {
	uint16_t macro_dac;
	uint16_t inf_dac;
	uint16_t start_dac;
};

struct msm_calib_lsc {
	uint16_t r_gain[221];
	uint16_t b_gain[221];
	uint16_t gr_gain[221];
	uint16_t gb_gain[221];
};

struct pixel_t {
	int x;
	int y;
};

struct msm_calib_dpc {
	uint16_t validcount;
	struct pixel_t snapshot_coord[128];
	struct pixel_t preview_coord[128];
	struct pixel_t video_coord[128];
};

struct msm_calib_raw {
	uint8_t *data;
	uint32_t size;
};

struct msm_camera_eeprom_info_t {
	struct msm_eeprom_support af;
	struct msm_eeprom_support wb;
	struct msm_eeprom_support lsc;
	struct msm_eeprom_support dpc;
	struct msm_eeprom_support raw;
};

struct msm_eeprom_cfg_data {
	int cfgtype;
	uint8_t is_eeprom_supported;
	union {
		struct msm_eeprom_data_t get_data;
		struct msm_camera_eeprom_info_t get_info;
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

struct msm_mctl_node_info {
	int num_mctl_nodes;
	const char *mctl_node_name[MSM_MAX_CAMERA_SENSORS];
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
	uint8_t strobe_flash_enabled;
	uint8_t actuator_enabled;
	uint8_t ispif_supported;
	int8_t total_steps;
	uint8_t support_3d;
	enum flash_type flashtype;
	enum sensor_type_t sensor_type;
	uint32_t pxlcode; /* enum v4l2_mbus_pixelcode */
	uint32_t camera_type; /* msm_camera_type */
	int mount_angle;
	uint32_t max_width;
	uint32_t max_height;
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
	uint32_t inst_handle;
};

#define QCAMERA_NAME "qcamera"
#define QCAMERA_SERVER_NAME "qcamera_server"
#define QCAMERA_VNODE_GROUP_ID MEDIA_ENT_F_IO_V4L

enum msm_cam_subdev_type {
	CSIPHY_DEV,
	CSID_DEV,
	CSIC_DEV,
	ISPIF_DEV,
	VFE_DEV,
	AXI_DEV,
	VPE_DEV,
	SENSOR_DEV,
	ACTUATOR_DEV,
	EEPROM_DEV,
	GESTURE_DEV,
	IRQ_ROUTER_DEV,
	CPP_DEV,
	CCI_DEV,
	FLASH_DEV,
};

struct msm_mctl_set_sdev_data {
	uint32_t revision;
	enum msm_cam_subdev_type sdev_type;
};

#define MSM_CAM_V4L2_IOCTL_GET_CAMERA_INFO \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 1, struct msm_camera_v4l2_ioctl_t)

#define MSM_CAM_V4L2_IOCTL_GET_CONFIG_INFO \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 2, struct msm_camera_v4l2_ioctl_t)

#define MSM_CAM_V4L2_IOCTL_GET_MCTL_INFO \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 3, struct msm_camera_v4l2_ioctl_t)

#define MSM_CAM_V4L2_IOCTL_CTRL_CMD_DONE \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 4, struct msm_camera_v4l2_ioctl_t)

#define MSM_CAM_V4L2_IOCTL_GET_EVENT_PAYLOAD \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 5, struct msm_camera_v4l2_ioctl_t)

#define MSM_CAM_IOCTL_SEND_EVENT \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 6, struct v4l2_event)

#define MSM_CAM_V4L2_IOCTL_CFG_VPE \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 7, struct msm_vpe_cfg_cmd)

#define MSM_CAM_V4L2_IOCTL_PRIVATE_S_CTRL \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 8, struct msm_camera_v4l2_ioctl_t)

#define MSM_CAM_V4L2_IOCTL_PRIVATE_G_CTRL \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 9, struct msm_camera_v4l2_ioctl_t)

#define MSM_CAM_V4L2_IOCTL_PRIVATE_GENERAL \
	_IOW('V', BASE_VIDIOC_PRIVATE + 10, struct msm_camera_v4l2_ioctl_t)

#define VIDIOC_MSM_VPE_INIT \
	_IO('V', BASE_VIDIOC_PRIVATE + 15)

#define VIDIOC_MSM_VPE_RELEASE \
	_IO('V', BASE_VIDIOC_PRIVATE + 16)

#define VIDIOC_MSM_VPE_CFG \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 17, struct msm_mctl_pp_params *)

#define VIDIOC_MSM_AXI_INIT \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 18, uint8_t *)

#define VIDIOC_MSM_AXI_RELEASE \
	_IO('V', BASE_VIDIOC_PRIVATE + 19)

#define VIDIOC_MSM_AXI_CFG \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 20, void *)

#define VIDIOC_MSM_AXI_IRQ \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 21, void *)

#define VIDIOC_MSM_AXI_BUF_CFG \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 22, void *)

#define VIDIOC_MSM_AXI_RDI_COUNT_UPDATE \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 23, void *)

#define VIDIOC_MSM_VFE_INIT \
	_IO('V', BASE_VIDIOC_PRIVATE + 24)

#define VIDIOC_MSM_VFE_RELEASE \
	_IO('V', BASE_VIDIOC_PRIVATE + 25)

struct msm_camera_v4l2_ioctl_t {
	uint32_t id;
	uint32_t len;
	uint32_t trans_code;
	void __user *ioctl_ptr;
};

struct msm_camera_vfe_params_t {
	uint32_t operation_mode;
	uint32_t capture_count;
	uint8_t  skip_reset;
	uint8_t  stop_immediately;
	uint16_t port_info;
	uint32_t inst_handle;
	uint16_t cmd_type;
};

enum msm_camss_irq_idx {
	CAMERA_SS_IRQ_0,
	CAMERA_SS_IRQ_1,
	CAMERA_SS_IRQ_2,
	CAMERA_SS_IRQ_3,
	CAMERA_SS_IRQ_4,
	CAMERA_SS_IRQ_5,
	CAMERA_SS_IRQ_6,
	CAMERA_SS_IRQ_7,
	CAMERA_SS_IRQ_8,
	CAMERA_SS_IRQ_9,
	CAMERA_SS_IRQ_10,
	CAMERA_SS_IRQ_11,
	CAMERA_SS_IRQ_12,
	CAMERA_SS_IRQ_MAX
};

enum msm_cam_hw_idx {
	MSM_CAM_HW_MICRO,
	MSM_CAM_HW_CCI,
	MSM_CAM_HW_CSI0,
	MSM_CAM_HW_CSI1,
	MSM_CAM_HW_CSI2,
	MSM_CAM_HW_CSI3,
	MSM_CAM_HW_ISPIF,
	MSM_CAM_HW_CPP,
	MSM_CAM_HW_VFE0,
	MSM_CAM_HW_VFE1,
	MSM_CAM_HW_JPEG0,
	MSM_CAM_HW_JPEG1,
	MSM_CAM_HW_JPEG2,
	MSM_CAM_HW_MAX
};

struct msm_camera_irq_cfg {
	/* Bit mask of all the camera hardwares that needs to
	 * be composited into a single IRQ to the MSM.
	 * Current usage: (may be updated based on hw changes)
	 * Bits 31:13 - Reserved.
	 * Bits 12:0
	 * 12 - MSM_CAM_HW_JPEG2
	 * 11 - MSM_CAM_HW_JPEG1
	 * 10 - MSM_CAM_HW_JPEG0
	 *  9 - MSM_CAM_HW_VFE1
	 *  8 - MSM_CAM_HW_VFE0
	 *  7 - MSM_CAM_HW_CPP
	 *  6 - MSM_CAM_HW_ISPIF
	 *  5 - MSM_CAM_HW_CSI3
	 *  4 - MSM_CAM_HW_CSI2
	 *  3 - MSM_CAM_HW_CSI1
	 *  2 - MSM_CAM_HW_CSI0
	 *  1 - MSM_CAM_HW_CCI
	 *  0 - MSM_CAM_HW_MICRO
	 */
	uint32_t cam_hw_mask;
	uint8_t  irq_idx;
	uint8_t  num_hwcore;
};

#define MSM_IRQROUTER_CFG_COMPIRQ \
	_IOWR('V', BASE_VIDIOC_PRIVATE, void __user *)

#define MAX_NUM_CPP_STRIPS 8

enum msm_cpp_frame_type {
	MSM_CPP_OFFLINE_FRAME,
	MSM_CPP_REALTIME_FRAME,
};

struct msm_cpp_frame_info_t {
	int32_t frame_id;
	uint32_t inst_id;
	uint32_t client_id;
	enum msm_cpp_frame_type frame_type;
	uint32_t num_strips;
};

struct msm_ver_num_info {
	uint32_t main;
	uint32_t minor;
	uint32_t rev;
};

#define VIDIOC_MSM_CPP_CFG \
	_IOWR('V', BASE_VIDIOC_PRIVATE, struct msm_camera_v4l2_ioctl_t)

#define VIDIOC_MSM_CPP_GET_EVENTPAYLOAD \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 1, struct msm_camera_v4l2_ioctl_t)

#define VIDIOC_MSM_CPP_GET_INST_INFO \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 2, struct msm_camera_v4l2_ioctl_t)

#define V4L2_EVENT_CPP_FRAME_DONE  (V4L2_EVENT_PRIVATE_START + 0)

/* Instance Handle - inst_handle
 * Data bundle containing the information about where
 * to get a buffer for a particular camera instance.
 * This is a bitmask containing the following data:
 * Buffer Handle Bitmask:
 *      ------------------------------------
 *      Bits    :  Purpose
 *      ------------------------------------
 *      31      :  is Dev ID valid?
 *      30 - 24 :  Dev ID.
 *      23      :  is Image mode valid?
 *      22 - 16 :  Image mode.
 *      15      :  is MCTL PP inst idx valid?
 *      14 - 8  :  MCTL PP inst idx.
 *      7       :  is Video inst idx valid?
 *      6 - 0   :  Video inst idx.
 */
#define CLR_DEVID_MODE(handle)	(handle &= 0x00FFFFFF)
#define SET_DEVID_MODE(handle, data)	\
	(handle |= ((0x1 << 31) | ((data & 0x7F) << 24)))
#define GET_DEVID_MODE(handle)	\
	((handle & 0x80000000) ? ((handle & 0x7F000000) >> 24) : 0xFF)

#define CLR_IMG_MODE(handle)	(handle &= 0xFF00FFFF)
#define SET_IMG_MODE(handle, data)	\
	(handle |= ((0x1 << 23) | ((data & 0x7F) << 16)))
#define GET_IMG_MODE(handle)	\
	((handle & 0x800000) ? ((handle & 0x7F0000) >> 16) : 0xFF)

#define CLR_MCTLPP_INST_IDX(handle)	(handle &= 0xFFFF00FF)
#define SET_MCTLPP_INST_IDX(handle, data)	\
	(handle |= ((0x1 << 15) | ((data & 0x7F) << 8)))
#define GET_MCTLPP_INST_IDX(handle)	\
	((handle & 0x8000) ? ((handle & 0x7F00) >> 8) : 0xFF)

#define CLR_VIDEO_INST_IDX(handle)	(handle &= 0xFFFFFF00)
#define GET_VIDEO_INST_IDX(handle)	\
	((handle & 0x80) ? (handle & 0x7F) : 0xFF)
#define SET_VIDEO_INST_IDX(handle, data)	\
	(handle |= (0x1 << 7) | (data & 0x7F))
#endif
