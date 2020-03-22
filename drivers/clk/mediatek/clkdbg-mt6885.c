/*
 * Copyright (C) 2019 MediaTek Inc.
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

#ifdef CONFIG_MTK_DEVAPC
#include <mt-plat/devapc_public.h>
#endif
#include "clkdbg.h"

void print_subsys_reg(char *subsys_name);

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
	mdpsys,
	img1sys,
	img2sys,
	ipesys,
	camsys,
	cam_rawa_sys,
	cam_rawb_sys,
	cam_rawc_sys,
	vencsys,
	venc_c1_sys,
	vdecsys,
	vdec_soc_sys,
	ipu_vcore,
	ipu_conn,
	ipu0,
	ipu1,
	ipu2,
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
	[audio]    = REGBASE_V(0x11210000, audio),
	[mfgsys]   = REGBASE_V(0x13fbf000, mfgsys),
	[mmsys]    = REGBASE_V(0x14116000, mmsys),
	[mdpsys]    = REGBASE_V(0x1F000000, mdpsys),
	[img1sys]   = REGBASE_V(0x15020000, img1sys),
	[img2sys]   = REGBASE_V(0x15820000, img2sys),
	[ipesys]   = REGBASE_V(0x1b000000, ipesys),
	[camsys]   = REGBASE_V(0x1a000000, camsys),
	[cam_rawa_sys]   = REGBASE_V(0x1a04f000, cam_rawa_sys),
	[cam_rawb_sys]   = REGBASE_V(0x1a06f000, cam_rawb_sys),
	[cam_rawc_sys]   = REGBASE_V(0x1a08f000, cam_rawc_sys),
	[vencsys]  = REGBASE_V(0x17000000, vencsys),
	[venc_c1_sys]  = REGBASE_V(0x17800000, venc_c1_sys),
	[vdecsys]  = REGBASE_V(0x1602f000, vdecsys),
	[vdec_soc_sys]  = REGBASE_V(0x1600f000, vdec_soc_sys),
	[ipu_vcore]  = REGBASE_V(0x19029000, ipu_vcore),
	[ipu_conn]  = REGBASE_V(0x19020000, ipu_conn),
	[ipu0]  = REGBASE_V(0x19030000, ipu0),
	[ipu1]  = REGBASE_V(0x19031000, ipu1),
	[ipu2]  = REGBASE_V(0x19032000, ipu2),
};

#define REGNAME(_base, _ofs, _name)	\
	{ .base = &rb[_base], .ofs = _ofs, .name = #_name }

static struct regname rn[] = {
	REGNAME(topckgen,  0x010, CLK_CFG_0),
	REGNAME(topckgen,  0x020, CLK_CFG_1),
	REGNAME(topckgen,  0x030, CLK_CFG_2),
	REGNAME(topckgen,  0x040, CLK_CFG_3),
	REGNAME(topckgen,  0x050, CLK_CFG_4),
	REGNAME(topckgen,  0x060, CLK_CFG_5),
	REGNAME(topckgen,  0x070, CLK_CFG_6),
	REGNAME(topckgen,  0x080, CLK_CFG_7),
	REGNAME(topckgen,  0x090, CLK_CFG_8),
	REGNAME(topckgen,  0x0A0, CLK_CFG_9),
	REGNAME(topckgen,  0x0B0, CLK_CFG_10),
	REGNAME(topckgen,  0x0C0, CLK_CFG_11),
	REGNAME(topckgen,  0x0D0, CLK_CFG_12),
	REGNAME(topckgen,  0x0E0, CLK_CFG_13),
	REGNAME(topckgen,  0x0F0, CLK_CFG_14),
	REGNAME(topckgen,  0x100, CLK_CFG_15),
	REGNAME(topckgen,  0x110, CLK_CFG_16),

	REGNAME(apmixed, 0x050, PLLON_CON0),
	REGNAME(apmixed, 0x054, PLLON_CON1),
	REGNAME(apmixed, 0x058, PLLON_CON2),
	REGNAME(apmixed, 0x05C, PLLON_CON3),
	REGNAME(apmixed, 0x208, ARMPLL_LL_CON0),
	REGNAME(apmixed, 0x20C, ARMPLL_LL_CON1),
	REGNAME(apmixed, 0x210, ARMPLL_LL_CON2),
	REGNAME(apmixed, 0x214, ARMPLL_LL_CON3),
	REGNAME(apmixed, 0x218, ARMPLL_BL0_CON0),
	REGNAME(apmixed, 0x21C, ARMPLL_BL0_CON1),
	REGNAME(apmixed, 0x220, ARMPLL_BL0_CON2),
	REGNAME(apmixed, 0x224, ARMPLL_BL0_CON3),
	REGNAME(apmixed, 0x228, ARMPLL_BL1_CON0),
	REGNAME(apmixed, 0x22C, ARMPLL_BL1_CON1),
	REGNAME(apmixed, 0x230, ARMPLL_BL1_CON2),
	REGNAME(apmixed, 0x234, ARMPLL_BL1_CON3),
	REGNAME(apmixed, 0x238, ARMPLL_BL2_CON0),
	REGNAME(apmixed, 0x23C, ARMPLL_BL2_CON1),
	REGNAME(apmixed, 0x240, ARMPLL_BL2_CON2),
	REGNAME(apmixed, 0x244, ARMPLL_BL2_CON3),
	REGNAME(apmixed, 0x248, ARMPLL_BL3_CON0),
	REGNAME(apmixed, 0x24C, ARMPLL_BL3_CON1),
	REGNAME(apmixed, 0x250, ARMPLL_BL3_CON2),
	REGNAME(apmixed, 0x254, ARMPLL_BL3_CON3),
	REGNAME(apmixed, 0x258, CCIPLL_CON0),
	REGNAME(apmixed, 0x25C, CCIPLL_CON1),
	REGNAME(apmixed, 0x260, CCIPLL_CON2),
	REGNAME(apmixed, 0x264, CCIPLL_CON3),
	REGNAME(apmixed, 0x268, MFGPLL_CON0),
	REGNAME(apmixed, 0x26C, MFGPLL_CON1),
	REGNAME(apmixed, 0x274, MFGPLL_CON3),
	REGNAME(apmixed, 0x308, UNIVPLL_CON0),
	REGNAME(apmixed, 0x30C, UNIVPLL_CON1),
	REGNAME(apmixed, 0x314, UNIVPLL_CON3),
	REGNAME(apmixed, 0x318, APLL1_CON0),
	REGNAME(apmixed, 0x31C, APLL1_CON1),
	REGNAME(apmixed, 0x320, APLL1_CON2),
	REGNAME(apmixed, 0x324, APLL1_CON3),
	REGNAME(apmixed, 0x328, APLL1_CON4),
	REGNAME(apmixed, 0x32C, APLL2_CON0),
	REGNAME(apmixed, 0x330, APLL2_CON1),
	REGNAME(apmixed, 0x334, APLL2_CON2),
	REGNAME(apmixed, 0x338, APLL2_CON3),
	REGNAME(apmixed, 0x33C, APLL2_CON4),
	REGNAME(apmixed, 0x340, MAINPLL_CON0),
	REGNAME(apmixed, 0x344, MAINPLL_CON1),
	REGNAME(apmixed, 0x34C, MAINPLL_CON3),
	REGNAME(apmixed, 0x350, MSDCPLL_CON0),
	REGNAME(apmixed, 0x354, MSDCPLL_CON1),
	REGNAME(apmixed, 0x35C, MSDCPLL_CON3),
	REGNAME(apmixed, 0x360, MMPLL_CON0),
	REGNAME(apmixed, 0x364, MMPLL_CON1),
	REGNAME(apmixed, 0x36C, MMPLL_CON3),
	REGNAME(apmixed, 0x370, ADSPPLL_CON0),
	REGNAME(apmixed, 0x374, ADSPPLL_CON1),
	REGNAME(apmixed, 0x37C, ADSPPLL_CON3),
	REGNAME(apmixed, 0x380, TVDPLL_CON0),
	REGNAME(apmixed, 0x384, TVDPLL_CON1),
	REGNAME(apmixed, 0x38C, TVDPLL_CON3),
	REGNAME(apmixed, 0x390, MPLL_CON0),
	REGNAME(apmixed, 0x394, MPLL_CON1),
	REGNAME(apmixed, 0x39C, MPLL_CON3),
	REGNAME(apmixed, 0x3A0, APUPLL_CON0),
	REGNAME(apmixed, 0x3A4, APUPLL_CON1),
	REGNAME(apmixed, 0x3AC, APUPLL_CON3),

	REGNAME(scpsys, 0x0000, POWERON_CONFIG_EN),
	REGNAME(scpsys, 0x016C, PWR_STATUS),
	REGNAME(scpsys, 0x0170, PWR_STATUS_2ND),
	REGNAME(scpsys, 0x0178, OTHER_PWR_STATUS),
	REGNAME(scpsys, 0x300, MD1_PWR_CON),
	REGNAME(scpsys, 0x304, CONN_PWR_CON),
	REGNAME(scpsys, 0x308, MFG0_PWR_CON),
	REGNAME(scpsys, 0x30C, MFG1_PWR_CON),
	REGNAME(scpsys, 0x310, MFG2_PWR_CON),
	REGNAME(scpsys, 0x314, MFG3_PWR_CON),
	REGNAME(scpsys, 0x318, MFG4_PWR_CON),
	REGNAME(scpsys, 0x31C, MFG5_PWR_CON),
	REGNAME(scpsys, 0x320, MFG6_PWR_CON),
	REGNAME(scpsys, 0x324, IFR_PWR_CON),
	REGNAME(scpsys, 0x328, IFR_SUB_PWR_CON),
	REGNAME(scpsys, 0x32C, DPY_PWR_CON),
	REGNAME(scpsys, 0x330, ISP_PWR_CON),
	REGNAME(scpsys, 0x334, ISP2_PWR_CON),
	REGNAME(scpsys, 0x338, IPE_PWR_CON),
	REGNAME(scpsys, 0x33C, VDE_PWR_CON),
	REGNAME(scpsys, 0x340, VDE2_PWR_CON),
	REGNAME(scpsys, 0x344, VEN_PWR_CON),
	REGNAME(scpsys, 0x348, VEN_CORE1_PWR_CON),
	REGNAME(scpsys, 0x34C, MDP_PWR_CON),
	REGNAME(scpsys, 0x350, DIS_PWR_CON),
	REGNAME(scpsys, 0x354, AUDIO_PWR_CON),
	REGNAME(scpsys, 0x358, ADSP_PWR_CON),
	REGNAME(scpsys, 0x35C, CAM_PWR_CON),
	REGNAME(scpsys, 0x360, CAM_RAWA_PWR_CON),
	REGNAME(scpsys, 0x364, CAM_RAWB_PWR_CON),
	REGNAME(scpsys, 0x368, CAM_RAWC_PWR_CON),
	REGNAME(scpsys, 0x3AC, DP_TX_PWR_CON),
	REGNAME(scpsys, 0x3C4, DPY2_PWR_CON),
	REGNAME(scpsys, 0x398, MD_EXT_BUCK_ISO_CON),

	REGNAME(audio, 0x0000, AUDIO_TOP_CON0),
	REGNAME(audio, 0x0004, AUDIO_TOP_CON1),
	REGNAME(audio, 0x0008, AUDIO_TOP_CON2),

	REGNAME(camsys, 0x0000, CAMSYS_CG_CON),
	REGNAME(cam_rawa_sys, 0x0000, CAMSYS_RAWA_CG_CON),
	REGNAME(cam_rawb_sys, 0x0000, CAMSYS_RAWB_CG_CON),
	REGNAME(cam_rawc_sys, 0x0000, CAMSYS_RAWC_CG_CON),

	REGNAME(img1sys, 0x0000, IMG1_CG_CON),
	REGNAME(img2sys, 0x0000, IMG2_CG_CON),
	REGNAME(ipesys, 0x0000, IPE_CG_CON),

	REGNAME(infracfg,  0x090, MODULE_SW_CG_0),
	REGNAME(infracfg,  0x094, MODULE_SW_CG_1),
	REGNAME(infracfg,  0x0ac, MODULE_SW_CG_2),
	REGNAME(infracfg,  0x0c8, MODULE_SW_CG_3),
	REGNAME(infracfg,  0xe98, MM_HANG_FREE),
#if 0
/*
 *	REGNAME(ipu0,  0x000, IPU0_CORE_CG),
 *	REGNAME(ipu1,  0x000, IPU1_CORE_CG),
 *	REGNAME(ipu2,  0x000, IPU2_CORE_CG),
 *	REGNAME(ipu_conn,  0x000, IPU_CONN_CG),
 *	REGNAME(ipu_vcore,  0x000, IPU_VCORE_CG),
 */
