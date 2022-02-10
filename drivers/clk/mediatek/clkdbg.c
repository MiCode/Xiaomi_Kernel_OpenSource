// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt) "[clkdbg] " fmt

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <linux/module.h>
#include <linux/version.h>

#include "clkdbg.h"

#if defined(CONFIG_PM_DEBUG)
#define CLKDBG_PM_DOMAIN		1
#else
#define CLKDBG_PM_DOMAIN		0
#endif
#define CLKDBG_PM_DOMAIN_API_4_9	1
#define CLKDBG_PM_DOMAIN_API_4_19	1
#define CLKDBG_CCF_API_4_4		1
#define CLKDBG_HACK_CLK			0
#define CLKDBG_HACK_CLK_CORE		1


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

static unsigned int clk_hw_get_num_parents(const struct clk_hw *hw)
{
	return __clk_get_num_parents(hw->clk);
}

static struct clk_hw *clk_hw_get_parent_by_index(const struct clk_hw *hw,
					  unsigned int index)
{
	return __clk_get_hw(clk_get_parent_by_index(hw->clk, index));
}

#endif /* !CLKDBG_CCF_API_4_4 */

#if CLKDBG_HACK_CLK

#include <linux/clk-private.h>

static bool clk_hw_is_on(struct clk_hw *hw)
{
	const struct clk_ops *ops = hw->clk->ops;

	if (ops->is_enabled)
		return clk_hw_is_enabled(hw);
	else if (ops->is_prepared)
		return clk_hw_is_prepared(hw);
	return clk_hw_is_enabled(hw) || clk_hw_is_prepared(hw);
}

#elif CLKDBG_HACK_CLK_CORE

struct clk_core {
	const char		*name;
	const struct clk_ops	*ops;
	struct clk_hw		*hw;
};

static bool clk_hw_is_on(struct clk_hw *hw)
{
	const struct clk_ops *ops = hw->core->ops;

	if (ops->is_enabled)
		return clk_hw_is_enabled(hw);
	else if (ops->is_prepared)
		return clk_hw_is_prepared(hw);
	return clk_hw_is_enabled(hw) || clk_hw_is_prepared(hw);
}

#else

static bool clk_hw_is_on(struct clk_hw *hw)
{
	return __clk_get_enable_count(hw->clk) || clk_hw_is_prepared(hw);
}

#endif /* !CLKDBG_HACK_CLK && !CLKDBG_HACK_CLK_CORE */

static const struct clkdbg_ops *clkdbg_ops;

void set_clkdbg_ops(const struct clkdbg_ops *ops)
{
	clkdbg_ops = ops;
}
EXPORT_SYMBOL(set_clkdbg_ops);

static const struct fmeter_clk *get_all_fmeter_clks(void)
{
	if (clkdbg_ops == NULL || clkdbg_ops->get_all_fmeter_clks  == NULL)
		return NULL;

	return clkdbg_ops->get_all_fmeter_clks();
}

static void *prepare_fmeter(void)
{
	if (clkdbg_ops == NULL || clkdbg_ops->prepare_fmeter == NULL)
		return NULL;

	return clkdbg_ops->prepare_fmeter();
}

static void unprepare_fmeter(void *data)
{
	if (clkdbg_ops == NULL || clkdbg_ops->unprepare_fmeter == NULL)
		return;

	clkdbg_ops->unprepare_fmeter(data);
}

static u32 fmeter_freq(const struct fmeter_clk *fclk)
{
	if (clkdbg_ops == NULL || clkdbg_ops->fmeter_freq == NULL)
		return 0;

	return clkdbg_ops->fmeter_freq(fclk);
}

static const struct regname *get_all_regnames(void)
{
	if (clkdbg_ops == NULL || clkdbg_ops->get_all_regnames == NULL)
		return NULL;

	return clkdbg_ops->get_all_regnames();
}

static const char * const *get_all_clk_names(void)
{
	if (clkdbg_ops == NULL || clkdbg_ops->get_all_clk_names == NULL)
		return NULL;

	return clkdbg_ops->get_all_clk_names();
}

static const char * const *get_pwr_names(void)
{
	static const char * const default_pwr_names[] = {
		[0]  = "(MD)",
		[1]  = "(CONN)",
		[2]  = "(DDRPHY)",
		[3]  = "(DISP)",
		[4]  = "(MFG)",
		[5]  = "(ISP)",
		[6]  = "(INFRA)",
		[7]  = "(VDEC)",
		[8]  = "(CPU, CA7_CPUTOP)",
		[9]  = "(FC3, CA7_CPU0, CPUTOP)",
		[10] = "(FC2, CA7_CPU1, CPU3)",
		[11] = "(FC1, CA7_CPU2, CPU2)",
		[12] = "(FC0, CA7_CPU3, CPU1)",
		[13] = "(MCUSYS, CA7_DBG, CPU0)",
		[14] = "(MCUSYS, VEN, BDP)",
		[15] = "(CA15_CPUTOP, ETH, MCUSYS)",
		[16] = "(CA15_CPU0, HIF)",
		[17] = "(CA15_CPU1, CA15-CX0, INFRA_MISC)",
		[18] = "(CA15_CPU2, CA15-CX1)",
		[19] = "(CA15_CPU3, CA15-CPU0)",
		[20] = "(VEN2, MJC, CA15-CPU1)",
		[21] = "(VEN, CA15-CPUTOP)",
		[22] = "(MFG_2D)",
		[23] = "(MFG_ASYNC, DBG)",
		[24] = "(AUDIO, MFG_2D)",
		[25] = "(USB, VCORE_PDN, MFG_ASYNC)",
		[26] = "(ARMPLL_DIV, CPUTOP_SRM_SLPB)",
		[27] = "(MD2, CPUTOP_SRM_PDN)",
		[28] = "(CPU3_SRM_PDN)",
		[29] = "(CPU2_SRM_PDN)",
		[30] = "(CPU1_SRM_PDN)",
		[31] = "(CPU0_SRM_PDN)",
	};

	if (clkdbg_ops == NULL || clkdbg_ops->get_pwr_names == NULL)
		return default_pwr_names;

	return clkdbg_ops->get_pwr_names();
}

static void setup_provider_clk(struct provider_clk *pvdck)
{
	if (clkdbg_ops == NULL || clkdbg_ops->setup_provider_clk == NULL)
		return;

	clkdbg_ops->setup_provider_clk(pvdck);
}

static bool is_valid_reg(void __iomem *addr)
{
#ifdef CONFIG_64BIT
	return ((u64)addr & 0xf0000000) != 0UL ||
			(((u64)addr >> 32U) & 0xf0000000) != 0UL;
#else
	return ((u32)addr & 0xf0000000) != 0U;
#endif
}

enum clkdbg_opt {
	CLKDBG_EN_SUSPEND_SAVE_1,
	CLKDBG_EN_SUSPEND_SAVE_2,
	CLKDBG_EN_SUSPEND_SAVE_3,
	CLKDBG_EN_LOG_SAVE_POINTS,
};

static u32 clkdbg_flags;

static void set_clkdbg_flag(enum clkdbg_opt opt)
{
	clkdbg_flags |= BIT(opt);
}

static void clr_clkdbg_flag(enum clkdbg_opt opt)
{
	clkdbg_flags &= ~BIT(opt);
}

static bool has_clkdbg_flag(enum clkdbg_opt opt)
{
	return (clkdbg_flags & BIT(opt)) != 0U;
}

typedef void (*fn_fclk_freq_proc)(const struct fmeter_clk *fclk,
					u32 freq, void *data);

static void proc_all_fclk_freq(fn_fclk_freq_proc proc, void *data)
{
	void *fmeter_data;
	const struct fmeter_clk *fclk;

	fclk = get_all_fmeter_clks();

	if (fclk == NULL || proc == NULL)
		return;

	fmeter_data = prepare_fmeter();

	for (; fclk->type != FT_NULL; fclk++) {
		u32 freq;

		freq = fmeter_freq(fclk);
		proc(fclk, freq, data);
	}

	unprepare_fmeter(fmeter_data);
}

static void print_fclk_freq(const struct fmeter_clk *fclk, u32 freq, void *data)
{
	pr_info("%2d: %-29s: %u\n", fclk->id, fclk->name, freq);
}

void print_fmeter_all(void)
{
	proc_all_fclk_freq(print_fclk_freq, NULL);
}
EXPORT_SYMBOL(print_fmeter_all);

