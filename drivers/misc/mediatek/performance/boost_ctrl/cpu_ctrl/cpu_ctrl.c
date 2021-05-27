// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt) "[cpu_ctrl]"fmt
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include <mt-plat/cpu_ctrl.h>
#include <mt-plat/mtk_ppm_api.h>
#include "boost_ctrl.h"
#include "mtk_perfmgr_internal.h"

#ifdef CONFIG_MTK_CPU_CTRL_CFP
#include "cpu_ctrl_cfp.h"
#endif

#ifdef CONFIG_TRACING
#include <linux/kallsyms.h>
#include <linux/trace_events.h>
#endif

#include <linux/io.h>

static struct mutex boost_freq;
static struct ppm_limit_data *current_freq;
static struct ppm_limit_data *freq_set[CPU_MAX_KIR];
static int log_enable;
static unsigned long *policy_mask;

#ifdef CONFIG_MTK_CPU_CTRL_CFP
static int cfp_init_ret;
#endif

int powerhal_tid;

/*******************************************/
int update_userlimit_cpu_freq(int kicker, int num_cluster
		, struct cpu_ctrl_data *freq_limit)
{
	struct ppm_limit_data *final_freq;
	int retval = 0;
	int i, j, len = 0, len1 = 0;
	char msg[LOG_BUF_SIZE];
	char msg1[LOG_BUF_SIZE];


	mutex_lock(&boost_freq);

	final_freq = kcalloc(perfmgr_clusters
			, sizeof(struct ppm_limit_data), GFP_KERNEL);
	if (!final_freq) {
		retval = -1;
		perfmgr_trace_printk("cpu_ctrl", "!final_freq\n");
		goto ret_update;
	}
	if (num_cluster != perfmgr_clusters) {
		pr_debug(
				"num_cluster : %d perfmgr_clusters: %d, doesn't match\n",
				num_cluster, perfmgr_clusters);
		retval = -1;
		perfmgr_trace_printk("cpu_ctrl",
			"num_cluster != perfmgr_clusters\n");
		goto ret_update;
	}

	if (kicker < 0 || kicker >= CPU_MAX_KIR) {
		pr_debug("kicker:%d, error\n", kicker);
		retval = -1;
		goto ret_update;
	}

	for_each_perfmgr_clusters(i) {
		final_freq[i].min = -1;
		final_freq[i].max = -1;
	}

	len += snprintf(msg + len, sizeof(msg) - len, "[%d] ", kicker);
	if (len < 0) {
		perfmgr_trace_printk("cpu_ctrl", "return -EIO 1\n");
		mutex_unlock(&boost_freq);
		return -EIO;
	}
	for_each_perfmgr_clusters(i) {
		freq_set[kicker][i].min = freq_limit[i].min >= -1 ?
			freq_limit[i].min : -1;
		freq_set[kicker][i].max = freq_limit[i].max >= -1 ?
			freq_limit[i].max : -1;

		len += snprintf(msg + len, sizeof(msg) - len, "(%d)(%d) ",
		freq_set[kicker][i].min, freq_set[kicker][i].max);
		if (len < 0) {
			perfmgr_trace_printk("cpu_ctrl", "return -EIO 2\n");
			mutex_unlock(&boost_freq);
			return -EIO;
		}

		if (freq_set[kicker][i].min == -1 &&
				freq_set[kicker][i].max == -1)
			clear_bit(kicker, &policy_mask[i]);
		else
			set_bit(kicker, &policy_mask[i]);

		len1 += snprintf(msg1 + len1, sizeof(msg1) - len1,
				"[0x %lx] ", policy_mask[i]);
		if (len1 < 0) {
			perfmgr_trace_printk("cpu_ctrl", "return -EIO 3\n");
			mutex_unlock(&boost_freq);
			return -EIO;
		}
	}

	for (i = 0; i < CPU_MAX_KIR; i++) {
		for_each_perfmgr_clusters(j) {
			final_freq[j].min
				= MAX(freq_set[i][j].min, final_freq[j].min);
#ifdef CONFIG_MTK_CPU_CTRL_CFP
			final_freq[j].max
				= final_freq[j].max != -1 &&
				freq_set[i][j].max != -1 ?
				MIN(freq_set[i][j].max, final_freq[j].max) :
				MAX(freq_set[i][j].max, final_freq[j].max);
#else
			final_freq[j].max
				= MAX(freq_set[i][j].max, final_freq[j].max);
#endif
			if (final_freq[j].min > final_freq[j].max &&
					final_freq[j].max != -1)
				final_freq[j].max = final_freq[j].min;
		}
	}

	for_each_perfmgr_clusters(i) {
		current_freq[i].min = final_freq[i].min;
		current_freq[i].max = final_freq[i].max;
		len += snprintf(msg + len, sizeof(msg) - len, "{%d}{%d} ",
				current_freq[i].min, current_freq[i].max);
		if (len < 0) {
			perfmgr_trace_printk("cpu_ctrl", "return -EIO 4\n");
			mutex_unlock(&boost_freq);
			return -EIO;
		}
	}

	if (len >= 0 && len < LOG_BUF_SIZE) {
		len1 = LOG_BUF_SIZE - len - 1;
		if (len1 > 0)
			strncat(msg, msg1, len1);
	}
	if (log_enable)
		pr_debug("%s", msg);

#ifdef CONFIG_TRACING
	perfmgr_trace_printk("cpu_ctrl", msg);
#endif


#ifdef CONFIG_MTK_CPU_CTRL_CFP
	if (!cfp_init_ret)
		cpu_ctrl_cfp(final_freq);
	else
		mt_ppm_userlimit_cpu_freq(perfmgr_clusters, final_freq);
#else
	mt_ppm_userlimit_cpu_freq(perfmgr_clusters, final_freq);
#endif

ret_update:
	kfree(final_freq);
	mutex_unlock(&boost_freq);
	return retval;

	return 0;
}
EXPORT_SYMBOL(update_userlimit_cpu_freq);



