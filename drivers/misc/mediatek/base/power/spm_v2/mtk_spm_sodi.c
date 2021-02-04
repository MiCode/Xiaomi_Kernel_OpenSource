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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/slab.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif

#include <mach/mtk_gpt.h>
#include <mt-plat/mtk_secure_api.h>

#include <mt-plat/mtk_boot.h>
#if defined(CONFIG_MTK_SYS_CIRQ)
#include <mt-plat/mtk_cirq.h>
#endif
#include <mt-plat/upmu_common.h>
#include <mt-plat/mtk_io.h>

#include <mtk_spm_sodi.h>
#include <mtk_idle_profile.h>


/**************************************
 * only for internal debug
 **************************************/
#define LOG_BUF_SIZE                    (256)
#define SODI_LOGOUT_TIMEOUT_CRITERIA    (20)
#define SODI_LOGOUT_MAXTIME_CRITERIA    (2000)
#define SODI_LOGOUT_FREQUENT_CRITERIA   (20U)   /* unit:ms */
#define SODI_LOGOUT_INTERVAL_CRITERIA   (5000U)	/* unit:ms */

#if defined(CONFIG_OF)
#define MCUCFG_NODE "mediatek,MCUCFG"
static unsigned long mcucfg_base;
static unsigned long mcucfg_phys_base;
#undef MCUCFG_BASE
#define MCUCFG_BASE (mcucfg_base)

#define M4U_NODE "mediatek,M4U"
static unsigned long m4u_base;
static unsigned long m4u_phys_base;
#undef M4U_BASE
#define M4U_BASE (m4u_base)

#else /* #if defined (CONFIG_OF) */
#undef MCUCFG_BASE
#define MCUCFG_BASE 0xF0200000 /* 0x1020_0000 */

#undef M4U_BASE
#define M4U_BASE 0xF0205000 /* 0x1020_5000 */
#endif /* #if defined (CONFIG_OF) */

/* MCUCFG registers */
#define MP0_AXI_CONFIG		(MCUCFG_BASE + 0x2C)
#define MP0_AXI_CONFIG_PHYS	(mcucfg_phys_base + 0x2C)
#define MP1_AXI_CONFIG		(MCUCFG_BASE + 0x22C)
#define MP1_AXI_CONFIG_PHYS	(mcucfg_phys_base + 0x22C)
#define ACINACTM		(1 << 4)

/* M4U registers */
#define MMU_SMI_ASYNC_CFG	(M4U_BASE + 0xB80)
#define MMU_SMI_ASYNC_CFG_PHYS	(m4u_phys_base + 0xB80)
#define SMI_COMMON_ASYNC_DCM	(0x3 << 14)

unsigned int __attribute__((weak)) pmic_read_interface_nolock(unsigned int RegNum,
					unsigned int *val, unsigned int MASK, unsigned int SHIFT)
{
	return -1;
}

#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
static struct pwr_ctrl sodi_ctrl = {
	.wake_src			= WAKE_SRC_FOR_SODI,
	.wake_src_md32		= WAKE_SRC_FOR_MD32,
	.r0_ctrl_en			= 1,
	.r7_ctrl_en			= 1,
	.infra_dcm_lock		= 1,
	.wfi_op				= WFI_OP_AND,

	/* SPM_AP_STANDBY_CON */
	.mp0_cputop_idle_mask = 0,
	.mp1_cputop_idle_mask = 0,
	.mcusys_idle_mask = 0,
	.md_ddr_en_dbc_en = 0,
	.md_mask_b = 1,
	.scp_mask_b = 0,
	.lte_mask_b = 0,
	.md_apsrc_1_sel = 0,
	.md_apsrc_0_sel = 0,
	.conn_mask_b = 1,
	.conn_apsrc_sel = 0,

	/* SPM_SRC_REQ */
	.spm_apsrc_req = 0,
	.spm_f26m_req = 0,
	.spm_lte_req = 0,
	.spm_infra_req = 0,
	.spm_vrf18_req = 0,
	.spm_dvfs_req = 0,
	.spm_dvfs_force_down = 0,
	.spm_ddren_req = 0,
	.cpu_md_dvfs_sop_force_on = 0,

