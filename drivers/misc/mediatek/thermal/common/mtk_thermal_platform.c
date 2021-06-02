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
 */

#include <linux/uaccess.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/dmi.h>
#include <linux/acpi.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/err.h>
#include <linux/syscalls.h>
#include <linux/time.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/bug.h>
#include <linux/workqueue.h>
#include <mach/mtk_thermal.h>
#include <mt-plat/aee.h>
#include <mt-plat/mtk_thermal_platform.h>
#include <tscpu_settings.h>
/* ************************************ */
/* Definition */
/* ************************************ */

/* Number of CPU CORE */
#define NUMBER_OF_CORE (8)

/* This function pointer is for GPU LKM to register
 * a function to get GPU loading.
 */
unsigned long (*mtk_thermal_get_gpu_loading_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_thermal_get_gpu_loading_fp);

bool __attribute__ ((weak))
mtk_get_gpu_loading(unsigned int *pLoading)
{
#ifdef CONFIG_MTK_GPU_SUPPORT
	pr_notice("E_WF: %s doesn't exist\n", __func__);
#endif
	return 0;
}

int __attribute__ ((weak))
force_get_tbat(void)
{
	pr_notice("E_WF: %s doesn't exist\n", __func__);
	return 30;
}

unsigned int __attribute__ ((weak))
mt_gpufreq_get_cur_freq(void)
{
	return 0;
}

/* ************************************ */
/* Global Variable */
/* ************************************ */
static bool enable_ThermalMonitor;

static DEFINE_MUTEX(MTM_SYSINFO_LOCK);

