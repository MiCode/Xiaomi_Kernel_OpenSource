// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <asm/cpuidle.h>
#include <asm/suspend.h>
#include <linux/cpu_pm.h>
#include <linux/cpuidle.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/syscalls.h>
#include <linux/suspend.h>
#include <linux/syscore_ops.h>

#include <mtk_lpm.h>
#include <mtk_lpm_internal.h>
#include <mtk_lpm_module.h>
#include <mtk_lpm_platform.h>
#include "mtk_lpm_registry.h"

#define MTK_LPM_MODULE_MAGIC	0xFCD03DC3
#define MTK_LPM_CPUIDLE_DRIVER		0xD0
#define MTK_LPM_MODLE			0xD1
#define MTK_LPM_SYS_ISSUER		0xD2

DEFINE_SPINLOCK(__lpm_sys_sync_lock);

typedef int (*state_enter)(struct cpuidle_device *dev,
			   struct cpuidle_driver *drv, int idx);
typedef int (*s2idle_enter)(struct cpuidle_device *dev,
			   struct cpuidle_driver *drv, int idx);

struct mtk_lpm_state_enter_fp {
	state_enter state;
	s2idle_enter s2idle;
};

struct mtk_lpm_model_info {
	const char *name;
	void *mod;
};

struct mtk_lpm_module_reg {
	int magic;
	int type;
	union {
		struct mtk_lpm_issuer *issuer;
		struct mtk_lpm_model_info info;
		struct mtk_lpm_state_enter_fp fp;
	} data;
};

struct mtk_lpm {
	struct mtk_lpm_model suspend;
	struct mtk_lpm_issuer *issuer;
};

static struct mtk_lpm mtk_lpm_system;

static DEFINE_SPINLOCK(mtk_lp_mod_locker);
static DEFINE_SPINLOCK(mtk_lpm_sys_locker);
static RAW_NOTIFIER_HEAD(mtk_lpm_notifier);


struct mtk_lpm_models {
	struct mtk_lpm_model *mod[CPUIDLE_STATE_MAX];
};
static DEFINE_PER_CPU(struct mtk_lpm_models, mtk_lpm_mods);

struct mtk_lpm_states_enter {
	state_enter cpuidle[CPUIDLE_STATE_MAX];
	s2idle_enter s2idle[CPUIDLE_STATE_MAX];
};
static DEFINE_PER_CPU(struct mtk_lpm_states_enter, mtk_lpm_cstate);


#define mtk_lp_pm_notify(_id, _data)\
		({raw_notifier_call_chain(&mtk_lpm_notifier,\
					  _id, _data); })



#define mtk_lp_notify_var(action, var)\
		((var & MTK_LPM_NB_MASK) | action)


#define IS_MTK_LPM_MODS_VALID(idx)\
		(idx < CPUIDLE_STATE_MAX)

enum mtk_lpm_state_type {
	mtk_lpm_state_cpuidle,
	mtk_lpm_state_s2idle,
};

static int mtk_lpm_state_enter(int type, struct cpuidle_device *dev,
			     struct cpuidle_driver *drv, int idx);

int mtk_lpm_s2idle_state_enter(struct cpuidle_device *dev,
			   struct cpuidle_driver *drv, int idx)
{
	return mtk_lpm_state_enter(mtk_lpm_state_s2idle, dev, drv, idx);
}

int mtk_lpm_cpuidle_state_enter(struct cpuidle_device *dev,
			   struct cpuidle_driver *drv, int idx)
{
	return mtk_lpm_state_enter(mtk_lpm_state_cpuidle, dev, drv, idx);
}

int mtk_lpm_cpuidle_state_percpu_set(int cpu, struct mtk_lpm_module_reg *p)
{
	struct mtk_lpm_state_enter_fp *fp = &p->data.fp;
	struct cpuidle_driver *drv;
	struct mtk_lpm_states_enter *ptr;
	int idx;

	drv = cpuidle_get_driver();

	if (!drv || !fp) {
		pr_info("[name:mtk_lpm][P] - cpuidle state register fail (%s:%d)\n",
					__func__, __LINE__);
		return -EINVAL;
	}

	for (idx = 1; idx < drv->state_count; ++idx) {
		if (!IS_MTK_LPM_MODS_VALID(idx)) {
			pr_info("[name:mtk_lpm][P] - mod(%s) out of index (%d) (%s:%d)\n",
					p->data.info.name, idx,
					__func__, __LINE__);
			break;
		}

		ptr = &per_cpu(mtk_lpm_cstate, cpu);
		if (fp->state) {
			ptr->cpuidle[idx] = drv->states[idx].enter;
			drv->states[idx].enter = fp->state;
		}
		if (fp->s2idle) {
			ptr->s2idle[idx] = drv->states[idx].enter_s2idle;
			drv->states[idx].enter_s2idle = fp->s2idle;
		}
	}
	return 0;
}

