/*
 * Copyright (C) 2016 MediaTek Inc.
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

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#include "mtk_cpufreq_internal.h"
#include "mtk_cpufreq_hybrid.h"
#include "mtk_cpufreq_platform.h"
//MTK wfq add for bugreport
#include <linux/thermal.h>
unsigned int func_lv_mask;
unsigned int do_dvfs_stress_test;
unsigned int dvfs_power_mode;
unsigned int sched_dvfs_enable;

ktime_t now[NR_SET_V_F];
ktime_t delta[NR_SET_V_F];
ktime_t max[NR_SET_V_F];

enum ppb_power_mode {
	DEFAULT_MODE,		/* normal mode */
	LOW_POWER_MODE,
	JUST_MAKE_MODE,
	PERFORMANCE_MODE,	/* sports mode */
	NUM_PPB_POWER_MODE
};

static const char *power_mode_str[NUM_PPB_POWER_MODE] = {
	"Default(Normal) mode",
	"Low Power mode",
	"Just Make mode",
	"Performance(Sports) mode"
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

static ssize_t cpufreq_debug_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int dbg_lv;
	int rc;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	rc = kstrtoint(buf, 10, &dbg_lv);
	if (rc < 0)
		tag_pr_info
		("echo dbg_lv (dec) > /proc/cpufreq/cpufreq_debug\n");
	else
		func_lv_mask = dbg_lv;

	free_page((unsigned long)buf);
	return count;
}

static int cpufreq_power_mode_proc_show(struct seq_file *m, void *v)
{
	unsigned int mode = dvfs_power_mode;

	seq_printf(m, "%s\n",
	mode < NUM_PPB_POWER_MODE ? power_mode_str[mode] : "Unknown");

	return 0;
}

static ssize_t cpufreq_power_mode_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int mode;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (!kstrtouint(buf, 10, &mode) && mode < NUM_PPB_POWER_MODE) {
		dvfs_power_mode = mode;
		tag_pr_debug("%s start\n", power_mode_str[mode]);
	} else {
		tag_pr_info
		("echo 0/1/2/3 > /proc/cpufreq/cpufreq_power_mode\n");
	}

	free_page((unsigned long)buf);
	return count;
}

static int cpufreq_stress_test_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", do_dvfs_stress_test);

	return 0;
}

static ssize_t cpufreq_stress_test_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int do_stress;
	int rc;
	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;
	rc = kstrtoint(buf, 10, &do_stress);
	if (rc < 0)
		tag_pr_info("echo 0/1 > /proc/cpufreq/cpufreq_stress_test\n");
	else {
		do_dvfs_stress_test = do_stress;
#ifdef CONFIG_HYBRID_CPU_DVFS
		cpuhvfs_set_dvfs_stress(do_stress);
#endif
	}

	free_page((unsigned long)buf);
	return count;
}

/* cpufreq_oppidx */
static int cpufreq_oppidx_proc_show(struct seq_file *m, void *v)
{
	struct mt_cpu_dvfs *p = m->private;
	int j;
	unsigned long flags;


	cpufreq_lock(flags);
	seq_printf(m, "[%s/%u]\n", p->name, p->cpu_id);
	seq_printf(m, "cpufreq_oppidx = %d\n", p->idx_opp_tbl);

	for (j = 0; j < p->nr_opp_tbl; j++) {
		seq_printf(m, "\t%-2d (%u, %u)\n",
			      j, cpu_dvfs_get_freq_by_idx(p, j),
			      cpu_dvfs_get_volt_by_idx(p, j));
	}
	cpufreq_unlock(flags);

	return 0;
}

static ssize_t cpufreq_oppidx_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	struct mt_cpu_dvfs *p = PDE_DATA(file_inode(file));
	int oppidx;
	int rc;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	rc = kstrtoint(buf, 10, &oppidx);
	if (rc < 0) {
		p->dvfs_disable_by_procfs = false;
		tag_pr_info("echo oppidx > /proc/cpufreq/%s/cpufreq_oppidx\n",
		p->name);
	} else {
		if (oppidx >= 0 && oppidx < p->nr_opp_tbl) {
			p->dvfs_disable_by_procfs = true;
#ifdef CONFIG_HYBRID_CPU_DVFS
			cpuhvfs_set_freq(arch_get_cluster_id(p->cpu_id),
			cpu_dvfs_get_freq_by_idx(p, oppidx));
#else
			_mt_cpufreq_dvfs_request_wrapper(p, oppidx,
			MT_CPU_DVFS_NORMAL, NULL);
#endif
		} else {
			p->dvfs_disable_by_procfs = false;
			tag_pr_info
			("echo oppidx > /proc/cpufreq/%s/cpufreq_oppidx\n",
			p->name);
		}
	}

	free_page((unsigned long)buf);

	return count;
}

