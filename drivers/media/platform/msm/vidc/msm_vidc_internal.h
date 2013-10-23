/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#ifndef _MSM_VIDC_INTERNAL_H_
#define _MSM_VIDC_INTERNAL_H_

#include <linux/atomic.h>
#include <linux/list.h>
#include <linux/time.h>
#include <linux/types.h>
#include <linux/completion.h>
#include <linux/wait.h>
#include <mach/msm_bus.h>
#include <mach/msm_bus_board.h>
#include <mach/ocmem.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ctrls.h>
#include <media/videobuf2-core.h>
#include <media/msm_vidc.h>
#include <media/msm_media_info.h>

#include "vidc_hfi_api.h"
#include "vidc_hfi_api.h"

#define MSM_VIDC_DRV_NAME "msm_vidc_driver"
#define MSM_VIDC_VERSION KERNEL_VERSION(0, 0, 1);
#define MAX_DEBUGFS_NAME 50
#define DEFAULT_TIMEOUT 3
#define DEFAULT_HEIGHT 1088
#define DEFAULT_WIDTH 1920
#define MIN_SUPPORTED_WIDTH 32
#define MIN_SUPPORTED_HEIGHT 32
#define MAX_SUPPORTED_WIDTH 3820
#define MAX_SUPPORTED_HEIGHT 2160



#define V4L2_EVENT_VIDC_BASE  10

#define SYS_MSG_START VIDC_EVENT_CHANGE
#define SYS_MSG_END SYS_DEBUG
#define SESSION_MSG_START SESSION_LOAD_RESOURCE_DONE
#define SESSION_MSG_END SESSION_PROPERTY_INFO
#define SYS_MSG_INDEX(__msg) (__msg - SYS_MSG_START)
#define SESSION_MSG_INDEX(__msg) (__msg - SESSION_MSG_START)

#define MAX_NAME_LENGTH 64

#define EXTRADATA_IDX(__num_planes) (__num_planes - 1)
enum vidc_ports {
	OUTPUT_PORT,
	CAPTURE_PORT,
	MAX_PORT_NUM
};

enum vidc_core_state {
	VIDC_CORE_UNINIT = 0,
	VIDC_CORE_INIT,
	VIDC_CORE_INIT_DONE,
	VIDC_CORE_INVALID
};

/*Donot change the enum values unless
 * you know what you are doing*/
enum instance_state {
	MSM_VIDC_CORE_UNINIT_DONE = 0x0001,
	MSM_VIDC_CORE_INIT,
	MSM_VIDC_CORE_INIT_DONE,
	MSM_VIDC_OPEN,
	MSM_VIDC_OPEN_DONE,
	MSM_VIDC_LOAD_RESOURCES,
	MSM_VIDC_LOAD_RESOURCES_DONE,
	MSM_VIDC_START,
	MSM_VIDC_START_DONE,
	MSM_VIDC_STOP,
	MSM_VIDC_STOP_DONE,
	MSM_VIDC_RELEASE_RESOURCES,
	MSM_VIDC_RELEASE_RESOURCES_DONE,
	MSM_VIDC_CLOSE,
	MSM_VIDC_CLOSE_DONE,
	MSM_VIDC_CORE_UNINIT,
	MSM_VIDC_CORE_INVALID
};

struct buf_info {
	struct list_head list;
	struct vb2_buffer *buf;
};

enum buffer_owner {
	DRIVER,
	FIRMWARE,
	CLIENT,
	MAX_OWNER
};

struct internal_buf {
	struct list_head list;
	enum hal_buffer buffer_type;
	struct msm_smem *handle;
	enum buffer_owner buffer_ownership;
};

struct msm_vidc_format {
	char name[MAX_NAME_LENGTH];
	u8 description[32];
	u32 fourcc;
	int num_planes;
	int type;
	u32 (*get_frame_size)(int plane, u32 height, u32 width);
};

struct msm_vidc_drv {
	struct mutex lock;
	struct list_head cores;
	int num_cores;
	struct dentry *debugfs_root;
};

struct msm_video_device {
	int type;
	struct video_device vdev;
};

struct session_prop {
	u32 width[MAX_PORT_NUM];
	u32 height[MAX_PORT_NUM];
	u32 fps;
	u32 bitrate;
};

struct buf_queue {
	struct vb2_queue vb2_bufq;
	struct mutex lock;
};

enum profiling_points {
	SYS_INIT = 0,
	SESSION_INIT,
	LOAD_RESOURCES,
	FRAME_PROCESSING,
	FW_IDLE,
	MAX_PROFILING_POINTS,
};

struct buf_count {
	int etb;
	int ftb;
	int fbd;
	int ebd;
};

struct profile_data {
	int start;
	int stop;
	int cumulative;
	char name[64];
	int sampling;
	int average;
};

struct msm_vidc_debug {
	struct profile_data pdata[MAX_PROFILING_POINTS];
	int profile;
	int samples;
};

