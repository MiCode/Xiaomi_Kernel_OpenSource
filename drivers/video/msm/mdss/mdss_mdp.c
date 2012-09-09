/*
 * MDSS MDP Interface (used by framebuffer core)
 *
 * Copyright (c) 2007-2012, Code Aurora Forum. All rights reserved.
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

#include "mdss.h"
#include "mdss_fb.h"
#include "mdss_mdp.h"

struct mdss_data_type *mdss_res;

static DEFINE_SPINLOCK(mdp_lock);
static DEFINE_MUTEX(mdp_clk_lock);
static DEFINE_MUTEX(mdp_suspend_mutex);

u32 mdss_mdp_pipe_type_map[MDSS_MDP_MAX_SSPP] = {
	MDSS_MDP_PIPE_TYPE_VIG,
	MDSS_MDP_PIPE_TYPE_VIG,
	MDSS_MDP_PIPE_TYPE_VIG,
	MDSS_MDP_PIPE_TYPE_RGB,
	MDSS_MDP_PIPE_TYPE_RGB,
	MDSS_MDP_PIPE_TYPE_RGB,
	MDSS_MDP_PIPE_TYPE_DMA,
	MDSS_MDP_PIPE_TYPE_DMA,
};

u32 mdss_mdp_mixer_type_map[MDSS_MDP_MAX_LAYERMIXER] = {
	MDSS_MDP_MIXER_TYPE_INTF,
	MDSS_MDP_MIXER_TYPE_INTF,
	MDSS_MDP_MIXER_TYPE_INTF,
	MDSS_MDP_MIXER_TYPE_WRITEBACK,
	MDSS_MDP_MIXER_TYPE_WRITEBACK,
};

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
	.name = "mdss_mdp",
};

struct msm_iova_partition mdp_iommu_partitions[] = {
	{
		.start = SZ_128K,
		.size = SZ_2G - SZ_128K,
	},
};
struct msm_iova_layout mdp_iommu_layout = {
	.client_name = "mdss_mdp",
	.partitions = mdp_iommu_partitions,
	.npartitions = ARRAY_SIZE(mdp_iommu_partitions),
};

struct {
	char *name;
	struct device *ctx;
} mdp_iommu_ctx[] = {
	{
		.name = "mdp_0",
	},
	{
		.name = "mdp_1",
	}
};

struct mdss_hw mdss_mdp_hw = {
	.hw_ndx = MDSS_HW_MDP,
	.ptr = NULL,
	.irq_handler = mdss_mdp_isr,
};

static DEFINE_SPINLOCK(mdss_lock);
struct mdss_hw *mdss_irq_handlers[MDSS_MAX_HW_BLK];

static inline int mdss_irq_dispatch(u32 hw_ndx, int irq, void *ptr)
{
	struct mdss_hw *hw;

	spin_lock(&mdss_lock);
	hw = mdss_irq_handlers[hw_ndx];
	spin_unlock(&mdss_lock);
	if (hw)
		return hw->irq_handler(irq, hw->ptr);

	return -ENODEV;
}

static irqreturn_t mdss_irq_handler(int irq, void *ptr)
{
	struct mdss_data_type *mdata = ptr;
	u32 intr = MDSS_MDP_REG_READ(MDSS_REG_HW_INTR_STATUS);

	if (!mdata)
		return IRQ_NONE;

	mdata->irq_buzy = true;

	if (intr & MDSS_INTR_MDP)
		mdss_irq_dispatch(MDSS_HW_MDP, irq, ptr);

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


void mdss_enable_irq(struct mdss_hw *hw)
{
	unsigned long irq_flags;
	u32 ndx_bit;

	if (hw->hw_ndx >= MDSS_MAX_HW_BLK)
		return;

	ndx_bit = BIT(hw->hw_ndx);

	pr_debug("Enable HW=%d irq ena=%d mask=%x\n", hw->hw_ndx,
			mdss_res->irq_ena, mdss_res->irq_mask);

	spin_lock_irqsave(&mdss_lock, irq_flags);
	if (mdss_res->irq_mask & ndx_bit) {
		pr_debug("MDSS HW ndx=%d is already set, mask=%x\n",
				hw->hw_ndx, mdss_res->irq_mask);
	} else {
		mdss_irq_handlers[hw->hw_ndx] = hw;
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
		pr_warn("MDSS HW ndx=%d is NOT set, mask=%x\n",
			hw->hw_ndx, mdss_res->mdp_irq_mask);
	} else {
		mdss_irq_handlers[hw->hw_ndx] = NULL;
		mdss_res->irq_mask &= ~ndx_bit;
		if (mdss_res->irq_mask == 0) {
			mdss_res->irq_ena = false;
			disable_irq(mdss_res->irq);
		}
	}
	spin_unlock_irqrestore(&mdss_lock, irq_flags);
}
EXPORT_SYMBOL(mdss_disable_irq);

void mdss_disable_irq_nosync(struct mdss_hw *hw)
{
	u32 ndx_bit;

	if (hw->hw_ndx >= MDSS_MAX_HW_BLK)
		return;

	ndx_bit = BIT(hw->hw_ndx);

	pr_debug("Disable HW=%d irq ena=%d mask=%x\n", hw->hw_ndx,
			mdss_res->irq_ena, mdss_res->irq_mask);

	spin_lock(&mdss_lock);
	if (!(mdss_res->irq_mask & ndx_bit)) {
		pr_warn("MDSS HW ndx=%d is NOT set, mask=%x\n",
			hw->hw_ndx, mdss_res->mdp_irq_mask);
	} else {
		mdss_irq_handlers[hw->hw_ndx] = NULL;
		mdss_res->irq_mask &= ~ndx_bit;
		if (mdss_res->irq_mask == 0) {
			mdss_res->irq_ena = false;
			disable_irq_nosync(mdss_res->irq);
		}
	}
	spin_unlock(&mdss_lock);
}
EXPORT_SYMBOL(mdss_disable_irq_nosync);

static int mdss_mdp_bus_scale_register(struct mdss_data_type *mdata)
{
	if (!mdata->bus_hdl) {
		struct msm_bus_scale_pdata *bus_pdata = &mdp_bus_scale_table;
		int i;

		for (i = 0; i < bus_pdata->num_usecases; i++) {
			mdp_bus_usecases[i].num_paths = 1;
			mdp_bus_usecases[i].vectors = &mdp_bus_vectors[i];
		}

		mdata->bus_hdl = msm_bus_scale_register_client(bus_pdata);
		if (!mdata->bus_hdl) {
			pr_err("not able to get bus scale\n");
			return -ENOMEM;
		}

		pr_debug("register bus_hdl=%x\n", mdata->bus_hdl);
	}
	return 0;
}

static void mdss_mdp_bus_scale_unregister(struct mdss_data_type *mdata)
{
	pr_debug("unregister bus_hdl=%x\n", mdata->bus_hdl);

	if (mdata->bus_hdl)
		msm_bus_scale_unregister_client(mdata->bus_hdl);
}

int mdss_mdp_bus_scale_set_quota(u32 ab_quota, u32 ib_quota)
{
	static int current_bus_idx;
	int bus_idx;

	if (mdss_res->bus_hdl < 1) {
		pr_err("invalid bus handle %d\n", mdss_res->bus_hdl);
		return -EINVAL;
	}

	if ((ab_quota | ib_quota) == 0) {
		bus_idx = 0;
	} else {
		int num_cases = mdp_bus_scale_table.num_usecases;
		struct msm_bus_vectors *vect = NULL;

		bus_idx = (current_bus_idx % (num_cases - 1)) + 1;

		vect = mdp_bus_scale_table.usecase[current_bus_idx].vectors;
		if ((ab_quota == vect->ab) && (ib_quota == vect->ib)) {
			pr_debug("skip bus scaling, no change in vectors\n");
			return 0;
		}

		vect = mdp_bus_scale_table.usecase[bus_idx].vectors;
		vect->ab = ab_quota;
		vect->ib = ib_quota;

		pr_debug("bus scale idx=%d ab=%u ib=%u\n", bus_idx,
				vect->ab, vect->ib);
	}
	current_bus_idx = bus_idx;
	return msm_bus_scale_client_update_request(mdss_res->bus_hdl, bus_idx);
}

static inline u32 mdss_mdp_irq_mask(u32 intr_type, u32 intf_num)
{
	if (intr_type == MDSS_MDP_IRQ_INTF_UNDER_RUN ||
	    intr_type == MDSS_MDP_IRQ_INTF_VSYNC)
		intf_num = (intf_num - MDSS_MDP_INTF0) * 2;
	return 1 << (intr_type + intf_num);
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
		if (mdss_res->mdp_irq_mask == 0)
			mdss_disable_irq(&mdss_mdp_hw);
	}
	spin_unlock_irqrestore(&mdp_lock, irq_flags);
}

void mdss_mdp_irq_disable_nosync(u32 intr_type, u32 intf_num)
{
	u32 irq;

	irq = mdss_mdp_irq_mask(intr_type, intf_num);

	spin_lock(&mdp_lock);
	if (!(mdss_res->mdp_irq_mask & irq)) {
		pr_warn("MDSS MDP IRQ-%x is NOT set, mask=%x\n",
				irq, mdss_res->mdp_irq_mask);
	} else {
		mdss_res->mdp_irq_mask &= ~irq;
		MDSS_MDP_REG_WRITE(MDSS_MDP_REG_INTR_EN,
				mdss_res->mdp_irq_mask);
		if (mdss_res->mdp_irq_mask == 0)
			mdss_disable_irq_nosync(&mdss_mdp_hw);
	}
	spin_unlock(&mdp_lock);
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

void mdss_mdp_set_clk_rate(unsigned long min_clk_rate)
{
	unsigned long clk_rate;
	struct clk *clk = mdss_mdp_get_clk(MDSS_CLK_MDP_SRC);
	if (clk) {
		mutex_lock(&mdp_clk_lock);
		clk_rate = clk_round_rate(clk, min_clk_rate);
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

static void mdss_mdp_clk_ctrl_update(int enable)
{
	if (mdss_res->clk_ena == enable)
		return;

	pr_debug("MDP CLKS %s\n", (enable ? "Enable" : "Disable"));

	mutex_lock(&mdp_clk_lock);
	mdss_res->clk_ena = enable;
	mb();

	mdss_mdp_clk_update(MDSS_CLK_AHB, enable);
	mdss_mdp_clk_update(MDSS_CLK_AXI, enable);

	mdss_mdp_clk_update(MDSS_CLK_MDP_CORE, enable);
	mdss_mdp_clk_update(MDSS_CLK_MDP_LUT, enable);
	if (mdss_res->vsync_ena)
		mdss_mdp_clk_update(MDSS_CLK_MDP_VSYNC, enable);

	mutex_unlock(&mdp_clk_lock);
}

static void mdss_mdp_clk_ctrl_workqueue_handler(struct work_struct *work)
{
	mdss_mdp_clk_ctrl(MDP_BLOCK_MASTER_OFF, false);
}

void mdss_mdp_clk_ctrl(int enable, int isr)
{
	static atomic_t clk_ref = ATOMIC_INIT(0);
	static DEFINE_MUTEX(clk_ctrl_lock);
	int force_off = 0;

	pr_debug("clk enable=%d isr=%d clk_ref=%d\n", enable, isr,
			atomic_read(&clk_ref));
	/*
	 * It is assumed that if isr = TRUE then start = OFF
	 * if start = ON when isr = TRUE it could happen that the usercontext
	 * could turn off the clocks while the interrupt is updating the
	 * power to ON
	 */
	WARN_ON(isr == true && enable);

	if (enable == MDP_BLOCK_POWER_ON) {
		atomic_inc(&clk_ref);
	} else if (!atomic_add_unless(&clk_ref, -1, 0)) {
		if (enable == MDP_BLOCK_MASTER_OFF) {
			pr_debug("master power-off req\n");
			force_off = 1;
		} else {
			WARN(1, "too many mdp clock off call\n");
		}
	}

	WARN_ON(enable == MDP_BLOCK_MASTER_OFF && !force_off);

	if (isr) {
		/* if it's power off send workqueue to turn off clocks */
		if (mdss_res->clk_ena && !atomic_read(&clk_ref))
			queue_delayed_work(mdss_res->clk_ctrl_wq,
					   &mdss_res->clk_ctrl_worker,
					   mdss_res->timeout);
	} else {
		mutex_lock(&clk_ctrl_lock);
		if (delayed_work_pending(&mdss_res->clk_ctrl_worker))
			cancel_delayed_work(&mdss_res->clk_ctrl_worker);

		if (atomic_read(&clk_ref)) {
			mdss_mdp_clk_ctrl_update(true);
		} else if (mdss_res->clk_ena) {
			mutex_lock(&mdp_suspend_mutex);
			if (force_off || mdss_res->suspend) {
				mdss_mdp_clk_ctrl_update(false);
			} else {
				/* send workqueue to turn off mdp power */
				queue_delayed_work(mdss_res->clk_ctrl_wq,
						   &mdss_res->clk_ctrl_worker,
						   mdss_res->timeout);
			}
			mutex_unlock(&mdp_suspend_mutex);
		}
		mutex_unlock(&clk_ctrl_lock);
	}
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
	regulator_enable(mdata->fs);
	mdata->fs_ena = true;

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

