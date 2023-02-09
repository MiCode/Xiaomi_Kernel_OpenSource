// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Chong-ming Wei <chong-ming.wei@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>

#include <dt-bindings/power/mt6886-power.h>

#if IS_ENABLED(CONFIG_MTK_DEVAPC)
#include <devapc_public.h>
#endif

#if IS_ENABLED(CONFIG_MTK_DVFSRC_HELPER)
#include <mt-plat/dvfsrc-exp.h>
#endif

#include "clkchk.h"
#include "clkchk-mt6886.h"
#include "clk-fmeter.h"
#include "clk-mt6886-fmeter.h"

#define BUG_ON_CHK_ENABLE		0
#define CHECK_VCORE_FREQ		1
#define CG_CHK_PWRON_ENABLE		0

#define HWV_DOMAIN_KEY			0x055C
#define HWV_SECURE_KEY			0x10907
#define HWV_CG_SET(xpu, id)		((0x200 * (xpu)) + (id * 0x8))
#define HWV_CG_STA(id)			(0x1800 + (id * 0x4))
#define HWV_CG_EN(id)			(0x1900 + (id * 0x4))
#define HWV_CG_SET_STA(id)		(0x1A00 + (id * 0x4))
#define HWV_CG_CLR_STA(id)		(0x1B00 + (id * 0x4))
#define HWV_CG_DONE(id)			(0x1C00 + (id * 0x4))


static unsigned int suspend_cnt;

/* xpu*/
enum {
	APMCU = 0,
	MD,
	SSPM,
	MMUP,
	SCP,
	XPU_NUM,
};

static u32 xpu_id[XPU_NUM] = {
	[APMCU] = 0,
	[MD] = 2,
	[SSPM] = 4,
	[MMUP] = 7,
	[SCP] = 9,
};

enum {
	CHK_FM_ADSPPLL = 0,
	CHK_FM_MAINPLL,
	CHK_FM_MMPLL,
	CHK_FM_MSDCPLL,
	CHK_FM_IMGPLL,
	CHK_FM_UNIVPLL,
	CHK_FM_UFS,
	CHK_FM_IMG1,
	CHK_FM_IPE,
	CHK_FM_VDE,
	CHK_FM_NUM,
};

struct clkchk_fm {
	const char *fm_name;
	unsigned int fm_id;
	unsigned int fm_type;
};

struct  clkchk_fm chk_fm_list[] = {
	[CHK_FM_ADSPPLL] = {"adsppll", FM_ADSPPLL_CK, ABIST},
	[CHK_FM_MAINPLL] = {"mainpll", FM_MAINPLL_CK, ABIST},
	[CHK_FM_MMPLL] = {"mmpll", FM_MMPLL_CK, ABIST},
	[CHK_FM_MSDCPLL] = {"msdcpll", FM_MSDCPLL_CK, ABIST},
	[CHK_FM_IMGPLL] = {"imgpll", FM_IMGPLL_CK, ABIST},
	[CHK_FM_UNIVPLL] = {"univpll", FM_UNIVPLL_CK, ABIST},
	[CHK_FM_UFS] = {"ufs sel", FM_U_CK, CKGEN},
	[CHK_FM_IMG1] = {"img1 sel", FM_IMG1_CK, CKGEN},
	[CHK_FM_IPE] = {"ipe sel", FM_IPE_CK, CKGEN},
	[CHK_FM_VDE] = {"vde sel", FM_VDEC_CK, CKGEN},
	{},
};

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
	[emi_reg] = REGBASE_V(0x10219000, emi_reg, PD_NULL, CLK_NULL),
	[emi_bus] = REGBASE_V(0x10270000, emi_bus, PD_NULL, CLK_NULL),
	[perao] = REGBASE_V(0x11036000, perao, PD_NULL, CLK_NULL),
	[afe] = REGBASE_V(0x11050000, afe, MT6886_CHK_PD_AUDIO, CLK_NULL),
	[impc] = REGBASE_V(0x11284000, impc, PD_NULL, "i2c_sel"),
	[ufscfg_ao_bus] = REGBASE_V(0x112B8000, ufscfg_ao_bus, PD_NULL, CLK_NULL),
	[ufsao] = REGBASE_V(0x112b8000, ufsao, PD_NULL, CLK_NULL),
	[ufspdn] = REGBASE_V(0x112bc000, ufspdn, PD_NULL, CLK_NULL),
	[impes] = REGBASE_V(0x11C73000, impes, PD_NULL, "i2c_sel"),
	[impw] = REGBASE_V(0x11E02000, impw, PD_NULL, "i2c_sel"),
	[impe] = REGBASE_V(0x11E83000, impe, PD_NULL, "i2c_sel"),
	[gpu_eb_rpc] = REGBASE_V(0x13F91000, gpu_eb_rpc, PD_NULL, CLK_NULL),
	[mfg_ao] = REGBASE_V(0x13fa0000, mfg_ao, PD_NULL, CLK_NULL),
	[mfgsc_ao] = REGBASE_V(0x13fa0c00, mfgsc_ao, PD_NULL, CLK_NULL),
	[mm] = REGBASE_V(0x14000000, mm, MT6886_CHK_PD_DIS0, CLK_NULL),
	[img] = REGBASE_V(0x15000000, img, MT6886_CHK_PD_ISP_MAIN, CLK_NULL),
	[img_sub0_bus] = REGBASE_V(0x15002000, img_sub0_bus, MT6886_CHK_PD_ISP_MAIN, CLK_NULL),
	[img_sub1_bus] = REGBASE_V(0x15003000, img_sub1_bus, MT6886_CHK_PD_ISP_MAIN, CLK_NULL),
	[dip_top_dip1] = REGBASE_V(0x15110000, dip_top_dip1, MT6886_CHK_PD_ISP_DIP1, CLK_NULL),
	[dip_nr1_dip1] = REGBASE_V(0x15130000, dip_nr1_dip1, MT6886_CHK_PD_ISP_DIP1, CLK_NULL),
	[dip_nr2_dip1] = REGBASE_V(0x15170000, dip_nr2_dip1, MT6886_CHK_PD_ISP_DIP1, CLK_NULL),
	[wpe1_dip1] = REGBASE_V(0x15220000, wpe1_dip1, MT6886_CHK_PD_ISP_DIP1, CLK_NULL),
	[wpe2_dip1] = REGBASE_V(0x15520000, wpe2_dip1, MT6886_CHK_PD_ISP_DIP1, CLK_NULL),
	[wpe3_dip1] = REGBASE_V(0x15620000, wpe3_dip1, MT6886_CHK_PD_ISP_DIP1, CLK_NULL),
	[traw_dip1] = REGBASE_V(0x15710000, traw_dip1, MT6886_CHK_PD_ISP_DIP1, CLK_NULL),
	[vde2] = REGBASE_V(0x1602f000, vde2, MT6886_CHK_PD_VDE0, CLK_NULL),
	[ven] = REGBASE_V(0x17000000, ven, MT6886_CHK_PD_VEN0, CLK_NULL),
	[cam_sub0_bus] = REGBASE_V(0x1A005000, cam_sub0_bus, MT6886_CHK_PD_CAM_MAIN, CLK_NULL),
	[cam_sub2_bus] = REGBASE_V(0x1A006000, cam_sub2_bus, MT6886_CHK_PD_CAM_MAIN, CLK_NULL),
	[cam_sub1_bus] = REGBASE_V(0x1A007000, cam_sub1_bus, MT6886_CHK_PD_CAM_MAIN, CLK_NULL),
	[spm] = REGBASE_V(0x1C001000, spm, PD_NULL, CLK_NULL),
	[vlpcfg] = REGBASE_V(0x1C00C000, vlpcfg, PD_NULL, CLK_NULL),
	[vlp_ck] = REGBASE_V(0x1C013000, vlp_ck, PD_NULL, CLK_NULL),
	[scp] = REGBASE_V(0x1C721000, scp, PD_NULL, CLK_NULL),
	[scp_iic] = REGBASE_V(0x1C7B7000, scp_iic, PD_NULL, CLK_NULL),
	[cam_m] = REGBASE_V(0x1a000000, cam_m, MT6886_CHK_PD_CAM_MAIN, CLK_NULL),
	[cam_ra] = REGBASE_V(0x1a04f000, cam_ra, MT6886_CHK_PD_CAM_SUBA, CLK_NULL),
	[cam_ya] = REGBASE_V(0x1a06f000, cam_ya, MT6886_CHK_PD_CAM_SUBA, CLK_NULL),
	[cam_rb] = REGBASE_V(0x1a08f000, cam_rb, MT6886_CHK_PD_CAM_SUBB, CLK_NULL),
	[cam_yb] = REGBASE_V(0x1a0af000, cam_yb, MT6886_CHK_PD_CAM_SUBB, CLK_NULL),
	[cam_mr] = REGBASE_V(0x1a170000, cam_mr, MT6886_CHK_PD_CAM_MRAW, CLK_NULL),
	[ccu] = REGBASE_V(0x1b200000, ccu, MT6886_CHK_PD_CAM_MAIN, CLK_NULL),
	[dvfsrc_apb] = REGBASE_V(0x1c00f000, dvfsrc_apb, PD_NULL, CLK_NULL),
	[mminfra_config] = REGBASE_V(0x1e800000, mminfra_config, MT6886_CHK_PD_MM_INFRA, CLK_NULL),
	[mdp] = REGBASE_V(0x1f000000, mdp, MT6886_CHK_PD_MDP0, CLK_NULL),
	[cci] = REGBASE_V(0xc030000, cci, PD_NULL, CLK_NULL),
	[cpu_ll] = REGBASE_V(0xc030400, cpu_ll, PD_NULL, CLK_NULL),
	[cpu_bl] = REGBASE_V(0xc030800, cpu_bl, PD_NULL, CLK_NULL),
	[ptp] = REGBASE_V(0xc034000, ptp, PD_NULL, CLK_NULL),
	[hwv_wrt] = REGBASE_V(0x10321000, hwv_wrt, PD_NULL, CLK_NULL),
	[hwv] = REGBASE_V(0x10320000, hwv, PD_NULL, CLK_NULL),
	[hwv_ext] = REGBASE_V(0x10321000, hwv_ext, PD_NULL, CLK_NULL),
	{},
};

