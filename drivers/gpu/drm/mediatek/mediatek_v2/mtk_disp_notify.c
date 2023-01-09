// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/module.h>
#include "mtk_disp_notify.h"
#include "mtk_log.h"

static BLOCKING_NOTIFIER_HEAD(disp_notifier_list);
static BLOCKING_NOTIFIER_HEAD(disp_sub_notifier_list);

int mtk_disp_notifier_register(const char *source, struct notifier_block *nb)
{
	if (!source)
		return -EINVAL;

	DDPFUNC(":%s", source);

	return blocking_notifier_chain_register(&disp_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(mtk_disp_notifier_register);

int mtk_disp_notifier_unregister(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&disp_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(mtk_disp_notifier_unregister);

int mtk_disp_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&disp_notifier_list, val, v);
}
EXPORT_SYMBOL_GPL(mtk_disp_notifier_call_chain);

int mtk_disp_sub_notifier_register(const char *source, struct notifier_block *nb)
{
	if (!source)
		return -EINVAL;

	DDPFUNC(":%s", source);

	return blocking_notifier_chain_register(&disp_sub_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(mtk_disp_sub_notifier_register);

int mtk_disp_sub_notifier_unregister(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&disp_sub_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(mtk_disp_sub_notifier_unregister);

int mtk_disp_sub_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&disp_sub_notifier_list, val, v);
}
EXPORT_SYMBOL_GPL(mtk_disp_sub_notifier_call_chain);

MODULE_AUTHOR("Freya Li <freya.li@mediatek.com>");
MODULE_DESCRIPTION("mtk disp notify");
MODULE_LICENSE("GPL v2");

