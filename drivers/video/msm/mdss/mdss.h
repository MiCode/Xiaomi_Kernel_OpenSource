/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
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
#include <linux/mdss_io_util.h>
#include <linux/msm_iommu_domains.h>

#include "mdss_panel.h"

#define MAX_DRV_SUP_MMB_BLKS	44
#define MAX_DRV_SUP_PIPES 10

#define MDSS_PINCTRL_STATE_DEFAULT "mdss_default"
#define MDSS_PINCTRL_STATE_SLEEP  "mdss_sleep"

enum mdss_mdp_clk_type {
	MDSS_CLK_AHB,
	MDSS_CLK_AXI,
	MDSS_CLK_MDP_SRC,
	MDSS_CLK_MDP_CORE,
	MDSS_CLK_MDP_LUT,
	MDSS_CLK_MDP_VSYNC,
	MDSS_CLK_MDP_TBU,
	MDSS_CLK_MDP_TBU_RT,
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

struct mdss_max_bw_settings {
	u32 mdss_max_bw_mode;
	u32 mdss_max_bw_val;
};

struct mdss_debug_inf {
	void *debug_data;
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

struct mdss_mdp_ppb {
	u32 ctl_off;
	u32 cfg_off;
};

struct mdss_mdp_dsc {
	u32 num;
	char __iomem *base;
};

enum mdss_hw_index {
	MDSS_HW_MDP,
	MDSS_HW_DSI0 = 1,
	MDSS_HW_DSI1,
	MDSS_HW_HDMI,
	MDSS_HW_EDP,
	MDSS_MAX_HW_BLK
};

enum mdss_bus_clients {
	MDSS_MDP_RT,
	MDSS_DSI_RT,
	MDSS_MDP_NRT,
	MDSS_IOMMU_RT,
	MDSS_MAX_BUS_CLIENTS
};

enum mdss_hw_quirk {
	MDSS_QUIRK_BWCPANIC,
	MDSS_QUIRK_DOWNSCALE_HANG,
	MDSS_QUIRK_SVS_PLUS_VOTING,
	MDSS_QUIRK_MAX,
};

struct mdss_data_type {
	u32 mdp_rev;
	struct clk *mdp_clk[MDSS_MAX_CLK];
	struct regulator *fs;
	struct regulator *vdd_cx;
	bool batfet_required;
	struct regulator *batfet;
	bool en_svs_high;
	u32 max_mdp_clk_rate;
	struct mdss_util_intf *mdss_util;

	struct platform_device *pdev;
	struct dss_io_data mdss_io;
	struct dss_io_data vbif_io;
	struct dss_io_data vbif_nrt_io;
	char __iomem *mdp_base;

	/* Used to store if vote to enable svs plus has been sent or not */
	u32 svs_plus_vote;
	/* Min rate from where SVS plus vote is needed */
	u32 svs_plus_min;
	/* Max rate till where SVS plus vote is needed */
	u32 svs_plus_max;

	struct mutex reg_lock;

	/* bitmap to track pipes that have BWC enabled */
	DECLARE_BITMAP(bwc_enable_map, MAX_DRV_SUP_PIPES);
	/* bitmap to track hw workarounds */
	DECLARE_BITMAP(mdss_quirk_map, MDSS_QUIRK_MAX);
	/* bitmap to track total mmbs in use */
	DECLARE_BITMAP(mmb_alloc_map, MAX_DRV_SUP_MMB_BLKS);

	u32 has_bwc;
	u32 default_panic_lut0;
	u32 default_panic_lut1;
	u32 default_robust_lut;

	u32 has_decimation;
	bool has_fixed_qos_arbiter_enabled;
	bool has_panic_ctrl;
	u32 wfd_mode;
	u32 has_no_lut_read;
	atomic_t sd_client_count;
	u8 has_wb_ad;
	u8 has_non_scalar_rgb;
	bool has_src_split;
	bool idle_pc_enabled;
	bool needs_iommu_bw_vote;
	bool has_pingpong_split;
	bool has_pixel_ram;
	bool needs_hist_vote;
	bool has_10_bit_pa;

	u32 default_ot_rd_limit;
	u32 default_ot_wr_limit;

	u32 mdp_irq_mask;
	u32 mdp_hist_irq_mask;

	int suspend_fs_ena;
	u8 clk_ena;
	u8 fs_ena;
	u8 vsync_ena;

	struct notifier_block gdsc_cb;
	struct notifier_block tlb_timeout_cb;

	u32 res_init;

	u32 highest_bank_bit;
	u32 smp_mb_cnt;
	u32 smp_mb_size;
	u32 smp_mb_per_pipe;

	u32 rot_block_size;

	u32 axi_port_cnt;
	u32 nrt_axi_port_cnt;
	u32 bus_channels;
	u32 curr_bw_uc_idx;
	u32 bus_hdl;
	struct msm_bus_scale_pdata *bus_scale_table;
	u32 max_bw_low;
	u32 max_bw_high;
	u32 max_bw_per_pipe;
	u32 *vbif_rt_qos;
	u32 *vbif_nrt_qos;
	u32 npriority_lvl;
	u32 bus_bw_cnt;
	struct mutex bus_bw_lock;

	u32 reg_bus_hdl;

	struct mdss_fudge_factor ab_factor;
	struct mdss_fudge_factor ib_factor;
	struct mdss_fudge_factor ib_factor_overlap;
	struct mdss_fudge_factor ib_factor_cmd;
	struct mdss_fudge_factor clk_factor;

	u32 disable_prefill;
	u32 *clock_levels;
	u32 nclk_lvl;

	u32 enable_bw_release;
	u32 enable_rotator_bw_release;
	u32 serialize_wait4pp;