/* cpufreq_freq */
static int cpufreq_freq_proc_show(struct seq_file *m, void *v)
{
	struct mt_cpu_dvfs *p = m->private;
	struct pll_ctrl_t *pll_p = id_to_pll_ctrl(p->Pll_id);

	seq_printf(m, "%d KHz\n", pll_p->pll_ops->get_cur_freq(pll_p));

	return 0;
}

static ssize_t cpufreq_freq_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	struct mt_cpu_dvfs *p = PDE_DATA(file_inode(file));
	int freq, i, found = 0;
	int rc;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	rc = kstrtoint(buf, 10, &freq);
	if (rc < 0) {
		p->dvfs_disable_by_procfs = false;
		tag_pr_info
		("echo khz > /proc/cpufreq/%s/cpufreq_freq\n", p->name);
	} else {
		if (freq < p->opp_tbl[p->nr_opp_tbl - 1].cpufreq_khz) {
			if (freq != 0)
				tag_pr_info
				("frequency should higher than %dKHz!\n",
				p->opp_tbl[p->nr_opp_tbl - 1].cpufreq_khz);

			p->dvfs_disable_by_procfs = false;
		} else {
			for (i = 0; i < p->nr_opp_tbl; i++) {
				if (freq == p->opp_tbl[i].cpufreq_khz) {
					found = 1;
					break;
				}
			}

			if (found == 1) {
				p->dvfs_disable_by_procfs = true;
#ifdef CONFIG_HYBRID_CPU_DVFS
				cpuhvfs_set_freq(
				arch_get_cluster_id(p->cpu_id),
				cpu_dvfs_get_freq_by_idx(p, i));
#else
				_mt_cpufreq_dvfs_request_wrapper(p,
				i, MT_CPU_DVFS_NORMAL, NULL);
#endif
			} else {
				p->dvfs_disable_by_procfs = false;
				tag_pr_info
			("frequency %dKHz! is not found in CPU opp table\n",
				freq);
			}
		}
	}

	free_page((unsigned long)buf);

	return count;
}

/* cpufreq_volt */
static int cpufreq_volt_proc_show(struct seq_file *m, void *v)
{
	struct mt_cpu_dvfs *p = m->private;
	struct buck_ctrl_t *vproc_p = id_to_buck_ctrl(p->Vproc_buck_id);
	struct buck_ctrl_t *vsram_p = id_to_buck_ctrl(p->Vsram_buck_id);
	unsigned long flags;

	cpufreq_lock(flags);
	seq_printf(m, "Vproc: %d uV\n",
		vproc_p->buck_ops->get_cur_volt(vproc_p) * 10);
	seq_printf(m, "Vsram: %d uV\n",
		vsram_p->buck_ops->get_cur_volt(vsram_p) * 10);
	cpufreq_unlock(flags);

	return 0;
}

static ssize_t cpufreq_volt_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned long flags;
	struct mt_cpu_dvfs *p = PDE_DATA(file_inode(file));
#ifndef CONFIG_HYBRID_CPU_DVFS
	struct buck_ctrl_t *vproc_p = id_to_buck_ctrl(p->Vproc_buck_id);
#endif
	int uv;
	int rc;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;
	rc = kstrtoint(buf, 10, &uv);
	if (rc < 0) {
		p->dvfs_disable_by_procfs = false;
		tag_pr_info
		("echo uv > /proc/cpufreq/%s/cpufreq_volt\n", p->name);
	} else {
		p->dvfs_disable_by_procfs = true;
		cpufreq_lock(flags);
#ifdef CONFIG_HYBRID_CPU_DVFS
		/* cpuhvfs_set_volt(*/
		/*arch_get_cluster_id(p->cpu_id), uv / 10); */
#else
		vproc_p->fix_volt = uv / 10;
		set_cur_volt_wrapper(p, vproc_p->fix_volt);
#endif
		cpufreq_unlock(flags);
	}

	free_page((unsigned long)buf);

	return count;
}

/* cpufreq_turbo_mode */
int disable_turbo;
static int cpufreq_turbo_mode_proc_show(struct seq_file *m, void *v)
{
	struct mt_cpu_dvfs *p = m->private;

	seq_printf(m, "turbo_mode(support, disable, loc_opt) = %d, %d, %d\n",
		      turbo_flag, disable_turbo, p->turbo_mode);

	return 0;
}

