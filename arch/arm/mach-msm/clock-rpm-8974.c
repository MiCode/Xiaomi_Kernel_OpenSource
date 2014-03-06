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
#include <linux/regulator/consumer.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/clk/msm-clock-generic.h>
#include <soc/qcom/clock-local2.h>
#include <soc/qcom/clock-rpm.h>
#include <soc/qcom/clock-voter.h>
#include <soc/qcom/rpm-smd.h>

#include <dt-bindings/clock/msm-clocks-8974.h>

#include "clock.h"

#define GCC_DEBUG_CLK_CTL_REG          0x1880
#define CLOCK_FRQ_MEASURE_CTL_REG      0x1884
#define CLOCK_FRQ_MEASURE_STATUS_REG   0x1888
#define GCC_XO_DIV4_CBCR_REG           0x10C8
#define GCC_PLLTEST_PAD_CFG_REG        0x188C
#define APCS_GPLL_ENA_VOTE_REG         0x1480
#define MMSS_PLL_VOTE_APCS_REG         0x0100
#define MMSS_DEBUG_CLK_CTL_REG         0x0900
#define LPASS_DEBUG_CLK_CTL_REG        0x29000

#define RPM_MISC_CLK_TYPE	0x306b6c63
#define RPM_BUS_CLK_TYPE	0x316b6c63
#define RPM_MEM_CLK_TYPE	0x326b6c63

#define RPM_SMD_KEY_ENABLE	0x62616E45

#define CXO_ID			0x0
#define QDSS_ID			0x1

#define PNOC_ID		0x0
#define SNOC_ID		0x1
#define CNOC_ID		0x2
#define MMSSNOC_AHB_ID  0x3

#define BIMC_ID		0x0
#define OXILI_ID	0x1
#define OCMEM_ID	0x2

#define D0_ID		 1
#define D1_ID		 2
#define A0_ID		 4
#define A1_ID		 5
#define A2_ID		 6
#define DIFF_CLK_ID	 7
#define DIV_CLK1_ID	11
#define DIV_CLK2_ID	12

#define GCC_DEBUG_CLK_CTL_REG 0x1880

static void __iomem *virt_base;

DEFINE_CLK_RPM_SMD(pnoc_clk, pnoc_a_clk, RPM_BUS_CLK_TYPE, PNOC_ID, NULL);
DEFINE_CLK_RPM_SMD(snoc_clk, snoc_a_clk, RPM_BUS_CLK_TYPE, SNOC_ID, NULL);
DEFINE_CLK_RPM_SMD(cnoc_clk, cnoc_a_clk, RPM_BUS_CLK_TYPE, CNOC_ID, NULL);
DEFINE_CLK_RPM_SMD(mmssnoc_ahb_clk, mmssnoc_ahb_a_clk, RPM_BUS_CLK_TYPE,
			MMSSNOC_AHB_ID, NULL);

DEFINE_CLK_RPM_SMD(bimc_clk, bimc_a_clk, RPM_MEM_CLK_TYPE, BIMC_ID, NULL);
DEFINE_CLK_RPM_SMD(ocmemgx_clk, ocmemgx_a_clk, RPM_MEM_CLK_TYPE, OCMEM_ID,
			NULL);
DEFINE_CLK_RPM_SMD(gfx3d_clk_src, gfx3d_a_clk_src, RPM_MEM_CLK_TYPE, OXILI_ID,
			NULL);

DEFINE_CLK_RPM_SMD_BRANCH(cxo_clk_src, cxo_a_clk_src,
				RPM_MISC_CLK_TYPE, CXO_ID, 19200000);
DEFINE_CLK_RPM_SMD_QDSS(qdss_clk, qdss_a_clk, RPM_MISC_CLK_TYPE, QDSS_ID);

