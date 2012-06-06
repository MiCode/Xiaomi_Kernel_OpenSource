/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
#include <linux/regulator/consumer.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/clk.h>

#include <mach/msm_iomap.h>

#include "peripheral-loader.h"
#include "pil-q6v4.h"
#include "scm-pas.h"

#define MSS_S_HCLK_CTL		(MSM_CLK_CTL_BASE + 0x2C70)
#define MSS_SLP_CLK_CTL		(MSM_CLK_CTL_BASE + 0x2C60)
#define SFAB_MSS_M_ACLK_CTL	(MSM_CLK_CTL_BASE + 0x2340)
#define SFAB_MSS_S_HCLK_CTL	(MSM_CLK_CTL_BASE + 0x2C00)
#define MSS_RESET		(MSM_CLK_CTL_BASE + 0x2C64)

struct q6v4_modem {
	struct q6v4_data q6_fw;
	struct q6v4_data q6_sw;
	void __iomem *modem_base;
};

static DEFINE_MUTEX(pil_q6v4_modem_lock);
static unsigned pil_q6v4_modem_count;

/* Bring modem subsystem out of reset */
static void pil_q6v4_init_modem(void __iomem *base, void __iomem *jtag_clk)
{
	mutex_lock(&pil_q6v4_modem_lock);
	if (!pil_q6v4_modem_count) {
		/* Enable MSS clocks */
		writel_relaxed(0x10, SFAB_MSS_M_ACLK_CTL);
		writel_relaxed(0x10, SFAB_MSS_S_HCLK_CTL);
		writel_relaxed(0x10, MSS_S_HCLK_CTL);
		writel_relaxed(0x10, MSS_SLP_CLK_CTL);
		/* Wait for clocks to enable */
		mb();
		udelay(10);

		/* De-assert MSS reset */
		writel_relaxed(0x0, MSS_RESET);
		mb();
		udelay(10);
		/* Enable MSS */
		writel_relaxed(0x7, base);
	}

	/* Enable JTAG clocks */
	/* TODO: Remove if/when Q6 software enables them? */
	writel_relaxed(0x10, jtag_clk);

	pil_q6v4_modem_count++;
	mutex_unlock(&pil_q6v4_modem_lock);
}

/* Put modem subsystem back into reset */
static void pil_q6v4_shutdown_modem(void)
{
	mutex_lock(&pil_q6v4_modem_lock);
	if (pil_q6v4_modem_count)
		pil_q6v4_modem_count--;
	if (pil_q6v4_modem_count == 0)
		writel_relaxed(0x1, MSS_RESET);
	mutex_unlock(&pil_q6v4_modem_lock);
}

static int pil_q6v4_modem_boot(struct pil_desc *pil)
{
	struct q6v4_data *drv = pil_to_q6v4_data(pil);
	struct q6v4_modem *mdm = dev_get_drvdata(pil->dev);
	int err;

	err = pil_q6v4_power_up(drv);
	if (err)
		return err;

	pil_q6v4_init_modem(mdm->modem_base, drv->jtag_clk_reg);
	return pil_q6v4_boot(pil);
}

static int pil_q6v4_modem_shutdown(struct pil_desc *pil)
{
	struct q6v4_data *drv = pil_to_q6v4_data(pil);
	int ret;

	ret = pil_q6v4_shutdown(pil);
	if (ret)
		return ret;
	pil_q6v4_shutdown_modem();
	pil_q6v4_power_down(drv);
	return 0;
}

static struct pil_reset_ops pil_q6v4_modem_ops = {
	.init_image = pil_q6v4_init_image,
	.auth_and_reset = pil_q6v4_modem_boot,
	.shutdown = pil_q6v4_modem_shutdown,
	.proxy_vote = pil_q6v4_make_proxy_votes,
	.proxy_unvote = pil_q6v4_remove_proxy_votes,
};

static struct pil_reset_ops pil_q6v4_modem_ops_trusted = {
	.init_image = pil_q6v4_init_image_trusted,
	.auth_and_reset = pil_q6v4_boot_trusted,
	.shutdown = pil_q6v4_shutdown_trusted,
	.proxy_vote = pil_q6v4_make_proxy_votes,
	.proxy_unvote = pil_q6v4_remove_proxy_votes,
};

