/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _SDE_HW_CTL_H
#define _SDE_HW_CTL_H

#include "sde_hw_mdss.h"
#include "sde_hw_util.h"
#include "sde_hw_catalog.h"
#include "sde_hw_sspp.h"
#include "sde_hw_blk.h"

#define INVALID_CTL_STATUS 0xfffff88e
#define CTL_MAX_DSPP_COUNT (DSPP_MAX - DSPP_0)

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

/**
 * ctl_hw_flush_type - active ctl hw types
 * SDE_HW_FLUSH_WB: WB block
 * SDE_HW_FLUSH_DSC: DSC block
 * SDE_HW_FLUSH_VDC: VDC bits of DSC block
 * SDE_HW_FLUSH_MERGE_3D: Merge 3D block
 * SDE_HW_FLUSH_CDM: CDM block
 * SDE_HW_FLUSH_CWB: CWB block
 * SDE_HW_FLUSH_PERIPH: Peripheral
 * SDE_HW_FLUSH_INTF: Interface
 */
enum ctl_hw_flush_type {
	SDE_HW_FLUSH_WB,
	SDE_HW_FLUSH_DSC,
	SDE_HW_FLUSH_VDC,
	SDE_HW_FLUSH_MERGE_3D,
	SDE_HW_FLUSH_CDM,
	SDE_HW_FLUSH_CWB,
	SDE_HW_FLUSH_PERIPH,
	SDE_HW_FLUSH_INTF,
	SDE_HW_FLUSH_MAX
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
 * struct sde_hw_intf_cfg_v1 :Describes the data strcuture to configure the
 *                            output interfaces for a particular display on a
 *                            platform which supports ctl path version 1.
 * @intf_count:               No. of active interfaces for this display
 * @intf :                    Interface ids of active interfaces
 * @intf_mode_sel:            Interface mode, cmd / vid
 * @intf_master:              Master interface for split display
 * @wb_count:                 No. of active writebacks
 * @wb:                       Writeback ids of active writebacks
 * @merge_3d_count            No. of active merge_3d blocks
 * @merge_3d:                 Id of the active merge 3d blocks
 * @cwb_count:                No. of active concurrent writebacks
 * @cwb:                      Id of active cwb blocks
 * @cdm_count:                No. of active chroma down module
 * @cdm:                      Id of active cdm blocks
 * @dsc_count:                No. of active dsc blocks
 * @dsc:                      Id of active dsc blocks
 * @vdc_count:                No. of active vdc blocks
 * @vdc:                      Id of active vdc blocks
 */
struct sde_hw_intf_cfg_v1 {
	uint32_t intf_count;
	enum sde_intf intf[MAX_INTF_PER_CTL_V1];
	enum sde_ctl_mode_sel intf_mode_sel;
	enum sde_intf intf_master;

	uint32_t wb_count;
	enum sde_wb wb[MAX_WB_PER_CTL_V1];

	uint32_t merge_3d_count;
	enum sde_merge_3d merge_3d[MAX_MERGE_3D_PER_CTL_V1];

	uint32_t cwb_count;
	enum sde_cwb cwb[MAX_CWB_PER_CTL_V1];

	uint32_t cdm_count;
	enum sde_cdm cdm[MAX_CDM_PER_CTL_V1];

	uint32_t dsc_count;
	enum sde_dsc dsc[MAX_DSC_PER_CTL_V1];

	uint32_t vdc_count;
	enum sde_vdc vdc[MAX_VDC_PER_CTL_V1];
};

/**
 * struct sde_ctl_flush_cfg - struct describing flush configuration managed
 * via set, trigger and clear ops.
 * set ops corresponding to the hw_block is called, when the block's
 * configuration is changed and needs to be committed on Hw. Flush mask caches
 * the different bits for the ongoing commit.
 * clear ops clears the bitmask and cancels the update to the corresponding
 * hw block.
 * trigger op will trigger the update on the hw for the blocks cached in the
 * pending flush mask.
 *
 * @pending_flush_mask: pending ctl_flush
 * CTL path version SDE_CTL_CFG_VERSION_1_0_0 has * two level flush mechanism
 * for lower pipe controls. individual control should be flushed before
 * exercising top level flush
 * @pending_hw_flush_mask: pending flush mask for each active HW blk
 * @pending_dspp_flush_masks: pending flush masks for sub-blks of each DSPP
 */
struct sde_ctl_flush_cfg {
	u32 pending_flush_mask;
	u32 pending_hw_flush_mask[SDE_HW_FLUSH_MAX];
	u32 pending_dspp_flush_masks[CTL_MAX_DSPP_COUNT];
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
	 * @Return: error code
	 */
	int (*trigger_start)(struct sde_hw_ctl *ctx);

