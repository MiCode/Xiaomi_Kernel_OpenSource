// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/module.h>

#include "clkdbg.h"
#include "mt6761_clkmgr.h"

#define ALL_CLK_ON		0
#define DUMP_INIT_STATE		0

/*
 * clkdbg dump_regs
 */

enum {
	topckgen,
	infracfg,
	pericfg,
	scpsys,
	apmixed,
	gce,
	audio,
	mipi_0a,
	mipi_0b,
	mipi_1a,
	mipi_1b,
	mipi_2a,
	mipi_2b,
	mfgsys,
	mmsys,
	camsys,
	vencsys,
};

#define REGBASE_V(_phys, _id_name) { .phys = _phys, .name = #_id_name }

/*
 * checkpatch.pl ERROR:COMPLEX_MACRO
 *
 * #define REGBASE(_phys, _id_name) [_id_name] = REGBASE_V(_phys, _id_name)
 */

static struct regbase rb[] = {
	[topckgen] = REGBASE_V(0x10000000, topckgen),
	[infracfg] = REGBASE_V(0x10001000, infracfg),
	[pericfg] = REGBASE_V(0x10003000, pericfg),
	[scpsys]   = REGBASE_V(0x10006000, scpsys),
	[apmixed]  = REGBASE_V(0x1000c000, apmixed),
	[gce]  = REGBASE_V(0x10238000, gce),
	[audio]    = REGBASE_V(0x11220000, audio),
	[mipi_0a]    = REGBASE_V(0x11c10000, mipi_0a),
	[mipi_0b]    = REGBASE_V(0x11c11000, mipi_0b),
	[mipi_1a]    = REGBASE_V(0x11c12000, mipi_1a),
	[mipi_1b]    = REGBASE_V(0x11c13000, mipi_1b),
	[mipi_2a]    = REGBASE_V(0x11c14000, mipi_2a),
	[mipi_2b]    = REGBASE_V(0x11c15000, mipi_2b),
	[mfgsys]   = REGBASE_V(0x13ffe000, mfgsys),
	[mmsys]    = REGBASE_V(0x14000000, mmsys),
	[camsys]   = REGBASE_V(0x15000000, camsys),
	[vencsys]  = REGBASE_V(0x17000000, vencsys),
};

