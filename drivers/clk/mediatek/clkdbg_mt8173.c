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

#define ALL_CLK_ON		0
#define DUMP_INIT_STATE		0

/*
 * clkdbg dump_regs
 */

enum {
	topckgen,
	infrasys,
	perisys,
	scpsys,
	mcucfg,
	apmixed,
	audiosys,
	mfgsys,
	mmsys,
	imgsys,
	vdecsys,
	vencsys,
	vencltsys,
};

#define REGBASE_V(_phys, _id_name) { .phys = _phys, .name = #_id_name }

/*
 * checkpatch.pl ERROR:COMPLEX_MACRO
 *
 * #define REGBASE(_phys, _id_name) [_id_name] = REGBASE_V(_phys, _id_name)
 */

static struct regbase rb[] = {
	[topckgen]  = REGBASE_V(0x10000000, topckgen),
	[infrasys]  = REGBASE_V(0x10001000, infrasys),
	[perisys]   = REGBASE_V(0x10003000, perisys),
	[scpsys]    = REGBASE_V(0x10006000, scpsys),
	[mcucfg]    = REGBASE_V(0x10200000, mcucfg),
	[apmixed]   = REGBASE_V(0x10209000, apmixed),
	[audiosys]  = REGBASE_V(0x11220000, audiosys),
	[mfgsys]    = REGBASE_V(0x13fff000, mfgsys),
	[mmsys]     = REGBASE_V(0x14000000, mmsys),
	[imgsys]    = REGBASE_V(0x15000000, imgsys),
	[vdecsys]   = REGBASE_V(0x16000000, vdecsys),
	[vencsys]   = REGBASE_V(0x18000000, vencsys),
	[vencltsys] = REGBASE_V(0x19000000, vencltsys),
};

