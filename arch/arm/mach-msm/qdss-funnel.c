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
#include <linux/types.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/err.h>

#include "qdss.h"

#define funnel_writel(funnel, id, val, off)	\
			__raw_writel((val), funnel.base + (SZ_4K * id) + off)
#define funnel_readl(funnel, id, off)		\
			__raw_readl(funnel.base + (SZ_4K * id) + off)

#define CS_TFUNNEL_FUNCTL		(0x000)
#define CS_TFUNNEL_PRICTL		(0x004)
#define CS_TFUNNEL_ITATBDATA0		(0xEEC)
#define CS_TFUNNEL_ITATBCTR2		(0xEF0)
#define CS_TFUNNEL_ITATBCTR1		(0xEF4)
#define CS_TFUNNEL_ITATBCTR0		(0xEF8)


#define FUNNEL_LOCK(id)							\
do {									\
	mb();								\
	funnel_writel(funnel, id, 0x0, CS_LAR);				\
} while (0)
#define FUNNEL_UNLOCK(id)						\
do {									\
	funnel_writel(funnel, id, CS_UNLOCK_MAGIC, CS_LAR);		\
	mb();								\
} while (0)

#define DEFAULT_HOLDTIME_MASK		(0xF00)
#define DEFAULT_HOLDTIME_SHFT		(0x8)
#define DEFAULT_HOLDTIME		(0x7 << DEFAULT_HOLDTIME_SHFT)
#define DEFAULT_PRIORITY		(0xFAC680)

struct funnel_ctx {
	void __iomem	*base;
	bool		enabled;
	struct device	*dev;
};

static struct funnel_ctx funnel;

static void __funnel_enable(uint8_t id, uint32_t port_mask)
{
	uint32_t functl;

	FUNNEL_UNLOCK(id);

	functl = funnel_readl(funnel, id, CS_TFUNNEL_FUNCTL);
	functl &= ~DEFAULT_HOLDTIME_MASK;
	functl |= DEFAULT_HOLDTIME;
	functl |= port_mask;
	funnel_writel(funnel, id, functl, CS_TFUNNEL_FUNCTL);
	funnel_writel(funnel, id, DEFAULT_PRIORITY, CS_TFUNNEL_PRICTL);

	FUNNEL_LOCK(id);
}

void funnel_enable(uint8_t id, uint32_t port_mask)
{
	__funnel_enable(id, port_mask);
	funnel.enabled = true;
	dev_info(funnel.dev, "funnel port mask 0x%lx enabled\n",
					(unsigned long) port_mask);
}

static void __funnel_disable(uint8_t id, uint32_t port_mask)
{
	uint32_t functl;

	FUNNEL_UNLOCK(id);

	functl = funnel_readl(funnel, id, CS_TFUNNEL_FUNCTL);
	functl &= ~port_mask;
	funnel_writel(funnel, id, functl, CS_TFUNNEL_FUNCTL);

	FUNNEL_LOCK(id);
}

void funnel_disable(uint8_t id, uint32_t port_mask)
{
	__funnel_disable(id, port_mask);
	funnel.enabled = false;
	dev_info(funnel.dev, "funnel port mask 0x%lx disabled\n",
					(unsigned long) port_mask);
}

static int __devinit funnel_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		ret = -EINVAL;
		goto err_res;
	}

	funnel.base = ioremap_nocache(res->start, resource_size(res));
	if (!funnel.base) {
		ret = -EINVAL;
		goto err_ioremap;
	}

	funnel.dev = &pdev->dev;

	return 0;

err_ioremap:
err_res:
	return ret;
}

static int funnel_remove(struct platform_device *pdev)
{
	if (funnel.enabled)
		funnel_disable(0x0, 0xFF);
	iounmap(funnel.base);

	return 0;
}

static struct platform_driver funnel_driver = {
	.probe          = funnel_probe,
	.remove         = funnel_remove,
	.driver         = {
		.name   = "msm_funnel",
	},
};

int __init funnel_init(void)
{
	return platform_driver_register(&funnel_driver);
}

void funnel_exit(void)
{
	platform_driver_unregister(&funnel_driver);
}
