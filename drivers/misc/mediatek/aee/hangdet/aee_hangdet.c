// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <asm/cacheflush.h>
#include <asm/kexec.h>
#include <asm/memory.h>
#include <asm/stacktrace.h>
#include <linux/cpu.h>
#include <linux/delay.h>
#if IS_ENABLED(CONFIG_MTK_TICK_BROADCAST_DEBUG)
#include <linux/hrtimer.h>
#endif
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/rtc.h>
#include <linux/sched/clock.h>
#include <linux/sched/signal.h>
#include <linux/spinlock.h>
#include <linux/suspend.h>
#include <linux/sysrq.h>
#include <sched/sched.h>
#include <uapi/linux/sched/types.h>
#if IS_ENABLED(CONFIG_MTK_TICK_BROADCAST_DEBUG)
#include "../../../../../kernel/time/tick-internal.h"
#include <linux/of_irq.h>
#endif
#include <mt-plat/mboot_params.h>
#include <mt-plat/mrdump.h>
#include "mrdump_helper.h"
#include "mrdump_private.h"

#if IS_ENABLED(CONFIG_MTK_TICK_BROADCAST_DEBUG)
extern void mt_irq_dump_status(unsigned int irq);
#endif

/*************************************************************************
 * Feature configure region
 *************************************************************************/
#define WK_MAX_MSG_SIZE (128)
#define SOFT_KICK_RANGE     (100*1000) // 100ms

#define WDT_MODE		0x0
#define WDT_MODE_EN		0x1
#define WDT_LENGTH_TIMEOUT(n)   ((n) << 5)
#define WDT_LENGTH      0x04
#define WDT_LENGTH_KEY      0x8
#define WDT_RST         0x08
#define WDT_RST_RELOAD      0x1971
#define WDT_NONRST_REG2     0x24
#define WDT_STAGE_OFS       29
#define WDT_STAGE_MASK      0x07
#define WDT_STAGE_KERNEL    0x03
#define CPU_NR (nr_cpu_ids)
#define DEFAULT_INTERVAL    15
#define WDT_COUNTER     0x514

#define SYST0_CON		0x40
#define SYST0_VAL		0x44

#define SYSTIMER_CNTCV_L	(0x8)
#define SYSTIMER_CNTCV_H	(0xC)

/* Delay to change RGU timeout in ms */
#define CHG_TMO_DLY_SEC		8L
#define CHG_TMO_EN		0

static int start_kicker(void);
static int g_kicker_init;
static DEFINE_SPINLOCK(lock);
struct task_struct *wk_tsk[16] = { 0 };	/* max cpu 16 */
static unsigned int wk_tsk_bind[16] = { 0 };	/* max cpu 16 */
static unsigned long long wk_tsk_bind_time[16] = { 0 };	/* max cpu 16 */
static unsigned long long wk_tsk_kick_time[16] = { 0 };	/* max cpu 16 */
static char wk_tsk_buf[128] = { 0 };
static unsigned long kick_bit;
static int g_kinterval = -1;
static struct work_struct wdk_work;
static struct workqueue_struct *wdk_workqueue;
static unsigned int lasthpg_act;
static unsigned int lasthpg_cpu;
static unsigned long long lasthpg_t;
static unsigned long long wk_lasthpg_t[16] = { 0 };	/* max cpu 16 */
static unsigned int cpuid_t[16] = { 0 };	/* max cpu 16 */
static unsigned long long lastsuspend_t;
static unsigned long long lastresume_t;
static unsigned long long lastsuspend_syst;
static unsigned long long lastresume_syst;
static struct notifier_block wdt_pm_nb;
static unsigned long g_nxtKickTime;
static int g_hang_detected;
static int g_change_tmo;
static void __iomem *toprgu_base;
static void __iomem *systimer_base;
#if IS_ENABLED(CONFIG_MTK_TICK_BROADCAST_DEBUG)
static unsigned int systimer_irq;
#endif
static unsigned int cpus_kick_bit;
static atomic_t plug_mask = ATOMIC_INIT(0x0);

