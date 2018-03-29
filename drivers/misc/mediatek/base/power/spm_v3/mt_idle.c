/*
 * Copyright (C) 2016 MediaTek Inc.
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

#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/kallsyms.h>

#include <linux/irqchip/mt-gic.h>

#include <asm/system_misc.h>
#include <mt-plat/sync_write.h>
#include <mach/mt_gpt.h>
#include <mach/mt_cpuxgpt.h>
#include "mt_spm.h"
#include "mt_spm_idle.h"
#include "hotplug.h"
#ifdef CONFIG_THERMAL
#include <mach/mt_thermal.h>
#endif
#include "mt_idle.h"
#include "mt_idle_internal.h"
#include <mach/mt_spm_mtcmos_internal.h>
#include "mt_spm_reg.h"
#include "mt_cpufreq_hybrid.h"

#include <linux/uaccess.h>

#define FEATURE_ENABLE_SODI2P5

#define IDLE_TAG     "Power/swap "
#define spm_emerg(fmt, args...)		pr_emerg(IDLE_TAG fmt, ##args)
#define spm_alert(fmt, args...)		pr_alert(IDLE_TAG fmt, ##args)
#define spm_crit(fmt, args...)		pr_crit(IDLE_TAG fmt, ##args)
#define idle_err(fmt, args...)		pr_err(IDLE_TAG fmt, ##args)
#define idle_warn(fmt, args...)		pr_warn(IDLE_TAG fmt, ##args)
#define spm_notice(fmt, args...)	pr_notice(IDLE_TAG fmt, ##args)
#define idle_info(fmt, args...)		pr_debug(IDLE_TAG fmt, ##args)
#define idle_ver(fmt, args...)		pr_debug(IDLE_TAG fmt, ##args)
#define idle_dbg(fmt, args...)		pr_debug(IDLE_TAG fmt, ##args)

#define idle_warn_log(fmt, args...) { \
	if (dpidle_dump_log == DEEPIDLE_LOG_FULL) \
		pr_warn(IDLE_TAG fmt, ##args); \
	}

#define idle_gpt GPT4

#define idle_writel(addr, val)   \
	mt65xx_reg_sync_writel(val, addr)

#define idle_setl(addr, val) \
	mt65xx_reg_sync_writel(idle_readl(addr) | (val), addr)

#define idle_clrl(addr, val) \
	mt65xx_reg_sync_writel(idle_readl(addr) & ~(val), addr)

#define log2buf(p, s, fmt, args...) \
	(p += snprintf(p, sizeof(s) - strlen(s), fmt, ##args))

static atomic_t is_in_hotplug = ATOMIC_INIT(0);

static bool mt_idle_chk_golden;
static bool mt_dpidle_chk_golden;

#ifdef CONFIG_HYBRID_CPU_DVFS
static bool mt_dvfsp_paused_by_idle;
#endif

#define NR_CMD_BUF		128

void go_to_wfi(void)
{
	isb();
	mb();
	__asm__ __volatile__("wfi" : : : "memory");
}

/* FIXME: early porting */
#if 1
void __attribute__((weak))
bus_dcm_enable(void)
{
	/* FIXME: early porting */
}
void __attribute__((weak))
bus_dcm_disable(void)
{
	/* FIXME: early porting */
}

unsigned int __attribute__((weak))
mt_get_clk_mem_sel(void)
{
	return 1;
}

void __attribute__((weak))
tscpu_cancel_thermal_timer(void)
{

}
void __attribute__((weak))
tscpu_start_thermal_timer(void)
{
	/* FIXME: early porting */
}

void __attribute__((weak)) mtkts_bts_cancel_thermal_timer(void)
{

}

void __attribute__((weak)) mtkts_btsmdpa_cancel_thermal_timer(void)
{

}

void __attribute__((weak)) mtkts_pmic_cancel_thermal_timer(void)
{

}

void __attribute__((weak)) mtkts_battery_cancel_thermal_timer(void)
{

}

void __attribute__((weak)) mtkts_pa_cancel_thermal_timer(void)
{

}

void __attribute__((weak)) mtkts_wmt_cancel_thermal_timer(void)
{

}

void __attribute__((weak)) mtkts_bts_start_thermal_timer(void)
{

}

void __attribute__((weak)) mtkts_btsmdpa_start_thermal_timer(void)
{

}

void __attribute__((weak)) mtkts_pmic_start_thermal_timer(void)
{

}

void __attribute__((weak)) mtkts_battery_start_thermal_timer(void)
{

}

void __attribute__((weak)) mtkts_pa_start_thermal_timer(void)
{

}

void __attribute__((weak)) mtkts_wmt_start_thermal_timer(void)
{

}

void __attribute__((weak)) mtkts_allts_cancel_ts1_timer(void)
{

}

void __attribute__((weak)) mtkts_allts_cancel_ts2_timer(void)
{

}

void __attribute__((weak)) mtkts_allts_cancel_ts3_timer(void)
{

}

void __attribute__((weak)) mtkts_allts_cancel_ts4_timer(void)
{

}

void __attribute__((weak)) mtkts_allts_cancel_ts5_timer(void)
{

}

void __attribute__((weak)) mtkts_allts_start_ts1_timer(void)
{

}

void __attribute__((weak)) mtkts_allts_start_ts2_timer(void)
{

}

void __attribute__((weak)) mtkts_allts_start_ts3_timer(void)
{

}

void __attribute__((weak)) mtkts_allts_start_ts4_timer(void)
{

}

void __attribute__((weak)) mtkts_allts_start_ts5_timer(void)
{

}

bool __attribute__((weak)) mtk_gpu_sodi_entry(void)
{
	return false;
}

bool __attribute__((weak)) mtk_gpu_sodi_exit(void)
{
	return false;
}

bool __attribute__((weak)) spm_mcdi_can_enter(void)
{
	return false;
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

void __attribute__((weak)) msdc_clk_status(int *status)
{
	*status = 0x1;
}

wake_reason_t __attribute__((weak)) spm_go_to_dpidle(u32 spm_flags, u32 spm_data, u32 sodi_flags)
{
	go_to_wfi();

	return WR_NONE;
}

void __attribute__((weak)) spm_enable_sodi(bool en)
{

}

void __attribute__((weak)) spm_sodi_mempll_pwr_mode(bool pwr_mode)
{

}

wake_reason_t __attribute__((weak)) spm_go_to_sodi3(u32 spm_flags, u32 spm_data, u32 sodi_flags)
{
	return WR_UNKNOWN;
}

wake_reason_t __attribute__((weak)) spm_go_to_sodi(u32 spm_flags, u32 spm_data, u32 sodi_flags)
{
	return WR_UNKNOWN;
}

bool __attribute__((weak)) go_to_mcidle(int cpu)
{
	return false;
}

void __attribute__((weak)) spm_mcdi_switch_on_off(enum spm_mcdi_lock_id id, int mcdi_en)
{

}

int __attribute__((weak)) is_teei_ready(void)
{
	return 1;
}

#endif

static char log_buf[500];
static char log_buf_2[500];

static unsigned long long idle_block_log_prev_time;
static unsigned int idle_block_log_time_criteria = 5000;	/* 5 sec */
static unsigned long long idle_cnt_dump_prev_time;
static unsigned int idle_cnt_dump_criteria = 10000;			/* 10 sec */

static bool             idle_ratio_en;
static unsigned long long idle_ratio_profile_start_time;
static unsigned long long idle_ratio_profile_duration;
static unsigned long long idle_ratio_start_time[NR_TYPES];
static unsigned long long idle_ratio_value[NR_TYPES];


static unsigned int idle_block_mask[NR_TYPES][NR_GRPS+1];

/* DeepIdle */
static unsigned int     dpidle_time_critera = 2000; /* 2ms */
static unsigned int     dpidle_block_time_critera = 30000; /* 30sec */
static unsigned long    dpidle_cnt[NR_CPUS] = {0};
static unsigned long    dpidle_last_cnt[NR_CPUS] = {0};
static unsigned long    dpidle_f26m_cnt[NR_CPUS] = {0};
static unsigned long    dpidle_block_cnt[NR_REASONS] = {0};
static unsigned long long dpidle_block_prev_time;
static bool             dpidle_by_pass_cg;
static bool             dpidle_by_pass_i2c_appm_cg;
bool                    dpidle_by_pass_pg;
static unsigned int     dpidle_dump_log = DEEPIDLE_LOG_REDUCED;
#ifdef SPM_DEEPIDLE_PROFILE_TIME
unsigned int			dpidle_profile[4];
#endif

/* SODI3 */
static unsigned int     soidle3_pll_block_mask[NR_PLLS] = {0x0};
static unsigned int     soidle3_time_critera = 5000; /* 5ms */
static unsigned int     soidle3_block_time_critera = 30000; /* 30sec */
static unsigned long    soidle3_cnt[NR_CPUS] = {0};
static unsigned long    soidle3_last_cnt[NR_CPUS] = {0};
static unsigned long    soidle3_block_cnt[NR_REASONS] = {0};
static unsigned long long soidle3_block_prev_time;
static bool             soidle3_by_pass_cg;
static bool             soidle3_by_pass_i2c_appm_cg;
static bool             soidle3_by_pass_pll;
static bool             soidle3_by_pass_en;
static u32				sodi3_flags = SODI_FLAG_REDUCE_LOG|SODI_FLAG_V3;
#ifdef SPM_SODI3_PROFILE_TIME
unsigned int			soidle3_profile[4];
#endif

/* SODI */
static unsigned int     soidle_time_critera = 2000; /* 2ms */
static unsigned int     soidle_block_time_critera = 30000; /* 30sec */
static unsigned long    soidle_cnt[NR_CPUS] = {0};
static unsigned long    soidle_last_cnt[NR_CPUS] = {0};
static unsigned long    soidle_block_cnt[NR_REASONS] = {0};
static unsigned long long soidle_block_prev_time;
static bool             soidle_by_pass_cg;
static bool             soidle_by_pass_i2c_appm_cg;
bool                    soidle_by_pass_pg;
static bool             soidle_by_pass_en;
static u32				sodi_flags = SODI_FLAG_REDUCE_LOG;
#ifdef SPM_SODI_PROFILE_TIME
unsigned int			soidle_profile[4];
#endif

/* MCDI */
static unsigned int     mcidle_time_critera = 3000; /* 3ms */
static unsigned long    mcidle_cnt[NR_CPUS] = {0};
static unsigned long    mcidle_block_cnt[NR_CPUS][NR_REASONS] = { {0}, {0} };

/* Slow Idle */
static unsigned long    slidle_cnt[NR_CPUS] = {0};
static unsigned long    slidle_block_cnt[NR_REASONS] = {0};

/* Regular Idle */
static unsigned long    rgidle_cnt[NR_CPUS] = {0};

/* idle_notifier */
static ATOMIC_NOTIFIER_HEAD(mtk_idle_notifier);

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
		pr_err("[mt_idle_ntf] <%02d>%08lx (%s)\n",
			index++, (unsigned long)n->notifier_call, symname);
	} else {
		pr_err("[mt_idle_ntf] <%02d>%08lx\n",
			index++, (unsigned long)n->notifier_call);
	}
