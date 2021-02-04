/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/tracepoint.h>
#include <trace/events/sched.h>
#include <trace/events/power.h>
#include <linux/dma-mapping.h>

#include <linux/kallsyms.h>
#include <linux/printk.h>
#include <linux/perf_event.h>
#include <linux/kthread.h>
#include <asm/arch_timer.h>
#include <asm/cpu.h>
#include <linux/smp.h> /* arch_send_call_function_single_ipi */

/******************************************************************************
 * Tracepoints
 ******************************************************************************/
#define MET_DEFINE_PROBE(probe_name, proto) \
		static void probe_##probe_name(void *data, PARAMS(proto))
#define MET_REGISTER_TRACE(probe_name) \
		register_trace_##probe_name(probe_##probe_name, NULL)
#define MET_UNREGISTER_TRACE(probe_name) \
		unregister_trace_##probe_name(probe_##probe_name, NULL)

struct met_api_tbl {
	int (*met_tag_start)(unsigned int class_id,
			     const char *name);
	int (*met_tag_end)(unsigned int class_id,
			   const char *name);
	int (*met_tag_async_start)(unsigned int class_id,
				   const char *name,
				   unsigned int cookie);
	int (*met_tag_async_end)(unsigned int class_id,
				 const char *name,
				 unsigned int cookie);
	int (*met_tag_oneshot)(unsigned int class_id,
			       const char *name,
			       unsigned int value);
	int (*met_tag_userdata)(char *pData);
	int (*met_tag_dump)(unsigned int class_id,
			    const char *name,
			    void *data,
			    unsigned int length);
	int (*met_tag_disable)(unsigned int class_id);
	int (*met_tag_enable)(unsigned int class_id);
	int (*met_set_dump_buffer)(int size);
	int (*met_save_dump_buffer)(const char *pathname);
	int (*met_save_log)(const char *pathname);
	int (*met_show_bw_limiter)(void);
	int (*met_reg_bw_limiter)(void *fp);
	int (*met_show_clk_tree)(const char *name,
				 unsigned int addr,
				 unsigned int status);
	int (*met_reg_clk_tree)(void *fp);
	void (*met_sched_switch)(struct task_struct *prev,
				 struct task_struct *next);
	int (*enable_met_backlight_tag)(void);
	int (*output_met_backlight_tag)(int level);
};

struct met_api_tbl met_ext_api;
EXPORT_SYMBOL(met_ext_api);

int met_tag_init(void)
{
	return 0;
}
EXPORT_SYMBOL(met_tag_init);

int met_tag_uninit(void)
{
	return 0;
}
EXPORT_SYMBOL(met_tag_uninit);

int met_tag_start(unsigned int class_id, const char *name)
{
	if (met_ext_api.met_tag_start)
		return met_ext_api.met_tag_start(class_id, name);
	return 0;
}
EXPORT_SYMBOL(met_tag_start);

int met_tag_end(unsigned int class_id, const char *name)
{
	if (met_ext_api.met_tag_end)
		return met_ext_api.met_tag_end(class_id, name);
	return 0;
}
EXPORT_SYMBOL(met_tag_end);

int met_tag_async_start(unsigned int class_id,
			const char *name,
			unsigned int cookie)
{
	if (met_ext_api.met_tag_async_start)
		return met_ext_api.met_tag_async_start(class_id, name, cookie);
	return 0;
}
EXPORT_SYMBOL(met_tag_async_start);

int met_tag_async_end(unsigned int class_id,
		      const char *name,
		      unsigned int cookie)
{
	if (met_ext_api.met_tag_async_end)
		return met_ext_api.met_tag_async_end(class_id, name, cookie);
	return 0;
}
EXPORT_SYMBOL(met_tag_async_end);

int met_tag_oneshot(unsigned int class_id, const char *name, unsigned int value)
{
	if (met_ext_api.met_tag_oneshot)
		return met_ext_api.met_tag_oneshot(class_id, name, value);
	return 0;
}
EXPORT_SYMBOL(met_tag_oneshot);

