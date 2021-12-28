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

#include "mtk_cam.h"
#include "mtk_cam-feature.h"
#include "mtk_cam_pm.h"
#include "mtk_cam-pool.h"
#include "mtk_cam-sv-regs.h"
#include "mtk_cam-sv.h"
#include "mtk_camera-v4l2-controls.h"
#include "mtk_camera-videodev2.h"

#ifdef CONFIG_MTK_SMI_EXT
#include "smi_public.h"
#endif

#ifdef CONFIG_MTK_IOMMU_V2
#include "mtk_iommu_ext.h"
#include "mach/pseudo_m4u.h"
#endif

#ifdef CAMSYS_TF_DUMP_71_1
#include <dt-bindings/memory/mt6983-larb-port.h>
#include "iommu_debug.h"
#endif

#define MTK_CAMSV_STOP_HW_TIMEOUT			(33 * USEC_PER_MSEC)

static const struct of_device_id mtk_camsv_of_ids[] = {
	{.compatible = "mediatek,camsv",},
	{}
};
MODULE_DEVICE_TABLE(of, mtk_camsv_of_ids);

static void mtk_camsv_register_iommu_tf_callback(struct mtk_camsv_device *sv)
{
#ifdef CAMSYS_TF_DUMP_71_1
	dev_dbg(sv->dev, "%s : sv->id:%d\n", __func__, sv->id);

	switch (sv->id) {
	case CAMSV_0:
		mtk_iommu_register_fault_callback(M4U_PORT_L13_CAM1_GCAMSV_A_IMGO_1,
			mtk_camsv_translation_fault_callback, (void *)sv, false);
		break;
	case CAMSV_1:
		mtk_iommu_register_fault_callback(M4U_PORT_L13_CAM1_GCAMSV_A_IMGO_2,
			mtk_camsv_translation_fault_callback, (void *)sv, false);
		break;
	case CAMSV_2:
		mtk_iommu_register_fault_callback(M4U_PORT_L14_CAM1_GCAMSV_B_IMGO_1,
			mtk_camsv_translation_fault_callback, (void *)sv, false);
		break;
	case CAMSV_3:
		mtk_iommu_register_fault_callback(M4U_PORT_L14_CAM1_GCAMSV_B_IMGO_2,
			mtk_camsv_translation_fault_callback, (void *)sv, false);
		break;
	case CAMSV_4:
		mtk_iommu_register_fault_callback(M4U_PORT_L13_CAM1_GCAMSV_C_IMGO_1,
			mtk_camsv_translation_fault_callback, (void *)sv, false);
		break;
	case CAMSV_5:
		mtk_iommu_register_fault_callback(M4U_PORT_L13_CAM1_GCAMSV_C_IMGO_2,
			mtk_camsv_translation_fault_callback, (void *)sv, false);
		break;
	case CAMSV_6:
		mtk_iommu_register_fault_callback(M4U_PORT_L14_CAM1_GCAMSV_D_IMGO_1,
			mtk_camsv_translation_fault_callback, (void *)sv, false);
		break;
	case CAMSV_7:
		mtk_iommu_register_fault_callback(M4U_PORT_L14_CAM1_GCAMSV_D_IMGO_2,
			mtk_camsv_translation_fault_callback, (void *)sv, false);
		break;
	case CAMSV_8:
		mtk_iommu_register_fault_callback(M4U_PORT_L13_CAM1_GCAMSV_E_IMGO_1,
			mtk_camsv_translation_fault_callback, (void *)sv, false);
		break;
	case CAMSV_9:
		mtk_iommu_register_fault_callback(M4U_PORT_L13_CAM1_GCAMSV_E_IMGO_2,
			mtk_camsv_translation_fault_callback, (void *)sv, false);
		break;
	case CAMSV_10:
		mtk_iommu_register_fault_callback(M4U_PORT_L14_CAM1_GCAMSV_F_IMGO_1,
			mtk_camsv_translation_fault_callback, (void *)sv, false);
		break;
	case CAMSV_11:
		mtk_iommu_register_fault_callback(M4U_PORT_L14_CAM1_GCAMSV_F_IMGO_2,
			mtk_camsv_translation_fault_callback, (void *)sv, false);
		break;
	case CAMSV_12:
		mtk_iommu_register_fault_callback(M4U_PORT_L13_CAM1_GCAMSV_G_IMGO_1,
			mtk_camsv_translation_fault_callback, (void *)sv, false);
		break;
	case CAMSV_13:
		mtk_iommu_register_fault_callback(M4U_PORT_L13_CAM1_GCAMSV_G_IMGO_2,
			mtk_camsv_translation_fault_callback, (void *)sv, false);
		break;
	case CAMSV_14:
		mtk_iommu_register_fault_callback(M4U_PORT_L14_CAM1_GCAMSV_H_IMGO_1,
			mtk_camsv_translation_fault_callback, (void *)sv, false);
		break;
	case CAMSV_15:
		mtk_iommu_register_fault_callback(M4U_PORT_L14_CAM1_GCAMSV_H_IMGO_2,
			mtk_camsv_translation_fault_callback, (void *)sv, false);
		break;
	}
#endif
};

#ifdef CAMSYS_TF_DUMP_71_1
int mtk_camsv_translation_fault_callback(int port, dma_addr_t mva, void *data)
{
	struct mtk_camsv_device *sv_dev = (struct mtk_camsv_device *)data;

	dev_info(sv_dev->dev, "mod_en:0x%x fmt_sel:0x%x imgo_fbc_ctrl1:0x%x imgo_fbc_ctrl2:0x%x",
		readl_relaxed(sv_dev->base_inner + REG_CAMSV_MODULE_EN),
		readl_relaxed(sv_dev->base_inner + REG_CAMSV_FMT_SEL),
		readl_relaxed(sv_dev->base_inner + REG_CAMSV_FBC_IMGO_CTL1),
		readl_relaxed(sv_dev->base_inner + REG_CAMSV_FBC_IMGO_CTL2));

	dev_info(sv_dev->dev, "tg_sen_mode:0x%x tg_vf_con:0x%x tg_path_cfg:0x%x tg_grab_pxl:0x%x tg_grab_lin:0x%x",
		readl_relaxed(sv_dev->base_inner + REG_CAMSV_TG_SEN_MODE),
		readl_relaxed(sv_dev->base_inner + REG_CAMSV_TG_VF_CON),
		readl_relaxed(sv_dev->base_inner + REG_CAMSV_TG_PATH_CFG),
		readl_relaxed(sv_dev->base_inner + REG_CAMSV_TG_SEN_GRAB_PXL),
		readl_relaxed(sv_dev->base_inner + REG_CAMSV_TG_SEN_GRAB_LIN));

	dev_info(sv_dev->dev, "imgo_xsize:0x%x imgo_ysize:0x%x imgo_stride:0x%x imgo_addr:0x%x_%x imgo_ofst_addr:0x%x_%x",
		readl_relaxed(sv_dev->base_inner + REG_CAMSV_IMGO_XSIZE),
		readl_relaxed(sv_dev->base_inner + REG_CAMSV_IMGO_YSIZE),
		readl_relaxed(sv_dev->base_inner + REG_CAMSV_IMGO_STRIDE),
		readl_relaxed(sv_dev->base_inner + REG_CAMSV_IMGO_BASE_ADDR_MSB),
		readl_relaxed(sv_dev->base_inner + REG_CAMSV_IMGO_BASE_ADDR),
		readl_relaxed(sv_dev->base_inner + REG_CAMSV_IMGO_OFST_ADDR_MSB),
		readl_relaxed(sv_dev->base_inner + REG_CAMSV_IMGO_OFST_ADDR));

	return 0;
}
#endif

static const struct v4l2_mbus_framefmt sv_mfmt_default = {
	.code = MEDIA_BUS_FMT_SBGGR10_1X10,
	.width = DEFAULT_WIDTH,
	.height = DEFAULT_HEIGHT,
	.field = V4L2_FIELD_NONE,
	.colorspace = V4L2_COLORSPACE_SRGB,
	.xfer_func = V4L2_XFER_FUNC_DEFAULT,
	.ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT,
	.quantization = V4L2_QUANTIZATION_DEFAULT,
};

static int mtk_camsv_sd_subscribe_event(struct v4l2_subdev *subdev,
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

int mtk_cam_sv_select(struct mtk_camsv_pipeline *pipe,
			struct mtkcam_ipi_input_param *cfg_in_param)
{
	pipe->enabled_sv = 1 << (pipe->id - MTKCAM_SUBDEV_CAMSV_START);

	return 0;
}

static int mtk_camsv_sd_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct mtk_camsv_pipeline *pipe =
		container_of(sd, struct mtk_camsv_pipeline, subdev);
	struct mtk_camsv *sv = pipe->sv;
	struct mtk_cam_device *cam = dev_get_drvdata(sv->cam_dev);
	struct mtk_cam_ctx *ctx = mtk_cam_find_ctx(cam, &sd->entity);
	unsigned int i;

	if (WARN_ON(!ctx))
		return -EINVAL;

	if (enable) {
		pipe->enabled_dmas = 0;
		if (ctx->used_sv_num < MAX_SV_PIPES_PER_STREAM)
			ctx->sv_pipe[ctx->used_sv_num++] = pipe;
		else
			dev_dbg(sv->cam_dev, "un-expected used sv number:%d\n", ctx->used_sv_num);

		for (i = 0; i < ARRAY_SIZE(pipe->vdev_nodes); i++) {
			if (!pipe->vdev_nodes[i].enabled)
				continue;
			pipe->enabled_dmas |= pipe->vdev_nodes[i].desc.dma_port;
		}
	} else {
		pipe->enabled_sv = 0;
		pipe->enabled_dmas = 0;
	}

	dev_info(sv->cam_dev, "%s:camsv-%d: en %d, dev 0x%x dmas 0x%x\n",
		 __func__, pipe->id-MTKCAM_SUBDEV_CAMSV_START, enable, pipe->enabled_sv,
		 pipe->enabled_dmas);

	return 0;
}

static int mtk_camsv_init_cfg(struct v4l2_subdev *sd,
		struct v4l2_subdev_pad_config *cfg)
{
	struct v4l2_mbus_framefmt *mf;
	unsigned int i;
	struct mtk_camsv_pipeline *pipe =
		container_of(sd, struct mtk_camsv_pipeline, subdev);
	struct mtk_camsv *sv = pipe->sv;

	for (i = 0; i < sd->entity.num_pads; i++) {
		mf = v4l2_subdev_get_try_format(sd, cfg, i);
		*mf = sv_mfmt_default;
		pipe->cfg[i].mbus_fmt = sv_mfmt_default;

		dev_dbg(sv->cam_dev, "%s init pad:%d format:0x%x\n",
			sd->name, i, mf->code);
	}

	return 0;
}

static int mtk_camsv_try_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_format *fmt)
{
	struct mtk_camsv_pipeline *pipe =
		container_of(sd, struct mtk_camsv_pipeline, subdev);
	struct mtk_camsv *sv = pipe->sv;
	unsigned int sensor_fmt = mtk_cam_get_sensor_fmt(fmt->format.code);

	dev_dbg(sv->cam_dev, "%s try format 0x%x, w:%d, h:%d field:%d\n",
		sd->name, fmt->format.code, fmt->format.width,
		fmt->format.height, fmt->format.field);

	/* check sensor format */
	if (!sensor_fmt || fmt->pad == MTK_CAMSV_SINK)
		return sensor_fmt;
	else if (fmt->pad < MTK_CAMSV_PIPELINE_PADS_NUM) {
		/* check vdev node format */
		unsigned int img_fmt, i;
		struct mtk_cam_video_device *node =
			&pipe->vdev_nodes[fmt->pad - MTK_CAMSV_SINK_NUM];

		dev_dbg(sv->cam_dev, "node:%s num_fmts:%d",
				node->desc.name, node->desc.num_fmts);
		for (i = 0; i < node->desc.num_fmts; i++) {
			img_fmt = mtk_cam_get_img_fmt(
				node->desc.fmts[i].vfmt.fmt.pix_mp.pixelformat);
			dev_dbg(sv->cam_dev,
				"try format sensor_fmt 0x%x img_fmt 0x%x",
				sensor_fmt, img_fmt);
			if (sensor_fmt == img_fmt)
				return img_fmt;
		}
	}

	return MTKCAM_IPI_IMG_FMT_UNKNOWN;
}

static struct v4l2_mbus_framefmt *get_sv_fmt(struct mtk_camsv_pipeline *pipe,
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

static int mtk_camsv_call_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *fmt)
{
	struct mtk_camsv_pipeline *pipe =
		container_of(sd, struct mtk_camsv_pipeline, subdev);
	struct mtk_camsv *sv = pipe->sv;
	struct v4l2_mbus_framefmt *mf;

	if (!sd || !fmt) {
		dev_dbg(sv->cam_dev, "%s: Required sd(%p), fmt(%p)\n",
			__func__, sd, fmt);
		return -EINVAL;
	}

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY && !cfg) {
		dev_dbg(sv->cam_dev, "%s: Required sd(%p), cfg(%p) for FORMAT_TRY\n",
					__func__, sd, cfg);
		return -EINVAL;
	}

	if (!mtk_camsv_try_fmt(sd, fmt)) {
		mf = get_sv_fmt(pipe, cfg, fmt->pad, fmt->which);
		fmt->format = *mf;
	} else {
		mf = get_sv_fmt(pipe, cfg, fmt->pad, fmt->which);
		*mf = fmt->format;
		dev_dbg(sv->cam_dev,
			"sd:%s pad:%d set format w/h/code %d/%d/0x%x\n",
			sd->name, fmt->pad, mf->width, mf->height, mf->code);
	}

	return 0;
}

