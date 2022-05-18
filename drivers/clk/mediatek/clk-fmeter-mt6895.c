// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2021 MediaTek Inc.
// Author: Ren-Ting Wang <ren-ting.wang@mediatek.com>

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "clk-fmeter.h"
#include "clk-mt6895-fmeter.h"

#define FM_TIMEOUT			30
#define SUBSYS_PLL_NUM			4
#define VLP_FM_WAIT_TIME		40	/* ~= 38.64ns * 1023 */

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
	FMCLK2(CKGEN, FM_AXI_CK, "fm_axi_ck", 0x0010, 7, 1),
	FMCLK2(CKGEN, FMP_AXI_CK, "fmp_axi_ck", 0x0010, 15, 1),
	FMCLK2(CKGEN, FM_U_HAXI_CK, "fm_u_haxi_ck", 0x0010, 23, 1),
	FMCLK2(CKGEN, FM_B, "fm_b", 0x0010, 31, 1),
	FMCLK2(CKGEN, FM_DISP0_CK, "fm_disp0_ck", 0x0020, 7, 1),
	FMCLK2(CKGEN, FM_DISP1_CK, "fm_disp1_ck", 0x0020, 15, 1),
	FMCLK2(CKGEN, FM_MDP0_CK, "fm_mdp0_ck", 0x0020, 23, 1),
	FMCLK2(CKGEN, FM_MDP1_CK, "fm_mdp1_ck", 0x0020, 31, 1),
	FMCLK2(CKGEN, FM_MMINFRA_CK, "fm_mminfra_ck", 0x0030, 7, 1),
	FMCLK2(CKGEN, FM_MMUP_CK, "fm_mmup_ck", 0x0030, 15, 1),
	FMCLK2(CKGEN, FM_DSP_CK, "fm_dsp_ck", 0x0030, 23, 1),
	FMCLK2(CKGEN, FM_DSP1_CK, "fm_dsp1_ck", 0x0030, 31, 1),
	FMCLK2(CKGEN, FM_DSP2_CK, "fm_dsp2_ck", 0x0040, 7, 1),
	FMCLK2(CKGEN, FM_DSP3_CK, "fm_dsp3_ck", 0x0040, 15, 1),
	FMCLK2(CKGEN, FM_DSP4_CK, "fm_dsp4_ck", 0x0040, 23, 1),
	FMCLK2(CKGEN, FM_DSP5_CK, "fm_dsp5_ck", 0x0040, 31, 1),
	FMCLK2(CKGEN, FM_DSP6_CK, "fm_dsp6_ck", 0x0050, 7, 1),
	FMCLK2(CKGEN, FM_DSP7_CK, "fm_dsp7_ck", 0x0050, 15, 1),
	FMCLK(CKGEN, FM_IPU_IF_CK, "fm_ipu_if_ck", 1),
	FMCLK2(CKGEN, FM_MFG_REF_CK, "fm_mfg_ref_ck", 0x0050, 31, 1),
	FMCLK2(CKGEN, FM_MFGSC_REF_CK, "fm_mfgsc_ref_ck", 0x0060, 7, 1),
	FMCLK2(CKGEN, FM_CAMTG_CK, "fm_camtg_ck", 0x0060, 15, 1),
	FMCLK2(CKGEN, FM_CAMTG2_CK, "fm_camtg2_ck", 0x0060, 23, 1),
	FMCLK2(CKGEN, FM_CAMTG3_CK, "fm_camtg3_ck", 0x0060, 31, 1),
	FMCLK2(CKGEN, FM_CAMTG4_CK, "fm_camtg4_ck", 0x0070, 7, 1),
	FMCLK2(CKGEN, FM_CAMTG5_CK, "fm_camtg5_ck", 0x0070, 15, 1),
	FMCLK2(CKGEN, FM_CAMTG6_CK, "fm_camtg6_ck", 0x0070, 23, 1),
	FMCLK2(CKGEN, FM_CAMTG7_CK, "fm_camtg7_ck", 0x0070, 31, 1),
	FMCLK(CKGEN, FM_CAMTG8_CK, "fm_camtg8_ck", 1),
	FMCLK2(CKGEN, FM_UART_CK, "fm_uart_ck", 0x0080, 15, 1),
	FMCLK2(CKGEN, FM_SPI_CK, "fm_spi_ck", 0x0080, 23, 1),
	FMCLK(CKGEN, FM_MSDC5HCLK_CK, "fm_msdc5hclk_ck", 1),
	FMCLK2(CKGEN, FM_MSDC_MACRO_CK, "fm_msdc_macro_ck", 0x0090, 7, 1),
	FMCLK2(CKGEN, FM_MSDC30_1_CK, "fm_msdc30_1_ck", 0x0090, 15, 1),
	FMCLK2(CKGEN, FM_MSDC30_2_CK, "fm_msdc30_2_ck", 0x0090, 23, 1),
	FMCLK2(CKGEN, FM_AUDIO_CK, "fm_audio_ck", 0x0090, 31, 1),
	FMCLK2(CKGEN, FM_AUD_INTBUS_CK, "fm_aud_intbus_ck", 0x00A0, 7, 1),
	FMCLK(CKGEN, FM_PWRAP_ULPOSC_CK, "fm_pwrap_ulposc_ck", 1),
	FMCLK2(CKGEN, FM_ATB_CK, "fm_atb_ck", 0x00A0, 23, 1),
	FMCLK2(CKGEN, FM_DP_CK, "fm_dp_ck", 0x00A0, 31, 1),
	FMCLK2(CKGEN, FM_DISP_PWM_CK, "fm_disp_pwm_ck", 0x00B0, 7, 1),
	FMCLK2(CKGEN, FM_USB_CK, "fm_usb_ck", 0x00B0, 15, 1),
	FMCLK2(CKGEN, FM_USB_XHCI_CK, "fm_usb_xhci_ck", 0x00B0, 23, 1),
	FMCLK2(CKGEN, FM_USB_1P_CK, "fm_usb_1p_ck", 0x00B0, 31, 1),
	FMCLK2(CKGEN, FM_USB_XHCI_1P_CK, "fm_usb_xhci_1p_ck", 0x00C0, 7, 1),
	FMCLK2(CKGEN, FM_I2C_CK, "fm_i2c_ck", 0x00C0, 15, 1),
	FMCLK2(CKGEN, FM_SENINF_CK, "fm_seninf_ck", 0x00C0, 23, 1),
	FMCLK2(CKGEN, FM_SENINF1_CK, "fm_seninf1_ck", 0x00C0, 31, 1),
	FMCLK2(CKGEN, FM_SENINF2_CK, "fm_seninf2_ck", 0x00D0, 7, 1),
	FMCLK2(CKGEN, FM_SENINF3_CK, "fm_seninf3_ck", 0x00D0, 15, 1),
	FMCLK2(CKGEN, FM_SENINF4_CK, "fm_seninf4_ck", 0x00D0, 23, 1),
	FMCLK(CKGEN, FM_SENINF5_CK, "fm_seninf5_ck", 1),
	FMCLK2(CKGEN, FM_DXCC_CK, "fm_dxcc_ck", 0x00E0, 7, 1),
	FMCLK2(CKGEN, FM_AUD_ENGEN1_CK, "fm_aud_engen1_ck", 0x00E0, 15, 1),
	FMCLK2(CKGEN, FM_AUD_ENGEN2_CK, "fm_aud_engen2_ck", 0x00E0, 23, 1),
	FMCLK2(CKGEN, FM_AES_UFSFDE_CK, "fm_aes_ufsfde_ck", 0x00E0, 31, 1),
	FMCLK2(CKGEN, FM_U_CK, "fm_u_ck", 0x00F0, 7, 1),
	FMCLK2(CKGEN, FM_U_MBIST_CK, "fm_u_mbist_ck", 0x00F0, 15, 1),
	FMCLK2(CKGEN, FM_PEXTP_MBIST_CK, "fm_pextp_mbist_ck", 0x00F0, 23, 1),
	FMCLK2(CKGEN, FM_AUD_1_CK, "fm_aud_1_ck", 0x00F0, 31, 1),
	FMCLK2(CKGEN, FM_AUD_2_CK, "fm_aud_2_ck", 0x0100, 7, 1),
	FMCLK2(CKGEN, FM_ADSP_CK, "fm_adsp_ck", 0x0100, 15, 1),
	FMCLK2(CKGEN, FM_DPMAIF_MAIN_CK, "fm_dpmaif_main_ck", 0x0100, 23, 1),
	FMCLK2(CKGEN, FM_VENC_CK, "fm_venc_ck", 0x0100, 31, 1),
	FMCLK2(CKGEN, FM_VDEC_CK, "fm_vdec_ck", 0x0110, 7, 1),
	FMCLK2(CKGEN, FM_PWM_CK, "fm_pwm_ck", 0x0110, 15, 1),
	FMCLK2(CKGEN, FM_AUDIO_H_CK, "fm_audio_h_ck", 0x0110, 23, 1),
	FMCLK2(CKGEN, FM_MCUPM_CK, "fm_mcupm_ck", 0x0110, 31, 1),
	FMCLK(CKGEN, FM_SPMI_P_CK, "fm_spmi_p_ck", 1),
	FMCLK(CKGEN, FM_SPMI_M_CK, "fm_spmi_m_ck", 1),
	FMCLK(CKGEN, FM_TL_CK, "fm_tl_ck", 1),
	FMCLK2(CKGEN, FM_MEM_SUB_CK, "fm_mem_sub_ck", 0x0120, 31, 1),
	FMCLK2(CKGEN, FMP_MEM_CK, "fmp_mem_ck", 0x0130, 7, 1),
	FMCLK2(CKGEN, FM_U_MEM_CK, "fm_u_mem_ck", 0x0130, 15, 1),
	FMCLK(CKGEN, FM_AES_MSDCFDE_CK, "fm_aes_msdcfde_ck", 1),
	FMCLK2(CKGEN, FM_EMI_N_CK, "fm_emi_n_ck", 0x0130, 31, 1),
	FMCLK2(CKGEN, FM_EMI_S_CK, "fm_emi_s_ck", 0x0140, 7, 1),
	FMCLK2(CKGEN, FM_DSI_OCC_CK, "fm_dsi_occ_ck", 0x0140, 15, 1),
	FMCLK(CKGEN, FM_DPTX_CK, "fm_dptx_ck", 1),
	FMCLK2(CKGEN, FM_CCU_AHB_CK, "fm_ccu_ahb_ck", 0x0140, 31, 1),
	FMCLK2(CKGEN, FM_AP2CONN_HOST_CK, "fm_ap2conn_host_ck", 0x0150, 7, 1),
	FMCLK2(CKGEN, FM_IMG1_CK, "fm_img1_ck", 0x0150, 15, 1),
	FMCLK2(CKGEN, FM_IPE_CK, "fm_ipe_ck", 0x0150, 23, 1),
	FMCLK2(CKGEN, FM_CAM_CK, "fm_cam_ck", 0x0150, 31, 1),
	FMCLK2(CKGEN, FM_CCUSYS_CK, "fm_ccusys_ck", 0x0160, 7, 1),
	FMCLK2(CKGEN, FM_CAMTM_CK, "fm_camtm_ck", 0x0160, 15, 1),
	FMCLK(CKGEN, FM_SFLASH_CK, "fm_sflash_ck", 1),
	FMCLK2(CKGEN, FM_MCU_ACP_CK, "fm_mcu_acp_ck", 0x0160, 31, 1),
	FMCLK(CKGEN, FM_TL_P1, "fm_tl_p1", 1),
	/* ABIST Part */
	FMCLK(ABIST, FM_LVTS_CKMON_LM, "fm_lvts_ckmon_lm", 1),
	FMCLK(ABIST, FM_LVTS_CKMON_L9, "fm_lvts_ckmon_l9", 1),
	FMCLK(ABIST, FM_LVTS_CKMON_L8, "fm_lvts_ckmon_l8", 1),
	FMCLK(ABIST, FM_LVTS_CKMON_L7, "fm_lvts_ckmon_l7", 1),
	FMCLK(ABIST, FM_LVTS_CKMON_L6, "fm_lvts_ckmon_l6", 1),
	FMCLK(ABIST, FM_LVTS_CKMON_L5, "fm_lvts_ckmon_l5", 1),
	FMCLK(ABIST, FM_LVTS_CKMON_L4, "fm_lvts_ckmon_l4", 1),
	FMCLK(ABIST, FM_LVTS_CKMON_L3, "fm_lvts_ckmon_l3", 1),
	FMCLK(ABIST, FM_LVTS_CKMON_L2, "fm_lvts_ckmon_l2", 1),
	FMCLK(ABIST, FM_LVTS_CKMON_L1, "fm_lvts_ckmon_l1", 1),
	FMCLK(ABIST, FM_RCLRPLL_DIV4_CHD, "fm_rclrpll_div4_chd", 1),
	FMCLK(ABIST, FM_RPHYPLL_DIV4_CHD, "fm_rphypll_div4_chd", 1),
	FMCLK(ABIST, FMEM_AFT_CHD, "fmem_aft_chd", 1),
	FMCLK(ABIST, FMEM_BFE_CHD, "fmem_bfe_chd", 1),
	FMCLK(ABIST, FM_RCLRPLL_DIV4_CHC, "fm_rclrpll_div4_chc", 1),
	FMCLK(ABIST, FM_RPHYPLL_DIV4_CHC, "fm_rphypll_div4_chc", 1),
	FMCLK(ABIST, FMEM_AFT_CHC, "fmem_aft_chc", 1),
	FMCLK(ABIST, FMEM_BFE_CHC, "fmem_bfe_chc", 1),
	FMCLK(ABIST, FM_RCLRPLL_DIV4_CHB, "fm_rclrpll_div4_chb", 1),
	FMCLK(ABIST, FM_RPHYPLL_DIV4_CHB, "fm_rphypll_div4_chb", 1),
	FMCLK(ABIST, FMEM_AFT_CHB, "fmem_aft_chb", 1),
	FMCLK(ABIST, FMEM_BFE_CHB, "fmem_bfe_chb", 1),
	FMCLK(ABIST, FM_RCLRPLL_DIV4_CHA, "fm_rclrpll_div4_cha", 1),
	FMCLK(ABIST, FM_RPHYPLL_DIV4_CHA, "fm_rphypll_div4_cha", 1),
	FMCLK(ABIST, FMEM_AFT_CHA, "fmem_aft_cha", 1),
	FMCLK(ABIST, FMEM_BFE_CHA, "fmem_bfe_cha", 1),
	FMCLK(ABIST, FM_ADSPPLL_CK, "fm_adsppll_ck", 1),
	FMCLK(ABIST, FM_APLL1_CK, "fm_apll1_ck", 1),
	FMCLK(ABIST, FM_APLL2_CK, "fm_apll2_ck", 1),
	FMCLK(ABIST, FM_APPLLGP_MON_FM_CK, "fm_appllgp_mon_fm_ck", 1),
	FMCLK(ABIST, FM_ARMPLL_LL_CK, "fm_armpll_ll_ck", 1),
	FMCLK(ABIST, FM_ARMPLL_BL_CK, "fm_armpll_bl_ck", 1),
	FMCLK(ABIST, FM_ARMPLL_B_CK, "fm_armpll_b_ck", 1),
	FMCLK(ABIST, FM_CCIPLL_CK, "fm_ccipll_ck", 1),
	FMCLK(ABIST, FM_CSI0A_CDPHY_DELAYCAL_CK, "fm_csi0a_cdphy_delaycal_ck", 1),
	FMCLK(ABIST, FM_CSI0B_CDPHY_DELAYCAL_CK, "fm_csi0b_cdphy_delaycal_ck", 1),
	FMCLK(ABIST, FM_CSI1A_DPHY_DELAYCAL_CK, "fm_csi1a_dphy_delaycal_ck", 1),
	FMCLK(ABIST, FM_CSI1B_DPHY_DELAYCAL_CK, "fm_csi1b_dphy_delaycal_ck", 1),
	FMCLK(ABIST, FM_CSI2A_DPHY_DELAYCAL_CK, "fm_csi2a_dphy_delaycal_ck", 1),
	FMCLK(ABIST, FM_CSI2B_DPHY_DELAYCAL_CK, "fm_csi2b_dphy_delaycal_ck", 1),
	FMCLK(ABIST, FM_CSI3A_DPHY_DELAYCAL_CK, "fm_csi3a_dphy_delaycal_ck", 1),
	FMCLK(ABIST, FM_CSI3B_DPHY_DELAYCAL_CK, "fm_csi3b_dphy_delaycal_ck", 1),
	FMCLK(ABIST, FM_CSI4A_DPHY_DELAYCAL_CK, "fm_csi4a_dphy_delaycal_ck", 1),
	FMCLK(ABIST, FM_CSI4B_DPHY_DELAYCAL_CK, "fm_csi4b_dphy_delaycal_ck", 1),
	FMCLK(ABIST, FM_DSI0_LNTC_DSICLK, "fm_dsi0_lntc_dsiclk", 1),
	FMCLK(ABIST, FM_DSI0_MPPLL_TST_CK, "fm_dsi0_mppll_tst_ck", 1),
	FMCLK(ABIST, FM_MAINPLL_CK, "fm_mainpll_ck", 1),
	FMCLK(ABIST, FM_MDPLL1_FS26M_GUIDE, "fm_mdpll1_fs26m_guide", 1),
	FMCLK(ABIST, FM_MMPLL_CK, "fm_mmpll_ck", 1),
	FMCLK(ABIST, FM_MMPLL_D3_CK, "fm_mmpll_d3_ck", 1),
	FMCLK(ABIST, FM_MPLL_CK, "fm_mpll_ck", 1),
	FMCLK(ABIST, FM_MSDCPLL_CK, "fm_msdcpll_ck", 1),
	FMCLK(ABIST, FM_IMGPLL_CK, "fm_imgpll_ck", 1),
	FMCLK(ABIST, FM_EMIPLL_CK, "fm_emipll_ck", 1),
	FMCLK(ABIST, FM_TVDPLL_CK, "fm_tvdpll_ck", 1),
	FMCLK(ABIST, FM_ULPOSC2_MON_V_VCORE_CK, "fm_ulposc2_mon_v_vcore_ck", 1),
	FMCLK(ABIST, FM_ULPOSC_MON_VCROE_CK, "fm_ulposc_mon_vcroe_ck", 1),
	FMCLK(ABIST, FM_UNIVPLL_CK, "fm_univpll_ck", 1),
	FMCLK3(ABIST, FM_UNIVPLL_192M_CK, "fm_univpll_192m_ck", 0x030c, 2, 13),
	FMCLK(ABIST, FM_U_CLK2FREQ, "fm_u_clk2freq", 1),
	FMCLK(ABIST, FM_WBG_DIG_BPLL_CK, "fm_wbg_dig_bpll_ck", 1),
	FMCLK(ABIST, FM_WBG_DIG_WPLL_CK960, "fm_wbg_dig_wpll_ck960", 1),
	FMCLK(ABIST, FM_466M_FMEM_INFRASYS, "fm_466m_fmem_infrasys", 1),
	FMCLK(ABIST, FM_MCUSYS_ARM_OUT_ALL, "fm_mcusys_arm_out_all", 1),
	FMCLK(ABIST, FM_APPLLGP_MON_FM_CK_2, "fm_appllgp_mon_fm_ck_2", 1),
	FMCLK(ABIST, FM_F32K_VCORE_CK, "fm_f32k_vcore_ck", 1),
	FMCLK(ABIST, FM_UNIVPLL_DIV3_CK, "fm_univpll_div3_ck", 1),
	FMCLK3(ABIST, FM_APLL2_CKDIV_CK, "fm_apll2_ckdiv_ck", 0x0340, 1, 8),
	FMCLK3(ABIST, FM_APLL1_CKDIV_CK, "fm_apll1_ckdiv_ck", 0x032c, 1, 8),
	FMCLK3(ABIST, FM_ADSPPLL_CKDIV_CK, "fm_adsppll_ckdiv_ck", 0x0384, 1, 8),
	FMCLK3(ABIST, FM_TVDPLL_CKDIV_CK, "fm_tvdpll_ckdiv_ck", 0x024c, 1, 8),
	FMCLK3(ABIST, FM_MPLL_CKDIV_CK, "fm_mpll_ckdiv_ck", 0x0394, 1, 8),
	FMCLK3(ABIST, FM_MMPLL_CKDIV_CK, "fm_mmpll_ckdiv_ck", 0x03A4, 1, 8),
	FMCLK3(ABIST, FM_MAINPLL_CKDIV_CK, "fm_mainpll_ckdiv_ck", 0x0354, 1, 8),
	FMCLK3(ABIST, FM_IMGPLL_CKDIV_CK, "fm_imgpll_ckdiv_ck", 0x0374, 1, 8),
	FMCLK3(ABIST, FM_MSDCPLL_CKDIV_CK, "fm_msdcpll_ckdiv_ck", 0x0364, 1, 8),
	FMCLK(ABIST, FM_PLLGP_MON_FM_CK, "fm_pllgp_mon_fm_ck", 1),
	/* ABIST_2 Part */
	FMCLK(ABIST_2, FM_ROSC_OUT_FREQ, "fm_rosc_out_freq", 1),
	FMCLK(ABIST_2, FM_MMPLL_D4_CK, "fm_mmpll_d4_ck", 1),
	FMCLK(ABIST_2, FM_MMPLL_D3_CK_2, "fm_mmpll_d3_ck_2", 1),
	FMCLK(ABIST_2, FM_UNIV_499M_CK, "fm_univ_499m_ck", 1),
	FMCLK(ABIST_2, FM_UNIV_624M_CK, "fm_univ_624m_ck", 1),
	FMCLK(ABIST_2, FM_UNIV_832M_CK, "fm_univ_832m_ck", 1),
	FMCLK(ABIST_2, FM_MAIN_H436P8M_CK, "fm_main_h436p8m_ck", 1),
	FMCLK(ABIST_2, FM_MAIN_H546M_CK, "fm_main_h546m_ck", 1),
	FMCLK(ABIST_2, FM_MAIN_H728M_CK, "fm_main_h728m_ck", 1),
	FMCLK(ABIST_2, FM_SPMI_MST_32K_CK, "fm_spmi_mst_32k_ck", 1),
	FMCLK(ABIST_2, FM_SRCK_CK, "fm_srck_ck", 1),
	FMCLK(ABIST_2, FM_ULPOSC_CORE_CK, "fm_ulposc_core_ck", 1),
	FMCLK(ABIST_2, FM_ULPOSC_CK, "fm_ulposc_ck", 1),
	FMCLK(ABIST_2, FM_UNIPLL_SES_CK, "fm_unipll_ses_ck", 1),
	FMCLK(ABIST_2, FM_AUD_EDTM_OUT1_M_CK, "fm_aud_edtm_out1_m_ck", 1),
	FMCLK(ABIST_2, FM_AUD_EDTM_IN1_M_CK, "fm_aud_edtm_in1_m_ck", 1),
	FMCLK(ABIST_2, FM_APLL_I2S9_M_CK, "fm_apll_i2s9_m_ck", 1),
	FMCLK(ABIST_2, FM_APLL_I2S8_M_CK, "fm_apll_i2s8_m_ck", 1),
	FMCLK(ABIST_2, FM_APLL_I2S7_M_CK, "fm_apll_i2s7_m_ck", 1),
	FMCLK(ABIST_2, FM_APLL_I2S6_M_CK, "fm_apll_i2s6_m_ck", 1),
	FMCLK(ABIST_2, FM_APLL_I2S5_M_CK, "fm_apll_i2s5_m_ck", 1),
	FMCLK(ABIST_2, FM_APLL_I2S4_B_CK, "fm_apll_i2s4_b_ck", 1),
	FMCLK(ABIST_2, FM_APLL_I2S4_M_CK, "fm_apll_i2s4_m_ck", 1),
	FMCLK(ABIST_2, FM_APLL_I2S3_M_CK, "fm_apll_i2s3_m_ck", 1),
	FMCLK(ABIST_2, FM_APLL_I2S2_M_CK, "fm_apll_i2s2_m_ck", 1),
	FMCLK(ABIST_2, FM_APLL_I2S1_M_CK, "fm_apll_i2s1_m_ck", 1),
	FMCLK(ABIST_2, FM_APLL_I2S0_M_CK, "fm_apll_i2s0_m_ck", 1),
	FMCLK(ABIST_2, FM_CKMON4_CK, "fm_ckmon4_ck", 1),
	FMCLK(ABIST_2, FM_CKMON3_CK, "fm_ckmon3_ck", 1),
	FMCLK(ABIST_2, FM_CKMON2_CK, "fm_ckmon2_ck", 1),
	FMCLK(ABIST_2, FM_CKMON1_CK, "fm_ckmon1_ck", 1),
	/* VLPCK Part */
	FMCLK2(VLPCK, FM_SCP_CK, "fm_scp_ck", 0x0008, 7, 1),
	FMCLK(VLPCK, FM_SPM_CK, "fm_spm_ck", 1),
	FMCLK2(VLPCK, FM_PWRAP_ULPOSC_CK_2, "fm_pwrap_ulposc_ck_2", 0x0008, 15, 1),
	FMCLK2(VLPCK, FM_GPT_BCLK_CK, "fm_gpt_bclk_ck", 0x0008, 23, 1),
	FMCLK2(VLPCK, FM_DXCC_CK_2, "fm_dxcc_ck_2", 0x0008, 31, 1),
	FMCLK2(VLPCK, FM_SPMI_P_CK_2, "fm_spmi_p_ck_2", 0x0014, 7, 1),
	FMCLK2(VLPCK, FM_SPMI_M_CK_2, "fm_spmi_m_ck_2", 0x0014, 15, 1),
	FMCLK2(VLPCK, FM_DVFSRC_CK, "fm_dvfsrc_ck", 0x0014, 23, 1),
	FMCLK2(VLPCK, FM_PWM_VLP_CK, "fm_pwm_vlp_ck", 0x0014, 31, 1),
	FMCLK2(VLPCK, FM_AXI_VLP_CK, "fm_axi_vlp_ck", 0x0020, 7, 1),
	FMCLK2(VLPCK, FM_DBGAO_26M_CK, "fm_dbgao_26m_ck", 0x0020, 15, 1),
	FMCLK2(VLPCK, FM_SYSTIMER_26M_CK, "fm_systimer_26m_ck", 0x0020, 23, 1),
	FMCLK2(VLPCK, FM_SSPM_CK, "fm_sspm_ck", 0x0020, 31, 1),
	FMCLK2(VLPCK, FM_SSPM_F26M_CK, "fm_sspm_f26m_ck", 0x002C, 7, 1),
	FMCLK2(VLPCK, FM_APEINT_66M_CK, "fm_apeint_66m_ck", 0x002C, 15, 1),
	FMCLK2(VLPCK, FM_SRCK_CK_2, "fm_srck_ck_2", 0x002C, 23, 1),
	FMCLK2(VLPCK, FM_SRAMRC_CK, "fm_sramrc_ck", 0x002C, 31, 1),
	FMCLK(VLPCK, FM_SEJ_26M_CK, "fm_sej_26m_ck", 1),
	FMCLK(VLPCK, FM_MD_BUCK_26M_CK, "fm_md_buck_26m_ck", 1),
	FMCLK(VLPCK, FM_SSPM_ULPOSC_CK, "fm_sspm_ulposc_ck", 1),
	FMCLK(VLPCK, FM_DBGAO_66M_CK, "fm_dbgao_66m_ck", 1),
	FMCLK(VLPCK, FM_RTC_CK, "fm_rtc_ck", 1),
	FMCLK(VLPCK, FM_ULPOSC_CORE_CK_2, "fm_ulposc_core_ck_2", 1),
	FMCLK(VLPCK, FM_ULPOSC_CK_2, "fm_ulposc_ck_2", 1),
	FMCLK2(VLPCK, FM_SCP_SPI_CK, "fm_scp_spi_ck", 0x0038, 7, 1),
	FMCLK2(VLPCK, FM_SCP_IIC_CK, "fm_scp_iic_ck", 0x0038, 15, 1),
	FMCLK(VLPCK, FM_OSC_SYNC_CK, "fm_osc_sync_ck", 1),
	FMCLK(VLPCK, FM_OSC_SYNC_CK2, "fm_osc_sync_ck2", 1),
	{},
};

