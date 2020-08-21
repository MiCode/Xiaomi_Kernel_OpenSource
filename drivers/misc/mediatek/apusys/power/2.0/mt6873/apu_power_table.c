// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/devfreq.h>
#include <linux/devfreq_cooling.h>

#include "apusys_power_ctl.h"
#include "apusys_power.h"
#include "apu_power_api.h"
#include "apu_log.h"

#include "apu_power_table.h"

// FIXME: update vpu power table in DVT stage
/* opp, mW */
struct apu_opp_info vpu_power_table[APU_OPP_NUM] = {
	{APU_OPP_0, 226},
	{APU_OPP_1, 226},
	{APU_OPP_2, 176},
	{APU_OPP_3, 133},
	{APU_OPP_4, 98},
	{APU_OPP_5, 44},
};
EXPORT_SYMBOL(vpu_power_table);

// FIXME: update mdla power table in DVT stage
/* opp, mW */
struct apu_opp_info mdla_power_table[APU_OPP_NUM] = {
	{APU_OPP_0, 161},
	{APU_OPP_1, 161},
	{APU_OPP_2, 161},
	{APU_OPP_3, 120},
	{APU_OPP_4, 86},
	{APU_OPP_5, 44},
};
EXPORT_SYMBOL(mdla_power_table);

#if APUSYS_DEVFREQ_COOLING

#define VPU_CORES	(2)
#define MDLA_CORES	(1)

#define LOCAL_DBG                       (0)
#define DEVFREQ_POOLING_INTERVAL        (1000) // ms

struct apu_pwr_devfreq_st {
	struct devfreq *apu_devfreq;
	struct devfreq_dev_profile profile;
	struct devfreq_cooling_power cooling_power_ops;
	struct thermal_cooling_device *apu_devfreq_cooling;
};

static struct apu_pwr_devfreq_st apu_pwr_devfreq_arr[APUSYS_DVFS_USER_NUM];

int devfreq_set_target(struct device *dev, unsigned long *freq, u32 flags)
{
	struct dev_pm_opp *opp;
	uint8_t normalize_opp = 0;
	unsigned long normalize_freq = 0;
	unsigned long ori_freq;
	uint8_t dvfs_user;
	struct apu_pwr_devfreq_st *apu_pwr_devfreq_ptr;

	dvfs_user = *((uint8_t *)dev_get_drvdata(dev));
	if (dvfs_user < 0 || dvfs_user >= APUSYS_DVFS_USER_NUM)
		return -1;

	apu_pwr_devfreq_ptr = &apu_pwr_devfreq_arr[dvfs_user];

	if (!freq) {
		LOG_ERR("%s freq ptr is NULL\n", __func__);
		return -1;
	}

	/* The freq is an upper bound. opp should be lower */
	flags |= DEVFREQ_FLAG_LEAST_UPPER_BOUND;
	ori_freq = *freq;

	/*
	 * due to pm opp is not allow duplicated freq,
	 * so it is different with our freq table : pm_opp : 9 vs ours : 10
	 * we need to handle opp remap task as well.
	 * devfreq_recommended_opp will help to limit opp range (limit by therm)
	 */
	opp = devfreq_recommended_opp(dev, freq, flags);
	*freq = dev_pm_opp_get_freq(opp);
	normalize_freq = *freq / 1000;
	normalize_opp = apusys_freq_to_opp(
				apusys_user_to_buck_domain[dvfs_user],
				normalize_freq);
	dev_pm_opp_put(opp);

#if LOCAL_DBG
	pr_info("%s target:%lu limit:%lu (opp:%u)\n",
					__func__,
					ori_freq / 1000000,
					normalize_freq / 1000,
					normalize_opp);
#endif

	if (*freq == apu_pwr_devfreq_ptr->apu_devfreq->previous_freq) {
#if LOCAL_DBG
		pr_info("%s the same freq : %lu\n",
			__func__, normalize_freq / 1000);
#endif
		return 0;
	}

	/*
	 * apusys_thermal_en_throttle_cb only distinguish VPU0 or MDLA0
	 * and apply corresponding thermal_opp to related cores.
	 */
	if (dvfs_user <= VPU1)
		dvfs_user = VPU0;
	else
		dvfs_user = MDLA0;

	if (normalize_opp == 0)
		apusys_thermal_dis_throttle_cb(dvfs_user);
	else
		apusys_thermal_en_throttle_cb(dvfs_user, normalize_opp);

	return 0;
}

