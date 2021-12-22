// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2021 MediaTek Inc.
// Author: Owen Chen <owen.chen@mediatek.com>

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>

#include <dt-bindings/power/mt6879-power.h>

#if IS_ENABLED(CONFIG_MTK_DEVAPC)
#include <devapc_public.h>
#endif

#if IS_ENABLED(CONFIG_MTK_DVFSRC_HELPER)
#include <mt-plat/dvfsrc-exp.h>
#endif

#include "clkchk.h"
#include "clkchk-mt6879.h"

#define BUG_ON_CHK_ENABLE		0
#define CHECK_VCORE_FREQ		0
#define CG_CHK_PWRON_ENABLE		1

#define HWV_ADDR_HISTORY_0		0x1F04
#define HWV_DATA_HISTORY_0		0x1F44
#define HWV_IDX_POINTER			0x1F84
#define HWV_DOMAIN_KEY			0x155C
#define HWV_SECURE_KEY			0x10907
#define HWV_CG_SET(id)			(0x0 + (id * 0x8))
#define HWV_CG_STA(id)			(0x1800 + (id * 0x4))
#define HWV_CG_EN(id)			(0x1900 + (id * 0x4))
#define HWV_CG_SET_STA(id)		(0x1A00 + (id * 0x4))
#define HWV_CG_CLR_STA(id)		(0x1B00 + (id * 0x4))
#define HWV_CG_DONE(id)			(0x1C00 + (id * 0x4))
#define HWV_PLL_SET			(0x190)
#define HWV_PLL_CLR			(0x194)
#define HWV_PLL_EN			(0x1400)
#define HWV_PLL_STA			(0x1404)
#define HWV_PLL_DONE			(0x140C)
#define HWV_PLL_SET_STA			(0x1464)
#define HWV_PLL_CLR_STA			(0x1468)

/*
 * clkchk dump_regs
 */

#define REGBASE_V(_phys, _id_name, _pg, _pn) { .phys = _phys, .id = _id_name,	\
		.name = #_id_name, .pg = _pg, .pn = _pn}

