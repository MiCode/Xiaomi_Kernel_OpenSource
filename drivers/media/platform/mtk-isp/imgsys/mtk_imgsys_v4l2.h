/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Frederic Chen <frederic.chen@mediatek.com>
 *         Holmes Chiou <holmes.chiou@mediatek.com>
 *
 */

#ifndef _MTK_IMGSYS_V4L2_H_
#define _MTK_IMGSYS_V4L2_H_

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/pm_runtime.h>
#include <linux/remoteproc.h>
//#include <linux/remoteproc/mtk_scp.h>
#include <linux/videodev2.h>
#include <media/videobuf2-dma-contig.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-event.h>
#include "mtk_imgsys-dev.h"
#include "mtk_imgsys-hw.h"
#include "mtk-hcp.h"
#include "mtkdip.h"

static int mtk_imgsys_sd_subscribe_event(struct v4l2_subdev *subdev,
				      struct v4l2_fh *fh,
				      struct v4l2_event_subscription *sub);

static int mtk_imgsys_subdev_s_stream(struct v4l2_subdev *sd,
				int enable);

static int mtk_imgsys_subdev_get_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_pad_config *cfg,
			   struct v4l2_subdev_format *fmt);

static int mtk_imgsys_subdev_set_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_pad_config *cfg,
			   struct v4l2_subdev_format *fmt);

static int mtk_imgsys_subdev_get_selection(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_selection *sel);

static int mtk_imgsys_subdev_set_selection(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_selection *sel);

static int mtk_imgsys_link_setup(struct media_entity *entity,
			   const struct media_pad *local,
			   const struct media_pad *remote,
			   u32 flags);

static int mtk_imgsys_vb2_meta_buf_prepare(struct vb2_buffer *vb);

static int mtk_imgsys_vb2_video_buf_prepare(struct vb2_buffer *vb);

static int mtk_imgsys_vb2_buf_out_validate(struct vb2_buffer *vb);

static int mtk_imgsys_vb2_meta_buf_init(struct vb2_buffer *vb);

static int mtk_imgsys_vb2_video_buf_init(struct vb2_buffer *vb);

static void mtk_imgsys_vb2_queue_meta_buf_cleanup(struct vb2_buffer *vb);

static void mtk_imgsys_vb2_buf_queue(struct vb2_buffer *vb);

static int mtk_imgsys_vb2_meta_queue_setup(struct vb2_queue *vq,
				 unsigned int *num_buffers,
				 unsigned int *num_planes,
				 unsigned int sizes[],
				 struct device *alloc_devs[]);

static int mtk_imgsys_vb2_video_queue_setup(struct vb2_queue *vq,
				  unsigned int *num_buffers,
				  unsigned int *num_planes,
				  unsigned int sizes[],
				  struct device *alloc_devs[]);

static int mtk_imgsys_vb2_start_streaming(struct vb2_queue *vq,
							unsigned int count);

static void mtk_imgsys_vb2_stop_streaming(struct vb2_queue *vq);

static void mtk_imgsys_vb2_request_complete(struct vb2_buffer *vb);

static int mtk_imgsys_videoc_querycap(struct file *file, void *fh,
				struct v4l2_capability *cap);

static int mtk_imgsys_videoc_try_fmt(struct file *file, void *fh,
			   struct v4l2_format *f);

static int mtk_imgsys_videoc_g_fmt(struct file *file, void *fh,
			 struct v4l2_format *f);

static int mtk_imgsys_videoc_s_fmt(struct file *file, void *fh,
			 struct v4l2_format *f);

static int mtk_imgsys_videoc_enum_framesizes(struct file *file, void *priv,
				   struct v4l2_frmsizeenum *sizes);

static int mtk_imgsys_videoc_enum_fmt(struct file *file, void *fh,
				struct v4l2_fmtdesc *f);

static int mtk_imgsys_meta_enum_format(struct file *file, void *fh,
				 struct v4l2_fmtdesc *f);

