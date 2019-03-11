/* Copyright (c) 2013-2019, The Linux Foundation. All rights reserved.
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
#include <linux/dma-buf.h>
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
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/uaccess.h>
#include <linux/file.h>
#include <linux/msm_kgsl.h>
#include <linux/major.h>
#include <linux/bootmem.h>
#include <linux/memblock.h>
#include <linux/iopoll.h>
#include <linux/clk/msm-clk.h>
#include <linux/regulator/rpm-smd-regulator.h>

#include <linux/msm-bus.h>
#include <linux/msm-bus-board.h>
#include <linux/vmalloc.h>

#include <linux/msm_dma_iommu_mapping.h>

#include "mdp3.h"
#include "mdss_fb.h"
#include "mdp3_hwio.h"
#include "mdp3_ctrl.h"
#include "mdp3_ppp.h"
#include "mdss_debug.h"
#include "mdss_smmu.h"
#include "mdss.h"
#include "mdss_spi_panel.h"

#ifndef EXPORT_COMPAT
#define EXPORT_COMPAT(x)
#endif

#define AUTOSUSPEND_TIMEOUT_MS	100
#define MISR_POLL_SLEEP                 2000
#define MISR_POLL_TIMEOUT               32000
#define MDP3_REG_CAPTURED_DSI_PCLK_MASK 1

#define MDP_CORE_HW_VERSION	0x03050306
struct mdp3_hw_resource *mdp3_res;

#define MDP_BUS_VECTOR_ENTRY(ab_val, ib_val)		\
	{						\
		.src = MSM_BUS_MASTER_MDP_PORT0,	\
		.dst = MSM_BUS_SLAVE_EBI_CH0,		\
		.ab = (ab_val),				\
		.ib = (ib_val),				\
	}

#define SET_BIT(value, bit_num) \
{ \
	value[bit_num >> 3] |= (1 << (bit_num & 7)); \
}

#define MAX_BPP_SUPPORTED 4

static struct msm_bus_vectors mdp_bus_vectors[] = {
	MDP_BUS_VECTOR_ENTRY(0, 0),
	MDP_BUS_VECTOR_ENTRY(SZ_128M, SZ_256M),
	MDP_BUS_VECTOR_ENTRY(SZ_256M, SZ_512M),
};
static struct msm_bus_paths
	mdp_bus_usecases[ARRAY_SIZE(mdp_bus_vectors)];
static struct msm_bus_scale_pdata mdp_bus_scale_table = {
	.usecase = mdp_bus_usecases,
	.num_usecases = ARRAY_SIZE(mdp_bus_usecases),
	.name = "mdp3",
};

struct mdp3_bus_handle_map mdp3_bus_handle[MDP3_BUS_HANDLE_MAX] = {
	[MDP3_BUS_HANDLE] = {
		.bus_vector = mdp_bus_vectors,
		.usecases = mdp_bus_usecases,
		.scale_pdata = &mdp_bus_scale_table,
		.current_bus_idx = 0,
		.handle = 0,
	},
};

static struct mdss_panel_intf pan_types[] = {
	{"dsi", MDSS_PANEL_INTF_DSI},
	{"spi", MDSS_PANEL_INTF_SPI},
};
static char mdss_mdp3_panel[MDSS_MAX_PANEL_LEN];

struct mdp3_iommu_domain_map mdp3_iommu_domains[MDP3_IOMMU_DOMAIN_MAX] = {
	[MDP3_IOMMU_DOMAIN_UNSECURE] = {
		.domain_type = MDP3_IOMMU_DOMAIN_UNSECURE,
		.client_name = "mdp_ns",
		.npartitions = 1,
		.domain_idx = MDP3_IOMMU_DOMAIN_UNSECURE,
	},
	[MDP3_IOMMU_DOMAIN_SECURE] = {
		.domain_type = MDP3_IOMMU_DOMAIN_SECURE,
		.client_name = "mdp_secure",
		.npartitions = 1,
		.domain_idx = MDP3_IOMMU_DOMAIN_SECURE,
	},
};

#ifndef CONFIG_FB_MSM_MDSS_SPI_PANEL
void mdss_spi_panel_bl_ctrl_update(struct mdss_panel_data *pdata, u32 bl_level)
{

}
#endif

static irqreturn_t mdp3_irq_handler(int irq, void *ptr)
{
	int i = 0;
	struct mdp3_hw_resource *mdata = (struct mdp3_hw_resource *)ptr;
	u32 mdp_interrupt = 0;
	u32 mdp_status = 0;

	spin_lock(&mdata->irq_lock);
	if (!mdata->irq_mask) {
		pr_err("spurious interrupt\n");
		spin_unlock(&mdata->irq_lock);
		return IRQ_HANDLED;
	}
	mdp_status = MDP3_REG_READ(MDP3_REG_INTR_STATUS);
	mdp_interrupt = mdp_status;
	pr_debug("mdp3_irq_handler irq=%d\n", mdp_interrupt);

	mdp_interrupt &= mdata->irq_mask;

	while (mdp_interrupt && i < MDP3_MAX_INTR) {
		if ((mdp_interrupt & 0x1) && mdata->callbacks[i].cb)
			mdata->callbacks[i].cb(i, mdata->callbacks[i].data);
		mdp_interrupt = mdp_interrupt >> 1;
		i++;
	}
	MDP3_REG_WRITE(MDP3_REG_INTR_CLEAR, mdp_status);

	spin_unlock(&mdata->irq_lock);

	return IRQ_HANDLED;
}

void mdp3_irq_enable(int type)
{
	unsigned long flag;

	pr_debug("mdp3_irq_enable type=%d\n", type);
	spin_lock_irqsave(&mdp3_res->irq_lock, flag);
	if (mdp3_res->irq_ref_count[type] > 0) {
		pr_debug("interrupt %d already enabled\n", type);
		spin_unlock_irqrestore(&mdp3_res->irq_lock, flag);
		return;
	}

	mdp3_res->irq_mask |= BIT(type);
	MDP3_REG_WRITE(MDP3_REG_INTR_ENABLE, mdp3_res->irq_mask);

	mdp3_res->irq_ref_count[type] += 1;
	spin_unlock_irqrestore(&mdp3_res->irq_lock, flag);
}

void mdp3_irq_disable(int type)
{
	unsigned long flag;

	spin_lock_irqsave(&mdp3_res->irq_lock, flag);
	mdp3_irq_disable_nosync(type);
	spin_unlock_irqrestore(&mdp3_res->irq_lock, flag);
}

void mdp3_irq_disable_nosync(int type)
{
	if (mdp3_res->irq_ref_count[type] <= 0) {
		pr_debug("interrupt %d not enabled\n", type);
		return;
	}
	mdp3_res->irq_ref_count[type] -= 1;
	if (mdp3_res->irq_ref_count[type] == 0) {
		mdp3_res->irq_mask &= ~BIT(type);
		MDP3_REG_WRITE(MDP3_REG_INTR_ENABLE, mdp3_res->irq_mask);
	}
}

int mdp3_set_intr_callback(u32 type, struct mdp3_intr_cb *cb)
{
	unsigned long flag;

	pr_debug("interrupt %d callback\n", type);
	spin_lock_irqsave(&mdp3_res->irq_lock, flag);
	if (cb)
		mdp3_res->callbacks[type] = *cb;
	else
		mdp3_res->callbacks[type].cb = NULL;

	spin_unlock_irqrestore(&mdp3_res->irq_lock, flag);
	return 0;
}

void mdp3_irq_register(void)
{
	unsigned long flag;
	struct mdss_hw *mdp3_hw;

	pr_debug("mdp3_irq_register\n");
	mdp3_hw = &mdp3_res->mdp3_hw;
	spin_lock_irqsave(&mdp3_res->irq_lock, flag);
	mdp3_res->irq_ref_cnt++;
	if (mdp3_res->irq_ref_cnt == 1) {
		MDP3_REG_WRITE(MDP3_REG_INTR_ENABLE, mdp3_res->irq_mask);
		mdp3_res->mdss_util->enable_irq(&mdp3_res->mdp3_hw);
	}
	spin_unlock_irqrestore(&mdp3_res->irq_lock, flag);
}

void mdp3_irq_deregister(void)
{
	unsigned long flag;
	bool irq_enabled = true;
	struct mdss_hw *mdp3_hw;

	pr_debug("mdp3_irq_deregister\n");
	mdp3_hw = &mdp3_res->mdp3_hw;
	spin_lock_irqsave(&mdp3_res->irq_lock, flag);
	memset(mdp3_res->irq_ref_count, 0, sizeof(u32) * MDP3_MAX_INTR);
	mdp3_res->irq_mask = 0;
	MDP3_REG_WRITE(MDP3_REG_INTR_ENABLE, 0);
	mdp3_res->irq_ref_cnt--;
	/* This can happen if suspend is called first */
	if (mdp3_res->irq_ref_cnt < 0) {
		irq_enabled = false;
		mdp3_res->irq_ref_cnt = 0;
	}
	if (mdp3_res->irq_ref_cnt == 0 && irq_enabled)
		mdp3_res->mdss_util->disable_irq_nosync(&mdp3_res->mdp3_hw);
	spin_unlock_irqrestore(&mdp3_res->irq_lock, flag);
}

void mdp3_irq_suspend(void)
{
	unsigned long flag;
	bool irq_enabled = true;
	struct mdss_hw *mdp3_hw;

	pr_debug("%s\n", __func__);
	mdp3_hw = &mdp3_res->mdp3_hw;
	spin_lock_irqsave(&mdp3_res->irq_lock, flag);
	mdp3_res->irq_ref_cnt--;
	if (mdp3_res->irq_ref_cnt < 0) {
		irq_enabled = false;
		mdp3_res->irq_ref_cnt = 0;
	}
	if (mdp3_res->irq_ref_cnt == 0 && irq_enabled) {
		MDP3_REG_WRITE(MDP3_REG_INTR_ENABLE, 0);
		mdp3_res->mdss_util->disable_irq_nosync(&mdp3_res->mdp3_hw);
	}
	spin_unlock_irqrestore(&mdp3_res->irq_lock, flag);
}

static int mdp3_bus_scale_register(void)
{
	int i, j;

	if (!mdp3_res->bus_handle) {
		pr_err("No bus handle\n");
		return -EINVAL;
	}
	for (i = 0; i < MDP3_BUS_HANDLE_MAX; i++) {
		struct mdp3_bus_handle_map *bus_handle =
			&mdp3_res->bus_handle[i];

		if (!bus_handle->handle) {
			int j;
			struct msm_bus_scale_pdata *bus_pdata =
				bus_handle->scale_pdata;

			for (j = 0; j < bus_pdata->num_usecases; j++) {
				bus_handle->usecases[j].num_paths = 1;
				bus_handle->usecases[j].vectors =
					&bus_handle->bus_vector[j];
			}

			bus_handle->handle =
				msm_bus_scale_register_client(bus_pdata);
			if (!bus_handle->handle) {
				pr_err("not able to get bus scale i=%d\n", i);
				return -ENOMEM;
			}
			pr_debug("register bus_hdl=%x\n",
				bus_handle->handle);
		}

		for (j = 0; j < MDP3_CLIENT_MAX; j++) {
			bus_handle->ab[j] = 0;
			bus_handle->ib[j] = 0;
		}
	}
	return 0;
}

static void mdp3_bus_scale_unregister(void)
{
	int i;

	if (!mdp3_res->bus_handle)
		return;

	for (i = 0; i < MDP3_BUS_HANDLE_MAX; i++) {
		pr_debug("unregister index=%d bus_handle=%x\n",
			i, mdp3_res->bus_handle[i].handle);
		if (mdp3_res->bus_handle[i].handle) {
			msm_bus_scale_unregister_client(
				mdp3_res->bus_handle[i].handle);
			mdp3_res->bus_handle[i].handle = 0;
		}
	}
}

