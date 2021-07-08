// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 *
 * Author: Fish Wu <fish.wu@mediatek.com>
 *
 */

#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/remoteproc.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-mem2mem.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-dma-contig.h>
#include <media/videobuf2-v4l2.h>

#include <soc/mediatek/smi.h>

#include "mtk_aie.h"

#define AIE_QOS_MAX 4
#define AIE_QOS_RA_IDX 0
#define AIE_QOS_RB_IDX 1
#define AIE_QOS_WA_IDX 2
#define AIE_QOS_WB_IDX 3
#define AIE_READ_AVG_BW 213
#define AIE_WRITE_AVG_BW 145
#define CHECK_SERVICE_0 0

#if CHECK_SERVICE_0 //Remove CID
#define V4L2_CID_MTK_AIE_INIT (V4L2_CID_USER_MTK_FD_BASE + 1)
#define V4L2_CID_MTK_AIE_PARAM (V4L2_CID_USER_MTK_FD_BASE + 2)
#define V4L2_CID_MTK_AIE_FD_VER (V4L2_CID_USER_MTK_FD_BASE + 3)
#define V4L2_CID_MTK_AIE_ATTR_VER (V4L2_CID_USER_MTK_FD_BASE + 4)

#define V4L2_CID_MTK_AIE_MAX 4
#endif

struct mtk_aie_user_para g_user_param;

static const struct v4l2_pix_format_mplane mtk_aie_img_fmts[] = {
	{
		.pixelformat = V4L2_PIX_FMT_NV16M, .num_planes = 2,
	},
	{
		.pixelformat = V4L2_PIX_FMT_NV61M, .num_planes = 2,
	},
	{
		.pixelformat = V4L2_PIX_FMT_YUYV, .num_planes = 1,
	},
	{
		.pixelformat = V4L2_PIX_FMT_YVYU, .num_planes = 1,
	},
	{
		.pixelformat = V4L2_PIX_FMT_UYVY, .num_planes = 1,
	},
	{
		.pixelformat = V4L2_PIX_FMT_VYUY, .num_planes = 1,
	},
	{
		.pixelformat = V4L2_PIX_FMT_GREY, .num_planes = 1,
	},
	{
		.pixelformat = V4L2_PIX_FMT_NV12M, .num_planes = 2,
	},
	{
		.pixelformat = V4L2_PIX_FMT_NV12, .num_planes = 1,
	},
};

static struct mtk_aie_qos_path aie_qos_path[AIE_QOS_MAX] = {
	{NULL, "l12_fdvt_rda", 0},
	{NULL, "l12_fdvt_rdb", 0},
	{NULL, "l12_fdvt_wra", 0},
	{NULL, "l12_fdvt_wrb", 0}
};

#define NUM_FORMATS ARRAY_SIZE(mtk_aie_img_fmts)

static inline struct mtk_aie_ctx *fh_to_ctx(struct v4l2_fh *fh)
{
	return container_of(fh, struct mtk_aie_ctx, fh);
}

static void mtk_aie_mmdvfs_init(struct mtk_aie_dev *fd)
{
	struct mtk_aie_dvfs *dvfs_info = &fd->dvfs_info;
	u64 freq = 0;
	int ret = 0, opp_num = 0, opp_idx = 0, idx = 0, volt;
	struct device_node *np, *child_np = NULL;
	struct of_phandle_iterator it;

	memset((void *)dvfs_info, 0x0, sizeof(struct mtk_aie_dvfs));
	dvfs_info->dev = fd->dev;
	ret = dev_pm_opp_of_add_table(dvfs_info->dev);
	if (ret < 0) {
		dev_info(dvfs_info->dev, "fail to init opp table: %d\n", ret);
		return;
	}
	dvfs_info->reg = devm_regulator_get(dvfs_info->dev, "dvfsrc-vcore");
	if (IS_ERR(dvfs_info->reg)) {
		dev_info(dvfs_info->dev, "can't get dvfsrc-vcore\n");
		return;
	}

	opp_num = regulator_count_voltages(dvfs_info->reg);
	of_for_each_phandle(
		&it, ret, dvfs_info->dev->of_node, "operating-points-v2", NULL, 0) {
		np = of_node_get(it.node);
		if (!np) {
			dev_info(dvfs_info->dev, "of_node_get fail\n");
			return;
		}

		do {
			child_np = of_get_next_available_child(np, child_np);
			if (child_np) {
				of_property_read_u64(child_np, "opp-hz", &freq);
				dvfs_info->clklv[opp_idx][idx] = freq;
				of_property_read_u32(child_np, "opp-microvolt", &volt);
				dvfs_info->voltlv[opp_idx][idx] = volt;
				idx++;
			}
		} while (child_np);
		dvfs_info->clklv_num[opp_idx] = idx;
		dvfs_info->clklv_target[opp_idx] = dvfs_info->clklv[opp_idx][0];
		dvfs_info->clklv_idx[opp_idx] = 0;
		idx = 0;
		opp_idx++;
		of_node_put(np);
	}

	opp_num = opp_idx;
	for (opp_idx = 0; opp_idx < opp_num; opp_idx++) {
		for (idx = 0; idx < dvfs_info->clklv_num[opp_idx]; idx++) {
			dev_dbg(dvfs_info->dev, "[%s] opp=%d, idx=%d, clk=%d volt=%d\n",
				__func__, opp_idx, idx, dvfs_info->clklv[opp_idx][idx],
				dvfs_info->voltlv[opp_idx][idx]);
		}
	}
	dvfs_info->cur_volt = 0;

}

static void mtk_aie_mmdvfs_uninit(struct mtk_aie_dev *fd)
{
	struct mtk_aie_dvfs *dvfs_info = &fd->dvfs_info;
	int volt = 0, ret = 0;

	dev_dbg(dvfs_info->dev, "[%s]\n", __func__);

	dvfs_info->cur_volt = volt;

	ret = regulator_set_voltage(dvfs_info->reg, volt, INT_MAX);

}

static void mtk_aie_mmdvfs_set(struct mtk_aie_dev *fd,
				bool isSet, unsigned int level)
{
	struct mtk_aie_dvfs *dvfs_info = &fd->dvfs_info;
	int volt = 0, ret = 0, idx = 0, opp_idx = 0;

	if (isSet) {
		if (level < dvfs_info->clklv_num[opp_idx])
			idx = level;
	}
	volt = dvfs_info->voltlv[opp_idx][idx];

	if (dvfs_info->cur_volt != volt) {
		dev_dbg(dvfs_info->dev, "[%s] volt change opp=%d, idx=%d, clk=%d volt=%d\n",
			__func__, opp_idx, idx, dvfs_info->clklv[opp_idx][idx],
			dvfs_info->voltlv[opp_idx][idx]);
		ret = regulator_set_voltage(dvfs_info->reg, volt, INT_MAX);
		dvfs_info->cur_volt = volt;
	}
}

