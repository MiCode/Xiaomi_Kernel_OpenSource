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

#ifndef _SDE_HW_CTL_H
#define _SDE_HW_CTL_H

#include "sde_hw_mdss.h"
#include "sde_hw_util.h"
#include "sde_hw_catalog.h"

/**
 * sde_ctl_mode_sel: Interface mode selection
 * SDE_CTL_MODE_SEL_VID:    Video mode interface
 * SDE_CTL_MODE_SEL_CMD:    Command mode interface
 */
enum sde_ctl_mode_sel {
	SDE_CTL_MODE_SEL_VID = 0,
	SDE_CTL_MODE_SEL_CMD
};

struct sde_hw_ctl;
/**
 * struct sde_hw_stage_cfg - blending stage cfg
 * @stage
 */
struct sde_hw_stage_cfg {
	enum sde_sspp stage[CRTC_DUAL_MIXERS][SDE_STAGE_MAX][PIPES_PER_STAGE];
};

/**
 * struct sde_hw_intf_cfg :Describes how the SDE writes data to output interface
 * @intf :                 Interface id
 * @wb:                    Writeback id
 * @mode_3d:               3d mux configuration
 * @intf_mode_sel:         Interface mode, cmd / vid
 * @stream_sel:            Stream selection for multi-stream interfaces
 */
struct sde_hw_intf_cfg {
	enum sde_intf intf;
	enum sde_wb wb;
	enum sde_3d_blend_mode mode_3d;
	enum sde_ctl_mode_sel intf_mode_sel;
	int stream_sel;
};

/**
 * struct sde_hw_ctl_ops - Interface to the wb Hw driver functions
 * Assumption is these functions will be called after clocks are enabled
 */
struct sde_hw_ctl_ops {
	/**
	 * kickoff hw operation for Sw controlled interfaces
	 * DSI cmd mode and WB interface are SW controlled
	 * @ctx       : ctl path ctx pointer
	 */
	void (*trigger_start)(struct sde_hw_ctl *ctx);

	/**
	 * Clear the value of the cached pending_flush_mask
	 * No effect on hardware
	 * @ctx       : ctl path ctx pointer
	 */
	void (*clear_pending_flush)(struct sde_hw_ctl *ctx);

	/**
	 * Query the value of the cached pending_flush_mask
	 * No effect on hardware
	 * @ctx       : ctl path ctx pointer
	 */
	u32 (*get_pending_flush)(struct sde_hw_ctl *ctx);

	/**
	 * OR in the given flushbits to the cached pending_flush_mask
	 * No effect on hardware
	 * @ctx       : ctl path ctx pointer
	 * @flushbits : module flushmask
	 */
	void (*update_pending_flush)(struct sde_hw_ctl *ctx,
		u32 flushbits);

	/**
	 * Write the value of the pending_flush_mask to hardware
	 * @ctx       : ctl path ctx pointer
	 */
	void (*trigger_flush)(struct sde_hw_ctl *ctx);

	/**
	 * Setup ctl_path interface config
	 * @ctx
	 * @cfg    : interface config structure pointer
	 */
	void (*setup_intf_cfg)(struct sde_hw_ctl *ctx,
		struct sde_hw_intf_cfg *cfg);

	int (*reset)(struct sde_hw_ctl *c);

	/*
	 * wait_reset_status - checks ctl reset status
	 * @ctx       : ctl path ctx pointer
	 *
	 * This function checks the ctl reset status bit.
	 * If the reset bit is set, it keeps polling the status till the hw
	 * reset is complete.
	 * Returns: 0 on success or -error if reset incomplete within interval
	 */
	int (*wait_reset_status)(struct sde_hw_ctl *ctx);

	uint32_t (*get_bitmask_sspp)(struct sde_hw_ctl *ctx,
		enum sde_sspp blk);

	uint32_t (*get_bitmask_mixer)(struct sde_hw_ctl *ctx,
		enum sde_lm blk);

	int (*get_bitmask_dspp)(struct sde_hw_ctl *ctx,
		u32 *flushbits,
		enum sde_dspp blk);

	int (*get_bitmask_intf)(struct sde_hw_ctl *ctx,
		u32 *flushbits,
		enum sde_intf blk);

	int (*get_bitmask_cdm)(struct sde_hw_ctl *ctx,
		u32 *flushbits,
		enum sde_cdm blk);

	int (*get_bitmask_wb)(struct sde_hw_ctl *ctx,
		u32 *flushbits,
		enum sde_wb blk);

	/**
	 * Set all blend stages to disabled
	 * @ctx       : ctl path ctx pointer
	 */
	void (*clear_all_blendstages)(struct sde_hw_ctl *ctx);

	/**
	 * Configure layer mixer to pipe configuration
	 * @ctx       : ctl path ctx pointer
	 * @lm        : layer mixer enumeration
	 * @cfg       : blend stage configuration
	 */
	void (*setup_blendstage)(struct sde_hw_ctl *ctx,
		enum sde_lm lm, struct sde_hw_stage_cfg *cfg, u32 index);
};

/**
 * struct sde_hw_ctl : CTL PATH driver object
 * @hw: block register map object
 * @idx: control path index
 * @ctl_hw_caps: control path capabilities
 * @mixer_count: number of mixers
 * @mixer_hw_caps: mixer hardware capabilities
 * @pending_flush_mask: storage for pending ctl_flush managed via ops
 * @ops: operation list
 */
struct sde_hw_ctl {
	/* base */
	struct sde_hw_blk_reg_map hw;

	/* ctl path */
	int idx;
	const struct sde_ctl_cfg *caps;
	int mixer_count;
	const struct sde_lm_cfg *mixer_hw_caps;
	u32 pending_flush_mask;

	/* ops */
	struct sde_hw_ctl_ops ops;
};

/**
 * sde_hw_ctl_init(): Initializes the ctl_path hw driver object.
 * should be called before accessing every ctl path registers.
 * @idx:  ctl_path index for which driver object is required
 * @addr: mapped register io address of MDP
 * @m :   pointer to mdss catalog data
 */
struct sde_hw_ctl *sde_hw_ctl_init(enum sde_ctl idx,
		void __iomem *addr,
		struct sde_mdss_cfg *m);

/**
 * sde_hw_ctl_destroy(): Destroys ctl driver context
 * should be called to free the context
 */
void sde_hw_ctl_destroy(struct sde_hw_ctl *ctx);

#endif /*_SDE_HW_CTL_H */
