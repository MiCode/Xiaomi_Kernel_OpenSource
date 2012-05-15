/*
 * MDSS MDP Interface (used by framebuffer core)
 *
 * Copyright (c) 2007-2012, The Linux Foundation. All rights reserved.
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
#include <linux/of.h>
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

#include "mdss.h"
#include "mdss_fb.h"
#include "mdss_mdp.h"

/* 1.15 mdp clk factor */
#define MDP_CLK_FACTOR(rate) (((rate) * 23) / 20)

unsigned char *mdss_reg_base;

struct mdss_res_type *mdss_res;
static struct msm_panel_common_pdata *mdp_pdata;

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

irqreturn_t mdss_irq_handler(int mdss_irq, void *ptr)
{
	u32 intr = MDSS_MDP_REG_READ(MDSS_REG_HW_INTR_STATUS);

	mdss_res->irq_buzy = true;

	if (intr & MDSS_INTR_MDP)
		mdss_mdp_isr(mdss_irq, ptr);

	mdss_res->irq_buzy = false;

	return IRQ_HANDLED;
}

int mdss_mdp_irq_enable(u32 intr_type, u32 intf_num)
{
	u32 irq;
	unsigned long irq_flags;
	int ret = 0;

	if (intr_type == MDSS_MDP_IRQ_INTF_UNDER_RUN ||
	    intr_type == MDSS_MDP_IRQ_INTF_VSYNC)
		intf_num = intf_num << 1;

	irq =  BIT(intr_type + intf_num);

	spin_lock_irqsave(&mdp_lock, irq_flags);
	if (mdss_res->irq_mask & irq) {
		pr_warn("MDSS IRQ-0x%x is already set, mask=%x irq=%d\n",
			irq, mdss_res->irq_mask, mdss_res->irq_ena);
		ret = -EBUSY;
	} else {
		mdss_res->irq_mask |= irq;
		MDSS_MDP_REG_WRITE(MDSS_MDP_REG_INTR_CLEAR, irq);
		MDSS_MDP_REG_WRITE(MDSS_MDP_REG_INTR_EN, mdss_res->irq_mask);
		if (!mdss_res->irq_ena) {
			mdss_res->irq_ena = true;
			enable_irq(mdss_res->irq);
		}
	}
	spin_unlock_irqrestore(&mdp_lock, irq_flags);

	return ret;
}

void mdss_mdp_irq_disable(u32 intr_type, u32 intf_num)
{
	u32 irq;
	unsigned long irq_flags;


	if (intr_type == MDSS_MDP_IRQ_INTF_UNDER_RUN ||
	    intr_type == MDSS_MDP_IRQ_INTF_VSYNC)
		intf_num = intf_num << 1;

	irq = BIT(intr_type + intf_num);

	spin_lock_irqsave(&mdp_lock, irq_flags);
	if (!(mdss_res->irq_mask & irq)) {
		pr_warn("MDSS IRQ-%x is NOT set, mask=%x irq=%d\n",
			irq, mdss_res->irq_mask, mdss_res->irq_ena);
	} else {
		mdss_res->irq_mask &= ~irq;
		MDSS_MDP_REG_WRITE(MDSS_MDP_REG_INTR_EN, mdss_res->irq_mask);
		if (!mdss_res->irq_mask) {
			mdss_res->irq_ena = false;
			disable_irq(mdss_res->irq);
		}
	}
	spin_unlock_irqrestore(&mdp_lock, irq_flags);
}