#define REGNAME(_base, _ofs, _name)	\
	{ .base = &rb[_base], .ofs = _ofs, .name = #_name }

static struct regname rn[] = {
	REGNAME(topckgen,  0x040, CLK_CFG_0),
	REGNAME(topckgen,  0x050, CLK_CFG_1),
	REGNAME(topckgen,  0x060, CLK_CFG_2),
	REGNAME(topckgen,  0x070, CLK_CFG_3),
	REGNAME(topckgen,  0x080, CLK_CFG_4),
	REGNAME(topckgen,  0x090, CLK_CFG_5),
	REGNAME(topckgen,  0x0a0, CLK_CFG_6),
	REGNAME(topckgen,  0x0b0, CLK_CFG_7),
	REGNAME(audio,	0x000, AUDIO_TOP_CON0),
	REGNAME(audio,	0x004, AUDIO_TOP_CON1),
	REGNAME(camsys,  0x000, CAMSYS_CG),
	REGNAME(infracfg,  0x074, PERI_BUS_DCM_CTRL),
	REGNAME(infracfg,  0x090, MODULE_SW_CG_0),
	REGNAME(infracfg,  0x094, MODULE_SW_CG_1),
	REGNAME(infracfg,  0x0ac, MODULE_SW_CG_2),
	REGNAME(infracfg,  0x0c8, MODULE_SW_CG_3),
	REGNAME(infracfg,  0x200, INFRA_TOPAXI_SI0_CTL),
	REGNAME(pericfg,  0x20C, PERIAXI_SI0_CTL),
	REGNAME(gce,  0x0F0, GCE_CTL_INT0),
	REGNAME(mfgsys,  0x000, MFG_CG),
	REGNAME(mmsys,	0x100, MMSYS_CG_CON0),
	REGNAME(vencsys,  0x000, VCODECSYS_CG),
	REGNAME(mipi_0a,  0x080, MIPI_RX_WRAPPER80_CSI0A),
	REGNAME(mipi_0b,  0x080, MIPI_RX_WRAPPER80_CSI0A),
	REGNAME(mipi_1a,  0x080, MIPI_RX_WRAPPER80_CSI0A),
	REGNAME(mipi_1b,  0x080, MIPI_RX_WRAPPER80_CSI0A),
	REGNAME(mipi_2a,  0x080, MIPI_RX_WRAPPER80_CSI0A),
	REGNAME(mipi_2b,  0x080, MIPI_RX_WRAPPER80_CSI0A),

	REGNAME(apmixed,  0x208, UNIVPLL_CON0),
	REGNAME(apmixed,  0x20C, UNIVPLL_CON1),
	REGNAME(apmixed,  0x214, UNIVPLL_PWR_CON0),
	REGNAME(apmixed,  0x218, MFGPLL_CON0),
	REGNAME(apmixed,  0x21C, MFGPLL_CON1),
	REGNAME(apmixed,  0x224, MFGPLL_PWR_CON0),
	REGNAME(apmixed,  0x228, MAINPLL_CON0),
	REGNAME(apmixed,  0x22C, MAINPLL_CON1),
	REGNAME(apmixed,  0x234, MAINPLL_PWR_CON0),
	REGNAME(apmixed,  0x30C, ARMPLL_CON0),
	REGNAME(apmixed,  0x310, ARMPLL_CON1),
	REGNAME(apmixed,  0x318, ARMPLL_PWR_CON0),
	REGNAME(apmixed,  0x31C, APLL1_CON0),
	REGNAME(apmixed,  0x320, APLL1_CON1),
	REGNAME(apmixed,  0x328, APLL1_CON3),
	REGNAME(apmixed,  0x32C, APLL1_PWR_CON0),
	REGNAME(apmixed,  0x330, MMPLL_CON0),
	REGNAME(apmixed,  0x334, MMPLL_CON1),
	REGNAME(apmixed,  0x33C, MMPLL_PWR_CON0),
	REGNAME(apmixed,  0x340, MPLL_CON0),
	REGNAME(apmixed,  0x344, MPLL_CON1),
	REGNAME(apmixed,  0x34C, MPLL_PWR_CON0),
	REGNAME(apmixed,  0x350, MSDCPLL_CON0),
	REGNAME(apmixed,  0x354, MSDCPLL_CON1),
	REGNAME(apmixed,  0x35C, MSDCPLL_PWR_CON0),
	REGNAME(apmixed,  0x014, AP_PLL_CON5),

	REGNAME(scpsys,  0x0180, PWR_STATUS),
	REGNAME(scpsys,  0x0184, PWR_STATUS_2ND),
	REGNAME(scpsys,  0x0300, VCODEC_PWR_CON),
	REGNAME(scpsys,  0x0308, ISP_PWR_CON),
	REGNAME(scpsys,  0x030C, DIS_PWR_CON),
	REGNAME(scpsys,  0x0314, AUDIO_PWR_CON),
	REGNAME(scpsys,  0x0318, IFR_PWR_CON),
	REGNAME(scpsys,  0x031C, DPY_PWR_CON),
	REGNAME(scpsys,  0x0320, MD1_PWR_CON),
	REGNAME(scpsys,  0x032C, CONN_PWR_CON),
	REGNAME(scpsys,  0x0334, MFG_ASYNC_PWR_CON),
	REGNAME(scpsys,  0x0338, MFG_PWR_CON),
	REGNAME(scpsys,  0x033C, MFG_CORE0_PWR_CON),
	REGNAME(scpsys,  0x0344, CAM_PWR_CON),
	{}
};

static const struct regname *get_all_regnames(void)
{
	return rn;
}

static void __init init_regbase(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(rb); i++)
		rb[i].virt = ioremap(rb[i].phys, PAGE_SIZE);
}

