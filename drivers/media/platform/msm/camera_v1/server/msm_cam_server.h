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

#ifndef _MSM_CAM_SERVER_H
#define _MSM_CAM_SERVER_H

#include <linux/proc_fs.h>
#include <linux/ioctl.h>
#include <mach/camera.h>
#include "../msm.h"

uint32_t msm_cam_server_get_mctl_handle(void);
struct iommu_domain *msm_cam_server_get_domain(void);
int msm_cam_server_get_domain_num(void);
struct msm_cam_media_controller *msm_cam_server_get_mctl(uint32_t handle);
void msm_cam_server_free_mctl(uint32_t handle);
/* Server session control APIs */
int msm_server_begin_session(struct msm_cam_v4l2_device *pcam,
	int server_q_idx);
int msm_server_end_session(struct msm_cam_v4l2_device *pcam);
int msm_send_open_server(struct msm_cam_v4l2_device *pcam);
int msm_send_close_server(struct msm_cam_v4l2_device *pcam);
int msm_server_update_sensor_info(struct msm_cam_v4l2_device *pcam,
	struct msm_camera_sensor_info *sdata);
/* Server camera control APIs */
int msm_server_streamon(struct msm_cam_v4l2_device *pcam, int idx);
int msm_server_streamoff(struct msm_cam_v4l2_device *pcam, int idx);
int msm_server_get_usecount(void);
int32_t msm_find_free_queue(void);
int msm_server_proc_ctrl_cmd(struct msm_cam_v4l2_device *pcam,
	struct msm_camera_v4l2_ioctl_t *ioctl_ptr, int is_set_cmd);
int msm_server_private_general(struct msm_cam_v4l2_device *pcam,
	struct msm_camera_v4l2_ioctl_t *ioctl_ptr);
int msm_server_s_ctrl(struct msm_cam_v4l2_device *pcam,
	struct v4l2_control *ctrl);
int msm_server_g_ctrl(struct msm_cam_v4l2_device *pcam,
	struct v4l2_control *ctrl);
int msm_server_q_ctrl(struct msm_cam_v4l2_device *pcam,
	struct v4l2_queryctrl *queryctrl);
int msm_server_set_fmt(struct msm_cam_v4l2_device *pcam, int idx,
	struct v4l2_format *pfmt);
int msm_server_set_fmt_mplane(struct msm_cam_v4l2_device *pcam, int idx,
	struct v4l2_format *pfmt);
int msm_server_get_fmt(struct msm_cam_v4l2_device *pcam,
	int idx, struct v4l2_format *pfmt);
int msm_server_get_fmt_mplane(struct msm_cam_v4l2_device *pcam,
	int idx, struct v4l2_format *pfmt);
int msm_server_try_fmt(struct msm_cam_v4l2_device *pcam,
	struct v4l2_format *pfmt);
int msm_server_try_fmt_mplane(struct msm_cam_v4l2_device *pcam,
	struct v4l2_format *pfmt);
int msm_server_v4l2_subscribe_event(struct v4l2_fh *fh,
	struct v4l2_event_subscription *sub);
int msm_server_v4l2_unsubscribe_event(struct v4l2_fh *fh,
	struct v4l2_event_subscription *sub);
int msm_server_get_crop(struct msm_cam_v4l2_device *pcam,
	int idx, struct v4l2_crop *crop);
int msm_cam_server_request_irq(void *arg);
int msm_cam_server_update_irqmap(
	struct msm_cam_server_irqmap_entry *entry);
int msm_cam_server_config_interface_map(u32 extendedmode,
	uint32_t mctl_handle, int vnode_id, int is_bayer_sensor);
#endif /* _MSM_CAM_SERVER_H */
