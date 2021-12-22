// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
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
#include "mach/mtk_thermal.h"
#if defined(CONFIG_MTK_CLKMGR)
#include <mach/mtk_clkmgr.h>
#else
#include <linux/clk.h>
#endif
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <tscpu_settings.h>
#include <ap_thermal_limit.h>

#include <linux/uidgid.h>

#if defined(THERMAL_VPU_SUPPORT)
#if defined(CONFIG_MTK_APUSYS_SUPPORT)
#include "apu_power_table.h"
#else
#include "vpu_dvfs.h"
#endif
#endif
#if defined(THERMAL_MDLA_SUPPORT)
#if defined(CONFIG_MTK_APUSYS_SUPPORT)
#include "apu_power_table.h"
#else
#include "mdla_dvfs.h"
#endif
#endif
/*=============================================================
 *Local variable definition
 *=============================================================
 */
#if defined(THERMAL_VPU_SUPPORT) || defined(THERMAL_MDLA_SUPPORT)
static kuid_t uid = KUIDT_INIT(0);
static kgid_t gid = KGIDT_INIT(1000);
#endif
int tscpu_cpu_dmips[CPU_COOLER_NUM] = { 0 };
int mtktscpu_limited_dmips = 1;	/* Use in mtk_thermal_platform.c */
static int previous_step = -1;
static unsigned int *cl_dev_state;
static int Num_of_OPP;
static struct thermal_cooling_device **cl_dev;
static char *cooler_name;
static unsigned int prv_stc_cpu_pwr_lim;
static unsigned int prv_stc_gpu_pwr_lim;
unsigned int static_cpu_power_limit = 0x7FFFFFFF;
unsigned int static_gpu_power_limit = 0x7FFFFFFF;
#if defined(THERMAL_VPU_SUPPORT)
static unsigned int prv_stc_vpu_pwr_lim;
unsigned int static_vpu_power_limit = 0x7FFFFFFF;
#endif
#if defined(THERMAL_MDLA_SUPPORT)
static unsigned int prv_stc_mdla_pwr_lim;
unsigned int static_mdla_power_limit = 0x7FFFFFFF;
#endif
static struct apthermolmt_user ap_dtm;
static char *ap_dtm_log = "ap_dtm";

/*=============================================================
 *Local function prototype
 *=============================================================
 */
static void set_static_cpu_power_limit(unsigned int limit);
static void set_static_gpu_power_limit(unsigned int limit);

/*=============================================================
 *Weak functions
 *=============================================================
 */

/*=============================================================
 */
static void set_static_cpu_power_limit(unsigned int limit)
{
	prv_stc_cpu_pwr_lim = static_cpu_power_limit;
	static_cpu_power_limit = (limit != 0) ? limit : 0x7FFFFFFF;

	if (prv_stc_cpu_pwr_lim != static_cpu_power_limit) {
#ifdef FAST_RESPONSE_ATM
		tscpu_printk("%s %d, T=%d\n", __func__,
				(static_cpu_power_limit != 0x7FFFFFFF) ?
				static_cpu_power_limit : 0,
			tscpu_get_curr_max_ts_temp());
#else
		tscpu_printk("%s %d, T=%d\n", __func__,
				(static_cpu_power_limit != 0x7FFFFFFF) ?
				static_cpu_power_limit : 0,
			tscpu_get_curr_temp());
#endif

		apthermolmt_set_cpu_power_limit(&ap_dtm,
					static_cpu_power_limit);
	}
}

static void set_static_gpu_power_limit(unsigned int limit)
{
	prv_stc_gpu_pwr_lim = static_gpu_power_limit;
	static_gpu_power_limit = (limit != 0) ? limit : 0x7FFFFFFF;

	if (prv_stc_gpu_pwr_lim != static_gpu_power_limit) {
		tscpu_printk("%s %d\n", __func__,
				(static_gpu_power_limit != 0x7FFFFFFF) ?
						static_gpu_power_limit : 0);

		apthermolmt_set_gpu_power_limit(&ap_dtm,
					static_gpu_power_limit);
	}
}