static ssize_t cpufreq_turbo_mode_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	struct mt_cpu_dvfs *p = PDE_DATA(file_inode(file));
	unsigned int turbo_mode;
	int rc;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;
	rc = kstrtoint(buf, 10, &turbo_mode);
	if (rc < 0)
		tag_pr_info
		("echo 0/1 > /proc/cpufreq/%s/cpufreq_turbo_mode\n", p->name);
	else {
		p->turbo_mode = turbo_mode;
#ifdef CONFIG_HYBRID_CPU_DVFS
		if (turbo_mode == 0) {
			cpuhvfs_set_turbo_disable(1);
			cpuhvfs_set_turbo_mode(0, 0, 0);
			disable_turbo = 1;
		}
#endif
	}

	free_page((unsigned long)buf);

	return count;
}

/* cpufreq_sched_disable */
static int cpufreq_sched_disable_proc_show(struct seq_file *m, void *v)
{
	int r = 1;

#ifdef CONFIG_HYBRID_CPU_DVFS
	r = cpuhvfs_get_sched_dvfs_disable();
#endif

	seq_printf(m, "cpufreq_sched_disable = %d\n", r);

	return 0;
}

static ssize_t cpufreq_sched_disable_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int sched_disable;
	int rc;
	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;
	rc = kstrtoint(buf, 10, &sched_disable);
	if (rc < 0)
		tag_pr_info
		("echo 0/1 > /proc/cpufreq/cpufreq_sched_disable\n");
	else {
#ifdef CONFIG_HYBRID_CPU_DVFS
		cpuhvfs_set_sched_dvfs_disable(sched_disable);
		if (sched_disable)
			sched_dvfs_enable = 0;
		else
			sched_dvfs_enable = 1;
#endif
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

#ifdef CONFIG_HYBRID_CPU_DVFS
	cpuhvfs_get_time_profile();
#endif

	return 0;
}

static ssize_t cpufreq_dvfs_time_profile_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int temp;
	int rc;
	int i;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	rc = kstrtoint(buf, 10, &temp);
	if (rc < 0)
		tag_pr_info
		("echo 1 > /proc/cpufreq/cpufreq_dvfs_time_profile\n");
	else {
		if (temp == 1) {
			for (i = 0; i < NR_SET_V_F; i++)
				max[i].tv64 = 0;
		}
	}
	free_page((unsigned long)buf);

	return count;
}
//MTK wfq add for bugreport +
extern int mtkts_bts_get_hw_temp(void);
extern int tscpu_get_curr_temp(void);
extern int tscpu_get_curr_max_ts_temp(void);
extern unsigned int apthermolmt_get_cpu_power_limit(void);
extern unsigned int apthermolmt_get_gpu_power_limit(void);
static int cpufreq_sum_proc_show(struct seq_file *m, void *v)
{
	struct mt_cpu_dvfs *pLL,*pL;
	struct pll_ctrl_t *pll_pLL,*pll_pL;
	struct buck_ctrl_t *vproc_pLL,*vproc_pL;
	unsigned int freqLL=0,freqL=0,mvLL=0,mvL=0;
	unsigned int cpu_power,gpu_power;
	pL = id_to_cpu_dvfs(MT_CPU_DVFS_L);
	if(NULL==pL)
		return 0;
	pll_pL = id_to_pll_ctrl(pL->Pll_id);
	if(NULL==pll_pL)
		return 0;
	freqL=pll_pL->pll_ops->get_cur_freq(pll_pL)/1000;
	vproc_pL = id_to_buck_ctrl(pL->Vproc_buck_id);
	if(NULL==vproc_pL)
		return 0;
	mvL=vproc_pL->buck_ops->get_cur_volt(vproc_pL)/100;
	if(8==NR_CPUS)
	{
		pLL = id_to_cpu_dvfs(MT_CPU_DVFS_LL);
		if(NULL==pLL)
		return 0;
		pll_pLL = id_to_pll_ctrl(pLL->Pll_id);
		if(NULL==pll_pLL)
		return 0;
		freqLL=pll_pLL->pll_ops->get_cur_freq(pll_pLL)/1000;
		vproc_pLL = id_to_buck_ctrl(pLL->Vproc_buck_id);
		mvLL=vproc_pLL->buck_ops->get_cur_volt(vproc_pLL)/100;
		seq_printf(m, "cpufreq:[%u,%u;%u,%u]\n", freqL,mvL,freqLL,mvLL);
	}
	if(4==NR_CPUS)
	seq_printf(m, "cpufreq:(%u,%u)\n", freqL,mvL);
	cpu_power=apthermolmt_get_cpu_power_limit();
	gpu_power=apthermolmt_get_gpu_power_limit();
	seq_printf(m, "Temp:%d,%d,%u,%u\n", (mtkts_bts_get_hw_temp()/1000),(tscpu_get_curr_max_ts_temp()/1000),\
		(int)((cpu_power != 0x7FFFFFFF) ? cpu_power : 0),\
		(int)((gpu_power != 0x7FFFFFFF) ? gpu_power : 0));
	return 0;
}

