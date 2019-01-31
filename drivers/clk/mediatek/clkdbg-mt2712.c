/*
 * Copyright (c) 2017 MediaTek Inc.
 * Author: Weiyi Lu <weiyi.lu@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clk-provider.h>
#include <linux/io.h>

#include "clkdbg.h"

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
	fhctl,
	mfgsys,
	mmsys,
	imgsys,
	bdpsys,
	vdecsys,
	vencsys,
	jpgdecsys,
};

#define REGBASE_V(_phys, _id_name) { .phys = _phys, .name = #_id_name }

/*
 * checkpatch.pl ERROR:COMPLEX_MACRO
 *
 * #define REGBASE(_phys, _id_name) [_id_name] = REGBASE_V(_phys, _id_name)
 */

static struct regbase rb[] = {
	[topckgen]  = REGBASE_V(0x10000000, topckgen),
	[infracfg]  = REGBASE_V(0x10001000, infracfg),
	[pericfg]   = REGBASE_V(0x10003000, pericfg),
	[scpsys]    = REGBASE_V(0x10006000, scpsys),
	[apmixed]   = REGBASE_V(0x10209000, apmixed),
	[fhctl]     = REGBASE_V(0x10209e00, fhctl),
	[mfgsys]    = REGBASE_V(0x13000000, mfgsys),
	[mmsys]     = REGBASE_V(0x14000000, mmsys),
	[imgsys]    = REGBASE_V(0x15000000, imgsys),
	[bdpsys]    = REGBASE_V(0x15010000, bdpsys),
	[vdecsys]   = REGBASE_V(0x16000000, vdecsys),
	[vencsys]   = REGBASE_V(0x18000000, vencsys),
	[jpgdecsys] = REGBASE_V(0x19000000, jpgdecsys),
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
	REGNAME(topckgen, 0x0c0, CLK_CFG_8),
	REGNAME(topckgen, 0x0d0, CLK_CFG_9),
	REGNAME(topckgen, 0x134, CLK_AUDDIV_4),
	REGNAME(topckgen, 0x500, CLK_CFG_10),
	REGNAME(topckgen, 0x510, CLK_CFG_11),
	REGNAME(topckgen, 0x520, CLK_CFG_12),
	REGNAME(topckgen, 0x530, CLK_CFG_13),
	REGNAME(topckgen, 0x540, CLK_CFG_14),
	REGNAME(topckgen, 0x550, CLK_CFG_15),
	REGNAME(topckgen, 0x560, CLK_CFG_16),
	REGNAME(topckgen, 0x570, CLK_CFG_17),
	REGNAME(scpsys, 0x210, SPM_VDE_PWR_CON),
	REGNAME(scpsys, 0x214, SPM_MFG_PWR_CON),
	REGNAME(scpsys, 0x230, SPM_VEN_PWR_CON),
	REGNAME(scpsys, 0x238, SPM_ISP_PWR_CON),
	REGNAME(scpsys, 0x23c, SPM_DIS_PWR_CON),
	REGNAME(scpsys, 0x29c, SPM_AUDIO_PWR_CON),
	REGNAME(scpsys, 0x2cc, SPM_USB_PWR_CON),
	REGNAME(scpsys, 0x2d4, SPM_USB2_PWR_CON),
	REGNAME(scpsys, 0x60c, SPM_PWR_STATUS),
	REGNAME(scpsys, 0x610, SPM_PWR_STATUS_2ND),
	REGNAME(apmixed, 0x004, AP_PLL_CON1),
	REGNAME(apmixed, 0x008, AP_PLL_CON2),
	REGNAME(apmixed, 0x100, ARMCA35PLL_CON0),
	REGNAME(apmixed, 0x104, ARMCA35PLL_CON1),
	REGNAME(apmixed, 0x110, ARMCA35PLL_PWR_CON0),
	REGNAME(apmixed, 0x210, ARMCA72PLL_CON0),
	REGNAME(apmixed, 0x214, ARMCA72PLL_CON1),
	REGNAME(apmixed, 0x210, ARMCA72PLL_PWR_CON0),
	REGNAME(apmixed, 0x230, MAINPLL_CON0),
	REGNAME(apmixed, 0x234, MAINPLL_CON1),
	REGNAME(apmixed, 0x23c, MAINPLL_PWR_CON0),
	REGNAME(apmixed, 0x240, UNIVPLL_CON0),
	REGNAME(apmixed, 0x244, UNIVPLL_CON1),
	REGNAME(apmixed, 0x24c, UNIVPLL_PWR_CON0),
	REGNAME(apmixed, 0x250, MMPLL_CON0),
	REGNAME(apmixed, 0x254, MMPLL_CON1),
	REGNAME(apmixed, 0x260, MMPLL_PWR_CON0),
	REGNAME(apmixed, 0x270, MSDCPLL_CON0),
	REGNAME(apmixed, 0x274, MSDCPLL_CON1),
	REGNAME(apmixed, 0x27c, MSDCPLL_PWR_CON0),
	REGNAME(apmixed, 0x280, VENCPLL_CON0),
	REGNAME(apmixed, 0x284, VENCPLL_CON1),
	REGNAME(apmixed, 0x28c, VENCPLL_PWR_CON0),
	REGNAME(apmixed, 0x290, TVDPLL_CON0),
	REGNAME(apmixed, 0x294, TVDPLL_CON1),
	REGNAME(apmixed, 0x29c, TVDPLL_PWR_CON0),
	REGNAME(apmixed, 0x300, ETHERPLL_CON0),
	REGNAME(apmixed, 0x304, ETHERPLL_CON1),
	REGNAME(apmixed, 0x30c, ETHERPLL_PWR_CON0),
	REGNAME(apmixed, 0x320, VCODECPLL_CON0),
	REGNAME(apmixed, 0x324, VCODECPLL_CON1),
	REGNAME(apmixed, 0x32c, VCODECPLL_PWR_CON0),
	REGNAME(apmixed, 0x330, APLL1_CON0),
	REGNAME(apmixed, 0x334, APLL1_CON1),
	REGNAME(apmixed, 0x340, APLL1_PWR_CON0),
	REGNAME(apmixed, 0x350, APLL2_CON0),
	REGNAME(apmixed, 0x354, APLL2_CON1),
	REGNAME(apmixed, 0x360, APLL2_PWR_CON0),
	REGNAME(apmixed, 0x370, LVDSPLL_CON0),
	REGNAME(apmixed, 0x374, LVDSPLL_CON1),
	REGNAME(apmixed, 0x37c, LVDSPLL_PWR_CON0),
	REGNAME(apmixed, 0x390, LVDSPLL2_CON0),
	REGNAME(apmixed, 0x394, LVDSPLL2_CON1),
	REGNAME(apmixed, 0x39c, LVDSPLL2_PWR_CON0),
	REGNAME(apmixed, 0x410, MSDCPLL2_CON0),
	REGNAME(apmixed, 0x414, MSDCPLL2_CON1),
	REGNAME(apmixed, 0x41c, MSDCPLL2_PWR_CON0),
	REGNAME(topckgen, 0x120, CLK_AUDDIV_0),
	REGNAME(infracfg, 0x040, INFRA_PDN_STA),
	REGNAME(pericfg, 0x018, PERI_PDN0_STA),
	REGNAME(pericfg, 0x01c, PERI_PDN1_STA),
	REGNAME(pericfg, 0x42c, PERI_MSDC_CLK_EN),
	REGNAME(mfgsys, 0x000, MFG_CG_STA),
	REGNAME(mmsys, 0x100, MMSYS_CG0_STA),
	REGNAME(mmsys, 0x110, MMSYS_CG1_STA),
	REGNAME(mmsys, 0x220, MMSYS_CG2_STA),
	REGNAME(imgsys, 0x000, IMG_CG),
	REGNAME(bdpsys, 0x100, BDP_DISPSYS_CG_CON0),
	REGNAME(vdecsys, 0x000, VDEC_CKEN_SET),
	REGNAME(vdecsys, 0x008, VDEC_LARB1_CKEN_STA),
	REGNAME(vencsys, 0x000, VENC_CG_STA),
	REGNAME(jpgdecsys, 0x000, JPGDEC_CG_STA),
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
#define clk_writel_mask(addr, mask, val)	\
	clk_writel(addr, (clk_readl(addr) & ~(mask)) | (val))

#define ABS_DIFF(a, b)	((a) > (b) ? (a) - (b) : (b) - (a))

enum FMETER_TYPE {
	FT_NULL,
	ABIST,
	CKGEN
};

#define FMCLK(_t, _i, _n) { .type = _t, .id = _i, .name = _n }

static const struct fmeter_clk fclks[] = {
	FMCLK(ABIST,   2, "AD_ARMCA35PLL_600M_CORE_CK"),
	FMCLK(ABIST,   3, "AD_ARMCA35PLL_400M_CORE_CK"),
	FMCLK(ABIST,   4, "AD_MAIN_H546M_CK"),
	FMCLK(ABIST,   5, "AD_MAIN_H364M_CK"),
	FMCLK(ABIST,   6, "AD_MAIN_H218P4M_CK"),
	FMCLK(ABIST,   7, "AD_MAIN_H156M_CK"),
	FMCLK(ABIST,   8, "AD_UNIV_178P3M_CK"),
	FMCLK(ABIST,   9, "AD_UNIVPLL_UNIV_48M_CK"),
	FMCLK(ABIST,  10, "AD_UNIV_624M_CK"),
	FMCLK(ABIST,  11, "AD_UNIV_416M_CK"),
	FMCLK(ABIST,  12, "AD_UNIV_249P6M_CK"),
	FMCLK(ABIST,  13, "AD_APLL1_CK"),
	FMCLK(ABIST,  14, "AD_APLL2_CK"),
	FMCLK(ABIST,  15, "AD_LTEPLL_FS26M_CK"),
	FMCLK(ABIST,  16, "rtc32k_ck_i"),
	FMCLK(ABIST,  17, "AD_MMPLL_500M_CK"),
	FMCLK(ABIST,  18, "AD_VENCPLL_380M_CK"),
	FMCLK(ABIST,  19, "AD_VCODEPLL_442M_CK"),
	FMCLK(ABIST,  20, "AD_TVDPLL_572M_CK"),
	FMCLK(ABIST,  21, "AD_LVDSPLL_150M_CK"),
	FMCLK(ABIST,  22, "AD_MSDCPLL_400M_CK"),
	FMCLK(ABIST,  23, "AD_ETHERPLL_50M_CK"),
	FMCLK(ABIST,  24, "clkph_MCK_o"),
	FMCLK(ABIST,  25, "AD_USB_48M_CK"),
	FMCLK(ABIST,  26, "AD_MSDCPLL2_400M_CK"),
	FMCLK(ABIST,  27, "AD_CVBSADC_CKOUTA"),
	FMCLK(ABIST,  30, "AD_TVDPLL_429M_CK"),
	FMCLK(ABIST,  33, "AD_LVDSPLL2_150M_CK"),
	FMCLK(ABIST,  34, "AD_ETHERPLL_125M_CK"),
	FMCLK(ABIST,  35, "AD_MIPI_26M_CK_CKSYS"),
	FMCLK(ABIST,  36, "AD_LTEPLL_ARMPLL26M_CK_CKSYS"),
	FMCLK(ABIST,  37, "AD_LETPLL_SSUSB26M_CK_CKSYS"),
	FMCLK(ABIST,  38, "AD_DSI2_LNTC_DSICLK_CKSYS"),
	FMCLK(ABIST,  39, "AD_DSI3_LNTC_DSICLK_CKSYS"),
	FMCLK(ABIST,  40, "AD_DSI2_MPPLL_TST_CK_CKSYS"),
	FMCLK(ABIST,  41, "AD_DSI3_MPPLL_TST_CK_CKSYS"),
	FMCLK(ABIST,  42, "AD_LVDSTX3_MONCLK"),
	FMCLK(ABIST,  43, "AD_PLLGP_TST_CK_CKSYS"),
	FMCLK(ABIST,  44, "AD_SSUSB_48M_CK_CKSYS"),
	FMCLK(ABIST,  45, "AD_MONREF3_CK"),
	FMCLK(ABIST,  46, "AD_MONFBK3_CK"),
	FMCLK(ABIST,  47, "big_clkmon_o"),
	FMCLK(ABIST,  48, "DA_ARMCPU_MON_CK"),
	FMCLK(ABIST,  49, "AD_CSI0_LNRC_BYTE_CLK"),
	FMCLK(ABIST,  50, "AD_CSI1_LNRC_BYTE_CLK"),
	FMCLK(ABIST,  51, "AD_CSI0_LNRC_4X_CLK"),
	FMCLK(ABIST,  52, "AD_CSI1_LNRC_4X_CLK"),
	FMCLK(ABIST,  53, "AD_CSI0_CAL_CLK"),
	FMCLK(ABIST,  54, "AD_CSI1_CAL_CLK"),
	FMCLK(ABIST,  55, "AD_UNIVPL_1248M_CK"),
	FMCLK(ABIST,  56, "AD_MAINPLL_1092_CORE_CK"),
	FMCLK(ABIST,  57, "AD_ARMCA15PLL_2002M_CORE_CK"),
	FMCLK(ABIST,  58, "mcusys_arm_clk_out_all"),
	FMCLK(ABIST,  59, "AD_ARMCA7PLL_1508M_CORE_CK"),
	FMCLK(ABIST,  61, "AD_UNIVPLL_USB20_48M_CK"),
	FMCLK(ABIST,  62, "AD_UNIVPLL_USB20_48M_CK1"),
	FMCLK(ABIST,  63, "AD_UNIVPLL_USB20_48M_CK2"),
	FMCLK(ABIST,  65, "AD_UNIVPLL_USB20_48M_CK3"),
	FMCLK(ABIST,  77, "AD_LVDSTX1_MONCLK"),
	FMCLK(ABIST,  78, "AD_MONREF1_CK"),
	FMCLK(ABIST,  79, "AD_MONFBK1_CK"),
	FMCLK(ABIST,  87, "AD_DSI0_LNTC_DSICLK_CKSYS"),
	FMCLK(ABIST,  88, "AD_DSI0_MPLL_TST_CK_CKSYS"),
	FMCLK(ABIST,  89, "AD_DSI1_LNTC_DSICLK_CKSYS"),
	FMCLK(ABIST,  90, "AD_DSI1_MPLL_TST_CK_CKSYS"),
	FMCLK(ABIST,  91, "ddr_clk_freq_meter[0]"),
	FMCLK(ABIST,  92, "ddr_clk_freq_meter[1]"),
	FMCLK(ABIST,  93, "ddr_clk_freq_meter[2]"),
	FMCLK(ABIST,  94, "ddr_clk_freq_meter[3]"),
	FMCLK(ABIST,  95, "ddr_clk_freq_meter[4]"),
	FMCLK(ABIST,  96, "ddr_clk_freq_meter[5]"),
	FMCLK(ABIST,  97, "ddr_clk_freq_meter[6]"),
	FMCLK(ABIST,  98, "ddr_clk_freq_meter[7]"),
	FMCLK(ABIST,  99, "ddr_clk_freq_meter[8]"),
	FMCLK(ABIST, 100, "ddr_clk_freq_meter[9]"),
	FMCLK(ABIST, 101, "ddr_clk_freq_meter[10]"),
	FMCLK(ABIST, 102, "ddr_clk_freq_meter[11]"),
	FMCLK(ABIST, 103, "ddr_clk_freq_meter[12]"),
	FMCLK(ABIST, 104, "ddr_clk_freq_meter[13]"),
	FMCLK(ABIST, 105, "ddr_clk_freq_meter[14]"),
	FMCLK(ABIST, 106, "ddr_clk_freq_meter[15]"),
	FMCLK(ABIST, 107, "ddr_clk_freq_meter[16]"),
	FMCLK(ABIST, 108, "ddr_clk_freq_meter[17]"),
	FMCLK(ABIST, 109, "ddr_clk_freq_meter[18]"),
	FMCLK(ABIST, 110, "ddr_clk_freq_meter[19]"),
	FMCLK(ABIST, 111, "ddr_clk_freq_meter[20]"),
	FMCLK(ABIST, 112, "ddr_clk_freq_meter[21]"),
	FMCLK(ABIST, 113, "ddr_clk_freq_meter[22]"),
	FMCLK(ABIST, 114, "ddr_clk_freq_meter[23]"),
	FMCLK(ABIST, 115, "ddr_clk_freq_meter[24]"),
	FMCLK(ABIST, 116, "ddr_clk_freq_meter[25]"),
	FMCLK(ABIST, 117, "ddr_clk_freq_meter[26]"),
	FMCLK(ABIST, 118, "ddr_clk_freq_meter[27]"),
	FMCLK(ABIST, 119, "ddr_clk_freq_meter[28]"),
	FMCLK(ABIST, 120, "ddr_clk_freq_meter[29]"),
	FMCLK(ABIST, 121, "ddr_clk_freq_meter[30]"),
	FMCLK(ABIST, 122, "ddr_clk_freq_meter[31]"),
	FMCLK(ABIST, 123, "ddr_clk_freq_meter[32]"),
	FMCLK(ABIST, 124, "ddr_clk_freq_meter[33]"),
	FMCLK(ABIST, 125, "ddr_clk_freq_meter[34]"),
	FMCLK(CKGEN,   1, "hf_faxi_ck"),
	FMCLK(CKGEN,   2, "hd_faxi_ck"),
	FMCLK(CKGEN,   3, "hf_fscam_ck"),
	FMCLK(CKGEN,   5, "hf_fmm_ck"),
	FMCLK(CKGEN,   6, "f_fpwm_ck"),
	FMCLK(CKGEN,   7, "hf_fvdec_ck"),
	FMCLK(CKGEN,   8, "hf_fvenc_ck"),
	FMCLK(CKGEN,   9, "hf_fmfg_ck"),
	FMCLK(CKGEN,  10, "hf_fcamtg_ck"),
	FMCLK(CKGEN,  11, "f_fuart_ck"),
	FMCLK(CKGEN,  12, "hf_fspi_ck"),
	FMCLK(CKGEN,  13, "f_fusb20_ck"),
	FMCLK(CKGEN,  14, "f_fusb30_ck"),
	FMCLK(CKGEN,  15, "hf_fmsdc50_0_hclk_ck"),
	FMCLK(CKGEN,  16, "hf_fmsdc50_0_ck"),
	FMCLK(CKGEN,  17, "hf_fmsdc30_1_ck"),
	FMCLK(CKGEN,  18, "hf_fmsdc30_2_ck"),
	FMCLK(CKGEN,  19, "hf_fmsdc30_3_ck"),
	FMCLK(CKGEN,  20, "hf_faudio_ck"),
	FMCLK(CKGEN,  21, "hf_faud_intbus_ck"),
	FMCLK(CKGEN,  22, "hf_fpmicspi_ck"),
	FMCLK(CKGEN,  23, "hf_fdpilvds1_ck"),
	FMCLK(CKGEN,  24, "hf_fatb_ck"),
	FMCLK(CKGEN,  25, "hf_fnr_ck"),
	FMCLK(CKGEN,  26, "hf_firda_ck"),
	FMCLK(CKGEN,  27, "hf_fcci400_ck"),
	FMCLK(CKGEN,  28, "hf_faud_1_ck"),
	FMCLK(CKGEN,  29, "hf_faud_2_ck"),
	FMCLK(CKGEN,  30, "hf_fmem_mfg_in_as_ck"),
	FMCLK(CKGEN,  31, "hf_faxi_mfg_in_as_ck"),
	FMCLK(CKGEN,  32, "f_frtc_ck"),
	FMCLK(CKGEN,  33, "f_f26m_ck"),
	FMCLK(CKGEN,  34, "f_f32k_md1_ck"),
	FMCLK(CKGEN,  35, "f_frtc_conn_ck"),
	FMCLK(CKGEN,  36, "hg_fmipicfg_ck"),
	FMCLK(CKGEN,  37, "hd_haxi_nli_ck"),
	FMCLK(CKGEN,  38, "hd_qaxidcm_ck"),
	FMCLK(CKGEN,  39, "f_ffpc_ck"),
	FMCLK(CKGEN,  40, "f_fckbus_ck_scan"),
	FMCLK(CKGEN,  41, "f_fckrtc_ck_scan"),
	FMCLK(CKGEN,  42, "hf_flvds_pxl_ck"),
	FMCLK(CKGEN,  43, "hf_flvds_cts_ck"),
	FMCLK(CKGEN,  44, "hf_fdpilvds_ck"),
	FMCLK(CKGEN,  45, "hf_flvds1_pxl_ck"),
	FMCLK(CKGEN,  46, "hf_flvds1_cts_ck"),
	FMCLK(CKGEN,  47, "hf_fhdcp_ck"),
	FMCLK(CKGEN,  48, "hf_fmsdc50_3_hclk_ck"),
	FMCLK(CKGEN,  49, "hf_fhdcp_24m_ck"),
	FMCLK(CKGEN,  50, "hf_fmsdc0p_aes_ck"),
	FMCLK(CKGEN,  51, "hf_fgcpu_ck"),
	FMCLK(CKGEN,  52, "hf_fmem_ck"),
	FMCLK(CKGEN,  53, "hf_fi2so1_mck"),
	FMCLK(CKGEN,  54, "hf_fcam2tg_ck"),
	FMCLK(CKGEN,  55, "hf_fether_125m_ck"),
	FMCLK(CKGEN,  56, "hf_fapll2_ck"),
	FMCLK(CKGEN,  57, "hf_fa2sys_hp_ck"),
	FMCLK(CKGEN,  58, "hf_fasm_l_ck"),
	FMCLK(CKGEN,  59, "hf_fspislv_ck"),
	FMCLK(CKGEN,  60, "hf_ftdmo1_mck"),
	FMCLK(CKGEN,  61, "hf_fasm_h_ck"),
	FMCLK(CKGEN,  62, "hf_ftdmo0_mck"),
	FMCLK(CKGEN,  63, "hf_fa1sys_hp_ck"),
	FMCLK(CKGEN,  65, "hf_fasm_m_ck"),
	FMCLK(CKGEN,  66, "hf_fapll_ck"),
	FMCLK(CKGEN,  67, "hf_fspinor_ck"),
	FMCLK(CKGEN,  68, "hf_fpe2_mac_p0_ck"),
	FMCLK(CKGEN,  69, "hf_fjpgdec_ck"),
	FMCLK(CKGEN,  70, "hf_fpwm_infra_ck"),
	FMCLK(CKGEN,  71, "hf_fnfiecc_ck"),
	FMCLK(CKGEN,  72, "hf_fether_50m_rmii_ck"),
	FMCLK(CKGEN,  73, "hf_fi2c_ck"),
	FMCLK(CKGEN,  74, "hf_fcmsys_ck"),
	FMCLK(CKGEN,  75, "hf_fpe2_mac_p1_ck"),
	FMCLK(CKGEN,  76, "hf_fdi_ck"),
	FMCLK(CKGEN,  77, "hf_fi2si3_mck"),
	FMCLK(CKGEN,  78, "hf_fether_50m_ck"),
	FMCLK(CKGEN,  79, "hf_fi2si2_mck"),
	FMCLK(CKGEN,  80, "hf_fi2so3_mck"),
	FMCLK(CKGEN,  81, "hf_ftvd_ck"),
	FMCLK(CKGEN,  82, "hf_fnfi2x_ck"),
	FMCLK(CKGEN,  83, "hf_fi2si1_mck"),
	FMCLK(CKGEN,  84, "hf_fi2so2_mck"),
	{}
};

#define FHCTL_HP_EN		(rb[fhctl].virt + 0x000)
#define CLK_CFG_M0		(rb[topckgen].virt + 0x100)
#define CLK_CFG_M1		(rb[topckgen].virt + 0x104)
#define CLK_MISC_CFG_1	(rb[topckgen].virt + 0x214)
#define CLK_MISC_CFG_2	(rb[topckgen].virt + 0x218)
#define CLK26CALI_0		(rb[topckgen].virt + 0x220)
#define CLK26CALI_1		(rb[topckgen].virt + 0x224)
#define CLK26CALI_2		(rb[topckgen].virt + 0x228)
#define PLL_TEST_CON0	(rb[apmixed].virt + 0x040)
#define CVBSPLL_CON1	(rb[apmixed].virt + 0x314)
#define CVBSREFPLL_CON1	(rb[apmixed].virt + 0x31c)

#define RG_FRMTR_WINDOW     1023

static void set_fmeter_divider_ca35(u32 k1)
{
	u32 v = clk_readl(CLK_MISC_CFG_1);

	v = ALT_BITS(v, 15, 8, k1);
	clk_writel(CLK_MISC_CFG_1, v);
}

static void set_fmeter_divider_ca72(u32 k1)
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

static u8 wait_fmeter_done(u32 tri_bit)
{
	static int max_wait_count;
	int wait_count = (max_wait_count > 0) ? (max_wait_count * 2 + 2) : 100;
	int i;

	/* wait fmeter */
	for (i = 0; i < wait_count && (clk_readl(CLK26CALI_0) & tri_bit); i++)
		udelay(20);

	if (!(clk_readl(CLK26CALI_0) & tri_bit)) {
		max_wait_count = max(max_wait_count, i);
		return 1;
	}

	return 0;
}
static u32 fmeter_freq(enum FMETER_TYPE type, int k1, int clk)
{
	void __iomem *clk_cfg_reg = (type == CKGEN) ? CLK_CFG_M1 : CLK_CFG_M0;
	void __iomem *cnt_reg = (type == CKGEN) ? CLK26CALI_2 : CLK26CALI_1;
	u32 cksw_mask = (type == CKGEN) ? GENMASK(22, 16) : GENMASK(14, 8);
	u32 cksw_val = (type == CKGEN) ? (clk << 16) : (clk << 8);
	u32 tri_bit = (type == CKGEN) ? BIT(4) : BIT(0);
	u32 clk_exc = (type == CKGEN) ? BIT(5) : BIT(2);
	u32 clk_misc_cfg_1, clk_misc_cfg_2, clk_cfg_val, cnt, freq = 0;

	/* setup fmeter */
	clk_setl(CLK26CALI_0, BIT(7));	/* enable fmeter_en */
	clk_clrl(CLK26CALI_0, clk_exc);	/* set clk_exc */
	clk_writel_mask(cnt_reg, GENMASK(25, 16), RG_FRMTR_WINDOW << 16);	/* load_cnt */

	clk_misc_cfg_1 = clk_readl(CLK_MISC_CFG_1);	/* backup CLK_MISC_CFG_1 value */
	clk_misc_cfg_2 = clk_readl(CLK_MISC_CFG_2);	/* backup CLK_MISC_CFG_2 value */
	clk_cfg_val = clk_readl(clk_cfg_reg);		/* backup clk_cfg_reg value */

	set_fmeter_divider(k1);			/* set divider (0 = /1) */
	set_fmeter_divider_ca35(k1);
	set_fmeter_divider_ca72(k1);
	clk_writel_mask(clk_cfg_reg, cksw_mask, cksw_val);	/* select cksw */

	clk_setl(CLK26CALI_0, tri_bit);	/* start fmeter */

	if (wait_fmeter_done(tri_bit)) {
		cnt = clk_readl(cnt_reg) & 0xFFFF;
		freq = (cnt * 26000) * (k1 + 1) / (RG_FRMTR_WINDOW + 1); /* (KHz) ; freq = counter * 26M / 1024 */
	}

	/* restore register settings */
	clk_writel(clk_cfg_reg, clk_cfg_val);
	clk_writel(CLK_MISC_CFG_2, clk_misc_cfg_2);
	clk_writel(CLK_MISC_CFG_1, clk_misc_cfg_1);

	clk_clrl(CLK26CALI_0, BIT(7));	/* disable fmeter_en */

	return freq;
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

static const struct fmeter_clk *get_all_fmeter_clks(void)
{
	return fclks;
}

struct bak {
	u32 fhctl_hp_en;
};

static void *prepare_fmeter(void)
{
	static struct bak regs;

	regs.fhctl_hp_en = clk_readl(FHCTL_HP_EN);

	clk_writel(FHCTL_HP_EN, 0x0);		/* disable PLL hopping */
	udelay(10);

	/* use AD_PLLGP_TST_CK_CKSYS to measure CVBSPLL */
	clk_setl(PLL_TEST_CON0, 0x30F); /* [9:8]:TST_SEL, [3:0]:TSTOD_EN, A2DCK_EN, TSTCK_EN, TST_EN */
	clk_setl(CVBSREFPLL_CON1, 0x11); /* [4]:CVBS_MONCK_EN, [3:0]:CVBSREFPLL_TESTMUX */
	clk_setl(CVBSPLL_CON1, 0x20); /* [5]: CVBSPLL_MONCK_EN */

	return &regs;
}

static void unprepare_fmeter(void *data)
{
	struct bak *regs = data;

	clk_clrl(PLL_TEST_CON0, 0x30F); /* [9:8]:TST_SEL, [3:0]:TSTOD_EN, A2DCK_EN, TSTCK_EN, TST_EN */
	clk_clrl(CVBSREFPLL_CON1, 0x11); /* [4]:CVBS_MONCK_EN, [3:0]:CVBSREFPLL_TESTMUX */
	clk_clrl(CVBSPLL_CON1, 0x20); /* [5]: CVBSPLL_MONCK_EN */

	/* restore old setting */
	clk_writel(FHCTL_HP_EN, regs->fhctl_hp_en);
}

static u32 fmeter_freq_op(const struct fmeter_clk *fclk)
{
	if (fclk->type)
		return measure_stable_fmeter_freq(fclk->type, 0, fclk->id);

	return 0;
}

/*
 * clkdbg dump_state
 */

static const char * const *get_all_clk_names(void)
{
	static const char * const clks[] = {
		/* plls */
		"mainpll",
		"univpll",
		"vcodecpll",
		"vencpll",
		"apll1",
		"apll2",
		"lvdspll",
		"lvdspll2",
		"msdcpll",
		"msdcpll2",
		"tvdpll",
		"mmpll",
		"armca35pll",
		"armca72pll",
		"etherpll",
		"cvbspll",
		/* apmixedsys */
		"ref2usb_tx",
		/* topckgen */
		"armca35pll_ck",
		"armca35pll_600m",
		"armca35pll_400m",
		"armca72pll_ck",
		"syspll_ck",
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
		"univpll_ck",
		"univpll_d7",
		"univpll_d26",
		"univpll_d52",
		"univpll_d104",
		"univpll_d208",
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
		"f_mp0_pll1_ck",
		"f_mp0_pll2_ck",
		"f_big_pll1_ck",
		"f_big_pll2_ck",
		"f_bus_pll1_ck",
		"f_bus_pll2_ck",
		"apll1_ck",
		"apll1_d2",
		"apll1_d4",
		"apll1_d8",
		"apll1_d16",
		"apll2_ck",
		"apll2_d2",
		"apll2_d4",
		"apll2_d8",
		"apll2_d16",
		"lvdspll_ck",
		"lvdspll_d2",
		"lvdspll_d4",
		"lvdspll_d8",
		"lvdspll2_ck",
		"lvdspll2_d2",
		"lvdspll2_d4",
		"lvdspll2_d8",
		"etherpll_125m",
		"etherpll_50m",
		"cvbs",
		"cvbs_d2",
		"sys_26m",
		"mmpll_ck",
		"mmpll_d2",
		"vencpll_ck",
		"vencpll_d2",
		"vcodecpll_ck",
		"vcodecpll_d2",
		"tvdpll_ck",
		"tvdpll_d2",
		"tvdpll_d4",
		"tvdpll_d8",
		"tvdpll_429m",
		"tvdpll_429m_d2",
		"tvdpll_429m_d4",
		"msdcpll_ck",
		"msdcpll_d2",
		"msdcpll_d4",
		"msdcpll2_ck",
		"msdcpll2_d2",
		"msdcpll2_d4",
		"clk26m_d2",
		"d2a_ulclk_6p5m",
		"vpll3_dpix",
		"vpll_dpix",
		"ltepll_fs26m",
		"dmpll_ck",
		"dsi0_lntc",
		"dsi1_lntc",
		"lvdstx3",
		"lvdstx",
		"clkrtc_ext",
		"clkrtc_int",
		"csi0",
		"apll_div0",
		"apll_div1",
		"apll_div2",
		"apll_div3",
		"apll_div4",
		"apll_div5",
		"apll_div6",
		"apll_div7",
		"apll_div_pdn0",
		"apll_div_pdn1",
		"apll_div_pdn2",
		"apll_div_pdn3",
		"apll_div_pdn4",
		"apll_div_pdn5",
		"apll_div_pdn6",
		"apll_div_pdn7",
		"axi_sel",
		"mem_sel",
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
		"dpilvds1_sel",
		"atb_sel",
		"nr_sel",
		"nfi2x_sel",
		"irda_sel",
		"cci400_sel",
		"aud_1_sel",
		"aud_2_sel",
		"mem_mfg_sel",
		"axi_mfg_sel",
		"scam_sel",
		"nfiecc_sel",
		"pe2_mac_p0_sel",
		"pe2_mac_p1_sel",
		"dpilvds_sel",
		"msdc50_3_h_sel",
		"hdcp_sel",
		"hdcp_24m_sel",
		"rtc_sel",
		"spinor_sel",
		"apll_sel",
		"apll2_sel",
		"a1sys_hp_sel",
		"a2sys_hp_sel",
		"asm_l_sel",
		"asm_m_sel",
		"asm_h_sel",
		"i2so1_sel",
		"i2so2_sel",
		"i2so3_sel",
		"tdmo0_sel",
		"tdmo1_sel",
		"i2si1_sel",
		"i2si2_sel",
		"i2si3_sel",
		"ether_125m_sel",
		"ether_50m_sel",
		"jpgdec_sel",
		"spislv_sel",
		"ether_sel",
		"cam2tg_sel",
		"di_sel",
		"tvd_sel",
		"i2c_sel",
		"pwm_infra_sel",
		"msdc0p_aes_sel",
		"cmsys_sel",
		"gcpu_sel",
		"aud_apll1_sel",
		"aud_apll2_sel",
		"audull_vtx_sel",
		/* mcucfg */
		"mcu_mp0_sel",
		"mcu_mp2_sel",
		"mcu_bus_sel",
		/* bdpsys */
		"bdp_bridge_b",
		"bdp_bridge_d",
		"bdp_larb_d",
		"bdp_vdi_pxl",
		"bdp_vdi_d",
		"bdp_vdi_b",
		"bdp_fmt_b",
		"bdp_27m",
		"bdp_27m_vdout",
		"bdp_27_74_74",
		"bdp_2fs",
		"bdp_2fs74_148",
		"bdp_b",
		"bdp_vdo_d",
		"bdp_vdo_2fs",
		"bdp_vdo_b",
		"bdp_di_pxl",
		"bdp_di_d",
		"bdp_di_b",
		"bdp_nr_agent",
		"bdp_nr_d",
		"bdp_nr_b",
		"bdp_bridge_rt_b",
		"bdp_bridge_rt_d",
		"bdp_larb_rt_d",
		"bdp_tvd_tdc",
		"bdp_tvd_clk_54",
		"bdp_tvd_cbus",
		/* infracfg */
		"infra_dbgclk",
		"infra_gce",
		"infra_m4u",
		"infra_kp",
		"infra_ao_spi0",
		"infra_ao_spi1",
		"infra_ao_uart5",
		/* imgsys */
		"img_smi_larb2",
		"img_scam_en",
		"img_cam_en",
		"img_cam_sv_en",
		"img_cam_sv1_en",
		"img_cam_sv2_en",
		/* jpgdecsys */
		"jpgdec_jpgdec1",
		"jpgdec_jpgdec",
		/* mfgcfg */
		"mfg_bg3d",
		/* mmsys */
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
		"mm_mdp_crop",
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
		"mm_disp_od",
		"mm_pwm0_mm",
		"mm_pwm0_26m",
		"mm_pwm1_mm",
		"mm_pwm1_26m",
		"mm_dsi0_engine",
		"mm_dsi0_digital",
		"mm_dsi1_engine",
		"mm_dsi1_digital",
		"mm_dpi_pixel",
		"mm_dpi_engine",
		"mm_dpi1_pixel",
		"mm_dpi1_engine",
		"mm_lvds_pixel",
		"mm_lvds_cts",
		"mm_smi_larb4",
		"mm_smi_common1",
		"mm_smi_larb5",
		"mm_mdp_rdma2",
		"mm_mdp_tdshp2",
		"mm_disp_ovl2",
		"mm_disp_wdma2",
		"mm_disp_color2",
		"mm_disp_aal1",
		"mm_disp_od1",
		"mm_lvds1_pixel",
		"mm_lvds1_cts",
		"mm_smi_larb7",
		"mm_mdp_rdma3",
		"mm_mdp_wrot2",
		"mm_dsi2",
		"mm_dsi2_digital",
		"mm_dsi3",
		"mm_dsi3_digital",
		/* pericfg */
		"per_nfi",
		"per_therm",
		"per_pwm0",
		"per_pwm1",
		"per_pwm2",
		"per_pwm3",
		"per_pwm4",
		"per_pwm5",
		"per_pwm6",
		"per_pwm7",
		"per_pwm",
		"per_ap_dma",
		"per_msdc30_0",
		"per_msdc30_1",
		"per_msdc30_2",
		"per_msdc30_3",
		"per_uart0",
		"per_uart1",
		"per_uart2",
		"per_uart3",
		"per_i2c0",
		"per_i2c1",
		"per_i2c2",
		"per_i2c3",
		"per_i2c4",
		"per_auxadc",
		"per_spi0",
		"per_spi",
		"per_i2c5",
		"per_spi2",
		"per_spi3",
		"per_spi5",
		"per_uart4",
		"per_sflash",
		"per_gmac",
		"per_pcie0",
		"per_pcie1",
		"per_gmac_pclk",
		"per_msdc50_0_en",
		"per_msdc30_1_en",
		"per_msdc30_2_en",
		"per_msdc30_3_en",
		"per_msdc50_0_h",
		"per_msdc50_3_h",
		/* vdecsys */
		"vdec_cken",
		"vdec_larb1_cken",
		"vdec_imgrz_cken",
		/* vencsys */
		"venc_smi",
		"venc_venc",
		"venc_smi_larb6",
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
		[1]  = "CONN",
		[2]  = "DDRPHY0",
		[3]  = "DISP",
		[4]  = "MFG",
		[5]  = "ISP",
		[6]  = "INFRA",
		[7]  = "VDEC",
		[8]  = "MP0_CPUTOP",
		[9]  = "MP0_CPU0",
		[10] = "MP0_CPU1",
		[11] = "MP0_CPU2",
		[12] = "MP0_CPU3",
		[13] = "",
		[14] = "MCUSYS",
		[15] = "MP1_CPUTOP",
		[16] = "MP1_CPU0",
		[17] = "MP1_CPU1",
		[18] = "",
		[19] = "USB2",
		[20] = "",
		[21] = "VENC",
		[22] = "",
		[23] = "",
		[24] = "AUDIO",
		[25] = "USB",
		[26] = "",
		[27] = "DDRPHY1",
		[28] = "DDRPHY2",
		[29] = "DDRPHY3",
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
		{"mfgcfg", BIT(4)},
		{"mmsys", BIT(3)},
		{"imgsys", BIT(3) | BIT(5)},
		{"bdpsys", BIT(3) | BIT(5)},
		{"vdecsys", BIT(3) | BIT(7)},
		{"vencsys", BIT(3) | BIT(21)},
		{"jpgdecsys", BIT(3) | BIT(21)},
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
 * init functions
 */

static struct clkdbg_ops clkdbg_mt2712_ops = {
	.get_all_fmeter_clks = get_all_fmeter_clks,
	.prepare_fmeter = prepare_fmeter,
	.unprepare_fmeter = unprepare_fmeter,
	.fmeter_freq = fmeter_freq_op,
	.get_all_regnames = get_all_regnames,
	.get_all_clk_names = get_all_clk_names,
	.get_pwr_names = get_pwr_names,
	.setup_provider_clk = setup_provider_clk,
};

static void __init init_custom_cmds(void)
{
	static const struct cmd_fn cmds[] = {
		{}
	};

	set_custom_cmds(cmds);
}

static int __init clkdbg_mt2712_init(void)
{
	if (!of_machine_is_compatible("mediatek,mt2712"))
		return -ENODEV;

	init_regbase();

	init_custom_cmds();
	set_clkdbg_ops(&clkdbg_mt2712_ops);

#if DUMP_INIT_STATE
	print_regs();
	print_fmeter_all();
#endif /* DUMP_INIT_STATE */

	return 0;
}
device_initcall(clkdbg_mt2712_init);
