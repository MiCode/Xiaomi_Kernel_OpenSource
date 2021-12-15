/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <mtk_idle_module.h>
#include <linux/printk.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/sched/clock.h>

#if defined(CONFIG_THERMAL)
#include <mtk_thermal.h> /* mtkTTimer_start/cancel_timer */
#endif
#include <mtk_idle_profile.h>
#include <linux/atomic.h>

DEFINE_MUTEX(mtk_idle_module_locker);
atomic_t mtk_idle_module_atomic = ATOMIC_INIT(0);


enum MTK_IDLE_MODULE_STATUS {
	MTK_IDLE_MODULE_ENABLE = 0,
	MTK_IDLE_MODULE_LOCKED,
	MTK_IDLE_RATIO_CAL_ENABLE,
};

extern int mtk8250_request_to_sleep(void);
extern int mtk8250_request_to_wakeup(void);
extern void mtk8250_backup_dev(void);
extern void mtk8250_restore_dev(void);

/* Sometimes the idle type's sequence won't be same as
 *  the selection's sequence, so we need map to idle model
 *  from idle type to idle selection index.
 */
struct MTK_IDLE_MODEL_MAP {
	unsigned int valid;
	struct MTK_IDLE_MODEL *mods[MTK_IDLE_MODULE_MODEL_MAX];
};

struct MTK_IDLE_MODULE_INTERNAL {
	unsigned int status;
	unsigned int valid;
	struct MTK_IDLE_MODEL_MAP mod_map;
	struct MTK_IDLE_MODULE *module;
};

static struct MTK_IDLE_MODULE_INTERNAL g_mtk_idle_module = {
	.status = 0,
	.valid = 0,
	.mod_map = {
		.valid = 0,
	},
	.module = NULL,
};

#define Get_next_module_model(_mod, num)\
	({ do {\
		_mod = g_mtk_idle_module.module->models + num;\
		if (*_mod &&\
			((g_mtk_idle_module.valid & (1<<num)) == 0))\
			num++;\
		else\
			break;\
	} while (1); _mod; })

#define foreach_mtk_idle_module_model(_mod, num, _start)\
	for (num = _start, _mod = Get_next_module_model(_mod, num);\
		*_mod; num++, _mod = Get_next_module_model(_mod, num))

#define foreach_mtk_idle_models(_mod, num, _start)\
	for (_mod = g_mtk_idle_module.module->models, num = _start;\
		*_mod ; _mod++, num++)

/* Mtk idle module status control */
#define MTK_IDLE_MODULE_STATUS_SET(x)\
	(g_mtk_idle_module.status |= (1<<x))

#define MTK_IDLE_MODULE_STATUS_CLEAN(x)\
	(g_mtk_idle_module.status &= ~(1<<x))

#define IS_MTK_IDLE_MODULE_STATUS(x)\
	(g_mtk_idle_module.status & (1<<x))

#define IS_MTK_IDLE_MODEL_TYPE_VALID(_type)\
	(g_mtk_idle_module.mod_map.valid & (1<<_type))

#define MTK_IDLE_MODEL_PTR(_type)\
	g_mtk_idle_module.mod_map.mods[_type]

/* Mtk idle model clerk status clear control */
#define MTK_IDLE_MODEL_CLR_SET(_mod, _clr)\
	(_mod->clerk.status.clr |= 1<<_clr)

#define MTK_IDLE_MODEL_CLR_CLEAR(_mod, _clr)\
	(_mod->clerk.status.clr &= ~(1<<_clr))

#define IS_MTK_IDLE_MODEL_CLR(_mod, _clr)\
	((_mod->clerk.status.clr & (1<<_clr)))

#define MTK_IDLE_MODEL_ENTER(_cpu, _mod) do {\
	u64 time_start = 0;\
	if (!_mod->policy.enter)\
		break;\
	_mod->clerk.status.cnt.enter[_cpu] += 1;\
	if (IS_MTK_IDLE_MODULE_STATUS(MTK_IDLE_RATIO_CAL_ENABLE)) {\
		if (IS_MTK_IDLE_MODEL_CLR(_mod\
				, MTK_IDLE_MODEL_CLR_RESIDENCY)) {\
			_mod->clerk.status.residency_ms = 0;\
			mutex_lock(&mtk_idle_module_locker);\
			MTK_IDLE_MODEL_CLR_CLEAR(_mod\
				, MTK_IDLE_MODEL_CLR_RESIDENCY);\
			mutex_unlock(&mtk_idle_module_locker);\
		} \
		mtk_idle_recent_ratio_calc_start_plat(_mod->clerk.type);\
		time_start = sched_clock(); } \
	_mod->policy.enter(_cpu);\
	if (IS_MTK_IDLE_MODULE_STATUS(MTK_IDLE_RATIO_CAL_ENABLE)) {\
		time_start = sched_clock() - time_start;\
		do_div(time_start, 1000000);\
		mtk_idle_recent_ratio_calc_stop_plat(_mod->clerk.type);\
		_mod->clerk.status.residency_ms += time_start; } \
	} while (0)


