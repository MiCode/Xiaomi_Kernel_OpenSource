
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#include "mt_cpufreq_internal.h"
#include "mt_cpufreq_hybrid.h"

unsigned int func_lv_mask = 0;
unsigned int do_dvfs_stress_test;
unsigned int dvfs_power_mode;
int release_dvfs = 0;
int thres_ll = 0;
int thres_l = 0;
int thres_b = 0;

ktime_t now[NR_SET_V_F];
ktime_t delta[NR_SET_V_F];
ktime_t max[NR_SET_V_F];


enum ppb_power_mode {
	Default = 0,
	Low_power_Mode = 1,
	Just_Make_Mode = 2,
	Performance_Mode = 3,
};

char *_copy_from_user_for_proc(const char __user *buffer, size_t count)
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

/* cpufreq_debug */
static int cpufreq_debug_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "cpufreq debug (log level) = %d\n", func_lv_mask);

	return 0;
}

static ssize_t cpufreq_debug_proc_write(struct file *file, const char __user *buffer, size_t count,
					loff_t *pos)
{
	unsigned int dbg_lv;
	int rc;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	rc = kstrtoint(buf, 10, &dbg_lv);
	if (rc < 0)
		cpufreq_err("echo dbg_lv (dec) > /proc/cpufreq/cpufreq_debug\n");
	else
		func_lv_mask = dbg_lv;

	free_page((unsigned long)buf);
	return count;
}

static int cpufreq_power_mode_proc_show(struct seq_file *m, void *v)
{
	switch (dvfs_power_mode) {
	case Default:
		seq_puts(m, "Default\n");
		break;
	case Low_power_Mode:
		seq_puts(m, "Low_power_Mode\n");
		break;
	case Just_Make_Mode:
		seq_puts(m, "Just_Make_Mode\n");
		break;
	case Performance_Mode:
		seq_puts(m, "Performance_Mode\n");
		break;
	};

	return 0;
}

static ssize_t cpufreq_power_mode_proc_write(struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int do_power_mode;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (!kstrtoint(buf, 10, &do_power_mode))
		dvfs_power_mode = do_power_mode;
	else
		cpufreq_err("echo 0/1/2/3 > /proc/cpufreq/cpufreq_power_mode\n");

	free_page((unsigned long)buf);
	return count;
}

static int cpufreq_stress_test_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", do_dvfs_stress_test);

	return 0;
}

static ssize_t cpufreq_stress_test_proc_write(struct file *file, const char __user *buffer,
					      size_t count, loff_t *pos)
{
	unsigned int do_stress;
	int rc;
	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;
	rc = kstrtoint(buf, 10, &do_stress);
	if (rc < 0)
		cpufreq_err("echo 0/1 > /proc/cpufreq/cpufreq_stress_test\n");
	else
		do_dvfs_stress_test = do_stress;

	free_page((unsigned long)buf);
	return count;
}

/* cpufreq_ptpod_freq_volt */
static int cpufreq_ptpod_freq_volt_proc_show(struct seq_file *m, void *v)
{
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)m->private;
	int j;

	for (j = 0; j < p->nr_opp_tbl; j++) {
		seq_printf(m,
			   "[%d] = { .cpufreq_khz = %d,\t.cpufreq_volt = %d},\n",
			   j, p->opp_tbl[j].cpufreq_khz, p->opp_tbl[j].cpufreq_volt);
	}

	return 0;
}

/* cpufreq_oppidx */
static int cpufreq_oppidx_proc_show(struct seq_file *m, void *v)
{
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)m->private;
	int j;

	seq_printf(m, "[%s/%d]\n" "cpufreq_oppidx = %d\n", p->name, p->cpu_id, p->idx_opp_tbl);

	for (j = 0; j < p->nr_opp_tbl; j++) {
		seq_printf(m, "\tOP(%d, %d),\n",
			   cpu_dvfs_get_freq_by_idx(p, j), cpu_dvfs_get_volt_by_idx(p, j)
		    );
	}

	return 0;
}

