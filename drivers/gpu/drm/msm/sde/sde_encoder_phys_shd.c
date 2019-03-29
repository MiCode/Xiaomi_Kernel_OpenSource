/*
 * Copyright (c) 2018-2019 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt)	"[drm-shd:%s:%d] " fmt, __func__, __LINE__

#include <linux/debugfs.h>
#include <uapi/drm/sde_drm.h>

#include "sde_encoder_phys.h"
#include "sde_formats.h"
#include "sde_hw_top.h"
#include "sde_hw_interrupts.h"
#include "sde_core_irq.h"
#include "sde_crtc.h"
#include "sde_trace.h"
#include "sde_shd.h"
#include "sde_plane.h"

#define SHD_DEBUG(fmt, ...) pr_debug(fmt, ##__VA_ARGS__)

#define SDE_ERROR_PHYS(p, fmt, ...) SDE_ERROR("enc%d intf%d " fmt,\
		(p) ? (p)->parent->base.id : -1, \
		(p) ? (p)->intf_idx - INTF_0 : -1, \
	##__VA_ARGS__)

#define SDE_DEBUG_PHYS(p, fmt, ...) SDE_DEBUG("enc%d intf%d " fmt,\
			(p) ? (p)->parent->base.id : -1, \
			(p) ? (p)->intf_idx - INTF_0 : -1, \
	##__VA_ARGS__)

#define CTL_SSPP_FLUSH_MASK              0xCC183F
#define CTL_MIXER_FLUSH_MASK             0x1007C0

#define   CTL_LAYER(lm)                 \
		(((lm) == LM_5) ? (0x024) : (((lm) - LM_0) * 0x004))
#define   CTL_LAYER_EXT(lm)             \
		(0x40 + (((lm) - LM_0) * 0x004))
#define   CTL_LAYER_EXT2(lm)             \
		(0x70 + (((lm) - LM_0) * 0x004))

#define CTL_MIXER_BORDER_OUT            BIT(24)

#define LM_BLEND0_OP                     0x00

static inline struct sde_encoder_phys_shd *to_sde_encoder_phys_shd(
		struct sde_encoder_phys *phys_enc)
{
	return container_of(phys_enc, struct sde_encoder_phys_shd, base);
}

static DEFINE_SPINLOCK(hw_ctl_lock);

struct sde_shd_ctl_mixer_cfg {
	u32 mixercfg;
	u32 mixercfg_ext;
	u32 mixercfg_ext2;

	u32 mixercfg_mask;
	u32 mixercfg_ext_mask;
	u32 mixercfg_ext2_mask;
};

struct sde_shd_hw_ctl {
	struct sde_hw_ctl base;
	struct shd_stage_range range;
	struct sde_hw_ctl *orig;
	u32 flush_mask;
	struct sde_shd_ctl_mixer_cfg mixer_cfg[MAX_BLOCKS];
	struct sde_encoder_phys_shd *shd_enc;
};

struct sde_shd_mixer_cfg {
	uint32_t fg_alpha;
	uint32_t bg_alpha;
	uint32_t blend_op;
	bool dirty;
};

struct sde_shd_hw_mixer {
	struct sde_hw_mixer base;
	struct shd_stage_range range;
	struct sde_hw_mixer *orig;
	struct sde_shd_mixer_cfg cfg[SDE_STAGE_MAX];
};

static bool sde_encoder_phys_shd_is_master(struct sde_encoder_phys *phys_enc)
{
	return true;
}

static void sde_encoder_phys_shd_vblank_irq(void *arg, int irq_idx)
{
	struct sde_encoder_phys *phys_enc = arg;
	struct sde_hw_ctl *hw_ctl;
	struct sde_shd_hw_ctl *shd_ctl;
	unsigned long lock_flags;
	u32 flush_register = ~0;
	int new_cnt = -1, old_cnt = -1;

	if (!phys_enc)
		return;

	hw_ctl = phys_enc->hw_ctl;
	if (!hw_ctl)
		return;

	if (phys_enc->parent_ops.handle_vblank_virt)
		phys_enc->parent_ops.handle_vblank_virt(phys_enc->parent,
			phys_enc);

	old_cnt = atomic_read(&phys_enc->pending_kickoff_cnt);

	/*
	 * only decrement the pending flush count if we've actually flushed
	 * hardware. due to sw irq latency, vblank may have already happened
	 * so we need to double-check with hw that it accepted the flush bits
	 */
	spin_lock_irqsave(phys_enc->enc_spinlock, lock_flags);

	if (hw_ctl && hw_ctl->ops.get_flush_register)
		flush_register = hw_ctl->ops.get_flush_register(hw_ctl);

	shd_ctl = container_of(hw_ctl, struct sde_shd_hw_ctl, base);

	/*
	 * When bootloader's splash is presented, as bootloader is concurrently
	 * flushing hardware pipes, so when checking flush_register, we need
	 * to care if the active bit in the flush_register matches with the
	 * bootloader's splash pipe flush bits.
	 */
	if ((flush_register & shd_ctl->flush_mask &
		~phys_enc->splash_flush_bits) == 0)
		new_cnt = atomic_add_unless(&phys_enc->pending_kickoff_cnt,
				-1, 0);

	spin_unlock_irqrestore(phys_enc->enc_spinlock, lock_flags);

	SDE_EVT32_IRQ(DRMID(phys_enc->parent), phys_enc->intf_idx - INTF_0,
			old_cnt, new_cnt, flush_register);

	/* Signal any waiting atomic commit thread */
	wake_up_all(&phys_enc->pending_kickoff_wq);
}

