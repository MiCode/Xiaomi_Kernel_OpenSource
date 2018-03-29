/*
 * Copyright (C) 2015 MediaTek Inc.
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

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kobject.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/err.h>
#include <linux/syscalls.h>
#include "mach/mt_thermal.h"
#include "mt_cpufreq.h"

#include "mt-plat/mtk_thermal_monitor.h"

#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/time.h>

#define MAX_NUM_INSTANCE_MTK_COOLER_TM_NTH  (1)

#define MTK_CL_TM_NTH_GET_LIMIT(limit, state) \
	{ (limit) = (short) (((unsigned long) (state))>>16); }

#define MTK_CL_TM_NTH_SET_LIMIT(limit, state) \
	{ state = ((((unsigned long) (state))&0xFFFF) | ((short) limit<<16)); }

#define MTK_CL_TM_NTH_GET_CURR_STATE(curr_state, state) \
	{ curr_state = (((unsigned long) (state))&0xFFFF); }

#define MTK_CL_TM_NTH_SET_CURR_STATE(curr_state, state) \
	do {	if (0 == curr_state) \
			state &= ~0x1;\
		else \
			state |= 0x1;\
	} while (0)

/* extern int mt_cpufreq_thermal_protect(unsigned int limited_power); */
/* extern int mtk_thermal_get_cpu_load_sum(void); */

static void tm_nth_loop(struct work_struct *work);

static struct thermal_cooling_device *cl_tm_nth_dev[MAX_NUM_INSTANCE_MTK_COOLER_TM_NTH] = { 0 };
static unsigned long cl_tm_nth_state[MAX_NUM_INSTANCE_MTK_COOLER_TM_NTH] = { 0 };

#if 0
static int cl_tm_nth_cur_limit = 65535;
#endif

static int tm_nth_on;

static int NTHNTHTHRESENTER = 90 * 4, NTHNTHTHRESEXIT = 85 * 4;
static int CPULOADSMASAMPLECNT = 5;
static int TGTTEMP = 85000;
static int KPINIT = 15;
static int KIINIT = 300;
static int KDINIT = -10000;
static int NTHPOLLINGINTERVAL = 200;	/* ms */
static int cpu_load[20] = { 0 };

static int cpu_load_idx;
static int nth_state;		/* 0: idle, 1: working */
static int cpu_load_sma;
static int prv_cpu_temp = -127000;
static int cur_cpu_temp = -127000;
static int tempError, tempErrorInt, tempErrorDer;
static int curScale = 1000;
static int CPU_POWER_LIMIT[7];

static struct delayed_work tm_nth_poll_queue;

static kuid_t uid = KUIDT_INIT(0);
static kgid_t gid = KGIDT_INIT(1000);

static int mtk_cooler_tm_nth_register_ltf(void)
{
#if 0				/* nth is not a cooler for now */
	int i;

	pr_debug("register ltf\n");

	for (i = MAX_NUM_INSTANCE_MTK_COOLER_TM_NTH; i-- > 0;) {
		char temp[20] = { 0 };

		sprintf(temp, "mtk-cl-tm_nth%02d", i);
		cl_tm_nth_dev[i] = mtk_thermal_cooling_device_register(temp, (void *)&cl_tm_nth_state[i],
								       &mtk_cl_tm_nth_ops);
	}
#endif
	return 0;
}

static void mtk_cooler_tm_nth_unregister_ltf(void)
{
#if 0				/* nth is not a cooler for now */
	int i;

	pr_debug("unregister ltf\n");

	for (i = MAX_NUM_INSTANCE_MTK_COOLER_TM_NTH; i-- > 0;) {
		if (cl_tm_nth_dev[i]) {
			mtk_thermal_cooling_device_unregister(cl_tm_nth_dev[i]);
			cl_tm_nth_dev[i] = NULL;
			cl_tm_nth_state[i] = 0;
		}
	}
#endif
}

