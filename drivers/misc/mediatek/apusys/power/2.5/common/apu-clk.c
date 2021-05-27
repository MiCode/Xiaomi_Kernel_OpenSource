// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/errno.h>
#include <linux/iopoll.h>
#include <linux/pm_runtime.h>

#include "apusys_power.h"
#include "apu_common.h"
#include "apu_clk.h"
#include "apu_log.h"
#include "apu_io.h"
#include "apu_dbg.h"

static struct clk *_clk_apu_get_closest_parent(struct device *dev,
		struct apu_clk *mux, unsigned long rate)
{
	int i = 0;
	struct apu_clk *parents;
	ulong parent_rate = 0;

	parents = mux->parents;
	for (i = 0; i < parents->clk_num; i++) {
		parent_rate = clk_get_rate(parents->clks[i].clk);
		if (round_Mhz(rate, parent_rate))
			return parents->clks[i].clk;
	}
	aclk_err(dev, "[%s] \"%s\" has no parent rate close to %luMhz\n",
		__func__, __clk_get_name(mux->clks->clk), TOMHZ(rate));
	clk_apu_show_clk_info(mux, false);
	return ERR_PTR(-ENODEV);
}

static int _clk_apu_set_closest_parent(struct device *dev, struct apu_clk *mux, unsigned long rate)
{
	struct clk *new_parent = NULL;
	int ret = 0;

	new_parent = _clk_apu_get_closest_parent(dev, mux, rate);
	if (IS_ERR_OR_NULL(new_parent))
		return PTR_ERR(new_parent);

	ret = clk_set_parent(mux->clks->clk, new_parent);
	if (ret)
		aclk_err(dev, "clk \"%s\" cannot re-parent to \"%s\" for rate %lu\n",
			mux->clks->id, __clk_get_name(new_parent), TOMHZ(rate));
	return ret;
}

static int _clk_apu_set_common_parent(struct apu_clk *child, struct apu_clk *parent)
{
	int ret = -ENODEV, idx = 0;

	if (!IS_ERR_OR_NULL(child) && !IS_ERR_OR_NULL(parent)) {
		for (idx = 0; idx < child->clk_num; idx++) {
			ret = clk_set_parent(child->clks[idx].clk, parent->clks->clk);
			if (ret) {
				aclk_err(parent->dev,
					 "[%s] change \"%s\" parent to \"%s\" fail, ret %d\n",
					 __func__, child->clks[idx].id, parent->clks->id, ret);
				goto out;
			}
		}
	}

out:
	return ret;
}

static int _clk_apu_bulk_setparent(struct apu_clk *child, struct apu_clk *parent)
{
	int ret = -ENODEV, idx = 0;

	if (!IS_ERR_OR_NULL(child) && !IS_ERR_OR_NULL(parent)) {
		if (child->clk_num != parent->clk_num) {
			aclk_err(parent->dev, " clk_num of parent/child is not match\n");
			ret = -EINVAL;
			goto out;
		}

		for (idx = 0; idx < parent->clk_num; idx++) {
			ret = clk_set_parent(child->clks[idx].clk, parent->clks[idx].clk);
			if (ret) {
				aclk_err(parent->dev, " change %s's parent to %s fail, ret %d\n",
					 child->clks[idx].id, parent->clks[idx].id, ret);
				goto out;
			}
		}
	}

out:
	return ret;
}

static int _clk_apu_mux_set_rate(struct apu_clk *dst, unsigned long rate)
{
	int ret = -ENODEV;

	/* no parents initial, report error and return */
	if (IS_ERR_OR_NULL(dst->parents)) {
		aclk_err(dst->dev, "[%s] %s has no parent, ret %d\n",
			 __func__, __clk_get_name(dst->clks->clk), ret);
		clk_apu_show_clk_info(dst, false);
		goto out;
	}

	if (!IS_ERR_OR_NULL(dst)) {
		ret = _clk_apu_set_closest_parent(dst->dev, dst, rate);
		if (ret) {
			aclk_err(dst->dev, "[%s] %s set %lu fail, ret %d\n",
				 __func__, __clk_get_name(dst->clks->clk),
				 rate, ret);
			clk_apu_show_clk_info(dst, false);
			goto out;
		}
	}
out:
	return ret;
}