/*
 * clkdbg fmeter
 */

#include <linux/delay.h>

#ifndef GENMASK
#define GENMASK(h, l)	(((1U << ((h) - (l) + 1)) - 1) << (l))
#endif

#define ALT_BITS(o, h, l, v) \
	(((o) & ~GENMASK(h, l)) | (((v) << (l)) & GENMASK(h, l)))

#define clk_readl(addr)		readl(addr)
#define clk_writel(addr, val)	\
	do { writel(val, addr); wmb(); } while (0) /* sync write */
#define clk_writel_mask(addr, h, l, v)	\
	clk_writel(addr, (clk_readl(addr) & ~GENMASK(h, l)) | ((v) << (l)))

#define ABS_DIFF(a, b)	((a) > (b) ? (a) - (b) : (b) - (a))

#define FMCLK(_t, _i, _n) { .type = _t, .id = _i, .name = _n }

static const struct fmeter_clk fclks[] = {
	FMCLK(CKGEN,  1, "hd_faxi_ck"),
	FMCLK(CKGEN,  3, "hf_fmm_ck"),
	FMCLK(CKGEN,  4, "hf_fscp_ck"),
	FMCLK(CKGEN,  5, "hf_fmfg_ck"),
	FMCLK(CKGEN,  6, "hf_fatb_ck"),
	FMCLK(CKGEN,  7, "f_fcamtg_ck"),
	FMCLK(CKGEN,  8, "f_fcamtg1_ck"),
	FMCLK(CKGEN,  9, "f_fcamtg2_ck"),
	FMCLK(CKGEN,  10, "f_fcamtg3_ck"),
	FMCLK(CKGEN,  11, "f_fuart_ck"),
	FMCLK(CKGEN,  12, "hf_fspi_ck"),
	FMCLK(CKGEN,  13, "hf_fmsdc50_0_hclk_ck"),
	FMCLK(CKGEN,  14, "hf_fmsdc50_0_ck"),
	FMCLK(CKGEN,  15, "hf_fmsdc30_1_ck"),
	FMCLK(CKGEN,  16, "hf_faudio_ck"),
	FMCLK(CKGEN,  17, "hf_faud_intbus_ck"),
	FMCLK(CKGEN,  18, "hf_faud_1_ck"),
	FMCLK(CKGEN,  19, "hf_faud_engen1_ck"),
	FMCLK(CKGEN,  20, "f_fdisp_pwm_ck"),
	FMCLK(CKGEN,  21, "hf_fsspm_ck"),
	FMCLK(CKGEN,  22, "hf_fdxcc_ck"),
	FMCLK(CKGEN,  23, "f_fusb_top_ck"),
	FMCLK(CKGEN,  24, "hf_fspm_ck"),
	FMCLK(CKGEN,  25, "hf_fi2c_ck"),
	FMCLK(CKGEN,  26, "f_fpwm_ck"),
	FMCLK(CKGEN,  27, "f_fseninf_ck"),
	FMCLK(CKGEN,  28, "hf_faes_fde_ck"),
	FMCLK(CKGEN,  29, "f_fpwrap_ulposc_ck"),
	FMCLK(CKGEN,  30, "f_fcamtm_ck"),
	FMCLK(CKGEN,  48, "f_ufs_mp_sap_cfg_ck"),
	FMCLK(CKGEN,  49, "f_ufs_tick1us_ck"),
	FMCLK(CKGEN,  50, "hd_faxi_east_ck"),
	FMCLK(CKGEN,  51, "hd_faxi_west_ck"),
	FMCLK(CKGEN,  52, "hd_faxi_north_ck"),
	FMCLK(CKGEN,  53, "hd_faxi_south_ck"),
	FMCLK(CKGEN,  54, "hg_fmipicfg_tx_ck"),
	FMCLK(CKGEN,  55, "fmem_ck_bfe_dcm_ch0"),
	FMCLK(CKGEN,  56, "fmem_ck_aft_dcm_ch0"),
	FMCLK(CKGEN,  57, "fmem_ck_bfe_dcm_ch1"),
	FMCLK(CKGEN,  58, "fmem_ck_aft_dcm_ch1"),
	FMCLK(ABIST,  1, "AD_CSI0A_CDPHY_DELAYCAL_CK"),
	FMCLK(ABIST,  2, "AD_CSI0B_CDPHY_DELAYCAL_CK"),
	FMCLK(ABIST,  4, "AD_MDBPIPLL_DIV3_CK"),
	FMCLK(ABIST,  5, "AD_MDBPIPLL_DIV7_CK"),
	FMCLK(ABIST,  6, "AD_MDBRPPLL_DIV6_CK"),
	FMCLK(ABIST,  7, "AD_UNIV_624M_CK"),
	FMCLK(ABIST,  8, "AD_MAIN_H546M_CK"),
	FMCLK(ABIST,  9, "AD_MAIN_H364M_CK"),
	FMCLK(ABIST,  10, "AD_MAIN_H218P4M_CK"),
	FMCLK(ABIST,  11, "AD_MAIN_H156M_CK"),
	FMCLK(ABIST,  12, "AD_UNIV_624M_CK_DUMMY"),
	FMCLK(ABIST,  13, "AD_UNIV_416M_CK"),
	FMCLK(ABIST,  14, "AD_UNIV_249P6M_CK"),
	FMCLK(ABIST,  15, "AD_UNIV_178P3M_CK"),
	FMCLK(ABIST,  16, "AD_MDPLL1_FS26M_CK"),
	FMCLK(ABIST,  17, "AD_CSI1A_CDPHY_DELAYCAL_CK"),
	FMCLK(ABIST,  18, "AD_CSI1B_CDPHY_DELAYCAL_CK"),
	FMCLK(ABIST,  19, "AD_CSI2A_CDPHY_DELAYCAL_CK"),
	FMCLK(ABIST,  20, "AD_CSI2B_CDPHY_DELAYCAL_CK"),
	FMCLK(ABIST,  22, "AD_ARMPLL_CK"),
	FMCLK(ABIST,  23, "AD_MAINPLL_1092M_CK"),
	FMCLK(ABIST,  24, "AD_UNIVPLL_1248M_CK"),
	FMCLK(ABIST,  25, "AD_MFGPLL_CK"),
	FMCLK(ABIST,  26, "AD_MSDCPLL_CK"),
	FMCLK(ABIST,  27, "AD_MMPLL_CK"),
	FMCLK(ABIST,  28, "AD_APLL1_196P608M_CK"),
	FMCLK(ABIST,  30, "AD_APPLLGP_TST_CK"),
	FMCLK(ABIST,  31, "AD_USB20_192M_CK"),
	FMCLK(ABIST,  35, "AD_DSI0_MPPLL_TST_CK"),
	FMCLK(ABIST,  36, "AD_DSI0_LNTC_DSICLK"),
	FMCLK(ABIST,  37, "ad_ulposc1_ck"),
	FMCLK(ABIST,  38, "ad_ulposc2_ck"),
	FMCLK(ABIST,  39, "rtc32k_ck_i"),
	FMCLK(ABIST,  40, "mcusys_arm_clk_out_all"),
	FMCLK(ABIST,  41, "AD_ULPOSC1_SYNC_CK"),
	FMCLK(ABIST,  42, "AD_ULPOSC2_SYNC_CK"),
	FMCLK(ABIST,  43, "msdc01_in_ck"),
	FMCLK(ABIST,  44, "msdc02_in_ck"),
	FMCLK(ABIST,  45, "msdc11_in_ck"),
	FMCLK(ABIST,  46, "msdc12_in_ck"),
	FMCLK(ABIST,  49, "AD_PLLGP2_TST_CK"),
	FMCLK(ABIST,  50, "AD_MPLL_208M_CK"),
	FMCLK(ABIST,  54, "DA_USB20_48M_DIV_CK"),
	FMCLK(ABIST,  55, "DA_UNIV_48M_DIV_CK"),
	FMCLK(ABIST,  57, "DA_MPLL_52M_DIV_CK"),
	FMCLK(ABIST,  58, "DA_ARMCPU_MON_CK"),
	FMCLK(ABIST,  60, "ckmon1_ck"),
	FMCLK(ABIST,  61, "ckmon2_ck"),
	FMCLK(ABIST,  62, "ckmon3_ck"),
	FMCLK(ABIST,  63, "ckmon4_ck"),
	{}
};


