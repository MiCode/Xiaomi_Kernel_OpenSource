/* Copyright (c) 2010-2012, The Linux Foundation. All rights reserved.
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
#include <linux/elf.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/reboot.h>

#include <mach/msm_iomap.h>
#include <mach/subsystem_restart.h>
#include <mach/msm_smsm.h>
#include <mach/peripheral-loader.h>

#include "modem_notifier.h"
#include "peripheral-loader.h"
#include "scm-pas.h"
#include "ramdump.h"

#define MARM_BOOT_CONTROL		0x0010
#define MARM_RESET			(MSM_CLK_CTL_BASE + 0x2BD4)
#define MAHB0_SFAB_PORT_RESET		(MSM_CLK_CTL_BASE + 0x2304)
#define MARM_CLK_BRANCH_ENA_VOTE	(MSM_CLK_CTL_BASE + 0x3000)
#define MARM_CLK_SRC0_NS		(MSM_CLK_CTL_BASE + 0x2BC0)
#define MARM_CLK_SRC1_NS		(MSM_CLK_CTL_BASE + 0x2BC4)
#define MARM_CLK_SRC_CTL		(MSM_CLK_CTL_BASE + 0x2BC8)
#define MARM_CLK_CTL			(MSM_CLK_CTL_BASE + 0x2BCC)
#define SFAB_MSS_S_HCLK_CTL		(MSM_CLK_CTL_BASE + 0x2C00)
#define MSS_MODEM_CXO_CLK_CTL		(MSM_CLK_CTL_BASE + 0x2C44)
#define MSS_SLP_CLK_CTL			(MSM_CLK_CTL_BASE + 0x2C60)
#define MSS_MARM_SYS_REF_CLK_CTL	(MSM_CLK_CTL_BASE + 0x2C64)
#define MAHB0_CLK_CTL			(MSM_CLK_CTL_BASE + 0x2300)
#define MAHB1_CLK_CTL			(MSM_CLK_CTL_BASE + 0x2BE4)
#define MAHB2_CLK_CTL			(MSM_CLK_CTL_BASE + 0x2C20)
#define MAHB1_NS			(MSM_CLK_CTL_BASE + 0x2BE0)
#define MARM_CLK_FS			(MSM_CLK_CTL_BASE + 0x2BD0)
#define MAHB2_CLK_FS			(MSM_CLK_CTL_BASE + 0x2C24)
#define PLL_ENA_MARM			(MSM_CLK_CTL_BASE + 0x3500)
#define PLL8_STATUS			(MSM_CLK_CTL_BASE + 0x3158)
#define CLK_HALT_MSS_SMPSS_MISC_STATE	(MSM_CLK_CTL_BASE + 0x2FDC)
#define MSS_MODEM_RESET			(MSM_CLK_CTL_BASE + 0x2C48)

struct modem_data {
	void __iomem *base;
	void __iomem *wdog;
	unsigned long start_addr;
	struct pil_device *pil;
	struct clk *xo;
	struct notifier_block notifier;
	int ignore_smsm_ack;
	int irq;
	struct subsys_device *subsys;
	struct subsys_desc subsys_desc;
	struct delayed_work unlock_work;
	struct work_struct fatal_work;
	struct ramdump_device *ramdump_dev;
};

static int make_modem_proxy_votes(struct pil_desc *pil)
{
	int ret;
	struct modem_data *drv = dev_get_drvdata(pil->dev);

	ret = clk_prepare_enable(drv->xo);
	if (ret) {
		dev_err(pil->dev, "Failed to enable XO\n");
		return ret;
	}
	return 0;
}

static void remove_modem_proxy_votes(struct pil_desc *pil)
{
	struct modem_data *drv = dev_get_drvdata(pil->dev);
	clk_disable_unprepare(drv->xo);
}

static int modem_init_image(struct pil_desc *pil, const u8 *metadata,
		size_t size)
{
	const struct elf32_hdr *ehdr = (struct elf32_hdr *)metadata;
	struct modem_data *drv = dev_get_drvdata(pil->dev);
	drv->start_addr = ehdr->e_entry;
	return 0;
}

static int modem_reset(struct pil_desc *pil)
{
	u32 reg;
	const struct modem_data *drv = dev_get_drvdata(pil->dev);

	/* Put modem AHB0,1,2 clocks into reset */
	writel_relaxed(BIT(0) | BIT(1), MAHB0_SFAB_PORT_RESET);
	writel_relaxed(BIT(7), MAHB1_CLK_CTL);
	writel_relaxed(BIT(7), MAHB2_CLK_CTL);

	/* Vote for pll8 on behalf of the modem */
	reg = readl_relaxed(PLL_ENA_MARM);
	reg |= BIT(8);
	writel_relaxed(reg, PLL_ENA_MARM);

	/* Wait for PLL8 to enable */
	while (!(readl_relaxed(PLL8_STATUS) & BIT(16)))
		cpu_relax();

	/* Set MAHB1 divider to Div-5 to run MAHB1,2 and sfab at 79.8 Mhz*/
	writel_relaxed(0x4, MAHB1_NS);

	/* Vote for modem AHB1 and 2 clocks to be on on behalf of the modem */
	reg = readl_relaxed(MARM_CLK_BRANCH_ENA_VOTE);
	reg |= BIT(0) | BIT(1);
	writel_relaxed(reg, MARM_CLK_BRANCH_ENA_VOTE);

	/* Source marm_clk off of PLL8 */
	reg = readl_relaxed(MARM_CLK_SRC_CTL);
	if ((reg & 0x1) == 0) {
		writel_relaxed(0x3, MARM_CLK_SRC1_NS);
		reg |= 0x1;
	} else {
		writel_relaxed(0x3, MARM_CLK_SRC0_NS);
		reg &= ~0x1;
	}
	writel_relaxed(reg | 0x2, MARM_CLK_SRC_CTL);

	/*
	 * Force core on and periph on signals to remain active during halt
	 * for marm_clk and mahb2_clk
	 */
	writel_relaxed(0x6F, MARM_CLK_FS);
	writel_relaxed(0x6F, MAHB2_CLK_FS);

	/*
	 * Enable all of the marm_clk branches, cxo sourced marm branches,
	 * and sleep clock branches
	 */
	writel_relaxed(0x10, MARM_CLK_CTL);
	writel_relaxed(0x10, MAHB0_CLK_CTL);
	writel_relaxed(0x10, SFAB_MSS_S_HCLK_CTL);
	writel_relaxed(0x10, MSS_MODEM_CXO_CLK_CTL);
	writel_relaxed(0x10, MSS_SLP_CLK_CTL);
	writel_relaxed(0x10, MSS_MARM_SYS_REF_CLK_CTL);

	/* Wait for above clocks to be turned on */
	while (readl_relaxed(CLK_HALT_MSS_SMPSS_MISC_STATE) & (BIT(7) | BIT(8) |
				BIT(9) | BIT(10) | BIT(4) | BIT(6)))
		cpu_relax();

	/* Take MAHB0,1,2 clocks out of reset */
	writel_relaxed(0x0, MAHB2_CLK_CTL);
	writel_relaxed(0x0, MAHB1_CLK_CTL);
	writel_relaxed(0x0, MAHB0_SFAB_PORT_RESET);
	mb();

	/* Setup exception vector table base address */
	writel_relaxed(drv->start_addr | 0x1, drv->base + MARM_BOOT_CONTROL);

	/* Wait for vector table to be setup */
	mb();

	/* Bring modem out of reset */
	writel_relaxed(0x0, MARM_RESET);

	return 0;
}

