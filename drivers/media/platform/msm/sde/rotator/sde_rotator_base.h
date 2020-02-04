/* Copyright (c) 2015-2019, The Linux Foundation. All rights reserved.
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
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#include "sde_rotator_hwio.h"
#include "sde_rotator_io_util.h"
#include "sde_rotator_smmu.h"
#include "sde_rotator_formats.h"
#include <linux/pm_qos.h>

/* HW Revisions for different targets */
#define SDE_GET_MAJOR_REV(rev)	((rev) >> 28)
#define SDE_GET_MAJOR_MINOR(rev)	((rev) >> 16)

#define IS_SDE_MAJOR_SAME(rev1, rev2)   \
	(SDE_GET_MAJOR_REV((rev1)) == SDE_GET_MAJOR_REV((rev2)))

#define IS_SDE_MAJOR_MINOR_SAME(rev1, rev2) \
	(SDE_GET_MAJOR_MINOR(rev1) == SDE_GET_MAJOR_MINOR(rev2))

#define SDE_MDP_REV(major, minor, step) \
	((((major) & 0x000F) << 28) | \
	 (((minor) & 0x0FFF) << 16) | \
	  ((step)  & 0xFFFF))

#define SDE_MDP_HW_REV_107	SDE_MDP_REV(1, 0, 7)	/* 8996 v1.0 */
#define SDE_MDP_HW_REV_300	SDE_MDP_REV(3, 0, 0)	/* 8998 v1.0 */
#define SDE_MDP_HW_REV_301	SDE_MDP_REV(3, 0, 1)	/* 8998 v1.1 */
#define SDE_MDP_HW_REV_320	SDE_MDP_REV(3, 2, 0)    /* sdm660 */
#define SDE_MDP_HW_REV_400	SDE_MDP_REV(4, 0, 0)	/* sdm845 v1.0 */
#define SDE_MDP_HW_REV_410	SDE_MDP_REV(4, 1, 0)	/* sdm670 v1.0 */
#define SDE_MDP_HW_REV_500	SDE_MDP_REV(5, 0, 0)	/* sm8150 v1.0 */
#define SDE_MDP_HW_REV_520	SDE_MDP_REV(5, 2, 0)	/* sdmmagpie v1.0 */
#define SDE_MDP_HW_REV_530	SDE_MDP_REV(5, 3, 0)	/* sm6150 v1.0 */
#define SDE_MDP_HW_REV_540	SDE_MDP_REV(5, 4, 0)	/* sdmtrinket v1.0 */
#define SDE_MDP_HW_REV_620	SDE_MDP_REV(6, 2, 0)	/* atoll */

#define SDE_MDP_VBIF_4_LEVEL_REMAPPER	4
#define SDE_MDP_VBIF_8_LEVEL_REMAPPER	8

/* XIN mapping */
#define XIN_SSPP	0
#define XIN_WRITEBACK	1
#define MAX_XIN		2

struct sde_mult_factor {
	uint32_t numer;
	uint32_t denom;
};

struct sde_mdp_set_ot_params {
	u32 xin_id;
	u32 num;
	u32 width;
	u32 height;
	u32 fps;
	u32 fmt;
	u32 reg_off_vbif_lim_conf;
	u32 reg_off_mdp_clk_ctrl;
	u32 bit_off_mdp_clk_ctrl;
	char __iomem *rotsts_base;
	u32 rotsts_busy_mask;
};

/*
 * struct sde_mdp_vbif_halt_params: parameters for issue halt request to vbif
 * @xin_id: xin port number of vbif
 * @reg_off_mdp_clk_ctrl: reg offset for vbif clock control
 * @bit_off_mdp_clk_ctrl: bit offset for vbif clock control
 * @xin_timeout: bit position indicates timeout on corresponding xin id
 */
struct sde_mdp_vbif_halt_params {
	u32 xin_id;
	u32 reg_off_mdp_clk_ctrl;
	u32 bit_off_mdp_clk_ctrl;
	u32 xin_timeout;
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
	SDE_QOS_LUT,
	SDE_QOS_DANGER_LUT,
	SDE_QOS_SAFE_LUT,
	SDE_QOS_MAX,
};

enum sde_inline_qos_settings {
	SDE_INLINE_QOS_LUT,
	SDE_INLINE_QOS_DANGER_LUT,
	SDE_INLINE_QOS_SAFE_LUT,
	SDE_INLINE_QOS_MAX,
};

/**
 * enum sde_rot_type: SDE rotator HW version
 * @SDE_ROT_TYPE_V1_0: V1.0 HW version
 * @SDE_ROT_TYPE_V1_1: V1.1 HW version
 */
enum sde_rot_type {
	SDE_ROT_TYPE_V1_0 = 0x10000000,
	SDE_ROT_TYPE_V1_1 = 0x10010000,
	SDE_ROT_TYPE_MAX,
};