static inline int _clk_cal_postdiv(int *pd)
{
	static const int pds[] = {
			[1] = 0,
			[2] = 1,
			[4] = 2,
			[8] = 3,
			[16] = 4,
	};

	if (*pd > 16)
		return 0;
	return pds[*pd];
}

static int _clk_cal_pll_data(struct apu_clk_gp *aclk, int *pd, int *dds, ulong freq)
{
	ulong vco, tmp_vco;
	ulong quot, dds_0, dds_1, remain;
	u32 ls;

	/* find post diver to meet 1500Mhz <= Vco <= 3800Mhz */
	for (*pd = 1; *pd < 32; (*pd) *= 2) {
		vco = freq * (*pd);
		if (vco >= 1500 && vco <= 3800)
			break;
	}

	if (*pd > 16)
		return -EINVAL;

	/*
	 * Below is PLL's fomular:
	 * PLL out freq = VCO/postdiv.
	 * VCO = FIN(26Mhz) * N_INFO
	 *  (1500Mhz < VCO < 3800Mhz)
	 *
	 * Suppose out freq = 525Mhz, postdiv = 4,
	 * VCO = 4*525 = 2100Mhz
	 * VCO/26 = 80.769230769
	 *
	 * N_INFO[26:24] = post div
	 * N_INFO[21:14] = 80 << 14
	 * N_INFO[13:5]  = (MSB 9 bits of (0.769230769)) << 5
	 *               = (MSB 9 bits of ((2100 % 26) << 9)/26) << 5
	 *               = (MSB 9 bits of 393) << 5
	 *               = 0x189 << 5
	 * N_INFO[ 4:0]  = next MSB 5 bits of (0.769230769)
	 *               = (MSB 5 bits of ((2100 << 9) % 26) << 5)/26
	 *               = (MSB 5 bits of ((2100 << 9) % 26) << 5)/26
	 *               = 0x1b
	 *
	 * 1. Get Quotient of VCO/26Mhz
	 *
	 * 2. Get MSB 9 bist of decimal points for VCO/26
	 * A = ((VCO % 26) << 9) / 26
	 * take MSB 9 bits of A
	 * ex: A = 1011110001
	 *     MSB 9 of A = 101111000
	 */
	tmp_vco = vco;
	remain = do_div(tmp_vco, PLL_FIN);
	quot = tmp_vco;
	remain = remain << DDS0_SHIFT;
	do_div(remain, PLL_FIN);
	ls = fls(remain);
	dds_0 = (remain >> ((ls > DDS0_SHIFT) ? (ls - DDS0_SHIFT) : 0));
	dds_0 &= MASK(BIT(10));

	/*
	 * 3. Get next MSB 5 bist of decimal points for VCO/26
	 * B = (((VCO << 9) % 26) << 5) / 26
	 * take MSB 5 bits of A
	 * ex: B = 1011110001
	 *     MSB 5 of A = 10111
	 */
	tmp_vco = vco << DDS0_SHIFT;
	remain = do_div(tmp_vco, PLL_FIN);
	remain = remain << DDS1_SHIFT;
	do_div(remain, PLL_FIN);
	ls = fls(remain);
	dds_1 = (remain >> ((ls > DDS1_SHIFT) ? (ls - DDS1_SHIFT) : 0));
	dds_1 &= MASK(BIT(6));

	/* get quot, dds_0 and dds_1 together */
	*dds = quot << 14 | dds_0 << 5 | dds_1;
	*pd = _clk_cal_postdiv(pd);
	return 0;
}

