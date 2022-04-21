// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2021 MediaTek Inc.

#include "mtk_cam-tg-flash.h"
#include "mtk_cam-regs.h"

static void
mtk_cam_tg_flash_common_config(struct mtk_raw_device *raw,
			       struct mtk_cam_tg_flash_config *tg_config)
{
	u32 val;

	/**
	 * Flash ID selection - each selection has 4bits setting:
	 *	   0: from cam_raw_a tg_flash_0 (XENON)
	 *	   1: from cam_raw_a tg_flash_1 (IR)
	 *	   2: from cam_raw_b tg_flash_0 (XENON)
	 *	   3: from cam_raw_b tg_flash_1 (IR)
	 *	   4: from cam_raw_c tg_flash_0 (XENON)
	 *	   5: from cam_raw_c tg_flash_1 (IR)
	 *
	 * We select IR by default.
	 */
	val = (raw->id * 2 + 1) << (tg_config->flash_light_id * 4);
	writel_relaxed(val, raw->cam->base + REG_FLASH);

	/* fixed clk: 208MHz, micro sec to tick */
	val = tg_config->flash_offset * 208;
	writel_relaxed(val, raw->base + REG_TG_IR_FLASH_OFFSET);

	val = tg_config->flash_high_width * 208;
	writel_relaxed(val, raw->base + REG_TG_IR_FLASH_HIGH_WIDTH);
}

static void
mtk_cam_tg_flash_trigger_single(struct mtk_raw_device *raw,
				struct mtk_cam_tg_flash_config *tg_config)
{
	u32 val;

	mtk_cam_tg_flash_common_config(raw, tg_config);

	val = 0x1; /* enable and single mode:0x0 */
	writel_relaxed(val, raw->base + REG_TG_IR_FLASH_CTL);
}

static void
mtk_cam_tg_flash_trigger_multiple(struct mtk_raw_device *raw,
				  struct mtk_cam_tg_flash_config *tg_config)
{
	u32 val;

	mtk_cam_tg_flash_common_config(raw, tg_config);

	val = tg_config->flash_low_width * 208;
	writel_relaxed(val, raw->base + REG_TG_IR_FLASH_LOW_WIDTH);

	/* enable, multiple mode:0x2 and pluse num */
	val = 0x1 | 0x2 << 3 | tg_config->flash_pluse_num << 8;
	writel_relaxed(val, raw->base + REG_TG_IR_FLASH_CTL);
}

static void
mtk_cam_tg_flash_trigger_continuous(struct mtk_raw_device *raw,
				    struct mtk_cam_tg_flash_config *tg_config)
{
	u32 val;

	mtk_cam_tg_flash_common_config(raw, tg_config);

	/* enable and multiple pulse mode:0x2 */
	val = 0x1 | 0x1 << 3;
	writel_relaxed(val, raw->base + REG_TG_IR_FLASH_CTL);
}

void
mtk_cam_tg_flash_stop(struct mtk_raw_device *raw,
		      struct mtk_cam_tg_flash_config *tg_config)
{
	writel_relaxed(0x0, raw->base_inner + REG_TG_IR_FLASH_CTL);
}

void mtk_cam_tg_flash_setup(struct mtk_raw_device *raw,
			    struct mtk_cam_tg_flash_config *tg_config)
{
	switch (tg_config->flash_mode) {
	case V4L2_MTK_CAM_TG_FLASH_MODE_SINGLE:
			mtk_cam_tg_flash_trigger_single(raw, tg_config);
		break;
	case V4L2_MTK_CAM_TG_FLASH_MODE_CONTINUOUS:
		if (tg_config->flash_enable)
			mtk_cam_tg_flash_trigger_continuous(raw,
							    tg_config);
		else
			mtk_cam_tg_flash_stop(raw, tg_config);
		break;
	case V4L2_MTK_CAM_TG_FLASH_MODE_MULTIPLE:
			mtk_cam_tg_flash_trigger_multiple(raw,
							  tg_config);
		break;
	default:
		dev_info(raw->dev, "%s:unknown flash mode(%d)\n",
			 __func__, tg_config->flash_mode);
		break;
	}

	/* TO BE REMOVED */
	dev_info(raw->dev,
		 "%s:REG: [0x%llx]0X%x,[0x%llx]0x%x,[0x%llx]0x%x,[0x%llx]0x%x,[0x%llx]0x%x\n",
		 __func__,
		 raw->cam->base + REG_FLASH,
		 readl_relaxed(raw->cam->base + REG_FLASH),
		 raw->base + REG_TG_IR_FLASH_CTL,
		 readl_relaxed(raw->base + REG_TG_IR_FLASH_CTL),
		 raw->base + REG_TG_IR_FLASH_OFFSET,
		 readl_relaxed(raw->base + REG_TG_IR_FLASH_OFFSET),
		 raw->base + REG_TG_IR_FLASH_HIGH_WIDTH,
		 readl_relaxed(raw->base + REG_TG_IR_FLASH_HIGH_WIDTH),
		 raw->base + REG_TG_IR_FLASH_LOW_WIDTH,
		 readl_relaxed(raw->base + REG_TG_IR_FLASH_LOW_WIDTH));
}

