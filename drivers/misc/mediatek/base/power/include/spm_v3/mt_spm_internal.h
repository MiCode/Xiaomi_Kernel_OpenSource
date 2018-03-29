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

#ifndef _MT_SPM_INTERNAL_
#define _MT_SPM_INTERNAL_

#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/atomic.h>
#include <linux/io.h>
#include <mt-plat/aee.h>

#include "mt_clkbuf_ctl.h"
#include "mt_spm.h"
#include "mt_lpae.h"
#include "mt_gpio.h"

/**************************************
 * Config and Parameter
 **************************************/
/* FIXME: ifdef MTK_FORCE_CLUSTER1 */
#if 0
#define SPM_CTRL_BIG_CPU	1
#else
#define SPM_CTRL_BIG_CPU	0
#endif

#define POWER_ON_VAL1_DEF	0x00015830
/* FIXME: */
#define PCM_FSM_STA_DEF		0x00048490
/* FIXME: */
#define SPM_WAKEUP_EVENT_MASK_DEF	0xf0f83ebb

#define PCM_WDT_TIMEOUT		(30 * 32768)	/* 30s */
#define PCM_TIMER_MAX		(0xffffffff - PCM_WDT_TIMEOUT)

/**************************************
 * Define and Declare
 **************************************/
#define PCM_PWRIO_EN_R0		(1U << 0)
#define PCM_PWRIO_EN_R7		(1U << 7)
#define PCM_RF_SYNC_R0		(1U << 16)
#define PCM_RF_SYNC_R6		(1U << 22)
#define PCM_RF_SYNC_R7		(1U << 23)

#define PCM_SW_INT0		(1U << 0)
#define PCM_SW_INT1		(1U << 1)
#define PCM_SW_INT2		(1U << 2)
#define PCM_SW_INT3		(1U << 3)
#define PCM_SW_INT4		(1U << 4)
#define PCM_SW_INT5		(1U << 5)
#define PCM_SW_INT6		(1U << 6)
#define PCM_SW_INT7		(1U << 7)
#define PCM_SW_INT8		(1U << 8)
#define PCM_SW_INT9		(1U << 9)
#define PCM_SW_INT_ALL		(PCM_SW_INT9 | PCM_SW_INT8 | PCM_SW_INT7 | \
				 PCM_SW_INT6 | PCM_SW_INT5 | PCM_SW_INT4 | \
				 PCM_SW_INT3 | PCM_SW_INT2 | PCM_SW_INT1 | \
				 PCM_SW_INT0)

#define WFI_OP_AND		1
#define WFI_OP_OR		0

#define ISRM_TWAM		(1U << 2)
#define ISRM_PCM_RETURN		(1U << 3)
#define ISRM_RET_IRQ0		(1U << 8)
#define ISRM_RET_IRQ1		(1U << 9)
#define ISRM_RET_IRQ2		(1U << 10)
#define ISRM_RET_IRQ3		(1U << 11)
#define ISRM_RET_IRQ4		(1U << 12)
#define ISRM_RET_IRQ5		(1U << 13)
#define ISRM_RET_IRQ6		(1U << 14)
#define ISRM_RET_IRQ7		(1U << 15)
#define ISRM_RET_IRQ8		(1U << 16)
#define ISRM_RET_IRQ9		(1U << 17)
#define ISRM_RET_IRQ10		(1U << 18)
#define ISRM_RET_IRQ11		(1U << 19)
#define ISRM_RET_IRQ12		(1U << 20)
#define ISRM_RET_IRQ13		(1U << 21)
#define ISRM_RET_IRQ14		(1U << 22)
#define ISRM_RET_IRQ15		(1U << 23)

#define ISRM_RET_IRQ_AUX	(ISRM_RET_IRQ15 | ISRM_RET_IRQ14 | \
				 ISRM_RET_IRQ13 | ISRM_RET_IRQ12 | \
				 ISRM_RET_IRQ11 | ISRM_RET_IRQ10 | \
				 ISRM_RET_IRQ9 | ISRM_RET_IRQ8 | \
				 ISRM_RET_IRQ7 | ISRM_RET_IRQ6 | \
				 ISRM_RET_IRQ5 | ISRM_RET_IRQ4 | \
				 ISRM_RET_IRQ3 | ISRM_RET_IRQ2 | \
				 ISRM_RET_IRQ1)
