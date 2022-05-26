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

#include <dt-bindings/power/mt6855-power.h>

#if IS_ENABLED(CONFIG_MTK_DEVAPC)
#include <devapc_public.h>
#endif

#if IS_ENABLED(CONFIG_MTK_DVFSRC_HELPER)
#include <mt-plat/dvfsrc-exp.h>
#endif

#include "clkchk.h"
#include "clkchk-mt6855.h"

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
	[infracfg] = REGBASE_V(0x10001000, infracfg, PD_NULL, CLK_NULL),
	[apmixed] = REGBASE_V(0x1000C000, apmixed, PD_NULL, CLK_NULL),
	[imp] = REGBASE_V(0x11000000, imp, PD_NULL, CLK_NULL),
	[perao] = REGBASE_V(0x11036000, perao, PD_NULL, CLK_NULL),
	[afe] = REGBASE_V(0x11050000, afe, MT6855_POWER_DOMAIN_AUDIO, CLK_NULL),
	[mfg_ao] = REGBASE_V(0x13fa0000, mfg_ao, PD_NULL, CLK_NULL),
	[mm] = REGBASE_V(0x14000000, mm, MT6855_POWER_DOMAIN_DISP, CLK_NULL),
	[imgsys1] = REGBASE_V(0x15020000, imgsys1, MT6855_POWER_DOMAIN_ISP_DIP1, CLK_NULL),
	[imgsys2] = REGBASE_V(0x15820000, imgsys2, MT6855_POWER_DOMAIN_ISP_MAIN, CLK_NULL),
	[vde2] = REGBASE_V(0x1602f000, vde2, MT6855_POWER_DOMAIN_VDE0, CLK_NULL),
	[ven1] = REGBASE_V(0x17000000, ven1, MT6855_POWER_DOMAIN_VEN0, CLK_NULL),
	[spm] = REGBASE_V(0x1C001000, spm, PD_NULL, CLK_NULL),
	[vlpcfg] = REGBASE_V(0x1C00C000, vlpcfg, PD_NULL, CLK_NULL),
	[vlp_ck] = REGBASE_V(0x1C013000, vlp_ck, PD_NULL, CLK_NULL),
	[cam_m] = REGBASE_V(0x1a000000, cam_m, MT6855_POWER_DOMAIN_CAM_MAIN, CLK_NULL),
	[cam_ra] = REGBASE_V(0x1a04f000, cam_ra, MT6855_POWER_DOMAIN_CAM_SUBA, CLK_NULL),
	[cam_rb] = REGBASE_V(0x1a06f000, cam_rb, MT6855_POWER_DOMAIN_CAM_SUBB, CLK_NULL),
	[ipe] = REGBASE_V(0x1b000000, ipe, MT6855_POWER_DOMAIN_ISP_IPE, CLK_NULL),
	[mminfra_config] = REGBASE_V(0x1e800000, mminfra_config,
		MT6855_POWER_DOMAIN_MM_INFRA, CLK_NULL),
	[mdp] = REGBASE_V(0x1f000000, mdp, MT6855_POWER_DOMAIN_DISP, CLK_NULL),
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
	REGNAME(top, 0x0320, CLK_AUDDIV_0),
	REGNAME(top, 0x0328, CLK_AUDDIV_2),
	REGNAME(top, 0x0334, CLK_AUDDIV_3),
	REGNAME(top, 0x0338, CLK_AUDDIV_4),
	/* INFRACFG_AO register */
	REGNAME(ifrao, 0x80, MODULE_CG_0),
	REGNAME(ifrao, 0x88, MODULE_CG_1),
	REGNAME(ifrao, 0xA4, MODULE_CG_2),
	REGNAME(ifrao, 0xC0, MODULE_CG_3),
	REGNAME(ifrao, 0xE0, MODULE_CG_4),
	/* INFRACFG_AO_BUS register */
	REGNAME(infracfg, 0x0C90, MCU_CONNSYS_PROTECT_EN_0),
	REGNAME(infracfg, 0x0C9C, MCU_CONNSYS_PROTECT_RDY_STA_0),
	REGNAME(infracfg, 0x0C50, INFRASYS_PROTECT_EN_1),
	REGNAME(infracfg, 0x0C5C, INFRASYS_PROTECT_RDY_STA_1),
	REGNAME(infracfg, 0x0C40, INFRASYS_PROTECT_EN_0),
	REGNAME(infracfg, 0x0C4C, INFRASYS_PROTECT_RDY_STA_0),
	REGNAME(infracfg, 0x0C80, PERISYS_PROTECT_EN_0),
	REGNAME(infracfg, 0x0C8C, PERISYS_PROTECT_RDY_STA_0),
	REGNAME(infracfg, 0x0C30, MMSYS_PROTECT_EN_2),
	REGNAME(infracfg, 0x0C3C, MMSYS_PROTECT_RDY_STA_2),
	REGNAME(infracfg, 0x0C10, MMSYS_PROTECT_EN_0),
	REGNAME(infracfg, 0x0C1C, MMSYS_PROTECT_RDY_STA_0),
	REGNAME(infracfg, 0x0C20, MMSYS_PROTECT_EN_1),
	REGNAME(infracfg, 0x0C2C, MMSYS_PROTECT_RDY_STA_1),
	REGNAME(infracfg, 0x0CC0, DRAMC_CCUSYS_PROTECT_EN_0),
	REGNAME(infracfg, 0x0CCC, DRAMC_CCUSYS_PROTECT_RDY_STA_0),
	REGNAME(infracfg, 0x0C60, EMISYS_PROTECT_EN_0),
	REGNAME(infracfg, 0x0C6C, EMISYS_PROTECT_RDY_STA_0),
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
	/* IMP_IIC_WRAP register */
	//REGNAME(imp, 0x281E00, AP_CLOCK_CG_CEN),
	//REGNAME(imp, 0xED5E00, AP_CLOCK_CG_EST_NOR),
	//REGNAME(imp, 0xDB5E00, AP_CLOCK_CG_SOU),
	//REGNAME(imp, 0xB21E00, AP_CLOCK_CG_WST),
	/* PERICFG_AO register */
	REGNAME(perao, 0x10, PERI_CG_0),
	REGNAME(perao, 0x14, PERI_CG_1),
	REGNAME(perao, 0x1C, PERI_CG_3),
	/* AFE register */
	REGNAME(afe, 0x0, AUDIO_TOP_0),
	REGNAME(afe, 0x4, AUDIO_TOP_1),
	REGNAME(afe, 0x8, AUDIO_TOP_2),
	/* MFG_PLL_CTRL register */
	REGNAME(mfg_ao, 0x8, MFGPLL_CON0),
	REGNAME(mfg_ao, 0xc, MFGPLL_CON1),
	REGNAME(mfg_ao, 0x10, MFGPLL_CON2),
	REGNAME(mfg_ao, 0x14, MFGPLL_CON3),
	REGNAME(mfg_ao, 0x38, MFGSCPLL_CON0),
	REGNAME(mfg_ao, 0x3c, MFGSCPLL_CON1),
	REGNAME(mfg_ao, 0x40, MFGSCPLL_CON2),
	REGNAME(mfg_ao, 0x44, MFGSCPLL_CON3),
	/* DISPSYS_CONFIG register */
	REGNAME(mm, 0x100, MMSYS_CG_0),
	REGNAME(mm, 0x110, MMSYS_CG_1),
	REGNAME(mm, 0x1A0, MMSYS_CG_2),
	/* IMGSYS1 register */
	REGNAME(imgsys1, 0x0, IMG_1_CG),
	/* IMGSYS2 register */
	REGNAME(imgsys2, 0x0, IMG_2_CG),
	/* VDEC_GCON_BASE register */
	REGNAME(vde2, 0x8, LARB_CKEN_CON),
	REGNAME(vde2, 0x190, MINI_MDP_CFG_0),
	REGNAME(vde2, 0x0, VDEC_CKEN),
	/* VENC_GCON register */
	REGNAME(ven1, 0x0, VENCSYS_CG),
	/* SPM register */
	REGNAME(spm, 0xE00, MD1_PWR_CON),
	REGNAME(spm, 0xF34, PWR_STATUS),
	REGNAME(spm, 0xF38, PWR_STATUS_2ND),
	REGNAME(spm, 0xF2C, MD_BUCK_ISO_CON),
	REGNAME(spm, 0xE04, CONN_PWR_CON),
	REGNAME(spm, 0xE10, UFS0_PWR_CON),
	REGNAME(spm, 0xE14, AUDIO_PWR_CON),
	REGNAME(spm, 0xE24, ISP_MAIN_PWR_CON),
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
	REGNAME(spm, 0xF48, CSI_RX_PWR_CON),
	REGNAME(spm, 0xEB8, MFG0_PWR_CON),
	REGNAME(spm, 0xF3C, XPU_PWR_STATUS),
	REGNAME(spm, 0xF40, XPU_PWR_STATUS_2ND),
	REGNAME(spm, 0xEBC, MFG1_PWR_CON),
	REGNAME(spm, 0xEC0, MFG2_PWR_CON),
	/* VLPCFG_BUS register */
	REGNAME(vlpcfg, 0x0210, VLP_TOPAXI_PROTECTEN),
	REGNAME(vlpcfg, 0x0220, VLP_TOPAXI_PROTECTEN_STA1),
	/* VLP_CKSYS register */
	REGNAME(vlp_ck, 0x0008, VLP_CLK_CFG_0),
	REGNAME(vlp_ck, 0x0014, VLP_CLK_CFG_1),
	REGNAME(vlp_ck, 0x0020, VLP_CLK_CFG_2),
	REGNAME(vlp_ck, 0x002C, VLP_CLK_CFG_3),
	/* CAMSYS_MAIN register */
	REGNAME(cam_m, 0x0, CAMSYS_MAIN_CG),
	/* CAMSYS_RAWA register */
	REGNAME(cam_ra, 0x0, CAMSYS_RA_CG),
	/* CAMSYS_RAWB register */
	REGNAME(cam_rb, 0x0, CAMSYS_RB_CG),
	/* IPESYS register */
	REGNAME(ipe, 0x0, IMG_IPE_CG),
	/* MMINFRA_CONFIG register */
	REGNAME(mminfra_config, 0x100, MMINFRA_CG_0),
	REGNAME(mminfra_config, 0x110, MMINFRA_CG_1),
	/* MDPSYS_CONFIG register */
	REGNAME(mdp, 0x100, MDPSYS_CG_0),
	REGNAME(mdp, 0x110, MDPSYS_CG_1),
	{},
};

