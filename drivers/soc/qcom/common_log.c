/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
 * Copyright (C) 2019 XiaoMi, Inc.
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
#include <soc/qcom/minidump.h>
#include <asm/sections.h>

#define MISC_DUMP_DATA_LEN		4096
#define PMIC_DUMP_DATA_LEN		4096
#define VSENSE_DUMP_DATA_LEN		4096
#define RPM_DUMP_DATA_LEN		(160 * 1024)

void register_misc_dump(void)
{
	void *misc_buf;
	int ret;
	struct msm_dump_entry dump_entry;
	struct msm_dump_data *misc_data;

	if (MSM_DUMP_MAJOR(msm_dump_table_version()) > 1) {
		misc_data = kzalloc(sizeof(struct msm_dump_data), GFP_KERNEL);
		if (!misc_data)
			return;
		misc_buf = kzalloc(MISC_DUMP_DATA_LEN, GFP_KERNEL);
		if (!misc_buf)
			goto err0;

		strlcpy(misc_data->name, "KMISC", sizeof(misc_data->name));
		misc_data->addr = virt_to_phys(misc_buf);
		misc_data->len = MISC_DUMP_DATA_LEN;
		dump_entry.id = MSM_DUMP_DATA_MISC;
		dump_entry.addr = virt_to_phys(misc_data);
		ret = msm_dump_data_register(MSM_DUMP_TABLE_APPS, &dump_entry);
		if (ret)
			goto err1;
		return;
err1:
		kfree(misc_buf);
err0:
		kfree(misc_data);
	}
}

static void register_pmic_dump(void)
{
	static void *dump_addr;
	int ret;
	struct msm_dump_entry dump_entry;
	struct msm_dump_data *dump_data;

	if (MSM_DUMP_MAJOR(msm_dump_table_version()) > 1) {
		dump_data = kzalloc(sizeof(struct msm_dump_data), GFP_KERNEL);
		if (!dump_data) {
			pr_err("dump data structure allocation failed\n");
			return;
		}
		dump_addr = kzalloc(PMIC_DUMP_DATA_LEN, GFP_KERNEL);
		if (!dump_addr)
			goto err0;

		strlcpy(dump_data->name, "KPMIC", sizeof(dump_data->name));
		dump_data->addr = virt_to_phys(dump_addr);
		dump_data->len = PMIC_DUMP_DATA_LEN;
		dump_entry.id = MSM_DUMP_DATA_PMIC;
		dump_entry.addr = virt_to_phys(dump_data);
		ret = msm_dump_data_register(MSM_DUMP_TABLE_APPS, &dump_entry);
		if (ret) {
			pr_err("Registering pmic dump region failed\n");
			goto err1;
		}
		return;
err1:
		kfree(dump_addr);
err0:
		kfree(dump_data);
	}
}

static void register_vsense_dump(void)
{
	static void *dump_addr;
	int ret;
	struct msm_dump_entry dump_entry;
	struct msm_dump_data *dump_data;

	if (MSM_DUMP_MAJOR(msm_dump_table_version()) > 1) {
		dump_data = kzalloc(sizeof(struct msm_dump_data), GFP_KERNEL);
		if (!dump_data) {
			pr_err("dump data structure allocation failed for vsense data\n");
			return;
		}
		dump_addr = kzalloc(VSENSE_DUMP_DATA_LEN, GFP_KERNEL);
		if (!dump_addr)
			goto err0;

		strlcpy(dump_data->name, "KVSENSE",
				sizeof(dump_data->name));
		dump_data->addr = virt_to_phys(dump_addr);
		dump_data->len = VSENSE_DUMP_DATA_LEN;
		dump_entry.id = MSM_DUMP_DATA_VSENSE;
		dump_entry.addr = virt_to_phys(dump_data);
		ret = msm_dump_data_register(MSM_DUMP_TABLE_APPS, &dump_entry);
		if (ret) {
			pr_err("Registering vsense dump region failed\n");
			goto err1;
		}
		return;
err1:
		kfree(dump_addr);
err0:
		kfree(dump_data);
	}
}

void register_rpm_dump(void)
{
	static void *dump_addr;
	int ret;
	struct msm_dump_entry dump_entry;
	struct msm_dump_data *dump_data;

	if (MSM_DUMP_MAJOR(msm_dump_table_version()) > 1) {
		dump_data = kzalloc(sizeof(struct msm_dump_data), GFP_KERNEL);
		if (!dump_data) {
			pr_err("rpm dump data structure allocation failed\n");
			return;
		}
		dump_addr = kzalloc(RPM_DUMP_DATA_LEN, GFP_KERNEL);
		if (!dump_addr)
			goto err0;

		strlcpy(dump_data->name, "KRPM", sizeof(dump_data->name));
		dump_data->addr = virt_to_phys(dump_addr);
		dump_data->len = RPM_DUMP_DATA_LEN;
		dump_entry.id = MSM_DUMP_DATA_RPM;
		dump_entry.addr = virt_to_phys(dump_data);
		ret = msm_dump_data_register(MSM_DUMP_TABLE_APPS, &dump_entry);
		if (ret) {
			pr_err("Registering rpm dump region failed\n");
			goto err1;
		}
		return;
err1:
		kfree(dump_addr);
err0:
		kfree(dump_data);
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
		if (!dump_data)
			return;
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
			if (!dump_data)
				return;
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

static void __init register_kernel_sections(void)
{
	struct md_region ksec_entry;
	char *data_name = "KDATABSS";
	const size_t static_size = __per_cpu_end - __per_cpu_start;
	void __percpu *base = (void __percpu *)__per_cpu_start;
	unsigned int cpu;

	strlcpy(ksec_entry.name, data_name, sizeof(ksec_entry.name));
	ksec_entry.virt_addr = (uintptr_t)_sdata;
	ksec_entry.phys_addr = virt_to_phys(_sdata);
	ksec_entry.size = roundup((__bss_stop - _sdata), 4);
	if (msm_minidump_add_region(&ksec_entry))
		pr_err("Failed to add data section in Minidump\n");

	/* Add percpu static sections */
	for_each_possible_cpu(cpu) {
		void *start = per_cpu_ptr(base, cpu);

		memset(&ksec_entry, 0, sizeof(ksec_entry));
		scnprintf(ksec_entry.name, sizeof(ksec_entry.name),
			"KSPERCPU%d", cpu);
		ksec_entry.virt_addr = (uintptr_t)start;
		ksec_entry.phys_addr = per_cpu_ptr_to_phys(start);
		ksec_entry.size = static_size;
		if (msm_minidump_add_region(&ksec_entry))
			pr_err("Failed to add percpu sections in Minidump\n");
	}
}

static int __init msm_common_log_init(void)
{
	register_kernel_sections();
	common_log_register_log_buf();
	register_misc_dump();
	register_pmic_dump();
	register_vsense_dump();
	register_rpm_dump();
	return 0;
}
late_initcall(msm_common_log_init);
