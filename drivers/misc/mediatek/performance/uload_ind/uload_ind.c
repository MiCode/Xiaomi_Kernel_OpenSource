// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 */

#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/cpumask.h>
#include "mtk_perfmgr_internal.h"
#include "load_track.h"
#include "uload_ind.h"

#define CREATE_TRACE_POINTS
#include "trace_uload_ind.h"

#if IS_ENABLED(CONFIG_CPU_FREQ)
#include <linux/cpufreq.h>
#endif

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kobject.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>   /* for misc_register, and SYNTH_MINOR */
#include <linux/proc_fs.h>

#include <linux/preempt.h>
#include <linux/trace_events.h>
#include <linux/kernel.h>

#define REG_SUCCESS (0)
#define REG_FAIL (-1)
#define UNREG_SUCCESS (-2)
#define UNREG_FAIL (-3)

static int onoff;/*master switch*/
static int polling_sec;
static int polling_ms;
static bool debug_enable;
static int over_threshold; /*threshold value for sent uevent*/
static int specify_overThrhld; /*specify cpus threshold value for sent uevent*/
static int under_threshold; /*threshold value for sent uevent*/
static bool uevent_enable; /*sent uevent switch*/
static int curr_cpu_loading; /*cat curr cpu loading node*/
static int specify_cpus; /*specify cpus*/
static int nr_cpus;/*cpu numbers*/
static int state;

static cpumask_t specify_cpu_mask = CPU_MASK_NONE; /*specify cpus' mask*/

#define show_debug(fmt, x...) \
	do { \
		if (debug_enable) \
			pr_debug(fmt, ##x); \
	} while (0)

DEFINE_MUTEX(cl_mlock);
static inline void cl_lock(const char *tag)
{
	mutex_lock(&cl_mlock);
}

static inline void cl_unlock(const char *tag)
{
	mutex_unlock(&cl_mlock);
}

enum {
	ULOAD_STATE_HIGH = 1,
	ULOAD_STATE_MID,
	ULOAD_STATE_LOW,
};

static struct miscdevice cpu_loading_object;

/*default setting*/
static void init_cpu_loading_value(void)
{

	cl_lock(__func__);

	/*default setting*/
	onoff = 0;
	polling_sec = 10;
	polling_ms = 10000;
	over_threshold = 85;
	specify_overThrhld = 85;
	specify_cpus = 0;
	nr_cpus = num_possible_cpus();
	under_threshold = 20;
	uevent_enable = 1;
	debug_enable = 0;
	curr_cpu_loading = 0;
	state = ULOAD_STATE_MID;
	cl_unlock(__func__);
}

void perfmgr_trace_printk(char *module, char *string)
{
	preempt_disable();
	trace_cpu_loading(module, string);
	preempt_enable();
}

void perfmgr_trace_log(char *module, const char *fmt, ...)
{
	char log[256];
	va_list args;
	int len;

	va_start(args, fmt);
	len = vsnprintf(log, sizeof(log), fmt, args);

	if (unlikely(len == 256))
		log[255] = '\0';
	va_end(args);
	trace_cpu_loading(module, log);
}

static bool sentuevent(const char *src)
{
	int ret;
	char *envp[2];
	char event_string[32];

	envp[0] = event_string;
	envp[1] = NULL;


	/*send uevent*/
	if (uevent_enable) {
		strlcpy(event_string, src, 32);
		if (event_string[0] == '\0') { /*string is null*/

			perfmgr_trace_printk("cpu_loading", "string is null");
			return false;
		}

		ret = kobject_uevent_env(
				&cpu_loading_object.this_device->kobj,
				KOBJ_CHANGE, envp);
		if (ret != 0) {
			perfmgr_trace_printk("cpu_loading", "uevent failed");
			show_debug("uevent failed");

			return false;
		}

		show_debug("sent uevent success:%s", src);

		perfmgr_trace_log("cpu_loading",
				"sent uevent success:%s", src);
	}
	return true;
}

/*update info*/
static void calculat_loading_callback(int mask_loading, int loading)
{
	cl_lock(__func__);

	show_debug("update cpu_loading");
	perfmgr_trace_log("cpu_loading",
			"loading:%d mask_loading:%d curr_cpu_loading:%d previous state:%d",
			loading, mask_loading, curr_cpu_loading, state);

	show_debug("loading:%d mask_loading:%d curr_cpu_loading:%d previous state:%d\n",
			loading, mask_loading, curr_cpu_loading, state);

	if (loading > over_threshold) {
		state = ULOAD_STATE_HIGH;
		sentuevent("over=1");
	} else if (loading > under_threshold) {
		state = ULOAD_STATE_MID;
		if (specify_cpus != 0
				&& mask_loading >= specify_overThrhld) {
			sentuevent("specify_over=1");
		}
	} else {
		state = ULOAD_STATE_LOW;
	}

	show_debug("current state:%d\n", state);
	curr_cpu_loading = loading;
	cl_unlock(__func__);
}

static void start_calculate_loading(void)
{
	int ret_reg;
	int poll_ms;
	unsigned int i, start, end;

	poll_ms = polling_ms;

	cl_unlock(__func__);

	if (specify_cpus != 0) {
		cpumask_clear(&specify_cpu_mask);

		start = specify_cpus%10;
		end = specify_cpus/10;

		for (i = start; i <= end; i++)
			cpumask_set_cpu(i, &specify_cpu_mask);
		ret_reg = reg_loading_tracking(calculat_loading_callback, poll_ms,
				&specify_cpu_mask);
	} else {
		ret_reg = reg_loading_tracking(calculat_loading_callback, poll_ms,
				cpu_possible_mask);
	}

	show_debug("ret_reg:%d\n", ret_reg);

	cl_lock(__func__);
	if (!ret_reg)
		curr_cpu_loading = REG_SUCCESS;
	else
		curr_cpu_loading = REG_FAIL;
	perfmgr_trace_log("cpu_loading", "ret_reg:%d\n", ret_reg);
	state = ULOAD_STATE_MID;
}

static void stop_calculate_loading(void)
{
	int ret_unreg;

	cl_unlock(__func__);
	ret_unreg = unreg_loading_tracking(calculat_loading_callback);

	cl_lock(__func__);
	show_debug("ret_unreg:%d\n", ret_unreg);
	if (!ret_unreg)
		curr_cpu_loading = UNREG_SUCCESS;
	else
		curr_cpu_loading = UNREG_FAIL;
	perfmgr_trace_log("cpu_loading", "ret_unreg:%d\n", ret_unreg);
}

static int perfmgr_proc_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;

	return 0;
}

