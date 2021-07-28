// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2021 MediaTek Inc.
// Author: Wendell Lin <wendell.lin@mediatek.com>

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "clk-fmeter.h"
#include "clkchk.h"
#include "clk-mt6983-fmeter.h"

#define FM_TIMEOUT		30
#define SUBSYS_PLL_NUM		4

static DEFINE_SPINLOCK(meter_lock);
#define fmeter_lock(flags)   spin_lock_irqsave(&meter_lock, flags)
#define fmeter_unlock(flags) spin_unlock_irqrestore(&meter_lock, flags)

static DEFINE_SPINLOCK(subsys_meter_lock);
#define subsys_fmeter_lock(flags)   spin_lock_irqsave(&subsys_meter_lock, flags)
#define subsys_fmeter_unlock(flags) spin_unlock_irqrestore(&subsys_meter_lock, flags)

/*
 * clk fmeter
 */

#define clk_readl(addr)		readl(addr)
#define clk_writel(addr, val)	\
	do { writel(val, addr); wmb(); } while (0) /* sync write */

#define FMCLK3(_t, _i, _n, _o, _g, _c) { .type = _t, \
		.id = _i, .name = _n, .ofs = _o, .grp = _g, .ck_div = _c}
#define FMCLK2(_t, _i, _n, _o, _p, _c) { .type = _t, \
		.id = _i, .name = _n, .ofs = _o, .pdn = _p, .ck_div = _c}
#define FMCLK(_t, _i, _n, _c) { .type = _t, .id = _i, .name = _n, .ck_div = _c}

static struct fmeter_clk *fm_all_clks;
static struct fm_subsys *fm_sub_clks;
static unsigned int fm_clk_cnt;

