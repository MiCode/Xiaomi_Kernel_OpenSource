/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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
#include <linux/err.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/delay.h>

#include <mach/subsystem_restart.h>
#include <mach/ramdump.h>

#include "peripheral-loader.h"
#include "scm-pas.h"

struct bcss_data {
	struct pil_desc desc;
	struct subsys_device *subsys;
	struct subsys_desc subsys_desc;
	bool is_booted;
	void *ramdump_dev;
};

static int pil_bcss_init_image(struct pil_desc *pil,
		const u8 *metadata, size_t size)
{
	return pas_init_image(PAS_BCSS, metadata, size);
}

static int pil_bcss_mem_setup(struct pil_desc *pil, phys_addr_t addr,
			       size_t size)
{
	return pas_mem_setup(PAS_BCSS, addr, size);
}

static int pil_bcss_auth(struct pil_desc *pil)
{
	return pas_auth_and_reset(PAS_BCSS);
}

static int pil_bcss_shutdown(struct pil_desc *pil)
{
	return pas_shutdown(PAS_BCSS);
}

static struct pil_reset_ops pil_bcss_ops = {
	.init_image = pil_bcss_init_image,
	.mem_setup =  pil_bcss_mem_setup,
	.auth_and_reset = pil_bcss_auth,
	.shutdown = pil_bcss_shutdown,
};

#define subsys_to_drv(d) container_of(d, struct bcss_data, subsys_desc)

static int bcss_shutdown(const struct subsys_desc *subsys, bool force_stop)
{
	struct bcss_data *drv = subsys_to_drv(subsys);

	pil_shutdown(&drv->desc);

	return 0;
}

static int bcss_powerup(const struct subsys_desc *subsys)
{
	struct bcss_data *drv = subsys_to_drv(subsys);

	return pil_boot(&drv->desc);
}

static int bcss_ramdump(int enable, const struct subsys_desc *subsys)
{
	struct bcss_data *drv = subsys_to_drv(subsys);

	if (!enable)
		return 0;

	return pil_do_ramdump(&drv->desc, drv->ramdump_dev);
}

static int pil_bcss_driver_probe(struct platform_device *pdev)
{
	struct bcss_data *drv;
	struct pil_desc *desc;
	int ret;

	drv = devm_kzalloc(&pdev->dev, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;
	platform_set_drvdata(pdev, drv);

	desc = &drv->desc;
	ret = of_property_read_string(pdev->dev.of_node, "qcom,firmware-name",
			&desc->name);
	if (ret)
		return ret;

	desc->dev = &pdev->dev;
	desc->owner = THIS_MODULE;

	ret = pas_supported(PAS_BCSS);
	if (ret > 0) {
		desc->ops = &pil_bcss_ops;
		dev_info(&pdev->dev, "using secure boot\n");
	} else {
		dev_err(&pdev->dev, "Secure boot is not supported\n");
		return ret;
	}

	ret = pil_desc_init(desc);
	if (ret)
		return ret;

	drv->subsys_desc.name = desc->name;
	drv->subsys_desc.dev = &pdev->dev;
	drv->subsys_desc.owner = THIS_MODULE;
	drv->subsys_desc.shutdown = bcss_shutdown;
	drv->subsys_desc.powerup = bcss_powerup;
	drv->subsys_desc.ramdump = bcss_ramdump;

	drv->ramdump_dev = create_ramdump_device("bcss", &pdev->dev);
	if (!drv->ramdump_dev) {
		ret = -ENOMEM;
		goto err_ramdump;
	}

	drv->subsys = subsys_register(&drv->subsys_desc);
	if (IS_ERR(drv->subsys)) {
		ret = PTR_ERR(drv->subsys);
		goto err_subsys;
	}
	return ret;

err_subsys:
	destroy_ramdump_device(drv->ramdump_dev);
err_ramdump:
	pil_desc_release(desc);

	return ret;
}

static int pil_bcss_driver_remove(struct platform_device *pdev)
{
	struct bcss_data *drv = platform_get_drvdata(pdev);

	subsys_unregister(drv->subsys);
	destroy_ramdump_device(drv->ramdump_dev);
	pil_desc_release(&drv->desc);

	return 0;
}

static const struct of_device_id msm_pil_bcss_match[] = {
	{.compatible = "qcom,pil-bcss"},
	{}
};

static struct platform_driver pil_bcss_driver = {
	.probe = pil_bcss_driver_probe,
	.remove = pil_bcss_driver_remove,
	.driver = {
		.name = "pil-bcss",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(msm_pil_bcss_match),
	},
};

module_platform_driver(pil_bcss_driver);

MODULE_DESCRIPTION("Support for booting broadcast subsystem");
MODULE_LICENSE("GPL v2");
