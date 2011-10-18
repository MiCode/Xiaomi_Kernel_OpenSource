/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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

#ifndef _MSM_H
#define _MSM_H

#ifdef __KERNEL__

/* Header files */
#include <linux/i2c.h>
#include <linux/videodev2.h>
#include <linux/pm_qos_params.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-event.h>
#include <media/v4l2-mediabus.h>
#include <media/videobuf2-dma-contig.h>
#include <media/videobuf2-msm-mem.h>
#include <media/msm_isp.h>
#include <mach/camera.h>
#include <media/msm_isp.h>
#include <linux/ion.h>

#define MSM_V4L2_DIMENSION_SIZE 96
#define MAX_DEV_NAME_LEN 50

#define ERR_USER_COPY(to) pr_debug("%s(%d): copy %s user\n", \
				__func__, __LINE__, ((to) ? "to" : "from"))
#define ERR_COPY_FROM_USER() ERR_USER_COPY(0)
#define ERR_COPY_TO_USER() ERR_USER_COPY(1)

#define MSM_CSIPHY_DRV_NAME "msm_csiphy"
#define MSM_CSID_DRV_NAME "msm_csid"
#define MSM_CSIC_DRV_NAME "msm_csic"
#define MSM_ISPIF_DRV_NAME "msm_ispif"
#define MSM_VFE_DRV_NAME "msm_vfe"
#define MSM_VPE_DRV_NAME "msm_vpe"
#define MSM_GEMINI_DRV_NAME "msm_gemini"

/* msm queue management APIs*/

#define msm_dequeue(queue, member) ({	   \
	unsigned long flags;		  \
	struct msm_device_queue *__q = (queue);	 \
	struct msm_queue_cmd *qcmd = 0;	   \
	spin_lock_irqsave(&__q->lock, flags);	 \
	if (!list_empty(&__q->list)) {		\
		__q->len--;		 \
		qcmd = list_first_entry(&__q->list,   \
		struct msm_queue_cmd, member);  \
		list_del_init(&qcmd->member);	 \
	}			 \
	spin_unlock_irqrestore(&__q->lock, flags);  \
	qcmd;			 \
})

#define msm_queue_drain(queue, member) do {	 \
	unsigned long flags;		  \
	struct msm_device_queue *__q = (queue);	 \
	struct msm_queue_cmd *qcmd;	   \
	spin_lock_irqsave(&__q->lock, flags);	 \
	while (!list_empty(&__q->list)) {	 \
		qcmd = list_first_entry(&__q->list,   \
			struct msm_queue_cmd, member);	\
			list_del_init(&qcmd->member);	 \
			free_qcmd(qcmd);		\
	 };			  \
	spin_unlock_irqrestore(&__q->lock, flags);	\
} while (0)

static inline void free_qcmd(struct msm_queue_cmd *qcmd)
{
	if (!qcmd || !atomic_read(&qcmd->on_heap))
		return;
	if (!atomic_sub_return(1, &qcmd->on_heap))
		kfree(qcmd);
}

struct isp_msg_stats {
	uint32_t    id;
	uint32_t    buffer;
	uint32_t    frameCounter;
};

struct msm_free_buf {
	uint8_t num_planes;
	uint32_t ch_paddr[VIDEO_MAX_PLANES];
	uint32_t vb;
};

struct isp_msg_event {
	uint32_t msg_id;
	uint32_t sof_count;
};

struct isp_msg_output {
	uint8_t   output_id;
	struct msm_free_buf buf;
	uint32_t  frameCounter;
};

/* message id for v4l2_subdev_notify*/
enum msm_camera_v4l2_subdev_notify {
	NOTIFY_CID_CHANGE, /* arg = msm_camera_csid_params */
	NOTIFY_ISP_MSG_EVT, /* arg = enum ISP_MESSAGE_ID */
	NOTIFY_VFE_MSG_OUT, /* arg = struct isp_msg_output */
	NOTIFY_VFE_MSG_STATS,  /* arg = struct isp_msg_stats */
	NOTIFY_VFE_MSG_COMP_STATS, /* arg = struct msm_stats_buf */
	NOTIFY_VFE_BUF_EVT, /* arg = struct msm_vfe_resp */
	NOTIFY_ISPIF_STREAM, /* arg = enable parameter for s_stream */
	NOTIFY_VPE_MSG_EVT,
	NOTIFY_PCLK_CHANGE, /* arg = pclk */
	NOTIFY_CSIPHY_CFG, /* arg = msm_camera_csiphy_params */
	NOTIFY_CSID_CFG, /* arg = msm_camera_csid_params */
	NOTIFY_CSIC_CFG, /* arg = msm_camera_csic_params */
	NOTIFY_VFE_BUF_FREE_EVT, /* arg = msm_camera_csic_params */
	NOTIFY_INVALID
};