static int _clk_apu_hopping_set_rate(struct apu_clk_gp *aclk, unsigned long rate)
{
	int ret = 0, parking = 0, val = 0;
	int cur_pd = 0, pd = 0, dds = 0;
	struct clk *pll_clk = NULL;
	struct apmixpll *mixpll = NULL;

	mixpll = aclk->apmix_pll->mixpll;
	/*
	 * APUPLL --> div2 --> MDLA,
	 * that is why need to multiply MDLA's rate with 2
	 *
	 * Meanwhile, the unit of input for _clk_cal_pll_data is Mhz.
	 */
	val = rate * mixpll->multiplier / 1000000;

	cur_pd = (apu_readl(mixpll->regs) >> POSDIV_SHIFT) & 0x7;

	ret = _clk_cal_pll_data(aclk, &pd, &dds, val);
	if (ret) {
		aclk_err(aclk->dev, "%luMhz get pll fail, ret %d\n", TOMHZ(rate), ret);
		goto out;
	}

	if (cur_pd != pd)
		parking = 1;

	aclk_info(aclk->dev, "[%s] c_pd/n_pd = %d/%d, parking: %d, dds/freq 0x%x/%dMhz\n",
		  __func__, cur_pd, pd, parking, dds, val);
	if (parking) {
		pll_clk = aclk->apmix_pll->clks->clk;

		/* switch all TOP_MUX to SYS_MUX */
		ret = _clk_apu_bulk_setparent(aclk->top_mux, aclk->sys_mux);
		if (ret)
			goto out;

		/* calculate target value of pll for current rate */
		val = ((1 << 31) | (pd << POSDIV_SHIFT) | dds);
		aclk_info(aclk->dev, "[%s] ori/new reg:0x%08x/0x%08x\n",
			  __func__, apu_readl(mixpll->regs), val);
		/* set up pll's value */
		apu_writel(val, mixpll->regs);
		ret = clk_set_rate(pll_clk, rate);
		if (ret) {
			aclk_err(aclk->dev, "clk %s set %lu fail, ret %d\n",
				 __clk_get_name(pll_clk), rate, ret);
			goto out;
		}

		/* switch all TOP_MUX to APMIX PLL */
		ret = _clk_apu_set_common_parent(aclk->top_mux, aclk->top_pll);
		if (ret)
			goto out;
	} else {
		/* no parking, directly program CCF's node */
		pll_clk = aclk->top_pll->clks->clk;

		ret = clk_set_rate(pll_clk, rate);
		if (ret) {
			aclk_err(aclk->dev, "clk %s set %luMhz fail, ret %d\n",
				 __clk_get_name(pll_clk), TOMHZ(rate), ret);
			goto out;
		}
	}

	if (apupw_dbg_get_loglvl() >= VERBOSE_LVL)
		clk_apu_show_clk_info(aclk->top_mux, true);
out:
	return ret;
}

static int _clk_apu_no_fhctl_set_rate(struct apu_clk_gp *aclk, unsigned long rate)
{
	int ret = 0;
	struct clk *pll_clk = NULL;

	/* switch all TOP_MUX to SYS_MUX */
	ret = _clk_apu_bulk_setparent(aclk->top_mux, aclk->sys_mux);
	if (ret)
		goto out;

	pll_clk = aclk->top_pll->clks->clk;
	if (IS_ERR_OR_NULL(pll_clk)) {
		aclk_err(aclk->dev, "[%s] pll clk not exist\n", __func__);
		ret = -ENONET;
		goto out;
	}

	/* clk_set_rate(TOP_PLL) */
	ret = clk_set_rate(pll_clk, rate);
	if (ret) {
		aclk_err(aclk->dev, "clk %s set %luMhz fail, ret %d\n",
			 __clk_get_name(pll_clk), TOMHZ(rate), ret);
		goto out;
	}

	/* switch all TOP_MUX to TOP PLL */
	ret = _clk_apu_set_common_parent(aclk->top_mux, aclk->top_pll);
	if (ret)
		goto out;

	if (apupw_dbg_get_loglvl() >= VERBOSE_LVL)
		clk_apu_show_clk_info(aclk->top_mux, true);

	aclk_info(aclk->dev, "[%s] final rate %dMhz\n", __func__, TOMHZ(rate));

out:
	return ret;
}

