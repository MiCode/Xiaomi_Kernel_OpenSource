// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/proc_fs.h>
#include "cpu_ctrl.h"

int powerhal_tid;

/*******************************************/
int update_userlimit_cpu_freq(int kicker, int num_cluster
		, struct ppm_limit_data *freq_limit)
{
	return 0;
}
EXPORT_SYMBOL(update_userlimit_cpu_freq);

int update_isolation_cpu(int kicker, int enable, int cpu)
{
	return 0;
}
EXPORT_SYMBOL(update_isolation_cpu);

int cpu_ctrl_init(struct proc_dir_entry *parent)
{
	return 0;
}

void cpu_ctrl_exit(void)
{

}

