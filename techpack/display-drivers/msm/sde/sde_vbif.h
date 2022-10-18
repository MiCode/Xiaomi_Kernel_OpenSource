/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
 */

#ifndef __SDE_VBIF_H__
#define __SDE_VBIF_H__

#include "sde_kms.h"

struct sde_vbif_set_ot_params {
	u32 xin_id;
	u32 num;
	u32 width;
	u32 height;
	u32 frame_rate;
	bool rd;
	bool is_wfd;
	u32 vbif_idx;
	u32 clk_ctrl;
};

struct sde_vbif_set_memtype_params {
	u32 xin_id;
	u32 vbif_idx;
	u32 clk_ctrl;
	bool is_cacheable;
};

/**
 * struct sde_vbif_set_xin_halt_params - xin halt parameters
 * @vbif_idx: vbif identifier
 * @xin_id: client interface identifier
 * @clk_ctrl: clock control identifier of the xin
 * @forced_on: whether or not previous call to xin halt forced the clocks on,
 *	only applicable to xin halt disable calls
 * @enable: whether to enable/disable xin halts
 */
struct sde_vbif_set_xin_halt_params {
	u32 vbif_idx;
	u32 xin_id;
	u32 clk_ctrl;
	bool forced_on;
	bool enable;
};

/**
 * struct sde_vbif_get_xin_status_params - xin halt parameters
 * @vbif_idx: vbif identifier
 * @xin_id: client interface identifier
 * @clk_ctrl: clock control identifier of the xin
 */
struct sde_vbif_get_xin_status_params {
	u32 vbif_idx;
	u32 xin_id;
	u32 clk_ctrl;
};

/**
 * struct sde_vbif_set_qos_params - QoS remapper parameter
 * @vbif_idx: vbif identifier
 * @xin_id: client interface identifier
 * @clk_ctrl: clock control identifier of the xin
 * @num: pipe identifier (debug only)
 * @client_type: client type enumerated by sde_vbif_client_type
 */
struct sde_vbif_set_qos_params {
	u32 vbif_idx;
	u32 xin_id;
	u32 clk_ctrl;
	u32 num;
	enum sde_vbif_client_type client_type;
};

/**
 * sde_vbif_clk_register - register vbif clk client
 * @sde_kms:	SDE handler
 * @client:	pointer to VBIF clk client info
 * Returns:	0 on success, error code otherwise
 */
int sde_vbif_clk_register(struct sde_kms *sde_kms, struct sde_vbif_clk_client *client);

/**
 * sde_vbif_set_ot_limit - set OT limit for vbif client
 * @sde_kms:	SDE handler
 * @params:	Pointer to OT configuration parameters
 */
void sde_vbif_set_ot_limit(struct sde_kms *sde_kms,
		struct sde_vbif_set_ot_params *params);

/**
 * sde_vbif_set_xin_halt - halt one of the xin ports
 *	This function isn't thread safe.
 * @sde_kms:	SDE handler
 * @params:	Pointer to halt configuration parameters
 * Returns:	Whether or not VBIF clocks were forced on
 */
bool sde_vbif_set_xin_halt(struct sde_kms *sde_kms,
		struct sde_vbif_set_xin_halt_params *params);

/**
 * sde_vbif_get_xin_status - halt one of the xin ports
 *	This function isn't thread safe.
 * @sde_kms:	SDE handler
 * @params:	Pointer to xin status parameters
 * Returns:	true if xin client is idle, false otherwise
 */
bool sde_vbif_get_xin_status(struct sde_kms *sde_kms,
		struct sde_vbif_get_xin_status_params *params);

/**
 * sde_vbif_set_qos_remap - set QoS priority level remap
 * @sde_kms:	SDE handler
 * @params:	Pointer to QoS configuration parameters
 */
void sde_vbif_set_qos_remap(struct sde_kms *sde_kms,
		struct sde_vbif_set_qos_params *params);

/**
 * sde_vbif_clear_errors - clear any vbif errors
 * @sde_kms:	SDE handler
 */
void sde_vbif_clear_errors(struct sde_kms *sde_kms);

/**
 * sde_vbif_init_memtypes - initialize xin memory types for vbif
 * @sde_kms:	SDE handler
 */
void sde_vbif_init_memtypes(struct sde_kms *sde_kms);

/**
 * sde_vbif_axi_halt_request - halt all axi transcations on vbif
 * @sde_kms:	SDE handler
 */
void sde_vbif_axi_halt_request(struct sde_kms *sde_kms);

/**
 * sde_vbif_halt_plane_xin - halts the xin client for the unused plane
 * On unused plane, check if the vbif for this plane is idle or not.
 * If not then first force_on the planes clock and then send the
 * halt request. Wait for some time then check for the vbif idle
 * or not again.
 * @sde_kms:	SDE handler
 * @xin_id:	xin id of the unused plane
 * @clk_ctrl:	clk ctrl type for the unused plane
 * Returns:	0 on success, error code otherwise
 */
int sde_vbif_halt_plane_xin(struct sde_kms *sde_kms, u32 xin_id,
	       u32 clk_ctrl);

/**
 * sde_vbif_halt_xin_mask - halts/unhalts all the xin clients present in
 * the mask.
 * @sde_kms:	SDE handler
 * @xin_id_mask: Mask of all the xin-ids to be halted/unhalted
 * halt:	boolen to indicate halt/unhalt
 */
int sde_vbif_halt_xin_mask(struct sde_kms *sde_kms, u32 xin_id_mask, bool halt);

#ifdef CONFIG_DEBUG_FS
int sde_debugfs_vbif_init(struct sde_kms *sde_kms, struct dentry *debugfs_root);
void sde_debugfs_vbif_destroy(struct sde_kms *sde_kms);
#else
static inline int sde_debugfs_vbif_init(struct sde_kms *sde_kms,
		struct dentry *debugfs_root)
{
	return 0;
}
static inline void sde_debugfs_vbif_destroy(struct sde_kms *sde_kms)
{
}
#endif
#endif /* __SDE_VBIF_H__ */