static void mtk_aie_mmqos_init(struct mtk_aie_dev *fd)
{
	struct mtk_aie_qos *qos_info = &fd->qos_info;
	//struct icc_path *path;
	int idx = 0;

	memset((void *)qos_info, 0x0, sizeof(struct mtk_aie_qos));
	qos_info->dev = fd->dev;
	qos_info->qos_path = aie_qos_path;

	for (idx = 0; idx < AIE_QOS_MAX; idx++) {
		qos_info->qos_path[idx].path =
			of_mtk_icc_get(qos_info->dev, qos_info->qos_path[idx].dts_name);
		qos_info->qos_path[idx].bw = 0;
		dev_dbg(qos_info->dev, "[%s] idx=%d, path=%p, name=%s, bw=%d\n",
			__func__, idx,
			qos_info->qos_path[idx].path,
			qos_info->qos_path[idx].dts_name,
			qos_info->qos_path[idx].bw);
	}
}

static void mtk_aie_mmqos_uninit(struct mtk_aie_dev *fd)
{
	struct mtk_aie_qos *qos_info = &fd->qos_info;
	int idx = 0;

	for (idx = 0; idx < AIE_QOS_MAX; idx++) {
		if (qos_info->qos_path[idx].path == NULL) {
			dev_dbg(qos_info->dev, "[%s] path of idx(%d) is NULL\n", __func__, idx);
			continue;
		}
		dev_dbg(qos_info->dev, "[%s] idx=%d, path=%p, bw=%d\n",
			__func__, idx,
			qos_info->qos_path[idx].path,
			qos_info->qos_path[idx].bw);
		qos_info->qos_path[idx].bw = 0;
		mtk_icc_set_bw(qos_info->qos_path[idx].path, 0, 0);
	}
}

static void mtk_aie_mmqos_set(struct mtk_aie_dev *fd,
				bool isSet)
{
	struct mtk_aie_qos *qos_info = &fd->qos_info;
	int r_bw = 0;
	int w_bw = 0;
	int idx = 0;

	if (isSet) {
		r_bw = AIE_READ_AVG_BW;
		w_bw = AIE_WRITE_AVG_BW;
	}

	for (idx = 0; idx < AIE_QOS_MAX; idx++) {
		if (qos_info->qos_path[idx].path == NULL) {
			dev_dbg(qos_info->dev, "[%s] path of idx(%d) is NULL\n",
				 __func__, idx);
			continue;
		}

		if (idx == AIE_QOS_RA_IDX &&
		    qos_info->qos_path[idx].bw != r_bw) {
			dev_dbg(qos_info->dev, "[%s] idx=%d, path=%p, bw=%d/%d,\n",
				__func__, idx,
				qos_info->qos_path[idx].path,
				qos_info->qos_path[idx].bw, r_bw);
			qos_info->qos_path[idx].bw = r_bw;
			mtk_icc_set_bw(qos_info->qos_path[idx].path,
				       MBps_to_icc(qos_info->qos_path[idx].bw), 0);
		}

		if (idx == AIE_QOS_WA_IDX &&
		    qos_info->qos_path[idx].bw != w_bw) {
			dev_dbg(qos_info->dev, "[%s] idx=%d, path=%p, bw=%d/%d,\n",
				__func__, idx,
				qos_info->qos_path[idx].path,
				qos_info->qos_path[idx].bw, w_bw);
			qos_info->qos_path[idx].bw = w_bw;
			mtk_icc_set_bw(qos_info->qos_path[idx].path,
				       MBps_to_icc(qos_info->qos_path[idx].bw), 0);
		}
	}
}
#if CHECK_SERVICE_0
static void mtk_aie_fill_init_param(struct mtk_aie_dev *fd,
				    struct user_init *user_init,
				    struct v4l2_ctrl_handler *hdl)
{
	struct v4l2_ctrl *ctrl;

	ctrl = v4l2_ctrl_find(hdl, V4L2_CID_MTK_AIE_INIT);
	if (ctrl) {
		memcpy(user_init, ctrl->p_new.p_u32, sizeof(struct user_init));
		dev_dbg(fd->dev, "init param : max w:%d, max h:%d",
			user_init->max_img_width, user_init->max_img_height);
		dev_dbg(fd->dev, "init param : p_w:%d, p_h:%d, f thread:%d",
			user_init->pyramid_width, user_init->pyramid_height,
			user_init->feature_thread);
	}
}
#endif
static int mtk_aie_hw_enable(struct mtk_aie_dev *fd)
{
	//struct mtk_aie_ctx *ctx = fd->ctx;
	//struct user_init user_init;
	struct aie_init_info init_info;

	/* initial value */
	//mtk_aie_fill_init_param(fd, &user_init, &ctx->hdl);
	init_info.max_img_width = 960; //tina log: user_init.max_img_width
	init_info.max_img_height = 960; //tina log: user_init.max_img_height
	init_info.is_secure = 0;
	init_info.pyramid_height = 504;//tina log: user_init.pyramid_height;
	init_info.pyramid_width = 640;//tina log: user_init.pyramid_width;

	return aie_init(fd, init_info);
}

static void mtk_aie_hw_job_finish(struct mtk_aie_dev *fd,
				  enum vb2_buffer_state vb_state)
{
	struct mtk_aie_ctx *ctx;
	struct vb2_v4l2_buffer *src_vbuf = NULL, *dst_vbuf = NULL;

	pm_runtime_put(fd->dev);

	ctx = v4l2_m2m_get_curr_priv(fd->m2m_dev);
	src_vbuf = v4l2_m2m_src_buf_remove(ctx->fh.m2m_ctx);
	dst_vbuf = v4l2_m2m_dst_buf_remove(ctx->fh.m2m_ctx);

	v4l2_m2m_buf_copy_metadata(src_vbuf, dst_vbuf,
				   V4L2_BUF_FLAG_TSTAMP_SRC_MASK);
	v4l2_m2m_buf_done(src_vbuf, vb_state);
	v4l2_m2m_buf_done(dst_vbuf, vb_state);
	v4l2_m2m_job_finish(fd->m2m_dev, ctx->fh.m2m_ctx);
	complete_all(&fd->fd_job_finished);
}

static void mtk_aie_hw_done(struct mtk_aie_dev *fd,
			    enum vb2_buffer_state vb_state)
{
	if (!cancel_delayed_work(&fd->job_timeout_work))
		return;

	atomic_dec(&fd->num_composing);
	mtk_aie_hw_job_finish(fd, vb_state);
	wake_up(&fd->flushing_waitq);
}

static int mtk_aie_hw_connect(struct mtk_aie_dev *fd)
{
	fd->fd_stream_count++;
	fd->map_count = 0;
	if (fd->fd_stream_count == 1) {
		if (mtk_aie_hw_enable(fd))
			return -EINVAL;
		mtk_aie_mmdvfs_set(fd, 1, 0);
		mtk_aie_mmqos_set(fd, 1);
	}
	return 0;
}

static void mtk_aie_hw_disconnect(struct mtk_aie_dev *fd)
{
	fd->fd_stream_count--;
	if (fd->fd_stream_count == 0) {
		mtk_aie_mmqos_set(fd, 0);
		mtk_aie_mmdvfs_set(fd, 0, 0);
		aie_uninit(fd);
	}
}

static int mtk_aie_hw_job_exec(struct mtk_aie_dev *fd,
			       struct fd_enq_param *fd_param)
{
	pm_runtime_get_sync((fd->dev));

