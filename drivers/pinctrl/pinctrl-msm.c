/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#include <linux/err.h>
#include <linux/irqdomain.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/pinctrl/machine.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include "core.h"
#include "pinconf.h"
#include "pinctrl-msm.h"

/**
 * struct msm_pinctrl_dd: represents the pinctrol driver data.
 * @base: virtual base of TLMM.
 * @irq: interrupt number for TLMM summary interrupt.
 * @num_pins: Number of total pins present on TLMM.
 * @msm_pindesc: list of descriptors for each pin.
 * @num_pintypes: number of pintypes on TLMM.
 * @msm_pintype: points to the representation of all pin types supported.
 * @pctl: pin controller instance managed by the driver.
 * @pctl_dev: pin controller descriptor registered with the pinctrl subsystem.
 * @pin_grps: list of pin groups available to the driver.
 * @num_grps: number of groups.
 * @pmx_funcs:list of pin functions available to the driver
 * @num_funcs: number of functions.
 * @dev: pin contol device.
 */
struct msm_pinctrl_dd {
	void __iomem *base;
	int	irq;
	unsigned int num_pins;
	struct msm_pindesc *msm_pindesc;
	unsigned int num_pintypes;
	struct msm_pintype_info *msm_pintype;
	struct pinctrl_desc pctl;
	struct pinctrl_dev *pctl_dev;
	struct msm_pin_grps *pin_grps;
	unsigned int num_grps;
	struct  msm_pmx_funcs *pmx_funcs;
	unsigned int num_funcs;
	struct device *dev;
};

static int msm_pmx_functions_count(struct pinctrl_dev *pctldev)
{
	struct msm_pinctrl_dd *dd;

	dd = pinctrl_dev_get_drvdata(pctldev);
	return dd->num_funcs;
}

static const char *msm_pmx_get_fname(struct pinctrl_dev *pctldev,
						unsigned selector)
{
	struct msm_pinctrl_dd *dd;

	dd = pinctrl_dev_get_drvdata(pctldev);
	return dd->pmx_funcs[selector].name;
}

static int msm_pmx_get_groups(struct pinctrl_dev *pctldev,
		unsigned selector, const char * const **groups,
		unsigned * const num_groups)
{
	struct msm_pinctrl_dd *dd;

	dd = pinctrl_dev_get_drvdata(pctldev);
	*groups = dd->pmx_funcs[selector].gps;
	*num_groups = dd->pmx_funcs[selector].num_grps;
	return 0;
}

static void msm_pmx_prg_fn(struct pinctrl_dev *pctldev, unsigned selector,
					unsigned group, bool enable)
{
	struct msm_pinctrl_dd *dd;
	const unsigned int *pins;
	struct msm_pindesc *pindesc;
	struct msm_pintype_info *pinfo;
	unsigned int pin, cnt, func;

	dd = pinctrl_dev_get_drvdata(pctldev);
	pins = dd->pin_grps[group].pins;
	pindesc = dd->msm_pindesc;

	/*
	 * for each pin in the pin group selected, program the correspoding
	 * pin function number in the config register.
	 */
	for (cnt = 0; cnt < dd->pin_grps[group].num_pins; cnt++) {
		pin = pins[cnt];
		pinfo = pindesc[pin].pin_info;
		pin = pin - pinfo->pin_start;
		func = dd->pin_grps[group].func;
		pinfo->prg_func(pin, func, pinfo->reg_base, enable);
	}
}

static int msm_pmx_enable(struct pinctrl_dev *pctldev, unsigned selector,
					unsigned group)
{
	msm_pmx_prg_fn(pctldev, selector, group, true);
	return 0;
}

static void msm_pmx_disable(struct pinctrl_dev *pctldev,
					unsigned selector, unsigned group)
{
	msm_pmx_prg_fn(pctldev, selector, group, false);
}

