// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: PC Chen <pc.chen@mediatek.com>
 *       Tiffany Lin <tiffany.lin@mediatek.com>
 */
#include <linux/time.h>
#include <linux/time64.h>
#include <linux/ktime.h>
#include <linux/timekeeping.h>
#if !IS_ENABLED(CONFIG_64BIT)
#include <asm/div64.h>
#endif
#include <linux/slab.h>
#include "dvfs_v2.h"

#define DEFAULT_MHZ 99999
#define MAX_SUBMIT (33*1000)
#define SHOW_ALGO_INFO 0

long long div_64(long long a, long long b)
{
#if IS_ENABLED(CONFIG_64BIT)
	return (a/b);
#else
	uint32_t rem = 0;
	uint64_t dividend, divisor;

	if ((a >= 0) && (b > 0)) {
		dividend = (uint64_t)a;
		divisor = (uint64_t)b;
		rem = do_div(dividend, divisor);
		a = (long long)dividend;
	} else if ((a >= 0) && (b < 0)) {
		dividend = (uint64_t)a;
		divisor = (uint64_t)(-b);
		rem = do_div(dividend, divisor);
		a = 0LL - (long long)dividend;
	} else if ((a < 0) && (b > 0)) {
		dividend = (uint64_t)(-a);
		divisor = (uint64_t)b;
		rem = do_div(dividend, divisor);
		a = 0LL - (long long)dividend;
	} else {
		dividend = (uint64_t)(-a);
		divisor = (uint64_t)(-b);
		rem = do_div(dividend, divisor);
		a = (long long)dividend;
	}
	return a;
#endif
}


/**
 * get_time_us - Get current time in us
 *
 * To prevent overflow, long long is used for return value
 */
long long get_time_us(void)
{
	struct timespec64 tv;

	ktime_get_ts64(&tv);
	return (1000000LL * tv.tv_sec + tv.tv_nsec/10000);
}

/**
 * new_hist_to_end - add a new blank codec_history to end of list
 *
 * The newly added codec_history is returned and user is expected to
 * fill required information
 */
struct codec_history *new_hist_to_end(struct codec_history **head)
{
	struct codec_history *hist;
	struct codec_history *tgt = *head;

	hist = kmalloc(sizeof(struct codec_history), GFP_KERNEL);
	if (hist == 0)
		return hist;

	memset(hist, 0, sizeof(struct codec_history));

	if (tgt == 0) {
		*head = hist;
		tgt = *head;
	} else {
		while (tgt->next != 0)
			tgt = tgt->next;

		tgt->next = hist;
	}

	return hist;
}

/**
 * free_next_hist - free next codec_history and shift next_next one forward
 *                  (not used)
 *
 * Returns the new next codec_history element.
 */
struct codec_history *free_next_hist(struct codec_history *cur_hist)
{
	struct codec_history *next;

	if (cur_hist == 0)
		return 0;

	next = cur_hist->next;
	if (next != 0) {
		cur_hist->next = next->next;
		kfree(next);
	}

	return cur_hist->next;
}

/**
 * free_hist - remove target codec_history from list and free its memory
 *
 * Return 0 if free successfully
 *        -1 means there is error and nothing is changed
 */
int free_hist_(struct codec_history **head, struct codec_history *target)
{
	struct codec_history *temp;

	/* Either one is null, do nothing */
	if (target == 0 || (*head) == 0)
		return -1;

	/* Free head */
	if ((*head) == target) {
		*head = target->next;
		kfree(target);
	} else {
		/* Find target in list */
		temp = *head;
		while (temp->next != target && temp->next != 0)
			temp = temp->next;

		if (temp->next == target) {
			temp->next = target->next;
			kfree(target);
		} else {
			pr_info("VCODEC free history %p not found",
				target->handle);
			return -1;
		}
	}

	return 0;
}

/**
 * free_hist - Scan through the history list and free unused ones
 *
 * Return 0 for success
 *        -1 if error
 */
int free_hist(struct codec_history **head, int only_unused)
{
	struct codec_history *chk_hist; /* history item to check */
	struct codec_history *prev_hist;
	int hist_idx;
	int prev_idx;
	long long cur_time;

	if (head == 0)
		return -1;

	chk_hist = *head;
	prev_hist = chk_hist;
	cur_time = get_time_us();

	while (chk_hist != 0) {
		hist_idx = chk_hist->cur_idx;
		prev_idx = (hist_idx == 0) ? (MAX_HISTORY-1) : (hist_idx-1);

		if (only_unused == 0 ||
		    (cur_time - chk_hist->end[prev_idx]) > FREE_HIST_DELAY) {

			/* free head */
			if (chk_hist == *head) {
				*head = chk_hist->next;
				kfree(chk_hist);
				chk_hist = *head;
				prev_hist = chk_hist;
			} else { /* free other items */
				prev_hist->next = chk_hist->next;
				kfree(chk_hist);
				chk_hist = prev_hist->next;
			}
		} else { /* do nothing to current, check next */
			prev_hist = chk_hist;
			chk_hist = chk_hist->next;
		}
	}

	return 0;
}


