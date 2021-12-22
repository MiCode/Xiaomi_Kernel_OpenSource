// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */
#define pr_fmt(fmt) "pob_xpu: " fmt
#include <linux/notifier.h>
#include <mt-plat/mtk_perfobserver.h>

static BLOCKING_NOTIFIER_HEAD(pob_xpufreq_notifier_list);

int pob_xpufreq_register_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&pob_xpufreq_notifier_list, nb);
}
EXPORT_SYMBOL(pob_xpufreq_register_client);

int pob_xpufreq_unregister_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&pob_xpufreq_notifier_list,
							nb);
}
EXPORT_SYMBOL(pob_xpufreq_unregister_client);

int pob_xpufreq_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&pob_xpufreq_notifier_list, val, v);
}

int pob_xpufreq_update(enum pob_xpufreq_info_num info_num,
			struct pob_xpufreq_info *v)
{
	pob_xpufreq_notifier_call_chain(info_num, v);

	return 0;
}
EXPORT_SYMBOL(pob_xpufreq_update);