static void seq_print_fclk_freq(const struct fmeter_clk *fclk,
				u32 freq, void *data)
{
	struct seq_file *s = data;

	seq_printf(s, "%2d: %-29s: %u\n", fclk->id, fclk->name, freq);
}

static int seq_print_fmeter_all(struct seq_file *s, void *v)
{
	proc_all_fclk_freq(seq_print_fclk_freq, s);

	return 0;
}

typedef void (*fn_regname_proc)(const struct regname *rn, void *data);

static void proc_all_regname(fn_regname_proc proc, void *data)
{
	const struct regname *rn = get_all_regnames();

	if (rn == NULL)
		return;

	for (; rn->base != NULL; rn++)
		proc(rn, data);
}

static void print_reg(const struct regname *rn, void *data)
{
	if (!is_valid_reg(ADDR(rn)))
		return;

	pr_info("%-21s: [0x%08x][0x%p] = 0x%08x\n",
		rn->name, PHYSADDR(rn), ADDR(rn), clk_readl(ADDR(rn)));
}

void print_regs(void)
{
	proc_all_regname(print_reg, NULL);
}
EXPORT_SYMBOL(print_regs);

static void seq_print_reg(const struct regname *rn, void *data)
{
	struct seq_file *s = data;

	if (!is_valid_reg(ADDR(rn)))
		return;

	seq_printf(s, "%-21s: [0x%08x][0x%p] = 0x%08x\n",
		rn->name, PHYSADDR(rn), ADDR(rn), clk_readl(ADDR(rn)));
}

static int seq_print_regs(struct seq_file *s, void *v)
{
	proc_all_regname(seq_print_reg, s);

	return 0;
}

static void print_reg2(const struct regname *rn, void *data)
{
	if (!is_valid_reg(ADDR(rn)))
		return;

	pr_info("%-21s: [0x%08x][0x%p] = 0x%08x\n",
		rn->name, PHYSADDR(rn), ADDR(rn), clk_readl(ADDR(rn)));

	msleep(20);
}

static int clkdbg_dump_regs2(struct seq_file *s, void *v)
{
	proc_all_regname(print_reg2, s);

	return 0;
}

static u32 read_spm_pwr_status(void)
{
	static void __iomem *scpsys_base;

	if (scpsys_base == NULL)
		scpsys_base = ioremap(0x10006000, PAGE_SIZE);

	return clk_readl(scpsys_base + 0x60c);
}

static bool clk_hw_pwr_is_on(struct clk_hw *c_hw,
			u32 spm_pwr_status, u32 pwr_mask)
{
	if ((spm_pwr_status & pwr_mask) != pwr_mask)
		return false;

	return clk_hw_is_on(c_hw);
}

static bool pvdck_pwr_is_on(struct provider_clk *pvdck, u32 spm_pwr_status)
{
	struct clk *c = pvdck->ck;
	struct clk_hw *c_hw = __clk_get_hw(c);

	return clk_hw_pwr_is_on(c_hw, spm_pwr_status, pvdck->pwr_mask);
}

static bool pvdck_is_on(struct provider_clk *pvdck)
{
	u32 spm_pwr_status = 0;

	if (pvdck->pwr_mask != 0U)
		spm_pwr_status = read_spm_pwr_status();

	return pvdck_pwr_is_on(pvdck, spm_pwr_status);
}

static const char *ccf_state(struct clk_hw *hw)
{
	if (__clk_get_enable_count(hw->clk))
		return "enabled";

	if (clk_hw_is_prepared(hw))
		return "prepared";

	return "disabled";
}

static void dump_clk_state(const char *clkname, struct seq_file *s)
{
	struct clk *c = __clk_lookup(clkname);
	struct clk *p = IS_ERR_OR_NULL(c) ? NULL : clk_get_parent(c);
	struct clk_hw *c_hw = __clk_get_hw(c);
	struct clk_hw *p_hw = __clk_get_hw(p);

	if (IS_ERR_OR_NULL(c)) {
		seq_printf(s, "[%17s: NULL]\n", clkname);
		return;
	}

	seq_printf(s, "[%-17s: %8s, %3d, %3d, %10ld, %17s]\n",
		clk_hw_get_name(c_hw),
		ccf_state(c_hw),
		clk_hw_is_prepared(c_hw),
		__clk_get_enable_count(c),
		clk_hw_get_rate(c_hw),
		p != NULL ? clk_hw_get_name(p_hw) : "- ");
}

static int clkdbg_dump_state_all(struct seq_file *s, void *v)
{
	const char * const *ckn = get_all_clk_names();

	if (ckn == NULL)
		return 0;

	for (; *ckn != NULL; ckn++)
		dump_clk_state(*ckn, s);

	return 0;
}

static const char *get_provider_name(struct device_node *node, u32 *cells)
{
	const char *name;
	const char *p;
	u32 cc = 0;

	if (of_property_read_u32(node, "#clock-cells", &cc) != 0)
		cc = 0;

	if (cells != NULL)
		*cells = cc;

	if (cc == 0U) {
		if (of_property_read_string(node,
				"clock-output-names", &name) < 0)
			name = node->name;

		return name;
	}

	if (of_property_read_string(node, "compatible", &name) < 0)
		name = node->name;

	p = strchr(name, (int)'-');

	if (p != NULL)
		return p + 1;
	else
		return name;
}

struct provider_clk *get_all_provider_clks(void)
{
	static struct provider_clk provider_clks[512];
	struct device_node *node = NULL;
	unsigned int n = 0;

	if (provider_clks[0].ck != NULL)
		return provider_clks;

	do {
		const char *node_name;
		u32 cells;

		node = of_find_node_with_property(node, "#clock-cells");

		if (node == NULL)
			break;

		node_name = get_provider_name(node, &cells);

		if (cells == 0U) {
			struct clk *ck = __clk_lookup(node_name);

			if (IS_ERR_OR_NULL(ck))
				continue;

			provider_clks[n].ck = ck;
			setup_provider_clk(&provider_clks[n]);
			++n;
		} else {
			unsigned int i;

			for (i = 0; i < 256; i++) {
				struct of_phandle_args pa;
				struct clk *ck;

				pa.np = node;
				pa.args[0] = i;
				pa.args_count = 1;
				ck = of_clk_get_from_provider(&pa);

				if (PTR_ERR(ck) == -EINVAL)
					break;
				else if (IS_ERR_OR_NULL(ck))
					continue;

				provider_clks[n].ck = ck;
				provider_clks[n].idx = i;
				provider_clks[n].provider_name = node_name;
				setup_provider_clk(&provider_clks[n]);
				++n;
			}
		}
	} while (node != NULL);

	return provider_clks;
}
EXPORT_SYMBOL(get_all_provider_clks);

static void dump_provider_clk(struct provider_clk *pvdck, struct seq_file *s)
{
	struct clk *c = pvdck->ck;
	struct clk *p = IS_ERR_OR_NULL(c) ? NULL : clk_get_parent(c);
	struct clk_hw *c_hw = __clk_get_hw(c);
	struct clk_hw *p_hw = __clk_get_hw(p);

	seq_printf(s, "[%10s: %-17s: %3s, %3d, %3d, %10ld, %17s]\n",
		pvdck->provider_name != NULL ? pvdck->provider_name : "/ ",
		clk_hw_get_name(c_hw),
		pvdck_is_on(pvdck) ? "ON" : "off",
		clk_hw_is_prepared(c_hw),
		__clk_get_enable_count(c),
		clk_hw_get_rate(c_hw),
		p != NULL ? clk_hw_get_name(p_hw) : "- ");
}

static int clkdbg_dump_provider_clks(struct seq_file *s, void *v)
{
	struct provider_clk *pvdck = get_all_provider_clks();

	for (; pvdck->ck != NULL; pvdck++)
		dump_provider_clk(pvdck, s);

	return 0;
}

static void dump_provider_mux(struct provider_clk *pvdck, struct seq_file *s)
{
	unsigned int i;
	struct clk *c = pvdck->ck;
	struct clk_hw *c_hw = __clk_get_hw(c);
	unsigned int np = clk_hw_get_num_parents(c_hw);

	if (np <= 1U)
		return;

	dump_provider_clk(pvdck, s);

	for (i = 0; i < np; i++) {
		struct clk_hw *p_hw = clk_hw_get_parent_by_index(c_hw, i);

		if (IS_ERR_OR_NULL(p_hw))
			continue;

		seq_printf(s, "\t\t\t(%2d: %-17s: %8s, %10ld)\n",
			i,
			clk_hw_get_name(p_hw),
			ccf_state(p_hw),
			clk_hw_get_rate(p_hw));
	}
}

