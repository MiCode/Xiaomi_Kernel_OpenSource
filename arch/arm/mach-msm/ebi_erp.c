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

#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/proc_fs.h>
#include <linux/cpu.h>

#define MODULE_NAME "msm_ebi_erp"

#define EBI_ERR_ADDR		0x100
#define SLV_ERR_APACKET_0	0x108
#define SLV_ERR_APACKET_1	0x10C
#define SLV_ERR_CNTL		0x114

#define CNTL_ERR_OCCURRED	BIT(4)
#define CNTL_CLEAR_ERR		BIT(8)
#define CNTL_IRQ_EN		BIT(12)

#define AMID_MASK		0xFFFF
#define ERR_AWRITE		BIT(0)
#define ERR_AOOOWR		BIT(1)
#define ERR_AOOORD		BIT(2)
#define ERR_APTORNS		BIT(3)
#define ERR_ALOCK_SHIFT		6
#define ERR_ALOCK_MASK		0x3
#define ERR_ATYPE_SHIFT		8
#define ERR_ATYPE_MASK		0xF
#define ERR_ABURST		BIT(12)
#define ERR_ASIZE_SHIFT		13
#define ERR_ASIZE_MASK		0x7
#define ERR_ATID_SHIFT		16
#define ERR_ATID_MASK		0xFF
#define ERR_ALEN_SHIFT		24
#define ERR_ALEN_MASK		0xF

#define ERR_CODE_DECODE_ERROR	BIT(0)
#define ERR_CODE_MPU_ERROR	BIT(1)

struct msm_ebi_erp_data {
	void __iomem *base;
	struct device *dev;
};

static const char *err_lock_types[4] = {
	"normal",
	"exclusive",
	"locked",
	"barrier",
};

static const char *err_sizes[8] = {
	"byte",
	"half word",
	"word",
	"double word",
	"reserved_4",
	"reserved_5",
	"reserved_6",
	"reserved_7",
};

static irqreturn_t msm_ebi_irq(int irq, void *dev_id)
{
	struct msm_ebi_erp_data *drvdata = dev_id;
	void __iomem *base = drvdata->base;
	unsigned int err_addr, err_apacket0, err_apacket1, err_cntl;

	err_addr = readl_relaxed(base + EBI_ERR_ADDR);
	err_apacket0 = readl_relaxed(base + SLV_ERR_APACKET_0);
	err_apacket1 = readl_relaxed(base + SLV_ERR_APACKET_1);
	err_cntl = readl_relaxed(base + SLV_ERR_CNTL);

	if (!(err_cntl & CNTL_ERR_OCCURRED))
		return IRQ_NONE;

	pr_alert("EBI error detected!\n");
	pr_alert("\tDevice   = %s\n", dev_name(drvdata->dev));
	pr_alert("\tERR_ADDR = 0x%08x\n", err_addr);
	pr_alert("\tAPACKET0 = 0x%08x\n", err_apacket0);
	pr_alert("\tAPACKET1 = 0x%08x\n", err_apacket1);
	pr_alert("\tERR_CNTL = 0x%08x\n", err_cntl);

	pr_alert("\tAMID     = 0x%08x\n", err_apacket0 & AMID_MASK);
	pr_alert("\tType     = %s, %s, %s\n",
		err_apacket1 & ERR_AWRITE ? "write" : "read",
		err_sizes[(err_apacket1 >> ERR_ASIZE_SHIFT) & ERR_ASIZE_MASK],
		err_apacket1 & ERR_APTORNS ? "non-secure" : "secure");

	pr_alert("\tALOCK    = %s\n",
	    err_lock_types[(err_apacket1 >> ERR_ALOCK_SHIFT) & ERR_ALOCK_MASK]);

	pr_alert("\tABURST   = %s\n", err_apacket1 & ERR_ABURST ?
						"increment" : "wrap");

	pr_alert("\tCODE     = %s %s\n", err_cntl & ERR_CODE_DECODE_ERROR ?
						"decode error" : "",
					 err_cntl & ERR_CODE_MPU_ERROR ?
						"mpu error" : "");
	err_cntl |= CNTL_CLEAR_ERR;
	writel_relaxed(err_cntl, base + SLV_ERR_CNTL);
	mb();	/* Ensure interrupt is cleared before returning */
	return IRQ_HANDLED;
}

static int __devinit msm_ebi_erp_probe(struct platform_device *pdev)
{
	struct resource *r;
	struct msm_ebi_erp_data *drvdata;
	int ret, irq;
	unsigned int err_cntl;

	drvdata = devm_kzalloc(&pdev->dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	drvdata->dev = &pdev->dev;
	platform_set_drvdata(pdev, drvdata);

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!r)
		return -EINVAL;

	drvdata->base = devm_ioremap(&pdev->dev, r->start, resource_size(r));
	if (!drvdata->base)
		return -ENOMEM;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	ret = devm_request_irq(&pdev->dev, irq, msm_ebi_irq, IRQF_TRIGGER_HIGH,
			       dev_name(&pdev->dev), drvdata);
	if (ret)
		return ret;

	/* Enable the interrupt */
	err_cntl = readl_relaxed(drvdata->base + SLV_ERR_CNTL);
	err_cntl |= CNTL_IRQ_EN;
	writel_relaxed(err_cntl, drvdata->base + SLV_ERR_CNTL);
	mb();	/* Ensure interrupt is enabled before returning */
	return 0;
}

static int msm_ebi_erp_remove(struct platform_device *pdev)
{
	struct msm_ebi_erp_data *drvdata = platform_get_drvdata(pdev);
	unsigned int err_cntl;

	/* Disable the interrupt */
	err_cntl = readl_relaxed(drvdata->base + SLV_ERR_CNTL);
	err_cntl &= ~CNTL_IRQ_EN;
	writel_relaxed(err_cntl, drvdata->base + SLV_ERR_CNTL);
	mb();	/* Ensure interrupt is disabled before returning */
	return 0;
}

static struct platform_driver msm_ebi_erp_driver = {
	.probe = msm_ebi_erp_probe,
	.remove = __devexit_p(msm_ebi_erp_remove),
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init msm_ebi_erp_init(void)
{
	return platform_driver_register(&msm_ebi_erp_driver);
}

static void __exit msm_ebi_erp_exit(void)
{
	platform_driver_unregister(&msm_ebi_erp_driver);
}


module_init(msm_ebi_erp_init);
module_exit(msm_ebi_erp_exit);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MSM cache error reporting driver");
