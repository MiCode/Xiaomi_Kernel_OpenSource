// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <mt-plat/cpu_ctrl.h>
#include <linux/proc_fs.h>

int powerhal_tid;

/*******************************************/
int update_userlimit_cpu_freq(int kicker, int num_cluster
		, struct cpu_ctrl_data *freq_limit)
{
	return 0;
}
EXPORT_SYMBOL(update_userlimit_cpu_freq);

int cpu_ctrl_init(struct proc_dir_entry *parent)
{
	struct proc_dir_entry *boost_dir = NULL;

	boost_dir = proc_mkdir("cpu_ctrl_dummy", parent);
	if (!boost_dir)
		pr_debug("boost_dir null\n ");

	return 0;
}