void clk_apu_show_clk_info(struct apu_clk *dst, bool only_active)
{
	int i = 0, j = 0, num_parent = 0;
	struct clk_hw *parent, *cur_parent, *mux_hw;

	for (i = 0; i < dst->clk_num; i++) {
		aclk_info(dst->dev, "[%d] clk \"%s\" rate %uMhz\n",
			  i, dst->clks[i].id,
			  TOMHZ(clk_get_rate(dst->clks[i].clk)));
		mux_hw = __clk_get_hw(dst->clks[i].clk);
		num_parent = clk_hw_get_num_parents(mux_hw);
		if (num_parent <= 1)
			continue;
		cur_parent = clk_hw_get_parent(mux_hw);
		for (j = 0; j < num_parent; j++) {
			parent = clk_hw_get_parent_by_index(mux_hw, j);
			if (IS_ERR_OR_NULL(parent))
				continue;
			if (cur_parent == parent) {
				aclk_info(dst->dev,
					  "\t parent %d [%s] rate %dMhz (*)\n",
					  j, clk_hw_get_name(parent),
					  TOMHZ(clk_hw_get_rate(parent)));
			} else {
				if (only_active)
					continue;
				aclk_info(dst->dev,
					  "\t parent %d [%s] rate %dMhz\n",
					  j, clk_hw_get_name(parent),
					  TOMHZ(clk_hw_get_rate(parent)));
			}
		}
	}
}

static unsigned long clk_apu_get_rate(struct apu_clk_gp *aclk)
{
	struct clk *dst = NULL;

	mutex_lock(&aclk->clk_lock);
	if (!IS_ERR_OR_NULL(aclk->top_mux))
		dst = aclk->top_mux->clks->clk;
	else if (!IS_ERR_OR_NULL(aclk->sys_mux))
		dst = aclk->sys_mux->clks->clk;
	mutex_unlock(&aclk->clk_lock);

	return clk_get_rate(dst);
}

static int clk_apu_set_rate(struct apu_clk_gp *aclk, unsigned long rate)
{
	int ret = -ENOENT;

	mutex_lock(&aclk->clk_lock);
	if (!IS_ERR_OR_NULL(aclk->top_pll) && aclk->fhctl)
		ret = _clk_apu_hopping_set_rate(aclk, rate);
	else if (!IS_ERR_OR_NULL(aclk->top_pll) && !aclk->fhctl)
		ret = _clk_apu_no_fhctl_set_rate(aclk, rate);
	else if (!IS_ERR_OR_NULL(aclk->sys_mux) && !aclk->sys_mux->fix_rate)
		ret = _clk_apu_mux_set_rate(aclk->sys_mux, rate);
	mutex_unlock(&aclk->clk_lock);

	if (ret)
		aclk_err(aclk->dev,
			 "[%s] has no pll/sys_mux to set %luMhz\n",
			 __func__, TOMHZ(rate));
	apu_get_power_info(0);
	return ret;
}

static int clk_apu_enable(struct apu_clk_gp *aclk)
{
	int ret = 0;
	struct apu_clk *dst;

	dst = aclk->sys_mux;
	if (!IS_ERR_OR_NULL(dst)) {
		if (!dst->always_on) {
			ret = clk_bulk_prepare_enable(dst->clk_num, dst->clks);
			if (ret) {
				aclk_err(aclk->dev, "[%s] %s fail, ret %d\n",
					 __func__, dst->clks->id, ret);
				goto out;
			}
			if (dst->keep_enable)
				dst->always_on = 1;
		}
		if (!dst->fix_rate) {
			ret = _clk_apu_mux_set_rate(dst, dst->def_freq);
			if (ret)
				goto out;
		}
	}

	dst = aclk->top_mux;
	if (!IS_ERR_OR_NULL(dst)) {
		if (!dst->always_on) {
			ret = clk_bulk_prepare_enable(dst->clk_num, dst->clks);
			if (ret) {
				aclk_err(aclk->dev, "[%s] %s fail, ret %d\n",
					 __func__, dst->clks->id, ret);
				goto out;
			}
			if (dst->keep_enable)
				dst->always_on = 1;
		}
		if (!dst->fix_rate) {
			ret = _clk_apu_bulk_setparent(dst, aclk->sys_mux);
			if (ret)
				goto out;
		}
		if (apupw_dbg_get_loglvl() >= VERBOSE_LVL)
			clk_apu_show_clk_info(dst, false);
	}

	dst = aclk->apmix_pll;
	if (!IS_ERR_OR_NULL(dst)) {
		if (!dst->always_on) {
			ret = clk_bulk_prepare_enable(dst->clk_num, dst->clks);
			if (ret) {
				aclk_err(aclk->dev, "[%s] fail, ret %d\n", __func__, ret);
				goto out;
			}
			if (dst->keep_enable)
				dst->always_on = 1;
		}
		if (!dst->fix_rate) {
			ret = _clk_apu_hopping_set_rate(aclk, dst->def_freq);
			if (ret)
				goto out;
		}
	}

	dst = aclk->top_pll;
	if (!IS_ERR_OR_NULL(dst)) {
		if (!dst->always_on) {
			ret = clk_bulk_prepare_enable(dst->clk_num, dst->clks);
			if (ret) {
				aclk_err(aclk->dev, "[%s] fail, ret %d\n", __func__, ret);
				goto out;
			}
			if (dst->keep_enable)
				dst->always_on = 1;
		}
		if (!dst->fix_rate) {
			ret = clk_set_rate(dst->clks->clk, dst->def_freq);
			if (ret)
				goto out;
		}
		if (apupw_dbg_get_loglvl() >= VERBOSE_LVL)
			clk_apu_show_clk_info(dst, false);
	}

out:
	apu_get_power_info(0);
	return ret;
}

