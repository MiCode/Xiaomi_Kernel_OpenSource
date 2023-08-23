/* Copyright (c) 2015-2020, The Linux Foundation. All rights reserved.
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
#include "sde_plane.h"
#include "shd_drm.h"
#include "shd_hw.h"

#define SDE_ERROR_PHYS(p, fmt, ...) SDE_ERROR("enc%d intf%d " fmt,\
		(p) ? (p)->parent->base.id : -1, \
		(p) ? (p)->intf_idx - INTF_0 : -1, \
	##__VA_ARGS__)

#define SDE_DEBUG_PHYS(p, fmt, ...) SDE_DEBUG("enc%d intf%d " fmt,\
			(p) ? (p)->parent->base.id : -1, \
			(p) ? (p)->intf_idx - INTF_0 : -1, \
	##__VA_ARGS__)

/**
 * struct sde_encoder_phys_shd - sub-class of sde_encoder_phys to handle shared
 *	mode specific operations
 * @base:	Baseclass physical encoder structure
 * @hw_lm:	HW LM blocks created by this shared encoder
 * @hw_ctl:	HW CTL blocks created by this shared encoder
 * @num_mixers:	Number of LM blocks
 * @num_ctls:	Number of CTL blocks
 */
struct sde_encoder_phys_shd {
	struct sde_encoder_phys base;
	struct sde_hw_mixer *hw_lm[CRTC_DUAL_MIXERS];
	struct sde_hw_ctl *hw_ctl[CRTC_DUAL_MIXERS];
	u32 num_mixers;
	u32 num_ctls;
};

#define to_sde_encoder_phys_shd(x) \
	container_of(x, struct sde_encoder_phys_shd, base)

static inline
bool sde_encoder_phys_shd_is_master(struct sde_encoder_phys *phys_enc)
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
	u32 event = 0;

	if (!phys_enc)
		return;

	hw_ctl = phys_enc->hw_ctl;
	if (!hw_ctl)
		return;

	SDE_ATRACE_BEGIN("vblank_irq");

	/*
	 * only decrement the pending flush count if we've actually flushed
	 * hardware. due to sw irq latency, vblank may have already happened
	 * so we need to double-check with hw that it accepted the flush bits
	 */
	spin_lock_irqsave(phys_enc->enc_spinlock, lock_flags);

	old_cnt = atomic_read(&phys_enc->pending_kickoff_cnt);

	if (hw_ctl && hw_ctl->ops.get_flush_register)
		flush_register = hw_ctl->ops.get_flush_register(hw_ctl);

	shd_ctl = container_of(hw_ctl, struct sde_shd_hw_ctl, base);

	if (flush_register)
		SDE_DEBUG("%d irq flush=0x%x mask=0x%x\n",
			DRMID(phys_enc->parent),
			flush_register, shd_ctl->flush_mask);

	if (flush_register & shd_ctl->flush_mask)
		goto not_flushed;

	/*
	 * When flush_mask is changed to 0, we need additional vsync
	 * to make sure the detach flush is done
	 */
	if (flush_register && !shd_ctl->flush_mask && shd_ctl->old_mask) {
		shd_ctl->old_mask = 0;
		goto not_flushed;
	}

	new_cnt = atomic_add_unless(&phys_enc->pending_kickoff_cnt, -1, 0);

	if (atomic_add_unless(&phys_enc->pending_retire_fence_cnt, -1, 0))
		event |= SDE_ENCODER_FRAME_EVENT_SIGNAL_RETIRE_FENCE |
			SDE_ENCODER_FRAME_EVENT_SIGNAL_RELEASE_FENCE;