#endif

	REGNAME(mfgsys, 0x0000, MFG_CG_CON),

	REGNAME(mmsys, 0x100, MM_CG_CON0),
	REGNAME(mmsys, 0x110, MM_CG_CON1),
	REGNAME(mmsys, 0x1a0, MM_CG_CON2),

	REGNAME(mdpsys, 0x100, MDP_CG_CON0),
	REGNAME(mdpsys, 0x104, MDP_CG_SET0),
	REGNAME(mdpsys, 0x114, MDP_CG_SET1),
	REGNAME(mdpsys, 0x124, MDP_CG_SET2),


	REGNAME(vdecsys, 0x0000, VDEC_CKEN_SET),
	REGNAME(vdecsys, 0x0008, VDEC_LARB1_CKEN_SET),
	REGNAME(vdecsys, 0x0200, VDEC_LAT_CKEN_SET),

	REGNAME(vdec_soc_sys, 0x0000, VDEC_SOC_CKEN_SET),
	REGNAME(vdec_soc_sys, 0x0008, VDEC_SOC_LARB1_CKEN_SET),
	REGNAME(vdec_soc_sys, 0x0200, VDEC_SOC_LAT_CKEN_SET),

	REGNAME(venc_c1_sys, 0x0000, VENC_C1_CG_CON),

	REGNAME(vencsys, 0x0000, VENC_CG_CON),
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
#if 0
enum FMETER_TYPE {
	FT_NULL,
	ABIST,
	CKGEN
};
#endif
#define FMCLK(_t, _i, _n) { .type = _t, .id = _i, .name = _n }

