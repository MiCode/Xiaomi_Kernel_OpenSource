/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _SDE_HW_WB_H
#define _SDE_HW_WB_H

#include "sde_hw_catalog.h"
#include "sde_hw_mdss.h"
#include "sde_hw_top.h"
#include "sde_hw_util.h"
#include "sde_hw_pingpong.h"

struct sde_hw_wb;

struct sde_hw_wb_cfg {
	struct sde_hw_fmt_layout dest;
	enum sde_intf_mode intf_mode;
	struct sde_rect roi;
	struct sde_rect crop;
	bool is_secure;
};

/**
 * enum CDP preload ahead address size
 */
enum {
	SDE_WB_CDP_PRELOAD_AHEAD_32,
	SDE_WB_CDP_PRELOAD_AHEAD_64
};

/**
 * struct sde_hw_wb_cdp_cfg : CDP configuration
 * @enable: true to enable CDP
 * @ubwc_meta_enable: true to enable ubwc metadata preload
 * @tile_amortize_enable: true to enable amortization control for tile format
 * @preload_ahead: number of request to preload ahead
 *	SDE_WB_CDP_PRELOAD_AHEAD_32,
 *	SDE_WB_CDP_PRELOAD_AHEAD_64
 */
struct sde_hw_wb_cdp_cfg {
	bool enable;
	bool ubwc_meta_enable;
	bool tile_amortize_enable;
	u32 preload_ahead;
};

/**
 * struct sde_hw_wb_qos_cfg : Writeback pipe QoS configuration
 * @danger_lut: LUT for generate danger level based on fill level
 * @safe_lut: LUT for generate safe level based on fill level
 * @creq_lut: LUT for generate creq level based on fill level
 * @danger_safe_en: enable danger safe generation
 */
struct sde_hw_wb_qos_cfg {
	u32 danger_lut;
	u32 safe_lut;
	u64 creq_lut;
	bool danger_safe_en;
};

/**
 *
 * struct sde_hw_wb_ops : Interface to the wb Hw driver functions
 *  Assumption is these functions will be called after clocks are enabled
 */
struct sde_hw_wb_ops {
	void (*setup_csc_data)(struct sde_hw_wb *ctx,
			struct sde_csc_cfg *data);

	void (*setup_outaddress)(struct sde_hw_wb *ctx,
		struct sde_hw_wb_cfg *wb);

	void (*setup_outformat)(struct sde_hw_wb *ctx,
		struct sde_hw_wb_cfg *wb);

	void (*setup_rotator)(struct sde_hw_wb *ctx,
		struct sde_hw_wb_cfg *wb);

	void (*setup_dither)(struct sde_hw_wb *ctx,
		struct sde_hw_wb_cfg *wb);

	void (*setup_cdwn)(struct sde_hw_wb *ctx,
		struct sde_hw_wb_cfg *wb);

	void (*setup_trafficshaper)(struct sde_hw_wb *ctx,
		struct sde_hw_wb_cfg *wb);

	void (*setup_roi)(struct sde_hw_wb *ctx,
		struct sde_hw_wb_cfg *wb);

	void (*setup_crop)(struct sde_hw_wb *ctx,
		struct sde_hw_wb_cfg *wb, bool crop);

	/**
	 * setup_qos_lut - setup danger, safe, creq, etc. LUTs
	 * @ctx: Pointer to pipe context
	 * @cfg: Pointer to pipe QoS configuration
	 */
	void (*setup_qos_lut)(struct sde_hw_wb *ctx,
			struct sde_hw_wb_qos_cfg *cfg);

	/**
	 * setup_cdp - setup CDP
	 * @ctx: Pointer to pipe context
	 * @cfg: Pointer to pipe CDP configuration
	 */
	void (*setup_cdp)(struct sde_hw_wb *ctx,
			struct sde_hw_wb_cdp_cfg *cfg);

	/**
	 * bind_pingpong_blk - enable/disable the connection with pp
	 * @ctx: Pointer to wb context
	 * @enable: enable/disable connection
	 * @pp: pingpong blk id
	 */
	void (*bind_pingpong_blk)(struct sde_hw_wb *ctx,
			bool enable,
			const enum sde_pingpong pp);

