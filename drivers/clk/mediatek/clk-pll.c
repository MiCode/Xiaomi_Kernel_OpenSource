// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: James Liao <jamesjj.liao@mediatek.com>
 */

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/mfd/syscon.h>
#include <linux/slab.h>
#include <linux/clkdev.h>
#include <linux/delay.h>
#include <linux/module.h>
#include "clk-mtk.h"

#define REG_CON0		0
#define REG_CON1		4

#define CON0_BASE_EN		BIT(0)
#define CON0_PWR_ON		BIT(0)
#define CON0_ISO_EN		BIT(1)
#define PCW_CHG_MASK		BIT(31)

#define AUDPLL_TUNER_EN		BIT(31)

#define POSTDIV_MASK		0x7

/* default 7 bits integer, can be overridden with pcwibits. */
#define INTEGER_BITS		7

#define MTK_WAIT_HWV_PLL_PREPARE_CNT	10
#define MTK_WAIT_HWV_PLL_PREPARE_US		10
#define MTK_WAIT_HWV_PLL_VOTE_CNT		100
#define MTK_WAIT_HWV_PLL_VOTE_US		2
#define MTK_WAIT_HWV_PLL_DONE_CNT		10000
#define MTK_WAIT_HWV_PLL_DONE_US		10

static bool hwv_pll_prepared = true;
static bool is_registered;

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
	const struct mtk_pll_data *data;
	struct regmap	*hwv_regmap;
};

#if IS_ENABLED(CONFIG_MEDIATEK_FHCTL_V1)
bool (*mtk_fh_set_rate)(int pll_id, unsigned long dds, int postdiv) = NULL;
#else
bool (*mtk_fh_set_rate)(const char *name, unsigned long dds, int postdiv) = NULL;
#endif
EXPORT_SYMBOL(mtk_fh_set_rate);

static inline struct mtk_clk_pll *to_mtk_clk_pll(struct clk_hw *hw)
{
	return container_of(hw, struct mtk_clk_pll, hw);
}

static int mtk_pll_is_prepared(struct clk_hw *hw)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);

	if (!is_registered)
		return 0;

	return (readl(pll->en_addr) & BIT(pll->data->pll_en_bit)) != 0;
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
	if (pcwbits > ibits)
		pcwfbits = pcwbits - ibits;

	vco = (u64)fin * pcw;

	if (pcwfbits && (vco & GENMASK(pcwfbits - 1, 0)))
		c = 1;

	vco >>= pcwfbits;

	if (c)
		vco++;

	return ((unsigned long)vco + postdiv - 1) / postdiv;
}

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
	do_div(_pcw, fin);

	*pcw = (u32)_pcw;
}

static int mtk_pll_set_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long parent_rate)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);
	u32 pcw = 0;
	u32 postdiv;

	mtk_pll_calc_values(pll, &pcw, &postdiv, rate, parent_rate);
#if IS_ENABLED(CONFIG_MEDIATEK_FHCTL_V1)
	if (!mtk_fh_set_rate || !mtk_fh_set_rate(pll->data->id, pcw, postdiv))
		mtk_pll_set_rate_regs(pll, pcw, postdiv);
#else
	if (!mtk_fh_set_rate || !mtk_fh_set_rate(pll->data->name, pcw, postdiv))
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

	if (pll->data->en_mask) {
		r = readl(pll->en_addr) | pll->data->en_mask;
		writel(r, pll->en_addr);
	}

	r = readl(pll->en_addr) | BIT(pll->data->pll_en_bit);
	writel(r, pll->en_addr);

	__mtk_pll_tuner_enable(pll);

	udelay(20);

	if (pll->data->flags & HAVE_RST_BAR) {
		r = readl(pll->base_addr + REG_CON0);
		r |= pll->data->rst_bar_mask;
		writel(r, pll->base_addr + REG_CON0);
	}

	return 0;
}

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

	r = readl(pll->en_addr) & ~BIT(pll->data->pll_en_bit);
	writel(r, pll->en_addr);

	if (pll->data->en_mask) {
		r = readl(pll->en_addr) & ~pll->data->en_mask;
		writel(r, pll->en_addr);
	}

	r = readl(pll->pwr_addr) | CON0_ISO_EN;
	writel(r, pll->pwr_addr);

	r = readl(pll->pwr_addr) & ~CON0_PWR_ON;
	writel(r, pll->pwr_addr);
}

static int mtk_hwv_pll_is_prepared_done(struct mtk_clk_pll *pll)
{
	u32 val, pll_sta;

	regmap_read(pll->hwv_regmap, pll->data->hwv_done_ofs, &val);

	if ((val & BIT(pll->data->hwv_shift))) {
		if (pll->data->flags & HWV_CHK_FULL_STA) {
			regmap_read(pll->hwv_regmap, pll->data->hwv_set_sta_ofs, &val);
			pll_sta = readl(pll->en_addr) & BIT(pll->data->pll_en_bit);
			if (((val & BIT(pll->data->hwv_shift)) == 0x0)
					&& ((pll_sta & BIT(pll->data->pll_en_bit)))) {
				hwv_pll_prepared = true;
				return 1;
			}
		} else {
			hwv_pll_prepared = true;
			return 1;
		}
	}

	return 0;
}

