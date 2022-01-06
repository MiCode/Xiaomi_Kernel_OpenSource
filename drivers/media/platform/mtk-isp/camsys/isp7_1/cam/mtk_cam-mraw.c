// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/vmalloc.h>

#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-subdev.h>

#include <soc/mediatek/smi.h>

#include "mtk_cam.h"
#include "mtk_cam_pm.h"
#include "mtk_cam-pool.h"
#include "mtk_cam-mraw-regs.h"
#include "mtk_cam-mraw.h"
#ifdef CAMSYS_MRAW_V1
#include "mtk_cam-meta-mt6983.h"
#endif
#ifdef CAMSYS_MRAW_V2
#include "mtk_cam-meta-mt6879.h"
#endif
#ifdef CAMSYS_MRAW_V3
#include "mtk_cam-meta-mt6895.h"
#endif
#ifndef CAMSYS_MRAW_V1
#ifndef CAMSYS_MRAW_V2
#ifndef CAMSYS_MRAW_V3
#include "mtk_cam-meta-dummy.h"  // TODO: temp for other platform build pass
#endif
#endif
#endif
#include "mtk_camera-v4l2-controls.h"
#include "mtk_camera-videodev2.h"

#ifdef CAMSYS_TF_DUMP_71_1
#include <dt-bindings/memory/mt6983-larb-port.h>
#include "iommu_debug.h"
#endif

#define MTK_MRAW_STOP_HW_TIMEOUT			(33 * USEC_PER_MSEC)


static const struct of_device_id mtk_mraw_of_ids[] = {
	{.compatible = "mediatek,mraw",},
	{}
};
MODULE_DEVICE_TABLE(of, mtk_mraw_of_ids);

#define MAX_NUM_MRAW_CLOCKS 5
#define LARB25_PORT_SIZE 12
#define LARB26_PORT_SIZE 13

static const struct v4l2_mbus_framefmt mraw_mfmt_default = {
	.code = MEDIA_BUS_FMT_SBGGR10_1X10,
	.width = DEFAULT_WIDTH,
	.height = DEFAULT_HEIGHT,
	.field = V4L2_FIELD_NONE,
	.colorspace = V4L2_COLORSPACE_SRGB,
	.xfer_func = V4L2_XFER_FUNC_DEFAULT,
	.ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT,
	.quantization = V4L2_QUANTIZATION_DEFAULT,
};

static void mtk_mraw_register_iommu_tf_callback(struct mtk_mraw_device *mraw)
{
#ifdef CAMSYS_TF_DUMP_71_1
	dev_dbg(mraw->dev, "%s : mraw->id:%d\n", __func__, mraw->id);

	switch (mraw->id) {
	case MRAW_0:
		mtk_iommu_register_fault_callback(M4U_PORT_L25_CAM_MRAW0_CQI_M1,
			mtk_mraw_translation_fault_callback, (void *)mraw, false);
		mtk_iommu_register_fault_callback(M4U_PORT_L25_CAM_MRAW0_CQI_M2,
			mtk_mraw_translation_fault_callback, (void *)mraw, false);
		mtk_iommu_register_fault_callback(M4U_PORT_L25_CAM_MRAW0_IMGO_M1,
			mtk_mraw_translation_fault_callback, (void *)mraw, false);
		mtk_iommu_register_fault_callback(M4U_PORT_L25_CAM_MRAW0_IMGBO_M1,
			mtk_mraw_translation_fault_callback, (void *)mraw, false);
		break;
	case MRAW_1:
		mtk_iommu_register_fault_callback(M4U_PORT_L26_CAM_MRAW1_CQI_M1,
			mtk_mraw_translation_fault_callback, (void *)mraw, false);
		mtk_iommu_register_fault_callback(M4U_PORT_L26_CAM_MRAW1_CQI_M2,
			mtk_mraw_translation_fault_callback, (void *)mraw, false);
		mtk_iommu_register_fault_callback(M4U_PORT_L26_CAM_MRAW1_IMGO_M1,
			mtk_mraw_translation_fault_callback, (void *)mraw, false);
		mtk_iommu_register_fault_callback(M4U_PORT_L26_CAM_MRAW1_IMGBO_M1,
			mtk_mraw_translation_fault_callback, (void *)mraw, false);
		break;
	case MRAW_2:
		mtk_iommu_register_fault_callback(M4U_PORT_L25_CAM_MRAW2_CQI_M1,
			mtk_mraw_translation_fault_callback, (void *)mraw, false);
		mtk_iommu_register_fault_callback(M4U_PORT_L25_CAM_MRAW2_CQI_M2,
			mtk_mraw_translation_fault_callback, (void *)mraw, false);
		mtk_iommu_register_fault_callback(M4U_PORT_L25_CAM_MRAW2_IMGO_M1,
			mtk_mraw_translation_fault_callback, (void *)mraw, false);
		mtk_iommu_register_fault_callback(M4U_PORT_L25_CAM_MRAW2_IMGBO_M1,
			mtk_mraw_translation_fault_callback, (void *)mraw, false);
		break;
	case MRAW_3:
		mtk_iommu_register_fault_callback(M4U_PORT_L26_CAM_MRAW3_CQI_M1,
			mtk_mraw_translation_fault_callback, (void *)mraw, false);
		mtk_iommu_register_fault_callback(M4U_PORT_L26_CAM_MRAW3_CQI_M2,
			mtk_mraw_translation_fault_callback, (void *)mraw, false);
		mtk_iommu_register_fault_callback(M4U_PORT_L26_CAM_MRAW3_IMGO_M1,
			mtk_mraw_translation_fault_callback, (void *)mraw, false);
		mtk_iommu_register_fault_callback(M4U_PORT_L26_CAM_MRAW3_IMGBO_M1,
			mtk_mraw_translation_fault_callback, (void *)mraw, false);
		break;
	}
#endif
};

#ifdef CAMSYS_TF_DUMP_71_1
int mtk_mraw_translation_fault_callback(int port, dma_addr_t mva, void *data)
{
	struct mtk_mraw_device *mraw_dev = (struct mtk_mraw_device *)data;

	dev_info(mraw_dev->dev, "tg_sen_mode:0x%x tg_vf_con:0x%x tg_path_cfg:0x%x tg_grab_pxl:0x%x tg_grab_lin:0x%x\n",
		readl_relaxed(mraw_dev->base_inner + REG_MRAW_TG_SEN_MODE),
		readl_relaxed(mraw_dev->base_inner + REG_MRAW_TG_VF_CON),
		readl_relaxed(mraw_dev->base_inner + REG_MRAW_TG_PATH_CFG),
		readl_relaxed(mraw_dev->base_inner + REG_MRAW_TG_SEN_GRAB_PXL),
		readl_relaxed(mraw_dev->base_inner + REG_MRAW_TG_SEN_GRAB_LIN));

	dev_info(mraw_dev->dev, "mod_en:0x%x mod2_en:0x%x cq_thr0_addr:0x%x_%x cq_thr0_desc_size:0x%x\n",
		readl_relaxed(mraw_dev->base_inner + REG_MRAW_MRAWCTL_MOD_EN),
		readl_relaxed(mraw_dev->base_inner + REG_MRAW_MRAWCTL_MOD2_EN),
		readl_relaxed(mraw_dev->base_inner + REG_MRAW_CQ_THR0_BASEADDR_MSB),
		readl_relaxed(mraw_dev->base_inner + REG_MRAW_CQ_THR0_BASEADDR),
		readl_relaxed(mraw_dev->base_inner + REG_MRAW_CQ_THR0_DESC_SIZE));

	dev_info(mraw_dev->dev, "imgo_fbc_ctrl1:0x%x imgo_fbc_ctrl2:0x%x imgBo_fbc_ctrl1:0x%x imgBo_fbc_ctrl2:0x%x cpio_fbc_ctrl1:0x%x cpio_fbc_ctrl2:0x%x\n",
		readl_relaxed(mraw_dev->base_inner + REG_MRAW_FBC_IMGO_CTL1),
		readl_relaxed(mraw_dev->base_inner + REG_MRAW_FBC_IMGO_CTL2),
		readl_relaxed(mraw_dev->base_inner + REG_MRAW_FBC_IMGBO_CTL1),
		readl_relaxed(mraw_dev->base_inner + REG_MRAW_FBC_IMGBO_CTL2),
		readl_relaxed(mraw_dev->base_inner + REG_MRAW_FBC_CPIO_CTL1),
		readl_relaxed(mraw_dev->base_inner + REG_MRAW_FBC_CPIO_CTL2));

	dev_info(mraw_dev->dev, "imgo_xsize:0x%x imgo_ysize:0x%x imgo_stride:0x%x imgo_addr:0x%x_%x imgo_ofst_addr:0x%x_%x\n",
		readl_relaxed(mraw_dev->base_inner + REG_MRAW_IMGO_XSIZE),
		readl_relaxed(mraw_dev->base_inner + REG_MRAW_IMGO_YSIZE),
		readl_relaxed(mraw_dev->base_inner + REG_MRAW_IMGO_STRIDE),
		readl_relaxed(mraw_dev->base_inner + REG_MRAW_IMGO_BASE_ADDR_MSB),
		readl_relaxed(mraw_dev->base_inner + REG_MRAW_IMGO_BASE_ADDR),
		readl_relaxed(mraw_dev->base_inner + REG_MRAW_IMGO_OFST_ADDR_MSB),
		readl_relaxed(mraw_dev->base_inner + REG_MRAW_IMGO_OFST_ADDR));

	dev_info(mraw_dev->dev, "imgbo_xsize:0x%x imgbo_ysize:0x%x imgbo_stride:0x%x imgbo_addr:0x%x_%x imgbo_ofst_addr:0x%x_%x\n",
		readl_relaxed(mraw_dev->base_inner + REG_MRAW_IMGBO_XSIZE),
		readl_relaxed(mraw_dev->base_inner + REG_MRAW_IMGBO_YSIZE),
		readl_relaxed(mraw_dev->base_inner + REG_MRAW_IMGBO_STRIDE),
		readl_relaxed(mraw_dev->base_inner + REG_MRAW_IMGBO_BASE_ADDR_MSB),
		readl_relaxed(mraw_dev->base_inner + REG_MRAW_IMGBO_BASE_ADDR),
		readl_relaxed(mraw_dev->base_inner + REG_MRAW_IMGBO_OFST_ADDR_MSB),
		readl_relaxed(mraw_dev->base_inner + REG_MRAW_IMGBO_OFST_ADDR));

	dev_info(mraw_dev->dev, "cpio_xsize:0x%x cpio_ysize:0x%x cpio_stride:0x%x cpio_addr:0x%x_%x cpio_ofst_addr:0x%x_%x\n",
		readl_relaxed(mraw_dev->base_inner + REG_MRAW_CPIO_XSIZE),
		readl_relaxed(mraw_dev->base_inner + REG_MRAW_CPIO_YSIZE),
		readl_relaxed(mraw_dev->base_inner + REG_MRAW_CPIO_STRIDE),
		readl_relaxed(mraw_dev->base_inner + REG_MRAW_CPIO_BASE_ADDR_MSB),
		readl_relaxed(mraw_dev->base_inner + REG_MRAW_CPIO_BASE_ADDR),
		readl_relaxed(mraw_dev->base_inner + REG_MRAW_CPIO_OFST_ADDR_MSB),
		readl_relaxed(mraw_dev->base_inner + REG_MRAW_CPIO_OFST_ADDR));

	return 0;
}
#endif

static int mtk_mraw_sd_subscribe_event(struct v4l2_subdev *subdev,
				      struct v4l2_fh *fh,
				      struct v4l2_event_subscription *sub)
{
	switch (sub->type) {
	case V4L2_EVENT_FRAME_SYNC:
		return v4l2_event_subscribe(fh, sub, 0, NULL);
	case V4L2_EVENT_REQUEST_DRAINED:
		return v4l2_event_subscribe(fh, sub, 0, NULL);
	default:
		return -EINVAL;
	}
}

int mtk_cam_mraw_select(struct mtk_mraw_pipeline *pipe)
{
	pipe->enabled_mraw = 1 << (pipe->id - MTKCAM_SUBDEV_MRAW_START);

	return 0;
}

static int mtk_mraw_sd_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct mtk_mraw_pipeline *pipe =
		container_of(sd, struct mtk_mraw_pipeline, subdev);
	struct mtk_mraw *mraw = pipe->mraw;
	struct mtk_cam_device *cam = dev_get_drvdata(mraw->cam_dev);
	struct mtk_cam_ctx *ctx = mtk_cam_find_ctx(cam, &sd->entity);
	unsigned int i;

	if (WARN_ON(!ctx))
		return -EINVAL;

	if (enable) {
		pipe->enabled_dmas = 0;
		if (ctx->used_mraw_num < MAX_MRAW_PIPES_PER_STREAM)
			ctx->mraw_pipe[ctx->used_mraw_num++] = pipe;
		else
			dev_info(mraw->cam_dev,
				"un-expected used mraw number:%d\n", ctx->used_mraw_num);

		for (i = 0; i < ARRAY_SIZE(pipe->vdev_nodes); i++) {
			if (!pipe->vdev_nodes[i].enabled)
				continue;
			pipe->enabled_dmas |= (1ULL << pipe->vdev_nodes[i].desc.dma_port);
		}
	} else {
		pipe->enabled_mraw = 0;
		pipe->enabled_dmas = 0;
	}

	dev_dbg(mraw->cam_dev, "%s:mraw-%d: en %d, dev 0x%x dmas %llx\n",
		 __func__, pipe->id-MTKCAM_SUBDEV_MRAW_START, enable, pipe->enabled_mraw,
		 pipe->enabled_dmas);

	return 0;
}

static int mtk_mraw_init_cfg(struct v4l2_subdev *sd,
		struct v4l2_subdev_pad_config *cfg)
{
	struct v4l2_mbus_framefmt *mf;
	unsigned int i;
	struct mtk_mraw_pipeline *pipe =
		container_of(sd, struct mtk_mraw_pipeline, subdev);
	struct mtk_mraw *mraw = pipe->mraw;

	for (i = 0; i < sd->entity.num_pads; i++) {
		mf = v4l2_subdev_get_try_format(sd, cfg, i);
		*mf = mraw_mfmt_default;
		pipe->cfg[i].mbus_fmt = mraw_mfmt_default;

		dev_dbg(mraw->cam_dev, "%s init pad:%d format:0x%x\n",
			sd->name, i, mf->code);
	}

	return 0;
}

