// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2021 MediaTek Inc.
// Author: Owen Chen <owen.chen@mediatek.com>

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "clk-fmeter.h"
#include "clk-mt6739-fmeter.h"

#define FM_TIMEOUT		30

//static DEFINE_SPINLOCK(meter_lock);
//#define fmeter_lock(flags)   spin_lock_irqsave(&meter_lock, flags)
//#define fmeter_unlock(flags) spin_unlock_irqrestore(&meter_lock, flags)
/*
 * clk fmeter
 */
#define clk_readl(addr)		readl(addr)
#define clk_writel(addr, val)	\
	do { writel(val, addr); wmb(); } while (0) /* sync write */

// #define FMCLK2(_t, _i, _n, _o, _p, _c) { .type = _t, \
//		.id = _i, .name = _n, .ofs = _o, .pdn = _p, .ck_div = _c}
#define FMCLK(_t, _i, _n, _c) { .type = _t, .id = _i, .name = _n, .ck_div = _c}

static const struct fmeter_clk fclks[] = {
	/* CKGEN Part normal 7 15 23 31 bit*/
	FMCLK(CKGEN, hd_faxi_ck, "hd_faxi_ck", 1),
	FMCLK(CKGEN, hf_fddrphycfg_ck, "hf_fddrphycfg_ck", 1),
	FMCLK(CKGEN, hf_fmm_ck, "hf_fmm_ck", 1),
	FMCLK(CKGEN, hf_fmfg_ck, "hf_fmfg_ck", 1),
	FMCLK(CKGEN, f_fcamtg_ck, "f_fcamtg_ck", 1),
	FMCLK(CKGEN, f_fuart_ck, "f_fuart_ck", 1),
	FMCLK(CKGEN, hf_fspi_ck, "hf_fspi_ck", 1),
	FMCLK(CKGEN, hf_fmsdc50_0_hclk_ck, "hf_fmsdc50_0_hclk_ck", 1),
	FMCLK(CKGEN, hf_fmsdc50_0_ck, "hf_fmsdc50_0_ck", 1),
	FMCLK(CKGEN, hf_fmsdc30_1_ck, "hf_fmsdc30_1_ck", 1),
	FMCLK(CKGEN, hf_faudio_ck, "hf_faudio_ck", 1),
	FMCLK(CKGEN, hf_faud_intbus_ck, "hf_faud_intbus_ck", 1),
	FMCLK(CKGEN, hf_fdbi0_ck, "hf_fdbi0_ck", 1),
	FMCLK(CKGEN, hf_fscam_ck, "hf_fscam_ck", 1),
	FMCLK(CKGEN, hf_faud_1_ck, "hf_faud_1_ck", 1),
	FMCLK(CKGEN, f_fdisp_pwm_ck, "f_fdisp_pwm_ck", 1),
	FMCLK(CKGEN, hf_fnfi2x_ck, "hf_fnfi2x_ck", 1),
	FMCLK(CKGEN, hf_fnfiecc_ck, "hf_fnfiecc_ck", 1),
	FMCLK(CKGEN, f_fusb_top_ck, "f_fusb_top_ck", 1),
	FMCLK(CKGEN, hg_fspm_ck, "hg_fspm_ck", 1),
	FMCLK(CKGEN, f_fi2c_ck, "f_fi2c_ck", 1),
	FMCLK(CKGEN, f_fseninf_ck, "f_fseninf_ck", 1),
	FMCLK(CKGEN, f_fdxcc_ck, "f_fdxcc_ck", 1),
	FMCLK(CKGEN, hf_faud_engin1_ck, "hf_faud_engin1_ck", 1),
	FMCLK(CKGEN, hf_faud_engin2_ck, "hf_faud_engin2_ck", 1),
	FMCLK(CKGEN, f_fcamtg2_ck, "f_fcamtg2_ck", 1),
	FMCLK(CKGEN, hf_fnfi1x_ck, "hf_fnfi1x_ck", 1),
	FMCLK(CKGEN, f_ufs_mp_sap_cfg_ck, "f_ufs_mp_sap_cfg_ck", 1),
	FMCLK(CKGEN, f_ufs_tick1us_ck, "f_ufs_tick1us_ck", 1),
	FMCLK(CKGEN, hd_faxi_east_ck, "hd_faxi_east_ck", 1),
	FMCLK(CKGEN, hd_faxi_west_ck, "hd_faxi_west_ck", 1),
	FMCLK(CKGEN, hd_faxi_north_ck, "hd_faxi_north_ck", 1),
	FMCLK(CKGEN, hd_faxi_south_ck, "hd_faxi_south_ck", 1),
	FMCLK(CKGEN, hg_fmipicfg_tx_ck, "hg_fmipicfg_tx_ck", 1),
	FMCLK(CKGEN, fmem_ck_bfe_dcm_ch0, "fmem_ck_bfe_dcm_ch0", 1),
	FMCLK(CKGEN, fmem_ck_aft_dcm_ch0, "fmem_ck_aft_dcm_ch0", 1),
	FMCLK(CKGEN, dramc_pll104m_ck, "dramc_pll104m_ck", 1),
	//ABIST
	FMCLK(ABIST, AD_CSI0_DELAY_TSTCLK, "AD_CSI0_DELAY_TSTCLK", 1),
	FMCLK(ABIST, AD_CSI1_DELAY_TSTCLK, "AD_CSI1_DELAY_TSTCLK", 1),
	FMCLK(ABIST, AD_MDBPIPLL_DIV3_CK, "AD_MDBPIPLL_DIV3_CK", 1),
	FMCLK(ABIST, AD_MDBPIPLL_DIV7_CK, "AD_MDBPIPLL_DIV7_CK", 1),
	FMCLK(ABIST, AD_MDBRPPLL_DIV6_CK, "AD_MDBRPPLL_DIV6_CK", 1),
	FMCLK(ABIST, AD_UNIV_624M_CK, "AD_UNIV_624M_CK", 1),
	FMCLK(ABIST, AD_MAIN_H546_CK, "AD_MAIN_H546_CK", 1),
	FMCLK(ABIST, AD_MEMPLL_MONCLK, "AD_MEMPLL_MONCLK", 1),
	FMCLK(ABIST, AD_MEMPLL2_MONCLK, "AD_MEMPLL2_MONCLK", 1),
	FMCLK(ABIST, AD_MEMPLL3_MONCLK, "AD_MEMPLL3_MONCLK", 1),
	FMCLK(ABIST, AD_MEMPLL4_MONCLK, "AD_MEMPLL4_MONCLK", 1),
	FMCLK(ABIST, AD_MDPLL_FS26M_CK, "AD_MDPLL_FS26M_CK", 1),
	FMCLK(ABIST, AD_ARMPLL_L_CK, "AD_ARMPLL_L_CK", 1),
	FMCLK(ABIST, AD_ARMPLL_LL_CK, "AD_ARMPLL_LL_CK", 1),
	FMCLK(ABIST, AD_MAINPLL_CK, "AD_MAINPLL_CK", 1),
	FMCLK(ABIST, AD_UNIVPLL_CK, "AD_UNIVPLL_CK", 1),
	FMCLK(ABIST, AD_MSDCPLL_CK, "AD_MSDCPLL_CK", 1),
	FMCLK(ABIST, AD_MMPLL_CK, "AD_MMPLL_CK", 1),
	FMCLK(ABIST, AD_APLL1_CK, "AD_APLL1_CK", 1),
	FMCLK(ABIST, AD_APPLLGP_TST_CK, "AD_APPLLGP_TST_CK", 1),
	FMCLK(ABIST, AD_USB20_192M_CK, "AD_USB20_192M_CK", 1),
	FMCLK(ABIST, AD_UNIV_192M_CK, "AD_UNIV_192M_CK", 1),
	FMCLK(ABIST, AD_VENCPLL_CK, "AD_VENCPLL_CK", 1),
	FMCLK(ABIST, AD_DSI0_MPPLL_TST_CK, "AD_DSI0_MPPLL_TST_CK", 1),
	FMCLK(ABIST, AD_DSI0_LNTC_DSICLK, "AD_DSI0_LNTC_DSICLK", 1),
	FMCLK(ABIST, rtc32k_ck_i, "rtc32k_ck_i", 1),
	FMCLK(ABIST, mcusys_arm_clk_out_all, "mcusys_arm_clk_out_all", 1),
	FMCLK(ABIST, msdc01_in_ck, "msdc01_in_ck", 1),
	FMCLK(ABIST, msdc02_in_ck, "msdc02_in_ck", 1),
	FMCLK(ABIST, msdc11_in_ck, "msdc11_in_ck", 1),
	FMCLK(ABIST, msdc12_in_ck, "msdc12_in_ck", 1),
	FMCLK(ABIST, AD_MPLL_208M_CK, "AD_MPLL_208M_CK", 1),
	FMCLK(ABIST, DA_USB20_48M_DIV_CK, "DA_USB20_48M_DIV_CK", 1),
	FMCLK(ABIST, DA_UNIV_48M_DIV_CK, "DA_UNIV_48M_DIV_CK", 1),
	{},
};

