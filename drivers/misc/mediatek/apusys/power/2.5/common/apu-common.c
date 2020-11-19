// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/slab.h>

#include "apusys_power_user.h"
#include "apu_devfreq.h"
#include "apu_common.h"
#include "apu_clk.h"
#include "apu_log.h"

static DEFINE_SPINLOCK(adev_list_lock);

/* The list of all apu devices */
static LIST_HEAD(adev_list);

/**
 * apu_dev_string() - return string of dvfs user
 * @user:	apu dvfs user.
 *
 * Return the name string of dvfs user
 */
const char *apu_dev_string(enum DVFS_USER user)
{
	static const char *const names[] = {
		[VPU] = "APUVPU",
		[VPU0] = "APUVPU0",
		[VPU1] = "APUVPU1",
		[VPU2] = "APUVPU2",
		[MDLA] = "APUMDLA",
		[MDLA0] = "APUMDLA0",
		[MDLA1] = "APUMDLA1",
		[APUCONN] = "APUCONN",
		[APUCORE] = "APUCORE",
		[APUCB] = "APUCB",
		[EDMA] = "APUEDMA",
		[EDMA2] = "APUEDMA2",
		[REVISER] = "APUREVISER",
		[APUMNOC] = "APUMNOC",
	};

	if (user < 0 || user >= ARRAY_SIZE(names))
		return NULL;

	return names[user];
}

/**
 * apu_find_device() - find apu_dev struct using enum dvfs_user
 * @user:	apu dvfs user.
 *
 * Search the list of device adev_list and return the matched device's
 * apu_dev info.
 */
struct apu_dev *apu_find_device(enum DVFS_USER user)
{
	struct apu_dev *ret_dev = NULL;

	if (user > APUSYS_POWER_USER_NUM) {
		pr_info("%s: user %d Invalid parameters\n",
			   __func__, user);
		return ERR_PTR(-EINVAL);
	}

	spin_lock(&adev_list_lock);
	list_for_each_entry(ret_dev, &adev_list, node) {
		if (ret_dev->user == user) {
			spin_unlock(&adev_list_lock);
			return ret_dev; /* Got it */
		}
	}
	spin_unlock(&adev_list_lock);
	return ERR_PTR(-ENODEV);
}

/**
 * apu_dev_user() - return dvfs user
 * @name: the name of apu_device
 *
 * Return the dvfs user of given name
 */
enum DVFS_USER apu_dev_user(const char *name)
{
	int idx = 0;
	static const char *const names[] = {
		[VPU] = "APUVPU",
		[VPU0] = "APUVPU0",
		[VPU1] = "APUVPU1",
		[VPU2] = "APUVPU2",
		[MDLA] = "APUMDLA",
		[MDLA0] = "APUMDLA0",
		[MDLA1] = "APUMDLA1",
		[APUCB] = "APUCB",
		[APUCONN] = "APUCONN",
		[APUCORE] = "APUCORE",
	};

	for (idx = 0; idx < ARRAY_SIZE(names); idx++)
		if (names[idx] && !strcmp(name, names[idx]))
			return idx;

	return APUSYS_POWER_USER_NUM;
}

/**
 * apu_add_device() - Add apu_dev
 * @add_dev: the apu_dev for adding.
 * @user:	 the dvfs user
 *
 * link dvfs user's dev with adev_list
 */
int apu_add_devfreq(struct apu_dev *ad)
{

	if (!ad || ad->user < 0 || ad->user > APUSYS_POWER_USER_NUM) {
		apower_err(ad->dev, "%s: Invalid parameters.\n", __func__);
		return -EINVAL;
	}

	if (!IS_ERR(apu_find_device(ad->user))) {
		apower_err(ad->dev, "%s: device %s already exist.\n",
				__func__, apu_dev_string(ad->user));
		goto out;
	}

	spin_lock(&adev_list_lock);
	list_add(&ad->node, &adev_list);
	spin_unlock(&adev_list_lock);
out:
	return 0;
}


/**
 * apu_del_device() - Add apu_dev
 * @add_dev: the apu_dev for deleting.
 *
 * link dvfs user's dev with adev_list
 */
int apu_del_devfreq(struct apu_dev *del_dev)
{

	if (IS_ERR(del_dev)) {
		pr_info("%s: Invalid parameters.\n", __func__);
		return -EINVAL;
	}

	spin_lock(&adev_list_lock);
	list_del(&del_dev->node);
	spin_unlock(&adev_list_lock);

	return 0;
}

/**
 * apu_boost2opp() - get opp from boost
 * @adev: apu_dev
 * @boost: boost value (0 ~ 100)
 *
 *  opp = abs(max_state - boost/opp_div)
 *  for example:
 *      opps is from 0 ~ 5 (total 6)
 *      100/6 = 16 (each increase interval)
 *
 * boost            0  ~  16 ~ 32 ~ 48 ~ 64 ~ 80 ~ 96 ~ 100
 * boost/opp_div       0     1    2    3    4    5    5
 * opp                 5     4    3    2    1    0    0
 */