enum isp_vfe_cmd_id {
	/*
	*Important! Command_ID are arranged in order.
	*Don't change!*/
	ISP_VFE_CMD_ID_STREAM_ON,
	ISP_VFE_CMD_ID_STREAM_OFF,
	ISP_VFE_CMD_ID_FRAME_BUF_RELEASE
};

struct msm_cam_v4l2_device;
struct msm_cam_v4l2_dev_inst;
#define MSM_MAX_IMG_MODE                8

enum msm_buffer_state {
	MSM_BUFFER_STATE_UNUSED,
	MSM_BUFFER_STATE_INITIALIZED,
	MSM_BUFFER_STATE_PREPARED,
	MSM_BUFFER_STATE_QUEUED,
	MSM_BUFFER_STATE_RESERVED,
	MSM_BUFFER_STATE_DEQUEUED
};

/* buffer for one video frame */
struct msm_frame_buffer {
	/* common v4l buffer stuff -- must be first */
	struct vb2_buffer         vidbuf;
	struct list_head		  list;
	enum v4l2_mbus_pixelcode  pxlcode;
	enum msm_buffer_state state;
	int active;
};

struct msm_isp_color_fmt {
	char *name;
	int depth;
	int bitsperpxl;
	u32 fourcc;
	enum v4l2_mbus_pixelcode pxlcode;
	enum v4l2_colorspace colorspace;
};

struct msm_mctl_pp_frame_info {
	int user_cmd;
	struct msm_pp_frame src_frame;
	struct msm_pp_frame dest_frame;
	struct msm_mctl_pp_frame_cmd pp_frame_cmd;
};

struct msm_mctl_pp_ctrl {
	int pp_msg_type;
	struct msm_mctl_pp_frame_info *pp_frame_info;

};
struct msm_mctl_pp_info {
	spinlock_t lock;
	uint32_t cnt;
	uint32_t pp_key;
	uint32_t cur_frame_id[MSM_MAX_IMG_MODE];
	struct msm_free_buf div_frame[MSM_MAX_IMG_MODE];
	struct msm_mctl_pp_ctrl pp_ctrl;

};
/* "Media Controller" represents a camera steaming session,
 * which consists of a "sensor" device and an "isp" device
 * (such as VFE, if needed), connected via an "IO" device,
 * (such as IPIF on 8960, or none on 8660) plus other extra
 * sub devices such as VPE and flash.
 */

struct msm_cam_media_controller {

	int (*mctl_open)(struct msm_cam_media_controller *p_mctl,
					 const char *const apps_id);
	int (*mctl_cb)(void);
	int (*mctl_notify)(struct msm_cam_media_controller *p_mctl,
			unsigned int notification, void *arg);
	int (*mctl_cmd)(struct msm_cam_media_controller *p_mctl,
					unsigned int cmd, unsigned long arg);
	int (*mctl_release)(struct msm_cam_media_controller *p_mctl);
	int (*mctl_buf_init)(struct msm_cam_v4l2_dev_inst *pcam);
	int (*mctl_vbqueue_init)(struct msm_cam_v4l2_dev_inst *pcam,
				struct vb2_queue *q, enum v4l2_buf_type type);
	int (*mctl_ufmt_init)(struct msm_cam_media_controller *p_mctl);

	struct v4l2_fh  eventHandle; /* event queue to export events */
	/* most-frequently accessed manager object*/
	struct msm_sync sync;


	/* the following reflect the HW topology information*/
	/*mandatory*/
	struct v4l2_subdev *sensor_sdev; /* sensor sub device */
	struct v4l2_subdev mctl_sdev;   /*  media control sub device */
	struct platform_device *plat_dev;
	/*optional*/
	struct msm_isp_ops *isp_sdev;    /* isp sub device : camif/VFE */
	struct v4l2_subdev *vpe_sdev;    /* vpe sub device : VPE */
	struct v4l2_subdev *flash_sdev;    /* vpe sub device : VPE */
	struct msm_cam_config_dev *config_device;
	struct v4l2_subdev *csiphy_sdev; /*csiphy sub device*/
	struct v4l2_subdev *csid_sdev; /*csid sub device*/
	struct v4l2_subdev *csic_sdev; /*csid sub device*/
	struct v4l2_subdev *ispif_sdev; /* ispif sub device */
	struct v4l2_subdev *act_sdev; /* actuator sub device */
	struct v4l2_subdev *gemini_sdev; /* gemini sub device */

