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

#ifndef _MT_SPM_INTERNAL_
#define _MT_SPM_INTERNAL_

#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/atomic.h>
#include <linux/io.h>
#include <mt-plat/aee.h>
#include <mt-plat/mt_chip.h>

#include "mt_clkbuf_ctl.h"
#include "mt_spm.h"
#include "mt_lpae.h"
#include "mt_gpio.h"

/**************************************
 * Config and Parameter
 **************************************/
#ifdef MTK_FORCE_CLUSTER1
#define SPM_CTRL_BIG_CPU	1
#else
#define SPM_CTRL_BIG_CPU	0
#endif

#define POWER_ON_VAL1_DEF	0x00015820
#define PCM_FSM_STA_DEF		0x00048490
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

#define CC_SYSCLK1_EN_0		(1U << 2)
#define CC_SYSCLK1_EN_1		(1U << 3)
#define CC_SRCLKENA_MASK_0	(1U << 6)
#define CC_SRCLKENA_MASK_1	(1U << 7)
#define CC_SRCLKENA_MASK_2	(1U << 8)
#define CC_SYSCLK1_SRC_MASK_B_MD2_SRCCLKENA	(1U << 27)

#define WFI_OP_AND		1
#define WFI_OP_OR		0
#define SEL_MD_DDR_EN		1
#define SEL_MD_APSRC_REQ	0

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

#define ISRM_RET_IRQ_AUX	(ISRM_RET_IRQ9 | ISRM_RET_IRQ8 | \
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

#define WAKE_MISC_TWAM		(1U << 18)
#define WAKE_MISC_PCM_TIMER	(1U << 19)
#define WAKE_MISC_CPU_WAKE	(1U << 20)

#if defined(CONFIG_OF)
extern void __iomem *scp_i2c3_base;
#endif
#include <linux/platform_device.h>
extern struct platform_device *pspmdev;
extern struct clk *i2c3_clk_main;
#if !defined(CONFIG_MTK_CLKMGR)
#include <linux/clk.h>
#else
#include <mach/mt_clkmgr.h>
#endif

#ifndef CONFIG_MTK_FPGA
#if defined(CONFIG_ARCH_MT6797)
#define SPM_VCORE_EN_MT6797
#endif
#endif

#ifndef CONFIG_MTK_FPGA
#if defined(CONFIG_ARCH_MT6755)
#define SPM_VCORE_EN_MT6755
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

#if defined(CONFIG_ARCH_MT6757)
struct pwr_ctrl {
	u32 pcm_flags;
	u32 pcm_flags_cust;	/* can override pcm_flags */
	u32 pcm_reserve;
	u32 timer_val;		/* @ 1T 32K */
	u32 timer_val_cust;	/* @ 1T 32K, can override timer_val */
	u32 timer_val_ramp_en;
	u32 timer_val_ramp_en_sec;
	u32 wake_src;
	u32 wake_src_cust;	/* can override wake_src */
	u32 wake_src_md32;
	u8 r0_ctrl_en;
	u8 r7_ctrl_en;
	u8 infra_dcm_lock;
	u8 wdt_disable;
	u8 dvfs_halt_src_chk;

	/* SPM_AP_STANDBY_CON */
	u8 wfi_op;
	u8 mp0_cputop_idle_mask;
	u8 mp1_cputop_idle_mask;
	u8 mcusys_idle_mask;
	u8 mm_mask_b;
	u8 md_ddr_en_dbc_en;
	u8 md_mask_b;
	u8 scp_mask_b;
	u8 lte_mask_b;
	u8 srcclkeni_mask_b;
	u8 md_apsrc_1_sel;
	u8 md_apsrc_0_sel;
	u8 conn_mask_b;
	u8 conn_apsrc_sel;

	/* SPM_SRC_REQ */
	u8 spm_apsrc_req;
	u8 spm_f26m_req;
	u8 spm_lte_req;
	u8 spm_infra_req;
	u8 spm_vrf18_req;
	u8 spm_dvfs_req;
	u8 spm_dvfs_force_down;
	u8 spm_ddren_req;
	u8 spm_rsv_src_req;
	u8 cpu_md_dvfs_sop_force_on;