static struct pt_regs saved_regs;
struct timer_list aee_dump_timer;
static unsigned long long aee_dump_timer_t;
static unsigned long long all_k_timer_t;
static unsigned int aee_dump_timer_c;
static unsigned int cpus_skip_bit;

__weak void mt_irq_dump_status(unsigned int irq)
{
	pr_info("empty gic dump\n");
};

static unsigned int get_check_bit(void)
{
	return cpus_kick_bit;
}

static unsigned int get_kick_bit(void)
{
	return kick_bit;
}

static int start_kicker_thread_with_default_setting(void)
{
	g_kinterval = DEFAULT_INTERVAL;
	start_kicker();

	pr_debug("[wdk] %s done\n", __func__);
	return 0;
}

void wk_start_kick_cpu(int cpu)
{
	if (IS_ERR(wk_tsk[cpu])) {
		pr_debug("[wdk] wk_task[%d] is NULL\n", cpu);
	} else {
		kthread_bind(wk_tsk[cpu], cpu);
		pr_info("[wdk] bind thread %d to cpu %d\n",
			wk_tsk[cpu]->pid, cpu);
		wake_up_process(wk_tsk[cpu]);
	}
}

#if IS_ENABLED(CONFIG_MTK_TICK_BROADCAST_DEBUG)
static char tick_broadcast_mtk_aee_dump_buf[128];

void tick_broadcast_mtk_aee_dump(void)
{
	int i, ret = -1;

	pr_info("[name:bc&]%s\n", bc_dump_buf.buf);

	/* tick_broadcast_oneshot_mask */

	memset(tick_broadcast_mtk_aee_dump_buf, 0,
		sizeof(tick_broadcast_mtk_aee_dump_buf));
	ret = snprintf(tick_broadcast_mtk_aee_dump_buf,
		sizeof(tick_broadcast_mtk_aee_dump_buf),
		"[TICK] oneshot_mask: %*pbl\n",
		cpumask_pr_args(bc_tick_get_broadcast_oneshot_mask()));
	if (ret >= 0)
		aee_sram_fiq_log(tick_broadcast_mtk_aee_dump_buf);

	/* tick_broadcast_pending_mask */

	memset(tick_broadcast_mtk_aee_dump_buf, 0,
		sizeof(tick_broadcast_mtk_aee_dump_buf));
	ret = snprintf(tick_broadcast_mtk_aee_dump_buf,
		sizeof(tick_broadcast_mtk_aee_dump_buf),
		"[TICK] pending_mask: %*pbl\n",
		cpumask_pr_args(bc_tick_get_broadcast_pending_mask()));
	if (ret >= 0)
		aee_sram_fiq_log(tick_broadcast_mtk_aee_dump_buf);

	/* tick_broadcast_force_mask */

	memset(tick_broadcast_mtk_aee_dump_buf, 0,
		sizeof(tick_broadcast_mtk_aee_dump_buf));
	ret = snprintf(tick_broadcast_mtk_aee_dump_buf,
		sizeof(tick_broadcast_mtk_aee_dump_buf),
		"[TICK] force_mask: %*pbl\n",
		cpumask_pr_args(bc_tick_get_broadcast_force_mask()));
	if (ret >= 0)
		aee_sram_fiq_log(tick_broadcast_mtk_aee_dump_buf);

	memset(tick_broadcast_mtk_aee_dump_buf, 0,
		sizeof(tick_broadcast_mtk_aee_dump_buf));
	ret = snprintf(tick_broadcast_mtk_aee_dump_buf,
		sizeof(tick_broadcast_mtk_aee_dump_buf),
		"[TICK] affin_e cpu: %d affin_h cpu: %d last_handle %lld\n",
		tick_broadcast_history[0].affin_enter_cpu,
		tick_broadcast_history[0].affin_handle_cpu,
		tick_broadcast_history[0].handle_time);
	if (ret >= 0)
		aee_sram_fiq_log(tick_broadcast_mtk_aee_dump_buf);

	for_each_possible_cpu(i) {
		/* to avoid unexpected overrun */
		if (i >= num_possible_cpus())
			break;
		memset(tick_broadcast_mtk_aee_dump_buf, 0,
			sizeof(tick_broadcast_mtk_aee_dump_buf));
		ret = snprintf(tick_broadcast_mtk_aee_dump_buf,
			sizeof(tick_broadcast_mtk_aee_dump_buf),
			"[TICK] cpu %d, %llu, %d, %llu\n",
			i, tick_broadcast_history[i].time_enter,
			tick_broadcast_history[i].ret_enter,
			tick_broadcast_history[i].time_exit);
		if (ret >= 0)
			aee_sram_fiq_log(tick_broadcast_mtk_aee_dump_buf);
	}
}
#endif

