// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 *
 */

#include "sde_hwio.h"
#include "sde_hw_catalog.h"
#include "sde_hw_top.h"
#include "sde_dbg.h"
#include "sde_kms.h"

#define UIDLE_CTL 0x0
#define UIDLE_STATUS 0x4
#define UIDLE_FAL10_VETO_OVERRIDE 0x8
#define UIDLE_QACTIVE_HF_OVERRIDE 0xc

#define UIDLE_WD_TIMER_CTL 0x10
#define UIDLE_WD_TIMER_CTL2 0x14
#define UIDLE_WD_TIMER_LOAD_VALUE 0x18

#define UIDLE_DANGER_STATUS_0 0x20
#define UIDLE_DANGER_STATUS_1 0x24
#define UIDLE_SAFE_STATUS_0 0x30
#define UIDLE_SAFE_STATUS_1 0x34
#define UIDLE_IDLE_STATUS_0 0x38
#define UIDLE_IDLE_STATUS_1 0x3c
#define UIDLE_FAL_STATUS_0 0x40
#define UIDLE_FAL_STATUS_1 0x44

#define UIDLE_GATE_CNTR_CTL 0x50
#define UIDLE_FAL1_GATE_CNTR 0x54
#define UIDLE_FAL10_GATE_CNTR 0x58
#define UIDLE_FAL_WAIT_GATE_CNTR 0x5c
#define UIDLE_FAL1_NUM_TRANSITIONS_CNTR 0x60
#define UIDLE_FAL10_NUM_TRANSITIONS_CNTR 0x64
#define UIDLE_MIN_GATE_CNTR 0x68
#define UIDLE_MAX_GATE_CNTR 0x6c

static const struct sde_uidle_cfg *_top_offset(enum sde_uidle uidle,
		struct sde_mdss_cfg *m, void __iomem *addr,
		unsigned long len, struct sde_hw_blk_reg_map *b)
{

	/* Make sure length of regs offsets is within the mapped memory */
	if ((uidle == m->uidle_cfg.id) &&
		(m->uidle_cfg.base + m->uidle_cfg.len) < len) {

		b->base_off = addr;
		b->blk_off = m->uidle_cfg.base;
		b->length = m->uidle_cfg.len;
		b->hwversion = m->hwversion;
		b->log_mask = SDE_DBG_MASK_UIDLE;
		SDE_DEBUG("base:0x%p blk_off:0x%x length:%d hwversion:0x%x\n",
			b->base_off, b->blk_off, b->length, b->hwversion);
		return &m->uidle_cfg;
	}

	SDE_ERROR("wrong uidle mapping params, will disable UIDLE!\n");
	SDE_ERROR("base_off:0x%pK id:%d base:0x%x len:%d mmio_len:%ld\n",
		addr, m->uidle_cfg.id, m->uidle_cfg.base,
		m->uidle_cfg.len, len);
	m->uidle_cfg.uidle_rev = 0;

	return ERR_PTR(-EINVAL);
}

void sde_hw_uidle_get_status(struct sde_hw_uidle *uidle,
		struct sde_uidle_status *status)
{
	struct sde_hw_blk_reg_map *c = &uidle->hw;

	status->uidle_danger_status_0 =
		SDE_REG_READ(c, UIDLE_DANGER_STATUS_0);
	status->uidle_danger_status_1 =
		SDE_REG_READ(c, UIDLE_DANGER_STATUS_1);
	status->uidle_safe_status_0 =
		SDE_REG_READ(c, UIDLE_SAFE_STATUS_0);
	status->uidle_safe_status_1 =
		SDE_REG_READ(c, UIDLE_SAFE_STATUS_1);
	status->uidle_idle_status_0 =
		SDE_REG_READ(c, UIDLE_IDLE_STATUS_0);
	status->uidle_idle_status_1 =
		SDE_REG_READ(c, UIDLE_IDLE_STATUS_1);
	status->uidle_fal_status_0 =
		SDE_REG_READ(c, UIDLE_FAL_STATUS_0);
	status->uidle_fal_status_1 =
		SDE_REG_READ(c, UIDLE_FAL_STATUS_1);