static ssize_t perfmgr_poltime_secs_proc_write(struct file *filp,
		const char __user *ubuf, size_t cnt, loff_t *pos)
{
	int val, ret;

	ret = kstrtoint_from_user(ubuf, cnt, 10, &val);

	if (ret != 0)
		return ret;

	if (val <= 0 || val >= 10000)
		return -EINVAL;

	cl_lock(__func__);

	polling_sec = val;
	polling_ms = val * 1000;

	pr_debug("c polling_sec :%d\n", polling_sec);
	if (onoff) {
		stop_calculate_loading();
		start_calculate_loading();
	}

	cl_unlock(__func__);

	return cnt;
}

static ssize_t perfmgr_poltime_secs_proc_show(struct file *file,
		char __user *ubuf, size_t count, loff_t *ppos)
{
	int n = 0;
	char buffer[512];

	if (*ppos != 0)
		goto out;

	n = scnprintf(buffer, 512, "%u", polling_sec);
out:
	if (n < 0)
		return -EINVAL;

	return simple_read_from_buffer(ubuf, count, ppos, buffer, n);
}

static ssize_t perfmgr_poltime_nsecs_proc_write(struct file *filp,
		const char __user *ubuf, size_t cnt, loff_t *pos)
{
	int val, ret;

	ret = kstrtoint_from_user(ubuf, cnt, 10, &val);

	if (ret != 0)
		return ret;

	if (val <= 0)
		return -EINVAL;

	pr_debug("c polling_nsec :%d\n", val);

	return cnt;
}

static ssize_t perfmgr_poltime_nsecs_proc_show(struct file *file,
		char __user *ubuf, size_t count, loff_t *ppos)
{
	int n = 0;
	char buffer[512];

	if (*ppos != 0)
		goto out;

	n = scnprintf(buffer, 512, "%u", 0);
out:
	if (n < 0)
		return -EINVAL;

	return simple_read_from_buffer(ubuf, count, ppos, buffer, n);
}

static ssize_t perfmgr_onoff_proc_write(struct file *filp,
		const char __user *ubuf, size_t cnt, loff_t *pos)
{
	int val, ret;

	ret = kstrtoint_from_user(ubuf, cnt, 10, &val);

	if (ret != 0)
		return ret;

	if (val == onoff)
		return -EINVAL;

	if (val > 1 || 0 > val)
		return -EINVAL;

	cl_lock(__func__);

	onoff = val;
	if (onoff)
		start_calculate_loading();
	else
		stop_calculate_loading();

	pr_debug("c onoff :%d\n", onoff);
	cl_unlock(__func__);
	return cnt;
}

static ssize_t perfmgr_onoff_proc_show(struct file *file,
		char __user *ubuf, size_t count, loff_t *ppos)
{
	int n = 0;
	char buffer[512];

	if (*ppos != 0)
		goto out;

	n = scnprintf(buffer, 512, "%u", onoff);
out:
	if (n < 0)
		return -EINVAL;

	return simple_read_from_buffer(ubuf, count, ppos, buffer, n);
}

