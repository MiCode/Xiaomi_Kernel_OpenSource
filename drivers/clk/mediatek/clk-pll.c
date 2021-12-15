/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: James Liao <jamesjj.liao@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clkdev.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include <linux/mfd/syscon.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#define REG_CON0		0
#define REG_CON1		4

#define CON0_BASE_EN		BIT(0)
#define CON0_PWR_ON		BIT(0)
#define CON0_ISO_EN		BIT(1)
#define PCW_CHG_MASK		BIT(31)

#define AUDPLL_TUNER_EN		BIT(31)

#define POSTDIV_MASK		0x7
#define INTEGER_BITS		7
#define INV_OFS		-1

/*
 * MediaTek PLLs are configured through their pcw value. The pcw value describes
 * a divider in the PLL feedback loop which consists of 7 bits for the integer
 * part and the remaining bits (if present) for the fractional part. Also they
 * have a 3 bit power-of-two post divider.
 */

struct mtk_clk_pll {
	struct clk_hw	hw;
	void __iomem	*base_addr;
	void __iomem	*pd_addr;
	void __iomem	*pwr_addr;
	void __iomem	*tuner_addr;
	void __iomem	*tuner_en_addr;
	void __iomem	*pcw_addr;
	void __iomem	*pcw_chg_addr;
	void __iomem	*en_addr;
	void __iomem	*rst_bar_addr;
	const struct mtk_pll_data *data;
	uint32_t	en_mask;
	uint32_t	iso_mask;
	uint32_t	pwron_mask;
	struct pwr_status	*pwr_stat;
	struct regmap	*pwr_regmap;
};

bool (*mtk_fh_set_rate)(int pll_id, unsigned long dds, int postdiv) = NULL;
EXPORT_SYMBOL(mtk_fh_set_rate);


static inline struct mtk_clk_pll *to_mtk_clk_pll(struct clk_hw *hw)
{
	return container_of(hw, struct mtk_clk_pll, hw);
}

#if (defined(CONFIG_MACH_MT6877) \
	|| defined(CONFIG_MACH_MT6768) \
	|| defined(CONFIG_MACH_MT6781) \
	|| defined(CONFIG_MACH_MT6739))
static int is_subsys_pwr_on(struct mtk_clk_pll *pll)
{
	struct pwr_status *pwr = pll->pwr_stat;
	u32 val = 0, val2 = 0;

	if (pwr != NULL && pll->pwr_regmap != NULL) {
		if (pwr->pwr_ofs != INV_OFS && pwr->pwr2_ofs != INV_OFS) {
			regmap_read(pll->pwr_regmap, pwr->pwr_ofs, &val);
			regmap_read(pll->pwr_regmap, pwr->pwr2_ofs, &val2);

			pr_notice("stat: 0x%x, msk: 0x%x\n", val, pwr->mask);
			if ((val & pwr->mask) != pwr->val &&
					(val2 & pwr->mask) != pwr->val)
				return false;
		} else if (pwr->other_ofs != INV_OFS) {
			regmap_read(pll->pwr_regmap, pwr->other_ofs, &val);
			if ((val & pwr->mask) != pwr->val)
				return false;
		}
	}

	return true;
}
#endif
static int mtk_pll_is_prepared(struct clk_hw *hw)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);

#if (defined(CONFIG_MACH_MT6877) \
	|| defined(CONFIG_MACH_MT6768) \
	|| defined(CONFIG_MACH_MT6781) \
	|| defined(CONFIG_MACH_MT6739))
    if (is_subsys_pwr_on(pll))
		return (readl(pll->en_addr) & pll->data->en_mask) != 0;
	return false;
#else
	return (readl(pll->base_addr + REG_CON0) & CON0_BASE_EN) != 0;
#endif
}

static unsigned long __mtk_pll_recalc_rate(struct mtk_clk_pll *pll, u32 fin,
		u32 pcw, int postdiv)
{
	int pcwbits = pll->data->pcwbits;
	int pcwfbits = 0;
	int ibits;
	u64 vco;
	u8 c = 0;

	/* The fractional part of the PLL divider. */
	ibits = pll->data->pcwibits ? pll->data->pcwibits : INTEGER_BITS;
	pcwfbits = pcwbits > ibits ? pcwbits - ibits : 0;

	vco = (u64)fin * pcw;

	if (pcwfbits && (vco & GENMASK(pcwfbits - 1, 0)))
		c = 1;

	vco >>= pcwfbits;

	if (c)
		vco++;

	return ((unsigned long)vco + postdiv - 1) / postdiv;
}