	status->uidle_status =
		SDE_REG_READ(c, UIDLE_STATUS);
	status->uidle_en_fal10 =
		(status->uidle_status & BIT(2)) ? 1 : 0;
}

void sde_hw_uidle_get_cntr(struct sde_hw_uidle *uidle,
		struct sde_uidle_cntr *cntr)
{
	struct sde_hw_blk_reg_map *c = &uidle->hw;
	u32 reg_val;

	cntr->fal1_gate_cntr =
		SDE_REG_READ(c, UIDLE_FAL1_GATE_CNTR);
	cntr->fal10_gate_cntr =
		SDE_REG_READ(c, UIDLE_FAL10_GATE_CNTR);
	cntr->fal_wait_gate_cntr =
		SDE_REG_READ(c, UIDLE_FAL_WAIT_GATE_CNTR);
	cntr->fal1_num_transitions_cntr =
		SDE_REG_READ(c, UIDLE_FAL1_NUM_TRANSITIONS_CNTR);
	cntr->fal10_num_transitions_cntr =
		SDE_REG_READ(c, UIDLE_FAL10_NUM_TRANSITIONS_CNTR);
	cntr->min_gate_cntr =
		SDE_REG_READ(c, UIDLE_MIN_GATE_CNTR);
	cntr->max_gate_cntr =
		SDE_REG_READ(c, UIDLE_MAX_GATE_CNTR);

	/* clear counters after read */
	reg_val = SDE_REG_READ(c, UIDLE_GATE_CNTR_CTL);
	reg_val = reg_val | BIT(31);
	SDE_REG_WRITE(c, UIDLE_GATE_CNTR_CTL, reg_val);
	reg_val = (reg_val & ~BIT(31));
	SDE_REG_WRITE(c, UIDLE_GATE_CNTR_CTL, reg_val);
}

void sde_hw_uidle_setup_cntr(struct sde_hw_uidle *uidle, bool enable)
{
	struct sde_hw_blk_reg_map *c = &uidle->hw;
	u32 reg_val;

	reg_val = SDE_REG_READ(c, UIDLE_GATE_CNTR_CTL);
	reg_val = (reg_val & ~BIT(8)) | (enable ? BIT(8) : 0);

	SDE_REG_WRITE(c, UIDLE_GATE_CNTR_CTL, reg_val);
}

void sde_hw_uidle_setup_wd_timer(struct sde_hw_uidle *uidle,
		struct sde_uidle_wd_cfg *cfg)
{
	struct sde_hw_blk_reg_map *c = &uidle->hw;
	u32 val_ctl, val_ctl2, val_ld;

	val_ctl = SDE_REG_READ(c, UIDLE_WD_TIMER_CTL);
	val_ctl2 = SDE_REG_READ(c, UIDLE_WD_TIMER_CTL2);
	val_ld = SDE_REG_READ(c, UIDLE_WD_TIMER_LOAD_VALUE);

	val_ctl = (val_ctl & ~BIT(0)) | (cfg->clear ? BIT(0) : 0);

	val_ctl2 = (val_ctl2 & ~BIT(0)) | (cfg->enable ? BIT(0) : 0);
	val_ctl2 = (val_ctl2 & ~GENMASK(4, 1)) |
		((cfg->granularity & 0xF) << 1);
	val_ctl2 = (val_ctl2 & ~BIT(8)) | (cfg->heart_beat ? BIT(8) : 0);

	val_ld = cfg->load_value;

	SDE_REG_WRITE(c, UIDLE_WD_TIMER_CTL, val_ctl);
	SDE_REG_WRITE(c, UIDLE_WD_TIMER_CTL2, val_ctl2);
	SDE_REG_WRITE(c, UIDLE_WD_TIMER_LOAD_VALUE, val_ld);
}

void sde_hw_uidle_setup_ctl(struct sde_hw_uidle *uidle,
		struct sde_uidle_ctl_cfg *cfg)
{
	struct sde_hw_blk_reg_map *c = &uidle->hw;
	bool enable = false;
	u32 reg_val, fal10_veto_regval = 0;

