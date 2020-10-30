// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/spinlock.h>

#include <lpm_call.h>

static DEFINE_SPINLOCK(lpm_plat_call_locker);
static LIST_HEAD(lpm_callees);

int lpm_callee_registry(struct lpm_callee *callee)
{
	struct lpm_callee *pos;

	if (!callee)
		return -EINVAL;

	spin_lock(&lpm_plat_call_locker);
	list_for_each_entry(pos, &lpm_callees, list) {
		if (pos && (pos != callee) &&
		   (pos->uid == callee->uid)) {
			pos = NULL;
			break;
		}
	}

	if (pos) {
		callee->ref = 0;
		list_add(&callee->list, &lpm_callees);
	}
	spin_unlock(&lpm_plat_call_locker);

	return 0;
}
EXPORT_SYMBOL(lpm_callee_registry);

int lpm_callee_unregistry(struct lpm_callee *callee)
{
	int bRet = 0;
	struct lpm_callee *pos;

	if (!callee)
		return -EINVAL;

	spin_lock(&lpm_plat_call_locker);
	list_for_each_entry(pos, &lpm_callees, list) {
		if (pos && (pos->uid == callee->uid)) {
			if (!pos->ref)
				list_del(&pos->list);
			else
				bRet = -EPERM;
			break;
		}
	}
	spin_unlock(&lpm_plat_call_locker);

	return bRet;
}
EXPORT_SYMBOL(lpm_callee_unregistry);

int lpm_callee_get_impl(int uid, const struct lpm_callee **callee)
{
	struct lpm_callee *pos;

	if (!callee)
		return -EINVAL;

	*callee = NULL;
	spin_lock(&lpm_plat_call_locker);
	list_for_each_entry(pos, &lpm_callees, list) {
		if (pos && pos->uid == uid) {
			pos->ref++;
			*callee = pos;
			break;
		}
	}
	spin_unlock(&lpm_plat_call_locker);

	return *callee ? 0 : -EINVAL;
}
EXPORT_SYMBOL(lpm_callee_get_impl);

int lpm_callee_put_impl(struct lpm_callee const *callee)
{
	struct lpm_callee *pos;
	int ret = -EPERM;

	if (!callee)
		return -EINVAL;

	spin_lock(&lpm_plat_call_locker);
	list_for_each_entry(pos, &lpm_callees, list) {
		if (pos && (pos->uid == callee->uid)) {
			pos->ref--;
			ret = 0;
			break;
		}
	}
	spin_unlock(&lpm_plat_call_locker);

	return ret;
}
EXPORT_SYMBOL(lpm_callee_put_impl);