#else
	pr_err("[mt_idle_ntf] <%02d>%08lx\n",
			index++, (unsigned long)n->notifier_call);
#endif

	ret = atomic_notifier_chain_register(&mtk_idle_notifier, n);

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_idle_notifier_register);

void mtk_idle_notifier_unregister(struct notifier_block *n)
{
	atomic_notifier_chain_unregister(&mtk_idle_notifier, n);
}
EXPORT_SYMBOL_GPL(mtk_idle_notifier_unregister);

void mtk_idle_notifier_call_chain(unsigned long val)
{
	atomic_notifier_call_chain(&mtk_idle_notifier, val, NULL);
}
EXPORT_SYMBOL_GPL(mtk_idle_notifier_call_chain);

static long int idle_get_current_time_ms(void)
{
	struct timeval t;

	do_gettimeofday(&t);
	return ((t.tv_sec & 0xFFF) * 1000000 + t.tv_usec) / 1000;
}

static unsigned int get_next_event_time_us(void)
{
	struct timespec t;

	t = ktime_to_timespec(tick_nohz_get_sleep_length());
	return t.tv_sec * USEC_PER_SEC + t.tv_nsec / NSEC_PER_USEC;
}

static unsigned int idle_spm_lock;
static DEFINE_SPINLOCK(idle_spm_spin_lock);

void idle_lock_spm(enum idle_lock_spm_id id)
{
	unsigned long flags;

	spin_lock_irqsave(&idle_spm_spin_lock, flags);
	idle_spm_lock |= (1 << id);
	spin_unlock_irqrestore(&idle_spm_spin_lock, flags);
}

void idle_unlock_spm(enum idle_lock_spm_id id)
{
	unsigned long flags;

	spin_lock_irqsave(&idle_spm_spin_lock, flags);
	idle_spm_lock &= ~(1 << id);
	spin_unlock_irqrestore(&idle_spm_spin_lock, flags);
}

/************************************************
 * SODI3 part
 ************************************************/
static DEFINE_MUTEX(soidle3_locked);

static void enable_soidle3_by_mask(int grp, unsigned int mask)
{
	mutex_lock(&soidle3_locked);
	idle_condition_mask[IDLE_TYPE_SO3][grp] &= ~mask;
	mutex_unlock(&soidle3_locked);
}

static void disable_soidle3_by_mask(int grp, unsigned int mask)
{
	mutex_lock(&soidle3_locked);
	idle_condition_mask[IDLE_TYPE_SO3][grp] |= mask;
	mutex_unlock(&soidle3_locked);
}

void enable_soidle3_by_bit(int id)
{
	int grp = id / 32;
	unsigned int mask = 1U << (id % 32);

	BUG_ON(INVALID_GRP_ID(grp));
	enable_soidle3_by_mask(grp, mask);
}
EXPORT_SYMBOL(enable_soidle3_by_bit);

void disable_soidle3_by_bit(int id)
{
	int grp = id / 32;
	unsigned int mask = 1U << (id % 32);

	BUG_ON(INVALID_GRP_ID(grp));
	disable_soidle3_by_mask(grp, mask);
}
EXPORT_SYMBOL(disable_soidle3_by_bit);

#define clk_readl(addr)			__raw_readl((void __force __iomem *)(addr))
#define clk_writel(addr, val)	mt_reg_sync_writel(val, addr)

void faudintbus_pll2sq(void)
{
}

void faudintbus_sq2pll(void)
{
}

#ifndef CONFIG_MTK_FPGA
static bool mt_idle_cpu_criteria(void)
{
	unsigned int cpu_pwr_stat = 0;

	cpu_pwr_stat = spm_get_cpu_pwr_status();

	return ((cpu_pwr_stat == CPU_0) || (cpu_pwr_stat == CPU_4)) ? true : false;
}
#endif

static bool soidle3_can_enter(int cpu, int reason)
{
	int i;
	unsigned long long soidle3_block_curr_time = 0;
	char *p;
	bool ret = false;

	#ifdef SPM_SODI3_PROFILE_TIME
	gpt_get_cnt(SPM_SODI3_PROFILE_APXGPT, &soidle3_profile[0]);
	#endif

	/* check previous common criterion */
	if (reason == BY_CLK) {
		if (soidle3_by_pass_cg == 0) {
			if (idle_block_mask[IDLE_TYPE_SO3][NR_GRPS])
				goto out;
		}
	} else if (reason < NR_REASONS)
		goto out;

	reason = NR_REASONS;

	#ifndef CONFIG_MTK_FPGA
	if (soidle3_by_pass_en == 0) {
		if ((spm_get_sodi_en() == 0) || (spm_get_sodi3_en() == 0)) {
			/* if SODI is disabled, SODI3 is also disabled */
			reason = BY_OTH;
			goto out;
		}
	}

	if (!mt_idle_disp_is_pwm_rosc()) {
		reason = BY_PWM;
		goto out;
	}

	if (soidle3_by_pass_pll == 0) {
		if (!mt_idle_check_pll(soidle3_pll_condition_mask, soidle3_pll_block_mask)) {
			reason = BY_PLL;
			goto out;
		}
	}
	#endif

	if (get_next_event_time_us() < soidle3_time_critera) {
		reason = BY_TMR;
		goto out;
	}

#ifdef CONFIG_HYBRID_CPU_DVFS
	/* Try to pause DVFSP, xxidle will be blocked if DVFSP can NOT be paused */
	if (cpuhvfs_pause_dvfsp_running(PAUSE_IDLE) != 0) {
		reason = BY_DVFSP;
		goto out;
	}

	mt_dvfsp_paused_by_idle = true;
#endif

	if (soidle3_by_pass_i2c_appm_cg == 0) {
		/* Check if I2C-appm gated since DVFSP will control it */
		if (!mt_idle_check_cg_i2c_appm(idle_block_mask[IDLE_TYPE_SO3])) {
#ifdef CONFIG_HYBRID_CPU_DVFS
			cpuhvfs_unpause_dvfsp_to_run(PAUSE_IDLE);
			mt_dvfsp_paused_by_idle = false;
#endif
			reason = BY_CLK;
			goto out;
		}
	}

out:
	if (reason < NR_REASONS) {
		if (soidle3_block_prev_time == 0)
			soidle3_block_prev_time = idle_get_current_time_ms();

		soidle3_block_curr_time = idle_get_current_time_ms();
		if (((soidle3_block_curr_time - soidle3_block_prev_time) > soidle3_block_time_critera)
			&& ((soidle3_block_curr_time - idle_block_log_prev_time) > idle_block_log_time_criteria)) {

			if ((cpu % 4) == 0) {
				/* soidle3,rgidle count */
				p = log_buf;
				log2buf(p, log_buf, "CNT(soidle3,rgidle): ");

				for (i = 0; i < nr_cpu_ids; i++)
					log2buf(p, log_buf, "[%d] = (%lu,%lu), ",
						i, soidle3_cnt[i], rgidle_cnt[i]);
				idle_warn("%s\n", log_buf);

				/* block category */
				p = log_buf;
				log2buf(p, log_buf, "soidle3_block_cnt: ");
				for (i = 0; i < NR_REASONS; i++)
					log2buf(p, log_buf, "[%s] = %lu, ",
						mt_get_reason_name(i), soidle3_block_cnt[i]);
				idle_warn("%s\n", log_buf);

				p = log_buf;
				log2buf(p, log_buf, "soidle3_block_mask: ");
				for (i = 0; i < NR_GRPS; i++)
					log2buf(p, log_buf, "0x%08x, ", idle_block_mask[IDLE_TYPE_SO3][i]);
				idle_warn("%s\n", log_buf);

				memset(soidle3_block_cnt, 0, sizeof(soidle3_block_cnt));
				soidle3_block_prev_time = idle_get_current_time_ms();
				idle_block_log_prev_time = soidle3_block_prev_time;
			}
		}

		soidle3_block_cnt[reason]++;
		ret = false;
	} else {
		soidle3_block_prev_time = idle_get_current_time_ms();
		ret = true;
	}

	return ret;
}

void soidle3_before_wfi(int cpu)
{
}

void soidle3_after_wfi(int cpu)
{
#ifdef CONFIG_HYBRID_CPU_DVFS
	if (mt_dvfsp_paused_by_idle) {
		cpuhvfs_unpause_dvfsp_to_run(PAUSE_IDLE);
		mt_dvfsp_paused_by_idle = false;
	}
#endif

	soidle3_cnt[cpu]++;
}

/************************************************
 * SODI part
 ************************************************/
static DEFINE_MUTEX(soidle_locked);

static void enable_soidle_by_mask(int grp, unsigned int mask)
{
	mutex_lock(&soidle_locked);
	idle_condition_mask[IDLE_TYPE_SO][grp] &= ~mask;
	mutex_unlock(&soidle_locked);
}

static void disable_soidle_by_mask(int grp, unsigned int mask)
{
	mutex_lock(&soidle_locked);
	idle_condition_mask[IDLE_TYPE_SO][grp] |= mask;
	mutex_unlock(&soidle_locked);
}

void enable_soidle_by_bit(int id)
{
	int grp = id / 32;
	unsigned int mask = 1U << (id % 32);

	BUG_ON(INVALID_GRP_ID(grp));
	enable_soidle_by_mask(grp, mask);
	/* enable the settings for SODI3 at the same time */
	enable_soidle3_by_mask(grp, mask);
}
EXPORT_SYMBOL(enable_soidle_by_bit);

void disable_soidle_by_bit(int id)
{
	int grp = id / 32;
	unsigned int mask = 1U << (id % 32);

	BUG_ON(INVALID_GRP_ID(grp));
	disable_soidle_by_mask(grp, mask);
	/* disable the settings for SODI3 at the same time */
	disable_soidle3_by_mask(grp, mask);
}
EXPORT_SYMBOL(disable_soidle_by_bit);

