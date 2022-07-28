// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>

#include <dt-bindings/power/mt6983-power.h>

#if IS_ENABLED(CONFIG_MTK_DEVAPC)
#include <devapc_public.h>
#endif

#if IS_ENABLED(CONFIG_MTK_DVFSRC_HELPER)
#include <mt-plat/dvfsrc-exp.h>
#endif

#include "clkchk.h"
#include "clkchk-mt6983.h"

#define BUG_ON_CHK_ENABLE		0
#define CHECK_VCORE_FREQ		0

/*
 * clkchk dump_regs
 */

#define REGBASE_V(_phys, _id_name, _pg, _pn) { .phys = _phys,	\
		.name = #_id_name, .pg = _pg, .pn = _pn}

static struct regbase rb[] = {
	[top] = REGBASE_V(0x10000000, top, PD_NULL, CLK_NULL),
	[ifrao] = REGBASE_V(0x10001000, ifrao, PD_NULL, CLK_NULL),
	[apmixed] = REGBASE_V(0x1000C000, apmixed, PD_NULL, CLK_NULL),
	[perao] = REGBASE_V(0x11036000, perao, PD_NULL, CLK_NULL),

	[vde1] = REGBASE_V(0x1600f000, vde1, MT6983_POWER_DOMAIN_VDE0, CLK_NULL),
	[vde2] = REGBASE_V(0x1602f000, vde2, MT6983_POWER_DOMAIN_VDE1, CLK_NULL),

	[ven1] = REGBASE_V(0x17000000, ven1, MT6983_POWER_DOMAIN_VEN0, CLK_NULL),
	[ven2] = REGBASE_V(0x17800000, ven2, MT6983_POWER_DOMAIN_VEN1, CLK_NULL),

	[disp] = REGBASE_V(0x14000000, disp, MT6983_POWER_DOMAIN_DIS0, CLK_NULL),
	[disp1] = REGBASE_V(0x14400000, disp1, MT6983_POWER_DOMAIN_DIS1, CLK_NULL),

	[mdp] = REGBASE_V(0x1f000000, mdp, MT6983_POWER_DOMAIN_MDP0, CLK_NULL),
	[mdp1] = REGBASE_V(0x1f80000, mdp1, MT6983_POWER_DOMAIN_MDP1, CLK_NULL),

	[img] = REGBASE_V(0x15000000, img, MT6983_POWER_DOMAIN_ISP_MAIN, CLK_NULL),
	[dip_top_dip1] = REGBASE_V(0x15110000, dip_top_dip1, MT6983_POWER_DOMAIN_ISP_DIP1,
		CLK_NULL),
	[dip_nr_dip1] = REGBASE_V(0x15130000, dip_nr_dip1, MT6983_POWER_DOMAIN_ISP_DIP1, CLK_NULL),
	[ipe] = REGBASE_V(0x15330000, ipe, MT6983_POWER_DOMAIN_ISP_IPE, CLK_NULL),
	[wpe1_dip1] = REGBASE_V(0x15220000, wpe1_dip1, MT6983_POWER_DOMAIN_ISP_DIP1, CLK_NULL),
	[wpe2_dip1] = REGBASE_V(0x15520000, wpe2_dip1, MT6983_POWER_DOMAIN_ISP_DIP1, CLK_NULL),
	[wpe3_dip1] = REGBASE_V(0x15620000, wpe3_dip1, MT6983_POWER_DOMAIN_ISP_DIP1, CLK_NULL),

	[spm] = REGBASE_V(0x1C001000, spm, PD_NULL, CLK_NULL),
	[vlpcfg] = REGBASE_V(0x1C00C000, vlpcfg, PD_NULL, CLK_NULL),
	[vlp_ck] = REGBASE_V(0x1C013000, vlp_ck, PD_NULL, CLK_NULL),

	[cam_m] = REGBASE_V(0x1a000000, cam_m, MT6983_POWER_DOMAIN_CAM_MAIN, CLK_NULL),
	[cam_ra] = REGBASE_V(0x1a04f000, cam_ra, MT6983_POWER_DOMAIN_CAM_SUBA, CLK_NULL),
	[cam_ya] = REGBASE_V(0x1a06f000, cam_ya, MT6983_POWER_DOMAIN_CAM_SUBA, CLK_NULL),
	[cam_rb] = REGBASE_V(0x1a08f000, cam_rb, MT6983_POWER_DOMAIN_CAM_SUBB, CLK_NULL),
	[cam_yb] = REGBASE_V(0x1a0af000, cam_yb, MT6983_POWER_DOMAIN_CAM_SUBB, CLK_NULL),
	[cam_rc] = REGBASE_V(0x190f3000, cam_rc, MT6983_POWER_DOMAIN_CAM_SUBC, CLK_NULL),
	[cam_yc] = REGBASE_V(0x11282000, cam_yc, MT6983_POWER_DOMAIN_CAM_SUBC, CLK_NULL),
	[cam_mr] = REGBASE_V(0x1a170000, cam_mr, MT6983_POWER_DOMAIN_CAM_MRAW, CLK_NULL),
	[ccu] = REGBASE_V(0x1b200000, ccu_m, MT6983_POWER_DOMAIN_CAM_MAIN, CLK_NULL),