/**
 * find_hist = find a codec_history from the list by handle
 *
 * Returns the codec_history
 *         0 if not found
 */
struct codec_history *find_hist(void *handle, struct codec_history *head)
{
	while (head != 0 && head->handle != handle)
		head = head->next;

	return head;
}

/**
 * find_calc_idx - helper function to find the starting (first_idx) and
 *                 previous (prev_idx) index of current history
 *
 * Return 0 if found indices
 *         -1 if there is no codec_history or history is empty
 */
int find_calc_idx(struct codec_history *hist, int *first_idx, int *prev_idx)
{
	if (hist == 0 || hist->cur_cnt == 0)
		return -1;

	*first_idx = hist->cur_idx - hist->cur_cnt;
	if (*first_idx < 0)
		*first_idx += MAX_HISTORY;


	*prev_idx = hist->cur_idx - 1;
	if (*prev_idx < 0)
		*prev_idx += MAX_HISTORY;

	return 0;
}

/**
 * est_next_submit - estimate next submission time by previous history
 *                   This function is called when a new job is submitted but
 *                   not yet completed & added to history. So the estimation
 *                   is really for the next_next job.
 *
 * Return time in us. 0 means no history available, treat it as coming now
 */
long long est_next_submit(struct codec_history *hist)
{
	int first_idx, prev_idx;
	long long next_submit;

	if (find_calc_idx(hist, &first_idx, &prev_idx) < 0)
		return 0;

	/* Add 2x estimated gap for next_next job */
	if (hist->cur_cnt == 1)
		return (hist->submit[prev_idx] + MIN_SUBMIT_GAP * 2);

#if SHOW_ALGO_INFO
	pr_info("%s first_idx %d(%lld) prev_idx %d(%lld)",
		__func__, first_idx, hist->submit[first_idx],
		prev_idx, hist->submit[prev_idx]);
#endif

	/* next_submit need to *2 because it's estimating 2 gaps after
	 * (1 for current, 1 for next)
	 */
	next_submit = div_64(
		(hist->submit[prev_idx] - hist->submit[first_idx]) * 2,
		(hist->cur_cnt - 1));

	if (next_submit > MAX_SUBMIT * 2) {
#if SHOW_ALGO_INFO
		pr_info("%s %lld -> MAX SUBMIT(%d)",
			__func__, next_submit, MAX_SUBMIT);
#endif
		next_submit = MAX_SUBMIT * 2;
	}

	return hist->submit[prev_idx] + next_submit;
}

/**
 * est_new_kcy - Estimate cycles required
 *
 * Return estimate k(10^3) cycles for new job
 */
int est_new_kcy(struct codec_history *hist)
{
	if (hist == 0)
		return 0;

	return (hist->tot_kcy / hist->cur_cnt);
}


/**
 * est_next_job - Estimate next job's required finish time and clock frequency
 * required
 *
 * Return 0 for success
 *        -1 for error
 */
int est_next_job(long long now_us, long long *t_us, int *kcy, int *min_mhz,
		struct codec_job *job, struct codec_history *head)
{
	struct codec_history *hist;
	long long deadline;
	long long exec_dur;
	long long new_mhz;

	if (t_us == 0 || kcy == 0 || min_mhz == 0 || job == 0)
		return -1;

	hist = find_hist(job->handle, head);

#if SHOW_ALGO_INFO
	pr_info("%s find_hist %p handle %p\n", __func__, hist,
		(hist == 0) ? 0 : hist->handle);
#endif

	/* This is a new instance - no history yet */
	if (hist == 0) {
		/* Set *t_us = now_us to signal full speed */
		*t_us = now_us;
#if SHOW_ALGO_INFO
		pr_info("%s not history yet, full speed\n", __func__);
#endif
	} else {
		*kcy += est_new_kcy(hist);
		deadline = est_next_submit(hist);
		if (deadline == 0)
			*t_us = now_us;
		else {
			if (deadline > now_us) {
				exec_dur = deadline - now_us;
				exec_dur = (exec_dur > (MAX_SUBMIT * 2)) ?
						(MAX_SUBMIT * 2) : exec_dur;
				new_mhz = div_64((*kcy) * 1000LL, exec_dur);
				if (new_mhz > *min_mhz)
					*min_mhz = (int)new_mhz;

				if (*min_mhz == 0)
					*min_mhz = 1;

				*t_us = now_us + div_64((*kcy) * 1000LL,
							(*min_mhz));
			} else {
				/**
				 * Overdue, set *t_us = now_us to signal full
				 * speed
				 */
				*t_us = now_us;
			}
		}
		/* Extra boost for first frame time */
		if (hist->cur_cnt < MAX_HISTORY)
			*t_us = now_us;

#if SHOW_ALGO_INFO
		pr_info("%s deadline %llu, kcy %d\n", __func__, deadline, *kcy);
#endif
	}

#if SHOW_ALGO_INFO
	pr_info("%s now_us %lld, target_us %lld, min_mhz %d\n",
		__func__, now_us, *t_us, *min_mhz);
#endif

