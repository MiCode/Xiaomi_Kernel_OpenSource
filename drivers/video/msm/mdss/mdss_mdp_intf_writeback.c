/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include "mdss.h"
#include "mdss_mdp.h"
#include "mdss_mdp_rotator.h"
#include "mdss_panel.h"
#include "mdss_mdp_trace.h"
#include "mdss_debug.h"

/*
 * if BWC enabled and format is H1V2 or 420, do not use site C or I.
 * Hence, set the bits 29:26 in format register, as zero.
 */
#define BWC_FMT_MASK	0xC3FFFFFF
#define MDSS_DEFAULT_OT_SETTING    0x10

enum mdss_mdp_writeback_type {
	MDSS_MDP_WRITEBACK_TYPE_ROTATOR,
	MDSS_MDP_WRITEBACK_TYPE_LINE,
	MDSS_MDP_WRITEBACK_TYPE_WFD,
};

struct mdss_mdp_writeback_ctx {
	u32 wb_num;
	char __iomem *base;
	u8 ref_cnt;
	u8 type;
	bool is_vbif_nrt;
	struct completion wb_comp;
	int comp_cnt;

	u32 intr_type;
	u32 intf_num;

	u32 xin_id;
	u32 wr_lim;
	struct mdss_mdp_shared_reg_ctrl clk_ctrl;

	u32 opmode;
	struct mdss_mdp_format_params *dst_fmt;
	u16 width;
	u16 height;
	u16 frame_rate;
	struct mdss_rect dst_rect;

	u32 dnsc_factor_w;
	u32 dnsc_factor_h;

	u8 rot90;
	u32 bwc_mode;
	int initialized;

	struct mdss_mdp_plane_sizes dst_planes;

	spinlock_t wb_lock;
	struct list_head vsync_handlers;

	ktime_t start_time;
	ktime_t end_time;
};

static struct mdss_mdp_writeback_ctx wb_ctx_list[MDSS_MDP_MAX_WRITEBACK] = {
	{
		.type = MDSS_MDP_WRITEBACK_TYPE_ROTATOR,
		.intr_type = MDSS_MDP_IRQ_WB_ROT_COMP,
		.intf_num = 0,
		.xin_id = 3,
		.clk_ctrl.reg_off = 0x2BC,
		.clk_ctrl.bit_off = 0x8,
	},
	{
		.type = MDSS_MDP_WRITEBACK_TYPE_ROTATOR,
		.intr_type = MDSS_MDP_IRQ_WB_ROT_COMP,
		.intf_num = 1,
		.xin_id = 11,
		.clk_ctrl.reg_off = 0x2BC,
		.clk_ctrl.bit_off = 0xC,
	},
	{
		.type = MDSS_MDP_WRITEBACK_TYPE_LINE,
		.intr_type = MDSS_MDP_IRQ_WB_ROT_COMP,
		.intf_num = 0,
		.xin_id = 3,
		.clk_ctrl.reg_off = 0x2BC,
		.clk_ctrl.bit_off = 0x8,
	},
	{
		.type = MDSS_MDP_WRITEBACK_TYPE_LINE,
		.intr_type = MDSS_MDP_IRQ_WB_ROT_COMP,
		.intf_num = 1,
		.xin_id = 11,
		.clk_ctrl.reg_off = 0x2BC,
		.clk_ctrl.bit_off = 0xC,
	},
	{
		.type = MDSS_MDP_WRITEBACK_TYPE_WFD,
		.intr_type = MDSS_MDP_IRQ_WB_WFD,
		.intf_num = 0,
		.xin_id = 6,
		.clk_ctrl.reg_off = 0x2BC,
		.clk_ctrl.bit_off = 0x10,
	},
};

static inline void mdp_wb_write(struct mdss_mdp_writeback_ctx *ctx,
				u32 reg, u32 val)
{
	writel_relaxed(val, ctx->base + reg);
}

