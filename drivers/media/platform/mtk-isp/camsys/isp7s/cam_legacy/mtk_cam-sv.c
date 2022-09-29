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
#include "mtk_cam-feature.h"
#include "mtk_cam-pool.h"
#include "mtk_cam-sv-regs.h"
#include "mtk_cam-sv.h"
#include "mtk_cam-regs.h"
#include "mtk_camera-v4l2-controls.h"
#include "mtk_camera-videodev2.h"

#ifdef CAMSYS_TF_DUMP_7S
#include <dt-bindings/memory/mt6985-larb-port.h>
#include "iommu_debug.h"
#endif

#define MTK_CAMSV_STOP_HW_TIMEOUT			(33 * USEC_PER_MSEC)

static unsigned int debug_sv_fbc;
module_param(debug_sv_fbc, uint, 0644);
MODULE_PARM_DESC(debug_sv_fbc, "debug: sv fbc");

static int debug_cam_sv;
module_param(debug_cam_sv, int, 0644);

#undef dev_dbg
#define dev_dbg(dev, fmt, arg...)		\
	do {					\
		if (debug_cam_sv >= 1)	\
			dev_info(dev, fmt,	\
				## arg);	\
	} while (0)

static const struct of_device_id mtk_camsv_of_ids[] = {
	{.compatible = "mediatek,camsv",},
	{}
};
MODULE_DEVICE_TABLE(of, mtk_camsv_of_ids);

static void mtk_camsv_register_iommu_tf_callback(struct mtk_camsv_device *sv)
{
#ifdef CAMSYS_TF_DUMP_7S
	dev_dbg(sv->dev, "%s : sv->id:%d\n", __func__, sv->id);

	switch (sv->id) {
	case CAMSV_0:
		mtk_iommu_register_fault_callback(M4U_PORT_L14_CAMSV_0_WDMA,
			mtk_camsv_translation_fault_callback, (void *)sv, false);
		break;
	case CAMSV_1:
		mtk_iommu_register_fault_callback(M4U_PORT_L13_CAMSV_1_WDMA,
			mtk_camsv_translation_fault_callback, (void *)sv, false);
		break;
	case CAMSV_2:
		mtk_iommu_register_fault_callback(M4U_PORT_L29_CAMSV_2_WDMA,
			mtk_camsv_translation_fault_callback, (void *)sv, false);
		break;
	case CAMSV_3:
		mtk_iommu_register_fault_callback(M4U_PORT_L29_CAMSV_3_WDMA,
			mtk_camsv_translation_fault_callback, (void *)sv, false);
		break;
	case CAMSV_4:
		mtk_iommu_register_fault_callback(M4U_PORT_L29_CAMSV_4_WDMA,
			mtk_camsv_translation_fault_callback, (void *)sv, false);
		break;
	case CAMSV_5:
		mtk_iommu_register_fault_callback(M4U_PORT_L29_CAMSV_5_WDMA,
			mtk_camsv_translation_fault_callback, (void *)sv, false);
		break;
	}
#endif
};

#ifdef CAMSYS_TF_DUMP_7S
int mtk_camsv_translation_fault_callback(int port, dma_addr_t mva, void *data)
{
	int index;
	struct mtk_camsv_device *sv_dev = (struct mtk_camsv_device *)data;

	dev_info(sv_dev->dev, "tg_sen_mode:0x%x tg_vf_con:0x%x tg_path_cfg:0x%x",
		readl_relaxed(sv_dev->base_inner + REG_CAMSVCENTRAL_SEN_MODE),
		readl_relaxed(sv_dev->base_inner + REG_CAMSVCENTRAL_VF_CON),
		readl_relaxed(sv_dev->base_inner + REG_CAMSVCENTRAL_PATH_CFG));

	for (index = 0; index < MAX_SV_HW_TAGS; index++) {
		dev_info(sv_dev->dev, "tag:%d tg_grab_pxl:0x%x tg_grab_lin:0x%x fmt:0x%x imgo_fbc0: 0x%x imgo_fbc1: 0x%x",
		index,
		readl_relaxed(sv_dev->base_inner + REG_CAMSVCENTRAL_GRAB_PXL_TAG1 +
			index * CAMSVCENTRAL_GRAB_PXL_TAG_SHIFT),
		readl_relaxed(sv_dev->base_inner + REG_CAMSVCENTRAL_GRAB_LIN_TAG1 +
			index * CAMSVCENTRAL_GRAB_LIN_TAG_SHIFT),
		readl_relaxed(sv_dev->base_inner + REG_CAMSVCENTRAL_FORMAT_TAG1 +
			index * CAMSVCENTRAL_FORMAT_TAG_SHIFT),
		readl_relaxed(sv_dev->base_inner + REG_CAMSVCENTRAL_FBC0_TAG1 +
			index * CAMSVCENTRAL_FBC0_TAG_SHIFT),
		readl_relaxed(sv_dev->base_inner + REG_CAMSVCENTRAL_FBC1_TAG1 +
			index * CAMSVCENTRAL_FBC1_TAG_SHIFT));
	}


	for (index = 0; index < MAX_SV_HW_TAGS; index++) {
		dev_info(sv_dev->dev, "tag:%d imgo_stride_img:0x%x imgo_addr_img:0x%x_%x",
			index,
			readl_relaxed(sv_dev->base_inner_dma +
				REG_CAMSVDMATOP_WDMA_BASIC_IMG1 +
				index * CAMSVDMATOP_WDMA_BASIC_IMG_SHIFT),
			readl_relaxed(sv_dev->base_inner_dma +
				REG_CAMSVDMATOP_WDMA_BASE_ADDR_IMG1 +
				index * CAMSVDMATOP_WDMA_BASE_ADDR_IMG_SHIFT),
			readl_relaxed(sv_dev->base_inner_dma +
				REG_CAMSVDMATOP_WDMA_BASE_ADDR_MSB_IMG1 +
				index * CAMSVDMATOP_WDMA_BASE_ADDR_MSB_IMG_SHIFT));
	}
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
	case V4L2_EVENT_EOS:
		return v4l2_event_subscribe(fh, sub, 0, NULL);
	default:
		return -EINVAL;
	}
}

static int mtk_camsv_sd_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct mtk_camsv_pipeline *pipe =
		container_of(sd, struct mtk_camsv_pipeline, subdev);
	struct mtk_camsv *sv = pipe->sv;
	struct mtk_cam_device *cam = dev_get_drvdata(sv->cam_dev);
	struct mtk_cam_ctx *ctx = mtk_cam_find_ctx(cam, &sd->entity);

	if (WARN_ON(!ctx))
		return -EINVAL;

	if (enable) {
		if (ctx->used_sv_num < MAX_SV_PIPES_PER_STREAM)
			ctx->sv_pipe[ctx->used_sv_num++] = pipe;
		else
			dev_dbg(sv->cam_dev, "un-expected used sv number:%d\n", ctx->used_sv_num);
	} else {
		pipe->req_pfmt_update = 0;
		pipe->req_vfmt_update = 0;
	}

	dev_info(sv->cam_dev, "%s:camsv-%d: en %d\n",
		 __func__, pipe->id - MTKCAM_SUBDEV_CAMSV_START, enable);
	return 0;
}

static int mtk_camsv_init_cfg(struct v4l2_subdev *sd,
		struct v4l2_subdev_state *state)
{
	struct v4l2_mbus_framefmt *mf;
	unsigned int i;
	struct mtk_camsv_pipeline *pipe =
		container_of(sd, struct mtk_camsv_pipeline, subdev);
	struct mtk_camsv *sv = pipe->sv;

	for (i = 0; i < sd->entity.num_pads; i++) {
		mf = v4l2_subdev_get_try_format(sd, state, i);
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
					  struct v4l2_subdev_state *state,
					  unsigned int padid, int which)
{
	/* format invalid and return default format */
	if (which == V4L2_SUBDEV_FORMAT_TRY)
		return v4l2_subdev_get_try_format(&pipe->subdev, state, padid);

	if (WARN_ON(padid >= pipe->subdev.entity.num_pads))
		return &pipe->cfg[0].mbus_fmt;

	return &pipe->cfg[padid].mbus_fmt;
}

static int mtk_camsv_call_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *state,
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

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY && !state) {
		dev_dbg(sv->cam_dev, "%s: Required sd(%p), state(%p) for FORMAT_TRY\n",
					__func__, sd, state);
		return -EINVAL;
	}

	if (mtk_camsv_try_fmt(sd, fmt) == MTKCAM_IPI_IMG_FMT_UNKNOWN) {
		mf = get_sv_fmt(pipe, state, fmt->pad, fmt->which);
		fmt->format = *mf;

		dev_info(sv->cam_dev,
			"sd:%s pad:%d set format w/h/code/which %d/%d/0x%x/%d\n",
			sd->name, fmt->pad, mf->width, mf->height, mf->code, fmt->which);
	} else {
		mf = get_sv_fmt(pipe, state, fmt->pad, fmt->which);
		*mf = fmt->format;

		dev_info(sv->cam_dev,
			"sd:%s pad:%d set format w/h/code/which %d/%d/0x%x/%d\n",
			sd->name, fmt->pad, mf->width, mf->height, mf->code, fmt->which);
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

static int mtk_camsv_collect_pfmt(struct mtk_camsv_pipeline *pipe,
				struct v4l2_subdev_format *fmt)
{
	unsigned int pad = fmt->pad;

	pipe->req_pfmt_update |= 1 << pad;
	pipe->req_pad_fmt[pad] = *fmt;

	dev_dbg(pipe->subdev.v4l2_dev->dev,
		"%s:%s:pad(%d), pending s_fmt, w/h/code=%d/%d/0x%x\n",
		__func__, pipe->subdev.name,
		pad, fmt->format.width, fmt->format.height,
		fmt->format.code);

	return 0;
}

static int mtk_camsv_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *state,
			  struct v4l2_subdev_format *fmt)
{
	struct mtk_camsv_pipeline *pipe =
		container_of(sd, struct mtk_camsv_pipeline, subdev);

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY)
		return mtk_camsv_call_set_fmt(sd, state, fmt);

	/* if the pipeline is streaming, pending the change */
	if (!sd->entity.stream_count)
		return mtk_camsv_call_set_fmt(sd, state, fmt);

	mtk_camsv_collect_pfmt(pipe, fmt);

	return 0;
}

static int mtk_camsv_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *state,
			  struct v4l2_subdev_format *fmt)
{
	struct mtk_camsv_pipeline *pipe =
		container_of(sd, struct mtk_camsv_pipeline, subdev);
	struct mtk_camsv *sv = pipe->sv;
	struct v4l2_mbus_framefmt *mf;

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY)
		mf = v4l2_subdev_get_try_format(sd, state, fmt->pad);
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
	.link_validate = mtk_cam_sv_link_validate,
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
	.vidioc_qbuf = mtk_cam_vidioc_qbuf,
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
			.num_planes = 1,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SBGGR8,
			.num_planes = 1,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SBGGR12,
			.num_planes = 1,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SBGGR14,
			.num_planes = 1,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGBRG8,
			.num_planes = 1,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGBRG10,
			.num_planes = 1,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGBRG12,
			.num_planes = 1,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGBRG14,
			.num_planes = 1,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGRBG8,
			.num_planes = 1,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGRBG10,
			.num_planes = 1,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGRBG12,
			.num_planes = 1,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGRBG14,
			.num_planes = 1,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SRGGB8,
			.num_planes = 1,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SRGGB10,
			.num_planes = 1,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SRGGB12,
			.num_planes = 1,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SRGGB14,
			.num_planes = 1,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SBGGR10P,
			.num_planes = 1,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGBRG10P,
			.num_planes = 1,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGRBG10P,
			.num_planes = 1,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SRGGB10P,
			.num_planes = 1,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SBGGR10,
			.num_planes = 1,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SBGGR12,
			.num_planes = 1,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SBGGR14,
			.num_planes = 1,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGBRG10,
			.num_planes = 1,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGBRG12,
			.num_planes = 1,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGBRG14,
			.num_planes = 1,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGRBG10,
			.num_planes = 1,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGRBG12,
			.num_planes = 1,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGRBG14,
			.num_planes = 1,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SRGGB10,
			.num_planes = 1,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SRGGB12,
			.num_planes = 1,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SRGGB14,
			.num_planes = 1,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV12,
			.num_planes = 1,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV21,
			.num_planes = 1,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV12_10,
			.num_planes = 1,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_NV21_10,
			.num_planes = 1,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV12_10P,
			.num_planes = 1,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_NV21_10P,
			.num_planes = 1,
		},
	},
};

