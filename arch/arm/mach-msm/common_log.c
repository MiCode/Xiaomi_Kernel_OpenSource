/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/kallsyms.h>
#include <mach/msm_memory_dump.h>

static void __init common_log_register(void)
{
	struct msm_client_dump dump;
	char **log_bufp;
	uint32_t *log_buf_lenp;

	log_bufp = (char **)kallsyms_lookup_name("log_buf");
	log_buf_lenp = (uint32_t *)kallsyms_lookup_name("log_buf_len");
	if (!log_bufp || !log_buf_lenp) {
		pr_err("common_log_register: Symbol log_buf not found!\n");
		return;
	}
	dump.id = MSM_LOG_BUF;
	dump.start_addr = virt_to_phys(*log_bufp);
	dump.end_addr = virt_to_phys(*log_bufp + *log_buf_lenp);
	if (msm_dump_table_register(&dump))
		pr_err("common_log_register: Could not register log_bug.\n");
}

static int __init msm_common_log_init(void)
{
	common_log_register();
	return 0;
}
late_initcall(msm_common_log_init);
