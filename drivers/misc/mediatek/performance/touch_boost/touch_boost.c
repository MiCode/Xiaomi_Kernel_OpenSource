// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#define pr_fmt(fmt) "[touch_boost]"fmt

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/string.h>
#include <linux/notifier.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/input.h>
#include <linux/cpumask.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kobject.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/preempt.h>
#include <linux/trace_events.h>
#include <linux/kernel.h>
#include <linux/cpufreq.h>
#include <linux/pm_qos.h>
#include <linux/sort.h>
#include "perf_ioctl.h"
#include "touch_boost.h"

#define PROC_FOPS_RW(name) \
static const struct proc_ops perfmgr_ ## name ## _proc_fops = { \
	.proc_read	= perfmgr_ ## name ## _proc_show, \
	.proc_write	= perfmgr_ ## name ## _proc_write,\
	.proc_open	= perfmgr_proc_open, \
}

#define PROC_FOPS_RO(name) \
static const struct proc_ops perfmgr_ ## name ## _proc_fops = { \
	.proc_read	= perfmgr_ ## name ## _proc_show, \
	.proc_open	= perfmgr_proc_open, \
}

#define PROC_ENTRY(name) {__stringify(name), &perfmgr_ ## name ## _proc_fops}

#define show_debug(fmt, x...) \
	do { \
		if (debug_enable) \
			pr_debug(fmt, ##x); \
	} while (0)

static void notify_touch_up_timeout(struct work_struct *work);
static DECLARE_WORK(mt_touch_timeout_work, (void *) notify_touch_up_timeout);
static struct workqueue_struct *wq;
static struct boost ktchboost;
static int policy_num;
static int *opp_count;
static unsigned int **opp_table;
static int *_opp_cnt;
static unsigned int **cpu_opp_tbl;
static DEFINE_MUTEX(cpu_ctrl_lock);
static struct _cpufreq freq_to_set[CLUSTER_MAX];
struct freq_qos_request *freq_min_request;
struct freq_qos_request *freq_max_request;
static int enable = 1;
static int boost_ta;
static int boost_opp_cluster[CLUSTER_MAX];
static int deboost_when_render = 1;
static int force_stop_boost;
static long long active_time = TOUCH_FSTB_ACTIVE_MS;
static long long boost_duration = TOUCH_TIMEOUT_MS;
static struct hrtimer hrt1;
static int my_tid = -1;

static noinline int tracing_mark_write(const char *buf)
{
	trace_printk(buf);
	return 0;
}

static void _cpu_ctrl_systrace(int val, const char *fmt, ...)
{
	char log[256];
	va_list args;
	int len;
	char buf[256];

	memset(log, ' ', sizeof(log));
	va_start(args, fmt);
	len = vsnprintf(log, sizeof(log), fmt, args);
	va_end(args);

	if (unlikely(len < 0))
		return;
	else if (unlikely(len == 256))
		log[255] = '\0';

	len = snprintf(buf, sizeof(buf), "C|%d|%s|%d\n", my_tid, log, val);
	if (unlikely(len < 0))
		return;
	else if (unlikely(len == 256))
		buf[255] = '\0';

	tracing_mark_write(buf);
}

int _update_userlimit_cpufreq_min(int cid, int value)
{
	int ret = -1;

	ret = freq_qos_update_request(&(freq_min_request[cid]), value);
	_cpu_ctrl_systrace(value, "tchbst_freq_c%d", cid);

	return ret;
}

static void notify_touch_up_timeout(struct work_struct *work)
{
	int i = 0, ret = -1;

	for (i = 0 ; i < policy_num ; i++)
		ret = _update_userlimit_cpufreq_min(i, 0);
}

void enable_touch_boost_timer(void)
{
	ktime_t ktime;

	ktime = ktime_set(0, boost_duration * 1000000); // unit: nsec
	hrtimer_start(&hrt1, ktime, HRTIMER_MODE_REL);
}

void disable_touch_boost_timer(void)
{
	hrtimer_cancel(&hrt1);
}

enum hrtimer_restart mt_touch_timeout(struct hrtimer *timer)
{
	if (wq)
		queue_work(wq, &mt_touch_timeout_work);

	return HRTIMER_NORESTART;
}

void _force_stop_touch_boost(void)
{
	int i = 0, ret = -1;

	disable_touch_boost_timer();
	for (i = 0 ; i < policy_num ; i++)
		ret = _update_userlimit_cpufreq_min(i, 0);

	force_stop_boost = 0;
}

void touch_boost(void)
{
	int i = 0, ret = -1, isact = 0;

	isact = fpsgo_get_fstb_active_fp(active_time * 1000);
	_cpu_ctrl_systrace(isact, "is_fstb_active");
	if (isact || !enable)
		return;

	disable_touch_boost_timer();
	enable_touch_boost_timer();

	for (i = 0 ; i < policy_num ; i++) {
		if (boost_opp_cluster[i] >= 0) {
			freq_to_set[i].min = cpu_opp_tbl[i][boost_opp_cluster[i]];
			ret = _update_userlimit_cpufreq_min(i, freq_to_set[i].min);
		}
	}
}

static int ktchboost_thread(void *ptr)
{
	int event;

	my_tid = current->pid;

	while (!kthread_should_stop()) {
		while (!atomic_read(&ktchboost.event))
			wait_event(ktchboost.wq, atomic_read(&ktchboost.event));

		atomic_dec(&ktchboost.event);
		event = ktchboost.touch_event;
		_cpu_ctrl_systrace(event, "touch_down");
		touch_boost();
	}
	return 0;
}

static ssize_t perfmgr_deboost_when_render_proc_write(struct file *filp,
		const char *ubuf, size_t cnt, loff_t *data)
{
	char buf[64];
	int value;
	int ret;

	if (cnt >= sizeof(buf) || copy_from_user(buf, ubuf, cnt))
		return -EINVAL;

	buf[cnt] = 0;
	ret = kstrtoint(buf, 10, &value);
	if (ret < 0)
		return ret;

	deboost_when_render = value;

	return cnt;
}

static ssize_t perfmgr_deboost_when_render_proc_show(struct file *file,
		char __user *ubuf, size_t count, loff_t *ppos)
{
	int n = 0;
	char buffer[64];

	if (*ppos != 0)
		goto out;
	n = scnprintf(buffer, 64, "%d\n", deboost_when_render);
out:
	if (n < 0)
		return -EINVAL;

	return simple_read_from_buffer(ubuf, count, ppos, buffer, n);
}

static ssize_t perfmgr_force_stop_boost_proc_write(struct file *filp,
		const char *ubuf, size_t cnt, loff_t *data)
{
	char buf[64];
	int value;
	int ret;

	if (cnt >= sizeof(buf) || copy_from_user(buf, ubuf, cnt))
		return -EINVAL;

	buf[cnt] = 0;
	ret = kstrtoint(buf, 10, &value);
	if (ret < 0)
		return ret;

	force_stop_boost = value;
	if (force_stop_boost == 1)
		_force_stop_touch_boost();

	return cnt;
}

static ssize_t perfmgr_force_stop_boost_proc_show(struct file *file,
		char __user *ubuf, size_t count, loff_t *ppos)
{
	int n = 0;
	char buffer[64];

	if (*ppos != 0)
		goto out;
	n = scnprintf(buffer, 64, "%d\n", force_stop_boost);
out:
	if (n < 0)
		return -EINVAL;

	return simple_read_from_buffer(ubuf, count, ppos, buffer, n);
}

static ssize_t perfmgr_boost_opp_cluster_0_proc_write(struct file *filp,
		const char *ubuf, size_t cnt, loff_t *data)
{
	char buf[64];
	int value;
	int ret;

	if (cnt >= sizeof(buf) || copy_from_user(buf, ubuf, cnt))
		return -EINVAL;

	buf[cnt] = 0;
	ret = kstrtoint(buf, 10, &value);
	if (ret < 0)
		return ret;

	if (policy_num >= 1) {
		if (value >= 0 && value < opp_count[0])
			boost_opp_cluster[0] = value;
		else
			boost_opp_cluster[0] = opp_count[0]-1;
	}

	return cnt;
}

static ssize_t perfmgr_boost_opp_cluster_0_proc_show(struct file *file,
		char __user *ubuf, size_t count, loff_t *ppos)
{
	int n = 0;
	char buffer[64];

	if (*ppos != 0)
		goto out;
	n = scnprintf(buffer, 64, "%d\n", boost_opp_cluster[0]);
out:
	if (n < 0)
		return -EINVAL;

	return simple_read_from_buffer(ubuf, count, ppos, buffer, n);
}

static ssize_t perfmgr_boost_opp_cluster_1_proc_write(struct file *filp,
		const char *ubuf, size_t cnt, loff_t *data)
{
	char buf[64];
	int value;
	int ret;

	if (cnt >= sizeof(buf) || copy_from_user(buf, ubuf, cnt))
		return -EINVAL;

	buf[cnt] = 0;
	ret = kstrtoint(buf, 10, &value);
	if (ret < 0)
		return ret;

	if (policy_num >= 2) {
		if (value >= 0 && value < opp_count[1])
			boost_opp_cluster[1] = value;
		else
			boost_opp_cluster[1] = opp_count[1]-1;
	}

	return cnt;
}

static ssize_t perfmgr_boost_opp_cluster_1_proc_show(struct file *file,
		char __user *ubuf, size_t count, loff_t *ppos)
{
	int n = 0;
	char buffer[64];

	if (*ppos != 0)
		goto out;
	n = scnprintf(buffer, 64, "%d\n", boost_opp_cluster[1]);
out:
	if (n < 0)
		return -EINVAL;

	return simple_read_from_buffer(ubuf, count, ppos, buffer, n);
}

static ssize_t perfmgr_boost_opp_cluster_2_proc_write(struct file *filp,
		const char *ubuf, size_t cnt, loff_t *data)
{
	char buf[64];
	int value;
	int ret;

	if (cnt >= sizeof(buf) || copy_from_user(buf, ubuf, cnt))
		return -EINVAL;

	buf[cnt] = 0;
	ret = kstrtoint(buf, 10, &value);
	if (ret < 0)
		return ret;

	if (policy_num >= 3) {
		if (value >= 0 && value < opp_count[2])
			boost_opp_cluster[2] = value;
		else
			boost_opp_cluster[2] = opp_count[2]-1;
	}

	return cnt;
}

static ssize_t perfmgr_boost_opp_cluster_2_proc_show(struct file *file,
		char __user *ubuf, size_t count, loff_t *ppos)
{
	int n = 0;
	char buffer[64];

	if (*ppos != 0)
		goto out;
	n = scnprintf(buffer, 64, "%d\n", boost_opp_cluster[2]);
out:
	if (n < 0)
		return -EINVAL;

	return simple_read_from_buffer(ubuf, count, ppos, buffer, n);
}

static ssize_t perfmgr_active_time_proc_write(struct file *filp,
		const char *ubuf, size_t cnt, loff_t *data)
{
	char buf[64];
	int value;
	int ret;

	if (cnt >= sizeof(buf) || copy_from_user(buf, ubuf, cnt))
		return -EINVAL;

	buf[cnt] = 0;
	ret = kstrtoint(buf, 10, &value);
	if (ret < 0)
		return ret;

	active_time = value;

	return cnt;
}

static ssize_t perfmgr_active_time_proc_show(struct file *file,
		char __user *ubuf, size_t count, loff_t *ppos)
{
	int n = 0;
	char buffer[64];

	if (*ppos != 0)
		goto out;
	n = scnprintf(buffer, 64, "%d\n", active_time);
out:
	if (n < 0)
		return -EINVAL;

	return simple_read_from_buffer(ubuf, count, ppos, buffer, n);
}

static ssize_t perfmgr_boost_ta_proc_write(struct file *filp,
		const char *ubuf, size_t cnt, loff_t *data)
{
	char buf[64];
	int value;
	int ret;

	if (cnt >= sizeof(buf) || copy_from_user(buf, ubuf, cnt))
		return -EINVAL;

	buf[cnt] = 0;
	ret = kstrtoint(buf, 10, &value);
	if (ret < 0)
		return ret;

	boost_ta = value;

	return cnt;
}

static ssize_t perfmgr_boost_ta_proc_show(struct file *file,
		char __user *ubuf, size_t count, loff_t *ppos)
{
	int n = 0;
	char buffer[64];

	if (*ppos != 0)
		goto out;
	n = scnprintf(buffer, 64, "%d\n", boost_ta);
out:
	if (n < 0)
		return -EINVAL;

	return simple_read_from_buffer(ubuf, count, ppos, buffer, n);
}

static ssize_t perfmgr_boost_duration_proc_write(struct file *filp,
		const char *ubuf, size_t cnt, loff_t *data)
{
	char buf[64];
	int value;
	int ret;

	if (cnt >= sizeof(buf) || copy_from_user(buf, ubuf, cnt))
		return -EINVAL;

	buf[cnt] = 0;
	ret = kstrtoint(buf, 10, &value);
	if (ret < 0)
		return ret;

	boost_duration = value;

	return cnt;
}

static ssize_t perfmgr_boost_duration_proc_show(struct file *file,
		char __user *ubuf, size_t count, loff_t *ppos)
{
	int n = 0;
	char buffer[64];

	if (*ppos != 0)
		goto out;
	n = scnprintf(buffer, 64, "%d\n", boost_duration);
out:
	if (n < 0)
		return -EINVAL;

	return simple_read_from_buffer(ubuf, count, ppos, buffer, n);
}

static ssize_t perfmgr_enable_proc_write(struct file *filp,
		const char *ubuf, size_t cnt, loff_t *data)
{
	char buf[64];
	int value;
	int ret;

	if (cnt >= sizeof(buf) || copy_from_user(buf, ubuf, cnt))
		return -EINVAL;

	buf[cnt] = 0;
	ret = kstrtoint(buf, 10, &value);
	if (ret < 0)
		return ret;

	enable = value;

	return cnt;
}

static ssize_t perfmgr_enable_proc_show(struct file *file,
		char __user *ubuf, size_t count, loff_t *ppos)
{
	int n = 0;
	char buffer[64];

	if (*ppos != 0)
		goto out;
	n = scnprintf(buffer, 64, "%d\n", enable);
out:
	if (n < 0)
		return -EINVAL;

	return simple_read_from_buffer(ubuf, count, ppos, buffer, n);
}

static void dbs_input_event(struct input_handle *handle, unsigned int type,
		unsigned int code, int value)
{
	unsigned long flags;

	if (!enable)
		return;

	if ((type == EV_KEY) && (code == BTN_TOUCH)) {
		pr_debug("input cb, type:%d, code:%d, value:%d\n",
				type, code, value);
		spin_lock_irqsave(&ktchboost.touch_lock, flags);
		ktchboost.touch_event = value;
		spin_unlock_irqrestore(&ktchboost.touch_lock, flags);

		atomic_inc(&ktchboost.event);
		wake_up(&ktchboost.wq);
	}
}

static int dbs_input_connect(struct input_handler *handler,
		struct input_dev *dev,
		const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);

	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "perfmgr";

	error = input_register_handle(handle);

	if (error)
		goto err2;

	error = input_open_device(handle);

	if (error)
		goto err1;

	return 0;
err1:
	input_unregister_handle(handle);
err2:
	kfree(handle);
	return error;
}