void dump_wdk_bind_info(void)
{
	int i = 0;

	snprintf(wk_tsk_buf, sizeof(wk_tsk_buf),
		"kick=0x%x,check=0x%x\n",
		get_kick_bit(), get_check_bit());

#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
	aee_rr_rec_kick(('D' << 24) | get_kick_bit());
	aee_rr_rec_check(('B' << 24) | get_check_bit());
#endif

	pr_info("%s", wk_tsk_buf);
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
	aee_sram_fiq_log("\n");
	aee_sram_fiq_log(wk_tsk_buf);
#endif
	for (i = 0; i < CPU_NR; i++) {
		if (wk_tsk[i] != NULL) {
			memset(wk_tsk_buf, 0, sizeof(wk_tsk_buf));
			snprintf(wk_tsk_buf, sizeof(wk_tsk_buf),
				"[wdk]CPU %d, %d, %lld, %d, %ld, %lld\n",
				i, wk_tsk_bind[i], wk_tsk_bind_time[i],
				wk_tsk[i]->on_rq, wk_tsk[i]->state,
				wk_tsk_kick_time[i]);
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
			aee_sram_fiq_log(wk_tsk_buf);
#endif
		}
	}
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
	aee_sram_fiq_log("\n");
#endif
}

void kicker_cpu_bind(int cpu)
{
	if (IS_ERR(wk_tsk[cpu]))
		pr_debug("[wdk]wk_task[%d] is NULL\n", cpu);
	else {
		/* kthread_bind(wk_tsk[cpu], cpu); */
		WARN_ON_ONCE(set_cpus_allowed_ptr(wk_tsk[cpu],
			cpumask_of(cpu)) < 0);
		wake_up_process(wk_tsk[cpu]);
		wk_tsk_bind[cpu] = 1;
		wk_tsk_bind_time[cpu] = sched_clock();
	}
}

void wk_cpu_update_bit_flag(int cpu, int plug_status, int set_check)
{
	if (plug_status == 1) {	/* plug on */
		spin_lock_bh(&lock);
		if (set_check)
			cpus_kick_bit |= (1 << cpu);
		lasthpg_cpu = cpu;
		lasthpg_act = plug_status;
		lasthpg_t = sched_clock();
		spin_unlock_bh(&lock);
	}
	if (plug_status == 0) {	/* plug off */
		spin_lock_bh(&lock);
		cpus_kick_bit &= (~(1 << cpu));
		lasthpg_cpu = cpu;
		lasthpg_act = plug_status;
		lasthpg_t = sched_clock();
		wk_tsk_bind[cpu] = 0;
		spin_unlock_bh(&lock);
	}
}

static void (*p_mt_aee_dump_irq_info)(void);
void kwdt_regist_irq_info(void (*fn)(void))
{
	p_mt_aee_dump_irq_info = fn;
}
EXPORT_SYMBOL_GPL(kwdt_regist_irq_info);