static int mdss_mdp_writeback_addr_setup(struct mdss_mdp_writeback_ctx *ctx,
					 const struct mdss_mdp_data *in_data)
{
	int ret;
	struct mdss_mdp_data data;

	if (!in_data)
		return -EINVAL;
	data = *in_data;

	pr_debug("wb_num=%d addr=0x%pa\n", ctx->wb_num, &data.p[0].addr);

	ret = mdss_mdp_data_check(&data, &ctx->dst_planes);
	if (ret)
		return ret;

	mdss_mdp_data_calc_offset(&data, ctx->dst_rect.x, ctx->dst_rect.y,
			&ctx->dst_planes, ctx->dst_fmt);

	if ((ctx->dst_fmt->fetch_planes == MDSS_MDP_PLANE_PLANAR) &&
			(ctx->dst_fmt->element[0] == C1_B_Cb))
		swap(data.p[1].addr, data.p[2].addr);

	mdp_wb_write(ctx, MDSS_MDP_REG_WB_DST0_ADDR, data.p[0].addr);
	mdp_wb_write(ctx, MDSS_MDP_REG_WB_DST1_ADDR, data.p[1].addr);
	mdp_wb_write(ctx, MDSS_MDP_REG_WB_DST2_ADDR, data.p[2].addr);
	mdp_wb_write(ctx, MDSS_MDP_REG_WB_DST3_ADDR, data.p[3].addr);

	return 0;
}

