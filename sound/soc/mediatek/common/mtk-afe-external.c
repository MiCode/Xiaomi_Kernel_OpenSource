// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#include "mtk-afe-external.h"
#include <linux/module.h>

static RAW_NOTIFIER_HEAD(afe_mem_init_noitify_chain);

int register_afe_allocate_mem_notifier(struct notifier_block *nb)
{
	int status;

	status = raw_notifier_chain_register(&afe_mem_init_noitify_chain, nb);
	return status;
}
EXPORT_SYMBOL_GPL(register_afe_allocate_mem_notifier);

int unregister_afe_allocate_mem_notifier(struct notifier_block *nb)
{
	int status;

	status = raw_notifier_chain_unregister(&afe_mem_init_noitify_chain, nb);
	return status;
}
EXPORT_SYMBOL_GPL(unregister_afe_allocate_mem_notifier);

int notify_allocate_mem(unsigned long module, void *v)
{
	return raw_notifier_call_chain(&afe_mem_init_noitify_chain, module, v);
}
EXPORT_SYMBOL_GPL(notify_allocate_mem);

MODULE_DESCRIPTION("Mediatek afe external");
MODULE_AUTHOR("Shane Chien <shane.chien@mediatek.com>");
MODULE_LICENSE("GPL v2");
