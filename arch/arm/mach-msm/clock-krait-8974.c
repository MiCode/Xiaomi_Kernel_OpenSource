/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/ctype.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/of.h>
#include <linux/cpumask.h>

#include <asm/cputype.h>

#include <mach/rpm-regulator-smd.h>
#include <mach/clk-provider.h>
#include <mach/clock-generic.h>
#include <mach/clk.h>
#include "clock-krait.h"
#include "clock.h"

/* Clock inputs coming into Krait subsystem */
DEFINE_FIXED_DIV_CLK(hfpll_src_clk, 1, NULL);
DEFINE_FIXED_DIV_CLK(acpu_aux_clk, 2, NULL);

static int hfpll_uv[] = {
	RPM_REGULATOR_CORNER_NONE, 0,
	RPM_REGULATOR_CORNER_SVS_SOC, 1800000,
	RPM_REGULATOR_CORNER_NORMAL, 1800000,
	RPM_REGULATOR_CORNER_SUPER_TURBO, 1800000,
};
static DEFINE_VDD_REGULATORS(vdd_hfpll, ARRAY_SIZE(hfpll_uv)/2, 2,
				hfpll_uv, NULL);

static unsigned long hfpll_fmax[] = { 0, 998400000, 1996800000, 2900000000UL };

static struct hfpll_data hdata = {
	.mode_offset = 0x0,
	.l_offset = 0x4,
	.m_offset = 0x8,
	.n_offset = 0xC,
	.user_offset = 0x10,
	.config_offset = 0x14,
	.status_offset = 0x1C,

	.user_val = 0x8,
	.low_vco_max_rate = 1248000000,
	.min_rate = 537600000UL,
	.max_rate = 2900000000UL,
};

static struct hfpll_clk hfpll0_clk = {
	.d = &hdata,
	.src_rate = 19200000,
	.c = {
		.parent = &hfpll_src_clk.c,
		.dbg_name = "hfpll0_clk",
		.ops = &clk_ops_hfpll,
		.vdd_class = &vdd_hfpll,
		.fmax = hfpll_fmax,
		.num_fmax = ARRAY_SIZE(hfpll_fmax),
		CLK_INIT(hfpll0_clk.c),
	},
};

DEFINE_KPSS_DIV2_CLK(hfpll0_div_clk, &hfpll0_clk.c, 0x4501, true);

static struct hfpll_clk hfpll1_clk = {
	.d = &hdata,
	.src_rate = 19200000,
	.c = {
		.parent = &hfpll_src_clk.c,
		.dbg_name = "hfpll1_clk",
		.ops = &clk_ops_hfpll,
		.vdd_class = &vdd_hfpll,
		.fmax = hfpll_fmax,
		.num_fmax = ARRAY_SIZE(hfpll_fmax),
		CLK_INIT(hfpll1_clk.c),
	},
};

DEFINE_KPSS_DIV2_CLK(hfpll1_div_clk, &hfpll1_clk.c, 0x5501, true);

static struct hfpll_clk hfpll2_clk = {
	.d = &hdata,
	.src_rate = 19200000,
	.c = {
		.parent = &hfpll_src_clk.c,
		.dbg_name = "hfpll2_clk",
		.ops = &clk_ops_hfpll,
		.vdd_class = &vdd_hfpll,
		.fmax = hfpll_fmax,
		.num_fmax = ARRAY_SIZE(hfpll_fmax),
		CLK_INIT(hfpll2_clk.c),
	},
};

DEFINE_KPSS_DIV2_CLK(hfpll2_div_clk, &hfpll2_clk.c, 0x6501, true);

static struct hfpll_clk hfpll3_clk = {
	.d = &hdata,
	.src_rate = 19200000,
	.c = {
		.parent = &hfpll_src_clk.c,
		.dbg_name = "hfpll3_clk",
		.ops = &clk_ops_hfpll,
		.vdd_class = &vdd_hfpll,
		.fmax = hfpll_fmax,
		.num_fmax = ARRAY_SIZE(hfpll_fmax),
		CLK_INIT(hfpll3_clk.c),
	},
};

DEFINE_KPSS_DIV2_CLK(hfpll3_div_clk, &hfpll3_clk.c, 0x7501, true);