static int _sde_encoder_phys_shd_register_irq(
		struct sde_encoder_phys *phys_enc,
		enum sde_intr_type intr_type, int idx,
		void (*irq_func)(void *, int), const char *irq_name)
{
	struct sde_encoder_phys_shd *shd_enc;
	int ret = 0;

	shd_enc = to_sde_encoder_phys_shd(phys_enc);
	shd_enc->irq_idx[idx] = sde_core_irq_idx_lookup(phys_enc->sde_kms,
			intr_type, phys_enc->intf_idx);

	if (shd_enc->irq_idx[idx] < 0) {
		SDE_DEBUG_PHYS(phys_enc,
			"failed to lookup IRQ index for %s type:%d\n", irq_name,
			intr_type);
		return -EINVAL;
	}

	shd_enc->irq_cb[idx].func = irq_func;
	shd_enc->irq_cb[idx].arg = phys_enc;
	ret = sde_core_irq_register_callback(phys_enc->sde_kms,
			shd_enc->irq_idx[idx], &shd_enc->irq_cb[idx]);
	if (ret) {
		SDE_ERROR_PHYS(phys_enc,
			"failed to register IRQ callback for %s\n", irq_name);
		shd_enc->irq_idx[idx] = -EINVAL;
		return ret;
	}

	SDE_DEBUG_PHYS(phys_enc, "registered irq %s idx: %d\n",
			irq_name, shd_enc->irq_idx[idx]);

	return ret;
}

static int _sde_encoder_phys_shd_unregister_irq(
	struct sde_encoder_phys *phys_enc, int idx)
{
	struct sde_encoder_phys_shd *shd_enc;

	shd_enc = to_sde_encoder_phys_shd(phys_enc);

	sde_core_irq_unregister_callback(phys_enc->sde_kms,
			shd_enc->irq_idx[idx], &shd_enc->irq_cb[idx]);

	SDE_DEBUG_PHYS(phys_enc, "unregistered %d\n", shd_enc->irq_idx[idx]);

	return 0;
}

