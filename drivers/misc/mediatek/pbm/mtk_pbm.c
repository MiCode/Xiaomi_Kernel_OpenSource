// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/atomic.h>
#include <linux/cpufreq.h>
#include <linux/energy_model.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/pm_qos.h>
#include <linux/power_supply.h>
#include <linux/spinlock.h>
#include <linux/suspend.h>
#include <trace/events/power.h>
#include <linux/tracepoint.h>
#include <linux/kallsyms.h>

#include "mtk_dynamic_loading_throttling.h"
#include "mtk_pbm.h"
#include "mtk_pbm_common.h"
#if IS_ENABLED(CONFIG_MTK_MDPM)
#include "mtk_mdpm.h"
#endif
#include "mtk_gpufreq.h"

#define DEFAULT_PBM_WEIGHT 1024
#define MAX_FLASH_POWER 3500
#define MAX_MD1_POWER 4000
#define MIN_CPU_POWER 600

static LIST_HEAD(pbm_policy_list);

static bool mt_pbm_debug;
static int pbm_drv_done;

#define DLPTCB_MAX_NUM 16
static struct pbm_callback_table pbmcb_tb[DLPTCB_MAX_NUM] = { {0} };

static struct hpf hpf_ctrl = {
	.switch_md1 = 1,
	.switch_gpu = 0,
	.switch_flash = 0,

	.cpu_volt = 1000,
	.gpu_volt = 0,
	.cpu_num = 1,

	.loading_dlpt = 0,
	.loading_md1 = 0,
	.loading_cpu = 0,
	.loading_gpu = 0,
	.loading_flash = MAX_FLASH_POWER,
	.to_cpu_budget = 0,
};

static int g_dlpt_need_do = 1;
static DEFINE_MUTEX(pbm_mutex);
static struct task_struct *pbm_thread;
static atomic_t kthread_nreq = ATOMIC_INIT(0);

static unsigned int ma_to_mw(unsigned int bat_cur)
{
	union power_supply_propval prop;
	struct power_supply *psy;
	int ret;
	unsigned int bat_vol, ret_val;

	psy = power_supply_get_by_name("battery");
	if (psy == NULL) {
		pr_notice("%s can't get battery node\n", __func__);
		return 0;
	}

	ret = power_supply_get_property(psy,
		POWER_SUPPLY_PROP_VOLTAGE_NOW, &prop);
	if (ret || prop.intval < 0) {
		pr_info("%s: POWER_SUPPLY_PROP_VOLTAGE_NOW fail\n", __func__);
		return 0;
	}

	bat_vol = prop.intval / 1000;
	ret_val = (bat_vol * bat_cur) / 1000;
	pr_info("[%s] %d(mV) * %d(mA) = %d(mW)\n",
		__func__, bat_vol, bat_cur, ret_val);

	return ret_val;
}

static unsigned long hpf_get_power_cpu(void)
{
	struct hpf *hpfmgr = &hpf_ctrl;

	return hpfmgr->loading_cpu;
}

static unsigned long hpf_get_power_gpu(void)
{
	struct hpf *hpfmgr = &hpf_ctrl;

	if (hpfmgr->switch_gpu)
		return hpfmgr->loading_gpu;
	else
		return 0;
}

static unsigned long hpf_get_power_flash(void)
{
	struct hpf *hpfmgr = &hpf_ctrl;

	if (hpfmgr->switch_flash)
		return hpfmgr->loading_flash;
	else
		return 0;
}

static unsigned long hpf_get_power_dlpt(void)
{
	struct hpf *hpfmgr = &hpf_ctrl;

	return hpfmgr->loading_dlpt;
}

static unsigned long hpf_get_power_md1(void)
{
	struct hpf *hpfmgr = &hpf_ctrl;

	if (hpfmgr->switch_md1)
#if IS_ENABLED(CONFIG_MTK_MDPM)
		hpfmgr->loading_md1 = get_md1_power(MAX_POWER, true);
#else
		hpfmgr->loading_md1 = MAX_MD1_POWER;
#endif
	else
		hpfmgr->loading_md1 = 0;

	return hpfmgr->loading_md1;
}

