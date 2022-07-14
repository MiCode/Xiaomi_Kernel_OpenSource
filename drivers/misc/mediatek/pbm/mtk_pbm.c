// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/cpufreq.h>
#include <linux/energy_model.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/pm_qos.h>
#include <linux/power_supply.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/suspend.h>
#include <trace/events/power.h>
#include <linux/tracepoint.h>
#include <linux/kallsyms.h>
#include <linux/of.h>
#include <linux/of_device.h>

#include "mtk_dynamic_loading_throttling.h"
#include "mtk_pbm.h"
#include "mtk_pbm_common.h"
#if IS_ENABLED(CONFIG_MTK_MDPM)
#include "mtk_mdpm.h"
#endif
#if IS_ENABLED(CONFIG_MTK_GPUFREQ_V2)
#include <mtk_gpufreq.h>
#endif
#include "mtk_pbm_gpu_cb.h"

#define DEFAULT_PBM_WEIGHT 1024
#define MAX_FLASH_POWER 3500
#define MAX_MD1_POWER 4000
#define MIN_CPU_POWER 600
#define BAT_PERCENT_LIMIT (15)
#define TIMER_INTERVAL_MS (200)

static LIST_HEAD(pbm_policy_list);

static bool mt_pbm_debug;

#define DLPTCB_MAX_NUM 16
static struct pbm_callback_table pbmcb_tb[DLPTCB_MAX_NUM] = { {0} };
#if IS_ENABLED(CONFIG_MTK_GPUFREQ_V2)
static struct pbm_gpu_callback_table pbm_gpu_cb = {0,0,0,0,0,0};
#endif

struct hpf hpf_ctrl = {
	.switch_md1 = 1,
	.switch_gpu = 0,
	.switch_flash = 0,
	.loading_dlpt = 0,
	.loading_md1 = 0,
	.loading_cpu = 0,
	.loading_gpu = 0,
	.loading_flash = MAX_FLASH_POWER,
	.to_cpu_budget = 0,
	.to_gpu_budget = 0,
};

static struct hpf hpf_ctrl_manual = {
	.loading_dlpt = 0,
	.loading_md1 = 0,
	.loading_cpu = 0,
	.loading_gpu = 0,
	.loading_flash = 0,
	.to_cpu_budget = 0,
	.to_gpu_budget = 0,
};

static struct pbm pbm_ctrl = {
	/* feature key */
	.pbm_stop = 0,
	.pbm_drv_done = 0,
	.hpf_en = 63,/* bin: 111111 (Flash, GPU, CPU, MD3, MD1, DLPT) */
	.manual_mode = 0, /*normal=0, UT(throttle)=1, UT(NO throttle)=2 */
};

static int g_dlpt_need_do = 1;
static int g_start_polling = 0;
static DEFINE_MUTEX(pbm_mutex);
static struct delayed_work poll_queue;
static struct notifier_block pbm_nb;
static bool g_pbm_update = false;
static unsigned int gpu_max_pb = 0;
static unsigned int gpu_min_pb = 0;
static int uisoc = -1;

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

unsigned long hpf_get_power_flash(void)
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

	if (!pbm_policy->em)
		return 0;

	for (i = pbm_policy->max_perf_state - 1; i > 0; i--) {
		if (power >= pbm_policy->em->table[i].power)
			break;
	}

	return pbm_policy->em->table[i].frequency;
}

static u32 cpu_freq_to_power(struct cpu_pbm_policy *pbm_policy, u32 freq)
{
	int i;

	if (!pbm_policy->em)
		return 0;

	for (i = pbm_policy->max_perf_state - 1; i > 0; i--) {
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
		if (frequency)
			freq_qos_update_request(&pbm_policy->qos_req, frequency);
	}
}

static void mtk_cpu_dlpt_unlimit_by_pbm(void)
{
	struct cpu_pbm_policy *pbm_policy;

	list_for_each_entry(pbm_policy, &pbm_policy_list, cpu_pbm_list) {
		freq_qos_update_request(&pbm_policy->qos_req, FREQ_QOS_MAX_DEFAULT_VALUE);
	}
}

static void mtk_gpufreq_set_power_limit_by_pbm(int limit_power)
{
#if IS_ENABLED(CONFIG_MTK_GPUFREQ_V2)
	if (pbm_gpu_cb.set_limit != NULL)
		pbm_gpu_cb.set_limit(TARGET_DEFAULT, LIMIT_PBM, limit_power, GPUPPM_KEEP_IDX);
#else
	return;
#endif
}

