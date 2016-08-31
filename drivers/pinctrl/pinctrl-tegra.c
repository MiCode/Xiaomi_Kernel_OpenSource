/*
 * Driver for the NVIDIA Tegra pinmux
 *
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * Derived from code:
 * Copyright (C) 2010 Google, Inc.
 * Copyright (C) 2010 NVIDIA Corporation
 * Copyright (C) 2009-2011 ST-Ericsson AB
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/err.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/machine.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/slab.h>
#include <linux/syscore_ops.h>
#include <linux/uaccess.h>
#include <linux/pinctrl/pinctrl-tegra.h>

#include <mach/pinconf-tegra.h>
#include <mach/pinmux-defines.h>

#include "core.h"
#include "pinctrl-tegra.h"

static DEFINE_SPINLOCK(mux_lock);

struct tegra_pmx {
	struct device *dev;
	struct pinctrl_dev *pctl;

	const struct tegra_pinctrl_soc_data *soc;

	int nbanks;
	void __iomem **regs;
	int *regs_size;

	u32 *pg_data;
	unsigned drive_group_start_index;
};

static struct tegra_pmx *pmx;

static inline u32 pmx_readl(struct tegra_pmx *pmx, u32 bank, u32 reg)
{
	return readl(pmx->regs[bank] + reg);
}

static inline void pmx_writel(struct tegra_pmx *pmx, u32 val, u32 bank, u32 reg)
{
	writel(val, pmx->regs[bank] + reg);
}

static int tegra_pinctrl_get_groups_count(struct pinctrl_dev *pctldev)
{
	struct tegra_pmx *pmx = pinctrl_dev_get_drvdata(pctldev);

	return pmx->soc->ngroups;
}

static const char *tegra_pinctrl_get_group_name(struct pinctrl_dev *pctldev,
						unsigned group)
{
	struct tegra_pmx *pmx = pinctrl_dev_get_drvdata(pctldev);

	return pmx->soc->groups[group].name;
}

static int tegra_pinctrl_get_group_pins(struct pinctrl_dev *pctldev,
					unsigned group,
					const unsigned **pins,
					unsigned *num_pins)
{
	struct tegra_pmx *pmx = pinctrl_dev_get_drvdata(pctldev);

	*pins = pmx->soc->groups[group].pins;
	*num_pins = pmx->soc->groups[group].npins;

	return 0;
}

#ifdef CONFIG_DEBUG_FS
static void tegra_pinctrl_pin_dbg_show(struct pinctrl_dev *pctldev,
				       struct seq_file *s,
				       unsigned offset)
{
	seq_printf(s, " %s", dev_name(pctldev->dev));
}
#endif

static int reserve_map(struct device *dev, struct pinctrl_map **map,
		       unsigned *reserved_maps, unsigned *num_maps,
		       unsigned reserve)
{
	unsigned old_num = *reserved_maps;
	unsigned new_num = *num_maps + reserve;
	struct pinctrl_map *new_map;

	if (old_num >= new_num)
		return 0;

	new_map = krealloc(*map, sizeof(*new_map) * new_num, GFP_KERNEL);
	if (!new_map) {
		dev_err(dev, "krealloc(map) failed\n");
		return -ENOMEM;
	}

	memset(new_map + old_num, 0, (new_num - old_num) * sizeof(*new_map));

	*map = new_map;
	*reserved_maps = new_num;

	return 0;
}

static int add_map_mux(struct pinctrl_map **map, unsigned *reserved_maps,
		       unsigned *num_maps, const char *group,
		       const char *function)
{
	if (WARN_ON(*num_maps == *reserved_maps))
		return -ENOSPC;

	(*map)[*num_maps].type = PIN_MAP_TYPE_MUX_GROUP;
	(*map)[*num_maps].data.mux.group = group;
	(*map)[*num_maps].data.mux.function = function;
	(*num_maps)++;

	return 0;
}

static int add_map_configs(struct device *dev, struct pinctrl_map **map,
			   unsigned *reserved_maps, unsigned *num_maps,
			   const char *group, unsigned long *configs,
			   unsigned num_configs)
{
	unsigned long *dup_configs;

	if (WARN_ON(*num_maps == *reserved_maps))
		return -ENOSPC;

	dup_configs = kmemdup(configs, num_configs * sizeof(*dup_configs),
			      GFP_KERNEL);
	if (!dup_configs) {
		dev_err(dev, "kmemdup(configs) failed\n");
		return -ENOMEM;
	}

	(*map)[*num_maps].type = PIN_MAP_TYPE_CONFIGS_GROUP;
	(*map)[*num_maps].data.configs.group_or_pin = group;
	(*map)[*num_maps].data.configs.configs = dup_configs;
	(*map)[*num_maps].data.configs.num_configs = num_configs;
	(*num_maps)++;

	return 0;
}

static int add_config(struct device *dev, unsigned long **configs,
		      unsigned *num_configs, unsigned long config)
{
	unsigned old_num = *num_configs;
	unsigned new_num = old_num + 1;
	unsigned long *new_configs;

	new_configs = krealloc(*configs, sizeof(*new_configs) * new_num,
			       GFP_KERNEL);
	if (!new_configs) {
		dev_err(dev, "krealloc(configs) failed\n");
		return -ENOMEM;
	}

	new_configs[old_num] = config;

	*configs = new_configs;
	*num_configs = new_num;

	return 0;
}

static void tegra_pinctrl_dt_free_map(struct pinctrl_dev *pctldev,
				      struct pinctrl_map *map,
				      unsigned num_maps)
{
	int i;

	for (i = 0; i < num_maps; i++)
		if (map[i].type == PIN_MAP_TYPE_CONFIGS_GROUP)
			kfree(map[i].data.configs.configs);

	kfree(map);
}

static const struct cfg_param {
	const char *property;
	enum tegra_pinconf_param param;
} cfg_params[] = {
	{"nvidia,pull",			TEGRA_PINCONF_PARAM_PULL},
	{"nvidia,tristate",		TEGRA_PINCONF_PARAM_TRISTATE},
	{"nvidia,enable-input",		TEGRA_PINCONF_PARAM_ENABLE_INPUT},
	{"nvidia,open-drain",		TEGRA_PINCONF_PARAM_OPEN_DRAIN},
	{"nvidia,lock",			TEGRA_PINCONF_PARAM_LOCK},
	{"nvidia,io-reset",		TEGRA_PINCONF_PARAM_IORESET},
	{"nvidia,rcv-sel",		TEGRA_PINCONF_PARAM_RCV_SEL},
	{"nvidia,high-speed-mode",	TEGRA_PINCONF_PARAM_HIGH_SPEED_MODE},
	{"nvidia,schmitt",		TEGRA_PINCONF_PARAM_SCHMITT},
	{"nvidia,low-power-mode",	TEGRA_PINCONF_PARAM_LOW_POWER_MODE},
	{"nvidia,pull-down-strength",	TEGRA_PINCONF_PARAM_DRIVE_DOWN_STRENGTH},
	{"nvidia,pull-up-strength",	TEGRA_PINCONF_PARAM_DRIVE_UP_STRENGTH},
	{"nvidia,slew-rate-falling",	TEGRA_PINCONF_PARAM_SLEW_RATE_FALLING},
	{"nvidia,slew-rate-rising",	TEGRA_PINCONF_PARAM_SLEW_RATE_RISING},
	{"nvidia,drive-type",		TEGRA_PINCONF_PARAM_DRIVE_TYPE},
};

static int tegra_pinctrl_dt_subnode_to_map(struct device *dev,
					   struct device_node *np,
					   struct pinctrl_map **map,
					   unsigned *reserved_maps,
					   unsigned *num_maps)
{
	int ret, i;
	const char *function;
	u32 val;
	unsigned long config;
	unsigned long *configs = NULL;
	unsigned num_configs = 0;
	unsigned reserve;
	struct property *prop;
	const char *group;

	ret = of_property_read_string(np, "nvidia,function", &function);
	if (ret < 0) {
		/* EINVAL=missing, which is fine since it's optional */
		if (ret != -EINVAL)
			dev_err(dev,
				"could not parse property nvidia,function\n");
		function = NULL;
	}

	for (i = 0; i < ARRAY_SIZE(cfg_params); i++) {
		ret = of_property_read_u32(np, cfg_params[i].property, &val);
		if (!ret) {
			config = TEGRA_PINCONF_PACK(cfg_params[i].param, val);
			ret = add_config(dev, &configs, &num_configs, config);
			if (ret < 0)
				goto exit;
		/* EINVAL=missing, which is fine since it's optional */
		} else if (ret != -EINVAL) {
			dev_err(dev, "could not parse property %s\n",
				cfg_params[i].property);
		}
	}

	reserve = 0;
	if (function != NULL)
		reserve++;
	if (num_configs)
		reserve++;
	ret = of_property_count_strings(np, "nvidia,pins");
	if (ret < 0) {
		dev_err(dev, "could not parse property nvidia,pins\n");
		goto exit;
	}
	reserve *= ret;

	ret = reserve_map(dev, map, reserved_maps, num_maps, reserve);
	if (ret < 0)
		goto exit;

	of_property_for_each_string(np, "nvidia,pins", prop, group) {
		if (function) {
			ret = add_map_mux(map, reserved_maps, num_maps,
					  group, function);
			if (ret < 0)
				goto exit;
		}

		if (num_configs) {
			ret = add_map_configs(dev, map, reserved_maps,
					      num_maps, group, configs,
					      num_configs);
			if (ret < 0)
				goto exit;
		}
	}

	ret = 0;

