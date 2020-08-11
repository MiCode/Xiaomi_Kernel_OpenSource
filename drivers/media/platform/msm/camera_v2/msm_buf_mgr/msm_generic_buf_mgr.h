/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2013-2016, 2018, 2020, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MSM_BUF_GENERIC_MNGR_H__
#define __MSM_BUF_GENERIC_MNGR_H__

#include <linux/io.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <media/v4l2-subdev.h>
#include <media/msmb_camera.h>
#include <media/msmb_generic_buf_mgr.h>

#include "msm.h"
#include "msm_sd.h"

struct msm_get_bufs {
	struct list_head entry;
	struct vb2_v4l2_buffer *vb2_v4l2_buf;
	uint32_t session_id;
	uint32_t stream_id;
	uint32_t index;
};

struct msm_buf_mngr_device {
	struct list_head buf_qhead;
	spinlock_t buf_q_spinlock;
	struct msm_sd_subdev subdev;
	struct msm_sd_req_vb2_q vb2_ops;
	struct list_head cont_qhead;
	struct mutex cont_mutex;
};

struct msm_buf_mngr_user_buf_cont_info {
	struct list_head entry;
	uint32_t sessid;
	uint32_t strid;
	uint32_t index;
	int32_t main_fd;
	struct msm_camera_user_buf_cont_t *paddr;
	uint32_t cnt;
	struct dma_buf *dmabuf;
};

/* kernel space functions*/
struct msm_cam_buf_mgr_req_ops {
	int (*msm_cam_buf_mgr_ops)(unsigned int cmd, void *argp);
};

/* API to register callback from client. This assumes cb_struct is allocated by
 * client.
 */
int msm_cam_buf_mgr_register_ops(struct msm_cam_buf_mgr_req_ops *cb_struct);
#endif
