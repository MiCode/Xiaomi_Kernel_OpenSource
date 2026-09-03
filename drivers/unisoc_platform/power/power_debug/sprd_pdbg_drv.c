// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2017 Spreadtrum Communications Inc.
 */
#include <linux/cpu_pm.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/proc_fs.h>
#include <linux/time.h>
#include <linux/rtc.h>
#include <linux/sched.h>
#include <linux/soc/sprd/sprd_pdbg.h>
#include <linux/sprd_sip_svc.h>
#include <linux/suspend.h>
#include <linux/wakeup_reason.h>
#include "sprd_pdbg_comm.h"
#include "sprd_regs_info.h"
#include "sprd_slp_info.h"
#include "sprd_wakeup_info.h"
#include "sprd_pdbg_misc.h"

int sprd_pdbg_log_force;
static struct power_debug *g_pdbg;
#define PDBG_PROC_NAME "sprd_pdbg"
static BLOCKING_NOTIFIER_HEAD(pdbg_nb_chain);

enum {
	PDBG_PHASE0,
	PDBG_PHASE1,
	PDBG_PHASE_MAX
};

enum {
	SIP_SVC_PWR_V0,
	SIP_SVC_PWR_V1,
};

struct power_debug {
	struct task_struct *task;
	struct device *dev;
	struct sprd_sip_svc_pwr_ops *power_ops;
	struct slp_info_data *slp_info_data;
	struct wakeup_info_data *ws_data;
	struct regs_info_data *regs_data;
	struct misc_data *misc_data;
	struct notifier_block pm_notifier_block;
	struct notifier_block cpu_pm_notifier_block;
	bool pm_notifier_inited;
	bool cpu_pm_notifier_inited;
	u32 scan_interval;
	bool module_log_enable;
	bool is_32b_machine;
	struct proc_dir_entry *proc_dir;
};

static inline struct power_debug *sprd_pdbg_get_instance(void)
{
	return g_pdbg;
}


int sprd_pdbg_notify_register(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&pdbg_nb_chain, nb);
}
EXPORT_SYMBOL(sprd_pdbg_notify_register);

int sprd_pdbg_notify_unregister(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&pdbg_nb_chain, nb);
}
EXPORT_SYMBOL(sprd_pdbg_notify_unregister);

int pdbg_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&pdbg_nb_chain, val, v);
}

void smccc_res_to_u64_array(struct arm_smccc_res *src, u64 *dst, u64 *merge)
{
	int i;

	dst[0] = src->a0;
	dst[1] = src->a1;
	dst[2] = src->a2;
	dst[3] = src->a3;

	if (!merge)
		return;

	for (i = 0; i < PDBG_INFO_NUM; i++)
		merge[i] = (merge[i] | (dst[i] << 32));
}

int sprd_pdbg_regs_get_once(u32 info_type, u64 *ret_vals)
{
	u64 ret, ret_vals_h[PDBG_INFO_NUM] = {0};
	struct arm_smccc_res ret_l, ret_h;
	struct power_debug *pdbg = sprd_pdbg_get_instance();

	if (!pdbg || !pdbg->power_ops) {
		SPRD_PDBG_ERR("pdbg null!!!\n");
		return -EINVAL;
	}

	ret = pdbg->power_ops->pdbg_info_trans(info_type, PDBG_PHASE0, 0, 0, &ret_l);
	if (ret == ERROR_MAGIC) {
		SPRD_PDBG_ERR("Get pdbg info ret_l: %d error\n", info_type);
		return -EINVAL;
	}
	smccc_res_to_u64_array(&ret_l, ret_vals, NULL);

	if (pdbg->is_32b_machine) {
		ret = pdbg->power_ops->pdbg_info_trans(info_type, PDBG_PHASE1, 0, 0, &ret_h);
		if (ret == ERROR_MAGIC) {
			SPRD_PDBG_ERR("Get pdbg info ret_h: %d error\n", info_type);
			return -EINVAL;
		}
		smccc_res_to_u64_array(&ret_h, ret_vals_h, ret_vals);
	}

	return 0;
}