static struct regbase rb[] = {
	[top] = REGBASE_V(0x10000000, top, PD_NULL, CLK_NULL),
	[ifrao] = REGBASE_V(0x10001000, ifrao, PD_NULL, CLK_NULL),
	[infracfg] = REGBASE_V(0x10001000, infracfg, PD_NULL, CLK_NULL),
	[apmixed] = REGBASE_V(0x1000C000, apmixed, PD_NULL, CLK_NULL),
	[ifr] = REGBASE_V(0x1020e000, ifr, PD_NULL, CLK_NULL),
	[nemi_reg] = REGBASE_V(0x10219000, nemi_reg, PD_NULL, CLK_NULL),
	[perao] = REGBASE_V(0x11036000, perao, PD_NULL, CLK_NULL),
	[impc] = REGBASE_V(0x11282000, impc, PD_NULL, "i2c_ck"),
	[ufsao] = REGBASE_V(0x112b8000, ufsao, PD_NULL, CLK_NULL),
	[ufspdn] = REGBASE_V(0x112bc000, ufspdn, PD_NULL, CLK_NULL),
	[impe] = REGBASE_V(0x11CB3000, impe, PD_NULL, "i2c_ck"),
	[impw] = REGBASE_V(0x11E03000, impw, PD_NULL, "i2c_ck"),
	[impen] = REGBASE_V(0x11ED4000, impen, PD_NULL, "i2c_ck"),
	[mfg_ao] = REGBASE_V(0x13fa0000, mfg_ao, PD_NULL, CLK_NULL),
	[mfgcfg] = REGBASE_V(0x13fbf000, mfgcfg, MT6879_POWER_DOMAIN_MFG1, CLK_NULL),
	[mm] = REGBASE_V(0x14000000, mm, MT6879_POWER_DOMAIN_DISP, CLK_NULL),
	[img] = REGBASE_V(0x15000000, img, MT6879_POWER_DOMAIN_ISP_MAIN, CLK_NULL),
	[dip_top_dip1] = REGBASE_V(0x15110000, dip_top_dip1, MT6879_POWER_DOMAIN_ISP_DIP1,
		CLK_NULL),
	[dip_nr_dip1] = REGBASE_V(0x15130000, dip_nr_dip1, MT6879_POWER_DOMAIN_ISP_DIP1, CLK_NULL),
	[wpe1_dip1] = REGBASE_V(0x15220000, wpe1_dip1, MT6879_POWER_DOMAIN_ISP_DIP1, CLK_NULL),
	[ipe] = REGBASE_V(0x15330000, ipe, MT6879_POWER_DOMAIN_ISP_IPE, CLK_NULL),
	[wpe2_dip1] = REGBASE_V(0x15520000, wpe2_dip1, MT6879_POWER_DOMAIN_ISP_DIP1, CLK_NULL),
	[wpe3_dip1] = REGBASE_V(0x15620000, wpe3_dip1, MT6879_POWER_DOMAIN_ISP_DIP1, CLK_NULL),
	[vde2] = REGBASE_V(0x1602f000, vde2, MT6879_POWER_DOMAIN_VDE0, CLK_NULL),
	[ven1] = REGBASE_V(0x17000000, ven1, MT6879_POWER_DOMAIN_VEN0, CLK_NULL),
	[apu_ao] = REGBASE_V(0x190f3000, apu_ao, PD_NULL, CLK_NULL),
	[spm] = REGBASE_V(0x1C001000, spm, PD_NULL, CLK_NULL),
	[vlpcfg] = REGBASE_V(0x1C00C000, vlpcfg, PD_NULL, CLK_NULL),
	[vlp_ck] = REGBASE_V(0x1C013000, vlp_ck, PD_NULL, CLK_NULL),
	[cam_m] = REGBASE_V(0x1a000000, cam_m, MT6879_POWER_DOMAIN_CAM_MAIN, CLK_NULL),
	[cam_ra] = REGBASE_V(0x1a04f000, cam_ra, MT6879_POWER_DOMAIN_CAM_SUBA, CLK_NULL),
	[cam_ya] = REGBASE_V(0x1a06f000, cam_ya, MT6879_POWER_DOMAIN_CAM_SUBA, CLK_NULL),
	[cam_rb] = REGBASE_V(0x1a08f000, cam_rb, MT6879_POWER_DOMAIN_CAM_SUBB, CLK_NULL),
	[cam_yb] = REGBASE_V(0x1a0af000, cam_yb, MT6879_POWER_DOMAIN_CAM_SUBB, CLK_NULL),
	[cam_mr] = REGBASE_V(0x1a170000, cam_mr, MT6879_POWER_DOMAIN_CAM_MRAW, CLK_NULL),
	[ccu] = REGBASE_V(0x1b200000, ccu, MT6879_POWER_DOMAIN_CAM_MAIN, CLK_NULL),
	[afe] = REGBASE_V(0x1e100000, afe, MT6879_POWER_DOMAIN_AUDIO, CLK_NULL),
	[mminfra_config] = REGBASE_V(0x1e800000, mminfra_config, MT6879_POWER_DOMAIN_MM_INFRA,
		CLK_NULL),
	[mdp] = REGBASE_V(0x1f000000, mdp, MT6879_POWER_DOMAIN_DISP, CLK_NULL),
	[bcrm_ifr_ao] = REGBASE_V(0x10022000, bcrm_ifr_ao, PD_NULL, CLK_NULL),
	[bcrm_ifr_ao1] = REGBASE_V(0x1002A000, bcrm_ifr_ao1, PD_NULL, CLK_NULL),
	[bcrm_ifr_pdn] = REGBASE_V(0x10215000, bcrm_ifr_pdn, PD_NULL, CLK_NULL),
	[bcrm_ifr_pdn1] = REGBASE_V(0x1027200, bcrm_ifr_pdn1, PD_NULL, CLK_NULL),
	[hfrp] = REGBASE_V(0x1EC24000, hfrp, MT6879_POWER_DOMAIN_MM_PROC_DORMANT, CLK_NULL),
	[mminfra_smi] = REGBASE_V(0x1E801000, mminfra_smi, MT6879_POWER_DOMAIN_MM_INFRA, CLK_NULL),
	[sspm] = REGBASE_V(0x1C342000, sspm, PD_NULL, CLK_NULL),
	[sspm_cfg] = REGBASE_V(0x1C343000, sspm, PD_NULL, CLK_NULL),
	[infracfg1] = REGBASE_V(0x10001000, infracfg1, PD_NULL, CLK_NULL),
	{},
};

