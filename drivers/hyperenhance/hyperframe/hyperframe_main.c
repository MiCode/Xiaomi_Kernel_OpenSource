// MIUI ADD: Performance_FramePredictBoost
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
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
#include <linux/cpufreq.h>
#include <linux/cpumask.h>
#include <trace/events/power.h>
#include <linux/tracepoint.h>
#include <linux/kallsyms.h>
#include <uapi/linux/sched/types.h>

#include "hyperframe.h"
#include "hyperframe_base.h"
#include "hyperframe_cpu_loading_track.h"
#include "hyperframe_cpu_tracer.h"
#include "hyperframe_utils.h"
#include "hyperframe_frame_info.h"
#include "hyperframe_inter_predict.h"
#include "hyperframe_intra_predict.h"

extern void (*hyperframe_notify_doframe_fp)(unsigned int pid, long long vsync_id, unsigned int start, unsigned long long time);
extern void (*hyperframe_notify_recordview_fp)(unsigned int pid, long long vsync_id, unsigned int start, unsigned long long time);
extern void (*hyperframe_notify_drawframes_fp)(unsigned int pid, long long vsync_id, unsigned int start, unsigned long long time);
extern void (*hyperframe_notify_dequeue_fp)(unsigned int pid, long long vsync_id, unsigned int start, unsigned long long time);
extern void (*hyperframe_notify_queue_fp)(unsigned int pid, long long vsync_id, unsigned int start, unsigned long long time);
extern void (*hyperframe_notify_gpu_fp)(unsigned int pid, long long vsync_id, unsigned int start, unsigned long long time);
extern void (*hyperframe_notify_vsync_fp)(unsigned long long time);
extern void (*hyperframe_notify_id_fp)(unsigned int pid, unsigned int tid, unsigned int is_focus);

#define FRAME_COUNT_WINDOW 10

struct workqueue_struct *hyperframe_notify_wq;
static struct HYPERFRAME_NOTIFIER_PUSH_TAG **mPushes;
static int hyperframe_enable;
static struct mutex notify_lock;
static unsigned long long vsync_time;
static unsigned int ui_thread_id;
static int frame_count;

extern int hyperframe_predict_intra_running;

int hyperframe_is_enable(void)
{
	int enable;

	mutex_lock(&notify_lock);
	enable = hyperframe_enable;
	mutex_unlock(&notify_lock);

	return enable;
}

static void hyperframe_notify_doframe_cb(unsigned int pid, long long vsync_id, unsigned int start, unsigned long long time)
{
	if(!hyperframe_is_enable())
		return;

	hyperframe_update_time(pid, vsync_id, time, TIME_DOFRAME, start);

	switch (start) {
	case 1:
		if (ui_thread_id != pid)
			ui_thread_id = pid;
		break;
	case 0:
		break;
	default:
		break;
	}
}

static void hyperframe_notify_recordview_cb(unsigned int pid, long long vsync_id, unsigned int start, unsigned long long time)
{
	if(!hyperframe_is_enable())
		return;

	hyperframe_update_time(pid, vsync_id, time, TIME_RECOREVIEW, start);
}

static void hyperframe_notify_drawframes_cb(unsigned int pid, long long vsync_id, unsigned int start, unsigned long long time)
{
	if(!hyperframe_is_enable())
		return;

	hyperframe_update_time(pid, vsync_id, time, TIME_DRAWFRAME, start);

	switch (start) {
	case 1:
		break;
	case 0:
		notify_frame_loading_update(pid, vsync_id);

		if (!hyperframe_predict_intra_running)
			break;

		if (frame_count >= FRAME_COUNT_WINDOW) {
			frame_count = 0;
			htrace_b_predict(current->tgid, "HYPERFRAME#INTRA#%s", __func__);
			hyperframe_intra_predict(ui_thread_id);
			htrace_e_predict();
		}
		frame_count++;
		break;
	default:
		break;
	}
}

