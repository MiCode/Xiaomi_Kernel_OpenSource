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
	}

	dev_info(sv->cam_dev, "%s:camsv-%d: en %d\n",
		 __func__, pipe->id - MTKCAM_SUBDEV_CAMSV_START);
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

	if (!mtk_camsv_try_fmt(sd, fmt)) {
		mf = get_sv_fmt(pipe, state, fmt->pad, fmt->which);
		fmt->format = *mf;
		/* camsv todo: log level */
		dev_info(sv->cam_dev,
			"sd:%s pad:%d set format w/h/code %d/%d/0x%x\n",
			sd->name, fmt->pad, mf->width, mf->height, mf->code);
	} else {
		mf = get_sv_fmt(pipe, state, fmt->pad, fmt->which);
		*mf = fmt->format;
		/* camsv todo: log level */
		dev_info(sv->cam_dev,
			"sd:%s pad:%d try format w/h/code %d/%d/0x%x\n",
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
			  struct v4l2_subdev_state *state,
			  struct v4l2_subdev_format *fmt)
{
	struct media_request *req;
	struct mtk_cam_request *cam_req;
	struct mtk_cam_request_stream_data *stream_data;

	struct mtk_camsv_pipeline *pipe =
		container_of(sd, struct mtk_camsv_pipeline, subdev);
	struct mtk_cam_device *cam = dev_get_drvdata(pipe->sv->cam_dev);

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY)
		return mtk_camsv_call_set_fmt(sd, state, fmt);

	/* if the pipeline is streaming, pending the change */
	if (!sd->entity.stream_count)
		return mtk_camsv_call_set_fmt(sd, state, fmt);

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
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SBGGR10,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SBGGR12,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SBGGR14,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGBRG10,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGBRG12,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGBRG14,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGRBG10,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGRBG12,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGRBG14,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SRGGB10,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SRGGB12,
		},
	},
	{
		.vfmt.fmt.pix_mp = {
			.width = SV_IMG_MAX_WIDTH,
			.height = SV_IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SRGGB14,
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

static const char *sv_capture_queue_names[MAX_SV_PIPELINE_NUN][MTK_CAMSV_TOTAL_CAPTURE_QUEUES] = {
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
		if (dev->group_info[i] != 0) {
			CAMSV_WRITE_REG(dev->base + REG_CAMSVCENTRAL_GROUP_TAG0 +
				REG_CAMSVCENTRAL_GROUP_TAG_SHIFT * i,
				dev->group_info[i]);

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
	if (dev->tag_info[tag_idx].exp_order == MTK_CAM_EXP_FIRST)
		CAMSV_WRITE_REG(dev->base + REG_CAMSVCENTRAL_FIRST_TAG, (1 << tag_idx));
	else if (dev->tag_info[tag_idx].exp_order == MTK_CAM_EXP_LAST)
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

	/* imgo con */
	switch (dev->id) {
	case CAMSV_0: //342
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON3_IMG, 0x080406AE); //urgent
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON2_IMG, 0x05580402); //ultra
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON1_IMG, 0x02AC0156); //pre-ultra
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON4_IMG, 0x01560000); //DVFS
		break;
	case CAMSV_1: //231
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON3_IMG, 0x056A0483); //urgent
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON2_IMG, 0x039C02B5); //ultra
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON1_IMG, 0x01CE00E7); //pre-ultra
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON4_IMG, 0x00E70000); //DVFS
		break;
	case CAMSV_2: //205
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON3_IMG, 0x04CE0401); //urgent
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON2_IMG, 0x03340267); //ultra
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON1_IMG, 0x019A00CD); //pre-ultra
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON4_IMG, 0x00CD0000); //DVFS
		break;
	case CAMSV_3: //128
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON3_IMG, 0x03000280); //urgent
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON2_IMG, 0x02000180); //ultra
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON1_IMG, 0x01000080); //pre-ultra
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON4_IMG, 0x00800000); //DVFS
		break;
	case CAMSV_4: //36
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON3_IMG, 0x00D800B4); //urgent
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON2_IMG, 0x0090006C); //ultra
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON1_IMG, 0x00480024); //pre-ultra
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON4_IMG, 0x00240000); //DVFS
		break;
	case CAMSV_5: //36
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON3_IMG, 0x00D800B4); //urgent
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON2_IMG, 0x0090006C); //ultra
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON1_IMG, 0x00480024); //pre-ultra
		CAMSV_WRITE_REG(dev->base_dma + REG_CAMSVDMATOP_CON4_IMG, 0x00240000); //DVFS
		break;
	}

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

