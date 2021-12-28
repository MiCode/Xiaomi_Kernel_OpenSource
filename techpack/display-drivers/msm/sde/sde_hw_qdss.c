// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt)     "[drm:%s:%d] " fmt, __func__, __LINE__

#include <linux/mutex.h>
#include <linux/platform_device.h>

#include "sde_kms.h"
#include "sde_dbg.h"
#include "sde_hw_qdss.h"

#define QDSS_CONFIG	0x0

static struct sde_qdss_cfg *_qdss_offset(enum sde_qdss qdss,
		struct sde_mdss_cfg *m,
		void __iomem *addr,
		struct sde_hw_blk_reg_map *b)
{
	int i;

	for (i = 0; i < m->qdss_count; i++) {
		if (qdss == m->qdss[i].id) {
			b->base_off = addr;
			b->blk_off = m->qdss[i].base;
			b->length = m->qdss[i].len;
			b->hwversion = m->hwversion;
			b->log_mask = SDE_DBG_MASK_QDSS;
			return &m->qdss[i];
		}
	}

	return ERR_PTR(-EINVAL);
}

static void sde_hw_qdss_enable_qdss_events(struct sde_hw_qdss *hw_qdss,
							bool enable)
{
	struct sde_hw_blk_reg_map *c = &hw_qdss->hw;
	u32 val;

	val = enable ? 0x100 : 0;

	if (c)
		SDE_REG_WRITE(c, QDSS_CONFIG, val);
}

static void _setup_qdss_ops(struct sde_hw_qdss_ops *ops)
{
	ops->enable_qdss_events = sde_hw_qdss_enable_qdss_events;
}

static struct sde_hw_blk_ops sde_hw_ops = {
	.start = NULL,
	.stop = NULL,
};

struct sde_hw_qdss *sde_hw_qdss_init(enum sde_qdss idx,
			void __iomem *addr,
			struct sde_mdss_cfg *m)
{
	struct sde_hw_qdss *c;
	struct sde_qdss_cfg *cfg;
	int rc;

	c = kzalloc(sizeof(*c), GFP_KERNEL);
	if (!c)
		return ERR_PTR(-ENOMEM);

	cfg = _qdss_offset(idx, m, addr, &c->hw);
	if (IS_ERR_OR_NULL(cfg)) {
		kfree(c);
		return ERR_PTR(-EINVAL);
	}

	c->idx = idx;
	c->caps = cfg;
	_setup_qdss_ops(&c->ops);

	rc = sde_hw_blk_init(&c->base, SDE_HW_BLK_QDSS, idx, &sde_hw_ops);
	if (rc) {
		SDE_ERROR("failed to init hw blk %d\n", rc);
		kfree(c);
		return ERR_PTR(rc);
	}

	sde_dbg_reg_register_dump_range(SDE_DBG_NAME, cfg->name, c->hw.blk_off,
			c->hw.blk_off + c->hw.length, c->hw.xin_id);

	return c;
}

void sde_hw_qdss_destroy(struct sde_hw_qdss *qdss)
{
	if (qdss)
		sde_hw_blk_destroy(&qdss->base);
	kfree(qdss);
}
