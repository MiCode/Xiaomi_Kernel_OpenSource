/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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

#ifndef _SDE_HW_MDP_TOP_H
#define _SDE_HW_MDP_TOP_H

#include "sde_hw_catalog.h"
#include "sde_hw_mdss.h"
#include "sde_hw_mdp_util.h"

struct sde_hw_mdp;

/**
 * struct split_pipe_cfg - pipe configuration for dual display panels
 * @en        : Enable/disable dual pipe confguration
 * @mode      : Panel interface mode
 * @intf      : Interface id for main control path
 * @pp_split  : Ping pong split is enabled or disabled
 * @split_flush_en: Allows both the paths to be flushed when master path is
 *              flushed
 */
struct split_pipe_cfg {
	bool en;
	enum sde_intf_mode mode;
	enum sde_intf intf;
	bool pp_split;
	bool split_flush_en;
};

/**
 * struct sde_hw_mdp_ops - interface to the MDP TOP Hw driver functions
 * Assumption is these functions will be called after clocks are enabled.
 * @setup_split_pipe : Programs the pipe control registers
 */
struct sde_hw_mdp_ops {
	/** setup_split_pipe() : Regsiters are not double buffered, thisk
	 * function should be called before timing control enable
	 * @mdp  : mdp top context driver
	 * @cfg  : upper and lower part of pipe configuration
	 */
	void (*setup_split_pipe)(struct sde_hw_mdp *mdp,
			struct split_pipe_cfg *p);
};

struct sde_hw_mdp {
	/* base */
	struct sde_hw_blk_reg_map hw;

	/* intf */
	enum sde_mdp idx;
	const struct sde_mdp_cfg *cap;

	/* ops */
	struct sde_hw_mdp_ops ops;
};

/**
 * sde_hw_intf_init - initializes the intf driver for the passed interface idx
 * @idx:  Interface index for which driver object is required
 * @addr: Mapped register io address of MDP
 * @m:    Pointer to mdss catalog data
 */
struct sde_hw_mdp *sde_hw_mdptop_init(enum sde_mdp idx,
		void __iomem *addr,
		const struct sde_mdss_cfg *m);

void sde_hw_mdp_destroy(struct sde_hw_mdp *mdp);

#endif /*_SDE_HW_MDP_TOP_H */