static int mdss_iommu_fault_handler(struct iommu_domain *domain,
		struct device *dev, unsigned long iova, int flags)
{
	pr_err("MDP IOMMU page fault: iova 0x%lx\n", iova);
	return 0;
}

int mdss_iommu_attach(void)
{
	struct iommu_domain *domain;
	int i, domain_idx;

	if (mdss_res->iommu_attached) {
		pr_warn("mdp iommu already attached\n");
		return 0;
	}

	domain_idx = mdss_get_iommu_domain();
	domain = msm_get_iommu_domain(domain_idx);
	if (!domain) {
		pr_err("unable to get iommu domain(%d)\n", domain_idx);
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(mdp_iommu_ctx); i++) {
		if (iommu_attach_device(domain, mdp_iommu_ctx[i].ctx)) {
			WARN(1, "could not attach iommu domain %d to ctx %s\n",
				domain_idx, mdp_iommu_ctx[i].name);
			return -EINVAL;
		}
	}
	mdss_res->iommu_attached = true;

	return 0;
}

int mdss_iommu_dettach(void)
{
	struct iommu_domain *domain;
	int i, domain_idx;

	if (!mdss_res->iommu_attached) {
		pr_warn("mdp iommu already dettached\n");
		return 0;
	}

	domain_idx = mdss_get_iommu_domain();
	domain = msm_get_iommu_domain(domain_idx);
	if (!domain) {
		pr_err("unable to get iommu domain(%d)\n", domain_idx);
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(mdp_iommu_ctx); i++)
		iommu_detach_device(domain, mdp_iommu_ctx[i].ctx);
	mdss_res->iommu_attached = false;

	return 0;
}

