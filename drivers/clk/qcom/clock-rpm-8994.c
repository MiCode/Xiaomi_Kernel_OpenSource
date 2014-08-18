/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/ctype.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/clk/msm-clock-generic.h>
#include <soc/qcom/clock-rpm.h>
#include <soc/qcom/clock-voter.h>
#include <soc/qcom/clock-local2.h>
#include <soc/qcom/rpm-smd.h>

#include <dt-bindings/clock/msm-clocks-8994.h>

#define RPM_MISC_CLK_TYPE	0x306b6c63
#define RPM_BUS_CLK_TYPE	0x316b6c63
#define RPM_MEM_CLK_TYPE	0x326b6c63
#define RPM_IPA_CLK_TYPE	0x617069
#define RPM_CE_CLK_TYPE		0x6563
#define RPM_MCFG_CLK_TYPE	0x6766636d

#define RPM_SMD_KEY_ENABLE	0x62616E45

#define CXO_CLK_SRC_ID		0x0
#define QDSS_CLK_ID		0x1

#define PNOC_CLK_ID		0x0
#define SNOC_CLK_ID		0x1
#define CNOC_CLK_ID		0x2
#define MMSSNOC_AHB_CLK_ID	0x3

#define BIMC_CLK_ID		0x0
#define GFX3D_CLK_SRC_ID	0x1
#define OCMEMGX_CLK_ID		0x2

#define IPA_CLK_ID		0x0

#define CE1_CLK_ID		0x0
#define CE2_CLK_ID		0x1
#define CE3_CLK_ID		0x2

#define MSS_CFG_AHB_CLK_ID      0x0

#define BB_CLK1_ID	0x1
#define BB_CLK2_ID	0x2
#define RF_CLK1_ID	0x4
#define RF_CLK2_ID	0x5
#define LN_BB_CLK_ID	0x8
#define DIV_CLK1_ID	0xb
#define DIV_CLK2_ID	0xc
#define DIV_CLK3_ID	0xd
#define BB_CLK1_PIN_ID	0x1
#define BB_CLK2_PIN_ID	0x2
#define RF_CLK1_PIN_ID	0x4
#define RF_CLK2_PIN_ID	0x5

static void __iomem *virt_base;

DEFINE_CLK_RPM_SMD_BRANCH(cxo_clk_src, cxo_clk_src_ao, RPM_MISC_CLK_TYPE,
			  CXO_CLK_SRC_ID, 19200000);
DEFINE_CLK_RPM_SMD(pnoc_clk, pnoc_a_clk, RPM_BUS_CLK_TYPE,
		   PNOC_CLK_ID, NULL);
DEFINE_CLK_RPM_SMD(ocmemgx_clk, ocmemgx_a_clk, RPM_MEM_CLK_TYPE,
		   OCMEMGX_CLK_ID, NULL);
DEFINE_CLK_RPM_SMD(bimc_clk, bimc_a_clk, RPM_MEM_CLK_TYPE,
		   BIMC_CLK_ID, NULL);
DEFINE_CLK_RPM_SMD(cnoc_clk, cnoc_a_clk, RPM_BUS_CLK_TYPE,
		   CNOC_CLK_ID, NULL);
DEFINE_CLK_RPM_SMD(gfx3d_clk_src, gfx3d_clk_src_ao, RPM_MEM_CLK_TYPE,
		   GFX3D_CLK_SRC_ID, NULL);
DEFINE_CLK_RPM_SMD(snoc_clk, snoc_a_clk, RPM_BUS_CLK_TYPE,
		   SNOC_CLK_ID, NULL);
