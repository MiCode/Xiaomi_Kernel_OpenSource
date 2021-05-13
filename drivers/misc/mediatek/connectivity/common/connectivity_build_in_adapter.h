/*
 * Copyright (C) 2016 MediaTek Inc.
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

#ifndef CONNECTIVITY_BUILD_IN_ADAPTER_H
#define CONNECTIVITY_BUILD_IN_ADAPTER_H

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/dma-mapping.h>
#include <linux/sched/clock.h>

/*******************************************************************************
 * Clock Buffer Control
 *
 * The Connsys adaptation layer must provide Clock Buffer Control support
 * should it be available from the platform.
 * Therefore CONNADP_HAS_CLOCK_BUF_CTRL is defined based on platform chip, and
 * is used to decide if adaptation has support on Clock Buffer Control.
 *
 * Each WMT platform file must still define its own CONSYS_CLOCK_BUF_CTRL to
 * decide if it is a co-clock'd platform.
 * It is possible that the Clock Buffer Control is available on the platform,
 * but is not used by Connsys.
 *
 * For Kernel-4,4, definition of CONNADP_HAS_CLOCK_BUF_CTRL must align with:
 *	drivers/misc/mediatek/base/power/include/mtk_clkbuf_ctl.h
 *
 * Platform that wishes to use Clock Buffer Control, please be sure to #include
 * the header file above.
 ******************************************************************************/
#if defined(CONFIG_MACH_MT6735) || \
	defined(CONFIG_MACH_MT6735M) || \
	defined(CONFIG_MACH_MT6753) || \
	defined(CONFIG_MACH_MT6739) || \
	defined(CONFIG_MACH_MT6755) || \
	defined(CONFIG_MACH_MT6757) || \
	defined(CONFIG_MACH_MT6758) || \
	defined(CONFIG_MACH_MT6759) || \
	defined(CONFIG_MACH_MT6763) || \
	defined(CONFIG_MACH_MT6775) || \
	defined(CONFIG_MACH_MT6797) || \
	defined(CONFIG_MACH_MT6799) || \
	defined(CONFIG_MACH_MT6580) || \
	defined(CONFIG_MACH_MT6765) || \
	defined(CONFIG_MACH_MT6761) || \
	defined(CONFIG_MACH_MT3967) || \
	defined(CONFIG_MACH_MT6771) || \
	defined(CONFIG_MACH_MT6768) || \
	defined(CONFIG_MACH_MT6785) || \
	defined(CONFIG_MACH_KIBOPLUS) || \
	defined(CONFIG_MACH_MT6885) || \
	defined(CONFIG_MACH_MT6853) || \
	defined(CONFIG_MACH_MT6873) || \
	defined(CONFIG_MACH_ELBRUS) || \
	defined(CONFIG_MACH_MT6893) || \
	defined(CONFIG_MACH_MT6877)
#define CONNADP_HAS_CLOCK_BUF_CTRL
#define KERNEL_CLK_BUF_CHIP_NOT_SUPPORT -7788
#define KERNEL_clk_buf_ctrl connectivity_export_clk_buf_ctrl
#define KERNEL_clk_buf_show_status_info \
		connectivity_export_clk_buf_show_status_info
#define KERNEL_clk_buf_get_xo_en_sta \
		connectivity_export_clk_buf_get_xo_en_sta
enum clk_buf_id;
void connectivity_export_clk_buf_ctrl(enum clk_buf_id id, bool onoff);
void connectivity_export_clk_buf_show_status_info(void);
int connectivity_export_clk_buf_get_xo_en_sta(/*enum xo_id id*/ int id);
#endif

/*******************************************************************************
 * PMIC
 * Caller please be sure to #include:
 *	drivers/misc/mediatek/pmic/include/mt6359/mtk_pmic_api_buck.h
 *	drivers/misc/mediatek/include/mt-plat/upmu_common.h
 ******************************************************************************/
#define KERNEL_pmic_config_interface \
	connectivity_export_pmic_config_interface
#define KERNEL_pmic_read_interface \
	connectivity_export_pmic_read_interface
#define KERNEL_pmic_set_register_value \
	connectivity_export_pmic_set_register_value
#define KERNEL_pmic_get_register_value \
	connectivity_export_pmic_get_register_value
#define KERNEL_upmu_set_reg_value \
	connectivity_export_upmu_set_reg_value
#if defined(CONFIG_MTK_PMIC_CHIP_MT6359) || \
	defined(CONFIG_MTK_PMIC_CHIP_MT6359P)
