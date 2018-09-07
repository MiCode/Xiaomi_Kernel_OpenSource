/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
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
#include <dt-bindings/clock/qcom,cpu-qcs405.h>

#include "clk-alpha-pll.h"
#include "clk-pll.h"
#include "clk-debug.h"
#include "clk-rcg.h"
#include "clk-regmap-mux-div.h"
#include "common.h"
#include "vdd-level-405.h"

#define to_clk_regmap_mux_div(_hw) \
	container_of(to_clk_regmap(_hw), struct clk_regmap_mux_div, clkr)

#define REG_OFFSET	0x4
#define APCS_PLL	0x0b016000
#define APCS_CMD	0x0b011050
#define XO_RATE		19200000

static DEFINE_VDD_REGULATORS(vdd_hf_pll, VDD_HF_PLL_NUM, 2, vdd_hf_levels);
static DEFINE_VDD_REGS_INIT(vdd_cpu, 1);
static unsigned int cpucc_clk_init_rate;

enum apcs_mux_clk_parent {
	P_BI_TCXO_AO,
	P_GPLL0_AO_OUT_MAIN,
	P_APCS_CPU_PLL,
};

static const struct parent_map apcs_mux_clk_parent_map[] = {
	{ P_BI_TCXO_AO, 0 },
	{ P_GPLL0_AO_OUT_MAIN, 4 },
	{ P_APCS_CPU_PLL, 5 },
};

static const char *const apcs_mux_clk_parent_name[] = {
	"cxo_a",
	"gpll0_ao_out_main",
	"apcs_cpu_pll",
};

static int cpucc_clk_set_rate_and_parent(struct clk_hw *hw, unsigned long rate,
						unsigned long prate, u8 index)
{
	struct clk_regmap_mux_div *cpuclk = to_clk_regmap_mux_div(hw);

	return __mux_div_set_src_div(cpuclk, cpuclk->parent_map[index].cfg,
					cpuclk->div);
}

static int cpucc_clk_set_parent(struct clk_hw *hw, u8 index)
{
	/*
	 * Since cpucc_clk_set_rate_and_parent() is defined and set_parent()
	 * will never gets called from clk_change_rate() so return 0.
	 */
	return 0;
}

static int cpucc_clk_set_rate(struct clk_hw *hw, unsigned long rate,
						unsigned long prate)
{
	struct clk_regmap_mux_div *cpuclk = to_clk_regmap_mux_div(hw);

	/*
	 * Parent is same as the last rate.
	 * Here just configure new div.
	 */
	return __mux_div_set_src_div(cpuclk, cpuclk->src, cpuclk->div);
}

static int cpucc_clk_determine_rate(struct clk_hw *hw,
					struct clk_rate_request *req)
{
	struct clk_hw *xo, *apc0_auxclk_hw, *apcs_cpu_pll_hw;
	struct clk_rate_request parent_req = { };
	struct clk_regmap_mux_div *cpuclk = to_clk_regmap_mux_div(hw);
	unsigned long apc0_auxclk_rate, rate = req->rate;
	unsigned long mask = BIT(cpuclk->hid_width) - 1;
	int ret;
	u32 div = 1;

	xo = clk_hw_get_parent_by_index(hw, P_BI_TCXO_AO);
	if (rate == clk_hw_get_rate(xo)) {
		req->best_parent_hw = xo;
		req->best_parent_rate = rate;
		cpuclk->div = div;
		cpuclk->src = cpuclk->parent_map[P_BI_TCXO_AO].cfg;
		return 0;
	}

	apc0_auxclk_hw = clk_hw_get_parent_by_index(hw, P_GPLL0_AO_OUT_MAIN);
	apcs_cpu_pll_hw = clk_hw_get_parent_by_index(hw, P_APCS_CPU_PLL);

	apc0_auxclk_rate = clk_hw_get_rate(apc0_auxclk_hw);
	if (rate <= apc0_auxclk_rate) {
		req->best_parent_hw = apc0_auxclk_hw;
		req->best_parent_rate = apc0_auxclk_rate;

		div = DIV_ROUND_UP((2 * req->best_parent_rate), rate) - 1;
		div = min_t(unsigned long, div, mask);

		req->rate = clk_rcg2_calc_rate(req->best_parent_rate, 0,
							0, 0, div);
		cpuclk->src = cpuclk->parent_map[P_GPLL0_AO_OUT_MAIN].cfg;
	} else {
		parent_req.rate = rate;
		parent_req.best_parent_hw = apcs_cpu_pll_hw;

		req->best_parent_hw = apcs_cpu_pll_hw;
		ret = __clk_determine_rate(req->best_parent_hw, &parent_req);
		if (ret)
			return ret;

		req->best_parent_rate = parent_req.rate;
		cpuclk->src = cpuclk->parent_map[P_APCS_CPU_PLL].cfg;
	}
	cpuclk->div = div;

