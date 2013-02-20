#ifndef __UAPI_IDLE_STATS_DEVICE_H
#define __UAPI_IDLE_STATS_DEVICE_H

#include <linux/types.h>
#include <linux/ioctl.h>

#define MSM_IDLE_STATS_EVENT_NONE                     0
#define MSM_IDLE_STATS_EVENT_BUSY_TIMER_EXPIRED       1
#define MSM_IDLE_STATS_EVENT_BUSY_TIMER_EXPIRED_RESET 2
#define MSM_IDLE_STATS_EVENT_COLLECTION_NEARLY_FULL   4
#define MSM_IDLE_STATS_EVENT_COLLECTION_FULL          8
#define MSM_IDLE_STATS_EVENT_IDLE_TIMER_EXPIRED      16

/*
 * All time, timer, and time interval values are in units of
 * microseconds unless stated otherwise.
 */
#define MSM_IDLE_STATS_NR_MAX_INTERVALS 200

struct msm_idle_pulse {
	__s64 busy_start_time;
	__u32 busy_interval;
	__u32 wait_interval;
};

struct msm_idle_read_stats {
	__u32 event;
	__s64 return_timestamp;
	__u32 busy_timer_remaining;
	__u32 nr_collected;
	struct msm_idle_pulse pulse_chain[MSM_IDLE_STATS_NR_MAX_INTERVALS];
};

struct msm_idle_write_stats {
	__u32 busy_timer;
	__u32 next_busy_timer;
	__u32 max_samples;
};

#define MSM_IDLE_STATS_IOC_MAGIC  0xD8
#define MSM_IDLE_STATS_IOC_READ_STATS  \
		_IOWR(MSM_IDLE_STATS_IOC_MAGIC, 1, struct msm_idle_read_stats)
#define MSM_IDLE_STATS_IOC_WRITE_STATS  \
		_IOWR(MSM_IDLE_STATS_IOC_MAGIC, 2, struct msm_idle_write_stats)

#endif  /* __UAPI_IDLE_STATS_DEVICE_H */