#define _CKGEN(x)		(topck_base + (x))
#define CLK_MISC_CFG_0		_CKGEN(0x104)
#define CLK_DBG_CFG		_CKGEN(0x10C)
#define CLK26CALI_0		_CKGEN(0x220)
#define CLK26CALI_1		_CKGEN(0x224)

static void __iomem *topck_base;
static void __iomem *apmixed_base;
static void __iomem *spm_base;

const struct fmeter_clk *mt6739_get_fmeter_clks(void)
{
	return fclks;
}

static unsigned int check_mux_pdn(unsigned int ID)
{
	int i;
	if ((ID > 0) && (ID < 64)) {
		for (i = 0; i < ARRAY_SIZE(fclks) - 1; i++)
			if (fclks[i].id == ID)
				break;
		if (i >= ARRAY_SIZE(fclks) - 1)
			return 1;
		if ((clk_readl(topck_base + fclks[i].ofs)
				& BIT(fclks[i].pdn)))
			return 1;
		else
			return 0;
	} else
		return 1;
}

/* need implement ckgen&abist api here */
/* implement done */
static unsigned int mt6739_get_ckgen_freq(unsigned int ID)
{
	int output = 0, i = 0;
	unsigned int temp, clk26cali_0, clk_dbg_cfg, clk_misc_cfg_0, clk26cali_1;

	if (check_mux_pdn(ID)) {
		pr_notice("ID-%d: MUX PDN, return 0.\n", ID);
		return 0;
	}

	clk_dbg_cfg = clk_readl(CLK_DBG_CFG);
	clk_writel(CLK_DBG_CFG,
		(clk_dbg_cfg & 0xFFFFC0FC)|(ID << 8)|(0x1));

	clk_misc_cfg_0 = clk_readl(CLK_MISC_CFG_0);
	clk_writel(CLK_MISC_CFG_0, (clk_misc_cfg_0 & 0x00FFFFFF)|0x00000010);
	clk26cali_0 = clk_readl(CLK26CALI_0);
	clk26cali_1 = clk_readl(CLK26CALI_1);
	clk_writel(CLK26CALI_0, 0x1000);
	clk_writel(CLK26CALI_0, 0x1010);

	/* wait frequency meter finish */
	while (clk_readl(CLK26CALI_0) & 0x10) {
		udelay(10);
		i++;
		if (i > 10000)
			break;
	}

	temp = clk_readl(CLK26CALI_1) & 0xFFFF;

	output = ((temp * 26000)) / 1024;

	clk_writel(CLK_DBG_CFG, clk_dbg_cfg);
	clk_writel(CLK_MISC_CFG_0, clk_misc_cfg_0|0x00000010);
	clk_writel(CLK26CALI_0, clk26cali_0);
	clk_writel(CLK26CALI_1, clk26cali_1);

	/*pr_debug("%s = %d Khz\n", ckgen_array[ID-1], output);*/
	if (i > 10000)
		return 0;
	else
		return output;
}