static ssize_t _cl_tm_nth_write(struct file *filp, const char __user *buf, size_t len,
				loff_t *data)
{
	/* int ret = 0; */
	char tmp[512] = { 0 };
	int nth_on = 0;
	int klog_on;
	int tmp_NTHNTHTHRESENTER, tmp_NTHNTHTHRESEXIT, tmp_CPULOADSMASAMPLECNT, tmp_TGTTEMP,
	    tmp_KPINIT, tmp_KIINIT, tmp_KDINIT, tmp_NTHPOLLINGINTERVAL;
	int tmp_CPU_POWER_LIMIT0, tmp_CPU_POWER_LIMIT1, tmp_CPU_POWER_LIMIT2, tmp_CPU_POWER_LIMIT3,
	    tmp_CPU_POWER_LIMIT4, tmp_CPU_POWER_LIMIT5, tmp_CPU_POWER_LIMIT6;
	len = (len < (512 - 1)) ? len : (512 - 1);
	/* write data to the buffer */
	if (copy_from_user(tmp, buf, len))
		return -EFAULT;

    /**
     * sscanf format <on/off> <klog_on> <NTHNTHTHRESENTER> <NTHNTHTHRESEXIT> <CPULOADSMASAMPLECNT> <TGTTEMP> <KPINIT> <KIINIT> <KDINIT> <NTHPOLLINGINTERVAL> <CPU_POWER_LIMIT>[0~6]
     * <on/off> 0 to off, 1 to on
     * <klog_on> can only be 0 or 1
     * <NTHNTHTHRESENTER> 0~99
     * <NTHNTHTHRESEXIT> 0~99
     * <CPULOADSMASAMPLECNT> 0~20
     * <TGTTEMP> 1/1000 C
     * <KPINIT>
     * <KIINIT>
     * <KDINIT>
     * <NTHPOLLINGINTERVAL> ms
     * <CPU_POWER_LIMIT[0]>
     * <CPU_POWER_LIMIT[1]>
     * <CPU_POWER_LIMIT[2]>
     * <CPU_POWER_LIMIT[3]>
     * <CPU_POWER_LIMIT[4]>
     * <CPU_POWER_LIMIT[5]>
     * <CPU_POWER_LIMIT[6]>
     */

	if (NULL == data) {
		pr_debug("[%s] null data\n", __func__);
		return -EINVAL;
	}
	/* WARNING: Modify here if MTK_THERMAL_MONITOR_COOLER_MAX_EXTRA_CONDITIONS is changed to other than 3 */
	if (17 == sscanf(tmp, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
			 &nth_on, &klog_on, &tmp_NTHNTHTHRESENTER, &tmp_NTHNTHTHRESEXIT,
			 &tmp_CPULOADSMASAMPLECNT, &tmp_TGTTEMP, &tmp_KPINIT, &tmp_KIINIT,
			 &tmp_KDINIT, &tmp_NTHPOLLINGINTERVAL, &tmp_CPU_POWER_LIMIT0,
			 &tmp_CPU_POWER_LIMIT1, &tmp_CPU_POWER_LIMIT2, &tmp_CPU_POWER_LIMIT3,
			 &tmp_CPU_POWER_LIMIT4, &tmp_CPU_POWER_LIMIT5, &tmp_CPU_POWER_LIMIT6)) {
		if (nth_on == 0 || nth_on == 1)
			tm_nth_on = nth_on;

		if (tmp_NTHNTHTHRESENTER >= 0)
			NTHNTHTHRESENTER = tmp_NTHNTHTHRESENTER;

		if (tmp_NTHNTHTHRESEXIT >= 0)
			NTHNTHTHRESEXIT = tmp_NTHNTHTHRESEXIT;

		if (tmp_CPULOADSMASAMPLECNT > 0 && tmp_CPULOADSMASAMPLECNT < 21)
			CPULOADSMASAMPLECNT = tmp_CPULOADSMASAMPLECNT;

		TGTTEMP = tmp_TGTTEMP;

		if (tmp_KPINIT > 0)
			KPINIT = tmp_KPINIT;

		if (tmp_KIINIT > 0)
			KIINIT = tmp_KIINIT;

		if (tmp_KDINIT < 0)
			KDINIT = tmp_KDINIT;

		if (tmp_NTHPOLLINGINTERVAL >= 0)
			NTHPOLLINGINTERVAL = tmp_NTHPOLLINGINTERVAL;

		CPU_POWER_LIMIT[0] = tmp_CPU_POWER_LIMIT0;
		CPU_POWER_LIMIT[1] = tmp_CPU_POWER_LIMIT1;
		CPU_POWER_LIMIT[2] = tmp_CPU_POWER_LIMIT2;
		CPU_POWER_LIMIT[3] = tmp_CPU_POWER_LIMIT3;
		CPU_POWER_LIMIT[4] = tmp_CPU_POWER_LIMIT4;
		CPU_POWER_LIMIT[5] = tmp_CPU_POWER_LIMIT5;
		CPU_POWER_LIMIT[6] = tmp_CPU_POWER_LIMIT6;

		tm_nth_loop(NULL);

		return len;
	}

	return -EINVAL;
}

