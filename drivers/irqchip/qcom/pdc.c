/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/irqchip.h>
#include <linux/irqdomain.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/types.h>
#include "pdc.h"
#define CREATE_TRACE_POINTS
#include "trace/events/pdc.h"

#define MAX_IRQS 126
#define CLEAR_INTR(reg, intr) (reg & ~(1 << intr))
#define ENABLE_INTR(reg, intr) (reg | (1 << intr))

enum pdc_register_offsets {
	IRQ_ENABLE_BANK = 0x10,
	IRQ_i_CFG = 0x110,
};

static DEFINE_SPINLOCK(pdc_lock);
static void __iomem *pdc_base;

static int get_pdc_pin(irq_hw_number_t hwirq, void *data)
{
	int i;
	struct pdc_pin *pdc_data = (struct pdc_pin *) data;

	for (i = 0; pdc_data[i].pin >= 0; i++) {
		if (pdc_data[i].hwirq == hwirq)
			return pdc_data[i].pin;
	}

	return -EINVAL;
}

static inline int pdc_enable_intr(struct irq_data *d, bool on)
{
	int pin_out = get_pdc_pin(d->hwirq, d->chip_data);
	unsigned int index, mask;
	u32 enable, r_enable;
	unsigned long flags;

	if (pin_out < 0)
		return 0;

	index = pin_out / 32;
	mask = pin_out % 32;
	spin_lock_irqsave(&pdc_lock, flags);

	enable = readl_relaxed(pdc_base + IRQ_ENABLE_BANK + (index *
					sizeof(uint32_t)));
	if (on)
		enable = ENABLE_INTR(enable, mask);
	else
		enable = CLEAR_INTR(enable, mask);

	writel_relaxed(enable, pdc_base + IRQ_ENABLE_BANK + (index *
						sizeof(uint32_t)));

	do {
		r_enable = readl_relaxed(pdc_base + IRQ_ENABLE_BANK +
					(index * sizeof(uint32_t)));
		if (r_enable == enable)
			break;
		udelay(5);
	} while (1);

	spin_unlock_irqrestore(&pdc_lock, flags);

	trace_irq_pin_config("enable", (u32)pin_out, (u32)d->hwirq,
			0, on);

	return 0;
}

static void qcom_pdc_gic_mask(struct irq_data *d)
{
	pdc_enable_intr(d, false);
	irq_chip_mask_parent(d);
}

static void qcom_pdc_gic_unmask(struct irq_data *d)
{
	pdc_enable_intr(d, true);
	irq_chip_unmask_parent(d);
}

static void qcom_pdc_gic_enable(struct irq_data *d)
{
	pdc_enable_intr(d, true);
	irq_chip_enable_parent(d);
}

static void qcom_pdc_gic_disable(struct irq_data *d)
{
	pdc_enable_intr(d, false);
	irq_chip_disable_parent(d);
}

/*
 * GIC does not handle falling edge or active low. To allow falling edge and
 * active low interrupts to be handled at GIC, PDC has an inverter that inverts
 * falling edge into a rising edge and active low into an active high.
 * For the inverter to work, the polarity bit in the IRQ_CONFIG register has to
 * set as per the table below.
 * (polarity, falling edge, rising edge )  ORIG          POL CONV     POLARITY
 * 3'b0 00  Level sensitive active low    (~~~|_____)   (___|~~~~~)   LOW
 * 3'b0 01  Rising edge sensitive         (___|~~|__)   (~~~|__|~~)   NOT USED
 * 3'b0 10  Falling edge sensitive        (~~~|__|~~)   (___|~~|__)   LOW
 * 3'b0 11  Dual Edge sensitive                                       NOT USED
 * 3'b1 00  Level senstive active High    (___|~~~~~)   (___|~~~~~)   HIGH
 * 3'b1 01  Falling Edge sensitive        (~~~|__|~~)   (~~~|__|~~)   NOT USED
 * 3'b1 10  Rising edge sensitive         (___|~~|__)   (___|~~|__)   HIGH
 * 3'b1 11  Dual Edge sensitive                                       HIGH
 */
enum pdc_irq_config_bits {
	POLARITY_LOW = 0, //0 00
	FALLING_EDGE = 2, //0 10
	POLARITY_HIGH = 4,//1 00
	RISING_EDGE = 6,  //1 10
	DUAL_EDGE = 7,    //1 11
};