static unsigned int mt6739_get_abist_freq(unsigned int ID)
{
	int output = 0, i = 0;
	unsigned int temp, clk26cali_0, clk_dbg_cfg, clk_misc_cfg_0, clk26cali_1;

	clk_dbg_cfg = clk_readl(CLK_DBG_CFG);
	clk_writel(CLK_DBG_CFG, (clk_dbg_cfg & 0xFFC0FFFC)|(ID << 16));
	clk_misc_cfg_0 = clk_readl(CLK_MISC_CFG_0);
	clk_writel(CLK_MISC_CFG_0, (clk_misc_cfg_0 & 0x00FFFFFF)|0x00000010);
	clk26cali_0 = clk_readl(CLK26CALI_0);
	clk26cali_1 = clk_readl(CLK26CALI_1);
	clk_writel(CLK26CALI_0, 0x1000);
	clk_writel(CLK26CALI_0, 0x1010);

	/* wait frequency meter finish */
	while (clk_readl(CLK26CALI_0) & 0x10) {
		udelay(10);
		i++;
		if (i > 10000)
		break;
	}

	temp = clk_readl(CLK26CALI_1) & 0xFFFF;

	output = ((temp * 26000)) / 1024;

	clk_writel(CLK_DBG_CFG, clk_dbg_cfg);
	clk_writel(CLK_MISC_CFG_0, clk_misc_cfg_0|0x00000010);
	clk_writel(CLK26CALI_0, clk26cali_0);
	clk_writel(CLK26CALI_1, clk26cali_1);

	/*pr_debug("%s = %d Khz\n", abist_array[ID-1], output);*/
	if (i > 10000)
		return 0;
	else
		return output;
}

