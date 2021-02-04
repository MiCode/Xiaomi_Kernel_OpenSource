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

static bool _valid_ad(struct apu_dev *ad)
{
	if (IS_ERR(ad) || !ad->df ||
	    !ad->df->profile || !ad->df->profile->max_state) {
		apower_warn(ad->dev, "%ps: Invalid input.\n",
			   __builtin_return_address(0));
		return false;
	}
	return true;
}
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
		[APUIOMMU] = "APUIOMMU",
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
		apower_warn(ad->dev, "%s: device %s already exist.\n",
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
 * @ad: apu_dev
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
int apu_boost2opp(struct apu_dev *ad, int boost)
{
	u32 rdown;
	u32 max_st;
	u32 opp;

	if (!_valid_ad(ad))
		return -EINVAL;

	/* minus 1 for opp inex starts from 0 */
	max_st = ad->df->profile->max_state - 1;
	rdown = DIV_ROUND_DOWN_ULL(boost, ad->opp_div);
	opp = (rdown > max_st) ? 0 : abs(max_st - rdown);

	return opp;
}

/**
 * apu_boost2freq() - get freq from boost
 * @ad: apu_dev
 * @boost: boost value (0 ~ 100)
 *
 * frq = freq_table[opp]
 */
int apu_boost2freq(struct apu_dev *ad, int boost)
{
	int opp = 0;
	int max_st = 0;

	if (!_valid_ad(ad))
		return -EINVAL;

	opp = apu_boost2opp(ad, boost);
	/*
	 * opp 0 mean the max freq, but index 0 of freq_table
	 * is the slowest freq. So we need to swap them here.
	 */
	max_st = ad->df->profile->max_state - 1;
	return ad->df->profile->freq_table[max_st - opp];
}


/**
 * apu_opp2freq() - get freq from opp
 * @ad: apu_dev
 * @opp: opp value (0 means the fastest)
 *
 */
int apu_opp2freq(struct apu_dev *ad, int opp)
{
	int max_st = 0;

	if (!_valid_ad(ad))
		return -EINVAL;

	max_st = ad->df->profile->max_state - 1;
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
	return ad->df->profile->freq_table[max_st - opp];
}


/**
 * apu_opp2boost() - get opp from boost
 * @ad: apu_dev
 * @opp: opp value
 *
 *  opp = abs(max_state - boost/opp_div)
 *  boost = (max_state - opp + 1) * opp_div
 *
 * boost            0  ~  16 ~ 32 ~ 48 ~ 64 ~ 80 ~ 96 ~ 100
 * boost/opp_div       0     1    2    3    4    5    5
 * opp                 5     4    3    2    1    0    0
 */
int apu_opp2boost(struct apu_dev *ad, int opp)
{
	int max_st = 0;

	if (!_valid_ad(ad))
		return -EINVAL;

	max_st = ad->df->profile->max_state - 1;
	if (opp < 0)
		opp = 0;
	if (opp > max_st)
		opp = max_st;

	return (max_st - opp + 1) * ad->opp_div;
}

/**
 * apu_freq2opp() - get freq from opp
 * @ad: apu_dev
 * @freq: frequency
 *
 */
int apu_freq2opp(struct apu_dev *ad, unsigned long freq)
{
	int max_st = 0;
	int opp = 0;

	if (!_valid_ad(ad))
		return -EINVAL;

	max_st = ad->df->profile->max_state - 1;
	if (freq > ad->df->profile->freq_table[max_st])
		return -EINVAL;

	/*
	 * opp 0 mean the max freq, but index 0 of freq_table
	 * is the slowest one.
	 *
	 * So we need to take index
	 * as [max_st - opp], while getting freq we want in
	 * profile->freq_table.
	 */

	for (opp = 0; max_st >= 0; max_st--, opp++) {
		if (round_khz(ad->df->profile->freq_table[max_st], freq))
			break;
	}
	return opp;
}

/**
 * apu_freq2boost() - get freq from opp
 * @ad: apu_dev
 * @freq: frequency
 *
 */
int apu_freq2boost(struct apu_dev *ad, unsigned long freq)
{
	int max_st = 0;
	int opp = 0;

	if (!_valid_ad(ad))
		return -EINVAL;

	max_st = ad->df->profile->max_state - 1;
	if (freq > ad->df->profile->freq_table[max_st])
		return -EINVAL;

	/*
	 * opp 0 mean the max freq, but index 0 of freq_table
	 * is the slowest one.
	 *
	 * So we need to take index
	 * as [max_st - opp], while getting freq we want in
	 * profile->freq_table.
	 */

	for (opp = 0; max_st >= 0; max_st--, opp++) {
		if (round_khz(ad->df->profile->freq_table[max_st], freq))
			break;
	}
	return apu_opp2boost(ad, opp);
}

/**
 * apu_volt2opp() - get freq from opp
 * @ad: apu_dev
 * @volt: volt
 *
 */
int apu_volt2opp(struct apu_dev *ad, int volt)
{
	int max_st = 0, ret = -ERANGE;
	ulong freq = 0, tmp = 0;

	if (!_valid_ad(ad))
		return -EINVAL;

	/* search from slowest rate/volt and opp is reverse of max_state*/
	max_st = ad->df->profile->max_state - 1;

	do {
		ret = apu_get_recommend_freq_volt(ad->dev, &freq, &tmp, 0);
		if (ret)
			break;
		if (tmp != volt)
			max_st--;
		else
			goto out;
		freq++;
	} while (!ret && max_st >= 0);

	apower_err(ad->dev, "[%s] fail to find opp for %dmV.\n",
		   __func__, TOMV(volt));
	return -EINVAL;

out:
	return max_st;
}

/**
 * apu_volt2boost() - get freq from opp
 * @ad: apu_dev
 * @volt: volt
 *
 */
int apu_volt2boost(struct apu_dev *ad, int volt)
{
	int max_st = 0, ret = -ERANGE;
	ulong freq = 0, tmp = 0;

	if (!_valid_ad(ad))
		return -EINVAL;

	/* search from slowest rate/volt and opp is reverse of max_state*/
	max_st = ad->df->profile->max_state - 1;

	do {
		ret = apu_get_recommend_freq_volt(ad->dev, &freq, &tmp, 0);
		if (ret)
			break;
		if (tmp != volt)
			max_st--;
		else
			goto out;
		freq++;
	} while (!ret && max_st >= 0);

	apower_err(ad->dev, "[%s] fail to find boost for %dmV.\n",
		   __func__, TOMV(volt));
	return -EINVAL;
out:
	return apu_opp2boost(ad, max_st);
}

int apu_get_recommend_freq_volt(struct device *dev, unsigned long *freq,
				unsigned long *volt, int flag)
{
	struct dev_pm_opp *opp;

	if (!freq)
		return -EINVAL;

	/* get the slowest frq in opp */
	opp = devfreq_recommended_opp(dev, freq, flag);
	if (IS_ERR(opp)) {
		apower_err(dev, "[%s] no opp for %luMHz, ret %d\n",
			   __func__, TOMHZ(*freq), PTR_ERR(opp));
		return PTR_ERR(opp);
	}

	if (volt)
		*volt = dev_pm_opp_get_voltage(opp);
	dev_pm_opp_put(opp);
	return 0;
}
