// MIUI ADD: Performance_FramePredictBoost
#ifndef HYPERFRAME_H
#define HYPERFRAME_H

#include <linux/jiffies.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/utsname.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/uaccess.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/notifier.h>
#include <linux/suspend.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/hrtimer.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/ioctl.h>

#define HYPERFRAME_DIR "hyperframe"
#define HYPERFRAME_IOCTL "frame_ioctl"

struct _FRAME_PACKAGE {
	__u32 pid;
	__u32 tid;
	__s64 vsync_id;
	__u32 start;
	__u64 time_stamp;
	__u32 is_focus;
};

enum FRAME_PUSH_TYPE {
	HYPERFRAME_NOTIFIER_DOFRAME = 0,
	HYPERFRAME_NOTIFIER_RECORDVIEW,
	HYPERFRAME_NOTIFIER_DRAWFRAMES,
	HYPERFRAME_NOTIFIER_DEQUEUE,
	HYPERFRAME_NOTIFIER_QUEUE,
	HYPERFRAME_NOTIFIER_GPU,
	HYPERFRAME_NOTIFIER_VSYNC,
	HYPERFRAME_NOTIFIER_ID,

	HYPERFRAME_NOTIFIER_COUNT,
};

struct HYPERFRAME_NOTIFIER_PUSH_TAG {
	enum FRAME_PUSH_TYPE ePushType;
	__u32 pid;
	__u32 tid;
	__u32 start;
	__u64 vsync_peroid;
	__s64 vsync_id;
	__u32 is_focus;

	__u64 doframe_time;
	__u64 recordview_time;
	__u64 drawframes_time;
	__u64 dequeue_time;
	__u64 queue_time;
	__u64 gpu_time;

	struct work_struct sWork;
	struct hrtimer hr_timer;
};

#define HYPER_FRAME_VSYNC            _IOW('g', 1,  struct _FRAME_PACKAGE)
#define HYPER_FRAME_DOFRAME          _IOW('g', 30, struct _FRAME_PACKAGE)
#define HYPER_FRAME_RECORDVIEW       _IOW('g', 31, struct _FRAME_PACKAGE)
#define HYPER_FRAME_DRAWFRAMES       _IOW('g', 32, struct _FRAME_PACKAGE)
#define HYPER_FRAME_DEQUEUE          _IOW('g', 33, struct _FRAME_PACKAGE)
#define HYPER_FRAME_QUEUE            _IOW('g', 34, struct _FRAME_PACKAGE)
#define HYPER_FRAME_GPU              _IOW('g', 35, struct _FRAME_PACKAGE)
#define HYPER_FRAME_ID               _IOW('g', 36, struct _FRAME_PACKAGE)

unsigned long long get_vsync_time(void);
extern int framectl_init(void);
extern int framectl_exit(void);

#endif
// END Performance_FramePredictBoost