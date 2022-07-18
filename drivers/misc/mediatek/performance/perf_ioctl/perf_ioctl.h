/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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
	union {
		__u32 tid;
		__s32 fps;
		__s32 cmd;
		__s32 active;
	};
	union {
		__u32 start;
		__u32 connectedAPI;
		__u32 value1;
	};
	union {
		__u64 frame_time;
		__u64 bufID;
		__s64 time_diff;
	};
	__u64 frame_id;
	union {
		__s32 queue_SF;
		__s32 value2;
	};
	__u64 identifier;
};

struct _XGFFRAME_PACKAGE {
	__u32 tid;
	__u64 queueid;
	__u64 frameid;

	__u64 cputime;
	__u32 area;
	__u32 deplist_size;

	union {
		__u32 *deplist;
		__u64 p_dummy_deplist;
		__u32 min_cap;
	};
};

struct _XGFFRAME_BOOST_PARAM {
	__u32 rescue_pct_1;
	__u32 rescue_f_1;
	__u32 rescue_pct_2;
	__u32 rescue_f_2;
	__u32 gcc_std_filter;
	__u32 gcc_history_window;
	__u32 gcc_up_check;
	__u32 gcc_up_thrs;
	__u32 gcc_up_step;
	__u32 gcc_down_check;
	__u32 gcc_down_thrs;
	__u32 gcc_down_step;
};

struct _XGFFRAME_BOOST_PACKAGE {
	__u32 start;
	__u32 group_id;
	__u32 *dep_list;
	__u32 dep_list_num;
	__u32 prefer_cluster;
	__u64 target_time;
	struct _XGFFRAME_BOOST_PARAM param;
};


#define FPSGO_QUEUE                  _IOW('g', 1,  struct _FPSGO_PACKAGE)
#define FPSGO_DEQUEUE                _IOW('g', 3,  struct _FPSGO_PACKAGE)
#define FPSGO_VSYNC                  _IOW('g', 5,  struct _FPSGO_PACKAGE)
#define FPSGO_TOUCH                  _IOW('g', 10, struct _FPSGO_PACKAGE)
#define FPSGO_SWAP_BUFFER            _IOW('g', 14, struct _FPSGO_PACKAGE)
#define FPSGO_QUEUE_CONNECT          _IOW('g', 15, struct _FPSGO_PACKAGE)
#define FPSGO_BQID                   _IOW('g', 16, struct _FPSGO_PACKAGE)
#define FPSGO_GET_FPS                _IOW('g', 17, struct _FPSGO_PACKAGE)
#define FPSGO_GET_CMD                _IOW('g', 18, struct _FPSGO_PACKAGE)
#define FPSGO_GBE_GET_CMD            _IOW('g', 19, struct _FPSGO_PACKAGE)
#define FPSGO_GET_FSTB_ACTIVE        _IOW('g', 20, struct _FPSGO_PACKAGE)
#define FPSGO_WAIT_FSTB_ACTIVE       _IOW('g', 21, struct _FPSGO_PACKAGE)
#define FPSGO_SBE_RESCUE             _IOW('g', 22, struct _FPSGO_PACKAGE)

#define XGFFRAME_START              _IOW('g', 1, struct _XGFFRAME_PACKAGE)
#define XGFFRAME_END                _IOW('g', 2, struct _XGFFRAME_PACKAGE)
#define XGFFRAME_MIN_CAP            _IOW('g', 3, struct _XGFFRAME_PACKAGE)

#define XGFFRAME_BOOST_START              _IOW('g', 1, struct _XGFFRAME_BOOST_PACKAGE)
#define XGFFRAME_BOOST_END                _IOW('g', 2, struct _XGFFRAME_BOOST_PACKAGE)


#endif