#define MTK_CAMSV_TOTAL_CAPTURE_QUEUES 2

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
	{
		.id = MTK_CAMSV_EXT_STREAM_OUT,
		.name = "ext stream",
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

static const char *sv_capture_queue_names[MAX_SV_PIPELINE_NUN][MTK_CAMSV_TOTAL_CAPTURE_QUEUES] = {
	{"mtk-cam camsv-0 main-stream",
		"mtk-cam camsv-0 ext-stream"},
	{"mtk-cam camsv-1 main-stream",
		"mtk-cam camsv-1 ext-stream"},
	{"mtk-cam camsv-2 main-stream",
		"mtk-cam camsv-2 ext-stream"},
	{"mtk-cam camsv-3 main-stream",
		"mtk-cam camsv-3 ext-stream"},
	{"mtk-cam camsv-4 main-stream",
		"mtk-cam camsv-4 ext-stream"},
	{"mtk-cam camsv-5 main-stream",
		"mtk-cam camsv-5 ext-stream"},
	{"mtk-cam camsv-6 main-stream",
		"mtk-cam camsv-6 ext-stream"},
	{"mtk-cam camsv-7 main-stream",
		"mtk-cam camsv-7 ext-stream"},
	{"mtk-cam camsv-8 main-stream",
		"mtk-cam camsv-8 ext-stream"},
	{"mtk-cam camsv-9 main-stream",
		"mtk-cam camsv-9 ext-stream"},
	{"mtk-cam camsv-10 main-stream",
		"mtk-cam camsv-10 ext-stream"},
	{"mtk-cam camsv-11 main-stream",
		"mtk-cam camsv-11 ext-stream"},
	{"mtk-cam camsv-12 main-stream",
		"mtk-cam camsv-12 ext-stream"},
	{"mtk-cam camsv-13 main-stream",
		"mtk-cam camsv-13 ext-stream"},
	{"mtk-cam camsv-14 main-stream",
		"mtk-cam camsv-14 ext-stream"},
	{"mtk-cam camsv-15 main-stream",
		"mtk-cam camsv-15 ext-stream"},
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
	int dma_sw_ctl;
	int ret;

	dev_dbg(dev->dev, "%s camsv_id:%d\n", __func__, dev->id);

	writel(0, dev->base_dma + REG_CAMSVDMATOP_SW_RST_CTL);
	writel(1, dev->base_dma + REG_CAMSVDMATOP_SW_RST_CTL);
	wmb(); /* make sure committed */

	ret = readx_poll_timeout(readl, dev->base_dma + REG_CAMSVDMATOP_SW_RST_CTL,
				 dma_sw_ctl,
				 dma_sw_ctl & 0x2,
				 1 /* delay, us */,
				 100000 /* timeout, us */);
	if (ret < 0) {
		dev_info(dev->dev, "%s: timeout\n", __func__);

		dev_info(dev->dev,
			 "tg_sen_mode: 0x%x, dma_sw_ctl:0x%x, frame_no:0x%x\n",
			 readl(dev->base + REG_CAMSVCENTRAL_SEN_MODE),
			 readl(dev->base_dma + REG_CAMSVDMATOP_SW_RST_CTL),
			 readl(dev->base + REG_CAMSVCENTRAL_FRAME_SEQ_NO)
			);
		mtk_smi_dbg_hang_detect("camsys-camsv");
		goto RESET_FAILURE;
	}

	/* enable dma dcm after dma is idle */
	writel(0, dev->base + REG_CAMSVCENTRAL_DCM_DIS);

	writel(0, dev->base + REG_CAMSVCENTRAL_SW_CTL);
	writel(1, dev->base + REG_CAMSVCENTRAL_SW_CTL);
	writel(0, dev->base_dma + REG_CAMSVDMATOP_SW_RST_CTL);
	writel(0, dev->base + REG_CAMSVCENTRAL_SW_CTL);
	wmb(); /* make sure committed */

RESET_FAILURE:
	return;
}

unsigned int mtk_cam_sv_format_sel(unsigned int pixel_fmt)
{
	union CAMSVCENTRAL_FORMAT_TAG1 fmt;

	fmt.Raw = 0;
	fmt.Bits.DATA_SWAP_TAG1 = TG_SW_UYVY;

	switch (pixel_fmt) {
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SGBRG8:
	case V4L2_PIX_FMT_SGRBG8:
	case V4L2_PIX_FMT_SRGGB8:
		fmt.Bits.FMT_TAG1 = SV_TG_FMT_RAW8;
		break;
	case V4L2_PIX_FMT_MTISP_SBGGR10:
	case V4L2_PIX_FMT_MTISP_SGBRG10:
	case V4L2_PIX_FMT_MTISP_SGRBG10:
	case V4L2_PIX_FMT_MTISP_SRGGB10:
	case V4L2_PIX_FMT_SBGGR10P:
	case V4L2_PIX_FMT_SGBRG10P:
	case V4L2_PIX_FMT_SGRBG10P:
	case V4L2_PIX_FMT_SRGGB10P:
	case V4L2_PIX_FMT_SBGGR10:
	case V4L2_PIX_FMT_SGBRG10:
	case V4L2_PIX_FMT_SGRBG10:
	case V4L2_PIX_FMT_SRGGB10:
		fmt.Bits.FMT_TAG1 = SV_TG_FMT_RAW10;
		break;
	case V4L2_PIX_FMT_MTISP_SBGGR12:
	case V4L2_PIX_FMT_MTISP_SGBRG12:
	case V4L2_PIX_FMT_MTISP_SGRBG12:
	case V4L2_PIX_FMT_MTISP_SRGGB12:
	case V4L2_PIX_FMT_SBGGR12:
	case V4L2_PIX_FMT_SGBRG12:
	case V4L2_PIX_FMT_SGRBG12:
	case V4L2_PIX_FMT_SRGGB12:
		fmt.Bits.FMT_TAG1 = SV_TG_FMT_RAW12;
		break;
	case V4L2_PIX_FMT_MTISP_SBGGR14:
	case V4L2_PIX_FMT_MTISP_SGBRG14:
	case V4L2_PIX_FMT_MTISP_SGRBG14:
	case V4L2_PIX_FMT_MTISP_SRGGB14:
	case V4L2_PIX_FMT_SBGGR14:
	case V4L2_PIX_FMT_SGBRG14:
	case V4L2_PIX_FMT_SGRBG14:
	case V4L2_PIX_FMT_SRGGB14:
		fmt.Bits.FMT_TAG1 = SV_TG_FMT_RAW14;
		break;
	case V4L2_PIX_FMT_YUYV:
		fmt.Bits.DATA_SWAP_TAG1 = TG_SW_YUYV;
		fmt.Bits.FMT_TAG1 = SV_TG_FMT_YUV422;
		break;
	case V4L2_PIX_FMT_YVYU:
		fmt.Bits.DATA_SWAP_TAG1 = TG_SW_YVYU;
		fmt.Bits.FMT_TAG1 = SV_TG_FMT_YUV422;
		break;
	case V4L2_PIX_FMT_UYVY:
		fmt.Bits.DATA_SWAP_TAG1 = TG_SW_UYVY;
		fmt.Bits.FMT_TAG1 = SV_TG_FMT_YUV422;
		break;
	case V4L2_PIX_FMT_VYUY:
		fmt.Bits.DATA_SWAP_TAG1 = TG_SW_VYUY;
		fmt.Bits.FMT_TAG1 = SV_TG_FMT_YUV422;
		break;
	default:
		break;
	}

	return fmt.Raw;
}

unsigned int mtk_cam_sv_pak_sel(unsigned int pixel_fmt,
	unsigned int pixel_mode)
{
	unsigned int is_unpak = 0;

	switch (pixel_fmt) {
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SGBRG8:
	case V4L2_PIX_FMT_SGRBG8:
	case V4L2_PIX_FMT_SRGGB8:
	case V4L2_PIX_FMT_MTISP_SBGGR10:
	case V4L2_PIX_FMT_MTISP_SGBRG10:
	case V4L2_PIX_FMT_MTISP_SGRBG10:
	case V4L2_PIX_FMT_MTISP_SRGGB10:
	case V4L2_PIX_FMT_SBGGR10P:
	case V4L2_PIX_FMT_SGBRG10P:
	case V4L2_PIX_FMT_SGRBG10P:
	case V4L2_PIX_FMT_SRGGB10P:
	case V4L2_PIX_FMT_MTISP_SBGGR12:
	case V4L2_PIX_FMT_MTISP_SGBRG12:
	case V4L2_PIX_FMT_MTISP_SGRBG12:
	case V4L2_PIX_FMT_MTISP_SRGGB12:
		is_unpak = 0;
		break;
	/* for unpak fmt */
	case V4L2_PIX_FMT_SBGGR10:
	case V4L2_PIX_FMT_SGBRG10:
	case V4L2_PIX_FMT_SGRBG10:
	case V4L2_PIX_FMT_SRGGB10:
	case V4L2_PIX_FMT_SBGGR12:
	case V4L2_PIX_FMT_SGBRG12:
	case V4L2_PIX_FMT_SGRBG12:
	case V4L2_PIX_FMT_SRGGB12:
	case V4L2_PIX_FMT_SBGGR14:
	case V4L2_PIX_FMT_SGBRG14:
	case V4L2_PIX_FMT_SGRBG14:
	case V4L2_PIX_FMT_SRGGB14:
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_VYUY:
		is_unpak = 1;
		break;
	default:
		break;
	}

	return is_unpak;
}

unsigned int mtk_cam_sv_xsize_cal(struct mtkcam_ipi_input_param *cfg_in_param)
{

	unsigned int size = 0;
	unsigned int divisor = 0;

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
	/* for unpak fmt */
	case V4L2_PIX_FMT_SBGGR10:
	case V4L2_PIX_FMT_SGBRG10:
	case V4L2_PIX_FMT_SGRBG10:
	case V4L2_PIX_FMT_SRGGB10:
	case V4L2_PIX_FMT_SBGGR12:
	case V4L2_PIX_FMT_SGBRG12:
	case V4L2_PIX_FMT_SGRBG12:
	case V4L2_PIX_FMT_SRGGB12:
	case V4L2_PIX_FMT_SBGGR14:
	case V4L2_PIX_FMT_SGBRG14:
	case V4L2_PIX_FMT_SGRBG14:
	case V4L2_PIX_FMT_SRGGB14:
		size = (cfg_in_param->in_crop.s.w * 16) / 8;
		break;
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_VYUY:
		size = cfg_in_param->in_crop.s.w;
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

int mtk_cam_sv_central_common_config(struct mtk_camsv_device *dev,
	unsigned int sub_ratio)
{
	int ret = 0;
	unsigned int done_int_en = 0;
	int i;

	/* timestamp */
	CAMSV_WRITE_BITS(dev->base + REG_CAMSVCENTRAL_TIME_STAMP_CTL,
		CAMSVCENTRAL_TIME_STAMP_CTL, TIME_STP_EN, 1);

	/* trig mode */
	CAMSV_WRITE_BITS(dev->base + REG_CAMSVCENTRAL_VF_CON,
		CAMSVCENTRAL_VF_CON, SINGLE_MODE, 0);

	/* disable vf db */
	CAMSV_WRITE_BITS(dev->base + REG_CAMSVCENTRAL_PATH_CFG,
		CAMSVCENTRAL_PATH_CFG, SYNC_VF_EN_DB_LOAD_DIS, 0);

	/* reset */
	sv_reset(dev);

	/* fun en */
	CAMSV_WRITE_BITS(dev->base + REG_CAMSVCENTRAL_MODULE_DB,
		CAMSVCENTRAL_MODULE_DB, CAM_DB_EN, 0);

	/* central sub en */
	if (sub_ratio > 0)
		CAMSV_WRITE_BITS(dev->base + REG_CAMSVCENTRAL_SEN_MODE,
			CAMSVCENTRAL_SEN_MODE, CAM_SUB_EN, 1);
	else
		CAMSV_WRITE_BITS(dev->base + REG_CAMSVCENTRAL_SEN_MODE,
			CAMSVCENTRAL_SEN_MODE, CAM_SUB_EN, 0);

	/* sub p1 done */
	if (sub_ratio > 0) {
		CAMSV_WRITE_BITS(dev->base + REG_CAMSVCENTRAL_SEN_MODE,
			CAMSVCENTRAL_SEN_MODE, DOWN_SAMPLE_EN, 1);
		CAMSV_WRITE_BITS(dev->base + REG_CAMSVCENTRAL_SEN_MODE,
			CAMSVCENTRAL_SEN_MODE, SOF_SUB_EN, 1);
		CAMSV_WRITE_BITS(dev->base + REG_CAMSVCENTRAL_SEN_MODE,
			CAMSVCENTRAL_SEN_MODE, VS_SUB_EN, 1);
		CAMSV_WRITE_BITS(dev->base + REG_CAMSVCENTRAL_SUB_PERIOD,
			CAMSVCENTRAL_SUB_PERIOD, DOWN_SAMPLE_PERIOD,
			sub_ratio);
		CAMSV_WRITE_BITS(dev->base + REG_CAMSVCENTRAL_SUB_PERIOD,
			CAMSVCENTRAL_SUB_PERIOD, SOF_PERIOD,
			sub_ratio);
		CAMSV_WRITE_BITS(dev->base + REG_CAMSVCENTRAL_SUB_PERIOD,
			CAMSVCENTRAL_SUB_PERIOD, VS_PERIOD,
			sub_ratio);
	} else {
		CAMSV_WRITE_BITS(dev->base + REG_CAMSVCENTRAL_SEN_MODE,
			CAMSVCENTRAL_SEN_MODE, DOWN_SAMPLE_EN, 0);
		CAMSV_WRITE_BITS(dev->base + REG_CAMSVCENTRAL_SEN_MODE,
			CAMSVCENTRAL_SEN_MODE, SOF_SUB_EN, 0);
		CAMSV_WRITE_BITS(dev->base + REG_CAMSVCENTRAL_SEN_MODE,
			CAMSVCENTRAL_SEN_MODE, VS_SUB_EN, 0);
		CAMSV_WRITE_BITS(dev->base + REG_CAMSVCENTRAL_SUB_PERIOD,
			CAMSVCENTRAL_SUB_PERIOD, DOWN_SAMPLE_PERIOD,
			sub_ratio);
		CAMSV_WRITE_BITS(dev->base + REG_CAMSVCENTRAL_SUB_PERIOD,
			CAMSVCENTRAL_SUB_PERIOD, SOF_PERIOD,
			sub_ratio);
		CAMSV_WRITE_BITS(dev->base + REG_CAMSVCENTRAL_SUB_PERIOD,
			CAMSVCENTRAL_SUB_PERIOD, VS_PERIOD,
			sub_ratio);
	}

	/* set groups and done_int_en */
	for (i = 0; i < MAX_SV_HW_GROUPS; i++) {
		if (dev->cfg_group_info[i] != 0) {
			CAMSV_WRITE_REG(dev->base + REG_CAMSVCENTRAL_GROUP_TAG0 +
				REG_CAMSVCENTRAL_GROUP_TAG_SHIFT * i,
				dev->cfg_group_info[i]);

			done_int_en |= (1 << i);
			done_int_en |= (1 << (i + 16));
		}
	}
	CAMSV_WRITE_REG(dev->base + REG_CAMSVCENTRAL_DONE_STATUS_EN,
		done_int_en);

	CAMSV_WRITE_BITS(dev->base + REG_CAMSVCENTRAL_ERR_CTL,
		CAMSVCENTRAL_ERR_CTL, GRAB_ERR_EN, 1);

	return ret;
}

int mtk_cam_sv_central_pertag_config(struct mtk_camsv_device *dev,
	unsigned int tag_idx)
{
	int ret = 0;
	unsigned int pxl, lin;
	unsigned int error_int_en = 0, sof_int_en = 0;
	struct mtkcam_ipi_input_param *cfg_in_param;

	cfg_in_param = &dev->tag_info[tag_idx].cfg_in_param;

	/* camsv todo: check this logic on stagger case, and add scq stagger mode s*/
	if (dev->tag_info[tag_idx].tag_order == MTKCAM_IPI_ORDER_FIRST_TAG)
		CAMSV_WRITE_REG(dev->base + REG_CAMSVCENTRAL_FIRST_TAG, (1 << tag_idx));
	else if (dev->tag_info[tag_idx].tag_order == MTKCAM_IPI_ORDER_LAST_TAG)
		CAMSV_WRITE_REG(dev->base + REG_CAMSVCENTRAL_LAST_TAG, (1 << tag_idx));

	/* grab size */
	pxl = ((cfg_in_param->in_crop.s.w + cfg_in_param->in_crop.p.x) << 16) |
			cfg_in_param->in_crop.p.x;
	lin = ((cfg_in_param->in_crop.s.h + cfg_in_param->in_crop.p.y) << 16) |
			cfg_in_param->in_crop.p.y;
	CAMSV_WRITE_REG(dev->base + REG_CAMSVCENTRAL_GRAB_PXL_TAG1 +
		CAMSVCENTRAL_GRAB_PXL_TAG_SHIFT * tag_idx, pxl);
	CAMSV_WRITE_REG(dev->base + REG_CAMSVCENTRAL_GRAB_LIN_TAG1 +
		CAMSVCENTRAL_GRAB_LIN_TAG_SHIFT * tag_idx, lin);

	dev_info(dev->dev, "pxl mode:%d sub:%d fmt:0x%x x:%d y:%d w:%d h:%d\n",
		cfg_in_param->pixel_mode, cfg_in_param->subsample,
		cfg_in_param->fmt, cfg_in_param->in_crop.p.x,
		cfg_in_param->in_crop.p.y, cfg_in_param->in_crop.s.w,
		cfg_in_param->in_crop.s.h);

	/* fmt sel */
	CAMSV_WRITE_BITS(dev->base + REG_CAMSVCENTRAL_FORMAT_TAG1
		+ CAMSVCENTRAL_FORMAT_TAG_SHIFT * tag_idx,
		CAMSVCENTRAL_FORMAT_TAG1, FMT_TAG1,
		mtk_cam_sv_format_sel(cfg_in_param->fmt));

	/* int en */
	error_int_en = CAMSV_READ_REG(dev->base + REG_CAMSVCENTRAL_ERR_STATUS_EN);
	error_int_en |= (1 << (8 + tag_idx * 3));
	error_int_en |= (1 << (9 + tag_idx * 3));
	CAMSV_WRITE_REG(dev->base + REG_CAMSVCENTRAL_ERR_STATUS_EN, error_int_en);

	sof_int_en = CAMSV_READ_REG(dev->base + REG_CAMSVCENTRAL_SOF_STATUS_EN);
	sof_int_en |= (1 << (2 + tag_idx * 4));
	CAMSV_WRITE_REG(dev->base + REG_CAMSVCENTRAL_SOF_STATUS_EN, sof_int_en);

	/* pak */
	CAMSV_WRITE_BITS(dev->base + REG_CAMSVCENTRAL_FORMAT_TAG1
		+ CAMSVCENTRAL_FORMAT_TAG_SHIFT * tag_idx,
		CAMSVCENTRAL_FORMAT_TAG1, UNPACK_MODE_TAG1,
		mtk_cam_sv_pak_sel(cfg_in_param->fmt, cfg_in_param->pixel_mode));

	/* ufe disable */
	CAMSV_WRITE_BITS(dev->base + REG_CAMSVCENTRAL_FORMAT_TAG1
		+ CAMSVCENTRAL_FORMAT_TAG_SHIFT * tag_idx,
		CAMSVCENTRAL_FORMAT_TAG1, UFE_EN_TAG1, 0);

	return ret;
}

int mtk_cam_sv_dmao_common_config(struct mtk_camsv_device *dev)
{
	int ret = 0;

	switch (dev->id) {
	case CAMSV_0:
		/* imgo */
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON3_IMG, 0x880406AE);
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON2_IMG, 0x05580402);
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON1_IMG, 0x02AC0156);
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON4_IMG, 0x81560000);
		break;
	case CAMSV_1:
		/* imgo */
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON3_IMG, 0x856A0483);
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON2_IMG, 0x039C02B5);
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON1_IMG, 0x01CE00E7);
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON4_IMG, 0x80E70000);
		break;
	case CAMSV_2:
		/* imgo */
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON3_IMG, 0x84CE0401);
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON2_IMG, 0x03340267);
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON1_IMG, 0x019A00CD);
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON4_IMG, 0x80CD0000);
		break;
	case CAMSV_3:
		/* imgo */
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON3_IMG, 0x83000280);
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON2_IMG, 0x02000180);
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON1_IMG, 0x01000080);
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON4_IMG, 0x80800000);
		break;
	case CAMSV_4:
		/* imgo */
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON3_IMG, 0x80D800B4);
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON2_IMG, 0x0090006C);
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON1_IMG, 0x00480024);
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON4_IMG, 0x80240000);
		break;
	case CAMSV_5:
		/* imgo */
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON3_IMG, 0x80D800B4);
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON2_IMG, 0x0090006C);
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON1_IMG, 0x00480024);
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON4_IMG, 0x80240000);
		break;
	}

	/* cqi */
	CAMSV_WRITE_REG(dev->base_scq + REG_CAMSV_M1_CQI_ORIRDMA_CON0, 0x10000040);
	CAMSV_WRITE_REG(dev->base_scq + REG_CAMSV_M1_CQI_ORIRDMA_CON1, 0x000D0007);
	CAMSV_WRITE_REG(dev->base_scq + REG_CAMSV_M1_CQI_ORIRDMA_CON2, 0x001A0014);
	CAMSV_WRITE_REG(dev->base_scq + REG_CAMSV_M1_CQI_ORIRDMA_CON3, 0x00270020);
	CAMSV_WRITE_REG(dev->base_scq + REG_CAMSV_M1_CQI_ORIRDMA_CON4, 0x00070000);

	CAMSV_WRITE_REG(dev->base_scq + REG_CAMSV_M2_CQI_ORIRDMA_CON0, 0x10000040);
	CAMSV_WRITE_REG(dev->base_scq + REG_CAMSV_M2_CQI_ORIRDMA_CON1, 0x000D0007);
	CAMSV_WRITE_REG(dev->base_scq + REG_CAMSV_M2_CQI_ORIRDMA_CON2, 0x001A0014);
	CAMSV_WRITE_REG(dev->base_scq + REG_CAMSV_M2_CQI_ORIRDMA_CON3, 0x00270020);
	CAMSV_WRITE_REG(dev->base_scq + REG_CAMSV_M2_CQI_ORIRDMA_CON4, 0x00070000);

	return ret;
}

