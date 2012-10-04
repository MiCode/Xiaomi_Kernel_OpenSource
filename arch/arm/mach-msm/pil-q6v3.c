/* Copyright (c) 2010-2013, The Linux Foundation. All rights reserved.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>

#include <mach/subsystem_restart.h>
#include <mach/scm.h>
#include <mach/ramdump.h>
#include <mach/msm_bus_board.h>

#include "peripheral-loader.h"
#include "scm-pas.h"

#define QDSP6SS_RST_EVB		0x0000
#define QDSP6SS_STRAP_TCM	0x001C
#define QDSP6SS_STRAP_AHB	0x0020

#define LCC_Q6_FUNC		0x001C
#define LV_EN			BIT(27)
#define STOP_CORE		BIT(26)
#define CLAMP_IO		BIT(25)
#define Q6SS_PRIV_ARES		BIT(24)
#define Q6SS_SS_ARES		BIT(23)
#define Q6SS_ISDB_ARES		BIT(22)
#define Q6SS_ETM_ARES		BIT(21)
#define Q6_JTAG_CRC_EN		BIT(20)
#define Q6_JTAG_INV_EN		BIT(19)
#define Q6_JTAG_CXC_EN		BIT(18)
#define Q6_PXO_CRC_EN		BIT(17)
#define Q6_PXO_INV_EN		BIT(16)
#define Q6_PXO_CXC_EN		BIT(15)
#define Q6_PXO_SLEEP_EN		BIT(14)
#define Q6_SLP_CRC_EN		BIT(13)
#define Q6_SLP_INV_EN		BIT(12)
#define Q6_SLP_CXC_EN		BIT(11)
#define CORE_ARES		BIT(10)
#define CORE_L1_MEM_CORE_EN	BIT(9)
#define CORE_TCM_MEM_CORE_EN	BIT(8)
#define CORE_TCM_MEM_PERPH_EN	BIT(7)
#define CORE_GFM4_CLK_EN	BIT(2)
#define CORE_GFM4_RES		BIT(1)
#define RAMP_PLL_SRC_SEL	BIT(0)

#define Q6_STRAP_AHB_UPPER	(0x290 << 12)
#define Q6_STRAP_AHB_LOWER	0x280
#define Q6_STRAP_TCM_BASE	(0x28C << 15)
#define Q6_STRAP_TCM_CONFIG	0x28B

#define SCM_Q6_NMI_CMD		0x1

/**
 * struct q6v3_data - LPASS driver data
 * @base: register base
 * @cbase: clock base
 * @wk_base: wakeup register base
 * @wd_base: watchdog register base
 * @irq: watchdog irq
 * @pil: peripheral handle
 * @subsys: subsystem restart handle
 * @subsys_desc: subsystem restart descriptor
 * @fatal_wrk: fatal error workqueue
 * @pll: pll clock handle
 * @ramdump_dev: ramdump device
 */
struct q6v3_data {
	void __iomem *base;
	void __iomem *cbase;
	void __iomem *wk_base;
	void __iomem *wd_base;
	int irq;
	struct pil_desc pil_desc;
	struct subsys_device *subsys;
	struct subsys_desc subsys_desc;
	struct work_struct fatal_wrk;
	struct clk *pll;
	struct ramdump_device *ramdump_dev;
};

static void pil_q6v3_remove_proxy_votes(struct pil_desc *pil)
{
	struct q6v3_data *drv = dev_get_drvdata(pil->dev);
	clk_disable_unprepare(drv->pll);
}

static int pil_q6v3_make_proxy_votes(struct pil_desc *pil)
{
	int ret;
	struct q6v3_data *drv = dev_get_drvdata(pil->dev);

	ret = clk_prepare_enable(drv->pll);
	if (ret) {
		dev_err(pil->dev, "Failed to enable PLL\n");
		return ret;
	}
	return 0;
}

static int pil_q6v3_reset(struct pil_desc *pil)
{
	u32 reg;
	struct q6v3_data *drv = dev_get_drvdata(pil->dev);
	phys_addr_t start_addr = pil_get_entry_addr(pil);

	/* Put Q6 into reset */
	reg = readl_relaxed(drv->cbase + LCC_Q6_FUNC);
	reg |= Q6SS_SS_ARES | Q6SS_ISDB_ARES | Q6SS_ETM_ARES | STOP_CORE |
		CORE_ARES;
	reg &= ~CORE_GFM4_CLK_EN;
	writel_relaxed(reg, drv->cbase + LCC_Q6_FUNC);

	/* Wait 8 AHB cycles for Q6 to be fully reset (AHB = 1.5Mhz) */
	usleep_range(20, 30);

	/* Turn on Q6 memory */
	reg |= CORE_GFM4_CLK_EN | CORE_L1_MEM_CORE_EN | CORE_TCM_MEM_CORE_EN |
		CORE_TCM_MEM_PERPH_EN;
	writel_relaxed(reg, drv->cbase + LCC_Q6_FUNC);

	/* Turn on Q6 core clocks and take core out of reset */
	reg &= ~(CLAMP_IO | Q6SS_SS_ARES | Q6SS_ISDB_ARES | Q6SS_ETM_ARES |
			CORE_ARES);
	writel_relaxed(reg, drv->cbase + LCC_Q6_FUNC);

	/* Wait for clocks to be enabled */
	mb();
	/* Program boot address */
	writel_relaxed((start_addr >> 12) & 0xFFFFF,
			drv->base + QDSP6SS_RST_EVB);

	writel_relaxed(Q6_STRAP_TCM_CONFIG | Q6_STRAP_TCM_BASE,
			drv->base + QDSP6SS_STRAP_TCM);
	writel_relaxed(Q6_STRAP_AHB_UPPER | Q6_STRAP_AHB_LOWER,
			drv->base + QDSP6SS_STRAP_AHB);

	/* Wait for addresses to be programmed before starting Q6 */
	mb();

	/* Start Q6 instruction execution */
	reg &= ~STOP_CORE;
	writel_relaxed(reg, drv->cbase + LCC_Q6_FUNC);

	return 0;
}