int mdss_iommu_init(void)
{
	struct iommu_domain *domain;
	int domain_idx, i;

	domain_idx = msm_register_domain(&mdp_iommu_layout);
	if (IS_ERR_VALUE(domain_idx))
		return -EINVAL;

	domain = msm_get_iommu_domain(domain_idx);
	if (!domain) {
		pr_err("unable to get iommu domain(%d)\n", domain_idx);
		return -EINVAL;
	}

	iommu_set_fault_handler(domain, mdss_iommu_fault_handler);

	for (i = 0; i < ARRAY_SIZE(mdp_iommu_ctx); i++) {
		mdp_iommu_ctx[i].ctx = msm_iommu_get_ctx(mdp_iommu_ctx[i].name);
		if (!mdp_iommu_ctx[i].ctx) {
			pr_warn("unable to get iommu ctx(%s)\n",
					mdp_iommu_ctx[i].name);
			return -EINVAL;
		}
	}
	mdss_res->iommu_domain = domain_idx;

	return 0;
}

static int mdss_hw_init(struct mdss_data_type *mdata)
{
	char *base = mdata->vbif_base;

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);
	/* Setup VBIF QoS settings*/
	MDSS_MDP_REG_WRITE(0x2E0, 0x000000AA);
	MDSS_MDP_REG_WRITE(0x2E4, 0x00000055);
	writel_relaxed(0x00000001, base + 0x004);
	writel_relaxed(0x00000707, base + 0x0D8);
	writel_relaxed(0x00000030, base + 0x0F0);
	writel_relaxed(0x00000001, base + 0x124);
	writel_relaxed(0x00000FFF, base + 0x178);
	writel_relaxed(0x0FFF0FFF, base + 0x17C);
	writel_relaxed(0x22222222, base + 0x160);
	writel_relaxed(0x00002222, base + 0x164);
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
	pr_debug("MDP hw init done\n");

	return 0;
}