static void dbs_input_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id dbs_ids[] = {
	{.driver_info = 1},
	{},
};

static struct input_handler dbs_input_handler = {
	.event = dbs_input_event,
	.connect = dbs_input_connect,
	.disconnect = dbs_input_disconnect,
	.name = "cpufreq_ond",
	.id_table = dbs_ids,
};

char *perfmgr_copy_from_user_for_proc(const char __user *buffer, size_t count)
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

int cmp_uint(const void *a, const void *b)
{
	return *(unsigned int *)b - *(unsigned int *)a;
}

void cpu_policy_init(void)
{
	int cpu;
	int num = 0, count;
	struct cpufreq_policy *policy;
	struct cpufreq_frequency_table *pos;

	for_each_possible_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);

		if (policy) {
			pr_info("%s, policy[%d]: first:%d, min:%d, max:%d",
				__func__, num, cpu, policy->min, policy->max);

			num++;
			cpu = cpumask_last(policy->related_cpus);
			cpufreq_cpu_put(policy);
		}
	}

	policy_num = num;

	if (policy_num == 0) {
		pr_info("%s, no policy", __func__);
		return;
	}

	opp_count = kcalloc(policy_num, sizeof(int), GFP_KERNEL);
	opp_table = kcalloc(policy_num, sizeof(unsigned int *), GFP_KERNEL);
	if (opp_count == NULL || opp_table == NULL)
		return;

	num = 0;
	for_each_possible_cpu(cpu) {
		if (num >= policy_num)
			break;

		policy = cpufreq_cpu_get(cpu);

		if (!policy)
			continue;

		/* calc opp count */
		count = 0;
		cpufreq_for_each_entry(pos, policy->freq_table) {
			count++;
		}
		opp_count[num] = count;
		opp_table[num] = kcalloc(count, sizeof(unsigned int), GFP_KERNEL);
		count = 0;
		cpufreq_for_each_entry(pos, policy->freq_table) {
			opp_table[num][count] = pos->frequency;
			count++;
		}

		sort(opp_table[num], opp_count[num], sizeof(unsigned int), cmp_uint, NULL);

		num++;
		cpu = cpumask_last(policy->related_cpus);
		cpufreq_cpu_put(policy);
	}
}

