/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/err.h>

#include "qdss.h"

#define tpiu_writel(tpiu, val, off)	__raw_writel((val), tpiu.base + off)
#define tpiu_readl(tpiu, off)		__raw_readl(tpiu.base + off)

#define TPIU_SUPPORTED_PORT_SIZE			(0x000)
#define TPIU_CURRENT_PORT_SIZE				(0x004)
#define TPIU_SUPPORTED_TRIGGER_MODES			(0x100)
#define TPIU_TRIGGER_COUNTER_VALUE			(0x104)
#define TPIU_TRIGGER_MULTIPLIER				(0x108)
#define TPIU_SUPPORTED_TEST_PATTERNM			(0x200)
#define TPIU_CURRENT_TEST_PATTERNM			(0x204)
#define TPIU_TEST_PATTERN_REPEAT_COUNTER		(0x208)
#define TPIU_FORMATTER_AND_FLUSH_STATUS			(0x300)
#define TPIU_FORMATTER_AND_FLUSH_CONTROL		(0x304)
#define TPIU_FORMATTER_SYNCHRONIZATION_COUNTER		(0x308)
#define TPIU_EXTCTL_IN_PORT				(0x400)
#define TPIU_EXTCTL_OUT_PORT				(0x404)
#define TPIU_ITTRFLINACK				(0xEE4)
#define TPIU_ITTRFLIN					(0xEE8)
#define TPIU_ITATBDATA0					(0xEEC)
#define TPIU_ITATBCTR2					(0xEF0)
#define TPIU_ITATBCTR1					(0xEF4)
#define TPIU_ITATBCTR0					(0xEF8)


#define TPIU_LOCK()							\
do {									\
	mb();								\
	tpiu_writel(tpiu, 0x0, CS_LAR);					\
} while (0)
#define TPIU_UNLOCK()							\
do {									\
	tpiu_writel(tpiu, CS_UNLOCK_MAGIC, CS_LAR);			\
	mb();								\
} while (0)

struct tpiu_ctx {
	void __iomem	*base;
	bool		enabled;
	struct device	*dev;
};

static struct tpiu_ctx tpiu;

static void __tpiu_disable(void)
{
	TPIU_UNLOCK();

	tpiu_writel(tpiu, 0x3000, TPIU_FORMATTER_AND_FLUSH_CONTROL);
	tpiu_writel(tpiu, 0x3040, TPIU_FORMATTER_AND_FLUSH_CONTROL);

	TPIU_LOCK();
}

void tpiu_disable(void)
{
	__tpiu_disable();
	tpiu.enabled = false;
	dev_info(tpiu.dev, "tpiu disabled\n");
}

static int __devinit tpiu_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		ret = -EINVAL;
		goto err_res;
	}

	tpiu.base = ioremap_nocache(res->start, resource_size(res));
	if (!tpiu.base) {
		ret = -EINVAL;
		goto err_ioremap;
	}

	tpiu.dev = &pdev->dev;

	return 0;

err_ioremap:
err_res:
	return ret;
}

static int tpiu_remove(struct platform_device *pdev)
{
	if (tpiu.enabled)
		tpiu_disable();
	iounmap(tpiu.base);

	return 0;
}

static struct platform_driver tpiu_driver = {
	.probe          = tpiu_probe,
	.remove         = tpiu_remove,
	.driver         = {
		.name   = "msm_tpiu",
	},
};

int __init tpiu_init(void)
{
	return platform_driver_register(&tpiu_driver);
}

void tpiu_exit(void)
{
	platform_driver_unregister(&tpiu_driver);
}