/* Enable gpio function for a pin */
static int msm_pmx_gpio_request(struct pinctrl_dev *pctldev,
				struct pinctrl_gpio_range *grange,
				unsigned pin)
{
	struct msm_pinctrl_dd *dd;
	struct msm_pindesc *pindesc;
	struct msm_pintype_info *pinfo;

	dd = pinctrl_dev_get_drvdata(pctldev);
	pindesc = dd->msm_pindesc;
	pinfo = pindesc[pin].pin_info;
	/* All TLMM versions use function 0 for gpio function */
	pinfo->prg_func(pin, 0, pinfo->reg_base, true);
	return 0;
}

static struct pinmux_ops msm_pmxops = {
	.get_functions_count	= msm_pmx_functions_count,
	.get_function_name	= msm_pmx_get_fname,
	.get_function_groups	= msm_pmx_get_groups,
	.enable			= msm_pmx_enable,
	.disable		= msm_pmx_disable,
	.gpio_request_enable	= msm_pmx_gpio_request,
};

static int msm_pconf_prg(struct pinctrl_dev *pctldev, unsigned int pin,
				unsigned long *config, bool rw)
{
	struct msm_pinctrl_dd *dd;
	struct msm_pindesc *pindesc;
	struct msm_pintype_info *pinfo;

	dd = pinctrl_dev_get_drvdata(pctldev);
	pindesc = dd->msm_pindesc;
	pinfo = pindesc[pin].pin_info;
	pin = pin - pinfo->pin_start;
	return pinfo->prg_cfg(pin, config, pinfo->reg_base, rw);
}

static int msm_pconf_set(struct pinctrl_dev *pctldev, unsigned int pin,
				unsigned long config)
{
	return msm_pconf_prg(pctldev, pin, &config, true);
}

static int msm_pconf_get(struct pinctrl_dev *pctldev, unsigned int pin,
					unsigned long *config)
{
	return msm_pconf_prg(pctldev, pin, config, false);
}

static int msm_pconf_group_set(struct pinctrl_dev *pctldev,
			unsigned group, unsigned long config)
{
	struct msm_pinctrl_dd *dd;
	const unsigned int *pins;
	unsigned int cnt;

	dd = pinctrl_dev_get_drvdata(pctldev);
	pins = dd->pin_grps[group].pins;

	for (cnt = 0; cnt < dd->pin_grps[group].num_pins; cnt++)
		msm_pconf_set(pctldev, pins[cnt], config);

	return 0;
}

static int msm_pconf_group_get(struct pinctrl_dev *pctldev,
				unsigned int group, unsigned long *config)
{
	struct msm_pinctrl_dd *dd;
	const unsigned int *pins;

	dd = pinctrl_dev_get_drvdata(pctldev);
	pins = dd->pin_grps[group].pins;
	msm_pconf_get(pctldev, pins[0], config);
	return 0;
}

static struct pinconf_ops msm_pconfops = {
	.pin_config_get		= msm_pconf_get,
	.pin_config_set		= msm_pconf_set,
	.pin_config_group_get	= msm_pconf_group_get,
	.pin_config_group_set	= msm_pconf_group_set,
};

static int msm_get_grps_count(struct pinctrl_dev *pctldev)
{
	struct msm_pinctrl_dd *dd;

	dd = pinctrl_dev_get_drvdata(pctldev);
	return dd->num_grps;
}

static const char *msm_get_grps_name(struct pinctrl_dev *pctldev,
						unsigned selector)
{
	struct msm_pinctrl_dd *dd;

	dd = pinctrl_dev_get_drvdata(pctldev);
	return dd->pin_grps[selector].name;
}

static int msm_get_grps_pins(struct pinctrl_dev *pctldev,
		unsigned selector, const unsigned **pins, unsigned *num_pins)
{
	struct msm_pinctrl_dd *dd;

	dd = pinctrl_dev_get_drvdata(pctldev);
	*pins = dd->pin_grps[selector].pins;
	*num_pins = dd->pin_grps[selector].num_pins;
	return 0;
}

static struct msm_pintype_info *msm_pgrp_to_pintype(struct device_node *nd,
						struct msm_pinctrl_dd *dd)
{
	struct device_node *ptype_nd;
	struct msm_pintype_info *pinfo = NULL;
	int idx = 0;