int mdp3_bus_scale_set_quota(int client, u64 ab_quota, u64 ib_quota)
{
	struct mdp3_bus_handle_map *bus_handle;
	int cur_bus_idx;
	int bus_idx;
	int client_idx;
	u64 total_ib = 0, total_ab = 0;
	int i, rc;

	client_idx  = MDP3_BUS_HANDLE;

	bus_handle = &mdp3_res->bus_handle[client_idx];
	cur_bus_idx = bus_handle->current_bus_idx;

	if (bus_handle->handle < 1) {
		pr_err("invalid bus handle %d\n", bus_handle->handle);
		return -EINVAL;
	}

	bus_handle->ab[client] = ab_quota;
	bus_handle->ib[client] = ib_quota;

	for (i = 0; i < MDP3_CLIENT_MAX; i++) {
		total_ab += bus_handle->ab[i];
		total_ib += bus_handle->ib[i];
	}

	if ((total_ab | total_ib) == 0) {
		bus_idx = 0;
	} else {
		int num_cases = bus_handle->scale_pdata->num_usecases;
		struct msm_bus_vectors *vect = NULL;

		bus_idx = (cur_bus_idx % (num_cases - 1)) + 1;

		/* aligning to avoid performing updates for small changes */
		total_ab = ALIGN(total_ab, SZ_64M);
		total_ib = ALIGN(total_ib, SZ_64M);

		vect = bus_handle->scale_pdata->usecase[cur_bus_idx].vectors;
		if ((total_ab == vect->ab) && (total_ib == vect->ib)) {
			pr_debug("skip bus scaling, no change in vectors\n");
			return 0;
		}

		vect = bus_handle->scale_pdata->usecase[bus_idx].vectors;
		vect->ab = total_ab;
		vect->ib = total_ib;

		pr_debug("bus scale idx=%d ab=%llu ib=%llu\n", bus_idx,
				vect->ab, vect->ib);
	}
	bus_handle->current_bus_idx = bus_idx;
	rc = msm_bus_scale_client_update_request(bus_handle->handle, bus_idx);

	if (!rc && ab_quota != 0 && ib_quota != 0) {
		bus_handle->restore_ab[client] = ab_quota;
		bus_handle->restore_ib[client] = ib_quota;
	}

	return rc;
}

static int mdp3_clk_update(u32 clk_idx, u32 enable)
{
	int ret = 0;
	struct clk *clk;
	int count = 0;

	if (clk_idx >= MDP3_MAX_CLK || !mdp3_res->clocks[clk_idx])
		return -ENODEV;

	clk = mdp3_res->clocks[clk_idx];

	if (enable)
		mdp3_res->clock_ref_count[clk_idx]++;
	else
		mdp3_res->clock_ref_count[clk_idx]--;

	count = mdp3_res->clock_ref_count[clk_idx];
	if (count == 1 && enable) {
		pr_debug("clk=%d en=%d\n", clk_idx, enable);
		ret = clk_prepare(clk);
		if (ret) {
			pr_err("%s: Failed to prepare clock %d",
						__func__, clk_idx);
			mdp3_res->clock_ref_count[clk_idx]--;
			return ret;
		}
		if (clk_idx == MDP3_CLK_MDP_CORE)
			MDSS_XLOG(enable);
		ret = clk_enable(clk);
		if (ret)
			pr_err("%s: clock enable failed %d\n", __func__,
					clk_idx);
	} else if (count == 0) {
		pr_debug("clk=%d disable\n", clk_idx);
		if (clk_idx == MDP3_CLK_MDP_CORE)
			MDSS_XLOG(enable);
		clk_disable(clk);
		clk_unprepare(clk);
		ret = 0;
	} else if (count < 0) {
		pr_err("clk=%d count=%d\n", clk_idx, count);
		ret = -EINVAL;
	}
	return ret;
}



int mdp3_clk_set_rate(int clk_type, unsigned long clk_rate,
			int client)
{
	int ret = 0;
	unsigned long rounded_rate;
	struct clk *clk = mdp3_res->clocks[clk_type];

	if (clk) {
		mutex_lock(&mdp3_res->res_mutex);
		rounded_rate = clk_round_rate(clk, clk_rate);
		if (IS_ERR_VALUE(rounded_rate)) {
			pr_err("unable to round rate err=%ld\n", rounded_rate);
			mutex_unlock(&mdp3_res->res_mutex);
			return -EINVAL;
		}
		if (clk_type == MDP3_CLK_MDP_SRC) {
			if (client == MDP3_CLIENT_DMA_P) {
				mdp3_res->dma_core_clk_request = rounded_rate;
			} else if (client == MDP3_CLIENT_PPP) {
				mdp3_res->ppp_core_clk_request = rounded_rate;
			} else {
				pr_err("unrecognized client=%d\n", client);
				mutex_unlock(&mdp3_res->res_mutex);
				return -EINVAL;
			}
			rounded_rate = max(mdp3_res->dma_core_clk_request,
				mdp3_res->ppp_core_clk_request);
		}
		if (rounded_rate != clk_get_rate(clk)) {
			ret = clk_set_rate(clk, rounded_rate);
			if (ret)
				pr_err("clk_set_rate failed ret=%d\n", ret);
			else
				pr_debug("mdp clk rate=%lu, client = %d\n",
					rounded_rate, client);
		}
		mutex_unlock(&mdp3_res->res_mutex);
	} else {
		pr_err("mdp src clk not setup properly\n");
		ret = -EINVAL;
	}
	return ret;
}

unsigned long mdp3_get_clk_rate(u32 clk_idx)
{
	unsigned long clk_rate = 0;
	struct clk *clk;

	if (clk_idx >= MDP3_MAX_CLK)
		return -ENODEV;

	clk = mdp3_res->clocks[clk_idx];

	if (clk) {
		mutex_lock(&mdp3_res->res_mutex);
		clk_rate = clk_get_rate(clk);
		mutex_unlock(&mdp3_res->res_mutex);
	}
	return clk_rate;
}

static int mdp3_clk_register(char *clk_name, int clk_idx)
{
	struct clk *tmp;

	if (clk_idx >= MDP3_MAX_CLK) {
		pr_err("invalid clk index %d\n", clk_idx);
		return -EINVAL;
	}

	tmp = devm_clk_get(&mdp3_res->pdev->dev, clk_name);
	if (IS_ERR(tmp)) {
		pr_err("unable to get clk: %s\n", clk_name);
		return PTR_ERR(tmp);
	}

	mdp3_res->clocks[clk_idx] = tmp;

	return 0;
}

static int mdp3_clk_setup(void)
{
	int rc;

	rc = mdp3_clk_register("iface_clk", MDP3_CLK_AHB);
	if (rc)
		return rc;

	rc = mdp3_clk_register("bus_clk", MDP3_CLK_AXI);
	if (rc)
		return rc;

	rc = mdp3_clk_register("core_clk_src", MDP3_CLK_MDP_SRC);
	if (rc)
		return rc;

	rc = mdp3_clk_register("core_clk", MDP3_CLK_MDP_CORE);
	if (rc)
		return rc;

	rc = mdp3_clk_register("vsync_clk", MDP3_CLK_VSYNC);
	if (rc)
		return rc;

	rc = mdp3_clk_set_rate(MDP3_CLK_MDP_SRC, MDP_CORE_CLK_RATE_SVS,
			MDP3_CLIENT_DMA_P);
	if (rc)
		pr_err("%s: Error setting max clock during probe\n", __func__);
	return rc;
}

static void mdp3_clk_remove(void)
{
	if (!IS_ERR_OR_NULL(mdp3_res->clocks[MDP3_CLK_AHB]))
		clk_put(mdp3_res->clocks[MDP3_CLK_AHB]);

	if (!IS_ERR_OR_NULL(mdp3_res->clocks[MDP3_CLK_AXI]))
		clk_put(mdp3_res->clocks[MDP3_CLK_AXI]);

	if (!IS_ERR_OR_NULL(mdp3_res->clocks[MDP3_CLK_MDP_SRC]))
		clk_put(mdp3_res->clocks[MDP3_CLK_MDP_SRC]);

	if (!IS_ERR_OR_NULL(mdp3_res->clocks[MDP3_CLK_MDP_CORE]))
		clk_put(mdp3_res->clocks[MDP3_CLK_MDP_CORE]);

	if (!IS_ERR_OR_NULL(mdp3_res->clocks[MDP3_CLK_VSYNC]))
		clk_put(mdp3_res->clocks[MDP3_CLK_VSYNC]);

}

u64 mdp3_clk_round_off(u64 clk_rate)
{
	u64 clk_round_off = 0;

	if (clk_rate <= MDP_CORE_CLK_RATE_SVS)
		clk_round_off = MDP_CORE_CLK_RATE_SVS;
	else if (clk_rate <= MDP_CORE_CLK_RATE_SUPER_SVS)
		clk_round_off = MDP_CORE_CLK_RATE_SUPER_SVS;
	else
		clk_round_off = MDP_CORE_CLK_RATE_MAX;

	pr_debug("clk = %llu rounded to = %llu\n",
		clk_rate, clk_round_off);
	return clk_round_off;
}

int mdp3_clk_enable(int enable, int dsi_clk)
{
	int rc = 0;
	int changed = 0;

	pr_debug("MDP CLKS %s\n", (enable ? "Enable" : "Disable"));

	mutex_lock(&mdp3_res->res_mutex);

	if (enable) {
		if (mdp3_res->clk_ena == 0)
			changed++;
		mdp3_res->clk_ena++;
	} else {
		if (mdp3_res->clk_ena) {
			mdp3_res->clk_ena--;
			if (mdp3_res->clk_ena == 0)
				changed++;
		} else {
			pr_err("Can not be turned off\n");
		}
	}
	pr_debug("%s: clk_ena=%d changed=%d enable=%d\n",
		__func__, mdp3_res->clk_ena, changed, enable);

	if (changed) {
		if (enable)
			pm_runtime_get_sync(&mdp3_res->pdev->dev);

	rc = mdp3_clk_update(MDP3_CLK_AHB, enable);
	rc |= mdp3_clk_update(MDP3_CLK_AXI, enable);
	rc |= mdp3_clk_update(MDP3_CLK_MDP_SRC, enable);
	rc |= mdp3_clk_update(MDP3_CLK_MDP_CORE, enable);
	rc |= mdp3_clk_update(MDP3_CLK_VSYNC, enable);

		if (!enable) {
			pm_runtime_mark_last_busy(&mdp3_res->pdev->dev);
			pm_runtime_put_autosuspend(&mdp3_res->pdev->dev);
		}
	}

	mutex_unlock(&mdp3_res->res_mutex);
	return rc;
}

void mdp3_bus_bw_iommu_enable(int enable, int client)
{
	struct mdp3_bus_handle_map *bus_handle;
	int client_idx;
	u64 ab = 0, ib = 0;
	int ref_cnt;

	client_idx  = MDP3_BUS_HANDLE;

	bus_handle = &mdp3_res->bus_handle[client_idx];
	if (bus_handle->handle < 1) {
		pr_err("invalid bus handle %d\n", bus_handle->handle);
		return;
	}
	mutex_lock(&mdp3_res->res_mutex);
	if (enable)
		bus_handle->ref_cnt++;
	else
		if (bus_handle->ref_cnt)
			bus_handle->ref_cnt--;
	ref_cnt = bus_handle->ref_cnt;
	mutex_unlock(&mdp3_res->res_mutex);

	if (enable) {
		if (mdp3_res->allow_iommu_update)
			mdp3_iommu_enable(client);
		if (ref_cnt == 1) {
			pm_runtime_get_sync(&mdp3_res->pdev->dev);
			ab = bus_handle->restore_ab[client];
			ib = bus_handle->restore_ib[client];
		mdp3_bus_scale_set_quota(client, ab, ib);
		}
	} else {
		if (ref_cnt == 0) {
			mdp3_bus_scale_set_quota(client, 0, 0);
			pm_runtime_mark_last_busy(&mdp3_res->pdev->dev);
			pm_runtime_put_autosuspend(&mdp3_res->pdev->dev);
		}
		mdp3_iommu_disable(client);
	}

	if (ref_cnt < 0) {
		pr_err("Ref count < 0, bus client=%d, ref_cnt=%d",
				client_idx, ref_cnt);
	}
}

