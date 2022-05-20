// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <gs/lpm_pwr_gs.h>
#include <gs/v1/lpm_power_gs.h>

static DEFINE_SPINLOCK(lpm_gs_locker);
static struct lpm_gs_cmp *lpm_gs_cmps[LPM_GS_CMP_MAX];

int lpm_pwr_gs_compare_register(int comparer, struct lpm_gs_cmp *cmp)
{
	if ((comparer < 0) || (comparer >= LPM_GS_CMP_MAX)
	   || !cmp || lpm_gs_cmps[comparer])
		return -EINVAL;

	spin_lock(&lpm_gs_locker);

	lpm_gs_cmps[comparer] = cmp;
	if (lpm_gs_cmps[comparer]->init)
		lpm_gs_cmps[comparer]->init();

	spin_unlock(&lpm_gs_locker);
	return 0;
}

void lpm_pwr_gs_compare_unregister(int comparer)
{
	if ((comparer < 0) || (comparer >= LPM_GS_CMP_MAX))
		return;

	spin_lock(&lpm_gs_locker);

	if (lpm_gs_cmps[comparer] && lpm_gs_cmps[comparer]->deinit)
		lpm_gs_cmps[comparer]->deinit();
	lpm_gs_cmps[comparer] = NULL;

	spin_unlock(&lpm_gs_locker);
}

int lpm_pwr_gs_compare(int comparer, int user)
{
	int ret = 0;

	if ((comparer < 0) || (comparer >= LPM_GS_CMP_MAX)
	    || !lpm_gs_cmps[comparer])
		return -EINVAL;

	if (lpm_gs_cmps[comparer]->cmp) {
		spin_lock(&lpm_gs_locker);
		lpm_gs_cmps[comparer]->cmp(user);
		spin_unlock(&lpm_gs_locker);
	} else
		ret = -EACCES;

	return ret;
}
EXPORT_SYMBOL(lpm_pwr_gs_compare);

int lpm_pwr_gs_compare_by_type(int comparer, int user,
						      unsigned int type)
{
	int ret = 0;

	if ((comparer < 0) || (comparer >= LPM_GS_CMP_MAX)
	    || !lpm_gs_cmps[comparer])
		return -EINVAL;

	if ((user < 0) || (user >= LPM_PWR_GS_TYPE_MAX))
		return -EINVAL;

	spin_lock(&lpm_gs_locker);

	if (lpm_gs_cmps[comparer]->cmp_by_type)
		lpm_gs_cmps[comparer]->cmp_by_type(user, type);
	else if (lpm_gs_cmps[comparer]->cmp)
		lpm_gs_cmps[comparer]->cmp(user);
	else
		ret = -EACCES;

	spin_unlock(&lpm_gs_locker);
	return ret;
}
EXPORT_SYMBOL(lpm_pwr_gs_compare_by_type);

int lpm_pwr_gs_compare_init(int comparer, void *info)
{
	int ret = 0;

	if ((comparer < 0) || (comparer >= LPM_GS_CMP_MAX)
	    || !lpm_gs_cmps[comparer])
		return -EINVAL;
	if (lpm_gs_cmps[comparer]->cmp_init) {
		spin_lock(&lpm_gs_locker);
		lpm_gs_cmps[comparer]->cmp_init(info);
		spin_unlock(&lpm_gs_locker);
	} else
		ret = -EACCES;
	return ret;
}
EXPORT_SYMBOL(lpm_pwr_gs_compare_init);