static int modem_pil_shutdown(struct pil_desc *pil)
{
	u32 reg;

	/* Put modem into reset */
	writel_relaxed(0x1, MARM_RESET);
	mb();

	/* Put modem AHB0,1,2 clocks into reset */
	writel_relaxed(BIT(0) | BIT(1), MAHB0_SFAB_PORT_RESET);
	writel_relaxed(BIT(7), MAHB1_CLK_CTL);
	writel_relaxed(BIT(7), MAHB2_CLK_CTL);
	mb();

	/*
	 * Disable all of the marm_clk branches, cxo sourced marm branches,
	 * and sleep clock branches
	 */
	writel_relaxed(0x0, MARM_CLK_CTL);
	writel_relaxed(0x0, MAHB0_CLK_CTL);
	writel_relaxed(0x0, SFAB_MSS_S_HCLK_CTL);
	writel_relaxed(0x0, MSS_MODEM_CXO_CLK_CTL);
	writel_relaxed(0x0, MSS_SLP_CLK_CTL);
	writel_relaxed(0x0, MSS_MARM_SYS_REF_CLK_CTL);

	/* Disable marm_clk */
	reg = readl_relaxed(MARM_CLK_SRC_CTL);
	reg &= ~0x2;
	writel_relaxed(reg, MARM_CLK_SRC_CTL);

	/* Clear modem's votes for ahb clocks */
	writel_relaxed(0x0, MARM_CLK_BRANCH_ENA_VOTE);

	/* Clear modem's votes for PLLs */
	writel_relaxed(0x0, PLL_ENA_MARM);

	return 0;
}

