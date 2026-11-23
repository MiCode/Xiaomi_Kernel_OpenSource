/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2023-2025 MI Technologies Co., Ltd.
 */

#ifndef __PERF_ACTUATOR_H__
#define __PERF_ACTUATOR_H__

#define PERF_ACTUATOR_MAGIC 'x'
#define PERF_R_CMD(idx, struct)  _IOR(PERF_ACTUATOR_MAGIC, idx, struct)
#define PERF_W_CMD(idx, struct)  _IOW(PERF_ACTUATOR_MAGIC, idx, struct)
#define PERF_RW_CMD(idx, struct)  _IOWR(PERF_ACTUATOR_MAGIC, idx, struct)

typedef int (*PERF_ACTUATOR_FUNC) (void __user *uarg);

enum {
	GET_SCHED_STAT = 1,
	SET_TASK_RTG,
	SET_RTG_CPUS,
	SET_RTG_UTIL,
	SET_RTG_LOCKFREQ, // 5
	SET_RTG_ROLLOVER,
	CLR_RTG,
	START_RT_TRACKER,
	STOP_RT_TRACKER,
	GET_RT_INFO, // 10
	GET_DDR_FLUX,
	SET_PRIOR_AFFINITY,
	GET_GPU_FENCE,
	GET_GPU_COUNTER,
	SET_GPU_CORENUM, // 15
	GET_CPU_UTIL,

	// using for multiframe
	SET_VSYNC_SF,
	SET_LIMIT_PID,
	GET_FENCE_QUEUE,
	GET_FRAME_QUEUE, // 20
	GET_SF_QUEUE,
	GET_FPS_QUEUE,
	GET_LIMIT_PID_QUEUE,

	GET_DEV_CAP = 24,
	SET_SCHED_PRIO, // 25
	SET_TRANS_RULE,

	/* ADD BELOW */
	SET_TID_QOS,
	GET_PID_QOS,
	GET_TID_QOS,

	SET_VIP_PRIO, // 30
	SET_RTG_ED_TASK,

	GET_DDR_VOTE_DEV,
	GET_REGULATE_RES,
	GET_DISCOUNT_COEF,
	SET_PERI_RATE, // 35
	SET_VIP_WAIT_THRES,
	SET_TASK_ARRAY_RTG,

	SET_SCREEN_STATUS,

	GET_CPU_PARTIAL_GOOD = 40, //pg use this id
	SET_CPU_PARTIAL_GOOD = 41,

	PERF_ACTUATOR_MAX_NR,
};

#if IS_ENABLED(CONFIG_OS_CPU_PERF_ACTUATOR)
extern int register_perf_actuator(unsigned int cmd, PERF_ACTUATOR_FUNC func);
extern int unregister_perf_actuator(unsigned int cmd);
extern void on_gpu_fence_signal(int pid, int tid, uint64_t fence_addr);
extern void on_hwc_fence_signal(int64_t timestamp);
#else
static inline int register_perf_actuator(unsigned int cmd, PERF_ACTUATOR_FUNC func)
{
	return 0;
}
static inline int unregister_perf_actuator(unsigned int cmd)
{
	return 0;
}
static inline void on_gpu_fence_signal(int pid, int tid, uint64_t fence_addr) {}
static inline void on_hwc_fence_signal(int64_t timestamp) {}
#endif

#define XR_FBT_MAGIC 'x'

#define XR_FBT_R_CMD(idx, struct)  _IOR(XR_FBT_MAGIC, idx, struct)
#define XR_FBT_W_CMD(idx, struct)  _IOW(XR_FBT_MAGIC, idx, struct)
#define XR_FBT_RW_CMD(idx, struct)  _IOWR(XR_FBT_MAGIC, idx, struct)

typedef int (*XR_FBT_FUNC) (void __user *uarg);

enum {
	XR_FBT_BASE_IDX = 0,
	SET_DEQUEUE_BUFFER = XR_FBT_BASE_IDX,
	SET_QUEUE_BUFFER,
	SET_DO_FRAME,
	SET_DO_FRAME_END,
	SET_COMMON_FRAME,
	SET_COMMON_FRAME_END,
	SET_FPS,
	SET_TID_QOS_EXT,
	GET_PID_QOS_EXT,
	GET_TID_QOS_EXT,
	ISSUE_QOS_EVENT,
	SET_ASYNC_BINDER,

	XR_FBT_MAX_NR,
};

#if IS_ENABLED(CONFIG_OS_CPU_FBT)
extern int register_xr_fbt(unsigned int cmd, XR_FBT_FUNC func);
extern int unregister_xr_fbt(unsigned int cmd);
#else
static inline int register_xr_fbt(unsigned int cmd, XR_FBT_FUNC func)
{
	return 0;
}
static inline int unregister_xr_fbt(unsigned int cmd)
{
	return 0;
}
#endif

#endif /*__PERF_ACTUATOR_H__*/