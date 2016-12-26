/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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
#include <linux/delay.h>
#include <linux/clk/msm-clock-generic.h>

#include <soc/qcom/clock-local2.h>
#include <soc/qcom/clock-voter.h>
#include <soc/qcom/clock-pll.h>
#include <soc/qcom/clock-alpha-pll.h>

#include <dt-bindings/clock/msm-clocks-8998.h>
#include <dt-bindings/clock/msm-clocks-hwio-8998.h>

#include "vdd-level-8998.h"

static void __iomem *virt_base;
static void __iomem *virt_base_gfx;

#define gpucc_cxo_clk_source_val		0
#define gpucc_gpll0_source_val			5
#define gpu_pll0_pll_out_even_source_val	1
#define gpu_pll0_pll_out_odd_source_val		2

#define SW_COLLAPSE_MASK			BIT(0)
#define GPU_CX_GDSCR_OFFSET			0x1004
#define GPU_GX_GDSCR_OFFSET			0x1094
#define CRC_SID_FSM_OFFSET			0x10A0
#define CRC_MND_CFG_OFFSET			0x10A4

#define F(f, s, div, m, n) \
	{ \
		.freq_hz = (f), \
		.src_clk = &s.c, \
		.m_val = (m), \
		.n_val = ~((n)-(m)) * !!(n), \
		.d_val = ~(n),\
		.div_src_val = BVAL(4, 0, (int)(2*(div) - 1)) \
			| BVAL(10, 8, s##_source_val), \
	}

#define F_SLEW(f, s_f, s, div, m, n) \
	{ \
		.freq_hz = (f), \
		.src_freq = (s_f), \
		.src_clk = &s.c, \
		.m_val = (m), \
		.n_val = ~((n)-(m)) * !!(n), \
		.d_val = ~(n),\
		.div_src_val = BVAL(4, 0, (int)(2*(div) - 1)) \
			| BVAL(10, 8, s##_source_val), \
	}

static struct alpha_pll_masks pll_masks_p = {
	.lock_mask = BIT(31),
	.active_mask = BIT(30),
	.update_mask = BIT(22),
	.output_mask = 0xf,
	.post_div_mask = BM(15, 8),
};

static DEFINE_VDD_REGULATORS(vdd_dig, VDD_DIG_NUM, 1, vdd_corner, NULL);
DEFINE_VDD_REGS_INIT(vdd_gpucc, 2);
DEFINE_VDD_REGS_INIT(vdd_gpucc_mx, 1);

DEFINE_EXT_CLK(gpucc_xo, NULL);
DEFINE_EXT_CLK(gpucc_gpll0, NULL);

static struct branch_clk gpucc_cxo_clk = {
	.cbcr_reg = GPUCC_CXO_CBCR,
	.has_sibling = 1,
	.base = &virt_base,
	.c = {
		.dbg_name = "gpucc_cxo_clk",
		.parent = &gpucc_xo.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gpucc_cxo_clk.c),
	},
};

static struct alpha_pll_clk gpu_pll0_pll = {
	.masks = &pll_masks_p,
	.base = &virt_base_gfx,
	.offset = GPUCC_GPU_PLL0_PLL_MODE,
	.enable_config = 0x1,
	.is_fabia = true,
	.c = {
		.rate = 0,
		.parent = &gpucc_xo.c,
		.dbg_name = "gpu_pll0_pll",
		.ops = &clk_ops_fabia_alpha_pll,
		VDD_GPU_PLL_FMAX_MAP1(MIN, 1300000500),
		CLK_INIT(gpu_pll0_pll.c),
	},
};

static struct div_clk gpu_pll0_pll_out_even = {
	.base = &virt_base_gfx,
	.offset = GPUCC_GPU_PLL0_USER_CTL_MODE,
	.mask = 0xf,
	.shift = 8,
	.data = {
		.max_div = 8,
		.min_div = 1,
		.skip_odd_div = true,
		.allow_div_one = true,
		.rate_margin = 500,
	},
	.ops = &postdiv_reg_ops,
	.c = {
		.parent = &gpu_pll0_pll.c,
		.dbg_name = "gpu_pll0_pll_out_even",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(gpu_pll0_pll_out_even.c),
	},
};

static struct div_clk gpu_pll0_pll_out_odd = {
	.base = &virt_base_gfx,
	.offset = GPUCC_GPU_PLL0_USER_CTL_MODE,
	.mask = 0xf,
	.shift = 12,
	.data = {
		.max_div = 7,
		.min_div = 3,
		.skip_even_div = true,
		.rate_margin = 500,
	},
	.ops = &postdiv_reg_ops,
	.c = {
		.parent = &gpu_pll0_pll.c,
		.dbg_name = "gpu_pll0_pll_out_odd",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(gpu_pll0_pll_out_odd.c),
	},
};

static struct clk_freq_tbl ftbl_gfx3d_clk_src[] = {
	F_SLEW( 171000000,  342000000, gpu_pll0_pll_out_even,    1, 0, 0),
	F_SLEW( 251000000,  502000000, gpu_pll0_pll_out_even,    1, 0, 0),
	F_SLEW( 332000000,  664000000, gpu_pll0_pll_out_even,    1, 0, 0),
	F_SLEW( 403000000,  806000000, gpu_pll0_pll_out_even,    1, 0, 0),
	F_SLEW( 504000000, 1008000000, gpu_pll0_pll_out_even,    1, 0, 0),
	F_SLEW( 650000000, 1300000000, gpu_pll0_pll_out_even,    1, 0, 0),
	F_END
};

static struct clk_freq_tbl ftbl_gfx3d_clk_src_v2[] = {
	F_SLEW( 180000000,  360000000, gpu_pll0_pll_out_even,    1, 0, 0),
	F_SLEW( 257000000,  514000000, gpu_pll0_pll_out_even,    1, 0, 0),
	F_SLEW( 342000000,  684000000, gpu_pll0_pll_out_even,    1, 0, 0),
	F_SLEW( 414000000,  828000000, gpu_pll0_pll_out_even,    1, 0, 0),
	F_SLEW( 515000000, 1030000000, gpu_pll0_pll_out_even,    1, 0, 0),
	F_SLEW( 596000000, 1192000000, gpu_pll0_pll_out_even,    1, 0, 0),
	F_SLEW( 670000000, 1340000000, gpu_pll0_pll_out_even,    1, 0, 0),
	F_SLEW( 710000000, 1420000000, gpu_pll0_pll_out_even,    1, 0, 0),
	F_END
};

static struct clk_freq_tbl ftbl_gfx3d_clk_src_vq[] = {
	F_SLEW( 180000000,  360000000, gpu_pll0_pll_out_even,    1, 0, 0),
	F_SLEW( 265000000,  530000000, gpu_pll0_pll_out_even,    1, 0, 0),
	F_SLEW( 358000000,  716000000, gpu_pll0_pll_out_even,    1, 0, 0),
	F_SLEW( 434000000,  868000000, gpu_pll0_pll_out_even,    1, 0, 0),
	F_SLEW( 542000000, 1084000000, gpu_pll0_pll_out_even,    1, 0, 0),
	F_SLEW( 630000000, 1260000000, gpu_pll0_pll_out_even,    1, 0, 0),
	F_SLEW( 700000000, 1400000000, gpu_pll0_pll_out_even,    1, 0, 0),
	F_SLEW( 750000000, 1500000000, gpu_pll0_pll_out_even,    1, 0, 0),
	F_END
};

static struct rcg_clk gfx3d_clk_src = {
	.cmd_rcgr_reg = GPUCC_GFX3D_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gfx3d_clk_src,
	.current_freq = &rcg_dummy_freq,
	.force_enable_rcgr = true,
	.base = &virt_base_gfx,
	.c = {
		.dbg_name = "gfx3d_clk_src",
		.ops = &clk_ops_rcg,
		.vdd_class = &vdd_gpucc,
		CLK_INIT(gfx3d_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_rbbmtimer_clk_src[] = {
	F( 19200000, gpucc_cxo_clk,    1,    0,     0),
	F_END
};

static struct rcg_clk rbbmtimer_clk_src = {
	.cmd_rcgr_reg = GPUCC_RBBMTIMER_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_rbbmtimer_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "rbbmtimer_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP1(MIN, 19200000),
		CLK_INIT(rbbmtimer_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_gfx3d_isense_clk_src[] = {
	F(  19200000, gpucc_cxo_clk,    1,    0,     0),
	F(  40000000,   gpucc_gpll0,   15,    0,     0),
	F( 200000000,   gpucc_gpll0,    3,    0,     0),
	F( 300000000,   gpucc_gpll0,    2,    0,     0),
	F_END
};

static struct rcg_clk gfx3d_isense_clk_src = {
	.cmd_rcgr_reg = GPUCC_GFX3D_ISENSE_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_gfx3d_isense_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "gfx3d_isense_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP4(MIN, 19200000, LOWER, 40000000,
				LOW, 200000000, HIGH, 300000000),
		CLK_INIT(gfx3d_isense_clk_src.c),
	},
};

static struct clk_freq_tbl ftbl_rbcpr_clk_src[] = {
	F( 19200000, gpucc_cxo_clk,    1,    0,     0),
	F( 50000000,   gpucc_gpll0,   12,    0,     0),
	F_END
};

static struct rcg_clk rbcpr_clk_src = {
	.cmd_rcgr_reg = GPUCC_RBCPR_CMD_RCGR,
	.set_rate = set_rate_hid,
	.freq_tbl = ftbl_rbcpr_clk_src,
	.current_freq = &rcg_dummy_freq,
	.base = &virt_base,
	.c = {
		.dbg_name = "rbcpr_clk_src",
		.ops = &clk_ops_rcg,
		VDD_DIG_FMAX_MAP2(MIN, 19200000, NOMINAL, 50000000),
		CLK_INIT(rbcpr_clk_src.c),
	},
};

static struct branch_clk gpucc_gfx3d_clk = {
	.cbcr_reg = GPUCC_GFX3D_CBCR,
	.has_sibling = 0,
	.base = &virt_base_gfx,
	.c = {
		.dbg_name = "gpucc_gfx3d_clk",
		.parent = &gfx3d_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gpucc_gfx3d_clk.c),
	},
};

static struct branch_clk gpucc_rbbmtimer_clk = {
	.cbcr_reg = GPUCC_RBBMTIMER_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gpucc_rbbmtimer_clk",
		.parent = &rbbmtimer_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gpucc_rbbmtimer_clk.c),
	},
};

static struct branch_clk gpucc_gfx3d_isense_clk = {
	.cbcr_reg = GPUCC_GFX3D_ISENSE_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gpucc_gfx3d_isense_clk",
		.parent = &gfx3d_isense_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gpucc_gfx3d_isense_clk.c),
	},
};