int sprd_pdbg_ws_info_trans(u32 *major, u32 *domain_id, u32 *hwirq)
{
	u64 ret;
	struct arm_smccc_res ret_vals = {0};

	struct power_debug *pdbg = sprd_pdbg_get_instance();

	if (!pdbg || !pdbg->power_ops) {
		SPRD_PDBG_ERR("pdbg null!!!\n");
		return -EINVAL;
	}

	ret = pdbg->power_ops->pdbg_info_trans(PDBG_WS, 0, 0, 0, &ret_vals);
	if (ret == ERROR_MAGIC) {
		SPRD_PDBG_ERR("Get ws info error\n");
		return -EINVAL;
	}

	*major = (u32)ret_vals.a0;
	*domain_id = (u32)ret_vals.a1;
	*hwirq = (u32)ret_vals.a2;

	return 0;
}

int sprd_pdbg_misc_trans(u32 cmd, u32 priv, struct arm_smccc_res *ret_vals)
{
	u64 ret;
	struct power_debug *pdbg = sprd_pdbg_get_instance();

	if (!pdbg || !pdbg->power_ops) {
		SPRD_PDBG_ERR("pdbg null!!!\n");
		return -EINVAL;
	}

	ret = pdbg->power_ops->pdbg_info_trans(PDBG_MISC, cmd, priv, 0, ret_vals);
	if (ret == ERROR_MAGIC) {
		SPRD_PDBG_ERR("misc info trans: [%u %u] error\n", cmd, priv);
		return -EINVAL;
	}

	return 0;
}

static void sprd_pdbg_notify_cb(struct power_debug *pdbg, unsigned long cmd)
{
	if (pdbg->slp_info_data->notify_cb)
		pdbg->slp_info_data->notify_cb(pdbg->slp_info_data, cmd);

	if (pdbg->ws_data->notify_cb)
		pdbg->ws_data->notify_cb(pdbg->ws_data, cmd);

	if (pdbg->regs_data->notify_cb)
		pdbg->regs_data->notify_cb(pdbg->regs_data, cmd);
}

void sprd_pdbg_kernel_active_ws_show(void)
{
	char log_buf[MAX_SUSPEND_ABORT_LEN];

	pm_get_active_wakeup_sources(log_buf, MAX_SUSPEND_ABORT_LEN);
	SPRD_PDBG_INFO("%s\n", log_buf);
}

