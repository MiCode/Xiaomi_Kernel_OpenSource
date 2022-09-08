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

#include "clkchk-mt6768.h"
#include <dt-bindings/power/mt6768-power.h>
#include "clkchk.h"
#if IS_ENABLED(CONFIG_MTK_DEVAPC)
#include <devapc_public.h>
#endif
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
	[topckgen]	= REGBASE_V(0x10000000, topckgen, PD_NULL, CLK_NULL),
	[infracfg]	= REGBASE_V(0x10001000, infracfg, PD_NULL, CLK_NULL),
	[pericfg]	= REGBASE_V(0x10003000, pericfg, PD_NULL, CLK_NULL),
	[scpsys]	= REGBASE_V(0x10006000, scpsys, PD_NULL, CLK_NULL),
	[apmixedsys]	= REGBASE_V(0x1000c000, apmixedsys, PD_NULL, CLK_NULL),
	[audiosys]	= REGBASE_V(0x11220000, audiosys, PD_NULL, CLK_NULL),
	[gce]		= REGBASE_V(0x10238000, gce, PD_NULL, CLK_NULL),
	[mipi_0a]	= REGBASE_V(0x11c10000, mipi_0a, PD_NULL, CLK_NULL),
	[mipi_0b]	= REGBASE_V(0x11c11000, mipi_0b, PD_NULL, CLK_NULL),
	[mipi_1a]	= REGBASE_V(0x11c12000, mipi_1a, PD_NULL, CLK_NULL),
	[mipi_1b]	= REGBASE_V(0x11c13000, mipi_1b, PD_NULL, CLK_NULL),
	[mipi_2a]	= REGBASE_V(0x11c14000, mipi_2a, PD_NULL, CLK_NULL),
	[mipi_2b]	= REGBASE_V(0x11c15000, mipi_2b, PD_NULL, CLK_NULL),
	[mfgsys]	= REGBASE_V(0x13000000, mfgsys, MT6768_POWER_DOMAIN_MFG, CLK_NULL),
	[mmsys]		= REGBASE_V(0x14000000, mmsys, PD_NULL, CLK_NULL),
	[imgsys]	= REGBASE_V(0x15020000, imgsys, MT6768_POWER_DOMAIN_ISP, CLK_NULL),
	[camsys]	= REGBASE_V(0x1a000000, camsys, MT6768_POWER_DOMAIN_CAM, CLK_NULL),
	[vencsys]	= REGBASE_V(0x17000000, vencsys, MT6768_POWER_DOMAIN_VENC, CLK_NULL),
	[vdecsys]	= REGBASE_V(0x16000000, vdecsys, MT6768_POWER_DOMAIN_VDEC, CLK_NULL),
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

static void  init_regbase(void)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(rb); i++)
		rb[i].virt = ioremap(rb[i].phys, PAGE_SIZE);
}