static struct branch_clk gpucc_rbcpr_clk = {
	.cbcr_reg = GPUCC_RBCPR_CBCR,
	.has_sibling = 0,
	.base = &virt_base,
	.c = {
		.dbg_name = "gpucc_rbcpr_clk",
		.parent = &rbcpr_clk_src.c,
		.ops = &clk_ops_branch,
		CLK_INIT(gpucc_rbcpr_clk.c),
	},
};

static struct fixed_clk gpucc_mx_clk = {
	.c = {
		.dbg_name = "gpucc_mx_clk",
		.vdd_class = &vdd_gpucc_mx,
		.ops = &clk_ops_dummy,
		CLK_INIT(gpucc_mx_clk.c),
	},
};

static int of_get_fmax_vdd_class(struct platform_device *pdev, struct clk *c,
								char *prop_name)
{
	struct device_node *of = pdev->dev.of_node;
	int prop_len, i, j;
	struct clk_vdd_class *vdd = c->vdd_class;
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

	c->fmax = devm_kzalloc(&pdev->dev, prop_len * sizeof(unsigned long),
					GFP_KERNEL);
	if (!c->fmax)
		return -ENOMEM;

	array = devm_kzalloc(&pdev->dev,
			prop_len * sizeof(u32) * num, GFP_KERNEL);
	if (!array)
		return -ENOMEM;

	of_property_read_u32_array(of, prop_name, array, prop_len * num);
	for (i = 0; i < prop_len; i++) {
		c->fmax[i] = array[num * i];
		for (j = 1; j < num; j++) {
			vdd->vdd_uv[(num - 1) * i + (j - 1)] =
						array[num * i + j];
		}
	}

	devm_kfree(&pdev->dev, array);
	vdd->num_levels = prop_len;
	vdd->cur_level = prop_len;
	c->num_fmax = prop_len;
	return 0;
}