static int _cl_tm_nth_read(struct seq_file *m, void *v)
{
    /**
     * The format to print out:
     *  kernel_log <0 or 1>
     *  <mtk-cl-tm_nth<ID>> <bcc limit>
     *  ..
     */

	pr_debug("[%s]\n", __func__);

	{
		seq_printf(m, "nth on %d\n", tm_nth_on);
		seq_printf(m, "NTHNTHTHRESENTER %d\n", NTHNTHTHRESENTER);
		seq_printf(m, "NTHNTHTHRESEXIT %d\n", NTHNTHTHRESEXIT);
		seq_printf(m, "CPULOADSMASAMPLECNT %d\n", CPULOADSMASAMPLECNT);
		seq_printf(m, "TGTTEMP %d\n", TGTTEMP);
		seq_printf(m, "KPINIT %d\n", KPINIT);
		seq_printf(m, "KIINIT %d\n", KIINIT);
		seq_printf(m, "KDINIT %d\n", KDINIT);
		seq_printf(m, "NTHPOLLINGINTERVAL %d\n", NTHPOLLINGINTERVAL);
		seq_printf(m, "CPU_POWER_LIMIT %d %d %d %d %d %d %d\n", CPU_POWER_LIMIT[0],
			   CPU_POWER_LIMIT[1], CPU_POWER_LIMIT[2], CPU_POWER_LIMIT[3],
			   CPU_POWER_LIMIT[4], CPU_POWER_LIMIT[5], CPU_POWER_LIMIT[6]);
	}

	return 0;
}

static int _cl_tm_nth_open(struct inode *inode, struct file *file)
{
	return single_open(file, _cl_tm_nth_read, PDE_DATA(inode));
}

static const struct file_operations _cl_tm_nth_fops = {
	.owner = THIS_MODULE,
	.open = _cl_tm_nth_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = _cl_tm_nth_write,
	.release = single_release,
};

#define CPU_USAGE_CURRENT_FIELD (0)
#define CPU_USAGE_SAVE_FIELD    (1)
#define CPU_USAGE_FRAME_FIELD   (2)

struct cpu_index_st {
	unsigned long u[3];
	unsigned long s[3];
	unsigned long n[3];
	unsigned long i[3];
	unsigned long w[3];
	unsigned long q[3];
	unsigned long sq[3];
	unsigned long tot_frme;
	unsigned long tz;
	int usage;
	int freq;
};

#define NO_CPU_CORES (8)
static struct cpu_index_st cpu_index_list[NO_CPU_CORES];
static int cpuloadings[NO_CPU_CORES];

#define TRIMz_ex(tz, x)   ((tz = (unsigned long long)(x)) < 0 ? 0 : tz)

#include <linux/kernel_stat.h>
#include <linux/cpumask.h>
#include <asm/cputime.h>
#include <linux/sched.h>
#include <linux/tick.h>
#include <linux/time.h>

#ifdef arch_idle_time

static cputime64_t get_idle_time(int cpu)
{
	cputime64_t idle;

	idle = kcpustat_cpu(cpu).cpustat[CPUTIME_IDLE];
	if (cpu_online(cpu) && !nr_iowait_cpu(cpu))
		idle += arch_idle_time(cpu);
	return idle;
}

static cputime64_t get_iowait_time(int cpu)
{
	cputime64_t iowait;

	iowait = kcpustat_cpu(cpu).cpustat[CPUTIME_IOWAIT];
	if (cpu_online(cpu) && nr_iowait_cpu(cpu))
		iowait += arch_idle_time(cpu);
	return iowait;
}

#else

static u64 get_idle_time(int cpu)
{
	u64 idle, idle_time = -1ULL;

	if (cpu_online(cpu))
		idle_time = get_cpu_idle_time_us(cpu, NULL);

	if (idle_time == -1ULL)
		/* !NO_HZ or cpu offline so we can rely on cpustat.idle */
		idle = kcpustat_cpu(cpu).cpustat[CPUTIME_IDLE];
	else
		idle = usecs_to_cputime64(idle_time);

	return idle;
}

