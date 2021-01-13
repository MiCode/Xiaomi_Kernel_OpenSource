/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016, 2019-2021, The Linux Foundation. All rights reserved.
 */

#ifndef __CORE_CTL_H
#define __CORE_CTL_H

#include <linux/types.h>

#define MAX_CPUS_PER_CLUSTER 6
#define MAX_CLUSTERS 3

struct core_ctl_notif_data {
	unsigned int nr_big;
	unsigned int coloc_load_pct;
	unsigned int ta_util_pct[MAX_CLUSTERS];
	unsigned int cur_cap_pct[MAX_CLUSTERS];
};

struct notifier_block;
#endif
