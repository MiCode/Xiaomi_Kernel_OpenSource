/* arch/arm/mach-msm/pm.c
 *
 * MSM Power Management Routines
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2008-2011, Code Aurora Forum. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/pm.h>
#include <linux/pm_qos_params.h>
#include <linux/proc_fs.h>
#include <linux/suspend.h>
#include <linux/reboot.h>
#include <linux/uaccess.h>
#include <mach/msm_iomap.h>
#include <mach/system.h>
#include <asm/io.h>

#ifdef CONFIG_HAS_WAKELOCK
#include <linux/wakelock.h>
#endif

#include "smd_private.h"
#include "smd_rpcrouter.h"
#include "acpuclock.h"
#include "clock.h"
#include "proc_comm.h"
#include "idle.h"
#include "irq.h"
#include "gpio.h"
#include "timer.h"
#include "pm.h"
#include "pm-boot.h"

enum {
	MSM_PM_DEBUG_SUSPEND = 1U << 0,
	MSM_PM_DEBUG_POWER_COLLAPSE = 1U << 1,
	MSM_PM_DEBUG_STATE = 1U << 2,
	MSM_PM_DEBUG_CLOCK = 1U << 3,
	MSM_PM_DEBUG_RESET_VECTOR = 1U << 4,
	MSM_PM_DEBUG_SMSM_STATE = 1U << 5,
	MSM_PM_DEBUG_IDLE = 1U << 6,
};
static int msm_pm_debug_mask;
module_param_named(debug_mask, msm_pm_debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP);

#ifdef CONFIG_MSM_SLEEP_TIME_OVERRIDE
static int msm_pm_sleep_time_override;
module_param_named(sleep_time_override,
	msm_pm_sleep_time_override, int, S_IRUGO | S_IWUSR | S_IWGRP);
#endif

static int msm_pm_sleep_mode = CONFIG_MSM7X00A_SLEEP_MODE;
module_param_named(sleep_mode, msm_pm_sleep_mode, int, S_IRUGO | S_IWUSR | S_IWGRP);
static int msm_pm_idle_sleep_mode = CONFIG_MSM7X00A_IDLE_SLEEP_MODE;
module_param_named(idle_sleep_mode, msm_pm_idle_sleep_mode, int, S_IRUGO | S_IWUSR | S_IWGRP);
static int msm_pm_idle_sleep_min_time = CONFIG_MSM7X00A_IDLE_SLEEP_MIN_TIME;
module_param_named(idle_sleep_min_time, msm_pm_idle_sleep_min_time, int, S_IRUGO | S_IWUSR | S_IWGRP);
static int msm_pm_idle_spin_time = CONFIG_MSM7X00A_IDLE_SPIN_TIME;
module_param_named(idle_spin_time, msm_pm_idle_spin_time, int, S_IRUGO | S_IWUSR | S_IWGRP);

#define A11S_CLK_SLEEP_EN (MSM_CSR_BASE + 0x11c)
#define A11S_PWRDOWN (MSM_CSR_BASE + 0x440)
#define A11S_STANDBY_CTL (MSM_CSR_BASE + 0x108)
#define A11RAMBACKBIAS (MSM_CSR_BASE + 0x508)

enum {
	SLEEP_LIMIT_NONE = 0,
	SLEEP_LIMIT_NO_TCXO_SHUTDOWN = 2
};

static atomic_t msm_pm_init_done = ATOMIC_INIT(0);
struct smsm_interrupt_info_ext {
	uint32_t aArm_en_mask;
	uint32_t aArm_interrupts_pending;
	uint32_t aArm_wakeup_reason;
	uint32_t aArm_rpc_prog;
	uint32_t aArm_rpc_proc;
	char aArm_smd_port_name[20];
	uint32_t aArm_gpio_info;
};
static struct msm_pm_smem_addr_t {
	uint32_t *sleep_delay;
	uint32_t *limit_sleep;
	struct smsm_interrupt_info *int_info;
	struct smsm_interrupt_info_ext *int_info_ext;
} msm_pm_sma;

static uint32_t msm_pm_max_sleep_time;
static struct msm_pm_platform_data *msm_pm_modes;

#ifdef CONFIG_MSM_IDLE_STATS
enum msm_pm_time_stats_id {
	MSM_PM_STAT_REQUESTED_IDLE,
	MSM_PM_STAT_IDLE_SPIN,
	MSM_PM_STAT_IDLE_WFI,
	MSM_PM_STAT_IDLE_SLEEP,
	MSM_PM_STAT_IDLE_FAILED_SLEEP,
	MSM_PM_STAT_IDLE_POWER_COLLAPSE,
	MSM_PM_STAT_IDLE_FAILED_POWER_COLLAPSE,
	MSM_PM_STAT_SUSPEND,
	MSM_PM_STAT_FAILED_SUSPEND,
	MSM_PM_STAT_NOT_IDLE,
	MSM_PM_STAT_COUNT
};

static struct msm_pm_time_stats {
	const char *name;
	int64_t first_bucket_time;
	int bucket[CONFIG_MSM_IDLE_STATS_BUCKET_COUNT];
	int64_t min_time[CONFIG_MSM_IDLE_STATS_BUCKET_COUNT];
	int64_t max_time[CONFIG_MSM_IDLE_STATS_BUCKET_COUNT];
	int count;
	int64_t total_time;
} msm_pm_stats[MSM_PM_STAT_COUNT] = {
	[MSM_PM_STAT_REQUESTED_IDLE].name = "idle-request",
	[MSM_PM_STAT_REQUESTED_IDLE].first_bucket_time =
		CONFIG_MSM_IDLE_STATS_FIRST_BUCKET,

	[MSM_PM_STAT_IDLE_SPIN].name = "idle-spin",
	[MSM_PM_STAT_IDLE_SPIN].first_bucket_time =
		CONFIG_MSM_IDLE_STATS_FIRST_BUCKET,

	[MSM_PM_STAT_IDLE_WFI].name = "idle-wfi",
	[MSM_PM_STAT_IDLE_WFI].first_bucket_time =
		CONFIG_MSM_IDLE_STATS_FIRST_BUCKET,

	[MSM_PM_STAT_IDLE_SLEEP].name = "idle-sleep",
	[MSM_PM_STAT_IDLE_SLEEP].first_bucket_time =
		CONFIG_MSM_IDLE_STATS_FIRST_BUCKET,

	[MSM_PM_STAT_IDLE_FAILED_SLEEP].name = "idle-failed-sleep",
	[MSM_PM_STAT_IDLE_FAILED_SLEEP].first_bucket_time =
		CONFIG_MSM_IDLE_STATS_FIRST_BUCKET,

	[MSM_PM_STAT_IDLE_POWER_COLLAPSE].name = "idle-power-collapse",
	[MSM_PM_STAT_IDLE_POWER_COLLAPSE].first_bucket_time =
		CONFIG_MSM_IDLE_STATS_FIRST_BUCKET,

	[MSM_PM_STAT_IDLE_FAILED_POWER_COLLAPSE].name =
		"idle-failed-power-collapse",
	[MSM_PM_STAT_IDLE_FAILED_POWER_COLLAPSE].first_bucket_time =
		CONFIG_MSM_IDLE_STATS_FIRST_BUCKET,

	[MSM_PM_STAT_SUSPEND].name = "suspend",
	[MSM_PM_STAT_SUSPEND].first_bucket_time =
		CONFIG_MSM_SUSPEND_STATS_FIRST_BUCKET,

	[MSM_PM_STAT_FAILED_SUSPEND].name = "failed-suspend",
	[MSM_PM_STAT_FAILED_SUSPEND].first_bucket_time =
		CONFIG_MSM_IDLE_STATS_FIRST_BUCKET,

	[MSM_PM_STAT_NOT_IDLE].name = "not-idle",
	[MSM_PM_STAT_NOT_IDLE].first_bucket_time =
		CONFIG_MSM_IDLE_STATS_FIRST_BUCKET,
};

static void msm_pm_add_stat(enum msm_pm_time_stats_id id, int64_t t)
{
	int i;
	int64_t bt;
	msm_pm_stats[id].total_time += t;
	msm_pm_stats[id].count++;
	bt = t;
	do_div(bt, msm_pm_stats[id].first_bucket_time);
	if (bt < 1ULL << (CONFIG_MSM_IDLE_STATS_BUCKET_SHIFT *
				(CONFIG_MSM_IDLE_STATS_BUCKET_COUNT - 1)))
		i = DIV_ROUND_UP(fls((uint32_t)bt),
					CONFIG_MSM_IDLE_STATS_BUCKET_SHIFT);
	else
		i = CONFIG_MSM_IDLE_STATS_BUCKET_COUNT - 1;
	msm_pm_stats[id].bucket[i]++;
	if (t < msm_pm_stats[id].min_time[i] || !msm_pm_stats[id].max_time[i])
		msm_pm_stats[id].min_time[i] = t;
	if (t > msm_pm_stats[id].max_time[i])
		msm_pm_stats[id].max_time[i] = t;
}

static uint32_t msm_pm_sleep_limit = SLEEP_LIMIT_NONE;
#endif

static int
msm_pm_wait_state(uint32_t wait_state_all_set, uint32_t wait_state_all_clear,
                  uint32_t wait_state_any_set, uint32_t wait_state_any_clear)
{
	int i;
	uint32_t state;

	for (i = 0; i < 2000000; i++) {
		state = smsm_get_state(SMSM_MODEM_STATE);
		if (((state & wait_state_all_set) == wait_state_all_set) &&
		    ((~state & wait_state_all_clear) == wait_state_all_clear) &&
		    (wait_state_any_set == 0 || (state & wait_state_any_set) ||
		     wait_state_any_clear == 0 || (state & wait_state_any_clear)))
			return 0;
	}
	printk(KERN_ERR "msm_pm_wait_state(%x, %x, %x, %x) failed %x\n",
	       wait_state_all_set, wait_state_all_clear,
	       wait_state_any_set, wait_state_any_clear, state);
	return -ETIMEDOUT;
}

/*
 * Respond to timing out waiting for Modem
 *
 * NOTE: The function never returns.
 */