static ssize_t cpufreq_oppidx_proc_write(struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)PDE_DATA(file_inode(file));
	int oppidx;
	int rc;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	BUG_ON(NULL == p);
	rc = kstrtoint(buf, 10, &oppidx);
	if (rc < 0) {
		p->dvfs_disable_by_procfs = false;
		cpufreq_err("echo oppidx > /proc/cpufreq/cpufreq_oppidx (0 <= %d < %d)\n", oppidx,
			p->nr_opp_tbl);
	} else {
		if (0 <= oppidx && oppidx < p->nr_opp_tbl) {
			p->dvfs_disable_by_procfs = true;
			_mt_cpufreq_dvfs_request_wrapper(p, oppidx, MT_CPU_DVFS_NORMAL, NULL);
		} else {
			p->dvfs_disable_by_procfs = false;
			cpufreq_err("echo oppidx > /proc/cpufreq/cpufreq_oppidx (0 <= %d < %d)\n", oppidx,
				p->nr_opp_tbl);
		}
	}

	free_page((unsigned long)buf);

	return count;
}

/* cpufreq_freq */
static int cpufreq_freq_proc_show(struct seq_file *m, void *v)
{
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)m->private;
	struct pll_ctrl_t *pll_p = id_to_pll_ctrl(p->Pll_id);

	seq_printf(m, "%d KHz\n",
		pll_p->pll_ops->get_cur_freq(pll_p));

	return 0;
}

static ssize_t cpufreq_freq_proc_write(struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned long flags;
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)PDE_DATA(file_inode(file));
	int freq, i, found;
	int rc;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	BUG_ON(NULL == p);
	rc = kstrtoint(buf, 10, &freq);
	if (rc < 0) {
		p->dvfs_disable_by_procfs = false;
		cpufreq_err("echo khz > /proc/cpufreq/cpufreq_freq\n");
	} else {
		if (freq < p->opp_tbl[p->nr_opp_tbl].cpufreq_khz) {
			if (freq != 0)
				cpufreq_err("frequency should higher than %dKHz!\n",
					    p->opp_tbl[p->nr_opp_tbl].cpufreq_khz);

			p->dvfs_disable_by_procfs = false;
			goto end;
		} else {
			for (i = 0; i < p->nr_opp_tbl; i++) {
				if (freq == p->opp_tbl[i].cpufreq_khz) {
					found = 1;
					break;
				}
			}

			if (found == 1) {
				p->dvfs_disable_by_procfs = true;
				cpufreq_lock(flags);
				_mt_cpufreq_dvfs_request_wrapper(p, i, MT_CPU_DVFS_NORMAL, NULL);
				cpufreq_unlock(flags);
			} else {
				p->dvfs_disable_by_procfs = false;
				cpufreq_err("frequency %dKHz! is not found in CPU opp table\n",
					    freq);
			}
		}
	}

end:
	free_page((unsigned long)buf);

	return count;
}

/* cpufreq_volt */
static int cpufreq_volt_proc_show(struct seq_file *m, void *v)
{
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)m->private;
	unsigned long flags;
	struct buck_ctrl_t *vproc_p = id_to_buck_ctrl(p->Vproc_buck_id);
	struct buck_ctrl_t *vsram_p = id_to_buck_ctrl(p->Vsram_buck_id);

	cpufreq_lock(flags);
	seq_printf(m, "Vproc: %d mv\n", vproc_p->buck_ops->get_cur_volt(vproc_p) / 100);	/* mv */
	seq_printf(m, "Vsram: %d mv\n", vsram_p->buck_ops->get_cur_volt(vsram_p) / 100);	/* mv */
	cpufreq_unlock(flags);

	return 0;
}

static ssize_t cpufreq_volt_proc_write(struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned long flags;
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)PDE_DATA(file_inode(file));
#ifndef CONFIG_HYBRID_CPU_DVFS
	struct buck_ctrl_t *vproc_p = id_to_buck_ctrl(p->Vproc_buck_id);