static bool soidle_can_enter(int cpu, int reason)
{
	int i;
	unsigned long long soidle_block_curr_time = 0;
	char *p;
	bool ret = false;

	#ifdef SPM_SODI_PROFILE_TIME
	gpt_get_cnt(SPM_SODI_PROFILE_APXGPT, &soidle_profile[0]);
	#endif

	/* check previous common criterion */
	if (reason == BY_CLK) {
		if (soidle_by_pass_cg == 0) {
			if (idle_block_mask[IDLE_TYPE_SO][NR_GRPS])
				goto out;
		}
	} else if (reason < NR_REASONS)
		goto out;

	reason = NR_REASONS;

	#ifndef CONFIG_MTK_FPGA
	if (soidle_by_pass_en == 0) {
		if (spm_get_sodi_en() == 0) {
			reason = BY_OTH;
			goto out;
		}
	}
	#endif

	if (get_next_event_time_us() < soidle_time_critera) {
		reason = BY_TMR;
		goto out;
	}

	#ifndef CONFIG_MTK_FPGA
	#ifdef CONFIG_HYBRID_CPU_DVFS
	/* Try to pause DVFSP, xxidle will be blocked if DVFSP can NOT be paused */
	if (cpuhvfs_pause_dvfsp_running(PAUSE_IDLE) != 0) {
		reason = BY_DVFSP;
		goto out;
	}

	mt_dvfsp_paused_by_idle = true;
	#endif
	#endif

	#ifndef CONFIG_MTK_FPGA
	if (soidle_by_pass_i2c_appm_cg == 0) {
		/* Check if I2C-appm gated since DVFSP will control it */
		if (!mt_idle_check_cg_i2c_appm(idle_block_mask[IDLE_TYPE_SO])) {
	#ifdef CONFIG_HYBRID_CPU_DVFS
			cpuhvfs_unpause_dvfsp_to_run(PAUSE_IDLE);
			mt_dvfsp_paused_by_idle = false;
	#endif
			reason = BY_CLK;
			goto out;
		}
	}
	#endif

out:
	if (reason < NR_REASONS) {
		if (soidle_block_prev_time == 0)
			soidle_block_prev_time = idle_get_current_time_ms();

		soidle_block_curr_time = idle_get_current_time_ms();
		if (((soidle_block_curr_time - soidle_block_prev_time) > soidle_block_time_critera)
			&& ((soidle_block_curr_time - idle_block_log_prev_time) > idle_block_log_time_criteria)) {

			if ((cpu % 4) == 0) {
				/* soidle,rgidle count */
				p = log_buf;
				log2buf(p, log_buf, "CNT(soidle,rgidle): ");
				for (i = 0; i < nr_cpu_ids; i++)
					log2buf(p, log_buf, "[%d] = (%lu,%lu), ",
						i, soidle_cnt[i], rgidle_cnt[i]);
				idle_warn("%s\n", log_buf);

				/* block category */
				p = log_buf;
				log2buf(p, log_buf, "soidle_block_cnt: ");
				for (i = 0; i < NR_REASONS; i++)
					log2buf(p, log_buf, "[%s] = %lu, ",
						mt_get_reason_name(i), soidle_block_cnt[i]);
				idle_warn("%s\n", log_buf);

				p = log_buf;
				log2buf(p, log_buf, "soidle_block_mask: ");
				for (i = 0; i < NR_GRPS; i++)
					log2buf(p, log_buf, "0x%08x, ",
						idle_block_mask[IDLE_TYPE_SO][i]);
				idle_warn("%s\n", log_buf);

				memset(soidle_block_cnt, 0, sizeof(soidle_block_cnt));
				soidle_block_prev_time = idle_get_current_time_ms();
				idle_block_log_prev_time = soidle_block_prev_time;
			}
		}

		soidle_block_cnt[reason]++;
		ret = false;
	} else {
		soidle_block_prev_time = idle_get_current_time_ms();
		ret = true;
	}

	return ret;
}

void soidle_before_wfi(int cpu)
{
#ifdef FEATURE_ENABLE_SODI2P5
	faudintbus_pll2sq();
#endif
}

void soidle_after_wfi(int cpu)
{
#ifdef FEATURE_ENABLE_SODI2P5
	faudintbus_sq2pll();
#endif

#ifdef CONFIG_HYBRID_CPU_DVFS
	if (mt_dvfsp_paused_by_idle) {
		cpuhvfs_unpause_dvfsp_to_run(PAUSE_IDLE);
		mt_dvfsp_paused_by_idle = false;
	}
#endif

	soidle_cnt[cpu]++;
}

/************************************************
 * multi-core idle part
 ************************************************/
static DEFINE_MUTEX(mcidle_locked);
static bool mcidle_can_enter(int cpu, int reason)
{
	/* reset reason */
	reason = NR_REASONS;

#ifdef CONFIG_ARM64
	if (num_online_cpus() == 1) {
		reason = BY_CPU;
		goto mcidle_out;
	}
#else
	if (num_online_cpus() == 1) {
		reason = BY_CPU;
		goto mcidle_out;
	}
#endif

	if (spm_mcdi_can_enter() == 0) {
		reason = BY_OTH;
		goto mcidle_out;
	}

	if (get_next_event_time_us() < mcidle_time_critera) {
		reason = BY_TMR;
		goto mcidle_out;
	}

mcidle_out:
	if (reason < NR_REASONS) {
		mcidle_block_cnt[cpu][reason]++;
		return false;
	}

	return true;
}

void mcidle_before_wfi(int cpu)
{
}

int mcdi_xgpt_wakeup_cnt[NR_CPUS];
void mcidle_after_wfi(int cpu)
{
}

/************************************************
 * deep idle part
 ************************************************/
static DEFINE_MUTEX(dpidle_locked);

static void enable_dpidle_by_mask(int grp, unsigned int mask)
{
	mutex_lock(&dpidle_locked);
	idle_condition_mask[IDLE_TYPE_DP][grp] &= ~mask;
	mutex_unlock(&dpidle_locked);
}

static void disable_dpidle_by_mask(int grp, unsigned int mask)
{
	mutex_lock(&dpidle_locked);
	idle_condition_mask[IDLE_TYPE_DP][grp] |= mask;
	mutex_unlock(&dpidle_locked);
}

void enable_dpidle_by_bit(int id)
{
	int grp = id / 32;
	unsigned int mask = 1U << (id % 32);

	BUG_ON(INVALID_GRP_ID(grp));
	enable_dpidle_by_mask(grp, mask);
}
EXPORT_SYMBOL(enable_dpidle_by_bit);

void disable_dpidle_by_bit(int id)
{
	int grp = id / 32;
	unsigned int mask = 1U << (id % 32);

	BUG_ON(INVALID_GRP_ID(grp));
	disable_dpidle_by_mask(grp, mask);
}
EXPORT_SYMBOL(disable_dpidle_by_bit);

static bool dpidle_can_enter(int cpu, int reason)
{
	int i = 0;
	unsigned long long dpidle_block_curr_time = 0;
	char *p;
	bool ret = false;

	#ifdef SPM_DEEPIDLE_PROFILE_TIME
	gpt_get_cnt(SPM_PROFILE_APXGPT, &dpidle_profile[0]);
	#endif

	/* check previous common criterion */
	if (reason == BY_CLK) {
		if (dpidle_by_pass_cg == 0) {
			if (idle_block_mask[IDLE_TYPE_DP][NR_GRPS])
				goto out;
		}
	} else if (reason < NR_REASONS)
		goto out;

	reason = NR_REASONS;

	/* TODO: check if mt_cpufreq_earlysuspend_status_get() should be used */
#if 0
	if (dpidle_by_pass_cg == 0) {
		if (!mt_cpufreq_earlysuspend_status_get()) {
			reason = BY_VTG;
			goto out;
		}
	}
#endif

	if (get_next_event_time_us() < dpidle_time_critera) {
		reason = BY_TMR;
		goto out;
	}

#ifdef CONFIG_HYBRID_CPU_DVFS
	/* Try to pause DVFSP, xxidle will be blocked if DVFSP can NOT be paused */
	if (cpuhvfs_pause_dvfsp_running(PAUSE_IDLE) != 0) {
		reason = BY_DVFSP;
		goto out;
	}

	mt_dvfsp_paused_by_idle = true;
#endif

	if (dpidle_by_pass_i2c_appm_cg == 0) {
		/* Check if I2C-appm gated since DVFSP will control it */
		if (!mt_idle_check_cg_i2c_appm(idle_block_mask[IDLE_TYPE_DP])) {
#ifdef CONFIG_HYBRID_CPU_DVFS
			cpuhvfs_unpause_dvfsp_to_run(PAUSE_IDLE);
			mt_dvfsp_paused_by_idle = false;
#endif
			reason = BY_CLK;
			goto out;
		}
	}

out:
	if (reason < NR_REASONS) {
		if (dpidle_block_prev_time == 0)
			dpidle_block_prev_time = idle_get_current_time_ms();

		dpidle_block_curr_time = idle_get_current_time_ms();
		if (((dpidle_block_curr_time - dpidle_block_prev_time) > dpidle_block_time_critera)
			&& ((dpidle_block_curr_time - idle_block_log_prev_time) > idle_block_log_time_criteria)) {

			if ((cpu % 4) == 0) {
				/* dpidle,rgidle count */
				p = log_buf;
				log2buf(p, log_buf, "CNT(dpidle,rgidle): ");
				for (i = 0; i < nr_cpu_ids; i++)
					log2buf(p, log_buf, "[%d] = (%lu,%lu), ",
						i, dpidle_cnt[i], rgidle_cnt[i]);
				idle_warn("%s\n", log_buf);

				/* block category */
				p = log_buf;
				log2buf(p, log_buf, "dpidle_block_cnt: ");
				for (i = 0; i < NR_REASONS; i++)
					log2buf(p, log_buf, "[%s] = %lu, ",
						mt_get_reason_name(i), dpidle_block_cnt[i]);
				idle_warn("%s\n", log_buf);

				p = log_buf;
				log2buf(p, log_buf, "dpidle_block_mask: ");
				for (i = 0; i < NR_GRPS; i++)
					log2buf(p, log_buf, "0x%08x, ",
						idle_block_mask[IDLE_TYPE_DP][i]);
				idle_warn("%s\n", log_buf);

				memset(dpidle_block_cnt, 0, sizeof(dpidle_block_cnt));
				dpidle_block_prev_time = idle_get_current_time_ms();
				idle_block_log_prev_time = dpidle_block_prev_time;
			}
		}
		dpidle_block_cnt[reason]++;
		ret = false;
	} else {
		dpidle_block_prev_time = idle_get_current_time_ms();
		ret = true;
	}

	return ret;
}

/************************************************
 * slow idle part
 ************************************************/
static DEFINE_MUTEX(slidle_locked);

static void enable_slidle_by_mask(int grp, unsigned int mask)
{
	mutex_lock(&slidle_locked);
	idle_condition_mask[IDLE_TYPE_SL][grp] &= ~mask;
	mutex_unlock(&slidle_locked);
}

static void disable_slidle_by_mask(int grp, unsigned int mask)
{
	mutex_lock(&slidle_locked);
	idle_condition_mask[IDLE_TYPE_SL][grp] |= mask;
	mutex_unlock(&slidle_locked);
}

void enable_slidle_by_bit(int id)
{
	int grp = id / 32;
	unsigned int mask = 1U << (id % 32);

	BUG_ON(INVALID_GRP_ID(grp));
	enable_slidle_by_mask(grp, mask);
}
EXPORT_SYMBOL(enable_slidle_by_bit);

void disable_slidle_by_bit(int id)
{
	int grp = id / 32;
	unsigned int mask = 1U << (id % 32);

	BUG_ON(INVALID_GRP_ID(grp));
	disable_slidle_by_mask(grp, mask);
}
EXPORT_SYMBOL(disable_slidle_by_bit);