	reinit_completion(&fd->fd_job_finished);
	schedule_delayed_work(&fd->job_timeout_work,
			      msecs_to_jiffies(MTK_FD_HW_TIMEOUT));

	return 0;
}

static int mtk_aie_vb2_buf_out_validate(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *v4l2_buf = to_vb2_v4l2_buffer(vb);

	if (v4l2_buf->field == V4L2_FIELD_ANY)
		v4l2_buf->field = V4L2_FIELD_NONE;
	if (v4l2_buf->field != V4L2_FIELD_NONE)
		return -EINVAL;

	return 0;
}

static int mtk_aie_vb2_buf_prepare(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct vb2_queue *vq = vb->vb2_queue;
	struct mtk_aie_ctx *ctx = vb2_get_drv_priv(vq);
	struct device *dev = ctx->dev;
	struct v4l2_pix_format_mplane *pixfmt;

	switch (vq->type) {
	case V4L2_BUF_TYPE_META_CAPTURE:
		if (vb2_plane_size(vb, 0) < ctx->dst_fmt.buffersize) {
			dev_info(dev, "meta size %lu is too small\n",
				 vb2_plane_size(vb, 0));
			return -EINVAL;
		}
		break;
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		pixfmt = &ctx->src_fmt;

		if (vbuf->field == V4L2_FIELD_ANY)
			vbuf->field = V4L2_FIELD_NONE;

		if (vb->num_planes > 2 || vbuf->field != V4L2_FIELD_NONE) {
			dev_info(dev, "plane %d or field %d not supported\n",
				 vb->num_planes, vbuf->field);
			return -EINVAL;
		}

		if (vb2_plane_size(vb, 0) < pixfmt->plane_fmt[0].sizeimage) {
			dev_info(dev, "plane 0 %lu is too small than %x\n",
				 vb2_plane_size(vb, 0),
				 pixfmt->plane_fmt[0].sizeimage);
			return -EINVAL;
		}

		if (pixfmt->num_planes == 2 &&
		    vb2_plane_size(vb, 1) < pixfmt->plane_fmt[1].sizeimage) {
			dev_info(dev, "plane 1 %lu is too small than %x\n",
				 vb2_plane_size(vb, 1),
				 pixfmt->plane_fmt[1].sizeimage);
			return -EINVAL;
		}
		break;
	}

	return 0;
}

static void mtk_aie_vb2_buf_queue(struct vb2_buffer *vb)
{
	struct mtk_aie_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);

	v4l2_m2m_buf_queue(ctx->fh.m2m_ctx, vbuf);
}

static int mtk_aie_vb2_queue_setup(struct vb2_queue *vq,
				   unsigned int *num_buffers,
				   unsigned int *num_planes,
				   unsigned int sizes[],
				   struct device *alloc_devs[])
{
	struct mtk_aie_ctx *ctx = vb2_get_drv_priv(vq);
	unsigned int size[2];
	unsigned int plane;

	switch (vq->type) {
	case V4L2_BUF_TYPE_META_CAPTURE:
		size[0] = ctx->dst_fmt.buffersize;
		break;
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		size[0] = ctx->src_fmt.plane_fmt[0].sizeimage;
		size[1] = ctx->src_fmt.plane_fmt[1].sizeimage;
		break;
	}

	if (*num_planes > 2)
		return -EINVAL;
	if (*num_planes == 0) {
		if (vq->type == V4L2_BUF_TYPE_META_CAPTURE) {
			sizes[0] = ctx->dst_fmt.buffersize;
			*num_planes = 1;
			return 0;
		}

		*num_planes = ctx->src_fmt.num_planes;
		if (*num_planes > 2)
			return -EINVAL;
		for (plane = 0; plane < *num_planes; plane++)
			sizes[plane] = ctx->src_fmt.plane_fmt[plane].sizeimage;

		return 0;
	}

	return 0;
}

static int mtk_aie_vb2_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct mtk_aie_ctx *ctx = vb2_get_drv_priv(vq);

	if (vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		return mtk_aie_hw_connect(ctx->fd_dev);

	return 0;
}

static void mtk_aie_job_timeout_work(struct work_struct *work)
{
	struct mtk_aie_dev *fd =
		container_of(work, struct mtk_aie_dev, job_timeout_work.work);

	dev_info(fd->dev, "FD Job timeout!");

	dev_info(fd->dev, "AIE mode:%d fmt:%d w:%d h:%d s:%d pw%d ph%d np%d dg%d",
		 fd->aie_cfg->sel_mode,
		 fd->aie_cfg->src_img_fmt,
		 fd->aie_cfg->src_img_width,
		 fd->aie_cfg->src_img_height,
		 fd->aie_cfg->src_img_stride,
		 fd->aie_cfg->pyramid_base_width,
		 fd->aie_cfg->pyramid_base_height,
		 fd->aie_cfg->number_of_pyramid,
		 fd->aie_cfg->rotate_degree);
	dev_info(fd->dev, "roi%d x1:%d y1:%d x2:%d y2:%d pad%d l%d r%d d%d u%d f%d",
		 fd->aie_cfg->en_roi,
		 fd->aie_cfg->src_roi.x1,
		 fd->aie_cfg->src_roi.y1,
		 fd->aie_cfg->src_roi.x2,
		 fd->aie_cfg->src_roi.y2,
		 fd->aie_cfg->en_padding,
		 fd->aie_cfg->src_padding.left,
		 fd->aie_cfg->src_padding.right,
		 fd->aie_cfg->src_padding.down,
		 fd->aie_cfg->src_padding.up,
		 fd->aie_cfg->freq_level);

	dev_info(fd->dev, "%s result result1: %x, %x, %x", __func__,
		 readl(fd->fd_base + AIE_RESULT_0_REG),
		 readl(fd->fd_base + AIE_RESULT_1_REG),
		 readl(fd->fd_base + AIE_DMA_CTL_REG));

	dev_info(fd->dev, "%s interrupt status: %x", __func__,
		 readl(fd->fd_base + AIE_INT_EN_REG));

	if (fd->aie_cfg->sel_mode == 1)
		dev_info(fd->dev, "[ATTRMODE] w_idx = %d, r_idx = %d\n",
			 fd->attr_para->w_idx, fd->attr_para->r_idx);

	aie_irqhandle(fd);
	aie_reset(fd);
	atomic_dec(&fd->num_composing);
	mtk_aie_hw_job_finish(fd, VB2_BUF_STATE_ERROR);
	wake_up(&fd->flushing_waitq);
}

static void mtk_aie_job_wait_finish(struct mtk_aie_dev *fd)
{
	wait_for_completion(&fd->fd_job_finished);
}