int mtk_cam_sv_central_enable(struct mtk_camsv_device *dev)
{
	int ret = 0;

	CAMSV_WRITE_BITS(dev->base + REG_CAMSVCENTRAL_SEN_MODE,
		CAMSVCENTRAL_SEN_MODE, CMOS_EN, 1);

	return ret;
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

	CAMSV_WRITE_REG(dev->base + REG_CAMSVCENTRAL_DMA_EN_IMG, dev->enabled_tags);
	mtk_cam_sv_toggle_db(dev);

	if (CAMSV_READ_BITS(dev->base + REG_CAMSVCENTRAL_SEN_MODE,
		CAMSVCENTRAL_SEN_MODE, CMOS_EN)) {
		CAMSV_WRITE_BITS(dev->base + REG_CAMSVCENTRAL_VF_CON,
			CAMSVCENTRAL_VF_CON, VFDATA_EN, 1);
	}

	return ret;
}

int mtk_cam_sv_print_fbc_status(struct mtk_camsv_device *dev, unsigned int tag)
{
	int ret = 0;

	dev_info(dev->dev, "%s tag(%d) FBC_IMGO_CTRL1/2:0x%x/0x%x\n",
		__func__, tag,
		CAMSV_READ_REG(dev->base + REG_CAMSVCENTRAL_FBC0_TAG1 +
			CAMSVCENTRAL_FBC0_TAG_SHIFT * tag),
		CAMSV_READ_REG(dev->base + REG_CAMSVCENTRAL_FBC1_TAG1 +
			CAMSVCENTRAL_FBC0_TAG_SHIFT * tag));

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
		CAMSVCENTRAL_FBC0_TAG1, FBC_SUB_EN_TAG1, cfg_in_param->subsample);
	CAMSV_WRITE_BITS(dev->base +
		REG_CAMSVCENTRAL_FBC0_TAG1 + CAMSVCENTRAL_FBC0_TAG_SHIFT * tag_idx,
		CAMSVCENTRAL_FBC0_TAG1, FBC_EN_TAG1, 1);

	return ret;
}

int mtk_cam_sv_tg_disable(struct mtk_camsv_device *dev)
{
	int ret = 0;

	CAMSV_WRITE_BITS(dev->base + REG_CAMSVCENTRAL_SEN_MODE,
		CAMSVCENTRAL_SEN_MODE, CMOS_EN, 0);

	return ret;
}

int mtk_cam_sv_central_common_disable(struct mtk_camsv_device *dev)
{
	int ret = 0;

	if (CAMSV_READ_BITS(dev->base + REG_CAMSVCENTRAL_VF_CON,
			CAMSVCENTRAL_VF_CON, VFDATA_EN)) {
		CAMSV_WRITE_BITS(dev->base + REG_CAMSVCENTRAL_VF_CON,
			CAMSVCENTRAL_VF_CON, VFDATA_EN, 0);
		mtk_cam_sv_toggle_db(dev);
	}
	sv_reset(dev);

	CAMSV_WRITE_REG(dev->base + REG_CAMSVCENTRAL_DMA_EN_IMG, 0);

	CAMSV_WRITE_BITS(dev->base + REG_CAMSVCENTRAL_MODULE_DB,
		CAMSVCENTRAL_MODULE_DB, CAM_DB_EN, 0);

	CAMSV_WRITE_REG(dev->base + REG_CAMSVCENTRAL_DONE_STATUS_EN, 0);
	CAMSV_WRITE_REG(dev->base + REG_CAMSVCENTRAL_ERR_STATUS_EN, 0);
	CAMSV_WRITE_REG(dev->base + REG_CAMSVCENTRAL_SOF_STATUS_EN, 0);

	CAMSV_WRITE_BITS(dev->base + REG_CAMSVCENTRAL_MODULE_DB,
		CAMSVCENTRAL_MODULE_DB, CAM_DB_EN, 1);

	return ret;
}

int mtk_cam_sv_fbc_disable(struct mtk_camsv_device *dev, unsigned int tag_idx)
{
	int ret = 0;

	CAMSV_WRITE_REG(dev->base + REG_CAMSVCENTRAL_FBC0_TAG1 +
		CAMSVCENTRAL_FBC0_TAG_SHIFT * tag_idx, 0);

	return ret;
}

int mtk_cam_sv_is_vf_on(struct mtk_camsv_device *dev)
{
	if (CAMSV_READ_BITS(dev->base + REG_CAMSVCENTRAL_VF_CON,
			CAMSVCENTRAL_VF_CON, VFDATA_EN) == 1)
		return 1;
	else
		return 0;
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

	/* camsv todo: log level */
	dev_info(dev->dev, "[%s] iova:0x%x, seq:%d\n", __func__, ba, seq_no);

	return ret;
}