static ssize_t perfmgr_underThrhld_proc_show(struct file *file,
		char __user *ubuf, size_t count, loff_t *ppos)
{
	int n = 0;
	char buffer[512];

	if (*ppos != 0)
		goto out;

	n = scnprintf(buffer, 512, "%u", under_threshold);
out:
	if (n < 0)
		return -EINVAL;

	return simple_read_from_buffer(ubuf, count, ppos, buffer, n);
}

static ssize_t perfmgr_underThrhld_proc_write(
		struct file *filp, const char *ubuf,
		size_t cnt, loff_t *data)
{
	int val, ret;

	ret = kstrtoint_from_user(ubuf, cnt, 10, &val);

	if (ret != 0)
		return ret;

	cl_lock(__func__);

	if (val < 1 || over_threshold <= val) {
		cl_unlock(__func__);
		return -EINVAL;
	}

	under_threshold = val;

	pr_debug("c under_threshold :%d\n", under_threshold);
	if (onoff) {
		stop_calculate_loading();
		start_calculate_loading();
	}

	cl_unlock(__func__);

	return cnt;
}

static ssize_t perfmgr_overThrhld_proc_show(struct file *file,
		char __user *ubuf, size_t count, loff_t *ppos)
{
	int n = 0;
	char buffer[512];

	if (*ppos != 0)
		goto out;

	n = scnprintf(buffer, 512, "%u", over_threshold);
out:
	if (n < 0)
		return -EINVAL;

	return simple_read_from_buffer(ubuf, count, ppos, buffer, n);
}

static ssize_t perfmgr_overThrhld_proc_write(
		struct file *filp, const char *ubuf,
		size_t cnt, loff_t *data)
{
	int val, ret;

	ret = kstrtoint_from_user(ubuf, cnt, 10, &val);

	if (ret != 0)
		return ret;

	cl_lock(__func__);

	if (val > 99 || under_threshold >= val) {
		cl_unlock(__func__);
		return -EINVAL;
	}

	over_threshold = val;
	pr_debug("c over_threshold :%d\n", over_threshold);
	if (onoff) {
		stop_calculate_loading();
		start_calculate_loading();
	}
	cl_unlock(__func__);

	return cnt;
}

static ssize_t perfmgr_specify_cpus_proc_show(struct file *file,
		char __user *ubuf, size_t count, loff_t *ppos)
{
	int n = 0;
	char buffer[512];

	if (*ppos != 0)
		goto out;

	n = scnprintf(buffer, 512, "%u", specify_cpus);
out:
	if (n < 0)
		return -EINVAL;

	return simple_read_from_buffer(ubuf, count, ppos, buffer, n);
}

static ssize_t perfmgr_specify_cpus_proc_write(
		struct file *filp, const char *ubuf,
		size_t cnt, loff_t *data)
{
	int val, ret;

	ret = kstrtoint_from_user(ubuf, cnt, 10, &val);

	if (ret != 0)
		return ret;

	if (val < 0 || val > 76543210)
		return -EINVAL;

	cl_lock(__func__);

	specify_cpus = val;
	pr_debug("c specify_cpus :%d\n", specify_cpus);
	if (onoff) {
		stop_calculate_loading();
		start_calculate_loading();
	}
	cl_unlock(__func__);

	return cnt;

}

static ssize_t perfmgr_specify_overThrhld_proc_show(struct file *file,
		char __user *ubuf, size_t count, loff_t *ppos)
{
	int n = 0;
	char buffer[512];

	if (*ppos != 0)
		goto out;

	n = scnprintf(buffer, 512, "%u", specify_overThrhld);
out:
	if (n < 0)
		return -EINVAL;

	return simple_read_from_buffer(ubuf, count, ppos, buffer, n);
}

static ssize_t perfmgr_specify_overThrhld_proc_write(
		struct file *filp, const char *ubuf,
		size_t cnt, loff_t *data)
{
	int val, ret;

	ret = kstrtoint_from_user(ubuf, cnt, 10, &val);

	if (ret != 0)
		return ret;

	if (val < 0 || val > 100)
		return -EINVAL;

	cl_lock(__func__);

	specify_overThrhld = val;
	pr_debug("c specify_overThrhld :%d\n", specify_overThrhld);
	if (onoff) {
		stop_calculate_loading();
		start_calculate_loading();
	}
	cl_unlock(__func__);

	return cnt;

}

static ssize_t perfmgr_uevent_enable_proc_show(struct file *file,
		char __user *ubuf, size_t count, loff_t *ppos)
{
	int n = 0;
	char buffer[512];

	if (*ppos != 0)
		goto out;

	n = scnprintf(buffer, 512, "%u", uevent_enable);
out:
	if (n < 0)
		return -EINVAL;

	return simple_read_from_buffer(ubuf, count, ppos, buffer, n);
}