int __attribute__((weak))
mtk_idle_recent_ratio_calc_start_plat(int IdleType)
{
	return MTK_IDLE_MOD_FAIL;
}
int __attribute__((weak))
mtk_idle_recent_ratio_calc_stop_plat(int IdleType)
{
	return MTK_IDLE_MOD_FAIL;
}
int __attribute__((weak))
mtk_idle_mod_recent_ratio_get_plat(
	struct mtk_idle_ratio_recent_info *feed, int *win_ms)
{
	return MTK_IDLE_MOD_FAIL;
}


/* Migrate from mtk_idle_internal.c */
/* --------------------------------------
 *   mtk idle scenario footprint definitions
 *  --------------------------------------
 */
enum idle_fp_step {
	IDLE_FP_ENTER = 0x1,
	IDLE_FP_PREHANDLER = 0x3,
	IDLE_FP_PCM_SETUP = 0x7,
	IDLE_FP_PWR_PRE_SYNC = 0xf,
	IDLE_FP_UART_SLEEP = 0x1f,
	IDLE_FP_ENTER_WFI = 0xff,
	IDLE_FP_LEAVE_WFI = 0x1ff,
	IDLE_FP_UART_RESUME = 0x3ff,
	IDLE_FP_PCM_CLEANUP = 0x7ff,
	IDLE_FP_POSTHANDLER = 0xfff,
	IDLE_FP_PWR_POST_SYNC = 0x1fff,
	IDLE_FP_LEAVE = 0xffff,
	IDLE_FP_SLEEP_DEEPIDLE = 0x80000000,
};

#ifdef CONFIG_MTK_RAM_CONSOLE
#define __mtk_idle_footprint_start(idle_type)\
	aee_rr_rec_cidle_model_val((smp_processor_id() << 24)\
		| idle_type)

#define __mtk_idle_footprint(value)\
	aee_rr_rec_cidle_data_val(value)

#define __mtk_idle_footprint_stop() do {\
	aee_rr_rec_cidle_model_val(0); aee_rr_rec_cidle_data_val(0);\
	} while (0)

#else /* CONFIG_MTK_RAM_CONSOLE */
#define __mtk_idle_footprint_start(idle_type)
#define __mtk_idle_footprint(value)
#define __mtk_idle_footprint_stop()
#endif /* CONFIG_MTK_RAM_CONSOLE */


/************************************************************
 * Weak functions for chip dependent flow.
 ************************************************************/
/* [ByChip] Internal weak functions: implemented in mtk_spm_idle.c */
int __attribute__((weak)) mtk_idle_trigger_wfi(
	int idle_type, unsigned int idle_flag, int cpu)
{
	printk_deferred("[name:spm&]Power/swap %s is not implemented!\n"
		, __func__);

	do {
		isb();
		mb();	/* memory barrier */
		__asm__ __volatile__("wfi" : : : "memory");
	} while (0);

	return 0;
}

void __attribute__((weak)) mtk_idle_pre_process_by_chip(
	int idle_type, int cpu, unsigned int op_cond, unsigned int idle_flag) {}

void __attribute__((weak)) mtk_idle_post_process_by_chip(
	int idle_type, int cpu, unsigned int op_cond, unsigned int idle_flag) {}

bool __attribute__((weak)) mtk_idle_cond_vcore_lp_mode(int idle_type)
{
	return false;
}

/* [ByChip] internal weak functions: implmented in mtk_spm_power.c */
void __attribute__((weak)) mtk_idle_power_pre_process(
	int idle_type, unsigned int op_cond) {}

void __attribute__((weak)) mtk_idle_power_pre_process_async_wait(
	int idle_type, unsigned int op_cond) {}