static u32 cpu_power_to_freq(struct cpu_pbm_policy *pbm_policy, u32 power)
{
	int i;

	for (i = pbm_policy->max_cap_state - 1; i > 0; i--) {
		if (power >= pbm_policy->em->table[i].power)
			break;
	}

	return pbm_policy->em->table[i].frequency;
}

static u32 cpu_freq_to_power(struct cpu_pbm_policy *pbm_policy, u32 freq)
{
	int i;

	for (i = pbm_policy->max_cap_state - 1; i > 0; i--) {
		if (freq >= pbm_policy->em->table[i].frequency)
			break;
	}

	return pbm_policy->em->table[i].power;
}

static void mtk_cpu_dlpt_set_limit_by_pbm(unsigned int limit_power)
{
	struct cpu_pbm_policy *pbm_policy;
	unsigned int total_power_weight, granted_power, num_cpus;
	u32 frequency;

	total_power_weight = 0;
	list_for_each_entry(pbm_policy, &pbm_policy_list, cpu_pbm_list)
		total_power_weight += pbm_policy->power_weight;

	list_for_each_entry(pbm_policy, &pbm_policy_list, cpu_pbm_list) {
		num_cpus = cpumask_weight(pbm_policy->policy->cpus);

		if (num_cpus == 0 || total_power_weight == 0) {
			pr_info("warning: num_cpus=%d total_power_weight=%d\n", num_cpus,
				total_power_weight);
			continue;
		}

		granted_power = limit_power * pbm_policy->power_weight / total_power_weight
			/ num_cpus;
		frequency = cpu_power_to_freq(pbm_policy, granted_power);
		freq_qos_update_request(&pbm_policy->qos_req, frequency);
	}
}

static void mtk_gpufreq_set_power_limit_by_pbm(unsigned int limit_power)
{
	if (pbmcb_tb[PBM_PRIO_GPU].pbmcb != NULL)
		pbmcb_tb[PBM_PRIO_GPU].pbmcb(limit_power);
}

static void pbm_allocate_budget_manager(void)
{
	int _dlpt, md1, dlpt, cpu, gpu, flash, tocpu, togpu;
	int multiple;
	int cpu_lower_bound = MIN_CPU_POWER;
	static int pre_tocpu, pre_togpu;

	md1 = hpf_get_power_md1();
	dlpt = hpf_get_power_dlpt();
	cpu = hpf_get_power_cpu();
	gpu = hpf_get_power_gpu();
	flash = hpf_get_power_flash();

	if (dlpt == 0) {
		if (mt_pbm_debug)
			pr_info("DLPT=0\n");

		return;
	}

	_dlpt = dlpt - (md1 + flash);
	if (_dlpt < 0)
		_dlpt = 0;

	if (gpu == 0) {
		tocpu = _dlpt;
		togpu = 0;

		/* check CPU lower bound */
		if (tocpu < cpu_lower_bound)
			tocpu = cpu_lower_bound;

		if (tocpu <= 0)
			tocpu = 1;

		mtk_cpu_dlpt_set_limit_by_pbm(tocpu);
	} else {
		multiple = (_dlpt * 1000) / (cpu + gpu);

		if (multiple > 0) {
			tocpu = (multiple * cpu) / 1000;
			togpu = (multiple * gpu) / 1000;
		} else {
			tocpu = 1;
			togpu = 1;
		}

		if (tocpu < cpu_lower_bound) {
			tocpu = cpu_lower_bound;
			togpu = _dlpt - cpu_lower_bound;
		}

		if (tocpu <= 0)
			tocpu = 1;
		if (togpu <= 0)
			togpu = 1;

		mtk_cpu_dlpt_set_limit_by_pbm(tocpu);
		mtk_gpufreq_set_power_limit_by_pbm(togpu);
	}

	hpf_ctrl.to_cpu_budget = tocpu;
	hpf_ctrl.to_gpu_budget = togpu;
	mtk_gpufreq_set_power_limit_by_pbm(togpu);

	if (mt_pbm_debug) {
		pr_info("(C/G)=%d,%d=>(D/M1/F/C/G)=%d,%d,%d,%d,%d(Multi:%d),%d\n",
			cpu, gpu, dlpt, md1, flash, tocpu, togpu,
			multiple, cpu_lower_bound);
	} else {
		if (((abs(pre_tocpu - tocpu) >= 10) && cpu > tocpu) ||
			((abs(pre_togpu - togpu) >= 10) && gpu > togpu)) {
			pr_info("(C/G)=%d,%d=>(D/M1/F/C/G)=%d,%d,%d,%d,%d(Multi:%d),%d\n",
				cpu, gpu, dlpt, md1, flash, tocpu, togpu,
				multiple, cpu_lower_bound);
			pre_tocpu = tocpu;
			pre_togpu = togpu;
		} else if ((cpu > tocpu) || (gpu > togpu)) {
			pr_info_ratelimited("(C/G)=%d,%d => (D/M1/F/C/G)=%d,%d,%d,%d,%d (Multi:%d),%d\n",
				cpu, gpu, dlpt, md1, flash, tocpu, togpu, multiple,
				cpu_lower_bound);
		} else {
			pre_tocpu = tocpu;
			pre_togpu = togpu;
		}
	}
}

