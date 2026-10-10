/* SPDX-License-Identifier: GPL-2.0 */
/**
 * Copyright (c) 2025 Huaqin Technology(Shanghai) Co., Ltd.
 */

#include <linux/module.h>
#include "lx_notify.h"

// subpmic/buck charger notifer intertface to other module
static BLOCKING_NOTIFIER_HEAD(charger_notifier);

// chargepump notifer intertface to other module
// TODO: add chargepump notifier
//static BLOCKING_NOTIFIER_HEAD(chargepump_notifiier);

int lx_charger_notifier_register(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&charger_notifier, nb);
}
EXPORT_SYMBOL_GPL(lx_charger_notifier_register);

int lx_charger_notifier_unregister(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&charger_notifier, nb);
}
EXPORT_SYMBOL_GPL(lx_charger_notifier_unregister);

int lx_charger_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&charger_notifier, val, v);
}
EXPORT_SYMBOL_GPL(lx_charger_notifier_call_chain);

MODULE_DESCRIPTION("lx notify common driver");
MODULE_LICENSE("GPL v2");
