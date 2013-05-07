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

#define MDP_CORE_HW_VERSION	0x03040310
struct mdp3_hw_resource *mdp3_res;

#define MDP_BUS_VECTOR_ENTRY(ab_val, ib_val)		\
	{						\
		.src = MSM_BUS_MASTER_MDP_PORT0,	\
		.dst = MSM_BUS_SLAVE_EBI_CH0,		\
		.ab = (ab_val),				\
		.ib = (ib_val),				\
	}

static struct msm_bus_vectors mdp_bus_vectors[] = {
	MDP_BUS_VECTOR_ENTRY(0, 0),
	MDP_BUS_VECTOR_ENTRY(SZ_128M, SZ_256M),
	MDP_BUS_VECTOR_ENTRY(SZ_256M, SZ_512M),
};

static struct msm_bus_paths mdp_bus_usecases[ARRAY_SIZE(mdp_bus_vectors)];

static struct msm_bus_scale_pdata mdp_bus_scale_table = {
	.usecase = mdp_bus_usecases,
	.num_usecases = ARRAY_SIZE(mdp_bus_usecases),
	.name = "mdp3",
};

struct mdp3_iommu_domain_map mdp3_iommu_domains[MDP3_IOMMU_DOMAIN_MAX] = {
	[MDP3_IOMMU_DOMAIN] = {
		.domain_type = MDP3_IOMMU_DOMAIN,
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
		.domain = &mdp3_iommu_domains[MDP3_IOMMU_DOMAIN],
		.ctx_name = "mdpe_0",
		.attached = 0,
	},
	[MDP3_IOMMU_CTX_PPP_1] = {
		.ctx_type = MDP3_IOMMU_CTX_PPP_1,
		.domain = &mdp3_iommu_domains[MDP3_IOMMU_DOMAIN],
		.ctx_name = "mdpe_1",
		.attached = 0,
	},

	[MDP3_IOMMU_CTX_DMA_0] = {
		.ctx_type = MDP3_IOMMU_CTX_DMA_0,
		.domain = &mdp3_iommu_domains[MDP3_IOMMU_DOMAIN],
		.ctx_name = "mdps_0",
		.attached = 0,
	},

	[MDP3_IOMMU_CTX_DMA_1] = {
		.ctx_type = MDP3_IOMMU_CTX_DMA_1,
		.domain = &mdp3_iommu_domains[MDP3_IOMMU_DOMAIN],
		.ctx_name = "mdps_1",
		.attached = 0,
	},
};

static irqreturn_t mdp3_irq_handler(int irq, void *ptr)
{
	int i = 0;
	struct mdp3_hw_resource *mdata = (struct mdp3_hw_resource *)ptr;
	u32 mdp_interrupt = MDP3_REG_READ(MDP3_REG_INTR_STATUS);

	MDP3_REG_WRITE(MDP3_REG_INTR_CLEAR, mdp_interrupt);
	pr_debug("mdp3_irq_handler irq=%d\n", mdp_interrupt);

	spin_lock(&mdata->irq_lock);
	mdp_interrupt &= mdata->irqMask;

	while (mdp_interrupt && i < MDP3_MAX_INTR) {
		if ((mdp_interrupt & 0x1) && mdata->callbacks[i].cb)
			mdata->callbacks[i].cb(i, mdata->callbacks[i].data);
		mdp_interrupt = mdp_interrupt >> 1;
		i++;
	}
	spin_unlock(&mdata->irq_lock);

	return IRQ_HANDLED;
}

void mdp3_irq_enable(int type)
{
	unsigned long flag;
	int irqEnabled = 0;

	pr_debug("mdp3_irq_enable type=%d\n", type);
	spin_lock_irqsave(&mdp3_res->irq_lock, flag);
	if (mdp3_res->irqMask & BIT(type)) {
		pr_debug("interrupt %d already enabled\n", type);
		spin_unlock_irqrestore(&mdp3_res->irq_lock, flag);
		return;
	}
	irqEnabled = mdp3_res->irqMask;
	mdp3_res->irqMask |= BIT(type);
	MDP3_REG_WRITE(MDP3_REG_INTR_ENABLE, mdp3_res->irqMask);
	if (!irqEnabled)
		enable_irq(mdp3_res->irq);
	spin_unlock_irqrestore(&mdp3_res->irq_lock, flag);
}

