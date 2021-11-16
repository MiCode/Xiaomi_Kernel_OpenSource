/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cpu.h>

#include <linux/types.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/spinlock.h>
#include <linux/cpumask.h>
#include <linux/tick.h>

#include <linux/seq_file.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/kallsyms.h>

//#include <linux/irqchip/mtk-gic.h>
#include <linux/irqchip/mtk-gic-extend.h>
#include <asm/system_misc.h>
#include <mt-plat/sync_write.h>
#include <mtk_gpt.h>
#include <mtk_spm.h>
#include <mtk_spm_dpidle.h>
#include <mtk_spm_idle.h>
#ifdef CONFIG_THERMAL
#include <mach/mtk_thermal.h>
#endif
#include <mtk_idle.h>
#include <mtk_idle_internal.h>
#include <mtk_idle_profile.h>
#include <mtk_spm_reg.h>
#include <mtk_spm_misc.h>
#include <mtk_spm_resource_req.h>
#include <mtk_spm_resource_req_internal.h>

#include <trace/events/mtk_idle_event.h>

#include "ufs-mtk.h"

#ifdef CONFIG_MTK_DCS
#include <mt-plat/mtk_meminfo.h>
#endif

#include <linux/uaccess.h>

#if defined(CONFIG_MTK_PMIC) || defined(CONFIG_MTK_PMIC_NEW_ARCH)
#include <mt-plat/upmu_common.h>
#include <include/pmic.h>
#endif

#if !defined(SPM_K414_EARLY_PORTING)
#include "mtk_mcdi_governor.h"
#endif

#include <mt-plat/mtk_boot.h>

#include <mtk_lp_sysfs.h>
#include <mtk_lp_kernfs.h>
#include "mtk_idle_sysfs.h"

#define IDLE_GPT GPT4

#define IDLE_TAG     "[name:spm&]Power/swap "
#define idle_err(fmt, args...)		printk_deferred(IDLE_TAG fmt, ##args)
#define idle_warn(fmt, args...)		printk_deferred(IDLE_TAG fmt, ##args)
#define idle_info(fmt, args...)		printk_deferred(IDLE_TAG fmt, ##args)
#define idle_ver(fmt, args...)		printk_deferred(IDLE_TAG fmt, ##args)
#define idle_dbg(fmt, args...)		printk_deferred(IDLE_TAG fmt, ##args)

