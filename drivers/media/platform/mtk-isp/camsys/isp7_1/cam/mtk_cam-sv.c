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
#include <linux/jiffies.h>

#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-subdev.h>

#include "mtk_cam.h"
#include "mtk_cam_pm.h"
#include "mtk_cam-pool.h"
#include "mtk_cam-sv-regs.h"
#include "mtk_cam-sv.h"
#include "mtk_cam-v4l2.h"

#ifdef CONFIG_MTK_SMI_EXT
#include "smi_public.h"
#endif

#ifdef CONFIG_MTK_IOMMU_V2
#include "mtk_iommu_ext.h"
#include "mach/pseudo_m4u.h"
#endif

#define MTK_CAMSV_STOP_HW_TIMEOUT			(33 * USEC_PER_MSEC)

static const struct of_device_id mtk_camsv_of_ids[] = {
	{.compatible = "mediatek,mt8195-camsv",},
	{}
};
MODULE_DEVICE_TABLE(of, mtk_camsv_of_ids);

#define MAX_NUM_CAMSV_CLOCKS 6
#define LARB13_PORT_SIZE 12
#define LARB14_PORT_SIZE 6


struct sv_resource {
	char *clock[MAX_NUM_CAMSV_CLOCKS];
};

static const struct sv_resource sv_resources[] = {
	[CAMSV_0] = {
		.clock = {"camsys_larb13_cgpdn", "camsys_larb14_cgpdn",
			"camsys_gcamsva_cgpdn", "camsys_camsv_top_cgpdn",
			"camsys_camsv_cq_cgpdn", "topckgen_top_muxcamtm"},
	},
	[CAMSV_1] = {
		.clock = {"camsys_larb13_cgpdn", "camsys_larb14_cgpdn",
			"camsys_gcamsva_cgpdn", "camsys_camsv_top_cgpdn",
			"camsys_camsv_cq_cgpdn", "topckgen_top_muxcamtm"},
	},
	[CAMSV_2] = {
		.clock = {"camsys_larb13_cgpdn", "camsys_larb14_cgpdn",
			"camsys_gcamsvb_cgpdn", "camsys_camsv_top_cgpdn",
			"camsys_camsv_cq_cgpdn", "topckgen_top_muxcamtm"},
	},
	[CAMSV_3] = {
		.clock = {"camsys_larb13_cgpdn", "camsys_larb14_cgpdn",
			"camsys_gcamsvb_cgpdn", "camsys_camsv_top_cgpdn",
			"camsys_camsv_cq_cgpdn", "topckgen_top_muxcamtm"},
	},
	[CAMSV_4] = {
		.clock = {"camsys_larb13_cgpdn", "camsys_larb14_cgpdn",
			"camsys_gcamsvc_cgpdn", "camsys_camsv_top_cgpdn",
			"camsys_camsv_cq_cgpdn", "topckgen_top_muxcamtm"},
	},
	[CAMSV_5] = {
		.clock = {"camsys_larb13_cgpdn", "camsys_larb14_cgpdn",
			"camsys_gcamsvc_cgpdn", "camsys_camsv_top_cgpdn",
			"camsys_camsv_cq_cgpdn", "topckgen_top_muxcamtm"},
	},
};

#ifdef CONFIG_MTK_SMI_EXT
static const char *larb13_port_name[LARB13_PORT_SIZE] = {
	"mrawi_mdp",
	"mrawo0_mdp",
	"mrawo1_mdp",
	"camsv1_mdp",
	"camsv2_mdp",
	"camsv3_mdp",
	"camsv4_mdp",
	"camsv5_mdp",
	"camsv6_mdp",
	"ccui_mdp",
	"ccuo_mdp",
	"fake_mdp"
};

static const char *larb14_port_name[LARB14_PORT_SIZE] = {
	"mrawi_disp",
	"mrawo0_disp",
	"mrawo1_disp",
	"camsv0_disp",
	"ccui_disp",
	"ccuo_disp"
};
#endif

static const int larb13_support_port_map[LARB13_PORT_SIZE] = {
	false,	 /* MRAWI */
	false,	 /* MRAWO0 */
	false,	 /* MRAWO1 */
	true,	 /* CAMSV1 */
	true,	 /* CAMSV2 */
	true,	 /* CAMSV3 */
	true,	 /* CAMSV4 */
	true,	 /* CAMSV5 */
	true,	 /* CAMSV6 */
	false,	 /* CCUI */
	false,	 /* CCUO */
	false,	 /* FAKE */
};

static const int larb14_support_port_map[LARB14_PORT_SIZE] = {
	false,	 /* MRAWI */
	false,	 /* MRAWO0 */
	false,	 /* MRAWO1 */
	true,	 /* CAMSV0 */
	false,	 /* CCUI */
	false,	 /* CCUO */
};

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
				node->desc.fmts[i].fmt.pix_mp.pixelformat);
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