static int clkdbg_dump_muxes(struct seq_file *s, void *v)
{
	struct provider_clk *pvdck = get_all_provider_clks();

	for (; pvdck->ck != NULL; pvdck++)
		dump_provider_mux(pvdck, s);

	return 0;
}

static void show_pwr_status(u32 spm_pwr_status)
{
	unsigned int i;
	const char * const *pwr_name = get_pwr_names();

	pr_info("SPM_PWR_STATUS: 0x%08x\n\n", spm_pwr_status);

	for (i = 0; i < 32; i++) {
		const char *st = (spm_pwr_status & BIT(i)) != 0U ? "ON" : "off";

		pr_info("[%2d]: %3s: %s\n", i, st, pwr_name[i]);
		mdelay(20);
	}
}

static int dump_pwr_status(u32 spm_pwr_status, struct seq_file *s)
{
	unsigned int i;
	const char * const *pwr_name = get_pwr_names();

	seq_printf(s, "SPM_PWR_STATUS: 0x%08x\n\n", spm_pwr_status);

	for (i = 0; i < 32; i++) {
		const char *st = (spm_pwr_status & BIT(i)) != 0U ? "ON" : "off";

		seq_printf(s, "[%2d]: %3s: %s\n", i, st, pwr_name[i]);
	}

	return 0;
}

static int clkdbg_pwr_status(struct seq_file *s, void *v)
{
	return dump_pwr_status(read_spm_pwr_status(), s);
}

static char last_cmd[128] = "null";

const char *get_last_cmd(void)
{
	return last_cmd;
}
EXPORT_SYMBOL(get_last_cmd);

static int clkop_int_ckname(int (*clkop)(struct clk *clk),
			const char *clkop_name, const char *clk_name,
			struct clk *ck, struct seq_file *s)
{
	struct clk *clk;

	if (!IS_ERR_OR_NULL(ck)) {
		clk = ck;
	} else {
		clk = __clk_lookup(clk_name);
		if (IS_ERR_OR_NULL(clk)) {
			seq_printf(s, "clk_lookup(%s): 0x%p\n", clk_name, clk);
			return PTR_ERR(clk);
		}
	}

	return clkop(clk);
}

static int clkdbg_clkop_int_ckname(int (*clkop)(struct clk *clk),
			const char *clkop_name, struct seq_file *s, void *v)
{
	char cmd[sizeof(last_cmd)];
	char *c = cmd;
	char *ign;
	char *clk_name;
	int r = 0;

	strncpy(cmd, last_cmd, sizeof(cmd));
	cmd[sizeof(cmd) - 1UL] = '\0';

	ign = strsep(&c, " ");
	clk_name = strsep(&c, " ");

	if (clk_name == NULL)
		return 0;

	if (strcmp(clk_name, "all") == 0) {
		struct provider_clk *pvdck = get_all_provider_clks();

		for (; pvdck->ck != NULL; pvdck++) {
			r |= clkop_int_ckname(clkop, clkop_name, NULL,
						pvdck->ck, s);
		}

		seq_printf(s, "%s(%s): %d\n", clkop_name, clk_name, r);

		return r;
	}

	r = clkop_int_ckname(clkop, clkop_name, clk_name, NULL, s);
	seq_printf(s, "%s(%s): %d\n", clkop_name, clk_name, r);

	return r;
}

static void clkop_void_ckname(void (*clkop)(struct clk *clk),
			const char *clkop_name, const char *clk_name,
			struct clk *ck, struct seq_file *s)
{
	struct clk *clk;

	if (!IS_ERR_OR_NULL(ck)) {
		clk = ck;
	} else {
		clk = __clk_lookup(clk_name);
		if (IS_ERR_OR_NULL(clk)) {
			seq_printf(s, "clk_lookup(%s): 0x%p\n", clk_name, clk);
			return;
		}
	}

	clkop(clk);
}

static int clkdbg_clkop_void_ckname(void (*clkop)(struct clk *clk),
			const char *clkop_name, struct seq_file *s, void *v)
{
	char cmd[sizeof(last_cmd)];
	char *c = cmd;
	char *ign;
	char *clk_name;

	strncpy(cmd, last_cmd, sizeof(cmd));
	cmd[sizeof(cmd) - 1UL] = '\0';

	ign = strsep(&c, " ");
	clk_name = strsep(&c, " ");

	if (clk_name == NULL)
		return 0;

	if (strcmp(clk_name, "all") == 0) {
		struct provider_clk *pvdck = get_all_provider_clks();

		for (; pvdck->ck != NULL; pvdck++) {
			clkop_void_ckname(clkop, clkop_name, NULL,
						pvdck->ck, s);
		}

		seq_printf(s, "%s(%s)\n", clkop_name, clk_name);

		return 0;
	}

	clkop_void_ckname(clkop, clkop_name, clk_name, NULL, s);
	seq_printf(s, "%s(%s)\n", clkop_name, clk_name);

	return 0;
}

static int clkdbg_prepare(struct seq_file *s, void *v)
{
	return clkdbg_clkop_int_ckname(clk_prepare,
					"clk_prepare", s, v);
}

static int clkdbg_unprepare(struct seq_file *s, void *v)
{
	return clkdbg_clkop_void_ckname(clk_unprepare,
					"clk_unprepare", s, v);
}

static int clkdbg_enable(struct seq_file *s, void *v)
{
	return clkdbg_clkop_int_ckname(clk_enable,
					"clk_enable", s, v);
}

static int clkdbg_disable(struct seq_file *s, void *v)
{
	return clkdbg_clkop_void_ckname(clk_disable,
					"clk_disable", s, v);
}

static int clkdbg_prepare_enable(struct seq_file *s, void *v)
{
	return clkdbg_clkop_int_ckname(clk_prepare_enable,
					"clk_prepare_enable", s, v);
}

static int clkdbg_disable_unprepare(struct seq_file *s, void *v)
{
	return clkdbg_clkop_void_ckname(clk_disable_unprepare,
					"clk_disable_unprepare", s, v);
}

void prepare_enable_provider(const char *pvd)
{
	bool allpvd = (pvd == NULL || strcmp(pvd, "all") == 0);
	struct provider_clk *pvdck = get_all_provider_clks();

	for (; pvdck->ck != NULL; pvdck++) {
		if (allpvd || (pvdck->provider_name != NULL &&
				strcmp(pvd, pvdck->provider_name) == 0)) {
			int r = clk_prepare_enable(pvdck->ck);

			if (r != 0)
				pr_info("clk_prepare_enable(): %d\n", r);
		}
	}
}
EXPORT_SYMBOL(prepare_enable_provider);

void disable_unprepare_provider(const char *pvd)
{
	bool allpvd = (pvd == NULL || strcmp(pvd, "all") == 0);
	struct provider_clk *pvdck = get_all_provider_clks();

	for (; pvdck->ck != NULL; pvdck++) {
		if (allpvd || (pvdck->provider_name != NULL &&
				strcmp(pvd, pvdck->provider_name) == 0))
			clk_disable_unprepare(pvdck->ck);
	}
}
EXPORT_SYMBOL(disable_unprepare_provider);

static void clkpvdop(void (*pvdop)(const char *), const char *clkpvdop_name,
			struct seq_file *s)
{
	char cmd[sizeof(last_cmd)];
	char *c = cmd;
	char *ign;
	char *pvd_name;

	strncpy(cmd, last_cmd, sizeof(cmd));
	cmd[sizeof(cmd) - 1UL] = '\0';

	ign = strsep(&c, " ");
	pvd_name = strsep(&c, " ");

	if (pvd_name == NULL)
		return;

	pvdop(pvd_name);
	seq_printf(s, "%s(%s)\n", clkpvdop_name, pvd_name);
}

static int clkdbg_prepare_enable_provider(struct seq_file *s, void *v)
{
	clkpvdop(prepare_enable_provider, "prepare_enable_provider", s);
	return 0;
}

static int clkdbg_disable_unprepare_provider(struct seq_file *s, void *v)
{
	clkpvdop(disable_unprepare_provider, "disable_unprepare_provider", s);
	return 0;
}