static bool slidle_can_enter(int cpu, int reason)
{
	/* check previous common criterion */
	/* reset reason if reason is not BY_CLK */
	if (reason == BY_CLK) {
		if (idle_block_mask[IDLE_TYPE_SL][NR_GRPS])
			goto out;
	}

	reason = NR_REASONS;

	#ifndef CONFIG_MTK_FPGA
	if (!mt_idle_cpu_criteria()) {
		reason = BY_CPU;
		goto out;
	}
	#endif

out:
	if (reason < NR_REASONS) {
		slidle_block_cnt[reason]++;
		return false;
	} else {
		return true;
	}
}

static void slidle_before_wfi(int cpu)
{
	/* struct mtk_irq_mask mask; */
	bus_dcm_enable();
}

static void slidle_after_wfi(int cpu)
{
	bus_dcm_disable();
	slidle_cnt[cpu]++;
}

static void go_to_slidle(int cpu)
{
	slidle_before_wfi(cpu);

	mb();
	__asm__ __volatile__("wfi" : : : "memory");

	slidle_after_wfi(cpu);
}


/************************************************
 * regular idle part
 ************************************************/

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

	go_to_wfi();

	rgidle_after_wfi(cpu);
}

/************************************************
 * idle task flow part
 ************************************************/
static inline void soidle_pre_handler(void)
{
	mtk_idle_notifier_call_chain(SOIDLE_START);

#ifndef CONFIG_MTK_FPGA
	hps_del_timer();
	/* stop Mali dvfs_callback timer */
	if (!mtk_gpu_sodi_entry())
		idle_warn("not stop GPU timer in SODI\n");
#endif

#ifdef CONFIG_THERMAL
	/* cancel thermal hrtimer for power saving */
	tscpu_cancel_thermal_timer();

	/* cancel thermal timer/workqueues for power saving */
	mtkts_bts_cancel_thermal_timer();
	mtkts_btsmdpa_cancel_thermal_timer();
	mtkts_pmic_cancel_thermal_timer();
	mtkts_battery_cancel_thermal_timer();
	mtkts_pa_cancel_thermal_timer();
	mtkts_wmt_cancel_thermal_timer();

	mtkts_allts_cancel_ts1_timer();
	mtkts_allts_cancel_ts2_timer();
	mtkts_allts_cancel_ts3_timer();
	mtkts_allts_cancel_ts4_timer();
	mtkts_allts_cancel_ts5_timer();
#endif
}

static inline void soidle_post_handler(void)
{
	mtk_idle_notifier_call_chain(SOIDLE_END);

#ifndef CONFIG_MTK_FPGA
	hps_restart_timer();
	/* restart Mali dvfs_callback timer */
	if (!mtk_gpu_sodi_exit())
		idle_warn("not restart GPU timer outside SODI\n");
#endif

#ifdef CONFIG_THERMAL
	/* restart thermal hrtimer for update temp info */
	tscpu_start_thermal_timer();

	/* restart thermal timer/workqueues */
	mtkts_bts_start_thermal_timer();
	mtkts_btsmdpa_start_thermal_timer();
	mtkts_pmic_start_thermal_timer();
	mtkts_battery_start_thermal_timer();
	mtkts_pa_start_thermal_timer();
	mtkts_wmt_start_thermal_timer();

	mtkts_allts_start_ts1_timer();
	mtkts_allts_start_ts2_timer();
	mtkts_allts_start_ts3_timer();
	mtkts_allts_start_ts4_timer();
	mtkts_allts_start_ts5_timer();
#endif
}

/*
 * xxidle_handler return 1 if enter and exit the low power state
 */


static u32 slp_spm_SODI3_flags = {
	#ifdef CONFIG_MTK_FPGA
	SPM_FLAG_DIS_CPU_PDN |
	SPM_FLAG_DIS_INFRA_PDN |
	SPM_FLAG_DIS_DDRPHY_PDN |
	SPM_FLAG_DIS_VCORE_DVS |
	SPM_FLAG_DIS_VCORE_DFS |
	SPM_FLAG_DIS_BUS_CLOCK_OFF |
	SPM_FLAG_DIS_VPROC_VSRAM_DVS |
	SPM_FLAG_DIS_PMCU_SRAM_SLEEP
	#endif
#if 0
	SPM_FLAG_ENABLE_SODI3 |
	#ifdef FEATURE_ENABLE_SODI2P5
	SPM_FLAG_DIS_SRCCLKEN_LOW |
	#endif
	#ifdef CONFIG_MTK_ICUSB_SUPPORT
	SPM_FLAG_DIS_INFRA_PDN |
	#endif
	SPM_FLAG_DIS_SYSRAM_SLEEP
#endif
};

static u32 slp_spm_SODI_flags = {
	#ifdef CONFIG_MTK_FPGA
	SPM_FLAG_DIS_CPU_PDN |
	SPM_FLAG_DIS_INFRA_PDN |
	SPM_FLAG_DIS_DDRPHY_PDN |
	SPM_FLAG_DIS_VCORE_DVS |
	SPM_FLAG_DIS_VCORE_DFS |
	SPM_FLAG_DIS_BUS_CLOCK_OFF |
	SPM_FLAG_DIS_VPROC_VSRAM_DVS |
	SPM_FLAG_DIS_PMCU_SRAM_SLEEP
	#endif
#if 0
	#ifdef FEATURE_ENABLE_SODI2P5
	SPM_FLAG_ENABLE_SODI3 |
	SPM_FLAG_DIS_SRCCLKEN_LOW |
	#endif
	#ifdef CONFIG_MTK_ICUSB_SUPPORT
	SPM_FLAG_DIS_INFRA_PDN |
	#endif
	SPM_FLAG_DIS_SYSRAM_SLEEP
#endif
};

u32 slp_spm_deepidle_flags = {
	SPM_FLAG_DIS_CPU_PDN  |
	SPM_FLAG_DIS_INFRA_PDN |
	SPM_FLAG_DIS_DDRPHY_PDN |
	SPM_FLAG_DIS_VCORE_DVS |
	SPM_FLAG_DIS_VCORE_DFS |
	SPM_FLAG_DIS_BUS_CLOCK_OFF |
	SPM_FLAG_DIS_VPROC_VSRAM_DVS |
	SPM_FLAG_DIS_PMCU_SRAM_SLEEP
};

static inline void dpidle_pre_handler(void)
{
#ifndef CONFIG_MTK_FPGA
	hps_del_timer();

#ifdef CONFIG_THERMAL
	/* cancel thermal hrtimer for power saving */
	tscpu_cancel_thermal_timer();

	/* cancel thermal timer/workqueues for power saving */
	mtkts_bts_cancel_thermal_timer();
	mtkts_btsmdpa_cancel_thermal_timer();
	mtkts_pmic_cancel_thermal_timer();
	mtkts_battery_cancel_thermal_timer();
	mtkts_pa_cancel_thermal_timer();
	mtkts_wmt_cancel_thermal_timer();

	mtkts_allts_cancel_ts1_timer();
	mtkts_allts_cancel_ts2_timer();
	mtkts_allts_cancel_ts3_timer();
	mtkts_allts_cancel_ts4_timer();
	mtkts_allts_cancel_ts5_timer();
#endif
#endif
}

static inline void dpidle_post_handler(void)
{
#ifndef CONFIG_MTK_FPGA
	hps_restart_timer();

#ifdef CONFIG_THERMAL
	/* restart thermal hrtimer for update temp info */
	tscpu_start_thermal_timer();

	/* restart thermal timer/workqueues */
	mtkts_bts_start_thermal_timer();
	mtkts_btsmdpa_start_thermal_timer();
	mtkts_pmic_start_thermal_timer();
	mtkts_battery_start_thermal_timer();
	mtkts_pa_start_thermal_timer();
	mtkts_wmt_start_thermal_timer();

	mtkts_allts_start_ts1_timer();
	mtkts_allts_start_ts2_timer();
	mtkts_allts_start_ts3_timer();
	mtkts_allts_start_ts4_timer();
	mtkts_allts_start_ts5_timer();
#endif
#endif
}

static void restart_dvfsp(void)
{
#ifdef CONFIG_HYBRID_CPU_DVFS
	if (mt_dvfsp_paused_by_idle) {
		cpuhvfs_unpause_dvfsp_to_run(PAUSE_IDLE);
		mt_dvfsp_paused_by_idle = false;
	}
#endif
}

static void update_dpidle_cnt(int cpu)
{
	dpidle_cnt[cpu]++;
/*
	if ((spm_debug_flag & (SPM_DBG_DEBUG_IDX_26M_WAKE | SPM_DBG_DEBUG_IDX_26M_SLEEP))
			== (SPM_DBG_DEBUG_IDX_26M_WAKE | SPM_DBG_DEBUG_IDX_26M_SLEEP))
		dpidle_f26m_cnt[cpu]++;
*/
}

static void dpidle_pre_process(int cpu)
{
	mtk_idle_notifier_call_chain(DPIDLE_START);

	dpidle_pre_handler();
}

static void dpidle_post_process(int cpu)
{
	mtk_idle_notifier_call_chain(DPIDLE_END);

	dpidle_post_handler();

	restart_dvfsp();

	update_dpidle_cnt(cpu);
}

static bool (*idle_can_enter[NR_TYPES])(int cpu, int reason) = {
	dpidle_can_enter,
	soidle3_can_enter,
	soidle_can_enter,
	mcidle_can_enter,
	slidle_can_enter,
	rgidle_can_enter,
};

void dump_idle_cnt_in_interval(int cpu)
{
	int i = 0;
	char *p = log_buf;
	char *p2 = log_buf_2;
	unsigned long long idle_cnt_dump_curr_time = 0;
	bool have_dpidle = false;
	bool have_soidle3 = false;
	bool have_soidle = false;

	if (idle_cnt_dump_prev_time == 0)
		idle_cnt_dump_prev_time = idle_get_current_time_ms();

	idle_cnt_dump_curr_time = idle_get_current_time_ms();

	if (!(cpu == 0 || cpu == 4))
		return;

	if (!((idle_cnt_dump_curr_time - idle_cnt_dump_prev_time) > idle_cnt_dump_criteria))
		return;

	/* dump idle count */
	/* deepidle */
	p = log_buf;
	for (i = 0; i < nr_cpu_ids; i++) {
		if ((dpidle_cnt[i] - dpidle_last_cnt[i]) != 0) {
			log2buf(p, log_buf, "[%d] = %lu, ",
				i, dpidle_cnt[i] - dpidle_last_cnt[i]);
			have_dpidle = true;
		}

		dpidle_last_cnt[i] = dpidle_cnt[i];
	}

	log2buf(p2, log_buf_2, "DP: %s --- ", have_dpidle ? log_buf : "No enter");

	/* sodi3 */
	p = log_buf;
	for (i = 0; i < nr_cpu_ids; i++) {
		if ((soidle3_cnt[i] - soidle3_last_cnt[i]) != 0) {
			log2buf(p, log_buf, "[%d] = %lu, ",
				i, soidle3_cnt[i] - soidle3_last_cnt[i]);
			have_soidle3 = true;
		}

		soidle3_last_cnt[i] = soidle3_cnt[i];
	}

	log2buf(p2, log_buf_2, "SODI3: %s --- ",
		have_soidle3 ? log_buf : "No enter");

	/* sodi3 */
	p = log_buf;
	for (i = 0; i < nr_cpu_ids; i++) {
		if ((soidle_cnt[i] - soidle_last_cnt[i]) != 0) {
			log2buf(p, log_buf, "[%d] = %lu, ",
				i, soidle_cnt[i] - soidle_last_cnt[i]);
			have_soidle = true;
		}

		soidle_last_cnt[i] = soidle_cnt[i];
	}

	log2buf(p2, log_buf_2, "SODI: %s --- ",
		have_soidle ? log_buf : "No enter");

	/* dump log */
	idle_warn("%s\n", log_buf_2);

	/* dump idle ratio */
	if (idle_ratio_en) {
		idle_ratio_profile_duration = idle_get_current_time_ms() - idle_ratio_profile_start_time;
		idle_warn("--- CPU 0 idle: %llu, DP = %llu, SO3 = %llu, SO = %llu, RG = %llu --- (ms)\n",
				idle_ratio_profile_duration,
				idle_ratio_value[IDLE_TYPE_DP],
				idle_ratio_value[IDLE_TYPE_SO3],
				idle_ratio_value[IDLE_TYPE_SO],
				idle_ratio_value[IDLE_TYPE_RG]);

		idle_ratio_profile_start_time = idle_get_current_time_ms();
		for (i = 0; i < NR_TYPES; i++)
			idle_ratio_value[i] = 0;
	}

	/* update time base */
	idle_cnt_dump_prev_time = idle_cnt_dump_curr_time;
}

