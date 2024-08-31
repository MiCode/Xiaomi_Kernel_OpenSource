// MIUI ADD: Performance_FramePredictBoost
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ctype.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/kthread.h>
#include <linux/preempt.h>
#include <linux/sched/cputime.h>
#include <linux/sched/task.h>
#include <linux/string.h>
#include <linux/unistd.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/cpufreq.h>
#include <trace/events/power.h>
#include <linux/tracepoint.h>
#include <linux/kallsyms.h>
#include <uapi/linux/sched/types.h>
#include <trace/hooks/cpufreq.h>

#include "hyperframe_cpu_loading_track.h"
#include "hyperframe_cpu_tracer.h"

#define SCHEDULE_RECORD_LIST_SIZE 100
#define SCHEDULE_SWITCH_RECORD_LIST_SIZE 100

static DEFINE_MUTEX(data_lock);

// static DEFINE_SPINLOCK(freq_slock);
static DEFINE_SPINLOCK(switch_slock);

struct hyperframe_cpu_freq **cluster_freq_queue = NULL;
int* freq_idx;

struct hyperframe_sched_switch* uithread_sched_switch_queue = NULL;
struct hyperframe_sched_switch* renderthread_sched_switch_queue = NULL;
int uithread_idx = 0;
int renderthread_idx = 0;

bool started = false;
bool first_started = false;
bool initialized = false;

int ui_thread_id = 0;
int render_thread_id = 0;

extern int hf_cluster_num;
extern bool hyperframe_predict_intra_running;

void set_ui_and_renderthread(int ui_thread, int render_thread, int is_focus)
{
	int cluster;

	if (!initialized)
		return;

	if (ui_thread <= 0 || render_thread <= 0) {
		end_cpu_tracer();
		return;
	}

	if (ui_thread_id == ui_thread && render_thread_id == render_thread) {
		if (is_focus)
			return;

		ui_thread_id = 0;
		render_thread_id = 0;
		hyperframe_predict_intra_running = false;
		end_cpu_tracer();
		hyperframe_reset_cpufreq_qos(ui_thread);
	} else {
		if (!is_focus)
			return;

		ui_thread_id = ui_thread;
		render_thread_id = render_thread;
		memset(uithread_sched_switch_queue, 0,
				SCHEDULE_SWITCH_RECORD_LIST_SIZE * sizeof(struct hyperframe_sched_switch));
		memset(renderthread_sched_switch_queue, 0,
				SCHEDULE_SWITCH_RECORD_LIST_SIZE * sizeof(struct hyperframe_sched_switch));
		uithread_idx = 0;
		renderthread_idx = 0;
		hyperframe_predict_intra_running = true;

		cluster = hyperframe_init_sched_policy(ui_thread_id, render_thread_id);
		start_cpu_tracer();
	}
}

// handle data start
void start_cpu_tracer(void)
{
	unsigned long long ts;
	int i;
	if (started)
		return;

	ts = nsec_to_100usec(sched_clock());

	for (i = 0; i < hf_cluster_num; i++) {
		struct hyperframe_cpu_freq *hb_c_freq;
		hb_c_freq = cluster_freq_queue[i];
		hb_c_freq->ts = ts;
		hb_c_freq->cluster = i;
		hb_c_freq->freq = 0; //TODO: FIXME sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq
	}

	if (!first_started)
		first_started = true;

	started = true;
}

void end_cpu_tracer(void)
{
	started = false;
}

void add_freq_data(int cluster, int freq)
{
	struct hyperframe_cpu_freq *hb_c_freq;

	if (!first_started)
		return;

	hb_c_freq = (struct hyperframe_cpu_freq*)cluster_freq_queue[cluster] + freq_idx[cluster];
	hb_c_freq->ts = nsec_to_100usec(sched_clock());
	hb_c_freq->cluster = cluster;
	hb_c_freq->freq = freq;

	idx_add_self(freq_idx + cluster, 1, SCHEDULE_RECORD_LIST_SIZE);
}