static int mtk_camsv_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *fmt)
{
	struct mtk_camsv_pipeline *pipe =
		container_of(sd, struct mtk_camsv_pipeline, subdev);
	struct mtk_camsv *sv = pipe->sv;
	struct v4l2_mbus_framefmt *mf;

	if (!sd || !cfg) {
		dev_dbg(sv->cam_dev, "%s: Required sd(%p), cfg(%p)\n",
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
	.link_validate = v4l2_subdev_link_validate_default,
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

static const struct v4l2_format sv_stream_out_fmts[] = {
	/* This is a default image format */
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SBGGR10,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SBGGR8,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SBGGR12,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SBGGR14,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGBRG8,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGBRG10,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGBRG12,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGBRG14,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SGRBG8,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGRBG10,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGRBG12,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SGRBG14,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_SRGGB8,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SRGGB10,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SRGGB12,
		},
	},
	{
		.fmt.pix_mp = {
			.width = IMG_MAX_WIDTH,
			.height = IMG_MAX_HEIGHT,
			.pixelformat = V4L2_PIX_FMT_MTISP_SRGGB14,
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
		.link_flags = 0,
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
				.max_width = IMG_MAX_WIDTH,
				.min_width = IMG_MIN_WIDTH,
				.max_height = IMG_MAX_HEIGHT,
				.min_height = IMG_MIN_HEIGHT,
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
	{"mtk-cam camsv-5 main-stream"}
};

void sv_reset(struct mtk_camsv_device *dev)
{
	unsigned long end = jiffies + msecs_to_jiffies(100);

	dev_dbg(dev->dev, "%s\n", __func__);

	writel_relaxed(0, dev->base + REG_CAMSV_SW_CTL);
	writel_relaxed(1, dev->base + REG_CAMSV_SW_CTL);
	wmb(); /* TBC */

	while (time_before(jiffies, end)) {
		if (readl(dev->base + REG_CAMSV_SW_CTL) & 0x2) {
			// do hw rst
			writel_relaxed(4, dev->base + REG_CAMSV_SW_CTL);
			writel_relaxed(0, dev->base + REG_CAMSV_SW_CTL);
			wmb(); /* TBC */
			return;
		}

		dev_info(dev->dev,
			"tg_sen_mode: 0x%x, ctl_en: 0x%x, ctl_sw_ctl:0x%x, frame_no:0x%x\n",
			readl(dev->base + REG_CAMSV_TG_SEN_MODE),
			readl(dev->base + REG_CAMSV_MODULE_EN),
			readl(dev->base + REG_CAMSV_SW_CTL),
			readl(dev->base + REG_CAMSV_FRAME_SEQ_NO)
			);
		usleep_range(10, 20);
	}

	dev_dbg(dev->dev, "reset hw timeout\n");
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
				pm_runtime_put(sv->devs[i]);
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
				pm_runtime_put(sv->devs[i]);
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

unsigned int mtk_cam_sv_xsize_cal(struct mtkcam_ipi_input_param *cfg_in_param)
{

	union CAMSV_FMT_SEL fmt;
	unsigned int size;
	unsigned int divisor;

	fmt.Raw = cfg_in_param->fmt;

	switch (fmt.Bits.TG1_FMT) {
	case SV_TG_FMT_RAW8:
		size = cfg_in_param->in_crop.s.w;
		break;
	case SV_TG_FMT_RAW10:
		size = (cfg_in_param->in_crop.s.w * 10) / 8;
		break;
	case SV_TG_FMT_RAW12:
		size = (cfg_in_param->in_crop.s.w * 12) / 8;
		break;
	case SV_TG_FMT_RAW14:
		size = (cfg_in_param->in_crop.s.w * 14) / 8;
		break;
	default:
		return 0;
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
		(1 << MTKCAM_IPI_HW_PATH_ON_THE_FLY_DCIF_STAGGER)) {
		if (dev->id == 0) {
			CAMSV_WRITE_BITS(dev->base + REG_CAMSV_TG_SEN_MODE,
				CAMSV_TG_SEN_MODE, STAGGER_EN, 0);
			CAMSV_WRITE_BITS(dev->base + REG_CAMSV_TG_PATH_CFG,
				CAMSV_TG_PATH_CFG, SUB_SOF_SRC_SEL, 0);
		} else if (dev->id == 1) {
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
	pxl = ((cfg_in_param->in_crop.s.w+cfg_in_param->in_crop.p.x)<<16)|
			cfg_in_param->in_crop.p.x;
	lin = ((cfg_in_param->in_crop.s.h+cfg_in_param->in_crop.p.y)<<16)|
			cfg_in_param->in_crop.p.y;
	CAMSV_WRITE_REG(dev->base + REG_CAMSV_TG_SEN_GRAB_PXL, pxl);
	CAMSV_WRITE_REG(dev->base + REG_CAMSV_TG_SEN_GRAB_LIN, lin);

	dev_info(dev->dev, "pixel mode:%d\n", cfg_in_param->pixel_mode);
	dev_info(dev->dev, "sub-sample:%d\n", cfg_in_param->subsample);
	dev_info(dev->dev, "fmt:%d\n", cfg_in_param->fmt);
	dev_info(dev->dev, "crop_x:%d\n", cfg_in_param->in_crop.p.x);
	dev_info(dev->dev, "crop_y:%d\n", cfg_in_param->in_crop.p.y);
	dev_info(dev->dev, "crop_w:%d\n", cfg_in_param->in_crop.s.w);
	dev_info(dev->dev, "crop_h:%d\n", cfg_in_param->in_crop.s.h);

EXIT:
	return ret;
}

int mtk_cam_sv_top_config(
	struct mtk_camsv_device *dev,
	struct mtkcam_ipi_input_param *cfg_in_param)
{
	unsigned int int_en = (SV_INT_EN_VS1_INT_EN |
							SV_INT_EN_TG_ERR_INT_EN |
							SV_INT_EN_TG_GBERR_INT_EN |
							SV_INT_EN_TG_SOF_INT_EN |
							SV_INT_EN_PASS1_DON_INT_EN |
							SV_INT_EN_SW_PASS1_DON_INT_EN |
							SV_INT_EN_DMA_ERR_INT_EN |
							SV_INT_EN_IMGO_OVERR_INT_EN);
	union CAMSV_FMT_SEL fmt;
	int ret = 0;

	/* reset */
	sv_reset(dev);

	/* fun en */
	CAMSV_WRITE_BITS(dev->base + REG_CAMSV_MODULE_EN,
		CAMSV_MODULE_EN, TG_EN, 1);
	CAMSV_WRITE_BITS(dev->base + REG_CAMSV_MODULE_EN,
		CAMSV_MODULE_EN, DB_EN, 0);
	CAMSV_WRITE_BITS(dev->base + REG_CAMSV_MODULE_EN,
		CAMSV_MODULE_EN, DB_LOAD_SRC, SV_DB_SRC_SOF);
	if (dev->pipeline->hw_scen &
		(1 << MTKCAM_IPI_HW_PATH_ON_THE_FLY_DCIF_STAGGER)) {
		CAMSV_WRITE_BITS(dev->base + REG_CAMSV_MODULE_EN,
			CAMSV_MODULE_EN, DB_LOAD_SRC, SV_DB_SRC_SUB_SOF);
	} else {
		CAMSV_WRITE_BITS(dev->base + REG_CAMSV_MODULE_EN,
			CAMSV_MODULE_EN, DB_LOAD_SRC, SV_DB_SRC_SUB_SOF);
	}

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

	if (dev->pipeline->hw_scen &
		(1 << MTKCAM_IPI_HW_PATH_ON_THE_FLY_DCIF_STAGGER)) {
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
	CAMSV_WRITE_REG(dev->base + REG_CAMSV_FMT_SEL, cfg_in_param->fmt);

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

	fmt.Raw = cfg_in_param->fmt;
	switch (fmt.Bits.TG1_FMT) {
	case SV_TG_FMT_RAW8:
		CAMSV_WRITE_BITS(dev->base + REG_CAMSV_MODULE_EN,
			CAMSV_MODULE_EN, PAK_EN, 1);
		CAMSV_WRITE_BITS(dev->base + REG_CAMSV_MODULE_EN,
			CAMSV_MODULE_EN, PAK_SEL, 0);
		CAMSV_WRITE_BITS(dev->base + REG_CAMSV_PAK,
			CAMSV_PAK, PAK_MODE, 128);
		CAMSV_WRITE_BITS(dev->base + REG_CAMSV_PAK_CON,
			CAMSV_PAK_CON, PAK_IN_BIT, 14);
		break;
	case SV_TG_FMT_RAW10:
		CAMSV_WRITE_BITS(dev->base + REG_CAMSV_MODULE_EN,
			CAMSV_MODULE_EN, PAK_EN, 1);
		CAMSV_WRITE_BITS(dev->base + REG_CAMSV_MODULE_EN,
			CAMSV_MODULE_EN, PAK_SEL, 0);
		CAMSV_WRITE_BITS(dev->base + REG_CAMSV_PAK,
			CAMSV_PAK, PAK_MODE, 129);
		CAMSV_WRITE_BITS(dev->base + REG_CAMSV_PAK_CON,
			CAMSV_PAK_CON, PAK_IN_BIT, 14);
		break;
	case SV_TG_FMT_RAW12:
		CAMSV_WRITE_BITS(dev->base + REG_CAMSV_MODULE_EN,
			CAMSV_MODULE_EN, PAK_EN, 1);
		CAMSV_WRITE_BITS(dev->base + REG_CAMSV_MODULE_EN,
			CAMSV_MODULE_EN, PAK_SEL, 0);
		CAMSV_WRITE_BITS(dev->base + REG_CAMSV_PAK,
			CAMSV_PAK, PAK_MODE, 130);
		CAMSV_WRITE_BITS(dev->base + REG_CAMSV_PAK_CON,
			CAMSV_PAK_CON, PAK_IN_BIT, 14);
		break;
	default:
		dev_dbg(dev->dev, "unknown tg format(%d)", fmt.Bits.TG1_FMT);
		ret = -1;
		goto EXIT;
	}

	/* ufe disable */
	CAMSV_WRITE_BITS(dev->base + REG_CAMSV_MODULE_EN,
		CAMSV_MODULE_EN, UFE_EN, 0);

	/* pixel mode */
	switch (cfg_in_param->pixel_mode) {
	case 0:
		CAMSV_WRITE_BITS(dev->base + REG_CAMSV_PAK,
			CAMSV_PAK, PAK_DBL_MODE, 0);
		break;
	case 1:
		CAMSV_WRITE_BITS(dev->base + REG_CAMSV_PAK,
			CAMSV_PAK, PAK_DBL_MODE, 1);
		break;
	case 2:
		CAMSV_WRITE_BITS(dev->base + REG_CAMSV_PAK,
			CAMSV_PAK, PAK_DBL_MODE, 2);
		break;
	case 3:
		CAMSV_WRITE_BITS(dev->base + REG_CAMSV_PAK,
			CAMSV_PAK, PAK_DBL_MODE, 3);
		break;
	default:
		dev_dbg(dev->dev, "unknown pixel mode(%d)", cfg_in_param->pixel_mode);
		ret = -1;
		goto EXIT;
	}

	/* dma performance */
	CAMSV_WRITE_REG(dev->base + REG_CAMSV_SPECIAL_FUN_EN, 0x4000000);

EXIT:
	return ret;
}

int mtk_cam_sv_dmao_config(
	struct mtk_camsv_device *top_dev,
	struct mtk_camsv_device *sub_dev,
	struct mtkcam_ipi_input_param *cfg_in_param,
	int hw_scen, int raw_imgo_stride)
{
	int ret = 0;
	unsigned int stride;

	/* imgo dma setting */
	CAMSV_WRITE_REG(sub_dev->base + REG_CAMSV_IMGO_XSIZE,
		mtk_cam_sv_xsize_cal(cfg_in_param) - 1);
	CAMSV_WRITE_REG(sub_dev->base + REG_CAMSV_IMGO_YSIZE,
		cfg_in_param->in_crop.s.h - 1);
	CAMSV_WRITE_REG(sub_dev->base + REG_CAMSV_IMGO_STRIDE,
		mtk_cam_sv_xsize_cal(cfg_in_param));

	/* check raw's imgo stride */
	if (hw_scen & MTK_CAMSV_SUPPORTED_SPECIAL_HW_SCENARIO) {
		if (raw_imgo_stride > mtk_cam_sv_xsize_cal(cfg_in_param)) {
			dev_info(sub_dev->dev, "Special feature:0x%x, raw/sv stride = %d(THIS)/%d\n",
				hw_scen, raw_imgo_stride, mtk_cam_sv_xsize_cal(cfg_in_param));
			CAMSV_WRITE_REG(sub_dev->base + REG_CAMSV_IMGO_STRIDE,
				raw_imgo_stride);
		}
	}

	dev_info(sub_dev->dev, "xsize:%d\n",
		CAMSV_READ_REG(sub_dev->base + REG_CAMSV_IMGO_XSIZE));
	dev_info(sub_dev->dev, "ysize:%d\n",
		CAMSV_READ_REG(sub_dev->base + REG_CAMSV_IMGO_YSIZE));
	dev_info(sub_dev->dev, "stride:%d\n",
		CAMSV_READ_REG(sub_dev->base + REG_CAMSV_IMGO_STRIDE));

	/* imgo crop */
	CAMSV_WRITE_REG(sub_dev->base + REG_CAMSV_IMGO_CROP, 0);

	/* imgo stride */
	stride = CAMSV_READ_REG(sub_dev->base + REG_CAMSV_IMGO_STRIDE);
	switch (cfg_in_param->pixel_mode) {
	case 0:
		stride = stride | (1<<27) | (1<<16);
		CAMSV_WRITE_REG(sub_dev->base + REG_CAMSV_IMGO_STRIDE, stride);
		break;
	case 1:
		stride = stride | (1<<27) | (3<<16);
		CAMSV_WRITE_REG(sub_dev->base + REG_CAMSV_IMGO_STRIDE, stride);
		break;
	case 2:
		stride = stride | (1<<27) | (7<<16);
		CAMSV_WRITE_REG(sub_dev->base + REG_CAMSV_IMGO_STRIDE, stride);
		break;
	case 3:
		stride = stride | (1<<27) | (15<<16);
		CAMSV_WRITE_REG(sub_dev->base + REG_CAMSV_IMGO_STRIDE, stride);
		break;
	default:
		dev_dbg(sub_dev->dev, "unknown pixel mode(%d)", cfg_in_param->pixel_mode);
		ret = -1;
		goto EXIT;
	}

	/* imgo con */
	switch (top_dev->id) {
	case 0:
	case 1:
	case 2:
	case 3:
		CAMSV_WRITE_REG(top_dev->base + REG_CAMSV_IMGO_CON0, 0x80000300);
		CAMSV_WRITE_REG(top_dev->base + REG_CAMSV_IMGO_CON1, 0x00C00060);
		CAMSV_WRITE_REG(top_dev->base + REG_CAMSV_IMGO_CON2, 0x01800120);
		CAMSV_WRITE_REG(top_dev->base + REG_CAMSV_IMGO_CON3, 0x020001A0);
		CAMSV_WRITE_REG(top_dev->base + REG_CAMSV_IMGO_CON4, 0x012000C0);
		break;
	default:
		CAMSV_WRITE_REG(top_dev->base + REG_CAMSV_IMGO_CON0, 0x80000100);
		CAMSV_WRITE_REG(top_dev->base + REG_CAMSV_IMGO_CON1, 0x00400020);
		CAMSV_WRITE_REG(top_dev->base + REG_CAMSV_IMGO_CON2, 0x00800060);
		CAMSV_WRITE_REG(top_dev->base + REG_CAMSV_IMGO_CON3, 0x00AA0082);
		CAMSV_WRITE_REG(top_dev->base + REG_CAMSV_IMGO_CON4, 0x00600040);
		break;
	}
	switch (sub_dev->id) {
	case 0:
	case 1:
	case 2:
	case 3:
		CAMSV_WRITE_REG(sub_dev->base + REG_CAMSV_IMGO_CON0, 0x80000300);
		CAMSV_WRITE_REG(sub_dev->base + REG_CAMSV_IMGO_CON1, 0x00C00060);
		CAMSV_WRITE_REG(sub_dev->base + REG_CAMSV_IMGO_CON2, 0x01800120);
		CAMSV_WRITE_REG(sub_dev->base + REG_CAMSV_IMGO_CON3, 0x020001A0);
		CAMSV_WRITE_REG(sub_dev->base + REG_CAMSV_IMGO_CON4, 0x012000C0);
		break;
	default:
		CAMSV_WRITE_REG(sub_dev->base + REG_CAMSV_IMGO_CON0, 0x80000100);
		CAMSV_WRITE_REG(sub_dev->base + REG_CAMSV_IMGO_CON1, 0x00400020);
		CAMSV_WRITE_REG(sub_dev->base + REG_CAMSV_IMGO_CON2, 0x00800060);
		CAMSV_WRITE_REG(sub_dev->base + REG_CAMSV_IMGO_CON3, 0x00AA0082);
		CAMSV_WRITE_REG(sub_dev->base + REG_CAMSV_IMGO_CON4, 0x00600040);
		break;
	}

EXIT:
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

int mtk_cam_sv_top_enable(struct mtk_camsv_device *dev)
{
	int ret = 0;

	CAMSV_WRITE_BITS(dev->base + REG_CAMSV_CLK_EN,
		CAMSV_CLK_EN, TG_DP_CK_EN, 1);
	CAMSV_WRITE_BITS(dev->base + REG_CAMSV_CLK_EN,
		CAMSV_CLK_EN, PAK_DP_CK_EN, 1);
	CAMSV_WRITE_BITS(dev->base + REG_CAMSV_CLK_EN,
		CAMSV_CLK_EN, IMGO_DP_CK_EN, 1);

	CAMSV_WRITE_BITS(dev->base + REG_CAMSV_MODULE_EN,
		CAMSV_MODULE_EN, DB_EN, 0);
	CAMSV_WRITE_BITS(dev->base + REG_CAMSV_MODULE_EN,
		CAMSV_MODULE_EN, DB_EN, 1);

	dev_info(dev->dev, "%s FBC_IMGO_CTRL1/2:0x%x/0x%x\n",
		__func__,
		CAMSV_READ_REG(dev->base + REG_CAMSV_FBC_IMGO_CTL1),
		CAMSV_READ_REG(dev->base + REG_CAMSV_FBC_IMGO_CTL2));

	if (CAMSV_READ_BITS(dev->base + REG_CAMSV_TG_SEN_MODE,
		CAMSV_TG_SEN_MODE, CMOS_EN))
		CAMSV_WRITE_BITS(dev->base + REG_CAMSV_TG_VF_CON,
			CAMSV_TG_VF_CON, VFDATA_EN, 1);

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
		sv_reset(dev);
	}
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

int mtk_cam_sv_enquehwbuf(
	struct mtk_camsv_device *top_dev,
	struct mtk_camsv_device *sub_dev,
	dma_addr_t ba,
	unsigned int seq_no)
{
	int ret = 0;
	union CAMSV_TOP_FBC_CNT_SET reg;

	reg.Raw = 0;

	CAMSV_WRITE_REG(sub_dev->base + REG_CAMSV_FRAME_SEQ_NO, seq_no);
	CAMSV_WRITE_REG(sub_dev->base + REG_CAMSV_IMGO_BASE_ADDR, ba);
	if (sub_dev->id % 2)
		reg.Bits.RCNT_INC3 = 1;
	else
		reg.Bits.RCNT_INC1 = 1;
	CAMSV_WRITE_REG(top_dev->base + REG_CAMSV_TOP_FBC_CNT_SET, reg.Raw);

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

int mtk_cam_sv_apply_next_buffer(struct mtk_cam_ctx *ctx)
{
	unsigned int seq_no, used_sv_dev;
	dma_addr_t base_addr;
	struct mtk_camsv_working_buf_entry *buf_entry;
	struct device *dev_sv;
	struct mtk_camsv_device *camsv_top_dev;
	struct mtk_camsv_device *camsv_dev;

	spin_lock(&ctx->sv_using_buffer_list.lock);
	if (list_empty(&ctx->sv_using_buffer_list.list)) {
		spin_unlock(&ctx->sv_using_buffer_list.lock);
		return 0;
	}
	buf_entry = list_first_entry(&ctx->sv_using_buffer_list.list,
			struct mtk_camsv_working_buf_entry, list_entry);
	list_del(&buf_entry->list_entry);
	ctx->sv_using_buffer_list.cnt--;
	spin_unlock(&ctx->sv_using_buffer_list.lock);
	spin_lock(&ctx->sv_processing_buffer_list.lock);
	list_add_tail(&buf_entry->list_entry,
			&ctx->sv_processing_buffer_list.list);
	ctx->sv_processing_buffer_list.cnt++;
	spin_unlock(&ctx->sv_processing_buffer_list.lock);
	used_sv_dev = buf_entry->buffer.used_sv_dev;
	seq_no = buf_entry->buffer.frame_seq_no;
	base_addr = buf_entry->buffer.img_iova;
	dev_sv = mtk_cam_find_sv_dev(ctx->cam, used_sv_dev);
	if (dev_sv == NULL) {
		dev_dbg(ctx->cam->dev, "camsv device not found\n");
		return 0;
	}
	camsv_dev = dev_get_drvdata(dev_sv);
	camsv_top_dev = dev_get_drvdata(ctx->cam->sv.devs[camsv_dev->id / 2 * 2]);
	mtk_cam_sv_enquehwbuf(camsv_top_dev, camsv_dev, base_addr, seq_no);
	return 1;
}

int mtk_cam_sv_dev_config(
	struct mtk_cam_ctx *ctx,
	unsigned int idx,
	unsigned int hw_scen,
	unsigned int is_first_expo)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct device *dev = cam->dev;
	struct mtkcam_ipi_input_param cfg_in_param;
	struct v4l2_mbus_framefmt *mf;
	struct device *dev_sv;
	struct mtk_camsv_device *camsv_dev;
	struct mtk_camsv_device *camsv_top_dev;
	struct v4l2_format *img_fmt;
	unsigned int i;
	int ret, pad_idx, pixel_mode = 0;

	if (hw_scen & MTK_CAMSV_SUPPORTED_SPECIAL_HW_SCENARIO) {
		img_fmt = &ctx->pipe->vdev_nodes[MTK_RAW_MAIN_STREAM_OUT - MTK_RAW_SINK_NUM]
			.active_fmt;
		pad_idx = PAD_SRC_RAW0;
		mf = &ctx->pipe->cfg[MTK_RAW_SINK].mbus_fmt;
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
	cfg_in_param.in_crop.s.w = ALIGN(img_fmt->fmt.pix_mp.width, 4);
	cfg_in_param.in_crop.s.h = ALIGN(img_fmt->fmt.pix_mp.height, 4);
	dev_info(dev, "sink pad code:0x%x raw's imgo stride:%d\n", mf->code,
		img_fmt->fmt.pix_mp.plane_fmt[0].bytesperline);
	cfg_in_param.raw_pixel_id = mtk_cam_get_sensor_pixel_id(mf->code);
	cfg_in_param.subsample = 0;
	cfg_in_param.fmt = mtk_cam_sv_format_sel(img_fmt->fmt.pix_mp.pixelformat);

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
		camsv_dev->pipeline->is_first_expo = is_first_expo;
		camsv_top_dev = dev_get_drvdata(cam->sv.devs[camsv_dev->id / 2 * 2]);
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
		camsv_dev->pipeline->is_first_expo = 0;
		camsv_top_dev = dev_get_drvdata(cam->sv.devs[camsv_dev->id / 2 * 2]);
	}

	mtk_cam_sv_tg_config(camsv_dev, &cfg_in_param);
	mtk_cam_sv_top_config(camsv_dev, &cfg_in_param);
	mtk_cam_sv_dmao_config(camsv_top_dev, camsv_dev, &cfg_in_param,
				hw_scen, img_fmt->fmt.pix_mp.plane_fmt[0].bytesperline);
	mtk_cam_sv_fbc_config(camsv_dev, &cfg_in_param);
	mtk_cam_sv_tg_enable(camsv_dev, &cfg_in_param);
	mtk_cam_sv_dmao_enable(camsv_dev, &cfg_in_param);
	mtk_cam_sv_fbc_enable(camsv_dev, &cfg_in_param);

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
		ret = mtk_cam_sv_top_disable(camsv_dev) ||
			mtk_cam_sv_fbc_disable(camsv_dev) ||
			mtk_cam_sv_dmao_disable(camsv_dev) ||
			mtk_cam_sv_tg_disable(camsv_dev);
		pm_runtime_put(camsv_dev->dev);
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

		switch (pipe->id) {
		case MTKCAM_SUBDEV_CAMSV_0:
		case MTKCAM_SUBDEV_CAMSV_1:
		case MTKCAM_SUBDEV_CAMSV_2:
		case MTKCAM_SUBDEV_CAMSV_3:
		case MTKCAM_SUBDEV_CAMSV_4:
		case MTKCAM_SUBDEV_CAMSV_5:
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
		case 3: /* special case: 13 + 14 */
			supplier = find_larb(larb, 14);
			link = device_link_add(consumer, supplier,
					       DL_FLAG_AUTOREMOVE_CONSUMER |
					       DL_FLAG_PM_RUNTIME);
			if (!link) {
				dev_info(dev, "Unable to create link between %s and %s\n",
					dev_name(consumer), dev_name(supplier));
				return -ENODEV;
			}
			break;
		case 0:
		case 2:
		case 4:
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

void camsv_irq_handle_tg_grab_err(
	struct mtk_camsv_device *camsv_dev)
{
	int val, val2;

	val = readl_relaxed(camsv_dev->base + REG_CAMSV_TG_PATH_CFG);
	val = val|CAMSV_TG_PATH_TG_FULL_SEL;
	writel_relaxed(val, camsv_dev->base + REG_CAMSV_TG_PATH_CFG);
	wmb(); /* TBC */
	val2 = readl_relaxed(camsv_dev->base + REG_CAMSV_TG_SEN_MODE);
	val2 = val2|CAMSV_TG_SEN_MODE_CMOS_RDY_SEL;
	writel_relaxed(val2, camsv_dev->base + REG_CAMSV_TG_SEN_MODE);
	wmb(); /* TBC */
	dev_dbg_ratelimited(camsv_dev->dev,
		"TG PATHCFG/SENMODE/FRMSIZE/RGRABPXL/LIN:%x/%x/%x/%x/%x/%x\n",
		readl_relaxed(camsv_dev->base + REG_CAMSV_TG_PATH_CFG),
		readl_relaxed(camsv_dev->base + REG_CAMSV_TG_SEN_MODE),
		readl_relaxed(camsv_dev->base + REG_CAMSV_TG_FRMSIZE_ST),
		readl_relaxed(camsv_dev->base + REG_CAMSV_TG_FRMSIZE_ST_R),
		readl_relaxed(camsv_dev->base + REG_CAMSV_TG_SEN_GRAB_PXL),
		readl_relaxed(camsv_dev->base + REG_CAMSV_TG_SEN_GRAB_LIN));
}

void camsv_irq_handle_dma_err(struct mtk_camsv_device *camsv_dev)
{
	dev_dbg_ratelimited(camsv_dev->dev,
		"IMGO:0x%x\n",
		readl_relaxed(camsv_dev->base + REG_CAMSV_IMGO_ERR_STAT));
}


static irqreturn_t mtk_irq_camsv(int irq, void *data)
{
	struct mtk_camsv_device *camsv_dev = (struct mtk_camsv_device *)data;
	struct device *dev = camsv_dev->dev;
	struct mtk_camsys_irq_info irq_info;
	unsigned int dequeued_imgo_seq_no, dequeued_imgo_seq_no_inner;
	unsigned int tg_timestamp;
	unsigned int irq_status, err_status;
	unsigned int drop_status, imgo_err_status, imgo_overr_status;
	unsigned int fbc_imgo_status, imgo_addr;
	unsigned int tg_sen_mode, dcif_set, tg_vf_con, tg_path_cfg;
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&camsv_dev->spinlock_irq, flags);
	irq_status	= readl_relaxed(camsv_dev->base + REG_CAMSV_INT_STATUS);
	tg_timestamp = readl_relaxed(camsv_dev->base + REG_CAMSV_TG_TIME_STAMP);
	dequeued_imgo_seq_no =
		readl_relaxed(camsv_dev->base + REG_CAMSV_FRAME_SEQ_NO);
	dequeued_imgo_seq_no_inner =
		readl_relaxed(camsv_dev->base_inner + REG_CAMSV_FRAME_SEQ_NO);
	fbc_imgo_status =
		readl_relaxed(camsv_dev->base + REG_CAMSV_FBC_IMGO_CTL2);
	imgo_addr =
		readl_relaxed(camsv_dev->base + REG_CAMSV_IMGO_BASE_ADDR);
	tg_sen_mode =
		readl_relaxed(camsv_dev->base_inner + REG_CAMSV_TG_SEN_MODE);
	dcif_set =
		readl_relaxed(camsv_dev->base_inner + REG_CAMSV_DCIF_SET);
	tg_vf_con =
		readl_relaxed(camsv_dev->base_inner + REG_CAMSV_TG_VF_CON);
	tg_path_cfg =
		readl_relaxed(camsv_dev->base_inner + REG_CAMSV_TG_PATH_CFG);
	spin_unlock_irqrestore(&camsv_dev->spinlock_irq, flags);

	err_status = irq_status & INT_ST_MASK_CAMSV_ERR;
	imgo_err_status = irq_status & CAMSV_INT_DMA_ERR_ST;
	imgo_overr_status = irq_status & CAMSV_INT_IMGO_OVERR_ST;
	drop_status = irq_status & CAMSV_INT_IMGO_DROP_ST;

	dev_info(dev,
		"%i status:0x%x(err:0x%x) drop:0x%x imgo_dma_err:0x%x_%x fbc:0x%x (imgo:0x%x) in:%d tg_sen/dcif_set/tg_vf/tg_path:0x%x_%x_%x_%x\n",
		camsv_dev->id,
		irq_status, err_status,
		drop_status, imgo_err_status, imgo_overr_status,
		fbc_imgo_status, imgo_addr, dequeued_imgo_seq_no_inner,
		tg_sen_mode, dcif_set, tg_vf_con, tg_path_cfg);
	/*
	 * In normal case, the next SOF ISR should come after HW PASS1 DONE ISR.
	 * If these two ISRs come together, print warning msg to hint.
	 */
	irq_info.engine_id = CAMSYS_ENGINE_CAMSV_BEGIN + camsv_dev->id;
	irq_info.frame_idx = dequeued_imgo_seq_no;
	irq_info.frame_inner_idx = dequeued_imgo_seq_no_inner;
	irq_info.irq_type = 0;
	irq_info.slave_engine = 0;
	if ((irq_status & CAMSV_INT_TG_SOF_INT_ST) &&
		(irq_status & CAMSV_INT_PASS1_DON_ST))
		dev_dbg(dev, "sof_done block cnt:%d\n", camsv_dev->sof_count);
	/* Frame done */
	if (irq_status & CAMSV_INT_SW_PASS1_DON_ST)
		irq_info.irq_type |= 1<<CAMSYS_IRQ_FRAME_DONE;
	/* Frame start */
	if (irq_status & CAMSV_INT_TG_SOF_INT_ST) {
		irq_info.irq_type |= 1<<CAMSYS_IRQ_FRAME_START;
		camsv_dev->sof_count++;
	}
	/* inform interrupt information to camsys controller */
	ret = mtk_camsys_isr_event(camsv_dev->cam, &irq_info);
	if (ret)
		goto ctx_not_found;
	/* Check ISP error status */
	if (err_status) {
		dev_dbg(dev, "int_err:0x%x 0x%x\n", irq_status, err_status);
		/* Show DMA errors in detail */
		if (err_status & DMA_ST_MASK_CAMSV_ERR)
			camsv_irq_handle_dma_err(camsv_dev);
		/* Show TG register for more error detail*/
		if (err_status & CAMSV_INT_TG_GBERR_ST)
			camsv_irq_handle_tg_grab_err(camsv_dev);
	}
ctx_not_found:

	return IRQ_HANDLED;
}

bool mtk_cam_sv_finish_buf(struct mtk_cam_ctx *ctx,
			     struct mtk_cam_request *req)
{
	bool result = false;
	int cnt = 0;
	struct mtk_camsv_working_buf_entry *sv_buf_entry, *sv_buf_entry_prev;


	spin_lock(&ctx->sv_processing_buffer_list.lock);

	/* TBC: Workaround, to be optimized */
	list_for_each_entry_safe(sv_buf_entry, sv_buf_entry_prev,
				 &ctx->sv_processing_buffer_list.list,
				 list_entry) {
		if (sv_buf_entry->buffer.frame_seq_no ==
		    req->stream_data[ctx->stream_id].frame_seq_no) {
			list_del(&sv_buf_entry->list_entry);
			mtk_cam_sv_working_buf_put(ctx, sv_buf_entry);
			ctx->sv_processing_buffer_list.cnt--;
			cnt++;
			result = true;
		}
	}
	spin_unlock(&ctx->sv_processing_buffer_list.lock);

	dev_dbg(ctx->cam->dev, "put %d sv bufs, %s\n", cnt, req->req.debug_str);

	return result;
}

static int mtk_camsv_of_probe(struct platform_device *pdev,
			    struct mtk_camsv_device *sv)
{
	struct device *dev = &pdev->dev;
	const struct sv_resource *sv_res;
	struct resource *res, *res_inner;
	unsigned int i;
	int irq, ret;

	ret = of_property_read_u32(dev->of_node, "mediatek,camsv-id",
						       &sv->id);
	if (ret) {
		dev_dbg(dev, "missing camid property\n");
		return ret;
	}

	ret = of_property_read_u32(dev->of_node, "mediatek,camsv-hwcap",
						       &sv->hw_cap);
	if (ret) {
		dev_dbg(dev, "missing hardware capability property\n");
		return ret;
	}

	sv_res = sv_resources + sv->id;
	/* base outer register */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_dbg(dev, "failed to get mem\n");
		return -ENODEV;
	}

	sv->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(sv->base)) {
		dev_dbg(dev, "failed to map register base\n");
		return PTR_ERR(sv->base);
	}
	dev_dbg(dev, "camsv, map_addr=0x%pK\n", sv->base);

	/* base inner register */
	res_inner = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res_inner) {
		dev_dbg(dev, "failed to get mem\n");
		return -ENODEV;
	}

	sv->base_inner = devm_ioremap_resource(dev, res_inner);
	if (IS_ERR(sv->base_inner)) {
		dev_dbg(dev, "failed to map register inner base\n");
		return PTR_ERR(sv->base_inner);
	}
	dev_dbg(dev, "camsv, map_addr(inner)=0x%pK\n", sv->base_inner);


	irq = platform_get_irq(pdev, 0);
	if (!irq) {
		dev_dbg(dev, "failed to get irq\n");
		return -ENODEV;
	}

	ret = devm_request_irq(dev, irq, mtk_irq_camsv, 0,
				dev_name(dev), sv);
	if (ret) {
		dev_dbg(dev, "failed to request irq=%d\n", irq);
		return ret;
	}
	dev_dbg(dev, "registered irq=%d\n", irq);

	sv->num_clks = 0;
	while (sv_res->clock[sv->num_clks] && sv->num_clks < MAX_NUM_CAMSV_CLOCKS)
		sv->num_clks++;
	if (!sv->num_clks) {
		dev_dbg(dev, "no clock\n");
		return -ENODEV;
	}

	sv->clks = devm_kcalloc(dev, sv->num_clks, sizeof(*sv->clks),
				 GFP_KERNEL);
	if (!sv->clks)
		return -ENOMEM;

	for (i = 0; i < sv->num_clks; i++)
		sv->clks[i].id = sv_res->clock[i];

	ret = devm_clk_bulk_get(dev, sv->num_clks, sv->clks);
	if (ret) {
		dev_dbg(dev, "failed to get camsv clock:%d\n", ret);
		return ret;
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

	spin_lock_init(&camsv_dev->spinlock_irq);

	ret = mtk_camsv_of_probe(pdev, camsv_dev);
	if (ret)
		return ret;

	pm_runtime_set_autosuspend_delay(dev, 2 * MTK_CAMSV_STOP_HW_TIMEOUT);
	pm_runtime_use_autosuspend(dev);
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
	ret = pm_runtime_force_suspend(dev);
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
	ret = pm_runtime_force_resume(dev);
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

static int mtk_camsv_runtime_suspend(struct device *dev)
{
	struct mtk_camsv_device *camsv_dev = dev_get_drvdata(dev);
	int ret = 0;

	dev_dbg(dev, "%s:disable clock\n", __func__);
	clk_bulk_disable_unprepare(camsv_dev->num_clks, camsv_dev->clks);
	pm_runtime_put(camsv_dev->cam->dev);

#ifdef CONFIG_MTK_SMI_EXT
	int i;

	for (i = 0; i < LARB13_PORT_SIZE; i++) {
		if (larb13_support_port_map[i]) {
			ret = smi_bus_disable_unprepare(SMI_LARB13,
						larb13_port_name[i]);
			if (ret != 0) {
				dev_dbg(dev, "smi_bus_disable_unprepare fail:%d, %s\n",
					SMI_LARB13, larb13_port_name[i]);
			}
		}
	}
	for (i = 0; i < LARB14_PORT_SIZE; i++) {
		if (larb14_support_port_map[i]) {
			ret = smi_bus_disable_unprepare(SMI_LARB14,
						larb14_port_name[i]);
			if (ret != 0) {
				dev_dbg(dev, "smi_bus_disable_unprepare fail:%d, %s\n",
					SMI_LARB14, larb14_port_name[i]);
			}
		}
	}
#endif

	return ret;
}

static int mtk_camsv_runtime_resume(struct device *dev)
{
	struct mtk_camsv_device *camsv_dev = dev_get_drvdata(dev);
	int ret = 0;

	dev_dbg(dev, "%s:enable clock\n", __func__);
	pm_runtime_get_sync(camsv_dev->cam->dev);
	ret = clk_bulk_prepare_enable(camsv_dev->num_clks, camsv_dev->clks);
	if (ret) {
		dev_dbg(dev, "failed to enable clock:%d\n", ret);
		return ret;
	}

#ifdef CONFIG_MTK_SMI_EXT
	int i;

	for (i = 0; i < LARB13_PORT_SIZE; i++) {
		if (larb13_support_port_map[i]) {
			ret = smi_bus_prepare_enable(SMI_LARB13,
						larb13_port_name[i]);
			if (ret != 0) {
				dev_dbg(dev, "smi_bus_disable_unprepare fail:%d, %s\n",
					SMI_LARB13, larb13_port_name[i]);
			}
		}
	}
	for (i = 0; i < LARB14_PORT_SIZE; i++) {
		if (larb14_support_port_map[i]) {
			ret = smi_bus_prepare_enable(SMI_LARB14,
						larb14_port_name[i]);
			if (ret != 0) {
				dev_dbg(dev, "smi_bus_disable_unprepare fail:%d, %s\n",
					SMI_LARB14, larb14_port_name[i]);
			}
		}
	}
#endif

#ifdef CONFIG_MTK_IOMMU_V2
	m4u_control_camsv_iommu_port();
#endif
	sv_reset(camsv_dev);

	return ret;
}

static const struct dev_pm_ops mtk_camsv_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_camsv_pm_suspend, mtk_camsv_pm_resume)
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