#if defined(THERMAL_VPU_SUPPORT)
static void set_static_vpu_power_limit(unsigned int limit)
{
	prv_stc_vpu_pwr_lim = static_vpu_power_limit;
	static_vpu_power_limit = (limit != 0) ? limit : 0x7FFFFFFF;

	if (prv_stc_vpu_pwr_lim != static_vpu_power_limit) {
		tscpu_printk("%s %d\n", __func__,
			(static_vpu_power_limit != 0x7FFFFFFF) ?
				static_vpu_power_limit : 0);

		apthermolmt_set_vpu_power_limit(&ap_dtm,
			static_vpu_power_limit);
	}
}
#endif

#if defined(THERMAL_MDLA_SUPPORT)
static void set_static_mdla_power_limit(unsigned int limit)
{
	prv_stc_mdla_pwr_lim = static_mdla_power_limit;
	static_mdla_power_limit = (limit != 0) ? limit : 0x7FFFFFFF;

	if (prv_stc_mdla_pwr_lim != static_mdla_power_limit) {
		tscpu_printk("%s %d\n", __func__,
			(static_mdla_power_limit != 0x7FFFFFFF) ?
						static_mdla_power_limit : 0);

		apthermolmt_set_mdla_power_limit(&ap_dtm,
					static_mdla_power_limit);
	}
}
#endif

static int tscpu_set_power_consumption_state(void)
{
	int i = 0;
	int power = 0;

	tscpu_dprintk("%s Num_of_OPP=%d\n", __func__, Num_of_OPP);

	/* in 92, Num_of_OPP=34 */
	for (i = 0; i < Num_of_OPP; i++) {
		if (cl_dev_state[i] == 1) {
			if (i != previous_step) {
				tscpu_printk("%s prev=%d curr=%d\n", __func__,
							previous_step, i);
				previous_step = i;
				mtktscpu_limited_dmips =
						tscpu_cpu_dmips[previous_step];

				if (Num_of_GPU_OPP == 3) {
					power = (i * 100 + 700) -
						mtk_gpu_power[Num_of_GPU_OPP-1]
								.gpufreq_power;

					set_static_cpu_power_limit(power);

					set_static_gpu_power_limit(
						mtk_gpu_power[Num_of_GPU_OPP-1]
								.gpufreq_power);

					tscpu_dprintk(
						"Num_of_GPU_OPP=%d, gpufreq_power=%d, power=%d\n",
					     Num_of_GPU_OPP,
						 mtk_gpu_power[Num_of_GPU_OPP-1]
								.gpufreq_power,
					     power);

				} else if (Num_of_GPU_OPP == 2) {
					power =	(i * 100 + 700) -
						mtk_gpu_power[1].gpufreq_power;

					set_static_cpu_power_limit(power);

					set_static_gpu_power_limit(
						mtk_gpu_power[1].gpufreq_power);

					tscpu_dprintk(
						"Num_of_GPU_OPP=%d, gpufreq_power=%d, power=%d\n",
						 Num_of_GPU_OPP,
						mtk_gpu_power[1].gpufreq_power,
						power);

				} else if (Num_of_GPU_OPP == 1) {
#if 0
					/* 653mW,GPU 500Mhz,1V
					 * (preloader default)
					 */
					/* 1016mW,GPU 700Mhz,1.1V */
					power = (i * 100 + 700) - 653;
#else
					power = (i * 100 + 700) -
						mtk_gpu_power[0].gpufreq_power;
#endif
					set_static_cpu_power_limit(power);
					tscpu_dprintk(
						"Num_of_GPU_OPP=%d, gpufreq_power=%d, power=%d\n",
						 Num_of_GPU_OPP,
						mtk_gpu_power[0].gpufreq_power,
						power);
				} else {/* TODO: fix this, temp solution
					 * , this project has over 5 GPU OPP...
					 */
					power = (i * 100 + 700);
					set_static_cpu_power_limit(power);
					tscpu_dprintk(
						"Num_of_GPU_OPP=%d, gpufreq_power=%d, power=%d\n",
						 Num_of_GPU_OPP,
						mtk_gpu_power[0].gpufreq_power,
						power);
				}
			}
			break;
		}
	}

	/* If temp drop to our expect value,
	 * we need to restore initial cpu freq setting
	 */
	if (i == Num_of_OPP) {
		if (previous_step != -1) {
			tscpu_printk(
				"Free all static thermal limit, previous_opp=%d\n",
				     previous_step);
			previous_step = -1;

			mtktscpu_limited_dmips = /* highest dmips */
				tscpu_cpu_dmips[CPU_COOLER_NUM - 1];

			set_static_cpu_power_limit(0);
			set_static_gpu_power_limit(0);
		}
	}
	return 0;
}

