// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */
#define pr_fmt(fmt) "pob_nn: " fmt
#include <linux/notifier.h>
#include <mt-plat/mtk_perfobserver.h>

static BLOCKING_NOTIFIER_HEAD(pob_nn_notifier_list);

int pob_nn_register_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&pob_nn_notifier_list, nb);
}

int pob_nn_unregister_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&pob_nn_notifier_list, nb);
}

int pob_nn_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&pob_nn_notifier_list, val, v);
}

int pob_nn_update(enum pob_nn_info_num info_num, void *v)
{
	pob_nn_notifier_call_chain(info_num, v);

	return 0;
}