	struct pm_qos_request_list pm_qos_req_list;
	struct msm_mctl_pp_info pp_info;
	struct ion_client *client;
	/* VFE output mode.
	* Used to interpret the Primary/Secondary messages
	* to preview/video/main/thumbnail image types*/
	uint32_t vfe_output_mode;
};

/* abstract camera device represents a VFE and connected sensor */
struct msm_isp_ops {
	char *config_dev_name;

	/*int (*isp_init)(struct msm_cam_v4l2_device *pcam);*/
	int (*isp_open)(struct v4l2_subdev *sd, struct v4l2_subdev *sd_vpe,
		struct v4l2_subdev *gemini_sdev, struct msm_sync *sync);
	int (*isp_config)(struct msm_cam_media_controller *pmctl,
		 unsigned int cmd, unsigned long arg);
	int (*isp_notify)(struct v4l2_subdev *sd,
		unsigned int notification, void *arg);
	void (*isp_release)(struct msm_sync *psync,
		struct v4l2_subdev *gemini_sdev);
	int (*isp_pp_cmd)(struct msm_cam_media_controller *pmctl,
		 struct msm_mctl_pp_cmd, void *data);

	/* vfe subdevice */
	struct v4l2_subdev *sd;
	struct v4l2_subdev *sd_vpe;
};

struct msm_isp_buf_info {
	int type;
	unsigned long buffer;
	int fd;
};
struct msm_cam_buf_offset {
	uint32_t addr_offset;
	uint32_t data_offset;
};

#define MSM_DEV_INST_MAX                    16
struct msm_cam_v4l2_dev_inst {
	struct v4l2_fh  eventHandle;
	struct vb2_queue vid_bufq;
	spinlock_t vq_irqlock;
	struct list_head free_vq;
	struct v4l2_format vid_fmt;
	/* sensor pixel code*/
	enum v4l2_mbus_pixelcode sensor_pxlcode;
	struct msm_cam_v4l2_device *pcam;
	int my_index;
	int image_mode;
	int path;
	int buf_count;
	/* buffer offsets, if any */
	struct msm_cam_buf_offset **buf_offset;
	struct v4l2_crop crop;
	int streamon;
	struct msm_mem_map_info mem_map;
	int is_mem_map_inst;
	struct img_plane_info plane_info;
	int vbqueue_initialized;
};

struct msm_cam_mctl_node {
	/* MCTL V4l2 device */
	struct v4l2_device v4l2_dev;
	struct video_device *pvdev;
	struct msm_cam_v4l2_dev_inst *dev_inst[MSM_DEV_INST_MAX];
	struct msm_cam_v4l2_dev_inst *dev_inst_map[MSM_MAX_IMG_MODE];
	struct mutex dev_lock;
};

/* abstract camera device for each sensor successfully probed*/
struct msm_cam_v4l2_device {
	/* standard device interfaces */
	/* parent of video device to trace back */
	struct device dev;
	/* sensor's platform device*/
	struct platform_device *pdev;
	/* V4l2 device */
	struct v4l2_device v4l2_dev;
	/* will be registered as /dev/video*/
	struct video_device *pvdev;
	int use_count;
	/* will be used to init/release HW */
	struct msm_cam_media_controller mctl;

	/* parent device */
	struct device *parent_dev;

	struct mutex vid_lock;
	/* v4l2 format support */
	struct msm_isp_color_fmt *usr_fmts;
	int num_fmts;
	/* preview or snapshot */
	u32 mode;
	u32 memsize;

	int op_mode;
	int vnode_id;
	struct msm_cam_v4l2_dev_inst *dev_inst[MSM_DEV_INST_MAX];
	struct msm_cam_v4l2_dev_inst *dev_inst_map[MSM_MAX_IMG_MODE];
	/* native config device */
	struct cdev cdev;

