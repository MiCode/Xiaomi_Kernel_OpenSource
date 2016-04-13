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

#ifndef __SDE_ROTATOR_BASE_H__
#define __SDE_ROTATOR_BASE_H__

#include <linux/types.h>
#include <linux/file.h>
#include <linux/kref.h>
#include <linux/kernel.h>
#include <linux/regulator/consumer.h>

#include "sde_rotator_hwio.h"
#include "sde_rotator_io_util.h"
#include "sde_rotator_smmu.h"
#include "sde_rotator_formats.h"

struct sde_mult_factor {
	uint32_t numer;
	uint32_t denom;
};

struct sde_mdp_set_ot_params {
	u32 xin_id;
	u32 num;
	u32 width;
	u32 height;
	bool is_rot;
	bool is_wb;
	bool is_yuv;
	u32 reg_off_vbif_lim_conf;
	u32 reg_off_mdp_clk_ctrl;
	u32 bit_off_mdp_clk_ctrl;
};

enum sde_bus_vote_type {
	VOTE_INDEX_DISABLE,
	VOTE_INDEX_19_MHZ,
	VOTE_INDEX_40_MHZ,
	VOTE_INDEX_80_MHZ,
	VOTE_INDEX_MAX,
};

#define MAX_CLIENT_NAME_LEN 64

enum sde_qos_settings {
	SDE_QOS_PER_PIPE_IB,
	SDE_QOS_OVERHEAD_FACTOR,
	SDE_QOS_CDP,
	SDE_QOS_OTLIM,
	SDE_QOS_PER_PIPE_LUT,
	SDE_QOS_SIMPLIFIED_PREFILL,
	SDE_QOS_VBLANK_PANIC_CTRL,
	SDE_QOS_MAX,
};

enum sde_caps_settings {
	SDE_CAPS_R1_WB,
	SDE_CAPS_R3_WB,
	SDE_CAPS_MAX,
};

enum sde_bus_clients {
	SDE_ROT_RT,
	SDE_ROT_NRT,
	SDE_MAX_BUS_CLIENTS
};

struct reg_bus_client {
	char name[MAX_CLIENT_NAME_LEN];
	short usecase_ndx;
	u32 id;
	struct list_head list;
};

struct sde_smmu_client {
	struct device *dev;
	struct dma_iommu_mapping *mmu_mapping;
	struct sde_module_power mp;
	struct reg_bus_client *reg_bus_clt;
	bool domain_attached;
};

struct sde_rot_data_type {
	u32 mdss_version;

	struct platform_device *pdev;
	struct sde_io_data sde_io;
	struct sde_io_data vbif_nrt_io;
	char __iomem *mdp_base;

	struct sde_smmu_client sde_smmu[SDE_IOMMU_MAX_DOMAIN];

	/* bitmap to track qos applicable settings */
	DECLARE_BITMAP(sde_qos_map, SDE_QOS_MAX);

	/* bitmap to track capability settings */
	DECLARE_BITMAP(sde_caps_map, SDE_CAPS_MAX);

	u32 default_ot_rd_limit;
	u32 default_ot_wr_limit;
	u32 highest_bank_bit;
	u32 rot_block_size;

	/* register bus (AHB) */
	u32 reg_bus_hdl;
	u32 reg_bus_usecase_ndx;
	struct list_head reg_bus_clist;
	struct mutex reg_bus_lock;

	u32 *vbif_rt_qos;
	u32 *vbif_nrt_qos;
	u32 npriority_lvl;

	int iommu_attached;
	int iommu_ref_cnt;
};

int sde_rotator_base_init(struct sde_rot_data_type **pmdata,
		struct platform_device *pdev,
		const void *drvdata);

void sde_rotator_base_destroy(struct sde_rot_data_type *data);

struct sde_rot_data_type *sde_rot_get_mdata(void);

struct reg_bus_client *sde_reg_bus_vote_client_create(char *client_name);

void sde_reg_bus_vote_client_destroy(struct reg_bus_client *client);

int sde_update_reg_bus_vote(struct reg_bus_client *bus_client, u32 usecase_ndx);

u32 sde_apply_comp_ratio_factor(u32 quota,
	struct sde_mdp_format_params *fmt,
	struct sde_mult_factor *factor);

void sde_mdp_set_ot_limit(struct sde_mdp_set_ot_params *params);

#define SDE_VBIF_WRITE(mdata, offset, value) \
		(sde_reg_w(&mdata->vbif_nrt_io, offset, value, 0))
#define SDE_VBIF_READ(mdata, offset) \
		(sde_reg_r(&mdata->vbif_nrt_io, offset, 0))
#define SDE_REG_WRITE(mdata, offset, value) \
		sde_reg_w(&mdata->sde_io, offset, value, 0)
#define SDE_REG_READ(mdata, offset) \
		sde_reg_r(&mdata->sde_io, offset, 0)

#define ATRACE_END(name) trace_rot_mark_write(current->tgid, name, 0)
#define ATRACE_BEGIN(name) trace_rot_mark_write(current->tgid, name, 1)
#define ATRACE_INT(name, value) \
	trace_rot_trace_counter(current->tgid, name, value)

#endif /* __SDE_ROTATOR_BASE__ */