	return 0;
}

static void cpucc_clk_list_registers(struct seq_file *f, struct clk_hw *hw)
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

static unsigned long cpucc_clk_recalc_rate(struct clk_hw *hw,
					unsigned long prate)
{
	struct clk_regmap_mux_div *cpuclk = to_clk_regmap_mux_div(hw);
	struct clk_hw *parent;
	const char *name = clk_hw_get_name(hw);
	unsigned long parent_rate;
	u32 i, div, src = 0;
	u32 num_parents = clk_hw_get_num_parents(hw);
	int ret = 0;

	ret = mux_div_get_src_div(cpuclk, &src, &div);
	if (ret)
		return ret;

	cpuclk->src = src;
	cpuclk->div = div;

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

static int cpucc_clk_enable(struct clk_hw *hw)
{
	return clk_regmap_mux_div_ops.enable(hw);
}

static void cpucc_clk_disable(struct clk_hw *hw)
{
	clk_regmap_mux_div_ops.disable(hw);
}

static u8 cpucc_clk_get_parent(struct clk_hw *hw)
{
	return clk_regmap_mux_div_ops.get_parent(hw);
}

/*
 * We use the notifier function for switching to a temporary safe configuration
 * (mux and divider), while the APSS pll is reconfigured.
 */
static int cpucc_notifier_cb(struct notifier_block *nb, unsigned long event,
			     void *data)
{
	struct clk_regmap_mux_div *cpuclk = container_of(nb,
					struct clk_regmap_mux_div, clk_nb);
	int ret = 0;
	int safe_src = cpuclk->safe_src;

	if (event == PRE_RATE_CHANGE)
		/* set the mux to safe source(sys_apc0_aux_clk) & div */
		ret = __mux_div_set_src_div(cpuclk, safe_src, 1);

	if (event == ABORT_RATE_CHANGE)
		pr_err("Error in configuring PLL - stay at safe src only\n");

	return notifier_from_errno(ret);
}

static const struct clk_ops cpucc_clk_ops = {
	.enable = cpucc_clk_enable,
	.disable = cpucc_clk_disable,
	.get_parent = cpucc_clk_get_parent,
	.set_rate = cpucc_clk_set_rate,
	.set_parent = cpucc_clk_set_parent,
	.set_rate_and_parent = cpucc_clk_set_rate_and_parent,
	.determine_rate = cpucc_clk_determine_rate,
	.recalc_rate = cpucc_clk_recalc_rate,
	.debug_init = clk_debug_measure_add,
	.list_registers = cpucc_clk_list_registers,
};

/* Initial configuration for 960MHz */
static const struct pll_config apcs_cpu_pll_config = {
	.l = 0x32,
	.m = 0,
	.n = 1,
	.vco_val = 0x0,
	.vco_mask = 0x3 << 20,
	.pre_div_val = 0x0,
	.pre_div_mask = 0x7 << 12,
	.post_div_val = 0x0,
	.post_div_mask = 0x3 << 8,
	.main_output_mask = BIT(3),
	.aux_output_mask = BIT(0),
};

static struct clk_pll apcs_cpu_pll = {
	.mode_reg = 0x0,
	.l_reg = 0x4,
	.m_reg = 0x8,
	.n_reg = 0xc,
	.config_reg = 0x10,
	.status_reg = 0x1c,
	.status_bit = 16,
	.clkr.hw.init = &(struct clk_init_data){
		.name = "apcs_cpu_pll",
		.parent_names = (const char *[]){ "cxo_a" },
		.num_parents = 1,
		.ops = &clk_pll_hf_ops,
		.vdd_class = &vdd_hf_pll,
		.rate_max = (unsigned long[VDD_HF_PLL_NUM]) {
			[VDD_HF_PLL_SVS] = 2000000000,
		},
		.num_rate_max = VDD_HF_PLL_NUM,
	},
};

static struct clk_regmap_mux_div apcs_mux_clk = {
	.reg_offset = 0x0,
	.hid_width  = 5,
	.hid_shift  = 0,
	.src_width  = 3,
	.src_shift  = 8,
	.safe_src = 4,
	.safe_div = 1,
	.parent_map = apcs_mux_clk_parent_map,
	.clk_nb.notifier_call = cpucc_notifier_cb,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "apcs_mux_clk",
		.parent_names = apcs_mux_clk_parent_name,
		.num_parents = 3,
		.vdd_class = &vdd_cpu,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &cpucc_clk_ops,
	},
};