static struct pil_reset_ops pil_modem_ops = {
	.init_image = modem_init_image,
	.auth_and_reset = modem_reset,
	.shutdown = modem_pil_shutdown,
	.proxy_vote = make_modem_proxy_votes,
	.proxy_unvote = remove_modem_proxy_votes,
};

static int modem_init_image_trusted(struct pil_desc *pil, const u8 *metadata,
		size_t size)
{
	return pas_init_image(PAS_MODEM, metadata, size);
}

static int modem_reset_trusted(struct pil_desc *pil)
{
	return pas_auth_and_reset(PAS_MODEM);
}

static int modem_shutdown_trusted(struct pil_desc *pil)
{
	return pas_shutdown(PAS_MODEM);
}

static struct pil_reset_ops pil_modem_ops_trusted = {
	.init_image = modem_init_image_trusted,
	.auth_and_reset = modem_reset_trusted,
	.shutdown = modem_shutdown_trusted,
	.proxy_vote = make_modem_proxy_votes,
	.proxy_unvote = remove_modem_proxy_votes,
};

static void modem_crash_shutdown(const struct subsys_desc *subsys)
{
	struct modem_data *drv;

	/* If modem hasn't already crashed, send SMSM_RESET. */
	drv = container_of(subsys, struct modem_data, subsys_desc);
	if (!(smsm_get_state(SMSM_MODEM_STATE) & SMSM_RESET)) {
		modem_unregister_notifier(&drv->notifier);
		smsm_reset_modem(SMSM_RESET);
	}

	/* Wait to allow the modem to clean up caches etc. */
	mdelay(5);
}

static irqreturn_t modem_wdog_bite_irq(int irq, void *dev_id)
{
	struct modem_data *drv = dev_id;

	schedule_work(&drv->fatal_work);
	disable_irq_nosync(drv->irq);

	return IRQ_HANDLED;
}

static void modem_unlock_timeout(struct work_struct *work)
{
	struct modem_data *drv;
	struct delayed_work *dwork = to_delayed_work(work);

	pr_crit("Timeout waiting for modem to unlock.\n");

	drv = container_of(dwork, struct modem_data, unlock_work);
	/* The unlock didn't work, clear the reset */
	writel_relaxed(0x0, MSS_MODEM_RESET);
	mb();

	subsystem_restart_dev(drv->subsys);
	enable_irq(drv->irq);
}

