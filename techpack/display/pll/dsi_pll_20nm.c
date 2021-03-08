// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2014-2019, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/clk/msm-clk-provider.h>
#include <linux/clk/msm-clk.h>
#include <linux/workqueue.h>
#include <linux/clk/msm-clock-generic.h>
#include <dt-bindings/clock/msm-clocks-8994.h>

#include "pll_drv.h"
#include "dsi_pll.h"

#define VCO_DELAY_USEC		1

static const struct clk_ops bypass_lp_div_mux_clk_ops;
static const struct clk_ops pixel_clk_src_ops;
static const struct clk_ops byte_clk_src_ops;
static const struct clk_ops ndiv_clk_ops;

static const struct clk_ops shadow_pixel_clk_src_ops;
static const struct clk_ops shadow_byte_clk_src_ops;
static const struct clk_ops clk_ops_gen_mux_dsi;

static int vco_set_rate_20nm(struct clk *c, unsigned long rate)
{
	int rc;
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);
	struct mdss_pll_resources *dsi_pll_res = vco->priv;

	rc = mdss_pll_resource_enable(dsi_pll_res, true);
	if (rc) {
		pr_err("Failed to enable mdss dsi pll resources\n");
		return rc;
	}

	pr_debug("Cancel pending pll off work\n");
	cancel_work_sync(&dsi_pll_res->pll_off);
	rc = pll_20nm_vco_set_rate(vco, rate);

	mdss_pll_resource_enable(dsi_pll_res, false);
	return rc;
}

static int pll1_vco_set_rate_20nm(struct clk *c, unsigned long rate)
{
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);
	struct mdss_pll_resources *pll_res = vco->priv;

	mdss_pll_resource_enable(pll_res, true);
	__dsi_pll_disable(pll_res->pll_base);
	mdss_pll_resource_enable(pll_res, false);

	pr_debug("Configuring PLL1 registers.\n");

	return 0;
}

static int shadow_vco_set_rate_20nm(struct clk *c, unsigned long rate)
{
	int rc;
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);
	struct mdss_pll_resources *dsi_pll_res = vco->priv;

	if (!dsi_pll_res->resource_enable) {
		pr_err("PLL resources disabled. Dynamic fps invalid\n");
		return -EINVAL;
	}

	rc = shadow_pll_20nm_vco_set_rate(vco, rate);

	return rc;
}

/* Op structures */

static const struct clk_ops pll1_clk_ops_dsi_vco = {
	.set_rate = pll1_vco_set_rate_20nm,
};

static const struct clk_ops clk_ops_dsi_vco = {
	.set_rate = vco_set_rate_20nm,
	.round_rate = pll_20nm_vco_round_rate,
	.handoff = pll_20nm_vco_handoff,
	.prepare = pll_20nm_vco_prepare,
	.unprepare = pll_20nm_vco_unprepare,
};

static struct clk_div_ops fixed_hr_oclk2_div_ops = {
	.set_div = fixed_hr_oclk2_set_div,
	.get_div = fixed_hr_oclk2_get_div,
};

static struct clk_div_ops ndiv_ops = {
	.set_div = ndiv_set_div,
	.get_div = ndiv_get_div,
};

static struct clk_div_ops hr_oclk3_div_ops = {
	.set_div = hr_oclk3_set_div,
	.get_div = hr_oclk3_get_div,
};

static struct clk_mux_ops bypass_lp_div_mux_ops = {
	.set_mux_sel = set_bypass_lp_div_mux_sel,
	.get_mux_sel = get_bypass_lp_div_mux_sel,
};

static const struct clk_ops shadow_clk_ops_dsi_vco = {
	.set_rate = shadow_vco_set_rate_20nm,
	.round_rate = pll_20nm_vco_round_rate,
	.handoff = pll_20nm_vco_handoff,
};

static struct clk_div_ops shadow_fixed_hr_oclk2_div_ops = {
	.set_div = shadow_fixed_hr_oclk2_set_div,
	.get_div = fixed_hr_oclk2_get_div,
};

