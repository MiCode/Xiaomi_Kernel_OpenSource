// MIUI ADD: Performance_FramePredictBoost
#include <linux/cpu.h>
#include <linux/cpufreq.h>

#include "hyperframe.h"
#include "hyperframe_base.h"
#include "hyperframe_utils.h"
#include "hyperframe_inter_predict.h"
#include "hyperframe_energy_sched.h"
#include "hyperframe_intra_predict.h"

#define CPU_DRAW_LEVEL 3
#define GPU_RENDER_LEVEL 2

long long g_current_frame;
atomic_t g_run_siginal = ATOMIC_INIT(-1);
static int s_danger_signal = 0;
static int s_recordview_finish = 0;

static struct HYPERFRAME_NOTIFIER_PUSH_TAG **interPushes = NULL;

bool hyperframe_predict_intra_running = false;

static unsigned long long get_cpu_level2delay(unsigned long long time, int level)
{
	unsigned long long delay;
	switch (level) {
	case 0:
		// 3/8
		delay = (time + (time << 1)) >> 3;
		break;
	case 1:
		// 6/8
		delay = ((time << 1) + (time << 2)) >> 3;
		break;
	case 2:
		// 9/8
		delay = (time + (time << 3)) >> 3;
		break;
	default:
		delay = 0;
		break;
	}
	return delay;
}

void hyperframe_inter_doframe_predict(unsigned int pid, long long vsync_id, unsigned int start, unsigned long long time)
{
	int i;
	ktime_t interval;

	if (!hyperframe_notify_wq) {
		pr_debug("[hyperframe_CTRL] NULL WorkQueue\n");
		return;
	}

	if (start) {
		atomic_set(&g_run_siginal, -1);
		for (i = 0; i < CPU_DRAW_LEVEL; i++) {
			interPushes[i]->pid = pid;
			interPushes[i]->ePushType = HYPERFRAME_NOTIFIER_DOFRAME;
			interPushes[i]->vsync_id = vsync_id;
			interPushes[i]->start = start;

			interval = ns_to_ktime(get_cpu_level2delay(time, i));
			hrtimer_start(&interPushes[i]->hr_timer, interval, HRTIMER_MODE_REL);
			s_recordview_finish = 0;
		}
	} else {
		for (i = 0; i < CPU_DRAW_LEVEL; i++) {
			if (interPushes[i]->hr_timer.function != hyperframe_inter_timer_cb)
				return;
			// doframe完成，若定时提频还未执行，规定时间内完成绘制，直接取消;
			else if (hrtimer_active(&interPushes[i]->hr_timer) && s_recordview_finish == 0)
				hrtimer_cancel(&interPushes[i]->hr_timer);
			else if (!hrtimer_active(&interPushes[i]->hr_timer)) { //若提频任务已执行，在绘制结束时将提频取消，恢复提频前策略。
				interPushes[i]->ePushType = HYPERFRAME_NOTIFIER_DOFRAME;
				interPushes[i]->vsync_id = vsync_id;
				interPushes[i]->start = start;

				hrtimer_start(&interPushes[i]->hr_timer, 0, HRTIMER_MODE_REL);
				break;
			}
		}
	}
}

void hyperframe_inter_recordview_predict(unsigned int pid, long long vsync_id,
					 unsigned int start,
					 unsigned long long time)
{
	int i;

	if (!hyperframe_notify_wq) {
		pr_debug("[hyperframe_CTRL] NULL WorkQueue\n");
		return;
	}

	if (!start) {
		for (i = 0; i < CPU_DRAW_LEVEL; i++) {
			if (interPushes[i]->hr_timer.function != hyperframe_inter_timer_cb)
				return;

			// recordview完成，若定时提频还未执行，规定时间内完成绘制，直接取消定时boost
			if (hrtimer_active(&interPushes[i]->hr_timer) && s_recordview_finish == 0) {
				hrtimer_cancel(&interPushes[i]->hr_timer);
			}
		}
		s_recordview_finish = 1;
	}
}

static void hyperframe_boost_start(unsigned int pid, int dangerSignal)
{
	htrace_b_predict(current->tgid,
			"[HYPERFRAME#INTER#%s] pid: %d, danger: %d, vsync: %llu",
			__func__, pid, dangerSignal, get_vsync_time());
	hyperframe_boost_uclamp_start(pid, dangerSignal);

	htrace_e_predict();
}

static void hyperframe_boost_end(unsigned int pid, int dangerSignal)
{
	htrace_b_predict(current->tgid,
			"[HYPERFRAME#INTER#%s] pid: %d, danger: %d, intra_running:%d run_sig:%d",
			__func__, pid, dangerSignal, hyperframe_predict_intra_running, atomic_read(&g_run_siginal));
	if (atomic_read(&g_run_siginal) == s_danger_signal)
		hyperframe_boost_uclamp_end(pid);

	htrace_e_predict();
}

