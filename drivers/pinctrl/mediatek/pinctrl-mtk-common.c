/*
 * mt65xx pinctrl driver based on Allwinner A1X pinctrl driver.
 * Copyright (c) 2014 MediaTek Inc.
 * Author: Hongzhou.Yang <hongzhou.yang@mediatek.com>
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

#include <linux/io.h>
#include <linux/gpio/driver.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pinctrl/machine.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/bitops.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/pm.h>
#include <dt-bindings/pinctrl/mt65xx.h>

#include "../core.h"
#include "../pinconf.h"
#include "../pinctrl-utils.h"
#include "pinctrl-mtk-common.h"
#if defined(CONFIG_PINCTRL_MTK_ALTERNATIVE)
#include "pinctrl-mtk-common_debug.h"
struct mtk_pinctrl *pctl_alt;
#endif
#define MAX_GPIO_MODE_PER_REG 5
#define GPIO_MODE_BITS        3
#define GPIO_MODE_PREFIX "GPIO"

static const char * const mtk_gpio_functions[] = {
	"func0", "func1", "func2", "func3",
	"func4", "func5", "func6", "func7",
	"func8", "func9", "func10", "func11",
	"func12", "func13", "func14", "func15",
};
static const struct mtk_pin_info *mtk_pinctrl_get_gpio_array(int pin, int size,
	const struct mtk_pin_info pArray[])
{
	int i = 0;

	for (i = 0; i < size; i++) {
		if (pin == pArray[i].pin)
			return &pArray[i];
	}
	return NULL;
}

int mtk_pinctrl_set_gpio_value(struct mtk_pinctrl *pctl, int pin,
	bool value, int size, const struct mtk_pin_info pin_info[])
{
	unsigned int reg_bit, reg_set_addr, reg_rst_addr;
	const struct mtk_pin_info *spec_pin_info;
	struct regmap *regmap;
	unsigned char  port_align;
	unsigned char bit_width;
	unsigned int mask, reg_value;

	spec_pin_info = mtk_pinctrl_get_gpio_array(pin, size, pin_info);

	if (spec_pin_info != NULL) {
		port_align = pctl->devdata->port_align;
		reg_set_addr = spec_pin_info->offset + port_align;
		reg_rst_addr = spec_pin_info->offset + (port_align << 1);
		reg_bit = BIT(spec_pin_info->bit);
		regmap = pctl->regmap[spec_pin_info->ip_num];
		reg_value = value << spec_pin_info->bit;
		bit_width = spec_pin_info->width;
		mask = (BIT(bit_width) - 1) << spec_pin_info->bit;
		return regmap_update_bits(regmap,
			spec_pin_info->offset, mask, reg_value);
	} else {
		return -EPERM;
	}
	return 0;
}

int mtk_pinctrl_update_gpio_value(struct mtk_pinctrl *pctl, int pin,
	unsigned char value, int size, const struct mtk_pin_info pin_info[])
{
	unsigned int reg_update_addr;
	unsigned int mask, reg_value;
	const struct mtk_pin_info *spec_update_pin;
	struct regmap *regmap;
	unsigned char bit_width;

	spec_update_pin = mtk_pinctrl_get_gpio_array(pin, size, pin_info);

	if (spec_update_pin != NULL) {
		reg_update_addr = spec_update_pin->offset;
		regmap = pctl->regmap[spec_update_pin->ip_num];
		reg_value = value << spec_update_pin->bit;
		bit_width = spec_update_pin->width;
		mask = (BIT(bit_width) - 1) << spec_update_pin->bit;
		return regmap_update_bits(regmap,
			reg_update_addr, mask, reg_value);
	} else {
		return -EPERM;
	}

	return 0;
}

int mtk_pinctrl_get_gpio_value(struct mtk_pinctrl *pctl,
	int pin, int size, const struct mtk_pin_info pin_info[])
{
	unsigned int reg_value, reg_get_addr;
	const struct mtk_pin_info *spec_pin_info;
	struct regmap *regmap;
	unsigned char bit_width, reg_bit;

	spec_pin_info = mtk_pinctrl_get_gpio_array(pin, size, pin_info);

	if (spec_pin_info != NULL) {
		reg_get_addr = spec_pin_info->offset;
		bit_width = spec_pin_info->width;
		reg_bit = spec_pin_info->bit;
		regmap = pctl->regmap[spec_pin_info->ip_num];
		regmap_read(regmap, reg_get_addr, &reg_value);
		return ((reg_value >> reg_bit) & (BIT(bit_width) - 1));
	} else {
		return -EPERM;
	}

	return 0;
}

/*
 * There are two base address for pull related configuration
 * in mt8135, and different GPIO pins use different base address.
 * When pin number greater than type1_start and less than type1_end,
 * should use the second base address.
 */
static struct regmap *mtk_get_regmap(struct mtk_pinctrl *pctl,
		unsigned long pin)
{
	if (pin >= pctl->devdata->type1_start && pin < pctl->devdata->type1_end)
		return pctl->regmap2;
	return pctl->regmap1;
}

static unsigned int mtk_get_port(struct mtk_pinctrl *pctl, unsigned long pin)
{
	/* Different SoC has different mask and port shift. */
	return ((pin >> pctl->devdata->port_pin_shf) & pctl->devdata->port_mask)
			<< pctl->devdata->port_shf;
}

static int mtk_pmx_gpio_set_direction(struct pinctrl_dev *pctldev,
			struct pinctrl_gpio_range *range, unsigned offset,
			bool input)
{
	unsigned int reg_addr;
	unsigned int bit;
	struct mtk_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

#if defined(CONFIG_PINCTRL_MTK_ALTERNATIVE)
	if (pctl->devdata->pin_dir_grps)/* because input is true */
		return mtk_pinctrl_set_gpio_direction(pctl, offset, !input);
#endif

	reg_addr = mtk_get_port(pctl, offset) + pctl->devdata->dir_offset;
	bit = BIT(offset & pctl->devdata->port_mask);

	if (pctl->devdata->spec_dir_set)
		pctl->devdata->spec_dir_set(&reg_addr, offset);

	if (input)
		/* Different SoC has different alignment offset. */
		reg_addr = CLR_ADDR(reg_addr, pctl);
	else
		reg_addr = SET_ADDR(reg_addr, pctl);

	regmap_write(mtk_get_regmap(pctl, offset), reg_addr, bit);
	return 0;
}

static void mtk_gpio_set(struct gpio_chip *chip, unsigned offset, int value)
{
	unsigned int reg_addr;
	unsigned int bit;
	struct mtk_pinctrl *pctl = gpiochip_get_data(chip);

#if defined(CONFIG_PINCTRL_MTK_ALTERNATIVE)
	if (pctl->devdata->pin_dout_grps) {
		/* Just Used by smartphone projects */
		mtk_pinctrl_set_gpio_output(pctl, offset, value);
		return;
	}
#endif
	reg_addr = mtk_get_port(pctl, offset) + pctl->devdata->dout_offset;
	bit = BIT(offset & pctl->devdata->port_mask);

	if (value)
		reg_addr = SET_ADDR(reg_addr, pctl);
	else
		reg_addr = CLR_ADDR(reg_addr, pctl);

	regmap_write(mtk_get_regmap(pctl, offset), reg_addr, bit);
}

static int mtk_pconf_set_ies_smt(struct mtk_pinctrl *pctl, unsigned pin,
		int value, enum pin_config_param arg)
{
	unsigned int reg_addr, offset;
	unsigned int bit;

#if defined(CONFIG_PINCTRL_MTK_ALTERNATIVE)
	if (pctl->devdata->pin_ies_grps ||
		pctl->devdata->pin_smt_grps) {
		if (arg == PIN_CONFIG_INPUT_ENABLE)
			return mtk_pinctrl_set_gpio_ies(pctl,
				pin, value);
		else if (arg == PIN_CONFIG_INPUT_SCHMITT_ENABLE)
			return mtk_pinctrl_set_gpio_smt(pctl,
				pin, value);
	}
#endif

	/**
	 * Due to some soc are not support ies/smt config, add this special
	 * control to handle it.
	 */
	if (!pctl->devdata->spec_ies_smt_set &&
		pctl->devdata->ies_offset == MTK_PINCTRL_NOT_SUPPORT &&
			arg == PIN_CONFIG_INPUT_ENABLE)
		return -EINVAL;

	if (!pctl->devdata->spec_ies_smt_set &&
		pctl->devdata->smt_offset == MTK_PINCTRL_NOT_SUPPORT &&
			arg == PIN_CONFIG_INPUT_SCHMITT_ENABLE)
		return -EINVAL;

	/*
	 * Due to some pins are irregular, their input enable and smt
	 * control register are discontinuous, so we need this special handle.
	 */
	if (pctl->devdata->spec_ies_smt_set) {
		return pctl->devdata->spec_ies_smt_set(pctl,
			mtk_get_regmap(pctl, pin), pin,
			pctl->devdata->port_align, value, arg);
	}

	bit = BIT(pin & 0xf);

	if (arg == PIN_CONFIG_INPUT_ENABLE)
		offset = pctl->devdata->ies_offset;
	else
		offset = pctl->devdata->smt_offset;

	if (value)
		reg_addr = SET_ADDR(mtk_get_port(pctl, pin) + offset, pctl);
	else
		reg_addr = CLR_ADDR(mtk_get_port(pctl, pin) + offset, pctl);

	regmap_write(mtk_get_regmap(pctl, pin), reg_addr, bit);
	return 0;
}