DEFINE_CLK_RPM_SMD_XO_BUFFER(cxo_d0, cxo_d0_a, D0_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(cxo_d1, cxo_d1_a, D1_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(cxo_a0, cxo_a0_a, A0_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(cxo_a1, cxo_a1_a, A1_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(cxo_a2, cxo_a2_a, A2_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(div_clk1, div_a_clk1, DIV_CLK1_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(div_clk2, div_a_clk2, DIV_CLK2_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER(diff_clk, diff_a_clk, DIFF_CLK_ID);

DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(cxo_d0_pin, cxo_d0_a_pin, D0_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(cxo_d1_pin, cxo_d1_a_pin, D1_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(cxo_a0_pin, cxo_a0_a_pin, A0_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(cxo_a1_pin, cxo_a1_a_pin, A1_ID);
DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(cxo_a2_pin, cxo_a2_a_pin, A2_ID);

static DEFINE_CLK_VOTER(pnoc_msmbus_clk, &pnoc_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(snoc_msmbus_clk, &snoc_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(cnoc_msmbus_clk, &cnoc_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(pnoc_msmbus_a_clk, &pnoc_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(pnoc_pm_clk, &pnoc_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(snoc_msmbus_a_clk, &snoc_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(cnoc_msmbus_a_clk, &cnoc_a_clk.c, LONG_MAX);

static DEFINE_CLK_VOTER(bimc_msmbus_clk, &bimc_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(bimc_msmbus_a_clk, &bimc_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(bimc_acpu_a_clk, &bimc_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(oxili_gfx3d_clk_src, &gfx3d_clk_src.c, LONG_MAX);
static DEFINE_CLK_VOTER(ocmemgx_msmbus_clk, &ocmemgx_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(ocmemgx_msmbus_a_clk, &ocmemgx_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(ocmemgx_core_clk, &ocmemgx_clk.c, LONG_MAX);

static DEFINE_CLK_VOTER(pnoc_keepalive_a_clk, &pnoc_a_clk.c, LONG_MAX);
static DEFINE_CLK_VOTER(pnoc_sps_clk, &pnoc_clk.c, 0);

static DEFINE_CLK_BRANCH_VOTER(cxo_gcc, &cxo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(cxo_mmss, &cxo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(cxo_otg_clk, &cxo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(cxo_pil_lpass_clk, &cxo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(cxo_pil_mss_clk, &cxo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(cxo_wlan_clk, &cxo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(cxo_pil_pronto_clk, &cxo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(cxo_dwc3_clk, &cxo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(cxo_ehci_host_clk, &cxo_clk_src.c);
static DEFINE_CLK_BRANCH_VOTER(cxo_lpm_clk, &cxo_clk_src.c);

static struct mux_clk rpm_debug_mux = {
	.ops = &mux_reg_ops,
	.offset = GCC_DEBUG_CLK_CTL_REG,
	.en_mask = BIT(16),
	.mask = 0x1FF,
	.base = &virt_base,
	MUX_SRC_LIST(
	{&cnoc_clk.c, 0x0008},
	{&pnoc_clk.c, 0x0010},
	{&snoc_clk.c, 0x0000},
	{&bimc_clk.c, 0x0155},
	),
	.c = {
		.dbg_name = "rpm_debug_mux",
		.ops = &clk_ops_gen_mux,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(rpm_debug_mux.c),
	},
};

static struct clk_lookup msm_clocks_rpm_8974[] = {
	CLK_LOOKUP_OF("rpm_debug_mux", rpm_debug_mux,
			"fc401880.qcom,cc-debug"),
	CLK_LOOKUP_OF("xo", cxo_gcc,   "fc400000.qcom,gcc"),
	CLK_LOOKUP_OF("xo", cxo_mmss, "fd8c0000.qcom,mmsscc"),
	CLK_LOOKUP_OF("mmssnoc_ahb", mmssnoc_ahb_clk, "fd8c0000.qcom,mmsscc"),

	CLK_LOOKUP_OF("xo",        cxo_otg_clk,                  "msm_otg"),
	CLK_LOOKUP_OF("xo",  cxo_pil_lpass_clk,      "fe200000.qcom,lpass"),
	CLK_LOOKUP_OF("xo",    cxo_pil_mss_clk,        "fc880000.qcom,mss"),
	CLK_LOOKUP_OF("xo",       cxo_wlan_clk, "fb000000.qcom,wcnss-wlan"),
	CLK_LOOKUP_OF("rf_clk",         cxo_a2, "fb000000.qcom,wcnss-wlan"),
	CLK_LOOKUP_OF("xo", cxo_pil_pronto_clk,     "fb21b000.qcom,pronto"),
	CLK_LOOKUP_OF("xo",       cxo_dwc3_clk,                 "msm_dwc3"),
	CLK_LOOKUP_OF("xo",  cxo_ehci_host_clk,            "msm_ehci_host"),
	CLK_LOOKUP_OF("xo",        cxo_lpm_clk,        "fc4281d0.qcom,mpm"),
	CLK_LOOKUP_OF("hfpll_src", cxo_a_clk_src,  "f9016000.qcom,clock-krait"),

	CLK_LOOKUP_OF("bus_clk",	cnoc_msmbus_clk, "msm_config_noc"),
	CLK_LOOKUP_OF("bus_a_clk",	cnoc_msmbus_a_clk, "msm_config_noc"),
	CLK_LOOKUP_OF("bus_clk",	snoc_msmbus_clk,	"msm_sys_noc"),
	CLK_LOOKUP_OF("bus_a_clk",	snoc_msmbus_a_clk,	"msm_sys_noc"),
	CLK_LOOKUP_OF("bus_clk",	pnoc_msmbus_clk, "msm_periph_noc"),
	CLK_LOOKUP_OF("bus_clk",   pnoc_pm_clk,      "pm_8x60"),
	CLK_LOOKUP_OF("bus_a_clk",	pnoc_msmbus_a_clk, "msm_periph_noc"),
	CLK_LOOKUP_OF("mem_clk",	bimc_msmbus_clk,	"msm_bimc"),
	CLK_LOOKUP_OF("mem_a_clk",	bimc_msmbus_a_clk,	"msm_bimc"),
	CLK_LOOKUP_OF("mem_clk",	bimc_acpu_a_clk,	""),
	CLK_LOOKUP_OF("ocmem_clk",	ocmemgx_msmbus_clk,	  "msm_bus"),
	CLK_LOOKUP_OF("ocmem_a_clk", ocmemgx_msmbus_a_clk, "msm_bus"),
	CLK_LOOKUP_OF("core_clk", ocmemgx_core_clk, "fdd00000.qcom,ocmem"),
	CLK_LOOKUP_OF("dfab_clk", pnoc_sps_clk, "msm_sps"),
	CLK_LOOKUP_OF("bus_clk", pnoc_keepalive_a_clk, ""),

	CLK_LOOKUP_OF("bus_clk", mmssnoc_ahb_clk, ""),
	CLK_LOOKUP_OF("gfx3d_src_clk", oxili_gfx3d_clk_src,
			"fd8c0000.qcom,mmsscc"),

	/* CoreSight clocks */
	CLK_LOOKUP_OF("core_clk", qdss_clk, "fc322000.tmc"),
	CLK_LOOKUP_OF("core_clk", qdss_clk, "fc318000.tpiu"),
	CLK_LOOKUP_OF("core_clk", qdss_clk, "fc31c000.replicator"),
	CLK_LOOKUP_OF("core_clk", qdss_clk, "fc307000.tmc"),
	CLK_LOOKUP_OF("core_clk", qdss_clk, "fc31b000.funnel"),
	CLK_LOOKUP_OF("core_clk", qdss_clk, "fc319000.funnel"),
	CLK_LOOKUP_OF("core_clk", qdss_clk, "fc31a000.funnel"),
	CLK_LOOKUP_OF("core_clk", qdss_clk, "fc345000.funnel"),
	CLK_LOOKUP_OF("core_clk", qdss_clk, "fc364000.funnel"),
	CLK_LOOKUP_OF("core_clk", qdss_clk, "fc321000.stm"),
	CLK_LOOKUP_OF("core_clk", qdss_clk, "fc33c000.etm"),
	CLK_LOOKUP_OF("core_clk", qdss_clk, "fc33d000.etm"),
	CLK_LOOKUP_OF("core_clk", qdss_clk, "fc33e000.etm"),
	CLK_LOOKUP_OF("core_clk", qdss_clk, "fc33f000.etm"),
	CLK_LOOKUP_OF("core_clk", qdss_clk, "fc308000.cti"),
	CLK_LOOKUP_OF("core_clk", qdss_clk, "fc309000.cti"),
	CLK_LOOKUP_OF("core_clk", qdss_clk, "fc30a000.cti"),
	CLK_LOOKUP_OF("core_clk", qdss_clk, "fc30b000.cti"),
	CLK_LOOKUP_OF("core_clk", qdss_clk, "fc30c000.cti"),
	CLK_LOOKUP_OF("core_clk", qdss_clk, "fc30d000.cti"),
	CLK_LOOKUP_OF("core_clk", qdss_clk, "fc30e000.cti"),
	CLK_LOOKUP_OF("core_clk", qdss_clk, "fc30f000.cti"),
	CLK_LOOKUP_OF("core_clk", qdss_clk, "fc310000.cti"),
	CLK_LOOKUP_OF("core_clk", qdss_clk, "fc340000.cti"),
	CLK_LOOKUP_OF("core_clk", qdss_clk, "fc341000.cti"),
	CLK_LOOKUP_OF("core_clk", qdss_clk, "fc342000.cti"),
	CLK_LOOKUP_OF("core_clk", qdss_clk, "fc343000.cti"),
	CLK_LOOKUP_OF("core_clk", qdss_clk, "fc344000.cti"),
	CLK_LOOKUP_OF("core_clk", qdss_clk, "fc348000.cti"),
	CLK_LOOKUP_OF("core_clk", qdss_clk, "fc34d000.cti"),
	CLK_LOOKUP_OF("core_clk", qdss_clk, "fc350000.cti"),
	CLK_LOOKUP_OF("core_clk", qdss_clk, "fc354000.cti"),
	CLK_LOOKUP_OF("core_clk", qdss_clk, "fc358000.cti"),
	CLK_LOOKUP_OF("core_clk", qdss_clk, "fdf30018.hwevent"),

	CLK_LOOKUP_OF("core_a_clk", qdss_a_clk, "fc322000.tmc"),
	CLK_LOOKUP_OF("core_a_clk", qdss_a_clk, "fc318000.tpiu"),
	CLK_LOOKUP_OF("core_a_clk", qdss_a_clk, "fc31c000.replicator"),
	CLK_LOOKUP_OF("core_a_clk", qdss_a_clk, "fc307000.tmc"),
	CLK_LOOKUP_OF("core_a_clk", qdss_a_clk, "fc31b000.funnel"),
	CLK_LOOKUP_OF("core_a_clk", qdss_a_clk, "fc319000.funnel"),
	CLK_LOOKUP_OF("core_a_clk", qdss_a_clk, "fc31a000.funnel"),
	CLK_LOOKUP_OF("core_a_clk", qdss_a_clk, "fc345000.funnel"),
	CLK_LOOKUP_OF("core_a_clk", qdss_a_clk, "fc364000.funnel"),
	CLK_LOOKUP_OF("core_a_clk", qdss_a_clk, "fc321000.stm"),
	CLK_LOOKUP_OF("core_a_clk", qdss_a_clk, "fc33c000.etm"),
	CLK_LOOKUP_OF("core_a_clk", qdss_a_clk, "fc33d000.etm"),
	CLK_LOOKUP_OF("core_a_clk", qdss_a_clk, "fc33e000.etm"),
	CLK_LOOKUP_OF("core_a_clk", qdss_a_clk, "fc33f000.etm"),
	CLK_LOOKUP_OF("core_a_clk", qdss_a_clk, "fc308000.cti"),
	CLK_LOOKUP_OF("core_a_clk", qdss_a_clk, "fc309000.cti"),
	CLK_LOOKUP_OF("core_a_clk", qdss_a_clk, "fc30a000.cti"),
	CLK_LOOKUP_OF("core_a_clk", qdss_a_clk, "fc30b000.cti"),
	CLK_LOOKUP_OF("core_a_clk", qdss_a_clk, "fc30c000.cti"),
	CLK_LOOKUP_OF("core_a_clk", qdss_a_clk, "fc30d000.cti"),
	CLK_LOOKUP_OF("core_a_clk", qdss_a_clk, "fc30e000.cti"),
	CLK_LOOKUP_OF("core_a_clk", qdss_a_clk, "fc30f000.cti"),
	CLK_LOOKUP_OF("core_a_clk", qdss_a_clk, "fc310000.cti"),
	CLK_LOOKUP_OF("core_a_clk", qdss_a_clk, "fc340000.cti"),
	CLK_LOOKUP_OF("core_a_clk", qdss_a_clk, "fc341000.cti"),
	CLK_LOOKUP_OF("core_a_clk", qdss_a_clk, "fc342000.cti"),
	CLK_LOOKUP_OF("core_a_clk", qdss_a_clk, "fc343000.cti"),
	CLK_LOOKUP_OF("core_a_clk", qdss_a_clk, "fc344000.cti"),
	CLK_LOOKUP_OF("core_a_clk", qdss_a_clk, "fc348000.cti"),
	CLK_LOOKUP_OF("core_a_clk", qdss_a_clk, "fc34d000.cti"),
	CLK_LOOKUP_OF("core_a_clk", qdss_a_clk, "fc350000.cti"),
	CLK_LOOKUP_OF("core_a_clk", qdss_a_clk, "fc354000.cti"),
	CLK_LOOKUP_OF("core_a_clk", qdss_a_clk, "fc358000.cti"),
	CLK_LOOKUP_OF("core_a_clk", qdss_a_clk, "fdf30018.hwevent"),

	CLK_LOOKUP_OF("ref_clk", diff_clk, "msm_dwc3"),
	CLK_LOOKUP_OF("pwm_clk", div_clk2, "0-0048"),
	CLK_LOOKUP_OF("ref_clk", diff_clk, "msm_dwc3"),
	CLK_LOOKUP_OF("osr_clk", div_clk1, "msm-dai-q6-dev.16384"),
	CLK_LOOKUP_OF("ref_clk", div_clk2, "msm_smsc_hub"),

	CLK_LOOKUP_OF("bus_clk", snoc_clk, ""),
	CLK_LOOKUP_OF("bus_clk", pnoc_clk, ""),
	CLK_LOOKUP_OF("bus_clk", cnoc_clk, ""),
	CLK_LOOKUP_OF("mem_clk", bimc_clk, ""),
	CLK_LOOKUP_OF("mem_clk", ocmemgx_clk, ""),
	CLK_LOOKUP_OF("bus_clk", snoc_a_clk, ""),
	CLK_LOOKUP_OF("bus_clk", pnoc_a_clk, ""),
	CLK_LOOKUP_OF("bus_clk", cnoc_a_clk, ""),
	CLK_LOOKUP_OF("mem_clk", bimc_a_clk, ""),
	CLK_LOOKUP_OF("mem_clk", ocmemgx_a_clk, ""),

	CLK_LOOKUP_OF("bus_clk",	cnoc_msmbus_clk, "msm_config_noc"),
	CLK_LOOKUP_OF("bus_a_clk",	cnoc_msmbus_a_clk, "msm_config_noc"),
	CLK_LOOKUP_OF("bus_clk",	snoc_msmbus_clk, "msm_sys_noc"),
	CLK_LOOKUP_OF("bus_a_clk",	snoc_msmbus_a_clk, "msm_sys_noc"),
	CLK_LOOKUP_OF("bus_clk",	pnoc_msmbus_clk, "msm_periph_noc"),
	CLK_LOOKUP_OF("bus_clk",   pnoc_pm_clk,      "pm_8x60"),
	CLK_LOOKUP_OF("bus_a_clk",	pnoc_msmbus_a_clk, "msm_periph_noc"),
	CLK_LOOKUP_OF("mem_clk",	bimc_msmbus_clk,	"msm_bimc"),
	CLK_LOOKUP_OF("mem_a_clk",	bimc_msmbus_a_clk,	"msm_bimc"),
	CLK_LOOKUP_OF("mem_clk",	bimc_acpu_a_clk,	""),
	CLK_LOOKUP_OF("ocmem_clk",	ocmemgx_msmbus_clk,	  "msm_bus"),
	CLK_LOOKUP_OF("ocmem_a_clk", ocmemgx_msmbus_a_clk, "msm_bus"),

	CLK_LOOKUP_OF("ref_clk",    cxo_d1_a_pin, "3-000e"),
	CLK_LOOKUP_OF("ref_clk_rf", cxo_a2_a_pin, "3-000e"),
};

static struct of_device_id msm_clock_rpm_match_table[] = {
	{ .compatible = "qcom,rpmcc-8974" },
	{}
};

static int msm_rpmcc_8974_probe(struct platform_device *pdev)
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

	ret = of_msm_clock_register(pdev->dev.of_node, msm_clocks_rpm_8974,
			   ARRAY_SIZE(msm_clocks_rpm_8974));
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

	/*
	 * Hold an active set vote for CXO; this is because CXO is expected
	 * to remain on whenever CPUs aren't power collapsed.
	 */
	clk_prepare_enable(&cxo_a_clk_src.c);

	dev_info(&pdev->dev, "Registered RPM clocks.\n");

	return 0;
}

static struct platform_driver msm_clock_rpm_driver = {
	.probe = msm_rpmcc_8974_probe,
	.driver = {
		.name = "qcom,rpmcc-8974",
		.of_match_table = msm_clock_rpm_match_table,
		.owner = THIS_MODULE,
	},
};

int __init msm_rpmcc_8974_init(void)
{
	return platform_driver_register(&msm_clock_rpm_driver);
}
arch_initcall(msm_rpmcc_8974_init);