#define REGNAME(_base, _ofs, _name)	\
	{ .base = &rb[_base], .id = _base, .ofs = _ofs, .name = #_name }

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
	REGNAME(top, 0x0180, CLK_CFG_17),
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
	REGNAME(infracfg, 0x0C90, MCU_CONNSYS_PROTECT_EN_0),
	REGNAME(infracfg, 0x0C9C, MCU_CONNSYS_PROTECT_RDY_STA_0),
	REGNAME(infracfg, 0x0C50, INFRASYS_PROTECT_EN_1),
	REGNAME(infracfg, 0x0C5C, INFRASYS_PROTECT_RDY_STA_1),
	REGNAME(infracfg, 0x0C40, INFRASYS_PROTECT_EN_0),
	REGNAME(infracfg, 0x0C4C, INFRASYS_PROTECT_RDY_STA_0),
	REGNAME(infracfg, 0x0C80, PERISYS_PROTECT_EN_0),
	REGNAME(infracfg, 0x0C8C, PERISYS_PROTECT_RDY_STA_0),
	REGNAME(infracfg, 0x0C60, EMISYS_PROTECT_EN_0),
	REGNAME(infracfg, 0x0C6C, EMISYS_PROTECT_RDY_STA_0),
	REGNAME(infracfg, 0x0C30, MMSYS_PROTECT_EN_2),
	REGNAME(infracfg, 0x0C3C, MMSYS_PROTECT_RDY_STA_2),
	REGNAME(infracfg, 0x0C10, MMSYS_PROTECT_EN_0),
	REGNAME(infracfg, 0x0C1C, MMSYS_PROTECT_RDY_STA_0),
	REGNAME(infracfg, 0x0C20, MMSYS_PROTECT_EN_1),
	REGNAME(infracfg, 0x0C2C, MMSYS_PROTECT_RDY_STA_1),
	REGNAME(infracfg, 0x0CC0, DRAMC_CCUSYS_PROTECT_EN_0),
	REGNAME(infracfg, 0x0CCC, DRAMC_CCUSYS_PROTECT_RDY_STA_0),
	REGNAME(infracfg, 0x0CA0, MD_MFGSYS_PROTECT_EN_0),
	REGNAME(infracfg, 0x0CAC, MD_MFGSYS_PROTECT_RDY_STA_0),
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
	/* INFRACFG register */
	REGNAME(ifr, 0xB00, BUS_MON_CKEN),
	/* NEMI_REG register */
	REGNAME(nemi_reg, 0x858, EMI_THRO_CTRL1),
	/* PERICFG_AO register */
	REGNAME(perao, 0x10, PERI_CG_0),
	REGNAME(perao, 0x14, PERI_CG_1),
	/* IMP_IIC_WRAP_C register */
	REGNAME(impc, 0xE00, AP_CLOCK_CG_CEN),
	/* UFS_AO_CONFIG register */
	REGNAME(ufsao, 0xC, UFS_AO_CG_0),
	/* UFS_PDN_CFG register */
	REGNAME(ufspdn, 0xC, UFS_PDN_CG_0),
	/* IMP_IIC_WRAP_E register */
	REGNAME(impe, 0xE00, AP_CLOCK_CG_EST),
	/* IMP_IIC_WRAP_W register */
	REGNAME(impw, 0xE00, AP_CLOCK_CG_WST),
	/* IMP_IIC_WRAP_EN register */
	REGNAME(impen, 0xE00, AP_CLOCK_CG_EST_NOR),
	/* MFG_PLL_CTRL register */
	REGNAME(mfg_ao, 0x8, MFGPLL_CON0),
	REGNAME(mfg_ao, 0xc, MFGPLL_CON1),
	REGNAME(mfg_ao, 0x10, MFGPLL_CON2),
	REGNAME(mfg_ao, 0x14, MFGPLL_CON3),
	REGNAME(mfg_ao, 0x38, MFGSCPLL_CON0),
	REGNAME(mfg_ao, 0x3c, MFGSCPLL_CON1),
	REGNAME(mfg_ao, 0x40, MFGSCPLL_CON2),
	REGNAME(mfg_ao, 0x44, MFGSCPLL_CON3),
	/* MFG_TOP_CONFIG register */
	REGNAME(mfgcfg, 0x0, MFG_CG),
	/* DISPSYS_CONFIG register */
	REGNAME(mm, 0x100, MMSYS_CG_0),
	REGNAME(mm, 0x110, MMSYS_CG_1),
	REGNAME(mm, 0x1A0, MMSYS_CG_2),
	/* IMGSYS_MAIN register */
	REGNAME(img, 0x0, IMG_MAIN_CG),
	/* DIP_TOP_DIP1 register */
	REGNAME(dip_top_dip1, 0x0, MACRO_CG),
	/* DIP_NR_DIP1 register */
	REGNAME(dip_nr_dip1, 0x0, MACRO_CG),
	/* WPE1_DIP1 register */
	REGNAME(wpe1_dip1, 0x0, MACRO_CG),
	/* IPESYS register */
	REGNAME(ipe, 0x0, MACRO_CG),
	/* WPE2_DIP1 register */
	REGNAME(wpe2_dip1, 0x0, MACRO_CG),
	/* WPE3_DIP1 register */
	REGNAME(wpe3_dip1, 0x0, MACRO_CG),
	/* VDEC_GCON_BASE register */
	REGNAME(vde2, 0x8, LARB_CKEN_CON),
	REGNAME(vde2, 0x190, MINI_MDP_CFG_0),
	REGNAME(vde2, 0x0, VDEC_CKEN),
	/* VENC_GCON register */
	REGNAME(ven1, 0x0, VENCSYS_CG),
	/* APU_PLL_CTRL register */
	REGNAME(apu_ao, 0x8, APUPLL_CON0),
	REGNAME(apu_ao, 0xc, APUPLL_CON1),
	REGNAME(apu_ao, 0x10, APUPLL_CON2),
	REGNAME(apu_ao, 0x14, APUPLL_CON3),
	REGNAME(apu_ao, 0x18, NPUPLL_CON0),
	REGNAME(apu_ao, 0x1c, NPUPLL_CON1),
	REGNAME(apu_ao, 0x20, NPUPLL_CON2),
	REGNAME(apu_ao, 0x24, NPUPLL_CON3),
	REGNAME(apu_ao, 0x28, APUPLL1_CON0),
	REGNAME(apu_ao, 0x2c, APUPLL1_CON1),
	REGNAME(apu_ao, 0x30, APUPLL1_CON2),
	REGNAME(apu_ao, 0x34, APUPLL1_CON3),
	REGNAME(apu_ao, 0x38, APUPLL2_CON0),
	REGNAME(apu_ao, 0x3c, APUPLL2_CON1),
	REGNAME(apu_ao, 0x40, APUPLL2_CON2),
	REGNAME(apu_ao, 0x44, APUPLL2_CON3),
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
	REGNAME(spm, 0xE3C, VEN0_PWR_CON),
	REGNAME(spm, 0xE44, CAM_MAIN_PWR_CON),
	REGNAME(spm, 0xE48, CAM_MRAW_PWR_CON),
	REGNAME(spm, 0xE4C, CAM_SUBA_PWR_CON),
	REGNAME(spm, 0xE50, CAM_SUBB_PWR_CON),
	REGNAME(spm, 0xE58, CAM_VCORE_PWR_CON),
	REGNAME(spm, 0xE64, DIS0_PWR_CON),
	REGNAME(spm, 0xE6C, MM_INFRA_PWR_CON),
	REGNAME(spm, 0xE70, MM_PROC_PWR_CON),
	REGNAME(spm, 0xF48, CSI_RX_PWR_CON),
	REGNAME(spm, 0xEB8, MFG0_PWR_CON),
	REGNAME(spm, 0xF3C, XPU_PWR_STATUS),
	REGNAME(spm, 0xF40, XPU_PWR_STATUS_2ND),
	REGNAME(spm, 0xEBC, MFG1_PWR_CON),
	REGNAME(spm, 0xEC0, MFG2_PWR_CON),
	REGNAME(spm, 0xEC4, MFG3_PWR_CON),
	REGNAME(spm, 0xEC8, MFG4_PWR_CON),
	REGNAME(spm, 0xECC, MFG5_PWR_CON),
	REGNAME(spm, 0x670, SPM_CROSS_WAKE_M01_REQ),
	REGNAME(spm, 0x414, SPM2APU_CON),
	REGNAME(spm, 0xEA0, DPM1_PWR_CON),
	REGNAME(spm, 0xEA4, DPM2_PWR_CON),
	REGNAME(spm, 0xEA8, DPM3_PWR_CON),
	REGNAME(spm, 0xEB0, EMI1_PWR_CON),
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
	/* CAMSYS_RAWA register */
	REGNAME(cam_ra, 0x0, CAMSYS_CG),
	/* CAMSYS_YUVA register */
	REGNAME(cam_ya, 0x0, CAMSYS_CG),
	/* CAMSYS_RAWB register */
	REGNAME(cam_rb, 0x0, CAMSYS_CG),
	/* CAMSYS_YUVB register */
	REGNAME(cam_yb, 0x0, CAMSYS_CG),
	/* CAMSYS_MRAW register */
	REGNAME(cam_mr, 0x0, CAMSYS_CG),
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
	/* bus latency debug */
	REGNAME(infracfg1, 0x0230, INFRA_APB_ASYNC_STA),
	REGNAME(bcrm_ifr_ao, 0x0024, SI18_CTRL_0),
	REGNAME(bcrm_ifr_ao, 0x0028, SI18_CTRL_1),
	REGNAME(bcrm_ifr_ao, 0x002C, SI18A_CTRL_0),
	REGNAME(bcrm_ifr_ao1, 0x0000, SI18C_CTRL_0),
	REGNAME(bcrm_ifr_pdn, 0x0054, SIP9_CTRL_0),
	REGNAME(bcrm_ifr_pdn, 0x00B8, SI8_CTRL_0),
	REGNAME(bcrm_ifr_pdn, 0x00E8, SI7_CTRL_0),
	REGNAME(bcrm_ifr_pdn, 0x015C, SIP9_CTRL_0),
	REGNAME(bcrm_ifr_pdn, 0x0164, ASL2A_CTRL_0),
	REGNAME(bcrm_ifr_pdn, 0x01C0, SI17_CTRL_0),
	REGNAME(bcrm_ifr_pdn, 0x01C4, SI17_CTRL_1),
	REGNAME(bcrm_ifr_pdn, 0x01C8, SI17_CTRL_2),
	REGNAME(bcrm_ifr_pdn, 0x01CC, SI17B_CTRL_0),
	REGNAME(bcrm_ifr_pdn, 0x01D0, SI17B_CTRL_1),
	REGNAME(bcrm_ifr_pdn, 0x01D4, SI17B_CTRL_2),
	REGNAME(bcrm_ifr_pdn, 0x02b8, INFRA_QAXI_BUS_DOM8),
	REGNAME(bcrm_ifr_pdn1, 0x0034, SI19_CTRL_0),
	REGNAME(bcrm_ifr_pdn1, 0x0038, SI19_CTRL_1),
	REGNAME(bcrm_ifr_pdn1, 0x003C, SI19B_CTRL_0),
	REGNAME(bcrm_ifr_pdn1, 0x0040, SI19B_CTRL_1),
	REGNAME(bcrm_ifr_pdn1, 0x0044, SI19B_CTRL_2),
	REGNAME(bcrm_ifr_pdn1, 0x0048, SI19B_CTRL_3),
	REGNAME(bcrm_ifr_pdn1, 0x004C, SI19B_CTRL_4),
	REGNAME(bcrm_ifr_pdn1, 0x0050, SI19B_CTRL_5),
	REGNAME(bcrm_ifr_pdn1, 0x0054, SI19B_CTRL_6),
	REGNAME(bcrm_ifr_pdn1, 0x0058, SI19C_CTRL_0),
	/* mm proc debug */
	REGNAME(hfrp, 0x0150, BUS_DBG_OUT),
	/* mminfra debug */
	REGNAME(mminfra_smi, 0x0400, SMI_DEBUG_S0),
	REGNAME(mminfra_smi, 0x0404, SMI_DEBUG_S1),
	REGNAME(mminfra_smi, 0x0408, SMI_DEBUG_S2),
	REGNAME(mminfra_smi, 0x040C, SMI_DEBUG_S3),
	REGNAME(mminfra_smi, 0x0410, SMI_DEBUG_S4),
	REGNAME(mminfra_smi, 0x0414, SMI_DEBUG_S5),
	REGNAME(mminfra_smi, 0x0418, SMI_DEBUG_S6),
	REGNAME(mminfra_smi, 0x041C, SMI_DEBUG_S7),
	REGNAME(mminfra_smi, 0x0430, SMI_DEBUG_M0),
	REGNAME(mminfra_smi, 0x0434, SMI_DEBUG_M1),
	REGNAME(mminfra_smi, 0x0440, SMI_DEBUG_MISC),
	/* sspm debug */
	REGNAME(sspm, 0x0000, INTC_IRQ_RAW_STA0),
	REGNAME(sspm, 0x0004, INTC_IRQ_RAW_STA1),
	REGNAME(sspm, 0x0010, INTC_IRQ_STA0),
	REGNAME(sspm, 0x0014, INTC_IRQ_STA1),
	REGNAME(sspm, 0x0020, INTC_IRQ_EN0),
	REGNAME(sspm, 0x0024, INTC_IRQ_EN1),
	REGNAME(sspm, 0x0070, INTC_IRQ_GRP2_0),
	REGNAME(sspm, 0x0074, INTC_IRQ_GRP2_1),
	REGNAME(sspm, 0x00C0, INTC_IRQ_GRP7_0),
	REGNAME(sspm, 0x00C4, INTC_IRQ_GRP7_1),
	REGNAME(sspm, 0x0170, INTC_IRQ_GRP2_STA0),
	REGNAME(sspm, 0x0174, INTC_IRQ_GRP2_STA1),
	REGNAME(sspm, 0x01C0, INTC_IRQ_GRP7_STA0),
	REGNAME(sspm, 0x01C4, INTC_IRQ_GRP7_STA1),
	REGNAME(sspm_cfg, 0x0018, SSPM_WFI),
	{},
};