static struct hfpll_clk hfpll_l2_clk = {
	.d = &hdata,
	.src_rate = 19200000,
	.c = {
		.parent = &hfpll_src_clk.c,
		.dbg_name = "hfpll_l2_clk",
		.ops = &clk_ops_hfpll,
		.vdd_class = &vdd_hfpll,
		.fmax = hfpll_fmax,
		.num_fmax = ARRAY_SIZE(hfpll_fmax),
		CLK_INIT(hfpll_l2_clk.c),
	},
};

DEFINE_KPSS_DIV2_CLK(hfpll_l2_div_clk, &hfpll_l2_clk.c, 0x500, false);

#define SEC_MUX_COMMON_DATA		\
	.safe_parent = &acpu_aux_clk.c,	\
	.ops = &clk_mux_ops_kpss,	\
	.mask = 0x3,			\
	.shift = 2,			\
	MUX_SRC_LIST(			\
		{&acpu_aux_clk.c, 2},	\
		{NULL /* QSB */, 0},	\
	)

static struct mux_clk krait0_sec_mux_clk = {
	.offset = 0x4501,
	.priv = (void *) true,
	SEC_MUX_COMMON_DATA,
	.c = {
		.dbg_name = "krait0_sec_mux_clk",
		.ops = &clk_ops_gen_mux,
		CLK_INIT(krait0_sec_mux_clk.c),
	},
};

static struct mux_clk krait1_sec_mux_clk = {
	.offset = 0x5501,
	.priv = (void *) true,
	SEC_MUX_COMMON_DATA,
	.c = {
		.dbg_name = "krait1_sec_mux_clk",
		.ops = &clk_ops_gen_mux,
		CLK_INIT(krait1_sec_mux_clk.c),
	},
};

static struct mux_clk krait2_sec_mux_clk = {
	.offset = 0x6501,
	.priv = (void *) true,
	SEC_MUX_COMMON_DATA,
	.c = {
		.dbg_name = "krait2_sec_mux_clk",
		.ops = &clk_ops_gen_mux,
		CLK_INIT(krait2_sec_mux_clk.c),
	},
};

static struct mux_clk krait3_sec_mux_clk = {
	.offset = 0x7501,
	.priv = (void *) true,
	SEC_MUX_COMMON_DATA,
	.c = {
		.dbg_name = "krait3_sec_mux_clk",
		.ops = &clk_ops_gen_mux,
		CLK_INIT(krait3_sec_mux_clk.c),
	},
};

static struct mux_clk l2_sec_mux_clk = {
	.offset = 0x500,
	SEC_MUX_COMMON_DATA,
	.c = {
		.dbg_name = "l2_sec_mux_clk",
		.ops = &clk_ops_gen_mux,
		CLK_INIT(l2_sec_mux_clk.c),
	},
};

#define PRI_MUX_COMMON_DATA		\
	.ops = &clk_mux_ops_kpss,	\
	.mask = 0x3,			\
	.shift = 0

static struct mux_clk krait0_pri_mux_clk = {
	.offset = 0x4501,
	.priv = (void *) true,
	MUX_SRC_LIST(
		{ &hfpll0_clk.c, 1 },
		{ &hfpll0_div_clk.c, 2 },
		{ &krait0_sec_mux_clk.c, 0 },
	),
	.safe_parent = &krait0_sec_mux_clk.c,
	PRI_MUX_COMMON_DATA,
	.c = {
		.dbg_name = "krait0_pri_mux_clk",
		.ops = &clk_ops_gen_mux,
		CLK_INIT(krait0_pri_mux_clk.c),
	},
};

static struct mux_clk krait1_pri_mux_clk = {
	.offset = 0x5501,
	.priv = (void *) true,
	MUX_SRC_LIST(
		{ &hfpll1_clk.c, 1 },
		{ &hfpll1_div_clk.c, 2 },
		{ &krait1_sec_mux_clk.c, 0 },
	),
	.safe_parent = &krait1_sec_mux_clk.c,
	PRI_MUX_COMMON_DATA,
	.c = {
		.dbg_name = "krait1_pri_mux_clk",
		.ops = &clk_ops_gen_mux,
		CLK_INIT(krait1_pri_mux_clk.c),
	},
};