#define ISRM_ALL_EXC_TWAM	(ISRM_RET_IRQ_AUX /*| ISRM_RET_IRQ0 | ISRM_PCM_RETURN*/)
#define ISRM_ALL		(ISRM_ALL_EXC_TWAM | ISRM_TWAM)

#define ISRS_TWAM		(1U << 2)
#define ISRS_PCM_RETURN		(1U << 3)
#define ISRS_SW_INT0		(1U << 4)

#define ISRC_TWAM		ISRS_TWAM
#define ISRC_ALL_EXC_TWAM	ISRS_PCM_RETURN
#define ISRC_ALL		(ISRC_ALL_EXC_TWAM | ISRC_TWAM)

#define WAKE_MISC_TWAM		(1U << 22)
#define WAKE_MISC_PCM_TIMER	(1U << 23)
#define WAKE_MISC_CPU_WAKE	(1U << 24)

#include <linux/platform_device.h>
extern struct platform_device *pspmdev;
#if !defined(CONFIG_MTK_CLKMGR)
#include <linux/clk.h>
#else
#include <mach/mt_clkmgr.h>
#endif

#ifndef CONFIG_MTK_FPGA
#if defined(CONFIG_ARCH_ELBRUS)
#define SPM_VCORE_EN_ELBRUS
#endif
#endif

struct pcm_desc {
	const char *version;	/* PCM code version */
	const u32 *base;	/* binary array base */
	dma_addr_t base_dma;	/* dma addr of base */
	const u16 size;		/* binary array size */
	const u8 sess;		/* session number */
	const u8 replace;	/* replace mode */
	const u16 addr_2nd;	/* 2nd binary array size */
	const u16 reserved;	/* for 32bit alignment */

	u32 vec0;		/* event vector 0 config */
	u32 vec1;		/* event vector 1 config */
	u32 vec2;		/* event vector 2 config */
	u32 vec3;		/* event vector 3 config */
	u32 vec4;		/* event vector 4 config */
	u32 vec5;		/* event vector 5 config */
	u32 vec6;		/* event vector 6 config */
	u32 vec7;		/* event vector 7 config */
	u32 vec8;		/* event vector 8 config */
	u32 vec9;		/* event vector 9 config */
	u32 vec10;		/* event vector 10 config */
	u32 vec11;		/* event vector 11 config */
	u32 vec12;		/* event vector 12 config */
	u32 vec13;		/* event vector 13 config */
	u32 vec14;		/* event vector 14 config */
	u32 vec15;		/* event vector 15 config */
};

struct pwr_ctrl {
	/* for SPM */
	u32 pcm_flags;
	u32 pcm_flags_cust;	/* can override pcm_flags */
	u32 pcm_flags_cust_set;	/* set bit of pcm_flags, after pcm_flags_cust */
	u32 pcm_flags_cust_clr;	/* clr bit of pcm_flags, after pcm_flags_cust */
	u32 pcm_reserve;
	u32 timer_val;		/* @ 1T 32K */
	u32 timer_val_cust;	/* @ 1T 32K, can override timer_val */
	u32 timer_val_ramp_en;		/* stress for dpidle */
	u32 timer_val_ramp_en_sec;	/* stress for suspend */
	u32 wake_src;
	u32 wake_src_cust;	/* can override wake_src */
	u32 wake_src_md32;
	u8 wdt_disable;		/* disable wdt in suspend */
	u8 dvfs_halt_src_chk;	/* vocre use */
	u8 syspwreq_mask;	/* make 26M off when attach ICE */

	/* Auto-gen Start */

	/* SPM_CLK_CON */
	u8 reg_srcclken0_ctl;
	u8 reg_srcclken1_ctl;
	u8 reg_spm_lock_infra_dcm;
	u8 reg_srcclken_mask;
	u8 reg_md1_c32rm_en;
	u8 reg_md2_c32rm_en;
	u8 reg_clksq0_sel_ctrl;
	u8 reg_clksq1_sel_ctrl;
	u8 reg_srcclken0_en;
	u8 reg_srcclken1_en;
	u8 reg_sysclk0_src_mask_b;
	u8 reg_sysclk1_src_mask_b;

