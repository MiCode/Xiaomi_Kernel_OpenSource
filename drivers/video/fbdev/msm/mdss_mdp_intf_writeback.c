/* Copyright (c) 2012-2016, 2018, The Linux Foundation. All rights reserved.
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
#include "mdss_rotator_internal.h"
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
	struct completion wb_comp;
	int comp_cnt;

	u32 intr_type;
	u32 intf_num;

	u32 xin_id;
	u32 wr_lim;
	struct mdss_mdp_shared_reg_ctrl clk_ctrl;

	u32 opmode;
	struct mdss_mdp_format_params *dst_fmt;
	u16 img_width;
	u16 img_height;
	u16 width;
	u16 height;
	u16 frame_rate;
	enum mdss_mdp_csc_type csc_type;
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
		.intr_type = MDSS_MDP_IRQ_TYPE_WB_ROT_COMP,
		.intf_num = 0,
		.xin_id = 3,
		.clk_ctrl.reg_off = 0x2BC,
		.clk_ctrl.bit_off = 0x8,
	},
	{
		.type = MDSS_MDP_WRITEBACK_TYPE_ROTATOR,
		.intr_type = MDSS_MDP_IRQ_TYPE_WB_ROT_COMP,
		.intf_num = 1,
		.xin_id = 11,
		.clk_ctrl.reg_off = 0x2BC,
		.clk_ctrl.bit_off = 0xC,
	},
	{
		.type = MDSS_MDP_WRITEBACK_TYPE_LINE,
		.intr_type = MDSS_MDP_IRQ_TYPE_WB_ROT_COMP,
		.intf_num = 0,
		.xin_id = 3,
		.clk_ctrl.reg_off = 0x2BC,
		.clk_ctrl.bit_off = 0x8,
	},
	{
		.type = MDSS_MDP_WRITEBACK_TYPE_LINE,
		.intr_type = MDSS_MDP_IRQ_TYPE_WB_ROT_COMP,
		.intf_num = 1,
		.xin_id = 11,
		.clk_ctrl.reg_off = 0x2BC,
		.clk_ctrl.bit_off = 0xC,
	},
	{
		.type = MDSS_MDP_WRITEBACK_TYPE_WFD,
		.intr_type = MDSS_MDP_IRQ_TYPE_WB_WFD_COMP,
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

	ret = mdss_mdp_data_check(&data, &ctx->dst_planes, ctx->dst_fmt);
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

static int mdss_mdp_writeback_cdm_setup(struct mdss_mdp_writeback_ctx *ctx,
	struct mdss_mdp_cdm *cdm, struct mdss_mdp_format_params *fmt)
{
	struct mdp_cdm_cfg setup;

	switch (fmt->chroma_sample) {
	case MDSS_MDP_CHROMA_RGB:
		setup.horz_downsampling_type = MDP_CDM_CDWN_DISABLE;
		setup.vert_downsampling_type = MDP_CDM_CDWN_DISABLE;
		break;
	case MDSS_MDP_CHROMA_H2V1:
		setup.horz_downsampling_type = MDP_CDM_CDWN_COSITE;
		setup.vert_downsampling_type = MDP_CDM_CDWN_DISABLE;
		break;
	case MDSS_MDP_CHROMA_420:
		setup.horz_downsampling_type = MDP_CDM_CDWN_COSITE;
		setup.vert_downsampling_type = MDP_CDM_CDWN_OFFSITE;
		break;
	case MDSS_MDP_CHROMA_H1V2:
	default:
		pr_err("%s: unsupported chroma sampling type\n", __func__);
		return -EINVAL;
	}

	setup.out_format = fmt->format;
	setup.mdp_csc_bit_depth = MDP_CDM_CSC_8BIT;
	setup.output_width = ctx->width;
	setup.output_height = ctx->height;
	setup.csc_type = ctx->csc_type;
	return mdss_mdp_cdm_setup(cdm, &setup);
}

void mdss_mdp_set_wb_cdp(struct mdss_mdp_writeback_ctx *ctx,
	struct mdss_mdp_format_params *fmt)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	u32 cdp_settings = 0x0;

	/* Disable CDP for rotator in v1 */
	if (ctx->type == MDSS_MDP_WRITEBACK_TYPE_ROTATOR &&
			mdss_has_quirk(mdata, MDSS_QUIRK_ROTCDP))
		goto exit;

	cdp_settings = MDSS_MDP_CDP_ENABLE;

	if (!mdss_mdp_is_linear_format(fmt))
		cdp_settings |= MDSS_MDP_CDP_ENABLE_UBWCMETA;

	/* 64-transactions for line mode otherwise we keep 32 */
	if (ctx->type != MDSS_MDP_WRITEBACK_TYPE_ROTATOR)
		cdp_settings |= MDSS_MDP_CDP_AHEAD_64;