#define _CKGEN(x)		(topck_base + (x))
#define CLK_MISC_CFG_0		_CKGEN(0x240)
#define CLK_DBG_CFG		_CKGEN(0x28C)
#define CLK26CALI_0		_CKGEN(0x220)
#define CLK26CALI_1		_CKGEN(0x224)
#define _VLP_CK(x)		(vlp_ck_base + (x))
#define VLP_CK_CON0		_VLP_CK(0x230)
#define VLP_CK_CON1		_VLP_CK(0x234)

static void __iomem *topck_base;
static void __iomem *apmixed_base;
static void __iomem *spm_base;
static void __iomem *vlp_ck_base;

const struct fmeter_clk *mt6895_get_fmeter_clks(void)
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

	if ((ID <= 0) || (ID >= 128))
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

	if ((ID <= 0) || (ID >= 128))
		return clk_div;

	for (i = 0; i < ARRAY_SIZE(fclks) - 1; i++)
		if (fclks[i].type == type && fclks[i].id == ID)
			break;

	if (i >= ARRAY_SIZE(fclks) - 1)
		return clk_div;

	if (fclks[i].grp == 1)
		clk_div = 8;
	else
		clk_div = 1;

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
		clk_writel(CLK_DBG_CFG,
			(clk_dbg_cfg & 0xFFFF80FC) | (ID << 8) | (0x1));
	} else if (type == ABIST) {
		clk_dbg_cfg = clk_readl(CLK_DBG_CFG);
		clk_writel(CLK_DBG_CFG,
			(clk_dbg_cfg & 0xFF80FFFC) | (ID << 16));
	} else if (type == ABIST_2) {
		clk_dbg_cfg = clk_readl(CLK_DBG_CFG);
		clk_writel(CLK_DBG_CFG,
			(clk_dbg_cfg & 0xC0FFFFFC) | (ID << 24) | (0x2));
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
	if (type == VLPCK) {
		udelay(VLP_FM_WAIT_TIME);
	} else {
		while (clk_readl(con0) & 0x10) {
			udelay(10);
			i++;
			if (i > 30) {
				pr_notice("[%d]con0: 0x%x, con1: 0x%x\n",
					id, clk_readl(con0), clk_readl(con1));
				break;
			}
		}
	}

	temp = clk_readl(con1) & 0xFFFF;
	output = ((temp * 26000)) / 1024; // Khz

	clk_writel(con0, 0x8000);

	fmeter_unlock(flags);

	return output * 4;
}

