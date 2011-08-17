/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <mach/sdio_al.h>
#include <mach/sdio_tty.h>

#define SDIO_TTY_DEV		"sdio_tty_ciq_0"
#define SDIO_CIQ		"sdio_ciq"
#define SDIO_TTY_DEV_TEST	"sdio_tty_ciq_test_0"
#define TTY_CIQ_MODULE_NAME	"sdio_tty_ciq"

struct sdio_tty_ciq {
	void *sdio_tty_handle;
};

static struct sdio_tty_ciq sdio_tty_ciq;

/*
 * Enable sdio_tty debug messages
 * By default the sdio_tty debug messages are turned off
 */
static int debug_msg_on;
module_param(debug_msg_on, int, 0);

static int sdio_tty_probe(struct platform_device *pdev)
{
	sdio_tty_ciq.sdio_tty_handle = sdio_tty_init_tty(SDIO_TTY_DEV,
							 "SDIO_CIQ");
	if (!sdio_tty_ciq.sdio_tty_handle) {
		pr_err(TTY_CIQ_MODULE_NAME ": %s: NULL sdio_tty_handle",
		       __func__);
		return -ENODEV;
	}

	if (debug_msg_on)
		sdio_tty_enable_debug_msg(sdio_tty_ciq.sdio_tty_handle,
					  debug_msg_on);

	return 0;
}

static int sdio_tty_test_probe(struct platform_device *pdev)
{
	sdio_tty_ciq.sdio_tty_handle = sdio_tty_init_tty(SDIO_TTY_DEV_TEST,
							 "SDIO_CIQ");
	if (!sdio_tty_ciq.sdio_tty_handle) {
		pr_err(TTY_CIQ_MODULE_NAME ": %s: NULL sdio_tty_handle",
		       __func__);
		return -ENODEV;
	}

	if (debug_msg_on)
		sdio_tty_enable_debug_msg(sdio_tty_ciq.sdio_tty_handle,
					  debug_msg_on);

	return 0;
}

static int sdio_tty_remove(struct platform_device *pdev)
{
	int ret = 0;

	pr_info(TTY_CIQ_MODULE_NAME ": %s", __func__);
	ret = sdio_tty_uninit_tty(sdio_tty_ciq.sdio_tty_handle);
	if (ret) {
		pr_err(TTY_CIQ_MODULE_NAME ": %s: sdio_tty_uninit_tty failed",
		       __func__);
		return ret;
	}

	return 0;
}

static struct platform_driver sdio_tty_pdrv = {
	.probe		= sdio_tty_probe,
	.remove		= sdio_tty_remove,
	.driver		= {
		.name	= "SDIO_CIQ",
		.owner	= THIS_MODULE,
	},
};

static struct platform_driver sdio_tty_test_pdrv = {
	.probe		= sdio_tty_test_probe,
	.remove		= sdio_tty_remove,
	.driver		= {
		.name	= "SDIO_CIQ_TEST",
		.owner	= THIS_MODULE,
	},
};

static int __init sdio_tty_ciq_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&sdio_tty_pdrv);
	if (ret) {
		pr_err(TTY_CIQ_MODULE_NAME ": %s: platform_driver_register "
					    "failed", __func__);
		return ret;
	}
	return platform_driver_register(&sdio_tty_test_pdrv);
};

/*
 *  Module Exit.
 *
 *  Unregister SDIO driver.
 *
 */
static void __exit sdio_tty_ciq_exit(void)
{
	platform_driver_unregister(&sdio_tty_pdrv);
	platform_driver_unregister(&sdio_tty_test_pdrv);
}

module_init(sdio_tty_ciq_init);
module_exit(sdio_tty_ciq_exit);

MODULE_DESCRIPTION("SDIO TTY CIQ");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Maya Erez <merez@codeaurora.org>");