#define PLL_HP_CON0			(rb[apmixed].virt + 0x014)
#define PLL_TEST_CON1			(rb[apmixed].virt + 0x03c)
#define TEST_DBG_CTRL			(rb[topckgen].virt + 0x38)
#define FREQ_MTR_CTRL_REG		(rb[topckgen].virt + 0x10)
#define FREQ_MTR_CTRL_RDATA		(rb[topckgen].virt + 0x14)

#define RG_FQMTR_CKDIV_GET(x)		(((x) >> 28) & 0x3)
#define RG_FQMTR_CKDIV_SET(x)		(((x) & 0x3) << 28)
#define RG_FQMTR_FIXCLK_SEL_GET(x)	(((x) >> 24) & 0x3)
#define RG_FQMTR_FIXCLK_SEL_SET(x)	(((x) & 0x3) << 24)
#define RG_FQMTR_MONCLK_SEL_GET(x)	(((x) >> 16) & 0x7f)
#define RG_FQMTR_MONCLK_SEL_SET(x)	(((x) & 0x7f) << 16)
#define RG_FQMTR_MONCLK_EN_GET(x)	(((x) >> 15) & 0x1)
#define RG_FQMTR_MONCLK_EN_SET(x)	(((x) & 0x1) << 15)
#define RG_FQMTR_MONCLK_RST_GET(x)	(((x) >> 14) & 0x1)
#define RG_FQMTR_MONCLK_RST_SET(x)	(((x) & 0x1) << 14)
#define RG_FQMTR_MONCLK_WINDOW_GET(x)	(((x) >> 0) & 0xfff)
#define RG_FQMTR_MONCLK_WINDOW_SET(x)	(((x) & 0xfff) << 0)

