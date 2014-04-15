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
#include <linux/slab.h>
#include <soc/qcom/memory_dump.h>

static void __init common_log_register(char *addr, uint32_t length,
						unsigned long *reserved)
{
	struct msm_client_dump dump;
	struct msm_dump_entry dump_entry;
	struct msm_dump_data *dump_data;
	int ret;
	static int count;

	if (MSM_DUMP_MAJOR(msm_dump_table_version()) == 1) {
		dump.id = MSM_COMMON_LOG + count;
		dump.start_addr = virt_to_phys(addr);
		dump.end_addr = virt_to_phys(addr + length);
		if (msm_dump_tbl_register(&dump))
			pr_err("Unable to register %d.\n", dump.id);
	} else {
		dump_data = kzalloc(sizeof(struct msm_dump_data), GFP_KERNEL);
		if (!dump_data) {
			pr_err("Unable to alloc data space.\n");
			return;
		}
		if (reserved != 0)
			dump_data->len = virt_to_phys(reserved);
		else
			dump_data->len = 0;
		dump_data->reserved = length;
		dump_data->addr = virt_to_phys(addr);
		dump_entry.id = MSM_DUMP_DATA_COMMON_LOG + count;
		dump_entry.addr = virt_to_phys(dump_data);
		ret = msm_dump_data_register(MSM_DUMP_TABLE_APPS, &dump_entry);
		if (ret) {
			kfree(dump_data);
			pr_err("Unable to register %d.\n", dump_entry.id);
		}
	}
	count++;
}

static void __init register_symbols(char *name, char *len)
{
	char **addr;
	uint32_t *length;
	unsigned long *reserved = 0;

	addr = (char **)kallsyms_lookup_name(name);
	length = (uint32_t *)kallsyms_lookup_name(len);
	if (!addr || !length) {
		pr_err("Unable to find %s by kallsyms!\n", name);
		return;
	}
	/* If item is log_buf, register first index to locate
	* the start address of kmsg in ring buffer. */
	if (strcmp(name, "log_buf") == 0)
		reserved = (unsigned long *)kallsyms_lookup_name(
							"log_first_idx");
	common_log_register(*addr, *length, reserved);
}

static int __init msm_common_log_init(void)
{
	register_symbols("log_buf", "log_buf_len");
	return 0;
}
late_initcall(msm_common_log_init);