int apu_boost2opp(struct apu_dev *adev, int boost)
{
	u32 rdown;
	u32 max_state;
	u32 opp;

	if (IS_ERR(adev)) {
		pr_info("%s: Invalid adev parameters.\n", __func__);
		return -EINVAL;
	}

	/* minus 1 for opp inex starts from 0 */
	max_state = adev->devfreq->profile->max_state - 1;
	rdown = DIV_ROUND_DOWN_ULL(boost, adev->opp_div);
	opp = (rdown > max_state) ? 0 : abs(max_state - rdown);

	dev_dbg(adev->devfreq->dev.parent,
		 "opp %d, rdown %d, boost %d\n",
		 opp, rdown, boost);

	return opp;
}

/**
 * apu_boost2freq() - get freq from boost
 * @adev: apu_dev
 * @boost: boost value (0 ~ 100)
 *
 * frq = freq_table[opp]
 */
unsigned long apu_boost2freq(struct apu_dev *ad, int boost)
{
	int opp = 0;
	int max_st = 0;

	if (IS_ERR(ad)) {
		apower_err(ad->dev, "%s: Invalid parameters.\n", __func__);
		return -EINVAL;
	}

	opp = apu_boost2opp(ad, boost);
	/*
	 * opp 0 mean the max freq, but index 0 of freq_table
	 * is the slowest freq. So we need to swap them here.
	 */
	max_st = ad->devfreq->profile->max_state - 1;
	return ad->devfreq->profile->freq_table[max_st - opp];
}


/**
 * apu_opp2freq() - get freq from opp
 * @adev: apu_dev
 * @opp: opp value (0 means the fastest)
 *
 */
int apu_opp2freq(struct apu_dev *ad, int opp)
{
	int max_st = 0;

	if (IS_ERR(ad)) {
		apower_err(ad->dev, "[%s] Invalid adev parameters.\n",
			   __func__);
		return -EINVAL;
	}

	max_st = ad->devfreq->profile->max_state - 1;
	if (opp < 0)
		opp = 0;
	if (opp > max_st)
		opp = max_st;

	/*
	 * opp 0 mean the max freq, but index 0 of freq_table
	 * is the slowest one.
	 *
	 * So we need to take index
	 * as [max_st - opp], while getting freq we want in
	 * profile->freq_table.
	 */
	return ad->devfreq->profile->freq_table[max_st - opp];
}


/**
 * apu_freq2opp() - get freq from opp
 * @adev: apu_dev
 * @freq: frequency
 *
 */
int apu_freq2opp(struct apu_dev *adev, unsigned long freq)
{
	int max_st = 0;
	int opp = 0;

	if (IS_ERR(adev)) {
		pr_info("%s: Invalid parameters.\n", __func__);
		return -EINVAL;
	}

	max_st = adev->devfreq->profile->max_state - 1;
	if (freq > adev->devfreq->profile->freq_table[max_st - opp])
		return -EINVAL;

	/*
	 * opp 0 mean the max freq, but index 0 of freq_table
	 * is the slowest one.
	 *
	 * So we need to take index
	 * as [max_st - opp], while getting freq we want in
	 * profile->freq_table.
	 */

	for (opp = max_st; opp >= 0; opp--)
		if (round_khz(adev->devfreq->profile->freq_table[opp], freq))
			break;

	return opp;
}

/**
 * apu_create_child_freq_list() - set child freq list
 * @pgov_data: governor data
 *
 * Based on dev's child count, create governor->child_freq list
 */
int apu_create_child_array(struct apu_gov_data *pgov_data)
{
	int i, err = 0;
	u32 *tmp = NULL;
	struct device *dev = NULL;
	struct apu_dev *ad = NULL;

	dev = pgov_data->this->dev.parent;
	ad = dev_get_drvdata(dev);

	/* create array of child_freq */
	pgov_data->child_opp =
		kcalloc(APUSYS_POWER_USER_NUM, sizeof(u32 *), GFP_KERNEL);
	if (!pgov_data->child_opp) {
		apower_err(dev, "cannot allocate child_freq\n");
		err = -ENOMEM;
		goto out;
	}

	/* initialize child freq element */
	for (i = 0; i < APUSYS_POWER_USER_NUM; i++) {
		tmp = kmalloc(sizeof(*tmp), GFP_KERNEL);
		if (!tmp) {
			err = -ENOMEM;
			goto out;
		}
		/* set each child's value as slowest opp of parent. */
		*tmp = pgov_data->this->profile->max_state - 1;
		pgov_data->child_opp[i] = tmp;
	}

out:
	return err;
}

/**
 * apu_release_child_freq() - release child freq
 * @dev: struct device, used for checking child number
 * @user: dvfs user
 * @pgov_data: governor data
 *
 * Based on dev's child count, create governor->child_freq list
 */
void apu_release_child_array(struct apu_gov_data *pgov_data)
{
	int i = 0;
	struct device *dev = NULL;
	struct apu_dev *adev = NULL;

	dev = pgov_data->this->dev.parent;
	adev = dev_get_drvdata(dev);
	for (i = 0; i < adev->user; i++)
		kfree(pgov_data->child_opp[i]);
	kfree(pgov_data->child_opp);
}