	/* SPM_AP_STANDBY_CON */
	u8 reg_mpwfi_op;
	u8 reg_mp0_cputop_idle_mask;
	u8 reg_mp1_cputop_idle_mask;
	u8 reg_debugtop_idle_mask;
	u8 reg_mp_top_idle_mask;
	u8 reg_mcusys_idle_mask;
	u8 reg_md_ddr_en_0_dbc_en;
	u8 reg_md_ddr_en_1_dbc_en;
	u8 reg_conn_ddr_en_dbc_en;
	u8 reg_md32_mask_b;
	u8 reg_md_0_mask_b;
	u8 reg_md_1_mask_b;
	u8 reg_scp_mask_b;
	u8 reg_srcclkeni0_mask_b;
	u8 reg_srcclkeni1_mask_b;
	u8 reg_md_apsrc_1_sel;
	u8 reg_md_apsrc_0_sel;
	u8 reg_conn_mask_b;
	u8 reg_conn_apsrc_sel;

	/* SPM_SRC_REQ */
	u8 reg_spm_apsrc_req;
	u8 reg_spm_f26m_req;
	u8 reg_spm_infra_req;
	u8 reg_spm_ddren_req;
	u8 reg_spm_vrf18_req;
	u8 reg_spm_dvfs_level0_req;
	u8 reg_spm_dvfs_level1_req;
	u8 reg_spm_dvfs_level2_req;
	u8 reg_spm_dvfs_level3_req;
	u8 reg_spm_dvfs_level4_req;
	u8 reg_spm_pmcu_mailbox_req;
	u8 reg_spm_sw_mailbox_req;
	u8 reg_spm_cksel2_req;
	u8 reg_spm_cksel3_req;

	/* SPM_SRC_MASK */
	u8 reg_csyspwreq_mask;
	u8 reg_md_srcclkena_0_infra_mask_b;
	u8 reg_md_srcclkena_1_infra_mask_b;
	u8 reg_md_apsrc_req_0_infra_mask_b;
	u8 reg_md_apsrc_req_1_infra_mask_b;
	u8 reg_conn_srcclkena_infra_mask_b;
	u8 reg_conn_infra_req_mask_b;
	u8 reg_md32_srcclkena_infra_mask_b;
	u8 reg_md32_infra_req_mask_b;
	u8 reg_scp_srcclkena_infra_mask_b;
	u8 reg_scp_infra_req_mask_b;
	u8 reg_srcclkeni0_infra_mask_b;
	u8 reg_srcclkeni1_infra_mask_b;
	u8 reg_ccif0_md_event_mask_b;
	u8 reg_ccif0_ap_event_mask_b;
	u8 reg_ccif1_md_event_mask_b;
	u8 reg_ccif1_ap_event_mask_b;
	u8 reg_ccif2_md_event_mask_b;
	u8 reg_ccif2_ap_event_mask_b;
	u8 reg_ccif3_md_event_mask_b;
	u8 reg_ccif3_ap_event_mask_b;
	u8 reg_ccifmd_md1_event_mask_b;
	u8 reg_ccifmd_md2_event_mask_b;
	u8 reg_c2k_ps_rccif_wake_mask_b;
	u8 reg_c2k_l1_rccif_wake_mask_b;
	u8 reg_ps_c2k_rccif_wake_mask_b;
	u8 reg_l1_c2k_rccif_wake_mask_b;
	u8 reg_dqssoc_req_mask_b;
	u8 reg_disp2_req_mask_b;
	u8 reg_md_ddr_en_0_mask_b;
	u8 reg_md_ddr_en_1_mask_b;
	u8 reg_conn_ddr_en_mask_b;

