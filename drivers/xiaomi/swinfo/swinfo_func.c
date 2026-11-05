/*
 * swinfo.c
 *
 * Copyright (C) 2022 Xiaomi Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#include <soc/qcom/minidump.h>

/*
 * "IKCFG_ST" and "IKCFG_ED" are used to extract the config data from
 * a binary kernel image or a module. See scripts/extract-ikconfig.
 */
asm (
"	.pushsection .rodata, \"a\"		\n"
"	.ascii \"IKCFG_ST\"			\n"
"	.global platform_kernel_config_data		\n"
"platform_kernel_config_data:				\n"
"	.incbin \"drivers/xiaomi/swinfo/config_platform_data.gz\"	\n"
"	.global platform_kernel_config_data_end		\n"
"platform_kernel_config_data_end:			\n"
"	.ascii \"IKCFG_ED\"			\n"
"	.popsection				\n"
);

// from incbin
extern char platform_kernel_config_data;
extern char platform_kernel_config_data_end;

#define MAX_CMDLINE_PARAM_LEN 128
static char build_fingerprint[MAX_CMDLINE_PARAM_LEN] = {0};

struct proc_dir_entry *entry_swinfo = NULL;

static void add_data_to_dump_region(void *data, char *name, size_t region_size)
{
	struct md_region md_entry;
	void *buffer_start;

	pr_debug("%s: Adding %s in Minidump, size %zu", __func__, name, region_size);

	buffer_start = kzalloc(region_size, GFP_KERNEL);
	if (buffer_start == NULL) {
		pr_err("%s: Failed to add %s in Minidump, alloc memory failed!\n", __func__, name);
		return;
	}

	memcpy(buffer_start, data, region_size);

	/* Add data to minidump table */
	strlcpy(md_entry.name, name, sizeof(md_entry.name));
	md_entry.virt_addr = (uintptr_t)buffer_start;
	md_entry.phys_addr = virt_to_phys(buffer_start);
	md_entry.size = region_size;
	if (msm_minidump_add_region(&md_entry) < 0) {
		pr_err("%s: Failed to add %s data in Minidump, add region failed!\n", __func__, name);
		kfree(buffer_start);
	}
}

static ssize_t platform_ikconfig_read(struct file *file, char __user *buf,
			size_t len, loff_t * offset)
{
	return simple_read_from_buffer(buf, len, offset,
					&platform_kernel_config_data,
					&platform_kernel_config_data_end -
					&platform_kernel_config_data);
}

static const struct proc_ops platform_config_gz_proc_ops = {
	.proc_read	= platform_ikconfig_read,
	.proc_lseek	= default_llseek,
};

static int __init swinfo_init(void)
{
	struct proc_dir_entry *entry;
	int ret = -ENOMEM;

	/* create swinfo dir */
	entry_swinfo = proc_mkdir("swinfo", NULL);
	if (entry_swinfo == NULL) {
		pr_err("%s: Can't create swinfo proc entry\n", __func__);
		return ret;
	}

	entry = proc_create("platform_config.gz", S_IFREG | S_IRUGO, entry_swinfo,
			    &platform_config_gz_proc_ops);
	if (!entry) {
		pr_err("%s: %d: Can't create platform config proc entry!\n", __func__, __LINE__);
		return ret;
	}

	proc_set_size(entry, &platform_kernel_config_data_end - &platform_kernel_config_data);

	if (build_fingerprint[0] != 0) {
		add_data_to_dump_region(&build_fingerprint, "FINGERPRINT", MAX_CMDLINE_PARAM_LEN);
	}

	return 0;
}

static void __exit swinfo_exit(void)
{
	remove_proc_entry("platform_config.gz", entry_swinfo);
	remove_proc_entry("swinfo", NULL);
	entry_swinfo = NULL;
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("chenxinyuanchen@xiaomi.com");
core_initcall(swinfo_init);
module_exit(swinfo_exit);

module_param_string(fingerprint, build_fingerprint, MAX_CMDLINE_PARAM_LEN, 0644);