	/* SPM_SRC_MASK */
	.ccif0_md_event_mask_b = 1,
	.ccif0_ap_event_mask_b = 1,
	.ccif1_md_event_mask_b = 1,
	.ccif1_ap_event_mask_b = 1,
	.ccifmd_md1_event_mask_b = 1,
	.ccifmd_md2_event_mask_b = 1,
	.dsi0_vsync_mask_b = 0,
	.dsi1_vsync_mask_b = 0,
	.dpi_vsync_mask_b = 0,
	.isp0_vsync_mask_b = 0,
	.isp1_vsync_mask_b = 0,
	.md_srcclkena_0_infra_mask_b = 0,
	.md_srcclkena_1_infra_mask_b = 0,
	.conn_srcclkena_infra_mask_b = 0,
	.md32_srcclkena_infra_mask_b = 0,
	.srcclkeni_infra_mask_b = 0,
	.md_apsrc_req_0_infra_mask_b = 1,
	.md_apsrc_req_1_infra_mask_b = 0,
	.conn_apsrcreq_infra_mask_b = 1,
	.md32_apsrcreq_infra_mask_b = 0,
	.md_ddr_en_0_mask_b = 1,
	.md_ddr_en_1_mask_b = 0,
	.md_vrf18_req_0_mask_b = 1,
	.md_vrf18_req_1_mask_b = 0,
	.emi_bw_dvfs_req_mask = 1,
	.md_srcclkena_0_dvfs_req_mask_b = 0,
	.md_srcclkena_1_dvfs_req_mask_b = 0,
	.conn_srcclkena_dvfs_req_mask_b = 0,

	/* SPM_SRC2_MASK */
	.dvfs_halt_mask_b = 0x1f,	/* 5bit */
	.vdec_req_mask_b = 0,
	.gce_req_mask_b = 1,
	.cpu_md_dvfs_req_merge_mask_b = 0,
	.md_ddr_en_dvfs_halt_mask_b = 0,
	.dsi0_vsync_dvfs_halt_mask_b = 0,	/* 5bit */
	.conn_ddr_en_mask_b = 1,
	.disp_req_mask_b = 1,
	.disp1_req_mask_b = 1,
	.mfg_req_mask_b = 0,
	.c2k_ps_rccif_wake_mask_b = 1,
	.c2k_l1_rccif_wake_mask_b = 1,
	.ps_c2k_rccif_wake_mask_b = 1,
	.l1_c2k_rccif_wake_mask_b = 1,
	.sdio_on_dvfs_req_mask_b = 0,
	.emi_boost_dvfs_req_mask_b = 0,
	.cpu_md_emi_dvfs_req_prot_dis = 0,
	.dramc_spcmd_apsrc_req_mask_b = 0,

	/* SPM_CLK_CON */
	.srclkenai_mask = 1,

	.mp1_cpu0_wfi_en	= 1,
	.mp1_cpu1_wfi_en	= 1,
	.mp1_cpu2_wfi_en	= 1,
	.mp1_cpu3_wfi_en	= 1,
	.mp0_cpu0_wfi_en	= 1,
	.mp0_cpu1_wfi_en	= 1,
	.mp0_cpu2_wfi_en	= 1,
	.mp0_cpu3_wfi_en	= 1,

#if SPM_BYPASS_SYSPWREQ
	.csyspwreq_mask = 1,
#endif
};
#else
#error "Not support SODI"
#endif

/* please put firmware to vendor/mediatek/proprietary/hardware/spm/mtxxxx/ */
struct spm_lp_scen __spm_sodi = {
	.pwrctrl = &sodi_ctrl,
};

static bool gSpm_SODI_mempll_pwr_mode;
static bool gSpm_sodi_en;
static bool gSpm_lcm_vdo_mode;

static long int sodi_logout_prev_time;
static int pre_emi_refresh_cnt;
static bool memPllCG_prev_status = true;	/* true:CG, false:pwrdn */
static unsigned int logout_sodi_cnt;
static unsigned int logout_selfrefresh_cnt;
static unsigned int last_r12;

#define IS_NOT_FREQUENT_EVENT(evt, curr)	((evt != last_r12) || \
					 (((curr) - sodi_logout_prev_time) > SODI_LOGOUT_FREQUENT_CRITERIA))
#define IS_NOT_IGNORE_EVENT(evt)		(((evt)&WAKE_SRC_R12_APXGPT1_EVENT_B) == 0)