	/* Stop estimating if no more job or worst time constraint is reached */
	if (job->next == 0 || (*t_us) == now_us)
		return 0;

	return est_next_job(now_us, t_us, kcy, min_mhz, job->next, head);
}

/**
 * update_hist_item - Use a completed job to update a history item
 *
 * Return 1 if previous history is cleared & only new job info stays
 *        0 if new job info is added to history
 */
int update_hist_item(struct codec_job *job, struct codec_history *hist)
{
	int hist_idx;
	int prev_idx;

	if (job->handle != hist->handle) {
		pr_info("VCODEC dvfs job - history mismatch\n");
		return -1;
	}

	hist_idx = hist->cur_idx;
	prev_idx = (hist_idx == 0) ? (MAX_HISTORY-1) : (hist_idx-1);

	/* Previous history is too far away, restart */
	if (hist->cur_cnt > 1 &&
		(job->submit - hist->submit[prev_idx]) > MAX_SUBMIT_GAP) {
#if SHOW_ALGO_INFO
		pr_info("%s %p, gap (%lld), reset hist\n",
			__func__, hist->handle,
			(job->submit-hist->submit[prev_idx]));
#endif
		memset(hist->kcy, 0, sizeof(int)*MAX_HISTORY);
		memset(hist->submit, 0, sizeof(long long)*MAX_HISTORY);
		memset(hist->start, 0, sizeof(long long)*MAX_HISTORY);
		memset(hist->end, 0, sizeof(long long)*MAX_HISTORY);

		hist->kcy[0] = (int)div_64(job->mhz * (job->end - job->start),
					1000LL);
		hist->submit[0] = job->submit;
		hist->start[0] = job->start;
		hist->end[0] = job->end;
		hist->cur_idx = 1; /* cur_idx = 0 + 1 (updated for next) */
		hist->cur_cnt = 1;
		hist->tot_kcy = hist->kcy[0];
		hist->tot_time = hist->end[0] - hist->start[0];

		return 1;
	}

	/* Update history */
	if (hist->cur_cnt == MAX_HISTORY) {
		hist->tot_kcy = hist->tot_kcy - hist->kcy[hist_idx] +
			(int)div_64(job->mhz * (job->end - job->start), 1000);
		hist->tot_time = hist->tot_time -
				(hist->end[hist_idx] - hist->start[hist_idx]) +
				(job->end - job->start);
#if SHOW_ALGO_INFO
		pr_info("%s 1 kcy %d, time %llu\n",
			__func__, hist->tot_kcy, hist->tot_time);
#endif
	} else {
		hist->cur_cnt++;
		hist->tot_kcy = hist->tot_kcy +
			(int)div_64(job->mhz * (job->end - job->start), 1000);
		hist->tot_time = hist->tot_time + (job->end - job->start);
#if SHOW_ALGO_INFO
		pr_info("%s 2 kcy %d, time %llu, cnt %d\n",
			__func__, hist->tot_kcy, hist->tot_time, hist->cur_cnt);
#endif
	}

	hist->kcy[hist_idx] = (int)div_64(job->mhz * (job->end - job->start),
					1000LL);
	hist->submit[hist_idx] = job->submit;
	hist->start[hist_idx] = job->start;
	hist->end[hist_idx] = job->end;

#if SHOW_ALGO_INFO
	pr_info("%s %p, mhz %d, sub %lld, start %lld, end %lld\n",
		__func__, hist->handle, job->mhz,
		job->submit, job->start, job->end);
#endif
	hist->cur_idx = (hist_idx + 1) % MAX_HISTORY;


	return 0;
}

/**
 * update_hist - Use a completed job to update corresponding history
 *
 * Return 0 for success
 *        1 for clear old history and update success
 *        -1 for not updated due to error
 */
int update_hist(struct codec_job *job, struct codec_history **head)
{
	struct codec_history *target;
	int ret;

	target = find_hist(job->handle, *head);
	if (target == 0) {
		target = new_hist_to_end(head);
		if (target == 0)
			return -1;

		target->handle = job->handle;
#if SHOW_ALGO_INFO
		pr_info("%s new history %p head %p\n", __func__, target, *head);
#endif
	}

	ret = update_hist_item(job, target);
	if (ret == 1) {
		/* Long pause, start over */
#if SHOW_ALGO_INFO
		pr_info("VCODEC dvfs start over for handle %p", job->handle);
#endif
	}

	return ret;
}


