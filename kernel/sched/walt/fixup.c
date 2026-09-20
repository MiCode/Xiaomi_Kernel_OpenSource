// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <trace/hooks/cpufreq.h>
#include <trace/hooks/sched.h>
#include <trace/hooks/topology.h>

#include <linux/delay.h>
#include "walt.h"
#include "trace.h"

unsigned int cpuinfo_max_freq_cached;

char sched_lib_name[LIB_PATH_LENGTH];
char sched_lib_task[LIB_PATH_LENGTH];
unsigned int sched_lib_mask_force;

static bool is_sched_lib_based_app(pid_t pid)
{
	const char *name = NULL;
	char *libname, *lib_list;
	struct vm_area_struct *vma;
	char path_buf[LIB_PATH_LENGTH];
	char *tmp_lib_name;
	bool found = false;
	struct task_struct *p;
	struct mm_struct *mm;

	if (strnlen(sched_lib_name, LIB_PATH_LENGTH) == 0)
		return false;

	tmp_lib_name = kmalloc(LIB_PATH_LENGTH, GFP_KERNEL);
	if (!tmp_lib_name)
		return false;

	rcu_read_lock();
	p = pid ? get_pid_task(find_vpid(pid), PIDTYPE_PID) : get_task_struct(current);
	rcu_read_unlock();
	if (!p) {
		kfree(tmp_lib_name);
		return false;
	}

	mm = get_task_mm(p);
	if (mm) {
		MA_STATE(mas, &mm->mm_mt, 0, 0);
		down_read(&mm->mmap_lock);

		mas_for_each(&mas, vma, ULONG_MAX) {
			if (vma->vm_file && vma->vm_flags & VM_EXEC) {
				name = d_path(&vma->vm_file->f_path,
						path_buf, LIB_PATH_LENGTH);
				if (IS_ERR(name))
					goto release_sem;

				strscpy(tmp_lib_name, sched_lib_name, LIB_PATH_LENGTH);
				lib_list = tmp_lib_name;
				while ((libname = strsep(&lib_list, ","))) {
					libname = skip_spaces(libname);
					if (strnstr(name, libname,
						strnlen(name, LIB_PATH_LENGTH))) {
						found = true;
						goto release_sem;
					}
				}
			}
		}

release_sem:
		up_read(&mm->mmap_lock);
		mmput(mm);

	}
	put_task_struct(p);
	kfree(tmp_lib_name);
	return found;
}

bool is_sched_lib_task(void)
{
	if (strnlen(sched_lib_task, LIB_PATH_LENGTH) == 0)
		return false;

	if (strnstr(current->comm, sched_lib_task, strnlen(current->comm, LIB_PATH_LENGTH)))
		return true;

	return false;
}

static void android_rvh_show_max_freq(void *unused, struct cpufreq_policy *policy,
				     unsigned int *max_freq)
{
	if (!cpuinfo_max_freq_cached)
		return;

	if (!(BIT(policy->cpu) & sched_lib_mask_force))
		return;

	if (is_sched_lib_based_app(current->pid) || is_sched_lib_task())
		*max_freq = cpuinfo_max_freq_cached << 1;
}

static void android_rvh_cpu_capacity_show(void *unused,
		unsigned long *capacity, int cpu)
{
	if (!soc_sched_lib_name_capacity)
		return;

	if ((is_sched_lib_based_app(current->pid) || is_sched_lib_task()) &&
			cpu < soc_sched_lib_name_capacity)
		*capacity = 100;
}

/* frequent yielder tracking */
u8 contiguous_yielding_windows;
unsigned int total_yield_cnt;
unsigned int total_sleep_cnt;

static u64 frame_from_ravg_window(void)
{
	if (sched_ravg_window <= SCHED_RAVG_8MS_WINDOW)
		return FRAME120_WINDOW_NSEC;
	if (sched_ravg_window == SCHED_RAVG_12MS_WINDOW)
		return FRAME90_WINDOW_NSEC;
	if (sched_ravg_window >= SCHED_RAVG_16MS_WINDOW)
		return FRAME60_WINDOW_NSEC;
	return 0;
}

void account_yields(u64 wallclock)
{
	static u64 yield_counting_window_ts;
	u64 delta = wallclock - yield_counting_window_ts;
	unsigned int target_threshold_wake = FORCE_MAX_YIELD_CNT_GLOBAL_THR_DEFAULT;
	unsigned int target_threshold_sleep = FORCE_MAX_YIELD_SLEEP_CNT_GLOBAL_THR;
	u8 continuous_window_th = FORCE_MIN_CONTIGUOUS_YIELDING_WINDOW;

	if (!sysctl_force_frequent_yielder)
		return;

	/* window boundary crossed */
	if (delta > YIELD_WINDOW_SIZE_NSEC) {
		/*
		 * if update_window_start comes more than
		 * YIELD_GRACE_PERIOD_NSEC after the YIELD_WINDOW_SIZE_NSEC then
		 * extrapolate the thresholds based on  delta time.
		 */

		if (unlikely(delta > YIELD_WINDOW_SIZE_NSEC + YIELD_GRACE_PERIOD_NSEC)) {
			target_threshold_wake = div64_u64(delta * target_threshold_wake,
					YIELD_WINDOW_SIZE_NSEC);
			target_threshold_sleep = div64_u64(delta * target_threshold_sleep,
					YIELD_WINDOW_SIZE_NSEC);
		}

		if ((total_yield_cnt >= target_threshold_wake) ||
				(total_sleep_cnt >= target_threshold_sleep / 2)) {
			if (contiguous_yielding_windows < continuous_window_th)
				contiguous_yielding_windows++;
		} else {
			contiguous_yielding_windows = 0;
		}

		trace_walt_account_yields(wallclock, yield_counting_window_ts,
				contiguous_yielding_windows,
				total_yield_cnt, target_threshold_wake,
				total_sleep_cnt, target_threshold_sleep);

		yield_counting_window_ts = wallclock;
		total_yield_cnt = 0;
		total_sleep_cnt = 0;
	}
}

