// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/tracepoint.h>
#include <trace/events/sched.h>
#include <linux/pm_qos.h>
#include <trace/events/power.h>
#include <linux/dma-mapping.h>

#include <linux/kallsyms.h>
#include <linux/printk.h>
#include <linux/perf_event.h>
#include <linux/kthread.h>
#include <asm/arch_timer.h>
#include <asm/cpu.h>
#include <linux/smp.h> /* arch_send_call_function_single_ipi */

#include "met_api.h"

/******************************************************************************
 * Tracepoints
 ******************************************************************************/
#define MET_DEFINE_PROBE(probe_name, proto) \
		static void probe_##probe_name(void *data, PARAMS(proto))
#define MET_REGISTER_TRACE(probe_name) \
		register_trace_##probe_name(probe_##probe_name, NULL)
#define MET_UNREGISTER_TRACE(probe_name) \
		unregister_trace_##probe_name(probe_##probe_name, NULL)

struct met_register_tbl met_register_api;
EXPORT_SYMBOL(met_register_api);

MET_DEFINE_PROBE(sched_switch,
		 TP_PROTO(bool preempt,
			  struct task_struct *prev,
			  struct task_struct *next))
{
	if (met_register_api.met_sched_switch)
		met_register_api.met_sched_switch(prev, next);
}

int met_reg_switch(void)
{
	if (MET_REGISTER_TRACE(sched_switch)) {
		pr_debug("can not register callback of sched_switch\n");
		return -ENODEV;
	} else
		return 0;
}

void met_unreg_switch(void)
{
	MET_UNREGISTER_TRACE(sched_switch);
}

MET_DEFINE_PROBE(pm_qos_update_request,
	TP_PROTO(int pm_qos_class, s32 value))
{
	if (met_register_api.met_pm_qos_update_request)
		met_register_api.met_pm_qos_update_request(pm_qos_class, value);
}

MET_DEFINE_PROBE(pm_qos_update_target,
	TP_PROTO(enum pm_qos_req_action action, int prev_value, int curr_value))
{
	if (met_register_api.met_pm_qos_update_target)
		met_register_api.met_pm_qos_update_target((unsigned int)action,
			prev_value, curr_value);
}

int met_reg_event_power(void)
{
	do {
		if (MET_REGISTER_TRACE(pm_qos_update_request)) {
			pr_debug("can not register callback of pm_qos_update_request\n");
			return -ENODEV;
		}
		if (MET_REGISTER_TRACE(pm_qos_update_target)) {
			pr_debug("can not register callback of pm_qos_update_target\n");
			MET_UNREGISTER_TRACE(pm_qos_update_target);
			return -ENODEV;
		}
	} while (0);
	return 0;
}

void met_unreg_event_power(void)
{
	MET_UNREGISTER_TRACE(pm_qos_update_request);
	MET_UNREGISTER_TRACE(pm_qos_update_target);
}

#if	!defined(CONFIG_MET_ARM_32BIT)
void met_get_cpuinfo(int cpu, struct cpuinfo_arm64 **cpuinfo)
{
	*cpuinfo = &per_cpu(cpu_data, cpu);
}
#endif

void met_cpu_frequency(unsigned int frequency, unsigned int cpu_id)
{
	trace_cpu_frequency(frequency, cpu_id);
}

void met_set_kptr_restrict(int value)
{
	kptr_restrict = value;
}

int met_get_kptr_restrict(void)
{
	return kptr_restrict;
}

void met_arch_setup_dma_ops(struct device *dev)
{
	arch_setup_dma_ops(dev, 0, 0, NULL, false);
}

int met_perf_event_read_local(struct perf_event *ev, u64 *value)
{
	return perf_event_read_local(ev, value, NULL, NULL);
}

int met_smp_call_function_single(
	int cpu,
	smp_call_func_t func,
	void *info,
	int wait)
{
	return smp_call_function_single(cpu, func, info, wait);
}

u64 met_arch_counter_get_cntvct(void)
{
	return arch_counter_get_cntvct();
}

void met_arch_send_call_function_single_ipi(int cpu)
{
	return arch_send_call_function_single_ipi(cpu);
}

struct met_export_tbl met_export_api = {
	.met_reg_switch = met_reg_switch,
	.met_unreg_switch = met_unreg_switch,
	.met_reg_event_power = met_reg_event_power,
	.met_unreg_event_power = met_unreg_event_power,
#if	!defined(CONFIG_MET_ARM_32BIT)
	.met_get_cpuinfo = met_get_cpuinfo,
#endif
	.met_cpu_frequency = met_cpu_frequency,
	.met_set_kptr_restrict = met_set_kptr_restrict,
	.met_get_kptr_restrict = met_get_kptr_restrict,
	.met_arch_setup_dma_ops = met_arch_setup_dma_ops,
	.met_perf_event_read_local = met_perf_event_read_local,
	.met_smp_call_function_single = met_smp_call_function_single,
	.met_arch_counter_get_cntvct = met_arch_counter_get_cntvct,
	.met_arch_send_call_function_single_ipi =
		met_arch_send_call_function_single_ipi,
};
EXPORT_SYMBOL(met_export_api);