int mtk_pconf_spec_set_ies_smt_range(struct regmap *regmap,
		const struct mtk_pin_ies_smt_set *ies_smt_infos,
		unsigned int info_num, unsigned int pin,
		unsigned char align, int value)
{
	unsigned int i, reg_addr, bit;

	for (i = 0; i < info_num; i++) {
		if (pin >= ies_smt_infos[i].start &&
				pin <= ies_smt_infos[i].end) {
			break;
		}
	}

	if (i == info_num)
		return -EINVAL;

	if (value)
		reg_addr = ies_smt_infos[i].offset + align;
	else
		reg_addr = ies_smt_infos[i].offset + (align << 1);

	bit = BIT(ies_smt_infos[i].bit);
	regmap_write(regmap, reg_addr, bit);
	return 0;
}

static const struct mtk_pin_drv_grp *mtk_find_pin_drv_grp_by_pin(
		struct mtk_pinctrl *pctl,  unsigned long pin) {
	int i;

	for (i = 0; i < pctl->devdata->n_pin_drv_grps; i++) {
		const struct mtk_pin_drv_grp *pin_drv =
				pctl->devdata->pin_drv_grp + i;
		if (pin == pin_drv->pin)
			return pin_drv;
	}

	return NULL;
}

static void mtk_pconf_set_direction(struct mtk_pinctrl *pctl, unsigned int pin,
		int value, enum pin_config_param param)

{
	if (pctl->devdata->pin_dir_grps)
		mtk_pinctrl_set_gpio_direction(pctl, pin, value);
}

static int mtk_pconf_set_driving(struct mtk_pinctrl *pctl,
		unsigned int pin, unsigned char driving)
{
	const struct mtk_pin_drv_grp *pin_drv;
	unsigned int val;
	unsigned int bits, mask, shift;
	const struct mtk_drv_group_desc *drv_grp;

	if (pctl->devdata->pin_drv_grps) {
		return mtk_pinctrl_set_gpio_driving(pctl,
			pin, driving);
	}

#if defined(CONFIG_PINCTRL_MTK_ALTERNATIVE)
	if (pctl->devdata->mtk_pctl_set_gpio_drv)
		return pctl->devdata->mtk_pctl_set_gpio_drv(pctl,
			pin, driving);
#endif

	if (pin >= pctl->devdata->npins)
		return -EINVAL;

	pin_drv = mtk_find_pin_drv_grp_by_pin(pctl, pin);
	if (!pin_drv || pin_drv->grp > pctl->devdata->n_grp_cls)
		return -EINVAL;

	drv_grp = pctl->devdata->grp_desc + pin_drv->grp;
	if (driving >= drv_grp->min_drv && driving <= drv_grp->max_drv
		&& !(driving % drv_grp->step)) {
		val = driving / drv_grp->step - 1;
		bits = drv_grp->high_bit - drv_grp->low_bit + 1;
		mask = BIT(bits) - 1;
		shift = pin_drv->bit + drv_grp->low_bit;
		mask <<= shift;
		val <<= shift;
		return regmap_update_bits(mtk_get_regmap(pctl, pin),
				pin_drv->offset, mask, val);
	}

	return -EINVAL;
}

