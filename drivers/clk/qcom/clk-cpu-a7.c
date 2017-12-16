/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#include <linux/cpu.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/pm_opp.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <dt-bindings/clock/qcom,cpu-a7.h>

#include "clk-alpha-pll.h"
#include "clk-debug.h"
#include "clk-rcg.h"
#include "clk-regmap-mux-div.h"
#include "common.h"
#include "vdd-level-sdm845.h"

#define SYS_APC0_AUX_CLK_SRC	1

#define PLL_MODE_REG		0x0
#define PLL_OPMODE_RUN		0x1
#define PLL_OPMODE_REG		0x38
#define PLL_MODE_OUTCTRL	BIT(0)

#define to_clk_regmap_mux_div(_hw) \
	container_of(to_clk_regmap(_hw), struct clk_regmap_mux_div, clkr)

static DEFINE_VDD_REGULATORS(vdd_cx, VDD_CX_NUM, 1, vdd_corner);
static DEFINE_VDD_REGS_INIT(vdd_cpu, 1);

enum apcs_clk_parent_index {
	XO_AO_INDEX,
	SYS_APC0_AUX_CLK_INDEX,
	APCS_CPU_PLL_INDEX,
};

enum {
	P_SYS_APC0_AUX_CLK,
	P_APCS_CPU_PLL,
	P_BI_TCXO_AO,
};

static const struct parent_map apcs_clk_parent_map[] = {
	[XO_AO_INDEX] = { P_BI_TCXO_AO, 0 },
	[SYS_APC0_AUX_CLK_INDEX] = { P_SYS_APC0_AUX_CLK, 1 },
	[APCS_CPU_PLL_INDEX] = { P_APCS_CPU_PLL, 5 },
};

static const char *const apcs_clk_parent_name[] = {
	[XO_AO_INDEX] = "bi_tcxo_ao",
	[SYS_APC0_AUX_CLK_INDEX] = "sys_apc0_aux_clk",
	[APCS_CPU_PLL_INDEX] = "apcs_cpu_pll",
};

static int a7cc_clk_set_rate_and_parent(struct clk_hw *hw, unsigned long rate,
						unsigned long prate, u8 index)
{
	struct clk_regmap_mux_div *cpuclk = to_clk_regmap_mux_div(hw);

	return __mux_div_set_src_div(cpuclk, cpuclk->parent_map[index].cfg,
					cpuclk->div);
}

static int a7cc_clk_set_parent(struct clk_hw *hw, u8 index)
{
	/*
	 * Since a7cc_clk_set_rate_and_parent() is defined and set_parent()
	 * will never gets called from clk_change_rate() so return 0.
	 */
	return 0;
}

static int a7cc_clk_set_rate(struct clk_hw *hw, unsigned long rate,
						unsigned long prate)
{
	struct clk_regmap_mux_div *cpuclk = to_clk_regmap_mux_div(hw);

	/*
	 * Parent is same as the last rate.
	 * Here just configure new div.
	 */
	return __mux_div_set_src_div(cpuclk, cpuclk->src, cpuclk->div);
}

static int a7cc_clk_determine_rate(struct clk_hw *hw,
					struct clk_rate_request *req)
{
	int ret;
	u32 div = 1;
	struct clk_hw *xo, *apc0_auxclk_hw, *apcs_cpu_pll_hw;
	unsigned long apc0_auxclk_rate, rate = req->rate;
	struct clk_rate_request parent_req = { };
	struct clk_regmap_mux_div *cpuclk = to_clk_regmap_mux_div(hw);
	unsigned long mask = BIT(cpuclk->hid_width) - 1;

	xo = clk_hw_get_parent_by_index(hw, XO_AO_INDEX);
	if (rate == clk_hw_get_rate(xo)) {
		req->best_parent_hw = xo;
		req->best_parent_rate = rate;
		cpuclk->div = div;
		cpuclk->src = cpuclk->parent_map[XO_AO_INDEX].cfg;
		return 0;
	}

	apc0_auxclk_hw = clk_hw_get_parent_by_index(hw, SYS_APC0_AUX_CLK_INDEX);
	apcs_cpu_pll_hw = clk_hw_get_parent_by_index(hw, APCS_CPU_PLL_INDEX);

