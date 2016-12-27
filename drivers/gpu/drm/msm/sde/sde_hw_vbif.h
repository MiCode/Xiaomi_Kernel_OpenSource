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

#ifndef _SDE_HW_VBIF_H
#define _SDE_HW_VBIF_H

#include "sde_hw_catalog.h"
#include "sde_hw_mdss.h"
#include "sde_hw_util.h"

struct sde_hw_vbif;

/**
 * struct sde_hw_vbif_ops : Interface to the VBIF hardware driver functions
 *  Assumption is these functions will be called after clocks are enabled
 */
struct sde_hw_vbif_ops {
	/**
	 * set_limit_conf - set transaction limit config
	 * @vbif: vbif context driver
	 * @xin_id: client interface identifier
	 * @rd: true for read limit; false for write limit
	 * @limit: outstanding transaction limit
	 */
	void (*set_limit_conf)(struct sde_hw_vbif *vbif,
			u32 xin_id, bool rd, u32 limit);

	/**
	 * get_limit_conf - get transaction limit config
	 * @vbif: vbif context driver
	 * @xin_id: client interface identifier
	 * @rd: true for read limit; false for write limit
	 * @return: outstanding transaction limit
	 */
	u32 (*get_limit_conf)(struct sde_hw_vbif *vbif,
			u32 xin_id, bool rd);

	/**
	 * set_halt_ctrl - set halt control
	 * @vbif: vbif context driver
	 * @xin_id: client interface identifier
	 * @enable: halt control enable
	 */
	void (*set_halt_ctrl)(struct sde_hw_vbif *vbif,
			u32 xin_id, bool enable);

	/**
	 * get_halt_ctrl - get halt control
	 * @vbif: vbif context driver
	 * @xin_id: client interface identifier
	 * @return: halt control enable
	 */
	bool (*get_halt_ctrl)(struct sde_hw_vbif *vbif,
			u32 xin_id);
};

struct sde_hw_vbif {
	/* base */
	struct sde_hw_blk_reg_map hw;

	/* vbif */
	enum sde_vbif idx;
	const struct sde_vbif_cfg *cap;

	/* ops */
	struct sde_hw_vbif_ops ops;
};

/**
 * sde_hw_vbif_init - initializes the vbif driver for the passed interface idx
 * @idx:  Interface index for which driver object is required
 * @addr: Mapped register io address of MDSS
 * @m:    Pointer to mdss catalog data
 */
struct sde_hw_vbif *sde_hw_vbif_init(enum sde_vbif idx,
		void __iomem *addr,
		const struct sde_mdss_cfg *m);

void sde_hw_vbif_destroy(struct sde_hw_vbif *vbif);

#endif /*_SDE_HW_VBIF_H */