int mtk_pctrl_spec_pull_set_samereg(struct mtk_pinctrl *pctl,
		struct regmap *regmap,
		const struct mtk_pin_spec_pupd_set_samereg *pupd_infos,
		unsigned int info_num, unsigned int pin,
		unsigned char align, bool isup, unsigned int r1r0)
{
	unsigned int i;
	unsigned int reg_pupd, reg_set, reg_rst;
	unsigned int bit_pupd, bit_r0, bit_r1;
	const struct mtk_pin_spec_pupd_set_samereg *spec_pupd_pin;
	bool find = false;

	for (i = 0; i < info_num; i++) {
		if (pin == pupd_infos[i].pin) {
			find = true;
			break;
		}
	}

	if (!find)
		return -EINVAL;

	spec_pupd_pin = pupd_infos + i;
	reg_set = spec_pupd_pin->offset + align;
	reg_rst = spec_pupd_pin->offset + (align << 1);

	if (isup)
		reg_pupd = reg_rst;
	else
		reg_pupd = reg_set;

	if (spec_pupd_pin->ip_num != 0)
		regmap = pctl->regmap[spec_pupd_pin->ip_num];
	bit_pupd = BIT(spec_pupd_pin->pupd_bit);
	regmap_write(regmap, reg_pupd, bit_pupd);

	bit_r0 = BIT(spec_pupd_pin->r0_bit);
	bit_r1 = BIT(spec_pupd_pin->r1_bit);

	switch (r1r0) {
	case MTK_PUPD_SET_R1R0_00:
		regmap_write(regmap, reg_rst, bit_r0);
		regmap_write(regmap, reg_rst, bit_r1);
		break;
	case MTK_PUPD_SET_R1R0_01:
		regmap_write(regmap, reg_set, bit_r0);
		regmap_write(regmap, reg_rst, bit_r1);
		break;
	case MTK_PUPD_SET_R1R0_10:
		regmap_write(regmap, reg_rst, bit_r0);
		regmap_write(regmap, reg_set, bit_r1);
		break;
	case MTK_PUPD_SET_R1R0_11:
		regmap_write(regmap, reg_set, bit_r0);
		regmap_write(regmap, reg_set, bit_r1);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

int mtk_spec_pull_get_samereg(struct regmap *regmap,
		const struct mtk_pin_spec_pupd_set_samereg *pupd_infos,
		unsigned int info_num, unsigned int pin)
{
	unsigned int i;
	unsigned int reg_pupd;
	unsigned int val = 0, bit_pupd, bit_r0, bit_r1;
	const struct mtk_pin_spec_pupd_set_samereg *spec_pupd_pin;
	bool find = false;

	for (i = 0; i < info_num; i++) {
		if (pin == pupd_infos[i].pin) {
			find = true;
			break;
		}
	}

	if (!find)
		return -1;

	spec_pupd_pin = pupd_infos + i;
	reg_pupd = spec_pupd_pin->offset;

	regmap_read(regmap, reg_pupd, &val);
	bit_pupd = !(val & BIT(spec_pupd_pin->pupd_bit));
	bit_r0 = !!(val & BIT(spec_pupd_pin->r0_bit));
	bit_r1 = !!(val & BIT(spec_pupd_pin->r1_bit));

	return (bit_pupd)|(bit_r0<<1)|(bit_r1<<2)|(1<<3);
}

static int mtk_pconf_set_pull_select(struct mtk_pinctrl *pctl,
		unsigned int pin, bool enable, bool isup, unsigned int arg)
{
	unsigned int bit;
	unsigned int reg_pullen, reg_pullsel, r1r0;
	int ret;

	/* Some pins' pull setting are very different,
	 * they have separate pull up/down bit, R0 and R1
	 * resistor bit, so we need this special handle.
	 */
#if defined(CONFIG_PINCTRL_MTK_ALTERNATIVE)
	if (pctl->devdata->mtk_pctl_set_pull_sel)
		return pctl->devdata->mtk_pctl_set_pull_sel(pctl, pin,
			enable, isup, arg);
#endif

	if (pctl->devdata->spec_pull_set) {
		/* For special pins, bias-disable is set by R1R0,
		 * the parameter should be "MTK_PUPD_SET_R1R0_00".
		 */
		r1r0 = enable ? arg : MTK_PUPD_SET_R1R0_00;
		ret = pctl->devdata->spec_pull_set(pctl,
			mtk_get_regmap(pctl, pin), pin,
			pctl->devdata->port_align, isup, r1r0);
		if (!ret)
			return 0;
	}

	if (pctl->devdata->pin_pullen_grps ||
		pctl->devdata->pin_pullsel_grps) {
		mtk_pinctrl_set_gpio_value(pctl, pin, enable,
			pctl->devdata->n_pin_pullen,
			pctl->devdata->pin_pullen_grps);
		mtk_pinctrl_set_gpio_value(pctl, pin, isup,
			pctl->devdata->n_pin_pullsel,
			pctl->devdata->pin_pullsel_grps);
		return 0;
	}

	bit = BIT(pin & pctl->devdata->port_mask);
	if (enable)
		reg_pullen = SET_ADDR(mtk_get_port(pctl, pin) +
			pctl->devdata->pullen_offset, pctl);
	else
		reg_pullen = CLR_ADDR(mtk_get_port(pctl, pin) +
			pctl->devdata->pullen_offset, pctl);

	if (isup)
		reg_pullsel = SET_ADDR(mtk_get_port(pctl, pin) +
			pctl->devdata->pullsel_offset, pctl);
	else
		reg_pullsel = CLR_ADDR(mtk_get_port(pctl, pin) +
			pctl->devdata->pullsel_offset, pctl);

	regmap_write(mtk_get_regmap(pctl, pin), reg_pullen, bit);
	regmap_write(mtk_get_regmap(pctl, pin), reg_pullsel, bit);
	return 0;
}

static int mtk_pconf_parse_conf(struct pinctrl_dev *pctldev,
		unsigned int pin, enum pin_config_param param,
		enum pin_config_param arg)
{
	int ret = 0;
	struct mtk_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	switch (param) {
	case PIN_CONFIG_BIAS_DISABLE:
		ret = mtk_pconf_set_pull_select(pctl, pin, false, false, arg);
		break;
	case PIN_CONFIG_BIAS_PULL_UP:
		ret = mtk_pconf_set_pull_select(pctl, pin, true, true, arg);
		break;
	case PIN_CONFIG_BIAS_PULL_DOWN:
		ret = mtk_pconf_set_pull_select(pctl, pin, true, false, arg);
		break;
	case PIN_CONFIG_INPUT_ENABLE:
		mtk_pmx_gpio_set_direction(pctldev, NULL, pin, true);
		ret = mtk_pconf_set_ies_smt(pctl, pin, arg, param);
		break;
	case PIN_CONFIG_OUTPUT:
		mtk_gpio_set(pctl->chip, pin, arg);
		ret = mtk_pmx_gpio_set_direction(pctldev, NULL, pin, false);
		break;
	case PIN_CONFIG_INPUT_SCHMITT_ENABLE:
		mtk_pmx_gpio_set_direction(pctldev, NULL, pin, true);
		ret = mtk_pconf_set_ies_smt(pctl, pin, arg, param);
		break;
	case PIN_CONFIG_DRIVE_STRENGTH:
		ret = mtk_pconf_set_driving(pctl, pin, arg);
		break;
	case PIN_CONFIG_SLEW_RATE:
		mtk_pconf_set_direction(pctl, pin, arg, param);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int mtk_pconf_group_get(struct pinctrl_dev *pctldev,
				 unsigned group,
				 unsigned long *config)
{
	struct mtk_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	*config = pctl->groups[group].config;

	return 0;
}

static int mtk_pconf_group_set(struct pinctrl_dev *pctldev, unsigned group,
				 unsigned long *configs, unsigned num_configs)
{
	struct mtk_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	struct mtk_pinctrl_group *g = &pctl->groups[group];
	int i, ret;

	for (i = 0; i < num_configs; i++) {
		ret = mtk_pconf_parse_conf(pctldev, g->pin,
			pinconf_to_config_param(configs[i]),
			pinconf_to_config_argument(configs[i]));
		if (ret < 0)
			return ret;

		g->config = configs[i];
	}

	return 0;
}

static const struct pinconf_ops mtk_pconf_ops = {
	.pin_config_group_get	= mtk_pconf_group_get,
	.pin_config_group_set	= mtk_pconf_group_set,
};

static struct mtk_pinctrl_group *
mtk_pctrl_find_group_by_pin(struct mtk_pinctrl *pctl, u32 pin)
{
	int i;

	for (i = 0; i < pctl->ngroups; i++) {
		struct mtk_pinctrl_group *grp = pctl->groups + i;

		if (grp->pin == pin)
			return grp;
	}

	return NULL;
}

static const struct mtk_desc_function *mtk_pctrl_find_function_by_pin(
		struct mtk_pinctrl *pctl, u32 pin_num, u32 fnum)
{
	const struct mtk_desc_pin *pin = pctl->devdata->pins + pin_num;
	const struct mtk_desc_function *func = pin->functions;

	while (func && func->name) {
		if (func->muxval == fnum)
			return func;
		func++;
	}

	return NULL;
}

static bool mtk_pctrl_is_function_valid(struct mtk_pinctrl *pctl,
		u32 pin_num, u32 fnum)
{
	int i;

	for (i = 0; i < pctl->devdata->npins; i++) {
		const struct mtk_desc_pin *pin = pctl->devdata->pins + i;

		if (pin->pin.number == pin_num) {
			const struct mtk_desc_function *func =
					pin->functions;

			while (func && func->name) {
				if (func->muxval == fnum)
					return true;
				func++;
			}

			break;
		}
	}

	return false;
}

static int mtk_pctrl_dt_node_to_map_func(struct mtk_pinctrl *pctl,
		u32 pin, u32 fnum, struct mtk_pinctrl_group *grp,
		struct pinctrl_map **map, unsigned *reserved_maps,
		unsigned *num_maps)
{
	bool ret;

	if (*num_maps == *reserved_maps)
		return -ENOSPC;

	(*map)[*num_maps].type = PIN_MAP_TYPE_MUX_GROUP;
	(*map)[*num_maps].data.mux.group = grp->name;

	ret = mtk_pctrl_is_function_valid(pctl, pin, fnum);
	if (!ret) {
		dev_err(pctl->dev, "invalid function %d on pin %d .\n",
				fnum, pin);
		return -EINVAL;
	}

	(*map)[*num_maps].data.mux.function = mtk_gpio_functions[fnum];
	(*num_maps)++;

	return 0;
}

static int mtk_pctrl_dt_subnode_to_map(struct pinctrl_dev *pctldev,
				      struct device_node *node,
				      struct pinctrl_map **map,
				      unsigned *reserved_maps,
				      unsigned *num_maps)
{
	struct property *pins;
	u32 pinfunc, pin, func;
	int num_pins, num_funcs, maps_per_pin;
	unsigned long *configs;
	unsigned int num_configs;
	bool has_config = 0;
	int i, err;
	unsigned reserve = 0;
	struct mtk_pinctrl_group *grp;
	struct mtk_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	pins = of_find_property(node, "pinmux", NULL);
	if (!pins) {
		dev_err(pctl->dev, "missing pins property in node %s .\n",
				node->name);
		return -EINVAL;
	}

	err = pinconf_generic_parse_dt_config(node, pctldev, &configs,
		&num_configs);
	if (err)
		return err;

	if (num_configs)
		has_config = 1;

	num_pins = pins->length / sizeof(u32);
	num_funcs = num_pins;
	maps_per_pin = 0;
	if (num_funcs)
		maps_per_pin++;
	if (has_config && num_pins >= 1)
		maps_per_pin++;

	if (!num_pins || !maps_per_pin) {
		err = -EINVAL;
		goto exit;
	}

	reserve = num_pins * maps_per_pin;

	err = pinctrl_utils_reserve_map(pctldev, map,
			reserved_maps, num_maps, reserve);
	if (err < 0)
		goto exit;

	for (i = 0; i < num_pins; i++) {
		err = of_property_read_u32_index(node, "pinmux",
				i, &pinfunc);
		if (err)
			goto exit;

		pin = MTK_GET_PIN_NO(pinfunc);
		func = MTK_GET_PIN_FUNC(pinfunc);

		if (pin >= pctl->devdata->npins ||
				func >= ARRAY_SIZE(mtk_gpio_functions)) {
			dev_err(pctl->dev, "invalid pins value.\n");
			err = -EINVAL;
			goto exit;
		}

		grp = mtk_pctrl_find_group_by_pin(pctl, pin);
		if (!grp) {
			dev_err(pctl->dev, "unable to match pin %d to group\n",
					pin);
			err = -EINVAL;
			goto exit;
		}

		err = mtk_pctrl_dt_node_to_map_func(pctl, pin, func, grp, map,
				reserved_maps, num_maps);
		if (err < 0)
			goto exit;

		if (has_config) {
			err = pinctrl_utils_add_map_configs(pctldev, map,
					reserved_maps, num_maps, grp->name,
					configs, num_configs,
					PIN_MAP_TYPE_CONFIGS_GROUP);
			if (err < 0)
				goto exit;
		}
	}

	err = 0;

exit:
	kfree(configs);
	return err;
}

static int mtk_pctrl_dt_node_to_map(struct pinctrl_dev *pctldev,
				 struct device_node *np_config,
				 struct pinctrl_map **map, unsigned *num_maps)
{
	struct device_node *np;
	unsigned reserved_maps;
	int ret;

	*map = NULL;
	*num_maps = 0;
	reserved_maps = 0;

	for_each_child_of_node(np_config, np) {
		ret = mtk_pctrl_dt_subnode_to_map(pctldev, np, map,
				&reserved_maps, num_maps);
		if (ret < 0) {
			pinctrl_utils_free_map(pctldev, *map, *num_maps);
			of_node_put(np);
			return ret;
		}
	}

	return 0;
}

static int mtk_pctrl_get_groups_count(struct pinctrl_dev *pctldev)
{
	struct mtk_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	return pctl->ngroups;
}

static const char *mtk_pctrl_get_group_name(struct pinctrl_dev *pctldev,
					      unsigned group)
{
	struct mtk_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	return pctl->groups[group].name;
}

static int mtk_pctrl_get_group_pins(struct pinctrl_dev *pctldev,
				      unsigned group,
				      const unsigned **pins,
				      unsigned *num_pins)
{
	struct mtk_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	*pins = (unsigned *)&pctl->groups[group].pin;
	*num_pins = 1;

	return 0;
}

static const struct pinctrl_ops mtk_pctrl_ops = {
	.dt_node_to_map		= mtk_pctrl_dt_node_to_map,
	.dt_free_map		= pinctrl_utils_free_map,
	.get_groups_count	= mtk_pctrl_get_groups_count,
	.get_group_name		= mtk_pctrl_get_group_name,
	.get_group_pins		= mtk_pctrl_get_group_pins,
};

static int mtk_pmx_get_funcs_cnt(struct pinctrl_dev *pctldev)
{
	return ARRAY_SIZE(mtk_gpio_functions);
}

static const char *mtk_pmx_get_func_name(struct pinctrl_dev *pctldev,
					   unsigned selector)
{
	return mtk_gpio_functions[selector];
}

static int mtk_pmx_get_func_groups(struct pinctrl_dev *pctldev,
				     unsigned function,
				     const char * const **groups,
				     unsigned * const num_groups)
{
	struct mtk_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	*groups = pctl->grp_names;
	*num_groups = pctl->ngroups;

	return 0;
}

static int mtk_pmx_set_mode(struct pinctrl_dev *pctldev,
		unsigned long pin, unsigned long mode)
{
	unsigned int reg_addr;
	unsigned char bit;
	unsigned int val;
	unsigned int mask = (1L << GPIO_MODE_BITS) - 1;
	struct mtk_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

#if defined(CONFIG_PINCTRL_MTK_ALTERNATIVE)
	if (pctl->devdata->pin_mode_grps)
		return mtk_pinctrl_set_gpio_mode(pctl, pin, mode);
#endif

	if (pctl->devdata->spec_pinmux_set) {
		pctl->devdata->spec_pinmux_set(mtk_get_regmap(pctl, pin),
					pin, mode);
		return 0;
	}

	reg_addr = ((pin / MAX_GPIO_MODE_PER_REG) << pctl->devdata->port_shf)
			+ pctl->devdata->pinmux_offset;

	mode &= mask;
	bit = pin % MAX_GPIO_MODE_PER_REG;
	mask <<= (GPIO_MODE_BITS * bit);
	val = (mode << (GPIO_MODE_BITS * bit));
	return regmap_update_bits(mtk_get_regmap(pctl, pin),
			reg_addr, mask, val);
}

static const struct mtk_desc_pin *
mtk_find_pin_by_eint_num(struct mtk_pinctrl *pctl, unsigned int eint_num)
{
	int i;
	const struct mtk_desc_pin *pin;

	for (i = 0; i < pctl->devdata->npins; i++) {
		pin = pctl->devdata->pins + i;
		if (pin->eint.eintnum == eint_num)
			return pin;
	}

	return NULL;
}

static int mtk_pmx_set_mux(struct pinctrl_dev *pctldev,
			    unsigned function,
			    unsigned group)
{
	bool ret;
	const struct mtk_desc_function *desc;
	struct mtk_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);
	struct mtk_pinctrl_group *g = pctl->groups + group;

	ret = mtk_pctrl_is_function_valid(pctl, g->pin, function);
	if (!ret) {
		dev_err(pctl->dev, "invalid function %d on group %d .\n",
				function, group);
		return -EINVAL;
	}

	desc = mtk_pctrl_find_function_by_pin(pctl, g->pin, function);
	if (!desc)
		return -EINVAL;
	mtk_pmx_set_mode(pctldev, g->pin, desc->muxval);
	return 0;
}

static int mtk_pmx_find_gpio_mode(struct mtk_pinctrl *pctl,
				unsigned offset)
{
	const struct mtk_desc_pin *pin = pctl->devdata->pins + offset;
	const struct mtk_desc_function *func = pin->functions;

	while (func && func->name) {
		if (!strncmp(func->name, GPIO_MODE_PREFIX,
			sizeof(GPIO_MODE_PREFIX)-1))
			return func->muxval;
		func++;
	}
	return -EINVAL;
}

static int mtk_pmx_gpio_request_enable(struct pinctrl_dev *pctldev,
				    struct pinctrl_gpio_range *range,
				    unsigned offset)
{
	int muxval;
	struct mtk_pinctrl *pctl = pinctrl_dev_get_drvdata(pctldev);

	muxval = mtk_pmx_find_gpio_mode(pctl, offset);

	if (muxval < 0) {
		dev_err(pctl->dev, "invalid gpio pin %d.\n", offset);
		return -EINVAL;
	}

	mtk_pmx_set_mode(pctldev, offset, muxval);
	mtk_pconf_set_ies_smt(pctl, offset, 1, PIN_CONFIG_INPUT_ENABLE);

	return 0;
}

static const struct pinmux_ops mtk_pmx_ops = {
	.get_functions_count	= mtk_pmx_get_funcs_cnt,
	.get_function_name	= mtk_pmx_get_func_name,
	.get_function_groups	= mtk_pmx_get_func_groups,
	.set_mux		= mtk_pmx_set_mux,
	.gpio_set_direction	= mtk_pmx_gpio_set_direction,
	.gpio_request_enable	= mtk_pmx_gpio_request_enable,
};

static int mtk_gpio_direction_input(struct gpio_chip *chip,
					unsigned offset)
{
	return pinctrl_gpio_direction_input(chip->base + offset);
}

static int mtk_gpio_direction_output(struct gpio_chip *chip,
					unsigned offset, int value)
{
	mtk_gpio_set(chip, offset, value);
	return pinctrl_gpio_direction_output(chip->base + offset);
}

static int mtk_gpio_get_direction(struct gpio_chip *chip, unsigned offset)
{
	unsigned int reg_addr;
	unsigned int bit;
	unsigned int read_val = 0;

	struct mtk_pinctrl *pctl = gpiochip_get_data(chip);

#if defined(CONFIG_PINCTRL_MTK_ALTERNATIVE)
	const struct mtk_pin_info *spec_pin_info;

	if (pctl->devdata->pin_dir_grps) {
		spec_pin_info = mtk_pinctrl_get_gpio_array(offset,
			pctl->devdata->n_pin_dir, pctl->devdata->pin_dir_grps);
		if (spec_pin_info == NULL)
			return 1;/* for virtual GPIO that return 1 */
		/* need reverse the direction for gpiolib */
		return !mtk_pinctrl_get_gpio_direction(pctl, offset);
	}
	pr_info("pinctrl direction array is NULL of phone\n");
#endif

	reg_addr =  mtk_get_port(pctl, offset) + pctl->devdata->dir_offset;
	bit = BIT(offset & 0xf);

	if (pctl->devdata->spec_dir_get)
		pctl->devdata->spec_dir_get(pctl, &reg_addr, offset, &read_val);

	regmap_read(pctl->regmap1, reg_addr, &read_val);
	return !(read_val & bit);
}

static int mtk_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	unsigned int reg_addr;
	unsigned int bit;
	unsigned int read_val = 0;
	struct mtk_pinctrl *pctl = gpiochip_get_data(chip);

#if defined(CONFIG_PINCTRL_MTK_ALTERNATIVE)
	if (pctl->devdata->pin_din_grps)
		return mtk_pinctrl_get_gpio_input(pctl, offset);
#endif

	reg_addr = mtk_get_port(pctl, offset) +
		pctl->devdata->din_offset;

	bit = BIT(offset & 0xf);
	regmap_read(pctl->regmap1, reg_addr, &read_val);
	return !!(read_val & bit);
}

static int mtk_gpio_to_irq(struct gpio_chip *chip, unsigned offset)
{
	const struct mtk_desc_pin *pin;
	struct mtk_pinctrl *pctl = gpiochip_get_data(chip);
	struct mtk_pinctrl_group *g = pctl->groups + offset;
	int irq;

	pin = pctl->devdata->pins + offset;
	if (pin->eint.eintnum == NO_EINT_SUPPORT)
		return -EINVAL;

	mtk_pmx_set_mode(pctl->pctl_dev, g->pin, pin->functions->muxval);
	irq = irq_find_mapping(pctl->domain, pin->eint.eintnum);
	if (!irq)
		return -EINVAL;

	return irq;
}

static int mtk_pinctrl_irq_request_resources(struct irq_data *d)
{
	struct mtk_pinctrl *pctl = irq_data_get_irq_chip_data(d);
	const struct mtk_desc_pin *pin;
	int ret;

	pin = mtk_find_pin_by_eint_num(pctl, d->hwirq);

	if (!pin) {
		dev_err(pctl->dev, "Can not find pin\n");
		return -EINVAL;
	}

	ret = gpiochip_lock_as_irq(pctl->chip, pin->pin.number);
	if (ret) {
		dev_err(pctl->dev, "unable to lock HW IRQ %lu for IRQ\n",
			irqd_to_hwirq(d));
		return ret;
	}
	pr_debug("%s eint %d, mode%d\n", __func__,
		pin->eint.eintnum, pin->eint.eintmux);

	/* set mux to INT mode */
	mtk_pmx_set_mode(pctl->pctl_dev, pin->pin.number, pin->eint.eintmux);
	/* set gpio direction to input */
	mtk_pmx_gpio_set_direction(pctl->pctl_dev, NULL, pin->pin.number, true);
	/* set input-enable */
	mtk_pconf_set_ies_smt(pctl, pin->pin.number, 1,
		PIN_CONFIG_INPUT_ENABLE);

	return 0;
}

static void mtk_pinctrl_irq_release_resources(struct irq_data *d)
{
	struct mtk_pinctrl *pctl = irq_data_get_irq_chip_data(d);
	const struct mtk_desc_pin *pin;

	pin = mtk_find_pin_by_eint_num(pctl, d->hwirq);

	if (!pin) {
		dev_err(pctl->dev, "Can not find pin\n");
		return;
	}

	gpiochip_unlock_as_irq(pctl->chip, pin->pin.number);
}

static void __iomem *mtk_eint_get_offset(struct mtk_pinctrl *pctl,
	unsigned int eint_num, unsigned int offset)
{
	unsigned int eint_base = 0;
	void __iomem *reg;

	if (eint_num >= pctl->devdata->ap_num)
		eint_base = pctl->devdata->ap_num;

	reg = pctl->eint_reg_base + offset + ((eint_num - eint_base) / 32) * 4;

	return reg;
}

/*
 * mtk_can_en_debounce: Check the EINT number is able to enable debounce or not
 * @eint_num: the EINT number to setmtk_pinctrl
 */
static unsigned int mtk_eint_can_en_debounce(struct mtk_pinctrl *pctl,
	unsigned int eint_num)
{
	unsigned int sens;
	unsigned int bit = BIT(eint_num % 32);
	const struct mtk_eint_offsets *eint_offsets =
		&pctl->devdata->eint_offsets;

	void __iomem *reg = mtk_eint_get_offset(pctl, eint_num,
			eint_offsets->sens);

	if (readl(reg) & bit)
		sens = MT_LEVEL_SENSITIVE;
	else
		sens = MT_EDGE_SENSITIVE;

	if ((eint_num < pctl->devdata->db_cnt) && (sens != MT_EDGE_SENSITIVE))
		return 1;
	else
		return 0;
}

/*
 * mtk_eint_get_mask: To get the eint mask
 * @eint_num: the EINT number to get
 */
static unsigned int mtk_eint_get_mask(struct mtk_pinctrl *pctl,
	unsigned int eint_num)
{
	unsigned int bit = BIT(eint_num % 32);
	const struct mtk_eint_offsets *eint_offsets =
		&pctl->devdata->eint_offsets;

	void __iomem *reg = mtk_eint_get_offset(pctl, eint_num,
			eint_offsets->mask);

	return !!(readl(reg) & bit);
}

static int mtk_eint_flip_edge(struct mtk_pinctrl *pctl, int hwirq)
{
	int start_level, curr_level;
	unsigned int reg_offset;
	const struct mtk_eint_offsets *eint_offsets =
		&(pctl->devdata->eint_offsets);
	u32 mask = BIT(hwirq & 0x1f);
	u32 port = (hwirq >> 5) & eint_offsets->port_mask;
	void __iomem *reg = pctl->eint_reg_base + (port << 2);
	const struct mtk_desc_pin *pin;

	pin = mtk_find_pin_by_eint_num(pctl, hwirq);
	curr_level = mtk_gpio_get(pctl->chip, pin->pin.number);
	do {
		start_level = curr_level;
		if (start_level)
			reg_offset = eint_offsets->pol_clr;
		else
			reg_offset = eint_offsets->pol_set;
		writel(mask, reg + reg_offset);

		curr_level = mtk_gpio_get(pctl->chip, pin->pin.number);
	} while (start_level != curr_level);

	return start_level;
}

static void mtk_eint_mask(struct irq_data *d)
{
	struct mtk_pinctrl *pctl = irq_data_get_irq_chip_data(d);
	const struct mtk_eint_offsets *eint_offsets =
			&pctl->devdata->eint_offsets;
	u32 mask = BIT(d->hwirq & 0x1f);
	void __iomem *reg = mtk_eint_get_offset(pctl, d->hwirq,
			eint_offsets->mask_set);

	writel(mask, reg);
}

static void mtk_eint_unmask(struct irq_data *d)
{
	struct mtk_pinctrl *pctl = irq_data_get_irq_chip_data(d);
	const struct mtk_eint_offsets *eint_offsets =
		&pctl->devdata->eint_offsets;
	u32 mask = BIT(d->hwirq & 0x1f);
	void __iomem *reg = mtk_eint_get_offset(pctl, d->hwirq,
			eint_offsets->mask_clr);

	writel(mask, reg);

	if (pctl->eint_dual_edges[d->hwirq])
		mtk_eint_flip_edge(pctl, d->hwirq);
}

unsigned int mtk_gpio_debounce_select(const unsigned int *dbnc_infos,
	int dbnc_infos_num, unsigned int debounce)
{
	unsigned int i;
	unsigned int dbnc = dbnc_infos_num;

	for (i = 0; i < dbnc_infos_num; i++) {
		if (debounce <= dbnc_infos[i]) {
			dbnc = i;
			break;
		}
	}
	return dbnc;
}

static void mtk_eint_set_sw_debounce(struct irq_data *d,
				     struct mtk_pinctrl *pctl,
				     unsigned int debounce)
{
	unsigned int eint = d->hwirq;

	pctl->eint_sw_debounce_en[eint] = 1;
	pctl->eint_sw_debounce[eint] = debounce;
}

static int mtk_gpio_set_debounce(struct gpio_chip *chip, unsigned int offset,
	unsigned int debounce)
{
	struct mtk_pinctrl *pctl = gpiochip_get_data(chip);
	int eint_num, virq, eint_offset;
	unsigned int set_offset, bit, clr_bit, clr_offset, rst, unmask, dbnc;
	static const unsigned int debounce_time[] = {500, 1000, 16000,
		32000, 64000, 128000, 256000};
	const struct mtk_desc_pin *pin;
	struct irq_data *d;

	pin = pctl->devdata->pins + offset;
	if (pin->eint.eintnum == NO_EINT_SUPPORT)
		return -EINVAL;

	eint_num = pin->eint.eintnum;
	virq = irq_find_mapping(pctl->domain, eint_num);
	eint_offset = (eint_num % 4) * 8;
	d = irq_get_irq_data(virq);

	set_offset = (eint_num / 4) * 4 + pctl->devdata->eint_offsets.dbnc_set;
	clr_offset = (eint_num / 4) * 4 + pctl->devdata->eint_offsets.dbnc_clr;
	if (!mtk_eint_can_en_debounce(pctl, eint_num))
		return -EINVAL;

	if (pctl->devdata->spec_debounce_select)
		dbnc = pctl->devdata->spec_debounce_select(debounce);
	else
		dbnc = mtk_gpio_debounce_select(debounce_time,
			ARRAY_SIZE(debounce_time), debounce);

	if (!mtk_eint_get_mask(pctl, eint_num)) {
		mtk_eint_mask(d);
		unmask = 1;
	} else {
		unmask = 0;
	}

	clr_bit = 0xff << eint_offset;
	writel(clr_bit, pctl->eint_reg_base + clr_offset);

	bit = ((dbnc << EINT_DBNC_SET_DBNC_BITS) | EINT_DBNC_SET_EN) <<
		eint_offset;
	rst = EINT_DBNC_RST_BIT << eint_offset;
	writel(rst | bit, pctl->eint_reg_base + set_offset);

	/* Delay a while (more than 2T) to wait for hw debounce */
	/* counter reset work correctly */
	udelay(100);
	if (unmask == 1)
		mtk_eint_unmask(d);

	if (d->hwirq >= pctl->devdata->db_cnt)
		mtk_eint_set_sw_debounce(d, pctl, debounce);

	return 0;
}

static int mtk_gpio_set_config(struct gpio_chip *chip, unsigned offset,
			       unsigned long config)
{
	u32 debounce;

	if (pinconf_to_config_param(config) != PIN_CONFIG_INPUT_DEBOUNCE)
		return -ENOTSUPP;

	debounce = pinconf_to_config_argument(config);
	return mtk_gpio_set_debounce(chip, offset, debounce);
}

static const struct gpio_chip mtk_gpio_chip = {
	.owner			= THIS_MODULE,
	.request		= gpiochip_generic_request,
	.free			= gpiochip_generic_free,
	.get_direction		= mtk_gpio_get_direction,
	.direction_input	= mtk_gpio_direction_input,
	.direction_output	= mtk_gpio_direction_output,
	.get			= mtk_gpio_get,
	.set			= mtk_gpio_set,
	.to_irq			= mtk_gpio_to_irq,
	.set_config		= mtk_gpio_set_config,
	.of_gpio_n_cells	= 2,
};

static int mtk_eint_set_type(struct irq_data *d,
				      unsigned int type)

{
	struct mtk_pinctrl *pctl = irq_data_get_irq_chip_data(d);
	const struct mtk_eint_offsets *eint_offsets =
		&pctl->devdata->eint_offsets;
	u32 mask = BIT(d->hwirq & 0x1f);
	void __iomem *reg;

	if (((type & IRQ_TYPE_EDGE_BOTH) && (type & IRQ_TYPE_LEVEL_MASK)) ||
		((type & IRQ_TYPE_LEVEL_MASK) == IRQ_TYPE_LEVEL_MASK)) {
		pr_info("[GPIO]Can't config IRQ%d (EINT%lu) for type 0x%X\n",
			d->irq, d->hwirq, type);
		return -EINVAL;
	}

	if ((type & IRQ_TYPE_EDGE_BOTH) == IRQ_TYPE_EDGE_BOTH)
		pctl->eint_dual_edges[d->hwirq] = 1;
	else
		pctl->eint_dual_edges[d->hwirq] = 0;

	if (type & (IRQ_TYPE_LEVEL_LOW | IRQ_TYPE_EDGE_FALLING)) {
		reg = mtk_eint_get_offset(pctl, d->hwirq,
			eint_offsets->pol_clr);
		writel(mask, reg);
	} else {
		reg = mtk_eint_get_offset(pctl, d->hwirq,
			eint_offsets->pol_set);
		writel(mask, reg);
	}

	if (type & (IRQ_TYPE_EDGE_RISING | IRQ_TYPE_EDGE_FALLING)) {
		reg = mtk_eint_get_offset(pctl, d->hwirq,
			eint_offsets->sens_clr);
		writel(mask, reg);
	} else {
		reg = mtk_eint_get_offset(pctl, d->hwirq,
			eint_offsets->sens_set);
		writel(mask, reg);
	}

	if (pctl->eint_dual_edges[d->hwirq])
		mtk_eint_flip_edge(pctl, d->hwirq);

	return 0;
}

static int mtk_eint_irq_set_wake(struct irq_data *d, unsigned int on)
{
	struct mtk_pinctrl *pctl = irq_data_get_irq_chip_data(d);
	int shift = d->hwirq & 0x1f;
	int reg = d->hwirq >> 5;

	if (on)
		pctl->wake_mask[reg] |= BIT(shift);
	else
		pctl->wake_mask[reg] &= ~BIT(shift);

	return 0;
}

static void mtk_eint_chip_write_mask(const struct mtk_eint_offsets *chip,
		void __iomem *eint_reg_base, u32 *buf)
{
	int port;
	void __iomem *reg;

	for (port = 0; port < chip->ports; port++) {
		reg = eint_reg_base + (port << 2);
		writel_relaxed(~buf[port], reg + chip->mask_set);
		writel_relaxed(buf[port], reg + chip->mask_clr);
	}
}

static void mtk_eint_chip_read_mask(const struct mtk_eint_offsets *chip,
		void __iomem *eint_reg_base, u32 *buf)
{
	int port;
	void __iomem *reg;

	for (port = 0; port < chip->ports; port++) {
		reg = eint_reg_base + chip->mask + (port << 2);
		buf[port] = ~readl_relaxed(reg);
		/* Mask is 0 when irq is enabled, and 1 when disabled. */
	}
}

static int mtk_eint_suspend(struct device *device)
{
	void __iomem *reg;
	struct mtk_pinctrl *pctl = dev_get_drvdata(device);
	const struct mtk_eint_offsets *eint_offsets =
			&pctl->devdata->eint_offsets;

	reg = pctl->eint_reg_base;
	mtk_eint_chip_read_mask(eint_offsets, reg, pctl->cur_mask);
	mtk_eint_chip_write_mask(eint_offsets, reg, pctl->wake_mask);

	return 0;
}

static int mtk_eint_resume(struct device *device)
{
	struct mtk_pinctrl *pctl = dev_get_drvdata(device);
	const struct mtk_eint_offsets *eint_offsets =
			&pctl->devdata->eint_offsets;

	mtk_eint_chip_write_mask(eint_offsets,
			pctl->eint_reg_base, pctl->cur_mask);

	return 0;
}

const struct dev_pm_ops mtk_eint_pm_ops = {
	.suspend_noirq = mtk_eint_suspend,
	.resume_noirq = mtk_eint_resume,
};

static void mtk_eint_ack(struct irq_data *d)
{
	struct mtk_pinctrl *pctl = irq_data_get_irq_chip_data(d);
	const struct mtk_eint_offsets *eint_offsets =
		&pctl->devdata->eint_offsets;
	u32 mask = BIT(d->hwirq & 0x1f);
	void __iomem *reg = mtk_eint_get_offset(pctl, d->hwirq,
			eint_offsets->ack);

	writel(mask, reg);
}
static void mtk_eint_mask_ack(struct irq_data *d)
{
	mtk_eint_mask(d);
	mtk_eint_ack(d);
}

static struct irq_chip mtk_pinctrl_irq_chip = {
	.name = "mt-eint",
	.irq_disable = mtk_eint_mask,
	.irq_mask = mtk_eint_mask,
	.irq_unmask = mtk_eint_unmask,
	.irq_ack = mtk_eint_ack,
	.irq_mask_ack = mtk_eint_mask_ack,
	.irq_set_type = mtk_eint_set_type,
	.irq_set_wake = mtk_eint_irq_set_wake,
	.irq_request_resources = mtk_pinctrl_irq_request_resources,
	.irq_release_resources = mtk_pinctrl_irq_release_resources,
};

static ssize_t mtk_eint_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int i, j, irq, len, bufLen;
	u32 mask, msk, sen, pol, val;
	void __iomem *reg;
	struct irq_desc *desc;
	struct irqaction	*action;
	const struct mtk_desc_pin *pin;
	char * const irq_type[] = {"NONE", "R", "F", "RF", "H", "RH", "FH",
		"RFH", "L", "RL", "FL", "RFL", "HL", "RHL", "FHL", "RFHL"};
	struct mtk_pinctrl *pctl = dev_get_drvdata(dev);
	const struct mtk_eint_offsets *eint_offsets =
			&pctl->devdata->eint_offsets;

	len = 0;
	bufLen = PAGE_SIZE;

	len += snprintf(buf+len, bufLen-len,
		"eint\tpin\tirq\tdis\tdep\ttrig\twake\tmask\tsen\tpol\tisr\n");
	for (i = 0; i < pctl->devdata->ap_num; i++) {
		int shift = i & 0x1f;
		int offset = i >> 5;

		irq = irq_find_mapping(pctl->domain, i);
		if (!irq)
			continue;

		desc = irq_to_desc(irq);
		if (!desc || !irqd_get_trigger_type(&desc->irq_data))
			continue;
		action = desc->action;

		mask = BIT(i & 0x1f);
		reg = mtk_eint_get_offset(pctl, i, eint_offsets->mask);
		val = readl(reg);
		msk = val & mask;

		reg = mtk_eint_get_offset(pctl, i, eint_offsets->sens);
		val = readl(reg);
		sen = val & mask;

		reg = mtk_eint_get_offset(pctl, i, eint_offsets->pol);
		val = readl(reg);
		pol = val & mask;

		for (j = 0; j < pctl->devdata->npins; j++) {
			pin = pctl->devdata->pins + j;
			if (pin->eint.eintnum == i)
				break;
		}
		len += snprintf(buf+len, bufLen-len,
		"%3d\t%3d\t%3d\t%3d\t%3d\t%4s\t%4d\t%4d\t%3d\t%3d\t%s\n",
			i, j, irq,
			irqd_irq_disabled(&desc->irq_data) ? 1 : 0,
			desc->depth,
			irq_type[irqd_get_trigger_type(&desc->irq_data)],
			(pctl->wake_mask[offset] & BIT(shift)) ? 1 : 0,
			msk ? 1 : 0,
			sen ? 1 : 0,
			pol ? 1 : 0,
		action ? (action->name ? action->name : "NONE") : "NONE");
	}

	return len;
}

static ssize_t mtk_eint_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	int eint, irq;
	struct irq_desc *desc;
	struct mtk_pinctrl *pctl = dev_get_drvdata(dev);
	const struct mtk_eint_offsets *eint_offsets =
		&pctl->devdata->eint_offsets;
	u32 mask;
	void __iomem *reg;

	if (!strncmp(buf, "mask", 4) && (sscanf(buf+4, "%d", &eint) == 1)) {
		irq = irq_find_mapping(pctl->domain, eint);
		desc = irq_to_desc(irq);
		if (!desc)
			return count;
		mtk_eint_mask(&desc->irq_data);
	} else if (!strncmp(buf, "unmask", 6)
		&& (sscanf(buf + 6, "%d", &eint) == 1)) {
		irq = irq_find_mapping(pctl->domain, eint);
		desc = irq_to_desc(irq);
		if (!desc)
			return count;
		mtk_eint_unmask(&desc->irq_data);
	} else if (!strncmp(buf, "trigger", 7)
		&& (sscanf(buf + 7, "%d", &eint) == 1)) {
		if (eint >= pctl->devdata->ap_num)
			return count;

		mask = BIT(eint & 0x1f);
		reg = mtk_eint_get_offset(pctl, eint, eint_offsets->soft_set);
		writel(mask, reg);
	}
	return count;
}