int mtk_cam_get_sv_pixel_mode(struct mtk_cam_ctx *ctx, unsigned int idx)
{
	struct v4l2_format *img_fmt;

	img_fmt = &ctx->sv_pipe[idx]
		->vdev_nodes[MTK_CAMSV_MAIN_STREAM_OUT-MTK_CAMSV_SINK_NUM].active_fmt;

	return (mtk_camsv_is_yuv_format(img_fmt->fmt.pix_mp.pixelformat)) ? 0 : 3;
}

int mtk_cam_sv_write_rcnt(
	struct mtk_camsv_device *camsv_dev,
	unsigned int tag)
{
	int ret = 0;

	CAMSV_WRITE_BITS(camsv_dev->base + REG_CAMSVCENTRAL_FBC0_TAG1 +
		CAMSVCENTRAL_FBC0_TAG_SHIFT * tag,
		CAMSVCENTRAL_FBC0_TAG1, RCNT_INC_TAG1, 1);

	return ret;
}

bool mtk_cam_sv_is_zero_fbc_cnt(struct mtk_cam_ctx *ctx,
	unsigned int pipe_id)
{
	bool result = false;
	struct mtk_camsv_device *camsv_dev;
	unsigned int tag;

	camsv_dev = mtk_cam_get_used_sv_dev(ctx);
	tag = mtk_cam_get_sv_tag(ctx, pipe_id);

	if (CAMSV_READ_BITS(camsv_dev->base +
			REG_CAMSVCENTRAL_FBC1_TAG1 + CAMSVCENTRAL_FBC1_TAG_SHIFT * tag,
			CAMSVCENTRAL_FBC1_TAG1, FBC_CNT_TAG1) == 0)
		result = true;

	return result;
}

int mtk_cam_sv_cal_cfg_info(struct mtk_cam_ctx *ctx,
	const struct v4l2_format *img_fmt,
	struct mtk_camsv_frame_params *params)
{
	int ret = 0;

	/* camsv todo */

	return ret;
}

int mtk_cam_sv_setup_cfg_info(struct mtk_camsv_device *dev,
	struct mtk_cam_request_stream_data *s_data,
	unsigned int tag)
{
	int ret = 0;

	/* camsv todo */

	return ret;
}