#if (defined(CONFIG_MACH_MT6877) \
	|| defined(CONFIG_MACH_MT6768) \
	|| defined(CONFIG_MACH_MT6781) \
	|| defined(CONFIG_MACH_MT6739))

static void mtk_pll_set_rate_regs(struct mtk_clk_pll *pll, u32 pcw,
		int postdiv)
{
	u32 val;
	u32 tuner_en = 0;
	u32 tuner_en_mask;
	void __iomem *tuner_en_addr = NULL;

	/* disable tuner */
	if (pll->tuner_en_addr) {
		tuner_en_addr = pll->tuner_en_addr;
		tuner_en_mask = BIT(pll->data->tuner_en_bit);
	} else if (pll->tuner_addr) {
		tuner_en_addr = pll->tuner_addr;
		tuner_en_mask = AUDPLL_TUNER_EN;
	}

	if (tuner_en_addr) {
		val = readl(tuner_en_addr);
		tuner_en = val & tuner_en_mask;

		if (tuner_en) {
			val &= ~tuner_en_mask;
			writel(val, tuner_en_addr);
		}
	}

	/* set postdiv & pcw_chg */
	val = readl(pll->pd_addr);
	val &= ~(POSTDIV_MASK << pll->data->pd_shift);
	val |= (ffs(postdiv) - 1) << pll->data->pd_shift;
	val &= ~PCW_CHG_MASK;

	/* postdiv and pcw need to set at the same time if on same register */
	if (pll->pd_addr != pll->pcw_addr) {
		writel(val, pll->pd_addr);
		val = readl(pll->pcw_addr);
	}

	/* set pcw */
	val &= ~GENMASK(pll->data->pcw_shift + pll->data->pcwbits - 1,
			pll->data->pcw_shift);
	val |= pcw << pll->data->pcw_shift;
	writel(val, pll->pcw_addr);

	if (pll->tuner_addr)
		writel(val + 1, pll->tuner_addr);

	if (pll->pd_addr != pll->pcw_addr)
		val = readl(pll->pd_addr);

	val |= PCW_CHG_MASK;

	writel(val, pll->pd_addr);

	/* restore tuner_en */
	if (tuner_en_addr && tuner_en) {
		val = readl(tuner_en_addr);
		val |= tuner_en_mask;
		writel(val, tuner_en_addr);
	}

	udelay(20);
}

#else

static void __mtk_pll_tuner_enable(struct mtk_clk_pll *pll)
{
	u32 r;

	if (pll->tuner_en_addr) {
		r = readl(pll->tuner_en_addr) | BIT(pll->data->tuner_en_bit);
		writel(r, pll->tuner_en_addr);
	} else if (pll->tuner_addr) {
		r = readl(pll->tuner_addr) | AUDPLL_TUNER_EN;
		writel(r, pll->tuner_addr);
	}
}

static void __mtk_pll_tuner_disable(struct mtk_clk_pll *pll)
{
	u32 r;

	if (pll->tuner_en_addr) {
		r = readl(pll->tuner_en_addr) & ~BIT(pll->data->tuner_en_bit);
		writel(r, pll->tuner_en_addr);
	} else if (pll->tuner_addr) {
		r = readl(pll->tuner_addr) & ~AUDPLL_TUNER_EN;
		writel(r, pll->tuner_addr);
	}
}

static void mtk_pll_set_rate_regs(struct mtk_clk_pll *pll, u32 pcw,
		int postdiv)
{
	u32 chg, val;

	/* disable tuner */
	__mtk_pll_tuner_disable(pll);

	/* set postdiv */
	val = readl(pll->pd_addr);
	val &= ~(POSTDIV_MASK << pll->data->pd_shift);
	val |= (ffs(postdiv) - 1) << pll->data->pd_shift;

	/* postdiv and pcw need to set at the same time if on same register */
	if (pll->pd_addr != pll->pcw_addr) {
		writel(val, pll->pd_addr);
		val = readl(pll->pcw_addr);
	}

	/* set pcw */
	val &= ~GENMASK(pll->data->pcw_shift + pll->data->pcwbits - 1,
			pll->data->pcw_shift);
	val |= pcw << pll->data->pcw_shift;
	writel(val, pll->pcw_addr);
	chg = readl(pll->pcw_chg_addr) | PCW_CHG_MASK;
	writel(chg, pll->pcw_chg_addr);
	if (pll->tuner_addr)
		writel(val + 1, pll->tuner_addr);

	/* restore tuner_en */
	__mtk_pll_tuner_enable(pll);

	udelay(20);
}
#endif
/*
 * mtk_pll_calc_values - calculate good values for a given input frequency.
 * @pll:	The pll
 * @pcw:	The pcw value (output)
 * @postdiv:	The post divider (output)
 * @freq:	The desired target frequency
 * @fin:	The input frequency
 *
 */
