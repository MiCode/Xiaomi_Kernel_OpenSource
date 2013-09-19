/* Copyright (c)2013, The Linux Foundation. All rights reserved.
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
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>

#include <mach/subsystem_restart.h>
#include <mach/msm_bus_board.h>
#include <mach/msm_bus.h>
#include <mach/ramdump.h>

#include "peripheral-loader.h"
#include "scm-pas.h"

/* PIL proxy vote timeout */
#define VPU_PROXY_TIMEOUT_MS				10000

static const char * const clk_names[] = {
	"core_clk",
	"iface_clk",
	"bus_clk",
	"vdp_clk",
	"vdp_bus_clk",
	"cxo_clk",
	"sleep_clk",
	"maple_bus_clk"
};

struct vpu_data {
	struct pil_desc desc;
	struct subsys_device *subsys;
	struct subsys_desc subsys_desc;
	struct regulator *gdsc;
	struct clk *clks[ARRAY_SIZE(clk_names)];
	void *ramdump_dev;
};

#define subsys_to_drv(d) container_of(d, struct vpu_data, subsys_desc)

/* Get vpu clocks and set rates for rate-settable clocks */
static int vpu_clock_setup(struct device *dev)
{
	struct vpu_data *drv = dev_get_drvdata(dev);
	int i;

	for (i = 0; i < ARRAY_SIZE(drv->clks); i++) {
		drv->clks[i] = devm_clk_get(dev, clk_names[i]);
		if (IS_ERR(drv->clks[i])) {
			dev_err(dev, "failed to get %s\n",
				clk_names[i]);
			return PTR_ERR(drv->clks[i]);
		}
		/* Make sure rate-settable clocks' rates are set */
		if (clk_get_rate(drv->clks[i]) == 0)
			clk_set_rate(drv->clks[i],
				     clk_round_rate(drv->clks[i], 0));
	}

	return 0;
}

static int vpu_clock_prepare_enable(struct device *dev)
{
	struct vpu_data *drv = dev_get_drvdata(dev);
	int rc, i;

	for (i = 0; i < ARRAY_SIZE(drv->clks); i++) {
		rc = clk_prepare_enable(drv->clks[i]);
		if (rc) {
			dev_err(dev, "failed to enable %s\n",
				clk_names[i]);
			for (i--; i >= 0; i--)
				clk_disable_unprepare(drv->clks[i]);
			return rc;
		}
	}

	return 0;
}

static void vpu_clock_disable_unprepare(struct device *dev)
{
	struct vpu_data *drv = dev_get_drvdata(dev);
	int i;

	for (i = 0; i < ARRAY_SIZE(drv->clks); i++)
		clk_disable_unprepare(drv->clks[i]);
}

static int pil_vpu_make_proxy_vote(struct pil_desc *pil)
{
	struct vpu_data *drv = dev_get_drvdata(pil->dev);
	int rc;

	/*
	 * Clocks need to be proxy voted to be able to pass control
	 * of clocks from PIL driver to the VPU driver. But GDSC
	 * needs to be turned on before clocks can be turned on. So
	 * enable the GDSC here.
	 */
	rc = regulator_enable(drv->gdsc);
	if (rc) {
		dev_err(pil->dev, "GDSC enable failed\n");
		goto err_regulator;
	}

	rc = vpu_clock_prepare_enable(pil->dev);
	if (rc) {
		dev_err(pil->dev, "clock prepare and enable failed\n");
		goto err_clock;
	}

	return 0;

err_clock:
	regulator_disable(drv->gdsc);
err_regulator:
	return rc;
}

static void pil_vpu_remove_proxy_vote(struct pil_desc *pil)
{
	struct vpu_data *drv = dev_get_drvdata(pil->dev);

	vpu_clock_disable_unprepare(pil->dev);

	/* Disable GDSC */
	regulator_disable(drv->gdsc);
}

static int pil_vpu_init_image_trusted(struct pil_desc *pil,
		const u8 *metadata, size_t size)
{
	return pas_init_image(PAS_VPU, metadata, size);
}

static int pil_vpu_mem_setup_trusted(struct pil_desc *pil, phys_addr_t addr,
			       size_t size)
{
	return pas_mem_setup(PAS_VPU, addr, size);
}

static int pil_vpu_reset_trusted(struct pil_desc *pil)
{
	int rc;
	struct vpu_data *drv = dev_get_drvdata(pil->dev);

	/*
	 * GDSC needs to remain on till VPU is shutdown. So, enable
	 * the GDSC here again to make sure it remains on beyond the
	 * expiry of the proxy vote timer.
	 */
	rc = regulator_enable(drv->gdsc);
	if (rc) {
		dev_err(pil->dev, "GDSC enable failed\n");
		return rc;
	}

	rc = pas_auth_and_reset(PAS_VPU);
	if (rc)
		regulator_disable(drv->gdsc);


	return rc;
}