/**
 * add_job - Add a new job to job queue
 *
 * Return total job count after add
 *        -1 if error
 */
int add_job_(struct codec_job *job, struct codec_job **head)
{
	int job_cnt;
	struct codec_job *last_job;

	/* Error case */
	if (head == 0)
		return -1;

	/* New job is head */
	if (*head == 0) {
		*head = job;
		return 1;
	}

	/* Add job to tail */
	job_cnt = 1; /* at least head job exists */
	last_job = *head;
	while (last_job->next != 0) {
		if (last_job->handle == job->handle) {
			pr_info("VCODEC dvfs multiple jobs from same instance");
			return -1;
		}
		last_job = last_job->next;
		job_cnt++;
	}
	last_job->next = job;
	job_cnt++;

	return job_cnt;
}

/**
 * add_job - Add a new job with only the handle when entering lock hw
 *
 * Return new job just added
 *        0 if failed to add
 */
struct codec_job *add_job(void *handle, struct codec_job **head)
{
	struct codec_job *new_job;
	int add_result = 0;

	new_job = kmalloc(sizeof(struct codec_job), GFP_KERNEL);
	if (new_job == 0)
		return 0;

	memset(new_job, 0, sizeof(struct codec_job));

	/* New job with handle & current time */
	new_job->handle = handle;
	new_job->submit = get_time_us();

	add_result = add_job_(new_job, head);
	if (add_result < 0) {
		kfree(new_job);
		new_job = 0;
	}

	return new_job;
}

/**
 * move_job_to_head - Move the target job to head for faster access next time
 *                    (most recently used)
 *
 * Return pointer to the moved job (should now be the head)
 *        0 if job not found
 */
struct codec_job *move_job_to_head(void *handle, struct codec_job **head)
{
	struct codec_job *prev_job;
	struct codec_job *target_job;

	/* Empty job list, do nothing */
	if (*head == 0)
		return 0;

	/* Target job is head already, do nothing */
	if ((*head)->handle == handle)
		return *head;

	/* Search for target job */
	prev_job = *head;
	while (prev_job->next != 0 && prev_job->next->handle != handle)
		prev_job = prev_job->next;

	/* Job not found */
	if (prev_job->next == 0)
		return 0;

	/* Found job, move it to head */
	target_job = prev_job->next;
	prev_job->next = target_job->next;
	target_job->next = *head;
	*head = target_job;

	return target_job;
}

/**
 * est_freq - Estimate minimum running frequency that provides enough
 *            performance
 *
 * Return minimum required mhz to finish all jobs in time
 */
int est_freq(void *handle, struct codec_job **job, struct codec_history *head)
{
	long long cur_time;
	long long end_time;
	int kcy;
	int min_mhz;
	int est_res;
	struct codec_job *target_job;

	cur_time = get_time_us();
	end_time = cur_time;
	kcy = 0;
	min_mhz = 0;

	target_job = move_job_to_head(handle, job);

	/* Error case, just run at max freq */
	if (target_job == 0) {
		pr_info("%s job not found!\n", __func__);
		return DEFAULT_MHZ;
	}

	if (target_job != *job)
		pr_info("%s target_job != job queue head\n", __func__);

	est_res = est_next_job(cur_time, &end_time, &kcy, &min_mhz, target_job,
				head);

#if SHOW_ALGO_INFO
	pr_info("%s res %d, min_mhz %d\n", __func__, est_res, min_mhz);
#endif

	/* Error case or do it ASAP */
	if (est_res == -1 || (cur_time == end_time))
		return DEFAULT_MHZ;

	return min_mhz;
}

/**
 * match_freq - Match estimated vcodec frequency with available frequencies
 *
 * Match requested mhz to available mhz
 */
u64 match_freq(int target_mhz, u64 *freq_list, u32 freq_cnt)
{
	u64 res_mhz = DEFAULT_MHZ;
	int i;
	u64 target64;

	if (freq_list == 0)
		return 0;

	target64 = (u64)target_mhz;

	for (i = 0; i < freq_cnt ; i++) {
		if (freq_list[i] > target_mhz && freq_list[i] < res_mhz)
			res_mhz = freq_list[i];
	}

	/* target_mhz is higher than all available frequency, choose max freq */
	if (res_mhz == DEFAULT_MHZ)
		res_mhz = freq_list[0];

#if SHOW_ALGO_INFO
	pr_info("%s %d -> %llu\n", __func__, target_mhz, res_mhz);
#endif
	return res_mhz;
}