/***************************************/
static ssize_t perfmgr_perfserv_freq_proc_write(struct file *filp
		, const char __user *ubuf, size_t cnt, loff_t *pos)
{
	int i = 0, data;
	struct cpu_ctrl_data *freq_limit;
	unsigned int arg_num = perfmgr_clusters * 2; /* for min and max */
	char *tok, *tmp;
	char *buf = perfmgr_copy_from_user_for_proc(ubuf, cnt);

	if (!buf) {
		pr_debug("buf is null\n");
		goto out1;
	}
	freq_limit = kcalloc(perfmgr_clusters, sizeof(struct cpu_ctrl_data),
			GFP_KERNEL);
	if (!freq_limit)
		goto out;

	tmp = buf;
	pr_debug("freq write_to_file\n");
	while ((tok = strsep(&tmp, " ")) != NULL) {
		if (i == arg_num) {
			pr_debug(
					"@%s: number of arguments > %d!\n",
					__func__, arg_num);
			goto out;
		}

		if (kstrtoint(tok, 10, &data)) {
			pr_debug("@%s: Invalid input: %s\n",
					__func__, tok);
			goto out;
		} else {
			if (i % 2) /* max */
				freq_limit[i/2].max = data;
			else /* min */
				freq_limit[i/2].min = data;
			i++;
		}
	}

	if (i < arg_num) {
		pr_debug(
				"@%s: number of arguments < %d!\n",
				__func__, arg_num);
	} else {
		powerhal_tid = current->pid;
		update_userlimit_cpu_freq(CPU_KIR_PERF
				, perfmgr_clusters, freq_limit);
	}
out:
	free_page((unsigned long)buf);
	kfree(freq_limit);
out1:
	return cnt;

}

static int perfmgr_perfserv_freq_proc_show(struct seq_file *m, void *v)
{
	int i;

	for_each_perfmgr_clusters(i)
		seq_printf(m, "cluster %d min:%d max:%d\n",
				i, freq_set[CPU_KIR_PERF][i].min,
				freq_set[CPU_KIR_PERF][i].max);
	return 0;
}
/***************************************/
#define MAX_NR_FREQ 16
static ssize_t perfmgr_boot_freq_proc_write(struct file *filp,
		const char __user *ubuf, size_t cnt, loff_t *pos)
{
	int i = 0, data;
	struct cpu_ctrl_data *freq_limit;
	unsigned int arg_num = perfmgr_clusters * 2; /* for min and max */
	char *tok, *tmp;
	char *buf = perfmgr_copy_from_user_for_proc(ubuf, cnt);

	freq_limit = kcalloc(perfmgr_clusters,
			sizeof(struct cpu_ctrl_data), GFP_KERNEL);
	if (!freq_limit)
		goto out;

	tmp = buf;
	while ((tok = strsep(&tmp, " ")) != NULL) {
		if (i == arg_num) {
			pr_debug("@%s: number of arguments > %d!\n",
					__func__, arg_num);
			break;
		}

		if (kstrtoint(tok, 10, &data)) {
			pr_debug("@%s: Invalid input: %s\n", __func__, tok);
			goto out;
		} else {
			if (i % 2) /* max */
#ifdef CONFIG_MTK_CPU_FREQ
				freq_limit[i/2].max =
				(data < 0 || data > MAX_NR_FREQ - 1)
				? -1 :
				mt_cpufreq_get_freq_by_idx(i / 2, data);
#else
				freq_limit[i/2].max = -1;
#endif
			else /* min */
#ifdef CONFIG_MTK_CPU_FREQ
				freq_limit[i/2].min =
				(data < 0 || data > MAX_NR_FREQ - 1)
				? -1 :
				mt_cpufreq_get_freq_by_idx(i / 2, data);
#else
				freq_limit[i/2].min = -1;
#endif
			i++;
		}
	}

	if (i < arg_num)
		pr_debug("@%s: number of arguments < %d!\n",
				__func__, arg_num);
	else
		update_userlimit_cpu_freq(CPU_KIR_BOOT,
				perfmgr_clusters, freq_limit);

out:
	free_page((unsigned long)buf);
	kfree(freq_limit);
	return cnt;

}

