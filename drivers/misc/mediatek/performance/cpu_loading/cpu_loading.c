/*
 * Copyright (C) 2018 MediaTek Inc.
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

#include <linux/seq_file.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kobject.h>
#include <linux/device.h>
#include <linux/cpu.h>
#include <linux/slab.h>
#include <linux/tick.h>
#include <linux/kernel_stat.h>
#include <linux/module.h>
#include <linux/miscdevice.h>	/* for misc_register, and SYNTH_MINOR */
#include <linux/proc_fs.h>

#ifdef CONFIG_TRACING
#include <linux/kallsyms.h>
#include <linux/trace_events.h>
#endif

#ifdef CONFIG_CPU_FREQ
#include <linux/cpufreq.h>
#endif

#define TAG "[cpuLoading]"

static unsigned long poltime_nsecs; /*set nanoseconds to polling time*/
static int poltime_secs; /*set seconds to polling time*/
static int onoff; /*master switch*/
static bool debug_enable; /*master switch*/
static int over_threshold; /*threshold value for sent uevent*/
static int under_threshold; /*threshold value for sent uevent*/
static int uevent_enable; /*sent uevent switch*/
static int cpu_num; /*cpu number*/
static int prev_cpu_loading; /*record previous cpu loading*/
static bool reset_flag; /*check if need to reset value*/
static int state;

static struct mutex cpu_loading_lock;
static struct workqueue_struct *wq;
static struct hrtimer hrt1;
static void notify_cpu_loading_timeout(void);
static DECLARE_WORK(cpu_loading_timeout_work,
		(void *) notify_cpu_loading_timeout);/*statically create work */

struct cpu_info {
	cputime64_t time;
};

enum {
	high = 1,
	mid,
	low,

};

/* cpu loading tracking */
static struct cpu_info *cur_wall_time, *cur_idle_time,
		       *prev_wall_time, *prev_idle_time;

struct cpu_loading_context   {
	struct miscdevice mdev;
};

struct cpu_loading_context *cpu_loading_object;

#ifdef CONFIG_TRACING
static unsigned long __read_mostly tracing_mark_write_addr;

static inline void __mt_update_tracing_mark_write_addr(void)
{
	if (unlikely(tracing_mark_write_addr == 0))
		tracing_mark_write_addr =
			kallsyms_lookup_name("tracing_mark_write");
}

void cpu_loading_trace(char *module, char *string)
{
	__mt_update_tracing_mark_write_addr();
	preempt_disable();
	event_trace_printk(tracing_mark_write_addr, "%d [%s] %s\n",
			current->tgid, module, string);
	preempt_enable();
}

void trace_cpu_loading_log(char *module, const char *fmt, ...)
{
	char log[256];
	va_list args;
	int len;


	va_start(args, fmt);
	len = vsnprintf(log, sizeof(log), fmt, args);

	if (unlikely(len == 256))
		log[255] = '\0';
	va_end(args);
	cpu_loading_trace(module, log);
}
#endif

/*hrtimer trigger*/
static void enable_cpu_loading_timer(void)
{
	ktime_t ktime;

	ktime = ktime_set(poltime_secs, poltime_nsecs);
	hrtimer_start(&hrt1, ktime, HRTIMER_MODE_REL);
	trace_cpu_loading_log("cpu_loading", "enable timer");
}

/*close hrtimer*/
static void disable_cpu_loading_timer(void)
{
	hrtimer_cancel(&hrt1);
	trace_cpu_loading_log("cpu_loading", "disable timer");
}

static enum hrtimer_restart handle_cpu_loading_timeout(struct hrtimer *timer)
{
	if (wq)
		queue_work(wq, &cpu_loading_timeout_work);
	/*submit work[handle_cpu_loading_timeout_work] to workqueue*/
	return HRTIMER_NORESTART;
}