static ssize_t perfmgr_uevent_enable_proc_write(
		struct file *filp, const char *ubuf,
		size_t cnt, loff_t *data)
{
	int val, ret;

	ret = kstrtoint_from_user(ubuf, cnt, 10, &val);

	if (ret != 0)
		return ret;


	if (val > 1 || 0 > val)
		return -EINVAL;

	cl_lock(__func__);

	uevent_enable = val;
	pr_debug("c uevent_enable :%d\n", uevent_enable);
	if (onoff) {
		stop_calculate_loading();
		start_calculate_loading();
	}
	cl_unlock(__func__);

	return cnt;

}

static ssize_t perfmgr_curr_cpu_loading_proc_show(struct file *file,
		char __user *ubuf, size_t count, loff_t *ppos)
{
	int n = 0;
	char buffer[512];

	if (*ppos != 0)
		goto out;

	n = scnprintf(buffer, 512, "%u", curr_cpu_loading);
out:
	if (n < 0)
		return -EINVAL;

	return simple_read_from_buffer(ubuf, count, ppos, buffer, n);
}

static ssize_t perfmgr_debug_enable_proc_show(struct file *file,
		char __user *ubuf, size_t count, loff_t *ppos)
{
	int n = 0;
	char buffer[512];

	if (*ppos != 0)
		goto out;

	n = scnprintf(buffer, 512, "%u", debug_enable);
out:
	if (n < 0)
		return -EINVAL;

	return simple_read_from_buffer(ubuf, count, ppos, buffer, n);
}

static ssize_t perfmgr_debug_enable_proc_write(
		struct file *filp, const char *ubuf,
		size_t cnt, loff_t *data)
{
	int val, ret;

	ret = kstrtoint_from_user(ubuf, cnt, 10, &val);

	if (ret != 0)
		return ret;

	if (val > 1 || 0 > val)
		return -EINVAL;

	cl_lock(__func__);

	debug_enable = val;

	cl_unlock(__func__);

	return cnt;

}

PROC_FOPS_RW(poltime_secs);
PROC_FOPS_RW(poltime_nsecs);
PROC_FOPS_RW(onoff);
PROC_FOPS_RW(overThrhld);
PROC_FOPS_RW(specify_cpus);
PROC_FOPS_RW(specify_overThrhld);
PROC_FOPS_RW(underThrhld);
PROC_FOPS_RW(uevent_enable);
PROC_FOPS_RW(debug_enable);
PROC_FOPS_RO(curr_cpu_loading);

static int init_cpu_loading_kobj(void)
{
	int ret = 0;

	/* dev init */

	cpu_loading_object.name = "cpu_loading";
	cpu_loading_object.minor = MISC_DYNAMIC_MINOR;
	ret = misc_register(&cpu_loading_object);
	if (ret) {
		pr_debug("misc_register error:%d\n", ret);
		return ret;
	}

	ret = kobject_uevent(
			&cpu_loading_object.this_device->kobj, KOBJ_ADD);

	if (ret) {
		misc_deregister(&cpu_loading_object);
		pr_debug("uevent creat fail:%d\n", ret);
		return ret;
	}

	return ret;

}

static int __init uload_init(void)
{
	struct proc_dir_entry *lt_dir = NULL;
	struct proc_dir_entry *parent = NULL;
	int ret;
	size_t i;

	struct pentry {
		const char *name;
		const struct proc_ops *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(poltime_secs),
		PROC_ENTRY(poltime_nsecs),
		PROC_ENTRY(onoff),
		PROC_ENTRY(overThrhld),
		PROC_ENTRY(specify_cpus),
		PROC_ENTRY(specify_overThrhld),
		PROC_ENTRY(underThrhld),
		PROC_ENTRY(uevent_enable),
		PROC_ENTRY(curr_cpu_loading),
		PROC_ENTRY(debug_enable),
	};

	lt_dir = proc_mkdir("cpu_loading", parent);

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create(entries[i].name, 0644,
					lt_dir, entries[i].fops)) {
			pr_debug("%s(), lt_dir%s failed\n",
					__func__, entries[i].name);
			ret = -EINVAL;
			return ret;
		}
	}


	/* dev init */
	ret = init_cpu_loading_kobj();
	if (ret) {
		pr_debug("init cpu_loading_kobj failed");
		return ret;
	}

	init_cpu_loading_value();

	return 0;
}

static void __exit uload_exit(void){}

module_init(uload_init);
module_exit(uload_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek ULOAD_IND");
MODULE_AUTHOR("MediaTek Inc.");
