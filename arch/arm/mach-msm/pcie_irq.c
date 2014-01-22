/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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

static void handle_wake_func(struct work_struct *work)
{
	int ret;
	struct msm_pcie_dev_t *dev = container_of(work, struct msm_pcie_dev_t,
					handle_wake_work);

	PCIE_DBG("Wake work for RC %d\n", dev->rc_idx);

	if (!dev->enumerated) {
		ret = msm_pcie_enumerate(dev->rc_idx);
		if (ret) {
			pr_err(
				"PCIe: failed to enable RC %d upon wake request from the device.\n",
				dev->rc_idx);
			return;
		}
	} else {
		pr_err("PCIe: %s: RC %d has already been enumerated.\n",
			__func__, dev->rc_idx);
	}
}

static irqreturn_t handle_wake_irq(int irq, void *data)
{
	struct msm_pcie_dev_t *dev = data;

	PCIE_DBG("PCIe WAKE is asserted by Endpoint of RC %d\n", dev->rc_idx);

	if (!dev->enumerated) {
		PCIE_DBG("Start enumeating RC %d\n", dev->rc_idx);
		schedule_work(&dev->handle_wake_work);
	} else {
		__pm_stay_awake(&dev->ws);
		__pm_relax(&dev->ws);
	}

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

void msm_pcie_destroy_irq(unsigned int irq, struct msm_pcie_dev_t *pcie_dev)
{
	int pos;
	struct msm_pcie_dev_t *dev;

	if (pcie_dev)
		dev = pcie_dev;
	else
		dev = irq_get_chip_data(irq);

	if (dev->msi_gicm_addr) {
		PCIE_DBG("destroy QGIC based irq\n");
		pos = irq - dev->msi_gicm_base;
	} else {
		PCIE_DBG("destroy default MSI irq\n");
		pos = irq - irq_find_mapping(dev->irq_domain, 0);
	}

	PCIE_DBG("\n");

	if (!dev->msi_gicm_addr)
		dynamic_irq_cleanup(irq);

	PCIE_DBG("Before clear_bit pos:%d msi_irq_in_use:%ld\n",
		pos, *dev->msi_irq_in_use);
	clear_bit(pos, dev->msi_irq_in_use);
	PCIE_DBG("After clear_bit pos:%d msi_irq_in_use:%ld\n",
		pos, *dev->msi_irq_in_use);
}

/* hookup to linux pci msi framework */
void arch_teardown_msi_irq(unsigned int irq)
{
	PCIE_DBG("irq %d deallocated\n", irq);
	msm_pcie_destroy_irq(irq, NULL);
}

void arch_teardown_msi_irqs(struct pci_dev *dev)
{
	struct msi_desc *entry;
	struct msm_pcie_dev_t *pcie_dev = PCIE_BUS_PRIV_DATA(dev);

	PCIE_DBG("RC:%d EP: vendor_id:0x%x device_id:0x%x\n",
		pcie_dev->rc_idx, dev->vendor, dev->device);

	list_for_each_entry(entry, &dev->msi_list, list) {
		int i, nvec;
		if (entry->irq == 0)
			continue;
		nvec = 1 << entry->msi_attrib.multiple;
		for (i = 0; i < nvec; i++)
			msm_pcie_destroy_irq(entry->irq + i, pcie_dev);
	}
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

	PCIE_DBG("pos:%d msi_irq_in_use:%ld\n", pos, *dev->msi_irq_in_use);

	if (test_and_set_bit(pos, dev->msi_irq_in_use))
		goto again;
	else
		PCIE_DBG("test_and_set_bit is successful\n");

	irq = irq_create_mapping(dev->irq_domain, pos);
	if (!irq)
		return -EINVAL;

	return irq;
}

static int arch_setup_msi_irq_default(struct pci_dev *pdev,
		struct msi_desc *desc, int nvec)
{
	int irq;
	struct msi_msg msg;
	struct msm_pcie_dev_t *dev = PCIE_BUS_PRIV_DATA(pdev);

	PCIE_DBG("\n");

	irq = msm_pcie_create_irq(dev);

	PCIE_DBG("IRQ %d is allocated.\n", irq);

	if (irq < 0)
		return irq;

	PCIE_DBG("irq %d allocated\n", irq);

	irq_set_msi_desc(irq, desc);

	/* write msi vector and data */
	msg.address_hi = 0;
	msg.address_lo = MSM_PCIE_MSI_PHY;
	msg.data = irq - irq_find_mapping(dev->irq_domain, 0);
	write_msi_msg(irq, &msg);

	return 0;
}

static int msm_pcie_create_irq_qgic(struct msm_pcie_dev_t *dev)
{
	int irq, pos;

	PCIE_DBG("\n");

again:
	pos = find_first_zero_bit(dev->msi_irq_in_use, PCIE_MSI_NR_IRQS);

	if (pos >= PCIE_MSI_NR_IRQS)
		return -ENOSPC;

	PCIE_DBG("pos:%d msi_irq_in_use:%ld\n", pos, *dev->msi_irq_in_use);

	if (test_and_set_bit(pos, dev->msi_irq_in_use))
		goto again;
	else
		PCIE_DBG("test_and_set_bit is successful\n");

	irq = dev->msi_gicm_base + pos;
	if (!irq) {
		pr_err("PCIe: failed to create QGIC MSI IRQ.\n");
		return -EINVAL;
	}

	return irq;
}

static int arch_setup_msi_irq_qgic(struct pci_dev *pdev,
		struct msi_desc *desc, int nvec)
{
	int irq, index, firstirq = 0;
	struct msi_msg msg;
	struct msm_pcie_dev_t *dev = PCIE_BUS_PRIV_DATA(pdev);

	PCIE_DBG("\n");

	for (index = 0; index < nvec; index++) {
		irq = msm_pcie_create_irq_qgic(dev);
		PCIE_DBG("irq %d is allocated\n", irq);

		if (irq < 0)
			return irq;

		if (index == 0)
			firstirq = irq;

		irq_set_irq_type(irq, IRQ_TYPE_EDGE_RISING);
	}

	/* write msi vector and data */
	irq_set_msi_desc(firstirq, desc);
	msg.address_hi = 0;
	msg.address_lo = dev->msi_gicm_addr;
	msg.data = firstirq;
	write_msi_msg(firstirq, &msg);

	return 0;
}

int arch_setup_msi_irq(struct pci_dev *pdev, struct msi_desc *desc)
{
	struct msm_pcie_dev_t *dev = PCIE_BUS_PRIV_DATA(pdev);

	PCIE_DBG("\n");

	if (dev->msi_gicm_addr)
		return arch_setup_msi_irq_qgic(pdev, desc, 1);
	else
		return arch_setup_msi_irq_default(pdev, desc, 1);
}

static int msm_pcie_get_msi_multiple(int nvec)
{
	int msi_multiple = 0;

	PCIE_DBG("\n");

	while (nvec) {
		nvec = nvec >> 1;
		msi_multiple++;
	}
	PCIE_DBG("log2 number of MSI multiple:%d\n",
		msi_multiple - 1);

	return msi_multiple - 1;
}

int arch_setup_msi_irqs(struct pci_dev *dev, int nvec, int type)
{
	struct msi_desc *entry;
	int ret;
	struct msm_pcie_dev_t *pcie_dev = PCIE_BUS_PRIV_DATA(dev);

	PCIE_DBG("\n");

	if (type != PCI_CAP_ID_MSI || nvec > 32)
		return -ENOSPC;

	PCIE_DBG("nvec = %d\n", nvec);

	list_for_each_entry(entry, &dev->msi_list, list) {
		entry->msi_attrib.multiple =
				msm_pcie_get_msi_multiple(nvec);

		if (pcie_dev->msi_gicm_addr)
			ret = arch_setup_msi_irq_qgic(dev, entry, nvec);
		else
			ret = arch_setup_msi_irq_default(dev, entry, nvec);

		PCIE_DBG("ret from msi_irq: %d\n", ret);

		if (ret < 0)
			return ret;
		if (ret > 0)
			return -ENOSPC;
	}

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

	wakeup_source_init(&dev->ws, "pcie_wakeup_source");

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

	INIT_WORK(&dev->handle_wake_work, handle_wake_func);

	rc = enable_irq_wake(dev->wake_n);
	if (rc) {
		pr_err("PCIe: Unable to enable wake interrupt\n");
		return rc;
	}

	/* Create a virtual domain of interrupts */
	if (!dev->msi_gicm_addr) {
		dev->irq_domain = irq_domain_add_linear(dev->pdev->dev.of_node,
			PCIE_MSI_NR_IRQS, &msm_pcie_msi_ops, dev);

		if (!dev->irq_domain) {
			pr_err("PCIe: Unable to initialize irq domain\n");
			disable_irq(dev->wake_n);
			return PTR_ERR(dev->irq_domain);
		}

		msi_start = irq_create_mapping(dev->irq_domain, 0);
	}

	return 0;
}

void msm_pcie_irq_deinit(struct msm_pcie_dev_t *dev)
{
	PCIE_DBG("\n");

	wakeup_source_trash(&dev->ws);
	disable_irq(dev->wake_n);
}
