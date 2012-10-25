/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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
#include <linux/elf.h>
#include <linux/err.h>
#include <linux/clk.h>

#include <mach/peripheral-loader.h>
#include <mach/subsystem_restart.h>

#include "peripheral-loader.h"
#include "scm-pas.h"

struct vidc_data {
	struct clk *smmu_iface;
	struct clk *core;
	struct pil_device *pil;
	struct subsys_device *subsys;
	struct subsys_desc subsys_desc;
};

static int pil_vidc_init_image(struct pil_desc *pil, const u8 *metadata,
		size_t size)
{
	return pas_init_image(PAS_VIDC, metadata, size);
}

static int pil_vidc_reset(struct pil_desc *pil)
{
	int ret;
	struct vidc_data *drv = dev_get_drvdata(pil->dev);

	ret = clk_prepare_enable(drv->smmu_iface);
	if (ret)
		goto err_smmu;
	ret = clk_prepare_enable(drv->core);
	if (ret)
		goto err_core;
	ret = pas_auth_and_reset(PAS_VIDC);

	clk_disable_unprepare(drv->core);
err_core:
	clk_disable_unprepare(drv->smmu_iface);
err_smmu:
	return ret;
}

static int pil_vidc_shutdown(struct pil_desc *pil)
{
	return pas_shutdown(PAS_VIDC);
}

static struct pil_reset_ops pil_vidc_ops = {
	.init_image = pil_vidc_init_image,
	.auth_and_reset = pil_vidc_reset,
	.shutdown = pil_vidc_shutdown,
};

#define subsys_to_drv(d) container_of(d, struct vidc_data, subsys_desc)

static int vidc_start(const struct subsys_desc *desc)
{
	void *ret;

	ret = pil_get("vidc");
	if (IS_ERR(ret))
		return PTR_ERR(ret);
	return 0;
}

static void vidc_stop(const struct subsys_desc *desc)
{
	struct vidc_data *drv = subsys_to_drv(desc);
	pil_put(drv->pil);
}

static int __devinit pil_vidc_driver_probe(struct platform_device *pdev)
{
	struct pil_desc *desc;
	struct vidc_data *drv;

	if (pas_supported(PAS_VIDC) < 0)
		return -ENOSYS;

	desc = devm_kzalloc(&pdev->dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;

	drv = devm_kzalloc(&pdev->dev, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;
	platform_set_drvdata(pdev, drv);

	drv->smmu_iface = devm_clk_get(&pdev->dev, "smmu_iface_clk");
	if (IS_ERR(drv->smmu_iface))
		return PTR_ERR(drv->smmu_iface);

	drv->core = devm_clk_get(&pdev->dev, "core_clk");
	if (IS_ERR(drv->core))
		return PTR_ERR(drv->core);

	desc->name = "vidc";
	desc->dev = &pdev->dev;
	desc->ops = &pil_vidc_ops;
	desc->owner = THIS_MODULE;
	drv->pil = msm_pil_register(desc);
	if (IS_ERR(drv->pil))
		return PTR_ERR(drv->pil);

	drv->subsys_desc.name = "vidc";
	drv->subsys_desc.dev = &pdev->dev;
	drv->subsys_desc.owner = THIS_MODULE;
	drv->subsys_desc.start = vidc_start;
	drv->subsys_desc.stop = vidc_stop;

	drv->subsys = subsys_register(&drv->subsys_desc);
	if (IS_ERR(drv->subsys)) {
		msm_pil_unregister(drv->pil);
		return PTR_ERR(drv->subsys);
	}
	return 0;
}

static int __devexit pil_vidc_driver_exit(struct platform_device *pdev)
{
	struct vidc_data *drv = platform_get_drvdata(pdev);
	subsys_unregister(drv->subsys);
	msm_pil_unregister(drv->pil);
	return 0;
}

static struct platform_driver pil_vidc_driver = {
	.probe = pil_vidc_driver_probe,
	.remove = __devexit_p(pil_vidc_driver_exit),
	.driver = {
		.name = "pil_vidc",
		.owner = THIS_MODULE,
	},
};

static int __init pil_vidc_init(void)
{
	return platform_driver_register(&pil_vidc_driver);
}
module_init(pil_vidc_init);

static void __exit pil_vidc_exit(void)
{
	platform_driver_unregister(&pil_vidc_driver);
}
module_exit(pil_vidc_exit);

MODULE_DESCRIPTION("Support for secure booting vidc images");
MODULE_LICENSE("GPL v2");