int devfreq_get_status(struct device *dev, struct devfreq_dev_status *stat)
{
	uint8_t dvfs_user;
	struct apu_pwr_devfreq_st *apu_pwr_devfreq_ptr;

	dvfs_user = *((uint8_t *)dev_get_drvdata(dev));
	if (dvfs_user < 0 || dvfs_user >= APUSYS_DVFS_USER_NUM)
		return -1;

	apu_pwr_devfreq_ptr = &apu_pwr_devfreq_arr[dvfs_user];

	if (!stat)
		return -1;

	// total_time assign to 0 could force simple_ondemand return max freq
	stat->total_time = 0;
	stat->busy_time = 0;
	stat->current_frequency =
		apu_pwr_devfreq_ptr->apu_devfreq->previous_freq;

#if LOCAL_DBG
	pr_info("%s stat->current_frequency:%lu\n",
				__func__, stat->current_frequency / 1000000);
#endif
	return 0;
}

int devfreq_get_cur_freq(struct device *dev, unsigned long *freq)
{
	uint8_t dvfs_user;
	struct apu_pwr_devfreq_st *apu_pwr_devfreq_ptr;

	dvfs_user = *((uint8_t *)dev_get_drvdata(dev));
	if (dvfs_user < 0 || dvfs_user >= APUSYS_DVFS_USER_NUM)
		return -1;

	apu_pwr_devfreq_ptr = &apu_pwr_devfreq_arr[dvfs_user];

	if (!freq)
		return -1;

	*freq = apu_pwr_devfreq_ptr->apu_devfreq->previous_freq;

#if LOCAL_DBG
	pr_info("%s: %lu max:%lu min:%lu\n",
			__func__, *freq / 1000000,
			apu_pwr_devfreq_ptr->apu_devfreq->max_freq / 1000000,
			apu_pwr_devfreq_ptr->apu_devfreq->min_freq / 1000000);
#endif
	return 0;
}

static unsigned long apusys_static_power(struct devfreq *devfreq,
					unsigned long voltage_mv)
{
#if LOCAL_DBG
	// FIXME: need static power ?
	pr_info("%s volt=%lu\n", __func__, voltage_mv);
#endif
	return 0;
}

static unsigned long apusys_dynamic_power(struct devfreq *devfreq,
			unsigned long freqHz, unsigned long voltage_mv)
{
	int opp;
	unsigned long power;
	unsigned long normalize_freq = freqHz / 1000;
	uint8_t dvfs_user;

	dvfs_user = *((uint8_t *)dev_get_drvdata(devfreq->dev.parent));

#if LOCAL_DBG
	pr_info("%s usr:%d, 0x%x\n", __func__, dvfs_user,
			dev_get_drvdata(devfreq->dev.parent));
#endif

	if (dvfs_user < 0 || dvfs_user >= APUSYS_DVFS_USER_NUM)
		return -1;

	opp = apusys_freq_to_opp(
		apusys_user_to_buck_domain[dvfs_user], normalize_freq);

	// need to div core number since power table is total power cross cores
	if (dvfs_user >= VPU0 && dvfs_user <= VPU1)
		power = ((unsigned int)vpu_power_table[
				(enum APU_OPP_INDEX)opp].power) / VPU_CORES;
	else
		power = ((unsigned int)mdla_power_table[
				(enum APU_OPP_INDEX)opp].power) / MDLA_CORES;
#if LOCAL_DBG
	pr_info("%s freq=%lu, volt=%lu, power=%lu\n",
			__func__, freqHz, voltage_mv, power);
#endif
	return power;
}

static int apusys_real_power(struct devfreq *devfreq, unsigned int *power,
			unsigned long freqHz, unsigned long voltage_mv)
{
	int opp;
	unsigned long normalize_freq = freqHz / 1000;
	uint8_t dvfs_user;

	if (!power)
		return -1;

	dvfs_user = *((uint8_t *)dev_get_drvdata(devfreq->dev.parent));
	if (dvfs_user < 0 || dvfs_user >= APUSYS_DVFS_USER_NUM)
		return -1;

	opp = apusys_freq_to_opp(
		apusys_user_to_buck_domain[dvfs_user], normalize_freq);

	// need to div core number since power table is total power cross cores
	if (dvfs_user >= VPU0 && dvfs_user <= VPU1)
		*power = ((unsigned int)vpu_power_table[
				(enum APU_OPP_INDEX)opp].power) / VPU_CORES;
	else
		*power = ((unsigned int)mdla_power_table[
				(enum APU_OPP_INDEX)opp].power) / MDLA_CORES;

#if LOCAL_DBG
	pr_info("%s freq=%lu, volt=%lu, power=%ld, opp=%d\n",
				__func__, freqHz, voltage_mv, *power, opp);
#endif
	return 0;
}

