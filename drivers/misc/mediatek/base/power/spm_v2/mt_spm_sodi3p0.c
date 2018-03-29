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

#include <mach/irqs.h>
#include <mach/mt_gpt.h>
#ifdef CONFIG_MTK_WD_KICKER
#include <mach/wd_api.h>
#endif

#include <mt-plat/mt_boot.h>
#if defined(CONFIG_MTK_SYS_CIRQ)
#include <mt-plat/mt_cirq.h>
#endif
#include <mt-plat/upmu_common.h>
#include <mt-plat/mt_io.h>

#include <mt_clkbuf_ctl.h>
#include <mt_spm_sodi.h>
#include <mt_spm_sodi3.h>
#include <mt_idle_profile.h>

#if defined(CONFIG_ARCH_MT6755)
#include "ext_wd_drv.h"
#endif

/**************************************
 * only for internal debug
 **************************************/

#define PCM_SEC_TO_TICK(sec)        (sec * 32768)

unsigned int __attribute__((weak)) pmic_read_interface_nolock(unsigned int RegNum,
					unsigned int *val, unsigned int MASK, unsigned int SHIFT)
{
	return -1;
}


#if defined(CONFIG_ARCH_MT6757)
static struct pwr_ctrl sodi3_ctrl = {
	.wake_src			= WAKE_SRC_FOR_SODI3,
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
static struct pwr_ctrl sodi3_ctrl = {
	.wake_src = WAKE_SRC_FOR_SODI3,

	.wake_src_md32 = WAKE_SRC_FOR_MD32,
	.r0_ctrl_en = 1,
	.r7_ctrl_en = 1,
	.infra_dcm_lock = 1, /* set to be 1 if SODI 3.0 */
	.wfi_op = WFI_OP_AND,

	/* SPM_AP_STANDBY_CON */
	.mp0top_idle_mask = 0,
	.mp1top_idle_mask = 0,
	.mcusys_idle_mask = 0,
	.md_ddr_dbc_en = 0,
	.md1_req_mask_b = 1,
	.md2_req_mask_b = 0, /* bit 20 */
#if defined(CONFIG_ARCH_MT6755)
	.scp_req_mask_b = 0, /* bit 21 */
#elif defined(CONFIG_ARCH_MT6797)
	.scp_req_mask_b = 1, /* bit 21 */
#endif
	.lte_mask_b = 0,
	.md_apsrc1_sel = 0, /* bit 24 */
	.md_apsrc0_sel = 0, /* bit 25 */
	.conn_mask_b = 1,
	.conn_apsrc_sel = 0, /* bit 27 */

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
	.ccif0_to_md_mask_b = 1,
	.ccif0_to_ap_mask_b = 1,
	.ccif1_to_md_mask_b = 1,
	.ccif1_to_ap_mask_b = 1,
	.ccifmd_md1_event_mask_b = 1,
	.ccifmd_md2_event_mask_b = 1,
	.vsync_mask_b = 0,	/* 5-bit */
	.md_srcclkena_0_infra_mask_b = 0, /* bit 12 */
	.md_srcclkena_1_infra_mask_b = 0, /* bit 13 */
	.conn_srcclkena_infra_mask_b = 0, /* bit 14 */
	.md32_srcclkena_infra_mask_b = 0, /* bit 15 */
	.srcclkeni_infra_mask_b = 0, /* bit 16 */
	.md_apsrcreq_0_infra_mask_b = 1,
	.md_apsrcreq_1_infra_mask_b = 0,
	.conn_apsrcreq_infra_mask_b = 1,
	.md32_apsrcreq_infra_mask_b = 0,
	.md_ddr_en_0_mask_b = 1,
	.md_ddr_en_1_mask_b = 0, /* bit 22 */
	.md_vrf18_req_0_mask_b = 1,
	.md_vrf18_req_1_mask_b = 0, /* bit 24 */
	.emi_bw_dvfs_req_mask = 1,
	.md_srcclkena_0_dvfs_req_mask_b = 0,
	.md_srcclkena_1_dvfs_req_mask_b = 0,
	.conn_srcclkena_dvfs_req_mask_b = 0,

