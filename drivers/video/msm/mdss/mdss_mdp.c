/*
 * MDSS MDP Interface (used by framebuffer core)
 *
 * Copyright (c) 2007-2014, The Linux Foundation. All rights reserved.
 * Copyright (C) 2007 Google Incorporated
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iommu.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/memory_alloc.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/uaccess.h>

#include <mach/board.h>
#include <mach/clk.h>
#include <mach/hardware.h>
#include <mach/msm_bus.h>
#include <mach/msm_bus_board.h>
#include <mach/iommu.h>
#include <mach/iommu_domains.h>
#include <mach/memory.h>
#include <mach/msm_memtypes.h>
#include <mach/rpm-regulator-smd.h>

#include "mdss.h"
#include "mdss_fb.h"
#include "mdss_mdp.h"
#include "mdss_panel.h"
#include "mdss_debug.h"

struct mdss_data_type *mdss_res;

static int mdss_fb_mem_get_iommu_domain(void)
{
	return mdss_get_iommu_domain(MDSS_IOMMU_DOMAIN_UNSECURE);
}

struct msm_mdp_interface mdp5 = {
	.init_fnc = mdss_mdp_overlay_init,
	.fb_mem_get_iommu_domain = mdss_fb_mem_get_iommu_domain,
	.panel_register_done = mdss_panel_register_done,
	.fb_stride = mdss_mdp_fb_stride,
};

#define DEFAULT_TOTAL_RGB_PIPES 3
#define DEFAULT_TOTAL_VIG_PIPES 3
#define DEFAULT_TOTAL_DMA_PIPES 2

#define IB_QUOTA 800000000
#define AB_QUOTA 800000000

static DEFINE_SPINLOCK(mdp_lock);
static DEFINE_MUTEX(mdp_clk_lock);
static DEFINE_MUTEX(bus_bw_lock);
static DEFINE_MUTEX(mdp_iommu_lock);

static struct mdss_panel_intf pan_types[] = {
	{"dsi", MDSS_PANEL_INTF_DSI},
	{"edp", MDSS_PANEL_INTF_EDP},
	{"hdmi", MDSS_PANEL_INTF_HDMI},
};

struct mdss_iommu_map_type mdss_iommu_map[MDSS_IOMMU_MAX_DOMAIN] = {
	[MDSS_IOMMU_DOMAIN_UNSECURE] = {
		.client_name = "mdp_ns",
		.ctx_name = "mdp_0",
		.partitions = {
			{
				.start = SZ_128K,
				.size = SZ_1G - SZ_128K,
			},
		},
		.npartitions = 1,
	},
	[MDSS_IOMMU_DOMAIN_SECURE] = {
		.client_name = "mdp_secure",
		.ctx_name = "mdp_1",
		.partitions = {
			{
				.start = SZ_1G,
				.size = SZ_1G,
			},
		},
		.npartitions = 1,
	},
};

struct mdss_hw mdss_mdp_hw = {
	.hw_ndx = MDSS_HW_MDP,
	.ptr = NULL,
	.irq_handler = mdss_mdp_isr,
};

static DEFINE_SPINLOCK(mdss_lock);
struct mdss_hw *mdss_irq_handlers[MDSS_MAX_HW_BLK];

static void mdss_mdp_footswitch_ctrl(struct mdss_data_type *mdata, int on);
static int mdss_mdp_parse_dt(struct platform_device *pdev);
static int mdss_mdp_parse_dt_pipe(struct platform_device *pdev);
static int mdss_mdp_parse_dt_mixer(struct platform_device *pdev);
static int mdss_mdp_parse_dt_ctl(struct platform_device *pdev);
static int mdss_mdp_parse_dt_video_intf(struct platform_device *pdev);
static int mdss_mdp_parse_dt_handler(struct platform_device *pdev,
				      char *prop_name, u32 *offsets, int len);
static int mdss_mdp_parse_dt_prop_len(struct platform_device *pdev,
				       char *prop_name);
static int mdss_mdp_parse_dt_smp(struct platform_device *pdev);
static int mdss_mdp_parse_dt_misc(struct platform_device *pdev);
static int mdss_mdp_parse_dt_ad_cfg(struct platform_device *pdev);
static int mdss_mdp_parse_dt_bus_scale(struct platform_device *pdev);

u32 mdss_mdp_fb_stride(u32 fb_index, u32 xres, int bpp)
{
	/* The adreno GPU hardware requires that the pitch be aligned to
	   32 pixels for color buffers, so for the cases where the GPU
	   is writing directly to fb0, the framebuffer pitch
	   also needs to be 32 pixel aligned */

	if (fb_index == 0)
		return ALIGN(xres, 32) * bpp;
	else
		return xres * bpp;
}

static inline int mdss_irq_dispatch(u32 hw_ndx, int irq, void *ptr)
{
	struct mdss_hw *hw;
	int rc = -ENODEV;

	spin_lock(&mdss_lock);
	hw = mdss_irq_handlers[hw_ndx];
	spin_unlock(&mdss_lock);

	if (hw)
		rc = hw->irq_handler(irq, hw->ptr);

	return rc;
}

static irqreturn_t mdss_irq_handler(int irq, void *ptr)
{
	struct mdss_data_type *mdata = ptr;
	u32 intr = MDSS_MDP_REG_READ(MDSS_REG_HW_INTR_STATUS);

	if (!mdata)
		return IRQ_NONE;

	mdata->irq_buzy = true;

	if (intr & MDSS_INTR_MDP) {
		spin_lock(&mdp_lock);
		mdss_irq_dispatch(MDSS_HW_MDP, irq, ptr);
		spin_unlock(&mdp_lock);
	}

	if (intr & MDSS_INTR_DSI0)
		mdss_irq_dispatch(MDSS_HW_DSI0, irq, ptr);

	if (intr & MDSS_INTR_DSI1)
		mdss_irq_dispatch(MDSS_HW_DSI1, irq, ptr);

	if (intr & MDSS_INTR_EDP)
		mdss_irq_dispatch(MDSS_HW_EDP, irq, ptr);

	if (intr & MDSS_INTR_HDMI)
		mdss_irq_dispatch(MDSS_HW_HDMI, irq, ptr);

	mdata->irq_buzy = false;

	return IRQ_HANDLED;
}

int mdss_register_irq(struct mdss_hw *hw)
{
	unsigned long irq_flags;
	u32 ndx_bit;

	if (!hw || hw->hw_ndx >= MDSS_MAX_HW_BLK)
		return -EINVAL;

	ndx_bit = BIT(hw->hw_ndx);

	spin_lock_irqsave(&mdss_lock, irq_flags);
	if (!mdss_irq_handlers[hw->hw_ndx])
		mdss_irq_handlers[hw->hw_ndx] = hw;
	else
		pr_err("panel %d's irq at %p is already registered\n",
			hw->hw_ndx, hw->irq_handler);
	spin_unlock_irqrestore(&mdss_lock, irq_flags);

	return 0;
} /* mdss_regsiter_irq */
EXPORT_SYMBOL(mdss_register_irq);

void mdss_enable_irq(struct mdss_hw *hw)
{
	unsigned long irq_flags;
	u32 ndx_bit;

	if (hw->hw_ndx >= MDSS_MAX_HW_BLK)
		return;

	if (!mdss_irq_handlers[hw->hw_ndx]) {
		pr_err("failed. First register the irq then enable it.\n");
		return;
	}

	ndx_bit = BIT(hw->hw_ndx);

	pr_debug("Enable HW=%d irq ena=%d mask=%x\n", hw->hw_ndx,
			mdss_res->irq_ena, mdss_res->irq_mask);

	spin_lock_irqsave(&mdss_lock, irq_flags);
	if (mdss_res->irq_mask & ndx_bit) {
		pr_debug("MDSS HW ndx=%d is already set, mask=%x\n",
				hw->hw_ndx, mdss_res->irq_mask);
	} else {
		mdss_res->irq_mask |= ndx_bit;
		if (!mdss_res->irq_ena) {
			mdss_res->irq_ena = true;
			enable_irq(mdss_res->irq);
		}
	}
	spin_unlock_irqrestore(&mdss_lock, irq_flags);
}
EXPORT_SYMBOL(mdss_enable_irq);

void mdss_disable_irq(struct mdss_hw *hw)
{
	unsigned long irq_flags;
	u32 ndx_bit;

	if (hw->hw_ndx >= MDSS_MAX_HW_BLK)
		return;

	ndx_bit = BIT(hw->hw_ndx);

	pr_debug("Disable HW=%d irq ena=%d mask=%x\n", hw->hw_ndx,
			mdss_res->irq_ena, mdss_res->irq_mask);

	spin_lock_irqsave(&mdss_lock, irq_flags);
	if (!(mdss_res->irq_mask & ndx_bit)) {
		pr_warn("MDSS HW ndx=%d is NOT set, mask=%x, hist mask=%x\n",
			hw->hw_ndx, mdss_res->mdp_irq_mask,
			mdss_res->mdp_hist_irq_mask);
	} else {
		mdss_res->irq_mask &= ~ndx_bit;
		if (mdss_res->irq_mask == 0) {
			mdss_res->irq_ena = false;
			disable_irq_nosync(mdss_res->irq);
		}
	}
	spin_unlock_irqrestore(&mdss_lock, irq_flags);
}
EXPORT_SYMBOL(mdss_disable_irq);

/* called from interrupt context */
void mdss_disable_irq_nosync(struct mdss_hw *hw)
{
	u32 ndx_bit;

	if (hw->hw_ndx >= MDSS_MAX_HW_BLK)
		return;

	ndx_bit = BIT(hw->hw_ndx);

	pr_debug("Disable HW=%d irq ena=%d mask=%x\n", hw->hw_ndx,
			mdss_res->irq_ena, mdss_res->irq_mask);

	if (!(mdss_res->irq_mask & ndx_bit)) {
		pr_warn("MDSS HW ndx=%d is NOT set, mask=%x, hist mask=%x\n",
			hw->hw_ndx, mdss_res->mdp_irq_mask,
			mdss_res->mdp_hist_irq_mask);
	} else {
		mdss_res->irq_mask &= ~ndx_bit;
		if (mdss_res->irq_mask == 0) {
			mdss_res->irq_ena = false;
			disable_irq_nosync(mdss_res->irq);
		}
	}
}
EXPORT_SYMBOL(mdss_disable_irq_nosync);

static int mdss_mdp_bus_scale_register(struct mdss_data_type *mdata)
{
	if (!mdata->bus_hdl) {
		mdata->bus_hdl =
			msm_bus_scale_register_client(mdata->bus_scale_table);
		if (IS_ERR_VALUE(mdata->bus_hdl)) {
			pr_err("bus_client register failed\n");
			return -EINVAL;
		}

		pr_debug("register bus_hdl=%x\n", mdata->bus_hdl);
	}

	return mdss_mdp_bus_scale_set_quota(AB_QUOTA, IB_QUOTA);
}