static struct clk_div_ops shadow_ndiv_ops = {
	.set_div = shadow_ndiv_set_div,
	.get_div = ndiv_get_div,
};

static struct clk_div_ops shadow_hr_oclk3_div_ops = {
	.set_div = shadow_hr_oclk3_set_div,
	.get_div = hr_oclk3_get_div,
};

static struct clk_mux_ops shadow_bypass_lp_div_mux_ops = {
	.set_mux_sel = set_shadow_bypass_lp_div_mux_sel,
	.get_mux_sel = get_bypass_lp_div_mux_sel,
};

static struct clk_mux_ops mdss_byte_mux_ops = {
	.set_mux_sel = set_mdss_byte_mux_sel,
	.get_mux_sel = get_mdss_byte_mux_sel,
};

static struct clk_mux_ops mdss_pixel_mux_ops = {
	.set_mux_sel = set_mdss_pixel_mux_sel,
	.get_mux_sel = get_mdss_pixel_mux_sel,
};

static struct dsi_pll_vco_clk mdss_dsi1_vco_clk_src = {
	.c = {
		.dbg_name = "mdss_dsi1_vco_clk_src",
		.ops = &pll1_clk_ops_dsi_vco,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(mdss_dsi1_vco_clk_src.c),
	},
};

static struct dsi_pll_vco_clk dsi_vco_clk_8994 = {
	.ref_clk_rate = 19200000,
	.min_rate = 300000000,
	.max_rate = 1500000000,
	.pll_en_seq_cnt = 1,
	.pll_enable_seqs[0] = pll_20nm_vco_enable_seq,
	.c = {
		.dbg_name = "dsi_vco_clk_8994",
		.ops = &clk_ops_dsi_vco,
		CLK_INIT(dsi_vco_clk_8994.c),
	},
};

static struct dsi_pll_vco_clk shadow_dsi_vco_clk_8994 = {
	.ref_clk_rate = 19200000,
	.min_rate = 300000000,
	.max_rate = 1500000000,
	.c = {
		.dbg_name = "shadow_dsi_vco_clk_8994",
		.ops = &shadow_clk_ops_dsi_vco,
		CLK_INIT(shadow_dsi_vco_clk_8994.c),
	},
};

static struct div_clk ndiv_clk_8994 = {
	.data = {
		.max_div = 15,
		.min_div = 1,
	},
	.ops = &ndiv_ops,
	.c = {
		.parent = &dsi_vco_clk_8994.c,
		.dbg_name = "ndiv_clk_8994",
		.ops = &ndiv_clk_ops,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(ndiv_clk_8994.c),
	},
};

static struct div_clk shadow_ndiv_clk_8994 = {
	.data = {
		.max_div = 15,
		.min_div = 1,
	},
	.ops = &shadow_ndiv_ops,
	.c = {
		.parent = &shadow_dsi_vco_clk_8994.c,
		.dbg_name = "shadow_ndiv_clk_8994",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(shadow_ndiv_clk_8994.c),
	},
};

static struct div_clk indirect_path_div2_clk_8994 = {
	.data = {
		.div = 2,
		.min_div = 2,
		.max_div = 2,
	},
	.c = {
		.parent = &ndiv_clk_8994.c,
		.dbg_name = "indirect_path_div2_clk_8994",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(indirect_path_div2_clk_8994.c),
	},
};

static struct div_clk shadow_indirect_path_div2_clk_8994 = {
	.data = {
		.div = 2,
		.min_div = 2,
		.max_div = 2,
	},
	.c = {
		.parent = &shadow_ndiv_clk_8994.c,
		.dbg_name = "shadow_indirect_path_div2_clk_8994",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(shadow_indirect_path_div2_clk_8994.c),
	},
};

static struct div_clk hr_oclk3_div_clk_8994 = {
	.data = {
		.max_div = 255,
		.min_div = 1,
	},
	.ops = &hr_oclk3_div_ops,
	.c = {
		.parent = &dsi_vco_clk_8994.c,
		.dbg_name = "hr_oclk3_div_clk_8994",
		.ops = &pixel_clk_src_ops,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(hr_oclk3_div_clk_8994.c),
	},
};