static int sprd_pdbg_cpu_pm_notifier(struct notifier_block *self,
	unsigned long cmd, void *v)
{

	struct power_debug *pdbg = sprd_pdbg_get_instance();

	if (!pdbg)
		return NOTIFY_DONE;

	switch (cmd) {
	case CPU_CLUSTER_PM_ENTER:
		SPRD_PDBG_INFO("#---------PDBG DEEP SLEEP START---------#\n");
		pdbg->module_log_enable = true;
		sprd_pdbg_notify_cb(pdbg, SPRD_CPU_PM_ENTER);
		SPRD_PDBG_INFO("#---------PDBG DEEP SLEEP END-----------#\n");
		break;
	case CPU_CLUSTER_PM_EXIT:
		SPRD_PDBG_INFO("#---------PDBG WAKEUP SCENE START---------#\n");
		sprd_pdbg_notify_cb(pdbg, SPRD_CPU_PM_EXIT);
		SPRD_PDBG_INFO("#---------PDBG WAKEUP SCENE END-----------#\n");
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static int sprd_pdbg_pm_notifier(struct notifier_block *notifier,
		unsigned long pm_event, void *unused)
{
	struct power_debug *pdbg = sprd_pdbg_get_instance();

	if (!pdbg)
		return NOTIFY_DONE;

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		sprd_pdbg_notify_cb(pdbg, SPRD_PM_ENTER);
		break;
	case PM_POST_SUSPEND:
		pdbg->module_log_enable = false;
		sprd_pdbg_notify_cb(pdbg, SPRD_PM_EXIT);
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}

static int sprd_pdbg_thread(void *data)
{
	struct power_debug *pdbg = (struct power_debug *)data;

	while (pdbg->task) {

		if (kthread_should_stop())
			break;

		SPRD_PDBG_INFO("#---------PDBG LIGHT SLEEP START---------#\n");
		sprd_pdbg_notify_cb(pdbg, SPRD_PM_MONITOR);
		SPRD_PDBG_INFO("#---------PDBG LIGHT SLEEP END-----------#\n");

		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(pdbg->scan_interval * (long)HZ);
	}

	return 0;
}

static void sprd_pdbg_stop_monitor(struct power_debug *pdbg)
{
	if (!pdbg)
		return;

	if (pdbg->task) {
		kthread_stop(pdbg->task);
		pdbg->task = NULL;
		if (pdbg->cpu_pm_notifier_inited)
			cpu_pm_unregister_notifier(&pdbg->cpu_pm_notifier_block);
		if (pdbg->pm_notifier_inited)
			unregister_pm_notifier(&pdbg->pm_notifier_block);
	}
}

static void sprd_pdbg_devm_monitor_action(void *_data)
{
	struct power_debug *pdbg = _data;

	sprd_pdbg_stop_monitor(pdbg);
}

static int sprd_pdbg_start_monitor(struct power_debug *pdbg)
{
	struct task_struct *ptask;
	int err;

	if (!pdbg)
		return -EINVAL;

	pdbg->pm_notifier_inited = false;
	pdbg->cpu_pm_notifier_inited = false;

	if (!pdbg->task) {
		ptask = kthread_create(sprd_pdbg_thread, pdbg, "sprd-pdbg-thread");
		if (IS_ERR(ptask)) {
			SPRD_PDBG_ERR("Unable to start kernel thread.\n");
			return PTR_ERR(ptask);
		}
		pdbg->task = ptask;
		wake_up_process(ptask);

		err = devm_add_action(pdbg->dev, sprd_pdbg_devm_monitor_action, pdbg);
		if (err) {
			sprd_pdbg_devm_monitor_action(pdbg);
			SPRD_PDBG_ERR("failed to add sprd_pdbg_devm_action\n");
			return err;
		}

		err = cpu_pm_register_notifier(&pdbg->cpu_pm_notifier_block);
		if (err) {
			SPRD_PDBG_ERR("cpu_pm_notifier_block register failed!!!\n");
			return err;
		}
		pdbg->cpu_pm_notifier_inited = true;

		err = register_pm_notifier(&pdbg->pm_notifier_block);
		if (err) {
			SPRD_PDBG_ERR("pm_notifier_block register failed!!!\n");
			return err;
		}
		pdbg->pm_notifier_inited = true;
	}

	return 0;
}

void sprd_pdbg_msg_print(const char *format, ...)
{

	struct va_format vaf;
	va_list args;
	struct power_debug *pdbg = sprd_pdbg_get_instance();

	if (!pdbg || !pdbg->module_log_enable)
		return;

	va_start(args, format);
	vaf.fmt = format;
	vaf.va = &args;
	SPRD_PDBG_INFO("%pV", &vaf);
	va_end(args);
}
EXPORT_SYMBOL_GPL(sprd_pdbg_msg_print);

void sprd_pdbg_time_get(struct rtc_time *time)
{
	struct timespec64 ts;

	ts.tv_sec = 0;
	ktime_get_real_ts64(&ts);
	rtc_time64_to_tm(ts.tv_sec, time);
}

static void sprd_pdbg_devm_proc_action(void *_data)
{
	struct power_debug *pdbg = _data;

	proc_remove(pdbg->proc_dir);
}

static int sprd_pdbg_proc_create(struct power_debug *pdbg)
{
	int ret;

	pdbg->proc_dir = proc_mkdir(PDBG_PROC_NAME, NULL);
	if (!pdbg->proc_dir) {
		SPRD_PDBG_ERR("Proc dir create failed\n");
		return -EBADF;
	}

	ret = devm_add_action(pdbg->dev, sprd_pdbg_devm_proc_action, pdbg);
	if (ret) {
		sprd_pdbg_devm_proc_action(pdbg);
		SPRD_PDBG_ERR("failed to add sprd_pdbg_devm_proc_action\n");
		return ret;
	}

	return 0;
}

static int sprd_pdbg_late_init(struct power_debug *pdbg)
{
	if (sprd_pdbg_misc_trans(PDBG_MISC_APSYS_PD_SET, pdbg->misc_data->engpc_boot_mode, NULL)) {
		SPRD_PDBG_ERR("apsys_pd_disable set failed\n");
		return -EINVAL;
	}

	return 0;
}

static int sprd_pdbg_probe(struct platform_device *pdev)
{
	int ret;
	struct sprd_sip_svc_handle *sip;
	struct power_debug *pdbg;


	pdbg = devm_kzalloc(&pdev->dev, sizeof(struct power_debug), GFP_KERNEL);
	if (!pdbg) {
		SPRD_PDBG_ERR("%s: pdbg alloc error\n", __func__);
		return -ENOMEM;
	}

	pdbg->scan_interval = 30;
	pdbg->dev = &pdev->dev;
	pdbg->task = NULL;
	pdbg->module_log_enable = false;
	pdbg->cpu_pm_notifier_block.notifier_call = sprd_pdbg_cpu_pm_notifier;
	pdbg->pm_notifier_block.notifier_call = sprd_pdbg_pm_notifier;
	sip = sprd_sip_svc_get_handle();
	pdbg->power_ops = &sip->pwr_ops;
	pdbg->is_32b_machine = (sizeof(unsigned long) < sizeof(u64));

	ret = sprd_pdbg_proc_create(pdbg);
	if (ret) {
		SPRD_PDBG_ERR("sprd_pdbg_proc_create failed\n");
		return ret;
	}

	ret = sprd_slp_info_init(pdbg->dev, pdbg->proc_dir, &pdbg->slp_info_data);
	if (ret) {
		SPRD_PDBG_ERR("failed to sprd_slp_info_init\n");
		return ret;
	}

	ret = sprd_regs_info_init(pdbg->dev, pdbg->proc_dir, &pdbg->regs_data);
	if (ret) {
		SPRD_PDBG_ERR("failed to sprd_regs_info_init\n");
		return ret;
	}

	ret = sprd_pdbg_ws_info_init(pdbg->dev, pdbg->proc_dir, &pdbg->ws_data);
	if (ret) {
		SPRD_PDBG_ERR("failed to sprd_pdbg_ws_info_init\n");
		return ret;
	}

	ret = sprd_pdbg_misc_init(pdbg->dev, pdbg->proc_dir, &pdbg->misc_data);
	if (ret) {
		SPRD_PDBG_ERR("failed to sprd_pdbg_misc_init\n");
		return ret;
	}

	ret = sprd_pdbg_start_monitor(pdbg);
	if (ret) {
		SPRD_PDBG_ERR("failed to start pdbg monitor\n");
		return ret;
	}

	g_pdbg = pdbg;

	ret = sprd_pdbg_late_init(pdbg);
	if (ret) {
		SPRD_PDBG_ERR("pdbg late inti failed\n");
		return ret;
	}

	return 0;
}

static const struct of_device_id sprd_pdbg_of_match[] = {
{
	.compatible = "sprd,pdbg",
},
{},
};
MODULE_DEVICE_TABLE(of, sprd_pdbg_of_match);

static struct platform_driver sprd_pdbg_driver = {
	.probe = sprd_pdbg_probe,
	.driver = {
		.name = "sprd_pdbg",
		.of_match_table = sprd_pdbg_of_match,
	},
};

module_platform_driver(sprd_pdbg_driver);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("sprd power debug driver");
