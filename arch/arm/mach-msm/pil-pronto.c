/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/iopoll.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/wcnss_wlan.h>
#include <linux/of_gpio.h>

#include <mach/subsystem_restart.h>
#include <mach/msm_smsm.h>
#include <mach/ramdump.h>
#include <mach/msm_smem.h>

#include "peripheral-loader.h"
#include "scm-pas.h"

#define PRONTO_PMU_COMMON_GDSCR				0x24
#define PRONTO_PMU_COMMON_GDSCR_SW_COLLAPSE		BIT(0)
#define CLK_DIS_WAIT					12
#define EN_FEW_WAIT					16
#define EN_REST_WAIT					20

#define PRONTO_PMU_COMMON_CPU_CBCR			0x30
#define PRONTO_PMU_COMMON_CPU_CBCR_CLK_EN		BIT(0)
#define PRONTO_PMU_COMMON_CPU_CLK_OFF			BIT(31)

#define PRONTO_PMU_COMMON_AHB_CBCR			0x34
#define PRONTO_PMU_COMMON_AHB_CBCR_CLK_EN		BIT(0)
#define PRONTO_PMU_COMMON_AHB_CLK_OFF			BIT(31)

#define PRONTO_PMU_COMMON_CSR				0x1040
#define PRONTO_PMU_COMMON_CSR_A2XB_CFG_EN		BIT(0)

#define PRONTO_PMU_SOFT_RESET				0x104C
#define PRONTO_PMU_SOFT_RESET_CRCM_CCPU_SOFT_RESET	BIT(10)

#define PRONTO_PMU_CCPU_CTL				0x2000
#define PRONTO_PMU_CCPU_CTL_REMAP_EN			BIT(2)
#define PRONTO_PMU_CCPU_CTL_HIGH_IVT			BIT(0)

#define PRONTO_PMU_CCPU_BOOT_REMAP_ADDR			0x2004

#define CLK_CTL_WCNSS_RESTART_BIT			BIT(0)

#define AXI_HALTREQ					0x0
#define AXI_HALTACK					0x4
#define AXI_IDLE					0x8

#define HALT_ACK_TIMEOUT_US				500000
#define CLK_UPDATE_TIMEOUT_US				500000

struct pronto_data {
	void __iomem *base;
	void __iomem *reset_base;
	void __iomem *axi_halt_base;
	struct pil_device *pil;
	struct pil_desc desc;
	struct subsys_device *subsys;
	struct subsys_desc subsys_desc;
	struct clk *cxo;
	struct regulator *vreg;
	bool restart_inprogress;
	bool crash;
	struct delayed_work cancel_vote_work;
	int irq;
	unsigned int err_fatal_irq;
	int force_stop_gpio;
	struct ramdump_device *ramdump_dev;
};

static int pil_pronto_make_proxy_vote(struct pil_desc *pil)
{
	struct pronto_data *drv = dev_get_drvdata(pil->dev);
	int ret;

	ret = regulator_enable(drv->vreg);
	if (ret) {
		dev_err(pil->dev, "failed to enable pll supply\n");
		goto err;
	}
	ret = clk_prepare_enable(drv->cxo);
	if (ret) {
		dev_err(pil->dev, "failed to enable cxo\n");
		goto err_clk;
	}
	return 0;
err_clk:
	regulator_disable(drv->vreg);
err:
	return ret;
}

static void pil_pronto_remove_proxy_vote(struct pil_desc *pil)
{
	struct pronto_data *drv = dev_get_drvdata(pil->dev);
	regulator_disable(drv->vreg);
	clk_disable_unprepare(drv->cxo);
}