static int opp_tbl_to_freq_tbl(struct device *dev, enum DVFS_USER user)
{
	enum DVFS_VOLTAGE_DOMAIN buck_domain;
	int opp_num, ret = 0;
	unsigned long rate, volt;

	buck_domain = apusys_user_to_buck_domain[user];

	LOG_INF("%s buck_domain:%d\n", __func__, buck_domain);
	for (opp_num = 0 ; opp_num < APUSYS_MAX_NUM_OPPS ; opp_num++) {
		rate = apusys_opps.opps[opp_num][buck_domain].freq * 1000;
		volt = apusys_opps.opps[opp_num][buck_domain].voltage;
		ret = dev_pm_opp_add(dev, rate, volt);
#if LOCAL_DBG
		pr_info("%s, dev_pm_opp_adding opp[%d] = (%lu,%lu) ret %d\n",
				__func__, opp_num, rate, volt, ret);
#endif
		/* add opp fail and break */
		if (ret)
			break;
	}
out:
	return ret;
}

int register_devfreq_cooling(struct platform_device *pdev, enum DVFS_USER user)
{
	struct device *dev = &pdev->dev;
	struct device_node *of_node;
	int ret = 0;
	struct dev_pm_opp *opp;
	unsigned long rate;
	uint8_t *drvdata_user;
	struct apu_pwr_devfreq_st *apu_pwr_devfreq_ptr;

	/*
	 * only VPU cores and MDLA cores need to be cooling
	 */
	if (user < 0 || user >= APUSYS_DVFS_USER_NUM)
		return -1;

	apu_pwr_devfreq_ptr = &apu_pwr_devfreq_arr[user];

	if (!apu_pwr_devfreq_ptr) {
		LOG_ERR("%s, apu_pwr_devfreq_ptr is NULL\n", __func__);
		return -1;
	}

	// use driver_data of dev to record device corresponding DVFS_USER num
	drvdata_user = kzalloc(sizeof(uint8_t), GFP_KERNEL);
	*drvdata_user = user;
	dev_set_drvdata(dev, drvdata_user);

	ret = opp_tbl_to_freq_tbl(dev, user);
	if (ret)
		return ret;

	apu_pwr_devfreq_ptr->profile.initial_freq = 0;
	apu_pwr_devfreq_ptr->profile.polling_ms = DEVFREQ_POOLING_INTERVAL;
	apu_pwr_devfreq_ptr->profile.target = devfreq_set_target;
	apu_pwr_devfreq_ptr->profile.get_dev_status = devfreq_get_status;
	apu_pwr_devfreq_ptr->profile.get_cur_freq = devfreq_get_cur_freq;
	apu_pwr_devfreq_ptr->cooling_power_ops.get_static_power =
						apusys_static_power;
	apu_pwr_devfreq_ptr->cooling_power_ops.get_dynamic_power =
						apusys_dynamic_power;
	apu_pwr_devfreq_ptr->cooling_power_ops.get_real_power =
						apusys_real_power;

	apu_pwr_devfreq_ptr->apu_devfreq = devm_devfreq_add_device(
			dev,
			&apu_pwr_devfreq_ptr->profile,
			"simple_ondemand",
			NULL);

	if (IS_ERR(apu_pwr_devfreq_ptr->apu_devfreq)) {
		LOG_ERR("%s error in devm_devfreq_add_device\n", __func__);
		return PTR_ERR(apu_pwr_devfreq_ptr->apu_devfreq);
	}

	/* set devfreq's min freq */
	rate = 0;
	opp = dev_pm_opp_find_freq_ceil(dev, &rate);
	if (IS_ERR(opp)) {
		ret = PTR_ERR(opp);
		goto remove_devfreq;
	}
	ret = dev_pm_qos_update_request(
		&apu_pwr_devfreq_ptr->apu_devfreq->user_min_freq_req,
		rate);
	if (ret < 0)
		goto remove_devfreq;

	dev_pm_opp_put(opp);

	/* set devfreq's max freq */
	rate = ULONG_MAX;
	opp = dev_pm_opp_find_freq_floor(dev, &rate);
	if (IS_ERR(opp)) {
		ret = PTR_ERR(opp);
		goto remove_devfreq;
	}
	ret = dev_pm_qos_update_request(
		&apu_pwr_devfreq_ptr->apu_devfreq->user_max_freq_req,
		rate);
	if (ret < 0)
		goto remove_devfreq;
	dev_pm_opp_put(opp);

	LOG_INF("%s for usr:%d %d, 0x%x\n", __func__, user,
			*((uint8_t *)dev_get_drvdata(
				apu_pwr_devfreq_ptr->apu_devfreq->dev.parent)),
			dev_get_drvdata(
				apu_pwr_devfreq_ptr->apu_devfreq->dev.parent));

	of_node = of_node_get(dev->of_node);
	apu_pwr_devfreq_ptr->apu_devfreq_cooling =
		of_devfreq_cooling_register_power(
				of_node,
				apu_pwr_devfreq_ptr->apu_devfreq,
				&apu_pwr_devfreq_ptr->cooling_power_ops);
	of_node_put(of_node);

	if (IS_ERR(apu_pwr_devfreq_ptr->apu_devfreq_cooling)) {
		LOG_ERR("%s error in of_devfreq_cooling_register_power\n",
				__func__);
		ret = PTR_ERR(apu_pwr_devfreq_ptr->apu_devfreq_cooling);
		goto remove_devfreq;
	}

	return ret;

remove_devfreq:
	devm_devfreq_remove_device(dev, apu_pwr_devfreq_ptr->apu_devfreq);
	return ret;
}

