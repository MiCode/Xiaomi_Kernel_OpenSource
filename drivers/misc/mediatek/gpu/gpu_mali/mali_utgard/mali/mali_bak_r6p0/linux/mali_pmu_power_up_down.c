/**
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010, 2012-2015 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file mali_pmu_power_up_down.c
 */

#include <linux/module.h>
#include "mali_executor.h"

int mali_perf_set_num_pp_cores(unsigned int num_cores)
{
	return mali_executor_set_perf_level(num_cores, MALI_FALSE);
}

EXPORT_SYMBOL(mali_perf_set_num_pp_cores);