exit:
	kfree(configs);
	return ret;
}

static int tegra_pinctrl_dt_node_to_map(struct pinctrl_dev *pctldev,
					struct device_node *np_config,
					struct pinctrl_map **map,
					unsigned *num_maps)
{
	unsigned reserved_maps;
	struct device_node *np;
	int ret;

	reserved_maps = 0;
	*map = NULL;
	*num_maps = 0;

	for_each_child_of_node(np_config, np) {
		ret = tegra_pinctrl_dt_subnode_to_map(pctldev->dev, np, map,
						      &reserved_maps, num_maps);
		if (ret < 0) {
			tegra_pinctrl_dt_free_map(pctldev, *map, *num_maps);
			return ret;
		}
	}

	return 0;
}

static const struct pinctrl_ops tegra_pinctrl_ops = {
	.get_groups_count = tegra_pinctrl_get_groups_count,
	.get_group_name = tegra_pinctrl_get_group_name,
	.get_group_pins = tegra_pinctrl_get_group_pins,
#ifdef CONFIG_DEBUG_FS
	.pin_dbg_show = tegra_pinctrl_pin_dbg_show,
#endif
	.dt_node_to_map = tegra_pinctrl_dt_node_to_map,
	.dt_free_map = tegra_pinctrl_dt_free_map,
};

static int tegra_pinctrl_get_funcs_count(struct pinctrl_dev *pctldev)
{
	struct tegra_pmx *pmx = pinctrl_dev_get_drvdata(pctldev);

	return pmx->soc->nfunctions;
}

static const char *tegra_pinctrl_get_func_name(struct pinctrl_dev *pctldev,
					       unsigned function)
{
	struct tegra_pmx *pmx = pinctrl_dev_get_drvdata(pctldev);

	return pmx->soc->functions[function].name;
}

static int tegra_pinctrl_get_func_groups(struct pinctrl_dev *pctldev,
					 unsigned function,
					 const char * const **groups,
					 unsigned * const num_groups)
{
	struct tegra_pmx *pmx = pinctrl_dev_get_drvdata(pctldev);

	*groups = pmx->soc->functions[function].groups;
	*num_groups = pmx->soc->functions[function].ngroups;

	return 0;
}

static int tegra_pinctrl_enable(struct pinctrl_dev *pctldev, unsigned req_function,
			       unsigned group)
{
	struct tegra_pmx *pmx = pinctrl_dev_get_drvdata(pctldev);
	const struct tegra_pingroup *g;
	unsigned function = req_function;
	int i;
	u32 val;
	unsigned long flags;

	g = &pmx->soc->groups[group];

	if (WARN_ON(g->mux_reg < 0))
		return -EINVAL;

	/* Last function option is safe option */
	if (!req_function)
		function = g->func_safe;

	for (i = 0; i < ARRAY_SIZE(g->funcs); i++) {
		if (g->funcs[i] == function)
			break;
	}
	if (WARN_ON(i == ARRAY_SIZE(g->funcs)))
		return -EINVAL;

	spin_lock_irqsave(&mux_lock, flags);

	val = pmx_readl(pmx, g->mux_bank, g->mux_reg);
	val &= ~(0x3 << g->mux_bit);
	val |= i << g->mux_bit;
	pmx_writel(pmx, val, g->mux_bank, g->mux_reg);

	spin_unlock_irqrestore(&mux_lock, flags);

	return 0;
}

static void tegra_pinctrl_disable(struct pinctrl_dev *pctldev,
				  unsigned function, unsigned group)
{
	struct tegra_pmx *pmx = pinctrl_dev_get_drvdata(pctldev);
	const struct tegra_pingroup *g;
	u32 val;
	unsigned long flags;

	g = &pmx->soc->groups[group];

	if (WARN_ON(g->mux_reg < 0))
		return;

	spin_lock_irqsave(&mux_lock, flags);

	val = pmx_readl(pmx, g->mux_bank, g->mux_reg);
	val &= ~(0x3 << g->mux_bit);
	val |= g->func_safe << g->mux_bit;
	pmx_writel(pmx, val, g->mux_bank, g->mux_reg);

	spin_unlock_irqrestore(&mux_lock, flags);
}

static const struct pinmux_ops tegra_pinmux_ops = {
	.get_functions_count = tegra_pinctrl_get_funcs_count,
	.get_function_name = tegra_pinctrl_get_func_name,
	.get_function_groups = tegra_pinctrl_get_func_groups,
	.enable = tegra_pinctrl_enable,
	.disable = tegra_pinctrl_disable,
};

static int tegra_pinconf_reg(struct tegra_pmx *pmx,
			     const struct tegra_pingroup *g,
			     enum tegra_pinconf_param param,
			     bool report_err,
			     s8 *bank, s16 *reg, s8 *bit, s8 *width)
{
	switch (param) {
	case TEGRA_PINCONF_PARAM_PULL:
		*bank = g->pupd_bank;
		*reg = g->pupd_reg;
		*bit = g->pupd_bit;
		*width = 2;
		break;
	case TEGRA_PINCONF_PARAM_TRISTATE:
		*bank = g->tri_bank;
		*reg = g->tri_reg;
		*bit = g->tri_bit;
		*width = 1;
		break;
	case TEGRA_PINCONF_PARAM_ENABLE_INPUT:
		*bank = g->einput_bank;
		*reg = g->einput_reg;
		*bit = g->einput_bit;
		*width = 1;
		break;
	case TEGRA_PINCONF_PARAM_OPEN_DRAIN:
		*bank = g->odrain_bank;
		*reg = g->odrain_reg;
		*bit = g->odrain_bit;
		*width = 1;
		break;
	case TEGRA_PINCONF_PARAM_LOCK:
		*bank = g->lock_bank;
		*reg = g->lock_reg;
		*bit = g->lock_bit;
		*width = 1;
		break;
	case TEGRA_PINCONF_PARAM_IORESET:
		*bank = g->ioreset_bank;
		*reg = g->ioreset_reg;
		*bit = g->ioreset_bit;
		*width = 1;
		break;
	case TEGRA_PINCONF_PARAM_RCV_SEL:
		*bank = g->rcv_sel_bank;
		*reg = g->rcv_sel_reg;
		*bit = g->rcv_sel_bit;
		*width = 1;
		break;
	case TEGRA_PINCONF_PARAM_HIGH_SPEED_MODE:
		*bank = g->drv_bank;
		*reg = g->drv_reg;
		*bit = g->hsm_bit;
		*width = 1;
		break;
	case TEGRA_PINCONF_PARAM_SCHMITT:
		*bank = g->drv_bank;
		*reg = g->drv_reg;
		*bit = g->schmitt_bit;
		*width = 1;
		break;
	case TEGRA_PINCONF_PARAM_LOW_POWER_MODE:
		*bank = g->drv_bank;
		*reg = g->drv_reg;
		*bit = g->lpmd_bit;
		*width = 2;
		break;
	case TEGRA_PINCONF_PARAM_DRIVE_DOWN_STRENGTH:
		*bank = g->drv_bank;
		*reg = g->drv_reg;
		*bit = g->drvdn_bit;
		*width = g->drvdn_width;
		break;
	case TEGRA_PINCONF_PARAM_DRIVE_UP_STRENGTH:
		*bank = g->drv_bank;
		*reg = g->drv_reg;
		*bit = g->drvup_bit;
		*width = g->drvup_width;
		break;
	case TEGRA_PINCONF_PARAM_SLEW_RATE_FALLING:
		*bank = g->drv_bank;
		*reg = g->drv_reg;
		*bit = g->slwf_bit;
		*width = g->slwf_width;
		break;
	case TEGRA_PINCONF_PARAM_SLEW_RATE_RISING:
		*bank = g->drv_bank;
		*reg = g->drv_reg;
		*bit = g->slwr_bit;
		*width = g->slwr_width;
		break;
	case TEGRA_PINCONF_PARAM_DRIVE_TYPE:
		*bank = g->drvtype_bank;
		*reg = g->drvtype_reg;
		*bit = g->drvtype_bit;
		*width = g->drvtype_width;
		break;
	default:
		dev_err(pmx->dev, "Invalid config param %04x\n", param);
		return -ENOTSUPP;
	}

	if (*reg < 0) {
		if (report_err)
			dev_err(pmx->dev,
				"Config param %04x not supported on group %s\n",
				param, g->name);
		return -ENOTSUPP;
	}

	return 0;
}

static int tegra_pinconf_get(struct pinctrl_dev *pctldev,
			     unsigned pin, unsigned long *config)
{
	dev_err(pctldev->dev, "pin_config_get op not supported\n");
	return -ENOTSUPP;
}

static int tegra_pinconf_set(struct pinctrl_dev *pctldev,
			     unsigned pin, unsigned long config)
{
	dev_err(pctldev->dev, "pin_config_set op not supported\n");
	return -ENOTSUPP;
}