static int dtm_cpu_get_max_state
(struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = 1;
	return 0;
}

static int dtm_cpu_get_cur_state
(struct thermal_cooling_device *cdev, unsigned long *state)
{
	int i = 0;

	for (i = 0; i < Num_of_OPP; i++) {
		if (!strcmp(cdev->type, &cooler_name[i * 20]))
			*state = cl_dev_state[i];
	}
	return 0;
}

static int dtm_cpu_set_cur_state
(struct thermal_cooling_device *cdev, unsigned long state)
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

#if defined(THERMAL_VPU_SUPPORT)
static ssize_t clvpu_opp_proc_write
(struct file *filp, const char __user *buf, size_t len, loff_t *data)
{
	int vpu_upper_opp = -1;
	unsigned int vpu_power = 0;
	char tmp[32] = {0};

	len = (len < (sizeof(tmp) - 1)) ? len : (sizeof(tmp) - 1);

	/* write data to the buffer */
	if (copy_from_user(tmp, buf, len))
		return -EFAULT;

	if (kstrtoint(tmp, 10, &vpu_upper_opp) == 0) {
		if (vpu_upper_opp == -1)
			vpu_power = 0;
#ifdef CONFIG_MTK_APUSYS_SUPPORT
		else if (vpu_upper_opp >= APU_OPP_0 &&
			vpu_upper_opp < APU_OPP_NUM)
#else
		else if (vpu_upper_opp >= VPU_OPP_0 &&
			vpu_upper_opp < VPU_OPP_NUM)
#endif
			vpu_power = vpu_power_table[vpu_upper_opp].power;
		else
			vpu_power = 0;

		set_static_vpu_power_limit(vpu_power);
		tscpu_printk("[%s] = %d\n", __func__, vpu_power);
		return len;
	}

	tscpu_dprintk("[%s] invalid input\n", __func__);

	return -EINVAL;
}

static int clvpu_opp_proc_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%d,%d\n", prv_stc_vpu_pwr_lim, static_vpu_power_limit);

	tscpu_dprintk("[%s] %d\n", __func__, static_vpu_power_limit);

	return 0;
}

static int clvpu_opp_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, clvpu_opp_proc_read, PDE_DATA(inode));
}

static const struct file_operations clvpu_opp_fops = {
	.owner = THIS_MODULE,
	.open = clvpu_opp_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = clvpu_opp_proc_write,
	.release = single_release,
};

static void thermal_vpu_init(void)
{
	struct proc_dir_entry *dir_entry = NULL;
	struct proc_dir_entry *entry = NULL;

	dir_entry = mtk_thermal_get_proc_drv_therm_dir_entry();
	if (!dir_entry)
		tscpu_printk("[%s]: mkdir /proc/driver/thermal failed\n",
		__func__);
	else {
		entry = proc_create("thermal_vpu_limit", 0664, dir_entry,
			&clvpu_opp_fops);
		if (entry)
			proc_set_user(entry, uid, gid);
	}
}
#endif

#if defined(THERMAL_MDLA_SUPPORT)
static ssize_t clmdla_opp_proc_write(
struct file *filp, const char __user *buf, size_t len, loff_t *data)
{
	int mdla_upper_opp = -1;
	unsigned int mdla_power = 0;
	char tmp[32] = {0};