static const struct fmeter_clk fclks[] = {
	/* CKGEN Part */
	FMCLK2(CKGEN, hd_faxi_ck, "hd_faxi_ck", 0x0010, 7, 1),
	FMCLK2(CKGEN, f_fperi_hd_faxi_ck, "f_fperi_hd_faxi_ck", 0x0010, 15, 1),
	FMCLK2(CKGEN, f_fufs_hd_haxi_ck, "f_fufs_hd_haxi_ck", 0x0010, 23, 1),
	FMCLK2(CKGEN, hd_fbus_aximem_ck, "hd_fbus_aximem_ck", 0x0010, 31, 1),
	FMCLK2(CKGEN, hf_fdisp0_ck, "hf_fdisp0_ck", 0x0020, 7, 1),
	FMCLK2(CKGEN, hf_fdisp1_ck, "hf_fdisp1_ck", 0x0020, 15, 1),
	FMCLK2(CKGEN, hf_fmdp0_ck, "hf_fmdp0_ck", 0x0020, 23, 1),
	FMCLK2(CKGEN, hf_fmdp1_ck, "hf_fmdp1_ck", 0x0020, 31, 1),
	FMCLK2(CKGEN, f_fmminfra_ck, "f_fmminfra_ck", 0x0030, 7, 1),
	FMCLK2(CKGEN, f_fmmup_ck, "f_fmmup_ck", 0x0030, 15, 1),
	FMCLK2(CKGEN, hf_fdsp_ck, "hf_fdsp_ck", 0x0030, 23, 1),
	FMCLK2(CKGEN, hf_fdsp1_ck, "hf_fdsp1_ck", 0x0030, 31, 1),
	FMCLK2(CKGEN, hf_fdsp2_ck, "hf_fdsp2_ck", 0x0040, 7, 1),
	FMCLK2(CKGEN, hf_fdsp3_ck, "hf_fdsp3_ck", 0x0040, 15, 1),
	FMCLK2(CKGEN, hf_fdsp4_ck, "hf_fdsp4_ck", 0x0040, 23, 1),
	FMCLK2(CKGEN, hf_fdsp5_ck, "hf_fdsp5_ck", 0x0040, 31, 1),
	FMCLK2(CKGEN, hf_fdsp6_ck, "hf_fdsp6_ck", 0x0050, 7, 1),
	FMCLK2(CKGEN, hf_fdsp7_ck, "hf_fdsp7_ck", 0x0050, 15, 1),
	FMCLK2(CKGEN, hf_fipu_if_ck, "hf_fipu_if_ck", 0x0050, 23, 1),
	FMCLK2(CKGEN, hf_fmfg_ref_ck, "hf_fmfg_ref_ck", 0x0050, 31, 1),
	FMCLK2(CKGEN, hf_fmfgsc_ref_ck, "hf_fmfgsc_ref_ck", 0x0060, 7, 1),
	FMCLK2(CKGEN, f_fcamtg_ck, "f_fcamtg_ck", 0x0060, 15, 1),
	FMCLK2(CKGEN, f_fcamtg2_ck, "f_fcamtg2_ck", 0x0060, 23, 1),
	FMCLK2(CKGEN, f_fcamtg3_ck, "f_fcamtg3_ck", 0x0060, 31, 1),
	FMCLK2(CKGEN, f_fcamtg4_ck, "f_fcamtg4_ck", 0x0070, 7, 1),
	FMCLK2(CKGEN, f_fcamtg5_ck, "f_fcamtg5_ck", 0x0070, 15, 1),
	FMCLK2(CKGEN, f_fcamtg6_ck, "f_fcamtg6_ck", 0x0070, 23, 1),
	FMCLK2(CKGEN, f_fcamtg7_ck, "f_fcamtg7_ck", 0x0070, 31, 1),
	FMCLK2(CKGEN, f_fcamtg8_ck, "f_fcamtg8_ck", 0x0080, 7, 1),
	FMCLK2(CKGEN, f_fuart_ck, "f_fuart_ck", 0x0080, 15, 1),
	FMCLK2(CKGEN, hf_fspi_ck, "hf_fspi_ck", 0x0080, 23, 1),
	FMCLK2(CKGEN, hf_fmsdc50_0_hclk_ck, "hf_fmsdc50_0_hclk_ck", 0x0080, 31, 1),
	FMCLK2(CKGEN, hf_fmsdc_macro_ck, "hf_fmsdc_macro_ck", 0x0090, 7, 1),
	FMCLK2(CKGEN, hf_fmsdc30_1_ck, "hf_fmsdc30_1_ck", 0x0090, 15, 1),
	FMCLK2(CKGEN, hf_fmsdc30_2_ck, "hf_fmsdc30_2_ck", 0x0090, 23, 1),
	FMCLK2(CKGEN, hf_faudio_ck, "hf_faudio_ck", 0x0090, 31, 1),
	FMCLK2(CKGEN, hf_faud_intbus_ck, "hf_faud_intbus_ck", 0x00A0, 7, 1),
	FMCLK2(CKGEN, f_fpwrap_ulposc_ck, "f_fpwrap_ulposc_ck", 0x00A0, 15, 1),
	FMCLK2(CKGEN, hf_fatb_ck, "hf_fatb_ck", 0x00A0, 23, 1),
	FMCLK2(CKGEN, hf_fdp_ck, "hf_fdp_ck", 0x00A0, 31, 1),
	FMCLK2(CKGEN, f_fdisp_pwm_ck, "f_fdisp_pwm_ck", 0x00B0, 7, 1),
	FMCLK2(CKGEN, f_fusb_top_ck, "f_fusb_top_ck", 0x00B0, 15, 1),
	FMCLK2(CKGEN, f_fssusb_xhci_ck, "f_fssusb_xhci_ck", 0x00B0, 23, 1),
	FMCLK2(CKGEN, f_fusb_top_1p_ck, "f_fusb_top_1p_ck", 0x00B0, 31, 1),
	FMCLK2(CKGEN, f_fssusb_xhci_1p_ck, "f_fssusb_xhci_1p_ck", 0x00C0, 7, 1),
	FMCLK2(CKGEN, f_fi2c_ck, "f_fi2c_ck", 0x00C0, 15, 1),
	FMCLK2(CKGEN, f_fseninf_ck, "f_fseninf_ck", 0x00C0, 23, 1),
	FMCLK2(CKGEN, f_fseninf1_ck, "f_fseninf1_ck", 0x00C0, 31, 1),
	FMCLK2(CKGEN, f_fseninf2_ck, "f_fseninf2_ck", 0x00D0, 7, 1),
	FMCLK2(CKGEN, f_fseninf3_ck, "f_fseninf3_ck", 0x00D0, 15, 1),
	FMCLK2(CKGEN, f_fseninf4_ck, "f_fseninf4_ck", 0x00D0, 23, 1),
	FMCLK2(CKGEN, f_fseninf5_ck, "f_fseninf5_ck", 0x00D0, 31, 1),
	FMCLK2(CKGEN, hf_fdxcc_ck, "hf_fdxcc_ck", 0x00E0, 7, 1),
	FMCLK2(CKGEN, hf_faud_engen1_ck, "hf_faud_engen1_ck", 0x00E0, 15, 1),
	FMCLK2(CKGEN, hf_faud_engen2_ck, "hf_faud_engen2_ck", 0x00E0, 23, 1),
	FMCLK2(CKGEN, hf_faes_ufsfde_ck, "hf_faes_ufsfde_ck", 0x00E0, 31, 1),
	FMCLK2(CKGEN, hf_fufs_ck, "hf_fufs_ck", 0x00F0, 7, 1),
	FMCLK2(CKGEN, f_fufs_mbist_ck, "f_fufs_mbist_ck", 0x00F0, 15, 1),
	FMCLK2(CKGEN, f_fpextp_mbist_ck, "f_fpextp_mbist_ck", 0x00F0, 23, 1),
	FMCLK2(CKGEN, hf_faud_1_ck, "hf_faud_1_ck", 0x00F0, 31, 1),
	FMCLK2(CKGEN, hf_faud_2_ck, "hf_faud_2_ck", 0x0100, 7, 1),
	FMCLK2(CKGEN, hf_fadsp_ck, "hf_fadsp_ck", 0x0100, 15, 1),
	FMCLK2(CKGEN, hf_fdpmaif_main_ck, "hf_fdpmaif_main_ck", 0x0100, 23, 1),
	FMCLK2(CKGEN, hf_fvenc_ck, "hf_fvenc_ck", 0x0100, 31, 1),
	FMCLK2(CKGEN, hf_fvdec_ck, "hf_fvdec_ck", 0x0110, 7, 1),
	FMCLK2(CKGEN, hf_fpwm_ck, "hf_fpwm_ck", 0x0110, 15, 1),
	FMCLK2(CKGEN, hf_faudio_h_ck, "hf_faudio_h_ck", 0x0110, 23, 1),
	FMCLK2(CKGEN, hg_fmcupm_ck, "hg_fmcupm_ck", 0x0110, 31, 1),
	FMCLK2(CKGEN, hf_fspmi_p_mst_ck, "hf_fspmi_p_mst_ck", 0x0120, 7, 1),
	FMCLK2(CKGEN, hf_fspmi_m_mst_ck, "hf_fspmi_m_mst_ck", 0x0120, 15, 1),
	FMCLK2(CKGEN, hf_ftl_ck, "hf_ftl_ck", 0x0120, 23, 1),
	FMCLK2(CKGEN, hf_fmem_sub_ck, "hf_fmem_sub_ck", 0x0120, 31, 1),
	FMCLK2(CKGEN, f_fperi_hf_fmem_ck, "f_fperi_hf_fmem_ck", 0x0130, 7, 1),
	FMCLK2(CKGEN, f_fufs_hf_fmem_ck, "f_fufs_hf_fmem_ck", 0x0130, 15, 1),
	FMCLK2(CKGEN, hf_faes_msdcfde_ck, "hf_faes_msdcfde_ck", 0x0130, 23, 1),
	FMCLK2(CKGEN, hf_femi_n_ck, "hf_femi_n_ck", 0x0130, 31, 1),
	FMCLK2(CKGEN, hf_femi_s_ck, "hf_femi_s_ck", 0x0140, 7, 1),
	FMCLK2(CKGEN, hf_fdsi_occ_ck, "hf_fdsi_occ_ck", 0x0140, 15, 1),
	FMCLK2(CKGEN, f_fdptx_ck, "f_fdptx_ck", 0x0140, 23, 1),
	FMCLK2(CKGEN, hf_fccu_ahb_ck, "hf_fccu_ahb_ck", 0x0140, 31, 1),
	FMCLK2(CKGEN, f_fap2conn_host_ck, "f_fap2conn_host_ck", 0x0150, 7, 1),
	FMCLK2(CKGEN, hf_fimg1_ck, "hf_fimg1_ck", 0x0150, 15, 1),
	FMCLK2(CKGEN, hf_fipe_ck, "hf_fipe_ck", 0x0150, 23, 1),
	FMCLK2(CKGEN, hf_fcam_ck, "hf_fcam_ck", 0x0150, 31, 1),
	FMCLK2(CKGEN, hf_fccusys_ck, "hf_fccusys_ck", 0x0160, 7, 1),
	FMCLK2(CKGEN, f_fcamtm_ck, "f_fcamtm_ck", 0x0160, 15, 1),
	FMCLK2(CKGEN, hf_fsflash_ck, "hf_fsflash_ck", 0x0160, 23, 1),
	FMCLK2(CKGEN, hf_fmcu_acp_ck, "hf_fmcu_acp_ck", 0x0160, 31, 1),
	/* ABIST Part */
	FMCLK(ABIST, FM_ADSPPLL_CK, "fm_adsppll_ck", 1),
	FMCLK(ABIST, FM_APLL1_CK, "fm_apll1_ck", 1),
	FMCLK(ABIST, FM_APLL2_CK, "fm_apll2_ck", 1),
	FMCLK(ABIST, FM_APPLLGP_MON_FM_CK, "fm_appllgp_mon_fm_ck", 1),
	FMCLK3(ABIST, FM_ARMPLL_BL_CKDIV_CK, "fm_armpll_bl_ckdiv_ck", 0x021c, 1, 8),
	FMCLK3(ABIST, FM_ARMPLL_LL_CKDIV_CK, "fm_armpll_ll_ckdiv_ck", 0x020c, 1, 8),
	FMCLK(ABIST, FM_USBPLL_OPP_CK, "fm_usbpll_opp_ck", 1),
	FMCLK3(ABIST, FM_CCIPLL_CKDIV_CK, "fm_ccipll_ckdiv_ck", 0x023c, 1, 8),
	FMCLK(ABIST, FM_CSI0A_DELAYCAL_CK, "fm_csi0a_delaycal_ck", 1),
	FMCLK(ABIST, FM_CSI0B_DELAYCAL_CK, "fm_csi0b_delaycal_ck", 1),
	FMCLK(ABIST, FM_CSI1A_DELAYCAL_CK, "fm_csi1a_delaycal_ck", 1),
	FMCLK(ABIST, FM_CSI1B_DELAYCAL_CK, "fm_csi1b_delaycal_ck", 1),
	FMCLK(ABIST, FM_CSI2A_DELAYCAL_CK, "fm_csi2a_delaycal_ck", 1),
	FMCLK(ABIST, FM_CSI2B_DELAYCAL_CK, "fm_csi2b_delaycal_ck", 1),
	FMCLK(ABIST, FM_CSI3A_DELAYCAL_CK, "fm_csi3a_delaycal_ck", 1),
	FMCLK(ABIST, FM_CSI3B_DELAYCAL_CK, "fm_csi3b_delaycal_ck", 1),
	FMCLK(ABIST, FM_DSI0_LNTC_DSICLK, "fm_dsi0_lntc_dsiclk", 1),
	FMCLK(ABIST, FM_DSI0_MPPLL_TST_CK, "fm_dsi0_mppll_tst_ck", 1),
	FMCLK3(ABIST, FM_MAINPLL_CKDIV_CK, "fm_mainpll_ckdiv_ck", 0x0354, 1, 8),
	FMCLK(ABIST, FM_MAINPLL_CK, "fm_mainpll_ck", 1),
	FMCLK(ABIST, FM_MDPLL1_FS26M_GUIDE, "fm_mdpll1_fs26m_guide", 1),
	FMCLK3(ABIST, FM_MMPLL_CKDIV_CK, "fm_mmpll_ckdiv_ck", 0x03A4, 1, 8),
	FMCLK(ABIST, FM_MMPLL_CK, "fm_mmpll_ck", 1),
	FMCLK(ABIST, FM_MMPLL_D3_CK, "fm_mmpll_d3_ck", 1),
	FMCLK(ABIST, FM_MPLL_CK, "fm_mpll_ck", 1),
	FMCLK(ABIST, FM_MSDCPLL_CK, "fm_msdcpll_ck", 1),
	FMCLK(ABIST, FM_RCLRPLL_DIV4_CH01, "fm_rclrpll_div4_ch01", 1),
	FMCLK(ABIST, FM_IMGPLL_CK, "fm_imgpll_ck", 1),
	FMCLK(ABIST, FM_RPHYPLL_DIV4_CH01, "fm_rphypll_div4_ch01", 1),
	FMCLK(ABIST, FM_EMIPLL_CK, "fm_emipll_ck", 1),
	FMCLK(ABIST, FM_TVDPLL_CK, "fm_tvdpll_ck", 1),
	FMCLK(ABIST, FM_ULPOSC2_MON_V_VCORE_CK, "fm_ulposc2_mon_v_vcore_ck", 1),
	FMCLK(ABIST, FM_ULPOSC_MON_VCORE_CK, "fm_ulposc_mon_vcore_ck", 1),
	FMCLK(ABIST, FM_UNIVPLL_CK, "fm_univpll_ck", 1),
	FMCLK3(ABIST, FM_USBPLL_CKDIV_CK, "fm_usbpll_ckdiv_ck", 0x031C, 2, 13),
	FMCLK3(ABIST, FM_UNIVPLL_CKDIV_CK, "fm_univpll_ckdiv_ck", 0x030c, 2, 13),
	FMCLK(ABIST, FM_U_CLK2FREQ, "fm_u_clk2freq", 1),
	FMCLK(ABIST, FM_WBG_DIG_BPLL_CK, "fm_wbg_dig_bpll_ck", 1),
	FMCLK(ABIST, FM_WBG_DIG_WPLL_CK960, "fm_wbg_dig_wpll_ck960", 1),
	FMCLK(ABIST, FMEM_AFT_CH0, "fmem_aft_ch0", 1),
	FMCLK(ABIST, FMEM_AFT_CH1, "fmem_aft_ch1", 1),
	FMCLK(ABIST, FMEM_BFE_CH0, "fmem_bfe_ch0", 1),
	FMCLK(ABIST, FMEM_BFE_CH1, "fmem_bfe_ch1", 1),
	FMCLK(ABIST, FM_466M_FMEM_INFRASYS, "fm_466m_fmem_infrasys", 1),
	FMCLK(ABIST, FM_MCUSYS_ARM_OUT_ALL, "fm_mcusys_arm_out_all", 1),
	FMCLK(ABIST, FM_MSDC1_IN_CK, "fm_msdc1_in_ck", 1),
	FMCLK(ABIST, FM_MSDC2_IN_CK, "fm_msdc2_in_ck", 1),
	FMCLK(ABIST, FM_F32K_VCORE_CK, "fm_f32k_vcore_ck", 1),
	FMCLK(ABIST, FM_APU_OCC_TO_SOC, "fm_apu_occ_to_soc", 1),
	FMCLK(ABIST, FM_ALVTS_T0_PLLGP_MON_L6, "fm_alvts_t0_pllgp_mon_l6", 1),
	FMCLK(ABIST, FM_ALVTS_T0_PLLGP_MON_L5, "fm_alvts_t0_pllgp_mon_l5", 1),
	FMCLK(ABIST, FM_ALVTS_T0_PLLGP_MON_L4, "fm_alvts_t0_pllgp_mon_l4", 1),
	FMCLK(ABIST, FM_ALVTS_T0_PLLGP_MON_L3, "fm_alvts_t0_pllgp_mon_l3", 1),
	FMCLK(ABIST, FM_ALVTS_T0_PLLGP_MON_L2, "fm_alvts_t0_pllgp_mon_l2", 1),
	FMCLK(ABIST, FM_ALVTS_T0_PLLGP_MON_L1, "fm_alvts_t0_pllgp_mon_l1", 1),
	FMCLK(ABIST, FM_ALVTS_T0_PLLGP_MON_LM, "fm_alvts_t0_pllgp_mon_lm", 1),
	/* VLPCK Part */
	FMCLK2(VLPCK, FM_SCP_CK, "fm_scp_ck", 0x0008, 7, 1),
	FMCLK(VLPCK, FM_SPM_CK, "fm_spm_ck", 1),
	FMCLK2(VLPCK, FM_PWRAP_ULPOSC_CK, "fm_pwrap_ulposc_ck", 0x0008, 15, 1),
	FMCLK2(VLPCK, FM_DXCC_VLP_CK, "fm_dxcc_vlp_ck", 0x0008, 31, 1),
	FMCLK2(VLPCK, FM_SPMI_P_MST_CK, "fm_spmi_p_mst_ck", 0x0014, 7, 1),
	FMCLK2(VLPCK, FM_SPMI_M_MST_CK, "fm_spmi_m_mst_ck", 0x0014, 15, 1),
	FMCLK2(VLPCK, FM_DVFSRC_CK, "fm_dvfsrc_ck", 0x0014, 23, 1),
	FMCLK2(VLPCK, FM_PWM_VLP_CK, "fm_pwm_vlp_ck", 0x0014, 31, 1),
	FMCLK2(VLPCK, FM_AXI_VLP_CK, "fm_axi_vlp_ck", 0x0020, 7, 1),
	FMCLK2(VLPCK, FM_DBGAO_26M_CK, "fm_dbgao_26m_ck", 0x0020, 15, 1),
	FMCLK2(VLPCK, FM_SYSTIMER_26M_CK, "fm_systimer_26m_ck", 0x0020, 23, 1),
	FMCLK2(VLPCK, FM_SSPM_CK, "fm_sspm_ck", 0x0020, 31, 1),
	FMCLK2(VLPCK, FM_SSPM_F26M_CK, "fm_sspm_f26m_ck", 0x002C, 7, 1),
	FMCLK2(VLPCK, FM_APEINT_66M_CK, "fm_apeint_66m_ck", 0x002C, 15, 1),
	FMCLK2(VLPCK, FM_SRCK_CK, "fm_srck_ck", 0x002C, 23, 1),
	FMCLK2(VLPCK, FM_SRAMRC_CK, "fm_sramrc_ck", 0x002C, 31, 1),
	FMCLK(VLPCK, FM_SEJ_26M_CK, "fm_sej_26m_ck", 1),
	FMCLK(VLPCK, FM_MD_BUCK_CTRL_OSC26M_CK, "fm_md_buck_ctrl_osc26m_ck", 1),
	FMCLK(VLPCK, FM_SSPM_ULPOSC_CK, "fm_sspm_ulposc_ck", 1),
	FMCLK(VLPCK, FM_RTC_CK, "fm_rtc_ck", 1),
	FMCLK(VLPCK, FM_ULPOSC_CORE_CK, "fm_ulposc_core_ck", 1),
	FMCLK(VLPCK, FM_ULPOSC_CK, "fm_ulposc_ck", 1),
	{},
};

