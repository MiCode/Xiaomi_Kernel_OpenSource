// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt) "[clkchk] " fmt

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/slab.h>
#include <linux/syscore_ops.h>

#if IS_ENABLED(CONFIG_MTK_DEVAPC) && !IS_ENABLED(CONFIG_DEVAPC_LEGACY)
#include <mt-plat/devapc_public.h>
#endif
#include "clkchk.h"
#include "clkdbg.h"
#define AEE_EXCP_CHECK_PLL_FAIL	0
#define CLKDBG_CCF_API_4_4	1
#define MAX_PLLS		32
#define MAX_MTCMOS			32
#define CLKCHK_OFF_MODE			0
#define CLKCHK_NOTICE_MODE		1
#define PLL_TYPE			0
#define MTCMOS_TYPE			1

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
static struct clk **off_plls;
static struct clk **notice_plls;
static struct clk **off_mtcmos;
static struct clk **notice_mtcmos;

#define clk_readl(addr)		readl(addr)
#define clk_writel(addr, val)	\
	do { writel(val, addr); wmb(); } while (0) /* sync write */

/*
 * for mtcmos debug
 */
bool is_valid_reg(void __iomem *addr)
{
#ifdef CONFIG_64BIT
	return ((u64)addr & 0xf0000000) != 0UL ||
			(((u64)addr >> 32U) & 0xf0000000) != 0UL;
#else
	return ((u32)addr & 0xf0000000) != 0U;
#endif
}

#if (defined(CONFIG_MACH_MT6877) \
			|| defined(CONFIG_MACH_MT6768) \
			|| defined(CONFIG_MACH_MT6781) \
			|| defined(CONFIG_MACH_MT6739))
#if IS_ENABLED(CONFIG_MTK_DEVAPC) && !IS_ENABLED(CONFIG_DEVAPC_LEGACY)
static void devapc_dump_regs(void)
{
	if (!clkchk_cfg || !clkchk_cfg->get_devapc_dump)
		return;
	clkchk_cfg->get_devapc_dump();
}

static struct devapc_vio_callbacks devapc_vio_handle = {
	.id = DEVAPC_SUBSYS_CLKMGR,
	.debug_dump = devapc_dump_regs,
};
#endif
#endif
/******************* TOPCKGEN Subsys *******************************/
static int get_vcore_opp(void)
{
	if (!clkchk_cfg || !clkchk_cfg->get_vcore_opp)
		return VCORE_NULL;

	return clkchk_cfg->get_vcore_opp();
}

static void warn_vcore(int opp, const char *clk_name, int rate, int id)
{
	int vf_opp;

	if (!clkchk_cfg || !clkchk_cfg->get_vf_opp)
		return;

	vf_opp = clkchk_cfg->get_vf_opp(id, opp);
	if ((opp >= 0) && (id >= 0) && (vf_opp > 0) &&
			((rate/1000) > vf_opp)) {
		pr_notice("%s Choose %d FAIL!!!![MAX(%d/%d): %d]\r\n",
				clk_name, rate/1000, id, opp,
				vf_opp);

		BUG_ON(1);
	}
}

static int mtk_mux2id(const char **mux_name)
{
	int i = 0;

	if (!clkchk_cfg || !clkchk_cfg->get_vf_name
			|| !clkchk_cfg->get_vf_num)
		return -1;

	for (i = 0; clkchk_cfg->get_vf_num(); i++) {
		if (strcmp(*mux_name, clkchk_cfg->get_vf_name(i)) == 0)
			return i;
	}

	return -2;
}

/* The clocks have a mechanism for synchronizing rate changes. */
static int mtk_clk_rate_change(struct notifier_block *nb,
					  unsigned long flags, void *data)
{
	struct clk_notifier_data *ndata = data;
	struct clk_hw *hw = __clk_get_hw(ndata->clk);
	const char *clk_name = __clk_get_name(hw->clk);
	int vcore_opp = get_vcore_opp();
	if (!hw) {
		pr_notice("%s: hw is NULL", __func__);
		return NOTIFY_BAD;
	}

	if (flags == PRE_RATE_CHANGE && clk_name) {
		warn_vcore(vcore_opp, clk_name,
			ndata->new_rate, mtk_mux2id(&clk_name));
	}
	return NOTIFY_OK;
}