bool sentuevent(const char *src)
{
	int ret;
	char *envp[2];
	int string_size = 15;
	char  event_string[string_size];

	envp[0] = event_string;
	envp[1] = NULL;


	/*send uevent*/
	if (uevent_enable) {
		strlcpy(event_string, src, string_size);
		if (event_string[0] == '\0') { /*string is null*/

			trace_cpu_loading_log("cpu_loading", "string is null");
			return false;
		}

		ret = kobject_uevent_env(
				&cpu_loading_object->mdev.this_device->kobj,
				KOBJ_CHANGE, envp);
		if (ret != 0) {
			trace_cpu_loading_log("cpu_loading", "uevent failed");
			if (debug_enable)
				pr_debug("uevent failed");
			return false;
		}
		if (debug_enable)
			pr_debug("sent uevent success:%s", src);

		trace_cpu_loading_log("cpu_loading",
			 "sent uevent success:%s", src);
	}

	return true;
}

/*update info*/
int update_cpu_loading(void)
{
	int i, j, tmp_cpu_loading = 0;

	cputime64_t wall_time = 0, idle_time = 0;

	mutex_lock(&cpu_loading_lock);

	trace_cpu_loading_log("cpu_loading", "update cpu_loading");

	if (debug_enable)
		pr_debug("update cpu_loading");
	for_each_possible_cpu(j) {
		/*idle time include iowait time*/
		cur_idle_time[j].time = get_cpu_idle_time(j,
				&cur_wall_time[j].time, 1);

		trace_cpu_loading_log("cpu_loading",
			"cur_idle_time[%d].time:%llu cur_wall_time[%d].time:%llu\n",
			j, cur_idle_time[j].time, j, cur_wall_time[j].time);
	}

	if (reset_flag) {
		trace_cpu_loading_log("cpu_loading", "return reset");

		reset_flag = false;
		/*meet global value at first,and then set info to false  */

		mutex_unlock(&cpu_loading_lock);

		return 0;
	}

	for_each_possible_cpu(i) {
		wall_time += cur_wall_time[i].time - prev_wall_time[i].time;
		idle_time += cur_idle_time[i].time - prev_idle_time[i].time;

		trace_cpu_loading_log("cpu_loading",
			"cur_idle_time[%d].time:%llu cur_wall_time[%d].time:%llu\n",
			i, cur_idle_time[i].time, i, cur_wall_time[i].time);
	}

	if (wall_time > 0 && wall_time > idle_time)
		tmp_cpu_loading = div_u64((100 * (wall_time - idle_time)),
			wall_time);

	trace_cpu_loading_log("cpu_loading",
			"tmp_cpu_loading:%d prev_cpu_loading:%d previous state:%d\n",
			tmp_cpu_loading, prev_cpu_loading, state);
	if (debug_enable)
		pr_debug("tmp_cpu_loading:%d prev_cpu_loading:%d previous state:%d\n",
			tmp_cpu_loading, prev_cpu_loading, state);
	if (state == high) {
		if (under_threshold > tmp_cpu_loading) {
			state = low;
			sentuevent("lower=2");
		} else if (over_threshold > tmp_cpu_loading) {
			state = mid;
		} else {
			state = high;
			sentuevent("over=1");
		}

	} else if (state == mid) {
		if (tmp_cpu_loading > over_threshold) {
			state = high;
			sentuevent("over=1");
		} else if (under_threshold > tmp_cpu_loading) {
			state = low;
			sentuevent("lower=2");
		} else {
			state = mid;
		}
	} else {
		if (tmp_cpu_loading > over_threshold) {
			state = high;
			sentuevent("over=1");
		} else if (tmp_cpu_loading > under_threshold) {
			state = mid;
		} else {
			state = low;
			sentuevent("lower=2");
		}
	}
	if (debug_enable)
		pr_debug("current state:%d\n", state);
	trace_cpu_loading_log("cpu_loading", "current state:%d\n", state);
	prev_cpu_loading = tmp_cpu_loading;
	mutex_unlock(&cpu_loading_lock);
	return 3;
}