not_flushed:
	spin_unlock_irqrestore(phys_enc->enc_spinlock, lock_flags);

	if (event && phys_enc->parent_ops.handle_frame_done)
		phys_enc->parent_ops.handle_frame_done(phys_enc->parent,
			phys_enc, event);

	if (phys_enc->parent_ops.handle_vblank_virt)
		phys_enc->parent_ops.handle_vblank_virt(phys_enc->parent,
				phys_enc);

	SDE_EVT32_IRQ(DRMID(phys_enc->parent), phys_enc->hw_intf->idx - INTF_0,
		old_cnt, new_cnt,
		flush_register, event);

	/* Signal any waiting atomic commit thread */
	wake_up_all(&phys_enc->pending_kickoff_wq);

	SDE_ATRACE_END("vblank_irq");
}

static int _sde_encoder_phys_shd_register_irq(
		struct sde_encoder_phys *phys_enc,
		enum sde_intr_idx intr_idx,
		bool enable)
{
	SDE_DEBUG("%d enable %d\n", DRMID(phys_enc->parent), enable);

	if (enable)
		return sde_encoder_helper_register_irq(phys_enc,
				INTR_IDX_VSYNC);
	else
		return sde_encoder_helper_unregister_irq(phys_enc,
				INTR_IDX_VSYNC);
}

static inline
void _sde_encoder_phys_shd_setup_irq_hw_idx(
		struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_irq *irq;

	irq = &phys_enc->irq[INTR_IDX_VSYNC];

	if (irq->irq_idx < 0)
		irq->hw_idx = phys_enc->intf_idx;
}

static int _sde_encoder_phys_shd_rm_reserve(
		struct sde_encoder_phys *phys_enc,
		struct shd_display *display)
{
	struct sde_encoder_phys_shd *shd_enc;
	struct sde_rm *rm;
	struct sde_rm_hw_iter ctl_iter, lm_iter, pp_iter;
	struct drm_encoder *encoder;
	struct sde_shd_hw_ctl *hw_ctl;
	struct sde_shd_hw_mixer *hw_lm;
	struct sde_hw_pingpong *hw_pp;
	int i, rc = 0;

	encoder = display->base->encoder;
	rm = &phys_enc->sde_kms->rm;
	shd_enc = to_sde_encoder_phys_shd(phys_enc);

	sde_rm_init_hw_iter(&ctl_iter, encoder->base.id, SDE_HW_BLK_CTL);
	sde_rm_init_hw_iter(&lm_iter, encoder->base.id, SDE_HW_BLK_LM);
	sde_rm_init_hw_iter(&pp_iter, encoder->base.id, SDE_HW_BLK_PINGPONG);

	shd_enc->num_mixers = 0;
	shd_enc->num_ctls = 0;

	for (i = 0; i < CRTC_DUAL_MIXERS; i++) {
		/* reserve lm */
		if (!sde_rm_get_hw(rm, &lm_iter))
			break;
		hw_lm = container_of(shd_enc->hw_lm[i],
				struct sde_shd_hw_mixer, base);
		hw_lm->base = *(struct sde_hw_mixer *)lm_iter.hw;
		hw_lm->range = display->stage_range;
		hw_lm->roi = display->roi;
		hw_lm->orig = lm_iter.hw;
		sde_shd_hw_lm_init_op(&hw_lm->base);

		SDE_DEBUG("reserve LM%d %pK from enc %d to %d\n",
			hw_lm->base.idx, hw_lm,
			DRMID(encoder),
			DRMID(phys_enc->parent));

		rc = sde_rm_ext_blk_create_reserve(rm,
			&hw_lm->base.base, phys_enc->parent);
		if (rc) {
			SDE_ERROR("failed to create & reserve lm\n");
			break;
		}
		shd_enc->num_mixers++;

		/* reserve ctl */
		if (!sde_rm_get_hw(rm, &ctl_iter))
			break;
		hw_ctl = container_of(shd_enc->hw_ctl[i],
				struct sde_shd_hw_ctl, base);
		hw_ctl->base = *(struct sde_hw_ctl *)ctl_iter.hw;
		hw_ctl->range = display->stage_range;
		hw_ctl->orig = ctl_iter.hw;
		sde_shd_hw_ctl_init_op(&hw_ctl->base);

		SDE_DEBUG("reserve CTL%d %pK from enc %d to %d\n",
			hw_ctl->base.idx, hw_ctl,
			DRMID(encoder),
			DRMID(phys_enc->parent));

		rc = sde_rm_ext_blk_create_reserve(rm,
			&hw_ctl->base.base, phys_enc->parent);
		if (rc) {
			SDE_ERROR("failed to create & reserve ctl\n");
			break;
		}
		shd_enc->num_ctls++;

		/* reserve pingpong */
		if (!sde_rm_get_hw(rm, &pp_iter))
			break;
		hw_pp = pp_iter.hw;

		SDE_DEBUG("reserve PP%d from enc %d to %d\n",
			hw_pp->idx,
			DRMID(encoder),
			DRMID(phys_enc->parent));

		rc = sde_rm_ext_blk_create_reserve(rm,
			&hw_pp->base, phys_enc->parent);
		if (rc) {
			SDE_ERROR("failed to create & reserve pingpong\n");
			break;
		}
	}