static int tegra_pinconf_group_get(struct pinctrl_dev *pctldev,
				   unsigned group, unsigned long *config)
{
	struct tegra_pmx *pmx = pinctrl_dev_get_drvdata(pctldev);
	enum tegra_pinconf_param param = TEGRA_PINCONF_UNPACK_PARAM(*config);
	u16 arg;
	const struct tegra_pingroup *g;
	int ret;
	s8 bank, bit, width;
	s16 reg;
	u32 val, mask;
	unsigned long flags;

	g = &pmx->soc->groups[group];

	ret = tegra_pinconf_reg(pmx, g, param, true, &bank, &reg, &bit,
				&width);
	if (ret < 0)
		return ret;

	spin_lock_irqsave(&mux_lock, flags);

	val = pmx_readl(pmx, bank, reg);

	spin_unlock_irqrestore(&mux_lock, flags);

	mask = (1 << width) - 1;
	arg = (val >> bit) & mask;

	*config = TEGRA_PINCONF_PACK(param, arg);

	return 0;
}

static int tegra_pinconf_group_set(struct pinctrl_dev *pctldev,
				   unsigned group, unsigned long config)
{
	struct tegra_pmx *pmx = pinctrl_dev_get_drvdata(pctldev);
	enum tegra_pinconf_param param = TEGRA_PINCONF_UNPACK_PARAM(config);
	u16 arg = TEGRA_PINCONF_UNPACK_ARG(config);
	const struct tegra_pingroup *g;
	int ret;
	s8 bank, bit, width;
	s16 reg;
	u32 val, mask;
	unsigned long flags;

	g = &pmx->soc->groups[group];

	ret = tegra_pinconf_reg(pmx, g, param, true, &bank, &reg, &bit,
				&width);
	if (ret < 0)
		return ret;

	spin_lock_irqsave(&mux_lock, flags);
	val = pmx_readl(pmx, bank, reg);

	/* LOCK can't be cleared */
	if (param == TEGRA_PINCONF_PARAM_LOCK) {
		if ((val & BIT(bit)) && !arg) {
			dev_err(pctldev->dev, "LOCK bit cannot be cleared\n");
			ret = -EINVAL;
			goto end;
		}
	}

	/* Special-case Boolean values; allow any non-zero as true */
	if (width == 1)
		arg = !!arg;

	/* Range-check user-supplied value */
	mask = (1 << width) - 1;
	if (arg & ~mask) {
		dev_err(pctldev->dev,
			"group %s config %lx: %x too big for %d bit register\n",
			g->name, config, arg, width);
			ret = -EINVAL;
			goto end;
	}

	/* Update register */
	val &= ~(mask << bit);
	val |= arg << bit;
	pmx_writel(pmx, val, bank, reg);

end:
	spin_unlock_irqrestore(&mux_lock, flags);
	return ret;
}

#ifdef CONFIG_DEBUG_FS
static void tegra_pinconf_dbg_show(struct pinctrl_dev *pctldev,
				   struct seq_file *s, unsigned offset)
{
}

static const char *strip_prefix(const char *s)
{
	const char *comma = strchr(s, ',');
	if (!comma)
		return s;

	return comma + 1;
}

static void tegra_pinconf_group_dbg_show(struct pinctrl_dev *pctldev,
					 struct seq_file *s, unsigned group)
{
	struct tegra_pmx *pmx = pinctrl_dev_get_drvdata(pctldev);
	const struct tegra_pingroup *g;
	int i, ret;
	s8 bank, bit, width;
	s16 reg;
	u32 val;

	g = &pmx->soc->groups[group];

	for (i = 0; i < ARRAY_SIZE(cfg_params); i++) {
		ret = tegra_pinconf_reg(pmx, g, cfg_params[i].param, false,
					&bank, &reg, &bit, &width);
		if (ret < 0)
			continue;

		val = pmx_readl(pmx, bank, reg);
		val >>= bit;
		val &= (1 << width) - 1;

		seq_printf(s, "\n\t%s=%u",
			   strip_prefix(cfg_params[i].property), val);
	}
}

static void tegra_pinconf_config_dbg_show(struct pinctrl_dev *pctldev,
					  struct seq_file *s,
					  unsigned long config)
{
	enum tegra_pinconf_param param = TEGRA_PINCONF_UNPACK_PARAM(config);
	u16 arg = TEGRA_PINCONF_UNPACK_ARG(config);
	const char *pname = "unknown";
	int i;

	for (i = 0; i < ARRAY_SIZE(cfg_params); i++) {
		if (cfg_params[i].param == param) {
			pname = cfg_params[i].property;
			break;
		}
	}

	seq_printf(s, "%s=%d", strip_prefix(pname), arg);
}
#endif

static const struct pinconf_ops tegra_pinconf_ops = {
	.pin_config_get = tegra_pinconf_get,
	.pin_config_set = tegra_pinconf_set,
	.pin_config_group_get = tegra_pinconf_group_get,
	.pin_config_group_set = tegra_pinconf_group_set,
#ifdef CONFIG_DEBUG_FS
	.pin_config_dbg_show = tegra_pinconf_dbg_show,
	.pin_config_group_dbg_show = tegra_pinconf_group_dbg_show,
	.pin_config_config_dbg_show = tegra_pinconf_config_dbg_show,
#endif
};

static struct pinctrl_gpio_range tegra_pinctrl_gpio_range = {
	.name = "Tegra GPIOs",
	.id = 0,
	.base = 0,
};

static struct pinctrl_desc tegra_pinctrl_desc = {
	.pctlops = &tegra_pinctrl_ops,
	.pmxops = &tegra_pinmux_ops,
	.confops = &tegra_pinconf_ops,
	.owner = THIS_MODULE,
};

#ifdef CONFIG_PM_SLEEP

static int pinctrl_suspend(void)
{
	int i, j;
	u32 *pg_data = pmx->pg_data;
	u32 *regs;

	if (pmx->soc->suspend) {
		int ret;

		ret = pmx->soc->suspend(pg_data);
		if (!ret)
			pinctrl_configure_user_state(pmx->pctl, "suspend");
		return ret;
	}

	for (i = 0; i < pmx->nbanks; i++) {
		regs = pmx->regs[i];
		for (j = 0; j < pmx->regs_size[i] / 4; j++)
			*pg_data++ = readl(regs++);
	}
	return 0;
}

static void pinctrl_resume(void)
{
	int i, j;
	u32 *pg_data = pmx->pg_data;
	u32 *regs;

	if (pmx->soc->resume) {
		pmx->soc->resume(pg_data);
		return;
	}

	for (i = 0; i < pmx->nbanks; i++) {
		regs = pmx->regs[i];
		for (j = 0; j < pmx->regs_size[i] / 4; j++)
			writel(*pg_data++, regs++);
	}
}

static struct syscore_ops pinctrl_syscore_ops = {
	.suspend = pinctrl_suspend,
	.resume = pinctrl_resume,
};

#endif

static int tegra_pinctrl_get_group(struct tegra_pmx *pmx, const char *name)
{
	int i;

	for (i = 0; i< pmx->soc->ngroups; ++i) {
		if (!strcmp(pmx->soc->groups[i].name, name))
			return i;
	}
	return -EINVAL;
}

static int tegra_pinctrl_set_config(struct pinctrl_dev *pctldev,
	int pg, int param, int val)
{
	unsigned long config;

	config = TEGRA_PINCONF_PACK(param, val);
	return tegra_pinconf_group_set(pmx->pctl, pg, config);
}

static void tegra_pinctrl_default_soc_init(struct tegra_pmx *pmx)
{
	struct tegra_pinctrl_group_config_data *cdata;
	int group;
	int i;

	for (i = 0; i < pmx->soc->nconfig_data; ++i) {
		cdata = &pmx->soc->config_data[i];
		group = tegra_pinctrl_get_group(pmx, cdata->name);
		if (group < 0) {
			dev_warn(pmx->dev, "Group name %s not found\n",
				cdata->name);
			continue;
		}

		tegra_pinctrl_set_config(pmx->pctl, group,
				TEGRA_PINCONF_PARAM_HIGH_SPEED_MODE,
				cdata->high_speed_mode);

		tegra_pinctrl_set_config(pmx->pctl, group,
				TEGRA_PINCONF_PARAM_SCHMITT,
				cdata->schmitt);

		tegra_pinctrl_set_config(pmx->pctl, group,
				TEGRA_PINCONF_PARAM_LOW_POWER_MODE,
				cdata->low_power_mode);

		tegra_pinctrl_set_config(pmx->pctl, group,
				TEGRA_PINCONF_PARAM_DRIVE_DOWN_STRENGTH,
				cdata->pull_down_strength);

		tegra_pinctrl_set_config(pmx->pctl, group,
				TEGRA_PINCONF_PARAM_DRIVE_UP_STRENGTH,
				cdata->pull_up_strength);

		tegra_pinctrl_set_config(pmx->pctl, group,
				TEGRA_PINCONF_PARAM_SLEW_RATE_FALLING,
				cdata->slew_rate_falling);

		tegra_pinctrl_set_config(pmx->pctl, group,
				TEGRA_PINCONF_PARAM_SLEW_RATE_RISING,
				cdata->slew_rate_rising);

		if (pmx->soc->groups[i].drvtype_reg >= 0)
			tegra_pinctrl_set_config(pmx->pctl, group,
					TEGRA_PINCONF_PARAM_DRIVE_TYPE,
					cdata->drive_type);
	}
}