#define REGNAME(_base, _ofs, _name)	\
	{ .base = &rb[_base], .id = _base, .ofs = _ofs, .name = #_name }

static struct regname rn[] = {
	/* TOPCKGEN register */
	REGNAME(top, 0x0010, CLK_CFG_0),
	REGNAME(top, 0x0020, CLK_CFG_1),
	REGNAME(top, 0x0030, CLK_CFG_2),
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
	REGNAME(top, 0x0320, CLK_AUDDIV_0),
	REGNAME(top, 0x0328, CLK_AUDDIV_2),
	REGNAME(top, 0x0334, CLK_AUDDIV_3),
	REGNAME(top, 0x0338, CLK_AUDDIV_4),
	REGNAME(top, 0x033C, CLK_AUDDIV_5),
	REGNAME(top, 0x240, CLK_MISC_CFG_0),
	REGNAME(top, 0x0, CLK_MODE),
	/* INFRACFG_AO register */
	REGNAME(ifrao, 0x70, INFRA_BUS_DCM_CTRL),
	REGNAME(ifrao, 0x90, MODULE_CG_0),
	REGNAME(ifrao, 0x94, MODULE_CG_1),
	REGNAME(ifrao, 0xAC, MODULE_CG_2),
	REGNAME(ifrao, 0xC8, MODULE_CG_3),
	REGNAME(ifrao, 0xE8, MODULE_CG_4),
	REGNAME(ifrao, 0x74, PERI_BUS_DCM_CTRL),
	/* INFRACFG_AO_BUS register */
	REGNAME(infracfg, 0x0C50, INFRASYS_PROTECT_EN_STA_1),
	REGNAME(infracfg, 0x0C5C, INFRASYS_PROTECT_RDY_STA_1),
	REGNAME(infracfg, 0x0C60, EMISYS_PROTECT_EN_STA_0),
	REGNAME(infracfg, 0x0C6C, EMISYS_PROTECT_RDY_STA_0),
	REGNAME(infracfg, 0x0C90, MCU_CONNSYS_PROTECT_EN_STA_0),
	REGNAME(infracfg, 0x0C9C, MCU_CONNSYS_PROTECT_RDY_STA_0),
	REGNAME(infracfg, 0x0C40, INFRASYS_PROTECT_EN_STA_0),
	REGNAME(infracfg, 0x0C4C, INFRASYS_PROTECT_RDY_STA_0),
	REGNAME(infracfg, 0x0CA0, MD_MFGSYS_PROTECT_EN_STA_0),
	REGNAME(infracfg, 0x0CAC, MD_MFGSYS_PROTECT_RDY_STA_0),
	REGNAME(infracfg, 0x0C20, MMSYS_PROTECT_EN_STA_1),
	REGNAME(infracfg, 0x0C2C, MMSYS_PROTECT_RDY_STA_1),
	REGNAME(infracfg, 0x0CC0, DRAMC_CCUSYS_PROTECT_EN_STA_0),
	REGNAME(infracfg, 0x0CCC, DRAMC_CCUSYS_PROTECT_RDY_STA_0),
	REGNAME(infracfg, 0x0C80, PERISYS_PROTECT_EN_STA_0),
	REGNAME(infracfg, 0x0C8C, PERISYS_PROTECT_RDY_STA_0),
	REGNAME(infracfg, 0x0CB0, ADSP_APUSYS_PROTECT_EN_STA_0),
	REGNAME(infracfg, 0x0CBC, ADSP_APUSYS_PROTECT_RDY_STA_0),
	REGNAME(infracfg, 0x0C30, MMSYS_PROTECT_EN_STA_2),
	REGNAME(infracfg, 0x0C3C, MMSYS_PROTECT_RDY_STA_2),
	REGNAME(infracfg, 0x0C10, MMSYS_PROTECT_EN_STA_0),
	REGNAME(infracfg, 0x0C1C, MMSYS_PROTECT_RDY_STA_0),
	/* APMIXEDSYS register */
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
	REGNAME(apmixed, 0x248, UFSPLL_CON0),
	REGNAME(apmixed, 0x24c, UFSPLL_CON1),
	REGNAME(apmixed, 0x250, UFSPLL_CON2),
	REGNAME(apmixed, 0x254, UFSPLL_CON3),
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
	REGNAME(apmixed, 0x3b0, EMIPLL_CON0),
	REGNAME(apmixed, 0x3b4, EMIPLL_CON1),
	REGNAME(apmixed, 0x3b8, EMIPLL_CON2),
	REGNAME(apmixed, 0x3bc, EMIPLL_CON3),
	REGNAME(apmixed, 0x370, IMGPLL_CON0),
	REGNAME(apmixed, 0x374, IMGPLL_CON1),
	REGNAME(apmixed, 0x378, IMGPLL_CON2),
	REGNAME(apmixed, 0x37c, IMGPLL_CON3),
	/* EMI_REG register */
	REGNAME(emi_reg, 0x858, EMI_THRO_CTRL1),
	/* EMI_BUS register */
	REGNAME(emi_bus, 0x40, GLITCH_PROT_EN),
	REGNAME(emi_bus, 0x8c, GLITCH_PROT_RDY),
	/* PERICFG_AO register */
	REGNAME(perao, 0x10, PERI_CG_0),
	REGNAME(perao, 0x14, PERI_CG_1),
	REGNAME(perao, 0x18, PERI_CG_2),
	/* AFE register */
	REGNAME(afe, 0x0, AUDIO_TOP_0),
	REGNAME(afe, 0x4, AUDIO_TOP_1),
	REGNAME(afe, 0x8, AUDIO_TOP_2),
	/* IMP_IIC_WRAP_C register */
	REGNAME(impc, 0xE00, AP_CLOCK_CG_CEN),
	/* UFSCFG_AO_BUS register */
	REGNAME(ufscfg_ao_bus, 0x50, UFS_AO2FE_SLPPROT_EN),
	REGNAME(ufscfg_ao_bus, 0x5c, UFS_AO2FE_SLPPROT_RDY_STA),
	/* UFSCFG_AO register */
	REGNAME(ufsao, 0x4, UFS_AO_CG_0),
	/* UFSCFG_PDN register */
	REGNAME(ufspdn, 0x4, UFS_PDN_CG_0),
	/* IMP_IIC_WRAP_ES register */
	REGNAME(impes, 0xE00, AP_CLOCK_CG_ES),
	/* IMP_IIC_WRAP_W register */
	REGNAME(impw, 0xE00, AP_CLOCK_CG_WST),
	/* IMP_IIC_WRAP_E register */
	REGNAME(impe, 0xE00, AP_CLOCK_CG_EST),
	/* GPU_EB_RPC register */
	REGNAME(gpu_eb_rpc, 0x70, MFG1_PWR_CON),
	REGNAME(gpu_eb_rpc, 0xF88, XPU_PWR_STATUS),
	REGNAME(gpu_eb_rpc, 0xF8C, XPU_PWR_STATUS_2ND),
	REGNAME(gpu_eb_rpc, 0xA0, MFG2_PWR_CON),
	REGNAME(gpu_eb_rpc, 0xBC, MFG9_PWR_CON),
	REGNAME(gpu_eb_rpc, 0xC0, MFG10_PWR_CON),
	REGNAME(gpu_eb_rpc, 0xC4, MFG11_PWR_CON),
	REGNAME(gpu_eb_rpc, 0xC8, MFG12_PWR_CON),
	REGNAME(gpu_eb_rpc, 0x40, MFGSYS_RPC_PROTECT_EN_SET_0),
	REGNAME(gpu_eb_rpc, 0x48, MFGSYS_RPC_PROTECT_RDY_STA_0),
	/* MFGPLL_PLL_CTRL register */
	REGNAME(mfg_ao, 0x8, MFGPLL_CON0),
	REGNAME(mfg_ao, 0xc, MFGPLL_CON1),
	REGNAME(mfg_ao, 0x10, MFGPLL_CON2),
	REGNAME(mfg_ao, 0x14, MFGPLL_CON3),
	/* MFGSCPLL_PLL_CTRL register */
	REGNAME(mfgsc_ao, 0x8, MFGSCPLL_CON0),
	REGNAME(mfgsc_ao, 0xc, MFGSCPLL_CON1),
	REGNAME(mfgsc_ao, 0x10, MFGSCPLL_CON2),
	REGNAME(mfgsc_ao, 0x14, MFGSCPLL_CON3),
	/* DISPSYS_CONFIG register */
	REGNAME(mm, 0x100, MMSYS_CG_0),
	REGNAME(mm, 0x110, MMSYS_CG_1),
	REGNAME(mm, 0x1A0, MMSYS_CG_2),
	/* IMGSYS_MAIN register */
	REGNAME(img, 0x50, IMG_IPE_CG),
	REGNAME(img, 0x0, IMG_MAIN_CG),
	/* IMG_SUB0_BUS register */
	REGNAME(img_sub0_bus, 0x3c0, SMI_COMMON_PROTECT_EN),
	REGNAME(img_sub0_bus, 0x3c0, SMI_COMMON_PROTECT_EN),
	/* IMG_SUB1_BUS register */
	REGNAME(img_sub1_bus, 0x3c0, SMI_COMMON_PROTECT_EN),
	REGNAME(img_sub1_bus, 0x3c0, SMI_COMMON_PROTECT_EN),
	/* DIP_TOP_DIP1 register */
	REGNAME(dip_top_dip1, 0x0, MACRO_CG),
	/* DIP_NR1_DIP1 register */
	REGNAME(dip_nr1_dip1, 0x0, MACRO_CG),
	/* DIP_NR2_DIP1 register */
	REGNAME(dip_nr2_dip1, 0x0, MACRO_CG),
	/* WPE1_DIP1 register */
	REGNAME(wpe1_dip1, 0x0, MACRO_CG),
	/* WPE2_DIP1 register */
	REGNAME(wpe2_dip1, 0x0, MACRO_CG),
	/* WPE3_DIP1 register */
	REGNAME(wpe3_dip1, 0x0, MACRO_CG),
	/* TRAW_DIP1 register */
	REGNAME(traw_dip1, 0x0, MACRO_CG),
	/* VDEC_GCON_BASE register */
	REGNAME(vde2, 0x8, LARB_CKEN_CON),
	REGNAME(vde2, 0x200, LAT_CKEN),
	REGNAME(vde2, 0x190, MINI_MDP_CFG_0),
	REGNAME(vde2, 0x0, VDEC_CKEN),
	/* VENC_GCON register */
	REGNAME(ven, 0x0, VENCSYS_CG),
	/* CAM_SUB0_BUS register */
	REGNAME(cam_sub0_bus, 0x3c0, SMI_COMMON_PROTECT_EN),
	REGNAME(cam_sub0_bus, 0x3c0, SMI_COMMON_PROTECT_EN),
	/* CAM_SUB2_BUS register */
	REGNAME(cam_sub2_bus, 0x3c0, SMI_COMMON_PROTECT_EN),
	REGNAME(cam_sub2_bus, 0x3c0, SMI_COMMON_PROTECT_EN),
	/* CAM_SUB1_BUS register */
	REGNAME(cam_sub1_bus, 0x3c0, SMI_COMMON_PROTECT_EN),
	REGNAME(cam_sub1_bus, 0x3c0, SMI_COMMON_PROTECT_EN),
	/* SPM register */
	REGNAME(spm, 0xE00, MD1_PWR_CON),
	REGNAME(spm, 0xF78, PWR_STATUS),
	REGNAME(spm, 0xF7C, PWR_STATUS_2ND),
	REGNAME(spm, 0xF70, MD_BUCK_ISO_CON),
	REGNAME(spm, 0xF74, SOC_BUCK_ISO_CON),
	REGNAME(spm, 0xE04, CONN_PWR_CON),
	REGNAME(spm, 0xE10, UFS0_PWR_CON),
	REGNAME(spm, 0xE14, UFS0_PHY_PWR_CON),
	REGNAME(spm, 0xE2C, AUDIO_PWR_CON),
	REGNAME(spm, 0xE30, ADSP_TOP_PWR_CON),
	REGNAME(spm, 0xE34, ADSP_INFRA_PWR_CON),
	REGNAME(spm, 0xE38, ADSP_AO_PWR_CON),
	REGNAME(spm, 0xE3C, ISP_MAIN_PWR_CON),
	REGNAME(spm, 0xE40, ISP_DIP1_PWR_CON),
	REGNAME(spm, 0xE48, ISP_VCORE_PWR_CON),
	REGNAME(spm, 0xE4C, VDE0_PWR_CON),
	REGNAME(spm, 0xE54, VEN0_PWR_CON),
	REGNAME(spm, 0xE60, CAM_MAIN_PWR_CON),
	REGNAME(spm, 0xE64, CAM_MRAW_PWR_CON),
	REGNAME(spm, 0xE68, CAM_SUBA_PWR_CON),
	REGNAME(spm, 0xE6C, CAM_SUBB_PWR_CON),
	REGNAME(spm, 0xE74, CAM_VCORE_PWR_CON),
	REGNAME(spm, 0xE78, MDP0_PWR_CON),
	REGNAME(spm, 0xE80, DIS0_PWR_CON),
	REGNAME(spm, 0xE90, MM_INFRA_PWR_CON),
	REGNAME(spm, 0xE94, MM_PROC_PWR_CON),
	REGNAME(spm, 0xEE0, DXCC_PWR_CON),
	REGNAME(spm, 0xEE8, MFG0_PWR_CON),
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
	REGNAME(vlp_ck, 0x0038, VLP_CLK_CFG_4),
	/* SCP register */
	REGNAME(scp, 0x58, CLR_CLK_CG),
	REGNAME(scp, 0x30, SET_CLK_CG),
	/* SCP_IIC register */
	REGNAME(scp_iic, 0xE10, CCU_CLOCK_CG_CEN),
	/* CAMSYS_MAIN register */
	REGNAME(cam_m, 0x0, CAM_MAIN_CG_0),
	REGNAME(cam_m, 0x4C, CAM_MAIN_CG_1),
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
	/* DVFSRC_APB register */
	REGNAME(dvfsrc_apb, 0x0, DVFSRC_BASIC_CONTROL),
	/* MMINFRA_CONFIG register */
	REGNAME(mminfra_config, 0x100, MMINFRA_CG_0),
	REGNAME(mminfra_config, 0x110, MMINFRA_CG_1),
	/* MDPSYS_CONFIG register */
	REGNAME(mdp, 0x100, MDPSYS_CG_0),
	/* CCIPLL_PLL_CTRL register */
	REGNAME(cci, 0x8, CCIPLL_CON0),
	REGNAME(cci, 0xc, CCIPLL_CON1),
	REGNAME(cci, 0x10, CCIPLL_CON2),
	REGNAME(cci, 0x14, CCIPLL_CON3),
	/* ARMPLL_LL_PLL_CTRL register */
	REGNAME(cpu_ll, 0x8, ARMPLL_LL_CON0),
	REGNAME(cpu_ll, 0xc, ARMPLL_LL_CON1),
	REGNAME(cpu_ll, 0x10, ARMPLL_LL_CON2),
	REGNAME(cpu_ll, 0x14, ARMPLL_LL_CON3),
	/* ARMPLL_BL_PLL_CTRL register */
	REGNAME(cpu_bl, 0x8, ARMPLL_BL_CON0),
	REGNAME(cpu_bl, 0xc, ARMPLL_BL_CON1),
	REGNAME(cpu_bl, 0x10, ARMPLL_BL_CON2),
	REGNAME(cpu_bl, 0x14, ARMPLL_BL_CON3),
	/* PTPPLL_PLL_CTRL register */
	REGNAME(ptp, 0x8, PTPPLL_CON0),
	REGNAME(ptp, 0xc, PTPPLL_CON1),
	REGNAME(ptp, 0x10, PTPPLL_CON2),
	REGNAME(ptp, 0x14, PTPPLL_CON3),
	/* HWV register */
	REGNAME(hwv_wrt, 0x055C, HWV_DOMAIN_KEY),
	REGNAME(hwv, 0x0190, HW_CCF_AP_PLL_SET),
	REGNAME(hwv, 0x0990, HW_CCF_SSPM_PLL_SET),
	REGNAME(hwv, 0x0F90, HW_CCF_MMUP_PLL_SET),
	REGNAME(hwv_ext, 0x0390, HW_CCF_SCP_PLL_SET),
	REGNAME(hwv_ext, 0x0400, HW_CCF_PLL_ENABLE),
	REGNAME(hwv_ext, 0x0404, HW_CCF_PLL_STA),
	REGNAME(hwv_ext, 0x040C, HW_CCF_PLL_DONE),
	REGNAME(hwv_ext, 0x0464, HW_CCF_PLL_SET_STA),
	REGNAME(hwv_ext, 0x0468, HW_CCF_PLL_CLR_STA),
	REGNAME(hwv, 0x0198, HW_CCF_AP_MTCMOS_SET),
	REGNAME(hwv, 0x0998, HW_CCF_SSPM_MTCMOS_SET),
	REGNAME(hwv, 0x0F98, HW_CCF_MMUP_MTCMOS_SET),
	REGNAME(hwv_ext, 0x0398, HW_CCF_SCP_MTCMOS_SET),
	REGNAME(hwv_ext, 0x0410, HW_CCF_MTCMOS_ENABLE),
	REGNAME(hwv_ext, 0x0414, HW_CCF_MTCMOS_STA),
	REGNAME(hwv_ext, 0x041C, HW_CCF_MTCMOS_DONE),
	REGNAME(hwv_ext, 0x046C, HW_CCF_MTCMOS_SET_STA),
	REGNAME(hwv_ext, 0x0470, HW_CCF_MTCMOS_CLR_STA),
	REGNAME(hwv_ext, 0x0500, HW_CCF_IRQ_STATUS),
	REGNAME(hwv_ext, 0x0F04, HWV_ADDR_HISTORY_0),
	REGNAME(hwv_ext, 0x0F08, HWV_ADDR_HISTORY_1),
	REGNAME(hwv_ext, 0x0F0C, HWV_ADDR_HISTORY_2),
	REGNAME(hwv_ext, 0x0F10, HWV_ADDR_HISTORY_3),
	REGNAME(hwv_ext, 0x0F14, HWV_ADDR_HISTORY_4),
	REGNAME(hwv_ext, 0x0F18, HWV_ADDR_HISTORY_5),
	REGNAME(hwv_ext, 0x0F1C, HWV_ADDR_HISTORY_6),
	REGNAME(hwv_ext, 0x0F20, HWV_ADDR_HISTORY_7),
	REGNAME(hwv_ext, 0x0F24, HWV_ADDR_HISTORY_8),
	REGNAME(hwv_ext, 0x0F28, HWV_ADDR_HISTORY_9),
	REGNAME(hwv_ext, 0x0F2C, HWV_ADDR_HISTORY_10),
	REGNAME(hwv_ext, 0x0F30, HWV_ADDR_HISTORY_11),
	REGNAME(hwv_ext, 0x0F34, HWV_ADDR_HISTORY_12),
	REGNAME(hwv_ext, 0x0F38, HWV_ADDR_HISTORY_13),
	REGNAME(hwv_ext, 0x0F3C, HWV_ADDR_HISTORY_14),
	REGNAME(hwv_ext, 0x0F40, HWV_ADDR_HISTORY_15),
	REGNAME(hwv_ext, 0x0F44, HWV_DATA_HISTORY_0),
	REGNAME(hwv_ext, 0x0F48, HWV_DATA_HISTORY_1),
	REGNAME(hwv_ext, 0x0F4C, HWV_DATA_HISTORY_2),
	REGNAME(hwv_ext, 0x0F50, HWV_DATA_HISTORY_3),
	REGNAME(hwv_ext, 0x0F54, HWV_DATA_HISTORY_4),
	REGNAME(hwv_ext, 0x0F58, HWV_DATA_HISTORY_5),
	REGNAME(hwv_ext, 0x0F5C, HWV_DATA_HISTORY_6),
	REGNAME(hwv_ext, 0x0F60, HWV_DATA_HISTORY_7),
	REGNAME(hwv_ext, 0x0F64, HWV_DATA_HISTORY_8),
	REGNAME(hwv_ext, 0x0F68, HWV_DATA_HISTORY_9),
	REGNAME(hwv_ext, 0x0F6C, HWV_DATA_HISTORY_10),
	REGNAME(hwv_ext, 0x0F70, HWV_DATA_HISTORY_11),
	REGNAME(hwv_ext, 0x0F74, HWV_DATA_HISTORY_12),
	REGNAME(hwv_ext, 0x0F78, HWV_DATA_HISTORY_13),
	REGNAME(hwv_ext, 0x0F7C, HWV_DATA_HISTORY_14),
	REGNAME(hwv_ext, 0x0F70, HWV_DATA_HISTORY_15),
	REGNAME(hwv_ext, 0x0F84, HWV_IDX_POINTER),
	{},
};

static const struct regname *get_all_mt6886_regnames(void)
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

u32 get_mt6886_reg_value(u32 id, u32 ofs)
{
	if (id >= chk_sys_num)
		return 0;

	return clk_readl(rb[id].virt + ofs);
}
EXPORT_SYMBOL_GPL(get_mt6886_reg_value);

static void set_mt6886_reg_value(u32 id, u32 ofs, u32 val)
{
	if (id >= chk_sys_num)
		return;

	clk_writel(rb[id].virt + ofs, val);
}

void release_mt6886_hwv_secure(void)
{
	set_mt6886_reg_value(hwv_wrt, HWV_DOMAIN_KEY, HWV_SECURE_KEY);
}
EXPORT_SYMBOL_GPL(release_mt6886_hwv_secure);

/*
 * clkchk pwr_data
 */

struct pwr_data {
	const char *pvdname;
	enum chk_sys_id id;
	u32 base;
	u32 ofs;
};

static struct pwr_data pvd_pwr_data[] = {
	{"audiosys", afe, spm, 0x0E2C},
	{"camsys_main", cam_m, spm, 0x0E60},
	{"camsys_mraw", cam_mr, spm, 0x0E64},
	{"camsys_rawa", cam_ra, spm, 0x0E68},
	{"camsys_rawb", cam_rb, spm, 0x0E6C},
	{"camsys_yuva", cam_ya, spm, 0x0E68},
	{"camsys_yuvb", cam_yb, spm, 0x0E6C},
	{"ccu", ccu, spm, 0x0E60},
	{"dip_nr1_dip1", dip_nr1_dip1, spm, 0x0E40},
	{"dip_nr2_dip1", dip_nr2_dip1, spm, 0x0E40},
	{"dip_top_dip1", dip_top_dip1, spm, 0x0E40},
	{"mmsys0", mm, spm, 0x0E80},
	{"imgsys_main", img, spm, 0x0E3C},
	{"mdpsys", mdp, spm, 0x0E78},
	{"mminfra_config", mminfra_config, spm, 0x0E90},
	{"traw_dip1", traw_dip1, spm, 0x0E40},
	{"vdecsys", vde2, spm, 0x0E4C},
	{"vencsys", ven, spm, 0x0E54},
	{"wpe1_dip1", wpe1_dip1, spm, 0x0E40},
	{"wpe2_dip1", wpe2_dip1, spm, 0x0E40},
	{"wpe3_dip1", wpe3_dip1, spm, 0x0E40},
};

static int get_pvd_pwr_data_idx(const char *pvdname)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(pvd_pwr_data); i++) {
		if (pvd_pwr_data[i].pvdname == NULL)
			continue;
		if (!strcmp(pvdname, pvd_pwr_data[i].pvdname))
			return i;
	}

	return -1;
}