static ssize_t cpufreq_sum_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	return 0;
}
/*
 *tsbattery,tspmic,tsbtsmdpa,tswmt,tscharger
 */
static int cpufreq_tz_related_proc_show(struct seq_file *m, void *v)
{
	int temp_tz1=0,temp_tz2=0,temp_tz3=0,temp_tz4=0,temp_tz5=0;
	int ret=0;
	struct thermal_zone_device *mtktsbattery,*mtktspmic,*mtktsbtsmdpa,*mtktswmt,*mtktscharger;
	mtktsbattery=thermal_zone_get_zone_by_name("mtktsbattery");
        if(!IS_ERR(mtktsbattery))
	{
		ret=mtktsbattery->ops->get_temp(mtktsbattery,&temp_tz1);
		if(0!=ret)
		return 0;
	}

	mtktspmic=thermal_zone_get_zone_by_name("mtktspmic");
        if(!IS_ERR(mtktspmic))
	{
		ret=mtktspmic->ops->get_temp(mtktspmic,&temp_tz2);
		if(0!=ret)
		return 0;
	}
	mtktsbtsmdpa=thermal_zone_get_zone_by_name("mtktsbtsmdpa");
        if(!IS_ERR(mtktsbtsmdpa))
	{
		ret=mtktsbtsmdpa->ops->get_temp(mtktsbtsmdpa,&temp_tz3);
		if(0!=ret)
		return 0;
	}
	mtktswmt=thermal_zone_get_zone_by_name("mtktswmt");
        if(!IS_ERR(mtktswmt))
	{
		ret=mtktswmt->ops->get_temp(mtktswmt,&temp_tz4);
		if(0!=ret)
		return 0;
	}
	mtktscharger=thermal_zone_get_zone_by_name("mtktscharger");
        if(!IS_ERR(mtktscharger))
	{
		ret=mtktscharger->ops->get_temp(mtktscharger,&temp_tz5);
		if(0!=ret)
		return 0;
	}
	seq_printf(m, "mtktz:%d,%d,%d,%d,%d\n", temp_tz1/1000,temp_tz2/1000,temp_tz3/1000,temp_tz4/1000,temp_tz5/1000);
	return 0;
}
PROC_FOPS_RW(cpufreq_sum);
PROC_FOPS_RO(cpufreq_tz_related);
//MTK wfq add for bugreport -
PROC_FOPS_RW(cpufreq_debug);
PROC_FOPS_RW(cpufreq_stress_test);
PROC_FOPS_RW(cpufreq_power_mode);
PROC_FOPS_RW(cpufreq_sched_disable);
PROC_FOPS_RW(cpufreq_dvfs_time_profile);

PROC_FOPS_RW(cpufreq_oppidx);
PROC_FOPS_RW(cpufreq_freq);
PROC_FOPS_RW(cpufreq_volt);
PROC_FOPS_RW(cpufreq_turbo_mode);

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
		PROC_ENTRY(cpufreq_sched_disable),
		PROC_ENTRY(cpufreq_dvfs_time_profile),
	};

	const struct pentry cpu_entries[] = {
		PROC_ENTRY(cpufreq_oppidx),
		PROC_ENTRY(cpufreq_freq),
		PROC_ENTRY(cpufreq_volt),
		PROC_ENTRY(cpufreq_turbo_mode),
		PROC_ENTRY(cpufreq_sum),//MTK wfq add for bugreport
		PROC_ENTRY(cpufreq_tz_related),//MTK wfq add for bugreport
	};

	dir = proc_mkdir("cpufreq", NULL);

	if (!dir) {
		tag_pr_notice("fail to create /proc/cpufreq @ %s()\n",
								__func__);
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create
		    (entries[i].name, 0664, dir, entries[i].fops))
			tag_pr_notice("%s(), create /proc/cpufreq/%s failed\n",
				__func__, entries[i].name);
	}

	for_each_cpu_dvfs(j, p) {
		cpu_dir = proc_mkdir(p->name, dir);

		if (!cpu_dir) {
			tag_pr_notice
				("fail to create /proc/cpufreq/%s @ %s()\n",
				p->name, __func__);
			return -ENOMEM;
		}

		for (i = 0; i < ARRAY_SIZE(cpu_entries); i++) {
			if (!proc_create_data
			    (cpu_entries[i].name, 0664,
			    cpu_dir, cpu_entries[i].fops, p))
				tag_pr_notice
				("%s(), create /proc/cpufreq/%s/%s failed\n",
				__func__, p->name, entries[i].name);
		}
	}

	return 0;
}
