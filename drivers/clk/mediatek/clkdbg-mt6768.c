/*
 * Copyright (c) 2018 MediaTek Inc.
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

#include <mt-plat/devapc_public.h>

#include "clkdbg.h"
#include "clkchk.h"
#include "clkchk-mt6768.h"

#define DUMP_INIT_STATE		0

/*
 * clkdbg dump_regs
 */

enum {
	topckgen,
	infracfg,
	pericfg,
	scpsys,
	apmixedsys,
	audiosys,
	gce,
	mipi_0a,
	mipi_0b,
	mipi_1a,
	mipi_1b,
	mipi_2a,
	mipi_2b,
	mfgsys,
	mmsys,
	imgsys,
	camsys,
	vencsys,
	vdecsys,
};

#define REGBASE_V(_phys, _id_name) { .phys = _phys, .name = #_id_name }

/*
 * checkpatch.pl ERROR:COMPLEX_MACRO
 *
 * #define REGBASE(_phys, _id_name) [_id_name] = REGBASE_V(_phys, _id_name)
 */

static struct regbase rb[] = {
	[topckgen]	= REGBASE_V(0x10000000, topckgen),
	[infracfg]	= REGBASE_V(0x10001000, infracfg),
	[pericfg]	= REGBASE_V(0x10003000, pericfg),
	[scpsys]	= REGBASE_V(0x10006000, scpsys),
	[apmixedsys]	= REGBASE_V(0x1000c000, apmixedsys),
	[audiosys]	= REGBASE_V(0x11220000, audiosys),
	[gce]		= REGBASE_V(0x10238000, gce),
	[audiosys]	= REGBASE_V(0x11220000, audio),
	[mipi_0a]	= REGBASE_V(0x11c10000, mipi_0a),
	[mipi_0b]	= REGBASE_V(0x11c11000, mipi_0b),
	[mipi_1a]	= REGBASE_V(0x11c12000, mipi_1a),
	[mipi_1b]	= REGBASE_V(0x11c13000, mipi_1b),
	[mipi_2a]	= REGBASE_V(0x11c14000, mipi_2a),
	[mipi_2b]	= REGBASE_V(0x11c15000, mipi_2b),
	[mfgsys]	= REGBASE_V(0x13000000, mfgsys),
	[mmsys]		= REGBASE_V(0x14000000, mmsys),
	[imgsys]	= REGBASE_V(0x15020000, imgsys),
	[camsys]	= REGBASE_V(0x1a000000, camsys),
	[vencsys]	= REGBASE_V(0x17000000, vencsys),
	[vdecsys]	= REGBASE_V(0x16000000, vdecsys),
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
	REGNAME(audiosys,  0x000, AUDIO_TOP_CON0),
	REGNAME(audiosys,  0x004, AUDIO_TOP_CON1),
	REGNAME(camsys,  0x000, CAMSYS_CG),
	REGNAME(imgsys,  0x000, IMG_CG),
	REGNAME(infracfg,  0x074, PERI_BUS_DCM_CTRL),
	REGNAME(infracfg,  0x090, MODULE_SW_CG_0),
	REGNAME(infracfg,  0x094, MODULE_SW_CG_1),
	REGNAME(infracfg,  0x0ac, MODULE_SW_CG_2),
	REGNAME(infracfg,  0x0c8, MODULE_SW_CG_3),
	REGNAME(infracfg,  0x200, INFRA_TOPAXI_SI0_CTL),
	REGNAME(pericfg,  0x20C, PERIAXI_SI0_CTL),
	REGNAME(gce,  0x0F0, GCE_CTL_INT0),
	REGNAME(mfgsys,  0x000, MFG_CG),
	REGNAME(mmsys,  0x100, MMSYS_CG_CON0),
	REGNAME(vencsys,  0x000, VENCSYS_CG),
	REGNAME(vdecsys,  0x000, VDECSYS_CG_0),
	REGNAME(vdecsys,  0x008, VDECSYS_CG_1),
	REGNAME(mipi_0a,  0x080, MIPI_RX_WRAPPER80_CSI0A),
	REGNAME(mipi_0b,  0x080, MIPI_RX_WRAPPER80_CSI0A),
	REGNAME(mipi_1a,  0x080, MIPI_RX_WRAPPER80_CSI0A),
	REGNAME(mipi_1b,  0x080, MIPI_RX_WRAPPER80_CSI0A),
	REGNAME(mipi_2a,  0x080, MIPI_RX_WRAPPER80_CSI0A),
	REGNAME(mipi_2b,  0x080, MIPI_RX_WRAPPER80_CSI0A),