static void mdss_mdp_bus_scale_unregister(struct mdss_data_type *mdata)
{
	pr_debug("unregister bus_hdl=%x\n", mdata->bus_hdl);

	if (mdata->bus_hdl)
		msm_bus_scale_unregister_client(mdata->bus_hdl);
}

int mdss_mdp_bus_scale_set_quota(u64 ab_quota, u64 ib_quota)
{
	int new_uc_idx;

	if (mdss_res->bus_hdl < 1) {
		pr_err("invalid bus handle %d\n", mdss_res->bus_hdl);
		return -EINVAL;
	}

	if ((ab_quota | ib_quota) == 0) {
		new_uc_idx = 0;
	} else {
		int i;
		struct msm_bus_vectors *vect = NULL;
		struct msm_bus_scale_pdata *bw_table =
			mdss_res->bus_scale_table;
		unsigned long size;

		if (!bw_table || !mdss_res->axi_port_cnt) {
			pr_err("invalid input\n");
			return -EINVAL;
		}

		size = SZ_64M / mdss_res->axi_port_cnt;

		ab_quota = div_u64(ab_quota, mdss_res->axi_port_cnt);
		ib_quota = div_u64(ib_quota, mdss_res->axi_port_cnt);

		new_uc_idx = (mdss_res->curr_bw_uc_idx %
			(bw_table->num_usecases - 1)) + 1;

		for (i = 0; i < mdss_res->axi_port_cnt; i++) {
			vect = &bw_table->usecase[mdss_res->curr_bw_uc_idx].
				vectors[i];

			/* avoid performing updates for small changes */
			if ((ALIGN(ab_quota, size) == ALIGN(vect->ab, size)) &&
			    (ALIGN(ib_quota, size) == ALIGN(vect->ib, size))) {
				pr_debug("skip bus scaling, no changes\n");
				return 0;
			}

			vect = &bw_table->usecase[new_uc_idx].vectors[i];
			vect->ab = ab_quota;
			vect->ib = ib_quota;

			pr_debug("uc_idx=%d path_idx=%d ab=%llu ib=%llu\n",
				new_uc_idx, i, vect->ab, vect->ib);
		}
	}
	mdss_res->curr_bw_uc_idx = new_uc_idx;

	return msm_bus_scale_client_update_request(mdss_res->bus_hdl,
		new_uc_idx);
}

static inline u32 mdss_mdp_irq_mask(u32 intr_type, u32 intf_num)
{
	if (intr_type == MDSS_MDP_IRQ_INTF_UNDER_RUN ||
	    intr_type == MDSS_MDP_IRQ_INTF_VSYNC)
		intf_num = (intf_num - MDSS_MDP_INTF0) * 2;
	return 1 << (intr_type + intf_num);
}

/* function assumes that mdp is clocked to access hw registers */
void mdss_mdp_irq_clear(struct mdss_data_type *mdata,
		u32 intr_type, u32 intf_num)
{
	unsigned long irq_flags;
	u32 irq;

	irq = mdss_mdp_irq_mask(intr_type, intf_num);

	pr_debug("clearing mdp irq mask=%x\n", irq);
	spin_lock_irqsave(&mdp_lock, irq_flags);
	writel_relaxed(irq, mdata->mdp_base + MDSS_MDP_REG_INTR_CLEAR);
	spin_unlock_irqrestore(&mdp_lock, irq_flags);
}

int mdss_mdp_irq_enable(u32 intr_type, u32 intf_num)
{
	u32 irq;
	unsigned long irq_flags;
	int ret = 0;

	irq = mdss_mdp_irq_mask(intr_type, intf_num);

	spin_lock_irqsave(&mdp_lock, irq_flags);
	if (mdss_res->mdp_irq_mask & irq) {
		pr_warn("MDSS MDP IRQ-0x%x is already set, mask=%x\n",
				irq, mdss_res->mdp_irq_mask);
		ret = -EBUSY;
	} else {
		pr_debug("MDP IRQ mask old=%x new=%x\n",
				mdss_res->mdp_irq_mask, irq);
		mdss_res->mdp_irq_mask |= irq;
		MDSS_MDP_REG_WRITE(MDSS_MDP_REG_INTR_CLEAR, irq);
		MDSS_MDP_REG_WRITE(MDSS_MDP_REG_INTR_EN,
				mdss_res->mdp_irq_mask);
		mdss_enable_irq(&mdss_mdp_hw);
	}
	spin_unlock_irqrestore(&mdp_lock, irq_flags);

	return ret;
}
int mdss_mdp_hist_irq_enable(u32 irq)
{
	unsigned long irq_flags;
	int ret = 0;

	spin_lock_irqsave(&mdp_lock, irq_flags);
	if (mdss_res->mdp_hist_irq_mask & irq) {
		pr_warn("MDSS MDP Hist IRQ-0x%x is already set, mask=%x\n",
				irq, mdss_res->mdp_hist_irq_mask);
		ret = -EBUSY;
	} else {
		pr_debug("MDP IRQ mask old=%x new=%x\n",
				mdss_res->mdp_hist_irq_mask, irq);
		mdss_res->mdp_hist_irq_mask |= irq;
		MDSS_MDP_REG_WRITE(MDSS_MDP_REG_HIST_INTR_CLEAR, irq);
		MDSS_MDP_REG_WRITE(MDSS_MDP_REG_HIST_INTR_EN,
				mdss_res->mdp_hist_irq_mask);
		mdss_enable_irq(&mdss_mdp_hw);
	}
	spin_unlock_irqrestore(&mdp_lock, irq_flags);

	return ret;
}

void mdss_mdp_irq_disable(u32 intr_type, u32 intf_num)
{
	u32 irq;
	unsigned long irq_flags;

	irq = mdss_mdp_irq_mask(intr_type, intf_num);

	spin_lock_irqsave(&mdp_lock, irq_flags);
	if (!(mdss_res->mdp_irq_mask & irq)) {
		pr_warn("MDSS MDP IRQ-%x is NOT set, mask=%x\n",
				irq, mdss_res->mdp_irq_mask);
	} else {
		mdss_res->mdp_irq_mask &= ~irq;

		MDSS_MDP_REG_WRITE(MDSS_MDP_REG_INTR_EN,
				mdss_res->mdp_irq_mask);
		if ((mdss_res->mdp_irq_mask == 0) &&
			(mdss_res->mdp_hist_irq_mask == 0))
			mdss_disable_irq(&mdss_mdp_hw);
	}
	spin_unlock_irqrestore(&mdp_lock, irq_flags);
}

void mdss_mdp_hist_irq_disable(u32 irq)
{
	unsigned long irq_flags;

	spin_lock_irqsave(&mdp_lock, irq_flags);
	if (!(mdss_res->mdp_hist_irq_mask & irq)) {
		pr_warn("MDSS MDP IRQ-%x is NOT set, mask=%x\n",
				irq, mdss_res->mdp_hist_irq_mask);
	} else {
		mdss_res->mdp_hist_irq_mask &= ~irq;
		MDSS_MDP_REG_WRITE(MDSS_MDP_REG_HIST_INTR_EN,
				mdss_res->mdp_hist_irq_mask);
		if ((mdss_res->mdp_irq_mask == 0) &&
			(mdss_res->mdp_hist_irq_mask == 0))
			mdss_disable_irq(&mdss_mdp_hw);
	}
	spin_unlock_irqrestore(&mdp_lock, irq_flags);
}

/* called from interrupt context */
void mdss_mdp_irq_disable_nosync(u32 intr_type, u32 intf_num)
{
	u32 irq;

	irq = mdss_mdp_irq_mask(intr_type, intf_num);

	if (!(mdss_res->mdp_irq_mask & irq)) {
		pr_warn("MDSS MDP IRQ-%x is NOT set, mask=%x\n",
				irq, mdss_res->mdp_irq_mask);
	} else {
		mdss_res->mdp_irq_mask &= ~irq;
		MDSS_MDP_REG_WRITE(MDSS_MDP_REG_INTR_EN,
				mdss_res->mdp_irq_mask);
		if ((mdss_res->mdp_irq_mask == 0) &&
			(mdss_res->mdp_hist_irq_mask == 0))
			mdss_disable_irq_nosync(&mdss_mdp_hw);
	}
}

static inline struct clk *mdss_mdp_get_clk(u32 clk_idx)
{
	if (clk_idx < MDSS_MAX_CLK)
		return mdss_res->mdp_clk[clk_idx];
	return NULL;
}

static int mdss_mdp_clk_update(u32 clk_idx, u32 enable)
{
	int ret = -ENODEV;
	struct clk *clk = mdss_mdp_get_clk(clk_idx);

	if (clk) {
		pr_debug("clk=%d en=%d\n", clk_idx, enable);
		if (enable) {
			if (clk_idx == MDSS_CLK_MDP_VSYNC)
				clk_set_rate(clk, 19200000);
			ret = clk_prepare_enable(clk);
		} else {
			clk_disable_unprepare(clk);
			ret = 0;
		}
	}
	return ret;
}

int mdss_mdp_vsync_clk_enable(int enable)
{
	int ret = 0;
	pr_debug("clk enable=%d\n", enable);
	mutex_lock(&mdp_clk_lock);
	if (mdss_res->vsync_ena != enable) {
		mdss_res->vsync_ena = enable;
		ret = mdss_mdp_clk_update(MDSS_CLK_MDP_VSYNC, enable);
	}
	mutex_unlock(&mdp_clk_lock);
	return ret;
}

void mdss_mdp_set_clk_rate(unsigned long rate)
{
	struct mdss_data_type *mdata = mdss_res;
	unsigned long clk_rate;
	struct clk *clk = mdss_mdp_get_clk(MDSS_CLK_MDP_SRC);
	unsigned long min_clk_rate;

	min_clk_rate = max(rate, mdata->min_mdp_clk);

	if (clk) {
		mutex_lock(&mdp_clk_lock);
		if (min_clk_rate < mdata->max_mdp_clk_rate)
			clk_rate = clk_round_rate(clk, min_clk_rate);
		else
			clk_rate = mdata->max_mdp_clk_rate;
		if (IS_ERR_VALUE(clk_rate)) {
			pr_err("unable to round rate err=%ld\n", clk_rate);
		} else if (clk_rate != clk_get_rate(clk)) {
			if (IS_ERR_VALUE(clk_set_rate(clk, clk_rate)))
				pr_err("clk_set_rate failed\n");
			else
				pr_debug("mdp clk rate=%lu\n", clk_rate);
		}
		mutex_unlock(&mdp_clk_lock);
	} else {
		pr_err("mdp src clk not setup properly\n");
	}
}

unsigned long mdss_mdp_get_clk_rate(u32 clk_idx)
{
	unsigned long clk_rate = 0;
	struct clk *clk = mdss_mdp_get_clk(clk_idx);
	mutex_lock(&mdp_clk_lock);
	if (clk)
		clk_rate = clk_get_rate(clk);
	mutex_unlock(&mdp_clk_lock);

	return clk_rate;
}

