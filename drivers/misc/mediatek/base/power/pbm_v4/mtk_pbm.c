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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kobject.h>
#include <linux/kthread.h>
#include <linux/atomic.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/sched/rt.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>

#include <mach/mtk_pbm.h>
#include <mtk_pbm_rel.h>

#ifndef DISABLE_PBM_FEATURE
#include <mach/upmu_sw.h>
#include <mt-plat/upmu_common.h>
#include <mt-plat/mtk_auxadc_intf.h>
#include <mtk_cpufreq_api.h>
#include <mtk_gpufreq.h>
#include <mach/mtk_thermal.h>
#include <mtk_ppm_api.h>
#endif

#ifndef DISABLE_PBM_FEATURE
static bool mt_pbm_debug;

static int mt_pbm_log_divisor;
static int mt_pbm_log_counter;

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "[PBM] " fmt

#define BIT_CHECK(a, b) ((a) & (1<<(b)))

static struct hpf hpf_ctrl = {
	.switch_md1 = 1,
	.switch_gpu = 0,
	.switch_flash = 0,

	.cpu_volt = 1000,	/* 1V = boot up voltage */
	.gpu_volt = 0,
	.cpu_num = 1,		/* default cpu0 core */

	.loading_leakage = 0,
	.loading_dlpt = 0,
	.loading_md1 = 0,
	.loading_cpu = 0,
	.loading_gpu = 0,
	.loading_flash = MAX_FLASH_POWER,	/* fixed */
};

static struct pbm pbm_ctrl = {
	/* feature key */
	.feature_en = 1,
	.pbm_drv_done = 0,
	.hpf_en = 63,/* bin: 111111 (Flash, GPU, CPU, MD3, MD1, DLPT) */
};


int g_dlpt_need_do = 1;
static DEFINE_MUTEX(pbm_mutex);
static DEFINE_MUTEX(pbm_table_lock);
static struct task_struct *pbm_thread;
static atomic_t kthread_nreq = ATOMIC_INIT(0);
/* extern u32 get_devinfo_with_index(u32 index); */

/*
 * weak function
 */
int __attribute__ ((weak))
tscpu_get_min_cpu_pwr(void)
{
	pr_warn_ratelimited("%s not ready\n", __func__);
	return 0;
}

unsigned int __attribute__ ((weak))
mt_gpufreq_get_leakage_mw(void)
{
	pr_warn_ratelimited("%s not ready\n", __func__);
	return 0;
}

void __attribute__ ((weak))
mt_gpufreq_set_power_limit_by_pbm(unsigned int limited_power)
{
	pr_warn_ratelimited("%s not ready\n", __func__);
}

u32 __attribute__ ((weak))
spm_vcorefs_get_MD_status(void)
{
	pr_warn_ratelimited("%s not ready\n", __func__);
	return 0;
}

int get_battery_volt(void)
{
	return pmic_get_auxadc_value(AUXADC_LIST_BATADC);
	/* return 3900; */
}

unsigned int ma_to_mw(unsigned int val)
{
	unsigned int bat_vol = 0;
	unsigned int ret_val = 0;

	bat_vol = get_battery_volt();	/* return mV */
	ret_val = (bat_vol * val) / 1000;	/* mW = (mV * mA)/1000 */
	pr_info("[%s] %d(mV) * %d(mA) = %d(mW)\n",
		__func__, bat_vol, val, ret_val);

	return ret_val;
}

void dump_kicker_info(void)
{
	struct hpf *hpfmgr = &hpf_ctrl;

	if (mt_pbm_debug)
		pr_info("(M1/F/G)=%d,%d,%d;(C/G)=%ld,%ld\n",
			hpfmgr->switch_md1, hpfmgr->switch_flash,
			hpfmgr->switch_gpu, hpfmgr->loading_cpu,
			hpfmgr->loading_gpu);
}