int mtk_camsv_call_pending_set_fmt(struct v4l2_subdev *sd,
				 struct v4l2_subdev_format *fmt)
{
	struct mtk_camsv_pipeline *pipe =
		container_of(sd, struct mtk_camsv_pipeline, subdev);
	struct mtk_cam_device *cam = dev_get_drvdata(pipe->sv->cam_dev);

	/* We only allow V4L2_SUBDEV_FORMAT_ACTIVE for pending set fmt */
	if (fmt->which != V4L2_SUBDEV_FORMAT_ACTIVE) {
		dev_info(cam->dev,
			"%s:pipe(%d):pad(%d): only allow V4L2_SUBDEV_FORMAT_ACTIVE\n",
			__func__, pipe->id, fmt->pad);
		return -EINVAL;
	}

	return mtk_camsv_call_set_fmt(sd, NULL, fmt);
}

static int mtk_camsv_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *fmt)
{
	struct media_request *req;
	struct mtk_cam_request *cam_req;
	struct mtk_cam_request_stream_data *stream_data;

	struct mtk_camsv_pipeline *pipe =
		container_of(sd, struct mtk_camsv_pipeline, subdev);
	struct mtk_cam_device *cam = dev_get_drvdata(pipe->sv->cam_dev);

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY)
		return mtk_camsv_call_set_fmt(sd, cfg, fmt);

	/* if the pipeline is streaming, pending the change */
	if (!sd->entity.stream_count)
		return mtk_camsv_call_set_fmt(sd, cfg, fmt);

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

static int mtk_camsv_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *fmt)
{
	struct mtk_camsv_pipeline *pipe =
		container_of(sd, struct mtk_camsv_pipeline, subdev);
	struct mtk_camsv *sv = pipe->sv;
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
	dev_dbg(sv->cam_dev, "sd:%s pad:%d get format 0x%x\n",
		sd->name, fmt->pad, fmt->format.code);

	return 0;
}

static int mtk_camsv_media_link_setup(struct media_entity *entity,
				    const struct media_pad *local,
				    const struct media_pad *remote, u32 flags)
{
	struct mtk_camsv_pipeline *pipe =
		container_of(entity, struct mtk_camsv_pipeline, subdev.entity);
	struct mtk_camsv *sv = pipe->sv;
	u32 pad = local->index;

	dev_info(sv->cam_dev, "%s: camsv %d: %d->%d flags:0x%x\n",
		__func__, pipe->id, remote->index, local->index, flags);

	if (pad == MTK_CAMSV_SINK)
		pipe->seninf_padidx = remote->index;

	if (pad < MTK_CAMSV_PIPELINE_PADS_NUM && pad != MTK_CAMSV_SINK)
		pipe->vdev_nodes[pad-MTK_CAMSV_SINK_NUM].enabled =
			!!(flags & MEDIA_LNK_FL_ENABLED);

	if (!(flags & MEDIA_LNK_FL_ENABLED))
		memset(pipe->cfg, 0, sizeof(pipe->cfg));

	return 0;
}

static const struct v4l2_subdev_core_ops mtk_camsv_subdev_core_ops = {
	.subscribe_event = mtk_camsv_sd_subscribe_event,
	.unsubscribe_event = v4l2_event_subdev_unsubscribe,
};

static const struct v4l2_subdev_video_ops mtk_camsv_subdev_video_ops = {
	.s_stream =  mtk_camsv_sd_s_stream,
};

static const struct v4l2_subdev_pad_ops mtk_camsv_subdev_pad_ops = {
	.link_validate = mtk_cam_link_validate,
	.init_cfg = mtk_camsv_init_cfg,
	.set_fmt = mtk_camsv_set_fmt,
	.get_fmt = mtk_camsv_get_fmt,
};

static const struct v4l2_subdev_ops mtk_camsv_subdev_ops = {
	.core = &mtk_camsv_subdev_core_ops,
	.video = &mtk_camsv_subdev_video_ops,
	.pad = &mtk_camsv_subdev_pad_ops,
};

static const struct media_entity_operations mtk_camsv_media_entity_ops = {
	.link_setup = mtk_camsv_media_link_setup,
	.link_validate = v4l2_subdev_link_validate,
};

static const struct v4l2_ioctl_ops mtk_camsv_v4l2_vcap_ioctl_ops = {
	.vidioc_querycap = mtk_cam_vidioc_querycap,
	.vidioc_enum_framesizes = mtk_cam_vidioc_enum_framesizes,
	.vidioc_enum_fmt_vid_cap = mtk_cam_vidioc_enum_fmt,
	.vidioc_g_fmt_vid_cap_mplane = mtk_cam_vidioc_g_fmt,
	.vidioc_s_fmt_vid_cap_mplane = mtk_cam_vidioc_s_fmt,
	.vidioc_try_fmt_vid_cap_mplane = mtk_cam_vidioc_try_fmt,
	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = vb2_ioctl_qbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,
	.vidioc_expbuf = vb2_ioctl_expbuf,
	.vidioc_subscribe_event = v4l2_ctrl_subscribe_event,
	.vidioc_unsubscribe_event = v4l2_event_unsubscribe,
};

static const struct mtk_cam_format_desc sv_stream_out_fmts[] = {
	/* This is a default image format */
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SBGGR10,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SBGGR8,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SBGGR12,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SBGGR14,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGBRG8,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGBRG10,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGBRG12,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGBRG14,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGRBG8,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGRBG10,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGRBG12,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGRBG14,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SRGGB8,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SRGGB10,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SRGGB12,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SRGGB14,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SBGGR10P,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGBRG10P,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGRBG10P,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SRGGB10P,
		},
	},
};

#define MTK_CAMSV_TOTAL_CAPTURE_QUEUES 1

static const struct
mtk_cam_dev_node_desc sv_capture_queues[] = {
	{
		.id = MTK_CAMSV_MAIN_STREAM_OUT,
		.name = "main stream",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.link_flags = MEDIA_LNK_FL_ENABLED |  MEDIA_LNK_FL_IMMUTABLE,
		.image = true,
		.smem_alloc = false,
		.dma_port = MTKCAM_IPI_CAMSV_MAIN_OUT,
		.fmts = sv_stream_out_fmts,
		.num_fmts = ARRAY_SIZE(sv_stream_out_fmts),
		.default_fmt_idx = 0,
		.ioctl_ops = &mtk_camsv_v4l2_vcap_ioctl_ops,
		.frmsizes = &(struct v4l2_frmsizeenum) {
			.index = 0,
			.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
			.stepwise = {
				.max_width = SV_IMG_MAX_WIDTH,
				.min_width = SV_IMG_MIN_WIDTH,
				.max_height = SV_IMG_MAX_HEIGHT,
				.min_height = SV_IMG_MIN_HEIGHT,
				.step_height = 1,
				.step_width = 1,
			},
		},
	},
};

static const char *sv_capture_queue_names[CAMSV_PIPELINE_NUM][MTK_CAMSV_TOTAL_CAPTURE_QUEUES] = {
	{"mtk-cam camsv-0 main-stream"},
	{"mtk-cam camsv-1 main-stream"},
	{"mtk-cam camsv-2 main-stream"},
	{"mtk-cam camsv-3 main-stream"},
	{"mtk-cam camsv-4 main-stream"},
	{"mtk-cam camsv-5 main-stream"},
	{"mtk-cam camsv-6 main-stream"},
	{"mtk-cam camsv-7 main-stream"},
	{"mtk-cam camsv-8 main-stream"},
	{"mtk-cam camsv-9 main-stream"},
	{"mtk-cam camsv-10 main-stream"},
	{"mtk-cam camsv-11 main-stream"},
	{"mtk-cam camsv-12 main-stream"},
	{"mtk-cam camsv-13 main-stream"},
	{"mtk-cam camsv-14 main-stream"},
	{"mtk-cam camsv-15 main-stream"},
};

static int reset_msgfifo(struct mtk_camsv_device *dev)
{
	atomic_set(&dev->is_fifo_overflow, 0);
	return kfifo_init(&dev->msg_fifo, dev->msg_buffer, dev->fifo_size);
}

