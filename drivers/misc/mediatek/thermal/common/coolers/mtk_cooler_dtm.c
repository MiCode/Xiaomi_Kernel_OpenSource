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
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <mt-plat/aee.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include "mt-plat/mtk_thermal_monitor.h"
#include "mach/mt_thermal.h"
#include <mach/mt_clkmgr.h>
#include <mt_ptp.h>
#include <mach/wd_api.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <tscpu_settings.h>
#include <ap_thermal_limit.h>
#if 0
#if defined(ATM_USES_PPM)
#include "mach/mt_ppm_api.h"
#else
#include "mt_cpufreq.h"
#endif
#endif

/*=============================================================
 *Local variable definition
 *=============================================================*/
int tscpu_cpu_dmips[CPU_COOLER_NUM] = { 0 };
int mtktscpu_limited_dmips = 0;	/* Use in mtk_thermal_platform.c */
static int previous_step = -1;
static unsigned int *cl_dev_state;
static int Num_of_OPP;
static struct thermal_cooling_device **cl_dev;
static char *cooler_name;
static unsigned int prv_stc_cpu_pwr_lim;
static unsigned int prv_stc_gpu_pwr_lim;
unsigned int static_cpu_power_limit = 0x7FFFFFFF;
unsigned int static_gpu_power_limit = 0x7FFFFFFF;
static struct apthermolmt_user ap_dtm;
static char *ap_dtm_log = "ap_dtm";

/*=============================================================
 *Local function prototype
 *=============================================================*/
static void set_static_cpu_power_limit(unsigned int limit);
static void set_static_gpu_power_limit(unsigned int limit);

/*=============================================================
 *Weak functions
 *=============================================================*/
#if 0
#if defined(ATM_USES_PPM)
void __attribute__ ((weak))
mt_ppm_cpu_thermal_protect(unsigned int limited_power)
{
	pr_err("E_WF: %s doesn't exist\n", __func__);
}
#else
void __attribute__ ((weak))
mt_cpufreq_thermal_protect(unsigned int limited_power)
{
	pr_err("E_WF: %s doesn't exist\n", __func__);
}
#endif
#endif
/*=============================================================*/
static void set_static_cpu_power_limit(unsigned int limit)
{
	prv_stc_cpu_pwr_lim = static_cpu_power_limit;
	static_cpu_power_limit = (limit != 0) ? limit : 0x7FFFFFFF;

	if (prv_stc_cpu_pwr_lim != static_cpu_power_limit) {
#ifdef FAST_RESPONSE_ATM
		tscpu_printk("%s %d, T=%d\n", __func__,
			(static_cpu_power_limit != 0x7FFFFFFF) ? static_cpu_power_limit : 0,
			tscpu_get_curr_max_ts_temp());
#else
		tscpu_printk("%s %d, T=%d\n", __func__,
			(static_cpu_power_limit != 0x7FFFFFFF) ? static_cpu_power_limit : 0,
			tscpu_get_curr_temp());
#endif

		apthermolmt_set_cpu_power_limit(&ap_dtm, static_cpu_power_limit);
	}
}

static void set_static_gpu_power_limit(unsigned int limit)
{
	prv_stc_gpu_pwr_lim = static_gpu_power_limit;
	static_gpu_power_limit = (limit != 0) ? limit : 0x7FFFFFFF;

	if (prv_stc_gpu_pwr_lim != static_gpu_power_limit) {
		tscpu_printk("%s %d\n", __func__,
			(static_gpu_power_limit != 0x7FFFFFFF) ? static_gpu_power_limit : 0);

		apthermolmt_set_gpu_power_limit(&ap_dtm, static_gpu_power_limit);
	}
}