	/* The message queue is used by the control thread to send commands
	 * to the config thread, and also by the HW to send messages to the
	 * config thread.  Thus it is the only queue that is accessed from
	 * both interrupt and process context.
	 */
	/* struct msm_device_queue event_q; */

	/* This queue used by the config thread to send responses back to the
	 * control thread.  It is accessed only from a process context.
	 * TO BE REMOVED
	 */
	struct msm_device_queue ctrl_q;

	struct mutex lock;
	uint8_t ctrl_data[max_control_command_size];
	struct msm_ctrl_cmd ctrl;
	uint32_t event_mask;
	struct msm_cam_mctl_node mctl_node;
};
static inline struct msm_cam_v4l2_device *to_pcam(
	struct v4l2_device *v4l2_dev)
{
	return container_of(v4l2_dev, struct msm_cam_v4l2_device, v4l2_dev);
}

/*pseudo v4l2 device and v4l2 event queue
  for server and config cdevs*/
struct v4l2_queue_util {
	struct video_device *pvdev;
	struct v4l2_fh  eventHandle;
};

/* abstract config device for all sensor successfully probed*/
struct msm_cam_config_dev {
	struct cdev config_cdev;
	struct v4l2_queue_util config_stat_event_queue;
	int use_count;
	/*struct msm_isp_ops* isp_subdev;*/
	struct msm_cam_media_controller *p_mctl;
	struct msm_mem_map_info mem_map;
};

/* abstract camera server device for all sensor successfully probed*/
struct msm_cam_server_dev {

	/* config node device*/
	struct cdev server_cdev;
	/* info of sensors successfully probed*/
	struct msm_camera_info camera_info;
	/* info of configs successfully created*/
	struct msm_cam_config_dev_info config_info;
	/* active working camera device - only one allowed at this time*/
	struct msm_cam_v4l2_device *pcam_active;
	/* number of camera devices opened*/
	atomic_t number_pcam_active;
	struct v4l2_queue_util server_command_queue;
	/* This queue used by the config thread to send responses back to the
	 * control thread.  It is accessed only from a process context.
	 */
	struct msm_device_queue ctrl_q;
	uint8_t ctrl_data[max_control_command_size];
	struct msm_ctrl_cmd ctrl;
	int use_count;
	/* all the registered ISP subdevice*/
	struct msm_isp_ops *isp_subdev[MSM_MAX_CAMERA_CONFIGS];
	/* info of MCTL nodes successfully probed*/
	struct msm_mctl_node_info mctl_node_info;
};

/* camera server related functions */


/* ISP related functions */
void msm_isp_vfe_dev_init(struct v4l2_subdev *vd);
/*
int msm_isp_register(struct msm_cam_v4l2_device *pcam);
*/
int msm_isp_register(struct msm_cam_server_dev *psvr);
void msm_isp_unregister(struct msm_cam_server_dev *psvr);
int msm_sensor_register(struct v4l2_subdev *);
int msm_isp_init_module(int g_num_config_nodes);

int msm_mctl_init_module(struct msm_cam_v4l2_device *pcam);
int msm_mctl_buf_init(struct msm_cam_v4l2_device *pcam);
int msm_mctl_init_user_formats(struct msm_cam_v4l2_device *pcam);
int msm_mctl_buf_done(struct msm_cam_media_controller *pmctl,
			int msg_type, struct msm_free_buf *buf,
			uint32_t frame_id);
int msm_mctl_buf_done_pp(struct msm_cam_media_controller *pmctl,
	int msg_type, struct msm_free_buf *frame, int dirty, int node_type);
int msm_mctl_reserve_free_buf(struct msm_cam_media_controller *pmctl,
				struct msm_cam_v4l2_dev_inst *pcam_inst,
				int path, struct msm_free_buf *free_buf);
int msm_mctl_release_free_buf(struct msm_cam_media_controller *pmctl,
				struct msm_cam_v4l2_dev_inst *pcam_inst,
				int path, struct msm_free_buf *free_buf);
/*Memory(PMEM) functions*/
int msm_register_pmem(struct hlist_head *ptype, void __user *arg,
				struct ion_client *client);
int msm_pmem_table_del(struct hlist_head *ptype, void __user *arg,
				struct ion_client *client);
int msm_pmem_region_get_phy_addr(struct hlist_head *ptype,
	struct msm_mem_map_info *mem_map, int32_t *phyaddr);
uint8_t msm_pmem_region_lookup(struct hlist_head *ptype,
	int pmem_type, struct msm_pmem_region *reg, uint8_t maxcount);