static int mtk_mraw_try_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_format *fmt)
{
	struct mtk_mraw_pipeline *pipe =
		container_of(sd, struct mtk_mraw_pipeline, subdev);
	struct mtk_mraw *mraw = pipe->mraw;
	unsigned int sensor_fmt = mtk_cam_get_sensor_fmt(fmt->format.code);

	dev_dbg(mraw->cam_dev, "%s try format 0x%x, w:%d, h:%d field:%d\n",
		sd->name, fmt->format.code, fmt->format.width,
		fmt->format.height, fmt->format.field);

	/* check sensor format */
	if (!sensor_fmt || fmt->pad == MTK_MRAW_SINK)
		return sensor_fmt;
	else if (fmt->pad < MTK_MRAW_PIPELINE_PADS_NUM) {
		/* check vdev node format */
		unsigned int img_fmt, i;
		struct mtk_cam_video_device *node =
			&pipe->vdev_nodes[fmt->pad - MTK_MRAW_SINK_NUM];

		dev_dbg(mraw->cam_dev, "node:%s num_fmts:%d",
				node->desc.name, node->desc.num_fmts);
		for (i = 0; i < node->desc.num_fmts; i++) {
			img_fmt = mtk_cam_get_img_fmt(
				node->desc.fmts[i].vfmt.fmt.pix_mp.pixelformat);
			dev_dbg(mraw->cam_dev,
				"try format sensor_fmt 0x%x img_fmt 0x%x",
				sensor_fmt, img_fmt);
			if (sensor_fmt == img_fmt)
				return img_fmt;
		}
	}

	return MTKCAM_IPI_IMG_FMT_UNKNOWN;
}

static struct v4l2_mbus_framefmt *get_mraw_fmt(struct mtk_mraw_pipeline *pipe,
					  struct v4l2_subdev_pad_config *cfg,
					  unsigned int padid, int which)
{
	/* format invalid and return default format */
	if (which == V4L2_SUBDEV_FORMAT_TRY)
		return v4l2_subdev_get_try_format(&pipe->subdev, cfg, padid);

	if (WARN_ON(padid >= pipe->subdev.entity.num_pads))
		return &pipe->cfg[0].mbus_fmt;

	return &pipe->cfg[padid].mbus_fmt;
}

static int mtk_mraw_call_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *fmt)
{
	struct mtk_mraw_pipeline *pipe =
		container_of(sd, struct mtk_mraw_pipeline, subdev);
	struct mtk_mraw *mraw = pipe->mraw;
	struct v4l2_mbus_framefmt *mf;
	unsigned int ipi_fmt;

	if (!sd || !fmt) {
		dev_dbg(mraw->cam_dev, "%s: Required sd(%p), fmt(%p)\n",
			__func__, sd, fmt);
		return -EINVAL;
	}

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY && !cfg) {
		dev_dbg(mraw->cam_dev, "%s: Required sd(%p), cfg(%p) for FORMAT_TRY\n",
					__func__, sd, cfg);
		return -EINVAL;
	}

	if (!mtk_mraw_try_fmt(sd, fmt)) {
		mf = get_mraw_fmt(pipe, cfg, fmt->pad, fmt->which);
		fmt->format = *mf;
	} else {
		mf = get_mraw_fmt(pipe, cfg, fmt->pad, fmt->which);
		*mf = fmt->format;
		dev_info(mraw->cam_dev,
			"sd:%s pad:%d set format w/h/code %d/%d/0x%x\n",
			sd->name, fmt->pad, mf->width, mf->height, mf->code);

		if (fmt->pad == MTK_MRAW_SINK &&
			fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE) {
			dev_info(mraw->cam_dev,
				"%s: set mraw res_config\n", __func__);
			/* set cfg buffer for tg/crp info. */
			ipi_fmt = mtk_cam_get_sensor_fmt(fmt->format.code);
			if (ipi_fmt == MTKCAM_IPI_IMG_FMT_UNKNOWN) {
				dev_info(mraw->cam_dev, "%s:Unknown pixelfmt:%d\n"
					, __func__, ipi_fmt);
			}
			pipe->res_config.width = mf->width;
			pipe->res_config.height = mf->height;
			pipe->res_config.img_fmt = ipi_fmt;
		}
	}

	return 0;
}

int mtk_mraw_call_pending_set_fmt(struct v4l2_subdev *sd,
				 struct v4l2_subdev_format *fmt)
{
	struct mtk_mraw_pipeline *pipe =
		container_of(sd, struct mtk_mraw_pipeline, subdev);
	struct mtk_cam_device *cam = dev_get_drvdata(pipe->mraw->cam_dev);

	/* We only allow V4L2_SUBDEV_FORMAT_ACTIVE for pending set fmt */
	if (fmt->which != V4L2_SUBDEV_FORMAT_ACTIVE) {
		dev_info(cam->dev,
			"%s:pipe(%d):pad(%d): only allow V4L2_SUBDEV_FORMAT_ACTIVE\n",
			__func__, pipe->id, fmt->pad);
		return -EINVAL;
	}

	return mtk_mraw_call_set_fmt(sd, NULL, fmt);
}

static int mtk_mraw_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *fmt)
{
	struct media_request *req;
	struct mtk_cam_request *cam_req;
	struct mtk_cam_request_stream_data *stream_data;

	struct mtk_mraw_pipeline *pipe =
		container_of(sd, struct mtk_mraw_pipeline, subdev);
	struct mtk_cam_device *cam = dev_get_drvdata(pipe->mraw->cam_dev);

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY)
		return mtk_mraw_call_set_fmt(sd, cfg, fmt);

	/* if the pipeline is streaming, pending the change */
	if (!sd->entity.stream_count) {
		pipe->res_config.is_initial = 1;
		return mtk_mraw_call_set_fmt(sd, cfg, fmt);
	}

	if (v4l2_subdev_format_request_fd(fmt) <= 0)
		return -EINVAL;

	req = media_request_get_by_fd(&cam->media_dev,
				v4l2_subdev_format_request_fd(fmt));
	if (req) {
		cam_req = to_mtk_cam_req(req);
		dev_info(cam->dev, "sd:%s pad:%d pending success, req fd(%d)\n",
			sd->name, fmt->pad, v4l2_subdev_format_request_fd(fmt));
	} else {
		dev_info(cam->dev, "sd:%s pad:%d pending failed, req fd(%d) invalid\n",
			sd->name, fmt->pad, v4l2_subdev_format_request_fd(fmt));
		return -EINVAL;
	}

	stream_data = mtk_cam_req_get_s_data_no_chk(cam_req, pipe->id, 0);
	stream_data->pad_fmt_update |= (1 << fmt->pad);
	stream_data->pad_fmt[fmt->pad] = *fmt;

	media_request_put(req);

	return 0;
}

static int mtk_mraw_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *fmt)
{
	struct mtk_mraw_pipeline *pipe =
		container_of(sd, struct mtk_mraw_pipeline, subdev);
	struct mtk_mraw *mraw = pipe->mraw;
	struct v4l2_mbus_framefmt *mf;

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY)
		mf = v4l2_subdev_get_try_format(sd, cfg, fmt->pad);
	else {
		if (WARN_ON(fmt->pad >= sd->entity.num_pads))
			mf = &pipe->cfg[0].mbus_fmt;
		else
			mf = &pipe->cfg[fmt->pad].mbus_fmt;
	}

	fmt->format = *mf;
	dev_dbg(mraw->cam_dev, "sd:%s pad:%d get format 0x%x\n",
		sd->name, fmt->pad, fmt->format.code);

	return 0;
}

static int mtk_mraw_media_link_setup(struct media_entity *entity,
				    const struct media_pad *local,
				    const struct media_pad *remote, u32 flags)
{
	struct mtk_mraw_pipeline *pipe =
		container_of(entity, struct mtk_mraw_pipeline, subdev.entity);
	struct mtk_mraw *mraw = pipe->mraw;
	u32 pad = local->index;

	dev_info(mraw->cam_dev, "%s: mraw %d: %d->%d flags:0x%x\n",
		__func__, pipe->id, remote->index, local->index, flags);

	if (pad == MTK_MRAW_SINK)
		pipe->seninf_padidx = remote->index;

	if (pad < MTK_MRAW_PIPELINE_PADS_NUM && pad != MTK_MRAW_SINK)
		pipe->vdev_nodes[pad-MTK_MRAW_SINK_NUM].enabled =
			!!(flags & MEDIA_LNK_FL_ENABLED);

	if (!(flags & MEDIA_LNK_FL_ENABLED))
		memset(pipe->cfg, 0, sizeof(pipe->cfg));

	return 0;
}

static const struct v4l2_subdev_core_ops mtk_mraw_subdev_core_ops = {
	.subscribe_event = mtk_mraw_sd_subscribe_event,
	.unsubscribe_event = v4l2_event_subdev_unsubscribe,
};

static const struct v4l2_subdev_video_ops mtk_mraw_subdev_video_ops = {
	.s_stream =  mtk_mraw_sd_s_stream,
};

static const struct v4l2_subdev_pad_ops mtk_mraw_subdev_pad_ops = {
	.link_validate = mtk_cam_link_validate,
	.init_cfg = mtk_mraw_init_cfg,
	.set_fmt = mtk_mraw_set_fmt,
	.get_fmt = mtk_mraw_get_fmt,
};

static const struct v4l2_subdev_ops mtk_mraw_subdev_ops = {
	.core = &mtk_mraw_subdev_core_ops,
	.video = &mtk_mraw_subdev_video_ops,
	.pad = &mtk_mraw_subdev_pad_ops,
};

static const struct media_entity_operations mtk_mraw_media_entity_ops = {
	.link_setup = mtk_mraw_media_link_setup,
	.link_validate = v4l2_subdev_link_validate,
};

static const struct v4l2_ioctl_ops mtk_mraw_v4l2_meta_cap_ioctl_ops = {
	.vidioc_querycap = mtk_cam_vidioc_querycap,
	.vidioc_enum_fmt_meta_cap = mtk_cam_vidioc_meta_enum_fmt,
	.vidioc_g_fmt_meta_cap = mtk_cam_vidioc_g_meta_fmt,
	.vidioc_s_fmt_meta_cap = mtk_cam_vidioc_g_meta_fmt,
	.vidioc_try_fmt_meta_cap = mtk_cam_vidioc_g_meta_fmt,
	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = vb2_ioctl_qbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,
	.vidioc_expbuf = vb2_ioctl_expbuf,
};

static const struct v4l2_ioctl_ops mtk_mraw_v4l2_meta_out_ioctl_ops = {
	.vidioc_querycap = mtk_cam_vidioc_querycap,
	.vidioc_enum_fmt_meta_out = mtk_cam_vidioc_meta_enum_fmt,
	.vidioc_g_fmt_meta_out = mtk_cam_vidioc_g_meta_fmt,
	.vidioc_s_fmt_meta_out = mtk_cam_vidioc_g_meta_fmt,
	.vidioc_try_fmt_meta_out = mtk_cam_vidioc_g_meta_fmt,
	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = vb2_ioctl_qbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,
	.vidioc_expbuf = vb2_ioctl_expbuf,
};

static const struct mtk_cam_format_desc meta_fmts[] = { /* FIXME for ISP6 meta format */
	{
		.vfmt.fmt.meta = {
			.dataformat = V4L2_META_FMT_MTISP_PARAMS,
			.buffersize = MRAW_STATS_CFG_SIZE,
		},
	},
	{
		.vfmt.fmt.meta = {
			.dataformat = V4L2_META_FMT_MTISP_PARAMS,
			.buffersize = MRAW_STATS_0_SIZE,
		},
	},
};

#define  MTK_MRAW_TOTAL_OUTPUT_QUEUES 1

static const struct
mtk_cam_dev_node_desc mraw_output_queues[] = {
	{
		.id = MTK_MRAW_META_IN,
		.name = "meta input",
		.cap = V4L2_CAP_META_OUTPUT,
		.buf_type = V4L2_BUF_TYPE_META_OUTPUT,
		.link_flags = MEDIA_LNK_FL_ENABLED |  MEDIA_LNK_FL_IMMUTABLE,
		.image = false,
#ifdef CONFIG_MTK_SCP
		.smem_alloc = true,
#else
		.smem_alloc = false,
#endif
		.dma_port = MTKCAM_IPI_MRAW_META_STATS_CFG,
		.fmts = meta_fmts,
		.default_fmt_idx = 0,
		.max_buf_count = 16,
		.ioctl_ops = &mtk_mraw_v4l2_meta_out_ioctl_ops,
	},
};

static const char *mraw_output_queue_names[MRAW_PIPELINE_NUM]
	[MTK_MRAW_TOTAL_OUTPUT_QUEUES] = {
	{"mtk-cam mraw-0 meta-input"},
	{"mtk-cam mraw-1 meta-input"},
	{"mtk-cam mraw-2 meta-input"},
	{"mtk-cam mraw-3 meta-input"}
};

#define MTK_MRAW_TOTAL_CAPTURE_QUEUES 1

static const struct
mtk_cam_dev_node_desc mraw_capture_queues[] = {
	{
		.id = MTK_MRAW_META_OUT,
		.name = "meta output",
		.cap = V4L2_CAP_META_CAPTURE,
		.buf_type = V4L2_BUF_TYPE_META_CAPTURE,
		.link_flags = MEDIA_LNK_FL_ENABLED |  MEDIA_LNK_FL_IMMUTABLE,
		.image = false,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_MRAW_META_STATS_0,
		.fmts = meta_fmts,
		.default_fmt_idx = 1,
		.max_buf_count = 16,
		.ioctl_ops = &mtk_mraw_v4l2_meta_cap_ioctl_ops,
	}
};

static const char *mraw_capture_queue_names[MRAW_PIPELINE_NUM]
	[MTK_MRAW_TOTAL_CAPTURE_QUEUES] = {
	{"mtk-cam mraw-0 partial-meta-0"},
	{"mtk-cam mraw-1 partial-meta-0"},
	{"mtk-cam mraw-2 partial-meta-0"},
	{"mtk-cam mraw-3 partial-meta-0"}
};

void apply_mraw_cq(struct mtk_mraw_device *dev,
	      dma_addr_t cq_addr, unsigned int cq_size, unsigned int cq_offset,
	      int initial)
{
#define CQ_VADDR_MASK 0xffffffff
	u32 cq_addr_lsb = (cq_addr + cq_offset) & CQ_VADDR_MASK;
	u32 cq_addr_msb = ((cq_addr + cq_offset) >> 32);

	dev_dbg(dev->dev,
		"apply mraw%d cq - addr:0x%llx ,size:%d,offset:%d, REG_MRAW_CQ_THR0_CTL:0x%8x\n",
		dev->id, cq_addr, cq_size, cq_offset,
		readl_relaxed(dev->base + REG_MRAW_CQ_THR0_CTL));

	if (cq_size == 0)
		return;

	writel_relaxed(cq_addr_lsb, dev->base + REG_MRAW_CQ_THR0_BASEADDR);
	writel_relaxed(cq_addr_msb, dev->base + REG_MRAW_CQ_THR0_BASEADDR_MSB);
	writel_relaxed(cq_size, dev->base + REG_MRAW_CQ_THR0_DESC_SIZE);

	wmb(); /* TBC */
	if (initial) {
		writel_relaxed(MRAWCTL_CQ_THR0_DONE_ST,
			       dev->base + REG_MRAW_CTL_INT6_EN);
		writel_relaxed(MRAWCTL_CQ_THR0_START,
			       dev->base + REG_MRAW_CTL_START);
		wmb(); /* TBC */
	} else {
#if USING_MRAW_SCQ
		writel_relaxed(MRAWCTL_CQ_THR0_START, dev->base + REG_MRAW_CTL_START);
		wmb(); /* TBC */
#endif
	}
	dev_dbg(dev->dev,
		"apply mraw%d scq - addr/size = [main] 0x%llx/%d\n",
		dev->id, cq_addr, cq_size);
}