#define _CKGEN(x)		(topck_base + (x))
#define CLK_MISC_CFG_0		_CKGEN(0x240)
#define CLK_DBG_CFG		_CKGEN(0x28C)
#define CLK26CALI_0		_CKGEN(0x220)
#define CLK26CALI_1		_CKGEN(0x224)
#define _APMIXED(x)		(apmixed_base + (x))
#define _VLP_CK(x)		(vlp_ck_base + (x))
#define VLP_CK_CON0		_VLP_CK(0x230)
#define VLP_CK_CON1		_VLP_CK(0x234)

static void __iomem *topck_base;
static void __iomem *apmixed_base;
static void __iomem *spm_base;
static void __iomem *vlp_ck_base;

const struct fmeter_clk *mt6983_get_fmeter_clks(void)
{
	return fm_all_clks;
}

static unsigned int check_pdn(void __iomem *base,
		unsigned int type, unsigned int ID)
{
	int i;

	for (i = 0; i < fm_clk_cnt; i++) {
		if (fm_all_clks[i].type == type && fm_all_clks[i].id == ID)
			break;
	}

	if (i >= fm_clk_cnt)
		return 1;

	if (!fm_all_clks[i].ofs)
		return 0;

	if (type == SUBSYS) {
		if ((clk_readl(base + fm_all_clks[i].ofs) & fm_all_clks[i].pdn)
				!= fm_all_clks[i].pdn) {
			return 1;
		}
	} else if (type != SUBSYS && ((clk_readl(base + fm_all_clks[i].ofs)
			& BIT(fm_all_clks[i].pdn)) == BIT(fm_all_clks[i].pdn)))
		return 1;

	return 0;
}