static int push_msgfifo(struct mtk_camsv_device *dev,
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

void sv_reset(struct mtk_camsv_device *dev)
{
	int sw_ctl;
	int ret;

	dev_dbg(dev->dev, "%s camsv_id:%d\n", __func__, dev->id);

	writel(0, dev->base + REG_CAMSV_SW_CTL);
	writel(1, dev->base + REG_CAMSV_SW_CTL);
	wmb(); /* make sure committed */

	ret = readx_poll_timeout(readl, dev->base + REG_CAMSV_SW_CTL, sw_ctl,
				 sw_ctl & 0x2,
				 1 /* delay, us */,
				 100000 /* timeout, us */);
	if (ret < 0) {
		dev_info(dev->dev, "%s: timeout\n", __func__);

		dev_info(dev->dev,
			 "tg_sen_mode: 0x%x, ctl_en: 0x%x, ctl_sw_ctl:0x%x, frame_no:0x%x\n",
			 readl(dev->base + REG_CAMSV_TG_SEN_MODE),
			 readl(dev->base + REG_CAMSV_MODULE_EN),
			 readl(dev->base + REG_CAMSV_SW_CTL),
			 readl(dev->base + REG_CAMSV_FRAME_SEQ_NO)
			);

		goto RESET_FAILURE;
	}

	/* hw issue: second channel dma's ck/rst not refer to its own status */
	/* for above reason, merely do fbc reset rather than do hw reset */
	/* writel(4, dev->base + REG_CAMSV_SW_CTL); */
	writel(0x100, dev->base + REG_CAMSV_FBC_IMGO_CTL1);

	writel(0, dev->base + REG_CAMSV_SW_CTL);
	wmb(); /* make sure committed */

RESET_FAILURE:
	return;
}

int mtk_cam_sv_pipeline_config(
	struct mtk_cam_ctx *ctx, unsigned int idx,
	struct mtkcam_ipi_input_param *cfg_in_param)
{
	struct mtk_camsv_pipeline *sv_pipe = ctx->sv_pipe[idx];
	struct mtk_camsv *sv = sv_pipe->sv;
	unsigned int i;
	int ret;

	/* reset pm_runtime during streaming dynamic change */
	if (ctx->streaming) {
		for (i = 0; i < ARRAY_SIZE(sv->devs); i++)
			if (sv_pipe->enabled_sv & 1<<i)
				pm_runtime_put_sync(sv->devs[i]);
	}

	ret = mtk_cam_sv_select(sv_pipe, cfg_in_param);
	if (ret) {
		dev_dbg(sv->cam_dev, "failed select camsv: %d\n",
			ctx->stream_id);
		return ret;
	}

	for (i = 0; i < ARRAY_SIZE(sv->devs); i++)
		if (sv_pipe->enabled_sv & 1<<i)
			pm_runtime_get_sync(sv->devs[i]);

	if (ret < 0) {
		dev_dbg(sv->cam_dev,
			"failed at pm_runtime_get_sync: %s\n",
			dev_driver_string(sv->devs[i]));
		for (i = i-1; i >= 0; i--)
			if (sv_pipe->enabled_sv & 1<<i)
				pm_runtime_put_sync(sv->devs[i]);
		return ret;
	}

	ctx->used_sv_dev[idx] = sv_pipe->enabled_sv;
	dev_info(sv->cam_dev, "ctx_id %d used_sv_dev %d pipe_id %d\n",
		ctx->stream_id, ctx->used_sv_dev[idx], sv_pipe->id);
	return 0;
}


struct device *mtk_cam_find_sv_dev(struct mtk_cam_device *cam, unsigned int sv_mask)
{
	unsigned int i;

	for (i = 0; i < cam->num_camsv_drivers; i++) {
		if (sv_mask & (1 << i))
			return cam->sv.devs[i];
	}

	return NULL;
}

unsigned int mtk_cam_sv_format_sel(unsigned int pixel_fmt)
{
	union CAMSV_FMT_SEL fmt;

	fmt.Raw = 0;
	fmt.Bits.TG1_SW = TG_SW_UYVY;

	switch (pixel_fmt) {
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SGBRG8:
	case V4L2_PIX_FMT_SGRBG8:
	case V4L2_PIX_FMT_SRGGB8:
		fmt.Bits.TG1_FMT = SV_TG_FMT_RAW8;
		break;
	case V4L2_PIX_FMT_MTISP_SBGGR10:
	case V4L2_PIX_FMT_MTISP_SGBRG10:
	case V4L2_PIX_FMT_MTISP_SGRBG10:
	case V4L2_PIX_FMT_MTISP_SRGGB10:
	case V4L2_PIX_FMT_SBGGR10P:
	case V4L2_PIX_FMT_SGBRG10P:
	case V4L2_PIX_FMT_SGRBG10P:
	case V4L2_PIX_FMT_SRGGB10P:
		fmt.Bits.TG1_FMT = SV_TG_FMT_RAW10;
		break;
	case V4L2_PIX_FMT_MTISP_SBGGR12:
	case V4L2_PIX_FMT_MTISP_SGBRG12:
	case V4L2_PIX_FMT_MTISP_SGRBG12:
	case V4L2_PIX_FMT_MTISP_SRGGB12:
		fmt.Bits.TG1_FMT = SV_TG_FMT_RAW12;
		break;
	case V4L2_PIX_FMT_MTISP_SBGGR14:
	case V4L2_PIX_FMT_MTISP_SGBRG14:
	case V4L2_PIX_FMT_MTISP_SGRBG14:
	case V4L2_PIX_FMT_MTISP_SRGGB14:
		fmt.Bits.TG1_FMT = SV_TG_FMT_RAW14;
		break;
	default:
		break;
	}

	return fmt.Raw;
}

unsigned int mtk_cam_sv_pak_sel(unsigned int pixel_fmt,
	unsigned int pixel_mode)
{
	union CAMSV_PAK pak;

	pak.Raw = 0;

	switch (pixel_fmt) {
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SGBRG8:
	case V4L2_PIX_FMT_SGRBG8:
	case V4L2_PIX_FMT_SRGGB8:
		pak.Bits.PAK_MODE = 128;
		break;
	case V4L2_PIX_FMT_MTISP_SBGGR10:
	case V4L2_PIX_FMT_MTISP_SGBRG10:
	case V4L2_PIX_FMT_MTISP_SGRBG10:
	case V4L2_PIX_FMT_MTISP_SRGGB10:
		pak.Bits.PAK_MODE = 129;
		break;
	case V4L2_PIX_FMT_SBGGR10P:
	case V4L2_PIX_FMT_SGBRG10P:
	case V4L2_PIX_FMT_SGRBG10P:
	case V4L2_PIX_FMT_SRGGB10P:
		pak.Bits.PAK_MODE = 143;
		break;
	case V4L2_PIX_FMT_MTISP_SBGGR12:
	case V4L2_PIX_FMT_MTISP_SGBRG12:
	case V4L2_PIX_FMT_MTISP_SGRBG12:
	case V4L2_PIX_FMT_MTISP_SRGGB12:
		pak.Bits.PAK_MODE = 130;
		break;
	default:
		break;
	}

	pak.Bits.PAK_DBL_MODE = pixel_mode;

	return pak.Raw;
}

unsigned int mtk_cam_sv_xsize_cal(struct mtkcam_ipi_input_param *cfg_in_param)
{

	unsigned int size;
	unsigned int divisor;

	switch (cfg_in_param->fmt) {
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SGBRG8:
	case V4L2_PIX_FMT_SGRBG8:
	case V4L2_PIX_FMT_SRGGB8:
		size = cfg_in_param->in_crop.s.w;
		break;
	case V4L2_PIX_FMT_MTISP_SBGGR10:
	case V4L2_PIX_FMT_MTISP_SGBRG10:
	case V4L2_PIX_FMT_MTISP_SGRBG10:
	case V4L2_PIX_FMT_MTISP_SRGGB10:
	case V4L2_PIX_FMT_SBGGR10P:
	case V4L2_PIX_FMT_SGBRG10P:
	case V4L2_PIX_FMT_SGRBG10P:
	case V4L2_PIX_FMT_SRGGB10P:
		size = (cfg_in_param->in_crop.s.w * 10) / 8;
		break;
	case V4L2_PIX_FMT_MTISP_SBGGR12:
	case V4L2_PIX_FMT_MTISP_SGBRG12:
	case V4L2_PIX_FMT_MTISP_SGRBG12:
	case V4L2_PIX_FMT_MTISP_SRGGB12:
		size = (cfg_in_param->in_crop.s.w * 12) / 8;
		break;
	case V4L2_PIX_FMT_MTISP_SBGGR14:
	case V4L2_PIX_FMT_MTISP_SGBRG14:
	case V4L2_PIX_FMT_MTISP_SGRBG14:
	case V4L2_PIX_FMT_MTISP_SRGGB14:
		size = (cfg_in_param->in_crop.s.w * 14) / 8;
		break;
	default:
		break;
	}

	switch (cfg_in_param->pixel_mode) {
	case 0:
		divisor = 0x1;
		break;
	case 1:
		divisor = 0x3;
		break;
	case 2:
		divisor = 0x7;
		break;
	case 3:
		divisor = 0xF;
		break;
	default:
		return 0;
	}
	size = ((size + divisor) & ~divisor);
	return size;
}

int mtk_cam_sv_tg_config(struct mtk_camsv_device *dev, struct mtkcam_ipi_input_param *cfg_in_param)
{
	int ret = 0;
	unsigned int pxl, lin;

	CAMSV_WRITE_BITS(dev->base + REG_CAMSV_TG_SEN_MODE, CAMSV_TG_SEN_MODE, CMOS_EN, 0);

	/* subsample */
	if (cfg_in_param->subsample > 0) {
		CAMSV_WRITE_BITS(dev->base + REG_CAMSV_TG_SEN_MODE,
			CAMSV_TG_SEN_MODE, SOF_SUB_EN, 1);
		CAMSV_WRITE_BITS(dev->base + REG_CAMSV_TG_SEN_MODE,
			CAMSV_TG_SEN_MODE, VS_SUB_EN, 1);
		CAMSV_WRITE_BITS(dev->base + REG_CAMSV_TG_SUB_PERIOD,
			CAMSV_TG_SUB_PERIOD, VS_PERIOD, cfg_in_param->subsample);
		CAMSV_WRITE_BITS(dev->base + REG_CAMSV_TG_SUB_PERIOD,
			CAMSV_TG_SUB_PERIOD, SOF_PERIOD, cfg_in_param->subsample);
	} else {
		CAMSV_WRITE_BITS(dev->base + REG_CAMSV_TG_SEN_MODE,
			CAMSV_TG_SEN_MODE, SOF_SUB_EN, 0);
		CAMSV_WRITE_BITS(dev->base + REG_CAMSV_TG_SEN_MODE,
			CAMSV_TG_SEN_MODE, VS_SUB_EN, 0);
		CAMSV_WRITE_BITS(dev->base + REG_CAMSV_TG_SUB_PERIOD,
			CAMSV_TG_SUB_PERIOD, VS_PERIOD, cfg_in_param->subsample);
		CAMSV_WRITE_BITS(dev->base + REG_CAMSV_TG_SUB_PERIOD,
			CAMSV_TG_SUB_PERIOD, SOF_PERIOD, cfg_in_param->subsample);
	}

	if (dev->pipeline->hw_scen &
		MTK_CAMSV_SUPPORTED_STAGGER_SCENARIO) {
		if (dev->pipeline->exp_order == 0) {
			CAMSV_WRITE_BITS(dev->base + REG_CAMSV_TG_SEN_MODE,
				CAMSV_TG_SEN_MODE, STAGGER_EN, 0);
			CAMSV_WRITE_BITS(dev->base + REG_CAMSV_TG_PATH_CFG,
				CAMSV_TG_PATH_CFG, SUB_SOF_SRC_SEL, 0);
		} else {
			CAMSV_WRITE_BITS(dev->base + REG_CAMSV_TG_SEN_MODE,
				CAMSV_TG_SEN_MODE, STAGGER_EN, 1);
			CAMSV_WRITE_BITS(dev->base + REG_CAMSV_TG_PATH_CFG,
				CAMSV_TG_PATH_CFG, SUB_SOF_SRC_SEL, 0);
		}
	} else {
		CAMSV_WRITE_BITS(dev->base + REG_CAMSV_TG_SEN_MODE,
				CAMSV_TG_SEN_MODE, STAGGER_EN, 0);
		CAMSV_WRITE_BITS(dev->base + REG_CAMSV_TG_PATH_CFG,
				CAMSV_TG_PATH_CFG, SUB_SOF_SRC_SEL, 0);
	}

	/* timestamp */
	CAMSV_WRITE_BITS(dev->base + REG_CAMSV_TG_SEN_MODE,
		CAMSV_TG_SEN_MODE, TIME_STP_EN, 1);

	/* trig mode */
	CAMSV_WRITE_BITS(dev->base + REG_CAMSV_TG_VF_CON,
		CAMSV_TG_VF_CON, SINGLE_MODE, 0);

	/* pixel mode */
	switch (cfg_in_param->pixel_mode) {
	case 0:
		CAMSV_WRITE_BITS(dev->base + REG_CAMSV_TG_SEN_MODE,
			CAMSV_TG_SEN_MODE, DBL_DATA_BUS, 0);
		break;
	case 1:
		CAMSV_WRITE_BITS(dev->base + REG_CAMSV_TG_SEN_MODE,
			CAMSV_TG_SEN_MODE, DBL_DATA_BUS, 1);
		break;
	case 2:
		CAMSV_WRITE_BITS(dev->base + REG_CAMSV_TG_SEN_MODE,
			CAMSV_TG_SEN_MODE, DBL_DATA_BUS, 2);
		break;
	case 3:
		CAMSV_WRITE_BITS(dev->base + REG_CAMSV_TG_SEN_MODE,
			CAMSV_TG_SEN_MODE, DBL_DATA_BUS, 3);
		break;
	default:
		dev_dbg(dev->dev, "unknown pixel mode(%d)", cfg_in_param->pixel_mode);
		ret = -1;
		goto EXIT;
	}

	/* grab size */
	pxl = ((cfg_in_param->in_crop.s.w + cfg_in_param->in_crop.p.x) << 16) |
			cfg_in_param->in_crop.p.x;
	lin = ((cfg_in_param->in_crop.s.h + cfg_in_param->in_crop.p.y) << 16) |
			cfg_in_param->in_crop.p.y;
	CAMSV_WRITE_REG(dev->base + REG_CAMSV_TG_SEN_GRAB_PXL, pxl);
	CAMSV_WRITE_REG(dev->base + REG_CAMSV_TG_SEN_GRAB_LIN, lin);

	dev_info(dev->dev, "pxl mode:%d sub:%d fmt:0x%x x:%d y:%d w:%d h:%d\n",
		cfg_in_param->pixel_mode, cfg_in_param->subsample,
		cfg_in_param->fmt, cfg_in_param->in_crop.p.x,
		cfg_in_param->in_crop.p.y, cfg_in_param->in_crop.s.w,
		cfg_in_param->in_crop.s.h);
EXIT:
	return ret;
}

int mtk_cam_sv_top_config(
	struct mtk_camsv_device *dev,
	struct mtkcam_ipi_input_param *cfg_in_param)
{
	unsigned int int_en = (SV_INT_EN_TG_ERR_INT_EN |
							SV_INT_EN_TG_GBERR_INT_EN |
							SV_INT_EN_TG_SOF_INT_EN |
							SV_INT_EN_SW_PASS1_DON_INT_EN |
							SV_INT_EN_DMA_ERR_INT_EN);
	union CAMSV_PAK pak;
	int ret = 0;

	/* reset */
	sv_reset(dev);

	/* fun en */
	CAMSV_WRITE_BITS(dev->base + REG_CAMSV_MODULE_EN,
		CAMSV_MODULE_EN, TG_EN, 1);
	CAMSV_WRITE_BITS(dev->base + REG_CAMSV_MODULE_EN,
		CAMSV_MODULE_EN, DB_EN, 0);
	CAMSV_WRITE_BITS(dev->base + REG_CAMSV_MODULE_EN,
		CAMSV_MODULE_EN, DB_LOAD_SRC, SV_DB_SRC_SUB_SOF);

	/* central sub en */
	if (cfg_in_param->subsample > 0)
		CAMSV_WRITE_BITS(dev->base + REG_CAMSV_SUB_CTRL,
			CAMSV_SUB_CTRL, CENTRAL_SUB_EN, 1);
	else
		CAMSV_WRITE_BITS(dev->base + REG_CAMSV_SUB_CTRL,
			CAMSV_SUB_CTRL, CENTRAL_SUB_EN, 0);

	/* disable db load mask for non-dcif case */
	CAMSV_WRITE_BITS(dev->base + REG_CAMSV_DCIF_SET,
		CAMSV_DCIF_SET, MASK_DB_LOAD, 0);

	if ((dev->pipeline->hw_scen &
		(1 << MTKCAM_IPI_HW_PATH_ON_THE_FLY_DCIF_STAGGER)) ||
		(dev->pipeline->hw_scen &
		(1 << MTKCAM_IPI_HW_PATH_OFFLINE_SRT_DCIF_STAGGER))) {
		CAMSV_WRITE_BITS(dev->base + REG_CAMSV_DCIF_SET,
			CAMSV_DCIF_SET, FOR_DCIF_SUBSAMPLE_EN, 1);
		CAMSV_WRITE_BITS(dev->base + REG_CAMSV_DCIF_SET,
			CAMSV_DCIF_SET, ENABLE_OUTPUT_CQ_START_SIGNAL, 1);
	} else {
		CAMSV_WRITE_BITS(dev->base + REG_CAMSV_DCIF_SET,
			CAMSV_DCIF_SET, FOR_DCIF_SUBSAMPLE_EN, 0);
		CAMSV_WRITE_BITS(dev->base + REG_CAMSV_DCIF_SET,
			CAMSV_DCIF_SET, ENABLE_OUTPUT_CQ_START_SIGNAL, 0);
	}

	/* fmt sel */
	CAMSV_WRITE_REG(dev->base + REG_CAMSV_FMT_SEL,
		mtk_cam_sv_format_sel(cfg_in_param->fmt));

	/* int en */
	CAMSV_WRITE_REG(dev->base + REG_CAMSV_INT_EN, int_en);

	/* sub p1 done */
	if (cfg_in_param->subsample > 0) {
		CAMSV_WRITE_BITS(dev->base + REG_CAMSV_MODULE_EN,
			CAMSV_MODULE_EN, DOWN_SAMPLE_EN, 1);
		CAMSV_WRITE_BITS(dev->base + REG_CAMSV_MODULE_EN,
			CAMSV_MODULE_EN, DOWN_SAMPLE_PERIOD, cfg_in_param->subsample);
	} else {
		CAMSV_WRITE_BITS(dev->base + REG_CAMSV_MODULE_EN,
			CAMSV_MODULE_EN, DOWN_SAMPLE_EN, 0);
		CAMSV_WRITE_BITS(dev->base + REG_CAMSV_MODULE_EN,
			CAMSV_MODULE_EN, DOWN_SAMPLE_PERIOD, cfg_in_param->subsample);
	}

	/* pak */
	pak.Raw = mtk_cam_sv_pak_sel(cfg_in_param->fmt, cfg_in_param->pixel_mode);
	CAMSV_WRITE_BITS(dev->base + REG_CAMSV_MODULE_EN,
		CAMSV_MODULE_EN, PAK_EN, 1);
	CAMSV_WRITE_BITS(dev->base + REG_CAMSV_MODULE_EN,
		CAMSV_MODULE_EN, PAK_SEL, 0);
	CAMSV_WRITE_BITS(dev->base + REG_CAMSV_PAK_CON,
		CAMSV_PAK_CON, PAK_IN_BIT, 14);
	CAMSV_WRITE_REG(dev->base + REG_CAMSV_PAK, pak.Raw);

	/* ufe disable */
	CAMSV_WRITE_BITS(dev->base + REG_CAMSV_MODULE_EN,
		CAMSV_MODULE_EN, UFE_EN, 0);

	/* dma performance */
	CAMSV_WRITE_REG(dev->base + REG_CAMSV_SPECIAL_FUN_EN, 0x4000000);

	return ret;
}

int mtk_cam_sv_dmao_config(
	struct mtk_camsv_device *dev,
	struct mtkcam_ipi_input_param *cfg_in_param,
	int hw_scen, int raw_imgo_stride)
{
	int ret = 0;

	/* imgo dma setting */
	CAMSV_WRITE_REG(dev->base + REG_CAMSV_IMGO_XSIZE,
		mtk_cam_sv_xsize_cal(cfg_in_param) - 1);
	CAMSV_WRITE_REG(dev->base + REG_CAMSV_IMGO_YSIZE,
		cfg_in_param->in_crop.s.h - 1);
	CAMSV_WRITE_REG(dev->base + REG_CAMSV_IMGO_STRIDE,
		mtk_cam_sv_xsize_cal(cfg_in_param));

	/* check raw's imgo stride */
	if (hw_scen & MTK_CAMSV_SUPPORTED_SPECIAL_HW_SCENARIO) {
		if (raw_imgo_stride > mtk_cam_sv_xsize_cal(cfg_in_param)) {
			dev_info(dev->dev, "Special feature:0x%x, raw/sv stride = %d(THIS)/%d\n",
				hw_scen, raw_imgo_stride, mtk_cam_sv_xsize_cal(cfg_in_param));
			CAMSV_WRITE_REG(dev->base + REG_CAMSV_IMGO_STRIDE,
				raw_imgo_stride);
		}
	}

	dev_info(dev->dev, "xsize:%d ysize:%d stride:%d\n",
		CAMSV_READ_REG(dev->base + REG_CAMSV_IMGO_XSIZE),
		CAMSV_READ_REG(dev->base + REG_CAMSV_IMGO_YSIZE),
		CAMSV_READ_REG(dev->base + REG_CAMSV_IMGO_STRIDE));

	/* imgo crop */
	CAMSV_WRITE_REG(dev->base + REG_CAMSV_IMGO_CROP, 0);

	/* imgo con */
	if (dev->id >= 0 && dev->id < 10) {
		CAMSV_WRITE_REG(dev->base + REG_CAMSV_IMGO_CON0, 0x10000300);
		CAMSV_WRITE_REG(dev->base + REG_CAMSV_IMGO_CON1, 0x00C00060);
		CAMSV_WRITE_REG(dev->base + REG_CAMSV_IMGO_CON2, 0x01800120);
		CAMSV_WRITE_REG(dev->base + REG_CAMSV_IMGO_CON3, 0x020001A0);
		CAMSV_WRITE_REG(dev->base + REG_CAMSV_IMGO_CON4, 0x012000C0);
	} else {
		CAMSV_WRITE_REG(dev->base + REG_CAMSV_IMGO_CON0, 0x10000080);
		CAMSV_WRITE_REG(dev->base + REG_CAMSV_IMGO_CON1, 0x00200010);
		CAMSV_WRITE_REG(dev->base + REG_CAMSV_IMGO_CON2, 0x00400030);
		CAMSV_WRITE_REG(dev->base + REG_CAMSV_IMGO_CON3, 0x00550045);
		CAMSV_WRITE_REG(dev->base + REG_CAMSV_IMGO_CON4, 0x00300020);
	}

	return ret;
}

int mtk_cam_sv_fbc_config(
	struct mtk_camsv_device *dev,
	struct mtkcam_ipi_input_param *cfg_in_param)
{
	int ret = 0;

	CAMSV_WRITE_REG(dev->base + REG_CAMSV_FBC_IMGO_CTL1, 0);

	return ret;
}

int mtk_cam_sv_tg_enable(
	struct mtk_camsv_device *dev,
	struct mtkcam_ipi_input_param *cfg_in_param)
{
	int ret = 0;

	CAMSV_WRITE_BITS(dev->base + REG_CAMSV_TG_SEN_MODE,
		CAMSV_TG_SEN_MODE, CMOS_EN, 1);

	return ret;
}

int mtk_cam_sv_toggle_tg_db(struct mtk_camsv_device *dev)
{
	int val, val2;

	val = CAMSV_READ_REG(dev->base_inner + REG_CAMSV_TG_PATH_CFG);
	CAMSV_WRITE_BITS(dev->base + REG_CAMSV_TG_PATH_CFG,
		CAMSV_TG_PATH_CFG, DB_LOAD_DIS, 1);

	CAMSV_WRITE_BITS(dev->base + REG_CAMSV_TG_PATH_CFG,
		CAMSV_TG_PATH_CFG, DB_LOAD_DIS, 0);
	val2 = CAMSV_READ_REG(dev->base_inner + REG_CAMSV_TG_PATH_CFG);
	dev_info(dev->dev, "%s 0x%x->0x%x\n", __func__, val, val2);

	return 0;
}

int mtk_cam_sv_toggle_db(struct mtk_camsv_device *dev)
{
	int val, val2;

	val = CAMSV_READ_REG(dev->base_inner + REG_CAMSV_MODULE_EN);
	CAMSV_WRITE_BITS(dev->base + REG_CAMSV_MODULE_EN,
		CAMSV_MODULE_EN, DB_EN, 0);

	CAMSV_WRITE_BITS(dev->base + REG_CAMSV_MODULE_EN,
		CAMSV_MODULE_EN, DB_EN, 1);
	val2 = CAMSV_READ_REG(dev->base_inner + REG_CAMSV_MODULE_EN);
	dev_info(dev->dev, "%s 0x%x->0x%x\n", __func__, val, val2);

	return 0;
}

int mtk_cam_sv_top_enable(struct mtk_camsv_device *dev)
{
	int ret = 0;

	CAMSV_WRITE_BITS(dev->base + REG_CAMSV_CLK_EN,
		CAMSV_CLK_EN, TG_DP_CK_EN, 1);
	CAMSV_WRITE_BITS(dev->base + REG_CAMSV_CLK_EN,
		CAMSV_CLK_EN, QBN_DP_CK_EN, 1);
	CAMSV_WRITE_BITS(dev->base + REG_CAMSV_CLK_EN,
		CAMSV_CLK_EN, PAK_DP_CK_EN, 1);
	CAMSV_WRITE_BITS(dev->base + REG_CAMSV_CLK_EN,
		CAMSV_CLK_EN, IMGO_DP_CK_EN, 1);

	mtk_cam_sv_toggle_db(dev);
	mtk_cam_sv_toggle_tg_db(dev);

	CAMSV_WRITE_BITS(dev->base + REG_CAMSV_FBC_IMGO_CTL1,
		CAMSV_FBC_IMGO_CTL1, FBC_DB_EN, 1);

	dev_info(dev->dev, "%s FBC_IMGO_CTRL1/2:0x%x/0x%x\n",
		__func__,
		CAMSV_READ_REG(dev->base + REG_CAMSV_FBC_IMGO_CTL1),
		CAMSV_READ_REG(dev->base + REG_CAMSV_FBC_IMGO_CTL2));

	if (CAMSV_READ_BITS(dev->base + REG_CAMSV_TG_SEN_MODE,
		CAMSV_TG_SEN_MODE, CMOS_EN) && dev->is_enqueued) {
		CAMSV_WRITE_BITS(dev->base + REG_CAMSV_TG_VF_CON,
			CAMSV_TG_VF_CON, VFDATA_EN, 1);
	}

	return ret;
}

int mtk_cam_sv_dmao_enable(
	struct mtk_camsv_device *dev,
	struct mtkcam_ipi_input_param *cfg_in_param)
{
	int ret = 0;

	CAMSV_WRITE_BITS(dev->base + REG_CAMSV_MODULE_EN,
		CAMSV_MODULE_EN, IMGO_EN, 1);

	return ret;
}

int mtk_cam_sv_fbc_enable(
	struct mtk_camsv_device *dev,
	struct mtkcam_ipi_input_param *cfg_in_param)
{
	int ret = 0;

	if (CAMSV_READ_BITS(dev->base + REG_CAMSV_TG_VF_CON,
			CAMSV_TG_VF_CON, VFDATA_EN) == 1) {
		ret = -1;
		dev_dbg(dev->dev, "cannot enable fbc when streaming");
		goto EXIT;
	}
	CAMSV_WRITE_BITS(dev->base + REG_CAMSV_FBC_IMGO_CTL1,
		CAMSV_FBC_IMGO_CTL1, SUB_RATIO, cfg_in_param->subsample);
	CAMSV_WRITE_BITS(dev->base + REG_CAMSV_FBC_IMGO_CTL1,
		CAMSV_FBC_IMGO_CTL1, FBC_EN, 1);
	CAMSV_WRITE_BITS(dev->base + REG_CAMSV_FBC_IMGO_CTL1,
		CAMSV_FBC_IMGO_CTL1, FBC_DB_EN, 0);

EXIT:
	return ret;
}

int mtk_cam_sv_tg_disable(struct mtk_camsv_device *dev)
{
	int ret = 0;

	CAMSV_WRITE_BITS(dev->base + REG_CAMSV_TG_SEN_MODE,
		CAMSV_TG_SEN_MODE, CMOS_EN, 0);

	return ret;
}

int mtk_cam_sv_top_disable(struct mtk_camsv_device *dev)
{
	int ret = 0;

	if (CAMSV_READ_BITS(dev->base + REG_CAMSV_TG_VF_CON,
			CAMSV_TG_VF_CON, VFDATA_EN)) {
		CAMSV_WRITE_BITS(dev->base + REG_CAMSV_TG_VF_CON,
			CAMSV_TG_VF_CON, VFDATA_EN, 0);
		mtk_cam_sv_toggle_tg_db(dev);
	}

	sv_reset(dev);

	CAMSV_WRITE_BITS(dev->base + REG_CAMSV_MODULE_EN,
		CAMSV_MODULE_EN, DB_EN, 0);
	CAMSV_WRITE_REG(dev->base + REG_CAMSV_MODULE_EN, 0);
	CAMSV_WRITE_REG(dev->base + REG_CAMSV_FMT_SEL, 0);
	CAMSV_WRITE_REG(dev->base + REG_CAMSV_INT_EN, 0);
	CAMSV_WRITE_REG(dev->base + REG_CAMSV_FBC_IMGO_CTL1, 0);
	CAMSV_WRITE_BITS(dev->base + REG_CAMSV_MODULE_EN,
		CAMSV_MODULE_EN, DB_EN, 1);

	CAMSV_WRITE_BITS(dev->base + REG_CAMSV_CLK_EN,
		CAMSV_CLK_EN, TG_DP_CK_EN, 0);
	CAMSV_WRITE_BITS(dev->base + REG_CAMSV_CLK_EN,
		CAMSV_CLK_EN, PAK_DP_CK_EN, 0);
	CAMSV_WRITE_BITS(dev->base + REG_CAMSV_CLK_EN,
		CAMSV_CLK_EN, IMGO_DP_CK_EN, 0);

	return ret;
}

int mtk_cam_sv_dmao_disable(struct mtk_camsv_device *dev)
{
	int ret = 0;

	CAMSV_WRITE_BITS(dev->base + REG_CAMSV_MODULE_EN,
		CAMSV_MODULE_EN, IMGO_EN, 0);

	return ret;
}

int mtk_cam_sv_fbc_disable(struct mtk_camsv_device *dev)
{
	int ret = 0;

	CAMSV_WRITE_REG(dev->base + REG_CAMSV_FBC_IMGO_CTL1, 0);

	return ret;
}

int mtk_cam_sv_vf_on(struct mtk_camsv_device *dev, unsigned int is_on)
{
	int ret = 0;

	if (is_on) {
		if (CAMSV_READ_BITS(dev->base + REG_CAMSV_TG_VF_CON,
			CAMSV_TG_VF_CON, VFDATA_EN) == 0) {
			CAMSV_WRITE_BITS(dev->base + REG_CAMSV_TG_VF_CON,
				CAMSV_TG_VF_CON, VFDATA_EN, 1);
		}
	} else {
		if (CAMSV_READ_BITS(dev->base + REG_CAMSV_TG_VF_CON,
			CAMSV_TG_VF_CON, VFDATA_EN) == 1) {
			CAMSV_WRITE_BITS(dev->base + REG_CAMSV_TG_VF_CON,
				CAMSV_TG_VF_CON, VFDATA_EN, 0);
		}
	}

	return ret;
}

int mtk_cam_sv_is_vf_on(struct mtk_camsv_device *dev)
{
	if (CAMSV_READ_BITS(dev->base + REG_CAMSV_TG_VF_CON,
			CAMSV_TG_VF_CON, VFDATA_EN) == 1)
		return 1;
	else
		return 0;
}

int mtk_cam_sv_enquehwbuf(
	struct mtk_camsv_device *dev,
	dma_addr_t ba,
	unsigned int seq_no)
{
	int ret = 0;
	union CAMSV_TOP_FBC_CNT_SET reg;

	reg.Raw = 0;

	CAMSV_WRITE_REG(dev->base + REG_CAMSV_FRAME_SEQ_NO, seq_no);
	CAMSV_WRITE_REG(dev->base + REG_CAMSV_IMGO_BASE_ADDR, ba & 0xFFFFFFFF);
	CAMSV_WRITE_REG(dev->base + REG_CAMSV_IMGO_BASE_ADDR_MSB, (ba >> 32) & 0xF);
	reg.Bits.RCNT_INC1 = 1;
	CAMSV_WRITE_REG(dev->base + REG_CAMSV_TOP_FBC_CNT_SET, reg.Raw);

	return ret;
}

int mtk_cam_sv_write_rcnt(struct mtk_cam_ctx *ctx, unsigned int pipe_id)
{
	int ret = 0;
	union CAMSV_TOP_FBC_CNT_SET reg;
	struct device *dev_sv;
	struct mtk_camsv_device *camsv_dev;

	dev_sv = ctx->cam->sv.devs[pipe_id - MTKCAM_SUBDEV_CAMSV_START];
	camsv_dev = dev_get_drvdata(dev_sv);

	reg.Raw = 0;
	reg.Bits.RCNT_INC1 = 1;
	CAMSV_WRITE_REG(camsv_dev->base + REG_CAMSV_TOP_FBC_CNT_SET, reg.Raw);

	return ret;
}

int mtk_cam_sv_cal_cfg_info(struct mtk_cam_ctx *ctx,
	const struct v4l2_format *img_fmt,
	struct mtk_camsv_frame_params *params)
{
	int ret = 0;
	union CAMSV_FMT_SEL fmt;
	struct mtkcam_ipi_input_param cfg_in_param;
	unsigned int pxl, lin;

	fmt.Raw = mtk_cam_sv_format_sel(img_fmt->fmt.pix_mp.pixelformat);

	cfg_in_param.fmt = fmt.Raw;
	cfg_in_param.in_crop.p.x = 0;
	cfg_in_param.in_crop.p.y = 0;
	cfg_in_param.in_crop.s.w = img_fmt->fmt.pix_mp.width;
	cfg_in_param.in_crop.s.h = img_fmt->fmt.pix_mp.height;
	cfg_in_param.pixel_mode = 3;

	pxl = ((cfg_in_param.in_crop.s.w + cfg_in_param.in_crop.p.x) << 16) |
			cfg_in_param.in_crop.p.x;
	lin = ((cfg_in_param.in_crop.s.h + cfg_in_param.in_crop.p.y) << 16) |
			cfg_in_param.in_crop.p.y;

	params->is_reconfig = 1;
	params->cfg_info.grab_pxl = pxl;
	params->cfg_info.grab_lin = lin;
	params->cfg_info.fmt_sel = fmt.Raw;
	params->cfg_info.pak =
		mtk_cam_sv_pak_sel(img_fmt->fmt.pix_mp.pixelformat, cfg_in_param.pixel_mode);
	params->cfg_info.imgo_xsize =
		mtk_cam_sv_xsize_cal(&cfg_in_param) - 1;
	params->cfg_info.imgo_ysize = cfg_in_param.in_crop.s.h - 1;
	params->cfg_info.imgo_stride =
		mtk_cam_sv_xsize_cal(&cfg_in_param);

	dev_info(ctx->cam->dev, "%s pxl(0x%x)lin(0x%x)fmtsel(0x%x)pak(0x%x)xsize(0x%x)ysize(0x%x)stride(0x%x)",
		__func__, params->cfg_info.grab_pxl, params->cfg_info.grab_lin,
		params->cfg_info.fmt_sel, params->cfg_info.pak,
		params->cfg_info.imgo_xsize, params->cfg_info.imgo_ysize,
		params->cfg_info.imgo_stride);

	return ret;
}

int mtk_cam_sv_setup_cfg_info(struct mtk_camsv_device *dev,
	struct mtk_cam_request_stream_data *s_data)
{
	int ret = 0;

	if (s_data->sv_frame_params.is_reconfig) {
		dev_dbg(dev->dev, "%s +", __func__);
		CAMSV_WRITE_BITS(dev->base + REG_CAMSV_TG_PATH_CFG,
			CAMSV_TG_PATH_CFG, DB_LOAD_HOLD, 1);
		CAMSV_WRITE_REG(dev->base + REG_CAMSV_TG_SEN_GRAB_PXL,
			s_data->sv_frame_params.cfg_info.grab_pxl);
		CAMSV_WRITE_REG(dev->base + REG_CAMSV_TG_SEN_GRAB_LIN,
			s_data->sv_frame_params.cfg_info.grab_lin);
		CAMSV_WRITE_REG(dev->base + REG_CAMSV_FMT_SEL,
			s_data->sv_frame_params.cfg_info.fmt_sel);
		CAMSV_WRITE_REG(dev->base + REG_CAMSV_PAK,
			s_data->sv_frame_params.cfg_info.pak);
		CAMSV_WRITE_REG(dev->base + REG_CAMSV_IMGO_XSIZE,
			s_data->sv_frame_params.cfg_info.imgo_xsize);
		CAMSV_WRITE_REG(dev->base + REG_CAMSV_IMGO_YSIZE,
			s_data->sv_frame_params.cfg_info.imgo_ysize);
		CAMSV_WRITE_REG(dev->base + REG_CAMSV_IMGO_STRIDE,
			s_data->sv_frame_params.cfg_info.imgo_stride);
		CAMSV_WRITE_BITS(dev->base + REG_CAMSV_TG_PATH_CFG,
			CAMSV_TG_PATH_CFG, DB_LOAD_HOLD, 0);
		dev_dbg(dev->dev, "%s -", __func__);
	}

	return ret;
}

int mtk_cam_find_sv_dev_index(
	struct mtk_cam_ctx *ctx,
	unsigned int idx)
{
	int i;

	for (i = 0 ; i < MAX_SV_PIPES_PER_STREAM ; i++) {
		if (ctx->used_sv_dev[i] == (1 << idx))
			return i;
	}

	return -1;
}

int mtk_cam_sv_update_all_buffer_ts(struct mtk_cam_ctx *ctx, u64 ts_ns)
{
	struct mtk_camsv_working_buf_entry *buf_entry;
	int i;

	for (i = 0; i < ctx->used_sv_num; i++) {
		spin_lock(&ctx->sv_using_buffer_list[i].lock);
		if (list_empty(&ctx->sv_using_buffer_list[i].list)) {
			spin_unlock(&ctx->sv_using_buffer_list[i].lock);
			return 0;
		}
		buf_entry = list_first_entry(&ctx->sv_using_buffer_list[i].list,
				struct mtk_camsv_working_buf_entry, list_entry);
		buf_entry->ts_raw = ts_ns;
		spin_unlock(&ctx->sv_using_buffer_list[i].lock);
	}

	return 1;
}

int mtk_cam_sv_apply_all_buffers(struct mtk_cam_ctx *ctx)
{
	unsigned int seq_no;
	dma_addr_t base_addr;
	struct mtk_camsv_working_buf_entry *buf_entry, *buf_entry_prev;
	struct mtk_camsv_device *camsv_dev;
	int i;

	for (i = 0; i < ctx->used_sv_num; i++) {
		camsv_dev = get_camsv_dev(ctx->cam, ctx->sv_pipe[i]);

		spin_lock(&ctx->sv_using_buffer_list[i].lock);
		if (list_empty(&ctx->sv_using_buffer_list[i].list)) {
			spin_unlock(&ctx->sv_using_buffer_list[i].lock);
			return 0;
		}
		list_for_each_entry_safe(buf_entry, buf_entry_prev,
			&ctx->sv_using_buffer_list[i].list, list_entry) {
			if (atomic_read(&buf_entry->is_apply) == 0) {
				atomic_set(&buf_entry->is_apply, 1);
				break;
			}
		}
		buf_entry = list_first_entry(&ctx->sv_using_buffer_list[i].list,
				struct mtk_camsv_working_buf_entry, list_entry);
		if (mtk_cam_sv_is_vf_on(camsv_dev) &&
			(ctx->used_raw_num != 0)) {
			if (buf_entry->is_stagger == 0 ||
				(buf_entry->is_stagger == 1 && STAGGER_CQ_LAST_SOF == 0)) {
				if ((buf_entry->ts_sv == 0) ||
					((buf_entry->ts_sv < buf_entry->ts_raw) &&
					((buf_entry->ts_raw - buf_entry->ts_sv) > 10000000))) {
					dev_dbg(ctx->cam->dev, "%s pipe_id:%d ts_raw:%lld ts_sv:%lld is_apply:%d",
						__func__, ctx->sv_pipe[i]->id,
						buf_entry->ts_raw, buf_entry->ts_sv,
						atomic_read(&buf_entry->is_apply));
					spin_unlock(&ctx->sv_using_buffer_list[i].lock);
					continue;
				}
			}
		}
		list_del(&buf_entry->list_entry);
		ctx->sv_using_buffer_list[i].cnt--;
		spin_unlock(&ctx->sv_using_buffer_list[i].lock);
		spin_lock(&ctx->sv_processing_buffer_list[i].lock);
		list_add_tail(&buf_entry->list_entry,
				&ctx->sv_processing_buffer_list[i].list);
		ctx->sv_processing_buffer_list[i].cnt++;
		spin_unlock(&ctx->sv_processing_buffer_list[i].lock);

		if (buf_entry->s_data->req->pipe_used & (1 << ctx->sv_pipe[i]->id)) {
			if (buf_entry->s_data->frame_seq_no == 1) {
				seq_no = buf_entry->s_data->frame_seq_no;
				base_addr =
					buf_entry->s_data->sv_frame_params.img_out.buf[0][0].iova;
				camsv_dev->is_enqueued = 1;
				mtk_cam_sv_enquehwbuf(camsv_dev, base_addr, seq_no);
				/* initial request readout will be delayed 1 frame */
				if (ctx->used_raw_num && !mtk_cam_is_subsample(ctx) &&
					!mtk_cam_is_stagger(ctx) && !mtk_cam_is_stagger_m2m(ctx) &&
					!mtk_cam_is_time_shared(ctx) && !mtk_cam_is_mstream(ctx))
					mtk_cam_sv_write_rcnt(ctx, ctx->sv_pipe[i]->id);
			} else {
				if (ctx->sv_wq)
					queue_work(ctx->sv_wq, &buf_entry->s_data->sv_work.work);
			}
		} else {
			/* under seamless/sat case, to turn off vf */
			if (ctx->sv_wq)
				queue_work(ctx->sv_wq, &buf_entry->s_data->sv_work.work);
		}
	}

	return 1;
}

int mtk_cam_sv_apply_next_buffer(struct mtk_cam_ctx *ctx,
	unsigned int pipe_id, u64 ts_ns)
{
	unsigned int seq_no;
	dma_addr_t base_addr;
	struct mtk_camsv_working_buf_entry *buf_entry;
	struct mtk_camsv_device *camsv_dev;
	struct mtk_cam_request_stream_data *s_data;
	int i;

	for (i = 0; i < ctx->used_sv_num; i++) {
		if (ctx->sv_pipe[i]->id == pipe_id) {
			camsv_dev = get_camsv_dev(ctx->cam, ctx->sv_pipe[i]);

			spin_lock(&ctx->sv_using_buffer_list[i].lock);
			if (list_empty(&ctx->sv_using_buffer_list[i].list)) {
				spin_unlock(&ctx->sv_using_buffer_list[i].lock);
				return 0;
			}
			buf_entry = list_first_entry(&ctx->sv_using_buffer_list[i].list,
					struct mtk_camsv_working_buf_entry, list_entry);
			buf_entry->ts_sv = ts_ns;
			if (((buf_entry->ts_raw == 0) && (ctx->used_raw_num != 0)) ||
				((atomic_read(&buf_entry->is_apply) == 0) &&
				(ctx->used_raw_num != 0)) ||
				((buf_entry->ts_sv < buf_entry->ts_raw) &&
				((buf_entry->ts_raw - buf_entry->ts_sv) > 10000000))) {
				dev_dbg(ctx->cam->dev, "%s pipe_id:%d ts_raw:%lld ts_sv:%lld",
					__func__, ctx->sv_pipe[i]->id,
					buf_entry->ts_raw, buf_entry->ts_sv);
				spin_unlock(&ctx->sv_using_buffer_list[i].lock);
				return 1;
			}
			list_del(&buf_entry->list_entry);
			ctx->sv_using_buffer_list[i].cnt--;
			spin_unlock(&ctx->sv_using_buffer_list[i].lock);
			spin_lock(&ctx->sv_processing_buffer_list[i].lock);
			list_add_tail(&buf_entry->list_entry,
					&ctx->sv_processing_buffer_list[i].list);
			ctx->sv_processing_buffer_list[i].cnt++;
			spin_unlock(&ctx->sv_processing_buffer_list[i].lock);

			if (buf_entry->s_data->req->pipe_used & (1 << ctx->sv_pipe[i]->id)) {
				if (buf_entry->s_data->frame_seq_no == 1) {
					s_data = buf_entry->s_data;
					seq_no = s_data->frame_seq_no;
					base_addr =
						s_data->sv_frame_params.img_out.buf[0][0].iova;
					camsv_dev->is_enqueued = 1;
					mtk_cam_sv_enquehwbuf(camsv_dev, base_addr, seq_no);
					/* initial request readout will be delayed 1 frame */
					if (ctx->used_raw_num && !mtk_cam_is_subsample(ctx) &&
						!mtk_cam_is_stagger(ctx) &&
						!mtk_cam_is_stagger_m2m(ctx) &&
						!mtk_cam_is_time_shared(ctx) &&
						!mtk_cam_is_mstream(ctx))
						mtk_cam_sv_write_rcnt(ctx, ctx->sv_pipe[i]->id);
				} else {
					if (ctx->sv_wq)
						queue_work(ctx->sv_wq,
							&buf_entry->s_data->sv_work.work);
				}
			} else {
				/* under seamless/sat case, to turn off vf */
				if (ctx->sv_wq)
					queue_work(ctx->sv_wq, &buf_entry->s_data->sv_work.work);
			}
			break;
		}
	}

	return 1;
}

int mtk_cam_sv_rgbw_apply_next_buffer(
	struct mtk_cam_request_stream_data *s_data)
{
	struct mtk_cam_ctx *ctx = s_data->ctx;
	struct mtk_camsv_device *camsv_dev;
	dma_addr_t base_addr;
	unsigned int seq_no;
	int i, ret = 0;

	for (i = MTKCAM_SUBDEV_CAMSV_START; i < MTKCAM_SUBDEV_CAMSV_END; i++) {
		if (ctx->pipe->enabled_raw & (1 << i)) {
			camsv_dev = get_camsv_dev(
				ctx->cam, &ctx->cam->sv.pipelines[i - MTKCAM_SUBDEV_CAMSV_START]);
			base_addr = s_data->sv_frame_params.img_out.buf[0][0].iova;
			seq_no = s_data->frame_seq_no;
			mtk_cam_sv_enquehwbuf(camsv_dev, base_addr, seq_no);
			ret = 1;
		}
	}

	return ret;
}

int mtk_cam_sv_apply_switch_buffers(struct mtk_cam_ctx *ctx)
{
	unsigned int seq_no;
	dma_addr_t base_addr;
	struct mtk_camsv_working_buf_entry *buf_entry;
	struct mtk_camsv_device *camsv_dev;
	int i;

	for (i = 0; i < ctx->used_sv_num; i++) {
		camsv_dev = get_camsv_dev(ctx->cam, ctx->sv_pipe[i]);

		spin_lock(&ctx->sv_using_buffer_list[i].lock);
		if (list_empty(&ctx->sv_using_buffer_list[i].list)) {
			spin_unlock(&ctx->sv_using_buffer_list[i].lock);
			return 0;
		}
		buf_entry = list_first_entry(&ctx->sv_using_buffer_list[i].list,
				struct mtk_camsv_working_buf_entry, list_entry);
		list_del(&buf_entry->list_entry);
		ctx->sv_using_buffer_list[i].cnt--;
		spin_unlock(&ctx->sv_using_buffer_list[i].lock);
		spin_lock(&ctx->sv_processing_buffer_list[i].lock);
		list_add_tail(&buf_entry->list_entry,
				&ctx->sv_processing_buffer_list[i].list);
		ctx->sv_processing_buffer_list[i].cnt++;
		spin_unlock(&ctx->sv_processing_buffer_list[i].lock);

		if (buf_entry->s_data->req->pipe_used & (1 << ctx->sv_pipe[i]->id)) {
			seq_no = buf_entry->s_data->frame_seq_no;
			base_addr =
				buf_entry->s_data->sv_frame_params.img_out.buf[0][0].iova;
			camsv_dev->is_enqueued = 1;
			mtk_cam_sv_enquehwbuf(camsv_dev, base_addr, seq_no);
		}
	}

	return 1;
}

int mtk_cam_sv_dev_config(
	struct mtk_cam_ctx *ctx,
	unsigned int idx,
	unsigned int hw_scen,
	unsigned int exp_order)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct device *dev = cam->dev;
	struct mtkcam_ipi_input_param cfg_in_param;
	struct v4l2_mbus_framefmt *mf;
	struct device *dev_sv;
	struct mtk_camsv_device *camsv_dev;
	struct v4l2_format *img_fmt;
	unsigned int i;
	int ret, pad_idx, pixel_mode = 0;

	if (hw_scen & MTK_CAMSV_SUPPORTED_SPECIAL_HW_SCENARIO) {
		if (hw_scen & (1 << MTKCAM_SV_SPECIAL_SCENARIO_ADDITIONAL_RAW)) {
			img_fmt = &ctx->pipe->vdev_nodes[
				MTK_RAW_MAIN_STREAM_SV_1_OUT - MTK_RAW_SINK_NUM].active_fmt;
			pad_idx = PAD_SRC_RAW_W0;
			mf = &ctx->pipe->cfg[MTK_RAW_SINK].mbus_fmt;
		} else {
			img_fmt = &ctx->pipe->vdev_nodes[MTK_RAW_MAIN_STREAM_OUT - MTK_RAW_SINK_NUM]
				.active_fmt;
			pad_idx = PAD_SRC_RAW0;
			mf = &ctx->pipe->cfg[MTK_RAW_SINK].mbus_fmt;
		}
	} else {
		img_fmt = &ctx->sv_pipe[idx]
			->vdev_nodes[MTK_CAMSV_MAIN_STREAM_OUT-MTK_CAMSV_SINK_NUM].active_fmt;
		pad_idx = ctx->sv_pipe[idx]->seninf_padidx;
		mf = &ctx->sv_pipe[idx]->cfg[MTK_CAMSV_SINK].mbus_fmt;
	}

	/* Update cfg_in_param */
	mtk_cam_seninf_get_pixelmode(ctx->seninf, pad_idx, &pixel_mode);
	cfg_in_param.pixel_mode = pixel_mode;
	cfg_in_param.data_pattern = 0x0;
	cfg_in_param.in_crop.p.x = 0;
	cfg_in_param.in_crop.p.y = 0;
	cfg_in_param.in_crop.s.w = img_fmt->fmt.pix_mp.width;
	cfg_in_param.in_crop.s.h = img_fmt->fmt.pix_mp.height;
	dev_info(dev, "sink pad code:0x%x raw's imgo stride:%d\n", mf->code,
		img_fmt->fmt.pix_mp.plane_fmt[0].bytesperline);
	cfg_in_param.raw_pixel_id = mtk_cam_get_sensor_pixel_id(mf->code);
	cfg_in_param.subsample = 0;
	cfg_in_param.fmt = img_fmt->fmt.pix_mp.pixelformat;

	if (cfg_in_param.in_crop.s.w % (1 << pixel_mode))
		dev_info(dev, "crop width(%d) is not the multiple of pixel mode(%d)\n",
			cfg_in_param.in_crop.s.w, pixel_mode);

	if (hw_scen & MTK_CAMSV_SUPPORTED_SPECIAL_HW_SCENARIO) {
		pm_runtime_get_sync(cam->sv.devs[idx]);
	} else {
		ret = mtk_cam_sv_pipeline_config(ctx, idx, &cfg_in_param);
		if (ret)
			return ret;
	}

	if (hw_scen & MTK_CAMSV_SUPPORTED_SPECIAL_HW_SCENARIO) {
		dev_sv = cam->sv.devs[idx];
		if (dev_sv == NULL) {
			dev_dbg(dev, "config camsv device not found\n");
			return -EINVAL;
		}
		camsv_dev = dev_get_drvdata(dev_sv);
		camsv_dev->pipeline = &cam->sv.pipelines[idx];
		camsv_dev->pipeline->hw_scen = hw_scen;
		camsv_dev->pipeline->master_pipe_id = ctx->pipe->id;
		camsv_dev->pipeline->exp_order = exp_order;
	} else {
		dev_sv = mtk_cam_find_sv_dev(cam, ctx->used_sv_dev[idx]);
		if (dev_sv == NULL) {
			dev_dbg(dev, "config camsv device not found\n");
			return -EINVAL;
		}
		camsv_dev = dev_get_drvdata(dev_sv);
		for (i = 0; i < CAMSV_PIPELINE_NUM; i++)
			if (cam->sv.pipelines[i].enabled_sv & 1<<camsv_dev->id) {
				camsv_dev->pipeline = &cam->sv.pipelines[i];
				break;
			}
		camsv_dev->pipeline->hw_scen = hw_scen;
		camsv_dev->pipeline->master_pipe_id = 0;
		camsv_dev->pipeline->exp_order = 0;
	}

	/* reset enqueued status */
	camsv_dev->is_enqueued = 0;
	reset_msgfifo(camsv_dev);

	mtk_cam_sv_tg_config(camsv_dev, &cfg_in_param);
	mtk_cam_sv_top_config(camsv_dev, &cfg_in_param);
	mtk_cam_sv_dmao_config(camsv_dev, &cfg_in_param,
				hw_scen, img_fmt->fmt.pix_mp.plane_fmt[0].bytesperline);
	mtk_cam_sv_fbc_config(camsv_dev, &cfg_in_param);
	mtk_cam_sv_tg_enable(camsv_dev, &cfg_in_param);
	mtk_cam_sv_dmao_enable(camsv_dev, &cfg_in_param);
	mtk_cam_sv_fbc_enable(camsv_dev, &cfg_in_param);

	mtk_camsv_register_iommu_tf_callback(camsv_dev);

	dev_info(dev, "camsv %d %s done\n", camsv_dev->id, __func__);

	return 0;
}

