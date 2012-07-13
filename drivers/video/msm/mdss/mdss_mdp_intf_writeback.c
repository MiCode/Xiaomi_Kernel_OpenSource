/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#include "mdss_fb.h"
#include "mdss_mdp.h"
#include "mdss_mdp_rotator.h"

#define ROT_BLK_SIZE	128

enum mdss_mdp_writeback_type {
	MDSS_MDP_WRITEBACK_TYPE_ROTATOR,
	MDSS_MDP_WRITEBACK_TYPE_LINE,
	MDSS_MDP_WRITEBACK_TYPE_WFD,
};

struct mdss_mdp_writeback_ctx {
	u32 wb_num;
	u8 ref_cnt;
	u8 type;

	u32 intr_type;
	u32 intf_num;

	u32 opmode;
	u32 format;
	u16 width;
	u16 height;
	u8 rot90;

	int initialized;

	struct mdss_mdp_plane_sizes dst_planes;

	void (*callback_fnc) (void *arg);
	void *callback_arg;
};

static struct mdss_mdp_writeback_ctx wb_ctx_list[MDSS_MDP_MAX_WRITEBACK] = {
	{
		.type = MDSS_MDP_WRITEBACK_TYPE_ROTATOR,
		.intr_type = MDSS_MDP_IRQ_WB_ROT_COMP,
		.intf_num = 0,
	},
	{
		.type = MDSS_MDP_WRITEBACK_TYPE_ROTATOR,
		.intr_type = MDSS_MDP_IRQ_WB_ROT_COMP,
		.intf_num = 1,
	},
	{
		.type = MDSS_MDP_WRITEBACK_TYPE_LINE,
		.intr_type = MDSS_MDP_IRQ_WB_ROT_COMP,
		.intf_num = 0,
	},
	{
		.type = MDSS_MDP_WRITEBACK_TYPE_LINE,
		.intr_type = MDSS_MDP_IRQ_WB_ROT_COMP,
		.intf_num = 1,
	},
	{
		.type = MDSS_MDP_WRITEBACK_TYPE_WFD,
		.intr_type = MDSS_MDP_IRQ_WB_WFD,
		.intf_num = 0,
	},
};

static int mdss_mdp_writeback_addr_setup(struct mdss_mdp_writeback_ctx *ctx,
					 struct mdss_mdp_data *data)
{
	int off, ret;

	if (!data)
		return -EINVAL;

	pr_debug("wb_num=%d addr=0x%x\n", ctx->wb_num, data->p[0].addr);

	ret = mdss_mdp_data_check(data, &ctx->dst_planes);
	if (ret)
		return ret;

	off = MDSS_MDP_REG_WB_OFFSET(ctx->wb_num);
	MDSS_MDP_REG_WRITE(off + MDSS_MDP_REG_WB_DST0_ADDR, data->p[0].addr);
	MDSS_MDP_REG_WRITE(off + MDSS_MDP_REG_WB_DST1_ADDR, data->p[1].addr);
	MDSS_MDP_REG_WRITE(off + MDSS_MDP_REG_WB_DST2_ADDR, data->p[2].addr);
	MDSS_MDP_REG_WRITE(off + MDSS_MDP_REG_WB_DST3_ADDR, data->p[3].addr);

	return 0;
}