static int mtk_hwv_pll_is_unprepared_done(struct mtk_clk_pll *pll)
{
	u32 val;

	regmap_read(pll->hwv_regmap, pll->data->hwv_done_ofs, &val);

	if ((val & BIT(pll->data->hwv_shift))) {
		if (pll->data->flags & HWV_CHK_FULL_STA) {
			regmap_read(pll->hwv_regmap, pll->data->hwv_clr_sta_ofs, &val);
			if ((val & BIT(pll->data->hwv_shift)) == 0x0) {
				hwv_pll_prepared = false;
				return 1;
			}
		} else {
			hwv_pll_prepared = false;
			return 1;
		}
	}

	return 0;
}

static int mtk_hwv_pll_is_prepared(struct clk_hw *hw)
{
	return hwv_pll_prepared;
}

static int mtk_hwv_pll_prepare(struct clk_hw *hw)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);
	u32 val = 0, val2 = 0;
	int i = 0;

	/* wait for irq idle */
	do {
		regmap_read(pll->hwv_regmap, pll->data->hwv_done_ofs, &val);
		if ((val & BIT(pll->data->hwv_shift)) != 0)
			break;

		if (i < MTK_WAIT_HWV_PLL_PREPARE_CNT)
			udelay(MTK_WAIT_HWV_PLL_PREPARE_US);
		else
			goto err_hwv_prepare;
		i++;
	} while (1);

	i = 0;

	/* dummy read to clr idle signal of hw voter bus */
	regmap_read(pll->hwv_regmap, pll->data->hwv_set_ofs, &val);
	regmap_write(pll->hwv_regmap, pll->data->hwv_set_ofs, BIT(pll->data->hwv_shift));

	do {
		regmap_read(pll->hwv_regmap, pll->data->hwv_set_ofs, &val);
		if ((val & BIT(pll->data->hwv_shift)) != 0)
			break;

		udelay(MTK_WAIT_HWV_PLL_VOTE_US);
		if (i > MTK_WAIT_HWV_PLL_VOTE_CNT)
			goto err_hwv_vote;
		i++;
	} while (1);

	i = 0;

	do {
		if (mtk_hwv_pll_is_prepared_done(pll))
			break;

		if (i < MTK_WAIT_HWV_PLL_DONE_CNT)
			udelay(MTK_WAIT_HWV_PLL_DONE_US);
		else
			goto err_hwv_done;

		i++;
	} while (1);

	return 0;

err_hwv_done:
	regmap_read(pll->hwv_regmap, pll->data->hwv_done_ofs, &val);
	regmap_read(pll->hwv_regmap, pll->data->hwv_clr_sta_ofs, &val2);
	pr_err("%s pll enable timeout(%dus)(%x %x)\n", pll->data->name,
			i * MTK_WAIT_HWV_PLL_DONE_US, val, val2);
err_hwv_vote:
	pr_err("%s pll vote timeout(%dus)(0x%x)\n", pll->data->name,
			i * MTK_WAIT_HWV_PLL_VOTE_US, val);
err_hwv_prepare:
	pr_err("%s pll prepare timeout(%dus)(0x%x)\n", pll->data->name,
			i * MTK_WAIT_HWV_PLL_PREPARE_US, val);
	mtk_clk_notify(NULL, pll->hwv_regmap, NULL,
			pll->data->hwv_set_ofs, 0,
			pll->data->hwv_shift, CLK_EVT_HWV_PLL_TIMEOUT);

	return -EBUSY;
}

static void mtk_hwv_pll_unprepare(struct clk_hw *hw)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);
	u32 val = 0, val2 = 0;
	int i = 0;

	/* wait for irq idle */
	do {
		regmap_read(pll->hwv_regmap, pll->data->hwv_done_ofs, &val);
		if ((val & BIT(pll->data->hwv_shift)) != 0)
			break;

		if (i < MTK_WAIT_HWV_PLL_PREPARE_CNT)
			udelay(MTK_WAIT_HWV_PLL_PREPARE_US);
		else
			goto err_hwv_prepare;
		i++;
	} while (1);

	i = 0;

	/* dummy read to clr idle signal of hw voter bus */
	regmap_read(pll->hwv_regmap, pll->data->hwv_clr_ofs, &val);
	regmap_write(pll->hwv_regmap, pll->data->hwv_clr_ofs, BIT(pll->data->hwv_shift));

	do {
		regmap_read(pll->hwv_regmap, pll->data->hwv_clr_ofs, &val);
		if ((val & BIT(pll->data->hwv_shift)) == 0)
			break;

		udelay(MTK_WAIT_HWV_PLL_VOTE_US);
		if (i > MTK_WAIT_HWV_PLL_VOTE_CNT)
			goto err_hwv_vote;
		i++;
	} while (1);

	i = 0;

	/* delay 100us to prevent false ack check */
	udelay(100);
	do {
		if (mtk_hwv_pll_is_unprepared_done(pll))
			break;

		if (i < MTK_WAIT_HWV_PLL_DONE_CNT)
			udelay(MTK_WAIT_HWV_PLL_DONE_US);
		else
			goto err_hwv_done;

		i++;
	} while (1);

	return;