/**
 * mdss_bus_bandwidth_ctrl() -- place bus bandwidth request
 * @enable:	value of enable or disable
 *
 * Function place bus bandwidth request to allocate saved bandwidth
 * if enabled or free bus bandwidth allocation if disabled.
 * Bus bandwidth is required by mdp.For dsi, it only requires to send
 * dcs coammnd.
 */
void mdss_bus_bandwidth_ctrl(int enable)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	static int bus_bw_cnt;
	int changed = 0;

	mutex_lock(&bus_bw_lock);
	if (enable) {
		if (bus_bw_cnt == 0)
			changed++;
		bus_bw_cnt++;
	} else {
		if (bus_bw_cnt) {
			bus_bw_cnt--;
			if (bus_bw_cnt == 0)
				changed++;
		} else {
			pr_err("Can not be turned off\n");
		}
	}

	pr_debug("bw_cnt=%d changed=%d enable=%d\n",
			bus_bw_cnt, changed, enable);

	if (changed) {
		if (!enable) {
			msm_bus_scale_client_update_request(
				mdata->bus_hdl, 0);
			mdss_iommu_dettach(mdata);
			pm_runtime_put(&mdata->pdev->dev);
		} else {
			pm_runtime_get_sync(&mdata->pdev->dev);
			msm_bus_scale_client_update_request(
				mdata->bus_hdl, mdata->curr_bw_uc_idx);
			if (!mdata->handoff_pending)
				mdss_iommu_attach(mdata);
		}
	}

	mutex_unlock(&bus_bw_lock);
}
EXPORT_SYMBOL(mdss_bus_bandwidth_ctrl);

void mdss_mdp_clk_ctrl(int enable, int isr)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	static int mdp_clk_cnt;
	int changed = 0;

	mutex_lock(&mdp_clk_lock);
	if (enable) {
		if (mdp_clk_cnt == 0)
			changed++;
		mdp_clk_cnt++;
	} else {
		if (mdp_clk_cnt) {
			mdp_clk_cnt--;
			if (mdp_clk_cnt == 0)
				changed++;
		} else {
			pr_err("Can not be turned off\n");
		}
	}

	pr_debug("%s: clk_cnt=%d changed=%d enable=%d\n",
			__func__, mdp_clk_cnt, changed, enable);

	if (changed) {
		mdata->clk_ena = enable;
		if (enable)
			pm_runtime_get_sync(&mdata->pdev->dev);

		mdss_mdp_clk_update(MDSS_CLK_AHB, enable);
		mdss_mdp_clk_update(MDSS_CLK_AXI, enable);
		mdss_mdp_clk_update(MDSS_CLK_MDP_CORE, enable);
		mdss_mdp_clk_update(MDSS_CLK_MDP_LUT, enable);
		if (mdata->vsync_ena)
			mdss_mdp_clk_update(MDSS_CLK_MDP_VSYNC, enable);

		mdss_bus_bandwidth_ctrl(enable);

		if (!enable)
			pm_runtime_put(&mdata->pdev->dev);
	}

	mutex_unlock(&mdp_clk_lock);
}

static inline int mdss_mdp_irq_clk_register(struct mdss_data_type *mdata,
					    char *clk_name, int clk_idx)
{
	struct clk *tmp;
	if (clk_idx >= MDSS_MAX_CLK) {
		pr_err("invalid clk index %d\n", clk_idx);
		return -EINVAL;
	}

	tmp = devm_clk_get(&mdata->pdev->dev, clk_name);
	if (IS_ERR(tmp)) {
		pr_err("unable to get clk: %s\n", clk_name);
		return PTR_ERR(tmp);
	}

	mdata->mdp_clk[clk_idx] = tmp;
	return 0;
}

static int mdss_mdp_irq_clk_setup(struct mdss_data_type *mdata)
{
	int ret;

	ret = of_property_read_u32(mdata->pdev->dev.of_node,
			"qcom,max-clk-rate", &mdata->max_mdp_clk_rate);
	if (ret) {
		pr_err("failed to get max mdp clock rate\n");
		return ret;
	}

	pr_debug("max mdp clk rate=%d\n", mdata->max_mdp_clk_rate);

	ret = devm_request_irq(&mdata->pdev->dev, mdata->irq, mdss_irq_handler,
			 IRQF_DISABLED,	"MDSS", mdata);
	if (ret) {
		pr_err("mdp request_irq() failed!\n");
		return ret;
	}
	disable_irq(mdata->irq);

	mdata->fs = devm_regulator_get(&mdata->pdev->dev, "vdd");
	if (IS_ERR_OR_NULL(mdata->fs)) {
		mdata->fs = NULL;
		pr_err("unable to get gdsc regulator\n");
		return -EINVAL;
	}
	mdata->fs_ena = false;

	mdata->vdd_cx = devm_regulator_get(&mdata->pdev->dev,
				"vdd-cx");
	if (IS_ERR_OR_NULL(mdata->vdd_cx)) {
		pr_debug("unable to get CX reg. rc=%d\n",
					PTR_RET(mdata->vdd_cx));
		mdata->vdd_cx = NULL;
	}

	if (mdss_mdp_irq_clk_register(mdata, "bus_clk", MDSS_CLK_AXI) ||
	    mdss_mdp_irq_clk_register(mdata, "iface_clk", MDSS_CLK_AHB) ||
	    mdss_mdp_irq_clk_register(mdata, "core_clk_src",
				      MDSS_CLK_MDP_SRC) ||
	    mdss_mdp_irq_clk_register(mdata, "core_clk",
				      MDSS_CLK_MDP_CORE) ||
	    mdss_mdp_irq_clk_register(mdata, "lut_clk", MDSS_CLK_MDP_LUT) ||
	    mdss_mdp_irq_clk_register(mdata, "vsync_clk", MDSS_CLK_MDP_VSYNC))
		return -EINVAL;

	mdss_mdp_set_clk_rate(MDP_CLK_DEFAULT_RATE);
	pr_debug("mdp clk rate=%ld\n", mdss_mdp_get_clk_rate(MDSS_CLK_MDP_SRC));

	return 0;
}

int mdss_iommu_attach(struct mdss_data_type *mdata)
{
	struct iommu_domain *domain;
	struct mdss_iommu_map_type *iomap;
	int i;

	mutex_lock(&mdp_iommu_lock);
	if (mdata->iommu_attached) {
		pr_debug("mdp iommu already attached\n");
		mutex_unlock(&mdp_iommu_lock);
		return 0;
	}

	for (i = 0; i < MDSS_IOMMU_MAX_DOMAIN; i++) {
		iomap = mdata->iommu_map + i;

		domain = msm_get_iommu_domain(iomap->domain_idx);
		if (!domain) {
			WARN(1, "could not attach iommu client %s to ctx %s\n",
				iomap->client_name, iomap->ctx_name);
			continue;
		}
		iommu_attach_device(domain, iomap->ctx);
	}

	mdata->iommu_attached = true;
	mutex_unlock(&mdp_iommu_lock);

	return 0;
}

int mdss_iommu_dettach(struct mdss_data_type *mdata)
{
	struct iommu_domain *domain;
	struct mdss_iommu_map_type *iomap;
	int i;

	mutex_lock(&mdp_iommu_lock);
	if (!mdata->iommu_attached) {
		pr_debug("mdp iommu already dettached\n");
		mutex_unlock(&mdp_iommu_lock);
		return 0;
	}

	for (i = 0; i < MDSS_IOMMU_MAX_DOMAIN; i++) {
		iomap = mdata->iommu_map + i;

		domain = msm_get_iommu_domain(iomap->domain_idx);
		if (!domain) {
			pr_err("unable to get iommu domain(%d)\n",
				iomap->domain_idx);
			continue;
		}
		iommu_detach_device(domain, iomap->ctx);
	}

	mdata->iommu_attached = false;
	mutex_unlock(&mdp_iommu_lock);

	return 0;
}

int mdss_iommu_init(struct mdss_data_type *mdata)
{
	struct msm_iova_layout layout;
	struct iommu_domain *domain;
	struct mdss_iommu_map_type *iomap;
	int i;

	if (mdata->iommu_map) {
		pr_warn("iommu already initialized\n");
		return 0;
	}

	for (i = 0; i < MDSS_IOMMU_MAX_DOMAIN; i++) {
		iomap = &mdss_iommu_map[i];

		layout.client_name = iomap->client_name;
		layout.partitions = iomap->partitions;
		layout.npartitions = iomap->npartitions;
		layout.is_secure = (i == MDSS_IOMMU_DOMAIN_SECURE);

		iomap->domain_idx = msm_register_domain(&layout);
		if (IS_ERR_VALUE(iomap->domain_idx))
			return -EINVAL;

		domain = msm_get_iommu_domain(iomap->domain_idx);
		if (!domain) {
			pr_err("unable to get iommu domain(%d)\n",
				iomap->domain_idx);
			return -EINVAL;
		}

		iomap->ctx = msm_iommu_get_ctx(iomap->ctx_name);
		if (!iomap->ctx) {
			pr_warn("unable to get iommu ctx(%s)\n",
				iomap->ctx_name);
			return -EINVAL;
		}
	}

	mdata->iommu_map = mdss_iommu_map;

	return 0;
}

static int mdss_debug_stat_ctl_dump(struct mdss_mdp_ctl *ctl,
		char *bp, int len)
{
	int total = 0;

	if (!ctl->ref_cnt)
		return 0;

	if (ctl->intf_num) {
		total = scnprintf(bp, len,
			"intf%d: play: %08u \tvsync: %08u \tunderrun: %08u\n",
				ctl->intf_num, ctl->play_cnt,
				ctl->vsync_cnt, ctl->underrun_cnt);
	} else {
		total = scnprintf(bp, len, "wb: \tmode=%x \tplay: %08u\n",
				ctl->opmode, ctl->play_cnt);
	}

	return total;
}

static int mdss_debug_dump_stats(void *data, char *buf, int len)
{
	struct mdss_data_type *mdata = data;
	struct mdss_mdp_pipe *pipe;
	int i, total = 0;

	for (i = 0; i < mdata->nctl; i++)
		total += mdss_debug_stat_ctl_dump(mdata->ctl_off + i,
			buf + total, len - total);

	total += scnprintf(buf + total, len - total, "\n");

	for (i = 0; i < mdata->nvig_pipes; i++) {
		pipe = mdata->vig_pipes + i;
		total += scnprintf(buf + total, len - total,
			"VIG%d :   %08u\t", i, pipe->play_cnt);
	}
	total += scnprintf(buf + total, len - total, "\n");

	for (i = 0; i < mdata->nrgb_pipes; i++) {
		pipe = mdata->rgb_pipes + i;
		total += scnprintf(buf + total, len - total,
			"RGB%d :   %08u\t", i, pipe->play_cnt);
	}
	total += scnprintf(buf + total, len - total, "\n");

	for (i = 0; i < mdata->ndma_pipes; i++) {
		pipe = mdata->dma_pipes + i;
		total += scnprintf(buf + total, len - total,
			"DMA%d :   %08u\t", i, pipe->play_cnt);
	}
	return total;
}