	/*Extract pin type node from parent node */
	ptype_nd = of_parse_phandle(nd, "qcom,pins", 0);
	/* find the pin type info for this pin type node */
	for (idx = 0; idx < dd->num_pintypes; idx++) {
		pinfo = &dd->msm_pintype[idx];
		if (ptype_nd == pinfo->node) {
			of_node_put(ptype_nd);
			break;
		}
	}
	return pinfo;
}

/* create pinctrl_map entries by parsing device tree nodes */
static int msm_dt_node_to_map(struct pinctrl_dev *pctldev,
			struct device_node *cfg_np, struct pinctrl_map **maps,
			unsigned *nmaps)
{
	struct msm_pinctrl_dd *dd;
	struct device_node *parent;
	struct msm_pindesc *pindesc;
	struct msm_pintype_info *pinfo;
	struct pinctrl_map *map;
	const char *grp_name;
	char *fn_name;
	u32 val;
	unsigned long *cfg;
	int cfg_cnt = 0, map_cnt = 0, func_cnt = 0, ret = 0;

	dd = pinctrl_dev_get_drvdata(pctldev);
	pindesc = dd->msm_pindesc;
	/* get parent node of config node */
	parent = of_get_parent(cfg_np);
	/*
	 * parent node contains pin grouping
	 * get pin type from pin grouping
	 */
	pinfo = msm_pgrp_to_pintype(parent, dd);
	/* check if there is a function associated with the parent pin group */
	if (of_find_property(parent, "qcom,pin-func", NULL))
		func_cnt++;
	/* get pin configs */
	ret = pinconf_generic_parse_dt_config(cfg_np, &cfg, &cfg_cnt);
	if (ret) {
		dev_err(dd->dev, "properties incorrect\n");
		return ret;
	}

	map_cnt = cfg_cnt + func_cnt;

	/* Allocate memory for pin-map entries */
	map = kzalloc(sizeof(*map) * map_cnt, GFP_KERNEL);
	if (!map)
		return -ENOMEM;
	*nmaps = 0;

	/* Get group name from node */
	of_property_read_string(parent, "label", &grp_name);
	/* create the config map entry */
	map[*nmaps].data.configs.group_or_pin = grp_name;
	map[*nmaps].data.configs.configs = cfg;
	map[*nmaps].data.configs.num_configs = cfg_cnt;
	map[*nmaps].type = PIN_MAP_TYPE_CONFIGS_GROUP;
	*nmaps += 1;

	/* If there is no function specified in device tree return */
	if (func_cnt == 0) {
		*maps = map;
		goto no_func;
	}
	/* Get function mapping */
	of_property_read_u32(parent, "qcom,pin-func", &val);
	fn_name = kzalloc(strlen(grp_name) + strlen("-func"),
						GFP_KERNEL);
	if (!fn_name) {
		ret = -ENOMEM;
		goto func_err;
	}
	snprintf(fn_name, strlen(grp_name) + strlen("-func") + 1, "%s%s",
						grp_name, "-func");
	map[*nmaps].data.mux.group = grp_name;
	map[*nmaps].data.mux.function = fn_name;
	map[*nmaps].type = PIN_MAP_TYPE_MUX_GROUP;
	*nmaps += 1;
	*maps = map;
	of_node_put(parent);
	return 0;

func_err:
	kfree(cfg);
	kfree(map);
no_func:
	of_node_put(parent);
	return ret;
}

/* free the memory allocated to hold the pin-map table */
static void msm_dt_free_map(struct pinctrl_dev *pctldev,
			     struct pinctrl_map *map, unsigned num_maps)
{
	int idx;

	for (idx = 0; idx < num_maps; idx++) {
		if (map[idx].type == PIN_MAP_TYPE_CONFIGS_GROUP)
			kfree(map[idx].data.configs.configs);
		else if (map->type == PIN_MAP_TYPE_MUX_GROUP)
			kfree(map[idx].data.mux.function);
	};

	kfree(map);
}