enum msm_vidc_modes {
	VIDC_SECURE = 1 << 0,
	VIDC_TURBO = 1 << 1,
};

struct msm_vidc_core_capability {
	struct hal_capability_supported width;
	struct hal_capability_supported height;
	struct hal_capability_supported frame_rate;
	u32 pixelprocess_capabilities;
	struct hal_capability_supported scale_x;
	struct hal_capability_supported scale_y;
	u32 capability_set;
	enum buffer_mode_type buffer_mode[MAX_PORT_NUM];
};

struct msm_vidc_core {
	struct list_head list;
	struct mutex sync_lock, lock;
	int id;
	void *device;
	struct msm_video_device vdev[MSM_VIDC_MAX_DEVICES];
	struct v4l2_device v4l2_dev;
	struct list_head instances;
	struct dentry *debugfs_root;
	enum vidc_core_state state;
	struct completion completions[SYS_MSG_END - SYS_MSG_START + 1];
	enum msm_vidc_hfi_type hfi_type;
	struct msm_vidc_platform_resources resources;
};

struct msm_vidc_inst {
	struct list_head list;
	struct mutex sync_lock, lock;
	struct msm_vidc_core *core;
	int session_type;
	void *session;
	struct session_prop prop;
	int state;
	struct msm_vidc_format *fmts[MAX_PORT_NUM];
	struct buf_queue bufq[MAX_PORT_NUM];
	struct list_head pendingq;
	struct list_head internalbufs;
	struct list_head persistbufs;
	struct list_head outputbufs;
	struct buffer_requirements buff_req;
	void *mem_client;
	struct v4l2_ctrl_handler ctrl_handler;
	struct completion completions[SESSION_MSG_END - SESSION_MSG_START + 1];
	struct list_head ctrl_clusters;
	struct v4l2_fh event_handler;
	struct msm_smem *extradata_handle;
	wait_queue_head_t kernel_event_queue;
	bool in_reconfig;
	u32 reconfig_width;
	u32 reconfig_height;
	struct dentry *debugfs_root;
	u32 ftb_count;
	struct vb2_buffer *vb2_seq_hdr;
	void *priv;
	struct msm_vidc_debug debug;
	struct buf_count count;
	enum msm_vidc_modes flags;
	u32 multi_stream_mode;
	struct msm_vidc_core_capability capability;
	enum buffer_mode_type buffer_mode_set[MAX_PORT_NUM];
	struct list_head registered_bufs;
	bool map_output_buffer;
};

extern struct msm_vidc_drv *vidc_driver;

struct msm_vidc_ctrl_cluster {
	struct v4l2_ctrl **cluster;
	struct list_head list;
};

struct msm_vidc_ctrl {
	u32 id;
	char name[MAX_NAME_LENGTH];
	enum v4l2_ctrl_type type;
	s32 minimum;
	s32 maximum;
	s32 default_value;
	u32 step;
	u32 menu_skip_mask;
	const char * const *qmenu;
	u32 cluster;
	struct v4l2_ctrl *priv;
};

void handle_cmd_response(enum command_response cmd, void *data);
int msm_vidc_trigger_ssr(struct msm_vidc_core *core,
	enum hal_ssr_trigger_type type);
int msm_vidc_check_session_supported(struct msm_vidc_inst *inst);
void msm_vidc_queue_v4l2_event(struct msm_vidc_inst *inst, int event_type);

struct buffer_info {
	struct list_head list;
	int type;
	int num_planes;
	int fd[VIDEO_MAX_PLANES];
	int buff_off[VIDEO_MAX_PLANES];
	int size[VIDEO_MAX_PLANES];
	u32 uvaddr[VIDEO_MAX_PLANES];
	u32 device_addr[VIDEO_MAX_PLANES];
	struct msm_smem *handle[VIDEO_MAX_PLANES];
	enum v4l2_memory memory;
	u32 v4l2_index;
	bool pending_deletion;
	atomic_t ref_count;
	bool dequeued;
	bool inactive;
	bool mapped[VIDEO_MAX_PLANES];
	int same_fd_ref[VIDEO_MAX_PLANES];
	struct timeval timestamp;
};

struct buffer_info *device_to_uvaddr(struct msm_vidc_inst *inst,
			struct list_head *list, u32 device_addr);
int buf_ref_get(struct msm_vidc_inst *inst, struct buffer_info *binfo);
int buf_ref_put(struct msm_vidc_inst *inst, struct buffer_info *binfo);
int output_buffer_cache_invalidate(struct msm_vidc_inst *inst,
				struct buffer_info *binfo);
int qbuf_dynamic_buf(struct msm_vidc_inst *inst,
			struct buffer_info *binfo);
int unmap_and_deregister_buf(struct msm_vidc_inst *inst,
			struct buffer_info *binfo);
#endif
