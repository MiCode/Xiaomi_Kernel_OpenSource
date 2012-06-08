/*
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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
#include <linux/iopoll.h>
#include <linux/ioport.h>
#include <linux/elf.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>

#include <mach/clk.h>

#include "peripheral-loader.h"
#include "pil-q6v5.h"

/* Q6 Register Offsets */
#define QDSP6SS_RST_EVB			0x010

/* AXI Halting Registers */
#define MSS_Q6_HALT_BASE		0x180
#define MSS_MODEM_HALT_BASE		0x200
#define MSS_NC_HALT_BASE		0x280

/* RMB Status Register Values */
#define STATUS_PBL_SUCCESS		0x1
#define STATUS_XPU_UNLOCKED		0x1
#define STATUS_XPU_UNLOCKED_SCRIBBLED	0x2

/* PBL/MBA interface registers */
#define RMB_MBA_IMAGE			0x00
#define RMB_PBL_STATUS			0x04
#define RMB_MBA_STATUS			0x0C

#define PBL_MBA_WAIT_TIMEOUT_US		100000
#define PROXY_TIMEOUT_MS		10000
#define POLL_INTERVAL_US		50

static int pil_mss_power_up(struct device *dev)
{
	int ret;
	struct q6v5_data *drv = dev_get_drvdata(dev);

	ret = regulator_enable(drv->vreg);
	if (ret)
		dev_err(dev, "Failed to enable regulator.\n");

	return ret;
}

static int pil_mss_power_down(struct device *dev)
{
	struct q6v5_data *drv = dev_get_drvdata(dev);

	return regulator_disable(drv->vreg);
}

static int wait_for_mba_ready(struct device *dev)
{
	struct q6v5_data *drv = dev_get_drvdata(dev);
	int ret;
	u32 status;

	/* Wait for PBL completion. */
	ret = readl_poll_timeout(drv->rmb_base + RMB_PBL_STATUS, status,
		status != 0, POLL_INTERVAL_US, PBL_MBA_WAIT_TIMEOUT_US);
	if (ret) {
		dev_err(dev, "PBL boot timed out\n");
		return ret;
	}
	if (status != STATUS_PBL_SUCCESS) {
		dev_err(dev, "PBL returned unexpected status %d\n", status);
		return -EINVAL;
	}

	/* Wait for MBA completion. */
	ret = readl_poll_timeout(drv->rmb_base + RMB_MBA_STATUS, status,
		status != 0, POLL_INTERVAL_US, PBL_MBA_WAIT_TIMEOUT_US);
	if (ret) {
		dev_err(dev, "MBA boot timed out\n");
		return ret;
	}
	if (status != STATUS_XPU_UNLOCKED &&
	    status != STATUS_XPU_UNLOCKED_SCRIBBLED) {
		dev_err(dev, "MBA returned unexpected status %d\n", status);
		return -EINVAL;
	}

	return 0;
}

static int pil_mss_shutdown(struct pil_desc *pil)
{
	struct q6v5_data *drv = dev_get_drvdata(pil->dev);

	pil_q6v5_halt_axi_port(pil, drv->axi_halt_base + MSS_Q6_HALT_BASE);
	pil_q6v5_halt_axi_port(pil, drv->axi_halt_base + MSS_MODEM_HALT_BASE);
	pil_q6v5_halt_axi_port(pil, drv->axi_halt_base + MSS_NC_HALT_BASE);

	/*
	 * If the shutdown function is called before the reset function, clocks
	 * and power will not be enabled yet. Enable them here so that register
	 * writes performed during the shutdown succeed.
	 */
	if (drv->is_booted == false) {
		pil_mss_power_up(pil->dev);
		pil_q6v5_enable_clks(pil);
	}
	pil_q6v5_shutdown(pil);

	pil_q6v5_disable_clks(pil);
	pil_mss_power_down(pil->dev);

	writel_relaxed(1, drv->restart_reg);

	drv->is_booted = false;

	return 0;
}

