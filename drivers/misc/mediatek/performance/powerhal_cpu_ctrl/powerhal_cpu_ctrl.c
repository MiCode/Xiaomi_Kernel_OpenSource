// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/cpumask.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kobject.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>
#include <linux/preempt.h>
#include <linux/trace_events.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/cpufreq.h>
#include <linux/pm_qos.h>
#include <linux/sort.h>
//#include "mtk_perfmgr_internal.h"
/* PROCFS */
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

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#define CLUSTER_MAX 10

typedef struct _cpufreq {
	int min;
	int max;
} _cpufreq;

static int policy_num;
static int *opp_count;
static unsigned int **opp_table;
static int *_opp_cnt;
static unsigned int **cpu_opp_tbl;
static DEFINE_MUTEX(cpu_ctrl_lock);
static struct _cpufreq freq_to_set[CLUSTER_MAX];
struct freq_qos_request *freq_min_request;
struct freq_qos_request *freq_max_request;

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

	len = snprintf(buf, sizeof(buf), "C|%d|%s|%d\n", current->pid, log, val);
	if (unlikely(len < 0))
		return;
	else if (unlikely(len == 256))
		buf[255] = '\0';

	tracing_mark_write(buf);
}

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

int update_userlimit_cpufreq_max(int cid, int value)
{
	int ret = -1;

	ret = freq_qos_update_request(&(freq_max_request[cid]), value);
	_cpu_ctrl_systrace(value, "powerhal_cpu_ctrl c%d Max", cid);

	return ret;
}

int update_userlimit_cpufreq_min(int cid, int value)
{
	int ret = -1;

	ret = freq_qos_update_request(&(freq_min_request[cid]), value);
	_cpu_ctrl_systrace(value, "powerhal_cpu_ctrl c%d min", cid);

	return ret;
}

static int perfmgr_proc_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;

	return 0;
}

static ssize_t perfmgr_perfserv_freq_proc_write(
		struct file *filp, const char *ubuf,
		size_t cnt, loff_t *_data)
{
	int i = 0, data;
	unsigned int arg_num = policy_num * 2;
	char *tok, *tmp;
	char *buf = perfmgr_copy_from_user_for_proc(ubuf, cnt);
	//int ret = 0;
	int update_failed = 0;

	if (!buf) {
		pr_debug("buf is null\n");
		goto out1;
	}

	tmp = buf;
	while ((tok = strsep(&tmp, " ")) != NULL) {
		if (i == arg_num) {
			pr_debug("@%s: number of arguments > %d\n", __func__, arg_num);
			goto out;
		}

		if (kstrtoint(tok, 10, &data)) {
			pr_debug("@%s: invalid input: %s\n", __func__, tok);
			goto out;
		} else {
			if (i % 2) /* max */
				freq_to_set[i/2].max = data;
			else /* min */
				freq_to_set[i/2].min = data;
			i++;
		}
	}

	if (i < arg_num) {
		pr_info("@%s: number of arguments %d < %d\n", __func__, i, arg_num);
	} else {
		for (i = 0; i < policy_num; i++) {
			if ((update_userlimit_cpufreq_max(i, freq_to_set[i].max) < 0) ||
					(update_userlimit_cpufreq_min(i, freq_to_set[i].min) < 0)) {
				pr_info("update cpufreq failed.");
				update_failed = 1;
			}
		}
	}

out:
	free_page((unsigned long)buf);
out1:
	if (update_failed)
		return -1;
	else
		return cnt;
}

static ssize_t perfmgr_perfserv_freq_proc_show(struct file *file,
		char __user *ubuf, size_t count, loff_t *ppos)
{
	int i, n = 0;
	char buffer[512];
	char _buf[64];

	if (*ppos != 0)
		goto out;

	for (i = 0; i < policy_num; i++) {
		scnprintf(_buf, 64, "%d %d ", freq_to_set[i].min, freq_to_set[i].max);
		strncat(buffer, _buf, strlen(_buf));
	}
	n = scnprintf(buffer, 512, "%s\n", buffer);

out:
	if (n < 0)
		return -EINVAL;

	return simple_read_from_buffer(ubuf, count, ppos, buffer, n);
}

PROC_FOPS_RW(perfserv_freq);

static int __init powerhal_cpu_ctrl_init(void)
{
	int cpu_num = 0;
	int num = 0;
	int cpu;
	int i, ret = 0;
	struct proc_dir_entry *lt_dir = NULL;
	struct proc_dir_entry *parent = NULL;
	struct cpufreq_policy *policy;
	struct pentry {
		const char *name;
		const struct proc_ops *fops;
	};
	const struct pentry entries[] = {
		PROC_ENTRY(perfserv_freq),
	};

	lt_dir = proc_mkdir("powerhal_cpu_ctrl", parent);
	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create(entries[i].name, 0644, lt_dir, entries[i].fops)) {
			pr_info("%s(), lt_dir%s failed\n", __func__, entries[i].name);
			ret = -EINVAL;
			return ret;
		}
	}

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

static void __exit powerhal_cpu_ctrl_exit(void)
{
	kvfree(opp_count);
	kvfree(opp_table);
	kvfree(freq_min_request);
	kvfree(freq_max_request);
}

module_init(powerhal_cpu_ctrl_init);
module_exit(powerhal_cpu_ctrl_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek POWERHAL_CPU_CTRL");
MODULE_AUTHOR("MediaTek Inc.");