static void mtk_gpufreq_get_max_min_pb(void)
{
#if IS_ENABLED(CONFIG_MTK_GPUFREQ_V2)
	if (pbm_gpu_cb.get_max_pb != NULL
			&& gpu_max_pb == 0) {
		gpu_max_pb = pbm_gpu_cb.get_max_pb(TARGET_DEFAULT);
	}
	if (pbm_gpu_cb.get_min_pb != NULL
			&& gpu_min_pb == 0) {
		gpu_min_pb = pbm_gpu_cb.get_min_pb(TARGET_DEFAULT);
	}
#else
	return;
#endif
}

static void pbm_allocate_budget_manager(void)
{
	int _dlpt, md1, dlpt, cpu, gpu, flash, tocpu, togpu;
	int multiple;
	int cpu_lower_bound = MIN_CPU_POWER;
	static int pre_tocpu, pre_togpu;

	if (pbm_ctrl.manual_mode == 1 || pbm_ctrl.manual_mode == 2) {
		dlpt = hpf_ctrl_manual.loading_dlpt;
		md1 = hpf_ctrl_manual.loading_md1;
		cpu = hpf_ctrl_manual.loading_cpu;
		gpu = hpf_ctrl_manual.loading_gpu;
		flash = hpf_ctrl_manual.loading_flash;

	} else {
		md1 = hpf_get_power_md1();
		dlpt = hpf_get_power_dlpt();
		cpu = hpf_get_power_cpu();
		gpu = hpf_get_power_gpu();
		flash = hpf_get_power_flash();
	}

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

		if (pbm_ctrl.manual_mode != 2) {
			mtk_cpu_dlpt_set_limit_by_pbm(tocpu);
			mtk_gpufreq_set_power_limit_by_pbm(togpu);
		}
	} else {
		multiple = (_dlpt * 1000) / (cpu + gpu);

		if (multiple > 0) {
			tocpu = (multiple * cpu) / 1000;
			togpu = (multiple * gpu) / 1000;
		} else {
			tocpu = 1;
			togpu = 1;
		}

		if (togpu > gpu_max_pb) {
			togpu = gpu_max_pb;
			tocpu = _dlpt - gpu_max_pb;
		}

		if (tocpu < cpu_lower_bound) {
			tocpu = cpu_lower_bound;
			togpu = _dlpt - cpu_lower_bound;
		}

		if (togpu < gpu_min_pb)
			togpu = gpu_min_pb;

		if (tocpu <= 0)
			tocpu = 1;
		if (togpu <= 0)
			togpu = 1;

		if (pbm_ctrl.manual_mode != 2) {
			mtk_cpu_dlpt_set_limit_by_pbm(tocpu);
			mtk_gpufreq_set_power_limit_by_pbm(togpu);
		}
	}

	hpf_ctrl.to_cpu_budget = tocpu;
	hpf_ctrl.to_gpu_budget = togpu;

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
	struct pbm *pwrctrl = &pbm_ctrl;

	if (!pwrctrl->pbm_drv_done) {
		pr_info("pwrctrl->pbm_drv_done: %d\n", pwrctrl->pbm_drv_done);
		return false;
	}

	return true;
}