void spm_trigger_wfi_for_sodi(struct pwr_ctrl *pwrctrl)
{
	u32 v0, v1;

	if (is_cpu_pdn(pwrctrl->pcm_flags)) {
#if (defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)) && defined(SODI_VSRAM_VPROC_SHUTDOWN)
		mtk_enter_idle_state(MTK_SUSPEND_MODE);
#else
		mtk_enter_idle_state(MTK_SODI_MODE);
#endif
	} else {
		/* backup MPx_AXI_CONFIG */
		v0 = reg_read(MP0_AXI_CONFIG);
		v1 = reg_read(MP1_AXI_CONFIG);

		/* disable snoop function */
		MCUSYS_SMC_WRITE(MP0_AXI_CONFIG, v0 | ACINACTM);
		MCUSYS_SMC_WRITE(MP1_AXI_CONFIG, v1 | ACINACTM);

		sodi_debug("enter legacy WFI, MP0_AXI_CONFIG=0x%x, MP1_AXI_CONFIG=0x%x\n",
			   reg_read(MP0_AXI_CONFIG), reg_read(MP1_AXI_CONFIG));

		/* enter WFI */
		wfi_with_sync();

		/* restore MP0_AXI_CONFIG */
		MCUSYS_SMC_WRITE(MP0_AXI_CONFIG, v0);
		MCUSYS_SMC_WRITE(MP1_AXI_CONFIG, v1);

		sodi_debug("exit legacy WFI, MP0_AXI_CONFIG=0x%x, MP1_AXI_CONFIG=0x%x\n",
			   reg_read(MP0_AXI_CONFIG), reg_read(MP1_AXI_CONFIG));
	}
}

static u32 mmu_smi_async_cfg;
void spm_disable_mmu_smi_async(void)
{
	mmu_smi_async_cfg = reg_read(MMU_SMI_ASYNC_CFG);
	reg_write(MMU_SMI_ASYNC_CFG, mmu_smi_async_cfg | SMI_COMMON_ASYNC_DCM);
}

void spm_enable_mmu_smi_async(void)
{
	reg_write(MMU_SMI_ASYNC_CFG, mmu_smi_async_cfg);
}

#if defined(CONFIG_FPGA_EARLY_PORTING)
void spm_sodi_pmic_before_wfi(void)
{
}

void spm_sodi_pmic_after_wfi(void)
{
}

#elif defined(CONFIG_MTK_PMIC_CHIP_MT6355)
void spm_sodi_pmic_before_wfi(void)
{
#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
	u32 val;

	__spm_pmic_pg_force_on();

#ifdef SODI_VSRAM_VPROC_SHUTDOWN
	pmic_read_interface_nolock(PMIC_RG_LDO_VSRAM_PROC_EN_ADDR, &val, PMIC_VAL_SIZE_MASK, 0);
	mt_spm_pmic_wrap_set_cmd_full(PMIC_WRAP_PHASE_DEEPIDLE,
								IDX_DI_VSRAM_NORMAL,
								PMIC_RG_LDO_VSRAM_PROC_EN_ADDR,
								0x1);
	mt_spm_pmic_wrap_set_cmd_full(PMIC_WRAP_PHASE_DEEPIDLE,
								IDX_DI_VSRAM_SLEEP,
								PMIC_RG_LDO_VSRAM_PROC_EN_ADDR,
								0x3);

	pmic_read_interface_nolock(PMIC_RG_BUCK_VPROC11_EN_ADDR, &val, PMIC_VAL_SIZE_MASK, 0);
	mt_spm_pmic_wrap_set_cmd_full(PMIC_WRAP_PHASE_DEEPIDLE,
								IDX_DI_VPROC_NORMAL,
								PMIC_RG_BUCK_VPROC11_EN_ADDR,
								0x1);
	mt_spm_pmic_wrap_set_cmd_full(PMIC_WRAP_PHASE_DEEPIDLE,
								IDX_DI_VPROC_SLEEP,
								PMIC_RG_BUCK_VPROC11_EN_ADDR,
								0x3);
#else
	pmic_read_interface_nolock(PMIC_RG_LDO_VSRAM_PROC_VOSEL_ADDR,
								&val,
								PMIC_RG_LDO_VSRAM_PROC_VOSEL_MASK,
								PMIC_RG_LDO_VSRAM_PROC_VOSEL_SHIFT);
	mt_spm_pmic_wrap_set_cmd_full(PMIC_WRAP_PHASE_DEEPIDLE,
								IDX_DI_VSRAM_NORMAL,
								PMIC_RG_LDO_VSRAM_PROC_VOSEL_ADDR,
								val);
	mt_spm_pmic_wrap_set_cmd_full(PMIC_WRAP_PHASE_DEEPIDLE,
								IDX_DI_VSRAM_SLEEP,
								PMIC_RG_LDO_VSRAM_PROC_VOSEL_ADDR,
								VOLT_TO_PMIC_VAL(60000));

	pmic_read_interface_nolock(PMIC_RG_BUCK_VPROC11_VOSEL_ADDR, &val, PMIC_VAL_SIZE_MASK, 0);
	mt_spm_pmic_wrap_set_cmd_full(PMIC_WRAP_PHASE_DEEPIDLE,
								IDX_DI_VPROC_NORMAL,
								PMIC_RG_BUCK_VPROC11_VOSEL_ADDR,
								val);
	mt_spm_pmic_wrap_set_cmd_full(PMIC_WRAP_PHASE_DEEPIDLE,
								IDX_DI_VPROC_SLEEP,
								PMIC_RG_BUCK_VPROC11_VOSEL_ADDR,
								VOLT_TO_PMIC_VAL(60000));
#endif
	pmic_read_interface_nolock(MT6355_TOP_SPI_CON0, &val, ALL_TOP_CON_MASK, 0);
	mt_spm_pmic_wrap_set_cmd(PMIC_WRAP_PHASE_DEEPIDLE,
					IDX_DI_SRCCLKEN_IN2_NORMAL,
					val | (1 << PMIC_RG_SRCLKEN_IN2_EN_SHIFT));
	mt_spm_pmic_wrap_set_cmd(PMIC_WRAP_PHASE_DEEPIDLE,
					IDX_DI_SRCCLKEN_IN2_SLEEP,
					val & ~(1 << PMIC_RG_SRCLKEN_IN2_EN_SHIFT));
#endif
}