static void msm_pm_timeout(void)
{
#if defined(CONFIG_MSM_PM_TIMEOUT_RESET_CHIP)
	printk(KERN_EMERG "%s(): resetting chip\n", __func__);
	msm_proc_comm(PCOM_RESET_CHIP_IMM, NULL, NULL);
#elif defined(CONFIG_MSM_PM_TIMEOUT_RESET_MODEM)
	printk(KERN_EMERG "%s(): resetting modem\n", __func__);
	msm_proc_comm_reset_modem_now();
#elif defined(CONFIG_MSM_PM_TIMEOUT_HALT)
	printk(KERN_EMERG "%s(): halting\n", __func__);
#endif
	for (;;)
		;
}

static int msm_sleep(int sleep_mode, uint32_t sleep_delay,
	uint32_t sleep_limit, int from_idle)
{
	int collapsed;
	uint32_t enter_state;
	uint32_t enter_wait_set = 0;
	uint32_t enter_wait_clear = 0;
	uint32_t exit_state;
	uint32_t exit_wait_clear = 0;
	uint32_t exit_wait_set = 0;
	unsigned long pm_saved_acpu_clk_rate = 0;
	int ret;
	int rv = -EINTR;

	if (msm_pm_debug_mask & MSM_PM_DEBUG_SUSPEND)
		printk(KERN_INFO "msm_sleep(): "
			"mode %d delay %u limit %u idle %d\n",
			sleep_mode, sleep_delay, sleep_limit, from_idle);

	switch (sleep_mode) {
	case MSM_PM_SLEEP_MODE_POWER_COLLAPSE:
		enter_state = SMSM_PWRC;
		enter_wait_set = SMSM_RSA;
		exit_state = SMSM_WFPI;
		exit_wait_clear = SMSM_RSA;
		break;
	case MSM_PM_SLEEP_MODE_POWER_COLLAPSE_SUSPEND:
		enter_state = SMSM_PWRC_SUSPEND;
		enter_wait_set = SMSM_RSA;
		exit_state = SMSM_WFPI;
		exit_wait_clear = SMSM_RSA;
		break;
	case MSM_PM_SLEEP_MODE_APPS_SLEEP:
		enter_state = SMSM_SLEEP;
		exit_state = SMSM_SLEEPEXIT;
		exit_wait_set = SMSM_SLEEPEXIT;
		break;
	default:
		enter_state = 0;
		exit_state = 0;
	}

	if (enter_state && !(smsm_get_state(SMSM_MODEM_STATE) & SMSM_RUN)) {
		if ((MSM_PM_DEBUG_POWER_COLLAPSE | MSM_PM_DEBUG_SUSPEND) &
			msm_pm_debug_mask)
			printk(KERN_INFO "msm_sleep(): modem not ready\n");
		rv = -EBUSY;
		goto check_failed;
	}

	memset(msm_pm_sma.int_info, 0, sizeof(*msm_pm_sma.int_info));
	msm_irq_enter_sleep1(!!enter_state, from_idle,
		&msm_pm_sma.int_info->aArm_en_mask);
	msm_gpio_enter_sleep(from_idle);

	if (enter_state) {
		if (sleep_delay == 0 && sleep_mode >= MSM_PM_SLEEP_MODE_APPS_SLEEP)
			sleep_delay = 192000*5; /* APPS_SLEEP does not allow infinite timeout */

		*msm_pm_sma.sleep_delay = sleep_delay;
		*msm_pm_sma.limit_sleep = sleep_limit;
		ret = smsm_change_state(SMSM_APPS_STATE, SMSM_RUN, enter_state);
		if (ret) {
			printk(KERN_ERR "msm_sleep(): smsm_change_state %x failed\n", enter_state);
			enter_state = 0;
			exit_state = 0;
		}
		ret = msm_pm_wait_state(enter_wait_set, enter_wait_clear, 0, 0);
		if (ret) {
			printk(KERN_EMERG "msm_sleep(): power collapse entry "
				"timed out waiting for Modem's response\n");
			msm_pm_timeout();
		}
	}
	if (msm_irq_enter_sleep2(!!enter_state, from_idle))
		goto enter_failed;

	if (enter_state) {
		__raw_writel(0x1f, A11S_CLK_SLEEP_EN);
		__raw_writel(1, A11S_PWRDOWN);

		__raw_writel(0, A11S_STANDBY_CTL);
		__raw_writel(0, A11RAMBACKBIAS);

		if (msm_pm_debug_mask & MSM_PM_DEBUG_STATE)
			printk(KERN_INFO "msm_sleep(): enter "
			       "A11S_CLK_SLEEP_EN %x, A11S_PWRDOWN %x, "
			       "smsm_get_state %x\n",
			       __raw_readl(A11S_CLK_SLEEP_EN),
			       __raw_readl(A11S_PWRDOWN),
			       smsm_get_state(SMSM_MODEM_STATE));
	}

	if (sleep_mode <= MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT) {
		pm_saved_acpu_clk_rate = acpuclk_power_collapse();
		if (msm_pm_debug_mask & MSM_PM_DEBUG_CLOCK)
			printk(KERN_INFO "msm_sleep(): %ld enter power collapse"
			       "\n", pm_saved_acpu_clk_rate);
		if (pm_saved_acpu_clk_rate == 0)
			goto ramp_down_failed;
	}
	if (sleep_mode < MSM_PM_SLEEP_MODE_APPS_SLEEP) {
		if (msm_pm_debug_mask & MSM_PM_DEBUG_SMSM_STATE)
			smsm_print_sleep_info(*msm_pm_sma.sleep_delay,
				*msm_pm_sma.limit_sleep,
				msm_pm_sma.int_info->aArm_en_mask,
				msm_pm_sma.int_info->aArm_wakeup_reason,
				msm_pm_sma.int_info->aArm_interrupts_pending);
		msm_pm_boot_config_before_pc(smp_processor_id(),
				virt_to_phys(msm_pm_collapse_exit));
		collapsed = msm_pm_collapse();
		msm_pm_boot_config_after_pc(smp_processor_id());
		if (collapsed) {
			cpu_init();
			local_fiq_enable();
			rv = 0;
		}
		if (msm_pm_debug_mask & MSM_PM_DEBUG_POWER_COLLAPSE)
			printk(KERN_INFO "msm_pm_collapse(): returned %d\n",
			       collapsed);
		if (msm_pm_debug_mask & MSM_PM_DEBUG_SMSM_STATE)
			smsm_print_sleep_info(*msm_pm_sma.sleep_delay,
				*msm_pm_sma.limit_sleep,
				msm_pm_sma.int_info->aArm_en_mask,
				msm_pm_sma.int_info->aArm_wakeup_reason,
				msm_pm_sma.int_info->aArm_interrupts_pending);
	} else {
		msm_arch_idle();
		rv = 0;
	}

	if (sleep_mode <= MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT) {
		if (msm_pm_debug_mask & MSM_PM_DEBUG_CLOCK)
			printk(KERN_INFO "msm_sleep(): exit power collapse %ld"
			       "\n", pm_saved_acpu_clk_rate);
		if (acpuclk_set_rate(smp_processor_id(),
				pm_saved_acpu_clk_rate, SETRATE_PC) < 0)
			printk(KERN_ERR "msm_sleep(): clk_set_rate %ld "
			       "failed\n", pm_saved_acpu_clk_rate);
	}
	if (msm_pm_debug_mask & MSM_PM_DEBUG_STATE)
		printk(KERN_INFO "msm_sleep(): exit A11S_CLK_SLEEP_EN %x, "
		       "A11S_PWRDOWN %x, smsm_get_state %x\n",
		       __raw_readl(A11S_CLK_SLEEP_EN),
		       __raw_readl(A11S_PWRDOWN),
		       smsm_get_state(SMSM_MODEM_STATE));
ramp_down_failed:
	msm_irq_exit_sleep1(msm_pm_sma.int_info->aArm_en_mask,
		msm_pm_sma.int_info->aArm_wakeup_reason,
		msm_pm_sma.int_info->aArm_interrupts_pending);
enter_failed:
	if (enter_state) {
		__raw_writel(0x00, A11S_CLK_SLEEP_EN);
		__raw_writel(0, A11S_PWRDOWN);
		smsm_change_state(SMSM_APPS_STATE, enter_state, exit_state);
		if (msm_pm_wait_state(exit_wait_set, exit_wait_clear, 0, 0)) {
			printk(KERN_EMERG "msm_sleep(): power collapse exit "
				"timed out waiting for Modem's response\n");
			msm_pm_timeout();
		}
		if (msm_pm_debug_mask & MSM_PM_DEBUG_STATE)
			printk(KERN_INFO "msm_sleep(): sleep exit "
			       "A11S_CLK_SLEEP_EN %x, A11S_PWRDOWN %x, "
			       "smsm_get_state %x\n",
			       __raw_readl(A11S_CLK_SLEEP_EN),
			       __raw_readl(A11S_PWRDOWN),
			       smsm_get_state(SMSM_MODEM_STATE));
		if (msm_pm_debug_mask & MSM_PM_DEBUG_SMSM_STATE)
			smsm_print_sleep_info(*msm_pm_sma.sleep_delay,
				*msm_pm_sma.limit_sleep,
				msm_pm_sma.int_info->aArm_en_mask,
				msm_pm_sma.int_info->aArm_wakeup_reason,
				msm_pm_sma.int_info->aArm_interrupts_pending);
	}
	msm_irq_exit_sleep2(msm_pm_sma.int_info->aArm_en_mask,
		msm_pm_sma.int_info->aArm_wakeup_reason,
		msm_pm_sma.int_info->aArm_interrupts_pending);
	if (enter_state) {
		smsm_change_state(SMSM_APPS_STATE, exit_state, SMSM_RUN);
		if (msm_pm_debug_mask & MSM_PM_DEBUG_STATE)
			printk(KERN_INFO "msm_sleep(): sleep exit "
			       "A11S_CLK_SLEEP_EN %x, A11S_PWRDOWN %x, "
			       "smsm_get_state %x\n",
			       __raw_readl(A11S_CLK_SLEEP_EN),
			       __raw_readl(A11S_PWRDOWN),
			       smsm_get_state(SMSM_MODEM_STATE));
	}
	msm_irq_exit_sleep3(msm_pm_sma.int_info->aArm_en_mask,
		msm_pm_sma.int_info->aArm_wakeup_reason,
		msm_pm_sma.int_info->aArm_interrupts_pending);
	msm_gpio_exit_sleep();
	smd_sleep_exit();

check_failed:
	return rv;
}

