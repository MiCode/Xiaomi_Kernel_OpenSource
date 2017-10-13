/* Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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

#ifndef _SDE_HW_DSPP_H
#define _SDE_HW_DSPP_H

#include "sde_hw_blk.h"

struct sde_hw_dspp;

/**
 * struct sde_hw_dspp_ops - interface to the dspp hardware driver functions
 * Caller must call the init function to get the dspp context for each dspp
 * Assumption is these functions will be called after clocks are enabled
 */
struct sde_hw_dspp_ops {
	/**
	 * setup_histogram - setup dspp histogram
	 * @ctx: Pointer to dspp context
	 * @cfg: Pointer to configuration
	 */
	void (*setup_histogram)(struct sde_hw_dspp *ctx, void *cfg);

	/**
	 * read_histogram - read dspp histogram
	 * @ctx: Pointer to dspp context
	 * @cfg: Pointer to configuration
	 */
	void (*read_histogram)(struct sde_hw_dspp *ctx, void *cfg);

	/**
	 * lock_histogram - lock dspp histogram buffer
	 * @ctx: Pointer to dspp context
	 * @cfg: Pointer to configuration
	 */
	void (*lock_histogram)(struct sde_hw_dspp *ctx, void *cfg);

	/**
	 * setup_igc - update dspp igc
	 * @ctx: Pointer to dspp context
	 * @cfg: Pointer to configuration
	 */
	void (*setup_igc)(struct sde_hw_dspp *ctx, void *cfg);

	/**
	 * setup_pa_hsic - setup dspp pa hsic
	 * @ctx: Pointer to dspp context
	 * @cfg: Pointer to configuration
	 */
	void (*setup_pa_hsic)(struct sde_hw_dspp *dspp, void *cfg);

	/**
	 * setup_pcc - setup dspp pcc
	 * @ctx: Pointer to dspp context
	 * @cfg: Pointer to configuration
	 */
	void (*setup_pcc)(struct sde_hw_dspp *ctx, void *cfg);

	/**
	 * setup_sharpening - setup dspp sharpening
	 * @ctx: Pointer to dspp context
	 * @cfg: Pointer to configuration
	 */
	void (*setup_sharpening)(struct sde_hw_dspp *ctx, void *cfg);

	/**
	 * setup_pa_memcol_skin - setup dspp memcolor skin
	 * @ctx: Pointer to dspp context
	 * @cfg: Pointer to configuration
	 */
	void (*setup_pa_memcol_skin)(struct sde_hw_dspp *ctx, void *cfg);

	/**
	 * setup_pa_memcol_sky - setup dspp memcolor sky
	 * @ctx: Pointer to dspp context
	 * @cfg: Pointer to configuration
	 */
	void (*setup_pa_memcol_sky)(struct sde_hw_dspp *ctx, void *cfg);

	/**
	 * setup_pa_memcol_foliage - setup dspp memcolor foliage
	 * @ctx: Pointer to dspp context
	 * @cfg: Pointer to configuration
	 */
	void (*setup_pa_memcol_foliage)(struct sde_hw_dspp *ctx, void *cfg);

	/**
	 * setup_pa_memcol_prot - setup dspp memcolor protection
	 * @ctx: Pointer to dspp context
	 * @cfg: Pointer to configuration
	 */
	void (*setup_pa_memcol_prot)(struct sde_hw_dspp *ctx, void *cfg);

	/**
	 * setup_sixzone - setup dspp six zone
	 * @ctx: Pointer to dspp context
	 * @cfg: Pointer to configuration
	 */
	void (*setup_sixzone)(struct sde_hw_dspp *dspp, void *cfg);

	/**
	 * setup_danger_safe - setup danger safe LUTS
	 * @ctx: Pointer to dspp context
	 * @cfg: Pointer to configuration
	 */
	void (*setup_danger_safe)(struct sde_hw_dspp *ctx, void *cfg);

	/**
	 * setup_dither - setup dspp dither
	 * @ctx: Pointer to dspp context
	 * @cfg: Pointer to configuration
	 */
	void (*setup_dither)(struct sde_hw_dspp *ctx, void *cfg);

	/**
	 * setup_vlut - setup dspp PA VLUT
	 * @ctx: Pointer to dspp context
	 * @cfg: Pointer to configuration
	 */
	void (*setup_vlut)(struct sde_hw_dspp *ctx, void *cfg);

	/**
	 * setup_gc - update dspp gc
	 * @ctx: Pointer to dspp context
	 * @cfg: Pointer to configuration
	 */
	void (*setup_gc)(struct sde_hw_dspp *ctx, void *cfg);

	/**
	 * setup_gamut - update dspp gamut
	 * @ctx: Pointer to dspp context
	 * @cfg: Pointer to configuration
	 */
	void (*setup_gamut)(struct sde_hw_dspp *ctx, void *cfg);

	/**
	 * validate_ad - check if ad property can be set
	 * @ctx: Pointer to dspp context
	 * @prop: Pointer to ad property being validated
	 */
	int (*validate_ad)(struct sde_hw_dspp *ctx, u32 *prop);

	/**
	 * setup_ad - update the ad property
	 * @ctx: Pointer to dspp context
	 * @cfg: Pointer to ad configuration
	 */
	void (*setup_ad)(struct sde_hw_dspp *ctx, void *cfg);

	/**
	 * ad_read_intr_resp - function to get interrupt response for ad
	 * @event: Event for which response needs to be read
	 * @resp: Pointer to u32 where response value is dumped.
	 */
	void (*ad_read_intr_resp)(struct sde_hw_dspp *ctx, u32 event,
			u32 *resp);

};

/**
 * struct sde_hw_dspp - dspp description
 * @base: Hardware block base structure
 * @hw: Block hardware details
 * @hw_top: Block hardware top details
 * @idx: DSPP index
 * @cap: Pointer to layer_cfg
 * @ops: Pointer to operations possible for this DSPP
 */
struct sde_hw_dspp {
	struct sde_hw_blk base;
	struct sde_hw_blk_reg_map hw;

	/* dspp top */
	struct sde_hw_blk_reg_map hw_top;

	/* dspp */
	enum sde_dspp idx;
	const struct sde_dspp_cfg *cap;

	/* Ops */
	struct sde_hw_dspp_ops ops;
};

/**
 * sde_hw_dspp - convert base object sde_hw_base to container
 * @hw: Pointer to base hardware block
 * return: Pointer to hardware block container
 */
static inline struct sde_hw_dspp *to_sde_hw_dspp(struct sde_hw_blk *hw)
{
	return container_of(hw, struct sde_hw_dspp, base);
}

/**
 * sde_hw_dspp_init - initializes the dspp hw driver object.
 * should be called once before accessing every dspp.
 * @idx:  DSPP index for which driver object is required
 * @addr: Mapped register io address of MDP
 * @Return: pointer to structure or ERR_PTR
 */
struct sde_hw_dspp *sde_hw_dspp_init(enum sde_dspp idx,
			void __iomem *addr,
			struct sde_mdss_cfg *m);

/**
 * sde_hw_dspp_destroy(): Destroys DSPP driver context
 * @dspp:   Pointer to DSPP driver context
 */
void sde_hw_dspp_destroy(struct sde_hw_dspp *dspp);

#endif /*_SDE_HW_DSPP_H */