	/**
	 * bind_dcwb_pp_blk - enable/disable the connection with cwb pp
	 * @ctx: Pointer to wb context
	 * @enable: enable/disable connection
	 * @pp: pingpong blk id
	 */
	void (*bind_dcwb_pp_blk)(struct sde_hw_wb *ctx,
			bool enable,
			const enum sde_pingpong pp);

	/**
	 * program_cwb_ctrl - program cwb block configp
	 * @ctx: Pointer to wb context
	 * @pp_idx: Current CWB block index to poram
	 * @data_src: Source CWB/PingPong block index
	 * @dspp_out: Tap dspp output or default LM output
	 * @enable: enable or disable the CWB path to tap the output
	 */
	void (*program_cwb_ctrl)(struct sde_hw_wb *ctx, const enum sde_cwb cwb,
		const enum sde_cwb data_src, bool dspp_out, bool enable);

	/**
	 * program_dcwb_ctrl - program cwb block configp
	 * @ctx: Pointer to wb context
	 * @pp_idx: Current CWB block index to poram
	 * @data_src: Source CWB/PingPong block index
	 * @tap_location: Tap LM output, dspp output or Demura output
	 * @enable: enable or disable the CWB path to tap the output
	 */
	void (*program_dcwb_ctrl)(struct sde_hw_wb *ctx, const enum sde_dcwb cwb,
		const enum sde_cwb data_src, int tap_location, bool enable);

	/**
	 * program_cwb_dither_ctrl - program cwb dither block config
	 * @ctx: Pointer to wb context
	 * @dcwb_idx: Current Ping-Pong CWB block index to program
	 * @cfg: cwb dither data
	 * @len: the size of cwb dither data
	 * @enable: enable or disable the cwb dither
	 */
	void (*program_cwb_dither_ctrl)(struct sde_hw_wb *ctx,
		const enum sde_dcwb dcwb_idx, void *cfg, size_t len, bool enable);
};

/**
 * struct sde_hw_wb : WB driver object
 * @base: hardware block base structure
 * @hw: block hardware details
 * @catalog: back pointer to catalog
 * @mdp: pointer to associated mdp portion of the catalog
 * @idx: hardware index number within type
 * @wb_hw_caps: hardware capabilities
 * @ops: function pointers
 * @hw_mdp: MDP top level hardware block
 * @cwb_hw: CWB control hwio details
 * @dcwb_hw: DCWB control hwio details
 * @dcwb_pp_hw: DCWB PingPong control hwio details
 */
struct sde_hw_wb {
	struct sde_hw_blk base;
	struct sde_hw_blk_reg_map hw;
	struct sde_mdss_cfg *catalog;
	struct sde_mdp_cfg *mdp;

	/* wb path */
	int idx;
	const struct sde_wb_cfg *caps;

	/* ops */
	struct sde_hw_wb_ops ops;

	struct sde_hw_mdp *hw_mdp;
	struct sde_hw_blk_reg_map cwb_hw;
	struct sde_hw_blk_reg_map dcwb_hw;
	struct sde_hw_pingpong dcwb_pp_hw[DCWB_MAX - DCWB_0];
};

/**
 * sde_hw_wb - convert base object sde_hw_base to container
 * @hw: Pointer to base hardware block
 * return: Pointer to hardware block container
 */
static inline struct sde_hw_wb *to_sde_hw_wb(struct sde_hw_blk *hw)
{
	return container_of(hw, struct sde_hw_wb, base);
}

/**
 * sde_hw_wb_init(): Initializes and return writeback hw driver object.
 * @idx:  wb_path index for which driver object is required
 * @addr: mapped register io address of MDP
 * @m :   pointer to mdss catalog data
 * @hw_mdp: pointer to mdp top hw driver object
 */
struct sde_hw_wb *sde_hw_wb_init(enum sde_wb idx,
		void __iomem *addr,
		struct sde_mdss_cfg *m,
		struct sde_hw_mdp *hw_mdp);

/**
 * sde_hw_wb_destroy(): Destroy writeback hw driver object.
 * @hw_wb:  Pointer to writeback hw driver object
 */
void sde_hw_wb_destroy(struct sde_hw_wb *hw_wb);

#endif /*_SDE_HW_WB_H */
