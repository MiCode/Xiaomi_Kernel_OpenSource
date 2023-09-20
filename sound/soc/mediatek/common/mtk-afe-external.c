// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#include "mtk-afe-external.h"
#include <linux/module.h>

static RAW_NOTIFIER_HEAD(afe_mem_init_noitify_chain);
static ATOMIC_NOTIFIER_HEAD(semaphore_noitify_chain);
static RAW_NOTIFIER_HEAD(vow_ipi_send_noitify_chain);

/* memory allocate */
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

/* semaphore control */
int register_3way_semaphore_notifier(struct notifier_block *nb)
{
	return atomic_notifier_chain_register(&semaphore_noitify_chain, nb);
}
EXPORT_SYMBOL_GPL(register_3way_semaphore_notifier);

int unregister_3way_semaphore_notifier(struct notifier_block *nb)
{
	return atomic_notifier_chain_unregister(&semaphore_noitify_chain, nb);
}
EXPORT_SYMBOL_GPL(unregister_3way_semaphore_notifier);

int notify_3way_semaphore_control(unsigned long module, void *v)
{
	return atomic_notifier_call_chain(&semaphore_noitify_chain, module, v);
}
EXPORT_SYMBOL_GPL(notify_3way_semaphore_control);

/* vow send ipi */
int register_vow_ipi_send_notifier(struct notifier_block *nb)
{
	int status;

	status = raw_notifier_chain_register(&vow_ipi_send_noitify_chain, nb);
	return status;
}
EXPORT_SYMBOL_GPL(register_vow_ipi_send_notifier);

int unregister_vow_ipi_send_notifier(struct notifier_block *nb)
{
	int status;

	status = raw_notifier_chain_unregister(&vow_ipi_send_noitify_chain, nb);
	return status;
}
EXPORT_SYMBOL_GPL(unregister_vow_ipi_send_notifier);

int notify_vow_ipi_send(unsigned long module, void *v)
{
	return raw_notifier_call_chain(&vow_ipi_send_noitify_chain, module, v);
}
EXPORT_SYMBOL_GPL(notify_vow_ipi_send);

MODULE_DESCRIPTION("Mediatek afe external");
MODULE_AUTHOR("Shane Chien <shane.chien@mediatek.com>");
MODULE_LICENSE("GPL v2");