void spm_sodi_pmic_after_wfi(void)
{
	__spm_pmic_pg_force_off();
}

#else
void spm_sodi_pmic_before_wfi(void)
{
	u32 val;

	__spm_pmic_pg_force_on();
#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
#ifdef SODI_VSRAM_VPROC_SHUTDOWN
	pmic_read_interface_nolock(MT6351_PMIC_RG_VSRAM_PROC_EN_ADDR, &val, PMIC_VAL_SIZE_MASK, 0);
	mt_spm_pmic_wrap_set_cmd_full(PMIC_WRAP_PHASE_DEEPIDLE,
								IDX_DI_VSRAM_NORMAL,
								MT6351_PMIC_RG_VSRAM_PROC_EN_ADDR,
								val | (1 << MT6351_PMIC_RG_VSRAM_PROC_EN_SHIFT));
	mt_spm_pmic_wrap_set_cmd_full(PMIC_WRAP_PHASE_DEEPIDLE,
								IDX_DI_VSRAM_SLEEP,
								MT6351_PMIC_RG_VSRAM_PROC_EN_ADDR,
								val & ~(1 << MT6351_PMIC_RG_VSRAM_PROC_EN_SHIFT));
#else
	pmic_read_interface_nolock(MT6351_PMIC_BUCK_VSRAM_PROC_VOSEL_ON_ADDR,
								&val,
								MT6351_PMIC_BUCK_VSRAM_PROC_VOSEL_ON_MASK,
								MT6351_PMIC_BUCK_VSRAM_PROC_VOSEL_ON_SHIFT);
	mt_spm_pmic_wrap_set_cmd_full(PMIC_WRAP_PHASE_DEEPIDLE,
								IDX_DI_VSRAM_NORMAL,
								MT6351_PMIC_BUCK_VSRAM_PROC_VOSEL_ON_ADDR,
								val);
	mt_spm_pmic_wrap_set_cmd_full(PMIC_WRAP_PHASE_DEEPIDLE,
								IDX_DI_VSRAM_SLEEP,
								MT6351_PMIC_BUCK_VSRAM_PROC_VOSEL_ON_ADDR,
								VOLT_TO_PMIC_VAL(60000));
#endif
#endif

	pmic_read_interface_nolock(MT6351_TOP_CON, &val, ALL_TOP_CON_MASK, 0);
	mt_spm_pmic_wrap_set_cmd(PMIC_WRAP_PHASE_DEEPIDLE,
					IDX_DI_SRCCLKEN_IN2_NORMAL,
					val | (1 << MT6351_PMIC_RG_SRCLKEN_IN2_EN_SHIFT));
	mt_spm_pmic_wrap_set_cmd(PMIC_WRAP_PHASE_DEEPIDLE,
					IDX_DI_SRCCLKEN_IN2_SLEEP,
					val & ~(1 << MT6351_PMIC_RG_SRCLKEN_IN2_EN_SHIFT));
}