static unsigned int get_post_div(unsigned int type, unsigned int ID)
{
	unsigned int post_div = 1;
	int i;

	if ((ID <= 0) || (ID >= 89))
		return post_div;

	for (i = 0; i < ARRAY_SIZE(fclks) - 1; i++) {
		if (fclks[i].type == type && fclks[i].id == ID
				&& fclks[i].grp == 1) {
			post_div =  clk_readl(apmixed_base + fclks[i].ofs);
			post_div = 1 << ((post_div >> 24) & 0x7);
			break;
		}
	}

	return post_div;
}

static unsigned int get_clk_div(unsigned int type, unsigned int ID)
{
	unsigned int clk_div = 1;
	int i;

	if ((ID <= 0) || (ID >= 89))
		return clk_div;

	for (i = 0; i < ARRAY_SIZE(fclks) - 1; i++)
		if (fclks[i].type == type && fclks[i].id == ID)
			break;

	if (i >= ARRAY_SIZE(fclks) - 1)
		return clk_div;

	return fclks[i].ck_div;
}

static int __mt_get_freq(unsigned int ID, int type)
{
	unsigned int temp, clk_dbg_cfg, clk_misc_cfg_0, clk26cali_1 = 0;
	unsigned int clk_div = 1, post_div = 1;
	unsigned long flags;
	int output = 0, i = 0;

	fmeter_lock(flags);

	if (type == CKGEN && check_pdn(topck_base, CKGEN, ID)) {
		pr_notice("ID-%d: MUX PDN, return 0.\n", ID);
		fmeter_unlock(flags);
		return 0;
	}

	while (clk_readl(CLK26CALI_0) & 0x10) {
		udelay(10);
		i++;
		if (i > FM_TIMEOUT)
			break;
	}

	/* CLK26CALI_0[15]: rst 1 -> 0 */
	clk_writel(CLK26CALI_0, (clk_readl(CLK26CALI_0) & 0xFFFF7FFF));
	/* CLK26CALI_0[15]: rst 0 -> 1 */
	clk_writel(CLK26CALI_0, (clk_readl(CLK26CALI_0) | 0x00008000));

	if (type == CKGEN) {
		clk_dbg_cfg = clk_readl(CLK_DBG_CFG);
		clk_writel(CLK_DBG_CFG, (clk_dbg_cfg & 0xFFFF80FC) | (ID << 8) | (0x1));
	} else if (type == ABIST) {
		clk_dbg_cfg = clk_readl(CLK_DBG_CFG);
		clk_writel(CLK_DBG_CFG, (clk_dbg_cfg & 0xFF80FFFC) | (ID << 16));
	} else if (type == ABIST_2) {
		clk_dbg_cfg = clk_readl(CLK_DBG_CFG);
		clk_writel(CLK_DBG_CFG, (clk_dbg_cfg & 0xC0FFFFFC) | (ID << 24) | (0x2));
	} else {
		fmeter_unlock(flags);
		return 0;
	}

	clk_misc_cfg_0 = clk_readl(CLK_MISC_CFG_0);
	clk_writel(CLK_MISC_CFG_0, (clk_misc_cfg_0 & 0x00FFFFFF) | (3 << 24));

	clk26cali_1 = clk_readl(CLK26CALI_1);
	clk_writel(CLK26CALI_0, 0x9000);
	clk_writel(CLK26CALI_0, 0x9010);

	/* wait frequency meter finish */
	i = 0;
	do {
		udelay(10);
		i++;
		if (i > FM_TIMEOUT)
			break;
	} while (clk_readl(CLK26CALI_0) & 0x10);

	temp = clk_readl(CLK26CALI_1) & 0xFFFF;

	if (type == ABIST)
		post_div = get_post_div(type, ID);

	clk_div = get_clk_div(type, ID);

	output = (temp * 26000) / 1024 * clk_div / post_div;

	clk_writel(CLK_DBG_CFG, clk_dbg_cfg);
	clk_writel(CLK_MISC_CFG_0, clk_misc_cfg_0);
	/*clk_writel(CLK26CALI_0, clk26cali_0);*/
	/*clk_writel(CLK26CALI_1, clk26cali_1);*/

	clk_writel(CLK26CALI_0, 0x8000);
	fmeter_unlock(flags);

	if (i > FM_TIMEOUT)
		return 0;

	if ((output * 4) < 1000) {
		pr_notice("%s(%d): CLK_DBG_CFG = 0x%x, CLK_MISC_CFG_0 = 0x%x, CLK26CALI_0 = 0x%x, CLK26CALI_1 = 0x%x\n",
			__func__,
			ID,
			clk_readl(CLK_DBG_CFG),
			clk_readl(CLK_MISC_CFG_0),
			clk_readl(CLK26CALI_0),
			clk_readl(CLK26CALI_1));
	}

	return (output * 4);
}

