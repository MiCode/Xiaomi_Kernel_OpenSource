// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/clk-provider.h>
#include <linux/syscore_ops.h>
#include <linux/version.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>

#include "clkchk-mt6761.h"
#include <dt-bindings/power/mt6761-power.h>
#include "clkchk.h"
#if IS_ENABLED(CONFIG_MTK_DEVAPC)
#include <devapc_public.h>
#endif

#define ARMPLL_L_EXIST		0
#define CCIPLL_EXIST		0

#define WARN_ON_CHECK_FAIL		0
#define CLKDBG_CCF_API_4_4		1

#define BUG_ON_CHK_ENABLE		0
#define CHECK_VCORE_FREQ		0
#define TAG	"[clkchk] "

#define clk_warn(fmt, args...)	pr_notice(TAG fmt, ##args)

#if !CLKDBG_CCF_API_4_4

/* backward compatible */

static const char *clk_hw_get_name(const struct clk_hw *hw)
{
	return __clk_get_name(hw->clk);
}

static bool clk_hw_is_prepared(const struct clk_hw *hw)
{
	return __clk_is_prepared(hw->clk);
}

static bool clk_hw_is_enabled(const struct clk_hw *hw)
{
	return __clk_is_enabled(hw->clk);
}

#endif /* !CLKDBG_CCF_API_4_4 */

/*
 * clkchk dump_regs
 */

#define REGBASE_V(_phys, _id_name, _pg, _pn) { .phys = _phys,	\
		.name = #_id_name, .pg = _pg, .pn = _pn}

static struct regbase rb[] = {
	[topckgen] = REGBASE_V(0x10000000, topckgen, PD_NULL, CLK_NULL),
	[infracfg] = REGBASE_V(0x10001000, infracfg, PD_NULL, CLK_NULL),
	[pericfg] = REGBASE_V(0x10003000, pericfg, PD_NULL, CLK_NULL),
	[scpsys]   = REGBASE_V(0x10006000, scpsys, PD_NULL, CLK_NULL),
	[apmixed]  = REGBASE_V(0x1000c000, apmixed, PD_NULL, CLK_NULL),
	[gce]  = REGBASE_V(0x10238000, gce, MT6761_POWER_DOMAIN_DIS, CLK_NULL),
	[audio]    = REGBASE_V(0x11220000, audio, PD_NULL, CLK_NULL),
	[mipi_0a]    = REGBASE_V(0x11c10000, mipi_0a, PD_NULL, CLK_NULL),
	[mipi_0b]    = REGBASE_V(0x11c11000, mipi_0b, PD_NULL, CLK_NULL),
	[mipi_1a]    = REGBASE_V(0x11c12000, mipi_1a, PD_NULL, CLK_NULL),
	[mipi_1b]    = REGBASE_V(0x11c13000, mipi_1b, PD_NULL, CLK_NULL),
	[mipi_2a]    = REGBASE_V(0x11c14000, mipi_2a, PD_NULL, CLK_NULL),
	[mipi_2b]    = REGBASE_V(0x11c15000, mipi_2b, PD_NULL, CLK_NULL),
	[mfgsys]   = REGBASE_V(0x13ffe000, mfgsys, MT6761_POWER_DOMAIN_MFG_CORE0, CLK_NULL),
	[mmsys]    = REGBASE_V(0x14000000, mmsys, MT6761_POWER_DOMAIN_DIS, CLK_NULL),
	[camsys]   = REGBASE_V(0x15000000, camsys, MT6761_POWER_DOMAIN_CAM, CLK_NULL),
	[vencsys]  = REGBASE_V(0x17000000, vencsys, MT6761_POWER_DOMAIN_VCODEC, CLK_NULL),
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
	REGNAME(audio,	0x000, AUDIO_TOP_CON0),
	REGNAME(audio,	0x004, AUDIO_TOP_CON1),
	REGNAME(camsys,  0x000, CAMSYS_CG),
	REGNAME(infracfg,  0x074, PERI_BUS_DCM_CTRL),
	REGNAME(infracfg,  0x090, MODULE_SW_CG_0),
	REGNAME(infracfg,  0x094, MODULE_SW_CG_1),
	REGNAME(infracfg,  0x0ac, MODULE_SW_CG_2),
	REGNAME(infracfg,  0x0c8, MODULE_SW_CG_3),
	REGNAME(infracfg,  0x200, INFRA_TOPAXI_SI0_CTL),
	REGNAME(pericfg,  0x20C, PERIAXI_SI0_CTL),
	REGNAME(gce,  0x0F0, GCE_CTL_INT0),
	REGNAME(mfgsys,  0x000, MFG_CG),
	REGNAME(mmsys,	0x100, MMSYS_CG_CON0),
	REGNAME(vencsys,  0x000, VCODECSYS_CG),
	REGNAME(mipi_0a,  0x080, MIPI_RX_WRAPPER80_CSI0A),
	REGNAME(mipi_0b,  0x080, MIPI_RX_WRAPPER80_CSI0A),
	REGNAME(mipi_1a,  0x080, MIPI_RX_WRAPPER80_CSI0A),
	REGNAME(mipi_1b,  0x080, MIPI_RX_WRAPPER80_CSI0A),
	REGNAME(mipi_2a,  0x080, MIPI_RX_WRAPPER80_CSI0A),
	REGNAME(mipi_2b,  0x080, MIPI_RX_WRAPPER80_CSI0A),