int mtk_lpm_model_percpu_set(int cpu, struct mtk_lpm_module_reg *p)
{
	struct mtk_lpm_models *ptr;
	struct cpuidle_driver *drv;
	int idx;

	drv = cpuidle_get_driver();

	if (!drv) {
		pr_info("[name:mtk_lpm][P] - cpuidle drv is null (%s:%d)\n",
					__func__, __LINE__);
		return -EINVAL;
	}

	for (idx = 0; idx < drv->state_count; ++idx) {
		if (!IS_MTK_LPM_MODS_VALID(idx)) {
			pr_info("[name:mtk_lpm][P] - mod(%s) out of index (%d) (%s:%d)\n",
					p->data.info.name, idx,
					__func__, __LINE__);
			break;
		}
		if (!strcmp(p->data.info.name, drv->states[idx].name)) {
			ptr = &per_cpu(mtk_lpm_mods, cpu);
			ptr->mod[idx] = p->data.info.mod;
			break;
		}
	}
	return 0;
}

int mtk_lpm_notifier_register(struct notifier_block *n)
{
	return raw_notifier_chain_register(&mtk_lpm_notifier, n);
}
EXPORT_SYMBOL(mtk_lpm_notifier_register);

int mtk_lpm_notifier_unregister(struct notifier_block *n)
{
	return raw_notifier_chain_unregister(&mtk_lpm_notifier, n);
}
EXPORT_SYMBOL(mtk_lpm_notifier_unregister);

void mtk_lpm_system_spin_lock(unsigned long *irqflag)
{
	if (irqflag) {
		unsigned long flag;

		spin_lock_irqsave(&mtk_lpm_sys_locker, flag);
		*irqflag = flag;
	} else
		spin_lock(&mtk_lpm_sys_locker);
}
void mtk_lpm_system_spin_unlock(unsigned long *irqflag)
{
	if (irqflag) {
		unsigned long flag = *irqflag;

		spin_unlock_irqrestore(&mtk_lpm_sys_locker, flag);
	} else
		spin_unlock(&mtk_lpm_sys_locker);
}