	/* SPM_SRC2_MASK */
	u8 reg_disp0_req_mask_b;
	u8 reg_disp1_req_mask_b;
	u8 reg_disp_od_req_mask_b;
	u8 reg_mfg_req_mask_b;
	u8 reg_vdec0_req_mask_b;
	u8 reg_gce_vrf18_req_mask_b;
	u8 reg_gce_req_mask_b;
	u8 reg_lpdma_req_mask_b;
	u8 reg_srcclkeni1_cksel2_mask_b;
	u8 reg_conn_srcclkena_cksel2_mask_b;
	u8 reg_srcclkeni0_cksel3_mask_b;
	u8 reg_md32_apsrc_req_ddren_mask_b;
	u8 reg_scp_apsrc_req_ddren_mask_b;
	u8 reg_md_vrf18_req_0_mask_b;
	u8 reg_md_vrf18_req_1_mask_b;
	u8 reg_next_dvfs_level0_mask_b;
	u8 reg_next_dvfs_level1_mask_b;
	u8 reg_next_dvfs_level2_mask_b;
	u8 reg_next_dvfs_level3_mask_b;
	u8 reg_next_dvfs_level4_mask_b;
	u8 reg_msdc1_dvfs_halt_mask;
	u8 reg_msdc2_dvfs_halt_mask;
	u8 reg_msdc3_dvfs_halt_mask;
	u8 reg_sw2spm_int0_mask_b;
	u8 reg_sw2spm_int1_mask_b;
	u8 reg_sw2spm_int2_mask_b;
	u8 reg_sw2spm_int3_mask_b;
	u8 reg_pmcu2spm_int0_mask_b;
	u8 reg_pmcu2spm_int1_mask_b;
	u8 reg_pmcu2spm_int2_mask_b;
	u8 reg_pmcu2spm_int3_mask_b;

	/* SPM_WAKEUP_EVENT_MASK */
	u32 reg_wakeup_event_mask;

	/* SPM_EXT_WAKEUP_EVENT_MASK */
	u32 reg_ext_wakeup_event_mask;

	/* MP0_CPU0_WFI_EN */
	u8 mp0_cpu0_wfi_en;

	/* MP0_CPU1_WFI_EN */
	u8 mp0_cpu1_wfi_en;

	/* MP0_CPU2_WFI_EN */
	u8 mp0_cpu2_wfi_en;

	/* MP0_CPU3_WFI_EN */
	u8 mp0_cpu3_wfi_en;

	/* MP1_CPU0_WFI_EN */
	u8 mp1_cpu0_wfi_en;

	/* MP1_CPU1_WFI_EN */
	u8 mp1_cpu1_wfi_en;

	/* MP1_CPU2_WFI_EN */
	u8 mp1_cpu2_wfi_en;

	/* MP1_CPU3_WFI_EN */
	u8 mp1_cpu3_wfi_en;

	/* DEBUG0_WFI_EN */
	u8 debug0_wfi_en;

	/* DEBUG1_WFI_EN */
	u8 debug1_wfi_en;

	/* DEBUG2_WFI_EN */
	u8 debug2_wfi_en;

	/* DEBUG3_WFI_EN */
	u8 debug3_wfi_en;

	/* Auto-gen End */
};