static const struct regname *get_all_mt6879_regnames(void)
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
	{"vlp_cksys", PWR_STA, 0x00000000},
	{"mfg_pll_ctrl", PWR_STA, 0x00000000},
	{"apu_pll_ctrl", PWR_STA, 0x00000000},
	{"afe", PWR_STA, 0x00000020},
	{"camsys_mraw", PWR_STA, 0x00040000},
	{"camsys_rawa", PWR_STA, 0x00080000},
	{"camsys_rawb", PWR_STA, 0x00100000},
	{"camsys_yuva", PWR_STA, 0x00080000},
	{"camsys_yuvb", PWR_STA, 0x00100000},
	{"cam_main_r1a", PWR_STA, 0x00020000},
	{"ccu", PWR_STA, 0x00020000},
	{"dip_nr_dip1", PWR_STA, 0x00000400},
	{"dip_top_dip1", PWR_STA, 0x00000400},
	{"dispsys", PWR_STA, 0x02000000},
	{"imgsys_main", PWR_STA, 0x00000200},
	{"imp_iic_wrap_c", PWR_STA, 0x00000000},
	{"imp_iic_wrap_e", PWR_STA, 0x00000000},
	{"imp_iic_wrap_en", PWR_STA, 0x00000000},
	{"imp_iic_wrap_w", PWR_STA, 0x00000000},
	{"infracfg", PWR_STA, 0x00000000},
	{"infracfg_ao", PWR_STA, 0x00000000},
	{"ipesys", PWR_STA, 0x00000800},
	{"mdpsys", PWR_STA, 0x02000000},
	{"mfg", XPU_PWR_STA, 0x00000080},
	{"mminfra_config", PWR_STA, 0x08000000},
	{"nemi_reg", PWR_STA, 0x00000000},
	{"pericfg_ao", PWR_STA, 0x00000000},
	{"ufs_ao_config", PWR_STA, 0x00000000},
	{"ufs_pdn_cfg", PWR_STA, 0x00000000},
	{"vdec_gcon_base", PWR_STA, 0x00002000},
	{"vencsys", PWR_STA, 0x00008000},
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