static int mdss_mdp_writeback_format_setup(struct mdss_mdp_writeback_ctx *ctx,
		u32 format)
{
	struct mdss_mdp_format_params *fmt;
	u32 dst_format, pattern, ystride0, ystride1, outsize, chroma_samp;
	u32 dnsc_factor;
	u32 opmode = ctx->opmode;
	bool rotation = false;
	struct mdss_data_type *mdata;

	pr_debug("wb_num=%d format=%d\n", ctx->wb_num, format);

	if (ctx->rot90)
		rotation = true;

	mdss_mdp_get_plane_sizes(format, ctx->width, ctx->height,
				 &ctx->dst_planes,
				 ctx->opmode & MDSS_MDP_OP_BWC_EN, rotation);

	fmt = mdss_mdp_get_format_params(format);
	if (!fmt) {
		pr_err("wb format=%d not supported\n", format);
		return -EINVAL;
	}
	ctx->dst_fmt = fmt;

	chroma_samp = fmt->chroma_sample;

	if (ctx->type != MDSS_MDP_WRITEBACK_TYPE_ROTATOR && fmt->is_yuv) {
		mdss_mdp_csc_setup(MDSS_MDP_BLOCK_WB, ctx->wb_num,
				   MDSS_MDP_CSC_RGB2YUV_601L);
		opmode |= (1 << 8) |	/* CSC_EN */
			  (0 << 9) |	/* SRC_DATA=RGB */
			  (1 << 10);	/* DST_DATA=YCBCR */

		switch (chroma_samp) {
		case MDSS_MDP_CHROMA_RGB:
		case MDSS_MDP_CHROMA_420:
		case MDSS_MDP_CHROMA_H2V1:
			opmode |= (chroma_samp << 11);
			break;
		case MDSS_MDP_CHROMA_H1V2:
		default:
			pr_err("unsupported wb chroma samp=%d\n", chroma_samp);
			return -EINVAL;
		}
	} else if (ctx->rot90) {
		if (chroma_samp == MDSS_MDP_CHROMA_H2V1)
			chroma_samp = MDSS_MDP_CHROMA_H1V2;
		else if (chroma_samp == MDSS_MDP_CHROMA_H1V2)
			chroma_samp = MDSS_MDP_CHROMA_H2V1;
	}

	dst_format = (chroma_samp << 23) |
		     (fmt->fetch_planes << 19) |
		     (fmt->bits[C3_ALPHA] << 6) |
		     (fmt->bits[C2_R_Cr] << 4) |
		     (fmt->bits[C1_B_Cb] << 2) |
		     (fmt->bits[C0_G_Y] << 0);

	dst_format &= BWC_FMT_MASK;

	if (fmt->bits[C3_ALPHA] || fmt->alpha_enable) {
		dst_format |= BIT(8); /* DSTC3_EN */
		if (!fmt->alpha_enable)
			dst_format |= BIT(14); /* DST_ALPHA_X */
	}

	mdata = mdss_mdp_get_mdata();
	if (mdata && mdata->mdp_rev >= MDSS_MDP_HW_REV_102 &&
			mdata->mdp_rev < MDSS_MDP_HW_REV_200) {
		pattern = (fmt->element[3] << 24) |
			  (fmt->element[2] << 16) |
			  (fmt->element[1] << 8)  |
			  (fmt->element[0] << 0);
	} else {
		pattern = (fmt->element[3] << 24) |
			  (fmt->element[2] << 15) |
			  (fmt->element[1] << 8)  |
			  (fmt->element[0] << 0);
	}

	dst_format |= (fmt->unpack_align_msb << 18) |
		      (fmt->unpack_tight << 17) |
		      ((fmt->unpack_count - 1) << 12) |
		      ((fmt->bpp - 1) << 9);

	ystride0 = (ctx->dst_planes.ystride[0]) |
		   (ctx->dst_planes.ystride[1] << 16);
	ystride1 = (ctx->dst_planes.ystride[2]) |
		   (ctx->dst_planes.ystride[3] << 16);
	outsize = (ctx->dst_rect.h << 16) | ctx->dst_rect.w;

	if (ctx->type == MDSS_MDP_WRITEBACK_TYPE_ROTATOR &&
			mdata && mdata->has_rot_dwnscale) {
		dnsc_factor = (ctx->dnsc_factor_h) | (ctx->dnsc_factor_w << 16);
		mdp_wb_write(ctx, MDSS_MDP_REG_WB_ROTATOR_PIPE_DOWNSCALER,
								dnsc_factor);
	}
	mdp_wb_write(ctx, MDSS_MDP_REG_WB_ALPHA_X_VALUE, 0xFF);
	mdp_wb_write(ctx, MDSS_MDP_REG_WB_DST_FORMAT, dst_format);
	mdp_wb_write(ctx, MDSS_MDP_REG_WB_DST_OP_MODE, opmode);
	mdp_wb_write(ctx, MDSS_MDP_REG_WB_DST_PACK_PATTERN, pattern);
	mdp_wb_write(ctx, MDSS_MDP_REG_WB_DST_YSTRIDE0, ystride0);
	mdp_wb_write(ctx, MDSS_MDP_REG_WB_DST_YSTRIDE1, ystride1);
	mdp_wb_write(ctx, MDSS_MDP_REG_WB_OUT_SIZE, outsize);

	return 0;
}

static int mdss_mdp_writeback_prepare_wfd(struct mdss_mdp_ctl *ctl, void *arg)
{
	struct mdss_mdp_writeback_ctx *ctx;
	int ret;

	ctx = (struct mdss_mdp_writeback_ctx *) ctl->priv_data;
	if (!ctx)
		return -ENODEV;

	if (ctx->initialized && !ctl->shared_lock) /* already set */
		return 0;

	pr_debug("wfd setup ctl=%d\n", ctl->num);

	ctx->opmode = 0;
	ctx->width = ctl->width;
	ctx->height = ctl->height;
	ctx->frame_rate = ctl->frame_rate;
	ctx->dst_rect.x = 0;
	ctx->dst_rect.y = 0;
	ctx->dst_rect.w = ctx->width;
	ctx->dst_rect.h = ctx->height;

	ret = mdss_mdp_writeback_format_setup(ctx, ctl->dst_format);
	if (ret) {
		pr_err("format setup failed\n");
		return ret;
	}

	ctx->initialized = true;

	return 0;
}