/* ************************************ */
/* Macro */
/* ************************************ */
#define THRML_LOG(fmt, args...) \
do { \
	if (enable_ThermalMonitor)\
		pr_notice("THERMAL/PLATFORM" fmt, ##args); \
} while (0)


#define THRML_ERROR_LOG(fmt, args...) \
	pr_notice("THERMAL/PLATFORM" fmt, ##args)

/* ************************************ */
/* Define */
/* ************************************ */

/* ********************************************* */
/* For get_sys_cpu_usage_info_ex() */
/* ********************************************* */

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

struct gpu_index_st {
	int usage;
	int freq;
};

#define NO_CPU_CORES (TZCPU_NO_CPU_CORES)
				/* /< 4-Core is maximum */
static struct cpu_index_st cpu_index_list[NO_CPU_CORES];
static int cpufreqs[NO_CPU_CORES];
static int cpuloadings[NO_CPU_CORES];

#define SEEK_BUFF(x, c)	\
do { \
	while (*x != c)\
		x++; \
	x++; \
} while (0)


#define TRIMz_ex(tz, x)   ((tz = (unsigned long long)(x)) < 0 ? 0 : tz)

/* ********************************************* */
/* CPU Index */
/* ********************************************* */
#include <linux/kernel_stat.h>
#include <linux/cpumask.h>
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
		idle = idle_time * NSEC_PER_USEC;

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
		iowait = iowait_time * NSEC_PER_USEC;

	return iowait;
}

#endif

static int get_sys_cpu_usage_info_ex(void)
{
	int nCoreIndex = 0, i;

	for (i = 0; i < NO_CPU_CORES; i++)
		cpuloadings[i] = 0;

	for_each_online_cpu(nCoreIndex) {
		if (nCoreIndex >= NO_CPU_CORES) {
			#ifdef CONFIG_MTK_AEE_FEATURE
			aee_kernel_warning("thermal",
				"nCoreIndex %d over NO_CPU_CORES %d\n",
				nCoreIndex, NO_CPU_CORES);
			#endif
			return 0;
		}
		/* Get CPU Info */
		cpu_index_list[nCoreIndex].u[CPU_USAGE_CURRENT_FIELD] =
		    kcpustat_cpu(nCoreIndex).cpustat[CPUTIME_USER];

		cpu_index_list[nCoreIndex].n[CPU_USAGE_CURRENT_FIELD] =
		    kcpustat_cpu(nCoreIndex).cpustat[CPUTIME_NICE];

		cpu_index_list[nCoreIndex].s[CPU_USAGE_CURRENT_FIELD] =
		    kcpustat_cpu(nCoreIndex).cpustat[CPUTIME_SYSTEM];

		cpu_index_list[nCoreIndex].i[CPU_USAGE_CURRENT_FIELD] =
						get_idle_time(nCoreIndex);

		cpu_index_list[nCoreIndex].w[CPU_USAGE_CURRENT_FIELD] =
						get_iowait_time(nCoreIndex);

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
			(((int)cpu_index_list[nCoreIndex]
			.i[CPU_USAGE_FRAME_FIELD] * 100) /
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

		THRML_LOG("CPU%d Frame:%lu USAGE:%d\n", nCoreIndex,
				cpu_index_list[nCoreIndex].tot_frme,
				cpuloadings[nCoreIndex]);

		for (i = 0; i < 3; i++) {
			THRML_LOG(
				"Index %d [u:%lu] [n:%lu] [s:%lu] [i:%lu] [w:%lu] [q:%lu] [sq:%lu]\n",
					i, cpu_index_list[nCoreIndex].u[i],
					cpu_index_list[nCoreIndex].n[i],
					cpu_index_list[nCoreIndex].s[i],
					cpu_index_list[nCoreIndex].i[i],
					cpu_index_list[nCoreIndex].w[i],
					cpu_index_list[nCoreIndex].q[i],
			     cpu_index_list[nCoreIndex].sq[i]);

		}
	}

	return 0;

}

static bool dmips_limit_warned;
static int check_dmips_limit;

#include <linux/cpufreq.h>
static int get_sys_all_cpu_freq_info(void)
{
	int i;
	int cpu_total_dmips = 0;

	for (i = 0; i < NO_CPU_CORES; i++) {
		cpufreqs[i] = cpufreq_quick_get(i) / 1000;	/* MHz */
		cpu_total_dmips += cpufreqs[i];
	}

	cpu_total_dmips /= 1000;
	/* TODO: think a way to easy start and stop, and start for only once */
	if (check_dmips_limit == 1) {
		if (cpu_total_dmips > mtktscpu_limited_dmips) {
			THRML_ERROR_LOG("cpu %d over limit %d\n",
					cpu_total_dmips,
					mtktscpu_limited_dmips);

			if (dmips_limit_warned == false) {
				#ifdef CONFIG_MTK_AEE_FEATURE
				aee_kernel_warning("thermal",
						"cpu %d over limit %d\n",
						cpu_total_dmips,
						mtktscpu_limited_dmips);
				#endif
				dmips_limit_warned = true;
			}
		}
	}

	return 0;
}

static int mtk_thermal_validation_rd(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", check_dmips_limit);

	return 0;
}

static ssize_t mtk_thermal_validation_wr
(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[32];
	int check_switch;
	int len = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (kstrtoint(desc, 10, &check_switch) == 0) {
		if (check_switch == 1) {
			dmips_limit_warned = false;
			check_dmips_limit = check_switch;
		} else if (check_switch == 0) {
			check_dmips_limit = check_switch;
		}
		return count;
	}
	THRML_ERROR_LOG("[%s] bad argument\n", __func__);
	return -EINVAL;
}

static int mtk_thermal_validation_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtk_thermal_validation_rd, NULL);
}

static const struct file_operations mtk_thermal_validation_fops = {
	.owner = THIS_MODULE,
	.open = mtk_thermal_validation_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mtk_thermal_validation_wr,
	.release = single_release,
};


/* Init */
static int __init mtk_thermal_platform_init(void)
{
	int err = 0;
	struct proc_dir_entry *entry;

	entry = proc_create("driver/tm_validation", 0644, NULL,
			&mtk_thermal_validation_fops);
	if (!entry) {
		THRML_ERROR_LOG(
			"[%s] Can not create /proc/driver/tm_validation\n",
			__func__);
	}

	return err;
}

