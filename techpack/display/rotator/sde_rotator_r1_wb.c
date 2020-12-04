// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2019, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/delay.h>
#include <linux/interrupt.h>

#include "sde_rotator_r1_hwio.h"
#include "sde_rotator_util.h"
#include "sde_rotator_r1_internal.h"
#include "sde_rotator_core.h"

/* wait for at most 2 vsync for lowest refresh rate (24hz) */
#define KOFF_TIMEOUT msecs_to_jiffies(84)

/*
 * if BWC enabled and format is H1V2 or 420, do not use site C or I.
 * Hence, set the bits 29:26 in format register, as zero.
 */
#define BWC_FMT_MASK	0xC3FFFFFF
#define MDSS_DEFAULT_OT_SETTING    0x10

enum sde_mdp_writeback_type {
	SDE_MDP_WRITEBACK_TYPE_ROTATOR,
	SDE_MDP_WRITEBACK_TYPE_LINE,
	SDE_MDP_WRITEBACK_TYPE_WFD,
};

struct sde_mdp_writeback_ctx {
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
	struct sde_mdp_shared_reg_ctrl clk_ctrl;

	u32 opmode;
	struct sde_mdp_format_params *dst_fmt;
	u16 img_width;
	u16 img_height;
	u16 width;
	u16 height;
	struct sde_rect dst_rect;

	u32 dnsc_factor_w;
	u32 dnsc_factor_h;

	u8 rot90;
	u32 bwc_mode;

	struct sde_mdp_plane_sizes dst_planes;

	ktime_t start_time;
	ktime_t end_time;
	u32 offset;
};

static struct sde_mdp_writeback_ctx wb_ctx_list[SDE_MDP_MAX_WRITEBACK] = {
	{
		.type = SDE_MDP_WRITEBACK_TYPE_ROTATOR,
		.intr_type = SDE_MDP_IRQ_WB_ROT_COMP,
		.intf_num = 0,
		.xin_id = 3,
		.clk_ctrl.reg_off = 0x2BC,
		.clk_ctrl.bit_off = 0x8,
	},
	{
		.type = SDE_MDP_WRITEBACK_TYPE_ROTATOR,
		.intr_type = SDE_MDP_IRQ_WB_ROT_COMP,
		.intf_num = 1,
		.xin_id = 11,
		.clk_ctrl.reg_off = 0x2BC,
		.clk_ctrl.bit_off = 0xC,
	},
};

static inline void sde_wb_write(struct sde_mdp_writeback_ctx *ctx,
				u32 reg, u32 val)
{
	SDEROT_DBG("wb%d:%6.6x:%8.8x\n", ctx->wb_num, ctx->offset + reg, val);
	writel_relaxed(val, ctx->base + reg);
}

static int sde_mdp_writeback_addr_setup(struct sde_mdp_writeback_ctx *ctx,
					 const struct sde_mdp_data *in_data)
{
	int ret;
	struct sde_mdp_data data;

	if (!in_data)
		return -EINVAL;
	data = *in_data;

	SDEROT_DBG("wb_num=%d addr=0x%pa\n", ctx->wb_num, &data.p[0].addr);

	ret = sde_mdp_data_check(&data, &ctx->dst_planes, ctx->dst_fmt);
	if (ret)
		return ret;

	sde_rot_data_calc_offset(&data, ctx->dst_rect.x, ctx->dst_rect.y,
			&ctx->dst_planes, ctx->dst_fmt);

	if ((ctx->dst_fmt->fetch_planes == SDE_MDP_PLANE_PLANAR) &&
			(ctx->dst_fmt->element[0] == C1_B_Cb))
		swap(data.p[1].addr, data.p[2].addr);

	sde_wb_write(ctx, SDE_MDP_REG_WB_DST0_ADDR, data.p[0].addr);
	sde_wb_write(ctx, SDE_MDP_REG_WB_DST1_ADDR, data.p[1].addr);
	sde_wb_write(ctx, SDE_MDP_REG_WB_DST2_ADDR, data.p[2].addr);
	sde_wb_write(ctx, SDE_MDP_REG_WB_DST3_ADDR, data.p[3].addr);

	return 0;
}