int hpf_get_power_leakage(void)
{
	struct hpf *hpfmgr = &hpf_ctrl;
	unsigned int leakage_cpu = 0, leakage_gpu = 0;

	leakage_cpu = mt_ppm_get_leakage_mw(TOTAL_CLUSTER_LKG);
	leakage_gpu = mt_gpufreq_get_leakage_mw();
	hpfmgr->loading_leakage = leakage_cpu + leakage_gpu;

	if (mt_pbm_debug)
		pr_info("[%s] %ld=%d+%d\n", __func__,
			hpfmgr->loading_leakage, leakage_cpu, leakage_gpu);

	return hpfmgr->loading_leakage;
}

unsigned long hpf_get_power_cpu(void)
{
	struct hpf *hpfmgr = &hpf_ctrl;

	return hpfmgr->loading_cpu;
}

unsigned long hpf_get_power_gpu(void)
{
	struct hpf *hpfmgr = &hpf_ctrl;

	if (hpfmgr->switch_gpu)
		return hpfmgr->loading_gpu;
	else
		return 0;
}

unsigned long hpf_get_power_flash(void)
{
	struct hpf *hpfmgr = &hpf_ctrl;

	if (hpfmgr->switch_flash)
		return hpfmgr->loading_flash;
	else
		return 0;
}

unsigned long hpf_get_power_dlpt(void)
{
	struct hpf *hpfmgr = &hpf_ctrl;

	return hpfmgr->loading_dlpt;
}

unsigned long hpf_get_power_md1(void)
{
	struct hpf *hpfmgr = &hpf_ctrl;

	if (hpfmgr->switch_md1)
		hpfmgr->loading_md1 = get_md1_power(MAX_POWER);
	else
		hpfmgr->loading_md1 = 0;

	return hpfmgr->loading_md1;
}

static void pbm_allocate_budget_manager(void)
{
	int _dlpt = 0, leakage = 0, md1 = 0, dlpt = 0;
	int cpu = 0, gpu = 0, flash = 0, tocpu = 0, togpu = 0;
	int multiple = 0;
	int cpu_lower_bound = tscpu_get_min_cpu_pwr();
	static int pre_tocpu, pre_togpu;

	mutex_lock(&pbm_table_lock);
	/* dump_kicker_info(); */
	leakage = hpf_get_power_leakage();
	md1 = hpf_get_power_md1();
	dlpt = hpf_get_power_dlpt();
	cpu = hpf_get_power_cpu();
	gpu = hpf_get_power_gpu();
	flash = hpf_get_power_flash();

	if (mt_pbm_log_divisor) {
		mt_pbm_log_counter = (mt_pbm_log_counter + 1) %
			mt_pbm_log_divisor;

		if (mt_pbm_log_counter == 1)
			mt_pbm_debug = 1;
		else
			mt_pbm_debug = 0;
	}

	mutex_unlock(&pbm_table_lock);

	/* no any resource can allocate */
	if (dlpt == 0) {
		if (mt_pbm_debug)
			pr_info("DLPT=0\n");

		return;
	}

	_dlpt = dlpt - (leakage + md1 + flash);
	if (_dlpt < 0)
		_dlpt = 0;

	/* if gpu no need resource, so all allocate to cpu */
	if (gpu == 0) {
		tocpu = _dlpt;

		/* check CPU lower bound */
		if (tocpu < cpu_lower_bound)
			tocpu = cpu_lower_bound;

		if (tocpu <= 0)
			tocpu = 1;

		mt_ppm_dlpt_set_limit_by_pbm(tocpu);
	} else {
		multiple = (_dlpt * 1000) / (cpu + gpu);

		if (multiple > 0) {
			tocpu = (multiple * cpu) / 1000;
			togpu = (multiple * gpu) / 1000;
		} else {
			tocpu = 1;
			togpu = 1;
		}

		/* check CPU lower bound */
		if (tocpu < cpu_lower_bound) {
			tocpu = cpu_lower_bound;
			togpu = _dlpt - cpu_lower_bound;
		}

		if (tocpu <= 0)
			tocpu = 1;
		if (togpu <= 0)
			togpu = 1;

		mt_ppm_dlpt_set_limit_by_pbm(tocpu);
		mt_gpufreq_set_power_limit_by_pbm(togpu);
	}

	if (mt_pbm_debug) {
		pr_info
("(C/G)=%d,%d=>(D/L/M1/F/C/G)=%d,%d,%d,%d,%d,%d(Multi:%d),%d\n",
cpu, gpu, dlpt, leakage, md1, flash, tocpu, togpu,
multiple, cpu_lower_bound);
	} else {
		if (((abs(pre_tocpu - tocpu) >= 10) && cpu > tocpu) ||
			((abs(pre_togpu - togpu) >= 10) && gpu > togpu)) {
			pr_info
("(C/G)=%d,%d=>(D/L/M1/F/C/G)=%d,%d,%d,%d,%d,%d(Multi:%d),%d\n",
cpu, gpu, dlpt, leakage, md1, flash, tocpu, togpu,
multiple, cpu_lower_bound);
			pre_tocpu = tocpu;
			pre_togpu = togpu;
		} else if ((cpu > tocpu) || (gpu > togpu)) {
			pr_warn_ratelimited
("(C/G)=%d,%d => (D/L/M1/F/C/G)=%d,%d,%d,%d,%d,%d (Multi:%d),%d\n",
cpu, gpu, dlpt, leakage, md1, flash, tocpu, togpu, multiple, cpu_lower_bound);
		} else {
			pre_tocpu = tocpu;
			pre_togpu = togpu;
		}
	}
}