static struct mux_clk krait2_pri_mux_clk = {
	.offset = 0x6501,
	.priv = (void *) true,
	MUX_SRC_LIST(
		{ &hfpll2_clk.c, 1 },
		{ &hfpll2_div_clk.c, 2 },
		{ &krait2_sec_mux_clk.c, 0 },
	),
	.safe_parent = &krait2_sec_mux_clk.c,
	PRI_MUX_COMMON_DATA,
	.c = {
		.dbg_name = "krait2_pri_mux_clk",
		.ops = &clk_ops_gen_mux,
		CLK_INIT(krait2_pri_mux_clk.c),
	},
};

static struct mux_clk krait3_pri_mux_clk = {
	.offset = 0x7501,
	.priv = (void *) true,
	MUX_SRC_LIST(
		{ &hfpll3_clk.c, 1 },
		{ &hfpll3_div_clk.c, 2 },
		{ &krait3_sec_mux_clk.c, 0 },
	),
	.safe_parent = &krait3_sec_mux_clk.c,
	PRI_MUX_COMMON_DATA,
	.c = {
		.dbg_name = "krait3_pri_mux_clk",
		.ops = &clk_ops_gen_mux,
		CLK_INIT(krait3_pri_mux_clk.c),
	},
};

static struct mux_clk l2_pri_mux_clk = {
	.offset = 0x500,
	MUX_SRC_LIST(
		{&hfpll_l2_clk.c, 1 },
		{&hfpll_l2_div_clk.c, 2},
		{&l2_sec_mux_clk.c, 0}
	),
	.safe_parent = &l2_sec_mux_clk.c,
	PRI_MUX_COMMON_DATA,
	.c = {
		.dbg_name = "l2_pri_mux_clk",
		.ops = &clk_ops_gen_mux,
		CLK_INIT(l2_pri_mux_clk.c),
	},
};

static struct avs_data avs_table;

static DEFINE_VDD_REGS_INIT(vdd_krait0, 1);
static DEFINE_VDD_REGS_INIT(vdd_krait1, 1);
static DEFINE_VDD_REGS_INIT(vdd_krait2, 1);
static DEFINE_VDD_REGS_INIT(vdd_krait3, 1);
static DEFINE_VDD_REGS_INIT(vdd_l2, 1);

/*
 * This clock is mostly a dummy clock in the sense it can't really gate the
 * CPU/L2 clocks or affect their frequency. It exists solely to:
 *
 * - Capture the PVS requirements for each CPU.
 * - Implement HW clock gating disable ops needed for measuring the freq of
 *   Krait/L2 properly.
 * - Implement AVS requirement.
 */
struct kpss_core_clk krait0_clk = {
	.id	= 0,
	.avs_tbl = &avs_table,
	.c = {
		.parent = &krait0_pri_mux_clk.c,
		.dbg_name = "krait0_clk",
		.ops = &clk_ops_kpss_cpu,
		.vdd_class = &vdd_krait0,
		CLK_INIT(krait0_clk.c),
	},
};

struct kpss_core_clk krait1_clk = {
	.id	= 1,
	.avs_tbl = &avs_table,
	.c = {
		.parent = &krait1_pri_mux_clk.c,
		.dbg_name = "krait1_clk",
		.ops = &clk_ops_kpss_cpu,
		.vdd_class = &vdd_krait1,
		CLK_INIT(krait1_clk.c),
	},
};

struct kpss_core_clk krait2_clk = {
	.id	= 2,
	.avs_tbl = &avs_table,
	.c = {
		.parent = &krait2_pri_mux_clk.c,
		.dbg_name = "krait2_clk",
		.ops = &clk_ops_kpss_cpu,
		.vdd_class = &vdd_krait2,
		CLK_INIT(krait2_clk.c),
	},
};

struct kpss_core_clk krait3_clk = {
	.id	= 3,
	.avs_tbl = &avs_table,
	.c = {
		.parent = &krait3_pri_mux_clk.c,
		.dbg_name = "krait3_clk",
		.ops = &clk_ops_kpss_cpu,
		.vdd_class = &vdd_krait3,
		CLK_INIT(krait3_clk.c),
	},
};

struct kpss_core_clk l2_clk = {
	.cp15_iaddr = 0x0500,
	.c = {
		.parent = &l2_pri_mux_clk.c,
		.dbg_name = "l2_clk",
		.ops = &clk_ops_kpss_l2,
		.vdd_class = &vdd_l2,
		CLK_INIT(l2_clk.c),
	},
};

