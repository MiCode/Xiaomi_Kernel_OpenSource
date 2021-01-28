/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MET_API_H__
#define __MET_API_H__

#if	!defined(CONFIG_MET_ARM_32BIT)
#include <asm/cpu.h>
#endif

struct met_register_tbl {
	void (*met_sched_switch)(struct task_struct *prev,
				 struct task_struct *next);
	void (*met_pm_qos_update_request)(int pm_qos_class, s32 value);
	void (*met_pm_qos_update_target)(unsigned int action,
		int prev_value, int curr_value);
};

struct met_export_tbl {
	int (*met_reg_switch)(void);
	void (*met_unreg_switch)(void);
	int (*met_reg_event_power)(void);
	void (*met_unreg_event_power)(void);
#if	!defined(CONFIG_MET_ARM_32BIT)
	void (*met_get_cpuinfo)(int cpu, struct cpuinfo_arm64 **cpuinfo);
#endif
	void (*met_cpu_frequency)(unsigned int frequency, unsigned int cpu_id);
	void (*met_set_kptr_restrict)(int value);
	int (*met_get_kptr_restrict)(void);
	void (*met_arch_setup_dma_ops)(struct device *dev);
	int (*met_perf_event_read_local)(struct perf_event *ev, u64 *value);
	int (*met_smp_call_function_single)(int cpu, smp_call_func_t func,
					void *info, int wait);
	u64 (*met_arch_counter_get_cntvct)(void);
	void (*met_arch_send_call_function_single_ipi)(int cpu);
};

extern struct met_register_tbl met_register_api;
extern struct met_export_tbl met_export_api;

#endif