static int pil_mss_reset(struct pil_desc *pil)
{
	struct q6v5_data *drv = dev_get_drvdata(pil->dev);
	int ret;

	writel_relaxed(0, drv->restart_reg);
	mb();

	/*
	 * Bring subsystem out of reset and enable required
	 * regulators and clocks.
	 */
	ret = pil_mss_power_up(pil->dev);
	if (ret)
		goto err_power;

	ret = pil_q6v5_enable_clks(pil);
	if (ret)
		goto err_clks;

	/* Program Image Address */
	if (drv->self_auth)
		writel_relaxed(drv->start_addr, drv->rmb_base + RMB_MBA_IMAGE);
	else
		writel_relaxed((drv->start_addr >> 4) & 0x0FFFFFF0,
				drv->reg_base + QDSP6SS_RST_EVB);

	ret = pil_q6v5_reset(pil);
	if (ret)
		goto err_q6v5_reset;

	/* Wait for MBA to start. Check for PBL and MBA errors while waiting. */
	if (drv->self_auth) {
		ret = wait_for_mba_ready(pil->dev);
		if (ret)
			goto err_auth;
	}

	drv->is_booted = true;

	return 0;

err_auth:
	pil_q6v5_shutdown(pil);
err_q6v5_reset:
	pil_q6v5_disable_clks(pil);
err_clks:
	pil_mss_power_down(pil->dev);
err_power:
	return ret;
}

static struct pil_reset_ops pil_mss_ops = {
	.init_image = pil_q6v5_init_image,
	.proxy_vote = pil_q6v5_make_proxy_votes,
	.proxy_unvote = pil_q6v5_remove_proxy_votes,
	.auth_and_reset = pil_mss_reset,
	.shutdown = pil_mss_shutdown,
};

static int __devinit pil_mss_driver_probe(struct platform_device *pdev)
{
	struct q6v5_data *drv;
	struct pil_desc *desc;
	struct resource *res;
	int ret;

	desc = pil_q6v5_init(pdev);
	if (IS_ERR(desc))
		return PTR_ERR(desc);
	drv = platform_get_drvdata(pdev);
	if (drv == NULL)
		return -ENODEV;

	desc->ops = &pil_mss_ops;
	desc->owner = THIS_MODULE;
	desc->proxy_timeout = PROXY_TIMEOUT_MS;

	of_property_read_u32(pdev->dev.of_node, "qcom,pil-self-auth",
			     &drv->self_auth);
	if (drv->self_auth) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
		drv->rmb_base = devm_ioremap(&pdev->dev, res->start,
					     resource_size(res));
		if (!drv->rmb_base)
			return -ENOMEM;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	drv->restart_reg = devm_ioremap(&pdev->dev, res->start,
					resource_size(res));
	if (!drv->restart_reg)
		return -ENOMEM;

	drv->vreg = devm_regulator_get(&pdev->dev, "vdd_mss");
	if (IS_ERR(drv->vreg))
		return PTR_ERR(drv->vreg);

	ret = regulator_set_voltage(drv->vreg, 1150000, 1150000);
	if (ret)
		dev_err(&pdev->dev, "Failed to set regulator's voltage.\n");

	ret = regulator_set_optimum_mode(drv->vreg, 100000);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to set regulator's mode.\n");
		return ret;
	}

	drv->mem_clk = devm_clk_get(&pdev->dev, "mem_clk");
	if (IS_ERR(drv->mem_clk))
		return PTR_ERR(drv->mem_clk);

	drv->pil = msm_pil_register(desc);
	if (IS_ERR(drv->pil))
		return PTR_ERR(drv->pil);

	return 0;
}

static int __devexit pil_mss_driver_exit(struct platform_device *pdev)
{
	struct q6v5_data *drv = platform_get_drvdata(pdev);
	msm_pil_unregister(drv->pil);
	return 0;
}

static struct of_device_id mss_match_table[] = {
	{ .compatible = "qcom,pil-q6v5-mss" },
	{}
};

static struct platform_driver pil_mss_driver = {
	.probe = pil_mss_driver_probe,
	.remove = __devexit_p(pil_mss_driver_exit),
	.driver = {
		.name = "pil-q6v5-mss",
		.of_match_table = mss_match_table,
		.owner = THIS_MODULE,
	},
};

static int __init pil_mss_init(void)
{
	return platform_driver_register(&pil_mss_driver);
}
module_init(pil_mss_init);

static void __exit pil_mss_exit(void)
{
	platform_driver_unregister(&pil_mss_driver);
}
module_exit(pil_mss_exit);

MODULE_DESCRIPTION("Support for booting modem subsystems with QDSP6v5 Hexagon processors");
MODULE_LICENSE("GPL v2");