static bool pbm_func_enable_check(void)
{
	struct pbm *pwrctrl = &pbm_ctrl;

	if (!pwrctrl->feature_en || !pwrctrl->pbm_drv_done) {
		pr_info("feature_en: %d, pbm_drv_done: %d\n",
		pwrctrl->feature_en, pwrctrl->pbm_drv_done);
		return false;
	}

	return true;
}

static bool pbm_update_table_info(enum pbm_kicker kicker, struct mrp *mrpmgr)
{
	struct hpf *hpfmgr = &hpf_ctrl;
	bool is_update = false;

	switch (kicker) {
	case KR_DLPT:		/* kicker 0 */
		if (hpfmgr->loading_dlpt != mrpmgr->loading_dlpt) {
			hpfmgr->loading_dlpt = mrpmgr->loading_dlpt;
			is_update = true;
		}
		break;
	case KR_MD1:		/* kicker 1 */
		if (hpfmgr->switch_md1 != mrpmgr->switch_md) {
			hpfmgr->switch_md1 = mrpmgr->switch_md;
			is_update = true;
		}
		break;
	case KR_MD3:		/* kicker 2 */
		pr_warn("should not kicker KR_MD3\n");
		break;
	case KR_CPU:		/* kicker 3 */
		hpfmgr->cpu_volt = mrpmgr->cpu_volt;
		if (hpfmgr->loading_cpu != mrpmgr->loading_cpu
		    || hpfmgr->cpu_num != mrpmgr->cpu_num) {
			hpfmgr->loading_cpu = mrpmgr->loading_cpu;
			hpfmgr->cpu_num = mrpmgr->cpu_num;
			is_update = true;
		}
		break;
	case KR_GPU:		/* kicker 4 */
		hpfmgr->gpu_volt = mrpmgr->gpu_volt;
		if (hpfmgr->switch_gpu != mrpmgr->switch_gpu
		    || hpfmgr->loading_gpu != mrpmgr->loading_gpu) {
			hpfmgr->switch_gpu = mrpmgr->switch_gpu;
			hpfmgr->loading_gpu = mrpmgr->loading_gpu;
			is_update = true;
		}
		break;
	case KR_FLASH:		/* kicker 5 */
		if (hpfmgr->switch_flash != mrpmgr->switch_flash) {
			hpfmgr->switch_flash = mrpmgr->switch_flash;
			is_update = true;
		}
		break;
	default:
		pr_warn("[%s] ERROR, unknown kicker [%d]\n", __func__, kicker);
		WARN_ON_ONCE(1);
		break;
	}

	return is_update;
}

