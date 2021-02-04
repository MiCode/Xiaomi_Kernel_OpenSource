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

#ifndef __MTK_SPM_SODI_H_
#define __MTK_SPM_SODI_H_

#include <mtk_cpuidle.h>
#include <mtk_spm_idle.h>
#include "mtk_spm_misc.h"
#include "mtk_spm_internal.h"
#include "mtk_spm_pmic_wrap.h"
#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
#include <mtk_dramc.h>
#endif
#include "mtk_vcorefs_governor.h"

#if defined(CONFIG_ARM_PSCI) || defined(CONFIG_MTK_PSCI)
#define MCUSYS_SMC_WRITE(addr, val)  mcusys_smc_write_phy(addr##_PHYS, val)
#else
#define MCUSYS_SMC_WRITE(addr, val)  mcusys_smc_write(addr, val)
#endif

#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)

#define SODI_VSRAM_VPROC_SHUTDOWN
#define VCORE_OPP_MASK (0x00ff0000)
#define VCORE_OPP_SHIFT (16)

#if defined(CONFIG_MICROTRUST_TEE_SUPPORT)
#define WAKE_SRC_FOR_SODI \
		(WAKE_SRC_R12_PCM_TIMER | \
		WAKE_SRC_R12_KP_IRQ_B | \
		WAKE_SRC_R12_APXGPT1_EVENT_B | \
		WAKE_SRC_R12_CONN2AP_SPM_WAKEUP_B | \
		WAKE_SRC_R12_EINT_EVENT_B | \
		WAKE_SRC_R12_CONN_WDT_IRQ_B | \
		WAKE_SRC_R12_CCIF0_EVENT_B | \
		WAKE_SRC_R12_MD32_SPM_IRQ_B | \
		WAKE_SRC_R12_USB_CDSC_B | \
		WAKE_SRC_R12_USB_POWERDWN_B | \
		WAKE_SRC_R12_C2K_WDT_IRQ_B | \
		WAKE_SRC_R12_EINT_EVENT_SECURE_B | \
		WAKE_SRC_R12_CCIF1_EVENT_B | \
		WAKE_SRC_R12_AFE_IRQ_MCU_B | \
		WAKE_SRC_R12_SYS_CIRQ_IRQ_B | \
		WAKE_SRC_R12_CSYSPWREQ_B | \
		WAKE_SRC_R12_MD1_WDT_B | \
		WAKE_SRC_R12_CLDMA_EVENT_B)
#else
#define WAKE_SRC_FOR_SODI \
		(WAKE_SRC_R12_PCM_TIMER | \
		WAKE_SRC_R12_KP_IRQ_B | \
		WAKE_SRC_R12_APXGPT1_EVENT_B | \
		WAKE_SRC_R12_CONN2AP_SPM_WAKEUP_B | \
		WAKE_SRC_R12_EINT_EVENT_B | \
		WAKE_SRC_R12_CONN_WDT_IRQ_B | \
		WAKE_SRC_R12_CCIF0_EVENT_B | \
		WAKE_SRC_R12_MD32_SPM_IRQ_B | \
		WAKE_SRC_R12_USB_CDSC_B | \
		WAKE_SRC_R12_USB_POWERDWN_B | \
		WAKE_SRC_R12_C2K_WDT_IRQ_B | \
		WAKE_SRC_R12_EINT_EVENT_SECURE_B | \
		WAKE_SRC_R12_CCIF1_EVENT_B | \
		WAKE_SRC_R12_AFE_IRQ_MCU_B | \
		WAKE_SRC_R12_SYS_CIRQ_IRQ_B | \
		WAKE_SRC_R12_CSYSPWREQ_B | \
		WAKE_SRC_R12_MD1_WDT_B | \
		WAKE_SRC_R12_CLDMA_EVENT_B | \
		WAKE_SRC_R12_SEJ_WDT_GPT_B)
#endif /* #if defined(CONFIG_MICROTRUST_TEE_SUPPORT) */

#define WAKE_SRC_FOR_MD32  0

#else
#error "Does not support!"
#endif

#define reg_read(addr)         __raw_readl(IOMEM(addr))
#define reg_write(addr, val)   mt_reg_sync_writel((val), ((void *)addr))

#define SODI_TAG     "[SODI] "
#define SODI3_TAG    "[SODI3] "