static void kwdt_time_sync(void)
{
	struct rtc_time tm;
	struct timespec64 tv = { 0 };
	/* android time */
	struct rtc_time tm_android;
	struct timespec64 tv_android = { 0 };

	ktime_get_real_ts64(&tv);
	tv_android = tv;
	rtc_time64_to_tm(tv.tv_sec, &tm);
	tv_android.tv_sec -= (uint64_t)sys_tz.tz_minuteswest * 60;
	rtc_time64_to_tm(tv_android.tv_sec, &tm_android);
	pr_info("[thread:%d] %d-%02d-%02d %02d:%02d:%02d.%u UTC;"
		"android time %d-%02d-%02d %02d:%02d:%02d.%03d\n",
		current->pid, tm.tm_year + 1900, tm.tm_mon + 1,
		tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
		(unsigned int)(tv.tv_nsec / 1000), tm_android.tm_year + 1900,
		tm_android.tm_mon + 1, tm_android.tm_mday, tm_android.tm_hour,
		tm_android.tm_min, tm_android.tm_sec,
		(unsigned int)(tv_android.tv_nsec / 1000));
}

static void kwdt_dump_func(void)
{
	struct task_struct *g, *t;
	int i = 0;

	for_each_process_thread(g, t) {
		if (!strcmp(t->comm, "watchdogd")) {
#ifdef CONFIG_ARM64
			pr_info("watchdogd on CPU %d\n", t->cpu);
#endif
			sched_show_task(t);
			break;
		}
	}

	for (i = 0; i < CPU_NR; i++) {
		struct rq *rq;

		pr_info("task on CPU%d\n", i);
		rq = cpu_rq(i);
		if (cpu_rq(i))
			sched_show_task(rq->curr);
	}

	dump_wdk_bind_info();

#if IS_ENABLED(CONFIG_MTK_IRQ_MONITOR)
	if (p_mt_aee_dump_irq_info)
		p_mt_aee_dump_irq_info();
#endif
#ifdef CONFIG_MTK_SCHED_EXTENSION
	sysrq_sched_debug_show_at_AEE();
#endif
	if (toprgu_base)
		iowrite32(WDT_RST_RELOAD, toprgu_base + WDT_RST);
	/* trigger HWT */
	crash_setup_regs(&saved_regs, NULL);
	mrdump_common_die(AEE_REBOOT_MODE_HANG_DETECT, AEE_REBOOT_MODE_WDT, "HWT", &saved_regs);
}

static void aee_dump_timer_func(struct timer_list *t)
{
	spin_lock(&lock);

	if (sched_clock() - aee_dump_timer_t < CHG_TMO_DLY_SEC * 1000000000ULL) {
		g_change_tmo = 0;
		aee_dump_timer_t = 0;
		spin_unlock(&lock);
		return;
	}

	if ((sched_clock() > all_k_timer_t) &&
	    (sched_clock() - all_k_timer_t) < (CHG_TMO_DLY_SEC + 1) * 1000000000ULL) {
		g_change_tmo = 0;
		aee_dump_timer_t = 0;
		spin_unlock(&lock);
		return;
	} else if ((all_k_timer_t > sched_clock()) &&
	    (ULLONG_MAX - all_k_timer_t + sched_clock()) < (CHG_TMO_DLY_SEC + 1) * 1000000000ULL) {
		g_change_tmo = 0;
		aee_dump_timer_t = 0;
		spin_unlock(&lock);
		return;
	}

	if (!g_hang_detected ||
	    (get_kick_bit() & get_check_bit()) == get_check_bit()) {
		g_change_tmo = 0;
		aee_dump_timer_t = 0;
		spin_unlock(&lock);
		if (toprgu_base) {
			unsigned int tmo_len = 0;

			tmo_len = ioread32(toprgu_base + WDT_LENGTH);
			iowrite32(tmo_len | WDT_LENGTH_KEY, toprgu_base + WDT_LENGTH);
			iowrite32(WDT_RST_RELOAD, toprgu_base + WDT_RST);
		}
	} else {
		spin_unlock(&lock);
		kwdt_dump_func();
	}
}