static int mdss_mdp_writeback_prepare_rot(struct mdss_mdp_ctl *ctl, void *arg)
{
	struct mdss_mdp_writeback_ctx *ctx;
	struct mdss_mdp_writeback_arg *wb_args;
	struct mdss_mdp_rotator_session *rot;
	struct mdss_data_type *mdata;
	struct mdss_mdp_format_params *fmt;
	u32 format;

	ctx = (struct mdss_mdp_writeback_ctx *) ctl->priv_data;
	if (!ctx)
		return -ENODEV;
	wb_args = (struct mdss_mdp_writeback_arg *) arg;
	if (!wb_args)
		return -ENOENT;

	rot = (struct mdss_mdp_rotator_session *) wb_args->priv_data;
	if (!rot) {
		pr_err("unable to retrieve rot session ctl=%d\n", ctl->num);
		return -ENODEV;
	}
	mdata = ctl->mdata;
	if (!mdata) {
		pr_err("no mdata attached to ctl=%d", ctl->num);
		return -ENODEV;
	}
	pr_debug("rot setup wb_num=%d\n", ctx->wb_num);

	ctx->opmode = BIT(6); /* ROT EN */
	if (ctl->mdata->rot_block_size == 128)
		ctx->opmode |= BIT(4); /* block size 128 */

	ctx->bwc_mode = rot->bwc_mode;
	ctx->opmode |= ctx->bwc_mode;

	ctx->width = ctx->dst_rect.w = rot->dnsc_factor_w ?
		rot->dst.w / rot->dnsc_factor_w : rot->dst.w;
	ctx->height = ctx->dst_rect.h = rot->dnsc_factor_h ?
		rot->dst.h / rot->dnsc_factor_h : rot->dst.h;
	ctx->dst_rect.x = rot->dst.x;
	ctx->dst_rect.y = rot->dst.y;
	ctx->dnsc_factor_w = rot->dnsc_factor_w;
	ctx->dnsc_factor_h = rot->dnsc_factor_h;

	ctx->rot90 = !!(rot->flags & MDP_ROT_90);

	fmt = mdss_mdp_get_format_params(rot->format);
	if (!fmt) {
		pr_err("invalid pipe format %d\n", rot->format);
		return -EINVAL;
	}

	format = mdss_mdp_get_rotator_dst_format(rot->format,
			ctx->rot90, ctx->bwc_mode);

	if (ctx->rot90)
		ctx->opmode |= BIT(5); /* ROT 90 */

	return mdss_mdp_writeback_format_setup(ctx, format);
}

static int mdss_mdp_wb_add_vsync_handler(struct mdss_mdp_ctl *ctl,
		struct mdss_mdp_vsync_handler *handle)
{
	struct mdss_mdp_writeback_ctx *ctx;
	unsigned long flags;
	int ret = 0;

	if (!handle || !(handle->vsync_handler)) {
		ret = -EINVAL;
		goto exit;
	}

	ctx = (struct mdss_mdp_writeback_ctx *) ctl->priv_data;
	if (!ctx) {
		pr_err("invalid ctx for ctl=%d\n", ctl->num);
		ret = -ENODEV;
		goto exit;
	}

	spin_lock_irqsave(&ctx->wb_lock, flags);
	if (!handle->enabled) {
		handle->enabled = true;
		list_add(&handle->list, &ctx->vsync_handlers);
	}
	spin_unlock_irqrestore(&ctx->wb_lock, flags);
exit:
	return ret;
}