static int __mt_get_freq2(void __iomem *con0, void __iomem *con1,
		unsigned int id, int type)
{
	unsigned int temp;
	unsigned long flags;
	int output = 0, i = 0;

	fmeter_lock(flags);

	/* PLL4H_FQMTR_CON1[15]: rst 1 -> 0 */
	clk_writel(con0, clk_readl(con0) & 0xFFFF7FFF);
	/* PLL4H_FQMTR_CON1[15]: rst 0 -> 1 */
	clk_writel(con0, clk_readl(con0) | 0x8000);

	/* sel fqmtr_cksel */
	if (type == SUBSYS)
		clk_writel(con0, (clk_readl(con0) & 0x00FFFFF8) | (id << 0));
	else if (type == VLPCK)
		clk_writel(con0, (clk_readl(con0) & 0xFFE0FFF8) | (id << 16));
	/* set ckgen_load_cnt to 1024 */
	clk_writel(con1, (clk_readl(con1) & 0xFC00FFFF) | (0x3FF << 16));

	/* sel fqmtr_cksel and set ckgen_k1 to 0(DIV4) */
	clk_writel(con0, (clk_readl(con0) & 0x00FFFFFF) | (3 << 24));

	/* fqmtr_en set to 1, fqmtr_exc set to 0, fqmtr_start set to 0 */
	clk_writel(con0, (clk_readl(con0) & 0xFFFF8007) | 0x1000);
	/*fqmtr_start set to 1 */
	clk_writel(con0, clk_readl(con0) | 0x10);

	/* wait frequency meter finish */
	while (clk_readl(con0) & 0x10) {
		udelay(10);
		i++;
		if (i > 30) {
			pr_notice("[%d]con0: 0x%x, con1: 0x%x\n",
				id, clk_readl(con0), clk_readl(con1));
			break;
		}
	}

	temp = clk_readl(con1) & 0xFFFF;
	output = ((temp * 26000)) / 1024; // Khz

	clk_writel(con0, 0x8000);

	fmeter_unlock(flags);

	return output * 4;
}

