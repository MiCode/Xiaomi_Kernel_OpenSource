// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/tracepoint.h>

#include <mt-plat/aee.h>
#define CREATE_TRACE_POINTS
#include <mt-plat/slog.h>

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
	unsigned int gpu_freq, unsigned int gpu_volt, unsigned int gpu_vsram,
	unsigned int stack_freq, unsigned int stack_volt, unsigned int stack_vsram)
{
	gpu_count++;
	slog("#$#%s#@#%s#%d:%d:%d:%d:%d:%d#%d",
		string, sub_string, gpu_freq, gpu_volt, gpu_vsram,
		stack_freq, stack_volt, stack_vsram, gpu_count);
}

static struct tracepoints_table interests[] = {
	{.name = "ufs_mtk_event", .mod_name = NULL, .module = false, .func = probe_ufs_mtk_event},
	{.name = "ccci_event", .mod_name = NULL, .module = false, .func = probe_ccci_event},
	{.name = "gpu_hardstop", .mod_name = "mtk_gpufreq_wrapper", .module = true, .func = probe_gpu_hardstop_event},
};

/**
 * Find the struct tracepoint* associated with a given tracepoint
 * name.
 */
static void lookup_tracepoints(struct tracepoint *tp, void *ignore)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(interests); i++) {
		if ((interests[i].module == false) && (strcmp(interests[i].name, tp->name) == 0)) {
			interests[i].tp = tp;
			tracepoint_probe_register(interests[i].tp, interests[i].func, NULL);
			interests[i].init = true;
		}
	}
}

static void cleanup(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(interests); i++) {
		if (interests[i].init) {
			tracepoint_probe_unregister(interests[i].tp,
						    interests[i].func, NULL);
		}
	}
}

#ifdef MODULE
static int slog_module_callback(struct notifier_block *nb,
                                unsigned long val, void *data)
{
	struct module *mod = data;
	tracepoint_ptr_t *iter, *begin, *end;
	struct tracepoint *tp;
	int i;

	if (val !=MODULE_STATE_LIVE)
		return NOTIFY_DONE;

	for (i = 0; i < ARRAY_SIZE(interests); i++) {
		if ((interests[i].module == true) && (interests[i].init == false)) {
			begin = mod->tracepoints_ptrs;
			end = mod->tracepoints_ptrs + mod->num_tracepoints;
			for (iter = begin; iter < end; iter++) {
				tp = tracepoint_ptr_deref(iter);
				if (strcmp(interests[i].name, tp->name) == 0) {
					interests[i].tp = tp;
					tracepoint_probe_register(interests[i].tp, interests[i].func, NULL);
					interests[i].init = true;
				}
			}
		}
	}

	return NOTIFY_OK;
}

static struct notifier_block slog_module_nb = {
	.notifier_call = slog_module_callback,
};
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
static const struct proc_ops name ## _proc_fops = {             	 \
	.proc_open           = name ## _proc_open,                           \
	.proc_read           = seq_read,                                     \
	.proc_lseek         = seq_lseek,                                    \
	.proc_release        = single_release,                               \
	.proc_write          = name ## _proc_write,                          \
}

#define PROC_ENTRY(name)        {__stringify(name), &name ## _proc_fops}

PROC_FOPS_RW(slog_threshold);

int slog_procfs_init(void)
{
	int i = 0;

	struct pentry {
		const char *name;
		const struct proc_ops *fops;
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

int mtk_slog_init(void)
{
#ifdef MODULE
	register_module_notifier(&slog_module_nb);
#endif
	for_each_kernel_tracepoint(lookup_tracepoints, NULL);
	slog_procfs_init();

	return 0;
}

void mtk_slog_exit(void)
{
#ifdef MODULE
	unregister_module_notifier(&slog_module_nb);
#endif
	remove_proc_entry("slog_threshold", dir);
	cleanup();
}