void mdp3_calc_dma_res(struct mdss_panel_info *panel_info, u64 *clk_rate,
		u64 *ab, u64 *ib, uint32_t bpp)
{
	u32 vtotal = mdss_panel_get_vtotal(panel_info);
	u32 htotal = mdss_panel_get_htotal(panel_info, 0);
	u64 clk    = htotal * vtotal * panel_info->mipi.frame_rate;

	pr_debug("clk_rate for dma = %llu, bpp = %d\n", clk, bpp);
	if (clk_rate)
		*clk_rate = mdp3_clk_round_off(clk);

	/* ab and ib vote should be same for honest voting */
	if (ab || ib) {
		*ab = clk * bpp;
		*ib = *ab;
	}
}

int mdp3_res_update(int enable, int dsi_clk, int client)
{
	int rc = 0;

	if (enable) {
		rc = mdp3_clk_enable(enable, dsi_clk);
		if (rc < 0) {
			pr_err("mdp3_clk_enable failed, enable=%d, dsi_clk=%d\n",
				enable, dsi_clk);
			goto done;
		}
		mdp3_irq_register();
		mdp3_bus_bw_iommu_enable(enable, client);
	} else {
		mdp3_bus_bw_iommu_enable(enable, client);
		mdp3_irq_suspend();
		rc = mdp3_clk_enable(enable, dsi_clk);
		if (rc < 0) {
			pr_err("mdp3_clk_enable failed, enable=%d, dsi_clk=%d\n",
				enable, dsi_clk);
			goto done;
		}
	}

done:
	return rc;
}

int mdp3_get_mdp_dsi_clk(void)
{
	int rc;

	mutex_lock(&mdp3_res->res_mutex);
	rc = mdp3_clk_update(MDP3_CLK_DSI, 1);
	mutex_unlock(&mdp3_res->res_mutex);
	return rc;
}

int mdp3_put_mdp_dsi_clk(void)
{
	int rc;

	mutex_lock(&mdp3_res->res_mutex);
	rc = mdp3_clk_update(MDP3_CLK_DSI, 0);
	mutex_unlock(&mdp3_res->res_mutex);
	return rc;
}

static int mdp3_irq_setup(void)
{
	int ret;
	struct mdss_hw *mdp3_hw;

	mdp3_hw = &mdp3_res->mdp3_hw;
	ret = devm_request_irq(&mdp3_res->pdev->dev,
				mdp3_hw->irq_info->irq,
				mdp3_irq_handler,
				0, "MDP", mdp3_res);
	if (ret) {
		pr_err("mdp request_irq() failed!\n");
		return ret;
	}
	disable_irq_nosync(mdp3_hw->irq_info->irq);
	mdp3_res->irq_registered = true;
	return 0;
}

static int mdp3_get_iommu_domain(u32 type)
{
	if (type >= MDSS_IOMMU_MAX_DOMAIN)
		return -EINVAL;

	if (!mdp3_res)
		return -ENODEV;

	return mdp3_res->domains[type].domain_idx;
}

static int mdp3_check_version(void)
{
	int rc;

	rc = mdp3_clk_enable(1, 0);
	if (rc) {
		pr_err("fail to turn on MDP core clks\n");
		return rc;
	}

	mdp3_res->mdp_rev = MDP3_REG_READ(MDP3_REG_HW_VERSION);

	if (mdp3_res->mdp_rev != MDP_CORE_HW_VERSION) {
		pr_err("mdp_hw_revision=%x mismatch\n", mdp3_res->mdp_rev);
		rc = -ENODEV;
	}

	rc = mdp3_clk_enable(0, 0);
	if (rc)
		pr_err("fail to turn off MDP core clks\n");

	return rc;
}

static int mdp3_hw_init(void)
{
	int i;

	for (i = MDP3_DMA_P; i < MDP3_DMA_MAX; i++) {
		mdp3_res->dma[i].dma_sel = i;
		mdp3_res->dma[i].capability = MDP3_DMA_CAP_ALL;
		mdp3_res->dma[i].in_use = 0;
		mdp3_res->dma[i].available = 1;
		mdp3_res->dma[i].cc_vect_sel = 0;
		mdp3_res->dma[i].lut_sts = 0;
		mdp3_res->dma[i].hist_cmap = NULL;
		mdp3_res->dma[i].gc_cmap = NULL;
		mutex_init(&mdp3_res->dma[i].pp_lock);
	}
	mdp3_res->dma[MDP3_DMA_S].capability = MDP3_DMA_CAP_DITHER;
	mdp3_res->dma[MDP3_DMA_E].available = 0;

	for (i = MDP3_DMA_OUTPUT_SEL_AHB; i < MDP3_DMA_OUTPUT_SEL_MAX; i++) {
		mdp3_res->intf[i].cfg.type = i;
		mdp3_res->intf[i].active = 0;
		mdp3_res->intf[i].in_use = 0;
		mdp3_res->intf[i].available = 1;
	}
	mdp3_res->intf[MDP3_DMA_OUTPUT_SEL_AHB].available = 0;
	mdp3_res->intf[MDP3_DMA_OUTPUT_SEL_LCDC].available = 0;
	mdp3_res->smart_blit_en = SMART_BLIT_RGB_EN | SMART_BLIT_YUV_EN;
	mdp3_res->solid_fill_vote_en = false;
	return 0;
}

int mdp3_dynamic_clock_gating_ctrl(int enable)
{
	int rc = 0;
	int cgc_cfg = 0;
	/*Disable dynamic auto clock gating*/
	pr_debug("%s Status %s\n", __func__, (enable ? "ON":"OFF"));
	rc = mdp3_clk_enable(1, 0);
	if (rc) {
		pr_err("fail to turn on MDP core clks\n");
		return rc;
	}
	cgc_cfg = MDP3_REG_READ(MDP3_REG_CGC_EN);
	if (enable) {
		cgc_cfg |= (BIT(10));
		cgc_cfg |= (BIT(18));
		MDP3_REG_WRITE(MDP3_REG_CGC_EN, cgc_cfg);
		VBIF_REG_WRITE(MDP3_VBIF_REG_FORCE_EN, 0x0);
	} else {
		cgc_cfg &= ~(BIT(10));
		cgc_cfg &= ~(BIT(18));
		MDP3_REG_WRITE(MDP3_REG_CGC_EN, cgc_cfg);
		VBIF_REG_WRITE(MDP3_VBIF_REG_FORCE_EN, 0x3);
	}

	rc = mdp3_clk_enable(0, 0);
	if (rc)
		pr_err("fail to turn off MDP core clks\n");

	return rc;
}

/**
 * mdp3_get_panic_lut_cfg() - calculate panic and robust lut mask
 * @panel_width: Panel width
 *
 * DMA buffer has 16 fill levels. Which needs to configured as safe
 * and panic levels based on panel resolutions.
 * No. of fill levels used = ((panel active width * 8) / 512).
 * Roundoff the fill levels if needed.
 * half of the total fill levels used will be treated as panic levels.
 * Roundoff panic levels if total used fill levels are odd.
 *
 * Sample calculation for 720p display:
 * Fill levels used = (720 * 8) / 512 = 12.5 after round off 13.
 * panic levels = 13 / 2 = 6.5 after roundoff 7.
 * Panic mask = 0x3FFF (2 bits per level)
 * Robust mask = 0xFF80 (1 bit per level)
 */
u64 mdp3_get_panic_lut_cfg(u32 panel_width)
{
	u32 fill_levels = (((panel_width * 8) / 512) + 1);
	u32 panic_mask = 0;
	u32 robust_mask = 0;
	u32 i = 0;
	u64 panic_config = 0;
	u32 panic_levels = 0;

	panic_levels = fill_levels / 2;
	if (fill_levels % 2)
		panic_levels++;

	for (i = 0; i < panic_levels; i++) {
		panic_mask |= (BIT((i * 2) + 1) | BIT(i * 2));
		robust_mask |= BIT(i);
	}
	panic_config = ~robust_mask;
	panic_config = panic_config << 32;
	panic_config |= panic_mask;
	return panic_config;
}

int mdp3_qos_remapper_setup(struct mdss_panel_data *panel)
{
	int rc = 0;
	u64 panic_config = mdp3_get_panic_lut_cfg(panel->panel_info.xres);

	rc = mdp3_clk_update(MDP3_CLK_AHB, 1);
	rc |= mdp3_clk_update(MDP3_CLK_AXI, 1);
	rc |= mdp3_clk_update(MDP3_CLK_MDP_CORE, 1);
	if (rc) {
		pr_err("fail to turn on MDP core clks\n");
		return rc;
	}

	if (!panel)
		return -EINVAL;
	/* Program MDP QOS Remapper */
	MDP3_REG_WRITE(MDP3_DMA_P_QOS_REMAPPER, 0x1A9);
	MDP3_REG_WRITE(MDP3_DMA_P_WATERMARK_0, 0x0);
	MDP3_REG_WRITE(MDP3_DMA_P_WATERMARK_1, 0x0);
	MDP3_REG_WRITE(MDP3_DMA_P_WATERMARK_2, 0x0);
	/* PANIC setting depends on panel width*/
	MDP3_REG_WRITE(MDP3_PANIC_LUT0,	(panic_config & 0xFFFF));
	MDP3_REG_WRITE(MDP3_PANIC_LUT1, ((panic_config >> 16) & 0xFFFF));
	MDP3_REG_WRITE(MDP3_ROBUST_LUT, ((panic_config >> 32) & 0xFFFF));
	MDP3_REG_WRITE(MDP3_PANIC_ROBUST_CTRL, 0x1);
	pr_debug("Panel width %d Panic Lut0 %x Lut1 %x Robust %x\n",
		panel->panel_info.xres,
		MDP3_REG_READ(MDP3_PANIC_LUT0),
		MDP3_REG_READ(MDP3_PANIC_LUT1),
		MDP3_REG_READ(MDP3_ROBUST_LUT));

	rc = mdp3_clk_update(MDP3_CLK_AHB, 0);
	rc |= mdp3_clk_update(MDP3_CLK_AXI, 0);
	rc |= mdp3_clk_update(MDP3_CLK_MDP_CORE, 0);
	if (rc)
		pr_err("fail to turn off MDP core clks\n");
	return rc;
}

static int mdp3_res_init(void)
{
	int rc = 0;

	rc = mdp3_irq_setup();
	if (rc)
		return rc;

	rc = mdp3_clk_setup();
	if (rc)
		return rc;

	mdp3_res->ion_client = msm_ion_client_create(mdp3_res->pdev->name);
	if (IS_ERR_OR_NULL(mdp3_res->ion_client)) {
		pr_err("msm_ion_client_create() return error (%pK)\n",
				mdp3_res->ion_client);
		mdp3_res->ion_client = NULL;
		return -EINVAL;
	}
	mutex_init(&mdp3_res->iommu_lock);

	mdp3_res->domains = mdp3_iommu_domains;
	mdp3_res->bus_handle = mdp3_bus_handle;
	rc = mdp3_bus_scale_register();
	if (rc) {
		pr_err("unable to register bus scaling\n");
		return rc;
	}

	rc = mdp3_hw_init();

	return rc;
}

static void mdp3_res_deinit(void)
{
	struct mdss_hw *mdp3_hw;
	int rc = 0;

	mdp3_hw = &mdp3_res->mdp3_hw;
	mdp3_bus_scale_unregister();
	mutex_lock(&mdp3_res->iommu_lock);
	if (mdp3_res->iommu_ref_cnt) {
		mdp3_res->iommu_ref_cnt--;
		if (mdp3_res->iommu_ref_cnt == 0)
			rc = mdss_smmu_detach(mdss_res);
		} else {
			pr_err("iommu ref count %d\n", mdp3_res->iommu_ref_cnt);
		}
	mutex_unlock(&mdp3_res->iommu_lock);

	if (!IS_ERR_OR_NULL(mdp3_res->ion_client))
		ion_client_destroy(mdp3_res->ion_client);

	mdp3_clk_remove();

	if (mdp3_res->irq_registered)
		devm_free_irq(&mdp3_res->pdev->dev,
				mdp3_hw->irq_info->irq, mdp3_res);
}