static void mdss_debug_enable_clock(int on)
{
	if (on)
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);
	else
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
}

static int mdss_mdp_debug_init(struct mdss_data_type *mdata)
{
	int rc;

	mdata->debug_inf.debug_dump_stats = mdss_debug_dump_stats;
	mdata->debug_inf.debug_enable_clock = mdss_debug_enable_clock;

	rc = mdss_debugfs_init(mdata);
	if (rc)
		return rc;

	mdss_debug_register_base(NULL, mdata->mdp_base, mdata->mdp_reg_size);

	return 0;
}

int mdss_hw_init(struct mdss_data_type *mdata)
{
	int i, j;
	char *offset;
	struct mdss_mdp_pipe *vig;

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);
	mdata->mdp_rev = MDSS_MDP_REG_READ(MDSS_MDP_REG_HW_VERSION);
	pr_info_once("MDP Rev=%x\n", mdata->mdp_rev);

	/* disable hw underrun recovery */
	writel_relaxed(0x0, mdata->mdp_base +
			MDSS_MDP_REG_VIDEO_INTF_UNDERFLOW_CTL);

	if (mdata->hw_settings) {
		struct mdss_hw_settings *hws = mdata->hw_settings;

		while (hws->reg) {
			writel_relaxed(hws->val, hws->reg);
			hws++;
		}
	}

	for (i = 0; i < mdata->nmixers_intf; i++) {
		offset = mdata->mixer_intf[i].dspp_base +
				MDSS_MDP_REG_DSPP_HIST_LUT_BASE;
		for (j = 0; j < ENHIST_LUT_ENTRIES; j++)
			writel_relaxed(j, offset);

		/* swap */
		writel_relaxed(1, offset + 4);
	}
	vig = mdata->vig_pipes;
	for (i = 0; i < mdata->nvig_pipes; i++) {
		offset = vig[i].base +
			MDSS_MDP_REG_VIG_HIST_LUT_BASE;
		for (j = 0; j < ENHIST_LUT_ENTRIES; j++)
			writel_relaxed(j, offset);
		/* swap */
		writel_relaxed(1, offset + 16);
	}

	mdata->nmax_concurrent_ad_hw = (mdata->mdp_rev <= MDSS_MDP_HW_REV_102) ?
									1 : 2;
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
	pr_debug("MDP hw init done\n");

	return 0;
}

static u32 mdss_mdp_res_init(struct mdss_data_type *mdata)
{
	u32 rc = 0;

	if (mdata->res_init) {
		pr_err("mdss resources already initialized\n");
		return -EPERM;
	}

	mdata->res_init = true;
	mdata->clk_ena = false;
	mdata->irq_mask = MDSS_MDP_DEFAULT_INTR_MASK;
	mdata->irq_ena = false;

	rc = mdss_mdp_irq_clk_setup(mdata);
	if (rc)
		return rc;

	mdata->hist_intr.req = 0;
	mdata->hist_intr.curr = 0;
	mdata->hist_intr.state = 0;
	spin_lock_init(&mdata->hist_intr.lock);

	mdata->iclient = msm_ion_client_create(-1, mdata->pdev->name);
	if (IS_ERR_OR_NULL(mdata->iclient)) {
		pr_err("msm_ion_client_create() return error (%p)\n",
				mdata->iclient);
		mdata->iclient = NULL;
	}

	rc = mdss_iommu_init(mdata);

	return rc;
}

void mdss_mdp_footswitch_ctrl_splash(int on)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	if (mdata != NULL) {
		if (on) {
			pr_debug("Enable MDP FS for splash.\n");
			mdata->handoff_pending = true;
			regulator_enable(mdata->fs);
			mdss_hw_init(mdata);
		} else {
			pr_debug("Disable MDP FS for splash.\n");
			regulator_disable(mdata->fs);
			mdata->handoff_pending = false;
		}
	} else {
		pr_warn("mdss mdata not initialized\n");
	}
}

