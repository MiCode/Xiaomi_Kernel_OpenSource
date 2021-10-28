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

#include <lpm.h>
#include <lpm_internal.h>
#include <lpm_module.h>
#include <lpm_plat_common.h>
#include "lpm_registry.h"

#define LPM_MODULE_MAGIC	0xFCD03DC3
#define LPM_CPUIDLE_DRIVER	0xD0
#define LPM_MODLE		0xD1
#define LPM_SYS_ISSUER		0xD2

DEFINE_SPINLOCK(__lpm_sys_sync_lock);

typedef int (*state_enter)(struct cpuidle_device *dev,
			   struct cpuidle_driver *drv, int idx);
typedef int (*s2idle_enter)(struct cpuidle_device *dev,
			   struct cpuidle_driver *drv, int idx);


struct lpm_state_enter_fp {
	state_enter state;
	s2idle_enter s2idle;
};

struct lpm_model_info {
	const char *name;
	void *mod;
};

struct lpm_module_reg {
	int magic;
	int type;
	union {
		struct lpm_issuer *issuer;
		struct lpm_model_info info;
		struct lpm_state_enter_fp fp;
	} data;
};

struct mtk_lpm {
	struct lpm_model suspend;
	struct lpm_issuer *issuer;
};

static struct mtk_lpm lpm_system;

static DEFINE_SPINLOCK(lpm_mod_locker);
static DEFINE_SPINLOCK(lpm_sys_locker);
static RAW_NOTIFIER_HEAD(lpm_notifier);


struct lpm_models {
	struct lpm_model *mod[CPUIDLE_STATE_MAX];
};
static DEFINE_PER_CPU(struct lpm_models, lpm_mods);

struct lpm_states_enter {
	state_enter cpuidle[CPUIDLE_STATE_MAX];
	s2idle_enter s2idle[CPUIDLE_STATE_MAX];
};
static DEFINE_PER_CPU(struct lpm_states_enter, lpm_cstate);


#define lpm_pm_notify(_id, _data)\
		({raw_notifier_call_chain(&lpm_notifier,\
					  _id, _data); })



#define lpm_notify_var(action, var)\
		((var & LPM_NB_MASK) | action)


#define IS_LPM_MODS_VALID(idx)\
		(idx < CPUIDLE_STATE_MAX)

enum lpm_state_type {
	lpm_state_cpuidle,
	lpm_state_s2idle,
};

static int lpm_state_enter(int type, struct cpuidle_device *dev,
			     struct cpuidle_driver *drv, int idx);

static int lpm_s2idle_state_enter(struct cpuidle_device *dev,
			   struct cpuidle_driver *drv, int idx)
{
	return lpm_state_enter(lpm_state_s2idle, dev, drv, idx);
}

static int lpm_cpuidle_state_enter(struct cpuidle_device *dev,
			   struct cpuidle_driver *drv, int idx)
{
	return lpm_state_enter(lpm_state_cpuidle, dev, drv, idx);
}