#define RG_FQMTR_CKDIV_DIV_2		0
#define RG_FQMTR_CKDIV_DIV_4		1
#define RG_FQMTR_CKDIV_DIV_8		2
#define RG_FQMTR_CKDIV_DIV_16		3

#define RG_FQMTR_FIXCLK_26MHZ		0
#define RG_FQMTR_FIXCLK_32KHZ		2

#define RG_FQMTR_EN     1
#define RG_FQMTR_RST    1

#define RG_FRMTR_WINDOW     519

static const struct fmeter_clk *get_all_fmeter_clks(void)
{
	return fclks;
}

struct bak {
	u32 pll_hp_con0;
	u32 pll_test_con1;
	u32 test_dbg_ctrl;
};

static u32 fmeter_freq_op(const struct fmeter_clk *fclk)
{
	if (fclk->type == ABIST)
		return mt_get_abist_freq(fclk->id);
	else if (fclk->type == CKGEN)
		return mt_get_ckgen_freq(fclk->id);
	return 0;
}

/*
 * clkdbg dump_state
 */

static const char * const *get_all_clk_names(void)
{
	static const char * const clks[] = {
		/* PLLs */
		"armpll",
		"mainpll",
		"univpll",
		"msdcpll",
		"mfgpll",
		"mmpll",
		"mpll",
		"apll1",

		/* TOP */
		"syspll_ck",
		"syspll_d2",
		"syspll1_d2",
		"syspll1_d4",
		"syspll1_d8",
		"syspll1_d16",
		"syspll_d3",
		"syspll2_d2",
		"syspll2_d4",
		"syspll2_d8",
		"syspll_d5",
		"syspll3_d2",
		"syspll3_d4",
		"syspll_d7",
		"syspll4_d2",
		"syspll4_d4",
		"usb20_192m_ck",
		"usb20_192m_d4",
		"usb20_192m_d8",
		"usb20_192m_d16",
		"usb20_192m_d32",
		"univpll_d2",
		"univpll1_d2",
		"univpll1_d4",
		"univpll_d3",
		"univpll2_d2",
		"univpll2_d4",
		"univpll2_d8",
		"univpll2_d32",
		"univpll_d5",
		"univpll3_d2",
		"univpll3_d4",
		"mmpll_ck",
		"mmpll_d2",
		"mpll_ck",
		"mpll_104m_div_ck",
		"mpll_52m_div_ck",
		"mfgpll_ck",
		"msdcpll_ck",
		"msdcpll_d2",
		"apll1_ck",
		"apll1_d2",
		"apll1_d4",
		"apll1_d8",
		"ulposc1_ck",
		"ulposc1_d2",
		"ulposc1_d4",
		"ulposc1_d8",
		"ulposc1_d16",
		"ulposc1_d32",

		"f_frtc_ck",
		"clk_26m_ck",
		"dmpll_ck",

		"axi_sel",
		"mem_sel",
		"mm_sel",
		"scp_sel",

		"mfg_sel",
		"atb_sel",
		"camtg_sel",
		"camtg1_sel",

		"camtg2_sel",
		"camtg3_sel",
		"uart_sel",
		"spi_sel",

		"msdc50_hclk_sel",
		"msdc50_0_sel",
		"msdc30_1_sel",
		"audio_sel",

		"aud_intbus_sel",
		"aud_1_sel",
		"aud_engen1_sel",
		"disp_pwm_sel",

		"sspm_sel",
		"dxcc_sel",
		"usb_top_sel",
		"spm_sel",

		"i2c_sel",
		"pwm_sel",
		"seninf_sel",
		"aes_fde_sel",

		"pwrap_ulposc_sel",
		"camtm_sel",

		/* INFRACFG */
		"ifr_axi_dis",
		"ifr_pmic_tmr",
		"ifr_pmic_ap",
		"ifr_pmic_md",
		"ifr_pmic_conn",
		"ifr_scp_core",
		"ifr_sej",
		"ifr_apxgpt",
		"ifr_icusb",
		"ifr_gce",
		"ifr_therm",
		"ifr_i2c_ap",
		"ifr_i2c_ccu",
		"ifr_i2c_sspm",
		"ifr_i2c_rsv",
		"ifr_pwm_hclk",
		"ifr_pwm1",
		"ifr_pwm2",
		"ifr_pwm3",
		"ifr_pwm4",
		"ifr_pwm5",
		"ifr_pwm",
		"ifr_uart0",
		"ifr_uart1",
		"ifr_gce_26m",
		"ifr_dma",
		"ifr_btif",
		"ifr_spi0",
		"ifr_msdc0",
		"ifr_msdc1",
		"ifr_dvfsrc",
		"ifr_gcpu",
		"ifr_trng",
		"ifr_auxadc",
		"ifr_cpum",
		"ifr_ccif1_ap",
		"ifr_ccif1_md",
		"ifr_auxadc_md",
		"ifr_ap_dma",
		"ifr_xiu",
		"ifr_dapc",
		"ifr_ccif_ap",
		"ifr_debugtop",
		"ifr_audio",
		"ifr_ccif_md",
		"ifr_secore",
		"ifr_dxcc_ao",
		"ifr_dramc26",
		"ifr_pwmfb",
		"ifr_disp_pwm",
		"ifr_cldmabclk",
		"ifr_audio26m",
		"ifr_spi1",
		"ifr_i2c4",
		"ifr_mdtemp",
		"ifr_spi2",
		"ifr_spi3",
		"ifr_hf_fsspm",
		"ifr_i2c5",
		"ifr_i2c5a",
		"ifr_i2c5_imm",
		"ifr_i2c1a",
		"ifr_i2c1_imm",
		"ifr_i2c2a",
		"ifr_i2c2_imm",
		"ifr_spi4",
		"ifr_spi5",
		"ifr_cq_dma",
		"ifr_faes_fde_ck",
		"ifr_msdc0f",
		"ifr_msdc1sf",
		"ifr_sspm_26m",
		"ifr_sspm_32k",
		"ifr_i2c6",
		"ifr_ap_msdc0",
		"ifr_md_msdc0",
		"ifr_msdc0_clk",
		"ifr_msdc1_clk",
		"ifr_sej_f13m",
		"ifr_aes",
		"ifr_mcu_pm_bclk",
		"ifr_ccif2_ap",
		"ifr_ccif2_md",
		"ifr_ccif3_ap",
		"ifr_ccif3_md",

		/* PERICFG */
		"periaxi_disable",

		/* AUDIO */
		"aud_afe",
		"aud_22m",
		"aud_apll_tuner",
		"aud_adc",
		"aud_dac",
		"aud_dac_predis",
		"aud_tml",
		"aud_i2s1_bclk",
		"aud_i2s2_bclk",
		"aud_i2s3_bclk",
		"aud_i2s4_bclk",

		/* CAM */
		"cam_larb2",
		"cam",
		"camtg",
		"cam_seninf",
		"camsv0",
		"camsv1",
		"cam_fdvt",

		/* MFG */
		"mfgcfg_baxi",
		"mfgcfg_bmem",
		"mfgcfg_bg3d",
		"mfgcfg_b26m",

		/* MM */
		"mm_mdp_rdma0",
		"mm_mdp_ccorr0",
		"mm_mdp_rsz0",
		"mm_mdp_rsz1",
		"mm_mdp_tdshp0",
		"mm_mdp_wrot0",
		"mm_mdp_wdma0",
		"mm_disp_ovl0",
		"mm_disp_ovl0_2l",
		"mm_disp_rsz",
		"mm_disp_rdma0",
		"mm_disp_wdma0",
		"mm_disp_color0",
		"mm_disp_ccorr0",
		"mm_disp_aal0",
		"mm_disp_gamma0",
		"mm_disp_dither0",
		"mm_dsi0",
		"mm_fake_eng",
		"mm_smi_common",
		"mm_smi_larb0",
		"mm_smi_comm0",
		"mm_smi_comm1",
		"mm_cam_mdp_ck",
		"mm_smi_img_ck",
		"mm_smi_cam_ck",
		"mm_img_dl_relay",
		"mm_imgdl_async",
		"mm_dig_dsi_ck",
		"mm_hrtwt",

		/* VENC */
		"venc_set0_larb",
		"venc_set1_venc",
		"jpgenc",
		"venc_set3_vdec",

		/* MIPI */
		"mipi0a_csr_0a",
		"mipi0b_csr_0b",
		"mipi1a_csr_1a",
		"mipi1b_csr_1b",
		"mipi2a_csr_2a",
		"mipi2b_csr_2b",

		/* GCE */
		"gce",

		/* APMIXED 26MCK */
		"apmixed_ssusb26m",
		"apmixed_appll26m",
		"apmixed_mipic026m",
		"apmixed_mdpll26m",
		"apmixed_mmsys26m",
		"apmixed_ufs26m",
		"apmixed_mipic126m",
		"apmixed_mempll26m",
		"apmixed_lvpll26m",
		"apmixed_mipid026m",

		/* end */
		NULL
	};

	return clks;
}