inline void idle_ratio_calc_start(int type, int cpu)
{
	if (type >= 0 && type < NR_TYPES && cpu == 0)
		idle_ratio_start_time[type] = idle_get_current_time_ms();
}

inline void idle_ratio_calc_stop(int type, int cpu)
{
	if (type >= 0 && type < NR_TYPES && cpu == 0)
		idle_ratio_value[type] += (idle_get_current_time_ms() - idle_ratio_start_time[type]);
}

int mt_idle_select(int cpu)
{
	int idx = 0;
	int reason = NR_REASONS;

	dump_idle_cnt_in_interval(cpu);

	#ifndef CONFIG_MTK_FPGA
	/* check if firmware loaded or not */
	if (!spm_load_firmware_status()) {
		reason = BY_FRM;
		goto get_idle_idx;
	}
	#endif

	#ifndef CONFIG_MTK_FPGA
	#ifdef CONFIG_SMP
	/* check cpu status */
	if (!mt_idle_cpu_criteria()) {
		reason = BY_CPU;
		goto get_idle_idx;
	}
	#endif
	#endif

	if (cpu % 4) {
		reason = BY_CPU;
		goto get_idle_idx;
	}

	/* check idle_spm_lock */
	if (idle_spm_lock) {
		reason = BY_VTG;
		goto get_idle_idx;
	}

	/* teei ready */
	#ifndef CONFIG_MTK_FPGA
	#ifdef CONFIG_MICROTRUST_TEE_SUPPORT
	if (!is_teei_ready()) {
		reason = BY_OTH;
		goto get_idle_idx;
	}
	#endif
	#endif

	#ifndef CONFIG_MTK_FPGA
	/* cg check */
	memset(idle_block_mask, 0,
		NR_TYPES * (NR_GRPS + 1) * sizeof(unsigned int));
	if (!mt_idle_check_cg(idle_block_mask)) {
		reason = BY_CLK;
		goto get_idle_idx;
	}
	#endif

get_idle_idx:
	/* check if criteria check fail in common part */
	for (idx = 0; idx < NR_TYPES; idx++) {
		if (idle_switch[idx]) {
			/* call each idle scenario check functions */
			if (idle_can_enter[idx](cpu, reason))
				break;
		}
	}

	return idx;
}

int dpidle_enter(int cpu)
{
	int ret = IDLE_TYPE_DP;

	idle_ratio_calc_start(IDLE_TYPE_DP, cpu);

	dpidle_pre_process(cpu);
	spm_go_to_dpidle(slp_spm_deepidle_flags, (u32)cpu, dpidle_dump_log);
	dpidle_post_process(cpu);

	idle_ratio_calc_stop(IDLE_TYPE_DP, cpu);

#ifdef SPM_DEEPIDLE_PROFILE_TIME
	gpt_get_cnt(SPM_PROFILE_APXGPT, &dpidle_profile[3]);
	idle_warn_log("1:%u, 2:%u, 3:%u, 4:%u\n",
				dpidle_profile[0], dpidle_profile[1], dpidle_profile[2], dpidle_profile[3]);
#endif

	return ret;
}
EXPORT_SYMBOL(dpidle_enter);

int soidle3_enter(int cpu)
{
	int ret = IDLE_TYPE_SO3;
	unsigned long long soidle3_time = 0;
	static unsigned long long soidle3_residency;

	if (sodi3_flags & SODI_FLAG_RESIDENCY)
		soidle3_time = idle_get_current_time_ms();

	idle_ratio_calc_start(IDLE_TYPE_SO3, cpu);

	soidle_pre_handler();

	if (mt_idle_auxadc_is_released())
		slp_spm_SODI3_flags &= ~SPM_FLAG_DIS_SRCCLKEN_LOW;
	else
		slp_spm_SODI3_flags |= SPM_FLAG_DIS_SRCCLKEN_LOW;
#ifdef DEFAULT_MMP_ENABLE
	MMProfileLogEx(sodi_mmp_get_events()->sodi_enable, MMProfileFlagStart, 0, 0);
#endif /* DEFAULT_MMP_ENABLE */

	spm_go_to_sodi3(slp_spm_SODI3_flags, (u32)cpu, sodi3_flags);

#ifdef DEFAULT_MMP_ENABLE
	MMProfileLogEx(sodi_mmp_get_events()->sodi_enable, MMProfileFlagEnd, 0, spm_read(SPM_PASR_DPD_3));
#endif /* DEFAULT_MMP_ENABLE */

	soidle_post_handler();

	idle_ratio_calc_stop(IDLE_TYPE_SO3, cpu);

	if (sodi3_flags & SODI_FLAG_RESIDENCY) {
		soidle3_residency += idle_get_current_time_ms() - soidle3_time;
		idle_dbg("SO3: soidle3_residency = %llu\n", soidle3_residency);
	}

#ifdef SPM_SODI3_PROFILE_TIME
	gpt_get_cnt(SPM_SODI3_PROFILE_APXGPT, &soidle3_profile[3]);
	idle_ver("SODI3: cpu_freq:%u, 1=>2:%u, 2=>3:%u, 3=>4:%u\n",
			mt_cpufreq_get_cur_freq(0), soidle3_profile[1] - soidle3_profile[0],
			soidle3_profile[2] - soidle3_profile[1],
			soidle3_profile[3] - soidle3_profile[2]);
#endif

	return ret;
}
EXPORT_SYMBOL(soidle3_enter);

int soidle_enter(int cpu)
{
	int ret = IDLE_TYPE_SO;
	unsigned long long soidle_time = 0;
	static unsigned long long soidle_residency;

	if (sodi_flags & SODI_FLAG_RESIDENCY)
		soidle_time = idle_get_current_time_ms();

	idle_ratio_calc_start(IDLE_TYPE_SO, cpu);

	soidle_pre_handler();

#ifdef DEFAULT_MMP_ENABLE
	MMProfileLogEx(sodi_mmp_get_events()->sodi_enable, MMProfileFlagStart, 0, 0);
#endif /* DEFAULT_MMP_ENABLE */

	spm_go_to_sodi(slp_spm_SODI_flags, (u32)cpu, sodi_flags);

#ifdef DEFAULT_MMP_ENABLE
	MMProfileLogEx(sodi_mmp_get_events()->sodi_enable, MMProfileFlagEnd, 0, spm_read(SPM_PASR_DPD_3));
#endif /* DEFAULT_MMP_ENABLE */

	soidle_post_handler();

	idle_ratio_calc_stop(IDLE_TYPE_SO, cpu);

	if (sodi_flags & SODI_FLAG_RESIDENCY) {
		soidle_residency += idle_get_current_time_ms() - soidle_time;
		idle_dbg("SO: soidle_residency = %llu\n", soidle_residency);
	}

#ifdef SPM_SODI_PROFILE_TIME
	gpt_get_cnt(SPM_SODI_PROFILE_APXGPT, &soidle_profile[3]);
	idle_ver("SODI: cpu_freq:%u, 1=>2:%u, 2=>3:%u, 3=>4:%u\n",
			mt_cpufreq_get_cur_freq(0), soidle_profile[1] - soidle_profile[0],
			soidle_profile[2] - soidle_profile[1],
			soidle_profile[3] - soidle_profile[2]);
#endif

	return ret;
}
EXPORT_SYMBOL(soidle_enter);

int mcidle_enter(int cpu)
{
	int ret = IDLE_TYPE_MC;

#ifndef CONFIG_MTK_FPGA
	go_to_mcidle(cpu);
	mcidle_cnt[cpu] += 1;
#endif

	return ret;
}
EXPORT_SYMBOL(mcidle_enter);

int slidle_enter(int cpu)
{
	int ret = IDLE_TYPE_SL;

	go_to_slidle(cpu);

	return ret;
}
EXPORT_SYMBOL(slidle_enter);

int rgidle_enter(int cpu)
{
	int ret = IDLE_TYPE_RG;

	idle_ratio_calc_start(IDLE_TYPE_RG, cpu);

	go_to_rgidle(cpu);

	idle_ratio_calc_stop(IDLE_TYPE_RG, cpu);

	return ret;
}
EXPORT_SYMBOL(rgidle_enter);

static int mcdi_cpu_notify(struct notifier_block *self, unsigned long action, void *hcpu)
{
	if (!idle_switch[IDLE_TYPE_MC])
		return NOTIFY_OK;

	switch (action) {

	case CPU_UP_PREPARE:
	case CPU_UP_PREPARE_FROZEN:
		if (num_online_cpus() == 1)
			spm_mcdi_switch_on_off(SPM_MCDI_EARLY_SUSPEND, 1);

		break;

#ifdef CONFIG_HOTPLUG_CPU
	case CPU_DYING:
	case CPU_DYING_FROZEN:
		break;
	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		if (num_online_cpus() == 1)
			spm_mcdi_switch_on_off(SPM_MCDI_EARLY_SUSPEND, 0);

		break;
#endif

	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block mcdi_nb = {
	.notifier_call = mcdi_cpu_notify,
};

void mt_idle_init(void)
{
	register_cpu_notifier(&mcdi_nb);
}

/***************************/
/* debugfs                 */
/***************************/
static char dbg_buf[4096] = { 0 };
static char cmd_buf[512] = { 0 };

#undef mt_idle_log
#define mt_idle_log(fmt, args...)	log2buf(p, dbg_buf, fmt, ##args)

/* idle_state */
static int _idle_state_open(struct seq_file *s, void *data)
{
	return 0;
}

static int idle_state_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, _idle_state_open, inode->i_private);
}