static int mtk_imgsys_videoc_g_meta_fmt(struct file *file, void *fh,
				  struct v4l2_format *f);

static int mtk_imgsys_videoc_s_meta_fmt(struct file *file, void *fh,
				  struct v4l2_format *f);

static int mtk_imgsys_video_device_s_ctrl(struct v4l2_ctrl *ctrl);

static int mtk_imgsys_vidioc_qbuf(struct file *file, void *priv,
				  struct v4l2_buffer *buf);

#ifdef BATCH_MODE_V3
long mtk_imgsys_vidioc_default(struct file *file, void *fh,
			bool valid_prio, unsigned int cmd, void *arg);
#endif
long mtk_imgsys_subdev_ioctl(struct v4l2_subdev *subdev, unsigned int cmd,
								void *arg);

/******************** function pointers ********************/
static const struct v4l2_subdev_core_ops mtk_imgsys_subdev_core_ops = {
	.subscribe_event = mtk_imgsys_sd_subscribe_event,
	.ioctl = mtk_imgsys_subdev_ioctl,
	.unsubscribe_event = v4l2_event_subdev_unsubscribe,
};

static const struct v4l2_subdev_video_ops mtk_imgsys_subdev_video_ops = {
	.s_stream = mtk_imgsys_subdev_s_stream,
};

static const struct v4l2_subdev_pad_ops mtk_imgsys_subdev_pad_ops = {
	.link_validate = v4l2_subdev_link_validate_default,
	.get_fmt = mtk_imgsys_subdev_get_fmt,
	.set_fmt = mtk_imgsys_subdev_set_fmt,
	.get_selection = mtk_imgsys_subdev_get_selection,
	.set_selection = mtk_imgsys_subdev_set_selection,
};

static const struct v4l2_subdev_ops mtk_imgsys_subdev_ops = {
	.core = &mtk_imgsys_subdev_core_ops,
	.video = &mtk_imgsys_subdev_video_ops,
	.pad = &mtk_imgsys_subdev_pad_ops,
};

static const struct media_entity_operations mtk_imgsys_media_ops = {
	.link_setup = mtk_imgsys_link_setup,
	.link_validate = v4l2_subdev_link_validate,
};

static struct media_request *mtk_imgsys_request_alloc(
						struct media_device *mdev);
static void mtk_imgsys_request_free(struct media_request *req);

static int mtk_imgsys_vb2_request_validate(struct media_request *req);

static void mtk_imgsys_vb2_request_queue(struct media_request *req);

int mtk_imgsys_v4l2_fh_open(struct file *filp);

int mtk_imgsys_v4l2_fh_release(struct file *filp);

static const struct media_device_ops mtk_imgsys_media_req_ops = {
	.req_validate = mtk_imgsys_vb2_request_validate,
	.req_queue = mtk_imgsys_vb2_request_queue,
	.req_alloc = mtk_imgsys_request_alloc,
	.req_free = mtk_imgsys_request_free,
};

static const struct v4l2_ctrl_ops mtk_imgsys_video_device_ctrl_ops = {
	.s_ctrl = mtk_imgsys_video_device_s_ctrl,
};

static const struct vb2_ops mtk_imgsys_vb2_meta_ops = {
	.buf_queue = mtk_imgsys_vb2_buf_queue,
	.queue_setup = mtk_imgsys_vb2_meta_queue_setup,
	.buf_init = mtk_imgsys_vb2_meta_buf_init,
	.buf_prepare  = mtk_imgsys_vb2_meta_buf_prepare,
	.buf_out_validate = mtk_imgsys_vb2_buf_out_validate,
	.buf_cleanup = mtk_imgsys_vb2_queue_meta_buf_cleanup,
	.start_streaming = mtk_imgsys_vb2_start_streaming,
	.stop_streaming = mtk_imgsys_vb2_stop_streaming,
	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,
	.buf_request_complete = mtk_imgsys_vb2_request_complete,
};