static void notify_cpu_loading_timeout(void)
{

	mutex_lock(&cpu_loading_lock);

	if (reset_flag) {
		int i;

		trace_cpu_loading_log("cpu_loading", "reset_cpu_loading");
		for_each_possible_cpu(i) {
			cur_wall_time[i].time = prev_wall_time[i].time = 0;
			cur_idle_time[i].time = prev_idle_time[i].time = 0;
		}

		prev_cpu_loading = 0;
		state = mid;
	} else {
		int i;

		trace_cpu_loading_log("cpu_loading", "not_reset_cpu_loading");
		for_each_possible_cpu(i) {
			prev_wall_time[i].time = cur_wall_time[i].time;
			prev_idle_time[i].time = cur_idle_time[i].time;
		}
	}

	mutex_unlock(&cpu_loading_lock);

	update_cpu_loading();
	enable_cpu_loading_timer();

}

/*default setting*/
void init_cpu_loading_value(void)
{

	mutex_lock(&cpu_loading_lock);

	/*default setting*/
	over_threshold = 85;
	under_threshold = 20;
	poltime_secs = 10;
	poltime_nsecs = 0;
	onoff = 0;
	uevent_enable = 1;
	debug_enable = 0;
	prev_cpu_loading = 0;
	reset_flag = true;

	mutex_unlock(&cpu_loading_lock);
}

/* PROCFS */
#define PROC_FOPS_RW(name) \
	static int cpu_loading_ ## name ## _proc_open(\
			struct inode *inode, struct file *file) \
{ \
	return single_open(file,\
			cpu_loading_ ## name ## _proc_show, PDE_DATA(inode));\
} \
static const struct file_operations cpu_loading_ ## name ## _proc_fops = { \
	.owner	= THIS_MODULE, \
	.open	= cpu_loading_ ## name ## _proc_open, \
	.read	= seq_read, \
	.llseek	= seq_lseek,\
	.release = single_release,\
	.write	= cpu_loading_ ## name ## _proc_write,\
}

#define PROC_FOPS_RO(name) \
	static int cpu_loading_ ## name ## _proc_open(\
			struct inode *inode, struct file *file) \
{  \
	return single_open(file,\
			cpu_loading_ ## name ## _proc_show, PDE_DATA(inode));\
}  \
static const struct file_operations cpu_loading_ ## name ## _proc_fops = { \
	.owner	= THIS_MODULE, \
	.open	= cpu_loading_ ## name ## _proc_open, \
	.read	= seq_read, \
	.llseek	= seq_lseek,\
	.release = single_release, \
}

#define PROC_ENTRY(name) {__stringify(name), \
	&cpu_loading_ ## name ## _proc_fops}

static int cpu_loading_onoff_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", onoff);
	return 0;
}

static ssize_t cpu_loading_onoff_proc_write(struct file *filp, const char *ubuf,
		size_t cnt, loff_t *data)
{
	int val;
	int ret;

	char buf[64];

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, ubuf, cnt))
		return -EFAULT;

	buf[cnt] = 0;
	ret = kstrtoint(buf, 10, &val);
	if (ret < 0)
		return ret;
	if (val > 1 || 0 > val)
		return -EINVAL;

	if (val)
		enable_cpu_loading_timer();
	else
		disable_cpu_loading_timer();

	mutex_lock(&cpu_loading_lock);

	onoff  = val;
	reset_flag = true;

	mutex_unlock(&cpu_loading_lock);

	return cnt;
}

static int cpu_loading_poltime_secs_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", poltime_secs);
	return 0;
}

static ssize_t cpu_loading_poltime_secs_proc_write(
		struct file *filp, const char *ubuf,
		size_t cnt, loff_t *data)
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

	if (val < 0) {

		trace_cpu_loading_log("cpu_loading",
				"out of range val:%d", val);
		return -EINVAL;
	}

	/*check both poltime_secs and poltime_nsecs can't be zero */
	if (!(poltime_nsecs | val)) {

		trace_cpu_loading_log("cpu_loading",
				"both 0, val:%d poltime_nsecs:%lu",
				val, poltime_nsecs);

		return -EINVAL;
	}

	mutex_lock(&cpu_loading_lock);

	poltime_secs  = val;
	reset_flag = true;

	mutex_unlock(&cpu_loading_lock);

	return cnt;
}