int met_tag_userdata(char *pData)
{
	if (met_ext_api.met_tag_userdata)
		return met_ext_api.met_tag_userdata(pData);
	return 0;
}
EXPORT_SYMBOL(met_tag_userdata);

int met_tag_dump(unsigned int class_id,
		 const char *name,
		 void *data,
		 unsigned int length)
{
	if (met_ext_api.met_tag_dump)
		return met_ext_api.met_tag_dump(class_id, name, data, length);
	return 0;
}
EXPORT_SYMBOL(met_tag_dump);

int met_tag_disable(unsigned int class_id)
{
	if (met_ext_api.met_tag_disable)
		return met_ext_api.met_tag_disable(class_id);
	return 0;
}
EXPORT_SYMBOL(met_tag_disable);

int met_tag_enable(unsigned int class_id)
{
	if (met_ext_api.met_tag_enable)
		return met_ext_api.met_tag_enable(class_id);
	return 0;
}
EXPORT_SYMBOL(met_tag_enable);

int met_set_dump_buffer(int size)
{
	if (met_ext_api.met_set_dump_buffer)
		return met_ext_api.met_set_dump_buffer(size);
	return 0;
}
EXPORT_SYMBOL(met_set_dump_buffer);

int met_save_dump_buffer(const char *pathname)
{
	if (met_ext_api.met_save_dump_buffer)
		return met_ext_api.met_save_dump_buffer(pathname);
	return 0;
}
EXPORT_SYMBOL(met_save_dump_buffer);

int met_save_log(const char *pathname)
{
	if (met_ext_api.met_save_log)
		return met_ext_api.met_save_log(pathname);
	return 0;
}
EXPORT_SYMBOL(met_save_log);

int met_show_bw_limiter(void)
{
	if (met_ext_api.met_show_bw_limiter)
		return met_ext_api.met_show_bw_limiter();
	return 0;
}
EXPORT_SYMBOL(met_show_bw_limiter);

int met_reg_bw_limiter(void *fp)
{
	if (met_ext_api.met_reg_bw_limiter)
		return met_ext_api.met_reg_bw_limiter(fp);
	return 0;
}
EXPORT_SYMBOL(met_reg_bw_limiter);

int met_show_clk_tree(const char *name,
				unsigned int addr,
				unsigned int status)
{
	if (met_ext_api.met_show_clk_tree)
		return met_ext_api.met_show_clk_tree(name, addr, status);
	return 0;
}
EXPORT_SYMBOL(met_show_clk_tree);

int met_reg_clk_tree(void *fp)
{
	if (met_ext_api.met_reg_clk_tree)
		return met_ext_api.met_reg_clk_tree(fp);
	return 0;
}
EXPORT_SYMBOL(met_reg_clk_tree);

MET_DEFINE_PROBE(sched_switch,
		 TP_PROTO(bool preempt,
			  struct task_struct *prev,
			  struct task_struct *next))
{
	if (met_ext_api.met_sched_switch)
		met_ext_api.met_sched_switch(prev, next);
}

int met_reg_switch(void)
{
	if (MET_REGISTER_TRACE(sched_switch)) {
		pr_debug("can not register callback of sched_switch\n");
		return -ENODEV;
	} else
		return 0;
}
EXPORT_SYMBOL(met_reg_switch);

void met_unreg_switch(void)
{
	MET_UNREGISTER_TRACE(sched_switch);
}
EXPORT_SYMBOL(met_unreg_switch);

#if	defined(CONFIG_MET_ARM_32BIT)
void met_get_cpuinfo(int cpu, struct cpuinfo_arm **cpuinfo)
{
	*cpuinfo = &per_cpu(cpu_data, cpu);
}
#else
void met_get_cpuinfo(int cpu, struct cpuinfo_arm64 **cpuinfo)
{
	*cpuinfo = &per_cpu(cpu_data, cpu);
}
#endif
EXPORT_SYMBOL(met_get_cpuinfo);