static void mtk_aie_vb2_stop_streaming(struct vb2_queue *vq)
{
	struct mtk_aie_ctx *ctx = vb2_get_drv_priv(vq);
	struct mtk_aie_dev *fd = ctx->fd_dev;
	struct vb2_v4l2_buffer *vb;
	struct v4l2_m2m_ctx *m2m_ctx = ctx->fh.m2m_ctx;
	struct v4l2_m2m_queue_ctx *queue_ctx;

	mtk_aie_job_wait_finish(fd);
	queue_ctx = V4L2_TYPE_IS_OUTPUT(vq->type) ? &m2m_ctx->out_q_ctx
						  : &m2m_ctx->cap_q_ctx;
	while ((vb = v4l2_m2m_buf_remove(queue_ctx)))
		v4l2_m2m_buf_done(vb, VB2_BUF_STATE_ERROR);

	dma_buf_kunmap(fd->dmabuf, 0, (void *)fd->kva);
	dma_buf_end_cpu_access(fd->dmabuf, DMA_BIDIRECTIONAL);
	fd->map_count--;

	if (vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		mtk_aie_hw_disconnect(fd);
}

static void mtk_aie_vb2_request_complete(struct vb2_buffer *vb)
{
	struct mtk_aie_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);

	v4l2_ctrl_request_complete(vb->req_obj.req, &ctx->hdl);
}

static int mtk_aie_querycap(struct file *file, void *fh,
			    struct v4l2_capability *cap)
{
	struct mtk_aie_dev *fd = video_drvdata(file);
	struct device *dev = fd->dev;

	strscpy(cap->driver, dev_driver_string(dev), sizeof(cap->driver));
	strscpy(cap->card, dev_driver_string(dev), sizeof(cap->card));
	snprintf(cap->bus_info, sizeof(cap->bus_info), "platform:%s",
		 dev_name(fd->dev));

	return 0;
}

static int mtk_aie_enum_fmt_out_mp(struct file *file, void *fh,
				   struct v4l2_fmtdesc *f)
{
	if (f->index >= NUM_FORMATS)
		return -EINVAL;

	f->pixelformat = mtk_aie_img_fmts[f->index].pixelformat;
	return 0;
}

static void mtk_aie_fill_pixfmt_mp(struct v4l2_pix_format_mplane *dfmt,
				   const struct v4l2_pix_format_mplane *sfmt)
{
	dfmt->field = V4L2_FIELD_NONE;
	dfmt->colorspace = V4L2_COLORSPACE_BT2020;
	dfmt->num_planes = sfmt->num_planes;
	dfmt->ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
	dfmt->quantization = V4L2_QUANTIZATION_DEFAULT;
	dfmt->xfer_func = V4L2_MAP_XFER_FUNC_DEFAULT(dfmt->colorspace);

	/* Keep user setting as possible */
	dfmt->width = clamp(dfmt->width, MTK_FD_OUTPUT_MIN_WIDTH,
			    MTK_FD_OUTPUT_MAX_WIDTH);
	dfmt->height = clamp(dfmt->height, MTK_FD_OUTPUT_MIN_HEIGHT,
			     MTK_FD_OUTPUT_MAX_HEIGHT);

	if (sfmt->num_planes == 2) {
		dfmt->plane_fmt[0].sizeimage =
			dfmt->height * dfmt->plane_fmt[0].bytesperline;
		dfmt->plane_fmt[1].sizeimage =
			dfmt->height * dfmt->plane_fmt[1].bytesperline;
		if (sfmt->pixelformat == V4L2_PIX_FMT_NV12M)
			dfmt->plane_fmt[1].sizeimage =
				dfmt->height * dfmt->plane_fmt[1].bytesperline /
				2;
	} else {
		dfmt->plane_fmt[0].sizeimage =
			dfmt->height * dfmt->plane_fmt[0].bytesperline;
		if (sfmt->pixelformat == V4L2_PIX_FMT_NV12)
			dfmt->plane_fmt[0].sizeimage =
				dfmt->height * dfmt->plane_fmt[0].bytesperline *
				3 / 2;
	}
}

static const struct v4l2_pix_format_mplane *mtk_aie_find_fmt(u32 format)
{
	unsigned int i;

	for (i = 0; i < NUM_FORMATS; i++) {
		if (mtk_aie_img_fmts[i].pixelformat == format)
			return &mtk_aie_img_fmts[i];
	}

	return NULL;
}

static int mtk_aie_try_fmt_out_mp(struct file *file, void *fh,
				  struct v4l2_format *f)
{
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;
	const struct v4l2_pix_format_mplane *fmt;

	fmt = mtk_aie_find_fmt(pix_mp->pixelformat);
	if (!fmt)
		fmt = &mtk_aie_img_fmts[0]; /* Get default img fmt */

	mtk_aie_fill_pixfmt_mp(pix_mp, fmt);
	return 0;
}

static int mtk_aie_g_fmt_out_mp(struct file *file, void *fh,
				struct v4l2_format *f)
{
	struct mtk_aie_ctx *ctx = fh_to_ctx(fh);

	f->fmt.pix_mp = ctx->src_fmt;

	return 0;
}

static int mtk_aie_s_fmt_out_mp(struct file *file, void *fh,
				struct v4l2_format *f)
{
	struct mtk_aie_ctx *ctx = fh_to_ctx(fh);
	struct vb2_queue *vq = v4l2_m2m_get_vq(ctx->fh.m2m_ctx, f->type);

	/* Change not allowed if queue is streaming. */
	if (vb2_is_streaming(vq)) {
		dev_info(ctx->dev, "Failed to set format, vb2 is busy\n");
		return -EBUSY;
	}

	mtk_aie_try_fmt_out_mp(file, fh, f);
	ctx->src_fmt = f->fmt.pix_mp;

	return 0;
}

static int mtk_aie_enum_fmt_meta_cap(struct file *file, void *fh,
				     struct v4l2_fmtdesc *f)
{
	if (f->index)
		return -EINVAL;

	strscpy(f->description, "Face detection result",
		sizeof(f->description));

	f->pixelformat = V4L2_META_FMT_MTFD_RESULT;
	f->flags = 0;

	return 0;
}

static int mtk_aie_g_fmt_meta_cap(struct file *file, void *fh,
				  struct v4l2_format *f)
{
	f->fmt.meta.dataformat = V4L2_META_FMT_MTFD_RESULT;
	f->fmt.meta.buffersize = sizeof(struct aie_enq_info);

	return 0;
}

int mtk_aie_vidioc_qbuf(struct file *file, void *priv,
				  struct v4l2_buffer *buf)
{
	struct mtk_aie_dev *fd = video_drvdata(file);

	if (buf->length) {
		if (!fd->map_count) {
			fd->dmabuf = dma_buf_get(buf->m.planes[buf->length-1].m.fd);
			dma_buf_begin_cpu_access(fd->dmabuf, DMA_BIDIRECTIONAL);
			fd->kva = (u64)dma_buf_kmap(fd->dmabuf, 0);
			memcpy((char *)&g_user_param, (char *)fd->kva, sizeof(g_user_param));
			fd->base_para->rpn_anchor_thrd = (signed short)
						 (g_user_param.feature_threshold & 0x0000FFFF);
			fd->map_count++;
		} else {
			memcpy((char *)&g_user_param, (char *)fd->kva, sizeof(g_user_param));
#ifdef FLD
			dev_info(fd->dev, "%s face_num: %x", __func__,
					g_user_param.user_param.fld_face_num);
			dev_info(fd->dev, "%s face_num: %x", __func__,
					g_user_param.user_param.fld_input[0].fld_in_crop.x1);
			dev_info(fd->dev, "%s face_num: %x", __func__,
					g_user_param.user_param.fld_input[0].fld_in_crop.y1);
			dev_info(fd->dev, "%s face_num: %x", __func__,
					g_user_param.user_param.fld_input[0].fld_in_crop.x2);
			dev_info(fd->dev, "%s face_num: %x", __func__,
					g_user_param.user_param.fld_input[0].fld_in_crop.y2);
			dev_info(fd->dev, "%s face_num: %x", __func__,
					g_user_param.user_param.fld_input[0].fld_in_rip);
			dev_info(fd->dev, "%s face_num: %x", __func__,
					g_user_param.user_param.fld_input[0].fld_in_rop);
#endif
		}
	}

