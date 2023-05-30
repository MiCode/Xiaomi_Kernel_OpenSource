// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Chuan-Wen Chen <chuan-wen.chen@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>

#include <dt-bindings/power/mt6835-power.h>

#if IS_ENABLED(CONFIG_MTK_DEVAPC)
#include <devapc_public.h>
#endif

#if IS_ENABLED(CONFIG_MTK_DVFSRC_HELPER)
#include <mt-plat/dvfsrc-exp.h>
#endif

#include "clkchk.h"
#include "clkchk-mt6835.h"

#define BUG_ON_CHK_ENABLE		0
#define CHECK_VCORE_FREQ		0
#define CG_CHK_PWRON_ENABLE		0

/*
 * clkchk dump_regs
 */

#define REGBASE_V(_phys, _id_name, _pg, _pn) { .phys = _phys, .id = _id_name,	\
		.name = #_id_name, .pg = _pg, .pn = _pn}

static unsigned int suspend_cnt;

static struct regbase rb[] = {
	[top] = REGBASE_V(0x10000000, top, PD_NULL, CLK_NULL),
	[ifrao] = REGBASE_V(0x10001000, ifrao, PD_NULL, CLK_NULL),
	[infracfg] = REGBASE_V(0x10001000, infracfg, PD_NULL, CLK_NULL),
	[apmixed] = REGBASE_V(0x1000C000, apmixed, PD_NULL, CLK_NULL),
	[nemi_reg] = REGBASE_V(0x10219000, nemi_reg, PD_NULL, CLK_NULL),
	[dpmaif] = REGBASE_V(0x1022d400, dpmaif, PD_NULL, CLK_NULL),
	[emi_bus] = REGBASE_V(0x10270000, emi_bus, PD_NULL, CLK_NULL),
	[perao] = REGBASE_V(0x11036000, perao, PD_NULL, CLK_NULL),
	[afe] = REGBASE_V(0x11050000, afe, MT6835_CHK_PD_AUDIO, CLK_NULL),
	[impc] = REGBASE_V(0x11282000, impc, PD_NULL, CLK_NULL),
	[ufsao] = REGBASE_V(0x112b8000, ufsao, PD_NULL, CLK_NULL),
	[ufspdn] = REGBASE_V(0x112bb000, ufspdn, PD_NULL, CLK_NULL),
	[impws] = REGBASE_V(0x11B22000, impws, PD_NULL, CLK_NULL),
	[imps] = REGBASE_V(0x11DB4000, imps, PD_NULL, CLK_NULL),
	[impen] = REGBASE_V(0x11ED4000, impen, PD_NULL, CLK_NULL),
	[mfgcfg] = REGBASE_V(0x13fbf000, mfgcfg, PD_NULL, CLK_NULL),
	[mm] = REGBASE_V(0x14000000, mm, MT6835_CHK_PD_DIS0, CLK_NULL),
	[imgsys1] = REGBASE_V(0x15020000, imgsys1, MT6835_CHK_PD_ISP_DIP1, CLK_NULL),
	[vde2] = REGBASE_V(0x1602f000, vde2, MT6835_CHK_PD_VDE0, CLK_NULL),
	[ven1] = REGBASE_V(0x17000000, ven1, MT6835_CHK_PD_VEN0, CLK_NULL),
	[spm] = REGBASE_V(0x1C001000, spm, PD_NULL, CLK_NULL),
	[vlpcfg] = REGBASE_V(0x1C00C000, vlpcfg, PD_NULL, CLK_NULL),
	[vlp_ck] = REGBASE_V(0x1C013000, vlp_ck, PD_NULL, CLK_NULL),
	[scp_iic] = REGBASE_V(0x1C7B7000, scp_iic, PD_NULL, CLK_NULL),
	[cam_m] = REGBASE_V(0x1a000000, cam_m, MT6835_CHK_PD_CAM_MAIN, CLK_NULL),
	[cam_sub1_bus] = REGBASE_V(0x1a00c000, cam_sub1_bus, PD_NULL, CLK_NULL),
	[cam_sub0_bus] = REGBASE_V(0x1a00d000, cam_sub0_bus, PD_NULL, CLK_NULL),
	[cam_ra] = REGBASE_V(0x1a04f000, cam_ra, MT6835_CHK_PD_CAM_SUBA, CLK_NULL),
	[cam_rb] = REGBASE_V(0x1a06f000, cam_rb, MT6835_CHK_PD_CAM_SUBB, CLK_NULL),
	[ipe] = REGBASE_V(0x1b000000, ipe, MT6835_CHK_PD_ISP_IPE, CLK_NULL),
	[dvfsrc_apb] = REGBASE_V(0x1c00f000, dvfsrc_apb, PD_NULL, CLK_NULL),
	[sramrc_apb] = REGBASE_V(0x1c01f000, sramrc_apb, PD_NULL, CLK_NULL),
	[mminfra_config] = REGBASE_V(0x1e800000, mminfra_config, MT6835_CHK_PD_MM_INFRA, CLK_NULL),
	[mdp] = REGBASE_V(0x1f000000, mdp, MT6835_CHK_PD_DIS0, CLK_NULL),
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
	REGNAME(top, 0x0120, CLK_CFG_20),
	REGNAME(top, 0x0320, CLK_AUDDIV_0),
	REGNAME(top, 0x0328, CLK_AUDDIV_2),
	REGNAME(top, 0x0334, CLK_AUDDIV_3),
	REGNAME(top, 0x0338, CLK_AUDDIV_4),
	/* INFRACFG_AO register */
	REGNAME(ifrao, 0x90, MODULE_CG_0),
	REGNAME(ifrao, 0x94, MODULE_CG_1),
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
	REGNAME(infracfg, 0x0C80, PERISYS_PROTECT_EN_STA_0),
	REGNAME(infracfg, 0x0C8C, PERISYS_PROTECT_RDY_STA_0),
	REGNAME(infracfg, 0x0C10, MMSYS_PROTECT_EN_STA_0),
	REGNAME(infracfg, 0x0C1C, MMSYS_PROTECT_RDY_STA_0),
	REGNAME(infracfg, 0x0C20, MMSYS_PROTECT_EN_STA_1),
	REGNAME(infracfg, 0x0C2C, MMSYS_PROTECT_RDY_STA_1),
	REGNAME(infracfg, 0x0C30, MMSYS_PROTECT_EN_STA_2),
	REGNAME(infracfg, 0x0C3C, MMSYS_PROTECT_RDY_STA_2),
	REGNAME(infracfg, 0x0CA0, MD_MFGSYS_PROTECT_EN_STA_0),
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
	REGNAME(apmixed, 0x3c0, MFGPLL_CON0),
	REGNAME(apmixed, 0x3c4, MFGPLL_CON1),
	REGNAME(apmixed, 0x3c8, MFGPLL_CON2),
	REGNAME(apmixed, 0x3cc, MFGPLL_CON3),
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
	/* NEMI_REG register */
	REGNAME(nemi_reg, 0x858, EMI_THRO_CTRL1),
	/* NRL2_DPMAIF_AP_MISC_CFG_BOL register */
	REGNAME(dpmaif, 0x68, CG_EN),
	REGNAME(dpmaif, 0x78, CG_GATED),
	/* EMI_BUS register */
	REGNAME(emi_bus, 0x80, GLITCH_PROT_EN),
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
	/* UFS_AO_CONFIG register */
	REGNAME(ufsao, 0x4, UFS_AO_CG_0),
	/* UFS_PDN_CFG register */
	REGNAME(ufspdn, 0x4, UFS_PDN_CG_0),
	/* IMP_IIC_WRAP_WS register */
	REGNAME(impws, 0xE00, AP_CLOCK_CG_WST_SOU),
	/* IMP_IIC_WRAP_S register */
	REGNAME(imps, 0xE00, AP_CLOCK_CG_SOU),
	/* IMP_IIC_WRAP_EN register */
	REGNAME(impen, 0xE00, AP_CLOCK_CG_EST_NOR),
	/* MFG_TOP_CONFIG register */
	REGNAME(mfgcfg, 0x0, MFG_CG),
	/* DISPSYS_CONFIG register */
	REGNAME(mm, 0x100, MMSYS_CG_0),
	REGNAME(mm, 0x110, MMSYS_CG_1),
	REGNAME(mm, 0x1A0, MMSYS_CG_2),
	/* IMGSYS1 register */
	REGNAME(imgsys1, 0x0, IMG_CG),
	/* VDEC_GCON_BASE register */
	REGNAME(vde2, 0x8, LARB_CKEN_CON),
	REGNAME(vde2, 0x0, VDEC_CKEN),
	/* VENC_GCON register */
	REGNAME(ven1, 0x0, VENCSYS_CG),
	/* SPM register */
	REGNAME(spm, 0xE00, MD1_PWR_CON),
	REGNAME(spm, 0xF3C, PWR_STATUS),
	REGNAME(spm, 0xF40, PWR_STATUS_2ND),
	REGNAME(spm, 0xF2C, MD_BUCK_ISO_CON),
	REGNAME(spm, 0xE04, CONN_PWR_CON),
	REGNAME(spm, 0xE10, UFS0_PWR_CON),
	REGNAME(spm, 0xE14, AUDIO_PWR_CON),
	REGNAME(spm, 0xE28, ISP_DIP1_PWR_CON),
	REGNAME(spm, 0xE2C, ISP_IPE_PWR_CON),
	REGNAME(spm, 0xE34, VDE0_PWR_CON),
	REGNAME(spm, 0xE3C, VEN0_PWR_CON),
	REGNAME(spm, 0xE44, CAM_MAIN_PWR_CON),
	REGNAME(spm, 0xE4C, CAM_SUBA_PWR_CON),
	REGNAME(spm, 0xE50, CAM_SUBB_PWR_CON),
	REGNAME(spm, 0xE64, DIS0_PWR_CON),
	REGNAME(spm, 0xE6C, MM_INFRA_PWR_CON),
	REGNAME(spm, 0xE70, MM_PROC_PWR_CON),
	REGNAME(spm, 0xEB8, MFG0_PWR_CON),
	REGNAME(spm, 0xF4C, XPU_PWR_STATUS),
	REGNAME(spm, 0xF50, XPU_PWR_STATUS_2ND),
	REGNAME(spm, 0xEBC, MFG1_PWR_CON),
	REGNAME(spm, 0xEC0, MFG2_PWR_CON),
	REGNAME(spm, 0xEC4, MFG3_PWR_CON),
	/* VLPCFG_BUS register */
	REGNAME(vlpcfg, 0x0210, VLP_TOPAXI_PROTECTEN),
	REGNAME(vlpcfg, 0x0220, VLP_TOPAXI_PROTECTEN_STA1),
	/* VLP_CKSYS register */
	REGNAME(vlp_ck, 0x0008, VLP_CLK_CFG_0),
	REGNAME(vlp_ck, 0x0014, VLP_CLK_CFG_1),
	REGNAME(vlp_ck, 0x0020, VLP_CLK_CFG_2),
	REGNAME(vlp_ck, 0x002C, VLP_CLK_CFG_3),
	/* SCP_IIC register */
	REGNAME(scp_iic, 0xE10, CCU_CLOCK_CG_CEN),
	/* CAMSYS_MAIN register */
	REGNAME(cam_m, 0x0, CAMSYS_CG),
	/* CAM_SUB1_BUS register */
	REGNAME(cam_sub1_bus, 0x3c0, SMI_COMMON_PROTECT_EN),
	REGNAME(cam_sub1_bus, 0x3c0, SMI_COMMON_PROTECT_EN),
	/* CAM_SUB0_BUS register */
	REGNAME(cam_sub0_bus, 0x3c0, SMI_COMMON_PROTECT_EN),
	REGNAME(cam_sub0_bus, 0x3c0, SMI_COMMON_PROTECT_EN),
	/* CAMSYS_RAWA register */
	REGNAME(cam_ra, 0x0, CAMSYS_CG),
	/* CAMSYS_RAWB register */
	REGNAME(cam_rb, 0x0, CAMSYS_CG),
	/* IPESYS register */
	REGNAME(ipe, 0x0, IMG_CG),
	/* DVFSRC_APB register */
	REGNAME(dvfsrc_apb, 0x0, DVFSRC_BASIC_CONTROL),
	/* SRAMRC_APB register */
	REGNAME(sramrc_apb, 0x0, BASIC),
	/* MMINFRA_CONFIG register */
	REGNAME(mminfra_config, 0x100, MMINFRA_CG_0),
	REGNAME(mminfra_config, 0x110, MMINFRA_CG_1),
	/* MDPSYS_CONFIG register */
	REGNAME(mdp, 0x100, MDPSYS_CG_0),
	REGNAME(mdp, 0x110, MDPSYS_CG_1),
	{},
};