void mdss_mdp_irq_disable_nosync(u32 intr_type, u32 intf_num)
{
	u32 irq;

	if (intr_type == MDSS_MDP_IRQ_INTF_UNDER_RUN ||
	    intr_type == MDSS_MDP_IRQ_INTF_VSYNC)
		intf_num = intf_num << 1;

	irq = BIT(intr_type + intf_num);

	spin_lock(&mdp_lock);
	if (!(mdss_res->irq_mask & irq)) {
		pr_warn("MDSS IRQ-%x is NOT set, mask=%x irq=%d\n",
			irq, mdss_res->irq_mask, mdss_res->irq_ena);
	} else {
		mdss_res->irq_mask &= ~irq;
		MDSS_MDP_REG_WRITE(MDSS_MDP_REG_INTR_EN, mdss_res->irq_mask);
		if (!mdss_res->irq_mask) {
			mdss_res->irq_ena = false;
			disable_irq_nosync(mdss_res->irq);
		}
	}
	spin_unlock(&mdp_lock);
}

static void mdss_mdp_clk_ctrl_update(int enable)
{
	if (mdss_res->clk_ena == enable)
		return;

	pr_debug("MDP CLKS %s\n", (enable ? "Enable" : "Disable"));
	mdss_res->clk_ena = enable;
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

	if (enable) {
		atomic_inc(&clk_ref);
	} else if (!atomic_add_unless(&clk_ref, -1, 0)) {
		pr_debug("master power-off req\n");
		force_off = 1;
	}

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

static void mdss_mdp_clk_ctrl_workqueue_handler(struct work_struct *work)
{
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
}

static int mdss_mdp_irq_clk_setup(void)
{
	int ret;

	ret = request_irq(mdss_res->irq, mdss_irq_handler, IRQF_DISABLED,
			  "MDSS", 0);
	if (ret) {
		pr_err("mdp request_irq() failed!\n");
		return ret;
	}
	disable_irq(mdss_res->irq);

	mdss_res->fs = regulator_get(NULL, "fs_mdp");
	if (IS_ERR(mdss_res->fs))
		mdss_res->fs = NULL;
	else {
		regulator_enable(mdss_res->fs);
		mdss_res->fs_ena = true;
	}
	regulator_enable(mdss_res->fs);

	return 0;
}

static struct msm_panel_common_pdata *mdss_mdp_populate_pdata(
	struct device *dev)
{
	struct msm_panel_common_pdata *pdata;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		dev_err(dev, "could not allocate memory for pdata\n");
	return pdata;
}

static u32 mdss_mdp_res_init(void)
{
	u32 rc;

	rc = mdss_mdp_irq_clk_setup();
	if (rc)
		return rc;

	mdss_res->clk_ctrl_wq = create_singlethread_workqueue("mdp_clk_wq");
	INIT_DELAYED_WORK(&mdss_res->clk_ctrl_worker,
			  mdss_mdp_clk_ctrl_workqueue_handler);

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);
	mdss_res->rev = MDSS_MDP_REG_READ(MDSS_REG_HW_VERSION);
	mdss_res->mdp_rev = MDSS_MDP_REG_READ(MDSS_MDP_REG_HW_VERSION);
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);

	mdss_res->smp_mb_cnt = MDSS_MDP_SMP_MMB_BLOCKS;
	mdss_res->smp_mb_size = MDSS_MDP_SMP_MMB_SIZE;
	mdss_res->pipe_type_map = mdss_mdp_pipe_type_map;
	mdss_res->mixer_type_map = mdss_mdp_mixer_type_map;

	pr_info("mdss_revision=%x\n", mdss_res->rev);
	pr_info("mdp_hw_revision=%x\n", mdss_res->mdp_rev);

	mdss_res->res_init = true;
	mdss_res->timeout = HZ/20;
	mdss_res->clk_ena = false;
	mdss_res->irq_mask = MDSS_MDP_DEFAULT_INTR_MASK;
	mdss_res->suspend = false;
	mdss_res->prim_ptype = NO_PANEL;
	mdss_res->irq_ena = false;

	return 0;
}

