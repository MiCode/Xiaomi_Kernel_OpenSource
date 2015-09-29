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

#include <linux/irqdomain.h>
#include <linux/irq.h>

#include "msm_drv.h"
#include "sde_kms.h"

void sde_set_irqmask(struct mdp_kms *mdp_kms, uint32_t irqmask,
	uint32_t old_irqmask)
{
}

void sde_irq_preinstall(struct msm_kms *kms)
{
}

int sde_irq_postinstall(struct msm_kms *kms)
{
	return 0;
}

void sde_irq_uninstall(struct msm_kms *kms)
{
}

irqreturn_t sde_irq(struct msm_kms *kms)
{
	return IRQ_HANDLED;
}

int sde_enable_vblank(struct msm_kms *kms, struct drm_crtc *crtc)
{
	return 0;
}

void sde_disable_vblank(struct msm_kms *kms, struct drm_crtc *crtc)
{
}

static void sde_hw_irq_mask(struct irq_data *irqd)
{
	struct sde_kms *sde_kms = irq_data_get_irq_chip_data(irqd);

	smp_mb__before_atomic();
	clear_bit(irqd->hwirq, &sde_kms->irqcontroller.enabled_mask);
	smp_mb__after_atomic();
}

static void sde_hw_irq_unmask(struct irq_data *irqd)
{
	struct sde_kms *sde_kms = irq_data_get_irq_chip_data(irqd);

	smp_mb__before_atomic();
	set_bit(irqd->hwirq, &sde_kms->irqcontroller.enabled_mask);
	smp_mb__after_atomic();
}

static struct irq_chip sde_hw_irq_chip = {
	.name = "sde",
	.irq_mask = sde_hw_irq_mask,
	.irq_unmask = sde_hw_irq_unmask,
};

static int sde_hw_irqdomain_map(struct irq_domain *d,
		unsigned int irq, irq_hw_number_t hwirq)
{
	struct sde_kms *sde_kms = d->host_data;

	irq_set_chip_and_handler(irq, &sde_hw_irq_chip, handle_level_irq);
	irq_set_chip_data(irq, sde_kms);

	return 0;
}

static struct irq_domain_ops sde_hw_irqdomain_ops = {
	.map = sde_hw_irqdomain_map,
	.xlate = irq_domain_xlate_onecell,
};

int sde_irq_domain_init(struct sde_kms *sde_kms)
{
	struct device *dev = sde_kms->dev->dev;
	struct irq_domain *d;

	d = irq_domain_add_linear(dev->of_node, 32,
			&sde_hw_irqdomain_ops, sde_kms);

	if (!d)
		return -ENXIO;

	sde_kms->irqcontroller.enabled_mask = 0;
	sde_kms->irqcontroller.domain = d;

	return 0;
}

int sde_irq_domain_fini(struct sde_kms *sde_kms)
{
	if (sde_kms->irqcontroller.domain) {
		irq_domain_remove(sde_kms->irqcontroller.domain);
		sde_kms->irqcontroller.domain = NULL;
	}
	return 0;
}