#endif
	int mv;
	int rc;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;
	rc = kstrtoint(buf, 10, &mv);
	if (rc < 0) {
		cpufreq_err("echo mv > /proc/cpufreq/cpufreq_volt\n");
	} else {
		cpufreq_lock(flags);
#ifdef CONFIG_HYBRID_CPU_DVFS
		cpuhvfs_set_volt(arch_get_cluster_id(p->cpu_id), mv * 100);
#else
		vproc_p->fix_volt = mv * 100;
		set_cur_volt_wrapper(p, vproc_p->fix_volt);
#endif
		cpufreq_unlock(flags);
	}

	free_page((unsigned long)buf);

	return count;
}

/* cpufreq_turbo_mode */
static int cpufreq_turbo_mode_proc_show(struct seq_file *m, void *v)
{
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)m->private;

	seq_printf(m, "turbo_mode = %d\n", p->turbo_mode);

	return 0;
}

static ssize_t cpufreq_turbo_mode_proc_write(struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)PDE_DATA(file_inode(file));
	unsigned int turbo_mode;
	int rc;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;
	rc = kstrtoint(buf, 10, &turbo_mode);
	if (rc < 0)
		cpufreq_err("echo 0/1 > /proc/cpufreq/%s/cpufreq_turbo_mode\n", p->name);
	else
		p->turbo_mode = turbo_mode;

	free_page((unsigned long)buf);

	return count;
}

static int cpufreq_up_threshold_ll_proc_show(struct seq_file *m, void *v)
{
	release_dvfs = 1;
	seq_printf(m, "thres_ll = %d\n", thres_ll);

	return 0;
}

static ssize_t cpufreq_up_threshold_ll_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned long flags;
	int mv;
	int rc;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;
	rc = kstrtoint(buf, 10, &mv);
	if (rc < 0) {
		cpufreq_err("echo 0 to 15 > /proc/cpufreq/cpufreq_volt\n");
	} else {
		cpufreq_lock(flags);
		cpufreq_ver("thres_ll change to %d\n", thres_ll);
		thres_ll = mv;
		cpufreq_unlock(flags);
	}

	free_page((unsigned long)buf);

	return count;
}

static int cpufreq_up_threshold_l_proc_show(struct seq_file *m, void *v)
{
	release_dvfs = 0;
	seq_printf(m, "thres_l = %d\n", thres_l);

	return 0;
}

static ssize_t cpufreq_up_threshold_l_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned long flags;
	int mv;
	int rc;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;
	rc = kstrtoint(buf, 10, &mv);
	if (rc < 0) {
		cpufreq_err("echo 0 to 15 > /proc/cpufreq/cpufreq_volt\n");
	} else {
		cpufreq_lock(flags);
		cpufreq_ver("thres_l change to %d\n", thres_l);
		thres_l = mv;
		cpufreq_unlock(flags);
	}

	free_page((unsigned long)buf);

	return count;
}

static int cpufreq_up_threshold_b_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "thres_b = %d\n", thres_b);

	return 0;
}

static ssize_t cpufreq_up_threshold_b_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned long flags;
	int mv;
	int rc;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;
	rc = kstrtoint(buf, 10, &mv);
	if (rc < 0) {
		cpufreq_err("echo 0 to 15 > /proc/cpufreq/cpufreq_volt\n");
	} else {
		cpufreq_lock(flags);
		cpufreq_ver("thres_b change to %d\n", thres_b);
		thres_b = mv;
		cpufreq_unlock(flags);
	}

	free_page((unsigned long)buf);

	return count;
}

/* cpufreq_time_profile */
static int cpufreq_dvfs_time_profile_proc_show(struct seq_file *m, void *v)
{
	int i;

	for (i = 0; i < NR_SET_V_F; i++)
		seq_printf(m, "max[%d] = %lld us\n", i, ktime_to_us(max[i]));

	return 0;
}