static void pbm_wake_up_thread(enum pbm_kicker kicker, struct mrp *mrpmgr)
{
	if (atomic_read(&kthread_nreq) <= 0) {
		atomic_inc(&kthread_nreq);
		wake_up_process(pbm_thread);
	}

	while (kicker == KR_FLASH && mrpmgr->switch_flash == 1) {
		if (atomic_read(&kthread_nreq) == 0)
			return;
	}
}

static void mtk_power_budget_manager(enum pbm_kicker kicker, struct mrp *mrpmgr)
{
	bool pbm_enable = false;
	bool pbm_update = false;

	mutex_lock(&pbm_table_lock);
	pbm_update = pbm_update_table_info(kicker, mrpmgr);
	mutex_unlock(&pbm_table_lock);
	if (!pbm_update)
		return;

	pbm_enable = pbm_func_enable_check();
	if (!pbm_enable)
		return;

	pbm_wake_up_thread(kicker, mrpmgr);
}

/*
 * kicker: 0
 * who call : PMIC
 * i_max: mA
 * condition: persentage decrease 1%, then update i_max
 */
void kicker_pbm_by_dlpt(unsigned int i_max)
{
	struct pbm *pwrctrl = &pbm_ctrl;
	struct mrp mrpmgr;

	mrpmgr.loading_dlpt = ma_to_mw(i_max);

	if (BIT_CHECK(pwrctrl->hpf_en, KR_DLPT))
		mtk_power_budget_manager(KR_DLPT, &mrpmgr);
}

/*
 * kicker: 1, 2
 * who call : MD1
 * condition: on/off
 */
void kicker_pbm_by_md(enum pbm_kicker kicker, bool status)
{
	struct pbm *pwrctrl = &pbm_ctrl;
	struct mrp mrpmgr;

	mrpmgr.switch_md = status;

	if (BIT_CHECK(pwrctrl->hpf_en, kicker))
		mtk_power_budget_manager(kicker, &mrpmgr);
}

/*
 * kicker: 3
 * who call : CPU
 * loading: mW
 * condition: opp changed
 */
void kicker_pbm_by_cpu(unsigned int loading, int core, int voltage)
{
	struct pbm *pwrctrl = &pbm_ctrl;
	struct mrp mrpmgr;

	mrpmgr.loading_cpu = loading;
	mrpmgr.cpu_num = core;
	mrpmgr.cpu_volt = voltage;

	if (BIT_CHECK(pwrctrl->hpf_en, KR_CPU))
		mtk_power_budget_manager(KR_CPU, &mrpmgr);
}

/*
 * kicker: 4
 * who call : GPU
 * loading: mW
 * condition: opp changed
 */
void kicker_pbm_by_gpu(bool status, unsigned int loading, int voltage)
{
	struct pbm *pwrctrl = &pbm_ctrl;
	struct mrp mrpmgr;

	mrpmgr.switch_gpu = status;
	mrpmgr.loading_gpu = loading;
	mrpmgr.gpu_volt = voltage;

	if (BIT_CHECK(pwrctrl->hpf_en, KR_GPU))
		mtk_power_budget_manager(KR_GPU, &mrpmgr);
}

/*
 * kicker: 5
 * who call : Flash
 * condition: on/off
 */
void kicker_pbm_by_flash(bool status)
{
	struct pbm *pwrctrl = &pbm_ctrl;
	struct mrp mrpmgr;

	mrpmgr.switch_flash = status;

	if (BIT_CHECK(pwrctrl->hpf_en, KR_FLASH))
		mtk_power_budget_manager(KR_FLASH, &mrpmgr);
}