int tegra_pinctrl_probe(struct platform_device *pdev,
			const struct tegra_pinctrl_soc_data *soc_data)
{
	struct resource *res;
	int i, pg_data_size = 0;

	pmx = devm_kzalloc(&pdev->dev, sizeof(*pmx), GFP_KERNEL);
	if (!pmx) {
		dev_err(&pdev->dev, "Can't alloc tegra_pmx\n");
		return -ENOMEM;
	}
	pmx->dev = &pdev->dev;
	pmx->soc = soc_data;

	pmx->drive_group_start_index = -1;

	for (i = 0; i < pmx->soc->ngroups; ++i) {
		if (pmx->soc->groups[i].drv_reg < 0)
			continue;
		pmx->drive_group_start_index = i;
		break;
	}

	tegra_pinctrl_gpio_range.npins = pmx->soc->ngpios;
	tegra_pinctrl_desc.name = dev_name(&pdev->dev);
	tegra_pinctrl_desc.pins = pmx->soc->pins;
	tegra_pinctrl_desc.npins = pmx->soc->npins;

	for (i = 0; ; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res)
			break;
		pg_data_size += resource_size(res);
	}
	pmx->nbanks = i;

	pmx->regs = devm_kzalloc(&pdev->dev, pmx->nbanks * sizeof(*pmx->regs),
				 GFP_KERNEL);
	if (!pmx->regs) {
		dev_err(&pdev->dev, "Can't alloc regs pointer\n");
		return -ENODEV;
	}

#ifdef CONFIG_PM_SLEEP
	pmx->regs_size = devm_kzalloc(&pdev->dev,
				pmx->nbanks * sizeof(*(pmx->regs_size)),
				GFP_KERNEL);
	if (!pmx->regs_size) {
		dev_err(&pdev->dev, "Can't alloc regs pointer\n");
		return -ENODEV;
	}

	pmx->pg_data = devm_kzalloc(&pdev->dev, pg_data_size, GFP_KERNEL);
	if (!pmx->pg_data) {
		dev_err(&pdev->dev, "Can't alloc pingroup data pointer\n");
		return -ENODEV;
	}
#endif

	for (i = 0; i < pmx->nbanks; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res) {
			dev_err(&pdev->dev, "Missing MEM resource\n");
			return -ENODEV;
		}

		if (!devm_request_mem_region(&pdev->dev, res->start,
					    resource_size(res),
					    dev_name(&pdev->dev))) {
			dev_err(&pdev->dev,
				"Couldn't request MEM resource %d\n", i);
			return -ENODEV;
		}

		pmx->regs[i] = devm_ioremap(&pdev->dev, res->start,
					    resource_size(res));
		if (!pmx->regs[i]) {
			dev_err(&pdev->dev, "Couldn't ioremap regs %d\n", i);
			return -ENODEV;
		}

#ifdef CONFIG_PM_SLEEP
		pmx->regs_size[i] = resource_size(res);
#endif
	}

	pmx->pctl = pinctrl_register(&tegra_pinctrl_desc, &pdev->dev, pmx);
	if (!pmx->pctl) {
		dev_err(&pdev->dev, "Couldn't register pinctrl driver\n");
		return -ENODEV;
	}

	pinctrl_add_gpio_range(pmx->pctl, &tegra_pinctrl_gpio_range);

	platform_set_drvdata(pdev, pmx);

	tegra_pinctrl_default_soc_init(pmx);

	pinctrl_configure_user_state(pmx->pctl, "drive");
	pinctrl_configure_user_state(pmx->pctl, "unused");

#ifdef CONFIG_PM_SLEEP
	register_syscore_ops(&pinctrl_syscore_ops);
#endif
	dev_dbg(&pdev->dev, "Probed Tegra pinctrl driver\n");

	return 0;
}
EXPORT_SYMBOL_GPL(tegra_pinctrl_probe);

int tegra_pinctrl_remove(struct platform_device *pdev)
{
	struct tegra_pmx *pmx = platform_get_drvdata(pdev);

	pinctrl_unregister(pmx->pctl);

	return 0;
}
EXPORT_SYMBOL_GPL(tegra_pinctrl_remove);

/*** To support non-dt pin control */

static const char *tegra_pinctrl_drive_names[TEGRA_MAX_DRIVE] = {
	[TEGRA_DRIVE_DIV_8] = "DIV_8",
	[TEGRA_DRIVE_DIV_4] = "DIV_4",
	[TEGRA_DRIVE_DIV_2] = "DIV_2",
	[TEGRA_DRIVE_DIV_1] = "DIV_1",
};

static const char *tegra_pinctrl_slew_names[TEGRA_MAX_SLEW] = {
	[TEGRA_SLEW_FASTEST] = "FASTEST",
	[TEGRA_SLEW_FAST] = "FAST",
	[TEGRA_SLEW_SLOW] = "SLOW",
	[TEGRA_SLEW_SLOWEST] = "SLOWEST",
};

#define HSM_EN(reg)     (((reg) >> 2) & 0x1)
#define SCHMT_EN(reg)   (((reg) >> 3) & 0x1)
#define LPMD(reg)       (((reg) >> 4) & 0x3)
#define DRVDN(reg, offset)      (((reg) >> offset) & 0x1f)
#define DRVUP(reg, offset)      (((reg) >> offset) & 0x1f)
#define SLWR(reg, offset)       (((reg) >> offset) & 0x3)
#define SLWF(reg, offset)       (((reg) >> offset) & 0x3)

static const char *tegra_pinctrl_function_name(enum tegra_mux_func func)
{
	if (func == TEGRA_MUX_RSVD1)
		return "RSVD1";

	if (func == TEGRA_MUX_RSVD2)
		return "RSVD2";

	if (func == TEGRA_MUX_RSVD3)
		return "RSVD3";

	if (func == TEGRA_MUX_RSVD4)
		return "RSVD4";

	if (func == TEGRA_MUX_INVALID)
		return "INVALID";

	if (func < 0 || func >=  TEGRA_MAX_MUX)
		return "<UNKNOWN>";

	return tegra_pinctrl_get_func_name(pmx->pctl, func);
}


static const char *tegra_pinctrl_tri_name(unsigned long val)
{
	return val ? "TRISTATE" : "NORMAL";
}
static const char *tegra_pinctrl_pupd_name(unsigned long val)
{
	switch (val) {
	case 0:
		return "NORMAL";

	case 1:
		return "PULL_DOWN";

	case 2:
		return "PULL_UP";

	default:
		return "RSVD";
	}
}
#if !defined(CONFIG_ARCH_TEGRA_2x_SOC)
static const char *tegra_pinctrl_lock_name(unsigned long val)
{
	switch (val) {
	case TEGRA_PIN_LOCK_DEFAULT:
		return "LOCK_DEFUALT";

	case TEGRA_PIN_LOCK_DISABLE:
		return "LOCK_DISABLE";

	case TEGRA_PIN_LOCK_ENABLE:
		return "LOCK_ENABLE";
	default:
		return "LOCK_DEFAULT";
	}
}

static const char *tegra_pinctrl_od_name(unsigned long val)
{
	switch (val) {
	case TEGRA_PIN_OD_DEFAULT:
		return "OD_DEFAULT";

	case TEGRA_PIN_OD_DISABLE:
		return "OD_DISABLE";

	case TEGRA_PIN_OD_ENABLE:
		return "OD_ENABLE";
	default:
		return "OD_DEFAULT";
	}
}

static const char *tegra_pinctrl_ioreset_name(unsigned long val)
{
	switch (val) {
	case TEGRA_PIN_IO_RESET_DEFAULT:
		return "IO_RESET_DEFAULT";

	case TEGRA_PIN_IO_RESET_DISABLE:
		return "IO_RESET_DISABLE";

	case TEGRA_PIN_IO_RESET_ENABLE:
		return "IO_RESET_ENABLE";
	default:
		return "IO_RESET_DEFAULT";
	}
}
#endif
static const char *tegra_pinctrl_io_name(unsigned long val)
{
	switch (val) {
	case 0:
		return "OUTPUT";

	case 1:
		return "INPUT";

	default:
		return "RSVD";
	}
}

static const char *tegra_pinctrl_rcv_sel_name(unsigned long val)
{
	switch (val) {
	case 0:
		return "RCV-DISABLE";

	case 1:
		return "RCV-DISABLE";

	default:
		return "RSVD";
	}
}

static const char *drive_pinmux_name(int pg)
{
	if (pg < 0 || !pmx || pg >=  pmx->soc->ngroups)
		return "<UNKNOWN>";
	if (pmx->soc->groups[pg].drv_reg < 0)
		return "<UNKNOWN>";

	return pmx->soc->groups[pg].name;
}

static const char *enable_name(unsigned long val)
{
	return val ? "ENABLE" : "DISABLE";
}

