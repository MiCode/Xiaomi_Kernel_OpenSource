/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/delay.h>
#include <linux/of_address.h>

#include "clkchk.h"
#include "clk-fmeter.h"
#include "clkdbg.h"

static DEFINE_SPINLOCK(meter_lock);
#define fmeter_lock(flags)   spin_lock_irqsave(&meter_lock, flags)
#define fmeter_unlock(flags) spin_unlock_irqrestore(&meter_lock, flags)

/*
 * clkdbg fmeter
 */

#define clk_readl(addr)		readl(addr)
#define clk_writel(addr, val)	\
	do { writel(val, addr); wmb(); } while (0) /* sync write */

#define FMCLK2(_t, _i, _n, _o, _p) { .type = _t, \
		.id = _i, .name = _n, .ofs = _o, .pdn = _p}
#define FMCLK(_t, _i, _n) { .type = _t, .id = _i, .name = _n}

static const struct fmeter_clk fclks[] = {
	/* CKGEN Part */
	FMCLK2(CKGEN, FM_AXI_CK, "fm_axi_ck", 0x10, 7),
	FMCLK2(CKGEN, FM_SPM_CK, "fm_spm_ck", 0x10, 15),
	FMCLK2(CKGEN, FM_SCP_CK, "fm_scp_ck", 0x10, 23),
	FMCLK2(CKGEN, FM_BUS_CK, "fm_bus_ck", 0x10, 31),
	FMCLK2(CKGEN, FM_DISP_CK, "fm_disp_ck", 0x20, 7),
	FMCLK2(CKGEN, FM_MDP_CK, "fm_mdp_ck", 0x20, 15),
	FMCLK2(CKGEN, FM_IMG1_CK, "fm_img1_ck", 0x20, 23),
	FMCLK2(CKGEN, FM_IMG2_CK, "fm_img2_ck", 0x20, 31),
	FMCLK2(CKGEN, FM_IPE_CK, "fm_ipe_ck", 0x30, 7),
	FMCLK2(CKGEN, FM_DPE_CK, "fm_dpe_ck", 0x30, 15),
	FMCLK2(CKGEN, FM_CAM_CK, "fm_cam_ck", 0x30, 23),
	FMCLK2(CKGEN, FM_CCU_CK, "fm_ccu_ck", 0x30, 31),
	FMCLK2(CKGEN, FM_DSP_CK, "fm_dsp_ck", 0x40, 7),
	FMCLK2(CKGEN, FM_DSP1_CK, "fm_dsp1_ck", 0x40, 15),
	FMCLK2(CKGEN, FM_DSP2_CK, "fm_dsp2_ck", 0x40, 23),
	FMCLK2(CKGEN, FM_IPU_IF_CK, "fm_ipu_if_ck", 0x50, 15),
	FMCLK2(CKGEN, FM_MFG_CK, "fm_mfg_ck", 0x50, 23),
	FMCLK2(CKGEN, FM_FCAMTG_CK, "fm_fcamtg_ck", 0x50, 31),
	FMCLK2(CKGEN, FM_FCAMTG2_CK, "fm_fcamtg2_ck", 0x60, 7),
	FMCLK2(CKGEN, FM_FCAMTG3_CK, "fm_fcamtg3_ck", 0x60, 15),
	FMCLK2(CKGEN, FM_FCAMTG4_CK, "fm_fcamtg4_ck", 0x60, 23),
	FMCLK2(CKGEN, FM_FUART_CK, "fm_fuart_ck", 0x60, 15),
	FMCLK2(CKGEN, FM_SPI_CK, "fm_spi_ck", 0x60, 23),
	FMCLK2(CKGEN, FM_MSDC50_0_H_CK, "fm_msdc50_0_h_ck", 0x60, 31),
	FMCLK2(CKGEN, FM_MSDC50_0_CK, "fm_msdc50_0_ck", 0x80, 7),
	FMCLK2(CKGEN, FM_MSDC30_1_CK, "fm_msdc30_1_ck", 0x80, 15),
	FMCLK2(CKGEN, FM_AUDIO_CK, "fm_audio_ck", 0x80, 31),
	FMCLK2(CKGEN, FM_AUD_INTBUS_CK, "fm_aud_intbus_ck", 0x90, 7),
	FMCLK2(CKGEN, FM_FPWRAP_ULPOSC_CK, "fm_fpwrap_ulposc_ck", 0x90, 15),
	FMCLK2(CKGEN, FM_ATB_CK, "fm_atb_ck", 0x90, 23),
	FMCLK2(CKGEN, FM_SSPM_CK, "fm_sspm_ck", 0x90, 31),
	FMCLK2(CKGEN, FM_DPI_CK, "fm_dpi_ck", 0xA0, 7),
	FMCLK2(CKGEN, FM_SCAM_CK, "fm_scam_ck", 0xA0, 15),
	FMCLK2(CKGEN, FM_FDISP_PWM_CK, "fm_fdisp_pwm_ck", 0xA0, 23),
	FMCLK2(CKGEN, FM_FUSB_CK, "fm_fusb_ck", 0xA0, 31),
	FMCLK2(CKGEN, FM_FSSUSB_XHCI_CK, "fm_fssusb_xhci_ck", 0xB0, 7),
	FMCLK2(CKGEN, FM_I2C_CK, "fm_i2c_ck", 0xB0, 15),
	FMCLK2(CKGEN, FM_FSENINF_CK, "fm_fseninf_ck", 0xB0, 23),
	FMCLK2(CKGEN, FM_FSENINF1_CK, "fm_fseninf1_ck", 0xB0, 31),
	FMCLK2(CKGEN, FM_FSENINF2_CK, "fm_fseninf2_ck", 0xC0, 7),
	FMCLK2(CKGEN, FM_DXCC_CK, "fm_dxcc_ck", 0xC0, 31),
	FMCLK2(CKGEN, FM_AUD_ENGEN1_CK, "fm_aud_engen1_ck", 0xD0, 7),
	FMCLK2(CKGEN, FM_AUD_ENGEN2_CK, "fm_aud_engen2_ck", 0xD0, 15),
	FMCLK2(CKGEN, FM_AES_UFSFDE_CK, "fm_aes_ufsfde_ck", 0xD0, 23),
	FMCLK2(CKGEN, FM_UFS_CK, "fm_ufs_ck", 0xD0, 31),
	FMCLK2(CKGEN, FM_AUD_1_CK, "fm_aud_1_ck", 0xE0, 7),
	FMCLK2(CKGEN, FM_AUD_2_CK, "fm_aud_2_ck", 0xE0, 15),
	FMCLK2(CKGEN, FM_ADSP_CK, "fm_adsp_ck", 0xE0, 23),
	FMCLK2(CKGEN, FM_DPMAIF_MAIN_CK, "fm_dpmaif_main_ck", 0xE0, 31),
	FMCLK2(CKGEN, FM_VENC_CK, "fm_venc_ck", 0xF0, 7),
	FMCLK2(CKGEN, FM_VDEC_CK, "fm_vdec_ck", 0xF0, 15),
	FMCLK2(CKGEN, FM_CAMTM_CK, "fm_camtm_ck", 0xF0, 23),
	FMCLK2(CKGEN, FM_PWM_CK, "fm_pwm_ck", 0xF0, 31),
	FMCLK2(CKGEN, FM_AUDIO_H_CK, "fm_audio_h_ck", 0x100, 7),
	FMCLK2(CKGEN, FM_SPMI_MST_CK, "fm_spmi_mst_ck", 0x100, 15),
	FMCLK2(CKGEN, FM_DVFSRC_CK, "fm_dvfsrc_ck", 0x100, 23),
	/* ABIST Part */
	FMCLK(ABIST, FM_ADSPPLL_CK, "fm_adsppll_ck"),
	FMCLK(ABIST, FM_APLL1_CK, "fm_apll1_ck"),
	FMCLK(ABIST, FM_APLL2_CK, "fm_apll2_ck"),
	FMCLK(ABIST, FM_APPLLGP_MON_FM_CK, "fm_appllgp_mon_fm_ck"),
	FMCLK(ABIST, FM_ARMPLL_BL_CK, "fm_armpll_bl_ck"),
	FMCLK(ABIST, FM_NPUPLL_CK, "fm_npupll_ck"),
	FMCLK(ABIST, FM_USBPLL_CK, "fm_usbpll_ck"),
	FMCLK(ABIST, FM_ARMPLL_LL_CK, "fm_armpll_ll_ck"),
	FMCLK(ABIST, FM_CCIPLL_CK, "fm_ccipll_ck"),
	FMCLK(ABIST, FM_CSI0A_CDPHY_DELAYCAL_CK, "fm_csi0a_cdphy_delaycal_ck"),
	FMCLK(ABIST, FM_CSI0B_CDPHY_DELAYCAL_CK, "fm_csi0b_cdphy_delaycal_ck"),
	FMCLK(ABIST, FM_CSI1A_DPHY_DELAYCAL_CK, "fm_csi1a_dphy_delaycal_ck"),
	FMCLK(ABIST, FM_CSI1B_DPHY_DELAYCAL_CK, "fm_csi1b_dphy_delaycal_ck"),
	FMCLK(ABIST, FM_CSI2A_DPHY_DELAYCAL_CK, "fm_csi2a_dphy_delaycal_ck"),
	FMCLK(ABIST, FM_CSI2B_DPHY_DELAYCAL_CK, "fm_csi2b_dphy_delaycal_ck"),
	FMCLK(ABIST, FM_DSI0_LNTC_DSICLK, "fm_dsi0_lntc_dsiclk"),
	FMCLK(ABIST, FM_DSI0_MPPLL_TST_CK, "fm_dsi0_mppll_tst_ck"),
	FMCLK(ABIST, FM_MFGPLL_CK, "fm_mfgpll_ck"),
	FMCLK(ABIST, FM_MAINPLL_CK, "fm_mainpll_ck"),
	FMCLK(ABIST, FM_MDPLL_FS26M_CK, "fm_mdpll_fs26m_ck"),
	FMCLK(ABIST, FM_MGPLL_CK, "fm_mgpll_ck"),
	FMCLK(ABIST, FM_MMPLL_CK, "fm_mmpll_ck"),
	FMCLK(ABIST, FM_MMPLL_D3_CK, "fm_mmpll_d3_ck"),
	FMCLK(ABIST, FM_MPLL_CK, "fm_mpll_ck"),
	FMCLK(ABIST, FM_MSDCPLL_CK, "fm_msdcpll_ck"),
	FMCLK(ABIST, FM_RCLRPLL_DIV4_CH2, "fm_rclrpll_div4_ch2"),
	FMCLK(ABIST, FM_RPHYPLL_DIV4_CH2, "fm_rphypll_div4_ch2"),
	FMCLK(ABIST, FM_TVDPLL_CK, "fm_tvdpll_ck"),
	FMCLK(ABIST, FM_ULPOSC2_CK, "fm_ulposc2_ck"),
	FMCLK(ABIST, FM_ULPOSC_CK, "fm_ulposc_ck"),
	FMCLK(ABIST, FM_UNIVPLL_CK, "fm_univpll_ck"),
	FMCLK(ABIST, FM_USB20_192M_CK, "fm_usb20_192m_ck"),
	FMCLK(ABIST, FM_USBPLL_192M_CK, "fm_usbpll_192m_ck"),
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
	FMCLK(ABIST, FM_MSDC21_IN_CK, "fm_msdc21_in_ck"),
	FMCLK(ABIST, FM_MSDC22_IN_CK, "fm_msdc22_in_ck"),
	FMCLK(ABIST, FM_RTC32K_I, "fm_rtc32k_i"),
	FMCLK(ABIST, FM_CKMON1_CK, "fm_ckmon1_ck"),
	FMCLK(ABIST, FM_CKMON2_CK, "fm_ckmon2_ck"),
	FMCLK(ABIST, FM_CKMON3_CK, "fm_ckmon3_ck"),
	FMCLK(ABIST, FM_CKMON4_CK, "fm_ckmon4_ck"),
	/* ABIST_2 Part */
	FMCLK(ABIST_2, FM_AUD_I2S0_M_CK, "fm_aud_i2s0_m_ck"),
	FMCLK(ABIST_2, FM_AUD_I2S1_M_CK, "fm_aud_i2s1_m_ck"),
	FMCLK(ABIST_2, FM_AUD_I2S2_M_CK, "fm_aud_i2s2_m_ck"),
	FMCLK(ABIST_2, FM_AUD_I2S3_M_CK, "fm_aud_i2s3_m_ck"),
	FMCLK(ABIST_2, FM_AUD_I2S4_M_CK, "fm_aud_i2s4_m_ck"),
	FMCLK(ABIST_2, FM_AUD_I2S4_B_CK, "fm_aud_i2s4_b_ck"),
	FMCLK(ABIST_2, FM_AUD_I2S5_M_CK, "fm_aud_i2s5_m_ck"),
	FMCLK(ABIST_2, FM_AUD_I2S6_M_CK, "fm_aud_i2s6_m_ck"),
	FMCLK(ABIST_2, FM_AUD_I2S7_M_CK, "fm_aud_i2s7_m_ck"),
	FMCLK(ABIST_2, FM_AUD_I2S8_M_CK, "fm_aud_i2s8_m_ck"),
	FMCLK(ABIST_2, FM_AUD_I2S9_M_CK, "fm_aud_i2s9_m_ck"),
	FMCLK(ABIST_2, FM_UNIPLL_SES_CK, "fm_unipll_ses_ck"),
	FMCLK(ABIST_2, FM_F_ULPOSC_CK, "fm_f_ulposc_ck"),
	FMCLK(ABIST_2, FM_F_ULPOSC_CORE_CK, "fm_f_ulposc_core_ck"),
	FMCLK(ABIST_2, FM_SRCK_CK, "fm_srck_ck"),
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
	{},
};