static void kwdt_process_kick(int local_bit, int cpu,
				unsigned long curInterval, char msg_buf[],
				unsigned int original_kicker)
{
	unsigned int dump_timeout = 0, r_counter = DEFAULT_INTERVAL;
	int i = 0;

	if (toprgu_base && (ioread32(toprgu_base + WDT_MODE) & WDT_MODE_EN))
		r_counter = ioread32(toprgu_base + WDT_COUNTER) / (32 * 1024);

	if (aee_dump_timer_t && ((sched_clock() - aee_dump_timer_t) >
	    (CHG_TMO_DLY_SEC + 5) * 1000000000ULL)) {
		if (!aee_dump_timer_c) {
			aee_dump_timer_c = 1;
			snprintf(msg_buf, WK_MAX_MSG_SIZE, "wdtk-et %s %d cpu=%d o_k=%d\n",
				  __func__, __LINE__, cpu, original_kicker);
			spin_unlock_bh(&lock);
			pr_info("%s", msg_buf);
			kwdt_dump_func();
			return;
		}

		snprintf(msg_buf, WK_MAX_MSG_SIZE,
			"all wdtk was already stopped cpu=%d o_k=%d\n",
			cpu, original_kicker);

		spin_unlock_bh(&lock);
		pr_info("%s", msg_buf);
		return;
	}

	local_bit = kick_bit;
	if (cpu != original_kicker) {
		/* wdtk-(original_kicker) is migrated to (cpu) */
		local_bit |= (1 << original_kicker);
	} else if ((local_bit & (1 << cpu)) == 0) {
		/* pr_debug("[wdk] set kick_bit\n"); */
		local_bit |= (1 << cpu);
		/* aee_rr_rec_wdk_kick_jiffies(jiffies); */
	} else if ((g_hang_detected == 0) &&
		    ((local_bit & get_check_bit()) != get_check_bit()) &&
		    (sched_clock() - wk_lasthpg_t[cpu] >
		     curInterval * 1000ULL)) {
		g_hang_detected = 1;
		dump_timeout = 1;
	}

	if ((g_hang_detected == 0) &&
		    (r_counter < DEFAULT_INTERVAL - 10) && !g_change_tmo) {
		g_hang_detected = 1;
		dump_timeout = 2;
	}

	wk_tsk_kick_time[cpu] = sched_clock();
#ifdef CONFIG_ARM64
	snprintf(msg_buf, WK_MAX_MSG_SIZE,
	 "[wdk-c] cpu=%d o_k=%d lbit=0x%x cbit=0x%x,%x,%d,%d,%lld,%x,%ld,%ld,%ld,%ld,[%lld,%ld] %d\n",
	 cpu, original_kicker, local_bit, get_check_bit(),
	 (local_bit ^ get_check_bit()) & get_check_bit(), lasthpg_cpu,
	 lasthpg_act, lasthpg_t, atomic_read(&plug_mask), lastsuspend_t / 1000000,
	 lastsuspend_syst / 1000000, lastresume_t / 1000000, lastresume_syst / 1000000,
	 wk_tsk_kick_time[cpu], curInterval, r_counter);
#else
	snprintf(msg_buf, WK_MAX_MSG_SIZE,
	 "[wdk-c] cpu=%d o_k=%d lbit=0x%x cbit=0x%x,%x,%d,%d,%lld,%x,[%lld,%ld] %d\n",
	 cpu, original_kicker, local_bit, get_check_bit(),
	 (local_bit ^ get_check_bit()) & get_check_bit(), lasthpg_cpu,
	 lasthpg_act, lasthpg_t, atomic_read(&plug_mask),
	 wk_tsk_kick_time[cpu], curInterval, r_counter);
#endif

	if ((local_bit & get_check_bit()) == get_check_bit()) {
		all_k_timer_t = sched_clock();
		del_timer(&aee_dump_timer);
		aee_dump_timer_t = 0;
		cpus_skip_bit = 0;
		msg_buf[5] = 'k';
		g_hang_detected = 0;
		dump_timeout = 0;
		local_bit = 0;
		kwdt_time_sync();
	}

	kick_bit = local_bit;

#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
	if (!dump_timeout) {
		aee_rr_rec_kick(('D' << 24) | local_bit);
		aee_rr_rec_check(('B' << 24) | get_check_bit());
	}
#endif

	for (i = 0; i < CPU_NR; i++) {
		if ((atomic_read(&plug_mask) & (1 << i)) || (i == cpu)) {
			cpus_kick_bit |= (1 << i);
			if (cpus_skip_bit & (1 << i))
				cpus_kick_bit &= ~(1 << i);
		}
	}

	if (cpu != original_kicker) {
		cpus_kick_bit &= ~(1 << cpu);
		cpus_skip_bit |= (1 << cpu);
	}

	spin_unlock_bh(&lock);

	pr_info("%s", msg_buf);

	if (dump_timeout) {
#if IS_ENABLED(CONFIG_MTK_TICK_BROADCAST_DEBUG)
		tick_broadcast_mtk_aee_dump();
		if (systimer_irq)
			mt_irq_dump_status(systimer_irq);
#endif
		dump_wdk_bind_info();

		if (systimer_base)
			pr_info("SYST0 CON%x VAL%x\n",
				ioread32(systimer_base + SYST0_CON),
				ioread32(systimer_base + SYST0_VAL));
#if CHG_TMO_EN
		if (toprgu_base) {
			spin_lock_bh(&lock);
			g_change_tmo = 1;
			spin_unlock_bh(&lock);
			iowrite32((WDT_LENGTH_TIMEOUT(6) << 6) | WDT_LENGTH_KEY,
				toprgu_base + WDT_LENGTH);
			iowrite32(WDT_RST_RELOAD, toprgu_base + WDT_RST);
		}
#endif
		/* abort suspend when wdt kick */
		if (dump_timeout == 2)
			pm_system_wakeup();
		else {
			spin_lock_bh(&lock);
			if (g_hang_detected && !aee_dump_timer_t) {
				pm_system_wakeup();
				aee_dump_timer_t = sched_clock();
				g_change_tmo = 1;
				spin_unlock_bh(&lock);
				aee_dump_timer.expires = jiffies + CHG_TMO_DLY_SEC * HZ;
				add_timer(&aee_dump_timer);
				return;
			}
			spin_unlock_bh(&lock);
		}
	}
}