static int sde_mdp_writeback_format_setup(struct sde_mdp_writeback_ctx *ctx,
		u32 format, struct sde_mdp_ctl *ctl)
{
	struct sde_mdp_format_params *fmt;
	u32 dst_format, pattern, ystride0, ystride1, outsize, chroma_samp;
	u32 dnsc_factor, write_config = 0;
	u32 opmode = ctx->opmode;
	bool rotation = false;
	struct sde_rot_data_type *mdata = sde_rot_get_mdata();

	SDEROT_DBG("wb_num=%d format=%d\n", ctx->wb_num, format);

	if (ctx->rot90)
		rotation = true;

	fmt = sde_get_format_params(format);
	if (!fmt) {
		SDEROT_ERR("wb format=%d not supported\n", format);
		return -EINVAL;
	}

	sde_mdp_get_plane_sizes(fmt, ctx->img_width, ctx->img_height,
				 &ctx->dst_planes,
				 ctx->opmode & SDE_MDP_OP_BWC_EN, rotation);

	ctx->dst_fmt = fmt;

	chroma_samp = fmt->chroma_sample;

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

	if (fmt->is_yuv)
		dst_format |= BIT(15);

	pattern = (fmt->element[3] << 24) |
			  (fmt->element[2] << 16) |
			  (fmt->element[1] << 8)  |
			  (fmt->element[0] << 0);

	dst_format |= (fmt->unpack_align_msb << 18) |
		      (fmt->unpack_tight << 17) |
		      ((fmt->unpack_count - 1) << 12) |
		      ((fmt->bpp - 1) << 9);

	ystride0 = (ctx->dst_planes.ystride[0]) |
		   (ctx->dst_planes.ystride[1] << 16);
	ystride1 = (ctx->dst_planes.ystride[2]) |
		   (ctx->dst_planes.ystride[3] << 16);
	outsize = (ctx->dst_rect.h << 16) | ctx->dst_rect.w;

	if (sde_mdp_is_ubwc_format(fmt)) {
		opmode |= BIT(0);

		dst_format |= BIT(31);
		if (mdata->highest_bank_bit)
			write_config |= (mdata->highest_bank_bit << 8);

		if (fmt->format == SDE_PIX_FMT_RGB_565_UBWC)
			write_config |= 0x8;
	}

	if (ctx->type == SDE_MDP_WRITEBACK_TYPE_ROTATOR) {
		dnsc_factor = (ctx->dnsc_factor_h) | (ctx->dnsc_factor_w << 16);
		sde_wb_write(ctx, SDE_MDP_REG_WB_ROTATOR_PIPE_DOWNSCALER,
								dnsc_factor);
	}
	sde_wb_write(ctx, SDE_MDP_REG_WB_ALPHA_X_VALUE, 0xFF);
	sde_wb_write(ctx, SDE_MDP_REG_WB_DST_FORMAT, dst_format);
	sde_wb_write(ctx, SDE_MDP_REG_WB_DST_OP_MODE, opmode);
	sde_wb_write(ctx, SDE_MDP_REG_WB_DST_PACK_PATTERN, pattern);
	sde_wb_write(ctx, SDE_MDP_REG_WB_DST_YSTRIDE0, ystride0);
	sde_wb_write(ctx, SDE_MDP_REG_WB_DST_YSTRIDE1, ystride1);
	sde_wb_write(ctx, SDE_MDP_REG_WB_OUT_SIZE, outsize);
	sde_wb_write(ctx, SDE_MDP_REG_WB_DST_WRITE_CONFIG, write_config);
	return 0;
}

static int sde_mdp_writeback_prepare_rot(struct sde_mdp_ctl *ctl, void *arg)
{
	struct sde_mdp_writeback_ctx *ctx;
	struct sde_mdp_writeback_arg *wb_args;
	struct sde_rot_entry *entry;
	struct sde_rotation_item *item;
	struct sde_rot_data_type *mdata;
	u32 format;

	ctx = (struct sde_mdp_writeback_ctx *) ctl->priv_data;
	if (!ctx)
		return -ENODEV;
	wb_args = (struct sde_mdp_writeback_arg *) arg;
	if (!wb_args)
		return -ENOENT;

	entry = (struct sde_rot_entry *) wb_args->priv_data;
	if (!entry) {
		SDEROT_ERR("unable to retrieve rot session ctl=%d\n", ctl->num);
		return -ENODEV;
	}
	item = &entry->item;
	mdata = ctl->mdata;
	if (!mdata) {
		SDEROT_ERR("no mdata attached to ctl=%d", ctl->num);
		return -ENODEV;
	}
	SDEROT_DBG("rot setup wb_num=%d\n", ctx->wb_num);

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
	ctx->dnsc_factor_w = entry->dnsc_factor_w;
	ctx->dnsc_factor_h = entry->dnsc_factor_h;

	ctx->rot90 = !!(item->flags & SDE_ROTATION_90);

	format = item->output.format;

	if (ctx->rot90)
		ctx->opmode |= BIT(5); /* ROT 90 */

	return sde_mdp_writeback_format_setup(ctx, format, ctl);
}

