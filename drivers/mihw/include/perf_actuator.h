/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2023-2023 XRing Technologies Co., Ltd.
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
	SET_RTG_LOCKFREQ = 5,
	SET_RTG_ROLLOVER,
	CLR_RTG,
	START_RT_TRACKER,
	STOP_RT_TRACKER,
	GET_RT_INFO = 10,
	GET_DDR_FLUX,
	SET_PRIOR_AFFINITY,
	GET_GPU_FENCE,
	GET_GPU_COUNTER,
	SET_GPU_CORENUM = 15,
	GET_CPU_UTIL,
	GET_DEV_CAP = 24,
	SET_SCHED_PRIO,
	SET_TRANS_RULE,

	/* ADD BELOW */
	SET_PID_QOS,
	GET_PID_QOS,
	GET_TID_QOS,

	SET_VIP_PRIO = 30,
	SET_RTG_ED_TASK,

	GET_DDR_VOTE_DEV,
	GET_REGULATE_RES,
	GET_DISCOUNT_COEF,
	SET_PERI_RATE = 35,
	SET_VIP_WAIT_THRES,
	PERF_ACTUATOR_MAX_NR,
};

#if IS_ENABLED(CONFIG_XRING_PERF_ACTUATOR)
extern int register_perf_actuator(unsigned int cmd, PERF_ACTUATOR_FUNC func);
extern int unregister_perf_actuator(unsigned int cmd);
#else
static inline int register_perf_actuator(unsigned int cmd, PERF_ACTUATOR_FUNC func)
{
	return 0;
}
static inline int unregister_perf_actuator(unsigned int cmd)
{
	return 0;
}
#endif

#endif /*__PERF_ACTUATOR_H__*/