	/* SPM_SRC2_MASK */
	.dvfs_halt_mask_b = 0x1f, /* 5-bit */
	.vdec_req_mask_b = 0, /* bit 6 */
	.gce_req_mask_b = 1, /* bit 7, set to be 1 for SODI */
	.cpu_md_dvfs_erq_merge_mask_b = 0,
	.md1_ddr_en_dvfs_halt_mask_b = 0,
	.md2_ddr_en_dvfs_halt_mask_b = 0,
	.vsync_dvfs_halt_mask_b = 0, /* 5-bit, bit 11 ~ 15 */
	.conn_ddr_en_mask_b = 1,
	.disp_req_mask_b = 1, /* bit 17, set to be 1 for SODI */
	.disp1_req_mask_b = 1, /* bit 18, set to be 1 for SODI */
#if defined(CONFIG_ARCH_MT6755)
	.mfg_req_mask_b = 0, /* bit 19 */
#elif defined(CONFIG_ARCH_MT6797)
	.mfg_req_mask_b = 1, /* bit 19, set to be 1 for SODI */
#endif
	.c2k_ps_rccif_wake_mask_b = 1,
	.c2k_l1_rccif_wake_mask_b = 1,
	.ps_c2k_rccif_wake_mask_b = 1,
	.l1_c2k_rccif_wake_mask_b = 1,
	.sdio_on_dvfs_req_mask_b = 0,
	.emi_boost_dvfs_req_mask_b = 0,
	.cpu_md_emi_dvfs_req_prot_dis = 0,
#if defined(CONFIG_ARCH_MT6797)
	.disp_od_req_mask_b = 1, /* bit 27, set to be 1 for SODI */
#endif
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
	.syspwreq_mask = 1,
#endif
};
#endif

struct spm_lp_scen __spm_sodi3 = {
	.pwrctrl = &sodi3_ctrl,
};

static bool gSpm_sodi3_en;

static void spm_sodi3_pre_process(void)
{
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	u32 val;
#endif

	spm_disable_mmu_smi_async();
	spm_bypass_boost_gpio_set();

#if !defined(CONFIG_FPGA_EARLY_PORTING)
	__spm_pmic_pg_force_on();
	spm_pmic_power_mode(PMIC_PWR_SODI3, 0, 0);

#if defined(CONFIG_ARCH_MT6755) && defined(CONFIG_MTK_PMIC_CHIP_MT6353)
	spm_vmd_sel_gpio_set();
#endif

#if defined(CONFIG_ARCH_MT6755)
#if defined(CONFIG_MTK_PMIC_CHIP_MT6353)
	pmic_read_interface_nolock(PMIC_LDO_VSRAM_PROC_VOSEL_ON_ADDR,
					&val,
					PMIC_LDO_VSRAM_PROC_VOSEL_ON_MASK,
					PMIC_LDO_VSRAM_PROC_VOSEL_ON_SHIFT);
#else
	pmic_read_interface_nolock(MT6351_PMIC_BUCK_VSRAM_PROC_VOSEL_ON_ADDR,
					&val,
					MT6351_PMIC_BUCK_VSRAM_PROC_VOSEL_ON_MASK,
					MT6351_PMIC_BUCK_VSRAM_PROC_VOSEL_ON_SHIFT);
#endif
	mt_spm_pmic_wrap_set_cmd(PMIC_WRAP_PHASE_DEEPIDLE, IDX_DI_VSRAM_NORMAL, val);
#elif defined(CONFIG_ARCH_MT6757)
	pmic_read_interface_nolock(MT6351_PMIC_BUCK_VSRAM_PROC_VOSEL_ON_ADDR,
					&val,
					MT6351_PMIC_BUCK_VSRAM_PROC_VOSEL_ON_MASK,
					MT6351_PMIC_BUCK_VSRAM_PROC_VOSEL_ON_SHIFT);
	mt_spm_pmic_wrap_set_cmd(PMIC_WRAP_PHASE_DEEPIDLE, IDX_DI_VSRAM_NORMAL, val);
#elif defined(CONFIG_ARCH_MT6797)
	pmic_read_interface_nolock(MT6351_PMIC_RG_VCORE_VDIFF_ENLOWIQ_ADDR, &val, 0xFFFF, 0);
	mt_spm_pmic_wrap_set_cmd(PMIC_WRAP_PHASE_DEEPIDLE,
			IDX_DI_VCORE_LQ_EN,
			val | (1 << MT6351_PMIC_RG_VCORE_VDIFF_ENLOWIQ_SHIFT));
	mt_spm_pmic_wrap_set_cmd(PMIC_WRAP_PHASE_DEEPIDLE,
			IDX_DI_VCORE_LQ_DIS,
			val & ~(1 << MT6351_PMIC_RG_VCORE_VDIFF_ENLOWIQ_SHIFT));

	__spm_pmic_low_iq_mode(1);
#endif

#if defined(CONFIG_ARCH_MT6755) && defined(CONFIG_MTK_PMIC_CHIP_MT6353)
	pmic_read_interface_nolock(PMIC_RG_SRCLKEN_IN2_EN_ADDR, &val, ALL_TOP_CON_MASK, 0);
	mt_spm_pmic_wrap_set_cmd(PMIC_WRAP_PHASE_DEEPIDLE,
					IDX_DI_SRCCLKEN_IN2_NORMAL,
					val | (1 << PMIC_RG_SRCLKEN_IN2_EN_SHIFT));
	mt_spm_pmic_wrap_set_cmd(PMIC_WRAP_PHASE_DEEPIDLE,
					IDX_DI_SRCCLKEN_IN2_SLEEP,
					val & ~(1 << PMIC_RG_SRCLKEN_IN2_EN_SHIFT));
#else
	pmic_read_interface_nolock(MT6351_TOP_CON, &val, ALL_TOP_CON_MASK, 0);
	mt_spm_pmic_wrap_set_cmd(PMIC_WRAP_PHASE_DEEPIDLE,
					IDX_DI_SRCCLKEN_IN2_NORMAL,
					val | (1 << MT6351_PMIC_RG_SRCLKEN_IN2_EN_SHIFT));
	mt_spm_pmic_wrap_set_cmd(PMIC_WRAP_PHASE_DEEPIDLE,
					IDX_DI_SRCCLKEN_IN2_SLEEP,
					val & ~(1 << MT6351_PMIC_RG_SRCLKEN_IN2_EN_SHIFT));
#endif

#if defined(CONFIG_ARCH_MT6755) && defined(CONFIG_MTK_PMIC_CHIP_MT6353)
	pmic_read_interface_nolock(PMIC_BUCK_VPROC_VOSEL_ON_ADDR,
					&val,
					PMIC_BUCK_VPROC_VOSEL_ON_MASK,
					PMIC_BUCK_VPROC_VOSEL_ON_SHIFT);
	mt_spm_pmic_wrap_set_cmd(PMIC_WRAP_PHASE_DEEPIDLE, IDX_DI_VPROC_NORMAL, val);
#else
	/* nothing */
#endif

	/* set PMIC WRAP table for deepidle power control */
	mt_spm_pmic_wrap_set_phase(PMIC_WRAP_PHASE_DEEPIDLE);
#endif

	/* for afcdac setting */
	clk_buf_write_afcdac();

	/* Do more low power setting when MD1/C2K/CONN off */
	if (is_md_c2k_conn_power_off()) {
		__spm_bsi_top_init_setting();
		__spm_backup_pmic_ck_pdn();
	}
}

static void spm_sodi3_post_process(void)
{
	if (is_md_c2k_conn_power_off())
		__spm_restore_pmic_ck_pdn();
#if defined(CONFIG_ARCH_MT6797)
	__spm_pmic_low_iq_mode(0);
#endif

#if !defined(CONFIG_FPGA_EARLY_PORTING)
	/* set PMIC WRAP table for normal power control */
	mt_spm_pmic_wrap_set_phase(PMIC_WRAP_PHASE_NORMAL);
	__spm_pmic_pg_force_off();
#endif

	spm_enable_mmu_smi_async();
}

static void rekick_sodi3_common_scenario(void)
{
#if defined(CONFIG_ARCH_MT6797)
	spm_sodi3_footprint(SPM_SODI3_REKICK_VCORE);
	spm_write(PCM_CON1, SPM_REGWR_CFG_KEY | (spm_read(PCM_CON1) & ~PCM_TIMER_EN_LSB));
	__spm_backup_vcore_dvfs_dram_shuffle();
	vcorefs_go_to_vcore_dvfs();
#endif
}


wake_reason_t spm_go_to_sodi3(u32 spm_flags, u32 spm_data, u32 sodi3_flags)
{
	u32 sec = 2;
#ifdef CONFIG_MTK_WD_KICKER
	int wd_ret;
	struct wd_api *wd_api;
#endif
	u32 con1;
	struct wake_status wakesta;
	unsigned long flags;
	struct mtk_irq_mask *mask;
	wake_reason_t wr = WR_NONE;
	struct pcm_desc *pcmdesc;
	struct pwr_ctrl *pwrctrl = __spm_sodi3.pwrctrl;
	int vcore_status = 0;
	u32 cpu = spm_data;
	u32 sodi_idx;

	sodi_idx = DYNA_LOAD_PCM_SODI + cpu / 4;

	if (!dyna_load_pcm[sodi_idx].ready) {
		sodi3_err("ERROR: LOAD FIRMWARE FAIL\n");
		BUG();
	}
	pcmdesc = &(dyna_load_pcm[sodi_idx].desc);

#if !defined(CONFIG_FPGA_EARLY_PORTING)
	vcore_status = vcorefs_get_curr_ddr();
#endif

	spm_sodi3_footprint(SPM_SODI3_ENTER);

	if (spm_get_sodi_mempll() == MEMPLL_CG_MODE)
		spm_flags |= SPM_FLAG_SODI_CG_MODE;	/* CG mode */
	else
		spm_flags &= ~SPM_FLAG_SODI_CG_MODE;	/* PDN mode */

	update_pwrctrl_pcm_flags(&spm_flags);
	set_pwrctrl_pcm_flags(pwrctrl, spm_flags);

	pwrctrl->timer_val = PCM_SEC_TO_TICK(sec);

#ifdef CONFIG_MTK_WD_KICKER
	wd_ret = get_wd_api(&wd_api);
	if (!wd_ret) {
		wd_api->wd_spmwdt_mode_config(WD_REQ_EN, WD_REQ_RST_MODE);
#if defined(CONFIG_ARCH_MT6755)
		mtk_wd_suspend_sodi();
#else
		wd_api->wd_suspend_notify();
#endif
	}
#endif

	/* enable APxGPT timer */
	soidle3_before_wfi(cpu);

	lockdep_off();
	spin_lock_irqsave(&__spm_lock, flags);

	mask = kmalloc(sizeof(struct mtk_irq_mask), GFP_ATOMIC);
	if (!mask) {
		wr = -ENOMEM;
		goto UNLOCK_SPM;
	}
	mt_irq_mask_all(mask);
	mt_irq_unmask_for_sleep(SPM_IRQ0_ID);
	unmask_edge_trig_irqs_for_cirq();
#if defined(CONFIG_MTK_SYS_CIRQ)
	mt_cirq_clone_gic();
	mt_cirq_enable();
#endif

	spm_sodi3_footprint(SPM_SODI3_ENTER_UART_SLEEP);

	if (request_uart_to_sleep()) {
		wr = WR_UART_BUSY;
		goto RESTORE_IRQ;
	}

	spm_sodi3_footprint(SPM_SODI3_ENTER_SPM_FLOW);

	__spm_reset_and_init_pcm(pcmdesc);

	__spm_kick_im_to_fetch(pcmdesc);

	__spm_init_pcm_register();

	__spm_init_event_vector(pcmdesc);

	__spm_check_md_pdn_power_control(pwrctrl);
	
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	__spm_sync_vcore_dvfs_power_control(pwrctrl, __spm_vcore_dvfs.pwrctrl);
#endif

	__spm_set_power_control(pwrctrl);

	__spm_set_wakeup_event(pwrctrl);

	/* PCM WDT WAKE MODE for lastPC */
	con1 = spm_read(PCM_CON1) & ~(PCM_WDT_WAKE_MODE_LSB | PCM_WDT_EN_LSB);
	spm_write(PCM_CON1, SPM_REGWR_CFG_KEY | con1);
	if (spm_read(PCM_TIMER_VAL) > PCM_TIMER_MAX)
		spm_write(PCM_TIMER_VAL, PCM_TIMER_MAX);
	spm_write(PCM_WDT_VAL, spm_read(PCM_TIMER_VAL) + PCM_WDT_TIMEOUT);
	if (pwrctrl->timer_val_cust == 0)
		spm_write(PCM_CON1, con1 | SPM_REGWR_CFG_KEY | PCM_WDT_EN_LSB | PCM_TIMER_EN_LSB);
	else
		spm_write(PCM_CON1, con1 | SPM_REGWR_CFG_KEY | PCM_WDT_EN_LSB);

	spm_sodi3_pre_process();

	__spm_kick_pcm_to_run(pwrctrl);

	spm_sodi3_footprint_val((1 << SPM_SODI3_ENTER_WFI) |
				(1 << SPM_SODI3_B3) | (1 << SPM_SODI3_B4) |
				(1 << SPM_SODI3_B5) | (1 << SPM_SODI3_B6));

	soidle3_profile_time(1);

	spm_trigger_wfi_for_sodi(pwrctrl);

	soidle3_profile_time(2);

	spm_sodi3_footprint(SPM_SODI3_LEAVE_WFI);

	spm_sodi3_post_process();

	__spm_get_wakeup_status(&wakesta);

	/* disable PCM WDT to stop count if needed */
	spm_write(PCM_CON1, SPM_REGWR_CFG_KEY | (spm_read(PCM_CON1) & ~PCM_WDT_EN_LSB));

	__spm_clean_after_wakeup();

	spm_sodi3_footprint(SPM_SODI3_ENTER_UART_AWAKE);

	request_uart_to_wakeup();

	wr = spm_sodi_output_log(&wakesta, pcmdesc, vcore_status, sodi3_flags);

	spm_sodi3_footprint(SPM_SODI3_LEAVE_SPM_FLOW);

RESTORE_IRQ:
#if defined(CONFIG_MTK_SYS_CIRQ)
	mt_cirq_flush();
	mt_cirq_disable();
#endif
	mt_irq_mask_restore(mask);
	kfree(mask);

UNLOCK_SPM:
	spin_unlock_irqrestore(&__spm_lock, flags);
	lockdep_on();

	/* stop APxGPT timer and enable caore0 local timer */
	soidle3_after_wfi(cpu);
#ifdef CONFIG_MTK_WD_KICKER
	if (!wd_ret) {
#if defined(CONFIG_ARCH_MT6755)
		mtk_wd_resume_sodi();
#else
		wd_api->wd_resume_notify();
#endif
		wd_api->wd_spmwdt_mode_config(WD_REQ_DIS, WD_REQ_RST_MODE);
	}
#endif

	if (wr != WR_UART_BUSY)
		rekick_sodi3_common_scenario();

	spm_sodi3_reset_footprint();
	return wr;
}

void spm_enable_sodi3(bool en)
{
	gSpm_sodi3_en = en;
}

bool spm_get_sodi3_en(void)
{
	return gSpm_sodi3_en;
}

void spm_sodi3_init(void)
{
	sodi3_debug("spm_sodi3_init\n");
#ifdef SPM_SODI3_PROFILE_TIME
	request_gpt(SPM_SODI3_PROFILE_APXGPT, GPT_FREE_RUN, GPT_CLK_SRC_RTC, GPT_CLK_DIV_1,
			  0, NULL, GPT_NOIRQEN);
#endif
	spm_sodi3_aee_init();
}

MODULE_DESCRIPTION("SPM-SODI3 Driver v0.1");