static unsigned int mtk_cam_mraw_powi(unsigned int x, unsigned int n)
{
	unsigned int rst = 1.0;
	unsigned int m = (n >= 0) ? n : -n;

	while (m--)
		rst *= x;

	return (n >= 0) ? rst : 1.0 / rst;
}

static unsigned int mtk_cam_mraw_xsize_cal(unsigned int length)
{
	return length * 16 / 8;
}

static unsigned int mtk_cam_mraw_xsize_cal_cpio(unsigned int length)
{
	return (length + 7) / 8;
}

static void mtk_cam_mraw_set_dense_fmt(
	struct device *dev, unsigned int *tg_width_temp,
	unsigned int *tg_height_temp,
	struct mtk_cam_mraw_resource_config *res_config, unsigned int dmao)
{
	if (dmao == IMGO) {
		if (res_config->mbn_pow < 2 || res_config->mbn_pow > 6) {
			dev_info(dev, "%s:Invalid mbn_pow: %d",
				__func__, res_config->mbn_pow);
			return;
		}
		switch (res_config->mbn_dir) {
		case MBN_POW_VERTICAL:
			*tg_height_temp /= mtk_cam_mraw_powi(2, res_config->mbn_pow);
			break;
		case MBN_POW_HORIZONTAL:
			*tg_width_temp /= mtk_cam_mraw_powi(2, res_config->mbn_pow);
			break;
		default:
			dev_info(dev, "%s:MBN's dir %d %s fail",
				__func__, res_config->mbn_dir, "unknown idx");
			return;
		}
		 // divided for 2 path from MBN
		*tg_width_temp /= 2;
	} else if (dmao == CPIO) {
		if (res_config->cpi_pow < 2 || res_config->cpi_pow > 6) {
			dev_info(dev, "Invalid cpi_pow: %d", res_config->cpi_pow);
			return;
		}
		switch (res_config->cpi_dir) {
		case CPI_POW_VERTICAL:
			*tg_height_temp /= mtk_cam_mraw_powi(2, res_config->cpi_pow);
			break;
		case CPI_POW_HORIZONTAL:
			*tg_width_temp /= mtk_cam_mraw_powi(2, res_config->cpi_pow);
			break;
		default:
			dev_info(dev, "%s:CPI's dir %d %s fail",
				__func__, res_config->cpi_dir, "unknown idx");
			return;
		}
	}
}

static void mtk_cam_mraw_set_concatenation_fmt(
	struct device *dev, unsigned int *tg_width_temp,
	unsigned int *tg_height_temp,
	struct mtk_cam_mraw_resource_config *res_config, unsigned int dmao)
{
	if (dmao == IMGO) {
		if (res_config->mbn_spar_pow < 1 || res_config->mbn_spar_pow > 6) {
			dev_info(dev, "%s:Invalid mbn_spar_pow: %d",
				__func__, res_config->mbn_spar_pow);
			return;
		}
		// concatenated
		*tg_width_temp *= res_config->mbn_spar_fac;
		*tg_height_temp /= res_config->mbn_spar_fac;

		// vertical binning
		*tg_height_temp /= mtk_cam_mraw_powi(2, res_config->mbn_spar_pow);

		// divided for 2 path from MBN
		*tg_width_temp /= 2;
	} else if (dmao == CPIO) {
		if (res_config->cpi_spar_pow < 1 || res_config->cpi_spar_pow > 6) {
			dev_info(dev, "%s:Invalid cpi_spar_pow: %d",
				__func__, res_config->cpi_spar_pow);
			return;
		}
		// concatenated
		*tg_width_temp *= res_config->cpi_spar_fac;
		*tg_height_temp /= res_config->cpi_spar_fac;

		// vertical binning
		*tg_height_temp /= mtk_cam_mraw_powi(2, res_config->cpi_spar_pow);
	}
}

static void mtk_cam_mraw_set_interleving_fmt(
	unsigned int *tg_width_temp,
	unsigned int *tg_height_temp, unsigned int dmao)
{
	if (dmao == IMGO)
		*tg_height_temp /= 2; // divided for 2 path from MBN
	else if (dmao == CPIO) {
		// concatenated
		*tg_width_temp *= 2;
		*tg_height_temp /= 2;
	}
}

static void mtk_cam_mraw_set_mraw_dmao_info(
	struct device *dev, struct mtk_cam_mraw_resource_config *res_config,
	struct mtk_cam_mraw_dmao_info *mraw_dmao_info)
{
	unsigned int tg_width_mqe = res_config->width;
	unsigned int tg_height_mqe = res_config->height;
	unsigned int tg_width_temp;
	unsigned int tg_height_temp;
	int i;

	if (res_config->mqe_en) {
		switch (res_config->mqe_mode) {
		case UL_MODE:
		case UR_MODE:
		case DL_MODE:
		case DR_MODE:
			tg_width_mqe /= 2;
			tg_height_mqe /= 2;
			break;
		case PD_L_MODE:
		case PD_R_MODE:
		case PD_M_MODE:
		case PD_B01_MODE:
		case PD_B02_MODE:
			tg_width_mqe /= 2;
			break;
		default:
			dev_info(dev, "%s:MQE-Mode %d %s fail\n",
				__func__, res_config->mqe_mode, "unknown idx");
			return;
		}
	}

	/* cal. for IMGO */
	tg_width_temp = tg_width_mqe;
	tg_height_temp = tg_height_mqe;
	switch (res_config->mbn_dir) {
	case MBN_POW_VERTICAL:
	case MBN_POW_HORIZONTAL:
		mtk_cam_mraw_set_dense_fmt(dev, &tg_width_temp,
			&tg_height_temp, res_config, IMGO);
		break;
	case MBN_POW_SPARSE_CONCATENATION:
		mtk_cam_mraw_set_concatenation_fmt(
			dev, &tg_width_temp, &tg_height_temp, res_config, IMGO);
		break;
	case MBN_POW_SPARSE_INTERLEVING:
		mtk_cam_mraw_set_interleving_fmt(&tg_width_temp, &tg_height_temp, IMGO);
		break;
	default:
		dev_info(dev, "%s:MBN's dir %d %s fail",
			__func__, res_config->mbn_dir, "unknown idx");
		return;
	}

	mraw_dmao_info->dmao_width[IMGO] =
		mtk_cam_mraw_xsize_cal(tg_width_temp);
	mraw_dmao_info->dmao_height[IMGO] = tg_height_temp;
	mraw_dmao_info->dmao_stride[IMGO] = mraw_dmao_info->dmao_width[IMGO];

	/* IMGBO's w/h is the same as IMGO */
	mraw_dmao_info->dmao_width[IMGBO]
		= mtk_cam_mraw_xsize_cal(tg_width_temp);
	mraw_dmao_info->dmao_height[IMGBO] = tg_height_temp;
	mraw_dmao_info->dmao_stride[IMGBO] = mraw_dmao_info->dmao_width[IMGBO];

	/* cal. for CPIO */
	tg_width_temp = tg_width_mqe;
	tg_height_temp = tg_height_mqe;
	switch (res_config->cpi_dir) {
	case CPI_POW_VERTICAL:
	case CPI_POW_HORIZONTAL:
		mtk_cam_mraw_set_dense_fmt(dev, &tg_width_temp,
			&tg_height_temp, res_config, CPIO);
		break;
	case CPI_POW_SPARSE_CONCATENATION:
		mtk_cam_mraw_set_concatenation_fmt(dev, &tg_width_temp,
			&tg_height_temp, res_config, CPIO);
		break;
	case CPI_POW_SPARSE_INTERLEVING:
		mtk_cam_mraw_set_interleving_fmt(&tg_width_temp, &tg_height_temp, CPIO);
		break;
	default:
		dev_info(dev, "%s:CPI's dir %d %s fail",
			__func__, res_config->cpi_dir, "unknown idx");
		return;
	}

	mraw_dmao_info->dmao_width[CPIO] =
		mtk_cam_mraw_xsize_cal_cpio(tg_width_temp);
	mraw_dmao_info->dmao_height[CPIO] = tg_height_temp;
	mraw_dmao_info->dmao_stride[CPIO] = mraw_dmao_info->dmao_width[CPIO];

	for (i = DMAO_ID_BEGIN; i < DMAO_ID_MAX; i++) {
		dev_dbg(dev, "dma_id:%d, w:%d s:%d stride:%d\n",
			i, mraw_dmao_info->dmao_width[i],
			mraw_dmao_info->dmao_height[i],
			mraw_dmao_info->dmao_stride[i]);
	}
}

static void mtk_cam_mraw_copy_user_input_param(
	struct device *dev,
	struct mtk_cam_uapi_meta_mraw_stats_cfg *mraw_meta_in_buf,
	struct mtkcam_ipi_frame_param *frame_param,
	struct mtk_mraw_pipeline *mraw_pipline)
{
	/* Set tg/crp param from kernel info. */
	mtk_cam_mraw_update_param(frame_param, mraw_pipline);

	/* Set mraw res_config */
	mraw_pipline->res_config.mqe_en = mraw_meta_in_buf->mqe_enable;
	mraw_pipline->res_config.mqe_mode = mraw_meta_in_buf->mqe_param.mqe_mode;

	mraw_pipline->res_config.mbn_dir = mraw_meta_in_buf->mbn_param.mbn_dir;
	mraw_pipline->res_config.mbn_pow = mraw_meta_in_buf->mbn_param.mbn_pow;
	mraw_pipline->res_config.cpi_pow = mraw_meta_in_buf->cpi_param.cpi_pow;
	mraw_pipline->res_config.cpi_dir = mraw_meta_in_buf->cpi_param.cpi_dir;

	mraw_pipline->res_config.mbn_spar_pow = mraw_meta_in_buf->mbn_param.mbn_spar_pow;
	mraw_pipline->res_config.mbn_spar_fac = mraw_meta_in_buf->mbn_param.mbn_spar_fac;
	mraw_pipline->res_config.cpi_spar_pow = mraw_meta_in_buf->cpi_param.cpi_spar_pow;
	mraw_pipline->res_config.cpi_spar_fac = mraw_meta_in_buf->cpi_param.cpi_spar_fac;

	dev_dbg(dev, "%s:enable:(%d,%d,%d,%d) mqe_mode:%d mbn_dir/pow:(%d,%d) cpi_dir/pow:(%d,%d) mbn_spar(%x,%x,%x,%x,%x) cpi_spar(%x,%x,%x,%x,%x,%x)\n",
		__func__, mraw_meta_in_buf->mqe_enable, mraw_meta_in_buf->mobc_enable,
		mraw_meta_in_buf->lsc_enable, mraw_meta_in_buf->lsci_enable,
		mraw_meta_in_buf->mqe_param.mqe_mode,
		mraw_meta_in_buf->mbn_param.mbn_dir, mraw_meta_in_buf->mbn_param.mbn_pow,
		mraw_meta_in_buf->cpi_param.cpi_dir, mraw_meta_in_buf->cpi_param.cpi_pow,
		mraw_meta_in_buf->mbn_param.mbn_spar_hei,
		mraw_meta_in_buf->mbn_param.mbn_spar_pow,
		mraw_meta_in_buf->mbn_param.mbn_spar_fac,
		mraw_meta_in_buf->mbn_param.mbn_spar_con1,
		mraw_meta_in_buf->mbn_param.mbn_spar_con0,
		mraw_meta_in_buf->cpi_param.cpi_th,
		mraw_meta_in_buf->cpi_param.cpi_spar_hei, mraw_meta_in_buf->cpi_param.cpi_spar_pow,
		mraw_meta_in_buf->cpi_param.cpi_spar_fac, mraw_meta_in_buf->cpi_param.cpi_spar_con1,
		mraw_meta_in_buf->cpi_param.cpi_spar_con0);
}

static void mtk_cam_mraw_set_frame_param_dmao(
	struct device *dev,
	struct mtkcam_ipi_frame_param *frame_param,
	struct mtk_cam_mraw_dmao_info mraw_dmao_info, int pipe_id,
	dma_addr_t buf_daddr)
{
	struct mtkcam_ipi_img_output *mraw_img_outputs, *last_mraw_img_outputs;
	int i;

	for (i = DMAO_ID_BEGIN; i < DMAO_ID_MAX; i++) {
		last_mraw_img_outputs = mraw_img_outputs;
		mraw_img_outputs = &frame_param->mraw_param[0].mraw_img_outputs[i];

		mraw_img_outputs->uid.id = MTKCAM_IPI_MRAW_META_STATS_0;
		mraw_img_outputs->uid.pipe_id = pipe_id;

		mraw_img_outputs->fmt.stride[0] = mraw_dmao_info.dmao_stride[i];
		mraw_img_outputs->fmt.s.w = mraw_dmao_info.dmao_width[i];
		mraw_img_outputs->fmt.s.h = mraw_dmao_info.dmao_height[i];

		mraw_img_outputs->crop.p.x = 0;
		mraw_img_outputs->crop.p.y = 0;
		mraw_img_outputs->crop.s.w = mraw_dmao_info.dmao_width[i];
		mraw_img_outputs->crop.s.h = mraw_dmao_info.dmao_height[i];

		if (i == 0)
			mraw_img_outputs->buf[0][0].iova
				= buf_daddr + sizeof(struct mtk_cam_uapi_meta_mraw_stats_0);
		else
			mraw_img_outputs->buf[0][0].iova = last_mraw_img_outputs->buf[0][0].iova
				+ last_mraw_img_outputs->fmt.stride[0] *
					last_mraw_img_outputs->fmt.s.h;

		mraw_img_outputs->buf[0][0].size =
			mraw_img_outputs->fmt.stride[0] * mraw_img_outputs->fmt.s.h;

		dev_dbg(dev, "%s:dmao_id:%d iova:0x%llx size:%d\n",
			__func__, i, mraw_img_outputs->buf[0][0].iova,
			mraw_img_outputs->buf[0][0].size);
	}
}

static void set_payload(struct mtk_cam_uapi_meta_hw_buf *buf,
			unsigned int size, unsigned long *offset)
{
	buf->offset = *offset;
	buf->size = size;
	*offset += size;
}