	/**
	 * kickoff prepare is in progress hw operation for sw
	 * controlled interfaces: DSI cmd mode and WB interface
	 * are SW controlled
	 * @ctx       : ctl path ctx pointer
	 * @Return: error code
	 */
	int (*trigger_pending)(struct sde_hw_ctl *ctx);

	/**
	 * kickoff rotator operation for Sw controlled interfaces
	 * DSI cmd mode and WB interface are SW controlled
	 * @ctx       : ctl path ctx pointer
	 * @Return: error code
	 */
	int (*trigger_rot_start)(struct sde_hw_ctl *ctx);

	/**
	 * enable/disable UIDLE feature
	 * @ctx       : ctl path ctx pointer
	 * @enable: true to enable the feature
	 */
	void (*uidle_enable)(struct sde_hw_ctl *ctx, bool enable);

	/**
	 * Clear the value of the cached pending_flush_mask
	 * No effect on hardware
	 * @ctx       : ctl path ctx pointer
	 * @Return: error code
	 */
	int (*clear_pending_flush)(struct sde_hw_ctl *ctx);

	/**
	 * Query the value of the cached pending_flush_mask
	 * No effect on hardware
	 * @ctx       : ctl path ctx pointer
	 * @cfg       : current flush configuration
	 * @Return: error code
	 */
	int (*get_pending_flush)(struct sde_hw_ctl *ctx,
			struct sde_ctl_flush_cfg *cfg);

	/**
	 * OR in the given flushbits to the flush_cfg
	 * No effect on hardware
	 * @ctx       : ctl path ctx pointer
	 * @cfg     : flush configuration pointer
	 * @Return: error code
	 */
	int (*update_pending_flush)(struct sde_hw_ctl *ctx,
		struct sde_ctl_flush_cfg *cfg);

	/**
	 * Write the value of the pending_flush_mask to hardware
	 * @ctx       : ctl path ctx pointer
	 * @Return: error code
	 */
	int (*trigger_flush)(struct sde_hw_ctl *ctx);

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
	 * @Return: error code
	 */
	int (*setup_intf_cfg)(struct sde_hw_ctl *ctx,
		struct sde_hw_intf_cfg *cfg);

	/**
	 * Reset ctl_path interface config
	 * @ctx   : ctl path ctx pointer
	 * @cfg    : interface config structure pointer
	 * @merge_3d_idx	: index of merge3d blk
	 * @Return: error code
	 */
	int (*reset_post_disable)(struct sde_hw_ctl *ctx,
		struct sde_hw_intf_cfg_v1 *cfg, u32 merge_3d_idx);

	/** update cwb  for ctl_path
	 * @ctx       : ctl path ctx pointer
	 * @cfg    : interface config structure pointer
	 * @enable    : enable/disable the dynamic sub-blocks in interface cfg
	 * @Return: error code
	 */
	int (*update_intf_cfg)(struct sde_hw_ctl *ctx,
		struct sde_hw_intf_cfg_v1 *cfg, bool enable);

	/**
	 * Setup ctl_path interface config for SDE_CTL_ACTIVE_CFG
	 * @ctx   : ctl path ctx pointer
	 * @cfg    : interface config structure pointer
	 * @Return: error code
	 */
	int (*setup_intf_cfg_v1)(struct sde_hw_ctl *ctx,
		struct sde_hw_intf_cfg_v1 *cfg);

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
	 * get_scheduler_reset - check ctl scheduler status bit
	 * @ctx    : ctl path ctx pointer
	 * Returns: current value of ctl scheduler and idle status
	 */
	u32 (*get_scheduler_status)(struct sde_hw_ctl *ctx);

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

	/**
	 * update_bitmask_sspp: updates mask corresponding to sspp
	 * @blk               : blk id
	 * @enable            : true to enable, 0 to disable
	 */
	int (*update_bitmask_sspp)(struct sde_hw_ctl *ctx,
		enum sde_sspp blk, bool enable);

	/**
	 * update_bitmask_mixer: updates mask corresponding to mixer
	 * @blk               : blk id
	 * @enable            : true to enable, 0 to disable
	 */
	int (*update_bitmask_mixer)(struct sde_hw_ctl *ctx,
		enum sde_lm blk, bool enable);

	/**
	 * update_bitmask_dspp: updates mask corresponding to dspp
	 * @blk               : blk id
	 * @enable            : true to enable, 0 to disable
	 */
	int (*update_bitmask_dspp)(struct sde_hw_ctl *ctx,
		enum sde_dspp blk, bool enable);

	/**
	 * update_bitmask_dspp_pavlut: updates mask corresponding to dspp pav
	 * @blk               : blk id
	 * @enable            : true to enable, 0 to disable
	 */
	int (*update_bitmask_dspp_pavlut)(struct sde_hw_ctl *ctx,
		enum sde_dspp blk, bool enable);

