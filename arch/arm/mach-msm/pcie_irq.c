/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

/*
 * MSM PCIe controller IRQ driver.
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/bitops.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/msi.h>
#include <linux/pci.h>
#include <mach/irqs.h>
#include <linux/irqdomain.h>
#include "pcie.h"

/* Any address will do here, as it won't be dereferenced */
#define MSM_PCIE_MSI_PHY 0xa0000000

#define PCIE20_MSI_CTRL_ADDR            (0x820)
#define PCIE20_MSI_CTRL_UPPER_ADDR      (0x824)
#define PCIE20_MSI_CTRL_INTR_EN         (0x828)
#define PCIE20_MSI_CTRL_INTR_MASK       (0x82C)
#define PCIE20_MSI_CTRL_INTR_STATUS     (0x830)

#define PCIE20_MSI_CTRL_MAX 8

static irqreturn_t handle_wake_irq(int irq, void *data)
{
	PCIE_DBG("\n");
	return IRQ_HANDLED;
}

static irqreturn_t handle_msi_irq(int irq, void *data)
{
	int i, j;
	unsigned long val;
	struct msm_pcie_dev_t *dev = data;
	void __iomem *ctrl_status;

	PCIE_DBG("\n");

	/* check for set bits, clear it by setting that bit
	   and trigger corresponding irq */
	for (i = 0; i < PCIE20_MSI_CTRL_MAX; i++) {
		ctrl_status = dev->dm_core +
				PCIE20_MSI_CTRL_INTR_STATUS + (i * 12);

		val = readl_relaxed(ctrl_status);
		while (val) {
			j = find_first_bit(&val, 32);
			writel_relaxed(BIT(j), ctrl_status);
			/* ensure that interrupt is cleared (acked) */
			wmb();
			generic_handle_irq(
			   irq_find_mapping(dev->irq_domain, (j + (32*i)))
			   );
			val = readl_relaxed(ctrl_status);
		}
	}

	return IRQ_HANDLED;
}

void msm_pcie_config_msi_controller(struct msm_pcie_dev_t *dev)
{
	int i;

	PCIE_DBG("\n");

	/* program MSI controller and enable all interrupts */
	writel_relaxed(MSM_PCIE_MSI_PHY, dev->dm_core + PCIE20_MSI_CTRL_ADDR);
	writel_relaxed(0, dev->dm_core + PCIE20_MSI_CTRL_UPPER_ADDR);

	for (i = 0; i < PCIE20_MSI_CTRL_MAX; i++)
		writel_relaxed(~0, dev->dm_core +
			       PCIE20_MSI_CTRL_INTR_EN + (i * 12));

	/* ensure that hardware is configured before proceeding */
	wmb();
}

void msm_pcie_destroy_irq(unsigned int irq)
{
	int pos;
	struct msm_pcie_dev_t *dev = irq_get_chip_data(irq);

	pos = irq - irq_find_mapping(dev->irq_domain, 0);

	PCIE_DBG("\n");

	dynamic_irq_cleanup(irq);
	clear_bit(pos, dev->msi_irq_in_use);
}

/* hookup to linux pci msi framework */
void arch_teardown_msi_irq(unsigned int irq)
{
	PCIE_DBG("irq %d deallocated\n", irq);
	msm_pcie_destroy_irq(irq);
}

static void msm_pcie_msi_nop(struct irq_data *d)
{
	PCIE_DBG("\n");
	return;
}

static struct irq_chip pcie_msi_chip = {
	.name = "msm-pcie-msi",
	.irq_ack = msm_pcie_msi_nop,
	.irq_enable = unmask_msi_irq,
	.irq_disable = mask_msi_irq,
	.irq_mask = mask_msi_irq,
	.irq_unmask = unmask_msi_irq,
};

static int msm_pcie_create_irq(struct msm_pcie_dev_t *dev)
{
	int irq, pos;

	PCIE_DBG("\n");

again:
	pos = find_first_zero_bit(dev->msi_irq_in_use, PCIE_MSI_NR_IRQS);

	if (pos >= PCIE_MSI_NR_IRQS)
		return -ENOSPC;

	if (test_and_set_bit(pos, dev->msi_irq_in_use))
		goto again;

	irq = irq_create_mapping(dev->irq_domain, pos);
	if (!irq)
		return -EINVAL;


	return irq;
}

/* hookup to linux pci msi framework */
int arch_setup_msi_irq(struct pci_dev *pdev, struct msi_desc *desc)
{
	int irq;
	struct msi_msg msg;
	struct msm_pcie_dev_t *dev = PCIE_BUS_PRIV_DATA(pdev);

	PCIE_DBG("\n");

	irq = msm_pcie_create_irq(dev);
	if (irq < 0)
		return irq;

	PCIE_DBG("irq %d allocated\n", irq);

	irq_set_msi_desc(irq, desc);

	/* write msi vector and data */
	msg.address_hi = 0;
	msg.address_lo = MSM_PCIE_MSI_PHY;
	msg.data = irq - irq_find_mapping(dev->irq_domain, 0);
	write_msi_msg(irq, &msg);

	irq_set_chip_and_handler(irq, &pcie_msi_chip, handle_simple_irq);
	set_irq_flags(irq, IRQF_VALID);
	return 0;
}


static int msm_pcie_msi_map(struct irq_domain *domain, unsigned int irq,
	   irq_hw_number_t hwirq)
{
	irq_set_chip_and_handler (irq, &pcie_msi_chip, handle_simple_irq);
	irq_set_chip_data(irq, domain->host_data);
	set_irq_flags(irq, IRQF_VALID);
	return 0;
}

static const struct irq_domain_ops msm_pcie_msi_ops = {
	.map = msm_pcie_msi_map,
};

int32_t msm_pcie_irq_init(struct msm_pcie_dev_t *dev)
{
	int rc;
	int msi_start =  0;
	struct device *pdev = &dev->pdev->dev;

	PCIE_DBG("\n");
	/* register handler for physical MSI interrupt line */
	rc = devm_request_irq(pdev,
		dev->irq[MSM_PCIE_INT_MSI].num, handle_msi_irq,
		IRQF_TRIGGER_RISING, dev->irq[MSM_PCIE_INT_MSI].name, dev);
	if (rc) {
		pr_err("PCIe: Unable to request MSI interrupt\n");
		return rc;
	}

	/* register handler for PCIE_WAKE_N interrupt line */
	rc = devm_request_irq(pdev,
			dev->wake_n, handle_wake_irq, IRQF_TRIGGER_FALLING,
			 "msm_pcie_wake", dev);
	if (rc) {
		pr_err("PCIe: Unable to request wake interrupt\n");
		return rc;
	}

	rc = enable_irq_wake(dev->wake_n);
	if (rc) {
		pr_err("PCIe: Unable to enable wake interrupt\n");
		return rc;
	}

	/* PCIE_WAKE_N should be enabled only during system suspend */
	disable_irq(dev->wake_n);

	/* Create a virtual domain of interrupts */
	dev->irq_domain = irq_domain_add_linear(dev->pdev->dev.of_node,
			PCIE_MSI_NR_IRQS,
			&msm_pcie_msi_ops,
			&dev);
	if (!dev->irq_domain) {
		pr_err("PCIe: Unable to initialize irq domain\n");
		return PTR_ERR(dev->irq_domain);
	}

	msi_start = irq_create_mapping(dev->irq_domain, 0);

	return 0;
}

void msm_pcie_irq_deinit(struct msm_pcie_dev_t *dev)
{
	PCIE_DBG("\n");
}