static ssize_t mdss_mdp_show_capabilities(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdss_data_type *mdata = dev_get_drvdata(dev);
	size_t len = PAGE_SIZE;
	int cnt = 0;

#define SPRINT(fmt, ...) \
		(cnt += scnprintf(buf + cnt, len - cnt, fmt, ##__VA_ARGS__))

	SPRINT("mdp_version=5\n");
	SPRINT("hw_rev=%d\n", mdata->mdp_rev);
	SPRINT("rgb_pipes=%d\n", mdata->nrgb_pipes);
	SPRINT("vig_pipes=%d\n", mdata->nvig_pipes);
	SPRINT("dma_pipes=%d\n", mdata->ndma_pipes);
	SPRINT("smp_count=%d\n", mdata->smp_mb_cnt);
	SPRINT("smp_size=%d\n", mdata->smp_mb_size);
	SPRINT("max_downscale_ratio=%d\n", MAX_DOWNSCALE_RATIO);
	SPRINT("max_upscale_ratio=%d\n", MAX_UPSCALE_RATIO);
	if (mdata->max_bw_low)
		SPRINT("max_bandwidth_low=%u\n", mdata->max_bw_low);
	if (mdata->max_bw_high)
		SPRINT("max_bandwidth_high=%u\n", mdata->max_bw_high);
	SPRINT("features=");
	if (mdata->has_bwc)
		SPRINT(" bwc");
	if (mdata->has_decimation)
		SPRINT(" decimation");
	if (mdata->highest_bank_bit)
		SPRINT(" tile_format");
	SPRINT("\n");

	return cnt;
}

static DEVICE_ATTR(caps, S_IRUGO, mdss_mdp_show_capabilities, NULL);

static struct attribute *mdp_fs_attrs[] = {
	&dev_attr_caps.attr,
	NULL
};

static struct attribute_group mdp_fs_attr_group = {
	.attrs = mdp_fs_attrs
};

static int mdss_mdp_register_sysfs(struct mdss_data_type *mdata)
{
	struct device *dev = &mdata->pdev->dev;
	int rc;

	rc = sysfs_create_group(&dev->kobj, &mdp_fs_attr_group);

	return rc;
}

static int mdss_mdp_probe(struct platform_device *pdev)
{
	struct resource *res;
	int rc;
	struct mdss_data_type *mdata;

	if (!pdev->dev.of_node) {
		pr_err("MDP driver only supports device tree probe\n");
		return -ENOTSUPP;
	}

	if (mdss_res) {
		pr_err("MDP already initialized\n");
		return -EINVAL;
	}

	mdata = devm_kzalloc(&pdev->dev, sizeof(*mdata), GFP_KERNEL);
	if (mdata == NULL)
		return -ENOMEM;

	pdev->id = 0;
	mdata->pdev = pdev;
	platform_set_drvdata(pdev, mdata);
	mdss_res = mdata;
	mutex_init(&mdata->reg_lock);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mdp_phys");
	if (!res) {
		pr_err("unable to get MDP base address\n");
		rc = -ENOMEM;
		goto probe_done;
	}

	mdata->mdp_reg_size = resource_size(res);
	mdata->mdp_base = devm_ioremap(&pdev->dev, res->start,
				       mdata->mdp_reg_size);
	if (unlikely(!mdata->mdp_base)) {
		pr_err("unable to map MDP base\n");
		rc = -ENOMEM;
		goto probe_done;
	}
	pr_info("MDP HW Base phy_Address=0x%x virt=0x%x\n",
		(int) res->start,
		(int) mdata->mdp_base);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "vbif_phys");
	if (!res) {
		pr_err("unable to get MDSS VBIF base address\n");
		rc = -ENOMEM;
		goto probe_done;
	}

	mdata->vbif_base = devm_ioremap(&pdev->dev, res->start,
					resource_size(res));
	if (unlikely(!mdata->vbif_base)) {
		pr_err("unable to map MDSS VBIF base\n");
		rc = -ENOMEM;
		goto probe_done;
	}
	pr_info("MDSS VBIF HW Base phy_Address=0x%x virt=0x%x\n",
		(int) res->start,
		(int) mdata->vbif_base);

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		pr_err("unable to get MDSS irq\n");
		rc = -ENOMEM;
		goto probe_done;
	}
	mdata->irq = res->start;
	mdss_mdp_hw.ptr = mdata;

	/*populate hw iomem base info from device tree*/
	rc = mdss_mdp_parse_dt(pdev);
	if (rc) {
		pr_err("unable to parse device tree\n");
		goto probe_done;
	}

	rc = mdss_mdp_res_init(mdata);
	if (rc) {
		pr_err("unable to initialize mdss mdp resources\n");
		goto probe_done;
	}
	rc = mdss_mdp_pp_init(&pdev->dev);
	if (rc) {
		pr_err("unable to initialize mdss pp resources\n");
		goto probe_done;
	}
	rc = mdss_mdp_bus_scale_register(mdata);
	if (rc) {
		pr_err("unable to register bus scaling\n");
		goto probe_done;
	}

	rc = mdss_mdp_debug_init(mdata);
	if (rc) {
		pr_err("unable to initialize mdp debugging\n");
		goto probe_done;
	}

	pm_runtime_set_suspended(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
	if (!pm_runtime_enabled(&pdev->dev))
		mdss_mdp_footswitch_ctrl(mdata, true);

	rc = mdss_mdp_register_sysfs(mdata);
	if (rc)
		pr_err("unable to register mdp sysfs nodes\n");

	rc = mdss_fb_register_mdp_instance(&mdp5);
	if (rc)
		pr_err("unable to register mdp instance\n");

	rc = mdss_register_irq(&mdss_mdp_hw);
	if (rc)
		pr_err("mdss_register_irq failed.\n");

probe_done:
	if (IS_ERR_VALUE(rc)) {
		mdss_mdp_hw.ptr = NULL;
		mdss_mdp_pp_term(&pdev->dev);
		mutex_destroy(&mdata->reg_lock);
		mdss_res = NULL;
	}

	return rc;
}

static void mdss_mdp_parse_dt_regs_array(const u32 *arr, char __iomem *hw_base,
	struct mdss_hw_settings *hws, int count)
{
	u32 len, reg;
	int i;

	if (!arr)
		return;

	for (i = 0, len = count * 2; i < len; i += 2) {
		reg = be32_to_cpu(arr[i]);
		hws->reg = hw_base + reg;
		hws->val = be32_to_cpu(arr[i + 1]);
		pr_debug("reg: 0x%04x=0x%08x\n", reg, hws->val);
		hws++;
	}
}

int mdss_mdp_parse_dt_hw_settings(struct platform_device *pdev)
{
	struct mdss_data_type *mdata = platform_get_drvdata(pdev);
	struct mdss_hw_settings *hws;
	const u32 *vbif_arr, *mdp_arr;
	int vbif_len, mdp_len;

	vbif_arr = of_get_property(pdev->dev.of_node, "qcom,vbif-settings",
			&vbif_len);
	if (!vbif_arr || (vbif_len & 1)) {
		pr_warn("MDSS VBIF settings not found\n");
		vbif_len = 0;
	}
	vbif_len /= 2 * sizeof(u32);

	mdp_arr = of_get_property(pdev->dev.of_node, "qcom,mdp-settings",
			&mdp_len);
	if (!mdp_arr || (mdp_len & 1)) {
		pr_warn("MDSS MDP settings not found\n");
		mdp_len = 0;
	}
	mdp_len /= 2 * sizeof(u32);

	if ((mdp_len + vbif_len) == 0)
		return 0;

	hws = devm_kzalloc(&pdev->dev, sizeof(*hws) * (vbif_len + mdp_len + 1),
			GFP_KERNEL);
	if (!hws)
		return -ENOMEM;

	mdss_mdp_parse_dt_regs_array(vbif_arr, mdata->vbif_base, hws, vbif_len);
	mdss_mdp_parse_dt_regs_array(mdp_arr, mdata->mdp_base,
		hws + vbif_len, mdp_len);

	mdata->hw_settings = hws;

	return 0;
}

static int mdss_mdp_get_pan_intf(const char *pan_intf)
{
	int i, rc = MDSS_PANEL_INTF_INVALID;

	if (!pan_intf)
		return rc;

	for (i = 0; i < ARRAY_SIZE(pan_types); i++) {
		if (!strncmp(pan_intf, pan_types[i].name, MDSS_MAX_PANEL_LEN)) {
			rc = pan_types[i].type;
			break;
		}
	}
	return rc;
}

static int mdss_mdp_get_pan_cfg(struct mdss_panel_cfg *pan_cfg)
{
	char *t = NULL;
	char pan_intf_str[MDSS_MAX_PANEL_LEN];
	int rc, i, panel_len;
	char pan_name[MDSS_MAX_PANEL_LEN];

	if (!pan_cfg)
		return -EINVAL;

	strlcpy(pan_name, &pan_cfg->arg_cfg[0], sizeof(pan_cfg->arg_cfg));
	if (pan_name[0] == '0') {
		pan_cfg->lk_cfg = false;
	} else if (pan_name[0] == '1') {
		pan_cfg->lk_cfg = true;
	} else {
		/* read from dt */
		pan_cfg->lk_cfg = true;
		pan_cfg->pan_intf = MDSS_PANEL_INTF_INVALID;
		return -EINVAL;
	}

	/* skip lk cfg and delimiter; ex: "0:" */
	strlcpy(pan_name, &pan_name[2], MDSS_MAX_PANEL_LEN);
	t = strnstr(pan_name, ":", MDSS_MAX_PANEL_LEN);
	if (!t) {
		pr_err("%s: pan_name=[%s] invalid\n",
			__func__, pan_name);
		pan_cfg->pan_intf = MDSS_PANEL_INTF_INVALID;
		return -EINVAL;
	}

	for (i = 0; ((pan_name + i) < t) && (i < 4); i++)
		pan_intf_str[i] = *(pan_name + i);
	pan_intf_str[i] = 0;
	pr_debug("%s:%d panel intf %s\n", __func__, __LINE__, pan_intf_str);
	/* point to the start of panel name */
	t = t + 1;
	strlcpy(&pan_cfg->arg_cfg[0], t, sizeof(pan_cfg->arg_cfg));
	pr_debug("%s:%d: t=[%s] panel name=[%s]\n", __func__, __LINE__,
		t, pan_cfg->arg_cfg);

	panel_len = strlen(pan_cfg->arg_cfg);
	if (!panel_len) {
		pr_err("%s: Panel name is invalid\n", __func__);
		pan_cfg->pan_intf = MDSS_PANEL_INTF_INVALID;
		return -EINVAL;
	}

	rc = mdss_mdp_get_pan_intf(pan_intf_str);
	pan_cfg->pan_intf = (rc < 0) ?  MDSS_PANEL_INTF_INVALID : rc;
	return 0;
}

static int mdss_mdp_parse_dt_pan_intf(struct platform_device *pdev)
{
	int rc;
	struct mdss_data_type *mdata = platform_get_drvdata(pdev);
	const char *prim_intf = NULL;

	rc = of_property_read_string(pdev->dev.of_node,
				"qcom,mdss-pref-prim-intf", &prim_intf);
	if (rc)
		return -ENODEV;

	rc = mdss_mdp_get_pan_intf(prim_intf);
	if (rc < 0) {
		mdata->pan_cfg.pan_intf = MDSS_PANEL_INTF_INVALID;
	} else {
		mdata->pan_cfg.pan_intf = rc;
		rc = 0;
	}
	return rc;
}

static int mdss_mdp_parse_bootarg(struct platform_device *pdev)
{
	struct device_node *chosen_node;
	static const char *cmd_line;
	char *disp_idx, *end_idx;
	int rc, len = 0, name_len, cmd_len;
	int *intf_type;
	char *panel_name;
	struct mdss_panel_cfg *pan_cfg;
	struct mdss_data_type *mdata = platform_get_drvdata(pdev);

	mdata->pan_cfg.arg_cfg[MDSS_MAX_PANEL_LEN] = 0;
	pan_cfg = &mdata->pan_cfg;
	panel_name = &pan_cfg->arg_cfg[0];
	intf_type = &pan_cfg->pan_intf;

	/* reads from dt by default */
	pan_cfg->lk_cfg = true;

	chosen_node = of_find_node_by_name(NULL, "chosen");
	if (!chosen_node) {
		pr_err("%s: get chosen node failed\n", __func__);
		rc = -ENODEV;
		goto get_dt_pan;
	}

	cmd_line = of_get_property(chosen_node, "bootargs", &len);
	if (!cmd_line || len <= 0) {
		pr_err("%s: get bootargs failed\n", __func__);
		rc = -ENODEV;
		goto get_dt_pan;
	}

	name_len = strlen("mdss_mdp.panel=");
	cmd_len = strlen(cmd_line);
	disp_idx = strnstr(cmd_line, "mdss_mdp.panel=", cmd_len);
	if (!disp_idx) {
		pr_err("%s:%d:cmdline panel not set disp_idx=[%p]\n",
				__func__, __LINE__, disp_idx);
		memset(panel_name, 0x00, MDSS_MAX_PANEL_LEN);
		*intf_type = MDSS_PANEL_INTF_INVALID;
		rc = MDSS_PANEL_INTF_INVALID;
		goto get_dt_pan;
	}

	disp_idx += name_len;

	end_idx = strnstr(disp_idx, " ", MDSS_MAX_PANEL_LEN);
	pr_debug("%s:%d: pan_name=[%s] end=[%s]\n", __func__, __LINE__,
		 disp_idx, end_idx);
	if (!end_idx) {
		end_idx = disp_idx + strlen(disp_idx) + 1;
		pr_warn("%s:%d: pan_name=[%s] end=[%s]\n", __func__,
		       __LINE__, disp_idx, end_idx);
	}

	if (end_idx <= disp_idx) {
		pr_err("%s:%d:cmdline pan incorrect end=[%p] disp=[%p]\n",
			__func__, __LINE__, end_idx, disp_idx);
		memset(panel_name, 0x00, MDSS_MAX_PANEL_LEN);
		*intf_type = MDSS_PANEL_INTF_INVALID;
		rc = MDSS_PANEL_INTF_INVALID;
		goto get_dt_pan;
	}

	*end_idx = 0;
	len = end_idx - disp_idx + 1;
	if (len <= 0) {
		pr_warn("%s: panel name not rx", __func__);
		rc = -EINVAL;
		goto get_dt_pan;
	}

	strlcpy(panel_name, disp_idx, min(++len, MDSS_MAX_PANEL_LEN));
	pr_debug("%s:%d panel:[%s]", __func__, __LINE__, panel_name);
	of_node_put(chosen_node);

	rc = mdss_mdp_get_pan_cfg(pan_cfg);
	if (!rc) {
		pan_cfg->init_done = true;
		return rc;
	}

get_dt_pan:
	rc = mdss_mdp_parse_dt_pan_intf(pdev);
	/* if pref pan intf is not present */
	if (rc)
		pr_err("%s:unable to parse device tree for pan intf\n",
			__func__);
	else
		pan_cfg->init_done = true;

	of_node_put(chosen_node);
	return rc;
}

static int mdss_mdp_parse_dt(struct platform_device *pdev)
{
	int rc;

	rc = mdss_mdp_parse_dt_hw_settings(pdev);
	if (rc) {
		pr_err("Error in device tree : hw settings\n");
		return rc;
	}

	rc = mdss_mdp_parse_dt_pipe(pdev);
	if (rc) {
		pr_err("Error in device tree : pipes\n");
		return rc;
	}

	rc = mdss_mdp_parse_dt_mixer(pdev);
	if (rc) {
		pr_err("Error in device tree : mixers\n");
		return rc;
	}

	rc = mdss_mdp_parse_dt_ctl(pdev);
	if (rc) {
		pr_err("Error in device tree : ctl\n");
		return rc;
	}

	rc = mdss_mdp_parse_dt_video_intf(pdev);
	if (rc) {
		pr_err("Error in device tree : ctl\n");
		return rc;
	}

	rc = mdss_mdp_parse_dt_smp(pdev);
	if (rc) {
		pr_err("Error in device tree : smp\n");
		return rc;
	}

	rc = mdss_mdp_parse_dt_misc(pdev);
	if (rc) {
		pr_err("Error in device tree : misc\n");
		return rc;
	}

	rc = mdss_mdp_parse_dt_ad_cfg(pdev);
	if (rc) {
		pr_err("Error in device tree : ad\n");
		return rc;
	}

	rc = mdss_mdp_parse_bootarg(pdev);
	if (rc) {
		pr_err("%s: Error in panel override:rc=[%d]\n",
		       __func__, rc);
		return rc;
	}

	rc = mdss_mdp_parse_dt_bus_scale(pdev);
	if (rc) {
		pr_err("Error in device tree : bus scale\n");
		return rc;
	}

	return 0;
}


static int mdss_mdp_parse_dt_pipe(struct platform_device *pdev)
{
	u32 npipes, dma_off;
	int rc = 0;
	u32 nfids = 0, setup_cnt = 0, len, nxids = 0;
	u32 *offsets = NULL, *ftch_id = NULL, *xin_id = NULL;

	struct mdss_data_type *mdata = platform_get_drvdata(pdev);

	mdata->nvig_pipes = mdss_mdp_parse_dt_prop_len(pdev,
				"qcom,mdss-pipe-vig-off");
	mdata->nrgb_pipes = mdss_mdp_parse_dt_prop_len(pdev,
				"qcom,mdss-pipe-rgb-off");
	mdata->ndma_pipes = mdss_mdp_parse_dt_prop_len(pdev,
				"qcom,mdss-pipe-dma-off");

	npipes = mdata->nvig_pipes + mdata->nrgb_pipes + mdata->ndma_pipes;

	nfids  += mdss_mdp_parse_dt_prop_len(pdev,
			"qcom,mdss-pipe-vig-fetch-id");
	nfids  += mdss_mdp_parse_dt_prop_len(pdev,
			"qcom,mdss-pipe-rgb-fetch-id");
	nfids  += mdss_mdp_parse_dt_prop_len(pdev,
			"qcom,mdss-pipe-dma-fetch-id");
	if (npipes != nfids) {
		pr_err("device tree err: unequal number of pipes and smp ids");
		return -EINVAL;
	}

	nxids += mdss_mdp_parse_dt_prop_len(pdev, "qcom,mdss-pipe-vig-xin-id");
	nxids += mdss_mdp_parse_dt_prop_len(pdev, "qcom,mdss-pipe-rgb-xin-id");
	nxids += mdss_mdp_parse_dt_prop_len(pdev, "qcom,mdss-pipe-dma-xin-id");
	if (npipes != nxids) {
		pr_err("device tree err: unequal number of pipes and xin ids");
		return -EINVAL;
	}

	offsets = kzalloc(sizeof(u32) * npipes, GFP_KERNEL);
	if (!offsets) {
		pr_err("no mem assigned for offsets: kzalloc fail\n");
		return -ENOMEM;
	}

	ftch_id = kzalloc(sizeof(u32) * nfids, GFP_KERNEL);
	if (!ftch_id) {
		pr_err("no mem assigned for ftch_id: kzalloc fail\n");
		rc = -ENOMEM;
		goto ftch_alloc_fail;
	}

	xin_id = kzalloc(sizeof(u32) * nxids, GFP_KERNEL);
	if (!xin_id) {
		pr_err("no mem assigned for xin_id: kzalloc fail\n");
		rc = -ENOMEM;
		goto xin_alloc_fail;
	}

	mdata->vig_pipes = devm_kzalloc(&mdata->pdev->dev,
		sizeof(struct mdss_mdp_pipe) * mdata->nvig_pipes, GFP_KERNEL);
	if (!mdata->vig_pipes) {
		pr_err("no mem for vig_pipes: kzalloc fail\n");
		rc = -ENOMEM;
		goto vig_alloc_fail;
	}

	mdata->rgb_pipes = devm_kzalloc(&mdata->pdev->dev,
		sizeof(struct mdss_mdp_pipe) * mdata->nrgb_pipes, GFP_KERNEL);
	if (!mdata->rgb_pipes) {
		pr_err("no mem for rgb_pipes: kzalloc fail\n");
		rc = -ENOMEM;
		goto rgb_alloc_fail;
	}

	mdata->dma_pipes = devm_kzalloc(&mdata->pdev->dev,
		sizeof(struct mdss_mdp_pipe) * mdata->ndma_pipes, GFP_KERNEL);
	if (!mdata->dma_pipes) {
		pr_err("no mem for dma_pipes: kzalloc fail\n");
		rc = -ENOMEM;
		goto dma_alloc_fail;
	}

	rc = mdss_mdp_parse_dt_handler(pdev, "qcom,mdss-pipe-vig-fetch-id",
		ftch_id, mdata->nvig_pipes);
	if (rc)
		goto parse_fail;

	rc = mdss_mdp_parse_dt_handler(pdev, "qcom,mdss-pipe-vig-xin-id",
		xin_id, mdata->nvig_pipes);
	if (rc)
		goto parse_fail;

	rc = mdss_mdp_parse_dt_handler(pdev, "qcom,mdss-pipe-vig-off",
		offsets, mdata->nvig_pipes);
	if (rc)
		goto parse_fail;

	len = min_t(int, DEFAULT_TOTAL_VIG_PIPES, (int)mdata->nvig_pipes);
	rc = mdss_mdp_pipe_addr_setup(mdata, mdata->vig_pipes, offsets, ftch_id,
		xin_id, MDSS_MDP_PIPE_TYPE_VIG, MDSS_MDP_SSPP_VIG0, len);
	if (rc)
		goto parse_fail;

	setup_cnt += len;

	rc = mdss_mdp_parse_dt_handler(pdev, "qcom,mdss-pipe-rgb-fetch-id",
		ftch_id + mdata->nvig_pipes, mdata->nrgb_pipes);
	if (rc)
		goto parse_fail;

	rc = mdss_mdp_parse_dt_handler(pdev, "qcom,mdss-pipe-rgb-xin-id",
		xin_id + mdata->nvig_pipes, mdata->nrgb_pipes);
	if (rc)
		goto parse_fail;

	rc = mdss_mdp_parse_dt_handler(pdev, "qcom,mdss-pipe-rgb-off",
		offsets + mdata->nvig_pipes, mdata->nrgb_pipes);
	if (rc)
		goto parse_fail;

	len = min_t(int, DEFAULT_TOTAL_RGB_PIPES, (int)mdata->nrgb_pipes);
	rc = mdss_mdp_pipe_addr_setup(mdata, mdata->rgb_pipes,
		offsets + mdata->nvig_pipes, ftch_id + mdata->nvig_pipes,
		xin_id + mdata->nvig_pipes, MDSS_MDP_PIPE_TYPE_RGB,
		MDSS_MDP_SSPP_RGB0, len);
	if (rc)
		goto parse_fail;

	setup_cnt += len;
	dma_off = mdata->nvig_pipes + mdata->nrgb_pipes;

	rc = mdss_mdp_parse_dt_handler(pdev, "qcom,mdss-pipe-dma-fetch-id",
		ftch_id + dma_off, mdata->ndma_pipes);
	if (rc)
		goto parse_fail;

	rc = mdss_mdp_parse_dt_handler(pdev, "qcom,mdss-pipe-dma-xin-id",
		xin_id + dma_off, mdata->ndma_pipes);
	if (rc)
		goto parse_fail;

	rc = mdss_mdp_parse_dt_handler(pdev, "qcom,mdss-pipe-dma-off",
		offsets + dma_off, mdata->ndma_pipes);
	if (rc)
		goto parse_fail;

	len = mdata->ndma_pipes;
	rc = mdss_mdp_pipe_addr_setup(mdata, mdata->dma_pipes,
		offsets + dma_off, ftch_id + dma_off, xin_id + dma_off,
		MDSS_MDP_PIPE_TYPE_DMA, MDSS_MDP_SSPP_DMA0, len);
	if (rc)
		goto parse_fail;

	setup_cnt += len;

	if (mdata->nvig_pipes > DEFAULT_TOTAL_VIG_PIPES) {
		rc = mdss_mdp_pipe_addr_setup(mdata,
			mdata->vig_pipes + DEFAULT_TOTAL_VIG_PIPES,
			offsets + DEFAULT_TOTAL_VIG_PIPES,
			ftch_id + DEFAULT_TOTAL_VIG_PIPES,
			xin_id + DEFAULT_TOTAL_VIG_PIPES,
			MDSS_MDP_PIPE_TYPE_VIG, setup_cnt,
			mdata->nvig_pipes - DEFAULT_TOTAL_VIG_PIPES);
		if (rc)
			goto parse_fail;

		setup_cnt += mdata->nvig_pipes - DEFAULT_TOTAL_VIG_PIPES;
	}

	if (mdata->nrgb_pipes > DEFAULT_TOTAL_RGB_PIPES) {
		rc = mdss_mdp_pipe_addr_setup(mdata,
			mdata->rgb_pipes + DEFAULT_TOTAL_RGB_PIPES,
			offsets + mdata->nvig_pipes + DEFAULT_TOTAL_RGB_PIPES,
			ftch_id + mdata->nvig_pipes + DEFAULT_TOTAL_RGB_PIPES,
			xin_id + mdata->nvig_pipes + DEFAULT_TOTAL_RGB_PIPES,
			MDSS_MDP_PIPE_TYPE_RGB, setup_cnt,
			mdata->nrgb_pipes - DEFAULT_TOTAL_RGB_PIPES);
		if (rc)
			goto parse_fail;

		setup_cnt += mdata->nrgb_pipes - DEFAULT_TOTAL_RGB_PIPES;
	}

	goto parse_done;

parse_fail:
	kfree(mdata->dma_pipes);
dma_alloc_fail:
	kfree(mdata->rgb_pipes);
rgb_alloc_fail:
	kfree(mdata->vig_pipes);
parse_done:
vig_alloc_fail:
	kfree(xin_id);
xin_alloc_fail:
	kfree(ftch_id);
ftch_alloc_fail:
	kfree(offsets);
	return rc;
}

static int mdss_mdp_parse_dt_mixer(struct platform_device *pdev)
{

	u32 nmixers, ndspp, npingpong;
	int rc = 0;
	u32 *mixer_offsets = NULL, *dspp_offsets = NULL,
	    *pingpong_offsets = NULL;

	struct mdss_data_type *mdata = platform_get_drvdata(pdev);

	mdata->nmixers_intf = mdss_mdp_parse_dt_prop_len(pdev,
				"qcom,mdss-mixer-intf-off");
	mdata->nmixers_wb = mdss_mdp_parse_dt_prop_len(pdev,
				"qcom,mdss-mixer-wb-off");
	ndspp = mdss_mdp_parse_dt_prop_len(pdev,
				"qcom,mdss-dspp-off");
	npingpong = mdss_mdp_parse_dt_prop_len(pdev,
				"qcom,mdss-pingpong-off");
	nmixers = mdata->nmixers_intf + mdata->nmixers_wb;

	if (mdata->nmixers_intf != ndspp) {
		pr_err("device tree err: unequal no of dspp and intf mixers\n");
		return -EINVAL;
	}

	if (mdata->nmixers_intf != npingpong) {
		pr_err("device tree err: unequal no of pingpong and intf mixers\n");
		return -EINVAL;
	}

	mixer_offsets = kzalloc(sizeof(u32) * nmixers, GFP_KERNEL);
	if (!mixer_offsets) {
		pr_err("no mem assigned: kzalloc fail\n");
		return -ENOMEM;
	}

	dspp_offsets = kzalloc(sizeof(u32) * ndspp, GFP_KERNEL);
	if (!dspp_offsets) {
		pr_err("no mem assigned: kzalloc fail\n");
		rc = -ENOMEM;
		goto dspp_alloc_fail;
	}
	pingpong_offsets = kzalloc(sizeof(u32) * npingpong, GFP_KERNEL);
	if (!pingpong_offsets) {
		pr_err("no mem assigned: kzalloc fail\n");
		rc = -ENOMEM;
		goto pingpong_alloc_fail;
	}

	rc = mdss_mdp_parse_dt_handler(pdev, "qcom,mdss-mixer-intf-off",
		mixer_offsets, mdata->nmixers_intf);
	if (rc)
		goto parse_done;

	rc = mdss_mdp_parse_dt_handler(pdev, "qcom,mdss-mixer-wb-off",
		mixer_offsets + mdata->nmixers_intf, mdata->nmixers_wb);
	if (rc)
		goto parse_done;

	rc = mdss_mdp_parse_dt_handler(pdev, "qcom,mdss-dspp-off",
		dspp_offsets, ndspp);
	if (rc)
		goto parse_done;

	rc = mdss_mdp_parse_dt_handler(pdev, "qcom,mdss-pingpong-off",
		pingpong_offsets, npingpong);
	if (rc)
		goto parse_done;

	rc = mdss_mdp_mixer_addr_setup(mdata, mixer_offsets,
			dspp_offsets, pingpong_offsets,
			MDSS_MDP_MIXER_TYPE_INTF, mdata->nmixers_intf);
	if (rc)
		goto parse_done;

	rc = mdss_mdp_mixer_addr_setup(mdata, mixer_offsets +
			mdata->nmixers_intf, NULL, NULL,
			MDSS_MDP_MIXER_TYPE_WRITEBACK, mdata->nmixers_wb);
	if (rc)
		goto parse_done;

parse_done:
	kfree(pingpong_offsets);
pingpong_alloc_fail:
	kfree(dspp_offsets);
dspp_alloc_fail:
	kfree(mixer_offsets);

	return rc;
}

static int mdss_mdp_parse_dt_ctl(struct platform_device *pdev)
{
	u32 nwb;
	int rc = 0;
	u32 *ctl_offsets = NULL, *wb_offsets = NULL;

	struct mdss_data_type *mdata = platform_get_drvdata(pdev);

	mdata->nctl = mdss_mdp_parse_dt_prop_len(pdev,
			"qcom,mdss-ctl-off");
	nwb =  mdss_mdp_parse_dt_prop_len(pdev,
			"qcom,mdss-wb-off");

	if (mdata->nctl != nwb) {
		pr_err("device tree err: unequal number of ctl and wb\n");
		rc = -EINVAL;
		goto parse_done;
	}

	ctl_offsets = kzalloc(sizeof(u32) * mdata->nctl, GFP_KERNEL);
	if (!ctl_offsets) {
		pr_err("no more mem for ctl offsets\n");
		return -ENOMEM;
	}

	wb_offsets = kzalloc(sizeof(u32) * nwb, GFP_KERNEL);
	if (!wb_offsets) {
		pr_err("no more mem for writeback offsets\n");
		rc = -ENOMEM;
		goto wb_alloc_fail;
	}

	rc = mdss_mdp_parse_dt_handler(pdev, "qcom,mdss-ctl-off",
		ctl_offsets, mdata->nctl);
	if (rc)
		goto parse_done;

	rc = mdss_mdp_parse_dt_handler(pdev, "qcom,mdss-wb-off",
		wb_offsets, nwb);
	if (rc)
		goto parse_done;

	rc = mdss_mdp_ctl_addr_setup(mdata, ctl_offsets, wb_offsets,
						 mdata->nctl);
	if (rc)
		goto parse_done;

parse_done:
	kfree(wb_offsets);
wb_alloc_fail:
	kfree(ctl_offsets);

	return rc;
}

static int mdss_mdp_parse_dt_video_intf(struct platform_device *pdev)
{
	struct mdss_data_type *mdata = platform_get_drvdata(pdev);
	u32 count;
	u32 *offsets;
	int rc;


	count = mdss_mdp_parse_dt_prop_len(pdev, "qcom,mdss-intf-off");
	if (count == 0)
		return -EINVAL;

	offsets = kzalloc(sizeof(u32) * count, GFP_KERNEL);
	if (!offsets) {
		pr_err("no mem assigned for video intf\n");
		return -ENOMEM;
	}

	rc = mdss_mdp_parse_dt_handler(pdev, "qcom,mdss-intf-off",
			offsets, count);
	if (rc)
		goto parse_fail;

	rc = mdss_mdp_video_addr_setup(mdata, offsets, count);
	if (rc)
		pr_err("unable to setup video interfaces\n");

parse_fail:
	kfree(offsets);

	return rc;
}

static int mdss_mdp_parse_dt_smp(struct platform_device *pdev)
{
	struct mdss_data_type *mdata = platform_get_drvdata(pdev);
	u32 num;
	u32 data[2];
	int rc, len;
	const u32 *arr;

	num = mdss_mdp_parse_dt_prop_len(pdev, "qcom,mdss-smp-data");

	if (num != 2)
		return -EINVAL;

	rc = mdss_mdp_parse_dt_handler(pdev, "qcom,mdss-smp-data", data, num);
	if (rc)
		return rc;

	rc = mdss_mdp_smp_setup(mdata, data[0], data[1]);

	if (rc) {
		pr_err("unable to setup smp data\n");
		return rc;
	}

	rc = of_property_read_u32(pdev->dev.of_node,
		"qcom,mdss-smp-mb-per-pipe", data);
	mdata->smp_mb_per_pipe = (!rc ? data[0] : 0);

	rc = 0;
	arr = of_get_property(pdev->dev.of_node,
		"qcom,mdss-pipe-rgb-fixed-mmb", &len);
	if (arr) {
		int i, j, k;
		u32 cnt, mmb;

		len /= sizeof(u32);
		for (i = 0, k = 0; i < len; k++) {
			struct mdss_mdp_pipe *pipe = NULL;

			if (k >= mdata->nrgb_pipes) {
				pr_err("invalid fixed mmbs for rgb pipes\n");
				return -EINVAL;
			}

			pipe = &mdata->rgb_pipes[k];

			cnt = be32_to_cpu(arr[i++]);
			if (cnt == 0)
				continue;

			for (j = 0; j < cnt; j++) {
				mmb = be32_to_cpu(arr[i++]);
				if (mmb > mdata->smp_mb_cnt) {
					pr_err("overflow mmb%d: rgb%d: max%d\n",
						mmb, k, mdata->smp_mb_cnt);
					return -EINVAL;
				}
				/* rgb pipes fetches only single plane */
				set_bit(mmb, pipe->smp_map[0].fixed);
			}
			if (bitmap_intersects(pipe->smp_map[0].fixed,
				mdata->mmb_alloc_map, mdata->smp_mb_cnt)) {
				pr_err("overlapping fixed mmb map\n");
				return -EINVAL;
			}
			bitmap_or(mdata->mmb_alloc_map, pipe->smp_map[0].fixed,
				mdata->mmb_alloc_map, mdata->smp_mb_cnt);
		}
	}

	return rc;
}

static void mdss_mdp_parse_dt_fudge_factors(struct platform_device *pdev,
	char *prop_name, struct mdss_fudge_factor *ff)
{
	int rc;
	u32 data[2] = {0, 0};

	ff->numer = 1;
	ff->denom = 1;

	rc = mdss_mdp_parse_dt_handler(pdev, prop_name, data, 2);
	if (rc) {
		pr_debug("err reading %s\n", prop_name);
	} else {
		ff->numer = data[0];
		ff->denom = data[1];
	}
}

static int mdss_mdp_parse_dt_misc(struct platform_device *pdev)
{
	struct mdss_data_type *mdata = platform_get_drvdata(pdev);
	u32 data;
	int rc;
	struct property *prop = NULL;

	rc = of_property_read_u32(pdev->dev.of_node, "qcom,mdss-rot-block-size",
		&data);
	mdata->rot_block_size = (!rc ? data : 128);

	mdata->has_bwc = of_property_read_bool(pdev->dev.of_node,
					       "qcom,mdss-has-bwc");
	mdata->has_decimation = of_property_read_bool(pdev->dev.of_node,
		"qcom,mdss-has-decimation");
	mdata->has_wfd_blk = of_property_read_bool(pdev->dev.of_node,
		"qcom,mdss-has-wfd-blk");
	prop = of_find_property(pdev->dev.of_node, "batfet-supply", NULL);
	mdata->batfet_required = prop ? true : false;
	rc = of_property_read_u32(pdev->dev.of_node,
		 "qcom,mdss-highest-bank-bit", &(mdata->highest_bank_bit));
	if (rc)
		pr_debug("Could not read optional property: highest bank bit\n");

	rc = of_property_read_u32(pdev->dev.of_node,
			"qcom,max-bandwidth-low-kbps", &mdata->max_bw_low);
	if (rc)
		pr_debug("max bandwidth (low) property not specified\n");

	rc = of_property_read_u32(pdev->dev.of_node,
			"qcom,max-bandwidth-high-kbps", &mdata->max_bw_high);
	if (rc)
		pr_debug("max bandwidth (high) property not specified\n");

	mdss_mdp_parse_dt_fudge_factors(pdev, "qcom,mdss-ab-factor",
		&mdata->ab_factor);
	mdss_mdp_parse_dt_fudge_factors(pdev, "qcom,mdss-ib-factor",
		&mdata->ib_factor);
	mdss_mdp_parse_dt_fudge_factors(pdev, "qcom,mdss-high-ib-factor",
		&mdata->high_ib_factor);
	mdss_mdp_parse_dt_fudge_factors(pdev, "qcom,mdss-clk-factor",
		&mdata->clk_factor);

	return 0;
}

static int mdss_mdp_parse_dt_ad_cfg(struct platform_device *pdev)
{
	struct mdss_data_type *mdata = platform_get_drvdata(pdev);
	u32 *ad_offsets = NULL;
	int rc;

	mdata->nad_cfgs = mdss_mdp_parse_dt_prop_len(pdev, "qcom,mdss-ad-off");

	if (mdata->nad_cfgs == 0) {
		mdata->ad_cfgs = NULL;
		return 0;
	}

	if (mdata->nad_cfgs > mdata->nmixers_intf)
		return -EINVAL;


	mdata->has_wb_ad = of_property_read_bool(pdev->dev.of_node,
		"qcom,mdss-has-wb-ad");

	ad_offsets = kzalloc(sizeof(u32) * mdata->nad_cfgs, GFP_KERNEL);
	if (!ad_offsets) {
		pr_err("no mem assigned: kzalloc fail\n");
		return -ENOMEM;
	}

	rc = mdss_mdp_parse_dt_handler(pdev, "qcom,mdss-ad-off", ad_offsets,
					mdata->nad_cfgs);
	if (rc)
		goto parse_done;

	rc = mdss_mdp_ad_addr_setup(mdata, ad_offsets);
	if (rc)
		pr_err("unable to setup assertive display\n");

parse_done:
	kfree(ad_offsets);
	return rc;
}

static int mdss_mdp_parse_dt_bus_scale(struct platform_device *pdev)
{
	int rc;
	struct mdss_data_type *mdata = platform_get_drvdata(pdev);

	rc = of_property_read_u32(pdev->dev.of_node, "qcom,msm-bus,num-paths",
		&mdata->axi_port_cnt);
	if (rc) {
		pr_err("Error. qcom,msm-bus,num-paths prop not found.rc=%d\n",
			rc);
		return rc;
	}

	mdata->bus_scale_table = msm_bus_cl_get_pdata(pdev);
	if (IS_ERR_OR_NULL(mdata->bus_scale_table)) {
		rc = PTR_ERR(mdata->bus_scale_table);
		if (!rc)
			rc = -EINVAL;
		pr_err("msm_bus_cl_get_pdata failed. rc=%d\n", rc);
		mdata->bus_scale_table = NULL;
	}

	return rc;
}

static int mdss_mdp_parse_dt_handler(struct platform_device *pdev,
		char *prop_name, u32 *offsets, int len)
{
	int rc;
	rc = of_property_read_u32_array(pdev->dev.of_node, prop_name,
					offsets, len);
	if (rc) {
		pr_err("Error from prop %s : u32 array read\n", prop_name);
		return -EINVAL;
	}

	return 0;
}

static int mdss_mdp_parse_dt_prop_len(struct platform_device *pdev,
				      char *prop_name)
{
	int len = 0;

	of_find_property(pdev->dev.of_node, prop_name, &len);

	if (len < 1) {
		pr_err("Error from prop %s : spec error in device tree\n",
		       prop_name);
		return 0;
	}

	len = len/sizeof(u32);

	return len;
}

struct mdss_data_type *mdss_mdp_get_mdata(void)
{
	return mdss_res;
}

/**
 * mdss_is_ready() - checks if mdss is probed and ready
 *
 * Checks if mdss resources have been initialized
 *
 * returns true if mdss is ready, else returns false
 */
bool mdss_is_ready(void)
{
	return mdss_mdp_get_mdata() ? true : false;
}
EXPORT_SYMBOL(mdss_mdp_get_mdata);

/**
 * mdss_panel_intf_type() - checks if a given intf type is primary
 * @intf_val: panel interface type of the individual controller
 *
 * Individual controller queries with MDP to check if it is
 * configured as the primary interface.
 *
 * returns a pointer to the configured structure mdss_panel_cfg
 * to the controller that's configured as the primary panel interface.
 * returns NULL on error or if @intf_val is not the configured
 * controller.
 */
struct mdss_panel_cfg *mdss_panel_intf_type(int intf_val)
{
	if (!mdss_res || !mdss_res->pan_cfg.init_done)
		return ERR_PTR(-EPROBE_DEFER);

	if (mdss_res->pan_cfg.pan_intf == intf_val)
		return &mdss_res->pan_cfg;
	else
		return NULL;
}
EXPORT_SYMBOL(mdss_panel_intf_type);

int mdss_panel_get_boot_cfg(void)
{
	int rc;

	if (!mdss_res || !mdss_res->pan_cfg.init_done)
		return -EPROBE_DEFER;
	if (mdss_res->pan_cfg.lk_cfg)
		rc = 1;
	else
		rc = 0;
	return rc;
}

static int mdss_mdp_cx_ctrl(struct mdss_data_type *mdata, int enable)
{
	int rc = 0;

	if (!mdata->vdd_cx)
		return rc;

	if (enable) {
		rc = regulator_set_voltage(
				mdata->vdd_cx,
				RPM_REGULATOR_CORNER_SVS_SOC,
				RPM_REGULATOR_CORNER_SUPER_TURBO);
		if (rc < 0)
			goto vreg_set_voltage_fail;

		pr_debug("Enabling CX power rail\n");
		rc = regulator_enable(mdata->vdd_cx);
		if (rc) {
			pr_err("Failed to enable regulator.\n");
			return rc;
		}
	} else {
		pr_debug("Disabling CX power rail\n");
		rc = regulator_disable(mdata->vdd_cx);
		if (rc) {
			pr_err("Failed to disable regulator.\n");
			return rc;
		}
		rc = regulator_set_voltage(
				mdata->vdd_cx,
				RPM_REGULATOR_CORNER_NONE,
				RPM_REGULATOR_CORNER_SUPER_TURBO);
		if (rc < 0)
			goto vreg_set_voltage_fail;
	}

	return rc;

vreg_set_voltage_fail:
	pr_err("Set vltg fail\n");
	return rc;
}

void mdss_mdp_batfet_ctrl(struct mdss_data_type *mdata, int enable)
{
	if (!mdata->batfet_required)
		return;

	if (!mdata->batfet) {
		if (enable) {
			mdata->batfet = devm_regulator_get(&mdata->pdev->dev,
				"batfet");
			if (IS_ERR_OR_NULL(mdata->batfet)) {
				pr_debug("unable to get batfet reg. rc=%d\n",
					PTR_RET(mdata->batfet));
				mdata->batfet = NULL;
				return;
			}
		} else {
			pr_debug("Batfet regulator disable w/o enable\n");
			return;
		}
	}

	if (enable)
		regulator_enable(mdata->batfet);
	else
		regulator_disable(mdata->batfet);
}

static void mdss_mdp_footswitch_ctrl(struct mdss_data_type *mdata, int on)
{
	if (!mdata->fs)
		return;

	if (on) {
		pr_debug("Enable MDP FS\n");
		if (!mdata->fs_ena) {
			regulator_enable(mdata->fs);
			mdss_mdp_cx_ctrl(mdata, true);
			mdss_mdp_batfet_ctrl(mdata, true);
		}
		mdata->fs_ena = true;
	} else {
		pr_debug("Disable MDP FS\n");
		mdss_iommu_dettach(mdata);
		if (mdata->fs_ena) {
			regulator_disable(mdata->fs);
			mdss_mdp_cx_ctrl(mdata, false);
			mdss_mdp_batfet_ctrl(mdata, false);
		}
		mdata->fs_ena = false;
	}
}

static inline int mdss_mdp_suspend_sub(struct mdss_data_type *mdata)
{
	mdata->suspend_fs_ena = mdata->fs_ena;
	mdss_mdp_footswitch_ctrl(mdata, false);

	pr_debug("suspend done fs=%d\n", mdata->suspend_fs_ena);

	return 0;
}

static inline int mdss_mdp_resume_sub(struct mdss_data_type *mdata)
{
	if (mdata->suspend_fs_ena)
		mdss_mdp_footswitch_ctrl(mdata, true);

	pr_debug("resume done fs=%d\n", mdata->suspend_fs_ena);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int mdss_mdp_pm_suspend(struct device *dev)
{
	struct mdss_data_type *mdata;

	mdata = dev_get_drvdata(dev);
	if (!mdata)
		return -ENODEV;

	dev_dbg(dev, "display pm suspend\n");

	return mdss_mdp_suspend_sub(mdata);
}

static int mdss_mdp_pm_resume(struct device *dev)
{
	struct mdss_data_type *mdata;

	mdata = dev_get_drvdata(dev);
	if (!mdata)
		return -ENODEV;

	dev_dbg(dev, "display pm resume\n");

	return mdss_mdp_resume_sub(mdata);
}
#endif

#if defined(CONFIG_PM) && !defined(CONFIG_PM_SLEEP)
static int mdss_mdp_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct mdss_data_type *mdata = platform_get_drvdata(pdev);

	if (!mdata)
		return -ENODEV;

	dev_dbg(&pdev->dev, "display suspend\n");

	return mdss_mdp_suspend_sub(mdata);
}

static int mdss_mdp_resume(struct platform_device *pdev)
{
	struct mdss_data_type *mdata = platform_get_drvdata(pdev);

	if (!mdata)
		return -ENODEV;

	dev_dbg(&pdev->dev, "display resume\n");

	return mdss_mdp_resume_sub(mdata);
}
#else
#define mdss_mdp_suspend NULL
#define mdss_mdp_resume NULL
#endif

#ifdef CONFIG_PM_RUNTIME
static int mdss_mdp_runtime_resume(struct device *dev)
{
	struct mdss_data_type *mdata = dev_get_drvdata(dev);
	bool device_on = true;
	if (!mdata)
		return -ENODEV;

	dev_dbg(dev, "pm_runtime: resuming...\n");
	device_for_each_child(dev, &device_on, mdss_fb_suspres_panel);
	mdss_mdp_footswitch_ctrl(mdata, true);

	return 0;
}

static int mdss_mdp_runtime_idle(struct device *dev)
{
	struct mdss_data_type *mdata = dev_get_drvdata(dev);
	if (!mdata)
		return -ENODEV;

	dev_dbg(dev, "pm_runtime: idling...\n");

	return 0;
}

static int mdss_mdp_runtime_suspend(struct device *dev)
{
	struct mdss_data_type *mdata = dev_get_drvdata(dev);
	bool device_on = false;
	if (!mdata)
		return -ENODEV;
	dev_dbg(dev, "pm_runtime: suspending...\n");

	if (mdata->clk_ena) {
		pr_err("MDP suspend failed\n");
		return -EBUSY;
	}
	device_for_each_child(dev, &device_on, mdss_fb_suspres_panel);
	mdss_mdp_footswitch_ctrl(mdata, false);

	return 0;
}
#endif

static const struct dev_pm_ops mdss_mdp_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mdss_mdp_pm_suspend, mdss_mdp_pm_resume)
	SET_RUNTIME_PM_OPS(mdss_mdp_runtime_suspend,
			mdss_mdp_runtime_resume,
			mdss_mdp_runtime_idle)
};

