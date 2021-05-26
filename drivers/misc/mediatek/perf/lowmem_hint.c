/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <mtk_meminfo.h>
#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/math64.h>
#include <linux/miscdevice.h>
#include <linux/swap.h>
#include <trace/events/mtk_events.h>

#define TAG "[mtk-perf]"
#define THROTTLE_ONE_SEC 1000000000L
#define MS_TO_NS 1000000L
#define K(x) ((x) << (PAGE_SHIFT - 10))


int lowmem_hint_init;
int lowmem_hint_enable;
static struct miscdevice mtk_perf_dev;
static void trigger_memory_thrash_event(void);
static DECLARE_WORK(mem_thrash_notify_work,
		(void *)trigger_memory_thrash_event);
static struct workqueue_struct *wq;
static unsigned long long mem_thrash_throttle_ns;
static long int g_mt_customized_target_kb;
static long mm_thrash_point_kb;
static ktime_t mem_thrash_throttle;

/* Memory headroom before saturation (in pages) */
#ifndef CONFIG_MTK_GMO_RAM_OPTIMIZE
#define MIN_MEM_HEADROOM	(18432)
#else
#define MIN_MEM_HEADROOM	(3072)
#endif
static unsigned long memory_headroom = MIN_MEM_HEADROOM;
module_param_named(mem_headroom, memory_headroom, ulong, 0644);

/* Memory level for early notification
 * to avoid saturation on memory (in pages)
 */
static unsigned long get_memory_headroom(void)
{
	unsigned long level = memory_headroom;

	/* Increase headroom for 64-bit */
	if (IS_ENABLED(CONFIG_64BIT))
		level = level * 3 / 2;

	return level + totalreserve_pages;
}

void trigger_lowmem_hint(long *out_avail_mem, long *out_free_mem)
{

	long free_mem_kb;

	if (!lowmem_hint_enable || !lowmem_hint_init)
		return;

	mm_thrash_point_kb = (g_mt_customized_target_kb > 0) ?
		g_mt_customized_target_kb : K(get_memory_headroom());

	*out_free_mem = global_zone_page_state(NR_FREE_PAGES);
	*out_avail_mem = si_mem_available();
	free_mem_kb = K(*out_free_mem);

	if (free_mem_kb <= mm_thrash_point_kb) {
		trace_trigger_lowmem_hint(free_mem_kb, mm_thrash_point_kb);

		/* is throttled ? */
		if (!ktime_before(ktime_get(), mem_thrash_throttle)) {
			if (wq)
				queue_work(wq, &mem_thrash_notify_work);

			mem_thrash_throttle = ktime_add_ns(
					ktime_get(),
					mem_thrash_throttle_ns);
		}
	}
}

void trigger_memory_thrash_event(void)
{
	char *envp[2];
	int ret = -1;
	char lowmem_hint[] = "low_memory_hint=1";

	envp[0] = lowmem_hint;
	envp[1] = NULL;

	if (lowmem_hint_init)
		ret = kobject_uevent_env(&mtk_perf_dev.this_device->kobj,
				KOBJ_CHANGE, envp);

	trace_lowmem_hint_uevent(ret);
}

static int mtk_perf_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int mtk_perf_release(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations mtk_perf_fops = {
	.owner = THIS_MODULE,
	.open = mtk_perf_open,
	.release = mtk_perf_release,
};

/* PROC fs */
#define PROC_FOPS_RW(name) \
	static int mtk_perf_ ## name ## _proc_open(\
			struct inode *inode, struct file *file) \
{ \
	return single_open(file,\
			mtk_perf_ ## name ## _proc_show, PDE_DATA(inode));\
} \
static const struct file_operations mtk_perf_ ## name ## _proc_fops = { \
	.owner  = THIS_MODULE, \
	.open   = mtk_perf_ ## name ## _proc_open, \
	.read   = seq_read, \
	.llseek = seq_lseek,\
	.release = single_release,\
	.write  = mtk_perf_ ## name ## _proc_write,\
}

#define PROC_FOPS_RO(name) \
	static int mtk_perf_ ## name ## _proc_open(\
			struct inode *inode, struct file *file) \
{  \
	return single_open(file,\
			mtk_perf_ ## name ## _proc_show, PDE_DATA(inode));\
}  \
static const struct file_operations mtk_perf_ ## name ## _proc_fops = { \
	.owner  = THIS_MODULE, \
	.open   = mtk_perf_ ## name ## _proc_open, \
	.read   = seq_read, \
	.llseek = seq_lseek,\
	.release = single_release, \
}

#define PROC_ENTRY(name) {__stringify(name), \
	&mtk_perf_ ## name ## _proc_fops}

