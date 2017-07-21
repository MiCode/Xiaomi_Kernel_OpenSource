/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "GICT: %s(): " fmt, __func__
#include <linux/kernel.h>
#include <linux/edac.h>
#include <linux/mutex.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <soc/qcom/scm.h>

#include "edac_mc.h"
#include "edac_device.h"

#ifdef CONFIG_EDAC_GIC_PANIC_ON_CE
#define GICT_PANIC_ON_CE 1
#else
#define GICT_PANIC_ON_CE 0
#endif

#ifdef CONFIG_EDAC_GIC_PANIC_ON_UE
#define GICT_PANIC_ON_UE 1
#else
#define GICT_PANIC_ON_UE 0
#endif

#define EDAC_NODE		"GICT"
#define RECORD_WIDTH(n)		(64 * n)
#define GICT_ERR_FR(n)		(RECORD_WIDTH(n) + 0x00)
#define GICT_ERR_CTRL(n)	(RECORD_WIDTH(n) + 0x08)
#define GICT_ERR_STATUS(n)	(RECORD_WIDTH(n) + 0x10)
#define GICT_ERR_ADDR(n)	(RECORD_WIDTH(n) + 0x18)
#define GICT_ERR_MISC0(n)	(RECORD_WIDTH(n) + 0x20)
#define GICT_ERR_MISC1(n)	(RECORD_WIDTH(n) + 0x28)
#define GICT_ERRGSR		0xE000
#define GICT_ERRDEVARCH		0xFFBC
#define GICT_ERRIDR		0xFFC8
#define GICT_ERRIRQCR0		0xE800
#define GICT_ERRIRQCR1		(GICT_ERRIRQCR0+0x8)

#define GICD_FCTLR		0x0020
#define GICR_FCTLR		0x0020
#define GICR_OFFSET		0x20000


#define RECORD0		"Software error in GICD programming(UEO)"
#define RECORD1		"Correctable SPI RAM errors(CE)"
#define RECORD2		"Uncorrectable SPI RAM errors(UER)"
#define RECORD3		"Correctable SGI RAM errors(CE)"
#define RECORD4		"Uncorrectable SGI RAM errors(UER)"
#define RECORD5		"Correctable TGT cache errors(CE)"
#define RECORD6		"Correctable TGT cache errors(UER)"
#define RECORD7		"Correctable PPI RAM errors(CE)"
#define RECORD8		"Uncorrectable PPI RAM errors(UER)"
#define RECORD9		"Correctable LPI RAM errors(CE)"
#define RECORD10	"Uncorrectable LPI RAM errors(UER)"
#define RECORD11	"Correctable ITS RAM errors(CE)"
#define RECORD12	"Uncorrectable ITS RAM errors(UEO)"
#define RECORD13	"Uncorrectable 2xITS RAM errors(UER)"
#define RECORD14	"Uncorrectable 2xITS RAM errors(UER)"
#define RECORD(N)	RECORD##N

#define ERR_FR_UI_MASK			GENMASK(5, 4)
#define ERR_FR_FI_MASK			GENMASK(7, 6)
#define ERR_FR_UE_MASK			GENMASK(9, 8)
#define ERR_FR_CFI_MASK			GENMASK(11, 10)

#define ERR_CTRL_UI_MASK		BIT(2)
#define ERR_CTRL_FI_MASK		BIT(3)
#define ERR_CTRL_UE_MASK		BIT(4)
#define ERR_CTRL_CFI_MASK		BIT(8)

#define ERR_STATUS_SERR_MASK		GENMASK(7, 0)
#define ERR_STATUS_IERR_MASK		GENMASK(15, 8)
#define ERR_STATUS_UET_MASK		GENMASK(21, 20)
#define ERR_STATUS_CE_MASK		GENMASK(25, 24)
#define ERR_STATUS_MV_MASK		BIT(26)
#define ERR_STATUS_OF_MASK		BIT(27)
#define ERR_STATUS_ER_MASK		BIT(28)
#define ERR_STATUS_UE_MASK		BIT(29)
#define ERR_STATUS_V_MASK		BIT(30)
#define ERR_STATUS_AV_MASK		BIT(31)

#define ERR_MISC0_COUNT_SHIFT		32
#define ERR_MISC0_COUNT_MASK		GENMASK(39, 32)
#define FAULT_THRESHOLD			0xFF

#define ERR_IRQCR_SPIID_MASK		GENMASK(9, 0)

#define ERR_ADDR_PADDR_MASK		GENMASK(47, 0)
#define ERR_ADDR_NS_MASK		BIT(63)

