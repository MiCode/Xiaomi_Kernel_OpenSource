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
	hh_irq_handle_fn_v2 v2_handle;
	hh_irq_handle_fn handle;
	void *data;

	enum {
		HH_IRQ_STATE_NONE,

		HH_IRQ_STATE_WAIT_RELEASE_OR_ACCEPT,
		HH_IRQ_STATE_WAIT_LEND,

		/* notification states */
		HH_IRQ_STATE_RELEASED, /* svm -> pvm */
		HH_IRQ_STATE_ACCEPTED, /* svm -> pvm */
		HH_IRQ_STATE_LENT, /* pvm -> svm */
	} state;
	hh_virq_handle_t virq_handle;
};

static struct hh_irq_entry hh_irq_entries[HH_IRQ_LABEL_MAX];
static DEFINE_SPINLOCK(hh_irq_lend_lock);

static int hh_irq_released_accepted_nb_handler(struct notifier_block *this,
				      unsigned long cmd, void *data)
{
	unsigned long flags;
	enum hh_irq_label label;
	struct hh_irq_entry *entry;
	struct hh_rm_notif_vm_irq_released_payload *released;
	struct hh_rm_notif_vm_irq_accepted_payload *accepted;

	if (cmd != HH_RM_NOTIF_VM_IRQ_RELEASED &&
			cmd != HH_RM_NOTIF_VM_IRQ_ACCEPTED)
		return NOTIFY_DONE;

	spin_lock_irqsave(&hh_irq_lend_lock, flags);
	for (label = 0; label < HH_IRQ_LABEL_MAX; label++) {
		entry = &hh_irq_entries[label];

		if (entry->state != HH_IRQ_STATE_WAIT_RELEASE_OR_ACCEPT &&
					entry->state != HH_IRQ_STATE_ACCEPTED)
			continue;

		switch (cmd) {
		case HH_RM_NOTIF_VM_IRQ_RELEASED:
			released = data;
			if (released->virq_handle == entry->virq_handle) {
				entry->state = HH_IRQ_STATE_RELEASED;
				spin_unlock_irqrestore(&hh_irq_lend_lock,
									flags);
				entry->v2_handle(entry->data, cmd, label);
				return NOTIFY_OK;
			}

			break;
		case HH_RM_NOTIF_VM_IRQ_ACCEPTED:
			accepted = data;
			if (accepted->virq_handle == entry->virq_handle) {
				entry->state = HH_IRQ_STATE_ACCEPTED;
				spin_unlock_irqrestore(&hh_irq_lend_lock,
									flags);
				entry->v2_handle(entry->data, cmd, label);
				return NOTIFY_OK;
			}

			break;
		}
	}
	spin_unlock_irqrestore(&hh_irq_lend_lock, flags);

	return NOTIFY_DONE;
}

static struct notifier_block hh_irq_released_accepted_nb = {
	.notifier_call = hh_irq_released_accepted_nb_handler,
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
		if (entry->state != HH_IRQ_STATE_WAIT_LEND &&
				entry->state != HH_IRQ_STATE_LENT)
			continue;