static DEVICE_ATTR(mtk_eint, 0664, mtk_eint_show, mtk_eint_store);

static struct device_attribute *eint_attr_list[] = {
	&dev_attr_mtk_eint,
};

static int mtk_eint_create_attr(struct device *dev)
{
	int idx, err = 0;
	int num = ARRAY_SIZE(eint_attr_list);

	if (!dev)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = device_create_file(dev, eint_attr_list[idx]);
		if (err)
			break;
	}

	return err;
}

static unsigned int mtk_eint_init(struct mtk_pinctrl *pctl)
{
	const struct mtk_eint_offsets *eint_offsets =
		&pctl->devdata->eint_offsets;
	void __iomem *reg = pctl->eint_reg_base + eint_offsets->dom_en;
	unsigned int i;

	for (i = 0; i < pctl->devdata->ap_num; i += 32) {
		writel(0xffffffff, reg);
		reg += 4;
	}
	return 0;
}

static inline void
mtk_eint_debounce_process(struct mtk_pinctrl *pctl, int index)
{
	unsigned int rst, ctrl_offset;
	unsigned int bit, dbnc;
	const struct mtk_eint_offsets *eint_offsets =
		&pctl->devdata->eint_offsets;

	ctrl_offset = (index / 4) * 4 + eint_offsets->dbnc_ctrl;
	dbnc = readl(pctl->eint_reg_base + ctrl_offset);
	bit = EINT_DBNC_SET_EN << ((index % 4) * 8);
	if ((bit & dbnc) > 0) {
		ctrl_offset = (index / 4) * 4 + eint_offsets->dbnc_set;
		rst = EINT_DBNC_RST_BIT << ((index % 4) * 8);
		writel(rst, pctl->eint_reg_base + ctrl_offset);
	}
}