static void _sde_shd_hw_ctl_clear_blendstages_in_range(
	struct sde_shd_hw_ctl *hw_ctl, enum sde_lm lm,
	bool handoff, u32 splash_mask, u32 splash_ext_mask)
{
	struct sde_hw_blk_reg_map *c = &hw_ctl->base.hw;
	u32 mixercfg, mixercfg_ext;
	u32 mixercfg_ext2;
	u32 mask = 0, ext_mask = 0, ext2_mask = 0;
	u32 start = hw_ctl->range.start + SDE_STAGE_0;
	u32 end = start + hw_ctl->range.size;
	u32 i;

	mixercfg = SDE_REG_READ(c, CTL_LAYER(lm));
	mixercfg_ext = SDE_REG_READ(c, CTL_LAYER_EXT(lm));
	mixercfg_ext2 = SDE_REG_READ(c, CTL_LAYER_EXT2(lm));

	if (!mixercfg && !mixercfg_ext && !mixercfg_ext2)
		goto end;

	if (handoff) {
		mask |= splash_mask;
		ext_mask |= splash_ext_mask;
	}

	/* SSPP_VIG0 */
	i = (mixercfg & 0x7) | ((mixercfg_ext & 1) << 3);
	if (i > start && i <= end) {
		mask |= 0x7;
		ext_mask |= 0x1;
	}

	/* SSPP_VIG1 */
	i = ((mixercfg >> 3) & 0x7) | (((mixercfg_ext >> 2) & 1) << 3);
	if (i > start && i <= end) {
		mask |= (0x7 << 3);
		ext_mask |= (0x1 << 2);
	}

	/* SSPP_VIG2 */
	i = ((mixercfg >> 6) & 0x7) | (((mixercfg_ext >> 4) & 1) << 3);
	if (i > start && i <= end) {
		mask |= (0x7 << 6);
		ext_mask |= (0x1 << 4);
	}

	/* SSPP_RGB0 */
	i = ((mixercfg >> 9) & 0x7) | (((mixercfg_ext >> 8) & 1) << 3);
	if (i > start && i <= end) {
		mask |= (0x7 << 9);
		ext_mask |= (0x1 << 8);
	}

	/* SSPP_RGB1 */
	i = ((mixercfg >> 12) & 0x7) | (((mixercfg_ext >> 10) & 1) << 3);
	if (i > start && i <= end) {
		mask |= (0x7 << 12);
		ext_mask |= (0x1 << 10);
	}

	/* SSPP_RGB2 */
	i = ((mixercfg >> 15) & 0x7) | (((mixercfg_ext >> 12) & 1) << 3);
	if (i > start && i <= end) {
		mask |= (0x7 << 15);
		ext_mask |= (0x1 << 12);
	}

	/* SSPP_DMA0 */
	i = ((mixercfg >> 18) & 0x7) | (((mixercfg_ext >> 16) & 1) << 3);
	if (i > start && i <= end) {
		mask |= (0x7 << 18);
		ext_mask |= (0x1 << 16);
	}

	/* SSPP_DMA1 */
	i = ((mixercfg >> 21) & 0x7) | (((mixercfg_ext >> 18) & 1) << 3);
	if (i > start && i <= end) {
		mask |= (0x7 << 21);
		ext_mask |= (0x1 << 18);
	}

	/* SSPP_VIG3 */
	i = ((mixercfg >> 26) & 0x7) | (((mixercfg_ext >> 6) & 1) << 3);
	if (i > start && i <= end) {
		mask |= (0x7 << 26);
		ext_mask |= (0x1 << 6);
	}

	/* SSPP_RGB3 */
	i = ((mixercfg >> 29) & 0x7) | (((mixercfg_ext >> 14) & 1) << 3);
	if (i > start && i <= end) {
		mask |= (0x7 << 29);
		ext_mask |= (0x1 << 14);
	}

	/* SSPP_CURSOR_0 */
	i = (mixercfg_ext >> 20) & 0xF;
	if (i > start && i <= end)
		ext_mask |= (0xF << 20);

	/* SSPP_CURSOR_1 */
	i = (mixercfg_ext >> 26) & 0xF;
	if (i > start && i <= end)
		ext_mask |= (0xF << 26);

	/* SSPP_DMA2 */
	i = (mixercfg_ext2 >> 0) & 0xF;
	if (i > start && i <= end)
		ext2_mask |= (0xF << 0);

	/* SSPP_DMA3 */
	i = (mixercfg_ext2 >> 4) & 0xF;
	if (i > start && i <= end)
		ext2_mask |= (0xF << 4);

end:
	hw_ctl->mixer_cfg[lm].mixercfg_mask = mask;
	hw_ctl->mixer_cfg[lm].mixercfg_ext_mask = ext_mask;
	hw_ctl->mixer_cfg[lm].mixercfg_ext2_mask = ext2_mask;
}

static void _sde_shd_hw_ctl_clear_all_blendstages(struct sde_hw_ctl *ctx,
	bool handoff, u32 splash_mask, u32 splash_ext_mask)
{
	struct sde_shd_hw_ctl *hw_ctl;
	int i;

	if (!ctx)
		return;

	hw_ctl = container_of(ctx, struct sde_shd_hw_ctl, base);

	for (i = 0; i < ctx->mixer_count; i++) {
		int mixer_id = ctx->mixer_hw_caps[i].id;

		_sde_shd_hw_ctl_clear_blendstages_in_range(hw_ctl, mixer_id,
				handoff, splash_mask, splash_ext_mask);
	}
}

static inline int _stage_offset(struct sde_hw_mixer *ctx, enum sde_stage stage)
{
	const struct sde_lm_sub_blks *sblk = ctx->cap->sblk;
	int rc;

	if (stage == SDE_STAGE_BASE)
		rc = -EINVAL;
	else if (stage <= sblk->maxblendstages)
		rc = sblk->blendstage_base[stage - SDE_STAGE_0];
	else
		rc = -EINVAL;

	return rc;
}