void add_sched_switch(unsigned int cid, struct task_struct *prev, struct task_struct *next)
{
	struct hyperframe_sched_switch *sched_switch = NULL;
	unsigned long flags_switch;
	char buf[256];
	int buf_len;
	int sched_idx = 0;

	if (!started)
		return;

	spin_lock_irqsave(&switch_slock, flags_switch);
	if (next->pid == ui_thread_id || prev->pid == ui_thread_id) {
		sched_switch = uithread_sched_switch_queue + uithread_idx;
		sched_switch->cid = cid;
		sched_switch->pid = ui_thread_id;
		sched_switch->ts = nsec_to_100usec(sched_clock());
		if (next->pid == ui_thread_id)
			sched_switch->running = true;
		else
			sched_switch->running = false;

		sched_idx = uithread_idx;
		uithread_idx++;
		if (uithread_idx >= SCHEDULE_SWITCH_RECORD_LIST_SIZE)
			uithread_idx = 0;
	}

	if (next->pid == render_thread_id || prev->pid == render_thread_id) {
		sched_switch = renderthread_sched_switch_queue + renderthread_idx;
		sched_switch->cid = cid;
		sched_switch->pid = render_thread_id;
		sched_switch->ts = nsec_to_100usec(sched_clock());
		if (next->pid == render_thread_id) {
			sched_switch->running = true;
		} else {
			sched_switch->running = false;
		}
		sched_idx = renderthread_idx;
		renderthread_idx++;
		if (renderthread_idx >= SCHEDULE_SWITCH_RECORD_LIST_SIZE)
			renderthread_idx = 0;
	}

	if (sched_switch == NULL) {
		spin_unlock_irqrestore(&switch_slock, flags_switch);
		return;
	}

	memset(buf, 0, sizeof(buf));
	buf_len = snprintf(buf, sizeof(buf), "HYPERFRAME-INFO: switch_time-%d", sched_switch->pid);
	if (unlikely(buf_len == 256))
		buf[255] = '\0';
	if (buf_len > 0)
		htrace_c_cpu_debug(ui_thread_id, sched_switch->ts, buf);

	memset(buf, 0, sizeof(buf));
	buf_len = snprintf(buf, sizeof(buf), "HYPERFRAME-INFO: switch-idx-%d", sched_switch->pid);
	if (unlikely(buf_len == 256))
		buf[255] = '\0';
	if (buf_len > 0)
		htrace_c_cpu_debug(ui_thread_id, sched_idx, buf);

	memset(buf, 0, sizeof(buf));
	buf_len = snprintf(buf, sizeof(buf), "HYPERFRAME-INFO: switch-running-%d", sched_switch->pid);
	if (unlikely(buf_len == 256))
		buf[255] = '\0';
	if (buf_len > 0)
		htrace_c_cpu_debug(ui_thread_id, sched_switch->running, buf);

	spin_unlock_irqrestore(&switch_slock, flags_switch);
}

