/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APU_GOV_H__
#define __APU_GOV_H__

#include <linux/devfreq.h>
#include "apusys_power_user.h"

/* DEVFREQ governor name */
#define APU_GOV_USERDEF			"apuuser"
#define APU_GOV_PASSIVE			"apupassive"

struct apu_dev;

/**
 * struct apu_gov_data - void *data fed to struct devfreq
 *	and devfreq_add_device
 * @parent:	the devfreq instance of parent device.
 * @this:	the devfreq instance of own device.
 * @nb:		the notifier block for DEVFREQ_TRANSITION_NOTIFIER list
 * @user_freq	the freq that user expect to be
 * @valid	whether user_boost is valid or not.
 *
 * The devfreq_passive_data have to set the devfreq instance of parent
 * device with governors except for the passive governor. But, don't need to
 * initialize the 'this' and 'nb' field because the devfreq core will handle
 * them.
 */
struct apu_gov_data {
	struct devfreq *parent;
	struct devfreq *this;

	/*
	 * Array of freqs that child wants parent to be.
	 * Suppose this parrent has VPU0, VPU1 and MDLA0,
	 * and it will looks like below:
	 * (the order of array is decided by enum dvfs_user)
	 *     child_opp[VPU0] = VPU0 expect opp of it's parent
	 *     child_opp[VPU1] = VPU1 expect opp of it's parent
	 *     child_opp[MDLA0] = MDLA0 expect opp of it's parent
	 */
	u32 **child_opp;

	/*
	 * DEVFREQ_PRECHANGE/DEVFREQ_POSTCHANGE
	 * will container_of(nb) to get apu_gov_data
	 */
	struct notifier_block nb;

	u32 depth;
	/* next boost user wants to set */
	int n_opp;
	bool valid;
};
#endif