	apc0_auxclk_rate = clk_hw_get_rate(apc0_auxclk_hw);
	if (rate <= apc0_auxclk_rate) {
		req->best_parent_hw = apc0_auxclk_hw;
		req->best_parent_rate = apc0_auxclk_rate;

		div = DIV_ROUND_UP((2 * req->best_parent_rate), rate) - 1;
		div = min_t(unsigned long, div, mask);

		req->rate = clk_rcg2_calc_rate(req->best_parent_rate, 0,
							0, 0, div);
		cpuclk->src = cpuclk->parent_map[SYS_APC0_AUX_CLK_INDEX].cfg;
	} else {
		parent_req.rate = rate;
		parent_req.best_parent_hw = apcs_cpu_pll_hw;

		req->best_parent_hw = apcs_cpu_pll_hw;
		ret = __clk_determine_rate(req->best_parent_hw, &parent_req);
		if (ret)
			return ret;

		req->best_parent_rate = parent_req.rate;
		cpuclk->src = cpuclk->parent_map[APCS_CPU_PLL_INDEX].cfg;
	}
	cpuclk->div = div;

	return 0;
}

static void a7cc_clk_list_registers(struct seq_file *f, struct clk_hw *hw)
{
	struct clk_regmap_mux_div *cpuclk = to_clk_regmap_mux_div(hw);
	int i = 0, size = 0, val;

	static struct clk_register_data data[] = {
		{"CMD_RCGR", 0x0},
		{"CFG_RCGR", 0x4},
	};

	size = ARRAY_SIZE(data);
	for (i = 0; i < size; i++) {
		regmap_read(cpuclk->clkr.regmap,
				cpuclk->reg_offset + data[i].offset, &val);
		seq_printf(f, "%20s: 0x%.8x\n", data[i].name, val);
	}
}

static unsigned long a7cc_clk_recalc_rate(struct clk_hw *hw,
					unsigned long prate)
{
	struct clk_regmap_mux_div *cpuclk = to_clk_regmap_mux_div(hw);
	const char *name = clk_hw_get_name(hw);
	struct clk_hw *parent;
	int ret = 0;
	unsigned long parent_rate;
	u32 i, div, src = 0;
	u32 num_parents = clk_hw_get_num_parents(hw);

	ret = mux_div_get_src_div(cpuclk, &src, &div);
	if (ret)
		return ret;

	for (i = 0; i < num_parents; i++) {
		if (src == cpuclk->parent_map[i].cfg) {
			parent = clk_hw_get_parent_by_index(hw, i);
			parent_rate = clk_hw_get_rate(parent);
			return clk_rcg2_calc_rate(parent_rate, 0, 0, 0, div);
		}
	}
	pr_err("%s: Can't find parent %d\n", name, src);
	return ret;
}

static int a7cc_clk_enable(struct clk_hw *hw)
{
	return clk_regmap_mux_div_ops.enable(hw);
}

static void a7cc_clk_disable(struct clk_hw *hw)
{
	clk_regmap_mux_div_ops.disable(hw);
}

static u8 a7cc_clk_get_parent(struct clk_hw *hw)
{
	return clk_regmap_mux_div_ops.get_parent(hw);
}

/*
 * We use the notifier function for switching to a temporary safe configuration
 * (mux and divider), while the APSS pll is reconfigured.
 */
static int a7cc_notifier_cb(struct notifier_block *nb, unsigned long event,
			     void *data)
{
	int ret = 0;
	struct clk_regmap_mux_div *cpuclk = container_of(nb,
					struct clk_regmap_mux_div, clk_nb);

	if (event == PRE_RATE_CHANGE)
		/* set the mux to safe source(sys_apc0_aux_clk) & div */
		ret = __mux_div_set_src_div(cpuclk, SYS_APC0_AUX_CLK_SRC, 1);

	if (event == ABORT_RATE_CHANGE)
		pr_err("Error in configuring PLL - stay at safe src only\n");

	return notifier_from_errno(ret);
}

static const struct clk_ops a7cc_clk_ops = {
	.enable = a7cc_clk_enable,
	.disable = a7cc_clk_disable,
	.get_parent = a7cc_clk_get_parent,
	.set_rate = a7cc_clk_set_rate,
	.set_parent = a7cc_clk_set_parent,
	.set_rate_and_parent = a7cc_clk_set_rate_and_parent,
	.determine_rate = a7cc_clk_determine_rate,
	.recalc_rate = a7cc_clk_recalc_rate,
	.debug_init = clk_debug_measure_add,
	.list_registers = a7cc_clk_list_registers,
};