int mtk_cam_sv_dmao_pertag_config(
	struct mtk_camsv_device *dev, unsigned int tag_idx)
{
	int ret = 0;
	struct mtkcam_ipi_input_param *cfg_in_param;

	cfg_in_param = &dev->tag_info[tag_idx].cfg_in_param;

	/* imgo dma setting */
	CAMSV_WRITE_BITS(dev->base_dma + REG_CAMSVDMATOP_WDMA_BASIC_IMG1
		+ CAMSVDMATOP_WDMA_BASIC_IMG_SHIFT * tag_idx,
		CAMSVDMATOP_WDMA_BASIC_IMG1, STRIDE_IMG1,
		dev->tag_info[tag_idx].stride);

	dev_info(dev->dev, "xsize:%d ysize:%d stride:%d\n",
		mtk_cam_sv_xsize_cal(cfg_in_param),
		cfg_in_param->in_crop.s.h,
		dev->tag_info[tag_idx].stride);

	return ret;
}

int mtk_cam_sv_fbc_pertag_config(
	struct mtk_camsv_device *dev, unsigned int tag_idx)
{
	int ret = 0;

	CAMSV_WRITE_REG(dev->base + REG_CAMSVCENTRAL_FBC0_TAG1 +
		CAMSVCENTRAL_FBC0_TAG_SHIFT * tag_idx, 0);