static unsigned int reg_dump_addr[ARRAY_SIZE(rn) - 1];
static unsigned int reg_dump_val[ARRAY_SIZE(rn) - 1];
static bool reg_dump_valid[ARRAY_SIZE(rn) - 1];

void set_subsys_reg_dump_mt6879(enum chk_sys_id id[])
{
	const struct regname *rns = &rn[0];
	int i, j;

	if (rns == NULL)
		return;

	for (i = 0; i < ARRAY_SIZE(rn) - 1; i++, rns++) {
		if (!is_valid_reg(ADDR(rns)))
			continue;

		for (j = 0; id[j] != chk_sys_num; j++) {
			/* filter out the subsys that we don't want */
			if (rns->id == id[j])
				break;
		}

		if (id[j] == chk_sys_num)
			continue;

		reg_dump_addr[i] = PHYSADDR(rns);
		reg_dump_val[i] = clk_readl(ADDR(rns));
		/* record each register dump index validation */
		reg_dump_valid[i] = true;
	}
}
EXPORT_SYMBOL(set_subsys_reg_dump_mt6879);

void get_subsys_reg_dump_mt6879(void)
{
	const struct regname *rns = &rn[0];
	int i;

	if (rns == NULL)
		return;

	for (i = 0; i < ARRAY_SIZE(rn) - 1; i++, rns++) {
		if (reg_dump_valid[i])
			pr_info("%-18s: [0x%08x] = 0x%08x\n",
					rns->name, reg_dump_addr[i], reg_dump_val[i]);
	}
}
EXPORT_SYMBOL(get_subsys_reg_dump_mt6879);