void msm_pm_set_max_sleep_time(int64_t max_sleep_time_ns)
{
	int64_t max_sleep_time_bs = max_sleep_time_ns;

	/* Convert from ns -> BS units */
	do_div(max_sleep_time_bs, NSEC_PER_SEC / 32768);

	if (max_sleep_time_bs > 0x6DDD000)
		msm_pm_max_sleep_time = (uint32_t) 0x6DDD000;
	else
		msm_pm_max_sleep_time = (uint32_t) max_sleep_time_bs;

	if (msm_pm_debug_mask & MSM_PM_DEBUG_SUSPEND)
		printk(KERN_INFO "%s: Requested %lldns (%lldbs), Giving %ubs\n",
			__func__, max_sleep_time_ns,
			max_sleep_time_bs,
			msm_pm_max_sleep_time);
}
EXPORT_SYMBOL(msm_pm_set_max_sleep_time);

void arch_idle(void)
{
	int ret;
	int spin;
	int64_t sleep_time;
	int low_power = 0;
	struct msm_pm_platform_data *mode;
#ifdef CONFIG_MSM_IDLE_STATS
	int64_t t1;
	static int64_t t2;
	int exit_stat;
#endif
	int latency_qos = pm_qos_request(PM_QOS_CPU_DMA_LATENCY);
	uint32_t sleep_limit = SLEEP_LIMIT_NONE;
	int allow_sleep =
		msm_pm_idle_sleep_mode < MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT &&
#ifdef CONFIG_HAS_WAKELOCK
		!has_wake_lock(WAKE_LOCK_IDLE) &&
#endif
		msm_irq_idle_sleep_allowed();

	if (!atomic_read(&msm_pm_init_done))
		return;

	sleep_time = msm_timer_enter_idle();

#ifdef CONFIG_MSM_IDLE_STATS
	t1 = ktime_to_ns(ktime_get());
	msm_pm_add_stat(MSM_PM_STAT_NOT_IDLE, t1 - t2);
	msm_pm_add_stat(MSM_PM_STAT_REQUESTED_IDLE, sleep_time);
#endif

	mode = &msm_pm_modes[MSM_PM_SLEEP_MODE_POWER_COLLAPSE];
	if (mode->latency >= latency_qos)
		sleep_limit = SLEEP_LIMIT_NO_TCXO_SHUTDOWN;

	mode = &msm_pm_modes[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_NO_XO_SHUTDOWN];
	if (mode->latency >= latency_qos)
		allow_sleep = false;

	mode = &msm_pm_modes[
		MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT];
	if (mode->latency >= latency_qos) {
		/* no time even for SWFI */
		while (!msm_irq_pending())
			udelay(1);
#ifdef CONFIG_MSM_IDLE_STATS
		exit_stat = MSM_PM_STAT_IDLE_SPIN;
#endif
		goto abort_idle;
	}

	if (msm_pm_debug_mask & MSM_PM_DEBUG_IDLE)
		printk(KERN_INFO "arch_idle: sleep time %llu, allow_sleep %d\n",
		       sleep_time, allow_sleep);
	spin = msm_pm_idle_spin_time >> 10;
	while (spin-- > 0) {
		if (msm_irq_pending()) {
#ifdef CONFIG_MSM_IDLE_STATS
			exit_stat = MSM_PM_STAT_IDLE_SPIN;
#endif
			goto abort_idle;
		}
		udelay(1);
	}
	if (sleep_time < msm_pm_idle_sleep_min_time || !allow_sleep) {
		unsigned long saved_rate;
		saved_rate = acpuclk_wait_for_irq();
		if (msm_pm_debug_mask & MSM_PM_DEBUG_CLOCK)
			printk(KERN_DEBUG "arch_idle: clk %ld -> swfi\n",
				saved_rate);
		if (saved_rate) {
			msm_arch_idle();
#ifdef CONFIG_MSM_IDLE_STATS
			exit_stat = MSM_PM_STAT_IDLE_WFI;
#endif
		} else {
			while (!msm_irq_pending())
				udelay(1);
#ifdef CONFIG_MSM_IDLE_STATS
			exit_stat = MSM_PM_STAT_IDLE_SPIN;
#endif
		}
		if (msm_pm_debug_mask & MSM_PM_DEBUG_CLOCK)
			printk(KERN_DEBUG "msm_sleep: clk swfi -> %ld\n",
				saved_rate);
		if (saved_rate
		    && acpuclk_set_rate(smp_processor_id(),
				saved_rate, SETRATE_SWFI) < 0)
			printk(KERN_ERR "msm_sleep(): clk_set_rate %ld "
			       "failed\n", saved_rate);
	} else {
		low_power = 1;
		do_div(sleep_time, NSEC_PER_SEC / 32768);
		if (sleep_time > 0x6DDD000) {
			printk("sleep_time too big %lld\n", sleep_time);
			sleep_time = 0x6DDD000;
		}
		ret = msm_sleep(msm_pm_idle_sleep_mode, sleep_time,
			sleep_limit, 1);
#ifdef CONFIG_MSM_IDLE_STATS
		switch (msm_pm_idle_sleep_mode) {
		case MSM_PM_SLEEP_MODE_POWER_COLLAPSE_SUSPEND:
		case MSM_PM_SLEEP_MODE_POWER_COLLAPSE:
			if (ret)
				exit_stat =
					MSM_PM_STAT_IDLE_FAILED_POWER_COLLAPSE;
			else {
				exit_stat = MSM_PM_STAT_IDLE_POWER_COLLAPSE;
				msm_pm_sleep_limit = sleep_limit;
			}
			break;
		case MSM_PM_SLEEP_MODE_APPS_SLEEP:
			if (ret)
				exit_stat = MSM_PM_STAT_IDLE_FAILED_SLEEP;
			else
				exit_stat = MSM_PM_STAT_IDLE_SLEEP;
			break;
		default:
			exit_stat = MSM_PM_STAT_IDLE_WFI;
		}
#endif
	}
abort_idle:
	msm_timer_exit_idle(low_power);
#ifdef CONFIG_MSM_IDLE_STATS
	t2 = ktime_to_ns(ktime_get());
	msm_pm_add_stat(exit_stat, t2 - t1);
#endif
}