exit:
	mdp_wb_write(ctx, MDSS_MDP_REG_WB_CDP_CTRL, cdp_settings);
}

static int mdss_mdp_writeback_format_setup(struct mdss_mdp_writeback_ctx *ctx,
		u32 format, struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_format_params *fmt;
	u32 dst_format, pattern, ystride0, ystride1, outsize, chroma_samp;
	u32 dnsc_factor, write_config = 0;
	u32 opmode = ctx->opmode;
	bool rotation = false;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	int rc;

	pr_debug("wb_num=%d format=%d\n", ctx->wb_num, format);

	if (ctx->rot90)
		rotation = true;

	fmt = mdss_mdp_get_format_params(format);
	if (!fmt) {
		pr_err("wb format=%d not supported\n", format);
		return -EINVAL;
	}

	mdss_mdp_get_plane_sizes(fmt, ctx->img_width, ctx->img_height,
				 &ctx->dst_planes,
				 ctx->opmode & MDSS_MDP_OP_BWC_EN, rotation);

	ctx->dst_fmt = fmt;

	chroma_samp = fmt->chroma_sample;

	if (ctl->cdm) {
		rc = mdss_mdp_writeback_cdm_setup(ctx, ctl->cdm, fmt);
		if (rc) {
			pr_err("%s: CDM config failed with error %d\n",
				__func__, rc);
			return rc;
		}
		ctl->flush_bits |= BIT(26);
	}
	if (ctx->type != MDSS_MDP_WRITEBACK_TYPE_ROTATOR &&
		fmt->is_yuv && !ctl->cdm) {
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

	if (fmt->is_yuv && test_bit(MDSS_CAPS_YUV_CONFIG, mdata->mdss_caps_map))
		dst_format |= BIT(15);

	if (mdss_has_quirk(mdata, MDSS_QUIRK_FMT_PACK_PATTERN)) {
		pattern = (fmt->element[3] << 24) |
			  (fmt->element[2] << 15) |
			  (fmt->element[1] << 8)  |
			  (fmt->element[0] << 0);
	} else {
		pattern = (fmt->element[3] << 24) |
			  (fmt->element[2] << 16) |
			  (fmt->element[1] << 8)  |
			  (fmt->element[0] << 0);
	}

	dst_format |= (fmt->unpack_align_msb << 18) |
		      (fmt->unpack_tight << 17) |
		      ((fmt->unpack_count - 1) << 12) |
		      ((fmt->bpp - 1) << 9);

	dst_format |= (fmt->unpack_dx_format << 21);

	ystride0 = (ctx->dst_planes.ystride[0]) |
		   (ctx->dst_planes.ystride[1] << 16);
	ystride1 = (ctx->dst_planes.ystride[2]) |
		   (ctx->dst_planes.ystride[3] << 16);
	outsize = (ctx->dst_rect.h << 16) | ctx->dst_rect.w;

	if (mdss_mdp_is_ubwc_format(fmt)) {
		opmode |= BIT(0);
		dst_format |= BIT(31);
		if (mdata->highest_bank_bit)
			write_config |= (mdata->highest_bank_bit << 8);
		if (fmt->format == MDP_RGB_565_UBWC)
			write_config |= 0x8;
	}

	if (ctx->type == MDSS_MDP_WRITEBACK_TYPE_ROTATOR
			&& mdata->has_rot_dwnscale) {
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
	mdp_wb_write(ctx, MDSS_MDP_REG_WB_DST_WRITE_CONFIG, write_config);

	/* configure CDP */
	if (test_bit(MDSS_QOS_CDP, mdata->mdss_qos_map))
		mdss_mdp_set_wb_cdp(ctx, fmt);

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
	ctx->img_width = ctl->width;
	ctx->img_height = ctl->height;
	ctx->width = ctl->width;
	ctx->height = ctl->height;
	ctx->frame_rate = ctl->frame_rate;
	ctx->csc_type = ctl->csc_type;
	ctx->dst_rect.x = 0;
	ctx->dst_rect.y = 0;
	ctx->dst_rect.w = ctx->width;
	ctx->dst_rect.h = ctx->height;

	ret = mdss_mdp_writeback_format_setup(ctx, ctl->dst_format, ctl);
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
	struct mdss_rot_entry *entry;
	struct mdp_rotation_item *item;
	struct mdss_rot_perf *perf;
	struct mdss_data_type *mdata;
	u32 format;

	ctx = (struct mdss_mdp_writeback_ctx *) ctl->priv_data;
	if (!ctx)
		return -ENODEV;
	wb_args = (struct mdss_mdp_writeback_arg *) arg;
	if (!wb_args)
		return -ENOENT;

	entry = (struct mdss_rot_entry *) wb_args->priv_data;
	if (!entry) {
		pr_err("unable to retrieve rot session ctl=%d\n", ctl->num);
		return -ENODEV;
	}
	item = &entry->item;
	perf = entry->perf;
	mdata = ctl->mdata;
	if (!mdata) {
		pr_err("no mdata attached to ctl=%d", ctl->num);
		return -ENODEV;
	}
	pr_debug("rot setup wb_num=%d\n", ctx->wb_num);

	ctx->opmode = BIT(6); /* ROT EN */
	if (ctl->mdata->rot_block_size == 128)
		ctx->opmode |= BIT(4); /* block size 128 */

	ctx->bwc_mode = 0;
	ctx->opmode |= ctx->bwc_mode;

	ctx->img_width = item->output.width;
	ctx->img_height = item->output.height;
	ctx->width = ctx->dst_rect.w = item->dst_rect.w;
	ctx->height = ctx->dst_rect.h = item->dst_rect.h;
	ctx->dst_rect.x = item->dst_rect.x;
	ctx->dst_rect.y = item->dst_rect.y;
	ctx->frame_rate = perf->config.frame_rate;
	ctx->dnsc_factor_w = entry->dnsc_factor_w;
	ctx->dnsc_factor_h = entry->dnsc_factor_h;

	ctx->rot90 = !!(item->flags & MDP_ROTATION_90);

	format = item->output.format;

	if (ctx->rot90)
		ctx->opmode |= BIT(5); /* ROT 90 */

	return mdss_mdp_writeback_format_setup(ctx, format, ctl);
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

	if (ctl->cdm) {
		if (!mdss_mdp_cdm_destroy(ctl->cdm))
			mdss_mdp_ctl_write(ctl,
				MDSS_MDP_REG_CTL_FLUSH, BIT(26));
		ctl->cdm = NULL;
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
	u32 status, mask, isr;

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
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	ot_params.xin_id = ctx->xin_id;
	ot_params.num = ctx->wb_num;
	ot_params.width = ctx->width;
	ot_params.height = ctx->height;
	ot_params.frame_rate = ctx->frame_rate;
	ot_params.reg_off_vbif_lim_conf = MMSS_VBIF_WR_LIM_CONF;
	ot_params.reg_off_mdp_clk_ctrl = ctx->clk_ctrl.reg_off;
	ot_params.bit_off_mdp_clk_ctrl = ctx->clk_ctrl.bit_off;
	ot_params.is_rot = (ctx->type == MDSS_MDP_WRITEBACK_TYPE_ROTATOR);
	ot_params.is_wb = true;
	ot_params.is_yuv = ctx->dst_fmt->is_yuv;
	ot_params.is_vbif_nrt = mdss_mdp_is_nrt_vbif_base_defined(mdata);

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
	MDSS_XLOG(ctl->intf_num, flush_bits);

	reinit_completion(&ctx->wb_comp);
	mdss_mdp_irq_enable(ctx->intr_type, ctx->intf_num);

	ret = mdss_iommu_ctrl(1);
	if (IS_ERR_VALUE((unsigned long)ret)) {
		pr_err("IOMMU attach failed\n");
		return ret;
	}
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);
	mdss_bus_bandwidth_ctrl(true);
	ctx->start_time = ktime_get();
	mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_START, 1);
	/* make sure MDP writeback is enabled */
	wmb();

	MDSS_XLOG(ctx->wb_num, ctx->type, ctx->xin_id, ctx->intf_num,
		ctx->dst_rect.w, ctx->dst_rect.h);
	pr_debug("ctx%d type:%d xin_id:%d intf_num:%d start\n",
		ctx->wb_num, ctx->type, ctx->xin_id, ctx->intf_num);

	ctx->comp_cnt++;

	return 0;
}

int mdss_mdp_writeback_start(struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_writeback_ctx *ctx;
	struct mdss_mdp_writeback *wb;
	u32 mem_sel;
	u32 mixer_type = MDSS_MDP_MIXER_TYPE_UNUSED;
	struct mdss_mdp_format_params *fmt = NULL;
	bool is_rot;

	pr_debug("start ctl=%d\n", ctl->num);

	if (!ctl->wb) {
		pr_debug("wb not setup in the ctl\n");
		return 0;
	}

	wb = ctl->wb;
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

	fmt = mdss_mdp_get_format_params(ctl->dst_format);
	if (!fmt)
		return -EINVAL;

	is_rot = (ctx->type == MDSS_MDP_WRITEBACK_TYPE_ROTATOR) ? true : false;

	if (ctl->mixer_left) {
		mixer_type = ctl->mixer_left->type;
		/*
		 * If the WB mixer is dedicated, the rotator uses a virtual
		 * mixer. Mark the mixer_type as UNUSED in such cases.
		 */
		if ((mixer_type == MDSS_MDP_MIXER_TYPE_WRITEBACK) && is_rot)
			mixer_type = MDSS_MDP_MIXER_TYPE_UNUSED;
	}

	if (mdss_mdp_is_cdm_supported(ctl->mdata, ctl->intf_type,
		mixer_type) && fmt->is_yuv) {
		ctl->cdm = mdss_mdp_cdm_init(ctl, MDP_CDM_CDWN_OUTPUT_WB);
		if (IS_ERR_OR_NULL(ctl->cdm)) {
			pr_err("cdm block already in use\n");
			ctl->cdm = NULL;
			return -EBUSY;
		}
	}
	ctl->priv_data = ctx;
	ctx->wb_num = wb->num;
	ctx->base = wb->base;
	ctx->initialized = false;
	init_completion(&ctx->wb_comp);
	spin_lock_init(&ctx->wb_lock);
	INIT_LIST_HEAD(&ctx->vsync_handlers);

	if (ctx->type == MDSS_MDP_WRITEBACK_TYPE_ROTATOR)
		ctl->ops.prepare_fnc = mdss_mdp_writeback_prepare_rot;
	else {  /* wfd or line mode */
		ctl->ops.prepare_fnc = mdss_mdp_writeback_prepare_wfd;

		/* WB2 Intr Enable is BIT(2) in MDSS 1.8.0 */
		if (ctl->mdata->mdp_rev == MDSS_MDP_HW_REV_108) {
			ctx->intr_type = MDSS_MDP_IRQ_TYPE_WB_ROT_COMP;
			ctx->intf_num = 2;
		}
	}
	ctl->ops.stop_fnc = mdss_mdp_writeback_stop;
	ctl->ops.display_fnc = mdss_mdp_writeback_display;
	ctl->ops.wait_fnc = mdss_mdp_wb_wait4comp;
	ctl->ops.add_vsync_handler = mdss_mdp_wb_add_vsync_handler;
	ctl->ops.remove_vsync_handler = mdss_mdp_wb_remove_vsync_handler;

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
