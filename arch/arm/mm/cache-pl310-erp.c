/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/cpu.h>
#include <linux/io.h>
#include <asm/cputype.h>
#include <asm/hardware/cache-l2x0.h>

#define MODULE_NAME "pl310_erp"

struct pl310_drv_data {
	unsigned int irq;
	unsigned int ecntr;
	unsigned int parrt;
	unsigned int parrd;
	unsigned int errwd;
	unsigned int errwt;
	unsigned int errrt;
	unsigned int errrd;
	unsigned int slverr;
	unsigned int decerr;
	void __iomem *base;
	unsigned int intr_mask_reg;
};

#define ECNTR	BIT(0)
#define PARRT	BIT(1)
#define PARRD	BIT(2)
#define ERRWT	BIT(3)
#define ERRWD	BIT(4)
#define ERRRT	BIT(5)
#define ERRRD	BIT(6)
#define SLVERR	BIT(7)
#define DECERR	BIT(8)

static irqreturn_t pl310_erp_irq(int irq, void *dev_id)
{
	struct pl310_drv_data *p = platform_get_drvdata(dev_id);
	uint16_t mask_int_stat, int_clear = 0, error = 0;

	mask_int_stat = readl_relaxed(p->base + L2X0_MASKED_INTR_STAT);

	if (mask_int_stat & ECNTR) {
		pr_alert("Event Counter1/0 Overflow Increment error\n");
		p->ecntr++;
		int_clear = mask_int_stat & ECNTR;
	}

	if (mask_int_stat & PARRT) {
		pr_alert("Read parity error on L2 Tag RAM\n");
		p->parrt++;
		error = 1;
		int_clear = mask_int_stat & PARRT;
	}

	if (mask_int_stat & PARRD) {
		pr_alert("Read parity error on L2 Tag RAM\n");
		p->parrd++;
		error = 1;
		int_clear = mask_int_stat & PARRD;
	}

	if (mask_int_stat & ERRWT) {
		pr_alert("Write error on L2 Tag RAM\n");
		p->errwt++;
		int_clear = mask_int_stat & ERRWT;
	}

	if (mask_int_stat & ERRWD) {
		pr_alert("Write error on L2 Data RAM\n");
		p->errwd++;
		int_clear = mask_int_stat & ERRWD;
	}

	if (mask_int_stat & ERRRT) {
		pr_alert("Read error on L2 Tag RAM\n");
		p->errrt++;
		int_clear = mask_int_stat & ERRRT;
	}

	if (mask_int_stat & ERRRD) {
		pr_alert("Read error on L2 Data RAM\n");
		p->errrd++;
		int_clear = mask_int_stat & ERRRD;
	}

	if (mask_int_stat & DECERR) {
		pr_alert("L2 master port decode error\n");
		p->decerr++;
		int_clear = mask_int_stat & DECERR;
	}

	if (mask_int_stat & SLVERR) {
		pr_alert("L2 slave port error\n");
		p->slverr++;
		int_clear = mask_int_stat & SLVERR;
	}

	writel_relaxed(int_clear, p->base + L2X0_INTR_CLEAR);

	/* Make sure the interrupts are cleared */
	mb();

	/* WARNING will be thrown whenever we receive any L2 interrupt.
	 * Other than parity on tag/data ram, irrespective of the bits
	 * set we will throw a warning.
	 */
	WARN_ON(!error);

	/* Panic in case we encounter parity error in TAG/DATA Ram */
	BUG_ON(error);

	return IRQ_HANDLED;
}

static void pl310_mask_int(struct pl310_drv_data *p, bool enable)
{
	/* L2CC register contents needs to be saved
	 * as it's power rail will be removed during suspend
	 */
	if (enable)
		p->intr_mask_reg = 0x1FF;
	else
		p->intr_mask_reg = 0x0;

	writel_relaxed(p->intr_mask_reg, p->base + L2X0_INTR_MASK);

	/* Make sure Mask is updated */
	mb();

	pr_debug("Mask interrupt 0%x\n",
			readl_relaxed(p->base + L2X0_INTR_MASK));
}