static const char *drive_name(unsigned long val)
{
	if (val >= TEGRA_MAX_DRIVE)
		return "<UNKNOWN>";

	return tegra_pinctrl_drive_names[val];
}

static const char *slew_name(unsigned long val)
{
	if (val >= TEGRA_MAX_SLEW)
		return "<UNKNOWN>";

	return tegra_pinctrl_slew_names[val];
}

u32 tegra_pinctrl_readl(u32 bank, u32 reg)
{
	return readl(pmx->regs[bank] + reg);
}
EXPORT_SYMBOL_GPL(tegra_pinctrl_readl);

void tegra_pinctrl_writel(u32 val, u32 bank, u32 reg)
{
	writel(val, pmx->regs[bank] + reg);
}
EXPORT_SYMBOL_GPL(tegra_pinctrl_writel);

int tegra_pinctrl_gpio_to_pingroup(int gpio_nr)
{
	int i;

	if (!pmx || gpio_nr < 0)
		return -EINVAL;

	for (i = 0; i < pmx->soc->ngroups; ++i) {
		if (pmx->soc->groups[i].drv_reg >= 0)
			continue;

		if (pmx->soc->groups[i].pins[0] == gpio_nr) {
			if (pmx->soc->groups[i].pins[0] >= pmx->soc->ngpios)
				return -EINVAL;
			return i;
		}
	}
	return -EINVAL;
}
EXPORT_SYMBOL(tegra_pinctrl_gpio_to_pingroup);

int tegra_pinctrl_pg_set_func(const struct tegra_pingroup_config *config)
{
	int mux = -1;
	int i;
	int find = 0;
	int ret;
	int pg = config->pingroup;
	enum tegra_mux_func func = config->func;
	const struct tegra_pingroup *g;
	int func_dt = -1;

	if (!pmx) {
		pr_err("Pingroup not registered yet\n");
		return -EPROBE_DEFER;
	}

	if (pg < 0 || pg >=  pmx->soc->ngroups)
		return -ERANGE;

	g = &pmx->soc->groups[pg];

	if (g->mux_reg < 0)
		return -EINVAL;

	if (func == TEGRA_MUX_INVALID) {
		pr_err("The pingroup %s is not recommended for option %s\n",
				g->name, tegra_pinctrl_function_name(func));
		WARN_ON(1);
		return -EINVAL;
	}

	if (func < 0)
		return -ERANGE;

	if (func == TEGRA_MUX_SAFE) {
		func = g->func_safe_non_dt;
		func_dt =  g->func_safe;
	}
	if (func & TEGRA_MUX_RSVD) {
		for (i = 0; i < 4; i++) {
			if (g->funcs_non_dt[i] & TEGRA_MUX_RSVD)
				mux = i;

			if (g->funcs_non_dt[i] == func) {
				mux = i;
				find = 1;
				break;
			}
		}
	} else {
		for (i = 0; i < 4; i++) {
			if (g->funcs_non_dt[i] == func) {
				mux = i;
				find = 1;
				break;
			}
		}
	}

	if (!find)
		pr_warn("The pingroup %s was configured to %s instead of %s\n",
			g->name,
			tegra_pinctrl_function_name(g->funcs_non_dt[mux]),
			tegra_pinctrl_function_name(func));
	if (mux >= 0) {
		func = g->funcs_non_dt[mux];
		func_dt = g->funcs[mux];
	}

	if (func_dt < 0) {
		pr_warn("The pingroup %s does not have option %s\n",
			g->name,  tegra_pinctrl_function_name(func));
		return -EINVAL;
	}
	
	ret = tegra_pinctrl_enable(pmx->pctl, func_dt, pg);
	if (ret < 0) {
		pr_err("Not able to set function %s for pin group %s\n",
			tegra_pinctrl_function_name(func), g->name);
		return ret;
	}

	ret = tegra_pinctrl_pg_set_io(pg, config->io);
	if (ret < 0) {
		pr_err("Not able to set io %s for pin group %s\n",
			tegra_pinctrl_io_name(config->io), g->name);
		return ret;
	}
	return 0;
}
EXPORT_SYMBOL(tegra_pinctrl_pg_set_func);

int tegra_pinctrl_pg_get_func(int pg)
{
	int mux;
	const struct tegra_pingroup *g;
	u32 val;

	if (!pmx) {
		pr_err("Pingroup not registered yet\n");
		return -EPROBE_DEFER;
	}

	if (pg < 0 || pg >=  pmx->soc->ngroups)
		return -ERANGE;

	g = &pmx->soc->groups[pg];

	if (g->mux_reg < 0 || g->mux_bit)
		return -EINVAL;

	val = pmx_readl(pmx, g->mux_bank, g->mux_reg);
	mux = (val >> g->mux_bit) & 0x3;
	return g->funcs_non_dt[mux];
}
EXPORT_SYMBOL(tegra_pinctrl_pg_get_func);

int tegra_pinctrl_pg_set_tristate(int pg, int tristate)
{
	if (!pmx) {
		pr_err("Pingroup not registered yet\n");
		return -EPROBE_DEFER;
	}

	if (pg < 0 || pg >=  pmx->soc->ngroups)
		return -ERANGE;

	return tegra_pinctrl_set_config(pmx->pctl, pg,
			TEGRA_PINCONF_PARAM_TRISTATE, tristate);
}
EXPORT_SYMBOL(tegra_pinctrl_pg_set_tristate);

int tegra_pinctrl_pg_set_io(int pg, int input)
{
	if (!pmx) {
		pr_err("Pingroup not registered yet\n");
		return -EPROBE_DEFER;
	}

	if (pg < 0 || pg >=  pmx->soc->ngroups)
		return -ERANGE;

	if (pmx->soc->groups[pg].einput_reg < 0)
		return 0;

	return tegra_pinctrl_set_config(pmx->pctl, pg,
			TEGRA_PINCONF_PARAM_ENABLE_INPUT, input);
}
EXPORT_SYMBOL(tegra_pinctrl_pg_set_io);

int tegra_pinctrl_pg_set_lock(int pg, int lock)
{
	int lv = (lock == TEGRA_PIN_LOCK_ENABLE) ? 1 : 0;

	if (!pmx) {
		pr_err("Pingroup not registered yet\n");
		return -EPROBE_DEFER;
	}

	if (pg < 0 || pg >=  pmx->soc->ngroups)
		return -ERANGE;

	if (lock == TEGRA_PIN_LOCK_DEFAULT)
		return 0;

	tegra_pinctrl_set_config(pmx->pctl, pg,
			TEGRA_PINCONF_PARAM_LOCK, lv);
	return 0;
}
EXPORT_SYMBOL(tegra_pinctrl_pg_set_lock);

int tegra_pinctrl_pg_set_od(int pg, int od)
{
	int ov = (od == TEGRA_PIN_OD_ENABLE) ? 1 : 0;

	if (!pmx) {
		pr_err("Pingroup not registered yet\n");
		return -EPROBE_DEFER;
	}

	if (pg < 0 || pg >=  pmx->soc->ngroups)
		return -ERANGE;

	if (od == TEGRA_PIN_OD_DEFAULT)
		return 0;

	tegra_pinctrl_set_config(pmx->pctl, pg,
			TEGRA_PINCONF_PARAM_OPEN_DRAIN, ov);
	return 0;
}
EXPORT_SYMBOL(tegra_pinctrl_pg_set_od);

int tegra_pinctrl_pg_set_ioreset(int pg, int ioreset)
{
	int iov = (ioreset == TEGRA_PIN_IO_RESET_ENABLE) ? 1 : 0;

	if (!pmx) {
		pr_err("Pingroup not registered yet\n");
		return -EPROBE_DEFER;
	}

	if (pg < 0 || pg >=  pmx->soc->ngroups)
		return -ERANGE;

	if (ioreset == TEGRA_PIN_IO_RESET_DEFAULT)
		return 0;

	tegra_pinctrl_set_config(pmx->pctl, pg,
			TEGRA_PINCONF_PARAM_IORESET, iov);
	return 0;
}
EXPORT_SYMBOL(tegra_pinctrl_pg_set_ioreset);

int tegra_pinctrl_pg_set_rcv_sel(int pg, int rcv_sel)
{
	int rcv = (rcv_sel == TEGRA_PIN_RCV_SEL_HIGH) ? 1 : 0;

	if (!pmx) {
		pr_err("Pingroup not registered yet\n");
		return -EPROBE_DEFER;
	}

	if (pg < 0 || pg >=  pmx->soc->ngroups)
		return -ERANGE;

	if (rcv_sel == TEGRA_PIN_RCV_SEL_DEFAULT)
		return 0;

	tegra_pinctrl_set_config(pmx->pctl, pg,
			TEGRA_PINCONF_PARAM_RCV_SEL, rcv);
	return 0;
}
EXPORT_SYMBOL(tegra_pinctrl_pg_set_rcv_sel);