static const struct regname *get_all_mt6855_regnames(void)
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
};

static u32 pwr_sta[STA_NUM];

u32 *get_spm_pwr_status_array(void)
{
	static void __iomem *scpsys_base, *pwr_addr[STA_NUM];
	int i;

	for (i = 0; i < STA_NUM; i++) {
		if (!scpsys_base)
			scpsys_base = ioremap(0x1C001000, PAGE_SIZE);

		if (pwr_ofs[i]) {
			pwr_addr[i] = scpsys_base + pwr_ofs[i];
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
	{"afe", PWR_STA, 0x00000020},
	{"camsys_main", PWR_STA, 0x00020000},
	{"camsys_rawa", PWR_STA, 0x00080000},
	{"camsys_rawb", PWR_STA, 0x00100000},
	{"dispsys", PWR_STA, 0x02000000},
	{"imgsys1", PWR_STA, 0x00000400},
	{"imgsys2", PWR_STA, 0x00000200},
	//{"imp_iic_wrap", PWR_STA, 0x00000000},
	{"infracfg_ao", PWR_STA, 0x00000000},
	{"ipesys", PWR_STA, 0x00000800},
	{"mdpsys1_config", PWR_STA, 0x00000000},
	{"mdpsys", PWR_STA, 0x02000000},
	{"mminfra_config", PWR_STA, 0x08000000},
	{"pericfg_ao", PWR_STA, 0x00000000},
	{"vdec_gcon_base", PWR_STA, 0x00002000},
	{"vencsys", PWR_STA, 0x00008000},
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

void print_subsys_reg_mt6855(enum chk_sys_id id)
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
		if (!is_valid_reg(ADDR(rns)))
			return;

		/* filter out the subsys that we don't want */
		if (rns->base != rb_dump)
			continue;

		pr_info("%-18s: [0x%08x] = 0x%08x\n",
			rns->name, PHYSADDR(rns), clk_readl(ADDR(rns)));
	}
}
EXPORT_SYMBOL(print_subsys_reg_mt6855);

#if IS_ENABLED(CONFIG_MTK_DEVAPC)
static void devapc_dump(void)
{
	print_subsys_reg_mt6855(spm);
	print_subsys_reg_mt6855(top);
	print_subsys_reg_mt6855(infracfg);
	print_subsys_reg_mt6855(apmixed);
	print_subsys_reg_mt6855(mfg_ao);
	print_subsys_reg_mt6855(vlpcfg);
	print_subsys_reg_mt6855(vlp_ck);
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
	NULL
};

static const char * const notice_pll_names[] = {
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

static struct clkchk_ops clkchk_mt6855_ops = {
	.get_all_regnames = get_all_mt6855_regnames,
	.get_spm_pwr_status_array = get_spm_pwr_status_array,
	.get_pvd_pwr_mask = get_pvd_pwr_mask,
	.get_off_pll_names = get_off_pll_names,
	.get_notice_pll_names = get_notice_pll_names,
	.is_pll_chk_bug_on = is_pll_chk_bug_on,
	.get_vf_table = get_vf_table,
	.get_vcore_opp = get_vcore_opp,
	.devapc_dump = devapc_dump,
};

static int clk_chk_mt6855_probe(struct platform_device *pdev)
{
	init_regbase();

	set_clkchk_ops(&clkchk_mt6855_ops);

#if IS_ENABLED(CONFIG_MTK_DEVAPC)
	register_devapc_vio_callback(&devapc_vio_handle);
#endif

	return 0;
}

static const struct of_device_id of_match_clkchk_mt6855[] = {
	{
		.compatible = "mediatek,mt6855-clkchk",
	}, {
		/* sentinel */
	}
};

static struct platform_driver clk_chk_mt6855_drv = {
	.probe = clk_chk_mt6855_probe,
	.driver = {
		.name = "clk-chk-mt6855",
		.owner = THIS_MODULE,
		.pm = &clk_chk_dev_pm_ops,
		.of_match_table = of_match_clkchk_mt6855,
	},
};

/*
 * init functions
 */

static int __init clkchk_mt6855_init(void)
{
	return platform_driver_register(&clk_chk_mt6855_drv);
}

static void __exit clkchk_mt6855_exit(void)
{
	platform_driver_unregister(&clk_chk_mt6855_drv);
}

subsys_initcall(clkchk_mt6855_init);
module_exit(clkchk_mt6855_exit);
MODULE_LICENSE("GPL");