static int tscpu_set_power_consumption_state(void)
{
	int i = 0;
	int power = 0;

	tscpu_dprintk("%s Num_of_OPP=%d\n", __func__, Num_of_OPP);

	/* in 92, Num_of_OPP=34 */
	for (i = 0; i < Num_of_OPP; i++) {
		if (1 == cl_dev_state[i]) {
			if (i != previous_step) {
				tscpu_printk("%s prev=%d curr=%d\n", __func__, previous_step, i);
				previous_step = i;
				mtktscpu_limited_dmips = tscpu_cpu_dmips[previous_step];
				if (Num_of_GPU_OPP == 3) {
					power =
					    (i * 100 + 700) - mtk_gpu_power[Num_of_GPU_OPP -
									    1].gpufreq_power;
					set_static_cpu_power_limit(power);
					set_static_gpu_power_limit(mtk_gpu_power
								   [Num_of_GPU_OPP -
								    1].gpufreq_power);
					tscpu_dprintk
					    ("Num_of_GPU_OPP=%d, gpufreq_power=%d, power=%d\n",
					     Num_of_GPU_OPP,
					     mtk_gpu_power[Num_of_GPU_OPP - 1].gpufreq_power,
					     power);
				} else if (Num_of_GPU_OPP == 2) {
					power = (i * 100 + 700) - mtk_gpu_power[1].gpufreq_power;
					set_static_cpu_power_limit(power);
					set_static_gpu_power_limit(mtk_gpu_power[1].gpufreq_power);
					tscpu_dprintk
					    ("Num_of_GPU_OPP=%d, gpufreq_power=%d, power=%d\n",
					     Num_of_GPU_OPP, mtk_gpu_power[1].gpufreq_power, power);
				} else if (Num_of_GPU_OPP == 1) {
#if 0
					/* 653mW,GPU 500Mhz,1V(preloader default) */
					/* 1016mW,GPU 700Mhz,1.1V */
					power = (i * 100 + 700) - 653;
#else
					power = (i * 100 + 700) - mtk_gpu_power[0].gpufreq_power;
#endif
					set_static_cpu_power_limit(power);
					tscpu_dprintk
					    ("Num_of_GPU_OPP=%d, gpufreq_power=%d, power=%d\n",
					     Num_of_GPU_OPP, mtk_gpu_power[0].gpufreq_power, power);
				} else {	/* TODO: fix this, temp solution, this project has over 5 GPU OPP... */
					power = (i * 100 + 700);
					set_static_cpu_power_limit(power);
					tscpu_dprintk
					    ("Num_of_GPU_OPP=%d, gpufreq_power=%d, power=%d\n",
					     Num_of_GPU_OPP, mtk_gpu_power[0].gpufreq_power, power);
				}
			}
			break;
		}
	}

	/* If temp drop to our expect value, we need to restore initial cpu freq setting */
	if (i == Num_of_OPP) {
		if (previous_step != -1) {
			tscpu_printk("Free all static thermal limit, previous_opp=%d\n",
				     previous_step);
			previous_step = -1;
			mtktscpu_limited_dmips = tscpu_cpu_dmips[CPU_COOLER_NUM - 1];	/* highest dmips */
			set_static_cpu_power_limit(0);
			set_static_gpu_power_limit(0);
		}
	}
	return 0;
}

static int dtm_cpu_get_max_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = 1;
	return 0;
}

static int dtm_cpu_get_cur_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	int i = 0;

	for (i = 0; i < Num_of_OPP; i++) {
		if (!strcmp(cdev->type, &cooler_name[i * 20]))
			*state = cl_dev_state[i];
	}
	return 0;
}

static int dtm_cpu_set_cur_state(struct thermal_cooling_device *cdev, unsigned long state)
{
	int i = 0;

	for (i = 0; i < Num_of_OPP; i++) {
		if (!strcmp(cdev->type, &cooler_name[i * 20])) {
			cl_dev_state[i] = state;
			tscpu_set_power_consumption_state();
			break;
		}
	}
	return 0;
}

static struct thermal_cooling_device_ops mtktscpu_cooling_F0x2_ops = {
	.get_max_state = dtm_cpu_get_max_state,
	.get_cur_state = dtm_cpu_get_cur_state,
	.set_cur_state = dtm_cpu_set_cur_state,
};

/* Init local structure for AP coolers */
static int init_cooler(void)
{
	int i;
	int num = CPU_COOLER_NUM;	/* 700~4000, 92 */

	cl_dev_state = kzalloc((num) * sizeof(unsigned int), GFP_KERNEL);
	if (cl_dev_state == NULL)
		return -ENOMEM;

	cl_dev = kzalloc((num) * sizeof(struct thermal_cooling_device *), GFP_KERNEL);
	if (cl_dev == NULL)
		return -ENOMEM;

	cooler_name = kzalloc((num) * sizeof(char) * 20, GFP_KERNEL);
	if (cooler_name == NULL)
		return -ENOMEM;

	for (i = 0; i < num; i++)
		sprintf(cooler_name + (i * 20), "cpu%02d", i);	/* using index=>0=700,1=800 ~ 33=4000 */

	Num_of_OPP = num;	/* CPU COOLER COUNT, not CPU OPP count */
	return 0;
}

static int __init mtk_cooler_dtm_init(void)
{
	int err = 0, i;

	tscpu_dprintk("%s start\n", __func__);

	err = apthermolmt_register_user(&ap_dtm, ap_dtm_log);
	if (err < 0)
		return err;

	err = init_cooler();
	if (err) {
		tscpu_printk("%s fail\n", __func__);
		return err;
	}
	for (i = 0; i < Num_of_OPP; i++) {
		cl_dev[i] = mtk_thermal_cooling_device_register(&cooler_name[i * 20], NULL,
						&mtktscpu_cooling_F0x2_ops);
	}
/*
	if (err) {
		tscpu_printk("tscpu_register_DVFS_hotplug_cooler fail\n");
		return err;
	}
*/
	tscpu_dprintk("%s end\n", __func__);
	return 0;
}

static void __exit mtk_cooler_dtm_exit(void)
{
	int i;

	tscpu_dprintk("%s\n", __func__);
	for (i = 0; i < Num_of_OPP; i++) {
		if (cl_dev[i]) {
			mtk_thermal_cooling_device_unregister(cl_dev[i]);
			cl_dev[i] = NULL;
		}
	}

	apthermolmt_unregister_user(&ap_dtm);
}
module_init(mtk_cooler_dtm_init);
module_exit(mtk_cooler_dtm_exit);
