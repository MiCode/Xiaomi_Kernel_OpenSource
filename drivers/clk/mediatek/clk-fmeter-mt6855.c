// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2021 MediaTek Inc.
// Author: Owen Chen <owen.chen@mediatek.com>

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "clk-fmeter.h"
#include "clk-mt6855-fmeter.h"

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
	FMCLK2(CKGEN, FM_AXI_CK, "fm_axi_ck", 0x0010, 7, 1),
	FMCLK(CKGEN, FM_AXIP_CK, "fm_axip_ck", 1),
	FMCLK(CKGEN, FM_HAXI_U_CK, "fm_haxi_u_ck", 1),
	FMCLK2(CKGEN, FM_B, "fm_b", 0x0010, 31, 1),
	FMCLK(CKGEN, FM_DISP0_CK, "fm_disp0_ck", 1),
	FMCLK(CKGEN, FM_MDP0_CK, "fm_mdp0_ck", 1),
	FMCLK(CKGEN, FM_MMINFRA_CK, "fm_mminfra_ck", 1),
	FMCLK(CKGEN, FM_MMUP_CK, "fm_mmup_ck", 1),
	FMCLK2(CKGEN, FM_CAMTG_CK, "fm_camtg_ck", 0x0030, 7, 1),
	FMCLK2(CKGEN, FM_CAMTG2_CK, "fm_camtg2_ck", 0x0030, 15, 1),
	FMCLK2(CKGEN, FM_CAMTG3_CK, "fm_camtg3_ck", 0x0030, 23, 1),
	FMCLK2(CKGEN, FM_CAMTG4_CK, "fm_camtg4_ck", 0x0030, 31, 1),
	FMCLK2(CKGEN, FM_CAMTG5_CK, "fm_camtg5_ck", 0x0040, 7, 1),
	FMCLK2(CKGEN, FM_CAMTG6_CK, "fm_camtg6_ck", 0x0040, 15, 1),
	FMCLK(CKGEN, FM_UART_CK, "fm_uart_ck", 1),
	FMCLK2(CKGEN, FM_SPI_CK, "fm_spi_ck", 0x0040, 31, 1),
	FMCLK(CKGEN, FM_MSDC_0P_MACRO_CK, "fm_msdc_0p_macro_ck", 1),
	FMCLK(CKGEN, FM_MSDC5HCLK_CK, "fm_msdc5hclk_ck", 1),
	FMCLK(CKGEN, FM_MSDC50_0_CK, "fm_msdc50_0_ck", 1),
	FMCLK(CKGEN, FM_AES_MSDCFDE_CK, "fm_aes_msdcfde_ck", 1),
	FMCLK(CKGEN, FM_MSDC_MACRO_CK, "fm_msdc_macro_ck", 1),
	FMCLK(CKGEN, FM_MSDC30_1_CK, "fm_msdc30_1_ck", 1),
	FMCLK(CKGEN, FM_AUDIO_CK, "fm_audio_ck", 1),
	FMCLK(CKGEN, FM_AUD_INTBUS_CK, "fm_aud_intbus_ck", 1),
	FMCLK(CKGEN, FM_ATB_CK, "fm_atb_ck", 1),
	FMCLK(CKGEN, FM_DISP_PWM_CK, "fm_disp_pwm_ck", 1),
	FMCLK(CKGEN, FM_USB_CK, "fm_usb_ck", 1),
	FMCLK(CKGEN, FM_USB_XHCI_CK, "fm_usb_xhci_ck", 1),
	FMCLK2(CKGEN, FM_I2C_CK, "fm_i2c_ck", 0x0080, 7, 1),
	FMCLK(CKGEN, FM_SENINF_CK, "fm_seninf_ck", 1),
	FMCLK(CKGEN, FM_SENINF1_CK, "fm_seninf1_ck", 1),
	FMCLK(CKGEN, FM_SENINF2_CK, "fm_seninf2_ck", 1),
	FMCLK(CKGEN, FM_SENINF3_CK, "fm_seninf3_ck", 1),
	FMCLK(CKGEN, FM_DXCC_CK, "fm_dxcc_ck", 1),
	FMCLK(CKGEN, FM_AUD_ENGEN1_CK, "fm_aud_engen1_ck", 1),
	FMCLK(CKGEN, FM_AUD_ENGEN2_CK, "fm_aud_engen2_ck", 1),
	FMCLK(CKGEN, FM_AES_UFSFDE_CK, "fm_aes_ufsfde_ck", 1),
	FMCLK(CKGEN, FM_U_CK, "fm_u_ck", 1),
	FMCLK(CKGEN, FM_U_MBIST_CK, "fm_u_mbist_ck", 1),
	FMCLK(CKGEN, FM_AUD_1_CK, "fm_aud_1_ck", 1),
	FMCLK(CKGEN, FM_AUD_2_CK, "fm_aud_2_ck", 1),
	FMCLK2(CKGEN, FM_DPMAIF_MAIN_CK, "fm_dpmaif_main_ck", 0x00B0, 15, 1),
	FMCLK(CKGEN, FM_VENC_CK, "fm_venc_ck", 1),
	FMCLK(CKGEN, FM_VDEC_CK, "fm_vdec_ck", 1),
	FMCLK2(CKGEN, FM_PWM_CK, "fm_pwm_ck", 0x00C0, 7, 1),
	FMCLK(CKGEN, FM_AUDIO_H_CK, "fm_audio_h_ck", 1),
	FMCLK(CKGEN, FM_MCUPM_CK, "fm_mcupm_ck", 1),
	FMCLK2(CKGEN, FM_MEM_SUB_CK, "fm_mem_sub_ck", 0x00C0, 31, 1),
	FMCLK(CKGEN, FM_MEM_SUBP_CK, "fm_mem_subp_ck", 1),
	FMCLK(CKGEN, FM_MEM_SUB_U_CK, "fm_mem_sub_u_ck", 1),
	FMCLK2(CKGEN, FM_EMI_N_CK, "fm_emi_n_ck", 0x00D0, 23, 1),
	FMCLK(CKGEN, FM_DSI_OCC_CK, "fm_dsi_occ_ck", 1),
	FMCLK(CKGEN, FM_AP2CONN_HOST_CK, "fm_ap2conn_host_ck", 1),
	FMCLK(CKGEN, FM_MCU_ACP_CK, "fm_mcu_acp_ck", 1),
	FMCLK(CKGEN, FM_IMG1_CK, "fm_img1_ck", 1),
	FMCLK(CKGEN, FM_IPE_CK, "fm_ipe_ck", 1),
	FMCLK(CKGEN, FM_CAM_CK, "fm_cam_ck", 1),
	FMCLK(CKGEN, FM_CAMTM_CK, "fm_camtm_ck", 1),
	FMCLK(CKGEN, FM_MSDC_1P_RX_CK, "fm_msdc_1p_rx_ck", 1),
	/* ABIST Part */
	FMCLK(ABIST, FM_APLL1_CK, "fm_apll1_ck", 1),
	FMCLK(ABIST, FM_APLL2_CK, "fm_apll2_ck", 1),
	FMCLK(ABIST, FM_APPLLGP_MON_FM_CK, "fm_appllgp_mon_fm_ck", 1),
	FMCLK(ABIST, FM_ARMPLL_BL_CK, "fm_armpll_bl_ck", 1),
	FMCLK3(ABIST, FM_ARMPLL_BL_CKDIV_CK, "fm_armpll_bl_ckdiv_ck", 0x021c, 1, 8),
	FMCLK(ABIST, FM_ARMPLL_LL_CK, "fm_armpll_ll_ck", 1),
	FMCLK3(ABIST, FM_ARMPLL_LL_CKDIV_CK, "fm_armpll_ll_ckdiv_ck", 0x020c, 1, 8),
	FMCLK(ABIST, FM_CCIPLL_CK, "fm_ccipll_ck", 1),
	FMCLK3(ABIST, FM_CCIPLL_CKDIV_CK, "fm_ccipll_ckdiv_ck", 0x023c, 1, 8),
	FMCLK(ABIST, FM_CSI0A_DELAYCAL_CK, "fm_csi0a_delaycal_ck", 1),
	FMCLK(ABIST, FM_CSI0B_DELAYCAL_CK, "fm_csi0b_delaycal_ck", 1),
	FMCLK(ABIST, FM_CSI1A_DEKAYCAL_CK, "fm_csi1a_dekaycal_ck", 1),
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
	FMCLK(ABIST, FM_RCKRPLL_DIV4_CH01, "fm_rckrpll_div4_ch01", 1),
	FMCLK(ABIST, FM_IMGPLL_CK, "fm_imgpll_ck", 1),
	FMCLK(ABIST, FM_RPHYPLL_DIV4_CH01, "fm_rphypll_div4_ch01", 1),
	FMCLK(ABIST, FM_EMIPLL_CK, "fm_emipll_ck", 1),
	FMCLK(ABIST, FM_TVDPLL_CK, "fm_tvdpll_ck", 1),
	FMCLK(ABIST, FM_ULPOSC2_MON_V_VCORE_CK, "fm_ulposc2_mon_v_vcore_ck", 1),
	FMCLK(ABIST, FM_ULPOSC_MON_VCORE_CK, "fm_ulposc_mon_vcore_ck", 1),
	FMCLK(ABIST, FM_UNIVPLL_CK, "fm_univpll_ck", 1),
	FMCLK3(ABIST, FM_UNIVPLL_192M_CK, "fm_univpll_192m_ck", 0x030c, 2, 13),
	FMCLK(ABIST, FM_U_CLK2FREQ, "fm_u_clk2freq", 1),
	FMCLK(ABIST, FM_WBG_DIG_BPLL_CK, "fm_wbg_dig_bpll_ck", 1),
	FMCLK(ABIST, FM_WBG_DIG_WPLL_CK960, "fm_wbg_dig_wpll_ck960", 1),
	FMCLK(ABIST, FMEM_AFT_CH0, "fmem_aft_ch0", 1),
	FMCLK(ABIST, FMEM_AFT_CH1, "fmem_aft_ch1", 1),
	FMCLK(ABIST, FMEM_BFE_CH0, "fmem_bfe_ch0", 1),
	FMCLK(ABIST, FM_GMEM_BFE_CH1, "fm_gmem_bfe_ch1", 1),
	FMCLK(ABIST, FM_466M_FMEM_INFRASYS, "fm_466m_fmem_infrasys", 1),
	FMCLK(ABIST, FM_MCUSYS_ARM_OUT_ALL, "fm_mcusys_arm_out_all", 1),
	FMCLK(ABIST, FM_MSDC11_IN_CK, "fm_msdc11_in_ck", 1),
	FMCLK(ABIST, FM_MSDC12_IN_CK, "fm_msdc12_in_ck", 1),
	FMCLK(ABIST, FM_MSDC21_IN_CK, "fm_msdc21_in_ck", 1),
	FMCLK(ABIST, FM_MSDC22_IN_CK, "fm_msdc22_in_ck", 1),
	FMCLK(ABIST, FM_F32K_VCORE_CK, "fm_f32k_vcore_ck", 1),
	FMCLK(ABIST, FM_ADA_LVTS_TO_PLLGP_MONCK_L6, "fm_ada_lvts_to_pllgp_monck_l6", 1),
	FMCLK(ABIST, FM_ADA_LVTS_TO_PLLGP_MONCK_L5, "fm_ada_lvts_to_pllgp_monck_l5", 1),
	FMCLK(ABIST, FM_ADA_LVTS_TO_PLLGP_MONCK_L4, "fm_ada_lvts_to_pllgp_monck_l4", 1),
	FMCLK(ABIST, FM_ADA_LVTS_TO_PLLGP_MONCK_L3, "fm_ada_lvts_to_pllgp_monck_l3", 1),
	FMCLK(ABIST, FM_ADA_LVTS_TO_PLLGP_MONCK_L2, "fm_ada_lvts_to_pllgp_monck_l2", 1),
	FMCLK(ABIST, FM_ADA_LVTS_TO_PLLGP_MONCK_L1, "fm_ada_lvts_to_pllgp_monck_l1", 1),
	FMCLK(ABIST, FM_LVTS_TO_PLLGP_MON_LM, "fm_lvts_to_pllgp_mon_lm", 1),
	FMCLK3(ABIST, FM_APLL1_CKDIV_CK, "fm_apll1_ckdiv_ck", 0x032c, 1, 8),
	FMCLK3(ABIST, FM_APLL2_CKDIV_CK, "fm_apll2_ckdiv_ck", 0x0340, 1, 8),
	FMCLK3(ABIST, FM_MPLL_CKDIV_CK, "fm_mpll_ckdiv_ck", 0x0394, 1, 8),
	FMCLK3(ABIST, FM_TVDPLL_CKDIV_CK, "fm_tvdpll_ckdiv_ck", 0x024c, 1, 8),
	FMCLK3(ABIST, FM_IMGPLL_CKDIV_CK, "fm_imgpll_ckdiv_ck", 0x0374, 1, 8),
	FMCLK3(ABIST, FM_EMIPLL_CKDIV_CK, "fm_emipll_ckdiv_ck", 0x03B4, 1, 8),
	FMCLK3(ABIST, FM_MSDCPLL_CKDIV_CK, "fm_msdcpll_ckdiv_ck", 0x0364, 1, 8),
	/* ABIST_2 Part */
	FMCLK(ABIST_2, FM_CKMON4_CK, "fm_ckmon4_ck", 1),
	FMCLK(ABIST_2, FM_CKMON3_CK, "fm_ckmon3_ck", 1),
	FMCLK(ABIST_2, FM_CKMON2_CK, "fm_ckmon2_ck", 1),
	FMCLK(ABIST_2, FM_CKMON1_CK, "fm_ckmon1_ck", 1),
	FMCLK(ABIST_2, FM_PMSRCK_CK, "fm_pmsrck_ck", 1),
	FMCLK(ABIST_2, FM_MMPLL_D9_CK, "fm_mmpll_d9_ck", 1),
	FMCLK(ABIST_2, FM_MMPLL_D7_CK, "fm_mmpll_d7_ck", 1),
	FMCLK(ABIST_2, FM_MMPLL_D6_CK, "fm_mmpll_d6_ck", 1),
	FMCLK(ABIST_2, FM_MMPLL_D5_CK, "fm_mmpll_d5_ck", 1),
	FMCLK(ABIST_2, FM_MMPLL_D4_CK, "fm_mmpll_d4_ck", 1),
	FMCLK(ABIST_2, FM_MMPLL_D3_CK_2, "fm_mmpll_d3_ck_2", 1),
	FMCLK(ABIST_2, FM_UNIV_416M_CK, "fm_univ_416m_ck", 1),
	FMCLK(ABIST_2, FM_UNIV_499M_CK, "fm_univ_499m_ck", 1),
	FMCLK(ABIST_2, FM_UNIV_624M_CK, "fm_univ_624m_ck", 1),
	FMCLK(ABIST_2, FM_UNIV_832M_CK, "fm_univ_832m_ck", 1),
	FMCLK(ABIST_2, FM_UNIV_1248M_CK, "fm_univ_1248m_ck", 1),
	FMCLK(ABIST_2, FM_MAIN_H312M_CK, "fm_main_h312m_ck", 1),
	FMCLK(ABIST_2, FM_MAIN_H436P8M_CK, "fm_main_h436p8m_ck", 1),
	FMCLK(ABIST_2, FM_MAIN_H546M_CK, "fm_main_h546m_ck", 1),
	FMCLK(ABIST_2, FM_MAIN_H728M_CK, "fm_main_h728m_ck", 1),
	FMCLK(ABIST_2, FM_MAINPLL_1092M_CORE_CK, "fm_mainpll_1092m_core_ck", 1),
	FMCLK(ABIST_2, FM_SPMI_MST_32K_CK, "fm_spmi_mst_32k_ck", 1),
	FMCLK(ABIST_2, FM_SRCK_CK, "fm_srck_ck", 1),
	FMCLK(ABIST_2, FM_UNIPLL_SES_CK, "fm_unipll_ses_ck", 1),
	FMCLK(ABIST_2, FM_APLL_I2S5_M_CK, "fm_apll_i2s5_m_ck", 1),
	FMCLK(ABIST_2, FM_APLL_I2S4_B_CK, "fm_apll_i2s4_b_ck", 1),
	FMCLK(ABIST_2, FM_APLL_I2S4_M_CK, "fm_apll_i2s4_m_ck", 1),
	FMCLK(ABIST_2, FM_APLL_I2S3_M_CK, "fm_apll_i2s3_m_ck", 1),
	FMCLK(ABIST_2, FM_APLL_I2S2_M_CK, "fm_apll_i2s2_m_ck", 1),
	FMCLK(ABIST_2, FM_APLL_I2S1_M_CK, "fm_apll_i2s1_m_ck", 1),
	FMCLK(ABIST_2, FM_APLL_I2S0_M_CK, "fm_apll_i2s0_m_ck", 1),
	{},
};