static int msm_pm_enter(suspend_state_t state)
{
	uint32_t sleep_limit = SLEEP_LIMIT_NONE;
	int ret;
#ifdef CONFIG_MSM_IDLE_STATS
	int64_t period = 0;
	int64_t time = 0;

	time = msm_timer_get_sclk_time(&period);
#endif

	clock_debug_print_enabled();

#ifdef CONFIG_MSM_SLEEP_TIME_OVERRIDE
	if (msm_pm_sleep_time_override > 0) {
		int64_t ns = NSEC_PER_SEC * (int64_t)msm_pm_sleep_time_override;
		msm_pm_set_max_sleep_time(ns);
		msm_pm_sleep_time_override = 0;
	}
#endif

	ret = msm_sleep(msm_pm_sleep_mode,
		msm_pm_max_sleep_time, sleep_limit, 0);

#ifdef CONFIG_MSM_IDLE_STATS
	if (msm_pm_sleep_mode == MSM_PM_SLEEP_MODE_POWER_COLLAPSE_SUSPEND ||
		msm_pm_sleep_mode == MSM_PM_SLEEP_MODE_POWER_COLLAPSE) {
		enum msm_pm_time_stats_id id;
		int64_t end_time;

		if (ret)
			id = MSM_PM_STAT_FAILED_SUSPEND;
		else {
			id = MSM_PM_STAT_SUSPEND;
			msm_pm_sleep_limit = sleep_limit;
		}

		if (time != 0) {
			end_time = msm_timer_get_sclk_time(NULL);
			if (end_time != 0) {
				time = end_time - time;
				if (time < 0)
					time += period;
			} else
				time = 0;
		}

		msm_pm_add_stat(id, time);
	}
#endif

	return 0;
}