static int sde_mdp_writeback_stop(struct sde_mdp_ctl *ctl,
	int panel_power_state)
{
	struct sde_mdp_writeback_ctx *ctx;

	SDEROT_DBG("stop ctl=%d\n", ctl->num);

	ctx = (struct sde_mdp_writeback_ctx *) ctl->priv_data;
	if (ctx) {
		sde_mdp_set_intr_callback(ctx->intr_type, ctx->intf_num,
				NULL, NULL);

		complete_all(&ctx->wb_comp);

		ctl->priv_data = NULL;
		ctx->ref_cnt--;
	}

	return 0;
}

static void sde_mdp_writeback_intr_done(void *arg)
{
	struct sde_mdp_ctl *ctl = arg;
	struct sde_mdp_writeback_ctx *ctx = ctl->priv_data;

	if (!ctx) {
		SDEROT_ERR("invalid ctx\n");
		return;
	}

	SDEROT_DBG("intr wb_num=%d\n", ctx->wb_num);
	if (ctl->irq_num >= 0)
		disable_irq_nosync(ctl->irq_num);
	complete_all(&ctx->wb_comp);
}

static int sde_mdp_wb_wait4comp(struct sde_mdp_ctl *ctl, void *arg)
{
	struct sde_mdp_writeback_ctx *ctx;
	int rc = 0;
	u64 rot_time = 0;
	u32 status, mask, isr = 0;

	ctx = (struct sde_mdp_writeback_ctx *) ctl->priv_data;
	if (!ctx) {
		SDEROT_ERR("invalid ctx\n");
		return -ENODEV;
	}

	if (ctx->comp_cnt == 0)
		return rc;

	if (ctl->irq_num >= 0) {
		rc = wait_for_completion_timeout(&ctx->wb_comp,
				KOFF_TIMEOUT);
		sde_mdp_set_intr_callback(ctx->intr_type, ctx->intf_num,
			NULL, NULL);

		if (rc == 0) {
			mask = BIT(ctx->intr_type + ctx->intf_num);

			isr = readl_relaxed(ctl->mdata->mdp_base +
						SDE_MDP_REG_INTR_STATUS);
			status = mask & isr;

			SDEROT_INFO_ONCE(
					"mask: 0x%x, isr: 0x%x, status: 0x%x\n",
					mask, isr, status);

			if (status) {
				SDEROT_WARN("wb done but irq not triggered\n");
				writel_relaxed(BIT(ctl->wb->num),
						ctl->mdata->mdp_base +
						SDE_MDP_REG_INTR_CLEAR);
				sde_mdp_writeback_intr_done(ctl);
				rc = 0;
			} else {
				rc = -ENODEV;
				WARN(1, "wb timeout (%d) ctl=%d\n",
								rc, ctl->num);
				if (ctl->irq_num >= 0)
					disable_irq_nosync(ctl->irq_num);
			}
		} else {
			rc = 0;
		}
	} else {
		/* use polling if interrupt is not available */
		int cnt = 200;

		mask = BIT(ctl->wb->num);
		do {
			udelay(500);
			isr = readl_relaxed(ctl->mdata->mdp_base +
					SDE_MDP_REG_INTR_STATUS);
			status = mask & isr;
			cnt--;
		} while (cnt > 0 && !status);
		writel_relaxed(mask, ctl->mdata->mdp_base +
				SDE_MDP_REG_INTR_CLEAR);

		rc = (status) ? 0 : -ENODEV;
	}

	if (rc == 0)
		ctx->end_time = ktime_get();

	sde_smmu_ctrl(0);
	ctx->comp_cnt--;

	if (!rc) {
		rot_time = (u64)ktime_to_us(ctx->end_time) -
				(u64)ktime_to_us(ctx->start_time);
		SDEROT_DBG(
			"ctx%d type:%d xin_id:%d intf_num:%d took %llu microsecs\n",
			ctx->wb_num, ctx->type, ctx->xin_id,
				ctx->intf_num, rot_time);
	}

	SDEROT_DBG("s:%8.8x %s t:%llu c:%d\n", isr,
			(rc)?"Timeout":"Done", rot_time, ctx->comp_cnt);
	return rc;
}

