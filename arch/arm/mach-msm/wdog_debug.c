/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <mach/scm.h>

#define SCM_WDOG_DEBUG_BOOT_PART	0x9
#define BOOT_PART_EN_VAL		0x5D1

void msm_enable_wdog_debug(void)
{
	int ret;

	ret = scm_call_atomic2(SCM_SVC_BOOT,
			       SCM_WDOG_DEBUG_BOOT_PART, 0, BOOT_PART_EN_VAL);
	if (ret)
		pr_err("failed to enable wdog debug: %d\n", ret);
}
EXPORT_SYMBOL(msm_enable_wdog_debug);

void msm_disable_wdog_debug(void)
{
	int ret;

	ret = scm_call_atomic2(SCM_SVC_BOOT,
			       SCM_WDOG_DEBUG_BOOT_PART, 1, 0);
	if (ret)
		pr_err("failed to disable wdog debug: %d\n", ret);
}
EXPORT_SYMBOL(msm_disable_wdog_debug);