static int mdss_mdp_writeback_format_setup(struct mdss_mdp_writeback_ctx *ctx)
{
	struct mdss_mdp_format_params *fmt;
	u32 dst_format, pattern, ystride0, ystride1, outsize, chroma_samp;
	int off;
	u32 opmode = ctx->opmode;

	pr_debug("wb_num=%d format=%d\n", ctx->wb_num, ctx->format);

	mdss_mdp_get_plane_sizes(ctx->format, ctx->width, ctx->height,
				 &ctx->dst_planes);

	fmt = mdss_mdp_get_format_params(ctx->format);
	if (!fmt) {
		pr_err("wb format=%d not supported\n", ctx->format);
		return -EINVAL;
	}

	chroma_samp = fmt->chroma_sample;

	if (ctx->type != MDSS_MDP_WRITEBACK_TYPE_ROTATOR && fmt->is_yuv) {
		mdss_mdp_csc_setup(MDSS_MDP_BLOCK_WB, ctx->wb_num, 0,
				   MDSS_MDP_CSC_RGB2YUV);
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

	if (fmt->alpha_enable)
		dst_format |= BIT(8); /* DSTC3_EN */

	if (fmt->fetch_planes != MDSS_MDP_PLANE_PLANAR) {
		pattern = (fmt->element[3] << 24) | (fmt->element[2] << 15) |
			(fmt->element[1] << 8) | (fmt->element[0] << 0);
		dst_format |= (fmt->unpack_align_msb << 18) |
			      (fmt->unpack_tight << 17) |
			      ((fmt->unpack_count - 1) << 12) |
			      ((fmt->bpp - 1) << 9);
	} else {
		pattern = 0;
	}

	ystride0 = (ctx->dst_planes.ystride[0]) |
		   (ctx->dst_planes.ystride[1] << 16);
	ystride1 = (ctx->dst_planes.ystride[2]) |
		   (ctx->dst_planes.ystride[3] << 16);
	outsize = (ctx->height << 16) | ctx->width;

	off = MDSS_MDP_REG_WB_OFFSET(ctx->wb_num);
	MDSS_MDP_REG_WRITE(off + MDSS_MDP_REG_WB_DST_FORMAT, dst_format);
	MDSS_MDP_REG_WRITE(off + MDSS_MDP_REG_WB_DST_OP_MODE, opmode);
	MDSS_MDP_REG_WRITE(off + MDSS_MDP_REG_WB_DST_PACK_PATTERN, pattern);
	MDSS_MDP_REG_WRITE(off + MDSS_MDP_REG_WB_DST_YSTRIDE0, ystride0);
	MDSS_MDP_REG_WRITE(off + MDSS_MDP_REG_WB_DST_YSTRIDE1, ystride1);
	MDSS_MDP_REG_WRITE(off + MDSS_MDP_REG_WB_OUT_SIZE, outsize);

	return 0;
}

static int mdss_mdp_writeback_prepare_wfd(struct mdss_mdp_ctl *ctl, void *arg)
{
	struct mdss_mdp_writeback_ctx *ctx;
	int ret;

	ctx = (struct mdss_mdp_writeback_ctx *) ctl->priv_data;
	if (!ctx)
		return -ENODEV;

	if (ctx->initialized) /* already set */
		return 0;

	pr_debug("wfd setup ctl=%d\n", ctl->num);

	ctx->opmode = 0;
	ctx->format = ctl->dst_format;
	ctx->width = ctl->width;
	ctx->height = ctl->height;

	ret = mdss_mdp_writeback_format_setup(ctx);
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
	pr_debug("rot setup wb_num=%d\n", ctx->wb_num);

	ctx->opmode = BIT(6); /* ROT EN */
	if (ROT_BLK_SIZE == 128)
		ctx->opmode |= BIT(4); /* block size 128 */

	ctx->opmode |= rot->bwc_mode;

	ctx->width = rot->src_rect.w;
	ctx->height = rot->src_rect.h;

	ctx->format = rot->format;

	ctx->rot90 = !!(rot->rotations & MDP_ROT_90);
	if (ctx->rot90) {
		ctx->opmode |= BIT(5); /* ROT 90 */
		swap(ctx->width, ctx->height);
	}

	if (mdss_mdp_writeback_format_setup(ctx))
		return -EINVAL;

	return 0;
}

static int mdss_mdp_writeback_stop(struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_writeback_ctx *ctx;

	pr_debug("stop ctl=%d\n", ctl->num);

	ctx = (struct mdss_mdp_writeback_ctx *) ctl->priv_data;
	if (ctx) {
		ctl->priv_data = NULL;
		ctx->ref_cnt--;
	}

	return 0;
}

static void mdss_mdp_writeback_intr_done(void *arg)
{
	struct mdss_mdp_writeback_ctx *ctx;

	ctx = (struct mdss_mdp_writeback_ctx *) arg;
	if (!ctx) {
		pr_err("invalid ctx\n");
		return;
	}

	pr_debug("intr wb_num=%d\n", ctx->wb_num);

	mdss_mdp_irq_disable_nosync(ctx->intr_type, ctx->intf_num);
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, true);

	if (ctx->callback_fnc)
		ctx->callback_fnc(ctx->callback_arg);
}

static int mdss_mdp_writeback_display(struct mdss_mdp_ctl *ctl, void *arg)
{
	struct mdss_mdp_writeback_ctx *ctx;
	struct mdss_mdp_writeback_arg *wb_args;
	u32 flush_bits;
	int ret;

	ctx = (struct mdss_mdp_writeback_ctx *) ctl->priv_data;
	if (!ctx)
		return -ENODEV;

	wb_args = (struct mdss_mdp_writeback_arg *) arg;
	if (!wb_args)
		return -ENOENT;

	ret = mdss_mdp_writeback_addr_setup(ctx, wb_args->data);
	if (ret) {
		pr_err("writeback data setup error ctl=%d\n", ctl->num);
		return ret;
	}

	ctx->callback_fnc = wb_args->callback_fnc;
	ctx->callback_arg = wb_args->priv_data;

	flush_bits = BIT(16); /* WB */
	mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_FLUSH, flush_bits);

	mdss_mdp_set_intr_callback(ctx->intr_type, ctx->intf_num,
				   mdss_mdp_writeback_intr_done, ctx);
	mdss_mdp_irq_enable(ctx->intr_type, ctx->intf_num);

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);
	mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_START, 1);
	wmb();

	return 0;
}

int mdss_mdp_writeback_start(struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_writeback_ctx *ctx;
	u32 mem_sel;
	int ret = 0;

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
	ctx->initialized = false;

	if (ctx->type == MDSS_MDP_WRITEBACK_TYPE_ROTATOR)
		ctl->prepare_fnc = mdss_mdp_writeback_prepare_rot;
	else /* wfd or line mode */
		ctl->prepare_fnc = mdss_mdp_writeback_prepare_wfd;
	ctl->stop_fnc = mdss_mdp_writeback_stop;
	ctl->display_fnc = mdss_mdp_writeback_display;

	return ret;
}
