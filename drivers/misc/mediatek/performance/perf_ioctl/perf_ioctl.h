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

#if IS_ENABLED(CONFIG_MTK_CORE_CTL)
extern int core_ctl_set_offline_throttle_ms(unsigned int cid,
                                unsigned int throttle_ms);
extern int core_ctl_set_limit_cpus(unsigned int cid, unsigned int min,
                                unsigned int max);
extern int core_ctl_set_not_preferred(int cid, int cpu, bool enable);
extern int core_ctl_set_boost(bool boost);
extern int core_ctl_set_up_thres(int cid, unsigned int val);
extern int core_ctl_force_pause_cpu(int cpu, bool paused);
extern int core_ctl_enable_policy(bool paused);
#endif

struct _FPSGO_PACKAGE {
	union {
		__u32 tid;
		__s32 fps;
		__s32 cmd;
	};
	union {
		__u32 start;
		__u32 connectedAPI;
		__u32 value1;
	};
	union {
		__u64 frame_time;
		__u64 bufID;
	};
	__u64 frame_id;
	union {
		__s32 queue_SF;
		__s32 value2;
	};
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
#define FPSGO_SWAP_BUFFER            _IOW('g', 14, struct _FPSGO_PACKAGE)
#define FPSGO_QUEUE_CONNECT          _IOW('g', 15, struct _FPSGO_PACKAGE)
#define FPSGO_BQID                   _IOW('g', 16, struct _FPSGO_PACKAGE)
#define FPSGO_GET_FPS                _IOW('g', 17, struct _FPSGO_PACKAGE)
#define FPSGO_GET_CMD                _IOW('g', 18, struct _FPSGO_PACKAGE)
#define FPSGO_GBE_GET_CMD            _IOW('g', 19, struct _FPSGO_PACKAGE)

#define EARA_NN_BEGIN               _IOW('g', 1, struct _EARA_NN_PACKAGE)
#define EARA_NN_END                 _IOW('g', 2, struct _EARA_NN_PACKAGE)
#define EARA_GETUSAGE               _IOW('g', 3, struct _EARA_NN_PACKAGE)
#define EARA_GETSTATE               _IOW('g', 4, struct _EARA_NN_PACKAGE)

#define EAS_SYNC_SET                            _IOW('g', 1,  unsigned int)
#define EAS_SYNC_GET                            _IOW('g', 2,  unsigned int)
#define EAS_PERTASK_LS_SET                      _IOW('g', 3,  unsigned int)
#define EAS_PERTASK_LS_GET                      _IOR('g', 4,  unsigned int)
#define EAS_ACTIVE_MASK_GET                     _IOR('g', 5,  unsigned int)
#define CORE_CTL_FORCE_RESUME_CPU               _IOW('g', 6, char *)
#define CORE_CTL_FORCE_PAUSE_CPU                _IOW('g', 7, char *)
#define CORE_CTL_SET_OFFLINE_THROTTLE_MS        _IOW('g', 8,  char *)
#define CORE_CTL_SET_LIMIT_CPUS                 _IOW('g', 9,  char *)
#define CORE_CTL_SET_NOT_PREFERRED              _IOW('g', 10, char *)
#define CORE_CTL_SET_BOOST                      _IOW('g', 11, char *)
#define CORE_CTL_SET_UP_THRES                   _IOW('g', 12, char *)
#define CORE_CTL_ENABLE_POLICY                  _IOW('g', 13, char *)

#endif

