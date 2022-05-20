// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include "mtk_cam-fmt_utils.h"
#include "mtk_cam.h"
#include "mtk_cam-ipi_7_1.h"
#include "mtk_cam-job.h"
#include "mtk_cam-ufbc-def.h"
#include "mtk_cam-plat.h"

static int mtk_cam_job_fill_ipi_config(struct mtk_cam_job *job,
				       struct mtkcam_ipi_config_param *config);
static int mtk_cam_job_fill_ipi_frame(struct mtk_cam_job *job);

static int mtk_cam_job_pack_init(struct mtk_cam_job *job,
				 struct mtk_cam_ctx *ctx,
				 struct mtk_cam_request *req)
{
	struct device *dev = ctx->cam->dev;
	int ret;

	job->req = req;
	job->src_ctx = ctx;

	ret = mtk_cam_buffer_pool_fetch(&ctx->cq_pool, &job->cq);
	if (ret) {
		dev_info(dev, "ctx %d failed to fetch cq buffer\n",
			 ctx->stream_id);
		return ret;
	}

	ret = mtk_cam_buffer_pool_fetch(&ctx->ipi_pool, &job->ipi);
	if (ret) {
		dev_info(dev, "ctx %d failed to fetch ipi buffer\n",
			 ctx->stream_id);
		mtk_cam_buffer_pool_return(&job->cq);
		return ret;
	}

	return ret;
}

static int mtk_cam_select_hw(struct mtk_cam_ctx *ctx, struct mtk_cam_job *job)
{
	struct mtk_cam_device *cam = ctx->cam;
	int available, raw_available;
	int selected;
	int i;

	selected = 0;
	available = mtk_cam_get_available_engine(cam);
	raw_available = USED_MASK_GET_SUBMASK(&available, raw);

	/* todo: more rules */
	for (i = 0; i < cam->engines.num_raw_devices; i++)
		if (USED_MASK_HAS(&raw_available, raw, i)) {
			USED_MASK_SET(&selected, raw, i);
			break;
		}

	if (!selected) {
		dev_info(cam->dev, "select hw failed\n");
		return -1;
	}

	/* todo: where to release engine? */
	if (mtk_cam_occupy_engine(cam, selected))
		return -1;

	return selected;
}

int mtk_cam_job_pack(struct mtk_cam_job *job, struct mtk_cam_ctx *ctx,
		     struct mtk_cam_request *req)
{
	struct mtk_cam_job_data *data;
	int ret;

	ret = mtk_cam_job_pack_init(job, ctx, req);
	if (ret)
		return ret;

	data = mtk_cam_job_to_data(job);
	data->job_type = 0; /* TODO */

	if (!ctx->used_engine) {
		ctx->used_engine = mtk_cam_select_hw(ctx, job);
		if (!ctx->used_engine)
			return -1;
	}
	job->used_engine = ctx->used_engine;

	job->do_config = false;
	if (!ctx->configured) {
		/* if has raw */
		if (USED_MASK_GET_SUBMASK(&ctx->used_engine, raw)) {
			/* ipi_config_param */
			ret = mtk_cam_job_fill_ipi_config(job, &ctx->ipi_config);
			if (ret)
				return ret;
		}

		job->do_config = true;
		ctx->configured = true;
	}

	/* clone into job for debug dump */
	job->ipi_config = ctx->ipi_config;

	ret = mtk_cam_job_fill_ipi_frame(job);
	if (ret)
		return ret;

	return 0;
}

static void ipi_add_hw_map(struct mtkcam_ipi_config_param *config,
				   int pipe_id, int dev_mask)
{
	int n_maps = config->n_maps;

	WARN_ON(n_maps >= ARRAY_SIZE(config->maps));
	WARN_ON(!dev_mask);

	config->maps[n_maps] = (struct mtkcam_ipi_hw_mapping) {
		.pipe_id = pipe_id,
		.dev_mask = dev_mask,
		.exp_order = 0
	};
}

static inline struct mtkcam_ipi_crop
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