static struct pinctrl_ops msm_pctrlops = {
	.get_groups_count	= msm_get_grps_count,
	.get_group_name		= msm_get_grps_name,
	.get_group_pins		= msm_get_grps_pins,
	.dt_node_to_map		= msm_dt_node_to_map,
	.dt_free_map		= msm_dt_free_map,
};

static int msm_pinctrl_request_gpio(struct gpio_chip *gc, unsigned offset)
{
	return pinctrl_request_gpio(gc->base + offset);
}

static void msm_pinctrl_free_gpio(struct gpio_chip *gc, unsigned offset)
{
	pinctrl_free_gpio(gc->base + offset);
}

static int msm_of_get_pin(struct device_node *np, int index,
				struct msm_pinctrl_dd *dd, uint *pin)
{
	struct of_phandle_args pargs;
	struct msm_pintype_info *pinfo, *pintype;
	int num_pintypes;
	int ret, i;

	ret = of_parse_phandle_with_args(np, "qcom,pins", "#qcom,pin-cells",
								index, &pargs);
	if (ret)
		return ret;
	pintype = dd->msm_pintype;
	num_pintypes = dd->num_pintypes;
	for (i = 0; i < num_pintypes; i++)  {
		pinfo = &pintype[i];
		/* Find the matching pin type node */
		if (pargs.np != pinfo->node)
			continue;
		/* Check if arg specified is in valid range for pin type */
		if (pargs.args[0] > pinfo->num_pins) {
			ret = -EINVAL;
			dev_err(dd->dev, "Invalid pin number for type %s\n",
								pinfo->name);
			goto out;
		}
		/*
		 * Pin number = index within pin type + start of pin numbers
		 * for this pin type
		 */
		*pin = pargs.args[0] + pinfo->pin_start;
	}
out:
	of_node_put(pargs.np);
	return ret;
}

static int msm_pinctrl_dt_parse_pins(struct device_node *dev_node,
						struct msm_pinctrl_dd *dd)
{
	struct device *dev;
	struct device_node *pgrp_np;
	struct msm_pin_grps *pin_grps, *curr_grp;
	struct msm_pmx_funcs *pmx_funcs, *curr_func;
	char *func_name;
	const char *grp_name;
	int ret, i, grp_index = 0, func_index = 0;
	uint pin = 0, *pins, num_grps = 0, num_pins = 0, len = 0;
	uint num_funcs = 0;
	u32 func = 0;

	dev = dd->dev;
	for_each_child_of_node(dev_node, pgrp_np) {
		if (!of_find_property(pgrp_np, "qcom,pins", NULL))
			continue;
		if (of_find_property(pgrp_np, "qcom,pin-func", NULL))
			num_funcs++;
		num_grps++;
	}

