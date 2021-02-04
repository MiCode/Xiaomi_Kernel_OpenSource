/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#define pr_fmt(fmt) "[cpu_ctrl]"fmt
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "cpu_ctrl.h"
#include "boost_ctrl.h"
#include "mtk_perfmgr_internal.h"

#ifdef CONFIG_MTK_CPU_CTRL_CFP
#include "cpu_ctrl_cfp.h"
#endif

#ifdef CONFIG_TRACING
#include <linux/kallsyms.h>
#include <linux/trace_events.h>
#endif

#if defined(CPU_CTRL_CORE_SUPPORT)
static struct mutex boost_core;
static struct ppm_limit_data *current_core;
static struct ppm_limit_data *core_set[CPU_MAX_KIR];
static unsigned long *core_policy_mask;
#endif
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
#if defined(CPU_CTRL_CORE_SUPPORT)
int update_userlimit_cpu_core(int kicker, int num_cluster
		, struct ppm_limit_data *core_limit)
{
	struct ppm_limit_data *final_core;
	int retval = 0;
	int i, j, len = 0, len1 = 0;
	char msg[LOG_BUF_SIZE];
	char msg1[LOG_BUF_SIZE];


	mutex_lock(&boost_core);

	final_core = kcalloc(perfmgr_clusters
			, sizeof(struct ppm_limit_data), GFP_KERNEL);
	if (!final_core) {
		retval = -1;
		perfmgr_trace_printk("cpu_ctrl", "!final_core\n");
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


	for_each_perfmgr_clusters(i) {
		final_core[i].min = -1;
		final_core[i].max = -1;
	}

	len += snprintf(msg + len, sizeof(msg) - len, "[%d] ", kicker);
	if (len < 0) {
		perfmgr_trace_printk("cpu_ctrl", "return -EIO 1\n");
		mutex_unlock(&boost_core);
		return -EIO;
	}
	for_each_perfmgr_clusters(i) {
		core_set[kicker][i].min = core_limit[i].min >= -1 ?
			core_limit[i].min : -1;
		core_set[kicker][i].max = core_limit[i].max >= -1 ?
			core_limit[i].max : -1;

		len += snprintf(msg + len, sizeof(msg) - len, "(%d)(%d) ",
		core_set[kicker][i].min, core_set[kicker][i].max);
		if (len < 0) {
			perfmgr_trace_printk("cpu_ctrl", "return -EIO 2\n");
			mutex_unlock(&boost_core);
			return -EIO;
		}

		if (core_set[kicker][i].min == -1 &&
				core_set[kicker][i].max == -1)
			clear_bit(kicker, &core_policy_mask[i]);
		else
			set_bit(kicker, &core_policy_mask[i]);

		len1 += snprintf(msg1 + len1, sizeof(msg1) - len1,
				"[0x %lx] ", core_policy_mask[i]);
		if (len1 < 0) {
			perfmgr_trace_printk("cpu_ctrl", "return -EIO 3\n");
			mutex_unlock(&boost_core);
			return -EIO;
		}
	}

	for (i = 0; i < CPU_MAX_KIR; i++) {
		for_each_perfmgr_clusters(j) {
			final_core[j].min
				= MAX(core_set[i][j].min, final_core[j].min);
			final_core[j].max
				= MAX(core_set[i][j].max, final_core[j].max);
			if (final_core[j].min > final_core[j].max &&
					final_core[j].max != -1)
				final_core[j].max = final_core[j].min;
		}
	}

	for_each_perfmgr_clusters(i) {
		current_core[i].min = final_core[i].min;
		current_core[i].max = final_core[i].max;
		len += snprintf(msg + len, sizeof(msg) - len, "{%d}{%d} ",
				current_core[i].min, current_core[i].max);
		if (len < 0) {
			perfmgr_trace_printk("cpu_ctrl", "return -EIO 4\n");
			mutex_unlock(&boost_core);
			return -EIO;
		}
	}

	if (strlen(msg) + strlen(msg1) < LOG_BUF_SIZE)
		strncat(msg, msg1, strlen(msg1));

	if (log_enable)
		pr_debug("%s", msg);

#ifdef CONFIG_TRACING
	perfmgr_trace_printk("cpu_ctrl(core)", msg);
#endif

	mt_ppm_userlimit_cpu_core(perfmgr_clusters, final_core);

ret_update:
	kfree(final_core);
	mutex_unlock(&boost_core);
	return retval;
}
EXPORT_SYMBOL(update_userlimit_cpu_core);
#endif //CPU_CTRL_CORE_SUPPORT

int update_userlimit_cpu_freq(int kicker, int num_cluster
		, struct ppm_limit_data *freq_limit)
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

	if (strlen(msg) + strlen(msg1) < LOG_BUF_SIZE)
		strncat(msg, msg1, strlen(msg1));

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
}
EXPORT_SYMBOL(update_userlimit_cpu_freq);