static int raw_set_ipi_input_param(struct mtkcam_ipi_input_param *input,
				   struct mtk_raw_sink_data *sink,
				   int pixel_mode, int dc_sv_pixel_mode,
				   int subsample)
{
	input->fmt = sensor_mbus_to_ipi_fmt(sink->mbus_code);
	input->raw_pixel_id = sensor_mbus_to_ipi_pixel_id(sink->mbus_code);
	input->data_pattern = MTKCAM_IPI_SENSOR_PATTERN_NORMAL;
	input->pixel_mode = pixel_mode;
	input->pixel_mode_before_raw = dc_sv_pixel_mode;
	input->subsample = subsample;
	input->in_crop = v4l2_rect_to_ipi_crop(&sink->crop);

	return 0;
}

static int get_raw_subdev_idx(int used_pipe)
{
	int used_raw = USED_MASK_GET_SUBMASK(&used_pipe, raw);
	int i;

	for (i = 0; used_raw; i++)
		if (USED_MASK_HAS(&used_raw, raw, i))
			return i;
	return -1;
}

static int mtk_cam_job_fill_ipi_config(struct mtk_cam_job *job,
				       struct mtkcam_ipi_config_param *config)
{
	struct mtk_cam_request *req = job->req;
	int used_engine = job->src_ctx->used_engine;
	struct mtkcam_ipi_input_param *input = &config->input;
	int raw_pipe_idx;

	memset(config, 0, sizeof(*config));

	/* assume: at most one raw-subdev is used */
	raw_pipe_idx = get_raw_subdev_idx(req->used_pipe);
	if (raw_pipe_idx != -1) {
		struct mtk_raw_sink_data *sink =
			&req->raw_data[raw_pipe_idx].sink;
		int raw_dev;

		config->flags = MTK_CAM_IPI_CONFIG_TYPE_INIT;
		config->sw_feature = MTKCAM_IPI_SW_FEATURE_NORMAL;

		raw_set_ipi_input_param(input, sink, 1, 0, 0); /* TODO */

		raw_dev = USED_MASK_GET_SUBMASK(&used_engine, raw);
		ipi_add_hw_map(config, MTKCAM_SUBDEV_RAW_0, raw_dev);
	}

	return 0;
}

static int update_job_cq_buffer_to_ipi_frame(struct mtk_cam_job *job,
					     struct mtkcam_ipi_frame_param *fp)
{
	struct mtk_cam_pool_buffer *cq = &job->cq;

	/* cq offset */
	fp->cur_workbuf_offset = cq->size * cq->priv.index;
	fp->cur_workbuf_size = cq->size;

	pr_info("mtk-cam cq buffer ofst %zu %zu\n",
		fp->cur_workbuf_offset, fp->cur_workbuf_size);
	return 0;
}

static int update_job_raw_param_to_ipi_frame(struct mtk_cam_job *job,
					     struct mtkcam_ipi_frame_param *fp)
{
	pr_info("%s %s [TODO]\n", __FILE__, __func__);
	return 0;
}

struct req_buffer_helper {
	struct mtk_cam_job *job;
	struct mtkcam_ipi_frame_param *fp;

	int ii_idx; /* image in */
	int io_idx; /* imgae out */
	int mi_idx; /* meta in */
	int mo_idx; /* meta out */
};

static inline
struct mtk_cam_video_device *dev_buf_to_vdev(struct mtk_cam_buffer *buf)
{
	WARN_ON(!buf->vbb.vb2_buf.vb2_queue);
	return mtk_cam_vbq_to_vdev(buf->vbb.vb2_buf.vb2_queue);
}

/* TODO: refine this function... */
static int mtk_cam_fill_img_buf(struct mtkcam_ipi_img_output *img_out,
				struct mtk_cam_buffer *buf)
{
	struct mtk_cam_cached_image_info *info = &buf->image_info;

	u32 pixelformat = info->v4l2_pixelformat;
	u32 width = info->width;
	u32 height = info->height;
	u32 stride = info->bytesperline[0];
	dma_addr_t daddr = buf->daddr;

	u32 aligned_width;
	unsigned int addr_offset = 0;
	int i;

	img_out->buf[0][0].ccd_fd = buf->vbb.vb2_buf.planes[0].m.fd;