	REGNAME(apmixedsys,  0x208, ARMPLL_CON0),
	REGNAME(apmixedsys,  0x20C, ARMPLL_CON1),
	REGNAME(apmixedsys,  0x214, ARMPLL_PWR_CON0),
	REGNAME(apmixedsys,  0x218, ARMPLL_L_CON0),
	REGNAME(apmixedsys,  0x21C, ARMPLL_L_CON1),
	REGNAME(apmixedsys,  0x224, ARMPLL_L_PWR_CON0),
	REGNAME(apmixedsys,  0x228, CCIPLL_CON0),
	REGNAME(apmixedsys,  0x22C, CCIPLL_CON1),
	REGNAME(apmixedsys,  0x234, CCIPLL_PWR_CON0),
	REGNAME(apmixedsys,  0x238, UNIVPLL_CON0),
	REGNAME(apmixedsys,  0x23C, UNIVPLL_CON1),
	REGNAME(apmixedsys,  0x244, UNIVPLL_PWR_CON0),
	REGNAME(apmixedsys,  0x248, MFGPLL_CON0),
	REGNAME(apmixedsys,  0x24C, MFGPLL_CON1),
	REGNAME(apmixedsys,  0x254, MFGPLL_PWR_CON0),
	REGNAME(apmixedsys,  0x258, MAINPLL_CON0),
	REGNAME(apmixedsys,  0x25C, MAINPLL_CON1),
	REGNAME(apmixedsys,  0x264, MAINPLL_PWR_CON0),
	REGNAME(apmixedsys,  0x308, APLL1_CON0),
	REGNAME(apmixedsys,  0x30C, APLL1_CON1),
	REGNAME(apmixedsys,  0x314, APLL1_CON3),
	REGNAME(apmixedsys,  0x318, APLL1_PWR_CON0),
	REGNAME(apmixedsys,  0x31C, MMPLL_CON0),
	REGNAME(apmixedsys,  0x320, MMPLL_CON1),
	REGNAME(apmixedsys,  0x328, MMPLL_PWR_CON0),
	REGNAME(apmixedsys,  0x32C, MPLL_CON0),
	REGNAME(apmixedsys,  0x330, MPLL_CON1),
	REGNAME(apmixedsys,  0x338, MPLL_PWR_CON0),
	REGNAME(apmixedsys,  0x33C, MSDCPLL_CON0),
	REGNAME(apmixedsys,  0x340, MSDCPLL_CON1),
	REGNAME(apmixedsys,  0x348, MSDCPLL_PWR_CON0),

	REGNAME(scpsys,  0x0180, PWR_STATUS),
	REGNAME(scpsys,  0x0184, PWR_STATUS_2ND),
	REGNAME(scpsys,  0x0300, VCODEC_PWR_CON),
	REGNAME(scpsys,  0x0304, VEN_PWR_CON),
	REGNAME(scpsys,  0x0308, ISP_PWR_CON),
	REGNAME(scpsys,  0x030C, DIS_PWR_CON),
	REGNAME(scpsys,  0x0310, MFG_CORE1_PWR_CON),
	REGNAME(scpsys,  0x0318, IFR_PWR_CON),
	REGNAME(scpsys,  0x031C, DPY_PWR_CON),
	REGNAME(scpsys,  0x0320, MD1_PWR_CON),
	REGNAME(scpsys,  0x032C, CONN_PWR_CON),
	REGNAME(scpsys,  0x0334, MFG_ASYNC_PWR_CON),
	REGNAME(scpsys,  0x0338, MFG_PWR_CON),
	REGNAME(scpsys,  0x0344, CAM_PWR_CON),
	REGNAME(scpsys,  0x034C, MFG_CORE0_PWR_CON),
	REGNAME(scpsys,  0x0370, VDE_PWR_CON),
	{}
};

static const struct regname *get_all_regnames(void)
{
	return rn;
}

static void __init init_regbase(void)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(rb); i++)
		rb[i].virt = ioremap(rb[i].phys, PAGE_SIZE);
}

/*
 * clkdbg fmeter
 */

#include <linux/delay.h>

#define clk_readl(addr)		readl(addr)
#define clk_writel(addr, val)	\
	do { writel(val, addr); wmb(); } while (0) /* sync write */

#define FMCLK(_t, _i, _n) { .type = _t, .id = _i, .name = _n }