void spm_sodi_pmic_after_wfi(void)
{
	__spm_pmic_pg_force_off();
}
#endif

void spm_sodi_pre_process(void)
{
	spm_disable_mmu_smi_async();
	spm_bypass_boost_gpio_set();
	spm_sodi_pmic_before_wfi();

	/* set PMIC WRAP table for deepidle power control */
	mt_spm_pmic_wrap_set_phase(PMIC_WRAP_PHASE_DEEPIDLE);

	/* Do more low power setting when MD1/C2K/CONN off */
	if (is_md_c2k_conn_power_off())
		__spm_bsi_top_init_setting();
}

static void spm_sodi_post_process(void)
{
	/* set PMIC WRAP table for normal power control */
	mt_spm_pmic_wrap_set_phase(PMIC_WRAP_PHASE_NORMAL);
	spm_sodi_pmic_after_wfi();
	spm_enable_mmu_smi_async();
}

static inline bool spm_sodi_assert(struct wake_status *wakesta)
{
	return (wakesta->assert_pc != 0) || (wakesta->r12 == 0);
}

static bool spm_sodi_is_not_gpt_event(struct wake_status *wakesta, long int curr_time)
{
	bool logout = false;

	if (IS_NOT_IGNORE_EVENT(wakesta->r12) && IS_NOT_FREQUENT_EVENT(wakesta->r12, curr_time))
		logout = true;
	return logout;
}

static inline bool spm_sodi_abnormal_residency(struct wake_status *wakesta)
{
	return wakesta->timer_out >= SODI_LOGOUT_MAXTIME_CRITERIA;
}

static inline bool spm_sodi_change_emi_state(struct wake_status *wakesta)
{
	return (spm_read(SPM_PASR_DPD_0) == 0 && pre_emi_refresh_cnt > 0) ||
				(spm_read(SPM_PASR_DPD_0) > 0 && pre_emi_refresh_cnt == 0);
}

static inline bool spm_sodi_last_logout(long int curr_time)
{
	return (curr_time - sodi_logout_prev_time) > SODI_LOGOUT_INTERVAL_CRITERIA;
}

static inline bool spm_sodi_memPllCG(void)
{
	return ((spm_read(SPM_SW_FLAG) & SPM_FLAG_SODI_CG_MODE) != 0) ||
			((spm_read(DUMMY1_PWR_CON) & DUMMY1_PWR_ISO_LSB) != 0);
}

static bool spm_sodi_mem_mode_change(void)
{
	bool logout = false;

	if (memPllCG_prev_status != spm_sodi_memPllCG()) {
		memPllCG_prev_status = !memPllCG_prev_status;
		logout = true;
	}
	return logout;
}

void spm_sodi_get_vcore_opp(u32 *flags)
{
#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
	*flags |= (vcorefs_get_hw_opp() << VCORE_OPP_SHIFT);
#endif
}

void spm_sodi_dvfs_status(u32 sodi_flags)
{
#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
	u32 HPM2LPM = spm_read(SPM_SW_RSV_10);
	u32 LPM2HPM = spm_read(SPM_SW_RSV_9);
	u32 hw_opp = (sodi_flags & VCORE_OPP_MASK) >> VCORE_OPP_SHIFT;

	if (HPM2LPM || LPM2HPM)
		so_debug(sodi_flags, "vcore dvfs: H->L %d, L->H %d\n", HPM2LPM, LPM2HPM);

	if (hw_opp != OPPI_LOW_PWR)
		so_err(sodi_flags, "vcore: opp=%d, RSV_5=0x%x\n", hw_opp, spm_read(SPM_SW_RSV_5));
#endif
}