static bool pbm_update_table_info(enum pbm_kicker kicker, struct mrp *mrpmgr)
{
	struct hpf *hpfmgr = &hpf_ctrl;
	bool is_update = false;

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
		if (hpfmgr->loading_cpu != mrpmgr->loading_cpu) {
			hpfmgr->loading_cpu = mrpmgr->loading_cpu;
			is_update = true;
		}
		break;
	case KR_GPU:
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

static void mtk_power_budget_manager(enum pbm_kicker kicker, struct mrp *mrpmgr)
{
	bool pbm_enable = false;
	bool pbm_update = false;

	pbm_enable = pbm_func_enable_check();
	if (!pbm_enable)
		return;

	pbm_update = pbm_update_table_info(kicker, mrpmgr);
	if (!pbm_update)
		return;
	else
		g_pbm_update = pbm_update;

}

static void pbm_timer_add(int enable)
{
	if (enable)
		mod_delayed_work(system_freezable_power_efficient_wq,
			&poll_queue,
			msecs_to_jiffies(TIMER_INTERVAL_MS));
	else {
		cancel_delayed_work(&poll_queue);
		//unlimit CG
		mtk_cpu_dlpt_unlimit_by_pbm();
		mtk_gpufreq_set_power_limit_by_pbm(0);
	}

}


void pbm_check_and_run_polling(int uisoc, int pbm_stop)
{
	if (((uisoc <= BAT_PERCENT_LIMIT && uisoc >= 0 && pbm_stop == 0)
				|| pbm_stop == 2) && g_start_polling == 0) {
		g_start_polling = 1;
		pbm_timer_add(g_start_polling);
		pr_info("[DLPT] pbm polling, soc=%d polling=%d stop=%d\n", uisoc,
			g_start_polling, pbm_ctrl.pbm_stop);
	} else if (((uisoc > BAT_PERCENT_LIMIT && pbm_stop == 0)
			|| pbm_stop == 1) && g_start_polling == 1) {
		g_start_polling = 0;

		pr_info("[DLPT] pbm release polling, soc=%d polling=%d stop=%d\n",
			uisoc, g_start_polling, pbm_ctrl.pbm_stop);
	}
}

int pbm_psy_event(struct notifier_block *nb, unsigned long event, void *v)
{
	struct power_supply *psy = v;
	union power_supply_propval val;
	int ret;

	if (strcmp(psy->desc->name, "battery") != 0)
		return NOTIFY_DONE;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CAPACITY, &val);
	if (ret)
		return NOTIFY_DONE;

	uisoc = val.intval;
	pbm_check_and_run_polling(uisoc, pbm_ctrl.pbm_stop);

	return NOTIFY_DONE;
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

void kicker_pbm_by_cpu(unsigned int loading)
{
	struct mrp mrpmgr = {0};

	mrpmgr.loading_cpu = loading;
	mtk_power_budget_manager(KR_CPU, &mrpmgr);
}

void kicker_pbm_by_gpu(bool status, unsigned int loading, int voltage)
{
	struct mrp mrpmgr = {0};

	mrpmgr.switch_gpu = status;
	mrpmgr.loading_gpu = loading;

	mtk_power_budget_manager(KR_GPU, &mrpmgr);
}

void kicker_pbm_by_flash(bool status)
{
	struct mrp mrpmgr = {0};

	mrpmgr.switch_flash = status;

	mtk_power_budget_manager(KR_FLASH, &mrpmgr);
}
EXPORT_SYMBOL(kicker_pbm_by_flash);

static void pbm_thread_handle(struct work_struct *work)
{
	int g_dlpt_state_sync = 0;
	unsigned int gpu_cur_pb = 0, gpu_cur_volt = 0;
	struct cpu_pbm_policy *pbm_policy;
	unsigned int req_total_power = 0;

	mtk_gpufreq_get_max_min_pb();
#if IS_ENABLED(CONFIG_MTK_GPUFREQ_V2)
	if (pbm_gpu_cb.get_cur_vol != NULL)
		gpu_cur_volt = pbm_gpu_cb.get_cur_vol(TARGET_DEFAULT);
	if (pbm_gpu_cb.get_cur_pb != NULL)
		gpu_cur_pb = pbm_gpu_cb.get_cur_pb(TARGET_DEFAULT);
#endif

	if (gpu_cur_volt)
		kicker_pbm_by_gpu(true, gpu_cur_pb, gpu_cur_volt);
	else
		kicker_pbm_by_gpu(false, gpu_cur_pb, gpu_cur_volt);


	list_for_each_entry(pbm_policy, &pbm_policy_list, cpu_pbm_list) {
		req_total_power += pbm_policy->power;
	}
	kicker_pbm_by_cpu(req_total_power);

	mutex_lock(&pbm_mutex);
	if (g_dlpt_need_do == 1 && g_pbm_update == true) {
		g_pbm_update = false;
		pbm_allocate_budget_manager();
		g_dlpt_state_sync = 0;
	}
	pbm_timer_add(g_start_polling);
	mutex_unlock(&pbm_mutex);
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
	unsigned int cpu;

	if (!g_start_polling)
		return;

	policy = cpufreq_cpu_get(cpu_id);
	if (!policy)
		return;
	if (cpu_id != cpumask_first(policy->related_cpus)) {
		cpufreq_cpu_put(policy);
		return;
	}

	list_for_each_entry(pbm_policy, &pbm_policy_list, cpu_pbm_list) {
		cpu = pbm_policy->policy->cpu;

		if (cpu == cpu_id)
			pbm_policy->freq = frequency;
		else if (pbm_policy->freq == 0)
			pbm_policy->freq = cpufreq_quick_get(cpu);
		else
			continue;

		pbm_policy->num_cpus = cpumask_weight(pbm_policy->policy->cpus);
		pbm_policy->power = cpu_freq_to_power(pbm_policy, pbm_policy->freq) *
			pbm_policy->num_cpus;
	}

	cpufreq_cpu_put(policy);
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
	if (prio_val == PBM_PRIO_GPU)
	{
#if IS_ENABLED(CONFIG_MTK_GPUFREQ_V2)
		struct pbm_gpu_callback_table *gpu_cb;
		gpu_cb = (struct pbm_gpu_callback_table *) oc_cb;
		pbm_gpu_cb.get_max_pb = gpu_cb->get_max_pb;
		pbm_gpu_cb.get_min_pb = gpu_cb->get_min_pb;
		pbm_gpu_cb.get_cur_pb = gpu_cb->get_cur_pb;
		pbm_gpu_cb.get_cur_vol = gpu_cb->get_cur_vol;
		pbm_gpu_cb.get_opp_by_pb = gpu_cb->get_opp_by_pb;
		pbm_gpu_cb.set_limit = gpu_cb->set_limit;
#else
		return;
#endif
	} else {
		pbmcb_tb[prio_val].pbmcb = oc_cb;
	}
}
EXPORT_SYMBOL(register_pbm_notify);

void register_pbm_gpu_notify(void *cb)
{
	register_pbm_notify(cb, PBM_PRIO_GPU);
}
EXPORT_SYMBOL(register_pbm_gpu_notify);

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

static int mt_pbm_manual_mode_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "manual_mode: %d\n", pbm_ctrl.manual_mode);
	if (pbm_ctrl.manual_mode > 0) {
		seq_printf(m, "request (C/G)=%lu,%lu=>(D/M1/F)=%lu,%lu,%lu\n",
			hpf_ctrl_manual.loading_cpu,
			hpf_ctrl_manual.loading_gpu,
			hpf_ctrl_manual.loading_dlpt,
			hpf_ctrl_manual.loading_md1,
			hpf_ctrl_manual.loading_flash);
	} else {
		seq_printf(m, "request (C/G)=%lu,%lu=>(D/M1/F)=%lu,%lu,%lu\n",
			hpf_ctrl.loading_cpu,
			hpf_ctrl.loading_gpu,
			hpf_ctrl.loading_dlpt,
			hpf_ctrl.loading_md1,
			hpf_ctrl.loading_flash);
	}
	seq_printf(m, "budget (C/G)=%lu,%lu\n",
		hpf_ctrl.to_cpu_budget,
		hpf_ctrl.to_gpu_budget);

	return 0;
}