	return v4l2_m2m_ioctl_qbuf(file, priv, buf);
}

static const struct vb2_ops mtk_aie_vb2_ops = {
	.queue_setup = mtk_aie_vb2_queue_setup,
	.buf_out_validate = mtk_aie_vb2_buf_out_validate,
	.buf_prepare = mtk_aie_vb2_buf_prepare,
	.buf_queue = mtk_aie_vb2_buf_queue,
	.start_streaming = mtk_aie_vb2_start_streaming,
	.stop_streaming = mtk_aie_vb2_stop_streaming,
	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,
	.buf_request_complete = mtk_aie_vb2_request_complete,
};

static const struct v4l2_ioctl_ops mtk_aie_v4l2_video_out_ioctl_ops = {
	.vidioc_querycap = mtk_aie_querycap,
	.vidioc_enum_fmt_vid_out = mtk_aie_enum_fmt_out_mp,
	.vidioc_g_fmt_vid_out_mplane = mtk_aie_g_fmt_out_mp,
	.vidioc_s_fmt_vid_out_mplane = mtk_aie_s_fmt_out_mp,
	.vidioc_try_fmt_vid_out_mplane = mtk_aie_try_fmt_out_mp,
	.vidioc_enum_fmt_meta_cap = mtk_aie_enum_fmt_meta_cap,
	.vidioc_g_fmt_meta_cap = mtk_aie_g_fmt_meta_cap,
	.vidioc_s_fmt_meta_cap = mtk_aie_g_fmt_meta_cap,
	.vidioc_try_fmt_meta_cap = mtk_aie_g_fmt_meta_cap,
	.vidioc_reqbufs = v4l2_m2m_ioctl_reqbufs,
	.vidioc_create_bufs = v4l2_m2m_ioctl_create_bufs,
	.vidioc_expbuf = v4l2_m2m_ioctl_expbuf,
	.vidioc_prepare_buf = v4l2_m2m_ioctl_prepare_buf,
	.vidioc_querybuf = v4l2_m2m_ioctl_querybuf,
	.vidioc_qbuf = mtk_aie_vidioc_qbuf,
	.vidioc_dqbuf = v4l2_m2m_ioctl_dqbuf,
	.vidioc_streamon = v4l2_m2m_ioctl_streamon,
	.vidioc_streamoff = v4l2_m2m_ioctl_streamoff,
	.vidioc_subscribe_event = v4l2_ctrl_subscribe_event,
	.vidioc_unsubscribe_event = v4l2_event_unsubscribe,
};

static int mtk_aie_queue_init(void *priv, struct vb2_queue *src_vq,
			      struct vb2_queue *dst_vq)
{
	struct mtk_aie_ctx *ctx = priv;
	int ret;

	src_vq->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	src_vq->io_modes = VB2_MMAP | VB2_DMABUF;
	src_vq->supports_requests = true;
	src_vq->drv_priv = ctx;
	src_vq->ops = &mtk_aie_vb2_ops;
	src_vq->mem_ops = &vb2_dma_contig_memops;
	src_vq->buf_struct_size = sizeof(struct v4l2_m2m_buffer);
	src_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	src_vq->lock = &ctx->fd_dev->vfd_lock;
	src_vq->dev = ctx->fd_dev->v4l2_dev.dev;

	ret = vb2_queue_init(src_vq);
	if (ret)
		return ret;

	dst_vq->type = V4L2_BUF_TYPE_META_CAPTURE;
	dst_vq->io_modes = VB2_MMAP | VB2_DMABUF;
	dst_vq->drv_priv = ctx;
	dst_vq->ops = &mtk_aie_vb2_ops;
	dst_vq->mem_ops = &vb2_dma_contig_memops;
	dst_vq->buf_struct_size = sizeof(struct v4l2_m2m_buffer);
	dst_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	dst_vq->lock = &ctx->fd_dev->vfd_lock;
	dst_vq->dev = ctx->fd_dev->v4l2_dev.dev;

	return vb2_queue_init(dst_vq);
}
#if CHECK_SERVICE_0 //Remove CID
static struct v4l2_ctrl_config mtk_aie_controls[] = {
	{
		.id = V4L2_CID_MTK_AIE_INIT,
		.name = "FD detection init",
		.type = V4L2_CTRL_TYPE_U32,
		.min = 0,
		.max = 0xffffffff,
		.step = 1,
		.def = 0,
		.dims = {sizeof(struct user_init) / 4},
	},
	{
		.id = V4L2_CID_MTK_AIE_PARAM,
		.name = "FD detection param",
		.type = V4L2_CTRL_TYPE_U32,
		.min = 0,
		.max = 0xffffffff,
		.step = 1,
		.def = 0,
		.dims = {sizeof(struct user_param) / 4},
	},
	{
		.id = V4L2_CID_MTK_AIE_FD_VER,
		.name = "FD detection fd ver",
		.type = V4L2_CTRL_TYPE_U32,
		.min = 0,
		.max = 0xffffffff,
		.step = 1,
		.def = FD_VERSION,
		.dims = {1},
	},
	{
		.id = V4L2_CID_MTK_AIE_ATTR_VER,
		.name = "FD detection attr ver",
		.type = V4L2_CTRL_TYPE_U32,
		.min = 0,
		.max = 0xffffffff,
		.step = 1,
		.def = ATTR_VERSION,
		.dims = {1},
	},
};

static int mtk_aie_ctrls_setup(struct mtk_aie_ctx *ctx)
{
	struct v4l2_ctrl_handler *hdl = &ctx->hdl;
	int i;

	v4l2_ctrl_handler_init(hdl, V4L2_CID_MTK_AIE_MAX);
	if (hdl->error)
		return hdl->error;

	for (i = 0; i < ARRAY_SIZE(mtk_aie_controls); i++) {
		v4l2_ctrl_new_custom(hdl, &mtk_aie_controls[i], ctx);
		if (hdl->error) {
			v4l2_ctrl_handler_free(hdl);
			dev_info(ctx->dev, "Failed to register controls:%d", i);
			return hdl->error;
		}
	}

	ctx->fh.ctrl_handler = &ctx->hdl;
	v4l2_ctrl_handler_setup(hdl);

	return 0;
}
#endif
static void init_ctx_fmt(struct mtk_aie_ctx *ctx)
{
	struct v4l2_pix_format_mplane *src_fmt = &ctx->src_fmt;
	struct v4l2_meta_format *dst_fmt = &ctx->dst_fmt;

	/* Initialize M2M source fmt */
	src_fmt->width = MTK_FD_OUTPUT_MAX_WIDTH;
	src_fmt->height = MTK_FD_OUTPUT_MAX_HEIGHT;
	mtk_aie_fill_pixfmt_mp(src_fmt, &mtk_aie_img_fmts[0]);

	/* Initialize M2M destination fmt */
	dst_fmt->buffersize = sizeof(struct aie_enq_info);
	dst_fmt->dataformat = V4L2_META_FMT_MTFD_RESULT;
}

