/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * Copyright (c) 2015-2020, The Linux Foundation. All rights reserved.
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
	 * set_xin_halt - set xin client halt control
	 * @vbif: vbif context driver
	 * @xin_id: client interface identifier
	 * @enable: halt control enable
	 */
	void (*set_xin_halt)(struct sde_hw_vbif *vbif,
			u32 xin_id, bool enable);

	/**
	 * get_xin_halt_status - get xin client halt control
	 * @vbif: vbif context driver
	 * @xin_id: client interface identifier
	 * @return: halt control enable
	 */
	bool (*get_xin_halt_status)(struct sde_hw_vbif *vbif,
			u32 xin_id);

	/**
	 * set_axi_halt - set axi port halt control
	 * @vbif: vbif context driver
	 */
	void (*set_axi_halt)(struct sde_hw_vbif *vbif);

	/**
	 * get_axi_halt_status - get axi port halt control status
	 * @vbif: vbif context driver
	 */
	int (*get_axi_halt_status)(struct sde_hw_vbif *vbif);

	/**
	 * set_qos_remap - set QoS priority remap
	 * @vbif: vbif context driver
	 * @xin_id: client interface identifier
	 * @level: priority level
	 * @remap_level: remapped level
	 */
	void (*set_qos_remap)(struct sde_hw_vbif *vbif,
			u32 xin_id, u32 level, u32 remap_level);

	/**
	 * set_mem_type - set memory type
	 * @vbif: vbif context driver
	 * @xin_id: client interface identifier
	 * @value: memory type value
	 */
	void (*set_mem_type)(struct sde_hw_vbif *vbif,
			u32 xin_id, u32 value);

	/**
	 * clear_errors - clear any vbif errors
	 *	This function clears any detected pending/source errors
	 *	on the VBIF interface, and optionally returns the detected
	 *	error mask(s).
	 * @vbif: vbif context driver
	 * @pnd_errors: pointer to pending error reporting variable
	 * @src_errors: pointer to source error reporting variable
	 */
	void (*clear_errors)(struct sde_hw_vbif *vbif,
		u32 *pnd_errors, u32 *src_errors);

	/**
	 * set_write_gather_en - set write_gather enable
	 * @vbif: vbif context driver
	 * @xin_id: client interface identifier
	 */
	void (*set_write_gather_en)(struct sde_hw_vbif *vbif, u32 xin_id);
};

struct sde_hw_vbif {
	/* base */
	struct sde_hw_blk_reg_map hw;

	/* vbif */
	enum sde_vbif idx;
	const struct sde_vbif_cfg *cap;

	/* ops */
	struct sde_hw_vbif_ops ops;

	/*
	 * vbif is common across all displays, lock to serialize access.
	 * must be take by client before using any ops
	 */
	struct mutex mutex;
};

struct sde_vbif_clk_ops {
	/**
	 * setup_clk_force_ctrl - set clock force control
	 * @hw:		hw block object
	 * @clk_ctrl:	clock to be controlled
	 * @enable:	force on enable
	 * @return:	if the clock is forced-on by this function
	 */
	bool (*setup_clk_force_ctrl)(struct sde_hw_blk_reg_map *hw,
			enum sde_clk_ctrl_type clk_ctrl, bool enable);

	/**
	 * get_clk_ctrl_status - get clock control status
	 * @hw:		hw block object
	 * @clk_ctrl:	clock to be controlled
	 * @return:	0 if success, otherwise return error code
	 */
	int (*get_clk_ctrl_status)(struct sde_hw_blk_reg_map *hw,
			enum sde_clk_ctrl_type clk_ctrl);
};

/**
 * sde_vbif_clk_client - vbif client info
 * @hw:		hw block object
 * @clk_ctrl:	clock to be controlled
 * @ops:	VBIF client ops
 */
struct sde_vbif_clk_client {
	struct sde_hw_blk_reg_map *hw;
	enum sde_clk_ctrl_type clk_ctrl;
	struct sde_vbif_clk_ops ops;
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