	return rc;
}

static inline void _sde_encoder_phys_shd_rm_release(
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
	struct shd_display *display;
	struct drm_encoder *encoder;
	struct sde_rm_hw_iter iter;
	struct sde_rm *rm;

	SDE_DEBUG("%d\n", phys_enc->parent->base.id);

	phys_enc->cached_mode = *adj_mode;

	connector = phys_enc->connector;
	if (!connector || connector->encoder != phys_enc->parent) {
		SDE_ERROR("failed to find connector\n");
		return;
	}

	display = sde_connector_get_display(connector);
	encoder = display->base->encoder;

	if (_sde_encoder_phys_shd_rm_reserve(phys_enc, display))
		return;

	rm = &phys_enc->sde_kms->rm;

	sde_rm_init_hw_iter(&iter, DRMID(phys_enc->parent), SDE_HW_BLK_CTL);
	if (sde_rm_get_hw(rm, &iter))
		phys_enc->hw_ctl = (struct sde_hw_ctl *)iter.hw;
	if (IS_ERR_OR_NULL(phys_enc->hw_ctl)) {
		SDE_DEBUG("failed to init ctl, %ld\n",
				PTR_ERR(phys_enc->hw_ctl));
		phys_enc->hw_ctl = NULL;
		return;
	}

	sde_rm_init_hw_iter(&iter, DRMID(encoder), SDE_HW_BLK_INTF);
	if (sde_rm_get_hw(rm, &iter))
		phys_enc->hw_intf = (struct sde_hw_intf *)iter.hw;
	if (IS_ERR_OR_NULL(phys_enc->hw_intf)) {
		SDE_DEBUG("failed to init intf: %ld\n",
				PTR_ERR(phys_enc->hw_intf));
		phys_enc->hw_intf = NULL;
		return;
	}

	sde_rm_init_hw_iter(&iter, DRMID(encoder), SDE_HW_BLK_PINGPONG);
	if (sde_rm_get_hw(rm, &iter))
		phys_enc->hw_pp = (struct sde_hw_pingpong *)iter.hw;
	if (IS_ERR_OR_NULL(phys_enc->hw_pp)) {
		SDE_DEBUG("failed to init pingpong: %ld\n",
				PTR_ERR(phys_enc->hw_pp));
		phys_enc->hw_pp = NULL;
		return;
	}

	_sde_encoder_phys_shd_setup_irq_hw_idx(phys_enc);
}

static int _sde_encoder_phys_shd_wait_for_vblank(
		struct sde_encoder_phys *phys_enc, bool notify)
{
	struct sde_encoder_wait_info wait_info;
	int ret = 0;
	u32 event = 0;
	u32 event_helper = 0;

	if (!phys_enc) {
		pr_err("invalid encoder\n");
		return -EINVAL;
	}