static void _sde_shd_hw_ctl_setup_blendstage(struct sde_hw_ctl *ctx,
	enum sde_lm lm, struct sde_hw_stage_cfg *stage_cfg, u32 index,
	bool handoff, u32 splash_mask, u32 splash_ext_mask)
{
	struct sde_shd_hw_ctl *hw_ctl;
	u32 mixercfg = 0, mixercfg_ext = 0, mix, ext, full, mixercfg_ext2;
	u32 mask = 0, ext_mask = 0, ext2_mask = 0;
	int i, j;
	int stages;
	int stage_offset = 0;
	int pipes_per_stage;
	struct sde_hw_blk_reg_map *c;

	if (!ctx)
		return;

	hw_ctl = container_of(ctx, struct sde_shd_hw_ctl, base);

	if (test_bit(SDE_MIXER_SOURCESPLIT,
		&ctx->mixer_hw_caps->features))
		pipes_per_stage = PIPES_PER_STAGE;
	else
		pipes_per_stage = 1;

	_sde_shd_hw_ctl_clear_blendstages_in_range(hw_ctl, lm, handoff,
			splash_mask, splash_ext_mask);

	if (!stage_cfg)
		goto exit;

	mixercfg = CTL_MIXER_BORDER_OUT;
	stage_offset = hw_ctl->range.start;
	stages = hw_ctl->range.size;

	c = &hw_ctl->base.hw;
	if (handoff) {
		mixercfg = SDE_REG_READ(c, CTL_LAYER(lm));
		mixercfg_ext = SDE_REG_READ(c, CTL_LAYER_EXT(lm));

		mixercfg &= splash_mask;
		mixercfg_ext &= splash_ext_mask;

		mask |= splash_mask;
		ext_mask |= splash_ext_mask;
		mixercfg |= CTL_MIXER_BORDER_OUT;
	}

	for (i = SDE_STAGE_0; i <= stages; i++) {
		/* overflow to ext register if 'i + 1 > 7' */
		mix = (i + stage_offset + 1) & 0x7;
		ext = (i + stage_offset) >= 7;
		full = (i + stage_offset + 1) & 0xF;

		for (j = 0 ; j < pipes_per_stage; j++) {
			switch (stage_cfg->stage[index][i][j]) {
			case SSPP_VIG0:
				mixercfg |= mix << 0;
				mixercfg_ext |= ext << 0;
				mask |= 0x7 << 0;
				ext_mask |= 0x1 << 0;
				break;
			case SSPP_VIG1:
				mixercfg |= mix << 3;
				mixercfg_ext |= ext << 2;
				mask |= 0x7 << 3;
				ext_mask |= 0x1 << 2;
				break;
			case SSPP_VIG2:
				mixercfg |= mix << 6;
				mixercfg_ext |= ext << 4;
				mask |= 0x7 << 6;
				ext_mask |= 0x1 << 4;
				break;
			case SSPP_VIG3:
				mixercfg |= mix << 26;
				mixercfg_ext |= ext << 6;
				mask |= 0x7 << 26;
				ext_mask |= 0x1 << 6;
				break;
			case SSPP_RGB0:
				mixercfg |= mix << 9;
				mixercfg_ext |= ext << 8;
				mask |= 0x7 << 9;
				ext_mask |= 0x1 << 8;
				break;
			case SSPP_RGB1:
				mixercfg |= mix << 12;
				mixercfg_ext |= ext << 10;
				mask |= 0x7 << 12;
				ext_mask |= 0x1 << 10;
				break;
			case SSPP_RGB2:
				mixercfg |= mix << 15;
				mixercfg_ext |= ext << 12;
				mask |= 0x7 << 15;
				ext_mask |= 0x1 << 12;
				break;
			case SSPP_RGB3:
				mixercfg |= mix << 29;
				mixercfg_ext |= ext << 14;
				mask |= 0x7 << 29;
				ext_mask |= 0x1 << 14;
				break;
			case SSPP_DMA0:
				mixercfg |= mix << 18;
				mixercfg_ext |= ext << 16;
				mask |= 0x7 << 18;
				ext_mask |= 0x1 << 16;
				break;
			case SSPP_DMA1:
				mixercfg |= mix << 21;
				mixercfg_ext |= ext << 18;
				mask |= 0x7 << 21;
				ext_mask |= 0x1 << 18;
				break;
			case SSPP_DMA2:
				mix |= full;
				mixercfg_ext2 |= mix << 0;
				ext2_mask |= 0xF << 0;
				break;
			case SSPP_DMA3:
				mix |= full;
				mixercfg_ext2 |= mix << 4;
				ext2_mask |= 0xF << 4;
				break;
			case SSPP_CURSOR0:
				mixercfg_ext |= full << 20;
				ext_mask |= 0xF << 20;
				break;
			case SSPP_CURSOR1:
				mixercfg_ext |= full << 26;
				ext_mask |= 0xF << 26;
				break;
			default:
				break;
			}
		}
	}

	hw_ctl->mixer_cfg[lm].mixercfg_mask |= mask;
	hw_ctl->mixer_cfg[lm].mixercfg_ext_mask |= ext_mask;
	hw_ctl->mixer_cfg[lm].mixercfg_ext2_mask |= ext2_mask;
exit:
	hw_ctl->mixer_cfg[lm].mixercfg = mixercfg;
	hw_ctl->mixer_cfg[lm].mixercfg_ext = mixercfg_ext;
	hw_ctl->mixer_cfg[lm].mixercfg_ext2 = mixercfg_ext2;
}

