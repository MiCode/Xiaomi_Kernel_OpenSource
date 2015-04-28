/*
 * usb_typec_phy.c: usb phy driver for type-c cable connector
 *
 * Copyright (C) 2014 Intel Corporation
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. Seee the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Author: Kannappan, R <r.kannappan@intel.com>
 */

#include <linux/slab.h>
#include <linux/export.h>
#include <linux/sched.h>
#include <linux/usb_typec_phy.h>

static LIST_HEAD(typec_phy_list);
static spinlock_t typec_irq_lock;

struct typec_phy *typec_get_phy(int type)
{
	struct typec_phy *phy;
	unsigned long flags;

	spin_lock_irqsave(&typec_irq_lock, flags);
	list_for_each_entry(phy, &typec_phy_list, list) {
		if (phy->type != type)
			continue;
		spin_unlock_irqrestore(&typec_irq_lock, flags);
		return phy;
	}
	spin_unlock_irqrestore(&typec_irq_lock, flags);

	return ERR_PTR(-ENODEV);
}
EXPORT_SYMBOL_GPL(typec_get_phy);

int typec_get_cc_orientation(struct typec_phy *x)
{
	struct typec_phy *phy;
	unsigned long flags;

	if (x) {
		spin_lock_irqsave(&typec_irq_lock, flags);
		list_for_each_entry(phy, &typec_phy_list, list) {
			if (phy == x) {
				spin_unlock_irqrestore(&typec_irq_lock, flags);
				switch (phy->valid_cc) {
				case TYPEC_PIN_CC1:
					return TYPEC_POS_NORMAL;
				case TYPEC_PIN_CC2:
					return TYPEC_POS_SWAP;
				default:
					return TYPEC_POS_DISCONNECT;
				}
			}
		}
		spin_unlock_irqrestore(&typec_irq_lock, flags);
	}

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(typec_get_cc_orientation);

int typec_add_phy(struct typec_phy *phy)
{
	if (phy) {
		phy->type = USB_TYPE_C;
		phy->state = TYPEC_STATE_UNKNOWN;
		ATOMIC_INIT_NOTIFIER_HEAD(&phy->notifier);
		ATOMIC_INIT_NOTIFIER_HEAD(&phy->prot_notifier);
		list_add_tail(&phy->list, &typec_phy_list);
		return 0;
	}

	return -EINVAL;
}

int typec_remove_phy(struct typec_phy *x)
{
	struct typec_phy *phy;
	unsigned long flags;

	if (x) {
		spin_lock_irqsave(&typec_irq_lock, flags);
		list_for_each_entry(phy, &typec_phy_list, list) {
			if (phy == x)
				list_del(&phy->list);
		}
		spin_unlock_irqrestore(&typec_irq_lock, flags);
		return 0;
	}

	return -EINVAL;
}

static int __init phy_init(void)
{
	spin_lock_init(&typec_irq_lock);
	return 0;
}
arch_initcall(phy_init);
