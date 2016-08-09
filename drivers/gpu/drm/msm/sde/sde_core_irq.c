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

#include "sde_core_irq.h"
#include "sde_power_handle.h"

static void sde_core_irq_callback_handler(void *arg, int irq_idx)
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
	 * NOTE: sde_core_irq_callback_handler is protected by top-level
	 *       spinlock, so it is safe to clear any interrupt status here.
	 */
	sde_kms->hw_intr->ops.clear_interrupt_status(
			sde_kms->hw_intr,
			irq_idx);
}

static void sde_core_irq_intf_error_handler(void *arg, int irq_idx)
{
	SDE_ERROR("INTF underrun detected, irq_idx=%d\n", irq_idx);
}

int sde_core_irq_idx_lookup(struct sde_kms *sde_kms,
		enum sde_intr_type intr_type, u32 instance_idx)
{
	if (!sde_kms || !sde_kms->hw_intr ||
			!sde_kms->hw_intr->ops.irq_idx_lookup)
		return -EINVAL;

	return sde_kms->hw_intr->ops.irq_idx_lookup(intr_type,
			instance_idx);
}

int sde_core_irq_enable(struct sde_kms *sde_kms, int *irq_idxs, u32 irq_count)
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
			SDE_ERROR("Fail to enable IRQ for irq_idx:%d\n",
					irq_idxs[i]);
			return ret;
		}
	}

	return ret;
}

int sde_core_irq_disable(struct sde_kms *sde_kms, int *irq_idxs, u32 irq_count)
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
			SDE_ERROR("Fail to disable IRQ for irq_idx:%d\n",
					irq_idxs[i]);
			return ret;
		}
	}

	return ret;
}

u32 sde_core_irq_read(struct sde_kms *sde_kms, int irq_idx, bool clear)
{
	if (!sde_kms || !sde_kms->hw_intr ||
			!sde_kms->hw_intr->ops.get_interrupt_status)
		return 0;

	return sde_kms->hw_intr->ops.get_interrupt_status(sde_kms->hw_intr,
			irq_idx, clear);
}

int sde_core_irq_register_callback(struct sde_kms *sde_kms, int irq_idx,
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
		SDE_ERROR("invalid IRQ index: [%d]\n", irq_idx);
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

static void sde_clear_all_irqs(struct sde_kms *sde_kms)
{
	if (!sde_kms || !sde_kms->hw_intr ||
			!sde_kms->hw_intr->ops.clear_all_irqs)
		return;

	sde_kms->hw_intr->ops.clear_all_irqs(sde_kms->hw_intr);
}

static void sde_disable_all_irqs(struct sde_kms *sde_kms)
{
	if (!sde_kms || !sde_kms->hw_intr ||
			!sde_kms->hw_intr->ops.disable_all_irqs)
		return;

	sde_kms->hw_intr->ops.disable_all_irqs(sde_kms->hw_intr);
}

void sde_core_irq_preinstall(struct sde_kms *sde_kms)
{
	struct msm_drm_private *priv;

	if (!sde_kms) {
		SDE_ERROR("invalid sde_kms\n");
		return;
	} else if (!sde_kms->dev) {
		SDE_ERROR("invalid drm device\n");
		return;
	} else if (!sde_kms->dev->dev_private) {
		SDE_ERROR("invalid device private\n");
		return;
	}
	priv = sde_kms->dev->dev_private;

	sde_power_resource_enable(&priv->phandle, sde_kms->core_client, true);
	sde_clear_all_irqs(sde_kms);
	sde_disable_all_irqs(sde_kms);
	sde_power_resource_enable(&priv->phandle, sde_kms->core_client, false);

	spin_lock_init(&sde_kms->irq_obj.cb_lock);

	/* Create irq callbacks for all possible irq_idx */
	sde_kms->irq_obj.total_irqs = sde_kms->hw_intr->irq_idx_tbl_size;
	sde_kms->irq_obj.irq_cb_tbl = kcalloc(sde_kms->irq_obj.total_irqs,
			sizeof(struct sde_irq_callback), GFP_KERNEL);
}

int sde_core_irq_postinstall(struct sde_kms *sde_kms)
{
	struct msm_drm_private *priv;
	struct sde_irq_callback irq_cb;
	int irq_idx;
	int i;

	if (!sde_kms) {
		SDE_ERROR("invalid sde_kms\n");
		return -EINVAL;
	} else if (!sde_kms->dev) {
		SDE_ERROR("invalid drm device\n");
		return -EINVAL;
	} else if (!sde_kms->dev->dev_private) {
		SDE_ERROR("invalid device private\n");
		return -EINVAL;
	}
	priv = sde_kms->dev->dev_private;

	irq_cb.func = sde_core_irq_intf_error_handler;
	irq_cb.arg  = sde_kms;

	/* Register interface underrun callback */
	sde_power_resource_enable(&priv->phandle, sde_kms->core_client, true);
	for (i = 0; i < sde_kms->catalog->intf_count; i++) {
		irq_idx = sde_core_irq_idx_lookup(sde_kms,
				SDE_IRQ_TYPE_INTF_UNDER_RUN, i+INTF_0);
		sde_core_irq_register_callback(sde_kms, irq_idx, &irq_cb);
		sde_core_irq_enable(sde_kms, &irq_idx, 1);
	}
	sde_power_resource_enable(&priv->phandle, sde_kms->core_client, false);

	return 0;
}

void sde_core_irq_uninstall(struct sde_kms *sde_kms)
{
	struct msm_drm_private *priv;

	if (!sde_kms) {
		SDE_ERROR("invalid sde_kms\n");
		return;
	} else if (!sde_kms->dev) {
		SDE_ERROR("invalid drm device\n");
		return;
	} else if (!sde_kms->dev->dev_private) {
		SDE_ERROR("invalid device private\n");
		return;
	}
	priv = sde_kms->dev->dev_private;

	sde_power_resource_enable(&priv->phandle, sde_kms->core_client, true);
	sde_clear_all_irqs(sde_kms);
	sde_disable_all_irqs(sde_kms);
	sde_power_resource_enable(&priv->phandle, sde_kms->core_client, false);

	kfree(sde_kms->irq_obj.irq_cb_tbl);
	sde_kms->irq_obj.irq_cb_tbl = NULL;
}

irqreturn_t sde_core_irq(struct sde_kms *sde_kms)
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
	 * sde_core_irq_callback_handler with the irq_idx from the lookup table.
	 * sde_core_irq_callback_handler will perform the registered function
	 * callback, and do the interrupt status clearing once the registered
	 * callback is finished.
	 */
	sde_kms->hw_intr->ops.dispatch_irqs(
			sde_kms->hw_intr,
			sde_core_irq_callback_handler,
			sde_kms);

	return IRQ_HANDLED;
}
