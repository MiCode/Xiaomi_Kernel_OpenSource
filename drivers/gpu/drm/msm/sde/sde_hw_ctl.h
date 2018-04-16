/* Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
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
#include "sde_hw_sspp.h"
#include "sde_hw_blk.h"

/**
 * sde_ctl_mode_sel: Interface mode selection
 * SDE_CTL_MODE_SEL_VID:    Video mode interface
 * SDE_CTL_MODE_SEL_CMD:    Command mode interface
 */
enum sde_ctl_mode_sel {
	SDE_CTL_MODE_SEL_VID = 0,
	SDE_CTL_MODE_SEL_CMD
};

/**
 * sde_ctl_rot_op_mode - inline rotation mode
 * SDE_CTL_ROT_OP_MODE_OFFLINE: offline rotation
 * SDE_CTL_ROT_OP_MODE_RESERVED: reserved
 * SDE_CTL_ROT_OP_MODE_INLINE_SYNC: inline rotation synchronous mode
 * SDE_CTL_ROT_OP_MODE_INLINE_ASYNC: inline rotation asynchronous mode
 */
enum sde_ctl_rot_op_mode {
	SDE_CTL_ROT_OP_MODE_OFFLINE,
	SDE_CTL_ROT_OP_MODE_RESERVED,
	SDE_CTL_ROT_OP_MODE_INLINE_SYNC,
	SDE_CTL_ROT_OP_MODE_INLINE_ASYNC,
};

struct sde_hw_ctl;
/**
 * struct sde_hw_stage_cfg - blending stage cfg
 * @stage : SSPP_ID at each stage
 * @multirect_index: index of the rectangle of SSPP.
 */
struct sde_hw_stage_cfg {
	enum sde_sspp stage[SDE_STAGE_MAX][PIPES_PER_STAGE];
	enum sde_sspp_multirect_index multirect_index
					[SDE_STAGE_MAX][PIPES_PER_STAGE];
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
 * struct sde_ctl_sbuf_cfg - control for stream buffer configuration
 * @rot_op_mode: rotator operation mode
 */
struct sde_ctl_sbuf_cfg {
	enum sde_ctl_rot_op_mode rot_op_mode;
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
	 * kickoff prepare is in progress hw operation for sw
	 * controlled interfaces: DSI cmd mode and WB interface
	 * are SW controlled
	 * @ctx       : ctl path ctx pointer
	 */
	void (*trigger_pending)(struct sde_hw_ctl *ctx);

	/**
	 * kickoff rotator operation for Sw controlled interfaces
	 * DSI cmd mode and WB interface are SW controlled
	 * @ctx       : ctl path ctx pointer
	 */
	void (*trigger_rot_start)(struct sde_hw_ctl *ctx);

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
	 * Read the value of the flush register
	 * @ctx       : ctl path ctx pointer
	 * @Return: value of the ctl flush register.
	 */
	u32 (*get_flush_register)(struct sde_hw_ctl *ctx);

	/**
	 * Setup ctl_path interface config
	 * @ctx
	 * @cfg    : interface config structure pointer
	 */
	void (*setup_intf_cfg)(struct sde_hw_ctl *ctx,
		struct sde_hw_intf_cfg *cfg);

	/**
	 * Update the interface selection with input WB config
	 * @ctx       : ctl path ctx pointer
	 * @cfg       : pointer to input wb config
	 * @enable    : set if true, clear otherwise
	 */
	void (*update_wb_cfg)(struct sde_hw_ctl *ctx,
		struct sde_hw_intf_cfg *cfg, bool enable);

	int (*reset)(struct sde_hw_ctl *c);

	/**
	 * get_reset - check ctl reset status bit
	 * @ctx    : ctl path ctx pointer
	 * Returns: current value of ctl reset status
	 */
	u32 (*get_reset)(struct sde_hw_ctl *ctx);

	/**
	 * hard_reset - force reset on ctl_path
	 * @ctx    : ctl path ctx pointer
	 * @enable : whether to enable/disable hard reset
	 */
	void (*hard_reset)(struct sde_hw_ctl *c, bool enable);

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

	int (*get_bitmask_dspp_pavlut)(struct sde_hw_ctl *ctx,
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

	int (*get_bitmask_rot)(struct sde_hw_ctl *ctx,
		u32 *flushbits,
		enum sde_rot blk);

	/**
	 * read CTL_TOP register value and return
	 * the data.
	 * @ctx		: ctl path ctx pointer
	 * @return	: CTL top register value
	 */
	u32 (*read_ctl_top)(struct sde_hw_ctl *ctx);

	/**
	 * read CTL layers register value and return
	 * the data.
	 * @ctx       : ctl path ctx pointer
	 * @index       : layer index for this ctl path
	 * @return	: CTL layers register value
	 */
	u32 (*read_ctl_layers)(struct sde_hw_ctl *ctx, int index);

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
		enum sde_lm lm, struct sde_hw_stage_cfg *cfg);

	void (*setup_sbuf_cfg)(struct sde_hw_ctl *ctx,
		struct sde_ctl_sbuf_cfg *cfg);

	/**
	 * Flush the reg dma by sending last command.
	 * @ctx       : ctl path ctx pointer
	 * @blocking  : if set to true api will block until flush is done
	 */
	void (*reg_dma_flush)(struct sde_hw_ctl *ctx, bool blocking);

	/**
	 * check if ctl start trigger state to confirm the frame pending
	 * status
	 * @ctx       : ctl path ctx pointer
	 */
	int (*get_start_state)(struct sde_hw_ctl *ctx);
};

/**
 * struct sde_hw_ctl : CTL PATH driver object
 * @base: hardware block base structure
 * @hw: block register map object
 * @idx: control path index
 * @caps: control path capabilities
 * @mixer_count: number of mixers
 * @mixer_hw_caps: mixer hardware capabilities
 * @pending_flush_mask: storage for pending ctl_flush managed via ops
 * @ops: operation list
 */
struct sde_hw_ctl {
	struct sde_hw_blk base;
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
 * sde_hw_ctl - convert base object sde_hw_base to container
 * @hw: Pointer to base hardware block
 * return: Pointer to hardware block container
 */
static inline struct sde_hw_ctl *to_sde_hw_ctl(struct sde_hw_blk *hw)
{
	return container_of(hw, struct sde_hw_ctl, base);
}

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