static int mdss_mdp_wb_remove_vsync_handler(struct mdss_mdp_ctl *ctl,
		struct mdss_mdp_vsync_handler *handle)
{
	struct mdss_mdp_writeback_ctx *ctx;
	unsigned long flags;
	int ret = 0;
	if (!handle || !(handle->vsync_handler)) {
		ret = -EINVAL;
		goto exit;
	}
	ctx = (struct mdss_mdp_writeback_ctx *) ctl->priv_data;
	if (!ctx) {
		pr_err("invalid ctx for ctl=%d\n", ctl->num);
		ret = -ENODEV;
		goto exit;
	}
	spin_lock_irqsave(&ctx->wb_lock, flags);
	if (handle->enabled) {
		handle->enabled = false;
		list_del_init(&handle->list);
	}
	spin_unlock_irqrestore(&ctx->wb_lock, flags);
exit:
	return ret;
}

static int mdss_mdp_writeback_stop(struct mdss_mdp_ctl *ctl,
	int panel_power_state)
{
	struct mdss_mdp_writeback_ctx *ctx;
	struct mdss_mdp_vsync_handler *t, *handle;

	pr_debug("stop ctl=%d\n", ctl->num);

	ctx = (struct mdss_mdp_writeback_ctx *) ctl->priv_data;
	if (ctx) {
		list_for_each_entry_safe(handle, t, &ctx->vsync_handlers, list)
			mdss_mdp_wb_remove_vsync_handler(ctl, handle);

		mdss_mdp_set_intr_callback(ctx->intr_type, ctx->intf_num,
				NULL, NULL);

		complete_all(&ctx->wb_comp);

		ctl->priv_data = NULL;
		ctx->ref_cnt--;
	}

	return 0;
}

static void mdss_mdp_writeback_intr_done(void *arg)
{
	struct mdss_mdp_ctl *ctl = arg;
	struct mdss_mdp_writeback_ctx *ctx = ctl->priv_data;
	struct mdss_mdp_vsync_handler *tmp;
	ktime_t vsync_time;

	if (!ctx) {
		pr_err("invalid ctx\n");
		return;
	}
	vsync_time = ktime_get();

	pr_debug("intr wb_num=%d\n", ctx->wb_num);
	trace_mdp_wb_done(ctx->wb_num, ctx->xin_id, ctx->intf_num);

	mdss_mdp_irq_disable_nosync(ctx->intr_type, ctx->intf_num);

	spin_lock(&ctx->wb_lock);
	list_for_each_entry(tmp, &ctx->vsync_handlers, list) {
		tmp->vsync_handler(ctl, vsync_time);
	}
	spin_unlock(&ctx->wb_lock);

	complete_all(&ctx->wb_comp);
	MDSS_XLOG(ctx->wb_num, ctx->type, ctx->xin_id, ctx->intf_num);
}

static bool mdss_mdp_traffic_shaper_helper(struct mdss_mdp_ctl *ctl,
					 struct mdss_mdp_writeback_ctx *ctx,
					 bool enable)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	bool traffic_shaper_enabled = false;
	struct mdss_mdp_mixer *mixer = ctl->mixer_left;
	int i;
	u32 clk_rate;
	u64 bw_rate;

	if (!mixer)
		return traffic_shaper_enabled;

	/* currently only for rotator pipes */
	if (!mixer->rotator_mode)
		return traffic_shaper_enabled;

	for (i = 0; i < MDSS_MDP_MAX_STAGE; i++) {
		struct mdss_mdp_pipe *pipe;
		struct mdss_mdp_perf_params perf;
		u32 traffic_shaper;
		pipe = mixer->stage_pipe[i];

		memset(&perf, 0, sizeof(perf));

		if (pipe == NULL)
			continue;

		if (enable) {
			if (mdss_mdp_perf_calc_pipe(pipe, &perf, &mixer->roi,
				PERF_CALC_PIPE_SINGLE_LAYER))
				continue;

			clk_rate = max(mdss_mdp_get_mdp_clk_rate(ctl->mdata),
					perf.mdp_clk_rate);
			ctl->traffic_shaper_mdp_clk = clk_rate;
			bw_rate = perf.bw_overlap;

			/*
			 * Bandwidth vote accounts for both read and write
			 * rotator, divide by 2 to get only the write bandwidth.
			 */
			do_div(bw_rate, 2);

			/*
			 * Calculating bytes per clock in 4.4 form
			 * allowing up to 1/16 granularity.
			 */
			do_div(bw_rate,
				(clk_rate >>
				 MDSS_MDP_REG_TRAFFIC_SHAPER_FIXPOINT_FACTOR));

			traffic_shaper = lower_32_bits(bw_rate) + 1;
			traffic_shaper |= MDSS_MDP_REG_TRAFFIC_SHAPER_EN;
			traffic_shaper_enabled = true;

			pr_debug("pnum=%d inum:%d bw=%lld clk_rate=%u shaper=0x%x ena:%d\n",
				pipe->num, ctx->intf_num, perf.bw_overlap,
				clk_rate, traffic_shaper, enable);

		} else {
			traffic_shaper = 0;

			pr_debug("inum:%d shaper=0x%x, ena:%d\n",
				ctx->intf_num, traffic_shaper, enable);
		}

		writel_relaxed(traffic_shaper, mdata->mdp_base +
			MDSS_MDP_REG_TRAFFIC_SHAPER_WR_CLIENT(ctx->intf_num));
	}

	return traffic_shaper_enabled;
}