/*
 * clkdbg pwr_status
 */

static const char * const *get_pwr_names(void)
{
	static const char * const pwr_names[] = {
		[0]  = "MD1",
		[1]  = "CONN",
		[2]  = "DPY",
		[3]  = "DISP",
		[4]  = "MFG",
		[5]  = "",
		[6]  = "IFR",
		[7]  = "MFG_CORE0",
		[8]  = "",
		[9]  = "",
		[10] = "",
		[11] = "",
		[12] = "",
		[13] = "",
		[14] = "",
		[15] = "",
		[16] = "",
		[17] = "",
		[18] = "",
		[19] = "",
		[20] = "",
		[21] = "",
		[22] = "",
		[23] = "MFG_ASYNC",
		[24] = "AUDIO",
		[25] = "CAM",
		[26] = "VCODEC",
		[27] = "",
		[28] = "C2K",
		[29] = "",
		[30] = "",
		[31] = "",
	};

	return pwr_names;
}

/*
 * clkdbg dump_clks
 */

static void setup_provider_clk(struct provider_clk *pvdck)
{
	static const struct {
		const char *pvdname;
		u32 pwr_mask;
	} pvd_pwr_mask[] = {
		{"mfgcfg", BIT(4) | BIT(7) | BIT(23)},
		{"mmsys", BIT(3)},
		{"camsys", BIT(3) | BIT(5)},
		{"venc_gcon", BIT(3) | BIT(26)},
		{"mmsys", BIT(3)},
	};

	int i;
	const char *pvdname = pvdck->provider_name;

	if (!pvdname)
		return;

	for (i = 0; i < ARRAY_SIZE(pvd_pwr_mask); i++) {
		if (strcmp(pvdname, pvd_pwr_mask[i].pvdname) == 0) {
			pvdck->pwr_mask = pvd_pwr_mask[i].pwr_mask;
			return;
		}
	}
}

