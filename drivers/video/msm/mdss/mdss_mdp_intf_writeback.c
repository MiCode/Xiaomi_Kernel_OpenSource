/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

	u32 opmode;
	struct mdss_mdp_format_params *dst_fmt;
	u16 width;
	u16 height;
	struct mdss_mdp_img_rect dst_rect;

	u8 rot90;
	u32 bwc_mode;
	int initialized;

	struct mdss_mdp_plane_sizes dst_planes;

	void (*callback_fnc) (void *arg);
	void *callback_arg;
	spinlock_t wb_lock;
	struct list_head vsync_handlers;
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

	if (ctx->bwc_mode)
		data.bwc_enabled = 1;

	ret = mdss_mdp_data_check(&data, &ctx->dst_planes);
	if (ret)
		return ret;

	mdss_mdp_data_calc_offset(&data, ctx->dst_rect.x, ctx->dst_rect.y,
			&ctx->dst_planes, ctx->dst_fmt);

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
	u32 opmode = ctx->opmode;
	struct mdss_data_type *mdata;

	pr_debug("wb_num=%d format=%d\n", ctx->wb_num, format);

	mdss_mdp_get_plane_sizes(format, ctx->width, ctx->height,
				 &ctx->dst_planes,
				 ctx->opmode & MDSS_MDP_OP_BWC_EN);

	fmt = mdss_mdp_get_format_params(format);
	if (!fmt) {
		pr_err("wb format=%d not supported\n", format);
		return -EINVAL;
	}
	ctx->dst_fmt = fmt;

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

	if (fmt->bits[C3_ALPHA] || fmt->alpha_enable) {
		dst_format |= BIT(8); /* DSTC3_EN */
		if (!fmt->alpha_enable)
			dst_format |= BIT(14); /* DST_ALPHA_X */
	}

	mdata = mdss_mdp_get_mdata();
	if (mdata && mdata->mdp_rev >= MDSS_MDP_HW_REV_102) {
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

	mdp_wb_write(ctx, MDSS_MDP_REG_WB_DST_FORMAT, dst_format);
	mdp_wb_write(ctx, MDSS_MDP_REG_WB_DST_OP_MODE, opmode);
	mdp_wb_write(ctx, MDSS_MDP_REG_WB_DST_PACK_PATTERN, pattern);
	mdp_wb_write(ctx, MDSS_MDP_REG_WB_DST_YSTRIDE0, ystride0);
	mdp_wb_write(ctx, MDSS_MDP_REG_WB_DST_YSTRIDE1, ystride1);
	mdp_wb_write(ctx, MDSS_MDP_REG_WB_OUT_SIZE, outsize);
	mdp_wb_write(ctx, MDSS_MDP_REG_WB_DST_WRITE_CONFIG, 0x58);

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

	ctx->width = rot->dst.w;
	ctx->height = rot->dst.h;
	ctx->dst_rect.x = rot->dst.x;
	ctx->dst_rect.y = rot->dst.y;
	ctx->dst_rect.w = rot->src_rect.w;
	ctx->dst_rect.h = rot->src_rect.h;

	ctx->rot90 = !!(rot->flags & MDP_ROT_90);

	if (ctx->bwc_mode || (ctx->rot90 &&
			     (mdata->mdp_rev < MDSS_MDP_HW_REV_102)))
		format = mdss_mdp_get_rotator_dst_format(rot->format);
	else
		format = rot->format;

	if (ctx->rot90) {
		ctx->opmode |= BIT(5); /* ROT 90 */
		swap(ctx->dst_rect.w, ctx->dst_rect.h);
	}

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

static int mdss_mdp_writeback_stop(struct mdss_mdp_ctl *ctl)
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

	mdss_mdp_irq_disable_nosync(ctx->intr_type, ctx->intf_num);

	if (ctx->callback_fnc)
		ctx->callback_fnc(ctx->callback_arg);

	spin_lock(&ctx->wb_lock);
	list_for_each_entry(tmp, &ctx->vsync_handlers, list) {
		tmp->vsync_handler(ctl, vsync_time);
	}
	spin_unlock(&ctx->wb_lock);

	complete_all(&ctx->wb_comp);
}

static int mdss_mdp_wb_wait4comp(struct mdss_mdp_ctl *ctl, void *arg)
{
	struct mdss_mdp_writeback_ctx *ctx;
	int rc = 0;

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
		rc = -ENODEV;
		WARN(1, "writeback kickoff timed out (%d) ctl=%d\n",
						rc, ctl->num);
	} else {
		rc = 0;
	}

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false); /* clock off */

	ctx->comp_cnt--;

	return rc;
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

	if (ctx->comp_cnt) {
		pr_err("previous kickoff not completed yet, ctl=%d\n",
					ctl->num);
		return -EPERM;
	}

	wb_args = (struct mdss_mdp_writeback_arg *) arg;
	if (!wb_args)
		return -ENOENT;

	ret = mdss_mdp_writeback_addr_setup(ctx, wb_args->data);
	if (ret) {
		pr_err("writeback data setup error ctl=%d\n", ctl->num);
		return ret;
	}

	mdss_mdp_set_intr_callback(ctx->intr_type, ctx->intf_num,
		   mdss_mdp_writeback_intr_done, ctl);

	ctx->callback_fnc = wb_args->callback_fnc;
	ctx->callback_arg = wb_args->priv_data;

	flush_bits = BIT(16); /* WB */
	mdp_wb_write(ctx, MDSS_MDP_REG_WB_DST_ADDR_SW_STATUS, ctl->is_secure);
	mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_FLUSH, flush_bits);

	INIT_COMPLETION(ctx->wb_comp);
	mdss_mdp_irq_enable(ctx->intr_type, ctx->intf_num);

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);
	mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_START, 1);
	wmb();

	ctx->comp_cnt++;

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
	ctx->base = ctl->wb_base;
	ctx->initialized = false;
	init_completion(&ctx->wb_comp);
	spin_lock_init(&ctx->wb_lock);
	INIT_LIST_HEAD(&ctx->vsync_handlers);

	if (ctx->type == MDSS_MDP_WRITEBACK_TYPE_ROTATOR)
		ctl->prepare_fnc = mdss_mdp_writeback_prepare_rot;
	else /* wfd or line mode */
		ctl->prepare_fnc = mdss_mdp_writeback_prepare_wfd;
	ctl->stop_fnc = mdss_mdp_writeback_stop;
	ctl->display_fnc = mdss_mdp_writeback_display;
	ctl->wait_fnc = mdss_mdp_wb_wait4comp;
	ctl->add_vsync_handler = mdss_mdp_wb_add_vsync_handler;
	ctl->remove_vsync_handler = mdss_mdp_wb_remove_vsync_handler;

	return ret;
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

	return mdss_mdp_display_commit(ctl, arg);
}
