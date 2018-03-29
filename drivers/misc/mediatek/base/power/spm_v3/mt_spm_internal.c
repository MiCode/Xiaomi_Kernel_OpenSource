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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/atomic.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/of_fdt.h>
#include <mach/mt_spm_mtcmos_internal.h>
#include <asm/setup.h>
#include "mt_spm_internal.h"
#include "mt_vcorefs_governor.h"
#include "mt_spm_vcorefs.h"
#include <mt-plat/upmu_common.h>

/**************************************
 * Config and Parameter
 **************************************/
#define LOG_BUF_SIZE		256

#define SPM_WAKE_PERIOD         600	/* sec */

/**************************************
 * Define and Declare
 **************************************/
DEFINE_SPINLOCK(__spm_lock);

static u32 pcm_timer_ramp_max = 1;
static u32 pcm_timer_ramp_max_sec_loop = 1;

const char *wakesrc_str[32] = {
	[0] = " R12_PCM_TIMER",
	[1] = " R12_MD32_WDT_EVENT_B",
	[2] = " R12_KP_IRQ_B",
	[3] = " R12_APWDT_EVENT_B",
	[4] = " R12_APXGPT1_EVENT_B",
	[5] = " R12_CONN2AP_SPM_WAKEUP_B",
	[6] = " R12_EINT_EVENT_B",
	[7] = " R12_CONN_WDT_IRQ_B",
	[8] = " R12_CCIF0_EVENT_B",
	[9] = " R12_LOWBATTERY_IRQ_B",
	[10] = " R12_MD32_SPM_IRQ_B",
	[11] = " R12_26M_WAKE",
	[12] = " R12_26M_SLEEP",
	[13] = " R12_PCM_WDT_WAKEUP_B",
	[14] = " R12_USBX_CDSC_B",
	[15] = " R12_USBX_POWERDWN_B",
	[16] = " R12_C2K_WDT_IRQ_B",
	[17] = " R12_EINT_EVENT_SECURE_B",
	[18] = " R12_CCIF1_EVENT_B",
	[19] = " R12_OTHERS_IRQ_B",
	[20] = " R12_AFE_IRQ_MCU_B",
	[21] = " R12_MAILBOX_IRQ_B",
	[22] = " R12_SYS_CIRQ_IRQ_B",
	[23] = " R12_IRRX_EVENT_B",
	[24] = " R12_CSYSPWREQ_B",
	[25] = " R12_MD1_WDT_B",
	[26] = " R12_CLDMA_EVENT_B",
	[27] = " R12_SEJ_EVENT_B",
	[28] = " R12_ALL_MD32_WAKEUP_B",
	[29] = " R12_CPU_IRQ_B",
	[30] = " R12_APSRC_WAKE",
	[31] = " R12_APSRC_SLEEP",
};

/* PWR_STATUS */
/* PWR_STATUS_2ND */
#define MP0_CPU0                (1U <<  9)
#define MP0_CPU1                (1U << 10)
#define MP0_CPU2                (1U << 11)
#define MP0_CPU3                (1U << 12)
#define MP1_CPU0                (1U << 16)
#define MP1_CPU1                (1U << 17)
#define MP1_CPU2                (1U << 18)
#define MP1_CPU3                (1U << 19)

unsigned int spm_cpu_bitmask[NR_CPUS] = {
	MP0_CPU0,
	MP0_CPU1,
	MP0_CPU2,
	MP0_CPU3,
	MP1_CPU0,
	MP1_CPU1,
	MP1_CPU2,
	MP1_CPU3,
};

unsigned int spm_cpu_bitmask_all = MP0_CPU0 |
	MP0_CPU1 |
	MP0_CPU2 |
	MP0_CPU3 |
	MP1_CPU0 |
	MP1_CPU1 |
	MP1_CPU2 |
	MP1_CPU2;

/**************************************
 * Function and API
 **************************************/
void __spm_set_cpu_status(int cpu)
{
	spm_write(COMMON_TOP_PWR_ADDR, 0x108C0210);
	spm_write(COMMON_CPU_PWR_ADDR, 0x108C0220);
	spm_write(SPM_SW_NONSERSV_0, 1 << cpu);
	spm_write(SPM_SW_NONSERSV_1, 1 << (cpu + 12));
	if (cpu >= 0 && cpu < 4) {
		spm_write(ARMPLL_CLK_CON,
				(spm_read(ARMPLL_CLK_CON) & ~(MUXSEL_SC_ARMPLL2_LSB | MUXSEL_SC_ARMPLL3_LSB)) |
				(MUXSEL_SC_CCIPLL_LSB | MUXSEL_SC_ARMPLL1_LSB));
	} else if (cpu >= 4 && cpu < 8) {
		spm_write(ARMPLL_CLK_CON,
				(spm_read(ARMPLL_CLK_CON) & ~(MUXSEL_SC_ARMPLL1_LSB | MUXSEL_SC_ARMPLL3_LSB)) |
				(MUXSEL_SC_CCIPLL_LSB | MUXSEL_SC_ARMPLL2_LSB));
	} else {
		spm_crit2("%s: error cpu number %d\n", __func__, cpu);
		BUG();
	}
}