void met_cpu_frequency(unsigned int frequency, unsigned int cpu_id)
{
	trace_cpu_frequency(frequency, cpu_id);
}
EXPORT_SYMBOL(met_cpu_frequency);

void met_tracing_record_cmdline(struct task_struct *tsk)
{
	tracing_record_cmdline(tsk);
}
EXPORT_SYMBOL(met_tracing_record_cmdline);

void met_set_kptr_restrict(int value)
{
	kptr_restrict = value;
}
EXPORT_SYMBOL(met_set_kptr_restrict);

int met_get_kptr_restrict(void)
{
	return kptr_restrict;
}
EXPORT_SYMBOL(met_get_kptr_restrict);

void met_arch_setup_dma_ops(struct device *dev)
{
	arch_setup_dma_ops(dev, 0, 0, NULL, false);
}
EXPORT_SYMBOL(met_arch_setup_dma_ops);

int enable_met_backlight_tag(void)
{
	if (met_ext_api.enable_met_backlight_tag)
		return met_ext_api.enable_met_backlight_tag();
	return 0;
}
EXPORT_SYMBOL(enable_met_backlight_tag);

int output_met_backlight_tag(int level)
{
	if (met_ext_api.output_met_backlight_tag)
		return met_ext_api.output_met_backlight_tag(level);
	return 0;
}
EXPORT_SYMBOL(output_met_backlight_tag);

/* the following handle weak function in met_drv.h */
void met_mmsys_event_gce_thread_begin(ulong thread_no, ulong task_handle,
				ulong engineFlag, void *pCmd, ulong size)
{
}
EXPORT_SYMBOL(met_mmsys_event_gce_thread_begin);

void met_mmsys_event_gce_thread_end(ulong thread_no,
				    ulong task_handle,
				    ulong engineFlag)
{
}
EXPORT_SYMBOL(met_mmsys_event_gce_thread_end);

void met_mmsys_event_disp_sof(int mutex_id)
{
}
EXPORT_SYMBOL(met_mmsys_event_disp_sof);

void met_mmsys_event_disp_mutex_eof(int mutex_id)
{
}
EXPORT_SYMBOL(met_mmsys_event_disp_mutex_eof);

void met_mmsys_event_disp_ovl_eof(int ovl_id)
{
}
EXPORT_SYMBOL(met_mmsys_event_disp_ovl_eof);

void met_mmsys_config_isp_base_addr(unsigned long *isp_reg_list)
{
}
EXPORT_SYMBOL(met_mmsys_config_isp_base_addr);

void met_mmsys_event_isp_pass1_begin(int sensor_id)
{
}
EXPORT_SYMBOL(met_mmsys_event_isp_pass1_begin);

void met_mmsys_event_isp_pass1_end(int sensor_id)
{
}
EXPORT_SYMBOL(met_mmsys_event_isp_pass1_end);

void met_show_pmic_info(unsigned int RegNum, unsigned int pmic_reg)
{
}
EXPORT_SYMBOL(met_show_pmic_info);

u64 met_perf_event_read_local(struct perf_event *ev)
{
	return perf_event_read_local(ev);
}
EXPORT_SYMBOL(met_perf_event_read_local);

struct task_struct *met_kthread_create_on_cpu(int (*threadfn)(void *data),
				void *data, unsigned int cpu,
				const char *namefmt)
{
	return kthread_create_on_cpu(threadfn, data, cpu, namefmt);
}
EXPORT_SYMBOL(met_kthread_create_on_cpu);

int met_smp_call_function_single(
	int cpu,
	smp_call_func_t func,
	void *info,
	int wait)
{
	return smp_call_function_single(cpu, func, info, wait);
}
EXPORT_SYMBOL(met_smp_call_function_single);

u64 met_arch_counter_get_cntvct(void)
{
	return arch_counter_get_cntvct();
}
EXPORT_SYMBOL(met_arch_counter_get_cntvct);

void met_arch_send_call_function_single_ipi(int cpu)
{
	return arch_send_call_function_single_ipi(cpu);
}
EXPORT_SYMBOL(met_arch_send_call_function_single_ipi);