static unsigned int mt6895_get_ckgen_freq(unsigned int ID)
{
	return __mt_get_freq(ID, CKGEN);
}

static unsigned int mt6895_get_abist_freq(unsigned int ID)
{
	return __mt_get_freq(ID, ABIST);
}

static unsigned int mt6895_get_abist2_freq(unsigned int ID)
{
	return __mt_get_freq(ID, ABIST_2);
}

static unsigned int mt6895_get_vlpck_freq(unsigned int ID)
{
	return __mt_get_freq2(VLP_CK_CON0, VLP_CK_CON1, ID, VLPCK);
}

static unsigned int mt6895_get_subsys_freq(unsigned int ID)
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
	if (subsys_idx >= (SUBSYS_PLL_NUM * FM_SYS_NUM)) {
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

static unsigned int mt6895_get_fmeter_freq(unsigned int id,
		enum FMETER_TYPE type)
{
	if (type == CKGEN)
		return mt6895_get_ckgen_freq(id);
	else if (type == ABIST)
		return mt6895_get_abist_freq(id);
	else if (type == ABIST_2)
		return mt6895_get_abist2_freq(id);
	else if (type == SUBSYS)
		return mt6895_get_subsys_freq(id);
	else if (type == VLPCK)
		return mt6895_get_vlpck_freq(id);

	return FT_NULL;
}

static int mt6895_subsys_freq_register(struct fm_subsys *fm, unsigned int size)
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

static int mt6895_get_fmeter_id(enum FMETER_ID fid)
{
	if (fid == FID_DISP_PWM)
		return FM_DISP_PWM_CK;
	else if (fid == FID_ULPOSC1)
		return FM_ULPOSC_CK_2;
	else if (fid == FID_ULPOSC2)
		return FM_ULPOSC_CORE_CK_2;

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
			pr_err("%s() can't find iomem for %s\n",
					__func__, comp);
			return ERR_PTR(-EINVAL);
		}

		return base;
	}

	pr_err("%s can't find compatible node\n", __func__);

	return ERR_PTR(-EINVAL);
}