static const struct fmeter_clk fclks[] = {
	FMCLK(CKGEN,	1, "axi_sel"),
	FMCLK(CKGEN,	2, "spm_sel"),
	FMCLK(CKGEN,	3, "scp_sel"),
	FMCLK(CKGEN,	4, "bus_aximem_sel"),
	FMCLK(CKGEN,	5, "mm_sel"),
	FMCLK(CKGEN,	6, "mdp_sel"),
	FMCLK(CKGEN,	7, "img1_sel"),
	FMCLK(CKGEN,	8, "img2_sel"),
	FMCLK(CKGEN,	9, "ipe_sel"),
	FMCLK(CKGEN,	10, "dpe_sel"),
	FMCLK(CKGEN,	11, "cam_sel"),
	FMCLK(CKGEN,	12, "ccu_sel"),
	FMCLK(CKGEN,	13, "dsp_sel"),
	FMCLK(CKGEN,	14, "dsp1_sel"),
	FMCLK(CKGEN,	15, "dsp2_sel"),
	FMCLK(CKGEN,	16, "dsp3_sel"),
	FMCLK(CKGEN,	17, "dsp4_sel"),
	FMCLK(CKGEN,	18, "dsp5_sel"),
	FMCLK(CKGEN,	19, "dsp6_sel"),
	FMCLK(CKGEN,	20, "dsp7_sel"),
	FMCLK(CKGEN,	21, "ipu_if_sel"),
	FMCLK(CKGEN,	22, "mfg_sel"),
	FMCLK(CKGEN,	23, "camtg_sel"),
	FMCLK(CKGEN,	24, "camtg2_sel"),
	FMCLK(CKGEN,	25, "camtg3_sel"),
	FMCLK(CKGEN,	26, "camtg4_sel"),
	FMCLK(CKGEN,	27, "uart_sel"),
	FMCLK(CKGEN,	28, "spi_sel"),
	FMCLK(CKGEN,	29, "msdc50_0_hclk_sel"),
	FMCLK(CKGEN,	30, "msdc50_0_sel"),
	FMCLK(CKGEN,	31, "msdc30_1_sel"),
	FMCLK(CKGEN,	32, "audio_sel"),
	FMCLK(CKGEN,	33, "aud_intbus_sel"),
	FMCLK(CKGEN,	34, "pwrap_ulposc_sel"),
	FMCLK(CKGEN,	35, "atb_sel"),
	FMCLK(CKGEN,	36, "p_w_r_mcu_sel"),
	FMCLK(CKGEN,	37, "dp_sel"),
	FMCLK(CKGEN,	38, "scam_sel"),
	FMCLK(CKGEN,	39, "disp_pwm_sel"),
	FMCLK(CKGEN,	40, "usb_top_sel"),
	FMCLK(CKGEN,	41, "ssusb_xhci_sel"),
	FMCLK(CKGEN,	42, "i2c_sel"),
	FMCLK(CKGEN,	43, "seninf_sel"),
	FMCLK(CKGEN,	44, "sspm_sel"),
	FMCLK(CKGEN,	45, "spmi_mst_sel"),
	FMCLK(CKGEN,	46, "dvfsrc_sel"),
	FMCLK(CKGEN,	47, "dxcc_sel"),
	FMCLK(CKGEN,	48, "aud_engen1_sel"),
	FMCLK(CKGEN,	49, "aud_engen2_sel"),
	FMCLK(CKGEN,	50, "aes_ufsfde_sel"),
	FMCLK(CKGEN,	51, "ufs_sel"),
	FMCLK(CKGEN,	52, "aud_1_sel"),
	FMCLK(CKGEN,	53, "aud_2_sel"),
	FMCLK(CKGEN,	54, "adsp_sel"),
	FMCLK(CKGEN,	55, "dpmaif_main_sel"),
	FMCLK(CKGEN,	56, "venc_sel"),
	FMCLK(CKGEN,	57, "vdec_sel"),
	FMCLK(CKGEN,	58, "vdec_lat_sel"),
	FMCLK(CKGEN,	59, "camtm_sel"),
	FMCLK(CKGEN,	60, "pwm_sel"),
	FMCLK(CKGEN,	61, "audio_h_sel"),
	FMCLK(CKGEN,	62, "camtg5_sel"),
	FMCLK(CKGEN,	63, "camtg6_sel"),

	FMCLK(ABIST,	1,	"AD_ADSPPLL_CK"),
	FMCLK(ABIST,	2,	"AD_APLL1_CK"),
	FMCLK(ABIST,	3,	"AD_APLL2_CK"),
	FMCLK(ABIST,	4,	"AD_APPLLGP_MON_FM_CK"),
	FMCLK(ABIST,	5,	"AD_APUPLL_CK"),
	FMCLK(ABIST,	6,	"AD_ARMPLL_BL0_CK"),
	FMCLK(ABIST,	7,	"AD_ARMPLL_BL1_CK"),
	FMCLK(ABIST,	8,	"AD_ARMPLL_BL2_CK"),
	FMCLK(ABIST,	9,	"AD_ARMPLL_BL3_CK"),
	FMCLK(ABIST,	10,	"AD_ARMPLL_LL_CK"),
	FMCLK(ABIST,	11,	"AD_CCIPLL_CK"),
	FMCLK(ABIST,	12,	"AD_CSI0A_CDPHY_DELAYCAL_CK"),
	FMCLK(ABIST,	13,	"AD_CSI0B_CDPHY_DELAYCAL_CK"),
	FMCLK(ABIST,	14,	"AD_CSI1A_DPHY_DELAYCAL_CK"),
	FMCLK(ABIST,	15,	"AD_CSI1B_DPHY_DELAYCAL_CK"),
	FMCLK(ABIST,	16,	"AD_CSI2A_DPHY_DELAYCAL_CK"),
	FMCLK(ABIST,	17,	"AD_CSI2B_DPHY_DELAYCAL_CK"),
	FMCLK(ABIST,	18,	"AD_CSI3A_DPHY_DELAYCAL_CK"),
	FMCLK(ABIST,	19,	"AD_CSI3B_DPHY_DELAYCAL_CK"),
	FMCLK(ABIST,	20,	"AD_DSI0_LNTC_DSICLK"),
	FMCLK(ABIST,	21,	"AD_DSI0_MPPLL_TST_CK"),
	FMCLK(ABIST,	22,	"AD_DSI1_LNTC_DSICLK"),
	FMCLK(ABIST,	23,	"AD_DSI1_MPPLL_TST_CK"),
	FMCLK(ABIST,	24,	"AD_MAINPLL_CK"),
	FMCLK(ABIST,	25,	"AD_MDPLL1_FS26M_CK_guide"),
	FMCLK(ABIST,	26,	"AD_MFGPLL_CK"),
	FMCLK(ABIST,	27,	"AD_MMPLL_CK"),
	FMCLK(ABIST,	28,	"AD_MMPLL_D3_CK"),
	FMCLK(ABIST,	29,	"AD_MPLL_CK"),
	FMCLK(ABIST,	30,	"AD_MSDCPLL_CK"),
	FMCLK(ABIST,	31,	"AD_RCLRPLL_DIV4_CK_ch02"),
	FMCLK(ABIST,	32,	"AD_RCLRPLL_DIV4_CK_ch13"),
	FMCLK(ABIST,	33,	"AD_RPHYPLL_DIV4_CK_ch02"),
	FMCLK(ABIST,	34,	"AD_RPHYPLL_DIV4_CK_ch13"),
	FMCLK(ABIST,	35,	"AD_TVDPLL_CK"),
	FMCLK(ABIST,	36,	"AD_ULPOSC2_CK"),
	FMCLK(ABIST,	37,	"AD_ULPOSC_CK"),
	FMCLK(ABIST,	38,	"AD_UNIVPLL_CK"),
	FMCLK(ABIST,	39,	"AD_USB20_192M_CK"),
	FMCLK(ABIST,	40,	"DA_MPLL_52M_DIV_CK"),
	FMCLK(ABIST,	41,	"UFS_MP_CLK2FREQ"),
	FMCLK(ABIST,	42,	"ad_wbg_dig_bpll_ck"),
	FMCLK(ABIST,	43,	"ad_wbg_dig_wpll_ck960"),
	FMCLK(ABIST,	44,	"fmem_ck_aft_dcm_ch0"),
	FMCLK(ABIST,	45,	"fmem_ck_aft_dcm_ch1"),
	FMCLK(ABIST,	46,	"fmem_ck_aft_dcm_ch2"),
	FMCLK(ABIST,	47,	"fmem_ck_aft_dcm_ch3"),
	FMCLK(ABIST,	48,	"fmem_ck_bfe_dcm_ch0"),
	FMCLK(ABIST,	49,	"fmem_ck_bfe_dcm_ch1"),
	FMCLK(ABIST,	50,	"hd_466m_fmem_ck_infrasys"),
	FMCLK(ABIST,	51,	"mcusys_arm_clk_out_all"),
	FMCLK(ABIST,	52,	"msdc01_in_ck"),
	FMCLK(ABIST,	53,	"msdc02_in_ck"),
	FMCLK(ABIST,	54,	"msdc11_in_ck"),
	FMCLK(ABIST,	55,	"msdc12_in_ck"),
	FMCLK(ABIST,	56,	"msdc21_in_ck"),
	FMCLK(ABIST,	57,	"msdc22_in_ck"),
	FMCLK(ABIST,	58,	"rtc32k_ck_i_vao"),
	FMCLK(ABIST,	60,	"ckomo1_ck"),
	FMCLK(ABIST,	61,	"ckmon2_ck"),
	FMCLK(ABIST,	62,	"ckmon3_ck"),
	FMCLK(ABIST,	63,	"ckmon4_ck"),
	{}
};

