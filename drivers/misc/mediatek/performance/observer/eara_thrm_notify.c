/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#define pr_fmt(fmt) "pob_eara_thrm_notifier: " fmt
#include <linux/notifier.h>
#include <mt-plat/mtk_perfobserver.h>

static BLOCKING_NOTIFIER_HEAD(pob_eara_thrm_notifier_list);

int pob_eara_thrm_register_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(
			&pob_eara_thrm_notifier_list, nb);
}

int pob_eara_thrm_unregister_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(
			&pob_eara_thrm_notifier_list, nb);
}

int pob_eara_thrm_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(
			&pob_eara_thrm_notifier_list, val, v);
}

int pob_eara_thrm_stats_update(enum pob_eara_thrm_info_num infonum)
{
	pob_eara_thrm_notifier_call_chain(infonum, NULL);

	return 0;
}