static void mtk_cam_mraw_set_meta_stats_info(
	void *vaddr, struct mtk_cam_mraw_dmao_info *mraw_dmao_info)
{
	struct mtk_cam_uapi_meta_mraw_stats_0 *mraw_stats0;
	unsigned long offset;
	unsigned int size;

	mraw_stats0 = (struct mtk_cam_uapi_meta_mraw_stats_0 *)vaddr;
	offset = sizeof(*mraw_stats0);
	/* imgo */
	size = mraw_dmao_info->dmao_stride[IMGO] * mraw_dmao_info->dmao_height[IMGO];
	set_payload(&mraw_stats0->pdp_0_stats.pdo_buf, size, &offset);
	mraw_stats0->pdp_0_stats_enabled = 1;
	mraw_stats0->pdp_0_stats.stats_src.width = mraw_dmao_info->dmao_width[IMGO];
	mraw_stats0->pdp_0_stats.stats_src.height = mraw_dmao_info->dmao_height[IMGO];
	mraw_stats0->pdp_0_stats.stride = mraw_dmao_info->dmao_stride[IMGO];
	/* imgbo */
	size = mraw_dmao_info->dmao_stride[IMGBO] * mraw_dmao_info->dmao_height[IMGBO];
	set_payload(&mraw_stats0->pdp_1_stats.pdo_buf, size, &offset);
	mraw_stats0->pdp_1_stats_enabled = 1;
	mraw_stats0->pdp_1_stats.stats_src.width = mraw_dmao_info->dmao_width[IMGBO];
	mraw_stats0->pdp_1_stats.stats_src.height = mraw_dmao_info->dmao_height[IMGBO];
	mraw_stats0->pdp_1_stats.stride = mraw_dmao_info->dmao_stride[IMGBO];
	/* cpio */
	size = mraw_dmao_info->dmao_stride[CPIO] * mraw_dmao_info->dmao_height[CPIO];
	set_payload(&mraw_stats0->cpi_stats.cpio_buf, size, &offset);
	mraw_stats0->cpi_stats_enabled = 1;
	mraw_stats0->cpi_stats.stats_src.width = mraw_dmao_info->dmao_width[CPIO];
	mraw_stats0->cpi_stats.stats_src.height = mraw_dmao_info->dmao_height[CPIO];
	mraw_stats0->cpi_stats.stride = mraw_dmao_info->dmao_stride[CPIO];
}

void mtk_cam_mraw_handle_enque(struct vb2_buffer *vb)
{
	struct mtk_cam_device *cam = vb2_get_drv_priv(vb->vb2_queue);
	struct mtk_cam_video_device *node = mtk_cam_vbq_to_vdev(vb->vb2_queue);
	struct mtk_cam_buffer *buf = mtk_cam_vb2_buf_to_dev_buf(vb);
	struct mtk_cam_request *req = to_mtk_cam_req(vb->request);
	void *vaddr = vb2_plane_vaddr(vb, 0);
	unsigned int pipe_id = node->uid.pipe_id;
	unsigned int dma_port = node->desc.dma_port;
	struct mtk_cam_request_stream_data *req_stream_data
		= mtk_cam_req_get_s_data(req, pipe_id, 0);
	struct device *dev = cam->dev;
	struct mtkcam_ipi_meta_input *meta_in;
	struct mtkcam_ipi_frame_param *frame_param;
	struct mtk_cam_uapi_meta_mraw_stats_cfg *mraw_meta_in_buf;
	struct mtk_mraw_pipeline *mraw_pipline;

	frame_param = &req_stream_data->frame_params;
	mraw_pipline = mtk_cam_dev_get_mraw_pipeline(cam, pipe_id);

	/* TODO: not proper to convert perframe data by using pipeline variable */
	switch (dma_port) {
	case MTKCAM_IPI_MRAW_META_STATS_CFG:
		mraw_meta_in_buf = (struct mtk_cam_uapi_meta_mraw_stats_cfg *)vaddr;
		mraw_pipline->res_config.vaddr[MTKCAM_IPI_MRAW_META_STATS_CFG
			- MTKCAM_IPI_MRAW_ID_START] = vaddr;
		mraw_pipline->res_config.daddr[MTKCAM_IPI_MRAW_META_STATS_CFG
			- MTKCAM_IPI_MRAW_ID_START] = buf->daddr;
		meta_in = &frame_param->mraw_param[0].mraw_meta_inputs;
		meta_in->buf.ccd_fd = vb->planes[0].m.fd;
		meta_in->buf.size = node->active_fmt.fmt.meta.buffersize;
		meta_in->buf.iova = buf->daddr;
		meta_in->uid.id = dma_port;
		meta_in->uid.pipe_id = node->uid.pipe_id;
		mtk_cam_mraw_copy_user_input_param(cam->dev, mraw_meta_in_buf,
			frame_param, mraw_pipline);
		mraw_pipline->res_config.enque_num++;
		break;
	case MTKCAM_IPI_MRAW_META_STATS_0:
		mraw_pipline->res_config.vaddr[MTKCAM_IPI_MRAW_META_STATS_0
			- MTKCAM_IPI_MRAW_ID_START] = vaddr;
		mraw_pipline->res_config.daddr[MTKCAM_IPI_MRAW_META_STATS_0
			- MTKCAM_IPI_MRAW_ID_START] = buf->daddr;
		mraw_pipline->res_config.enque_num++;
		break;
	default:
		dev_dbg(dev, "%s:pipe(%d):buffer with invalid port(%d)\n",
			__func__, pipe_id, dma_port);
		break;
	}

	dev_dbg(dev, "%s:pipe(%d) dma_port(%d) buf_length(%d)\n",
		__func__, pipe_id, dma_port, vb->planes[0].length);

	if (mraw_pipline->res_config.enque_num == MAX_MRAW_VIDEO_DEV_NUM) {
		mtk_cam_mraw_cal_cfg_info(cam, pipe_id,
			req_stream_data, mraw_pipline->res_config.is_initial);
		mraw_pipline->res_config.is_initial = 0;
		mraw_pipline->res_config.enque_num = 0;
	}
}

int mtk_cam_mraw_cal_cfg_info(struct mtk_cam_device *cam,
	unsigned int pipe_id, struct mtk_cam_request_stream_data *s_data,
	unsigned int is_config)
{
	struct mtk_cam_mraw_dmao_info mraw_dmao_info;
	struct mtk_mraw_pipeline *pipe =
		&cam->mraw.pipelines[pipe_id - MTKCAM_SUBDEV_MRAW_START];

	s_data->frame_params.mraw_param[0].is_config = is_config;

	mtk_cam_mraw_set_mraw_dmao_info(cam->dev,
		&pipe->res_config, &mraw_dmao_info);
	mtk_cam_mraw_set_frame_param_dmao(cam->dev, &s_data->frame_params,
		mraw_dmao_info, pipe->id,
		pipe->res_config.daddr[MTKCAM_IPI_MRAW_META_STATS_0
			- MTKCAM_IPI_MRAW_ID_START]);
	mtk_cam_mraw_set_meta_stats_info(
		pipe->res_config.vaddr[MTKCAM_IPI_MRAW_META_STATS_0
			- MTKCAM_IPI_MRAW_ID_START], &mraw_dmao_info);

	return 0;
}

void mtk_cam_mraw_update_param(struct mtkcam_ipi_frame_param *frame_param,
	struct mtk_mraw_pipeline *mraw_pipline)
{
	frame_param->mraw_param[0].tg_pos_x = 0;
	frame_param->mraw_param[0].tg_pos_y = 0;
	frame_param->mraw_param[0].tg_size_w = mraw_pipline->res_config.width;
	frame_param->mraw_param[0].tg_size_h = mraw_pipline->res_config.height;
	frame_param->mraw_param[0].tg_fmt = mraw_pipline->res_config.img_fmt;
	frame_param->mraw_param[0].crop_pos_x = 0;
	frame_param->mraw_param[0].crop_pos_y = 0;
	frame_param->mraw_param[0].crop_size_w = mraw_pipline->res_config.width;
	frame_param->mraw_param[0].crop_size_h = mraw_pipline->res_config.height;
}

int mtk_cam_mraw_update_all_buffer_ts(struct mtk_cam_ctx *ctx, u64 ts_ns)
{
	struct mtk_mraw_working_buf_entry *buf_entry;
	int i;

	for (i = 0; i < ctx->used_mraw_num; i++) {
		spin_lock(&ctx->mraw_composed_buffer_list[i].lock);
		if (list_empty(&ctx->mraw_composed_buffer_list[i].list)) {
			spin_unlock(&ctx->mraw_composed_buffer_list[i].lock);
			return 0;
		}
		buf_entry = list_first_entry(&ctx->mraw_composed_buffer_list[i].list,
							struct mtk_mraw_working_buf_entry,
							list_entry);
		buf_entry->ts_raw = ts_ns;
		spin_unlock(&ctx->mraw_composed_buffer_list[i].lock);
	}

	return 1;
}

int mtk_cam_mraw_apply_all_buffers(struct mtk_cam_ctx *ctx)
{
	struct mtk_mraw_working_buf_entry *buf_entry, *buf_entry_prev;
	struct mtk_mraw_device *mraw_dev;
	int i;

	for (i = 0; i < ctx->used_mraw_num; i++) {
		mraw_dev = get_mraw_dev(ctx->cam, ctx->mraw_pipe[i]);

		spin_lock(&ctx->mraw_composed_buffer_list[i].lock);
		if (list_empty(&ctx->mraw_composed_buffer_list[i].list)) {
			spin_unlock(&ctx->mraw_composed_buffer_list[i].lock);
			return 0;
		}
		list_for_each_entry_safe(buf_entry, buf_entry_prev,
			&ctx->mraw_composed_buffer_list[i].list, list_entry) {
			if (atomic_read(&buf_entry->is_apply) == 0) {
				atomic_set(&buf_entry->is_apply, 1);
				break;
			}
		}
		buf_entry = list_first_entry(&ctx->mraw_composed_buffer_list[i].list,
							struct mtk_mraw_working_buf_entry,
							list_entry);
		if (mtk_cam_mraw_is_vf_on(mraw_dev) &&
			buf_entry->s_data->req->pipe_used &
			(1 << ctx->mraw_pipe[i]->id)) {
			if (buf_entry->is_stagger == 0 ||
				(buf_entry->is_stagger == 1 && STAGGER_CQ_LAST_SOF == 0)) {
				if ((buf_entry->ts_mraw == 0) ||
					((buf_entry->ts_mraw < buf_entry->ts_raw) &&
					((buf_entry->ts_raw - buf_entry->ts_mraw) > 3000000))) {
					dev_dbg(ctx->cam->dev, "%s pipe_id:%d ts_raw:%lld ts_mraw:%lld is_apply:%d",
						__func__, ctx->mraw_pipe[i]->id,
						buf_entry->ts_raw, buf_entry->ts_mraw,
						atomic_read(&buf_entry->is_apply));
					spin_unlock(&ctx->mraw_composed_buffer_list[i].lock);
					continue;
				}
			}
		}
		list_del(&buf_entry->list_entry);
		ctx->mraw_composed_buffer_list[i].cnt--;
		spin_unlock(&ctx->mraw_composed_buffer_list[i].lock);
		spin_lock(&ctx->mraw_processing_buffer_list[i].lock);
		list_add_tail(&buf_entry->list_entry,
					&ctx->mraw_processing_buffer_list[i].list);
		ctx->mraw_processing_buffer_list[i].cnt++;
		spin_unlock(&ctx->mraw_processing_buffer_list[i].lock);

		if (buf_entry->s_data->req->pipe_used &
			(1 << ctx->mraw_pipe[i]->id)) {
			apply_mraw_cq(mraw_dev,
				buf_entry->buffer.iova,
				buf_entry->mraw_cq_desc_size,
				buf_entry->mraw_cq_desc_offset, 0);
		} else {
			mtk_cam_mraw_vf_on(mraw_dev, 0);
		}
	}

	return 1;
}

int mtk_cam_mraw_apply_next_buffer(struct mtk_cam_ctx *ctx,
	unsigned int pipe_id, u64 ts_ns)
{
	struct mtk_mraw_working_buf_entry *buf_entry;
	struct mtk_mraw_device *mraw_dev;
	int i;

	for (i = 0; i < ctx->used_mraw_num; i++) {
		if (ctx->mraw_pipe[i]->id == pipe_id) {
			spin_lock(&ctx->mraw_composed_buffer_list[i].lock);
			if (list_empty(&ctx->mraw_composed_buffer_list[i].list)) {
				spin_unlock(&ctx->mraw_composed_buffer_list[i].lock);
				return 0;
			}
			buf_entry = list_first_entry(&ctx->mraw_composed_buffer_list[i].list,
								struct mtk_mraw_working_buf_entry,
								list_entry);
			buf_entry->ts_mraw = ts_ns;
			if ((buf_entry->ts_raw == 0) ||
				(atomic_read(&buf_entry->is_apply) == 0) ||
				((buf_entry->ts_mraw < buf_entry->ts_raw) &&
				((buf_entry->ts_raw - buf_entry->ts_mraw) > 3000000))) {
				dev_dbg(ctx->cam->dev, "%s pipe_id:%d ts_raw:%lld ts_mraw:%lld",
					__func__, ctx->mraw_pipe[i]->id,
					buf_entry->ts_raw, buf_entry->ts_mraw);
				spin_unlock(&ctx->mraw_composed_buffer_list[i].lock);
				return 1;
			}
			list_del(&buf_entry->list_entry);
			ctx->mraw_composed_buffer_list[i].cnt--;
			spin_unlock(&ctx->mraw_composed_buffer_list[i].lock);
			spin_lock(&ctx->mraw_processing_buffer_list[i].lock);
			list_add_tail(&buf_entry->list_entry,
						&ctx->mraw_processing_buffer_list[i].list);
			ctx->mraw_processing_buffer_list[i].cnt++;
			spin_unlock(&ctx->mraw_processing_buffer_list[i].lock);

			mraw_dev = get_mraw_dev(ctx->cam, ctx->mraw_pipe[i]);
			if (buf_entry->s_data->req->pipe_used &
				(1 << ctx->mraw_pipe[i]->id)) {
				apply_mraw_cq(mraw_dev,
					buf_entry->buffer.iova,
					buf_entry->mraw_cq_desc_size,
					buf_entry->mraw_cq_desc_offset, 0);
			} else {
				mtk_cam_mraw_vf_on(mraw_dev, 0);
			}
			break;
		}
	}

	return 1;
}

static int reset_msgfifo(struct mtk_mraw_device *dev)
{
	atomic_set(&dev->is_fifo_overflow, 0);
	return kfifo_init(&dev->msg_fifo, dev->msg_buffer, dev->fifo_size);
}

static int push_msgfifo(struct mtk_mraw_device *dev,
			struct mtk_camsys_irq_info *info)
{
	int len;

	if (unlikely(kfifo_avail(&dev->msg_fifo) < sizeof(*info))) {
		atomic_set(&dev->is_fifo_overflow, 1);
		return -1;
	}

	len = kfifo_in(&dev->msg_fifo, info, sizeof(*info));
	WARN_ON(len != sizeof(*info));

	return 0;
}

void mraw_reset(struct mtk_mraw_device *dev)
{
	int sw_ctl;
	int ret;

	dev_dbg(dev->dev, "%s\n", __func__);

	writel(0, dev->base + REG_MRAW_CTL_SW_CTL);
	writel(1, dev->base + REG_MRAW_CTL_SW_CTL);
	wmb(); /* make sure committed */

	ret = readx_poll_timeout(readl, dev->base + REG_MRAW_CTL_SW_CTL, sw_ctl,
				 sw_ctl & 0x2,
				 1 /* delay, us */,
				 100000 /* timeout, us */);
	if (ret < 0) {
		dev_info(dev->dev, "%s: timeout\n", __func__);

		dev_info(dev->dev,
			 "tg_sen_mode: 0x%x, ctl_en: 0x%x, ctl_sw_ctl:0x%x, frame_no:0x%x\n",
			 readl(dev->base + REG_MRAW_TG_SEN_MODE),
			 readl(dev->base + REG_MRAW_CTL_MOD_EN),
			 readl(dev->base + REG_MRAW_CTL_SW_CTL),
			 readl(dev->base + REG_MRAW_FRAME_SEQ_NUM)
			);

		mtk_smi_dbg_hang_detect("camsys-mraw");

		goto RESET_FAILURE;
	}

	// do hw rst
	writel(4, dev->base + REG_MRAW_CTL_SW_CTL);
	writel(0, dev->base + REG_MRAW_CTL_SW_CTL);
	wmb(); /* make sure committed */

RESET_FAILURE:
	return;
}