	REGNAME(apmixed,  0x208, UNIVPLL_CON0),
	REGNAME(apmixed,  0x20C, UNIVPLL_CON1),
	REGNAME(apmixed,  0x214, UNIVPLL_PWR_CON0),
	REGNAME(apmixed,  0x218, MFGPLL_CON0),
	REGNAME(apmixed,  0x21C, MFGPLL_CON1),
	REGNAME(apmixed,  0x224, MFGPLL_PWR_CON0),
	REGNAME(apmixed,  0x228, MAINPLL_CON0),
	REGNAME(apmixed,  0x22C, MAINPLL_CON1),
	REGNAME(apmixed,  0x234, MAINPLL_PWR_CON0),
	REGNAME(apmixed,  0x30C, ARMPLL_CON0),
	REGNAME(apmixed,  0x310, ARMPLL_CON1),
	REGNAME(apmixed,  0x318, ARMPLL_PWR_CON0),
	REGNAME(apmixed,  0x31C, APLL1_CON0),
	REGNAME(apmixed,  0x320, APLL1_CON1),
	REGNAME(apmixed,  0x328, APLL1_CON3),
	REGNAME(apmixed,  0x32C, APLL1_PWR_CON0),
	REGNAME(apmixed,  0x330, MMPLL_CON0),
	REGNAME(apmixed,  0x334, MMPLL_CON1),
	REGNAME(apmixed,  0x33C, MMPLL_PWR_CON0),
	REGNAME(apmixed,  0x340, MPLL_CON0),
	REGNAME(apmixed,  0x344, MPLL_CON1),
	REGNAME(apmixed,  0x34C, MPLL_PWR_CON0),
	REGNAME(apmixed,  0x350, MSDCPLL_CON0),
	REGNAME(apmixed,  0x354, MSDCPLL_CON1),
	REGNAME(apmixed,  0x35C, MSDCPLL_PWR_CON0),
	REGNAME(apmixed,  0x014, AP_PLL_CON5),

