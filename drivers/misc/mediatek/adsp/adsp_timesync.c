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
#include "adsp_core.h"
#include "adsp_platform.h"
#include "adsp_timesync.h"

struct timesync_info_s {
	u32 tick_h;
	u32 tick_l;
	u32 ts_h;
	u32 ts_l;
	u32 freeze;
	u32 version;
};

struct timesync_control_s {
	struct timecounter tc;
	struct cyclecounter cc;
	struct hrtimer timer;
	u32 period_ms;
	struct timesync_info_s infos;
};

static struct timesync_control_s timesync_ctrl;

static u64 adsp_ts_tick_read(const struct cyclecounter *cc)
{
	return arch_timer_read_counter();
}

static void adsp_timesync_update(u32 fz)
{
	u64 tick, ts;
	struct timesync_info_s *infos = &timesync_ctrl.infos;
	int ret = 0;

	ts =  timecounter_read(&timesync_ctrl.tc);
	tick = timesync_ctrl.tc.cycle_last;

	infos->tick_h = (tick >> 32) & 0xFFFFFFFF;
	infos->tick_l = tick & 0xFFFFFFFF;
	infos->ts_h = (ts >> 32) & 0xFFFFFFFF;
	infos->ts_l = ts & 0xFFFFFFFF;
	infos->freeze = fz;
	infos->version++;

	if (is_adsp_system_running()) {
		ret = adsp_copy_to_sharedmem(get_adsp_core_by_id(ADSP_A_ID),
					ADSP_SHAREDMEM_TIMESYNC,
					infos, sizeof(*infos));
	}
}

static enum hrtimer_restart adsp_timesync_refresh(struct hrtimer *hrt)
{
	adsp_timesync_update(APTIME_UNFREEZE);

	hrtimer_forward_now(hrt, ms_to_ktime(timesync_ctrl.period_ms));
	return HRTIMER_RESTART;
}

int adsp_timesync_init(void)
{
	u32 multiplier, shifter;

	/* init cyclecounter mult and shift as sched_clock */
	clocks_calc_mult_shift(&multiplier,
			       &shifter,
			       arch_timer_get_cntfrq(),
			       NSEC_PER_SEC, 3600);

	timesync_ctrl.cc.read = adsp_ts_tick_read;
	timesync_ctrl.cc.mask = CLOCKSOURCE_MASK(56);
	timesync_ctrl.cc.mult = multiplier;
	timesync_ctrl.cc.shift = shifter;

	/* init time counter */
	timecounter_init(&timesync_ctrl.tc,
			 &timesync_ctrl.cc,
			 sched_clock());

	/* init refresh hr_timer */
	hrtimer_init(&timesync_ctrl.timer,
		     CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	timesync_ctrl.timer.function = adsp_timesync_refresh;
	timesync_ctrl.period_ms = TIMESYNC_WRAP_TIME_MS;

	pr_info("%s(), done", __func__);

	return 0;
}

void adsp_timesync_suspend(u32 fz)
{
	hrtimer_cancel(&timesync_ctrl.timer);

	/* update after suspend timer */
	adsp_timesync_update(fz);
}
void adsp_timesync_resume(void)
{
	/* re-init timecounter because sched_clock will be stopped during
	 * suspend but arch timer counter is not, so we need to update
	 *  start time after resume
	 */
	timecounter_init(&timesync_ctrl.tc,
			 &timesync_ctrl.cc,
			 sched_clock());

	/* kick start the timer immediately */
	hrtimer_start(&timesync_ctrl.timer, 0, HRTIMER_MODE_REL);
}