// static void  hyperframe_notify_dequeue_cb(unsigned int pid, long long vsync_id, unsigned int start, unsigned long long time)
// {
// if(!hyperframe_is_enable())
// return;
// 	pr_err("frame hook log test %s, pid: (%d)\n", __func__, pid);

// }

// static void  hyperframe_notify_queue_cb(unsigned int pid, long long vsync_id, unsigned int start, unsigned long long time)
// {
// if(!hyperframe_is_enable())
// return;
// pr_err("frame hook log test %s, pid: (%d)\n", __func__, pid);
// }

// static void  hyperframe_notify_gpu_cb(unsigned int pid, long long vsync_id, unsigned int start, unsigned long long time)
// {
// if(!hyperframe_is_enable())
// return;
// pr_err("frame hook log test %s, pid: (%d)\n", __func__, pid);
// }

static void  hyperframe_notify_vsync_cb(unsigned long long time)
{
	if(!hyperframe_is_enable())
		return;
}

static void hyperframe_notifier_wq_cb_enable(int enable)
{
	mutex_lock(&notify_lock);
	if (enable == hyperframe_enable) {
		mutex_unlock(&notify_lock);
		return;
	}
	hyperframe_enable = enable;
	mutex_unlock(&notify_lock);
}

static void hyperframe_notifer_wq_cb(struct work_struct *psWork)
{
	size_t recent_pos_type = -1;

	struct HYPERFRAME_NOTIFIER_PUSH_TAG *vpPush =
			container_of(psWork, struct HYPERFRAME_NOTIFIER_PUSH_TAG, sWork);

	htrace_b_fi(current->tgid, "HYPERFRAME: %s", __func__);
	if (!vpPush) {
		htrace_e_fi();
		return;
	}

	switch (vpPush->ePushType) {
	case HYPERFRAME_NOTIFIER_DOFRAME:
		hyperframe_notify_doframe_cb(vpPush->pid, vpPush->vsync_id, vpPush->start, vpPush->doframe_time);
		break;
	case HYPERFRAME_NOTIFIER_RECORDVIEW:
		hyperframe_notify_recordview_cb(vpPush->pid, vpPush->vsync_id, vpPush->start, vpPush->recordview_time);
		break;
	case HYPERFRAME_NOTIFIER_DRAWFRAMES:
		hyperframe_notify_drawframes_cb(vpPush->pid, vpPush->vsync_id, vpPush->start, vpPush->drawframes_time);
		break;
	// case HYPERFRAME_NOTIFIER_DEQUEUE:
	// hyperframe_notify_dequeue_cb(vpPush->pid, vpPush->vsync_id, vpPush->start, vpPush->dequeue_time);
	// break;
	// case HYPERFRAME_NOTIFIER_QUEUE:
	// hyperframe_notify_queue_cb(vpPush->pid, vpPush->vsync_id, vpPush->start, vpPush->queue_time);
	// break;
	// case HYPERFRAME_NOTIFIER_GPU:
	// hyperframe_notify_gpu_cb(vpPush->pid, vpPush->vsync_id, vpPush->start, vpPush->gpu_time);
	// break;
	case HYPERFRAME_NOTIFIER_VSYNC:
		hyperframe_notify_vsync_cb(vpPush->vsync_peroid);
		break;
	case HYPERFRAME_NOTIFIER_ID:
		htrace_b_cpu_debug(current->tgid,
				"[HYPERFRAME#CPU#set_ui_and_renderthread] pid:%d focus:%d",
				vpPush->pid, vpPush->is_focus);
		set_ui_and_renderthread(vpPush->pid, vpPush->tid, vpPush->is_focus);
		recent_pos_type = update_recent_queue(vpPush->pid, vpPush->tid);
		htrace_e_cpu_debug();
		break;
	default:
		break;
	}
	htrace_e_fi();
}