unsigned int
__spm_sodi_output_log(struct wake_status *wakesta, struct pcm_desc *pcmdesc, int vcore_status, u32 flags, int logout)
{
	unsigned int wr = WR_NONE;

	if ((wakesta->assert_pc != 0) || (wakesta->r12 == 0)) {
		if (wakesta->assert_pc != 0) {
			so_err(flags, "Warning: wakeup reason is WR_PCM_ASSERT!\n");
			wr = WR_PCM_ASSERT;
		} else if (wakesta->r12 == 0) {
			so_err(flags, "Warning: wakeup reason is WR_UNKNOWN!\n");
			wr = WR_UNKNOWN;
		}

		so_err(flags, "VCORE_STATUS = %d, SELF_REFRESH = 0x%x, SW_FLAG = 0x%x, 0x%x, %s\n",
					vcore_status, spm_read(SPM_PASR_DPD_0),
					spm_read(SPM_SW_FLAG), spm_read(DUMMY1_PWR_CON), pcmdesc->version);

		so_err(flags, "SODI_CNT = %d, SELF_REFRESH_CNT = 0x%x, SPM_PC = 0x%0x, R13 = 0x%x, DEBUG_FLAG = 0x%x\n",
					logout_sodi_cnt, logout_selfrefresh_cnt,
					wakesta->assert_pc, wakesta->r13, wakesta->debug_flag);

		so_err(flags, "R12 = 0x%x, R12_E = 0x%x, RAW_STA = 0x%x, IDLE_STA = 0x%x, EVENT_REG = 0x%x, ISR = 0x%x\n",
					wakesta->r12, wakesta->r12_ext, wakesta->raw_sta, wakesta->idle_sta,
					wakesta->event_reg, wakesta->isr);
	} else {
		char buf[LOG_BUF_SIZE] = { 0 };
		int i;

		if (wakesta->r12 & WAKE_SRC_R12_PCM_TIMER) {
			if (wakesta->wake_misc & WAKE_MISC_PCM_TIMER)
				strncat(buf, " PCM_TIMER", sizeof(buf) - strlen(buf) - 1);

			if (wakesta->wake_misc & WAKE_MISC_TWAM)
				strncat(buf, " TWAM", sizeof(buf) - strlen(buf) - 1);

			if (wakesta->wake_misc & WAKE_MISC_CPU_WAKE)
				strncat(buf, " CPU", sizeof(buf) - strlen(buf) - 1);
		}
		for (i = 1; i < 32; i++) {
			if (wakesta->r12 & (1U << i)) {
				strncat(buf, wakesrc_str[i], sizeof(buf) - strlen(buf) - 1);
				wr = WR_WAKE_SRC;
			}
		}

		if (unlikely(strlen(buf) >= LOG_BUF_SIZE))
			strncpy(buf, "None (LOG BUFFER OVERFLOW)", sizeof(buf));

		so_warn(flags, "wake up by %s, vcore_status = %d, self_refresh = 0x%x, sw_flag = 0x%x, 0x%x, %s, %d, 0x%x, timer_out = %u, r13 = 0x%x, debug_flag = 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, %d\n",
					    buf, vcore_status, spm_read(SPM_PASR_DPD_0),
					    spm_read(SPM_SW_FLAG), spm_read(DUMMY1_PWR_CON), pcmdesc->version,
					    logout_sodi_cnt, logout_selfrefresh_cnt,
					    wakesta->timer_out, wakesta->r13, wakesta->debug_flag,
					    wakesta->r12, wakesta->r12_ext, wakesta->raw_sta, wakesta->idle_sta,
					    wakesta->event_reg, wakesta->isr, logout);
	}
	last_r12 = wakesta->r12;
	return wr;
}

