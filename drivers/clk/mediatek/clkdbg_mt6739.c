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

#include <linux/clk-provider.h>
#include <linux/io.h>

#include "clkdbg.h"
#include "mt6739_clkmgr.h"

#define ALL_CLK_ON		0
#define DUMP_INIT_STATE		0

/*
 * clkdbg dump_regs
 */

enum {
	topckgen,
	infracfg,
	scpsys,
	apmixed,
	audio,
	mfgsys,
	mmsys,
	imgsys,
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
	[scpsys]   = REGBASE_V(0x10006000, scpsys),
	[apmixed]  = REGBASE_V(0x1000c000, apmixed),
	[audio]    = REGBASE_V(0x11220000, audio),
	[mfgsys]   = REGBASE_V(0x13000000, mfgsys),
	[mmsys]    = REGBASE_V(0x14000000, mmsys),
	[imgsys]   = REGBASE_V(0x15000000, imgsys),
	[vencsys]  = REGBASE_V(0x17000000, vencsys),
};

#define REGNAME(_base, _ofs, _name)	\
	{ .base = &rb[_base], .ofs = _ofs, .name = #_name }

static struct regname rn[] = {
	REGNAME(topckgen,  0x40, CLK_CFG_0),
	REGNAME(topckgen,  0x50, CLK_CFG_1),
	REGNAME(topckgen,  0x60, CLK_CFG_2),
	REGNAME(topckgen,  0x70, CLK_CFG_3),
	REGNAME(topckgen,  0x80, CLK_CFG_4),
	REGNAME(topckgen,  0x90, CLK_CFG_5),
	REGNAME(topckgen,  0xa0, CLK_CFG_6),
	REGNAME(topckgen,  0xb0, CLK_CFG_7),
	REGNAME(topckgen,  0xc0, CLK_CFG_8),
	REGNAME(topckgen,  0xd0, CLK_CFG_9),
	REGNAME(topckgen,  0xe0, CLK_CFG_10),
	REGNAME(infracfg,  0x90, MODULE_SW_CG_0),
	REGNAME(infracfg,  0x94, MODULE_SW_CG_1),
	REGNAME(infracfg,  0xac, MODULE_SW_CG_2),
	REGNAME(infracfg,  0xc8, MODULE_SW_CG_3),
	REGNAME(audio,	0x000, AUDIO_TOP_CON0),
	REGNAME(audio,	0x004, AUDIO_TOP_CON1),
	REGNAME(imgsys, 0x000, IMG_CG),
	REGNAME(mmsys,	0x100, MMSYS_CG_CON0),
	REGNAME(vencsys, 0x0, VCODECSYS_CG_CON),
	REGNAME(apmixed,  0x220, MAINPLL_CON0),
	REGNAME(apmixed,  0x224, MAINPLL_CON1),
	REGNAME(apmixed,  0x22C, MAINPLL_PWR_CON0),
	REGNAME(apmixed,  0x230, UNIVPLL_CON0),
	REGNAME(apmixed,  0x234, UNIVPLL_CON1),
	REGNAME(apmixed,  0x238, UNIVPLL_PWR_CON0),
	REGNAME(apmixed,  0x240, MFGPLL_CON0),
	REGNAME(apmixed,  0x244, MFGPLL_CON1),
	REGNAME(apmixed,  0x24C, MFGPLL_PWR_CON0),
	REGNAME(apmixed,  0x250, MSDCPLL_CON0),
	REGNAME(apmixed,  0x254, MSDCPLL_CON1),
	REGNAME(apmixed,  0x25C, MSDCPLL_PWR_CON0),
	REGNAME(apmixed,  0x270, MMPLL_CON0),
	REGNAME(apmixed,  0x274, MMPLL_CON1),
	REGNAME(apmixed,  0x27C, MMPLL_PWR_CON0),
	REGNAME(apmixed,  0x2A0, APLL1_CON0),
	REGNAME(apmixed,  0x2A4, APLL1_CON1),
	REGNAME(apmixed,  0x2B0, APLL1_PWR_CON0),
	REGNAME(apmixed,  0x000, AP_PLL_CON0),
	REGNAME(apmixed,  0x004, AP_PLL_CON1),
	REGNAME(apmixed,  0x008, AP_PLL_CON2),
	REGNAME(apmixed,  0x00C, AP_PLL_CON3),
	REGNAME(apmixed,  0x010, AP_PLL_CON4),
	REGNAME(apmixed,  0x018, AP_PLL_CON6),
	REGNAME(scpsys,  0x0180, PWR_STATUS),
	REGNAME(scpsys,  0x0184, PWR_STATUS_2ND),
	REGNAME(scpsys,  0x0300, VEN_PWR_CON),
	REGNAME(scpsys,  0x0338, MFG_PWR_CON),
	REGNAME(scpsys,  0x0314, C2K_PWR_CON),
	REGNAME(scpsys,  0x0320, MD1_PWR_CON),
	REGNAME(scpsys,  0x032C, CONN_PWR_CON),
	REGNAME(scpsys,  0x032C, AUD_PWR_CON),
	REGNAME(scpsys,  0x030C, MM0_PWR_CON),
	REGNAME(scpsys,  0x0308, ISP_PWR_CON),
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

enum FMETER_TYPE {
	FT_NULL,
	ABIST,
	CKGEN
};

#define FMCLK(_t, _i, _n) { .type = _t, .id = _i, .name = _n }

static const struct fmeter_clk fclks[] = {
	FMCLK(CKGEN, 1, "hd_faxi_ck"),
	FMCLK(CKGEN, 2, "hf_fddrphycfg_ck"),
	FMCLK(CKGEN, 3, "hf_fmm_ck"),
	FMCLK(CKGEN, 7, "hf_fmfg_ck"),
	FMCLK(CKGEN, 8, "f_fcamtg_ck"),
	FMCLK(CKGEN, 9, "f_fuart_ck"),
	FMCLK(CKGEN, 10, "hf_fspi_ck"),
	FMCLK(CKGEN, 12, "hf_fmsdc50_0_hclk_ck"),
	FMCLK(CKGEN, 13, "hf_fmsdc50_0_ck"),
	FMCLK(CKGEN, 14, "hf_fmsdc30_1_ck"),
	FMCLK(CKGEN, 17, "hf_faudio_ck"),
	FMCLK(CKGEN, 18, "hf_faud_intbus_ck"),
	FMCLK(CKGEN, 23, "hf_fdbi0_ck"),
	FMCLK(CKGEN, 24, "hf_fscam_ck"),
	FMCLK(CKGEN, 25, "hf_faud_1_ck"),
	FMCLK(CKGEN, 27, "f_fdisp_pwm_ck"),
	FMCLK(CKGEN, 28, "hf_fnfi2x_ck"),
	FMCLK(CKGEN, 29, "hf_fnfiecc_ck"),
	FMCLK(CKGEN, 30, "f_fusb_top_ck"),
	FMCLK(CKGEN, 31, "hg_fspm_ck"),
	FMCLK(CKGEN, 33, "f_fi2c_ck"),
	FMCLK(CKGEN, 35, "f_fseninf_ck"),
	FMCLK(CKGEN, 36, "f_fdxcc_ck"),
	FMCLK(CKGEN, 37, "hf_faud_engin1_ck"),
	FMCLK(CKGEN, 38, "hf_faud_engin2_ck"),
	FMCLK(CKGEN, 41, "f_fcamtg2_ck"),
	FMCLK(CKGEN, 47, "hf_fnfi1x_ck"),
	FMCLK(CKGEN, 48, "f_ufs_mp_sap_cfg_ck"),
	FMCLK(CKGEN, 49, "f_ufs_tick1us_ck"),
	FMCLK(CKGEN, 50, "hd_faxi_east_ck"),
	FMCLK(CKGEN, 51, "hd_faxi_west_ck"),
	FMCLK(CKGEN, 52, "hd_faxi_north_ck"),
	FMCLK(CKGEN, 53, "hd_faxi_south_ck"),
	FMCLK(CKGEN, 54, "hg_fmipicfg_tx_ck"),
	FMCLK(CKGEN, 55, "fmem_ck_bfe_dcm_ch0"),
	FMCLK(CKGEN, 56, "fmem_ck_aft_dcm_ch0"),
	FMCLK(CKGEN, 59, "dramc_pll104m_ck"),
	FMCLK(ABIST, 1, "AD_CSI0_DELAY_TSTCLK"),
	FMCLK(ABIST, 2, "AD_CSI1_DELAY_TSTCLK"),
	FMCLK(ABIST, 4, "AD_MDBPIPLL_DIV3_CK"),
	FMCLK(ABIST, 5, "AD_MDBPIPLL_DIV7_CK"),
	FMCLK(ABIST, 6, "AD_MDBRPPLL_DIV6_CK"),
	FMCLK(ABIST, 7, "AD_UNIV_624M_CK"),
	FMCLK(ABIST, 8, "AD_MAIN_H546_CK"),
	FMCLK(ABIST, 9, "AD_MEMPLL_MONCLK"),
	FMCLK(ABIST, 10, "AD_MEMPLL2_MONCLK"),
	FMCLK(ABIST, 11, "AD_MEMPLL3_MONCLK"),
	FMCLK(ABIST, 12, "AD_MEMPLL4_MONCLK"),
	FMCLK(ABIST, 16, "AD_MDPLL_FS26M_CK"),
	FMCLK(ABIST, 20, "AD_ARMPLL_L_CK"),
	FMCLK(ABIST, 22, "AD_ARMPLL_LL_CK"),
	FMCLK(ABIST, 23, "AD_MAINPLL_CK"),
	FMCLK(ABIST, 24, "AD_UNIVPLL_CK"),
	FMCLK(ABIST, 26, "AD_MSDCPLL_CK"),
	FMCLK(ABIST, 27, "AD_MMPLL_CK"),
	FMCLK(ABIST, 28, "AD_APLL1_CK"),
	FMCLK(ABIST, 30, "AD_APPLLGP_TST_CK"),
	FMCLK(ABIST, 31, "AD_USB20_192M_CK"),
	FMCLK(ABIST, 32, "AD_UNIV_192M_CK"),
	FMCLK(ABIST, 34, "AD_VENCPLL_CK"),
	FMCLK(ABIST, 35, "AD_DSI0_MPPLL_TST_CK"),
	FMCLK(ABIST, 36, "AD_DSI0_LNTC_DSICLK"),
	FMCLK(ABIST, 39, "rtc32k_ck_i"),
	FMCLK(ABIST, 40, "mcusys_arm_clk_out_all"),
	FMCLK(ABIST, 43, "msdc01_in_ck"),
	FMCLK(ABIST, 44, "msdc02_in_ck"),
	FMCLK(ABIST, 45, "msdc11_in_ck"),
	FMCLK(ABIST, 46, "msdc12_in_ck"),
	FMCLK(ABIST, 50, "AD_MPLL_208M_CK"),
	FMCLK(ABIST, 54, "DA_USB20_48M_DIV_CK"),
	FMCLK(ABIST, 55, "DA_UNIV_48M_DIV_CK"),
	{}
};


#define PLL_HP_CON0			(rb[apmixed].virt + 0x014)
#define PLL_TEST_CON1			(rb[apmixed].virt + 0x064)
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

#if 0 /*use other function*/
static u32 fmeter_freq(enum FMETER_TYPE type, int k1, int clk)
{
	u32 cnt = 0;

	/* reset & reset deassert */
	clk_writel(FREQ_MTR_CTRL_REG, RG_FQMTR_MONCLK_RST_SET(RG_FQMTR_RST));
	clk_writel(FREQ_MTR_CTRL_REG, RG_FQMTR_MONCLK_RST_SET(!RG_FQMTR_RST));

	/* set window and target */
	clk_writel(FREQ_MTR_CTRL_REG,
		RG_FQMTR_MONCLK_WINDOW_SET(RG_FRMTR_WINDOW) |
		RG_FQMTR_MONCLK_SEL_SET(clk) |
		RG_FQMTR_FIXCLK_SEL_SET(RG_FQMTR_FIXCLK_26MHZ) |
		RG_FQMTR_MONCLK_EN_SET(RG_FQMTR_EN));

	udelay(30);

	cnt = clk_readl(FREQ_MTR_CTRL_RDATA);
	/* reset & reset deassert */
	clk_writel(FREQ_MTR_CTRL_REG, RG_FQMTR_MONCLK_RST_SET(RG_FQMTR_RST));
	clk_writel(FREQ_MTR_CTRL_REG, RG_FQMTR_MONCLK_RST_SET(!RG_FQMTR_RST));

	return ((cnt * 26000) / (RG_FRMTR_WINDOW + 1));
}


static u32 measure_stable_fmeter_freq(enum FMETER_TYPE type, int k1, int clk)
{
	u32 last_freq = 0;
	u32 freq = fmeter_freq(type, k1, clk);
	u32 maxfreq = max(freq, last_freq);

	while (maxfreq > 0 && ABS_DIFF(freq, last_freq) * 100 / maxfreq > 10) {
		last_freq = freq;
		freq = fmeter_freq(type, k1, clk);
		maxfreq = max(freq, last_freq);
	}

	return freq;
}
#endif

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
		"mainpll",
		"univpll",
		"msdcpll",
		"mfgpll",
		"mmpll",
		"apll1",
		"axi_sel",
		"mem_sel",
		"ddrphycfg_sel",
		"mm_sel",
		"mfg_sel",
		"camtg_sel",
		"uart_sel",
		"spi_sel",
		"msdc50_0_hclk_sel",
		"msdc50_0_sel",
		"msdc30_1_sel",
		"audio_sel",
		"aud_intbus_sel",
		"dbi0_sel",
		"scam_sel",
		"aud_1_sel",
		"disp_pwm_sel",
		"nfi2x_sel",
		"nfiecc_sel",
		"usb_top_sel",
		"spm_sel",
		"i2c_sel",
		"senif_sel",
		"dxcc_sel",
		"camtg2_sel",
		"aud_engen1_sel",
		"PDN_AFE",
		"PDN_22M",
		"PDN_APLL_TUNER",
		"PDN_ADC",
		"PDN_DAC",
		"PDN_DAC_PREDIS",
		"PDN_TML",
		"I2S1_BCLK_SW_CG",
		"I2S2_BCLK_SW_CG",
		"I2S3_BCLK_SW_CG",
		"I2S4_BCLK_SW_CG",
		"LARB2_SMI_CKPDN",
		"CAM_SMI_CKPDN",
		"CAM_CAM_CKPDN",
		"SEN_TG_CKPDN",
		"SEN_CAM_CKPDN",
		"CAM_SV_CKPDN",
		"SUFOD_CKPDN",
		"FD_CKPDN",
		"PMIC_CG_TMR",
		"PMIC_CG_AP",
		"PMIC_CG_MD",
		"PMIC_CG_CONN",
		"SEJ_CG",
		"APXGPT_CG",
		"ICUSB_CG",
		"GCE_CG",
		"THERM_CG",
		"I2C0_CG",
		"I2C1_CG",
		"I2C2_CG",
		"I2C3_CG",
		"PWM_HCLK_CG",
		"PWM1_CG",
		"PWM2_CG",
		"PWM3_CG",
		"PWM4_CG",
		"PWM5_CG",
		"PWM_CG",
		"UART0_CG",
		"UART1_CG",
		"UART2_CG",
		"UART3_CG",
		"GCE_26M",
		"CQ_DMA_FPC",
		"BTIF_CG",
		"SPI0_CG",
		"MSDC0_CG",
		"MSDC1_CG",
		"NFIECC_312M_CG",
		"DVFSRC_CG",
		"GCPU_CG",
		"TRNG_CG",
		"AUXADC_CG",
		"CPUM_CG",
		"CCIF1_AP_CG",
		"CCIF1_MD_CG",
		"AUXADC_MD_CG",
		"NFI_CG",
		"NFI_1X_CG",
		"AP_DMA_CG",
		"XIU_CG",
		"DEVICE_APC_CG",
		"CCIF_AP_CG",
		"DEBUGSYS_CG",
		"AUDIO_CG",
		"CCIF_MD_CG",
		"DXCC_SEC_CORE_CG",
		"DXCC_AO_CG",
		"DRAMC_F26M_CG",
		"RG_PWM_FBCLK6_CK_CG",
		"DISP_PWM_CG",
		"CLDMA_BCLK_CK",
		"AUDIO_26M_BCLK_CK",
		"SPI1_CG",
		"I2C4_CG",
		"MODEM_TEMP_SHARE_CG",
		"SPI2_CG",
		"SPI3_CG",
		"I2C5_CG",
		"I2C5_ARBITER_CG",
		"I2C5_IMM_CG",
		"I2C1_ARBITER_CG",
		"I2C1_IMM_CG",
		"I2C2_ARBITER_CG",
		"I2C2_IMM_CG",
		"SPI4_CG",
		"SPI5_CG",
		"CQ_DMA_CG",
		"MSDC0_SELF_CG",
		"MSDC1_SELF_CG",
		"MSDC2_SELF_CG",
		"SSPM_26M_SELF_CG",
		"SSPM_32K_SELF_CG",
		"UFS_AXI_CG",
		"I2C6_CG",
		"AP_MSDC0_CG",
		"MD_MSDC0_CG",
		"MSDC0_SRC_CLK_CG",
		"MSDC1_SRC_CLK_CG",
		"MSDC2_SRC_CLK_CG",
		"SMI_COMMON",
		"SMI_LARB0",
		"GALS_COMM0",
		"GALS_COMM1",
		"ISP_DL",
		"MDP_RDMA0",
		"MDP_RSZ0",
		"MDP_RSZ1",
		"MDP_TDSHP",
		"MDP_WROT0",
		"MDP_WDMA0",
		"FAKE_ENG",
		"DISP_OVL0",
		"DISP_RDMA0",
		"DISP_WDMA0",
		"DISP_COLOR0",
		"DISP_CCORR0",
		"DISP_AAL0",
		"DISP_GAMMA0",
		"DISP_DITHER0",
		"DSI_MM_CLOCK",
		"DSI_INTERF",
		"DBI_MM_CLOCK",
		"DBI_INTERF",
		"F26M_HRT",
		"SET0_LARB",
		"SET1_VENC",
		"SET2_JPGENC",
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
		[0]  = "",
		[1]  = "MFG",
		[2]  = "MFG_CORE",
		[3]  = "MD1",
		[4]  = "CONN",
		[5]  = "MM0",
		[6]  = "ISP",
		[7]  = "VEN",
		[8]  = "DPHY",
		[9]  = "INFRA",
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
		[23] = "",
		[24] = "",
		[25] = "",
		[26] = "",
		[27] = "",
		[28] = "",
		[29] = "",
		[30] = "",
		[31] = "",
	};

	return pwr_names;
}

/*
 * clkdbg dump_clks
 */

void setup_provider_clk(struct provider_clk *pvdck)
{
	static const struct {
		const char *pvdname;
		u32 pwr_mask;
	} pvd_pwr_mask[] = {
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

	#if 0 /*no support*/
	enum chip_sw_ver ver = mt_get_chip_sw_ver();

	seq_printf(s, "mt_get_chip_sw_ver(): %d (%s)\n", ver, sw_ver_name[ver]);
	#else
	seq_printf(s, "mt_get_chip_sw_ver(): %d (%s)\n", 0, sw_ver_name[0]);
	#endif

	return 0;
}

/*
 * init functions
 */

static struct clkdbg_ops clkdbg_mt6739_ops = {
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

static int __init clkdbg_mt6739_init(void)
{
	if (!of_machine_is_compatible("mediatek,mt6739"))
		return -ENODEV;

	init_regbase();

	init_custom_cmds();
	set_clkdbg_ops(&clkdbg_mt6739_ops);

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
device_initcall(clkdbg_mt6739_init);