		if (label == lent->virq_label &&
		    (entry->vm_name == HH_VM_MAX ||
		     entry->vm_name == owner_name)) {
			entry->vmid = lent->owner_vmid;
			entry->virq_handle = lent->virq_handle;

			entry->state = HH_IRQ_STATE_LENT;
			spin_unlock_irqrestore(&hh_irq_lend_lock,
					       flags);

			entry->v2_handle(entry->data, cmd, label);

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
 * hh_irq_lend_v2: Lend a hardware interrupt to another VM
 * @label: vIRQ high-level label
 * @name: VM name to send interrupt to
 * @irq: Linux IRQ number to lend
 * @cb_handle: callback to invoke when other VM release or accept the interrupt
 * @data: Argument to pass to cb_handle
 *
 * Returns 0 on success also the handle corresponding to Linux IRQ#.
 * Returns < 0 on error
 */
int hh_irq_lend_v2(enum hh_irq_label label, enum hh_vm_names name,
		int irq, hh_irq_handle_fn_v2 cb_handle, void *data)
{
	int ret, virq;
	unsigned long flags;
	struct hh_irq_entry *entry;

	if (label >= HH_IRQ_LABEL_MAX || !cb_handle)
		return -EINVAL;

	entry = &hh_irq_entries[label];

	if (hh_rm_irq_to_virq(irq, &virq))
		return -EINVAL;

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

	entry->v2_handle = cb_handle;
	entry->data = data;
	entry->state = HH_IRQ_STATE_WAIT_RELEASE_OR_ACCEPT;
	spin_unlock_irqrestore(&hh_irq_lend_lock, flags);

	return hh_rm_vm_irq_lend(entry->vmid, virq, label, &entry->virq_handle);
}
EXPORT_SYMBOL(hh_irq_lend_v2);

/**
 * hh_irq_lend: Lend a hardware interrupt to another VM
 * @label: vIRQ high-level label
 * @name: VM name to send interrupt to
 * @irq: Linux IRQ number to lend
 * @cb_handle: callback to invoke when other VM release or accept the interrupt
 * @data: Argument to pass to cb_handle
 *
 * Returns 0 on success also the handle corresponding to Linux IRQ#.
 * Returns < 0 on error
 */
int hh_irq_lend(enum hh_irq_label label, enum hh_vm_names name,
		int irq, hh_irq_handle_fn cb_handle, void *data)
{
	struct hh_irq_entry *entry;

	if (label >= HH_IRQ_LABEL_MAX || !cb_handle)
		return -EINVAL;

	entry = &hh_irq_entries[label];
	entry->handle = cb_handle;

	return 0;
}
EXPORT_SYMBOL(hh_irq_lend);

/**
 * hh_irq_lend_notify: Pass the irq handle to other VM for accept
 * @label: vIRQ high-level label
 *
 * Returns 0 on success, < 0 on error
 */
int hh_irq_lend_notify(enum hh_irq_label label)
{
	struct hh_irq_entry *entry;

	if (label >= HH_IRQ_LABEL_MAX)
		return -EINVAL;

	entry = &hh_irq_entries[label];
	if (entry->state == HH_IRQ_STATE_NONE)
		return -EINVAL;

	return hh_rm_vm_irq_lend_notify(entry->vmid, entry->virq_handle);
}
EXPORT_SYMBOL(hh_irq_lend_notify);

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

	if (entry->state != HH_IRQ_STATE_WAIT_RELEASE_OR_ACCEPT &&
			(entry->state != HH_IRQ_STATE_RELEASED))
		return -EINVAL;

	ret = hh_rm_vm_irq_reclaim(entry->virq_handle);
	if (!ret)
		entry->state = HH_IRQ_STATE_NONE;
	return ret;
}
EXPORT_SYMBOL(hh_irq_reclaim);

/**
 * hh_irq_wait_for_lend_v2: Register to claim a lent interrupt from another VM
 * @label: vIRQ high-level label
 * @name: Lender's VM name. If don't care, then use HH_VM_MAX
 * @on_lend: callback to invoke when other VM lends the interrupt
 * @data: Argument to pass to on_lend
 */
int hh_irq_wait_for_lend_v2(enum hh_irq_label label, enum hh_vm_names name,
			 hh_irq_handle_fn_v2 on_lend, void *data)
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
	entry->v2_handle = on_lend;
	entry->data = data;
	entry->state = HH_IRQ_STATE_WAIT_LEND;
	spin_unlock_irqrestore(&hh_irq_lend_lock, flags);

	return 0;
}
EXPORT_SYMBOL(hh_irq_wait_for_lend_v2);

/**
 * hh_irq_wait_lend: Register to claim a lent interrupt from another VM
 * @label: vIRQ high-level label
 * @name: Lender's VM name. If don't care, then use HH_VM_MAX
 * @on_lend: callback to invoke when other VM lends the interrupt
 * @data: Argument to pass to on_lend
 */
int hh_irq_wait_for_lend(enum hh_irq_label label, enum hh_vm_names name,
			 hh_irq_handle_fn on_lend, void *data)
{
	return 0;
}
EXPORT_SYMBOL(hh_irq_wait_for_lend);