	return ret;
}

int mtk_cam_sv_toggle_tg_db(struct mtk_camsv_device *dev)
{
	int val, val2;

	val = CAMSV_READ_REG(dev->base_inner + REG_CAMSVCENTRAL_PATH_CFG);
	CAMSV_WRITE_BITS(dev->base + REG_CAMSVCENTRAL_PATH_CFG,
		CAMSVCENTRAL_PATH_CFG, SYNC_VF_EN_DB_LOAD_DIS, 1);

	CAMSV_WRITE_BITS(dev->base + REG_CAMSVCENTRAL_PATH_CFG,
		CAMSVCENTRAL_PATH_CFG, SYNC_VF_EN_DB_LOAD_DIS, 0);
	val2 = CAMSV_READ_REG(dev->base_inner + REG_CAMSVCENTRAL_PATH_CFG);
	dev_info(dev->dev, "%s 0x%x->0x%x\n", __func__, val, val2);
	return 0;
}

int mtk_cam_sv_toggle_db(struct mtk_camsv_device *dev)
{
	int val, val2;

	val = CAMSV_READ_REG(dev->base_inner + REG_CAMSVCENTRAL_MODULE_DB);
	CAMSV_WRITE_BITS(dev->base + REG_CAMSVCENTRAL_MODULE_DB,
		CAMSVCENTRAL_MODULE_DB, CAM_DB_EN, 0);

	CAMSV_WRITE_BITS(dev->base + REG_CAMSVCENTRAL_MODULE_DB,
		CAMSVCENTRAL_MODULE_DB, CAM_DB_EN, 1);
	val2 = CAMSV_READ_REG(dev->base_inner + REG_CAMSVCENTRAL_MODULE_DB);
	dev_info(dev->dev, "%s 0x%x->0x%x\n", __func__, val, val2);

	return 0;
}

int mtk_cam_sv_central_common_enable(struct mtk_camsv_device *dev)
{
	int ret = 0;

	mtk_cam_sv_toggle_db(dev);
	mtk_cam_sv_toggle_tg_db(dev);

	CAMSV_WRITE_BITS(dev->base + REG_CAMSVCENTRAL_SEN_MODE,
		CAMSVCENTRAL_SEN_MODE, CMOS_EN, 1);
	CAMSV_WRITE_BITS(dev->base + REG_CAMSVCENTRAL_VF_CON,
		CAMSVCENTRAL_VF_CON, VFDATA_EN, 1);

	dev_info(dev->dev, "%s sen_mode:0x%x vf_con:0x%x\n",
		__func__,
		CAMSV_READ_REG(dev->base + REG_CAMSVCENTRAL_SEN_MODE),
		CAMSV_READ_REG(dev->base + REG_CAMSVCENTRAL_VF_CON));

	return ret;
}

int mtk_cam_sv_print_fbc_status(struct mtk_camsv_device *dev)
{
	int ret = 0, i;

	if (debug_sv_fbc) {
		for (i = SVTAG_START; i < SVTAG_END; i++) {
			dev_info(dev->dev, "%s tag_idx(%d) FBC_IMGO_CTRL1/2:0x%x/0x%x\n",
				__func__, i,
				CAMSV_READ_REG(dev->base + REG_CAMSVCENTRAL_FBC0_TAG1 +
					CAMSVCENTRAL_FBC0_TAG_SHIFT * i),
				CAMSV_READ_REG(dev->base + REG_CAMSVCENTRAL_FBC1_TAG1 +
					CAMSVCENTRAL_FBC0_TAG_SHIFT * i));
		}
	}

	return ret;
}

int mtk_cam_sv_fbc_pertag_enable(
	struct mtk_camsv_device *dev, unsigned int tag_idx)
{
	int ret = 0;
	struct mtkcam_ipi_input_param *cfg_in_param;

	cfg_in_param = &dev->tag_info[tag_idx].cfg_in_param;

	CAMSV_WRITE_BITS(dev->base +
		REG_CAMSVCENTRAL_FBC0_TAG1 + CAMSVCENTRAL_FBC0_TAG_SHIFT * tag_idx,
		CAMSVCENTRAL_FBC0_TAG1, FBC_SUB_EN_TAG1,
		(cfg_in_param->subsample) ? 1 : 0);
	CAMSV_WRITE_BITS(dev->base +
		REG_CAMSVCENTRAL_FBC0_TAG1 + CAMSVCENTRAL_FBC0_TAG_SHIFT * tag_idx,
		CAMSVCENTRAL_FBC0_TAG1, FBC_EN_TAG1, 1);

	return ret;
}

int mtk_cam_sv_central_common_disable(struct mtk_camsv_device *dev)
{
	int ret = 0;

	/* disable dma dcm before do dma reset */
	writel(1, dev->base + REG_CAMSVCENTRAL_DCM_DIS);

	/* bypass tg_mode function before vf off */
	CAMSV_WRITE_BITS(dev->base + REG_CAMSVCENTRAL_SEN_MODE,
		CAMSVCENTRAL_SEN_MODE, TG_MODE_OFF, 1);

	CAMSV_WRITE_BITS(dev->base + REG_CAMSVCENTRAL_VF_CON,
		CAMSVCENTRAL_VF_CON, VFDATA_EN, 0);

	mtk_cam_sv_toggle_tg_db(dev);

	CAMSV_WRITE_REG(dev->base + REG_CAMSVCENTRAL_DONE_STATUS_EN, 0);
	CAMSV_WRITE_REG(dev->base + REG_CAMSVCENTRAL_ERR_STATUS_EN, 0);
	CAMSV_WRITE_REG(dev->base + REG_CAMSVCENTRAL_SOF_STATUS_EN, 0);

	CAMSV_WRITE_BITS(dev->base + REG_CAMSVCENTRAL_SEN_MODE,
		CAMSVCENTRAL_SEN_MODE, CMOS_EN, 0);

	sv_reset(dev);
	CAMSV_WRITE_REG(dev->base + REG_CAMSVCENTRAL_DMA_EN_IMG, 0);
	mtk_cam_sv_toggle_db(dev);

	return ret;
}

int mtk_cam_sv_fbc_disable(struct mtk_camsv_device *dev, unsigned int tag_idx)
{
	int ret = 0;

	CAMSV_WRITE_REG(dev->base + REG_CAMSVCENTRAL_FBC0_TAG1 +
		CAMSVCENTRAL_FBC0_TAG_SHIFT * tag_idx, 0);

	return ret;
}

int mtk_cam_sv_enquehwbuf(
	struct mtk_camsv_device *dev,
	dma_addr_t ba,
	unsigned int seq_no, unsigned int tag_idx)
{
	int ret = 0;

	CAMSV_WRITE_REG(dev->base + REG_CAMSVCENTRAL_FRAME_SEQ_NO, seq_no);
	CAMSV_WRITE_REG(dev->base_dma +
		REG_CAMSVDMATOP_WDMA_BASE_ADDR_IMG1 +
		(tag_idx * REG_CAMSVDMATOP_WDMA_ADDR_SHIFT), ba & 0xFFFFFFFF);
	CAMSV_WRITE_REG(dev->base_dma +
		REG_CAMSVDMATOP_WDMA_BASE_ADDR_MSB_IMG1 +
		(tag_idx * REG_CAMSVDMATOP_WDMA_ADDR_SHIFT), (ba >> 32) & 0xF);
	CAMSV_WRITE_BITS(dev->base + REG_CAMSVCENTRAL_FBC0_TAG1 +
		CAMSVCENTRAL_FBC0_TAG_SHIFT * tag_idx,
		CAMSVCENTRAL_FBC0_TAG1, RCNT_INC_TAG1, 1);

	dev_info(dev->dev, "[%s] iova:0x%x, seq:%d\n", __func__, ba, seq_no);

	return ret;
}

int mtk_cam_sv_dev_pertag_write_rcnt(struct mtk_camsv_device *camsv_dev,
	unsigned int tag_idx)
{
	int ret = 0;

	CAMSV_WRITE_BITS(camsv_dev->base + REG_CAMSVCENTRAL_FBC0_TAG1 +
		CAMSVCENTRAL_FBC0_TAG_SHIFT * tag_idx,
		CAMSVCENTRAL_FBC0_TAG1, RCNT_INC_TAG1, 1);

	return ret;
}
void mtk_cam_sv_vf_reset(struct mtk_cam_ctx *ctx,
	struct mtk_camsv_device *dev)
{
	if (CAMSV_READ_BITS(dev->base + REG_CAMSVCENTRAL_VF_CON,
			CAMSVCENTRAL_VF_CON, VFDATA_EN)) {
		CAMSV_WRITE_BITS(dev->base + REG_CAMSVCENTRAL_VF_CON,
			CAMSVCENTRAL_VF_CON, VFDATA_EN, 0);
		mtk_cam_sv_toggle_tg_db(dev);
		dev_info(dev->dev, "preisp sv_vf_reset vf_en off");
		CAMSV_WRITE_BITS(dev->base + REG_CAMSVCENTRAL_VF_CON,
			CAMSVCENTRAL_VF_CON, VFDATA_EN, 1);
		mtk_cam_sv_toggle_tg_db(dev);
		dev_info(dev->dev, "preisp sv_vf_reset vf_en on");
	}
	dev_info(dev->dev, "preisp sv_vf_reset");
}

bool mtk_cam_sv_is_zero_fbc_cnt(struct mtk_cam_ctx *ctx,
	unsigned int tag_idx)
{
	bool result = false;
	struct mtk_camsv_device *camsv_dev;

	camsv_dev = mtk_cam_get_used_sv_dev(ctx);
	if (!camsv_dev) {
		dev_info(ctx->cam->dev, "[%s] camsv_dev is null",
		__func__);
		return result;
	}

	if (CAMSV_READ_BITS(camsv_dev->base +
			REG_CAMSVCENTRAL_FBC1_TAG1 + CAMSVCENTRAL_FBC1_TAG_SHIFT * tag_idx,
			CAMSVCENTRAL_FBC1_TAG1, FBC_CNT_TAG1) == 0)
		result = true;

	return result;
}

void mtk_cam_sv_check_fbc_cnt(struct mtk_camsv_device *camsv_dev,
	unsigned int tag_idx)
{
	unsigned int fbc_cnt = 0;

	fbc_cnt = CAMSV_READ_BITS(camsv_dev->base +
		REG_CAMSVCENTRAL_FBC1_TAG1 + CAMSVCENTRAL_FBC1_TAG_SHIFT * tag_idx,
		CAMSVCENTRAL_FBC1_TAG1, FBC_CNT_TAG1);

	while (fbc_cnt < 2) {
		mtk_cam_sv_dev_pertag_write_rcnt(camsv_dev, tag_idx);
		fbc_cnt++;
	}
}