static int mdss_mdp_probe(struct platform_device *pdev)
{
	struct resource *mdss_mdp_mres;
	struct resource *mdss_mdp_ires;
	resource_size_t size;
	int rc;

	if (!mdss_res) {
		mdss_res = devm_kzalloc(&pdev->dev, sizeof(*mdss_res),
				GFP_KERNEL);
		if (mdss_res == NULL)
			return -ENOMEM;
	}

	if (pdev->dev.of_node) {
		pdev->id = 0;
		mdp_pdata = mdss_mdp_populate_pdata(&pdev->dev);
		mdss_mdp_mres = platform_get_resource(pdev,
						IORESOURCE_MEM, 0);
		mdss_mdp_ires = platform_get_resource(pdev,
						IORESOURCE_IRQ, 0);
		if (!mdss_mdp_mres || !mdss_mdp_ires) {
			pr_err("unable to get the MDSS resources");
			rc = -ENOMEM;
			goto probe_done;
		}
		mdss_reg_base = ioremap(mdss_mdp_mres->start,
					resource_size(mdss_mdp_mres));

		pr_info("MDP HW Base phy_Address=0x%x virt=0x%x\n",
			(int) mdss_mdp_mres->start,
			(int) mdss_reg_base);

		mdss_res->irq = mdss_mdp_ires->start;
	} else if ((pdev->id == 0) && (pdev->num_resources > 0)) {
		mdp_pdata = pdev->dev.platform_data;

		size =  resource_size(&pdev->resource[0]);
		mdss_reg_base = ioremap(pdev->resource[0].start, size);

		pr_info("MDP HW Base phy_Address=0x%x virt=0x%x\n",
			(int) pdev->resource[0].start,
			(int) mdss_reg_base);

		mdss_res->irq = platform_get_irq(pdev, 0);
		if (mdss_res->irq < 0) {
			pr_err("can not get mdp irq\n");
			rc = -ENOMEM;
			goto probe_done;
		}
	}

	if (unlikely(!mdss_reg_base)) {
		rc = -ENOMEM;
		goto probe_done;
	}

	rc = mdss_mdp_res_init();
	if (rc) {
		pr_err("unable to initialize mdss mdp resources\n");
		goto probe_done;
	}

probe_done:
	if (IS_ERR_VALUE(rc)) {
		if (mdss_res) {
			devm_kfree(&pdev->dev, mdss_res);
			mdss_res = NULL;
		}
	}

	return rc;
}

void mdss_mdp_footswitch_ctrl(int on)
{
	int ret;

	mutex_lock(&mdp_suspend_mutex);
	if (!mdss_res->suspend || mdss_res->eintf_ena || !mdss_res->fs) {
		mutex_unlock(&mdp_suspend_mutex);
		return;
	}

	if (on && !mdss_res->fs_ena) {
		pr_debug("Enable MDP FS\n");
		ret = regulator_enable(mdss_res->fs);
		if (ret)
			pr_err("Footswitch failed to enable\n");
		mdss_res->fs_ena = true;
	} else if (!on && mdss_res->fs_ena) {
		pr_debug("Disable MDP FS\n");
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

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);

	mutex_lock(&mdp_suspend_mutex);
	mdss_res->suspend = true;
	mutex_unlock(&mdp_suspend_mutex);
}

static int mdss_mdp_suspend(struct platform_device *pdev, pm_message_t state)
{
	if (pdev->id == 0) {
		mdss_mdp_suspend_sub();
		if (mdss_res->clk_ena) {
			pr_err("MDP suspend failed\n");
			return -EBUSY;
		}
		mdss_mdp_footswitch_ctrl(false);
	}
	return 0;
}

static int mdss_mdp_resume(struct platform_device *pdev)
{
	mdss_mdp_footswitch_ctrl(true);
	mutex_lock(&mdp_suspend_mutex);
	mdss_res->suspend = false;
	mutex_unlock(&mdp_suspend_mutex);
	return 0;
}
#else
#define mdss_mdp_suspend NULL
#define mdss_mdp_resume NULL
#endif

static int mdss_mdp_remove(struct platform_device *pdev)
{
	if (mdss_res->fs != NULL)
		regulator_put(mdss_res->fs);
	iounmap(mdss_reg_base);
	pm_runtime_disable(&pdev->dev);
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