static struct clk_lookup kpss_clocks_8974[] = {
	CLK_LOOKUP("",	hfpll_src_clk.c,	""),
	CLK_LOOKUP("",	acpu_aux_clk.c,		""),
	CLK_LOOKUP("",	hfpll0_clk.c,		""),
	CLK_LOOKUP("",	hfpll0_div_clk.c,	""),
	CLK_LOOKUP("",	hfpll0_clk.c,		""),
	CLK_LOOKUP("",	hfpll1_div_clk.c,	""),
	CLK_LOOKUP("",	hfpll1_clk.c,		""),
	CLK_LOOKUP("",	hfpll2_div_clk.c,	""),
	CLK_LOOKUP("",	hfpll2_clk.c,		""),
	CLK_LOOKUP("",	hfpll3_div_clk.c,	""),
	CLK_LOOKUP("",	hfpll3_clk.c,		""),
	CLK_LOOKUP("",	hfpll_l2_div_clk.c,	""),
	CLK_LOOKUP("",	hfpll_l2_clk.c,		""),
	CLK_LOOKUP("",	krait0_sec_mux_clk.c,		""),
	CLK_LOOKUP("",	krait1_sec_mux_clk.c,		""),
	CLK_LOOKUP("",	krait2_sec_mux_clk.c,		""),
	CLK_LOOKUP("",	krait3_sec_mux_clk.c,		""),
	CLK_LOOKUP("",	l2_sec_mux_clk.c,		""),
	CLK_LOOKUP("",	krait0_pri_mux_clk.c,		""),
	CLK_LOOKUP("",	krait1_pri_mux_clk.c,		""),
	CLK_LOOKUP("",	krait2_pri_mux_clk.c,		""),
	CLK_LOOKUP("",	krait3_pri_mux_clk.c,		""),
	CLK_LOOKUP("",	l2_pri_mux_clk.c,		""),
	CLK_LOOKUP("l2_clk",	l2_clk.c,     "0.qcom,msm-cpufreq"),
	CLK_LOOKUP("cpu0_clk",	krait0_clk.c, "0.qcom,msm-cpufreq"),
	CLK_LOOKUP("cpu1_clk",	krait1_clk.c, "0.qcom,msm-cpufreq"),
	CLK_LOOKUP("cpu2_clk",	krait2_clk.c, "0.qcom,msm-cpufreq"),
	CLK_LOOKUP("cpu3_clk",	krait3_clk.c, "0.qcom,msm-cpufreq"),
	CLK_LOOKUP("l2_clk",	l2_clk.c,     "fe805664.qcom,pm-8x60"),
	CLK_LOOKUP("cpu0_clk",	krait0_clk.c, "fe805664.qcom,pm-8x60"),
	CLK_LOOKUP("cpu1_clk",	krait1_clk.c, "fe805664.qcom,pm-8x60"),
	CLK_LOOKUP("cpu2_clk",	krait2_clk.c, "fe805664.qcom,pm-8x60"),
	CLK_LOOKUP("cpu3_clk",	krait3_clk.c, "fe805664.qcom,pm-8x60"),
};

static struct clk *cpu_clk[] = {
	&krait0_clk.c,
	&krait1_clk.c,
	&krait2_clk.c,
	&krait3_clk.c,
};

static void get_krait_bin_format_b(struct platform_device *pdev,
					int *speed, int *pvs, int *ver)
{
	u32 pte_efuse, redundant_sel;
	struct resource *res;
	void __iomem *base;

	*speed = 0;
	*pvs = 0;
	*ver = 0;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "efuse");
	if (!res) {
		dev_info(&pdev->dev,
			 "No speed/PVS binning available. Defaulting to 0!\n");
		return;
	}

	base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!base) {
		dev_warn(&pdev->dev,
			 "Unable to read efuse data. Defaulting to 0!\n");
		return;
	}

	pte_efuse = readl_relaxed(base);
	redundant_sel = (pte_efuse >> 24) & 0x7;
	*speed = pte_efuse & 0x7;
	/* 4 bits of PVS are in efuse register bits 31, 8-6. */
	*pvs = ((pte_efuse >> 28) & 0x8) | ((pte_efuse >> 6) & 0x7);
	*ver = (pte_efuse >> 4) & 0x3;

	switch (redundant_sel) {
	case 1:
		*speed = (pte_efuse >> 27) & 0xF;
		break;
	case 2:
		*pvs = (pte_efuse >> 27) & 0xF;
		break;
	}

	/* Check SPEED_BIN_BLOW_STATUS */
	if (pte_efuse & BIT(3)) {
		dev_info(&pdev->dev, "Speed bin: %d\n", *speed);
	} else {
		dev_warn(&pdev->dev, "Speed bin not set. Defaulting to 0!\n");
		*speed = 0;
	}

	/* Check PVS_BLOW_STATUS */
	pte_efuse = readl_relaxed(base + 0x4) & BIT(21);
	if (pte_efuse) {
		dev_info(&pdev->dev, "PVS bin: %d\n", *pvs);
	} else {
		dev_warn(&pdev->dev, "PVS bin not set. Defaulting to 0!\n");
		*pvs = 0;
	}

	dev_info(&pdev->dev, "PVS version: %d\n", *ver);

	devm_iounmap(&pdev->dev, base);
}