static int pil_vpu_shutdown_trusted(struct pil_desc *pil)
{
	int rc;
	struct vpu_data *drv = dev_get_drvdata(pil->dev);

	vpu_clock_prepare_enable(pil->dev);

	rc = pas_shutdown(PAS_VPU);

	vpu_clock_disable_unprepare(pil->dev);

	regulator_disable(drv->gdsc);

	return rc;
}

static struct pil_reset_ops pil_vpu_ops_trusted = {
	.init_image = pil_vpu_init_image_trusted,
	.mem_setup = pil_vpu_mem_setup_trusted,
	.auth_and_reset = pil_vpu_reset_trusted,
	.shutdown = pil_vpu_shutdown_trusted,
	.proxy_vote = pil_vpu_make_proxy_vote,
	.proxy_unvote = pil_vpu_remove_proxy_vote,
};

static int vpu_shutdown(const struct subsys_desc *desc, bool force_stop)
{
	struct vpu_data *drv = subsys_to_drv(desc);

	pil_shutdown(&drv->desc);

	return 0;
}

static int vpu_powerup(const struct subsys_desc *desc)
{
	struct vpu_data *drv = subsys_to_drv(desc);

	return pil_boot(&drv->desc);
}

static int vpu_ramdump(int enable, const struct subsys_desc *desc)
{
	struct vpu_data *drv = subsys_to_drv(desc);

	if (!enable)
		return 0;

	return pil_do_ramdump(&drv->desc, drv->ramdump_dev);
}

static int pil_vpu_probe(struct platform_device *pdev)
{
	struct vpu_data *drv;
	struct pil_desc *desc;
	int rc;

	drv = devm_kzalloc(&pdev->dev, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;
	platform_set_drvdata(pdev, drv);

	drv->gdsc = devm_regulator_get(&pdev->dev, "vdd");
	if (IS_ERR(drv->gdsc)) {
		dev_err(&pdev->dev, "Failed to get VPU GDSC\n");
		return -ENODEV;
	}

	rc = vpu_clock_setup(&pdev->dev);
	if (rc) {
		dev_err(&pdev->dev, "Failed to setup VPU clocks\n");
		return rc;
	}

	desc = &drv->desc;
	rc = of_property_read_string(pdev->dev.of_node, "qcom,firmware-name",
					&desc->name);
	if (rc) {
		dev_err(&pdev->dev, "Failed to read the firmware name\n");
		return rc;
	}

	desc->dev = &pdev->dev;
	desc->owner = THIS_MODULE;
	desc->proxy_timeout = VPU_PROXY_TIMEOUT_MS;

	rc = pas_supported(PAS_VPU);
	if (rc > 0) {
		desc->ops = &pil_vpu_ops_trusted;
		dev_info(&pdev->dev, "using secure boot\n");
	} else {
		dev_err(&pdev->dev, "Secure boot is not supported\n");
		return rc;
	}

	drv->ramdump_dev = create_ramdump_device("vpu", &pdev->dev);
	if (!drv->ramdump_dev)
		return -ENOMEM;

	rc = pil_desc_init(desc);
	if (rc)
		goto err_ramdump;

	scm_pas_init(MSM_BUS_MASTER_CRYPTO_CORE0);

	drv->subsys_desc.name = desc->name;
	drv->subsys_desc.owner = THIS_MODULE;
	drv->subsys_desc.dev = &pdev->dev;
	drv->subsys_desc.shutdown = vpu_shutdown;
	drv->subsys_desc.powerup = vpu_powerup;
	drv->subsys_desc.ramdump = vpu_ramdump;

	drv->subsys = subsys_register(&drv->subsys_desc);
	if (IS_ERR(drv->subsys)) {
		rc = PTR_ERR(drv->subsys);
		goto err_subsys;
	}
	return rc;

err_subsys:
	pil_desc_release(desc);
err_ramdump:
	destroy_ramdump_device(drv->ramdump_dev);

	return rc;
}

static int pil_vpu_remove(struct platform_device *pdev)
{
	struct vpu_data *drv = platform_get_drvdata(pdev);
	subsys_unregister(drv->subsys);
	pil_desc_release(&drv->desc);

	return 0;
}

static const struct of_device_id msm_pil_vpu_match[] = {
	{.compatible = "qcom,pil-vpu"},
	{}
};

static struct platform_driver pil_vpu_driver = {
	.probe = pil_vpu_probe,
	.remove = pil_vpu_remove,
	.driver = {
		.name = "pil_vpu",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(msm_pil_vpu_match),
	},
};

module_platform_driver(pil_vpu_driver);

MODULE_DESCRIPTION("Support for booting Maple processors");
MODULE_LICENSE("GPL v2");