int mtk_cam_sv_frame_no_inner(struct mtk_camsv_device *dev)
{
	return readl_relaxed(dev->base_inner + REG_CAMSVCENTRAL_FRAME_SEQ_NO);
}

int mtk_cam_sv_apply_all_buffers(struct mtk_cam_ctx *ctx)
{
	struct mtk_camsv_working_buf_entry *buf_entry;
	int i;

	if (ctx->sv_dev) {
		spin_lock(&ctx->sv_composed_buffer_list.lock);
		if (list_empty(&ctx->sv_composed_buffer_list.list)) {
			spin_unlock(&ctx->sv_composed_buffer_list.lock);
			return 0;
		}

		buf_entry = list_first_entry(&ctx->sv_composed_buffer_list.list,
							struct mtk_camsv_working_buf_entry,
							list_entry);
		list_del(&buf_entry->list_entry);
		ctx->sv_composed_buffer_list.cnt--;
		spin_unlock(&ctx->sv_composed_buffer_list.lock);

		spin_lock(&ctx->sv_processing_buffer_list.lock);
		list_add_tail(&buf_entry->list_entry,
					&ctx->sv_processing_buffer_list.list);
		ctx->sv_processing_buffer_list.cnt++;
		spin_unlock(&ctx->sv_processing_buffer_list.lock);

		apply_camsv_cq(ctx->sv_dev,
				buf_entry->buffer.iova,
				buf_entry->sv_cq_desc_size,
				buf_entry->sv_cq_desc_offset,
				0);
		if (watchdog_scenario(ctx)) {
			for (i = 0; i < ctx->used_sv_num; i++) {
				if (buf_entry->s_data->req->pipe_used &
					(1 << ctx->sv_pipe[i]->id))
					mtk_ctx_watchdog_start(ctx, 4, ctx->sv_pipe[i]->id);
				else
					mtk_ctx_watchdog_stop(ctx, ctx->sv_pipe[i]->id);
			}
		}
	}

	return 1;
}

void apply_camsv_cq(struct mtk_camsv_device *dev,
	dma_addr_t cq_addr, unsigned int cq_size,
	unsigned int cq_offset, int initial)
{
#define CQ_VADDR_MASK 0xffffffff
	u32 cq_addr_lsb = (cq_addr + cq_offset) & CQ_VADDR_MASK;
	u32 cq_addr_msb = ((cq_addr + cq_offset) >> 32);

	if (cq_size == 0)
		return;

	CAMSV_WRITE_REG(dev->base_scq  + REG_CAMSVCQ_CQ_SUB_THR0_DESC_SIZE_2,
		cq_size);
	CAMSV_WRITE_REG(dev->base_scq  + REG_CAMSVCQ_CQ_SUB_THR0_BASEADDR_2_MSB,
		cq_addr_msb);
	CAMSV_WRITE_REG(dev->base_scq  + REG_CAMSVCQ_CQ_SUB_THR0_BASEADDR_2,
		cq_addr_lsb);
	CAMSV_WRITE_BITS(dev->base_scq + REG_CAMSVCQTOP_THR_START,
		CAMSVCQTOP_THR_START, CAMSVCQTOP_CSR_CQ_THR0_START, 1);

	if (initial)
		dev_info(dev->dev, "apply 1st camsv scq: addr_msb:0x%x addr_lsb:0x%x size:%d cq_en(0x%x))",
			cq_addr_msb, cq_addr_lsb, cq_size,
			readl_relaxed(dev->base_scq + REG_CAMSVCQTOP_THR_START));
	else
		dev_dbg(dev->dev, "apply camsv scq: addr_msb:0x%x addr_lsb:0x%x size:%d",
			cq_addr_msb, cq_addr_lsb, cq_size);
}

int mtk_cam_sv_update_feature(struct mtk_cam_video_device *node)
{
	struct mtk_cam_device *cam = video_get_drvdata(&node->vdev);
	struct mtk_camsv_pipeline *sv_pipeline;

	sv_pipeline = mtk_cam_dev_get_sv_pipeline(cam, node->uid.pipe_id);
	if (sv_pipeline) {
		if (node->desc.id == MTK_CAMSV_EXT_STREAM_OUT)
			sv_pipeline->feature_pending |= DISPLAY_IC;
	}

	return 0;
}

int mtk_cam_sv_update_image_size(struct mtk_cam_video_device *node,
	struct v4l2_format *f)
{
	if (node->desc.dma_port == MTKCAM_IPI_CAMSV_MAIN_OUT &&
		node->desc.id == MTK_CAMSV_MAIN_STREAM_OUT)
		f->fmt.pix_mp.plane_fmt[0].sizeimage +=
			(GET_PLAT_V4L2(meta_sv_ext_size) + 16);

	return 0;
}

bool mtk_cam_is_display_ic(struct mtk_cam_ctx *ctx)
{
	if (!ctx->used_sv_num)
		return false;

	return (ctx->sv_pipe[0]->feature_pending & DISPLAY_IC) != 0;
}

unsigned int mtk_cam_get_sv_tag_index(struct mtk_cam_ctx *ctx,
		unsigned int pipe_id)
{
	int i;

	for (i = SVTAG_START; i < SVTAG_END; i++) {
		if (ctx->sv_dev->enabled_tags & (1 << i)) {
			struct mtk_camsv_tag_info tag_info =
				ctx->sv_dev->tag_info[i];
			if (tag_info.sv_pipe && (tag_info.sv_pipe->id == pipe_id))
				return i;
		}
	}

	dev_info(ctx->cam->dev, "[%s] tag is not found by pipe_id(%d)",
		__func__, pipe_id);
	return 0;
}

int mtk_cam_sv_pipeline_config(struct mtk_camsv_device *camsv_dev)
{
	camsv_dev->sof_count = 0;

	mtk_camsv_register_iommu_tf_callback(camsv_dev);

	mtk_cam_sv_dmao_common_config(camsv_dev);
	mtk_cam_sv_cq_config(camsv_dev);
	mtk_cam_sv_cq_enable(camsv_dev);

	return 0;
}

int mtk_cam_sv_cq_config(struct mtk_camsv_device *camsv_dev)
{
	/* camsv todo: db en */
	CAMSV_WRITE_BITS(camsv_dev->base_scq + REG_CAMSVCQ_CQ_EN,
		CAMSVCQ_CQ_EN, CAMSVCQ_CQ_DB_EN, 0);
	/* always enable stagger mode for multiple vsync(s) */
	CAMSV_WRITE_BITS(camsv_dev->base_scq + REG_CAMSVCQ_CQ_EN,
		CAMSVCQ_CQ_EN, CAMSVCQ_SCQ_STAGGER_MODE, 1);
	CAMSV_WRITE_BITS(camsv_dev->base_scq + REG_CAMSVCQ_CQ_SUB_THR0_CTL,
		CAMSVCQ_CQ_SUB_THR0_CTL, CAMSVCQ_CQ_SUB_THR0_MODE, 1);
	/* camsv todo: start period need to be calculated */
	CAMSV_WRITE_REG(camsv_dev->base_scq  + REG_CAMSVCQ_SCQ_START_PERIOD,
		0xFFFFFFFF);

	return 0;
}
int mtk_cam_sv_cq_enable(struct mtk_camsv_device *camsv_dev)
{
	int i, subsample = 0;

	for (i = SVTAG_START; i < SVTAG_END; i++) {
		if (camsv_dev->enabled_tags & (1 << i)) {
			subsample = camsv_dev->tag_info[i].cfg_in_param.subsample;
			break;
		}
	}

	if (subsample)
		CAMSV_WRITE_BITS(camsv_dev->base_scq + REG_CAMSVCQ_CQ_EN,
			CAMSVCQ_CQ_EN, CAMSVCQ_SCQ_SUBSAMPLE_EN, 1);

	CAMSV_WRITE_BITS(camsv_dev->base_scq + REG_CAMSVCQ_CQ_SUB_THR0_CTL,
		CAMSVCQ_CQ_SUB_THR0_CTL, CAMSVCQ_CQ_SUB_THR0_EN, 1);
	CAMSV_WRITE_BITS(camsv_dev->base_scq + REG_CAMSVCQTOP_INT_0_EN,
		CAMSVCQTOP_INT_0_EN, CAMSVCQTOP_CSR_SCQ_SUB_THR_DONE_INT_EN, 1);

	return 0;
}
int mtk_cam_sv_cq_disable(struct mtk_camsv_device *camsv_dev)
{
	int i, subsample = 0;

	for (i = SVTAG_START; i < SVTAG_END; i++) {
		if (camsv_dev->enabled_tags & (1 << i)) {
			subsample = camsv_dev->tag_info[i].cfg_in_param.subsample;
			break;
		}
	}

	if (subsample) {
		CAMSV_WRITE_BITS(camsv_dev->base_scq + REG_CAMSVCQ_CQ_EN,
			CAMSVCQ_CQ_EN, CAMSVCQ_SCQ_SUBSAMPLE_EN, 0);
	}

	CAMSV_WRITE_BITS(camsv_dev->base_scq + REG_CAMSVCQ_CQ_SUB_THR0_CTL,
		CAMSVCQ_CQ_SUB_THR0_CTL, CAMSVCQ_CQ_SUB_THR0_EN, 0);
	CAMSV_WRITE_BITS(camsv_dev->base_scq + REG_CAMSVCQ_CQ_EN,
		CAMSVCQ_CQ_EN, CAMSVCQ_SCQ_STAGGER_MODE, 0);
	CAMSV_WRITE_BITS(camsv_dev->base_scq + REG_CAMSVCQ_CQ_SUB_THR0_CTL,
		CAMSVCQ_CQ_SUB_THR0_CTL, CAMSVCQ_CQ_SUB_THR0_MODE, 0);
	CAMSV_WRITE_REG(camsv_dev->base_scq  + REG_CAMSVCQ_SCQ_START_PERIOD, 0);
	CAMSV_WRITE_BITS(camsv_dev->base_scq + REG_CAMSVCQTOP_INT_0_EN,
		CAMSVCQTOP_INT_0_EN, CAMSVCQTOP_CSR_SCQ_SUB_THR_DONE_INT_EN, 0);

	return 0;
}

int mtk_cam_call_sv_pipeline_config(
	struct mtk_cam_ctx *ctx,
	struct mtk_camsv_tag_info *arr_tag,
	unsigned int tag_idx,
	unsigned int seninf_padidx,
	unsigned int hw_scen,
	unsigned int tag_order,
	unsigned int pixelmode,
	unsigned int sub_ratio,
	struct mtk_camsv_pipeline *pipeline,
	struct v4l2_mbus_framefmt *mf,
	struct v4l2_format *img_fmt)
{
	struct mtk_camsv_tag_info *tag_info;
	struct mtk_camsv_device *camsv_dev;
	struct mtkcam_ipi_input_param *cfg_in_param;
	unsigned int bbp;
	struct mtk_cam_scen scen;

	memset(&scen, 0, sizeof(scen));
	mtk_cam_scen_init(&scen);
	if (mtk_cam_ctx_has_raw(ctx))
		scen = ctx->pipe->scen_active;

	camsv_dev = ctx->sv_dev;
	tag_info = &arr_tag[tag_idx];
	cfg_in_param = &tag_info->cfg_in_param;

	tag_info->sv_pipe = pipeline;
	tag_info->seninf_padidx = seninf_padidx;
	tag_info->hw_scen = hw_scen;
	tag_info->tag_order = tag_order;
	tag_info->cfg_in_param.pixel_mode = pixelmode;
	tag_info->img_fmt = *img_fmt;
	atomic_set(&tag_info->is_config_done, 0);
	atomic_set(&tag_info->is_stream_on, 0);

	dev_info(camsv_dev->dev, "update tag_info: tag_idx(%d) hw_scen(0x%x) tag_order(%d) pixelmode(%d)\n",
		tag_idx, hw_scen, tag_order, pixelmode);