	/**
	 * Program DSPP sub block specific bit of dspp flush register.
	 * @ctx       : ctl path ctx pointer
	 * @dspp      : HW block ID of dspp block
	 * @sub_blk   : enum of DSPP sub block to flush
	 * @enable    : true to enable, 0 to disable
	 *
	 * This API is for CTL with DSPP flush hierarchy registers.
	 */
	int (*update_bitmask_dspp_subblk)(struct sde_hw_ctl *ctx,
			enum sde_dspp dspp, u32 sub_blk, bool enable);

	/**
	 * update_bitmask_sspp: updates mask corresponding to sspp
	 * @blk               : blk id
	 * @enable            : true to enable, 0 to disable
	 */
	int (*update_bitmask_rot)(struct sde_hw_ctl *ctx,
		enum sde_rot blk, bool enable);

	/**
	 * update_bitmask: updates flush mask
	 * @type              : blk type to flush
	 * @blk_idx           : blk idx
	 * @enable            : true to enable, 0 to disable
	 */
	int (*update_bitmask)(struct sde_hw_ctl *ctx,
			enum ctl_hw_flush_type type, u32 blk_idx, bool enable);

	/**
	 * get interfaces for the active CTL .
	 * @ctx		: ctl path ctx pointer
	 * @return	: bit mask with the active interfaces for the CTL
	 */
	u32 (*get_ctl_intf)(struct sde_hw_ctl *ctx);

	/**
	 * read CTL layers register value and return
	 * the data.
	 * @ctx       : ctl path ctx pointer
	 * @index       : layer index for this ctl path
	 * @return	: CTL layers register value
	 */
	u32 (*read_ctl_layers)(struct sde_hw_ctl *ctx, int index);

	/**
	 * read active register configuration for this block
	 * @ctx       : ctl path ctx pointer
	 * @blk       : hw blk type, supported blocks are DSC, MERGE_3D, INTF,
	 *              CDM, WB
	 * @index     : blk index
	 * @return    : true if blk at idx is active or false
	 */
	bool (*read_active_status)(struct sde_hw_ctl *ctx,
			enum sde_hw_blk_type blk, int index);

	/**
	 * Set all blend stages to disabled
	 * @ctx       : ctl path ctx pointer
	 */
	void (*clear_all_blendstages)(struct sde_hw_ctl *ctx);

	/**
	 * Configure layer mixer to pipe configuration
	 * @ctx           : ctl path ctx pointer
	 * @lm            : layer mixer enumeration
	 * @cfg           : blend stage configuration
	 * @disable_border: if true disable border, else enable border out
	 */
	void (*setup_blendstage)(struct sde_hw_ctl *ctx,
		enum sde_lm lm, struct sde_hw_stage_cfg *cfg,
		bool disable_border);

	/**
	 * Get all the sspp staged on a layer mixer
	 * @ctx       : ctl path ctx pointer
	 * @lm        : layer mixer enumeration
	 * @info      : structure to populate connected sspp index info
	 * @Return: count of sspps info elements populated
	 */
	u32 (*get_staged_sspp)(struct sde_hw_ctl *ctx, enum sde_lm lm,
		struct sde_sspp_index_info *info);

	/**
	 * Flush the reg dma by sending last command.
	 * @ctx       : ctl path ctx pointer
	 * @blocking  : if set to true api will block until flush is done
	 * @Return: error code
	 */
	int (*reg_dma_flush)(struct sde_hw_ctl *ctx, bool blocking);

	/**
	 * check if ctl start trigger state to confirm the frame pending
	 * status
	 * @ctx       : ctl path ctx pointer
	 * @Return: error code
	 */
	int (*get_start_state)(struct sde_hw_ctl *ctx);

	/**
	 * set the active fetch pipes attached to this CTL
	 * @ctx         : ctl path ctx pointer
	 * @fetch_active: bitmap of enum sde_sspp pipes attached
	 */
	void (*set_active_pipes)(struct sde_hw_ctl *ctx,
			unsigned long *fetch_active);

	/**
	 * Get all the sspp marked for fetching on the control path.
	 * @ctx       : ctl path ctx pointer
	 * @Return: bitmap of enum sde_sspp pipes found
	 */
	u32 (*get_active_pipes)(struct sde_hw_ctl *ctx);
};

/**
 * struct sde_hw_ctl : CTL PATH driver object
 * @base: hardware block base structure
 * @hw: block register map object
 * @idx: control path index
 * @caps: control path capabilities
 * @mixer_count: number of mixers
 * @mixer_hw_caps: mixer hardware capabilities
 * @flush: storage for pending ctl_flush managed via ops
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
	struct sde_ctl_flush_cfg flush;

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