void __attribute__((weak)) mtk_idle_power_post_process(
	int idle_type, unsigned int op_cond) {}

void __attribute__((weak)) mtk_idle_power_post_process_async_wait(
	int idle_type, unsigned int op_cond) {}


/***********************************************************
 * local functions
 ***********************************************************/
static RAW_NOTIFIER_HEAD(mtk_idle_notifier);
int mtk_idle_module_notifier_register(
			struct notifier_block *n)
{
	return raw_notifier_chain_register(
						&mtk_idle_notifier, n);
}
int mtk_idle_module_notifier_unregister(
			struct notifier_block *n)
{
	raw_notifier_chain_unregister(&mtk_idle_notifier, n);
	return MTK_IDLE_MOD_OK;
}
int mtk_idle_module_notifier_call_chain(unsigned long notifyID)
{
	int bRet = MTK_IDLE_MOD_OK;

	if ((notifyID & MTK_IDLE_MAINPLL_OFF) != 0)
		raw_notifier_call_chain(&mtk_idle_notifier
				, IDLE_NOTIFY_MAINPLL_OFF, NULL);
	else if ((notifyID & MTK_IDLE_MAINPLL_ON) != 0)
		raw_notifier_call_chain(&mtk_idle_notifier
				, IDLE_NOTIFY_MAINPLL_ON, NULL);
	else
		bRet = MTK_IDLE_MOD_FAIL;

	return bRet;
}

static unsigned int mtk_idle_pre_handler(int idle_type, unsigned long notify)
{
	unsigned int op_cond = 0;

	/* notify mtk idle enter */
	mtk_idle_module_notifier_call_chain((unsigned long)notify);

	#if defined(CONFIG_THERMAL) && !defined(CONFIG_FPGA_EARLY_PORTING)
	/* cancel thermal hrtimer for power saving */
	mtkTTimer_cancel_timer();
	#endif

	/* check ufs */
	op_cond |= ufs_cb_before_idle();

	/* check vcore voltage config */
	op_cond |= (mtk_idle_cond_vcore_lp_mode(idle_type) ?
		MTK_IDLE_OPT_VCORE_LP_MODE : 0);

	return op_cond;
}

static void mtk_idle_post_handler(int idle_type, unsigned long notify)
{
	#if defined(CONFIG_THERMAL) && !defined(CONFIG_FPGA_EARLY_PORTING)
	/* restart thermal hrtimer for update temp info */
	mtkTTimer_start_timer();
	#endif

	ufs_cb_after_idle();

	/* notify mtk idle leave */
	mtk_idle_module_notifier_call_chain((unsigned long)notify);
}

