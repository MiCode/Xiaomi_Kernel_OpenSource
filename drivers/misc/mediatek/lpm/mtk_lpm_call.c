// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/spinlock.h>

#include <mtk_lpm_call.h>

static DEFINE_SPINLOCK(mtk_lp_plat_call_locker);
static LIST_HEAD(mtk_lpm_callees);

int mtk_lpm_callee_registry(struct mtk_lpm_callee *callee)
{
	struct mtk_lpm_callee *pos;

	if (!callee)
		return -EINVAL;

	spin_lock(&mtk_lp_plat_call_locker);
	list_for_each_entry(pos, &mtk_lpm_callees, list) {
		if (pos && (pos != callee) &&
		   (pos->uid == callee->uid)) {
			pos = NULL;
			break;
		}
	}

	if (pos) {
		callee->ref = 0;
		list_add(&callee->list, &mtk_lpm_callees);
	}
	spin_unlock(&mtk_lp_plat_call_locker);

	return 0;
}
EXPORT_SYMBOL(mtk_lpm_callee_registry);

int mtk_lpm_callee_unregistry(struct mtk_lpm_callee *callee)
{
	int bRet = 0;
	struct mtk_lpm_callee *pos;

	if (!callee)
		return -EINVAL;

	spin_lock(&mtk_lp_plat_call_locker);
	list_for_each_entry(pos, &mtk_lpm_callees, list) {
		if (pos && (pos->uid == callee->uid)) {
			if (!pos->ref)
				list_del(&pos->list);
			else
				bRet = -EPERM;
			break;
		}
	}
	spin_unlock(&mtk_lp_plat_call_locker);

	return bRet;
}
EXPORT_SYMBOL(mtk_lpm_callee_unregistry);

int mtk_lpm_callee_get_impl(int uid, const struct mtk_lpm_callee **callee)
{
	struct mtk_lpm_callee *pos;

	if (!callee)
		return -EINVAL;

	*callee = NULL;
	spin_lock(&mtk_lp_plat_call_locker);
	list_for_each_entry(pos, &mtk_lpm_callees, list) {
		if (pos && pos->uid == uid) {
			pos->ref++;
			*callee = pos;
			break;
		}
	}
	spin_unlock(&mtk_lp_plat_call_locker);

	return *callee ? 0 : -EINVAL;
}
EXPORT_SYMBOL(mtk_lpm_callee_get_impl);

int mtk_lpm_callee_put_impl(struct mtk_lpm_callee const *callee)
{
	struct mtk_lpm_callee *pos;
	int ret = -EPERM;

	if (!callee)
		return -EINVAL;

	spin_lock(&mtk_lp_plat_call_locker);
	list_for_each_entry(pos, &mtk_lpm_callees, list) {
		if (pos && (pos->uid == callee->uid)) {
			pos->ref--;
			ret = 0;
			break;
		}
	}
	spin_unlock(&mtk_lp_plat_call_locker);

	return ret;
}
EXPORT_SYMBOL(mtk_lpm_callee_put_impl);