static struct platform_suspend_ops msm_pm_ops = {
	.enter		= msm_pm_enter,
	.valid		= suspend_valid_only_mem,
};

static uint32_t restart_reason = 0x776655AA;

static void msm_pm_power_off(void)
{
	msm_rpcrouter_close();
	msm_proc_comm(PCOM_POWER_DOWN, 0, 0);
	for (;;) ;
}

static void msm_pm_restart(char str, const char *cmd)
{
	msm_rpcrouter_close();
	msm_proc_comm(PCOM_RESET_CHIP, &restart_reason, 0);

	for (;;) ;
}

static int msm_reboot_call(struct notifier_block *this, unsigned long code, void *_cmd)
{
	if((code == SYS_RESTART) && _cmd) {
		char *cmd = _cmd;
		if (!strcmp(cmd, "bootloader")) {
			restart_reason = 0x77665500;
		} else if (!strcmp(cmd, "recovery")) {
			restart_reason = 0x77665502;
		} else if (!strcmp(cmd, "eraseflash")) {
			restart_reason = 0x776655EF;
		} else if (!strncmp(cmd, "oem-", 4)) {
			unsigned code = simple_strtoul(cmd + 4, 0, 16) & 0xff;
			restart_reason = 0x6f656d00 | code;
		} else {
			restart_reason = 0x77665501;
		}
	}
	return NOTIFY_DONE;
}