	wait_info.wq = &phys_enc->pending_kickoff_wq;
	wait_info.atomic_cnt = &phys_enc->pending_kickoff_cnt;
	wait_info.timeout_ms = KICKOFF_TIMEOUT_MS;

	/* Wait for kickoff to complete */
	ret = sde_encoder_helper_wait_for_irq(phys_enc, INTR_IDX_VSYNC,
			&wait_info);

	event_helper = SDE_ENCODER_FRAME_EVENT_SIGNAL_RELEASE_FENCE
			| SDE_ENCODER_FRAME_EVENT_SIGNAL_RETIRE_FENCE;

	if (notify) {
		if (ret == -ETIMEDOUT) {
			event = SDE_ENCODER_FRAME_EVENT_ERROR;
			if (atomic_add_unless(
				&phys_enc->pending_retire_fence_cnt, -1, 0))
				event |= event_helper;
		} else if (!ret) {
			event = SDE_ENCODER_FRAME_EVENT_DONE;
		}
	}

	SDE_EVT32(DRMID(phys_enc->parent), event, notify, ret,
			ret ? SDE_EVTLOG_FATAL : 0);
	if (phys_enc->parent_ops.handle_frame_done && event)
		phys_enc->parent_ops.handle_frame_done(
				phys_enc->parent, phys_enc,
				event);
	return ret;
}

static inline
int sde_encoder_phys_shd_wait_for_vblank(
		struct sde_encoder_phys *phys_enc)
{
	return _sde_encoder_phys_shd_wait_for_vblank(phys_enc, true);
}

static inline
int sde_encoder_phys_shd_wait_for_vblank_no_notify(
		struct sde_encoder_phys *phys_enc)
{
	return _sde_encoder_phys_shd_wait_for_vblank(phys_enc, false);
}

static inline
int sde_encoder_phys_shd_prepare_for_kickoff(
		struct sde_encoder_phys *phys_enc,
		struct sde_encoder_kickoff_params *params)
{
	return 0;
}

static inline
void sde_encoder_phys_shd_handle_post_kickoff(
	struct sde_encoder_phys *phys_enc)
{
	if (!phys_enc || !phys_enc->hw_intf) {
		SDE_ERROR("invalid encoder\n");
		return;
	}

	if (phys_enc->enable_state == SDE_ENC_ENABLING) {
		SDE_EVT32(DRMID(phys_enc->parent),
				phys_enc->hw_intf->idx - INTF_0);
		phys_enc->enable_state = SDE_ENC_ENABLED;
	}
}

static inline
void sde_encoder_phys_shd_trigger_flush(
	struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_shd *shd_enc;

	shd_enc = container_of(phys_enc, struct sde_encoder_phys_shd, base);

	sde_shd_hw_flush(phys_enc->hw_ctl,
			shd_enc->hw_lm, shd_enc->num_mixers);
}

static int sde_encoder_phys_shd_control_vblank_irq(
		struct sde_encoder_phys *phys_enc,
		bool enable)
{
	int ret = 0;
	struct sde_encoder_phys_shd *shd_enc;
	int refcount;

	if (!phys_enc || !phys_enc->hw_intf) {
		SDE_ERROR("invalid encoder\n");
		return -EINVAL;
	}

	refcount = atomic_read(&phys_enc->vblank_refcount);
	shd_enc = to_sde_encoder_phys_shd(phys_enc);

	/* protect against negative */
	if (!enable && refcount == 0) {
		ret = -EINVAL;
		goto end;
	}

	SDE_DEBUG("[%pS] %d enable=%d/%d\n",
			__builtin_return_address(0), DRMID(phys_enc->parent),
			enable, atomic_read(&phys_enc->vblank_refcount));

	SDE_EVT32(DRMID(phys_enc->parent), enable,
			atomic_read(&phys_enc->vblank_refcount));

	if (enable && atomic_inc_return(&phys_enc->vblank_refcount) == 1) {
		ret = _sde_encoder_phys_shd_register_irq(phys_enc,
				INTR_IDX_VSYNC, true);
		if (ret)
			atomic_dec_return(&phys_enc->vblank_refcount);
	} else if (!enable &&
			atomic_dec_return(&phys_enc->vblank_refcount) == 0) {
		ret = _sde_encoder_phys_shd_register_irq(phys_enc,
				INTR_IDX_VSYNC, false);
		if (ret)
			atomic_inc_return(&phys_enc->vblank_refcount);
	}

end:
	if (ret) {
		SDE_DEBUG("control vblank irq error %d, enable %d\n",
				ret, enable);
		SDE_EVT32(DRMID(phys_enc->parent),
				phys_enc->hw_intf->idx - INTF_0,
				enable, refcount, SDE_EVTLOG_ERROR);
	}
	return ret;
}