	/* SPM_SRC_MASK */
	u8 csyspwreq_mask;
	u8 ccif0_md_event_mask_b;
	u8 ccif0_ap_event_mask_b;
	u8 ccif1_md_event_mask_b;
	u8 ccif1_ap_event_mask_b;
	u8 ccifmd_md1_event_mask_b;
	u8 ccifmd_md2_event_mask_b;
	u8 dsi0_vsync_mask_b;
	u8 dsi1_vsync_mask_b;
	u8 dpi_vsync_mask_b;
	u8 isp0_vsync_mask_b;
	u8 isp1_vsync_mask_b;
	u8 md_srcclkena_0_infra_mask_b;
	u8 md_srcclkena_1_infra_mask_b;
	u8 conn_srcclkena_infra_mask_b;
	u8 md32_srcclkena_infra_mask_b;
	u8 srcclkeni_infra_mask_b;
	u8 md_apsrc_req_0_infra_mask_b;
	u8 md_apsrc_req_1_infra_mask_b;
	u8 conn_apsrcreq_infra_mask_b;
	u8 md32_apsrcreq_infra_mask_b;
	u8 md_ddr_en_0_mask_b;
	u8 md_ddr_en_1_mask_b;
	u8 md_vrf18_req_0_mask_b;
	u8 md_vrf18_req_1_mask_b;
	u8 md1_dvfs_req_mask;
	u8 cpu_dvfs_req_mask;
	u8 emi_bw_dvfs_req_mask;
	u8 md_srcclkena_0_dvfs_req_mask_b;
	u8 md_srcclkena_1_dvfs_req_mask_b;
	u8 conn_srcclkena_dvfs_req_mask_b;

	/* SPM_SRC2_MASK */
	u8 dvfs_halt_mask_b;
	u8 vdec_req_mask_b;
	u8 gce_req_mask_b;
	u8 cpu_md_dvfs_req_merge_mask_b;
	u8 md_ddr_en_dvfs_halt_mask_b;
	u8 dsi0_vsync_dvfs_halt_mask_b;
	u8 dsi1_vsync_dvfs_halt_mask_b;
	u8 dpi_vsync_dvfs_halt_mask_b;
	u8 isp0_vsync_dvfs_halt_mask_b;
	u8 isp1_vsync_dvfs_halt_mask_b;
	u8 conn_ddr_en_mask_b;
	u8 disp_req_mask_b;
	u8 disp1_req_mask_b;
	u8 mfg_req_mask_b;
	u8 c2k_ps_rccif_wake_mask_b;
	u8 c2k_l1_rccif_wake_mask_b;
	u8 ps_c2k_rccif_wake_mask_b;
	u8 l1_c2k_rccif_wake_mask_b;
	u8 sdio_on_dvfs_req_mask_b;
	u8 emi_boost_dvfs_req_mask_b;
	u8 cpu_md_emi_dvfs_req_prot_dis;
	u8 dramc_spcmd_apsrc_req_mask_b;

	/* SW_CRTL_EVENT */
	u8 sw_ctrl_event_on;

	/* SPM_SW_RSV_6 */
	u8 md_srcclkena_0_2d_dvfs_req_mask_b;
	u8 md_srcclkena_1_2d_dvfs_req_mask_b;
	u8 dvfs_up_2d_dvfs_req_mask_b;
	u8 disable_off_load_lpm;
#if 0
	/* SPM_WAKEUP_EVENT_MASK */
	u32 spm_wakeup_event_mask;

	/* SPM_WAKEUP_EVENT_EXT_MASK */
	u32 spm_wakeup_event_ext_mask;
#endif

	u8 srclkenai_mask;

	u8 mp1_cpu0_wfi_en;
	u8 mp1_cpu1_wfi_en;
	u8 mp1_cpu2_wfi_en;
	u8 mp1_cpu3_wfi_en;
	u8 mp0_cpu0_wfi_en;
	u8 mp0_cpu1_wfi_en;
	u8 mp0_cpu2_wfi_en;
	u8 mp0_cpu3_wfi_en;

	u32 param1;
	u32 param2;
	u32 param3;
};
#else
struct pwr_ctrl {
	/* for SPM */
	u32 pcm_flags;
	u32 pcm_flags_cust;	/* can override pcm_flags */
	u32 pcm_reserve;
	u32 timer_val;		/* @ 1T 32K */
	u32 timer_val_cust;	/* @ 1T 32K, can override timer_val */
	u32 timer_val_ramp_en;
	u32 timer_val_ramp_en_sec;
	u32 wake_src;
	u32 wake_src_cust;	/* can override wake_src */
	u32 wake_src_md32;
	u8 r0_ctrl_en;
	u8 r7_ctrl_en;
	u8 infra_dcm_lock;
	u8 wdt_disable;
	u8 dvfs_halt_src_chk;
	u8 spm_apsrc_req;
	u8 spm_f26m_req;
	u8 spm_lte_req;
	u8 spm_infra_req;
	u8 spm_vrf18_req;
	u8 spm_dvfs_req;
	u8 spm_dvfs_force_down;
	u8 spm_ddren_req;
	u8 spm_flag_keep_csyspwrupack_high;
	u8 spm_flag_dis_vproc_vsram_dvs;
	u8 spm_flag_run_common_scenario;
	u8 cpu_md_dvfs_sop_force_on;

