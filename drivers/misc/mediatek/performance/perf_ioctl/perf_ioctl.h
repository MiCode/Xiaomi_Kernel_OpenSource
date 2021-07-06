/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#ifndef PERF_IOCTL_H
#define PERF_IOCTL_H
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

struct _FPSGO_PACKAGE {
	__u32 tid;
	union {
		__u32 start;
		__u32 connectedAPI;
	};
	union {
		__u64 frame_time;
		__u64 bufID;
	};
	__u64 frame_id; /* for HWUI only*/
	__s32 queue_SF;
	__u64 identifier;
};

#define MAX_DEVICE 2
struct _EARA_NN_PACKAGE {
	__u32 pid;
	__u32 tid;
	__u64 mid;
	__s32 errorno;
	__s32 priority;
	__s32 num_step;

	__s32 dev_usage;
	__u32 bw_usage;
	__s32 thrm_throttled;

	union {
		__s32 *device;
		__u64 p_dummy_device;
	};
	union {
		__s32 *boost;
		__u64 p_dummy_boost;
	};
	union {
		__u64 *exec_time;
		__u64 p_dummy_exec_time;
	};
	union {
		__u64 *target_time;
		__u64 p_dummy_target_time;
	};
};

enum  {
	USAGE_DEVTYPE_CPU  = 0,
	USAGE_DEVTYPE_GPU  = 1,
	USAGE_DEVTYPE_APU  = 2,
	USAGE_DEVTYPE_MDLA = 3,
	USAGE_DEVTYPE_VPU  = 4,
	USAGE_DEVTYPE_MAX  = 5,
};

#define FPSGO_QUEUE                  _IOW('g', 1,  struct _FPSGO_PACKAGE)
#define FPSGO_DEQUEUE                _IOW('g', 3,  struct _FPSGO_PACKAGE)
#define FPSGO_VSYNC                  _IOW('g', 5,  struct _FPSGO_PACKAGE)
#define FPSGO_TOUCH                  _IOW('g', 10, struct _FPSGO_PACKAGE)
#define FPSGO_QUEUE_CONNECT          _IOW('g', 15, struct _FPSGO_PACKAGE)
#define FPSGO_BQID                   _IOW('g', 16, struct _FPSGO_PACKAGE)

#define EARA_NN_BEGIN               _IOW('g', 1, struct _EARA_NN_PACKAGE)
#define EARA_NN_END                 _IOW('g', 2, struct _EARA_NN_PACKAGE)
#define EARA_GETUSAGE               _IOW('g', 3, struct _EARA_NN_PACKAGE)
#define EARA_GETSTATE               _IOW('g', 4, struct _EARA_NN_PACKAGE)

#endif