int get_cpu_opp_info(int **opp_cnt, unsigned int ***opp_tbl)
{
	int i, j;

	if (policy_num <= 0)
		return -EFAULT;

	*opp_cnt = kcalloc(policy_num, sizeof(int), GFP_KERNEL);
	*opp_tbl = kcalloc(policy_num, sizeof(unsigned int *), GFP_KERNEL);

	if (*opp_cnt == NULL || *opp_tbl == NULL)
		return -1;

	for (i = 0; i < policy_num; i++) {
		(*opp_cnt)[i] = opp_count[i];
		(*opp_tbl)[i] = kcalloc(opp_count[i], sizeof(unsigned int), GFP_KERNEL);

		for (j = 0; j < opp_count[i]; j++)
			(*opp_tbl)[i][j] = opp_table[i][j];
	}

	return 0;
}

int get_cpu_topology(void)
{
	if (get_cpu_opp_info(&_opp_cnt, &cpu_opp_tbl) < 0)
		return -EFAULT;

	return 0;
}

static int perfmgr_proc_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;

	return 0;
}


PROC_FOPS_RW(enable);
PROC_FOPS_RW(boost_duration);
PROC_FOPS_RW(boost_ta);
PROC_FOPS_RW(active_time);
PROC_FOPS_RW(boost_opp_cluster_0);
PROC_FOPS_RW(boost_opp_cluster_1);
PROC_FOPS_RW(boost_opp_cluster_2);
PROC_FOPS_RW(force_stop_boost);
PROC_FOPS_RW(deboost_when_render);