static int parse_tbl(struct device *dev, char *prop, int num_cols,
		u32 **col1, u32 **col2, u32 **col3)
{
	int ret, prop_len, num_rows, i, j, k;
	u32 *prop_data;
	u32 *col[num_cols];

	if (!of_find_property(dev->of_node, prop, &prop_len))
		return -EINVAL;

	prop_len /= sizeof(*prop_data);

	if (prop_len % num_cols || prop_len == 0)
		return -EINVAL;

	num_rows = prop_len / num_cols;

	prop_data = devm_kzalloc(dev, prop_len * sizeof(*prop_data),
				 GFP_KERNEL);
	if (!prop_data)
		return -ENOMEM;

	for (i = 0; i < num_cols; i++) {
		col[i] = devm_kzalloc(dev, num_rows * sizeof(u32), GFP_KERNEL);
		if (!col[i])
			return -ENOMEM;
	}

	ret = of_property_read_u32_array(dev->of_node, prop, prop_data,
					 prop_len);
	if (ret)
		return ret;

	k = 0;
	for (i = 0; i < num_rows; i++) {
		for (j = 0; j < num_cols; j++)
			col[j][i] = prop_data[k++];
	}
	if (col1)
		*col1 = col[0];
	if (col2)
		*col2 = col[1];
	if (col3)
		*col3 = col[2];

	devm_kfree(dev, prop_data);

	return num_rows;
}

static int clk_init_vdd_class(struct device *dev, struct clk *clk, int num,
				 unsigned long *fmax, int *uv, int *ua)
{
	struct clk_vdd_class *vdd = clk->vdd_class;

	vdd->level_votes = devm_kzalloc(dev, num * sizeof(int), GFP_KERNEL);
	if (!vdd->level_votes) {
		dev_err(dev, "Out of memory!\n");
		return -ENOMEM;
	}
	vdd->num_levels = num;
	vdd->cur_level = num;
	vdd->vdd_uv = uv;
	vdd->vdd_ua = ua;
	clk->fmax = fmax;
	clk->num_fmax = num;

	return 0;
}

static int hfpll_base_init(struct platform_device *pdev, struct hfpll_clk *h)
{
	struct resource *res;
	struct device *dev = &pdev->dev;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, h->c.dbg_name);
	if (!res) {
		dev_err(dev, "%s base addr not found!\n", h->c.dbg_name);
		return -EINVAL;
	}
	h->base = devm_ioremap(dev, res->start, resource_size(res));
	if (!h->base) {
		dev_err(dev, "%s ioremap failed!\n", h->c.dbg_name);
		return -ENOMEM;
	}
	return 0;
}

static bool enable_boost;
module_param_named(boost, enable_boost, bool, S_IRUGO | S_IWUSR);

static void krait_update_uv(int *uv, int num, int boost_uv)
{
	int i;

	switch (read_cpuid_id()) {
	case 0x511F04D0: /* KR28M2A20 */
	case 0x511F04D1: /* KR28M2A21 */
	case 0x510F06F0: /* KR28M4A10 */
		for (i = 0; i < num; i++)
			uv[i] = max(1150000, uv[i]);
	};

	if (enable_boost) {
		for (i = 0; i < num; i++)
			uv[i] += boost_uv;
	}
}

