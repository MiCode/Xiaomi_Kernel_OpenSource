/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
#include <linux/module.h>
#include "governor.h"

unsigned long (*extern_get_bw)(void) = NULL;
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
				unsigned long *freq,
				u32 *flag)
{
	/* If the IB isn't set yet, check if it should be non-zero. */
	if (!dev_ib && extern_get_bw) {
		dev_ib = extern_get_bw();
		if (dev_ab)
			*dev_ab = dev_ib / 4;
	}

	*freq = dev_ib;
	return 0;
}

/*
 * Registers a function to be used to request a frequency
 * value from legacy vbif based bus bandwidth governor.
 * This function is called by KGSL driver.
 */
void devfreq_vbif_register_callback(void *p)
{
	extern_get_bw = p;
}

int devfreq_vbif_update_bw(unsigned long ib, unsigned long ab)
{
	int ret = 0;

	mutex_lock(&df_lock);
	if (df) {
		mutex_lock(&df->lock);
		dev_ib = ib;
		*dev_ab = ab;
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
		if (df->profile->get_dev_status)
			ret = df->profile->get_dev_status(df->dev.parent,
					&stat);
		else
			ret = 0;
		if (ret || !stat.private_data)
			pr_warn("Device doesn't take AB votes!\n");
		else
			dev_ab = stat.private_data;
		mutex_unlock(&df_lock);

		ret = devfreq_vbif_update_bw(0, 0);
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

	return;
}
module_exit(devfreq_vbif_exit);

MODULE_DESCRIPTION("VBIF based GPU bus BW voting governor");
MODULE_LICENSE("GPL v2");