static int pil_q6v3_shutdown(struct pil_desc *pil)
{
	u32 reg;
	struct q6v3_data *drv = dev_get_drvdata(pil->dev);

	/* Put Q6 into reset */
	reg = readl_relaxed(drv->cbase + LCC_Q6_FUNC);
	reg |= Q6SS_SS_ARES | Q6SS_ISDB_ARES | Q6SS_ETM_ARES | STOP_CORE |
		CORE_ARES;
	reg &= ~CORE_GFM4_CLK_EN;
	writel_relaxed(reg, drv->cbase + LCC_Q6_FUNC);

	/* Wait 8 AHB cycles for Q6 to be fully reset (AHB = 1.5Mhz) */
	usleep_range(20, 30);

	/* Turn off Q6 memory */
	reg &= ~(CORE_L1_MEM_CORE_EN | CORE_TCM_MEM_CORE_EN |
		CORE_TCM_MEM_PERPH_EN);
	writel_relaxed(reg, drv->cbase + LCC_Q6_FUNC);

	reg |= CLAMP_IO;
	writel_relaxed(reg, drv->cbase + LCC_Q6_FUNC);

	return 0;
}

static struct pil_reset_ops pil_q6v3_ops = {
	.auth_and_reset = pil_q6v3_reset,
	.shutdown = pil_q6v3_shutdown,
	.proxy_vote = pil_q6v3_make_proxy_votes,
	.proxy_unvote = pil_q6v3_remove_proxy_votes,
};

static int pil_q6v3_init_image_trusted(struct pil_desc *pil,
		const u8 *metadata, size_t size)
{
	return pas_init_image(PAS_Q6, metadata, size);
}

static int pil_q6v3_reset_trusted(struct pil_desc *pil)
{
	return pas_auth_and_reset(PAS_Q6);
}

static int pil_q6v3_shutdown_trusted(struct pil_desc *pil)
{
	return pas_shutdown(PAS_Q6);
}

static struct pil_reset_ops pil_q6v3_ops_trusted = {
	.init_image = pil_q6v3_init_image_trusted,
	.auth_and_reset = pil_q6v3_reset_trusted,
	.shutdown = pil_q6v3_shutdown_trusted,
	.proxy_vote = pil_q6v3_make_proxy_votes,
	.proxy_unvote = pil_q6v3_remove_proxy_votes,
};

static void q6_fatal_fn(struct work_struct *work)
{
	struct q6v3_data *drv = container_of(work, struct q6v3_data, fatal_wrk);

	pr_err("Watchdog bite received from Q6!\n");
	subsystem_restart_dev(drv->subsys);
}

static void send_q6_nmi(struct q6v3_data *drv)
{
	/* Send NMI to QDSP6 via an SCM call. */
	scm_call_atomic1(SCM_SVC_UTIL, SCM_Q6_NMI_CMD, 0x1);

	/* Wakeup the Q6 */
	writel_relaxed(0x2000, drv->wk_base + 0x1c);
	/* Q6 requires atleast 100ms to dump caches etc.*/
	mdelay(100);
	pr_info("Q6 NMI was sent.\n");
}

static int lpass_q6_shutdown(const struct subsys_desc *subsys, bool force_stop)
{
	struct q6v3_data *drv;

	drv = container_of(subsys, struct q6v3_data, subsys_desc);
	if (force_stop) {
		send_q6_nmi(drv);
		writel_relaxed(0x0, drv->wd_base + 0x24);
		mb();
	}

	pil_shutdown(&drv->pil_desc);
	disable_irq_nosync(drv->irq);

	return 0;
}

static int lpass_q6_powerup(const struct subsys_desc *subsys)
{
	struct q6v3_data *drv;
	int ret;

	drv = container_of(subsys, struct q6v3_data, subsys_desc);
	ret = pil_boot(&drv->pil_desc);
	enable_irq(drv->irq);
	return ret;
}