	/* for AP */
	u8 mcusys_idle_mask;
	u8 mp1top_idle_mask;
	u8 mp0top_idle_mask;
#if defined(CONFIG_ARCH_MT6797)
	u8 mp2top_idle_mask;
	u8 mp3top_idle_mask;
	u8 mptop_idle_mask;
#endif
	u8 wfi_op;		/* 1:WFI_OP_AND, 0:WFI_OP_OR */
#if defined(CONFIG_ARCH_MT6797)
	u8 mp2_cpu0_wfi_en;
	u8 mp2_cpu1_wfi_en;
#endif
	u8 mp1_cpu0_wfi_en;
	u8 mp1_cpu1_wfi_en;
	u8 mp1_cpu2_wfi_en;
	u8 mp1_cpu3_wfi_en;
	u8 mp0_cpu0_wfi_en;
	u8 mp0_cpu1_wfi_en;
	u8 mp0_cpu2_wfi_en;
	u8 mp0_cpu3_wfi_en;

	/* for MD */
	u8 md1_req_mask_b;
	u8 md2_req_mask_b;
	u8 md_apsrc0_sel;	/* 1:SEL_MD_DDR_EN, 0:SEL_MD_APSRC_REQ */
	u8 md_apsrc1_sel;	/* 1:SEL_MD2_DDR_EN, 0:SEL_MD2_APSRC_REQ */
	u8 md_ddr_dbc_en;
	u8 ccif0_to_ap_mask_b;
	u8 ccif0_to_md_mask_b;
	u8 ccif1_to_ap_mask_b;
	u8 ccif1_to_md_mask_b;
	u8 lte_mask_b;
	u8 ccifmd_md1_event_mask_b;
	u8 ccifmd_md2_event_mask_b;
	u8 vsync_mask_b;	/* 5bit */
	u8 md_srcclkena_0_infra_mask_b;
	u8 md_srcclkena_1_infra_mask_b;
	u8 conn_srcclkena_infra_mask_b;
	u8 md32_srcclkena_infra_mask_b;
	u8 srcclkeni_infra_mask_b;
	u8 md_apsrcreq_0_infra_mask_b;
	u8 md_apsrcreq_1_infra_mask_b;
	u8 conn_apsrcreq_infra_mask_b;
	u8 md32_apsrcreq_infra_mask_b;
	u8 md_ddr_en_0_mask_b;
	u8 md_ddr_en_1_mask_b;
	u8 md_vrf18_req_0_mask_b;
	u8 md_vrf18_req_1_mask_b;
#if defined(CONFIG_ARCH_MT6797)
	u8 md1_dvfs_req_mask;
	u8 cpu_dvfs_req_mask;
#endif
	u8 emi_bw_dvfs_req_mask;
	u8 md_srcclkena_0_dvfs_req_mask_b;
	u8 md_srcclkena_1_dvfs_req_mask_b;
	u8 conn_srcclkena_dvfs_req_mask_b;

	u8 dvfs_halt_mask_b;	/* 5bit */
	u8 vdec_req_mask_b;
	u8 gce_req_mask_b;
	u8 cpu_md_dvfs_erq_merge_mask_b;
	u8 md1_ddr_en_dvfs_halt_mask_b;
	u8 md2_ddr_en_dvfs_halt_mask_b;
	u8 vsync_dvfs_halt_mask_b;	/* 5bit */
	u8 conn_ddr_en_mask_b;
	u8 disp_req_mask_b;
	u8 disp1_req_mask_b;
	u8 mfg_req_mask_b;
	u8 c2k_ps_rccif_wake_mask_b;
	u8 c2k_l1_rccif_wake_mask_b;
	u8 ps_c2k_rccif_wake_mask_b;
	u8 l1_c2k_rccif_wake_mask_b;
	u8 sdio_on_dvfs_req_mask_b;
	u8 emi_boost_dvfs_req_mask_b;
	u8 cpu_md_emi_dvfs_req_prot_dis;
#if defined(CONFIG_ARCH_MT6797)
	u8 disp_od_req_mask_b;
#endif

