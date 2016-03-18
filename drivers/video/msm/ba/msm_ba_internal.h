/* Copyright (c) 2012-2016, The Linux Foundation. All rights reserved.
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

#ifndef _MSM_BA_INTERNAL_H_
#define _MSM_BA_INTERNAL_H_

#include <linux/atomic.h>
#include <linux/list.h>
#include <linux/time.h>
#include <linux/types.h>
#include <linux/completion.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/msm-bus.h>
#include <linux/msm-bus-board.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ctrls.h>
#include <media/videobuf2-core.h>
#include <media/msm_ba.h>

#define MSM_BA_DRV_NAME "msm_ba_driver"

#define MSM_BA_VERSION KERNEL_VERSION(0, 0, 1)

#define MAX_NAME_LENGTH 64

#define MAX_DEBUGFS_NAME MAX_NAME_LENGTH

#define DEFAULT_WIDTH 720
#define DEFAULT_HEIGHT 507

enum ba_dev_state {
	BA_DEV_UNINIT = 0,
	BA_DEV_INIT,
	BA_DEV_INIT_DONE,
	BA_DEV_INVALID
};

enum instance_state {
	MSM_BA_DEV_UNINIT_DONE = 0x0001,
	MSM_BA_DEV_INIT,
	MSM_BA_DEV_INIT_DONE,
	MSM_BA_OPEN,
	MSM_BA_OPEN_DONE,
	MSM_BA_START,
	MSM_BA_START_DONE,
	MSM_BA_STOP,
	MSM_BA_STOP_DONE,
	MSM_BA_CLOSE,
	MSM_BA_CLOSE_DONE,
	MSM_BA_DEV_UNINIT,
	MSM_BA_DEV_INVALID
};

struct ba_ctxt {

	struct mutex ba_cs;

	struct msm_ba_dev *dev_ctxt;

	struct dentry *debugfs_root;
};

enum profiling_points {
	SYS_INIT = 0,
	SESSION_INIT,
	MAX_PROFILING_POINTS
};

struct profile_data {
	int start;
	int stop;
	int cumulative;
	char name[64];
	int sampling;
	int average;
};

struct msm_ba_debug {
	struct profile_data pdata[MAX_PROFILING_POINTS];
	int profile;
	int samples;
};

struct msm_ba_dev_capability {
	u32 capability_set;
};

enum msm_ba_ip_type {
	BA_INPUT_CVBS = 0,
	BA_INPUT_COMPONENT,
	BA_INPUT_YC,
	BA_INPUT_RGB,
	BA_INPUT_HDMI,
	BA_INPUT_MHL,
	BA_INPUT_DVI,
	BA_INPUT_TTL,
	BA_INPUT_MAX = 0xffffffff
};

struct msm_ba_input_config {
	enum msm_ba_ip_type inputType;
	unsigned int index;
	const char *name;
	int ba_ip;
	int ba_out;
	const char *sd_name;
	int signal_status;
};

struct msm_ba_sd_event {
	struct list_head list;
	struct v4l2_event sd_event;
};

struct msm_ba_input {
	struct list_head list;
	enum msm_ba_ip_type inputType;
	unsigned int name_index;
	char name[32];
	int bridge_chip_ip;
	int ba_node_addr;
	int ba_out;
	int ba_ip_idx;
	struct v4l2_subdev *sd;
	int signal_status;
	int in_use;
	int ba_out_in_use;
	enum v4l2_priority prio;
};

struct msm_ba_dev {
	struct mutex dev_cs;

	struct platform_device *pdev;
	enum ba_dev_state state;

	struct list_head inputs;
	uint32_t num_inputs;

	/* V4L2 Framework */
	struct v4l2_device v4l2_dev;
	struct video_device *vdev;
	struct media_device mdev;

	struct list_head instances;

	/* BA v4l2 sub devs */
	uint32_t num_ba_subdevs;
	struct list_head sd_events;
	struct delayed_work sd_events_work;

	struct dentry *debugfs_root;
};

struct msm_ba_inst {
	struct list_head list;
	struct mutex inst_cs;
	struct msm_ba_dev *dev_ctxt;

	struct v4l2_input sd_input;
	/* current input priority */
	enum v4l2_priority input_prio;
	struct v4l2_output sd_output;
	struct v4l2_subdev *sd;
	int state;
	int saved_input;
	int restore;

	struct v4l2_fh event_handler;
	wait_queue_head_t kernel_event_queue;

	struct v4l2_ctrl **cluster;
	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl **ctrls;

	struct msm_ba_debug debug;
	struct dentry *debugfs_root;

	const struct msm_ba_ext_ops *ext_ops;
};

struct msm_ba_ctrl {
	u32 id;
	char name[MAX_NAME_LENGTH];
	enum v4l2_ctrl_type type;
	s32 minimum;
	s32 maximum;
	s32 default_value;
	u32 step;
	u32 menu_skip_mask;
	u32 flags;
	const char * const *qmenu;
};

struct ba_ctxt *msm_ba_get_ba_context(void);

void msm_ba_subdev_event_hndlr(struct v4l2_subdev *sd,
					unsigned int notification, void *arg);
void msm_ba_subdev_event_hndlr_delayed(struct work_struct *work);

#endif