struct errors_edac {
	const char *const msg;
	void (*func)(struct edac_device_ctl_info *edac_dev,
			int inst_nr, int block_nr, const char *msg);
};
#define GICT_IRQS		2
#define GICT_FAULT_IRQ_IDX	0
#define GICT_ERROR_IRQ_IDX	1
#define GICT_BUFFER_SIZE	32
static u32 gict_buffer[GICT_BUFFER_SIZE] __aligned(PAGE_SIZE);

static void gict_spi_recovery(struct edac_device_ctl_info *edac_dev,
		int inst_nr, int block_nr, const char *msg);

static const struct errors_edac errors[] = {
	{RECORD(0), edac_device_handle_ue },
	{RECORD(1), edac_device_handle_ce },
	{RECORD(2), gict_spi_recovery },
	{RECORD(3), edac_device_handle_ce },
	{RECORD(4), edac_device_handle_ue },
	{RECORD(5), edac_device_handle_ce },
	{RECORD(6), edac_device_handle_ce },
	{RECORD(7), edac_device_handle_ce },
	{RECORD(8), edac_device_handle_ue },
	{RECORD(9), edac_device_handle_ce },
	{RECORD(10), edac_device_handle_ue },
	{RECORD(11), edac_device_handle_ce },
	{RECORD(12), edac_device_handle_ue },
	{RECORD(13), edac_device_handle_ue },
	{RECORD(14), edac_device_handle_ue },
};

struct erp_drvdata {
	struct edac_device_ctl_info *edev_ctl;
	void __iomem *base;
	u32 interrupt_config[GICT_IRQS];
	u32 max_records;
	struct mutex mutex;
};
static struct erp_drvdata *gict_edac;

static inline void gict_write(struct erp_drvdata *drv, u32 val, u32 off)
{
	writel_relaxed(val, drv->base + off);
}

static inline u32 gict_read(struct erp_drvdata *drv, u32 off)
{
	return readl_relaxed(drv->base + off);
}

static inline u64 gict_readq(struct erp_drvdata *drv, u32 off)
{
	return readq_relaxed(drv->base + off);
}

static inline void gict_writeq(struct erp_drvdata *drv, u64 val, u32 off)
{
	writeq_relaxed(val, drv->base + off);
}

#define for_each_bit(i, buf, max) \
	for (i = find_first_bit((unsigned long *)buf, max); \
	     i < max; \
	     i = find_next_bit((unsigned long *)buf, max, i + 1))

#define SCM_SVC_GIC			0x1D
#define GIC_ERROR_RECOVERY_SMC_ID	0x01
#define MAX_IRQS			1023
/* RECORD(2) - Recovery of GIC SPI interrupts */
static void gict_spi_recovery(struct edac_device_ctl_info *edac_dev,
			int inst_nr, int block_nr, const char *msg)
{
	struct scm_desc desc;
	u64 i;
	int ret;

	/*
	 * SPIs that are in the error state can be determined
	 * by reading the GICD_IERRRn register.
	 * Secure side reads GICD_IERRRn register into gict_buffer.
	 */
	memset(&gict_buffer, 0, sizeof(gict_buffer));
	desc.args[0] = virt_to_phys(gict_buffer);
	desc.args[1] = sizeof(gict_buffer);
	desc.arginfo = SCM_ARGS(2, SCM_RW, SCM_VAL);

	ret = scm_call2(SCM_SIP_FNID(SCM_SVC_GIC,
			GIC_ERROR_RECOVERY_SMC_ID), &desc);

	if (ret) {
		pr_warn("Recovery SCM call failed\n");
		return;
	}

	for_each_bit(i, gict_buffer, MAX_IRQS)
		pr_warn("GICT: Corrupted SPI:%d", i);
}

static inline void gict_dump_err_record(int record, u64 errxstatus,
			u64 errxmisc0, u64 errxmisc1, u64 errxaddr)
{
	edac_printk(KERN_INFO, EDAC_NODE, "RECORD: %d", record);
	edac_printk(KERN_INFO, EDAC_NODE, "%s\n", errors[record].msg);
	edac_printk(KERN_INFO, EDAC_NODE, "ERRXSTATUS: %llx\n", errxstatus);
	edac_printk(KERN_INFO, EDAC_NODE, "ERRXMISC0: %llx\n", errxmisc0);
	edac_printk(KERN_INFO, EDAC_NODE, "ERRXMISC1: %llx\n", errxmisc1);
	edac_printk(KERN_INFO, EDAC_NODE, "ERRXADDR: %llx\n", errxaddr);
}