	/* update cfg_in_param */
	cfg_in_param->pixel_mode = pixelmode;
	cfg_in_param->data_pattern = 0x0;
	cfg_in_param->in_crop.p.x = 0x0;
	cfg_in_param->in_crop.p.y = 0x0;
	cfg_in_param->in_crop.s.w =	img_fmt->fmt.pix_mp.width;
	cfg_in_param->in_crop.s.h = img_fmt->fmt.pix_mp.height;
	cfg_in_param->fmt = mtk_cam_get_sensor_fmt(mf->code);
	cfg_in_param->raw_pixel_id = mtk_cam_get_sensor_pixel_id(mf->code);
	cfg_in_param->subsample = sub_ratio;

	bbp = mtk_cam_sv_xsize_cal(cfg_in_param);
	if (bbp < img_fmt->fmt.pix_mp.plane_fmt[0].bytesperline)
		tag_info->stride = img_fmt->fmt.pix_mp.plane_fmt[0].bytesperline;
	else
		tag_info->stride = bbp;

	dev_info(camsv_dev->dev,
		"camsv(%d)-(%d) sink pad code:0x%x imgo w/h/stride:%d/%d/%d scen(%s)\n",
		camsv_dev->id, tag_idx,
		mf->code, cfg_in_param->in_crop.s.w, cfg_in_param->in_crop.s.h,
		tag_info->stride, scen.dbg_str);

	if (cfg_in_param->in_crop.s.w % (1 << pixelmode))
		dev_info(camsv_dev->dev, "crop width(%d) is not the multiple of pixel mode(%d)\n",
			cfg_in_param->in_crop.s.w, pixelmode);

	return 0;
}

struct mtk_camsv_device *mtk_cam_get_used_sv_dev(struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_device *cam = ctx->cam;
	unsigned int i;
	struct device *dev_sv;
	struct mtk_camsv_device *camsv_dev;

	/* camsv todo: consider mutliple camsv hw case */
	if (ctx->used_raw_num)
		i = get_master_raw_id(cam->num_raw_drivers, ctx->pipe->enabled_raw);
	else
		i = GET_PLAT_V4L2(reserved_camsv_dev_id);

	dev_sv = cam->sv.devs[i];
	if (dev_sv == NULL) {
		dev_dbg(cam->dev, "stream on camsv device not found\n");
		return NULL;
	}
	camsv_dev = dev_get_drvdata(dev_sv);
	camsv_dev->ctx_stream_id = ctx->stream_id;

	return camsv_dev;
}

int mtk_cam_get_sv_cammux_id(struct mtk_camsv_device *camsv_dev, int tag_idx)
{
	int cammux_id = 0;

	switch (camsv_dev->id) {
	case CAMSV_0:
	case CAMSV_1:
	case CAMSV_2:
	case CAMSV_3:
		cammux_id = camsv_dev->cammux_id + tag_idx;
		break;
	case CAMSV_4:
	case CAMSV_5:
		cammux_id = camsv_dev->cammux_id;
		break;
	}

	return cammux_id;
}

int mtk_cam_sv_dev_pertag_stream_on(
	struct mtk_cam_ctx *ctx,
	unsigned int tag_idx,
	unsigned int streaming)
{
	struct mtk_camsv_device *camsv_dev = ctx->sv_dev;
	struct mtk_camsv_pipeline *sv_pipe;
	int ret = 0;

	if (streaming) {
		camsv_dev->streaming_cnt++;
		if (camsv_dev->streaming_cnt == camsv_dev->used_tag_cnt)
			ret |= mtk_cam_sv_central_common_enable(camsv_dev);
	} else {
		if (camsv_dev->streaming_cnt == 0)
			goto EXIT;
		if (camsv_dev->streaming_cnt == camsv_dev->used_tag_cnt) {
			ret |= mtk_cam_sv_cq_disable(camsv_dev);
			ret |= mtk_cam_sv_central_common_disable(camsv_dev);
		}
		if (watchdog_scenario(ctx) &&
			(tag_idx >= SVTAG_META_START &&
			tag_idx < SVTAG_META_END)) {
			sv_pipe = camsv_dev->tag_info[tag_idx].sv_pipe;
			if (sv_pipe)
				mtk_ctx_watchdog_stop(ctx, sv_pipe->id);
		}

		ret |= mtk_cam_sv_fbc_disable(camsv_dev, tag_idx);
		camsv_dev->streaming_cnt--;
	}

EXIT:
	dev_info(ctx->cam->dev, "camsv %d %s en(%d) streaming_cnt:%d\n",
		camsv_dev->id, __func__, streaming, camsv_dev->streaming_cnt);
	return ret;
}

int mtk_cam_sv_dev_stream_on(
	struct mtk_cam_ctx *ctx,
	unsigned int streaming)
{
	int ret = 0, i;

	if (ctx->sv_dev) {
		for (i = SVTAG_START; i < SVTAG_END; i++) {
			if (ctx->sv_dev->enabled_tags & (1 << i))
				mtk_cam_sv_dev_pertag_stream_on(ctx, i, streaming);
		}
	}

	return ret;
}

static void mtk_camsv_pipeline_queue_setup(
	struct mtk_camsv_pipeline *pipe)
{
	unsigned int node_idx, i;

	node_idx = 0;
	/* setup the capture queue */
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
	struct v4l2_subdev *sd = &pipe->subdev;
	struct mtk_cam_video_device *video;
	unsigned int i, j;
	int ret;


	/* initialize subdev */
	v4l2_subdev_init(sd, &mtk_camsv_subdev_ops);
	sd->entity.function = MEDIA_ENT_F_PROC_VIDEO_PIXEL_FORMATTER;
	sd->entity.ops = &mtk_camsv_media_entity_ops;
	sd->flags = V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS;
	ret = snprintf(sd->name, sizeof(sd->name),
		 "%s-%d", dev_driver_string(dev), (pipe->id - MTKCAM_SUBDEV_CAMSV_START));
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

	/* setup pads of camsv pipeline */
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
	for (j = 0; j < i; j++)
		mtk_cam_video_unregister(pipe->vdev_nodes + j);

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

int mtk_camsv_register_entities(
	struct mtk_camsv *sv,
	struct v4l2_device *v4l2_dev)
{
	unsigned int i;
	int ret;

	for (i = 0; i < MAX_SV_PIPELINE_NUN; i++) {
		struct mtk_camsv_pipeline *pipe = sv->pipelines + i;
		pipe->id = MTKCAM_SUBDEV_CAMSV_START + i;
		pipe->sv = sv;
		memset(pipe->cfg, 0, sizeof(*pipe->cfg));
		/* camsv todo: should have a better implementation here */
		ret = mtk_camsv_pipeline_register(
						MTKCAM_SUBDEV_CAMSV_START + i,
						sv->devs[0],
						sv->pipelines + i, v4l2_dev);
	}