int mtk_idle_enter(int idle_type,
	int cpu, unsigned int op_cond, unsigned int idle_flag)
{
	struct MTK_IDLE_MODEL *mod;

	if (idle_type < 0)
		return -1;

	mod = IS_MTK_IDLE_MODEL_TYPE_VALID(idle_type) ?
			MTK_IDLE_MODEL_PTR(idle_type) : NULL;

	if (unlikely(!mod))
		return -1;

	__mtk_idle_footprint_start(idle_type);

	/* Disable log when we profiling idle latency */
	if (mtk_idle_latency_profile_is_on())
		idle_flag |= MTK_IDLE_LOG_DISABLE;

	__mtk_idle_footprint(IDLE_FP_ENTER);

	__profile_idle_stop(PIDX_SELECT_TO_ENTER);

	__profile_idle_start(PIDX_ENTER_TOTAL);

	/* Disable rcu lock checking */
	rcu_irq_enter_irqson();

	/* idle pre handler: setup notification/thermal/ufs */
	__profile_idle_start(PIDX_PRE_HANDLER);
	op_cond |= mtk_idle_pre_handler(idle_type, mod->notify.id_enter);
	__profile_idle_stop(PIDX_PRE_HANDLER);

	__mtk_idle_footprint(IDLE_FP_PREHANDLER);

	/* [by_chip] pre power setting: setup sleep voltage and power mode */
	__profile_idle_start(PIDX_PWR_PRE_WFI);
	mtk_idle_power_pre_process(idle_type, op_cond);
	__profile_idle_stop(PIDX_PWR_PRE_WFI);

	/* [by_chip] spm setup */
	__profile_idle_start(PIDX_SPM_PRE_WFI);
	mtk_idle_pre_process_by_chip(idle_type, cpu, op_cond, idle_flag);
	__profile_idle_stop(PIDX_SPM_PRE_WFI);

	__mtk_idle_footprint(IDLE_FP_PCM_SETUP);

	/* [by_chip] pre power setting sync wait */
	__profile_idle_start(PIDX_PWR_PRE_WFI_WAIT);
	mtk_idle_power_pre_process_async_wait(idle_type, op_cond);
	__profile_idle_stop(PIDX_PWR_PRE_WFI_WAIT);

	__mtk_idle_footprint(IDLE_FP_PWR_PRE_SYNC);

	/* uart sleep */
	#if defined(CONFIG_SERIAL_8250_MT6577)
	if (!(idle_flag & MTK_IDLE_LOG_DUMP_LP_GS)) {
		if (mtk8250_request_to_sleep()) {
			printk_deferred("[name:spm&]Power/swap Fail to request uart sleep\n");
			goto RESTORE_UART;
		}
	}
	#endif

	__mtk_idle_footprint(IDLE_FP_UART_SLEEP);

	__mtk_idle_footprint(IDLE_FP_ENTER_WFI);

	/* [by_chip] enter cpuidle driver for wfi */
	__profile_idle_stop(PIDX_ENTER_TOTAL);
	mtk_idle_trigger_wfi(idle_type, idle_flag, cpu);
	__profile_idle_start(PIDX_LEAVE_TOTAL);

	__mtk_idle_footprint(IDLE_FP_LEAVE_WFI);

	/* uart resume */
	#if defined(CONFIG_SERIAL_8250_MT6577)
	if (!(idle_flag & MTK_IDLE_LOG_DUMP_LP_GS))
		mtk8250_request_to_wakeup();
RESTORE_UART:
	#endif

	__mtk_idle_footprint(IDLE_FP_UART_RESUME);

	/* [by_chip] post power setting: restore  */
	__profile_idle_start(PIDX_PWR_POST_WFI);
	mtk_idle_power_post_process(idle_type, op_cond);
	__profile_idle_stop(PIDX_PWR_POST_WFI);

	/* [by_chip] spm clean up */
	__profile_idle_start(PIDX_SPM_POST_WFI);
	mtk_idle_post_process_by_chip(idle_type, cpu, op_cond, idle_flag);
	__profile_idle_stop(PIDX_SPM_POST_WFI);

	__mtk_idle_footprint(IDLE_FP_PCM_CLEANUP);

	/* idle post handler: setup notification/thermal/ufs */
	__profile_idle_start(PIDX_POST_HANDLER);
	mtk_idle_post_handler(idle_type, mod->notify.id_leave);
	__profile_idle_stop(PIDX_POST_HANDLER);

	__mtk_idle_footprint(IDLE_FP_POSTHANDLER);

	/* [by_chip] post power setting sync wait */
	__profile_idle_start(PIDX_PWR_POST_WFI_WAIT);
	mtk_idle_power_post_process_async_wait(idle_type, op_cond);
	__profile_idle_stop(PIDX_PWR_POST_WFI_WAIT);

	/* Eable rcu lock checking */
	rcu_irq_exit_irqson();

	__mtk_idle_footprint(IDLE_FP_PWR_POST_SYNC);

	__profile_idle_stop(PIDX_LEAVE_TOTAL);

	__mtk_idle_footprint(IDLE_FP_LEAVE);

	/* output idle latency profiling result if enabled */
	mtk_idle_latency_profile_result(&mod->clerk);

	__mtk_idle_footprint_stop();

	return 0;
}

