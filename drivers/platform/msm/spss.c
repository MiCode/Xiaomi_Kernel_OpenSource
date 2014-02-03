/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <mach/msm_iomap.h>

#define CLOCK_CONTROL_REG      0x00
#define BUS_SMCBC_REG          0x04
#define PSCBC_BUS_REG          0x0C
#define PSCBC_GENI_REG         0x10

#define STANDBY_MODE            0x2
#define ACTIVE_MODE             0x1

#define ASYNC_SW_CLK_EN         0x2

struct msm_spss_dev_t {
	void __iomem		  *base;
	struct clk                *clk;
};

static struct msm_spss_dev_t msm_spss_dev;

static int msm_spss_probe(struct platform_device *pdev)
{
	struct resource *res;
	int rc = 0;

	msm_spss_dev.clk = clk_get(&pdev->dev, "iface_clk");
	if (IS_ERR(msm_spss_dev.clk)) {
		rc = PTR_ERR(msm_spss_dev.clk);
		dev_err(&pdev->dev, "could not get ahb clk %d\n", rc);
		goto err_clk_get;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "base");
	if (!res) {
		dev_err(&pdev->dev, "missing memory resource\n");
		rc = -EINVAL;
		goto err_res;
	}

	msm_spss_dev.base = ioremap(res->start, resource_size(res));
	if (!msm_spss_dev.base) {
		dev_err(&pdev->dev, "ioremap failed\n");
		rc = -ENOMEM;
		goto err_ioremap;
	}

	rc = of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);
	if (rc) {
		dev_err(&pdev->dev, "of_platform_populate failed %d\n", rc);
		goto err_of_plat_pop;
	}

	rc = clk_prepare_enable(msm_spss_dev.clk);
	if (rc) {
		dev_err(&pdev->dev, "ahb clk enable failed %d\n", rc);
		goto err_clk_en;
	}

	writel_relaxed(ASYNC_SW_CLK_EN, (msm_spss_dev.base + BUS_SMCBC_REG));
	writel_relaxed(ASYNC_SW_CLK_EN, (msm_spss_dev.base + PSCBC_BUS_REG));
	writel_relaxed(ASYNC_SW_CLK_EN, (msm_spss_dev.base + PSCBC_GENI_REG));

	clk_disable_unprepare(msm_spss_dev.clk);

	return 0;

err_clk_en:
err_of_plat_pop:
	iounmap(msm_spss_dev.base);
err_ioremap:
err_res:
	clk_put(msm_spss_dev.clk);
err_clk_get:
	return rc;
}

static int __exit msm_spss_remove(struct platform_device *pdev)
{
	if (msm_spss_dev.base)
		iounmap(msm_spss_dev.base);

	if (msm_spss_dev.clk)
		clk_put(msm_spss_dev.clk);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int msm_spss_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	int rc;
	u32 val;

	rc = clk_prepare_enable(msm_spss_dev.clk);
	if (rc) {
		dev_err(&pdev->dev, "ahb clk enable failed %d\n", rc);
		return rc;
	}

	val = readl_relaxed(msm_spss_dev.base + CLOCK_CONTROL_REG);
	val &= ~ACTIVE_MODE;
	val |= STANDBY_MODE;
	writel_relaxed(val, (msm_spss_dev.base + CLOCK_CONTROL_REG));
	wmb();

	clk_disable_unprepare(msm_spss_dev.clk);
	return 0;
}

static int msm_spss_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	int rc;
	u32 val;

	rc = clk_prepare_enable(msm_spss_dev.clk);
	if (rc) {
		dev_err(&pdev->dev, "ahb clk enable failed %d\n", rc);
		return rc;
	}

	val = readl_relaxed(msm_spss_dev.base + CLOCK_CONTROL_REG);
	val &= ~STANDBY_MODE;
	val |= ACTIVE_MODE;
	writel_relaxed(val, (msm_spss_dev.base + CLOCK_CONTROL_REG));
	wmb();

	clk_disable_unprepare(msm_spss_dev.clk);
	return 0;
}
#endif /* CONFIG_PM_SLEEP */

static const struct dev_pm_ops msm_spss_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(
		msm_spss_suspend,
		msm_spss_resume
	)
};

static struct of_device_id msm_spss_match[] = {
	{	.compatible = "qcom,msm-spss",
	},
	{}
};

static struct platform_driver msm_spss_driver = {
	.probe	= msm_spss_probe,
	.remove	= msm_spss_remove,
	.driver	= {
		.name		= "msm_spss",
		.owner		= THIS_MODULE,
		.pm		= &msm_spss_dev_pm_ops,
		.of_match_table	= msm_spss_match,
	},
};

static int __init spss_init(void)
{
	return platform_driver_register(&msm_spss_driver);
}

static void __exit spss_exit(void)
{
	platform_driver_unregister(&msm_spss_driver);
}

module_init(spss_init);
module_exit(spss_exit);

MODULE_LICENSE("GPL v2");