#define _CKGEN(x)		(topck_base + (x))
#define CLK_MISC_CFG_0		_CKGEN(0x140)
#define CLK_DBG_CFG		_CKGEN(0x17C)
#define CLK26CALI_0		_CKGEN(0x220)
#define CLK26CALI_1		_CKGEN(0x224)

static void __iomem *topck_base;

const struct fmeter_clk *get_fmeter_clks(void)
{
	return fclks;
}

static unsigned int check_mux_pdn(unsigned int ID)
{
	int i;

	if ((ID > 0) && (ID < 64)) {
		for (i = 0; i < ARRAY_SIZE(fclks) - 1; i++)
			if (fclks[i].id == ID)
				break;
		if (i >= ARRAY_SIZE(fclks) - 1)
			return 1;
		if ((clk_readl(topck_base + fclks[i].ofs)
				& BIT(fclks[i].pdn)))
			return 1;
		else
			return 0;
	} else
		return 1;
}

unsigned int mt_get_ckgen_freq(unsigned int ID)
{
	int output = 0, i = 0;
	unsigned int temp, clk_dbg_cfg, clk_misc_cfg_0, clk26cali_1 = 0;
	unsigned long flags;

	if (check_mux_pdn(ID)) {
		pr_notice("ID-%d: MUX PDN, return 0.\n", ID);
		return 0;
	}

	fmeter_lock(flags);
	while (clk_readl(CLK26CALI_0) & 0x1000) {
		udelay(10);
		i++;
		if (i > 30)
			break;
	}

	clk_dbg_cfg = clk_readl(CLK_DBG_CFG);
	clk_writel(CLK_DBG_CFG, (clk_dbg_cfg & 0xFFFFC0FC)|(ID << 8)|(0x1));

	clk_misc_cfg_0 = clk_readl(CLK_MISC_CFG_0);
	clk_writel(CLK_MISC_CFG_0, (clk_misc_cfg_0 & 0x00FFFFFF) | (3 << 24));

	clk26cali_1 = clk_readl(CLK26CALI_1);
	clk_writel(CLK26CALI_0, 0x1000);
	clk_writel(CLK26CALI_0, 0x1010);

	/* wait frequency meter finish */
	while (clk_readl(CLK26CALI_0) & 0x10) {
		udelay(10);
		i++;
		if (i > 30)
			break;
	}
	/* illegal pass */
	if (i == 0) {
		clk_writel(CLK26CALI_0, 0x0000);
		//re-trigger
		clk_writel(CLK26CALI_0, 0x1000);
		clk_writel(CLK26CALI_0, 0x1010);
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

	clk_writel(CLK26CALI_0, 0x0000);
	fmeter_unlock(flags);
	/*print("ckgen meter[%d] = %d Khz\n", ID, output);*/
	if (i > 30)
		return 0;
	if ((output * 4) < 25000) {
		pr_notice("%s: CLK_DBG_CFG = 0x%x, CLK_MISC_CFG_0 = 0x%x, CLK26CALI_0 = 0x%x, CLK26CALI_1 = 0x%x\n",
			__func__,
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
	unsigned int temp, clk_dbg_cfg, clk_misc_cfg_0, clk26cali_1 = 0;

	fmeter_lock(flags);
	while (clk_readl(CLK26CALI_0) & 0x1000) {
		udelay(10);
		i++;
		if (i > 30)
			break;
	}

	clk_dbg_cfg = clk_readl(CLK_DBG_CFG);
	clk_writel(CLK_DBG_CFG, (clk_dbg_cfg & 0xFFC0FFFC)|(ID << 16));

	clk_misc_cfg_0 = clk_readl(CLK_MISC_CFG_0);
	clk_writel(CLK_MISC_CFG_0, (clk_misc_cfg_0 & 0x00FFFFFF) | (3 << 24));

	clk26cali_1 = clk_readl(CLK26CALI_1);

	clk_writel(CLK26CALI_0, 0x1000);
	clk_writel(CLK26CALI_0, 0x1010);

	/* wait frequency meter finish */
	while (clk_readl(CLK26CALI_0) & 0x10) {
		udelay(10);
		i++;
		if (i > 30)
			break;
	}
	/* illegal pass */
	if (i == 0) {
		clk_writel(CLK26CALI_0, 0x0000);
		//re-trigger
		clk_writel(CLK26CALI_0, 0x1000);
		clk_writel(CLK26CALI_0, 0x1010);
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
	clk_writel(CLK26CALI_0, 0x0000);
	fmeter_unlock(flags);

	if (i > 30)
		return 0;
	if ((output * 4) < 25000) {
		pr_notice("%s: CLK_DBG_CFG = 0x%x, CLK_MISC_CFG_0 = 0x%x, CLK26CALI_0 = 0x%x, CLK26CALI_1 = 0x%x\n",
			__func__,
			clk_readl(CLK_DBG_CFG),
			clk_readl(CLK_MISC_CFG_0),
			clk_readl(CLK26CALI_0),
			clk_readl(CLK26CALI_1));
	}
	return (output * 4);
}

unsigned int mt_get_abist2_freq(unsigned int ID)
{
	int output = 0, i = 0;
	unsigned long flags;
	unsigned int temp, clk_dbg_cfg, clk_misc_cfg_0, clk26cali_1 = 0;

	fmeter_lock(flags);
	while (clk_readl(CLK26CALI_0) & 0x1000) {
		udelay(10);
		i++;
		if (i > 30)
			break;
	}

	clk_dbg_cfg = clk_readl(CLK_DBG_CFG);
	clk_writel(CLK_DBG_CFG, (clk_dbg_cfg & 0xC0FFFFFC)
			| (ID << 24) | (0x2));

	clk_misc_cfg_0 = clk_readl(CLK_MISC_CFG_0);
	clk_writel(CLK_MISC_CFG_0, (clk_misc_cfg_0 & 0x00FFFFFF) | (1 << 24));

	clk26cali_1 = clk_readl(CLK26CALI_1);

	clk_writel(CLK26CALI_0, 0x1000);
	clk_writel(CLK26CALI_0, 0x1010);

	/* wait frequency meter finish */
	while (clk_readl(CLK26CALI_0) & 0x10) {
		udelay(10);
		i++;
		if (i > 30)
			break;
	}
	/* illegal pass */
	if (i == 0) {
		clk_writel(CLK26CALI_0, 0x0000);
		//re-trigger
		clk_writel(CLK26CALI_0, 0x1000);
		clk_writel(CLK26CALI_0, 0x1010);
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
	clk_writel(CLK26CALI_0, 0x0000);
	/*pr_debug("%s = %d Khz\n", abist_array[ID-1], output);*/
	fmeter_unlock(flags);

	if (i > 30)
		return 0;
	else
		return (output * 2);
}

static int __init clk_fmeter_mt6833_init(void)
{
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL,
		"mediatek,topckgen");
	if (node) {
		topck_base = of_iomap(node, 0);
		if (!topck_base) {
			pr_notice("%s() can't find iomem for topckgen\n",
					__func__);
			return -1;
		}
	} else {
		pr_notice("%s can't find compatible node for topckgen\n",
				__func__);
		return -1;
	}

	return 0;
}
subsys_initcall(clk_fmeter_mt6833_init);