static ssize_t idle_state_read(struct file *filp,
			       char __user *userbuf, size_t count, loff_t *f_pos)
{
	int i, len = 0;
	char *p = dbg_buf;

	mt_idle_log("********** idle state dump **********\n");

	for (i = 0; i < nr_cpu_ids; i++) {
		mt_idle_log("dpidle_cnt[%d]=%lu, dpidle_26m[%d]=%lu, soidle3_cnt[%d]=%lu, soidle_cnt[%d]=%lu, ",
			i, dpidle_cnt[i], i, dpidle_f26m_cnt[i], i, soidle3_cnt[i], i, soidle_cnt[i]);
		mt_idle_log("mcidle_cnt[%d]=%lu, slidle_cnt[%d]=%lu, rgidle_cnt[%d]=%lu\n",
			i, mcidle_cnt[i], i, slidle_cnt[i], i, rgidle_cnt[i]);
	}

	mt_idle_log("\n********** variables dump **********\n");
	for (i = 0; i < NR_TYPES; i++)
		mt_idle_log("%s_switch=%d, ", mt_get_idle_name(i), idle_switch[i]);

	mt_idle_log("\n");
	mt_idle_log("idle_ratio_en = %u\n", idle_ratio_en);

	mt_idle_log("\n********** idle command help **********\n");
	mt_idle_log("status help:   cat /sys/kernel/debug/cpuidle/idle_state\n");
	mt_idle_log("switch on/off: echo switch mask > /sys/kernel/debug/cpuidle/idle_state\n");
	mt_idle_log("idle ratio profile: echo ratio 1/0 > /sys/kernel/debug/cpuidle/idle_state\n");

	mt_idle_log("soidle3 help:  cat /sys/kernel/debug/cpuidle/soidle3_state\n");
	mt_idle_log("soidle help:   cat /sys/kernel/debug/cpuidle/soidle_state\n");
	mt_idle_log("dpidle help:   cat /sys/kernel/debug/cpuidle/dpidle_state\n");
	mt_idle_log("mcidle help:   cat /sys/kernel/debug/cpuidle/mcidle_state\n");
	mt_idle_log("slidle help:   cat /sys/kernel/debug/cpuidle/slidle_state\n");
	mt_idle_log("rgidle help:   cat /sys/kernel/debug/cpuidle/rgidle_state\n");

	len = p - dbg_buf;

	return simple_read_from_buffer(userbuf, count, f_pos, dbg_buf, len);
}

static ssize_t idle_state_write(struct file *filp,
				const char __user *userbuf, size_t count, loff_t *f_pos)
{
	char cmd[NR_CMD_BUF];
	int idx;
	int param;

	count = min(count, sizeof(cmd_buf) - 1);

	if (copy_from_user(cmd_buf, userbuf, count))
		return -EFAULT;

	cmd_buf[count] = '\0';

	if (sscanf(cmd_buf, "%127s %x", cmd, &param) == 2) {
		if (!strcmp(cmd, "switch")) {
			for (idx = 0; idx < NR_TYPES; idx++)
				idle_switch[idx] = (param & (1U << idx)) ? 1 : 0;
		} else if (!strcmp(cmd, "ratio")) {
			idle_ratio_en = param;

			if (idle_ratio_en) {
				idle_ratio_profile_start_time = idle_get_current_time_ms();
				for (idx = 0; idx < NR_TYPES; idx++)
					idle_ratio_value[idx] = 0;
			}
		}
		return count;
	}

	return -EINVAL;
}

static const struct file_operations idle_state_fops = {
	.open = idle_state_open,
	.read = idle_state_read,
	.write = idle_state_write,
	.llseek = seq_lseek,
	.release = single_release,
};

/* mcidle_state */
static int _mcidle_state_open(struct seq_file *s, void *data)
{
	return 0;
}

static int mcidle_state_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, _mcidle_state_open, inode->i_private);
}

static ssize_t mcidle_state_read(struct file *filp, char __user *userbuf, size_t count, loff_t *f_pos)
{
	int len = 0;
	int cpus, reason;
	char *p = dbg_buf;

	mt_idle_log("*********** deep idle state ************\n");
	mt_idle_log("mcidle_time_critera=%u\n", mcidle_time_critera);

	for (cpus = 0; cpus < nr_cpu_ids; cpus++) {
		mt_idle_log("cpu:%d\n", cpus);
		for (reason = 0; reason < NR_REASONS; reason++) {
			mt_idle_log("[%d]mcidle_block_cnt[%s]=%lu\n",
				reason, mt_get_reason_name(reason), mcidle_block_cnt[cpus][reason]);
		}
		mt_idle_log("\n");
	}

	mt_idle_log("\n********** mcidle command help **********\n");
	mt_idle_log("mcidle help:   cat /sys/kernel/debug/cpuidle/mcidle_state\n");
	mt_idle_log("switch on/off: echo [mcidle] 1/0 > /sys/kernel/debug/cpuidle/mcidle_state\n");
	mt_idle_log("modify tm_cri: echo time value(dec) > /sys/kernel/debug/cpuidle/mcidle_state\n");

	len = p - dbg_buf;

	return simple_read_from_buffer(userbuf, count, f_pos, dbg_buf, len);
}

static ssize_t mcidle_state_write(struct file *filp,
				  const char __user *userbuf,
				  size_t count,
				  loff_t *f_pos)
{
	char cmd[NR_CMD_BUF];
	int param;

	count = min(count, sizeof(cmd_buf) - 1);

	if (copy_from_user(cmd_buf, userbuf, count))
		return -EFAULT;

	cmd_buf[count] = '\0';

	if (sscanf(cmd_buf, "%127s %d", cmd, &param) == 2) {
		if (!strcmp(cmd, "mcidle"))
			idle_switch[IDLE_TYPE_MC] = param;
		else if (!strcmp(cmd, "time"))
			mcidle_time_critera = param;

		return count;
	} else if (!kstrtoint(cmd_buf, 10, &param) == 1) {
		idle_switch[IDLE_TYPE_MC] = param;

		return count;
	}

	return -EINVAL;
}

static const struct file_operations mcidle_state_fops = {
	.open = mcidle_state_open,
	.read = mcidle_state_read,
	.write = mcidle_state_write,
	.llseek = seq_lseek,
	.release = single_release,
};

/* dpidle_state */
static int _dpidle_state_open(struct seq_file *s, void *data)
{
	return 0;
}

static int dpidle_state_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, _dpidle_state_open, inode->i_private);
}

static ssize_t dpidle_state_read(struct file *filp, char __user *userbuf, size_t count, loff_t *f_pos)
{
	int i, k, len = 0;
	char *p = dbg_buf;

	mt_idle_log("*********** deep idle state ************\n");
	mt_idle_log("dpidle_time_critera=%u\n", dpidle_time_critera);

	for (i = 0; i < NR_REASONS; i++)
		mt_idle_log("[%d]dpidle_block_cnt[%s]=%lu\n", i, mt_get_reason_name(i), dpidle_block_cnt[i]);
	mt_idle_log("\n");

	for (i = 0; i < NR_GRPS; i++) {
		mt_idle_log("[%02d]dpidle_condition_mask[%-8s]=0x%08x\t\tdpidle_block_mask[%-8s]=0x%08x\n", i,
				mt_get_cg_group_name(i), idle_condition_mask[IDLE_TYPE_DP][i],
				mt_get_cg_group_name(i), idle_block_mask[IDLE_TYPE_DP][i]);
	}

	for (i = 0; i < NR_GRPS; i++) {
		mt_idle_log("[%-8s]\n", mt_get_cg_group_name(i));

		for (k = 0; k < 32; k++) {
			if (dpidle_blocking_stat[i][k] != 0)
				mt_idle_log("%-2d: %d\n", k, dpidle_blocking_stat[i][k]);
		}
	}
	for (i = 0; i < NR_GRPS; i++)
		for (k = 0; k < 32; k++)
			dpidle_blocking_stat[i][k] = 0;

	mt_idle_log("dpidle_by_pass_cg=%u\n", dpidle_by_pass_cg);
	mt_idle_log("dpidle_by_pass_i2c_appm_cg=%u\n", dpidle_by_pass_i2c_appm_cg);
	mt_idle_log("dpidle_by_pass_pg=%u\n", dpidle_by_pass_pg);
	mt_idle_log("dpidle_dump_log = %u\n", dpidle_dump_log);
	mt_idle_log("(0: None, 1: Reduced, 2: Full\n");

	mt_idle_log("\n*********** dpidle command help  ************\n");
	mt_idle_log("dpidle help:   cat /sys/kernel/debug/cpuidle/dpidle_state\n");
	mt_idle_log("switch on/off: echo [dpidle] 1/0 > /sys/kernel/debug/cpuidle/dpidle_state\n");
	mt_idle_log("cpupdn on/off: echo cpupdn 1/0 > /sys/kernel/debug/cpuidle/dpidle_state\n");
	mt_idle_log("en_dp_by_bit:  echo enable id > /sys/kernel/debug/cpuidle/dpidle_state\n");
	mt_idle_log("dis_dp_by_bit: echo disable id > /sys/kernel/debug/cpuidle/dpidle_state\n");
	mt_idle_log("modify tm_cri: echo time value(dec) > /sys/kernel/debug/cpuidle/dpidle_state\n");
	mt_idle_log("bypass cg:     echo bypass 1/0 > /sys/kernel/debug/cpuidle/dpidle_state\n");

	len = p - dbg_buf;

	return simple_read_from_buffer(userbuf, count, f_pos, dbg_buf, len);
}

static ssize_t dpidle_state_write(struct file *filp,
									const char __user *userbuf,
									size_t count,
									loff_t *f_pos)
{
	char cmd[NR_CMD_BUF];
	int param;

	count = min(count, sizeof(cmd_buf) - 1);

	if (copy_from_user(cmd_buf, userbuf, count))
		return -EFAULT;

	cmd_buf[count] = '\0';

	if (sscanf(cmd_buf, "%127s %d", cmd, &param) == 2) {
		if (!strcmp(cmd, "dpidle"))
			idle_switch[IDLE_TYPE_DP] = param;
		else if (!strcmp(cmd, "enable"))
			enable_dpidle_by_bit(param);
		else if (!strcmp(cmd, "disable"))
			disable_dpidle_by_bit(param);
		else if (!strcmp(cmd, "time"))
			dpidle_time_critera = param;
		else if (!strcmp(cmd, "bypass"))
			dpidle_by_pass_cg = param;
		else if (!strcmp(cmd, "bypass_appm"))
			dpidle_by_pass_i2c_appm_cg = param;
		else if (!strcmp(cmd, "bypass_pg")) {
			dpidle_by_pass_pg = param;
			idle_warn("bypass_pg = %d\n", dpidle_by_pass_pg);
		} else if (!strcmp(cmd, "log"))
			dpidle_dump_log = param;

		return count;
	} else if (!kstrtoint(cmd_buf, 10, &param) == 1) {
		idle_switch[IDLE_TYPE_DP] = param;

		return count;
	}

	return -EINVAL;
}