static void _sde_shd_hw_ctl_trigger_flush(struct sde_hw_ctl *ctx)
{
	struct sde_shd_hw_ctl *hw_ctl;
	struct sde_hw_blk_reg_map *c;
	u32 mixercfg, mixercfg_ext;
	u32 mixercfg_ext2;
	int i;

	hw_ctl = container_of(ctx, struct sde_shd_hw_ctl, base);

	hw_ctl->flush_mask = ctx->pending_flush_mask;

	hw_ctl->flush_mask &= CTL_SSPP_FLUSH_MASK;

	c = &ctx->hw;

	for (i = 0; i < ctx->mixer_count; i++) {
		int lm = ctx->mixer_hw_caps[i].id;

		mixercfg = SDE_REG_READ(c, CTL_LAYER(lm));
		mixercfg_ext = SDE_REG_READ(c, CTL_LAYER_EXT(lm));
		mixercfg_ext2 = SDE_REG_READ(c, CTL_LAYER_EXT2(lm));

		mixercfg &= ~hw_ctl->mixer_cfg[lm].mixercfg_mask;
		mixercfg_ext &= ~hw_ctl->mixer_cfg[lm].mixercfg_ext_mask;
		mixercfg_ext2 &= ~hw_ctl->mixer_cfg[lm].mixercfg_ext2_mask;

		mixercfg |= hw_ctl->mixer_cfg[lm].mixercfg;
		mixercfg_ext |= hw_ctl->mixer_cfg[lm].mixercfg_ext;
		mixercfg_ext2 |= hw_ctl->mixer_cfg[lm].mixercfg_ext2;

		SDE_REG_WRITE(c, CTL_LAYER(lm), mixercfg);
		SDE_REG_WRITE(c, CTL_LAYER_EXT(lm), mixercfg_ext);
		SDE_REG_WRITE(c, CTL_LAYER_EXT2(lm), mixercfg_ext2);
	}
}

static void _sde_shd_setup_blend_config(struct sde_hw_mixer *ctx,
		uint32_t stage,
		uint32_t fg_alpha, uint32_t bg_alpha, uint32_t blend_op)
{
	struct sde_shd_hw_mixer *hw_lm;
	struct sde_shd_mixer_cfg *cfg;

	if (!ctx)
		return;

	hw_lm = container_of(ctx, struct sde_shd_hw_mixer, base);

	cfg = &hw_lm->cfg[stage + hw_lm->range.start];

	cfg->fg_alpha = fg_alpha;
	cfg->bg_alpha = bg_alpha;
	cfg->blend_op = blend_op;
	cfg->dirty = true;
}

static void _sde_shd_setup_mixer_out(struct sde_hw_mixer *ctx,
		struct sde_hw_mixer_cfg *cfg)
{
	/* do nothing */
}

static void _sde_shd_flush_hw_lm(struct sde_hw_mixer *ctx)
{
	struct sde_shd_hw_mixer *hw_lm;
	struct sde_hw_blk_reg_map *c = &ctx->hw;
	int stage_off, i;
	u32 reset = BIT(16), val;
	int start, end;

	if (!ctx)
		return;

	hw_lm = container_of(ctx, struct sde_shd_hw_mixer, base);

	start = SDE_STAGE_0 + hw_lm->range.start;
	end = start + hw_lm->range.size;
	reset = ~reset;
	for (i = start; i < end; i++) {
		stage_off = _stage_offset(ctx, i);
		if (WARN_ON(stage_off < 0))
			return;

		val = SDE_REG_READ(c, LM_BLEND0_OP + stage_off);
		val &= reset;
		SDE_REG_WRITE(c, LM_BLEND0_OP + stage_off, val);

		if (hw_lm->cfg[i].dirty) {
			hw_lm->orig->ops.setup_blend_config(ctx, i,
				hw_lm->cfg[i].fg_alpha,
				hw_lm->cfg[i].bg_alpha,
				hw_lm->cfg[i].blend_op);
			hw_lm->cfg[i].dirty = false;
		}
	}
}

static void _sde_shd_trigger_flush(struct sde_hw_ctl *ctx)
{
	struct sde_shd_hw_ctl *hw_ctl;
	struct sde_encoder_phys_shd *shd_enc;
	struct sde_encoder_phys *phys;
	struct sde_hw_blk_reg_map *c;
	unsigned long lock_flags;
	int i;

	hw_ctl = container_of(ctx, struct sde_shd_hw_ctl, base);
	shd_enc = hw_ctl->shd_enc;

	c = &ctx->hw;

	spin_lock_irqsave(&hw_ctl_lock, lock_flags);

	phys = &shd_enc->base;
	phys->splash_flush_bits = phys->sde_kms->splash_info.flush_bits;

	_sde_shd_hw_ctl_trigger_flush(ctx);

	for (i = 0; i < shd_enc->num_mixers; i++)
		_sde_shd_flush_hw_lm(shd_enc->hw_lm[i]);

	hw_ctl->orig->ops.trigger_flush(ctx);

	spin_unlock_irqrestore(&hw_ctl_lock, lock_flags);
}

static void _sde_encoder_phys_shd_rm_reserve(
		struct sde_encoder_phys *phys_enc,
		struct shd_display *display)
{
	struct sde_encoder_phys_shd *shd_enc;
	struct sde_rm *rm;
	struct sde_rm_hw_iter ctl_iter, lm_iter;
	struct drm_encoder *encoder;
	struct sde_shd_hw_ctl *hw_ctl;
	struct sde_shd_hw_mixer *hw_lm;
	int i;

	encoder = display->base->encoder;
	rm = &phys_enc->sde_kms->rm;
	shd_enc = to_sde_encoder_phys_shd(phys_enc);