/* code gen by spm_pwr_ctrl.pl, need struct pwr_ctrl */
enum pwr_ctrl_enum {
	PWR_PCM_FLAGS,
	PWR_PCM_FLAGS_CUST,
	PWR_PCM_FLAGS_CUST_SET,
	PWR_PCM_FLAGS_CUST_CLR,
	PWR_PCM_RESERVE,
	PWR_TIMER_VAL,
	PWR_TIMER_VAL_CUST,
	PWR_TIMER_VAL_RAMP_EN,
	PWR_TIMER_VAL_RAMP_EN_SEC,
	PWR_WAKE_SRC,
	PWR_WAKE_SRC_CUST,
	PWR_WAKE_SRC_MD32,
	PWR_WDT_DISABLE,
	PWR_DVFS_HALT_SRC_CHK,
	PWR_SYSPWREQ_MASK,
	PWR_REG_SRCCLKEN0_CTL,
	PWR_REG_SRCCLKEN1_CTL,
	PWR_REG_SPM_LOCK_INFRA_DCM,
	PWR_REG_SRCCLKEN_MASK,
	PWR_REG_MD1_C32RM_EN,
	PWR_REG_MD2_C32RM_EN,
	PWR_REG_CLKSQ0_SEL_CTRL,
	PWR_REG_CLKSQ1_SEL_CTRL,
	PWR_REG_SRCCLKEN0_EN,
	PWR_REG_SRCCLKEN1_EN,
	PWR_REG_SYSCLK0_SRC_MASK_B,
	PWR_REG_SYSCLK1_SRC_MASK_B,
	PWR_REG_MPWFI_OP,
	PWR_REG_MP0_CPUTOP_IDLE_MASK,
	PWR_REG_MP1_CPUTOP_IDLE_MASK,
	PWR_REG_DEBUGTOP_IDLE_MASK,
	PWR_REG_MP_TOP_IDLE_MASK,
	PWR_REG_MCUSYS_IDLE_MASK,
	PWR_REG_MD_DDR_EN_0_DBC_EN,
	PWR_REG_MD_DDR_EN_1_DBC_EN,
	PWR_REG_CONN_DDR_EN_DBC_EN,
	PWR_REG_MD32_MASK_B,
	PWR_REG_MD_0_MASK_B,
	PWR_REG_MD_1_MASK_B,
	PWR_REG_SCP_MASK_B,
	PWR_REG_SRCCLKENI0_MASK_B,
	PWR_REG_SRCCLKENI1_MASK_B,
	PWR_REG_MD_APSRC_1_SEL,
	PWR_REG_MD_APSRC_0_SEL,
	PWR_REG_CONN_MASK_B,
	PWR_REG_CONN_APSRC_SEL,
	PWR_REG_SPM_APSRC_REQ,
	PWR_REG_SPM_F26M_REQ,
	PWR_REG_SPM_INFRA_REQ,
	PWR_REG_SPM_DDREN_REQ,
	PWR_REG_SPM_VRF18_REQ,
	PWR_REG_SPM_DVFS_LEVEL0_REQ,
	PWR_REG_SPM_DVFS_LEVEL1_REQ,
	PWR_REG_SPM_DVFS_LEVEL2_REQ,
	PWR_REG_SPM_DVFS_LEVEL3_REQ,
	PWR_REG_SPM_DVFS_LEVEL4_REQ,
	PWR_REG_SPM_PMCU_MAILBOX_REQ,
	PWR_REG_SPM_SW_MAILBOX_REQ,
	PWR_REG_SPM_CKSEL2_REQ,
	PWR_REG_SPM_CKSEL3_REQ,
	PWR_REG_CSYSPWREQ_MASK,
	PWR_REG_MD_SRCCLKENA_0_INFRA_MASK_B,
	PWR_REG_MD_SRCCLKENA_1_INFRA_MASK_B,
	PWR_REG_MD_APSRC_REQ_0_INFRA_MASK_B,
	PWR_REG_MD_APSRC_REQ_1_INFRA_MASK_B,
	PWR_REG_CONN_SRCCLKENA_INFRA_MASK_B,
	PWR_REG_CONN_INFRA_REQ_MASK_B,
	PWR_REG_MD32_SRCCLKENA_INFRA_MASK_B,
	PWR_REG_MD32_INFRA_REQ_MASK_B,
	PWR_REG_SCP_SRCCLKENA_INFRA_MASK_B,
	PWR_REG_SCP_INFRA_REQ_MASK_B,
	PWR_REG_SRCCLKENI0_INFRA_MASK_B,
	PWR_REG_SRCCLKENI1_INFRA_MASK_B,
	PWR_REG_CCIF0_MD_EVENT_MASK_B,
	PWR_REG_CCIF0_AP_EVENT_MASK_B,
	PWR_REG_CCIF1_MD_EVENT_MASK_B,
	PWR_REG_CCIF1_AP_EVENT_MASK_B,
	PWR_REG_CCIF2_MD_EVENT_MASK_B,
	PWR_REG_CCIF2_AP_EVENT_MASK_B,
	PWR_REG_CCIF3_MD_EVENT_MASK_B,
	PWR_REG_CCIF3_AP_EVENT_MASK_B,
	PWR_REG_CCIFMD_MD1_EVENT_MASK_B,
	PWR_REG_CCIFMD_MD2_EVENT_MASK_B,
	PWR_REG_C2K_PS_RCCIF_WAKE_MASK_B,
	PWR_REG_C2K_L1_RCCIF_WAKE_MASK_B,
	PWR_REG_PS_C2K_RCCIF_WAKE_MASK_B,
	PWR_REG_L1_C2K_RCCIF_WAKE_MASK_B,
	PWR_REG_DQSSOC_REQ_MASK_B,
	PWR_REG_DISP2_REQ_MASK_B,
	PWR_REG_MD_DDR_EN_0_MASK_B,
	PWR_REG_MD_DDR_EN_1_MASK_B,
	PWR_REG_CONN_DDR_EN_MASK_B,
	PWR_REG_DISP0_REQ_MASK_B,
	PWR_REG_DISP1_REQ_MASK_B,
	PWR_REG_DISP_OD_REQ_MASK_B,
	PWR_REG_MFG_REQ_MASK_B,
	PWR_REG_VDEC0_REQ_MASK_B,
	PWR_REG_GCE_VRF18_REQ_MASK_B,
	PWR_REG_GCE_REQ_MASK_B,
	PWR_REG_LPDMA_REQ_MASK_B,
	PWR_REG_SRCCLKENI1_CKSEL2_MASK_B,
	PWR_REG_CONN_SRCCLKENA_CKSEL2_MASK_B,
	PWR_REG_SRCCLKENI0_CKSEL3_MASK_B,
	PWR_REG_MD32_APSRC_REQ_DDREN_MASK_B,
	PWR_REG_SCP_APSRC_REQ_DDREN_MASK_B,
	PWR_REG_MD_VRF18_REQ_0_MASK_B,
	PWR_REG_MD_VRF18_REQ_1_MASK_B,
	PWR_REG_NEXT_DVFS_LEVEL0_MASK_B,
	PWR_REG_NEXT_DVFS_LEVEL1_MASK_B,
	PWR_REG_NEXT_DVFS_LEVEL2_MASK_B,
	PWR_REG_NEXT_DVFS_LEVEL3_MASK_B,
	PWR_REG_NEXT_DVFS_LEVEL4_MASK_B,
	PWR_REG_MSDC1_DVFS_HALT_MASK,
	PWR_REG_MSDC2_DVFS_HALT_MASK,
	PWR_REG_MSDC3_DVFS_HALT_MASK,
	PWR_REG_SW2SPM_INT0_MASK_B,
	PWR_REG_SW2SPM_INT1_MASK_B,
	PWR_REG_SW2SPM_INT2_MASK_B,
	PWR_REG_SW2SPM_INT3_MASK_B,
	PWR_REG_PMCU2SPM_INT0_MASK_B,
	PWR_REG_PMCU2SPM_INT1_MASK_B,
	PWR_REG_PMCU2SPM_INT2_MASK_B,
	PWR_REG_PMCU2SPM_INT3_MASK_B,
	PWR_REG_WAKEUP_EVENT_MASK,
	PWR_REG_EXT_WAKEUP_EVENT_MASK,
	PWR_MP0_CPU0_WFI_EN,
	PWR_MP0_CPU1_WFI_EN,
	PWR_MP0_CPU2_WFI_EN,
	PWR_MP0_CPU3_WFI_EN,
	PWR_MP1_CPU0_WFI_EN,
	PWR_MP1_CPU1_WFI_EN,
	PWR_MP1_CPU2_WFI_EN,
	PWR_MP1_CPU3_WFI_EN,
	PWR_DEBUG0_WFI_EN,
	PWR_DEBUG1_WFI_EN,
	PWR_DEBUG2_WFI_EN,
	PWR_DEBUG3_WFI_EN,
	PWR_MAX_COUNT,
};