/**
 * hh_irq_accept: Register to receive interrupts with a lent vIRQ
 * @label: vIRQ high-level label
 * @irq: Linux IRQ# to associate vIRQ with. If don't care, use -1
 * @type: IRQ flags to use when allowing RM to choose the IRQ. If irq parameter
 *        is specified, then type is unused.
 *
 * Returns the Linux IRQ# that vIRQ was registered to on success.
 * Returns <0 on error
 * This function is not thread-safe w.r.t. IRQ lend state. Do not race with
 * with hh_irq_release or another hh_irq_accept with same label.
 */
int hh_irq_accept(enum hh_irq_label label, int irq, int type)
{
	struct hh_irq_entry *entry;
	u32 virq;

	if (label >= HH_IRQ_LABEL_MAX)
		return -EINVAL;

	entry = &hh_irq_entries[label];

	if (entry->state != HH_IRQ_STATE_LENT)
		return -EINVAL;

	if (irq != -1) {
		if (hh_rm_irq_to_virq(irq, &virq))
			return -EINVAL;
	} else
		virq = -1;

	virq = hh_rm_vm_irq_accept(entry->virq_handle, virq);
	if (virq < 0)
		return virq;

	if (irq == -1)
		irq = hh_rm_virq_to_irq(virq, type);

	entry->state = HH_IRQ_STATE_ACCEPTED;
	return irq;
}
EXPORT_SYMBOL(hh_irq_accept);

/**
 * hh_irq_accept_notify: Notify the lend vm (pvm) that IRQ is accepted
 * @label: vIRQ high-level label
 * @irq: Linux IRQ# to associate vIRQ with. If don't care, use -1
 *
 * Returns the Linux IRQ# that vIRQ was registered to on success.
 * Returns <0 on error
 * This function is not thread-safe w.r.t. IRQ lend state. Do not race with
 * hh_irq_release or another hh_irq_accept with same label.
 */
int hh_irq_accept_notify(enum hh_irq_label label)
{
	struct hh_irq_entry *entry;

	if (label >= HH_IRQ_LABEL_MAX)
		return -EINVAL;

	entry = &hh_irq_entries[label];

	if (entry->state != HH_IRQ_STATE_ACCEPTED)
		return -EINVAL;

	return hh_rm_vm_irq_accept_notify(entry->vmid,
					  entry->virq_handle);
}
EXPORT_SYMBOL(hh_irq_accept_notify);

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

	if (entry->state != HH_IRQ_STATE_ACCEPTED)
		return -EINVAL;

	ret = hh_rm_vm_irq_release(entry->virq_handle);
	if (!ret)
		entry->state = HH_IRQ_STATE_WAIT_LEND;
	return ret;
}
EXPORT_SYMBOL(hh_irq_release);

int hh_irq_release_notify(enum hh_irq_label label)
{
	struct hh_irq_entry *entry;

	if (label >= HH_IRQ_LABEL_MAX)
		return -EINVAL;

	entry = &hh_irq_entries[label];

	if (entry->state != HH_IRQ_STATE_ACCEPTED &&
			entry->state != HH_IRQ_STATE_WAIT_LEND)
		return -EINVAL;

	return hh_rm_vm_irq_release_notify(entry->vmid,
					  entry->virq_handle);
}
EXPORT_SYMBOL(hh_irq_release_notify);

static int __init hh_irq_lend_init(void)
{
	int ret;

	ret = hh_rm_register_notifier(&hh_irq_lent_nb);
	if (ret)
		return ret;

	return hh_rm_register_notifier(&hh_irq_released_accepted_nb);
}
module_init(hh_irq_lend_init);

static void hh_irq_lend_exit(void)
{
	hh_rm_unregister_notifier(&hh_irq_lent_nb);
	hh_rm_unregister_notifier(&hh_irq_released_accepted_nb);
}
module_exit(hh_irq_lend_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Qualcomm Technologies, Inc. Haven IRQ Lending Library");