static int qcom_pdc_gic_set_type(struct irq_data *d, unsigned int type)
{
	int pin_out = get_pdc_pin(d->hwirq, d->chip_data);
	u32 pdc_type = 0, config;

	if (pin_out < 0)
		goto fwd_to_parent;

	switch (type) {
	case IRQ_TYPE_EDGE_RISING:
		pdc_type = RISING_EDGE;
		break;
	case IRQ_TYPE_EDGE_FALLING:
		pdc_type = FALLING_EDGE;
		break;
	case IRQ_TYPE_EDGE_BOTH:
		pdc_type = DUAL_EDGE;
		break;
	case IRQ_TYPE_LEVEL_HIGH:
		pdc_type = POLARITY_HIGH;
		break;
	case IRQ_TYPE_LEVEL_LOW:
		pdc_type = POLARITY_LOW;
		break;
	default:
		pdc_type = POLARITY_HIGH;
		break;
	}
	writel_relaxed(pdc_type, pdc_base + IRQ_i_CFG +
			(pin_out * sizeof(uint32_t)));

	do {
		config = readl_relaxed(pdc_base + IRQ_i_CFG +
				(pin_out * sizeof(uint32_t)));
		if (config == pdc_type)
			break;
		udelay(5);
	} while (1);

	trace_irq_pin_config("type_config", (u32)pin_out, (u32)d->hwirq,
			pdc_type, 0);

	/*
	 * If type is edge triggered, forward that as Rising edge as PDC
	 * takes care of converting falling edge to rising edge signal
	 */
	if (type & (IRQ_TYPE_EDGE_RISING | IRQ_TYPE_EDGE_FALLING))
		type = IRQ_TYPE_EDGE_RISING;

	/*
	 * If type is level, then forward that as level high as PDC
	 * takes care of converting falling edge to rising edge signal
	 */
	if (type & (IRQ_TYPE_LEVEL_LOW | IRQ_TYPE_LEVEL_HIGH))
		type = IRQ_TYPE_LEVEL_HIGH;

fwd_to_parent:

	return irq_chip_set_type_parent(d, type);
}

static struct irq_chip qcom_pdc_gic_chip = {
	.name			= "PDC-GIC",
	.irq_eoi		= irq_chip_eoi_parent,
	.irq_mask		= qcom_pdc_gic_mask,
	.irq_enable		= qcom_pdc_gic_enable,
	.irq_unmask		= qcom_pdc_gic_unmask,
	.irq_disable		= qcom_pdc_gic_disable,
	.irq_retrigger		= irq_chip_retrigger_hierarchy,
	.irq_set_type		= qcom_pdc_gic_set_type,
	.flags			= IRQCHIP_MASK_ON_SUSPEND |
					IRQCHIP_SET_TYPE_MASKED |
					IRQCHIP_SKIP_SET_WAKE,
	.irq_set_vcpu_affinity	= irq_chip_set_vcpu_affinity_parent,
#ifdef CONFIG_SMP
	.irq_set_affinity	= irq_chip_set_affinity_parent,
#endif
};

static int qcom_pdc_translate(struct irq_domain *d,
	struct irq_fwspec *fwspec, unsigned long *hwirq, unsigned int *type)
{
	return d->parent->ops->translate(d->parent, fwspec, hwirq, type);
}

static int qcom_pdc_alloc(struct irq_domain *domain,
	unsigned int virq, unsigned int nr_irqs, void *data)
{
	struct irq_fwspec *fwspec = data;
	struct irq_fwspec parent_fwspec;
	irq_hw_number_t hwirq;
	int i;
	unsigned int type;
	int ret;

	ret = qcom_pdc_translate(domain, fwspec, &hwirq, &type);
	if (ret)
		return -EINVAL;

	for (i = 0; i < nr_irqs; i++)
		irq_domain_set_hwirq_and_chip(domain, virq + i, hwirq + i,
			&qcom_pdc_gic_chip, domain->host_data);

	parent_fwspec = *fwspec;
	parent_fwspec.fwnode = domain->parent->fwnode;

	return irq_domain_alloc_irqs_parent(domain, virq, nr_irqs,
					    &parent_fwspec);
}

static const struct irq_domain_ops qcom_pdc_ops = {
	.translate	= qcom_pdc_translate,
	.alloc		= qcom_pdc_alloc,
	.free		= irq_domain_free_irqs_common,
};

int qcom_pdc_init(struct device_node *node,
		struct device_node *parent, void *data)
{
	struct irq_domain *parent_domain;
	int ret;
	struct irq_domain *pdc_domain;

	pdc_base = of_iomap(node, 0);
	if (!pdc_base) {
		pr_err("%s(): unable to map PDC registers\n", node->full_name);
		return -ENXIO;
	}

	parent_domain = irq_find_host(parent);
	if (!parent_domain) {
		pr_err("unable to obtain PDC parent domain\n");
		ret = -ENXIO;
		goto failure;
	}

	pdc_domain = irq_domain_add_hierarchy(parent_domain, 0, MAX_IRQS,
			node, &qcom_pdc_ops, data);
	if (!pdc_domain) {
		pr_err("GIC domain add failed\n");
		ret = -ENOMEM;
		goto failure;
	}

	pdc_domain->name = "qcom,pdc";

	return 0;

failure:
	iounmap(pdc_base);

	return ret;
}
EXPORT_SYMBOL(qcom_pdc_init);