	sde_rm_init_hw_iter(&ctl_iter, encoder->base.id, SDE_HW_BLK_CTL);
	sde_rm_init_hw_iter(&lm_iter, encoder->base.id, SDE_HW_BLK_LM);

	shd_enc->num_mixers = 0;
	shd_enc->num_ctls = 0;

	for (i = 0; i < CRTC_DUAL_MIXERS; i++) {
		/* reserve layer mixer */
		if (!sde_rm_get_hw(rm, &lm_iter))
			break;
		hw_lm = container_of(shd_enc->hw_lm[i],
				struct sde_shd_hw_mixer, base);
		hw_lm->base = *(struct sde_hw_mixer *)lm_iter.hw;
		hw_lm->range = display->stage_range;
		hw_lm->orig = lm_iter.hw;
		hw_lm->base.ops.setup_blend_config =
				_sde_shd_setup_blend_config;
		hw_lm->base.ops.setup_mixer_out =
				_sde_shd_setup_mixer_out;

		SHD_DEBUG("reserve LM%d %pK from enc %d to %d\n",
			hw_lm->base.idx, hw_lm,
			DRMID(encoder),
			DRMID(phys_enc->parent));

		sde_rm_ext_blk_create_reserve(rm,
			SDE_HW_BLK_LM, 0,
			&hw_lm->base, phys_enc->parent);
		shd_enc->num_mixers++;

		/* reserve ctl */
		if (!sde_rm_get_hw(rm, &ctl_iter))
			break;
		hw_ctl = container_of(shd_enc->hw_ctl[i],
				struct sde_shd_hw_ctl, base);
		hw_ctl->base = *(struct sde_hw_ctl *)ctl_iter.hw;
		hw_ctl->shd_enc = shd_enc;
		hw_ctl->range = display->stage_range;
		hw_ctl->orig = ctl_iter.hw;
		hw_ctl->base.ops.clear_all_blendstages =
			_sde_shd_hw_ctl_clear_all_blendstages;
		hw_ctl->base.ops.setup_blendstage =
			_sde_shd_hw_ctl_setup_blendstage;
		hw_ctl->base.ops.trigger_flush =
			_sde_shd_trigger_flush;

		SHD_DEBUG("reserve CTL%d %pK from enc %d to %d\n",
			hw_ctl->base.idx, hw_ctl,
			DRMID(encoder),
			DRMID(phys_enc->parent));

		sde_rm_ext_blk_create_reserve(rm,
			SDE_HW_BLK_CTL, 0,
			&hw_ctl->base, phys_enc->parent);
		shd_enc->num_ctls++;
	}
}

static void _sde_encoder_phys_shd_rm_release(
		struct sde_encoder_phys *phys_enc,
		struct shd_display *display)
{
	struct sde_rm *rm;

	rm = &phys_enc->sde_kms->rm;

	sde_rm_ext_blk_destroy(rm, phys_enc->parent);
}

static void sde_encoder_phys_shd_mode_set(
		struct sde_encoder_phys *phys_enc,
		struct drm_display_mode *mode,
		struct drm_display_mode *adj_mode)
{
	struct drm_connector *connector;
	struct sde_connector *sde_conn;
	struct shd_display *display;
	struct drm_encoder *encoder;
	struct sde_rm_hw_iter iter;
	struct sde_rm *rm;

	SHD_DEBUG("%d\n", phys_enc->parent->base.id);

	phys_enc->cached_mode = *adj_mode;

	connector = phys_enc->connector;
	if (!connector || connector->encoder != phys_enc->parent) {
		SDE_ERROR("failed to find connector\n");
		return;
	}

	sde_conn = to_sde_connector(connector);
	display = sde_conn->display;
	encoder = display->base->encoder;

	_sde_encoder_phys_shd_rm_reserve(phys_enc, display);

	rm = &phys_enc->sde_kms->rm;

	sde_rm_init_hw_iter(&iter, DRMID(phys_enc->parent), SDE_HW_BLK_CTL);
	if (sde_rm_get_hw(rm, &iter))
		phys_enc->hw_ctl = (struct sde_hw_ctl *)iter.hw;
	if (IS_ERR_OR_NULL(phys_enc->hw_ctl)) {
		SHD_DEBUG("failed to init ctl, %ld\n",
				PTR_ERR(phys_enc->hw_ctl));
		phys_enc->hw_ctl = NULL;
		return;
	}
}

static int _sde_encoder_phys_shd_wait_for_vblank(
		struct sde_encoder_phys *phys_enc, bool notify)
{
	struct sde_encoder_phys_shd *shd_enc;
	u32 irq_status;
	int ret = 0;

	if (!phys_enc) {
		pr_err("invalid encoder\n");
		return -EINVAL;
	}

	if (phys_enc->enable_state != SDE_ENC_ENABLED) {
		SDE_ERROR("encoder not enabled\n");
		return -EWOULDBLOCK;
	}