	len = (len < (sizeof(tmp) - 1)) ? len : (sizeof(tmp) - 1);

	/* write data to the buffer */
	if (copy_from_user(tmp, buf, len))
		return -EFAULT;

	if (kstrtoint(tmp, 10, &mdla_upper_opp) == 0) {
		if (mdla_upper_opp == -1)
			mdla_power = 0;
#ifdef CONFIG_MTK_APUSYS_SUPPORT
		else if (mdla_upper_opp >= APU_OPP_0 &&
			mdla_upper_opp < APU_OPP_NUM)
#else
		else if (mdla_upper_opp >= MDLA_OPP_0 &&
			mdla_upper_opp < MDLA_OPP_NUM)
#endif
			mdla_power = mdla_power_table[mdla_upper_opp].power;
		else
			mdla_power = 0;

		set_static_mdla_power_limit(mdla_power);
		tscpu_printk("[%s] = %d\n", __func__, mdla_power);
		return len;
	}

	tscpu_dprintk("[%s] invalid input\n", __func__);

	return -EINVAL;
}

static int clmdla_opp_proc_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%d,%d\n", prv_stc_mdla_pwr_lim, static_mdla_power_limit);

	tscpu_dprintk("[%s] %d\n", __func__, static_mdla_power_limit);

	return 0;
}

static int clmdla_opp_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, clmdla_opp_proc_read, PDE_DATA(inode));
}

static const struct file_operations clmdla_opp_fops = {
	.owner = THIS_MODULE,
	.open = clmdla_opp_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = clmdla_opp_proc_write,
	.release = single_release,
};

static void thermal_mdla_init(void)
{
	struct proc_dir_entry *dir_entry = NULL;
	struct proc_dir_entry *entry = NULL;

	dir_entry = mtk_thermal_get_proc_drv_therm_dir_entry();
	if (!dir_entry)
		tscpu_printk("[%s]: mkdir /proc/driver/thermal failed\n",
				__func__);
	else {
		entry = proc_create("thermal_mdla_limit", 0664,
			dir_entry, &clmdla_opp_fops);
		if (entry)
			proc_set_user(entry, uid, gid);
	}
}
#endif

/* Init local structure for AP coolers */
static int init_cooler(void)
{
	int i, ret = -ENOMEM;
	int num = CPU_COOLER_NUM;	/* 700~4000, 92 */

	cl_dev_state = kzalloc((num) * sizeof(unsigned int), GFP_KERNEL);
	if (cl_dev_state == NULL)
		return -ENOMEM;

	cl_dev = kzalloc((num) * sizeof(struct thermal_cooling_device *),
								GFP_KERNEL);

	if (cl_dev == NULL)
		goto free_cl_dev_state;

	cooler_name = kzalloc((num) * sizeof(char) * 20, GFP_KERNEL);
	if (cooler_name == NULL)
		goto free_cl_dev;

	for (i = 0; i < num; i++) {
		/* using index=>0=700,1=800 ~ 33=4000 */
		ret = sprintf(cooler_name + (i * 20), "cpu%02d", i);
		if (ret != 5) {
			ret = -EIO;
			goto free_cooler_name;
		}
	}

	Num_of_OPP = num;	/* CPU COOLER COUNT, not CPU OPP count */
	return 0;

free_cooler_name:
	kfree(cooler_name);
free_cl_dev:
	kfree(cl_dev);
free_cl_dev_state:
	kfree(cl_dev_state);

	return ret;
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
		cl_dev[i] = mtk_thermal_cooling_device_register(
				&cooler_name[i * 20], NULL,
						&mtktscpu_cooling_F0x2_ops);
	}

#if defined(THERMAL_VPU_SUPPORT)
	thermal_vpu_init();
#endif

#if defined(THERMAL_MDLA_SUPPORT)
	thermal_mdla_init();
#endif
/*
 *	if (err) {
 *		tscpu_printk(
 *				"tscpu_register_DVFS_hotplug_cooler fail\n");
 *		return err;
 *	}
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