void __spm_reset_and_init_pcm(const struct pcm_desc *pcmdesc)
{
	u32 con1;

#if 0 /* FIXME: enable when F/W release */
/* __spm_sync_vcore_dvfs_power_control */
	int retry = 0, timeout = 5000;

	/* SPM code swapping */
	if (spm_read(PCM_REG1_DATA) == 0x1) {

		con1 = spm_read(SPM_WAKEUP_EVENT_MASK);

		spm_write(SPM_WAKEUP_EVENT_MASK, (con1 & ~(0x1)));
		spm_write(SPM_CPU_WAKEUP_EVENT, 1);

		while ((spm_read(SPM_IRQ_STA) & RG_PCM_IRQ_MSK_LSB) == 0) {
			if (retry > timeout) {
				pr_err("[%s] r15: 0x%x, r6: 0x%x, r1: 0x%x, pcmsta: 0x%x, irqsta: 0x%x [%d]\n",
					__func__,
					spm_read(PCM_REG15_DATA), spm_read(PCM_REG6_DATA), spm_read(PCM_REG1_DATA),
					spm_read(PCM_FSM_STA), spm_read(SPM_IRQ_STA), timeout);
				BUG();
			}
			udelay(1);
			retry++;
		}

		spm_write(SPM_CPU_WAKEUP_EVENT, 0);
		spm_write(SPM_WAKEUP_EVENT_MASK, con1);

		/* backup mem control from r0 to POWER_ON_VAL0 */
		if (spm_read(SPM_POWER_ON_VAL0) != spm_read(PCM_REG0_DATA)) {
			spm_crit("VAL0 from 0x%x to 0x%x\n", spm_read(SPM_POWER_ON_VAL0), spm_read(PCM_REG0_DATA));
			spm_write(SPM_POWER_ON_VAL0, spm_read(PCM_REG0_DATA));
		}

		/* disable r0 and r7 to control power */
		spm_write(PCM_PWR_IO_EN, 0);

		/* disable pcm timer after leaving FW */
		spm_write(PCM_CON1, SPM_REGWR_CFG_KEY | (spm_read(PCM_CON1) & ~RG_PCM_TIMER_EN_LSB));
	}
#endif

	/* reset PCM */
	spm_write(PCM_CON0, SPM_REGWR_CFG_KEY | PCM_CK_EN_LSB | PCM_SW_RESET_LSB);
	spm_write(PCM_CON0, SPM_REGWR_CFG_KEY | PCM_CK_EN_LSB);
	if ((spm_read(PCM_FSM_STA) & 0x7fffff) != PCM_FSM_STA_DEF) {
		spm_crit2("reset pcm(PCM_FSM_STA=0x%x)\n", spm_read(PCM_FSM_STA));
		BUG(); /* PCM reset failed */
	}

	/* init PCM_CON0 (disable event vector) */
	spm_write(PCM_CON0, SPM_REGWR_CFG_KEY | PCM_CK_EN_LSB | RG_EN_IM_SLEEP_DVS_LSB);

	/* init PCM_CON1 (disable PCM timer but keep PCM WDT setting) */
	con1 = spm_read(PCM_CON1) & (RG_PCM_WDT_WAKE_LSB | RG_PCM_WDT_EN_LSB);
	spm_write(PCM_CON1, con1 | SPM_REGWR_CFG_KEY | REG_EVENT_LOCK_EN_LSB |
		  REG_SPM_SRAM_ISOINT_B_LSB | RG_PCM_WDT_EN_LSB |
		  (pcmdesc->replace ? 0 : RG_IM_NONRP_EN_LSB) |
		  RG_AHBMIF_APBEN_LSB | RG_MD32_APB_INTERNAL_EN_LSB);
}

void __spm_kick_im_to_fetch(const struct pcm_desc *pcmdesc)
{
	u32 ptr, len, con0;

	/* tell IM where is PCM code (use slave mode if code existed) */
	if (pcmdesc->base_dma) {
		ptr = pcmdesc->base_dma;
		/* for 4GB mode */
		if (enable_4G())
			MAPPING_DRAM_ACCESS_ADDR(ptr);
	} else {
		ptr = base_va_to_pa(pcmdesc->base);
	}
	len = pcmdesc->size - 1;
	if (spm_read(PCM_IM_PTR) != ptr || spm_read(PCM_IM_LEN) != len || pcmdesc->sess > 2) {
		spm_write(PCM_IM_PTR, ptr);
		spm_write(PCM_IM_LEN, len);
	} else {
		spm_write(PCM_CON1, spm_read(PCM_CON1) | SPM_REGWR_CFG_KEY | RG_IM_SLAVE_LSB);
	}

	/* kick IM to fetch (only toggle IM_KICK) */
	con0 = spm_read(PCM_CON0) & ~(RG_IM_KICK_L_LSB | RG_PCM_KICK_L_LSB);
	spm_write(PCM_CON0, con0 | SPM_REGWR_CFG_KEY | PCM_CK_EN_LSB | RG_IM_KICK_L_LSB);
	spm_write(PCM_CON0, con0 | SPM_REGWR_CFG_KEY | PCM_CK_EN_LSB);
}

void __spm_init_pcm_register(void)
{
	/* init r0 with POWER_ON_VAL0 */
	spm_write(PCM_REG_DATA_INI, spm_read(SPM_POWER_ON_VAL0));
	spm_write(PCM_PWR_IO_EN, PCM_RF_SYNC_R0);
	spm_write(PCM_PWR_IO_EN, 0);

	/* init r7 with POWER_ON_VAL1 */
	spm_write(PCM_REG_DATA_INI, spm_read(SPM_POWER_ON_VAL1));
	spm_write(PCM_PWR_IO_EN, PCM_RF_SYNC_R7);
	spm_write(PCM_PWR_IO_EN, 0);
}