static void hyperframe_inter_doframe_cb(unsigned int pid, long long vsync_id, unsigned int start)
{
	if (start) {
		if (g_current_frame == vsync_id) {
			s_danger_signal ++;
		} else {
			s_danger_signal = 0;
			g_current_frame = vsync_id;
		}

		if (hyperframe_predict_intra_running && s_danger_signal == 0) {
			htrace_b_predict_debug(current->tgid,
				"[HYPERFRAME#INTER#hyperframe_reset_cpufreq_qos] vsyncid: %d", vsync_id);
			hyperframe_reset_cpufreq_qos(pid);
			htrace_e_predict_debug();
		}

		hyperframe_boost_start(pid, s_danger_signal);

	} else {
		hyperframe_boost_end(pid, s_danger_signal);
		if (hyperframe_predict_intra_running && atomic_read(&g_run_siginal) == s_danger_signal) {
			htrace_b_predict(current->tgid, "HYPERFRAME#INTER-->INTRA#%s", __func__);
			hyperframe_intra_predict(pid);
			htrace_e_predict();
		}

	}
}

static void hyperframe_inter_recoedview_cb(void)
{

}

enum hrtimer_restart hyperframe_inter_timer_cb(struct hrtimer *timer)
{
	struct HYPERFRAME_NOTIFIER_PUSH_TAG *interPush =
		container_of(timer, struct HYPERFRAME_NOTIFIER_PUSH_TAG, hr_timer);
	if (!interPush)
		return HRTIMER_NORESTART;

	queue_work(hyperframe_notify_wq, &interPush->sWork);

	return HRTIMER_NORESTART;
}

static void hyperframe_inter_wq_cb(struct work_struct *work)
{
	struct HYPERFRAME_NOTIFIER_PUSH_TAG *interPush =
		container_of(work,struct HYPERFRAME_NOTIFIER_PUSH_TAG, sWork);

	htrace_b_predict(current->tgid, "HYPERFRAME-INTER: %s", __func__);

	if (!interPush) {
		htrace_e_predict();
		return;
	}

	switch (interPush->ePushType) {
		case HYPERFRAME_NOTIFIER_DOFRAME:
			hyperframe_inter_doframe_cb(interPush->pid, interPush->vsync_id, interPush->start);
			break;
		case HYPERFRAME_NOTIFIER_RECORDVIEW:
			hyperframe_inter_recoedview_cb();
			break;
		default:
			break;
	}

	htrace_e_predict();
}

int hyperframe_inter_init(void)
{
	int i,j;
	interPushes = (struct HYPERFRAME_NOTIFIER_PUSH_TAG **)
					hyperframe_alloc_atomic(CPU_DRAW_LEVEL * sizeof(struct HYPERFRAME_NOTIFIER_PUSH_TAG *));
	for (i = 0; i < CPU_DRAW_LEVEL; i++) {
		interPushes[i] = (struct HYPERFRAME_NOTIFIER_PUSH_TAG *)
				hyperframe_alloc_atomic(sizeof(struct HYPERFRAME_NOTIFIER_PUSH_TAG));
		if (!interPushes[i]) {
			if(i == 0)
				return -1;
			if(i > 0 && i < CPU_DRAW_LEVEL) {
				for (j = 0; j < i; j++)
					hyperframe_free(interPushes[j], sizeof(struct HYPERFRAME_NOTIFIER_PUSH_TAG));
				return -1;
			}
		}
		hrtimer_init(&interPushes[i]->hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		interPushes[i]->hr_timer.function = hyperframe_inter_timer_cb;

		INIT_WORK(&interPushes[i]->sWork, hyperframe_inter_wq_cb);
	}

	return 0;
}

int hyperframe_inter_exit(void)
{
	int i;

	for (i = 0; i < CPU_DRAW_LEVEL; i++) {
		if (interPushes[i] == NULL)
			continue;
		
		if (interPushes[i]->hr_timer.function == hyperframe_inter_timer_cb)
			hrtimer_cancel(&interPushes[i]->hr_timer);
		else
			pr_err("hyperframe_inter_exit hr_timer NULL");

		hyperframe_free(interPushes[i], sizeof(struct HYPERFRAME_NOTIFIER_PUSH_TAG));
		interPushes[i] = NULL;
	}

	hyperframe_free(interPushes, CPU_DRAW_LEVEL * sizeof(struct HYPERFRAME_NOTIFIER_PUSH_TAG *));

	return 0;
}
// END Performance_FramePredictBoost