static void sde_mdp_set_ot_limit_wb(struct sde_mdp_writeback_ctx *ctx)
{
	struct sde_mdp_set_ot_params ot_params = {0,};

	ot_params.xin_id = ctx->xin_id;
	ot_params.num = ctx->wb_num;
	ot_params.width = ctx->width;
	ot_params.height = ctx->height;
	ot_params.fps = 60;
	ot_params.reg_off_vbif_lim_conf = MMSS_VBIF_WR_LIM_CONF;
	ot_params.reg_off_mdp_clk_ctrl = ctx->clk_ctrl.reg_off;
	ot_params.bit_off_mdp_clk_ctrl = ctx->clk_ctrl.bit_off;
	ot_params.fmt = (ctx->dst_fmt) ? ctx->dst_fmt->format : 0;

	sde_mdp_set_ot_limit(&ot_params);
}

static int sde_mdp_writeback_display(struct sde_mdp_ctl *ctl, void *arg)
{
	struct sde_mdp_writeback_ctx *ctx;
	struct sde_mdp_writeback_arg *wb_args;
	u32 flush_bits = 0;
	int ret;

	if (!ctl || !ctl->mdata)
		return -ENODEV;

	ctx = (struct sde_mdp_writeback_ctx *) ctl->priv_data;
	if (!ctx)
		return -ENODEV;

	if (ctx->comp_cnt) {
		SDEROT_ERR("previous kickoff not completed yet, ctl=%d\n",
					ctl->num);
		return -EPERM;
	}

	if (ctl->mdata->default_ot_wr_limit ||
			ctl->mdata->default_ot_rd_limit)
		sde_mdp_set_ot_limit_wb(ctx);

	wb_args = (struct sde_mdp_writeback_arg *) arg;
	if (!wb_args)
		return -ENOENT;

	ret = sde_mdp_writeback_addr_setup(ctx, wb_args->data);
	if (ret) {
		SDEROT_ERR("writeback data setup error ctl=%d\n", ctl->num);
		return ret;
	}

	sde_mdp_set_intr_callback(ctx->intr_type, ctx->intf_num,
		   sde_mdp_writeback_intr_done, ctl);

	flush_bits |= ctl->flush_reg_data;
	flush_bits |= BIT(16); /* WB */
	sde_wb_write(ctx, SDE_MDP_REG_WB_DST_ADDR_SW_STATUS, ctl->is_secure);
	sde_mdp_ctl_write(ctl, SDE_MDP_REG_CTL_FLUSH, flush_bits);

	reinit_completion(&ctx->wb_comp);
	if (ctl->irq_num >= 0)
		enable_irq(ctl->irq_num);
	ret = sde_smmu_ctrl(1);
	if (ret < 0) {
		SDEROT_ERR("IOMMU attach failed\n");
		return ret;
	}

	ctx->start_time = ktime_get();
	sde_mdp_ctl_write(ctl, SDE_MDP_REG_CTL_START, 1);
	/* ensure that start command is issued after the barrier */
	wmb();

	SDEROT_DBG("ctx%d type:%d xin_id:%d intf_num:%d start\n",
		ctx->wb_num, ctx->type, ctx->xin_id, ctx->intf_num);

	ctx->comp_cnt++;

	return 0;
}

int sde_mdp_writeback_start(struct sde_mdp_ctl *ctl)
{
	struct sde_mdp_writeback_ctx *ctx;
	struct sde_mdp_writeback *wb;
	u32 mem_sel;

	SDEROT_DBG("start ctl=%d\n", ctl->num);

	if (!ctl->wb) {
		SDEROT_DBG("wb not setup in the ctl\n");
		return 0;
	}

	wb = ctl->wb;
	mem_sel = (ctl->opmode & 0xF) - 1;
	if (mem_sel < SDE_MDP_MAX_WRITEBACK) {
		ctx = &wb_ctx_list[mem_sel];
		if (ctx->ref_cnt) {
			SDEROT_ERR("writeback in use %d\n", mem_sel);
			return -EBUSY;
		}
		ctx->ref_cnt++;
	} else {
		SDEROT_ERR("invalid writeback mode %d\n", mem_sel);
		return -EINVAL;
	}

	ctl->priv_data = ctx;
	ctx->wb_num = wb->num;
	ctx->base = wb->base;
	ctx->offset = wb->offset;

	init_completion(&ctx->wb_comp);

	if (ctx->type == SDE_MDP_WRITEBACK_TYPE_ROTATOR)
		ctl->ops.prepare_fnc = sde_mdp_writeback_prepare_rot;

	ctl->ops.stop_fnc = sde_mdp_writeback_stop;
	ctl->ops.display_fnc = sde_mdp_writeback_display;
	ctl->ops.wait_fnc = sde_mdp_wb_wait4comp;

	return 0;
}

int sde_mdp_writeback_display_commit(struct sde_mdp_ctl *ctl, void *arg)
{
	return sde_mdp_display_commit(ctl, arg, NULL);
}