static int kwdt_thread(void *arg)
{
	struct sched_param param = {.sched_priority = 99 };
	int cpu = 0;
	int local_bit = 0;
	unsigned long curInterval = 0;
	char msg_buf[WK_MAX_MSG_SIZE];

	sched_setscheduler(current, SCHED_FIFO, &param);
	set_current_state(TASK_INTERRUPTIBLE);

	for (;;) {
		if (kthread_should_stop()) {
			pr_info("[wdk] kthread_should_stop do !!\n");
			break;
		}
		msg_buf[0] = '\0';
		/*
		 * pr_debug("[wdk] loc_wk_wdt(%x),loc_wk_wdt->ready(%d)\n",
		 * loc_wk_wdt ,loc_wk_wdt->ready);
		 */
		curInterval = g_kinterval*1000*1000;
		spin_lock_bh(&lock);
		/* smp_processor_id does not
		 * allowed preemptible context
		 */
		cpu = smp_processor_id();

		/* to avoid wk_tsk[cpu] had not created out */
		if (wk_tsk[cpu] != 0) {
			if ((kick_bit & get_check_bit()) == 0) {
				g_nxtKickTime = ktime_to_us(ktime_get())
					+ g_kinterval*1000*1000;
				curInterval = g_kinterval*1000*1000;
			} else {
				curInterval = g_nxtKickTime
				- ktime_to_us(ktime_get());
			}
			/* to avoid interval too long */
			if (curInterval > g_kinterval*1000*1000)
				curInterval = g_kinterval*1000*1000;

			kwdt_process_kick(local_bit, cpu, curInterval,
				msg_buf, *((unsigned int *)arg));
		} else {
			spin_unlock_bh(&lock);
		}

		usleep_range(curInterval, curInterval + SOFT_KICK_RANGE);
	}
	pr_debug("[wdk] wdk thread stop, cpu:%d, pid:%d\n", cpu, current->pid);
	return 0;
}