err_hwv_done:
	regmap_read(pll->hwv_regmap, pll->data->hwv_done_ofs, &val);
	regmap_read(pll->hwv_regmap, pll->data->hwv_clr_sta_ofs, &val2);
	pr_err("%s pll disable timeout(%dus)(%x %x)\n", pll->data->name,
			i * MTK_WAIT_HWV_PLL_DONE_US, val, val2);
err_hwv_vote:
	pr_err("%s pll unvote timeout(%dus)(0x%x)\n", pll->data->name,
			i * MTK_WAIT_HWV_PLL_PREPARE_US, val);
err_hwv_prepare:
	pr_err("%s pll unprepare timeout(%dus)(0x%x)\n", pll->data->name,
			i * MTK_WAIT_HWV_PLL_PREPARE_US, val);
	mtk_clk_notify(NULL, pll->hwv_regmap, NULL,
			pll->data->hwv_set_ofs, 0,
			pll->data->hwv_shift, CLK_EVT_HWV_PLL_TIMEOUT);

	return;
}

static const struct clk_ops mtk_pll_ops = {
	.is_prepared	= mtk_pll_is_prepared,
	.prepare	= mtk_pll_prepare,
	.unprepare	= mtk_pll_unprepare,
	.recalc_rate	= mtk_pll_recalc_rate,
	.round_rate	= mtk_pll_round_rate,
	.set_rate	= mtk_pll_set_rate,
};

static const struct clk_ops mtk_hwv_pll_ops = {
	.is_prepared	= mtk_hwv_pll_is_prepared,
	.prepare	= mtk_hwv_pll_prepare,
	.unprepare	= mtk_hwv_pll_unprepare,
	.recalc_rate	= mtk_pll_recalc_rate,
	.round_rate	= mtk_pll_round_rate,
	.set_rate	= mtk_pll_set_rate,
};

static struct clk *mtk_clk_register_pll(const struct mtk_pll_data *data,
		void __iomem *base,
		struct regmap *hw_voter_regmap)
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
	if (data->pcw_chg_reg)
		pll->pcw_chg_addr = base + data->pcw_chg_reg;
	else
		pll->pcw_chg_addr = pll->base_addr + REG_CON1;
	if (data->tuner_reg)
		pll->tuner_addr = base + data->tuner_reg;
	if (data->tuner_en_reg)
		pll->tuner_en_addr = base + data->tuner_en_reg;
	if (data->en_reg)
		pll->en_addr = base + data->en_reg;
	else
		pll->en_addr = pll->base_addr + REG_CON0;

	if (hw_voter_regmap && (data->flags & CLK_USE_HW_VOTER))
		pll->hwv_regmap = hw_voter_regmap;

	pll->hw.init = &init;
	pll->data = data;

	init.name = data->name;
	init.flags = (data->flags & PLL_AO) ? CLK_IS_CRITICAL : 0;
	if (hw_voter_regmap && (data->flags & CLK_USE_HW_VOTER))
		init.ops = &mtk_hwv_pll_ops;
	else
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
		const struct mtk_pll_data *plls, int num_plls, struct clk_onecell_data *clk_data)
{
	void __iomem *base;
	int i;
	struct clk *clk;
	struct regmap *hw_voter_regmap;

	is_registered = false;

	base = of_iomap(node, 0);
	if (!base) {
		pr_err("%s(): ioremap failed\n", __func__);
		return;
	}

	hw_voter_regmap = syscon_regmap_lookup_by_phandle(node, "hw-voter-regmap");
	if (IS_ERR_OR_NULL(hw_voter_regmap))
		hw_voter_regmap = NULL;

	for (i = 0; i < num_plls; i++) {
		const struct mtk_pll_data *pll = &plls[i];

		if (IS_ERR_OR_NULL(clk_data->clks[pll->id])) {
			clk = mtk_clk_register_pll(pll, base, hw_voter_regmap);

			if (IS_ERR_OR_NULL(clk)) {
				pr_err("Failed to register clk %s: %ld\n",
						pll->name, PTR_ERR(clk));
				continue;
			}

			clk_data->clks[pll->id] = clk;
		}
	}

	is_registered = true;
}
EXPORT_SYMBOL(mtk_clk_register_plls);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek PLL");
MODULE_AUTHOR("MediaTek Inc.");
