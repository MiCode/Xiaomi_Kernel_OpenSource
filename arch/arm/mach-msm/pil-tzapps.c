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
#include <linux/err.h>

#include <mach/subsystem_restart.h>
#include <mach/msm_bus_board.h>

#include "peripheral-loader.h"
#include "scm-pas.h"

struct tzapps_data {
	struct pil_desc pil_desc;
	struct subsys_device *subsys;
	struct subsys_desc subsys_desc;
};

static int pil_tzapps_init_image(struct pil_desc *pil, const u8 *metadata,
		size_t size)
{
	return pas_init_image(PAS_TZAPPS, metadata, size);
}

static int pil_tzapps_reset(struct pil_desc *pil)
{
	return pas_auth_and_reset(PAS_TZAPPS);
}

static int pil_tzapps_shutdown(struct pil_desc *pil)
{
	return pas_shutdown(PAS_TZAPPS);
}

static struct pil_reset_ops pil_tzapps_ops = {
	.init_image = pil_tzapps_init_image,
	.auth_and_reset = pil_tzapps_reset,
	.shutdown = pil_tzapps_shutdown,
};

#define subsys_to_drv(d) container_of(d, struct tzapps_data, subsys_desc)

static int tzapps_powerup(const struct subsys_desc *desc)
{
	struct tzapps_data *drv = subsys_to_drv(desc);

	return pil_boot(&drv->pil_desc);
}

static int tzapps_shutdown(const struct subsys_desc *desc, bool force_stop)
{
	struct tzapps_data *drv = subsys_to_drv(desc);
	pil_shutdown(&drv->pil_desc);
	return 0;
}

static int pil_tzapps_driver_probe(struct platform_device *pdev)
{
	struct pil_desc *desc;
	struct tzapps_data *drv;
	int ret;

	if (pas_supported(PAS_TZAPPS) < 0)
		return -ENOSYS;

	drv = devm_kzalloc(&pdev->dev, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;
	platform_set_drvdata(pdev, drv);

	desc = &drv->pil_desc;
	desc->name = "tzapps";
	desc->dev = &pdev->dev;
	desc->ops = &pil_tzapps_ops;
	desc->owner = THIS_MODULE;
	ret = pil_desc_init(desc);
	if (ret)
		return ret;

	drv->subsys_desc.name = "tzapps";
	drv->subsys_desc.dev = &pdev->dev;
	drv->subsys_desc.owner = THIS_MODULE;
	drv->subsys_desc.powerup = tzapps_powerup;
	drv->subsys_desc.shutdown = tzapps_shutdown;
	drv->subsys = subsys_register(&drv->subsys_desc);
	if (IS_ERR(drv->subsys)) {
		pil_desc_release(desc);
		return PTR_ERR(drv->subsys);
	}

	scm_pas_init(MSM_BUS_MASTER_SPS);

	return 0;
}

static int pil_tzapps_driver_exit(struct platform_device *pdev)
{
	struct tzapps_data *drv = platform_get_drvdata(pdev);
	subsys_unregister(drv->subsys);
	pil_desc_release(&drv->pil_desc);
	return 0;
}

static struct platform_driver pil_tzapps_driver = {
	.probe = pil_tzapps_driver_probe,
	.remove = pil_tzapps_driver_exit,
	.driver = {
		.name = "pil_tzapps",
		.owner = THIS_MODULE,
	},
};

static int __init pil_tzapps_init(void)
{
	return platform_driver_register(&pil_tzapps_driver);
}
module_init(pil_tzapps_init);

static void __exit pil_tzapps_exit(void)
{
	platform_driver_unregister(&pil_tzapps_driver);
}
module_exit(pil_tzapps_exit);

MODULE_DESCRIPTION("Support for booting TZApps images");
MODULE_LICENSE("GPL v2");