int calculate_frame_loading(int pid, struct frame_info* frame, int vsync_id)
{
	uint64 *loading;
	int latest_idx;
	int rel;

	struct hyperframe_sched_switch* sched_switch_queue = NULL;
	struct hyperframe_sched_switch *first_sched, *last_sched;
	struct hyperframe_sched_switch *pos, *next;

	t_time *runningtime;
	t_time *start_time;
	t_time *end_time;

	unsigned long flags_switch;

	int first_idx = -1;
	int last_idx = -1;
	int switch_latest_idx;
	int switch_latest_next_idx;
	int tmp_idx = -1;
	int tmp_idx_2 = -1;
	bool running = false;

	first_sched = NULL;
	last_sched = NULL;
	pos = NULL;
	next = NULL;

	latest_idx = get_frame_idx_by_vsyncid(frame, vsync_id);
	if (latest_idx == -1)
		return -1;

	spin_lock_irqsave(&switch_slock, flags_switch);
	if (pid == ui_thread_id) {
		start_time = &(frame->t_doframe[latest_idx].t_start);
		end_time = &(frame->t_drawframes[latest_idx].t_start);
		runningtime = &(frame->ui_running_time[latest_idx]);
		loading = &(frame->ui_loading[latest_idx]);
		*loading = 0;
		sched_switch_queue = uithread_sched_switch_queue;
		switch_latest_idx = idx_sub_safe(uithread_idx, 1, SCHEDULE_SWITCH_RECORD_LIST_SIZE);
		switch_latest_next_idx = uithread_idx;
	} else if (pid == render_thread_id) {
		start_time = &(frame->t_drawframes[latest_idx].t_start);
		end_time = &(frame->t_drawframes[latest_idx].t_end);
		runningtime = &(frame->render_running_time[latest_idx]);
		loading = &(frame->render_loading[latest_idx]);
		*loading = 0;
		sched_switch_queue = renderthread_sched_switch_queue;
		switch_latest_idx = idx_sub_safe(renderthread_idx, 1, SCHEDULE_SWITCH_RECORD_LIST_SIZE);
		switch_latest_next_idx = renderthread_idx;
	} else {
		spin_unlock_irqrestore(&switch_slock, flags_switch);
		return -1;
	}

	idx_add(&tmp_idx_2, switch_latest_idx, 1, SCHEDULE_SWITCH_RECORD_LIST_SIZE);

	for (tmp_idx = switch_latest_idx; tmp_idx != tmp_idx_2; idx_sub_self(&tmp_idx, 1, SCHEDULE_SWITCH_RECORD_LIST_SIZE)) {
		unsigned long long ts = 0;
		running = (sched_switch_queue + tmp_idx)->running;
		ts = (sched_switch_queue + tmp_idx)->ts;
		if (last_idx < 0 && ts <= *end_time)
			last_idx = tmp_idx;
		if (first_idx < 0 && ts <= *start_time)
			first_idx = tmp_idx;
		if (first_idx >= 0 && last_idx >= 0)
			break;
		if (ts == 0)
			break;
	}

	if (first_idx < 0 || last_idx < 0) {
		spin_unlock_irqrestore(&switch_slock, flags_switch);
		return -1;
	}

	htrace_b_cpu(current->tgid,
			"HYPERFRAME-INTRA: tid - %d, frame_idx - %d, first_idx - %d, last_idx - %d, s - %d, e - %d",
			pid, latest_idx, first_idx, last_idx, *start_time, *end_time);
	htrace_e_cpu();

	if (first_idx == last_idx) {
		if (!sched_switch_queue[first_idx].running) {
			spin_unlock_irqrestore(&switch_slock, flags_switch);
			return -1;
		}
		*runningtime = *end_time - *start_time;
		rel = hyperframe_find_freq_add_loading((sched_switch_queue + first_idx)->cid, *start_time, *end_time, loading);
		if (rel < 0) {
			*loading = 0;
			spin_unlock_irqrestore(&switch_slock, flags_switch);
			return -1;
		}

		htrace_b_cpu(current->tgid, "INTRA: same loading - %d", *loading);
		htrace_e_cpu();
	} else {
		tmp_idx = first_idx;
		tmp_idx_2 = idx_add_safe(first_idx, 1, SCHEDULE_SWITCH_RECORD_LIST_SIZE);
		if ((sched_switch_queue + first_idx)->running) {
			rel = hyperframe_find_freq_add_loading(
					sched_switch_queue[tmp_idx].cid, *start_time, sched_switch_queue[tmp_idx_2].ts, loading);
			if (rel < 0) {
				*loading = 0;
				spin_unlock_irqrestore(&switch_slock, flags_switch);
				return -1;
			}

			htrace_b_cpu_debug(current->tgid, "HYPERFRAME-LOADING: start loading - %d, idx - %d", *loading, tmp_idx);
			htrace_e_cpu_debug();
		}

		idx_add_self(&tmp_idx, 1, SCHEDULE_SWITCH_RECORD_LIST_SIZE);
		idx_add_self(&tmp_idx_2, 1, SCHEDULE_SWITCH_RECORD_LIST_SIZE);

		do {
			if (tmp_idx == last_idx) {
				if (sched_switch_queue[tmp_idx].running) {
					*runningtime += *end_time - (sched_switch_queue + tmp_idx)->ts;
					rel = hyperframe_find_freq_add_loading((sched_switch_queue + tmp_idx)->cid,
							(sched_switch_queue + tmp_idx)->ts,
							*end_time,
							loading);
					if (rel < 0) {
						*loading = 0;
						spin_unlock_irqrestore(&switch_slock, flags_switch);
						return -1;
					}

					htrace_b_cpu_debug(current->tgid, "HYPERFRAME-LOADING: end loading - %d, idx - %d", *loading, tmp_idx);
					htrace_e_cpu_debug();
				}
				break;
			} else {
				if ((sched_switch_queue + tmp_idx)->running) {
					*runningtime += (sched_switch_queue + tmp_idx_2)->ts - (sched_switch_queue + tmp_idx)->ts;
					rel = hyperframe_find_freq_add_loading((sched_switch_queue + tmp_idx)->cid,
						(sched_switch_queue + tmp_idx)->ts,
						(sched_switch_queue + tmp_idx_2)->ts,
						loading);
					if (rel < 0) {
						*loading = 0;
						spin_unlock_irqrestore(&switch_slock, flags_switch);
						return -1;
					}

					htrace_b_cpu_debug(current->tgid, "HYPERFRAME-LOADING: normal loading - %d, idx - %d", *loading, tmp_idx);
					htrace_e_cpu_debug();
				}
			}
			idx_add_self(&tmp_idx, 1, SCHEDULE_SWITCH_RECORD_LIST_SIZE);
			idx_add_self(&tmp_idx_2, 1, SCHEDULE_SWITCH_RECORD_LIST_SIZE);
		} while (true);
	}

	htrace_b_cpu(current->tgid, "HYPERFRAME-LOADING: tid - %d, vsyncid - %d, loading - %d", pid, vsync_id, *loading);
	htrace_e_cpu();

	spin_unlock_irqrestore(&switch_slock, flags_switch);

	return 0;
}