static void modem_fatal_fn(struct work_struct *work)
{
	u32 modem_state;
	u32 panic_smsm_states = SMSM_RESET | SMSM_SYSTEM_DOWNLOAD;
	u32 reset_smsm_states = SMSM_SYSTEM_REBOOT_USR | SMSM_SYSTEM_PWRDWN_USR;
	struct modem_data *drv;

	drv = container_of(work, struct modem_data, fatal_work);

	pr_err("Watchdog bite received from modem!\n");

	modem_state = smsm_get_state(SMSM_MODEM_STATE);
	pr_err("Modem SMSM state = 0x%x!\n", modem_state);

	if (modem_state == 0 || modem_state & panic_smsm_states) {
		subsystem_restart_dev(drv->subsys);
		enable_irq(drv->irq);
	} else if (modem_state & reset_smsm_states) {
		pr_err("User-invoked system reset/powerdown.");
		kernel_restart(NULL);
	} else {
		unsigned long timeout = msecs_to_jiffies(6000);

		pr_err("Modem AHB locked up. Trying to free up modem!\n");

		writel_relaxed(0x3, MSS_MODEM_RESET);
		/*
		 * If we are still alive (allowing for the 5 second
		 * delayed-panic-reboot), the modem is either still wedged or
		 * SMSM didn't come through. Force panic in that case.
		 */
		schedule_delayed_work(&drv->unlock_work, timeout);
	}
}

static int modem_notif_handler(struct notifier_block *nb, unsigned long code,
				void *p)
{
	struct modem_data *drv = container_of(nb, struct modem_data, notifier);

	if (code == MODEM_NOTIFIER_START_RESET) {
		if (drv->ignore_smsm_ack) {
			drv->ignore_smsm_ack = 0;
		} else {
			pr_err("Modem error fatal'ed.");
			subsystem_restart_dev(drv->subsys);
		}
	}
	return NOTIFY_DONE;
}

static int modem_shutdown(const struct subsys_desc *subsys)
{
	struct modem_data *drv;

	drv = container_of(subsys, struct modem_data, subsys_desc);
	/*
	 * If the modem didn't already crash, setting SMSM_RESET here will help
	 * flush caches etc. The ignore_smsm_ack flag is set to ignore the
	 * SMSM_RESET notification that is generated due to the modem settings
	 * its own SMSM_RESET bit in response to the apps setting the apps
	 * SMSM_RESET bit.
	 */
	if (!(smsm_get_state(SMSM_MODEM_STATE) & SMSM_RESET)) {
		drv->ignore_smsm_ack = 1;
		smsm_reset_modem(SMSM_RESET);
	}

	/* Disable the modem watchdog to allow clean modem bootup */
	writel_relaxed(0x0, drv->wdog + 0x8);
	/*
	 * The write above needs to go through before the modem is powered up
	 * again.
	 */
	mb();
	/* Wait here to allow the modem to clean up caches, etc. */
	msleep(20);

	pil_force_shutdown("modem");
	disable_irq_nosync(drv->irq);

	return 0;
}

static int modem_powerup(const struct subsys_desc *subsys)
{
	struct modem_data *drv;
	int ret;

	drv = container_of(subsys, struct modem_data, subsys_desc);
	ret = pil_force_boot("modem");
	enable_irq(drv->irq);

	return ret;
}

/* FIXME: Get address, size from PIL */
static struct ramdump_segment modem_segments[] = {
	{ 0x42F00000, 0x46000000 - 0x42F00000 },
};

static int modem_ramdump(int enable, const struct subsys_desc *subsys)
{
	struct modem_data *drv;

	drv = container_of(subsys, struct modem_data, subsys_desc);
	if (enable)
		return do_ramdump(drv->ramdump_dev, modem_segments,
				ARRAY_SIZE(modem_segments));
	else
		return 0;
}