static void mdss_mdp_traffic_shaper(struct mdss_mdp_ctl *ctl,
		struct mdss_mdp_writeback_ctx *ctx, bool enable)
{
	bool traffic_shaper_enabled = 0;

	if (mdss_mdp_ctl_is_power_on(ctl)) {
		traffic_shaper_enabled = mdss_mdp_traffic_shaper_helper
			(ctl, ctx, enable);
	}

	ctl->traffic_shaper_enabled = traffic_shaper_enabled;

	pr_debug("traffic shapper ctl:%d ena:%d\n", ctl->num,
		ctl->traffic_shaper_enabled);
}

static int mdss_mdp_wb_wait4comp(struct mdss_mdp_ctl *ctl, void *arg)
{
	struct mdss_mdp_writeback_ctx *ctx;
	int rc = 0;
	u64 rot_time;

	ctx = (struct mdss_mdp_writeback_ctx *) ctl->priv_data;
	if (!ctx) {
		pr_err("invalid ctx\n");
		return -ENODEV;
	}

	if (ctx->comp_cnt == 0)
		return rc;

	rc = wait_for_completion_timeout(&ctx->wb_comp,
			KOFF_TIMEOUT);
	mdss_mdp_set_intr_callback(ctx->intr_type, ctx->intf_num,
		NULL, NULL);

	if (rc == 0) {
		u32 status, mask, isr;

		mask = BIT(ctx->intr_type + ctx->intf_num);

		isr = readl_relaxed(ctl->mdata->mdp_base +
					MDSS_MDP_REG_INTR_STATUS);
		status = mask & isr;

		pr_info_once("mask: 0x%x, isr: 0x%x, status: 0x%x\n",
				mask, isr, status);

		if (status) {
			pr_warn_once("wb done but irq not triggered\n");
			mdss_mdp_irq_clear(ctl->mdata,
					ctx->intr_type,
					ctx->intf_num);

			mdss_mdp_writeback_intr_done(ctl);
			rc = 0;
		} else {
			mdss_mdp_ctl_notify(ctl, MDP_NOTIFY_FRAME_TIMEOUT);
			rc = -ENODEV;
			WARN(1, "writeback kickoff timed out (%d) ctl=%d\n",
							rc, ctl->num);
		}
	} else {
		rc = 0;
	}

	if (rc == 0) {
		ctx->end_time = ktime_get();
		mdss_mdp_ctl_notify(ctl, MDP_NOTIFY_FRAME_DONE);
	}

	/* once operation is done, disable traffic shaper */
	if (ctl->traffic_shaper_enabled)
		mdss_mdp_traffic_shaper(ctl, ctx, false);

	mdss_iommu_ctrl(0);
	mdss_bus_bandwidth_ctrl(false);
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);

	/* Set flag to release Controller Bandwidth */
	ctl->perf_release_ctl_bw = true;

	ctx->comp_cnt--;

	if (!rc) {
		rot_time = (u64)ktime_to_us(ctx->end_time) -
				(u64)ktime_to_us(ctx->start_time);
		pr_debug("ctx%d type:%d xin_id:%d intf_num:%d took %llu microsecs\n",
			ctx->wb_num, ctx->type, ctx->xin_id,
				ctx->intf_num, rot_time);
	}

	return rc;
}