struct mtk_mraw_pipeline*
mtk_cam_dev_get_mraw_pipeline(struct mtk_cam_device *cam,
			     unsigned int pipe_id)
{
	if (pipe_id < MTKCAM_SUBDEV_MRAW_0 || pipe_id > MTKCAM_SUBDEV_MRAW_END)
		return NULL;
	else
		return &cam->mraw.pipelines[pipe_id - MTKCAM_SUBDEV_MRAW_0];
}


int mtk_cam_mraw_pipeline_config(
	struct mtk_cam_ctx *ctx, unsigned int idx)
{
	struct mtk_mraw_pipeline *mraw_pipe = ctx->mraw_pipe[idx];
	struct mtk_mraw *mraw = mraw_pipe->mraw;
	unsigned int i;
	int ret;

	/* reset pm_runtime during streaming dynamic change */
	if (ctx->streaming) {
		for (i = 0; i < ARRAY_SIZE(mraw->devs); i++)
			if (mraw_pipe->enabled_mraw & 1<<i)
				pm_runtime_put_sync(mraw->devs[i]);
	}

	ret = mtk_cam_mraw_select(mraw_pipe);
	if (ret) {
		dev_dbg(mraw->cam_dev, "failed select raw: %d\n",
			ctx->stream_id);
		return ret;
	}

	for (i = 0; i < ARRAY_SIZE(mraw->devs); i++)
		if (mraw_pipe->enabled_mraw & 1<<i)
			ret = pm_runtime_get_sync(mraw->devs[i]);

	if (ret < 0) {
		dev_dbg(mraw->cam_dev,
			"failed at pm_runtime_get_sync: %s\n",
			dev_driver_string(mraw->devs[i]));
		for (i = i-1; i >= 0; i--)
			if (mraw_pipe->enabled_mraw & 1<<i)
				pm_runtime_put_sync(mraw->devs[i]);
		return ret;
	}

	ctx->used_mraw_dev[idx] = mraw_pipe->enabled_mraw;
	dev_info(mraw->cam_dev, "ctx_id %d used_mraw_dev %d pipe_id %d\n",
		ctx->stream_id, ctx->used_mraw_dev[idx], mraw_pipe->id);
	return 0;
}


struct device *mtk_cam_find_mraw_dev(struct mtk_cam_device *cam
	, unsigned int mraw_mask)
{
	unsigned int i;

	for (i = 0; i < cam->num_mraw_drivers; i++) {
		if (mraw_mask & (1 << i))
			return cam->mraw.devs[i];
	}

	return NULL;
}

int mtk_cam_mraw_tg_config(struct mtk_mraw_device *dev,
	unsigned int pixel_mode)
{
	int ret = 0;

	switch (pixel_mode) {
	case 1:
		MRAW_WRITE_BITS(dev->base + REG_MRAW_TG_SEN_MODE,
			MRAW_TG_SEN_MODE, TG_DBL_DATA_BUS, 0);
		break;
	case 2:
		MRAW_WRITE_BITS(dev->base + REG_MRAW_TG_SEN_MODE,
			MRAW_TG_SEN_MODE, TG_DBL_DATA_BUS, 1);
		break;
	case 4:
		MRAW_WRITE_BITS(dev->base + REG_MRAW_TG_SEN_MODE,
			MRAW_TG_SEN_MODE, TG_DBL_DATA_BUS, 2);
		break;
	case 8:
		MRAW_WRITE_BITS(dev->base + REG_MRAW_TG_SEN_MODE,
			MRAW_TG_SEN_MODE, TG_DBL_DATA_BUS, 3);
		break;
	}

	return ret;
}

int mtk_cam_mraw_top_config(struct mtk_mraw_device *dev)
{
	int ret = 0;

	unsigned int int_en1 = (MRAW_INT_EN1_TG_ERR_EN |
							MRAW_INT_EN1_TG_GBERR_EN |
							MRAW_INT_EN1_TG_SOF_INT_EN |
							MRAW_INT_EN1_CQ_CODE_ERR_EN |
							MRAW_INT_EN1_CQ_DB_LOAD_ERR_EN |
							MRAW_INT_EN1_CQ_VS_ERR_EN |
							MRAW_INT_EN1_CQ_TRIG_DLY_INT_EN |
							MRAW_INT_EN1_SW_PASS1_DONE_EN |
							MRAW_INT_EN1_DMA_ERR_EN
							);

	unsigned int int_en5 = (MRAW_INT_EN5_IMGO_M1_ERR_EN |
							MRAW_INT_EN5_IMGBO_M1_ERR_EN |
							MRAW_INT_EN5_CPIO_M1_ERR_EN
							);

	/* int en */
	MRAW_WRITE_REG(dev->base + REG_MRAW_CTL_INT_EN, int_en1);
	MRAW_WRITE_REG(dev->base + REG_MRAW_CTL_INT5_EN, int_en5);

	MRAW_WRITE_BITS(dev->base + REG_MRAW_M_MRAWCTL_MISC,
		MRAW_CTL_MISC, MRAWCTL_DB_EN, 0);

	MRAW_WRITE_BITS(dev->base + REG_MRAW_M_MRAWCTL_MISC,
		MRAW_CTL_MISC, MRAWCTL_DB_LOAD_SRC, MRAW_DB_SRC_SOF);

	return ret;
}

int mtk_cam_mraw_dma_config(struct mtk_mraw_device *dev)
{
	int ret = 0;

	/* imgo con */
	MRAW_WRITE_REG(dev->base + REG_MRAW_M_IMGO_ORIWDMA_CON0,
		0x10000280);  // BURST_LEN and FIFO_SIZE
	MRAW_WRITE_REG(dev->base + REG_MRAW_M_IMGO_ORIWDMA_CON1,
		0x00A00150);  // Threshold for pre-ultra
	MRAW_WRITE_REG(dev->base + REG_MRAW_M_IMGO_ORIWDMA_CON2,
		0x014000F0);  // Threshold for ultra
	MRAW_WRITE_REG(dev->base + REG_MRAW_M_IMGO_ORIWDMA_CON3,
		0x01AB049E);  // Threshold for urgent
	MRAW_WRITE_REG(dev->base + REG_MRAW_M_IMGO_ORIWDMA_CON4,
		0x00500000);  // Threshold for DVFS

	/* imgbo con */
	MRAW_WRITE_REG(dev->base + REG_MRAW_M_IMGBO_ORIWDMA_CON0,
		0x10000280);  // BURST_LEN and FIFO_SIZE
	MRAW_WRITE_REG(dev->base + REG_MRAW_M_IMGBO_ORIWDMA_CON1,
		0x00A00150);  // Threshold for pre-ultra
	MRAW_WRITE_REG(dev->base + REG_MRAW_M_IMGBO_ORIWDMA_CON2,
		0x014000F0);  // Threshold for ultra
	MRAW_WRITE_REG(dev->base + REG_MRAW_M_IMGBO_ORIWDMA_CON3,
		0x01AB049E);  // Threshold for urgent
	MRAW_WRITE_REG(dev->base + REG_MRAW_M_IMGBO_ORIWDMA_CON4,
		0x00500000);  // Threshold for DVFS

	/* cpio con */
	MRAW_WRITE_REG(dev->base + REG_MRAW_M_CPIO_ORIWDMA_CON0,
		0x10000100);  // BURST_LEN and FIFO_SIZE
	MRAW_WRITE_REG(dev->base + REG_MRAW_M_CPIO_ORIWDMA_CON1,
		0x00400020);  // Threshold for pre-ultra
	MRAW_WRITE_REG(dev->base + REG_MRAW_M_CPIO_ORIWDMA_CON2,
		0x00800060);  // Threshold for ultra
	MRAW_WRITE_REG(dev->base + REG_MRAW_M_CPIO_ORIWDMA_CON3,
		0x018001D9);  // Threshold for urgent
	MRAW_WRITE_REG(dev->base + REG_MRAW_M_CPIO_ORIWDMA_CON4,
		0x00200000);  // Threshold for DVFS

	/* lsci con */
	MRAW_WRITE_REG(dev->base + REG_MRAW_M_LSCI_ORIRDMA_CON0,
		0x10000080);  // BURST_LEN and FIFO_SIZE
	MRAW_WRITE_REG(dev->base + REG_MRAW_M_LSCI_ORIRDMA_CON1,
		0x00200010);  // Threshold for pre-ultra
	MRAW_WRITE_REG(dev->base + REG_MRAW_M_LSCI_ORIRDMA_CON2,
		0x00400030);  // Threshold for ultra
	MRAW_WRITE_REG(dev->base + REG_MRAW_M_LSCI_ORIRDMA_CON3,
		0x00560046);  // Threshold for urgent
	MRAW_WRITE_REG(dev->base + REG_MRAW_M_LSCI_ORIRDMA_CON4,
		0x00100000);  // Threshold for DVFS

	/* cqi con */
	MRAW_WRITE_REG(dev->base + REG_MRAW_M1_CQI_ORIRDMA_CON0,
		0x10000040);  // BURST_LEN and FIFO_SIZE
	MRAW_WRITE_REG(dev->base + REG_MRAW_M1_CQI_ORIRDMA_CON1,
		0x00100008);  // Threshold for pre-ultra
	MRAW_WRITE_REG(dev->base + REG_MRAW_M1_CQI_ORIRDMA_CON2,
		0x00200018);  // Threshold for ultra
	MRAW_WRITE_REG(dev->base + REG_MRAW_M1_CQI_ORIRDMA_CON3,
		0x002B0023);  // Threshold for urgent
	MRAW_WRITE_REG(dev->base + REG_MRAW_M1_CQI_ORIRDMA_CON4,
		0x00080000);  // Threshold for DVFS
	return ret;
}

int mtk_cam_mraw_fbc_config(
	struct mtk_mraw_device *dev)
{
	int ret = 0;

	MRAW_WRITE_REG(dev->base + REG_MRAW_FBC_IMGO_CTL1, 0);
	MRAW_WRITE_REG(dev->base + REG_MRAW_FBC_IMGBO_CTL1, 0);
	MRAW_WRITE_REG(dev->base + REG_MRAW_FBC_CPIO_CTL1, 0);

	MRAW_WRITE_BITS(dev->base + REG_MRAW_MRAWCTL_FBC_GROUP,
		MRAW_MRAWCTL_FBC_GROUP, MRAWCTL_IMGO_M1_FBC_SEL, 1);
	MRAW_WRITE_BITS(dev->base + REG_MRAW_MRAWCTL_FBC_GROUP,
		MRAW_MRAWCTL_FBC_GROUP, MRAWCTL_IMGBO_M1_FBC_SEL, 1);
	MRAW_WRITE_BITS(dev->base + REG_MRAW_MRAWCTL_FBC_GROUP,
		MRAW_MRAWCTL_FBC_GROUP, MRAWCTL_CPIO_M1_FBC_SEL, 1);
	return ret;
}

int mtk_cam_mraw_toggle_tg_db(struct mtk_mraw_device *dev)
{
	MRAW_WRITE_BITS(dev->base + REG_MRAW_TG_PATH_CFG,
		MRAW_TG_PATH_CFG, TG_M1_DB_LOAD_DIS, 1);
	MRAW_WRITE_BITS(dev->base + REG_MRAW_TG_PATH_CFG,
		MRAW_TG_PATH_CFG, TG_M1_DB_LOAD_DIS, 0);

	return 0;
}

int mtk_cam_mraw_toggle_db(struct mtk_mraw_device *dev)
{
	MRAW_WRITE_BITS(dev->base + REG_MRAW_M_MRAWCTL_MISC,
		MRAW_CTL_MISC, MRAWCTL_DB_EN, 0);
	MRAW_WRITE_BITS(dev->base + REG_MRAW_M_MRAWCTL_MISC,
		MRAW_CTL_MISC, MRAWCTL_DB_EN, 1);

	return 0;
}

int mtk_cam_mraw_cq_disable(struct mtk_mraw_device *dev)
{
	int ret = 0;

	writel_relaxed(~CQ_THR0_EN, dev->base + REG_MRAW_CQ_THR0_CTL);
	wmb(); /* TBC */

	return ret;
}

int mtk_cam_mraw_top_enable(struct mtk_mraw_device *dev)
{
	int ret = 0;

	//  toggle db
	mtk_cam_mraw_toggle_db(dev);

	//  toggle tg db
	mtk_cam_mraw_toggle_tg_db(dev);

	/* Enable CMOS */
	dev_info(dev->dev, "%s: enable CMOS and VF\n", __func__);
	MRAW_WRITE_BITS(dev->base + REG_MRAW_TG_SEN_MODE,
		MRAW_TG_SEN_MODE, TG_CMOS_EN, 1);

	/* Enable VF */
	if (MRAW_READ_BITS(dev->base + REG_MRAW_TG_SEN_MODE,
		MRAW_TG_SEN_MODE, TG_CMOS_EN) && dev->is_enqueued)
		mtk_cam_mraw_vf_on(dev, 1);
	else
		dev_info(dev->dev, "%s, CMOS is off\n", __func__);

	return ret;
}

int mtk_cam_mraw_fbc_enable(struct mtk_mraw_device *dev)
{
	int ret = 0;

	if (MRAW_READ_BITS(dev->base + REG_MRAW_TG_VF_CON,
			MRAW_TG_VF_CON, TG_M1_VFDATA_EN) == 1) {
		ret = -1;
		dev_dbg(dev->dev, "cannot enable fbc when streaming");
		goto EXIT;
	}
	MRAW_WRITE_BITS(dev->base + REG_MRAW_FBC_IMGO_CTL1,
		MRAW_FBC_IMGO_CTL1, FBC_IMGO_FBC_EN, 1);

	MRAW_WRITE_BITS(dev->base + REG_MRAW_FBC_IMGBO_CTL1,
		MRAW_FBC_IMGBO_CTL1, FBC_IMGBO_FBC_EN, 1);

	MRAW_WRITE_BITS(dev->base + REG_MRAW_FBC_CPIO_CTL1,
		MRAW_FBC_CPIO_CTL1, FBC_CPIO_FBC_EN, 1);

	MRAW_WRITE_BITS(dev->base + REG_MRAW_FBC_IMGO_CTL1,
		MRAW_FBC_IMGO_CTL1, FBC_IMGO_FBC_DB_EN, 0);
	MRAW_WRITE_BITS(dev->base + REG_MRAW_FBC_IMGBO_CTL1,
		MRAW_FBC_IMGBO_CTL1, FBC_IMGBO_FBC_DB_EN, 0);
	MRAW_WRITE_BITS(dev->base + REG_MRAW_FBC_CPIO_CTL1,
		MRAW_FBC_CPIO_CTL1, FBC_CPIO_FBC_DB_EN, 0);


EXIT:
	return ret;
}