/*
 * chip_ver functions
 */

#include <linux/seq_file.h>
#include <mt-plat/mtk_chip.h>

static int clkdbg_chip_ver(struct seq_file *s, void *v)
{
	static const char * const sw_ver_name[] = {
		"CHIP_SW_VER_01",
		"CHIP_SW_VER_02",
		"CHIP_SW_VER_03",
		"CHIP_SW_VER_04",
	};

	seq_printf(s, "mt_get_chip_sw_ver(): %d (%s)\n", 0, sw_ver_name[0]);

	return 0;
}

/*
 * init functions
 */

static struct clkdbg_ops clkdbg_mt6761_ops = {
	.get_all_fmeter_clks = get_all_fmeter_clks,
	.prepare_fmeter = NULL,
	.unprepare_fmeter = NULL,
	.fmeter_freq = fmeter_freq_op,
	.get_all_regnames = get_all_regnames,
	.get_all_clk_names = get_all_clk_names,
	.get_pwr_names = get_pwr_names,
	.setup_provider_clk = setup_provider_clk,
};

static void __init init_custom_cmds(void)
{
	static const struct cmd_fn cmds[] = {
		CMDFN("chip_ver", clkdbg_chip_ver),
		{}
	};

	set_custom_cmds(cmds);
}

static int __init clkdbg_mt6761_init(void)
{
	if (!of_machine_is_compatible("mediatek,MT6761"))
		return -ENODEV;

	init_regbase();

	init_custom_cmds();
	set_clkdbg_ops(&clkdbg_mt6761_ops);

#if ALL_CLK_ON
	prepare_enable_provider("topckgen");
	reg_pdrv("all");
	prepare_enable_provider("all");
#endif

#if DUMP_INIT_STATE
	print_regs();
	print_fmeter_all();
#endif /* DUMP_INIT_STATE */

	return 0;
}

static void __exit clkdbg_mt6761_exit(void)
{
}

subsys_initcall(clkdbg_mt6761_init);
module_exit(clkdbg_mt6761_exit);

MODULE_LICENSE("GPL");