static int pil_pronto_reset(struct pil_desc *pil)
{
	u32 reg;
	int rc;
	struct pronto_data *drv = dev_get_drvdata(pil->dev);
	void __iomem *base = drv->base;
	phys_addr_t start_addr = pil_get_entry_addr(pil);

	/* Deassert reset to subsystem and wait for propagation */
	reg = readl_relaxed(drv->reset_base);
	reg &= ~CLK_CTL_WCNSS_RESTART_BIT;
	writel_relaxed(reg, drv->reset_base);
	mb();
	udelay(2);

	/* Configure boot address */
	writel_relaxed(start_addr >> 16, base +
			PRONTO_PMU_CCPU_BOOT_REMAP_ADDR);

	/* Use the high vector table */
	reg = readl_relaxed(base + PRONTO_PMU_CCPU_CTL);
	reg |= PRONTO_PMU_CCPU_CTL_REMAP_EN | PRONTO_PMU_CCPU_CTL_HIGH_IVT;
	writel_relaxed(reg, base + PRONTO_PMU_CCPU_CTL);

	/* Turn on AHB clock of common_ss */
	reg = readl_relaxed(base + PRONTO_PMU_COMMON_AHB_CBCR);
	reg |= PRONTO_PMU_COMMON_AHB_CBCR_CLK_EN;
	writel_relaxed(reg, base + PRONTO_PMU_COMMON_AHB_CBCR);

	/* Turn on CPU clock of common_ss */
	reg = readl_relaxed(base + PRONTO_PMU_COMMON_CPU_CBCR);
	reg |= PRONTO_PMU_COMMON_CPU_CBCR_CLK_EN;
	writel_relaxed(reg, base + PRONTO_PMU_COMMON_CPU_CBCR);

	/* Enable A2XB bridge */
	reg = readl_relaxed(base + PRONTO_PMU_COMMON_CSR);
	reg |= PRONTO_PMU_COMMON_CSR_A2XB_CFG_EN;
	writel_relaxed(reg, base + PRONTO_PMU_COMMON_CSR);

	/* Enable common_ss power */
	reg = readl_relaxed(base + PRONTO_PMU_COMMON_GDSCR);
	reg &= ~PRONTO_PMU_COMMON_GDSCR_SW_COLLAPSE;
	writel_relaxed(reg, base + PRONTO_PMU_COMMON_GDSCR);

	/* Wait for AHB clock to be on */
	rc = readl_tight_poll_timeout(base + PRONTO_PMU_COMMON_AHB_CBCR,
				      reg,
				      !(reg & PRONTO_PMU_COMMON_AHB_CLK_OFF),
				      CLK_UPDATE_TIMEOUT_US);
	if (rc) {
		dev_err(pil->dev, "pronto common ahb clk enable timeout\n");
		return rc;
	}

	/* Wait for CPU clock to be on */
	rc = readl_tight_poll_timeout(base + PRONTO_PMU_COMMON_CPU_CBCR,
				      reg,
				      !(reg & PRONTO_PMU_COMMON_CPU_CLK_OFF),
				      CLK_UPDATE_TIMEOUT_US);
	if (rc) {
		dev_err(pil->dev, "pronto common cpu clk enable timeout\n");
		return rc;
	}

	/* Deassert ARM9 software reset */
	reg = readl_relaxed(base + PRONTO_PMU_SOFT_RESET);
	reg &= ~PRONTO_PMU_SOFT_RESET_CRCM_CCPU_SOFT_RESET;
	writel_relaxed(reg, base + PRONTO_PMU_SOFT_RESET);

	return 0;
}

static int pil_pronto_shutdown(struct pil_desc *pil)
{
	struct pronto_data *drv = dev_get_drvdata(pil->dev);
	int ret;
	u32 reg, status;

	/* Halt A2XB */
	writel_relaxed(1, drv->axi_halt_base + AXI_HALTREQ);
	ret = readl_poll_timeout(drv->axi_halt_base + AXI_HALTACK,
				status, status, 50, HALT_ACK_TIMEOUT_US);
	if (ret)
		dev_err(pil->dev, "Port halt timeout\n");
	else if (!readl_relaxed(drv->axi_halt_base + AXI_IDLE))
		dev_err(pil->dev, "Port halt failed\n");

	writel_relaxed(0, drv->axi_halt_base + AXI_HALTREQ);

	/* Assert reset to Pronto */
	reg = readl_relaxed(drv->reset_base);
	reg |= CLK_CTL_WCNSS_RESTART_BIT;
	writel_relaxed(reg, drv->reset_base);

	/* Wait for reset to complete */
	mb();
	usleep_range(1000, 2000);

	/* Deassert reset to subsystem and wait for propagation */
	reg = readl_relaxed(drv->reset_base);
	reg &= ~CLK_CTL_WCNSS_RESTART_BIT;
	writel_relaxed(reg, drv->reset_base);
	mb();
	udelay(2);

	return 0;
}