enum {
	SPM_SUSPEND,
	SPM_RESUME,
	SPM_PWR_CTRL_SUSPEND,
	SPM_PWR_CTRL_DPIDLE,
	SPM_PWR_CTRL_SODI,
	SPM_PWR_CTRL_SODI3,
	SPM_PWR_CTRL_MSDC,
	SPM_DDR_SYNC,
	SPM_REGISTER_INIT,
	SPM_DPD_WRITE,
	SPM_IRQ0_HANDLER,
	SPM_AP_MDSRC_REQ,
	SPM_DPIDLE_ENTER,
	SPM_DPIDLE_LEAVE,
	SPM_ENTER_SODI,
	SPM_LEAVE_SODI,
	SPM_VCORE_DVFS_ENTER,
	SPM_VCORE_PWARP_CMD,
	SPM_VCORE_DVFS_FWINIT,
	SPM_TIMESYNC,
};

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT

struct spm_data {
	unsigned int cmd;
	union {
		struct {
			unsigned int cpu;
			unsigned int pcm_flags;
			unsigned int pcm_reserve;
			unsigned int timer_val;
			unsigned int spm_pcmwdt_en;
			unsigned int sys_timestamp_l;
			unsigned int sys_timestamp_h;
			unsigned int sys_src_clk_l;
			unsigned int sys_src_clk_h;
			unsigned int sleep_dpidle;
		} suspend;
		struct {
			unsigned int idx;
			unsigned int val;
		} pwr_ctrl;
		struct {
			unsigned int arg0;
			unsigned int arg1;
			unsigned int arg2;
			unsigned int arg3;
			unsigned int arg4;
			unsigned int arg5;
			unsigned int arg6;
			unsigned int arg7;
			unsigned int arg8;
			unsigned int arg9;
		} args;
		struct {
			unsigned int cpu;
			unsigned int pcm_flags;
		} sodi;
		struct {
			unsigned int pcm_flags;
		} vcorefs;
	} u;
};