#define _CKGEN(x)		(rb[topckgen].virt + (x))
#define CLK_CFG_0		_CKGEN(0x10)
#define CLK_CFG_1		_CKGEN(0x20)
#define CLK_CFG_2		_CKGEN(0x30)
#define CLK_CFG_3		_CKGEN(0x40)
#define CLK_CFG_4		_CKGEN(0x50)
#define CLK_CFG_5		_CKGEN(0x60)
#define CLK_CFG_6		_CKGEN(0x70)
#define CLK_CFG_7		_CKGEN(0x80)
#define CLK_CFG_8		_CKGEN(0x90)
#define CLK_CFG_9		_CKGEN(0xA0)
#define CLK_CFG_10		_CKGEN(0xB0)
#define CLK_CFG_11		_CKGEN(0xC0)
#define CLK_CFG_12		_CKGEN(0xD0)
#define CLK_CFG_13		_CKGEN(0xE0)
#define CLK_MISC_CFG_0		_CKGEN(0x140)
#define CLK_DBG_CFG		_CKGEN(0x17C)
#define CLK26CALI_0		_CKGEN(0x220)
#define CLK26CALI_1		_CKGEN(0x224)

#define _SCPSYS(x)		(rb[scpsys].virt + (x))
#define SPM_PWR_STATUS		_SCPSYS(0x16C)
#define SPM_PWR_STATUS_2ND	_SCPSYS(0x170)