static const struct file_operations dpidle_state_fops = {
	.open = dpidle_state_open,
	.read = dpidle_state_read,
	.write = dpidle_state_write,
	.llseek = seq_lseek,
	.release = single_release,
};

/* soidle3_state */
static int _soidle3_state_open(struct seq_file *s, void *data)
{
	return 0;
}

static int soidle3_state_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, _soidle3_state_open, inode->i_private);
}

static ssize_t soidle3_state_read(struct file *filp, char __user *userbuf, size_t count, loff_t *f_pos)
{
	int i, len = 0;
	char *p = dbg_buf;

	mt_idle_log("*********** soidle3 state ************\n");
	mt_idle_log("soidle3_time_critera=%u\n", soidle3_time_critera);

	for (i = 0; i < NR_REASONS; i++)
		mt_idle_log("[%d]soidle3_block_cnt[%s]=%lu\n", i, mt_get_reason_name(i), soidle3_block_cnt[i]);
	mt_idle_log("\n");

	for (i = 0; i < NR_PLLS; i++) {
		mt_idle_log("[%02d]soidle3_pll_condition_mask[%-8s]=0x%08x\t\tsoidle3_pll_block_mask[%-8s]=0x%08x\n", i,
			mt_get_pll_group_name(i), soidle3_pll_condition_mask[i],
			mt_get_pll_group_name(i), soidle3_pll_block_mask[i]);
	}
	mt_idle_log("\n");

	for (i = 0; i < NR_GRPS; i++) {
		mt_idle_log("[%02d]soidle3_condition_mask[%-8s]=0x%08x\t\tsoidle3_block_mask[%-8s]=0x%08x\n", i,
			mt_get_cg_group_name(i), idle_condition_mask[IDLE_TYPE_SO3][i],
			mt_get_cg_group_name(i), idle_block_mask[IDLE_TYPE_SO3][i]);
	}

	mt_idle_log("soidle3_bypass_pll=%u\n", soidle3_by_pass_pll);
	mt_idle_log("soidle3_bypass_cg=%u\n", soidle3_by_pass_cg);
	mt_idle_log("soidle3_by_pass_i2c_appm_cg=%u\n", soidle3_by_pass_i2c_appm_cg);
	mt_idle_log("soidle3_bypass_en=%u\n", soidle3_by_pass_en);
	mt_idle_log("sodi3_flags=0x%x\n", sodi3_flags);

	mt_idle_log("\n*********** soidle3 command help  ************\n");
	mt_idle_log("soidle3 help:  cat /sys/kernel/debug/cpuidle/soidle3_state\n");
	mt_idle_log("switch on/off: echo [soidle3] 1/0 > /sys/kernel/debug/cpuidle/soidle3_state\n");
	mt_idle_log("cpupdn on/off: echo cpupdn 1/0 > /sys/kernel/debug/cpuidle/soidle3_state\n");
	mt_idle_log("en_dp_by_bit:  echo enable id > /sys/kernel/debug/cpuidle/soidle3_state\n");
	mt_idle_log("dis_dp_by_bit: echo disable id > /sys/kernel/debug/cpuidle/soidle3_state\n");
	mt_idle_log("modify tm_cri: echo time value(dec) > /sys/kernel/debug/cpuidle/soidle3_state\n");
	mt_idle_log("bypass pll:    echo bypass_pll 1/0 > /sys/kernel/debug/cpuidle/soidle3_state\n");
	mt_idle_log("bypass cg:     echo bypass 1/0 > /sys/kernel/debug/cpuidle/soidle3_state\n");
	mt_idle_log("bypass appm:   echo bypass_appm 1/0 > /sys/kernel/debug/cpuidle/soidle3_state\n");
	mt_idle_log("bypass en:     echo bypass_en 1/0 > /sys/kernel/debug/cpuidle/soidle3_state\n");
	mt_idle_log("sodi3 flags:   echo sodi3_flags value > /sys/kernel/debug/cpuidle/soidle3_state\n");

	len = p - dbg_buf;

	return simple_read_from_buffer(userbuf, count, f_pos, dbg_buf, len);
}

static ssize_t soidle3_state_write(struct file *filp,
									const char __user *userbuf,
									size_t count,
									loff_t *f_pos)
{
	char cmd[NR_CMD_BUF];
	int param;

	count = min(count, sizeof(cmd_buf) - 1);

	if (copy_from_user(cmd_buf, userbuf, count))
		return -EFAULT;

	cmd_buf[count] = '\0';

	if (sscanf(cmd_buf, "%127s %d", cmd, &param) == 2) {
		if (!strcmp(cmd, "soidle3"))
			idle_switch[IDLE_TYPE_SO3] = param;
		else if (!strcmp(cmd, "enable"))
			enable_soidle3_by_bit(param);
		else if (!strcmp(cmd, "disable"))
			disable_soidle3_by_bit(param);
		else if (!strcmp(cmd, "time"))
			soidle3_time_critera = param;
		else if (!strcmp(cmd, "bypass_pll")) {
			soidle3_by_pass_pll = param;
			idle_dbg("bypass_pll = %d\n", soidle3_by_pass_pll);
		} else if (!strcmp(cmd, "bypass")) {
			soidle3_by_pass_cg = param;
			idle_dbg("bypass = %d\n", soidle3_by_pass_cg);
		} else if (!strcmp(cmd, "bypass_appm")) {
			soidle3_by_pass_i2c_appm_cg = param;
			idle_dbg("bypass_appm = %d\n", soidle3_by_pass_i2c_appm_cg);
		} else if (!strcmp(cmd, "bypass_en")) {
			soidle3_by_pass_en = param;
			idle_dbg("bypass_en = %d\n", soidle3_by_pass_en);
		} else if (!strcmp(cmd, "sodi3_flags")) {
			sodi3_flags = param;
			idle_dbg("sodi3_flags = 0x%x\n", sodi3_flags);
		}
		return count;
	} else if (!kstrtoint(cmd_buf, 10, &param) == 1) {
		idle_switch[IDLE_TYPE_SO3] = param;
		return count;
	}

	return -EINVAL;
}

static const struct file_operations soidle3_state_fops = {
	.open = soidle3_state_open,
	.read = soidle3_state_read,
	.write = soidle3_state_write,
	.llseek = seq_lseek,
	.release = single_release,
};

/* soidle_state */
static int _soidle_state_open(struct seq_file *s, void *data)
{
	return 0;
}

static int soidle_state_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, _soidle_state_open, inode->i_private);
}

static ssize_t soidle_state_read(struct file *filp, char __user *userbuf, size_t count, loff_t *f_pos)
{
	int i, len = 0;
	char *p = dbg_buf;

	mt_idle_log("*********** soidle state ************\n");
	mt_idle_log("soidle_time_critera=%u\n", soidle_time_critera);

	for (i = 0; i < NR_REASONS; i++)
		mt_idle_log("[%d]soidle_block_cnt[%s]=%lu\n", i, mt_get_reason_name(i), soidle_block_cnt[i]);
	mt_idle_log("\n");

	for (i = 0; i < NR_GRPS; i++) {
		mt_idle_log("[%02d]soidle_condition_mask[%-8s]=0x%08x\t\tsoidle_block_mask[%-8s]=0x%08x\n", i,
			mt_get_cg_group_name(i), idle_condition_mask[IDLE_TYPE_SO][i],
			mt_get_cg_group_name(i), idle_block_mask[IDLE_TYPE_SO][i]);
	}

	mt_idle_log("soidle_bypass_cg=%u\n", soidle_by_pass_cg);
	mt_idle_log("soidle_by_pass_i2c_appm_cg=%u\n", soidle_by_pass_i2c_appm_cg);
	mt_idle_log("soidle_by_pass_pg=%u\n", soidle_by_pass_pg);
	mt_idle_log("soidle_bypass_en=%u\n", soidle_by_pass_en);
	mt_idle_log("sodi_flags=0x%x\n", sodi_flags);

	mt_idle_log("\n*********** soidle command help  ************\n");
	mt_idle_log("soidle help:   cat /sys/kernel/debug/cpuidle/soidle_state\n");
	mt_idle_log("switch on/off: echo [soidle] 1/0 > /sys/kernel/debug/cpuidle/soidle_state\n");
	mt_idle_log("cpupdn on/off: echo cpupdn 1/0 > /sys/kernel/debug/cpuidle/soidle_state\n");
	mt_idle_log("en_dp_by_bit:  echo enable id > /sys/kernel/debug/cpuidle/soidle_state\n");
	mt_idle_log("dis_dp_by_bit: echo disable id > /sys/kernel/debug/cpuidle/soidle_state\n");
	mt_idle_log("modify tm_cri: echo time value(dec) > /sys/kernel/debug/cpuidle/soidle_state\n");
	mt_idle_log("bypass cg:     echo bypass 1/0 > /sys/kernel/debug/cpuidle/soidle_state\n");
	mt_idle_log("bypass appm:   echo bypass_appm 1/0 > /sys/kernel/debug/cpuidle/soidle_state\n");
	mt_idle_log("bypass en:     echo bypass_en 1/0 > /sys/kernel/debug/cpuidle/soidle_state\n");
	mt_idle_log("sodi flags:    echo sodi_flags value > /sys/kernel/debug/cpuidle/soidle_state\n");

	len = p - dbg_buf;

	return simple_read_from_buffer(userbuf, count, f_pos, dbg_buf, len);
}

static ssize_t soidle_state_write(struct file *filp,
									const char __user *userbuf,
									size_t count,
									loff_t *f_pos)
{
	char cmd[NR_CMD_BUF];
	int param;

	count = min(count, sizeof(cmd_buf) - 1);

	if (copy_from_user(cmd_buf, userbuf, count))
		return -EFAULT;

	cmd_buf[count] = '\0';

	if (sscanf(cmd_buf, "%127s %d", cmd, &param) == 2) {
		if (!strcmp(cmd, "soidle"))
			idle_switch[IDLE_TYPE_SO] = param;
		else if (!strcmp(cmd, "enable"))
			enable_soidle_by_bit(param);
		else if (!strcmp(cmd, "disable"))
			disable_soidle_by_bit(param);
		else if (!strcmp(cmd, "time"))
			soidle_time_critera = param;
		else if (!strcmp(cmd, "bypass")) {
			soidle_by_pass_cg = param;
			idle_dbg("bypass = %d\n", soidle_by_pass_cg);
		} else if (!strcmp(cmd, "bypass_appm")) {
			soidle_by_pass_i2c_appm_cg = param;
			idle_dbg("bypass_appm = %d\n", soidle_by_pass_i2c_appm_cg);
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
		return count;
	} else if (!kstrtoint(cmd_buf, 10, &param) == 1) {
		idle_switch[IDLE_TYPE_SO] = param;
		return count;
	}

	return -EINVAL;
}

static const struct file_operations soidle_state_fops = {
	.open = soidle_state_open,
	.read = soidle_state_read,
	.write = soidle_state_write,
	.llseek = seq_lseek,
	.release = single_release,
};

/* slidle_state */
static int _slidle_state_open(struct seq_file *s, void *data)
{
	return 0;
}

static int slidle_state_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, _slidle_state_open, inode->i_private);
}

