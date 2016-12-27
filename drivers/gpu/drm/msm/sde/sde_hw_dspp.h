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

#ifndef _SDE_HW_DSPP_H
#define _SDE_HW_DSPP_H

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
	 * update_igc - update dspp igc
	 * @ctx: Pointer to dspp context
	 * @cfg: Pointer to configuration
	 */
	void (*update_igc)(struct sde_hw_dspp *ctx, void *cfg);

	/**
	 * setup_pa - setup dspp pa
	 * @ctx: Pointer to dspp context
	 * @cfg: Pointer to configuration
	 */
	void (*setup_pa)(struct sde_hw_dspp *dspp, void *cfg);

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
	 * setup_pa_memcolor - setup dspp memcolor
	 * @ctx: Pointer to dspp context
	 * @cfg: Pointer to configuration
	 */
	void (*setup_pa_memcolor)(struct sde_hw_dspp *ctx, void *cfg);

	/**
	 * setup_sixzone - setup dspp six zone
	 * @ctx: Pointer to dspp context
	 * @cfg: Pointer to configuration
	 */
	void (*setup_sixzone)(struct sde_hw_dspp *dspp);

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
};

/**
 * struct sde_hw_dspp - dspp description
 * @base_off:     MDP register mapped offset
 * @blk_off:      DSPP offset relative to mdss offset
 * @length        Length of register block offset
 * @hwversion     Mdss hw version number
 * @idx:          DSPP index
 * @dspp_hw_cap:  Pointer to layer_cfg
 * @highest_bank_bit:
 * @ops:          Pointer to operations possible for this dspp
 */
struct sde_hw_dspp {
	/* base */
	 struct sde_hw_blk_reg_map hw;

	/* dspp */
	enum sde_dspp idx;
	const struct sde_dspp_cfg *cap;

	/* Ops */
	struct sde_hw_dspp_ops ops;
};

/**
 * sde_hw_dspp_init - initializes the dspp hw driver object.
 * should be called once before accessing every dspp.
 * @idx:  DSPP index for which driver object is required
 * @addr: Mapped register io address of MDP
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