static bool pbm_func_enable_check(void)
{
	if (!pbm_drv_done) {
		pr_info("pbm_drv_done: %d\n", pbm_drv_done);
		return false;
	}

	return true;
}

static bool pbm_update_table_info(enum pbm_kicker kicker, struct mrp *mrpmgr)
{
	struct hpf *hpfmgr = &hpf_ctrl;
	bool is_update = true;

	switch (kicker) {
	case KR_DLPT:
		if (hpfmgr->loading_dlpt != mrpmgr->loading_dlpt) {
			hpfmgr->loading_dlpt = mrpmgr->loading_dlpt;
			is_update = true;
		}
		break;
	case KR_MD1:
		if (hpfmgr->switch_md1 != mrpmgr->switch_md) {
			hpfmgr->switch_md1 = mrpmgr->switch_md;
			is_update = true;
		}
		break;
	case KR_MD3:
		pr_info("should not kicker KR_MD3\n");
		break;
	case KR_CPU:
		hpfmgr->cpu_volt = mrpmgr->cpu_volt;
		if (hpfmgr->loading_cpu != mrpmgr->loading_cpu
		    || hpfmgr->cpu_num != mrpmgr->cpu_num) {
			hpfmgr->loading_cpu = mrpmgr->loading_cpu;
			hpfmgr->cpu_num = mrpmgr->cpu_num;
			is_update = true;
		}
		break;
	case KR_GPU:
		hpfmgr->gpu_volt = mrpmgr->gpu_volt;
		if (hpfmgr->switch_gpu != mrpmgr->switch_gpu
		    || hpfmgr->loading_gpu != mrpmgr->loading_gpu) {
			hpfmgr->switch_gpu = mrpmgr->switch_gpu;
			hpfmgr->loading_gpu = mrpmgr->loading_gpu;
			is_update = true;
		}
		break;
	case KR_FLASH:
		if (hpfmgr->switch_flash != mrpmgr->switch_flash) {
			hpfmgr->switch_flash = mrpmgr->switch_flash;
			is_update = true;
		}
		break;
	default:
		pr_info("[%s] ERROR, unknown kicker [%d]\n", __func__, kicker);
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

	pbm_update = pbm_update_table_info(kicker, mrpmgr);
	if (!pbm_update)
		return;

	pbm_enable = pbm_func_enable_check();
	if (!pbm_drv_done)
		return;

	if (kicker != KR_CPU)
		pbm_wake_up_thread(kicker, mrpmgr);
}

void kicker_pbm_by_dlpt(int i_max)
{
	struct mrp mrpmgr = {0};

	mrpmgr.loading_dlpt = ma_to_mw(i_max);
	mtk_power_budget_manager(KR_DLPT, &mrpmgr);
}

void kicker_pbm_by_md(enum pbm_kicker kicker, bool status)
{
	struct mrp mrpmgr = {0};

	mrpmgr.switch_md = status;
	mtk_power_budget_manager(kicker, &mrpmgr);
}
EXPORT_SYMBOL(kicker_pbm_by_md);

void kicker_pbm_by_cpu(unsigned int loading, int core, int voltage)
{
	struct mrp mrpmgr = {0};

	mrpmgr.loading_cpu = loading;
	mrpmgr.cpu_num = core;
	mrpmgr.cpu_volt = voltage;

	mtk_power_budget_manager(KR_CPU, &mrpmgr);
}

void kicker_pbm_by_gpu(bool status, unsigned int loading, int voltage)
{
	struct mrp mrpmgr = {0};

	mrpmgr.switch_gpu = status;
	mrpmgr.loading_gpu = loading;
	mrpmgr.gpu_volt = voltage;

	mtk_power_budget_manager(KR_GPU, &mrpmgr);
}
EXPORT_SYMBOL(kicker_pbm_by_gpu);

void kicker_pbm_by_flash(bool status)
{
	struct mrp mrpmgr = {0};

	mrpmgr.switch_flash = status;

	mtk_power_budget_manager(KR_FLASH, &mrpmgr);
}
EXPORT_SYMBOL(kicker_pbm_by_flash);

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
		set_current_state(TASK_RUNNING);
		mutex_lock(&pbm_mutex);
		if (g_dlpt_need_do == 1) {
			pbm_allocate_budget_manager();
			g_dlpt_state_sync = 0;
		}
		atomic_dec(&kthread_nreq);
		mutex_unlock(&pbm_mutex);
	}

	return 0;
}