static int pil_modem_driver_probe(struct platform_device *pdev)
{
	struct modem_data *drv;
	struct resource *res;
	struct pil_desc *desc;
	int ret;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -EINVAL;

	drv = devm_kzalloc(&pdev->dev, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;
	platform_set_drvdata(pdev, drv);

	drv->irq = platform_get_irq(pdev, 0);
	if (drv->irq < 0)
		return drv->irq;

	drv->base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!drv->base)
		return -ENOMEM;

	drv->xo = devm_clk_get(&pdev->dev, "xo");
	if (IS_ERR(drv->xo))
		return PTR_ERR(drv->xo);

	desc = devm_kzalloc(&pdev->dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res)
		return -EINVAL;

	drv->wdog = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!drv->wdog)
		return -ENOMEM;

	desc->name = "modem";
	desc->depends_on = "q6";
	desc->dev = &pdev->dev;
	desc->owner = THIS_MODULE;
	desc->proxy_timeout = 10000;

	if (pas_supported(PAS_MODEM) > 0) {
		desc->ops = &pil_modem_ops_trusted;
		dev_info(&pdev->dev, "using secure boot\n");
	} else {
		desc->ops = &pil_modem_ops;
		dev_info(&pdev->dev, "using non-secure boot\n");
	}
	drv->pil = msm_pil_register(desc);
	if (IS_ERR(drv->pil))
		return PTR_ERR(drv->pil);

	drv->notifier.notifier_call = modem_notif_handler,
	ret = modem_register_notifier(&drv->notifier);
	if (ret)
		goto err_notify;

	drv->subsys_desc.name = "modem";
	drv->subsys_desc.shutdown = modem_shutdown;
	drv->subsys_desc.powerup = modem_powerup;
	drv->subsys_desc.ramdump = modem_ramdump;
	drv->subsys_desc.crash_shutdown = modem_crash_shutdown;

	INIT_WORK(&drv->fatal_work, modem_fatal_fn);
	INIT_DELAYED_WORK(&drv->unlock_work, modem_unlock_timeout);

	drv->subsys = subsys_register(&drv->subsys_desc);
	if (IS_ERR(drv->subsys)) {
		ret = PTR_ERR(drv->subsys);
		goto err_subsys;
	}

	drv->ramdump_dev = create_ramdump_device("modem");
	if (!drv->ramdump_dev) {
		ret = -ENOMEM;
		goto err_ramdump;
	}

	ret = devm_request_irq(&pdev->dev, drv->irq, modem_wdog_bite_irq,
			IRQF_TRIGGER_RISING, "modem_watchdog", drv);
	if (ret)
		goto err_irq;
	return 0;

err_irq:
	destroy_ramdump_device(drv->ramdump_dev);
err_ramdump:
	subsys_unregister(drv->subsys);
err_subsys:
	modem_unregister_notifier(&drv->notifier);
err_notify:
	msm_pil_unregister(drv->pil);
	return ret;
}

static int pil_modem_driver_exit(struct platform_device *pdev)
{
	struct modem_data *drv = platform_get_drvdata(pdev);

	destroy_ramdump_device(drv->ramdump_dev);
	subsys_unregister(drv->subsys);
	modem_unregister_notifier(&drv->notifier);
	msm_pil_unregister(drv->pil);

	return 0;
}

static struct platform_driver pil_modem_driver = {
	.probe = pil_modem_driver_probe,
	.remove = pil_modem_driver_exit,
	.driver = {
		.name = "pil_modem",
		.owner = THIS_MODULE,
	},
};

static int __init pil_modem_init(void)
{
	return platform_driver_register(&pil_modem_driver);
}
module_init(pil_modem_init);

static void __exit pil_modem_exit(void)
{
	platform_driver_unregister(&pil_modem_driver);
}
module_exit(pil_modem_exit);

MODULE_DESCRIPTION("Support for booting modem processors");
MODULE_LICENSE("GPL v2");