int mtk_cam_mraw_cq_config(struct mtk_mraw_device *dev)
{
	int ret = 0;
#if USING_MRAW_SCQ
	u32 val;

	val = readl_relaxed(dev->base + REG_MRAW_CQ_EN);
	val = val & (~CQ_DB_EN);
	writel_relaxed(val | SCQ_EN, dev->base + REG_MRAW_CQ_EN);

	writel_relaxed(0xffffffff, dev->base + REG_MRAW_SCQ_START_PERIOD);
	wmb(); /* TBC */
#endif
	writel_relaxed(CQ_THR0_MODE_IMMEDIATE | CQ_THR0_EN,
		       dev->base + REG_MRAW_CQ_THR0_CTL);
	writel_relaxed(MRAWCTL_CQ_THR0_DONE_ST,
		       dev->base + REG_MRAW_CTL_INT6_EN);
	wmb(); /* TBC */

	dev->sof_count = 0;

	dev_info(dev->dev, "%s - REG_CQ_EN:0x%x ,REG_CQ_THR0_CTL:0x%8x\n",
		__func__,
			readl_relaxed(dev->base + REG_MRAW_CQ_EN),
			readl_relaxed(dev->base + REG_MRAW_CQ_THR0_CTL));

	return ret;
}

int mtk_cam_mraw_cq_enable(struct mtk_cam_ctx *ctx,
	struct mtk_mraw_device *dev)
{
	int ret = 0;
	u32 val;
#if USING_MRAW_SCQ
	val = readl_relaxed(dev->base + REG_MRAW_TG_TIME_STAMP_CNT);

	//[todo]: implement/check the start period
	writel_relaxed(SCQ_DEADLINE_MS * 1000 * SCQ_DEFAULT_CLK_RATE
		, dev->base + REG_MRAW_SCQ_START_PERIOD);
#else
	writel_relaxed(CQ_THR0_MODE_CONTINUOUS | CQ_THR0_EN,
				dev->base + REG_MRAW_CQ_THR0_CTL);

	writel_relaxed(CQ_DB_EN | CQ_DB_LOAD_MODE,
				dev->base + REG_MRAW_CQ_EN);
	wmb(); /* TBC */

	dev_info(dev->dev, "%s - REG_CQ_EN:0x%x ,REG_CQ_THR0_CTL:0x%8x\n",
		__func__,
			readl_relaxed(dev->base + REG_MRAW_CQ_EN),
			readl_relaxed(dev->base + REG_MRAW_CQ_THR0_CTL));
#endif
	return ret;
}

int mtk_cam_mraw_tg_disable(struct mtk_mraw_device *dev)
{
	int ret = 0;
	u32 val;

	dev_dbg(dev->dev, "stream off, disable CMOS\n");
	val = readl(dev->base + REG_MRAW_TG_SEN_MODE);
	writel(val & (~MRAW_TG_SEN_MODE_CMOS_EN),
		dev->base + REG_MRAW_TG_SEN_MODE);

	return ret;
}

int mtk_cam_mraw_top_disable(struct mtk_mraw_device *dev)
{
	int ret = 0;

	if (MRAW_READ_BITS(dev->base + REG_MRAW_TG_VF_CON,
			MRAW_TG_VF_CON, TG_M1_VFDATA_EN)) {
		MRAW_WRITE_BITS(dev->base + REG_MRAW_TG_VF_CON,
			MRAW_TG_VF_CON, TG_M1_VFDATA_EN, 0);
		mtk_cam_mraw_toggle_tg_db(dev);
	}

	mraw_reset(dev);

	MRAW_WRITE_BITS(dev->base + REG_MRAW_M_MRAWCTL_MISC,
		MRAW_CTL_MISC, MRAWCTL_DB_EN, 0);
	MRAW_WRITE_REG(dev->base + REG_MRAW_M_MRAWCTL_MISC, 0);
	MRAW_WRITE_REG(dev->base + REG_MRAW_MRAWCTL_FMT_SEL, 0);
	MRAW_WRITE_REG(dev->base + REG_MRAW_CTL_INT_EN, 0);
	MRAW_WRITE_REG(dev->base + REG_MRAW_CTL_INT5_EN, 0);
	MRAW_WRITE_REG(dev->base + REG_MRAW_FBC_IMGO_CTL1, 0);
	MRAW_WRITE_REG(dev->base + REG_MRAW_FBC_IMGBO_CTL1, 0);
	MRAW_WRITE_REG(dev->base + REG_MRAW_FBC_CPIO_CTL1, 0);

	MRAW_WRITE_BITS(dev->base + REG_MRAW_M_MRAWCTL_MISC,
		MRAW_CTL_MISC, MRAWCTL_DB_EN, 1);
	return ret;
}

int mtk_cam_mraw_dma_disable(struct mtk_mraw_device *dev)
{
	int ret = 0;

	MRAW_WRITE_BITS(dev->base + REG_MRAW_CTL_MOD2_EN,
		MRAW_CTL_MOD2_EN, MRAWCTL_IMGO_M1_EN, 0);
	MRAW_WRITE_BITS(dev->base + REG_MRAW_CTL_MOD2_EN,
		MRAW_CTL_MOD2_EN, MRAWCTL_IMGBO_M1_EN, 0);
	MRAW_WRITE_BITS(dev->base + REG_MRAW_CTL_MOD2_EN,
		MRAW_CTL_MOD2_EN, MRAWCTL_CPIO_M1_EN, 0);
	MRAW_WRITE_BITS(dev->base + REG_MRAW_CTL_MOD2_EN,
		MRAW_CTL_MOD2_EN, MRAWCTL_LSCI_M1_EN, 0);

	return ret;
}

int mtk_cam_mraw_fbc_disable(struct mtk_mraw_device *dev)
{
	int ret = 0;

	MRAW_WRITE_REG(dev->base + REG_MRAW_FBC_IMGO_CTL1, 0);
	MRAW_WRITE_REG(dev->base + REG_MRAW_FBC_IMGBO_CTL1, 0);
	MRAW_WRITE_REG(dev->base + REG_MRAW_FBC_CPIO_CTL1, 0);

	MRAW_WRITE_BITS(dev->base + REG_MRAW_MRAWCTL_FBC_GROUP,
		MRAW_MRAWCTL_FBC_GROUP, MRAWCTL_IMGO_M1_FBC_SEL, 0);
	MRAW_WRITE_BITS(dev->base + REG_MRAW_MRAWCTL_FBC_GROUP,
		MRAW_MRAWCTL_FBC_GROUP, MRAWCTL_IMGBO_M1_FBC_SEL, 0);
	MRAW_WRITE_BITS(dev->base + REG_MRAW_MRAWCTL_FBC_GROUP,
		MRAW_MRAWCTL_FBC_GROUP, MRAWCTL_CPIO_M1_FBC_SEL, 0);

	return ret;
}

int mtk_cam_mraw_vf_on(struct mtk_mraw_device *dev, unsigned int is_on)
{
	int ret = 0;

	if (is_on) {
		if (MRAW_READ_BITS(dev->base + REG_MRAW_TG_VF_CON,
			MRAW_TG_VF_CON, TG_M1_VFDATA_EN) == 0)
			MRAW_WRITE_BITS(dev->base + REG_MRAW_TG_VF_CON,
				MRAW_TG_VF_CON, TG_M1_VFDATA_EN, 1);
	} else {
		if (MRAW_READ_BITS(dev->base + REG_MRAW_TG_VF_CON,
			MRAW_TG_VF_CON, TG_M1_VFDATA_EN) == 1)
			MRAW_WRITE_BITS(dev->base + REG_MRAW_TG_VF_CON,
				MRAW_TG_VF_CON, TG_M1_VFDATA_EN, 0);
	}

	return ret;
}

int mtk_cam_mraw_is_vf_on(struct mtk_mraw_device *dev)
{
	if (MRAW_READ_BITS(dev->base + REG_MRAW_TG_VF_CON,
			MRAW_TG_VF_CON, TG_M1_VFDATA_EN) == 1)
		return 1;
	else
		return 0;
}

int mtk_cam_find_mraw_dev_index(
	struct mtk_cam_ctx *ctx,
	unsigned int idx)
{
	int i;

	for (i = 0 ; i < MAX_MRAW_PIPES_PER_STREAM ; i++) {
		if (ctx->used_mraw_dev[i] == (1 << idx))
			return i;
	}

	return -1;
}

int mtk_cam_mraw_dev_config(
	struct mtk_cam_ctx *ctx,
	unsigned int idx)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct device *dev = cam->dev;
	struct v4l2_mbus_framefmt *mf;
	struct device *dev_mraw;
	struct mtk_mraw_device *mraw_dev;
	struct v4l2_format *img_fmt;
	unsigned int i;
	int ret, pad_idx = 0;

	dev_dbg(dev, "%s\n", __func__);

	img_fmt = &ctx->mraw_pipe[idx]
		->vdev_nodes[MTK_MRAW_META_OUT-MTK_MRAW_SINK_NUM].active_fmt;
	pad_idx = ctx->mraw_pipe[idx]->seninf_padidx;
	mf = &ctx->mraw_pipe[idx]->cfg[MTK_MRAW_SINK].mbus_fmt;

	ret = mtk_cam_mraw_pipeline_config(ctx, idx);
	if (ret)
		return ret;

	dev_mraw = mtk_cam_find_mraw_dev(cam, ctx->used_mraw_dev[idx]);
	if (dev_mraw == NULL) {
		dev_dbg(dev, "config mraw device not found\n");
		return -EINVAL;
	}
	mraw_dev = dev_get_drvdata(dev_mraw);
	for (i = 0; i < MRAW_PIPELINE_NUM; i++)
		if (cam->mraw.pipelines[i].enabled_mraw & 1<<mraw_dev->id) {
			mraw_dev->pipeline = &cam->mraw.pipelines[i];
			break;
		}

	/* reset enqueued status */
	mraw_dev->is_enqueued = 0;
	reset_msgfifo(mraw_dev);

	mtk_cam_mraw_tg_config(mraw_dev, ctx->mraw_pipe[idx]->res_config.pixel_mode);
	mtk_cam_mraw_top_config(mraw_dev);
	mtk_cam_mraw_dma_config(mraw_dev);
	mtk_cam_mraw_fbc_config(mraw_dev);
	mtk_cam_mraw_fbc_enable(mraw_dev);
	mtk_cam_mraw_cq_config(mraw_dev);

	mtk_mraw_register_iommu_tf_callback(mraw_dev);

	dev_info(dev, "mraw %d %s done\n", mraw_dev->id, __func__);

	return 0;
}

int mtk_cam_mraw_dev_stream_on(
	struct mtk_cam_ctx *ctx,
	unsigned int idx,
	unsigned int streaming)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct device *dev = cam->dev;
	struct device *dev_mraw;
	struct mtk_mraw_device *mraw_dev;
	unsigned int i;
	int ret = 0;

	dev_dbg(dev, "%s\n", __func__);

	dev_mraw = mtk_cam_find_mraw_dev(cam, ctx->used_mraw_dev[idx]);
	if (dev_mraw == NULL) {
		dev_dbg(dev, "stream on mraw device not found\n");
		return -EINVAL;
	}
	mraw_dev = dev_get_drvdata(dev_mraw);
	for (i = 0; i < MRAW_PIPELINE_NUM; i++)
		if (cam->mraw.pipelines[i].enabled_mraw & 1<<mraw_dev->id) {
			mraw_dev->pipeline = &cam->mraw.pipelines[i];
			break;
		}

	if (streaming)
		ret = mtk_cam_mraw_cq_enable(ctx, mraw_dev) ||
			mtk_cam_mraw_top_enable(mraw_dev);
	else {
		/* reset enqueued status */
		mraw_dev->is_enqueued = 0;

		ret = mtk_cam_mraw_top_disable(mraw_dev) ||
			mtk_cam_mraw_cq_disable(mraw_dev) ||
			mtk_cam_mraw_fbc_disable(mraw_dev) ||
			mtk_cam_mraw_dma_disable(mraw_dev) ||
			mtk_cam_mraw_tg_disable(mraw_dev);

		pm_runtime_put_sync(mraw_dev->dev);
	}

	dev_info(dev, "mraw %d %s en(%d)\n", mraw_dev->id, __func__, streaming);

	return ret;
}

static void mtk_mraw_pipeline_queue_setup(
	struct mtk_mraw_pipeline *pipe)
{
	unsigned int node_idx, i;

	node_idx = 0;

	/* Setup the output queue */
	for (i = 0; i < MTK_MRAW_TOTAL_OUTPUT_QUEUES; i++) {
		pipe->vdev_nodes[node_idx].desc = mraw_output_queues[i];
		pipe->vdev_nodes[node_idx++].desc.name =
			mraw_output_queue_names[pipe->id-MTKCAM_SUBDEV_MRAW_START][i];
	}
	/* Setup the capture queue */
	for (i = 0; i < MTK_MRAW_TOTAL_CAPTURE_QUEUES; i++) {
		pipe->vdev_nodes[node_idx].desc = mraw_capture_queues[i];
		pipe->vdev_nodes[node_idx++].desc.name =
			mraw_capture_queue_names[pipe->id-MTKCAM_SUBDEV_MRAW_START][i];
	}
}

static int  mtk_mraw_pipeline_register(
	unsigned int id, struct device *dev,
	struct mtk_mraw_pipeline *pipe,
	struct v4l2_device *v4l2_dev)
{
	struct mtk_cam_device *cam = dev_get_drvdata(pipe->mraw->cam_dev);
	struct mtk_mraw_device *mraw_dev = dev_get_drvdata(dev);
	struct v4l2_subdev *sd = &pipe->subdev;
	struct mtk_cam_video_device *video;
	unsigned int i;
	int ret;

	pipe->id = id;
	pipe->cammux_id = mraw_dev->cammux_id;

	/* Initialize subdev */
	v4l2_subdev_init(sd, &mtk_mraw_subdev_ops);
	sd->entity.function = MEDIA_ENT_F_PROC_VIDEO_PIXEL_FORMATTER;
	sd->entity.ops = &mtk_mraw_media_entity_ops;
	sd->flags = V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS;
	ret = snprintf(sd->name, sizeof(sd->name),
		 "%s-%d", dev_driver_string(dev), (pipe->id-MTKCAM_SUBDEV_MRAW_START));
	if (ret < 0) {
		dev_info(dev, "Failed to compose device name: %d\n", ret);
		return ret;
	}
	v4l2_set_subdevdata(sd, pipe);

	dev_info(dev, "%s: %s\n", __func__, sd->name);

	ret = v4l2_device_register_subdev(v4l2_dev, sd);
	if (ret < 0) {
		dev_info(dev, "Failed to register subdev: %d\n", ret);
		return ret;
	}

	mtk_mraw_pipeline_queue_setup(pipe);

	//setup pads of mraw pipeline
	for (i = 0; i < ARRAY_SIZE(pipe->pads); i++) {
		pipe->pads[i].flags = i < MTK_MRAW_SOURCE_BEGIN ?
			MEDIA_PAD_FL_SINK : MEDIA_PAD_FL_SOURCE;
	}

	media_entity_pads_init(&sd->entity, ARRAY_SIZE(pipe->pads), pipe->pads);