static struct div_clk shadow_hr_oclk3_div_clk_8994 = {
	.data = {
		.max_div = 255,
		.min_div = 1,
	},
	.ops = &shadow_hr_oclk3_div_ops,
	.c = {
		.parent = &shadow_dsi_vco_clk_8994.c,
		.dbg_name = "shadow_hr_oclk3_div_clk_8994",
		.ops = &shadow_pixel_clk_src_ops,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(shadow_hr_oclk3_div_clk_8994.c),
	},
};

static struct div_clk pixel_clk_src = {
	.data = {
		.div = 2,
		.min_div = 2,
		.max_div = 2,
	},
	.c = {
		.parent = &hr_oclk3_div_clk_8994.c,
		.dbg_name = "pixel_clk_src",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(pixel_clk_src.c),
	},
};

static struct div_clk shadow_pixel_clk_src = {
	.data = {
		.div = 2,
		.min_div = 2,
		.max_div = 2,
	},
	.c = {
		.parent = &shadow_hr_oclk3_div_clk_8994.c,
		.dbg_name = "shadow_pixel_clk_src",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(shadow_pixel_clk_src.c),
	},
};

static struct mux_clk bypass_lp_div_mux_8994 = {
	.num_parents = 2,
	.parents = (struct clk_src[]){
		{&dsi_vco_clk_8994.c, 0},
		{&indirect_path_div2_clk_8994.c, 1},
	},
	.ops = &bypass_lp_div_mux_ops,
	.c = {
		.parent = &dsi_vco_clk_8994.c,
		.dbg_name = "bypass_lp_div_mux_8994",
		.ops = &bypass_lp_div_mux_clk_ops,
		CLK_INIT(bypass_lp_div_mux_8994.c),
	},
};

static struct mux_clk shadow_bypass_lp_div_mux_8994 = {
	.num_parents = 2,
	.parents = (struct clk_src[]){
		{&shadow_dsi_vco_clk_8994.c, 0},
		{&shadow_indirect_path_div2_clk_8994.c, 1},
	},
	.ops = &shadow_bypass_lp_div_mux_ops,
	.c = {
		.parent = &shadow_dsi_vco_clk_8994.c,
		.dbg_name = "shadow_bypass_lp_div_mux_8994",
		.ops = &clk_ops_gen_mux,
		CLK_INIT(shadow_bypass_lp_div_mux_8994.c),
	},
};

static struct div_clk fixed_hr_oclk2_div_clk_8994 = {
	.ops = &fixed_hr_oclk2_div_ops,
	.data = {
		.min_div = 4,
		.max_div = 4,
	},
	.c = {
		.parent = &bypass_lp_div_mux_8994.c,
		.dbg_name = "fixed_hr_oclk2_div_clk_8994",
		.ops = &byte_clk_src_ops,
		CLK_INIT(fixed_hr_oclk2_div_clk_8994.c),
	},
};

static struct div_clk shadow_fixed_hr_oclk2_div_clk_8994 = {
	.ops = &shadow_fixed_hr_oclk2_div_ops,
	.data = {
		.min_div = 4,
		.max_div = 4,
	},
	.c = {
		.parent = &shadow_bypass_lp_div_mux_8994.c,
		.dbg_name = "shadow_fixed_hr_oclk2_div_clk_8994",
		.ops = &shadow_byte_clk_src_ops,
		CLK_INIT(shadow_fixed_hr_oclk2_div_clk_8994.c),
	},
};

static struct div_clk byte_clk_src = {
	.data = {
		.div = 2,
		.min_div = 2,
		.max_div = 2,
	},
	.c = {
		.parent = &fixed_hr_oclk2_div_clk_8994.c,
		.dbg_name = "byte_clk_src",
		.ops = &clk_ops_div,
		CLK_INIT(byte_clk_src.c),
	},
};