static u32 mdss_mdp_res_init(struct mdss_data_type *mdata)
{
	u32 rc = 0;

	rc = mdss_mdp_irq_clk_setup(mdata);
	if (rc)
		return rc;

	mdata->clk_ctrl_wq = create_singlethread_workqueue("mdp_clk_wq");
	INIT_DELAYED_WORK(&mdata->clk_ctrl_worker,
			  mdss_mdp_clk_ctrl_workqueue_handler);

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);
	mdata->rev = MDSS_MDP_REG_READ(MDSS_REG_HW_VERSION);
	mdata->mdp_rev = MDSS_MDP_REG_READ(MDSS_MDP_REG_HW_VERSION);
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);

	mdata->smp_mb_cnt = MDSS_MDP_SMP_MMB_BLOCKS;
	mdata->smp_mb_size = MDSS_MDP_SMP_MMB_SIZE;
	mdata->pipe_type_map = mdss_mdp_pipe_type_map;
	mdata->mixer_type_map = mdss_mdp_mixer_type_map;

	pr_info("mdss_revision=%x\n", mdata->rev);
	pr_info("mdp_hw_revision=%x\n", mdata->mdp_rev);

	mdata->res_init = true;
	mdata->timeout = HZ/20;
	mdata->clk_ena = false;
	mdata->irq_mask = MDSS_MDP_DEFAULT_INTR_MASK;
	mdata->suspend = false;
	mdata->prim_ptype = NO_PANEL;
	mdata->irq_ena = false;

	mdata->iclient = msm_ion_client_create(-1, mdata->pdev->name);
	if (IS_ERR_OR_NULL(mdata->iclient)) {
		pr_err("msm_ion_client_create() return error (%p)\n",
				mdata->iclient);
		mdata->iclient = NULL;
	}

	rc = mdss_iommu_init();
	if (!IS_ERR_VALUE(rc))
		mdss_iommu_attach();

	rc = mdss_hw_init(mdata);

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

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mdp_phys");
	if (!res) {
		pr_err("unable to get MDP base address\n");
		rc = -ENOMEM;
		goto probe_done;
	}

	mdata->mdp_base = devm_ioremap(&pdev->dev, res->start,
				       resource_size(res));
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

	rc = mdss_mdp_res_init(mdata);
	if (rc) {
		pr_err("unable to initialize mdss mdp resources\n");
		goto probe_done;
	}
	rc = mdss_mdp_bus_scale_register(mdata);