static unsigned int mt6983_get_ckgen_freq(unsigned int ID)
{
	return __mt_get_freq(ID, CKGEN);
}

static unsigned int mt6983_get_abist_freq(unsigned int ID)
{
	return __mt_get_freq(ID, ABIST);
}

static unsigned int mt6983_get_abist2_freq(unsigned int ID)
{
	return __mt_get_freq(ID, ABIST_2);
}

static unsigned int mt6983_get_vlpck_freq(unsigned int ID)
{
	return __mt_get_freq2(VLP_CK_CON0, VLP_CK_CON1, ID, VLPCK);
}

static unsigned int mt6983_get_subsys_freq(unsigned int ID)
{
	int output = 0;
	unsigned long flags;
	void __iomem *base, *con0, *con1;
	unsigned int id;
	unsigned int subsys_idx;

	subsys_fmeter_lock(flags);
	if (check_pdn(spm_base, SUBSYS, ID)) {
		pr_notice("ID-%d: PDN, return 0.\n", ID);
		subsys_fmeter_unlock(flags);
		return 0;
	}

	id = FM_ID(ID);
	subsys_idx = FM_SYS(ID) * 4 + id;
	if (subsys_idx >= (SUBSYS_PLL_NUM * FM_SYS_NUM) || subsys_idx < 0) {
		subsys_fmeter_unlock(flags);
		return 0;
	}

	base = fm_sub_clks[subsys_idx].base;
	con0 = base + fm_sub_clks[subsys_idx].con0;
	con1 = base + fm_sub_clks[subsys_idx].con1;

	pr_notice("subsys ID: %d\n", ID);
	__mt_get_freq2(con0, con1, id, SUBSYS);

	subsys_fmeter_unlock(flags);

	return output * 4;
}