static int __init touch_boost_init(void)
{
	int cpu_num = 0, num = 0, cpu = 0, i = 0, handle = 0, ret = 0;
	struct proc_dir_entry *parent = NULL;
	struct proc_dir_entry *lt_dir = NULL;
	struct cpufreq_policy *policy;
	struct pentry {
		const char *name;
		const struct proc_ops *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(enable),
		PROC_ENTRY(boost_duration),
		PROC_ENTRY(boost_ta),
		PROC_ENTRY(active_time),
		PROC_ENTRY(boost_opp_cluster_0),
		PROC_ENTRY(boost_opp_cluster_1),
		PROC_ENTRY(boost_opp_cluster_2),
		PROC_ENTRY(force_stop_boost),
		PROC_ENTRY(deboost_when_render),
	};

	lt_dir = proc_mkdir("touch_boost", parent);
	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create(entries[i].name, 0644, lt_dir, entries[i].fops)) {
			pr_info("%s(), lt_dir%s failed\n", __func__, entries[i].name);
			ret = -EINVAL;
			return ret;
		}
	}

	wq = create_singlethread_workqueue("mt_usrtch__work");
	if (!wq) {
		pr_debug("work create fail\n");
		return -ENOMEM;
	}

	hrtimer_init(&hrt1, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hrt1.function = &mt_touch_timeout;

	spin_lock_init(&ktchboost.touch_lock);
	init_waitqueue_head(&ktchboost.wq);
	atomic_set(&ktchboost.event, 0);
	ktchboost.thread = (struct task_struct *)kthread_run(ktchboost_thread,
			&ktchboost, "k_touch_handler");
	if (IS_ERR(ktchboost.thread))
		return -EINVAL;

	handle = input_register_handler(&dbs_input_handler);

	for_each_possible_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);

		if (policy) {
			pr_info("%s, policy[%d]: first:%d, min:%d, max:%d",
				__func__, cpu_num, cpu, policy->min, policy->max);

			cpu_num++;
			cpu = cpumask_last(policy->related_cpus);
			cpufreq_cpu_put(policy);
		}
	}
	policy_num = cpu_num;
	pr_info("%s, cpu_num:%d\n", __func__, policy_num);
	if (policy_num == 0) {
		pr_info("%s, no cpu policy (policy_num=%d)\n", __func__, policy_num);
		return 0;
	}

	cpu_policy_init();

	if (get_cpu_topology() < 0) {
		kvfree(opp_count);
		kvfree(opp_table);
		return -EFAULT;
	}

	for (i = 0; i < policy_num; i++) {
		freq_to_set[i].min = 0;
		freq_to_set[i].max = cpu_opp_tbl[i][0];
		boost_opp_cluster[i] = -1;
	}

	freq_min_request = kcalloc(policy_num, sizeof(struct freq_qos_request), GFP_KERNEL);
	freq_max_request = kcalloc(policy_num, sizeof(struct freq_qos_request), GFP_KERNEL);
	if (freq_min_request == NULL || freq_max_request == NULL)
		return 0;

	num = 0;
	for_each_possible_cpu(cpu) {
		if (num >= policy_num)
			break;

		policy = cpufreq_cpu_get(cpu);

		if (!policy)
			continue;

		freq_qos_add_request(&policy->constraints,
			&(freq_min_request[num]),
			FREQ_QOS_MIN,
			0);
		freq_qos_add_request(&policy->constraints,
			&(freq_max_request[num]),
			FREQ_QOS_MAX,
			cpu_opp_tbl[num][0]);

		num++;
		cpu = cpumask_last(policy->related_cpus);
		cpufreq_cpu_put(policy);
	}

	return 0;
}

static void __exit touch_boost_exit(void)
{
	kvfree(opp_count);
	kvfree(opp_table);
	kvfree(freq_min_request);
	kvfree(freq_max_request);
}

module_init(touch_boost_init);
module_exit(touch_boost_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek TOUCH_BOOST");
MODULE_AUTHOR("MediaTek Inc.");