	struct mdss_hw_settings *hw_settings;

	struct mdss_mdp_pipe *vig_pipes;
	struct mdss_mdp_pipe *rgb_pipes;
	struct mdss_mdp_pipe *dma_pipes;
	struct mdss_mdp_pipe *cursor_pipes;
	u32 nvig_pipes;
	u32 nrgb_pipes;
	u32 ndma_pipes;
	u32 max_target_zorder;
	u8  ncursor_pipes;
	u32 max_cursor_size;

	u32 nppb;
	struct mdss_mdp_ppb *ppb;
	char __iomem *slave_pingpong_base;

	struct mdss_mdp_mixer *mixer_intf;
	struct mdss_mdp_mixer *mixer_wb;
	u32 nmixers_intf;
	u32 nmixers_wb;
	u32 max_mixer_width;
	u32 max_pipe_width;

	struct mdss_mdp_ctl *ctl_off;
	u32 nctl;
	u32 nwb;
	u32 ndspp;

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

	struct debug_bus *dbg_bus;
	u32 dbg_bus_size;
	struct mdss_debug_inf debug_inf;
	bool mixer_switched;
	struct mdss_panel_cfg pan_cfg;
	struct mdss_prefill_data prefill_data;
	u32 min_prefill_lines; /* this changes within different chipsets */
	u32 props;

	int handoff_pending;
	bool idle_pc;
	bool allow_cx_vddmin;
	bool vdd_cx_en;
	struct mdss_perf_tune perf_tune;
	bool traffic_shaper_en;
	int iommu_ref_cnt;
	u32 latency_buff_per;
	atomic_t active_intf_cnt;
	bool has_rot_dwnscale;
	bool regulator_notif_register;

	u64 ab[MDSS_MAX_BUS_CLIENTS];
	u64 ib[MDSS_MAX_BUS_CLIENTS];

	struct mdss_max_bw_settings *max_bw_settings;
	u32 bw_mode_bitmap;
	u32 max_bw_settings_cnt;

	struct mdss_max_bw_settings *max_per_pipe_bw_settings;
	u32 mdss_per_pipe_bw_cnt;
	u32 min_bw_per_pipe;

	u32 bcolor0;
	u32 bcolor1;
	u32 bcolor2;
	struct mdss_mdp_dsc *dsc_off;
	u32 ndsc;

};
extern struct mdss_data_type *mdss_res;

struct irq_info {
	u32 irq;
	u32 irq_mask;
	u32 irq_ena;
	u32 irq_buzy;
};

struct mdss_hw {
	u32 hw_ndx;
	void *ptr;
	struct irq_info *irq_info;
	irqreturn_t (*irq_handler)(int irq, void *ptr);
};

struct irq_info *mdss_intr_line(void);
void mdss_bus_bandwidth_ctrl(int enable);
int mdss_iommu_ctrl(int enable);
int mdss_bus_scale_set_quota(int client, u64 ab_quota, u64 ib_quota);

struct mdss_util_intf {
	bool mdp_probe_done;
	int (*register_irq)(struct mdss_hw *hw);
	void (*enable_irq)(struct mdss_hw *hw);
	void (*disable_irq)(struct mdss_hw *hw);
	void (*disable_irq_nosync)(struct mdss_hw *hw);
	int (*irq_dispatch)(u32 hw_ndx, int irq, void *ptr);
	int (*get_iommu_domain)(u32 type);
	int (*iommu_attached)(void);
	int (*iommu_ctrl)(int enable);
	void (*iommu_lock)(void);
	void (*iommu_unlock)(void);
	void (*bus_bandwidth_ctrl)(int enable);
	int (*bus_scale_set_quota)(int client, u64 ab_quota, u64 ib_quota);
	int (*panel_intf_status)(u32 disp_num, u32 intf_type);
	struct mdss_panel_cfg* (*panel_intf_type)(int intf_val);
};

struct mdss_util_intf *mdss_get_util_intf(void);

static inline struct ion_client *mdss_get_ionclient(void)
{
	if (!mdss_res)
		return NULL;
	return mdss_res->iclient;
}

static inline int mdss_get_iommu_domain(u32 type)
{
	if (type >= MDSS_IOMMU_MAX_DOMAIN)
		return -EINVAL;

	if (!mdss_res)
		return -ENODEV;

	return mdss_res->iommu_map[type].domain_idx;
}

static inline int mdss_get_sd_client_cnt(void)
{
	if (!mdss_res)
		return 0;
	else
		return atomic_read(&mdss_res->sd_client_count);
}

static inline void mdss_set_quirk(struct mdss_data_type *mdata,
	enum mdss_hw_quirk bit)
{
	set_bit(bit, mdata->mdss_quirk_map);
}

static inline bool mdss_has_quirk(struct mdss_data_type *mdata,
	enum mdss_hw_quirk bit)
{
	return test_bit(bit, mdata->mdss_quirk_map);
}

#define MDSS_VBIF_WRITE(mdata, offset, value, nrt_vbif) \
		(nrt_vbif ? dss_reg_w(&mdata->vbif_nrt_io, offset, value, 0) :\
		dss_reg_w(&mdata->vbif_io, offset, value, 0))
#define MDSS_VBIF_READ(mdata, offset, nrt_vbif) \
		(nrt_vbif ? dss_reg_r(&mdata->vbif_nrt_io, offset, 0) :\
		dss_reg_r(&mdata->vbif_io, offset, 0))
#define MDSS_REG_WRITE(mdata, offset, value) \
		dss_reg_w(&mdata->mdss_io, offset, value, 0)
#define MDSS_REG_READ(mdata, offset) \
		dss_reg_r(&mdata->mdss_io, offset, 0)

#endif /* MDSS_H */