static int start_kicker(void)
{
	int i;

	for (i = 0; i < CPU_NR; i++) {
		if (cpu_online(i)) {
			cpuid_t[i] = i;
			wk_tsk[i] = kthread_create(kwdt_thread,
				(void *) &cpuid_t[i], "wdtk-%d", i);
			if (IS_ERR(wk_tsk[i])) {
				int ret = PTR_ERR(wk_tsk[i]);

				wk_tsk[i] = NULL;
				pr_info("[wdk]kthread_create failed, wdtk-%d\n", i);
				return ret;
			}
			/* wk_cpu_update_bit_flag(i,1); */
			wk_start_kick_cpu(i);
			atomic_or(1 << i, &plug_mask);
		} else
			atomic_andnot(1 << i, &plug_mask);
	}
	g_kicker_init = 1;
	pr_info("[wdk] WDT start kicker done CPU_NR=%d online cpu NR%d\n",
		CPU_NR, num_online_cpus());
	return 0;
}

static int wk_cpu_callback_online(unsigned int cpu)
{
	wk_cpu_update_bit_flag(cpu, 1, 0);
	wk_lasthpg_t[cpu] = sched_clock();
	atomic_or(1 << cpu, &plug_mask);
	/*
	 * Bind WDK thread to this CPU.
	 * NOTE: Thread binding must be executed after CPU is ready
	 * (online).
	 */
	if (g_kicker_init == 1)
		kicker_cpu_bind(cpu);
	else
		pr_info("kicker was not bound to CPU%d\n", cpu);

	return 0;
}

static int wk_cpu_callback_offline(unsigned int cpu)
{
	wk_cpu_update_bit_flag(cpu, 0, 1);

	atomic_andnot(1 << cpu, &plug_mask);
	return 0;
}

static void wdk_work_callback(struct work_struct *work)
{
	int res = 0;
	int i = 0;

	cpu_hotplug_disable();

	res = cpuhp_setup_state_nocalls(CPUHP_AP_ONLINE_DYN,
		"watchdog:wdkctrl:online", wk_cpu_callback_online, NULL);
	if (res < 0)
		pr_info("[wdk]setup CPUHP_AP_ONLINE_DYN fail %d\n", res);

	res = cpuhp_setup_state_nocalls(CPUHP_BP_PREPARE_DYN,
		"watchdog:wdkctrl:offline", NULL, wk_cpu_callback_offline);
	if (res < 0)
		pr_info("[wdk]setup CPUHP_BP_PREPARE_DYN fail %d\n", res);

	for (i = 0; i < CPU_NR; i++) {
		if (cpu_online(i)) {
			wk_cpu_update_bit_flag(i, 1, 1);
			pr_debug("[wdk]init cpu online %d\n", i);
		} else {
			wk_cpu_update_bit_flag(i, 0, 1);
			pr_debug("[wdk]init cpu offline %d\n", i);
		}
	}

	start_kicker_thread_with_default_setting();

	cpu_hotplug_enable();

	pr_info("[wdk]init_wk done late_initcall cpus_kick_bit=0x%x -----\n",
		cpus_kick_bit);

}

static int wdt_pm_notify(struct notifier_block *notify_block,
			unsigned long mode, void *unused)
{
	uint64_t cnt = 0;

	if (systimer_base) {
		uint32_t low = 0;

		cnt = sched_clock();
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
		aee_rr_rec_wdk_ktime(cnt);
#endif

		low = readl(systimer_base + SYSTIMER_CNTCV_L);
		cnt = readl(systimer_base + SYSTIMER_CNTCV_H);
		cnt = cnt << 32 | low;
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
		aee_rr_rec_wdk_systimer_cnt(cnt);
#endif
	}

	switch (mode) {
	case PM_HIBERNATION_PREPARE:
	case PM_SUSPEND_PREPARE:
	case PM_RESTORE_PREPARE:
		lastsuspend_t = sched_clock();
		lastsuspend_syst = cnt;

		spin_lock_bh(&lock);
		del_timer_sync(&aee_dump_timer);
		aee_dump_timer_t = 0;
		g_hang_detected = 0;
		spin_unlock_bh(&lock);
		break;

	case PM_POST_SUSPEND:
	case PM_POST_HIBERNATION:
	case PM_POST_RESTORE:
		lastresume_t = sched_clock();
		lastresume_syst = cnt;
		break;
	}

	return 0;
}