	reg_val = SDE_REG_READ(c, UIDLE_CTL);

	enable = (cfg->uidle_state > UIDLE_STATE_DISABLE &&
		cfg->uidle_state < UIDLE_STATE_ENABLE_MAX);
	reg_val = (reg_val & ~BIT(31)) | (enable ? BIT(31) : 0);
	reg_val = (reg_val & ~BIT(30)) | (cfg->uidle_state
			== UIDLE_STATE_FAL1_ONLY ? BIT(30) : 0);

	reg_val = (reg_val & ~FAL10_DANGER_MSK) |
		((cfg->fal10_danger << FAL10_DANGER_SHFT) &
		FAL10_DANGER_MSK);
	reg_val = (reg_val & ~FAL10_EXIT_DANGER_MSK) |
		((cfg->fal10_exit_danger << FAL10_EXIT_DANGER_SHFT) &
		FAL10_EXIT_DANGER_MSK);
	reg_val = (reg_val & ~FAL10_EXIT_CNT_MSK) |
		((cfg->fal10_exit_cnt << FAL10_EXIT_CNT_SHFT) &
		FAL10_EXIT_CNT_MSK);

	SDE_REG_WRITE(c, UIDLE_CTL, reg_val);
	if (!enable)
		fal10_veto_regval |= (BIT(31) | BIT(0));

	SDE_REG_WRITE(c, UIDLE_FAL10_VETO_OVERRIDE, fal10_veto_regval);
}

static void sde_hw_uilde_active_override(struct sde_hw_uidle *uidle,
		bool enable)
{
	struct sde_hw_blk_reg_map *c = &uidle->hw;
	u32 reg_val = 0;

	if (enable)
		reg_val = BIT(0) | BIT(31);

	SDE_REG_WRITE(c, UIDLE_QACTIVE_HF_OVERRIDE, reg_val);
}

static void sde_hw_uidle_fal10_override(struct sde_hw_uidle *uidle,
		bool enable)
{
	struct sde_hw_blk_reg_map *c = &uidle->hw;
	u32 reg_val = 0;

	if (enable)
		reg_val = BIT(0) | BIT(31);

	SDE_REG_WRITE(c, UIDLE_FAL10_VETO_OVERRIDE, reg_val);
	wmb();
}

static inline void _setup_uidle_ops(struct sde_hw_uidle_ops *ops,
		unsigned long cap)
{
	ops->set_uidle_ctl = sde_hw_uidle_setup_ctl;
	ops->setup_wd_timer = sde_hw_uidle_setup_wd_timer;
	ops->uidle_setup_cntr = sde_hw_uidle_setup_cntr;
	ops->uidle_get_cntr = sde_hw_uidle_get_cntr;
	ops->uidle_get_status = sde_hw_uidle_get_status;
	if (cap & BIT(SDE_UIDLE_QACTIVE_OVERRIDE))
		ops->active_override_enable = sde_hw_uilde_active_override;
	ops->uidle_fal10_override = sde_hw_uidle_fal10_override;
}

struct sde_hw_uidle *sde_hw_uidle_init(enum sde_uidle idx,
		void __iomem *addr, unsigned long len,
		struct sde_mdss_cfg *m)
{
	struct sde_hw_uidle *c;
	const struct sde_uidle_cfg *cfg;

	c = kzalloc(sizeof(*c), GFP_KERNEL);
	if (!c)
		return ERR_PTR(-ENOMEM);

	cfg = _top_offset(idx, m, addr, len, &c->hw);
	if (IS_ERR_OR_NULL(cfg)) {
		kfree(c);
		return ERR_PTR(-EINVAL);
	}

	/*
	 * Assign ops
	 */
	c->idx = idx;
	c->cap = cfg;
	_setup_uidle_ops(&c->ops, c->cap->features);

	sde_dbg_reg_register_dump_range(SDE_DBG_NAME, "uidle", c->hw.blk_off,
		c->hw.blk_off + c->hw.length, 0);

	return c;
}