void mdp3_irq_disable(int type)
{
	unsigned long flag;

	spin_lock_irqsave(&mdp3_res->irq_lock, flag);
	if (mdp3_res->irqMask & BIT(type)) {
		mdp3_res->irqMask &= ~BIT(type);
		MDP3_REG_WRITE(MDP3_REG_INTR_ENABLE, mdp3_res->irqMask);
		if (!mdp3_res->irqMask)
			disable_irq(mdp3_res->irq);
	} else {
		pr_debug("interrupt %d not enabled\n", type);
	}
	spin_unlock_irqrestore(&mdp3_res->irq_lock, flag);
}

void mdp3_irq_disable_nosync(int type)
{
	if (mdp3_res->irqMask & BIT(type)) {
		mdp3_res->irqMask &= ~BIT(type);
		MDP3_REG_WRITE(MDP3_REG_INTR_ENABLE, mdp3_res->irqMask);
		if (!mdp3_res->irqMask)
			disable_irq_nosync(mdp3_res->irq);
	} else {
		pr_debug("interrupt %d not enabled\n", type);
	}
}

int mdp3_set_intr_callback(u32 type, struct mdp3_intr_cb *cb)
{
	unsigned long flag;

	pr_debug("interrupt %d callback n", type);
	spin_lock_irqsave(&mdp3_res->irq_lock, flag);
	if (cb)
		mdp3_res->callbacks[type] = *cb;
	else
		mdp3_res->callbacks[type].cb = NULL;

	spin_unlock_irqrestore(&mdp3_res->irq_lock, flag);
	return 0;
}

static int mdp3_bus_scale_register(void)
{
	if (!mdp3_res->bus_handle) {
		struct msm_bus_scale_pdata *bus_pdata = &mdp_bus_scale_table;
		int i;

		for (i = 0; i < bus_pdata->num_usecases; i++) {
			mdp_bus_usecases[i].num_paths = 1;
			mdp_bus_usecases[i].vectors = &mdp_bus_vectors[i];
		}

		mdp3_res->bus_handle = msm_bus_scale_register_client(bus_pdata);
		if (!mdp3_res->bus_handle) {
			pr_err("not able to get bus scale\n");
			return -ENOMEM;
		}
		pr_debug("register bus_hdl=%x\n", mdp3_res->bus_handle);
	}
	return 0;
}

static void mdp3_bus_scale_unregister(void)
{
	pr_debug("unregister bus_handle=%x\n", mdp3_res->bus_handle);

	if (mdp3_res->bus_handle)
		msm_bus_scale_unregister_client(mdp3_res->bus_handle);
}

int mdp3_bus_scale_set_quota(int client, u64 ab_quota, u64 ib_quota)
{
	static int current_bus_idx;
	int bus_idx;
	int rc;

	if (mdp3_res->bus_handle < 1) {
		pr_err("invalid bus handle %d\n", mdp3_res->bus_handle);
		return -EINVAL;
	}

	if ((ab_quota | ib_quota) == 0) {
		bus_idx = 0;
	} else {
		int num_cases = mdp_bus_scale_table.num_usecases;
		struct msm_bus_vectors *vect = NULL;

		bus_idx = (current_bus_idx % (num_cases - 1)) + 1;

		/* aligning to avoid performing updates for small changes */
		ab_quota = ALIGN(ab_quota, SZ_64M);
		ib_quota = ALIGN(ib_quota, SZ_64M);

		vect = mdp_bus_scale_table.usecase[current_bus_idx].vectors;
		if ((ab_quota == vect->ab) && (ib_quota == vect->ib)) {
			pr_debug("skip bus scaling, no change in vectors\n");
			return 0;
		}

		vect = mdp_bus_scale_table.usecase[bus_idx].vectors;
		vect->ab = ab_quota;
		vect->ib = ib_quota;

		pr_debug("bus scale idx=%d ab=%llu ib=%llu\n", bus_idx,
				vect->ab, vect->ib);
	}
	current_bus_idx = bus_idx;
	rc = msm_bus_scale_client_update_request(mdp3_res->bus_handle, bus_idx);
	return rc;
}