static void handle_record(int record, int level,
		struct edac_device_ctl_info *edev_ctl)
{
	errors[record].func(edev_ctl, raw_smp_processor_id(),
					level, errors[record].msg);
}

static void gict_check_error_records(struct erp_drvdata *drv)
{
	u64 errxstatus, errxaddr, errxmisc0, errxmisc1, errgsr, i;

	errgsr = gict_readq(drv, GICT_ERRGSR);
	/*
	 * Lets first dump all error records
	 * details which are having a valid status.
	 */
	for_each_bit(i, &errgsr, drv->max_records) {
		errxstatus = gict_readq(drv, GICT_ERR_STATUS(i));
		if (errxstatus & ERR_STATUS_V_MASK) {
			errxaddr = gict_readq(drv, GICT_ERR_ADDR(i));
			errxmisc0 = gict_readq(drv, GICT_ERR_MISC0(i));
			errxmisc1 = gict_readq(drv, GICT_ERR_MISC1(i));
			gict_dump_err_record(i, errxstatus,
				errxmisc0, errxmisc1, errxaddr);
		}
	}
}

static void gict_handle_records(struct erp_drvdata *drv)
{
	u64 errxstatus, errgsr, errxstatus_ack, i;

	errgsr = gict_readq(drv, GICT_ERRGSR);

	for_each_bit(i, &errgsr, drv->max_records) {
		errxstatus = gict_readq(drv, GICT_ERR_STATUS(i));
		errxstatus_ack = 0;
		if (errxstatus & ERR_STATUS_V_MASK) {
			handle_record(i, 0, drv->edev_ctl);
			errxstatus_ack = ERR_STATUS_CE_MASK |
				ERR_STATUS_UE_MASK | ERR_STATUS_UET_MASK |
				ERR_STATUS_OF_MASK | ERR_STATUS_MV_MASK |
				ERR_STATUS_AV_MASK | ERR_STATUS_V_MASK;
			gict_write(drv, errxstatus_ack, GICT_ERR_STATUS(i));
		}

	}
}

static void configure_thresholds(struct erp_drvdata *drv)
{
	u64 errxmisc0, i;

	/*
	 * Configure ERRXMISC0 count to 0xFF so that fault
	 * interrupt raised on every error observed.
	 */
	for (i = 0; i <= drv->max_records; i++) {
		errxmisc0 = gict_readq(drv, GICT_ERR_MISC0(i));
		errxmisc0 |= (u64)FAULT_THRESHOLD << ERR_MISC0_COUNT_SHIFT;
		gict_writeq(drv, errxmisc0, GICT_ERR_MISC0(i));
	}

}

static irqreturn_t gict_fault_handler(int irq, void *drvdata)
{
	struct erp_drvdata *drv = drvdata;

	mutex_lock(&drv->mutex);
	gict_check_error_records(drv);
	gict_handle_records(drv);
	configure_thresholds(drv);
	mutex_unlock(&drv->mutex);
	return IRQ_HANDLED;
}

static void configure_ctrl_registers(struct erp_drvdata *drv)
{
	u64 errxfr, errxctrl, i;

	configure_thresholds(drv);

	/* Configure Control Register for CFI, UE, FI and UI faults/errors. */
	for (i = 0; i <= drv->max_records; i++) {
		errxfr = gict_readq(drv, GICT_ERR_FR(i));
		errxctrl = gict_readq(drv, GICT_ERR_CTRL(i));
		/* Configure to enable Correctable Fault Interrupt(CFI) */
		if (errxfr & ERR_FR_CFI_MASK)
			errxctrl = errxctrl | ERR_CTRL_CFI_MASK;
		/* Configure to enable Uncorrectable Error(UE) */
		if (errxfr & ERR_FR_UE_MASK)
			errxctrl = errxctrl | ERR_CTRL_UE_MASK;
		/* Configure to enable Uncorrectable Fault Interrupt(FI) */
		if (errxfr & ERR_FR_FI_MASK)
			errxctrl = errxctrl | ERR_CTRL_FI_MASK;
		/* Configure to enable Uncorrectable Interrupt(UI) */
		if (errxfr & ERR_FR_UI_MASK)
			errxctrl = errxctrl | ERR_CTRL_UI_MASK;

		gict_writeq(drv, errxctrl, GICT_ERR_CTRL(i));
	}
}

