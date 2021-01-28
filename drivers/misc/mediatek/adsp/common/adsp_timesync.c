// SPDX-License-Identifier: GPL-2.0
//
// adsp_timesync.c--  Mediatek ADSP timesync
//
// Copyright (c) 2020 MediaTek Inc.
// Author: Celine Liu <Celine.liu@mediatek.com>

#include <linux/clocksource.h>
#include <linux/sched/clock.h>
#include <linux/suspend.h>
#include <linux/timex.h>
#include <linux/types.h>
#include <linux/io.h>
#include <asm/arch_timer.h>
#include <asm/timex.h>
#include "adsp_helper.h"
#include "adsp_timesync.h"

#ifdef CONFIG_ARM64
#define IOMEM(a)       ((void __force __iomem *)((a)))
#endif

#define adsp_reg_sync_writel(v, a) \
	do { \
		__raw_writel((v), IOMEM(a)); \
		dsb(sy); \
	} while (0)

#define TIMESYNC_TAG                      "[ADSP_TS]"

#define TIMESYNC_MAX_VER             (0xFF)

#define TIMESYNC_FLAG_SYNC     (1 << 0)
#define TIMESYNC_FLAG_ASYNC    (1 << 1)
#define TIMESYNC_FLAG_FREEZE   (1 << 2)
#define TIMESYNC_FLAG_UNFREEZE (1 << 3)

/* sched_clock wrap time is 4398 seconds for arm arch timer
 * applying a period less than it for tinysys timesync
 */
#define TIMESYNC_WRAP_TIME (4000*NSEC_PER_SEC)

struct adsp_ts_context_t {
	spinlock_t lock;
	struct work_struct work;
	ktime_t wrap_kt;
	u8 enabled;
	u8 init_synced;
	u64 base_tick;
	u64 base_ts;
};

static struct workqueue_struct *adsp_ts_workqueue;
static struct adsp_ts_context_t adsp_ts_ctx;
static struct timecounter adsp_ts_counter;
static struct hrtimer adsp_ts_refresh_timer;
static u32 adsp_base_ver;

static int adsp_ts_update(int fz, u64 tick, u64 ts)
{
	adsp_base_ver = (adsp_base_ver+1) & TIMESYNC_MAX_VER;
	if ((is_adsp_ready(ADSP_A_ID) == 1) || adsp_feature_is_active()
		|| adsp_ts_ctx.init_synced == 0) {
		adsp_reg_sync_writel((tick >> 32) & 0xFFFFFFFF,
			ADSP_TIMESYNC_TICK_H);
		adsp_reg_sync_writel(tick & 0xFFFFFFFF,
			ADSP_TIMESYNC_TICK_L);
		adsp_reg_sync_writel((ts >> 32) & 0xFFFFFFFF,
			ADSP_TIMESYNC_TS_H);
		adsp_reg_sync_writel(ts & 0xFFFFFFFF,
			ADSP_TIMESYNC_TS_L);
		adsp_reg_sync_writel(fz,
			ADSP_TIMESYNC_FREEZE);
		return 0;
	}
	return -1;
}

static u64 adsp_ts_tick_read(const struct cyclecounter *cc)
{
	return arch_timer_read_counter();
}

static struct cyclecounter adsp_timesync_cc __ro_after_init = {
	.read        = adsp_ts_tick_read,
	.mask        = CLOCKSOURCE_MASK(56),
};

static void adsp_timesync_sync_base_internal(unsigned int flag)
{
	u64 tick, ts;
	unsigned long irq_flags = 0;
	int freeze, unfreeze;
	int updated;

	spin_lock_irqsave(&adsp_ts_ctx.lock, irq_flags);

	ts =  timecounter_read(&adsp_ts_counter);
	tick = adsp_ts_counter.cycle_last;

	adsp_ts_ctx.base_tick = tick;
	adsp_ts_ctx.base_ts = ts;

	freeze = (flag & TIMESYNC_FLAG_FREEZE) ? 1 : 0;
	unfreeze = (flag & TIMESYNC_FLAG_UNFREEZE) ? 1 : 0;

	/* sync with adsp */
	updated = adsp_ts_update(freeze, tick, ts);

	spin_unlock_irqrestore(&adsp_ts_ctx.lock, irq_flags);

	pr_info("%s update base: updated=%d, ts=%llu, tick=0x%llx, fz=%d, ver=%d\n",
		TIMESYNC_TAG, updated, ts, tick, freeze, adsp_base_ver);
}