void __spm_init_event_vector(const struct pcm_desc *pcmdesc)
{
	/* init event vector register */
	spm_write(PCM_EVENT_VECTOR0, pcmdesc->vec0);
	spm_write(PCM_EVENT_VECTOR1, pcmdesc->vec1);
	spm_write(PCM_EVENT_VECTOR2, pcmdesc->vec2);
	spm_write(PCM_EVENT_VECTOR3, pcmdesc->vec3);
	spm_write(PCM_EVENT_VECTOR4, pcmdesc->vec4);
	spm_write(PCM_EVENT_VECTOR5, pcmdesc->vec5);
	spm_write(PCM_EVENT_VECTOR6, pcmdesc->vec6);
	spm_write(PCM_EVENT_VECTOR7, pcmdesc->vec7);
	spm_write(PCM_EVENT_VECTOR8, pcmdesc->vec8);
	spm_write(PCM_EVENT_VECTOR9, pcmdesc->vec9);
	spm_write(PCM_EVENT_VECTOR10, pcmdesc->vec10);
	spm_write(PCM_EVENT_VECTOR11, pcmdesc->vec11);
	spm_write(PCM_EVENT_VECTOR12, pcmdesc->vec12);
	spm_write(PCM_EVENT_VECTOR13, pcmdesc->vec13);
	spm_write(PCM_EVENT_VECTOR14, pcmdesc->vec14);
	spm_write(PCM_EVENT_VECTOR15, pcmdesc->vec15);

	/* event vector will be enabled by PCM itself */
}