static int cpu_loading_poltime_nsecs_proc_show(
		struct seq_file *m, void *v)
{
	seq_printf(m, "%lu\n", poltime_nsecs);
	return 0;
}

static ssize_t cpu_loading_poltime_nsecs_proc_write(
		struct file *filp, const char *ubuf,
		size_t cnt, loff_t *data)
{
	unsigned long ret, val;

	char buf[64];

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, ubuf, cnt))
		return -EFAULT;
	buf[cnt] = 0;

	ret = _kstrtoul(buf, 10, &val);

	if (ret < 0)
		return ret;

	if (val >  UINT_MAX || val < 0) {

		trace_cpu_loading_log("cpu_loading",
				"out of range val:%lu", val);
		return -EINVAL;
	}
	/*check both poltime_secs and poltime_nsecs can't be zero */
	if (!(poltime_secs | val)) {

		trace_cpu_loading_log("cpu_loading",
				"both 0, val:%lu poltime_secs:%d",
				val, poltime_secs);
		return -EINVAL;
	}

	mutex_lock(&cpu_loading_lock);

	poltime_nsecs  = val;
	reset_flag = true;

	mutex_unlock(&cpu_loading_lock);

	return cnt;
}

static int cpu_loading_underThrhld_proc_show(
		struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", under_threshold);
	return 0;
}

static ssize_t cpu_loading_underThrhld_proc_write(
		struct file *filp, const char *ubuf,
		size_t cnt, loff_t *data)
{
	int ret;
	int val;

	char buf[64];

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, ubuf, cnt))
		return -EFAULT;


	buf[cnt] = 0;
	ret = kstrtoint(buf, 10, &val);
	if (ret < 0)
		return ret;

	mutex_lock(&cpu_loading_lock);

	if (val < 1 || over_threshold <= val) {
		mutex_unlock(&cpu_loading_lock);
		return -EINVAL;
	}
	under_threshold = val;
	reset_flag = true;

	mutex_unlock(&cpu_loading_lock);

	return cnt;
}



static int cpu_loading_overThrhld_proc_show(
		struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", over_threshold);
	return 0;
}

static ssize_t cpu_loading_overThrhld_proc_write(
		struct file *filp, const char *ubuf,
		size_t cnt, loff_t *data)
{
	int ret;
	int val;

	char buf[64];

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, ubuf, cnt))
		return -EFAULT;


	buf[cnt] = 0;
	ret = kstrtoint(buf, 10, &val);
	if (ret < 0)
		return ret;

	mutex_lock(&cpu_loading_lock);
	if (val > 99 || under_threshold >= val) {
		mutex_unlock(&cpu_loading_lock);
		return -EINVAL;
	}

	over_threshold = val;
	reset_flag = true;

	mutex_unlock(&cpu_loading_lock);

	return cnt;
}

