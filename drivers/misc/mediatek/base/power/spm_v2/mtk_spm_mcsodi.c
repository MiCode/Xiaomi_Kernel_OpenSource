/*
 * Copyright (C) 2016 MediaTek Inc.
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
#include <linux/notifier.h>
#include <linux/uaccess.h>

#include <mtk_spm_mcsodi.h>
#include <mtk_idle_profile.h>

#define MCSODI_LOGOUT_MAX_INTERVAL   (5000U)	/* unit:ms */
static bool gSpm_mcsodi_en;

#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_KIBOPLUS)
static struct pwr_ctrl mcsodi_ctrl = {
	.wake_src			= WAKE_SRC_FOR_MCSODI,
	.wake_src_md32		= WAKE_SRC_FOR_MD32,
	.r0_ctrl_en			= 1,
	.r7_ctrl_en			= 1,
	.infra_dcm_lock		= 1,
	.wfi_op				= WFI_OP_AND,

	/* SPM_AP_STANDBY_CON */
	.mp0_cputop_idle_mask = 1,
	.mp1_cputop_idle_mask = 1,
	.mcusys_idle_mask = 1,
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
#error "Not support MC-SODI"
#endif

/* please put firmware to vendor/mediatek/proprietary/hardware/spm/mtxxxx/ */
struct spm_lp_scen __spm_mcsodi = {
	.pwrctrl = &mcsodi_ctrl,
	/* .pcmdesc = &mcsodi_pcm, */
};

#if SPM_MCSODI_PROFILE_RATIO
static u32 start_timestamp;
#endif

void spm_enable_mcsodi(bool en)
{
	gSpm_mcsodi_en = en;
}

bool spm_get_mcsodi_en(void)
{
	return gSpm_mcsodi_en;
}

static void spm_output_log(struct wake_status *wakesta)
{
	unsigned long int curr_time = 0;
	unsigned int RSV17;
	static unsigned long int pre_time;
	static unsigned int logout_mcsodi_cnt;
#if SPM_MCSODI_PROFILE_RATIO
	unsigned int dur;
#endif
	RSV17 = spm_read(SPM_SW_RSV_17);
	curr_time = spm_get_current_time_ms();

	if ((curr_time - pre_time > MCSODI_LOGOUT_MAX_INTERVAL) || (RSV17 == 0x1)) {
		ms_warn("sw_flag = 0x%x, %d, timer_out= %u r13 = 0x%x, debug_flag = 0x%x, RSV_17 = 0x%x, r12 = 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
					spm_read(SPM_SW_FLAG), logout_mcsodi_cnt,
					wakesta->timer_out, wakesta->r13, wakesta->debug_flag, RSV17,
					wakesta->r12, wakesta->r12_ext, wakesta->raw_sta, wakesta->idle_sta,
					wakesta->event_reg, wakesta->isr);
		pre_time = curr_time;
		logout_mcsodi_cnt = 0;
	} else {
		logout_mcsodi_cnt++;
	}

#if SPM_MCSODI_PROFILE_RATIO
	dur = ((mtk_mcsodi_timestamp()-start_timestamp)/13) + 1;
	if (dur > 10000000U) {
		ms_warn("Duration = %d ms, SR_ratio = %d/1000, MCSODI_ratio = %d/1000,  All_WFI_ratio = %d/1000\n",
			dur/1000, (spm_read(SPM_SW_RSV_15)/13)/(dur/1000),
			(mtk_mcsodi_duration(MCSODI_RES_RATIO)/13)/(dur/1000),
			(mtk_mcsodi_duration(MCSODI_WFI_RATIO)/13)/(dur/1000));

		start_timestamp = mtk_mcsodi_timestamp();
		mtk_mcsodi_prof_reset();
		spm_write(SPM_SW_RSV_15, 0x0);
	}
#endif
}

void spm_go_to_mcsodi(u32 spm_flags, u32 cpu, u32 mcsodi_flags)
{
	struct pwr_ctrl *pwrctrl = __spm_mcsodi.pwrctrl;

	spm_mcsodi_footprint(SPM_MCSODI_ENTER);

	if (request_uart_to_sleep()) {
		spm_write(SPM_BSI_CLK_SR, ~0);
		return;
	}

	spm_mcsodi_footprint(SPM_MCSODI_SPM_FLOW);
	set_pwrctrl_pcm_flags(pwrctrl, spm_flags);
	__spm_sync_vcore_dvfs_power_control(pwrctrl, __spm_vcore_dvfs.pwrctrl);
	__spm_set_power_control(pwrctrl);
	__spm_src_req_update(pwrctrl);

	spm_write(SPM_BSI_CLK_SR, 0);
	spm_write(SPM_SW_DEBUG, 0x0);
	spm_write(SPM_SW_RSV_17, 0x0);
	spm_write(SPM_SW_FLAG, pwrctrl->pcm_flags);

	spm_mcsodi_footprint_val((1 << SPM_MCSODI_ENTER_WFI) |
				(1 << SPM_MCSODI_B2) | (1 << SPM_MCSODI_B3) |
				(1 << SPM_MCSODI_B4) | (1 << SPM_MCSODI_B5) |
				(1 << SPM_MCSODI_B6));
}

void spm_leave_mcsodi(void)
{
	struct wake_status wakesta;

	spm_mcsodi_footprint(SPM_MCSODI_LEAVE_WFI);
	request_uart_to_wakeup();
	__spm_get_wakeup_status(&wakesta);
	spm_output_log(&wakesta);
	spm_write(SPM_BSI_CLK_SR, ~0);
	spm_mcsodi_footprint(SPM_MCSODI_LEAVE);
}

void spm_mcsodi_init(void)
{
	spm_enable_mcsodi(true);
}