probe_done:
	if (IS_ERR_VALUE(rc))
		mdss_res = NULL;

	return rc;
}

void mdss_mdp_footswitch_ctrl(int on)
{
	mutex_lock(&mdp_suspend_mutex);
	if (!mdss_res->suspend || mdss_res->eintf_ena || !mdss_res->fs) {
		mutex_unlock(&mdp_suspend_mutex);
		return;
	}

	if (on && !mdss_res->fs_ena) {
		pr_debug("Enable MDP FS\n");
		regulator_enable(mdss_res->fs);
		mdss_iommu_attach();
		mdss_res->fs_ena = true;
	} else if (!on && mdss_res->fs_ena) {
		pr_debug("Disable MDP FS\n");
		mdss_iommu_dettach();
		regulator_disable(mdss_res->fs);
		mdss_res->fs_ena = false;
	}
	mutex_unlock(&mdp_suspend_mutex);
}

#ifdef CONFIG_PM
static void mdss_mdp_suspend_sub(void)
{
	cancel_delayed_work(&mdss_res->clk_ctrl_worker);

	flush_workqueue(mdss_res->clk_ctrl_wq);

	mdss_mdp_clk_ctrl(MDP_BLOCK_MASTER_OFF, false);

	mutex_lock(&mdp_suspend_mutex);
	mdss_res->suspend = true;
	mutex_unlock(&mdp_suspend_mutex);
}

static int mdss_mdp_suspend(struct platform_device *pdev, pm_message_t state)
{
	int ret;
	pr_debug("display suspend");

	ret = mdss_fb_suspend_all();
	if (IS_ERR_VALUE(ret)) {
		pr_err("Unable to suspend all fb panels (%d)\n", ret);
		return ret;
	}
	mdss_mdp_suspend_sub();
	if (mdss_res->clk_ena) {
		pr_err("MDP suspend failed\n");
		return -EBUSY;
	}
	mdss_mdp_footswitch_ctrl(false);

	return 0;
}

static int mdss_mdp_resume(struct platform_device *pdev)
{
	struct mdss_data_type *mdata = platform_get_drvdata(pdev);
	int ret = 0;

	if (!mdata)
		return -ENODEV;

	pr_debug("resume display");

	mdss_mdp_footswitch_ctrl(true);
	mutex_lock(&mdp_suspend_mutex);
	mdss_res->suspend = false;
	mutex_unlock(&mdp_suspend_mutex);
	ret = mdss_fb_resume_all();
	if (IS_ERR_VALUE(ret))
		pr_err("Unable to resume all fb panels (%d)\n", ret);

	mdss_hw_init(mdata);
	return ret;
}
#else
#define mdss_mdp_suspend NULL
#define mdss_mdp_resume NULL
#endif

static int mdss_mdp_remove(struct platform_device *pdev)
{
	struct mdss_data_type *mdata = platform_get_drvdata(pdev);
	if (!mdata)
		return -ENODEV;
	pm_runtime_disable(&pdev->dev);
	mdss_mdp_bus_scale_unregister(mdata);
	return 0;
}

static const struct of_device_id mdss_mdp_dt_match[] = {
	{ .compatible = "qcom,mdss_mdp",},
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