#define log2buf(p, s, fmt, args...) \
	(p += scnprintf(p, sizeof(s) - strlen(s), fmt, ##args))

#define USING_STD_TIMER_OPS

void go_to_wfi(void)
{
	isb();
	mb();	/* memory barrier */
	__asm__ __volatile__("wfi" : : : "memory");
}

#if 1
void __attribute__((weak)) mtkTTimer_start_timer(void)
{

}

void __attribute__((weak)) mtkTTimer_cancel_timer(void)
{

}

bool __attribute__((weak)) spm_get_sodi3_en(void)
{
	return false;
}

bool __attribute__((weak)) spm_get_sodi_en(void)
{
	return false;
}

int __attribute__((weak)) hps_del_timer(void)
{
	return 0;
}

int __attribute__((weak)) hps_restart_timer(void)
{
	return 0;
}

unsigned int __attribute__((weak))
spm_go_to_dpidle(u32 spm_flags, u32 spm_data,
		 u32 log_cond, u32 operation_cond)
{
	go_to_wfi();

	return WR_NONE;
}

unsigned int __attribute__((weak))
spm_go_to_sodi3(u32 spm_flags, u32 spm_data, u32 sodi_flags)
{
	go_to_wfi();

	return WR_NONE;
}

unsigned int __attribute__((weak))
spm_go_to_sodi(u32 spm_flags, u32 spm_data, u32 sodi_flags)
{
	go_to_wfi();

	return WR_NONE;
}

int __attribute__((weak)) is_teei_ready(void)
{
	return 1;
}

unsigned long __attribute__((weak)) localtimer_get_counter(void)
{
	return 0;
}

int __attribute__((weak)) localtimer_set_next_event(unsigned long evt)
{
	return 0;
}

uint32_t __attribute__((weak)) clk_buf_bblpm_enter_cond(void)
{
	return -1;
}

void __attribute__((weak)) idle_refcnt_inc(void)
{
}

void __attribute__((weak)) idle_refcnt_dec(void)
{
}

#endif

#define IDLE_VCORE_CHECK_FOR_LP_MODE        0
#define IDLE_VCORE_FORCE_LP_MODE            1
#define IDLE_VCORE_FORCE_NORMAL_MODE        2
#define IDLE_VCORE_BYPASS_CHECK_FOR_LP_MODE 3

static int idle_force_vcore_lp_mode = IDLE_VCORE_CHECK_FOR_LP_MODE;
static bool idle_by_pass_secure_cg;
static unsigned int idle_block_mask[NR_TYPES][NF_CG_STA_RECORD];
static bool clkmux_cond[NR_TYPES];
static bool vcore_cond[NR_TYPES];
static unsigned int clkmux_block_mask[NR_TYPES][NF_CLK_CFG];
static unsigned int clkmux_addr[NF_CLK_CFG];

/* DeepIdle */
static unsigned int     dpidle_time_criteria = 26000; /* 2ms */
static unsigned long    dpidle_cnt[NR_CPUS] = {0};
static unsigned long    dpidle_f26m_cnt[NR_CPUS] = {0};
static unsigned long    dpidle_block_cnt[NR_REASONS] = {0};
static bool             dpidle_by_pass_cg;
bool                    dpidle_by_pass_pg;
static bool             dpidle_gs_dump_req;
static unsigned int     dpidle_gs_dump_delay_ms = 10000; /* 10 sec */
static unsigned int     dpidle_gs_dump_req_ts;
static unsigned int     dpidle_dump_log = DEEPIDLE_LOG_REDUCED;
static unsigned int     dpidle_run_once;

/* SODI3 */

static unsigned int     soidle3_pll_block_mask[NR_PLLS] = {0x0};
static unsigned long    soidle3_cnt[NR_CPUS] = {0};
static unsigned long    soidle3_block_cnt[NR_REASONS] = {0};

static bool             soidle3_by_pass_cg;
static bool             soidle3_by_pass_pll;
static bool             soidle3_by_pass_en;
static bool             soidle3_by_pass_disp_pwm;
static u32              sodi3_flags = SODI_FLAG_REDUCE_LOG;
static int              sodi3_by_uptime_count;

static bool             is_sodi3_enter;

/* SODI */
static unsigned long    soidle_cnt[NR_CPUS] = {0};
static unsigned long    soidle_block_cnt[NR_REASONS] = {0};
static bool             soidle_by_pass_cg;
bool                    soidle_by_pass_pg;
static bool             soidle_by_pass_en;
static u32              sodi_flags = SODI_FLAG_REDUCE_LOG;
static int              sodi_by_uptime_count;

/* Regular Idle */
static unsigned long    rgidle_cnt[NR_CPUS] = {0};

/* idle_notifier */
static RAW_NOTIFIER_HEAD(mtk_idle_notifier);

int mtk_idle_notifier_register(struct notifier_block *n)
{
	int ret = 0;
	int index = 0;
#ifdef CONFIG_KALLSYMS
	char namebuf[128] = {0};
	const char *symname = NULL;

	symname = kallsyms_lookup((unsigned long)n->notifier_call,
			NULL, NULL, NULL, namebuf);
	if (symname) {
		printk_deferred("[name:spm&][mt_idle_ntf] <%02d>%08lx (%s)\n",
			index++, (unsigned long)n->notifier_call, symname);
	} else {
		printk_deferred("[name:spm&][mt_idle_ntf] <%02d>%08lx\n",
			index++, (unsigned long)n->notifier_call);
	}
#else
	printk_deferred("[name:spm&][mt_idle_ntf] <%02d>%08lx\n",
			index++, (unsigned long)n->notifier_call);
#endif

	ret = raw_notifier_chain_register(&mtk_idle_notifier, n);

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_idle_notifier_register);

void mtk_idle_notifier_unregister(struct notifier_block *n)
{
	raw_notifier_chain_unregister(&mtk_idle_notifier, n);
}
EXPORT_SYMBOL_GPL(mtk_idle_notifier_unregister);

void mtk_idle_notifier_call_chain(unsigned long val)
{
	raw_notifier_call_chain(&mtk_idle_notifier, val, NULL);
}
EXPORT_SYMBOL_GPL(mtk_idle_notifier_call_chain);

/*
 * Check clkmux condition
 *   1. Only in deepidle/SODI3
 *   2. After mtk_idle_notifier_call_chain()
 *       - To ensure other subsystem controls its clkmux well
 */
static unsigned int check_and_update_vcore_lp_mode_cond(int type)
{
	unsigned int op_cond = 0;

	if (idle_force_vcore_lp_mode == IDLE_VCORE_BYPASS_CHECK_FOR_LP_MODE)
		goto END;

	memset(clkmux_block_mask[type],	0, NF_CLK_CFG * sizeof(unsigned int));

	clkmux_cond[type] = mtk_idle_check_clkmux(type, clkmux_block_mask);
	vcore_cond[type]  = mtk_idle_check_vcore_cond();

	switch (idle_force_vcore_lp_mode) {
	case IDLE_VCORE_CHECK_FOR_LP_MODE:
		/* by clkmux check */
		op_cond |= (clkmux_cond[type] ? DEEPIDLE_OPT_VCORE_LP_MODE : 0);
		op_cond |= (vcore_cond[type] ? DEEPIDLE_OPT_VCORE_LOW_VOLT : 0);
		break;
	case IDLE_VCORE_FORCE_LP_MODE:
		/* enter LP mode */
		op_cond |= (DEEPIDLE_OPT_VCORE_LP_MODE |
			    DEEPIDLE_OPT_VCORE_LOW_VOLT);
		break;
	case IDLE_VCORE_FORCE_NORMAL_MODE:
		/* no enter LP mode */
		op_cond &= ~(DEEPIDLE_OPT_VCORE_LP_MODE |
			     DEEPIDLE_OPT_VCORE_LOW_VOLT);
		break;
	default:
		op_cond = 0;
		break;
	}

END:
	return op_cond;
}

#ifndef USING_STD_TIMER_OPS
/* Workaround of static analysis defect*/
int idle_gpt_get_cnt(unsigned int id, unsigned int *ptr)
{
	unsigned int val[2] = {0};
	int ret = 0;

	ret = gpt_get_cnt(id, val);
	*ptr = val[0];

	return ret;
}

int idle_gpt_get_cmp(unsigned int id, unsigned int *ptr)
{
	unsigned int val[2] = {0};
	int ret = 0;

	ret = gpt_get_cmp(id, val);
	*ptr = val[0];

	return ret;
}
#endif

#if 0
static bool next_timer_criteria_check(unsigned int timer_criteria)
{
	unsigned int timer_left = 0;
	bool ret = true;

#ifdef USING_STD_TIMER_OPS
	struct timespec t;

	t = ktime_to_timespec(tick_nohz_get_sleep_length());
	timer_left = t.tv_sec * USEC_PER_SEC + t.tv_nsec / NSEC_PER_USEC;

	if (timer_left < timer_criteria)
		ret = false;
#else
#ifdef CONFIG_SMP
	timer_left = localtimer_get_counter();

	if ((int)timer_left < timer_criteria ||
			((int)timer_left) < 0)
		ret = false;
#else
	unsigned int timer_cmp = 0;

	gpt_get_cnt(GPT1, &timer_left);
	gpt_get_cmp(GPT1, &timer_cmp);

	if ((timer_cmp - timer_left) < timer_criteria)
		ret = false;
#endif
#endif

	return ret;
}
#endif

static void timer_setting_before_wfi(bool f26m_off)
{
#ifndef USING_STD_TIMER_OPS
#ifdef CONFIG_SMP
	unsigned int timer_left = 0;

	timer_left = localtimer_get_counter();

	if ((int)timer_left <= 0)
		/* Trigger idle_gpt Timeout imediately */
		gpt_set_cmp(IDLE_GPT, 1);
	else {
		if (f26m_off)
			gpt_set_cmp(IDLE_GPT, div_u64(timer_left, 406.25));
	else
		gpt_set_cmp(IDLE_GPT, timer_left);
	}

	if (f26m_off)
		gpt_set_clk(IDLE_GPT, GPT_CLK_SRC_RTC, GPT_CLK_DIV_1);

	start_gpt(IDLE_GPT);
#else
	gpt_get_cnt(GPT1, &timer_left);
#endif
#endif
}

static void timer_setting_after_wfi(bool f26m_off)
{
#ifndef USING_STD_TIMER_OPS
#ifdef CONFIG_SMP
	if (gpt_check_and_ack_irq(IDLE_GPT)) {
		localtimer_set_next_event(1);
		if (f26m_off)
			gpt_set_clk(IDLE_GPT, GPT_CLK_SRC_SYS, GPT_CLK_DIV_1);
	} else {
		/* waked up by other wakeup source */
		unsigned int cnt, cmp;

		idle_gpt_get_cnt(IDLE_GPT, &cnt);
		idle_gpt_get_cmp(IDLE_GPT, &cmp);
		if (unlikely(cmp < cnt)) {
			idle_err("[%s]GPT%d: counter = %10u, compare = %10u\n",
					__func__, IDLE_GPT + 1, cnt, cmp);
			/* BUG(); */
		}

		if (f26m_off) {
			localtimer_set_next_event((cmp - cnt) * 1625 / 4);
			gpt_set_clk(IDLE_GPT, GPT_CLK_SRC_SYS, GPT_CLK_DIV_1);
		} else
			localtimer_set_next_event(cmp - cnt);

		stop_gpt(IDLE_GPT);
	}
#endif
#endif
}

#if !defined(CONFIG_FPGA_EARLY_PORTING)
static bool mtk_idle_cpu_criteria(void)
{
	/* single core check will be checked mcdi driver for acao case */
	return true;
}
#endif

static unsigned int idle_ufs_lock;
static DEFINE_SPINLOCK(idle_ufs_spin_lock);

void idle_lock_by_ufs(unsigned int lock)
{
	unsigned long flags;

	spin_lock_irqsave(&idle_ufs_spin_lock, flags);
	idle_ufs_lock = lock;
	spin_unlock_irqrestore(&idle_ufs_spin_lock, flags);
}

/*
 * Enable/Disable idle condition mask
 */

static DEFINE_SPINLOCK(idle_condition_mask_spin_lock);

static void enable_idle_by_mask(int idle_type, int grp, unsigned int mask)
{
	unsigned long flags;

	if (!((idle_type >= 0 && idle_type < NR_TYPES) &&
		(grp >= 0 && grp < NR_GRPS)))
		return;
	spin_lock_irqsave(&idle_condition_mask_spin_lock, flags);
	idle_condition_mask[idle_type][grp] &= ~mask;
	spin_unlock_irqrestore(&idle_condition_mask_spin_lock, flags);
}

static void disable_idle_by_mask(int idle_type, int grp, unsigned int mask)
{
	unsigned long flags;

	if (!((idle_type >= 0 && idle_type < NR_TYPES) &&
		(grp >= 0 && grp < NR_GRPS)))
		return;
	spin_lock_irqsave(&idle_condition_mask_spin_lock, flags);
	idle_condition_mask[idle_type][grp] |= mask;
	spin_unlock_irqrestore(&idle_condition_mask_spin_lock, flags);
}

static void enable_idle_by_bit(int idle_type, int id)
{
	int grp = id / 32;
	unsigned int mask = 1U << (id % 32);

	if (!((idle_type >= 0 && idle_type < NR_TYPES) &&
		(grp >= 0 && grp < NR_GRPS)))
		return;
	enable_idle_by_mask(idle_type, grp, mask);

	if (idle_type == IDLE_TYPE_SO)
		enable_idle_by_mask(IDLE_TYPE_SO3, grp, mask);
}

static void disable_idle_by_bit(int idle_type, int id)
{
	int grp = id / 32;
	unsigned int mask = 1U << (id % 32);

	if (!((idle_type >= 0 && idle_type < NR_TYPES) &&
		(grp >= 0 && grp < NR_GRPS)))
		return;

	disable_idle_by_mask(idle_type, grp, mask);

	if (idle_type == IDLE_TYPE_SO)
		disable_idle_by_mask(IDLE_TYPE_SO3, grp, mask);
}

/*
 * SODI3 part
 */
static bool soidle3_can_enter(int cpu, int reason)
{
	/* check previous common criterion */
	if (reason == BY_CLK) {
		if (soidle3_by_pass_cg == 0) {
			if (idle_block_mask[IDLE_TYPE_SO3][NR_GRPS])
				goto out;
		}
		reason = NR_REASONS;
	} else if (reason < NR_REASONS)
		goto out;

	#if !defined(CONFIG_FPGA_EARLY_PORTING)
	if (soidle3_by_pass_en == 0) {
		if ((spm_get_sodi_en() == 0) || (spm_get_sodi3_en() == 0)) {
			/* if SODI is disabled, SODI3 is also disabled */
			reason = BY_DIS;
			goto out;
		}
	}

	if (soidle3_by_pass_disp_pwm == 0) {
		if (!mtk_idle_disp_is_pwm_rosc()) {
			reason = BY_PWM;
			goto out;
		}
	}

	if (soidle3_by_pass_pll == 0) {
		if (!mtk_idle_check_pll(soidle3_pll_condition_mask,
					soidle3_pll_block_mask)) {
			reason = BY_PLL;
			goto out;
		}
	}
	#endif

	/* do not enter sodi3 at first 30 seconds */
	if (sodi3_by_uptime_count != -1) {
		struct timespec uptime;
		unsigned long val;

		get_monotonic_boottime(&uptime);
		val = (unsigned long)uptime.tv_sec;
		if (val <= 30) {
			sodi3_by_uptime_count++;
			reason = BY_BOOT;
			goto out;
		} else {
			#if !defined(CONFIG_MACH_MT6739)
			idle_warn("SODI3: blocking by uptime, count = %d\n",
				  sodi3_by_uptime_count);
			#endif
			sodi3_by_uptime_count = -1;
		}
	}

	/* Check Secure CGs - after other SODI3 criteria PASS */
	if (!idle_by_pass_secure_cg) {
		if (!mtk_idle_check_secure_cg(idle_block_mask)) {
			reason = BY_CLK;
			goto out;
		}
	}

out:
	return mtk_idle_select_state(IDLE_TYPE_SO3, reason);
}

void soidle3_before_wfi(int cpu)
{
	timer_setting_before_wfi(true);
}

void soidle3_after_wfi(int cpu)
{
	timer_setting_after_wfi(true);

	soidle3_cnt[cpu]++;
}

/*
 * SODI part
 */
static bool soidle_can_enter(int cpu, int reason)
{
	/* check previous common criterion */
	if (reason == BY_CLK) {
		if (soidle_by_pass_cg == 0) {
			if (idle_block_mask[IDLE_TYPE_SO][NR_GRPS])
				goto out;
		}
		reason = NR_REASONS;
	} else if (reason < NR_REASONS)
		goto out;

	#if !defined(CONFIG_FPGA_EARLY_PORTING)
	if (soidle_by_pass_en == 0) {
		if (spm_get_sodi_en() == 0) {
			reason = BY_DIS;
			goto out;
		}
	}
	#endif

	/* do not enter sodi at first 20 seconds */
	if (sodi_by_uptime_count != -1) {
		struct timespec uptime;
		unsigned long val;

		get_monotonic_boottime(&uptime);
		val = (unsigned long)uptime.tv_sec;
		if (val <= 20) {
			sodi_by_uptime_count++;
			reason = BY_BOOT;
			goto out;
		} else {
			#if !defined(CONFIG_MACH_MT6739)
			idle_warn("SODI: blocking by uptime, count = %d\n",
				  sodi_by_uptime_count);
			#endif
			sodi_by_uptime_count = -1;
		}
	}

	/* Check Secure CGs - after other SODI criteria PASS */
	if (!idle_by_pass_secure_cg) {
		if (!mtk_idle_check_secure_cg(idle_block_mask)) {
			reason = BY_CLK;
			goto out;
		}
	}

out:
	return mtk_idle_select_state(IDLE_TYPE_SO, reason);
}

void soidle_before_wfi(int cpu)
{
	timer_setting_before_wfi(false);
}

void soidle_after_wfi(int cpu)
{
	timer_setting_after_wfi(false);

	soidle_cnt[cpu]++;
}

/*
 * deep idle part
 */
static bool dpidle_can_enter(int cpu, int reason)
{
	dpidle_profile_time(DPIDLE_PROFILE_CAN_ENTER_START);

	/* check previous common criterion */
	if (reason == BY_CLK) {
		if (dpidle_by_pass_cg == 0) {
			if (idle_block_mask[IDLE_TYPE_DP][NR_GRPS])
				goto out;
		}
	} else if (reason < NR_REASONS)
		goto out;

	reason = NR_REASONS;

	/* Check Secure CGs - after other deepidle criteria PASS */
	if (!idle_by_pass_secure_cg) {
		if (!mtk_idle_check_secure_cg(idle_block_mask)) {
			reason = BY_CLK;
			goto out;
		}
	}

out:
	dpidle_profile_time(DPIDLE_PROFILE_CAN_ENTER_END);

	return mtk_idle_select_state(IDLE_TYPE_DP, reason);
}

/*
 * regular idle part
 */
static bool rgidle_can_enter(int cpu, int reason)
{
	return true;
}

static void rgidle_before_wfi(int cpu)
{

}

static void rgidle_after_wfi(int cpu)
{
	rgidle_cnt[cpu]++;
}

static noinline void go_to_rgidle(int cpu)
{
	rgidle_before_wfi(cpu);

#if !defined(SPM_K414_EARLY_PORTING)
	trace_rgidle_rcuidle(cpu, 1);
#endif
	go_to_wfi();

#if !defined(SPM_K414_EARLY_PORTING)
	trace_rgidle_rcuidle(cpu, 0);
#endif

	rgidle_after_wfi(cpu);
}

/*
 * idle task flow part
 */

/*
 * xxidle_handler return 1 if enter and exit the low power state
 */
#if defined(CONFIG_MACH_MT6763)

static u32 slp_spm_SODI3_flags = {
	SPM_FLAG_DIS_INFRA_PDN |
	SPM_FLAG_DIS_VCORE_DVS |
	SPM_FLAG_DIS_VCORE_DFS |
	SPM_FLAG_DIS_ATF_ABORT |
	SPM_FLAG_KEEP_CSYSPWRUPACK_HIGH |
#if !defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	SPM_FLAG_DIS_SSPM_SRAM_SLEEP |
#endif
	SPM_FLAG_SODI_OPTION |
	SPM_FLAG_ENABLE_SODI3
};

static u32 slp_spm_SODI_flags = {
	SPM_FLAG_DIS_INFRA_PDN |
	SPM_FLAG_DIS_VCORE_DVS |
	SPM_FLAG_DIS_VCORE_DFS |
	SPM_FLAG_DIS_ATF_ABORT |
	SPM_FLAG_KEEP_CSYSPWRUPACK_HIGH |
#if !defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	SPM_FLAG_DIS_SSPM_SRAM_SLEEP |
#endif
	SPM_FLAG_SODI_OPTION
};

u32 slp_spm_deepidle_flags = {
	/* SPM_FLAG_DIS_CPU_PDN | */
	SPM_FLAG_DIS_INFRA_PDN |
	/* SPM_FLAG_DIS_DDRPHY_PDN | */
	SPM_FLAG_DIS_VCORE_DVS |
	SPM_FLAG_DIS_VCORE_DFS |
	SPM_FLAG_DIS_ATF_ABORT |
	SPM_FLAG_KEEP_CSYSPWRUPACK_HIGH |
#if !defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	SPM_FLAG_DIS_SSPM_SRAM_SLEEP |
#endif
	SPM_FLAG_DEEPIDLE_OPTION
};

static u32 slp_spm_SODI3_flags1 = {
	0
};

static u32 slp_spm_SODI_flags1 = {
	0
};

static u32 slp_spm_deepidle_flags1 = {
	0
};

#elif defined(CONFIG_MACH_MT6739)

static u32 slp_spm_SODI3_flags = {
	/* SPM_FLAG_DIS_CPU_PDN | */
	/* SPM_FLAG_DIS_VPROC_VSRAM_DVS | */
	SPM_FLAG_DIS_ATF_ABORT |
	SPM_FLAG_DIS_VCORE_DVS |
	SPM_FLAG_DIS_VCORE_DFS |
	SPM_FLAG_KEEP_CSYSPWRUPACK_HIGH |
	SPM_FLAG_ENABLE_SODI2P5
};

static u32 slp_spm_SODI_flags = {
	/* SPM_FLAG_DIS_CPU_PDN | */
	/* SPM_FLAG_DIS_VPROC_VSRAM_DVS | */
	SPM_FLAG_DIS_ATF_ABORT |
	SPM_FLAG_DIS_VCORE_DVS |
	SPM_FLAG_DIS_VCORE_DFS |
	SPM_FLAG_KEEP_CSYSPWRUPACK_HIGH |
	SPM_FLAG_ENABLE_SODI2P5
};

u32 slp_spm_deepidle_flags = {
	/* SPM_FLAG_DIS_CPU_PDN | */
	/* SPM_FLAG_DIS_VPROC_VSRAM_DVS | */
	SPM_FLAG_DIS_ATF_ABORT |
	SPM_FLAG_DIS_VCORE_DVS |
	SPM_FLAG_DIS_VCORE_DFS |
	SPM_FLAG_KEEP_CSYSPWRUPACK_HIGH
};

static u32 slp_spm_SODI3_flags1 = {
	0
};

static u32 slp_spm_SODI_flags1 = {
	0
};

static u32 slp_spm_deepidle_flags1 = {
	0
};


#elif defined(CONFIG_MACH_MT6771)

static u32 slp_spm_SODI3_flags = {
	SPM_FLAG_DIS_INFRA_PDN |
	SPM_FLAG_DIS_VCORE_DVS |
	SPM_FLAG_DIS_VCORE_DFS |
	SPM_FLAG_DIS_ATF_ABORT |
	SPM_FLAG_KEEP_CSYSPWRUPACK_HIGH |
	/* SPM_FLAG_DIS_VPROC_VSRAM_DVS | */
#if !defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	SPM_FLAG_DIS_SSPM_SRAM_SLEEP |
#endif
	SPM_FLAG_SODI_OPTION |
	SPM_FLAG_ENABLE_SODI3
};

static u32 slp_spm_SODI_flags = {
	SPM_FLAG_DIS_INFRA_PDN |
	SPM_FLAG_DIS_VCORE_DVS |
	SPM_FLAG_DIS_VCORE_DFS |
	SPM_FLAG_DIS_ATF_ABORT |
	SPM_FLAG_KEEP_CSYSPWRUPACK_HIGH |
	/* SPM_FLAG_DIS_VPROC_VSRAM_DVS | */
#if !defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	SPM_FLAG_DIS_SSPM_SRAM_SLEEP |
#endif
	SPM_FLAG_SODI_OPTION
};

u32 slp_spm_deepidle_flags = {
	/* SPM_FLAG_DIS_CPU_PDN | */
	SPM_FLAG_DIS_INFRA_PDN |
	/* SPM_FLAG_DIS_DDRPHY_PDN | */
	SPM_FLAG_DIS_VCORE_DVS |
	SPM_FLAG_DIS_VCORE_DFS |
	SPM_FLAG_DIS_ATF_ABORT |
	SPM_FLAG_KEEP_CSYSPWRUPACK_HIGH |
	/* SPM_FLAG_DIS_VPROC_VSRAM_DVS | */
#if !defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	SPM_FLAG_DIS_SSPM_SRAM_SLEEP |
#endif
	SPM_FLAG_DEEPIDLE_OPTION
};

static u32 slp_spm_SODI3_flags1 = {
	0
};

static u32 slp_spm_SODI_flags1 = {
	0
};

static u32 slp_spm_deepidle_flags1 = {
	0
};

#endif

unsigned int ufs_cb_before_xxidle(void)
{
	unsigned int op_cond = 0;
	bool bblpm_check = false;

#if defined(CONFIG_MTK_UFS_SUPPORT)
	bool ufs_in_hibernate = false;
	int boot_type;

	boot_type = get_boot_type();
	if (boot_type == BOOTDEV_UFS) {
		ufs_in_hibernate = !ufs_mtk_deepidle_hibern8_check();
		op_cond = ufs_in_hibernate ? DEEPIDLE_OPT_XO_UFS_ON_OFF : 0;
	}
#endif

	bblpm_check = !clk_buf_bblpm_enter_cond();
	op_cond |= bblpm_check ? DEEPIDLE_OPT_CLKBUF_BBLPM : 0;

	return op_cond;
}

void ufs_cb_after_xxidle(void)
{
#if defined(CONFIG_MTK_UFS_SUPPORT)
	int boot_type;

	boot_type = get_boot_type();
	if (boot_type == BOOTDEV_UFS)
		ufs_mtk_deepidle_leave();
#endif
}

unsigned int soidle_pre_handler(void)
{
	unsigned int op_cond = 0;

	op_cond = ufs_cb_before_xxidle();


#ifdef CONFIG_THERMAL
	/* cancel thermal hrtimer for power saving */
	mtkTTimer_cancel_timer();
#endif

	if (is_sodi3_enter) {
		op_cond |=
			check_and_update_vcore_lp_mode_cond(IDLE_TYPE_SO3);
	}

	return op_cond;
}

void soidle_post_handler(void)
{

#ifdef CONFIG_THERMAL
	/* restart thermal hrtimer for update temp info */
	mtkTTimer_start_timer();
#endif

	ufs_cb_after_xxidle();
}

static unsigned int dpidle_pre_process(int cpu)
{
	unsigned int op_cond = 0;

	dpidle_profile_time(DPIDLE_PROFILE_ENTER_UFS_CB_BEFORE_XXIDLE_START);

	op_cond = ufs_cb_before_xxidle();

	dpidle_profile_time(DPIDLE_PROFILE_ENTER_UFS_CB_BEFORE_XXIDLE_END);

	mtk_idle_notifier_call_chain(NOTIFY_DPIDLE_ENTER);

	dpidle_profile_time(DPIDLE_PROFILE_IDLE_NOTIFIER_END);

#if !defined(CONFIG_FPGA_EARLY_PORTING)
#ifdef CONFIG_THERMAL
	/* cancel thermal hrtimer for power saving */
	mtkTTimer_cancel_timer();
#endif
#endif

	timer_setting_before_wfi(false);

	dpidle_profile_time(DPIDLE_PROFILE_TIMER_DEL_END);

	op_cond |= check_and_update_vcore_lp_mode_cond(IDLE_TYPE_DP);

	return op_cond;
}

static void dpidle_post_process(int cpu)
{
	dpidle_profile_time(DPIDLE_PROFILE_TIMER_RESTORE_START);

#if !defined(CONFIG_FPGA_EARLY_PORTING)
	timer_setting_after_wfi(false);

#ifdef CONFIG_THERMAL
	/* restart thermal hrtimer for update temp info */
	mtkTTimer_start_timer();
#endif
#endif

	dpidle_profile_time(DPIDLE_PROFILE_TIMER_RESTORE_END);

	mtk_idle_notifier_call_chain(NOTIFY_DPIDLE_LEAVE);

	dpidle_profile_time(DPIDLE_PROFILE_UFS_CB_AFTER_XXIDLE_START);

	ufs_cb_after_xxidle();

	dpidle_profile_time(DPIDLE_PROFILE_UFS_CB_AFTER_XXIDLE_END);

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	spm_dpidle_notify_sspm_after_wfi_async_wait();
#endif

	dpidle_profile_time(
		DPIDLE_PROFILE_NOTIFY_SSPM_AFTER_WFI_ASYNC_WAIT_END);

	dpidle_cnt[cpu]++;
}

static bool (*idle_can_enter[NR_TYPES])(int cpu, int reason) = {
	dpidle_can_enter,
	soidle3_can_enter,
	soidle_can_enter,
	rgidle_can_enter,
};

/* Mapping idle_switch to CPUidle C States */
static int idle_stat_mapping_table[NR_TYPES] = {
	CPUIDLE_STATE_DP,
	CPUIDLE_STATE_SO3,
	CPUIDLE_STATE_SO,
	CPUIDLE_STATE_RG
};

int mtk_idle_select(int cpu)
{
	int i = NR_TYPES - 1;
	int reason = NR_REASONS;
#if defined(CONFIG_MTK_UFS_SUPPORT)
	unsigned long flags = 0;
	unsigned int ufs_locked;
	int boot_type;
#endif
#ifdef CONFIG_MTK_DCS
	int ch = 0, ret = -1;
	enum dcs_status dcs_status;
	bool dcs_lock_get = false;
#endif

	profile_so_start(PIDX_SELECT_TO_ENTER);
	profile_so3_start(PIDX_SELECT_TO_ENTER);
	dpidle_profile_time(DPIDLE_PROFILE_IDLE_SELECT_START);

#if !defined(CONFIG_FPGA_EARLY_PORTING)
	/* check if firmware loaded or not */
	if (!spm_load_firmware_status()) {
		reason = BY_FRM;
		goto get_idle_idx;
	}
#endif

#if !defined(CONFIG_FPGA_EARLY_PORTING)
#ifdef CONFIG_SMP
	/* check cpu status */
	if (!mtk_idle_cpu_criteria()) {
		reason = BY_CPU;
		goto get_idle_idx;
	}
#endif
#endif

	if (spm_get_resource_usage() == SPM_RESOURCE_ALL) {
		reason = BY_SRR;
		goto get_idle_idx;
	}

#if defined(CONFIG_MTK_UFS_SUPPORT)
	boot_type = get_boot_type();
	if (boot_type == BOOTDEV_UFS) {
		spin_lock_irqsave(&idle_ufs_spin_lock, flags);
		ufs_locked = idle_ufs_lock;
		spin_unlock_irqrestore(&idle_ufs_spin_lock, flags);

		if (ufs_locked) {
			reason = BY_UFS;
			goto get_idle_idx;
		}
	}
#endif

	/* teei ready */
#if !defined(CONFIG_FPGA_EARLY_PORTING)
#ifdef CONFIG_MICROTRUST_TEE_SUPPORT
	if (!is_teei_ready()) {
		reason = BY_TEE;
		goto get_idle_idx;
	}
#endif
#endif

#if !defined(CONFIG_FPGA_EARLY_PORTING)
	/* cg check */
	memset(idle_block_mask, 0,
		NR_TYPES * NF_CG_STA_RECORD * sizeof(unsigned int));
	if (!mtk_idle_check_cg(idle_block_mask)) {
		reason = BY_CLK;
		goto get_idle_idx;
	}
#endif

#ifdef CONFIG_MTK_DCS
	/* check if DCS channel switching */
	ret = dcs_get_dcs_status_trylock(&ch, &dcs_status);
	if (ret) {
		reason = BY_DCS;
		goto get_idle_idx;
	}

	dcs_lock_get = true;

#endif

get_idle_idx:
	/* check if criteria check fail in common part */
	for (i = 0; i < NR_TYPES; i++) {
		if (idle_switch[i]) {
			/* call each idle scenario check functions */
			if (idle_can_enter[i](cpu, reason))
				break;
		}
	}

	/* Prevent potential out-of-bounds vulnerability */
	i = (i >= NR_TYPES) ? NR_TYPES : i;

#ifdef CONFIG_MTK_DCS
	if (dcs_lock_get)
		dcs_get_dcs_status_unlock();
#endif

	dpidle_profile_time(DPIDLE_PROFILE_IDLE_SELECT_END);

	return i;
}

int mtk_idle_select_base_on_menu_gov(int cpu, int menu_select_state)
{
	int i;
	int state = CPUIDLE_STATE_RG;

	if (menu_select_state < 0)
		return menu_select_state;

	mtk_idle_dump_cnt_in_interval();

	i = mtk_idle_select(cpu);

	/* residency requirement of ALL C state is satisfied */
	if (menu_select_state == CPUIDLE_STATE_SO3) {
		state = idle_stat_mapping_table[i];
	/* SODI3.0 residency requirement does NOT satisfied */
	} else if (menu_select_state >= CPUIDLE_STATE_SO &&
		   menu_select_state <= CPUIDLE_STATE_DP) {
		if (i == IDLE_TYPE_SO3)
			i = idle_switch[IDLE_TYPE_SO] ?
				IDLE_TYPE_SO : IDLE_TYPE_RG;

		state = idle_stat_mapping_table[i];
	/* DPIDLE, SODI3.0, and SODI residency requirement does NOT satisfied */
	} else {
		state = CPUIDLE_STATE_RG;
	}

	return state;
}

int dpidle_enter(int cpu)
{
	int ret = CPUIDLE_STATE_DP;
	u32 operation_cond = 0;

	/* don't check lock dependency */
	lockdep_off();

	dpidle_profile_time(DPIDLE_PROFILE_ENTER);

	mtk_idle_ratio_calc_start(IDLE_TYPE_DP, cpu);

	operation_cond |= dpidle_pre_process(cpu);

	if (dpidle_gs_dump_req) {
		unsigned int current_ts = idle_get_current_time_ms();

		if ((current_ts - dpidle_gs_dump_req_ts) >=
			dpidle_gs_dump_delay_ms) {
			#if !defined(CONFIG_MACH_MT6739)
			idle_warn("dpidle dump LP golden\n");
			#endif
			dpidle_gs_dump_req = 0;
			operation_cond |= DEEPIDLE_OPT_DUMP_LP_GOLDEN;
		}
	}

	spm_go_to_dpidle(slp_spm_deepidle_flags,
			 slp_spm_deepidle_flags1,
			 dpidle_dump_log, operation_cond);

	dpidle_post_process(cpu);

	mtk_idle_ratio_calc_stop(IDLE_TYPE_DP, cpu);

	dpidle_profile_time(DPIDLE_PROFILE_LEAVE);
	dpidle_show_profile_time();

	lockdep_on();

	/* For test */
	if (dpidle_run_once)
		idle_switch[IDLE_TYPE_DP] = 0;

	return ret;
}
EXPORT_SYMBOL(dpidle_enter);

int soidle3_enter(int cpu)
{
	int ret = CPUIDLE_STATE_SO3;
	unsigned long long soidle3_time = 0;
	static unsigned long long soidle3_residency;

	is_sodi3_enter = true;

	/* don't check lock dependency */
	lockdep_off();

	profile_so3_end(PIDX_SELECT_TO_ENTER);

	profile_so3_start(PIDX_ENTER_TOTAL);

	if (sodi3_flags & SODI_FLAG_RESIDENCY)
		soidle3_time = idle_get_current_time_ms();

	mtk_idle_ratio_calc_start(IDLE_TYPE_SO3, cpu);

	profile_so3_start(PIDX_IDLE_NOTIFY_ENTER);
	mtk_idle_notifier_call_chain(NOTIFY_SOIDLE3_ENTER);
	profile_so3_end(PIDX_IDLE_NOTIFY_ENTER);

#ifdef DEFAULT_MMP_ENABLE
	mmprofile_log_ex(sodi_mmp_get_events()->sodi_enable,
			 MMPROFILE_FLAG_START, 0, 0);
#endif /* DEFAULT_MMP_ENABLE */

	spm_go_to_sodi3(slp_spm_SODI3_flags, slp_spm_SODI3_flags1, sodi3_flags);

	/* Clear SODI_FLAG_DUMP_LP_GS in sodi3_flags */
	sodi3_flags &= (~SODI_FLAG_DUMP_LP_GS);

#ifdef DEFAULT_MMP_ENABLE
	mmprofile_log_ex(sodi_mmp_get_events()->sodi_enable,
			 MMPROFILE_FLAG_END, 0, spm_read(SPM_PASR_DPD_3));
#endif /* DEFAULT_MMP_ENABLE */

	profile_so3_start(PIDX_IDLE_NOTIFY_LEAVE);
	mtk_idle_notifier_call_chain(NOTIFY_SOIDLE3_LEAVE);
	profile_so3_end(PIDX_IDLE_NOTIFY_LEAVE);

	mtk_idle_ratio_calc_stop(IDLE_TYPE_SO3, cpu);

	if (sodi3_flags & SODI_FLAG_RESIDENCY) {
		soidle3_residency += idle_get_current_time_ms() - soidle3_time;
		idle_dbg("SO3: soidle3_residency = %llu\n", soidle3_residency);
	}

	profile_so3_end(PIDX_LEAVE_TOTAL);

	/* dump latency profiling result */
	profile_so3_dump();

	lockdep_on();

	return ret;
}
EXPORT_SYMBOL(soidle3_enter);

int soidle_enter(int cpu)
{
	int ret = CPUIDLE_STATE_SO;
	unsigned long long soidle_time = 0;
	static unsigned long long soidle_residency;

	is_sodi3_enter = false;

	/* don't check lock dependency */
	lockdep_off();

	profile_so_end(PIDX_SELECT_TO_ENTER);

	profile_so_start(PIDX_ENTER_TOTAL);

	if (sodi_flags & SODI_FLAG_RESIDENCY)
		soidle_time = idle_get_current_time_ms();

	mtk_idle_ratio_calc_start(IDLE_TYPE_SO, cpu);

	profile_so_start(PIDX_IDLE_NOTIFY_ENTER);
	mtk_idle_notifier_call_chain(NOTIFY_SOIDLE_ENTER);
	profile_so_end(PIDX_IDLE_NOTIFY_ENTER);

#ifdef DEFAULT_MMP_ENABLE
	mmprofile_log_ex(sodi_mmp_get_events()->sodi_enable,
			 MMPROFILE_FLAG_START, 0, 0);
#endif /* DEFAULT_MMP_ENABLE */

	spm_go_to_sodi(slp_spm_SODI_flags, slp_spm_SODI_flags1, sodi_flags);

	/* Clear SODI_FLAG_DUMP_LP_GS in sodi_flags */
	sodi_flags &= (~SODI_FLAG_DUMP_LP_GS);

#ifdef DEFAULT_MMP_ENABLE
	mmprofile_log_ex(sodi_mmp_get_events()->sodi_enable,
			 MMPROFILE_FLAG_END, 0, spm_read(SPM_PASR_DPD_3));
#endif /* DEFAULT_MMP_ENABLE */

	profile_so_start(PIDX_IDLE_NOTIFY_LEAVE);
	mtk_idle_notifier_call_chain(NOTIFY_SOIDLE_LEAVE);
	profile_so_end(PIDX_IDLE_NOTIFY_LEAVE);

	mtk_idle_ratio_calc_stop(IDLE_TYPE_SO, cpu);

	if (sodi_flags & SODI_FLAG_RESIDENCY) {
		soidle_residency += idle_get_current_time_ms() - soidle_time;
		idle_dbg("SO: soidle_residency = %llu\n", soidle_residency);
	}

	profile_so_end(PIDX_LEAVE_TOTAL);

	/* dump latency profiling result */
	profile_so_dump();

	lockdep_on();

	return ret;
}
EXPORT_SYMBOL(soidle_enter);

int rgidle_enter(int cpu)
{
	int ret = CPUIDLE_STATE_RG;

	mtk_idle_dump_cnt_in_interval();
#if !defined(SPM_K414_EARLY_PORTING)
	mcdi_heart_beat_log_dump();
#endif
	remove_cpu_from_prefer_schedule_domain(cpu);

	mtk_idle_ratio_calc_start(IDLE_TYPE_RG, cpu);

	idle_refcnt_inc();

	go_to_rgidle(cpu);

	idle_refcnt_dec();

	mtk_idle_ratio_calc_stop(IDLE_TYPE_RG, cpu);

	add_cpu_to_prefer_schedule_domain(cpu);

	return ret;
}
EXPORT_SYMBOL(rgidle_enter);

/*
 * debugfs
 */
#define SYSFS_ENTRY "/sys/kernel/mtk_lpm/cpuidle"

/* idle_state */
static ssize_t idle_state_read(char *ToUserBuf, size_t sz_t, void *priv)
{
	int i;
	char *p = ToUserBuf;
	size_t sz = sz_t;
	struct mtk_idle_twam *cur_twam;

	cur_twam = mtk_idle_get_twam();

	#undef mt_idle_log
	#define mt_idle_log(fmt, args...) \
	do { \
		int l = scnprintf(p, sz, fmt, ##args); \
		p += l; \
		sz -= l; \
	} while (0)

	mt_idle_log("********** idle state dump **********\n");

	for (i = 0; i < nr_cpu_ids; i++) {
		mt_idle_log("dpidle_cnt[%d]=%lu, dpidle_26m[%d]=%lu, ",
			i, dpidle_cnt[i], i, dpidle_f26m_cnt[i]);
		mt_idle_log(
		"soidle3_cnt[%d]=%lu, soidle_cnt[%d]=%lu, rgidle_cnt[%d]=%lu\n",
		i, soidle3_cnt[i], i, soidle_cnt[i], i, rgidle_cnt[i]);
	}

	mt_idle_log("\n********** variables dump **********\n");
	for (i = 0; i < NR_TYPES; i++)
		mt_idle_log("%s_switch=%d, ",
			    mtk_get_idle_name(i), idle_switch[i]);

	mt_idle_log("\n");
	mt_idle_log("idle_ratio_en = %u\n", mtk_idle_get_ratio_status());
	mt_idle_log("idle_latency_en = %d\n",
		    mtk_idle_latency_profile_is_on() ? 1 : 0);
	if (cur_twam != NULL) {
		mt_idle_log("twam_handler:%s (clk:%s)\n",
						(cur_twam->running) ?
						"on":"off",
						(cur_twam->speed_mode) ?
						"speed":"normal");
	}
	mt_idle_log("bypass_secure_cg = %u\n", idle_by_pass_secure_cg);
	mt_idle_log("force VCORE lp mode = %u\n", idle_force_vcore_lp_mode);

	mt_idle_log("\n********** idle command help **********\n");
	mt_idle_log("status help:   cat %s/idle_state\n", SYSFS_ENTRY);
	mt_idle_log("switch on/off: echo switch mask > %s/idle_state\n",
		SYSFS_ENTRY);
	mt_idle_log("idle ratio profile: ");
	mt_idle_log("echo ratio 1/0 > %s/idle_state\n", SYSFS_ENTRY);
	mt_idle_log("idle latency profile: ");
	mt_idle_log("echo latency 1/0 > %s/idle_state\n", SYSFS_ENTRY);
	mt_idle_log("soidle3 help:  cat %s/soidle3_state\n", SYSFS_ENTRY);
	mt_idle_log("soidle help:   cat %s/soidle_state\n", SYSFS_ENTRY);
	mt_idle_log("dpidle help:   cat %s/dpidle_state\n", SYSFS_ENTRY);
	mt_idle_log("rgidle help:   cat %s/rgidle_state\n", SYSFS_ENTRY);

	return p - ToUserBuf;
}

static ssize_t idle_state_write(char *FromUserBuf, size_t sz, void *priv)
{
	char cmd[128];
	int param, idx;
	struct mtk_idle_twam *cur_twam;

	if (sscanf(FromUserBuf, "%127s %x", cmd, &param) == 2) {
		if (!strcmp(cmd, "switch")) {
			for (idx = 0; idx < NR_TYPES; idx++)
				idle_switch[idx] =
					(param & (1U << idx)) ? 1 : 0;
		} else if (!strcmp(cmd, "ratio")) {
			if (param == 1)
				mtk_idle_enable_ratio_calc();
			else
				mtk_idle_disable_ratio_calc();
		} else if (!strcmp(cmd, "latency")) {
			mtk_idle_latency_profile_enable(param ? true : false);
		} else if (!strcmp(cmd, "spmtwam_clk")) {
			cur_twam = mtk_idle_get_twam();
			if (cur_twam != NULL)
				cur_twam->speed_mode = param;
		} else if (!strcmp(cmd, "spmtwam_sel")) {
			cur_twam = mtk_idle_get_twam();
			if (cur_twam != NULL)
				cur_twam->sel = param;
		} else if (!strcmp(cmd, "spmtwam")) {
#if !defined(CONFIG_FPGA_EARLY_PORTING)
			idle_dbg("spmtwam_event = %d\n", param);
			if (param >= 0)
				mtk_idle_twam_enable((u32)param);
			else
				mtk_idle_twam_disable();
#endif
		} else if (!strcmp(cmd, "bypass_secure_cg")) {
			idle_by_pass_secure_cg = param;
		} else if (!strcmp(cmd, "force_vcore_lp_mode")) {
			idle_force_vcore_lp_mode = param;
		}
		return sz;
	} else if ((!kstrtoint(FromUserBuf, 10, &param)) == 1) {
		return sz;
	}

	return -EINVAL;
}

static const struct mtk_lp_sysfs_op idle_state_fops = {
	.fs_read = idle_state_read,
	.fs_write = idle_state_write,
};

/* dpidle_state */
static ssize_t dpidle_state_read(char *ToUserBuf, size_t sz_t, void *priv)
{
	int i, k;
	char *p = ToUserBuf;
	size_t sz = sz_t;

	#undef mt_idle_log
	#define mt_idle_log(fmt, args...) \
	do { \
		int l = scnprintf(p, sz, fmt, ##args); \
		p += l; \
		sz -= l; \
	} while (0)

	mt_idle_log("*********** deep idle state ************\n");
	mt_idle_log("dpidle_time_criteria=%u\n", dpidle_time_criteria);

	for (i = 0; i < NR_REASONS; i++)
		mt_idle_log("[%d]dpidle_block_cnt[%s]=%lu\n",
			    i, mtk_get_reason_name(i), dpidle_block_cnt[i]);
	mt_idle_log("\n");

	for (i = 0; i < NR_GRPS; i++) {
		mt_idle_log("[%02d]dpidle_condition_mask[%-10s]=0x%08x\t\t",
			    i, mtk_get_cg_group_name(i),
			    idle_condition_mask[IDLE_TYPE_DP][i]);
		mt_idle_log("dpidle_block_mask[%-10s]=0x%08x\n",
			    mtk_get_cg_group_name(i),
			    idle_block_mask[IDLE_TYPE_DP][i]);
	}

	mt_idle_log("dpidle pg_stat=0x%08x\n",
		    idle_block_mask[IDLE_TYPE_DP][NR_GRPS + 1]);

	mt_idle_log("dpidle_blocking_stat=\n");

	for (i = 0; i < NR_GRPS; i++) {
		mt_idle_log("[%-10s] - ", mtk_get_cg_group_name(i));

		for (k = 0; k < 32; k++) {
			if (dpidle_blocking_stat[i][k] != 0)
				mt_idle_log("%-2d: %d, ",
					    k, dpidle_blocking_stat[i][k]);
			dpidle_blocking_stat[i][k] = 0;
		}
		mt_idle_log("\n");
	}

	mt_idle_log("dpidle_clkmux_cond = %d\n", clkmux_cond[IDLE_TYPE_DP]);
	mt_idle_log("dpidle_vcore_cond = %d\n", vcore_cond[IDLE_TYPE_DP]);
	for (i = 0; i < NF_CLK_CFG; i++)
		mt_idle_log("[%02d]block_cond(0x%08x)=0x%08x\n",
			    i, clkmux_addr[i],
			    clkmux_block_mask[IDLE_TYPE_DP][i]);

	mt_idle_log("dpidle_by_pass_cg=%u\n", dpidle_by_pass_cg);
	mt_idle_log("dpidle_by_pass_pg=%u\n", dpidle_by_pass_pg);
	mt_idle_log("dpidle_dump_log = %u\n", dpidle_dump_log);
	mt_idle_log("([0]: Reduced, [1]: Full, [2]: resource_usage\n");

	mt_idle_log("\n*********** dpidle command help  ************\n");
	mt_idle_log("dpidle help:   cat %s/dpidle_state\n", SYSFS_ENTRY);
	mt_idle_log("switch on/off: ");
	mt_idle_log("echo [dpidle] 1/0 > %s/dpidle_state\n", SYSFS_ENTRY);
	mt_idle_log("cpupdn on/off: ");
	mt_idle_log("echo cpupdn 1/0 > %s/dpidle_state\n", SYSFS_ENTRY);
	mt_idle_log("en_dp_by_bit:  ");
	mt_idle_log("echo enable id > %s/dpidle_state\n", SYSFS_ENTRY);
	mt_idle_log("dis_dp_by_bit: ");
	mt_idle_log("echo disable id > %s/dpidle_state\n", SYSFS_ENTRY);
	mt_idle_log("modify tm_cri: ");
	mt_idle_log("echo time value(dec) > %s/dpidle_state\n", SYSFS_ENTRY);
	mt_idle_log("bypass cg:     ");
	mt_idle_log("echo bypass 1/0 > %s/dpidle_state\n", SYSFS_ENTRY);

	return p - ToUserBuf;
}

static ssize_t dpidle_state_write(char *FromUserBuf, size_t sz, void *priv)
{
	char cmd[128];
	int param;

	if (sscanf(FromUserBuf, "%127s %d", cmd, &param) == 2) {
		if (!strcmp(cmd, "dpidle"))
			idle_switch[IDLE_TYPE_DP] = param;
		else if (!strcmp(cmd, "enable"))
			enable_idle_by_bit(IDLE_TYPE_DP, param);
		else if (!strcmp(cmd, "disable"))
			disable_idle_by_bit(IDLE_TYPE_DP, param);
		else if (!strcmp(cmd, "once"))
			dpidle_run_once = param;
		else if (!strcmp(cmd, "time"))
			dpidle_time_criteria = param;
		else if (!strcmp(cmd, "bypass"))
			dpidle_by_pass_cg = param;
		else if (!strcmp(cmd, "bypass_pg")) {
			dpidle_by_pass_pg = param;
			idle_warn("bypass_pg = %d\n", dpidle_by_pass_pg);
		} else if (!strcmp(cmd, "golden")) {
			dpidle_gs_dump_req = param;

			if (dpidle_gs_dump_req)
				dpidle_gs_dump_req_ts =
					idle_get_current_time_ms();
		} else if (!strcmp(cmd, "golden_delay_ms")) {
			dpidle_gs_dump_delay_ms = (param >= 0) ? param : 0;
		} else if (!strcmp(cmd, "profile_sampling"))
			dpidle_set_profile_sampling(param);
		else if (!strcmp(cmd, "dump_profile_result"))
			(param) ? dpidle_show_profile_result() : 0;
		else if (!strcmp(cmd, "log"))
			dpidle_dump_log = param;

		return sz;
	} else if ((!kstrtoint(FromUserBuf, 10, &param)) == 1) {
		idle_switch[IDLE_TYPE_DP] = param;

		return sz;
	}

	return -EINVAL;
}

static const struct mtk_lp_sysfs_op dpidle_state_fops = {
	.fs_read = dpidle_state_read,
	.fs_write = dpidle_state_write,
};

/* soidle3_state */
static ssize_t soidle3_state_read(char *ToUserBuf, size_t sz_t, void *priv)
{
	int i;
	char *p = ToUserBuf;
	size_t sz = sz_t;

	#undef mt_idle_log
	#define mt_idle_log(fmt, args...) \
	do { \
		int l = scnprintf(p, sz, fmt, ##args); \
		p += l; \
		sz -= l; \
	} while (0)


	mt_idle_log("*********** soidle3 state ************\n");

	for (i = 0; i < NR_REASONS; i++)
		mt_idle_log("[%d]soidle3_block_cnt[%s]=%lu\n",
			    i, mtk_get_reason_name(i), soidle3_block_cnt[i]);
	mt_idle_log("\n");

	for (i = 0; i < NR_PLLS; i++) {
		mt_idle_log(
		"[%02d]soidle3_pll_condition_mask[%-8s]=0x%08x\t\t",
			    i, mtk_get_pll_group_name(i),
			    soidle3_pll_condition_mask[i]);
		mt_idle_log("soidle3_pll_block_mask[%-8s]=0x%08x\n",
			    mtk_get_pll_group_name(i),
			    soidle3_pll_block_mask[i]);
	}
	mt_idle_log("\n");

	for (i = 0; i < NR_GRPS; i++) {
		mt_idle_log("[%02d]soidle3_condition_mask[%-10s]=0x%08x\t\t",
			    i, mtk_get_cg_group_name(i),
			    idle_condition_mask[IDLE_TYPE_SO3][i]);
		mt_idle_log("soidle3_block_mask[%-10s]=0x%08x\n",
			    mtk_get_cg_group_name(i),
			    idle_block_mask[IDLE_TYPE_SO3][i]);
	}

	mt_idle_log("soidle3 pg_stat=0x%08x\n",
		    idle_block_mask[IDLE_TYPE_SO3][NR_GRPS + 1]);

	mt_idle_log("sodi3_clkmux_cond = %d\n",  clkmux_cond[IDLE_TYPE_SO3]);
	mt_idle_log("sodi3_vcore_cond = %d\n", vcore_cond[IDLE_TYPE_SO3]);
	for (i = 0; i < NF_CLK_CFG; i++)
		mt_idle_log("[%02d]block_cond(0x%08x)=0x%08x\n",
				i, clkmux_addr[i],
				clkmux_block_mask[IDLE_TYPE_SO3][i]);

	mt_idle_log("soidle3_bypass_pll=%u\n", soidle3_by_pass_pll);
	mt_idle_log("soidle3_bypass_cg=%u\n", soidle3_by_pass_cg);
	mt_idle_log("soidle3_bypass_en=%u\n", soidle3_by_pass_en);
	mt_idle_log("soidle3_by_pass_disp_pwm=%u\n", soidle3_by_pass_disp_pwm);
	mt_idle_log("sodi3_flags=0x%x\n", sodi3_flags);

	mt_idle_log("\n*********** soidle3 command help  ************\n");
	mt_idle_log("soidle3 help:  cat %s/soidle3_state\n", SYSFS_ENTRY);
	mt_idle_log("switch on/off: ");
	mt_idle_log("echo [soidle3] 1/0 > %s/soidle3_state\n", SYSFS_ENTRY);
	mt_idle_log("en_dp_by_bit:  ");
	mt_idle_log("echo enable id > %s/soidle3_state\n", SYSFS_ENTRY);
	mt_idle_log("dis_dp_by_bit: ");
	mt_idle_log("echo disable id > %s/soidle3_state\n", SYSFS_ENTRY);
	mt_idle_log("bypass pll:    ");
	mt_idle_log("echo bypass_pll 1/0 > %s/soidle3_state\n", SYSFS_ENTRY);
	mt_idle_log("bypass cg:     ");
	mt_idle_log("echo bypass 1/0 > %s/soidle3_state\n", SYSFS_ENTRY);
	mt_idle_log("bypass en:     ");
	mt_idle_log("echo bypass_en 1/0 > %s/soidle3_state\n", SYSFS_ENTRY);
	mt_idle_log("sodi3 flags:   ");
	mt_idle_log("echo sodi3_flags value > %s/soidle3_state\n", SYSFS_ENTRY);
	mt_idle_log("\t[0] reduce log, [1] residency, [2] resource usage\n");

	return p - ToUserBuf;
}

static ssize_t soidle3_state_write(char *FromUserBuf, size_t sz, void *priv)
{
	char cmd[128];
	int param;

	if (sscanf(FromUserBuf, "%127s %d", cmd, &param) == 2) {
		if (!strcmp(cmd, "soidle3"))
			idle_switch[IDLE_TYPE_SO3] = param;
		else if (!strcmp(cmd, "enable"))
			enable_idle_by_bit(IDLE_TYPE_SO3, param);
		else if (!strcmp(cmd, "disable"))
			disable_idle_by_bit(IDLE_TYPE_SO3, param);
		else if (!strcmp(cmd, "bypass_pll")) {
			soidle3_by_pass_pll = param;
			idle_dbg("bypass_pll = %d\n", soidle3_by_pass_pll);
		} else if (!strcmp(cmd, "bypass")) {
			soidle3_by_pass_cg = param;
			idle_dbg("bypass = %d\n", soidle3_by_pass_cg);
		} else if (!strcmp(cmd, "bypass_en")) {
			soidle3_by_pass_en = param;
			idle_dbg("bypass_en = %d\n", soidle3_by_pass_en);
		} else if (!strcmp(cmd, "bypass_pwm")) {
			soidle3_by_pass_disp_pwm = param;
			idle_dbg("bypass_pwm = %d\n", soidle3_by_pass_disp_pwm);
		} else if (!strcmp(cmd, "sodi3_flags")) {
			sodi3_flags = param;
			idle_dbg("sodi3_flags = 0x%x\n", sodi3_flags);
		}
		return sz;
	} else if ((!kstrtoint(FromUserBuf, 10, &param)) == 1) {
		idle_switch[IDLE_TYPE_SO3] = param;
		return sz;
	}

	return -EINVAL;
}

static const struct mtk_lp_sysfs_op soidle3_state_fops = {
	.fs_read = soidle3_state_read,
	.fs_write = soidle3_state_write,
};


/* soidle_state */
static ssize_t soidle_state_read(char *ToUserBuf, size_t sz_t, void *priv)
{
	int i;
	char *p = ToUserBuf;
	size_t sz  = sz_t;

	#undef mt_idle_log
	#define mt_idle_log(fmt, args...) \
	do { \
		int l = scnprintf(p, sz, fmt, ##args); \
		p += l; \
		sz -= l; \
	} while (0)

	mt_idle_log("*********** soidle state ************\n");
	for (i = 0; i < NR_REASONS; i++)
		mt_idle_log("[%d]soidle_block_cnt[%s]=%lu\n",
			    i, mtk_get_reason_name(i),
			    soidle_block_cnt[i]);
	mt_idle_log("\n");

	for (i = 0; i < NR_GRPS; i++) {
		mt_idle_log("[%02d]soidle_condition_mask[%-10s]=0x%08x\t\t",
			    i, mtk_get_cg_group_name(i),
			    idle_condition_mask[IDLE_TYPE_SO][i]);
		mt_idle_log("soidle_block_mask[%-10s]=0x%08x\n",
			    mtk_get_cg_group_name(i),
			    idle_block_mask[IDLE_TYPE_SO][i]);
	}

	mt_idle_log("soidle pg_stat=0x%08x\n",
		    idle_block_mask[IDLE_TYPE_SO][NR_GRPS + 1]);

	mt_idle_log("soidle_bypass_cg=%u\n", soidle_by_pass_cg);
	mt_idle_log("soidle_by_pass_pg=%u\n", soidle_by_pass_pg);
	mt_idle_log("soidle_bypass_en=%u\n", soidle_by_pass_en);
	mt_idle_log("sodi_flags=0x%x\n", sodi_flags);

	mt_idle_log("\n*********** soidle command help  ************\n");
	mt_idle_log("soidle help:   ");
	mt_idle_log("cat %s/soidle_state\n", SYSFS_ENTRY);
	mt_idle_log("switch on/off: ");
	mt_idle_log("echo [soidle] 1/0 > %s/soidle_state\n", SYSFS_ENTRY);
	mt_idle_log("en_dp_by_bit:  ");
	mt_idle_log("echo enable id > %s/soidle_state\n", SYSFS_ENTRY);
	mt_idle_log("dis_dp_by_bit: ");
	mt_idle_log("echo disable id > %s/soidle_state\n", SYSFS_ENTRY);
	mt_idle_log("bypass cg:     ");
	mt_idle_log("echo bypass 1/0 > %s/soidle_state\n", SYSFS_ENTRY);
	mt_idle_log("bypass en:     ");
	mt_idle_log("echo bypass_en 1/0 > %s/soidle_state\n", SYSFS_ENTRY);
	mt_idle_log("sodi flags:    ");
	mt_idle_log("echo sodi_flags value > %s/soidle_state\n", SYSFS_ENTRY);
	mt_idle_log("\t[0] reduce log, [1] residency, [2] resource usage\n");

	return p - ToUserBuf;
}


static ssize_t soidle_state_write(char *FromUserBuf, size_t sz, void *priv)
{
	char cmd[128];
	int param;

	if (sscanf(FromUserBuf, "%127s %d", cmd, &param) == 2) {
		if (!strcmp(cmd, "soidle"))
			idle_switch[IDLE_TYPE_SO] = param;
		else if (!strcmp(cmd, "enable"))
			enable_idle_by_bit(IDLE_TYPE_SO, param);
		else if (!strcmp(cmd, "disable"))
			disable_idle_by_bit(IDLE_TYPE_SO, param);
		else if (!strcmp(cmd, "bypass")) {
			soidle_by_pass_cg = param;
			idle_dbg("bypass = %d\n", soidle_by_pass_cg);
		} else if (!strcmp(cmd, "bypass_pg")) {
			soidle_by_pass_pg = param;
			idle_warn("bypass_pg = %d\n", soidle_by_pass_pg);
		} else if (!strcmp(cmd, "bypass_en")) {
			soidle_by_pass_en = param;
			idle_dbg("bypass_en = %d\n", soidle_by_pass_en);
		} else if (!strcmp(cmd, "sodi_flags")) {
			sodi_flags = param;
			idle_dbg("sodi_flags = 0x%x\n", sodi_flags);
		}
		return sz;
	} else if ((!kstrtoint(FromUserBuf, 10, &param)) == 1) {
		idle_switch[IDLE_TYPE_SO] = param;
		return sz;
	}

	return -EINVAL;
}

static const struct mtk_lp_sysfs_op soidle_state_fops = {
	.fs_read = soidle_state_read,
	.fs_write = soidle_state_write,
};

static int mtk_cpuidle_debugfs_init(void)
{
	struct mtk_lp_sysfs_handle *pParent = NULL;
	struct mtk_lp_sysfs_handle entry_cpuidle;

	/* create /sys/kernel/mtk_lpm as root */
	mtk_idle_sysfs_root_entry_create();

	/* create /sys/kernel/mtk_lpm/cpuidle */
	if (mtk_idle_sysfs_entry_root_get(&pParent) == 0) {
		mtk_lp_sysfs_entry_func_create("cpuidle",
			0444, pParent, &entry_cpuidle);
		mtk_lp_sysfs_entry_func_node_add("idle_state",
			0644, &idle_state_fops, &entry_cpuidle, NULL);
		mtk_lp_sysfs_entry_func_node_add("soidle_state",
			0644, &soidle_state_fops, &entry_cpuidle, NULL);
		mtk_lp_sysfs_entry_func_node_add("dpidle_state",
			0644, &dpidle_state_fops, &entry_cpuidle, NULL);
		mtk_lp_sysfs_entry_func_node_add("soidle3_state",
			0644, &soidle3_state_fops, &entry_cpuidle, NULL);
	}

	return 0;
}

#if defined(CONFIG_MACH_MT6763)
void mtk_spm_dump_debug_info(void)
{
	printk_deferred("[name:spm&]SPM_POWER_ON_VAL0     0x%08x\n",
			spm_read(SPM_POWER_ON_VAL0));
	printk_deferred("[name:spm&]SPM_POWER_ON_VAL1     0x%08x\n",
			spm_read(SPM_POWER_ON_VAL1));
	printk_deferred("[name:spm&]PCM_PWR_IO_EN         0x%08x\n",
			spm_read(PCM_PWR_IO_EN));
	printk_deferred("[name:spm&]PCM_REG0_DATA         0x%08x\n",
			spm_read(PCM_REG0_DATA));
	printk_deferred("[name:spm&]PCM_REG7_DATA         0x%08x\n",
			spm_read(PCM_REG7_DATA));
	printk_deferred("[name:spm&]PCM_REG12_DATA        0x%08x\n",
			spm_read(PCM_REG12_DATA));
	printk_deferred("[name:spm&]PCM_REG13_DATA        0x%08x\n",
			spm_read(PCM_REG13_DATA));
	printk_deferred("[name:spm&]PCM_REG15_DATA        0x%08x\n",
			spm_read(PCM_REG15_DATA));
	printk_deferred("[name:spm&]SPM_MAS_PAUSE_MASK_B  0x%08x\n",
		spm_read(SPM_MAS_PAUSE_MASK_B));
	printk_deferred("[name:spm&]SPM_MAS_PAUSE2_MASK_B 0x%08x\n",
		spm_read(SPM_MAS_PAUSE2_MASK_B));
	printk_deferred("[name:spm&]SPM_SW_FLAG           0x%08x\n",
			spm_read(SPM_SW_FLAG));
	printk_deferred("[name:spm&]SPM_DEBUG_FLAG        0x%08x\n",
			spm_read(SPM_SW_DEBUG));
	printk_deferred("[name:spm&]SPM_PC_TRACE_G0       0x%08x\n",
			spm_read(SPM_PC_TRACE_G0));
	printk_deferred("[name:spm&]SPM_PC_TRACE_G1       0x%08x\n",
			spm_read(SPM_PC_TRACE_G1));
	printk_deferred("[name:spm&]SPM_PC_TRACE_G2       0x%08x\n",
			spm_read(SPM_PC_TRACE_G2));
	printk_deferred("[name:spm&]SPM_PC_TRACE_G3       0x%08x\n",
			spm_read(SPM_PC_TRACE_G3));
	printk_deferred("[name:spm&]SPM_PC_TRACE_G4       0x%08x\n",
			spm_read(SPM_PC_TRACE_G4));
	printk_deferred("[name:spm&]SPM_PC_TRACE_G5       0x%08x\n",
			spm_read(SPM_PC_TRACE_G5));
	printk_deferred("[name:spm&]SPM_PC_TRACE_G6       0x%08x\n",
			spm_read(SPM_PC_TRACE_G6));
	printk_deferred("[name:spm&]SPM_PC_TRACE_G7       0x%08x\n",
			spm_read(SPM_PC_TRACE_G7));
	printk_deferred("[name:spm&]DCHA_GATING_LATCH_0   0x%08x\n",
		spm_read(DCHA_GATING_LATCH_0));
	printk_deferred("[name:spm&]DCHA_GATING_LATCH_5   0x%08x\n",
		spm_read(DCHA_GATING_LATCH_5));
	printk_deferred("[name:spm&]DCHB_GATING_LATCH_0   0x%08x\n",
		spm_read(DCHB_GATING_LATCH_0));
	printk_deferred("[name:spm&]DCHB_GATING_LATCH_5   0x%08x\n",
		spm_read(DCHB_GATING_LATCH_5));
}
#endif

void mtk_idle_gpt_init(void)
{
#ifndef USING_STD_TIMER_OPS
	int err = 0;

	err = request_gpt(IDLE_GPT, GPT_ONE_SHOT,
			  GPT_CLK_SRC_SYS, GPT_CLK_DIV_1,
			  0, NULL, GPT_NOAUTOEN);
	if (err)
		idle_warn("[%s] fail to request GPT %d\n",
			  __func__, IDLE_GPT + 1);
#endif
}

static void mtk_idle_profile_init(void)
{
	mtk_idle_twam_init();
	mtk_idle_block_setting(IDLE_TYPE_DP, dpidle_cnt,
			       dpidle_block_cnt,
			       idle_block_mask[IDLE_TYPE_DP]);
	mtk_idle_block_setting(IDLE_TYPE_SO3, soidle3_cnt,
			       soidle3_block_cnt,
			       idle_block_mask[IDLE_TYPE_SO3]);
	mtk_idle_block_setting(IDLE_TYPE_SO, soidle_cnt,
			       soidle_block_cnt,
			       idle_block_mask[IDLE_TYPE_SO]);
	mtk_idle_block_setting(IDLE_TYPE_RG, rgidle_cnt, NULL, NULL);
}

void mtk_idle_set_clkmux_addr(void)
{
	unsigned int i;

	for (i = 0; i < NF_CLK_CFG; i++)
		clkmux_addr[i] = 0x10000040 + i * 0x10;
}

void __init mtk_cpuidle_framework_init(void)
{
	#if !defined(CONFIG_MACH_MT6739)
	idle_ver("[%s]entry!!\n", __func__);
	#endif

#if defined(CONFIG_MACH_MT6763)
#ifdef CONFIG_BUILD_ARM64_APPENDED_DTB_IMAGE_NAMES
	/* Note: disable sodi/dpidle in mduse projects.
	 * k63v1_64_mduse
	 * k63v1_64_op01_mduse
	 * k63v1_64_op02_lwtg_mduse
	 * k63v1_64_op09_lwcg_mduse
	 */
	if (strstr(CONFIG_BUILD_ARM64_APPENDED_DTB_IMAGE_NAMES,
		   "_mduse") != NULL) {

		idle_switch[IDLE_TYPE_DP] = 0;
		idle_switch[IDLE_TYPE_SO3] = 0;
		idle_switch[IDLE_TYPE_SO] = 0;
	}
#endif
#endif

#if defined(CONFIG_MACH_MT6739)
#if defined(CONFIG_MTK_PMIC) || defined(CONFIG_MTK_PMIC_NEW_ARCH)
	if (PMIC_LP_CHIP_VER() == 1) {
		/* Note: For E1, disable Vproc/Vproc_sram DVS for SO/SO3/DP. */
		slp_spm_SODI3_flags |= SPM_FLAG_DIS_VPROC_VSRAM_DVS;
		slp_spm_SODI_flags |= SPM_FLAG_DIS_VPROC_VSRAM_DVS;
		slp_spm_deepidle_flags |= SPM_FLAG_DIS_VPROC_VSRAM_DVS;
		/* Note: For E1, disable Vcore_sram adjustment */
		slp_spm_SODI3_flags |= SPM_FLAG_DISABLE_VCORE_SRAM_ADJ;
		slp_spm_SODI_flags |= SPM_FLAG_DISABLE_VCORE_SRAM_ADJ;
		slp_spm_deepidle_flags |= SPM_FLAG_DISABLE_VCORE_SRAM_ADJ;
	}
#endif
#endif

	iomap_init();
	mtk_cpuidle_debugfs_init();

	mtk_idle_gpt_init();

	dpidle_by_pass_pg = false;

	mtk_idle_set_clkmux_addr();
	mtk_idle_profile_init();
}
EXPORT_SYMBOL(mtk_cpuidle_framework_init);