static void clk_apu_disable(struct apu_clk_gp *aclk)
{
	int ret = 0;
	struct apu_clk *dst;

	dst = aclk->sys_mux;
	if (!IS_ERR_OR_NULL(dst)) {
		if (!dst->fix_rate) {
			ret = _clk_apu_mux_set_rate(dst, dst->shut_freq);
			if (ret)
				goto out;
		}
		if (!dst->always_on && !dst->keep_enable)
			clk_bulk_disable_unprepare(dst->clk_num, dst->clks);
	}

	dst = aclk->top_mux;
	if (!IS_ERR_OR_NULL(dst)) {
		if (!dst->fix_rate) {
			ret = _clk_apu_bulk_setparent(dst, aclk->sys_mux);
			if (ret)
				goto out;
		}
		if (!dst->always_on && !dst->keep_enable)
			clk_bulk_disable_unprepare(dst->clk_num, dst->clks);
	}

	dst = aclk->apmix_pll;
	if (!IS_ERR_OR_NULL(dst)) {
		if (!dst->fix_rate) {
			ret = _clk_apu_hopping_set_rate(aclk, dst->shut_freq);
			if (ret)
				goto out;
		}
		if (!dst->always_on && !dst->keep_enable)
			clk_bulk_disable_unprepare(dst->clk_num, dst->clks);
	}

	dst = aclk->top_pll;
	if (!IS_ERR_OR_NULL(dst)) {
		if (!dst->fix_rate) {
			ret = clk_set_rate(dst->clks->clk, dst->shut_freq);
			if (ret)
				goto out;
		}
		if (!dst->always_on && !dst->keep_enable)
			clk_bulk_disable_unprepare(dst->clk_num, dst->clks);
	}

out:
	apu_get_power_info(0);
}

static int clk_apu_prepare(struct apu_clk_gp *aclk)
{
	int ret = 0;
	struct apu_clk *dst = NULL;

	if (!IS_ERR_OR_NULL(aclk->sys_mux)) {
		dst = aclk->sys_mux->parents;
		if (!IS_ERR_OR_NULL(dst)) {
			ret = clk_bulk_prepare(dst->clk_num, dst->clks);
			if (ret) {
				aclk_err(aclk->dev, "[%s] sys_mux fail, ret %d\n", __func__, ret);
				goto out;
			}
		}
		dst = aclk->sys_mux;
		ret = clk_bulk_prepare(dst->clk_num, dst->clks);
		if (ret) {
			aclk_err(aclk->dev, "[%s] sys_mux fail, ret %d\n", __func__, ret);
			goto out;
		}
	}

	dst = aclk->top_mux;
	if (!IS_ERR_OR_NULL(dst)) {
		ret = clk_bulk_prepare(dst->clk_num, dst->clks);
		if (ret) {
			aclk_err(aclk->dev, "[%s] top_mux fail, ret %d\n", __func__, ret);
			goto out;
		}
	}

	dst = aclk->top_pll;
	if (!IS_ERR_OR_NULL(dst)) {
		ret = clk_bulk_prepare(dst->clk_num, dst->clks);
		if (ret) {
			aclk_err(aclk->dev, "[%s] pll fail, ret %d\n", __func__, ret);
			goto out;
		}
	}

	dst = aclk->apmix_pll;
	if (!IS_ERR_OR_NULL(dst)) {
		ret = clk_bulk_prepare(dst->clk_num, dst->clks);
		if (ret) {
			aclk_err(aclk->dev, "[%s] pll fail, ret %d\n", __func__, ret);
			goto out;
		}
	}

out:
	return ret;
}