static int clkdbg_set_parent(struct seq_file *s, void *v)
{
	char cmd[sizeof(last_cmd)];
	char *c = cmd;
	char *ign;
	char *clk_name;
	char *parent_name;
	struct clk *clk;
	struct clk *parent;
	int r;

	strncpy(cmd, last_cmd, sizeof(cmd));
	cmd[sizeof(cmd) - 1UL] = '\0';

	ign = strsep(&c, " ");
	clk_name = strsep(&c, " ");
	parent_name = strsep(&c, " ");

	if (clk_name == NULL || parent_name == NULL)
		return 0;

	seq_printf(s, "clk_set_parent(%s, %s): ", clk_name, parent_name);

	clk = __clk_lookup(clk_name);
	if (IS_ERR_OR_NULL(clk)) {
		seq_printf(s, "__clk_lookup(): 0x%p\n", clk);
		return PTR_ERR(clk);
	}

	parent = __clk_lookup(parent_name);
	if (IS_ERR_OR_NULL(parent)) {
		seq_printf(s, "__clk_lookup(): 0x%p\n", parent);
		return PTR_ERR(parent);
	}

	r = clk_prepare_enable(clk);
	if (r != 0) {
		seq_printf(s, "clk_prepare_enable(): %d\n", r);
		return r;
	}

	r = clk_set_parent(clk, parent);
	seq_printf(s, "%d\n", r);

	clk_disable_unprepare(clk);

	return r;
}

static int clkdbg_set_rate(struct seq_file *s, void *v)
{
	char cmd[sizeof(last_cmd)];
	char *c = cmd;
	char *ign;
	char *clk_name;
	char *rate_str;
	struct clk *clk;
	unsigned long rate;
	int r;

	strncpy(cmd, last_cmd, sizeof(cmd));
	cmd[sizeof(cmd) - 1UL] = '\0';

	ign = strsep(&c, " ");
	clk_name = strsep(&c, " ");
	rate_str = strsep(&c, " ");

	if (clk_name == NULL || rate_str == NULL)
		return 0;

	r = kstrtoul(rate_str, 0, &rate);

	seq_printf(s, "clk_set_rate(%s, %lu): %d: ", clk_name, rate, r);

	clk = __clk_lookup(clk_name);
	if (IS_ERR_OR_NULL(clk)) {
		seq_printf(s, "__clk_lookup(): 0x%p\n", clk);
		return PTR_ERR(clk);
	}

	r = clk_set_rate(clk, rate);
	seq_printf(s, "%d\n", r);

	return r;
}

#if defined(CONFIG_MTK_ENG_BUILD)
static void *reg_from_str(const char *str)
{
	static phys_addr_t phys;
	static void __iomem *virt;

	if (sizeof(void *) == sizeof(unsigned long)) {
		unsigned long v;

		if (kstrtoul(str, 0, &v) == 0U) {
			if ((0xf0000000 & v) < 0x20000000) {
				if (virt != NULL && v > phys
						&& v < phys + PAGE_SIZE)
					return virt + v - phys;

				if (virt != NULL)
					iounmap(virt);

				phys = v & ~(PAGE_SIZE - 1U);
				virt = ioremap(phys, PAGE_SIZE);

				return virt + v - phys;
			}

			return (void *)((uintptr_t)v);
		}
	} else if (sizeof(void *) == sizeof(unsigned long long)) {
		unsigned long long v;

		if (kstrtoull(str, 0, &v) == 0) {
			if ((0xfffffffff0000000ULL & v) < 0x20000000) {
				if (virt && v > phys && v < phys + PAGE_SIZE)
					return virt + v - phys;

				if (virt != NULL)
					iounmap(virt);

				phys = v & ~(PAGE_SIZE - 1);
				virt = ioremap(phys, PAGE_SIZE);

				return virt + v - phys;
			}

			return (void *)((uintptr_t)v);
		}
	} else {
		pr_warn("unexpected pointer size: sizeof(void *): %zu\n",
			sizeof(void *));
	}

	pr_warn("%s(): parsing error: %s\n", __func__, str);

	return NULL;
}

static int parse_reg_val_from_cmd(void __iomem **preg, unsigned long *pval)
{
	char cmd[sizeof(last_cmd)];
	char *c = cmd;
	char *ign;
	char *reg_str;
	char *val_str;
	int r = 0;

	strncpy(cmd, last_cmd, sizeof(cmd));
	cmd[sizeof(cmd) - 1UL] = '\0';

	ign = strsep(&c, " ");
	reg_str = strsep(&c, " ");
	val_str = strsep(&c, " ");

	if (preg != NULL && reg_str != NULL) {
		*preg = reg_from_str(reg_str);
		if (*preg != NULL)
			r++;
	}

	if (pval != NULL && val_str != NULL && kstrtoul(val_str, 0, pval) == 0)
		r++;

	return r;
}

static int clkdbg_reg_read(struct seq_file *s, void *v)
{
	void __iomem *reg;
	unsigned long val;

	if (parse_reg_val_from_cmd(&reg, NULL) != 1)
		return 0;

	seq_printf(s, "readl(0x%p): ", reg);

	val = clk_readl(reg);
	seq_printf(s, "0x%08x\n", (u32)val);

	return 0;
}

static int clkdbg_reg_write(struct seq_file *s, void *v)
{
	void __iomem *reg;
	unsigned long val;

	if (parse_reg_val_from_cmd(&reg, &val) != 2)
		return 0;

	seq_printf(s, "writel(0x%p, 0x%08x): ", reg, (u32)val);

	clk_writel(reg, val);
	val = clk_readl(reg);
	seq_printf(s, "0x%08x\n", (u32)val);

	return 0;
}

static int clkdbg_reg_set(struct seq_file *s, void *v)
{
	void __iomem *reg;
	unsigned long val;

	if (parse_reg_val_from_cmd(&reg, &val) != 2)
		return 0;

	seq_printf(s, "writel(0x%p, 0x%08x): ", reg, (u32)val);

	clk_setl(reg, val);
	val = clk_readl(reg);
	seq_printf(s, "0x%08x\n", (u32)val);

	return 0;
}

static int clkdbg_reg_clr(struct seq_file *s, void *v)
{
	void __iomem *reg;
	unsigned long val;

	if (parse_reg_val_from_cmd(&reg, &val) != 2)
		return 0;

	seq_printf(s, "writel(0x%p, 0x%08x): ", reg, (u32)val);

	clk_clrl(reg, val);
	val = clk_readl(reg);
	seq_printf(s, "0x%08x\n", (u32)val);

	return 0;
}
#endif /* CONFIG_MTK_ENG_BUILD */


static int parse_val_from_cmd(unsigned long *pval)
{
	char cmd[sizeof(last_cmd)];
	char *c = cmd;
	char *ign;
	char *val_str;
	int r = 0;

	strncpy(cmd, last_cmd, sizeof(cmd));
	cmd[sizeof(cmd) - 1UL] = '\0';

	ign = strsep(&c, " ");
	val_str = strsep(&c, " ");

	if (pval != NULL && val_str != NULL && kstrtoul(val_str, 0, pval) == 0)
		r++;

	return r;
}

static int clkdbg_show_flags(struct seq_file *s, void *v)
{
	static const char * const clkdbg_opt_name[] = {
		"CLKDBG_EN_SUSPEND_SAVE_1",
		"CLKDBG_EN_SUSPEND_SAVE_2",
		"CLKDBG_EN_SUSPEND_SAVE_3",
		"CLKDBG_EN_LOG_SAVE_POINTS",
	};

	size_t i;

	seq_printf(s, "clkdbg_flags: 0x%08x\n", clkdbg_flags);

	for (i = 0; i < ARRAY_SIZE(clkdbg_opt_name); i++) {
		const char *onff =
			has_clkdbg_flag((enum clkdbg_opt)i) ? "ON" : "off";

		seq_printf(s, "[%2zd]: %3s: %s\n", i, onff, clkdbg_opt_name[i]);
	}

	return 0;
}

static int clkdbg_set_flag(struct seq_file *s, void *v)
{
	unsigned long val;

	if (parse_val_from_cmd(&val) != 1)
		return 0;

	set_clkdbg_flag((enum clkdbg_opt)val);

	seq_printf(s, "clkdbg_flags: 0x%08x\n", clkdbg_flags);

	return 0;
}