static void mdss_mdp_set_ot_limit_wb(struct mdss_mdp_writeback_ctx *ctx)
{
	struct mdss_mdp_set_ot_params ot_params;

	ot_params.xin_id = ctx->xin_id;
	ot_params.num = ctx->wb_num;
	ot_params.width = ctx->width;
	ot_params.height = ctx->height;
	ot_params.frame_rate = ctx->frame_rate;
	ot_params.reg_off_vbif_lim_conf = MMSS_VBIF_WR_LIM_CONF;
	ot_params.reg_off_mdp_clk_ctrl = ctx->clk_ctrl.reg_off;
	ot_params.bit_off_mdp_clk_ctrl = ctx->clk_ctrl.bit_off;
	ot_params.is_rot = (ctx->type == MDSS_MDP_WRITEBACK_TYPE_ROTATOR);
	ot_params.is_wb = (ctx->type == MDSS_MDP_WRITEBACK_TYPE_WFD) ||
		(ctx->type == MDSS_MDP_WRITEBACK_TYPE_LINE);
	ot_params.is_yuv = ctx->dst_fmt->is_yuv;

	mdss_mdp_set_ot_limit(&ot_params);

}

static int mdss_mdp_writeback_display(struct mdss_mdp_ctl *ctl, void *arg)
{
	struct mdss_mdp_writeback_ctx *ctx;
	struct mdss_mdp_writeback_arg *wb_args;
	u32 flush_bits = 0;
	int ret;

	if (!ctl || !ctl->mdata)
		return -ENODEV;

	ctx = (struct mdss_mdp_writeback_ctx *) ctl->priv_data;
	if (!ctx)
		return -ENODEV;

	if (ctx->comp_cnt) {
		pr_err("previous kickoff not completed yet, ctl=%d\n",
					ctl->num);
		return -EPERM;
	}

	if (ctl->mdata->default_ot_wr_limit ||
			ctl->mdata->default_ot_rd_limit)
		mdss_mdp_set_ot_limit_wb(ctx);

	wb_args = (struct mdss_mdp_writeback_arg *) arg;
	if (!wb_args)
		return -ENOENT;

	if (ctx->type == MDSS_MDP_WRITEBACK_TYPE_ROTATOR
			&& ctl->mdata->traffic_shaper_en)
		mdss_mdp_traffic_shaper(ctl, ctx, true);

	ret = mdss_mdp_writeback_addr_setup(ctx, wb_args->data);
	if (ret) {
		pr_err("writeback data setup error ctl=%d\n", ctl->num);
		return ret;
	}

	mdss_mdp_set_intr_callback(ctx->intr_type, ctx->intf_num,
		   mdss_mdp_writeback_intr_done, ctl);

	flush_bits |= ctl->flush_reg_data;
	flush_bits |= BIT(16); /* WB */
	mdp_wb_write(ctx, MDSS_MDP_REG_WB_DST_ADDR_SW_STATUS, ctl->is_secure);
	mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_FLUSH, flush_bits);

	INIT_COMPLETION(ctx->wb_comp);
	mdss_mdp_irq_enable(ctx->intr_type, ctx->intf_num);

	ret = mdss_iommu_ctrl(1);
	if (IS_ERR_VALUE(ret)) {
		pr_err("IOMMU attach failed\n");
		return ret;
	}
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);
	mdss_bus_bandwidth_ctrl(true);
	ctx->start_time = ktime_get();
	mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_START, 1);
	wmb();

	MDSS_XLOG(ctx->wb_num, ctx->type, ctx->xin_id, ctx->intf_num,
		ctx->dst_rect.w, ctx->dst_rect.h);
	pr_debug("ctx%d type:%d xin_id:%d intf_num:%d start\n",
		ctx->wb_num, ctx->type, ctx->xin_id, ctx->intf_num);

	trace_mdp_wb_display(ctx->wb_num, ctx->xin_id, ctx->intf_num,
		ctx->width, ctx->height, ctx->dst_rect.w, ctx->dst_rect.h,
		ctx->dst_fmt ? ctx->dst_fmt->format : -1);

	ctx->comp_cnt++;

	return 0;
}