	[afe] = REGBASE_V(0x1e100000, afe, MT6983_POWER_DOMAIN_AUDIO, CLK_NULL),
	[mminfra_config] = REGBASE_V(0x1e800000, mminfra_config, MT6983_POWER_DOMAIN_MM_INFRA,
		CLK_NULL),

	{},
};

#define REGNAME(_base, _ofs, _name)	\
	{ .base = &rb[_base], .ofs = _ofs, .name = #_name }

static struct regname rn[] = {
	/* TOPCKGEN register */
	REGNAME(top, 0x0010, CLK_CFG_0),
	REGNAME(top, 0x0020, CLK_CFG_1),
	REGNAME(top, 0x0030, CLK_CFG_2),
	REGNAME(top, 0x0040, CLK_CFG_3),
	REGNAME(top, 0x0050, CLK_CFG_4),
	REGNAME(top, 0x0060, CLK_CFG_5),
	REGNAME(top, 0x0070, CLK_CFG_6),
	REGNAME(top, 0x0080, CLK_CFG_7),
	REGNAME(top, 0x0090, CLK_CFG_8),
	REGNAME(top, 0x00A0, CLK_CFG_9),
	REGNAME(top, 0x00B0, CLK_CFG_10),
	REGNAME(top, 0x00C0, CLK_CFG_11),
	REGNAME(top, 0x00D0, CLK_CFG_12),
	REGNAME(top, 0x00E0, CLK_CFG_13),
	REGNAME(top, 0x00F0, CLK_CFG_14),
	REGNAME(top, 0x0100, CLK_CFG_15),
	REGNAME(top, 0x0110, CLK_CFG_16),
	REGNAME(top, 0x0120, CLK_CFG_17),
	REGNAME(top, 0x0130, CLK_CFG_18),
	REGNAME(top, 0x0140, CLK_CFG_19),
	REGNAME(top, 0x0150, CLK_CFG_20),
	REGNAME(top, 0x0160, CLK_CFG_21),
	REGNAME(top, 0x0170, CLK_CFG_22),
	REGNAME(top, 0x0320, CLK_AUDDIV_0),
	REGNAME(top, 0x0328, CLK_AUDDIV_2),
	REGNAME(top, 0x0334, CLK_AUDDIV_3),
	REGNAME(top, 0x0338, CLK_AUDDIV_4),
	REGNAME(top, 0x0, CLK_MODE),
	/* INFRACFG_AO register */
	REGNAME(ifrao, 0x6C, HRE_INFRA_BUS_CTRL),
	REGNAME(ifrao, 0x70, INFRA_BUS_DCM_CTRL),
	REGNAME(ifrao, 0x90, MODULE_CG_0),
	REGNAME(ifrao, 0x94, MODULE_CG_1),
	REGNAME(ifrao, 0xAC, MODULE_CG_2),
	REGNAME(ifrao, 0xC8, MODULE_CG_3),
	REGNAME(ifrao, 0xE8, MODULE_CG_4),
	/* INFRACFG_AO_BUS register */
	REGNAME(ifrao, 0x0C90, MCU_CONNSYS_PROTECT_EN_0),
	REGNAME(ifrao, 0x0C9C, MCU_CONNSYS_PROTECT_RDY_STA_0),
	REGNAME(ifrao, 0x0C50, INFRASYS_PROTECT_EN_1),
	REGNAME(ifrao, 0x0C5C, INFRASYS_PROTECT_RDY_STA_1),
	REGNAME(ifrao, 0x0C40, INFRASYS_PROTECT_EN_0),
	REGNAME(ifrao, 0x0C4C, INFRASYS_PROTECT_RDY_STA_0),
	REGNAME(ifrao, 0x0C80, PERISYS_PROTECT_EN_0),
	REGNAME(ifrao, 0x0C8C, PERISYS_PROTECT_RDY_STA_0),
	REGNAME(ifrao, 0x0C60, EMISYS_PROTECT_EN_0),
	REGNAME(ifrao, 0x0C6C, EMISYS_PROTECT_RDY_STA_0),
	REGNAME(ifrao, 0x0C30, MMSYS_PROTECT_EN_2),
	REGNAME(ifrao, 0x0C3C, MMSYS_PROTECT_RDY_STA_2),
	REGNAME(ifrao, 0x0C10, MMSYS_PROTECT_EN_0),
	REGNAME(ifrao, 0x0C1C, MMSYS_PROTECT_RDY_STA_0),
	REGNAME(ifrao, 0x0C20, MMSYS_PROTECT_EN_1),
	REGNAME(ifrao, 0x0C2C, MMSYS_PROTECT_RDY_STA_1),
	REGNAME(ifrao, 0x0CC0, DRAMC_CCUSYS_PROTECT_EN_0),
	REGNAME(ifrao, 0x0CCC, DRAMC_CCUSYS_PROTECT_RDY_STA_0),
	REGNAME(ifrao, 0x0CA0, MD_MFGSYS_PROTECT_EN_0),
	REGNAME(ifrao, 0x0CAC, MD_MFGSYS_PROTECT_RDY_STA_0),
	/* APMIXEDSYS register */
	REGNAME(apmixed, 0x208, ARMPLL_LL_CON0),
	REGNAME(apmixed, 0x20c, ARMPLL_LL_CON1),
	REGNAME(apmixed, 0x210, ARMPLL_LL_CON2),
	REGNAME(apmixed, 0x214, ARMPLL_LL_CON3),
	REGNAME(apmixed, 0x218, ARMPLL_BL_CON0),
	REGNAME(apmixed, 0x21c, ARMPLL_BL_CON1),
	REGNAME(apmixed, 0x220, ARMPLL_BL_CON2),
	REGNAME(apmixed, 0x224, ARMPLL_BL_CON3),
	REGNAME(apmixed, 0x238, CCIPLL_CON0),
	REGNAME(apmixed, 0x23c, CCIPLL_CON1),
	REGNAME(apmixed, 0x240, CCIPLL_CON2),
	REGNAME(apmixed, 0x244, CCIPLL_CON3),
	REGNAME(apmixed, 0x350, MAINPLL_CON0),
	REGNAME(apmixed, 0x354, MAINPLL_CON1),
	REGNAME(apmixed, 0x358, MAINPLL_CON2),
	REGNAME(apmixed, 0x35c, MAINPLL_CON3),
	REGNAME(apmixed, 0x308, UNIVPLL_CON0),
	REGNAME(apmixed, 0x30c, UNIVPLL_CON1),
	REGNAME(apmixed, 0x310, UNIVPLL_CON2),
	REGNAME(apmixed, 0x314, UNIVPLL_CON3),
	REGNAME(apmixed, 0x360, MSDCPLL_CON0),
	REGNAME(apmixed, 0x364, MSDCPLL_CON1),
	REGNAME(apmixed, 0x368, MSDCPLL_CON2),
	REGNAME(apmixed, 0x36c, MSDCPLL_CON3),
	REGNAME(apmixed, 0x3a0, MMPLL_CON0),
	REGNAME(apmixed, 0x3a4, MMPLL_CON1),
	REGNAME(apmixed, 0x3a8, MMPLL_CON2),
	REGNAME(apmixed, 0x3ac, MMPLL_CON3),
	REGNAME(apmixed, 0x380, ADSPPLL_CON0),
	REGNAME(apmixed, 0x384, ADSPPLL_CON1),
	REGNAME(apmixed, 0x388, ADSPPLL_CON2),
	REGNAME(apmixed, 0x38c, ADSPPLL_CON3),
	REGNAME(apmixed, 0x248, TVDPLL_CON0),
	REGNAME(apmixed, 0x24c, TVDPLL_CON1),
	REGNAME(apmixed, 0x250, TVDPLL_CON2),
	REGNAME(apmixed, 0x254, TVDPLL_CON3),
	REGNAME(apmixed, 0x328, APLL1_CON0),
	REGNAME(apmixed, 0x32c, APLL1_CON1),
	REGNAME(apmixed, 0x330, APLL1_CON2),
	REGNAME(apmixed, 0x334, APLL1_CON3),
	REGNAME(apmixed, 0x338, APLL1_CON4),
	REGNAME(apmixed, 0x0040, APLL1_TUNER_CON0),
	REGNAME(apmixed, 0x000C, AP_PLL_CON3),
	REGNAME(apmixed, 0x33c, APLL2_CON0),
	REGNAME(apmixed, 0x340, APLL2_CON1),
	REGNAME(apmixed, 0x344, APLL2_CON2),
	REGNAME(apmixed, 0x348, APLL2_CON3),
	REGNAME(apmixed, 0x34c, APLL2_CON4),
	REGNAME(apmixed, 0x0044, APLL2_TUNER_CON0),
	REGNAME(apmixed, 0x000C, AP_PLL_CON3),
	REGNAME(apmixed, 0x390, MPLL_CON0),
	REGNAME(apmixed, 0x394, MPLL_CON1),
	REGNAME(apmixed, 0x398, MPLL_CON2),
	REGNAME(apmixed, 0x39c, MPLL_CON3),
	REGNAME(apmixed, 0x370, IMGPLL_CON0),
	REGNAME(apmixed, 0x374, IMGPLL_CON1),
	REGNAME(apmixed, 0x378, IMGPLL_CON2),
	REGNAME(apmixed, 0x37c, IMGPLL_CON3),
	/* DISPSYS_CONFIG register */
	REGNAME(disp, 0x100, MMSYS_CG_0),
	REGNAME(disp, 0x110, MMSYS_CG_1),
	REGNAME(disp, 0x1A0, MMSYS_CG_2),
	/* DISPSYS1_CONFIG register */
	REGNAME(disp1, 0x100, MMSYS1_CG_0),
	REGNAME(disp1, 0x110, MMSYS1_CG_1),
	REGNAME(disp1, 0x1A0, MMSYS1_CG_2),
	/* PERICFG_AO register */
	REGNAME(perao, 0x3c, PERI_CG_0),
	REGNAME(perao, 0x40, PERI_CG_1),
	REGNAME(perao, 0x44, PERI_CG_2),
	/* IMGSYS_MAIN register */
	REGNAME(img, 0x0, IMG_MAIN_CG),
	/* DIP_TOP_DIP1 register */
	REGNAME(dip_top_dip1, 0x0, TOP_DIP1_CG),
	/* DIP_NR_DIP1 register */
	REGNAME(dip_nr_dip1, 0x0, NR_DIP1_CG),
	/* WPE1_DIP1 register */
	REGNAME(wpe1_dip1, 0x0, WPE1_DIP1_CG),
	/* IPESYS register */
	REGNAME(ipe, 0x0, IPE_CG),
	/* WPE2_DIP1 register */
	REGNAME(wpe2_dip1, 0x0, WPE2_DIP1_CG),
	/* WPE3_DIP1 register */
	REGNAME(wpe3_dip1, 0x0, WPE3_DIP1_CG),
	/* VDEC_GCON_BASE register */
	REGNAME(vde2, 0x8, LARB_CKEN_CON),
	REGNAME(vde2, 0x0, VDEC_CKEN),
	/* VENC_GCON register */
	REGNAME(ven1, 0x0, VENCSYS_CG),
	/* SPM register */
	REGNAME(spm, 0xE00, MD1_PWR_CON),
	REGNAME(spm, 0xF30, SOC_BUCK_ISO_CON),
	REGNAME(spm, 0xF34, PWR_STATUS),
	REGNAME(spm, 0xF38, PWR_STATUS_2ND),
	REGNAME(spm, 0xF2C, MD_BUCK_ISO_CON),
	REGNAME(spm, 0xE04, CONN_PWR_CON),
	REGNAME(spm, 0xE10, UFS0_PWR_CON),
	REGNAME(spm, 0xE14, AUDIO_PWR_CON),
	REGNAME(spm, 0xE18, ADSP_TOP_PWR_CON),
	REGNAME(spm, 0xE1C, ADSP_INFRA_PWR_CON),
	REGNAME(spm, 0xE20, ADSP_AO_PWR_CON),
	REGNAME(spm, 0xE24, ISP_MAIN_PWR_CON),
	REGNAME(spm, 0xE28, ISP_DIP1_PWR_CON),
	REGNAME(spm, 0xE2C, ISP_IPE_PWR_CON),
	REGNAME(spm, 0xE30, ISP_VCORE_PWR_CON),
	REGNAME(spm, 0xE34, VDE0_PWR_CON),
	REGNAME(spm, 0xE38, VDE1_PWR_CON),
	REGNAME(spm, 0xE3C, VEN0_PWR_CON),
	REGNAME(spm, 0xE40, VEN1_PWR_CON),
	REGNAME(spm, 0xE44, CAM_MAIN_PWR_CON),
	REGNAME(spm, 0xE48, CAM_MRAW_PWR_CON),
	REGNAME(spm, 0xE4C, CAM_SUBA_PWR_CON),
	REGNAME(spm, 0xE50, CAM_SUBB_PWR_CON),
	REGNAME(spm, 0xE54, CAM_SUBC_PWR_CON),
	REGNAME(spm, 0xE58, CAM_VCORE_PWR_CON),
	REGNAME(spm, 0xE5C, MDP0_PWR_CON),
	REGNAME(spm, 0xE60, MDP1_PWR_CON),
	REGNAME(spm, 0xE64, DIS0_PWR_CON),
	REGNAME(spm, 0xE68, DIS1_PWR_CON),
	REGNAME(spm, 0xE6C, MM_INFRA_PWR_CON),
	REGNAME(spm, 0xE70, MM_PROC_PWR_CON),
	REGNAME(spm, 0xE74, DP_TX_PWR_CON),
	REGNAME(spm, 0xE78, SCP_PWR_CON),
	REGNAME(spm, 0xEB8, MFG0_PWR_CON),
	REGNAME(spm, 0xF3C, XPU_PWR_STATUS),
	REGNAME(spm, 0xF40, XPU_PWR_STATUS_2ND),
	REGNAME(spm, 0xEBC, MFG1_PWR_CON),
	REGNAME(spm, 0xEC0, MFG2_PWR_CON),
	REGNAME(spm, 0xEC4, MFG3_PWR_CON),
	REGNAME(spm, 0xEC8, MFG4_PWR_CON),
	REGNAME(spm, 0xECC, MFG5_PWR_CON),
	REGNAME(spm, 0xED0, MFG6_PWR_CON),
	REGNAME(spm, 0xED4, MFG7_PWR_CON),
	REGNAME(spm, 0xED8, MFG8_PWR_CON),
	REGNAME(spm, 0xEDC, MFG9_PWR_CON),
	REGNAME(spm, 0xEE0, MFG10_PWR_CON),
	REGNAME(spm, 0xEE4, MFG11_PWR_CON),
	REGNAME(spm, 0xEE8, MFG12_PWR_CON),
	REGNAME(spm, 0xEEC, MFG13_PWR_CON),
	REGNAME(spm, 0xEF0, MFG14_PWR_CON),
	REGNAME(spm, 0xEF4, MFG15_PWR_CON),
	REGNAME(spm, 0xEF8, MFG16_PWR_CON),
	REGNAME(spm, 0xEFC, MFG17_PWR_CON),
	REGNAME(spm, 0xF00, MFG18_PWR_CON),
	REGNAME(spm, 0xF08, EMI_HRE_SRAM_CON),
	/* VLPCFG_BUS register */
	REGNAME(vlpcfg, 0x0210, VLP_TOPAXI_PROTECTEN),
	REGNAME(vlpcfg, 0x0220, VLP_TOPAXI_PROTECTEN_STA1),
	REGNAME(vlpcfg, 0x0230, VLP_TOPAXI_PROTECTEN1),
	REGNAME(vlpcfg, 0x0240, VLP_TOPAXI_PROTECTEN1_STA1),
	/* VLP_CKSYS register */
	REGNAME(vlp_ck, 0x0008, VLP_CLK_CFG_0),
	REGNAME(vlp_ck, 0x0014, VLP_CLK_CFG_1),
	REGNAME(vlp_ck, 0x0020, VLP_CLK_CFG_2),
	REGNAME(vlp_ck, 0x002C, VLP_CLK_CFG_3),
	/* CAM_MAIN_R1A register */
	REGNAME(cam_m, 0x0, CAM_MAIN_CG),
	REGNAME(cam_m, 0xA0, CAM_MAIN_SW_RST),
	REGNAME(cam_m, 0xA4, CAM_MAIN_SW_RST1),
	/* CAMSYS_RAWA register */
	REGNAME(cam_ra, 0x0, CAMSYS_RA_CG),
	/* CAMSYS_YUVA register */
	REGNAME(cam_ya, 0x0, CAMSYS_YA_CG),
	/* CAMSYS_RAWB register */
	REGNAME(cam_rb, 0x0, CAMSYS_RB_CG),
	/* CAMSYS_YUVB register */
	REGNAME(cam_yb, 0x0, CAMSYS_YB_CG),
	/* CAMSYS_RAWC register */
	REGNAME(cam_rc, 0x0, CAMSYS_RC_CG),
	/* CAMSYS_YUVC register */
	REGNAME(cam_yc, 0x0, CAMSYS_YC_CG),
	/* CAMSYS_MRAW register */
	REGNAME(cam_mr, 0x0, CAMSYS_MR_CG),
	/* CCU_MAIN register */
	REGNAME(ccu, 0x0, CCUSYS_CG),
	/* AFE register */
	REGNAME(afe, 0x0, AUDIO_TOP_0),
	REGNAME(afe, 0x4, AUDIO_TOP_1),
	REGNAME(afe, 0x8, AUDIO_TOP_2),
	/* MMINFRA_CONFIG register */
	REGNAME(mminfra_config, 0x100, MMINFRA_CG_0),
	REGNAME(mminfra_config, 0x110, MMINFRA_CG_1),
	/* MDPSYS_CONFIG register */
	REGNAME(mdp, 0x100, MDPSYS_CG_0),
	/* MDPSYS1_CONFIG register */
	REGNAME(mdp1, 0x100, MDPSYS1_CG_0),
	{},
};

static const struct regname *get_all_mt6983_regnames(void)
{
	return rn;
}

static void init_regbase(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(rb) - 1; i++) {
		if (!rb[i].phys)
			continue;

		rb[i].virt = ioremap(rb[i].phys, 0x1000);
	}
}

