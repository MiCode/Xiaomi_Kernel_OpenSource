/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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
#include <linux/file.h>
#include <linux/msm_kgsl.h>
#include <linux/major.h>
#include <linux/bootmem.h>
#include <linux/memblock.h>
#include <linux/iopoll.h>
#include <mach/board.h>
#include <mach/clk.h>
#include <mach/hardware.h>
#include <mach/msm_bus.h>
#include <mach/msm_bus_board.h>
#include <mach/iommu.h>
#include <mach/iommu_domains.h>
#include <mach/msm_memtypes.h>

#include "mdp3.h"
#include "mdss_fb.h"
#include "mdp3_hwio.h"
#include "mdp3_ctrl.h"
#include "mdp3_ppp.h"
#include "mdss_debug.h"

#define MISR_POLL_SLEEP                 2000
#define MISR_POLL_TIMEOUT               32000
#define MDP3_REG_CAPTURED_DSI_PCLK_MASK 1

#define MDP_CORE_HW_VERSION	0x03040310
struct mdp3_hw_resource *mdp3_res;

#define MDP_BUS_VECTOR_ENTRY_DMA(ab_val, ib_val)		\
	{						\
		.src = MSM_BUS_MASTER_MDP_PORT0,	\
		.dst = MSM_BUS_SLAVE_EBI_CH0,		\
		.ab = (ab_val),				\
		.ib = (ib_val),				\
	}

static struct msm_bus_vectors mdp_bus_dma_vectors[] = {
	MDP_BUS_VECTOR_ENTRY_DMA(0, 0),
	MDP_BUS_VECTOR_ENTRY_DMA(SZ_128M, SZ_256M),
	MDP_BUS_VECTOR_ENTRY_DMA(SZ_256M, SZ_512M),
};
static struct msm_bus_paths
	mdp_bus_dma_usecases[ARRAY_SIZE(mdp_bus_dma_vectors)];
static struct msm_bus_scale_pdata mdp_bus_dma_scale_table = {
	.usecase = mdp_bus_dma_usecases,
	.num_usecases = ARRAY_SIZE(mdp_bus_dma_usecases),
	.name = "mdp3",
};

#define MDP_BUS_VECTOR_ENTRY_PPP(ab_val, ib_val)		\
	{						\
		.src = MSM_BUS_MASTER_MDPE,	\
		.dst = MSM_BUS_SLAVE_EBI_CH0,		\
		.ab = (ab_val),				\
		.ib = (ib_val),				\
	}

static struct msm_bus_vectors mdp_bus_ppp_vectors[] = {
	MDP_BUS_VECTOR_ENTRY_PPP(0, 0),
	MDP_BUS_VECTOR_ENTRY_PPP(SZ_128M, SZ_256M),
	MDP_BUS_VECTOR_ENTRY_PPP(SZ_256M, SZ_512M),
};

static struct msm_bus_paths
	mdp_bus_ppp_usecases[ARRAY_SIZE(mdp_bus_ppp_vectors)];

static struct mdss_panel_intf pan_types[] = {
	{"dsi", MDSS_PANEL_INTF_DSI},
};

static struct msm_bus_scale_pdata mdp_bus_ppp_scale_table = {
	.usecase = mdp_bus_ppp_usecases,
	.num_usecases = ARRAY_SIZE(mdp_bus_ppp_usecases),
	.name = "mdp3_ppp",
};

struct mdp3_bus_handle_map mdp3_bus_handle[MDP3_BUS_HANDLE_MAX] = {
	[MDP3_BUS_HANDLE_DMA] = {
		.bus_vector = mdp_bus_dma_vectors,
		.usecases = mdp_bus_dma_usecases,
		.scale_pdata = &mdp_bus_dma_scale_table,
		.current_bus_idx = 0,
		.handle = 0,
	},
	[MDP3_BUS_HANDLE_PPP] = {
		.bus_vector = mdp_bus_ppp_vectors,
		.usecases = mdp_bus_ppp_usecases,
		.scale_pdata = &mdp_bus_ppp_scale_table,
		.current_bus_idx = 0,
		.handle = 0,
	},
};

struct mdp3_iommu_domain_map mdp3_iommu_domains[MDP3_IOMMU_DOMAIN_MAX] = {
	[MDP3_PPP_IOMMU_DOMAIN] = {
		.domain_type = MDP3_PPP_IOMMU_DOMAIN,
		.client_name = "mdp_ppp",
		.partitions = {
			{
				.start = SZ_128K,
				.size = SZ_1G - SZ_128K,
			},
		},
		.npartitions = 1,
	},
	[MDP3_DMA_IOMMU_DOMAIN] = {
		.domain_type = MDP3_DMA_IOMMU_DOMAIN,
		.client_name = "mdp_dma",
		.partitions = {
			{
				.start = SZ_128K,
				.size = SZ_1G - SZ_128K,
			},
		},
		.npartitions = 1,
	},
};

struct mdp3_iommu_ctx_map mdp3_iommu_contexts[MDP3_IOMMU_CTX_MAX] = {
	[MDP3_IOMMU_CTX_PPP_0] = {
		.ctx_type = MDP3_IOMMU_CTX_PPP_0,
		.domain = &mdp3_iommu_domains[MDP3_PPP_IOMMU_DOMAIN],
		.ctx_name = "mdpe_0",
		.attached = 0,
	},
	[MDP3_IOMMU_CTX_PPP_1] = {
		.ctx_type = MDP3_IOMMU_CTX_PPP_1,
		.domain = &mdp3_iommu_domains[MDP3_PPP_IOMMU_DOMAIN],
		.ctx_name = "mdpe_1",
		.attached = 0,
	},

	[MDP3_IOMMU_CTX_DMA_0] = {
		.ctx_type = MDP3_IOMMU_CTX_DMA_0,
		.domain = &mdp3_iommu_domains[MDP3_DMA_IOMMU_DOMAIN],
		.ctx_name = "mdps_0",
		.attached = 0,
	},

	[MDP3_IOMMU_CTX_DMA_1] = {
		.ctx_type = MDP3_IOMMU_CTX_DMA_1,
		.domain = &mdp3_iommu_domains[MDP3_DMA_IOMMU_DOMAIN],
		.ctx_name = "mdps_1",
		.attached = 0,
	},
};

