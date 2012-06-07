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

#include "pcie.h"

/* Any address will do here, as it won't be dereferenced */
#define MSM_PCIE_MSI_PHY 0xa0000000

#define PCIE20_MSI_CTRL_ADDR            (0x820)
#define PCIE20_MSI_CTRL_UPPER_ADDR      (0x824)
#define PCIE20_MSI_CTRL_INTR_EN         (0x828)
#define PCIE20_MSI_CTRL_INTR_MASK       (0x82C)
#define PCIE20_MSI_CTRL_INTR_STATUS     (0x830)

#define PCIE20_MSI_CTRL_MAX 8

static DECLARE_BITMAP(msi_irq_in_use, NR_PCIE_MSI_IRQS);

irqreturn_t handle_msi_irq(int irq, void *data)
{
	int i, j;
	unsigned long val;
	struct msm_pcie_dev_t *dev = data;
	void __iomem *ctrl_status;

	/* check for set bits, clear it by setting that bit
	   and trigger corresponding irq */
	for (i = 0; i < PCIE20_MSI_CTRL_MAX; i++) {
		ctrl_status = dev->pcie20 +
				PCIE20_MSI_CTRL_INTR_STATUS + (i * 12);

		val = readl_relaxed(ctrl_status);
		while (val) {
			j = find_first_bit(&val, 32);
			writel_relaxed(BIT(j), ctrl_status);
			/* ensure that interrupt is cleared (acked) */
			wmb();

			generic_handle_irq(MSM_PCIE_MSI_INT(j + (32 * i)));
			val = readl_relaxed(ctrl_status);
		}
	}

	return IRQ_HANDLED;
}

uint32_t __init msm_pcie_irq_init(struct msm_pcie_dev_t *dev)
{
	int i, rc;

	PCIE_DBG("\n");

	/* program MSI controller and enable all interrupts */
	writel_relaxed(MSM_PCIE_MSI_PHY, dev->pcie20 + PCIE20_MSI_CTRL_ADDR);
	writel_relaxed(0, dev->pcie20 + PCIE20_MSI_CTRL_UPPER_ADDR);

	for (i = 0; i < PCIE20_MSI_CTRL_MAX; i++)
		writel_relaxed(~0, dev->pcie20 +
			       PCIE20_MSI_CTRL_INTR_EN + (i * 12));

	/* ensure that hardware is configured before proceeding */
	wmb();

	/* register handler for physical MSI interrupt line */
	rc = request_irq(PCIE20_INT_MSI, handle_msi_irq, IRQF_TRIGGER_RISING,
			 "msm_pcie_msi", dev);
	if (rc)
		pr_err("Unable to allocate msi interrupt\n");

	return rc;
}

void __exit msm_pcie_irq_deinit(struct msm_pcie_dev_t *dev)
{
	free_irq(PCIE20_INT_MSI, dev);
}

void msm_pcie_destroy_irq(unsigned int irq)
{
	int pos = irq - MSM_PCIE_MSI_INT(0);

	dynamic_irq_cleanup(irq);
	clear_bit(pos, msi_irq_in_use);
}

/* hookup to linux pci msi framework */
void arch_teardown_msi_irq(unsigned int irq)
{
	PCIE_DBG("irq %d deallocated\n", irq);
	msm_pcie_destroy_irq(irq);
}

static void msm_pcie_msi_nop(struct irq_data *d)
{
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

static int msm_pcie_create_irq(void)
{
	int irq, pos;

again:
	pos = find_first_zero_bit(msi_irq_in_use, NR_PCIE_MSI_IRQS);
	irq = MSM_PCIE_MSI_INT(pos);
	if (irq >= (MSM_PCIE_MSI_INT(0) + NR_PCIE_MSI_IRQS))
		return -ENOSPC;

	if (test_and_set_bit(pos, msi_irq_in_use))
		goto again;

	dynamic_irq_init(irq);
	return irq;
}

/* hookup to linux pci msi framework */
int arch_setup_msi_irq(struct pci_dev *pdev, struct msi_desc *desc)
{
	int irq;
	struct msi_msg msg;

	irq = msm_pcie_create_irq();
	if (irq < 0)
		return irq;

	PCIE_DBG("irq %d allocated\n", irq);

	irq_set_msi_desc(irq, desc);

	/* write msi vector and data */
	msg.address_hi = 0;
	msg.address_lo = MSM_PCIE_MSI_PHY;
	msg.data = irq - MSM_PCIE_MSI_INT(0);
	write_msi_msg(irq, &msg);

	irq_set_chip_and_handler(irq, &pcie_msi_chip, handle_simple_irq);
	set_irq_flags(irq, IRQF_VALID);
	return 0;
}