/*
 * mt_eint_print_status: Print the EINT status register.
 */
void mt_eint_print_status(void)
{
#if defined(CONFIG_PINCTRL_MTK_ALTERNATIVE)
	unsigned int status, eint_num;
	unsigned int offset;
	const struct mtk_eint_offsets *eint_offsets =
		&pctl_alt->devdata->eint_offsets;
	void __iomem *reg_base =
		 mtk_eint_get_offset(pctl_alt, 0, eint_offsets->stat);
	unsigned int triggered_eint;

	pr_notice("EINT_STA:");
	for (eint_num = 0; eint_num < pctl_alt->devdata->ap_num;
		reg_base += 4, eint_num += 32) {
		/* read status register every 32 interrupts */
		status = readl(reg_base);
		if (status)
			pr_notice("EINT Module - addr:%p,EINT_STA = 0x%x\n",
				reg_base, status);
		else
			continue;

		while (status) {
			offset = __ffs(status);
			triggered_eint = eint_num + offset;
			pr_notice("EINT %d is pending\n", triggered_eint);
			status &= ~BIT(offset);
		}
	}
#endif
	pr_notice("\n");
}
EXPORT_SYMBOL(mt_eint_print_status);

static void mtk_eint_sw_debounce_end(unsigned long data)
{
	unsigned long flags;
	struct irq_data *d = (struct irq_data *)data;
	struct mtk_pinctrl *pctl = irq_data_get_irq_chip_data(d);
	const struct mtk_eint_offsets *eint_offsets =
					&pctl->devdata->eint_offsets;
	void __iomem *reg = mtk_eint_get_offset(pctl, d->hwirq,
						eint_offsets->stat);
	unsigned int status;

	local_irq_save(flags);

	mtk_eint_unmask(d);
	status = readl(reg) & (1 << (d->hwirq%32));
	if (status)
		generic_handle_irq(d->irq);

	local_irq_restore(flags);
}