static irqreturn_t mdp3_irq_handler(int irq, void *ptr)
{
	int i = 0;
	struct mdp3_hw_resource *mdata = (struct mdp3_hw_resource *)ptr;
	u32 mdp_interrupt = 0;

	spin_lock(&mdata->irq_lock);
	if (!mdata->irq_mask)
		pr_err("spurious interrupt\n");

	clk_enable(mdp3_res->clocks[MDP3_CLK_AHB]);
	clk_enable(mdp3_res->clocks[MDP3_CLK_CORE]);

	mdp_interrupt = MDP3_REG_READ(MDP3_REG_INTR_STATUS);
	MDP3_REG_WRITE(MDP3_REG_INTR_CLEAR, mdp_interrupt);
	pr_debug("mdp3_irq_handler irq=%d\n", mdp_interrupt);

	mdp_interrupt &= mdata->irq_mask;

	while (mdp_interrupt && i < MDP3_MAX_INTR) {
		if ((mdp_interrupt & 0x1) && mdata->callbacks[i].cb)
			mdata->callbacks[i].cb(i, mdata->callbacks[i].data);
		mdp_interrupt = mdp_interrupt >> 1;
		i++;
	}

	clk_disable(mdp3_res->clocks[MDP3_CLK_AHB]);
	clk_disable(mdp3_res->clocks[MDP3_CLK_CORE]);

	spin_unlock(&mdata->irq_lock);

	return IRQ_HANDLED;
}

void mdp3_irq_enable(int type)
{
	unsigned long flag;

	pr_debug("mdp3_irq_enable type=%d\n", type);
	spin_lock_irqsave(&mdp3_res->irq_lock, flag);
	mdp3_res->irq_ref_count[type] += 1;
	if (mdp3_res->irq_ref_count[type] > 1) {
		pr_debug("interrupt %d already enabled\n", type);
		spin_unlock_irqrestore(&mdp3_res->irq_lock, flag);
		return;
	}

	mdp3_res->irq_mask |= BIT(type);
	MDP3_REG_WRITE(MDP3_REG_INTR_ENABLE, mdp3_res->irq_mask);

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

	pr_debug("mdp3_irq_register\n");
	spin_lock_irqsave(&mdp3_res->irq_lock, flag);
	enable_irq(mdp3_res->irq);
	spin_unlock_irqrestore(&mdp3_res->irq_lock, flag);
}

void mdp3_irq_deregister(void)
{
	unsigned long flag;

	pr_debug("mdp3_irq_deregister\n");
	spin_lock_irqsave(&mdp3_res->irq_lock, flag);
	memset(mdp3_res->irq_ref_count, 0, sizeof(u32) * MDP3_MAX_INTR);
	mdp3_res->irq_mask = 0;
	disable_irq_nosync(mdp3_res->irq);
	spin_unlock_irqrestore(&mdp3_res->irq_lock, flag);
}