static int pl310_erp_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct pl310_drv_data *p = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE,
		"L2CC Interrupt Number:\t\t\t%d\n"\
		"Event Counter1/0 Overflow Increment:\t%u\n"\
		"Parity Error on L2 Tag RAM (Read):\t%u\n"\
		"Parity Error on L2 Data RAM (Read):\t%u\n"\
		"Error on L2 Tag RAM (Write):\t\t%u\n"\
		"Error on L2 Data RAM (Write):\t\t%u\n"\
		"Error on L2 Tag RAM (Read):\t\t%u\n"\
		"Error on L2 Data RAM (Read):\t\t%u\n"\
		"SLave Error from L3 Port:\t\t%u\n"\
		"Decode Error from L3 Port:\t\t%u\n",
		p->irq, p->ecntr, p->parrt, p->parrd, p->errwt, p->errwd,
		p->errrt, p->errrd, p->slverr, p->decerr);
}

static DEVICE_ATTR(cache_erp, 0664, pl310_erp_show, NULL);

static int __init pl310_create_sysfs(struct device *dev)
{
	/* create a sysfs entry at
	 * /sys/devices/platform/pl310_erp/cache_erp
	 */
	return device_create_file(dev, &dev_attr_cache_erp);
}

static int __devinit pl310_cache_erp_probe(struct platform_device *pdev)
{
	struct resource *r;
	struct pl310_drv_data *drv_data;
	int ret;

	drv_data = devm_kzalloc(&pdev->dev, sizeof(struct pl310_drv_data),
						GFP_KERNEL);
	if  (drv_data == NULL) {
		dev_err(&pdev->dev, "cannot allocate memory\n");
		ret = -ENOMEM;
		goto error;
	}

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!r) {
		dev_err(&pdev->dev, "No L2 base address\n");
		ret = -ENODEV;
		goto error;
	}

	if (!devm_request_mem_region(&pdev->dev, r->start, resource_size(r),
					"erp")) {
		ret = -EBUSY;
		goto error;
	}

	drv_data->base = devm_ioremap_nocache(&pdev->dev, r->start,
						resource_size(r));
	if (!drv_data->base) {
		dev_err(&pdev->dev, "errored to ioremap 0x%x\n", r->start);
		ret = -ENOMEM;
		goto error;
	}
	dev_dbg(&pdev->dev, "L2CC base 0x%p\n", drv_data->base);

	r = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "l2_irq");
	if (!r) {
		dev_err(&pdev->dev, "No L2 IRQ resource\n");
		ret = -ENODEV;
		goto error;
	}

	drv_data->irq = r->start;

	ret = devm_request_irq(&pdev->dev, drv_data->irq, pl310_erp_irq,
			IRQF_TRIGGER_RISING, "l2cc_intr", pdev);
	if (ret) {
		dev_err(&pdev->dev, "request irq for L2 interrupt failed\n");
		goto error;
	}

	platform_set_drvdata(pdev, drv_data);

	pl310_mask_int(drv_data, true);

	ret = pl310_create_sysfs(&pdev->dev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to create sysfs entry\n");
		goto sysfs_err;
	}

	return 0;

sysfs_err:
	platform_set_drvdata(pdev, NULL);
	pl310_mask_int(drv_data, false);
error:
	return  ret;
}

static int __devexit pl310_cache_erp_remove(struct platform_device *pdev)
{
	struct pl310_drv_data *p = platform_get_drvdata(pdev);

	pl310_mask_int(p, false);

	device_remove_file(&pdev->dev, &dev_attr_cache_erp);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

#ifdef CONFIG_PM
static int pl310_suspend(struct device *dev)
{
	struct pl310_drv_data *p = dev_get_drvdata(dev);

	disable_irq(p->irq);

	return 0;
}

static int pl310_resume_early(struct device *dev)
{
	struct pl310_drv_data *p = dev_get_drvdata(dev);

	pl310_mask_int(p, true);

	enable_irq(p->irq);

	return 0;
}

static const struct dev_pm_ops pl310_cache_pm_ops = {
	.suspend = pl310_suspend,
	.resume_early = pl310_resume_early,
};
#endif

static struct platform_driver pl310_cache_erp_driver = {
	.probe = pl310_cache_erp_probe,
	.remove = __devexit_p(pl310_cache_erp_remove),
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &pl310_cache_pm_ops,
#endif
	},
};

static int __init pl310_cache_erp_init(void)
{
	return platform_driver_register(&pl310_cache_erp_driver);
}
module_init(pl310_cache_erp_init);

static void __exit pl310_cache_erp_exit(void)
{
	platform_driver_unregister(&pl310_cache_erp_driver);
}
module_exit(pl310_cache_erp_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("PL310 cache error reporting driver");