uint8_t msm_pmem_region_lookup_2(struct hlist_head *ptype,
					int pmem_type,
					struct msm_pmem_region *reg,
					uint8_t maxcount);
unsigned long msm_pmem_stats_vtop_lookup(
				struct msm_sync *sync,
				unsigned long buffer,
				int fd);
unsigned long msm_pmem_stats_ptov_lookup(struct msm_sync *sync,
						unsigned long addr, int *fd);

int msm_vfe_subdev_init(struct v4l2_subdev *sd, void *data,
					struct platform_device *pdev);
void msm_vfe_subdev_release(struct platform_device *pdev);

int msm_isp_subdev_ioctl(struct v4l2_subdev *sd,
	struct msm_vfe_cfg_cmd *cfgcmd, void *data);
int msm_vpe_subdev_init(struct v4l2_subdev *sd, void *data,
	struct platform_device *pdev);
int msm_gemini_subdev_init(struct v4l2_subdev *sd);
void msm_vpe_subdev_release(struct platform_device *pdev);
void msm_gemini_subdev_release(struct v4l2_subdev *gemini_sd);
int msm_isp_subdev_ioctl_vpe(struct v4l2_subdev *isp_subdev,
	struct msm_mctl_pp_cmd *cmd, void *data);
int msm_mctl_is_pp_msg_type(struct msm_cam_media_controller *p_mctl,
	int msg_type);
int msm_mctl_do_pp(struct msm_cam_media_controller *p_mctl,
			int msg_type, uint32_t y_phy, uint32_t frame_id);
int msm_mctl_pp_ioctl(struct msm_cam_media_controller *p_mctl,
			unsigned int cmd, unsigned long arg);
int msm_mctl_pp_notify(struct msm_cam_media_controller *pmctl,
			struct msm_mctl_pp_frame_info *pp_frame_info);
int msm_mctl_img_mode_to_inst_index(struct msm_cam_media_controller *pmctl,
					int out_type, int node_type);
struct msm_frame_buffer *msm_mctl_buf_find(
	struct msm_cam_media_controller *pmctl,
	struct msm_cam_v4l2_dev_inst *pcam_inst, int del_buf,
	int msg_type, struct msm_free_buf *fbuf);
void msm_mctl_gettimeofday(struct timeval *tv);
struct msm_frame_buffer *msm_mctl_get_free_buf(
		struct msm_cam_media_controller *pmctl,
		int msg_type);
int msm_mctl_put_free_buf(
		struct msm_cam_media_controller *pmctl,
		int msg_type, struct msm_frame_buffer *buf);
int msm_mctl_check_pp(struct msm_cam_media_controller *p_mctl,
		int msg_type, int *pp_divert_type, int *pp_type);
int msm_mctl_do_pp_divert(
	struct msm_cam_media_controller *p_mctl,
	int msg_type, struct msm_free_buf *fbuf,
	uint32_t frame_id, int pp_type);
int msm_mctl_buf_del(struct msm_cam_media_controller *pmctl,
	int msg_type,
	struct msm_frame_buffer *my_buf);
int msm_mctl_pp_release_free_frame(
	struct msm_cam_media_controller *p_mctl,
	void __user *arg);
int msm_mctl_pp_reserve_free_frame(
	struct msm_cam_media_controller *p_mctl,
	void __user *arg);
int msm_mctl_set_pp_key(struct msm_cam_media_controller *p_mctl,
				void __user *arg);
int msm_mctl_pp_done(
	struct msm_cam_media_controller *p_mctl,
	void __user *arg);
int msm_mctl_pp_divert_done(
	struct msm_cam_media_controller *p_mctl,
	void __user *arg);
int msm_setup_v4l2_event_queue(struct v4l2_fh *eventHandle,
					struct video_device *pvdev);
int msm_setup_mctl_node(struct msm_cam_v4l2_device *pcam);
struct msm_cam_v4l2_dev_inst *msm_mctl_get_pcam_inst(
		struct msm_cam_media_controller *pmctl,
		int image_mode);
int msm_mctl_buf_return_buf(struct msm_cam_media_controller *pmctl,
			int image_mode, struct msm_frame_buffer *buf);
int msm_mctl_pp_mctl_divert_done(struct msm_cam_media_controller *p_mctl,
					void __user *arg);
#endif /* __KERNEL__ */

#endif /* _MSM_H */