	pin_grps = (struct msm_pin_grps *)devm_kzalloc(dd->dev,
						sizeof(*pin_grps) * num_grps,
						GFP_KERNEL);
	if (!pin_grps) {
		dev_err(dev, "Failed to allocate grp desc\n");
		return -ENOMEM;
	}
	pmx_funcs = (struct msm_pmx_funcs *)devm_kzalloc(dd->dev,
						sizeof(*pmx_funcs) * num_funcs,
						GFP_KERNEL);
	if (!pmx_funcs) {
		dev_err(dev, "Failed to allocate grp desc\n");
		return -ENOMEM;
	}
	/*
	 * Iterate over all child nodes, and for nodes containing pin lists
	 * populate corresponding pin group, and if provided, corresponding
	 * function
	 */
	for_each_child_of_node(dev_node, pgrp_np) {
		if (!of_find_property(pgrp_np, "qcom,pins", NULL))
			continue;
		curr_grp = pin_grps + grp_index;
		/* Get group name from label*/
		ret = of_property_read_string(pgrp_np, "label", &grp_name);
		if (ret) {
			dev_err(dev, "Unable to allocate group name\n");
			return ret;
		}
		ret = of_property_read_u32(pgrp_np, "qcom,num-grp-pins",
								&num_pins);
		if (ret) {
			dev_err(dev, "pin count not specified for groups %s\n",
								grp_name);
			return ret;
		}
		pins = devm_kzalloc(dd->dev, sizeof(unsigned int) * num_pins,
						GFP_KERNEL);
		if (!pins) {
			dev_err(dev, "Unable to allocte pins for %s\n",
								grp_name);
			return -ENOMEM;
		}
		for (i = 0; i < num_pins; i++) {
			ret = msm_of_get_pin(pgrp_np, i, dd, &pin);
			if (ret) {
				dev_err(dev, "Pin grp %s does not have pins\n",
								grp_name);
				return ret;
			}
			pins[i] = pin;
		}
		curr_grp->pins = pins;
		curr_grp->num_pins = num_pins;
		curr_grp->name = grp_name;
		grp_index++;
		/* Check if func specified */
		if (!of_find_property(pgrp_np, "qcom,pin-func", NULL))
			continue;
		curr_func = pmx_funcs + func_index;
		len = strlen(grp_name) + strlen("-func") + 1;
		func_name = devm_kzalloc(dev, len, GFP_KERNEL);
		if (!func_name) {
			dev_err(dev, "Cannot allocate func name for grp %s",
								grp_name);
			return -ENOMEM;
		}
		snprintf(func_name, len, "%s%s", grp_name, "-func");
		curr_func->name = func_name;
		curr_func->gps = devm_kzalloc(dev, sizeof(char *), GFP_KERNEL);
		if (!curr_func->gps) {
			dev_err(dev, "failed to alloc memory for group list ");
			return -ENOMEM;
		}
		of_property_read_u32(pgrp_np, "qcom,pin-func", &func);
		curr_grp->func = func;
		curr_func->gps[0] = grp_name;
		curr_func->num_grps = 1;
		func_index++;
	}
	dd->pin_grps = pin_grps;
	dd->num_grps = num_grps;
	dd->pmx_funcs = pmx_funcs;
	dd->num_funcs = num_funcs;
	return 0;
}

static void msm_populate_pindesc(struct msm_pintype_info *pinfo,
					struct msm_pindesc *msm_pindesc)
{
	int i;
	struct msm_pindesc *pindesc;

	for (i = 0; i < pinfo->num_pins; i++) {
		pindesc = &msm_pindesc[i + pinfo->pin_start];
		pindesc->pin_info = pinfo;
		snprintf(pindesc->name, sizeof(pindesc->name),
					"%s-%d", pinfo->name, i);
	}
}

static bool msm_pintype_supports_gpio(struct msm_pintype_info *pinfo)
{
	struct device_node *pt_node;

	if (!pinfo->node)
		return false;

	for_each_child_of_node(pinfo->node, pt_node) {
		if (of_find_property(pt_node, "gpio-controller", NULL)) {
			pinfo->gc.of_node = pt_node;
			pinfo->supports_gpio = true;
			return true;
		}
	}
	return false;
}

static bool msm_pintype_supports_irq(struct msm_pintype_info *pinfo)
{
	struct device_node *pt_node;

	if (!pinfo->init_irq)
		return false;
	for_each_child_of_node(pinfo->node, pt_node) {
		if (of_find_property(pt_node, "interrupt-controller", NULL)) {
			pinfo->irq_chip->node = pt_node;
			return true;
		}
	}
	return false;
}

static int msm_pinctrl_dt_parse_pintype(struct device_node *dev_node,
						struct msm_pinctrl_dd *dd)
{
	struct device_node *pt_node;
	struct msm_pindesc *msm_pindesc;
	struct msm_pintype_info *pintype, *pinfo;
	void __iomem **ptype_base;
	u32 num_pins, pinfo_entries, curr_pins;
	int i, ret;
	uint total_pins = 0;

	pinfo = dd->msm_pintype;
	pinfo_entries = dd->num_pintypes;
	curr_pins = 0;