static const struct of_device_id match_table[] = {
	{ .compatible = "qcom,cpu-qcs405" },
	{}
};

static struct regmap_config cpu_regmap_config = {
	.reg_bits	= 32,
	.reg_stride	= 4,
	.val_bits	= 32,
	.max_register	= 0x34,
	.fast_io	= true,
};

static struct clk_hw *cpu_clks_hws[] = {
	[APCS_CPU_PLL] = &apcs_cpu_pll.clkr.hw,
	[APCS_MUX_CLK] = &apcs_mux_clk.clkr.hw,
};

static void cpucc_clk_get_speed_bin(struct platform_device *pdev, int *bin,
							int *version)
{
	struct resource *res;
	u32 pte_efuse, valid;
	void __iomem *base;

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
	valid = ((pte_efuse >> 3) & 0x1) ? ((pte_efuse >> 3) & 0x1) : 0;
	*version = (pte_efuse >> 4) & 0x3;

	dev_info(&pdev->dev, "PVS version: %d bin: %d\n", *version, *bin);
}

static int cpucc_clk_get_fmax_vdd_class(struct platform_device *pdev,
			struct clk_init_data *clk_intd, char *prop_name)
{
	struct device_node *of = pdev->dev.of_node;
	struct clk_vdd_class *vdd = clk_intd->vdd_class;
	int prop_len, i, j, ret;
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