int mtk_cam_sv_dev_stream_on(
	struct mtk_cam_ctx *ctx,
	unsigned int idx,
	unsigned int streaming,
	unsigned int hw_scen)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct device *dev = cam->dev;
	struct device *dev_sv;
	struct mtk_camsv_device *camsv_dev;
	unsigned int i;
	int ret = 0;

	if (hw_scen & MTK_CAMSV_SUPPORTED_SPECIAL_HW_SCENARIO) {
		dev_sv = cam->sv.devs[idx];
		if (dev_sv == NULL) {
			dev_dbg(dev, "stream on camsv device not found\n");
			return -EINVAL;
		}
		camsv_dev = dev_get_drvdata(dev_sv);
		camsv_dev->is_enqueued = 1;
	} else {
		dev_sv = mtk_cam_find_sv_dev(cam, ctx->used_sv_dev[idx]);
		if (dev_sv == NULL) {
			dev_dbg(dev, "stream on camsv device not found\n");
			return -EINVAL;
		}
		camsv_dev = dev_get_drvdata(dev_sv);
		for (i = 0; i < CAMSV_PIPELINE_NUM; i++)
			if (cam->sv.pipelines[i].enabled_sv & 1<<camsv_dev->id) {
				camsv_dev->pipeline = &cam->sv.pipelines[i];
				break;
			}
	}

	if (streaming)
		ret = mtk_cam_sv_top_enable(camsv_dev);
	else {
		/* reset enqueued status */
		camsv_dev->is_enqueued = 0;

		ret = mtk_cam_sv_top_disable(camsv_dev) ||
			mtk_cam_sv_fbc_disable(camsv_dev) ||
			mtk_cam_sv_dmao_disable(camsv_dev) ||
			mtk_cam_sv_tg_disable(camsv_dev);
		pm_runtime_put_sync(camsv_dev->dev);
	}

	dev_info(dev, "camsv %d %s en(%d)\n", camsv_dev->id, __func__, streaming);

	return ret;
}