static u64 get_iowait_time(int cpu)
{
	u64 iowait, iowait_time = -1ULL;

	if (cpu_online(cpu))
		iowait_time = get_cpu_iowait_time_us(cpu, NULL);

	if (iowait_time == -1ULL)
		/* !NO_HZ or cpu offline so we can rely on cpustat.iowait */
		iowait = kcpustat_cpu(cpu).cpustat[CPUTIME_IOWAIT];
	else
		iowait = usecs_to_cputime64(iowait_time);

	return iowait;
}

#endif

static int tm_nth_get_sys_cpu_usage_info_ex(void)
{
	int nCoreIndex = 0, i;

	for_each_online_cpu(nCoreIndex) {
		/* Get CPU Info */
		cpu_index_list[nCoreIndex].u[CPU_USAGE_CURRENT_FIELD] =
		    kcpustat_cpu(nCoreIndex).cpustat[CPUTIME_USER];
		cpu_index_list[nCoreIndex].n[CPU_USAGE_CURRENT_FIELD] =
		    kcpustat_cpu(nCoreIndex).cpustat[CPUTIME_NICE];
		cpu_index_list[nCoreIndex].s[CPU_USAGE_CURRENT_FIELD] =
		    kcpustat_cpu(nCoreIndex).cpustat[CPUTIME_SYSTEM];
		cpu_index_list[nCoreIndex].i[CPU_USAGE_CURRENT_FIELD] = get_idle_time(nCoreIndex);
		cpu_index_list[nCoreIndex].w[CPU_USAGE_CURRENT_FIELD] = get_iowait_time(nCoreIndex);
		cpu_index_list[nCoreIndex].q[CPU_USAGE_CURRENT_FIELD] =
		    kcpustat_cpu(nCoreIndex).cpustat[CPUTIME_IRQ];
		cpu_index_list[nCoreIndex].sq[CPU_USAGE_CURRENT_FIELD] =
		    kcpustat_cpu(nCoreIndex).cpustat[CPUTIME_SOFTIRQ];

		/* Frame */
		cpu_index_list[nCoreIndex].u[CPU_USAGE_FRAME_FIELD] =
		    cpu_index_list[nCoreIndex].u[CPU_USAGE_CURRENT_FIELD] -
		    cpu_index_list[nCoreIndex].u[CPU_USAGE_SAVE_FIELD];
		cpu_index_list[nCoreIndex].n[CPU_USAGE_FRAME_FIELD] =
		    cpu_index_list[nCoreIndex].n[CPU_USAGE_CURRENT_FIELD] -
		    cpu_index_list[nCoreIndex].n[CPU_USAGE_SAVE_FIELD];
		cpu_index_list[nCoreIndex].s[CPU_USAGE_FRAME_FIELD] =
		    cpu_index_list[nCoreIndex].s[CPU_USAGE_CURRENT_FIELD] -
		    cpu_index_list[nCoreIndex].s[CPU_USAGE_SAVE_FIELD];
		cpu_index_list[nCoreIndex].i[CPU_USAGE_FRAME_FIELD] =
		    TRIMz_ex(cpu_index_list[nCoreIndex].tz,
			     (cpu_index_list[nCoreIndex].i[CPU_USAGE_CURRENT_FIELD] -
			      cpu_index_list[nCoreIndex].i[CPU_USAGE_SAVE_FIELD]));
		cpu_index_list[nCoreIndex].w[CPU_USAGE_FRAME_FIELD] =
		    cpu_index_list[nCoreIndex].w[CPU_USAGE_CURRENT_FIELD] -
		    cpu_index_list[nCoreIndex].w[CPU_USAGE_SAVE_FIELD];
		cpu_index_list[nCoreIndex].q[CPU_USAGE_FRAME_FIELD] =
		    cpu_index_list[nCoreIndex].q[CPU_USAGE_CURRENT_FIELD] -
		    cpu_index_list[nCoreIndex].q[CPU_USAGE_SAVE_FIELD];
		cpu_index_list[nCoreIndex].sq[CPU_USAGE_FRAME_FIELD] =
		    cpu_index_list[nCoreIndex].sq[CPU_USAGE_CURRENT_FIELD] -
		    cpu_index_list[nCoreIndex].sq[CPU_USAGE_SAVE_FIELD];

		/* Total Frame */
		cpu_index_list[nCoreIndex].tot_frme =
		    cpu_index_list[nCoreIndex].u[CPU_USAGE_FRAME_FIELD] +
		    cpu_index_list[nCoreIndex].n[CPU_USAGE_FRAME_FIELD] +
		    cpu_index_list[nCoreIndex].s[CPU_USAGE_FRAME_FIELD] +
		    cpu_index_list[nCoreIndex].i[CPU_USAGE_FRAME_FIELD] +
		    cpu_index_list[nCoreIndex].w[CPU_USAGE_FRAME_FIELD] +
		    cpu_index_list[nCoreIndex].q[CPU_USAGE_FRAME_FIELD] +
		    cpu_index_list[nCoreIndex].sq[CPU_USAGE_FRAME_FIELD];

		/* CPU Usage */
		if (cpu_index_list[nCoreIndex].tot_frme > 0) {
			cpuloadings[nCoreIndex] =
			    (100 -
			     (((int)cpu_index_list[nCoreIndex].i[CPU_USAGE_FRAME_FIELD] * 100) /
			      (int)cpu_index_list[nCoreIndex].tot_frme));
		} else {
			/* CPU unplug case */
			cpuloadings[nCoreIndex] = 0;
		}

		cpu_index_list[nCoreIndex].u[CPU_USAGE_SAVE_FIELD] =
		    cpu_index_list[nCoreIndex].u[CPU_USAGE_CURRENT_FIELD];
		cpu_index_list[nCoreIndex].n[CPU_USAGE_SAVE_FIELD] =
		    cpu_index_list[nCoreIndex].n[CPU_USAGE_CURRENT_FIELD];
		cpu_index_list[nCoreIndex].s[CPU_USAGE_SAVE_FIELD] =
		    cpu_index_list[nCoreIndex].s[CPU_USAGE_CURRENT_FIELD];
		cpu_index_list[nCoreIndex].i[CPU_USAGE_SAVE_FIELD] =
		    cpu_index_list[nCoreIndex].i[CPU_USAGE_CURRENT_FIELD];
		cpu_index_list[nCoreIndex].w[CPU_USAGE_SAVE_FIELD] =
		    cpu_index_list[nCoreIndex].w[CPU_USAGE_CURRENT_FIELD];
		cpu_index_list[nCoreIndex].q[CPU_USAGE_SAVE_FIELD] =
		    cpu_index_list[nCoreIndex].q[CPU_USAGE_CURRENT_FIELD];
		cpu_index_list[nCoreIndex].sq[CPU_USAGE_SAVE_FIELD] =
		    cpu_index_list[nCoreIndex].sq[CPU_USAGE_CURRENT_FIELD];

		pr_debug("CPU%d Frame:%ld USAGE:%d\n", nCoreIndex,
			 cpu_index_list[nCoreIndex].tot_frme, cpuloadings[nCoreIndex]);

		for (i = 0; i < 3; i++) {
			pr_debug
			    ("Index %d [u:%ld] [n:%ld] [s:%ld] [i:%ld] [w:%ld] [q:%ld] [sq:%ld]\n",
			     i, cpu_index_list[nCoreIndex].u[i], cpu_index_list[nCoreIndex].n[i],
			     cpu_index_list[nCoreIndex].s[i], cpu_index_list[nCoreIndex].i[i],
			     cpu_index_list[nCoreIndex].w[i], cpu_index_list[nCoreIndex].q[i],
			     cpu_index_list[nCoreIndex].sq[i]);

		}
	}

	return 0;

}