static int pbm_thread_handle(void *data)
{
	int g_dlpt_state_sync = 0;

	while (1) {

		set_current_state(TASK_INTERRUPTIBLE);

		if (kthread_should_stop())
			break;

		if (atomic_read(&kthread_nreq) <= 0) {
			schedule();
			continue;
		}

		mutex_lock(&pbm_mutex);
		if (g_dlpt_need_do == 1) {
			if (g_dlpt_stop == 0) {
				pbm_allocate_budget_manager();
				g_dlpt_state_sync = 0;
			} else {
				pr_notice("DISABLE PBM\n");

				if (g_dlpt_state_sync == 0) {
					mt_ppm_dlpt_set_limit_by_pbm(0);
					mt_gpufreq_set_power_limit_by_pbm(0);
					g_dlpt_state_sync = 1;
					pr_info("Release DLPT limit\n");
				}
			}
		}
		atomic_dec(&kthread_nreq);
		mutex_unlock(&pbm_mutex);
	}

	__set_current_state(TASK_RUNNING);

	return 0;
}

static int create_pbm_kthread(void)
{
	struct pbm *pwrctrl = &pbm_ctrl;

	pbm_thread = kthread_create(pbm_thread_handle, (void *)NULL, "pbm");
	if (IS_ERR(pbm_thread))
		return PTR_ERR(pbm_thread);

	wake_up_process(pbm_thread);
	pwrctrl->pbm_drv_done = 1;
	/* avoid other hpf call thread before thread init done */

	return 0;
}