static int mdp3_bus_scale_register(void)
{
	int i;

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
	int rc;

	if (client == MDP3_CLIENT_DMA_P) {
		client_idx  = MDP3_BUS_HANDLE_DMA;
	} else if (client == MDP3_CLIENT_PPP) {
		client_idx  = MDP3_BUS_HANDLE_PPP;
	} else {
		pr_err("invalid client %d\n", client);
		return -EINVAL;
	}

	bus_handle = &mdp3_res->bus_handle[client_idx];
	cur_bus_idx = bus_handle->current_bus_idx;

	if (bus_handle->handle < 1) {
		pr_err("invalid bus handle %d\n", bus_handle->handle);
		return -EINVAL;
	}

	if ((ab_quota | ib_quota) == 0) {
		bus_idx = 0;
	} else {
		int num_cases = bus_handle->scale_pdata->num_usecases;
		struct msm_bus_vectors *vect = NULL;

		bus_idx = (cur_bus_idx % (num_cases - 1)) + 1;

		/* aligning to avoid performing updates for small changes */
		ab_quota = ALIGN(ab_quota, SZ_64M);
		ib_quota = ALIGN(ib_quota, SZ_64M);

		vect = bus_handle->scale_pdata->usecase[cur_bus_idx].vectors;
		if ((ab_quota == vect->ab) && (ib_quota == vect->ib)) {
			pr_debug("skip bus scaling, no change in vectors\n");
			return 0;
		}

		vect = bus_handle->scale_pdata->usecase[bus_idx].vectors;
		vect->ab = ab_quota;
		vect->ib = ib_quota;

		pr_debug("bus scale idx=%d ab=%llu ib=%llu\n", bus_idx,
				vect->ab, vect->ib);
	}
	bus_handle->current_bus_idx = bus_idx;
	rc = msm_bus_scale_client_update_request(bus_handle->handle, bus_idx);
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
		ret = clk_enable(clk);
	} else if (count == 0) {
		pr_debug("clk=%d disable\n", clk_idx);
		clk_disable(clk);
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
		if (clk_type == MDP3_CLK_CORE) {
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
				pr_debug("mdp clk rate=%lu\n", rounded_rate);
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

	rc = mdp3_clk_register("core_clk", MDP3_CLK_CORE);
	if (rc)
		return rc;

	rc = mdp3_clk_register("vsync_clk", MDP3_CLK_VSYNC);
	if (rc)
		return rc;

	rc = mdp3_clk_register("lcdc_clk", MDP3_CLK_LCDC);
	if (rc)
		return rc;

	rc = mdp3_clk_register("dsi_clk", MDP3_CLK_DSI);
	if (rc)
		return rc;
	return rc;
}

static void mdp3_clk_remove(void)
{
	if (!IS_ERR_OR_NULL(mdp3_res->clocks[MDP3_CLK_AHB]))
		clk_put(mdp3_res->clocks[MDP3_CLK_AHB]);

	if (!IS_ERR_OR_NULL(mdp3_res->clocks[MDP3_CLK_CORE]))
		clk_put(mdp3_res->clocks[MDP3_CLK_CORE]);

	if (!IS_ERR_OR_NULL(mdp3_res->clocks[MDP3_CLK_VSYNC]))
		clk_put(mdp3_res->clocks[MDP3_CLK_VSYNC]);

	if (!IS_ERR_OR_NULL(mdp3_res->clocks[MDP3_CLK_LCDC]))
		clk_put(mdp3_res->clocks[MDP3_CLK_LCDC]);

	if (!IS_ERR_OR_NULL(mdp3_res->clocks[MDP3_CLK_DSI]))
		clk_put(mdp3_res->clocks[MDP3_CLK_DSI]);
}

int mdp3_clk_enable(int enable, int dsi_clk)
{
	int rc;

	pr_debug("MDP CLKS %s\n", (enable ? "Enable" : "Disable"));

	mutex_lock(&mdp3_res->res_mutex);
	rc = mdp3_clk_update(MDP3_CLK_AHB, enable);
	rc |= mdp3_clk_update(MDP3_CLK_CORE, enable);
	rc |= mdp3_clk_update(MDP3_CLK_VSYNC, enable);
	if (dsi_clk)
		rc |= mdp3_clk_update(MDP3_CLK_DSI, enable);
	mutex_unlock(&mdp3_res->res_mutex);
	return rc;
}

int mdp3_clk_prepare(void)
{
	int rc = 0;

	mutex_lock(&mdp3_res->res_mutex);
	mdp3_res->clk_prepare_count++;
	if (mdp3_res->clk_prepare_count == 1) {
		rc = clk_prepare(mdp3_res->clocks[MDP3_CLK_AHB]);
		if (rc < 0)
			goto error0;
		rc = clk_prepare(mdp3_res->clocks[MDP3_CLK_CORE]);
		if (rc < 0)
			goto error1;
		rc = clk_prepare(mdp3_res->clocks[MDP3_CLK_VSYNC]);
		if (rc < 0)
			goto error2;
		rc = clk_prepare(mdp3_res->clocks[MDP3_CLK_DSI]);
		if (rc < 0)
			goto error3;
	}
	mutex_unlock(&mdp3_res->res_mutex);
	return rc;

error3:
	clk_unprepare(mdp3_res->clocks[MDP3_CLK_VSYNC]);
error2:
	clk_unprepare(mdp3_res->clocks[MDP3_CLK_CORE]);
error1:
	clk_unprepare(mdp3_res->clocks[MDP3_CLK_AHB]);
error0:
	mdp3_res->clk_prepare_count--;
	mutex_unlock(&mdp3_res->res_mutex);
	return rc;
}

void mdp3_clk_unprepare(void)
{
	mutex_lock(&mdp3_res->res_mutex);
	mdp3_res->clk_prepare_count--;
	if (mdp3_res->clk_prepare_count == 0) {
		clk_unprepare(mdp3_res->clocks[MDP3_CLK_AHB]);
		clk_unprepare(mdp3_res->clocks[MDP3_CLK_CORE]);
		clk_unprepare(mdp3_res->clocks[MDP3_CLK_VSYNC]);
		clk_unprepare(mdp3_res->clocks[MDP3_CLK_DSI]);
	} else if (mdp3_res->clk_prepare_count < 0) {
		pr_err("mdp3 clk unprepare mismatch\n");
	}
	mutex_unlock(&mdp3_res->res_mutex);
}

int mdp3_get_mdp_dsi_clk(void)
{
	int rc;

	mutex_lock(&mdp3_res->res_mutex);
	clk_prepare(mdp3_res->clocks[MDP3_CLK_DSI]);
	rc = mdp3_clk_update(MDP3_CLK_DSI, 1);
	mutex_unlock(&mdp3_res->res_mutex);
	return rc;
}

int mdp3_put_mdp_dsi_clk(void)
{
	int rc;
	mutex_lock(&mdp3_res->res_mutex);
	rc = mdp3_clk_update(MDP3_CLK_DSI, 0);
	clk_unprepare(mdp3_res->clocks[MDP3_CLK_DSI]);
	mutex_unlock(&mdp3_res->res_mutex);
	return rc;
}

static int mdp3_irq_setup(void)
{
	int ret;

	ret = devm_request_irq(&mdp3_res->pdev->dev,
				mdp3_res->irq,
				mdp3_irq_handler,
				IRQF_DISABLED, "MDP", mdp3_res);
	if (ret) {
		pr_err("mdp request_irq() failed!\n");
		return ret;
	}
	disable_irq(mdp3_res->irq);
	mdp3_res->irq_registered = true;
	return 0;
}

int mdp3_iommu_attach(int context)
{
	struct mdp3_iommu_ctx_map *context_map;
	struct mdp3_iommu_domain_map *domain_map;

	if (context >= MDP3_IOMMU_CTX_MAX)
		return -EINVAL;

	context_map = mdp3_res->iommu_contexts + context;
	if (context_map->attached) {
		pr_warn("mdp iommu already attached\n");
		return 0;
	}

	domain_map = context_map->domain;

	iommu_attach_device(domain_map->domain, context_map->ctx);

	context_map->attached = true;
	return 0;
}

int mdp3_iommu_dettach(int context)
{
	struct mdp3_iommu_ctx_map *context_map;
	struct mdp3_iommu_domain_map *domain_map;

	if (!mdp3_res->iommu_contexts ||
		context >= MDP3_IOMMU_CTX_MAX)
		return -EINVAL;

	context_map = mdp3_res->iommu_contexts + context;
	if (!context_map->attached) {
		pr_warn("mdp iommu not attached\n");
		return 0;
	}

	domain_map = context_map->domain;
	iommu_detach_device(domain_map->domain, context_map->ctx);
	context_map->attached = false;

	return 0;
}

int mdp3_iommu_domain_init(void)
{
	struct msm_iova_layout layout;
	int i;

	if (mdp3_res->domains) {
		pr_warn("iommu domain already initialized\n");
		return 0;
	}

	for (i = 0; i < MDP3_IOMMU_DOMAIN_MAX; i++) {
		int domain_idx;
		layout.client_name = mdp3_iommu_domains[i].client_name;
		layout.partitions = mdp3_iommu_domains[i].partitions;
		layout.npartitions = mdp3_iommu_domains[i].npartitions;
		layout.is_secure = false;

		domain_idx = msm_register_domain(&layout);
		if (IS_ERR_VALUE(domain_idx))
			return -EINVAL;

		mdp3_iommu_domains[i].domain_idx = domain_idx;
		mdp3_iommu_domains[i].domain = msm_get_iommu_domain(domain_idx);
		if (IS_ERR_OR_NULL(mdp3_iommu_domains[i].domain)) {
			pr_err("unable to get iommu domain(%d)\n",
				domain_idx);
			if (!mdp3_iommu_domains[i].domain)
				return -EINVAL;
			else
				return PTR_ERR(mdp3_iommu_domains[i].domain);
		}
	}

	mdp3_res->domains = mdp3_iommu_domains;

	return 0;
}

int mdp3_iommu_context_init(void)
{
	int i;

	if (mdp3_res->iommu_contexts) {
		pr_warn("iommu context already initialized\n");
		return 0;
	}

	for (i = 0; i < MDP3_IOMMU_CTX_MAX; i++) {
		mdp3_iommu_contexts[i].ctx =
			msm_iommu_get_ctx(mdp3_iommu_contexts[i].ctx_name);

		if (IS_ERR_OR_NULL(mdp3_iommu_contexts[i].ctx)) {
			pr_warn("unable to get iommu ctx(%s)\n",
				mdp3_iommu_contexts[i].ctx_name);
			if (!mdp3_iommu_contexts[i].ctx)
				return -EINVAL;
			else
				return PTR_ERR(mdp3_iommu_contexts[i].ctx);
		}
	}

	mdp3_res->iommu_contexts = mdp3_iommu_contexts;

	return 0;
}

int mdp3_iommu_init(void)
{
	int ret;

	mutex_init(&mdp3_res->iommu_lock);

	ret = mdp3_iommu_domain_init();
	if (ret) {
		pr_err("mdp3 iommu domain init fails\n");
		return ret;
	}

	ret = mdp3_iommu_context_init();
	if (ret) {
		pr_err("mdp3 iommu context init fails\n");
		return ret;
	}
	return ret;
}

void mdp3_iommu_deinit(void)
{
	int i;

	if (!mdp3_res->domains)
		return;

	for (i = 0; i < MDP3_IOMMU_DOMAIN_MAX; i++) {
		if (!IS_ERR_OR_NULL(mdp3_res->domains[i].domain))
			msm_unregister_domain(mdp3_res->domains[i].domain);
	}
}

static int mdp3_check_version(void)
{
	int rc;

	rc = mdp3_clk_update(MDP3_CLK_AHB, 1);
	rc |= mdp3_clk_update(MDP3_CLK_CORE, 1);
	if (rc)
		return rc;

	mdp3_res->mdp_rev = MDP3_REG_READ(MDP3_REG_HW_VERSION);

	rc = mdp3_clk_update(MDP3_CLK_AHB, 0);
	rc |= mdp3_clk_update(MDP3_CLK_CORE, 0);
	if (rc)
		pr_err("fail to turn off the MDP3_CLK_AHB clk\n");

	if (mdp3_res->mdp_rev != MDP_CORE_HW_VERSION) {
		pr_err("mdp_hw_revision=%x mismatch\n", mdp3_res->mdp_rev);
		rc = -ENODEV;
	}
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

	return 0;
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

	mdp3_res->ion_client = msm_ion_client_create(-1, mdp3_res->pdev->name);
	if (IS_ERR_OR_NULL(mdp3_res->ion_client)) {
		pr_err("msm_ion_client_create() return error (%p)\n",
				mdp3_res->ion_client);
		mdp3_res->ion_client = NULL;
		return -EINVAL;
	}

	rc = mdp3_iommu_init();
	if (rc)
		return rc;

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
	mdp3_bus_scale_unregister();
	mdp3_iommu_dettach(MDP3_IOMMU_CTX_DMA_0);
	mdp3_iommu_deinit();

	if (!IS_ERR_OR_NULL(mdp3_res->ion_client))
		ion_client_destroy(mdp3_res->ion_client);

	mdp3_clk_remove();

	if (mdp3_res->irq_registered)
		devm_free_irq(&mdp3_res->pdev->dev, mdp3_res->irq, mdp3_res);
}

static int mdp3_get_pan_intf(const char *pan_intf)
{
	int i, rc = MDSS_PANEL_INTF_INVALID;

	if (!pan_intf)
		return rc;

	for (i = 0; i < ARRAY_SIZE(pan_types); i++) {
		if (!strncmp(pan_intf, pan_types[i].name,
				strlen(pan_types[i].name))) {
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
	int rc, i;
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
	rc = mdp3_get_pan_intf(pan_intf_str);
	pan_cfg->pan_intf = (rc < 0) ?  MDSS_PANEL_INTF_INVALID : rc;
	return 0;
}

static int mdp3_parse_bootarg(struct platform_device *pdev)
{
	struct device_node *chosen_node;
	static const char *cmd_line;
	char *disp_idx, *end_idx;
	int rc, len = 0, name_len, cmd_len;
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

	rc = mdp3_get_pan_cfg(pan_cfg);
	if (!rc)
		pan_cfg->init_done = true;

	return rc;

get_dt_pan:
	rc = mdp3_parse_dt_pan_intf(pdev);
	/* if pref pan intf is not present */
	if (rc)
		pr_err("%s:unable to parse device tree for pan intf\n",
			__func__);
	else
		pan_cfg->init_done = true;

	of_node_put(chosen_node);
	return rc;
}

static int mdp3_parse_dt(struct platform_device *pdev)
{
	struct resource *res;
	struct property *prop = NULL;
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

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		pr_err("unable to get MDSS irq\n");
		return -EINVAL;
	}
	mdp3_res->irq = res->start;

	rc = mdp3_parse_bootarg(pdev);
	if (rc) {
		pr_err("%s: Error in panel override:rc=[%d]\n",
		       __func__, rc);
		return rc;
	}

	prop = of_find_property(pdev->dev.of_node, "batfet-supply", NULL);
	mdp3_res->batfet_required = prop ? true : false;

	return 0;
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

static void mdp3_iommu_heap_unmap_iommu(struct mdp3_iommu_meta *meta)
{
	unsigned int domain_num;
	unsigned int partition_num = 0;
	struct iommu_domain *domain;

	domain_num = (mdp3_res->domains + MDP3_PPP_IOMMU_DOMAIN)->domain_idx;
	domain = msm_get_iommu_domain(domain_num);

	if (!domain) {
		pr_err("Could not get domain %d. Corruption?\n", domain_num);
		return;
	}

	iommu_unmap_range(domain, meta->iova_addr, meta->mapped_size);
	msm_free_iova_address(meta->iova_addr, domain_num, partition_num,
		meta->mapped_size);

	return;
}

static void mdp3_iommu_meta_destroy(struct kref *kref)
{
	struct mdp3_iommu_meta *meta =
			container_of(kref, struct mdp3_iommu_meta, ref);

	rb_erase(&meta->node, &mdp3_res->iommu_root);
	mdp3_iommu_heap_unmap_iommu(meta);
	dma_buf_put(meta->dbuf);
	kfree(meta);
}


static void mdp3_iommu_meta_put(struct mdp3_iommu_meta *meta)
{
	/* Need to lock here to prevent race against map/unmap */
	mutex_lock(&mdp3_res->iommu_lock);
	kref_put(&meta->ref, mdp3_iommu_meta_destroy);
	mutex_unlock(&mdp3_res->iommu_lock);
}

static struct mdp3_iommu_meta *mdp3_iommu_meta_lookup(struct sg_table *table)
{
	struct rb_root *root = &mdp3_res->iommu_root;
	struct rb_node **p = &root->rb_node;
	struct rb_node *parent = NULL;
	struct mdp3_iommu_meta *entry = NULL;

	while (*p) {
		parent = *p;
		entry = rb_entry(parent, struct mdp3_iommu_meta, node);

		if (table < entry->table)
			p = &(*p)->rb_left;
		else if (table > entry->table)
			p = &(*p)->rb_right;
		else
			return entry;
	}
	return NULL;
}

void mdp3_unmap_iommu(struct ion_client *client, struct ion_handle *handle)
{
	struct mdp3_iommu_meta *meta;
	struct sg_table *table;

	table = ion_sg_table(client, handle);

	mutex_lock(&mdp3_res->iommu_lock);
	meta = mdp3_iommu_meta_lookup(table);
	if (!meta) {
		WARN(1, "%s: buffer was never mapped for %p\n", __func__,
				handle);
		mutex_unlock(&mdp3_res->iommu_lock);
		goto out;
	}
	mutex_unlock(&mdp3_res->iommu_lock);

	mdp3_iommu_meta_put(meta);
out:
	return;
}

static void mdp3_iommu_meta_add(struct mdp3_iommu_meta *meta)
{
	struct rb_root *root = &mdp3_res->iommu_root;
	struct rb_node **p = &root->rb_node;
	struct rb_node *parent = NULL;
	struct mdp3_iommu_meta *entry;

	while (*p) {
		parent = *p;
		entry = rb_entry(parent, struct mdp3_iommu_meta, node);

		if (meta->table < entry->table) {
			p = &(*p)->rb_left;
		} else if (meta->table > entry->table) {
			p = &(*p)->rb_right;
		} else {
			pr_err("%s: handle %p already exists\n", __func__,
				entry->handle);
			BUG();
		}
	}

	rb_link_node(&meta->node, parent, p);
	rb_insert_color(&meta->node, root);
}

static int mdp3_iommu_map_iommu(struct mdp3_iommu_meta *meta,
	unsigned long align, unsigned long iova_length,
	unsigned int padding, unsigned long flags)
{
	struct iommu_domain *domain;
	int ret = 0;
	unsigned long size;
	unsigned long unmap_size;
	struct sg_table *table;
	int prot = IOMMU_WRITE | IOMMU_READ;
	unsigned int domain_num = (mdp3_res->domains +
			MDP3_PPP_IOMMU_DOMAIN)->domain_idx;
	unsigned int partition_num = 0;

	size = meta->size;
	table = meta->table;

	/* Use the biggest alignment to allow bigger IOMMU mappings.
	 * Use the first entry since the first entry will always be the
	 * biggest entry. To take advantage of bigger mapping sizes both the
	 * VA and PA addresses have to be aligned to the biggest size.
	 */
	if (sg_dma_len(table->sgl) > align)
		align = sg_dma_len(table->sgl);

	ret = msm_allocate_iova_address(domain_num, partition_num,
			meta->mapped_size, align, &meta->iova_addr);

	if (ret)
		goto out;

	domain = msm_get_iommu_domain(domain_num);

	if (!domain) {
		ret = -ENOMEM;
		goto out1;
	}

	/* Adding padding to before buffer */
	if (padding) {
		unsigned long phys_addr = sg_phys(table->sgl);
		ret = msm_iommu_map_extra(domain, meta->iova_addr, phys_addr,
				padding, SZ_4K, prot);
		if (ret)
			goto out1;
	}

	/* Mapping actual buffer */
	ret = iommu_map_range(domain, meta->iova_addr + padding,
			table->sgl, size, prot);
	if (ret) {
		pr_err("%s: could not map %lx in domain %p\n",
			__func__, meta->iova_addr, domain);
			unmap_size = padding;
		goto out2;
	}

	/* Adding padding to end of buffer */
	if (padding) {
		unsigned long phys_addr = sg_phys(table->sgl);
		unsigned long extra_iova_addr = meta->iova_addr +
				padding + size;
		ret = msm_iommu_map_extra(domain, extra_iova_addr, phys_addr,
				padding, SZ_4K, prot);
		if (ret) {
			unmap_size = padding + size;
			goto out2;
		}
	}
	return ret;

out2:
	iommu_unmap_range(domain, meta->iova_addr, unmap_size);
out1:
	msm_free_iova_address(meta->iova_addr, domain_num, partition_num,
				iova_length);

out:
	return ret;
}

static struct mdp3_iommu_meta *mdp3_iommu_meta_create(struct ion_client *client,
	struct ion_handle *handle, struct sg_table *table, unsigned long size,
	unsigned long align, unsigned long iova_length, unsigned int padding,
	unsigned long flags, unsigned long *iova)
{
	struct mdp3_iommu_meta *meta;
	int ret;

	meta = kzalloc(sizeof(*meta), GFP_KERNEL);

	if (!meta)
		return ERR_PTR(-ENOMEM);

	meta->handle = handle;
	meta->table = table;
	meta->size = size;
	meta->mapped_size = iova_length;
	meta->dbuf = ion_share_dma_buf(client, handle);
	kref_init(&meta->ref);

	ret = mdp3_iommu_map_iommu(meta,
		align, iova_length, padding, flags);
	if (ret < 0)	{
		pr_err("%s: Unable to map buffer\n", __func__);
		goto out;
	}

	*iova = meta->iova_addr;
	mdp3_iommu_meta_add(meta);

	return meta;
out:
	kfree(meta);
	return ERR_PTR(ret);
}

/*
 * PPP hw reads in tiles of 16 which might be outside mapped region
 * need to map buffers ourseleve to add extra padding
 */
int mdp3_self_map_iommu(struct ion_client *client, struct ion_handle *handle,
	unsigned long align, unsigned long padding, unsigned long *iova,
	unsigned long *buffer_size, unsigned long flags,
	unsigned long iommu_flags)
{
	struct mdp3_iommu_meta *iommu_meta = NULL;
	struct sg_table *table;
	struct scatterlist *sg;
	unsigned long size = 0, iova_length = 0;
	int ret = 0;
	int i;

	table = ion_sg_table(client, handle);
	if (IS_ERR_OR_NULL(table))
		return PTR_ERR(table);

	for_each_sg(table->sgl, sg, table->nents, i)
		size += sg_dma_len(sg);

	padding = PAGE_ALIGN(padding);

	/* Adding 16 lines padding before and after buffer */
	iova_length = size + 2 * padding;

	if (size & ~PAGE_MASK) {
		pr_debug("%s: buffer size %lx is not aligned to %lx", __func__,
			size, PAGE_SIZE);
		ret = -EINVAL;
		goto out;
	}

	if (iova_length & ~PAGE_MASK) {
		pr_debug("%s: iova_length %lx is not aligned to %lx", __func__,
			iova_length, PAGE_SIZE);
		ret = -EINVAL;
		goto out;
	}

	mutex_lock(&mdp3_res->iommu_lock);
	iommu_meta = mdp3_iommu_meta_lookup(table);

	if (!iommu_meta) {
		iommu_meta = mdp3_iommu_meta_create(client, handle, table, size,
				align, iova_length, padding, flags, iova);
		if (!IS_ERR_OR_NULL(iommu_meta)) {
			iommu_meta->flags = iommu_flags;
			ret = 0;
		} else {
			ret = PTR_ERR(iommu_meta);
			goto out_unlock;
		}
	} else {
		if (iommu_meta->flags != iommu_flags) {
			pr_err("%s: handle %p is already mapped with diff flag\n",
				__func__, handle);
			ret = -EINVAL;
			goto out_unlock;
		} else if (iommu_meta->mapped_size != iova_length) {
			pr_err("%s: handle %p is already mapped with diff len\n",
				__func__, handle);
			ret = -EINVAL;
			goto out_unlock;
		} else {
			kref_get(&iommu_meta->ref);
			*iova = iommu_meta->iova_addr;
		}
	}
	BUG_ON(iommu_meta->size != size);
	mutex_unlock(&mdp3_res->iommu_lock);

	*iova = *iova + padding;
	*buffer_size = size;
	return ret;

out_unlock:
	mutex_unlock(&mdp3_res->iommu_lock);
out:
	mdp3_iommu_meta_put(iommu_meta);
	return ret;
}

int mdp3_put_img(struct mdp3_img_data *data, int client)
{
	struct ion_client *iclient = mdp3_res->ion_client;
	int dom;

	 if (data->flags & MDP_MEMORY_ID_TYPE_FB) {
		pr_info("mdp3_put_img fb mem buf=0x%x\n", data->addr);
		fput_light(data->srcp_file, data->p_need);
		data->srcp_file = NULL;
	} else if (!IS_ERR_OR_NULL(data->srcp_ihdl)) {
		if (client == MDP3_CLIENT_DMA_P) {
			dom = (mdp3_res->domains +
				MDP3_DMA_IOMMU_DOMAIN)->domain_idx;
			ion_unmap_iommu(iclient, data->srcp_ihdl, dom, 0);
		} else {
			mdp3_unmap_iommu(iclient, data->srcp_ihdl);
		}
		ion_free(iclient, data->srcp_ihdl);
		data->srcp_ihdl = NULL;
	} else {
		return -EINVAL;
	}
	return 0;
}

int mdp3_get_img(struct msmfb_data *img, struct mdp3_img_data *data,
		int client)
{
	struct file *file;
	int ret = -EINVAL;
	int fb_num;
	unsigned long *start, *len;
	struct ion_client *iclient = mdp3_res->ion_client;
	int dom;

	start = (unsigned long *) &data->addr;
	len = (unsigned long *) &data->len;
	data->flags = img->flags;
	data->p_need = 0;

	if (img->flags & MDP_MEMORY_ID_TYPE_FB) {
		file = fget_light(img->memory_id, &data->p_need);
		if (file == NULL) {
			pr_err("invalid framebuffer file (%d)\n",
					img->memory_id);
			return -EINVAL;
		}
		if (MAJOR(file->f_dentry->d_inode->i_rdev) == FB_MAJOR) {
			fb_num = MINOR(file->f_dentry->d_inode->i_rdev);
			ret = mdss_fb_get_phys_info(start, len, fb_num);
			if (ret) {
				pr_err("mdss_fb_get_phys_info() failed\n");
				fput_light(file, data->p_need);
				file = NULL;
			}
		} else {
			pr_err("invalid FB_MAJOR\n");
			fput_light(file, data->p_need);
			file = NULL;
			ret = -EINVAL;
		}
		data->srcp_file = file;
		if (!ret)
			goto done;
	} else if (iclient) {
		data->srcp_ihdl = ion_import_dma_buf(iclient, img->memory_id);
		if (IS_ERR_OR_NULL(data->srcp_ihdl)) {
			pr_err("error on ion_import_fd\n");
			if (!data->srcp_ihdl)
				ret = -EINVAL;
			else
				ret = PTR_ERR(data->srcp_ihdl);
			data->srcp_ihdl = NULL;
			return ret;
		}
		if (client == MDP3_CLIENT_DMA_P) {
			dom = (mdp3_res->domains +
					MDP3_DMA_IOMMU_DOMAIN)->domain_idx;
			ret = ion_map_iommu(iclient, data->srcp_ihdl, dom,
					0, SZ_4K, 0, start, len, 0, 0);
		} else {
			ret = mdp3_self_map_iommu(iclient, data->srcp_ihdl,
				SZ_4K, data->padding, start, len, 0, 0);
		}
		if (IS_ERR_VALUE(ret)) {
			ion_free(iclient, data->srcp_ihdl);
			pr_err("failed to map ion handle (%d)\n", ret);
			return ret;
		}
	}
done:
	if (!ret && (img->offset < data->len)) {
		data->addr += img->offset;
		data->len -= img->offset;

		pr_debug("mem=%d ihdl=%p buf=0x%x len=0x%x\n", img->memory_id,
			 data->srcp_ihdl, data->addr, data->len);
	} else {
		mdp3_put_img(data, client);
		return -EINVAL;
	}

	return ret;
}

int mdp3_iommu_enable(int client)
{
	int rc;

	if (client == MDP3_CLIENT_DMA_P) {
		rc = mdp3_iommu_attach(MDP3_IOMMU_CTX_DMA_0);
	} else {
		rc = mdp3_iommu_attach(MDP3_IOMMU_CTX_PPP_0);
		rc |= mdp3_iommu_attach(MDP3_IOMMU_CTX_PPP_1);
	}

	return rc;
}

int mdp3_iommu_disable(int client)
{
	int rc;

	if (client == MDP3_CLIENT_DMA_P) {
		rc = mdp3_iommu_dettach(MDP3_IOMMU_CTX_DMA_0);
	} else {
		rc = mdp3_iommu_dettach(MDP3_IOMMU_CTX_PPP_0);
		rc |= mdp3_iommu_dettach(MDP3_IOMMU_CTX_PPP_1);
	}

	return rc;
}

int mdp3_iommu_is_attached(int client)
{
	struct mdp3_iommu_ctx_map *context_map;
	int context = MDP3_IOMMU_CTX_DMA_0;

	if (!mdp3_res->iommu_contexts)
		return 0;

	if (client == MDP3_CLIENT_PPP)
		context = MDP3_IOMMU_CTX_PPP_0;

	context_map = mdp3_res->iommu_contexts + context;
	return context_map->attached;
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

static int mdp3_alloc(size_t size, void **virt, unsigned long *phys)
{
	int ret = 0;

	if (mdp3_res->ion_handle) {
		pr_debug("memory already alloc\n");
		*virt = mdp3_res->virt;
		*phys = mdp3_res->phys;
		return 0;
	}

	mdp3_res->ion_handle = ion_alloc(mdp3_res->ion_client, size,
					SZ_1M,
					ION_HEAP(ION_QSECOM_HEAP_ID), 0);

	if (!IS_ERR_OR_NULL(mdp3_res->ion_handle)) {
		*virt = ion_map_kernel(mdp3_res->ion_client,
					mdp3_res->ion_handle);
		if (IS_ERR(*virt)) {
			pr_err("map kernel error\n");
			goto ion_map_kernel_err;
		}

		ret = ion_phys(mdp3_res->ion_client, mdp3_res->ion_handle,
				phys, &size);
		if (ret) {
			pr_err("%s ion_phys error\n", __func__);
			goto ion_map_phys_err;
		}

		mdp3_res->virt = *virt;
		mdp3_res->phys = *phys;
		mdp3_res->size = size;
	} else {
		pr_err("%s ion alloc fail\n", __func__);
		mdp3_res->ion_handle = NULL;
		return -ENOMEM;
	}

	return 0;

ion_map_phys_err:
	ion_unmap_kernel(mdp3_res->ion_client, mdp3_res->ion_handle);
ion_map_kernel_err:
	ion_free(mdp3_res->ion_client, mdp3_res->ion_handle);
	mdp3_res->ion_handle = NULL;
	mdp3_res->virt = NULL;
	mdp3_res->phys = 0;
	mdp3_res->size = 0;
	return -ENOMEM;
}

void mdp3_free(void)
{
	pr_debug("mdp3_fbmem_free\n");
	if (mdp3_res->ion_handle) {
		ion_unmap_kernel(mdp3_res->ion_client, mdp3_res->ion_handle);
		ion_free(mdp3_res->ion_client, mdp3_res->ion_handle);
		mdp3_res->ion_handle = NULL;
		mdp3_res->virt = NULL;
		mdp3_res->phys = 0;
		mdp3_res->size = 0;
	}
}

int mdp3_parse_dt_splash(struct msm_fb_data_type *mfd)
{
	struct platform_device *pdev = mfd->pdev;
	int rc;
	u32 offsets[2];

	rc = of_property_read_u32_array(pdev->dev.of_node,
				"qcom,memblock-reserve", offsets, 2);

	if (rc) {
		pr_err("fail to get memblock-reserve property\n");
		return rc;
	}

	if (mdp3_res->splash_mem_addr != offsets[0])
		rc = -EINVAL;

	mdp3_res->splash_mem_addr = offsets[0];
	mdp3_res->splash_mem_size = offsets[1];

	pr_debug("memaddr=%x size=%x\n", mdp3_res->splash_mem_addr,
		mdp3_res->splash_mem_size);

	return rc;
}

void mdp3_release_splash_memory(void)
{
	/* Give back the reserved memory to the system */
	if (mdp3_res->splash_mem_addr) {
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
	return mdp3_res->domains[MDP3_DMA_IOMMU_DOMAIN].domain_idx;
}

int mdp3_get_cont_spash_en(void)
{
	return mdp3_res->cont_splash_en;
}

int mdp3_continuous_splash_copy(struct mdss_panel_data *pdata)
{
	unsigned long splash_phys, phys;
	void *splash_virt, *virt;
	u32 height, width, rgb_size, stride;
	size_t size;
	int rc;

	if (pdata->panel_info.type != MIPI_VIDEO_PANEL) {
		pr_debug("cmd mode panel, no need to copy splash image\n");
		return 0;
	}

	rgb_size = MDP3_REG_READ(MDP3_REG_DMA_P_SIZE);
	stride = MDP3_REG_READ(MDP3_REG_DMA_P_IBUF_Y_STRIDE);
	stride = stride & 0x3FFF;
	splash_phys = MDP3_REG_READ(MDP3_REG_DMA_P_IBUF_ADDR);

	height = (rgb_size >> 16) & 0xffff;
	width  = rgb_size & 0xffff;
	size = PAGE_ALIGN(height * stride);
	pr_debug("splash_height=%d splash_width=%d Buffer size=%d\n",
		height, width, size);

	rc = mdp3_alloc(size, &virt, &phys);
	if (rc) {
		pr_err("fail to allocate memory for continuous splash image\n");
		return rc;
	}

	splash_virt = ioremap(splash_phys, stride * height);
	memcpy(virt, splash_virt, stride * height);
	iounmap(splash_virt);
	MDP3_REG_WRITE(MDP3_REG_DMA_P_IBUF_ADDR, phys);

	return 0;
}

static int mdp3_is_display_on(struct mdss_panel_data *pdata)
{
	int rc = 0;
	u32 status;

	mdp3_clk_update(MDP3_CLK_AHB, 1);
	mdp3_clk_update(MDP3_CLK_CORE, 1);

	if (pdata->panel_info.type == MIPI_VIDEO_PANEL) {
		status = MDP3_REG_READ(MDP3_REG_DSI_VIDEO_EN);
		rc = status & 0x1;
	} else {
		status = MDP3_REG_READ(MDP3_REG_DMA_P_CONFIG);
		status &= 0x180000;
		rc = (status == 0x080000);
	}

	mdp3_res->splash_mem_addr = MDP3_REG_READ(MDP3_REG_DMA_P_IBUF_ADDR);

	mdp3_clk_update(MDP3_CLK_AHB, 0);
	mdp3_clk_update(MDP3_CLK_CORE, 0);
	return rc;
}

static int mdp3_continuous_splash_on(struct mdss_panel_data *pdata)
{
	struct mdss_panel_info *panel_info = &pdata->panel_info;
	int ab, ib, rc;

	pr_debug("mdp3__continuous_splash_on\n");

	mdp3_clk_set_rate(MDP3_CLK_VSYNC, MDP_VSYNC_CLK_RATE,
			MDP3_CLIENT_DMA_P);

	rc = mdp3_clk_prepare();
	if (rc) {
		pr_err("fail to prepare clk\n");
		return rc;
	}

	rc = mdp3_clk_enable(1, 1);
	if (rc) {
		pr_err("fail to enable clk\n");
		mdp3_clk_unprepare();
		return rc;
	}

	ab = panel_info->xres * panel_info->yres * 4;
	ab *= panel_info->mipi.frame_rate;
	ib = (ab * 3) / 2;
	rc = mdp3_bus_scale_set_quota(MDP3_CLIENT_DMA_P, ab, ib);
	if (rc) {
		pr_err("fail to request bus bandwidth\n");
		goto splash_on_err;
	}

	rc = mdp3_ppp_init();
	if (rc) {
		pr_err("ppp init failed\n");
		goto splash_on_err;
	}

	mdp3_irq_register();

	if (pdata->event_handler) {
		rc = pdata->event_handler(pdata, MDSS_EVENT_CONT_SPLASH_BEGIN,
					NULL);
		if (rc) {
			pr_err("MDSS_EVENT_CONT_SPLASH_BEGIN event fail\n");
			goto splash_on_err;
		}
	}

	if (panel_info->type == MIPI_VIDEO_PANEL)
		mdp3_res->intf[MDP3_DMA_OUTPUT_SEL_DSI_VIDEO].active = 1;
	else
		mdp3_res->intf[MDP3_DMA_OUTPUT_SEL_DSI_CMD].active = 1;

	mdp3_batfet_ctrl(true);
	mdp3_res->cont_splash_en = 1;
	return 0;

splash_on_err:
	if (mdp3_clk_enable(0, 1))
		pr_err("%s: Unable to disable mdp3 clocks\n", __func__);

	mdp3_clk_unprepare();
	return rc;
}

static int mdp3_panel_register_done(struct mdss_panel_data *pdata)
{
	int rc = 0;

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
	return rc;
}

static int mdp3_debug_dump_stats(void *data, char *buf, int len)
{
	int total = 0;
	total = scnprintf(buf, len,"underrun: %08u\n",
			mdp3_res->underrun_cnt);
	return total;
}

static void mdp3_debug_enable_clock(int on)
{
	if (on) {
		mdp3_clk_prepare();
		mdp3_clk_enable(1, 0);
	} else {
		mdp3_clk_enable(0, 0);
		mdp3_clk_unprepare();
	}
}

static int mdp3_debug_init(struct platform_device *pdev)
{
	int rc;
	struct mdss_data_type *mdata;

	mdata = devm_kzalloc(&pdev->dev, sizeof(*mdata), GFP_KERNEL);
	if (!mdata)
		return -ENOMEM;

	mdss_res = mdata;

	mdata->debug_inf.debug_dump_stats = mdp3_debug_dump_stats;
	mdata->debug_inf.debug_enable_clock = mdp3_debug_enable_clock;

	rc = mdss_debugfs_init(mdata);
	if (rc)
		return rc;

	rc = mdss_debug_register_base(NULL, mdp3_res->mdp_base ,
					mdp3_res->mdp_reg_size);

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
	mdp3_res->underrun_cnt++;
	pr_debug("display underrun detected count=%d\n",
			mdp3_res->underrun_cnt);
}

static ssize_t mdp3_show_capabilities(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	size_t len = PAGE_SIZE;
	int cnt = 0;

#define SPRINT(fmt, ...) \
		(cnt += scnprintf(buf + cnt, len - cnt, fmt, ##__VA_ARGS__))

	SPRINT("mdp_version=3\n");
	SPRINT("hw_rev=%d\n", 304);
	SPRINT("dma_pipes=%d\n", 1);
	SPRINT("\n");

	return cnt;
}

static DEVICE_ATTR(caps, S_IRUGO, mdp3_show_capabilities, NULL);

static struct attribute *mdp3_fs_attrs[] = {
	&dev_attr_caps.attr,
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

static int mdp3_probe(struct platform_device *pdev)
{
	int rc;
	static struct msm_mdp_interface mdp3_interface = {
	.init_fnc = mdp3_init,
	.fb_mem_get_iommu_domain = mdp3_fb_mem_get_iommu_domain,
	.panel_register_done = mdp3_panel_register_done,
	.fb_stride = mdp3_fb_stride,
	};

	struct mdp3_intr_cb underrun_cb = {
		.cb = mdp3_dma_underrun_intr_handler,
		.data = NULL,
	};

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
	spin_lock_init(&mdp3_res->irq_lock);
	platform_set_drvdata(pdev, mdp3_res);

	rc = mdp3_parse_dt(pdev);
	if (rc)
		goto probe_done;

	rc = mdp3_res_init();
	if (rc) {
		pr_err("unable to initialize mdp3 resources\n");
		goto probe_done;
	}

	rc = mdp3_check_version();
	if (rc) {
		pr_err("mdp3 check version failed\n");
		goto probe_done;
	}

	rc = mdp3_debug_init(pdev);
	if (rc) {
		pr_err("unable to initialize mdp debugging\n");
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

probe_done:
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

int mdp3_panel_get_boot_cfg(void)
{
	int rc;

	if (!mdp3_res || !mdp3_res->pan_cfg.init_done)
		rc = -EPROBE_DEFER;
	if (mdp3_res->pan_cfg.lk_cfg)
		rc = 1;
	else
		rc = 0;
	return rc;
}

static  int mdp3_suspend_sub(struct mdp3_hw_resource *mdata)
{
	mdp3_batfet_ctrl(false);
	return 0;
}

static  int mdp3_resume_sub(struct mdp3_hw_resource *mdata)
{
	mdp3_batfet_ctrl(true);
	return 0;
}

static int mdp3_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct mdp3_hw_resource *mdata = platform_get_drvdata(pdev);

	if (!mdata)
		return -ENODEV;

	pr_debug("display suspend\n");

	return mdp3_suspend_sub(mdata);
}

static int mdp3_resume(struct platform_device *pdev)
{
	struct mdp3_hw_resource *mdata = platform_get_drvdata(pdev);

	if (!mdata)
		return -ENODEV;

	pr_debug("display resume\n");

	return mdp3_resume_sub(mdata);
}

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

module_init(mdp3_driver_init);