void __spm_set_power_control(const struct pwr_ctrl *pwrctrl)
{
	/* Auto-gen Start */

	/* SPM_CLK_CON */
	spm_write(SPM_CLK_CON,
		((pwrctrl->reg_srcclken0_ctl & 0x3) << 0) |
		((pwrctrl->reg_srcclken1_ctl & 0x3) << 2) |
		((pwrctrl->reg_spm_lock_infra_dcm & 0x1) << 5) |
		((pwrctrl->reg_srcclken_mask & 0x7) << 6) |
		((pwrctrl->reg_md1_c32rm_en & 0x1) << 9) |
		((pwrctrl->reg_md2_c32rm_en & 0x1) << 10) |
		((pwrctrl->reg_clksq0_sel_ctrl & 0x1) << 11) |
		((pwrctrl->reg_clksq1_sel_ctrl & 0x1) << 12) |
		((pwrctrl->reg_srcclken0_en & 0x1) << 13) |
		((pwrctrl->reg_srcclken1_en & 0x1) << 14) |
		((pwrctrl->reg_sysclk0_src_mask_b & 0x7f) << 16) |
		((pwrctrl->reg_sysclk1_src_mask_b & 0x7f) << 23));

	/* SPM_AP_STANDBY_CON */
	spm_write(SPM_AP_STANDBY_CON,
		((pwrctrl->reg_mpwfi_op & 0x1) << 0) |
		((pwrctrl->reg_mp0_cputop_idle_mask & 0x1) << 1) |
		((pwrctrl->reg_mp1_cputop_idle_mask & 0x1) << 2) |
		((pwrctrl->reg_debugtop_idle_mask & 0x1) << 3) |
		((pwrctrl->reg_mp_top_idle_mask & 0x1) << 5) |
		((pwrctrl->reg_mcusys_idle_mask & 0x1) << 6) |
		((pwrctrl->reg_md_ddr_en_0_dbc_en & 0x1) << 16) |
		((pwrctrl->reg_md_ddr_en_1_dbc_en & 0x1) << 17) |
		((pwrctrl->reg_conn_ddr_en_dbc_en & 0x1) << 18) |
		((pwrctrl->reg_md32_mask_b & 0x1) << 19) |
		((pwrctrl->reg_md_0_mask_b & 0x1) << 20) |
		((pwrctrl->reg_md_1_mask_b & 0x1) << 21) |
		((pwrctrl->reg_scp_mask_b & 0x1) << 22) |
		((pwrctrl->reg_srcclkeni0_mask_b & 0x1) << 23) |
		((pwrctrl->reg_srcclkeni1_mask_b & 0x1) << 24) |
		((pwrctrl->reg_md_apsrc_1_sel & 0x1) << 25) |
		((pwrctrl->reg_md_apsrc_0_sel & 0x1) << 26) |
		((pwrctrl->reg_conn_mask_b & 0x1) << 27) |
		((pwrctrl->reg_conn_apsrc_sel & 0x1) << 28));

	/* SPM_SRC_REQ */
	spm_write(SPM_SRC_REQ,
		((pwrctrl->reg_spm_apsrc_req & 0x1) << 0) |
		((pwrctrl->reg_spm_f26m_req & 0x1) << 1) |
		((pwrctrl->reg_spm_infra_req & 0x1) << 2) |
		((pwrctrl->reg_spm_ddren_req & 0x1) << 3) |
		((pwrctrl->reg_spm_vrf18_req & 0x1) << 4) |
		((pwrctrl->reg_spm_dvfs_level0_req & 0x1) << 5) |
		((pwrctrl->reg_spm_dvfs_level1_req & 0x1) << 6) |
		((pwrctrl->reg_spm_dvfs_level2_req & 0x1) << 7) |
		((pwrctrl->reg_spm_dvfs_level3_req & 0x1) << 8) |
		((pwrctrl->reg_spm_dvfs_level4_req & 0x1) << 9) |
		((pwrctrl->reg_spm_pmcu_mailbox_req & 0x1) << 10) |
		((pwrctrl->reg_spm_sw_mailbox_req & 0x1) << 11) |
		((pwrctrl->reg_spm_cksel2_req & 0x1) << 12) |
		((pwrctrl->reg_spm_cksel3_req & 0x1) << 13));

	/* SPM_SRC_MASK */
	spm_write(SPM_SRC_MASK,
		((pwrctrl->reg_csyspwreq_mask & 0x1) << 0) |
		((pwrctrl->reg_md_srcclkena_0_infra_mask_b & 0x1) << 1) |
		((pwrctrl->reg_md_srcclkena_1_infra_mask_b & 0x1) << 2) |
		((pwrctrl->reg_md_apsrc_req_0_infra_mask_b & 0x1) << 3) |
		((pwrctrl->reg_md_apsrc_req_1_infra_mask_b & 0x1) << 4) |
		((pwrctrl->reg_conn_srcclkena_infra_mask_b & 0x1) << 5) |
		((pwrctrl->reg_conn_infra_req_mask_b & 0x1) << 6) |
		((pwrctrl->reg_md32_srcclkena_infra_mask_b & 0x1) << 7) |
		((pwrctrl->reg_md32_infra_req_mask_b & 0x1) << 8) |
		((pwrctrl->reg_scp_srcclkena_infra_mask_b & 0x1) << 9) |
		((pwrctrl->reg_scp_infra_req_mask_b & 0x1) << 10) |
		((pwrctrl->reg_srcclkeni0_infra_mask_b & 0x1) << 11) |
		((pwrctrl->reg_srcclkeni1_infra_mask_b & 0x1) << 12) |
		((pwrctrl->reg_ccif0_md_event_mask_b & 0x1) << 13) |
		((pwrctrl->reg_ccif0_ap_event_mask_b & 0x1) << 14) |
		((pwrctrl->reg_ccif1_md_event_mask_b & 0x1) << 15) |
		((pwrctrl->reg_ccif1_ap_event_mask_b & 0x1) << 16) |
		((pwrctrl->reg_ccif2_md_event_mask_b & 0x1) << 17) |
		((pwrctrl->reg_ccif2_ap_event_mask_b & 0x1) << 18) |
		((pwrctrl->reg_ccif3_md_event_mask_b & 0x1) << 19) |
		((pwrctrl->reg_ccif3_ap_event_mask_b & 0x1) << 20) |
		((pwrctrl->reg_ccifmd_md1_event_mask_b & 0x1) << 21) |
		((pwrctrl->reg_ccifmd_md2_event_mask_b & 0x1) << 22) |
		((pwrctrl->reg_c2k_ps_rccif_wake_mask_b & 0x1) << 23) |
		((pwrctrl->reg_c2k_l1_rccif_wake_mask_b & 0x1) << 24) |
		((pwrctrl->reg_ps_c2k_rccif_wake_mask_b & 0x1) << 25) |
		((pwrctrl->reg_l1_c2k_rccif_wake_mask_b & 0x1) << 26) |
		((pwrctrl->reg_dqssoc_req_mask_b & 0x1) << 27) |
		((pwrctrl->reg_disp2_req_mask_b & 0x1) << 28) |
		((pwrctrl->reg_md_ddr_en_0_mask_b & 0x1) << 29) |
		((pwrctrl->reg_md_ddr_en_1_mask_b & 0x1) << 30) |
		((pwrctrl->reg_conn_ddr_en_mask_b & 0x1) << 31));

	/* SPM_SRC2_MASK */
	spm_write(SPM_SRC2_MASK,
		((pwrctrl->reg_disp0_req_mask_b & 0x1) << 0) |
		((pwrctrl->reg_disp1_req_mask_b & 0x1) << 1) |
		((pwrctrl->reg_disp_od_req_mask_b & 0x1) << 2) |
		((pwrctrl->reg_mfg_req_mask_b & 0x1) << 3) |
		((pwrctrl->reg_vdec0_req_mask_b & 0x1) << 4) |
		((pwrctrl->reg_gce_vrf18_req_mask_b & 0x1) << 6) |
		((pwrctrl->reg_gce_req_mask_b & 0x1) << 7) |
		((pwrctrl->reg_lpdma_req_mask_b & 0x1) << 8) |
		((pwrctrl->reg_srcclkeni1_cksel2_mask_b & 0x1) << 9) |
		((pwrctrl->reg_conn_srcclkena_cksel2_mask_b & 0x1) << 10) |
		((pwrctrl->reg_srcclkeni0_cksel3_mask_b & 0x1) << 11) |
		((pwrctrl->reg_md32_apsrc_req_ddren_mask_b & 0x1) << 12) |
		((pwrctrl->reg_scp_apsrc_req_ddren_mask_b & 0x1) << 13) |
		((pwrctrl->reg_md_vrf18_req_0_mask_b & 0x1) << 14) |
		((pwrctrl->reg_md_vrf18_req_1_mask_b & 0x1) << 15) |
		((pwrctrl->reg_next_dvfs_level0_mask_b & 0x1) << 16) |
		((pwrctrl->reg_next_dvfs_level1_mask_b & 0x1) << 17) |
		((pwrctrl->reg_next_dvfs_level2_mask_b & 0x1) << 18) |
		((pwrctrl->reg_next_dvfs_level3_mask_b & 0x1) << 19) |
		((pwrctrl->reg_next_dvfs_level4_mask_b & 0x1) << 20) |
		((pwrctrl->reg_msdc1_dvfs_halt_mask & 0x1) << 21) |
		((pwrctrl->reg_msdc2_dvfs_halt_mask & 0x1) << 22) |
		((pwrctrl->reg_msdc3_dvfs_halt_mask & 0x1) << 23) |
		((pwrctrl->reg_sw2spm_int0_mask_b & 0x1) << 24) |
		((pwrctrl->reg_sw2spm_int1_mask_b & 0x1) << 25) |
		((pwrctrl->reg_sw2spm_int2_mask_b & 0x1) << 26) |
		((pwrctrl->reg_sw2spm_int3_mask_b & 0x1) << 27) |
		((pwrctrl->reg_pmcu2spm_int0_mask_b & 0x1) << 28) |
		((pwrctrl->reg_pmcu2spm_int1_mask_b & 0x1) << 29) |
		((pwrctrl->reg_pmcu2spm_int2_mask_b & 0x1) << 30) |
		((pwrctrl->reg_pmcu2spm_int3_mask_b & 0x1) << 31));

	/* SPM_WAKEUP_EVENT_MASK */
	spm_write(SPM_WAKEUP_EVENT_MASK,
		((pwrctrl->reg_wakeup_event_mask & 0xffffffff) << 0));

	/* SPM_EXT_WAKEUP_EVENT_MASK */
	spm_write(SPM_EXT_WAKEUP_EVENT_MASK,
		((pwrctrl->reg_ext_wakeup_event_mask & 0xffffffff) << 0));

	/* MP0_CPU0_WFI_EN */
	spm_write(MP0_CPU0_WFI_EN,
		((pwrctrl->mp0_cpu0_wfi_en & 0x1) << 0));

	/* MP0_CPU1_WFI_EN */
	spm_write(MP0_CPU1_WFI_EN,
		((pwrctrl->mp0_cpu1_wfi_en & 0x1) << 0));

	/* MP0_CPU2_WFI_EN */
	spm_write(MP0_CPU2_WFI_EN,
		((pwrctrl->mp0_cpu2_wfi_en & 0x1) << 0));

	/* MP0_CPU3_WFI_EN */
	spm_write(MP0_CPU3_WFI_EN,
		((pwrctrl->mp0_cpu3_wfi_en & 0x1) << 0));

	/* MP1_CPU0_WFI_EN */
	spm_write(MP1_CPU0_WFI_EN,
		((pwrctrl->mp1_cpu0_wfi_en & 0x1) << 0));

	/* MP1_CPU1_WFI_EN */
	spm_write(MP1_CPU1_WFI_EN,
		((pwrctrl->mp1_cpu1_wfi_en & 0x1) << 0));

	/* MP1_CPU2_WFI_EN */
	spm_write(MP1_CPU2_WFI_EN,
		((pwrctrl->mp1_cpu2_wfi_en & 0x1) << 0));

	/* MP1_CPU3_WFI_EN */
	spm_write(MP1_CPU3_WFI_EN,
		((pwrctrl->mp1_cpu3_wfi_en & 0x1) << 0));

	/* DEBUG0_WFI_EN */
	spm_write(DEBUG0_WFI_EN,
		((pwrctrl->debug0_wfi_en & 0x1) << 0));

	/* DEBUG1_WFI_EN */
	spm_write(DEBUG1_WFI_EN,
		((pwrctrl->debug1_wfi_en & 0x1) << 0));

	/* DEBUG2_WFI_EN */
	spm_write(DEBUG2_WFI_EN,
		((pwrctrl->debug2_wfi_en & 0x1) << 0));

	/* DEBUG3_WFI_EN */
	spm_write(DEBUG3_WFI_EN,
		((pwrctrl->debug3_wfi_en & 0x1) << 0));
	/* Auto-gen End */
}