	if (is_mtk_format(pixelformat)) {
		const struct mtk_format_info *info;

		info = mtk_format_info(pixelformat);
		if (!info)
			return -EINVAL;

		if (info->mem_planes != 1) {
			pr_info("do not support non-contiguous mplane\n");
			return -EINVAL;
		}

		aligned_width = stride / info->bpp[0];
		if (is_yuv_ufo(pixelformat)) {
			aligned_width = ALIGN(width, 64);
			img_out->buf[0][0].iova = daddr;
			img_out->fmt.stride[0] = aligned_width * info->bit_r_num
				/ info->bit_r_den;
			img_out->buf[0][0].size = img_out->fmt.stride[0] * height;
			img_out->buf[0][0].size += img_out->fmt.stride[0] * height / 2;
			img_out->buf[0][0].size += ALIGN((aligned_width / 64), 8) * height;
			img_out->buf[0][0].size += ALIGN((aligned_width / 64), 8) * height
				/ 2;
			img_out->buf[0][0].size += sizeof(struct UfbcBufferHeader);

			pr_debug("plane:%d stride:%d plane_size:%d addr:0x%x\n",
				 0, img_out->fmt.stride[0], img_out->buf[0][0].size,
				 img_out->buf[0][0].iova);
		} else if (is_raw_ufo(pixelformat)) {
			aligned_width = ALIGN(width, 64);
			img_out->buf[0][0].iova = daddr;
			img_out->fmt.stride[0] = aligned_width * info->bit_r_num /
				info->bit_r_den;
			img_out->buf[0][0].size = img_out->fmt.stride[0] * height;
			img_out->buf[0][0].size += ALIGN((aligned_width / 64), 8) * height;
			img_out->buf[0][0].size += sizeof(struct UfbcBufferHeader);

			pr_debug("plane:%d stride:%d plane_size:%d addr:0x%x\n",
				 0, img_out->fmt.stride[0], img_out->buf[0][0].size,
				 img_out->buf[0][0].iova);
		} else {
			for (i = 0; i < info->comp_planes; i++) {
				unsigned int hdiv = (i == 0) ? 1 : info->hdiv;
				unsigned int vdiv = (i == 0) ? 1 : info->vdiv;

				img_out->buf[0][i].iova = daddr + addr_offset;
				img_out->fmt.stride[i] = info->bpp[i] *
					DIV_ROUND_UP(aligned_width, hdiv);
				img_out->buf[0][i].size = img_out->fmt.stride[i]
					* DIV_ROUND_UP(height, vdiv);
				addr_offset += img_out->buf[0][i].size;

				pr_debug("plane:%d stride:%d plane_size:%d addr:0x%x\n",
					 i, img_out->fmt.stride[i], img_out->buf[0][i].size,
					 img_out->buf[0][i].iova);
			}
		}
	} else {
		const struct v4l2_format_info *info;

		info = v4l2_format_info(pixelformat);
		if (!info)
			return -EINVAL;

		if (info->mem_planes != 1) {
			pr_debug("do not support non contiguous mplane\n");
			return -EINVAL;
		}

		aligned_width = stride / info->bpp[0];
		for (i = 0; i < info->comp_planes; i++) {
			unsigned int hdiv = (i == 0) ? 1 : info->hdiv;
			unsigned int vdiv = (i == 0) ? 1 : info->vdiv;

			img_out->buf[0][i].iova = daddr + addr_offset;
			img_out->fmt.stride[i] = info->bpp[i] *
				DIV_ROUND_UP(aligned_width, hdiv);
			img_out->buf[0][i].size = img_out->fmt.stride[i]
				* DIV_ROUND_UP(height, vdiv);
			addr_offset += img_out->buf[0][i].size;

			pr_debug("stride:%d plane_size:%d addr:0x%x\n",
				 img_out->fmt.stride[i], img_out->buf[0][i].size,
				 img_out->buf[0][i].iova);
		}
	}

	return 0;
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


static int fill_img_out(struct mtkcam_ipi_img_output *io,
			struct mtk_cam_buffer *buf,
			struct mtk_cam_video_device *node)
{
	int ret;

	/* uid */
	io->uid = node->uid;

	/* fmt */
	ret = fill_img_fmt(&io->fmt, buf);