int mtk_cam_tg_flash_try_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_raw_pipeline *pipeline;
	struct mtk_cam_tg_flash_config *flash_config;
	struct device *dev;

	pipeline = mtk_cam_ctrl_handler_to_raw_pipeline(ctrl->handler);
	dev = pipeline->raw->devs[pipeline->id];

	/**
	 * Do the checks here so that the user cam know if the ctrl is
	 * valid or not.
	 */
	flash_config = (struct mtk_cam_tg_flash_config *)ctrl->p_new.p;
	if (!flash_config) {
		dev_info(dev,
			 "%s:pipe(%d):ctrl(id:0x%x, val:%p) ctrl->p_new.p is NULL\n",
			 __func__, pipeline->id, ctrl->id, ctrl->p_new.p);
		return -EINVAL;
	}

	if (flash_config->flash_light_id >= V4L2_MTK_CAM_TG_FALSH_ID_MAX) {
		dev_info(dev,
			 "%s:pipe(%d):ctrl(id:0x%x): invalid flash id (%d)\n",
			 __func__, pipeline->id, ctrl->id, flash_config->flash_light_id);
		return -EINVAL;
	}

	if (!flash_config->flash_enable &&
	    flash_config->flash_mode != V4L2_MTK_CAM_TG_FLASH_MODE_CONTINUOUS) {
		dev_info(dev,
			 "%s:pipe(%d):ctrl(id:0x%x):mode(%d), en(%d), only continuous mode allow disable.\n",
			 __func__, pipeline->id, ctrl->id,
			 flash_config->flash_mode, flash_config->flash_enable);
		return -EINVAL;
	}

	return 0;
}

int mtk_cam_tg_flash_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_raw_pipeline *pipeline;
	struct mtk_cam_tg_flash_config *flash_config;
	struct device *dev;

	pipeline = mtk_cam_ctrl_handler_to_raw_pipeline(ctrl->handler);
	dev = pipeline->raw->devs[pipeline->id];

	if (mtk_cam_tg_flash_try_ctrl(ctrl)) {
		pipeline->enqueued_tg_flash_req = false;
		return -EINVAL;
	}

	flash_config = (struct mtk_cam_tg_flash_config *)ctrl->p_new.p;
	pipeline->tg_flash_config = *flash_config;
	pipeline->enqueued_tg_flash_req = true;

	return 0;
}

void
mtk_cam_tg_flash_req_update(struct mtk_raw_pipeline *pipe,
			    struct mtk_cam_request_stream_data *s_data)
{
	if (pipe->enqueued_tg_flash_req) {
		s_data->tg_flash_config = pipe->tg_flash_config;
		s_data->flags |= MTK_CAM_REQ_S_DATA_FLAG_TG_FLASH;
		memset(&pipe->tg_flash_config, 0,
		       sizeof(pipe->tg_flash_config));
	} else {
		s_data->flags &= ~(MTK_CAM_REQ_S_DATA_FLAG_TG_FLASH);
	}

	pipe->enqueued_tg_flash_req = false;
}

void
mtk_cam_tg_flash_req_setup(struct mtk_cam_ctx *ctx,
			   struct mtk_cam_request_stream_data *req_stream_data)
{
	struct mtk_cam_tg_flash_config *tg_config;
	struct mtk_raw_device *raw;

	if (!(req_stream_data->flags & MTK_CAM_REQ_S_DATA_FLAG_TG_FLASH))
		return;

	raw = get_master_raw_dev(ctx->cam, ctx->pipe);
	tg_config = &req_stream_data->tg_flash_config;

	dev_info(ctx->cam->dev,
		 "%s: en(%u),mode(%u),p_num(%u),ofst(%u),h_width(%u),l_width(%u),id(%u)",
		 __func__, tg_config->flash_enable, tg_config->flash_mode,
		 tg_config->flash_pluse_num, tg_config->flash_offset,
		 tg_config->flash_high_width, tg_config->flash_low_width,
		 tg_config->flash_light_id);

	mtk_cam_tg_flash_setup(raw, tg_config);
}

void
mtk_cam_tg_flash_req_done(struct mtk_cam_request_stream_data *req_stream_data)
{
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_tg_flash_config *tg_config;
	struct mtk_raw_device *raw;

	if (!(req_stream_data->flags & MTK_CAM_REQ_S_DATA_FLAG_TG_FLASH))
		return;

	tg_config = &req_stream_data->tg_flash_config;
	if (tg_config->flash_mode != V4L2_MTK_CAM_TG_FLASH_MODE_SINGLE)
		return;

	ctx = mtk_cam_s_data_get_ctx(req_stream_data);
	if (!ctx) {
		pr_info("%s: get ctx from s_data failed", __func__);
		return;
	}

	raw = get_master_raw_dev(ctx->cam, ctx->pipe);
	mtk_cam_tg_flash_stop(raw, tg_config);

	dev_info(ctx->cam->dev,
		 "%s: en(%u),mode(%u),p_num(%u),ofst(%u),h_width(%u),l_width(%u),id(%u)",
		 __func__, tg_config->flash_enable, tg_config->flash_mode,
		 tg_config->flash_pluse_num, tg_config->flash_offset,
		 tg_config->flash_high_width, tg_config->flash_low_width,
		 tg_config->flash_light_id);
}
