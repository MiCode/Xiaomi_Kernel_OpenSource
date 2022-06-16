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

#if IS_ENABLED(CONFIG_MTK_SCHEDULER)
extern void set_wake_sync(unsigned int sync);
extern unsigned int get_wake_sync(void);
extern void set_uclamp_min_ls(unsigned int val);
extern unsigned int get_uclamp_min_ls(void);
extern unsigned int set_newly_idle_balance_interval_us(unsigned int interval_us);
extern unsigned int get_newly_idle_balance_interval_us(void);
extern void set_get_thermal_headroom_interval_tick(unsigned int tick);
extern unsigned int get_thermal_headroom_interval_tick(void);
#endif

#if IS_ENABLED(CONFIG_MTK_CORE_CTL)
extern int core_ctl_set_offline_throttle_ms(unsigned int cid,
                                unsigned int throttle_ms);
extern int core_ctl_set_limit_cpus(unsigned int cid, unsigned int min,
                                unsigned int max);
extern int core_ctl_set_not_preferred(unsigned int not_preferred_cpus);
extern int core_ctl_set_boost(bool boost);
extern int core_ctl_set_up_thres(int cid, unsigned int val);
extern int core_ctl_force_pause_cpu(int cpu, bool paused);
extern int core_ctl_enable_policy(unsigned int policy);
#endif

#if IS_ENABLED(CONFIG_MTK_CPUQOS_V3)
extern int set_cpuqos_mode(int mode);
extern int set_ct_task(int pid, bool set);
extern int set_ct_group(int group_id, bool set);
#endif

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

struct _CORE_CTL_PACKAGE {
	union {
		__u32 cid;
		__u32 cpu;
	};
	union {
		__u32 min;
		__u32 is_pause;
		__u32 throttle_ms;
		__u32 not_preferred_cpus;
		__u32 boost;
		__u32 thres;
		__u32 enable_policy;
	};
	__u32 max;
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

struct _CPUQOS_V3_PACKAGE {
	__u32 mode;
	__u32 pid;
	__u32 set_task;
	__u32 group_id;
	__u32 set_group;
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

#define EARA_NN_BEGIN               _IOW('g', 1, struct _EARA_NN_PACKAGE)
#define EARA_NN_END                 _IOW('g', 2, struct _EARA_NN_PACKAGE)
#define EARA_GETUSAGE               _IOW('g', 3, struct _EARA_NN_PACKAGE)
#define EARA_GETSTATE               _IOW('g', 4, struct _EARA_NN_PACKAGE)

#define EAS_SYNC_SET                            _IOW('g', 1,  unsigned int)
#define EAS_SYNC_GET                            _IOW('g', 2,  unsigned int)
#define EAS_PERTASK_LS_SET                      _IOW('g', 3,  unsigned int)
#define EAS_PERTASK_LS_GET                      _IOR('g', 4,  unsigned int)
#define EAS_ACTIVE_MASK_GET                     _IOR('g', 5,  unsigned int)
#define CORE_CTL_FORCE_RESUME_CPU               _IOW('g', 6,  struct _CORE_CTL_PACKAGE)
#define CORE_CTL_FORCE_PAUSE_CPU                _IOW('g', 7,  struct _CORE_CTL_PACKAGE)
#define CORE_CTL_SET_OFFLINE_THROTTLE_MS        _IOW('g', 8,  struct _CORE_CTL_PACKAGE)
#define CORE_CTL_SET_LIMIT_CPUS                 _IOW('g', 9,  struct _CORE_CTL_PACKAGE)
#define CORE_CTL_SET_NOT_PREFERRED              _IOW('g', 10, struct _CORE_CTL_PACKAGE)
#define CORE_CTL_SET_BOOST                      _IOW('g', 11, struct _CORE_CTL_PACKAGE)
#define CORE_CTL_SET_UP_THRES                   _IOW('g', 12, struct _CORE_CTL_PACKAGE)
#define CORE_CTL_ENABLE_POLICY                  _IOW('g', 13, struct _CORE_CTL_PACKAGE)
#define CPUQOS_V3_SET_CPUQOS_MODE		_IOW('g', 14, struct _CPUQOS_V3_PACKAGE)
#define CPUQOS_V3_SET_CT_TASK			_IOW('g', 15, struct _CPUQOS_V3_PACKAGE)
#define CPUQOS_V3_SET_CT_GROUP			_IOW('g', 16, struct _CPUQOS_V3_PACKAGE)
#define EAS_NEWLY_IDLE_BALANCE_INTERVAL_SET	_IOW('g', 17,  unsigned int)
#define EAS_NEWLY_IDLE_BALANCE_INTERVAL_GET	_IOR('g', 18,  unsigned int)
#define EAS_GET_THERMAL_HEADROOM_INTERVAL_SET	_IOW('g', 19,  unsigned int)
#define EAS_GET_THERMAL_HEADROOM_INTERVAL_GET	_IOR('g', 20,  unsigned int)


#define XGFFRAME_START              _IOW('g', 1, struct _XGFFRAME_PACKAGE)
#define XGFFRAME_END                _IOW('g', 2, struct _XGFFRAME_PACKAGE)
#define XGFFRAME_MIN_CAP            _IOW('g', 3, struct _XGFFRAME_PACKAGE)

#endif