static int clkdbg_clr_flag(struct seq_file *s, void *v)
{
	unsigned long val;

	if (parse_val_from_cmd(&val) != 1)
		return 0;

	clr_clkdbg_flag((enum clkdbg_opt)val);

	seq_printf(s, "clkdbg_flags: 0x%08x\n", clkdbg_flags);

	return 0;
}

#if CLKDBG_PM_DOMAIN

/*
 * pm_domain support
 */

static struct generic_pm_domain **get_all_genpd(void)
{
	static struct generic_pm_domain *pds[20];
	static int num_pds;
	const size_t maxpd = ARRAY_SIZE(pds);
	struct device_node *node;
#if CLKDBG_PM_DOMAIN_API_4_9 || CLKDBG_PM_DOMAIN_API_4_19
	struct platform_device *pdev;
	int r;
#endif

	if (num_pds != 0)
		goto out;

	node = of_find_node_with_property(NULL, "#power-domain-cells");

	if (node == NULL)
		return NULL;

#if CLKDBG_PM_DOMAIN_API_4_9 || CLKDBG_PM_DOMAIN_API_4_19
	pdev = platform_device_alloc("traverse", 0);
	if (!pdev)
		return NULL;
#endif

	for (num_pds = 0; num_pds < maxpd; num_pds++) {
		struct of_phandle_args pa;

		pa.np = node;
		pa.args[0] = num_pds;
		pa.args_count = 1;

#if CLKDBG_PM_DOMAIN_API_4_9 || CLKDBG_PM_DOMAIN_API_4_19
		r = of_genpd_add_device(&pa, &pdev->dev);
		if (r == -EINVAL)
			continue;
		else if (r != 0)
			pr_warn("%s(): of_genpd_add_device(%d)\n", __func__, r);
		pds[num_pds] = pd_to_genpd(pdev->dev.pm_domain);
#if CLKDBG_PM_DOMAIN_API_4_19
		r = pm_genpd_remove_device(&pdev->dev);
#else /* < v4.19 */
		r = pm_genpd_remove_device(pds[num_pds], &pdev->dev);
#endif
		if (r != 0)
			pr_warn("%s(): pm_genpd_remove_device(%d)\n",
					__func__, r);
#else
		pds[num_pds] = of_genpd_get_from_provider(&pa);
#endif

		if (IS_ERR(pds[num_pds])) {
			pds[num_pds] = NULL;
			break;
		}
	}

#if CLKDBG_PM_DOMAIN_API_4_9 || CLKDBG_PM_DOMAIN_API_4_19
	platform_device_put(pdev);
#endif

out:
	return pds;
}

static struct platform_device *pdev_from_name(const char *name)
{
	struct generic_pm_domain **pds = get_all_genpd();

	for (; *pds != NULL; pds++) {
		struct pm_domain_data *pdd;
		struct generic_pm_domain *pd = *pds;

		if (IS_ERR_OR_NULL(pd))
			continue;

		list_for_each_entry(pdd, &pd->dev_list, list_node) {
			struct device *dev = pdd->dev;
			struct platform_device *pdev = to_platform_device(dev);

			if (strcmp(name, pdev->name) == 0)
				return pdev;
		}
	}

	return NULL;
}

static struct generic_pm_domain *genpd_from_name(const char *name)
{
	struct generic_pm_domain **pds = get_all_genpd();

	for (; *pds != NULL; pds++) {
		struct generic_pm_domain *pd = *pds;

		if (IS_ERR_OR_NULL(pd))
			continue;

		if (strcmp(name, pd->name) == 0)
			return pd;
	}

	return NULL;
}

struct genpd_dev_state {
	struct device *dev;
	bool active;
	atomic_t usage_count;
	unsigned int disable_depth;
	enum rpm_status runtime_status;
};

struct genpd_state {
	struct generic_pm_domain *pd;
	enum gpd_status status;
	struct genpd_dev_state *dev_state;
	int num_dev_state;
};

static void save_all_genpd_state(struct genpd_state *genpd_states,
				struct genpd_dev_state *genpd_dev_states)
{
	struct genpd_state *pdst = genpd_states;
	struct genpd_dev_state *devst = genpd_dev_states;
	struct generic_pm_domain **pds = get_all_genpd();

	for (; *pds != NULL; pds++) {
		struct pm_domain_data *pdd;
		struct generic_pm_domain *pd = *pds;

		if (IS_ERR_OR_NULL(pd))
			continue;

		pdst->pd = pd;
		pdst->status = pd->status;
		pdst->dev_state = devst;
		pdst->num_dev_state = 0;

		list_for_each_entry(pdd, &pd->dev_list, list_node) {
			struct device *d = pdd->dev;

			devst->dev = d;
			devst->active = pm_runtime_active(d);
			devst->usage_count = d->power.usage_count;
			devst->disable_depth = d->power.disable_depth;
			devst->runtime_status = d->power.runtime_status;

			devst++;
			pdst->num_dev_state++;
		}

		pdst++;
	}

	pdst->pd = NULL;
	devst->dev = NULL;
}

static void show_genpd_state(struct genpd_state *pdst)
{
	static const char * const gpd_status_name[] = {
		"ACTIVE",
		"POWER_OFF",
	};

	static const char * const prm_status_name[] = {
		"active",
		"resuming",
		"suspended",
		"suspending",
	};

	pr_info("domain_on [pmd_name  status]\n");
	pr_info("\tdev_on (dev_name usage_count, disable, status)\n");
	pr_info("------------------------------------------------------\n");

	for (; pdst->pd != NULL; pdst++) {
		int i;
		struct generic_pm_domain *pd = pdst->pd;

		if (IS_ERR_OR_NULL(pd)) {
			pr_info("pd: 0x%p\n", pd);
			continue;
		}

		pr_info("%c [%-9s %11s]\n",
			(pdst->status == GPD_STATE_ACTIVE) ? '+' : '-',
			pd->name, gpd_status_name[pdst->status]);

		for (i = 0; i < pdst->num_dev_state; i++) {
			struct genpd_dev_state *devst = &pdst->dev_state[i];
			struct device *dev = devst->dev;
			struct platform_device *pdev = to_platform_device(dev);

			pr_info("\t%c (%-19s %3d, %d, %10s)\n",
				devst->active ? '+' : '-',
				pdev->name,
				atomic_read(&dev->power.usage_count),
				devst->disable_depth,
				prm_status_name[devst->runtime_status]);
			mdelay(20);
		}
	}
}

static void dump_genpd_state(struct genpd_state *pdst, struct seq_file *s)
{
	static const char * const gpd_status_name[] = {
		"ACTIVE",
		"POWER_OFF",
	};

	static const char * const prm_status_name[] = {
		"active",
		"resuming",
		"suspended",
		"suspending",
	};

	seq_puts(s, "domain_on [pmd_name  status]\n");
	seq_puts(s, "\tdev_on (dev_name usage_count, disable, status)\n");
	seq_puts(s, "------------------------------------------------------\n");

	for (; pdst->pd != NULL; pdst++) {
		int i;
		struct generic_pm_domain *pd = pdst->pd;

		if (IS_ERR_OR_NULL(pd)) {
			seq_printf(s, "pd: 0x%p\n", pd);
			continue;
		}

		seq_printf(s, "%c [%-9s %11s]\n",
			(pdst->status == GPD_STATE_ACTIVE) ? '+' : '-',
			pd->name, gpd_status_name[pdst->status]);

		for (i = 0; i < pdst->num_dev_state; i++) {
			struct genpd_dev_state *devst = &pdst->dev_state[i];
			struct device *dev = devst->dev;
			struct platform_device *pdev = to_platform_device(dev);

			seq_printf(s, "\t%c (%-19s %3d, %d, %10s)\n",
				devst->active ? '+' : '-',
				pdev->name,
				atomic_read(&dev->power.usage_count),
				devst->disable_depth,
				prm_status_name[devst->runtime_status]);
		}
	}
}

static void seq_print_all_genpd(struct seq_file *s)
{
	static struct genpd_dev_state devst[100];
	static struct genpd_state pdst[20];

	save_all_genpd_state(pdst, devst);
	dump_genpd_state(pdst, s);
}

static int clkdbg_dump_genpd(struct seq_file *s, void *v)
{
	seq_print_all_genpd(s);

	return 0;
}