static struct div_clk shadow_byte_clk_src = {
	.data = {
		.div = 2,
		.min_div = 2,
		.max_div = 2,
	},
	.c = {
		.parent = &shadow_fixed_hr_oclk2_div_clk_8994.c,
		.dbg_name = "shadow_byte_clk_src",
		.ops = &clk_ops_div,
		CLK_INIT(shadow_byte_clk_src.c),
	},
};

static struct mux_clk mdss_pixel_clk_mux = {
	.num_parents = 2,
	.parents = (struct clk_src[]) {
		{&pixel_clk_src.c, 0},
		{&shadow_pixel_clk_src.c, 1},
	},
	.ops = &mdss_pixel_mux_ops,
	.c = {
		.parent = &pixel_clk_src.c,
		.dbg_name = "mdss_pixel_clk_mux",
		.ops = &clk_ops_gen_mux,
		CLK_INIT(mdss_pixel_clk_mux.c),
	}
};

static struct mux_clk mdss_byte_clk_mux = {
	.num_parents = 2,
	.parents = (struct clk_src[]) {
		{&byte_clk_src.c, 0},
		{&shadow_byte_clk_src.c, 1},
	},
	.ops = &mdss_byte_mux_ops,
	.c = {
		.parent = &byte_clk_src.c,
		.dbg_name = "mdss_byte_clk_mux",
		.ops = &clk_ops_gen_mux_dsi,
		CLK_INIT(mdss_byte_clk_mux.c),
	}
};

static struct clk_lookup mdss_dsi_pll_1_cc_8994[] = {
	CLK_LIST(mdss_dsi1_vco_clk_src),
};

static struct clk_lookup mdss_dsi_pllcc_8994[] = {
	CLK_LIST(mdss_pixel_clk_mux),
	CLK_LIST(mdss_byte_clk_mux),
	CLK_LIST(pixel_clk_src),
	CLK_LIST(byte_clk_src),
	CLK_LIST(fixed_hr_oclk2_div_clk_8994),
	CLK_LIST(bypass_lp_div_mux_8994),
	CLK_LIST(hr_oclk3_div_clk_8994),
	CLK_LIST(indirect_path_div2_clk_8994),
	CLK_LIST(ndiv_clk_8994),
	CLK_LIST(dsi_vco_clk_8994),
	CLK_LIST(shadow_pixel_clk_src),
	CLK_LIST(shadow_byte_clk_src),
	CLK_LIST(shadow_fixed_hr_oclk2_div_clk_8994),
	CLK_LIST(shadow_bypass_lp_div_mux_8994),
	CLK_LIST(shadow_hr_oclk3_div_clk_8994),
	CLK_LIST(shadow_indirect_path_div2_clk_8994),
	CLK_LIST(shadow_ndiv_clk_8994),
	CLK_LIST(shadow_dsi_vco_clk_8994),
};

static void dsi_pll_off_work(struct work_struct *work)
{
	struct mdss_pll_resources *pll_res;

	if (!work) {
		pr_err("pll_resource is invalid\n");
		return;
	}

	pr_debug("Starting PLL off Worker%s\n", __func__);

	pll_res = container_of(work, struct
			mdss_pll_resources, pll_off);

	mdss_pll_resource_enable(pll_res, true);
	__dsi_pll_disable(pll_res->pll_base);
	if (pll_res->pll_1_base)
		__dsi_pll_disable(pll_res->pll_1_base);
	mdss_pll_resource_enable(pll_res, false);
}

static int dsi_pll_regulator_notifier_call(struct notifier_block *self,
		unsigned long event, void *data)
{

	struct mdss_pll_resources *pll_res;

	if (!self) {
		pr_err("pll_resource is invalid\n");
		goto error;
	}

	pll_res = container_of(self, struct
			mdss_pll_resources, gdsc_cb);

	if (event & REGULATOR_EVENT_ENABLE) {
		pr_debug("Regulator ON event. Scheduling pll off worker\n");
		schedule_work(&pll_res->pll_off);
	}

	if (event & REGULATOR_EVENT_DISABLE)
		pr_debug("Regulator OFF event.\n");

error:
	return NOTIFY_OK;
}