static unsigned int mt6983_get_fmeter_freq(unsigned int id, enum  FMETER_TYPE type)
{
	if (type == CKGEN)
		return mt6983_get_ckgen_freq(id);
	else if (type == ABIST)
		return mt6983_get_abist_freq(id);
	else if (type == ABIST_2)
		return mt6983_get_abist2_freq(id);
	else if (type == SUBSYS)
		return mt6983_get_subsys_freq(id);
	else if (type == VLPCK)
		return mt6983_get_vlpck_freq(id);

	return FT_NULL;
}

static int mt6983_subsys_freq_register(struct fm_subsys *fm, unsigned int size)
{
	unsigned int all_idx, sub_idx;
	int i;

	fm_sub_clks = kcalloc(FM_SYS_NUM * SUBSYS_PLL_NUM,
			sizeof(*fm_all_clks), GFP_KERNEL);
	if (!fm_sub_clks)
		return -ENOMEM;

	for (i = 0; i < size; i++, fm++) {
		unsigned int sys = FM_SYS(fm->id);
		unsigned int id = FM_ID(fm->id);

		if (sys >= FM_SYS_NUM || id >= SUBSYS_PLL_NUM)
			continue;

		sub_idx = (sys * 4) + id;
		all_idx = fm_clk_cnt;

		fm_all_clks[all_idx].id = fm->id;
		fm_all_clks[all_idx].type = SUBSYS;
		fm_all_clks[all_idx].name = fm->name;
		fm_all_clks[all_idx].ofs = fm->pwr_sta.ofs;
		fm_all_clks[all_idx].pdn = fm->pwr_sta.msk;

		fm_sub_clks[sub_idx].base = fm->base;
		fm_sub_clks[sub_idx].con0 = fm->con0;
		fm_sub_clks[sub_idx].con1 = fm->con1;

		fm_clk_cnt++;
	}

	return 0;
}

