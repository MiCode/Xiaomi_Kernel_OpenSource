#ifndef __LINUX_IDLE_STATS_DEVICE_H
#define __LINUX_IDLE_STATS_DEVICE_H

#include <uapi/linux/idle_stats_device.h>
#include <linux/hrtimer.h>
#include <linux/mutex.h>
#include <linux/miscdevice.h>

struct msm_idle_stats_device {
	const char *name;
	void (*get_sample)(struct msm_idle_stats_device *device,
		struct msm_idle_pulse *pulse);

	struct miscdevice miscdev;
	spinlock_t lock;
	wait_queue_head_t wait;
	struct list_head list;
	struct hrtimer busy_timer;
	ktime_t busy_timer_interval;
	ktime_t idle_start;
	ktime_t remaining_time;
	__u32 max_samples;

	struct msm_idle_read_stats *stats;
	struct msm_idle_read_stats stats_vector[2];
};

int msm_idle_stats_register_device(struct msm_idle_stats_device *device);
int msm_idle_stats_deregister_device(struct msm_idle_stats_device *device);
void msm_idle_stats_prepare_idle_start(struct msm_idle_stats_device *device);
void msm_idle_stats_abort_idle_start(struct msm_idle_stats_device *device);
void msm_idle_stats_idle_start(struct msm_idle_stats_device *device);
void msm_idle_stats_idle_end(struct msm_idle_stats_device *device,
				struct msm_idle_pulse *pulse);
void msm_idle_stats_update_event(struct msm_idle_stats_device *device,
				__u32 event);

#endif  /* __LINUX_IDLE_STATS_DEVICE_H */

