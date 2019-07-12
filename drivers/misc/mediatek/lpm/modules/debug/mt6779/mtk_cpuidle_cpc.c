// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>

#include <mtk_lpm.h>

#include "mtk_cpuidle_cpc.h"

void mtk_cpc_prof_start(void)
{
	cpc_prof_en();
}

void mtk_cpc_prof_stop(void)
{
	cpc_prof_dis();
}

int mtk_cpc_notify(struct notifier_block *nb,
			unsigned long action, void *data)
{
	/* TODO : profile */
	return NOTIFY_OK;
}

struct notifier_block mtk_cpc_nb = {
	.notifier_call = mtk_cpc_notify,
};

int __init mtk_cpc_init(void)
{
	mtk_lpm_notifier_register(&mtk_cpc_nb);
	return 0;
}
late_initcall_sync(mtk_cpc_init);