static int clkdbg_pm_runtime_enable(struct seq_file *s, void *v)
{
	char cmd[sizeof(last_cmd)];
	char *c = cmd;
	char *ign;
	char *dev_name;
	struct platform_device *pdev;

	strncpy(cmd, last_cmd, sizeof(cmd));
	cmd[sizeof(cmd) - 1UL] = '\0';

	ign = strsep(&c, " ");
	dev_name = strsep(&c, " ");

	if (dev_name == NULL)
		return 0;

	seq_printf(s, "pm_runtime_enable(%s): ", dev_name);

	pdev = pdev_from_name(dev_name);
	if (pdev != NULL) {
		pm_runtime_enable(&pdev->dev);
		seq_puts(s, "\n");
	} else {
		seq_puts(s, "NULL\n");
	}

	return 0;
}

static int clkdbg_pm_runtime_disable(struct seq_file *s, void *v)
{
	char cmd[sizeof(last_cmd)];
	char *c = cmd;
	char *ign;
	char *dev_name;
	struct platform_device *pdev;

	strncpy(cmd, last_cmd, sizeof(cmd));
	cmd[sizeof(cmd) - 1UL] = '\0';

	ign = strsep(&c, " ");
	dev_name = strsep(&c, " ");

	if (dev_name == NULL)
		return 0;

	seq_printf(s, "pm_runtime_disable(%s): ", dev_name);

	pdev = pdev_from_name(dev_name);
	if (pdev != NULL) {
		pm_runtime_disable(&pdev->dev);
		seq_puts(s, "\n");
	} else {
		seq_puts(s, "NULL\n");
	}

	return 0;
}

static int clkdbg_pm_runtime_get_sync(struct seq_file *s, void *v)
{
	char cmd[sizeof(last_cmd)];
	char *c = cmd;
	char *ign;
	char *dev_name;
	struct platform_device *pdev;

	strncpy(cmd, last_cmd, sizeof(cmd));
	cmd[sizeof(cmd) - 1UL] = '\0';

	ign = strsep(&c, " ");
	dev_name = strsep(&c, " ");

	if (dev_name == NULL)
		return 0;

	seq_printf(s, "pm_runtime_get_sync(%s): ", dev_name);

	pdev = pdev_from_name(dev_name);
	if (pdev != NULL) {
		int r = pm_runtime_get_sync(&pdev->dev);

		seq_printf(s, "%d\n", r);
	} else {
		seq_puts(s, "NULL\n");
	}

	return 0;
}

static int clkdbg_pm_runtime_put_sync(struct seq_file *s, void *v)
{
	char cmd[sizeof(last_cmd)];
	char *c = cmd;
	char *ign;
	char *dev_name;
	struct platform_device *pdev;

	strncpy(cmd, last_cmd, sizeof(cmd));
	cmd[sizeof(cmd) - 1UL] = '\0';

	ign = strsep(&c, " ");
	dev_name = strsep(&c, " ");

	if (dev_name == NULL)
		return 0;

	seq_printf(s, "pm_runtime_put_sync(%s): ", dev_name);

	pdev = pdev_from_name(dev_name);
	if (pdev != NULL) {
		int r = pm_runtime_put_sync(&pdev->dev);

		seq_printf(s, "%d\n", r);
	} else {
		seq_puts(s, "NULL\n");
	}

	return 0;
}

static int genpd_op(const char *gpd_op_name, struct seq_file *s)
{
	char cmd[sizeof(last_cmd)];
	char *c = cmd;
	char *ign;
	char *pd_name;
	struct generic_pm_domain *genpd;
	int gpd_op_id;
	int (*gpd_op)(struct generic_pm_domain *genpd);
	int r = 0;

	strncpy(cmd, last_cmd, sizeof(cmd));
	cmd[sizeof(cmd) - 1UL] = '\0';

	ign = strsep(&c, " ");
	pd_name = strsep(&c, " ");

	if (pd_name == NULL)
		return 0;

	if (strcmp(gpd_op_name, "power_on") == 0)
		gpd_op_id = 1;
	else
		gpd_op_id = 0;

	if (strcmp(pd_name, "all") == 0) {
		struct generic_pm_domain **pds = get_all_genpd();

		for (; *pds != NULL; pds++) {
			genpd = *pds;

			if (IS_ERR_OR_NULL(genpd))
				continue;

			gpd_op = (gpd_op_id == 1) ?
					genpd->power_on : genpd->power_off;
			r |= gpd_op(genpd);
		}

		seq_printf(s, "%s(%s): %d\n", gpd_op_name, pd_name, r);

		return 0;
	}

	genpd = genpd_from_name(pd_name);
	if (genpd != NULL) {
		gpd_op = (gpd_op_id == 1) ? genpd->power_on : genpd->power_off;
		r = gpd_op(genpd);

		seq_printf(s, "%s(%s): %d\n", gpd_op_name, pd_name, r);
	} else {
		seq_printf(s, "genpd_from_name(%s): NULL\n", pd_name);
	}

	return 0;
}

static int clkdbg_pwr_on(struct seq_file *s, void *v)
{
	return genpd_op("power_on", s);
}

static int clkdbg_pwr_off(struct seq_file *s, void *v)
{
	return genpd_op("power_off", s);
}

/*
 * clkdbg reg_pdrv/runeg_pdrv support
 */

static int clkdbg_probe(struct platform_device *pdev)
{
	int r;

	pm_runtime_enable(&pdev->dev);
	r = pm_runtime_get_sync(&pdev->dev);
	if (r != 0)
		pr_warn("%s(): pm_runtime_get_sync(%d)\n", __func__, r);

	return r;
}

static int clkdbg_remove(struct platform_device *pdev)
{
	int r;

	r = pm_runtime_put_sync(&pdev->dev);
	if (r != 0)
		pr_warn("%s(): pm_runtime_put_sync(%d)\n", __func__, r);
	pm_runtime_disable(&pdev->dev);

	return r;
}

struct pdev_drv {
	struct platform_driver pdrv;
	struct platform_device *pdev;
	struct generic_pm_domain *genpd;
};

#define PDEV_DRV(_name) {				\
	.pdrv = {					\
		.probe		= clkdbg_probe,		\
		.remove		= clkdbg_remove,	\
		.driver		= {			\
			.name	= _name,		\
		},					\
	},						\
}

static struct pdev_drv pderv[] = {
	PDEV_DRV("clkdbg-pd0"),
	PDEV_DRV("clkdbg-pd1"),
	PDEV_DRV("clkdbg-pd2"),
	PDEV_DRV("clkdbg-pd3"),
	PDEV_DRV("clkdbg-pd4"),
	PDEV_DRV("clkdbg-pd5"),
	PDEV_DRV("clkdbg-pd6"),
	PDEV_DRV("clkdbg-pd7"),
	PDEV_DRV("clkdbg-pd8"),
	PDEV_DRV("clkdbg-pd9"),
	PDEV_DRV("clkdbg-pd10"),
};

static void reg_pdev_drv(const char *pdname, struct seq_file *s)
{
	size_t i;
	struct generic_pm_domain **pds = get_all_genpd();
	bool allpd = (pdname == NULL || strcmp(pdname, "all") == 0);
	int r;

	for (i = 0; i < ARRAY_SIZE(pderv) && *pds != NULL; i++, pds++) {
		const char *name = pderv[i].pdrv.driver.name;
		struct generic_pm_domain *pd = *pds;

		if (IS_ERR_OR_NULL(pd) || pderv[i].genpd != NULL)
			continue;

		if (!allpd && strcmp(pdname, pd->name) != 0)
			continue;

		pderv[i].genpd = pd;

		pderv[i].pdev = platform_device_alloc(name, 0);
		r = platform_device_add(pderv[i].pdev);
		if (r != 0 && s != NULL)
			seq_printf(s, "%s(): platform_device_add(%d)\n",
						__func__, r);

		r = pm_genpd_add_device(pd, &pderv[i].pdev->dev);
		if (r != 0 && s != NULL)
			seq_printf(s, "%s(): pm_genpd_add_device(%d)\n",
						__func__, r);
		r = platform_driver_register(&pderv[i].pdrv);
		if (r != 0 && s != NULL)
			seq_printf(s, "%s(): platform_driver_register(%d)\n",
						__func__, r);

		if (s != NULL)
			seq_printf(s, "%s --> %s\n", name, pd->name);
	}
}