int tegra_pinctrl_pg_set_pullupdown(int pg, int pupd)
{
	if (!pmx) {
		pr_err("Pingroup not registered yet\n");
		return -EPROBE_DEFER;
	}

	if (pg < 0 || pg >=  pmx->soc->ngroups)
		return -ERANGE;

	if (pupd != TEGRA_PUPD_NORMAL &&
	    pupd != TEGRA_PUPD_PULL_DOWN &&
	    pupd != TEGRA_PUPD_PULL_UP)
		return -EINVAL;

	tegra_pinctrl_set_config(pmx->pctl, pg,
			TEGRA_PINCONF_PARAM_PULL, pupd);
	return 0;
}
EXPORT_SYMBOL(tegra_pinctrl_pg_set_pullupdown);

void tegra_pinctrl_pg_config_pingroup(
	const struct tegra_pingroup_config *config)
{
	int pg = config->pingroup;
	enum tegra_mux_func func     = config->func;
	enum tegra_pullupdown pupd   = config->pupd;
	enum tegra_tristate tristate = config->tristate;
#if !defined(CONFIG_ARCH_TEGRA_2x_SOC)
	enum tegra_pin_lock lock     = config->lock;
	enum tegra_pin_od od	 = config->od;
	enum tegra_pin_ioreset ioreset = config->ioreset;
	enum tegra_pin_rcv_sel rcv_sel = config->rcv_sel;
#endif
	const struct tegra_pingroup *g;
	int err;

	if (!pmx) {
		pr_err("Pingroup not registered yet\n");
		return;
	}

	if (pg < 0 || pg >=  pmx->soc->ngroups)
		return;

	g = &pmx->soc->groups[pg];

	if (g->mux_reg >= 0) {
		err = tegra_pinctrl_pg_set_func(config);
		if (err < 0)
			pr_err("pinmux: can't set pingroup %s func to %s: %d\n",
			       g->name, tegra_pinctrl_function_name(func), err);
	}

	if (g->pupd_reg >= 0) {
		err = tegra_pinctrl_pg_set_pullupdown(pg, pupd);
		if (err < 0)
			pr_err("pinmux: can't set pingroup %s pullupdown to %s: %d\n",
			       g->name, tegra_pinctrl_pupd_name(pupd), err);
	}

	if (g->tri_reg >= 0) {
		err = tegra_pinctrl_pg_set_tristate(pg, tristate);
		if (err < 0)
			pr_err("pinmux: can't set pingroup %s tristate to %s: %d\n",
			       g->name, tegra_pinctrl_tri_name(tristate), err);
	}

#if !defined(CONFIG_ARCH_TEGRA_2x_SOC)
	if (g->mux_reg >= 0) {
		err = tegra_pinctrl_pg_set_lock(pg, lock);
		if (err < 0)
			pr_err("pinmux: can't set pingroup %s lock to %s: %d\n",
			       g->name, tegra_pinctrl_lock_name(lock), err);
	}

	if (g->mux_reg >= 0) {
		err = tegra_pinctrl_pg_set_od(pg, od);
		if (err < 0)
			pr_err("pinmux: can't set pingroup %s od to %s: %d\n",
			       g->name, tegra_pinctrl_od_name(od), err);
	}

	if (g->mux_reg >= 0) {
		err = tegra_pinctrl_pg_set_ioreset(pg, ioreset);
		if (err < 0)
			pr_err("pinmux: can't set pingroup %s ioreset to %s: %d\n",
			       g->name, tegra_pinctrl_ioreset_name(ioreset),
				err);
	}
	if (g->mux_reg >= 0) {
		err = tegra_pinctrl_pg_set_rcv_sel(pg, rcv_sel);
		if (err < 0)
			pr_err("pinmux: can't set pingroup %s rcv_sel to %s: %d\n",
			       g->name, tegra_pinctrl_rcv_sel_name(rcv_sel),
				err);
	}
#endif
}
EXPORT_SYMBOL(tegra_pinctrl_pg_config_pingroup);

void tegra_pinctrl_pg_config_table(const struct tegra_pingroup_config *config,
		int len)
{
	int i;

	for (i = 0; i < len; i++)
		tegra_pinctrl_pg_config_pingroup(&config[i]);
}
EXPORT_SYMBOL(tegra_pinctrl_pg_config_table);

int tegra_pinctrl_pg_drive_set_hsm(int pdg, int hsm)
{
	int val = (hsm == TEGRA_HSM_ENABLE) ? 1 : 0;
	int pg = pmx->drive_group_start_index + pdg;

	if (pg < 0 || pg >=  pmx->soc->ngroups)
		return -ERANGE;

	if (hsm != TEGRA_HSM_ENABLE && hsm != TEGRA_HSM_DISABLE)
		return -EINVAL;

	tegra_pinctrl_set_config(pmx->pctl, pg,
			TEGRA_PINCONF_PARAM_HIGH_SPEED_MODE, val);
	return 0;
}
EXPORT_SYMBOL(tegra_pinctrl_pg_drive_set_hsm);

int tegra_pinctrl_pg_drive_set_schmitt(int pdg, int schmitt)
{
	int pg = pmx->drive_group_start_index + pdg;
	int val = (schmitt == TEGRA_SCHMITT_ENABLE) ? 1 : 0;

	if (pg < 0 || pg >=  pmx->soc->ngroups)
		return -ERANGE;

	if (schmitt != TEGRA_SCHMITT_ENABLE && schmitt != TEGRA_SCHMITT_DISABLE)
		return -EINVAL;

	tegra_pinctrl_set_config(pmx->pctl, pg,
			TEGRA_PINCONF_PARAM_SCHMITT, val);
	return 0;
}
EXPORT_SYMBOL(tegra_pinctrl_pg_drive_set_schmitt);

int tegra_pinctrl_pg_drive_set_drive(int pdg, int drive)
{
	int pg = pmx->drive_group_start_index + pdg;

	if (pg < 0 || pg >=  pmx->soc->ngroups)
		return -ERANGE;

	if (drive < 0 || drive >= TEGRA_MAX_DRIVE)
		return -EINVAL;

	tegra_pinctrl_set_config(pmx->pctl, pg,
			TEGRA_PINCONF_PARAM_LOW_POWER_MODE, drive);
	return 0;
}
EXPORT_SYMBOL(tegra_pinctrl_pg_drive_set_drive);

int tegra_pinctrl_pg_drive_set_pull_down(int pdg, int pull_down)
{
	int pg = pmx->drive_group_start_index + pdg;

	if (pg < 0 || pg >=  pmx->soc->ngroups)
		return -ERANGE;

	if (pull_down < 0 || pull_down >= TEGRA_MAX_PULL)
		return -EINVAL;

	tegra_pinctrl_set_config(pmx->pctl, pg,
			TEGRA_PINCONF_PARAM_DRIVE_DOWN_STRENGTH, pull_down);
	return 0;
}
EXPORT_SYMBOL(tegra_pinctrl_pg_drive_set_pull_down);

int tegra_pinctrl_pg_drive_set_pull_up(int pdg, int pull_up)
{
	int pg = pmx->drive_group_start_index + pdg;

	if (pg < 0 || pg >=  pmx->soc->ngroups)
		return -ERANGE;

	if (pull_up < 0 || pull_up >= TEGRA_MAX_PULL)
		return -EINVAL;

	tegra_pinctrl_set_config(pmx->pctl, pg,
			TEGRA_PINCONF_PARAM_DRIVE_UP_STRENGTH, pull_up);
	return 0;
}
EXPORT_SYMBOL(tegra_pinctrl_pg_drive_set_pull_up);

int tegra_pinctrl_pg_drive_set_slew_rising(int pdg, int slew_rising)
{
	int pg = pmx->drive_group_start_index + pdg;

	if (pg < 0 || pg >=  pmx->soc->ngroups)
		return -ERANGE;

	if (slew_rising < 0 || slew_rising >= TEGRA_MAX_SLEW)
		return -EINVAL;

	tegra_pinctrl_set_config(pmx->pctl, pg,
			TEGRA_PINCONF_PARAM_SLEW_RATE_RISING, slew_rising);
	return 0;
}
EXPORT_SYMBOL(tegra_pinctrl_pg_drive_set_slew_rising);

int tegra_pinctrl_pg_drive_set_slew_falling(int pdg, int slew_falling)
{
	int pg = pmx->drive_group_start_index + pdg;

	if (pg < 0 || pg >=  pmx->soc->ngroups)
		return -ERANGE;

	if (slew_falling < 0 || slew_falling >= TEGRA_MAX_SLEW)
		return -EINVAL;

	tegra_pinctrl_set_config(pmx->pctl, pg,
			TEGRA_PINCONF_PARAM_SLEW_RATE_FALLING, slew_falling);
	return 0;
}
EXPORT_SYMBOL(tegra_pinctrl_pg_drive_set_slew_falling);

int tegra_pinctrl_pg_drive_set_drive_type(int pdg, int drive_type)
{
	int pg = pmx->drive_group_start_index + pdg;

	if (pg < 0 || pg >=  pmx->soc->ngroups)
		return -ERANGE;

	if (pmx->soc->groups[pg].drvtype_reg < 0)
		return 0;

	if (drive_type < 0 || drive_type >= TEGRA_MAX_DRIVE_TYPE)
		return -EINVAL;

	tegra_pinctrl_set_config(pmx->pctl, pg,
			TEGRA_PINCONF_PARAM_DRIVE_TYPE, drive_type);
	return 0;
}
EXPORT_SYMBOL(tegra_pinctrl_pg_drive_set_drive_type);