static void mtk_pll_calc_values(struct mtk_clk_pll *pll, u32 *pcw, u32 *postdiv,
		u32 freq, u32 fin)
{
	unsigned long fmin = pll->data->fmin ? pll->data->fmin : (1000 * MHZ);
	const struct mtk_pll_div_table *div_table = pll->data->div_table;
	u64 _pcw;
	int ibits;
	u32 val;

	if (freq > pll->data->fmax)
		freq = pll->data->fmax;

	if (div_table) {
		if (freq > div_table[0].freq)
			freq = div_table[0].freq;

		for (val = 0; div_table[val + 1].freq != 0; val++) {
			if (freq > div_table[val + 1].freq)
				break;
		}
		*postdiv = 1 << val;
	} else {
		for (val = 0; val < 5; val++) {
			*postdiv = 1 << val;
			if ((u64)freq * *postdiv >= fmin)
				break;
		}
	}

	/* _pcw = freq * postdiv / fin * 2^pcwfbits */
	ibits = pll->data->pcwibits ? pll->data->pcwibits : INTEGER_BITS;
	_pcw = ((u64)freq << val) << (pll->data->pcwbits - ibits);
	if (fin != 0)
		_pcw = div_u64(_pcw, fin);

	*pcw = (u32)_pcw;
}

static int mtk_pll_set_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long parent_rate)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);
	u32 pcw = 0;
	u32 postdiv;

	mtk_pll_calc_values(pll, &pcw, &postdiv, rate, parent_rate);
#if (defined(CONFIG_MACH_MT6877) \
	|| defined(CONFIG_MACH_MT6768) \
	|| defined(CONFIG_MACH_MT6781) \
	|| defined(CONFIG_MACH_MT6739))

	mtk_pll_set_rate_regs(pll, pcw, postdiv);
#else

	if (!mtk_fh_set_rate || !mtk_fh_set_rate(pll->data->id, pcw, postdiv))
		mtk_pll_set_rate_regs(pll, pcw, postdiv);
#endif
	return 0;
}

static unsigned long mtk_pll_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);
	u32 postdiv;
	u32 pcw;

	postdiv = (readl(pll->pd_addr) >> pll->data->pd_shift) & POSTDIV_MASK;
	postdiv = 1 << postdiv;

	pcw = readl(pll->pcw_addr) >> pll->data->pcw_shift;
	pcw &= GENMASK(pll->data->pcwbits - 1, 0);

	return __mtk_pll_recalc_rate(pll, parent_rate, pcw, postdiv);
}

static long mtk_pll_round_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long *prate)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);
	u32 pcw = 0;
	int postdiv;

	mtk_pll_calc_values(pll, &pcw, &postdiv, rate, *prate);

	return __mtk_pll_recalc_rate(pll, *prate, pcw, postdiv);
}

#if (defined(CONFIG_MACH_MT6877) \
	|| defined(CONFIG_MACH_MT6768) \
	|| defined(CONFIG_MACH_MT6781) \
	|| defined(CONFIG_MACH_MT6739))

static int mtk_pll_prepare(struct clk_hw *hw)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);
	u32 r;

	r = readl(pll->pwr_addr) | pll->pwron_mask;

	writel(r, pll->pwr_addr);
	udelay(1);

	r = readl(pll->pwr_addr) & ~pll->iso_mask;

	writel(r, pll->pwr_addr);
	udelay(1);

	r = readl(pll->en_addr) | pll->en_mask;
	writel(r, pll->en_addr);

	if (pll->tuner_en_addr) {
		r = readl(pll->tuner_en_addr) | BIT(pll->data->tuner_en_bit);
		writel(r, pll->tuner_en_addr);
	} else if (pll->tuner_addr) {
		r = readl(pll->tuner_addr) | AUDPLL_TUNER_EN;
		writel(r, pll->tuner_addr);
	}

	udelay(20);

	if (pll->data->flags & HAVE_RST_BAR) {
		r = readl(pll->rst_bar_addr);
		r |= pll->data->rst_bar_mask;
		writel(r, pll->rst_bar_addr);
	}

	return 0;
}

