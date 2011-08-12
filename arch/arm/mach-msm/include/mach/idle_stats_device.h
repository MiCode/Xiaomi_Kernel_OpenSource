/* Copyright (c) 2010, 2011 Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of Code Aurora Forum, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef __ARCH_ARM_MACH_MSM_IDLE_STATS_DEVICE_H
#define __ARCH_ARM_MACH_MSM_IDLE_STATS_DEVICE_H

#include <linux/types.h>
#include <linux/ioctl.h>

#define MSM_IDLE_STATS_EVENT_NONE                     0
#define MSM_IDLE_STATS_EVENT_BUSY_TIMER_EXPIRED       1
#define MSM_IDLE_STATS_EVENT_BUSY_TIMER_EXPIRED_RESET 2
#define MSM_IDLE_STATS_EVENT_COLLECTION_NEARLY_FULL   4
#define MSM_IDLE_STATS_EVENT_COLLECTION_FULL          8

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
};

#define MSM_IDLE_STATS_IOC_MAGIC  0xD8
#define MSM_IDLE_STATS_IOC_READ_STATS  \
		_IOWR(MSM_IDLE_STATS_IOC_MAGIC, 1, struct msm_idle_read_stats)
#define MSM_IDLE_STATS_IOC_WRITE_STATS  \
		_IOWR(MSM_IDLE_STATS_IOC_MAGIC, 2, struct msm_idle_write_stats)

#ifdef __KERNEL__
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
#endif

#endif  /* __ARCH_ARM_MACH_MSM_IDLE_STATS_DEVICE_H */