/*
 * clkchk pwr_status
 */
static u32 get_pwr_status(s32 idx)
{
	if (idx < 0 || idx >= ARRAY_SIZE(pvd_pwr_data))
		return 0;

	if (pvd_pwr_data[idx].id >= chk_sys_num)
		return 0;

	return  clk_readl(rb[pvd_pwr_data[idx].base].virt + pvd_pwr_data[idx].ofs);
}

static bool is_cg_chk_pwr_on(void)
{
#if CG_CHK_PWRON_ENABLE
	return true;
#endif
	return false;
}

#if CHECK_VCORE_FREQ
/*
 * clkchk vf table
 */

struct mtk_vf {
	const char *name;
	int freq_table[5];
};

#define MTK_VF_TABLE(_n, _freq0, _freq1, _freq2, _freq3, _freq4) {		\
		.name = _n,		\
		.freq_table = {_freq0, _freq1, _freq2, _freq3, _freq4},	\
	}

/*
 * Opp0 : 0p75v
 * Opp1 : 0p725v
 * Opp2 : 0p65v
 * Opp3 : 0p60v
 * Opp4 : 0p575v
 */
static struct mtk_vf vf_table[] = {
	/* Opp0, Opp1, Opp2, Opp3, Opp4 */
	MTK_VF_TABLE("axi_sel", 156000, 156000, 156000, 156000, 156000),
	MTK_VF_TABLE("peri_axi_sel", 156000, 156000, 156000, 156000, 156000),
	MTK_VF_TABLE("ufs_haxi_sel", 78000, 78000, 78000, 78000, 78000),
	MTK_VF_TABLE("bus_aximem_sel", 364000, 364000, 273000, 273000, 218400),
	MTK_VF_TABLE("disp0_sel", 660000, 624000, 458333, 249600, 208000),
	MTK_VF_TABLE("mdp0_sel", 660000, 624000, 458333, 273000, 229167),
	MTK_VF_TABLE("mminfra_sel", 660000, 624000, 458333, 273000, 229167),
	MTK_VF_TABLE("mmup_sel", 728000, 728000, 728000, 728000, 728000),
	MTK_VF_TABLE("dsp_sel", 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("camtg_sel", 52000, 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("camtg2_sel", 52000, 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("camtg3_sel", 52000, 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("camtg4_sel", 52000, 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("camtg5_sel", 52000, 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("camtg6_sel", 52000, 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("uart_sel", 26000, 26000, 26000, 26000, 26000),
	MTK_VF_TABLE("spi_sel", 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("msdc_macro_sel", 384000, 384000, 384000, 384000, 384000),
	MTK_VF_TABLE("msdc30_1_sel", 192000, 192000, 192000, 192000, 192000),
	MTK_VF_TABLE("msdc30_2_sel", 192000, 192000, 192000, 192000, 192000),
	MTK_VF_TABLE("audio_sel", 54600, 54600, 54600, 54600, 54600),
	MTK_VF_TABLE("aud_intbus_sel", 136500, 136500, 136500, 136500, 136500),
	MTK_VF_TABLE("atb_sel", 273000, 273000, 273000, 273000, 273000),
	MTK_VF_TABLE("disp_pwm_sel", 130000, 130000, 130000, 130000, 130000),
	MTK_VF_TABLE("usb_sel", 124800, 124800, 124800, 124800, 124800),
	MTK_VF_TABLE("ssusb_xhci_sel", 124800, 124800, 124800, 124800, 124800),
	MTK_VF_TABLE("i2c_sel", 124800, 124800, 124800, 124800, 124800),
	MTK_VF_TABLE("seninf_sel", 499200, 499200, 416000, 312000, 242667),
	MTK_VF_TABLE("seninf1_sel", 499200, 499200, 416000, 312000, 242667),
	MTK_VF_TABLE("seninf2_sel", 499200, 499200, 416000, 312000, 242667),
	MTK_VF_TABLE("seninf3_sel", 499200, 499200, 416000, 312000, 242667),
	MTK_VF_TABLE("aud_engen1_sel", 45158, 45158, 45158, 45158, 45158),
	MTK_VF_TABLE("aud_engen2_sel", 49152, 49152, 49152, 49152, 49152),
	MTK_VF_TABLE("aes_ufsfde_sel", 546000, 546000, 546000, 546000, 416000),
	MTK_VF_TABLE("ufs_sel", 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("ufs_mbist_sel", 297000, 297000, 297000, 297000, 297000),
	MTK_VF_TABLE("aud_1_sel", 180634, 180634, 180634, 180634, 180634),
	MTK_VF_TABLE("aud_2_sel", 196608, 196608, 196608, 196608, 196608),
	MTK_VF_TABLE("adsp_sel", 800000, 800000, 800000, 800000, 800000),
	MTK_VF_TABLE("dpmaif_main_sel", 499200, 436800, 364000, 273000, 273000),
	MTK_VF_TABLE("venc_sel", 624000, 624000, 458333, 312000, 249600),
	MTK_VF_TABLE("vdec_sel", 546000, 546000, 416000, 312000, 218400),
	MTK_VF_TABLE("pwm_sel", 78000, 78000, 78000, 78000, 78000),
	MTK_VF_TABLE("audio_h_sel", 196608, 196608, 196608, 196608, 196608),
	MTK_VF_TABLE("mcupm_sel", 218400, 218400, 218400, 218400, 218400),
	MTK_VF_TABLE("mem_sub_sel", 546000, 499200, 436800, 273000, 218400),
	MTK_VF_TABLE("peri_mem_sel", 546000, 499200, 416000, 273000, 218400),
	MTK_VF_TABLE("ufs_mem_sel", 546000, 499200, 416000, 273000, 218400),
	MTK_VF_TABLE("emi_n_sel", 333000, 333000, 333000, 333000, 333000),
	MTK_VF_TABLE("dsi_occ_sel", 312000, 312000, 312000, 249600, 208000),
	MTK_VF_TABLE("ccu_ahb_sel", 273000, 273000, 273000, 273000, 273000),
	MTK_VF_TABLE("ap2conn_host_sel", 78000, 78000, 78000, 78000, 78000),
	MTK_VF_TABLE("mcu_acp_sel", 624000, 624000, 312000, 312000, 192000),
	MTK_VF_TABLE("csi_occ_scan_sel", 312000, 312000, 312000, 312000, 208000),
	MTK_VF_TABLE("ipswest_sel", 832000, 832000, 624000, 312000, 312000),
	MTK_VF_TABLE("ipsnorth_sel", 832000, 832000, 624000, 312000, 312000),
	MTK_VF_TABLE("axi_l3gic_sel", 156000, 156000, 156000, 156000, 156000),
	{},
};
#endif

static const char *get_vf_name(int id)
{
	if (id < 0)
		return NULL;
#if CHECK_VCORE_FREQ
	return vf_table[id].name;
#else
	return NULL;
#endif
}

static int get_vf_opp(int id, int opp)
{
	if ((id < 0) || (opp < 0))
		return 0;
#if CHECK_VCORE_FREQ
	return vf_table[id].freq_table[opp];
#else
	return 0;
#endif
}

static u32 get_vf_num(void)
{
#if CHECK_VCORE_FREQ
	return ARRAY_SIZE(vf_table) - 1;
#else
	return 0;
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

void set_subsys_reg_dump_mt6886(enum chk_sys_id id[])
{
	const struct regname *rns = &rn[0];
	int i, j, k;

	for (i = 0; i < ARRAY_SIZE(rn) - 1; i++, rns++) {
		int pwr_idx = PD_NULL;

		if (!is_valid_reg(ADDR(rns)))
			continue;

		for (j = 0; id[j] != chk_sys_num; j++) {
			/* filter out the subsys that we don't want */
			if (rns->id == id[j])
				break;
			}

		if (id[j] == chk_sys_num)
			continue;

		for (k = 0; k < ARRAY_SIZE(pvd_pwr_data); k++) {
			if (pvd_pwr_data[k].id == id[j]) {
				pwr_idx = k;
				break;
			}
		}

		if (pwr_idx != PD_NULL)
			if (!pwr_hw_is_on(PWR_CON_STA, pwr_idx))
				continue;

		reg_dump_addr[i] = PHYSADDR(rns);
		reg_dump_val[i] = clk_readl(ADDR(rns));
		/* record each register dump index validation */
		reg_dump_valid[i] = true;
	}
}
EXPORT_SYMBOL_GPL(set_subsys_reg_dump_mt6886);

void get_subsys_reg_dump_mt6886(void)
{
	const struct regname *rns = &rn[0];
	int i;

	for (i = 0; i < ARRAY_SIZE(rn) - 1; i++, rns++) {
		if (reg_dump_valid[i])
			pr_info("%-18s: [0x%08x] = 0x%08x\n",
					rns->name, reg_dump_addr[i], reg_dump_val[i]);
	}
}
EXPORT_SYMBOL_GPL(get_subsys_reg_dump_mt6886);

void print_subsys_reg_mt6886(enum chk_sys_id id)
{
	struct regbase *rb_dump;
	const struct regname *rns = &rn[0];
	int pwr_idx = PD_NULL;
	int i;

	if (id >= chk_sys_num || id < 0) {
		pr_info("wrong id:%d\n", id);
		return;
	}

	for (i = 0; i < ARRAY_SIZE(pvd_pwr_data); i++) {
		if (pvd_pwr_data[i].id == id) {
			pwr_idx = i;
			break;
		}
	}

	rb_dump = &rb[id];

	for (i = 0; i < ARRAY_SIZE(rn) - 1; i++, rns++) {
		if (!is_valid_reg(ADDR(rns)))
			return;

		/* filter out the subsys that we don't want */
		if (rns->base != rb_dump)
			continue;

		if (pwr_idx != PD_NULL) {
			if (!pwr_hw_is_on(PWR_CON_STA, pwr_idx))
				return;
		}

		pr_info("%-18s: [0x%08x] = 0x%08x\n",
			rns->name, PHYSADDR(rns), clk_readl(ADDR(rns)));
	}
}
EXPORT_SYMBOL_GPL(print_subsys_reg_mt6886);

#if IS_ENABLED(CONFIG_MTK_DEVAPC)
static enum chk_sys_id devapc_dump_id[] = {
	ifrao,
	perao,
	spm,
	top,
	apmixed,
	ufscfg_ao_bus,
	gpu_eb_rpc,
	mfg_ao,
	mfgsc_ao,
	img_sub0_bus,
	img_sub1_bus,
	cam_sub0_bus,
	cam_sub2_bus,
	cam_sub1_bus,
	vlpcfg,
	vlp_ck,
	cci,
	cpu_ll,
	cpu_bl,
	ptp,
	hwv,
	hwv_ext,
	chk_sys_num,
};

static void devapc_dump(void)
{
	u32 freq[CHK_FM_NUM];
	int i;

	for (i = 0; i < CHK_FM_NUM; i++)
		freq[i] = mt_get_fmeter_freq(chk_fm_list[i].fm_id, chk_fm_list[i].fm_type);

	set_subsys_reg_dump_mt6886(devapc_dump_id);
	for (i = 0; i < CHK_FM_NUM; i++)
		pr_notice("[%s] %d khz\n", chk_fm_list[i].fm_name, freq[i]);

	get_subsys_reg_dump_mt6886();
}

static void serror_dump(void)
{
	u32 freq[CHK_FM_NUM];
	int i;

	for (i = 0; i < CHK_FM_NUM; i++)
		freq[i] = mt_get_fmeter_freq(chk_fm_list[i].fm_id, chk_fm_list[i].fm_type);

	set_subsys_reg_dump_mt6886(devapc_dump_id);
	for (i = 0; i < CHK_FM_NUM; i++)
		pr_notice("[%s] %d khz\n", chk_fm_list[i].fm_name, freq[i]);

	get_subsys_reg_dump_mt6886();
}

static struct devapc_vio_callbacks devapc_vio_handle = {
	.id = DEVAPC_SUBSYS_CLKMGR,
	.debug_dump = devapc_dump,
};

static struct devapc_vio_callbacks serror_handle = {
	.id = DEVAPC_SUBSYS_CLKM,
	.debug_dump = serror_dump,
};

#endif

static const char * const off_pll_names[] = {
	"univpll",
	"msdcpll",
	"mmpll",
	"ufspll",
	"imgpll",
	"mfg_ao_mfgpll",
	"mfgsc_ao_mfgscpll",
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
#if (BUG_ON_CHK_ENABLE) || (IS_ENABLED(CONFIG_MTK_CLKMGR_DEBUG))
	return true;
#endif
	return false;
}

static enum chk_sys_id history_dump_id[] = {
	top,
	apmixed,
	hwv,
	hwv_ext,
	chk_sys_num,
};

static void dump_hwv_history(struct regmap *regmap, u32 id)
{
	u32 set[XPU_NUM] = {0}, sta = 0, set_sta = 0, clr_sta = 0, en = 0, done = 0;
	int i;

	release_mt6886_hwv_secure();
	set_subsys_reg_dump_mt6886(history_dump_id);

	if (regmap != NULL) {
		for (i = 0; i < XPU_NUM; i++)
			regmap_read(regmap, HWV_CG_SET(xpu_id[i], id), &set[i]);

		regmap_read(regmap, HWV_CG_STA(id), &sta);
		regmap_read(regmap, HWV_CG_SET_STA(id), &set_sta);
		regmap_read(regmap, HWV_CG_CLR_STA(id), &clr_sta);
		regmap_read(regmap, HWV_CG_EN(id), &en);
		regmap_read(regmap, HWV_CG_DONE(id), &done);


		for (i = 0; i < XPU_NUM; i++)
			pr_notice("set: (%x)%x", HWV_CG_SET(xpu_id[i], id), set[i]);
		pr_notice("[%d] (%x)%x, (%x)%x, (%x)%x, (%x)%x, (%x)%x\n",
				id,
				HWV_CG_STA(id), sta,
				HWV_CG_SET_STA(id), set_sta,
				HWV_CG_CLR_STA(id), clr_sta,
				HWV_CG_EN(id), en,
				HWV_CG_DONE(id), done);
	}

	get_subsys_reg_dump_mt6886();
}

static enum chk_sys_id bus_dump_id[] = {
	chk_sys_num,
};

static void get_bus_reg(void)
{
	set_subsys_reg_dump_mt6886(bus_dump_id);
}

static void dump_bus_reg(struct regmap *regmap, u32 ofs)
{
	get_subsys_reg_dump_mt6886();
	set_subsys_reg_dump_mt6886(bus_dump_id);
	get_subsys_reg_dump_mt6886();
	/* sspm need some time to run isr */
	mdelay(1000);

	BUG_ON(1);
}

static enum chk_sys_id pll_dump_id[] = {
	apmixed,
	chk_sys_num,
};

static void dump_pll_reg(bool bug_on)
{
	release_mt6886_hwv_secure();
	set_subsys_reg_dump_mt6886(pll_dump_id);
	get_subsys_reg_dump_mt6886();

	if (bug_on) {
		mdelay(100);
		BUG_ON(1);
	}
}

static bool suspend_retry(bool reset_cnt)
{
	if (reset_cnt == true) {
		suspend_cnt = 0;
		return true;
	}

	suspend_cnt++;
	pr_notice("%s: suspend cnt: %d\n", __func__, suspend_cnt);

	if (suspend_cnt < 2)
		return false;

	return true;
}

/*
 * init functions
 */

static struct clkchk_ops clkchk_mt6886_ops = {
	.get_all_regnames = get_all_mt6886_regnames,
	.get_pvd_pwr_data_idx = get_pvd_pwr_data_idx,
	.get_pwr_status = get_pwr_status,
	.is_cg_chk_pwr_on = is_cg_chk_pwr_on,
	.get_off_pll_names = get_off_pll_names,
	.get_notice_pll_names = get_notice_pll_names,
	.is_pll_chk_bug_on = is_pll_chk_bug_on,
	.get_vf_name = get_vf_name,
	.get_vf_opp = get_vf_opp,
	.get_vf_num = get_vf_num,
	.get_vcore_opp = get_vcore_opp,
	.devapc_dump = devapc_dump,
	.dump_hwv_history = dump_hwv_history,
	.get_bus_reg = get_bus_reg,
	.dump_bus_reg = dump_bus_reg,
	.dump_pll_reg = dump_pll_reg,
	.suspend_retry = suspend_retry,
};

static int clk_chk_mt6886_probe(struct platform_device *pdev)
{
	suspend_cnt = 0;

	init_regbase();

	set_clkchk_notify();

	set_clkchk_ops(&clkchk_mt6886_ops);

#if IS_ENABLED(CONFIG_MTK_DEVAPC)
	register_devapc_vio_callback(&devapc_vio_handle);
	register_devapc_vio_callback(&serror_handle);
#endif

#if CHECK_VCORE_FREQ
	mtk_clk_check_muxes();
#endif

	return 0;
}

static const struct of_device_id of_match_clkchk_mt6886[] = {
	{
		.compatible = "mediatek,mt6886-clkchk",
	}, {
		/* sentinel */
	}
};

static struct platform_driver clk_chk_mt6886_drv = {
	.probe = clk_chk_mt6886_probe,
	.driver = {
		.name = "clk-chk-mt6886",
		.owner = THIS_MODULE,
		.pm = &clk_chk_dev_pm_ops,
		.of_match_table = of_match_clkchk_mt6886,
	},
};

/*
 * init functions
 */

static int __init clkchk_mt6886_init(void)
{
	return platform_driver_register(&clk_chk_mt6886_drv);
}

static void __exit clkchk_mt6886_exit(void)
{
	platform_driver_unregister(&clk_chk_mt6886_drv);
}

subsys_initcall(clkchk_mt6886_init);
module_exit(clkchk_mt6886_exit);
MODULE_LICENSE("GPL");