static const struct regname *get_all_mt6835_regnames(void)
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

u32 get_mt6835_reg_value(u32 id, u32 ofs)
{
	if (id >= chk_sys_num)
		return 0;

	return clk_readl(rb[id].virt + ofs);
}
EXPORT_SYMBOL_GPL(get_mt6835_reg_value);

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
	{"afe", afe, spm, 0x0E14},
	{"camsys_main", cam_m, spm, 0x0E44},
	{"camsys_rawa", cam_ra, spm, 0x0E4C},
	{"camsys_rawb", cam_rb, spm, 0x0E50},
	{"dispsys_config", mm, spm, 0x0E64},
	{"imgsys1", imgsys1, spm, 0x0E28},
	{"ipesys", ipe, spm, 0x0E2C},
	{"mdpsys", mdp, spm, 0x0E64},
	{"mminfra_config", mminfra_config, spm, 0x0E6C},
	{"vdec_gcon_base", vde2, spm, 0x0E34},
	{"vencsys", ven1, spm, 0x0E3C},
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
	int freq_table[6];
};

#define MTK_VF_TABLE(_n, _freq0, _freq1, _freq2, _freq3, _freq4, _freq5) {		\
		.name = _n,		\
		.freq_table = {_freq0, _freq1, _freq2, _freq3, _freq4, _freq5},	\
	}