/* Migrate from mtk_idle_profile.c */
#define idle_block_log_time_criteria	5000
static DEFINE_SPINLOCK(idle_blocking_spin_lock);
static unsigned long long idle_block_log_prev_time;
bool mtk_idle_select_state(int type, int reason)
{
	struct MTK_IDLE_MODEL *mod;
	u64 curr_time;
	unsigned long flags;
	bool dump_block_info;

	if (type < 0)
		return false;

	mod = IS_MTK_IDLE_MODEL_TYPE_VALID(type) ?
			MTK_IDLE_MODEL_PTR(type) : NULL;


	if (unlikely(!mod))
		return reason == NR_REASONS;

	curr_time = sched_clock();
	do_div(curr_time, 1000000);

	if (reason >= NR_REASONS) {
		mod->clerk.status.prev_time = curr_time;
		return true;
	}

	if (mod->clerk.status.prev_time == 0)
		mod->clerk.status.prev_time = curr_time;

	spin_lock_irqsave(&idle_blocking_spin_lock, flags);
	dump_block_info	=
		((curr_time - mod->clerk.status.prev_time) >
			mod->clerk.time_critera)
		&& ((curr_time - idle_block_log_prev_time) >
		idle_block_log_time_criteria);

	if (dump_block_info) {
		mod->clerk.status.prev_time = curr_time;
		idle_block_log_prev_time = curr_time;
	}
	spin_unlock_irqrestore(&idle_blocking_spin_lock, flags);

	if (dump_block_info) {
		mtk_idle_block_reason_report(&mod->clerk);

		memset(&mod->clerk.status.cnt.block, 0,
			NR_REASONS * sizeof(mod->clerk.status.cnt.block[0]));
	}
	mod->clerk.status.cnt.block[reason]++;
	return false;
}

/* mtk idle module */
int mtk_idle_module_enabled(void)
{
	if (!IS_MTK_IDLE_MODULE_STATUS(MTK_IDLE_MODULE_ENABLE))
		return MTK_IDLE_MOD_FAIL;
	return MTK_IDLE_MOD_OK;
}

int mtk_idle_model_count_get(int IdleModelType
				, struct MTK_IDLE_MODEL_COUNTER *cnt)
{
	int bRet = MTK_IDLE_MOD_FAIL;

	if (IdleModelType < 0)
		return MTK_IDLE_MOD_FAIL;

	atomic_inc(&mtk_idle_module_atomic);

	if (IS_MTK_IDLE_MODULE_STATUS(MTK_IDLE_MODULE_ENABLE)
		&& IS_MTK_IDLE_MODEL_TYPE_VALID(IdleModelType)
		&& cnt
	) {
		struct MTK_IDLE_MODEL *model = NULL;

		model = MTK_IDLE_MODEL_PTR(IdleModelType);
		memcpy(cnt, &model->clerk.status.cnt
			, sizeof(struct MTK_IDLE_MODEL_COUNTER));
		bRet = MTK_IDLE_MOD_OK;
	}
	atomic_dec(&mtk_idle_module_atomic);

	return bRet;
}

size_t mtk_idle_module_info_dump_count(char *buf, size_t len)
{
	struct MTK_IDLE_MODEL **mod = NULL;
	size_t mSize = 0;
	int i = 0, idx = 0;
	unsigned long cur_cnt = 0, total_cnt = 0;

	foreach_mtk_idle_module_model(mod, idx, 0) {
		mSize += scnprintf(buf + mSize, len - mSize,
				"%s: ", (*mod)->clerk.name);

		for (i = 0; i < nr_cpu_ids; i++) {
			if ((len - mSize) <= 0)
				break;

			cur_cnt = ((*mod)->clerk.status.cnt.enter[i]
				- (*mod)->clerk.status.cnt.prev_enter[i]);

			if (cur_cnt > 0)
				mSize += scnprintf(buf + mSize, len - mSize,
					"[%d] = %lu, ", i, cur_cnt);

			total_cnt += cur_cnt;
			(*mod)->clerk.status.cnt.prev_enter[i] =
					(*mod)->clerk.status.cnt.enter[i];
		}

		if ((len - mSize) > 0) {
			if (total_cnt > 0)
				mSize += scnprintf(buf + mSize, len - mSize,
						"Total = %lu, --- ", total_cnt);
			else
				mSize += scnprintf(buf + mSize, len - mSize,
						"No enter --- ");
		}
	}

	return mSize;
}
size_t mtk_idle_module_info_dump_ratio(char *buf, size_t len)
{
	struct MTK_IDLE_MODEL **mod = NULL;
	size_t mSize = 0;
	int idx = 0;

	foreach_mtk_idle_module_model(mod, idx, 0) {
		if ((len - mSize) <= 0)
			break;

		if (!IS_MTK_IDLE_MODEL_CLR((*mod)
				, MTK_IDLE_MODEL_CLR_RESIDENCY)) {

			mSize += scnprintf(buf + mSize, len - mSize
					, "%s = %llu, "
					, (*mod)->clerk.name
					, (*mod)->clerk.status.residency_ms);
			MTK_IDLE_MODEL_CLR_SET((*mod)
				, MTK_IDLE_MODEL_CLR_RESIDENCY);
		}
	}
	return mSize;
}

