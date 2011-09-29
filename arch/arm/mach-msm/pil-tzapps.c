/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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

#include "peripheral-loader.h"
#include "scm-pas.h"

static int nop_verify_blob(struct pil_desc *pil, u32 phy_addr, size_t size)
{
	return 0;
}

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
	.verify_blob = nop_verify_blob,
	.auth_and_reset = pil_tzapps_reset,
	.shutdown = pil_tzapps_shutdown,
};

static int __devinit pil_tzapps_driver_probe(struct platform_device *pdev)
{
	struct pil_desc *desc;

	if (pas_supported(PAS_TZAPPS) < 0)
		return -ENOSYS;

	desc = devm_kzalloc(&pdev->dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;

	desc->name = "tzapps";
	desc->dev = &pdev->dev;
	desc->ops = &pil_tzapps_ops;
	if (msm_pil_register(desc))
		return -EINVAL;
	return 0;
}

static int __devexit pil_tzapps_driver_exit(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver pil_tzapps_driver = {
	.probe = pil_tzapps_driver_probe,
	.remove = __devexit_p(pil_tzapps_driver_exit),
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