static int cpu_loading_uevent_enable_proc_show(
		struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", uevent_enable);
	return 0;
}
static ssize_t cpu_loading_uevent_enable_proc_write(
		struct file *filp, const char *ubuf,
		size_t cnt, loff_t *data)
{
	int val;
	int ret;
	char buf[64];

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, ubuf, cnt))
		return -EFAULT;

	buf[cnt] = 0;
	ret = kstrtoint(buf, 10, &val);

	if (ret < 0)
		return ret;

	if (val > 1 || 0 > val)
		return -EINVAL;

	mutex_lock(&cpu_loading_lock);

	uevent_enable = val;
	reset_flag = true;

	mutex_unlock(&cpu_loading_lock);

	return cnt;

}
static int cpu_loading_prev_cpu_loading_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", prev_cpu_loading);
	return 0;
}
static int cpu_loading_debug_enable_proc_show(
		struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", debug_enable);
	return 0;
}
static ssize_t cpu_loading_debug_enable_proc_write(
		struct file *filp, const char *ubuf,
		size_t cnt, loff_t *data)
{
	int val;
	int ret;
	char buf[64];

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, ubuf, cnt))
		return -EFAULT;

	buf[cnt] = 0;
	ret = kstrtoint(buf, 10, &val);

	if (ret < 0)
		return ret;

	if (val > 1 || 0 > val)
		return -EINVAL;

	mutex_lock(&cpu_loading_lock);

	debug_enable = val;

	mutex_unlock(&cpu_loading_lock);

	return cnt;

}
PROC_FOPS_RW(onoff);
PROC_FOPS_RW(poltime_secs);
PROC_FOPS_RW(poltime_nsecs);
PROC_FOPS_RW(overThrhld);
PROC_FOPS_RW(underThrhld);
PROC_FOPS_RW(uevent_enable);
PROC_FOPS_RW(debug_enable);
PROC_FOPS_RO(prev_cpu_loading);
/*--------------------INIT------------------------*/
static int init_cpu_loading_kobj(void)
{
	int ret;

	cpu_loading_object =
		kzalloc(sizeof(struct cpu_loading_context), GFP_KERNEL);

	trace_cpu_loading_log("cpu_loading", "init_cpu_loading_obj start");
	/* dev init */

	cpu_loading_object->mdev.name = "cpu_loading";
	ret = misc_register(&cpu_loading_object->mdev);
	if (ret) {
		pr_debug(TAG"misc_register error:%d\n", ret);
		return -2;
	}

	ret = kobject_uevent(
			&cpu_loading_object->mdev.this_device->kobj, KOBJ_ADD);

	if (ret) {
		trace_cpu_loading_log("cpu_loading", "uevent creat  fail");
		return -4;
	}

	return 0;
}

static int __init init_cpu_loading(void)
{
	struct proc_dir_entry *cpu_loading_dir = NULL;
	int i, ret;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};


	const struct pentry entries[] = {
		PROC_ENTRY(onoff),
		PROC_ENTRY(poltime_secs),
		PROC_ENTRY(poltime_nsecs),
		PROC_ENTRY(overThrhld),
		PROC_ENTRY(underThrhld),
		PROC_ENTRY(uevent_enable),
		PROC_ENTRY(prev_cpu_loading),
		PROC_ENTRY(debug_enable),
	};
	cpu_loading_dir = proc_mkdir("cpu_loading", NULL);

	if (!cpu_loading_dir)
		return -ENOMEM;

	/* create procfs */
	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create(entries[i].name, 0644,
					cpu_loading_dir, entries[i].fops)) {
			pr_debug("%s(), create cpu_loading%s failed\n",
					__func__, entries[i].name);
			ret = -EINVAL;
			return ret;
		}
	}
	/*calc cpu num*/
	cpu_num = num_possible_cpus();

	cur_wall_time  =  kcalloc(cpu_num, sizeof(struct cpu_info), GFP_KERNEL);
	cur_idle_time  =  kcalloc(cpu_num, sizeof(struct cpu_info), GFP_KERNEL);
	prev_wall_time  =  kcalloc(cpu_num, sizeof(struct cpu_info),
			GFP_KERNEL);
	prev_idle_time  =  kcalloc(cpu_num, sizeof(struct cpu_info),
			GFP_KERNEL);

	for_each_possible_cpu(i) {
		prev_wall_time[i].time = cur_wall_time[i].time = 0;
		prev_idle_time[i].time = cur_idle_time[i].time = 0;
	}

	/*mutex init*/
	mutex_init(&cpu_loading_lock);

	/*workqueue init*/
	wq = create_singlethread_workqueue("cpu_loading_work");
	if (!wq) {
		trace_cpu_loading_log("cpu_loading", "work create fail");
		return -ENOMEM;
	}

	/*timer init*/
	hrtimer_init(&hrt1, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hrt1.function = &handle_cpu_loading_timeout;

	/* dev init */
	init_cpu_loading_kobj();

	init_cpu_loading_value();
	/* poting */


	return 0;
}

void exit_cpu_loading(void)
{
	kfree(cpu_loading_object);
	kfree(cur_wall_time);
	kfree(cur_idle_time);
	kfree(prev_wall_time);
	kfree(prev_idle_time);
}

module_init(init_cpu_loading);
module_exit(exit_cpu_loading);
