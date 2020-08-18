/*
 * Copyright (C) 2015 MediaTek Inc.
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

#ifndef __MTK_RAM_CONSOLE_H__
#define __MTK_RAM_CONSOLE_H__

#include <linux/console.h>
#include <linux/pstore.h>

enum AEE_FIQ_STEP_NUM {
	AEE_FIQ_STEP_FIQ_ISR_BASE = 1,
	AEE_FIQ_STEP_WDT_FIQ_INFO = 4,
	AEE_FIQ_STEP_WDT_FIQ_STACK,
	AEE_FIQ_STEP_WDT_FIQ_LOOP,
	AEE_FIQ_STEP_WDT_FIQ_DONE,
	AEE_FIQ_STEP_WDT_IRQ_INFO = 8,
	AEE_FIQ_STEP_WDT_IRQ_KICK,
	AEE_FIQ_STEP_WDT_IRQ_SMP_STOP,
	AEE_FIQ_STEP_WDT_IRQ_TIME,
	AEE_FIQ_STEP_WDT_IRQ_STACK,
	AEE_FIQ_STEP_WDT_IRQ_GIC,
	AEE_FIQ_STEP_WDT_IRQ_LOCALTIMER,
	AEE_FIQ_STEP_WDT_IRQ_IDLE,
	AEE_FIQ_STEP_WDT_IRQ_SCHED,
	AEE_FIQ_STEP_WDT_IRQ_DONE,
	AEE_FIQ_STEP_HANG_DETECT,
	AEE_FIQ_STEP_KE_WDT_INFO = 20,
	AEE_FIQ_STEP_KE_WDT_PERCPU,
	AEE_FIQ_STEP_KE_WDT_LOG,
	AEE_FIQ_STEP_KE_SCHED_DEBUG,
	AEE_FIQ_STEP_KE_EINT_DEBUG,
	AEE_FIQ_STEP_KE_WDT_DONE,
	AEE_FIQ_STEP_KE_IPANIC_DIE = 32,
	AEE_FIQ_STEP_KE_IPANIC_START,
	AEE_FIQ_STEP_KE_IPANIC_OOP_HEADER,
	AEE_FIQ_STEP_KE_IPANIC_DETAIL,
	AEE_FIQ_STEP_KE_IPANIC_CONSOLE,
	AEE_FIQ_STEP_KE_IPANIC_USERSPACE,
	AEE_FIQ_STEP_KE_IPANIC_ANDROID,
	AEE_FIQ_STEP_KE_IPANIC_MMPROFILE,
	AEE_FIQ_STEP_KE_IPANIC_HEADER,
	AEE_FIQ_STEP_KE_IPANIC_DONE,
	AEE_FIQ_STEP_KE_NESTED_PANIC = 64,
};

enum AEE_EXP_TYPE_NUM {
	AEE_EXP_TYPE_HWT = 1,
	AEE_EXP_TYPE_KE = 2,
	AEE_EXP_TYPE_NESTED_PANIC = 3,
	AEE_EXP_TYPE_SMART_RESET = 4,
	AEE_EXP_TYPE_HANG_DETECT = 5,
};

#ifdef CONFIG_MTK_RAM_CONSOLE
extern void aee_rr_rec_clk(int id, u32 val);
extern int aee_rr_reboot_reason_show(struct seq_file *m, void *v);
extern int aee_rr_last_fiq_step(void);
extern void aee_rr_rec_exp_type(unsigned int type);
extern unsigned int aee_rr_curr_exp_type(void);
extern void aee_rr_rec_scp(void);
extern void aee_rr_rec_kaslr_offset(u64 value64);
extern void aee_rr_rec_cpu_dvfs_vproc_big(u8 val);
extern void aee_rr_rec_cpu_dvfs_vproc_little(u8 val);
extern void aee_rr_rec_cpu_dvfs_oppidx(u8 val);
extern void aee_rr_rec_cpu_dvfs_cci_oppidx(u8 val);
extern void aee_rr_rec_cpu_dvfs_status(u8 val);
extern void aee_rr_rec_cpu_dvfs_step(u8 val);
extern void aee_rr_rec_cpu_dvfs_cb(u8 val);
extern void aee_rr_rec_cpufreq_cb(u8 val);
extern u8 aee_rr_curr_cpu_dvfs_oppidx(void);
extern u8 aee_rr_curr_cpu_dvfs_cci_oppidx(void);
extern u8 aee_rr_curr_cpu_dvfs_status(void);
extern u8 aee_rr_curr_cpu_dvfs_step(void);
extern u8 aee_rr_curr_cpu_dvfs_cb(void);
extern u8 aee_rr_curr_cpufreq_cb(void);
extern void aee_rr_rec_ptp_devinfo_0(u32 val);
extern void aee_rr_rec_ptp_devinfo_1(u32 val);
extern void aee_rr_rec_ptp_devinfo_2(u32 val);
extern void aee_rr_rec_ptp_devinfo_3(u32 val);
extern void aee_rr_rec_ptp_devinfo_4(u32 val);
extern void aee_rr_rec_ptp_devinfo_5(u32 val);
extern void aee_rr_rec_ptp_devinfo_6(u32 val);
extern void aee_rr_rec_ptp_devinfo_7(u32 val);
extern void aee_rr_rec_ptp_e0(u32 val);
extern void aee_rr_rec_ptp_e1(u32 val);
extern void aee_rr_rec_ptp_e2(u32 val);
extern void aee_rr_rec_ptp_e3(u32 val);
extern void aee_rr_rec_ptp_e4(u32 val);
extern void aee_rr_rec_ptp_e5(u32 val);
extern void aee_rr_rec_ptp_e6(u32 val);
extern void aee_rr_rec_ptp_e7(u32 val);
extern void aee_rr_rec_ptp_e8(u32 val);
extern void aee_rr_rec_ptp_e9(u32 val);
extern void aee_rr_rec_ptp_e10(u32 val);
extern void aee_rr_rec_ptp_e11(u32 val);
extern void aee_rr_rec_ptp_vboot(u64 val);
extern void aee_rr_rec_ptp_cpu_big_volt(u64 val);
extern void aee_rr_rec_ptp_cpu_big_volt_1(u64 val);
extern void aee_rr_rec_ptp_cpu_big_volt_2(u64 val);
extern void aee_rr_rec_ptp_cpu_big_volt_3(u64 val);
extern void aee_rr_rec_ptp_gpu_volt(u64 val);
extern void aee_rr_rec_ptp_gpu_volt_1(u64 val);
extern void aee_rr_rec_ptp_gpu_volt_2(u64 val);
extern void aee_rr_rec_ptp_gpu_volt_3(u64 val);
extern void aee_rr_rec_ptp_cpu_little_volt(u64 val);
extern void aee_rr_rec_ptp_cpu_little_volt_1(u64 val);
extern void aee_rr_rec_ptp_cpu_little_volt_2(u64 val);
extern void aee_rr_rec_ptp_cpu_little_volt_3(u64 val);
extern void aee_rr_rec_ptp_cpu_2_little_volt(u64 val);
extern void aee_rr_rec_ptp_cpu_2_little_volt_1(u64 val);
extern void aee_rr_rec_ptp_cpu_2_little_volt_2(u64 val);
extern void aee_rr_rec_ptp_cpu_2_little_volt_3(u64 val);
extern void aee_rr_rec_ptp_cpu_cci_volt(u64 val);
extern void aee_rr_rec_ptp_cpu_cci_volt_1(u64 val);
extern void aee_rr_rec_ptp_cpu_cci_volt_2(u64 val);
extern void aee_rr_rec_ptp_cpu_cci_volt_3(u64 val);
extern void aee_rr_rec_ptp_temp(u64 val);
extern void aee_rr_rec_ptp_status(u8 val);
extern void aee_rr_rec_eem_pi_offset(u8 val);
extern u32 aee_rr_curr_ptp_devinfo_0(void);
extern u32 aee_rr_curr_ptp_devinfo_1(void);
extern u32 aee_rr_curr_ptp_devinfo_2(void);
extern u32 aee_rr_curr_ptp_devinfo_3(void);
extern u32 aee_rr_curr_ptp_devinfo_4(void);
extern u32 aee_rr_curr_ptp_devinfo_5(void);
extern u32 aee_rr_curr_ptp_devinfo_6(void);
extern u32 aee_rr_curr_ptp_devinfo_7(void);
extern u32 aee_rr_curr_ptp_e0(void);
extern u32 aee_rr_curr_ptp_e1(void);
extern u32 aee_rr_curr_ptp_e2(void);
extern u32 aee_rr_curr_ptp_e3(void);
extern u32 aee_rr_curr_ptp_e4(void);
extern u32 aee_rr_curr_ptp_e5(void);
extern u32 aee_rr_curr_ptp_e6(void);
extern u32 aee_rr_curr_ptp_e7(void);
extern u32 aee_rr_curr_ptp_e8(void);
extern u32 aee_rr_curr_ptp_e9(void);
extern u32 aee_rr_curr_ptp_e10(void);
extern u32 aee_rr_curr_ptp_e11(void);
extern u64 aee_rr_curr_ptp_vboot(void);
extern u64 aee_rr_curr_ptp_cpu_big_volt(void);
extern u64 aee_rr_curr_ptp_cpu_big_volt_1(void);
extern u64 aee_rr_curr_ptp_cpu_big_volt_2(void);
extern u64 aee_rr_curr_ptp_cpu_big_volt_3(void);
extern u64 aee_rr_curr_ptp_gpu_volt(void);
extern u64 aee_rr_curr_ptp_gpu_volt_1(void);
extern u64 aee_rr_curr_ptp_gpu_volt_2(void);
extern u64 aee_rr_curr_ptp_gpu_volt_3(void);
extern u64 aee_rr_curr_ptp_cpu_little_volt(void);
extern u64 aee_rr_curr_ptp_cpu_little_volt_1(void);
extern u64 aee_rr_curr_ptp_cpu_little_volt_2(void);
extern u64 aee_rr_curr_ptp_cpu_little_volt_3(void);
extern u64 aee_rr_curr_ptp_cpu_2_little_volt(void);
extern u64 aee_rr_curr_ptp_cpu_2_little_volt_1(void);
extern u64 aee_rr_curr_ptp_cpu_2_little_volt_2(void);
extern u64 aee_rr_curr_ptp_cpu_2_little_volt_3(void);
extern u64 aee_rr_curr_ptp_cpu_cci_volt(void);
extern u64 aee_rr_curr_ptp_cpu_cci_volt_1(void);
extern u64 aee_rr_curr_ptp_cpu_cci_volt_2(void);
extern u64 aee_rr_curr_ptp_cpu_cci_volt_3(void);
extern u64 aee_rr_curr_ptp_temp(void);
extern u8 aee_rr_curr_ptp_status(void);
extern unsigned long *aee_rr_rec_mtk_cpuidle_footprint_va(void);
extern unsigned long *aee_rr_rec_mtk_cpuidle_footprint_pa(void);
extern void aee_rr_rec_sodi3_val(u32 val);
extern u32 aee_rr_curr_sodi3_val(void);
extern void aee_rr_rec_sodi_val(u32 val);
extern u32 aee_rr_curr_sodi_val(void);
extern void aee_rr_rec_deepidle_val(u32 val);
extern u32 aee_rr_curr_deepidle_val(void);
extern void aee_rr_rec_spm_suspend_val(u32 val);
extern u32 aee_rr_curr_spm_suspend_val(void);
extern void aee_rr_rec_vcore_dvfs_status(u32 val);
extern u32 aee_rr_curr_vcore_dvfs_status(void);
extern unsigned int *aee_rr_rec_mcdi_wfi(void);
extern void aee_rr_rec_mcdi_val(int id, u32 val);
extern void aee_rr_rec_vcore_dvfs_opp(u32 val);
extern u32 aee_rr_curr_vcore_dvfs_opp(void);
extern void aee_rr_rec_ocp_target_limit(int id, u32 val);
extern u32 aee_rr_curr_ocp_target_limit(int id);
extern void aee_rr_rec_ocp_enable(u8 val);
extern u8 aee_rr_curr_ocp_enable(void);
extern void aee_rr_rec_ppm_cluster_limit(int id, u32 val);
extern void aee_rr_rec_ppm_step(u8 val);
extern void aee_rr_rec_ppm_cur_state(u8 val);
extern void aee_rr_rec_ppm_min_pwr_bgt(u32 val);
extern void aee_rr_rec_ppm_policy_mask(u32 val);
extern void aee_rr_rec_ppm_waiting_for_pbm(u8 val);
extern void aee_rr_rec_gpu_dvfs_vgpu(u8 val);
extern u8 aee_rr_curr_gpu_dvfs_vgpu(void);
extern void aee_rr_rec_gpu_dvfs_oppidx(u8 val);
extern void aee_rr_rec_gpu_dvfs_status(u8 val);
extern u8 aee_rr_curr_gpu_dvfs_status(void);
extern void aee_rr_rec_gpu_dvfs_power_count(int val);
extern void aee_rr_rec_hang_detect_timeout_count(unsigned int val);
extern int aee_rr_curr_fiq_step(void);
extern void aee_rr_rec_fiq_step(u8 i);
extern void aee_rr_rec_last_irq_enter(int cpu, int irq, u64 j);
extern void aee_rr_rec_last_irq_exit(int cpu, int irq, u64 j);
extern void aee_rr_rec_hotplug_footprint(int cpu, u8 fp);
extern void aee_rr_rec_hotplug_cpu_event(u8 val);
extern void aee_rr_rec_hotplug_cb_index(u8 val);
extern void aee_rr_rec_hotplug_cb_fp(unsigned long val);
extern void aee_rr_rec_hotplug_cb_times(unsigned long val);
extern void aee_rr_rec_hps_cb_enter_times(unsigned long long val);
extern void aee_rr_rec_hps_cb_cpu_bitmask(unsigned int val);
extern void aee_rr_rec_hps_cb_footprint(unsigned int val);
extern void aee_rr_rec_hps_cb_fp_times(unsigned long long val);
extern void aee_rr_rec_last_init_func(unsigned long val);
extern void aee_rr_rec_last_sync_func(unsigned long val);
extern void aee_rr_rec_last_async_func(unsigned long val);
extern void aee_rr_rec_set_bit_pmic_ext_buck(int bit, int loc);
extern void aee_rr_init_thermal_temp(int num);
extern void aee_rr_rec_thermal_temp(int index, s16 val);
extern void aee_rr_rec_thermal_status(u8 val);
extern void aee_rr_rec_thermal_ATM_status(u8 val);
extern void aee_rr_rec_thermal_ktime(u64 val);
extern s16 aee_rr_curr_thermal_temp(int index);
extern u8 aee_rr_curr_thermal_status(void);
extern u8 aee_rr_curr_thermal_ATM_status(void);
extern u64 aee_rr_curr_thermal_ktime(void);
extern void aee_rr_rec_cpu_caller(u32 val);
extern void aee_rr_rec_cpu_callee(u32 val);
extern void aee_rr_rec_cpu_up_prepare_ktime(u64 val);
extern void aee_rr_rec_cpu_starting_ktime(u64 val);
extern void aee_rr_rec_cpu_online_ktime(u64 val);
extern void aee_rr_rec_cpu_down_prepare_ktime(u64 val);
extern void aee_rr_rec_cpu_dying_ktime(u64 val);
extern void aee_rr_rec_cpu_dead_ktime(u64 val);
extern void aee_rr_rec_cpu_post_dead_ktime(u64 val);
extern void aee_sram_fiq_log(const char *msg);
extern void ram_console_write(struct console *console, const char *s,
				unsigned int count);
extern void aee_sram_fiq_save_bin(const char *buffer, size_t len);
#else
static inline void aee_rr_rec_clk(int id, u32 val)
{
}

static inline int aee_rr_reboot_reason_show(struct seq_file *m, void *v)
{
	return 0;
}

static inline int aee_rr_last_fiq_step(void)
{
	return 0;
}

static inline void aee_rr_rec_exp_type(unsigned int type)
{
}

static inline unsigned int aee_rr_curr_exp_type(void)
{
	return 0;
}

static inline void aee_rr_rec_scp(void)
{
}

static inline void aee_rr_rec_kaslr_offset(u64 value64)
{
}

static inline void aee_rr_rec_cpu_dvfs_vproc_big(u8 val)
{
}

static inline void aee_rr_rec_cpu_dvfs_vproc_little(u8 val)
{
}

static inline void aee_rr_rec_cpu_dvfs_oppidx(u8 val)
{
}

static inline void aee_rr_rec_cpu_dvfs_cci_oppidx(u8 val)
{
}

static inline void aee_rr_rec_cpu_dvfs_status(u8 val)
{
}

static inline void aee_rr_rec_cpu_dvfs_step(u8 val)
{
}

static inline void aee_rr_rec_cpu_dvfs_cb(u8 val)
{
}

static inline void aee_rr_rec_cpufreq_cb(u8 val)
{
}

static inline u8 aee_rr_curr_cpu_dvfs_oppidx(void)
{
	return 0;
}

static inline u8 aee_rr_curr_cpu_dvfs_cci_oppidx(void)
{
	return 0;
}

static inline u8 aee_rr_curr_cpu_dvfs_status(void)
{
	return 0;
}

static inline u8 aee_rr_curr_cpu_dvfs_step(void)
{
	return 0;
}

static inline u8 aee_rr_curr_cpu_dvfs_cb(void)
{
	return 0;
}

static inline u8 aee_rr_curr_cpufreq_cb(void)
{
	return 0;
}

static inline void aee_rr_rec_ptp_devinfo_0(u32 val)
{
}

static inline void aee_rr_rec_ptp_devinfo_1(u32 val)
{
}

static inline void aee_rr_rec_ptp_devinfo_2(u32 val)
{
}

static inline void aee_rr_rec_ptp_devinfo_3(u32 val)
{
}

static inline void aee_rr_rec_ptp_devinfo_4(u32 val)
{
}

static inline void aee_rr_rec_ptp_devinfo_5(u32 val)
{
}

static inline void aee_rr_rec_ptp_devinfo_6(u32 val)
{
}

static inline void aee_rr_rec_ptp_devinfo_7(u32 val)
{
}

static inline void aee_rr_rec_ptp_e0(u32 val)
{
}

static inline void aee_rr_rec_ptp_e1(u32 val)
{
}

static inline void aee_rr_rec_ptp_e2(u32 val)
{
}

static inline void aee_rr_rec_ptp_e3(u32 val)
{
}

static inline void aee_rr_rec_ptp_e4(u32 val)
{
}

static inline void aee_rr_rec_ptp_e5(u32 val)
{
}

static inline void aee_rr_rec_ptp_e6(u32 val)
{
}

static inline void aee_rr_rec_ptp_e7(u32 val)
{
}

static inline void aee_rr_rec_ptp_e8(u32 val)
{
}

static inline void aee_rr_rec_ptp_e9(u32 val)
{
}

static inline void aee_rr_rec_ptp_e10(u32 val)
{
}

static inline void aee_rr_rec_ptp_e11(u32 val)
{
}

static inline void aee_rr_rec_ptp_vboot(u64 val)
{
}

static inline void aee_rr_rec_ptp_cpu_big_volt(u64 val)
{
}

static inline void aee_rr_rec_ptp_cpu_big_volt_1(u64 val)
{
}

static inline void aee_rr_rec_ptp_cpu_big_volt_2(u64 val)
{
}

static inline void aee_rr_rec_ptp_cpu_big_volt_3(u64 val)
{
}

static inline void aee_rr_rec_ptp_gpu_volt(u64 val)
{
}

static inline void aee_rr_rec_ptp_gpu_volt_1(u64 val)
{
}

static inline void aee_rr_rec_ptp_gpu_volt_2(u64 val)
{
}

static inline void aee_rr_rec_ptp_gpu_volt_3(u64 val)
{
}

static inline void aee_rr_rec_ptp_cpu_little_volt(u64 val)
{
}

static inline void aee_rr_rec_ptp_cpu_little_volt_1(u64 val)
{
}

static inline void aee_rr_rec_ptp_cpu_little_volt_2(u64 val)
{
}

static inline void aee_rr_rec_ptp_cpu_little_volt_3(u64 val)
{
}

static inline void aee_rr_rec_ptp_cpu_2_little_volt(u64 val)
{
}

static inline void aee_rr_rec_ptp_cpu_2_little_volt_1(u64 val)
{
}

static inline void aee_rr_rec_ptp_cpu_2_little_volt_2(u64 val)
{
}

static inline void aee_rr_rec_ptp_cpu_2_little_volt_3(u64 val)
{
}

static inline void aee_rr_rec_ptp_cpu_cci_volt(u64 val)
{
}

static inline void aee_rr_rec_ptp_cpu_cci_volt_1(u64 val)
{
}

static inline void aee_rr_rec_ptp_cpu_cci_volt_2(u64 val)
{
}

static inline void aee_rr_rec_ptp_cpu_cci_volt_3(u64 val)
{
}

static inline void aee_rr_rec_ptp_temp(u64 val)
{
}

static inline void aee_rr_rec_ptp_status(u8 val)
{
}

static inline void aee_rr_rec_eem_pi_offset(u8 val)
{
}

static inline u32 aee_rr_curr_ptp_devinfo_0(void)
{
	return 0;
}

static inline u32 aee_rr_curr_ptp_devinfo_1(void)
{
	return 0;
}

static inline u32 aee_rr_curr_ptp_devinfo_2(void)
{
	return 0;
}

static inline u32 aee_rr_curr_ptp_devinfo_3(void)
{
	return 0;
}

static inline u32 aee_rr_curr_ptp_devinfo_4(void)
{
	return 0;
}

static inline u32 aee_rr_curr_ptp_devinfo_5(void)
{
	return 0;
}

static inline u32 aee_rr_curr_ptp_devinfo_6(void)
{
	return 0;
}

static inline u32 aee_rr_curr_ptp_devinfo_7(void)
{
	return 0;
}

static inline u32 aee_rr_curr_ptp_e0(void)
{
	return 0;
}

static inline u32 aee_rr_curr_ptp_e1(void)
{
	return 0;
}

static inline u32 aee_rr_curr_ptp_e2(void)
{
	return 0;
}

static inline u32 aee_rr_curr_ptp_e3(void)
{
	return 0;
}

static inline u32 aee_rr_curr_ptp_e4(void)
{
	return 0;
}

static inline u32 aee_rr_curr_ptp_e5(void)
{
	return 0;
}

static inline u32 aee_rr_curr_ptp_e6(void)
{
	return 0;
}

static inline u32 aee_rr_curr_ptp_e7(void)
{
	return 0;
}

static inline u32 aee_rr_curr_ptp_e8(void)
{
	return 0;
}

static inline u32 aee_rr_curr_ptp_e9(void)
{
	return 0;
}

static inline u32 aee_rr_curr_ptp_e10(void)
{
	return 0;
}

static inline u32 aee_rr_curr_ptp_e11(void)
{
	return 0;
}

static inline u64 aee_rr_curr_ptp_vboot(void)
{
	return 0;
}

static inline u64 aee_rr_curr_ptp_cpu_big_volt(void)
{
	return 0;
}

static inline u64 aee_rr_curr_ptp_cpu_big_volt_1(void)
{
	return 0;
}

static inline u64 aee_rr_curr_ptp_cpu_big_volt_2(void)
{
	return 0;
}

static inline u64 aee_rr_curr_ptp_cpu_big_volt_3(void)
{
	return 0;
}

static inline u64 aee_rr_curr_ptp_gpu_volt(void)
{
	return 0;
}

static inline u64 aee_rr_curr_ptp_gpu_volt_1(void)
{
	return 0;
}

static inline u64 aee_rr_curr_ptp_gpu_volt_2(void)
{
	return 0;
}

static inline u64 aee_rr_curr_ptp_gpu_volt_3(void)
{
	return 0;
}

static inline u64 aee_rr_curr_ptp_cpu_little_volt(void)
{
	return 0;
}

static inline u64 aee_rr_curr_ptp_cpu_little_volt_1(void)
{
	return 0;
}

static inline u64 aee_rr_curr_ptp_cpu_little_volt_2(void)
{
	return 0;
}

static inline u64 aee_rr_curr_ptp_cpu_little_volt_3(void)
{
	return 0;
}

static inline u64 aee_rr_curr_ptp_cpu_2_little_volt(void)
{
	return 0;
}

static inline u64 aee_rr_curr_ptp_cpu_2_little_volt_1(void)
{
	return 0;
}

static inline u64 aee_rr_curr_ptp_cpu_2_little_volt_2(void)
{
	return 0;
}

static inline u64 aee_rr_curr_ptp_cpu_2_little_volt_3(void)
{
	return 0;
}

static inline u64 aee_rr_curr_ptp_cpu_cci_volt(void)
{
	return 0;
}

static inline u64 aee_rr_curr_ptp_cpu_cci_volt_1(void)
{
	return 0;
}

static inline u64 aee_rr_curr_ptp_cpu_cci_volt_2(void)
{
	return 0;
}

static inline u64 aee_rr_curr_ptp_cpu_cci_volt_3(void)
{
	return 0;
}

static inline u64 aee_rr_curr_ptp_temp(void)
{
	return 0;
}

static inline u8 aee_rr_curr_ptp_status(void)
{
	return 0;
}

static inline unsigned long *aee_rr_rec_mtk_cpuidle_footprint_va(void)
{
	return NULL;
}

static inline unsigned long *aee_rr_rec_mtk_cpuidle_footprint_pa(void)
{
	return NULL;
}

static inline void aee_rr_rec_sodi3_val(u32 val)
{
}

static inline u32 aee_rr_curr_sodi3_val(void)
{
	return 0;
}

static inline void aee_rr_rec_sodi_val(u32 val)
{
}

static inline u32 aee_rr_curr_sodi_val(void)
{
	return 0;
}

static inline void aee_rr_rec_deepidle_val(u32 val)
{
}

static inline u32 aee_rr_curr_deepidle_val(void)
{
	return 0;
}

static inline void aee_rr_rec_spm_suspend_val(u32 val)
{
}

static inline u32 aee_rr_curr_spm_suspend_val(void)
{
	return 0;
}

static inline void aee_rr_rec_vcore_dvfs_status(u32 val)
{
}

static inline u32 aee_rr_curr_vcore_dvfs_status(void)
{
	return 0;
}

static inline unsigned int *aee_rr_rec_mcdi_wfi(void)
{
	return NULL;
}

static inline void aee_rr_rec_mcdi_val(int id, u32 val)
{
}

static inline void aee_rr_rec_vcore_dvfs_opp(u32 val)
{
}

static inline u32 aee_rr_curr_vcore_dvfs_opp(void)
{
	return 0;
}

static inline void aee_rr_rec_ocp_target_limit(int id, u32 val)
{
}

static inline u32 aee_rr_curr_ocp_target_limit(int id)
{
	return 0;
}

static inline void aee_rr_rec_ocp_enable(u8 val)
{
}

static inline u8 aee_rr_curr_ocp_enable(void)
{
	return 0;
}

static inline void aee_rr_rec_ppm_cluster_limit(int id, u32 val)
{
}

static inline void aee_rr_rec_ppm_step(u8 val)
{
}

static inline void aee_rr_rec_ppm_cur_state(u8 val)
{
}

static inline void aee_rr_rec_ppm_min_pwr_bgt(u32 val)
{
}

static inline void aee_rr_rec_ppm_policy_mask(u32 val)
{
}

static inline void aee_rr_rec_ppm_waiting_for_pbm(u8 val)
{
}

static inline void aee_rr_rec_gpu_dvfs_vgpu(u8 val)
{
}

static inline u8 aee_rr_curr_gpu_dvfs_vgpu(void)
{
	return 0;
}

static inline void aee_rr_rec_gpu_dvfs_oppidx(u8 val)
{
}

static inline void aee_rr_rec_gpu_dvfs_status(u8 val)
{
}

static inline u8 aee_rr_curr_gpu_dvfs_status(void)
{
	return 0;
}

static inline void aee_rr_rec_gpu_dvfs_power_count(int val)
{
}

static inline void aee_rr_rec_hang_detect_timeout_count(unsigned int val)
{
}

static inline int aee_rr_curr_fiq_step(void)
{
	return 0;
}

static inline void aee_rr_rec_fiq_step(u8 i)
{
}

static inline void aee_rr_rec_last_irq_enter(int cpu, int irq, u64 j)
{
}

static inline void aee_rr_rec_last_irq_exit(int cpu, int irq, u64 j)
{
}

static inline void aee_rr_rec_hotplug_footprint(int cpu, u8 fp)
{
}

static inline void aee_rr_rec_hotplug_cpu_event(u8 val)
{
}

static inline void aee_rr_rec_hotplug_cb_index(u8 val)
{
}

static inline void aee_rr_rec_hotplug_cb_fp(unsigned long val)
{
}

static inline void aee_rr_rec_hotplug_cb_times(unsigned long val)
{
}

static inline void aee_rr_rec_hps_cb_enter_times(unsigned long long val)
{
}

static inline void aee_rr_rec_hps_cb_cpu_bitmask(unsigned int val)
{
}

static inline void aee_rr_rec_hps_cb_footprint(unsigned int val)
{
}

static inline void aee_rr_rec_hps_cb_fp_times(unsigned long long val)
{
}

static inline void aee_rr_rec_last_init_func(unsigned long val)
{
}

static inline void aee_rr_rec_last_sync_func(unsigned long val)
{
}

static inline void aee_rr_rec_last_async_func(unsigned long val)
{
}

static inline void aee_rr_rec_set_bit_pmic_ext_buck(int bit, int loc)
{
}

static inline void aee_rr_init_thermal_temp(int num)
{
}

static inline void aee_rr_rec_thermal_temp(int index, s16 val)
{
}

static inline void aee_rr_rec_thermal_status(u8 val)
{
}

static inline void aee_rr_rec_thermal_ATM_status(u8 val)
{
}

static inline void aee_rr_rec_thermal_ktime(u64 val)
{
}

static inline s16 aee_rr_curr_thermal_temp(int index)
{
	return 0;
}

static inline u8 aee_rr_curr_thermal_status(void)
{
	return 0;
}

static inline u8 aee_rr_curr_thermal_ATM_status(void)
{
	return 0;
}

static inline u64 aee_rr_curr_thermal_ktime(void)
{
	return 0;
}

static inline void aee_rr_rec_cpu_caller(u32 val)
{
}

static inline void aee_rr_rec_cpu_callee(u32 val)
{
}

static inline void aee_rr_rec_cpu_up_prepare_ktime(u64 val)
{
}

static inline void aee_rr_rec_cpu_starting_ktime(u64 val)
{
}

static inline void aee_rr_rec_cpu_online_ktime(u64 val)
{
}

static inline void aee_rr_rec_cpu_down_prepare_ktime(u64 val)
{
}

static inline void aee_rr_rec_cpu_dying_ktime(u64 val)
{
}

static inline void aee_rr_rec_cpu_dead_ktime(u64 val)
{
}

static inline void aee_rr_rec_cpu_post_dead_ktime(u64 val)
{
}

static inline void aee_sram_fiq_log(const char *msg)
{
}

static inline void ram_console_write(struct console *console, const char *s,
				unsigned int count)
{
}

static inline void aee_sram_fiq_save_bin(unsigned char *buffer, size_t len)
{
}

#endif /* CONFIG_MTK_RAM_CONSOLE */

#ifdef CONFIG_MTK_AEE_IPANIC
extern int ipanic_kmsg_write(unsigned int part, const char *buf, size_t size);
extern int ipanic_kmsg_get_next(int *count, u64 *id, enum pstore_type_id *type,
		struct timespec *time, char **buf, struct pstore_info *psi);
#else
static inline int ipanic_kmsg_write(unsigned int part, const char *buf,
				size_t size)
{
	return 0;
}

static inline int ipanic_kmsg_get_next(int *count, u64 *id,
			enum pstore_type_id *type, struct timespec *time,
			char **buf, struct pstore_info *psi)
{
	return 0;
}
#endif /* CONFIG_MTK_AEE_IPANIC */

#endif
