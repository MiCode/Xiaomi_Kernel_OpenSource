/* Copyright (c) 2015-2020, The Linux Foundation. All rights reserved.
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

#include <linux/debugfs.h>
#include <linux/irqdomain.h>
#include <linux/irq.h>
#include <linux/kthread.h>

#include "sde_core_irq.h"
#include "sde_power_handle.h"

/**
 * sde_core_irq_callback_handler - dispatch core interrupts
 * @arg:		private data of callback handler
 * @irq_idx:		interrupt index
 */
static void sde_core_irq_callback_handler(void *arg, int irq_idx)
{
	struct sde_kms *sde_kms = arg;
	struct sde_irq *irq_obj = &sde_kms->irq_obj;
	struct sde_irq_callback *cb;
	unsigned long irq_flags;
	bool cb_tbl_error = false;
	int enable_counts = 0;

	pr_debug("irq_idx=%d\n", irq_idx);

	spin_lock_irqsave(&sde_kms->irq_obj.cb_lock, irq_flags);
	if (list_empty(&irq_obj->irq_cb_tbl[irq_idx])) {
		/* print error outside lock */
		cb_tbl_error = true;
		enable_counts = atomic_read(
				&sde_kms->irq_obj.enable_counts[irq_idx]);
	}

	atomic_inc(&irq_obj->irq_counts[irq_idx]);

	/*
	 * Perform registered function callback
	 */
	list_for_each_entry(cb, &irq_obj->irq_cb_tbl[irq_idx], list)
		if (cb->func)
			cb->func(cb->arg, irq_idx);
	spin_unlock_irqrestore(&sde_kms->irq_obj.cb_lock, irq_flags);

	if (cb_tbl_error) {
		/*
		 * If enable count is zero and callback list is empty, then it's
		 * not a fatal issue. Log this case as debug. If the enable
		 * count is nonzero and callback list is empty, then its a real
		 * issue. Log this case as error to ensure we don't have silent
		 * IRQs running.
		 */
		if (!enable_counts) {
			SDE_DEBUG("irq has no callback, idx %d enables %d\n",
					irq_idx, enable_counts);
			SDE_EVT32_IRQ(irq_idx, enable_counts);
		} else {
			SDE_ERROR("irq has no callback, idx %d enables %d\n",
					irq_idx, enable_counts);
			SDE_EVT32_IRQ(irq_idx, enable_counts, SDE_EVTLOG_ERROR);
		}
	}

	/*
	 * Clear pending interrupt status in HW.
	 * NOTE: sde_core_irq_callback_handler is protected by top-level
	 *       spinlock, so it is safe to clear any interrupt status here.
	 */
	sde_kms->hw_intr->ops.clear_intr_status_nolock(
			sde_kms->hw_intr,
			irq_idx);
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

/**
 * _sde_core_irq_enable - enable core interrupt given by the index
 * @sde_kms:		Pointer to sde kms context
 * @irq_idx:		interrupt index
 */
static int _sde_core_irq_enable(struct sde_kms *sde_kms, int irq_idx)
{
	unsigned long irq_flags;
	int ret = 0;

	if (!sde_kms || !sde_kms->hw_intr ||
			!sde_kms->irq_obj.enable_counts ||
			!sde_kms->irq_obj.irq_counts) {
		SDE_ERROR("invalid params\n");
		return -EINVAL;
	}

	if (irq_idx < 0 || irq_idx >= sde_kms->hw_intr->irq_idx_tbl_size) {
		SDE_ERROR("invalid IRQ index: [%d]\n", irq_idx);
		return -EINVAL;
	}

	SDE_DEBUG("irq_idx=%d enable_count=%d\n", irq_idx,
			atomic_read(&sde_kms->irq_obj.enable_counts[irq_idx]));

	SDE_EVT32(irq_idx,
			atomic_read(&sde_kms->irq_obj.enable_counts[irq_idx]));
	if (atomic_inc_return(&sde_kms->irq_obj.enable_counts[irq_idx]) == 1) {
		ret = sde_kms->hw_intr->ops.enable_irq(
				sde_kms->hw_intr,
				irq_idx);
		if (ret)
			SDE_ERROR("Fail to enable IRQ for irq_idx:%d\n",
					irq_idx);

		SDE_DEBUG("irq_idx=%d ret=%d\n", irq_idx, ret);

		spin_lock_irqsave(&sde_kms->irq_obj.cb_lock, irq_flags);
		/* empty callback list but interrupt is enabled */
		if (list_empty(&sde_kms->irq_obj.irq_cb_tbl[irq_idx]))
			SDE_ERROR("irq_idx=%d enabled with no callback\n",
					irq_idx);
		spin_unlock_irqrestore(&sde_kms->irq_obj.cb_lock, irq_flags);
	}

	return ret;
}

int sde_core_irq_enable(struct sde_kms *sde_kms, int *irq_idxs, u32 irq_count)
{
	int i, ret = 0, counts;

	if (!sde_kms || !irq_idxs || !irq_count) {
		SDE_ERROR("invalid params\n");
		return -EINVAL;
	}

	counts = atomic_read(&sde_kms->irq_obj.enable_counts[irq_idxs[0]]);
	if (counts) {
		SDE_DEBUG("%pS: irq_idx=%d enable_count=%d\n",
			__builtin_return_address(0), irq_idxs[0], counts);
		SDE_EVT32(irq_idxs[0], counts, SDE_EVTLOG_ERROR);
	}

	for (i = 0; (i < irq_count) && !ret; i++)
		ret = _sde_core_irq_enable(sde_kms, irq_idxs[i]);

	return ret;
}

/**
 * _sde_core_irq_disable - disable core interrupt given by the index
 * @sde_kms:		Pointer to sde kms context
 * @irq_idx:		interrupt index
 */
static int _sde_core_irq_disable(struct sde_kms *sde_kms, int irq_idx)
{
	int ret = 0;

	if (!sde_kms || !sde_kms->hw_intr || !sde_kms->irq_obj.enable_counts) {
		SDE_ERROR("invalid params\n");
		return -EINVAL;
	}

	if (irq_idx < 0 || irq_idx >= sde_kms->hw_intr->irq_idx_tbl_size) {
		SDE_ERROR("invalid IRQ index: [%d]\n", irq_idx);
		return -EINVAL;
	}

	SDE_DEBUG("irq_idx=%d enable_count=%d\n", irq_idx,
			atomic_read(&sde_kms->irq_obj.enable_counts[irq_idx]));

	SDE_EVT32(irq_idx,
			atomic_read(&sde_kms->irq_obj.enable_counts[irq_idx]));
	if (atomic_dec_return(&sde_kms->irq_obj.enable_counts[irq_idx]) == 0) {
		ret = sde_kms->hw_intr->ops.disable_irq(
				sde_kms->hw_intr,
				irq_idx);
		if (ret)
			SDE_ERROR("Fail to disable IRQ for irq_idx:%d\n",
					irq_idx);
		SDE_DEBUG("irq_idx=%d ret=%d\n", irq_idx, ret);
	}

	return ret;
}

int sde_core_irq_disable(struct sde_kms *sde_kms, int *irq_idxs, u32 irq_count)
{
	int i, ret = 0, counts;

	if (!sde_kms || !irq_idxs || !irq_count) {
		SDE_ERROR("invalid params\n");
		return -EINVAL;
	}

	counts = atomic_read(&sde_kms->irq_obj.enable_counts[irq_idxs[0]]);
	if (counts == 2) {
		SDE_DEBUG("%pS: irq_idx=%d enable_count=%d\n",
			__builtin_return_address(0), irq_idxs[0], counts);
		SDE_EVT32(irq_idxs[0], counts, SDE_EVTLOG_ERROR);
	}

	for (i = 0; (i < irq_count) && !ret; i++)
		ret = _sde_core_irq_disable(sde_kms, irq_idxs[i]);

	return ret;
}

/**
 * sde_core_irq_disable_nolock - disable core interrupt given by the index
 *                               without lock
 * @sde_kms:		Pointer to sde kms context
 * @irq_idx:		interrupt index
 */
int sde_core_irq_disable_nolock(struct sde_kms *sde_kms, int irq_idx)
{
	int ret = 0;

	if (!sde_kms || !sde_kms->hw_intr || !sde_kms->irq_obj.enable_counts) {
		SDE_ERROR("invalid params\n");
		return -EINVAL;
	}

	if (irq_idx < 0 || irq_idx >= sde_kms->hw_intr->irq_idx_tbl_size) {
		SDE_ERROR("invalid IRQ index: [%d]\n", irq_idx);
		return -EINVAL;
	}

	SDE_DEBUG("irq_idx=%d enable_count=%d\n", irq_idx,
			atomic_read(&sde_kms->irq_obj.enable_counts[irq_idx]));

	SDE_EVT32(irq_idx,
			atomic_read(&sde_kms->irq_obj.enable_counts[irq_idx]));
	if (atomic_dec_return(&sde_kms->irq_obj.enable_counts[irq_idx]) == 0) {
		ret = sde_kms->hw_intr->ops.disable_irq_nolock(
				sde_kms->hw_intr,
				irq_idx);
		if (ret)
			SDE_ERROR("Fail to disable IRQ for irq_idx:%d\n",
					irq_idx);
		SDE_DEBUG("irq_idx=%d ret=%d\n", irq_idx, ret);
	}

	return ret;
}

u32 sde_core_irq_read_nolock(struct sde_kms *sde_kms, int irq_idx, bool clear)
{
	if (!sde_kms || !sde_kms->hw_intr ||
			!sde_kms->hw_intr->ops.get_interrupt_status)
		return 0;

	if (irq_idx < 0) {
		SDE_ERROR("[%pS] invalid irq_idx=%d\n",
				__builtin_return_address(0), irq_idx);
		return 0;
	}

	return sde_kms->hw_intr->ops.get_intr_status_nolock(sde_kms->hw_intr,
			irq_idx, clear);
}

u32 sde_core_irq_read(struct sde_kms *sde_kms, int irq_idx, bool clear)
{
	if (!sde_kms || !sde_kms->hw_intr ||
			!sde_kms->hw_intr->ops.get_interrupt_status)
		return 0;

	if (irq_idx < 0) {
		SDE_ERROR("[%pS] invalid irq_idx=%d\n",
				__builtin_return_address(0), irq_idx);
		return 0;
	}

	return sde_kms->hw_intr->ops.get_interrupt_status(sde_kms->hw_intr,
			irq_idx, clear);
}

int sde_core_irq_register_callback(struct sde_kms *sde_kms, int irq_idx,
		struct sde_irq_callback *register_irq_cb)
{
	unsigned long irq_flags;

	if (!sde_kms || !sde_kms->irq_obj.irq_cb_tbl) {
		SDE_ERROR("invalid params\n");
		return -EINVAL;
	}

	if (!register_irq_cb || !register_irq_cb->func) {
		SDE_ERROR("invalid irq_cb:%d func:%d\n",
				register_irq_cb != NULL,
				register_irq_cb ?
					register_irq_cb->func != NULL : -1);
		return -EINVAL;
	}

	if (irq_idx < 0 || irq_idx >= sde_kms->hw_intr->irq_idx_tbl_size) {
		SDE_ERROR("invalid IRQ index: [%d]\n", irq_idx);
		return -EINVAL;
	}

	SDE_DEBUG("[%pS] irq_idx=%d\n", __builtin_return_address(0), irq_idx);

	spin_lock_irqsave(&sde_kms->irq_obj.cb_lock, irq_flags);
	SDE_EVT32(irq_idx, register_irq_cb);
	list_del_init(&register_irq_cb->list);
	list_add_tail(&register_irq_cb->list,
			&sde_kms->irq_obj.irq_cb_tbl[irq_idx]);
	spin_unlock_irqrestore(&sde_kms->irq_obj.cb_lock, irq_flags);

	return 0;
}

int sde_core_irq_unregister_callback(struct sde_kms *sde_kms, int irq_idx,
		struct sde_irq_callback *register_irq_cb)
{
	unsigned long irq_flags;

	if (!sde_kms || !sde_kms->irq_obj.irq_cb_tbl) {
		SDE_ERROR("invalid params\n");
		return -EINVAL;
	}

	if (!register_irq_cb || !register_irq_cb->func) {
		SDE_ERROR("invalid irq_cb:%d func:%d\n",
				register_irq_cb != NULL,
				register_irq_cb ?
					register_irq_cb->func != NULL : -1);
		return -EINVAL;
	}

	if (irq_idx < 0 || irq_idx >= sde_kms->hw_intr->irq_idx_tbl_size) {
		SDE_ERROR("invalid IRQ index: [%d]\n", irq_idx);
		return -EINVAL;
	}

	SDE_DEBUG("[%pS] irq_idx=%d\n", __builtin_return_address(0), irq_idx);

	spin_lock_irqsave(&sde_kms->irq_obj.cb_lock, irq_flags);
	SDE_EVT32(irq_idx, register_irq_cb);
	list_del_init(&register_irq_cb->list);
	/* empty callback list but interrupt is still enabled */
	if (list_empty(&sde_kms->irq_obj.irq_cb_tbl[irq_idx]) &&
			atomic_read(&sde_kms->irq_obj.enable_counts[irq_idx]))
		SDE_ERROR("irq_idx=%d enabled with no callback\n", irq_idx);
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

#ifdef CONFIG_DEBUG_FS
#define DEFINE_SDE_DEBUGFS_SEQ_FOPS(__prefix)				\
static int __prefix ## _open(struct inode *inode, struct file *file)	\
{									\
	return single_open(file, __prefix ## _show, inode->i_private);	\
}									\
static const struct file_operations __prefix ## _fops = {		\
	.owner = THIS_MODULE,						\
	.open = __prefix ## _open,					\
	.release = single_release,					\
	.read = seq_read,						\
	.llseek = seq_lseek,						\
}

static int sde_debugfs_core_irq_show(struct seq_file *s, void *v)
{
	struct sde_irq *irq_obj = s->private;
	struct sde_irq_callback *cb;
	unsigned long irq_flags;
	int i, irq_count, enable_count, cb_count;

	if (!irq_obj || !irq_obj->enable_counts || !irq_obj->irq_cb_tbl) {
		SDE_ERROR("invalid parameters\n");
		return 0;
	}

	for (i = 0; i < irq_obj->total_irqs; i++) {
		spin_lock_irqsave(&irq_obj->cb_lock, irq_flags);
		cb_count = 0;
		irq_count = atomic_read(&irq_obj->irq_counts[i]);
		enable_count = atomic_read(&irq_obj->enable_counts[i]);
		list_for_each_entry(cb, &irq_obj->irq_cb_tbl[i], list)
			cb_count++;
		spin_unlock_irqrestore(&irq_obj->cb_lock, irq_flags);

		if (irq_count || enable_count || cb_count)
			seq_printf(s, "idx:%d irq:%d enable:%d cb:%d\n",
					i, irq_count, enable_count, cb_count);
	}

	return 0;
}

DEFINE_SDE_DEBUGFS_SEQ_FOPS(sde_debugfs_core_irq);

int sde_debugfs_core_irq_init(struct sde_kms *sde_kms,
		struct dentry *parent)
{
	sde_kms->irq_obj.debugfs_file = debugfs_create_file("core_irq", 0600,
			parent, &sde_kms->irq_obj,
			&sde_debugfs_core_irq_fops);

	return 0;
}

void sde_debugfs_core_irq_destroy(struct sde_kms *sde_kms)
{
	debugfs_remove(sde_kms->irq_obj.debugfs_file);
	sde_kms->irq_obj.debugfs_file = NULL;
}

#else
int sde_debugfs_core_irq_init(struct sde_kms *sde_kms,
		struct dentry *parent)
{
	return 0;
}

void sde_debugfs_core_irq_destroy(struct sde_kms *sde_kms)
{
}
#endif

void sde_core_irq_preinstall(struct sde_kms *sde_kms)
{
	struct msm_drm_private *priv;
	int i;
	int rc;

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

	rc = sde_power_resource_enable(&priv->phandle, sde_kms->core_client,
			true);
	if (rc) {
		SDE_ERROR("failed to enable power resource %d\n", rc);
		SDE_EVT32(rc, SDE_EVTLOG_ERROR);
		return;
	}

	sde_clear_all_irqs(sde_kms);
	sde_disable_all_irqs(sde_kms);
	sde_power_resource_enable(&priv->phandle, sde_kms->core_client, false);

	spin_lock_init(&sde_kms->irq_obj.cb_lock);

	/* Create irq callbacks for all possible irq_idx */
	sde_kms->irq_obj.total_irqs = sde_kms->hw_intr->irq_idx_tbl_size;
	sde_kms->irq_obj.irq_cb_tbl = kcalloc(sde_kms->irq_obj.total_irqs,
			sizeof(struct list_head), GFP_KERNEL);
	sde_kms->irq_obj.enable_counts = kcalloc(sde_kms->irq_obj.total_irqs,
			sizeof(atomic_t), GFP_KERNEL);
	sde_kms->irq_obj.irq_counts = kcalloc(sde_kms->irq_obj.total_irqs,
			sizeof(atomic_t), GFP_KERNEL);
	if (!sde_kms->irq_obj.irq_cb_tbl || !sde_kms->irq_obj.enable_counts
			|| !sde_kms->irq_obj.irq_counts)
		return;

	for (i = 0; i < sde_kms->irq_obj.total_irqs; i++) {
		if (sde_kms->irq_obj.irq_cb_tbl)
			INIT_LIST_HEAD(&sde_kms->irq_obj.irq_cb_tbl[i]);
		if (sde_kms->irq_obj.enable_counts)
			atomic_set(&sde_kms->irq_obj.enable_counts[i], 0);
		if (sde_kms->irq_obj.irq_counts)
			atomic_set(&sde_kms->irq_obj.irq_counts[i], 0);
	}
}

int sde_core_irq_postinstall(struct sde_kms *sde_kms)
{
	return 0;
}

void sde_core_irq_uninstall(struct sde_kms *sde_kms)
{
	struct msm_drm_private *priv;
	int i;
	int rc;
	unsigned long irq_flags;

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

	rc = sde_power_resource_enable(&priv->phandle, sde_kms->core_client,
			true);
	if (rc) {
		SDE_ERROR("failed to enable power resource %d\n", rc);
		SDE_EVT32(rc, SDE_EVTLOG_ERROR);
		return;
	}

	for (i = 0; i < sde_kms->irq_obj.total_irqs; i++)
		if (atomic_read(&sde_kms->irq_obj.enable_counts[i]) ||
				!list_empty(&sde_kms->irq_obj.irq_cb_tbl[i]))
			SDE_ERROR("irq_idx=%d still enabled/registered\n", i);

	sde_clear_all_irqs(sde_kms);
	sde_disable_all_irqs(sde_kms);
	sde_power_resource_enable(&priv->phandle, sde_kms->core_client, false);

	spin_lock_irqsave(&sde_kms->irq_obj.cb_lock, irq_flags);
	kfree(sde_kms->irq_obj.irq_cb_tbl);
	kfree(sde_kms->irq_obj.enable_counts);
	kfree(sde_kms->irq_obj.irq_counts);
	sde_kms->irq_obj.irq_cb_tbl = NULL;
	sde_kms->irq_obj.enable_counts = NULL;
	sde_kms->irq_obj.irq_counts = NULL;
	sde_kms->irq_obj.total_irqs = 0;
	spin_unlock_irqrestore(&sde_kms->irq_obj.cb_lock, irq_flags);
}

static void sde_core_irq_mask(struct irq_data *irqd)
{
	struct sde_kms *sde_kms;

	if (!irqd || !irq_data_get_irq_chip_data(irqd)) {
		SDE_ERROR("invalid parameters irqd %d\n", irqd != NULL);
		return;
	}
	sde_kms = irq_data_get_irq_chip_data(irqd);

	/* memory barrier */
	smp_mb__before_atomic();
	clear_bit(irqd->hwirq, &sde_kms->irq_controller.enabled_mask);
	/* memory barrier */
	smp_mb__after_atomic();
}

static void sde_core_irq_unmask(struct irq_data *irqd)
{
	struct sde_kms *sde_kms;

	if (!irqd || !irq_data_get_irq_chip_data(irqd)) {
		SDE_ERROR("invalid parameters irqd %d\n", irqd != NULL);
		return;
	}
	sde_kms = irq_data_get_irq_chip_data(irqd);

	/* memory barrier */
	smp_mb__before_atomic();
	set_bit(irqd->hwirq, &sde_kms->irq_controller.enabled_mask);
	/* memory barrier */
	smp_mb__after_atomic();
}

static struct irq_chip sde_core_irq_chip = {
	.name = "sde",
	.irq_mask = sde_core_irq_mask,
	.irq_unmask = sde_core_irq_unmask,
};

static int sde_core_irqdomain_map(struct irq_domain *domain,
		unsigned int irq, irq_hw_number_t hwirq)
{
	struct sde_kms *sde_kms;
	int rc;

	if (!domain || !domain->host_data) {
		SDE_ERROR("invalid parameters domain %d\n", domain != NULL);
		return -EINVAL;
	}
	sde_kms = domain->host_data;

	irq_set_chip_and_handler(irq, &sde_core_irq_chip, handle_level_irq);
	rc = irq_set_chip_data(irq, sde_kms);

	return rc;
}

static const struct irq_domain_ops sde_core_irqdomain_ops = {
	.map = sde_core_irqdomain_map,
	.xlate = irq_domain_xlate_onecell,
};

int sde_core_irq_domain_add(struct sde_kms *sde_kms)
{
	struct device *dev;
	struct irq_domain *domain;

	if (!sde_kms->dev || !sde_kms->dev->dev) {
		pr_err("invalid device handles\n");
		return -EINVAL;
	}

	dev = sde_kms->dev->dev;

	domain = irq_domain_add_linear(dev->of_node, 32,
			&sde_core_irqdomain_ops, sde_kms);
	if (!domain) {
		pr_err("failed to add irq_domain\n");
		return -EINVAL;
	}

	sde_kms->irq_controller.enabled_mask = 0;
	sde_kms->irq_controller.domain = domain;

	return 0;
}

int sde_core_irq_domain_fini(struct sde_kms *sde_kms)
{
	if (sde_kms->irq_controller.domain) {
		irq_domain_remove(sde_kms->irq_controller.domain);
		sde_kms->irq_controller.domain = NULL;
	}
	return 0;
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
