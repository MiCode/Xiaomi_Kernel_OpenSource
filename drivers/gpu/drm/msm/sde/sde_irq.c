/* Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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
#include <linux/kthread.h>

#include "msm_drv.h"
#include "sde_kms.h"

static void sde_irq_callback_handler(void *arg, int irq_idx)
{
	struct sde_kms *sde_kms = arg;
	struct sde_irq *irq_obj = &sde_kms->irq_obj;

	/*
	 * Perform registered function callback
	 */
	if (irq_obj->irq_cb_tbl && irq_obj->irq_cb_tbl[irq_idx].func)
		irq_obj->irq_cb_tbl[irq_idx].func(
				irq_obj->irq_cb_tbl[irq_idx].arg,
				irq_idx);

	/*
	 * Clear pending interrupt status in HW.
	 * NOTE: sde_irq_callback_handler is protected by top-level
	 *       spinlock, so it is safe to clear any interrupt status here.
	 */
	sde_kms->hw_intr->ops.clear_interrupt_status(
			sde_kms->hw_intr,
			irq_idx);
}

static void sde_irq_intf_error_handler(void *arg, int irq_idx)
{
	DRM_ERROR("INTF underrun detected, irq_idx=%d\n", irq_idx);
}

void sde_set_irqmask(struct sde_kms *sde_kms, uint32_t reg, uint32_t irqmask)
{
	if (!sde_kms || !sde_kms->hw_intr ||
			!sde_kms->hw_intr->ops.set_mask)
		return;

	sde_kms->hw_intr->ops.set_mask(sde_kms->hw_intr, reg, irqmask);
}

int sde_irq_idx_lookup(struct sde_kms *sde_kms, enum sde_intr_type intr_type,
		u32 instance_idx)
{
	if (!sde_kms || !sde_kms->hw_intr ||
			!sde_kms->hw_intr->ops.irq_idx_lookup)
		return -EINVAL;

	return sde_kms->hw_intr->ops.irq_idx_lookup(intr_type,
			instance_idx);
}

int sde_enable_irq(struct sde_kms *sde_kms, int *irq_idxs, u32 irq_count)
{
	int i;
	int ret = 0;

	if (!sde_kms || !irq_idxs || !sde_kms->hw_intr ||
			!sde_kms->hw_intr->ops.enable_irq)
		return -EINVAL;

	for (i = 0; i < irq_count; i++) {
		ret = sde_kms->hw_intr->ops.enable_irq(
				sde_kms->hw_intr,
				irq_idxs[i]);
		if (ret) {
			DRM_ERROR("Fail to enable IRQ for irq_idx:%d\n",
					irq_idxs[i]);
			return ret;
		}
	}

	return ret;
}

int sde_disable_irq(struct sde_kms *sde_kms, int *irq_idxs, u32 irq_count)
{
	int i;
	int ret = 0;

	if (!sde_kms || !irq_idxs || !sde_kms->hw_intr ||
			!sde_kms->hw_intr->ops.disable_irq)
		return -EINVAL;

	for (i = 0; i < irq_count; i++) {
		ret = sde_kms->hw_intr->ops.disable_irq(
				sde_kms->hw_intr,
				irq_idxs[i]);
		if (ret) {
			DRM_ERROR("Fail to disable IRQ for irq_idx:%d\n",
					irq_idxs[i]);
			return ret;
		}
	}

	return ret;
}

int sde_register_irq_callback(struct sde_kms *sde_kms, int irq_idx,
		struct sde_irq_callback *register_irq_cb)
{
	struct sde_irq_callback *irq_cb_tbl;
	unsigned long irq_flags;

	/*
	 * We allow NULL register_irq_cb as input for callback registration
	 */
	if (!sde_kms || !sde_kms->irq_obj.irq_cb_tbl)
		return -EINVAL;

	if (irq_idx < 0 || irq_idx >= sde_kms->hw_intr->irq_idx_tbl_size) {
		DRM_ERROR("invalid IRQ index: [%d]\n", irq_idx);
		return -EINVAL;
	}

	irq_cb_tbl = sde_kms->irq_obj.irq_cb_tbl;
	spin_lock_irqsave(&sde_kms->irq_obj.cb_lock, irq_flags);
	irq_cb_tbl[irq_idx].func = register_irq_cb ?
		register_irq_cb->func : NULL;
	irq_cb_tbl[irq_idx].arg  = register_irq_cb ?
		register_irq_cb->arg : NULL;
	spin_unlock_irqrestore(&sde_kms->irq_obj.cb_lock, irq_flags);