unsigned int
spm_sodi_output_log(struct wake_status *wakesta, struct pcm_desc *pcmdesc, int vcore_status, u32 sodi_flags)
{
	unsigned int wr = WR_NONE;
	long int sodi_logout_curr_time = 0;
	int need_log_out = SODI_LOGOUT_NONE;

	if (sodi_flags&SODI_FLAG_NO_LOG) {
		if (spm_sodi_assert(wakesta)) {
			so_err(sodi_flags, "PCM ASSERT AT SPM_PC = 0x%0x (%s), R12 = 0x%x, R13 = 0x%x, DEBUG_FLAG = 0x%x\n",
				wakesta->assert_pc, pcmdesc->version,
				wakesta->r12, wakesta->r13, wakesta->debug_flag);
			wr = WR_PCM_ASSERT;
		}
	} else if (!(sodi_flags&SODI_FLAG_REDUCE_LOG) || (sodi_flags & SODI_FLAG_RESIDENCY)) {
		wr = __spm_sodi_output_log(wakesta, pcmdesc, vcore_status, sodi_flags, need_log_out);
	} else {
		sodi_logout_curr_time = spm_get_current_time_ms();

		if (spm_sodi_assert(wakesta))
			need_log_out = SODI_LOGOUT_ASSERT;
		else if (spm_sodi_is_not_gpt_event(wakesta, sodi_logout_curr_time))
			need_log_out = SODI_LOGOUT_NOT_GPT_EVENT;
		else if (spm_sodi_abnormal_residency(wakesta))
			need_log_out = SODI_LOGOUT_RESIDENCY_ABNORMAL;
		else if (spm_sodi_change_emi_state(wakesta))
			need_log_out = SODI_LOGOUT_EMI_STATE_CHANGE;
		else if (spm_sodi_last_logout(sodi_logout_curr_time))
			need_log_out = SODI_LOGOUT_LONG_INTERVAL;
		else if (spm_sodi_mem_mode_change())
			need_log_out = SODI_LOGOUT_CG_PD_STATE_CHANGE;

		logout_sodi_cnt++;
		logout_selfrefresh_cnt += spm_read(SPM_PASR_DPD_0);
		pre_emi_refresh_cnt = spm_read(SPM_PASR_DPD_0);

		if (need_log_out != SODI_LOGOUT_NONE) {
			wr = __spm_sodi_output_log(wakesta, pcmdesc, vcore_status, sodi_flags, need_log_out);
			sodi_logout_prev_time = sodi_logout_curr_time;
			logout_sodi_cnt = 0;
			logout_selfrefresh_cnt = 0;
		}
	}
	spm_sodi_dvfs_status(sodi_flags);
	return wr;
}

static void rekick_sodi_common_scenario(void)
{
}

unsigned int spm_go_to_sodi(u32 spm_flags, u32 spm_data, u32 sodi_flags)
{
	struct wake_status wakesta;
	unsigned long flags;
	struct mtk_irq_mask *mask;
	unsigned int wr = WR_NONE;
	struct pcm_desc *pcmdesc;
	struct pwr_ctrl *pwrctrl = __spm_sodi.pwrctrl;
	int vcore_status;
	u32 cpu = spm_data;
	u32 sodi_idx;

	sodi_idx = spm_sodi_get_pcm_idx(cpu);

	if (!dyna_load_pcm[sodi_idx].ready) {
		sodi_err("ERROR: LOAD FIRMWARE FAIL\n");
		return WR_NONE;
	}
	pcmdesc = &(dyna_load_pcm[sodi_idx].desc);

	spm_sodi_footprint(SPM_SODI_ENTER);

	if (gSpm_SODI_mempll_pwr_mode == MEMPLL_CG_MODE)
		spm_flags |= SPM_FLAG_SODI_CG_MODE;	/* CG mode */
	else
		spm_flags &= ~SPM_FLAG_SODI_CG_MODE;	/* PDN mode */

	update_pwrctrl_pcm_flags(&spm_flags);
	set_pwrctrl_pcm_flags(pwrctrl, spm_flags);

	/* enable APxGPT timer */
	soidle_before_wfi(cpu);

	spin_lock_irqsave(&__spm_lock, flags);

	mask = kmalloc(sizeof(struct mtk_irq_mask), GFP_ATOMIC);
	if (!mask) {
		wr = -ENOMEM;
		goto UNLOCK_SPM;
	}
	mt_irq_mask_all(mask);
	mt_irq_unmask_for_sleep_ex(SPM_IRQ0_ID);
	unmask_edge_trig_irqs_for_cirq();
#if defined(CONFIG_MTK_SYS_CIRQ)
	mt_cirq_clone_gic();
	mt_cirq_enable();
#endif

	spm_sodi_footprint(SPM_SODI_ENTER_UART_SLEEP);

	if (request_uart_to_sleep()) {
		wr = WR_UART_BUSY;
		goto RESTORE_IRQ;
	}

	spm_sodi_footprint(SPM_SODI_ENTER_SPM_FLOW);

	__spm_reset_and_init_pcm(pcmdesc);

	__spm_kick_im_to_fetch(pcmdesc);

	__spm_init_pcm_register();

	__spm_init_event_vector(pcmdesc);

	__spm_check_md_pdn_power_control(pwrctrl);

	__spm_sync_vcore_dvfs_power_control(pwrctrl, __spm_vcore_dvfs.pwrctrl);

	__spm_set_power_control(pwrctrl);

	__spm_src_req_update(pwrctrl);

	__spm_set_wakeup_event(pwrctrl);

	vcore_status = vcorefs_get_curr_ddr();
	spm_sodi_get_vcore_opp(&sodi_flags);

	spm_sodi_pre_process();

	__spm_kick_pcm_to_run(pwrctrl);

	spm_sodi_footprint_val((1 << SPM_SODI_ENTER_WFI) |
				(1 << SPM_SODI_B3) | (1 << SPM_SODI_B4) |
				(1 << SPM_SODI_B5) | (1 << SPM_SODI_B6));

	soidle_profile_time(1);

	spm_trigger_wfi_for_sodi(pwrctrl);

	soidle_profile_time(2);

	spm_sodi_footprint(SPM_SODI_LEAVE_WFI);

	spm_sodi_post_process();

	__spm_get_wakeup_status(&wakesta);

	__spm_clean_after_wakeup();

	spm_sodi_footprint(SPM_SODI_ENTER_UART_AWAKE);

	request_uart_to_wakeup();

	wr = spm_sodi_output_log(&wakesta, pcmdesc, vcore_status, sodi_flags);

	spm_sodi_footprint(SPM_SODI_LEAVE_SPM_FLOW);

RESTORE_IRQ:
#if defined(CONFIG_MTK_SYS_CIRQ)
	mt_cirq_flush();
	mt_cirq_disable();
#endif
	mt_irq_mask_restore(mask);
	kfree(mask);

UNLOCK_SPM:
	spin_unlock_irqrestore(&__spm_lock, flags);

	/* stop APxGPT timer and enable caore0 local timer */
	soidle_after_wfi(cpu);

	if (wr != WR_UART_BUSY)
		rekick_sodi_common_scenario();

	spm_sodi_reset_footprint();

	return wr;
}