void __spm_set_vcorefs_wakeup_event(const struct pwr_ctrl *src_pwr_ctrl)
{
	u32 mask;

	mask = src_pwr_ctrl->wake_src;

	spm_write(SPM_WAKEUP_EVENT_MASK, ~mask);
}

void __spm_set_wakeup_event(const struct pwr_ctrl *pwrctrl)
{
	u32 val, mask, isr;

	/* set PCM timer (set to max when disable) */
	if (pwrctrl->timer_val_ramp_en != 0) {
		val = pcm_timer_ramp_max;

		pcm_timer_ramp_max++;

		if (pcm_timer_ramp_max >= 300)
			pcm_timer_ramp_max = 1;
	} else if (pwrctrl->timer_val_ramp_en_sec != 0) {
		val = pcm_timer_ramp_max * 1600;	/* 50ms */

		pcm_timer_ramp_max += 1;
		if (pcm_timer_ramp_max >= 300)	/* max 15 sec */
			pcm_timer_ramp_max = 1;

		pcm_timer_ramp_max_sec_loop++;
		if (pcm_timer_ramp_max_sec_loop >= 50) {
			pcm_timer_ramp_max_sec_loop = 0;
			/* range 6min to 10min */
			val = (pcm_timer_ramp_max + 300) * 32000;
		}
	} else {
		if (pwrctrl->timer_val_cust == 0)
			val = pwrctrl->timer_val ? : PCM_TIMER_MAX;
		else
			val = pwrctrl->timer_val_cust;
	}

	spm_write(PCM_TIMER_VAL, val);
	spm_write(PCM_CON1, spm_read(PCM_CON1) | SPM_REGWR_CFG_KEY | RG_PCM_TIMER_EN_LSB);

	/* unmask AP wakeup source */
	if (pwrctrl->wake_src_cust == 0)
		mask = pwrctrl->wake_src;
	else
		mask = pwrctrl->wake_src_cust;

	if (pwrctrl->syspwreq_mask)
		mask &= ~WAKE_SRC_R12_CSYSPWREQ_B;
	spm_write(SPM_WAKEUP_EVENT_MASK, ~mask);

#if 0
	/* unmask MD32 wakeup source */
	spm_write(SPM_SLEEP_MD32_WAKEUP_EVENT_MASK, ~pwrctrl->wake_src_md32);
#endif

	/* unmask SPM ISR (keep TWAM setting) */
	isr = spm_read(SPM_IRQ_MASK) & REG_TWAM_IRQ_MASK_LSB;
	spm_write(SPM_IRQ_MASK, isr | ISRM_RET_IRQ_AUX);
}