/***************************************/
#if defined(CPU_CTRL_CORE_SUPPORT)
static ssize_t perfmgr_perfserv_core_proc_write(struct file *filp
		, const char __user *ubuf, size_t cnt, loff_t *pos)
{
	int i = 0, data;
	struct ppm_limit_data *core_limit;
	unsigned int arg_num = perfmgr_clusters * 2; /* for min and max */
	char *tok, *tmp;
	char *buf = perfmgr_copy_from_user_for_proc(ubuf, cnt);

	if (!buf) {
		pr_debug("buf is null\n");
		goto out1;
	}
	core_limit = kcalloc(perfmgr_clusters, sizeof(struct ppm_limit_data),
			GFP_KERNEL);
	if (!core_limit)
		goto out;

	tmp = buf;
	pr_debug("core write_to_file\n");
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
				core_limit[i/2].max = data;
			else /* min */
				core_limit[i/2].min = data;
			i++;
		}
	}

	if (i < arg_num) {
		pr_debug(
				"@%s: number of arguments < %d!\n",
				__func__, arg_num);
	} else {
		powerhal_tid = current->pid;
		update_userlimit_cpu_core(CPU_KIR_PERF
				, perfmgr_clusters, core_limit);
	}
out:
	free_page((unsigned long)buf);
	kfree(core_limit);
out1:
	return cnt;

}

static int perfmgr_perfserv_core_proc_show(struct seq_file *m, void *v)
{
	int i;

	for_each_perfmgr_clusters(i)
		seq_printf(m, "cluster %d min:%d max:%d\n",
				i, core_set[CPU_KIR_PERF][i].min,
				core_set[CPU_KIR_PERF][i].max);
	return 0;
}
#endif // CPU_CTRL_CORE_SUPPORT

static ssize_t perfmgr_perfserv_freq_proc_write(struct file *filp
		, const char __user *ubuf, size_t cnt, loff_t *pos)
{
	int i = 0, data;
	struct ppm_limit_data *freq_limit;
	unsigned int arg_num = perfmgr_clusters * 2; /* for min and max */
	char *tok, *tmp;
	char *buf = perfmgr_copy_from_user_for_proc(ubuf, cnt);

	if (!buf) {
		pr_debug("buf is null\n");
		goto out1;
	}
	freq_limit = kcalloc(perfmgr_clusters, sizeof(struct ppm_limit_data),
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
#if defined(CPU_CTRL_CORE_SUPPORT)
#define MAX_NR_CORE 4

static ssize_t perfmgr_boot_core_proc_write(struct file *filp,
		const char __user *ubuf, size_t cnt, loff_t *pos)
{
	int i = 0, data;
	struct ppm_limit_data *core_limit;
	unsigned int arg_num = perfmgr_clusters * 2; /* for min and max */
	char *tok, *tmp;
	char *buf = perfmgr_copy_from_user_for_proc(ubuf, cnt);

	core_limit = kcalloc(perfmgr_clusters,
			sizeof(struct ppm_limit_data), GFP_KERNEL);
	if (!core_limit)
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
				core_limit[i/2].max =
					(data < 1 || data > MAX_NR_CORE)
					? -1 : data;
			else /* min */
				core_limit[i/2].min =
					(data < 1 || data > MAX_NR_CORE)
					? -1 : data;
			i++;
		}
	}

	if (i < arg_num)
		pr_debug("@%s: number of arguments < %d!\n",
				__func__, arg_num);
	else
		update_userlimit_cpu_core(CPU_KIR_BOOT,
				perfmgr_clusters, core_limit);

out:
	free_page((unsigned long)buf);
	kfree(core_limit);
	return cnt;

}

static int perfmgr_boot_core_proc_show(struct seq_file *m, void *v)
{
	int i;

	for_each_perfmgr_clusters(i)
		seq_printf(m, "cluster %d min:%d max:%d\n",
				i, core_set[CPU_KIR_BOOT][i].min,
				core_set[CPU_KIR_BOOT][i].max);

	return 0;
}
#endif /* CPU_CTRL_CORE_SUPPORT */

