// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 */

#include <linux/irq.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/spinlock.h>
#include <dt-bindings/interrupt-controller/arm-gic.h>
#include <linux/haven/hh_irq_lend.h>
#include <linux/haven/hh_rm_drv.h>

#include "hh_rm_drv_private.h"

struct hh_irq_entry {
	hh_vmid_t vmid;
	enum hh_vm_names vm_name;
	hh_irq_handle_fn handle;
	void *data;

	enum {
		/* start state */
		HH_IRQ_STATE_NONE,
		/* NONE -> WAIT_RELEASE by hh_irq_lend */
		HH_IRQ_STATE_WAIT_RELEASE,
		/* NONE -> WAIT_LEND by hh_irq_wait_lend */
		HH_IRQ_STATE_WAIT_LEND,
		/* WAIT_RELEASE -> RELEASED by notifier */
		/* RELEASED -> NONE by hh_irq_reclaim */
		HH_IRQ_STATE_RELEASED,
		/* WAIT_LEND -> LENT by notifier */
		/* LENT -> NONE by hh_irq_release */
		HH_IRQ_STATE_LENT,
	} state;
	hh_virq_handle_t virq_handle;
};

static struct hh_irq_entry hh_irq_entries[HH_IRQ_LABEL_MAX];
static DEFINE_SPINLOCK(hh_irq_lend_lock);

static int hh_irq_released_nb_handler(struct notifier_block *this,
				      unsigned long cmd, void *data)
{
	unsigned long flags;
	enum hh_irq_label label;
	struct hh_irq_entry *entry;
	struct hh_rm_notif_vm_irq_released_payload *released = data;

	if (cmd != HH_RM_NOTIF_VM_IRQ_RELEASED)
		return NOTIFY_DONE;

	spin_lock_irqsave(&hh_irq_lend_lock, flags);
	for (label = 0; label < HH_IRQ_LABEL_MAX; label++) {
		entry = &hh_irq_entries[label];
		if (entry->state != HH_IRQ_STATE_WAIT_RELEASE)
			continue;

		if (released->virq_handle == entry->virq_handle) {
			entry->state = HH_IRQ_STATE_RELEASED;
			spin_unlock_irqrestore(&hh_irq_lend_lock,
				flags);

			entry->handle(entry->data, label);

			return NOTIFY_OK;
		}
	}
	spin_unlock_irqrestore(&hh_irq_lend_lock, flags);

	return NOTIFY_DONE;
}

static struct notifier_block hh_irq_released_nb = {
	.notifier_call = hh_irq_released_nb_handler,
};

static int hh_irq_lent_nb_handler(struct notifier_block *this,
				  unsigned long cmd, void *data)
{
	unsigned long flags;
	enum hh_irq_label label;
	enum hh_vm_names owner_name;
	struct hh_irq_entry *entry;
	struct hh_rm_notif_vm_irq_lent_payload *lent = data;
	int ret;

	if (cmd != HH_RM_NOTIF_VM_IRQ_LENT)
		return NOTIFY_DONE;

	ret = hh_rm_get_vm_name(lent->owner_vmid, &owner_name);
	if (ret) {
		pr_warn_ratelimited("%s: unknown name for vmid: %d\n", __func__,
				    lent->owner_vmid);
		return ret;
	}

	spin_lock_irqsave(&hh_irq_lend_lock, flags);
	for (label = 0; label < HH_IRQ_LABEL_MAX; label++) {
		entry = &hh_irq_entries[label];
		if (entry->state != HH_IRQ_STATE_WAIT_LEND)
			continue;

		if (label == lent->virq_label &&
		    (entry->vm_name == HH_VM_MAX ||
		     entry->vm_name == owner_name)) {
			entry->vmid = lent->owner_vmid;
			entry->virq_handle = lent->virq_handle;

			entry->state = HH_IRQ_STATE_LENT;
			spin_unlock_irqrestore(&hh_irq_lend_lock,
					       flags);

			entry->handle(entry->data, label);

			return NOTIFY_OK;
		}
	}
	spin_unlock_irqrestore(&hh_irq_lend_lock, flags);

	return NOTIFY_DONE;
}

static struct notifier_block hh_irq_lent_nb = {
	.notifier_call = hh_irq_lent_nb_handler,
};

/**
 * hh_irq_lend: Lend a hardware interrupt to another VM
 * @label: vIRQ high-level label
 * @name: VM name to send interrupt to
 * @irq: Linux IRQ number to lend
 * @on_release: callback to invoke when other VM returns the
 *              interrupt
 * @data: Argument to pass to on_release
 */
int hh_irq_lend(enum hh_irq_label label, enum hh_vm_names name,
		int irq, hh_irq_handle_fn on_release, void *data)
{
	int ret, virq;
	unsigned long flags;
	struct hh_irq_entry *entry;
	struct irq_data *irq_data;

	if (label >= HH_IRQ_LABEL_MAX || !on_release)
		return -EINVAL;

	entry = &hh_irq_entries[label];

	irq_data = irq_get_irq_data(irq);
	if (!irq_data)
		return -EINVAL;
	virq = irq_data->hwirq;

	spin_lock_irqsave(&hh_irq_lend_lock, flags);
	if (entry->state != HH_IRQ_STATE_NONE) {
		spin_unlock_irqrestore(&hh_irq_lend_lock, flags);
		return -EINVAL;
	}

	ret = hh_rm_get_vmid(name, &entry->vmid);
	if (ret) {
		entry->state = HH_IRQ_STATE_NONE;
		spin_unlock_irqrestore(&hh_irq_lend_lock, flags);
		return ret;
	}

	entry->handle = on_release;
	entry->data = data;
	entry->state = HH_IRQ_STATE_WAIT_RELEASE;
	spin_unlock_irqrestore(&hh_irq_lend_lock, flags);

	return hh_rm_vm_irq_lend_notify(entry->vmid, virq, label,
		&entry->virq_handle);
}
EXPORT_SYMBOL(hh_irq_lend);