void __spm_kick_pcm_to_run(struct pwr_ctrl *pwrctrl)
{
	u32 con0;

	/* init register to match PCM expectation */
	spm_write(SPM_MAS_PAUSE_MASK_B, 0xffffffff);
	spm_write(SPM_MAS_PAUSE2_MASK_B, 0xffffffff);
	spm_write(PCM_REG_DATA_INI, 0);

	/* set PCM flags and data */
	if (pwrctrl->pcm_flags_cust_set != 0)
		pwrctrl->pcm_flags |= pwrctrl->pcm_flags_cust_set;
	if (pwrctrl->pcm_flags_cust_clr != 0)
		pwrctrl->pcm_flags &= ~pwrctrl->pcm_flags_cust_clr;
	spm_write(SPM_SW_FLAG, pwrctrl->pcm_flags);
	spm_write(SPM_SW_RSV_0, pwrctrl->pcm_reserve);

	/* enable r0 and r7 to control power */
	spm_write(PCM_PWR_IO_EN, PCM_PWRIO_EN_R0 | PCM_PWRIO_EN_R7);

	/* kick PCM to run (only toggle PCM_KICK) */
	con0 = spm_read(PCM_CON0) & ~(RG_IM_KICK_L_LSB | RG_PCM_KICK_L_LSB);
	spm_write(PCM_CON0, con0 | SPM_REGWR_CFG_KEY | PCM_CK_EN_LSB | RG_PCM_KICK_L_LSB);
	spm_write(PCM_CON0, con0 | SPM_REGWR_CFG_KEY | PCM_CK_EN_LSB);
}

void __spm_get_wakeup_status(struct wake_status *wakesta)
{
	/* get PC value if PCM assert (pause abort) */
	wakesta->assert_pc = spm_read(PCM_REG_DATA_INI);

	/* get wakeup event */
	wakesta->r12 = spm_read(SPM_SW_RSV_0);        /* backup of PCM_REG12_DATA */
	wakesta->r12 = spm_read(PCM_REG12_DATA);
	wakesta->r12_ext = spm_read(PCM_REG12_EXT_DATA);
	wakesta->raw_sta = spm_read(SPM_WAKEUP_STA);
	wakesta->raw_ext_sta = spm_read(SPM_WAKEUP_EXT_STA);
	wakesta->wake_misc = spm_read(SPM_BSI_D0_SR);	/* backup of SPM_WAKEUP_MISC */
	wakesta->wake_misc = spm_read(SPM_WAKEUP_MISC);

	/* get sleep time */
	wakesta->timer_out = spm_read(SPM_BSI_D1_SR);	/* backup of PCM_TIMER_OUT */
	wakesta->timer_out = spm_read(PCM_TIMER_OUT);

	/* get other SYS and co-clock status */
	wakesta->r13 = spm_read(PCM_REG13_DATA);
	wakesta->idle_sta = spm_read(SUBSYS_IDLE_STA);

	/* get debug flag for PCM execution check */
	wakesta->debug_flag = spm_read(SPM_SW_DEBUG);

	/* get special pattern (0xf0000 or 0x10000) if sleep abort */
	wakesta->event_reg = spm_read(SPM_BSI_D2_SR);	/* PCM_EVENT_REG_STA */
	wakesta->event_reg = spm_read(PCM_EVENT_REG_STA);

	/* get ISR status */
	wakesta->isr = spm_read(SPM_IRQ_STA);
}

void __spm_clean_after_wakeup(void)
{
	/* [Vcorefs] can not switch back to POWER_ON_VAL0 here,
	   the FW stays in VCORE DVFS which use r0 to Ctrl MEM */
	/* disable r0 and r7 to control power */
	/* spm_write(PCM_PWR_IO_EN, 0); */

	/* clean CPU wakeup event */
	spm_write(SPM_CPU_WAKEUP_EVENT, 0);

	/* [Vcorefs] not disable pcm timer here, due to the
	   following vcore dvfs will use it for latency check */
	/* clean PCM timer event */
	/* spm_write(PCM_CON1, SPM_REGWR_CFG_KEY | (spm_read(PCM_CON1) & ~RG_PCM_TIMER_EN_LSB)); */

	/* clean wakeup event raw status (for edge trigger event) */
	spm_write(SPM_WAKEUP_EVENT_MASK, ~0);

	/* clean ISR status (except TWAM) */
	spm_write(SPM_IRQ_MASK, spm_read(SPM_IRQ_MASK) | ISRM_ALL_EXC_TWAM);
	spm_write(SPM_IRQ_STA, ISRC_ALL_EXC_TWAM);
	spm_write(SPM_SWINT_CLR, PCM_SW_INT_ALL);
}