#define _CKGEN(x)		(topck_base + (x))
#define CLK_MISC_CFG_0		_CKGEN(0x140)
#define CLK_DBG_CFG		_CKGEN(0x17C)
#define CLK26CALI_0		_CKGEN(0x220)
#define CLK26CALI_1		_CKGEN(0x224)
#define _APMIXED(x)		(apmixed_base + (x))
#define AP_PLLGP1_CON0		_APMIXED(0x200)

static void __iomem *topck_base;
static void __iomem *apmixed_base;
static void __iomem *spm_base;
static void __iomem *vlp_ck_base;

const struct fmeter_clk *mt6855_get_fmeter_clks(void)
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

	if ((ID <= 0) || (ID >= 64))
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

	if ((ID <= 0) || (ID >= 64))
		return clk_div;

	for (i = 0; i < ARRAY_SIZE(fclks) - 1; i++)
		if (fclks[i].type == type && fclks[i].id == ID)
			break;

	if (i >= ARRAY_SIZE(fclks) - 1)
		return clk_div;

	return fclks[i].ck_div;
}

/* need implement ckgen&abist api here */

/* implement done */

static unsigned int mt6855_get_ckgen_freq(unsigned int ID)
{
	return __mt_get_freq(ID, CKGEN);
}

static unsigned int mt6855_get_abist_freq(unsigned int ID)
{
	return __mt_get_freq(ID, ABIST);
}