#define REGNAME(_base, _ofs, _name)	\
	{ .base = &rb[_base], .ofs = _ofs, .name = #_name }

static struct regname rn[] = {
	REGNAME(topckgen, 0x040, CLK_CFG_0),
	REGNAME(topckgen, 0x050, CLK_CFG_1),
	REGNAME(topckgen, 0x060, CLK_CFG_2),
	REGNAME(topckgen, 0x070, CLK_CFG_3),
	REGNAME(topckgen, 0x080, CLK_CFG_4),
	REGNAME(topckgen, 0x090, CLK_CFG_5),
	REGNAME(topckgen, 0x0a0, CLK_CFG_6),
	REGNAME(topckgen, 0x0b0, CLK_CFG_7),
	REGNAME(topckgen, 0x100, CLK_CFG_8),
	REGNAME(topckgen, 0x104, CLK_CFG_9),
	REGNAME(topckgen, 0x108, CLK_CFG_10),
	REGNAME(topckgen, 0x10c, CLK_CFG_11),
	REGNAME(topckgen, 0x0c0, CLK_CFG_12),
	REGNAME(topckgen, 0x0d0, CLK_CFG_13),
	REGNAME(topckgen, 0x214, CLK_MISC_CFG_1),
	REGNAME(topckgen, 0x218, CLK_MISC_CFG_2),
	REGNAME(topckgen, 0x220, CLK26CALI_0),
	REGNAME(topckgen, 0x224, CLK26CALI_1),
	REGNAME(topckgen, 0x228, CLK26CALI_2),
	REGNAME(infrasys, 0x010, INFRA_TOPCKGEN_DCMCTL),
	REGNAME(infrasys, 0x040, INFRA_PDN_SET),
	REGNAME(infrasys, 0x044, INFRA_PDN_CLR),
	REGNAME(infrasys, 0x048, INFRA_PDN_STA),
	REGNAME(infrasys, 0x220, TOPAXI_PROT_EN),
	REGNAME(infrasys, 0x228, TOPAXI_PROT_STA1),
	REGNAME(infrasys, 0x250, TOPAXI_PROT_EN1),
	REGNAME(infrasys, 0x258, TOPAXI_PROT_STA3),
	REGNAME(perisys, 0x008, PERI_PDN0_SET),
	REGNAME(perisys, 0x010, PERI_PDN0_CLR),
	REGNAME(perisys, 0x00C, PERI_PDN1_SET),
	REGNAME(perisys, 0x014, PERI_PDN1_CLR),
	REGNAME(perisys, 0x018, PERI_PDN0_STA),
	REGNAME(perisys, 0x01C, PERI_PDN1_STA),
	REGNAME(scpsys, 0x210, SPM_VDE_PWR_CON),
	REGNAME(scpsys, 0x214, SPM_MFG_PWR_CON),
	REGNAME(scpsys, 0x230, SPM_VEN_PWR_CON),
	REGNAME(scpsys, 0x238, SPM_ISP_PWR_CON),
	REGNAME(scpsys, 0x23c, SPM_DIS_PWR_CON),
	REGNAME(scpsys, 0x298, SPM_VEN2_PWR_CON),
	REGNAME(scpsys, 0x29c, SPM_AUDIO_PWR_CON),
	REGNAME(scpsys, 0x2c0, SPM_MFG_2D_PWR_CON),
	REGNAME(scpsys, 0x2c4, SPM_MFG_ASYNC_PWR_CON),
	REGNAME(scpsys, 0x2cc, SPM_USB_PWR_CON),
	REGNAME(scpsys, 0x60c, SPM_PWR_STATUS),
	REGNAME(scpsys, 0x610, SPM_PWR_STATUS_2ND),
	REGNAME(mcucfg, 0x26c, MCU_26C),
	REGNAME(mcucfg, 0x64c, ARMPLL_JIT_CTRL),
	REGNAME(apmixed, 0x00c, AP_PLL_CON3),
	REGNAME(apmixed, 0x010, AP_PLL_CON4),
	REGNAME(apmixed, 0x200, ARMCA15PLL_CON0),
	REGNAME(apmixed, 0x204, ARMCA15PLL_CON1),
	REGNAME(apmixed, 0x20c, ARMCA15PLL_PWR_CON0),
	REGNAME(apmixed, 0x210, ARMCA7PLL_CON0),
	REGNAME(apmixed, 0x214, ARMCA7PLL_CON1),
	REGNAME(apmixed, 0x21c, ARMCA7PLL_PWR_CON0),
	REGNAME(apmixed, 0x220, MAINPLL_CON0),
	REGNAME(apmixed, 0x224, MAINPLL_CON1),
	REGNAME(apmixed, 0x22c, MAINPLL_PWR_CON0),
	REGNAME(apmixed, 0x230, UNIVPLL_CON0),
	REGNAME(apmixed, 0x234, UNIVPLL_CON1),
	REGNAME(apmixed, 0x23c, UNIVPLL_PWR_CON0),
	REGNAME(apmixed, 0x240, MMPLL_CON0),
	REGNAME(apmixed, 0x244, MMPLL_CON1),
	REGNAME(apmixed, 0x24c, MMPLL_PWR_CON0),
	REGNAME(apmixed, 0x250, MSDCPLL_CON0),
	REGNAME(apmixed, 0x254, MSDCPLL_CON1),
	REGNAME(apmixed, 0x25c, MSDCPLL_PWR_CON0),
	REGNAME(apmixed, 0x260, VENCPLL_CON0),
	REGNAME(apmixed, 0x264, VENCPLL_CON1),
	REGNAME(apmixed, 0x26c, VENCPLL_PWR_CON0),
	REGNAME(apmixed, 0x270, TVDPLL_CON0),
	REGNAME(apmixed, 0x274, TVDPLL_CON1),
	REGNAME(apmixed, 0x27c, TVDPLL_PWR_CON0),
	REGNAME(apmixed, 0x280, MPLL_CON0),
	REGNAME(apmixed, 0x284, MPLL_CON1),
	REGNAME(apmixed, 0x28c, MPLL_PWR_CON0),
	REGNAME(apmixed, 0x290, VCODECPLL_CON0),
	REGNAME(apmixed, 0x294, VCODECPLL_CON1),
	REGNAME(apmixed, 0x29c, VCODECPLL_PWR_CON0),
	REGNAME(apmixed, 0x2a0, APLL1_CON0),
	REGNAME(apmixed, 0x2a4, APLL1_CON1),
	REGNAME(apmixed, 0x2b0, APLL1_PWR_CON0),
	REGNAME(apmixed, 0x2b4, APLL2_CON0),
	REGNAME(apmixed, 0x2b8, APLL2_CON1),
	REGNAME(apmixed, 0x2c4, APLL2_PWR_CON0),
	REGNAME(apmixed, 0x2d0, LVDSPLL_CON0),
	REGNAME(apmixed, 0x2d4, LVDSPLL_CON1),
	REGNAME(apmixed, 0x2dc, LVDSPLL_PWR_CON0),
	REGNAME(apmixed, 0x2f0, MSDCPLL2_CON0),
	REGNAME(apmixed, 0x2f4, MSDCPLL2_CON1),
	REGNAME(apmixed, 0x2fc, MSDCPLL2_PWR_CON0),
	REGNAME(apmixed, 0xf00, PLL_HP_CON0),
	REGNAME(audiosys, 0x000, AUDIO_TOP_CON0),
	REGNAME(mfgsys, 0x000, MFG_CG_CON),
	REGNAME(mfgsys, 0x004, MFG_CG_SET),
	REGNAME(mfgsys, 0x008, MFG_CG_CLR),
	REGNAME(mmsys, 0x100, DISP_CG_CON0),
	REGNAME(mmsys, 0x104, DISP_CG_SET0),
	REGNAME(mmsys, 0x108, DISP_CG_CLR0),
	REGNAME(mmsys, 0x110, DISP_CG_CON1),
	REGNAME(mmsys, 0x114, DISP_CG_SET1),
	REGNAME(mmsys, 0x118, DISP_CG_CLR1),
	REGNAME(imgsys, 0x000, IMG_CG_CON),
	REGNAME(imgsys, 0x004, IMG_CG_SET),
	REGNAME(imgsys, 0x008, IMG_CG_CLR),
	REGNAME(vdecsys, 0x000, VDEC_CKEN_SET),
	REGNAME(vdecsys, 0x004, VDEC_CKEN_CLR),
	REGNAME(vdecsys, 0x008, LARB_CKEN_SET),
	REGNAME(vdecsys, 0x00c, LARB_CKEN_CLR),
	REGNAME(vencsys, 0x000, VENC_CG_CON),
	REGNAME(vencsys, 0x004, VENC_CG_SET),
	REGNAME(vencsys, 0x008, VENC_CG_CLR),
	REGNAME(vencltsys, 0x000, VENCLT_CG_CON),
	REGNAME(vencltsys, 0x004, VENCLT_CG_SET),
	REGNAME(vencltsys, 0x008, VENCLT_CG_CLR),
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

#define CLK_CFG_8		(rb[topckgen].virt + 0x100)
#define CLK_CFG_9		(rb[topckgen].virt + 0x104)
#define CLK_MISC_CFG_1		(rb[topckgen].virt + 0x214)
#define CLK_MISC_CFG_2		(rb[topckgen].virt + 0x218)
#define CLK26CALI_0		(rb[topckgen].virt + 0x220)
#define CLK26CALI_1		(rb[topckgen].virt + 0x224)
#define CLK26CALI_2		(rb[topckgen].virt + 0x228)
#define INFRA_TOPCKGEN_DCMCTL	(rb[infrasys].virt + 0x0010)
#define MCU_26C			(rb[mcucfg].virt + 0x26c)
#define ARMPLL_JIT_CTRL		(rb[mcucfg].virt + 0x64c)
#define PLL_HP_CON0		(rb[apmixed].virt + 0xf00)

enum FMETER_TYPE {
	FT_NULL,
	ABIST,
	CKGEN
};

#define FMCLK(_t, _i, _n) { .type = _t, .id = _i, .name = _n }

static const struct fmeter_clk fclks[] = {
	FMCLK(ABIST, 98, "CA53"),
	FMCLK(ABIST, 99, "CA57"),
	FMCLK(ABIST,  1, "AD_MEMPLL2_CKOUT0_PRE_ISO"),
	FMCLK(ABIST,  2, "AD_ARMCA7PLL_754M_CORE_CK"),
	FMCLK(ABIST,  3, "AD_ARMCA7PLL_502M_CORE_CK"),
	FMCLK(ABIST,  4, "AD_MAIN_H546M_CK"),
	FMCLK(ABIST,  5, "AD_MAIN_H364M_CK"),
	FMCLK(ABIST,  6, "AD_MAIN_H218P4M_CK"),
	FMCLK(ABIST,  7, "AD_MAIN_H156M_CK"),
	FMCLK(ABIST,  8, "AD_UNIV_178P3M_CK"),
	FMCLK(ABIST,  9, "AD_UNIV_48M_CK"),
	FMCLK(ABIST, 10, "AD_UNIV_624M_CK"),
	FMCLK(ABIST, 11, "AD_UNIV_416M_CK"),
	FMCLK(ABIST, 12, "AD_UNIV_249P6M_CK"),
	FMCLK(ABIST, 13, "AD_APLL1_180P6336M_CK"),
	FMCLK(ABIST, 14, "AD_APLL2_196P608M_CK"),
	FMCLK(ABIST, 15, "AD_LTEPLL_FS26M_CK"),
	FMCLK(ABIST, 16, "rtc32k_ck_i"),
	FMCLK(ABIST, 17, "AD_MMPLL_455M_CK"),
	FMCLK(ABIST, 18, "AD_VENCPLL_760M_CK"),
	FMCLK(ABIST, 19, "AD_VCODEPLL_494M_CK"),
	FMCLK(ABIST, 20, "AD_TVDPLL_594M_CK"),
	FMCLK(ABIST, 21, "AD_SSUSB_SYSPLL_125M_CK"),
	FMCLK(ABIST, 22, "AD_MSDCPLL_806M_CK"),
	FMCLK(ABIST, 23, "AD_DPICLK"),
	FMCLK(ABIST, 24, "clkph_MCK_o"),
	FMCLK(ABIST, 25, "AD_USB_48M_CK"),
	FMCLK(ABIST, 26, "AD_MSDCPLL2_CK"),
	FMCLK(ABIST, 27, "AD_VCODECPLL_370P5M_CK"),
	FMCLK(ABIST, 30, "AD_TVDPLL_445P5M_CK"),
	FMCLK(ABIST, 31, "soc_rosc"),
	FMCLK(ABIST, 32, "soc_rosc1"),
	FMCLK(ABIST, 33, "AD_HDMITX_CLKDIG_CTS_D4_debug"),
	FMCLK(ABIST, 34, "AD_HDMITX_MON_CK_D4_debug"),
	FMCLK(ABIST, 35, "AD_MIPI_26M_CK"),
	FMCLK(ABIST, 36, "AD_LTEPLL_ARMPLL26M_CK"),
	FMCLK(ABIST, 37, "AD_LETPLL_SSUSB26M_CK"),
	FMCLK(ABIST, 38, "AD_HDMITX_MON_CK"),
	FMCLK(ABIST, 39, "AD_MPLL_26M_26M_CK_CKSYS"),
	FMCLK(ABIST, 40, "HSIC_AD_480M_CK"),
	FMCLK(ABIST, 41, "AD_HDMITX_REF_CK_CKSYS"),
	FMCLK(ABIST, 42, "AD_HDMITX_CLKDIG_CTS_CKSYS"),
	FMCLK(ABIST, 43, "AD_PLLGP_TST_CK"),
	FMCLK(ABIST, 44, "AD_SSUSB_48M_CJ"),
	FMCLK(ABIST, 45, "AD_MEM_26M_CK"),
	FMCLK(ABIST, 46, "armpll_occ_mon"),
	FMCLK(ABIST, 47, "AD_ARMPLLGP_TST_CK"),
	FMCLK(ABIST, 48, "AD_MEMPLL_MONCLK"),
	FMCLK(ABIST, 49, "AD_MEMPLL2_MONCLK"),
	FMCLK(ABIST, 50, "AD_MEMPLL3_MONCLK"),
	FMCLK(ABIST, 51, "AD_MEMPLL4_MONCLK"),
	FMCLK(ABIST, 52, "AD_MEMPLL_REFCLK"),
	FMCLK(ABIST, 53, "AD_MEMPLL_FBCLK"),
	FMCLK(ABIST, 54, "AD_MEMPLL2_REFCLK"),
	FMCLK(ABIST, 55, "AD_MEMPLL2_FBCLK"),
	FMCLK(ABIST, 56, "AD_MEMPLL3_REFCLK"),
	FMCLK(ABIST, 57, "AD_MEMPLL3_FBCLK"),
	FMCLK(ABIST, 58, "AD_MEMPLL4_REFCLK"),
	FMCLK(ABIST, 59, "AD_MEMPLL4_FBCLK"),
	FMCLK(ABIST, 60, "AD_MEMPLL_TSTDIV2_CK"),
	FMCLK(ABIST, 61, "AD_MEM2MIPI_26M_CK_"),
	FMCLK(ABIST, 62, "AD_MEMPLL_MONCLK_B"),
	FMCLK(ABIST, 63, "AD_MEMPLL2_MONCLK_B"),
	FMCLK(ABIST, 65, "AD_MEMPLL3_MONCLK_B"),
	FMCLK(ABIST, 66, "AD_MEMPLL4_MONCLK_B"),
	FMCLK(ABIST, 67, "AD_MEMPLL_REFCLK_B"),
	FMCLK(ABIST, 68, "AD_MEMPLL_FBCLK_B"),
	FMCLK(ABIST, 69, "AD_MEMPLL2_REFCLK_B"),
	FMCLK(ABIST, 70, "AD_MEMPLL2_FBCLK_B"),
	FMCLK(ABIST, 71, "AD_MEMPLL3_REFCLK_B"),
	FMCLK(ABIST, 72, "AD_MEMPLL3_FBCLK_B"),
	FMCLK(ABIST, 73, "AD_MEMPLL4_REFCLK_B"),
	FMCLK(ABIST, 74, "AD_MEMPLL4_FBCLK_B"),
	FMCLK(ABIST, 75, "AD_MEMPLL_TSTDIV2_CK_B"),
	FMCLK(ABIST, 76, "AD_MEM2MIPI_26M_CK_B_"),
	FMCLK(ABIST, 77, "AD_LVDSTX1_MONCLK"),
	FMCLK(ABIST, 78, "AD_MONREF1_CK"),
	FMCLK(ABIST, 79, "AD_MONFBK1"),
	FMCLK(ABIST, 91, "AD_DSI0_LNTC_DSICLK"),
	FMCLK(ABIST, 92, "AD_DSI0_MPPLL_TST_CK"),
	FMCLK(ABIST, 93, "AD_DSI1_LNTC_DSICLK"),
	FMCLK(ABIST, 94, "AD_DSI1_MPPLL_TST_CK"),
	FMCLK(ABIST, 95, "AD_CR4_249P2M_CK"),
	FMCLK(CKGEN,  1, "hf_faxi_ck"),
	FMCLK(CKGEN,  2, "hd_faxi_ck"),
	FMCLK(CKGEN,  3, "hf_fdpi0_ck"),
	FMCLK(CKGEN,  4, "hf_fddrphycfg_ck"),
	FMCLK(CKGEN,  5, "hf_fmm_ck"),
	FMCLK(CKGEN,  6, "f_fpwm_ck"),
	FMCLK(CKGEN,  7, "hf_fvdec_ck"),
	FMCLK(CKGEN,  8, "hf_fvenc_ck"),
	FMCLK(CKGEN,  9, "hf_fmfg_ck"),
	FMCLK(CKGEN, 10, "hf_fcamtg_ck"),
	FMCLK(CKGEN, 11, "f_fuart_ck"),
	FMCLK(CKGEN, 12, "hf_fspi_ck"),
	FMCLK(CKGEN, 13, "f_fusb20_ck"),
	FMCLK(CKGEN, 14, "f_fusb30_ck"),
	FMCLK(CKGEN, 15, "hf_fmsdc50_0_hclk_ck"),
	FMCLK(CKGEN, 16, "hf_fmsdc50_0_ck"),
	FMCLK(CKGEN, 17, "hf_fmsdc30_1_ck"),
	FMCLK(CKGEN, 18, "hf_fmsdc30_2_ck"),
	FMCLK(CKGEN, 19, "hf_fmsdc30_3_ck"),
	FMCLK(CKGEN, 20, "hf_faudio_ck"),
	FMCLK(CKGEN, 21, "hf_faud_intbus_ck"),
	FMCLK(CKGEN, 22, "hf_fpmicspi_ck"),
	FMCLK(CKGEN, 23, "hf_fscp_ck"),
	FMCLK(CKGEN, 24, "hf_fatb_ck"),
	FMCLK(CKGEN, 25, "hf_fvenc_lt_ck"),
	FMCLK(CKGEN, 26, "hf_firda_ck"),
	FMCLK(CKGEN, 27, "hf_fcci400_ck"),
	FMCLK(CKGEN, 28, "hf_faud_1_ck"),
	FMCLK(CKGEN, 29, "hf_faud_2_ck"),
	FMCLK(CKGEN, 30, "hf_fmem_mfg_in_as_ck"),
	FMCLK(CKGEN, 31, "hf_faxi_mfg_in_as_ck"),
	FMCLK(CKGEN, 32, "f_frtc_ck"),
	FMCLK(CKGEN, 33, "f_f26m_ck"),
	FMCLK(CKGEN, 34, "f_f32k_md1_ck"),
	FMCLK(CKGEN, 35, "f_frtc_conn_ck"),
	FMCLK(CKGEN, 36, "hg_fmipicfg_ck"),
	FMCLK(CKGEN, 37, "hd_haxi_nli_ck"),
	FMCLK(CKGEN, 38, "hd_qaxidcm_ck"),
	FMCLK(CKGEN, 39, "f_ffpc_ck"),
	FMCLK(CKGEN, 40, "f_fckbus_ck_scan"),
	FMCLK(CKGEN, 41, "f_fckrtc_ck_scan"),
	FMCLK(CKGEN, 42, "hf_flvds_pxl_ck"),
	FMCLK(CKGEN, 43, "hf_flvds_cts_ck"),
	FMCLK(CKGEN, 44, "hf_fdpilvds_ck"),
	FMCLK(CKGEN, 45, "hf_fspinfi_infra_bclk_ck"),
	FMCLK(CKGEN, 46, "hf_fhdmi_ck"),
	FMCLK(CKGEN, 47, "hf_fhdcp_ck"),
	FMCLK(CKGEN, 48, "hf_fmsdc50_3_hclk_ck"),
	FMCLK(CKGEN, 49, "hf_fhdcp_24m_ck"),
	{}
};

static void set_fmeter_divider_ca7(u32 k1)
{
	u32 v = clk_readl(CLK_MISC_CFG_1);

	v = ALT_BITS(v, 15, 8, k1);
	clk_writel(CLK_MISC_CFG_1, v);
}

static void set_fmeter_divider_ca15(u32 k1)
{
	u32 v = clk_readl(CLK_MISC_CFG_2);

	v = ALT_BITS(v, 7, 0, k1);
	clk_writel(CLK_MISC_CFG_2, v);
}

static void set_fmeter_divider(u32 k1)
{
	u32 v = clk_readl(CLK_MISC_CFG_1);

	v = ALT_BITS(v, 7, 0, k1);
	v = ALT_BITS(v, 31, 24, k1);
	clk_writel(CLK_MISC_CFG_1, v);
}

static bool wait_fmeter_done(u32 tri_bit)
{
	static int max_wait_count;
	int wait_count = (max_wait_count > 0) ? (max_wait_count * 2 + 2) : 100;
	int i;

	/* wait fmeter */
	for (i = 0; i < wait_count && (clk_readl(CLK26CALI_0) & tri_bit); i++)
		udelay(20);

	if (!(clk_readl(CLK26CALI_0) & tri_bit)) {
		max_wait_count = max(max_wait_count, i);
		return true;
	}

	return false;
}

static u32 fmeter_freq(enum FMETER_TYPE type, int k1, int clk)
{
	u32 cksw_ckgen[] = {21, 16, clk};
	u32 cksw_abist[] = {14, 8, clk};
	void __iomem *clk_cfg_reg =
				(type == CKGEN) ? CLK_CFG_9	: CLK_CFG_8;
	void __iomem *cnt_reg =	(type == CKGEN) ? CLK26CALI_2	: CLK26CALI_1;
	u32 *cksw_hlv =		(type == CKGEN) ? cksw_ckgen	: cksw_abist;
	u32 tri_bit =		(type == CKGEN) ? BIT(4)	: BIT(0);
	u32 clk_exc =		(type == CKGEN) ? BIT(5)	: BIT(2);

	u32 clk_misc_cfg_1;
	u32 clk_misc_cfg_2;
	u32 clk_cfg_val;
	u32 freq = 0;

	/* setup fmeter */
	clk_setl(CLK26CALI_0, BIT(7));			/* enable fmeter_en */
	clk_clrl(CLK26CALI_0, clk_exc);			/* set clk_exc */

	/* load_cnt = 1023 */
	clk_writel_mask(cnt_reg, 25, 16, 1023);

	clk_misc_cfg_1 = clk_readl(CLK_MISC_CFG_1);	/* backup reg value */
	clk_misc_cfg_2 = clk_readl(CLK_MISC_CFG_2);
	clk_cfg_val = clk_readl(clk_cfg_reg);

	set_fmeter_divider(k1);				/* set div (0 = /1) */
	set_fmeter_divider_ca7(k1);
	set_fmeter_divider_ca15(k1);
	clk_writel_mask(clk_cfg_reg, cksw_hlv[0], cksw_hlv[1], cksw_hlv[2]);

	clk_setl(CLK26CALI_0, tri_bit);			/* start fmeter */

	if (wait_fmeter_done(tri_bit)) {
		u32 cnt = clk_readl(cnt_reg) & 0xFFFF;

		/* freq = counter * 26M / 1024 (KHz) */
		freq = (cnt * 26000) * (k1 + 1) / 1024;
	}

	/* restore register settings */
	clk_writel(clk_cfg_reg, clk_cfg_val);
	clk_writel(CLK_MISC_CFG_2, clk_misc_cfg_2);
	clk_writel(CLK_MISC_CFG_1, clk_misc_cfg_1);

	return freq;
}

static u32 measure_stable_fmeter_freq(enum FMETER_TYPE type, int k1, int clk)
{
	u32 last_freq = 0;
	u32 freq;
	u32 maxfreq;

	freq = fmeter_freq(type, k1, clk);
	maxfreq = max(freq, last_freq);

	while (maxfreq > 0 && ABS_DIFF(freq, last_freq) * 100 / maxfreq > 10) {
		last_freq = freq;
		freq = fmeter_freq(type, k1, clk);
		maxfreq = max(freq, last_freq);
	}

	return freq;
}

static u32 measure_armpll_freq(u32 jit_ctrl)
{
	u32 freq;
	u32 mcu26c;
	u32 armpll_jit_ctrl;
	u32 top_dcmctl;

	mcu26c = clk_readl(MCU_26C);
	armpll_jit_ctrl = clk_readl(ARMPLL_JIT_CTRL);
	top_dcmctl = clk_readl(INFRA_TOPCKGEN_DCMCTL);

	clk_setl(MCU_26C, 0x8);
	clk_setl(ARMPLL_JIT_CTRL, jit_ctrl);
	clk_clrl(INFRA_TOPCKGEN_DCMCTL, 0x700);

	freq = measure_stable_fmeter_freq(ABIST, 1, 46 /* ARMPLL_OCC_MON */);

	clk_writel(INFRA_TOPCKGEN_DCMCTL, top_dcmctl);
	clk_writel(ARMPLL_JIT_CTRL, armpll_jit_ctrl);
	clk_writel(MCU_26C, mcu26c);

	return freq;
}

static u32 measure_ca53_freq(void)
{
	return measure_armpll_freq(0x01);
}

static u32 measure_ca57_freq(void)
{
	return measure_armpll_freq(0x11);
}

static const struct fmeter_clk *get_all_fmeter_clks(void)
{
	return fclks;
}

static void *prepare_fmeter(void)
{
	static u32 old_pll_hp_con0;

	old_pll_hp_con0 = clk_readl(PLL_HP_CON0);
	clk_writel(PLL_HP_CON0, 0x0);		/* disable PLL hopping */
	udelay(10);

	return &old_pll_hp_con0;
}

static void unprepare_fmeter(void *data)
{
	u32 old_pll_hp_con0 = *(u32 *)data;

	/* restore old setting */
	clk_writel(PLL_HP_CON0, old_pll_hp_con0);
}

static u32 fmeter_freq_op(const struct fmeter_clk *fclk)
{
	if (!fclk->type)
		return 0;

	if (fclk->id == 98)
		return measure_ca53_freq();
	else if (fclk->id == 99)
		return measure_ca57_freq();

	return measure_stable_fmeter_freq(fclk->type, 0, fclk->id);
}

/*
 * clkdbg dump_state
 */

static const char * const *get_all_clk_names(void)
{
	static const char * const clks[] = {
		/* ROOT */
		"clk_null",
		"clk26m",
		"clk32k",
		"clkph_mck_o",
		"usb_syspll_125m",
		"hdmitx_dig_cts",
		/* PLL */
		"armca15pll",
		"armca7pll",
		"mainpll",
		"univpll",
		"mmpll",
		"msdcpll",
		"vencpll",
		"tvdpll",
		"mpll",
		"vcodecpll",
		"apll1",
		"apll2",
		"lvdspll",
		"msdcpll2",
		"ref2usb_tx",
		"armca7pll_754m",
		"armca7pll_502m",
		"main_h546m",
		"main_h364m",
		"main_h218p4m",
		"main_h156m",
		"tvdpll_445p5m",
		"tvdpll_594m",
		"univ_624m",
		"univ_416m",
		"univ_249p6m",
		"univ_178p3m",
		"univ_48m",
		"vcodecpll_370p5",
		/* DIV */
		"clkrtc_ext",
		"clkrtc_int",
		"fpc_ck",
		"hdmitxpll_d2",
		"hdmitxpll_d3",
		"armca7pll_d2",
		"armca7pll_d3",
		"apll1_ck",
		"apll2_ck",
		"dmpll_ck",
		"dmpll_d2",
		"dmpll_d4",
		"dmpll_d8",
		"dmpll_d16",
		"lvdspll_d2",
		"lvdspll_d4",
		"lvdspll_d8",
		"mmpll_ck",
		"mmpll_d2",
		"msdcpll_ck",
		"msdcpll_d2",
		"msdcpll_d4",
		"msdcpll2_ck",
		"msdcpll2_d2",
		"msdcpll2_d4",
		"syspll_d2",
		"syspll1_d2",
		"syspll1_d4",
		"syspll1_d8",
		"syspll1_d16",
		"syspll_d3",
		"syspll2_d2",
		"syspll2_d4",
		"syspll_d5",
		"syspll3_d2",
		"syspll3_d4",
		"syspll_d7",
		"syspll4_d2",
		"syspll4_d4",
		"tvdpll_ck",
		"tvdpll_d2",
		"tvdpll_d4",
		"tvdpll_d8",
		"tvdpll_d16",
		"univpll_d2",
		"univpll1_d2",
		"univpll1_d4",
		"univpll1_d8",
		"univpll_d3",
		"univpll2_d2",
		"univpll2_d4",
		"univpll2_d8",
		"univpll_d5",
		"univpll3_d2",
		"univpll3_d4",
		"univpll3_d8",
		"univpll_d7",
		"univpll_d26",
		"univpll_d52",
		"vcodecpll_ck",
		"vencpll_ck",
		"vencpll_d2",
		"vencpll_d4",
		/* TOP */
		"axi_sel",
		"mem_sel",
		"ddrphycfg_sel",
		"mm_sel",
		"pwm_sel",
		"vdec_sel",
		"venc_sel",
		"mfg_sel",
		"camtg_sel",
		"uart_sel",
		"spi_sel",
		"usb20_sel",
		"usb30_sel",
		"msdc50_0_h_sel",
		"msdc50_0_sel",
		"msdc30_1_sel",
		"msdc30_2_sel",
		"msdc30_3_sel",
		"audio_sel",
		"aud_intbus_sel",
		"pmicspi_sel",
		"scp_sel",
		"atb_sel",
		"venclt_sel",
		"dpi0_sel",
		"irda_sel",
		"cci400_sel",
		"aud_1_sel",
		"aud_2_sel",
		"mem_mfg_in_sel",
		"axi_mfg_in_sel",
		"scam_sel",
		"spinfi_ifr_sel",
		"hdmi_sel",
		"dpilvds_sel",
		"msdc50_2_h_sel",
		"hdcp_sel",
		"hdcp_24m_sel",
		"rtc_sel",
		/* INFRA */
		"infra_pmicwrap",
		"infra_pmicspi",
		"infra_cec",
		"infra_kp",
		"infra_cpum",
		"infra_m4u",
		"infra_l2c_sram",
		"infra_gce",
		"infra_audio",
		"infra_smi",
		"infra_dbgclk",
		/* PERI0 */
		"peri_nfiecc",
		"peri_i2c5",
		"peri_spi0",
		"peri_auxadc",
		"peri_i2c4",
		"peri_i2c3",
		"peri_i2c2",
		"peri_i2c1",
		"peri_i2c0",
		"peri_uart3",
		"peri_uart2",
		"peri_uart1",
		"peri_uart0",
		"peri_irda",
		"peri_nli_arb",
		"peri_msdc30_3",
		"peri_msdc30_2",
		"peri_msdc30_1",
		"peri_msdc30_0",
		"peri_ap_dma",
		"peri_usb1",
		"peri_usb0",
		"peri_pwm",
		"peri_pwm7",
		"peri_pwm6",
		"peri_pwm5",
		"peri_pwm4",
		"peri_pwm3",
		"peri_pwm2",
		"peri_pwm1",
		"peri_therm",
		"peri_nfi",
		/* PERI1 */
		"peri_i2c6",
		"peri_irrx",
		"peri_spi",
		/* IMG */
		"img_fd",
		"img_cam_sv",
		"img_sen_cam",
		"img_sen_tg",
		"img_cam_cam",
		"img_cam_smi",
		"img_larb2_smi",
		/* MM0 */
		"mm_smi_common",
		"mm_smi_larb0",
		"mm_cam_mdp",
		"mm_mdp_rdma0",
		"mm_mdp_rdma1",
		"mm_mdp_rsz0",
		"mm_mdp_rsz1",
		"mm_mdp_rsz2",
		"mm_mdp_tdshp0",
		"mm_mdp_tdshp1",
		"mm_mdp_wdma",
		"mm_mdp_wrot0",
		"mm_mdp_wrot1",
		"mm_fake_eng",
		"mm_mutex_32k",
		"mm_disp_ovl0",
		"mm_disp_ovl1",
		"mm_disp_rdma0",
		"mm_disp_rdma1",
		"mm_disp_rdma2",
		"mm_disp_wdma0",
		"mm_disp_wdma1",
		"mm_disp_color0",
		"mm_disp_color1",
		"mm_disp_aal",
		"mm_disp_gamma",
		"mm_disp_ufoe",
		"mm_disp_split0",
		"mm_disp_split1",
		"mm_disp_merge",
		"mm_disp_od",

		/* MM1 */
		"mm_disp_pwm0mm",
		"mm_disp_pwm026m",
		"mm_disp_pwm1mm",
		"mm_disp_pwm126m",
		"mm_dsi0_engine",
		"mm_dsi0_digital",
		"mm_dsi1_engine",
		"mm_dsi1_digital",
		"mm_dpi_pixel",
		"mm_dpi_engine",
		"mm_dpi1_pixel",
		"mm_dpi1_engine",
		"mm_hdmi_pixel",
		"mm_hdmi_pllck",
		"mm_hdmi_audio",
		"mm_hdmi_spdif",
		"mm_lvds_pixel",
		"mm_lvds_cts",
		"mm_smi_larb4",
		"mm_hdmi_hdcp",
		"mm_hdmi_hdcp24m",
		/* VDEC */
		"vdec_cken",
		"vdec_larb_cken",
		/* VENC */
		"venc_cke0",
		"venc_cke1",
		"venc_cke2",
		"venc_cke3",
		/* VENCLT */
		"venclt_cke0",
		"venclt_cke1",
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
		[0]  = "MD",
		[1]  = "",
		[2]  = "DDRPHY",
		[3]  = "DISP",
		[4]  = "MFG",
		[5]  = "ISP",
		[6]  = "INFRA",
		[7]  = "VDEC",
		[8]  = "CA7_CPUTOP",
		[9]  = "CA7_CPU0",
		[10] = "CA7_CPU1",
		[11] = "CA7_CPU2",
		[12] = "CA7_CPU3",
		[13] = "CA7_DBG",
		[14] = "MCUSYS",
		[15] = "CA15_CPUTOP",
		[16] = "CA15_CPU0",
		[17] = "CA15_CPU1",
		[18] = "CA15_CPU2",
		[19] = "CA15_CPU3",
		[20] = "VEN2",
		[21] = "VEN",
		[22] = "MFG_2D",
		[23] = "MFG_ASYNC",
		[24] = "AUDIO",
		[25] = "USB",
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
 * chip_id functions
 */

#include <linux/seq_file.h>

#define DEVINFO_MAX_SIZE 48

static u32 g_devinfo_data[DEVINFO_MAX_SIZE];

static u32 devinfo_get_size(void)
{
	u32 data_size = 0;

	data_size = ARRAY_SIZE(g_devinfo_data);
	return data_size;
}

static u32 get_devinfo_with_index(u32 index)
{
	int size = devinfo_get_size();
	u32 ret = 0;

	if ((index >= 0) && (index < size))
		ret = g_devinfo_data[index];
	else {
		pr_warn("devinfo data index out of range:%d\n", index);
		pr_warn("devinfo data size:%d\n", size);
		ret = 0xFFFFFFFF;
	}

	return ret;
}

#define C_UNKNOWN_CHIP_ID (0x0000FFFF)

enum chip_sw_ver {
	CHIP_SW_VER_01 = 0x0000,
	CHIP_SW_VER_02 = 0x0001,
	CHIP_SW_VER_03 = 0x0002,
	CHIP_SW_VER_04 = 0x0003,
};

static void __iomem *APHW_CHIPID;

enum {
	CID_UNINIT = 0,
	CID_INITIALIZING = 1,
	CID_INITIALIZED = 2,
};

static atomic_t g_cid_init = ATOMIC_INIT(CID_UNINIT);

static void init_chip_id(unsigned int line)
{
	if (atomic_read(&g_cid_init) == CID_INITIALIZED)
		return;

	if (atomic_read(&g_cid_init) == CID_INITIALIZING) {
		pr_warn("%s (%d) state(%d)\n", __func__, line,
			atomic_read(&g_cid_init));
		return;
	}

	atomic_set(&g_cid_init, CID_INITIALIZING);

	APHW_CHIPID = ioremap(0x08000000, PAGE_SIZE);
	atomic_set(&g_cid_init, CID_INITIALIZED);
}

static unsigned int __chip_sw_ver(void)
{
	return (APHW_CHIPID) ? readl(APHW_CHIPID + 12) : (C_UNKNOWN_CHIP_ID);
}

static unsigned int mt_get_chip_sw_ver(void)
{
	int chip_sw_ver;

	if (get_devinfo_with_index(39) & (1 << 1))
		return CHIP_SW_VER_04;

	if (get_devinfo_with_index(4) & (1 << 28))
		return CHIP_SW_VER_03;

	if (!APHW_CHIPID)
		init_chip_id(__LINE__);

	chip_sw_ver = __chip_sw_ver();

	if (chip_sw_ver != C_UNKNOWN_CHIP_ID)
		chip_sw_ver &= 0xf;

	return chip_sw_ver;
}

static int clkdbg_chip_ver(struct seq_file *s, void *v)
{
	static const char * const sw_ver_name[] = {
		"CHIP_SW_VER_01",
		"CHIP_SW_VER_02",
		"CHIP_SW_VER_03",
		"CHIP_SW_VER_04",
	};

	enum chip_sw_ver ver = mt_get_chip_sw_ver();

	seq_printf(s, "mt_get_chip_sw_ver(): %d (%s)\n", ver, sw_ver_name[ver]);

	return 0;
}

/*
 * init functions
 */

static void __init init_custom_cmds(void)
{
	static const struct cmd_fn cmds[] = {
		CMDFN("chip_ver", clkdbg_chip_ver),
		{}
	};

	set_custom_cmds(cmds);
}


static struct clkdbg_ops clkdbg_mt8173_ops = {
	.get_all_fmeter_clks = get_all_fmeter_clks,
	.prepare_fmeter = prepare_fmeter,
	.unprepare_fmeter = unprepare_fmeter,
	.fmeter_freq = fmeter_freq_op,
	.get_all_regnames = get_all_regnames,
	.get_all_clk_names = get_all_clk_names,
	.get_pwr_names = get_pwr_names,
};

static int __init clkdbg_mt8173_init(void)
{
	if (!of_machine_is_compatible("mediatek,mt8173"))
		return -ENODEV;

	init_regbase();

	init_custom_cmds();
	set_clkdbg_ops(&clkdbg_mt8173_ops);

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
device_initcall(clkdbg_mt8173_init);