void print_subsys_reg_mt6879(enum chk_sys_id id)
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
EXPORT_SYMBOL(print_subsys_reg_mt6879);

#if IS_ENABLED(CONFIG_MTK_DEVAPC)
static enum chk_sys_id devapc_dump_id[] = {
	spm,
	top,
	infracfg,
	apmixed,
	mfg_ao,
	apu_ao,
	vlpcfg,
	vlp_ck,
	chk_sys_num,
};

static void devapc_dump(void)
{
	set_subsys_reg_dump_mt6879(devapc_dump_id);
	get_subsys_reg_dump_mt6879();
}

static struct devapc_vio_callbacks devapc_vio_handle = {
	.id = DEVAPC_SUBSYS_CLKMGR,
	.debug_dump = devapc_dump,
};

#endif

static const char * const off_pll_names[] = {
	"univpll",
	"msdcpll",
	"mmpll",
	"tvdpll",
	"imgpll",
	"mfg_ao_mfgpll",
	"mfg_ao_mfgscpll",
	"apu_ao_apupll",
	"apu_ao_npupll",
	"apu_ao_apupll1",
	"apu_ao_apupll2",
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

static void dump_hwv_history(struct regmap *regmap, u32 id)
{
	u32 addr[16], val[16];
	u32 idx, set, sta, set_sta, clr_sta, en, done;
	int i;

	regmap_write(regmap, HWV_DOMAIN_KEY, HWV_SECURE_KEY);
	regmap_read(regmap, HWV_CG_SET(id), &set);
	regmap_read(regmap, HWV_CG_STA(id), &sta);
	regmap_read(regmap, HWV_CG_SET_STA(id), &set_sta);
	regmap_read(regmap, HWV_CG_CLR_STA(id), &clr_sta);
	regmap_read(regmap, HWV_CG_EN(id), &en);
	regmap_read(regmap, HWV_CG_DONE(id), &done);

	for (i = 0; i < 16; i++) {
		regmap_read(regmap, HWV_ADDR_HISTORY_0 + (0x4 * i), addr + i);
		regmap_read(regmap, HWV_DATA_HISTORY_0 + (0x4 * i), val + i);
	}
	regmap_read(regmap, HWV_IDX_POINTER, &idx);
	pr_notice("idx: 0x%x\n", val);
	pr_notice("[%d](%x)%x, (%x)%x, (%x)%x, (%x)%x, (%x)%x, (%x)%x\n",
			id, HWV_CG_SET(id), set, HWV_CG_STA(id), sta,
			HWV_CG_SET_STA(id), set_sta, HWV_CG_CLR_STA(id), clr_sta,
			HWV_CG_EN(id), en, HWV_CG_DONE(id), done);

	for (i = 0; i < 16; i++)
		pr_notice("[%d]addr: 0x%x, data: 0x%x\n", i, addr[i], val[i]);
}

static enum chk_sys_id bus_dump_id[] = {
	infracfg1,
	bcrm_ifr_ao,
	bcrm_ifr_ao1,
	bcrm_ifr_pdn,
	bcrm_ifr_pdn1,
	chk_sys_num,
};

static void get_bus_reg(void)
{
	set_subsys_reg_dump_mt6879(bus_dump_id);
}

static void dump_bus_reg(struct regmap *regmap, u32 ofs)
{
	get_subsys_reg_dump_mt6879();
	set_subsys_reg_dump_mt6879(bus_dump_id);
	get_subsys_reg_dump_mt6879();
	/* sspm need some time to run isr */
	mdelay(1000);

	BUG_ON(1);
}

static enum chk_sys_id hwv_pll_dump_id[] = {
	sspm,
	sspm_cfg,
	apmixed,
	chk_sys_num,
};

static void dump_hwv_pll_reg(struct regmap *regmap, u32 shift)
{
	u32 val[7];

	regmap_read(regmap, HWV_PLL_SET, &val[0]);
	regmap_read(regmap, HWV_PLL_CLR, &val[1]);
	regmap_read(regmap, HWV_PLL_STA, &val[2]);
	regmap_read(regmap, HWV_PLL_EN, &val[3]);
	regmap_read(regmap, HWV_PLL_DONE, &val[4]);
	regmap_read(regmap, HWV_PLL_SET_STA, &val[5]);
	regmap_read(regmap, HWV_PLL_CLR_STA, &val[6]);

	set_subsys_reg_dump_mt6879(hwv_pll_dump_id);

	regmap_write(regmap, HWV_DOMAIN_KEY, HWV_SECURE_KEY);

	pr_notice("[%x]%x, [%x]%x, [%x]%x, [%x]%x, [%x]%x, [%x]%x, [%x]%x\n",
			HWV_PLL_SET, val[0],
			HWV_PLL_CLR, val[1],
			HWV_PLL_STA, val[2],
			HWV_PLL_EN, val[3],
			HWV_PLL_DONE, val[4],
			HWV_PLL_SET_STA, val[5],
			HWV_PLL_CLR_STA, val[6]);

	get_subsys_reg_dump_mt6879();

	mdelay(100);

	BUG_ON(1);
}

static bool is_cg_chk_pwr_on(void)
{
#if CG_CHK_PWRON_ENABLE
	return true;
#endif
	return false;
}

/*
 * init functions
 */

static struct clkchk_ops clkchk_mt6879_ops = {
	.get_all_regnames = get_all_mt6879_regnames,
	.get_spm_pwr_status_array = get_spm_pwr_status_array,
	.get_pvd_pwr_mask = get_pvd_pwr_mask,
	.get_off_pll_names = get_off_pll_names,
	.get_notice_pll_names = get_notice_pll_names,
	.is_pll_chk_bug_on = is_pll_chk_bug_on,
	.get_vf_table = get_vf_table,
	.get_vcore_opp = get_vcore_opp,
	.devapc_dump = devapc_dump,
	.dump_hwv_history = dump_hwv_history,
	.get_bus_reg = get_bus_reg,
	.dump_bus_reg = dump_bus_reg,
	.dump_hwv_pll_reg = dump_hwv_pll_reg,
	.is_cg_chk_pwr_on = is_cg_chk_pwr_on,
};

static int clk_chk_mt6879_probe(struct platform_device *pdev)
{
	init_regbase();

	set_clkchk_notify();

	set_clkchk_ops(&clkchk_mt6879_ops);

#if IS_ENABLED(CONFIG_MTK_DEVAPC)
	register_devapc_vio_callback(&devapc_vio_handle);
#endif

	return 0;
}

static struct platform_driver clk_chk_mt6879_drv = {
	.probe = clk_chk_mt6879_probe,
	.driver = {
		.name = "clk-chk-mt6879",
		.owner = THIS_MODULE,
		.pm = &clk_chk_dev_pm_ops,
	},
};

/*
 * init functions
 */

static int __init clkchk_mt6879_init(void)
{
	static struct platform_device *clk_chk_dev;

	clk_chk_dev = platform_device_register_simple("clk-chk-mt6879", -1, NULL, 0);
	if (IS_ERR(clk_chk_dev))
		pr_warn("unable to register clk-chk device");

	return platform_driver_register(&clk_chk_mt6879_drv);
}

static void __exit clkchk_mt6879_exit(void)
{
	platform_driver_unregister(&clk_chk_mt6879_drv);
}

subsys_initcall(clkchk_mt6879_init);
module_exit(clkchk_mt6879_exit);
MODULE_LICENSE("GPL");