/*
 * clkchk pwr_status
 */
static u32 pwr_ofs[STA_NUM] = {
	[PWR_STA] = 0xF34,
	[PWR_STA2] = 0xF38,
	[XPU_PWR_STA] = 0xF3C,
	[XPU_PWR_STA2] = 0xF40,
	[OTHER_STA] = 0x414,
};

static u32 pwr_sta[STA_NUM];

u32 *get_spm_pwr_status_array(void)
{
	static void __iomem *pwr_addr[STA_NUM];
	int i;

	for (i = 0; i < STA_NUM; i++) {
		if (pwr_ofs[i]) {
			pwr_addr[i] = rb[spm].virt + pwr_ofs[i];
			pwr_sta[i] = clk_readl(pwr_addr[i]);
		}
	}

	return pwr_sta;
}

/*
 * clkchk pwr_msk
 */
static struct pvd_msk pvd_pwr_mask[] = {
	{"topckgen", PWR_STA, 0x00000000},
	{"apmixedsys", PWR_STA, 0x00000000},
	{"pericfg_ao", PWR_STA, 0x00000000},
	{"vlp_cksys", PWR_STA, 0x00000000},
	{"mfg_pll_ctrl", PWR_STA, 0x00000000},
	{"apu_pll_ctrl", PWR_STA, 0x00000000},
	{"afe", PWR_STA, 0x00000020},
	{"adsp", PWR_STA, 0x00000040},
	{"camsys_mraw", PWR_STA, 0x00040000},
	{"camsys_rawa", PWR_STA, 0x00080000},
	{"camsys_rawb", PWR_STA, 0x00100000},
	{"camsys_rawc", PWR_STA, 0x00200000},
	{"camsys_yuva", PWR_STA, 0x00080000},
	{"camsys_yuvb", PWR_STA, 0x00100000},
	{"camsys_yuvc", PWR_STA, 0x00200000},
	{"cam_main_r1a", PWR_STA, 0x00020000},
	{"ccu_main", PWR_STA, 0x00020000},
	{"dip_nr_dip1", PWR_STA, 0x00000400},
	{"dip_top_dip1", PWR_STA, 0x00000400},
	{"dispsys_config", PWR_STA, 0x02000000},
	{"dispsys1_config", PWR_STA, 0x04000000},
	{"gce_d", PWR_STA, 0x08000000},
	{"gce_m", PWR_STA, 0x08000000},
	{"mfgrpc", XPU_PWR_STA, 0x00000004},
	{"imgsys_main", PWR_STA, 0x00000200},
	{"imp_iic_wrap0", PWR_STA, 0x00000000},
	{"imp_iic_wrap1", PWR_STA, 0x00000000},
	{"imp_iic_wrap2", PWR_STA, 0x00000000},
	{"infracfg", PWR_STA, 0x00000000},
	{"infracfg_ao", PWR_STA, 0x00000000},
	{"ipesys", PWR_STA, 0x00000800},
	{"mdpsys1", PWR_STA, 0x01000000},
	{"mdpsys", PWR_STA, 0x00800000},
	{"mfg_top_config", XPU_PWR_STA, 0x00000004},
	{"mminfra_config", PWR_STA, 0x08000000},
	{"vdec_soc_gcon_base", PWR_STA, 0x00002000},
	{"vdec_gcon_base", PWR_STA, 0x00004000},
	{"venc_gcon", PWR_STA, 0x00008000},
	{"venc_gcon_core1", PWR_STA, 0x00010000},
	{"wpe1_dip1", PWR_STA, 0x00000400},
	{"wpe2_dip1", PWR_STA, 0x00000400},
	{"wpe3_dip1", PWR_STA, 0x00000400},
	{},
};

