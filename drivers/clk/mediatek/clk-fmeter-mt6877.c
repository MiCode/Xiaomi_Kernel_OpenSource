/*
 * Mediatek's MT6877 SoC device tree source
 *
 * Copyright (C) 2021 MediaTek Inc.
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


#include <linux/delay.h>
#include <linux/of_address.h>
#include <linux/slab.h>

#include "clk-fmeter.h"
#include "clkchk.h"

#define SUBSYS_PLL_NUM		4

static DEFINE_SPINLOCK(pdn_lock);
#define pdn_lock(flags)   spin_lock_irqsave(&pdn_lock, flags)
#define pdn_unlock(flags) spin_unlock_irqrestore(&pdn_lock, flags)

static DEFINE_SPINLOCK(meter_lock);
#define fmeter_lock(flags)   spin_lock_irqsave(&meter_lock, flags)
#define fmeter_unlock(flags) spin_unlock_irqrestore(&meter_lock, flags)

static DEFINE_SPINLOCK(subsys_meter_lock);
#define subsys_fmeter_lock(flags)	\
	spin_lock_irqsave(&subsys_meter_lock, flags)
#define subsys_fmeter_unlock(flags)	\
	spin_unlock_irqrestore(&subsys_meter_lock, flags)

/*
 * clkdbg fmeter
 */

#define clk_readl(addr)		readl(addr)
#define clk_writel(addr, val)	\
	do { writel(val, addr); wmb(); } while (0) /* sync write */

#define FMCLK3(_t, _i, _n, _o, _g) { .type = _t, \
		.id = _i, .name = _n, .ofs = _o, .grp = _g}
#define FMCLK2(_t, _i, _n, _o, _p) { .type = _t, \
		.id = _i, .name = _n, .ofs = _o, .pdn = _p}
#define FMCLK(_t, _i, _n) { .type = _t, .id = _i, .name = _n}

static struct fmeter_clk *fm_all_clks;
static struct fm_subsys *fm_sub_clks;
static unsigned int fm_clk_cnt;