void hyperframe_notify_doframe(unsigned int pid, long long vsync_id, unsigned int start, unsigned long long time)
{
	if(!hyperframe_is_enable())
		return;

	if (!mPushes[HYPERFRAME_NOTIFIER_DOFRAME])
		return;

	if (!hyperframe_notify_wq)
		return;

	mPushes[HYPERFRAME_NOTIFIER_DOFRAME]->ePushType = HYPERFRAME_NOTIFIER_DOFRAME;
	mPushes[HYPERFRAME_NOTIFIER_DOFRAME]->pid = pid;
	mPushes[HYPERFRAME_NOTIFIER_DOFRAME]->vsync_id = vsync_id;
	mPushes[HYPERFRAME_NOTIFIER_DOFRAME]->start = start;
	mPushes[HYPERFRAME_NOTIFIER_DOFRAME]->doframe_time = time;

	if (!work_pending(&mPushes[HYPERFRAME_NOTIFIER_DOFRAME]->sWork))
		queue_work(hyperframe_notify_wq, &mPushes[HYPERFRAME_NOTIFIER_DOFRAME]->sWork);

	hyperframe_inter_doframe_predict(pid, vsync_id, start, vsync_time);
}

void hyperframe_notify_recordview(unsigned int pid, long long vsync_id, unsigned int start, t_time time)
{
	if(!hyperframe_is_enable())
		return;

	if (!mPushes[HYPERFRAME_NOTIFIER_RECORDVIEW])
		return;

	if (!hyperframe_notify_wq) {
		return;
	}

	mPushes[HYPERFRAME_NOTIFIER_RECORDVIEW]->ePushType = HYPERFRAME_NOTIFIER_RECORDVIEW;
	mPushes[HYPERFRAME_NOTIFIER_RECORDVIEW]->start = start;
	mPushes[HYPERFRAME_NOTIFIER_RECORDVIEW]->recordview_time = time;

	if (!work_pending(&mPushes[HYPERFRAME_NOTIFIER_RECORDVIEW]->sWork))
		queue_work(hyperframe_notify_wq, &mPushes[HYPERFRAME_NOTIFIER_RECORDVIEW]->sWork);

	hyperframe_inter_recordview_predict(pid, vsync_id, start, vsync_time);
}

void hyperframe_notify_drawframes(unsigned int pid, long long vsync_id, unsigned int start, unsigned long long time)
{
	if(!hyperframe_is_enable())
		return;

	if (!mPushes[HYPERFRAME_NOTIFIER_DRAWFRAMES])
		return;

	if (!hyperframe_notify_wq)
		return;

	mPushes[HYPERFRAME_NOTIFIER_DRAWFRAMES]->ePushType = HYPERFRAME_NOTIFIER_DRAWFRAMES;
	mPushes[HYPERFRAME_NOTIFIER_DRAWFRAMES]->pid = pid;
	mPushes[HYPERFRAME_NOTIFIER_DRAWFRAMES]->vsync_id = vsync_id;
	mPushes[HYPERFRAME_NOTIFIER_DRAWFRAMES]->start = start;
	mPushes[HYPERFRAME_NOTIFIER_DRAWFRAMES]->drawframes_time = time;

	if (!work_pending(&mPushes[HYPERFRAME_NOTIFIER_DRAWFRAMES]->sWork))
		queue_work(hyperframe_notify_wq, &mPushes[HYPERFRAME_NOTIFIER_DRAWFRAMES]->sWork);
}

void hyperframe_notify_dequeue(unsigned int pid, long long vsync_id, unsigned int start, unsigned long long time)
{
	if(!hyperframe_is_enable())
		return;

	if (!mPushes[HYPERFRAME_NOTIFIER_DEQUEUE])
		return;

	if (!hyperframe_notify_wq) {
		return;
	}

	mPushes[HYPERFRAME_NOTIFIER_DEQUEUE]->ePushType = HYPERFRAME_NOTIFIER_DEQUEUE;
	mPushes[HYPERFRAME_NOTIFIER_DEQUEUE]->start = start;
	mPushes[HYPERFRAME_NOTIFIER_DEQUEUE]->dequeue_time = time;

	if (!work_pending(&mPushes[HYPERFRAME_NOTIFIER_DEQUEUE]->sWork))
		queue_work(hyperframe_notify_wq, &mPushes[HYPERFRAME_NOTIFIER_DEQUEUE]->sWork);
}