static int perfmgr_boot_freq_proc_show(struct seq_file *m, void *v)
{
	int i;

	for_each_perfmgr_clusters(i)
		seq_printf(m, "cluster %d min:%d max:%d\n",
				i, freq_set[CPU_KIR_BOOT][i].min,
				freq_set[CPU_KIR_BOOT][i].max);

	return 0;
}

/***************************************/
static int perfmgr_current_freq_proc_show(struct seq_file *m, void *v)
{
	int i, j;

	for_each_perfmgr_clusters(i)
		seq_printf(m, "cluster %d min:%d max:%d\n",
				i, current_freq[i].min, current_freq[i].max);

	seq_puts(m, "===== kicker =====\n");
	for (i = 0; i < CPU_MAX_KIR; i++)
		for_each_perfmgr_clusters(j)
			if (freq_set[i][j].min != -1 ||
				freq_set[i][j].max != -1)
				seq_printf(m,
				"KIR[%d] cluster %d min:%d max:%d\n",
				i, j, freq_set[i][j].min, freq_set[i][j].max);

	return 0;
}

/*******************************************/
static ssize_t perfmgr_perfmgr_log_proc_write(struct file *filp,
		const char __user *ubuf, size_t cnt, loff_t *pos)
{
	int data = 0;

	int rv = check_proc_write(&data, ubuf, cnt);

	if (rv != 0)
		return rv;
	log_enable = data > 0 ? 1 : 0;

	return cnt;
}

static int perfmgr_perfmgr_log_proc_show(struct seq_file *m, void *v)
{
	if (m)
		seq_printf(m, "%d\n", log_enable);
	return 0;
}

PROC_FOPS_RW(perfserv_freq);
PROC_FOPS_RW(boot_freq);
PROC_FOPS_RO(current_freq);
PROC_FOPS_RW(perfmgr_log);

/************************************************/
int cpu_ctrl_init(struct proc_dir_entry *parent)
{
	struct proc_dir_entry *boost_dir = NULL;
	int i, j, ret = 0;
	size_t idx;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(perfserv_freq),
		PROC_ENTRY(boot_freq),
		PROC_ENTRY(current_freq),
		PROC_ENTRY(perfmgr_log),
	};
	mutex_init(&boost_freq);

	boost_dir = proc_mkdir("cpu_ctrl", parent);

	if (!boost_dir)
		pr_debug("boost_dir null\n ");

	/* create procfs */
	for (idx = 0; idx < ARRAY_SIZE(entries); idx++) {
		if (!proc_create(entries[idx].name, 0644,
					boost_dir, entries[idx].fops)) {
			pr_debug("%s(), create /cpu_ctrl%s failed\n",
					__func__, entries[idx].name);
			ret = -EINVAL;
			goto out;
		}
	}

#ifdef CONFIG_MTK_CPU_CTRL_CFP
	cfp_init_ret = cpu_ctrl_cfp_init(boost_dir);
#endif

	current_freq = kcalloc(perfmgr_clusters, sizeof(struct ppm_limit_data),
			GFP_KERNEL);

	policy_mask = kcalloc(perfmgr_clusters, sizeof(unsigned long),
			GFP_KERNEL);


	for (i = 0; i < CPU_MAX_KIR; i++)
		freq_set[i] = kcalloc(perfmgr_clusters
				, sizeof(struct ppm_limit_data)
				, GFP_KERNEL);

	for_each_perfmgr_clusters(i) {
		current_freq[i].min = -1;
		current_freq[i].max = -1;
		policy_mask[i] = 0;
	}

	for (i = 0; i < CPU_MAX_KIR; i++) {
		for_each_perfmgr_clusters(j) {
			freq_set[i][j].min = -1;
			freq_set[i][j].max = -1;
		}
	}
out:
	return ret;

}

void cpu_ctrl_exit(void)
{
	int i;

	kfree(current_freq);
	kfree(policy_mask);
	for (i = 0; i < CPU_MAX_KIR; i++)
		kfree(freq_set[i]);

#ifdef CONFIG_MTK_CPU_CTRL_CFP
	if (!cfp_init_ret)
		cpu_ctrl_cfp_exit();
#endif
}