	for_each_child_of_node(dev_node, pt_node) {
		for (i = 0; i < pinfo_entries; i++) {
			pintype = &pinfo[i];
			/* Check if node is pintype node */
			if (!of_find_property(pt_node, pintype->prop_name,
									NULL))
				continue;
			of_node_get(pt_node);
			pintype->node = pt_node;
			/* determine number of pins of given pin type */
			ret = of_property_read_u32(pt_node, "qcom,num-pins",
								&num_pins);
			if (ret) {
				dev_err(dd->dev, "num pins not specified\n");
				return ret;
			}
			/* determine pin number range for given pin type */
			pintype->num_pins = num_pins;
			pintype->pin_start = curr_pins;
			pintype->pin_end = curr_pins + num_pins;
			ptype_base = &pintype->reg_base;
			pintype->set_reg_base(ptype_base, dd->base);
			total_pins += num_pins;
			curr_pins += num_pins;
		}
	}
	dd->msm_pindesc = devm_kzalloc(dd->dev,
						sizeof(struct msm_pindesc) *
						total_pins, GFP_KERNEL);
	if (!dd->msm_pindesc) {
		dev_err(dd->dev, "Unable to allocate msm pindesc");
		goto alloc_fail;
	}

	dd->num_pins = total_pins;
	msm_pindesc = dd->msm_pindesc;
	/*
	 * Populate pin descriptor based on each pin type present in Device
	 * tree and supported by the driver
	 */
	for (i = 0; i < pinfo_entries; i++) {
		pintype = &pinfo[i];
		/* If entry not in device tree, skip */
		if (!pintype->node)
			continue;
		msm_populate_pindesc(pintype, msm_pindesc);
	}
	return 0;
alloc_fail:
	for (i = 0; i < pinfo_entries; i++) {
		pintype = &pinfo[i];
		if (pintype->node)
			of_node_put(pintype->node);
	}
	return -ENOMEM;
}

static const struct of_device_id msm_pinctrl_dt_match[] = {
	{ .compatible = "qcom,msm-tlmm-v3",
		.data = &tlmm_v3_pintypes, },
	{},
};
MODULE_DEVICE_TABLE(of, msm_pinctrl_dt_match);

static void msm_pinctrl_cleanup_dd(struct msm_pinctrl_dd *dd)
{
	int i;
	struct msm_pintype_info *pintype;

	pintype = dd->msm_pintype;
	for (i = 0; i < dd->num_pintypes; i++) {
		if (pintype->node)
			of_node_put(dd->msm_pintype[i].node);
	}
}

static int msm_pinctrl_get_drvdata(struct msm_pinctrl_dd *dd,
						struct platform_device *pdev)
{
	const struct of_device_id *match;
	const struct msm_tlmm_pintype *tlmm_info;
	int ret;
	struct device_node *node = pdev->dev.of_node;

	match = of_match_node(msm_pinctrl_dt_match, node);
	if (IS_ERR(match))
		return PTR_ERR(match);
	tlmm_info = match->data;
	dd->msm_pintype = tlmm_info->pintype_info;
	dd->num_pintypes = tlmm_info->num_entries;
	ret = msm_pinctrl_dt_parse_pintype(node, dd);
	if (ret)
		goto out;

	ret = msm_pinctrl_dt_parse_pins(node, dd);
	if (ret)
		msm_pinctrl_cleanup_dd(dd);
out:
	return ret;
}

static int msm_register_pinctrl(struct msm_pinctrl_dd *dd)
{
	int i;
	struct pinctrl_pin_desc *pindesc;
	struct msm_pintype_info *pinfo, *pintype;
	struct pinctrl_desc *ctrl_desc = &dd->pctl;

	ctrl_desc->name = "msm-pinctrl";
	ctrl_desc->owner = THIS_MODULE;
	ctrl_desc->pmxops = &msm_pmxops;
	ctrl_desc->confops = &msm_pconfops;
	ctrl_desc->pctlops = &msm_pctrlops;

	pindesc = devm_kzalloc(dd->dev, sizeof(*pindesc) * dd->num_pins,
							 GFP_KERNEL);
	if (!pindesc) {
		dev_err(dd->dev, "Failed to allocate pinctrl pin desc\n");
		return -ENOMEM;
	}

	for (i = 0; i < dd->num_pins; i++) {
		pindesc[i].number = i;
		pindesc[i].name = dd->msm_pindesc[i].name;
	}
	ctrl_desc->pins = pindesc;
	ctrl_desc->npins = dd->num_pins;
	dd->pctl_dev = pinctrl_register(ctrl_desc, dd->dev, dd);
	if (!dd->pctl_dev) {
		dev_err(dd->dev, "could not register pinctrl driver\n");
		return -EINVAL;
	}

	pinfo = dd->msm_pintype;
	for (i = 0; i < dd->num_pintypes; i++) {
		pintype = &pinfo[i];
		if (!pintype->supports_gpio)
			continue;
		pintype->grange.name = pintype->name;
		pintype->grange.id = i;
		pintype->grange.pin_base = pintype->pin_start;
		pintype->grange.base = pintype->gc.base;
		pintype->grange.npins = pintype->gc.ngpio;
		pintype->grange.gc = &pintype->gc;
		pinctrl_add_gpio_range(dd->pctl_dev, &pintype->grange);
	}
	return 0;
}