#define MAX_NR_FREQ 16
static ssize_t perfmgr_boot_freq_proc_write(struct file *filp,
		const char __user *ubuf, size_t cnt, loff_t *pos)
{
	int i = 0, data;
	struct ppm_limit_data *freq_limit;
	unsigned int arg_num = perfmgr_clusters * 2; /* for min and max */
	char *tok, *tmp;
	char *buf = perfmgr_copy_from_user_for_proc(ubuf, cnt);

	freq_limit = kcalloc(perfmgr_clusters,
			sizeof(struct ppm_limit_data), GFP_KERNEL);
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
#if defined(CONFIG_MTK_CPU_FREQ) || defined(CONFIG_MACH_MT6757)
			if (i % 2) /* max */
				freq_limit[i/2].max =
					(data < 0 || data > MAX_NR_FREQ - 1)
					? -1 :
					mt_cpufreq_get_freq_by_idx(i / 2, data);
			else /* min */
				freq_limit[i/2].min =
					(data < 0 || data > MAX_NR_FREQ - 1)
					? -1 :
					mt_cpufreq_get_freq_by_idx(i / 2, data);
			i++;
#endif
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
static int perfmgr_current_freqy_proc_show(struct seq_file *m, void *v)
{
	int i;

	for_each_perfmgr_clusters(i)
		seq_printf(m, "cluster %d min:%d max:%d\n",
				i, current_freq[i].min, current_freq[i].max);

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

#if defined(CPU_CTRL_CORE_SUPPORT)
PROC_FOPS_RW(perfserv_core);
PROC_FOPS_RW(boot_core);
#endif
PROC_FOPS_RW(perfserv_freq);
PROC_FOPS_RW(boot_freq);
PROC_FOPS_RO(current_freqy);
PROC_FOPS_RW(perfmgr_log);

/************************************************/
int cpu_ctrl_init(struct proc_dir_entry *parent)
{
	struct proc_dir_entry *boost_dir = NULL;
	int i, j, ret = 0;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry entries[] = {
#if defined(CPU_CTRL_CORE_SUPPORT)
		PROC_ENTRY(perfserv_core),
		PROC_ENTRY(boot_core),
#endif
		PROC_ENTRY(perfserv_freq),
		PROC_ENTRY(boot_freq),
		PROC_ENTRY(current_freqy),
		PROC_ENTRY(perfmgr_log),
	};
#if defined(CPU_CTRL_CORE_SUPPORT)
	mutex_init(&boost_core);
#endif
	mutex_init(&boost_freq);

	boost_dir = proc_mkdir("cpu_ctrl", parent);

	if (!boost_dir)
		pr_debug("boost_dir null\n ");

	/* create procfs */
	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create(entries[i].name, 0644,
					boost_dir, entries[i].fops)) {
			pr_debug("%s(), create /cpu_ctrl%s failed\n",
					__func__, entries[i].name);
			ret = -EINVAL;
			goto out;
		}
	}

#ifdef CONFIG_MTK_CPU_CTRL_CFP
	cfp_init_ret = cpu_ctrl_cfp_init(boost_dir);
#endif

#if defined(CPU_CTRL_CORE_SUPPORT)
	current_core = kcalloc(perfmgr_clusters, sizeof(struct ppm_limit_data),
			GFP_KERNEL);

	core_policy_mask = kcalloc(perfmgr_clusters, sizeof(unsigned long),
			GFP_KERNEL);
#endif

	current_freq = kcalloc(perfmgr_clusters, sizeof(struct ppm_limit_data),
			GFP_KERNEL);

	policy_mask = kcalloc(perfmgr_clusters, sizeof(unsigned long),
			GFP_KERNEL);


	for (i = 0; i < CPU_MAX_KIR; i++) {
#if defined(CPU_CTRL_CORE_SUPPORT)
		core_set[i] = kcalloc(perfmgr_clusters
				, sizeof(struct ppm_limit_data)
				, GFP_KERNEL);
#endif
		freq_set[i] = kcalloc(perfmgr_clusters
				, sizeof(struct ppm_limit_data)
				, GFP_KERNEL);
	}

	for_each_perfmgr_clusters(i) {
#if defined(CPU_CTRL_CORE_SUPPORT)
		current_core[i].min = -1;
		current_core[i].max = -1;
		core_policy_mask[i] = 0;
#endif
		current_freq[i].min = -1;
		current_freq[i].max = -1;
		policy_mask[i] = 0;
	}

	for (i = 0; i < CPU_MAX_KIR; i++) {
		for_each_perfmgr_clusters(j) {
#if defined(CPU_CTRL_CORE_SUPPORT)
			core_set[i][j].min = -1;
			core_set[i][j].max = -1;
#endif
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

#if defined(CPU_CTRL_CORE_SUPPORT)
	kfree(current_core);
	kfree(core_policy_mask);
#endif
	kfree(current_freq);
	kfree(policy_mask);
	for (i = 0; i < CPU_MAX_KIR; i++) {
#if defined(CPU_CTRL_CORE_SUPPORT)
		kfree(core_set[i]);
#endif
		kfree(freq_set[i]);
	}

#ifdef CONFIG_MTK_CPU_CTRL_CFP
	if (!cfp_init_ret)
		cpu_ctrl_cfp_exit();
#endif
}