static ssize_t mt_pbm_manual_mode_proc_write
(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[64], cmd[21];
	int len = 0, manual_mode = 0;
	int loading_dlpt, loading_md1;
	int loading_cpu, loading_gpu, loading_flash;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (sscanf(desc, "%20s %d %d %d %d %d %d", cmd, &manual_mode, &loading_dlpt,
		&loading_md1, &loading_cpu, &loading_gpu, &loading_flash) != 7) {
		pr_notice("parameter number not correct\n");
		return -EPERM;
	}

	if (strncmp(cmd, "manual", 6))
		return -EINVAL;

	if (manual_mode == 1 || manual_mode == 2) {
		hpf_ctrl_manual.loading_dlpt = loading_dlpt;
		hpf_ctrl_manual.loading_md1 = loading_md1;
		hpf_ctrl_manual.loading_cpu = loading_cpu;
		hpf_ctrl_manual.loading_gpu = loading_gpu;
		hpf_ctrl_manual.loading_flash = loading_flash;
		pbm_ctrl.manual_mode = manual_mode;
		pbm_allocate_budget_manager();
	} else if (manual_mode == 0)
		pbm_ctrl.manual_mode = 0;
	else
		pr_notice("pbm manual setting should be 0 or 1 or 2\n");

	return count;
}

static int mt_pbm_stop_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "pbm stop: %d\n", pbm_ctrl.pbm_stop);
	return 0;
}

static ssize_t mt_pbm_stop_proc_write
(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[64], cmd[21];
	int len = 0, stop = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (sscanf(desc, "%20s %d", cmd, &stop) != 2) {
		pr_notice("parameter number not correct\n");
		return -EPERM;
	}

	if (strncmp(cmd, "stop", 4))
		return -EINVAL;

	if (stop == 0 || stop == 1 || stop == 2) {
		pbm_ctrl.pbm_stop = stop;
		pbm_check_and_run_polling(uisoc, pbm_ctrl.pbm_stop);
	} else
		pr_notice("pbm stop should be 0 or 1 or 2\n");

	return count;
}