static struct pvd_msk *get_pvd_pwr_mask(void)
{
	return pvd_pwr_mask;
}

/*
 * clkchk vf table
 */

/*
 * Opp0 : 0p75v
 * Opp1 : 0p725v
 * Opp2 : 0p65v
 * Opp3 : 0p60v
 * Opp4 : 0p55v
 */
#if CHECK_VCORE_FREQ
static struct mtk_vf vf_table[] = {
	/* Opp0, Opp1, Opp2, Opp3, Opp4 */
	MTK_VF_TABLE("axi_sel", 156000, 156000, 156000, 156000, 156000),
	MTK_VF_TABLE("axip_sel", 156000, 156000, 156000, 156000, 156000),
	MTK_VF_TABLE("axi_u_sel", 78000, 78000, 78000, 78000, 78000),
	MTK_VF_TABLE("bus_aximem_sel", 364000, 364000, 273000, 273000, 218400),
	MTK_VF_TABLE("disp0_sel", 624000, 594000, 436800, 312000, 208000),
	MTK_VF_TABLE("mdp0_sel", 624000, 594000, 436800, 343750, 229000),
	MTK_VF_TABLE("mminfra_sel", 624000, 624000, 436800, 273000, 229167),
	MTK_VF_TABLE("mmup_sel", 546000, 546000, 546000, 546000, 546000),
	MTK_VF_TABLE("dsp_sel", 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("dsp1_sel", 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("dsp2_sel", 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("dsp3_sel", 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("dsp4_sel", 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("dsp5_sel", 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("dsp6_sel", 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("dsp7_sel", 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("ipu_if_sel", 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("camtg_sel", 52000, 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("camtg2_sel", 52000, 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("camtg3_sel", 52000, 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("camtg4_sel", 52000, 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("camtg5_sel", 52000, 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("camtg6_sel", 52000, 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("spi_sel", 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("msdc_macro_sel", 384000, 384000, 384000, 384000, 384000),
	MTK_VF_TABLE("msdc30_1_sel", 192000, 192000, 192000, 192000, 192000),
	MTK_VF_TABLE("audio_sel", 54600, 54600, 54600, 54600, 54600),
	MTK_VF_TABLE("aud_intbus_sel", 136500, 136500, 136500, 136500, 136500),
	MTK_VF_TABLE("atb_sel", 273000, 273000, 273000, 273000, 273000),
	MTK_VF_TABLE("disp_pwm_sel", 130000, 130000, 130000, 130000, 130000),
	MTK_VF_TABLE("usb_sel", 124800, 124800, 124800, 124800, 124800),
	MTK_VF_TABLE("ssusb_xhci_sel", 124800, 124800, 124800, 124800, 124800),
	MTK_VF_TABLE("i2c_sel", 124800, 124800, 124800, 124800, 124800),
	MTK_VF_TABLE("seninf_sel", 499200, 499200, 499200, 392857, 297000),
	MTK_VF_TABLE("seninf1_sel", 499200, 499200, 499200, 392857, 297000),
	MTK_VF_TABLE("seninf2_sel", 499200, 499200, 499200, 392857, 297000),
	MTK_VF_TABLE("seninf3_sel", 499200, 499200, 499200, 392857, 297000),
	MTK_VF_TABLE("dxcc_sel", 273000, 273000, 273000, 273000, 273000),
	MTK_VF_TABLE("aud_engen1_sel", 45158, 45158, 45158, 45158, 45158),
	MTK_VF_TABLE("aud_engen2_sel", 49152, 49152, 49152, 49152, 49152),
	MTK_VF_TABLE("aes_ufsfde_sel", 416000, 416000, 416000, 416000, 416000),
	MTK_VF_TABLE("ufs_sel", 192000, 192000, 192000, 192000, 192000),
	MTK_VF_TABLE("ufs_mbist_sel", 297000, 297000, 297000, 297000, 297000),
	MTK_VF_TABLE("aud_1_sel", 180634, 180634, 180634, 180634, 180634),
	MTK_VF_TABLE("aud_2_sel", 196608, 196608, 196608, 196608, 196608),
	MTK_VF_TABLE("adsp_sel", 750000, 750000, 750000, 750000, 750000),
	MTK_VF_TABLE("dpmaif_main_sel", 436800, 436800, 364000, 273000, 242667),
	MTK_VF_TABLE("venc_sel", 624000, 624000, 458333, 343750, 249600),
	MTK_VF_TABLE("vdec_sel", 546000, 546000, 416000, 312000, 218400),
	MTK_VF_TABLE("pwm_sel", 78000, 78000, 78000, 78000, 78000),
	MTK_VF_TABLE("audio_h_sel", 196608, 196608, 196608, 196608, 196608),
	MTK_VF_TABLE("mcupm_sel", 182000, 182000, 182000, 182000, 182000),
	MTK_VF_TABLE("mem_sub_sel", 546000, 546000, 436800, 273000, 218400),
	MTK_VF_TABLE("mem_subp_sel", 546000, 499200, 416000, 273000, 218400),
	MTK_VF_TABLE("mem_sub_u_sel", 546000, 499200, 416000, 273000, 218400),
	MTK_VF_TABLE("emi_n_sel", 182000, 182000, 182000, 182000, 182000),
	MTK_VF_TABLE("dsi_occ_sel", 312000, 312000, 312000, 249600, 182000),
	MTK_VF_TABLE("ccu_ahb_sel", 273000, 273000, 273000, 273000, 273000),
	MTK_VF_TABLE("ap2conn_host_sel", 78000, 78000, 78000, 78000, 78000),
	MTK_VF_TABLE("mcu_acp_sel", 499200, 499200, 499200, 392857, 249600),
	MTK_VF_TABLE("dpi_sel", 297000, 297000, 297000, 297000, 297000),
	{},
};
#endif

static struct mtk_vf *get_vf_table(void)
{
#if CHECK_VCORE_FREQ
	return vf_table;
#else
	return NULL;
#endif
}

static int get_vcore_opp(void)
{
#if IS_ENABLED(CONFIG_MTK_DVFSRC_HELPER) && CHECK_VCORE_FREQ
	return mtk_dvfsrc_query_opp_info(MTK_DVFSRC_SW_REQ_VCORE_OPP);
#else
	return VCORE_NULL;
#endif
}

void print_subsys_reg_mt6983(enum chk_sys_id id)
{
	struct regbase *rb_dump;
	const struct regname *rns = &rn[0];
	int i;

	if (rns == NULL)
		return;

	if (id >= chk_sys_num || id < 0) {
		pr_info("wrong id:%d\n", id);
		return;
	}

	rb_dump = &rb[id];

	for (i = 0; i < ARRAY_SIZE(rn) - 1; i++, rns++) {
		if (!is_valid_reg(ADDR(rns)))
			return;

		/* filter out the subsys that we don't want */
		if (rns->base != rb_dump)
			continue;

		pr_info("%-18s: [0x%08x] = 0x%08x\n",
			rns->name, PHYSADDR(rns), clk_readl(ADDR(rns)));
	}
}
EXPORT_SYMBOL(print_subsys_reg_mt6983);

#if IS_ENABLED(CONFIG_MTK_DEVAPC)
static void devapc_dump(void)
{
	print_subsys_reg_mt6983(spm);
	print_subsys_reg_mt6983(top);
	print_subsys_reg_mt6983(ifrao);
	print_subsys_reg_mt6983(apmixed);
	print_subsys_reg_mt6983(vlpcfg);
	print_subsys_reg_mt6983(vlp_ck);
}

static struct devapc_vio_callbacks devapc_vio_handle = {
	.id = DEVAPC_SUBSYS_CLKMGR,
	.debug_dump = devapc_dump,
};

#endif

static const char * const off_pll_names[] = {
	"msdcpll",
	"univpll",
	"mmpll",
	"tvdpll",
	"imgpll",
	"mfgpll",
	"mfgscpll",
	NULL
};

static const char * const notice_pll_names[] = {
	"adsppll",
	"apll1",
	"apll2",
	NULL
};

static const char * const *get_off_pll_names(void)
{
	return off_pll_names;
}

static const char * const *get_notice_pll_names(void)
{
	return notice_pll_names;
}

static bool is_pll_chk_bug_on(void)
{
#if BUG_ON_CHK_ENABLE
	return true;
#endif
	return false;
}

/*
 * init functions
 */

static struct clkchk_ops clkchk_mt6983_ops = {
	.get_all_regnames = get_all_mt6983_regnames,
	.get_spm_pwr_status_array = get_spm_pwr_status_array,
	.get_pvd_pwr_mask = get_pvd_pwr_mask,
	.get_off_pll_names = get_off_pll_names,
	.get_notice_pll_names = get_notice_pll_names,
	.is_pll_chk_bug_on = is_pll_chk_bug_on,
	.get_vf_table = get_vf_table,
	.get_vcore_opp = get_vcore_opp,
	.devapc_dump = devapc_dump,
};

static int clk_chk_mt6983_probe(struct platform_device *pdev)
{
	init_regbase();

	set_clkchk_ops(&clkchk_mt6983_ops);

#if IS_ENABLED(CONFIG_MTK_DEVAPC)
	register_devapc_vio_callback(&devapc_vio_handle);
#endif

	return 0;
}

static const struct of_device_id of_match_clkchk_mt6983[] = {
	{
		.compatible = "mediatek,mt6983-clkchk",
	}, {
		/* sentinel */
	}
};

static struct platform_driver clk_chk_mt6983_drv = {
	.probe = clk_chk_mt6983_probe,
	.driver = {
		.name = "clk-chk-mt6983",
		.owner = THIS_MODULE,
		.pm = &clk_chk_dev_pm_ops,
		.of_match_table = of_match_clkchk_mt6983,
	},
};

/*
 * init functions
 */

static int __init clkchk_mt6983_init(void)
{
	return platform_driver_register(&clk_chk_mt6983_drv);
}

static void __exit clkchk_mt6983_exit(void)
{
	platform_driver_unregister(&clk_chk_mt6983_drv);
}

subsys_initcall(clkchk_mt6983_init);
module_exit(clkchk_mt6983_exit);
MODULE_LICENSE("GPL");