size_t mtk_idle_module_info_dump_idle_state(
				char *buf, size_t len)
{
	struct MTK_IDLE_MODEL **mod = NULL;
	size_t mSize = 0;
	int i = 0, idx = 0, nm_idx = 0;

	for (i = 0; i < nr_cpu_ids; i++) {
		if ((len - mSize) <= 0)
			break;
		mSize += scnprintf(buf + mSize, len - mSize
					, "cpu%d: ", i);

		nm_idx = 0;
		foreach_mtk_idle_module_model(mod, idx, 0) {
			if ((len - mSize) <= 0)
				break;

			mSize += scnprintf(buf + mSize, len - mSize,
					(nm_idx == 0) ?  "%s=%lu":", %s=%lu",
					(*mod)->clerk.name,
					(*mod)->clerk.status.cnt.enter[i]);
			nm_idx++;
		}
		mSize += scnprintf(buf + mSize, len - mSize, "\n");
	}
	return mSize;
}

size_t mtk_idle_module_info_dump_idle_enabled(
			char *buf, size_t len)
{
	struct MTK_IDLE_MODEL **mod = NULL;
	size_t mSize = 0;
	int idx = 0, nm_idx = 0;

	foreach_mtk_idle_module_model(mod, idx, 0) {
		if ((len - mSize) <= 0)
			break;
		mSize += scnprintf(buf + mSize, len - mSize,
				(nm_idx == 0) ? "%s=(%d)" : "-> %s=(%d)",
				(*mod)->clerk.name,
				(*mod)->policy.enabled());
		nm_idx++;
	}

	return mSize;
}

int mtk_idle_module_info_dump_locked(
			int info_type, char *buf, size_t len)
{
	size_t mSize = 0;

	if (!buf || len <= 0)
		return 0;

	mutex_lock(&mtk_idle_module_locker);
	mSize = mtk_idle_module_info_dump(info_type, buf, len);
	mutex_unlock(&mtk_idle_module_locker);

	return mSize;
}

int mtk_idle_module_info_dump(int info_type, char *buf, size_t len)
{
	size_t mSize = 0;

	if (!buf || len <= 0)
		return 0;

	if (!IS_MTK_IDLE_MODULE_STATUS(MTK_IDLE_MODULE_ENABLE))
		return 0;

	switch (info_type) {
	case MTK_IDLE_MODULE_INFO_COUNT:
		mSize = mtk_idle_module_info_dump_count(buf, len);
		break;
	case MTK_IDLE_MODULE_INFO_RATIO:
		mSize = mtk_idle_module_info_dump_ratio(buf, len);
		break;
	case MTK_IDLE_MODULE_INFO_IDLE_STATE:
		mSize = mtk_idle_module_info_dump_idle_state(buf, len);
		break;
	case MTK_IDLE_MODULE_INFO_IDLE_ENABLED:
		mSize = mtk_idle_module_info_dump_idle_enabled(buf, len);
		break;
	default:
		break;
	}

	return mSize;
}

int mtk_idle_module_enter(struct mtk_idle_info *info
		, int reason, int *IdleModelType)
{
	struct MTK_IDLE_MODEL **mod = NULL;
	int u64_local = 0;
	int bRet = MTK_IDLE_MOD_FAIL;

	if (!IS_MTK_IDLE_MODULE_STATUS(MTK_IDLE_MODULE_ENABLE))
		return bRet;

	atomic_inc(&mtk_idle_module_atomic);

	foreach_mtk_idle_module_model(mod, u64_local, 0) {
		if ((*mod)->policy.can_enter &&
			(*mod)->policy.enter &&
			(*mod)->policy.can_enter(reason, info)
		) {
			bRet = MTK_IDLE_MOD_OK;
			break;
		}
	}

	if (bRet == MTK_IDLE_MOD_OK) {
		int cpuId = (info) ? info->cpu : 0;

		MTK_IDLE_MODEL_ENTER(cpuId, (*mod));

		if (IdleModelType)
			*IdleModelType = (*mod)->clerk.type;
	}
	atomic_dec(&mtk_idle_module_atomic);

	return bRet;
}