#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */

/* FIXME: fix device tree reseverd memory, should be 0x4000 */
#define PCM_FIRMWARE_SIZE   0x2000
#define DYNA_LOAD_PCM_PATH_SIZE 128
#define PCM_FIRMWARE_VERSION_SIZE 128

enum dyna_load_pcm_index {
	DYNA_LOAD_PCM_SUSPEND = 0,
	DYNA_LOAD_PCM_SODI,
	DYNA_LOAD_PCM_DEEPIDLE,
	DYNA_LOAD_PCM_VCOREFS,
	DYNA_LOAD_PCM_MAX,
};

struct dyna_load_pcm_t {
	char path[DYNA_LOAD_PCM_PATH_SIZE];
	char version[PCM_FIRMWARE_VERSION_SIZE];
	char *buf;
	dma_addr_t buf_dma;
	struct pcm_desc desc;
	int ready;
};

extern struct dyna_load_pcm_t dyna_load_pcm[DYNA_LOAD_PCM_MAX];

struct wake_status {
	u32 assert_pc;		/* PCM_REG_DATA_INI */
	u32 r12;		/* PCM_REG12_DATA */
	u32 r12_ext;		/* PCM_REG12_DATA */
	u32 raw_sta;		/* SLEEP_ISR_RAW_STA */
	u32 raw_ext_sta;	/* SPM_WAKEUP_EXT_STA */
	u32 wake_misc;		/* SLEEP_WAKEUP_MISC */
	u32 timer_out;		/* PCM_TIMER_OUT */
	u32 r13;		/* PCM_REG13_DATA */
	u32 idle_sta;		/* SLEEP_SUBSYS_IDLE_STA */
	u32 debug_flag;		/* PCM_PASR_DPD_3 */
	u32 event_reg;		/* PCM_EVENT_REG_STA */
	u32 isr;		/* SLEEP_ISR_STATUS */
	u32 log_index;
};

struct spm_lp_scen {
	struct pcm_desc *pcmdesc;
	struct pwr_ctrl *pwrctrl;
	struct wake_status *wakestatus;
};

extern spinlock_t __spm_lock;

extern struct spm_lp_scen __spm_suspend;
extern struct spm_lp_scen __spm_dpidle;
extern struct spm_lp_scen __spm_sodi3;
extern struct spm_lp_scen __spm_sodi;
extern struct spm_lp_scen __spm_mcdi;
extern struct spm_lp_scen __spm_vcore_dvfs;