#define spm_print(suspend, fmt, args...)	\
do {						\
	if (!suspend)				\
		spm_debug(fmt, ##args);		\
	else					\
		spm_crit2(fmt, ##args);		\
} while (0)

wake_reason_t __spm_output_wake_reason(const struct wake_status *wakesta,
				       const struct pcm_desc *pcmdesc, bool suspend)
{
	int i;
	char buf[LOG_BUF_SIZE] = { 0 };
	wake_reason_t wr = WR_UNKNOWN;

	if (wakesta->assert_pc != 0) {
		/* add size check for vcoredvfs */
		spm_print(suspend, "PCM ASSERT AT %u (%s%s), r13 = 0x%x, debug_flag = 0x%x\n",
			  wakesta->assert_pc, (wakesta->assert_pc > pcmdesc->size) ? "NOT " : "",
			  pcmdesc->version, wakesta->r13, wakesta->debug_flag);
		return WR_PCM_ASSERT;
	}

	if (wakesta->r12 & WAKE_SRC_R12_PCM_TIMER) {
		if (wakesta->wake_misc & WAKE_MISC_PCM_TIMER) {
			strcat(buf, " PCM_TIMER");
			wr = WR_PCM_TIMER;
		}
		if (wakesta->wake_misc & WAKE_MISC_TWAM) {
			strcat(buf, " TWAM");
			wr = WR_WAKE_SRC;
		}
		if (wakesta->wake_misc & WAKE_MISC_CPU_WAKE) {
			strcat(buf, " CPU");
			wr = WR_WAKE_SRC;
		}
	}
	for (i = 1; i < 32; i++) {
		if (wakesta->r12 & (1U << i)) {
			if ((strlen(buf) + strlen(wakesrc_str[i])) < LOG_BUF_SIZE)
				strncat(buf, wakesrc_str[i], strlen(wakesrc_str[i]));

			wr = WR_WAKE_SRC;
		}
	}
	BUG_ON(strlen(buf) >= LOG_BUF_SIZE);

	spm_print(suspend, "wake up by %s, timer_out = %u, r13 = 0x%x, debug_flag = 0x%x\n",
		  buf, wakesta->timer_out, wakesta->r13, wakesta->debug_flag);

	spm_print(suspend,
		  "r12 = 0x%x, r12_ext = 0x%x, raw_sta = 0x%x, idle_sta = 0x%x, event_reg = 0x%x, isr = 0x%x\n",
		  wakesta->r12, wakesta->r12_ext, wakesta->raw_sta, wakesta->idle_sta,
		  wakesta->event_reg, wakesta->isr);

	spm_print(suspend, "raw_ext_sta = 0x%x, wake_misc = 0x%x", wakesta->raw_ext_sta,
		  wakesta->wake_misc);
	return wr;
}

unsigned int spm_get_cpu_pwr_status(void)
{
	unsigned int pwr_stat[2] = { 0 };
	unsigned int stat = 0;
	unsigned int ret_stat = 0;
	int i;

	pwr_stat[0] = spm_read(PWR_STATUS);
	pwr_stat[1] = spm_read(PWR_STATUS_2ND);

	stat = (pwr_stat[0] & spm_cpu_bitmask_all) & (pwr_stat[1] & spm_cpu_bitmask_all);

	for (i = 0; i < nr_cpu_ids; i++)
		if (stat & spm_cpu_bitmask[i])
			ret_stat |= (1 << i);

	return ret_stat;
}

long int spm_get_current_time_ms(void)
{
	struct timeval t;

	do_gettimeofday(&t);
	return ((t.tv_sec & 0xFFF) * 1000000 + t.tv_usec) / 1000;
}

void __spm_sync_vcore_dvfs_power_control(struct pwr_ctrl *dest_pwr_ctrl, const struct pwr_ctrl *src_pwr_ctrl)
{
	/* FIXME: */
#if 0
	/* pwr_ctrl for mask/ctrl register */
	dest_pwr_ctrl->dvfs_halt_mask_b			= src_pwr_ctrl->dvfs_halt_mask_b;
	dest_pwr_ctrl->sdio_on_dvfs_req_mask_b		= src_pwr_ctrl->sdio_on_dvfs_req_mask_b;
	dest_pwr_ctrl->cpu_md_dvfs_erq_merge_mask_b	= src_pwr_ctrl->cpu_md_dvfs_erq_merge_mask_b;
	dest_pwr_ctrl->md1_ddr_en_dvfs_halt_mask_b	= src_pwr_ctrl->md1_ddr_en_dvfs_halt_mask_b;
	dest_pwr_ctrl->md2_ddr_en_dvfs_halt_mask_b	= src_pwr_ctrl->md2_ddr_en_dvfs_halt_mask_b;
	dest_pwr_ctrl->md_srcclkena_0_dvfs_req_mask_b	= src_pwr_ctrl->md_srcclkena_0_dvfs_req_mask_b;
	dest_pwr_ctrl->md_srcclkena_1_dvfs_req_mask_b	= src_pwr_ctrl->md_srcclkena_1_dvfs_req_mask_b;
	dest_pwr_ctrl->conn_srcclkena_dvfs_req_mask_b	= src_pwr_ctrl->conn_srcclkena_dvfs_req_mask_b;

	dest_pwr_ctrl->vsync_dvfs_halt_mask_b		= src_pwr_ctrl->vsync_dvfs_halt_mask_b;
	dest_pwr_ctrl->emi_boost_dvfs_req_mask_b	= src_pwr_ctrl->emi_boost_dvfs_req_mask_b;
	dest_pwr_ctrl->emi_bw_dvfs_req_mask		= src_pwr_ctrl->emi_bw_dvfs_req_mask;
	dest_pwr_ctrl->cpu_md_emi_dvfs_req_prot_dis	= src_pwr_ctrl->cpu_md_emi_dvfs_req_prot_dis;
	dest_pwr_ctrl->spm_dvfs_req			= src_pwr_ctrl->spm_dvfs_req;
	dest_pwr_ctrl->spm_dvfs_force_down		= src_pwr_ctrl->spm_dvfs_force_down;
	dest_pwr_ctrl->cpu_md_dvfs_sop_force_on		= src_pwr_ctrl->cpu_md_dvfs_sop_force_on;
	dest_pwr_ctrl->dvfs_halt_src_chk = src_pwr_ctrl->dvfs_halt_src_chk;
	/* pwr_ctrl pcm_flag */
	if (src_pwr_ctrl->pcm_flags_cust != 0) {
		if ((src_pwr_ctrl->pcm_flags_cust & SPM_FLAG_DIS_VCORE_DVS) != 0)
			dest_pwr_ctrl->pcm_flags |= SPM_FLAG_DIS_VCORE_DVS;

		if ((src_pwr_ctrl->pcm_flags_cust & SPM_FLAG_DIS_VCORE_DFS) != 0)
			dest_pwr_ctrl->pcm_flags |= SPM_FLAG_DIS_VCORE_DFS;

		if ((src_pwr_ctrl->pcm_flags_cust & SPM_FLAG_RESERVED_BIT11) != 0)
			dest_pwr_ctrl->pcm_flags |= SPM_FLAG_RESERVED_BIT11;
	} else {
		if ((src_pwr_ctrl->pcm_flags & SPM_FLAG_DIS_VCORE_DVS) != 0)
			dest_pwr_ctrl->pcm_flags |= SPM_FLAG_DIS_VCORE_DVS;

		if ((src_pwr_ctrl->pcm_flags & SPM_FLAG_DIS_VCORE_DFS) != 0)
			dest_pwr_ctrl->pcm_flags |= SPM_FLAG_DIS_VCORE_DFS;

		if ((src_pwr_ctrl->pcm_flags & SPM_FLAG_RESERVED_BIT11) != 0)
			dest_pwr_ctrl->pcm_flags |= SPM_FLAG_RESERVED_BIT11;
	}
#endif
}

bool is_md_c2k_conn_power_off(void)
{
	u32 md1_pwr_con = 0;
	u32 c2k_pwr_con = 0;
	u32 conn_pwr_con = 0;

	md1_pwr_con = spm_read(MD1_PWR_CON);
	c2k_pwr_con = spm_read(C2K_PWR_CON);
	conn_pwr_con = spm_read(CONN_PWR_CON);

#if 0
	pr_err("md1_pwr_con = 0x%08x, c2k_pwr_con = 0x%08x, conn_pwr_con = 0x%08x\n",
	       md1_pwr_con, c2k_pwr_con, conn_pwr_con);
#endif

	if (!((md1_pwr_con & 0x1F) == 0x12))
		return false;

	if (!((c2k_pwr_con & 0x1F) == 0x12))
		return false;

	if (!((conn_pwr_con & 0x1F) == 0x12))
		return false;

	return true;
}

void __spm_set_pcm_wdt(int en)
{
	/* enable PCM WDT (normal mode) to start count if needed */
	if (en) {
		u32 con1;

		con1 = spm_read(PCM_CON1) & ~(RG_PCM_WDT_WAKE_LSB | RG_PCM_WDT_EN_LSB);
		spm_write(PCM_CON1, SPM_REGWR_CFG_KEY | con1);

		if (spm_read(PCM_TIMER_VAL) > PCM_TIMER_MAX)
			spm_write(PCM_TIMER_VAL, PCM_TIMER_MAX);
		spm_write(PCM_WDT_VAL, spm_read(PCM_TIMER_VAL) + PCM_WDT_TIMEOUT);
		spm_write(PCM_CON1, con1 | SPM_REGWR_CFG_KEY | RG_PCM_WDT_EN_LSB);
	} else {
		spm_write(PCM_CON1, SPM_REGWR_CFG_KEY | (spm_read(PCM_CON1) &
		~RG_PCM_WDT_EN_LSB));
	}

}

int __attribute__ ((weak)) get_dynamic_period(int first_use, int first_wakeup_time,
					      int battery_capacity_level)
{
	return 60;
}

u32 _spm_get_wake_period(int pwake_time, wake_reason_t last_wr)
{
	int period = SPM_WAKE_PERIOD;

	if (pwake_time < 0) {
		/* use FG to get the period of 1% battery decrease */
		period = get_dynamic_period(last_wr != WR_PCM_TIMER ? 1 : 0, SPM_WAKE_PERIOD, 1);
		if (period <= 0) {
			spm_warn("CANNOT GET PERIOD FROM FUEL GAUGE\n");
			period = SPM_WAKE_PERIOD;
		}
	} else {
		period = pwake_time;
		spm_crit2("pwake = %d\n", pwake_time);
	}

	if (period > 36 * 3600)	/* max period is 36.4 hours */
		period = 36 * 3600;

	return period;
}

MODULE_DESCRIPTION("SPM-Internal Driver v0.1");