	REGNAME(scpsys,  0x0180, PWR_STATUS),
	REGNAME(scpsys,  0x0184, PWR_STATUS_2ND),
	REGNAME(scpsys,  0x0300, VCODEC_PWR_CON),
	REGNAME(scpsys,  0x0308, ISP_PWR_CON),
	REGNAME(scpsys,  0x030C, DIS_PWR_CON),
	REGNAME(scpsys,  0x0314, AUDIO_PWR_CON),
	REGNAME(scpsys,  0x0318, IFR_PWR_CON),
	REGNAME(scpsys,  0x031C, DPY_PWR_CON),
	REGNAME(scpsys,  0x0320, MD1_PWR_CON),
	REGNAME(scpsys,  0x032C, CONN_PWR_CON),
	REGNAME(scpsys,  0x0334, MFG_ASYNC_PWR_CON),
	REGNAME(scpsys,  0x0338, MFG_PWR_CON),
	REGNAME(scpsys,  0x033C, MFG_CORE0_PWR_CON),
	REGNAME(scpsys,  0x0344, CAM_PWR_CON),
	{}
};

static const struct regname *get_all_regnames(void)
{
	return rn;
}

static void  init_regbase(void)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(rb); i++)
		rb[i].virt = ioremap(rb[i].phys, PAGE_SIZE);
}