static int mtk_perf_lowmem_hint_enable_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%u\n", lowmem_hint_enable);
	return 0;
}

static ssize_t mtk_perf_lowmem_hint_enable_proc_write(
		struct file *filp, const char *ubuf, size_t cnt, loff_t *data)
{
	char enable;

	if (cnt > 0) {
		if (get_user(enable, ubuf))
			return -EFAULT;
		if (enable == '0')
			lowmem_hint_enable = 0;
		else if (enable == '1')
			lowmem_hint_enable = 1;
		else
			return -EINVAL;
	}

	return cnt;
}

static int mtk_perf_mt_throttle_ms_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%llu\n", mem_thrash_throttle_ns ?
			div_u64(mem_thrash_throttle_ns, MS_TO_NS) : 0);

	return 0;
}

static ssize_t mtk_perf_mt_throttle_ms_proc_write(
		struct file *filp, const char *ubuf, size_t cnt, loff_t *data)
{
	int ret, val;
	char buf[64];

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, ubuf, cnt))
		return -EFAULT;

	buf[cnt] = 0;

	ret = kstrtoint(buf, 10, &val);
	if (ret < 0)
		return ret;

	if (val < 0)
		return -EINVAL;

	mem_thrash_throttle_ns = (unsigned long long)val * MS_TO_NS;

	return cnt;
}


static int mtk_perf_mt_customized_target_proc_show(
		struct seq_file *m, void *v)
{
	seq_printf(m, "%ld KB\n",  g_mt_customized_target_kb);

	return 0;
}

static ssize_t mtk_perf_mt_customized_target_proc_write(
		struct file *filp, const char *ubuf, size_t cnt, loff_t *data)
{
	int ret, val;
	char buf[64];

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, ubuf, cnt))
		return -EFAULT;

	buf[cnt] = 0;

	ret = kstrtoint(buf, 10, &val);
	if (ret < 0)
		return ret;

	if (val < 0)
		return -EINVAL;

	g_mt_customized_target_kb = val;

	return cnt;
}

static int mtk_perf_mt_info_proc_show(struct seq_file *m, void *v)
{
	struct sysinfo i;
	long cached;

	si_meminfo(&i);
	si_swapinfo(&i);

	cached = global_node_page_state(NR_FILE_PAGES) -
		total_swapcache_pages() - i.bufferram;

	if (cached < 0)
		cached = 0;

	seq_printf(m, "free=%ld KB\n", K(i.freeram));
	seq_printf(m, "cache=%ld KB\n", K(cached));
	seq_printf(m, "target=%ld KB\n", mm_thrash_point_kb);

	return 0;
}

PROC_FOPS_RW(mt_throttle_ms);
PROC_FOPS_RW(mt_customized_target);
PROC_FOPS_RO(mt_info);
PROC_FOPS_RW(lowmem_hint_enable);

static int init_proc_dir(void)
{
	int i;

	struct proc_dir_entry *mtk_perf_dir = NULL;
	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};
	const struct pentry entries[] = {
		PROC_ENTRY(mt_throttle_ms),
		PROC_ENTRY(mt_info),
		PROC_ENTRY(mt_customized_target),
		PROC_ENTRY(lowmem_hint_enable),
	};

	mtk_perf_dir = proc_mkdir("mtk-perf", NULL);

	/* create procfs */
	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create(entries[i].name, 0644,
					mtk_perf_dir, entries[i].fops)) {
			pr_info("%s %s: create %s error\n",
					TAG,
					__func__,
					entries[i].name);
			return -EINVAL;
		}
	}

	return 0;
}

static int init_mtk_perf_dev(void)
{
	int ret;

	mtk_perf_dev.name = "mtk-perf";
	mtk_perf_dev.minor = MISC_DYNAMIC_MINOR;
	ret = misc_register(&mtk_perf_dev);

	if (ret) {
		pr_info("%s %s: misc_register error:(%d)\n",
			TAG, __func__, ret);
		return ret;
	}

	ret = kobject_uevent(&mtk_perf_dev.this_device->kobj, KOBJ_ADD);

	if (ret) {
		pr_info("%s %s: kobject_uevent error:(%d)\n",
			TAG, __func__, ret);
		return ret;
	}

	wq = create_singlethread_workqueue("mem_thrash_detector");

	if (!wq) {
		pr_info("%s %s: create_workqueue error:(%d)\n",
			TAG, __func__, ret);
		return -ENOMEM;
	}

	mem_thrash_throttle_ns = THROTTLE_ONE_SEC;

	init_proc_dir();

	lowmem_hint_init = 1;

	return 0;
}
module_init(init_mtk_perf_dev);