static const struct vb2_ops mtk_imgsys_vb2_video_ops = {
	.buf_queue = mtk_imgsys_vb2_buf_queue,
	.queue_setup = mtk_imgsys_vb2_video_queue_setup,
	.buf_init = mtk_imgsys_vb2_video_buf_init,
	.buf_prepare  = mtk_imgsys_vb2_video_buf_prepare,
	.buf_out_validate = mtk_imgsys_vb2_buf_out_validate,
	.start_streaming = mtk_imgsys_vb2_start_streaming,
	.stop_streaming = mtk_imgsys_vb2_stop_streaming,
	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,
	.buf_request_complete = mtk_imgsys_vb2_request_complete,
};

static const struct v4l2_file_operations mtk_imgsys_v4l2_fops = {
	.unlocked_ioctl = video_ioctl2,
	.open = mtk_imgsys_v4l2_fh_open,
	.release = mtk_imgsys_v4l2_fh_release,
	.poll = vb2_fop_poll,
	.mmap = vb2_fop_mmap,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = v4l2_compat_ioctl32,
#endif
};

/********************************************
 * MTK DIP V4L2 Settings *
 *******************************************
 */

static const struct v4l2_ioctl_ops mtk_imgsys_v4l2_video_out_ioctl_ops = {
	.vidioc_querycap = mtk_imgsys_videoc_querycap,

	.vidioc_enum_framesizes = mtk_imgsys_videoc_enum_framesizes,
	.vidioc_enum_fmt_vid_out = mtk_imgsys_videoc_enum_fmt,
	.vidioc_g_fmt_vid_out_mplane = mtk_imgsys_videoc_g_fmt,
	.vidioc_s_fmt_vid_out_mplane = mtk_imgsys_videoc_s_fmt,
	.vidioc_try_fmt_vid_out_mplane = mtk_imgsys_videoc_try_fmt,

	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = mtk_imgsys_vidioc_qbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,
	.vidioc_expbuf = vb2_ioctl_expbuf,

	.vidioc_subscribe_event = v4l2_ctrl_subscribe_event,
	.vidioc_unsubscribe_event = v4l2_event_unsubscribe,
#ifdef BATCH_MODE_V3
	.vidioc_default = mtk_imgsys_vidioc_default,
#endif
};

static const struct v4l2_ioctl_ops mtk_imgsys_v4l2_video_cap_ioctl_ops = {
	.vidioc_querycap = mtk_imgsys_videoc_querycap,

	.vidioc_enum_framesizes = mtk_imgsys_videoc_enum_framesizes,
	.vidioc_enum_fmt_vid_cap = mtk_imgsys_videoc_enum_fmt,
	.vidioc_g_fmt_vid_cap_mplane = mtk_imgsys_videoc_g_fmt,
	.vidioc_s_fmt_vid_cap_mplane = mtk_imgsys_videoc_s_fmt,
	.vidioc_try_fmt_vid_cap_mplane = mtk_imgsys_videoc_try_fmt,

	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = mtk_imgsys_vidioc_qbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,
	.vidioc_expbuf = vb2_ioctl_expbuf,

	.vidioc_subscribe_event = v4l2_ctrl_subscribe_event,
	.vidioc_unsubscribe_event = v4l2_event_unsubscribe,
};

static const struct v4l2_ioctl_ops mtk_imgsys_v4l2_meta_out_ioctl_ops = {
	.vidioc_querycap = mtk_imgsys_videoc_querycap,

	.vidioc_enum_fmt_meta_out = mtk_imgsys_meta_enum_format,
	.vidioc_g_fmt_meta_out = mtk_imgsys_videoc_g_meta_fmt,
	.vidioc_s_fmt_meta_out = mtk_imgsys_videoc_s_meta_fmt,
	.vidioc_try_fmt_meta_out = mtk_imgsys_videoc_g_meta_fmt,

	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = mtk_imgsys_vidioc_qbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,
	.vidioc_expbuf = vb2_ioctl_expbuf,
};

#endif // _MTK_IMGSYS_V4L2_H_
