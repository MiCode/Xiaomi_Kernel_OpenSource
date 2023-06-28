/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CAM_VIDEO_H
#define __MTK_CAM_VIDEO_H

#include <linux/types.h>
#include <linux/videodev2.h>
#include <media/media-entity.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-dev.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-v4l2.h>

#include "mtk_cam-ipi.h"
#include "mtk_camera-videodev2.h"

#define MAX_PLANE_NUM 3
#define MAX_SUBSAMPLE_PLANE_NUM 8

struct mtk_cam_device;
struct mtk_cam_resource;
struct mtk_raw_pde_config;

typedef int (*set_pad_fmt_func_t)(struct v4l2_subdev *sd,
			   struct v4l2_subdev_pad_config *cfg,
			   struct v4l2_mbus_framefmt *sink_fmt,
			   struct mtk_cam_resource *res,
			   int pad, int which);

typedef int (*set_pad_selection_func_t)(struct v4l2_subdev *sd,
					  struct v4l2_subdev_pad_config *cfg,
					  struct v4l2_mbus_framefmt *sink_fmt,
					  struct mtk_cam_resource *res,
					  int pad, int which);

/*
 * struct mtk_cam_buffer - MTK camera device buffer.
 *
 * @vb: Embedded struct vb2_v4l2_buffer.
 * @queue: List entry of the object
 * @daddr: The DMA address of this buffer.
 * @scp_addr: The SCP address of this buffer which
 *            is only supported for meta input node.
 *
 */
struct mtk_cam_buffer {
	struct vb2_v4l2_buffer vbb;
	struct list_head list;
	struct list_head stream_data_list;

	dma_addr_t daddr;
	dma_addr_t scp_addr;
};

struct mtk_cam_format_desc {
	struct v4l2_format vfmt;
	struct v4l2_mbus_framefmt pfmt;
};

struct mtk_cam_pad_ops {
	set_pad_fmt_func_t set_pad_fmt;
	set_pad_selection_func_t set_pad_selection;
};

/*
 * struct mtk_cam_dev_node_desc - MTK camera device node descriptor
 *
 * @id: id of the node
 * @name: name of the node
 * @cap: supported V4L2 capabilities
 * @buf_type: supported V4L2 buffer type
 * @dma_port: the dma ports associated to the node
 * @link_flags: default media link flags
 * @smem_alloc: using the smem_dev as alloc device or not
 * @need_cache_sync_on_prepare: do cache sync at buf_prepare
 * @need_cache_sync_on_finish: do cache sync at buf_finish
 * @image: true for image node, false for meta node
 * @num_fmts: the number of supported node formats
 * @default_fmt_idx: default format of this node
 * @max_buf_count: maximum VB2 buffer count
 * @ioctl_ops:  mapped to v4l2_ioctl_ops
 * @fmts: supported format
 * @frmsizes: supported V4L2 frame size number
 *
 */
struct mtk_cam_dev_node_desc {
	u8 id;
	const char *name;
	u32 cap;
	u32 buf_type;
	u32 dma_port;
	u32 link_flags;
	u8 smem_alloc:1;
	u8 need_cache_sync_on_prepare:1;
	u8 need_cache_sync_on_finish:1;
	u8 image:1;
	u8 num_fmts;
	u8 default_fmt_idx;
	u8 max_buf_count;
	const struct v4l2_ioctl_ops *ioctl_ops;
	const struct mtk_cam_format_desc *fmts;
	const struct v4l2_frmsizeenum *frmsizes;
	struct mtk_cam_pad_ops *pad_ops;
	u8 hsf_en;
};

/*
 * struct mtk_cam_video_device - Mediatek video device structure.
 *
 * FIXME
 *
 */
struct mtk_cam_video_device {
	struct mtkcam_ipi_uid uid;
	struct mtk_cam_dev_node_desc desc;
	unsigned int enabled;

	struct vb2_queue vb2_q;
	struct video_device vdev;
	struct media_pad pad;
	struct v4l2_format active_fmt;
	/* use first 4 elements of reserved field of v4l2_pix_format_mplane as request fd */
	struct v4l2_format pending_fmt;
	/* use first elements of reserved field of v4l2_selection as request fd*/
	struct v4l2_format sink_fmt_for_dc_rawi;
	struct v4l2_selection pending_crop;
	/* Serializes vb2 queue and video device operations */
	struct mutex q_lock;
	int streaming_id;