static void mtk_eint_sw_debounce_start(struct mtk_pinctrl *pctl,
				       struct irq_data *d,
				       int index)
{
	struct timer_list *t = &pctl->eint_timers[index];
	u32 debounce = pctl->eint_sw_debounce[index];

	t->expires = jiffies + usecs_to_jiffies(debounce);
	t->data = (unsigned long)d;
	t->function = mtk_eint_sw_debounce_end;

	if (!timer_pending(t)) {
		init_timer(t);
		add_timer(t);
	}
}

static void mtk_eint_irq_handler(struct irq_desc *desc)
{
	struct irq_chip *chip = irq_desc_get_chip(desc);
	struct mtk_pinctrl *pctl = irq_desc_get_handler_data(desc);
	unsigned int status, eint_num;
	int offset, index, virq;
	const struct mtk_eint_offsets *eint_offsets =
		&pctl->devdata->eint_offsets;
	void __iomem *reg =  mtk_eint_get_offset(pctl, 0, eint_offsets->stat);
	int dual_edges, start_level, curr_level;
	const struct mtk_desc_pin *pin;

	chained_irq_enter(chip, desc);
	for (eint_num = 0;
	     eint_num < pctl->devdata->ap_num;
	     eint_num += 32, reg += 4) {
		status = readl(reg);
		while (status) {
			offset = __ffs(status);
			index = eint_num + offset;
			virq = irq_find_mapping(pctl->domain, index);
			status &= ~BIT(offset);

			dual_edges = pctl->eint_dual_edges[index];
			if (dual_edges) {
				/* Clear soft-irq in case we raised it
				 * last time
				 */
				writel(BIT(offset), reg - eint_offsets->stat +
					eint_offsets->soft_clr);

				pin = mtk_find_pin_by_eint_num(pctl, index);
				start_level = mtk_gpio_get(pctl->chip,
							   pin->pin.number);
			}
			if (pctl->eint_sw_debounce_en[index]) {
				mtk_eint_mask(irq_get_irq_data(virq));
				mtk_eint_sw_debounce_start(pctl,
						irq_get_irq_data(virq),
						index);
			} else
				generic_handle_irq(virq);

			if (dual_edges) {
				curr_level = mtk_eint_flip_edge(pctl, index);

				/* If level changed, we might lost one edge
				 * interrupt, raised it through soft-irq
				 */
				if (start_level != curr_level)
					writel(BIT(offset), reg -
						eint_offsets->stat +
						eint_offsets->soft_set);
			}

			if (index < pctl->devdata->db_cnt)
				mtk_eint_debounce_process(pctl, index);
		}
	}
	chained_irq_exit(chip, desc);
}