static ssize_t cpufreq_dvfs_time_profile_proc_write(struct file *file, const char __user *buffer,
	size_t count, loff_t *pos)
{
	struct mt_cpu_dvfs *p = (struct mt_cpu_dvfs *)PDE_DATA(file_inode(file));
	unsigned int temp;
	int rc;
	int i;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	rc = kstrtoint(buf, 10, &temp);
	if (rc < 0)
		cpufreq_err("echo 0/1 > /proc/cpufreq/%s/cpufreq_dvfs_time_profile\n", p->name);
	else {
		if (temp == 1) {
			for (i = 0; i < NR_SET_V_F; i++)
				max[i].tv64 = 0;
		}
	}
	free_page((unsigned long)buf);

	return count;
}

PROC_FOPS_RW(cpufreq_debug);
PROC_FOPS_RW(cpufreq_stress_test);
PROC_FOPS_RW(cpufreq_power_mode);
PROC_FOPS_RO(cpufreq_ptpod_freq_volt);
PROC_FOPS_RW(cpufreq_oppidx);
PROC_FOPS_RW(cpufreq_freq);
PROC_FOPS_RW(cpufreq_volt);
PROC_FOPS_RW(cpufreq_turbo_mode);
PROC_FOPS_RW(cpufreq_dvfs_time_profile);
PROC_FOPS_RW(cpufreq_up_threshold_ll);
PROC_FOPS_RW(cpufreq_up_threshold_l);
PROC_FOPS_RW(cpufreq_up_threshold_b);

int cpufreq_procfs_init(void)
{
	struct proc_dir_entry *dir = NULL;
	struct proc_dir_entry *cpu_dir = NULL;
	struct mt_cpu_dvfs *p = id_to_cpu_dvfs(0);
	int i, j;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(cpufreq_debug),
		PROC_ENTRY(cpufreq_stress_test),
		PROC_ENTRY(cpufreq_power_mode),
		PROC_ENTRY(cpufreq_up_threshold_ll),
		PROC_ENTRY(cpufreq_up_threshold_l),
		PROC_ENTRY(cpufreq_up_threshold_b),
		PROC_ENTRY(cpufreq_dvfs_time_profile),
	};

	const struct pentry cpu_entries[] = {
		PROC_ENTRY(cpufreq_oppidx),
		PROC_ENTRY(cpufreq_freq),
		PROC_ENTRY(cpufreq_volt),
		PROC_ENTRY(cpufreq_turbo_mode),
	};

	dir = proc_mkdir("cpufreq", NULL);

	if (!dir) {
		cpufreq_err("fail to create /proc/cpufreq @ %s()\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create
		    (entries[i].name, S_IRUGO | S_IWUSR | S_IWGRP, dir, entries[i].fops))
			cpufreq_err("%s(), create /proc/cpufreq/%s failed\n", __func__,
				    entries[i].name);
	}

	for (i = 0; i < ARRAY_SIZE(cpu_entries); i++) {
		if (!proc_create_data
		    (cpu_entries[i].name, S_IRUGO | S_IWUSR | S_IWGRP, dir, cpu_entries[i].fops, p))
			cpufreq_err("%s(), create /proc/cpufreq/%s failed\n", __func__,
				    entries[i].name);
	}

	for_each_cpu_dvfs(j, p) {
		cpu_dir = proc_mkdir(p->name, dir);

		if (!cpu_dir) {
			cpufreq_err("fail to create /proc/cpufreq/%s @ %s()\n", p->name, __func__);
			return -ENOMEM;
		}

		for (i = 0; i < ARRAY_SIZE(cpu_entries); i++) {
			if (!proc_create_data
			    (cpu_entries[i].name, S_IRUGO | S_IWUSR | S_IWGRP, cpu_dir,
			     cpu_entries[i].fops, p))
				cpufreq_err("%s(), create /proc/cpufreq/%s/%s failed\n", __func__,
					    p->name, entries[i].name);
		}
	}

	return 0;
}