static void clk_apu_unprepare(struct apu_clk_gp *aclk)
{
	struct apu_clk *dst;

	if (!IS_ERR_OR_NULL(aclk->sys_mux) && !aclk->sys_mux->always_on) {
		dst = aclk->sys_mux->parents;
		if (!IS_ERR_OR_NULL(dst) && !dst->always_on)
			clk_bulk_unprepare(dst->clk_num, dst->clks);
		dst = aclk->sys_mux;
		clk_bulk_unprepare(dst->clk_num, dst->clks);
	}

	dst = aclk->top_mux;
	if (!IS_ERR_OR_NULL(dst) && !dst->always_on)
		clk_bulk_unprepare(dst->clk_num, dst->clks);

	dst = aclk->top_pll;
	if (!IS_ERR_OR_NULL(dst) && !dst->always_on)
		clk_bulk_unprepare(dst->clk_num, dst->clks);

	dst = aclk->apmix_pll;
	if (!IS_ERR_OR_NULL(dst) && !dst->always_on)
		clk_bulk_unprepare(dst->clk_num, dst->clks);

}

static int clk_apu_cg_enable(struct apu_clk_gp *aclk)
{
	struct apu_cgs *dst = NULL;
	int ret = 0, idx = 0, tmp = 0;
	ulong cg_clr = 0, cg_con = 0;

	dst = aclk->cg;
	if (IS_ERR_OR_NULL(dst))
		return 0;
	for (idx = 0; idx < dst->clk_num; idx++) {
		cg_clr = (ulong)(dst->cgs[idx].regs) +
				(ulong)(dst->cgs[idx].cg_ctl[CG_CLR]);
		cg_con = (ulong)(dst->cgs[idx].regs) +
				(ulong)(dst->cgs[idx].cg_ctl[CG_CON]);
		apu_writel(0xFFFFFFFF, (void __iomem *)cg_clr);
		ret = readl_poll_timeout_atomic((void __iomem *)cg_con,
						tmp, (tmp == 0),
						POLL_INTERVAL, POLL_TIMEOUT);
		if (ret == -ETIMEDOUT) {
			aclk_err(aclk->dev, "[%s] fail, cg = 0x%x, ret %d\n",
				 __func__, tmp, ret);
			break;
		}
	}
	return ret;
}

static int clk_apu_cg_status(struct apu_clk_gp *aclk, u32 *result)
{
	struct apu_cgs *dst = NULL;
	ulong cg_con = 0;
	int idx = 0;

	dst = aclk->cg;
	if (IS_ERR_OR_NULL(dst) || IS_ERR_OR_NULL(result)) {
		aclk_err(aclk->dev, "[%s] input value are failed\n", __func__);
		return -EINVAL;
	}

	for (idx = 0; idx < dst->clk_num; idx++) {
		cg_con = (ulong)(dst->cgs[idx].regs) +
				(ulong)(dst->cgs[idx].cg_ctl[CG_CON]);
		result[idx] = apu_readl((void *)cg_con);
	}
	return 0;
}

static struct apu_clk_ops mt68xx_clk_ops = {
	.prepare = clk_apu_prepare,
	.unprepare = clk_apu_unprepare,
	.enable = clk_apu_enable,
	.disable = clk_apu_disable,
	.cg_enable = clk_apu_cg_enable,
	.cg_status = clk_apu_cg_status,
	.set_rate = clk_apu_set_rate,
	.get_rate = clk_apu_get_rate,
};

static struct apu_cg mt68xx_conn_cg[] = {
	{
		.phyaddr = 0x19029000,
		.cg_ctl = {0, 4, 8},
	},
	{
		.phyaddr = 0x19020000,
		.cg_ctl = {0, 4, 8},
	},
};

static struct apu_cgs mt68xx_conn_cgs = {
	.cgs = &mt68xx_conn_cg[0],
	.clk_num = ARRAY_SIZE(mt68xx_conn_cg),
};