static void sde_encoder_phys_shd_enable(struct sde_encoder_phys *phys_enc)
{
	struct drm_connector *connector;

	SDE_DEBUG("%d\n", phys_enc->parent->base.id);

	if (!phys_enc->parent || !phys_enc->parent->dev) {
		SDE_ERROR("invalid drm device\n");
		return;
	}

	connector = phys_enc->connector;
	if (!connector || connector->encoder != phys_enc->parent) {
		SDE_ERROR("failed to find connector\n");
		return;
	}

	if (phys_enc->enable_state == SDE_ENC_DISABLED)
		phys_enc->enable_state = SDE_ENC_ENABLING;

	SDE_EVT32(DRMID(phys_enc->parent),
		atomic_read(&phys_enc->pending_retire_fence_cnt));
}

static void sde_encoder_phys_shd_single_vblank_wait(
		struct sde_encoder_phys *phys_enc)
{
	int ret;

	ret = sde_encoder_phys_shd_control_vblank_irq(phys_enc, true);
	if (ret) {
		SDE_ERROR_PHYS(phys_enc,
				"failed to enable vblank irq: %d\n",
				ret);
		SDE_EVT32(DRMID(phys_enc->parent),
				phys_enc->hw_intf->idx - INTF_0, ret,
				SDE_EVTLOG_FUNC_CASE1,
				SDE_EVTLOG_ERROR);
	} else {
		ret = _sde_encoder_phys_shd_wait_for_vblank(phys_enc, false);
		if (ret) {
			atomic_set(&phys_enc->pending_kickoff_cnt, 0);
			SDE_ERROR_PHYS(phys_enc,
					"failure waiting for disable: %d\n",
					ret);
			SDE_EVT32(DRMID(phys_enc->parent),
					phys_enc->hw_intf->idx - INTF_0, ret,
					SDE_EVTLOG_FUNC_CASE2,
					SDE_EVTLOG_ERROR);
		}
		sde_encoder_phys_shd_control_vblank_irq(phys_enc, false);
	}
}

static void sde_encoder_phys_shd_disable(struct sde_encoder_phys *phys_enc)
{
	struct shd_display *display;
	unsigned long lock_flags;

	SDE_DEBUG("%d\n", phys_enc->parent->base.id);

	if (!phys_enc || !phys_enc->parent || !phys_enc->parent->dev ||
			!phys_enc->parent->dev->dev_private) {
		SDE_ERROR("invalid encoder/device\n");
		return;
	}

	if (!phys_enc->hw_intf || !phys_enc->hw_ctl) {
		SDE_ERROR("invalid hw_intf %d hw_ctl %d\n",
				phys_enc->hw_intf != 0, phys_enc->hw_ctl != 0);
		return;
	}

	if (phys_enc->enable_state == SDE_ENC_DISABLED) {
		SDE_ERROR("already disabled\n");
		return;
	}

	sde_encoder_helper_reset_mixers(phys_enc, NULL);

	display = sde_connector_get_display(phys_enc->connector);
	if (!display)
		goto next;

	/* if base display is already disabled, skip vsync check */
	if (!display->base->crtc->state->active)
		goto next;

	spin_lock_irqsave(phys_enc->enc_spinlock, lock_flags);
	sde_encoder_phys_shd_trigger_flush(phys_enc);
	sde_encoder_phys_inc_pending(phys_enc);
	spin_unlock_irqrestore(phys_enc->enc_spinlock, lock_flags);

	sde_encoder_phys_shd_single_vblank_wait(phys_enc);

next:
	phys_enc->enable_state = SDE_ENC_DISABLED;

	_sde_encoder_phys_shd_rm_release(phys_enc, display);

	SDE_EVT32(DRMID(phys_enc->parent),
		atomic_read(&phys_enc->pending_retire_fence_cnt));
}