static struct pil_reset_ops pil_pronto_ops = {
	.auth_and_reset = pil_pronto_reset,
	.shutdown = pil_pronto_shutdown,
	.proxy_vote = pil_pronto_make_proxy_vote,
	.proxy_unvote = pil_pronto_remove_proxy_vote,
};

static int pil_pronto_init_image_trusted(struct pil_desc *pil,
			const u8 *metadata, size_t size)
{
	return pas_init_image(PAS_WCNSS, metadata, size);
}

static int pil_pronto_mem_setup_trusted(struct pil_desc *pil, phys_addr_t addr,
			       size_t size)
{
	return pas_mem_setup(PAS_WCNSS, addr, size);
}

static int pil_pronto_reset_trusted(struct pil_desc *pil)
{
	return pas_auth_and_reset(PAS_WCNSS);
}

static int pil_pronto_shutdown_trusted(struct pil_desc *pil)
{
	return pas_shutdown(PAS_WCNSS);
}

static struct pil_reset_ops pil_pronto_ops_trusted = {
	.init_image = pil_pronto_init_image_trusted,
	.mem_setup = pil_pronto_mem_setup_trusted,
	.auth_and_reset = pil_pronto_reset_trusted,
	.shutdown = pil_pronto_shutdown_trusted,
	.proxy_vote = pil_pronto_make_proxy_vote,
	.proxy_unvote = pil_pronto_remove_proxy_vote,
};

#define subsys_to_drv(d) container_of(d, struct pronto_data, subsys_desc)

static int pronto_start(const struct subsys_desc *desc)
{
	struct pronto_data *drv = subsys_to_drv(desc);
	return pil_boot(&drv->desc);
}

static void pronto_stop(const struct subsys_desc *desc)
{
	struct pronto_data *drv = subsys_to_drv(desc);
	pil_shutdown(&drv->desc);
}

static void log_wcnss_sfr(void)
{
	char *smem_reset_reason;
	unsigned smem_reset_size;

	smem_reset_reason = smem_get_entry(SMEM_SSR_REASON_WCNSS0,
					   &smem_reset_size);

	if (!smem_reset_reason || !smem_reset_size) {
		pr_err("wcnss subsystem failure reason:\n"
		       "(unknown, smem_get_entry failed)");
	} else if (!smem_reset_reason[0]) {
		pr_err("wcnss subsystem failure reason:\n"
		       "(unknown, init string found)");
	} else {
		pr_err("wcnss subsystem failure reason: %.81s\n",
				smem_reset_reason);
		memset(smem_reset_reason, 0, smem_reset_size);
		wmb();
	}
}

static void restart_wcnss(struct pronto_data *drv)
{
	log_wcnss_sfr();
	subsystem_restart_dev(drv->subsys);
}

static irqreturn_t wcnss_err_fatal_intr_handler(int irq, void *dev_id)
{
	struct pronto_data *drv = dev_id;

	pr_err("Fatal error on the wcnss.\n");

	drv->crash = true;
	if (drv->restart_inprogress) {
		pr_err("wcnss: Ignoring error fatal, restart in progress\n");
		return IRQ_HANDLED;
	}

	drv->restart_inprogress = true;
	restart_wcnss(drv);

	return IRQ_HANDLED;
}

static irqreturn_t wcnss_wdog_bite_irq_hdlr(int irq, void *dev_id)
{
	struct pronto_data *drv = dev_id;

	drv->crash = true;

	disable_irq_nosync(drv->irq);

	if (drv->restart_inprogress) {
		pr_err("Ignoring wcnss bite irq, restart in progress\n");
		return IRQ_HANDLED;
	}

	drv->restart_inprogress = true;
	restart_wcnss(drv);

	return IRQ_HANDLED;
}

static void wcnss_post_bootup(struct work_struct *work)
{
	struct platform_device *pdev = wcnss_get_platform_device();
	struct wcnss_wlan_config *pwlanconfig = wcnss_get_wlan_config();

	wcnss_wlan_power(&pdev->dev, pwlanconfig, WCNSS_WLAN_SWITCH_OFF);
}

static int wcnss_shutdown(const struct subsys_desc *subsys)
{
	struct pronto_data *drv = subsys_to_drv(subsys);

	pil_shutdown(&drv->desc);
	flush_delayed_work(&drv->cancel_vote_work);
	wcnss_flush_delayed_boot_votes();

	return 0;
}