static const char * const off_pll_names[] = {
	"univ2pll",
	"apll1",
	"mfgpll",
	"msdcpll",
	"mmpll",
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


/*
 * clkdbg pwr_status
 */


static u32 pwr_ofs[STA_NUM] = {
	[PWR_STA] = 0x180,
	[PWR_STA2] = 0x184,
};

static u32 pwr_sta[STA_NUM];

u32 *get_spm_pwr_status_array(void)
{
	static void __iomem *pwr_addr[STA_NUM];
	int i;

	for (i = 0; i < STA_NUM; i++) {
		if (pwr_ofs[i]) {
			pwr_addr[i] = rb[scpsys].virt + pwr_ofs[i];
			pwr_sta[i] = clk_readl(pwr_addr[i]);
		}
	}

	return pwr_sta;
}


static bool is_pll_chk_bug_on(void)
{
#if BUG_ON_CHK_ENABLE
	return true;
#endif
	return false;
}

void print_subsys_reg_mt6761(enum chk_sys_id id)
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
EXPORT_SYMBOL(print_subsys_reg_mt6761);

#if IS_ENABLED(CONFIG_MTK_DEVAPC)
static void devapc_dump(void)
{
	print_subsys_reg_mt6761(scpsys);
	print_subsys_reg_mt6761(topckgen);
	print_subsys_reg_mt6761(infracfg);
	print_subsys_reg_mt6761(apmixed);
}

static struct devapc_vio_callbacks devapc_vio_handle = {
	.id = DEVAPC_SUBSYS_CLKMGR,
	.debug_dump = devapc_dump,
};

#endif

static int get_vcore_opp(void)
{
#if IS_ENABLED(CONFIG_MTK_DVFSRC_HELPER) && CHECK_VCORE_FREQ
	return mtk_dvfsrc_query_opp_info(MTK_DVFSRC_SW_REQ_VCORE_OPP);
#else
	return VCORE_NULL;
#endif
}

static struct pvd_msk pvd_pwr_mask[] = {
	{"topckgen", PWR_STA, 0x00000000},
	{"infracfg_ao", PWR_STA, 0x00000000},
	{"apmixed", PWR_STA, 0x00000000},
	{"audclk", PWR_STA, 0x00000000},
	{"mmsys_config", PWR_STA, BIT(3)},
	{"camsys", PWR_STA, BIT(25)},
	{"vcodecsys", PWR_STA, BIT(26)},
	{},
};

static struct pvd_msk *get_pvd_pwr_mask(void)
{
	return pvd_pwr_mask;
}

/*
 * clkchk vf table
 */

// #if CHECK_VCORE_FREQ
// /*
//  * Opp 0 : 0.8v
//  * Opp 1 : 0.7v
//  * Opp 2 : 0.7~0.65v
//  * Opp 3 : 0.65v
//  */

// static struct mtk_vf vf_table[] = {
// 	/* Opp0, Opp1, Opp2, Opp3 */
// 	MTK_VF_TABLE("mm_sel", 457000, 320000, 230000, 230000),//mm_sel
// 	MTK_VF_TABLE("scp_sel", 416000, 364000, 273000, 273000),//scp_sel
// 	MTK_VF_TABLE("camtg_sel", 26000, 26000, 26000, 26000),//camtg_sel
// 	MTK_VF_TABLE("camtg1_sel", 26000, 26000, 26000, 26000),//camtg1_sel
// 	MTK_VF_TABLE("camtg2_sel", 26000, 26000, 26000, 26000),//camtg2_sel
// 	MTK_VF_TABLE("camtg3_sel", 26000, 26000, 26000, 26000),//camtg3_sel
// 	MTK_VF_TABLE("spi_sel", 109200, 109200, 109200, 109200),//spi_sel
// 	MTK_VF_TABLE("audio_sel", 54600, 54600, 54600, 54600),//audio_sel
// 	MTK_VF_TABLE("aud_intbus_sel", 136500, 136500, 136500, 136500),//aud_intbus_sel
// 	MTK_VF_TABLE("aud_1_sel", 196608, 196608, 196608, 196608),//aud_1_sel
// 	MTK_VF_TABLE("aud_engen1_sel", 26000, 26000, 26000, 26000),//aud_engen1_sel
// 	MTK_VF_TABLE("disp_pwm_sel", 125000, 125000, 125000, 125000),//disp_pwm_sel
// 	MTK_VF_TABLE("usb_top_sel", 62400, 62400, 62400, 62400),//usb_top_sel
// 	MTK_VF_TABLE("camtm_sel", 208000, 208000, 208000, 208000),//camtm_sel
// 	MTK_VF_TABLE("venc_sel", 457000, 416000, 312000, 312000),//venc_sel
// 	MTK_VF_TABLE("cam_sel", 546000, 320000, 230000, 230000),//cam_sel
// };
// #endif

// static struct mtk_vf *get_vf_table(void)
// {
// #if CHECK_VCORE_FREQ
// 	return vf_table;
// #else
	// return NULL;
// #endif
// }


/*
 * init functions
 */

static struct clkchk_ops clkchk_mt6761_ops = {
	.get_all_regnames = get_all_regnames,
	.get_spm_pwr_status_array = get_spm_pwr_status_array,
	.get_pvd_pwr_mask = get_pvd_pwr_mask,  // get_pvd_pwr_mask need to recheck
	.get_off_pll_names = get_off_pll_names,
	.get_notice_pll_names = get_notice_pll_names,
	.is_pll_chk_bug_on = is_pll_chk_bug_on,
	// .get_vf_table = get_vf_table,
	.get_vcore_opp = get_vcore_opp,
#if IS_ENABLED(CONFIG_MTK_DEVAPC)
	.devapc_dump = devapc_dump,
#endif
};

static int clk_chk_mt6761_probe(struct platform_device *pdev)
{
	init_regbase();

	set_clkchk_notify();

	set_clkchk_ops(&clkchk_mt6761_ops);

#if IS_ENABLED(CONFIG_MTK_DEVAPC)
	register_devapc_vio_callback(&devapc_vio_handle);
#endif

	return 0;
}

static const struct of_device_id of_match_clkchk_mt6761[] = {
	{
		.compatible = "mediatek,mt6761-clkchk",
	}, {
		/* sentinel */
	}
};

static struct platform_driver clk_chk_mt6761_drv = {
	.probe = clk_chk_mt6761_probe,
	.driver = {
		.name = "clk-chk-mt6761",
		.owner = THIS_MODULE,
		.pm = &clk_chk_dev_pm_ops,
		.of_match_table = of_match_clkchk_mt6761,
	},
};

/*
 * init functions
 */

static int __init clkchk_mt6761_init(void)
{
	return platform_driver_register(&clk_chk_mt6761_drv);
}

static void __exit clkchk_mt6761_exit(void)
{
	platform_driver_unregister(&clk_chk_mt6761_drv);
}

late_initcall(clkchk_mt6761_init);
module_exit(clkchk_mt6761_exit);
MODULE_LICENSE("GPL");

