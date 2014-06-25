/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef MDSS_H
#define MDSS_H

#include <linux/msm_ion.h>
#include <linux/msm_mdp.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/irqreturn.h>

#include <linux/msm_iommu_domains.h>

#include "mdss_panel.h"

#define MAX_DRV_SUP_MMB_BLKS	44

#define MDSS_PINCTRL_STATE_DEFAULT "mdss_default"
#define MDSS_PINCTRL_STATE_SLEEP  "mdss_sleep"

enum mdss_mdp_clk_type {
	MDSS_CLK_AHB,
	MDSS_CLK_AXI,
	MDSS_CLK_MDP_SRC,
	MDSS_CLK_MDP_CORE,
	MDSS_CLK_MDP_LUT,
	MDSS_CLK_MDP_VSYNC,
	MDSS_MAX_CLK
};

enum mdss_iommu_domain_type {
	MDSS_IOMMU_DOMAIN_SECURE,
	MDSS_IOMMU_DOMAIN_UNSECURE,
	MDSS_IOMMU_MAX_DOMAIN
};

struct mdss_iommu_map_type {
	char *client_name;
	char *ctx_name;
	struct device *ctx;
	struct msm_iova_partition partitions[1];
	int npartitions;
	int domain_idx;
};

struct mdss_hw_settings {
	char __iomem *reg;
	u32 val;
};

struct mdss_debug_inf {
	void *debug_data;
	int (*debug_dump_stats)(void *data, char *buf, int len);
	void (*debug_enable_clock)(int on);
};

struct mdss_fudge_factor {
	u32 numer;
	u32 denom;
};

struct mdss_perf_tune {
	unsigned long min_mdp_clk;
	u64 min_bus_vote;
};

#define MDSS_IRQ_SUSPEND	-1
#define MDSS_IRQ_RESUME		1
#define MDSS_IRQ_REQ		0

struct mdss_intr {
	/* requested intr */
	u32 req;
	/* currently enabled intr */
	u32 curr;
	int state;
	spinlock_t lock;
};

struct mdss_prefill_data {
	u32 ot_bytes;
	u32 y_buf_bytes;
	u32 y_scaler_lines_bilinear;
	u32 y_scaler_lines_caf;
	u32 post_scaler_pixels;
	u32 pp_pixels;
	u32 fbc_lines;
};

enum mdss_hw_index {
	MDSS_HW_MDP,
	MDSS_HW_DSI0,
	MDSS_HW_DSI1,
	MDSS_HW_HDMI,
	MDSS_HW_EDP,
	MDSS_MAX_HW_BLK
};

struct mdss_data_type {
	u32 mdp_rev;
	struct clk *mdp_clk[MDSS_MAX_CLK];
	struct regulator *fs;
	struct regulator *vdd_cx;
	bool batfet_required;
	struct regulator *batfet;
	u32 max_mdp_clk_rate;

	struct platform_device *pdev;
	char __iomem *mdss_base;
	size_t mdp_reg_size;
	char __iomem *vbif_base;
	char __iomem *mdp_base;

	struct mutex reg_lock;

	u32 irq;
	u32 irq_mask;
	u32 irq_ena;
	u32 irq_buzy;
	u32 has_bwc;
	u32 has_decimation;
	bool has_fixed_qos_arbiter_enabled;
	bool has_panic_ctrl;
	u32 wfd_mode;
	u32 has_no_lut_read;
	u8 has_wb_ad;
	u8 has_non_scalar_rgb;
	bool has_src_split;
	bool idle_pc_enabled;

	u32 rotator_ot_limit;
	u32 mdp_irq_mask;
	u32 mdp_hist_irq_mask;

	int suspend_fs_ena;
	u8 clk_ena;
	u8 fs_ena;
	u8 vsync_ena;

	u32 res_init;

	u32 highest_bank_bit;
	u32 smp_mb_cnt;
	u32 smp_mb_size;
	u32 smp_mb_per_pipe;

	u32 rot_block_size;

	u32 axi_port_cnt;
	u32 curr_bw_uc_idx;
	u32 bus_hdl;
	struct msm_bus_scale_pdata *bus_scale_table;
	u32 max_bw_low;
	u32 max_bw_high;
	u32 max_bw_per_pipe;
	u32 *vbif_rt_qos;
	u32 *vbif_nrt_qos;
	u32 npriority_lvl;

	u32 reg_bus_hdl;

	struct mdss_fudge_factor ab_factor;
	struct mdss_fudge_factor ib_factor;
	struct mdss_fudge_factor ib_factor_overlap;
	struct mdss_fudge_factor clk_factor;

	u32 *clock_levels;
	u32 nclk_lvl;

	struct mdss_hw_settings *hw_settings;

	struct mdss_mdp_pipe *vig_pipes;
	struct mdss_mdp_pipe *rgb_pipes;
	struct mdss_mdp_pipe *dma_pipes;
	u32 nvig_pipes;
	u32 nrgb_pipes;
	u32 ndma_pipes;
	u32 max_target_zorder;

	DECLARE_BITMAP(mmb_alloc_map, MAX_DRV_SUP_MMB_BLKS);

	struct mdss_mdp_mixer *mixer_intf;
	struct mdss_mdp_mixer *mixer_wb;
	u32 nmixers_intf;
	u32 nmixers_wb;
	u32 max_mixer_width;

	struct mdss_mdp_ctl *ctl_off;
	u32 nctl;

	struct mdss_mdp_dp_intf *dp_off;
	u32 ndp;
	void *video_intf;
	u32 nintf;

	struct mdss_mdp_ad *ad_off;
	struct mdss_ad_info *ad_cfgs;
	u32 nad_cfgs;
	u32 nmax_concurrent_ad_hw;
	struct workqueue_struct *ad_calc_wq;

	struct mdss_intr hist_intr;

	struct ion_client *iclient;
	int iommu_attached;
	struct mdss_iommu_map_type *iommu_map;

	struct mdss_debug_inf debug_inf;
	bool mixer_switched;
	struct mdss_panel_cfg pan_cfg;
	struct mdss_prefill_data prefill_data;

	int handoff_pending;
	bool idle_pc;
	struct mdss_perf_tune perf_tune;
	bool traffic_shaper_en;
	int iommu_ref_cnt;

	u64 ab[MDSS_MAX_HW_BLK];
	u64 ib[MDSS_MAX_HW_BLK];
};
extern struct mdss_data_type *mdss_res;

struct mdss_hw {
	u32 hw_ndx;
	void *ptr;
	irqreturn_t (*irq_handler)(int irq, void *ptr);
};

int mdss_register_irq(struct mdss_hw *hw);
void mdss_enable_irq(struct mdss_hw *hw);
void mdss_disable_irq(struct mdss_hw *hw);
void mdss_disable_irq_nosync(struct mdss_hw *hw);
void mdss_bus_bandwidth_ctrl(int enable);
int mdss_iommu_ctrl(int enable);
int mdss_bus_scale_set_quota(int client, u64 ab_quota, u64 ib_quota);

static inline struct ion_client *mdss_get_ionclient(void)
{
	if (!mdss_res)
		return NULL;
	return mdss_res->iclient;
}

static inline int is_mdss_iommu_attached(void)
{
	if (!mdss_res)
		return false;
	return mdss_res->iommu_attached;
}

static inline int mdss_get_iommu_domain(u32 type)
{
	if (type >= MDSS_IOMMU_MAX_DOMAIN)
		return -EINVAL;

	if (!mdss_res)
		return -ENODEV;

	return mdss_res->iommu_map[type].domain_idx;
}
#endif /* MDSS_H */