static int clock_krait_8974_driver_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct clk *c;
	int speed, pvs, ver, rows, cpu;
	char prop_name[] = "qcom,speedXX-pvsXX-bin-vXX";
	unsigned long *freq, cur_rate, aux_rate;
	int *uv, *ua;
	u32 *dscr, vco_mask, config_val;
	int ret;

	vdd_l2.regulator[0] = devm_regulator_get(dev, "l2-dig");
	if (IS_ERR(vdd_l2.regulator[0])) {
		dev_err(dev, "Unable to get l2-dig regulator!\n");
		return PTR_ERR(vdd_l2.regulator[0]);
	}

	vdd_hfpll.regulator[0] = devm_regulator_get(dev, "hfpll-dig");
	if (IS_ERR(vdd_hfpll.regulator[0])) {
		dev_err(dev, "Unable to get hfpll-dig regulator!\n");
		return PTR_ERR(vdd_hfpll.regulator[0]);
	}

	vdd_hfpll.regulator[1] = devm_regulator_get(dev, "hfpll-analog");
	if (IS_ERR(vdd_hfpll.regulator[1])) {
		dev_err(dev, "Unable to get hfpll-analog regulator!\n");
		return PTR_ERR(vdd_hfpll.regulator[1]);
	}

	vdd_krait0.regulator[0] = devm_regulator_get(dev, "cpu0");
	if (IS_ERR(vdd_krait0.regulator[0])) {
		dev_err(dev, "Unable to get cpu0 regulator!\n");
		return PTR_ERR(vdd_krait0.regulator[0]);
	}

	vdd_krait1.regulator[0] = devm_regulator_get(dev, "cpu1");
	if (IS_ERR(vdd_krait1.regulator[0])) {
		dev_err(dev, "Unable to get cpu1 regulator!\n");
		return PTR_ERR(vdd_krait1.regulator[0]);
	}

	vdd_krait2.regulator[0] = devm_regulator_get(dev, "cpu2");
	if (IS_ERR(vdd_krait2.regulator[0])) {
		dev_err(dev, "Unable to get cpu2 regulator!\n");
		return PTR_ERR(vdd_krait2.regulator[0]);
	}

	vdd_krait3.regulator[0] = devm_regulator_get(dev, "cpu3");
	if (IS_ERR(vdd_krait3.regulator[0])) {
		dev_err(dev, "Unable to get cpu3 regulator!\n");
		return PTR_ERR(vdd_krait3.regulator[0]);
	}

	c = devm_clk_get(dev, "hfpll_src");
	if (IS_ERR(c)) {
		dev_err(dev, "Unable to get HFPLL source\n");
		return PTR_ERR(c);
	}
	hfpll_src_clk.c.parent = c;

	c = devm_clk_get(dev, "aux_clk");
	if (IS_ERR(c)) {
		dev_err(dev, "Unable to get AUX source\n");
		return PTR_ERR(c);
	}
	acpu_aux_clk.c.parent = c;

	if (hfpll_base_init(pdev, &hfpll0_clk))
		return -EINVAL;
	if (hfpll_base_init(pdev, &hfpll1_clk))
		return -EINVAL;
	if (hfpll_base_init(pdev, &hfpll2_clk))
		return -EINVAL;
	if (hfpll_base_init(pdev, &hfpll3_clk))
		return -EINVAL;
	if (hfpll_base_init(pdev, &hfpll_l2_clk))
		return -EINVAL;

	ret = of_property_read_u32(dev->of_node, "qcom,hfpll-config-val",
			     &config_val);
	if (!ret)
		hdata.config_val = config_val;

	ret = of_property_read_u32(dev->of_node, "qcom,hfpll-user-vco-mask",
			     &vco_mask);
	if (!ret)
		hdata.user_vco_mask = vco_mask;

	get_krait_bin_format_b(pdev, &speed, &pvs, &ver);
	snprintf(prop_name, ARRAY_SIZE(prop_name),
			"qcom,speed%d-pvs%d-bin-v%d", speed, pvs, ver);

	rows = parse_tbl(dev, prop_name, 3,
			(u32 **) &freq, (u32 **) &uv, (u32 **) &ua);
	if (rows < 0) {
		dev_err(dev, "Unable to load voltage plan %s!\n", prop_name);
		return rows;
	}

	krait_update_uv(uv, rows, pvs ? 25000 : 0);

	if (clk_init_vdd_class(dev, &krait0_clk.c, rows, freq, uv, ua))
		return -ENOMEM;
	if (clk_init_vdd_class(dev, &krait1_clk.c, rows, freq, uv, ua))
		return -ENOMEM;
	if (clk_init_vdd_class(dev, &krait2_clk.c, rows, freq, uv, ua))
		return -ENOMEM;
	if (clk_init_vdd_class(dev, &krait3_clk.c, rows, freq, uv, ua))
		return -ENOMEM;

	/* AVS is optional */
	rows = parse_tbl(dev, "qcom,avs-tbl", 2, (u32 **) &freq, &dscr, NULL);
	if (rows > 0) {
		avs_table.rate = freq;
		avs_table.dscr = dscr;
		avs_table.num  = rows;
	}

	rows = parse_tbl(dev, "qcom,l2-fmax", 2, (u32 **) &freq, (u32 **) &uv,
			 NULL);
	if (rows < 0) {
		dev_err(dev, "Unable to find L2 Fmax table!\n");
		return rows;
	}

	if (clk_init_vdd_class(dev, &l2_clk.c, rows, freq, uv, NULL))
		return -ENOMEM;

	msm_clock_register(kpss_clocks_8974, ARRAY_SIZE(kpss_clocks_8974));

	/*
	 * We don't want the CPU or L2 clocks to be turned off at late init
	 * if CPUFREQ or HOTPLUG configs are disabled. So, bump up the
	 * refcount of these clocks. Any cpufreq/hotplug manager can assume
	 * that the clocks have already been prepared and enabled by the time
	 * they take over.
	 */
	clk_prepare_enable(&l2_clk.c);
	for_each_online_cpu(cpu) {
		WARN(clk_prepare_enable(cpu_clk[cpu]),
			"Unable to turn on CPU%d clock", cpu);
	}

	/*
	 * Force reinit of HFPLLs and muxes to overwrite any potential
	 * incorrect configuration of HFPLLs and muxes by the bootloader.
	 * While at it, also make sure the cores are running at known rates
	 * and print the current rate.
	 *
	 * The clocks are set to aux clock rate first to make sure the
	 * secondary mux is not sourcing off of QSB. The rate is then set to
	 * two different rates to force a HFPLL reinit under all
	 * circumstances.
	 */
	cur_rate = clk_get_rate(&l2_clk.c);
	aux_rate = clk_get_rate(&acpu_aux_clk.c);
	if (!cur_rate) {
		pr_info("L2 @ unknown rate. Forcing new rate.\n");
		cur_rate = aux_rate;
	}
	clk_set_rate(&l2_clk.c, aux_rate);
	clk_set_rate(&l2_clk.c, clk_round_rate(&l2_clk.c, 1));
	clk_set_rate(&l2_clk.c, cur_rate);
	pr_info("L2 @ %lu KHz\n", clk_get_rate(&l2_clk.c) / 1000);
	for_each_possible_cpu(cpu) {
		struct clk *c = cpu_clk[cpu];
		cur_rate = clk_get_rate(c);
		if (!cur_rate) {
			pr_info("CPU%d @ unknown rate. Forcing new rate.\n",
				cpu);
			cur_rate = aux_rate;
		}
		clk_set_rate(c, aux_rate);
		clk_set_rate(c, clk_round_rate(c, 1));
		clk_set_rate(c, clk_round_rate(c, cur_rate));
		pr_info("CPU%d @ %lu KHz\n", cpu, clk_get_rate(c) / 1000);
	}

	return 0;
}

static struct of_device_id match_table[] = {
	{ .compatible = "qcom,clock-krait-8974" },
	{}
};

static struct platform_driver clock_krait_8974_driver = {
	.probe = clock_krait_8974_driver_probe,
	.driver = {
		.name = "clock-krait-8974",
		.of_match_table = match_table,
		.owner = THIS_MODULE,
	},
};

static int __init clock_krait_8974_init(void)
{
	return platform_driver_register(&clock_krait_8974_driver);
}
module_init(clock_krait_8974_init);

static void __exit clock_krait_8974_exit(void)
{
	platform_driver_unregister(&clock_krait_8974_driver);
}
module_exit(clock_krait_8974_exit);

MODULE_DESCRIPTION("Krait CPU clock driver for 8974");
MODULE_LICENSE("GPL v2");
