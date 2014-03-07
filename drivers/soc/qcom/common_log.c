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

#define MISC_DUMP_DATA_LEN		4096

void register_misc_dump(void)
{
	void *misc_buf;
	int ret;
	struct msm_dump_entry dump_entry;
	struct msm_dump_data *misc_data;

	if (MSM_DUMP_MAJOR(msm_dump_table_version()) > 1) {
		misc_data = kzalloc(sizeof(struct msm_dump_data), GFP_KERNEL);
		if (!misc_data) {
			pr_err("misc dump data structure allocation failed\n");
			return;
		}
		misc_buf = kzalloc(MISC_DUMP_DATA_LEN, GFP_KERNEL);
		if (!misc_buf) {
			pr_err("misc buffer space allocation failed\n");
			goto err0;
		}
		misc_data->addr = virt_to_phys(misc_buf);
		misc_data->len = MISC_DUMP_DATA_LEN;
		dump_entry.id = MSM_DUMP_DATA_MISC;
		dump_entry.addr = virt_to_phys(misc_data);
		ret = msm_dump_data_register(MSM_DUMP_TABLE_APPS, &dump_entry);
		if (ret) {
			pr_err("Registering misc dump region failed\n");
			goto err1;
		}
		return;
err1:
		kfree(misc_buf);
err0:
		kfree(misc_data);
	}
}

static void __init common_log_register_log_buf(void)
{
	char **log_bufp;
	uint32_t *log_buf_lenp;
	uint32_t *fist_idxp;
	struct msm_client_dump dump_log_buf, dump_first_idx;
	struct msm_dump_entry entry_log_buf, entry_first_idx;
	struct msm_dump_data *dump_data;

	log_bufp = (char **)kallsyms_lookup_name("log_buf");
	log_buf_lenp = (uint32_t *)kallsyms_lookup_name("log_buf_len");
	if (!log_bufp || !log_buf_lenp) {
		pr_err("Unable to find log_buf by kallsyms!\n");
		return;
	}
	fist_idxp = (uint32_t *)kallsyms_lookup_name("log_first_idx");
	if (MSM_DUMP_MAJOR(msm_dump_table_version()) == 1) {
		dump_log_buf.id = MSM_LOG_BUF;
		dump_log_buf.start_addr = virt_to_phys(*log_bufp);
		dump_log_buf.end_addr = virt_to_phys(*log_bufp + *log_buf_lenp);
		if (msm_dump_tbl_register(&dump_log_buf))
			pr_err("Unable to register %d.\n", dump_log_buf.id);
		dump_first_idx.id = MSM_LOG_BUF_FIRST_IDX;
		if (fist_idxp) {
			dump_first_idx.start_addr = virt_to_phys(fist_idxp);
			if (msm_dump_tbl_register(&dump_first_idx))
				pr_err("Unable to register %d.\n",
							dump_first_idx.id);
		}
	} else {
		dump_data = kzalloc(sizeof(struct msm_dump_data),
						GFP_KERNEL);
		if (!dump_data) {
			pr_err("Unable to alloc data space.\n");
			return;
		}
		dump_data->len = *log_buf_lenp;
		dump_data->addr = virt_to_phys(*log_bufp);
		entry_log_buf.id = MSM_DUMP_DATA_LOG_BUF;
		entry_log_buf.addr = virt_to_phys(dump_data);
		if (msm_dump_data_register(MSM_DUMP_TABLE_APPS,
							&entry_log_buf)) {
			kfree(dump_data);
			pr_err("Unable to register %d.\n", entry_log_buf.id);
		}
		if (fist_idxp) {
			dump_data = kzalloc(sizeof(struct msm_dump_data),
							GFP_KERNEL);
			if (!dump_data) {
				pr_err("Unable to alloc data space.\n");
				return;
			}
			dump_data->addr = virt_to_phys(fist_idxp);
			entry_first_idx.id = MSM_DUMP_DATA_LOG_BUF_FIRST_IDX;
			entry_first_idx.addr = virt_to_phys(dump_data);
			if (msm_dump_data_register(MSM_DUMP_TABLE_APPS,
						&entry_first_idx))
				pr_err("Unable to register %d.\n",
						entry_first_idx.id);
		}
	}
}

static int __init msm_common_log_init(void)
{
	common_log_register_log_buf();
	register_misc_dump();
	return 0;
}
late_initcall(msm_common_log_init);