static int
_mt_pbm_pm_callback(struct notifier_block *nb,
		unsigned long action, void *ptr)
{
	switch (action) {

	case PM_SUSPEND_PREPARE:
		if (mt_pbm_debug)
			pr_info("PM_SUSPEND_PREPARE:start\n");

		mutex_lock(&pbm_mutex);
		g_dlpt_need_do = 0;
		mutex_unlock(&pbm_mutex);
		if (mt_pbm_debug)
			pr_info("PM_SUSPEND_PREPARE:end\n");

		break;

	case PM_HIBERNATION_PREPARE:
		break;

	case PM_POST_SUSPEND:
		if (mt_pbm_debug)
			pr_info("PM_POST_SUSPEND:start\n");

		mutex_lock(&pbm_mutex);
		g_dlpt_need_do = 1;
		mutex_unlock(&pbm_mutex);
		if (mt_pbm_debug)
			pr_info("PM_POST_SUSPEND:end\n");

		break;

	case PM_POST_HIBERNATION:
		break;

	default:
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

#if 1 /* CONFIG_PBM_PROC_FS */
/*
 * show current debug status
 */
static int mt_pbm_debug_proc_show(struct seq_file *m, void *v)
{
	if (mt_pbm_debug)
		seq_puts(m, "pbm debug enabled\n");
	else
		seq_puts(m, "pbm debug disabled\n");

	return 0;
}

/*
 * enable debug message
 */
static ssize_t mt_pbm_debug_proc_write
(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;
	int debug = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	/* if (sscanf(desc, "%d", &debug) == 1) { */
	if (kstrtoint(desc, 10, &debug) == 0) {
		if (debug == 0)
			mt_pbm_debug = 0;
		else if (debug == 1)
			mt_pbm_debug = 1;
		else
			pr_notice("should be [0:disable,1:enable]\n");
	} else
		pr_notice("should be [0:disable,1:enable]\n");

	return count;
}

static int mt_pbm_debug_log_reduc_proc_show(struct seq_file *m, void *v)
{
	if (mt_pbm_log_divisor) {
		seq_puts(m, "pbm debug enabled\n");
		seq_printf(m, "The divisor number is :%d\n",
			mt_pbm_log_divisor);
	} else {
		seq_puts(m, "Log reduction disabled\n");
	}

	return 0;
}

static ssize_t mt_pbm_debug_log_reduc_proc_write
(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;
	int debug = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	/* if (sscanf(desc, "%d", &debug) == 1) { */
	if (kstrtoint(desc, 10, &debug) == 0) {
		if (debug == 0) {
			mt_pbm_log_divisor = 0;
			mt_pbm_debug = 0;
		} else if (debug > 0) {
			mt_pbm_log_divisor = debug;
			mt_pbm_debug = 1;
			mt_pbm_log_counter = 0;
		} else {
			pr_notice("Should be >=0 [0:disable,other:enable]\n");
		}
	} else
		pr_notice("Should be >=0 [0:disable,other:enable]\n");

	return count;
}

#define PROC_FOPS_RW(name)						\
static int mt_ ## name ## _proc_open(struct inode *inode, struct file *file)\
{									\
	return single_open(file, mt_ ## name ## _proc_show, PDE_DATA(inode));\
}									\
static const struct file_operations mt_ ## name ## _proc_fops = {	\
	.owner		= THIS_MODULE,					\
	.open		= mt_ ## name ## _proc_open,			\
	.read		= seq_read,					\
	.llseek		= seq_lseek,					\
	.release	= single_release,				\
	.write		= mt_ ## name ## _proc_write,			\
}

#define PROC_FOPS_RO(name)						\
static int mt_ ## name ## _proc_open(struct inode *inode, struct file *file)\
{									\
	return single_open(file, mt_ ## name ## _proc_show, PDE_DATA(inode));\
}									\
static const struct file_operations mt_ ## name ## _proc_fops = {	\
	.owner		= THIS_MODULE,				\
	.open		= mt_ ## name ## _proc_open,		\
	.read		= seq_read,				\
	.llseek		= seq_lseek,				\
	.release	= single_release,			\
}

#define PROC_ENTRY(name)	{__stringify(name), &mt_ ## name ## _proc_fops}

PROC_FOPS_RW(pbm_debug);

PROC_FOPS_RW(pbm_debug_log_reduc);

static int mt_pbm_create_procfs(void)
{
	struct proc_dir_entry *dir = NULL;
	int i;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(pbm_debug),
		PROC_ENTRY(pbm_debug_log_reduc),
	};

	dir = proc_mkdir("pbm", NULL);

	if (!dir) {
		pr_err("fail to create /proc/pbm @ %s()\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create
		    (entries[i].name, 0664, dir, entries[i].fops))
			pr_err("@%s: create /proc/pbm/%s failed\n", __func__,
				    entries[i].name);
	}

	return 0;
}
#endif	/* CONFIG_PBM_PROC_FS */

static int __init pbm_module_init(void)
{
	int ret = 0;

	#if 1 /* CONFIG_PBM_PROC_FS */
	mt_pbm_create_procfs();
	#endif

	pm_notifier(_mt_pbm_pm_callback, 0);

	register_dlpt_notify(&kicker_pbm_by_dlpt, DLPT_PRIO_PBM);
	ret = create_pbm_kthread();

	#ifdef MD_POWER_UT
	/* pr_info("share_reg: %x", spm_vcorefs_get_MD_status());*/
	mt_pbm_debug = 1;
	md_power_meter_ut();
	#endif

	pr_info("pbm_module_init : Done\n");

	if (ret) {
		pr_err("FAILED TO CREATE PBM KTHREAD\n");
		return ret;
	}
	return ret;
}

#else				/* #ifndef DISABLE_PBM_FEATURE */

void kicker_pbm_by_dlpt(unsigned int i_max)
{
}

void kicker_pbm_by_md(enum pbm_kicker kicker, bool status)
{
}

void kicker_pbm_by_cpu(unsigned int loading, int core, int voltage)
{
}

void kicker_pbm_by_gpu(bool status, unsigned int loading, int voltage)
{
}

void kicker_pbm_by_flash(bool status)
{
}

static int __init pbm_module_init(void)
{
	pr_notice("DISABLE_PBM_FEATURE is defined.\n");
	return 0;
}

#endif				/* #ifndef DISABLE_PBM_FEATURE */

static void __exit pbm_module_exit(void)
{

}

module_init(pbm_module_init);
module_exit(pbm_module_exit);

MODULE_DESCRIPTION("PBM Driver v0.1");