int mtk_idle_module_model_sel(struct mtk_idle_info *info
		, int reason, int *IdleModelType)
{
	struct MTK_IDLE_MODEL **mod = NULL;
	int Idle_Idx = 0;
	int bRet = MTK_IDLE_MOD_FAIL;

	if (!IS_MTK_IDLE_MODULE_STATUS(MTK_IDLE_MODULE_ENABLE))
		return bRet;

	atomic_inc(&mtk_idle_module_atomic);

	foreach_mtk_idle_module_model(mod, Idle_Idx, 0) {
		if ((*mod)->policy.can_enter &&
			(*mod)->policy.enter &&
			(*mod)->policy.can_enter(reason, info)
		) {
			bRet = MTK_IDLE_MOD_OK;
			break;
		}
	}

	if (bRet == MTK_IDLE_MOD_OK) {
		if (IdleModelType)
			*IdleModelType = (*mod)->clerk.type;
	}
	atomic_dec(&mtk_idle_module_atomic);

	return bRet;
}

int mtk_idle_module_model_enter(int IdleModelType, int cpuId)
{
	int bRet = MTK_IDLE_MOD_FAIL;

	if ((IdleModelType < 0) ||
	    !IS_MTK_IDLE_MODULE_STATUS(MTK_IDLE_MODULE_ENABLE))
		return bRet;

	atomic_inc(&mtk_idle_module_atomic);

	if (IS_MTK_IDLE_MODEL_TYPE_VALID(IdleModelType)) {
		MTK_IDLE_MODEL_ENTER(cpuId,
			MTK_IDLE_MODEL_PTR(IdleModelType));
		bRet = MTK_IDLE_MOD_OK;
	}
	atomic_dec(&mtk_idle_module_atomic);

	return bRet;
}

int mtk_idle_module_feature(int feature, int enabled)
{
	if (!IS_MTK_IDLE_MODULE_STATUS(MTK_IDLE_MODULE_ENABLE))
		return MTK_IDLE_MOD_FAIL;

	mutex_lock(&mtk_idle_module_locker);
	switch (feature) {
	case MTK_IDLE_MODULE_RATIO_CAL:
		if (enabled) {
			struct MTK_IDLE_MODEL **mod;
			int idx = 0;

			foreach_mtk_idle_module_model(mod, idx, 0) {
				MTK_IDLE_MODEL_CLR_SET((*mod)
					, MTK_IDLE_MODEL_CLR_RESIDENCY);
			}
			MTK_IDLE_MODULE_STATUS_SET(MTK_IDLE_RATIO_CAL_ENABLE);
		} else
			MTK_IDLE_MODULE_STATUS_CLEAN(MTK_IDLE_RATIO_CAL_ENABLE);
		break;
	default:
		break;
	}
	mutex_unlock(&mtk_idle_module_locker);
	return MTK_IDLE_MOD_OK;
}

const char *mtk_idle_module_get_mod_name(int IdleModelType)
{
	if (IdleModelType < 0)
		return "unknown";

	return IS_MTK_IDLE_MODEL_TYPE_VALID(IdleModelType) ?
		MTK_IDLE_MODEL_PTR(IdleModelType)->clerk.name : "unknown";
}

int mtk_idle_model_notify(int idle_type, struct MTK_IDLE_MODEL_NOTE *note)
{
	int bRet = MTK_IDLE_MOD_FAIL;

	if (idle_type < 0)
		return MTK_IDLE_MOD_FAIL;

	if (note && IS_MTK_IDLE_MODEL_TYPE_VALID(idle_type)) {
		struct MTK_IDLE_MODEL *mod = MTK_IDLE_MODEL_PTR(idle_type);

		if (mod->policy.receiver) {
			mod->policy.receiver(note);
			bRet = MTK_IDLE_MOD_OK;
		}
	}

	return bRet;
}

size_t mtk_idle_module_get_helper(char *buf, size_t sz)
{
	size_t bSz = 0;

	if (!IS_MTK_IDLE_MODULE_STATUS(MTK_IDLE_MODULE_ENABLE))
		return bSz;

	atomic_inc(&mtk_idle_module_atomic);
	if (g_mtk_idle_module.module &&
		g_mtk_idle_module.module->reg.get_helper_info
	) {
		bSz = g_mtk_idle_module.module->reg.get_helper_info(
			buf, sz);
	}
	atomic_dec(&mtk_idle_module_atomic);

	return bSz;
}

size_t mtk_idle_module_switch_support(char *buf, size_t len)
{
	struct MTK_IDLE_MODULE *m = g_mtk_idle_module.module;

	if (!buf || len == 0)
		return 0;

	if (m && m->func.model_switch_support)
		return m->func.model_switch_support(buf, len);

	return scnprintf(buf, len, "Not support");
}

