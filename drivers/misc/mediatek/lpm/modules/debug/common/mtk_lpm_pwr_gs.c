// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <gs/mtk_lpm_pwr_gs.h>

static DEFINE_SPINLOCK(mtk_lpm_gs_locker);
static struct mtk_lpm_gs_cmp *mtk_lpm_gs_cmps[MTK_LPM_GS_CMP_MAX];

int mtk_lpm_pwr_gs_compare_register(int comparer, struct mtk_lpm_gs_cmp *cmp)
{
	if ((comparer < 0) || (comparer >= MTK_LPM_GS_CMP_MAX)
	   || !cmp || mtk_lpm_gs_cmps[comparer])
		return -EINVAL;

	spin_lock(&mtk_lpm_gs_locker);
	mtk_lpm_gs_cmps[comparer] = cmp;

	if (mtk_lpm_gs_cmps[comparer]->init)
		mtk_lpm_gs_cmps[comparer]->init();
	spin_unlock(&mtk_lpm_gs_locker);

	return 0;
}

void mtk_lpm_pwr_gs_compare_unregister(int comparer)
{
	if ((comparer < 0) || (comparer >= MTK_LPM_GS_CMP_MAX))
		return;

	spin_lock(&mtk_lpm_gs_locker);
	if (mtk_lpm_gs_cmps[comparer] && mtk_lpm_gs_cmps[comparer]->deinit)
		mtk_lpm_gs_cmps[comparer]->deinit();
	mtk_lpm_gs_cmps[comparer] = NULL;
	spin_unlock(&mtk_lpm_gs_locker);
}

int mtk_lpm_pwr_gs_compare(int comparer, int user)
{
	int ret = 0;

	if ((comparer < 0) || (comparer >= MTK_LPM_GS_CMP_MAX)
	    || !mtk_lpm_gs_cmps[comparer])
		return -EINVAL;

	if (mtk_lpm_gs_cmps[comparer]->cmp) {
		spin_lock(&mtk_lpm_gs_locker);
		mtk_lpm_gs_cmps[comparer]->cmp(user);
		spin_unlock(&mtk_lpm_gs_locker);
	} else
		ret = -EACCES;

	return ret;
}

int mtk_lpm_pwr_gs_compare_by_type(int comparer, int user,
						      unsigned int type)
{
	int ret = 0;

	if ((type < 0) || (type >= MTK_LPM_GS_CMP_MAX)
	    || !mtk_lpm_gs_cmps[comparer])
		return -EINVAL;

	spin_lock(&mtk_lpm_gs_locker);
	if (mtk_lpm_gs_cmps[comparer]->cmp_by_type)
		mtk_lpm_gs_cmps[comparer]->cmp_by_type(user, type);
	else if (mtk_lpm_gs_cmps[comparer]->cmp)
		mtk_lpm_gs_cmps[comparer]->cmp(user);
	else
		ret = -EACCES;
	spin_unlock(&mtk_lpm_gs_locker);
	return ret;
}

int mtk_lpm_pwr_gs_compare_init(int comparer, void *info)
{
	int ret = 0;

	if ((comparer < 0) || (comparer >= MTK_LPM_GS_CMP_MAX)
	    || !mtk_lpm_gs_cmps[comparer])
		return -EINVAL;

	if (mtk_lpm_gs_cmps[comparer]->cmp_init) {
		spin_lock(&mtk_lpm_gs_locker);
		mtk_lpm_gs_cmps[comparer]->cmp_init(info);
		spin_unlock(&mtk_lpm_gs_locker);
	} else
		ret = -EACCES;

	return ret;
}