static int create_pbm_kthread(void)
{
	pbm_thread = kthread_create(pbm_thread_handle, (void *)NULL, "pbm");
	if (IS_ERR(pbm_thread))
		return PTR_ERR(pbm_thread);

	wake_up_process(pbm_thread);
	pbm_drv_done = 1;

	return 0;
}

static int _mt_pbm_pm_callback(struct notifier_block *nb, unsigned long action, void *ptr)
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

struct tracepoints_table {
	const char *name;
	void *func;
	struct tracepoint *tp;
	bool registered;
};

static void pbm_cpu_frequency_tracer(void *ignore, unsigned int frequency, unsigned int cpu_id)
{
	struct cpufreq_policy *policy = NULL;
	struct cpu_pbm_policy *pbm_policy;
	unsigned int cpu, num_cpus, freq, req_total_power = 0;

	policy = cpufreq_cpu_get(cpu_id);
	if (!policy)
		return;
	if (cpu_id != cpumask_first(policy->related_cpus))
		return;

	list_for_each_entry(pbm_policy, &pbm_policy_list, cpu_pbm_list) {
		cpu = pbm_policy->policy->cpu;

		if (cpu == cpu_id)
			freq = frequency;
		else
			freq = cpufreq_quick_get(cpu);

		num_cpus = cpumask_weight(pbm_policy->policy->cpus);
		req_total_power += cpu_freq_to_power(pbm_policy, freq) * num_cpus;

	}

	kicker_pbm_by_cpu(req_total_power, 0, 0);
}

struct tracepoints_table pbm_tracepoints[] = {
	{.name = "cpu_frequency", .func = pbm_cpu_frequency_tracer},
};

#define FOR_EACH_INTEREST(i) \
	for (i = 0; i < sizeof(pbm_tracepoints) / sizeof(struct tracepoints_table); i++)