void adsp_timesync_sync_base(unsigned int flag)
{
	if (!adsp_ts_ctx.enabled)
		return;

	if (flag & TIMESYNC_FLAG_ASYNC)
		queue_work(adsp_ts_workqueue, &(adsp_ts_ctx.work));
	else
		adsp_timesync_sync_base_internal(flag);
}

static enum hrtimer_restart adsp_ts_refresh(struct hrtimer *hrt)
{
	hrtimer_forward_now(hrt, adsp_ts_ctx.wrap_kt);
	/* snchronize new sched_clock base to co-processors */
	adsp_timesync_sync_base(TIMESYNC_FLAG_ASYNC);

	return HRTIMER_RESTART;
}

static void adsp_timesync_ws(struct work_struct *ws)
{
	adsp_timesync_sync_base(TIMESYNC_FLAG_SYNC);
}

int __init adsp_timesync_init(void)
{
	adsp_ts_workqueue = create_workqueue("adsp_ts_wq");
	if (!adsp_ts_workqueue) {
		pr_info("%s workqueue create failed\n", __func__);
		adsp_ts_ctx.enabled = 0;
		adsp_ts_ctx.init_synced = 0;
		return -1;
	}

	INIT_WORK(&(adsp_ts_ctx.work), adsp_timesync_ws);

	spin_lock_init(&adsp_ts_ctx.lock);

	/* init cyclecounter mult and shift as sched_clock */
	clocks_calc_mult_shift(&adsp_timesync_cc.mult, &adsp_timesync_cc.shift,
				arch_timer_get_cntfrq(), NSEC_PER_SEC, 3600);

	adsp_ts_ctx.wrap_kt = ns_to_ktime(TIMESYNC_WRAP_TIME);

	/* Init time counter:
	 * start_time: current sched_clock
	 * read: arch timer counter
	 */
	timecounter_init(&adsp_ts_counter, &adsp_timesync_cc, sched_clock());

	hrtimer_init(&adsp_ts_refresh_timer,
				CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	adsp_ts_refresh_timer.function = adsp_ts_refresh;
	hrtimer_start(&adsp_ts_refresh_timer,
		adsp_ts_ctx.wrap_kt, HRTIMER_MODE_REL);

	pr_info("%s ts: cycle_last %lld, time_base:%lld, wrap:%lld\n",
		TIMESYNC_TAG, adsp_ts_counter.cycle_last,
		adsp_ts_counter.nsec, adsp_ts_ctx.wrap_kt);

	adsp_ts_ctx.enabled = 1;

	adsp_timesync_sync_base(TIMESYNC_FLAG_SYNC);
	adsp_ts_ctx.init_synced = 1;

	return 0;
}

void adsp_timesync_suspend(void)
{
	if (!adsp_ts_ctx.enabled)
		return;

	hrtimer_cancel(&adsp_ts_refresh_timer);

	/* snchronize new sched_clock base to co-processors */
	adsp_timesync_sync_base(TIMESYNC_FLAG_SYNC |
		TIMESYNC_FLAG_FREEZE);
}
void adsp_timesync_resume(void)
{
	if (!adsp_ts_ctx.enabled)
		return;

	/* re-init timecounter because sched_clock will be stopped during
	 * suspend but arch timer counter is not, so we need to update
	 *  start time after resume
	 */
	timecounter_init(&adsp_ts_counter, &adsp_timesync_cc, sched_clock());

	hrtimer_start(&adsp_ts_refresh_timer,
		adsp_ts_ctx.wrap_kt, HRTIMER_MODE_REL);

	/* snchronize new sched_clock base to co-processors */
	adsp_timesync_sync_base(TIMESYNC_FLAG_SYNC |
		TIMESYNC_FLAG_UNFREEZE);
}