	/* cached ctx info */
	struct mtk_cam_ctx *ctx;
};

struct mtk_format_info {
	u32 format;
	u8 mem_planes;
	u8 comp_planes;
	u8 bpp[4];
	u8 hdiv;
	u8 vdiv;
	/* numerator of bit ratio */
	u8 bit_r_num;
	/* denominator of bit ratio */
	u8 bit_r_den;
};

int mtk_cam_dmao_xsize(int w, unsigned int ipi_fmt, int pixel_mode_shift);
int mtk_cam_fmt_get_raw_feature(struct v4l2_pix_format_mplane *fmt_mp);
void mtk_cam_fmt_set_raw_feature(struct v4l2_pix_format_mplane *fmt_mp, int raw_feature);
int mtk_cam_fmt_get_request(struct v4l2_pix_format_mplane *fmt_mp);
void mtk_cam_fmt_set_request(struct v4l2_pix_format_mplane *fmt_mp, int request_fd);
int mtk_cam_selection_get_request(struct v4l2_selection *crop);
void mtk_cam_selection_set_request(struct v4l2_selection *crop, s32 request_fd);
int mtk_cam_video_register(struct mtk_cam_video_device *video,
			   struct v4l2_device *v4l2_dev);
void mtk_cam_video_unregister(struct mtk_cam_video_device *video);

static inline struct mtk_cam_video_device *
file_to_mtk_cam_node(struct file *__file)
{
	return container_of(video_devdata(__file),
		struct mtk_cam_video_device, vdev);
}

static inline struct mtk_cam_buffer *
mtk_cam_vb2_buf_to_dev_buf(struct vb2_buffer *__vb)
{
	return container_of(__vb, struct mtk_cam_buffer, vbb.vb2_buf);
}

static inline struct mtk_cam_video_device *
mtk_cam_vbq_to_vdev(struct vb2_queue *__vq)
{
	return container_of(__vq, struct mtk_cam_video_device, vb2_q);
}

const struct v4l2_format *
mtk_cam_dev_find_fmt(struct mtk_cam_dev_node_desc *desc, u32 format);

int mtk_cam_vidioc_querycap(struct file *file, void *fh,
			    struct v4l2_capability *cap);

int mtk_cam_vidioc_enum_framesizes(struct file *filp, void *priv,
				   struct v4l2_frmsizeenum *sizes);

int mtk_cam_vidioc_enum_fmt(struct file *file, void *fh,
			    struct v4l2_fmtdesc *f);

int mtk_cam_vidioc_g_fmt(struct file *file, void *fh,
			 struct v4l2_format *f);

int mtk_cam_vidioc_s_fmt(struct file *file, void *fh,
			 struct v4l2_format *f);

int mtk_cam_vidioc_try_fmt(struct file *file, void *fh,
			   struct v4l2_format *f);

int mtk_cam_vidioc_meta_enum_fmt(struct file *file, void *fh,
				 struct v4l2_fmtdesc *f);

int mtk_cam_vidioc_s_selection(struct file *file, void *fh,
				struct v4l2_selection *s);

int mtk_cam_vidioc_g_meta_fmt(struct file *file, void *fh,
			      struct v4l2_format *f);

/* Utility functions to convert format enum */
unsigned int mtk_cam_get_sensor_pixel_id(unsigned int fmt);

unsigned int mtk_cam_get_sensor_fmt(unsigned int fmt);

int mtk_cam_get_fmt_size_factor(unsigned int ipi_fmt);

unsigned int mtk_cam_get_pixel_bits(unsigned int pix_fmt);

unsigned int mtk_cam_get_img_fmt(unsigned int fourcc);

int mtk_cam_video_set_fmt(struct mtk_cam_video_device *node, struct v4l2_format *f, int feature);

int is_mtk_format(u32 pixelformat);

int is_yuv_ufo(u32 pixelformat);

int is_raw_ufo(u32 pixelformat);

int is_fullg_rb(u32 pixelformat);

const struct mtk_format_info *mtk_format_info(u32 format);

#endif /*__MTK_CAM_VIDEO_H*/
