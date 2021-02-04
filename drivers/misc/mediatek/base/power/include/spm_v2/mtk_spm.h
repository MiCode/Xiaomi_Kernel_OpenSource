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

#ifndef _MT_SPM_
#define _MT_SPM_

#include <linux/kernel.h>
#include <linux/io.h>
#include <mach/upmu_hw.h> /* for PMIC power settings */

extern void __iomem *spm_base;
extern void __iomem *spm_infracfg_ao_base;
extern void __iomem *spm_cksys_base;
extern void __iomem *spm_mcucfg;
#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
extern void __iomem *spm_bsi1cfg;
#endif
#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
extern void __iomem *spm_dramc_ch0_top0_base;
extern void __iomem *spm_dramc_ch0_top1_base;
extern void __iomem *spm_dramc_ch1_top0_base;
extern void __iomem *spm_dramc_ch1_top1_base;
#else
extern void __iomem *spm_ddrphy_base;
#endif
extern u32 spm_irq_0;
extern u32 spm_irq_1;
extern u32 spm_irq_2;
extern u32 spm_irq_3;
extern u32 spm_irq_4;
extern u32 spm_irq_5;
extern u32 spm_irq_6;
extern u32 spm_irq_7;
#undef SPM_BASE
#define SPM_BASE spm_base
#define SPM_INFRACFG_AO_BASE spm_infracfg_ao_base
#define SPM_THERMAL_TIMER  23	/* 2 ^ (SPM_THERMAL_TIMER-15) second */
/* #include <mach/mt_irq.h> */
#include <mt-plat/sync_write.h>

/**************************************
 * Config and Parameter
 **************************************/
#define SPM_IRQ0_ID		spm_irq_0
#define SPM_IRQ1_ID		spm_irq_1
#define SPM_IRQ2_ID		spm_irq_2
#define SPM_IRQ3_ID		spm_irq_3
#define SPM_IRQ4_ID		spm_irq_4
#define SPM_IRQ5_ID		spm_irq_5
#define SPM_IRQ6_ID		spm_irq_6
#define SPM_IRQ7_ID		spm_irq_7

#include "mtk_spm_reg.h"

enum {
	WR_NONE = 0,
	WR_UART_BUSY = 1,
	WR_PCM_ASSERT = 2,
	WR_PCM_TIMER = 3,
	WR_WAKE_SRC = 4,
	WR_UNKNOWN = 5,
};

enum mt_vcorefs_fw {
	VCOREFS_FW_LPM	 = (1 << 0),	/*  1600/1.0  : 1270/0.9 : 1066/0.9 */
	VCOREFS_FW_HPM	 = (1 << 1),	/*  1700/1.0  : 1270/0.9 : 1066/0.9 */
	VCOREFS_FW_ULTRA = (1 << 2),	/*  1866/1.05 : 1600/1.0 : 1270/0.9 */
};

struct twam_sig {
	u32 sig0;		/* signal 0: config or status */
	u32 sig1;		/* signal 1: config or status */
	u32 sig2;		/* signal 2: config or status */
	u32 sig3;		/* signal 3: config or status */
};

typedef void (*twam_handler_t) (struct twam_sig *twamsig);
typedef void (*vcorefs_handler_t) (int opp);
typedef void (*vcorefs_start_handler_t) (void);

/* check if spm firmware ready */
extern int spm_load_firmware_status(void);

/* for power management init */
extern int spm_module_init(void);

/* for ANC in talking */
extern void spm_mainpll_on_request(const char *drv_name);
extern void spm_mainpll_on_unrequest(const char *drv_name);

/* for TWAM in MET */
extern void spm_twam_register_handler(twam_handler_t handler);
extern void spm_twam_enable_monitor(const struct twam_sig *twamsig, bool speed_mode);
extern void spm_twam_disable_monitor(void);
extern void spm_twam_set_idle_select(unsigned int sel);
extern void spm_twam_set_window_length(unsigned int len);
extern void spm_twam_set_mon_type(struct twam_sig *mon);

/* for Vcore DVFS */
extern int spm_go_to_ddrdfs(u32 spm_flags, u32 spm_data);

/* for Vcore DVFS in MET */
extern void spm_vcorefs_register_handler(vcorefs_handler_t handler, vcorefs_start_handler_t start_handler);
#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
#if !defined(CONFIG_FPGA_EARLY_PORTING)
extern int vcorefs_get_hw_opp(void);
#endif
#endif

/* for PMIC power settings */
enum {
	PMIC_PWR_NORMAL = 0,
	PMIC_PWR_DEEPIDLE,
	PMIC_PWR_SODI3,
	PMIC_PWR_SODI,
	PMIC_PWR_SUSPEND,
	PMIC_PWR_NUM,
};
void spm_pmic_power_mode(int mode, int force, int lock);
void spm_bypass_boost_gpio_set(void);

extern void unmask_edge_trig_irqs_for_cirq(void);

/**************************************
 * Macro and Inline
 **************************************/
#define get_high_cnt(sigsta)		((sigsta) & 0x3ff)
#define get_high_percent(sigsta)	((get_high_cnt(sigsta) * 100 + 511) / 1023)

#endif