void hyperframe_notify_queue(unsigned int pid, long long vsync_id, unsigned int start, unsigned long long time)
{
	if(!hyperframe_is_enable())
		return;

	if (!mPushes[HYPERFRAME_NOTIFIER_QUEUE])
		return;

	if (!hyperframe_notify_wq)
		return;

	mPushes[HYPERFRAME_NOTIFIER_QUEUE]->ePushType = HYPERFRAME_NOTIFIER_QUEUE;
	mPushes[HYPERFRAME_NOTIFIER_QUEUE]->start = start;
	mPushes[HYPERFRAME_NOTIFIER_QUEUE]->queue_time = time;

	if (!work_pending(&mPushes[HYPERFRAME_NOTIFIER_QUEUE]->sWork))
		queue_work(hyperframe_notify_wq, &mPushes[HYPERFRAME_NOTIFIER_QUEUE]->sWork);
}

void hyperframe_notify_gpu(unsigned int pid, long long vsync_id, unsigned int start, unsigned long long time)
{
	if(!hyperframe_is_enable())
		return;

	if (!mPushes[HYPERFRAME_NOTIFIER_GPU])
		return;

	if (!hyperframe_notify_wq)
		return;

	mPushes[HYPERFRAME_NOTIFIER_GPU]->ePushType = HYPERFRAME_NOTIFIER_GPU;
	mPushes[HYPERFRAME_NOTIFIER_GPU]->pid = pid;
	mPushes[HYPERFRAME_NOTIFIER_GPU]->vsync_id = vsync_id;
	mPushes[HYPERFRAME_NOTIFIER_GPU]->start = start;
	mPushes[HYPERFRAME_NOTIFIER_GPU]->gpu_time = time;

	if (!work_pending(&mPushes[HYPERFRAME_NOTIFIER_GPU]->sWork))
		queue_work(hyperframe_notify_wq, &mPushes[HYPERFRAME_NOTIFIER_GPU]->sWork);
}

void hyperframe_notify_vsync(unsigned long long time)
{
	if(!hyperframe_is_enable())
		return;

	if (!mPushes[HYPERFRAME_NOTIFIER_VSYNC])
		return;

	if (!hyperframe_notify_wq)
		return;

	mPushes[HYPERFRAME_NOTIFIER_VSYNC]->ePushType = HYPERFRAME_NOTIFIER_VSYNC;
	mPushes[HYPERFRAME_NOTIFIER_VSYNC]->vsync_peroid = time;
	vsync_time = time;

	if (!work_pending(&mPushes[HYPERFRAME_NOTIFIER_VSYNC]->sWork))
		queue_work(hyperframe_notify_wq, &mPushes[HYPERFRAME_NOTIFIER_VSYNC]->sWork);
}

unsigned long long get_vsync_time(void)
{
	return vsync_time;
}

void hyperframe_notify_id(unsigned int pid, unsigned int tid, unsigned int is_focus)
{
	if(!hyperframe_is_enable())
		return;

	if (!mPushes[HYPERFRAME_NOTIFIER_ID])
		return;

	if (!hyperframe_notify_wq)
		return;

	mPushes[HYPERFRAME_NOTIFIER_ID]->ePushType = HYPERFRAME_NOTIFIER_ID;
	mPushes[HYPERFRAME_NOTIFIER_ID]->pid = pid;
	mPushes[HYPERFRAME_NOTIFIER_ID]->tid = tid;
	mPushes[HYPERFRAME_NOTIFIER_ID]->is_focus = is_focus;

	if (!work_pending(&mPushes[HYPERFRAME_NOTIFIER_ID]->sWork))
		queue_work(hyperframe_notify_wq, &mPushes[HYPERFRAME_NOTIFIER_ID]->sWork);
}