static unsigned int mt6855_get_abist2_freq(unsigned int ID)
{
	return __mt_get_freq(ID, ABIST_2);
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

static int clk_fmeter_mt6855_probe(struct platform_device *pdev)
{
	topck_base = get_base_from_comp("mediatek,mt6855-topckgen");
	if (IS_ERR(topck_base))
		goto ERR;

	apmixed_base = get_base_from_comp("mediatek,mt6855-apmixedsys");
	if (IS_ERR(apmixed_base))
		goto ERR;

	spm_base = get_base_from_comp("mediatek,mt6855-scpsys");
	if (IS_ERR(spm_base))
		goto ERR;

	return 0;
ERR:
	pr_err("%s can't find base\n", __func__);

	return -EINVAL;
}

static struct platform_driver clk_fmeter_mt6855_drv = {
	.probe = clk_fmeter_mt6855_probe,
	.driver = {
		.name = "clk-fmeter-mt6855",
		.owner = THIS_MODULE,
	},
};

static int __init clk_fmeter_init(void)
{
	static struct platform_device *clk_fmeter_dev;

	clk_fmeter_dev = platform_device_register_simple("clk-fmeter-mt6855", -1, NULL, 0);
	if (IS_ERR(clk_fmeter_dev))
		pr_warn("unable to register clk-fmeter device");

	return platform_driver_register(&clk_fmeter_mt6855_drv);
}

static void __exit clk_fmeter_exit(void)
{
	platform_driver_unregister(&clk_fmeter_mt6855_drv);
}

subsys_initcall(clk_fmeter_init);
module_exit(clk_fmeter_exit);
MODULE_LICENSE("GPL");