int mtk_lpm_module_register_blockcall(int cpu, void *p)
{
	int ret = 0;
	struct mtk_lpm_module_reg *reg = (struct mtk_lpm_module_reg *)p;

	if (!reg || (reg->magic != MTK_LPM_MODULE_MAGIC)) {
		pr_info("[name:mtk_lpm][P] - registry(%d) fail (%s:%d)\n",
			reg ? reg->type : -1, __func__, __LINE__);
		return -EINVAL;
	}

	switch (reg->type) {
	case MTK_LPM_MODLE:
		if (reg->data.info.name)
			mtk_lpm_model_percpu_set(cpu, reg);
		break;
	case MTK_LPM_SYS_ISSUER:
		mtk_lpm_system.issuer =
			(struct mtk_lpm_issuer *)reg->data.issuer;
		break;
	case MTK_LPM_CPUIDLE_DRIVER:
		mtk_lpm_cpuidle_state_percpu_set(cpu, reg);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

int __mtk_lp_model_register(const char *name, struct mtk_lpm_model *lpm)
{
	struct mtk_lpm_module_reg reg = {
		.magic = MTK_LPM_MODULE_MAGIC,
		.type = MTK_LPM_MODLE,
		.data.info.name = name,
		.data.info.mod = lpm,
	};

	return mtk_lpm_do_work(MTK_LPM_REG_PER_CPU,
			       mtk_lpm_module_register_blockcall,
			       &reg);
}

int mtk_lp_model_register(const char *name, struct mtk_lpm_model *lpm)
{
	return __mtk_lp_model_register(name, lpm);
}
EXPORT_SYMBOL(mtk_lp_model_register);

int mtk_lp_model_unregister(const char *name)
{
	return __mtk_lp_model_register(name, NULL);
}
EXPORT_SYMBOL(mtk_lp_model_unregister);

int __mtk_lp_issuer_register(struct mtk_lpm_issuer *issuer)
{
	struct mtk_lpm_module_reg reg = {
		.magic = MTK_LPM_MODULE_MAGIC,
		.type = MTK_LPM_SYS_ISSUER,
		.data.issuer = issuer,
	};

	return mtk_lpm_do_work(MTK_LPM_REG_ALL_ONLINE,
			       mtk_lpm_module_register_blockcall,
			       &reg);
}

int mtk_lp_issuer_register(struct mtk_lpm_issuer *issuer)
{
	return __mtk_lp_issuer_register(issuer);
}
EXPORT_SYMBOL(mtk_lp_issuer_register);

int mtk_lp_issuer_unregister(struct mtk_lpm_issuer *issuer)
{
	if (issuer != mtk_lpm_system.issuer)
		return -EINVAL;
	return __mtk_lp_issuer_register(NULL);
}
EXPORT_SYMBOL(mtk_lp_issuer_unregister);

int mtk_lp_cpuidle_prepare(struct cpuidle_driver *drv, int index)
{
	struct mtk_lpm_models *lpmmods = NULL;
	struct mtk_lpm_model *lpm = NULL;
	struct mtk_lpm_nb_data nb_data;
	int prompt = 0;
	unsigned int model_flags = 0;
	unsigned long flags;
	const int cpuid = smp_processor_id();

	if (index < 0)
		return -1;

	lpmmods = this_cpu_ptr(&mtk_lpm_mods);

	if (lpmmods && lpmmods->mod[index])
		lpm = lpmmods->mod[index];

	model_flags = (lpm) ? lpm->flag : 0;

	nb_data.cpu = cpuid;
	nb_data.index = index;
	nb_data.model = lpm;
	nb_data.issuer = mtk_lpm_system.issuer;

	spin_lock_irqsave(&mtk_lp_mod_locker, flags);

	if (lpm && lpm->op.prompt)
		prompt = lpm->op.prompt(cpuid, nb_data.issuer);

	if (!unlikely(flags & MTK_LP_REQ_NOBROADCAST)) {
		prompt = mtk_lp_notify_var(MTK_LPM_NB_AFTER_PROMPT, prompt);
		mtk_lp_pm_notify(prompt, &nb_data);
	}

	spin_unlock_irqrestore(&mtk_lp_mod_locker, flags);

	if (lpm && lpm->op.prepare_enter)
		lpm->op.prepare_enter(prompt, cpuid, nb_data.issuer);

	mtk_lp_pm_notify(MTK_LPM_NB_PREPARE, &nb_data);

	return 0;
}

void mtk_lp_cpuidle_resume(struct cpuidle_driver *drv, int index)
{
	struct mtk_lpm_models *lpmmods = NULL;
	struct mtk_lpm_model *lpm = NULL;
	struct mtk_lpm_nb_data nb_data;
	unsigned int model_flags = 0;
	unsigned long flags;
	const int cpuid = smp_processor_id();

	if (index < 0)
		return;

	lpmmods = this_cpu_ptr(&mtk_lpm_mods);

	if (lpmmods && lpmmods->mod[index])
		lpm = lpmmods->mod[index];

	nb_data.cpu = cpuid;
	nb_data.index = index;
	nb_data.model = lpm;
	nb_data.issuer = mtk_lpm_system.issuer;

	model_flags = (lpm) ? lpm->flag : 0;

	mtk_lp_pm_notify(MTK_LPM_NB_RESUME, &nb_data);

	if (lpm && lpm->op.prepare_resume)
		lpm->op.prepare_resume(cpuid, nb_data.issuer);

	spin_lock_irqsave(&mtk_lp_mod_locker, flags);

	if (!unlikely(flags & MTK_LP_REQ_NOBROADCAST))
		mtk_lp_pm_notify(MTK_LPM_NB_BEFORE_REFLECT, &nb_data);

	if (lpm && lpm->op.reflect)
		lpm->op.reflect(cpuid, nb_data.issuer);

	spin_unlock_irqrestore(&mtk_lp_mod_locker, flags);
}

static int mtk_lpm_state_enter(int type, struct cpuidle_device *dev,
			   struct cpuidle_driver *drv, int idx)
{
	int ret;
	struct mtk_lpm_states_enter *cstate = this_cpu_ptr(&mtk_lpm_cstate);

	ret = mtk_lp_cpuidle_prepare(drv, idx);
	idx = ret ? 0 : idx;
	if (type == mtk_lpm_state_s2idle)
		cstate->s2idle[idx](dev, drv, idx);
	else
		ret = cstate->cpuidle[idx](dev, drv, idx);
	mtk_lp_cpuidle_resume(drv, idx);
	return ret;
}

int mtk_lpm_suspend_enter(void)
{
	int ret = 0;
	unsigned long flags;
	const int cpuid = smp_processor_id();

	if (mtk_lpm_system.suspend.flag & MTK_LP_REQ_NOSUSPEND)
		return -EACCES;

	spin_lock_irqsave(&mtk_lp_mod_locker, flags);
	if (mtk_lpm_system.suspend.op.prompt)
		ret = mtk_lpm_system.suspend.op.prompt(cpuid,
						       mtk_lpm_system.issuer);
	spin_unlock_irqrestore(&mtk_lp_mod_locker, flags);
	return ret;
}

void mtk_lpm_suspend_resume(void)
{
	unsigned long flags;
	const int cpuid = smp_processor_id();

	spin_lock_irqsave(&mtk_lp_mod_locker, flags);
	if (mtk_lpm_system.suspend.op.reflect)
		mtk_lpm_system.suspend.op.reflect(cpuid, mtk_lpm_system.issuer);
	spin_unlock_irqrestore(&mtk_lp_mod_locker, flags);
}

struct syscore_ops mtk_lpm_suspend = {
	.suspend = mtk_lpm_suspend_enter,
	.resume = mtk_lpm_suspend_resume,
};

int mtk_lpm_suspend_registry(const char *name, struct mtk_lpm_model *suspend)
{
	unsigned long flags;

	if (!suspend)
		return -EINVAL;

	if (mtk_lpm_system.suspend.flag &
			MTK_LP_REQ_NOSYSCORE_CB)
		mtk_lp_model_register(name, suspend);
	else {
		spin_lock_irqsave(&mtk_lp_mod_locker, flags);
		memcpy(&mtk_lpm_system.suspend, suspend,
				sizeof(struct mtk_lpm_model));
		spin_unlock_irqrestore(&mtk_lp_mod_locker, flags);
	}
	return 0;
}
EXPORT_SYMBOL(mtk_lpm_suspend_registry);

int mtk_lpm_suspend_type_get(void)
{
	return (mtk_lpm_system.suspend.flag & MTK_LP_REQ_NOSYSCORE_CB)
		? MTK_LPM_SUSPEND_S2IDLE : MTK_LPM_SUSPEND_SYSTEM;
}
EXPORT_SYMBOL(mtk_lpm_suspend_type_get);

static struct wakeup_source *mtk_lpm_lock;

#define SYS_SYNC_TIMEOUT 2000
#define SYS_SYNC_ONGOING 1
#define SYS_SYNC_DONE 0

static int sys_sync_ongoing;

static void suspend_sys_sync(struct work_struct *work);
static struct workqueue_struct *suspend_sys_sync_work_queue;
DECLARE_WORK(suspend_sys_sync_work, suspend_sys_sync);

static void suspend_sys_sync(struct work_struct *work)
{
	unsigned long flags;

	pr_debug("++\n");
	ksys_sync_helper();
	spin_lock_irqsave(&__lpm_sys_sync_lock, flags);
	sys_sync_ongoing = SYS_SYNC_DONE;
	spin_unlock_irqrestore(&__lpm_sys_sync_lock, flags);
	pr_debug("--\n");
}

int suspend_syssync_enqueue(void)
{
	unsigned long flags;

	if (sys_sync_ongoing != SYS_SYNC_DONE)
		return -EBUSY;
	if (suspend_sys_sync_work_queue == NULL) {
		suspend_sys_sync_work_queue =
			create_singlethread_workqueue("fs_suspend_syssync");
		if (suspend_sys_sync_work_queue == NULL) {
			pr_debug("fs_suspend_syssync workqueue create failed\n");
			return -EBUSY;
		}
	}
	spin_lock_irqsave(&__lpm_sys_sync_lock, flags);
	sys_sync_ongoing = SYS_SYNC_ONGOING;
	spin_unlock_irqrestore(&__lpm_sys_sync_lock, flags);
	pr_debug("PM: Syncing filesystems ... ");
	queue_work(suspend_sys_sync_work_queue, &suspend_sys_sync_work);

	return 0;
}

int suspend_syssync_check(void)
{
	int timeout = 10;
	int acc = 0;

	while (sys_sync_ongoing != SYS_SYNC_DONE) {
		msleep(timeout);
		if ((acc + 10) > SYS_SYNC_TIMEOUT) {
			pr_debug("Sync filesystems timeout");
			return -EBUSY;
		}
		acc += 10;
	}
	pr_debug("done.\n");
	return 0;
}

static int lpm_pm_early_event(struct notifier_block *notifier, unsigned long pm_event,
			void *unused)
{
	int ret;

	switch (pm_event) {
	case PM_HIBERNATION_PREPARE:
		return NOTIFY_DONE;
	case PM_RESTORE_PREPARE:
		return NOTIFY_DONE;
	case PM_POST_HIBERNATION:
		return NOTIFY_DONE;
	case PM_SUSPEND_PREPARE:
		ret = suspend_syssync_enqueue();
		if (ret < 0)
			return NOTIFY_BAD;
		return NOTIFY_DONE;
	case PM_POST_SUSPEND:
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static struct notifier_block lpm_pm_early_notifier_func = {
	.notifier_call = lpm_pm_early_event,
	.priority = 255,
};

static int lpm_pm_event(struct notifier_block *notifier, unsigned long pm_event,
			void *unused)
{
	int ret;

	switch (pm_event) {
	case PM_HIBERNATION_PREPARE:
		return NOTIFY_DONE;
	case PM_RESTORE_PREPARE:
		return NOTIFY_DONE;
	case PM_POST_HIBERNATION:
		return NOTIFY_DONE;
	case PM_SUSPEND_PREPARE:
		ret = suspend_syssync_check();
		if (ret < 0)
			return NOTIFY_BAD;
	/* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
		return NOTIFY_DONE;
	case PM_POST_SUSPEND:
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static struct notifier_block lpm_pm_notifier_func = {
	.notifier_call = lpm_pm_event,
	.priority = 0,
};


static int __init mtk_lpm_init(void)
{
	int ret;
	unsigned long flags;
	struct device_node *mtk_lpm;
	struct mtk_lpm_module_reg reg = {
		.magic = MTK_LPM_MODULE_MAGIC,
		.type = MTK_LPM_CPUIDLE_DRIVER,
		.data.fp.state = mtk_lpm_cpuidle_state_enter,
		.data.fp.s2idle = mtk_lpm_s2idle_state_enter
	};

	mtk_lpm_system.suspend.flag = MTK_LP_REQ_NOSUSPEND;

	mtk_lpm = of_find_compatible_node(NULL, NULL, MTK_LPM_DTS_COMPATIBLE);

	spin_lock_irqsave(&mtk_lp_mod_locker, flags);

	if (mtk_lpm) {
		const char *pMethod = NULL;

		of_property_read_string(mtk_lpm, "suspend-method", &pMethod);

		if (pMethod) {
			mtk_lpm_system.suspend.flag &=
						~MTK_LP_REQ_NOSUSPEND;

			if (!strcmp(pMethod, "enable")) {
				if (pm_suspend_default_s2idle()) {
					mtk_lpm_system.suspend.flag |=
					MTK_LP_REQ_NOSYSCORE_CB;
				} else {
					mtk_lpm_system.suspend.flag &=
					(~MTK_LP_REQ_NOSYSCORE_CB);
				}
			} else {
				mtk_lpm_system.suspend.flag |=
						MTK_LP_REQ_NOSUSPEND;
			}
			pr_info("[name:mtk_lpm][P] - suspend-method:%s (%s:%d)\n",
						pMethod, __func__, __LINE__);
		}
		of_node_put(mtk_lpm);
	}

	spin_unlock_irqrestore(&mtk_lp_mod_locker, flags);

	mtk_lpm_do_work(MTK_LPM_REG_PER_CPU,
			mtk_lpm_module_register_blockcall, &reg);

	if (mtk_lpm_system.suspend.flag & MTK_LP_REQ_NOSUSPEND) {
		mtk_lpm_lock = wakeup_source_register(NULL, "mtk_lpm_lock");
		if (!mtk_lpm_lock) {
			pr_info("[name:mtk_lpm][P] - initialize mtk_lpm_lock wakeup source fail\n");
			return -1;
		}
		__pm_stay_awake(mtk_lpm_lock);
		pr_info("[name:mtk_lpm][P] - device not support kernel suspend\n");
	}

	if (!(mtk_lpm_system.suspend.flag &
		MTK_LP_REQ_NOSYSCORE_CB))
		register_syscore_ops(&mtk_lpm_suspend);

	sys_sync_ongoing = SYS_SYNC_DONE;
	ret = register_pm_notifier(&lpm_pm_early_notifier_func);
	if (ret) {
		pr_debug("Failed to register PM early notifier.\n");
		return ret;
	}
	ret = 0;

	ret = register_pm_notifier(&lpm_pm_notifier_func);
	if (ret) {
		pr_debug("Failed to register PM notifier.\n");
		return ret;
	}

	mtk_lpm_platform_init();
	return 0;
}

#ifdef MTK_LPM_MODE_MODULE
static void __exit mtk_lpm_deinit(void)
{
}
module_init(mtk_lpm_init);
module_exit(mtk_lpm_deinit);
#else
device_initcall_sync(mtk_lpm_init);
#endif

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("mtk low power module");
MODULE_AUTHOR("MediaTek Inc.");