/*
 * V4L2 file operations.
 */
static int mtk_vfd_open(struct file *filp)
{
	struct mtk_aie_dev *fd = video_drvdata(filp);
	struct video_device *vdev = video_devdata(filp);
	struct mtk_aie_ctx *ctx;
	int ret;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->fd_dev = fd;
	ctx->dev = fd->dev;
	fd->ctx = ctx;

	v4l2_fh_init(&ctx->fh, vdev);
	filp->private_data = &ctx->fh;

	init_ctx_fmt(ctx);
#if CHECK_SERVICE_0 //Remove CID
	ret = mtk_aie_ctrls_setup(ctx);
	if (ret) {
		dev_info(ctx->dev, "Failed to set up controls:%d\n", ret);
		goto err_fh_exit;
	}
#endif
	ctx->fh.m2m_ctx =
		v4l2_m2m_ctx_init(fd->m2m_dev, ctx, &mtk_aie_queue_init);
	if (IS_ERR(ctx->fh.m2m_ctx)) {
		ret = PTR_ERR(ctx->fh.m2m_ctx);
		goto err_free_ctrl_handler;
	}

	v4l2_fh_add(&ctx->fh);

	return 0;

err_free_ctrl_handler:
	v4l2_ctrl_handler_free(&ctx->hdl);
#if CHECK_SERVICE_0 //Remove CID
err_fh_exit:
#endif
	v4l2_fh_exit(&ctx->fh);
	kfree(ctx);

	return ret;
}

static int mtk_vfd_release(struct file *filp)
{
	struct mtk_aie_ctx *ctx =
		container_of(filp->private_data, struct mtk_aie_ctx, fh);

	v4l2_m2m_ctx_release(ctx->fh.m2m_ctx);

	v4l2_ctrl_handler_free(&ctx->hdl);
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);

	kfree(ctx);

	return 0;
}

static const struct v4l2_file_operations fd_video_fops = {
	.owner = THIS_MODULE,
	.open = mtk_vfd_open,
	.release = mtk_vfd_release,
	.poll = v4l2_m2m_fop_poll,
	.unlocked_ioctl = video_ioctl2,
	.mmap = v4l2_m2m_fop_mmap,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = v4l2_compat_ioctl32,
#endif

};
#if CHECK_SERVICE_0
static void mtk_aie_fill_user_param(struct mtk_aie_dev *fd,
				    struct user_param *user_param,
				    struct v4l2_ctrl_handler *hdl)
{
	struct v4l2_ctrl *ctrl;

	ctrl = v4l2_ctrl_find(hdl, V4L2_CID_MTK_AIE_PARAM);
	if (ctrl)
		memcpy(user_param, ctrl->p_new.p_u32, sizeof(struct user_param));
}
#endif
static void mtk_aie_device_run(void *priv)
{
	struct mtk_aie_ctx *ctx = priv;
	struct mtk_aie_dev *fd = ctx->fd_dev;
	struct vb2_v4l2_buffer *src_buf, *dst_buf;
	struct fd_enq_param fd_param;
	void *plane_vaddr;
	int ret = 0;

	src_buf = v4l2_m2m_next_src_buf(ctx->fh.m2m_ctx);
	dst_buf = v4l2_m2m_next_dst_buf(ctx->fh.m2m_ctx);

	fd_param.src_img[0].dma_addr =
		vb2_dma_contig_plane_dma_addr(&src_buf->vb2_buf, 0);

	if (ctx->src_fmt.num_planes == 2) {
		fd_param.src_img[1].dma_addr =
			vb2_dma_contig_plane_dma_addr(&src_buf->vb2_buf, 1);
	}

	vb2_dma_contig_plane_dma_addr(&src_buf->vb2_buf, 1);
#if CHECK_SERVICE_0 //Remove CID
	mtk_aie_fill_user_param(fd, &fd_param.user_param, &ctx->hdl);
#endif
	plane_vaddr = vb2_plane_vaddr(&dst_buf->vb2_buf, 0);

	fd->aie_cfg = (struct aie_enq_info *)plane_vaddr;

	fd->aie_cfg->sel_mode = g_user_param.user_param.fd_mode;
	fd->aie_cfg->src_img_fmt = g_user_param.user_param.src_img_fmt;
	fd->aie_cfg->src_img_width = g_user_param.user_param.src_img_width;
	fd->aie_cfg->src_img_height = g_user_param.user_param.src_img_height;
	fd->aie_cfg->src_img_stride = g_user_param.user_param.src_img_stride;
	fd->aie_cfg->pyramid_base_width = g_user_param.user_param.pyramid_base_width;
	fd->aie_cfg->pyramid_base_height = g_user_param.user_param.pyramid_base_height;
	fd->aie_cfg->number_of_pyramid = g_user_param.user_param.number_of_pyramid;
	fd->aie_cfg->rotate_degree = g_user_param.user_param.rotate_degree;
	fd->aie_cfg->en_roi = g_user_param.user_param.en_roi;
	fd->aie_cfg->src_roi.x1 = g_user_param.user_param.src_roi_x1;
	fd->aie_cfg->src_roi.y1 = g_user_param.user_param.src_roi_y1;
	fd->aie_cfg->src_roi.x2 = g_user_param.user_param.src_roi_x2;
	fd->aie_cfg->src_roi.y2 = g_user_param.user_param.src_roi_y2;
	fd->aie_cfg->en_padding = g_user_param.user_param.en_padding;
	fd->aie_cfg->src_padding.left = g_user_param.user_param.src_padding_left;
	fd->aie_cfg->src_padding.right = g_user_param.user_param.src_padding_right;
	fd->aie_cfg->src_padding.down = g_user_param.user_param.src_padding_down;
	fd->aie_cfg->src_padding.up = g_user_param.user_param.src_padding_up;
	fd->aie_cfg->freq_level = g_user_param.user_param.freq_level;
#ifdef FLD
	fd->aie_cfg->fld_face_num = g_user_param.user_param.fld_face_num;
	memcpy(fd->aie_cfg->fld_input, g_user_param.user_param.fld_input,
		 sizeof(struct FLD_CROP_RIP_ROP)*g_user_param.user_param.fld_face_num);
#endif
	if (!(fd->aie_cfg->sel_mode == 2) & (fd->aie_cfg->src_img_fmt == FMT_YUV420_1P)) {
		fd_param.src_img[1].dma_addr = fd_param.src_img[0].dma_addr +
			g_user_param.user_param.src_img_stride *
			g_user_param.user_param.src_img_height;
	}

	fd->aie_cfg->src_img_addr = fd_param.src_img[0].dma_addr;
	fd->aie_cfg->src_img_addr_uv = fd_param.src_img[1].dma_addr;

	ret = aie_prepare(fd, fd->aie_cfg);//fld just setting debug param

	/* mmdvfs */
	mtk_aie_mmdvfs_set(fd, 1, fd->aie_cfg->freq_level);

	/* Complete request controls if any */
	v4l2_ctrl_request_complete(src_buf->vb2_buf.req_obj.req, &ctx->hdl);

	atomic_inc(&fd->num_composing);

	mtk_aie_hw_job_exec(fd, &fd_param);

	if (ret) {
		dev_info(fd->dev, "Failed to prepare aie setting\n");
		return;
	}

	aie_execute(fd, fd->aie_cfg);
}