/*
 * As per HW, sys_apc0_aux_clk runs at 300MHz and configured by BOOT
 * So adding it as dummy clock.
 */

static struct clk_dummy sys_apc0_aux_clk = {
	.rrate = 300000000,
	.hw.init = &(struct clk_init_data){
		.name = "sys_apc0_aux_clk",
		.ops = &clk_dummy_ops,
	},
};

/* Initial configuration for 1497.6MHz(Turbo) */
static const struct pll_config apcs_cpu_pll_config = {
	.l = 0x4E,
};

static struct pll_vco trion_vco[] = {
	{ 249600000, 2000000000, 0 },
};

static struct clk_alpha_pll apcs_cpu_pll = {
	.type = TRION_PLL,
	.vco_table = trion_vco,
	.num_vco = ARRAY_SIZE(trion_vco),
	.clkr.hw.init = &(struct clk_init_data){
		.name = "apcs_cpu_pll",
		.parent_names = (const char *[]){ "bi_tcxo_ao" },
		.num_parents = 1,
		.ops = &clk_trion_pll_ops,
		VDD_CX_FMAX_MAP4(LOWER, 345600000,
				LOW, 576000000,
				NOMINAL, 1094400000,
				HIGH, 1497600000),
	},
};

static struct clk_regmap_mux_div apcs_clk = {
	.hid_width  = 5,
	.hid_shift  = 0,
	.src_width  = 3,
	.src_shift  = 8,
	.safe_src = 1,
	.safe_div = 1,
	.parent_map = apcs_clk_parent_map,
	.clk_nb.notifier_call = a7cc_notifier_cb,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "apcs_clk",
		.parent_names = apcs_clk_parent_name,
		.num_parents = 3,
		.vdd_class = &vdd_cpu,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &a7cc_clk_ops,
	},
};

static const struct of_device_id match_table[] = {
	{ .compatible = "qcom,cpu-sdxpoorwills" },
	{}
};

static const struct regmap_config cpu_regmap_config = {
	.reg_bits               = 32,
	.reg_stride             = 4,
	.val_bits               = 32,
	.max_register           = 0x7F10,
	.fast_io                = true,
};

static struct clk_hw *cpu_clks_hws[] = {
	[SYS_APC0_AUX_CLK] = &sys_apc0_aux_clk.hw,
	[APCS_CPU_PLL] = &apcs_cpu_pll.clkr.hw,
	[APCS_CLK] = &apcs_clk.clkr.hw,
};

static void a7cc_clk_get_speed_bin(struct platform_device *pdev, int *bin,
							int *version)
{
	struct resource *res;
	void __iomem *base;
	u32 pte_efuse, valid;

	*bin = 0;
	*version = 0;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "efuse");
	if (!res) {
		dev_info(&pdev->dev,
			"No speed/PVS binning available. Defaulting to 0!\n");
		return;
	}

	base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!base) {
		dev_info(&pdev->dev,
			"Unable to read efuse data. Defaulting to 0!\n");
		return;
	}

	pte_efuse = readl_relaxed(base);
	devm_iounmap(&pdev->dev, base);

	*bin = pte_efuse & 0x7;
	valid = (pte_efuse >> 3) & 0x1;
	*version = (pte_efuse >> 4) & 0x3;

	if (!valid) {
		dev_info(&pdev->dev, "Speed bin not set. Defaulting to 0!\n");
		*bin = 0;
	} else {
		dev_info(&pdev->dev, "Speed bin: %d\n", *bin);
	}

	dev_info(&pdev->dev, "PVS version: %d\n", *version);
}

static int a7cc_clk_get_fmax_vdd_class(struct platform_device *pdev,
			struct clk_init_data *clk_intd, char *prop_name)
{
	struct device_node *of = pdev->dev.of_node;
	int prop_len, i, j;
	struct clk_vdd_class *vdd = clk_intd->vdd_class;
	int num = vdd->num_regulators + 1;
	u32 *array;

	if (!of_find_property(of, prop_name, &prop_len)) {
		dev_err(&pdev->dev, "missing %s\n", prop_name);
		return -EINVAL;
	}

