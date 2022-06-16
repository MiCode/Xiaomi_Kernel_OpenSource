// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.


#include "mtk_cam.h"
#include "mtk_cam-fmt_utils.h"
#include "mtk_cam-job_utils.h"
#include "mtk_cam-ufbc-def.h"

#define buf_printk(fmt, arg...)					\
	do {							\
		if (unlikely(CAM_DEBUG_ENABLED(IPI_BUF)))	\
			pr_info("%s: " fmt, __func__, ##arg);	\
	} while (0)


void _set_timestamp(struct mtk_cam_job *job,
	u64 time_boot, u64 time_mono)
{
	job->timestamp = time_boot;
	job->timestamp_mono = time_mono;
}

void _state_trans(struct mtk_cam_job *job,
	enum MTK_CAMSYS_STATE_IDX from, enum MTK_CAMSYS_STATE_IDX to)
{
	if (job->state == from)
		job->state = to;
}
int get_raw_subdev_idx(int used_pipe)
{
	int used_raw = USED_MASK_GET_SUBMASK(&used_pipe, raw);
	int i;

	for (i = 0; used_raw; i++)
		if (SUBMASK_HAS(&used_raw, raw, i))
			return i;

	return -1;
}
#define SENSOR_SET_DEADLINE_MS  18
#define SENSOR_SET_DEADLINE_MS_60FPS  6

static int
_sensor_margin_ms(int fps_ratio, int sub_sample)
{
	int timer_ms = 0;

	if (sub_sample > 0) {
		if (fps_ratio > 1)
			timer_ms = SENSOR_SET_DEADLINE_MS;
		else
			timer_ms = SENSOR_SET_DEADLINE_MS * sub_sample;
	} else {
		if (fps_ratio > 2)
			timer_ms = SENSOR_SET_DEADLINE_MS / fps_ratio;
		else if (fps_ratio == 2)
			timer_ms = SENSOR_SET_DEADLINE_MS_60FPS;
		else
			timer_ms = SENSOR_SET_DEADLINE_MS;
	}

	return timer_ms;
}

int get_subsample_ratio(int feature)
{
	int sub_ratio = 0;
	int sub_value = feature & MTK_CAM_FEATURE_SUBSAMPLE_MASK;

	switch (sub_value) {
	case HIGHFPS_2_SUBSAMPLE:
		sub_ratio = 1;
		break;
	case HIGHFPS_4_SUBSAMPLE:
		sub_ratio = 3;
		break;
	case HIGHFPS_8_SUBSAMPLE:
		sub_ratio = 7;
		break;
	case HIGHFPS_16_SUBSAMPLE:
		sub_ratio = 15;
		break;
	case HIGHFPS_32_SUBSAMPLE:
		sub_ratio = 31;
		break;
	default:
		sub_ratio = 0;
	}

	return sub_ratio;
}
int get_apply_sensor_margin_ms(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct v4l2_subdev_frame_interval fi;
	int fps_factor = 1, sub_ratio = 0;
	int apply_sensor_ms = 0;

	fi.pad = 0;
	v4l2_subdev_call(ctx->sensor, video, g_frame_interval, &fi);
	fps_factor = (fi.interval.numerator > 0) ?
		(fi.interval.denominator / fi.interval.numerator / 30) : 1;
	sub_ratio = get_subsample_ratio(job->feature_job);

	apply_sensor_ms = _sensor_margin_ms(fps_factor, sub_ratio);

	pr_info("[%s] fps_X/sub_X/apply_sensor_ms:%d/%d/%d",
		__func__, fps_factor, sub_ratio, apply_sensor_ms);

	return apply_sensor_ms;
}

unsigned int
_get_master_raw_id(unsigned int num_raw, unsigned int enabled_raw)
{
	unsigned int i;

	for (i = 0; i < num_raw; i++) {
		if (enabled_raw & (1 << i))
			break;
	}

	if (i == num_raw)
		pr_info("no raw id found, enabled_raw 0x%x", enabled_raw);

	return i;
}
static int fill_img_in_driver_buf(struct mtkcam_ipi_img_input *ii,
				  struct mtkcam_ipi_uid uid,
				  struct mtk_cam_driver_buf_desc *desc)
{
	int i;

	/* uid */
	ii->uid = uid;

	/* fmt */
	ii->fmt.format = desc->ipi_fmt;
	ii->fmt.s = (struct mtkcam_ipi_size) {
		.w = desc->width,
		.h = desc->height,
	};

	for (i = 0; i < ARRAY_SIZE(ii->fmt.stride); i++)
		ii->fmt.stride[i] = i < ARRAY_SIZE(desc->stride) ?
			desc->stride[i] : 0;

	/* buf */
	ii->buf[0].size = desc->size;
	ii->buf[0].iova = desc->daddr;
	ii->buf[0].ccd_fd = 0; /* TODO: ufo : desc->fd; */

	buf_printk("%dx%d sz %zu\n",
		   desc->width, desc->height, desc->size);
	return 0;
}

int update_work_buffer_to_ipi_frame(struct req_buffer_helper *helper)
{
	struct mtkcam_ipi_frame_param *fp = helper->fp;
	struct mtk_cam_job *job = helper->job;
	struct mtk_cam_ctx *ctx = job->src_ctx;
	int ret = 0;

	struct mtkcam_ipi_img_input *ii;
	struct mtkcam_ipi_uid uid;

	if (helper->filled_hdr_buffer)
		return 0;

	uid.pipe_id = get_raw_subdev_idx(ctx->used_pipe);
	uid.id = MTKCAM_IPI_RAW_RAWI_2;

	ii = &fp->img_ins[helper->ii_idx];
	++helper->ii_idx;

	ret = fill_img_in_driver_buf(ii, uid, &ctx->hdr_buf_desc);


	return ret;
}

static int fill_img_fmt(struct mtkcam_ipi_pix_fmt *ipi_pfmt,
			struct mtk_cam_buffer *buf)
{
	struct mtk_cam_cached_image_info *info = &buf->image_info;
	int i;

	ipi_pfmt->format = mtk_cam_get_img_fmt(info->v4l2_pixelformat);
	ipi_pfmt->s = (struct mtkcam_ipi_size) {
		.w = info->width,
		.h = info->height,
	};

	for (i = 0; i < ARRAY_SIZE(ipi_pfmt->stride); i++)
		ipi_pfmt->stride[i] = i < ARRAY_SIZE(info->bytesperline) ?
			info->bytesperline[i] : 0;
	return 0;
}

int fill_img_in_hdr(struct mtkcam_ipi_img_input *ii,
			struct mtk_cam_buffer *buf,
			struct mtk_cam_video_device *node)
{
	/* uid */
	ii->uid = node->uid;
	ii->uid.id = MTKCAM_IPI_RAW_RAWI_2;
	/* fmt */
	fill_img_fmt(&ii->fmt, buf);

	/* FIXME: porting workaround */
	ii->buf[0].size = buf->image_info.size[0];
	ii->buf[0].iova = buf->daddr;
	ii->buf[0].ccd_fd = buf->vbb.vb2_buf.planes[0].m.fd;

	buf_printk("buf->daddr:0x%x, io->buf[0][0].iova:0x%x, size%d",
		   buf->daddr, ii->buf[0].iova, ii->buf[0].size);

	return 0;
}

struct mtkcam_ipi_crop
v4l2_rect_to_ipi_crop(const struct v4l2_rect *r)
{
	return (struct mtkcam_ipi_crop) {
		.p = (struct mtkcam_ipi_point) {
			.x = r->left,
			.y = r->top,
		},
		.s = (struct mtkcam_ipi_size) {
			.w = r->width,
			.h = r->height,
		},
	};
}

int fill_imgo_out_subsample(struct mtkcam_ipi_img_output *io,
			struct mtk_cam_buffer *buf,
			struct mtk_cam_video_device *node,
			int subsample_ratio)
{
	int i;

	/* uid */
	io->uid = node->uid;

	/* fmt */
	fill_img_fmt(&io->fmt, buf);

	for (i = 0; i < subsample_ratio; i++) {
		/* FIXME: porting workaround */
		io->buf[i][0].size = buf->image_info.size[0];
		io->buf[i][0].iova = buf->daddr + io->buf[i][0].size;
		io->buf[i][0].ccd_fd = buf->vbb.vb2_buf.planes[0].m.fd;
		buf_printk("i=%d: buf->daddr:0x%x, io->buf[i][0].iova:0x%x, size:%d",
			   i, buf->daddr, io->buf[i][0].iova, io->buf[i][0].size);
	}

	/* crop */
	io->crop = v4l2_rect_to_ipi_crop(&buf->image_info.crop);

	buf_printk("%s %dx%d @%d,%d-%dx%d\n",
		   node->desc.name,
		   io->fmt.s.w, io->fmt.s.h,
		   io->crop.p.x, io->crop.p.y, io->crop.s.w, io->crop.s.h);

	return 0;
}
int fill_img_out_hdr(struct mtkcam_ipi_img_output *io,
			struct mtk_cam_buffer *buf,
			struct mtk_cam_video_device *node)
{
	/* uid */
	io->uid = node->uid;

	/* fmt */
	fill_img_fmt(&io->fmt, buf);

	/* FIXME: porting workaround */
	io->buf[0][0].size = buf->image_info.size[0];
	io->buf[0][0].iova = buf->daddr + io->buf[0][0].size;
	io->buf[0][0].ccd_fd = buf->vbb.vb2_buf.planes[0].m.fd;

	/* crop */
	io->crop = v4l2_rect_to_ipi_crop(&buf->image_info.crop);

	buf_printk("buf->daddr:0x%x, io->buf[0][0].iova:0x%x, size:%d",
		   buf->daddr, io->buf[0][0].iova, io->buf[0][0].size);
	buf_printk("%s %dx%d @%d,%d-%dx%d\n",
		   node->desc.name,
		   io->fmt.s.w, io->fmt.s.h,
		   io->crop.p.x, io->crop.p.y, io->crop.s.w, io->crop.s.h);

	return 0;
}

static int mtk_cam_fill_img_buf(struct mtkcam_ipi_img_output *io,
				struct mtk_cam_buffer *buf)
{
	struct mtk_cam_cached_image_info *img_info = &buf->image_info;
	dma_addr_t daddr;
	int i;

	io->buf[0][0].ccd_fd = buf->vbb.vb2_buf.planes[0].m.fd;

	daddr = buf->daddr;
	for (i = 0; i < ARRAY_SIZE(img_info->bytesperline); i++) {
		unsigned int size = img_info->size[i];

		if (!size)
			break;

		io->buf[0][i].iova = daddr;
		io->buf[0][i].size = size;
		daddr += size;
	}

	return 0;
}
static int mtk_cam_fill_img_buf_subsample(struct mtkcam_ipi_img_output *io,
				struct mtk_cam_buffer *buf, int sub_ratio)
{
	struct mtk_cam_cached_image_info *img_info = &buf->image_info;
	dma_addr_t daddr;
	int i;
	int j;

	io->buf[0][0].ccd_fd = buf->vbb.vb2_buf.planes[0].m.fd;

	daddr = buf->daddr;
	for (j = 0; j < sub_ratio; j++) {
		for (i = 0; i < ARRAY_SIZE(img_info->bytesperline); i++) {
			unsigned int size = img_info->size[i];

			if (!size)
				break;

			io->buf[j][i].iova = daddr;
			io->buf[j][i].size = size;
			daddr += size;
		}
	}

	return 0;
}
int fill_yuvo_out_subsample(struct mtkcam_ipi_img_output *io,
			struct mtk_cam_buffer *buf,
			struct mtk_cam_video_device *node,
			int sub_ratio)
{
	/* uid */
	io->uid = node->uid;

	/* fmt */
	fill_img_fmt(&io->fmt, buf);

	mtk_cam_fill_img_buf_subsample(io, buf, sub_ratio);

	/* crop */
	io->crop = v4l2_rect_to_ipi_crop(&buf->image_info.crop);

	buf_printk("%s %dx%d @%d,%d-%dx%d\n",
		   node->desc.name,
		   io->fmt.s.w, io->fmt.s.h,
		   io->crop.p.x, io->crop.p.y, io->crop.s.w, io->crop.s.h);
	return 0;
}

int fill_img_out(struct mtkcam_ipi_img_output *io,
			struct mtk_cam_buffer *buf,
			struct mtk_cam_video_device *node)
{
	/* uid */
	io->uid = node->uid;

	/* fmt */
	fill_img_fmt(&io->fmt, buf);

	mtk_cam_fill_img_buf(io, buf);

	/* crop */
	io->crop = v4l2_rect_to_ipi_crop(&buf->image_info.crop);

	buf_printk("%s %dx%d @%d,%d-%dx%d\n",
		   node->desc.name,
		   io->fmt.s.w, io->fmt.s.h,
		   io->crop.p.x, io->crop.p.y, io->crop.s.w, io->crop.s.h);
	return 0;
}