	/* for CONN */
	u8 conn_mask_b;
	u8 conn_apsrc_sel;

	/* for MM */
	u8 dsi0_ddr_en_mask_b;	/* E2 */
	u8 dsi1_ddr_en_mask_b;	/* E2 */
	u8 dpi_ddr_en_mask_b;	/* E2 */
	u8 isp0_ddr_en_mask_b;	/* E2 */
	u8 isp1_ddr_en_mask_b;	/* E2 */

	/* for other SYS */
	u8 scp_req_mask_b;
	u8 syspwreq_mask;	/* make 26M off when attach ICE */
	u8 srclkenai_mask;

	/* for scenario */
	u32 param1;
	u32 param2;
	u32 param3;
};
#endif

#if defined(CONFIG_ARCH_MT6757)
#define PCM_FIRMWARE_SIZE   0x4000 /* 16KB */
#else
#define PCM_FIRMWARE_SIZE   0x2000
#endif
#define DYNA_LOAD_PCM_PATH_SIZE 128
#define PCM_FIRMWARE_VERSION_SIZE 128

#if defined(CONFIG_ARCH_MT6757)
enum dyna_load_pcm_index {
	DYNA_LOAD_PCM_SUSPEND = 0,
	DYNA_LOAD_PCM_SUSPEND_BY_MP1,
	DYNA_LOAD_PCM_SUSPEND_LPDDR4,
	DYNA_LOAD_PCM_SUSPEND_LPDDR4_BY_MP1,
	DYNA_LOAD_PCM_SODI,
	DYNA_LOAD_PCM_SODI_BY_MP1,
	DYNA_LOAD_PCM_SODI_LPDDR4,
	DYNA_LOAD_PCM_SODI_LPDDR4_BY_MP1,
	DYNA_LOAD_PCM_DEEPIDLE,
	DYNA_LOAD_PCM_DEEPIDLE_BY_MP1,
	DYNA_LOAD_PCM_DEEPIDLE_LPDDR4,
	DYNA_LOAD_PCM_DEEPIDLE_LPDDR4_BY_MP1,
	DYNA_LOAD_PCM_MAX,
};
#else
enum dyna_load_pcm_index {
	DYNA_LOAD_PCM_SUSPEND = 0,
	DYNA_LOAD_PCM_SUSPEND_BY_MP1,
#if defined(CONFIG_ARCH_MT6797)
	DYNA_LOAD_PCM_VCOREFS_LPM,
	DYNA_LOAD_PCM_VCOREFS_HPM,
	DYNA_LOAD_PCM_VCOREFS_ULTRA,
#endif
	DYNA_LOAD_PCM_SODI,
	DYNA_LOAD_PCM_SODI_BY_MP1,

	DYNA_LOAD_PCM_DEEPIDLE,
	DYNA_LOAD_PCM_DEEPIDLE_BY_MP1,
	DYNA_LOAD_PCM_MAX,
};
#endif

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
	u32 wake_misc;		/* SLEEP_WAKEUP_MISC */
	u32 raw_ext_sta;	/* SPM_WAKEUP_EXT_STA */
	u32 timer_out;		/* PCM_TIMER_OUT */
	u32 r13;		/* PCM_REG13_DATA */
	u32 idle_sta;		/* SLEEP_SUBSYS_IDLE_STA */
	u32 debug_flag;		/* PCM_PASR_DPD_3 */
	u32 event_reg;		/* PCM_EVENT_REG_STA */
	u32 isr;		/* SLEEP_ISR_STATUS */
	u32 r9;			/* PCM_REG9_DATA */
	u32 log_index;
};

struct spm_lp_scen {
	struct pcm_desc *pcmdesc;
	struct pwr_ctrl *pwrctrl;
	struct wake_status *wakestatus;
};

extern spinlock_t __spm_lock;
extern atomic_t __spm_mainpll_req;

extern struct spm_lp_scen __spm_suspend;
extern struct spm_lp_scen __spm_dpidle;
extern struct spm_lp_scen __spm_sodi3;
extern struct spm_lp_scen __spm_sodi;
extern struct spm_lp_scen __spm_mcdi;
extern struct spm_lp_scen __spm_talking;
extern struct spm_lp_scen __spm_ddrdfs;
extern struct spm_lp_scen __spm_vcore_dvfs;