int mdss_mdp_writeback_start(struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_writeback_ctx *ctx;
	u32 mem_sel;

	pr_debug("start ctl=%d\n", ctl->num);

	mem_sel = (ctl->opmode & 0xF) - 1;
	if (mem_sel < MDSS_MDP_MAX_WRITEBACK) {
		ctx = &wb_ctx_list[mem_sel];
		if (ctx->ref_cnt) {
			pr_err("writeback in use %d\n", mem_sel);
			return -EBUSY;
		}
		ctx->ref_cnt++;
	} else {
		pr_err("invalid writeback mode %d\n", mem_sel);
		return -EINVAL;
	}
	ctl->priv_data = ctx;
	ctx->wb_num = ctl->num;	/* wb num should match ctl num */
	ctx->base = ctl->wb_base;
	ctx->initialized = false;
	init_completion(&ctx->wb_comp);
	spin_lock_init(&ctx->wb_lock);
	INIT_LIST_HEAD(&ctx->vsync_handlers);

	if (ctx->type == MDSS_MDP_WRITEBACK_TYPE_ROTATOR)
		ctl->ops.prepare_fnc = mdss_mdp_writeback_prepare_rot;
	else {  /* wfd or line mode */
		ctl->ops.prepare_fnc = mdss_mdp_writeback_prepare_wfd;

		/* WB2 Intr Enable is BIT(2) in MDSS 1.8.0 */
		if ((ctl->mdata->mdp_rev == MDSS_MDP_HW_REV_108) ||
			 (ctl->mdata->mdp_rev == MDSS_MDP_HW_REV_111)) {
			ctx->intr_type = MDSS_MDP_IRQ_WB_ROT_COMP;
			ctx->intf_num = 2;
		}
	}
	ctl->ops.stop_fnc = mdss_mdp_writeback_stop;
	ctl->ops.display_fnc = mdss_mdp_writeback_display;
	ctl->ops.wait_fnc = mdss_mdp_wb_wait4comp;
	ctl->ops.add_vsync_handler = mdss_mdp_wb_add_vsync_handler;
	ctl->ops.remove_vsync_handler = mdss_mdp_wb_remove_vsync_handler;

	ctx->is_vbif_nrt = mdss_mdp_is_vbif_nrt(ctl->mdata->mdp_rev);

	return 0;
}

int mdss_mdp_writeback_display_commit(struct mdss_mdp_ctl *ctl, void *arg)
{
	if (ctl->shared_lock && !mutex_is_locked(ctl->shared_lock)) {
		pr_err("shared mutex is not locked before commit on ctl=%d\n",
			ctl->num);
		return -EINVAL;
	}

	if (ctl->mdata->mixer_switched) {
		if (ctl->mixer_left)
			ctl->mixer_left->params_changed++;
		if (ctl->mixer_right)
			ctl->mixer_right->params_changed++;
	}

	return mdss_mdp_display_commit(ctl, arg, NULL);
}