static struct notifier_block mtk_clk_notifier = {
	.notifier_call = mtk_clk_rate_change,
};

static void mtk_clk_check_muxes(void)
{
	struct clk *clk;
	int i;

	if (!clkchk_cfg || !clkchk_cfg->get_vf_name
			|| !clkchk_cfg->get_vf_num)
		return;

	for (i = 0; i < clkchk_cfg->get_vf_num(); i++) {
		const char *name = clkchk_cfg->get_vf_name(i);

		if (!name)
			continue;

		pr_notice("name: %s\n", name);
		clk = __clk_lookup(name);
		clk_notifier_register(clk, &mtk_clk_notifier);
	}
}

void __init clkchk_swcg_init(struct pg_check_swcg *swcg)
{
	if (!swcg)
		return;

	while (swcg->name) {
		struct clk *c = __clk_lookup(swcg->name);

		if (IS_ERR_OR_NULL(c))
			pr_notice("[%17s: NULL]\n", swcg->name);
		else
			swcg->c = c;
		swcg++;
	}
}

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
	const char * const *off_pn;
	const char *fix_clk = "clk26m";
    if (!clkchk_cfg)
        return;

	pr_warn("enabled clks:\n");
	off_pn = clkchk_cfg->off_pll_names;

	for (; *cn != NULL; cn++) {
		int valid = 0;
		struct clk *c = __clk_lookup(*cn);
		struct clk_hw *c_hw = __clk_get_hw(c);
		struct clk_hw *p_hw;
		const char *c_name;
		const char *p_name;
		const char * const *pn;

		if (IS_ERR_OR_NULL(c) || c_hw == NULL)
			continue;

		if (!__clk_get_enable_count(c))
			continue;
		p_hw = clk_hw_get_parent(c_hw);
		c_name = clk_hw_get_name(c_hw);
		p_name = p_hw ? clk_hw_get_name(p_hw) : 0;
		while (p_name && strcmp(p_name, fix_clk)) {
			struct clk_hw *p_hw_temp;

			p_hw_temp = clk_hw_get_parent(p_hw);
			p_name = p_hw_temp ? clk_hw_get_name(p_hw_temp) : 0;
			if (p_name && strcmp(p_name, fix_clk))
				p_hw = p_hw_temp;
			else if (p_name && !strcmp(p_name, fix_clk)) {
				c_name = clk_hw_get_name(p_hw);
				break;
			}
		}
		for (pn = off_pn; *pn && c_name; pn++)
			if (!strncmp(c_name, *pn, 10)) {
				valid++;
				break;
			}

		if (!valid)
			continue;

		p_hw = clk_hw_get_parent(c_hw);
		pr_warn("[%-17s: %8s, %3d, %3d, %10ld, %17s]\n",
			clk_hw_get_name(c_hw),
			ccf_state(c_hw),
			clk_hw_is_prepared(c_hw),
			__clk_get_enable_count(c),
			clk_hw_get_rate(c_hw),
			p_hw != NULL ? clk_hw_get_name(p_hw) : "- ");
	}
}
void dump_enabled_clks_once(void)
{
	static bool first_flag = true;

	if (first_flag) {
		first_flag = false;
		print_enabled_clks();
	}
}

static int __clkchk_clk_init(struct clk **clks, const char * const *name, u32 len)
{
	struct clk **c;

	if (!clkchk_cfg || !name)
		return -EINVAL;

	if (!clks[0]) {
		struct clk **end = clks + len - 1;

		for (c = clks; *name != NULL && c < end; name++, c++)
			*c = __clk_lookup(*name);

		return 0;
	}

	return -EINVAL;
}



