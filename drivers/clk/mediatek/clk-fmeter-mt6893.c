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
#include "clk-mt6893-fmeter.h"

#define FM_TIMEOUT		30

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
	FMCLK2(CKGEN, FM_AXI_CK, "fm_axi_ck", 0x0010, 7),
	FMCLK2(CKGEN, FM_SPM_CK, "fm_spm_ck", 0x0010, 15),
	FMCLK2(CKGEN, FM_SCP_CK, "fm_scp_ck", 0x0010, 23),
	FMCLK2(CKGEN, FM_BUS_CK, "fm_bus_ck", 0x0010, 31),
	FMCLK2(CKGEN, FM_DISP_CK, "fm_disp_ck", 0x0020, 7),
	FMCLK2(CKGEN, FM_MDP_CK, "fm_mdp_ck", 0x0020, 15),
	FMCLK2(CKGEN, FM_IMG1_CK, "fm_img1_ck", 0x0020, 23),
	FMCLK2(CKGEN, FM_IMG2_CK, "fm_img2_ck", 0x0020, 31),
	FMCLK2(CKGEN, FM_IPE_CK, "fm_ipe_ck", 0x0030, 7),
	FMCLK2(CKGEN, FM_DPE_CK, "fm_dpe_ck", 0x0030, 15),
	FMCLK2(CKGEN, FM_MM_CK, "fm_mm_ck", 0x0030, 23),
	FMCLK2(CKGEN, FM_CCU_CK, "fm_ccu_ck", 0x0030, 31),
	FMCLK2(CKGEN, FM_DSP_CK, "fm_dsp_ck", 0x0040, 7),
	FMCLK2(CKGEN, FM_DSP1_CK, "fm_dsp1_ck", 0x0040, 15),
	FMCLK2(CKGEN, FM_DSP2_CK, "fm_dsp2_ck", 0x0040, 23),
	FMCLK2(CKGEN, FM_DSP3_CK, "fm_dsp3_ck", 0x0040, 31),
	FMCLK2(CKGEN, FM_DSP4_CK, "fm_dsp4_ck", 0x0050, 7),
	FMCLK2(CKGEN, FM_DSP5_CK, "fm_dsp5_ck", 0x0050, 15),
	FMCLK2(CKGEN, FM_DSP6_CK, "fm_dsp6_ck", 0x0050, 23),
	FMCLK2(CKGEN, FM_DSP7_CK, "fm_dsp7_ck", 0x0050, 31),
	FMCLK2(CKGEN, FM_IPU_IF_CK, "fm_ipu_if_ck", 0x0060, 7),
	FMCLK2(CKGEN, FM_MFG_REF_CK, "fm_mfg_ref_ck", 0x0060, 15),
	FMCLK2(CKGEN, FM_FCAMTG_CK, "fm_fcamtg_ck", 0x0060, 23),
	FMCLK2(CKGEN, FM_FCAMTG2_CK, "fm_fcamtg2_ck", 0x0060, 31),
	FMCLK2(CKGEN, FM_FCAMTG3_CK, "fm_fcamtg3_ck", 0x0070, 7),
	FMCLK2(CKGEN, FM_FCAMTG4_CK, "fm_fcamtg4_ck", 0x0070, 15),
	FMCLK2(CKGEN, FM_FUART_CK, "fm_fuart_ck", 0x0070, 23),
	FMCLK2(CKGEN, FM_SPI_CK, "fm_spi_ck", 0x0070, 31),
	FMCLK2(CKGEN, FM_MSDC50_0_H_CK, "fm_msdc50_0_h_ck", 0x0080, 7),
	FMCLK2(CKGEN, FM_MSDC50_0_CK, "fm_msdc50_0_ck", 0x0080, 15),
	FMCLK2(CKGEN, FM_MSDC30_1_CK, "fm_msdc30_1_ck", 0x0080, 23),
	FMCLK2(CKGEN, FM_AUDIO_CK, "fm_audio_ck", 0x0080, 31),
	FMCLK2(CKGEN, FM_AUD_INTBUS_CK, "fm_aud_intbus_ck", 0x0090, 7),
	FMCLK2(CKGEN, FM_FPWRAP_ULPOSC_CK, "fm_fpwrap_ulposc_ck", 0x0090, 15),
	FMCLK2(CKGEN, FM_ATB_CK, "fm_atb_ck", 0x0090, 23),
	FMCLK2(CKGEN, FM_SSPM_CK, "fm_sspm_ck", 0x0090, 31),
	FMCLK2(CKGEN, FM_DP_CK, "fm_dp_ck", 0x00A0, 7),
	FMCLK2(CKGEN, FM_SCAM_CK, "fm_scam_ck", 0x00A0, 15),
	FMCLK2(CKGEN, FM_FDISP_PWM_CK, "fm_fdisp_pwm_ck", 0x00A0, 23),
	FMCLK2(CKGEN, FM_FUSB_CK, "fm_fusb_ck", 0x00A0, 31),
	FMCLK2(CKGEN, FM_FSSUSB_XHCI_CK, "fm_fssusb_xhci_ck", 0x00B0, 7),
	FMCLK2(CKGEN, FM_I2C_CK, "fm_i2c_ck", 0x00B0, 15),
	FMCLK2(CKGEN, FM_FSENINF_CK, "fm_fseninf_ck", 0x00B0, 23),
	FMCLK2(CKGEN, FM_MCUPM_CK, "fm_mcupm_ck", 0x0100, 31),
	FMCLK2(CKGEN, FM_SPMI_MST_CK, "fm_spmi_mst_ck", 0x0110, 7),
	FMCLK2(CKGEN, FM_DVFSRC_CK, "fm_dvfsrc_ck", 0x0110, 15),
	FMCLK2(CKGEN, FM_DXCC_CK, "fm_dxcc_ck", 0x00C0, 23),
	FMCLK2(CKGEN, FM_AUD_ENGEN1_CK, "fm_aud_engen1_ck", 0x00C0, 31),
	FMCLK2(CKGEN, FM_AUD_ENGEN2_CK, "fm_aud_engen2_ck", 0x00D0, 7),
	FMCLK2(CKGEN, FM_AES_UFSFDE_CK, "fm_aes_ufsfde_ck", 0x00D0, 15),
	FMCLK2(CKGEN, FM_UFS_CK, "fm_ufs_ck", 0x00D0, 23),
	FMCLK2(CKGEN, FM_AUD_1_CK, "fm_aud_1_ck", 0x00D0, 31),
	FMCLK2(CKGEN, FM_AUD_2_CK, "fm_aud_2_ck", 0x00E0, 7),
	FMCLK2(CKGEN, FM_ADSP_CK, "fm_adsp_ck", 0x00E0, 15),
	FMCLK2(CKGEN, FM_DPMAIF_MAIN_CK, "fm_dpmaif_main_ck", 0x00E0, 23),
	FMCLK2(CKGEN, FM_VENC_CK, "fm_venc_ck", 0x00E0, 31),
	FMCLK2(CKGEN, FM_VDEC_CK, "fm_vdec_ck", 0x00F0, 7),
	FMCLK2(CKGEN, FM_VDEC_LAT_CK, "fm_vdec_lat_ck", 0x00F0, 15),
	FMCLK2(CKGEN, FM_CAMTM_CK, "fm_camtm_ck", 0x00F0, 23),
	FMCLK2(CKGEN, FM_PWM_CK, "fm_pwm_ck", 0x00F0, 31),
	FMCLK2(CKGEN, FM_AUDIO_H_CK, "fm_audio_h_ck", 0x0100, 7),
	FMCLK2(CKGEN, FM_FCAMTG5_CK, "fm_fcamtg5_ck", 0x0100, 15),
	FMCLK2(CKGEN, FM_FCAMTG6_CK, "fm_fcamtg6_ck", 0x0100, 23),
	/* ABIST Part */
	FMCLK(ABIST, FM_ADSPPLL_CK, "fm_adsppll_ck"),
	FMCLK(ABIST, FM_APLL1_CK, "fm_apll1_ck"),
	FMCLK(ABIST, FM_APLL2_CK, "fm_apll2_ck"),
	FMCLK(ABIST, FM_APPLLGP_MON_FM_CK, "fm_appllgp_mon_fm_ck"),
	FMCLK(ABIST, FM_APUPLL_D2_CK, "fm_apupll_d2_ck"),
	FMCLK(ABIST, FM_ARMPLL_BL0_CK, "fm_armpll_bl0_ck"),
	FMCLK(ABIST, FM_ARMPLL_BL1_CK, "fm_armpll_bl1_ck"),
	FMCLK(ABIST, FM_ARMPLL_BL2_CK, "fm_armpll_bl2_ck"),
	FMCLK(ABIST, FM_ARMPLL_BL3_CK, "fm_armpll_bl3_ck"),
	FMCLK(ABIST, FM_ARMPLL_LL_CK, "fm_armpll_ll_ck"),
	FMCLK(ABIST, FM_CCIPLL_CK, "fm_ccipll_ck"),
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
	FMCLK(ABIST, FM_DSI1_LNTC_DSICLK, "fm_dsi1_lntc_dsiclk"),
	FMCLK(ABIST, FM_DSI1_MPPLL_TST_CK, "fm_dsi1_mppll_tst_ck"),
	FMCLK(ABIST, FM_MAINPLL_CK, "fm_mainpll_ck"),
	FMCLK(ABIST, FM_MDPLL1_FS26M_GUIDE, "fm_mdpll1_fs26m_guide"),
	FMCLK(ABIST, FM_MFGPLL_CK, "fm_mfgpll_ck"),
	FMCLK(ABIST, FM_MMPLL_CK, "fm_mmpll_ck"),
	FMCLK(ABIST, FM_MMPLL_D3_CK, "fm_mmpll_d3_ck"),
	FMCLK(ABIST, FM_MPLL_CK, "fm_mpll_ck"),
	FMCLK(ABIST, FM_MSDCPLL_CK, "fm_msdcpll_ck"),
	FMCLK(ABIST, FM_RCLRPLL_DIV4_CH02, "fm_rclrpll_div4_ch02"),
	FMCLK(ABIST, FM_RCLRPLL_DIV4_CH13, "fm_rclrpll_div4_ch13"),
	FMCLK(ABIST, FM_RPHYPLL_DIV4_CH02, "fm_rphypll_div4_ch02"),
	FMCLK(ABIST, FM_RPHYPLL_DIV4_CH13, "fm_rphypll_div4_ch13"),
	FMCLK(ABIST, FM_TVDPLL_CK, "fm_tvdpll_ck"),
	FMCLK(ABIST, FM_ULPOSC2_CK, "fm_ulposc2_ck"),
	FMCLK(ABIST, FM_ULPOSC_CK, "fm_ulposc_ck"),
	FMCLK(ABIST, FM_UNIVPLL_CK, "fm_univpll_ck"),
	FMCLK(ABIST, FM_USB20_192M_CK, "fm_usb20_192m_ck"),
	FMCLK(ABIST, FM_MPLL_52M_DIV, "fm_mpll_52m_div"),
	FMCLK(ABIST, FM_UFS_MP_CLK2FREQ, "fm_ufs_mp_clk2freq"),
	FMCLK(ABIST, FM_WBG_DIG_BPLL_CK, "fm_wbg_dig_bpll_ck"),
	FMCLK(ABIST, FM_WBG_DIG_WPLL_CK960, "fm_wbg_dig_wpll_ck960"),
	FMCLK(ABIST, FMEM_AFT_CH0, "fmem_aft_ch0"),
	FMCLK(ABIST, FMEM_AFT_CH1, "fmem_aft_ch1"),
	FMCLK(ABIST, FMEM_AFT_CH2, "fmem_aft_ch2"),
	FMCLK(ABIST, FMEM_AFT_CH3, "fmem_aft_ch3"),
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
	FMCLK(ABIST, FM_RTC32K_I_VAO, "fm_rtc32k_i_vao"),
	FMCLK(ABIST, FM_CKOMO1_CK, "fm_ckomo1_ck"),
	FMCLK(ABIST, FM_CKMON2_CK, "fm_ckmon2_ck"),
	FMCLK(ABIST, FM_CKMON3_CK, "fm_ckmon3_ck"),
	FMCLK(ABIST, FM_CKMON4_CK, "fm_ckmon4_ck"),
	{},
};

