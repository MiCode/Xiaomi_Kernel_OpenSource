// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */
#define pr_fmt(fmt) "pob_fpsgo_notifier: " fmt
#include <linux/notifier.h>
#include <mt-plat/mtk_perfobserver.h>

static BLOCKING_NOTIFIER_HEAD(pob_fpsgo_notifier_list);

int pob_fpsgo_register_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&pob_fpsgo_notifier_list, nb);
}

int pob_fpsgo_unregister_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&pob_fpsgo_notifier_list, nb);
}

int pob_fpsgo_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&pob_fpsgo_notifier_list, val, v);
}

int pob_fpsgo_fstb_stats_update(unsigned long infonum,
				struct pob_fpsgo_fpsstats_info *info)
{
	pob_fpsgo_notifier_call_chain(infonum, info);

	return 0;
}

int pob_fpsgo_qtsk_update(unsigned long infonum,
				struct pob_fpsgo_qtsk_info *info)
{
	pob_fpsgo_notifier_call_chain(infonum, info);

	return 0;
}