static void __clkchk_clk_internal(struct clk **clks, unsigned int mode,
			unsigned int type)
{
	struct clk **c;
	int invalid = 0;

	if (!clkchk_cfg)
		return;

	for (c = clks; *c != NULL; c++) {
		struct clk_hw *c_hw = __clk_get_hw(*c);

		if (c_hw == NULL)
			continue;

		if (!clk_hw_is_prepared(c_hw) && !clk_hw_is_enabled(c_hw))
			continue;

		pr_notice("suspend warning[0m: %s is on\n",
				clk_hw_get_name(c_hw));

		invalid++;
	}

	if (invalid == 0)
		return;

	/* invalid. output debug info */
	if (type == PLL_TYPE)
		print_enabled_clks();

	if (mode == CLKCHK_OFF_MODE) {
#if AEE_EXCP_CHECK_PLL_FAIL
		if (clkchk_cfg->aee_excp_on_fail) {
			if (type == PLL_TYPE)
				aee_kernel_exception("clkchk",
						"unclosed PLL: %s\n", buf);
			if (type == MTCMOS_TYPE)
				aee_kernel_warning("clkchk",
					"@%s():%d, MTCMOS are not off\n",
					__func__, __LINE__);
		}
#endif

		if (clkchk_cfg->bug_on_fail)
			BUG_ON(true);

		if (clkchk_cfg->warn_on_fail)
			WARN_ON(true);
	}
}



static void check_pll_off(void)
{
	__clkchk_clk_internal(off_plls, CLKCHK_OFF_MODE, PLL_TYPE);
}

static void check_pll_notice(void)
{
	__clkchk_clk_internal(notice_plls, CLKCHK_NOTICE_MODE, PLL_TYPE);
}

static void check_mtcmos_off(void)
{
	__clkchk_clk_internal(off_mtcmos, CLKCHK_OFF_MODE, MTCMOS_TYPE);
}

static void check_mtcmos_notice(void)
{
	__clkchk_clk_internal(notice_mtcmos, CLKCHK_NOTICE_MODE, MTCMOS_TYPE);
}
static int clkchk_syscore_suspend(void)
{
	check_pll_off();
	check_pll_notice();
	check_mtcmos_off();
	check_mtcmos_notice();

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
	int ret1 = 0, ret2 = 0, ret3 = 0, ret4 = 0;

	if (cfg == NULL || cfg->compatible == NULL
		|| cfg->all_clk_names == NULL || cfg->off_pll_names == NULL) {
		pr_notice("Invalid clkchk_cfg.\n");
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

	off_plls = kcalloc(MAX_PLLS, sizeof(struct clk *), GFP_KERNEL);
	notice_plls = kcalloc(MAX_PLLS, sizeof(struct clk *), GFP_KERNEL);
	off_mtcmos = kcalloc(MAX_MTCMOS, sizeof(struct clk *), GFP_KERNEL);
	notice_mtcmos = kcalloc(MAX_MTCMOS, sizeof(struct clk *), GFP_KERNEL);
	ret1 = __clkchk_clk_init(off_plls, clkchk_cfg->off_pll_names,
			MAX_PLLS);
	ret2 = __clkchk_clk_init(notice_plls, clkchk_cfg->notice_pll_names,
			MAX_PLLS);
	ret3 = __clkchk_clk_init(off_mtcmos, clkchk_cfg->off_mtcmos_names,
			MAX_MTCMOS);
	ret4 = __clkchk_clk_init(notice_mtcmos, clkchk_cfg->notice_mtcmos_names,
			MAX_MTCMOS);
	if (!ret1 && !ret2 && !ret3 && !ret4)
		register_syscore_ops(&clkchk_syscore_ops);
	else
		pr_notice("clk register_syscore_ops fail\n");

#if IS_ENABLED(CONFIG_MTK_DEVAPC) && !IS_ENABLED(CONFIG_DEVAPC_LEGACY)
	register_devapc_vio_callback(&devapc_vio_handle);
#endif

	mtk_clk_check_muxes();

	return 0;
}