extern void __spm_reset_and_init_pcm(const struct pcm_desc *pcmdesc);
extern void __spm_kick_im_to_fetch(const struct pcm_desc *pcmdesc);

extern void __spm_init_pcm_register(void);	/* init r0 and r7 */
extern void __spm_init_event_vector(const struct pcm_desc *pcmdesc);
extern void __spm_set_power_control(const struct pwr_ctrl *pwrctrl);
extern void __spm_set_vcorefs_wakeup_event(const struct pwr_ctrl *src_pwr_ctrl);
extern void __spm_set_wakeup_event(const struct pwr_ctrl *pwrctrl);
extern void __spm_kick_pcm_to_run(const struct pwr_ctrl *pwrctrl);

extern void __spm_get_wakeup_status(struct wake_status *wakesta);
extern void __spm_clean_after_wakeup(void);
extern wake_reason_t __spm_output_wake_reason(const struct wake_status *wakesta,
					      const struct pcm_desc *pcmdesc, bool suspend);

extern void __spm_dbgout_md_ddr_en(bool enable);

extern void __spm_check_md_pdn_power_control(struct pwr_ctrl *pwr_ctrl);

#if defined(CONFIG_ARCH_MT6797)
bool is_vcorefs_fw(bool dynamic_load);
#endif
void __spm_backup_vcore_dvfs_dram_shuffle(void);
/* sync with vcore_dvfs related pwr_ctrl */
extern void __spm_sync_vcore_dvfs_power_control(struct pwr_ctrl *dest_pwr_ctrl, const struct pwr_ctrl *src_pwr_ctrl);

/* set vcore dummy read address */
void spm_set_dummy_read_addr(void);

extern int spm_fs_init(void);
/* extern int is_ext_buck_exist(void); */

/* check dvfs halt source by mask-off test */
int __check_dvfs_halt_source(int enable);

/*
 * if in talking, modify @spm_flags based on @lpscen and return __spm_talking,
 * otherwise, do nothing and return @lpscen
 */
extern struct spm_lp_scen *spm_check_talking_get_lpscen(struct spm_lp_scen *lpscen,
							u32 *spm_flags);

extern int spm_golden_setting_cmp(bool en);
extern void spm_get_twam_table(const char ***table);
extern bool is_md_c2k_conn_power_off(void);
extern void __spm_backup_pmic_ck_pdn(void);
extern void __spm_restore_pmic_ck_pdn(void);
extern void __spm_bsi_top_init_setting(void);
extern void __spm_pmic_pg_force_on(void);
extern void __spm_pmic_pg_force_off(void);
extern void __spm_pmic_low_iq_mode(int en);
extern void __spm_set_pcm_wdt(int en);
extern u32 _spm_get_wake_period(int pwake_time, wake_reason_t last_wr);
extern struct dram_info *g_dram_info_dummy_read;

#if defined(CONFIG_ARCH_MT6797)
extern u32 spm_get_pcm_vcorefs_index(void);
#endif

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
#if defined(CONFIG_ARCH_MT6755)
	if (is_clk_buf_from_pmic())
		(*flags) |= SPM_FLAG_IS_COTSX;
#endif
}

static inline void set_pwrctrl_pcm_flags(struct pwr_ctrl *pwrctrl, u32 flags)
{
#if defined(CONFIG_ARCH_MT6797)
	int segment_code = mt_get_chip_hw_ver();
#endif

	if (pwrctrl->pcm_flags_cust == 0)
		pwrctrl->pcm_flags = flags;
	else
		pwrctrl->pcm_flags = pwrctrl->pcm_flags_cust;

#if defined(CONFIG_ARCH_MT6797)
	if (0xCA01 == segment_code) {
		pwrctrl->pcm_flags |= SPM_FLAG_EN_SEGMENT2;
	} else {
		pwrctrl->pcm_flags &= ~SPM_FLAG_EN_SEGMENT2;
	}
#endif
}

static inline void set_pwrctrl_pcm_data(struct pwr_ctrl *pwrctrl, u32 data)
{
	pwrctrl->pcm_reserve = data;
}

#if 0
static inline void set_flags_for_mainpll(u32 *flags)
{
	if (atomic_read(&__spm_mainpll_req) != 0)
		*flags |= SPM_MAINPLL_PDN_DIS;
	else
		*flags &= ~SPM_MAINPLL_PDN_DIS;
}
#endif

#endif