static void lookup_tracepoints(struct tracepoint *tp, void *ignore)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (strcmp(pbm_tracepoints[i].name, tp->name) == 0)
			pbm_tracepoints[i].tp = tp;
	}
}

void tracepoint_cleanup(void)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (pbm_tracepoints[i].registered) {
			tracepoint_probe_unregister(
				pbm_tracepoints[i].tp,
				pbm_tracepoints[i].func, NULL);
			pbm_tracepoints[i].registered = false;
		}
	}
}

void register_pbm_notify(void *oc_cb, enum PBM_PRIO_TAG prio_val)
{
	pbmcb_tb[prio_val].pbmcb = oc_cb;
}
EXPORT_SYMBOL(register_pbm_notify);

static int __init pbm_module_init(void)
{
	struct cpufreq_policy *policy;
	struct cpu_pbm_policy *pbm_policy;
	unsigned int i;
	int cpu, ret;

	pm_notifier(_mt_pbm_pm_callback, 0);

	for_each_kernel_tracepoint(lookup_tracepoints, NULL);

	FOR_EACH_INTEREST(i) {
		if (pbm_tracepoints[i].tp == NULL) {
			pr_info("pbm Error, %s not found\n", pbm_tracepoints[i].name);
			tracepoint_cleanup();
			return -1;
		}
	}
	ret = tracepoint_probe_register(pbm_tracepoints[0].tp, pbm_tracepoints[0].func,  NULL);
	pbm_tracepoints[0].registered = true;

	if (!ret)
		pbm_tracepoints[0].registered = true;
	else
		pr_info("cpu_frequency: Couldn't activate tracepoint\n");

#if IS_ENABLED(CONFIG_MTK_DYNAMIC_LOADING_POWER_THROTTLING)
	register_dlpt_notify(&kicker_pbm_by_dlpt, DLPT_PRIO_PBM);
#endif

	for_each_possible_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);
		if (!policy)
			continue;

		if (policy->cpu == cpu) {
			pbm_policy = kzalloc(sizeof(*pbm_policy), GFP_KERNEL);
			if (!pbm_policy)
				return -ENOMEM;

			i = cpufreq_table_count_valid_entries(policy);
			if (!i) {
				pr_info("%s: CPUFreq table not found or has no valid entries\n",
					 __func__);
				return -ENODEV;
			}

			pbm_policy->em = em_cpu_get(policy->cpu);
			pbm_policy->max_cap_state = pbm_policy->em->nr_perf_states;
			pbm_policy->policy = policy;
			pbm_policy->cpu = cpu;
			pbm_policy->power_weight = DEFAULT_PBM_WEIGHT;

			ret = freq_qos_add_request(&policy->constraints,
				&pbm_policy->qos_req, FREQ_QOS_MAX,
				FREQ_QOS_MAX_DEFAULT_VALUE);

			if (ret < 0) {
				pr_notice("%s: Fail to add freq constraint (%d)\n",
					__func__, ret);
				return ret;
			}
			list_add_tail(&pbm_policy->cpu_pbm_list, &pbm_policy_list);
		}
	}

	ret = create_pbm_kthread();
	if (ret) {
		pr_notice("FAILED TO CREATE PBM KTHREAD\n");
		return ret;
	}

	return ret;
}

static void __exit pbm_module_exit(void)
{
	struct cpu_pbm_policy *pbm_policy, *pbm_policy_t;

	list_for_each_entry_safe(pbm_policy, pbm_policy_t, &pbm_policy_list, cpu_pbm_list) {
		freq_qos_remove_request(&pbm_policy->qos_req);
		list_del(&pbm_policy->cpu_pbm_list);
		kfree(pbm_policy);
	}
}

module_init(pbm_module_init);
module_exit(pbm_module_exit);

MODULE_AUTHOR("Samuel Hsieh");
MODULE_DESCRIPTION("MTK power budget management");
MODULE_LICENSE("GPL");