#ifdef CONFIG_MTK_DEVAPC
static void devapc_dump_regs(void)
{
	int i = 0;

	pr_notice("[devapc] CLK_CFG_0-16\r\n");
	for (i = 0; i < 17; i++)
		pr_notice("[%d]0x%08x\r\n", i, clk_readl(CLK_CFG_0 + (i << 4)));
	pr_notice("[devapc] PWR_STATUS(0x160,0x164) = 0x%08x 0x%08x\n",
		clk_readl(SPM_PWR_STATUS), clk_readl(SPM_PWR_STATUS_2ND));

	pr_notice("====================== [devapc clk subsys dump] =======================\r\n");
	print_subsys_reg("topckgen");
	print_subsys_reg("infracfg");
	print_subsys_reg("scpsys");
	print_subsys_reg("apmixed");
}

static struct devapc_vio_callbacks devapc_vio_handle = {
	.id = DEVAPC_SUBSYS_CLKMGR,
	.debug_dump = devapc_dump_regs,
};
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

static unsigned int mt_get_ckgen_freq(unsigned int ID)
{
	int output = 0, i = 0;
	unsigned int temp, clk_dbg_cfg, clk_misc_cfg_0, clk26cali_1 = 0;

	clk_dbg_cfg = clk_readl(CLK_DBG_CFG);
	clk_writel(CLK_DBG_CFG, (clk_dbg_cfg & 0xFFFFC0FC)|(ID << 8)|(0x1));

	clk_misc_cfg_0 = clk_readl(CLK_MISC_CFG_0);
	clk_writel(CLK_MISC_CFG_0, (clk_misc_cfg_0 & 0x00FFFFFF));

	clk26cali_1 = clk_readl(CLK26CALI_1);
	clk_writel(CLK26CALI_0, 0x1000);
	clk_writel(CLK26CALI_0, 0x1010);

	/* wait frequency meter finish */
	while (clk_readl(CLK26CALI_0) & 0x10) {
		udelay(10);
		i++;
		if (i > 20)
			break;
	}

	temp = clk_readl(CLK26CALI_1) & 0xFFFF;

	output = (temp * 26000) / 1024;

	clk_writel(CLK_DBG_CFG, clk_dbg_cfg);
	clk_writel(CLK_MISC_CFG_0, clk_misc_cfg_0);
	/*clk_writel(CLK26CALI_0, clk26cali_0);*/
	/*clk_writel(CLK26CALI_1, clk26cali_1);*/

	clk_writel(CLK26CALI_0, 0x0000);
	/*print("ckgen meter[%d] = %d Khz\n", ID, output);*/
	if (i > 20)
		return 0;
	else
		return output;

}

static unsigned int mt_get_abist_freq(unsigned int ID)
{
	int output = 0, i = 0;
	unsigned int temp, clk_dbg_cfg, clk_misc_cfg_0, clk26cali_1 = 0;

	clk_dbg_cfg = clk_readl(CLK_DBG_CFG);
	clk_writel(CLK_DBG_CFG, (clk_dbg_cfg & 0xFFC0FFFC)|(ID << 16));

	clk_misc_cfg_0 = clk_readl(CLK_MISC_CFG_0);
	clk_writel(CLK_MISC_CFG_0, (clk_misc_cfg_0 & 0x00FFFFFF) | (1 << 24));

	clk26cali_1 = clk_readl(CLK26CALI_1);

	clk_writel(CLK26CALI_0, 0x1000);
	clk_writel(CLK26CALI_0, 0x1010);

	/* wait frequency meter finish */
	while (clk_readl(CLK26CALI_0) & 0x10) {
		udelay(10);
		i++;
		if (i > 20)
			break;
	}

	temp = clk_readl(CLK26CALI_1) & 0xFFFF;

	output = (temp * 26000) / 1024;

	clk_writel(CLK_DBG_CFG, clk_dbg_cfg);
	clk_writel(CLK_MISC_CFG_0, clk_misc_cfg_0);
	/*clk_writel(CLK26CALI_0, clk26cali_0);*/
	/*clk_writel(CLK26CALI_1, clk26cali_1);*/
	clk_writel(CLK26CALI_0, 0x0000);
	/*pr_debug("%s = %d Khz\n", abist_array[ID-1], output);*/
	if (i > 20)
		return 0;
	else
		return (output * 2);
}

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
extern const char * const *get_mt6885_all_clk_names(void);

static const char * const *get_all_clk_names(void)
{
	return get_mt6885_all_clk_names();
}

/*
 * clkdbg pwr_status
 */