static struct v4l2_m2m_ops fd_m2m_ops = {
	.device_run = mtk_aie_device_run,
};

static const struct media_device_ops fd_m2m_media_ops = {
	.req_validate = vb2_request_validate,
	.req_queue = v4l2_m2m_request_queue,
};

static int mtk_aie_video_device_register(struct mtk_aie_dev *fd)
{
	struct video_device *vfd = &fd->vfd;
	struct v4l2_m2m_dev *m2m_dev = fd->m2m_dev;
	struct device *dev = fd->dev;
	int ret;

	vfd->fops = &fd_video_fops;
	vfd->release = video_device_release;
	vfd->lock = &fd->vfd_lock;
	vfd->v4l2_dev = &fd->v4l2_dev;
	vfd->vfl_dir = VFL_DIR_M2M;
	vfd->device_caps = V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_OUTPUT_MPLANE |
			   V4L2_CAP_META_CAPTURE;
	vfd->ioctl_ops = &mtk_aie_v4l2_video_out_ioctl_ops;

	strscpy(vfd->name, dev_driver_string(dev), sizeof(vfd->name));

	video_set_drvdata(vfd, fd);

	ret = video_register_device(vfd, VFL_TYPE_GRABBER, 0);
	if (ret) {
		dev_info(dev, "Failed to register video device\n");
		goto err_free_dev;
	}

	ret = v4l2_m2m_register_media_controller(
		m2m_dev, vfd, MEDIA_ENT_F_PROC_VIDEO_STATISTICS);
	if (ret) {
		dev_info(dev, "Failed to init mem2mem media controller\n");
		goto err_unreg_video;
	}
	return 0;

err_unreg_video:
	video_unregister_device(vfd);
err_free_dev:
	video_device_release(vfd);
	return ret;
}

static int mtk_aie_dev_larb_init(struct mtk_aie_dev *fd)
{
	struct device_node *node;
	struct platform_device *pdev;

	node = of_parse_phandle(fd->dev->of_node, "mediatek,larb", 0);
	if (!node)
		return -EINVAL;
	pdev = of_find_device_by_node(node);
	if (WARN_ON(!pdev)) {
		of_node_put(node);
		return -EINVAL;
	}
	of_node_put(node);

	fd->larb = &pdev->dev;

	return 0;
}

static int mtk_aie_dev_v4l2_init(struct mtk_aie_dev *fd)
{
	struct media_device *mdev = &fd->mdev;
	struct device *dev = fd->dev;
	int ret;

	ret = v4l2_device_register(dev, &fd->v4l2_dev);
	if (ret) {
		dev_info(dev, "Failed to register v4l2 device\n");
		return ret;
	}

	fd->m2m_dev = v4l2_m2m_init(&fd_m2m_ops);
	if (IS_ERR(fd->m2m_dev)) {
		dev_info(dev, "Failed to init mem2mem device\n");
		ret = PTR_ERR(fd->m2m_dev);
		goto err_unreg_v4l2_dev;
	}

	mdev->dev = dev;
	strscpy(mdev->model, dev_driver_string(dev), sizeof(mdev->model));
	snprintf(mdev->bus_info, sizeof(mdev->bus_info), "platform:%s",
		 dev_name(dev));
	media_device_init(mdev);
	mdev->ops = &fd_m2m_media_ops;
	fd->v4l2_dev.mdev = mdev;

	ret = mtk_aie_video_device_register(fd);
	if (ret) {
		dev_info(dev, "Failed to register video device\n");
		goto err_cleanup_mdev;
	}

	ret = media_device_register(mdev);
	if (ret) {
		dev_info(dev, "Failed to register mem2mem media device\n");
		goto err_unreg_vdev;
	}

	return 0;

err_unreg_vdev:
	v4l2_m2m_unregister_media_controller(fd->m2m_dev);
	video_unregister_device(&fd->vfd);
	video_device_release(&fd->vfd);
err_cleanup_mdev:
	media_device_cleanup(mdev);
	v4l2_m2m_release(fd->m2m_dev);
err_unreg_v4l2_dev:
	v4l2_device_unregister(&fd->v4l2_dev);
	return ret;
}

static void mtk_aie_dev_v4l2_release(struct mtk_aie_dev *fd)
{
	v4l2_m2m_unregister_media_controller(fd->m2m_dev);
	video_unregister_device(&fd->vfd);
	video_device_release(&fd->vfd);
	media_device_cleanup(&fd->mdev);
	v4l2_m2m_release(fd->m2m_dev);
	v4l2_device_unregister(&fd->v4l2_dev);
}

static irqreturn_t mtk_aie_irq(int irq, void *data)
{
	struct mtk_aie_dev *fd = (struct mtk_aie_dev *)data;

	aie_irqhandle(fd);

	queue_work(fd->frame_done_wq, &fd->req_work.work);

	return IRQ_HANDLED;
}

static void mtk_aie_frame_done_worker(struct work_struct *work)
{
	struct mtk_aie_req_work *req_work = (struct mtk_aie_req_work *)work;
	struct mtk_aie_dev *fd = (struct mtk_aie_dev *)req_work->fd_dev;

	if (fd->reg_cfg.fd_mode == 0) {
		fd->reg_cfg.hw_result = readl(fd->fd_base + AIE_RESULT_0_REG);
		fd->reg_cfg.hw_result1 = readl(fd->fd_base + AIE_RESULT_1_REG);
	}

	if (fd->aie_cfg->sel_mode == 0)
		aie_get_fd_result(fd, fd->aie_cfg);
	else if (fd->aie_cfg->sel_mode == 1)
		aie_get_attr_result(fd, fd->aie_cfg);
	else if (fd->aie_cfg->sel_mode == 2)
		aie_get_fld_result(fd, fd->aie_cfg);

	mtk_aie_hw_done(fd, VB2_BUF_STATE_DONE);
}