int mtk_cam_sv_frame_no_inner(struct mtk_camsv_device *dev)
{
	return readl_relaxed(dev->base_inner + REG_CAMSVCENTRAL_FRAME_SEQ_NO);
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

int mtk_cam_sv_apply_all_buffers(struct mtk_cam_ctx *ctx, bool is_check_ts)
{
	unsigned int seq_no;
	dma_addr_t base_addr;
	struct mtk_camsv_working_buf_entry *buf_entry, *buf_entry_prev;
	struct mtk_camsv_device *camsv_dev;
	int i;
	unsigned int tag_idx;
	for (i = 0; i < ctx->used_sv_num; i++) {
		camsv_dev = mtk_cam_get_used_sv_dev(ctx);

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
			(ctx->used_raw_num != 0) && is_check_ts) {
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
				tag_idx = mtk_cam_get_sv_tag(ctx, ctx->sv_pipe[i]->id);
				mtk_cam_sv_enquehwbuf(camsv_dev, base_addr, seq_no, tag_idx);
				/* initial request readout will be delayed 1 frame */
				if (ctx->used_raw_num && !mtk_cam_is_subsample(ctx) &&
					!mtk_cam_is_stagger(ctx) && !mtk_cam_is_stagger_m2m(ctx) &&
					!mtk_cam_is_time_shared(ctx) && !mtk_cam_is_mstream(ctx))
					mtk_cam_sv_write_rcnt(camsv_dev, tag_idx);
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
	unsigned int seq_no, tag_idx;
	dma_addr_t base_addr;
	struct mtk_camsv_working_buf_entry *buf_entry;
	struct mtk_camsv_device *camsv_dev;
	struct mtk_cam_request_stream_data *s_data;
	int i;

	for (i = 0; i < ctx->used_sv_num; i++) {
		if (ctx->sv_pipe[i]->id == pipe_id) {
			camsv_dev = mtk_cam_get_used_sv_dev(ctx);

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
					tag_idx = mtk_cam_get_sv_tag(ctx, ctx->sv_pipe[i]->id);
					mtk_cam_sv_enquehwbuf(camsv_dev, base_addr, seq_no,
						tag_idx);
					/* initial request readout will be delayed 1 frame */
					if (ctx->used_raw_num && !mtk_cam_is_subsample(ctx) &&
						!mtk_cam_is_stagger(ctx) &&
						!mtk_cam_is_stagger_m2m(ctx) &&
						!mtk_cam_is_time_shared(ctx) &&
						!mtk_cam_is_mstream(ctx))
						mtk_cam_sv_write_rcnt(camsv_dev, tag_idx);
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
	unsigned int seq_no, tag_idx = 0;
	int i, ret = 0;

	/* camsv todo */
	for (i = MTKCAM_SUBDEV_CAMSV_START; i < MTKCAM_SUBDEV_CAMSV_END; i++) {
		if (ctx->pipe->enabled_raw & (1 << i)) {
			camsv_dev = mtk_cam_get_used_sv_dev(ctx);
			base_addr = s_data->sv_frame_params.img_out.buf[0][0].iova;
			seq_no = s_data->frame_seq_no;
			mtk_cam_sv_enquehwbuf(camsv_dev, base_addr, seq_no, tag_idx);
			ret = 1;
		}
	}

	return ret;
}

int mtk_cam_sv_apply_switch_buffers(struct mtk_cam_ctx *ctx)
{
	unsigned int seq_no, tag_idx = 0;
	dma_addr_t base_addr;
	struct mtk_camsv_working_buf_entry *buf_entry;
	struct mtk_camsv_device *camsv_dev;
	int i;

	/* camsv todo */
	for (i = 0; i < ctx->used_sv_num; i++) {
		camsv_dev = mtk_cam_get_used_sv_dev(ctx);

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
			tag_idx = mtk_cam_get_sv_tag(ctx, ctx->sv_pipe[i]->id);
			mtk_cam_sv_setup_cfg_info(camsv_dev, buf_entry->s_data, tag_idx);
			mtk_cam_sv_enquehwbuf(camsv_dev, base_addr, seq_no, tag_idx);
		}
	}

	return 1;
}

int mtk_cam_sv_set_group_info(struct mtk_camsv_device *camsv_dev)
{
	int i;

	/* reset group info */
	for (i = 0; i < MAX_SV_HW_GROUPS; i++)
		camsv_dev->group_info[i] = 0;

	/* set groups */
	for (i = SVTAG_START; i < SVTAG_END; i++) {
		if (camsv_dev->enabled_tags & (1 << i)) {
			if (camsv_dev->tag_info[i].exp_order == MTK_CAM_EXP_FIRST)
				camsv_dev->group_info[0] |= (1 << i);
			else if (camsv_dev->tag_info[i].exp_order == MTK_CAM_EXP_SECOND)
				camsv_dev->group_info[1] |= (1 << i);
			else if (camsv_dev->tag_info[i].exp_order == MTK_CAM_EXP_LAST)
				camsv_dev->group_info[2] |= (1 << i);
			else
				dev_info(camsv_dev->dev, "tag[%d]: illegal exposure order(%d)",
					__func__, i, camsv_dev->tag_info[i].exp_order);
		}
	}

	return 0;
}

unsigned int mtk_cam_get_sv_tag(struct mtk_cam_ctx *ctx,
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

int mtk_cam_sv_dev_config(struct mtk_camsv_device *camsv_dev)
{
	unsigned int i;
	unsigned int is_config_done;
	unsigned int tag_idx = 0;
	unsigned int sub_ratio = 0;

	if (camsv_dev->used_tag_cnt == 0)
		return 0;

	camsv_dev->sof_count = 0;

	mtk_cam_sv_set_group_info(camsv_dev);
	mtk_cam_sv_tg_disable(camsv_dev);
	reset_msgfifo(camsv_dev);
	mtk_camsv_register_iommu_tf_callback(camsv_dev);

	/* tags */
	for (i = SVTAG_START; i < SVTAG_END; i++) {
		if (camsv_dev->enabled_tags & (1 << i)) {
			tag_idx = i;
			is_config_done = atomic_read(&camsv_dev->tag_info[i].is_config_done);
			sub_ratio = camsv_dev->tag_info[i].cfg_in_param.subsample;

			if (!is_config_done) {
				mtk_cam_sv_central_pertag_config(camsv_dev, tag_idx);
				mtk_cam_sv_dmao_pertag_config(camsv_dev, tag_idx);
				mtk_cam_sv_fbc_pertag_config(camsv_dev, tag_idx);
				mtk_cam_sv_fbc_pertag_enable(camsv_dev, tag_idx);
				atomic_set(&camsv_dev->tag_info[i].is_config_done, 1);
			}

			dev_info(camsv_dev->dev, "%s done: tag(%d)\n", camsv_dev->id, __func__, i);
		}
	}

	/* common */
	mtk_cam_sv_central_common_config(camsv_dev, sub_ratio);
	mtk_cam_sv_dmao_common_config(camsv_dev);
	mtk_cam_sv_central_enable(camsv_dev);

	return 0;
}

int mtk_cam_call_sv_dev_config(
	struct mtk_cam_ctx *ctx,
	unsigned int tag_idx,
	unsigned int seninf_padidx,
	unsigned int hw_scen,
	unsigned int exp_order,
	unsigned int pixelmode,
	struct mtk_camsv_pipeline *pipeline)
{
	struct mtk_cam_device *cam;
	struct mtk_camsv_tag_info *tag_info;
	struct mtk_camsv_device *camsv_dev;
	struct mtkcam_ipi_input_param *cfg_in_param;
	struct v4l2_mbus_framefmt *mf;
	struct v4l2_format *img_fmt;
	unsigned int bbp;

	cam = ctx->cam;
	camsv_dev = ctx->sv_dev;
	tag_info = &camsv_dev->tag_info[tag_idx];
	cfg_in_param = &tag_info->cfg_in_param;

	camsv_dev->used_tag_cnt++;
	camsv_dev->enabled_tags |= (1 << tag_idx);

	if (tag_idx >= SVTAG_META_START && tag_idx < SVTAG_META_END)
		tag_info->sv_pipe = pipeline;
	else
		tag_info->sv_pipe = NULL;

	tag_info->seninf_padidx = seninf_padidx;
	tag_info->hw_scen = hw_scen;
	tag_info->exp_order = exp_order;
	tag_info->cfg_in_param.pixel_mode = pixelmode;
	atomic_set(&tag_info->is_config_done, 0);
	atomic_set(&tag_info->is_stream_on, 0);

	dev_info(camsv_dev->dev, "update tag_info: tag_idx(%d) hw_scen(0x%x) exp_order(%d) pixelmode(%d)\n",
		tag_idx, hw_scen, exp_order, pixelmode);

	if (hw_scen & MTK_CAMSV_SUPPORTED_SPECIAL_HW_SCENARIO) {
		if (hw_scen & (1 << MTKCAM_SV_SPECIAL_SCENARIO_ADDITIONAL_RAW)) {
			img_fmt = &ctx->pipe->vdev_nodes[
				MTK_RAW_MAIN_STREAM_SV_1_OUT - MTK_RAW_SINK_NUM].active_fmt;
			mf = &ctx->pipe->cfg[MTK_RAW_SINK].mbus_fmt;
		} else if (hw_scen & (1 << MTKCAM_IPI_HW_PATH_DC_STAGGER)) {
			img_fmt = &ctx->pipe->img_fmt_sink_pad;
			mf = &ctx->pipe->cfg[MTK_RAW_SINK].mbus_fmt;
#ifdef CAMSV_TODO
		// camsv todo: preisp
		} else if (hw_scen & (1 << MTKCAM_SV_SPECIAL_SCENARIO_EXT_ISP)) {
			img_fmt = &ctx->pipe->vdev_nodes[
				cam->sv.pipelines[pipe_id].raw_vdevidx -
				MTK_RAW_SINK_NUM].active_fmt;
			if (seninf_padidx == PAD_SRC_GENERAL0) {
				img_fmt->fmt.pix_mp.plane_fmt[0].bytesperline =
					img_fmt->fmt.pix_mp.width;
				img_fmt->fmt.pix_mp.pixelformat = V4L2_PIX_FMT_SBGGR8;
			}
			if (mtk_cam_is_ext_isp_yuv(ctx) &&
				seninf_padidx == PAD_SRC_RAW_EXT0) {
				img_fmt->fmt.pix_mp.pixelformat = V4L2_PIX_FMT_MTISP_SRGGB10;
			}
			dev_info(camsv_dev->dev, "[%s:ext-isp] idx/vidx,
				w/h/pf:%d/%d, %d/%d/0x%x\n",
				__func__, pipe_id, cam->sv.pipelines[pipe_id].raw_vdevidx,
				img_fmt->fmt.pix_mp.width, img_fmt->fmt.pix_mp.height,
				img_fmt->fmt.pix_mp.pixelformat);
			mf = &ctx->pipe->cfg[MTK_RAW_SINK].mbus_fmt;
#endif
		} else {
			img_fmt = &ctx->pipe->vdev_nodes[MTK_RAW_MAIN_STREAM_OUT - MTK_RAW_SINK_NUM]
				.active_fmt;
			mf = &ctx->pipe->cfg[MTK_RAW_SINK].mbus_fmt;
		}
	} else {
		img_fmt = &tag_info->sv_pipe
			->vdev_nodes[MTK_CAMSV_MAIN_STREAM_OUT-MTK_CAMSV_SINK_NUM].active_fmt;
		mf = &tag_info->sv_pipe->cfg[MTK_CAMSV_SINK].mbus_fmt;
	}

	/* update cfg_in_param */
	cfg_in_param->pixel_mode = pixelmode;
	cfg_in_param->data_pattern = 0x0;
	cfg_in_param->in_crop.p.x = 0x0;
	cfg_in_param->in_crop.p.y = 0x0;
	cfg_in_param->in_crop.s.w =
		(mtk_cam_is_ext_isp_yuv(ctx) &&
			seninf_padidx == PAD_SRC_RAW_EXT0) ?
		img_fmt->fmt.pix_mp.width * 2 :
		img_fmt->fmt.pix_mp.width;
	cfg_in_param->in_crop.s.h = img_fmt->fmt.pix_mp.height;
	cfg_in_param->fmt = img_fmt->fmt.pix_mp.pixelformat;
	cfg_in_param->raw_pixel_id = mtk_cam_get_sensor_pixel_id(mf->code);
	cfg_in_param->subsample = 0; /* camsv todo: smvr */

	bbp = mtk_cam_sv_xsize_cal(cfg_in_param);
	if (bbp < img_fmt->fmt.pix_mp.plane_fmt[0].bytesperline)
		tag_info->stride = img_fmt->fmt.pix_mp.plane_fmt[0].bytesperline;
	else
		tag_info->stride = bbp;

	dev_info(camsv_dev->dev,
		"sink pad code:0x%x camsv's imgo w/h/stride:%d/%d/%d feature:%d\n",
		mf->code, cfg_in_param->in_crop.s.w, cfg_in_param->in_crop.s.h,
		tag_info->stride, (ctx->used_raw_num) ? ctx->pipe->feature_active : 0);

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
		i = 0;

	dev_sv = cam->sv.devs[i];
	if (dev_sv == NULL) {
		dev_dbg(cam->dev, "stream on camsv device not found\n");
		return NULL;
	}
	camsv_dev = dev_get_drvdata(dev_sv);
	camsv_dev->ctx_stream_id = ctx->stream_id;

	return camsv_dev;
}

int mtk_cam_get_sv_cammux_id(struct mtk_camsv_device *camsv_dev, int tags)
{
	int cammux_id;

	switch (camsv_dev->id) {
	case CAMSV_0:
	case CAMSV_1:
	case CAMSV_2:
	case CAMSV_3:
		cammux_id = camsv_dev->cammux_id + tags;
		break;
	case CAMSV_4:
	case CAMSV_5:
		cammux_id = camsv_dev->cammux_id;
		break;
	}

	return cammux_id;
}

int mtk_cam_sv_dev_stream_on(
	struct mtk_cam_ctx *ctx,
	unsigned int tag_idx,
	unsigned int streaming)
{
	struct mtk_camsv_device *camsv_dev = ctx->sv_dev;
	int ret = 0;

	if (streaming) {
		camsv_dev->streaming_cnt++;
		ret = mtk_cam_sv_print_fbc_status(camsv_dev, tag_idx);
		if (camsv_dev->streaming_cnt == camsv_dev->used_tag_cnt)
			ret |= mtk_cam_sv_central_common_enable(camsv_dev);
	}
	else {
		if (camsv_dev->streaming_cnt == camsv_dev->used_tag_cnt) {
			ret |= mtk_cam_sv_central_common_disable(camsv_dev);
			ret |= mtk_cam_sv_tg_disable(camsv_dev);
		}

		ret |= mtk_cam_sv_fbc_disable(camsv_dev, tag_idx);
		camsv_dev->streaming_cnt--;
	}

	dev_info(ctx->cam->dev, "camsv %d %s en(%d)\n", camsv_dev->id, __func__, streaming);

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
	unsigned int i;
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

#ifdef CAMSV_TODO
/* camsv todo: dma debug data */
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
#endif

void camsv_irq_handle_err(
	struct mtk_camsv_device *camsv_dev,
	unsigned int dequeued_frame_seq_no, unsigned int tag)
{
	int val;
	struct mtk_cam_request_stream_data *s_data;
	struct mtk_cam_device *cam = camsv_dev->cam;
	struct mtk_cam_ctx *ctx;
	struct mtk_raw_pipeline *raw_pipe;
	struct mtk_camsv_tag_info tag_info = camsv_dev->tag_info[tag];
	unsigned int stream_id;

	wmb(); /* TBC */
	val = readl_relaxed(camsv_dev->base + REG_CAMSVCENTRAL_SEN_MODE);
	val = val | REG_CAMSVCENTRAL_SEN_MODE_CMOS_RDY_SEL;
	writel_relaxed(val, camsv_dev->base + REG_CAMSVCENTRAL_SEN_MODE);
	wmb(); /* TBC */
	dev_info_ratelimited(camsv_dev->dev,
		"TG PATHCFG/SENMODE/FRMSIZE/RGRABPXL/LIN:%x/%x/%x/%x\n",
		readl_relaxed(camsv_dev->base + REG_CAMSVCENTRAL_PATH_CFG),
		readl_relaxed(camsv_dev->base + REG_CAMSVCENTRAL_SEN_MODE),
		readl_relaxed(camsv_dev->base + REG_CAMSVCENTRAL_GRAB_PXL_TAG1 +
			CAMSVCENTRAL_GRAB_PXL_TAG_SHIFT * tag),
		readl_relaxed(camsv_dev->base + REG_CAMSVCENTRAL_GRAB_LIN_TAG1 +
			CAMSVCENTRAL_GRAB_LIN_TAG_SHIFT * tag));

	if (camsv_dev->tag_info[tag].hw_scen &
		MTK_CAMSV_SUPPORTED_SPECIAL_HW_SCENARIO) {
		if (camsv_dev->ctx_stream_id < MTKCAM_SUBDEV_RAW_START ||
			camsv_dev->ctx_stream_id >= MTKCAM_SUBDEV_CAMSV_END) {
			dev_info(camsv_dev->dev, "stream id out of range : %d",
					camsv_dev->ctx_stream_id);
			return;
		}
		ctx = &cam->ctxs[camsv_dev->ctx_stream_id];
		raw_pipe = &cam->raw
			.pipelines[camsv_dev->ctx_stream_id];
		if (!ctx) {
			dev_info(camsv_dev->dev, "%s: cannot find ctx\n", __func__);
			return;
		}
		stream_id = camsv_dev->ctx_stream_id;
	} else {
		if (camsv_dev->ctx_stream_id < MTKCAM_SUBDEV_RAW_START ||
		camsv_dev->ctx_stream_id >= MTKCAM_SUBDEV_CAMSV_END) {
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
			dev_info(camsv_dev->dev, "%s: this tag: %d is not control by user\n",
				__func__, tag);
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

	dev_info_ratelimited(camsv_dev->dev, "error status:0x%x", err_status);

	/* camsv todo: show more error detail */
	for (tag_idx = 0; tag_idx < MAX_SV_HW_TAGS; tag_idx++) {
		if (!(data->err_tags & (1 << tag_idx)))
			continue;
		camsv_irq_handle_err(camsv_dev, frame_idx_inner, tag_idx);
	}

#ifdef CAMSV_TODO
	/* camsv todo: dma debug data */
	/* dump dma debug data */
	camsv_dump_dma_debug_data(camsv_dev);
#endif
}

static irqreturn_t mtk_irq_camsv_done(int irq, void *data)
{
	struct mtk_camsv_device *camsv_dev = (struct mtk_camsv_device *)data;
	struct mtk_camsys_irq_info irq_info;
	unsigned int dequeued_imgo_seq_no, dequeued_imgo_seq_no_inner;
	unsigned int irq_done_status;
	bool wake_thread = 0;


	dequeued_imgo_seq_no =
		readl_relaxed(camsv_dev->base + REG_CAMSVCENTRAL_FRAME_SEQ_NO);
	dequeued_imgo_seq_no_inner =
		readl_relaxed(camsv_dev->base_inner + REG_CAMSVCENTRAL_FRAME_SEQ_NO);

	dev_info(camsv_dev->dev, "done %d\n", dequeued_imgo_seq_no_inner);

	irq_info.irq_type = 0;
	irq_info.ts_ns = ktime_get_boottime_ns();
	irq_info.frame_idx = dequeued_imgo_seq_no;
	irq_info.frame_idx_inner = dequeued_imgo_seq_no_inner;

	irq_done_status	=
		readl_relaxed(camsv_dev->base + REG_CAMSVCENTRAL_DONE_STATUS);
	dev_info(camsv_dev->dev, "[test]done %d, status 0x%x\n",
		dequeued_imgo_seq_no_inner, irq_done_status);
	if (irq_done_status & CAMSVCENTRAL_GP_PASS1_DONE_0_ST)
		irq_info.done_groups |= (1<<0);
	if (irq_done_status & CAMSVCENTRAL_GP_PASS1_DONE_1_ST)
		irq_info.done_groups |= (1<<1);
	if (irq_done_status & CAMSVCENTRAL_GP_PASS1_DONE_2_ST)
		irq_info.done_groups |= (1<<2);
	if (irq_done_status & CAMSVCENTRAL_GP_PASS1_DONE_3_ST)
		irq_info.done_groups |= (1<<3);
	irq_info.irq_type |= (1 << CAMSYS_IRQ_FRAME_DONE);

	if (irq_info.irq_type && push_msgfifo(camsv_dev, &irq_info) == 0)
		wake_thread = 1;

	return wake_thread ? IRQ_WAKE_THREAD : IRQ_HANDLED;

}

static irqreturn_t mtk_irq_camsv_sof(int irq, void *data)
{
	struct mtk_camsv_device *camsv_dev = (struct mtk_camsv_device *)data;
	struct mtk_camsys_irq_info irq_info;
	unsigned int dequeued_imgo_seq_no, dequeued_imgo_seq_no_inner;
	unsigned int irq_sof_status;

	bool wake_thread = 0;

	dequeued_imgo_seq_no =
		readl_relaxed(camsv_dev->base + REG_CAMSVCENTRAL_FRAME_SEQ_NO);
	dequeued_imgo_seq_no_inner =
		readl_relaxed(camsv_dev->base_inner + REG_CAMSVCENTRAL_FRAME_SEQ_NO);

	irq_info.irq_type = 0;
	irq_info.ts_ns = ktime_get_boottime_ns();
	irq_info.frame_idx = dequeued_imgo_seq_no;
	irq_info.frame_idx_inner = dequeued_imgo_seq_no_inner;

	irq_sof_status	=
		readl_relaxed(camsv_dev->base + REG_CAMSVCENTRAL_SOF_STATUS);
	dev_info(camsv_dev->dev, "[test]sof %d, status 0x%x\n",
		dequeued_imgo_seq_no_inner, irq_sof_status);
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
	camsv_dev->sof_timestamp = ktime_get_boottime_ns();

	if (irq_info.irq_type && push_msgfifo(camsv_dev, &irq_info) == 0)
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
	err_status = readl_relaxed(camsv_dev->base_inner +
								REG_CAMSVCENTRAL_ERR_STATUS);

	dev_info(camsv_dev->dev, "[test]done %d, status 0x%x\n",
		dequeued_imgo_seq_no_inner, err_status);

	irq_info.irq_type = 0;
	irq_info.ts_ns = ktime_get_boottime_ns();
	irq_info.frame_idx = dequeued_imgo_seq_no;
	irq_info.frame_idx_inner = dequeued_imgo_seq_no_inner;
	if (err_status) {
		struct mtk_camsys_irq_info err_info;
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
	unsigned int irq_cq_status;

	bool wake_thread = 0;

	dequeued_imgo_seq_no =
		readl_relaxed(camsv_dev->base + REG_CAMSVCENTRAL_FRAME_SEQ_NO);
	dequeued_imgo_seq_no_inner =
		readl_relaxed(camsv_dev->base_inner + REG_CAMSVCENTRAL_FRAME_SEQ_NO);

	irq_cq_status =
		readl_relaxed(camsv_dev->base_inner + REG_CAMSVCQTOP_INT_0_STATUS);
	irq_info.irq_type = 0;
	irq_info.ts_ns = ktime_get_boottime_ns();
	irq_info.frame_idx = dequeued_imgo_seq_no;
	irq_info.frame_idx_inner = dequeued_imgo_seq_no_inner;

	if (irq_cq_status & CAMSVCQTOP_SCQ_SUB_THR_DONE)
		irq_info.irq_type |= (1 << CAMSYS_IRQ_SETTING_DONE);

	if (irq_info.irq_type && push_msgfifo(camsv_dev, &irq_info) == 0)
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
	writel(val & (~REG_CAMSVCENTRAL_SEN_MODE_CMOS_EN),
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
	writel(val | REG_CAMSVCENTRAL_SEN_MODE_CMOS_EN,
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
	// camsv todo: change to dbg
	dev_info(dev, "camsv, map_addr=0x%pK\n", sv->base);

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
	/* camsv todo: log level */
	dev_info(dev, "camsv, map_addr=0x%pK\n", sv->base_inner);

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
		/* camsv todo: log level */
		dev_info(dev, "registered irq=%d\n", sv->irq[i]);

		disable_irq(sv->irq[i]);
		/* camsv todo: log level */
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


#ifdef CAMSV_TODO
	/* camsv todo: clk control */
	for (i = 0; i < camsv_dev->num_clks; i++)
		clk_disable_unprepare(camsv_dev->clks[i]);
#endif

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

	for (i = 0; i < CAMSV_IRQ_NUM; i++) {
		enable_irq(camsv_dev->irq[i]);
		// camsv todo: remove info
		dev_info(dev, "%s:enable irq %d\n", __func__, camsv_dev->irq[i]);
	}


#ifdef CAMSV_TODO
	/* camsv todo: clk control */
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
#endif
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

