/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2023-2025 OS_CPU Technologies Co., Ltd.
 */

#ifndef __PLAT_COMMON_H__
#define __PLAT_COMMON_H__

#if IS_ENABLED(CONFIG_OS_CPU_PLAT_COMMON)
#include <linux/cpufreq.h>
#endif

enum {
	// qs.c
	FUNC_SCHED_SET_GROUP_ID = 1,
	FUNC_SCHED_GET_GROUP_ID,

	// vip_sched.c
	FUNC_TASK_UTIL,
	FUNC_CPU_HALTED,
	FUNC_CPU_HALT_MASK,
	FUNC_CAPACITY_ORIG_OF,
	FUNC_IS_MAX_POSSIBLE_CLUSTER_CPU,
	FUNC_FIND_IDLE_CPU_IN_FREQ_DOMAIN,
	// vip_sched.c depend RTG
	FUNC_GET_GROUP_PREFER_CPUS,
	FUNC_GET_GROUP_PREFER_UTIL,

	// rtg_pfr_cpu.c
	FUNC_CORE_CTL_UPDATE,
	FUNC_LOOKUP_RELATED_THREAD_GROUP,

	//mi_game.c
	FUNC_MI_GAME_FIND_ENERGY_HOOK,
	FUNC_MIGT_HOOK,
	FUNC_MI_GAME_WALT_GET_INDICIES,
	FUNC_GAME_GET_RAVG_WINDOW,
	FUNC_MI_SET_NEXT_FREQ,
	FUNC_MI_GAME_TASK_FORK,
	FUNC_MI_GAME_SCHEDULER_TICK,
	FUNC_MI_GAME_CHOOSE_CPU_FASTPATH,
	FUNC_FREE_MI_TASK_STRUCT,
	FUNC_MI_GAME_CHOOSE_EXCLUSIVE_CPUS,
	FUNC_MI_GAME_CHOOSE_CPU,
	FUNC_MI_GAME_SKIP_CPUS,
	FUNC_MI_GAME_SKIP_CPUS_RT,
	FUNC_MI_GAME_SHOULD_SKIP_CPU_RT,
	FUNC_MI_GAME_SHOULD_SKIP_MIGRATE,
	FUNC_MI_GAME_CHECK_RT,
	FUNC_MI_GAME_VIP_CHECK,

	//soc.c
	FUNC_UPDATE_FRAME_LOAD,
	FUNC_FLT_ENABLE,
	FUNC_GET_WAKEUP_INFO,
	FUNC_MI_FORCE_UPDATE_FREQ,
	FUNC_ENABLE_GPUSYNC,
	PLAT_HOOK_MAX_NR,
};

struct cgroup_subsys_state;

#if IS_ENABLED(CONFIG_OS_CPU_PLAT_COMMON)
// base function
extern int register_plat_func(unsigned int cmd, void *func);
extern int unregister_plat_func(unsigned int cmd);
// qs.c
extern int plat_sched_set_group_id(struct task_struct *p, unsigned int group_id);
extern unsigned int plat_sched_get_group_id(struct task_struct *p);
// vip_sched.c
extern unsigned long plat_task_util(struct task_struct *p);
extern bool plat_cpu_halted(int cpu);
extern struct cpumask * plat_cpu_halt_mask(void);
extern unsigned long plat_capacity_orig_of(int cpu);
extern bool plat_is_max_possible_cluster_cpu(int cpu);
extern int plat_find_idle_cpu_in_freq_domain(int cpu);
// vip_sched.c depend RTG
extern void plat_get_group_prefer_cpus(struct task_struct *p, struct cpumask *prefer_cpus);
extern unsigned long plat_get_group_prefer_util(struct task_struct *p);
// rtg_pfr_cpu.c
extern void plat_core_ctl_update(void);
extern struct walt_related_thread_group* plat_lookup_related_thread_group(unsigned int group_id);
#else /* CONFIG_OS_CPU_PLAT_COMMON */
// base function
static inline int register_plat_func(unsigned int cmd, void* func) { return 0; }
static inline int unregister_plat_func(unsigned int cmd) { return 0; }
// qs.c
static inline int plat_sched_set_group_id(struct task_struct *p, unsigned int group_id) { return 0; }
static inline unsigned int plat_sched_get_group_id(struct task_struct *p) { return 0; }
// vip_sched.c
static inline unsigned long plat_task_util(struct task_struct *p) { return 0; }
static inline bool plat_cpu_halted(int cpu) { return false; }
static inline struct cpumask * plat_cpu_halt_mask(void) { return NULL; }
static inline unsigned long plat_capacity_orig_of(int cpu) { return 0; }
static inline bool plat_is_max_possible_cluster_cpu(int cpu) { return false; }
static inline int plat_find_idle_cpu_in_freq_domain(int cpu) { return -1; }
// vip_sched.c depend RTG
static inline void plat_get_group_prefer_cpus(struct task_struct *p, struct cpumask *prefer_cpus) {}
static inline unsigned long plat_get_group_prefer_util(struct task_struct *p) { return 0; }
// rtg_pfr_cpu.c
static inline void plat_core_ctl_update(void) {}
static inline struct walt_related_thread_group* plat_lookup_related_thread_group(unsigned int group_id) { return NULL; }
#endif /* CONFIG_OS_CPU_PLAT_COMMON */

// plat_fork.c
#if IS_ENABLED(CONFIG_OS_CPU_DYNAMIC_TASK_SIZE) || !IS_ENABLED(CONFIG_OS_CPU_PLAT_COMMON)
static inline void os_cpu_sched_fork_init(struct task_struct *task) {}
static inline void os_cpu_dup_task_struct(struct task_struct *task) {}
static inline void os_cpu_free_task_struct(struct task_struct *task) {}
#else
extern void os_cpu_sched_fork_init(struct task_struct *task);
extern void os_cpu_dup_task_struct(struct task_struct *task);
extern void os_cpu_free_task_struct(struct task_struct *task);
#endif

#if IS_ENABLED(CONFIG_OS_CPU_PLAT_COMMON)
// audio
extern bool plat_is_relaxed_rt(struct task_struct *p);
#ifdef CONFIG_FAIR_GROUP_SCHED
extern void plat_init_sched_cgroup_id(struct cgroup_subsys_state *css);
#else
static inline void plat_init_sched_cgroup_id(struct cgroup_subsys_state *css) {};
#endif
#else /* CONFIG_OS_CPU_PLAT_COMMON */
// audio
static inline bool plat_is_relaxed_rt(struct task_struct *p) { return false; }
static inline void plat_init_sched_cgroup_id(struct cgroup_subsys_state *css) {}
#endif /* CONFIG_OS_CPU_PLAT_COMMON */

#endif /*__PLAT_COMMON_H__*/
