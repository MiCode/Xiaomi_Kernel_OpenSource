/*
 *
 * (C) COPYRIGHT ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained
 * from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 */



/**
 * pl111_drm_platform.c
 * Implementation of the Linux platform device entrypoints for PL111 DRM
 */
#include <linux/amba/bus.h>
#include <linux/amba/clcd.h>
#include <linux/version.h>
#include <linux/shmem_fs.h>
#include <linux/dma-buf.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>
#include "pl111_drm.h"

static int pl111_platform_drm_suspend(struct platform_device *dev,
					pm_message_t state)
{
	pr_debug("DRM %s\n", __func__);
	return 0;
}

static int pl111_platform_drm_resume(struct platform_device *dev)
{
	pr_debug("DRM %s\n", __func__);
	return 0;
}

int pl111_platform_drm_probe(struct platform_device *dev)
{
	pr_debug("DRM %s\n", __func__);
	return pl111_drm_init(dev);
}

static int pl111_platform_drm_remove(struct platform_device *dev)
{
	pr_debug("DRM %s\n", __func__);
	pl111_drm_exit(dev);

	return 0;
}

static struct amba_id pl111_id_table[] = {
	{
	.id = 0x00041110,
	.mask = 0x000ffffe,
	},
	{0, 0},
};

static struct amba_driver pl111_amba_driver = {
	.drv = {
		.name = "clcd-pl11x",
		},
	.probe = pl111_amba_probe,
	.remove = pl111_amba_remove,
	.id_table = pl111_id_table,
};

static struct platform_driver platform_drm_driver = {
	.probe = pl111_platform_drm_probe,
	.remove = pl111_platform_drm_remove,
	.suspend = pl111_platform_drm_suspend,
	.resume = pl111_platform_drm_resume,
	.driver = {
			.owner = THIS_MODULE,
			.name = DRIVER_NAME,
		},
};

static const struct platform_device_info pl111_drm_pdevinfo = {
	.name = DRIVER_NAME,
	.id = -1,
	.dma_mask = ~0UL
};

static struct platform_device *pl111_drm_device;

static int __init pl111_platform_drm_init(void)
{
	int ret;

	pr_debug("DRM %s\n", __func__);

	pl111_drm_device = platform_device_register_full(&pl111_drm_pdevinfo);
	if (pl111_drm_device == NULL) {
		pr_err("DRM platform_device_register_full() failed\n");
		return -ENOMEM;
	}

	ret = amba_driver_register(&pl111_amba_driver);
	if (ret != 0) {
		pr_err("DRM amba_driver_register() failed %d\n", ret);
		goto err_amba_reg;
	}

	ret = platform_driver_register(&platform_drm_driver);
	if (ret != 0) {
		pr_err("DRM platform_driver_register() failed %d\n", ret);
		goto err_pdrv_reg;
	}

	return 0;

err_pdrv_reg:
	amba_driver_unregister(&pl111_amba_driver);
err_amba_reg:
	platform_device_unregister(pl111_drm_device);

	return ret;
}

static void __exit pl111_platform_drm_exit(void)
{
	pr_debug("DRM %s\n", __func__);

	platform_device_unregister(pl111_drm_device);
	amba_driver_unregister(&pl111_amba_driver);
	platform_driver_unregister(&platform_drm_driver);
}

#ifdef MODULE
module_init(pl111_platform_drm_init);
#else
late_initcall(pl111_platform_drm_init);
#endif
module_exit(pl111_platform_drm_exit);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_VERSION(DRIVER_VERSION);
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_LICENSE(DRIVER_LICENCE);
MODULE_ALIAS(DRIVER_ALIAS);