DEFINE_CLK_RPM_SMD_XO_BUFFER(bb_clk1, bb_clk1_ao, BB_CLK1_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(bb_clk1_pin, bb_clk1_pin_ao,
				     BB_CLK1_PIN_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(bb_clk2, bb_clk2_ao, BB_CLK2_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(bb_clk2_pin, bb_clk2_pin_ao,
				     BB_CLK2_PIN_ID);
static DEFINE_CLK_VOTER(bimc_msmbus_clk, &bimc_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(bimc_msmbus_a_clk, &bimc_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(cnoc_msmbus_clk, &cnoc_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(cnoc_msmbus_a_clk, &cnoc_a_clk.c, LONG_MAX);
static DEFINE_CLK_BRANCH_VOTER(cxo_dwc3_clk, &cxo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(cxo_lpm_clk, &cxo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(cxo_otg_clk, &cxo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(cxo_pil_lpass_clk, &cxo_clk_src.c);
DEFINE_CLK_RPM_SMD_XO_BUFFER(div_clk1, div_clk1_ao, DIV_CLK1_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(div_clk2, div_clk2_ao, DIV_CLK2_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(div_clk3, div_clk3_ao, DIV_CLK3_ID);
DEFINE_CLK_RPM_SMD(ipa_clk, ipa_a_clk, RPM_IPA_CLK_TYPE,
		   IPA_CLK_ID, NULL);
DEFINE_CLK_RPM_SMD_XO_BUFFER(ln_bb_clk, ln_bb_a_clk, LN_BB_CLK_ID);
DEFINE_CLK_RPM_SMD(mmssnoc_ahb_clk, mmssnoc_ahb_a_clk, RPM_BUS_CLK_TYPE,
		   MMSSNOC_AHB_CLK_ID, NULL);
DEFINE_CLK_RPM_SMD_BRANCH(mss_cfg_ahb_clk, mss_cfg_ahb_a_clk, RPM_MCFG_CLK_TYPE,
			  MSS_CFG_AHB_CLK_ID, 19200000);
static DEFINE_CLK_VOTER(ocmemgx_core_clk, &ocmemgx_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(ocmemgx_msmbus_clk, &ocmemgx_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(ocmemgx_msmbus_a_clk, &ocmemgx_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(oxili_gfx3d_clk_src, &gfx3d_clk_src.c, LONG_MAX);
static DEFINE_CLK_VOTER(pnoc_keepalive_a_clk, &pnoc_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(pnoc_msmbus_clk, &pnoc_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(pnoc_msmbus_a_clk, &pnoc_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(pnoc_pm_clk, &pnoc_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(pnoc_sps_clk, &pnoc_clk.c, 0);
DEFINE_CLK_RPM_SMD_QDSS(qdss_clk, qdss_a_clk, RPM_MISC_CLK_TYPE,
			QDSS_CLK_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(rf_clk1, rf_clk1_ao, RF_CLK1_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(rf_clk1_pin, rf_clk1_pin_ao,
				     RF_CLK1_PIN_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(rf_clk2, rf_clk2_ao, RF_CLK2_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(rf_clk2_pin, rf_clk2_pin_ao,
				     RF_CLK2_PIN_ID);
static DEFINE_CLK_VOTER(snoc_msmbus_clk, &snoc_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(snoc_msmbus_a_clk, &snoc_a_clk.c, LONG_MAX);
DEFINE_CLK_RPM_SMD(ce1_clk, ce1_a_clk, RPM_CE_CLK_TYPE,
		   CE1_CLK_ID, NULL);
DEFINE_CLK_DUMMY(gcc_ce1_ahb_m_clk, 0);
DEFINE_CLK_DUMMY(gcc_ce1_axi_m_clk, 0);
static DEFINE_CLK_VOTER(mcd_ce1_clk, &ce1_clk.c, 85710000);
static DEFINE_CLK_VOTER(qseecom_ce1_clk, &ce1_clk.c, 85710000);
static DEFINE_CLK_VOTER(scm_ce1_clk, &ce1_clk.c, 85710000);
static DEFINE_CLK_VOTER(qcedev_ce1_clk, &ce1_clk.c, 85710000);
static DEFINE_CLK_VOTER(qcrypto_ce1_clk, &ce1_clk.c, 85710000);

DEFINE_CLK_RPM_SMD(ce2_clk, ce2_a_clk, RPM_CE_CLK_TYPE,
		   CE2_CLK_ID, NULL);
DEFINE_CLK_DUMMY(gcc_ce2_ahb_m_clk, 0);
DEFINE_CLK_DUMMY(gcc_ce2_axi_m_clk, 0);
static DEFINE_CLK_VOTER(mcd_ce2_clk, &ce2_clk.c, 85710000);
static DEFINE_CLK_VOTER(qseecom_ce2_clk, &ce2_clk.c, 85710000);
static DEFINE_CLK_VOTER(scm_ce2_clk, &ce2_clk.c, 85710000);
static DEFINE_CLK_VOTER(qcedev_ce2_clk, &ce2_clk.c, 85710000);
static DEFINE_CLK_VOTER(qcrypto_ce2_clk, &ce2_clk.c, 85710000);

DEFINE_CLK_RPM_SMD(ce3_clk, ce3_a_clk, RPM_CE_CLK_TYPE,
		   CE3_CLK_ID, NULL);
DEFINE_CLK_DUMMY(gcc_ce3_ahb_m_clk, 0);
DEFINE_CLK_DUMMY(gcc_ce3_axi_m_clk, 0);
static DEFINE_CLK_VOTER(mcd_ce3_clk, &ce3_clk.c, 85710000);
static DEFINE_CLK_VOTER(qseecom_ce3_clk, &ce3_clk.c, 85710000);
static DEFINE_CLK_VOTER(scm_ce3_clk, &ce3_clk.c, 85710000);
static DEFINE_CLK_VOTER(qcedev_ce3_clk, &ce3_clk.c, 85710000);
static DEFINE_CLK_VOTER(qcrypto_ce3_clk, &ce3_clk.c, 85710000);
DEFINE_CLK_DUMMY(gcc_bimc_kpss_axi_m_clk, 0);
DEFINE_CLK_DUMMY(gcc_mmss_bimc_gfx_m_clk, 0);

static struct mux_clk rpm_debug_mux = {
	.ops = &mux_reg_ops,
	.en_mask = BIT(16),
	.mask = 0x3FF,
	.base = &virt_base,
	MUX_SRC_LIST(
		{ &cnoc_clk.c, 0x0008 },
		{ &pnoc_clk.c, 0x0010 },
		{ &snoc_clk.c, 0x0000 },
		{ &bimc_clk.c, 0x015c },
		{ &mss_cfg_ahb_clk.c, 0x0030 },
		{ &gcc_mmss_bimc_gfx_m_clk.c, 0x002c },
		{ &ce1_clk.c, 0x0138 },
		{ &gcc_ce1_axi_m_clk.c, 0x0139 },
		{ &gcc_ce1_ahb_m_clk.c, 0x013a },
		{ &ce2_clk.c, 0x0140 },
		{ &gcc_ce2_axi_m_clk.c, 0x0141 },
		{ &gcc_ce2_ahb_m_clk.c, 0x0142 },
		{ &gcc_bimc_kpss_axi_m_clk.c, 0x0155 },
		{ &ce3_clk.c, 0x0228 },
		{ &gcc_ce3_axi_m_clk.c, 0x0229 },
		{ &gcc_ce3_ahb_m_clk.c, 0x022a },
	),
	.c = {
		.dbg_name = "rpm_debug_mux",
		.ops = &clk_ops_gen_mux,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(rpm_debug_mux.c),
	},
};

static struct clk_lookup msm_clocks_rpm_8994[] = {
	CLK_LIST(cxo_clk_src),
	CLK_LIST(pnoc_clk),
	CLK_LIST(ocmemgx_clk),
	CLK_LIST(pnoc_a_clk),
	CLK_LIST(bimc_clk),
	CLK_LIST(bimc_a_clk),
	CLK_LIST(cnoc_clk),
	CLK_LIST(cnoc_a_clk),
	CLK_LIST(gfx3d_clk_src),
	CLK_LIST(ocmemgx_a_clk),
	CLK_LIST(snoc_clk),
	CLK_LIST(snoc_a_clk),
	CLK_LIST(bb_clk1),
	CLK_LIST(bb_clk1_ao),
	CLK_LIST(bb_clk1_pin),
	CLK_LIST(bb_clk1_pin_ao),
	CLK_LIST(bb_clk2),
	CLK_LIST(bb_clk2_ao),
	CLK_LIST(bb_clk2_pin),
	CLK_LIST(bb_clk2_pin_ao),
	CLK_LIST(bimc_msmbus_clk),
	CLK_LIST(bimc_msmbus_a_clk),
	CLK_LIST(ce1_a_clk),
	CLK_LIST(ce2_a_clk),
	CLK_LIST(ce3_a_clk),
	CLK_LIST(cnoc_msmbus_clk),
	CLK_LIST(cnoc_msmbus_a_clk),
	CLK_LIST(cxo_clk_src_ao),
	CLK_LIST(cxo_dwc3_clk),
	CLK_LIST(cxo_lpm_clk),
	CLK_LIST(cxo_otg_clk),
	CLK_LIST(cxo_pil_lpass_clk),
	CLK_LIST(div_clk1),
	CLK_LIST(div_clk1_ao),
	CLK_LIST(div_clk2),
	CLK_LIST(div_clk2_ao),
	CLK_LIST(div_clk3),
	CLK_LIST(div_clk3_ao),
	CLK_LIST(gfx3d_clk_src_ao),
	CLK_LIST(ipa_clk),
	CLK_LIST(ipa_a_clk),
	CLK_LIST(ln_bb_clk),
	CLK_LIST(ln_bb_a_clk),
	CLK_LIST(mcd_ce1_clk),
	CLK_LIST(mcd_ce2_clk),
	CLK_LIST(mcd_ce3_clk),
	CLK_LIST(mmssnoc_ahb_clk),
	CLK_LIST(mmssnoc_ahb_a_clk),
	CLK_LIST(mss_cfg_ahb_clk),
	CLK_LIST(mss_cfg_ahb_a_clk),
	CLK_LIST(ocmemgx_core_clk),
	CLK_LIST(ocmemgx_msmbus_clk),
	CLK_LIST(ocmemgx_msmbus_a_clk),
	CLK_LIST(oxili_gfx3d_clk_src),
	CLK_LIST(pnoc_keepalive_a_clk),
	CLK_LIST(pnoc_msmbus_clk),
	CLK_LIST(pnoc_msmbus_a_clk),
	CLK_LIST(pnoc_pm_clk),
	CLK_LIST(pnoc_sps_clk),
	CLK_LIST(qcedev_ce1_clk),
	CLK_LIST(qcedev_ce2_clk),
	CLK_LIST(qcedev_ce3_clk),
	CLK_LIST(qcrypto_ce1_clk),
	CLK_LIST(qcrypto_ce2_clk),
	CLK_LIST(qcrypto_ce3_clk),
	CLK_LIST(qdss_clk),
	CLK_LIST(qdss_a_clk),
	CLK_LIST(qseecom_ce1_clk),
	CLK_LIST(qseecom_ce2_clk),
	CLK_LIST(qseecom_ce3_clk),
	CLK_LIST(rf_clk1),
	CLK_LIST(rf_clk1_ao),
	CLK_LIST(rf_clk1_pin),
	CLK_LIST(rf_clk1_pin_ao),
	CLK_LIST(rf_clk2),
	CLK_LIST(rf_clk2_ao),
	CLK_LIST(rf_clk2_pin),
	CLK_LIST(rf_clk2_pin_ao),
	CLK_LIST(scm_ce1_clk),
	CLK_LIST(scm_ce2_clk),
	CLK_LIST(scm_ce3_clk),
	CLK_LIST(snoc_msmbus_clk),
	CLK_LIST(snoc_msmbus_a_clk),
	CLK_LIST(ce1_clk),
	CLK_LIST(gcc_ce1_ahb_m_clk),
	CLK_LIST(gcc_ce1_axi_m_clk),
	CLK_LIST(ce2_clk),
	CLK_LIST(gcc_ce2_ahb_m_clk),
	CLK_LIST(gcc_ce2_axi_m_clk),
	CLK_LIST(ce3_clk),
	CLK_LIST(gcc_ce3_ahb_m_clk),
	CLK_LIST(gcc_ce3_axi_m_clk),
	CLK_LIST(gcc_bimc_kpss_axi_m_clk),
	CLK_LIST(gcc_mmss_bimc_gfx_m_clk),
	CLK_LIST(rpm_debug_mux),
};

static int msm_rpmcc_8994_probe(struct platform_device *pdev)
{
	struct resource *res;
	int ret;

	ret = enable_rpm_scaling();
	if (ret < 0)
		return ret;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cc_base");
	if (!res) {
		dev_err(&pdev->dev, "Unable to retrieve register base.\n");
		return -ENOMEM;
	}
	virt_base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!virt_base) {
		dev_err(&pdev->dev, "Failed to map in CC registers.\n");
		return -ENOMEM;
	}

	ret = of_msm_clock_register(pdev->dev.of_node, msm_clocks_rpm_8994,
				    ARRAY_SIZE(msm_clocks_rpm_8994));
	if (ret)
		return ret;

	/*
	 * Hold an active set vote for the PNOC AHB source. Sleep set vote is 0.
	 */
	clk_set_rate(&pnoc_keepalive_a_clk.c, 19200000);
	clk_prepare_enable(&pnoc_keepalive_a_clk.c);

	/*
	 * Hold an active set vote at a rate of 40MHz for the MMSS NOC AHB
	 * source. Sleep set vote is 0.
	 */
	clk_set_rate(&mmssnoc_ahb_a_clk.c, 40000000);
	clk_prepare_enable(&mmssnoc_ahb_a_clk.c);

	dev_info(&pdev->dev, "Registered RPM clocks.\n");

	return 0;
}

static struct of_device_id msm_clock_rpm_match_table[] = {
	{ .compatible = "qcom,rpmcc-8994" },
	{}
};

static struct platform_driver msm_clock_rpm_driver = {
	.probe = msm_rpmcc_8994_probe,
	.driver = {
		.name = "qcom,rpmcc-8994",
		.of_match_table = msm_clock_rpm_match_table,
		.owner = THIS_MODULE,
	},
};

int __init msm_rpmcc_8994_init(void)
{
	return platform_driver_register(&msm_clock_rpm_driver);
}
arch_initcall(msm_rpmcc_8994_init);