static const struct fmeter_clk fclks[] = {
	/* CKGEN Part */
	FMCLK2(CKGEN, FM_AXI_CK, "fm_axi_ck", 0x0010, 7),
	FMCLK2(CKGEN, FM_SPM_CK, "fm_spm_ck", 0x0010, 15),
	FMCLK2(CKGEN, FM_SCP_CK, "fm_scp_ck", 0x0010, 23),
	FMCLK2(CKGEN, FM_BUS_CK, "fm_bus_ck", 0x0010, 31),
	FMCLK2(CKGEN, FM_DISP0_CK, "fm_disp0_ck", 0x0020, 7),
	FMCLK2(CKGEN, FM_MDP0_CK, "fm_mdp0_ck", 0x0020, 15),
	FMCLK2(CKGEN, FM_IMG1_CK, "fm_img1_ck", 0x0020, 23),
	FMCLK2(CKGEN, FM_IPE_CK, "fm_ipe_ck", 0x0020, 31),
	FMCLK2(CKGEN, FM_DPE_CK, "fm_dpe_ck", 0x0030, 7),
	FMCLK2(CKGEN, FM_CAM_CK, "fm_cam_ck", 0x0030, 15),
	FMCLK2(CKGEN, FM_CCU_CK, "fm_ccu_ck", 0x0030, 23),
	FMCLK2(CKGEN, FM_DSP_CK, "fm_dsp_ck", 0x0030, 31),
	FMCLK2(CKGEN, FM_DSP1_CK, "fm_dsp1_ck", 0x0040, 7),
	FMCLK2(CKGEN, FM_DSP2_CK, "fm_dsp2_ck", 0x0040, 15),
	FMCLK2(CKGEN, FM_DSP4_CK, "fm_dsp4_ck", 0x0040, 31),
	FMCLK2(CKGEN, FM_DSP7_CK, "fm_dsp7_ck", 0x0050, 23),
	FMCLK2(CKGEN, FM_FCAMTG_CK, "fm_fcamtg_ck", 0x0050, 31),
	FMCLK2(CKGEN, FM_FCAMTG2_CK, "fm_fcamtg2_ck", 0x0060, 7),
	FMCLK2(CKGEN, FM_FCAMTG3_CK, "fm_fcamtg3_ck", 0x0060, 15),
	FMCLK2(CKGEN, FM_FCAMTG4_CK, "fm_fcamtg4_ck", 0x0060, 23),
	FMCLK2(CKGEN, FM_FCAMTG5_CK, "fm_fcamtg5_ck", 0x0060, 31),
	FMCLK2(CKGEN, FM_FUART_CK, "fm_fuart_ck", 0x0070, 7),
	FMCLK2(CKGEN, FM_SPI_CK, "fm_spi_ck", 0x0070, 15),
	FMCLK2(CKGEN, FM_MSDC5HCLK_CK, "fm_msdc5hclk_ck", 0x0070, 23),
	FMCLK2(CKGEN, FM_MSDC50_0_CK, "fm_msdc50_0_ck", 0x0070, 31),
	FMCLK2(CKGEN, FM_MSDC30_1_CK, "fm_msdc30_1_ck", 0x0080, 7),
	FMCLK2(CKGEN, FM_AUDIO_CK, "fm_audio_ck", 0x0080, 15),
	FMCLK2(CKGEN, FM_AUD_INTBUS_CK, "fm_aud_intbus_ck", 0x0080, 23),
	FMCLK2(CKGEN, FM_FPWRAP_ULPOSC_CK, "fm_fpwrap_ulposc_ck", 0x0080, 31),
	FMCLK2(CKGEN, FM_ATB_CK, "fm_atb_ck", 0x0090, 7),
	FMCLK2(CKGEN, FM_SSPM_CK, "fm_sspm_ck", 0x0090, 15),
	FMCLK2(CKGEN, FM_FDISP_PWM_CK, "fm_fdisp_pwm_ck", 0x0090, 31),
	FMCLK2(CKGEN, FM_FUSB_CK, "fm_fusb_ck", 0x00A0, 7),
	FMCLK2(CKGEN, FM_FSSUSB_XHCI_CK, "fm_fssusb_xhci_ck", 0x00A0, 15),
	FMCLK2(CKGEN, FM_I2C_CK, "fm_i2c_ck", 0x00B0, 7),
	FMCLK2(CKGEN, FM_FSENINF_CK, "fm_fseninf_ck", 0x00B0, 15),
	FMCLK2(CKGEN, FM_FSENINF1_CK, "fm_fseninf1_ck", 0x00B0, 23),
	FMCLK2(CKGEN, FM_FSENINF2_CK, "fm_fseninf2_ck", 0x00B0, 31),
	FMCLK2(CKGEN, FM_FSENINF3_CK, "fm_fseninf3_ck", 0x00C0, 7),
	FMCLK2(CKGEN, FM_DXCC_CK, "fm_dxcc_ck", 0x00C0, 15),
	FMCLK2(CKGEN, FM_AUD_ENGEN1_CK, "fm_aud_engen1_ck", 0x00C0, 23),
	FMCLK2(CKGEN, FM_AUD_ENGEN2_CK, "fm_aud_engen2_ck", 0x00C0, 31),
	FMCLK2(CKGEN, FM_AES_UFSFDE_CK, "fm_aes_ufsfde_ck", 0x00D0, 7),
	FMCLK2(CKGEN, FM_UFS_CK, "fm_ufs_ck", 0x00D0, 15),
	FMCLK2(CKGEN, FM_AUD_1_CK, "fm_aud_1_ck", 0x00D0, 23),
	FMCLK2(CKGEN, FM_AUD_2_CK, "fm_aud_2_ck", 0x00D0, 31),
	FMCLK2(CKGEN, FM_ADSP_CK, "fm_adsp_ck", 0x00E0, 7),
	FMCLK2(CKGEN, FM_DPMAIF_MAIN_CK, "fm_dpmaif_main_ck", 0x00E0, 15),
	FMCLK2(CKGEN, FM_VENC_CK, "fm_venc_ck", 0x00E0, 23),
	FMCLK2(CKGEN, FM_VDEC_CK, "fm_vdec_ck", 0x00E0, 31),
	FMCLK2(CKGEN, FM_CAMTM_CK, "fm_camtm_ck", 0x00F0, 7),
	FMCLK2(CKGEN, FM_PWM_CK, "fm_pwm_ck", 0x00F0, 15),
	FMCLK2(CKGEN, FM_AUDIO_H_CK, "fm_audio_h_ck", 0x00F0, 23),
	FMCLK2(CKGEN, FM_MCUPM_CK, "fm_mcupm_ck", 0x00F0, 31),
	FMCLK2(CKGEN, FM_SPMI_M_MST_CK, "fm_spmi_m_mst_ck", 0x0100, 15),
	FMCLK2(CKGEN, FM_DVFSRC_CK, "fm_dvfsrc_ck", 0x0100, 23),
	/* ABIST Part */
	FMCLK3(ABIST, FM_ADSPPLL_CKDIV_CK, "fm_adsppll_ckdiv_ck", 0x0384, 1),
	FMCLK3(ABIST, FM_APLL1_CKDIV_CK, "fm_apll1_ckdiv_ck", 0x032c, 1),
	FMCLK3(ABIST, FM_APLL2_CKDIV_CK, "fm_apll2_ckdiv_ck", 0x0340, 1),
	FMCLK(ABIST, FM_APPLLGP_MON_FM_CK, "fm_appllgp_mon_fm_ck"),
	FMCLK3(ABIST, FM_ARMPLL_BL_CKDIV_CK, "fm_armpll_bl_ckdiv_ck", 0x021c, 1),
	FMCLK3(ABIST, FM_ARMPLL_LL_CKDIV_CK, "fm_armpll_ll_ckdiv_ck", 0x020c, 1),
	FMCLK3(ABIST, FM_USBPLL_CKDIV_CK, "fm_usbpll_ckdiv_ck", 0x031C, 2),
	FMCLK3(ABIST, FM_CCIPLL_CKDIV_CK, "fm_ccipll_ckdiv_ck", 0x023c, 1),
	FMCLK(ABIST, FM_CSI0A_CDPHY_DELAYCAL_CK, "fm_csi0a_cdphy_delaycal_ck"),
	FMCLK(ABIST, FM_CSI0B_CDPHY_DELAYCAL_CK, "fm_csi0b_cdphy_delaycal_ck"),
	FMCLK(ABIST, FM_CSI1A_DPHY_DELAYCAL_CK, "fm_csi1a_dphy_delaycal_ck"),
	FMCLK(ABIST, FM_CSI1B_DPHY_DELAYCAL_CK, "fm_csi1b_dphy_delaycal_ck"),
	FMCLK(ABIST, FM_CSI2A_DPHY_DELAYCAL_CK, "fm_csi2a_dphy_delaycal_ck"),
	FMCLK(ABIST, FM_CSI2B_DPHY_DELAYCAL_CK, "fm_csi2b_dphy_delaycal_ck"),
	FMCLK(ABIST, FM_CSI3A_DPHY_DELAYCAL_CK, "fm_csi3a_dphy_delaycal_ck"),
	FMCLK(ABIST, FM_CSI3B_DPHY_DELAYCAL_CK, "fm_csi3b_dphy_delaycal_ck"),
	FMCLK(ABIST, FM_DSI0_LNTC_DSICLK, "fm_dsi0_lntc_dsiclk"),
	FMCLK(ABIST, FM_DSI0_MPPLL_TST_CK, "fm_dsi0_mppll_tst_ck"),
	FMCLK3(ABIST, FM_MAINPLL_CKDIV_CK, "fm_mainpll_ckdiv_ck", 0x0354, 1),
	FMCLK(ABIST, FM_MDPLL_FS26M_GUIDE, "fm_mdpll_fs26m_guide"),
	FMCLK3(ABIST, FM_MMPLL_CKDIV_CK, "fm_mmpll_ckdiv_ck", 0x03A4, 1),
	FMCLK(ABIST, FM_MMPLL_D3_CK, "fm_mmpll_d3_ck"),
	FMCLK3(ABIST, FM_MPLL_CKDIV_CK, "fm_mpll_ckdiv_ck", 0x0394, 1),
	FMCLK3(ABIST, FM_MSDCPLL_CKDIV_CK, "fm_msdcpll_ckdiv_ck", 0x0364, 1),
	FMCLK(ABIST, FM_RCLRPLL_DIV4_CK, "fm_rclrpll_div4_ck"),
	FMCLK(ABIST, FM_RPHYPLL_DIV4_CK, "fm_rphypll_div4_ck"),
	FMCLK3(ABIST, FM_TVDPLL_CKDIV_CK, "fm_tvdpll_ckdiv_ck", 0x024c, 1),
	FMCLK(ABIST, FM_ULPOSC2_CK, "fm_ulposc2_ck"),
	FMCLK(ABIST, FM_ULPOSC_CK, "fm_ulposc_ck"),
	FMCLK3(ABIST, FM_UNIVPLL_CKDIV_CK, "fm_univpll_ckdiv_ck", 0x030c, 2),
	FMCLK(ABIST, FM_USB20_192M_OPP_CK, "fm_usb20_192m_opp_ck"),
	FMCLK(ABIST, FM_UFS_MP_CLK2FREQ, "fm_ufs_mp_clk2freq"),
	FMCLK(ABIST, FM_WBG_DIG_BPLL_CK, "fm_wbg_dig_bpll_ck"),
	FMCLK(ABIST, FM_WBG_DIG_WPLL_CK960, "fm_wbg_dig_wpll_ck960"),
	FMCLK(ABIST, FMEM_AFT_CH0, "fmem_aft_ch0"),
	FMCLK(ABIST, FMEM_AFT_CH1, "fmem_aft_ch1"),
	FMCLK(ABIST, FMEM_BFE_CH0, "fmem_bfe_ch0"),
	FMCLK(ABIST, FMEM_BFE_CH1, "fmem_bfe_ch1"),
	FMCLK(ABIST, FM_466M_FMEM_INFRASYS, "fm_466m_fmem_infrasys"),
	FMCLK(ABIST, FM_MCUSYS_ARM_OUT_ALL, "fm_mcusys_arm_out_all"),
	FMCLK(ABIST, FM_MSDC01_IN_CK, "fm_msdc01_in_ck"),
	FMCLK(ABIST, FM_MSDC02_IN_CK, "fm_msdc02_in_ck"),
	FMCLK(ABIST, FM_MSDC11_IN_CK, "fm_msdc11_in_ck"),
	FMCLK(ABIST, FM_MSDC12_IN_CK, "fm_msdc12_in_ck"),
	FMCLK(ABIST, FM_RTC32K_I, "fm_rtc32k_i"),
	FMCLK(ABIST, FM_CKMON1_CK, "fm_ckmon1_ck"),
	FMCLK(ABIST, FM_CKMON2_CK, "fm_ckmon2_ck"),
	FMCLK(ABIST, FM_CKMON3_CK, "fm_ckmon3_ck"),
	FMCLK(ABIST, FM_CKMON4_CK, "fm_ckmon4_ck"),
	/* ABIST_2 Part */
	FMCLK(ABIST_2, FM_APLL_I2S0_M_CK, "fm_apll_i2s0_m_ck"),
	FMCLK(ABIST_2, FM_APLL_I2S1_M_CK, "fm_apll_i2s1_m_ck"),
	FMCLK(ABIST_2, FM_APLL_I2S2_M_CK, "fm_apll_i2s2_m_ck"),
	FMCLK(ABIST_2, FM_APLL_I2S3_M_CK, "fm_apll_i2s3_m_ck"),
	FMCLK(ABIST_2, FM_APLL_I2S4_M_CK, "fm_apll_i2s4_m_ck"),
	FMCLK(ABIST_2, FM_APLL_I2S4_B_CK, "fm_apll_i2s4_b_ck"),
	FMCLK(ABIST_2, FM_APLL_I2S5_M_CK, "fm_apll_i2s5_m_ck"),
	FMCLK(ABIST_2, FM_APLL_I2S6_M_CK, "fm_apll_i2s6_m_ck"),
	FMCLK(ABIST_2, FM_APLL_I2S7_M_CK, "fm_apll_i2s7_m_ck"),
	FMCLK(ABIST_2, FM_APLL_I2S8_M_CK, "fm_apll_i2s8_m_ck"),
	FMCLK(ABIST_2, FM_APLL_I2S9_M_CK, "fm_apll_i2s9_m_ck"),
	FMCLK2(ABIST_2, FM_MEM_SUB_CK, "fm_mem_sub_ck", 0x0100, 31),
	FMCLK2(ABIST_2, FM_AES_MSDCFDE_CK, "fm_aes_msdcfde_ck", 0x0110, 7),
	FMCLK(ABIST_2, FM_UFS_MBIST_CK, "fm_ufs_mbist_ck"),
	FMCLK(ABIST_2, FM_AP2CONN_HOST_CK, "fm_ap2conn_host_ck"),
	FMCLK(ABIST_2, FM_MSDC_NEW_RX_CK, "fm_msdc_new_rx_ck"),
	FMCLK(ABIST_2, FM_UNIPLL_SES_CK, "fm_unipll_ses_ck"),
	FMCLK(ABIST_2, FM_F_ULPOSC_CK, "fm_f_ulposc_ck"),
	FMCLK(ABIST_2, FM_F_ULPOSC_CORE_CK, "fm_f_ulposc_core_ck"),
	FMCLK(ABIST_2, FM_SRCK_CK, "fm_srck_ck"),
	FMCLK(ABIST_2, FM_SPMI_MST_32K_CK, "fm_spmi_mst_32k_ck"),
	FMCLK(ABIST_2, FM_MAIN_H728M_CK, "fm_main_h728m_ck"),
	FMCLK(ABIST_2, FM_MAIN_H546M_CK, "fm_main_h546m_ck"),
	FMCLK(ABIST_2, FM_MAIN_H436P8M_CK, "fm_main_h436p8m_ck"),
	FMCLK(ABIST_2, FM_MAIN_H364M_CK, "fm_main_h364m_ck"),
	FMCLK(ABIST_2, FM_MAIN_H312M_CK, "fm_main_h312m_ck"),
	FMCLK(ABIST_2, FM_UNIV_1248M_CK, "fm_univ_1248m_ck"),
	FMCLK(ABIST_2, FM_UNIV_832M_CK, "fm_univ_832m_ck"),
	FMCLK(ABIST_2, FM_UNIV_624M_CK, "fm_univ_624m_ck"),
	FMCLK(ABIST_2, FM_UNIV_499M_CK, "fm_univ_499m_ck"),
	FMCLK(ABIST_2, FM_UNIV_416M_CK, "fm_univ_416m_ck"),
	FMCLK(ABIST_2, FM_UNIV_356P6M_CK, "fm_univ_356p6m_ck"),
	FMCLK(ABIST_2, FM_MMPLL_D3_CK_2, "fm_mmpll_d3_ck_2"),
	FMCLK(ABIST_2, FM_MMPLL_D4_CK, "fm_mmpll_d4_ck"),
	FMCLK(ABIST_2, FM_MMPLL_D5_CK, "fm_mmpll_d5_ck"),
	FMCLK(ABIST_2, FM_MMPLL_D6_CK, "fm_mmpll_d6_ck"),
	FMCLK(ABIST_2, FM_MMPLL_D7_CK, "fm_mmpll_d7_ck"),
	FMCLK(ABIST_2, FM_MMPLL_D9_CK, "fm_mmpll_d9_ck"),
	FMCLK(ABIST_2, FM_ROSC_OUT_FREQ, "fm_rosc_out_freq"),
	FMCLK(ABIST_2, FM_ALVTS_TO_PLLGP_MON_L1, "fm_alvts_to_pllgp_mon_l1"),
	FMCLK(ABIST_2, FM_ALVTS_TO_PLLGP_MON_L2, "fm_alvts_to_pllgp_mon_l2"),
	FMCLK(ABIST_2, FM_ALVTS_TO_PLLGP_MON_L3, "fm_alvts_to_pllgp_mon_l3"),
	FMCLK(ABIST_2, FM_ALVTS_TO_PLLGP_MON_L4, "fm_alvts_to_pllgp_mon_l4"),
	FMCLK(ABIST_2, FM_ALVTS_TO_PLLGP_MON_L5, "fm_alvts_to_pllgp_mon_l5"),
	FMCLK(ABIST_2, FM_ALVTS_TO_PLLGP_MON_L6, "fm_alvts_to_pllgp_mon_l6"),
	FMCLK(ABIST_2, FM_ALVTS_TO_PLLGP_MON_L7, "fm_alvts_to_pllgp_mon_l7"),
	FMCLK(ABIST_2, FM_ALVTS_TO_PLLGP_MON_L8, "fm_alvts_to_pllgp_mon_l8"),
	{},
};