static struct mux_clk gpucc_gcc_dbg_clk = {
	.ops = &mux_reg_ops,
	.en_mask = BIT(16),
	.mask = 0x3FF,
	.offset = GPUCC_DEBUG_CLK_CTL,
	.en_offset = GPUCC_DEBUG_CLK_CTL,
	.base = &virt_base,
	MUX_SRC_LIST(
		{ &gpucc_rbcpr_clk.c, 0x0003 },
		{ &gpucc_rbbmtimer_clk.c, 0x0005 },
		{ &gpucc_gfx3d_isense_clk.c, 0x000a },
	),
	.c = {
		.dbg_name = "gpucc_gcc_dbg_clk",
		.ops = &clk_ops_gen_mux,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(gpucc_gcc_dbg_clk.c),
	},
};

static void enable_gfx_crc(void)
{
	u32 regval;

	/* Set graphics clock at a safe frequency */
	clk_set_rate(&gpucc_gfx3d_clk.c, gfx3d_clk_src.c.fmax[2]);
	/* Turn on the GPU_CX GDSC */
	regval = readl_relaxed(virt_base_gfx + GPU_CX_GDSCR_OFFSET);
	regval &= ~SW_COLLAPSE_MASK;
	writel_relaxed(regval, virt_base_gfx + GPU_CX_GDSCR_OFFSET);
	/* Wait for 10usecs to let the GDSC turn ON */
	mb();
	udelay(10);
	/* Turn on the Graphics rail */
	if (regulator_enable(vdd_gpucc.regulator[0]))
		pr_warn("Enabling the graphics rail during CRC sequence failed!\n");
	/* Turn on the GPU_GX GDSC */
	writel_relaxed(0x1, virt_base_gfx + GPU_GX_BCR);
	/*
	 * BLK_ARES should be kept asserted for 1us before being de-asserted.
	 */
	wmb();
	udelay(1);
	writel_relaxed(0x0, virt_base_gfx + GPU_GX_BCR);
	regval = readl_relaxed(virt_base_gfx + GPUCC_GX_DOMAIN_MISC);
	regval |= BIT(4);
	writel_relaxed(regval, virt_base_gfx + GPUCC_GX_DOMAIN_MISC);
	/* Keep reset asserted for at-least 1us before continuing. */
	wmb();
	udelay(1);
	regval &= ~BIT(4);
	writel_relaxed(regval, virt_base_gfx + GPUCC_GX_DOMAIN_MISC);
	/* Make sure GMEM_RESET is de-asserted before continuing. */
	wmb();
	regval &= ~BIT(0);
	writel_relaxed(regval, virt_base_gfx + GPUCC_GX_DOMAIN_MISC);
	/* All previous writes should be done at this point */
	wmb();
	regval = readl_relaxed(virt_base_gfx + GPU_GX_GDSCR_OFFSET);
	regval &= ~SW_COLLAPSE_MASK;
	writel_relaxed(regval, virt_base_gfx + GPU_GX_GDSCR_OFFSET);
	/* Wait for 10usecs to let the GDSC turn ON */
	mb();
	udelay(10);
	/* Enable the graphics clock */
	clk_prepare_enable(&gpucc_gfx3d_clk.c);
	/* Enabling MND RC in Bypass mode */
	writel_relaxed(0x00015010, virt_base_gfx + CRC_MND_CFG_OFFSET);
	writel_relaxed(0x00800000, virt_base_gfx + CRC_SID_FSM_OFFSET);
	/* Wait for 16 cycles before continuing */
	udelay(1);
	clk_set_rate(&gpucc_gfx3d_clk.c,
			gfx3d_clk_src.c.fmax[gfx3d_clk_src.c.num_fmax - 1]);
	/* Disable the graphics clock */
	clk_disable_unprepare(&gpucc_gfx3d_clk.c);
	/* Turn off the gpu_cx and gpu_gx GDSCs */
	regval = readl_relaxed(virt_base_gfx + GPU_GX_GDSCR_OFFSET);
	regval |= SW_COLLAPSE_MASK;
	writel_relaxed(regval, virt_base_gfx + GPU_GX_GDSCR_OFFSET);
	/* Write to disable GX GDSC should go through before continuing */
	wmb();
	regval = readl_relaxed(virt_base_gfx + GPUCC_GX_DOMAIN_MISC);
	regval |= BIT(0);
	writel_relaxed(regval, virt_base_gfx + GPUCC_GX_DOMAIN_MISC);
	/* Make sure GMEM_CLAMP_IO is asserted before continuing. */
	wmb();
	regulator_disable(vdd_gpucc.regulator[0]);
	regval = readl_relaxed(virt_base_gfx + GPU_CX_GDSCR_OFFSET);
	regval |= SW_COLLAPSE_MASK;
	writel_relaxed(regval, virt_base_gfx + GPU_CX_GDSCR_OFFSET);
}