static int __devinit
pil_q6v4_proc_init(struct q6v4_data *drv, struct platform_device *pdev, int i)
{
	static const char *name[2] = { "fw", "sw" };
	const struct pil_q6v4_pdata *pdata_p = pdev->dev.platform_data;
	const struct pil_q6v4_pdata *pdata = pdata_p + i;
	char reg_name[12];
	struct pil_desc *desc;
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1 + i);
	if (!res)
		return -EINVAL;

	drv->base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!drv->base)
		return -ENOMEM;

	snprintf(reg_name, sizeof(reg_name), "%s_core_vdd", name[i]);
	drv->vreg = devm_regulator_get(&pdev->dev, reg_name);
	if (IS_ERR(drv->vreg))
		return PTR_ERR(drv->vreg);

	drv->xo = devm_clk_get(&pdev->dev, "xo");
	if (IS_ERR(drv->xo))
		return PTR_ERR(drv->xo);

	desc = &drv->desc;
	desc->name = pdata->name;
	desc->depends_on = pdata->depends;
	desc->dev = &pdev->dev;
	desc->owner = THIS_MODULE;
	desc->proxy_timeout = 10000;
	pil_q6v4_init(drv, pdata);

	if (pas_supported(pdata->pas_id) > 0) {
		desc->ops = &pil_q6v4_modem_ops_trusted;
		dev_info(&pdev->dev, "using secure boot for %s\n", name[i]);
	} else {
		desc->ops = &pil_q6v4_modem_ops;
		dev_info(&pdev->dev, "using non-secure boot for %s\n", name[i]);
	}
	return 0;
}

static int __devinit pil_q6v4_modem_driver_probe(struct platform_device *pdev)
{
	struct q6v4_data *drv_fw, *drv_sw;
	struct q6v4_modem *drv;
	struct resource *res;
	struct regulator *pll_supply;
	int ret;

	drv = devm_kzalloc(&pdev->dev, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;
	platform_set_drvdata(pdev, drv);

	drv_fw = &drv->q6_fw;
	drv_sw = &drv->q6_sw;

	ret = pil_q6v4_proc_init(drv_fw, pdev, 0);
	if (ret)
		return ret;

	ret = pil_q6v4_proc_init(drv_sw, pdev, 1);
	if (ret)
		return ret;

	pll_supply = devm_regulator_get(&pdev->dev, "pll_vdd");
	drv_fw->pll_supply = drv_sw->pll_supply = pll_supply;
	if (IS_ERR(pll_supply))
		return PTR_ERR(pll_supply);

	ret = regulator_set_voltage(pll_supply, 1800000, 1800000);
	if (ret) {
		dev_err(&pdev->dev, "failed to set pll voltage\n");
		return ret;
	}

	ret = regulator_set_optimum_mode(pll_supply, 100000);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to set pll optimum mode\n");
		return ret;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -EINVAL;

	drv->modem_base = devm_ioremap(&pdev->dev, res->start,
			resource_size(res));
	if (!drv->modem_base)
		return -ENOMEM;

	drv_fw->pil = msm_pil_register(&drv_fw->desc);
	if (IS_ERR(drv_fw->pil))
		return PTR_ERR(drv_fw->pil);

	drv_sw->pil = msm_pil_register(&drv_sw->desc);
	if (IS_ERR(drv_sw->pil)) {
		msm_pil_unregister(drv_fw->pil);
		return PTR_ERR(drv_sw->pil);
	}
	return 0;
}

static int __devexit pil_q6v4_modem_driver_exit(struct platform_device *pdev)
{
	struct q6v4_modem *drv = platform_get_drvdata(pdev);
	msm_pil_unregister(drv->q6_sw.pil);
	msm_pil_unregister(drv->q6_fw.pil);
	return 0;
}

static struct platform_driver pil_q6v4_modem_driver = {
	.probe = pil_q6v4_modem_driver_probe,
	.remove = __devexit_p(pil_q6v4_modem_driver_exit),
	.driver = {
		.name = "pil-q6v4-modem",
		.owner = THIS_MODULE,
	},
};

static int __init pil_q6v4_modem_init(void)
{
	return platform_driver_register(&pil_q6v4_modem_driver);
}
module_init(pil_q6v4_modem_init);

static void __exit pil_q6v4_modem_exit(void)
{
	platform_driver_unregister(&pil_q6v4_modem_driver);
}
module_exit(pil_q6v4_modem_exit);

MODULE_DESCRIPTION("Support for booting QDSP6v4 (Hexagon) processors");
MODULE_LICENSE("GPL v2");