	return 0;
}

void sde_clear_all_irqs(struct sde_kms *sde_kms)
{
	if (!sde_kms || !sde_kms->hw_intr ||
			!sde_kms->hw_intr->ops.clear_all_irqs)
		return;

	sde_kms->hw_intr->ops.clear_all_irqs(sde_kms->hw_intr);
}

void sde_disable_all_irqs(struct sde_kms *sde_kms)
{
	if (!sde_kms || !sde_kms->hw_intr ||
			!sde_kms->hw_intr->ops.disable_all_irqs)
		return;

	sde_kms->hw_intr->ops.disable_all_irqs(sde_kms->hw_intr);
}

void sde_irq_preinstall(struct msm_kms *kms)
{
	struct sde_kms *sde_kms = to_sde_kms(kms);

	sde_enable(sde_kms);
	sde_clear_all_irqs(sde_kms);
	sde_disable_all_irqs(sde_kms);
	sde_disable(sde_kms);

	spin_lock_init(&sde_kms->irq_obj.cb_lock);

	/* Create irq callbacks for all possible irq_idx */
	sde_kms->irq_obj.total_irqs = sde_kms->hw_intr->irq_idx_tbl_size;
	sde_kms->irq_obj.irq_cb_tbl = kcalloc(sde_kms->irq_obj.total_irqs,
			sizeof(struct sde_irq_callback), GFP_KERNEL);
	if (!sde_kms->irq_obj.irq_cb_tbl)
		DRM_ERROR("Fail to allocate memory of IRQ callback list\n");
}

int sde_irq_postinstall(struct msm_kms *kms)
{
	struct sde_kms *sde_kms = to_sde_kms(kms);
	struct sde_irq_callback irq_cb;
	int irq_idx;
	int i;

	irq_cb.func = sde_irq_intf_error_handler;
	irq_cb.arg  = sde_kms;

	/* Register interface underrun callback */
	sde_enable(sde_kms);
	for (i = 0; i < sde_kms->catalog->intf_count; i++) {
		irq_idx = sde_irq_idx_lookup(sde_kms,
				SDE_IRQ_TYPE_INTF_UNDER_RUN, i+INTF_0);
		sde_register_irq_callback(sde_kms, irq_idx, &irq_cb);
		sde_enable_irq(sde_kms, &irq_idx, 1);
	}
	sde_disable(sde_kms);

	return 0;
}

void sde_irq_uninstall(struct msm_kms *kms)
{
	struct sde_kms *sde_kms = to_sde_kms(kms);

	sde_enable(sde_kms);
	sde_clear_all_irqs(sde_kms);
	sde_disable_all_irqs(sde_kms);
	sde_disable(sde_kms);

	kfree(sde_kms->irq_obj.irq_cb_tbl);
}

static void _sde_irq_mdp_done(struct sde_kms *sde_kms)
{
	/*
	 * Read interrupt status from all sources. Interrupt status are
	 * stored within hw_intr.
	 * Function will also clear the interrupt status after reading.
	 * Individual interrupt status bit will only get stored if it
	 * is enabled.
	 */
	sde_kms->hw_intr->ops.get_interrupt_statuses(sde_kms->hw_intr);

	/*
	 * Dispatch to HW driver to handle interrupt lookup that is being
	 * fired. When matching interrupt is located, HW driver will call to
	 * sde_irq_callback_handler with the irq_idx from the lookup table.
	 * sde_irq_callback_handler will perform the registered function
	 * callback, and do the interrupt status clearing once the registered
	 * callback is finished.
	 */
	sde_kms->hw_intr->ops.dispatch_irqs(
			sde_kms->hw_intr,
			sde_irq_callback_handler,
			sde_kms);
}

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
		_sde_irq_mdp_done(sde_kms);
	}

	/*
	 * Routing all other interrupts to external drivers
	 */
	while (interrupts) {
		irq_hw_number_t hwirq = fls(interrupts) - 1;

		generic_handle_irq(irq_find_mapping(
				sde_kms->irqcontroller.domain, hwirq));
		interrupts &= ~(1 << hwirq);
	}

	return IRQ_HANDLED;
}

int sde_enable_vblank(struct msm_kms *kms, struct drm_crtc *crtc)
{
	return sde_crtc_vblank(crtc);
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
	uint32_t valid_irqs;

	sde_kms->hw_intr->ops.get_valid_interrupts(sde_kms->hw_intr,
			&valid_irqs);

	if (!(valid_irqs & (1 << hwirq)))
		return -EPERM;

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