static void mtk_camsv_pipeline_queue_setup(
	struct mtk_camsv_pipeline *pipe)
{
	unsigned int node_idx, i;

	node_idx = 0;
	/* Setup the capture queue */
	for (i = 0; i < MTK_CAMSV_TOTAL_CAPTURE_QUEUES; i++) {
		pipe->vdev_nodes[node_idx].desc = sv_capture_queues[i];
		pipe->vdev_nodes[node_idx++].desc.name =
			sv_capture_queue_names[pipe->id-MTKCAM_SUBDEV_CAMSV_START][i];
	}
}

static int mtk_camsv_pipeline_register(
	unsigned int id, struct device *dev,
	struct mtk_camsv_pipeline *pipe,
	struct v4l2_device *v4l2_dev)
{
	struct mtk_cam_device *cam = dev_get_drvdata(pipe->sv->cam_dev);
	struct mtk_camsv_device *camsv_dev = dev_get_drvdata(dev);
	struct v4l2_subdev *sd = &pipe->subdev;
	struct mtk_cam_video_device *video;
	unsigned int i;
	int ret;

	pipe->id = id;
	pipe->hw_cap = camsv_dev->hw_cap;
	pipe->cammux_id = camsv_dev->cammux_id;

	/* Initialize subdev */
	v4l2_subdev_init(sd, &mtk_camsv_subdev_ops);
	sd->entity.function = MEDIA_ENT_F_PROC_VIDEO_PIXEL_FORMATTER;
	sd->entity.ops = &mtk_camsv_media_entity_ops;
	sd->flags = V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS;
	ret = snprintf(sd->name, sizeof(sd->name),
		 "%s-%d", dev_driver_string(dev), (pipe->id-MTKCAM_SUBDEV_CAMSV_START));
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