static void msm_register_gpiochip(struct msm_pinctrl_dd *dd)
{

	struct gpio_chip *gc;
	struct msm_pintype_info *pintype, *pinfo;
	int i, ret = 0;

	pinfo = dd->msm_pintype;
	for (i = 0; i < dd->num_pintypes; i++) {
		pintype = &pinfo[i];
		if (!msm_pintype_supports_gpio(pintype))
			continue;
		gc = &pintype->gc;
		gc->request = msm_pinctrl_request_gpio;
		gc->free = msm_pinctrl_free_gpio;
		gc->dev = dd->dev;
		gc->ngpio = pintype->num_pins;
		gc->base = -1;
		ret = gpiochip_add(gc);
		if (ret) {
			dev_err(dd->dev, "failed to register gpio chip\n");
			pinfo->supports_gpio = false;
		}
	}
}

static int msm_register_irqchip(struct msm_pinctrl_dd *dd)
{
	struct msm_pintype_info *pintype, *pinfo;
	int i, ret = 0;

	pinfo = dd->msm_pintype;
	for (i = 0; i < dd->num_pintypes; i++) {
		pintype = &pinfo[i];
		if (!msm_pintype_supports_irq(pintype))
			continue;
		ret = pintype->init_irq(dd->irq, pintype, dd->dev);
		return ret;
	}
	return 0;
}

static int msm_pinctrl_probe(struct platform_device *pdev)
{
	struct msm_pinctrl_dd *dd;
	struct device *dev = &pdev->dev;
	struct resource *res;
	int ret;

	dd = devm_kzalloc(dev, sizeof(*dd), GFP_KERNEL);
	if (!dd) {
		dev_err(dev, "Alloction failed for driver data\n");
		return -ENOMEM;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "cannot find IO resource\n");
		return -ENOENT;
	}
	dd->base = devm_ioremap(&pdev->dev, res->start,
							resource_size(res));
	if (IS_ERR(dd->base))
		return PTR_ERR(dd->base);
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (res)
		dd->irq = res->start;
	dd->dev = dev;
	ret = msm_pinctrl_get_drvdata(dd, pdev);
	if (ret) {
		dev_err(&pdev->dev, "driver data not available\n");
		return ret;
	}
	msm_register_gpiochip(dd);
	ret = msm_register_pinctrl(dd);
	if (ret) {
		msm_pinctrl_cleanup_dd(dd);
		return ret;
	}
	msm_register_irqchip(dd);
	platform_set_drvdata(pdev, dd);
	return 0;
}

static struct platform_driver msm_pinctrl_driver = {
	.probe		= msm_pinctrl_probe,
	.driver = {
		.name	= "msm-pinctrl",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(msm_pinctrl_dt_match),
	},
};

static int __init msm_pinctrl_drv_register(void)
{
	return platform_driver_register(&msm_pinctrl_driver);
}
postcore_initcall(msm_pinctrl_drv_register);

static void __exit msm_pinctrl_drv_unregister(void)
{
	platform_driver_unregister(&msm_pinctrl_driver);
}
module_exit(msm_pinctrl_drv_unregister);

MODULE_LICENSE("GPLv2");