#define _CKGEN(x)			(topck_base + (x))
#define _APMIXED(x)			(apmixed_base + (x))
#define CLK_MISC_CFG_0			_CKGEN(0x140)
#define CLK_DBG_CFG			_CKGEN(0x17C)
#define CLK26CALI_0			_CKGEN(0x220)
#define CLK26CALI_1			_CKGEN(0x224)
#define AP_PLLGP1_CON0			_APMIXED(0x200)

static void __iomem *topck_base;
static void __iomem *apmixed_base;
static void __iomem *gpu_pll_ctrl_base;
static void __iomem *apu_pll_ctrl_base;
static void __iomem *spm_base;

const struct fmeter_clk *get_fmeter_clks(void)
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

	if (fclks[i].grp == 1)
		clk_div = (clk_readl(AP_PLLGP1_CON0) >> 24) & 0xF;
	else if (fclks[i].grp == 2)
		clk_div = (clk_readl(AP_PLLGP1_CON0) >> 28) & 0xF;

	return clk_div;
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

unsigned int mt_get_ckgen_freq(unsigned int ID)
{
	int output = 0, i = 0;
	unsigned int temp, clk_dbg_cfg, clk_misc_cfg_0, clk26cali_1 = 0;
	unsigned long flags;

	fmeter_lock(flags);
	if (check_pdn(topck_base, CKGEN, ID)) {
		pr_notice("ID-%d: MUX PDN, return 0.\n", ID);
		fmeter_unlock(flags);
		return 0;
	}

	while (clk_readl(CLK26CALI_0) & 0x1000) {
		udelay(10);
		i++;
		if (i > 30)
			break;
	}

	/* CLK26CALI_0[15]: rst 1 -> 0 */
	clk_writel(CLK26CALI_0, (clk_readl(CLK26CALI_0) & 0xFFFF7FFF));
	/* CLK26CALI_0[15]: rst 0 -> 1 */
	clk_writel(CLK26CALI_0, (clk_readl(CLK26CALI_0) | 0x00008000));

	clk_dbg_cfg = clk_readl(CLK_DBG_CFG);
	clk_writel(CLK_DBG_CFG, (clk_dbg_cfg & 0xFFFFC0FC)|(ID << 8)|(0x1));

	clk_misc_cfg_0 = clk_readl(CLK_MISC_CFG_0);
	clk_writel(CLK_MISC_CFG_0, (clk_misc_cfg_0 & 0x00FFFFFF) | (3 << 24));

	clk26cali_1 = clk_readl(CLK26CALI_1);
	clk_writel(CLK26CALI_0, 0x9000);
	clk_writel(CLK26CALI_0, 0x9010);

	/* wait frequency meter finish */
	while (clk_readl(CLK26CALI_0) & 0x10) {
		udelay(10);
		i++;
		if (i > 30)
			break;
	}
	/* illegal pass */
	if (i == 0) {
		clk_writel(CLK26CALI_0, 0x8000);
		//re-trigger
		clk_writel(CLK26CALI_0, 0x9000);
		clk_writel(CLK26CALI_0, 0x9010);
		while (clk_readl(CLK26CALI_0) & 0x10) {
			udelay(10);
			i++;
			if (i > 30)
				break;
		}
	}

	temp = clk_readl(CLK26CALI_1) & 0xFFFF;

	output = (temp * 26000) / 1024;

	clk_writel(CLK_DBG_CFG, clk_dbg_cfg);
	clk_writel(CLK_MISC_CFG_0, clk_misc_cfg_0);
	/*clk_writel(CLK26CALI_0, clk26cali_0);*/
	/*clk_writel(CLK26CALI_1, clk26cali_1);*/

	clk_writel(CLK26CALI_0, 0x8000);
	fmeter_unlock(flags);
	/*print("ckgen meter[%d] = %d Khz\n", ID, output);*/
	if (i > 30)
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

unsigned int mt_get_abist_freq(unsigned int ID)
{
	int output = 0, i = 0;
	unsigned long flags;
	unsigned int temp, clk_dbg_cfg, clk_misc_cfg_0, clk26cali_1 = 0, clk_div, post_div;

	fmeter_lock(flags);
	while (clk_readl(CLK26CALI_0) & 0x1000) {
		udelay(10);
		i++;
		if (i > 30)
			break;
	}

	/* CLK26CALI_0[15]: rst 1 -> 0 */
	clk_writel(CLK26CALI_0, (clk_readl(CLK26CALI_0) & 0xFFFF7FFF));
	/* CLK26CALI_0[15]: rst 0 -> 1 */
	clk_writel(CLK26CALI_0, (clk_readl(CLK26CALI_0) | 0x00008000));

	clk_dbg_cfg = clk_readl(CLK_DBG_CFG);
	clk_writel(CLK_DBG_CFG, (clk_dbg_cfg & 0xFFC0FFFC)|(ID << 16));

	clk_misc_cfg_0 = clk_readl(CLK_MISC_CFG_0);
	clk_writel(CLK_MISC_CFG_0, (clk_misc_cfg_0 & 0x00FFFFFF) | (3 << 24));

	clk26cali_1 = clk_readl(CLK26CALI_1);

	clk_writel(CLK26CALI_0, 0x9000);
	clk_writel(CLK26CALI_0, 0x9010);

	/* wait frequency meter finish */
	while (clk_readl(CLK26CALI_0) & 0x10) {
		udelay(10);
		i++;
		if (i > 30)
			break;
	}
	/* illegal pass */
	if (i == 0) {
		clk_writel(CLK26CALI_0, 0x8000);
		//re-trigger
		clk_writel(CLK26CALI_0, 0x9000);
		clk_writel(CLK26CALI_0, 0x9010);
		while (clk_readl(CLK26CALI_0) & 0x10) {
			udelay(10);
			i++;
			if (i > 30)
				break;
		}
	}
	temp = clk_readl(CLK26CALI_1) & 0xFFFF;

	clk_div = get_clk_div(ABIST, ID);
	post_div = get_post_div(ABIST, ID);

	output = (temp * 26000) / 1024 * clk_div / post_div;

	clk_writel(CLK_DBG_CFG, clk_dbg_cfg);
	clk_writel(CLK_MISC_CFG_0, clk_misc_cfg_0);
	/*clk_writel(CLK26CALI_0, clk26cali_0);*/
	/*clk_writel(CLK26CALI_1, clk26cali_1);*/
	clk_writel(CLK26CALI_0, 0x8000);
	fmeter_unlock(flags);

	if (i > 30)
		return 0;
	if ((output * 4) < 1000) {
		pr_notice("%s: CLK_DBG_CFG = 0x%x, CLK_MISC_CFG_0 = 0x%x, CLK26CALI_0 = 0x%x, CLK26CALI_1 = 0x%x\n",
			__func__,
			clk_readl(CLK_DBG_CFG),
			clk_readl(CLK_MISC_CFG_0),
			clk_readl(CLK26CALI_0),
			clk_readl(CLK26CALI_1));
		pr_notice("clk_div = 0x%x, post_div = 0x%x\n", clk_div, post_div);
	}
	return (output * 4);
}

unsigned int mt_get_abist2_freq(unsigned int ID)
{
	int output = 0, i = 0;
	unsigned long flags;
	unsigned int temp, clk_dbg_cfg, clk_misc_cfg_0, clk26cali_1 = 0;

	fmeter_lock(flags);
	if (check_pdn(topck_base, ABIST_2, ID)) {
		pr_notice("ID-%d: MUX PDN, return 0.\n", ID);
		fmeter_unlock(flags);
		return 0;
	}

	while (clk_readl(CLK26CALI_0) & 0x1000) {
		udelay(10);
		i++;
		if (i > 30)
			break;
	}

	/* CLK26CALI_0[15]: rst 1 -> 0 */
	clk_writel(CLK26CALI_0, (clk_readl(CLK26CALI_0) & 0xFFFF7FFF));
	/* CLK26CALI_0[15]: rst 0 -> 1 */
	clk_writel(CLK26CALI_0, (clk_readl(CLK26CALI_0) | 0x00008000));

	clk_dbg_cfg = clk_readl(CLK_DBG_CFG);
	clk_writel(CLK_DBG_CFG, (clk_dbg_cfg & 0xC0FFFFFC)
			| (ID << 24) | (0x2));

	clk_misc_cfg_0 = clk_readl(CLK_MISC_CFG_0);
	clk_writel(CLK_MISC_CFG_0, (clk_misc_cfg_0 & 0x00FFFFFF) | (1 << 24));

	clk26cali_1 = clk_readl(CLK26CALI_1);

	clk_writel(CLK26CALI_0, 0x9000);
	clk_writel(CLK26CALI_0, 0x9010);

	/* wait frequency meter finish */
	while (clk_readl(CLK26CALI_0) & 0x10) {
		udelay(10);
		i++;
		if (i > 30)
			break;
	}
	/* illegal pass */
	if (i == 0) {
		clk_writel(CLK26CALI_0, 0x8000);
		//re-trigger
		clk_writel(CLK26CALI_0, 0x9000);
		clk_writel(CLK26CALI_0, 0x9010);
		while (clk_readl(CLK26CALI_0) & 0x10) {
			udelay(10);
			i++;
			if (i > 30)
				break;
		}
	}
	temp = clk_readl(CLK26CALI_1) & 0xFFFF;

	output = (temp * 26000) / 1024;

	clk_writel(CLK_DBG_CFG, clk_dbg_cfg);
	clk_writel(CLK_MISC_CFG_0, clk_misc_cfg_0);
	clk_writel(CLK26CALI_0, 0x8000);
	/*pr_debug("%s = %d Khz\n", abist_array[ID-1], output);*/
	fmeter_unlock(flags);

	if (i > 30)
		return 0;
	else
		return (output * 2);
}

unsigned int mt_get_subsys_freq(unsigned int ID)
{
	int output = 0, i = 0;
	unsigned int temp, pll4h_fqmtr_con0, pll4h_fqmtr_con1;
	unsigned long flags;
	void __iomem *base, *con0, *con1;
	unsigned int id;
	unsigned int subsys_idx;

	fmeter_lock(flags);
	if (check_pdn(spm_base, SUBSYS, ID)) {
		pr_notice("ID-%d: PDN, return 0.\n", ID);
		fmeter_unlock(flags);
		return 0;
	}

	id = FM_ID(ID);
	subsys_idx = FM_SYS(ID) * 4 + id;
	if (subsys_idx >= (SUBSYS_PLL_NUM * FM_SYS_NUM) || subsys_idx < 0) {
		fmeter_unlock(flags);
		return 0;
	}

	base = fm_sub_clks[subsys_idx].base;
	con0 = base + fm_sub_clks[subsys_idx].con0;
	con1 = base + fm_sub_clks[subsys_idx].con1;

	/* PLL4H_FQMTR_CON1[15]: rst 1 -> 0 */
	clk_writel(con0, clk_readl(con0) & 0xFFFF7FFF);
	/* PLL4H_FQMTR_CON1[15]: rst 0 -> 1 */
	clk_writel(con0, clk_readl(con0) | 0x8000);

	/* sel fqmtr_cksel */
	clk_writel(con0, (clk_readl(con0) & 0x00FFFFF8) | (id << 0));
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
				ID, clk_readl(con0), clk_readl(con1));
			break;
		}
	}

	temp = clk_readl(con1) & 0xFFFF;
	output = ((temp * 26000)) / 1024; // Khz

	clk_writel(con0, 0x8000);
	pr_notice("[%d(%d)]con0: 0x%x, con1: 0x%x\n",
				ID, id, clk_readl(con0), clk_readl(con1));
	fmeter_unlock(flags);

	return output * 4;
}