static int hyperframe_alloc_frame_package(void)
{
	int i,j;
	mPushes = (struct HYPERFRAME_NOTIFIER_PUSH_TAG **)
			hyperframe_alloc_atomic(HYPERFRAME_NOTIFIER_COUNT * sizeof(struct HYPERFRAME_NOTIFIER_PUSH_TAG *));

	for (i = 0; i < HYPERFRAME_NOTIFIER_COUNT; i++) {
		mPushes[i] =kcalloc(1, sizeof(struct HYPERFRAME_NOTIFIER_PUSH_TAG), GFP_KERNEL);

		if (!mPushes[i]) {
			if(i == 0) return -1;
			if(i > 0 && i < HYPERFRAME_NOTIFIER_COUNT) {
				for (j = 0; j < i; j++) {
					hyperframe_free(mPushes[j], sizeof(struct HYPERFRAME_NOTIFIER_PUSH_TAG));
				}
				return -1;
			}
		}
		INIT_WORK(&mPushes[i]->sWork, hyperframe_notifer_wq_cb);
	}
	return 0;
}

static int hyperframe_free_frame_package(void)
{
	int i;
	for (i = 0; i < HYPERFRAME_NOTIFIER_COUNT; i++) {
		if (mPushes[i] != NULL) {
			hyperframe_free(mPushes[i], sizeof(struct HYPERFRAME_NOTIFIER_PUSH_TAG));
			mPushes[i] = NULL;
		}
	}
	hyperframe_free(mPushes, HYPERFRAME_NOTIFIER_COUNT * sizeof(struct HYPERFRAME_NOTIFIER_PUSH_TAG *));
	return 0;
}

static int __init hyperframe_init(void)
{
	pr_err("hyperframe_init start");
	framectl_init();
	hyperframe_notify_wq =
			alloc_ordered_workqueue("%s", WQ_MEM_RECLAIM | WQ_HIGHPRI, "hyperframe_notifer_wq");
	if(hyperframe_notify_wq == NULL)
		return -EFAULT;
	if(hyperframe_alloc_frame_package() != 0)
		return -EFAULT;
	mutex_init(&notify_lock);
		
	hyperframe_init_energy();
	hyperframe_filesystem_init();
	hyperframe_frameinfo_init();
	hyperframe_cpu_loading_track_init();
	init_hyperframe_debug();
	hyperframe_inter_init();
	hyperframe_intra_init();

	hyperframe_notifier_wq_cb_enable(1);

	hyperframe_notify_doframe_fp = hyperframe_notify_doframe;
	hyperframe_notify_recordview_fp = hyperframe_notify_recordview;
	hyperframe_notify_drawframes_fp = hyperframe_notify_drawframes;
	hyperframe_notify_dequeue_fp = hyperframe_notify_dequeue;
	hyperframe_notify_queue_fp = hyperframe_notify_queue;
	hyperframe_notify_gpu_fp = hyperframe_notify_gpu;
	hyperframe_notify_vsync_fp = hyperframe_notify_vsync;
	hyperframe_notify_id_fp = hyperframe_notify_id;
	pr_err("hyperframe_init end");
	return 0;
}

static void __exit hyperframe_exit(void)
{
	pr_err("hyperframe_exit start");
	hyperframe_notifier_wq_cb_enable(0);
	hyperframe_cpu_loading_track_exit();

	hyperframe_inter_exit();
	hyperframe_intra_exit();

	framectl_exit();
	if (hyperframe_notify_wq) {
		flush_workqueue(hyperframe_notify_wq);
		destroy_workqueue(hyperframe_notify_wq);
		hyperframe_notify_wq = NULL;
	}
	hyperframe_free_frame_package();
	hyperframe_filesystem_exit();
	pr_err("hyperframe_exit end");
}

module_init(hyperframe_init);
module_exit(hyperframe_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mi");
MODULE_DESCRIPTION("HyperFrame module");
// END Performance_FramePredictBoost