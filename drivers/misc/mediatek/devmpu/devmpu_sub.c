// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <asm/page.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/device.h>
#include <linux/compiler.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/arm-smccc.h>
#include <mt-plat/mtk_secure_api.h>
#include <mt_emi.h>
#include <mpu_v1.h>
#include <mpu_platform.h>
#include <devmpu.h>

#define LOG_TAG "[DEVMPU_SUB]"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) LOG_TAG " " fmt

static irqreturn_t devmpu_sub_irq_handler(int irq, void *dev_id)
{
	devmpu_print_violation(0, 0, 0, 0, false);
	return IRQ_HANDLED;
}

/* driver registration */
static int devmpu_sub_probe(struct platform_device *pdev)
{
	int rc;

	void __iomem *reg_base;
	uint32_t virq;

	struct device_node *dn = pdev->dev.of_node;
	struct resource *res;

	pr_info("Device MPU probe\n");

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		pr_err("%s:%d failed to get resource\n",
				__func__, __LINE__);
		return -ENOENT;
	}

	reg_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(reg_base)) {
		pr_err("%s:%d unable to map DEVMPU_BASE\n",
				__func__, __LINE__);
		return -ENOENT;
	}

	virq = irq_of_parse_and_map(dn, 0);
	rc = request_irq(virq, (irq_handler_t)devmpu_sub_irq_handler,
			IRQF_TRIGGER_NONE, "devmpu_sub", NULL);
	if (rc) {
		pr_err("%s:%d failed to request irq, rc=%d\n",
				__func__, __LINE__, rc);
		return -EPERM;
	}

	pr_info("reg_base=0x%pK\n", reg_base);
	pr_info("virq=0x%x\n", virq);

	return 0;
}

static const struct of_device_id devmpu_of_match[] = {
	{ .compatible = "mediatek,device_mpu_sub" },
	{},
};

static struct platform_driver devmpu_sub_drv = {
	.probe = devmpu_sub_probe,
	.driver = {
		.name = "devmpu_sub",
		.owner = THIS_MODULE,
		.of_match_table = devmpu_of_match,
	},
};

static int __init devmpu_sub_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&devmpu_sub_drv);
	if (ret) {
		pr_err("%s:%d failed to register devmpu sub driver, ret=%d\n",
				__func__, __LINE__, ret);
	}

	return ret;
}

static void __exit devmpu_sub_exit(void)
{
}

postcore_initcall(devmpu_sub_init);
module_exit(devmpu_sub_exit);
