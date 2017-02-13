/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"[drm:%s:%d] " fmt, __func__, __LINE__

#include <linux/irqdomain.h>
#include <linux/irq.h>
#include <linux/kthread.h>

#include "sde_irq.h"
#include "sde_core_irq.h"

irqreturn_t sde_irq(struct msm_kms *kms)
{
	struct sde_kms *sde_kms = to_sde_kms(kms);
	u32 interrupts;

	sde_kms->hw_intr->ops.get_interrupt_sources(sde_kms->hw_intr,
			&interrupts);

	/*
	 * Taking care of MDP interrupt
	 */
	if (interrupts & IRQ_SOURCE_MDP) {
		interrupts &= ~IRQ_SOURCE_MDP;
		sde_core_irq(sde_kms);
	}

	/*
	 * Routing all other interrupts to external drivers
	 */
	while (interrupts) {
		irq_hw_number_t hwirq = fls(interrupts) - 1;

		generic_handle_irq(irq_find_mapping(
				sde_kms->irq_controller.domain, hwirq));
		interrupts &= ~(1 << hwirq);
	}

	return IRQ_HANDLED;
}

static void sde_hw_irq_mask(struct irq_data *irqd)
{
	struct sde_kms *sde_kms;

	if (!irqd || !irq_data_get_irq_chip_data(irqd)) {
		SDE_ERROR("invalid parameters irqd %d\n", irqd != 0);
		return;
	}
	sde_kms = irq_data_get_irq_chip_data(irqd);

	smp_mb__before_atomic();
	clear_bit(irqd->hwirq, &sde_kms->irq_controller.enabled_mask);
	smp_mb__after_atomic();
}

static void sde_hw_irq_unmask(struct irq_data *irqd)
{
	struct sde_kms *sde_kms;

	if (!irqd || !irq_data_get_irq_chip_data(irqd)) {
		SDE_ERROR("invalid parameters irqd %d\n", irqd != 0);
		return;
	}
	sde_kms = irq_data_get_irq_chip_data(irqd);

	smp_mb__before_atomic();
	set_bit(irqd->hwirq, &sde_kms->irq_controller.enabled_mask);
	smp_mb__after_atomic();
}

static struct irq_chip sde_hw_irq_chip = {
	.name = "sde",
	.irq_mask = sde_hw_irq_mask,
	.irq_unmask = sde_hw_irq_unmask,
};

static int sde_hw_irqdomain_map(struct irq_domain *domain,
		unsigned int irq, irq_hw_number_t hwirq)
{
	struct sde_kms *sde_kms;
	int rc;

	if (!domain || !domain->host_data) {
		SDE_ERROR("invalid parameters domain %d\n", domain != 0);
		return -EINVAL;
	}
	sde_kms = domain->host_data;

	irq_set_chip_and_handler(irq, &sde_hw_irq_chip, handle_level_irq);
	rc = irq_set_chip_data(irq, sde_kms);

	return rc;
}

static struct irq_domain_ops sde_hw_irqdomain_ops = {
	.map = sde_hw_irqdomain_map,
	.xlate = irq_domain_xlate_onecell,
};

void sde_irq_preinstall(struct msm_kms *kms)
{
	struct sde_kms *sde_kms = to_sde_kms(kms);
	struct device *dev;
	struct irq_domain *domain;

	if (!sde_kms->dev || !sde_kms->dev->dev) {
		pr_err("invalid device handles\n");
		return;
	}
	dev = sde_kms->dev->dev;

	domain = irq_domain_add_linear(dev->of_node, 32,
			&sde_hw_irqdomain_ops, sde_kms);
	if (!domain) {
		pr_err("failed to add irq_domain\n");
		return;
	}

	sde_kms->irq_controller.enabled_mask = 0;
	sde_kms->irq_controller.domain = domain;

	sde_core_irq_preinstall(sde_kms);
}

int sde_irq_postinstall(struct msm_kms *kms)
{
	struct sde_kms *sde_kms = to_sde_kms(kms);
	int rc;

	if (!kms) {
		SDE_ERROR("invalid parameters\n");
		return -EINVAL;
	}

	rc = sde_core_irq_postinstall(sde_kms);

	return rc;
}

void sde_irq_uninstall(struct msm_kms *kms)
{
	struct sde_kms *sde_kms = to_sde_kms(kms);

	if (!kms) {
		SDE_ERROR("invalid parameters\n");
		return;
	}

	sde_core_irq_uninstall(sde_kms);

	if (sde_kms->irq_controller.domain) {
		irq_domain_remove(sde_kms->irq_controller.domain);
		sde_kms->irq_controller.domain = NULL;
	}
}