static int mdp3_get_pan_intf(const char *pan_intf)
{
	int i, rc = MDSS_PANEL_INTF_INVALID;

	if (!pan_intf)
		return rc;

	for (i = 0; i < ARRAY_SIZE(pan_types); i++) {
		if (!strcmp(pan_intf, pan_types[i].name)) {
			rc = pan_types[i].type;
			break;
		}
	}

	return rc;
}

static int mdp3_parse_dt_pan_intf(struct platform_device *pdev)
{
	int rc;
	struct mdp3_hw_resource *mdata = platform_get_drvdata(pdev);
	const char *prim_intf = NULL;

	rc = of_property_read_string(pdev->dev.of_node,
				"qcom,mdss-pref-prim-intf", &prim_intf);
	if (rc)
		return -ENODEV;

	rc = mdp3_get_pan_intf(prim_intf);
	if (rc < 0) {
		mdata->pan_cfg.pan_intf = MDSS_PANEL_INTF_INVALID;
	} else {
		mdata->pan_cfg.pan_intf = rc;
		rc = 0;
	}
	return rc;
}

static int mdp3_get_pan_cfg(struct mdss_panel_cfg *pan_cfg)
{
	char *t = NULL;
	char pan_intf_str[MDSS_MAX_PANEL_LEN];
	int rc, i, panel_len;
	char pan_name[MDSS_MAX_PANEL_LEN];

	if (!pan_cfg)
		return -EINVAL;

	if (mdss_mdp3_panel[0] == '0') {
		pan_cfg->lk_cfg = false;
	} else if (mdss_mdp3_panel[0] == '1') {
		pan_cfg->lk_cfg = true;
	} else {
		/* read from dt */
		pan_cfg->lk_cfg = true;
		pan_cfg->pan_intf = MDSS_PANEL_INTF_INVALID;
		return -EINVAL;
	}

	/* skip lk cfg and delimiter; ex: "0:" */
	strlcpy(pan_name, &mdss_mdp3_panel[2], MDSS_MAX_PANEL_LEN);
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

	rc = mdp3_get_pan_intf(pan_intf_str);
	pan_cfg->pan_intf = (rc < 0) ?  MDSS_PANEL_INTF_INVALID : rc;
	return 0;
}

static int mdp3_get_cmdline_config(struct platform_device *pdev)
{
	int rc, len = 0;
	int *intf_type;
	char *panel_name;
	struct mdss_panel_cfg *pan_cfg;
	struct mdp3_hw_resource *mdata = platform_get_drvdata(pdev);

	mdata->pan_cfg.arg_cfg[MDSS_MAX_PANEL_LEN] = 0;
	pan_cfg = &mdata->pan_cfg;
	panel_name = &pan_cfg->arg_cfg[0];
	intf_type = &pan_cfg->pan_intf;

	/* reads from dt by default */
	pan_cfg->lk_cfg = true;

	len = strlen(mdss_mdp3_panel);

	if (len > 0) {
		rc = mdp3_get_pan_cfg(pan_cfg);
		if (!rc) {
			pan_cfg->init_done = true;
			return rc;
		}
	}

	rc = mdp3_parse_dt_pan_intf(pdev);
	/* if pref pan intf is not present */
	if (rc)
		pr_err("%s:unable to parse device tree for pan intf\n",
			__func__);
	else
		pan_cfg->init_done = true;

	return rc;
}


int mdp3_irq_init(u32 irq_start)
{
	struct mdss_hw *mdp3_hw;

	mdp3_hw = &mdp3_res->mdp3_hw;

	mdp3_hw->irq_info = kzalloc(sizeof(struct irq_info), GFP_KERNEL);
	if (!mdp3_hw->irq_info)
		return -ENOMEM;

	mdp3_hw->hw_ndx = MDSS_HW_MDP;
	mdp3_hw->irq_info->irq = irq_start;
	mdp3_hw->irq_info->irq_mask = 0;
	mdp3_hw->irq_info->irq_ena = false;
	mdp3_hw->irq_info->irq_buzy = false;

	mdp3_res->mdss_util->register_irq(&mdp3_res->mdp3_hw);
	return 0;
}

static int mdp3_parse_dt(struct platform_device *pdev)
{
	struct resource *res;
	struct property *prop = NULL;
	bool panic_ctrl;
	int rc;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mdp_phys");
	if (!res) {
		pr_err("unable to get MDP base address\n");
		return -EINVAL;
	}

	mdp3_res->mdp_reg_size = resource_size(res);
	mdp3_res->mdp_base = devm_ioremap(&pdev->dev, res->start,
					mdp3_res->mdp_reg_size);
	if (unlikely(!mdp3_res->mdp_base)) {
		pr_err("unable to map MDP base\n");
		return -ENOMEM;
	}

	pr_debug("MDP HW Base phy_Address=0x%x virt=0x%x\n",
		(int) res->start,
		(int) mdp3_res->mdp_base);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "vbif_phys");
	if (!res) {
		pr_err("unable to get VBIF base address\n");
		return -EINVAL;
	}

	mdp3_res->vbif_reg_size = resource_size(res);
	mdp3_res->vbif_base = devm_ioremap(&pdev->dev, res->start,
					mdp3_res->vbif_reg_size);
	if (unlikely(!mdp3_res->vbif_base)) {
		pr_err("unable to map VBIF base\n");
		return -ENOMEM;
	}

	pr_debug("VBIF HW Base phy_Address=0x%x virt=0x%x\n",
		(int) res->start,
		(int) mdp3_res->vbif_base);

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		pr_err("unable to get MDSS irq\n");
		return -EINVAL;
	}
	rc = mdp3_irq_init(res->start);
	if (rc) {
		pr_err("%s: Error in irq initialization:rc=[%d]\n",
		       __func__, rc);
		return rc;
	}

	rc = mdp3_get_cmdline_config(pdev);
	if (rc) {
		pr_err("%s: Error in panel override:rc=[%d]\n",
		       __func__, rc);
		kfree(mdp3_res->mdp3_hw.irq_info);
		return rc;
	}

	prop = of_find_property(pdev->dev.of_node, "batfet-supply", NULL);
	mdp3_res->batfet_required = prop ? true : false;

	panic_ctrl = of_property_read_bool(
				pdev->dev.of_node, "qcom,mdss-has-panic-ctrl");
	mdp3_res->dma[MDP3_DMA_P].has_panic_ctrl = panic_ctrl;

	mdp3_res->idle_pc_enabled = of_property_read_bool(
		pdev->dev.of_node, "qcom,mdss-idle-power-collapse-enabled");

	return 0;
}

void msm_mdp3_cx_ctrl(int enable)
{
	int rc;

	if (!mdp3_res->vdd_cx) {
		mdp3_res->vdd_cx = devm_regulator_get(&mdp3_res->pdev->dev,
								"vdd-cx");
		if (IS_ERR_OR_NULL(mdp3_res->vdd_cx)) {
			pr_debug("unable to get CX reg. rc=%d\n",
				PTR_RET(mdp3_res->vdd_cx));
			mdp3_res->vdd_cx = NULL;
			return;
		}
	}

	if (enable) {
		rc = regulator_set_voltage(
				mdp3_res->vdd_cx,
				RPM_REGULATOR_CORNER_SVS_SOC,
				RPM_REGULATOR_CORNER_SUPER_TURBO);
		if (rc < 0)
			goto vreg_set_voltage_fail;

		rc = regulator_enable(mdp3_res->vdd_cx);
		if (rc) {
			pr_err("Failed to enable regulator vdd_cx.\n");
			return;
		}
	} else {
		rc = regulator_disable(mdp3_res->vdd_cx);
		if (rc) {
			pr_err("Failed to disable regulator vdd_cx.\n");
			return;
		}
		rc = regulator_set_voltage(
				mdp3_res->vdd_cx,
				RPM_REGULATOR_CORNER_NONE,
				RPM_REGULATOR_CORNER_SUPER_TURBO);
		if (rc < 0)
			goto vreg_set_voltage_fail;
	}

	return;
vreg_set_voltage_fail:
	pr_err("Set vltg failed\n");
}

void mdp3_batfet_ctrl(int enable)
{
	int rc;

	if (!mdp3_res->batfet_required)
		return;

	if (!mdp3_res->batfet) {
		if (enable) {
			mdp3_res->batfet =
				devm_regulator_get(&mdp3_res->pdev->dev,
				"batfet");
			if (IS_ERR_OR_NULL(mdp3_res->batfet)) {
				pr_debug("unable to get batfet reg. rc=%d\n",
					PTR_RET(mdp3_res->batfet));
				mdp3_res->batfet = NULL;
				return;
			}
		} else {
			pr_debug("Batfet regulator disable w/o enable\n");
			return;
		}
	}

	if (enable)
		rc = regulator_enable(mdp3_res->batfet);
	else
		rc = regulator_disable(mdp3_res->batfet);

	if (rc < 0)
		pr_err("%s: reg enable/disable failed", __func__);
}

void mdp3_enable_regulator(int enable)
{
	mdp3_batfet_ctrl(enable);
}

int mdp3_put_img(struct mdp3_img_data *data, int client)
{
	struct ion_client *iclient = mdp3_res->ion_client;
	int dom = (mdp3_res->domains + MDP3_IOMMU_DOMAIN_UNSECURE)->domain_idx;
	int dir = DMA_BIDIRECTIONAL;

	if (data->flags & MDP_MEMORY_ID_TYPE_FB) {
		pr_info("mdp3_put_img fb mem buf=0x%pa\n", &data->addr);
		fdput(data->srcp_f);
		memset(&data->srcp_f, 0, sizeof(struct fd));
	} else if (!IS_ERR_OR_NULL(data->srcp_dma_buf)) {
		pr_debug("ion hdl = %pK buf=0x%pa\n", data->srcp_dma_buf,
							&data->addr);
		if (!iclient) {
			pr_err("invalid ion client\n");
			return -ENOMEM;
		}
		MDSS_XLOG(data->srcp_dma_buf, data->addr, data->len, client,
				data->mapped, data->skip_detach);
		if (data->mapped) {
			if (client == MDP3_CLIENT_PPP ||
						client == MDP3_CLIENT_DMA_P)
				mdss_smmu_unmap_dma_buf(data->tab_clone,
					dom, dir, data->srcp_dma_buf);
			else if (client == MDP3_CLIENT_SPI) {
				ion_unmap_kernel(iclient, data->srcp_ihdl);
				ion_free(iclient, data->srcp_ihdl);
				data->srcp_ihdl = NULL;
			} else
				mdss_smmu_unmap_dma_buf(data->srcp_table,
					dom, dir, data->srcp_dma_buf);
			data->mapped = false;
		}
		if (!data->skip_detach) {
			dma_buf_unmap_attachment(data->srcp_attachment,
				data->srcp_table,
			mdss_smmu_dma_data_direction(dir));
			dma_buf_detach(data->srcp_dma_buf,
					data->srcp_attachment);
			dma_buf_put(data->srcp_dma_buf);
			data->srcp_dma_buf = NULL;
		}
	} else {
		return -EINVAL;
	}
	if (client == MDP3_CLIENT_PPP || client == MDP3_CLIENT_DMA_P) {
		vfree(data->tab_clone->sgl);
		kfree(data->tab_clone);
	}
	return 0;
}