static unsigned int mt6739_get_abist2_freq(unsigned int ID)
{
	pr_notice("mt6739 not support abist2 func\n");
	return 0;
}

static unsigned int mt6739_get_fmeter_freq(unsigned int id, enum  FMETER_TYPE type)
{
	if (type == CKGEN)
		return mt6739_get_ckgen_freq(id);
	else if (type == ABIST)
		return mt6739_get_abist_freq(id);
	else if (type == ABIST_2)
		return mt6739_get_abist2_freq(id);
	return FT_NULL;
}

static int mt6739_get_fmeter_id(enum FMETER_ID fid)
{
	if (fid == FID_DISP_PWM)
		return f_fdisp_pwm_ck;
	/*else if (fid == FID_ULPOSC1)
		return f_fpwrap_ulposc_ck;*/
	return FID_NULL;
}

static void __iomem *get_base_from_comp(const char *comp)
{
	struct device_node *node;
	static void __iomem *base;
	node = of_find_compatible_node(NULL, NULL, comp);
	if (node) {
		base = of_iomap(node, 0);
		if (!base) {
			pr_err("%s() can't find iomem for %s\n",
					__func__, comp);
			return ERR_PTR(-EINVAL);
		}
		return base;
	}
	pr_err("%s can't find compatible node\n", __func__);
	return ERR_PTR(-EINVAL);
}

/*
 * init functions
 */
static struct fmeter_ops fm_ops = {
	.get_fmeter_clks = mt6739_get_fmeter_clks,
	.get_ckgen_freq = mt6739_get_ckgen_freq,
	.get_abist_freq = mt6739_get_abist_freq,
	.get_abist2_freq = mt6739_get_abist2_freq,
	.get_fmeter_freq = mt6739_get_fmeter_freq,
	.get_fmeter_id = mt6739_get_fmeter_id,
};

static int clk_fmeter_mt6739_probe(struct platform_device *pdev)
{
	topck_base = get_base_from_comp("mediatek,topckgen");
	if (IS_ERR(topck_base))
		goto ERR;
	apmixed_base = get_base_from_comp("mediatek,apmixedsys");
	if (IS_ERR(apmixed_base))
		goto ERR;
	spm_base = get_base_from_comp("mediatek,mt6739-scpsys");
	if (IS_ERR(spm_base))
		goto ERR;
	fmeter_set_ops(&fm_ops);
	pr_info("clk_fmeter_mt6739_probe done");
	return 0;
ERR:
	pr_err("%s can't find base\n", __func__);
	return -EINVAL;
}

static struct platform_driver clk_fmeter_mt6739_drv = {
	.probe = clk_fmeter_mt6739_probe,
	.driver = {
		.name = "clk-fmeter-mt6739",
		.owner = THIS_MODULE,
	},
};

static int __init clk_fmeter_init(void)
{
	static struct platform_device *clk_fmeter_dev;
	clk_fmeter_dev = platform_device_register_simple("clk-fmeter-mt6739", -1, NULL, 0);
	if (IS_ERR(clk_fmeter_dev))
		pr_warn("unable to register clk-fmeter device");
	return platform_driver_register(&clk_fmeter_mt6739_drv);
}

static void __exit clk_fmeter_exit(void)
{
	platform_driver_unregister(&clk_fmeter_mt6739_drv);
}

subsys_initcall(clk_fmeter_init);
module_exit(clk_fmeter_exit);
MODULE_LICENSE("GPL");