int dsi_pll_clock_register_20nm(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res)
{
	int rc;
	struct dss_vreg *pll_reg;

	/*
	 * Set client data to mux, div and vco clocks.
	 * This needs to be done only for PLL0 since, that is the one in
	 * use.
	 **/
	if (!pll_res->index) {
		byte_clk_src.priv = pll_res;
		pixel_clk_src.priv = pll_res;
		bypass_lp_div_mux_8994.priv = pll_res;
		indirect_path_div2_clk_8994.priv = pll_res;
		ndiv_clk_8994.priv = pll_res;
		fixed_hr_oclk2_div_clk_8994.priv = pll_res;
		hr_oclk3_div_clk_8994.priv = pll_res;
		dsi_vco_clk_8994.priv = pll_res;

		shadow_byte_clk_src.priv = pll_res;
		shadow_pixel_clk_src.priv = pll_res;
		shadow_bypass_lp_div_mux_8994.priv = pll_res;
		shadow_indirect_path_div2_clk_8994.priv = pll_res;
		shadow_ndiv_clk_8994.priv = pll_res;
		shadow_fixed_hr_oclk2_div_clk_8994.priv = pll_res;
		shadow_hr_oclk3_div_clk_8994.priv = pll_res;
		shadow_dsi_vco_clk_8994.priv = pll_res;

		pll_res->vco_delay = VCO_DELAY_USEC;

		/* Set clock source operations */
		pixel_clk_src_ops = clk_ops_slave_div;
		pixel_clk_src_ops.prepare = dsi_pll_div_prepare;

		ndiv_clk_ops = clk_ops_div;
		ndiv_clk_ops.prepare = dsi_pll_div_prepare;

		byte_clk_src_ops = clk_ops_div;
		byte_clk_src_ops.prepare = dsi_pll_div_prepare;

		bypass_lp_div_mux_clk_ops = clk_ops_gen_mux;
		bypass_lp_div_mux_clk_ops.prepare = dsi_pll_mux_prepare;

		clk_ops_gen_mux_dsi = clk_ops_gen_mux;
		clk_ops_gen_mux_dsi.round_rate = parent_round_rate;
		clk_ops_gen_mux_dsi.set_rate = parent_set_rate;

		shadow_pixel_clk_src_ops = clk_ops_slave_div;
		shadow_pixel_clk_src_ops.prepare = dsi_pll_div_prepare;

		shadow_byte_clk_src_ops = clk_ops_div;
		shadow_byte_clk_src_ops.prepare = dsi_pll_div_prepare;
	} else {
		mdss_dsi1_vco_clk_src.priv = pll_res;
	}

	if ((pll_res->target_id == MDSS_PLL_TARGET_8994) ||
			(pll_res->target_id == MDSS_PLL_TARGET_8992)) {
		if (pll_res->index) {
			rc = of_msm_clock_register(pdev->dev.of_node,
					mdss_dsi_pll_1_cc_8994,
					ARRAY_SIZE(mdss_dsi_pll_1_cc_8994));
			if (rc) {
				pr_err("Clock register failed\n");
				rc = -EPROBE_DEFER;
			}
		} else {
			rc = of_msm_clock_register(pdev->dev.of_node,
				mdss_dsi_pllcc_8994,
				ARRAY_SIZE(mdss_dsi_pllcc_8994));
			if (rc) {
				pr_err("Clock register failed\n");
				rc = -EPROBE_DEFER;
			}
			pll_res->gdsc_cb.notifier_call =
				dsi_pll_regulator_notifier_call;
			INIT_WORK(&pll_res->pll_off, dsi_pll_off_work);

			pll_reg = mdss_pll_get_mp_by_reg_name(pll_res, "gdsc");
			if (pll_reg) {
				pr_debug("Registering for gdsc regulator events\n");
				if (regulator_register_notifier(pll_reg->vreg,
							&(pll_res->gdsc_cb)))
					pr_err("Regulator notification registration failed!\n");
			}
		}

	} else {
		pr_err("Invalid target ID\n");
		rc = -EINVAL;
	}

	if (!rc)
		pr_info("Registered DSI PLL clocks successfully\n");

	return rc;
}