int mdp3_get_img(struct msmfb_data *img, struct mdp3_img_data *data, int client)
{
	struct fd f;
	int ret = -EINVAL;
	int fb_num;
	struct ion_client *iclient = mdp3_res->ion_client;
	int dom = (mdp3_res->domains + MDP3_IOMMU_DOMAIN_UNSECURE)->domain_idx;

	data->flags = img->flags;

	if (img->flags & MDP_MEMORY_ID_TYPE_FB) {
		f = fdget(img->memory_id);
		if (f.file == NULL) {
			pr_err("invalid framebuffer file (%d)\n",
					img->memory_id);
			return -EINVAL;
		}
		if (MAJOR(f.file->f_path.dentry->d_inode->i_rdev) == FB_MAJOR) {
			fb_num = MINOR(f.file->f_path.dentry->d_inode->i_rdev);
			ret = mdss_fb_get_phys_info(&data->addr,
							&data->len, fb_num);
			if (ret) {
				pr_err("mdss_fb_get_phys_info() failed\n");
				fdput(f);
				memset(&f, 0, sizeof(struct fd));
			}
		} else {
			pr_err("invalid FB_MAJOR\n");
			fdput(f);
			ret = -EINVAL;
		}
		data->srcp_f = f;
		if (!ret)
			goto done;
	} else if (iclient) {
		data->srcp_dma_buf = dma_buf_get(img->memory_id);
			if (IS_ERR(data->srcp_dma_buf)) {
				pr_err("DMA : error on ion_import_fd\n");
				ret = PTR_ERR(data->srcp_dma_buf);
				data->srcp_dma_buf = NULL;
				return ret;
			}
			if (client == MDP3_CLIENT_SPI) {
				data->srcp_ihdl = ion_import_dma_buf(iclient,
					data->srcp_dma_buf);
				if (IS_ERR_OR_NULL(data->srcp_ihdl)) {
					pr_err("error on ion_import_fd\n");
					data->srcp_ihdl = NULL;
					return -EIO;
				}
			}
			data->srcp_attachment =
			mdss_smmu_dma_buf_attach(data->srcp_dma_buf,
					&mdp3_res->pdev->dev, dom);
			if (IS_ERR(data->srcp_attachment)) {
				ret = PTR_ERR(data->srcp_attachment);
				goto err_put;
			}

			data->srcp_table =
				dma_buf_map_attachment(data->srcp_attachment,
			mdss_smmu_dma_data_direction(DMA_BIDIRECTIONAL));
			if (IS_ERR(data->srcp_table)) {
				ret = PTR_ERR(data->srcp_table);
				goto err_detach;
			}

			if (client == MDP3_CLIENT_PPP ||
						client == MDP3_CLIENT_DMA_P) {
				data->tab_clone =
				mdss_smmu_sg_table_clone(data->srcp_table,
							GFP_KERNEL, true);
				if (IS_ERR_OR_NULL(data->tab_clone)) {
					if (!(data->tab_clone))
						ret = -EINVAL;
					else
						ret = PTR_ERR(data->tab_clone);
					goto clone_err;
				}
				ret = mdss_smmu_map_dma_buf(data->srcp_dma_buf,
					data->tab_clone, dom,
					&data->addr, &data->len,
					DMA_BIDIRECTIONAL);
			} else if (client == MDP3_CLIENT_SPI) {
				void *vaddr;

					if (ion_handle_get_size(iclient,
						data->srcp_ihdl,
						(size_t *)&data->len) < 0) {
						pr_err("get size failed\n");
						return -EINVAL;
					}
					 vaddr = ion_map_kernel(iclient,
						data->srcp_ihdl);
					if (IS_ERR_OR_NULL(vaddr)) {
						pr_err("Mapping failed\n");
						mdp3_put_img(data, client);
						return -EINVAL;
					}
					data->addr = (dma_addr_t) vaddr;
					data->len -= img->offset;
					return 0;
			} else {
				ret = mdss_smmu_map_dma_buf(data->srcp_dma_buf,
					data->srcp_table, dom, &data->addr,
					&data->len, DMA_BIDIRECTIONAL);
			}

			if (IS_ERR_VALUE(ret)) {
				pr_err("smmu map dma buf failed: (%d)\n", ret);
				goto err_unmap;
			}

		data->mapped = true;
		data->skip_detach = false;
	}
done:
	if (client ==  MDP3_CLIENT_PPP || client == MDP3_CLIENT_DMA_P) {
		data->addr  += data->tab_clone->sgl->length;
		data->len   -= data->tab_clone->sgl->length;
	}
	if (!ret && (img->offset < data->len)) {
		data->addr += img->offset;
		data->len -= img->offset;

		pr_debug("mem=%d ihdl=%pK buf=0x%pa len=0x%lx\n",
			img->memory_id, data->srcp_dma_buf,
			&data->addr, data->len);

	} else {
		mdp3_put_img(data, client);
		return -EINVAL;
	}
	if (img->flags & MDP_MEMORY_ID_TYPE_FB) {
		MDSS_XLOG(img->memory_id, data->addr, data->len, fb_num);
	} else if (iclient) {
		MDSS_XLOG(img->memory_id, data->srcp_dma_buf, data->addr,
				data->len, client, data->mapped,
				data->skip_detach);
	}
	return ret;

clone_err:
	dma_buf_unmap_attachment(data->srcp_attachment, data->srcp_table,
		mdss_smmu_dma_data_direction(DMA_BIDIRECTIONAL));
err_detach:
	dma_buf_detach(data->srcp_dma_buf, data->srcp_attachment);
err_put:
	dma_buf_put(data->srcp_dma_buf);
	return ret;
err_unmap:
	dma_buf_unmap_attachment(data->srcp_attachment, data->srcp_table,
			mdss_smmu_dma_data_direction(DMA_BIDIRECTIONAL));
	dma_buf_detach(data->srcp_dma_buf, data->srcp_attachment);
	dma_buf_put(data->srcp_dma_buf);

	if (client ==  MDP3_CLIENT_PPP || client == MDP3_CLIENT_DMA_P) {
		vfree(data->tab_clone->sgl);
		kfree(data->tab_clone);
	}
	return ret;

}

int mdp3_iommu_enable(int client)
{
	int rc = 0;

	mutex_lock(&mdp3_res->iommu_lock);

	if (mdp3_res->iommu_ref_cnt == 0) {
		rc = mdss_smmu_attach(mdss_res);
		if (rc)
			rc = mdss_smmu_detach(mdss_res);
	}

	if (!rc)
		mdp3_res->iommu_ref_cnt++;
	mutex_unlock(&mdp3_res->iommu_lock);

	pr_debug("client :%d total_ref_cnt: %d\n",
			client, mdp3_res->iommu_ref_cnt);
	return rc;
}

int mdp3_iommu_disable(int client)
{
	int rc = 0;

	mutex_lock(&mdp3_res->iommu_lock);
	if (mdp3_res->iommu_ref_cnt) {
		mdp3_res->iommu_ref_cnt--;

		pr_debug("client :%d total_ref_cnt: %d\n",
				client, mdp3_res->iommu_ref_cnt);
		if (mdp3_res->iommu_ref_cnt == 0)
			rc = mdss_smmu_detach(mdss_res);
	} else {
		pr_err("iommu ref count unbalanced for client %d\n", client);
	}
	mutex_unlock(&mdp3_res->iommu_lock);

	return rc;
}

int mdp3_iommu_ctrl(int enable)
{
	int rc;

	if (mdp3_res->allow_iommu_update == false)
		return 0;

	if (enable)
		rc = mdp3_iommu_enable(MDP3_CLIENT_DSI);
	else
		rc = mdp3_iommu_disable(MDP3_CLIENT_DSI);
	return rc;
}

static int mdp3_init(struct msm_fb_data_type *mfd)
{
	int rc;

	rc = mdp3_ctrl_init(mfd);
	if (rc) {
		pr_err("mdp3 ctl init fail\n");
		return rc;
	}

	rc = mdp3_ppp_res_init(mfd);
	if (rc)
		pr_err("mdp3 ppp res init fail\n");

	return rc;
}

u32 mdp3_fb_stride(u32 fb_index, u32 xres, int bpp)
{
	/*
	 * The adreno GPU hardware requires that the pitch be aligned to
	 * 32 pixels for color buffers, so for the cases where the GPU
	 * is writing directly to fb0, the framebuffer pitch
	 * also needs to be 32 pixel aligned
	 */

	if (fb_index == 0)
		return ALIGN(xres, 32) * bpp;
	else
		return xres * bpp;
}

__ref int mdp3_parse_dt_splash(struct msm_fb_data_type *mfd)
{
	struct platform_device *pdev = mfd->pdev;
	int len = 0, rc = 0;
	u32 offsets[2];
	struct device_node *pnode, *child_node;
	struct property *prop = NULL;

	mfd->splash_info.splash_logo_enabled =
				of_property_read_bool(pdev->dev.of_node,
				"qcom,mdss-fb-splash-logo-enabled");

	prop = of_find_property(pdev->dev.of_node, "qcom,memblock-reserve",
				&len);
	if (!prop) {
		pr_debug("Read memblock reserve settings for fb failed\n");
		pr_debug("Read cont-splash-memory settings\n");
	}

	if (len) {
		len = len / sizeof(u32);

		rc = of_property_read_u32_array(pdev->dev.of_node,
			"qcom,memblock-reserve", offsets, len);
		if (rc) {
			pr_err("error reading mem reserve settings for fb\n");
			rc = -EINVAL;
			goto error;
		}
	} else {
		child_node = of_get_child_by_name(pdev->dev.of_node,
					"qcom,cont-splash-memory");
		if (!child_node) {
			pr_err("splash mem child node is not present\n");
			rc = -EINVAL;
			goto error;
		}

		pnode = of_parse_phandle(child_node, "linux,contiguous-region",
					0);
		if (pnode != NULL) {
			const u32 *addr;
			u64 size;

			addr = of_get_address(pnode, 0, &size, NULL);
			if (!addr) {
				pr_err("failed to parse the splash memory address\n");
				of_node_put(pnode);
				rc = -EINVAL;
				goto error;
			}
			offsets[0] = (u32) of_read_ulong(addr, 2);
			offsets[1] = (u32) size;
			of_node_put(pnode);
		} else {
			pr_err("mem reservation for splash screen fb not present\n");
			rc = -EINVAL;
			goto error;
		}
	}

	if (!memblock_is_reserved(offsets[0])) {
		pr_debug("failed to reserve memory for fb splash\n");
		rc = -EINVAL;
		goto error;
	}

	mdp3_res->splash_mem_addr = offsets[0];
	mdp3_res->splash_mem_size = offsets[1];
error:
	if (rc && mfd->panel_info->cont_splash_enabled)
		pr_err("no rsvd mem found in DT for splash screen\n");
	else
		rc = 0;

	return rc;
}

void mdp3_release_splash_memory(struct msm_fb_data_type *mfd)
{
	/* Give back the reserved memory to the system */
	if (mdp3_res->splash_mem_addr) {
		if ((mfd->panel.type == MIPI_VIDEO_PANEL) &&
				(mdp3_res->cont_splash_en)) {
			mdss_smmu_unmap(MDSS_IOMMU_DOMAIN_UNSECURE,
				mdp3_res->splash_mem_addr,
				mdp3_res->splash_mem_size);
		}
		pr_debug("mdp3_release_splash_memory\n");
		memblock_free(mdp3_res->splash_mem_addr,
				mdp3_res->splash_mem_size);
		free_bootmem_late(mdp3_res->splash_mem_addr,
				mdp3_res->splash_mem_size);
		mdp3_res->splash_mem_addr = 0;
	}
}

struct mdp3_dma *mdp3_get_dma_pipe(int capability)
{
	int i;

	for (i = MDP3_DMA_P; i < MDP3_DMA_MAX; i++) {
		if (!mdp3_res->dma[i].in_use && mdp3_res->dma[i].available &&
			mdp3_res->dma[i].capability & capability) {
			mdp3_res->dma[i].in_use = true;
			return &mdp3_res->dma[i];
		}
	}
	return NULL;
}

struct mdp3_intf *mdp3_get_display_intf(int type)
{
	int i;

	for (i = MDP3_DMA_OUTPUT_SEL_AHB; i < MDP3_DMA_OUTPUT_SEL_MAX; i++) {
		if (!mdp3_res->intf[i].in_use && mdp3_res->intf[i].available &&
			mdp3_res->intf[i].cfg.type == type) {
			mdp3_res->intf[i].in_use = true;
			return &mdp3_res->intf[i];
		}
	}
	return NULL;
}

static int mdp3_fb_mem_get_iommu_domain(void)
{
	if (!mdp3_res)
		return -ENODEV;
	return mdp3_res->domains[MDP3_IOMMU_DOMAIN_UNSECURE].domain_idx;
}

int mdp3_get_cont_spash_en(void)
{
	return mdp3_res->cont_splash_en;
}