static int mdp3_clk_update(u32 clk_idx, u32 enable)
{
	int ret = -EINVAL;
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
	if (count == 1) {
		pr_debug("clk=%d en=%d\n", clk_idx, enable);
		ret = clk_prepare_enable(clk);
	} else if (count == 0) {
		pr_debug("clk=%d disable\n", clk_idx);
		clk_disable_unprepare(clk);
		ret = 0;
	} else if (count < 0) {
		pr_err("clk=%d count=%d\n", clk_idx, count);
		ret = -EINVAL;
	}
	return ret;
}



int mdp3_clk_set_rate(int clk_type, unsigned long clk_rate)
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
	clk_put(mdp3_res->clocks[MDP3_CLK_AHB]);
	clk_put(mdp3_res->clocks[MDP3_CLK_CORE]);
	clk_put(mdp3_res->clocks[MDP3_CLK_VSYNC]);
	clk_put(mdp3_res->clocks[MDP3_CLK_LCDC]);
	clk_put(mdp3_res->clocks[MDP3_CLK_DSI]);
}

int mdp3_clk_enable(int enable)
{
	int rc;

	pr_debug("MDP CLKS %s\n", (enable ? "Enable" : "Disable"));

	mutex_lock(&mdp3_res->res_mutex);
	rc = mdp3_clk_update(MDP3_CLK_AHB, enable);
	rc |= mdp3_clk_update(MDP3_CLK_CORE, enable);
	rc |= mdp3_clk_update(MDP3_CLK_VSYNC, enable);
	rc |= mdp3_clk_update(MDP3_CLK_DSI, enable);
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
	return 0;
}

static int mdp3_iommu_fault_handler(struct iommu_domain *domain,
		struct device *dev, unsigned long iova, int flags, void *token)
{
	pr_err("MDP IOMMU page fault: iova 0x%lx\n", iova);
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

	if (context >= MDP3_IOMMU_CTX_MAX)
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
		if (!mdp3_iommu_domains[i].domain) {
			pr_err("unable to get iommu domain(%d)\n",
				domain_idx);
			return -EINVAL;
		}
		iommu_set_fault_handler(mdp3_iommu_domains[i].domain,
					mdp3_iommu_fault_handler,
					NULL);
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

		if (!mdp3_iommu_contexts[i].ctx) {
			pr_warn("unable to get iommu ctx(%s)\n",
				mdp3_iommu_contexts[i].ctx_name);
			return -EINVAL;
		}
	}

	mdp3_res->iommu_contexts = mdp3_iommu_contexts;

	return 0;
}

int mdp3_iommu_init(void)
{
	int ret;

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

	rc = mdp3_iommu_attach(MDP3_IOMMU_CTX_DMA_0);
	if (rc) {
		pr_err("fail to attach DMA-P context 0\n");
		return rc;
	}
	rc = mdp3_bus_scale_register();
	if (rc) {
		pr_err("unable to register bus scaling\n");
		return rc;
	}

	rc = mdp3_hw_init();

	return rc;
}

static int mdp3_parse_dt(struct platform_device *pdev)
{
	struct resource *res;

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

	return 0;
}

static int mdp3_init(struct msm_fb_data_type *mfd)
{
	return mdp3_ctrl_init(mfd);
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
	return mdp3_res->domains[MDP3_IOMMU_DOMAIN].domain_idx;
}

static int mdp3_probe(struct platform_device *pdev)
{
	int rc;
	static struct msm_mdp_interface mdp3_interface = {
	.init_fnc = mdp3_init,
	.fb_mem_get_iommu_domain = mdp3_fb_mem_get_iommu_domain,
	.fb_stride = mdp3_fb_stride,
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

	rc = mdss_fb_register_mdp_instance(&mdp3_interface);
	if (rc)
		pr_err("unable to register mdp instance\n");

probe_done:
	if (IS_ERR_VALUE(rc)) {
		devm_kfree(&pdev->dev, mdp3_res);
		mdp3_res = NULL;
	}

	return rc;
}

static  int mdp3_suspend_sub(struct mdp3_hw_resource *mdata)
{
	return 0;
}

static  int mdp3_resume_sub(struct mdp3_hw_resource *mdata)
{
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