int tm_nth_get_cpu_load_sum(void)
{
	int i, sum = 0;

	tm_nth_get_sys_cpu_usage_info_ex();

	for (i = 0; i < NO_CPU_CORES; i++)
		sum += cpuloadings[i];

	return sum;
}

static void tm_nth_set_cpu_power(int scale)
{
	static int set_scale = -1;

	if (scale < 0)
		scale = 0;
	else if (scale > 1000)
		scale = 1000;

	if (set_scale == scale)
		return;

	set_scale = scale;

	if (set_scale < 125)
		mt_cpufreq_thermal_protect(CPU_POWER_LIMIT[0], 0);
	else if (set_scale < 250)
		mt_cpufreq_thermal_protect(CPU_POWER_LIMIT[1], 0);
	else if (set_scale < 375)
		mt_cpufreq_thermal_protect(CPU_POWER_LIMIT[2], 0);
	else if (set_scale < 500)
		mt_cpufreq_thermal_protect(CPU_POWER_LIMIT[3], 0);
	else if (set_scale < 625)
		mt_cpufreq_thermal_protect(CPU_POWER_LIMIT[4], 0);
	else if (set_scale < 750)
		mt_cpufreq_thermal_protect(CPU_POWER_LIMIT[5], 0);
	else if (set_scale < 875)
		mt_cpufreq_thermal_protect(CPU_POWER_LIMIT[6], 0);
	else
		mt_cpufreq_thermal_protect(0, 0);	/* unlimit */
}

