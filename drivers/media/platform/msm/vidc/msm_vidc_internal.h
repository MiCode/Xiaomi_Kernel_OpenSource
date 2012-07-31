/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/completion.h>
#include <linux/clk.h>
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

#include "vidc_hal_api.h"

#define MSM_VIDC_DRV_NAME "msm_vidc_driver"
#define MSM_VIDC_VERSION KERNEL_VERSION(0, 0, 1);
#define MAX_DEBUGFS_NAME 50
#define DEFAULT_TIMEOUT 3

#define V4L2_EVENT_VIDC_BASE  10

#define SYS_MSG_START VIDC_EVENT_CHANGE
#define SYS_MSG_END SYS_DEBUG
#define SESSION_MSG_START SESSION_LOAD_RESOURCE_DONE
#define SESSION_MSG_END SESSION_PROPERTY_INFO
#define SYS_MSG_INDEX(__msg) (__msg - SYS_MSG_START)
#define SESSION_MSG_INDEX(__msg) (__msg - SESSION_MSG_START)

#define MAX_NAME_LENGTH 64
enum vidc_ports {
	OUTPUT_PORT,
	CAPTURE_PORT,
	MAX_PORT_NUM
};

enum vidc_core_state {
	VIDC_CORE_UNINIT = 0,
	VIDC_CORE_INIT,
	VIDC_CORE_INIT_DONE,
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
};

struct buf_info {
	struct list_head list;
	struct vb2_buffer *buf;
};

struct internal_buf {
	struct list_head list;
	struct msm_smem *handle;
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
	spinlock_t lock;
	struct list_head cores;
	int num_cores;
	struct dentry *debugfs_root;
};

struct msm_video_device {
	int type;
	struct video_device vdev;
};

struct msm_vidc_fw {
	void *cookie;
};

struct iommu_info {
	u32 addr_range[2];
	char name[MAX_NAME_LENGTH];
	char ctx[MAX_NAME_LENGTH];
	int domain;
	int partition;
};

enum io_maps {
	CP_MAP,
	NS_MAP,
	MAX_MAP
};

enum vidc_clocks {
	VCODEC_CLK,
	VCODEC_AHB_CLK,
	VCODEC_AXI_CLK,
	VCODEC_OCMEM_CLK,
	VCODEC_MAX_CLKS
};

struct load_freq_table {
	u32 load;
	u32 freq;
};

struct core_clock {
	char name[MAX_NAME_LENGTH];
	struct clk *clk;
	u32 count;
	struct load_freq_table load_freq_tbl[8];
};

struct vidc_bus_info {
	u32 vcodec_handle;
	u32 ocmem_handle;
};

struct on_chip_mem {
	struct ocmem_buf *buf;
	struct notifier_block vidc_ocmem_nb;
	void *handle;
};

struct msm_vidc_resources {
	struct msm_vidc_fw fw;
	struct iommu_info io_map[MAX_MAP];
	struct core_clock clock[VCODEC_MAX_CLKS];
	struct vidc_bus_info bus_info;
	struct on_chip_mem ocmem;
};

struct session_prop {
	u32 width;
	u32 height;
	u32 fps;
	u32 bitrate;
};

struct msm_vidc_core {
	struct list_head list;
	struct mutex sync_lock;
	int id;
	void *device;
	struct msm_video_device vdev[MSM_VIDC_MAX_DEVICES];
	struct v4l2_device v4l2_dev;
	spinlock_t lock;
	struct list_head instances;
	struct dentry *debugfs_root;
	u32 base_addr;
	u32 register_base;
	u32 register_size;
	u32 irq;
	enum vidc_core_state state;
	struct msm_vidc_resources resources;
	struct completion completions[SYS_MSG_END - SYS_MSG_START + 1];
};

struct msm_vidc_inst {
	struct list_head list;
	struct mutex sync_lock;
	struct msm_vidc_core *core;
	int session_type;
	void *session;
	struct session_prop prop;
	int state;
	const struct msm_vidc_format *fmts[MAX_PORT_NUM];
	struct vb2_queue vb2_bufq[MAX_PORT_NUM];
	spinlock_t lock;
	struct list_head pendingq;
	struct list_head internalbufs;
	struct list_head persistbufs;
	struct buffer_requirements buff_req;
	void *mem_client;
	struct v4l2_ctrl_handler ctrl_handler;
	struct completion completions[SESSION_MSG_END - SESSION_MSG_START + 1];
	struct v4l2_fh event_handler;
	struct msm_smem *extradata_handle;
	bool in_reconfig;
	u32 reconfig_width;
	u32 reconfig_height;
};

extern struct msm_vidc_drv *vidc_driver;

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
};

void handle_cmd_response(enum command_response cmd, void *data);
int msm_vidc_ocmem_notify_handler(struct notifier_block *this,
		unsigned long event, void *data);

#endif