#define KERNEL_pmic_ldo_vcn13_lp \
	connectivity_export_pmic_ldo_vcn13_lp
#define KERNEL_pmic_ldo_vcn18_lp \
	connectivity_export_pmic_ldo_vcn18_lp
#define KERNEL_pmic_ldo_vfe28_lp \
	connectivity_export_pmic_ldo_vfe28_lp
#define KERNEL_pmic_ldo_vcn33_1_lp \
	connectivity_export_pmic_ldo_vcn33_1_lp
#define KERNEL_pmic_ldo_vcn33_2_lp \
	connectivity_export_pmic_ldo_vcn33_2_lp
#endif
void connectivity_export_pmic_config_interface(unsigned int RegNum,
						unsigned int val,
						unsigned int MASK,
						unsigned int SHIFT);
void connectivity_export_pmic_read_interface(unsigned int RegNum,
						unsigned int *val,
						unsigned int MASK,
						unsigned int SHIFT);
void connectivity_export_pmic_set_register_value(int flagname,
						unsigned int val);
unsigned short connectivity_export_pmic_get_register_value(int flagname);
void connectivity_export_upmu_set_reg_value(unsigned int reg,
						unsigned int reg_val);
#if defined(CONFIG_MTK_PMIC_CHIP_MT6359) || \
	defined(CONFIG_MTK_PMIC_CHIP_MT6359P)
int connectivity_export_pmic_ldo_vcn13_lp(int user,
		int op_mode, unsigned char op_en, unsigned char op_cfg);
int connectivity_export_pmic_ldo_vcn18_lp(int user,
		int op_mode, unsigned char op_en, unsigned char op_cfg);
void connectivity_export_pmic_ldo_vfe28_lp(unsigned int user,
		int op_mode, unsigned char op_en, unsigned char op_cfg);
int connectivity_export_pmic_ldo_vcn33_1_lp(int user,
		int op_mode, unsigned char op_en, unsigned char op_cfg);
int connectivity_export_pmic_ldo_vcn33_2_lp(int user,
		int op_mode, unsigned char op_en, unsigned char op_cfg);
#endif

/*******************************************************************************
 * MMC
 * Caller please be sure to #include:
 *	<linux/mmc/host.h>
 *	<linux/mmc/card.h>
 *	drivers/mmc/core/sdio_ops.h
 ******************************************************************************/
#define KERNEL_mmc_io_rw_direct connectivity_export_mmc_io_rw_direct
struct mmc_card;
int connectivity_export_mmc_io_rw_direct(struct mmc_card *card, int write,
						unsigned int fn,
						unsigned int addr,
						u8 in, u8 *out);

/*******************************************************************************
 * MT6306 I2C-based GPIO Expander
 ******************************************************************************/
#ifdef CONFIG_MTK_MT6306_GPIO_SUPPORT
#define KERNEL_mt6306_set_gpio_out connectivity_export_mt6306_set_gpio_out
#define KERNEL_mt6306_set_gpio_dir connectivity_export_mt6306_set_gpio_dir
void connectivity_export_mt6306_set_gpio_out(unsigned long pin,
						unsigned long output);
void connectivity_export_mt6306_set_gpio_dir(unsigned long pin,
						unsigned long dir);
#endif


#ifdef CONFIG_ARCH_MT6570
#define CPU_BOOST y
#endif
#ifdef CONFIG_ARCH_MT6755
#define CPU_BOOST y
#endif
#ifdef CONFIG_MACH_MT6757
#define CPU_BOOST y
#endif
#ifdef CONFIG_MACH_MT6758
#define CPU_BOOST y
#endif
#ifdef CONFIG_MACH_MT6763
#define CPU_BOOST y
#endif
#ifdef CONFIG_MACH_MT6799
#define CPU_BOOST y
#endif
#ifdef CONFIG_MACH_MT6739
#define CPU_BOOST y
#endif
#ifdef CPU_BOOST
#include "mtk_ppm_api.h"
#include "mtk_spm_resource_req.h"
#endif

#define KERNEL_show_stack connectivity_export_show_stack
#define KERNEL_tracing_record_cmdline connectivity_export_tracing_record_cmdline
#define KERNEL_dump_thread_state connectivity_export_dump_thread_state

#ifdef CPU_BOOST
#define KERNEL_mt_ppm_sysboost_freq connectivity_export_mt_ppm_sysboost_freq
#define KERNEL_mt_ppm_sysboost_core connectivity_export_mt_ppm_sysboost_core
#define KERNEL_mt_ppm_sysboost_set_core_limit \
		connectivity_export_mt_ppm_sysboost_set_core_limit
