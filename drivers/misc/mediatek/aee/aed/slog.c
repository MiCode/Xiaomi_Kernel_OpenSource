/*
 * Copyright (C) 2020 MediaTek Inc.
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

#include <linux/module.h>
#include <linux/tracepoint.h>
#include <linux/proc_fs.h>

#define CREATE_TRACE_POINTS
#include <mt-plat/slog.h>
#include <mt-plat/aee.h>

/**
 * Data structures to store tracepoints information
 */
struct tracepoints_table {
	const char *name;
	const char *mod_name;
	bool module;
	void *func;
	struct tracepoint *tp;
	bool init;
};

static unsigned int ufs_count;
static unsigned int ccci_count;
static unsigned int gpu_count;
static unsigned int threshold;
static struct proc_dir_entry *dir;

void slog(const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, fmt);
	vaf.fmt = fmt;
	vaf.va = &args;
	trace_slog(&vaf);
	va_end(args);
}

static void probe_ufs_mtk_event(void *data, unsigned int type,
	unsigned int val)
{
	ufs_count++;
	slog("#$#ufs#@#event#%lu:%lu(0x%x)#%d", type, val, val, ufs_count);
	if ((threshold > 0) && ((ufs_count % threshold) == 0)) {
		aee_kernel_exception("ufs", "occur:%d, threshold:%d\n", ufs_count, threshold);
	}
}

static void probe_ccci_event(void *data, char *string, char *sub_string,
	unsigned int sub_type, unsigned int resv)
{
	ccci_count++;
	slog("#$#%s#@#%s#%d:%d#%d", string, sub_string, sub_type, resv, ccci_count);
}

static void probe_gpu_hardstop_event(void *data, char *string, char *sub_string,
	unsigned int mfgpll, unsigned int freq, unsigned int vgpu, unsigned int vsram_gpu)
{
	gpu_count++;
	slog("#$#%s#@#%s#%d:%d:%d:%d#%d",
		string, sub_string, mfgpll, freq, vgpu, vsram_gpu, gpu_count);
}

static struct tracepoints_table interests[] = {
	{.name = "ufs_mtk_event", .mod_name = NULL, .module = false, .func = probe_ufs_mtk_event},
	{.name = "ccci_event", .mod_name = "NULL", .module = false, .func = probe_ccci_event},
	{.name = "gpu_hardstop", .mod_name = "NULL", .module = false,
		.func = probe_gpu_hardstop_event},
};

#define FOR_EACH_INTEREST(i) \
	for (i = 0; i < sizeof(interests) / sizeof(struct tracepoints_table); \
	i++)

/**
 * Find the struct tracepoint* associated with a given tracepoint
 * name.
 */
static void lookup_tracepoints(struct tracepoint *tp, void *ignore)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (strcmp(interests[i].name, tp->name) == 0)
			interests[i].tp = tp;
	}
}

static void cleanup(void)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (interests[i].init) {
			tracepoint_probe_unregister(interests[i].tp,
						    interests[i].func, NULL);
		}
	}
}

#ifdef MODULE
static void for_each_modlist_tracepoint(
	void (*fct)(struct tracepoint *tp, void *priv),
	void *priv)
{
#if IS_ENABLED(CONFIG_TRACEPOINTS)
	struct module *mod;
	tracepoint_ptr_t *iter, *begin, *end;
	int i;
	if (!fct)
		return;
	for (i = 0; i < ARRAY_SIZE(interests); i++) {
		if (interests[i].module == true) {
			mutex_lock(&module_mutex);
			mod = find_module(interests[i].mod_name);
			mutex_unlock(&module_mutex);

			if (!mod)
				continue;

			begin = mod->tracepoints_ptrs;
			end = mod->tracepoints_ptrs + mod->num_tracepoints;
			for (iter = begin; iter < end; iter++)
				fct(tracepoint_ptr_deref(iter), NULL);
		}
	}
#endif
}
#endif

static int slog_threshold_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "slog threshold: %d\n", threshold);
	return 0;
}

static char *_copy_from_user_for_proc(const char __user *buffer, size_t count)
{
	char *buf = (char *)__get_free_page(GFP_USER);

	if (!buf)
		return NULL;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	return buf;

out:
	free_page((unsigned long)buf);

	return NULL;
}

static ssize_t slog_threshold_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int num;
	int rc;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	rc = kstrtouint(buf, 10, &num);

	if (rc < 0)
		pr_info("transfer string to int error\n");
	else
		threshold = num;

	free_page((unsigned long)buf);

	pr_info("set threshold to %d\n", threshold);
	return count;
}

#define PROC_FOPS_RW(name)                                              \
static int name ## _proc_open(struct inode *inode, struct file *file)	\
{                                                                       \
	return single_open(file, name ## _proc_show, PDE_DATA(inode));  \
}                                                                       \
static const struct file_operations name ## _proc_fops = {              \
	.owner          = THIS_MODULE,                                  \
	.open           = name ## _proc_open,                           \
	.read           = seq_read,                                     \
	.llseek         = seq_lseek,                                    \
	.release        = single_release,                               \
	.write          = name ## _proc_write,                          \
}

#define PROC_ENTRY(name)        {__stringify(name), &name ## _proc_fops}

PROC_FOPS_RW(slog_threshold);

int slog_procfs_init(void)
{
	int i = 0;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(slog_threshold),
	};

	dir = proc_mkdir("slog", NULL);

	if (!dir) {
		pr_info("mkdir /proc/slog failed\n");
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create(entries[i].name, 0640, dir, entries[i].fops))
			pr_info("create /proc/slog/%s failed\n", entries[i].name);
	}

	return 0;
}

static int __init mtk_slog_init(void)
{
	int i;
#ifdef MODULE
	for_each_modlist_tracepoint(lookup_tracepoints, NULL);
#endif
	for_each_kernel_tracepoint(lookup_tracepoints, NULL);

	FOR_EACH_INTEREST(i) {
		for (i = 0; i < ARRAY_SIZE(interests); i++) {
			if (interests[i].tp == NULL) {
				pr_info("Error: %s\n", interests[i].name);
				continue;
			}
			tracepoint_probe_register(interests[i].tp, interests[i].func,
						  NULL);

			interests[i].init = true;
		}
	}
	slog_procfs_init();
	return 0;
}

static void __exit mtk_slog_exit(void)
{
	remove_proc_entry("slog_threshold", dir);
	cleanup();
}

module_init(mtk_slog_init);
module_exit(mtk_slog_exit);

MODULE_AUTHOR("MediaTek Inc.");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek System Log");