static int mdp3_is_display_on(struct mdss_panel_data *pdata)
{
	int rc = 0;
	u32 status;

	rc = mdp3_clk_enable(1, 0);
	if (rc) {
		pr_err("fail to turn on MDP core clks\n");
		return rc;
	}
	if (pdata->panel_info.type == MIPI_VIDEO_PANEL) {
		status = MDP3_REG_READ(MDP3_REG_DSI_VIDEO_EN);
		rc = status & 0x1;
	} else if (pdata->panel_info.type == SPI_PANEL) {
		rc = is_spi_panel_continuous_splash_on(pdata);
	} else {
		status = MDP3_REG_READ(MDP3_REG_DMA_P_CONFIG);
		status &= 0x180000;
		rc = (status == 0x080000);
	}

	mdp3_res->splash_mem_addr = MDP3_REG_READ(MDP3_REG_DMA_P_IBUF_ADDR);

	if (mdp3_clk_enable(0, 0))
		pr_err("fail to turn off MDP core clks\n");
	return rc;
}

static int mdp3_continuous_splash_on(struct mdss_panel_data *pdata)
{
	struct mdss_panel_info *panel_info = &pdata->panel_info;
	struct mdp3_bus_handle_map *bus_handle;
	u64 ab = 0;
	u64 ib = 0;
	u64 mdp_clk_rate = 0;
	int rc = 0;

	pr_debug("mdp3__continuous_splash_on\n");

	bus_handle = &mdp3_res->bus_handle[MDP3_BUS_HANDLE];
	if (bus_handle->handle < 1) {
		pr_err("invalid bus handle %d\n", bus_handle->handle);
		return -EINVAL;
	}
	mdp3_calc_dma_res(panel_info, &mdp_clk_rate, &ab,
					&ib, MAX_BPP_SUPPORTED);

	mdp3_clk_set_rate(MDP3_CLK_VSYNC, MDP_VSYNC_CLK_RATE,
			MDP3_CLIENT_DMA_P);
	mdp3_clk_set_rate(MDP3_CLK_MDP_SRC, mdp_clk_rate,
			MDP3_CLIENT_DMA_P);

	/*DMA not used on SPI interface, remove DMA bus voting*/
	if (panel_info->type == SPI_PANEL)
		rc = mdp3_bus_scale_set_quota(MDP3_CLIENT_DMA_P, 0, 0);
	else
		rc = mdp3_bus_scale_set_quota(MDP3_CLIENT_DMA_P, ab, ib);
	bus_handle->restore_ab[MDP3_CLIENT_DMA_P] = ab;
	bus_handle->restore_ib[MDP3_CLIENT_DMA_P] = ib;

	rc = mdp3_res_update(1, 1, MDP3_CLIENT_DMA_P);
	if (rc) {
		pr_err("fail to enable clk\n");
		return rc;
	}

	rc = mdp3_ppp_init();
	if (rc) {
		pr_err("ppp init failed\n");
		goto splash_on_err;
	}

	if (panel_info->type == MIPI_VIDEO_PANEL)
		mdp3_res->intf[MDP3_DMA_OUTPUT_SEL_DSI_VIDEO].active = 1;
	else if (panel_info->type == SPI_PANEL)
		mdp3_res->intf[MDP3_DMA_OUTPUT_SEL_SPI_CMD].active = 1;
	else
		mdp3_res->intf[MDP3_DMA_OUTPUT_SEL_DSI_CMD].active = 1;

	mdp3_enable_regulator(true);
	mdp3_res->cont_splash_en = 1;
	return 0;

splash_on_err:
	if (mdp3_res_update(0, 1, MDP3_CLIENT_DMA_P))
		pr_err("%s: Unable to disable mdp3 clocks\n", __func__);

	return rc;
}

static int mdp3_panel_register_done(struct mdss_panel_data *pdata)
{
	int rc = 0;
	u64 ab = 0; u64 ib = 0;
	u64 mdp_clk_rate = 0;

	/* Store max bandwidth supported in mdp res */
	mdp3_calc_dma_res(&pdata->panel_info, &mdp_clk_rate, &ab, &ib,
			MAX_BPP_SUPPORTED);
	do_div(ab, 1024);
	mdp3_res->max_bw = ab+1;

	/*
	 * If idle pc feature is not enabled, then get a reference to the
	 * runtime device which will be released when device is turned off
	 */
	if (!mdp3_res->idle_pc_enabled ||
		pdata->panel_info.type != MIPI_CMD_PANEL) {
		pm_runtime_get_sync(&mdp3_res->pdev->dev);
	}

	if (pdata->panel_info.cont_splash_enabled) {
		if (!mdp3_is_display_on(pdata)) {
			pr_err("continuous splash, but bootloader is not\n");
			return 0;
		}
		rc = mdp3_continuous_splash_on(pdata);
	} else {
		if (mdp3_is_display_on(pdata)) {
			pr_err("lk continuous splash, but kerenl not\n");
			rc = mdp3_continuous_splash_on(pdata);
		}
	}
	/*
	 * We want to prevent iommu from being enabled if there is
	 * continue splash screen. This would have happened in
	 * res_update in continuous_splash_on without this flag.
	 */
	if (pdata->panel_info.cont_splash_enabled == false)
		mdp3_res->allow_iommu_update = true;

	mdss_res->pdata = pdata;
	return rc;
}

/* mdp3_clear_irq() - Clear interrupt
 * @ interrupt_mask : interrupt mask
 *
 * This function clear sync irq for command mode panel.
 * When system is entering in idle screen state.
 */
void mdp3_clear_irq(u32 interrupt_mask)
{
	unsigned long flag;
	u32 irq_status = 0;

	spin_lock_irqsave(&mdp3_res->irq_lock, flag);
	irq_status = interrupt_mask &
		MDP3_REG_READ(MDP3_REG_INTR_STATUS);
	if (irq_status)
		MDP3_REG_WRITE(MDP3_REG_INTR_CLEAR, irq_status);
	spin_unlock_irqrestore(&mdp3_res->irq_lock, flag);

}

/* mdp3_autorefresh_disable() - Disable Auto refresh
 * @ panel_info : pointer to panel configuration structure
 *
 * This function disable Auto refresh block for command mode panel.
 */
int mdp3_autorefresh_disable(struct mdss_panel_info *panel_info)
{
	if ((panel_info->type == MIPI_CMD_PANEL) &&
		(MDP3_REG_READ(MDP3_REG_AUTOREFRESH_CONFIG_P)))
		MDP3_REG_WRITE(MDP3_REG_AUTOREFRESH_CONFIG_P, 0);
	return 0;
}

int mdp3_splash_done(struct mdss_panel_info *panel_info)
{
	if (panel_info->cont_splash_enabled) {
		pr_err("continuous splash is on and splash done called\n");
		return -EINVAL;
	}
	mdp3_res->allow_iommu_update = true;
	return 0;
}

static int mdp3_debug_dump_stats_show(struct seq_file *s, void *v)
{
	struct mdp3_hw_resource *res = (struct mdp3_hw_resource *)s->private;

	seq_printf(s, "underrun: %08u\n", res->underrun_cnt);

	return 0;
}
DEFINE_MDSS_DEBUGFS_SEQ_FOPS(mdp3_debug_dump_stats);

static void mdp3_debug_enable_clock(int on)
{
	if (on)
		mdp3_clk_enable(1, 0);
	else
		mdp3_clk_enable(0, 0);
}

static int mdp3_debug_init(struct platform_device *pdev)
{
	int rc;
	struct mdss_data_type *mdata;
	struct mdss_debug_data *mdd;
	struct mdss_debug_base *mdp_dbg_blk = NULL;
	struct mdss_debug_base *vbif_dbg_blk = NULL;

	mdata = devm_kzalloc(&pdev->dev, sizeof(*mdata), GFP_KERNEL);
	if (!mdata)
		return -ENOMEM;

	mdss_res = mdata;
	mutex_init(&mdata->reg_lock);
	mutex_init(&mdata->reg_bus_lock);
	mutex_init(&mdata->bus_lock);
	INIT_LIST_HEAD(&mdata->reg_bus_clist);
	atomic_set(&mdata->sd_client_count, 0);
	atomic_set(&mdata->active_intf_cnt, 0);
	mdss_res->mdss_util = mdp3_res->mdss_util;

	mdata->debug_inf.debug_enable_clock = mdp3_debug_enable_clock;
	mdata->mdp_rev = mdp3_res->mdp_rev;
	mdata->pdev = pdev;

	rc = mdss_debugfs_init(mdata);
	if (rc)
		return rc;

	mdd = mdata->debug_inf.debug_data;
	if (!mdd)
		return -EINVAL;

	debugfs_create_file("stat", 0644, mdd->root, mdp3_res,
				&mdp3_debug_dump_stats_fops);

	/* MDP Debug base registration */
	rc = mdss_debug_register_base("mdp", mdp3_res->mdp_base,
					mdp3_res->mdp_reg_size, &mdp_dbg_blk);
	if (rc)
		return rc;

	mdss_debug_register_dump_range(pdev, mdp_dbg_blk, "qcom,regs-dump-mdp",
		"qcom,regs-dump-names-mdp", "qcom,regs-dump-xin-id-mdp");


	/* VBIF Debug base registration */
	if (mdp3_res->vbif_base) {
		rc = mdss_debug_register_base("vbif", mdp3_res->vbif_base,
					mdp3_res->vbif_reg_size, &vbif_dbg_blk);
		if (rc)
			return rc;

		mdss_debug_register_dump_range(pdev, vbif_dbg_blk,
			 "qcom,regs-dump-vbif", "qcom,regs-dump-names-vbif",
						 "qcom,regs-dump-xin-id-vbif");
	}

	return rc;
}

static void mdp3_debug_deinit(struct platform_device *pdev)
{
	if (mdss_res) {
		mdss_debugfs_remove(mdss_res);
		devm_kfree(&pdev->dev, mdss_res);
		mdss_res = NULL;
	}
}

static void mdp3_dma_underrun_intr_handler(int type, void *arg)
{
	struct mdp3_dma *dma = &mdp3_res->dma[MDP3_DMA_P];

	mdp3_res->underrun_cnt++;
	pr_err_ratelimited("display underrun detected count=%d\n",
			mdp3_res->underrun_cnt);
	ATRACE_INT("mdp3_dma_underrun_intr_handler", mdp3_res->underrun_cnt);

	if (dma->ccs_config.ccs_enable && !dma->ccs_config.ccs_dirty) {
		dma->ccs_config.ccs_dirty = true;
		schedule_work(&dma->underrun_work);
	}
}

uint32_t ppp_formats_supported[] = {
	MDP_RGB_565,
	MDP_BGR_565,
	MDP_RGB_888,
	MDP_BGR_888,
	MDP_XRGB_8888,
	MDP_ARGB_8888,
	MDP_RGBA_8888,
	MDP_BGRA_8888,
	MDP_RGBX_8888,
	MDP_Y_CBCR_H2V1,
	MDP_Y_CBCR_H2V2,
	MDP_Y_CBCR_H2V2_ADRENO,
	MDP_Y_CBCR_H2V2_VENUS,
	MDP_Y_CRCB_H2V1,
	MDP_Y_CRCB_H2V2,
	MDP_YCRYCB_H2V1,
	MDP_BGRX_8888,
};

uint32_t dma_formats_supported[] = {
	MDP_RGB_565,
	MDP_RGB_888,
	MDP_XRGB_8888,
};

static void __mdp3_set_supported_formats(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ppp_formats_supported); i++)
		SET_BIT(mdp3_res->ppp_formats, ppp_formats_supported[i]);

	for (i = 0; i < ARRAY_SIZE(dma_formats_supported); i++)
		SET_BIT(mdp3_res->dma_formats, dma_formats_supported[i]);
}