#define KERNEL_mt_ppm_sysboost_set_freq_limit \
		connectivity_export_mt_ppm_sysboost_set_freq_limit
#define KERNEL_spm_resource_req  connectivity_export_spm_resource_req
#else
#define KERNEL_mt_ppm_sysboost_freq
#define KERNEL_mt_ppm_sysboost_core
#define KERNEL_mt_ppm_sysboost_set_core_limit
#define KERNEL_mt_ppm_sysboost_set_freq_limit
#define KERNEL_spm_resource_req
#endif
extern void tracing_record_cmdline(struct task_struct *tsk);
extern void show_stack(struct task_struct *tsk, unsigned long *sp);
#ifdef CPU_BOOST
extern void mt_ppm_sysboost_freq(enum ppm_sysboost_user user,
				 unsigned int freq);
extern void mt_ppm_sysboost_core(enum ppm_sysboost_user user,
				 unsigned int core_num);
extern void mt_ppm_sysboost_set_core_limit(enum ppm_sysboost_user user,
					   unsigned int cluster,
					   int min_core, int max_core);
extern void mt_ppm_sysboost_set_freq_limit(enum ppm_sysboost_user user,
					   unsigned int cluster,
					   int min_freq, int max_freq);
extern bool spm_resource_req(unsigned int user, unsigned int req_mask);
#endif

void connectivity_export_show_stack(struct task_struct *tsk, unsigned long *sp);
void connectivity_export_dump_thread_state(const char *name);
void connectivity_export_tracing_record_cmdline(struct task_struct *tsk);

void connectivity_export_conap_scp_init(unsigned int chip_info, phys_addr_t emi_phy_addr);
void connectivity_export_conap_scp_deinit(void);

#ifdef CPU_BOOST
void connectivity_export_mt_ppm_sysboost_freq(enum ppm_sysboost_user user,
					      unsigned int freq);
void connectivity_export_mt_ppm_sysboost_core(enum ppm_sysboost_user user,
					      unsigned int core_num);
void connectivity_export_mt_ppm_sysboost_set_core_limit(
				enum ppm_sysboost_user user,
				unsigned int cluster,
				int min_core, int max_core);
void connectivity_export_mt_ppm_sysboost_set_freq_limit(
				enum ppm_sysboost_user user,
				unsigned int cluster,
				int min_freq, int max_freq);
bool connectivity_export_spm_resource_req(unsigned int user,
				unsigned int req_mask);
#endif

/*********************************************
 * copy from
 * kernel-3.18/include/linux/ftrace_event.h
 * kernel-4.4/include/linux/trace_events.h
 *
 * event_trace_printk()
 *********************************************/
#ifndef CONFIG_MACH_MT6739
#define KERNEL_event_trace_printk(ip, fmt, args...)               \
do {                                                              \
	__trace_printk_check_format(fmt, ##args);                 \
	KERNEL_tracing_record_cmdline(current);                   \
	if (__builtin_constant_p(fmt)) {                          \
		static const char *trace_printk_fmt               \
		__attribute__((section("__trace_printk_fmt"))) =  \
		__builtin_constant_p(fmt) ? fmt : NULL;           \
		__trace_bprintk(ip, trace_printk_fmt, ##args);    \
	} else                                                    \
		__trace_printk(ip, fmt, ##args);                  \
} while (0)
#endif
/******************************************************************************
 * GPIO dump information
 ******************************************************************************/
#ifndef CONFIG_MTK_GPIO
#define KERNEL_gpio_dump_regs_range connectivity_export_dump_gpio_info
extern void gpio_dump_regs_range(int start, int end);
void connectivity_export_dump_gpio_info(int start, int end);
#endif

int connectivity_export_gpio_get_tristate_input(unsigned int pin);

#if defined(CONFIG_MACH_MT8167)
#define CONNADP_HAS_UPMU_VCN_CTRL
#define KERNEL_upmu_set_vcn35_on_ctrl_bt conn_upmu_set_vcn35_on_ctrl_bt
#define KERNEL_upmu_set_vcn35_on_ctrl_wifi conn_upmu_set_vcn35_on_ctrl_wifi
void conn_upmu_set_vcn35_on_ctrl_bt(unsigned int val);
void conn_upmu_set_vcn35_on_ctrl_wifi(unsigned int val);
#endif

#endif /* CONNECTIVITY_BUILD_IN_ADAPTER_H */
