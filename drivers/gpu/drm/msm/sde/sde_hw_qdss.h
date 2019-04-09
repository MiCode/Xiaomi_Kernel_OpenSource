/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
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

#ifndef _SDE_HW_QDSS_H
#define _SDE_HW_QDSS_H

#include "sde_hw_catalog.h"
#include "sde_hw_mdss.h"
#include "sde_hw_blk.h"
#include "sde_hw_util.h"

struct sde_hw_qdss;

/**
 * struct sde_hw_qdss_ops - interface to the qdss hardware driver functions
 * Assumption is these functions will be called after clocks are enabled
 */
struct sde_hw_qdss_ops {
	/**
	 * enable_qdss_events - enable qdss events
	 * @hw_qdss: Pointer to qdss context
	 */
	void (*enable_qdss_events)(struct sde_hw_qdss *hw_qdss, bool enable);
};

struct sde_hw_qdss {
	struct sde_hw_blk base;
	struct sde_hw_blk_reg_map hw;

	/* qdss */
	enum sde_qdss idx;
	const struct sde_qdss_cfg *caps;

	/* ops */
	struct sde_hw_qdss_ops ops;
};

/**
 * to_sde_hw_qdss - convert base object sde_hw_base to container
 * @hw: Pointer to base hardware block
 * return: Pointer to hardware block container
 */
static inline struct sde_hw_qdss *to_sde_hw_qdss(struct sde_hw_blk *hw)
{
	return container_of(hw, struct sde_hw_qdss, base);
}

/**
 * sde_hw_qdss_init - initializes the qdss block for the passed qdss idx
 * @idx:  QDSS index for which driver object is required
 * @addr: Mapped register io address of MDP
 * @m:    Pointer to mdss catalog data
 * Returns: Error code or allocated sde_hw_qdss context
 */
struct sde_hw_qdss *sde_hw_qdss_init(enum sde_qdss idx,
				void __iomem *addr,
				struct sde_mdss_cfg *m);

/**
 * sde_hw_qdss_destroy - destroys qdss driver context
 *			 should be called to free the context
 * @qdss: Pointer to qdss driver context returned by sde_hw_qdss_init
 */
void sde_hw_qdss_destroy(struct sde_hw_qdss *qdss);

#endif /*_SDE_HW_QDSS_H */