int mt_subsys_freq_register(struct fm_subsys *fm, unsigned int size)
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

static int __init clk_fmeter_mt6877_init(void)
{
	struct device_node *node;
	int i;

	node = of_find_compatible_node(NULL, NULL,
		"mediatek,mt6877-topckgen");
	if (node) {
		topck_base = of_iomap(node, 0);
		if (!topck_base) {
			pr_err("%s() can't find iomem for topckgen\n",
					__func__);
			return -1;
		}
	} else {
		goto ERR;
	}

	node = of_find_compatible_node(NULL, NULL,
		"mediatek,mt6877-apmixedsys");
	if (node) {
		apmixed_base = of_iomap(node, 0);
		if (!apmixed_base) {
			pr_err("%s() can't find iomem for apmixedsys\n",
					__func__);
			return -1;
		}
	} else
		goto ERR;

	node = of_find_compatible_node(NULL, NULL,
		"mediatek,mt6877-scpsys");
	if (node) {
		spm_base = of_iomap(node, 0);
		if (!spm_base) {
			pr_err("%s() can't find iomem for spm\n",
					__func__);
			return -1;
		}
	} else
		goto ERR;

	fm_all_clks = kcalloc(FM_SYS_NUM * 4 + ARRAY_SIZE(fclks),
			sizeof(*fm_all_clks), GFP_KERNEL);
	if (!fm_all_clks)
		return -ENOMEM;

	for (i = 0; i < ARRAY_SIZE(fclks) - 1; i++) {
		fm_all_clks[i] = fclks[i];
		fm_clk_cnt++;
	}

	return 0;

ERR:
	pr_err("%s can't find compatible node for apmixedsys\n",
				__func__);
		return -1;
}
subsys_initcall(clk_fmeter_mt6877_init);