static int lpm_cpuidle_state_percpu_set(int cpu, struct lpm_module_reg *p)
{
	struct lpm_state_enter_fp *fp = &p->data.fp;
	struct cpuidle_driver *drv;
	struct lpm_states_enter *ptr;
	int idx;

	drv = cpuidle_get_driver();

	if (!drv || !fp) {
		pr_info("[name:mtk_lpm][P] - cpuidle state register fail (%s:%d)\n",
					__func__, __LINE__);
		return -EINVAL;
	}

	for (idx = 1; idx < drv->state_count; ++idx) {
		if (!IS_LPM_MODS_VALID(idx)) {
			pr_info("[name:mtk_lpm][P] - mod(%s) out of index (%d) (%s:%d)\n",
					p->data.info.name, idx,
					__func__, __LINE__);
			break;
		}

		ptr = &per_cpu(lpm_cstate, cpu);
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

static int lpm_model_percpu_set(int cpu, struct lpm_module_reg *p)
{
	struct lpm_models *ptr;
	struct cpuidle_driver *drv;
	int idx;

	drv = cpuidle_get_driver();

	if (!drv) {
		pr_info("[name:mtk_lpm][P] - cpuidle drv is null (%s:%d)\n",
					__func__, __LINE__);
		return -EINVAL;
	}

	for (idx = 0; idx < drv->state_count; ++idx) {
		if (!IS_LPM_MODS_VALID(idx)) {
			pr_info("[name:mtk_lpm][P] - mod(%s) out of index (%d) (%s:%d)\n",
					p->data.info.name, idx,
					__func__, __LINE__);
			break;
		}
		if (!strcmp(p->data.info.name, drv->states[idx].name)) {
			ptr = &per_cpu(lpm_mods, cpu);
			ptr->mod[idx] = p->data.info.mod;
			break;
		}
	}
	return 0;
}

int lpm_notifier_register(struct notifier_block *n)
{
	return raw_notifier_chain_register(&lpm_notifier, n);
}
EXPORT_SYMBOL(lpm_notifier_register);

int lpm_notifier_unregister(struct notifier_block *n)
{
	return raw_notifier_chain_unregister(&lpm_notifier, n);
}
EXPORT_SYMBOL(lpm_notifier_unregister);

void lpm_system_spin_lock(unsigned long *irqflag)
{
	if (irqflag) {
		unsigned long flag;

		spin_lock_irqsave(&lpm_sys_locker, flag);
		*irqflag = flag;
	} else
		spin_lock(&lpm_sys_locker);
}
void lpm_system_spin_unlock(unsigned long *irqflag)
{
	if (irqflag) {
		unsigned long flag = *irqflag;

		spin_unlock_irqrestore(&lpm_sys_locker, flag);
	} else
		spin_unlock(&lpm_sys_locker);
}

static int lpm_module_register_blockcall(int cpu, void *p)
{
	int ret = 0;
	struct lpm_module_reg *reg = (struct lpm_module_reg *)p;

	if (!reg || (reg->magic != LPM_MODULE_MAGIC)) {
		pr_info("[name:mtk_lpm][P] - registry(%d) fail (%s:%d)\n",
			reg ? reg->type : -1, __func__, __LINE__);
		return -EINVAL;
	}

	switch (reg->type) {
	case LPM_MODLE:
		if (reg->data.info.name)
			lpm_model_percpu_set(cpu, reg);
		break;
	case LPM_SYS_ISSUER:
		lpm_system.issuer =
			(struct lpm_issuer *)reg->data.issuer;
		break;
	case LPM_CPUIDLE_DRIVER:
		lpm_cpuidle_state_percpu_set(cpu, reg);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int __lpm_model_register(const char *name, struct lpm_model *lpm)
{
	struct lpm_module_reg reg = {
		.magic = LPM_MODULE_MAGIC,
		.type = LPM_MODLE,
		.data.info.name = name,
		.data.info.mod = lpm,
	};

	return lpm_do_work(LPM_REG_PER_CPU,
			       lpm_module_register_blockcall,
			       &reg);
}

int lpm_model_register(const char *name, struct lpm_model *lpm)
{
	return __lpm_model_register(name, lpm);
}
EXPORT_SYMBOL(lpm_model_register);

int lpm_model_unregister(const char *name)
{
	return __lpm_model_register(name, NULL);
}
EXPORT_SYMBOL(lpm_model_unregister);

static int __lpm_issuer_register(struct lpm_issuer *issuer)
{
	struct lpm_module_reg reg = {
		.magic = LPM_MODULE_MAGIC,
		.type = LPM_SYS_ISSUER,
		.data.issuer = issuer,
	};

	return lpm_do_work(LPM_REG_ALL_ONLINE,
			       lpm_module_register_blockcall,
			       &reg);
}

int lpm_issuer_register(struct lpm_issuer *issuer)
{
	return __lpm_issuer_register(issuer);
}
EXPORT_SYMBOL(lpm_issuer_register);

int lpm_issuer_unregister(struct lpm_issuer *issuer)
{
	if (issuer != lpm_system.issuer)
		return -EINVAL;
	return __lpm_issuer_register(NULL);
}
EXPORT_SYMBOL(lpm_issuer_unregister);

static int lpm_cpuidle_prepare(struct cpuidle_driver *drv, int index)
{
	struct lpm_models *lpmmods = NULL;
	struct lpm_model *lpm = NULL;
	struct lpm_nb_data nb_data;
	int prompt = 0;
	unsigned int model_flags = 0;
	unsigned long flags;
	const int cpuid = smp_processor_id();

	if (index < 0)
		return -1;

	lpmmods = this_cpu_ptr(&lpm_mods);

	if (lpmmods && lpmmods->mod[index])
		lpm = lpmmods->mod[index];

	model_flags = (lpm) ? lpm->flag : 0;

	nb_data.cpu = cpuid;
	nb_data.index = index;
	nb_data.model = lpm;
	nb_data.issuer = lpm_system.issuer;

	rcu_idle_exit();

	spin_lock_irqsave(&lpm_mod_locker, flags);

	if (lpm && lpm->op.prompt)
		prompt = lpm->op.prompt(cpuid, nb_data.issuer);

	if (!unlikely(model_flags & LPM_REQ_NOBROADCAST)) {
		prompt = lpm_notify_var(LPM_NB_AFTER_PROMPT, prompt);
		lpm_pm_notify(prompt, &nb_data);
	}

	spin_unlock_irqrestore(&lpm_mod_locker, flags);

	if (lpm && lpm->op.prepare_enter)
		lpm->op.prepare_enter(prompt, cpuid, nb_data.issuer);

	lpm_pm_notify(LPM_NB_PREPARE, &nb_data);

	rcu_idle_enter();

	return 0;
}

static void lpm_cpuidle_resume(struct cpuidle_driver *drv, int index, int ret)
{
	struct lpm_models *lpmmods = NULL;
	struct lpm_model *lpm = NULL;
	struct lpm_nb_data nb_data;
	unsigned int model_flags = 0;
	unsigned long flags;
	const int cpuid = smp_processor_id();

	if (index < 0)
		return;

	lpmmods = this_cpu_ptr(&lpm_mods);

	if (lpmmods && lpmmods->mod[index])
		lpm = lpmmods->mod[index];

	nb_data.cpu = cpuid;
	nb_data.index = index;
	nb_data.model = lpm;
	nb_data.issuer = lpm_system.issuer;
	nb_data.ret = ret;

	model_flags = (lpm) ? lpm->flag : 0;

	rcu_idle_exit();

	lpm_pm_notify(LPM_NB_RESUME, &nb_data);

	if (lpm && lpm->op.prepare_resume)
		lpm->op.prepare_resume(cpuid, nb_data.issuer);

	spin_lock_irqsave(&lpm_mod_locker, flags);

	if (!unlikely(model_flags & LPM_REQ_NOBROADCAST))
		lpm_pm_notify(LPM_NB_BEFORE_REFLECT, &nb_data);

	if (lpm && lpm->op.reflect)
		lpm->op.reflect(cpuid, nb_data.issuer);

	spin_unlock_irqrestore(&lpm_mod_locker, flags);

	rcu_idle_enter();
}

static int lpm_state_enter(int type, struct cpuidle_device *dev,
			   struct cpuidle_driver *drv, int idx)
{
	int ret;
	struct lpm_states_enter *cstate = this_cpu_ptr(&lpm_cstate);

	ret = lpm_cpuidle_prepare(drv, idx);
	idx = ret ? 0 : idx;
	if (type == lpm_state_s2idle)
		ret = cstate->s2idle[idx](dev, drv, idx);
	else
		ret = cstate->cpuidle[idx](dev, drv, idx);
	lpm_cpuidle_resume(drv, idx, ret);

	return ret;
}

static int lpm_suspend_enter(void)
{
	int ret = 0;
	unsigned long flags;
	const int cpuid = smp_processor_id();

	if (lpm_system.suspend.flag & LPM_REQ_NOSUSPEND)
		return -EACCES;

	spin_lock_irqsave(&lpm_mod_locker, flags);
	if (lpm_system.suspend.op.prompt)
		ret = lpm_system.suspend.op.prompt(cpuid,
						       lpm_system.issuer);
	spin_unlock_irqrestore(&lpm_mod_locker, flags);
	return ret;
}

static void lpm_suspend_resume(void)
{
	unsigned long flags;
	const int cpuid = smp_processor_id();

	spin_lock_irqsave(&lpm_mod_locker, flags);
	if (lpm_system.suspend.op.reflect)
		lpm_system.suspend.op.reflect(cpuid, lpm_system.issuer);
	spin_unlock_irqrestore(&lpm_mod_locker, flags);
}

struct syscore_ops lpm_suspend = {
	.suspend = lpm_suspend_enter,
	.resume = lpm_suspend_resume,
};

int lpm_suspend_registry(const char *name, struct lpm_model *suspend)
{
	unsigned long flags;

	if (!suspend)
		return -EINVAL;

	if (lpm_system.suspend.flag &
			LPM_REQ_NOSYSCORE_CB)
		lpm_model_register(name, suspend);
	else {
		spin_lock_irqsave(&lpm_mod_locker, flags);
		memcpy(&lpm_system.suspend, suspend,
				sizeof(struct lpm_model));
		spin_unlock_irqrestore(&lpm_mod_locker, flags);
	}
	return 0;
}
EXPORT_SYMBOL(lpm_suspend_registry);

int lpm_suspend_type_get(void)
{
	return (lpm_system.suspend.flag & LPM_REQ_NOSYSCORE_CB)
		? LPM_SUSPEND_S2IDLE : LPM_SUSPEND_SYSTEM;
}
EXPORT_SYMBOL(lpm_suspend_type_get);

static struct wakeup_source *lpm_lock;

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

static int suspend_syssync_enqueue(void)
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

static int suspend_syssync_check(void)
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


static int __init lpm_init(void)
{
	int ret;
	unsigned long flags;
	struct device_node *lpm_node;
	struct lpm_module_reg reg = {
		.magic = LPM_MODULE_MAGIC,
		.type = LPM_CPUIDLE_DRIVER,
		.data.fp.state = lpm_cpuidle_state_enter,
		.data.fp.s2idle = lpm_s2idle_state_enter
	};

	lpm_system.suspend.flag = LPM_REQ_NOSUSPEND;

	lpm_node = of_find_compatible_node(NULL, NULL, MTK_LPM_DTS_COMPATIBLE);

	spin_lock_irqsave(&lpm_mod_locker, flags);

	if (lpm_node) {
		const char *pMethod = NULL;

		of_property_read_string(lpm_node, "suspend-method", &pMethod);

		if (pMethod) {
			lpm_system.suspend.flag &=
						~LPM_REQ_NOSUSPEND;

			if (!strcmp(pMethod, "enable")) {
				if (pm_suspend_default_s2idle()) {
					lpm_system.suspend.flag |=
					LPM_REQ_NOSYSCORE_CB;
				} else {
					lpm_system.suspend.flag &=
					(~LPM_REQ_NOSYSCORE_CB);
				}
			} else {
				lpm_system.suspend.flag |=
						LPM_REQ_NOSUSPEND;
			}
			pr_info("[name:mtk_lpm][P] - suspend-method:%s (%s:%d)\n",
						pMethod, __func__, __LINE__);
		}
		of_node_put(lpm_node);
	}

	spin_unlock_irqrestore(&lpm_mod_locker, flags);

	lpm_do_work(LPM_REG_PER_CPU,
			lpm_module_register_blockcall, &reg);

	if (lpm_system.suspend.flag & LPM_REQ_NOSUSPEND) {
		lpm_lock = wakeup_source_register(NULL, "lpm_lock");
		if (!lpm_lock) {
			pr_info("[name:mtk_lpm][P] - initialize lpm_lock wakeup source fail\n");
			return -1;
		}
		__pm_stay_awake(lpm_lock);
		pr_info("[name:mtk_lpm][P] - device not support kernel suspend\n");
	}

	if (!(lpm_system.suspend.flag &
		LPM_REQ_NOSYSCORE_CB))
		register_syscore_ops(&lpm_suspend);

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

	lpm_platform_init();
	return 0;
}

#ifdef MTK_LPM_MODE_MODULE
static void __exit lpm_deinit(void)
{
}

module_init(lpm_init);
module_exit(lpm_deinit);
#else
device_initcall_sync(lpm_init);
#endif

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("mtk low power module");
MODULE_AUTHOR("MediaTek Inc.");