static const char * const *get_pwr_names(void)
{
	static const char * const pwr_names[] = {
		[0] = "MD",
		[1] = "CONN",
		[2] = "MFG0",
		[3] = "MFG1",
		[4] = "MFG2",
		[5] = "MFG3",
		[6] = "MFG4",
		[7] = "MFG5",
		[8] = "MFG6",
		[9] = "INFRA",
		[10] = "SUB_INFRA",
		[11] = "DDRPHY",
		[12] = "ISP",
		[13] = "ISP2",
		[14] = "IPE",
		[15] = "VDEC",
		[16] = "VDEC2",
		[17] = "VEN",
		[18] = "VEN_CORE1",
		[19] = "MDP",
		[20] = "DISP",
		[21] = "AUDIO",
		[22] = "ADSP",
		[23] = "CAM",
		[24] = "CAM_RAWA",
		[25] = "CAM_RAWB",
		[26] = "CAM_RAWC",
		[27] = "DP_TX",
		[28] = "DDRPHY2",
		[29] = "(Reserved)",
		[30] = "(Reserved)",
		[31] = "(Reserved)",
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
#if 0
#include <mt-plat/mtk_chip.h>
#endif
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

static struct clkdbg_ops clkdbg_mt6885_ops = {
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

static int __init clkdbg_mt6885_init(void)
{
	if (!of_machine_is_compatible("mediatek,MT6885"))
		return -ENODEV;

	init_regbase();

	init_custom_cmds();
	set_clkdbg_ops(&clkdbg_mt6885_ops);

#ifdef CONFIG_MTK_DEVAPC
	register_devapc_vio_callback(&devapc_vio_handle);
#endif

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
device_initcall(clkdbg_mt6885_init);

/*
 * MT6885: for mtcmos debug
 */
static struct regbase rb_mt6885[] = {
	[topckgen] = REGBASE_V(0x10000000, topckgen),
	[infracfg] = REGBASE_V(0x10001000, infracfg),
	[scpsys]   = REGBASE_V(0x10006000, scpsys),
	[apmixed]  = REGBASE_V(0x1000c000, apmixed),
	[audio]    = REGBASE_V(0x11210000, audio),
	[mfgsys]   = REGBASE_V(0x13fbf000, mfgsys),
	[mmsys]    = REGBASE_V(0x14116000, mmsys),
	[mdpsys]    = REGBASE_V(0x1F000000, mdpsys),
	[img1sys]   = REGBASE_V(0x15020000, img1sys),
	[img2sys]   = REGBASE_V(0x15820000, img2sys),
	[ipesys]   = REGBASE_V(0x1b000000, ipesys),
	[camsys]   = REGBASE_V(0x1a000000, camsys),
	[cam_rawa_sys]   = REGBASE_V(0x1a04f000, cam_rawa_sys),
	[cam_rawb_sys]   = REGBASE_V(0x1a06f000, cam_rawb_sys),
	[cam_rawc_sys]   = REGBASE_V(0x1a08f000, cam_rawc_sys),
	[vencsys]  = REGBASE_V(0x17000000, vencsys),
	[venc_c1_sys]  = REGBASE_V(0x17800000, venc_c1_sys),
	[vdecsys]  = REGBASE_V(0x1602f000, vdecsys),
	[vdec_soc_sys]  = REGBASE_V(0x1600f000, vdec_soc_sys),
	[ipu_vcore]  = REGBASE_V(0x19029000, ipu_vcore),
	[ipu_conn]  = REGBASE_V(0x19020000, ipu_conn),
	[ipu0]  = REGBASE_V(0x19030000, ipu0),
	[ipu1]  = REGBASE_V(0x19031000, ipu1),
	[ipu2]  = REGBASE_V(0x19032000, ipu2),
};

#define REGNAME_MT6885(_base, _ofs, _name)	\
	{ .base = &rb_mt6885[_base], .ofs = _ofs, .name = #_name }

static struct regname rn_mt6885[] = {
	REGNAME_MT6885(topckgen,  0x010, CLK_CFG_0),
	REGNAME_MT6885(topckgen,  0x020, CLK_CFG_1),
	REGNAME_MT6885(topckgen,  0x030, CLK_CFG_2),
	REGNAME_MT6885(topckgen,  0x040, CLK_CFG_3),
	REGNAME_MT6885(topckgen,  0x050, CLK_CFG_4),
	REGNAME_MT6885(topckgen,  0x060, CLK_CFG_5),
	REGNAME_MT6885(topckgen,  0x070, CLK_CFG_6),
	REGNAME_MT6885(topckgen,  0x080, CLK_CFG_7),
	REGNAME_MT6885(topckgen,  0x090, CLK_CFG_8),
	REGNAME_MT6885(topckgen,  0x0A0, CLK_CFG_9),
	REGNAME_MT6885(topckgen,  0x0B0, CLK_CFG_10),
	REGNAME_MT6885(topckgen,  0x0C0, CLK_CFG_11),
	REGNAME_MT6885(topckgen,  0x0D0, CLK_CFG_12),
	REGNAME_MT6885(topckgen,  0x0E0, CLK_CFG_13),
	REGNAME_MT6885(topckgen,  0x0F0, CLK_CFG_14),
	REGNAME_MT6885(topckgen,  0x100, CLK_CFG_15),
	REGNAME_MT6885(topckgen,  0x110, CLK_CFG_16),

	REGNAME_MT6885(apmixed, 0x050, PLLON_CON0),
	REGNAME_MT6885(apmixed, 0x054, PLLON_CON1),
	REGNAME_MT6885(apmixed, 0x058, PLLON_CON2),
	REGNAME_MT6885(apmixed, 0x05C, PLLON_CON3),
	REGNAME_MT6885(apmixed, 0x208, ARMPLL_LL_CON0),
	REGNAME_MT6885(apmixed, 0x20C, ARMPLL_LL_CON1),
	REGNAME_MT6885(apmixed, 0x210, ARMPLL_LL_CON2),
	REGNAME_MT6885(apmixed, 0x214, ARMPLL_LL_CON3),
	REGNAME_MT6885(apmixed, 0x218, ARMPLL_BL0_CON0),
	REGNAME_MT6885(apmixed, 0x21C, ARMPLL_BL0_CON1),
	REGNAME_MT6885(apmixed, 0x220, ARMPLL_BL0_CON2),
	REGNAME_MT6885(apmixed, 0x224, ARMPLL_BL0_CON3),
	REGNAME_MT6885(apmixed, 0x228, ARMPLL_BL1_CON0),
	REGNAME_MT6885(apmixed, 0x22C, ARMPLL_BL1_CON1),
	REGNAME_MT6885(apmixed, 0x230, ARMPLL_BL1_CON2),
	REGNAME_MT6885(apmixed, 0x234, ARMPLL_BL1_CON3),
	REGNAME_MT6885(apmixed, 0x238, ARMPLL_BL2_CON0),
	REGNAME_MT6885(apmixed, 0x23C, ARMPLL_BL2_CON1),
	REGNAME_MT6885(apmixed, 0x240, ARMPLL_BL2_CON2),
	REGNAME_MT6885(apmixed, 0x244, ARMPLL_BL2_CON3),
	REGNAME_MT6885(apmixed, 0x248, ARMPLL_BL3_CON0),
	REGNAME_MT6885(apmixed, 0x24C, ARMPLL_BL3_CON1),
	REGNAME_MT6885(apmixed, 0x250, ARMPLL_BL3_CON2),
	REGNAME_MT6885(apmixed, 0x254, ARMPLL_BL3_CON3),
	REGNAME_MT6885(apmixed, 0x258, CCIPLL_CON0),
	REGNAME_MT6885(apmixed, 0x25C, CCIPLL_CON1),
	REGNAME_MT6885(apmixed, 0x260, CCIPLL_CON2),
	REGNAME_MT6885(apmixed, 0x264, CCIPLL_CON3),
	REGNAME_MT6885(apmixed, 0x268, MFGPLL_CON0),
	REGNAME_MT6885(apmixed, 0x26C, MFGPLL_CON1),
	REGNAME_MT6885(apmixed, 0x274, MFGPLL_CON3),
	REGNAME_MT6885(apmixed, 0x308, UNIVPLL_CON0),
	REGNAME_MT6885(apmixed, 0x30C, UNIVPLL_CON1),
	REGNAME_MT6885(apmixed, 0x314, UNIVPLL_CON3),
	REGNAME_MT6885(apmixed, 0x318, APLL1_CON0),
	REGNAME_MT6885(apmixed, 0x31C, APLL1_CON1),
	REGNAME_MT6885(apmixed, 0x320, APLL1_CON2),
	REGNAME_MT6885(apmixed, 0x324, APLL1_CON3),
	REGNAME_MT6885(apmixed, 0x328, APLL1_CON4),
	REGNAME_MT6885(apmixed, 0x32C, APLL2_CON0),
	REGNAME_MT6885(apmixed, 0x330, APLL2_CON1),
	REGNAME_MT6885(apmixed, 0x334, APLL2_CON2),
	REGNAME_MT6885(apmixed, 0x338, APLL2_CON3),
	REGNAME_MT6885(apmixed, 0x33C, APLL2_CON4),
	REGNAME_MT6885(apmixed, 0x340, MAINPLL_CON0),
	REGNAME_MT6885(apmixed, 0x344, MAINPLL_CON1),
	REGNAME_MT6885(apmixed, 0x34C, MAINPLL_CON3),
	REGNAME_MT6885(apmixed, 0x350, MSDCPLL_CON0),
	REGNAME_MT6885(apmixed, 0x354, MSDCPLL_CON1),
	REGNAME_MT6885(apmixed, 0x35C, MSDCPLL_CON3),
	REGNAME_MT6885(apmixed, 0x360, MMPLL_CON0),
	REGNAME_MT6885(apmixed, 0x364, MMPLL_CON1),
	REGNAME_MT6885(apmixed, 0x36C, MMPLL_CON3),
	REGNAME_MT6885(apmixed, 0x370, ADSPPLL_CON0),
	REGNAME_MT6885(apmixed, 0x374, ADSPPLL_CON1),
	REGNAME_MT6885(apmixed, 0x37C, ADSPPLL_CON3),
	REGNAME_MT6885(apmixed, 0x380, TVDPLL_CON0),
	REGNAME_MT6885(apmixed, 0x384, TVDPLL_CON1),
	REGNAME_MT6885(apmixed, 0x38C, TVDPLL_CON3),
	REGNAME_MT6885(apmixed, 0x390, MPLL_CON0),
	REGNAME_MT6885(apmixed, 0x394, MPLL_CON1),
	REGNAME_MT6885(apmixed, 0x39C, MPLL_CON3),
	REGNAME_MT6885(apmixed, 0x3A0, APUPLL_CON0),
	REGNAME_MT6885(apmixed, 0x3A4, APUPLL_CON1),
	REGNAME_MT6885(apmixed, 0x3AC, APUPLL_CON3),

	REGNAME_MT6885(scpsys, 0x0000, POWERON_CONFIG_EN),
	REGNAME_MT6885(scpsys, 0x016C, PWR_STATUS),
	REGNAME_MT6885(scpsys, 0x0170, PWR_STATUS_2ND),
	REGNAME_MT6885(scpsys, 0x0178, OTHER_PWR_STATUS),
	REGNAME_MT6885(scpsys, 0x300, MD1_PWR_CON),
	REGNAME_MT6885(scpsys, 0x304, CONN_PWR_CON),
	REGNAME_MT6885(scpsys, 0x308, MFG0_PWR_CON),
	REGNAME_MT6885(scpsys, 0x30C, MFG1_PWR_CON),
	REGNAME_MT6885(scpsys, 0x310, MFG2_PWR_CON),
	REGNAME_MT6885(scpsys, 0x314, MFG3_PWR_CON),
	REGNAME_MT6885(scpsys, 0x318, MFG4_PWR_CON),
	REGNAME_MT6885(scpsys, 0x31C, MFG5_PWR_CON),
	REGNAME_MT6885(scpsys, 0x320, MFG6_PWR_CON),
	REGNAME_MT6885(scpsys, 0x324, IFR_PWR_CON),
	REGNAME_MT6885(scpsys, 0x328, IFR_SUB_PWR_CON),
	REGNAME_MT6885(scpsys, 0x32C, DPY_PWR_CON),
	REGNAME_MT6885(scpsys, 0x330, ISP_PWR_CON),
	REGNAME_MT6885(scpsys, 0x334, ISP2_PWR_CON),
	REGNAME_MT6885(scpsys, 0x338, IPE_PWR_CON),
	REGNAME_MT6885(scpsys, 0x33C, VDE_PWR_CON),
	REGNAME_MT6885(scpsys, 0x340, VDE2_PWR_CON),
	REGNAME_MT6885(scpsys, 0x344, VEN_PWR_CON),
	REGNAME_MT6885(scpsys, 0x348, VEN_CORE1_PWR_CON),
	REGNAME_MT6885(scpsys, 0x34C, MDP_PWR_CON),
	REGNAME_MT6885(scpsys, 0x350, DIS_PWR_CON),
	REGNAME_MT6885(scpsys, 0x354, AUDIO_PWR_CON),
	REGNAME_MT6885(scpsys, 0x358, ADSP_PWR_CON),
	REGNAME_MT6885(scpsys, 0x35C, CAM_PWR_CON),
	REGNAME_MT6885(scpsys, 0x360, CAM_RAWA_PWR_CON),
	REGNAME_MT6885(scpsys, 0x364, CAM_RAWB_PWR_CON),
	REGNAME_MT6885(scpsys, 0x368, CAM_RAWC_PWR_CON),
	REGNAME_MT6885(scpsys, 0x3AC, DP_TX_PWR_CON),
	REGNAME_MT6885(scpsys, 0x3C4, DPY2_PWR_CON),
	REGNAME_MT6885(scpsys, 0x398, MD_EXT_BUCK_ISO_CON),

	REGNAME_MT6885(audio, 0x0000, AUDIO_TOP_CON0),
	REGNAME_MT6885(audio, 0x0004, AUDIO_TOP_CON1),
	REGNAME_MT6885(audio, 0x0008, AUDIO_TOP_CON2),

	REGNAME_MT6885(camsys, 0x0000, CAMSYS_CG_CON),
	REGNAME_MT6885(cam_rawa_sys, 0x0000, CAMSYS_RAWA_CG_CON),
	REGNAME_MT6885(cam_rawb_sys, 0x0000, CAMSYS_RAWB_CG_CON),
	REGNAME_MT6885(cam_rawc_sys, 0x0000, CAMSYS_RAWC_CG_CON),

	REGNAME_MT6885(img1sys, 0x0000, IMG1_CG_CON),
	REGNAME_MT6885(img2sys, 0x0000, IMG2_CG_CON),
	REGNAME_MT6885(ipesys, 0x0000, IPE_CG_CON),

	REGNAME_MT6885(infracfg,  0x090, MODULE_SW_CG_0),
	REGNAME_MT6885(infracfg,  0x094, MODULE_SW_CG_1),
	REGNAME_MT6885(infracfg,  0x0ac, MODULE_SW_CG_2),
	REGNAME_MT6885(infracfg,  0x0c8, MODULE_SW_CG_3),
	REGNAME_MT6885(infracfg,  0xe98, MM_HANG_FREE),
#if 0
/*
 *	REGNAME_MT6885(ipu0,  0x000, IPU0_CORE_CG),
 *	REGNAME_MT6885(ipu1,  0x000, IPU1_CORE_CG),
 *	REGNAME_MT6885(ipu2,  0x000, IPU2_CORE_CG),
 *	REGNAME_MT6885(ipu_conn,  0x000, IPU_CONN_CG),
 *	REGNAME_MT6885(ipu_vcore,  0x000, IPU_VCORE_CG),
 */
#endif

	REGNAME_MT6885(mfgsys, 0x0000, MFG_CG_CON),

	REGNAME_MT6885(mmsys, 0x100, MM_CG_CON0),
	REGNAME_MT6885(mmsys, 0x110, MM_CG_CON1),
	REGNAME_MT6885(mmsys, 0x1a0, MM_CG_CON2),

	REGNAME_MT6885(mdpsys, 0x100, MDP_CG_CON0),
	REGNAME_MT6885(mdpsys, 0x104, MDP_CG_SET0),
	REGNAME_MT6885(mdpsys, 0x114, MDP_CG_SET1),
	REGNAME_MT6885(mdpsys, 0x124, MDP_CG_SET2),


	REGNAME_MT6885(vdecsys, 0x0000, VDEC_CKEN_SET),
	REGNAME_MT6885(vdecsys, 0x0008, VDEC_LARB1_CKEN_SET),
	REGNAME_MT6885(vdecsys, 0x0200, VDEC_LAT_CKEN_SET),

	REGNAME_MT6885(vdec_soc_sys, 0x0000, VDEC_SOC_CKEN_SET),
	REGNAME_MT6885(vdec_soc_sys, 0x0008, VDEC_SOC_LARB1_CKEN_SET),
	REGNAME_MT6885(vdec_soc_sys, 0x0200, VDEC_SOC_LAT_CKEN_SET),

	REGNAME_MT6885(venc_c1_sys, 0x0000, VENC_C1_CG_CON),

	REGNAME_MT6885(vencsys, 0x0000, VENC_CG_CON),
	{}
};



static bool is_valid_reg(void __iomem *addr)
{
#ifdef CONFIG_64BIT
	return ((u64)addr & 0xf0000000) != 0UL ||
			(((u64)addr >> 32U) & 0xf0000000) != 0UL;
#else
	return ((u32)addr & 0xf0000000) != 0U;
#endif
}

void init_regbase_mt6885(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(rb_mt6885); i++)
		rb_mt6885[i].virt = ioremap_nocache(rb_mt6885[i].phys,
								PAGE_SIZE);
}

static struct regbase *lookup_regbase(char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(rb_mt6885); i++) {
		if (!strcmp(name, rb_mt6885[i].name))
			return &rb_mt6885[i];
	}
	return NULL;
}

void print_subsys_reg(char *subsys_name)
{
	struct regbase *rb;
	const struct regname *rns = &rn_mt6885[0];

	/*
	 * if (rns == NULL)
	 *	return;
	 */

	rb = lookup_regbase(subsys_name);
	if (rb == NULL) {
		pr_info("wrong regbase name:%s\n", subsys_name);
		return;
	}

	for (; rns->base != NULL; rns++) {
		if (!is_valid_reg(ADDR(rns)))
			return;

		/* filter out the subsys that we don't want */
		if (rns->base != rb)
			continue;

		pr_info("%-18s: [0x%08x] = 0x%08x\n",
			rns->name, PHYSADDR(rns), clk_readl(ADDR(rns)));
	}
}