void unregister_devfreq_cooling(enum DVFS_USER user)
{
	uint8_t *drvdata_user;
	struct apu_pwr_devfreq_st *apu_pwr_devfreq_ptr;

	if (user < 0 || user >= APUSYS_DVFS_USER_NUM)
		return;

	LOG_INF("%s for usr:%d\n", __func__, user);

	apu_pwr_devfreq_ptr = &apu_pwr_devfreq_arr[user];
	if (!apu_pwr_devfreq_ptr) {
		LOG_ERR("%s, apu_pwr_devfreq_ptr is NULL\n", __func__);
		return;
	}

	if (apu_pwr_devfreq_ptr->apu_devfreq_cooling)
		devfreq_cooling_unregister(
			apu_pwr_devfreq_ptr->apu_devfreq_cooling);

	if (apu_pwr_devfreq_ptr->apu_devfreq) {
		drvdata_user = (uint8_t *)dev_get_drvdata(
				apu_pwr_devfreq_ptr->apu_devfreq->dev.parent);
		if (*drvdata_user >= 0 && *drvdata_user < APUSYS_DVFS_USER_NUM)
			kfree(drvdata_user);

		devm_devfreq_remove_device(
			apu_pwr_devfreq_ptr->apu_devfreq->dev.parent,
			apu_pwr_devfreq_ptr->apu_devfreq);
	}
}

void start_monitor_devfreq_cooling(enum DVFS_USER user)
{
	struct apu_pwr_devfreq_st *apu_pwr_devfreq_ptr;

	if (user < 0 || user >= APUSYS_DVFS_USER_NUM)
		return;

	LOG_INF("%s for usr:%d\n", __func__, user);
	apu_pwr_devfreq_ptr = &apu_pwr_devfreq_arr[user];
	if (!apu_pwr_devfreq_ptr) {
		LOG_ERR("%s, apu_pwr_devfreq_ptr is NULL\n", __func__);
		return;
	}

	devfreq_resume_device(apu_pwr_devfreq_ptr->apu_devfreq);
}

void stop_monitor_devfreq_cooling(enum DVFS_USER user)
{
	struct apu_pwr_devfreq_st *apu_pwr_devfreq_ptr;

	if (user < 0 || user >= APUSYS_DVFS_USER_NUM)
		return;

	LOG_INF("%s for usr:%d\n", __func__, user);
	apu_pwr_devfreq_ptr = &apu_pwr_devfreq_arr[user];
	if (!apu_pwr_devfreq_ptr) {
		LOG_ERR("%s, apu_pwr_devfreq_ptr is NULL\n", __func__);
		return;
	}

	devfreq_suspend_device(apu_pwr_devfreq_ptr->apu_devfreq);
}
#endif