static struct mux_clk gfxcc_dbg_clk = {
	.ops = &mux_reg_ops,
	.en_mask = BIT(16),
	.mask = 0x3FF,
	.offset = GPUCC_DEBUG_CLK_CTL,
	.en_offset = GPUCC_DEBUG_CLK_CTL,
	.base = &virt_base_gfx,
	MUX_SRC_LIST(
		{ &gpucc_gfx3d_clk.c, 0x0008 },
	),
	.c = {
		.dbg_name = "gfxcc_dbg_clk",
		.ops = &clk_ops_gen_mux,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(gfxcc_dbg_clk.c),
	},
};

static struct clk_lookup msm_clocks_gpucc_8998[] = {
	CLK_LIST(gpucc_xo),
	CLK_LIST(gpucc_gpll0),
	CLK_LIST(gpucc_cxo_clk),
	CLK_LIST(rbbmtimer_clk_src),
	CLK_LIST(gfx3d_isense_clk_src),
	CLK_LIST(rbcpr_clk_src),
	CLK_LIST(gpucc_rbbmtimer_clk),
	CLK_LIST(gpucc_gfx3d_isense_clk),
	CLK_LIST(gpucc_rbcpr_clk),
	CLK_LIST(gpucc_gcc_dbg_clk),
};

static void msm_gpucc_hamster_fixup(void)
{
	gfx3d_isense_clk_src.c.ops = &clk_ops_dummy;
	gpucc_gfx3d_isense_clk.c.ops = &clk_ops_dummy;
}