static inline
void sde_encoder_phys_shd_destroy(struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_shd *shd_enc =
		to_sde_encoder_phys_shd(phys_enc);

	if (!phys_enc)
		return;

	kfree(shd_enc);
}

static inline
void sde_encoder_phys_shd_irq_ctrl(
		struct sde_encoder_phys *phys_enc, bool enable)
{
	sde_encoder_phys_shd_control_vblank_irq(phys_enc, enable);
}

static inline
int sde_encoder_phys_shd_get_line_count(
		struct sde_encoder_phys *phys)
{
	return 0;
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
	ops->wait_for_commit_done = sde_encoder_phys_shd_wait_for_vblank;
	ops->wait_for_vblank = sde_encoder_phys_shd_wait_for_vblank_no_notify;
	ops->prepare_for_kickoff = sde_encoder_phys_shd_prepare_for_kickoff;
	ops->handle_post_kickoff = sde_encoder_phys_shd_handle_post_kickoff;
	ops->trigger_flush = sde_encoder_phys_shd_trigger_flush;
	ops->control_vblank_irq = sde_encoder_phys_shd_control_vblank_irq;
	ops->wait_for_tx_complete = sde_encoder_phys_shd_wait_for_vblank;
	ops->irq_control = sde_encoder_phys_shd_irq_ctrl;
	ops->get_line_count = sde_encoder_phys_shd_get_line_count;
}

void *sde_encoder_phys_shd_init(enum sde_intf_type type,
			u32 controller_id, void *phys_init_params)
{
	struct sde_enc_phys_init_params *p = phys_init_params;
	struct sde_encoder_phys *phys_enc;
	struct sde_encoder_phys_shd *shd_enc;
	struct sde_encoder_irq *irq;
	struct sde_shd_hw_ctl *hw_ctl;
	struct sde_shd_hw_mixer *hw_lm;
	int ret = 0, i;

	SDE_DEBUG("\n");

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
	phys_enc->intf_idx = INTF_0 + controller_id;
	phys_enc->enc_spinlock = p->enc_spinlock;
	atomic_set(&phys_enc->pending_retire_fence_cnt, 0);

	irq = &phys_enc->irq[INTR_IDX_VSYNC];
	irq->name = "vsync_irq";
	irq->intr_type = SDE_IRQ_TYPE_INTF_VSYNC;
	irq->intr_idx = INTR_IDX_VSYNC;
	irq->cb.func = sde_encoder_phys_shd_vblank_irq;

	for (i = 0; i < INTR_IDX_MAX; i++) {
		irq = &phys_enc->irq[i];
		INIT_LIST_HEAD(&irq->cb.list);
		irq->irq_idx = -EINVAL;
		irq->hw_idx = -EINVAL;
		irq->cb.arg = phys_enc;
	}

	atomic_set(&phys_enc->vblank_refcount, 0);
	atomic_set(&phys_enc->pending_kickoff_cnt, 0);
	atomic_set(&phys_enc->pending_retire_fence_cnt, 0);
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