/*
* start_time: running start
* end_time: running end
*/
int hyperframe_find_freq_add_loading(int cluster, t_time start_time, t_time end_time, uint64* loading)
{
	struct hyperframe_cpu_freq *freq_queue;
	int capacity;
	int first_freq_idx, last_freq_idx;
	int tmp_idx;
	int tmp_idx_2;
	int freq = 0;

	int debug_count = 0;
	first_freq_idx = -1;
	last_freq_idx = -1;

	freq_queue = (struct hyperframe_cpu_freq*)cluster_freq_queue[cluster];

	idx_add(&tmp_idx_2, freq_idx[cluster], 1, SCHEDULE_RECORD_LIST_SIZE);

	for (tmp_idx = freq_idx[cluster]; tmp_idx != tmp_idx_2; idx_sub_self(&tmp_idx, 1, SCHEDULE_RECORD_LIST_SIZE)) {
		if (last_freq_idx < 0 && (freq_queue + tmp_idx)->ts <= end_time)
			last_freq_idx = tmp_idx;
		if (first_freq_idx < 0 && (freq_queue + tmp_idx)->ts <= start_time)
			first_freq_idx = tmp_idx;
		if (first_freq_idx >= 0 && last_freq_idx >= 0)
			break;
	}

	if (first_freq_idx < 0 || last_freq_idx < 0)
		return -1;

	if (first_freq_idx == last_freq_idx) {
		freq = freq_queue[first_freq_idx].freq;

		htrace_b_cpu_debug(current->tgid, "[HYPERFRAME#SCHED#%s] start freq: %d", __func__, freq);
		htrace_e_cpu_debug();

		capacity = get_capacity_by_freq(cluster, freq);
		if (capacity < 0)
			return -1;
		*loading += (end_time - start_time) * capacity;
	} else {
		tmp_idx = first_freq_idx;
		idx_add(&tmp_idx_2, first_freq_idx, 1, SCHEDULE_RECORD_LIST_SIZE);
		debug_count = 0;
		do {
			freq = (freq_queue + tmp_idx)->freq;

			htrace_b_cpu_debug(current->tgid,
					"[HYPERFRAME#SCHED#%s] normal freq: %d", __func__, freq);
			htrace_e_cpu_debug();

			capacity = get_capacity_by_freq(cluster, freq);
			if (tmp_idx == first_freq_idx)
				*loading += ((freq_queue + tmp_idx_2)->ts - start_time) * capacity;
			else
				*loading += ((freq_queue + tmp_idx_2)->ts - (freq_queue + tmp_idx)->ts) * capacity;

			idx_add_self(&tmp_idx, 1, SCHEDULE_RECORD_LIST_SIZE);
			idx_add_self(&tmp_idx_2, 1, SCHEDULE_RECORD_LIST_SIZE);

			if (tmp_idx_2 == last_freq_idx) {
				freq = (freq_queue + tmp_idx)->freq;

				htrace_b_cpu_debug(current->tgid,
						"[HYPERFRAME#SCHED#%s] end freq: %d", __func__, freq);
				htrace_e_cpu_debug();

				capacity = get_capacity_by_freq(cluster, freq);
				*loading += ((freq_queue + tmp_idx_2)->ts - (freq_queue + tmp_idx)->ts) * capacity;

				freq = (freq_queue + tmp_idx)->freq;

				htrace_b_cpu_debug(current->tgid,
						"[HYPERFRAME#SCHED#%s] end freq: %d", __func__, freq);
				htrace_e_cpu_debug();

				capacity = get_capacity_by_freq(cluster, freq);
				*loading += (end_time - (freq_queue + tmp_idx_2)->ts) * capacity;
				break;
			}
			debug_count++;
		} while (true);
	}

	return 0;
}

