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
 */

#ifndef _MSM_H
#define _MSM_H

#include <linux/version.h>
#include <linux/i2c.h>
#include <linux/videodev2.h>
#include <linux/pm_qos.h>
#include <linux/wakelock.h>
#include <linux/msm_ion.h>
#include <linux/iommu.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-event.h>
#include <media/v4l2-mediabus.h>
#include <media/videobuf2-dma-contig.h>
#include <media/videobuf2-msm-mem.h>
#include <media/msmb_camera.h>

#define MSM_POST_EVT_TIMEOUT 5000
#define MSM_POST_EVT_NOTIMEOUT 0xFFFFFFFF

struct msm_video_device {
	struct video_device *vdev;
	atomic_t opened;
};

int msm_post_event(struct v4l2_event *event, int timeout);
int  msm_create_session(unsigned int session, struct video_device *vdev);
int msm_destroy_session(unsigned int session_id);

int msm_create_stream(unsigned int session_id,
	unsigned int stream_id, struct vb2_queue *q);
void msm_delete_stream(unsigned int session_id, unsigned int stream_id);
int  msm_create_command_ack_q(unsigned int session_id, unsigned int stream_id);
void msm_delete_command_ack_q(unsigned int session_id, unsigned int stream_id);
struct msm_stream *msm_get_stream(unsigned int session_id,
	unsigned int stream_id);
struct vb2_queue *msm_get_stream_vb2q(unsigned int session_id,
	unsigned int stream_id);
struct msm_stream *msm_get_stream_from_vb2q(struct vb2_queue *q);

#endif /*_MSM_H */