static const char * const off_pll_names[] = {
		"univ2pll",
		"mfgpll",
		"apll1",
		"mmpll",
		"msdcpll",
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

void print_subsys_reg_mt6768(enum chk_sys_id id)
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
EXPORT_SYMBOL(print_subsys_reg_mt6768);

#if IS_ENABLED(CONFIG_MTK_DEVAPC)
static void devapc_dump(void)
{
	print_subsys_reg_mt6768(scpsys);
	print_subsys_reg_mt6768(topckgen);
	print_subsys_reg_mt6768(infracfg);
	print_subsys_reg_mt6768(apmixedsys);
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


/*
 * clkchk pwr_msk  need to review
 */
static struct pvd_msk pvd_pwr_mask[] = {

	{"topckgen", PWR_STA, 0x00000000},
	{"infracfg", PWR_STA, BIT(3)},
	{"apmixedsys", PWR_STA, 0x00000000},
	{"mfg", PWR_STA, BIT(11)},
	{"mfgcore0", PWR_STA, BIT(12)},
	{"mfgcore1", PWR_STA, BIT(13)},
	{"mfgasync", PWR_STA, BIT(14)},
	{"mmsys", PWR_STA, BIT(5)},
	{"imgsys", PWR_STA, BIT(6)},
	{"camsys", PWR_STA, BIT(7)},
	{"vencsys", PWR_STA, BIT(9)},
	{"vdecsys", PWR_STA, BIT(8)},
	{},
};

static struct pvd_msk *get_pvd_pwr_mask(void)
{
	return pvd_pwr_mask;
}

/*
 * clkchk vf table
 */

#if CHECK_VCORE_FREQ
/*
 * Opp 0 : 0.8v
 * Opp 1 : 0.7v
 * Opp 2 : 0.7~0.65v
 * Opp 3 : 0.65v
 */

static struct mtk_vf vf_table[] = {
	/* Opp0, Opp1, Opp2, Opp3 */
	MTK_VF_TABLE("mm_sel", 457000, 320000, 230000, 230000),//mm_sel
	MTK_VF_TABLE("scp_sel", 416000, 364000, 273000, 273000),//scp_sel
	MTK_VF_TABLE("camtg_sel", 26000, 26000, 26000, 26000),//camtg_sel
	MTK_VF_TABLE("camtg1_sel", 26000, 26000, 26000, 26000),//camtg1_sel
	MTK_VF_TABLE("camtg2_sel", 26000, 26000, 26000, 26000),//camtg2_sel
	MTK_VF_TABLE("camtg3_sel", 26000, 26000, 26000, 26000),//camtg3_sel
	MTK_VF_TABLE("spi_sel", 109200, 109200, 109200, 109200),//spi_sel
	MTK_VF_TABLE("audio_sel", 54600, 54600, 54600, 54600),//audio_sel
	MTK_VF_TABLE("aud_intbus_sel", 136500, 136500, 136500, 136500),//aud_intbus_sel
	MTK_VF_TABLE("aud_1_sel", 196608, 196608, 196608, 196608),//aud_1_sel
	MTK_VF_TABLE("aud_engen1_sel", 26000, 26000, 26000, 26000),//aud_engen1_sel
	MTK_VF_TABLE("disp_pwm_sel", 125000, 125000, 125000, 125000),//disp_pwm_sel
	MTK_VF_TABLE("usb_top_sel", 62400, 62400, 62400, 62400),//usb_top_sel
	MTK_VF_TABLE("camtm_sel", 208000, 208000, 208000, 208000),//camtm_sel
	MTK_VF_TABLE("venc_sel", 457000, 416000, 312000, 312000),//venc_sel
	MTK_VF_TABLE("cam_sel", 546000, 320000, 230000, 230000),//cam_sel
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


/*
 * init functions
 */

static struct clkchk_ops clkchk_mt6768_ops = {
	.get_all_regnames = get_all_regnames,
	.get_spm_pwr_status_array = get_spm_pwr_status_array,
	.get_pvd_pwr_mask = get_pvd_pwr_mask,  // get_pvd_pwr_mask need to recheck
	.get_off_pll_names = get_off_pll_names,
	.get_notice_pll_names = get_notice_pll_names,
	.is_pll_chk_bug_on = is_pll_chk_bug_on,
	.get_vf_table = get_vf_table,
	.get_vcore_opp = get_vcore_opp,
#if IS_ENABLED(CONFIG_MTK_DEVAPC)
	.devapc_dump = devapc_dump,
#endif
};

static int clk_chk_mt6768_probe(struct platform_device *pdev)
{
	init_regbase();

	set_clkchk_notify();

	set_clkchk_ops(&clkchk_mt6768_ops);

#if IS_ENABLED(CONFIG_MTK_DEVAPC)
	register_devapc_vio_callback(&devapc_vio_handle);
#endif

	return 0;
}

static struct platform_driver clk_chk_mt6768_drv = {
	.probe = clk_chk_mt6768_probe,
	.driver = {
		.name = "clk-chk-mt6768",
		.owner = THIS_MODULE,
		.pm = &clk_chk_dev_pm_ops,
	},
};

/*
 * init functions
 */

static int __init clkchk_mt6768_init(void)
{
	static struct platform_device *clk_chk_dev;

	clk_chk_dev = platform_device_register_simple("clk-chk-mt6768", -1, NULL, 0);
	if (IS_ERR(clk_chk_dev))
		pr_warn("unable to register clk-chk device");

	return platform_driver_register(&clk_chk_mt6768_drv);
}

static void __exit clkchk_mt6768_exit(void)
{
	platform_driver_unregister(&clk_chk_mt6768_drv);
}

subsys_initcall(clkchk_mt6768_init);
module_exit(clkchk_mt6768_exit);
MODULE_LICENSE("GPL");

