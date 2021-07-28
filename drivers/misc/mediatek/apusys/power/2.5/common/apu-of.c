// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/coupler.h>

#include "apu_log.h"
#include "apu_of.h"
#include "apu_plat.h"
#include "apu_common.h"
#include "apu_dbg.h"

static int _apu_apmix_pll_init(struct apu_clk **dst)
{
	struct device_node *np;
	struct device *dev = (*dst)->dev;

	/* Init APMIXED base address */
	np = of_parse_phandle(dev->of_node, "mediatek,apmixed", 0);
	if (!np) {
		aprobe_err(dev, "%s has no pll info\n", __func__);
		return -ENODEV;
	}
	(*dst)->mixpll->regs = of_iomap(np, 0) + (*dst)->mixpll->offset;

	return 0;
}

static int _allocate_clk_bulk(struct apu_clk **dst, const char *id, int count)
{
	/* if passing NULL apu_clk, create it and clean the memory */
	if (!(*dst)) {
		*dst = kzalloc(sizeof(**dst), GFP_KERNEL);
		if (!(*dst))
			return -ENOMEM;
		(*dst)->dynamic_alloc = 1;
	}

	(*dst)->clks = kmalloc_array(count, sizeof(struct clk_bulk_data), GFP_KERNEL);
	if (!((*dst)->clks))
		return -ENOMEM;

	/* clear clks in apu_clk */
	memset((*dst)->clks, 0, count * sizeof(struct clk_bulk_data));

	return 0;
}

static void _free_clk_bulk(struct apu_clk **dst)
{
	if ((*dst)->dynamic_alloc)
		kfree(*dst);
}

static int _apu_buck_clk_get(struct device *dev,	const char *id,
			int num_clks, struct apu_clk **dst)
{
	struct of_phandle_args clkspec;
	int index, ret = 0;
	struct clk *clk = NULL;

	ret = _allocate_clk_bulk(dst, id, num_clks);
	if (ret)
		goto err;

	(*dst)->dev = dev;
	for (index = 0; index < num_clks; index++) {
		ret = of_parse_phandle_with_args(dev->of_node,
							id, "#clock-cells", index, &clkspec);
		if (ret < 0) {
			/* skip empty (null) phandles */
			if (ret == -ENOENT)
				continue;
			else
				goto err;
		}
		clk = of_clk_get_from_provider(&clkspec);
		if (IS_ERR(clk)) {
			if (PTR_ERR(clk) != -EPROBE_DEFER)
				aclk_err(dev, "[%s] fail get parent clock %d for %pOF\n",
					__func__, index, dev->of_node);
			ret = PTR_ERR(clk);
			goto err;
		}
		/* assign clk and consumer's name to array element */
		(*dst)->clks[index].clk = clk;
		(*dst)->clks[index].id = __clk_get_name(clk);

		if (!strcmp(id, APMIX_PLL_NODE)) {
			ret = _apu_apmix_pll_init(dst);
			if (ret)
				goto err;
		}
	}

	/* assign clk_num at the end, since put operation will check clk_num */
	(*dst)->clk_num = num_clks;

	if (apupw_dbg_get_loglvl() >= VERBOSE_LVL)
		clk_apu_show_clk_info(*dst, false);
	return 0;

err:
	_free_clk_bulk(dst);
	return ret;
}

static void _apu_buck_clk_put(struct apu_clk **dst)
{

	if (!IS_ERR_OR_NULL((*dst)->clks) && (*dst)->clk_num > 0) {
		clk_bulk_put_all((*dst)->clk_num, (*dst)->clks);
		if (!IS_ERR_OR_NULL((*dst)->mixpll) && !((*dst)->mixpll->regs))
			iounmap((*dst)->mixpll->regs);
		_free_clk_bulk(dst);
	}
}

static bool _apu_is_ancestor_of(struct device_node *test_ancestor,
			      struct device_node *child)
{
	of_node_get(child);
	while (child) {
		if (child == test_ancestor) {
			of_node_put(child);
			return true;
		}
		child = of_get_next_parent(child);
	}
	return false;
}

int of_apu_clk_get(struct device *dev, const char *id, struct apu_clk **dst)
{
	int count = 0, ret = 0;

	count = of_count_phandle_with_args(dev->of_node, id, "#clock-cells");
	if (count > 0) {
		aclk_info(dev, "[%s] %s has %d clks\n", __func__, id, count);
		ret = _apu_buck_clk_get(dev, id, count, dst);
	}
	return ret;
}

void of_apu_clk_put(struct apu_clk **dst)
{
	if (!IS_ERR_OR_NULL(*dst))
		_apu_buck_clk_put(dst);
}

int of_apu_cg_get(struct device *dev,	struct apu_cgs **dst)
{
	int ret = 0, idx = 0;

	if (!(*dst)) {
		aclk_info(dev, "[%s] has no cg\n", __func__);
		return 0;
	}

	(*dst)->dev = dev;
	for (idx = 0; idx < (*dst)->clk_num; idx++) {

		(*dst)->cgs[idx].regs =
			ioremap((*dst)->cgs[idx].phyaddr, PAGE_SIZE);
		if (!((*dst)->cgs[idx].regs)) {
			aclk_err(dev, "[%s] cannot iomap pa:0x%llx\n",
					__func__, (*dst)->cgs[idx].phyaddr);
			ret = -ENOMEM;
			break;
		}
	}

	return ret;

}