	shd_enc = to_sde_encoder_phys_shd(phys_enc);

	/* Wait for kickoff to complete */
	ret = sde_encoder_helper_wait_event_timeout(
			DRMID(phys_enc->parent),
			phys_enc->intf_idx - INTF_0,
			&phys_enc->pending_kickoff_wq,
			&phys_enc->pending_kickoff_cnt,
			KICKOFF_TIMEOUT_MS);

	if (ret <= 0) {
		irq_status = sde_core_irq_read(phys_enc->sde_kms,
				INTR_IDX_VSYNC, true);
		if (irq_status) {
			SDE_EVT32(DRMID(phys_enc->parent),
					phys_enc->intf_idx - INTF_0);
			SDE_DEBUG_PHYS(phys_enc, "done, irq not triggered\n");
			if (notify && phys_enc->parent_ops.handle_frame_done)
				phys_enc->parent_ops.handle_frame_done(
						phys_enc->parent, phys_enc,
						SDE_ENCODER_FRAME_EVENT_DONE);
			sde_encoder_phys_shd_vblank_irq(phys_enc,
						INTR_IDX_VSYNC);
			ret = 0;
		} else {
			SDE_EVT32(DRMID(phys_enc->parent),
					phys_enc->intf_idx - INTF_0);
			SDE_ERROR_PHYS(phys_enc, "kickoff timed out\n");
			if (notify && phys_enc->parent_ops.handle_frame_done)
				phys_enc->parent_ops.handle_frame_done(
						phys_enc->parent, phys_enc,
						SDE_ENCODER_FRAME_EVENT_ERROR);
			ret = -ETIMEDOUT;
		}
	} else {
		if (notify && phys_enc->parent_ops.handle_frame_done)
			phys_enc->parent_ops.handle_frame_done(
					phys_enc->parent, phys_enc,
					SDE_ENCODER_FRAME_EVENT_DONE);
		ret = 0;
	}

	return ret;
}

static int sde_encoder_phys_shd_wait_for_vblank(
		struct sde_encoder_phys *phys_enc)
{
	return _sde_encoder_phys_shd_wait_for_vblank(phys_enc, true);
}

void sde_encoder_phys_shd_handle_post_kickoff(
	struct sde_encoder_phys *phys_enc)
{
	if (!phys_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}

	if (phys_enc->enable_state == SDE_ENC_ENABLING) {
		SDE_EVT32(DRMID(phys_enc->parent));
		phys_enc->enable_state = SDE_ENC_ENABLED;
	}
}

static int sde_encoder_phys_shd_control_vblank_irq(
		struct sde_encoder_phys *phys_enc,
		bool enable)
{
	int ret = 0;
	struct sde_encoder_phys_shd *shd_enc;

	if (!phys_enc) {
		SDE_ERROR("invalid encoder\n");
		return -EINVAL;
	}

	shd_enc = to_sde_encoder_phys_shd(phys_enc);

	SHD_DEBUG("[%pS] %d enable=%d/%d\n",
			__builtin_return_address(0), DRMID(phys_enc->parent),
			enable, atomic_read(&phys_enc->vblank_refcount));

	SDE_EVT32(DRMID(phys_enc->parent), enable,
			atomic_read(&phys_enc->vblank_refcount));

	if (enable && atomic_inc_return(&phys_enc->vblank_refcount) == 1)
		ret = _sde_encoder_phys_shd_register_irq(phys_enc,
				SDE_IRQ_TYPE_INTF_VSYNC,
				INTR_IDX_VSYNC,
				sde_encoder_phys_shd_vblank_irq, "vsync_irq");
	else if (!enable && atomic_dec_return(&phys_enc->vblank_refcount) == 0)
		ret = _sde_encoder_phys_shd_unregister_irq(phys_enc,
				INTR_IDX_VSYNC);

	if (ret)
		SHD_DEBUG("control vblank irq error %d, enable %d\n",
				ret, enable);

	return ret;
}

static void sde_encoder_phys_shd_enable(struct sde_encoder_phys *phys_enc)
{
	struct drm_connector *connector;

	SHD_DEBUG("%d\n", phys_enc->parent->base.id);

	if (!phys_enc->parent || !phys_enc->parent->dev) {
		SDE_ERROR("invalid drm device\n");
		return;
	}

	connector = phys_enc->connector;
	if (!connector || connector->encoder != phys_enc->parent) {
		SDE_ERROR("failed to find connector\n");
		return;
	}

	sde_encoder_phys_shd_control_vblank_irq(phys_enc, true);

	if (phys_enc->enable_state == SDE_ENC_DISABLED)
		phys_enc->enable_state = SDE_ENC_ENABLING;
}