void tegra_pinctrl_pg_drive_config_pingroup(int pingroup,
	int hsm, int schmitt, int drive, int pull_down,
	int pull_up, int slew_rising, int slew_falling,
	int drive_type)
{
	int err;
	int pg = pmx->drive_group_start_index + pingroup;


	err = tegra_pinctrl_pg_drive_set_hsm(pingroup, hsm);
	if (err < 0)
		pr_err("pinmux: can't set pingroup %s hsm to %s: %d\n",
			drive_pinmux_name(pg),
			enable_name(hsm), err);

	err = tegra_pinctrl_pg_drive_set_schmitt(pingroup, schmitt);
	if (err < 0)
		pr_err("pinmux: can't set pingroup %s schmitt to %s: %d\n",
			drive_pinmux_name(pg),
			enable_name(schmitt), err);

	err = tegra_pinctrl_pg_drive_set_drive(pingroup, drive);
	if (err < 0)
		pr_err("pinmux: can't set pingroup %s drive to %s: %d\n",
			drive_pinmux_name(pg),
			drive_name(drive), err);

	err = tegra_pinctrl_pg_drive_set_pull_down(pingroup, pull_down);
	if (err < 0)
		pr_err("pinmux: can't set pingroup %s pull down to %d: %d\n",
			drive_pinmux_name(pg),
			pull_down, err);

	err = tegra_pinctrl_pg_drive_set_pull_up(pingroup, pull_up);
	if (err < 0)
		pr_err("pinmux: can't set pingroup %s pull up to %d: %d\n",
			drive_pinmux_name(pg),
			pull_up, err);

	err = tegra_pinctrl_pg_drive_set_slew_rising(pingroup, slew_rising);
	if (err < 0)
		pr_err("pinmux: can't set pingroup %s rising slew to %s: %d\n",
			drive_pinmux_name(pg),
			slew_name(slew_rising), err);

	err = tegra_pinctrl_pg_drive_set_slew_falling(pingroup, slew_falling);
	if (err < 0)
		pr_err("pinmux: can't set pingroup %s falling slew to %s: %d\n",
			drive_pinmux_name(pg),
			slew_name(slew_falling), err);

	err = tegra_pinctrl_pg_drive_set_drive_type(pingroup, drive_type);
	if (err < 0)
		pr_err("pinmux: can't set pingroup %s driver type to %d: %d\n",
			drive_pinmux_name(pg),
			drive_type, err);
}
EXPORT_SYMBOL(tegra_pinctrl_pg_drive_config_pingroup);

void tegra_pinctrl_pg_drive_config_table(
	struct tegra_drive_pingroup_config *config, int len)
{
	int i;

	for (i = 0; i < len; i++)
		tegra_pinctrl_pg_drive_config_pingroup(config[i].pingroup,
						     config[i].hsm,
						     config[i].schmitt,
						     config[i].drive,
						     config[i].pull_down,
						     config[i].pull_up,
						     config[i].slew_rising,
						     config[i].slew_falling,
						     config[i].drive_type);
}
EXPORT_SYMBOL(tegra_pinctrl_pg_drive_config_table);

int tegra_pinctrl_pg_drive_get_pingroup(struct device *dev)
{
	int pg = -1;
	const char *dev_id;

	if (!dev || !pmx)
		return -EINVAL;

	dev_id = dev_name(dev);
	for (pg = 0; pg < pmx->soc->ngroups; pg++) {
		if (pmx->soc->groups[pg].dev_id &&
			!(strcmp(pmx->soc->groups[pg].dev_id, dev_id))) {
			if (pg >= pmx->drive_group_start_index)
				pg -= pmx->drive_group_start_index;
			break;
		}
	}

	return (pg == pmx->soc->ngroups) ? -EINVAL : pg;
}
EXPORT_SYMBOL(tegra_pinctrl_pg_drive_get_pingroup);

void tegra_pinctrl_pg_set_safe_pinmux_table(
	const struct tegra_pingroup_config *config, int len)
{
	int i;
	struct tegra_pingroup_config c;

	for (i = 0; i < len; i++) {
		int err;
		c = config[i];
		if (c.pingroup < 0 || c.pingroup >= pmx->soc->ngroups) {
			WARN_ON(1);
			continue;
		}
		c.func = pmx->soc->groups[c.pingroup].func_safe;
		err = tegra_pinctrl_pg_set_func(&c);
		if (err < 0)
			pr_err("%s: tegra_pinctrl_pg_set_func returned %d "
				"setting %s to %s\n", __func__, err,
				pmx->soc->groups[c.pingroup].name,
				tegra_pinctrl_function_name(c.func));
	}
}
EXPORT_SYMBOL(tegra_pinctrl_pg_set_safe_pinmux_table);

void tegra_pinctrl_pg_config_pinmux_table(
	const struct tegra_pingroup_config *config, int len)
{
	int i;

	for (i = 0; i < len; i++) {
		int err;
		if (config[i].pingroup < 0 ||
		    config[i].pingroup >= pmx->soc->ngroups) {
			WARN_ON(1);
			continue;
		}
		err = tegra_pinctrl_pg_set_func(&config[i]);
		if (err < 0)
			pr_err("%s: tegra_pinctrl_pg_set_func returned %d "
				"setting %s to %s\n", __func__, err,
				pmx->soc->groups[config[i].pingroup].name,
			       tegra_pinctrl_function_name(config[i].func));
	}
}
EXPORT_SYMBOL(tegra_pinctrl_pg_config_pinmux_table);

void tegra_pinctrl_pg_config_tristate_table(
	const struct tegra_pingroup_config *config,
	int len, int tristate)
{
	int i;
	int err;
	int pingroup;

	for (i = 0; i < len; i++) {
		pingroup = config[i].pingroup;
		if (pmx->soc->groups[pingroup].tri_reg > 0) {
			err = tegra_pinctrl_pg_set_tristate(pingroup, tristate);
			if (err < 0)
				pr_err("pinmux: can't set pingroup %s tristate"
					" to %s: %d\n",
					pmx->soc->groups[pingroup].name,
					tegra_pinctrl_tri_name(tristate), err);
		}
	}
}
EXPORT_SYMBOL(tegra_pinctrl_pg_config_tristate_table);

void tegra_pinctrl_pg_config_pullupdown_table(
	const struct tegra_pingroup_config *config,
	int len, int pupd)
{
	int i;
	int err;
	int pingroup;

	for (i = 0; i < len; i++) {
		pingroup = config[i].pingroup;
		if (pmx->soc->groups[pingroup].pupd_reg > 0) {
			err = tegra_pinctrl_pg_set_pullupdown(pingroup, pupd);
			if (err < 0)
				pr_err("pinmux: can't set pingroup %s pullupdown"
					" to %s: %d\n",
					pmx->soc->groups[pingroup].name,
					tegra_pinctrl_pupd_name(pupd), err);
		}
	}
}
EXPORT_SYMBOL(tegra_pinctrl_pg_config_pullupdown_table);

#ifdef	CONFIG_DEBUG_FS

#include <linux/debugfs.h>
#include <linux/seq_file.h>

static void dbg_pad_field(struct seq_file *s, int len)
{
	seq_putc(s, ',');

	while (len-- > -1)
		seq_putc(s, ' ');
}

static int dbg_pinmux_show(struct seq_file *s, void *unused)
{
	int i;
	int len;

	for (i = 0; i < pmx->soc->ngroups; i++) {
		unsigned long reg;
		unsigned long tri;
		unsigned long mux;
		unsigned long pupd;

		if (!pmx->soc->groups[i].name)
			continue;

		if (pmx->soc->groups[i].mux_reg < 0)
			continue;

		seq_printf(s, "\t{%s", pmx->soc->groups[i].name);
		len = strlen(pmx->soc->groups[i].name);
		dbg_pad_field(s, 15 - len);

		if (pmx->soc->groups[i].mux_reg < 0) {
			seq_puts(s, "TEGRA_MUX_NONE");
			len = strlen("NONE");
		} else {
			reg = pmx_readl(pmx, pmx->soc->groups[i].mux_bank,
					pmx->soc->groups[i].mux_reg);
			mux = (reg >> pmx->soc->groups[i].mux_bit) & 0x3;
			BUG_ON(pmx->soc->groups[i].funcs[mux] == 0);
			if (pmx->soc->groups[i].funcs[mux] ==
						TEGRA_MUX_INVALID) {
				seq_puts(s, "TEGRA_MUX_INVALID");
				len = 7;
			} else if (pmx->soc->groups[i].funcs[mux] &
						TEGRA_MUX_RSVD) {
				seq_printf(s, "TEGRA_MUX_RSVD%1lu", mux);
				len = 5;
			} else {
				seq_printf(s, "TEGRA_MUX_%s",
					tegra_pinctrl_function_name(
					   pmx->soc->groups[i].funcs[mux]));
				len = strlen(tegra_pinctrl_function_name(
					pmx->soc->groups[i].funcs[mux]));
			}
		}
		dbg_pad_field(s, 13-len);

		if (pmx->soc->groups[i].einput_reg >= 0) {
			unsigned long io;
			io = (pmx_readl(pmx, pmx->soc->groups[i].mux_bank,
				pmx->soc->groups[i].mux_reg) >> 5) & 0x1;
			seq_printf(s, "TEGRA_PIN_%s",
					tegra_pinctrl_io_name(io));
			len = strlen(tegra_pinctrl_io_name(io));
			dbg_pad_field(s, 6 - len);
		}
		if (pmx->soc->groups[i].pupd_reg < 0) {
			seq_puts(s, "TEGRA_PUPD_NORMAL");
			len = strlen("NORMAL");
		} else {
			reg = pmx_readl(pmx, pmx->soc->groups[i].pupd_bank,
					pmx->soc->groups[i].pupd_reg);
			pupd = (reg >> pmx->soc->groups[i].pupd_bit) & 0x3;
			seq_printf(s, "TEGRA_PUPD_%s",
				tegra_pinctrl_pupd_name(pupd));
			len = strlen(tegra_pinctrl_pupd_name(pupd));
		}
		dbg_pad_field(s, 9 - len);

		if (pmx->soc->groups[i].tri_reg < 0) {
			seq_puts(s, "TEGRA_TRI_NORMAL");
		} else {
			reg = pmx_readl(pmx, pmx->soc->groups[i].tri_bank,
					pmx->soc->groups[i].tri_reg);
			tri = (reg >> pmx->soc->groups[i].tri_bit) & 0x1;

			seq_printf(s, "TEGRA_TRI_%s",
					tegra_pinctrl_tri_name(tri));
		}
		seq_puts(s, "},\n");
	}
	return 0;
}