static ssize_t slidle_state_read(struct file *filp, char __user *userbuf, size_t count,
				 loff_t *f_pos)
{
	int i, len = 0;
	char *p = dbg_buf;

	mt_idle_log("*********** slow idle state ************\n");

	for (i = 0; i < NR_REASONS; i++)
		mt_idle_log("[%d]slidle_block_cnt[%s]=%lu\n", i, mt_get_reason_name(i), slidle_block_cnt[i]);
	mt_idle_log("\n");

	for (i = 0; i < NR_GRPS; i++) {
		mt_idle_log("[%02d]slidle_condition_mask[%-8s]=0x%08x\t\tslidle_block_mask[%-8s]=0x%08x\n", i,
			mt_get_cg_group_name(i), idle_condition_mask[IDLE_TYPE_SL][i],
			mt_get_cg_group_name(i), idle_block_mask[IDLE_TYPE_SL][i]);
	}

	mt_idle_log("\n********** slidle command help **********\n");
	mt_idle_log("slidle help:   cat /sys/kernel/debug/cpuidle/slidle_state\n");
	mt_idle_log("switch on/off: echo [slidle] 1/0 > /sys/kernel/debug/cpuidle/slidle_state\n");

	len = p - dbg_buf;

	return simple_read_from_buffer(userbuf, count, f_pos, dbg_buf, len);
}

static ssize_t slidle_state_write(struct file *filp, const char __user *userbuf,
				  size_t count, loff_t *f_pos)
{
	char cmd[NR_CMD_BUF];
	int param;

	count = min(count, sizeof(cmd_buf) - 1);

	if (copy_from_user(cmd_buf, userbuf, count))
		return -EFAULT;

	cmd_buf[count] = '\0';

	if (sscanf(userbuf, "%127s %d", cmd, &param) == 2) {
		if (!strcmp(cmd, "slidle"))
			idle_switch[IDLE_TYPE_SL] = param;
		else if (!strcmp(cmd, "enable"))
			enable_slidle_by_bit(param);
		else if (!strcmp(cmd, "disable"))
			disable_slidle_by_bit(param);

		return count;
	} else if (!kstrtoint(userbuf, 10, &param) == 1) {
		idle_switch[IDLE_TYPE_SL] = param;
		return count;
	}

	return -EINVAL;
}

static const struct file_operations slidle_state_fops = {
	.open = slidle_state_open,
	.read = slidle_state_read,
	.write = slidle_state_write,
	.llseek = seq_lseek,
	.release = single_release,
};

/* CG/PLL/MTCMOS register dump */
static int _reg_dump_open(struct seq_file *s, void *data)
{
	return 0;
}

static int reg_dump_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, _reg_dump_open, inode->i_private);
}

static ssize_t reg_dump_read(struct file *filp, char __user *userbuf, size_t count, loff_t *f_pos)
{
	int len = 0;
	char *p = dbg_buf;

	mt_idle_log("SPM_PWR_STATUS = 0x%08x\n", idle_readl(SPM_PWR_STATUS));
	mt_idle_log("SPM_MFG_PWR_CON = 0x%08x\n", idle_readl(SPM_MFG_PWR_CON));
	mt_idle_log("SPM_ISP_PWR_CON = 0x%08x\n", idle_readl(SPM_ISP_PWR_CON));
	mt_idle_log("SPM_VDE_PWR_CON = 0x%08x\n", idle_readl(SPM_VDE_PWR_CON));
	mt_idle_log("SPM_VEN_PWR_CON = 0x%08x\n", idle_readl(SPM_VEN_PWR_CON));

	mt_idle_log("DISP_CG_CON0 = 0x%08x\n", idle_readl(DISP_CG_CON0));
	mt_idle_log("DISP_CG_CON1 = 0x%08x\n", idle_readl(DISP_CG_CON1));
	mt_idle_log("MFG_CG_CON = 0x%08x\n", idle_readl(MFG_CG_CON));
	mt_idle_log("IMG_CG_CON = 0x%08x\n", idle_readl(IMG_CG_CON));
	mt_idle_log("VDEC_CG_CON_0 = 0x%08x\n", idle_readl(VDEC_CG_CON_0));
	mt_idle_log("VDEC_CG_CON_1 = 0x%08x\n", idle_readl(VDEC_CG_CON_1));
	mt_idle_log("VENCSYS_CG_CON = 0x%08x\n", idle_readl(VENCSYS_CG_CON));

	/* INFRA CG */
	mt_idle_log("INFRA_SW_CG_0_STA = 0x%08x\n", idle_readl(INFRA_SW_CG_0_STA));
	mt_idle_log("INFRA_SW_CG_1_STA = 0x%08x\n", idle_readl(INFRA_SW_CG_1_STA));
	mt_idle_log("INFRA_SW_CG_2_STA = 0x%08x\n", idle_readl(INFRA_SW_CG_2_STA));

	/* PLL */
	mt_idle_log("=== PLL ====\n");
	mt_idle_log("MAINPLL_CON0 = 0x%08x\n", idle_readl(MAINPLL_CON0));
	mt_idle_log("UNIVPLL_CON0 = 0x%08x\n", idle_readl(UNIVPLL_CON0));
	mt_idle_log("MSDCPLL_CON0 = 0x%08x\n", idle_readl(MSDCPLL_CON0));
	mt_idle_log("TVDPLL_CON0 = 0x%08x\n", idle_readl(TVDPLL_CON0));
	mt_idle_log("APLL1_CON0 = 0x%08x\n", idle_readl(APLL1_CON0));
	mt_idle_log("APLL2_CON0 = 0x%08x\n", idle_readl(APLL2_CON0));
	mt_idle_log("ARMCA15PLL_CON0 = 0x%08x\n", idle_readl(ARMCA15PLL_CON0));
	mt_idle_log("ARMCA7PLL_CON0 = 0x%08x\n", idle_readl(ARMCA7PLL_CON0));
	mt_idle_log("MMPLL_CON0 = 0x%08x\n", idle_readl(MMPLL_CON0));
	mt_idle_log("VENCPLL_CON0 = 0x%08x\n", idle_readl(VENCPLL_CON0));

	/* MTCMOS */
	mt_idle_log("=== MTCMOS ====\n");
	mt_idle_log("SPM_ISP_PWR_CON = 0x%08x\n", idle_readl(SPM_ISP_PWR_CON));
	mt_idle_log("SPM_MFG_PWR_CON = 0x%08x\n", idle_readl(SPM_MFG_PWR_CON));
	mt_idle_log("SPM_MFGAYSNC_PWR_CON = 0x%08x\n", idle_readl(SPM_MFG_PWR_CON - 0x4));
	mt_idle_log("SPM_VDE_PWR_CON = 0x%08x\n", idle_readl(SPM_VDE_PWR_CON));
	mt_idle_log("SPM_VEN_PWR_CON = 0x%08x\n", idle_readl(SPM_VEN_PWR_CON));
	mt_idle_log("SPM_DIS_PWR_CON = 0x%08x\n", idle_readl(SPM_DIS_PWR_CON));
	mt_idle_log("SPM_AUDIO_PWR_CON = 0x%08x\n", idle_readl(SPM_AUDIO_PWR_CON));
	mt_idle_log("SPM_MD1_PWR_CON = 0x%08x\n", idle_readl(SPM_MD1_PWR_CON));
/*	mt_idle_log("SPM_MD2_PWR_CON = 0x%08x\n", idle_readl(SPM_MD2_PWR_CON));*/
	mt_idle_log("SPM_C2K_PWR_CON = 0x%08x\n", idle_readl(SPM_C2K_PWR_CON));
	mt_idle_log("SPM_CONN_PWR_CON = 0x%08x\n", idle_readl(SPM_CONN_PWR_CON));
	mt_idle_log("SPM_MDSYS_INTF_INFRA_PWR_CON = 0x%08x\n",
		idle_readl(SPM_MDSYS_INTF_INFRA_PWR_CON));

	len = p - dbg_buf;

	return simple_read_from_buffer(userbuf, count, f_pos, dbg_buf, len);
}

static ssize_t reg_dump_write(struct file *filp,
									const char __user *userbuf,
									size_t count,
									loff_t *f_pos)
{
	count = min(count, sizeof(cmd_buf) - 1);

	return count;
}

static const struct file_operations reg_dump_fops = {
	.open = reg_dump_open,
	.read = reg_dump_read,
	.write = reg_dump_write,
	.llseek = seq_lseek,
	.release = single_release,
};

/* debugfs entry */
static struct dentry *root_entry;

static int mt_cpuidle_debugfs_init(void)
{
	/* TODO: check if debugfs_create_file() failed */
	/* Initialize debugfs */
	root_entry = debugfs_create_dir("cpuidle", NULL);
	if (!root_entry) {
		idle_err("Can not create debugfs `dpidle_state`\n");
		return 1;
	}

	debugfs_create_file("idle_state", 0644, root_entry, NULL, &idle_state_fops);
	debugfs_create_file("dpidle_state", 0644, root_entry, NULL, &dpidle_state_fops);
	debugfs_create_file("soidle3_state", 0644, root_entry, NULL, &soidle3_state_fops);
	debugfs_create_file("soidle_state", 0644, root_entry, NULL, &soidle_state_fops);
	debugfs_create_file("mcidle_state", 0644, root_entry, NULL, &mcidle_state_fops);
	debugfs_create_file("slidle_state", 0644, root_entry, NULL, &slidle_state_fops);
	debugfs_create_file("reg_dump", 0644, root_entry, NULL, &reg_dump_fops);

	return 0;
}

/* CPU hotplug notifier, for informing whether CPU hotplug is working */
static int mt_idle_cpu_callback(struct notifier_block *nfb,
				   unsigned long action, void *hcpu)
{
	switch (action) {
	case CPU_UP_PREPARE:
	case CPU_UP_PREPARE_FROZEN:
	case CPU_DOWN_PREPARE:
	case CPU_DOWN_PREPARE_FROZEN:
		atomic_inc(&is_in_hotplug);
		break;

	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
	case CPU_UP_CANCELED:
	case CPU_UP_CANCELED_FROZEN:
	case CPU_DOWN_FAILED:
	case CPU_DOWN_FAILED_FROZEN:
	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		atomic_dec(&is_in_hotplug);
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block mt_idle_cpu_notifier = {
	.notifier_call = mt_idle_cpu_callback,
	.priority   = INT_MAX,
};

static int mt_idle_hotplug_cb_init(void)
{
	register_cpu_notifier(&mt_idle_cpu_notifier);

	return 0;
}

void mt_cpuidle_framework_init(void)
{
	idle_ver("[%s]entry!!\n", __func__);

	iomap_init();
	mt_cpuidle_debugfs_init();
	mt_idle_hotplug_cb_init();
}
EXPORT_SYMBOL(mt_cpuidle_framework_init);

module_param(mt_idle_chk_golden, bool, 0644);
module_param(mt_dpidle_chk_golden, bool, 0644);
