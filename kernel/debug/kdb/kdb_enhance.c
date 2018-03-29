/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/sched.h>
#include <linux/utsname.h>
#include <linux/kdb.h>

#ifdef CONFIG_SCHED_DEBUG

DEFINE_PER_CPU(int, kdb_in_use) = 0;

/*
 * Display sched_debug information
 */
static int kdb_sched_debug(int argc, const char **argv)
{
	sysrq_sched_debug_show();
	return 0;
}

#endif

static __init int kdb_enhance_register(void)
{
#ifdef CONFIG_SCHED_DEBUG
	kdb_register_repeat("sched_debug", kdb_sched_debug, "",
			    "Display sched_debug information", 0, KDB_REPEAT_NONE);
#endif
	return 0;
}

device_initcall(kdb_enhance_register);