static void unreg_pdev_drv(const char *pdname, struct seq_file *s)
{
	ssize_t i;
	bool allpd = (pdname == NULL || strcmp(pdname, "all") == 0);
	int r;

	for (i = ARRAY_SIZE(pderv) - 1L; i >= 0L; i--) {
		const char *name = pderv[i].pdrv.driver.name;
		struct generic_pm_domain *pd = pderv[i].genpd;

		if (IS_ERR_OR_NULL(pd))
			continue;

		if (!allpd && strcmp(pdname, pd->name) != 0)
			continue;

#if CLKDBG_PM_DOMAIN_API_4_19
		r = pm_genpd_remove_device(&pderv[i].pdev->dev);
#else
		r = pm_genpd_remove_device(pd, &pderv[i].pdev->dev);
#endif
		if (r != 0 && s != NULL)
			seq_printf(s, "%s(): pm_genpd_remove_device(%d)\n",
						__func__, r);

		platform_driver_unregister(&pderv[i].pdrv);
		platform_device_unregister(pderv[i].pdev);

		pderv[i].genpd = NULL;

		if (s != NULL)
			seq_printf(s, "%s -x- %s\n", name, pd->name);
	}
}

static int clkdbg_reg_pdrv(struct seq_file *s, void *v)
{
	char cmd[sizeof(last_cmd)];
	char *c = cmd;
	char *ign;
	char *pd_name;

	strncpy(cmd, last_cmd, sizeof(cmd));
	cmd[sizeof(cmd) - 1UL] = '\0';

	ign = strsep(&c, " ");
	pd_name = strsep(&c, " ");

	if (pd_name == NULL)
		return 0;

	reg_pdev_drv(pd_name, s);

	return 0;
}

static int clkdbg_unreg_pdrv(struct seq_file *s, void *v)
{
	char cmd[sizeof(last_cmd)];
	char *c = cmd;
	char *ign;
	char *pd_name;

	strncpy(cmd, last_cmd, sizeof(cmd));
	cmd[sizeof(cmd) - 1UL] = '\0';

	ign = strsep(&c, " ");
	pd_name = strsep(&c, " ");

	if (pd_name == NULL)
		return 0;

	unreg_pdev_drv(pd_name, s);

	return 0;
}

#endif /* CLKDBG_PM_DOMAIN */

void reg_pdrv(const char *pdname)
{
#if CLKDBG_PM_DOMAIN
	reg_pdev_drv(pdname, NULL);
#endif
}
EXPORT_SYMBOL(reg_pdrv);

void unreg_pdrv(const char *pdname)
{
#if CLKDBG_PM_DOMAIN
	unreg_pdev_drv(pdname, NULL);
#endif
}
EXPORT_SYMBOL(unreg_pdrv);

/*
 * Suspend / resume handler
 */

#include <linux/suspend.h>
#include <linux/syscore_ops.h>

struct provider_clk_state {
	struct provider_clk *pvdck;
	bool prepared;
	bool enabled;
	unsigned int enable_count;
	unsigned long rate;
	struct clk *parent;
};

struct save_point {
	u32 spm_pwr_status;
	struct provider_clk_state clks_states[512];
#if CLKDBG_PM_DOMAIN
	struct genpd_state genpd_states[20];
	struct genpd_dev_state genpd_dev_states[100];
#endif
};

static struct save_point save_point_1;
static struct save_point save_point_2;
static struct save_point save_point_3;

static void save_pwr_status(u32 *spm_pwr_status)
{
	*spm_pwr_status = read_spm_pwr_status();
}

static void save_all_clks_state(struct provider_clk_state *clks_states,
				u32 spm_pwr_status)
{
	struct provider_clk *pvdck = get_all_provider_clks();
	struct provider_clk_state *st = clks_states;

	for (; pvdck->ck != NULL; pvdck++, st++) {
		struct clk *c = pvdck->ck;
		struct clk_hw *c_hw = __clk_get_hw(c);

		st->pvdck = pvdck;
		st->prepared = clk_hw_is_prepared(c_hw);
		st->enabled = clk_hw_pwr_is_on(c_hw, spm_pwr_status,
							pvdck->pwr_mask);
		st->enable_count = __clk_get_enable_count(c);
		st->rate = clk_hw_get_rate(c_hw);
		st->parent = IS_ERR_OR_NULL(c) ? NULL : clk_get_parent(c);
	}
}

static void show_provider_clk_state(struct provider_clk_state *st)
{
	struct provider_clk *pvdck = st->pvdck;
	struct clk_hw *c_hw = __clk_get_hw(pvdck->ck);

	pr_info("[%10s: %-17s: %3s, %3d, %3d, %10ld, %17s]\n",
		pvdck->provider_name != NULL ? pvdck->provider_name : "/ ",
		clk_hw_get_name(c_hw),
		st->enabled ? "ON" : "off",
		st->prepared,
		st->enable_count,
		st->rate,
		st->parent != NULL ?
			clk_hw_get_name(__clk_get_hw(st->parent)) : "- ");
	mdelay(20);
}

static void dump_provider_clk_state(struct provider_clk_state *st,
					struct seq_file *s)
{
	struct provider_clk *pvdck = st->pvdck;
	struct clk_hw *c_hw = __clk_get_hw(pvdck->ck);

	seq_printf(s, "[%10s: %-17s: %3s, %3d, %3d, %10ld, %17s]\n",
		pvdck->provider_name != NULL ? pvdck->provider_name : "/ ",
		clk_hw_get_name(c_hw),
		st->enabled ? "ON" : "off",
		st->prepared,
		st->enable_count,
		st->rate,
		st->parent != NULL ?
			clk_hw_get_name(__clk_get_hw(st->parent)) : "- ");
}

static void show_save_point(struct save_point *sp)
{
	struct provider_clk_state *st = sp->clks_states;

	for (; st->pvdck != NULL; st++)
		show_provider_clk_state(st);

	pr_info("\n");
	show_pwr_status(sp->spm_pwr_status);

#if CLKDBG_PM_DOMAIN
	pr_info("\n");
	show_genpd_state(sp->genpd_states);
#endif
}

static void store_save_point(struct save_point *sp)
{
	save_pwr_status(&sp->spm_pwr_status);
	save_all_clks_state(sp->clks_states, sp->spm_pwr_status);

#if CLKDBG_PM_DOMAIN
	save_all_genpd_state(sp->genpd_states, sp->genpd_dev_states);
#endif

	if (has_clkdbg_flag(CLKDBG_EN_LOG_SAVE_POINTS))
		show_save_point(sp);
}

static void dump_save_point(struct save_point *sp, struct seq_file *s)
{
	struct provider_clk_state *st = sp->clks_states;

	for (; st->pvdck != NULL; st++)
		dump_provider_clk_state(st, s);

	seq_puts(s, "\n");
	dump_pwr_status(sp->spm_pwr_status, s);

#if CLKDBG_PM_DOMAIN
	seq_puts(s, "\n");
	dump_genpd_state(sp->genpd_states, s);
#endif
}

static int clkdbg_dump_suspend_clks_1(struct seq_file *s, void *v)
{
	dump_save_point(&save_point_1, s);
	return 0;
}

static int clkdbg_dump_suspend_clks_2(struct seq_file *s, void *v)
{
	dump_save_point(&save_point_2, s);
	return 0;
}

static int clkdbg_dump_suspend_clks_3(struct seq_file *s, void *v)
{
	dump_save_point(&save_point_3, s);
	return 0;
}

static int clkdbg_dump_suspend_clks(struct seq_file *s, void *v)
{
	if (has_clkdbg_flag(CLKDBG_EN_SUSPEND_SAVE_3) &&
			save_point_3.spm_pwr_status != 0U)
		return clkdbg_dump_suspend_clks_3(s, v);
	else if (has_clkdbg_flag(CLKDBG_EN_SUSPEND_SAVE_2) &&
			save_point_2.spm_pwr_status != 0U)
		return clkdbg_dump_suspend_clks_2(s, v);
	else if (has_clkdbg_flag(CLKDBG_EN_SUSPEND_SAVE_1) &&
			save_point_1.spm_pwr_status != 0U)
		return clkdbg_dump_suspend_clks_1(s, v);

	return 0;
}