#define PROC_FOPS_RW(name)						\
static int mt_ ## name ## _proc_open(struct inode *inode, struct file *file)\
{									\
	return single_open(file, mt_ ## name ## _proc_show, PDE_DATA(inode));\
}									\
static const struct proc_ops mt_ ## name ## _proc_fops = {	\
	.proc_open		= mt_ ## name ## _proc_open,			\
	.proc_read		= seq_read,					\
	.proc_lseek		= seq_lseek,					\
	.proc_release		= single_release,				\
	.proc_write		= mt_ ## name ## _proc_write,			\
}

#define PROC_FOPS_RO(name)						\
static int mt_ ## name ## _proc_open(struct inode *inode, struct file *file)\
{									\
	return single_open(file, mt_ ## name ## _proc_show, PDE_DATA(inode));\
}									\
static const struct proc_ops mt_ ## name ## _proc_fops = {	\
	.proc_open		= mt_ ## name ## _proc_open,		\
	.proc_read		= seq_read,				\
	.proc_lseek		= seq_lseek,				\
	.proc_release	= single_release,			\
}

#define PROC_ENTRY(name)	{__stringify(name), &mt_ ## name ## _proc_fops}
PROC_FOPS_RW(pbm_debug);
PROC_FOPS_RW(pbm_manual_mode);
PROC_FOPS_RW(pbm_stop);

static int mt_pbm_create_procfs(void)
{
	struct proc_dir_entry *dir = NULL;
	int i;

	struct pentry {
		const char *name;
		const struct proc_ops *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(pbm_debug),
		PROC_ENTRY(pbm_manual_mode),
		PROC_ENTRY(pbm_stop),
	};

	dir = proc_mkdir("pbm", NULL);

	if (!dir) {
		pr_notice("fail to create /proc/pbm @ %s()\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create(entries[i].name, 0660, dir, entries[i].fops))
			pr_notice("@%s: create /proc/pbm/%s failed\n", __func__,
				    entries[i].name);
	}

	return 0;
}

static int pbm_probe(struct platform_device *pdev)
{
	struct cpufreq_policy *policy;
	struct cpu_pbm_policy *pbm_policy;
	struct device_node *np;
	unsigned int i;
	int cpu, ret;

	np = of_find_compatible_node(NULL, NULL, "mediatek,pbm");

	if (!np) {
		dev_notice(&pdev->dev, "get pbm node fail\n");
		return -ENODATA;
	}

	mt_pbm_create_procfs();

	pm_notifier(_mt_pbm_pm_callback, 0);
	pbm_nb.notifier_call = pbm_psy_event;
	ret = power_supply_reg_notifier(&pbm_nb);
	if (ret) {
		pr_notice("pbm power_supply_reg_notifier fail\n");
		return ret;
	}

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
			if (pbm_policy->em) {
				pbm_policy->max_perf_state = pbm_policy->em->nr_perf_states;
			} else {
				pr_info("%s: Fail to get em from cpu %d\n",
					__func__, policy->cpu);
			}
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

	INIT_DELAYED_WORK(&poll_queue, pbm_thread_handle);
	pbm_ctrl.pbm_drv_done = 1;

	return ret;
}

static int pbm_remove(struct platform_device *pdev)
{
	struct cpu_pbm_policy *pbm_policy, *pbm_policy_t;

	cancel_delayed_work_sync(&poll_queue);
	//unlimit CG
	mtk_cpu_dlpt_unlimit_by_pbm();
	mtk_gpufreq_set_power_limit_by_pbm(0);

	list_for_each_entry_safe(pbm_policy, pbm_policy_t, &pbm_policy_list, cpu_pbm_list) {
		freq_qos_remove_request(&pbm_policy->qos_req);
		cpufreq_cpu_put(pbm_policy->policy);
		list_del(&pbm_policy->cpu_pbm_list);
		kfree(pbm_policy);
	}

	return 0;
}

static const struct of_device_id pbm_of_match[] = {
	{ .compatible = "mediatek,pbm", },
	{},
};
MODULE_DEVICE_TABLE(of, pbm_of_match);

static struct platform_driver pbm_driver = {
	.probe = pbm_probe,
	.remove = pbm_remove,
	.driver = {
		.name = "mtk-power_budget_management",
		.of_match_table = pbm_of_match,
	},
};
module_platform_driver(pbm_driver);

MODULE_AUTHOR("Samuel Hsieh");
MODULE_DESCRIPTION("MTK power budget management");
MODULE_LICENSE("GPL");