static int lpass_q6_ramdump(int enable, const struct subsys_desc *subsys)
{
	struct q6v3_data *drv;

	drv = container_of(subsys, struct q6v3_data, subsys_desc);
	if (!enable)
		return 0;

	return pil_do_ramdump(&drv->pil_desc, drv->ramdump_dev);
}

static void lpass_q6_crash_shutdown(const struct subsys_desc *subsys)
{
	struct q6v3_data *drv;

	drv = container_of(subsys, struct q6v3_data, subsys_desc);
	send_q6_nmi(drv);
}

static irqreturn_t lpass_wdog_bite_irq(int irq, void *dev_id)
{
	int ret;
	struct q6v3_data *drv = dev_id;

	ret = schedule_work(&drv->fatal_wrk);
	return IRQ_HANDLED;
}

static int pil_q6v3_driver_probe(struct platform_device *pdev)
{
	struct q6v3_data *drv;
	struct resource *res;
	struct pil_desc *desc;
	int ret;

	drv = devm_kzalloc(&pdev->dev, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;
	platform_set_drvdata(pdev, drv);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	drv->base = devm_request_and_ioremap(&pdev->dev, res);
	if (!drv->base)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	drv->wk_base = devm_request_and_ioremap(&pdev->dev, res);
	if (!drv->wk_base)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	drv->wd_base = devm_request_and_ioremap(&pdev->dev, res);
	if (!drv->wd_base)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	if (!res)
		return -EINVAL;
	drv->cbase = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!drv->cbase)
		return -ENOMEM;

	drv->irq = platform_get_irq(pdev, 0);
	if (drv->irq < 0)
		return drv->irq;

	drv->pll = devm_clk_get(&pdev->dev, "pll4");
	if (IS_ERR(drv->pll))
		return PTR_ERR(drv->pll);

	desc = &drv->pil_desc;
	desc->name = "q6";
	desc->dev = &pdev->dev;
	desc->owner = THIS_MODULE;
	desc->proxy_timeout = 10000;

	if (pas_supported(PAS_Q6) > 0) {
		desc->ops = &pil_q6v3_ops_trusted;
		dev_info(&pdev->dev, "using secure boot\n");
	} else {
		desc->ops = &pil_q6v3_ops;
		dev_info(&pdev->dev, "using non-secure boot\n");
	}

	ret = pil_desc_init(desc);
	if (ret)
		return ret;

	drv->subsys_desc.name = "adsp";
	drv->subsys_desc.dev = &pdev->dev;
	drv->subsys_desc.owner = THIS_MODULE;
	drv->subsys_desc.shutdown = lpass_q6_shutdown;
	drv->subsys_desc.powerup = lpass_q6_powerup;
	drv->subsys_desc.ramdump = lpass_q6_ramdump;
	drv->subsys_desc.crash_shutdown = lpass_q6_crash_shutdown;

	INIT_WORK(&drv->fatal_wrk, q6_fatal_fn);

	drv->ramdump_dev = create_ramdump_device("lpass", &pdev->dev);
	if (!drv->ramdump_dev) {
		ret = -ENOMEM;
		goto err_ramdump;
	}

	drv->subsys = subsys_register(&drv->subsys_desc);
	if (IS_ERR(drv->subsys)) {
		ret = PTR_ERR(drv->subsys);
		goto err_subsys;
	}

	scm_pas_init(MSM_BUS_MASTER_SPS);

	ret = devm_request_irq(&pdev->dev, drv->irq, lpass_wdog_bite_irq,
			       IRQF_TRIGGER_RISING, "lpass_wdog", drv);
	if (ret) {
		dev_err(&pdev->dev, "Unable to request wdog irq.\n");
		goto err_irq;
	}
	disable_irq(drv->irq);

	return 0;
err_irq:
	subsys_unregister(drv->subsys);
err_subsys:
	destroy_ramdump_device(drv->ramdump_dev);
err_ramdump:
	pil_desc_release(desc);
	return ret;
}

static int pil_q6v3_driver_exit(struct platform_device *pdev)
{
	struct q6v3_data *drv = platform_get_drvdata(pdev);
	subsys_unregister(drv->subsys);
	destroy_ramdump_device(drv->ramdump_dev);
	pil_desc_release(&drv->pil_desc);
	return 0;
}

static struct platform_driver pil_q6v3_driver = {
	.probe = pil_q6v3_driver_probe,
	.remove = pil_q6v3_driver_exit,
	.driver = {
		.name = "pil_qdsp6v3",
		.owner = THIS_MODULE,
	},
};

static int __init pil_q6v3_init(void)
{
	return platform_driver_register(&pil_q6v3_driver);
}
module_init(pil_q6v3_init);

static void __exit pil_q6v3_exit(void)
{
	platform_driver_unregister(&pil_q6v3_driver);
}
module_exit(pil_q6v3_exit);

MODULE_DESCRIPTION("Support for booting QDSP6v3 (Hexagon) processors");
MODULE_LICENSE("GPL v2");