static const struct fmeter_clk fclks[] = {

	FMCLK(CKGEN,  1, "hd_faxi_ck"),
	FMCLK(CKGEN,  2, "hf_fmem_ck"),
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
	FMCLK(CKGEN,  31, "f_fvenc_ck"),
	FMCLK(CKGEN,  32, "f_cam_ck"),
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
	FMCLK(ABIST,  3, "UFS_MP_CLK2FREQ"),
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
	FMCLK(ABIST,  21, "AD_ARMPLL_L_CK"),
	FMCLK(ABIST,  22, "AD_ARMPLL_CK"),
	FMCLK(ABIST,  23, "AD_MAINPLL_1092M_CK"),
	FMCLK(ABIST,  24, "AD_UNIVPLL_1248M_CK"),
	FMCLK(ABIST,  25, "AD_MFGPLL_CK"),
	FMCLK(ABIST,  26, "AD_MSDCPLL_CK"),
	FMCLK(ABIST,  27, "AD_MMPLL_CK"),
	FMCLK(ABIST,  28, "AD_APLL1_196P608M_CK"),
	FMCLK(ABIST,  30, "AD_APPLLGP_TST_CK"),
	FMCLK(ABIST,  31, "AD_USB20_192M_CK"),
	FMCLK(ABIST,  34, "AD_VENCPLL_CK"),
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
	FMCLK(ABIST,  49, "AD_CCIPLL_CK"),
	FMCLK(ABIST,  50, "AD_MPLL_208M_CK"),
	FMCLK(ABIST,  51, "AD_WBG_DIG_CK_416M"),
	FMCLK(ABIST,  52, "AD_WBG_B_DIG_CK_64M"),
	FMCLK(ABIST,  53, "AD_WBG_W_DIG_CK_160M"),
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

#define _CKGEN(x)		(rb[topckgen].virt + (x))
#define CLK_CFG_0		_CKGEN(0x40)
#define CLK_CFG_1		_CKGEN(0x50)
#define CLK_CFG_2		_CKGEN(0x60)
#define CLK_CFG_3		_CKGEN(0x70)
#define CLK_CFG_4		_CKGEN(0x80)
#define CLK_CFG_5		_CKGEN(0x90)
#define CLK_CFG_6		_CKGEN(0xA0)
#define CLK_CFG_7		_CKGEN(0xB0)
#define CLK_MISC_CFG_0		_CKGEN(0x104)
#define CLK_DBG_CFG		_CKGEN(0x10C)
#define CLK26CALI_0		_CKGEN(0x220)
#define CLK26CALI_1		_CKGEN(0x224)

#define _SCPSYS(x)		(rb[scpsys].virt + (x))
#define SPM_PWR_STATUS		_SCPSYS(0x180)
#define SPM_PWR_STATUS_2ND	_SCPSYS(0x184)

#define _INFRA_AO(x)		(rb[infracfg].virt + (x))
#define INFRA_AO_0		_INFRA_AO(0x90)
#define INFRA_AO_1		_INFRA_AO(0x94)
#define INFRA_AO_2		_INFRA_AO(0xAC)
#define INFRA_AO_3		_INFRA_AO(0xC8)

static void devapc_dump_regs(void)
{
	print_enabled_clks();
	pr_notice("[devapc] CLK_CFG_0-7 = 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
		clk_readl(CLK_CFG_0), clk_readl(CLK_CFG_1),
		clk_readl(CLK_CFG_2), clk_readl(CLK_CFG_3),
		clk_readl(CLK_CFG_4), clk_readl(CLK_CFG_5),
		clk_readl(CLK_CFG_6), clk_readl(CLK_CFG_7));
	pr_notice("[devapc] PWR_STATUS(0x180,0x184) = 0x%08x 0x%08x\n",
		clk_readl(SPM_PWR_STATUS), clk_readl(SPM_PWR_STATUS_2ND));

	pr_notice("[devapc] INFRA_AO(0x90,0x94,0xAC,0xC8) = 0x%08x 0x%08x 0x%08x 0x%08x\n",
		clk_readl(INFRA_AO_0), clk_readl(INFRA_AO_1),
		clk_readl(INFRA_AO_2), clk_readl(INFRA_AO_3));
}

static struct devapc_vio_callbacks devapc_vio_handle = {
	.id = DEVAPC_SUBSYS_CLKMGR,
	.debug_dump = devapc_dump_regs,
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

	while (clk_readl(CLK26CALI_0) & 0x10) {
		mdelay(10);
		i++;
		if (i > 10)
			break;
	}

	temp = clk_readl(CLK26CALI_1) & 0xFFFF;

	output = (temp * 26000) / 1024;

	clk_writel(CLK_DBG_CFG, clk_dbg_cfg);
	clk_writel(CLK_MISC_CFG_0, clk_misc_cfg_0);
	clk_writel(CLK26CALI_0, 0x0);

	if (i > 10)
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

	while (clk_readl(CLK26CALI_0) & 0x10) {
		mdelay(10);
		i++;
		if (i > 10)
			break;
	}

	temp = clk_readl(CLK26CALI_1) & 0xFFFF;

	output = (temp * 26000) / 1024;

	clk_writel(CLK_DBG_CFG, clk_dbg_cfg);
	clk_writel(CLK_MISC_CFG_0, clk_misc_cfg_0);
	clk_writel(CLK26CALI_0, 0x0);

	if (i > 10)
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

static const struct fmeter_clk *get_all_fmeter_clks(void)
{
	return fclks;
}

/*
 * clkdbg dump_state
 */

static const char * const *get_all_clk_names(void)
{
	static const char * const clks[] = {
		/* PLLs */
		"armpll",
		"armpll_l",
		"ccipll",
		"mainpll",
		"univ2pll",
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
		"univpll",
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
		"mpll_104m_div",
		"mpll_52m_div",
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

		"msdc5hclk",
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

		"ulposc_sel",
		"camtm_sel",
		"venc_sel",
		"cam_sel",

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
		"aud_24m",
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
		"cam_larb3",
		"cam_dfp_vad",
		"cam",
		"camtg",
		"cam_seninf",
		"camsv0",
		"camsv1",
		"camsv2",
		"cam_ccu",

		/* IMG */
		"img_larb2",
		"img_dip",
		"img_fdvt",
		"img_dpe",
		"img_rsc",

		/* MFG */
		"mfgcfg_bg3d",

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
		"mm_disp_rsz0",
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
		"mm_smi_venc_ck",
		"mm_smi_vdec_ck",
		"mm_img_dl_relay",
		"mm_imgdl_async",
		"mm_dig_dsi_ck",
		"mm_hrtwt",

		/* VENC */
		"venc_set0_larb",
		"venc_set1_venc",
		"jpgenc",

		/* VDEC */
		"vdec_cken",
		"vdec_active",
		"vdec_cken_eng",
		"vdec_larb1_cken",

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
		[2]  = "DDRPHY",
		[3]  = "INFRA",
		[4]  = "AUDIO",
		[5]  = "DIS",
		[6]  = "ISP",
		[7]  = "CAM",
		[8]  = "VDE",
		[9]  = "VEN",
		[10] = "VODEC(res)",
		[11] = "MFG",
		[12] = "MFG_CORE0",
		[13] = "MFG_CORE1",
		[14] = "MFG_ASYNC",
		[15] = "MFG_2D",
		[16] = "MP0_CPU0",
		[17] = "MP0_CPU1",
		[18] = "MP0_CPU2",
		[19] = "MP0_CPU3",
		[20] = "MP0_CPU4",
		[21] = "MP0_CPU5",
		[22] = "MP0_CPU6",
		[23] = "MP0_CPU7",
		[24] = "N/A",
		[25] = "MP0",
		[26] = "MCUSYS",
		[27] = "N/A",
		[28] = "VPU_CORE1",
		[29] = "N/A",
		[30] = "N/A",
		[31] = "N/A",
	};

	return pwr_names;
}

u32 get_spm_pwr_status(void)
{
	static void __iomem *scpsys_base, *pwr_sta, *pwr_sta_2nd;

	if (scpsys_base == NULL || pwr_sta == NULL || pwr_sta_2nd == NULL) {
		scpsys_base = ioremap(0x10006000, PAGE_SIZE);
		pwr_sta = scpsys_base + 0x180;
		pwr_sta_2nd = scpsys_base + 0x184;
	}

	return clk_readl(pwr_sta) & clk_readl(pwr_sta_2nd);
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
	};

	size_t i;
	const char *pvdname = pvdck->provider_name;

	if (pvdname == NULL)
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

static struct clkdbg_ops clkdbg_mt6768_ops = {
	.get_all_fmeter_clks = get_all_fmeter_clks,
	.fmeter_freq = fmeter_freq_op,
	.get_all_regnames = get_all_regnames,
	.get_all_clk_names = get_all_clk_names,
	.get_pwr_names = get_pwr_names,
	.setup_provider_clk = setup_provider_clk,
	.get_spm_pwr_status = get_spm_pwr_status,
};

static void __init init_custom_cmds(void)
{
	static const struct cmd_fn cmds[] = {
		{}
	};

	set_custom_cmds(cmds);
}

static int __init clkdbg_mt6768_init(void)
{
	if (of_machine_is_compatible("mediatek,MT6768") == 0)
		return -ENODEV;

	init_regbase();

	init_custom_cmds();
	set_clkdbg_ops(&clkdbg_mt6768_ops);

	register_devapc_vio_callback(&devapc_vio_handle);

#if DUMP_INIT_STATE
	print_regs();
	print_fmeter_all();
#endif /* DUMP_INIT_STATE */

	return 0;
}
device_initcall(clkdbg_mt6768_init);