static void __update_format_supported_info(char *buf, int *cnt)
{
	int j;
	size_t len = PAGE_SIZE;
	int num_bytes = BITS_TO_BYTES(MDP_IMGTYPE_LIMIT1);
#define SPRINT(fmt, ...) \
	(*cnt += scnprintf(buf + *cnt, len - *cnt, fmt, ##__VA_ARGS__))

	SPRINT("ppp_input_fmts=");
	for (j = 0; j < num_bytes; j++)
		SPRINT("%d,", mdp3_res->ppp_formats[j]);
	SPRINT("\ndma_output_fmts=");
	for (j = 0; j < num_bytes; j++)
		SPRINT("%d,", mdp3_res->dma_formats[j]);
	SPRINT("\n");
#undef SPRINT
}

static ssize_t mdp3_show_capabilities(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	size_t len = PAGE_SIZE;
	int cnt = 0;

#define SPRINT(fmt, ...) \
		(cnt += scnprintf(buf + cnt, len - cnt, fmt, ##__VA_ARGS__))

	SPRINT("dma_pipes=%d\n", 1);
	SPRINT("mdp_version=3\n");
	SPRINT("hw_rev=%d\n", 305);
	SPRINT("pipe_count:%d\n", 1);
	SPRINT("pipe_num:%d pipe_type:dma pipe_ndx:%d rects:%d ", 0, 1, 1);
	SPRINT("pipe_is_handoff:%d display_id:%d\n", 0, 0);
	__update_format_supported_info(buf, &cnt);
	SPRINT("rgb_pipes=%d\n", 0);
	SPRINT("vig_pipes=%d\n", 0);
	SPRINT("dma_pipes=%d\n", 1);
	SPRINT("blending_stages=%d\n", 1);
	SPRINT("cursor_pipes=%d\n", 0);
	SPRINT("max_cursor_size=%d\n", 0);
	SPRINT("smp_count=%d\n", 0);
	SPRINT("smp_size=%d\n", 0);
	SPRINT("smp_mb_per_pipe=%d\n", 0);
	SPRINT("max_downscale_ratio=%d\n", PPP_DOWNSCALE_MAX);
	SPRINT("max_upscale_ratio=%d\n", PPP_UPSCALE_MAX);
	SPRINT("max_pipe_bw=%u\n", mdp3_res->max_bw);
	SPRINT("max_bandwidth_low=%u\n", mdp3_res->max_bw);
	SPRINT("max_bandwidth_high=%u\n", mdp3_res->max_bw);
	SPRINT("max_mdp_clk=%u\n", MDP_CORE_CLK_RATE_MAX);
	SPRINT("clk_fudge_factor=%u,%u\n", CLK_FUDGE_NUM, CLK_FUDGE_DEN);
	SPRINT("features=has_ppp\n");

#undef SPRINT

	return cnt;
}

static DEVICE_ATTR(caps, 0444, mdp3_show_capabilities, NULL);

static ssize_t mdp3_store_smart_blit(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	u32 data = -1;
	ssize_t rc = 0;

	rc = kstrtoint(buf, 10, &data);
	if (rc) {
		pr_err("kstrtoint failed. rc=%d\n", rc);
		return rc;
	}
	mdp3_res->smart_blit_en = data;
	pr_debug("mdp3 smart blit RGB %s YUV %s\n",
		(mdp3_res->smart_blit_en & SMART_BLIT_RGB_EN) ?
		"ENABLED" : "DISABLED",
		(mdp3_res->smart_blit_en & SMART_BLIT_YUV_EN) ?
		"ENABLED" : "DISABLED");
	return len;
}

static ssize_t mdp3_show_smart_blit(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;

	pr_debug("mdp3 smart blit RGB %s YUV %s\n",
		(mdp3_res->smart_blit_en & SMART_BLIT_RGB_EN) ?
		"ENABLED" : "DISABLED",
		(mdp3_res->smart_blit_en & SMART_BLIT_YUV_EN) ?
		"ENABLED" : "DISABLED");
	ret = snprintf(buf, PAGE_SIZE, "%d\n", mdp3_res->smart_blit_en);
	return ret;
}

static DEVICE_ATTR(smart_blit, 0664,
			mdp3_show_smart_blit, mdp3_store_smart_blit);

static ssize_t mdp3_store_twm(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	u32 data = -1;
	ssize_t rc = 0;

	rc = kstrtoint(buf, 10, &data);
	if (rc) {
		pr_err("kstrtoint failed. rc=%d\n", rc);
		return rc;
	}
	mdp3_res->twm_en = data ? true : false;
	pr_err("TWM :  %s\n", (mdp3_res->twm_en) ?
		"ENABLED" : "DISABLED");
	return len;
}

static ssize_t mdp3_show_twm(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;

	pr_err("TWM :  %s\n", (mdp3_res->twm_en) ?
		"ENABLED" : "DISABLED");
	ret = snprintf(buf, PAGE_SIZE, "%d\n", mdp3_res->twm_en);
	return ret;
}

static DEVICE_ATTR(twm_enable, 0664,
		mdp3_show_twm, mdp3_store_twm);

static struct attribute *mdp3_fs_attrs[] = {
	&dev_attr_caps.attr,
	&dev_attr_smart_blit.attr,
	&dev_attr_twm_enable.attr,
	NULL
};

static struct attribute_group mdp3_fs_attr_group = {
	.attrs = mdp3_fs_attrs
};

static int mdp3_register_sysfs(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int rc;

	rc = sysfs_create_group(&dev->kobj, &mdp3_fs_attr_group);

	return rc;
}

int mdp3_create_sysfs_link(struct device *dev)
{
	int rc;

	rc = sysfs_create_link_nowarn(&dev->kobj,
			&mdp3_res->pdev->dev.kobj, "mdp");

	return rc;
}

int mdp3_misr_get(struct mdp_misr *misr_resp)
{
	int result = 0, ret = -1;
	int crc = 0;

	pr_debug("%s CRC Capture on DSI\n", __func__);
	switch (misr_resp->block_id) {
	case DISPLAY_MISR_DSI0:
		MDP3_REG_WRITE(MDP3_REG_DSI_VIDEO_EN, 0);
		/* Sleep for one vsync after DSI video engine is disabled */
		msleep(20);
		/* Enable DSI_VIDEO_0 MISR Block */
		MDP3_REG_WRITE(MDP3_REG_MODE_DSI_PCLK, 0x20);
		/* Reset MISR Block */
		MDP3_REG_WRITE(MDP3_REG_MISR_RESET_DSI_PCLK, 1);
		/* Clear MISR capture done bit */
		MDP3_REG_WRITE(MDP3_REG_CAPTURED_DSI_PCLK, 0);
		/* Enable MDP DSI interface */
		MDP3_REG_WRITE(MDP3_REG_DSI_VIDEO_EN, 1);
		ret = readl_poll_timeout(mdp3_res->mdp_base +
			MDP3_REG_CAPTURED_DSI_PCLK, result,
			result & MDP3_REG_CAPTURED_DSI_PCLK_MASK,
			MISR_POLL_SLEEP, MISR_POLL_TIMEOUT);
			MDP3_REG_WRITE(MDP3_REG_MODE_DSI_PCLK, 0);
		if (ret == 0) {
			/* Disable DSI MISR interface */
			MDP3_REG_WRITE(MDP3_REG_MODE_DSI_PCLK, 0x0);
			crc = MDP3_REG_READ(MDP3_REG_MISR_CAPT_VAL_DSI_PCLK);
			pr_debug("CRC Val %d\n", crc);
		} else {
			pr_err("CRC Read Timed Out\n");
		}
		break;

	case DISPLAY_MISR_DSI_CMD:
		/* Select DSI PCLK Domain */
		MDP3_REG_WRITE(MDP3_REG_SEL_CLK_OR_HCLK_TEST_BUS, 0x004);
		/* Select Block id DSI_CMD */
		MDP3_REG_WRITE(MDP3_REG_MODE_DSI_PCLK, 0x10);
		/* Reset MISR Block */
		MDP3_REG_WRITE(MDP3_REG_MISR_RESET_DSI_PCLK, 1);
		/* Drive Data on Test Bus */
		MDP3_REG_WRITE(MDP3_REG_EXPORT_MISR_DSI_PCLK, 0);
		/* Kikk off DMA_P */
		MDP3_REG_WRITE(MDP3_REG_DMA_P_START, 0x11);
		/* Wait for DMA_P Done */
		ret = readl_poll_timeout(mdp3_res->mdp_base +
			MDP3_REG_INTR_STATUS, result,
			result & MDP3_INTR_DMA_P_DONE_BIT,
			MISR_POLL_SLEEP, MISR_POLL_TIMEOUT);
		if (ret == 0) {
			crc = MDP3_REG_READ(MDP3_REG_MISR_CURR_VAL_DSI_PCLK);
			pr_debug("CRC Val %d\n", crc);
		} else {
			pr_err("CRC Read Timed Out\n");
		}
		break;

	default:
		pr_err("%s CRC Capture not supported\n", __func__);
		ret = -EINVAL;
		break;
	}

	misr_resp->crc_value[0] = crc;
	pr_debug("%s, CRC Capture on DSI Param Block = 0x%x, CRC 0x%x\n",
			__func__, misr_resp->block_id, misr_resp->crc_value[0]);
	return ret;
}

int mdp3_misr_set(struct mdp_misr *misr_req)
{
	int ret = 0;

	pr_debug("%s Parameters Block = %d Cframe Count = %d CRC = %d\n",
			__func__, misr_req->block_id, misr_req->frame_count,
			misr_req->crc_value[0]);

	switch (misr_req->block_id) {
	case DISPLAY_MISR_DSI0:
		pr_debug("In the case DISPLAY_MISR_DSI0\n");
		MDP3_REG_WRITE(MDP3_REG_SEL_CLK_OR_HCLK_TEST_BUS, 1);
		MDP3_REG_WRITE(MDP3_REG_MODE_DSI_PCLK, 0x20);
		MDP3_REG_WRITE(MDP3_REG_MISR_RESET_DSI_PCLK, 0x1);
		break;

	case DISPLAY_MISR_DSI_CMD:
		pr_debug("In the case DISPLAY_MISR_DSI_CMD\n");
		MDP3_REG_WRITE(MDP3_REG_SEL_CLK_OR_HCLK_TEST_BUS, 1);
		MDP3_REG_WRITE(MDP3_REG_MODE_DSI_PCLK, 0x10);
		MDP3_REG_WRITE(MDP3_REG_MISR_RESET_DSI_PCLK, 0x1);
		break;

	default:
		pr_err("%s CRC Capture not supported\n", __func__);
		ret = -EINVAL;
		break;
	}
	return ret;
}

struct mdss_panel_cfg *mdp3_panel_intf_type(int intf_val)
{
	if (!mdp3_res || !mdp3_res->pan_cfg.init_done)
		return ERR_PTR(-EPROBE_DEFER);

	if (mdp3_res->pan_cfg.pan_intf == intf_val)
		return &mdp3_res->pan_cfg;
	else
		return NULL;
}
EXPORT_SYMBOL(mdp3_panel_intf_type);

int mdp3_footswitch_ctrl(int enable)
{
	int rc = 0;
	int active_cnt = 0;

	mutex_lock(&mdp3_res->fs_idle_pc_lock);
	MDSS_XLOG(enable);
	if (!mdp3_res->fs_ena && enable) {
		rc = regulator_enable(mdp3_res->fs);
		if (rc) {
			pr_err("mdp footswitch ctrl enable failed\n");
			mutex_unlock(&mdp3_res->fs_idle_pc_lock);
			return -EINVAL;
		}
		pr_debug("mdp footswitch ctrl enable success\n");
		mdp3_enable_regulator(true);
		mdp3_res->fs_ena = true;
	} else if (!enable && mdp3_res->fs_ena) {
		active_cnt = atomic_read(&mdp3_res->active_intf_cnt);
		if (active_cnt != 0) {
			/*
			 * Turning off GDSC while overlays are still
			 * active.
			 */
			mdp3_res->idle_pc = true;
			pr_debug("idle pc. active overlays=%d\n",
				active_cnt);
		}
		mdp3_enable_regulator(false);
		rc = regulator_disable(mdp3_res->fs);
		if (rc) {
			pr_err("mdp footswitch ctrl disable failed\n");
			mutex_unlock(&mdp3_res->fs_idle_pc_lock);
			return -EINVAL;
		}
			mdp3_res->fs_ena = false;
		pr_debug("mdp3 footswitch ctrl disable configured\n");
	} else {
		pr_debug("mdp3 footswitch ctrl already configured\n");
	}

	mutex_unlock(&mdp3_res->fs_idle_pc_lock);
	return rc;
}

int mdp3_panel_get_intf_status(u32 disp_num, u32 intf_type)
{
	int rc = 0, status = 0;

	if (intf_type != MDSS_PANEL_INTF_DSI)
		return 0;

	rc = mdp3_clk_enable(1, 0);
	if (rc) {
		pr_err("fail to turn on MDP core clks\n");
		return rc;
	}

	status = (MDP3_REG_READ(MDP3_REG_DMA_P_CONFIG) & 0x180000);
	/* DSI video mode or command mode */
	rc = (status == 0x180000) || (status == 0x080000);

	if (mdp3_clk_enable(0, 0))
		pr_err("fail to turn off MDP core clks\n");
	return rc;
}

static int mdp3_probe(struct platform_device *pdev)
{
	int rc;
	static struct msm_mdp_interface mdp3_interface = {
	.init_fnc = mdp3_init,
	.fb_mem_get_iommu_domain = mdp3_fb_mem_get_iommu_domain,
	.panel_register_done = mdp3_panel_register_done,
	.fb_stride = mdp3_fb_stride,
	.check_dsi_status = mdp3_check_dsi_ctrl_status,
	};

	struct mdp3_intr_cb underrun_cb = {
		.cb = mdp3_dma_underrun_intr_handler,
		.data = NULL,
	};

	pr_debug("%s: START\n", __func__);
	if (!pdev->dev.of_node) {
		pr_err("MDP driver only supports device tree probe\n");
		return -ENOTSUPP;
	}

	if (mdp3_res) {
		pr_err("MDP already initialized\n");
		return -EINVAL;
	}

	mdp3_res = devm_kzalloc(&pdev->dev, sizeof(struct mdp3_hw_resource),
				GFP_KERNEL);
	if (mdp3_res == NULL)
		return -ENOMEM;

	pdev->id = 0;
	mdp3_res->pdev = pdev;
	mutex_init(&mdp3_res->res_mutex);
	mutex_init(&mdp3_res->fs_idle_pc_lock);
	spin_lock_init(&mdp3_res->irq_lock);
	platform_set_drvdata(pdev, mdp3_res);
	atomic_set(&mdp3_res->active_intf_cnt, 0);
	mutex_init(&mdp3_res->reg_bus_lock);
	INIT_LIST_HEAD(&mdp3_res->reg_bus_clist);

	mdp3_res->mdss_util = mdss_get_util_intf();
	if (mdp3_res->mdss_util == NULL) {
		pr_err("Failed to get mdss utility functions\n");
		rc =  -ENODEV;
		goto get_util_fail;
	}
	mdp3_res->mdss_util->get_iommu_domain = mdp3_get_iommu_domain;
	mdp3_res->mdss_util->iommu_attached = is_mdss_iommu_attached;
	mdp3_res->mdss_util->iommu_ctrl = mdp3_iommu_ctrl;
	mdp3_res->mdss_util->bus_scale_set_quota = mdp3_bus_scale_set_quota;
	mdp3_res->mdss_util->panel_intf_type = mdp3_panel_intf_type;
	mdp3_res->mdss_util->dyn_clk_gating_ctrl =
		mdp3_dynamic_clock_gating_ctrl;
	mdp3_res->mdss_util->panel_intf_type = mdp3_panel_intf_type;
	mdp3_res->mdss_util->panel_intf_status = mdp3_panel_get_intf_status;
	mdp3_res->twm_en = false;

	if (mdp3_res->mdss_util->param_check(mdss_mdp3_panel)) {
		mdp3_res->mdss_util->display_disabled = true;
		mdp3_res->mdss_util->mdp_probe_done = true;
		return 0;
	}

	rc = mdp3_parse_dt(pdev);
	if (rc)
		goto probe_done;

	rc = mdp3_res_init();
	if (rc) {
		pr_err("unable to initialize mdp3 resources\n");
		goto probe_done;
	}

	mdp3_res->fs_ena = false;
	mdp3_res->fs = devm_regulator_get(&pdev->dev, "vdd");
	if (IS_ERR_OR_NULL(mdp3_res->fs)) {
		pr_err("unable to get mdss gdsc regulator\n");
		return -EINVAL;
	}

	rc = mdp3_debug_init(pdev);
	if (rc) {
		pr_err("unable to initialize mdp debugging\n");
		goto probe_done;
	}

	pm_runtime_set_autosuspend_delay(&pdev->dev, AUTOSUSPEND_TIMEOUT_MS);
	if (mdp3_res->idle_pc_enabled) {
		pr_debug("%s: Enabling autosuspend\n", __func__);
		pm_runtime_use_autosuspend(&pdev->dev);
	}
	/* Enable PM runtime */
	pm_runtime_set_suspended(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	if (!pm_runtime_enabled(&pdev->dev)) {
		rc = mdp3_footswitch_ctrl(1);
		if (rc) {
			pr_err("unable to turn on FS\n");
			goto probe_done;
		}
	}

	rc = mdp3_check_version();
	if (rc) {
		pr_err("mdp3 check version failed\n");
		goto probe_done;
	}
	rc = mdp3_register_sysfs(pdev);
	if (rc)
		pr_err("unable to register mdp sysfs nodes\n");

	rc = mdss_fb_register_mdp_instance(&mdp3_interface);
	if (rc)
		pr_err("unable to register mdp instance\n");

	rc = mdp3_set_intr_callback(MDP3_INTR_LCDC_UNDERFLOW,
					&underrun_cb);
	if (rc)
		pr_err("unable to configure interrupt callback\n");

	rc = mdss_smmu_init(mdss_res, &pdev->dev);
	if (rc)
		pr_err("mdss smmu init failed\n");

	__mdp3_set_supported_formats();

	mdp3_res->mdss_util->mdp_probe_done = true;
	pr_debug("%s: END\n", __func__);

	if (mdp3_res->pan_cfg.pan_intf == MDSS_PANEL_INTF_SPI)
		mdp3_interface.check_dsi_status = mdp3_check_spi_panel_status;

probe_done:
	if (IS_ERR_VALUE(rc))
		kfree(mdp3_res->mdp3_hw.irq_info);
get_util_fail:
	if (IS_ERR_VALUE(rc)) {
		mdp3_res_deinit();

		if (mdp3_res->mdp_base)
			devm_iounmap(&pdev->dev, mdp3_res->mdp_base);

		devm_kfree(&pdev->dev, mdp3_res);
		mdp3_res = NULL;

		if (mdss_res) {
			devm_kfree(&pdev->dev, mdss_res);
			mdss_res = NULL;
		}
	}

	return rc;
}

int mdp3_panel_get_boot_cfg(void)
{
	int rc;

	if (!mdp3_res || !mdp3_res->pan_cfg.init_done)
		rc = -EPROBE_DEFER;
	else if (mdp3_res->pan_cfg.lk_cfg)
		rc = 1;
	else
		rc = 0;
	return rc;
}

static  int mdp3_suspend_sub(void)
{
	mdp3_footswitch_ctrl(0);
	return 0;
}

static  int mdp3_resume_sub(void)
{
	mdp3_footswitch_ctrl(1);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int mdp3_pm_suspend(struct device *dev)
{
	dev_dbg(dev, "Display pm suspend\n");
	MDSS_XLOG(XLOG_FUNC_ENTRY);
	return mdp3_suspend_sub();
}

static int mdp3_pm_resume(struct device *dev)
{
	dev_dbg(dev, "Display pm resume\n");

	/*
	 * It is possible that the runtime status of the mdp device may
	 * have been active when the system was suspended. Reset the runtime
	 * status to suspended state after a complete system resume.
	 */
	pm_runtime_disable(dev);
	pm_runtime_set_suspended(dev);
	pm_runtime_enable(dev);

	MDSS_XLOG(XLOG_FUNC_ENTRY);
	return mdp3_resume_sub();
}
#endif

#if defined(CONFIG_PM) && !defined(CONFIG_PM_SLEEP)
static int mdp3_suspend(struct platform_device *pdev, pm_message_t state)
{
	pr_debug("Display suspend\n");

	MDSS_XLOG(XLOG_FUNC_ENTRY);
	return mdp3_suspend_sub();
}

static int mdp3_resume(struct platform_device *pdev)
{
	pr_debug("Display resume\n");

	MDSS_XLOG(XLOG_FUNC_ENTRY);
	return mdp3_resume_sub();
}
#else
#define mdp3_suspend NULL
#define mdp3_resume  NULL
#endif

#ifdef CONFIG_PM
static int mdp3_runtime_resume(struct device *dev)
{
	bool device_on = true;

	dev_dbg(dev, "Display pm runtime resume, active overlay cnt=%d\n",
		atomic_read(&mdp3_res->active_intf_cnt));

	/* do not resume panels when coming out of idle power collapse */
	if (!mdp3_res->idle_pc)
		device_for_each_child(dev, &device_on, mdss_fb_suspres_panel);

	MDSS_XLOG(XLOG_FUNC_ENTRY);
	mdp3_footswitch_ctrl(1);

	return 0;
}

static int mdp3_runtime_idle(struct device *dev)
{
	dev_dbg(dev, "Display pm runtime idle\n");

	return 0;
}

static int mdp3_runtime_suspend(struct device *dev)
{
	bool device_on = false;

	dev_dbg(dev, "Display pm runtime suspend, active overlay cnt=%d\n",
		atomic_read(&mdp3_res->active_intf_cnt));

	if (mdp3_res->clk_ena) {
		pr_debug("Clk turned on...MDP suspend failed\n");
		return -EBUSY;
	}

	MDSS_XLOG(XLOG_FUNC_ENTRY);
	mdp3_footswitch_ctrl(0);

	/* do not suspend panels when going in to idle power collapse */
	if (!mdp3_res->idle_pc)
		device_for_each_child(dev, &device_on, mdss_fb_suspres_panel);

	return 0;
}
#endif

static const struct dev_pm_ops mdp3_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mdp3_pm_suspend,
				mdp3_pm_resume)
	SET_RUNTIME_PM_OPS(mdp3_runtime_suspend,
				mdp3_runtime_resume,
				mdp3_runtime_idle)
};


static int mdp3_remove(struct platform_device *pdev)
{
	struct mdp3_hw_resource *mdata = platform_get_drvdata(pdev);

	if (!mdata)
		return -ENODEV;
	pm_runtime_disable(&pdev->dev);
	mdp3_bus_scale_unregister();
	mdp3_clk_remove();
	mdp3_debug_deinit(pdev);
	return 0;
}

static const struct of_device_id mdp3_dt_match[] = {
	{ .compatible = "qcom,mdss_mdp3",},
	{}
};
MODULE_DEVICE_TABLE(of, mdp3_dt_match);
EXPORT_COMPAT("qcom,mdss_mdp3");

static struct platform_driver mdp3_driver = {
	.probe = mdp3_probe,
	.remove = mdp3_remove,
	.suspend = mdp3_suspend,
	.resume = mdp3_resume,
	.shutdown = NULL,
	.driver = {
		.name = "mdp3",
		.of_match_table = mdp3_dt_match,
		.pm             = &mdp3_pm_ops,
	},
};

static int __init mdp3_driver_init(void)
{
	int ret;

	ret = platform_driver_register(&mdp3_driver);
	if (ret) {
		pr_err("register mdp3 driver failed!\n");
		return ret;
	}

	return 0;
}

module_param_string(panel, mdss_mdp3_panel, MDSS_MAX_PANEL_LEN, 0600);
/*
 * panel=<lk_cfg>:<pan_intf>:<pan_intf_cfg>
 * where <lk_cfg> is "1"-lk/gcdb config or "0" non-lk/non-gcdb
 * config; <pan_intf> is dsi:0
 * <pan_intf_cfg> is panel interface specific string
 * Ex: This string is panel's device node name from DT
 *	for DSI interface
 */
MODULE_PARM_DESC(panel, "lk supplied panel selection string");
module_init(mdp3_driver_init);