static int dbg_pinmux_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_pinmux_show, &inode->i_private);
}

/*
 * Changing pinmux configuration at runtime
 *
 * Usage: Feed "<PINGROUP> <FUNCTION> <E_INPUT> <PUPD> <TRISTATE>"
 *	to tegra_pinmux
 * ex) # echo "HDMI_CEC CEC OUTPUT NORMAL TRISTATE" > /d/tegra_pinmux
 */
#define DELIMITER "\n"
static ssize_t dbg_pinmux_write(struct file *file,
	const char __user *userbuf, size_t count, loff_t *ppos)
{
	char buf[80];
	char *pbuf = &buf[0];
	char *token;
	struct tegra_pingroup_config pg_config;
	int i;

	if (sizeof(buf) <= count)
		return -EINVAL;
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	pr_debug("%s buf: %s\n", __func__, buf);

	/* ping group index by name */
	token = strsep(&pbuf, DELIMITER);
	for (i = 0; i < pmx->soc->ngroups; i++)
		if (!strcmp(token, pmx->soc->groups[i].name))
			break;
	if (i == pmx->soc->ngroups) { /* no pingroup matched with name */
		pr_err("no pingroup matched with name\n");
		return -EINVAL;
	}
	pg_config.pingroup = i;

	/* func index by name */
	token = strsep(&pbuf, DELIMITER);
	for (i = 0; i < TEGRA_MAX_MUX; i++)
		if (!strcmp(token, tegra_pinctrl_function_name(i)))
			break;
	if (i == TEGRA_MAX_MUX) { /* no func matched with name */
		pr_err("no func matched with name\n");
		return -EINVAL;
	}
	pg_config.func = i;

	/* i/o by name */
	token = strsep(&pbuf, DELIMITER);
	i = !strcmp(token, "OUTPUT") ? 0 :
		!strcmp(token, "INPUT") ? 1 : -1;
	if (i == -1) { /* no IO matched with name */
		pr_err("no IO matched with name\n");
		return -EINVAL;
	}
	pg_config.io = i;

	/* pull up/down by name */
	token = strsep(&pbuf, DELIMITER);
	i = !strcmp(token, "NORMAL") ? 0 :
		!strcmp(token, "PULL_DOWN") ? 1 :
		!strcmp(token, "PULL_UP") ? 2 : -1;
	if (i == -1) { /* no PUPD matched with  name */
		pr_err("no PUPD matched with  name\n");
		return -EINVAL;
	}
	pg_config.pupd = i;

	/* tristate by name */
	token = strsep(&pbuf, DELIMITER);
	i = !strcmp(token, "NORMAL") ? 0 :
		!strcmp(token, "TRISTATE") ? 1 : -1;
	if (i == -1) { /* no tristate matched with name */
		pr_err("no tristate matched with name\n");
		return -EINVAL;
	}
	pg_config.tristate = i;

	pr_debug("pingroup=%d, func=%d, io=%d, pupd=%d, tristate=%d\n",
			pg_config.pingroup, pg_config.func, pg_config.io,
			pg_config.pupd, pg_config.tristate);
	tegra_pinctrl_pg_set_func(&pg_config);
	tegra_pinctrl_pg_set_pullupdown(pg_config.pingroup, pg_config.pupd);
	tegra_pinctrl_pg_set_tristate(pg_config.pingroup, pg_config.tristate);

	return count;
}

static const struct file_operations debug_fops = {
	.open		= dbg_pinmux_open,
	.write		= dbg_pinmux_write,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int dbg_drive_pinmux_show(struct seq_file *s, void *unused)
{
	int i;
	int len;
	u8 offset;

	for (i = 0; i < pmx->soc->ngroups; i++) {
		u32 reg;

		if (pmx->soc->groups[i].drv_reg < 0)
			continue;

		seq_printf(s, "\t{%s", pmx->soc->groups[i].name);
		len = strlen(pmx->soc->groups[i].name);
		dbg_pad_field(s, 15 - len);


		reg = pmx_readl(pmx, pmx->soc->groups[i].drv_bank,
					pmx->soc->groups[i].drv_reg);
		if (HSM_EN(reg)) {
			seq_puts(s, "TEGRA_HSM_ENABLE");
			len = 16;
		} else {
			seq_puts(s, "TEGRA_HSM_DISABLE");
			len = 17;
		}
		dbg_pad_field(s, 17 - len);

		if (SCHMT_EN(reg)) {
			seq_puts(s, "TEGRA_SCHMITT_ENABLE");
			len = 21;
		} else {
			seq_puts(s, "TEGRA_SCHMITT_DISABLE");
			len = 22;
		}
		dbg_pad_field(s, 22 - len);

		seq_printf(s, "TEGRA_DRIVE_%s", drive_name(LPMD(reg)));
		len = strlen(drive_name(LPMD(reg)));
		dbg_pad_field(s, 5 - len);

		offset = pmx->soc->groups[i].drvdn_bit;
		seq_printf(s, "TEGRA_PULL_%d", DRVDN(reg, offset));
		len = DRVDN(reg, offset) < 10 ? 1 : 2;
		dbg_pad_field(s, 2 - len);

		offset = pmx->soc->groups[i].drvup_bit;
		seq_printf(s, "TEGRA_PULL_%d", DRVUP(reg, offset));
		len = DRVUP(reg, offset) < 10 ? 1 : 2;
		dbg_pad_field(s, 2 - len);

		offset = pmx->soc->groups[i].slwr_bit;
		seq_printf(s, "TEGRA_SLEW_%s", slew_name(SLWR(reg, offset)));
		len = strlen(slew_name(SLWR(reg, offset)));
		dbg_pad_field(s, 7 - len);

		offset = pmx->soc->groups[i].slwf_bit;
		seq_printf(s, "TEGRA_SLEW_%s", slew_name(SLWF(reg, offset)));

		seq_puts(s, "},\n");
	}
	return 0;
}

static int dbg_drive_pinmux_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_drive_pinmux_show, &inode->i_private);
}

static const struct file_operations debug_drive_fops = {
	.open		= dbg_drive_pinmux_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int dbg_reg_pinmux_show(struct seq_file *s, void *unused)
{
	int i;
	u32 offset;
	u32 reg;
	int bank;

	for (i = 0; i < pmx->soc->ngroups; i++) {
		if (pmx->soc->groups[i].drv_reg < 0) {
			bank = pmx->soc->groups[i].mux_bank;
			offset = pmx->soc->groups[i].mux_reg;
		} else {
			bank = pmx->soc->groups[i].drv_bank;
			offset = pmx->soc->groups[i].drv_reg;
		}
		reg = pmx_readl(pmx, bank, offset);
		seq_printf(s, "Bank: %d Reg: 0x%08x Val: 0x%08x\n",
			bank, offset, reg);
	}
	return 0;
}

static int dbg_reg_pinmux_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_reg_pinmux_show, &inode->i_private);
}

static const struct file_operations debug_reg_fops = {
	.open		= dbg_reg_pinmux_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init tegra_pinctrl_debuginit(void)
{
	if (!pmx)
		return 0;

	(void) debugfs_create_file("tegra_pinctrl", S_IRUGO | S_IWUSR | S_IWGRP,
					NULL, NULL, &debug_fops);
	(void) debugfs_create_file("tegra_pinctrl_drive", S_IRUGO,
					NULL, NULL, &debug_drive_fops);
	(void) debugfs_create_file("tegra_pinctrl_reg", S_IRUGO,
					NULL, NULL, &debug_reg_fops);
	return 0;
}
late_initcall(tegra_pinctrl_debuginit);
#endif