static int clkdbg_pm_event_handler(struct notifier_block *nb,
					unsigned long event, void *ptr)
{
	switch (event) {
	case PM_HIBERNATION_PREPARE:
	case PM_SUSPEND_PREPARE:
		/* suspend */
		if (has_clkdbg_flag(CLKDBG_EN_SUSPEND_SAVE_1)) {
			store_save_point(&save_point_1);
			return NOTIFY_OK;
		}

		break;
	case PM_POST_HIBERNATION:
	case PM_POST_SUSPEND:
		/* resume */
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block clkdbg_pm_notifier = {
	.notifier_call = clkdbg_pm_event_handler,
};

static int clkdbg_suspend_ops_valid(suspend_state_t state)
{
	return state == PM_SUSPEND_MEM ? 1 : 0;
}

static int clkdbg_suspend_ops_begin(suspend_state_t state)
{
	return 0;
}

static int clkdbg_suspend_ops_prepare(void)
{
	return 0;
}

static int clkdbg_suspend_ops_enter(suspend_state_t state)
{
	if (has_clkdbg_flag(CLKDBG_EN_SUSPEND_SAVE_3))
		store_save_point(&save_point_3);

	return 0;
}

static void clkdbg_suspend_ops_finish(void)
{
}

static void clkdbg_suspend_ops_end(void)
{
}

static const struct platform_suspend_ops clkdbg_suspend_ops = {
	.valid = clkdbg_suspend_ops_valid,
	.begin = clkdbg_suspend_ops_begin,
	.prepare = clkdbg_suspend_ops_prepare,
	.enter = clkdbg_suspend_ops_enter,
	.finish = clkdbg_suspend_ops_finish,
	.end = clkdbg_suspend_ops_end,
};

static int clkdbg_suspend_set_ops(struct seq_file *s, void *v)
{
	suspend_set_ops(&clkdbg_suspend_ops);

	return 0;
}

static const struct cmd_fn *custom_cmds;

void set_custom_cmds(const struct cmd_fn *cmds)
{
	custom_cmds = cmds;
}
EXPORT_SYMBOL(set_custom_cmds);

static int clkdbg_cmds(struct seq_file *s, void *v);

static const struct cmd_fn common_cmds[] = {
	CMDFN("dump_regs", seq_print_regs),
	CMDFN("dump_regs2", clkdbg_dump_regs2),
	CMDFN("dump_state", clkdbg_dump_state_all),
	CMDFN("dump_clks", clkdbg_dump_provider_clks),
	CMDFN("dump_muxes", clkdbg_dump_muxes),
	CMDFN("fmeter", seq_print_fmeter_all),
	CMDFN("pwr_status", clkdbg_pwr_status),
	CMDFN("prepare", clkdbg_prepare),
	CMDFN("unprepare", clkdbg_unprepare),
	CMDFN("enable", clkdbg_enable),
	CMDFN("disable", clkdbg_disable),
	CMDFN("prepare_enable", clkdbg_prepare_enable),
	CMDFN("disable_unprepare", clkdbg_disable_unprepare),
	CMDFN("prepare_enable_provider", clkdbg_prepare_enable_provider),
	CMDFN("disable_unprepare_provider", clkdbg_disable_unprepare_provider),
	CMDFN("set_parent", clkdbg_set_parent),
	CMDFN("set_rate", clkdbg_set_rate),
#if defined(CONFIG_MTK_ENG_BUILD)
	CMDFN("reg_read", clkdbg_reg_read),
	CMDFN("reg_write", clkdbg_reg_write),
	CMDFN("reg_set", clkdbg_reg_set),
	CMDFN("reg_clr", clkdbg_reg_clr),
#endif /* CONFIG_MTK_ENG_BUILD */
	CMDFN("show_flags", clkdbg_show_flags),
	CMDFN("set_flag", clkdbg_set_flag),
	CMDFN("clr_flag", clkdbg_clr_flag),
#if CLKDBG_PM_DOMAIN
	CMDFN("dump_genpd", clkdbg_dump_genpd),
	CMDFN("pm_runtime_enable", clkdbg_pm_runtime_enable),
	CMDFN("pm_runtime_disable", clkdbg_pm_runtime_disable),
	CMDFN("pm_runtime_get_sync", clkdbg_pm_runtime_get_sync),
	CMDFN("pm_runtime_put_sync", clkdbg_pm_runtime_put_sync),
	CMDFN("pwr_on", clkdbg_pwr_on),
	CMDFN("pwr_off", clkdbg_pwr_off),
	CMDFN("reg_pdrv", clkdbg_reg_pdrv),
	CMDFN("unreg_pdrv", clkdbg_unreg_pdrv),
#endif /* CLKDBG_PM_DOMAIN */
	CMDFN("suspend_set_ops", clkdbg_suspend_set_ops),
	CMDFN("dump_suspend_clks", clkdbg_dump_suspend_clks),
	CMDFN("dump_suspend_clks_1", clkdbg_dump_suspend_clks_1),
	CMDFN("dump_suspend_clks_2", clkdbg_dump_suspend_clks_2),
	CMDFN("dump_suspend_clks_3", clkdbg_dump_suspend_clks_3),
	CMDFN("cmds", clkdbg_cmds),
	{}
};

static int clkdbg_cmds(struct seq_file *s, void *v)
{
	const struct cmd_fn *cf;

	for (cf = common_cmds; cf->cmd != NULL; cf++)
		seq_printf(s, "%s\n", cf->cmd);

	for (cf = custom_cmds; cf != NULL && cf->cmd != NULL; cf++)
		seq_printf(s, "%s\n", cf->cmd);

	seq_puts(s, "\n");

	return 0;
}

static int clkdbg_show(struct seq_file *s, void *v)
{
	const struct cmd_fn *cf;
	char cmd[sizeof(last_cmd)];

	strncpy(cmd, last_cmd, sizeof(cmd));
	cmd[sizeof(cmd) - 1UL] = '\0';

	for (cf = custom_cmds; cf != NULL && cf->cmd != NULL; cf++) {
		char *c = cmd;
		char *token = strsep(&c, " ");

		if (strcmp(cf->cmd, token) == 0)
			return cf->fn(s, v);
	}

	for (cf = common_cmds; cf->cmd != NULL; cf++) {
		char *c = cmd;
		char *token = strsep(&c, " ");

		if (strcmp(cf->cmd, token) == 0)
			return cf->fn(s, v);
	}

	return 0;
}

static int clkdbg_open(struct inode *inode, struct file *file)
{
	return single_open(file, clkdbg_show, NULL);
}

static ssize_t clkdbg_write(
		struct file *file,
		const char __user *buffer,
		size_t count,
		loff_t *data)
{
	size_t len = 0;

	len = (count < (sizeof(last_cmd) - 1UL)) ?
				count : (sizeof(last_cmd) - 1UL);
	if (copy_from_user(last_cmd, buffer, len) != 0UL)
		return 0;

	last_cmd[len] = '\0';

	if (len >= 1 && last_cmd[len - 1UL] == '\n')
		last_cmd[len - 1UL] = '\0';

	return (ssize_t)len;
}

static const struct file_operations clkdbg_fops = {
	.owner		= THIS_MODULE,
	.open		= clkdbg_open,
	.read		= seq_read,
	.write		= clkdbg_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

/*
 * init functions
 */

static int __init clkdbg_debug_init(void)
{
	struct proc_dir_entry *entry;

	entry = proc_create("clkdbg", 0, 0, &clkdbg_fops);
	if (entry == 0)
		return -ENOMEM;

	set_clkdbg_flag(CLKDBG_EN_SUSPEND_SAVE_3);

	return 0;
}

static int clkdbg_syscore_suspend(void)
{
	if (has_clkdbg_flag(CLKDBG_EN_SUSPEND_SAVE_2))
		store_save_point(&save_point_2);

	return 0;
}

static void clkdbg_syscore_resume(void)
{
}

static struct syscore_ops clkdbg_syscore_ops = {
	.suspend = clkdbg_syscore_suspend,
	.resume = clkdbg_syscore_resume,
};

static int __init clkdbg_pm_init(void)
{
	int r;

	register_syscore_ops(&clkdbg_syscore_ops);
	r = register_pm_notifier(&clkdbg_pm_notifier);
	if (r != 0)
		pr_warn("%s(): register_pm_notifier(%d)\n", __func__, r);

	return r;
}

static int __init clkdbg_module_init(void)
{
	int r = 0;

	r = clkdbg_debug_init();
	if (r != 0)
		goto err;

	r = clkdbg_pm_init();
	if (r != 0)
		goto err;

err:
	return r;
}

static void __exit clkdbg_module_exit(void)
{
}

module_init(clkdbg_module_init);
module_exit(clkdbg_module_exit);
MODULE_LICENSE("GPL");
