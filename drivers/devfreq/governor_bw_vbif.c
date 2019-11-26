// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2014-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/devfreq.h>
#include <linux/module.h>
#include <linux/msm_adreno_devfreq.h>

#include "governor.h"

static getbw_func extern_get_bw;
static void *extern_get_bw_data;
unsigned long *dev_ab;
static unsigned long dev_ib;

DEFINE_MUTEX(df_lock);
static struct devfreq *df;

/*
 * This function is 'get_target_freq' API for the governor.
 * It just calls an external function that should be registered
 * by KGSL driver to get and return a value for frequency.
 */
static int devfreq_vbif_get_freq(struct devfreq *df,
				unsigned long *freq)
{
	unsigned long ab, ib;

	if (!extern_get_bw) {
		*freq = 0;
		return 0;
	}

	extern_get_bw(&ib, &ab, extern_get_bw_data);
	dev_ib = ib;
	*dev_ab = ab;

	*freq = dev_ib;
	return 0;
}

/*
 * Registers a function to be used to request a frequency
 * value from legacy vbif based bus bandwidth governor.
 * This function is called by KGSL driver.
 */
void devfreq_vbif_register_callback(getbw_func func, void *data)
{
	extern_get_bw = func;
	extern_get_bw_data = data;
}

int devfreq_vbif_update_bw(void)
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

static int devfreq_vbif_ev_handler(struct devfreq *devfreq,
					unsigned int event, void *data)
{
	int ret;
	struct devfreq_dev_status stat;

	switch (event) {
	case DEVFREQ_GOV_START:
		mutex_lock(&df_lock);
		df = devfreq;
		if (df->profile->get_dev_status &&
			!df->profile->get_dev_status(df->dev.parent, &stat) &&
			stat.private_data)
			dev_ab = stat.private_data;
		else
			pr_warn("Device doesn't take AB votes!\n");

		mutex_unlock(&df_lock);

		dev_ib = 0;
		*dev_ab = 0;

		ret = devfreq_vbif_update_bw();
		if (ret) {
			pr_err("Unable to update BW! Gov start failed!\n");
			return ret;
		}
		/*
		 * Normally at this point governors start the polling with
		 * devfreq_monitor_start(df);
		 * This governor doesn't poll, but expect external calls
		 * of its devfreq_vbif_update_bw() function
		 */
		pr_debug("Enabled MSM VBIF governor\n");
		break;

	case DEVFREQ_GOV_STOP:
		mutex_lock(&df_lock);
		df = NULL;
		mutex_unlock(&df_lock);

		pr_debug("Disabled MSM VBIF governor\n");
		break;
	}

	return 0;
}

static struct devfreq_governor devfreq_vbif = {
	.name = "bw_vbif",
	/* Restrict this governor to only gpu devfreq devices */
	.immutable = 1,
	.get_target_freq = devfreq_vbif_get_freq,
	.event_handler = devfreq_vbif_ev_handler,
};

static int __init devfreq_vbif_init(void)
{
	return devfreq_add_governor(&devfreq_vbif);
}
subsys_initcall(devfreq_vbif_init);

static void __exit devfreq_vbif_exit(void)
{
	int ret;

	ret = devfreq_remove_governor(&devfreq_vbif);
	if (ret)
		pr_err("%s: failed remove governor %d\n", __func__, ret);

}
module_exit(devfreq_vbif_exit);

MODULE_DESCRIPTION("VBIF based GPU bus BW voting governor");
MODULE_LICENSE("GPL v2");