#else
static int mtk_pll_prepare(struct clk_hw *hw)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);
	u32 r;

	r = readl(pll->pwr_addr) | CON0_PWR_ON;
	writel(r, pll->pwr_addr);
	udelay(1);

	r = readl(pll->pwr_addr) & ~CON0_ISO_EN;
	writel(r, pll->pwr_addr);
	udelay(1);

	r = readl(pll->en_addr) | pll->en_mask;
	writel(r, pll->en_addr);

	if (pll->data->flags & EN_BIT_CTRL) {
		r = readl(pll->en_addr) | BIT(pll->data->pll_en_bit);
		writel(r, pll->en_addr);
	}

	__mtk_pll_tuner_enable(pll);

	udelay(20);

	if (pll->data->flags & HAVE_RST_BAR) {
		r = readl(pll->base_addr + REG_CON0);
		r |= pll->data->rst_bar_mask;
		writel(r, pll->base_addr + REG_CON0);
	}

	return 0;
}
#endif

#if (defined(CONFIG_MACH_MT6877) \
	|| defined(CONFIG_MACH_MT6768) \
	|| defined(CONFIG_MACH_MT6781) \
	|| defined(CONFIG_MACH_MT6739))

static void mtk_pll_unprepare(struct clk_hw *hw)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);
	u32 r;
	u32 i;

	if (pll->data->flags & HAVE_RST_BAR_4_TIMES) {
		for (i = 0; i < 3; i++) {
			r = readl(pll->rst_bar_addr);
			r &= ~pll->data->rst_bar_mask;
			writel(r, pll->rst_bar_addr);

			udelay(1);

			r = readl(pll->rst_bar_addr);
			r |= pll->data->rst_bar_mask;
			writel(r, pll->rst_bar_addr);

			udelay(1);
		}
	}

	if (pll->data->flags & HAVE_RST_BAR) {
		r = readl(pll->rst_bar_addr);
		r &= ~pll->data->rst_bar_mask;
		writel(r, pll->rst_bar_addr);
	}

	if (pll->tuner_en_addr) {
		r = readl(pll->tuner_en_addr) & ~BIT(pll->data->tuner_en_bit);
		writel(r, pll->tuner_en_addr);
	} else if (pll->tuner_addr) {
		r = readl(pll->tuner_addr) & ~AUDPLL_TUNER_EN;
		writel(r, pll->tuner_addr);
	}

#ifdef CONFIG_MACH_MT6739
	r = readl(pll->en_addr);
	r &= ~CON0_BASE_EN;
	writel(r, pll->en_addr);
#else
	r = readl(pll->en_addr) & ~pll->en_mask;
	writel(r, pll->en_addr);
#endif

	r = readl(pll->pwr_addr) | pll->iso_mask;

	writel(r, pll->pwr_addr);

	r = readl(pll->pwr_addr) & ~pll->pwron_mask;

	writel(r, pll->pwr_addr);
}
#else

static void mtk_pll_unprepare(struct clk_hw *hw)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);
	u32 r;

	if (pll->data->flags & HAVE_RST_BAR) {
		r = readl(pll->base_addr + REG_CON0);
		r &= ~pll->data->rst_bar_mask;
		writel(r, pll->base_addr + REG_CON0);
	}

	__mtk_pll_tuner_disable(pll);

	if (pll->data->flags & EN_BIT_CTRL) {
		r = readl(pll->en_addr) & ~BIT(pll->data->pll_en_bit);
		writel(r, pll->en_addr);
	}

	r = readl(pll->en_addr) & ~pll->en_mask;
	writel(r, pll->en_addr);

	r = readl(pll->pwr_addr) | CON0_ISO_EN;
	writel(r, pll->pwr_addr);

	r = readl(pll->pwr_addr) & ~CON0_PWR_ON;
	writel(r, pll->pwr_addr);
}
#endif
#if (defined(CONFIG_MACH_MT6779) \
    || defined(CONFIG_MACH_MT6739) \
    || defined(CONFIG_MACH_MT6768) \
    || defined(CONFIG_MACH_MT6761))
 