	/* buf */
	ret = ret || mtk_cam_fill_img_buf(io, buf);

	/* crop */
	io->crop = v4l2_rect_to_ipi_crop(&buf->image_info.crop);

	return ret;
}

static int update_raw_image_buf_to_ipi_frame(struct req_buffer_helper *helper,
					     struct mtk_cam_buffer *buf,
					     struct mtk_cam_video_device *node)
{
	struct mtkcam_ipi_frame_param *fp = helper->fp;
	int ret = -1;

	switch (node->desc.dma_port) {
	case MTKCAM_IPI_RAW_RAWI_2:
		/* TODO */
		pr_info("%s:%d not implemented yet\n", __func__, __LINE__);
		break;
	case MTKCAM_IPI_RAW_IMGO:
		// TODO: special case for vhdr
		//break;
	case MTKCAM_IPI_RAW_YUVO_1:
	case MTKCAM_IPI_RAW_YUVO_2:
	case MTKCAM_IPI_RAW_YUVO_3:
	case MTKCAM_IPI_RAW_YUVO_4:
	case MTKCAM_IPI_RAW_YUVO_5:
	case MTKCAM_IPI_RAW_RZH1N2TO_1:
	case MTKCAM_IPI_RAW_RZH1N2TO_2:
	case MTKCAM_IPI_RAW_RZH1N2TO_3:
	case MTKCAM_IPI_RAW_DRZS4NO_1:
	case MTKCAM_IPI_RAW_DRZS4NO_2:
	case MTKCAM_IPI_RAW_DRZS4NO_3:
		{
			struct mtkcam_ipi_img_output *out;

			out = &fp->img_outs[helper->io_idx];
			++helper->io_idx;

			ret = fill_img_out(out, buf, node);
		}
		break;
	default:
		pr_info("%s %s: not supported port: %d\n",
			__FILE__, __func__, node->desc.dma_port);
	}

	return ret;
}

#define FILL_META_IN_OUT(_ipi_meta, _cam_buf, _id)		\
{								\
	typeof(_ipi_meta) _m = (_ipi_meta);			\
	typeof(_cam_buf) _b = (_cam_buf);			\
								\
	_m->buf.ccd_fd = _b->vbb.vb2_buf.planes[0].m.fd;	\
	_m->buf.size = _b->meta_info.buffersize;		\
	_m->buf.iova = _b->daddr;				\
	_m->uid.id = _id;					\
}

static int update_raw_meta_buf_to_ipi_frame(struct req_buffer_helper *helper,
					    struct mtk_cam_buffer *buf,
					    struct mtk_cam_video_device *node)
{
	struct mtkcam_ipi_frame_param *fp = helper->fp;
	int ret = -1;

	switch (node->desc.dma_port) {
	case MTKCAM_IPI_RAW_META_STATS_CFG:
		{
			struct mtkcam_ipi_meta_input *in;

			in = &fp->meta_inputs[helper->mi_idx];
			++helper->mi_idx;

			FILL_META_IN_OUT(in, buf, node->desc.dma_port);
		}
		break;
	case MTKCAM_IPI_RAW_META_STATS_0:
	case MTKCAM_IPI_RAW_META_STATS_1:
	//case MTKCAM_IPI_RAW_META_STATS_2:
		{
			struct mtkcam_ipi_meta_output *out;

			out = &fp->meta_outputs[helper->mo_idx];
			++helper->mo_idx;

			FILL_META_IN_OUT(out, buf, node->desc.dma_port);

			ret = CALL_PLAT_V4L2(set_meta_stats_info,
					     node->desc.dma_port,
					     vb2_plane_vaddr(&buf->vbb.vb2_buf, 0));
		}
		/* TODO */
		//vaddr = vb2_plane_vaddr(vb, 0);
		//mtk_cam_set_meta_stats_info(dma_port, vaddr, pde_cfg);
		break;
	default:
		pr_info("%s %s: not supported port: %d\n",
			__FILE__, __func__, node->desc.dma_port);
	}

