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
#define pr_fmt(fmt) "pob_bqd: " fmt
#include <linux/notifier.h>
#include <mt-plat/mtk_perfobserver.h>

static BLOCKING_NOTIFIER_HEAD(pob_bqd_notifier_list);

/**
 *	bqd_register_client - register a client notifier
 *	@nb: notifier block to callback on events
 */
int pob_bqd_register_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&pob_bqd_notifier_list, nb);
}

/**
 *	bqd_unregister_client - unregister a client notifier
 *	@nb: notifier block to callback on events
 */
int pob_bqd_unregister_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&pob_bqd_notifier_list, nb);
}

/**
 * bqd_notifier_call_chain - notify clients
 *
 */
int pob_bqd_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&pob_bqd_notifier_list, val, v);
}


int pob_bqd_queue_update(unsigned long long bufferid, int connectapi,
				unsigned long long cameraid)
{
	struct pob_bqd_info pbi = {bufferid, connectapi, cameraid};

	pob_bqd_notifier_call_chain(POB_BQD_QUEUE, (void *) &pbi);

	return 0;
}

int pob_bqd_acquire_update(unsigned long long bufferid, int connectapi)
{
	struct pob_bqd_info pbi = {bufferid, connectapi, 0};

	pob_bqd_notifier_call_chain(POB_BQD_ACQUIRE, (void *) &pbi);

	return 0;
}