static int mtk_pctrl_build_state(struct platform_device *pdev)
{
	struct mtk_pinctrl *pctl = platform_get_drvdata(pdev);
	int i;

	pctl->ngroups = pctl->devdata->npins;

	/* Allocate groups */
	pctl->groups = devm_kcalloc(&pdev->dev, pctl->ngroups,
				    sizeof(*pctl->groups), GFP_KERNEL);
	if (!pctl->groups)
		return -ENOMEM;

	/* We assume that one pin is one group, use pin name as group name. */
	pctl->grp_names = devm_kcalloc(&pdev->dev, pctl->ngroups,
				       sizeof(*pctl->grp_names), GFP_KERNEL);
	if (!pctl->grp_names)
		return -ENOMEM;

	for (i = 0; i < pctl->devdata->npins; i++) {
		const struct mtk_desc_pin *pin = pctl->devdata->pins + i;
		struct mtk_pinctrl_group *group = pctl->groups + i;

		group->name = pin->pin.name;
		group->pin = pin->pin.number;

		pctl->grp_names[i] = pin->pin.name;
	}

	return 0;
}

#if defined(CONFIG_PINCTRL_MTK_ALTERNATIVE)
#include "pinctrl-mtk-common_debug.c"
#endif

int mtk_pctrl_init(struct platform_device *pdev,
		const struct mtk_pinctrl_devdata *data,
		struct regmap *regmap)
{
	struct pinctrl_pin_desc *pins;
	struct mtk_pinctrl *pctl;
	struct device_node *np = pdev->dev.of_node, *node;
	struct property *prop;
	struct resource *res;
	int i, ret, irq, ports_buf;

