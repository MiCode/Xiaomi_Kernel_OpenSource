// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 Unisoc Communications Inc.
 */

#include <linux/module.h>
#include <linux/notifier.h>

ATOMIC_NOTIFIER_HEAD(unisoc_shrink_anon_notify_list);

int register_unisoc_shrink_anon_notifier(struct notifier_block *nb)
{
	return atomic_notifier_chain_register(&unisoc_shrink_anon_notify_list, nb);
}
EXPORT_SYMBOL_GPL(register_unisoc_shrink_anon_notifier);

int unregister_unisoc_shrink_anon_notifier(struct notifier_block *nb)
{
	return atomic_notifier_chain_unregister(&unisoc_shrink_anon_notify_list, nb);
}
EXPORT_SYMBOL_GPL(unregister_unisoc_shrink_anon_notifier);

int unisoc_shrink_anon_notifier_call_chain(void *data)
{
	return atomic_notifier_call_chain(&unisoc_shrink_anon_notify_list, 0, data);
}
EXPORT_SYMBOL_GPL(unisoc_shrink_anon_notifier_call_chain);
