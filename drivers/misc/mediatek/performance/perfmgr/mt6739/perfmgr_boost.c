/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/

#include <linux/seq_file.h>
#include <linux/kallsyms.h>
#include <linux/utsname.h>
#include <linux/uaccess.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/notifier.h>

#include <linux/platform_device.h>
#include <legacy_controller.h>

/*--------------DEFAULT SETTING-------------------*/

#define TARGET_CORE (3)
#define TARGET_FREQ (1183000)
#define CLUSTER_NUM (1)

/*-----------------------------------------------*/

int perfmgr_get_target_core(void)
{
	return TARGET_CORE;
}

int perfmgr_get_target_freq(void)
{
	return TARGET_FREQ;
}

void init_perfmgr_boost(void)
{
	/* do nothing */
}

void perfmgr_boost(int enable, int core, int freq)
{
	struct ppm_limit_data core_to_set, freq_to_set;

	if (enable) {
		core_to_set.min = core;
		core_to_set.max = -1;
		freq_to_set.min = freq;
		freq_to_set.max = -1;
	} else {
		core_to_set.min = -1;
		core_to_set.max = -1;
		freq_to_set.min = -1;
		freq_to_set.max = -1;
	}

	update_userlimit_cpu_core(PPM_KIR_FBC, CLUSTER_NUM, &core_to_set);
	update_userlimit_cpu_freq(PPM_KIR_FBC, CLUSTER_NUM, &freq_to_set);
}

/* redundant API */
void perfmgr_forcelimit_cpuset_cancel(void)
{

}