	ret = of_property_read_u32_array(of, prop_name, array, prop_len * num);
	if (ret)
		return -ENOMEM;

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
cpucc_clk_add_opp(struct clk_hw *hw, struct device *dev, unsigned long max_rate)
{
	struct clk_init_data *clk_intd =  (struct clk_init_data *)hw->init;
	struct clk_vdd_class *vdd = clk_intd->vdd_class;
	unsigned long rate = 0;
	long ret;
	int level, uv, j = 1;

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

static void cpucc_clk_print_opp_table(int cpu)
{
	struct dev_pm_opp *oppfmax, *oppfmin;
	unsigned long apc_fmax, apc_fmin;
	u32 max_cpuss_index = apcs_mux_clk.clkr.hw.init->num_rate_max;

	apc_fmax = apcs_mux_clk.clkr.hw.init->rate_max[max_cpuss_index - 1];
	apc_fmin = apcs_mux_clk.clkr.hw.init->rate_max[1];

	oppfmax = dev_pm_opp_find_freq_exact(get_cpu_device(cpu),
					apc_fmax, true);
	oppfmin = dev_pm_opp_find_freq_exact(get_cpu_device(cpu),
					apc_fmin, true);
	pr_info("Clock_cpu:(cpu %d) OPP voltage for %lu: %ld\n", cpu, apc_fmin,
		dev_pm_opp_get_voltage(oppfmin));
	pr_info("Clock_cpu:(cpu %d) OPP voltage for %lu: %ld\n", cpu, apc_fmax,
		dev_pm_opp_get_voltage(oppfmax));

}

static void cpucc_clk_populate_opp_table(struct platform_device *pdev)
{
	unsigned long apc_fmax;
	int cpu, qcs405_cpu = 0;
	u32 max_cpuss_index = apcs_mux_clk.clkr.hw.init->num_rate_max;

	apc_fmax = apcs_mux_clk.clkr.hw.init->rate_max[max_cpuss_index - 1];

	for_each_possible_cpu(cpu) {
		qcs405_cpu = cpu;
		WARN(cpucc_clk_add_opp(&apcs_mux_clk.clkr.hw,
				get_cpu_device(cpu), apc_fmax),
				"Failed to add OPP levels for apcs_mux_clk\n");
	}
	cpucc_clk_print_opp_table(qcs405_cpu);
}

static int cpucc_driver_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct clk_hw_onecell_data *data;
	struct device *dev = &pdev->dev;
	struct device_node *of = pdev->dev.of_node;
	struct clk *clk;
	u32 rate = 0;
	int i, ret, speed_bin, version, cpu;
	char prop_name[] = "qcom,speedX-bin-vX";
	void __iomem *base;

	/* Require the RPM-XO clock to be registered before */
	clk = devm_clk_get(dev, "xo_ao");
	if (IS_ERR(clk)) {
		if (PTR_ERR(clk) != -EPROBE_DEFER)
			dev_err(dev, "Unable to get xo clock\n");
		return PTR_ERR(clk);
	}

	clk = devm_clk_get(dev, "gpll0_ao");
	if (IS_ERR(clk)) {
		if (PTR_ERR(clk) != -EPROBE_DEFER)
			dev_err(dev, "Unable to get GPLL0 clock\n");
		return PTR_ERR(clk);
	}

	 /* Rail Regulator for apcs_pll */
	vdd_hf_pll.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_hf_pll");
	if (IS_ERR(vdd_hf_pll.regulator[0])) {
		if (!(PTR_ERR(vdd_hf_pll.regulator[0]) == -EPROBE_DEFER))
			dev_err(&pdev->dev,
				"Unable to get vdd_hf_pll regulator\n");
		return PTR_ERR(vdd_hf_pll.regulator[0]);
	}

	vdd_hf_pll.regulator[1] = devm_regulator_get(&pdev->dev, "vdd_dig_ao");
	if (IS_ERR(vdd_hf_pll.regulator[1])) {
		if (!(PTR_ERR(vdd_hf_pll.regulator[1]) == -EPROBE_DEFER))
			dev_err(&pdev->dev,
				"Unable to get vdd_dig_ao regulator\n");
		return PTR_ERR(vdd_hf_pll.regulator[1]);
	}

	/* Rail Regulator for APSS cpuss mux */
	vdd_cpu.regulator[0] = devm_regulator_get(&pdev->dev, "cpu-vdd");
	if (IS_ERR(vdd_cpu.regulator[0])) {
		if (!(PTR_ERR(vdd_cpu.regulator[0]) == -EPROBE_DEFER))
			dev_err(&pdev->dev,
				"Unable to get cpu-vdd regulator\n");
		return PTR_ERR(vdd_cpu.regulator[0]);
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "apcs_pll");
	base = devm_ioremap_resource(dev, res);
	if (IS_ERR(base)) {
		dev_err(&pdev->dev, "Failed to map apcs_cpu_pll register base\n");
		return PTR_ERR(base);
	}

	cpu_regmap_config.name = "apcs_pll";
	apcs_cpu_pll.clkr.regmap = devm_regmap_init_mmio(dev, base,
							&cpu_regmap_config);
	if (IS_ERR(apcs_cpu_pll.clkr.regmap)) {
		dev_err(&pdev->dev, "Couldn't get regmap for apcs_cpu_pll\n");
		return PTR_ERR(apcs_cpu_pll.clkr.regmap);
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "apcs_cmd");
	base = devm_ioremap_resource(dev, res);
	if (IS_ERR(base)) {
		dev_err(&pdev->dev, "Failed to map apcs_cmd register base\n");
		return PTR_ERR(base);
	}

	cpu_regmap_config.name = "apcs_cmd";
	apcs_mux_clk.clkr.regmap = devm_regmap_init_mmio(dev, base,
							&cpu_regmap_config);
	if (IS_ERR(apcs_mux_clk.clkr.regmap)) {
		dev_err(&pdev->dev, "Couldn't get regmap for apcs_cmd\n");
		return PTR_ERR(apcs_mux_clk.clkr.regmap);
	}

	/* Get speed bin information */
	cpucc_clk_get_speed_bin(pdev, &speed_bin, &version);

	snprintf(prop_name, ARRAY_SIZE(prop_name),
			"qcom,speed%d-bin-v%d", speed_bin, version);

	ret = cpucc_clk_get_fmax_vdd_class(pdev,
		(struct clk_init_data *)apcs_mux_clk.clkr.hw.init, prop_name);
	if (ret) {
		dev_err(&pdev->dev,
		"Can't get speed bin for apcs_mux_clk. Falling back to zero\n");
		ret = cpucc_clk_get_fmax_vdd_class(pdev,
				(struct clk_init_data *)
				apcs_mux_clk.clkr.hw.init,
					"qcom,speed0-bin-v0");
		if (ret) {
			dev_err(&pdev->dev,
			"Unable to get speed bin for apcs_mux_clk freq-corner mapping info\n");
			return ret;
		}
	}

	ret = of_property_read_u32(of, "qcom,cpucc-init-rate", &rate);
	if (ret || !rate)
		dev_err(&pdev->dev, "Init rate for clock not defined\n");

	cpucc_clk_init_rate = max(cpucc_clk_init_rate, rate);

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->num = ARRAY_SIZE(cpu_clks_hws);

	/* Register clocks with clock framework */
	for (i = 0; i < ARRAY_SIZE(cpu_clks_hws); i++) {
		ret = devm_clk_hw_register(dev, cpu_clks_hws[i]);
		if (ret) {
			dev_err(&pdev->dev, "Failed to register clock\n");
			return ret;
		}
		data->hws[i] = cpu_clks_hws[i];
	}

	ret = of_clk_add_hw_provider(dev->of_node, of_clk_hw_onecell_get, data);
	if (ret) {
		dev_err(&pdev->dev, "CPU clock driver registeration failed\n");
		return ret;
	}

	ret = clk_notifier_register(apcs_mux_clk.clkr.hw.clk,
							&apcs_mux_clk.clk_nb);
	if (ret) {
		dev_err(dev, "failed to register clock notifier: %d\n", ret);
		return ret;
	}

	/* Set to boot frequency */
	ret = clk_set_rate(apcs_mux_clk.clkr.hw.clk, cpucc_clk_init_rate);
	if (ret)
		dev_err(&pdev->dev, "Unable to set init rate on apcs_mux_clk\n");

	/*
	 * We don't want the CPU clocks to be turned off at late init
	 * if CPUFREQ or HOTPLUG configs are disabled. So, bump up the
	 * refcount of these clocks. Any cpufreq/hotplug manager can assume
	 * that the clocks have already been prepared and enabled by the time
	 * they take over.
	 */
	get_online_cpus();
	for_each_online_cpu(cpu)
		WARN(clk_prepare_enable(apcs_mux_clk.clkr.hw.clk),
			"Unable to turn on CPU clock\n");
	put_online_cpus();

	cpucc_clk_populate_opp_table(pdev);
	dev_info(dev, "CPU clock Driver probed successfully\n");

	return ret;
}

static struct platform_driver cpu_clk_driver = {
	.probe = cpucc_driver_probe,
	.driver = {
		.name = "qcom-cpu-qcs405",
		.of_match_table = match_table,
	},
};

static int __init cpu_clk_init(void)
{
	return platform_driver_register(&cpu_clk_driver);
}
subsys_initcall(cpu_clk_init);

static void __exit cpu_clk_exit(void)
{
	platform_driver_unregister(&cpu_clk_driver);
}
module_exit(cpu_clk_exit);

static void enable_hf_pll(void __iomem *base)
{
	writel_relaxed(0x2, base + apcs_cpu_pll.mode_reg);
	udelay(2);
	writel_relaxed(0x6, base + apcs_cpu_pll.mode_reg);
	udelay(50);
	writel_relaxed(0x7, base + apcs_cpu_pll.mode_reg);
	/* Ensure that the writes go through before enabling PLL */
	mb();
}

static void __init configure_hf_pll(void __iomem *base)
{
	/* Disable Mode */
	writel_relaxed(0x0, base + apcs_cpu_pll.mode_reg);

	/* Configure L/M/N values */
	writel_relaxed(apcs_cpu_pll_config.l, base + apcs_cpu_pll.l_reg);
	writel_relaxed(apcs_cpu_pll_config.m, base + apcs_cpu_pll.m_reg);
	writel_relaxed(apcs_cpu_pll_config.n, base + apcs_cpu_pll.n_reg);

	/* Configure USER_CTL value */
	writel_relaxed(0x0100000f, base + apcs_cpu_pll.config_reg);
}

static int __init cpu_clock_init(void)
{
	struct device_node *dev;
	void __iomem  *base;
	int count, regval = 0, l_val;
	unsigned long enable_mask = 0x7;

	dev = of_find_compatible_node(NULL, NULL, "qcom,cpu-qcs405");
	if (!dev) {
		pr_debug("device node not initialized\n");
		return -ENOMEM;
	}

	base = ioremap_nocache(APCS_PLL, SZ_64);
	if (!base)
		return -ENOMEM;

	l_val =  readl_relaxed(base + apcs_cpu_pll.l_reg);
	if (!l_val)
		configure_hf_pll(base);

	cpucc_clk_init_rate = l_val * XO_RATE;

	regval = readl_relaxed(base);
	if (!((regval & enable_mask) == enable_mask))
		enable_hf_pll(base);

	iounmap(base);

	base = ioremap_nocache(APCS_CMD, SZ_8);
	if (!base)
		return -ENOMEM;

	writel_relaxed(0x501, base + REG_OFFSET);

	/* Update bit */
	regval = readl_relaxed(base);
	regval |= BIT(0);
	writel_relaxed(regval, base);

	/* Wait for update to take effect */
	for (count = 500; count > 0; count--) {
		if ((!(readl_relaxed(base))) & BIT(0))
			break;
		udelay(1);
	}

	return 0;
}
early_initcall(cpu_clock_init);

MODULE_ALIAS("platform:cpu");
MODULE_DESCRIPTION("QCS405 CPU clock Driver");
MODULE_LICENSE("GPL v2");