	mtk_camsv_pipeline_queue_setup(pipe);

	//setup pads of camsv pipeline
	for (i = 0; i < ARRAY_SIZE(pipe->pads); i++) {
		pipe->pads[i].flags = i < MTK_CAMSV_SOURCE_BEGIN ?
			MEDIA_PAD_FL_SINK : MEDIA_PAD_FL_SOURCE;
	}

	media_entity_pads_init(&sd->entity, ARRAY_SIZE(pipe->pads), pipe->pads);

	/* setup video node */
	for (i = 0; i < ARRAY_SIZE(pipe->vdev_nodes); i++) {
		video = pipe->vdev_nodes + i;

		if (pipe->id >= MTKCAM_SUBDEV_CAMSV_START &&
			pipe->id < MTKCAM_SUBDEV_CAMSV_END)
			video->uid.pipe_id = pipe->id;
		else {
			dev_info(dev, "invalid pipe id\n");
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

static void mtk_camsv_pipeline_unregister(
	struct mtk_camsv_pipeline *pipe)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(pipe->vdev_nodes); i++)
		mtk_cam_video_unregister(pipe->vdev_nodes + i);

	v4l2_device_unregister_subdev(&pipe->subdev);
	media_entity_cleanup(&pipe->subdev.entity);
}

int mtk_camsv_setup_dependencies(struct mtk_camsv *sv, struct mtk_larb *larb)
{
	struct device *dev = sv->cam_dev;
	struct device *consumer, *supplier;
	struct device_link *link;
	int i;

	for (i = 0; i < CAMSV_PIPELINE_NUM; i++) {
		consumer = sv->devs[i];
		if (!consumer) {
			dev_info(dev, "failed to get dev for id %d\n", i);
			continue;
		}

		switch (i) {
		case 0:
		case 1:
		case 4:
		case 5:
		case 8:
		case 9:
		case 12:
		case 13:
			supplier = find_larb(larb, 13);
			break;
		default:
			supplier = find_larb(larb, 14);
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

int mtk_camsv_register_entities(
	struct mtk_camsv *sv,
	struct v4l2_device *v4l2_dev)
{
	unsigned int i;
	int ret;

	for (i = 0; i < CAMSV_PIPELINE_NUM; i++) {
		struct mtk_camsv_pipeline *pipe = sv->pipelines + i;

		pipe->sv = sv;
		memset(pipe->cfg, 0, sizeof(*pipe->cfg));
		ret = mtk_camsv_pipeline_register(MTKCAM_SUBDEV_CAMSV_START + i,
						sv->devs[i],
						sv->pipelines + i, v4l2_dev);
		if (ret)
			return ret;
	}
	return 0;
}

void mtk_camsv_unregister_entities(struct mtk_camsv *sv)
{
	unsigned int i;

	for (i = 0; i < CAMSV_PIPELINE_NUM; i++)
		mtk_camsv_pipeline_unregister(sv->pipelines + i);
}

void camsv_dump_dma_debug_data(struct mtk_camsv_device *camsv_dev)
{
	u32 checksum, line_pix_cnt, line_pix_cnt_temp, smi_debug_data;
	u32 fifo_debug_data_1, fifo_debug_data_3, smi_crc, smi_latency;
	u32 smi_len_dle_cnt, smi_com_bvalid_cnt;

	writel_relaxed(0x00000100, camsv_dev->base + REG_CAMSV_DMATOP_DMA_DEBUG_SEL);
	checksum = readl_relaxed(camsv_dev->base + REG_CAMSV_DMATOP_DMA_DEBUG_PORT);
	writel_relaxed(0x00000200, camsv_dev->base + REG_CAMSV_DMATOP_DMA_DEBUG_SEL);
	line_pix_cnt = readl_relaxed(camsv_dev->base + REG_CAMSV_DMATOP_DMA_DEBUG_PORT);
	writel_relaxed(0x00000300, camsv_dev->base + REG_CAMSV_DMATOP_DMA_DEBUG_SEL);
	line_pix_cnt_temp = readl_relaxed(camsv_dev->base + REG_CAMSV_DMATOP_DMA_DEBUG_PORT);
	writel_relaxed(0x00000800, camsv_dev->base + REG_CAMSV_DMATOP_DMA_DEBUG_SEL);
	smi_debug_data = readl_relaxed(camsv_dev->base + REG_CAMSV_DMATOP_DMA_DEBUG_PORT);
	writel_relaxed(0x00010700, camsv_dev->base + REG_CAMSV_DMATOP_DMA_DEBUG_SEL);
	fifo_debug_data_1 = readl_relaxed(camsv_dev->base + REG_CAMSV_DMATOP_DMA_DEBUG_PORT);
	writel_relaxed(0x00030700, camsv_dev->base + REG_CAMSV_DMATOP_DMA_DEBUG_SEL);
	fifo_debug_data_3 = readl_relaxed(camsv_dev->base + REG_CAMSV_DMATOP_DMA_DEBUG_PORT);
	writel_relaxed(0x01000040, camsv_dev->base + REG_CAMSV_DMATOP_DMA_DEBUG_SEL);
	smi_crc = readl_relaxed(camsv_dev->base + REG_CAMSV_DMATOP_DMA_DEBUG_PORT);
	writel_relaxed(0x00000080, camsv_dev->base + REG_CAMSV_DMATOP_DMA_DEBUG_SEL);
	smi_latency = readl_relaxed(camsv_dev->base + REG_CAMSV_DMATOP_DMA_DEBUG_PORT);
	writel_relaxed(0x000000A0, camsv_dev->base + REG_CAMSV_DMATOP_DMA_DEBUG_SEL);
	smi_len_dle_cnt = readl_relaxed(camsv_dev->base + REG_CAMSV_DMATOP_DMA_DEBUG_PORT);
	writel_relaxed(0x000000A1, camsv_dev->base + REG_CAMSV_DMATOP_DMA_DEBUG_SEL);
	smi_com_bvalid_cnt = readl_relaxed(camsv_dev->base + REG_CAMSV_DMATOP_DMA_DEBUG_PORT);

	dev_info_ratelimited(camsv_dev->dev,
		"dma_top_debug:0x%x_0x%x_0x%x_0x%x_0x%x_0x%x_0x%x_0x%x_0x%x_0x%x\n",
		checksum, line_pix_cnt, line_pix_cnt_temp, smi_debug_data,
		fifo_debug_data_1, fifo_debug_data_3, smi_crc, smi_latency,
		smi_len_dle_cnt, smi_com_bvalid_cnt);
}

void camsv_irq_handle_err(
	struct mtk_camsv_device *camsv_dev,
	unsigned int dequeued_frame_seq_no)
{
	int val, val2;
	struct mtk_cam_request_stream_data *s_data;
	struct mtk_cam_device *cam = camsv_dev->cam;
	struct mtk_cam_ctx *ctx;
	struct mtk_raw_pipeline *raw_pipe;
	unsigned int stream_id;

	val = readl_relaxed(camsv_dev->base + REG_CAMSV_TG_PATH_CFG);
	val = val | CAMSV_TG_PATH_TG_FULL_SEL;
	writel_relaxed(val, camsv_dev->base + REG_CAMSV_TG_PATH_CFG);
	wmb(); /* TBC */
	val2 = readl_relaxed(camsv_dev->base + REG_CAMSV_TG_SEN_MODE);
	val2 = val2 | CAMSV_TG_SEN_MODE_CMOS_RDY_SEL;
	writel_relaxed(val2, camsv_dev->base + REG_CAMSV_TG_SEN_MODE);
	wmb(); /* TBC */
	dev_info_ratelimited(camsv_dev->dev,
		"TG PATHCFG/SENMODE/FRMSIZE/RGRABPXL/LIN:%x/%x/%x/%x/%x/%x\n",
		readl_relaxed(camsv_dev->base + REG_CAMSV_TG_PATH_CFG),
		readl_relaxed(camsv_dev->base + REG_CAMSV_TG_SEN_MODE),
		readl_relaxed(camsv_dev->base + REG_CAMSV_TG_FRMSIZE_ST),
		readl_relaxed(camsv_dev->base + REG_CAMSV_TG_FRMSIZE_ST_R),
		readl_relaxed(camsv_dev->base + REG_CAMSV_TG_SEN_GRAB_PXL),
		readl_relaxed(camsv_dev->base + REG_CAMSV_TG_SEN_GRAB_LIN));
	dev_info_ratelimited(camsv_dev->dev,
		"IMGO:0x%x\n",
		readl_relaxed(camsv_dev->base + REG_CAMSV_IMGO_ERR_STAT));

	if (camsv_dev->pipeline->hw_scen &
		MTK_CAMSV_SUPPORTED_SPECIAL_HW_SCENARIO) {
		raw_pipe = &cam->raw
			.pipelines[camsv_dev->pipeline->master_pipe_id];
		ctx = mtk_cam_find_ctx(cam, &raw_pipe->subdev.entity);
		if (!ctx) {
			dev_info(camsv_dev->dev, "%s: cannot find ctx\n", __func__);
			return;
		}
		stream_id = camsv_dev->pipeline->master_pipe_id;
	} else {
		ctx = mtk_cam_find_ctx(camsv_dev->cam,
			&camsv_dev->pipeline->subdev.entity);
		if (!ctx) {
			dev_info(camsv_dev->dev, "%s: cannot find ctx\n", __func__);
			return;
		}
		stream_id = camsv_dev->id + MTKCAM_SUBDEV_CAMSV_START;
	}

	s_data = mtk_cam_get_req_s_data(ctx, stream_id, dequeued_frame_seq_no);
	if (s_data) {
		mtk_cam_debug_seninf_dump(s_data);
	} else {
		dev_info(camsv_dev->dev,
			 "%s: req(%d) can't be found for seninf dump\n",
			 __func__, dequeued_frame_seq_no);
	}
}

void camsv_handle_err(
	struct mtk_camsv_device *camsv_dev,
	struct mtk_camsys_irq_info *data)
{
	int err_status = data->e.err_status;
	int frame_idx_inner = data->frame_idx_inner;

	dev_info_ratelimited(camsv_dev->dev, "error status:0x%x", err_status);

	/* show more error detail */
	camsv_irq_handle_err(camsv_dev, frame_idx_inner);

	/* dump dma debug data */
	camsv_dump_dma_debug_data(camsv_dev);
}

static irqreturn_t mtk_irq_camsv(int irq, void *data)
{
	struct mtk_camsv_device *camsv_dev = (struct mtk_camsv_device *)data;
	struct device *dev = camsv_dev->dev;
	struct mtk_camsys_irq_info irq_info;
	unsigned int dequeued_imgo_seq_no, dequeued_imgo_seq_no_inner;
	unsigned int irq_status, err_status;
	unsigned int drop_status, imgo_err_status, imgo_overr_status;
	unsigned int fbc_imgo_status, imgo_addr, imgo_addr_msb;
	unsigned int tg_sen_mode, dcif_set, tg_vf_con, tg_path_cfg;
	bool wake_thread = 0;

	irq_status	= readl_relaxed(camsv_dev->base + REG_CAMSV_INT_STATUS);
	dequeued_imgo_seq_no =
		readl_relaxed(camsv_dev->base + REG_CAMSV_FRAME_SEQ_NO);
	dequeued_imgo_seq_no_inner =
		readl_relaxed(camsv_dev->base_inner + REG_CAMSV_FRAME_SEQ_NO);
	fbc_imgo_status =
		readl_relaxed(camsv_dev->base + REG_CAMSV_FBC_IMGO_CTL2);
	imgo_addr =
		readl_relaxed(camsv_dev->base + REG_CAMSV_IMGO_BASE_ADDR);
	imgo_addr_msb =
		readl_relaxed(camsv_dev->base + REG_CAMSV_IMGO_BASE_ADDR_MSB);
	tg_sen_mode =
		readl_relaxed(camsv_dev->base_inner + REG_CAMSV_TG_SEN_MODE);
	dcif_set =
		readl_relaxed(camsv_dev->base_inner + REG_CAMSV_DCIF_SET);
	tg_vf_con =
		readl_relaxed(camsv_dev->base_inner + REG_CAMSV_TG_VF_CON);
	tg_path_cfg =
		readl_relaxed(camsv_dev->base_inner + REG_CAMSV_TG_PATH_CFG);

	err_status = irq_status & INT_ST_MASK_CAMSV_ERR;
	imgo_err_status = irq_status & CAMSV_INT_DMA_ERR_ST;
	imgo_overr_status = irq_status & CAMSV_INT_IMGO_OVERR_ST;
	drop_status = irq_status & CAMSV_INT_IMGO_DROP_ST;

	dev_dbg(dev,
		"%i status:0x%x(err:0x%x) drop:0x%x imgo_dma_err:0x%x_%x fbc:0x%x (imgo:0x%x%08x) in:%d tg_sen/dcif_set/tg_vf/tg_path:0x%x_%x_%x_%x\n",
		camsv_dev->id,
		irq_status, err_status,
		drop_status, imgo_err_status, imgo_overr_status,
		fbc_imgo_status, imgo_addr_msb, imgo_addr, dequeued_imgo_seq_no_inner,
		tg_sen_mode, dcif_set, tg_vf_con, tg_path_cfg);
	/*
	 * In normal case, the next SOF ISR should come after HW PASS1 DONE ISR.
	 * If these two ISRs come together, print warning msg to hint.
	 */
	irq_info.irq_type = 0;
	irq_info.ts_ns = ktime_get_boottime_ns();
	irq_info.frame_idx = dequeued_imgo_seq_no;
	irq_info.frame_idx_inner = dequeued_imgo_seq_no_inner;

	if ((irq_status & CAMSV_INT_TG_SOF_INT_ST) &&
		(irq_status & CAMSV_INT_PASS1_DON_ST))
		dev_dbg(dev, "sof_done block cnt:%d\n", camsv_dev->sof_count);

	/* Frame done */
	if (irq_status & CAMSV_INT_SW_PASS1_DON_ST)
		irq_info.irq_type |= (1 << CAMSYS_IRQ_FRAME_DONE);
	/* Frame start */
	if (irq_status & CAMSV_INT_TG_SOF_INT_ST) {
		irq_info.irq_type |= (1 << CAMSYS_IRQ_FRAME_START);
		camsv_dev->sof_count++;
	}

	if (irq_info.irq_type && push_msgfifo(camsv_dev, &irq_info) == 0)
		wake_thread = 1;

	/* Check ISP error status */
	if (err_status) {
		struct mtk_camsys_irq_info err_info;

		err_info.irq_type = CAMSYS_IRQ_ERROR;
		err_info.ts_ns = irq_info.ts_ns;
		err_info.frame_idx = irq_info.frame_idx;
		err_info.frame_idx_inner = irq_info.frame_idx_inner;
		err_info.e.err_status = err_status;

		if (push_msgfifo(camsv_dev, &err_info) == 0)
			wake_thread = 1;
	}

	return wake_thread ? IRQ_WAKE_THREAD : IRQ_HANDLED;
}

static irqreturn_t mtk_thread_irq_camsv(int irq, void *data)
{
	struct mtk_camsv_device *camsv_dev = (struct mtk_camsv_device *)data;
	struct mtk_camsys_irq_info irq_info;

	if (unlikely(atomic_cmpxchg(&camsv_dev->is_fifo_overflow, 1, 0)))
		dev_info(camsv_dev->dev, "msg fifo overflow\n");

	while (kfifo_len(&camsv_dev->msg_fifo) >= sizeof(irq_info)) {
		int len = kfifo_out(&camsv_dev->msg_fifo, &irq_info, sizeof(irq_info));

		WARN_ON(len != sizeof(irq_info));

		/* error case */
		if (unlikely(irq_info.irq_type == CAMSYS_IRQ_ERROR)) {
			camsv_handle_err(camsv_dev, &irq_info);
			continue;
		}

		/* normal case */

		/* inform interrupt information to camsys controller */
		mtk_camsys_isr_event(camsv_dev->cam,
				     CAMSYS_ENGINE_CAMSV, camsv_dev->id,
				     &irq_info);
	}

	return IRQ_HANDLED;
}

bool
mtk_cam_sv_finish_buf(struct mtk_cam_request_stream_data *req_stream_data)
{
	bool result = false;
	struct mtk_camsv_working_buf_entry *sv_buf_entry, *sv_buf_entry_prev;
	struct mtk_cam_ctx *ctx = req_stream_data->ctx;
	int i;

	if (!ctx->used_sv_num)
		return false;

	for (i = 0; i < ctx->used_sv_num; i++) {
		spin_lock(&ctx->sv_processing_buffer_list[i].lock);
		list_for_each_entry_safe(sv_buf_entry, sv_buf_entry_prev,
					 &ctx->sv_processing_buffer_list[i].list,
					 list_entry) {
			if (sv_buf_entry->s_data->frame_seq_no ==
				req_stream_data->frame_seq_no) {
				list_del(&sv_buf_entry->list_entry);
				mtk_cam_sv_wbuf_set_s_data(sv_buf_entry, NULL);
				mtk_cam_sv_working_buf_put(sv_buf_entry);
				ctx->sv_processing_buffer_list[i].cnt--;
				result = true;
				break;
			}
		}
		spin_unlock(&ctx->sv_processing_buffer_list[i].lock);
	}

	return result;
}


static int mtk_camsv_pm_suspend(struct device *dev)
{
	struct mtk_camsv_device *camsv_dev = dev_get_drvdata(dev);
	u32 val;
	int ret;

	dev_dbg(dev, "- %s\n", __func__);

	if (pm_runtime_suspended(dev))
		return 0;

	/* Disable ISP's view finder and wait for TG idle */
	dev_dbg(dev, "camsv suspend, disable VF\n");
	val = readl(camsv_dev->base + REG_CAMSV_TG_VF_CON);
	writel(val & (~CAMSV_TG_VF_CON_VFDATA_EN),
		camsv_dev->base + REG_CAMSV_TG_VF_CON);
	ret = readl_poll_timeout_atomic(
					camsv_dev->base + REG_CAMSV_TG_INTER_ST, val,
					(val & CAMSV_TG_CS_MASK) == CAMSV_TG_IDLE_ST,
					USEC_PER_MSEC, MTK_CAMSV_STOP_HW_TIMEOUT);
	if (ret)
		dev_dbg(dev, "can't stop HW:%d:0x%x\n", ret, val);

	/* Disable CMOS */
	val = readl(camsv_dev->base + REG_CAMSV_TG_SEN_MODE);
	writel(val & (~CAMSV_TG_SEN_MODE_CMOS_EN),
		camsv_dev->base + REG_CAMSV_TG_SEN_MODE);

	/* Force ISP HW to idle */
	ret = pm_runtime_put_sync(dev);
	return ret;
}

static int mtk_camsv_pm_resume(struct device *dev)
{
	struct mtk_camsv_device *camsv_dev = dev_get_drvdata(dev);
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
	dev_dbg(dev, "camsv resume, enable CMOS/VF\n");
	val = readl(camsv_dev->base + REG_CAMSV_TG_SEN_MODE);
	writel(val | CAMSV_TG_SEN_MODE_CMOS_EN,
		camsv_dev->base + REG_CAMSV_TG_SEN_MODE);

	/* Enable VF */
	val = readl(camsv_dev->base + REG_CAMSV_TG_VF_CON);
	writel(val | CAMSV_TG_VF_CON_VFDATA_EN,
		camsv_dev->base + REG_CAMSV_TG_VF_CON);

	return 0;
}

static int mtk_camsv_suspend_pm_event(struct notifier_block *notifier,
			unsigned long pm_event, void *unused)
{
	struct mtk_camsv_device *camsv_dev =
		container_of(notifier, struct mtk_camsv_device, notifier_blk);
	struct device *dev = camsv_dev->dev;

	switch (pm_event) {
	case PM_HIBERNATION_PREPARE:
		return NOTIFY_DONE;
	case PM_RESTORE_PREPARE:
		return NOTIFY_DONE;
	case PM_POST_HIBERNATION:
		return NOTIFY_DONE;
	case PM_SUSPEND_PREPARE: /* before enter suspend */
		mtk_camsv_pm_suspend(dev);
		return NOTIFY_DONE;
	case PM_POST_SUSPEND: /* after resume */
		mtk_camsv_pm_resume(dev);
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static int mtk_camsv_of_probe(struct platform_device *pdev,
			    struct mtk_camsv_device *sv)
{
	struct device *dev = &pdev->dev;
	struct platform_device *larb_pdev;
	struct device_node *larb_node;
	struct device_link *link;
	struct resource *res;
	unsigned int i;
	int clks, larbs, ret;

	ret = of_property_read_u32(dev->of_node, "mediatek,camsv-id",
						       &sv->id);
	if (ret) {
		dev_dbg(dev, "missing camid property\n");
		return ret;
	}

	ret = of_property_read_u32(dev->of_node, "mediatek,camsv-hwcap",
						       &sv->hw_cap);
	sv->hw_cap |= (1 << MTKCAM_SV_SPECIAL_SCENARIO_ADDITIONAL_RAW);
	if (ret) {
		dev_dbg(dev, "missing hardware capability property\n");
		return ret;
	}

	ret = of_property_read_u32(dev->of_node, "mediatek,cammux-id",
						       &sv->cammux_id);
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

	sv->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(sv->base)) {
		dev_dbg(dev, "failed to map register base\n");
		return PTR_ERR(sv->base);
	}
	dev_dbg(dev, "camsv, map_addr=0x%pK\n", sv->base);

	/* base inner register */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "inner_base");
	if (!res) {
		dev_dbg(dev, "failed to get mem\n");
		return -ENODEV;
	}

	sv->base_inner = devm_ioremap_resource(dev, res);
	if (IS_ERR(sv->base_inner)) {
		dev_dbg(dev, "failed to map register inner base\n");
		return PTR_ERR(sv->base_inner);
	}
	dev_dbg(dev, "camsv, map_addr(inner)=0x%pK\n", sv->base_inner);

	sv->irq = platform_get_irq(pdev, 0);
	if (!sv->irq) {
		dev_dbg(dev, "failed to get irq\n");
		return -ENODEV;
	}

	ret = devm_request_threaded_irq(dev, sv->irq,
					mtk_irq_camsv,
					mtk_thread_irq_camsv,
					0, dev_name(dev), sv);
	if (ret) {
		dev_dbg(dev, "failed to request irq=%d\n", sv->irq);
		return ret;
	}
	dev_dbg(dev, "registered irq=%d\n", sv->irq);

	disable_irq(sv->irq);

	clks  = of_count_phandle_with_args(pdev->dev.of_node, "clocks",
			"#clock-cells");

	sv->num_clks = (clks == -ENOENT) ? 0:clks;
	dev_info(dev, "clk_num:%d\n", sv->num_clks);

	if (sv->num_clks) {
		sv->clks = devm_kcalloc(dev, sv->num_clks, sizeof(*sv->clks),
					 GFP_KERNEL);
		if (!sv->clks)
			return -ENOMEM;
	}

	for (i = 0; i < sv->num_clks; i++) {
		sv->clks[i] = of_clk_get(pdev->dev.of_node, i);
		if (IS_ERR(sv->clks[i])) {
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

	sv->notifier_blk.notifier_call = mtk_camsv_suspend_pm_event;
	sv->notifier_blk.priority = 0;
	ret = register_pm_notifier(&sv->notifier_blk);
	if (ret) {
		dev_info(dev, "Failed to register PM notifier");
		return -ENODEV;
	}

	return 0;
}

static int mtk_camsv_component_bind(
	struct device *dev,
	struct device *master,
	void *data)
{
	struct mtk_camsv_device *camsv_dev = dev_get_drvdata(dev);
	struct mtk_cam_device *cam_dev = data;
	struct mtk_camsv *sv = &cam_dev->sv;

	dev_info(dev, "%s: id=%d\n", __func__, camsv_dev->id);

	camsv_dev->cam = cam_dev;
	sv->devs[camsv_dev->id] = dev;
	sv->cam_dev = cam_dev->dev;

	return 0;
}

static void mtk_camsv_component_unbind(
	struct device *dev,
	struct device *master,
	void *data)
{
	struct mtk_camsv_device *camsv_dev = dev_get_drvdata(dev);
	struct mtk_cam_device *cam_dev = data;
	struct mtk_camsv *sv = &cam_dev->sv;

	dev_info(dev, "%s\n", __func__);

	camsv_dev->cam = NULL;
	camsv_dev->pipeline = NULL;
	sv->devs[camsv_dev->id] = NULL;
}


static const struct component_ops mtk_camsv_component_ops = {
	.bind = mtk_camsv_component_bind,
	.unbind = mtk_camsv_component_unbind,
};

#ifdef CONFIG_MTK_IOMMU_V2
static inline int m4u_control_camsv_iommu_port(void)
{
	struct M4U_PORT_STRUCT sPort;
	int use_m4u = 1;
	int ret = 0;
	int i = 0;

	for (i = 0; i < LARB13_PORT_SIZE; i++) {
		sPort.ePortID = M4U_PORT_L13_CAM_MRAWI_MDP + i;
		sPort.Virtuality = use_m4u;
		sPort.Security = 0;
		sPort.domain = 2;
		sPort.Distance = 1;
		sPort.Direction = 0;
		ret = m4u_config_port(&sPort);
		if (ret) {
			pr_debug("config M4U Port %s to %s FAIL(ret=%d)\n",
				iommu_get_port_name(
					M4U_PORT_L13_CAM_MRAWI_MDP + i),
				use_m4u ? "virtual" : "physical", ret);
			ret = -1;
		}
	}

	for (i = 0; i < LARB14_PORT_SIZE; i++) {
		sPort.ePortID = M4U_PORT_L14_CAM_MRAWI_DISP + i;
		sPort.Virtuality = use_m4u;
		sPort.Security = 0;
		sPort.domain = 2;
		sPort.Distance = 1;
		sPort.Direction = 0;
		ret = m4u_config_port(&sPort);
		if (ret) {
			pr_debug("config M4U Port %s to %s FAIL(ret=%d)\n",
				iommu_get_port_name(
					M4U_PORT_L14_CAM_MRAWI_DISP + i),
				use_m4u ? "virtual" : "physical", ret);
			ret = -1;
		}
	}

	return ret;
}
#endif

static int mtk_camsv_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_camsv_device *camsv_dev;
	int ret;

	dev_info(dev, "%s\n", __func__);

	camsv_dev = devm_kzalloc(dev, sizeof(*camsv_dev), GFP_KERNEL);
	if (!camsv_dev)
		return -ENOMEM;

	camsv_dev->dev = dev;
	dev_set_drvdata(dev, camsv_dev);

	ret = mtk_camsv_of_probe(pdev, camsv_dev);
	if (ret)
		return ret;

	camsv_dev->fifo_size =
		roundup_pow_of_two(8 * sizeof(struct mtk_camsys_irq_info));
	camsv_dev->msg_buffer = devm_kzalloc(dev, camsv_dev->fifo_size,
					     GFP_KERNEL);
	if (!camsv_dev->msg_buffer)
		return -ENOMEM;

	pm_runtime_enable(dev);

	return component_add(dev, &mtk_camsv_component_ops);
}

static int mtk_camsv_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	dev_info(dev, "%s\n", __func__);

	pm_runtime_disable(dev);

	component_del(dev, &mtk_camsv_component_ops);
	return 0;
}

static int mtk_camsv_runtime_suspend(struct device *dev)
{
	struct mtk_camsv_device *camsv_dev = dev_get_drvdata(dev);
	int i;

	dev_dbg(dev, "%s:disable clock\n", __func__);

	for (i = 0; i < camsv_dev->num_clks; i++)
		clk_disable_unprepare(camsv_dev->clks[i]);

	disable_irq(camsv_dev->irq);

	return 0;
}

static int mtk_camsv_runtime_resume(struct device *dev)
{
	struct mtk_camsv_device *camsv_dev = dev_get_drvdata(dev);
	int i, ret;

	/* reset_msgfifo before enable_irq */
	ret = reset_msgfifo(camsv_dev);
	if (ret)
		return ret;

	enable_irq(camsv_dev->irq);

	dev_dbg(dev, "%s:enable clock\n", __func__);

	for (i = 0; i < camsv_dev->num_clks; i++) {
		ret = clk_prepare_enable(camsv_dev->clks[i]);
		if (ret) {
			dev_info(dev, "enable failed at clk #%d, ret = %d\n",
				 i, ret);
			i--;
			while (i >= 0)
				clk_disable_unprepare(camsv_dev->clks[i--]);

			return ret;
		}
	}
	sv_reset(camsv_dev);

	return 0;
}

static const struct dev_pm_ops mtk_camsv_pm_ops = {
	SET_RUNTIME_PM_OPS(mtk_camsv_runtime_suspend, mtk_camsv_runtime_resume,
			   NULL)
};

struct platform_driver mtk_cam_sv_driver = {
	.probe   = mtk_camsv_probe,
	.remove  = mtk_camsv_remove,
	.driver  = {
		.name  = "mtk-cam camsv",
		.of_match_table = of_match_ptr(mtk_camsv_of_ids),
		.pm     = &mtk_camsv_pm_ops,
	}
};