static int mt6983_get_fmeter_id(enum FMETER_ID fid)
{
	return FID_NULL;
}

static void __iomem *get_base_from_comp(const char *comp)
{
	struct device_node *node;
	static void __iomem *base;

	node = of_find_compatible_node(NULL, NULL, comp);
	if (node) {
		base = of_iomap(node, 0);
		if (!base) {
			pr_notice("%s() can't find iomem for %s\n",
					__func__, comp);
			return ERR_PTR(-EINVAL);
		}

		return base;
	}

	pr_notice("%s can't find compatible node\n", __func__);

	return ERR_PTR(-EINVAL);
}

/*
 * init functions
 */

static struct fmeter_ops fm_ops = {
	.get_fmeter_clks = mt6983_get_fmeter_clks,
	.get_fmeter_freq = mt6983_get_fmeter_freq,
	.subsys_freq_register = mt6983_subsys_freq_register,
	.get_fmeter_id = mt6983_get_fmeter_id,
};

static int clk_fmeter_mt6983_probe(struct platform_device *pdev)
{
	int i;

	topck_base = get_base_from_comp("mediatek,mt6983-topckgen");
	if (IS_ERR(topck_base))
		goto ERR;

	apmixed_base = get_base_from_comp("mediatek,mt6983-apmixedsys");
	if (IS_ERR(apmixed_base))
		goto ERR;

	spm_base = get_base_from_comp("mediatek,mt6983-scpsys");
	if (IS_ERR(spm_base))
		goto ERR;

	vlp_ck_base = get_base_from_comp("mediatek,mt6983-vlp_cksys");
	if (IS_ERR(vlp_ck_base))
		goto ERR;

	fm_all_clks = kcalloc(FM_SYS_NUM * 4 + ARRAY_SIZE(fclks),
			sizeof(*fm_all_clks), GFP_KERNEL);
	if (!fm_all_clks)
		return -ENOMEM;

	for (i = 0; i < ARRAY_SIZE(fclks) - 1; i++) {
		fm_all_clks[i] = fclks[i];
		fm_clk_cnt++;
	}

	fmeter_set_ops(&fm_ops);

	return 0;
ERR:
	pr_notice("%s can't find base\n", __func__);

	return -EINVAL;
}

static struct platform_driver clk_fmeter_mt6983_drv = {
	.probe = clk_fmeter_mt6983_probe,
	.driver = {
		.name = "clk-fmeter-mt6983",
		.owner = THIS_MODULE,
	},
};

static int __init clk_fmeter_init(void)
{
	static struct platform_device *clk_fmeter_dev;

	clk_fmeter_dev = platform_device_register_simple("clk-fmeter-mt6983", -1, NULL, 0);
	if (IS_ERR(clk_fmeter_dev))
		pr_notice("unable to register clk-fmeter device");

	return platform_driver_register(&clk_fmeter_mt6983_drv);
}

static void __exit clk_fmeter_exit(void)
{
	platform_driver_unregister(&clk_fmeter_mt6983_drv);
}

subsys_initcall(clk_fmeter_init);
module_exit(clk_fmeter_exit);
MODULE_LICENSE("GPL");