/**
 * enum sde_caps_settings: SDE rotator capability definition
 * @SDE_CAPS_R1_WB: MDSS V1.x WB block
 * @SDE_CAPS_R3_WB: MDSS V3.x WB block
 * @SDE_CAPS_R3_1P5_DOWNSCALE: 1.5x downscale rotator support
 * @SDE_CAPS_SBUF_1: stream buffer support for inline rotation
 * @SDE_CAPS_UBWC_2: universal bandwidth compression version 2
 * @SDE_CAPS_PARTIALWR: partial write override
 * @SDE_CAPS_HW_TIMESTAMP: rotator has hw timestamp support
 * @SDE_CAPS_UBWC_3: universal bandwidth compression version 3
 */
enum sde_caps_settings {
	SDE_CAPS_R1_WB,
	SDE_CAPS_R3_WB,
	SDE_CAPS_R3_1P5_DOWNSCALE,
	SDE_CAPS_SEC_ATTACH_DETACH_SMMU,
	SDE_CAPS_SBUF_1,
	SDE_CAPS_UBWC_2,
	SDE_CAPS_PARTIALWR,
	SDE_CAPS_HW_TIMESTAMP,
	SDE_CAPS_UBWC_3,
	SDE_CAPS_MAX,
};

enum sde_bus_clients {
	SDE_ROT_RT,
	SDE_ROT_NRT,
	SDE_MAX_BUS_CLIENTS
};

enum sde_rot_op {
	SDE_ROT_RD,
	SDE_ROT_WR,
	SDE_ROT_OP_MAX
};

enum sde_rot_regdump_access {
	SDE_ROT_REGDUMP_READ,
	SDE_ROT_REGDUMP_WRITE,
	SDE_ROT_REGDUMP_VBIF,
	SDE_ROT_REGDUMP_MAX
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
	int domain;
	u32 sid;
};

/*
 * struct sde_rot_debug_bus: rotator debugbus header structure
 * @wr_addr: write address for debugbus controller
 * @block_id: rotator debugbus block id
 * @test_id: rotator debugbus test id
 */
struct sde_rot_debug_bus {
	u32 wr_addr;
	u32 block_id;
	u32 test_id;
};

struct sde_rot_vbif_debug_bus {
	u32 disable_bus_addr;
	u32 block_bus_addr;
	u32 bit_offset;
	u32 block_cnt;
	u32 test_pnt_cnt;
};

struct sde_rot_regdump {
	char *name;
	u32 offset;
	u32 len;
	enum sde_rot_regdump_access access;
	u32 value;
};

struct sde_rot_lut_cfg {
	u32 creq_lut_0;
	u32 creq_lut_1;
	u32 danger_lut;
	u32 safe_lut;
};

struct sde_rot_data_type {
	u32 mdss_version;

	struct platform_device *pdev;
	struct platform_device *parent_pdev;
	struct sde_io_data sde_io;
	struct sde_io_data vbif_nrt_io;
	char __iomem *mdp_base;

	struct sde_smmu_client sde_smmu[SDE_IOMMU_MAX_DOMAIN];

	/* bitmap to track qos applicable settings */
	DECLARE_BITMAP(sde_qos_map, SDE_QOS_MAX);
	DECLARE_BITMAP(sde_inline_qos_map, SDE_QOS_MAX);

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

	u32 vbif_xin_id[MAX_XIN];

	struct pm_qos_request pm_qos_rot_cpu_req;
	u32 rot_pm_qos_cpu_count;
	u32 rot_pm_qos_cpu_mask;
	u32 rot_pm_qos_cpu_dma_latency;

	u32 vbif_memtype_count;
	u32 *vbif_memtype;

	int iommu_attached;
	int iommu_ref_cnt;

	struct sde_rot_vbif_debug_bus *nrt_vbif_dbg_bus;
	u32 nrt_vbif_dbg_bus_size;
	struct sde_rot_debug_bus *rot_dbg_bus;
	u32 rot_dbg_bus_size;

	struct sde_rot_regdump *regdump;
	u32 regdump_size;

	void *sde_rot_hw;
	int sec_cam_en;

	u32 enable_cdp[SDE_ROT_OP_MAX];

	struct sde_rot_lut_cfg lut_cfg[SDE_ROT_OP_MAX];
	struct sde_rot_lut_cfg inline_lut_cfg[SDE_ROT_OP_MAX];

	bool clk_always_on;
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

u32 sde_mdp_get_ot_limit(u32 width, u32 height, u32 pixfmt, u32 fps, u32 is_rd);

void sde_mdp_set_ot_limit(struct sde_mdp_set_ot_params *params);

void vbif_lock(struct platform_device *parent_pdev);
void vbif_unlock(struct platform_device *parent_pdev);

void sde_mdp_halt_vbif_xin(struct sde_mdp_vbif_halt_params *params);

int sde_mdp_init_vbif(void);

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