static const struct clk_ops mtk_pll_ops = {
	.is_enabled	= mtk_pll_is_prepared,
	.enable		= mtk_pll_prepare,
	.disable	= mtk_pll_unprepare,
	.recalc_rate	= mtk_pll_recalc_rate,
	.round_rate	= mtk_pll_round_rate,
	.set_rate	= mtk_pll_set_rate,
};
#else
static const struct clk_ops mtk_pll_ops = {
	.is_prepared	= mtk_pll_is_prepared,
	.prepare	= mtk_pll_prepare,
	.unprepare	= mtk_pll_unprepare,
	.recalc_rate	= mtk_pll_recalc_rate,
	.round_rate	= mtk_pll_round_rate,
	.set_rate	= mtk_pll_set_rate,
};
#endif

static struct clk *mtk_clk_register_pll(const struct mtk_pll_data *data,
		void __iomem *base,
		struct regmap *pwr_regmap)
{
	struct mtk_clk_pll *pll;
	struct clk_init_data init = {};
	struct clk *clk;
	const char *parent_name = "clk26m";

	pll = kzalloc(sizeof(*pll), GFP_KERNEL);
	if (!pll)
		return ERR_PTR(-ENOMEM);

	pll->base_addr = base + data->reg;
	pll->pwr_addr = base + data->pwr_reg;
	pll->pd_addr = base + data->pd_reg;
	pll->pcw_addr = base + data->pcw_reg;
	if (data->en_reg)
		pll->en_addr = base + data->en_reg;
	else
		pll->en_addr = pll->base_addr + REG_CON0;
	if (data->pcw_chg_reg)
		pll->pcw_chg_addr = base + data->pcw_chg_reg;
	else
		pll->pcw_chg_addr = pll->base_addr + REG_CON1;
	if (data->rst_bar_reg)
		pll->rst_bar_addr = base + data->rst_bar_reg;
	else
		pll->rst_bar_addr = pll->base_addr + REG_CON0;
	if (data->tuner_reg)
		pll->tuner_addr = base + data->tuner_reg;
	if (data->tuner_en_reg)
		pll->tuner_en_addr = base + data->tuner_en_reg;
	if (data->en_mask)
		pll->en_mask = data->en_mask;
	else
		pll->en_mask = CON0_BASE_EN;
	if (data->iso_mask)
		pll->iso_mask = data->iso_mask;
	else
		pll->iso_mask = CON0_ISO_EN;
	if (data->pwron_mask)
		pll->pwron_mask = data->pwron_mask;
	else
		pll->pwron_mask = CON0_PWR_ON;

	if (data->pwr_stat)
		pll->pwr_stat = data->pwr_stat;
	else
		pll->pwr_stat = NULL;
	pll->pwr_regmap = pwr_regmap;

	pll->hw.init = &init;
	pll->data = data;

	init.name = data->name;
	init.flags = (data->flags & PLL_AO) ? CLK_IS_CRITICAL : 0;
	init.ops = &mtk_pll_ops;
	if (data->parent_name)
		init.parent_names = &data->parent_name;
	else
		init.parent_names = &parent_name;
	init.num_parents = 1;

	clk = clk_register(NULL, &pll->hw);

	if (IS_ERR(clk))
		kfree(pll);

	return clk;
}

void mtk_clk_register_plls(struct device_node *node,
		const struct mtk_pll_data *plls, int num_plls,
		struct clk_onecell_data *clk_data)
{
	void __iomem *base;
	int i;
	struct clk *clk;
	struct regmap *pwr_regmap;

	base = of_iomap(node, 0);
	if (!base) {
		pr_err("%s(): ioremap failed\n", __func__);
		return;
	}

	pwr_regmap = syscon_regmap_lookup_by_phandle(node, "pwr-regmap");
	if (IS_ERR(pwr_regmap))
		pwr_regmap = NULL;

	for (i = 0; i < num_plls; i++) {
		const struct mtk_pll_data *pll = &plls[i];

		clk = mtk_clk_register_pll(pll, base, pwr_regmap);

		if (IS_ERR(clk)) {
			pr_err("Failed to register clk %s: %ld\n",
					pll->name, PTR_ERR(clk));
			continue;
		}

		clk_data->clks[pll->id] = clk;
	}
}

EXPORT_SYMBOL(mtk_clk_register_plls);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek PLL");
MODULE_AUTHOR("MediaTek Inc.");