	return ret;
}

static bool belong_to_current_ctx(struct mtk_cam_job *job, int ipi_pipe_id)
{
	int ctx_used_pipe;
	int idx;
	bool ret = false;

	WARN_ON(!job->src_ctx);

	ctx_used_pipe = job->src_ctx->used_pipe;

	/* TODO: update for 7s */
	if (is_raw_subdev(ipi_pipe_id)) {
		idx = ipi_pipe_id;
		ret = USED_MASK_HAS(&ctx_used_pipe, raw, idx);
	} else if (is_camsv_subdev(ipi_pipe_id)) {
		idx = ipi_pipe_id - MTKCAM_SUBDEV_CAMSV_START;
		ret = USED_MASK_HAS(&ctx_used_pipe, camsv, idx);
	} else if (is_mraw_subdev(ipi_pipe_id)) {
		idx = ipi_pipe_id - MTKCAM_SUBDEV_MRAW_START;
		ret = USED_MASK_HAS(&ctx_used_pipe, mraw, idx);
	} else {
		WARN_ON(1);
	}

	return ret;
}

static int update_cam_buf_to_ipi_frame(struct req_buffer_helper *helper,
				       struct mtk_cam_buffer *buf)
{
	struct mtk_cam_video_device *node;
	int pipe_id;
	int ret = -1;

	node = dev_buf_to_vdev(buf);
	pipe_id = node->uid.pipe_id;

	/* skip if it does not belong to current ctx */
	if (!belong_to_current_ctx(helper->job, pipe_id))
		return 0;

	if (is_raw_subdev(pipe_id)) {
		if (node->desc.image)
			ret = update_raw_image_buf_to_ipi_frame(helper,
								buf, node);
		else
			ret = update_raw_meta_buf_to_ipi_frame(helper,
							       buf, node);
	}

	/* TODO: mraw/camsv */

	if (ret)
		pr_info("failed to update pipe %x buf %s\n",
			pipe_id, node->desc.name);

	return ret;
}

static void reset_unused_io_of_ipi_frame(struct req_buffer_helper *helper)
{
	struct mtkcam_ipi_frame_param *fp;
	int i;

	fp = helper->fp;

	for (i = helper->ii_idx; i < ARRAY_SIZE(fp->img_ins); i++) {
		struct mtkcam_ipi_img_input *io = &fp->img_ins[i];

		io->uid = (struct mtkcam_ipi_uid) {0, 0};
	}

	for (i = helper->io_idx; i < ARRAY_SIZE(fp->img_outs); i++) {
		struct mtkcam_ipi_img_output *io = &fp->img_outs[i];

		io->uid = (struct mtkcam_ipi_uid) {0, 0};
	}

	for (i = helper->mi_idx; i < ARRAY_SIZE(fp->meta_inputs); i++) {
		struct mtkcam_ipi_meta_input *io = &fp->meta_inputs[i];

		io->uid = (struct mtkcam_ipi_uid) {0, 0};
	}

	for (i = helper->mo_idx; i < ARRAY_SIZE(fp->meta_outputs); i++) {
		struct mtkcam_ipi_meta_output *io = &fp->meta_outputs[i];

		io->uid = (struct mtkcam_ipi_uid) {0, 0};
	}
}

static int update_job_buffer_to_ipi_frame(struct mtk_cam_job *job,
					  struct mtkcam_ipi_frame_param *fp)
{
	struct req_buffer_helper helper;
	struct mtk_cam_request *req = job->req;
	struct mtk_cam_buffer *buf;

	memset(&helper, 0, sizeof(helper));
	helper.job = job;
	helper.fp = fp;

	list_for_each_entry(buf, &req->buf_list, list) {
		update_cam_buf_to_ipi_frame(&helper, buf);
	}

	reset_unused_io_of_ipi_frame(&helper);

	return 0;
}

static int mtk_cam_job_fill_ipi_frame(struct mtk_cam_job *job)
{
	struct mtkcam_ipi_frame_param *fp;
	int ret;

	fp = (struct mtkcam_ipi_frame_param *)job->ipi.vaddr;

	ret = update_job_cq_buffer_to_ipi_frame(job, fp)
		|| update_job_raw_param_to_ipi_frame(job, fp)
		|| update_job_buffer_to_ipi_frame(job, fp);

	return ret;
}