	/* setup video node */
	for (i = 0; i < ARRAY_SIZE(pipe->vdev_nodes); i++) {
		video = pipe->vdev_nodes + i;

		switch (pipe->id) {
		case MTKCAM_SUBDEV_MRAW_0:
		case MTKCAM_SUBDEV_MRAW_1:
		case MTKCAM_SUBDEV_MRAW_2:
		case MTKCAM_SUBDEV_MRAW_3:
			video->uid.pipe_id = pipe->id;
			break;
		default:
			dev_dbg(dev, "invalid pipe id\n");
			return -EINVAL;
		}

		video->uid.id = video->desc.dma_port;
		video->ctx = &cam->ctxs[id];
		ret = mtk_cam_video_register(video, v4l2_dev);
		if (ret)
			goto fail_unregister_video;

		if (V4L2_TYPE_IS_OUTPUT(video->desc.buf_type))
			ret = media_create_pad_link(&video->vdev.entity, 0,
						    &sd->entity,
						    video->desc.id,
						    video->desc.link_flags);
		else
			ret = media_create_pad_link(&sd->entity,
						    video->desc.id,
						    &video->vdev.entity, 0,
						    video->desc.link_flags);

		if (ret)
			goto fail_unregister_video;
	}

	return 0;

fail_unregister_video:
	for (i = i-1; i >= 0; i--)
		mtk_cam_video_unregister(pipe->vdev_nodes + i);

	return ret;
}

static void mtk_mraw_pipeline_unregister(
	struct mtk_mraw_pipeline *pipe)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(pipe->vdev_nodes); i++)
		mtk_cam_video_unregister(pipe->vdev_nodes + i);

	v4l2_device_unregister_subdev(&pipe->subdev);
	media_entity_cleanup(&pipe->subdev.entity);
}

int mtk_mraw_setup_dependencies(struct mtk_mraw *mraw, struct mtk_larb *larb)
{
	struct device *dev = mraw->cam_dev;
	struct device *consumer, *supplier;
	struct device_link *link;
	int i;

	for (i = 0; i < MRAW_PIPELINE_NUM; i++) {
		consumer = mraw->devs[i];
		if (!consumer) {
			dev_info(dev, "failed to get dev for id %d\n", i);
			continue;
		}

		switch (i) {
		case 0:
		case 2:
			supplier = find_larb(larb, 25);
			break;
		default:
			supplier = find_larb(larb, 26);
			break;
		}

		if (!supplier) {
			dev_info(dev, "failed to get supplier for id %d\n", i);
			return -ENODEV;
		}

		link = device_link_add(consumer, supplier,
				       DL_FLAG_AUTOREMOVE_CONSUMER |
				       DL_FLAG_PM_RUNTIME);
		if (!link) {
			dev_info(dev, "Unable to create link between %s and %s\n",
				dev_name(consumer), dev_name(supplier));
			return -ENODEV;
		}
	}

	return 0;
}

int mtk_mraw_register_entities(
	struct mtk_mraw *mraw,
	struct v4l2_device *v4l2_dev)
{
	unsigned int i;
	int ret;

	for (i = 0; i < MRAW_PIPELINE_NUM; i++) {
		struct mtk_mraw_pipeline *pipe = mraw->pipelines + i;

		pipe->mraw = mraw;
		memset(pipe->cfg, 0, sizeof(*pipe->cfg));
		ret = mtk_mraw_pipeline_register(MTKCAM_SUBDEV_MRAW_START + i,
						mraw->devs[i],
						mraw->pipelines + i, v4l2_dev);
		if (ret)
			return ret;
	}
	return 0;
}

void mtk_mraw_unregister_entities(struct mtk_mraw *mraw)
{
	unsigned int i;

	for (i = 0; i < MRAW_PIPELINE_NUM; i++)
		mtk_mraw_pipeline_unregister(mraw->pipelines + i);
}

void mraw_irq_handle_tg_grab_err(struct mtk_mraw_device *mraw_dev,
	int dequeued_frame_seq_no)
{
	int val, val2;
	struct mtk_cam_request_stream_data *s_data;
	struct mtk_cam_ctx *ctx;

	val = readl_relaxed(mraw_dev->base + REG_MRAW_TG_PATH_CFG);
	val = val | MRAW_TG_PATH_TG_FULL_SEL;
	writel_relaxed(val, mraw_dev->base + REG_MRAW_TG_PATH_CFG);
	wmb(); /* TBC */
	val2 = readl_relaxed(mraw_dev->base + REG_MRAW_TG_SEN_MODE);
	val2 = val2 | MRAW_TG_CMOS_RDY_SEL;
	writel_relaxed(val2, mraw_dev->base + REG_MRAW_TG_SEN_MODE);
	wmb(); /* TBC */
	dev_info_ratelimited(mraw_dev->dev,
		"TG PATHCFG/SENMODE/FRMSIZE/RGRABPXL/LIN:%x/%x/%x/%x/%x/%x\n",
		readl_relaxed(mraw_dev->base + REG_MRAW_TG_PATH_CFG),
		readl_relaxed(mraw_dev->base + REG_MRAW_TG_SEN_MODE),
		readl_relaxed(mraw_dev->base + REG_MRAW_TG_FRMSIZE_ST),
		readl_relaxed(mraw_dev->base + REG_MRAW_TG_FRMSIZE_ST_R),
		readl_relaxed(mraw_dev->base + REG_MRAW_TG_SEN_GRAB_PXL),
		readl_relaxed(mraw_dev->base + REG_MRAW_TG_SEN_GRAB_LIN));

	ctx = mtk_cam_find_ctx(mraw_dev->cam, &mraw_dev->pipeline->subdev.entity);
	if (!ctx) {
		dev_info(mraw_dev->dev, "%s: cannot find ctx\n", __func__);
		return;
	}

	s_data = mtk_cam_get_req_s_data(ctx,
		mraw_dev->id + MTKCAM_SUBDEV_MRAW_START, dequeued_frame_seq_no);
	if (s_data) {
		mtk_cam_debug_seninf_dump(s_data);
	} else {
		dev_info(mraw_dev->dev,
			 "%s: req(%d) can't be found for seninf dump\n",
			 __func__, dequeued_frame_seq_no);
	}
}

void mraw_irq_handle_dma_err(struct mtk_mraw_device *mraw_dev)
{
	dev_info_ratelimited(mraw_dev->dev,
		"IMGO:0x%x IMGBO:0x%x CPIO:0x%x\n",
		readl_relaxed(mraw_dev->base + REG_MRAW_IMGO_ERR_STAT),
		readl_relaxed(mraw_dev->base + REG_MRAW_IMGBO_ERR_STAT),
		readl_relaxed(mraw_dev->base + REG_MRAW_CPIO_ERR_STAT));
}

static void mraw_irq_handle_tg_overrun_err(struct mtk_mraw_device *mraw_dev,
	int dequeued_frame_seq_no)
{
	int val, val2, irq_status5;
	struct mtk_cam_request_stream_data *s_data;
	struct mtk_cam_ctx *ctx;

	val = readl_relaxed(mraw_dev->base + REG_MRAW_TG_PATH_CFG);
	val = val | MRAW_TG_PATH_TG_FULL_SEL;
	writel_relaxed(val, mraw_dev->base + REG_MRAW_TG_PATH_CFG);
	wmb(); /* TBC */
	val2 = readl_relaxed(mraw_dev->base + REG_MRAW_TG_SEN_MODE);
	val2 = val2 | MRAW_TG_CMOS_RDY_SEL;
	writel_relaxed(val2, mraw_dev->base + REG_MRAW_TG_SEN_MODE);
	wmb(); /* TBC */
	irq_status5 = readl_relaxed(mraw_dev->base + REG_MRAW_CTL_INT5_STATUSX);
	dev_info_ratelimited(mraw_dev->dev,
			 "TG PATHCFG/SENMODE FRMSIZE/R GRABPXL/LIN:0x%x/0x%x 0x%x/0x%x 0x%x/0x%x\n",
			 readl_relaxed(mraw_dev->base + REG_MRAW_TG_PATH_CFG),
			 readl_relaxed(mraw_dev->base + REG_MRAW_TG_SEN_MODE),
			 readl_relaxed(mraw_dev->base + REG_MRAW_TG_FRMSIZE_ST),
			 readl_relaxed(mraw_dev->base + REG_MRAW_TG_FRMSIZE_ST_R),
			 readl_relaxed(mraw_dev->base + REG_MRAW_TG_SEN_GRAB_PXL),
			 readl_relaxed(mraw_dev->base + REG_MRAW_TG_SEN_GRAB_LIN));
	dev_info_ratelimited(mraw_dev->dev,
			"imgo_overr_status:0x%x, imgbo_overr_status:0x%x, cpio_overr_status:0x%x\n",
			irq_status5 & MRAWCTL_IMGO_M1_OTF_OVERFLOW_ST,
			irq_status5 & MRAWCTL_IMGBO_M1_OTF_OVERFLOW_ST,
			irq_status5 & MRAWCTL_CPIO_M1_OTF_OVERFLOW_ST);

	ctx = mtk_cam_find_ctx(mraw_dev->cam, &mraw_dev->pipeline->subdev.entity);
	if (!ctx) {
		dev_info(mraw_dev->dev, "%s: cannot find ctx\n", __func__);
		return;
	}

	s_data = mtk_cam_get_req_s_data(ctx,
		mraw_dev->id + MTKCAM_SUBDEV_MRAW_START, dequeued_frame_seq_no);
	if (s_data) {
		mtk_cam_debug_seninf_dump(s_data);
	} else {
		dev_info(mraw_dev->dev,
			 "%s: req(%d) can't be found for seninf dump\n",
			 __func__, dequeued_frame_seq_no);
	}
}

static void mraw_handle_error(struct mtk_mraw_device *mraw_dev,
			     struct mtk_camsys_irq_info *data)
{
	int err_status = data->e.err_status;
	int frame_idx_inner = data->frame_idx_inner;

	/* Show DMA errors in detail */
	if (err_status & DMA_ST_MASK_MRAW_ERR)
		mraw_irq_handle_dma_err(mraw_dev);
	/* Show TG register for more error detail*/
	if (err_status & MRAWCTL_TG_GBERR_ST)
		mraw_irq_handle_tg_grab_err(mraw_dev, frame_idx_inner);
	if (err_status & MRAWCTL_TG_ERR_ST)
		mraw_irq_handle_tg_overrun_err(mraw_dev, frame_idx_inner);
}

static irqreturn_t mtk_irq_mraw(int irq, void *data)
{
	struct mtk_mraw_device *mraw_dev = (struct mtk_mraw_device *)data;
	struct device *dev = mraw_dev->dev;
	struct mtk_camsys_irq_info irq_info;
	unsigned int dequeued_imgo_seq_no, dequeued_imgo_seq_no_inner;
	unsigned int irq_status, irq_status2, irq_status3, irq_status4;
	unsigned int irq_status5, irq_status6;
	unsigned int err_status, dma_err_status;
	unsigned int imgo_overr_status, imgbo_overr_status, cpio_overr_status;
	bool wake_thread = 0;

	irq_status	= readl_relaxed(mraw_dev->base + REG_MRAW_CTL_INT_STATUS);
	/*
	 * [ISP 7.0/7.1] HW Bug Workaround: read MRAWCTL_INT2_STATUS every irq
	 * Because MRAWCTL_INT2_EN is attach to OTF_OVER_FLOW ENABLE incorrectly
	 */
	irq_status2	= readl_relaxed(mraw_dev->base + REG_MRAW_CTL_INT2_STATUS);
	irq_status3	= readl_relaxed(mraw_dev->base + REG_MRAW_CTL_INT3_STATUS);
	irq_status4	= readl_relaxed(mraw_dev->base + REG_MRAW_CTL_INT4_STATUS);
	irq_status5 = readl_relaxed(mraw_dev->base + REG_MRAW_CTL_INT5_STATUS);
	irq_status6	= readl_relaxed(mraw_dev->base + REG_MRAW_CTL_INT6_STATUS);
	dequeued_imgo_seq_no =
		readl_relaxed(mraw_dev->base + REG_MRAW_FRAME_SEQ_NUM);
	dequeued_imgo_seq_no_inner =
		readl_relaxed(mraw_dev->base_inner + REG_MRAW_FRAME_SEQ_NUM);

	err_status = irq_status & INT_ST_MASK_MRAW_ERR;
	dma_err_status = irq_status & MRAWCTL_DMA_ERR_ST;
	imgo_overr_status = irq_status5 & MRAWCTL_IMGO_M1_OTF_OVERFLOW_ST;
	imgbo_overr_status = irq_status5 & MRAWCTL_IMGBO_M1_OTF_OVERFLOW_ST;
	cpio_overr_status = irq_status5 & MRAWCTL_CPIO_M1_OTF_OVERFLOW_ST;

	dev_dbg(dev,
		"%i status:0x%x(err:0x%x)/0x%x dma_err:0x%x seq_num:%d\n",
		mraw_dev->id, irq_status, err_status, irq_status6, dma_err_status,
		dequeued_imgo_seq_no_inner);

	dev_dbg(dev,
		"%i dma_overr:0x%x_0x%x_0x%x fbc_ctrl:0x%x_0x%x_0x%x dma_addr:0x%x%x_0x%x%x_0x%x%x\n",
		mraw_dev->id, imgo_overr_status, imgbo_overr_status, cpio_overr_status,
		readl_relaxed(mraw_dev->base + REG_MRAW_FBC_IMGO_CTL2),
		readl_relaxed(mraw_dev->base + REG_MRAW_FBC_IMGBO_CTL2),
		readl_relaxed(mraw_dev->base + REG_MRAW_FBC_CPIO_CTL2),
		readl_relaxed(mraw_dev->base_inner + REG_MRAW_IMGO_BASE_ADDR_MSB),
		readl_relaxed(mraw_dev->base_inner + REG_MRAW_IMGO_BASE_ADDR),
		readl_relaxed(mraw_dev->base_inner + REG_MRAW_IMGBO_BASE_ADDR_MSB),
		readl_relaxed(mraw_dev->base_inner + REG_MRAW_IMGBO_BASE_ADDR),
		readl_relaxed(mraw_dev->base_inner + REG_MRAW_CPIO_BASE_ADDR_MSB),
		readl_relaxed(mraw_dev->base_inner + REG_MRAW_CPIO_BASE_ADDR));

	/*
	 * In normal case, the next SOF ISR should come after HW PASS1 DONE ISR.
	 * If these two ISRs come together, print warning msg to hint.
	 */
	irq_info.irq_type = 0;
	irq_info.ts_ns = ktime_get_boottime_ns();
	irq_info.frame_idx = dequeued_imgo_seq_no;
	irq_info.frame_idx_inner = dequeued_imgo_seq_no_inner;

	if ((irq_status & MRAWCTL_SOF_INT_ST) &&
		(irq_status & MRAWCTL_PASS1_DONE_ST))
		dev_dbg(dev, "sof_done block cnt:%d\n", mraw_dev->sof_count);

	/* Frame done */
	if (irq_status & MRAWCTL_SW_PASS1_DONE_ST) {
		irq_info.irq_type |= (1 << CAMSYS_IRQ_FRAME_DONE);
		dev_dbg(dev, "p1_done sof_cnt:%d\n", mraw_dev->sof_count);
	}
	/* Frame start */
	if (irq_status & MRAWCTL_SOF_INT_ST) {
		irq_info.irq_type |= (1 << CAMSYS_IRQ_FRAME_START);
		mraw_dev->sof_count++;
		dev_dbg(dev, "sof block cnt:%d\n", mraw_dev->sof_count);
	}
	/* CQ done */
	if (irq_status6 & MRAWCTL_CQ_THR0_DONE_ST) {
		irq_info.irq_type |= (1 << CAMSYS_IRQ_SETTING_DONE);
		dev_dbg(dev, "CQ done:%d\n", mraw_dev->sof_count);
	}

	if (irq_info.irq_type && push_msgfifo(mraw_dev, &irq_info) == 0)
		wake_thread = 1;

	/* Check ISP error status */
	if (err_status) {
		struct mtk_camsys_irq_info err_info;

		err_info.irq_type = CAMSYS_IRQ_ERROR;
		err_info.ts_ns = irq_info.ts_ns;
		err_info.frame_idx = irq_info.frame_idx;
		err_info.frame_idx_inner = irq_info.frame_idx_inner;
		err_info.e.err_status = err_status;

		if (push_msgfifo(mraw_dev, &err_info) == 0)
			wake_thread = 1;
	}

	return wake_thread ? IRQ_WAKE_THREAD : IRQ_HANDLED;
}