static int wcnss_powerup(const struct subsys_desc *subsys)
{
	struct pronto_data *drv = subsys_to_drv(subsys);
	struct platform_device *pdev = wcnss_get_platform_device();
	struct wcnss_wlan_config *pwlanconfig = wcnss_get_wlan_config();
	int    ret = -1;

	if (pdev && pwlanconfig)
		ret = wcnss_wlan_power(&pdev->dev, pwlanconfig,
					WCNSS_WLAN_SWITCH_ON);
	if (!ret) {
		msleep(1000);
		ret = pil_boot(&drv->desc);
		if (ret)
			return ret;
	}
	drv->restart_inprogress = false;
	enable_irq(drv->irq);
	schedule_delayed_work(&drv->cancel_vote_work, msecs_to_jiffies(5000));

	return 0;
}

static void crash_shutdown(const struct subsys_desc *subsys)
{
	struct pronto_data *drv = subsys_to_drv(subsys);

	pr_err("wcnss crash shutdown %d\n", drv->crash);
	if (!drv->crash)
		gpio_set_value(drv->force_stop_gpio, 1);
}

static int wcnss_ramdump(int enable, const struct subsys_desc *subsys)
{
	struct pronto_data *drv = subsys_to_drv(subsys);

	if (!enable)
		return 0;

	return pil_do_ramdump(&drv->desc, drv->ramdump_dev);
}