extern void __spm_set_cpu_status(int cpu);
extern void __spm_reset_and_init_pcm(const struct pcm_desc *pcmdesc);
extern void __spm_kick_im_to_fetch(const struct pcm_desc *pcmdesc);

extern void __spm_init_pcm_register(void);	/* init r0 and r7 */
extern void __spm_init_event_vector(const struct pcm_desc *pcmdesc);
extern void __spm_set_power_control(const struct pwr_ctrl *pwrctrl);
extern void __spm_set_wakeup_event(const struct pwr_ctrl *pwrctrl);
extern void __spm_kick_pcm_to_run(struct pwr_ctrl *pwrctrl);

extern void __spm_get_wakeup_status(struct wake_status *wakesta);
extern void __spm_clean_after_wakeup(void);
extern wake_reason_t __spm_output_wake_reason(const struct wake_status *wakesta,
					      const struct pcm_desc *pcmdesc, bool suspend);

/* sync with vcore_dvfs related pwr_ctrl */
extern void __spm_sync_vcore_dvfs_power_control(struct pwr_ctrl *dest_pwr_ctrl, const struct pwr_ctrl *src_pwr_ctrl);

extern int spm_fs_init(void);
/* extern int is_ext_buck_exist(void); */

/* check dvfs halt source by mask-off test */
int __check_dvfs_halt_source(int enable);

extern int spm_golden_setting_cmp(bool en);
extern bool is_md_c2k_conn_power_off(void);
extern void __spm_set_pcm_wdt(int en);
extern u32 _spm_get_wake_period(int pwake_time, wake_reason_t last_wr);

/**************************************
 * Macro and Inline
 **************************************/
#define EVENT_VEC(event, resume, imme, pc)	\
	(((pc) << 16) |				\
	 (!!(imme) << 7) |			\
	 (!!(resume) << 6) |			\
	 ((event) & 0x3f))

#define spm_emerg(fmt, args...)		pr_emerg("[SPM] " fmt, ##args)
#define spm_alert(fmt, args...)		pr_alert("[SPM] " fmt, ##args)
#define spm_crit(fmt, args...)		pr_crit("[SPM] " fmt, ##args)
#define spm_err(fmt, args...)		pr_err("[SPM] " fmt, ##args)
#define spm_warn(fmt, args...)		pr_warn("[SPM] " fmt, ##args)
#define spm_notice(fmt, args...)	pr_notice("[SPM] " fmt, ##args)
#define spm_info(fmt, args...)		pr_info("[SPM] " fmt, ##args)
#define spm_debug(fmt, args...)		pr_info("[SPM] " fmt, ##args)	/* pr_debug show nothing */

/* just use in suspend flow for important log due to console suspend */
#define spm_crit2(fmt, args...)		\
do {					\
	aee_sram_printk(fmt, ##args);	\
	spm_crit(fmt, ##args);		\
} while (0)

#define wfi_with_sync()					\
do {							\
	isb();						\
	mb();						\
	__asm__ __volatile__("wfi" : : : "memory");	\
} while (0)

bool __attribute__((weak)) is_clk_buf_under_flightmode(void)
{
	return false;
}

static inline u32 base_va_to_pa(const u32 *base)
{
	phys_addr_t pa = virt_to_phys(base);

	MAPPING_DRAM_ACCESS_ADDR(pa);	/* for 4GB mode */
	return (u32) pa;
}

static inline void update_pwrctrl_pcm_flags(u32 *flags)
{
	/* SPM controls NFC clock buffer in RF only */
	if (!is_clk_buf_from_pmic() && is_clk_buf_under_flightmode())
		(*flags) |= SPM_FLAG_EN_NFC_CLOCK_BUF_CTRL;
}

static inline void set_pwrctrl_pcm_flags(struct pwr_ctrl *pwrctrl, u32 flags)
{
	if (pwrctrl->pcm_flags_cust == 0)
		pwrctrl->pcm_flags = flags;
	else
		pwrctrl->pcm_flags = pwrctrl->pcm_flags_cust;
}

static inline void set_pwrctrl_pcm_data(struct pwr_ctrl *pwrctrl, u32 data)
{
	pwrctrl->pcm_reserve = data;
}

#endif