static struct apu_cg mt68xx_mdla0_cg[] = {
	{
		.phyaddr = 0x19034000,
		.cg_ctl = {0, 4, 8},
	},
};

static struct apu_cgs mt68xx_mdla0_cgs = {
	.cgs = &mt68xx_mdla0_cg[0],
	.clk_num = ARRAY_SIZE(mt68xx_mdla0_cg),
};

static struct apu_cg mt68xx_mdla1_cg[] = {
	{
		.phyaddr = 0x19038000,
		.cg_ctl = {0, 4, 8},
	},
};

static struct apu_cgs mt68xx_mdla1_cgs = {
	.cgs = &mt68xx_mdla1_cg[0],
	.clk_num = ARRAY_SIZE(mt68xx_mdla1_cg),
};

static struct apu_cg mt68xx_vpu0_cg[] = {
	{
		.phyaddr = 0x19030000,
		.cg_ctl = {0x100, 0x104, 0x108},
	},
};

static struct apu_cgs mt68xx_vpu0_cgs = {
	.cgs = &mt68xx_vpu0_cg[0],
	.clk_num = ARRAY_SIZE(mt68xx_vpu0_cg),
};

static struct apu_cg mt68xx_vpu1_cg[] = {
	{
		.phyaddr = 0x19031000,
		.cg_ctl = {0x100, 0x104, 0x108},
	},
};

static struct apu_cgs mt68xx_vpu1_cgs = {
	.cgs = &mt68xx_vpu1_cg[0],
	.clk_num = ARRAY_SIZE(mt68xx_vpu1_cg),
};

static struct apu_cg mt68xx_vpu2_cg[] = {
	{
		.phyaddr = 0x19032000,
		.cg_ctl = {0x100, 0x104, 0x108},
	},
};

static struct apu_cgs mt68xx_vpu2_cgs = {
	.cgs = &mt68xx_vpu2_cg[0],
	.clk_num = ARRAY_SIZE(mt68xx_vpu2_cg),
};

struct apmixpll mt68xx_npu_mixpll_info = {
	.offset = 0x03B8,
	.multiplier = 1,
};

struct apmixpll mt68xx_apu_mixpll_info = {
	.offset = 0x03A4,
	.multiplier = 2,
};

struct apu_clk mt68xx_npu_mixpll = {
	.mixpll = &mt68xx_npu_mixpll_info,
	.clk_num = 1,
};

struct apu_clk mt68xx_apu_mixpll = {
	.mixpll = &mt68xx_apu_mixpll_info,
	.clk_num = 1,
};

struct apu_clk mt68xx_vcore_sysmux = {
	.shut_freq = 26000000,
	.keep_enable = 1,
};

struct apu_clk mt68xx_vpu_sysmux = {
	.fix_rate = 1,
};

struct apu_clk mt68xx_vpu0_topmux = {
	.always_on = 1,
	.fix_rate = 1,
};

struct apu_clk mt68xx_vpu1_topmux = {
	.always_on = 1,
	.fix_rate = 1,
};

struct apu_clk mt68xx_mdla_sysmux = {
	.fix_rate = 1,
};

struct apu_clk mt68xx_mdla0_topmux = {
	.always_on = 1,
	.fix_rate = 1,
};

struct apu_clk mt688x_mdla1_topmux = {
	.always_on = 1,
	.fix_rate = 1,
};

static struct apu_clk_gp mt68x3_core_clk_gp = {
	.sys_mux = &mt68xx_vcore_sysmux,
	.ops = &mt68xx_clk_ops,
};

static struct apu_clk_gp mt68x3_conn_clk_gp = {
	.cg = &mt68xx_conn_cgs,
	.ops = &mt68xx_clk_ops,
};

static struct apu_clk_gp mt688x_iommu_clk_gp = {
	.ops = &mt68xx_clk_ops,
};

static struct apu_clk_gp mt68x3_vpu_clk_gp = {
	.sys_mux = &mt68xx_vpu_sysmux,
	.apmix_pll = &mt68xx_npu_mixpll,
	.ops = &mt68xx_clk_ops,
	.fhctl = 1,
};