static int __devinit pil_pronto_probe(struct platform_device *pdev)
{
	struct pronto_data *drv;
	struct resource *res;
	struct pil_desc *desc;
	int ret, err_fatal_gpio, irq;
	uint32_t regval;

	int clk_ready = of_get_named_gpio(pdev->dev.of_node,
			"qcom,gpio-proxy-unvote", 0);
	if (clk_ready < 0)
		return clk_ready;

	clk_ready = gpio_to_irq(clk_ready);
	if (clk_ready < 0)
		return clk_ready;

	drv = devm_kzalloc(&pdev->dev, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;
	platform_set_drvdata(pdev, drv);

	drv->irq = platform_get_irq(pdev, 0);
	if (drv->irq < 0)
		return drv->irq;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "pmu_base");
	drv->base = devm_request_and_ioremap(&pdev->dev, res);
	if (!drv->base)
		return -ENOMEM;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "clk_base");
	drv->reset_base = devm_request_and_ioremap(&pdev->dev, res);
	if (!drv->reset_base)
		return -ENOMEM;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "halt_base");
	drv->axi_halt_base = devm_request_and_ioremap(&pdev->dev, res);
	if (!drv->axi_halt_base)
		return -ENOMEM;

	desc = &drv->desc;
	ret = of_property_read_string(pdev->dev.of_node, "qcom,firmware-name",
				      &desc->name);
	if (ret)
		return ret;

	err_fatal_gpio = of_get_named_gpio(pdev->dev.of_node,
			"qcom,gpio-err-fatal", 0);
	if (err_fatal_gpio < 0)
		return err_fatal_gpio;

	irq = gpio_to_irq(err_fatal_gpio);
	if (irq < 0)
		return irq;

	drv->err_fatal_irq = irq;

	drv->force_stop_gpio = of_get_named_gpio(pdev->dev.of_node,
			"qcom,gpio-force-stop", 0);
	if (drv->force_stop_gpio < 0)
		return drv->force_stop_gpio;


	desc->dev = &pdev->dev;
	desc->owner = THIS_MODULE;
	desc->proxy_timeout = 10000;
	desc->proxy_unvote_irq = clk_ready;

	if (pas_supported(PAS_WCNSS) > 0) {
		desc->ops = &pil_pronto_ops_trusted;
		dev_info(&pdev->dev, "using secure boot\n");
	} else {
		desc->ops = &pil_pronto_ops;
		dev_info(&pdev->dev, "using non-secure boot\n");
	}

	drv->vreg = devm_regulator_get(&pdev->dev, "vdd_pronto_pll");
	if (IS_ERR(drv->vreg)) {
		dev_err(&pdev->dev, "failed to get pronto pll supply");
		return PTR_ERR(drv->vreg);
	}

	ret = regulator_set_voltage(drv->vreg, 1800000, 1800000);
	if (ret) {
		dev_err(&pdev->dev, "failed to set pll supply voltage\n");
		return ret;
	}

	ret = regulator_set_optimum_mode(drv->vreg, 18000);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to set pll supply mode\n");
		return ret;
	}

	drv->cxo = devm_clk_get(&pdev->dev, "xo");
	if (IS_ERR(drv->cxo))
		return PTR_ERR(drv->cxo);

	ret = pil_desc_init(desc);
	if (ret)
		return ret;

	drv->subsys_desc.name = desc->name;
	drv->subsys_desc.dev = &pdev->dev;
	drv->subsys_desc.owner = THIS_MODULE;
	drv->subsys_desc.shutdown = wcnss_shutdown;
	drv->subsys_desc.powerup = wcnss_powerup;
	drv->subsys_desc.ramdump = wcnss_ramdump;
	drv->subsys_desc.crash_shutdown = crash_shutdown;
	drv->subsys_desc.start = pronto_start;
	drv->subsys_desc.stop = pronto_stop;

	ret = of_get_named_gpio(pdev->dev.of_node,
			"qcom,gpio-err-ready", 0);
	if (ret < 0)
		return ret;

	ret = gpio_to_irq(ret);
	if (ret < 0)
		return ret;

	drv->subsys_desc.err_ready_irq = ret;

	INIT_DELAYED_WORK(&drv->cancel_vote_work, wcnss_post_bootup);

	drv->subsys = subsys_register(&drv->subsys_desc);
	if (IS_ERR(drv->subsys)) {
		ret = PTR_ERR(drv->subsys);
		goto err_subsys;
	}

	ret = devm_request_irq(&pdev->dev, drv->irq, wcnss_wdog_bite_irq_hdlr,
			IRQF_TRIGGER_HIGH, "wcnss_wdog", drv);
	if (ret < 0)
		goto err_irq;

	ret = devm_request_irq(&pdev->dev, drv->err_fatal_irq,
			wcnss_err_fatal_intr_handler,
			IRQF_TRIGGER_RISING, "pil-pronto", drv);
	if (ret < 0) {
		dev_err(&pdev->dev, "Unable to register SMP2P err fatal handler!\n");
		goto err_irq;
	}

	drv->ramdump_dev = create_ramdump_device("pronto", &pdev->dev);
	if (!drv->ramdump_dev) {
		ret = -ENOMEM;
		goto err_irq;
	}

	/* Initialize common_ss GDSCR to wait 4 cycles between states */
	regval = readl_relaxed(drv->base + PRONTO_PMU_COMMON_GDSCR)
		& PRONTO_PMU_COMMON_GDSCR_SW_COLLAPSE;
	regval |= (2 << EN_REST_WAIT) | (2 << EN_FEW_WAIT)
		  | (2 << CLK_DIS_WAIT);
	writel_relaxed(regval, drv->base + PRONTO_PMU_COMMON_GDSCR);

	return 0;

err_irq:
	subsys_unregister(drv->subsys);
err_subsys:
	pil_desc_release(desc);
	return ret;
}

static int __devexit pil_pronto_remove(struct platform_device *pdev)
{
	struct pronto_data *drv = platform_get_drvdata(pdev);
	subsys_unregister(drv->subsys);
	pil_desc_release(&drv->desc);
	destroy_ramdump_device(drv->ramdump_dev);
	return 0;
}

static struct of_device_id msm_pil_pronto_match[] = {
	{.compatible = "qcom,pil-pronto"},
	{}
};

static struct platform_driver pil_pronto_driver = {
	.probe = pil_pronto_probe,
	.remove = __devexit_p(pil_pronto_remove),
	.driver = {
		.name = "pil_pronto",
		.owner = THIS_MODULE,
		.of_match_table = msm_pil_pronto_match,
	},
};

static int __init pil_pronto_init(void)
{
	return platform_driver_register(&pil_pronto_driver);
}
module_init(pil_pronto_init);

static void __exit pil_pronto_exit(void)
{
	platform_driver_unregister(&pil_pronto_driver);
}
module_exit(pil_pronto_exit);

MODULE_DESCRIPTION("Support for booting PRONTO (WCNSS) processors");
MODULE_LICENSE("GPL v2");