void notify_frame_loading_update(int pid, int vsync_id)
{
	struct frame_info *frame = NULL;

	if (!started)
		return;

	frame = get_frame_info(pid);

	if (frame == NULL)
		return;

	htrace_b_cpu(current->tgid, "HYPERFRAME-LOADING: calculate ui loading");
	calculate_frame_loading(frame->pid, frame, vsync_id);
	htrace_e_cpu();

	htrace_b_cpu(current->tgid, "HYPERFRAME-LOADING: calculate render loading");
	calculate_frame_loading(frame->render_tid, frame, vsync_id);
	htrace_e_cpu();
}

int hyperframe_init_cluster_num(void)
{
	int cpu, num = 0;
	struct cpufreq_policy *policy;

	for_each_possible_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);
		if (!policy) {
			num = 0;
			break;
		}
		num++;
		cpu = cpumask_last(policy->related_cpus);
		cpufreq_cpu_put(policy);
	}

	return num;
}

void hyperframe_cpu_loading_track_init(void)
{
	int i;
	hf_cluster_num = hyperframe_init_cluster_num();
	if (hf_cluster_num <= 0)
		return;

	cluster_freq_queue = kcalloc(hf_cluster_num, sizeof(struct hyperframe_cpu_freq*), GFP_KERNEL);
	freq_idx = kcalloc(hf_cluster_num, sizeof(int), GFP_KERNEL);
	for (i = 0; i < hf_cluster_num; i++) {
		cluster_freq_queue[i] = kcalloc(SCHEDULE_RECORD_LIST_SIZE, sizeof(struct hyperframe_cpu_freq), GFP_KERNEL);
		memset(cluster_freq_queue[i], 0, SCHEDULE_RECORD_LIST_SIZE * sizeof(struct hyperframe_cpu_freq));
	}

	memset(freq_idx, 0, hf_cluster_num * sizeof(int));
	uithread_sched_switch_queue = kcalloc(SCHEDULE_SWITCH_RECORD_LIST_SIZE, sizeof(struct hyperframe_sched_switch), GFP_KERNEL);
	renderthread_sched_switch_queue = kcalloc(SCHEDULE_SWITCH_RECORD_LIST_SIZE, sizeof(struct hyperframe_sched_switch), GFP_KERNEL);
	memset(uithread_sched_switch_queue, 0, SCHEDULE_SWITCH_RECORD_LIST_SIZE * sizeof(struct hyperframe_sched_switch));
	memset(renderthread_sched_switch_queue, 0, SCHEDULE_SWITCH_RECORD_LIST_SIZE * sizeof(struct hyperframe_sched_switch));

	hyperframe_cpu_tracer_init();

	initialized = true;
}

void hyperframe_cpu_loading_track_exit(void)
{
	int i;
	hyperframe_cpu_tracer_exit();
	kfree(uithread_sched_switch_queue);
	kfree(renderthread_sched_switch_queue);
	for(i = 0; i < hf_cluster_num; i++) {
		kfree(cluster_freq_queue[i]);
	}
	kfree(cluster_freq_queue);
	kfree(freq_idx);
}
// END Performance_FramePredictBoost