static int mdss_mdp_remove(struct platform_device *pdev)
{
	struct mdss_data_type *mdata = platform_get_drvdata(pdev);
	if (!mdata)
		return -ENODEV;
	pm_runtime_disable(&pdev->dev);
	mdss_mdp_pp_term(&pdev->dev);
	mdss_mdp_bus_scale_unregister(mdata);
	mdss_debugfs_remove(mdata);
	return 0;
}

static const struct of_device_id mdss_mdp_dt_match[] = {
	{ .compatible = "qcom,mdss_mdp",},
	{}
};
MODULE_DEVICE_TABLE(of, mdss_mdp_dt_match);

static struct platform_driver mdss_mdp_driver = {
	.probe = mdss_mdp_probe,
	.remove = mdss_mdp_remove,
	.suspend = mdss_mdp_suspend,
	.resume = mdss_mdp_resume,
	.shutdown = NULL,
	.driver = {
		/*
		 * Driver name must match the device name added in
		 * platform.c.
		 */
		.name = "mdp",
		.of_match_table = mdss_mdp_dt_match,
		.pm = &mdss_mdp_pm_ops,
	},
};

static int mdss_mdp_register_driver(void)
{
	return platform_driver_register(&mdss_mdp_driver);
}

static int __init mdss_mdp_driver_init(void)
{
	int ret;

	ret = mdss_mdp_register_driver();
	if (ret) {
		pr_err("mdp_register_driver() failed!\n");
		return ret;
	}

	return 0;

}

module_init(mdss_mdp_driver_init);