	return 0;
}

void mtk_camsv_unregister_entities(struct mtk_camsv *sv)
{
	unsigned int i;

	for (i = 0; i < MAX_SV_PIPELINE_NUN; i++)
		mtk_camsv_pipeline_unregister(sv->pipelines + i);
}

void camsv_dump_dma_debug_data(struct mtk_camsv_device *camsv_dev)
{
	u32 smi_crc_address, smi_crc_data, tag1_tag2_crc, len1_len2_crc, smi_cnt;
	u32 debug_img1, debug_len1, cmd_cnt_img1, cmd_cnt_len1;

	writel_relaxed(0x00010001, camsv_dev->base_dma + REG_CAMSV_DMATOP_DMA_DEBUG_SEL);
	smi_crc_address = readl_relaxed(camsv_dev->base_dma + REG_CAMSV_DMATOP_DMA_DEBUG_PORT);
	writel_relaxed(0x00010003, camsv_dev->base_dma + REG_CAMSV_DMATOP_DMA_DEBUG_SEL);
	smi_crc_data = readl_relaxed(camsv_dev->base_dma + REG_CAMSV_DMATOP_DMA_DEBUG_PORT);
	writel_relaxed(0x00010005, camsv_dev->base_dma + REG_CAMSV_DMATOP_DMA_DEBUG_SEL);
	tag1_tag2_crc = readl_relaxed(camsv_dev->base_dma + REG_CAMSV_DMATOP_DMA_DEBUG_PORT);
	writel_relaxed(0x00010009, camsv_dev->base_dma + REG_CAMSV_DMATOP_DMA_DEBUG_SEL);
	len1_len2_crc = readl_relaxed(camsv_dev->base_dma + REG_CAMSV_DMATOP_DMA_DEBUG_PORT);
	writel_relaxed(0x0001000F, camsv_dev->base_dma + REG_CAMSV_DMATOP_DMA_DEBUG_SEL);
	smi_cnt = readl_relaxed(camsv_dev->base_dma + REG_CAMSV_DMATOP_DMA_DEBUG_PORT);
	writel_relaxed(0x0001010B, camsv_dev->base_dma + REG_CAMSV_DMATOP_DMA_DEBUG_SEL);
	debug_img1 = readl_relaxed(camsv_dev->base_dma + REG_CAMSV_DMATOP_DMA_DEBUG_PORT);
	writel_relaxed(0x0001010C, camsv_dev->base_dma + REG_CAMSV_DMATOP_DMA_DEBUG_SEL);
	debug_len1 = readl_relaxed(camsv_dev->base_dma + REG_CAMSV_DMATOP_DMA_DEBUG_PORT);
	writel_relaxed(0x0001010E, camsv_dev->base_dma + REG_CAMSV_DMATOP_DMA_DEBUG_SEL);
	cmd_cnt_img1 = readl_relaxed(camsv_dev->base_dma + REG_CAMSV_DMATOP_DMA_DEBUG_PORT);
	writel_relaxed(0x0001090E, camsv_dev->base_dma + REG_CAMSV_DMATOP_DMA_DEBUG_SEL);
	cmd_cnt_len1 = readl_relaxed(camsv_dev->base_dma + REG_CAMSV_DMATOP_DMA_DEBUG_PORT);

	dev_info_ratelimited(camsv_dev->dev,
		"dma_top_debug:0x%x_0x%x_0x%x_0x%x_0x%x_0x%x_0x%x_0x%x_0x%x\n",
		smi_crc_address, smi_crc_data, tag1_tag2_crc, len1_len2_crc,
		smi_cnt, debug_img1, debug_len1, cmd_cnt_img1,
		cmd_cnt_len1);
}

void camsv_irq_handle_err(
	struct mtk_camsv_device *camsv_dev,
	unsigned int dequeued_frame_seq_no, unsigned int tag_idx)
{
	struct mtk_cam_request_stream_data *s_data;
	struct mtk_cam_device *cam = camsv_dev->cam;
	struct mtk_cam_ctx *ctx;
	struct mtk_camsv_tag_info tag_info = camsv_dev->tag_info[tag_idx];
	unsigned int stream_id;

	dev_info_ratelimited(camsv_dev->dev,
		"TAG_IDX:%d TG PATHCFG/SENMODE FRMSIZE/R RGRABPXL/LIN:0x%x/%x 0x%x/%x 0x%x/%x\n",
		tag_idx,
		readl_relaxed(camsv_dev->base + REG_CAMSVCENTRAL_PATH_CFG),
		readl_relaxed(camsv_dev->base + REG_CAMSVCENTRAL_SEN_MODE),
		readl_relaxed(camsv_dev->base + REG_CAMSVCENTRAL_FRMSIZE_ST),
		readl_relaxed(camsv_dev->base + REG_CAMSVCENTRAL_FRMSIZE_ST_R),
		readl_relaxed(camsv_dev->base + REG_CAMSVCENTRAL_GRAB_PXL_TAG1 +
			CAMSVCENTRAL_GRAB_PXL_TAG_SHIFT * tag_idx),
		readl_relaxed(camsv_dev->base + REG_CAMSVCENTRAL_GRAB_LIN_TAG1 +
			CAMSVCENTRAL_GRAB_LIN_TAG_SHIFT * tag_idx));

	if (camsv_dev->tag_info[tag_idx].hw_scen &
		MTK_CAMSV_SUPPORTED_SPECIAL_HW_SCENARIO) {
		if (camsv_dev->ctx_stream_id >= MTKCAM_SUBDEV_CAMSV_END) {
			dev_info(camsv_dev->dev, "stream id out of range : %d",
					camsv_dev->ctx_stream_id);
			return;
		}
		ctx = &cam->ctxs[camsv_dev->ctx_stream_id];
		if (!ctx) {
			dev_info(camsv_dev->dev, "%s: cannot find ctx\n", __func__);
			return;
		}
		stream_id = camsv_dev->ctx_stream_id;
	} else {
		if (camsv_dev->ctx_stream_id >= MTKCAM_SUBDEV_CAMSV_END) {
			dev_info(camsv_dev->dev, "stream id out of range : %d",
					camsv_dev->ctx_stream_id);
			return;
		}
		ctx = &cam->ctxs[camsv_dev->ctx_stream_id];
		if (!ctx) {
			dev_info(camsv_dev->dev, "%s: cannot find ctx\n", __func__);
			return;
		}
		if (!tag_info.sv_pipe) {
			dev_info(camsv_dev->dev, "%s: tag_idx:%d is not controlled by user\n",
				__func__, tag_idx);
			return;
		}
		stream_id = tag_info.sv_pipe->id;
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
	int tag_idx;
	unsigned int fbc_imgo_status, imgo_addr, imgo_addr_msb, dcif_set, tg_vf_con;
	unsigned int first_tag, last_tag, group_info, i;
	/* camsv todo: show more error detail */
	for (tag_idx = 0; tag_idx < MAX_SV_HW_TAGS; tag_idx++) {
		if (!(data->err_tags & (1 << tag_idx)))
			continue;
		fbc_imgo_status =
		readl_relaxed(camsv_dev->base + REG_CAMSVCENTRAL_FBC1_TAG1 + tag_idx *
			CAMSVCENTRAL_FBC1_TAG_SHIFT);
		imgo_addr =
			readl_relaxed(camsv_dev->base_dma + REG_CAMSVDMATOP_WDMA_BASE_ADDR_IMG1
			+ tag_idx * CAMSVDMATOP_WDMA_BASE_ADDR_IMG_SHIFT);
		imgo_addr_msb =
			readl_relaxed(camsv_dev->base_dma +
			REG_CAMSVDMATOP_WDMA_BASE_ADDR_MSB_IMG1 +  tag_idx *
			CAMSVDMATOP_WDMA_BASE_ADDR_MSB_IMG_SHIFT);
		dcif_set =
			readl_relaxed(camsv_dev->base_inner + REG_E_CAMSVCENTRAL_DCIF_SET);
		tg_vf_con =
			readl_relaxed(camsv_dev->base_inner + REG_CAMSVCENTRAL_VF_CON);
		dev_info_ratelimited(camsv_dev->dev,
			"camsv_id:%d tag_idx:%d error_status:0x%x fbc_status:0x%x imgo_addr:0x%x%08x dcif_set:0x%x tg_vf:0x%x\n",
			camsv_dev->id, tag_idx, err_status, fbc_imgo_status, imgo_addr_msb,
			imgo_addr, dcif_set, tg_vf_con);
		camsv_irq_handle_err(camsv_dev, frame_idx_inner, tag_idx);
	}
	first_tag = readl_relaxed(camsv_dev->base + REG_CAMSVCENTRAL_FIRST_TAG);
	last_tag = readl_relaxed(camsv_dev->base + REG_CAMSVCENTRAL_LAST_TAG);
	for (i = 0; i < MAX_SV_HW_GROUPS; i++) {
		group_info = readl_relaxed(camsv_dev->base + REG_CAMSVCENTRAL_GROUP_TAG0 +
			REG_CAMSVCENTRAL_GROUP_TAG_SHIFT * i);
		dev_info_ratelimited(camsv_dev->dev, "group%d: group_info:%x", i, group_info);
	}
	dev_info_ratelimited(camsv_dev->dev, "first_tag:0x%x last_tag:0x%x",
		first_tag, last_tag);
	if (!(data->err_tags) && (err_status & CAMSVCENTRAL_DMA_SRAM_FULL_ST))
		dev_info_ratelimited(camsv_dev->dev, "camsv_id:%d camsv dma full error_status:0x%x",
			camsv_dev->id, err_status);
	/* dump dma debug data */
	camsv_dump_dma_debug_data(camsv_dev);

}

static irqreturn_t mtk_irq_camsv_done(int irq, void *data)
{
	struct mtk_camsv_device *camsv_dev = (struct mtk_camsv_device *)data;
	struct mtk_camsys_irq_info irq_info;
	unsigned int dequeued_imgo_seq_no, dequeued_imgo_seq_no_inner;
	unsigned int irq_done_status;
	unsigned int irq_flag = 0;
	bool wake_thread = 0;

	memset(&irq_info, 0, sizeof(irq_info));
	dequeued_imgo_seq_no =
		readl_relaxed(camsv_dev->base + REG_CAMSVCENTRAL_FRAME_SEQ_NO);
	dequeued_imgo_seq_no_inner =
		readl_relaxed(camsv_dev->base_inner + REG_CAMSVCENTRAL_FRAME_SEQ_NO);
	irq_done_status	=
		readl_relaxed(camsv_dev->base + REG_CAMSVCENTRAL_DONE_STATUS);

	dev_dbg(camsv_dev->dev, "camsv-%d: done status:0x%x seq_no:%d_%d",
		camsv_dev->id, irq_done_status,
		dequeued_imgo_seq_no_inner, dequeued_imgo_seq_no);

	irq_info.irq_type = 0;
	irq_info.ts_ns = ktime_get_boottime_ns();
	irq_info.frame_idx = dequeued_imgo_seq_no;
	irq_info.frame_idx_inner = dequeued_imgo_seq_no_inner;

	if (irq_done_status & CAMSVCENTRAL_SW_GP_PASS1_DONE_0_ST)
		irq_info.done_groups |= (1<<0);
	if (irq_done_status & CAMSVCENTRAL_SW_GP_PASS1_DONE_1_ST)
		irq_info.done_groups |= (1<<1);
	if (irq_done_status & CAMSVCENTRAL_SW_GP_PASS1_DONE_2_ST)
		irq_info.done_groups |= (1<<2);
	if (irq_done_status & CAMSVCENTRAL_SW_GP_PASS1_DONE_3_ST)
		irq_info.done_groups |= (1<<3);
	irq_info.irq_type |= (1 << CAMSYS_IRQ_FRAME_DONE);
	irq_flag = irq_info.irq_type;
	if (irq_flag && push_msgfifo(camsv_dev, &irq_info) == 0)
		wake_thread = 1;

	return wake_thread ? IRQ_WAKE_THREAD : IRQ_HANDLED;

}

static irqreturn_t mtk_irq_camsv_sof(int irq, void *data)
{
	struct mtk_camsv_device *camsv_dev = (struct mtk_camsv_device *)data;
	struct mtk_camsys_irq_info irq_info;
	unsigned int dequeued_imgo_seq_no, dequeued_imgo_seq_no_inner;
	unsigned int tg_cnt;
	unsigned int irq_sof_status;
	unsigned int irq_flag = 0;
	bool wake_thread = 0;
	unsigned int i;

	memset(&irq_info, 0, sizeof(irq_info));
	dequeued_imgo_seq_no =
		readl_relaxed(camsv_dev->base + REG_CAMSVCENTRAL_FRAME_SEQ_NO);
	dequeued_imgo_seq_no_inner =
		readl_relaxed(camsv_dev->base_inner + REG_CAMSVCENTRAL_FRAME_SEQ_NO);
	irq_sof_status	=
		readl_relaxed(camsv_dev->base + REG_CAMSVCENTRAL_SOF_STATUS);
	for (i = 0; i < MAX_SV_HW_GROUPS; i++) {
		camsv_dev->active_group_info[i] =
			readl_relaxed(camsv_dev->base_inner + REG_CAMSVCENTRAL_GROUP_TAG0 +
				REG_CAMSVCENTRAL_GROUP_TAG_SHIFT * i);
	}
	camsv_dev->first_tag =
		readl_relaxed(camsv_dev->base_inner + REG_CAMSVCENTRAL_FIRST_TAG);
	camsv_dev->last_tag =
		readl_relaxed(camsv_dev->base_inner + REG_CAMSVCENTRAL_LAST_TAG);
	tg_cnt =
		readl_relaxed(camsv_dev->base + REG_CAMSVCENTRAL_VF_ST_TAG1 +
				CAMSVCENTRAL_VF_ST_TAG_SHIFT * 3);
	tg_cnt = (tg_cnt & 0xff000000) >> 24;
	dev_dbg(camsv_dev->dev, "camsv-%d: sof status:0x%x seq_no:%d_%d group_tags:0x%x_%x_%x_%x first_tag:0x%x last_tag:0x%x VF_ST_TAG4:%d",
		camsv_dev->id, irq_sof_status,
		dequeued_imgo_seq_no_inner, dequeued_imgo_seq_no,
		camsv_dev->active_group_info[0],
		camsv_dev->active_group_info[1],
		camsv_dev->active_group_info[2],
		camsv_dev->active_group_info[3],
		camsv_dev->first_tag,
		camsv_dev->last_tag,
		tg_cnt);

	irq_info.irq_type = 0;
	irq_info.ts_ns = ktime_get_boottime_ns();
	irq_info.frame_idx = dequeued_imgo_seq_no;
	irq_info.frame_idx_inner = dequeued_imgo_seq_no_inner;

	if (irq_sof_status & CAMSVCENTRAL_SOF_ST_TAG1)
		irq_info.sof_tags |= (1 << 0);
	if (irq_sof_status & CAMSVCENTRAL_SOF_ST_TAG2)
		irq_info.sof_tags |= (1 << 1);
	if (irq_sof_status & CAMSVCENTRAL_SOF_ST_TAG3)
		irq_info.sof_tags |= (1 << 2);
	if (irq_sof_status & CAMSVCENTRAL_SOF_ST_TAG4)
		irq_info.sof_tags |= (1 << 3);
	if (irq_sof_status & CAMSVCENTRAL_SOF_ST_TAG5)
		irq_info.sof_tags |= (1 << 4);
	if (irq_sof_status & CAMSVCENTRAL_SOF_ST_TAG6)
		irq_info.sof_tags |= (1 << 5);
	if (irq_sof_status & CAMSVCENTRAL_SOF_ST_TAG7)
		irq_info.sof_tags |= (1 << 6);
	if (irq_sof_status & CAMSVCENTRAL_SOF_ST_TAG8)
		irq_info.sof_tags |= (1 << 7);
	irq_info.irq_type |= (1 << CAMSYS_IRQ_FRAME_START);
	camsv_dev->sof_count++;
	camsv_dev->last_sof_time_ns = irq_info.ts_ns;
	camsv_dev->tg_cnt = tg_cnt;
	camsv_dev->sof_timestamp = ktime_get_boottime_ns();
	irq_flag = irq_info.irq_type;
	if (irq_flag && push_msgfifo(camsv_dev, &irq_info) == 0)
		wake_thread = 1;

	return wake_thread ? IRQ_WAKE_THREAD : IRQ_HANDLED;
}

static irqreturn_t mtk_irq_camsv_err(int irq, void *data)
{
	struct mtk_camsv_device *camsv_dev = (struct mtk_camsv_device *)data;
	struct mtk_camsys_irq_info irq_info;
	unsigned int dequeued_imgo_seq_no, dequeued_imgo_seq_no_inner;
	unsigned int err_status;
	bool wake_thread = 0;

	dequeued_imgo_seq_no =
		readl_relaxed(camsv_dev->base + REG_CAMSVCENTRAL_FRAME_SEQ_NO);
	dequeued_imgo_seq_no_inner =
		readl_relaxed(camsv_dev->base_inner + REG_CAMSVCENTRAL_FRAME_SEQ_NO);
	err_status =
		readl_relaxed(camsv_dev->base_inner + REG_CAMSVCENTRAL_ERR_STATUS);

	dev_dbg(camsv_dev->dev, "camsv-%d: error status:0x%x seq_no:%d_%d",
		camsv_dev->id, err_status,
		dequeued_imgo_seq_no_inner, dequeued_imgo_seq_no);

	irq_info.irq_type = 0;
	irq_info.ts_ns = ktime_get_boottime_ns();
	irq_info.frame_idx = dequeued_imgo_seq_no;
	irq_info.frame_idx_inner = dequeued_imgo_seq_no_inner;

	if (err_status) {
		struct mtk_camsys_irq_info err_info;
		memset(&err_info, 0, sizeof(err_info));
		if (err_status & ERR_ST_MASK_TAG1_ERR)
			err_info.err_tags |= (1 << 0);
		if (err_status & ERR_ST_MASK_TAG2_ERR)
			err_info.err_tags |= (1 << 1);
		if (err_status & ERR_ST_MASK_TAG3_ERR)
			err_info.err_tags |= (1 << 2);
		if (err_status & ERR_ST_MASK_TAG4_ERR)
			err_info.err_tags |= (1 << 3);
		if (err_status & ERR_ST_MASK_TAG5_ERR)
			err_info.err_tags |= (1 << 4);
		if (err_status & ERR_ST_MASK_TAG6_ERR)
			err_info.err_tags |= (1 << 5);
		if (err_status & ERR_ST_MASK_TAG7_ERR)
			err_info.err_tags |= (1 << 6);
		if (err_status & ERR_ST_MASK_TAG8_ERR)
			err_info.err_tags |= (1 << 7);
		err_info.irq_type = 1 << CAMSYS_IRQ_ERROR;
		err_info.ts_ns = irq_info.ts_ns;
		err_info.frame_idx = irq_info.frame_idx;
		err_info.frame_idx_inner = irq_info.frame_idx_inner;
		err_info.e.err_status = err_status;

		if (push_msgfifo(camsv_dev, &err_info) == 0)
			wake_thread = 1;
	}

	return wake_thread ? IRQ_WAKE_THREAD : IRQ_HANDLED;
}

static irqreturn_t mtk_irq_camsv_cq_done(int irq, void *data)
{
	struct mtk_camsv_device *camsv_dev = (struct mtk_camsv_device *)data;
	struct mtk_camsys_irq_info irq_info;
	unsigned int dequeued_imgo_seq_no, dequeued_imgo_seq_no_inner;
	unsigned int cq_done_status;
	unsigned int irq_flag = 0;
	bool wake_thread = 0;

	dequeued_imgo_seq_no =
		readl_relaxed(camsv_dev->base + REG_CAMSVCENTRAL_FRAME_SEQ_NO);
	dequeued_imgo_seq_no_inner =
		readl_relaxed(camsv_dev->base_inner + REG_CAMSVCENTRAL_FRAME_SEQ_NO);
	cq_done_status =
		readl_relaxed(camsv_dev->base_scq + REG_CAMSVCQTOP_INT_0_STATUS);

	dev_dbg(camsv_dev->dev, "camsv-%d: cq done status:0x%x seq_no:%d_%d",
		camsv_dev->id, cq_done_status,
		dequeued_imgo_seq_no_inner, dequeued_imgo_seq_no);

	irq_info.irq_type = 0;
	irq_info.ts_ns = ktime_get_boottime_ns();
	irq_info.frame_idx = dequeued_imgo_seq_no;
	irq_info.frame_idx_inner = dequeued_imgo_seq_no_inner;

	if (cq_done_status & CAMSVCQTOP_SCQ_SUB_THR_DONE)
		irq_info.irq_type |= (1 << CAMSYS_IRQ_SETTING_DONE);
	irq_flag = irq_info.irq_type;
	if (irq_flag && push_msgfifo(camsv_dev, &irq_info) == 0)
		wake_thread = 1;

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
		if (unlikely(irq_info.irq_type == (1 << CAMSYS_IRQ_ERROR))) {
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

static int mtk_camsv_pm_suspend(struct device *dev)
{
	struct mtk_camsv_device *camsv_dev = dev_get_drvdata(dev);
	u32 val;
	int ret;

	dev_dbg(dev, "- %s\n", __func__);

	if (pm_runtime_suspended(dev))
		return 0;

	/* Disable ISP's view finder and wait for TG idle */
	dev_info(dev, "camsv suspend, disable VF\n");
	val = readl(camsv_dev->base + REG_CAMSVCENTRAL_VF_CON);
	writel(val & (~CAMSVCENTRAL_VF_CON_VFDATA_EN),
		camsv_dev->base + REG_CAMSVCENTRAL_VF_CON);
#ifdef CAMSV_TODO
	// camsv todo: implement this usage
	ret = readl_poll_timeout_atomic(
					camsv_dev->base + REG_CAMSV_TG_INTER_ST, val,
					(val & CAMSV_TG_CS_MASK) == CAMSV_TG_IDLE_ST,
					USEC_PER_MSEC, MTK_CAMSV_STOP_HW_TIMEOUT);
	if (ret)
		dev_info(dev, "can't stop HW:%d:0x%x\n", ret, val);
#endif

	/* Disable CMOS */
	val = readl(camsv_dev->base + REG_CAMSVCENTRAL_SEN_MODE);
	writel(val & (~CAMSVCENTRAL_SEN_MODE_CMOS_EN),
		camsv_dev->base + REG_CAMSVCENTRAL_SEN_MODE);

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
	dev_info(dev, "camsv resume, enable CMOS/VF\n");
	val = readl(camsv_dev->base + REG_CAMSVCENTRAL_SEN_MODE);
	writel(val | CAMSVCENTRAL_SEN_MODE_CMOS_EN,
		camsv_dev->base + REG_CAMSVCENTRAL_SEN_MODE);

	/* Enable VF */
	val = readl(camsv_dev->base + REG_CAMSVCENTRAL_VF_CON);
	writel(val | CAMSVCENTRAL_VF_CON_VFDATA_EN,
		camsv_dev->base + REG_CAMSVCENTRAL_VF_CON);

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

	sv->used_tag_cnt = 0;
	sv->streaming_cnt = 0;
	sv->enabled_tags = 0;
	sv->ctx_stream_id = 0;

	ret = of_property_read_u32(dev->of_node, "mediatek,camsv-id",
						       &sv->id);
	if (ret) {
		dev_dbg(dev, "missing camid property\n");
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

	/* base dma outer register */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "base_DMA");
	if (!res) {
		dev_info(dev, "failed to get mem\n");
		return -ENODEV;
	}

	sv->base_dma = devm_ioremap_resource(dev, res);
	if (IS_ERR(sv->base_dma)) {
		dev_dbg(dev, "failed to map register base dma\n");
		return PTR_ERR(sv->base_dma);
	}
	dev_dbg(dev, "camsv, map_dma_addr=0x%pK\n", sv->base_dma);

	/* base scq outer register */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "base_SCQ");
	if (!res) {
		dev_info(dev, "failed to get mem\n");
		return -ENODEV;
	}

	sv->base_scq = devm_ioremap_resource(dev, res);
	if (IS_ERR(sv->base_scq)) {
		dev_dbg(dev, "failed to map register base scq\n");
		return PTR_ERR(sv->base_scq);
	}
	dev_dbg(dev, "camsv, map_scq_addr=0x%pK\n", sv->base_scq);

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

	dev_dbg(dev, "camsv, map_addr=0x%pK\n", sv->base_inner);

	/* base inner dma register */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "inner_base_DMA");
	if (!res) {
		dev_dbg(dev, "failed to get mem\n");
		return -ENODEV;
	}

	sv->base_inner_dma = devm_ioremap_resource(dev, res);
	if (IS_ERR(sv->base_inner_dma)) {
		dev_dbg(dev, "failed to map register inner base dma\n");
		return PTR_ERR(sv->base_inner_dma);
	}
	dev_dbg(dev, "camsv, map_addr(inner dma)=0x%pK\n", sv->base_inner_dma);

	/* base inner scq register */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "inner_base_SCQ");
	if (!res) {
		dev_dbg(dev, "failed to get mem\n");
		return -ENODEV;
	}

	sv->base_inner_scq = devm_ioremap_resource(dev, res);
	if (IS_ERR(sv->base_inner_scq)) {
		dev_dbg(dev, "failed to map register inner base\n");
		return PTR_ERR(sv->base_inner_scq);
	}
	dev_dbg(dev, "camsv, map_addr(inner scq)=0x%pK\n", sv->base_inner_scq);

	for (i = 0; i < CAMSV_IRQ_NUM; i++) {
		sv->irq[i] = platform_get_irq(pdev, i);
		if (!sv->irq[i]) {
			dev_dbg(dev, "failed to get irq\n");
			return -ENODEV;
		}
	}

	for (i = 0; i < CAMSV_IRQ_NUM; i++) {
		sv->irq[i] = platform_get_irq(pdev, i);
		if (!sv->irq[i]) {
			dev_dbg(dev, "failed to get irq\n");
			return -ENODEV;
		}
	}

	for (i = 0; i < CAMSV_IRQ_NUM; i++) {
		if (i == 0)
			ret = devm_request_threaded_irq(dev, sv->irq[i],
						mtk_irq_camsv_done,
						mtk_thread_irq_camsv,
						0, dev_name(dev), sv);
		else if (i == 1)
			ret = devm_request_threaded_irq(dev, sv->irq[i],
						mtk_irq_camsv_err,
						mtk_thread_irq_camsv,
						0, dev_name(dev), sv);
		else if (i == 2)
			ret = devm_request_threaded_irq(dev, sv->irq[i],
						mtk_irq_camsv_sof,
						mtk_thread_irq_camsv,
						0, dev_name(dev), sv);
		else
			ret = devm_request_threaded_irq(dev, sv->irq[i],
						mtk_irq_camsv_cq_done,
						mtk_thread_irq_camsv,
						0, dev_name(dev), sv);
		if (ret) {
			dev_dbg(dev, "failed to request irq=%d\n", sv->irq[i]);
			return ret;
		}

		dev_info(dev, "registered irq=%d\n", sv->irq[i]);

		disable_irq(sv->irq[i]);

		dev_info(dev, "%s:disable irq %d\n", __func__, sv->irq[i]);
	}

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
	larbs = (larbs == -ENOENT) ? 0:larbs;
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

	camsv_dev->cam = NULL;
	sv->devs[camsv_dev->id] = NULL;
}

static const struct component_ops mtk_camsv_component_ops = {
	.bind = mtk_camsv_component_bind,
	.unbind = mtk_camsv_component_unbind,
};

static int mtk_camsv_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_camsv_device *camsv_dev;
	int ret;

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

	pm_runtime_disable(dev);

	component_del(dev, &mtk_camsv_component_ops);
	return 0;
}

static int mtk_camsv_runtime_suspend(struct device *dev)
{
	struct mtk_camsv_device *camsv_dev = dev_get_drvdata(dev);
	int i;

	dev_dbg(dev, "%s:disable clock\n", __func__);

	for (i = 0; i < CAMSV_IRQ_NUM; i++)
		disable_irq(camsv_dev->irq[i]);

	for (i = 0; i < camsv_dev->num_clks; i++)
		clk_disable_unprepare(camsv_dev->clks[i]);

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

	for (i = 0; i < CAMSV_IRQ_NUM; i++) {
		enable_irq(camsv_dev->irq[i]);
		dev_dbg(dev, "%s:enable irq %d\n", __func__, camsv_dev->irq[i]);
	}


	dev_info(dev, "%s:enable irq\n", __func__);

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

