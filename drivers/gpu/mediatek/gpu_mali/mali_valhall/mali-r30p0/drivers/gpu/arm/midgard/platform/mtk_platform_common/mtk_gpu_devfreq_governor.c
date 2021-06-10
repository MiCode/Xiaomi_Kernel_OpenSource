// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <mali_kbase.h>
#include <platform/mtk_platform_common.h>
#include "governor.h"
#include "mtk_gpu_devfreq_governor.h"
#include <mtk_gpufreq.h>

static int mtk_common_gov_get_target_freq(struct devfreq *df,
                                          unsigned long *freq)
{
	/*
	 * target callback should be able to get floor value as
	 * said in devfreq.h
	 */
	int err;

	err = devfreq_update_stats(df);
	if (err)
		return err;

	*freq = (!df->scaling_max_freq) ? UINT_MAX : df->scaling_max_freq;

	return 0;
}

static int mtk_common_gov_event_handler(struct devfreq *devfreq,
                                        unsigned int event, void *data)
{
	switch (event) {
	case DEVFREQ_GOV_START:
		devfreq_monitor_start(devfreq);
		break;

	case DEVFREQ_GOV_STOP:
		devfreq_monitor_stop(devfreq);
		break;

	case DEVFREQ_GOV_UPDATE_INTERVAL:
		devfreq_update_interval(devfreq, (unsigned int *)data);
		break;

	case DEVFREQ_GOV_SUSPEND:
		devfreq_monitor_suspend(devfreq);
		break;

	case DEVFREQ_GOV_RESUME:
		devfreq_monitor_resume(devfreq);
		break;

	default:
		break;
	}

	return 0;
}

struct devfreq_governor mtk_common_gov_dummy = {
	.name = MTK_GPU_DEVFREQ_GOV_DUMMY,
	.get_target_freq = mtk_common_gov_get_target_freq,
	.event_handler = mtk_common_gov_event_handler,
};

static int mtk_common_devfreq_target(struct device *dev,
                                     unsigned long *freq, u32 flags)
{
	int opp_idx = 0;
	unsigned int pow = 0;
	unsigned long freq_khz = 0;
	static int resume;
	struct kbase_device *kbdev = dev_get_drvdata(dev);

	/* Now only thermal throttle will limit opps in devfreq
	 * So the request opp freq would reflect the valid pow
	 * Look up that power as throttle limit
	 * and apply legacy throttle flow
	 */

	freq_khz = *freq / 1000;

#if defined(CONFIG_MTK_GPUFREQ_V2)
	(void)(pow);
	(void)(opp_idx);
	gpufreq_set_limit(TARGET_DEFAULT, LIMIT_THERMAL_AP, freq_khz, GPUPPM_KEEP_IDX);

	kbdev->current_nominal_freq =
		gpufreq_get_cur_freq(TARGET_DEFAULT) * 1000; /* khz to hz*/
#else
	opp_idx = mt_gpufreq_get_opp_idx_by_freq(freq_khz);
	if (opp_idx) {
		pow = mt_gpufreq_get_power_by_idx(opp_idx);
		mt_gpufreq_thermal_protect(pow);
		resume = 0;
	} else {
		if (!resume) {
			mt_gpufreq_thermal_protect(0);
			resume = 1;
		}
	}

	opp_idx = mt_gpufreq_get_cur_freq_index();
	kbdev->current_nominal_freq = mt_gpufreq_get_freq_by_idx(opp_idx) * 1000;
#endif /* CONFIG_MTK_GPUFREQ_V2 */

	return 0;
}

void mtk_common_devfreq_update_profile(struct devfreq_dev_profile *dp)
{
	dp->polling_ms = 0;
	dp->target = mtk_common_devfreq_target;
}

int mtk_common_devfreq_init(void)
{
	int ret = 0;

	ret = devfreq_add_governor(&mtk_common_gov_dummy);
	if (ret) {
		pr_info("@%s: Failed to add governor '%s' (ret: %d)\n",
		        __func__, mtk_common_gov_dummy.name, ret);
		return ret;
	}

	return ret;
}

int mtk_common_devfreq_term(void)
{
	int ret = 0;

	ret = devfreq_remove_governor(&mtk_common_gov_dummy);
	if (ret) {
		pr_info("@%s: Failed to remove governor '%s' (ret: %d)\n",
		        __func__, mtk_common_gov_dummy.name, ret);
		return ret;
	}

	return ret;
}