/**
 * hh_irq_reclaim: Reclaim a hardware interrupt after other VM
 * has released.
 * @label: vIRQ high-level label
 *
 * This function should be called inside or after on_release()
 * callback from hh_irq_lend.
 * This function is not thread-safe. Do not race with another hh_irq_reclaim
 * with same label
 */
int hh_irq_reclaim(enum hh_irq_label label)
{
	int ret;
	struct hh_irq_entry *entry;

	if (label >= HH_IRQ_LABEL_MAX)
		return -EINVAL;

	entry = &hh_irq_entries[label];

	if (entry->state != HH_IRQ_STATE_RELEASED)
		return -EINVAL;

	ret = hh_rm_vm_irq_reclaim(entry->virq_handle);
	if (!ret)
		entry->state = HH_IRQ_STATE_NONE;
	return ret;
}
EXPORT_SYMBOL(hh_irq_reclaim);

/**
 * hh_irq_wait_lend: Register to claim a lent interrupt from another
 * VM
 * @label: vIRQ high-level label
 * @name: Lender's VM name. If don't care, then use HH_VM_MAX
 * @on_lend: callback to invoke when other VM lends the interrupt
 * @data: Argument to pass to on_lend
 */
int hh_irq_wait_for_lend(enum hh_irq_label label, enum hh_vm_names name,
			 hh_irq_handle_fn on_lend, void *data)
{
	unsigned long flags;
	struct hh_irq_entry *entry;

	if (label >= HH_IRQ_LABEL_MAX || !on_lend)
		return -EINVAL;

	entry = &hh_irq_entries[label];

	spin_lock_irqsave(&hh_irq_lend_lock, flags);
	if (entry->state != HH_IRQ_STATE_NONE) {
		spin_unlock_irqrestore(&hh_irq_lend_lock, flags);
		return -EINVAL;
	}

	entry->vm_name = name;
	entry->handle = on_lend;
	entry->data = data;
	entry->state = HH_IRQ_STATE_WAIT_LEND;
	spin_unlock_irqrestore(&hh_irq_lend_lock, flags);

	return 0;
}
EXPORT_SYMBOL(hh_irq_wait_for_lend);

/**
 * hh_irq_accept: Register to receive interrupts with a lent vIRQ
 * @label: vIRQ high-level label
 * @irq: Linux IRQ# to associate vIRQ with. If don't care, use -1
 *
 * Returns the Linux IRQ# that vIRQ was registered to on success.
 * Returns <0 on error
 * This function is not thread-safe w.r.t. IRQ lend state. Do not race with
 * with hh_irq_release or another hh_irq_accept with same label.
 */
int hh_irq_accept(enum hh_irq_label label, int irq)
{
	struct hh_irq_entry *entry;
	const struct irq_data *irq_data;
	int virq;

	if (label >= HH_IRQ_LABEL_MAX)
		return -EINVAL;

	entry = &hh_irq_entries[label];

	if (entry->state != HH_IRQ_STATE_LENT)
		return -EINVAL;

	if (irq != -1) {
		irq_data = irq_get_irq_data(irq);
		if (!irq_data)
			return -EINVAL;
		virq = irq_data->hwirq;
	} else
		virq = -1;

	virq = hh_rm_vm_irq_accept(entry->virq_handle, virq);
	if (virq < 0)
		return virq;

	if (irq == -1)
		irq = hh_rm_virq_to_linux_irq(virq - 32, GIC_SPI,
					      IRQ_TYPE_LEVEL_HIGH);

	return irq;
}
EXPORT_SYMBOL(hh_irq_accept);

/**
 * hh_irq_release: Release a lent interrupt
 * @label: vIRQ high-level label
 * This function is not thread-safe w.r.t. IRQ lend state. Do not race with
 * with hh_irq_accept or another hh_irq_release with same label.
 */
int hh_irq_release(enum hh_irq_label label)
{
	int ret;
	struct hh_irq_entry *entry;

	if (label >= HH_IRQ_LABEL_MAX)
		return -EINVAL;

	entry = &hh_irq_entries[label];

	if (entry->state != HH_IRQ_STATE_LENT)
		return -EINVAL;

	ret = hh_rm_vm_irq_release_notify(entry->vmid,
					  entry->virq_handle);
	if (!ret)
		entry->state = HH_IRQ_STATE_WAIT_LEND;
	return ret;
}
EXPORT_SYMBOL(hh_irq_release);

static int __init hh_irq_lend_init(void)
{
	int ret;

	ret = hh_rm_register_notifier(&hh_irq_lent_nb);
	if (ret)
		return ret;
	return hh_rm_register_notifier(&hh_irq_released_nb);
}
module_init(hh_irq_lend_init);

static void hh_irq_lend_exit(void)
{
	hh_rm_unregister_notifier(&hh_irq_lent_nb);
	hh_rm_unregister_notifier(&hh_irq_released_nb);
}
module_exit(hh_irq_lend_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Qualcomm Technologies, Inc. Haven IRQ Lending Library");