int mtk_idle_module_switch(int ModuleType)
{
	int bRet = MTK_IDLE_MOD_FAIL;
	struct MTK_IDLE_MODULE *m = g_mtk_idle_module.module;

	if (m && m->func.model_switch)
		bRet = m->func.model_switch(ModuleType);

	return bRet;
}

int mtk_idle_module_unregister(struct MTK_IDLE_MODULE *module)
{
	#define ATOMIC_WAIT_TIME_US		100
	#define ATOMIC_WAIT_COUNT_MAX	20
	#define ATOMIC_WAIT_FINE		0xffadca00

	int bRet = MTK_IDLE_MOD_FAIL;
	int Blk_cnt = 0;
	struct MTK_IDLE_MODEL **model = NULL;

	mutex_lock(&mtk_idle_module_locker);
	do {
		if (g_mtk_idle_module.module == module) {

			MTK_IDLE_MODULE_STATUS_CLEAN(MTK_IDLE_MODULE_ENABLE);
			while (1) {
				if (atomic_read(&mtk_idle_module_atomic) == 0) {
					Blk_cnt = ATOMIC_WAIT_FINE;
					break;
				}

				if (Blk_cnt > ATOMIC_WAIT_COUNT_MAX)
					break;
				Blk_cnt++;

				udelay(ATOMIC_WAIT_TIME_US);
			}

			if (Blk_cnt != ATOMIC_WAIT_FINE)
				break;

			foreach_mtk_idle_models(model, Blk_cnt, 0) {
				if ((*model)->policy.deinit)
					(*model)->policy.deinit();
			}

			if (g_mtk_idle_module.module->reg.init.dettach)
				g_mtk_idle_module.module->reg.init.dettach();

			g_mtk_idle_module.module = NULL;
			g_mtk_idle_module.valid = 0;
			g_mtk_idle_module.mod_map.valid = 0;

			bRet = MTK_IDLE_MOD_OK;
		}
	} while (0);

	mutex_unlock(&mtk_idle_module_locker);
	return bRet;
}

int mtk_idle_module_register(struct MTK_IDLE_MODULE *module)
{
	struct mtk_idle_init_data pData;
	struct MTK_IDLE_MODEL **model = NULL;

	int model_num = 0;
	unsigned int model_mask = 0;

	if (!module || !module->models)
		return MTK_IDLE_MOD_ERROR;

	if (g_mtk_idle_module.module) {
		int mRet = MTK_IDLE_MOD_FAIL;
		struct MTK_IDLE_MODULE *prev = g_mtk_idle_module.module;

		mRet = mtk_idle_module_unregister(prev);

		if (mRet != MTK_IDLE_MOD_OK)
			return MTK_IDLE_MOD_FAIL;
	}

	mutex_lock(&mtk_idle_module_locker);

	g_mtk_idle_module.module = module;
	g_mtk_idle_module.valid = 0;

	if (module->reg.init.attach)
		module->reg.init.attach();

	if (module->reg.get_init_data)
		module->reg.get_init_data(&pData);

	foreach_mtk_idle_models(model, model_num, 0) {
		if (((*model)->clerk.type >= MTK_IDLE_MODULE_MODEL_MAX)
			|| (model_mask & (1<<(*model)->clerk.type))) {
			pr_alert("[%s:%d] - invalid idle id = %d in %s\n"
				, __func__, __LINE__
				, (*model)->clerk.type, (*model)->clerk.name);
			continue;
		}

		if ((*model)->clerk.type < 0)
			continue;

		g_mtk_idle_module.mod_map.mods[(*model)->clerk.type] = *model;

		if ((*model)->policy.init)
			(*model)->policy.init(&pData);

		/* make sure data initialize */
		MTK_IDLE_MODEL_STATUS_INIT((*model)->clerk.status);

		model_mask |= (1<<(*model)->clerk.type);
		g_mtk_idle_module.valid |= (1<<model_num);
	}

	g_mtk_idle_module.mod_map.valid = model_mask;

	if (g_mtk_idle_module.mod_map.valid != 0)
		MTK_IDLE_MODULE_STATUS_SET(MTK_IDLE_MODULE_ENABLE);

	mutex_unlock(&mtk_idle_module_locker);

	return MTK_IDLE_MOD_OK;
}

