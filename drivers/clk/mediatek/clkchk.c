/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "[clkchk] " fmt

#include <linux/clk-provider.h>
#include <linux/syscore_ops.h>
#include "clkchk.h"

#define AEE_EXCP_CHECK_PLL_FAIL	0
#define CLKDBG_CCF_API_4_4	1
#define MAX_PLLS		32

#if AEE_EXCP_CHECK_PLL_FAIL
#include <mt-plat/aee.h>
#endif

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

static unsigned long clk_hw_get_rate(const struct clk_hw *hw)
{
	return __clk_get_rate(hw->clk);
}

static struct clk_hw *clk_hw_get_parent(const struct clk_hw *hw)
{
	return __clk_get_hw(clk_get_parent(hw->clk));
}

#endif /* !CLKDBG_CCF_API_4_4 */

static struct clkchk_cfg_t *clkchk_cfg;

static const char *ccf_state(struct clk_hw *hw)
{
	if (__clk_get_enable_count(hw->clk))
		return "enabled";

	if (clk_hw_is_prepared(hw))
		return "prepared";

	return "disabled";
}

static void print_enabled_clks(void)
{
	const char * const *cn = clkchk_cfg->all_clk_names;

	pr_warn("enabled clks:\n");

	for (; *cn != NULL; cn++) {
		struct clk *c = __clk_lookup(*cn);
		struct clk_hw *c_hw = __clk_get_hw(c);
		struct clk_hw *p_hw;

		if (IS_ERR_OR_NULL(c) || c_hw == NULL)
			continue;

		p_hw = clk_hw_get_parent(c_hw);

		if (p_hw == NULL)
			continue;

		if (!clk_hw_is_prepared(c_hw) &&
			__clk_get_enable_count(c) <= 0U)
			continue;

		pr_warn("[%-17s: %8s, %3d, %3d, %10ld, %17s]\n",
			clk_hw_get_name(c_hw),
			ccf_state(c_hw),
			clk_hw_is_prepared(c_hw),
			__clk_get_enable_count(c),
			clk_hw_get_rate(c_hw),
			clk_hw_get_name(p_hw));
	}
}

static void check_pll_off(void)
{
	static struct clk *off_plls[MAX_PLLS];

	struct clk **c;
	int invalid = 0;
	char buf[128] = {0};
	int n = 0;

	if (off_plls[0] == NULL) {
		const char * const *pn = clkchk_cfg->off_pll_names;
		struct clk **end = off_plls + MAX_PLLS - 1;

		for (c = off_plls; *pn != NULL && c < end; pn++, c++)
			*c = __clk_lookup(*pn);
	}

	for (c = off_plls; *c != NULL; c++) {
		struct clk_hw *c_hw = __clk_get_hw(*c);

		if (c_hw == NULL)
			continue;

		if (!clk_hw_is_prepared(c_hw) && !clk_hw_is_enabled(c_hw))
			continue;

		n += snprintf(buf + n, sizeof(buf) - (size_t)n, "%s ",
				clk_hw_get_name(c_hw));

		invalid++;
	}

	if (invalid == 0)
		return;

	/* invalid. output debug info */

	pr_warn("unexpected unclosed PLL: %s\n", buf);
	print_enabled_clks();

#if AEE_EXCP_CHECK_PLL_FAIL
	if (clkchk_cfg->aee_excp_on_fail)
		aee_kernel_exception("clkchk", "unclosed PLL: %s\n", buf);
#endif

	if (clkchk_cfg->warn_on_fail)
		WARN_ON(true);
}

static int clkchk_syscore_suspend(void)
{
	check_pll_off();

	return 0;
}

static void clkchk_syscore_resume(void)
{
}

static struct syscore_ops clkchk_syscore_ops = {
	.suspend = clkchk_syscore_suspend,
	.resume = clkchk_syscore_resume,
};

int clkchk_init(struct clkchk_cfg_t *cfg)
{
	const char * const *c;
	bool match = false;

	if (cfg == NULL || cfg->compatible == NULL
		|| cfg->all_clk_names == NULL || cfg->off_pll_names == NULL) {
		pr_warn("Invalid clkchk_cfg.\n");
		return -EINVAL;
	}

	clkchk_cfg = cfg;

	for (c = cfg->compatible; *c != NULL; c++) {
		if (of_machine_is_compatible(*c) != 0) {
			match = true;
			break;
		}
	}

	if (!match)
		return -ENODEV;

	register_syscore_ops(&clkchk_syscore_ops);

	return 0;
}