/*
 * init functions
 */

static struct fmeter_ops fm_ops = {
	.get_fmeter_clks = mt6895_get_fmeter_clks,
	.get_fmeter_freq = mt6895_get_fmeter_freq,
	.subsys_freq_register = mt6895_subsys_freq_register,
	.get_fmeter_id = mt6895_get_fmeter_id,
};

static int clk_fmeter_mt6895_probe(struct platform_device *pdev)
{
	int i;

	topck_base = get_base_from_comp("mediatek,mt6895-topckgen");
	if (IS_ERR(topck_base))
		goto ERR;

	apmixed_base = get_base_from_comp("mediatek,mt6895-apmixedsys");
	if (IS_ERR(apmixed_base))
		goto ERR;

	spm_base = get_base_from_comp("mediatek,mt6895-scpsys");
	if (IS_ERR(spm_base))
		goto ERR;

	vlp_ck_base = get_base_from_comp("mediatek,mt6895-vlp_cksys");
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
	pr_err("%s can't find base\n", __func__);

	return -EINVAL;
}

static struct platform_driver clk_fmeter_mt6895_drv = {
	.probe = clk_fmeter_mt6895_probe,
	.driver = {
		.name = "clk-fmeter-mt6895",
		.owner = THIS_MODULE,
	},
};

static int __init clk_fmeter_init(void)
{
	static struct platform_device *clk_fmeter_dev;

	clk_fmeter_dev = platform_device_register_simple("clk-fmeter-mt6895", -1, NULL, 0);
	if (IS_ERR(clk_fmeter_dev))
		pr_warn("unable to register clk-fmeter device");

	return platform_driver_register(&clk_fmeter_mt6895_drv);
}

static void __exit clk_fmeter_exit(void)
{
	platform_driver_unregister(&clk_fmeter_mt6895_drv);
}

subsys_initcall(clk_fmeter_init);
module_exit(clk_fmeter_exit);
MODULE_LICENSE("GPL");