void of_apu_cg_put(struct apu_cgs **dst)
{
	int idx = 0;

	for (idx = 0; idx < (*dst)->clk_num; idx++)
		iounmap((*dst)->cgs[idx].regs);
}

void of_apu_regulator_put(struct apu_regulator *rgul)
{
	if (rgul->enabled)
		regulator_disable(rgul->vdd);
	regulator_put(rgul->vdd);
}

int of_apu_regulator_get(struct device *dev,
		struct apu_regulator *rgul, unsigned long def_volt, ulong def_freq)
{
	int ret = 0;

	if (IS_ERR_OR_NULL(rgul))
		goto out;

	rgul->vdd = regulator_get_optional(dev, rgul->name);
	if (IS_ERR_OR_NULL(rgul->vdd)) {
		ret = PTR_ERR(rgul->vdd);
		aprobe_err(dev, "[%s] %s not get, ret = %d\n",
			__func__, rgul->name, ret);
		goto out;
	}

	if (!rgul->def_volt)
		rgul->def_volt = def_volt;
	if (!rgul->shut_volt)
		rgul->shut_volt = def_volt;

	rgul->cur_volt = regulator_get_voltage(rgul->vdd);
	aprobe_info(dev, "[%s] %s cur/def/shut %dmV/%dmV/%dmV\n",
		    __func__, rgul->name, TOMV(rgul->cur_volt), TOMV(rgul->def_volt),
		    TOMV(rgul->shut_volt));

	if (rgul->constrain_band) {
		if (rgul->constrain_volt)
			goto out;

		/* get the next above slowest frq in opp and set it as constrain voltage */
		def_freq += KHZ;
		ret = apu_get_recommend_freq_volt(dev, &def_freq, &def_volt, 0);
		if (ret)
			goto out;

		rgul->constrain_volt = def_volt;
		aprobe_info(dev, "[%s] %s constrain %dmV\n",
			    __func__, rgul->name, TOMV(rgul->constrain_volt));
	}

	/* register notification function on the notifier's chain */
	if (rgul->notify_reg) {
		BLOCKING_INIT_NOTIFIER_HEAD(&rgul->notify_reg->nf_head);
		rgul->nb.notifier_call = rgul->notify_func;
		ret = regulator_apu_register_notifier(rgul->notify_reg, &rgul->nb);
		if (ret)
			aprobe_err(dev, "[%s] failed to register notifier on %s, ret %d\n",
				__func__, rgul->notify_reg->name, ret);
	}
	/* initial deffer functino if need */
	if (*(rgul->deffer_func))
		INIT_WORK(&rgul->deffer_work, rgul->deffer_func);
out:
	return ret;
}

/**
 * of_link_to_phandle - Add device link to supplier from supplier phandle
 * @dev: consumer device
 * @sup_np: phandle to supplier device tree node
 *
 * Given a phandle to a supplier device tree node (@sup_np), this function
 * finds the device that owns the supplier device tree node and creates a
 * device link from @dev consumer device to the supplier device. This function
 * doesn't create device links for invalid scenarios such as trying to create a
 * link with a parent device as the consumer of its child device. In such
 * cases, it returns an error.
 *
 * Returns:
 * - 0 if link successfully created to supplier
 * - -EAGAIN if linking to the supplier should be reattempted
 * - -EINVAL if the supplier link is invalid and should not be created
 * - -ENODEV if there is no device that corresponds to the supplier phandle
 */
int of_apu_link(struct device *dev, struct device_node *con_np, struct device_node *sup_np,
			      u32 dl_flags)
{
	struct device *sup_dev, *con_dev;
	int ret = 0;
	struct platform_device *pdev;

	if (!sup_np || !con_np) {
		dev_info(dev, "Not linking %pOFP - %pOFP\n", con_np, sup_np);
		return 0;
	}

	/*
	 * Don't allow linking a device node as a consumer of one of its
	 * descendant nodes. By definition, a child node can't be a functional
	 * dependency for the parent node.
	 */
	if (_apu_is_ancestor_of(con_np, sup_np)) {
		aprobe_err(dev, "%pOFP is ancestor of %pOFP\n", con_np, sup_np);
		return -EINVAL;
	}

	pdev = of_find_device_by_node(con_np);
	con_dev = &pdev->dev;
	pdev = of_find_device_by_node(sup_np);
	sup_dev = &pdev->dev;
	if (sup_dev && of_node_check_flag(sup_np, OF_POPULATED) &&
		con_dev && of_node_check_flag(con_np, OF_POPULATED)) {

		get_device(sup_dev);
		get_device(con_dev);
		if (!device_link_add(con_dev, sup_dev, dl_flags)) {
			aprobe_err(dev, "Not linking %pOFP - %pOFP\n", con_np, sup_np);
			ret = -EINVAL;
		}
		put_device(sup_dev);
		put_device(con_dev);
	} else {
		ret = -EPROBE_DEFER;
	}
	return ret;
}