static int gic_erp_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct erp_drvdata *drv;
	struct resource *res;
	int ret, errirq, faultirq;
	u64 errirqcrx;
	const char *err_irqname = "gict-err";
	const char *fault_irqname = "gict-fault";

	drv = devm_kzalloc(dev, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "gict-base");
	if (!res)
		return -ENXIO;

	drv->base = devm_ioremap_resource(dev, res);
	if (IS_ERR_OR_NULL(drv->base))
		return -EBUSY;

	ret = of_property_read_u32_array(dev->of_node,
			"interrupt-config", drv->interrupt_config, GICT_IRQS);
	if (ret)
		return -ENXIO;

	errirq = platform_get_irq_byname(pdev, err_irqname);
	if (errirq < 0)
		return errirq;

	faultirq = platform_get_irq_byname(pdev, fault_irqname);
	if (faultirq < 0)
		return faultirq;

	drv->edev_ctl = edac_device_alloc_ctl_info(0, "gic",
					1, "T", 1, 1, NULL, 0,
					edac_device_alloc_index());

	if (!drv->edev_ctl)
		return -ENOMEM;

	drv->edev_ctl->dev = dev;
	drv->edev_ctl->mod_name = dev_name(dev);
	drv->edev_ctl->dev_name = dev_name(dev);
	drv->edev_ctl->ctl_name = "GICT";
	drv->edev_ctl->panic_on_ce = GICT_PANIC_ON_CE;
	drv->edev_ctl->panic_on_ue = GICT_PANIC_ON_UE;
	platform_set_drvdata(pdev, drv);
	gict_edac = drv;

	mutex_init(&drv->mutex);
	ret = edac_device_add_device(drv->edev_ctl);
	if (ret)
		goto out_mem;

	/*
	 * Find no of error records supported by GICT from ERRIDR register
	 * and configure control registers for all records supported.
	 */
	drv->max_records = gict_read(drv, GICT_ERRIDR);
	configure_ctrl_registers(drv);

	/* Configure GICT_ERRIRQCR0 register with fault_interrupt number */
	errirqcrx = gict_readq(drv, GICT_ERRIRQCR0);
	errirqcrx |= (drv->interrupt_config[GICT_FAULT_IRQ_IDX]
				& ERR_IRQCR_SPIID_MASK);
	gict_writeq(drv, errirqcrx, GICT_ERRIRQCR0);

	/* Configure GICT_ERRIRQCR1 register with err_interrupt number */
	errirqcrx = gict_readq(drv, GICT_ERRIRQCR1);
	errirqcrx |= (drv->interrupt_config[GICT_ERROR_IRQ_IDX]
				& ERR_IRQCR_SPIID_MASK);
	gict_writeq(drv, errirqcrx, GICT_ERRIRQCR1);

	ret = devm_request_threaded_irq(&pdev->dev, faultirq,
		NULL, gict_fault_handler,
		IRQF_ONESHOT | IRQF_TRIGGER_HIGH, fault_irqname, drv);
	if (ret) {
		dev_err(dev, "Failed to request %s IRQ %d: %d\n",
					fault_irqname, res->start, ret);
		goto out_dev;
	}

	ret = devm_request_threaded_irq(&pdev->dev, errirq,
		NULL, gict_fault_handler,
		IRQF_ONESHOT | IRQF_TRIGGER_HIGH, err_irqname, drv);
	if (ret) {
		dev_err(dev, "Failed to request %s IRQ %d: %d\n",
					err_irqname, res->start, ret);
		goto out_dev;
	}
	return ret;

out_dev:
	edac_device_del_device(dev);
out_mem:
	edac_device_free_ctl_info(drv->edev_ctl);
	return ret;
}

static int gic_erp_remove(struct platform_device *pdev)
{
	struct erp_drvdata *drv = dev_get_drvdata(&pdev->dev);
	struct edac_device_ctl_info *edac_ctl = drv->edev_ctl;

	edac_device_del_device(edac_ctl->dev);
	edac_device_free_ctl_info(edac_ctl);

	return 0;
}

static const struct of_device_id gic_edac_match[] = {
	{ .compatible = "arm,gic-600-erp", },
	{ }
};
MODULE_DEVICE_TABLE(of, gic_edac_match);

static struct platform_driver gic_edac_driver = {
	.probe = gic_erp_probe,
	.remove = gic_erp_remove,
	.driver = {
		.name = "gic_erp",
		.of_match_table = of_match_ptr(gic_edac_match),
	},
};
module_platform_driver(gic_edac_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("GICT driver");