static struct platform_driver msm_clock_gfxcc_driver;
int msm_gpucc_8998_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct device_node *of_node = pdev->dev.of_node;
	int rc;
	struct regulator *reg;
	u32 regval;
	struct clk *tmp;
	bool is_vq = 0;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cc_base");
	if (!res) {
		dev_err(&pdev->dev, "Unable to retrieve register base\n");
		return -ENOMEM;
	}

	virt_base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!virt_base) {
		dev_err(&pdev->dev, "Failed to map CC registers\n");
		return -ENOMEM;
	}

	reg = vdd_dig.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_dig");
	if (IS_ERR(reg)) {
		if (PTR_ERR(reg) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get vdd_dig regulator\n");
		return PTR_ERR(reg);
	}

	tmp = gpucc_xo.c.parent = devm_clk_get(&pdev->dev, "xo_ao");
	if (IS_ERR(tmp)) {
		if (PTR_ERR(tmp) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get xo_ao clock\n");
		return PTR_ERR(tmp);
	}

	tmp = gpucc_gpll0.c.parent = devm_clk_get(&pdev->dev, "gpll0");
	if (IS_ERR(tmp)) {
		if (PTR_ERR(tmp) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get gpll0 clock\n");
		return PTR_ERR(tmp);
	}

	/* Clear the DBG_CLK_DIV bits of the GPU debug register */
	regval = readl_relaxed(virt_base + gpucc_gcc_dbg_clk.offset);
	regval &= ~BM(18, 17);
	writel_relaxed(regval, virt_base + gpucc_gcc_dbg_clk.offset);

	is_vq = of_device_is_compatible(pdev->dev.of_node,
						"qcom,gpucc-hamster");
	if (is_vq)
		msm_gpucc_hamster_fixup();

	rc = of_msm_clock_register(of_node, msm_clocks_gpucc_8998,
					ARRAY_SIZE(msm_clocks_gpucc_8998));
	if (rc)
		return rc;

	/*
	 * gpucc_cxo_clk works as the root clock for all GPUCC RCGs and GDSCs.
	 *  Keep it enabled always.
	 */
	clk_prepare_enable(&gpucc_cxo_clk.c);

	dev_info(&pdev->dev, "Registered GPU clocks (barring gfx3d clocks)\n");
	return platform_driver_register(&msm_clock_gfxcc_driver);
}

static const struct of_device_id msm_clock_gpucc_match_table[] = {
	{ .compatible = "qcom,gpucc-8998" },
	{ .compatible = "qcom,gpucc-8998-v2" },
	{ .compatible = "qcom,gpucc-hamster" },
	{},
};

static struct platform_driver msm_clock_gpucc_driver = {
	.probe = msm_gpucc_8998_probe,
	.driver = {
		.name = "qcom,gpucc-8998",
		.of_match_table = msm_clock_gpucc_match_table,
		.owner = THIS_MODULE,
	},
};

static struct clk_lookup msm_clocks_gfxcc_8998[] = {
	CLK_LIST(gpu_pll0_pll),
	CLK_LIST(gpu_pll0_pll_out_even),
	CLK_LIST(gpu_pll0_pll_out_odd),
	CLK_LIST(gfx3d_clk_src),
	CLK_LIST(gpucc_gfx3d_clk),
	CLK_LIST(gpucc_mx_clk),
	CLK_LIST(gfxcc_dbg_clk),
};

static void msm_gfxcc_hamster_fixup(void)
{
	gpu_pll0_pll.c.fmax[VDD_DIG_MIN] = 1420000500;
	gfx3d_clk_src.freq_tbl = ftbl_gfx3d_clk_src_vq;
}

static void msm_gfxcc_8998_v2_fixup(void)
{
	gpu_pll0_pll.c.fmax[VDD_DIG_MIN] = 1420000500;
	gfx3d_clk_src.freq_tbl = ftbl_gfx3d_clk_src_v2;
}

int msm_gfxcc_8998_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct device_node *of_node = pdev->dev.of_node;
	int rc;
	struct regulator *reg;
	u32 regval;
	bool is_v2 = 0, is_vq = 0;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cc_base");
	if (!res) {
		dev_err(&pdev->dev, "Unable to retrieve register base\n");
		return -ENOMEM;
	}

	virt_base_gfx = devm_ioremap(&pdev->dev, res->start,
						resource_size(res));
	if (!virt_base_gfx) {
		dev_err(&pdev->dev, "Failed to map CC registers\n");
		return -ENOMEM;
	}

	reg = vdd_gpucc.regulator[0] = devm_regulator_get(&pdev->dev,
								"vdd_gpucc");
	if (IS_ERR(reg)) {
		if (PTR_ERR(reg) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get vdd_gpucc regulator\n");
		return PTR_ERR(reg);
	}

	reg = vdd_gpucc.regulator[1] = devm_regulator_get(&pdev->dev, "vdd_mx");
	if (IS_ERR(reg)) {
		if (PTR_ERR(reg) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get vdd_mx regulator\n");
		return PTR_ERR(reg);
	}

	reg = vdd_gpucc_mx.regulator[0] = devm_regulator_get(&pdev->dev,
								"vdd_gpu_mx");
	if (IS_ERR(reg)) {
		if (PTR_ERR(reg) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get vdd_gpu_mx regulator\n");
		return PTR_ERR(reg);
	}

	rc = of_get_fmax_vdd_class(pdev, &gfx3d_clk_src.c,
						"qcom,gfxfreq-speedbin0");
	if (rc) {
		dev_err(&pdev->dev, "Can't get freq-corner mapping for gfx3d_clk_src\n");
		return rc;
	}

	rc = of_get_fmax_vdd_class(pdev, &gpucc_mx_clk.c,
						"qcom,gfxfreq-mx-speedbin0");
	if (rc) {
		dev_err(&pdev->dev, "Can't get freq-corner mapping for gpucc_mx_clk\n");
		return rc;
	}

	is_v2 = of_device_is_compatible(pdev->dev.of_node,
						"qcom,gfxcc-8998-v2");
	if (is_v2)
		msm_gfxcc_8998_v2_fixup();

	is_vq = of_device_is_compatible(pdev->dev.of_node,
						"qcom,gfxcc-hamster");
	if (is_vq)
		msm_gfxcc_hamster_fixup();

	rc = of_msm_clock_register(of_node, msm_clocks_gfxcc_8998,
					ARRAY_SIZE(msm_clocks_gfxcc_8998));
	if (rc)
		return rc;

	enable_gfx_crc();

	/*
	 * Force periph logic to be ON since after NAP, the value of the perf
	 * counter might be corrupted frequently.
	 */
	clk_set_flags(&gpucc_gfx3d_clk.c, CLKFLAG_RETAIN_PERIPH);

	/*
	 * Program the droop detector's gfx_pdn to 1'b1 in order to reduce
	 * leakage between the graphics and CX rails.
	 */
	regval = readl_relaxed(virt_base_gfx + GPUCC_GPU_DD_WRAP_CTRL);
	regval |= BIT(0);
	writel_relaxed(regval, virt_base_gfx + GPUCC_GPU_DD_WRAP_CTRL);

	dev_info(&pdev->dev, "Completed registering all GPU clocks\n");

	return 0;
}

static const struct of_device_id msm_clock_gfxcc_match_table[] = {
	{ .compatible = "qcom,gfxcc-8998" },
	{ .compatible = "qcom,gfxcc-8998-v2" },
	{ .compatible = "qcom,gfxcc-hamster" },
	{},
};

static struct platform_driver msm_clock_gfxcc_driver = {
	.probe = msm_gfxcc_8998_probe,
	.driver = {
		.name = "qcom,gfxcc-8998",
		.of_match_table = msm_clock_gfxcc_match_table,
		.owner = THIS_MODULE,
	},
};

int __init msm_gpucc_8998_init(void)
{
	return platform_driver_register(&msm_clock_gpucc_driver);
}
arch_initcall(msm_gpucc_8998_init);