static void sde_encoder_phys_shd_disable(struct sde_encoder_phys *phys_enc)
{
	struct sde_connector *sde_conn;
	struct shd_display *display;
	bool splash_enabled = false;
	u32 mixer_mask = 0, mixer_ext_mask = 0;

	SHD_DEBUG("%d\n", phys_enc->parent->base.id);

	if (!phys_enc || !phys_enc->parent || !phys_enc->parent->dev ||
			!phys_enc->parent->dev->dev_private) {
		SDE_ERROR("invalid encoder/device\n");
		return;
	}

	if (phys_enc->enable_state == SDE_ENC_DISABLED) {
		SDE_ERROR("already disabled\n");
		return;
	}

	sde_splash_get_mixer_mask(&phys_enc->sde_kms->splash_info,
				&splash_enabled, &mixer_mask, &mixer_ext_mask);

	_sde_shd_hw_ctl_clear_all_blendstages(phys_enc->hw_ctl,
			splash_enabled, mixer_mask, mixer_ext_mask);

	_sde_shd_trigger_flush(phys_enc->hw_ctl);

	_sde_encoder_phys_shd_wait_for_vblank(phys_enc, false);

	sde_encoder_phys_shd_control_vblank_irq(phys_enc, false);

	phys_enc->enable_state = SDE_ENC_DISABLED;

	if (!phys_enc->connector)
		return;

	sde_conn = to_sde_connector(phys_enc->connector);
	display = sde_conn->display;

	_sde_encoder_phys_shd_rm_release(phys_enc, display);
}

static void sde_encoder_phys_shd_destroy(struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_shd *shd_enc =
		to_sde_encoder_phys_shd(phys_enc);

	if (!phys_enc)
		return;

	kfree(shd_enc);
}

/**
 * sde_encoder_phys_shd_init_ops - initialize writeback operations
 * @ops:	Pointer to encoder operation table
 */
static void sde_encoder_phys_shd_init_ops(struct sde_encoder_phys_ops *ops)
{
	ops->is_master = sde_encoder_phys_shd_is_master;
	ops->mode_set = sde_encoder_phys_shd_mode_set;
	ops->enable = sde_encoder_phys_shd_enable;
	ops->disable = sde_encoder_phys_shd_disable;
	ops->destroy = sde_encoder_phys_shd_destroy;
	ops->control_vblank_irq = sde_encoder_phys_shd_control_vblank_irq;
	ops->wait_for_commit_done = sde_encoder_phys_shd_wait_for_vblank;
	ops->handle_post_kickoff = sde_encoder_phys_shd_handle_post_kickoff;
}

struct sde_encoder_phys *sde_encoder_phys_shd_init(
		struct sde_enc_phys_init_params *p)
{
	struct sde_encoder_phys *phys_enc;
	struct sde_encoder_phys_shd *shd_enc;
	struct sde_shd_hw_ctl *hw_ctl;
	struct sde_shd_hw_mixer *hw_lm;
	int ret = 0, i;

	SHD_DEBUG("\n");

	if (!p || !p->parent) {
		SDE_ERROR("invalid params\n");
		ret = -EINVAL;
		goto fail_alloc;
	}

	shd_enc = kzalloc(sizeof(*shd_enc), GFP_KERNEL);
	if (!shd_enc) {
		ret = -ENOMEM;
		goto fail_alloc;
	}

	for (i = 0; i < CRTC_DUAL_MIXERS; i++) {
		hw_ctl = kzalloc(sizeof(*hw_ctl), GFP_KERNEL);
		if (!hw_ctl) {
			ret = -ENOMEM;
			goto fail_ctl;
		}
		shd_enc->hw_ctl[i] = &hw_ctl->base;

		hw_lm = kzalloc(sizeof(*hw_lm), GFP_KERNEL);
		if (!hw_lm) {
			ret = -ENOMEM;
			goto fail_ctl;
		}
		shd_enc->hw_lm[i] = &hw_lm->base;
	}

	phys_enc = &shd_enc->base;

	sde_encoder_phys_shd_init_ops(&phys_enc->ops);
	phys_enc->parent = p->parent;
	phys_enc->parent_ops = p->parent_ops;
	phys_enc->sde_kms = p->sde_kms;
	phys_enc->split_role = p->split_role;
	phys_enc->intf_mode = INTF_MODE_NONE;
	phys_enc->intf_idx = p->intf_idx;
	phys_enc->enc_spinlock = p->enc_spinlock;
	for (i = 0; i < INTR_IDX_MAX; i++)
		INIT_LIST_HEAD(&shd_enc->irq_cb[i].list);
	atomic_set(&phys_enc->vblank_refcount, 0);
	atomic_set(&phys_enc->pending_kickoff_cnt, 0);
	phys_enc->splash_flush_bits = 0;
	init_waitqueue_head(&phys_enc->pending_kickoff_wq);
	phys_enc->enable_state = SDE_ENC_DISABLED;

	return phys_enc;

fail_ctl:
	for (i = 0; i < CRTC_DUAL_MIXERS; i++) {
		kfree(shd_enc->hw_ctl[i]);
		kfree(shd_enc->hw_lm[i]);
	}
	kfree(shd_enc);
fail_alloc:
	return ERR_PTR(ret);
}