	prop_len /= sizeof(u32);
	if (prop_len % num) {
		dev_err(&pdev->dev, "bad length %d\n", prop_len);
		return -EINVAL;
	}

	prop_len /= num;
	vdd->level_votes = devm_kzalloc(&pdev->dev, prop_len * sizeof(int),
					GFP_KERNEL);
	if (!vdd->level_votes)
		return -ENOMEM;

	vdd->vdd_uv = devm_kzalloc(&pdev->dev,
				prop_len * sizeof(int) * (num - 1), GFP_KERNEL);
	if (!vdd->vdd_uv)
		return -ENOMEM;

	clk_intd->rate_max = devm_kzalloc(&pdev->dev,
				prop_len * sizeof(unsigned long), GFP_KERNEL);
	if (!clk_intd->rate_max)
		return -ENOMEM;

	array = devm_kzalloc(&pdev->dev,
			prop_len * sizeof(u32) * num, GFP_KERNEL);
	if (!array)
		return -ENOMEM;

	of_property_read_u32_array(of, prop_name, array, prop_len * num);
	for (i = 0; i < prop_len; i++) {
		clk_intd->rate_max[i] = array[num * i];
		for (j = 1; j < num; j++) {
			vdd->vdd_uv[(num - 1) * i + (j - 1)] =
					array[num * i + j];
		}
	}

	devm_kfree(&pdev->dev, array);
	vdd->num_levels = prop_len;
	vdd->cur_level = prop_len;
	clk_intd->num_rate_max = prop_len;

	return 0;
}

/*
 *  Find the voltage level required for a given clock rate.
 */
static int find_vdd_level(struct clk_init_data *clk_intd, unsigned long rate)
{
	int level;

	for (level = 0; level < clk_intd->num_rate_max; level++)
		if (rate <= clk_intd->rate_max[level])
			break;

	if (level == clk_intd->num_rate_max) {
		pr_err("Rate %lu for %s is greater than highest Fmax\n", rate,
				clk_intd->name);
		return -EINVAL;
	}

	return level;
}

static int
a7cc_clk_add_opp(struct clk_hw *hw, struct device *dev, unsigned long max_rate)
{
	unsigned long rate = 0;
	int level, uv, j = 1;
	long ret;
	struct clk_init_data *clk_intd =  (struct clk_init_data *)hw->init;
	struct clk_vdd_class *vdd = clk_intd->vdd_class;

	if (IS_ERR_OR_NULL(dev)) {
		pr_err("%s: Invalid parameters\n", __func__);
		return -EINVAL;
	}

	while (1) {
		rate = clk_intd->rate_max[j++];
		level = find_vdd_level(clk_intd, rate);
		if (level <= 0) {
			pr_warn("clock-cpu: no corner for %lu.\n", rate);
			return -EINVAL;
		}

		uv = vdd->vdd_uv[level];
		if (uv < 0) {
			pr_warn("clock-cpu: no uv for %lu.\n", rate);
			return -EINVAL;
		}

		ret = dev_pm_opp_add(dev, rate, uv);
		if (ret) {
			pr_warn("clock-cpu: failed to add OPP for %lu\n", rate);
			return rate;
		}

		if (rate >= max_rate)
			break;
	}

	return 0;
}

static void a7cc_clk_print_opp_table(int a7_cpu)
{
	struct dev_pm_opp *oppfmax, *oppfmin;
	unsigned long apc_fmax, apc_fmin;
	u32 max_a7ss_index = apcs_clk.clkr.hw.init->num_rate_max;

	apc_fmax = apcs_clk.clkr.hw.init->rate_max[max_a7ss_index - 1];
	apc_fmin = apcs_clk.clkr.hw.init->rate_max[1];

	rcu_read_lock();

	oppfmax = dev_pm_opp_find_freq_exact(get_cpu_device(a7_cpu),
					apc_fmax, true);
	oppfmin = dev_pm_opp_find_freq_exact(get_cpu_device(a7_cpu),
					apc_fmin, true);
	pr_info("Clock_cpu: OPP voltage for %lu: %ld\n", apc_fmin,
		dev_pm_opp_get_voltage(oppfmin));
	pr_info("Clock_cpu: OPP voltage for %lu: %ld\n", apc_fmax,
		dev_pm_opp_get_voltage(oppfmax));

	rcu_read_unlock();
}