static struct apu_clk_gp mt68x3_vpu0_clk_gp = {
	.top_mux = &mt68xx_vpu0_topmux,
	.cg = &mt68xx_vpu0_cgs,
	.ops = &mt68xx_clk_ops,
};

static struct apu_clk_gp mt68x3_vpu1_clk_gp = {
	.top_mux = &mt68xx_vpu1_topmux,
	.cg = &mt68xx_vpu1_cgs,
	.ops = &mt68xx_clk_ops,
};

static struct apu_clk_gp mt688x_vpu0_clk_gp = {
	.cg = &mt68xx_vpu0_cgs,
	.ops = &mt68xx_clk_ops,
};

static struct apu_clk_gp mt688x_vpu1_clk_gp = {
	.cg = &mt68xx_vpu1_cgs,
	.ops = &mt68xx_clk_ops,
};

static struct apu_clk_gp mt688x_vpu2_clk_gp = {
	.cg = &mt68xx_vpu2_cgs,
	.ops = &mt68xx_clk_ops,
};

static struct apu_clk_gp mt6873_mdla_clk_gp = {
	.sys_mux = &mt68xx_mdla_sysmux,
	.apmix_pll = &mt68xx_apu_mixpll,
	.ops = &mt68xx_clk_ops,
	.fhctl = 1,
};

static struct apu_clk_gp mt6873_mdla0_clk_gp = {
	.top_mux = &mt68xx_mdla0_topmux,
	.cg = &mt68xx_mdla0_cgs,
	.ops = &mt68xx_clk_ops,
};

static struct apu_clk_gp mt688x_mdla_clk_gp = {
	.sys_mux = &mt68xx_mdla_sysmux,
	.ops = &mt68xx_clk_ops,
	.fhctl = 0,
};

static struct apu_clk_gp mt688x_mdla0_clk_gp = {
	.top_mux = &mt68xx_mdla0_topmux,
	.cg = &mt68xx_mdla0_cgs,
	.ops = &mt68xx_clk_ops,
};

static struct apu_clk_gp mt688x_mdla1_clk_gp = {
	.top_mux = &mt688x_mdla1_topmux,
	.cg = &mt68xx_mdla1_cgs,
	.ops = &mt68xx_clk_ops,
};

static const struct apu_clk_array apu_clk_gps[] = {
	{ .name = "mt68x3_core", .aclk_gp = &mt68x3_core_clk_gp },
	{ .name = "mt68x3_conn", .aclk_gp = &mt68x3_conn_clk_gp },
	{ .name = "mt688x_iommu", .aclk_gp = &mt688x_iommu_clk_gp },
	{ .name = "mt6873_mdla", .aclk_gp = &mt6873_mdla_clk_gp },
	{ .name = "mt6873_mdla0", .aclk_gp = &mt6873_mdla0_clk_gp },
	{ .name = "mt68x3_vpu", .aclk_gp = &mt68x3_vpu_clk_gp },
	{ .name = "mt68x3_vpu0", .aclk_gp = &mt68x3_vpu0_clk_gp },
	{ .name = "mt68x3_vpu1", .aclk_gp = &mt68x3_vpu1_clk_gp },
	{ .name = "mt688x_mdla", .aclk_gp = &mt688x_mdla_clk_gp },
	{ .name = "mt688x_mdla0", .aclk_gp = &mt688x_mdla0_clk_gp },
	{ .name = "mt688x_mdla1", .aclk_gp = &mt688x_mdla1_clk_gp },
	{ .name = "mt688x_vpu0", .aclk_gp = &mt688x_vpu0_clk_gp },
	{ .name = "mt688x_vpu1", .aclk_gp = &mt688x_vpu1_clk_gp },
	{ .name = "mt688x_vpu2", .aclk_gp = &mt688x_vpu2_clk_gp },
};

struct apu_clk_gp *clk_apu_get_clkgp(struct apu_dev *ad, const char *name)
{
	int i = 0;

	if (!name)
		goto out;

	for (i = 0; i < ARRAY_SIZE(apu_clk_gps); i++) {
		if (strcmp(name, apu_clk_gps[i].name) == 0)
			return apu_clk_gps[i].aclk_gp;
	}

	aprobe_err(ad->dev, "[%s] cannot find \"%s\" clock\n", __func__, name);
out:
	return ERR_PTR(-ENOENT);
}