#define sodi_err(fmt, args...)     pr_info(SODI_TAG fmt, ##args)
#define sodi_warn(fmt, args...)    pr_info(SODI_TAG fmt, ##args)
#define sodi_debug(fmt, args...)   pr_info(SODI_TAG fmt, ##args)
#define sodi3_err(fmt, args...)	   pr_info(SODI3_TAG fmt, ##args)
#define sodi3_warn(fmt, args...)   pr_info(SODI3_TAG fmt, ##args)
#define sodi3_debug(fmt, args...)  pr_info(SODI3_TAG fmt, ##args)
#define so_err(fg, fmt, args...)   ((fg&SODI_FLAG_3P0)?pr_info(SODI3_TAG fmt, ##args):pr_info(SODI_TAG fmt, ##args))
#define so_warn(fg, fmt, args...)  ((fg&SODI_FLAG_3P0)?pr_info(SODI3_TAG fmt, ##args):pr_info(SODI_TAG fmt, ##args))
#define so_debug(fg, fmt, args...)				\
	do {										\
		if (fg&SODI_FLAG_3P0)					\
			pr_debug(SODI3_TAG fmt, ##args);	\
		else									\
			pr_debug(SODI_TAG fmt, ##args);		\
	} while (0)


#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
#define SPM_BYPASS_SYSPWREQ     1
#else
#define SPM_BYPASS_SYSPWREQ     0
#endif

#define ALL_TOP_CON_MASK	0x037F
#define PMIC_VAL_SIZE_MASK	0xFFFF
#define MEMPLL_PD_MODE		false
#define MEMPLL_CG_MODE		true

#ifdef SPM_SODI_PROFILE_TIME
extern unsigned int	soidle_profile[4];
#endif

enum spm_sodi_step {
	SPM_SODI_ENTER = 0,
	SPM_SODI_ENTER_UART_SLEEP,
	SPM_SODI_ENTER_SPM_FLOW,
	SPM_SODI_B3,
	SPM_SODI_B4,
	SPM_SODI_B5,
	SPM_SODI_B6,
	SPM_SODI_ENTER_WFI,
	SPM_SODI_LEAVE_WFI,
	SPM_SODI_LEAVE_SPM_FLOW,
	SPM_SODI_ENTER_UART_AWAKE,
	SPM_SODI_LEAVE,
	SPM_SODI_REKICK_VCORE,
};

enum spm_sodi_logout_reason {
	SODI_LOGOUT_NONE = 0,
	SODI_LOGOUT_ASSERT = 1,
	SODI_LOGOUT_NOT_GPT_EVENT = 2,
	SODI_LOGOUT_RESIDENCY_ABNORMAL = 3,
	SODI_LOGOUT_EMI_STATE_CHANGE = 4,
	SODI_LOGOUT_LONG_INTERVAL = 5,
	SODI_LOGOUT_CG_PD_STATE_CHANGE = 6,
	SODI_LOGOUT_UNKNOWN = -1,
};

#if SPM_AEE_RR_REC
void __attribute__((weak)) aee_rr_rec_sodi_val(u32 val)
{
}

u32 __attribute__((weak)) aee_rr_curr_sodi_val(void)
{
	return 0;
}
#endif

#if defined(CONFIG_MACH_KIBOPLUS)
int __attribute__((weak)) vcorefs_get_curr_ddr(void)
{
	return 0;
}
#endif

static __always_inline u32 spm_sodi_get_pcm_idx(u32 cpu)
{
	u32 idx = 0;

#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
	if (get_ddr_type() == TYPE_LPDDR3)
		idx = DYNA_LOAD_PCM_SODI + cpu / 4;
	else
		idx = DYNA_LOAD_PCM_SODI_LPDDR4 + cpu / 4;
#if defined(CONFIG_MTK_PMIC_CHIP_MT6355)
	if (get_ddr_type() == TYPE_LPDDR4)
		idx = DYNA_LOAD_PCM_SODI_LPDDR4_2400 + cpu / 4;
#endif
#else
	idx = DYNA_LOAD_PCM_SODI + cpu / 4;
#endif
	return idx;
}

static inline void spm_sodi_footprint(enum spm_sodi_step step)
{
#if SPM_AEE_RR_REC
	aee_rr_rec_sodi_val(aee_rr_curr_sodi_val() | (1 << step));
#endif
}

static inline void spm_sodi_footprint_val(u32 val)
{
#if SPM_AEE_RR_REC
		aee_rr_rec_sodi_val(aee_rr_curr_sodi_val() | val);
#endif
}

static inline void spm_sodi_aee_init(void)
{
#if SPM_AEE_RR_REC
	aee_rr_rec_sodi_val(0);
#endif
}

#define spm_sodi_reset_footprint() spm_sodi_aee_init()

void spm_trigger_wfi_for_sodi(struct pwr_ctrl *pwrctrl);
void spm_enable_mmu_smi_async(void);
void spm_disable_mmu_smi_async(void);
void spm_sodi_pmic_before_wfi(void);
void spm_sodi_pmic_after_wfi(void);
unsigned int
spm_sodi_output_log(struct wake_status *wakesta, struct pcm_desc *pcmdesc, int vcore_status, u32 sodi_flags);
void spm_sodi_get_vcore_opp(u32 *flags);

#endif /* __MTK_SPM_SODI_H_ */