static irqreturn_t mtk_thread_irq_mraw(int irq, void *data)
{
	struct mtk_mraw_device *mraw_dev = (struct mtk_mraw_device *)data;
	struct mtk_camsys_irq_info irq_info;

	if (unlikely(atomic_cmpxchg(&mraw_dev->is_fifo_overflow, 1, 0)))
		dev_info(mraw_dev->dev, "msg fifo overflow\n");

	while (kfifo_len(&mraw_dev->msg_fifo) >= sizeof(irq_info)) {
		int len = kfifo_out(&mraw_dev->msg_fifo, &irq_info, sizeof(irq_info));

		WARN_ON(len != sizeof(irq_info));

		/* error case */
		if (unlikely(irq_info.irq_type == CAMSYS_IRQ_ERROR)) {
			mraw_handle_error(mraw_dev, &irq_info);
			continue;
		}

		/* normal case */

		/* inform interrupt information to camsys controller */
		mtk_camsys_isr_event(mraw_dev->cam,
				     CAMSYS_ENGINE_MRAW, mraw_dev->id,
				     &irq_info);
	}

	return IRQ_HANDLED;
}


bool
mtk_cam_mraw_finish_buf(struct mtk_cam_request_stream_data *req_stream_data)
{
	bool result = false;
	struct mtk_mraw_working_buf_entry *mraw_buf_entry, *mraw_buf_entry_prev;
	struct mtk_cam_ctx *ctx = req_stream_data->ctx;
	int i;

	if (!ctx->used_mraw_num)
		return false;

	for (i = 0; i < ctx->used_mraw_num; i++) {
		spin_lock(&ctx->mraw_processing_buffer_list[i].lock);
		list_for_each_entry_safe(mraw_buf_entry, mraw_buf_entry_prev,
					 &ctx->mraw_processing_buffer_list[i].list,
					 list_entry) {
			if (mraw_buf_entry->s_data->frame_seq_no
				== req_stream_data->frame_seq_no) {
				list_del(&mraw_buf_entry->list_entry);
				mtk_cam_mraw_wbuf_set_s_data(mraw_buf_entry, NULL);
				mtk_cam_mraw_working_buf_put(ctx, mraw_buf_entry);
				ctx->mraw_processing_buffer_list[i].cnt--;
				result = true;
				break;
			}
		}
		spin_unlock(&ctx->mraw_processing_buffer_list[i].lock);
	}

	return result;
}


static int mtk_mraw_pm_suspend(struct device *dev)
{
	struct mtk_mraw_device *mraw_dev = dev_get_drvdata(dev);
	u32 val;
	int ret;

	dev_dbg(dev, "- %s\n", __func__);

	if (pm_runtime_suspended(dev))
		return 0;

	/* Disable ISP's view finder and wait for TG idle */
	dev_dbg(dev, "mraw suspend, disable VF\n");
	val = readl(mraw_dev->base + REG_MRAW_TG_VF_CON);
	writel(val & (~MRAW_TG_VF_CON_VFDATA_EN),
		mraw_dev->base + REG_MRAW_TG_VF_CON);
	ret = readl_poll_timeout_atomic(
					mraw_dev->base + REG_MRAW_TG_INTER_ST, val,
					(val & MRAW_TG_CS_MASK) == MRAW_TG_IDLE_ST,
					USEC_PER_MSEC, MTK_MRAW_STOP_HW_TIMEOUT);
	if (ret)
		dev_dbg(dev, "can't stop HW:%d:0x%x\n", ret, val);

	/* Disable CMOS */
	val = readl(mraw_dev->base + REG_MRAW_TG_SEN_MODE);
	writel(val & (~MRAW_TG_SEN_MODE_CMOS_EN),
		mraw_dev->base + REG_MRAW_TG_SEN_MODE);

	/* Force ISP HW to idle */
	ret = pm_runtime_put_sync(dev);
	return ret;
}

static int mtk_mraw_pm_resume(struct device *dev)
{
	struct mtk_mraw_device *mraw_dev = dev_get_drvdata(dev);
	u32 val;
	int ret;

	dev_dbg(dev, "- %s\n", __func__);

	if (pm_runtime_suspended(dev))
		return 0;

	/* Force ISP HW to resume */
	ret = pm_runtime_get_sync(dev);
	if (ret)
		return ret;

	/* Enable CMOS */
	dev_dbg(dev, "mraw resume, enable CMOS/VF\n");
	val = readl(mraw_dev->base + REG_MRAW_TG_SEN_MODE);
	writel(val | MRAW_TG_SEN_MODE_CMOS_EN,
		mraw_dev->base + REG_MRAW_TG_SEN_MODE);

	/* Enable VF */
	val = readl(mraw_dev->base + REG_MRAW_TG_VF_CON);
	writel(val | MRAW_TG_VF_CON_VFDATA_EN,
		mraw_dev->base + REG_MRAW_TG_VF_CON);

	return 0;
}

static int mtk_mraw_suspend_pm_event(struct notifier_block *notifier,
			unsigned long pm_event, void *unused)
{
	struct mtk_mraw_device *mraw_dev =
		container_of(notifier, struct mtk_mraw_device, notifier_blk);
	struct device *dev = mraw_dev->dev;

	switch (pm_event) {
	case PM_HIBERNATION_PREPARE:
		return NOTIFY_DONE;
	case PM_RESTORE_PREPARE:
		return NOTIFY_DONE;
	case PM_POST_HIBERNATION:
		return NOTIFY_DONE;
	case PM_SUSPEND_PREPARE: /* before enter suspend */
		mtk_mraw_pm_suspend(dev);
		return NOTIFY_DONE;
	case PM_POST_SUSPEND: /* after resume */
		mtk_mraw_pm_resume(dev);
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static int mtk_mraw_of_probe(struct platform_device *pdev,
			    struct mtk_mraw_device *mraw)
{
	struct device *dev = &pdev->dev;
	struct platform_device *larb_pdev;
	struct device_node *larb_node;
	struct device_link *link;
	struct resource *res;
	int ret;
	int clks, larbs, i;

	ret = of_property_read_u32(dev->of_node, "mediatek,mraw-id",
						       &mraw->id);
	if (ret) {
		dev_dbg(dev, "missing camid property\n");
		return ret;
	}

	ret = of_property_read_u32(dev->of_node, "mediatek,cammux-id",
						       &mraw->cammux_id);
	if (ret) {
		dev_dbg(dev, "missing cammux id property\n");
		return ret;
	}

	/* base outer register */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "base");
	if (!res) {
		dev_info(dev, "failed to get mem\n");
		return -ENODEV;
	}

	mraw->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(mraw->base)) {
		dev_dbg(dev, "failed to map register base\n");
		return PTR_ERR(mraw->base);
	}
	dev_dbg(dev, "mraw, map_addr=0x%pK\n", mraw->base);

	/* base inner register */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "inner_base");
	if (!res) {
		dev_dbg(dev, "failed to get mem\n");
		return -ENODEV;
	}

	mraw->base_inner = devm_ioremap_resource(dev, res);
	if (IS_ERR(mraw->base_inner)) {
		dev_dbg(dev, "failed to map register inner base\n");
		return PTR_ERR(mraw->base_inner);
	}
	dev_dbg(dev, "mraw, map_addr(inner)=0x%pK\n", mraw->base_inner);


	mraw->irq = platform_get_irq(pdev, 0);
	if (!mraw->irq) {
		dev_dbg(dev, "failed to get irq\n");
		return -ENODEV;
	}

	ret = devm_request_threaded_irq(dev, mraw->irq,
				mtk_irq_mraw, mtk_thread_irq_mraw,
				0, dev_name(dev), mraw);
	if (ret) {
		dev_dbg(dev, "failed to request irq=%d\n", mraw->irq);
		return ret;
	}
	dev_dbg(dev, "registered irq=%d\n", mraw->irq);
	disable_irq(mraw->irq);

	clks = of_count_phandle_with_args(pdev->dev.of_node, "clocks",
			"#clock-cells");
	mraw->num_clks = (clks == -ENOENT) ? 0:clks;
	dev_info(dev, "clk_num:%d\n", mraw->num_clks);
	if (mraw->num_clks) {
		mraw->clks = devm_kcalloc(dev, mraw->num_clks, sizeof(*mraw->clks),
					 GFP_KERNEL);
		if (!mraw->clks)
			return -ENOMEM;
	}

	for (i = 0; i < mraw->num_clks; i++) {
		mraw->clks[i] = of_clk_get(pdev->dev.of_node, i);
		if (IS_ERR(mraw->clks[i])) {
			dev_info(dev, "failed to get clk %d\n", i);
			return -ENODEV;
		}
	}

	larbs = of_count_phandle_with_args(
					pdev->dev.of_node, "mediatek,larbs", NULL);
	dev_info(dev, "larb_num:%d\n", larbs);

	for (i = 0; i < larbs; i++) {
		larb_node = of_parse_phandle(
					pdev->dev.of_node, "mediatek,larbs", i);
		if (!larb_node) {
			dev_info(dev, "failed to get larb id\n");
			continue;
		}

		larb_pdev = of_find_device_by_node(larb_node);
		if (WARN_ON(!larb_pdev)) {
			of_node_put(larb_node);
			dev_info(dev, "failed to get larb pdev\n");
			continue;
		}
		of_node_put(larb_node);

		link = device_link_add(&pdev->dev, &larb_pdev->dev,
						DL_FLAG_PM_RUNTIME | DL_FLAG_STATELESS);
		if (!link)
			dev_info(dev, "unable to link smi larb%d\n", i);
	}

	mraw->notifier_blk.notifier_call = mtk_mraw_suspend_pm_event;
	mraw->notifier_blk.priority = 0;
	ret = register_pm_notifier(&mraw->notifier_blk);
	if (ret) {
		dev_info(dev, "Failed to register PM notifier");
		return -ENODEV;
	}

	return 0;
}

static int mtk_mraw_component_bind(
	struct device *dev,
	struct device *master,
	void *data)
{
	struct mtk_mraw_device *mraw_dev = dev_get_drvdata(dev);
	struct mtk_cam_device *cam_dev = data;
	struct mtk_mraw *mraw = &cam_dev->mraw;

	dev_info(dev, "%s: id=%d\n", __func__, mraw_dev->id);

	mraw_dev->cam = cam_dev;
	mraw->devs[mraw_dev->id] = dev;
	mraw->cam_dev = cam_dev->dev;

	return 0;
}

static void mtk_mraw_component_unbind(
	struct device *dev,
	struct device *master,
	void *data)
{
	struct mtk_mraw_device *mraw_dev = dev_get_drvdata(dev);
	struct mtk_cam_device *cam_dev = data;
	struct mtk_mraw *mraw = &cam_dev->mraw;

	dev_info(dev, "%s\n", __func__);

	mraw_dev->cam = NULL;
	mraw_dev->pipeline = NULL;
	mraw->devs[mraw_dev->id] = NULL;
}


static const struct component_ops mtk_mraw_component_ops = {
	.bind = mtk_mraw_component_bind,
	.unbind = mtk_mraw_component_unbind,
};

static int mtk_mraw_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_mraw_device *mraw_dev;
	int ret;

	dev_info(dev, "%s\n", __func__);

	mraw_dev = devm_kzalloc(dev, sizeof(*mraw_dev), GFP_KERNEL);
	if (!mraw_dev)
		return -ENOMEM;

	mraw_dev->dev = dev;
	dev_set_drvdata(dev, mraw_dev);

	ret = mtk_mraw_of_probe(pdev, mraw_dev);
	if (ret)
		return ret;

	mraw_dev->fifo_size =
		roundup_pow_of_two(8 * sizeof(struct mtk_camsys_irq_info));
	mraw_dev->msg_buffer = devm_kzalloc(dev, mraw_dev->fifo_size, GFP_KERNEL);
	if (!mraw_dev->msg_buffer)
		return -ENOMEM;

	pm_runtime_enable(dev);

	return component_add(dev, &mtk_mraw_component_ops);
}

static int mtk_mraw_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	dev_info(dev, "%s\n", __func__);

	pm_runtime_disable(dev);

	component_del(dev, &mtk_mraw_component_ops);
	return 0;
}

static int mtk_mraw_runtime_suspend(struct device *dev)
{
	struct mtk_mraw_device *mraw_dev = dev_get_drvdata(dev);
	int i;

	disable_irq(mraw_dev->irq);

	dev_dbg(dev, "%s:disable clock\n", __func__);
	for (i = 0; i < mraw_dev->num_clks; i++)
		clk_disable_unprepare(mraw_dev->clks[i]);

	return 0;
}

static int mtk_mraw_runtime_resume(struct device *dev)
{
	struct mtk_mraw_device *mraw_dev = dev_get_drvdata(dev);
	int i, ret;

	/* reset_msgfifo before enable_irq */
	ret = reset_msgfifo(mraw_dev);
	if (ret)
		return ret;

	enable_irq(mraw_dev->irq);

	dev_dbg(dev, "%s:enable clock\n", __func__);
	for (i = 0; i < mraw_dev->num_clks; i++) {
		ret = clk_prepare_enable(mraw_dev->clks[i]);
		if (ret) {
			dev_info(dev, "enable failed at clk #%d, ret = %d\n",
				 i, ret);
			i--;
			while (i >= 0)
				clk_disable_unprepare(mraw_dev->clks[i--]);

			return ret;
		}
	}
	mraw_reset(mraw_dev);

	return 0;
}

static const struct dev_pm_ops mtk_mraw_pm_ops = {
	SET_RUNTIME_PM_OPS(mtk_mraw_runtime_suspend, mtk_mraw_runtime_resume,
			   NULL)
};

struct platform_driver mtk_cam_mraw_driver = {
	.probe   = mtk_mraw_probe,
	.remove  = mtk_mraw_remove,
	.driver  = {
		.name  = "mtk-cam mraw",
		.of_match_table = of_match_ptr(mtk_mraw_of_ids),
		.pm     = &mtk_mraw_pm_ops,
	}
};

