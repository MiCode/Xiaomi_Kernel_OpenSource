/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/devfreq.h>
#include <mach/cpufreq.h>
#include "governor.h"

DEFINE_MUTEX(df_lock);
static struct devfreq *df;

static int devfreq_msm_cpufreq_get_freq(struct devfreq *df,
					unsigned long *freq,
					u32 *flag)
{
	*freq = msm_cpufreq_get_bw();
	return 0;
}

int devfreq_msm_cpufreq_update_bw(void)
{
	int ret = 0;

	mutex_lock(&df_lock);
	if (df) {
		mutex_lock(&df->lock);
		ret = update_devfreq(df);
		mutex_unlock(&df->lock);
	}
	mutex_unlock(&df_lock);
	return ret;
}

static int devfreq_msm_cpufreq_ev_handler(struct devfreq *devfreq,
					unsigned int event, void *data)
{
	int ret;

	switch (event) {
	case DEVFREQ_GOV_START:
		mutex_lock(&df_lock);
		df = devfreq;
		mutex_unlock(&df_lock);

		ret = devfreq_msm_cpufreq_update_bw();
		if (ret) {
			pr_err("Unable to update BW! Gov start failed!\n");
			return ret;
		}

		devfreq_monitor_stop(df);
		pr_debug("Enabled MSM CPUfreq governor\n");
		break;

	case DEVFREQ_GOV_STOP:
		mutex_lock(&df_lock);
		df = NULL;
		mutex_unlock(&df_lock);

		pr_debug("Disabled MSM CPUfreq governor\n");
		break;
	}

	return 0;
}

static struct devfreq_governor devfreq_msm_cpufreq = {
	.name = "msm_cpufreq",
	.get_target_freq = devfreq_msm_cpufreq_get_freq,
	.event_handler = devfreq_msm_cpufreq_ev_handler,
};

int register_devfreq_msm_cpufreq(void)
{
	return devfreq_add_governor(&devfreq_msm_cpufreq);
}