#define _CKGEN(x)		(topck_base + (x))
#define CLK_MISC_CFG_0		_CKGEN(0x140)
#define CLK_DBG_CFG		_CKGEN(0x17C)
#define CLK26CALI_0		_CKGEN(0x220)
#define CLK26CALI_1		_CKGEN(0x224)

static void __iomem *topck_base;
static void __iomem *apmixed_base;
static void __iomem *spm_base;

const struct fmeter_clk *mt6893_get_fmeter_clks(void)
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

static int __mt_get_freq(unsigned int ID, int type)
{
	unsigned int temp, clk_dbg_cfg, clk_misc_cfg_0, clk26cali_1 = 0;
	unsigned long flags;
	int output = 0, i = 0;

	fmeter_lock(flags);
	while (clk_readl(CLK26CALI_0) & 0x10) {
		udelay(10);
		i++;
		if (i > FM_TIMEOUT)
			break;
	}

	if (type == CKGEN) {
		clk_dbg_cfg = clk_readl(CLK_DBG_CFG);
		clk_writel(CLK_DBG_CFG, (clk_dbg_cfg & 0xFFFFC0FC) | (ID << 8) | (0x1));
	} else if (type == ABIST) {
		clk_dbg_cfg = clk_readl(CLK_DBG_CFG);
		clk_writel(CLK_DBG_CFG, (clk_dbg_cfg & 0xFFC0FFFC) | (ID << 16));
	} else if (type == ABIST_2) {
		clk_dbg_cfg = clk_readl(CLK_DBG_CFG);
		clk_writel(CLK_DBG_CFG, (clk_dbg_cfg & 0xC0FFFFFC)| (ID << 24) | (0x2));
	} else
		return 0;

	clk_misc_cfg_0 = clk_readl(CLK_MISC_CFG_0);
	clk_writel(CLK_MISC_CFG_0, (clk_misc_cfg_0 & 0x00FFFFFF) | (3 << 24));

	clk26cali_1 = clk_readl(CLK26CALI_1);
	clk_writel(CLK26CALI_0, 0x0000);
	clk_writel(CLK26CALI_0, 0x1000);
	clk_writel(CLK26CALI_0, 0x1010);

	/* wait frequency meter finish */
	i = 0;
	do {
		udelay(10);
		i++;
		if (i > FM_TIMEOUT)
			break;
	} while (clk_readl(CLK26CALI_0) & 0x10);

	temp = clk_readl(CLK26CALI_1) & 0xFFFF;

	output = (temp * 26000) / 1024;

	clk_writel(CLK_DBG_CFG, clk_dbg_cfg);
	clk_writel(CLK_MISC_CFG_0, clk_misc_cfg_0);
	/*clk_writel(CLK26CALI_0, clk26cali_0);*/
	/*clk_writel(CLK26CALI_1, clk26cali_1);*/

	clk_writel(CLK26CALI_0, 0x0000);
	fmeter_unlock(flags);

	if (i > FM_TIMEOUT)
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

static unsigned int mt6893_get_ckgen_freq(unsigned int ID)
{
	if (check_mux_pdn(ID)) {
		pr_notice("ID-%d: MUX PDN, return 0.\n", ID);
		return 0;
	}

	return __mt_get_freq(ID, CKGEN);
}

static unsigned int mt6893_get_abist_freq(unsigned int ID)
{
	return __mt_get_freq(ID, ABIST);
}

static unsigned int mt6893_get_abist2_freq(unsigned int ID)
{
	return __mt_get_freq(ID, ABIST_2);
}

static unsigned int mt6893_get_fmeter_freq(unsigned int id, enum  FMETER_TYPE type)
{
	if (type == CKGEN)
		return mt6893_get_ckgen_freq(id);
	else if (type == ABIST)
		return mt6893_get_abist_freq(id);
	else if (type == ABIST_2)
		return mt6893_get_abist2_freq(id);

	return FT_NULL;
}

static int mt6893_get_fmeter_id(enum FMETER_ID fid)
{
	if (fid == FID_DISP_PWM)
		return FM_FDISP_PWM_CK;
	else if (fid == FID_ULPOSC1)
		return FM_ULPOSC_CK;
	else if (fid == FID_ULPOSC2)
		return FM_ULPOSC2_CK;

	return FID_NULL;;
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
	.get_fmeter_clks = mt6893_get_fmeter_clks,
	.get_ckgen_freq = mt6893_get_ckgen_freq,
	.get_abist_freq = mt6893_get_abist_freq,
	.get_abist2_freq = mt6893_get_abist2_freq,
	.get_fmeter_freq = mt6893_get_fmeter_freq,
	.get_fmeter_id = mt6893_get_fmeter_id,
};

static int clk_fmeter_mt6893_probe(struct platform_device *pdev)
{
	topck_base = get_base_from_comp("mediatek,mt6893-topckgen");
	if (IS_ERR(topck_base))
		goto ERR;

	apmixed_base = get_base_from_comp("mediatek,mt6893-apmixedsys");
	if (IS_ERR(apmixed_base))
		goto ERR;

	spm_base = get_base_from_comp("mediatek,mt6893-scpsys");
	if (IS_ERR(spm_base))
		goto ERR;

	fmeter_set_ops(&fm_ops);

	return 0;
ERR:
	pr_err("%s can't find base\n", __func__);

	return -EINVAL;
}

static struct platform_driver clk_fmeter_mt6893_drv = {
	.probe = clk_fmeter_mt6893_probe,
	.driver = {
		.name = "clk-fmeter-mt6893",
		.owner = THIS_MODULE,
	},
};

static int __init clk_fmeter_init(void)
{
	static struct platform_device *clk_fmeter_dev;

	clk_fmeter_dev = platform_device_register_simple("clk-fmeter-mt6893", -1, NULL, 0);
	if (IS_ERR(clk_fmeter_dev))
		pr_warn("unable to register clk-fmeter device");

	return platform_driver_register(&clk_fmeter_mt6893_drv);
}

static void __exit clk_fmeter_exit(void)
{
	platform_driver_unregister(&clk_fmeter_mt6893_drv);
}

subsys_initcall(clk_fmeter_init);
module_exit(clk_fmeter_exit);
MODULE_LICENSE("GPL");