static int __init init_wk_check_bit(void)
{
	int i = 0;

	pr_debug("[wdk]arch init check_bit=0x%x+++++\n", cpus_kick_bit);
	for (i = 0; i < CPU_NR; i++) {
		if (cpu_online(i))
			wk_cpu_update_bit_flag(i, 1, 1);
	}

	pr_debug("[wdk]arch init check_bit=0x%x-----\n", cpus_kick_bit);
	return 0;
}

static void wdt_mark_stage(unsigned int stage)
{
	unsigned int reg = ioread32(toprgu_base + WDT_NONRST_REG2);

	reg = (reg & ~(WDT_STAGE_MASK << WDT_STAGE_OFS))
		| (stage << WDT_STAGE_OFS);
	iowrite32(reg, toprgu_base + WDT_NONRST_REG2);
}

static const struct of_device_id toprgu_of_match[] = {
	{ .compatible = "mediatek,mt6589-wdt" },
	{},
};

static const struct of_device_id systimer_of_match[] = {
	{ .compatible = "mediatek,mt6765-timer" },
	{},
};

static int __init hangdet_init(void)
{
	int res = 0;
#if IS_ENABLED(CONFIG_MTK_TICK_BROADCAST_DEBUG)
	unsigned int systirq = 0;
#endif
	struct device_node *np_toprgu, *np_systimer;

	for_each_matching_node(np_toprgu, toprgu_of_match) {
		pr_info("%s: compatible node found: %s\n",
			 __func__, np_toprgu->name);
		break;
	}

	toprgu_base = of_iomap(np_toprgu, 0);
	if (!toprgu_base)
		pr_debug("toprgu iomap failed\n");
	else
		wdt_mark_stage(WDT_STAGE_KERNEL);

	for_each_matching_node(np_systimer, systimer_of_match) {
		pr_info("%s: compatible node found: %s\n",
			 __func__, np_systimer->name);
		break;
	}

	systimer_base = of_iomap(np_systimer, 0);
	if (!systimer_base)
		pr_debug("systimer iomap failed\n");

#if IS_ENABLED(CONFIG_MTK_TICK_BROADCAST_DEBUG)
	systirq = irq_of_parse_and_map(np_systimer, 0);
	if (systirq <= 0)
		systimer_irq = 0;
	else
		systimer_irq = systirq;
#endif
	init_wk_check_bit();

	wdk_workqueue = create_singlethread_workqueue("mt-wdk");
	INIT_WORK(&wdk_work, wdk_work_callback);

	res = queue_work(wdk_workqueue, &wdk_work);

	if (!res)
		pr_info("[wdk]wdk_work start return:%d!\n", res);

	wdt_pm_nb.notifier_call = wdt_pm_notify;
	register_pm_notifier(&wdt_pm_nb);

	if (systimer_base) {
		uint64_t cnt;
		uint32_t low;

		low = readl(systimer_base + SYSTIMER_CNTCV_L);
		cnt = readl(systimer_base + SYSTIMER_CNTCV_H);
		cnt = cnt << 32 | low;
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
		aee_rr_rec_wdk_systimer_cnt(cnt);
#endif
		pr_info("%s systimer_cnt %lld\n", __func__, cnt);

		cnt = sched_clock();
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
		aee_rr_rec_wdk_ktime(cnt);
#endif
		pr_info("%s set wdk_ktime %lld\n", __func__, cnt);
	}

	timer_setup(&aee_dump_timer, aee_dump_timer_func, 0);

	return 0;
}

static void __exit hangdet_exit(void)
{
	unregister_pm_notifier(&wdt_pm_nb);
	kthread_stop((struct task_struct *)wk_tsk);
}

module_init(hangdet_init);
module_exit(hangdet_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mediatek inc.");
MODULE_DESCRIPTION("The cpu hang detector");