	pctl = devm_kzalloc(&pdev->dev, sizeof(*pctl), GFP_KERNEL);
	if (!pctl)
		return -ENOMEM;

	platform_set_drvdata(pdev, pctl);

	prop = of_find_property(np, "pins-are-numbered", NULL);
	if (!prop) {
		dev_err(&pdev->dev, "only support pins-are-numbered format\n");
		return -EINVAL;
	}

	node = of_parse_phandle(np, "mediatek,pctl-regmap", 0);
	if (node) {
		pctl->regmap1 = syscon_node_to_regmap(node);
		if (IS_ERR(pctl->regmap1))
			return PTR_ERR(pctl->regmap1);
	} else if (regmap) {
		pctl->regmap1  = regmap;
	} else {
		dev_err(&pdev->dev, "Pinctrl node has not register regmap.\n");
		return -EINVAL;
	}

	/* Only 8135 has two base addr, other SoCs have only one. */
	node = of_parse_phandle(np, "mediatek,pctl-regmap", 1);
	if (node) {
		pctl->regmap2 = syscon_node_to_regmap(node);
		if (IS_ERR(pctl->regmap2))
			return PTR_ERR(pctl->regmap2);
	}

	if (data->regmap_num > 2) {
		for (i = 0; i <= data->regmap_num; i++) {
			node = of_parse_phandle(np, "mediatek,pctl-regmap", i);
			if (node) {
				pctl->regmap[i] = syscon_node_to_regmap(node);
				if (IS_ERR(pctl->regmap[i]))
					return PTR_ERR(pctl->regmap[i]);
			}
		}
	}

	pctl->devdata = data;
	ret = mtk_pctrl_build_state(pdev);
	if (ret) {
		dev_err(&pdev->dev, "build state failed: %d\n", ret);
		return -EINVAL;
	}

	pins = devm_kcalloc(&pdev->dev, pctl->devdata->npins, sizeof(*pins),
			    GFP_KERNEL);
	if (!pins)
		return -ENOMEM;

	for (i = 0; i < pctl->devdata->npins; i++)
		pins[i] = pctl->devdata->pins[i].pin;

	pctl->pctl_desc.name = dev_name(&pdev->dev);
	pctl->pctl_desc.owner = THIS_MODULE;
	pctl->pctl_desc.pins = pins;
	pctl->pctl_desc.npins = pctl->devdata->npins;
	pctl->pctl_desc.confops = &mtk_pconf_ops;
	pctl->pctl_desc.pctlops = &mtk_pctrl_ops;
	pctl->pctl_desc.pmxops = &mtk_pmx_ops;
	pctl->dev = &pdev->dev;

	pctl->pctl_dev = devm_pinctrl_register(&pdev->dev, &pctl->pctl_desc,
					       pctl);
	if (IS_ERR(pctl->pctl_dev)) {
		dev_err(&pdev->dev, "couldn't register pinctrl driver\n");
		return PTR_ERR(pctl->pctl_dev);
	}

	pctl->chip = devm_kzalloc(&pdev->dev, sizeof(*pctl->chip), GFP_KERNEL);
	if (!pctl->chip) {
		ret = -ENOMEM;
		goto pctrl_error;
	}

	*pctl->chip = mtk_gpio_chip;
	pctl->chip->ngpio = pctl->devdata->npins;
	pctl->chip->label = dev_name(&pdev->dev);
	pctl->chip->parent = &pdev->dev;
	pctl->chip->base = -1;

	ret = gpiochip_add_data(pctl->chip, pctl);
	if (ret) {
		ret = -EINVAL;
		goto pctrl_error;
	}

	/* Register the GPIO to pin mappings. */
	ret = gpiochip_add_pin_range(pctl->chip, dev_name(&pdev->dev),
			0, 0, pctl->devdata->npins);
	if (ret) {
		ret = -EINVAL;
		goto chip_error;
	}

#if defined(CONFIG_PINCTRL_MTK_ALTERNATIVE)
	if (mtk_gpio_create_attr(&pdev->dev))
		pr_debug("[pinctrl]mtk_gpio create attribute error\n");
#endif

	if (!of_property_read_bool(np, "interrupt-controller")) {
		pr_debug("[pinctrl]init:interrupt-controller node no found\n");
		return 0;
	}

	/* Get EINT register base from dts. */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "Unable to get Pinctrl resource\n");
		ret = -EINVAL;
		goto chip_error;
	}

	pctl->eint_reg_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(pctl->eint_reg_base)) {
		ret = -EINVAL;
		goto chip_error;
	}

	ports_buf = pctl->devdata->eint_offsets.ports;
	pctl->wake_mask = devm_kcalloc(&pdev->dev, ports_buf,
					sizeof(*pctl->wake_mask), GFP_KERNEL);
	if (!pctl->wake_mask) {
		ret = -ENOMEM;
		goto chip_error;
	}

	pctl->cur_mask = devm_kcalloc(&pdev->dev, ports_buf,
					sizeof(*pctl->cur_mask), GFP_KERNEL);
	if (!pctl->cur_mask) {
		ret = -ENOMEM;
		goto chip_error;
	}

	pctl->eint_dual_edges = devm_kcalloc(&pdev->dev, pctl->devdata->ap_num,
					     sizeof(int), GFP_KERNEL);
	if (!pctl->eint_dual_edges) {
		ret = -ENOMEM;
		goto chip_error;
	}

	pctl->eint_timers = devm_kcalloc(&pdev->dev, pctl->devdata->ap_num,
					 sizeof(struct timer_list), GFP_KERNEL);
	if (!pctl->eint_timers) {
		ret = -ENOMEM;
		goto chip_error;
	}

	pctl->eint_sw_debounce_en = devm_kcalloc(&pdev->dev,
						 pctl->devdata->ap_num,
						 sizeof(int), GFP_KERNEL);
	if (!pctl->eint_sw_debounce_en) {
		ret = -ENOMEM;
		goto chip_error;
	}

	pctl->eint_sw_debounce = devm_kcalloc(&pdev->dev, pctl->devdata->ap_num,
					      sizeof(u32), GFP_KERNEL);
	if (!pctl->eint_sw_debounce) {
		ret = -ENOMEM;
		goto chip_error;
	}

	irq = irq_of_parse_and_map(np, 0);
	if (!irq) {
		dev_err(&pdev->dev, "couldn't parse and map irq\n");
		ret = -EINVAL;
		goto chip_error;
	}

	pctl->domain = irq_domain_add_linear(np,
		pctl->devdata->ap_num, &irq_domain_simple_ops, NULL);
#if defined(CONFIG_PINCTRL_MTK_ALTERNATIVE)
	if (pctl->devdata->mtk_irq_domain_ops) {
		pctl->domain = irq_domain_add_linear(np,
			pctl->devdata->ap_num,
				pctl->devdata->mtk_irq_domain_ops, NULL);
	}
#endif
	if (!pctl->domain) {
		dev_err(&pdev->dev, "Couldn't register IRQ domain\n");
		ret = -ENOMEM;
		goto chip_error;
	}

	mtk_eint_init(pctl);
	for (i = 0; i < pctl->devdata->ap_num; i++) {
		int virq = irq_create_mapping(pctl->domain, i);

		irq_set_chip_and_handler(virq, &mtk_pinctrl_irq_chip,
			handle_level_irq);
		irq_set_chip_data(virq, pctl);
	}

	irq_set_chained_handler_and_data(irq, mtk_eint_irq_handler, pctl);
	if (mtk_eint_create_attr(&pdev->dev))
		pr_warn("mtk_eint create attribute error\n");

	pctl_alt = pctl;
	pr_info("mtk pctrl init OK\n");
	return 0;

chip_error:
	gpiochip_remove(pctl->chip);
pctrl_error:
	pinctrl_unregister(pctl->pctl_dev);
	pr_err("mtk pctrl init Failed\n");
	return ret;
}