void spm_sodi_set_vdo_mode(bool vdo_mode)
{
	gSpm_lcm_vdo_mode = vdo_mode;
}

bool spm_get_cmd_mode(void)
{
	return !gSpm_lcm_vdo_mode;
}

void spm_sodi_mempll_pwr_mode(bool pwr_mode)
{
	gSpm_SODI_mempll_pwr_mode = pwr_mode;
}

bool spm_get_sodi_mempll(void)
{
	return gSpm_SODI_mempll_pwr_mode;
}

void spm_enable_sodi(bool en)
{
	gSpm_sodi_en = en;
}

bool spm_get_sodi_en(void)
{
	return gSpm_sodi_en;
}

void spm_sodi_init(void)
{
#if defined(CONFIG_OF)
	struct device_node *node;
	struct resource r;

	/* mcucfg */
	node = of_find_compatible_node(NULL, NULL, MCUCFG_NODE);
	if (!node) {
		sodi_err("error: cannot find node " MCUCFG_NODE);
		goto mcucfg_exit;
	}
	if (of_address_to_resource(node, 0, &r)) {
		sodi_err("error: cannot get phys addr" MCUCFG_NODE);
		goto mcucfg_exit;
	}
	mcucfg_phys_base = r.start;

	mcucfg_base = (unsigned long)of_iomap(node, 0);
	if (!mcucfg_base) {
		sodi_err("error: cannot iomap " MCUCFG_NODE);
		goto mcucfg_exit;
	}

	sodi_debug("mcucfg_base = 0x%x\n", (unsigned int)mcucfg_base);

mcucfg_exit:
	/* m4u */
	node = of_find_compatible_node(NULL, NULL, M4U_NODE);
	if (!node) {
		sodi_err("error: cannot find node " M4U_NODE);
		goto m4u_exit;
	}
	if (of_address_to_resource(node, 0, &r)) {
		sodi_err("error: cannot get phys addr" M4U_NODE);
		goto m4u_exit;
	}
	m4u_phys_base = r.start;

	m4u_base = (unsigned long)of_iomap(node, 0);
	if (!m4u_base) {
		sodi_err("error: cannot iomap " M4U_NODE);
		goto m4u_exit;
	}

	sodi_debug("m4u_base = 0x%x\n", (unsigned int)m4u_base);

m4u_exit:
	sodi_debug("%s\n", __func__);
#endif

#if defined(SPM_SODI_PROFILE_TIME) && !defined(IDLE_PROF_USING_STD_TIMER) && defined(SPM_SODI_PROFILE_APXGPT)
	request_gpt(SPM_SODI_PROFILE_APXGPT, GPT_FREE_RUN, GPT_CLK_SRC_SYS, GPT_CLK_DIV_1,
			  0, NULL, GPT_NOIRQEN);
#endif
	spm_sodi_aee_init();
}

MODULE_DESCRIPTION("SPM-SODI Driver v0.1");