/*
 * Opp0 : 0p80v
 * Opp1 : 0p75v
 * Opp2 : 0p725v
 * Opp3 : 0p65v
 * Opp4 : 0p60v
 * Opp5 : 0p55v
 */
static struct mtk_vf vf_table[] = {
	/* Opp0, Opp1, Opp2, Opp3, Opp4, Opp5 */
	MTK_VF_TABLE("axi_sel", 156000, 156000, 156000, 156000, 156000),
	MTK_VF_TABLE("axip_sel", 156000, 156000, 156000, 156000, 156000),
	MTK_VF_TABLE("axi_u_sel", 78000, 78000, 78000, 78000, 78000),
	MTK_VF_TABLE("bus_aximem_sel", 364000, 364000, 273000, 273000, 218400),
	MTK_VF_TABLE("disp0_sel", 624000, 546000, 416000, 312000, 218400),
	MTK_VF_TABLE("mdp0_sel", 624000, 594000, 436800, 343750, 229167),
	MTK_VF_TABLE("mminfra_sel", 687500, 546000, 416000, 312000, 218400),
	MTK_VF_TABLE("mmup_sel", 218400, 218400, 218400, 218400),
	MTK_VF_TABLE("camtg_sel", 52000, 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("camtg2_sel", 52000, 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("camtg3_sel", 52000, 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("camtg4_sel", 52000, 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("uart_sel", 52000, 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("spi_sel", 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("msdc_0p_macro_sel", 384000, 384000, 384000, 384000, 384000),
	MTK_VF_TABLE("msdc5hclk_sel", 273000, 273000, 273000, 273000, 273000),
	MTK_VF_TABLE("msdc50_0_sel", 384000, 384000, 384000, 384000, 384000),
	MTK_VF_TABLE("aes_msdcfde_sel", 416000, 416000, 416000, 416000, 416000),
	MTK_VF_TABLE("msdc_macro_sel", 384000, 384000, 384000, 384000, 384000),
	MTK_VF_TABLE("msdc30_1_sel", 192000, 192000, 192000, 192000, 192000),
	MTK_VF_TABLE("audio_sel", 54600, 54600, 54600, 54600, 54600),
	MTK_VF_TABLE("aud_intbus_sel", 136500, 136500, 136500, 136500, 136500),
	MTK_VF_TABLE("disp_pwm_sel", 130000, 130000, 130000, 130000, 130000),
	MTK_VF_TABLE("usb_sel", 124800, 124800, 124800, 124800, 124800),
	MTK_VF_TABLE("ssusb_xhci_sel", 124800, 124800, 124800, 124800, 124800),
	MTK_VF_TABLE("i2c_sel", 136500, 136500, 136500, 136500, 136500),
	MTK_VF_TABLE("seninf_sel", 499200, 499200, 499200, 392857, 286000),
	MTK_VF_TABLE("seninf1_sel", 499200, 499200, 499200, 392857, 286000),
	MTK_VF_TABLE("seninf2_sel", 499200, 499200, 499200, 392857, 286000),
	MTK_VF_TABLE("dxcc_sel", 273000, 273000, 273000, 273000, 273000),
	MTK_VF_TABLE("aud_engen1_sel", 45158, 45158, 45158, 45158, 45158),
	MTK_VF_TABLE("aud_engen2_sel", 49152, 49152, 49152, 49152, 49152),
	MTK_VF_TABLE("aes_ufsfde_sel", 416000, 416000, 416000, 416000, 416000),
	MTK_VF_TABLE("ufs_sel", 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("aud_1_sel", 180634, 180634, 180634, 180634, 180634),
	MTK_VF_TABLE("aud_2_sel", 196608, 196608, 196608, 196608, 196608),
	MTK_VF_TABLE("dpmaif_main_sel", 499200, 436800, 364000, 364000, 273000),
	MTK_VF_TABLE("venc_sel", 624000, 624000, 458333, 364000, 249600),
	MTK_VF_TABLE("vdec_sel", 546000, 546000, 416000, 312000, 218400),
	MTK_VF_TABLE("pwm_sel", 78000, 78000, 78000, 78000, 78000),
	MTK_VF_TABLE("audio_h_sel", 196608, 196608, 196608, 196608, 196608),
	MTK_VF_TABLE("mcupm_sel", 218400, 182000, 182000, 182000, 182000),
	MTK_VF_TABLE("mem_sub_sel", 546000, 436800, 392857, 273000, 218400),
	MTK_VF_TABLE("mem_subp_sel", 436800, 364000, 273000, 218400),
	MTK_VF_TABLE("mem_sub_u_sel", 436800, 364000, 273000, 218400),
	MTK_VF_TABLE("ap2conn_host_sel", 78000, 78000, 78000, 78000, 78000),
	MTK_VF_TABLE("mcu_acp_sel", 594000, 594000, 546000, 392857, 249600),
	MTK_VF_TABLE("img1_sel", 624000, 624000, 458333, 343750, 229167),
	MTK_VF_TABLE("ipe_sel", 546000, 546000, 416000, 312000, 229167),
	MTK_VF_TABLE("cam_sel", 624000, 624000, 546000, 392857, 286000),
	MTK_VF_TABLE("camtm_sel", 208000, 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("msdc_1p_rx_sel", 192000, 192000, 192000, 192000, 192000),
	MTK_VF_TABLE("nfi1x_sel", 182000, 182000, 182000, 182000),
	MTK_VF_TABLE("dbi_sel", 178286, 178286, 178286, 178286, 124800),
	MTK_VF_TABLE("mfg_ref_sel", 364000, 364000, 364000, 364000),
	MTK_VF_TABLE("emi_546_sel", 546000, 546000, 546000, 546000),
	MTK_VF_TABLE("emi_624_sel", 624000, 624000, 624000, 624000),
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

void set_subsys_reg_dump_mt6835(enum chk_sys_id id[])
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
EXPORT_SYMBOL_GPL(set_subsys_reg_dump_mt6835);

void get_subsys_reg_dump_mt6835(void)
{
	const struct regname *rns = &rn[0];
	int i;

	for (i = 0; i < ARRAY_SIZE(rn) - 1; i++, rns++) {
		if (reg_dump_valid[i])
			pr_info("%-18s: [0x%08x] = 0x%08x\n",
					rns->name, reg_dump_addr[i], reg_dump_val[i]);
	}
}
EXPORT_SYMBOL_GPL(get_subsys_reg_dump_mt6835);

void print_subsys_reg_mt6835(enum chk_sys_id id)
{
	struct regbase *rb_dump;
	const struct regname *rns = &rn[0];
	int i;

	if (id >= chk_sys_num) {
		pr_info("wrong id:%d\n", id);
		return;
	}

	rb_dump = &rb[id];

	for (i = 0; i < ARRAY_SIZE(rn) - 1; i++, rns++) {
		u32 pg;

		if (!is_valid_reg(ADDR(rns)))
			return;

		/* filter out the subsys that we don't want */
		if (rns->base != rb_dump)
			continue;

		pg = rb_dump->pg;
		if (pg != PD_NULL && pvd_pwr_data[pg].ofs != 0)
			if (!pwr_hw_is_on(PWR_CON_STA, pg))
				return;

		pr_info("%-18s: [0x%08x] = 0x%08x\n",
			rns->name, PHYSADDR(rns), clk_readl(ADDR(rns)));
	}
}
EXPORT_SYMBOL_GPL(print_subsys_reg_mt6835);

#if IS_ENABLED(CONFIG_MTK_DEVAPC)
static enum chk_sys_id devapc_dump_id[] = {
	spm,
	top,
	infracfg,
	apmixed,
	vlpcfg,
	vlp_ck,
	chk_sys_num,
};

static void devapc_dump(void)
{
	set_subsys_reg_dump_mt6835(devapc_dump_id);
	get_subsys_reg_dump_mt6835();
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
	"mfgpll",
	"tvdpll",
	"apll1",
	"apll2",
	"imgpll",
	NULL
};

static const char * const notice_pll_names[] = {

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

static bool is_pll_chk_bug_on(void)
{
#if BUG_ON_CHK_ENABLE
	return true;
#endif
	return false;
}



static enum chk_sys_id bus_dump_id[] = {
	chk_sys_num,
};

static void get_bus_reg(void)
{
	set_subsys_reg_dump_mt6835(bus_dump_id);
}

static void dump_bus_reg(struct regmap *regmap, u32 ofs)
{
	get_subsys_reg_dump_mt6835();
	set_subsys_reg_dump_mt6835(bus_dump_id);
	get_subsys_reg_dump_mt6835();
	/* sspm need some time to run isr */
	mdelay(1000);

	BUG_ON(1);
}



/*
 * init functions
 */

static struct clkchk_ops clkchk_mt6835_ops = {
	.get_all_regnames = get_all_mt6835_regnames,
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
	.get_bus_reg = get_bus_reg,
	.dump_bus_reg = dump_bus_reg,
	.suspend_retry = suspend_retry,
};

static int clk_chk_mt6835_probe(struct platform_device *pdev)
{
	suspend_cnt = 0;

	init_regbase();

	set_clkchk_notify();

	set_clkchk_ops(&clkchk_mt6835_ops);

#if IS_ENABLED(CONFIG_MTK_DEVAPC)
	register_devapc_vio_callback(&devapc_vio_handle);
#endif

#if CHECK_VCORE_FREQ
	mtk_clk_check_muxes();
#endif

	return 0;
}

static struct platform_driver clk_chk_mt6835_drv = {
	.probe = clk_chk_mt6835_probe,
	.driver = {
		.name = "clk-chk-mt6835",
		.owner = THIS_MODULE,
		.pm = &clk_chk_dev_pm_ops,
	},
};

/*
 * init functions
 */

static int __init clkchk_mt6835_init(void)
{
	static struct platform_device *clk_chk_dev;

	clk_chk_dev = platform_device_register_simple("clk-chk-mt6835", -1, NULL, 0);
	if (IS_ERR(clk_chk_dev))
		pr_warn("unable to register clk-chk device");

	return platform_driver_register(&clk_chk_mt6835_drv);
}

static void __exit clkchk_mt6835_exit(void)
{
	platform_driver_unregister(&clk_chk_mt6835_drv);
}

subsys_initcall(clkchk_mt6835_init);
module_exit(clkchk_mt6835_exit);
MODULE_LICENSE("GPL");