/*
 * Sleep time prediction:
 * Mark two adjacent yields
 * cy: time now (current yield)
 * py: previous yield time stamp
 *
 * |-----|: useful work done during frame.
 * |*|*|*|: yields during frame
 *
 * Each frame consisit of two part:
 * 1. Part where useful processign work is done.
 * 2. Continuous yielding part and waiting for next frame to arrive.
 *
 * delta between cy and py decides the part of the frame
 * If cy - py < 1msec, signifies yield cycle within frame
 *                        py cy
 *                         | |
 *                         V V
 *   |-----------|*|*|*|*|*|*|*|*|
 *   |<------ curr frame ------->|
 *
 * If cy - py > 1ms, consider cy as start of new yielding cycle in the current frame.
 * Calculate new frames sleep headroom = (frame size - delta - 300ms)
 *              py                      cy
 *              |                       |
 *              V                       V
 * *|*|*|*|*|*|*|-----------------------|*|*|*|*|*|*|*|*|*|*|*|*|*|*|--
 * prev frame ->|<--------delta-------->|<- yield cycle ->|
 *              |                       |<--- sleep  ---->|<300 us> |
 *              |<----------------current frame-------------------->|
 */

DEFINE_PER_CPU(unsigned int, walt_yield_to_sleep);
static void inject_sleep(struct walt_task_struct *wts)
{
	struct rq *rq = task_rq(current);
	struct rq_flags rf;
	u64 current_ts = 0;
	u64 frame = 0, delta = 0, sleep_nsec = 0;

	/*
	 * updating and reading clock will not hurt here as this cpu is
	 * already in yield cycle not doing anything significant.
	 */
	rq_lock_irqsave(rq, &rf);
	update_rq_clock(rq);
	rq_unlock_irqrestore(rq, &rf);
	current_ts = rq->clock;
	if (current_ts > wts->yield_ts + MIN_FRAME_YIELD_INTERVAL_NSEC) {
		frame = frame_from_ravg_window();
		delta = current_ts - wts->yield_ts;
		wts->yield_total_sleep_usec = 0;
		if (frame > delta + YIELD_SLEEP_HEADROOM) {
			sleep_nsec = (frame - delta) - YIELD_SLEEP_HEADROOM;
			wts->yield_total_sleep_usec = sleep_nsec /
				NSEC_PER_USEC;
		}
	}
	wts->yield_ts = current_ts;
	if (wts->yield_total_sleep_usec >= YIELD_SLEEP_TIME_USEC) {
		per_cpu(walt_yield_to_sleep, raw_smp_processor_id())++;
		wts->yield_state |= YIELD_INDUCED_SLEEP;
		total_sleep_cnt++;
		usleep_range_state(YIELD_SLEEP_TIME_USEC,
				   YIELD_SLEEP_TIME_USEC, TASK_INTERRUPTIBLE);
		wts->yield_total_sleep_usec = wts->yield_total_sleep_usec -
			YIELD_SLEEP_TIME_USEC;
	}
}

static void walt_do_sched_yield_before(void *unused, long *skip)
{
	struct walt_task_struct *wts = (struct walt_task_struct *)current->android_vendor_data1;

	if (unlikely(walt_disabled))
		return;

	if (!walt_fair_task(current))
		return;

	if (!sysctl_force_frequent_yielder)
		return;

	if (pipeline_in_progress() && walt_pipeline_low_latency_task(current)) {
		*skip = true;
		inject_sleep(wts);
		return;
	}

	if ((wts->yield_state & YIELD_CNT_MASK) >= MAX_YIELD_CNT_PER_TASK_THR) {
		total_yield_cnt++;
		if (contiguous_yielding_windows >= FORCE_MIN_CONTIGUOUS_YIELDING_WINDOW) {
			*skip = true;
			inject_sleep(wts);
		}
	} else {
		wts->yield_state++;
	}
}

void walt_fixup_init(void)
{
	register_trace_android_rvh_show_max_freq(android_rvh_show_max_freq, NULL);
	register_trace_android_rvh_cpu_capacity_show(android_rvh_cpu_capacity_show, NULL);
	register_trace_android_rvh_before_do_sched_yield(walt_do_sched_yield_before, NULL);
}