static void tm_nth_loop(struct work_struct *work)
{
	if (1 == tm_nth_on) {
		/* get loading and count moving avg */
		cpu_load[cpu_load_idx] = tm_nth_get_cpu_load_sum();
		/* TODO: moving avg */
		cpu_load_sma =
		    (cpu_load_sma * (CPULOADSMASAMPLECNT - 1) +
		     cpu_load[cpu_load_idx]) / CPULOADSMASAMPLECNT;
		cpu_load_idx = (cpu_load_idx + 1) % 20;

		/* check state */

		/* if loading over threshold and state not working, start NTH */
		if ((cpu_load_sma >= NTHNTHTHRESENTER) && (nth_state == 0)) {
			/* start NTH */
			nth_state = 1;
		}
		/* if loading under exit threshold and state working, exit NTH */
		else if ((cpu_load_sma < NTHNTHTHRESEXIT) && (nth_state == 1)) {
			/* exit NTH */
			nth_state = 0;
		}
		/* others no state change */
		if (nth_state == 1) {
			/* get temp and prev temp */
			if (unlikely(prv_cpu_temp == -127000))
				prv_cpu_temp = cur_cpu_temp = tscpu_get_bL_temp(THERMAL_BANK1);
			else
				prv_cpu_temp = cur_cpu_temp;
			cur_cpu_temp = tscpu_get_bL_temp(THERMAL_BANK1);

			/* calc tempError, tempErrorInt, tempErrotDer */
			tempError = TGTTEMP - cur_cpu_temp;
			tempErrorInt += tempError;
			tempErrorDer = tempError - (TGTTEMP - prv_cpu_temp);
			if (tempErrorInt / KIINIT > 1000)
				tempErrorInt = 1000 * KIINIT;
			else if (tempErrorInt < 0)
				tempErrorInt = 0;

			/* calc currScale */
			curScale =
			    tempError / KPINIT + tempErrorInt / KIINIT + tempErrorDer / KDINIT;

			/* set DVFS OPP */
			tm_nth_set_cpu_power(curScale);
		} else {
			/* reset all var */
			prv_cpu_temp = -127000;
			curScale = 1000;

			/* unlimit CPU */
			tm_nth_set_cpu_power(curScale);
		}

		pr_debug
		    ("cpu_load %d state %d curTemp %d tempError %d tempErrorInt %d tempErrorDer %d curScale %d\n",
		     cpu_load_sma, nth_state, cur_cpu_temp, tempError, tempErrorInt, tempErrorDer,
		     curScale);

		cancel_delayed_work(&tm_nth_poll_queue);
		/* freeze when suspend */
		queue_delayed_work(system_freezable_wq, &tm_nth_poll_queue, msecs_to_jiffies(NTHPOLLINGINTERVAL));
	}
}

static int __init mtk_cooler_tm_nth_init(void)
{
	int err = 0;
	int i;

	for (i = MAX_NUM_INSTANCE_MTK_COOLER_TM_NTH; i-- > 0;) {
		cl_tm_nth_dev[i] = NULL;
		cl_tm_nth_state[i] = 0;
	}

	pr_debug("[%s]\n", __func__);

	err = mtk_cooler_tm_nth_register_ltf();
	if (err)
		goto err_unreg;

	/* create a proc file */
	{
		struct proc_dir_entry *entry = NULL;

		entry =
		    proc_create("driver/tm_nth", S_IRUGO | S_IWUSR | S_IWGRP, NULL,
				&_cl_tm_nth_fops);
		if (!entry)
			pr_debug("[%s] driver/tm_nth creation failed\n", __func__);
		else
			proc_set_user(entry, uid, gid);

	}

	INIT_DELAYED_WORK(&tm_nth_poll_queue, tm_nth_loop);
	tm_nth_loop(NULL);

	return 0;

err_unreg:
	mtk_cooler_tm_nth_unregister_ltf();
	return err;
}

static void __exit mtk_cooler_tm_nth_exit(void)
{
	pr_debug("[%s]\n", __func__);

	/* remove the proc file */
	remove_proc_entry("driver/tm_nth", NULL);

	mtk_cooler_tm_nth_unregister_ltf();
}
module_init(mtk_cooler_tm_nth_init);
module_exit(mtk_cooler_tm_nth_exit);