static int mtk_aie_probe(struct platform_device *pdev)
{
	struct mtk_aie_dev *fd;
	struct device *dev = &pdev->dev;

	struct resource *res;
	int irq;
	int ret;

	fd = devm_kzalloc(&pdev->dev, sizeof(*fd), GFP_KERNEL);
	if (!fd)
		return -ENOMEM;

	if (dma_set_mask_and_coherent(dev, DMA_BIT_MASK(34)))
		dev_info(dev, "%s: No suitable DMA available\n", __func__);

	dev_set_drvdata(dev, fd);
	fd->dev = dev;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_info(dev, "Failed to get irq by platform: %d\n", irq);
		return irq;
	}

	ret = devm_request_irq(dev, irq, mtk_aie_irq, IRQF_SHARED,
			       dev_driver_string(dev), fd);
	if (ret) {
		dev_info(dev, "Failed to request irq\n");
		return ret;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	fd->fd_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(fd->fd_base)) {
		dev_info(dev, "Failed to get fd reg base\n");
		return PTR_ERR(fd->fd_base);
	}

	ret = mtk_aie_dev_larb_init(fd);
	if (ret) {
		dev_info(dev, "Failed to init larb : %d\n", ret);
		return ret;
	}

	fd->img_ipe = devm_clk_get(dev, "IMG_IPE");
	if (IS_ERR(fd->img_ipe)) {
		dev_info(dev, "Failed to get img_ipe clock\n");
		return PTR_ERR(fd->img_ipe);
	}

	fd->ipe_fdvt = devm_clk_get(dev, "IPE_FDVT");
	if (IS_ERR(fd->ipe_fdvt)) {
		dev_info(dev, "Failed to get ipe_fdvt clock\n");
		return PTR_ERR(fd->ipe_fdvt);
	}

	fd->ipe_smi_larb12 = devm_clk_get(dev, "IPE_SMI_LARB12");
	if (IS_ERR(fd->ipe_smi_larb12)) {
		dev_info(dev, "Failed to get ipe_smi_larb12 clock\n");
		return PTR_ERR(fd->ipe_smi_larb12);
	}

	fd->ipe_top = devm_clk_get(dev, "IPE_TOP");
	if (IS_ERR(fd->ipe_top)) {
		dev_info(dev, "Failed to get ipe_top clock\n");
		return PTR_ERR(fd->ipe_top);
	}

	mutex_init(&fd->vfd_lock);
	init_completion(&fd->fd_job_finished);
	INIT_DELAYED_WORK(&fd->job_timeout_work, mtk_aie_job_timeout_work);
	init_waitqueue_head(&fd->flushing_waitq);
	atomic_set(&fd->num_composing, 0);

	fd->frame_done_wq =
			alloc_ordered_workqueue(dev_name(fd->dev),
						WQ_HIGHPRI | WQ_FREEZABLE);
	if (!fd->frame_done_wq) {
		dev_info(fd->dev, "failed to alloc frame_done workqueue\n");
		mutex_destroy(&fd->vfd_lock);
		return -ENOMEM;
	}

	INIT_WORK(&fd->req_work.work, mtk_aie_frame_done_worker);
	fd->req_work.fd_dev = fd;

	mtk_aie_mmdvfs_init(fd);
	mtk_aie_mmqos_init(fd);
	pm_runtime_enable(dev);
	ret = mtk_aie_dev_v4l2_init(fd);
	if (ret) {
		dev_info(dev, "Failed to init v4l2 device: %d\n", ret);
		goto err_destroy_mutex;
	}
	dev_info(dev, "AIE : Success to %s\n", __func__);

	return 0;

err_destroy_mutex:
	pm_runtime_disable(fd->dev);
	mtk_aie_mmdvfs_uninit(fd);
	mtk_aie_mmqos_uninit(fd);
	destroy_workqueue(fd->frame_done_wq);
	mutex_destroy(&fd->vfd_lock);

	return ret;
}

static int mtk_aie_remove(struct platform_device *pdev)
{
	struct mtk_aie_dev *fd = dev_get_drvdata(&pdev->dev);

	mtk_aie_dev_v4l2_release(fd);
	pm_runtime_disable(&pdev->dev);
	mtk_aie_mmdvfs_uninit(fd);
	mtk_aie_mmqos_uninit(fd);
	destroy_workqueue(fd->frame_done_wq);
	fd->frame_done_wq = NULL;
	mutex_destroy(&fd->vfd_lock);

	return 0;
}

static int mtk_aie_suspend(struct device *dev)
{
	struct mtk_aie_dev *fd = dev_get_drvdata(dev);
	int ret, num;

	if (pm_runtime_suspended(dev))
		return 0;

	num = atomic_read(&fd->num_composing);
	dev_dbg(dev, "%s: suspend aie job start, num(%d)\n", __func__, num);

	ret = wait_event_timeout
		(fd->flushing_waitq,
		 !(num = atomic_read(&fd->num_composing)),
		 msecs_to_jiffies(MTK_FD_HW_TIMEOUT));
	if (!ret && num) {
		dev_info(dev, "%s: flushing aie job timeout, num(%d)\n",
			__func__, num);

		return -EBUSY;
	}

	dev_dbg(dev, "%s: suspend aie job end, num(%d)\n", __func__, num);

	ret = pm_runtime_force_suspend(dev);
	if (ret)
		return ret;

	return 0;
}

static int mtk_aie_resume(struct device *dev)
{
	int ret;

	dev_dbg(dev, "%s: resume aie job start)\n", __func__);

	if (pm_runtime_suspended(dev)) {
		dev_dbg(dev, "%s: pm_runtime_suspended is true, no action\n",
			__func__);
		return 0;
	}

	ret = pm_runtime_force_resume(dev);
	if (ret)
		return ret;

	dev_dbg(dev, "%s: resume aie job end)\n", __func__);
	return 0;
}

static int mtk_aie_runtime_suspend(struct device *dev)
{
	struct mtk_aie_dev *fd = dev_get_drvdata(dev);

	dev_dbg(dev, "%s: runtime suspend aie job start)\n", __func__);

	clk_disable_unprepare(fd->ipe_top);
	clk_disable_unprepare(fd->ipe_smi_larb12);
	clk_disable_unprepare(fd->ipe_fdvt);
	clk_disable_unprepare(fd->img_ipe);

	dev_dbg(dev, "%s: runtime suspend aie job end)\n", __func__);

	return 0;
}

static int mtk_aie_runtime_resume(struct device *dev)
{
	struct mtk_aie_dev *fd = dev_get_drvdata(dev);
	int ret;

	dev_dbg(dev, "%s: runtime resume aie job start)\n", __func__);

	ret = clk_prepare_enable(fd->img_ipe);
	if (ret) {
		dev_info(dev, "Failed to open img_ipe clk:%d\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(fd->ipe_fdvt);
	if (ret) {
		dev_info(dev, "Failed to open ipe_fdvt clk:%d\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(fd->ipe_smi_larb12);
	if (ret) {
		dev_info(dev, "Failed to open ipe_smi_larb12 clk:%d\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(fd->ipe_top);
	if (ret) {
		dev_info(dev, "Failed to open ipe_top clk:%d\n", ret);
		return ret;
	}
	dev_dbg(dev, "%s: runtime resume aie job end)\n", __func__);

	return 0;
}

static const struct dev_pm_ops mtk_aie_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_aie_suspend, mtk_aie_resume)
		SET_RUNTIME_PM_OPS(mtk_aie_runtime_suspend,
				   mtk_aie_runtime_resume, NULL)};

static const struct of_device_id mtk_aie_of_ids[] = {
	{
		.compatible = "mediatek,aie-hw3.0",
	},
	{} };
MODULE_DEVICE_TABLE(of, mtk_aie_of_ids);

static struct platform_driver mtk_aie_driver = {
	.probe = mtk_aie_probe,
	.remove = mtk_aie_remove,
	.driver = {
		.name = "mtk-aie-5.3",
		.of_match_table = of_match_ptr(mtk_aie_of_ids),
		.pm = &mtk_aie_pm_ops,
	} };

module_platform_driver(mtk_aie_driver);
MODULE_AUTHOR("Fish Wu <fish.wu@mediatek.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Mediatek AIE driver");