static struct notifier_block msm_reboot_notifier = {
	.notifier_call = msm_reboot_call,
};

#ifdef CONFIG_MSM_IDLE_STATS
/*
 * Helper function of snprintf where buf is auto-incremented, size is auto-
 * decremented, and there is no return value.
 *
 * NOTE: buf and size must be l-values (e.g. variables)
 */
#define SNPRINTF(buf, size, format, ...) \
	do { \
		if (size > 0) { \
			int ret; \
			ret = snprintf(buf, size, format, ## __VA_ARGS__); \
			if (ret > size) { \
				buf += size; \
				size = 0; \
			} else { \
				buf += ret; \
				size -= ret; \
			} \
		} \
	} while (0)

/*
 * Write out the power management statistics.
 */
static int msm_pm_read_proc(
	char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int i;
	char *p = page;

	if (count < 1024) {
		*start = (char *) 0;
		*eof = 0;
		return 0;
	}

	if (!off) {
		SNPRINTF(p, count, "Last power collapse voted ");
		if (msm_pm_sleep_limit == SLEEP_LIMIT_NONE)
			SNPRINTF(p, count, "for TCXO shutdown\n\n");
		else
			SNPRINTF(p, count, "against TCXO shutdown\n\n");

		*start = (char *) 1;
		*eof = 0;
	} else if (--off < ARRAY_SIZE(msm_pm_stats)) {
		int64_t bucket_time;
		int64_t s;
		uint32_t ns;

		s = msm_pm_stats[off].total_time;
		ns = do_div(s, NSEC_PER_SEC);
		SNPRINTF(p, count,
			"%s:\n"
			"  count: %7d\n"
			"  total_time: %lld.%09u\n",
			msm_pm_stats[off].name,
			msm_pm_stats[off].count,
			s, ns);

		bucket_time = msm_pm_stats[off].first_bucket_time;
		for (i = 0; i < CONFIG_MSM_IDLE_STATS_BUCKET_COUNT - 1; i++) {
			s = bucket_time;
			ns = do_div(s, NSEC_PER_SEC);
			SNPRINTF(p, count,
				"   <%6lld.%09u: %7d (%lld-%lld)\n",
				s, ns, msm_pm_stats[off].bucket[i],
				msm_pm_stats[off].min_time[i],
				msm_pm_stats[off].max_time[i]);

			bucket_time <<= CONFIG_MSM_IDLE_STATS_BUCKET_SHIFT;
		}

		SNPRINTF(p, count, "  >=%6lld.%09u: %7d (%lld-%lld)\n",
			s, ns, msm_pm_stats[off].bucket[i],
			msm_pm_stats[off].min_time[i],
			msm_pm_stats[off].max_time[i]);

		*start = (char *) 1;
		*eof = (off + 1 >= ARRAY_SIZE(msm_pm_stats));
	}

	return p - page;
}
#undef SNPRINTF

#define MSM_PM_STATS_RESET "reset"

/*
 * Reset the power management statistics values.
 */
static int msm_pm_write_proc(struct file *file, const char __user *buffer,
	unsigned long count, void *data)
{
	char buf[sizeof(MSM_PM_STATS_RESET)];
	int ret;
	unsigned long flags;
	int i;

	if (count < strlen(MSM_PM_STATS_RESET)) {
		ret = -EINVAL;
		goto write_proc_failed;
	}

	if (copy_from_user(buf, buffer, strlen(MSM_PM_STATS_RESET))) {
		ret = -EFAULT;
		goto write_proc_failed;
	}

	if (memcmp(buf, MSM_PM_STATS_RESET, strlen(MSM_PM_STATS_RESET))) {
		ret = -EINVAL;
		goto write_proc_failed;
	}

	local_irq_save(flags);
	for (i = 0; i < ARRAY_SIZE(msm_pm_stats); i++) {
		memset(msm_pm_stats[i].bucket,
			0, sizeof(msm_pm_stats[i].bucket));
		memset(msm_pm_stats[i].min_time,
			0, sizeof(msm_pm_stats[i].min_time));
		memset(msm_pm_stats[i].max_time,
			0, sizeof(msm_pm_stats[i].max_time));
		msm_pm_stats[i].count = 0;
		msm_pm_stats[i].total_time = 0;
	}

	msm_pm_sleep_limit = SLEEP_LIMIT_NONE;
	local_irq_restore(flags);

	return count;

write_proc_failed:
	return ret;
}
#undef MSM_PM_STATS_RESET
#endif /* CONFIG_MSM_IDLE_STATS */

static int __init msm_pm_init(void)
{
#ifdef CONFIG_MSM_IDLE_STATS
	struct proc_dir_entry *d_entry;
#endif
	int ret;

	pm_power_off = msm_pm_power_off;
	arm_pm_restart = msm_pm_restart;
	msm_pm_max_sleep_time = 0;

	register_reboot_notifier(&msm_reboot_notifier);

	msm_pm_sma.sleep_delay = smem_alloc(SMEM_SMSM_SLEEP_DELAY,
		sizeof(*msm_pm_sma.sleep_delay));
	if (msm_pm_sma.sleep_delay == NULL) {
		printk(KERN_ERR "msm_pm_init: failed get SLEEP_DELAY\n");
		return -ENODEV;
	}

	msm_pm_sma.limit_sleep = smem_alloc(SMEM_SMSM_LIMIT_SLEEP,
		sizeof(*msm_pm_sma.limit_sleep));
	if (msm_pm_sma.limit_sleep == NULL) {
		printk(KERN_ERR "msm_pm_init: failed get LIMIT_SLEEP\n");
		return -ENODEV;
	}

	msm_pm_sma.int_info_ext = smem_alloc(SMEM_SMSM_INT_INFO,
		sizeof(*msm_pm_sma.int_info_ext));

	if (msm_pm_sma.int_info_ext)
		msm_pm_sma.int_info = (struct smsm_interrupt_info *)
			msm_pm_sma.int_info_ext;
	else
		msm_pm_sma.int_info = smem_alloc(SMEM_SMSM_INT_INFO,
			sizeof(*msm_pm_sma.int_info));

	if (msm_pm_sma.int_info == NULL) {
		printk(KERN_ERR "msm_pm_init: failed get INT_INFO\n");
		return -ENODEV;
	}

	ret = msm_timer_init_time_sync(msm_pm_timeout);
	if (ret)
		return ret;

	BUG_ON(msm_pm_modes == NULL);

	atomic_set(&msm_pm_init_done, 1);
	suspend_set_ops(&msm_pm_ops);

#ifdef CONFIG_MSM_IDLE_STATS
	d_entry = create_proc_entry("msm_pm_stats",
			S_IRUGO | S_IWUSR | S_IWGRP, NULL);
	if (d_entry) {
		d_entry->read_proc = msm_pm_read_proc;
		d_entry->write_proc = msm_pm_write_proc;
		d_entry->data = NULL;
	}
#endif

	return 0;
}

void __init msm_pm_set_platform_data(
	struct msm_pm_platform_data *data, int count)
{
	BUG_ON(MSM_PM_SLEEP_MODE_NR != count);
	msm_pm_modes = data;
}

late_initcall(msm_pm_init);
