// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <asm/cpuidle.h>
#include <asm/suspend.h>
#include <linux/cpu_pm.h>
#include <linux/cpuidle.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/suspend.h>
#include <linux/syscore_ops.h>

#include <mtk_lpm.h>
#include "mtk_lpm_registry.h"

#define MTK_LPM_MODULE_MAGIC	0xFCD03DC3
#define MTK_LPM_MODLE			0xD1
#define MTK_LPM_SYS_ISSUER		0xD2

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


#define mtk_lp_pm_notify(_id, _data)\
			({raw_notifier_call_chain(\
				&mtk_lpm_notifier, _id, _data); })



#define mtk_lp_notify_var(action, var)\
		((var & MTK_LPM_NB_MASK) | action)


#define IS_MTK_LPM_MODS_VALID(idx)\
		(idx < CPUIDLE_STATE_MAX)


int mtk_lpm_model_percpu_set(int cpu, struct mtk_lpm_module_reg *p)
{
	struct mtk_lpm_models *ptr;
	struct cpuidle_driver *drv;
	struct cpuidle_device *dev;
	int idx;

	dev = cpuidle_get_device();
	drv = cpuidle_get_cpu_driver(dev);

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
			reg->type, __func__, __LINE__);
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

struct mtk_cpuidle_op mtk_lpm_cpu_pm_op = {
	.cpuidle_prepare = mtk_lp_cpuidle_prepare,
	.cpuidle_resume = mtk_lp_cpuidle_resume,
};

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

	spin_lock_irqsave(&mtk_lp_mod_locker, flags);

	if (mtk_lpm_system.suspend.flag &
			MTK_LP_REQ_NOSYSCORE_CB) {
		mtk_lp_model_register(name, suspend);
	} else
		memcpy(&mtk_lpm_system.suspend, suspend,
				sizeof(struct mtk_lpm_model));

	spin_unlock_irqrestore(&mtk_lp_mod_locker, flags);
	return 0;
}
EXPORT_SYMBOL(mtk_lpm_suspend_registry);

static struct wakeup_source *mtk_lpm_lock;

static int __init mtk_lpm_init(void)
{
	struct device_node *mtk_lpm;
	unsigned long flags;

	mtk_lpm_system.suspend.flag = MTK_LP_REQ_NOSUSPEND;

	mtk_lpm = of_find_compatible_node(NULL, NULL, MTK_LPM_DTS_COMPATIBLE);
	mtk_lpm_drv_cpuidle_ops_set(&mtk_lpm_cpu_pm_op);

	spin_lock_irqsave(&mtk_lp_mod_locker, flags);

	if (mtk_lpm) {
		const char *pMethod = NULL;

		of_property_read_string(mtk_lpm, "suspend-method", &pMethod);

		if (pMethod) {
			mtk_lpm_system.suspend.flag &=
						~MTK_LP_REQ_NOSUSPEND;

			if (!strcmp(pMethod, "s2idle"))
				mtk_lpm_system.suspend.flag |=
						MTK_LP_REQ_NOSYSCORE_CB;
			else if (!strcmp(pMethod, "system"))
				mtk_lpm_system.suspend.flag &=
						~MTK_LP_REQ_NOSYSCORE_CB;
			else
				mtk_lpm_system.suspend.flag |=
						MTK_LP_REQ_NOSUSPEND;

			pr_info("[name:mtk_lpm][P] - suspend-method:%s (%s:%d)\n",
						pMethod, __func__, __LINE__);
		}
		of_node_put(mtk_lpm);
	}

	spin_unlock_irqrestore(&mtk_lp_mod_locker, flags);

	if (mtk_lpm_system.suspend.flag & MTK_LP_REQ_NOSUSPEND) {
		mtk_lpm_lock = wakeup_source_register("mtk_lpm_lock");
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
	else
		mem_sleep_current = PM_SUSPEND_TO_IDLE;

	return 0;
}
device_initcall(mtk_lpm_init);