static void a7cc_clk_populate_opp_table(struct platform_device *pdev)
{
	unsigned long apc_fmax;
	int cpu, a7_cpu = 0;
	u32 max_a7ss_index = apcs_clk.clkr.hw.init->num_rate_max;

	apc_fmax = apcs_clk.clkr.hw.init->rate_max[max_a7ss_index - 1];

	for_each_possible_cpu(cpu) {
		a7_cpu = cpu;
		WARN(a7cc_clk_add_opp(&apcs_clk.clkr.hw, get_cpu_device(cpu),
				apc_fmax),
				"Failed to add OPP levels for apcs_clk\n");
	}
	/* One time print during bootup */
	dev_info(&pdev->dev, "OPP tables populated (cpu %d)\n", a7_cpu);

	a7cc_clk_print_opp_table(a7_cpu);
}

static int a7cc_driver_probe(struct platform_device *pdev)
{
	struct clk *clk;
	void __iomem *base;
	u32 opmode_regval, mode_regval;
	struct resource *res;
	struct clk_onecell_data *data;
	struct device *dev = &pdev->dev;
	struct device_node *of = pdev->dev.of_node;
	int i, ret, speed_bin, version, cpu;
	int num_clks = ARRAY_SIZE(cpu_clks_hws);
	u32 a7cc_clk_init_rate = 0;
	char prop_name[] = "qcom,speedX-bin-vX";
	struct clk *ext_xo_clk;

	/* Require the RPMH-XO clock to be registered before */
	ext_xo_clk = devm_clk_get(dev, "xo_ao");
	if (IS_ERR(ext_xo_clk)) {
		if (PTR_ERR(ext_xo_clk) != -EPROBE_DEFER)
			dev_err(dev, "Unable to get xo clock\n");
		return PTR_ERR(ext_xo_clk);
	}

	/* Get speed bin information */
	a7cc_clk_get_speed_bin(pdev, &speed_bin, &version);

	/* Rail Regulator for apcs_pll */
	vdd_cx.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_dig_ao");
	if (IS_ERR(vdd_cx.regulator[0])) {
		if (!(PTR_ERR(vdd_cx.regulator[0]) == -EPROBE_DEFER))
			dev_err(&pdev->dev,
				"Unable to get vdd_dig_ao regulator\n");
		return PTR_ERR(vdd_cx.regulator[0]);
	}

	/* Rail Regulator for APSS a7ss mux */
	vdd_cpu.regulator[0] = devm_regulator_get(&pdev->dev, "cpu-vdd");
	if (IS_ERR(vdd_cpu.regulator[0])) {
		if (!(PTR_ERR(vdd_cpu.regulator[0]) == -EPROBE_DEFER))
			dev_err(&pdev->dev,
				"Unable to get cpu-vdd regulator\n");
		return PTR_ERR(vdd_cpu.regulator[0]);
	}

	snprintf(prop_name, ARRAY_SIZE(prop_name),
			"qcom,speed%d-bin-v%d", speed_bin, version);

	ret = a7cc_clk_get_fmax_vdd_class(pdev,
		(struct clk_init_data *)apcs_clk.clkr.hw.init, prop_name);
	if (ret) {
		dev_err(&pdev->dev,
		"Can't get speed bin for apcs_clk. Falling back to zero\n");
		ret = a7cc_clk_get_fmax_vdd_class(pdev,
				(struct clk_init_data *)apcs_clk.clkr.hw.init,
				"qcom,speed0-bin-v0");
		if (ret) {
			dev_err(&pdev->dev,
			"Unable to get speed bin for apcs_clk freq-corner mapping info\n");
			return ret;
		}
	}

	ret = of_property_read_u32(of, "qcom,a7cc-init-rate",
						&a7cc_clk_init_rate);
	if (ret) {
		dev_err(&pdev->dev,
			"unable to find qcom,a7cc_clk_init_rate property,ret=%d\n",
			ret);
		return -EINVAL;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "apcs_pll");
	base = devm_ioremap_resource(dev, res);
	if (IS_ERR(base)) {
		dev_err(&pdev->dev, "Failed to map apcs_cpu_pll register base\n");
		return PTR_ERR(base);
	}

	apcs_cpu_pll.clkr.regmap = devm_regmap_init_mmio(dev, base,
						&cpu_regmap_config);
	if (IS_ERR(apcs_cpu_pll.clkr.regmap)) {
		dev_err(&pdev->dev, "Couldn't get regmap for apcs_cpu_pll\n");
		return PTR_ERR(apcs_cpu_pll.clkr.regmap);
	}

	ret = of_property_read_u32(of, "qcom,rcg-reg-offset",
						&apcs_clk.reg_offset);
	if (ret) {
		dev_err(&pdev->dev,
			"unable to find qcom,rcg-reg-offset property,ret=%d\n",
			ret);
		return -EINVAL;
	}

	apcs_clk.clkr.regmap = apcs_cpu_pll.clkr.regmap;

	/* Read PLLs OPMODE and mode register */
	ret = regmap_read(apcs_cpu_pll.clkr.regmap, PLL_OPMODE_REG,
							&opmode_regval);
	if (ret)
		return ret;

	ret = regmap_read(apcs_cpu_pll.clkr.regmap, PLL_MODE_REG,
							&mode_regval);
	if (ret)
		return ret;

	/* Configure APSS PLL only if it is not enabled and running */
	if (!(opmode_regval & PLL_OPMODE_RUN) &&
				!(mode_regval & PLL_MODE_OUTCTRL))
		clk_trion_pll_configure(&apcs_cpu_pll,
			apcs_cpu_pll.clkr.regmap, &apcs_cpu_pll_config);

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->clk_num = num_clks;

	data->clks = devm_kzalloc(dev, num_clks * sizeof(struct clk *),
					GFP_KERNEL);
	if (!data->clks)
		return -ENOMEM;

	/* Register clocks with clock framework */
	for (i = 0; i < num_clks; i++) {
		clk = devm_clk_register(dev, cpu_clks_hws[i]);
		if (IS_ERR(clk))
			return PTR_ERR(clk);
		data->clks[i] = clk;
	}

	ret = of_clk_add_provider(dev->of_node, of_clk_src_onecell_get, data);
	if (ret) {
		dev_err(&pdev->dev, "CPU clock driver registeration failed\n");
		return ret;
	}

	ret = clk_notifier_register(apcs_cpu_pll.clkr.hw.clk, &apcs_clk.clk_nb);
	if (ret) {
		dev_err(dev, "failed to register clock notifier: %d\n", ret);
		return ret;
	}

	/* Put proxy vote for APSS PLL */
	clk_prepare_enable(apcs_cpu_pll.clkr.hw.clk);

	/* Reconfigure APSS RCG */
	ret = clk_set_rate(apcs_clk.clkr.hw.clk, sys_apc0_aux_clk.rrate);
	if (ret)
		dev_err(&pdev->dev, "Unable to set aux rate on apcs_clk\n");

	/* Set to TURBO boot frequency */
	ret = clk_set_rate(apcs_clk.clkr.hw.clk, a7cc_clk_init_rate);
	if (ret)
		dev_err(&pdev->dev, "Unable to set init rate on apcs_clk\n");

	/*
	 * We don't want the CPU clocks to be turned off at late init
	 * if CPUFREQ or HOTPLUG configs are disabled. So, bump up the
	 * refcount of these clocks. Any cpufreq/hotplug manager can assume
	 * that the clocks have already been prepared and enabled by the time
	 * they take over.
	 */

	get_online_cpus();
	for_each_online_cpu(cpu)
		WARN(clk_prepare_enable(apcs_clk.clkr.hw.clk),
			"Unable to turn on CPU clock\n");
	put_online_cpus();

	/* Remove proxy vote for APSS PLL */
	clk_disable_unprepare(apcs_cpu_pll.clkr.hw.clk);

	a7cc_clk_populate_opp_table(pdev);

	dev_info(dev, "CPU clock Driver probed successfully\n");

	return ret;
}

static struct platform_driver a7_clk_driver = {
	.probe = a7cc_driver_probe,
	.driver = {
		.name = "qcom-cpu-sdxpoorwills",
		.of_match_table = match_table,
	},
};

static int __init a7_clk_init(void)
{
	return platform_driver_register(&a7_clk_driver);
}
subsys_initcall(a7_clk_init);

static void __exit a7_clk_exit(void)
{
	platform_driver_unregister(&a7_clk_driver);
}
module_exit(a7_clk_exit);

MODULE_ALIAS("platform:cpu");
MODULE_DESCRIPTION("A7 CPU clock Driver");
MODULE_LICENSE("GPL v2");