/* Exit */
static void __exit mtk_thermal_platform_exit(void)
{

}

int mtk_thermal_get_cpu_info(int *nocores, int **cpufreq, int **cpuloading)
{
	/* ****************** */
	/* CPU Usage */
	/* ****************** */
	mutex_lock(&MTM_SYSINFO_LOCK);

	/* Read CPU Usage Information */
	get_sys_cpu_usage_info_ex();

	get_sys_all_cpu_freq_info();

	mutex_unlock(&MTM_SYSINFO_LOCK);

	if (nocores)
		*nocores = NO_CPU_CORES;

	if (cpufreq)
		*cpufreq = cpufreqs;

	if (cpuloading)
		*cpuloading = cpuloadings;

	return 0;
}
EXPORT_SYMBOL(mtk_thermal_get_cpu_info);

#define NO_GPU_CORES (1)
static int gpufreqs[NO_GPU_CORES];
static int gpuloadings[NO_GPU_CORES];

int mtk_thermal_get_gpu_info(int *nocores, int **gpufreq, int **gpuloading)
{
	/* ****************** */
	/* GPU Index */
	/* ****************** */
	THRML_LOG("[%s]\n", __func__);

	if (nocores)
		*nocores = NO_GPU_CORES;

	if (gpufreq) {
		gpufreqs[0] = mt_gpufreq_get_cur_freq() / 1000;	/* MHz */
		*gpufreq = gpufreqs;
	}

	if (gpuloading) {
		unsigned int rd_gpu_loading = 0;

		if (mtk_get_gpu_loading(&rd_gpu_loading)) {
			gpuloadings[0] = (int)rd_gpu_loading;
			*gpuloading = gpuloadings;
		}

	}

	return 0;
}
EXPORT_SYMBOL(mtk_thermal_get_gpu_info);

/* ********************************************* */
/* Get Extra Info */
/* ********************************************* */


enum {
/*	TXPWR_MD1 = 0,
 *	TXPWR_MD2 =1,
 *	RFTEMP_2G_MD1 =2,
 *	RFTEMP_2G_MD2 = 3,
 *	RFTEMP_3G_MD1 = 4,
 *	RFTEMP_3G_MD2 = 5,
 */
	WiFi_TP = 6,
	Mobile_TP = 7,
	NO_EXTRA_THERMAL_ATTR
};

int mtk_thermal_force_get_batt_temp(void)
{
	int ret = 0;

	ret = force_get_tbat();

	return ret;
}
EXPORT_SYMBOL(mtk_thermal_force_get_batt_temp);

static unsigned int _thermal_scen;

unsigned int mtk_thermal_set_user_scenarios(unsigned int mask)
{
	/* only one scen is handled now... */
	if ((mask & MTK_THERMAL_SCEN_CALL)) {
		/* make mtk_ts_cpu.c aware of call scenario */
		set_taklking_flag(true);
		_thermal_scen |= (unsigned int)MTK_THERMAL_SCEN_CALL;
	}
	return _thermal_scen;
}
EXPORT_SYMBOL(mtk_thermal_set_user_scenarios);

unsigned int mtk_thermal_clear_user_scenarios(unsigned int mask)
{
	/* only one scen is handled now... */
	if ((mask & MTK_THERMAL_SCEN_CALL)) {
		/* make mtk_ts_cpu.c aware of call scenario */
		set_taklking_flag(false);
		_thermal_scen &= ~((unsigned int)MTK_THERMAL_SCEN_CALL);
	}
	return _thermal_scen;
}
EXPORT_SYMBOL(mtk_thermal_clear_user_scenarios);

module_init(mtk_thermal_platform_init);
module_exit(mtk_thermal_platform_exit);
