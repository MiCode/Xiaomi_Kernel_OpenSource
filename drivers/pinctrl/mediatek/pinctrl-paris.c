// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek Pinctrl Paris Driver, which implement the vendor per-pin
 * bindings for MediaTek SoC.
 *
 * Copyright (C) 2018 MediaTek Inc.
 * Author: Sean Wang <sean.wang@mediatek.com>
 *	   Zhiyong Tao <zhiyong.tao@mediatek.com>
 *	   Hongzhou.Yang <hongzhou.yang@mediatek.com>
 */

#include <linux/gpio/driver.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <dt-bindings/pinctrl/mt65xx.h>
#include <linux/regmap.h>
#include "pinctrl-paris.h"

/* Custom pinconf parameters */
#define MTK_PIN_CONFIG_TDSEL	(PIN_CONFIG_END + 1)
#define MTK_PIN_CONFIG_RDSEL	(PIN_CONFIG_END + 2)
#define MTK_PIN_CONFIG_PU_ADV	(PIN_CONFIG_END + 3)
#define MTK_PIN_CONFIG_PD_ADV	(PIN_CONFIG_END + 4)
#define MTK_PIN_CONFIG_DRV_ADV	(PIN_CONFIG_END + 5)

static const struct pinconf_generic_params mtk_custom_bindings[] = {
	{"mediatek,tdsel",	MTK_PIN_CONFIG_TDSEL,		0},
	{"mediatek,rdsel",	MTK_PIN_CONFIG_RDSEL,		0},
	{"mediatek,pull-up-adv", MTK_PIN_CONFIG_PU_ADV,		1},
	{"mediatek,pull-down-adv", MTK_PIN_CONFIG_PD_ADV,	1},
	{"mediatek,drive-strength-adv", MTK_PIN_CONFIG_DRV_ADV,	2},
};

#ifdef CONFIG_DEBUG_FS
static const struct pin_config_item mtk_conf_items[] = {
	PCONFDUMP(MTK_PIN_CONFIG_TDSEL, "tdsel", NULL, true),
	PCONFDUMP(MTK_PIN_CONFIG_RDSEL, "rdsel", NULL, true),
	PCONFDUMP(MTK_PIN_CONFIG_PU_ADV, "pu-adv", NULL, true),
	PCONFDUMP(MTK_PIN_CONFIG_PD_ADV, "pd-adv", NULL, true),
	PCONFDUMP(MTK_PIN_CONFIG_DRV_ADV, "drive-strength-adv", NULL, true),
};
#endif

static const char * const mtk_gpio_functions[] = {
	"func0", "func1", "func2", "func3",
	"func4", "func5", "func6", "func7",
	"func8", "func9", "func10", "func11",
	"func12", "func13", "func14", "func15",
};

static int mtk_pinmux_gpio_request_enable(struct pinctrl_dev *pctldev,
					  struct pinctrl_gpio_range *range,
					  unsigned int pin)
{
	int err;
	struct mtk_pinctrl *hw = pinctrl_dev_get_drvdata(pctldev);
	const struct mtk_pin_desc *desc;

	desc = (const struct mtk_pin_desc *)&hw->soc->pins[pin];

	err = mtk_hw_set_value(hw, desc, PINCTRL_PIN_REG_MODE,
				hw->soc->gpio_m);
	if (err)
		return err;

	if (hw->soc->eh_pin_pinmux) {
		err = mtk_eh_ctrl(hw, desc, hw->soc->gpio_m);
		if (err)
			return err;
	}

	return 0;
}

static int mtk_pinmux_gpio_set_direction(struct pinctrl_dev *pctldev,
					 struct pinctrl_gpio_range *range,
					 unsigned int pin, bool input)
{
	struct mtk_pinctrl *hw = pinctrl_dev_get_drvdata(pctldev);
	const struct mtk_pin_desc *desc;

	desc = (const struct mtk_pin_desc *)&hw->soc->pins[pin];

	/* hardware would take 0 as input direction */
	return mtk_hw_set_value(hw, desc, PINCTRL_PIN_REG_DIR, !input);
}

static int mtk_pinconf_get(struct pinctrl_dev *pctldev,
			   unsigned int pin, unsigned long *config)
{
	struct mtk_pinctrl *hw = pinctrl_dev_get_drvdata(pctldev);
	u32 param = pinconf_to_config_param(*config);
	int err, reg, ret = 1;
	int pullup;
	const struct mtk_pin_desc *desc;

	if (pin >= hw->soc->npins) {
		err = -EINVAL;
		goto out;
	}
	desc = (const struct mtk_pin_desc *)&hw->soc->pins[pin];

	switch (param) {
	case PIN_CONFIG_BIAS_DISABLE:
	case PIN_CONFIG_BIAS_PULL_UP:
	case PIN_CONFIG_BIAS_PULL_DOWN:
		if (hw->soc->bias_get_combo) {
			err = hw->soc->bias_get_combo(hw, desc, &pullup, &ret);
			if (err)
				goto out;
			if (ret == MTK_PUPD_SET_R1R0_00)
				ret = MTK_DISABLE;
			if (param == PIN_CONFIG_BIAS_DISABLE) {
				if (ret != MTK_DISABLE)
					err = -EINVAL;
			} else if (param == PIN_CONFIG_BIAS_PULL_UP) {
				if (!pullup || ret == MTK_DISABLE)
					err = -EINVAL;
			} else if (param == PIN_CONFIG_BIAS_PULL_DOWN) {
				if (pullup || ret == MTK_DISABLE)
					err = -EINVAL;
			}
		} else {
			err = -ENOTSUPP;
		}
		break;
	case PIN_CONFIG_SLEW_RATE:
		err = mtk_hw_get_value(hw, desc, PINCTRL_PIN_REG_SR, &ret);
		break;
	case PIN_CONFIG_INPUT_ENABLE:
	case PIN_CONFIG_OUTPUT_ENABLE:
		err = mtk_hw_get_value(hw, desc, PINCTRL_PIN_REG_DIR, &ret);
		if (err)
			goto out;
		/*     CONFIG     Current direction return value
		 * -------------  ----------------- ----------------------
		 * OUTPUT_ENABLE       output       1 (= HW value)
		 *                     input        0 (= HW value)
		 * INPUT_ENABLE        output       0 (= reverse HW value)
		 *                     input        1 (= reverse HW value)
		 */
		if (param == PIN_CONFIG_INPUT_ENABLE)
			ret = !ret;

		break;
	case PIN_CONFIG_INPUT_SCHMITT_ENABLE:
		err = mtk_hw_get_value(hw, desc, PINCTRL_PIN_REG_DIR, &ret);
		if (err)
			goto out;
		/* return error when in output mode
		 * because schmitt trigger only work in input mode
		 */
		if (ret) {
			err = -EINVAL;
			goto out;
		}

		err = mtk_hw_get_value(hw, desc, PINCTRL_PIN_REG_SMT, &ret);

		break;
	case PIN_CONFIG_DRIVE_STRENGTH:
		if (hw->soc->capability_flags & FLAG_DRIVE_SET_RAW)
			err = mtk_hw_get_value(hw, desc, PINCTRL_PIN_REG_DRV,
				       &ret);
		else if (hw->soc->drive_get)
			err = hw->soc->drive_get(hw, desc, &ret);
		else
			err = -ENOTSUPP;

		break;
	case MTK_PIN_CONFIG_TDSEL:
	case MTK_PIN_CONFIG_RDSEL:
		reg = (param == MTK_PIN_CONFIG_TDSEL) ?
		       PINCTRL_PIN_REG_TDSEL : PINCTRL_PIN_REG_RDSEL;
		err = mtk_hw_get_value(hw, desc, reg, &ret);
		break;
	case MTK_PIN_CONFIG_PU_ADV:
	case MTK_PIN_CONFIG_PD_ADV:
		if (hw->soc->adv_pull_get) {
			pullup = param == MTK_PIN_CONFIG_PU_ADV;
			err = hw->soc->adv_pull_get(hw, desc, pullup, &ret);
		} else
			err = -ENOTSUPP;
		break;
	case MTK_PIN_CONFIG_DRV_ADV:
		if (hw->soc->adv_drive_get)
			err = hw->soc->adv_drive_get(hw, desc, &ret);
		else
			err = -ENOTSUPP;
		break;
	default:
		err = -ENOTSUPP;
	}

out:
	if (!err)
		*config = pinconf_to_config_packed(param, ret);

	return err;
}

static int mtk_pinconf_set(struct pinctrl_dev *pctldev, unsigned int pin,
			   enum pin_config_param param, u32 arg)
{
	struct mtk_pinctrl *hw = pinctrl_dev_get_drvdata(pctldev);
	const struct mtk_pin_desc *desc;
	int err = 0;
	u32 reg;

	if (pin >= hw->soc->npins) {
		err = -EINVAL;
		goto err;
	}
	desc = (const struct mtk_pin_desc *)&hw->soc->pins[pin];

	switch ((u32)param) {
	case PIN_CONFIG_BIAS_DISABLE:
		if (hw->soc->bias_set_combo)
			err = hw->soc->bias_set_combo(hw, desc, 0, MTK_DISABLE);
		else
			err = -ENOTSUPP;
		break;
	case PIN_CONFIG_BIAS_PULL_UP:
		if (hw->soc->bias_set_combo)
			err = hw->soc->bias_set_combo(hw, desc, 1, arg);
		else
			err = -ENOTSUPP;
		break;
	case PIN_CONFIG_BIAS_PULL_DOWN:
		if (hw->soc->bias_set_combo)
			err = hw->soc->bias_set_combo(hw, desc, 0, arg);
		else
			err = -ENOTSUPP;
		break;
	case PIN_CONFIG_OUTPUT_ENABLE:
		err = mtk_hw_set_value(hw, desc, PINCTRL_PIN_REG_SMT,
				       MTK_DISABLE);
		/* Keep set direction to consider the case that a GPIO pin
		 *  does not have SMT control
		 */
		if (err != -ENOTSUPP)
			goto err;

		err = mtk_hw_set_value(hw, desc, PINCTRL_PIN_REG_DIR,
				       MTK_OUTPUT);
		break;
	case PIN_CONFIG_INPUT_ENABLE:
		/* regard all non-zero value as enable */
		err = mtk_hw_set_value(hw, desc, PINCTRL_PIN_REG_IES, !!arg);
		if (err)
			goto err;

		err = mtk_hw_set_value(hw, desc, PINCTRL_PIN_REG_DIR,
				       MTK_INPUT);
		break;
	case PIN_CONFIG_SLEW_RATE:
		/* regard all non-zero value as enable */
		err = mtk_hw_set_value(hw, desc, PINCTRL_PIN_REG_SR, !!arg);
		break;
	case PIN_CONFIG_OUTPUT:
		err = mtk_hw_set_value(hw, desc, PINCTRL_PIN_REG_DIR,
				       MTK_OUTPUT);
		if (err)
			goto err;

		err = mtk_hw_set_value(hw, desc, PINCTRL_PIN_REG_DO,
				       arg);
		break;
	case PIN_CONFIG_INPUT_SCHMITT:
	case PIN_CONFIG_INPUT_SCHMITT_ENABLE:
		/* arg = 1: Input mode & SMT enable ;
		 * arg = 0: Output mode & SMT disable
		 */
		err = mtk_hw_set_value(hw, desc, PINCTRL_PIN_REG_DIR, !arg);
		if (err)
			goto err;

		err = mtk_hw_set_value(hw, desc, PINCTRL_PIN_REG_SMT, !!arg);
		break;
	case PIN_CONFIG_DRIVE_STRENGTH:
		if (hw->soc->capability_flags & FLAG_DRIVE_SET_RAW)
			err = mtk_hw_set_value(hw, desc, PINCTRL_PIN_REG_DRV,
				       arg);
		else if (hw->soc->drive_set)
			err = hw->soc->drive_set(hw, desc, arg);
		else
			err = -ENOTSUPP;

		break;
	case MTK_PIN_CONFIG_TDSEL:
	case MTK_PIN_CONFIG_RDSEL:
		reg = (param == MTK_PIN_CONFIG_TDSEL) ?
		       PINCTRL_PIN_REG_TDSEL : PINCTRL_PIN_REG_RDSEL;
		err = mtk_hw_set_value(hw, desc, reg, arg);
		break;
	case MTK_PIN_CONFIG_PU_ADV:
	case MTK_PIN_CONFIG_PD_ADV:
		if (hw->soc->adv_pull_set) {
			bool pullup;

			pullup = param == MTK_PIN_CONFIG_PU_ADV;
			err = hw->soc->adv_pull_set(hw, desc, pullup,
						    arg);
		} else
			err = -ENOTSUPP;
		break;
	case MTK_PIN_CONFIG_DRV_ADV:
		if (hw->soc->adv_drive_set)
			err = hw->soc->adv_drive_set(hw, desc, arg);
		else
			err = -ENOTSUPP;
		break;
	default:
		err = -ENOTSUPP;
	}

err:
	return err;
}

static struct mtk_pinctrl_group *
mtk_pctrl_find_group_by_pin(struct mtk_pinctrl *hw, u32 pin)
{
	int i;

	for (i = 0; i < hw->soc->ngrps; i++) {
		struct mtk_pinctrl_group *grp = hw->groups + i;

		if (grp->pin == pin)
			return grp;
	}

	return NULL;
}

static const struct mtk_func_desc *
mtk_pctrl_find_function_by_pin(struct mtk_pinctrl *hw, u32 pin_num, u32 fnum)
{
	const struct mtk_pin_desc *pin = hw->soc->pins + pin_num;
	const struct mtk_func_desc *func = pin->funcs;

	while (func && func->name) {
		if (func->muxval == fnum)
			return func;
		func++;
	}

	return NULL;
}

static bool mtk_pctrl_is_function_valid(struct mtk_pinctrl *hw, u32 pin_num,
					u32 fnum)
{
	int i;

	for (i = 0; i < hw->soc->npins; i++) {
		const struct mtk_pin_desc *pin = hw->soc->pins + i;

		if (pin->number == pin_num) {
			const struct mtk_func_desc *func = pin->funcs;

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
					 u32 pin, u32 fnum,
					 struct mtk_pinctrl_group *grp,
					 struct pinctrl_map **map,
					 unsigned *reserved_maps,
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
	struct mtk_pinctrl *hw = pinctrl_dev_get_drvdata(pctldev);
	int num_pins, num_funcs, maps_per_pin, i, err;
	struct mtk_pinctrl_group *grp;
	unsigned int num_configs;
	bool has_config = false;
	unsigned long *configs;
	u32 pinfunc, pin, func;
	struct property *pins;
	unsigned reserve = 0;

	pins = of_find_property(node, "pinmux", NULL);
	if (!pins) {
		dev_err(hw->dev, "missing pins property in node %pOFn .\n",
			node);
		return -EINVAL;
	}

	err = pinconf_generic_parse_dt_config(node, pctldev, &configs,
					      &num_configs);
	if (err)
		return err;

	if (num_configs)
		has_config = true;

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

	err = pinctrl_utils_reserve_map(pctldev, map, reserved_maps, num_maps,
					reserve);
	if (err < 0)
		goto exit;

	for (i = 0; i < num_pins; i++) {
		err = of_property_read_u32_index(node, "pinmux", i, &pinfunc);
		if (err)
			goto exit;

		pin = MTK_GET_PIN_NO(pinfunc);
		func = MTK_GET_PIN_FUNC(pinfunc);

		if (pin >= hw->soc->npins ||
		    func >= ARRAY_SIZE(mtk_gpio_functions)) {
			dev_err(hw->dev, "invalid pins value.\n");
			err = -EINVAL;
			goto exit;
		}

		grp = mtk_pctrl_find_group_by_pin(hw, pin);
		if (!grp) {
			dev_err(hw->dev, "unable to match pin %d to group\n",
				pin);
			err = -EINVAL;
			goto exit;
		}

		err = mtk_pctrl_dt_node_to_map_func(hw, pin, func, grp, map,
						    reserved_maps, num_maps);
		if (err < 0)
			goto exit;

		if (has_config) {
			err = pinctrl_utils_add_map_configs(pctldev, map,
							    reserved_maps,
							    num_maps,
							    grp->name,
							    configs,
							    num_configs,
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
				    struct pinctrl_map **map,
				    unsigned *num_maps)
{
	struct device_node *np;
	unsigned reserved_maps;
	int ret;

	*map = NULL;
	*num_maps = 0;
	reserved_maps = 0;

	for_each_child_of_node(np_config, np) {
		ret = mtk_pctrl_dt_subnode_to_map(pctldev, np, map,
						  &reserved_maps,
						  num_maps);
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
	struct mtk_pinctrl *hw = pinctrl_dev_get_drvdata(pctldev);

	return hw->soc->ngrps;
}

static const char *mtk_pctrl_get_group_name(struct pinctrl_dev *pctldev,
					    unsigned group)
{
	struct mtk_pinctrl *hw = pinctrl_dev_get_drvdata(pctldev);

	return hw->groups[group].name;
}

static int mtk_pctrl_get_group_pins(struct pinctrl_dev *pctldev,
				    unsigned group, const unsigned **pins,
				    unsigned *num_pins)
{
	struct mtk_pinctrl *hw = pinctrl_dev_get_drvdata(pctldev);

	*pins = (unsigned *)&hw->groups[group].pin;
	*num_pins = 1;

	return 0;
}

static int mtk_hw_get_value_wrap(struct mtk_pinctrl *hw, unsigned int gpio, int field)
{
	const struct mtk_pin_desc *desc;
	int value, err;

	if (gpio >= hw->soc->npins)
		return -EINVAL;

	desc = (const struct mtk_pin_desc *)&hw->soc->pins[gpio];

	err = mtk_hw_get_value(hw, desc, field, &value);
	if (err)
		return err;

	return value;
}

#define mtk_pctrl_get_pinmux(hw, gpio)			\
	mtk_hw_get_value_wrap(hw, gpio, PINCTRL_PIN_REG_MODE)

#define mtk_pctrl_get_direction(hw, gpio)		\
	mtk_hw_get_value_wrap(hw, gpio, PINCTRL_PIN_REG_DIR)

#define mtk_pctrl_get_out(hw, gpio)			\
	mtk_hw_get_value_wrap(hw, gpio, PINCTRL_PIN_REG_DO)

#define mtk_pctrl_get_in(hw, gpio)			\
	mtk_hw_get_value_wrap(hw, gpio, PINCTRL_PIN_REG_DI)

#define mtk_pctrl_get_smt(hw, gpio)			\
	mtk_hw_get_value_wrap(hw, gpio, PINCTRL_PIN_REG_SMT)

#define mtk_pctrl_get_ies(hw, gpio)			\
	mtk_hw_get_value_wrap(hw, gpio, PINCTRL_PIN_REG_IES)

#define mtk_pctrl_get_driving(hw, gpio)			\
	mtk_hw_get_value_wrap(hw, gpio, PINCTRL_PIN_REG_DRV)

ssize_t mtk_pctrl_show_one_pin(struct mtk_pinctrl *hw,
	unsigned int gpio, char *buf, unsigned int bufLen)
{
	int pullup = 0, pullen = 0, r1 = -1, r0 = -1, len = 0, rsel = -1;
	int pinmux, val;
	const struct mtk_pin_desc *desc;

	if (gpio >= hw->soc->npins)
		return -EINVAL;

	if (mtk_is_virt_gpio(hw, gpio))
		return -EINVAL;

	desc = (const struct mtk_pin_desc *)&hw->soc->pins[gpio];

	if (desc->eint.eint_m == EINT_NO_GPIO)
		return 0;

	if (desc->eint.eint_m != NO_EINT_SUPPORT
	 && desc->funcs[desc->eint.eint_m].name == 0)
		return 0;

	pinmux = mtk_pctrl_get_pinmux(hw, gpio);
	if (pinmux >= hw->soc->nfuncs)
		pinmux -= hw->soc->nfuncs;

	val = mtk_pinconf_bias_get_combo(hw, desc, &pullup, &pullen);
	if (val < 0) {
		pullen = -1;
	} else if (!val && pullen >= MTK_PULL_SET_RSEL_000
		&& pullen <= MTK_PULL_SET_RSEL_MAX) {
		rsel = pullen - MTK_PULL_SET_RSEL_000;
		if (pullup == 2) {
			pullup = 0;
			pullen = 0;
		} else
			pullen = 1;
	} else if (pullen == MTK_PUPD_SET_R1R0_00) {
		pullen = 0;
		r1 = 0;
		r0 = 0;
	} else if (pullen == MTK_PUPD_SET_R1R0_01) {
		pullen = 1;
		r1 = 0;
		r0 = 1;
	} else if (pullen == MTK_PUPD_SET_R1R0_10) {
		pullen = 1;
		r1 = 1;
		r0 = 0;
	} else if (pullen == MTK_PUPD_SET_R1R0_11) {
		pullen = 1;
		r1 = 1;
		r0 = 1;
	} else if (pullup == 2) {
		pullup = 0;
		pullen = 0;
	}

	len += snprintf(buf + len, bufLen - len,
			"%03d: %1d%1d%1d%1d",
			gpio,
			pinmux,
			mtk_pctrl_get_direction(hw, gpio),
			mtk_pctrl_get_out(hw, gpio),
			mtk_pctrl_get_in(hw, gpio));

	val = mtk_pctrl_get_driving(hw, gpio);
	if (val >= 0)
		len += snprintf(buf + len, bufLen - len, "%02d", val);
	else
		len += snprintf(buf + len, bufLen - len, "XX");

	val = mtk_pctrl_get_smt(hw, gpio);
	if (val >= 0)
		len += snprintf(buf + len, bufLen - len, "%1d", val);
	else
		len += snprintf(buf + len, bufLen - len, "X");

	val = mtk_pctrl_get_ies(hw, gpio);
	if (val >= 0)
		len += snprintf(buf + len, bufLen - len, "%1d", val);
	else
		len += snprintf(buf + len, bufLen - len, "X");

	if (pullen == -1)
		len += snprintf(buf + len, bufLen - len, "XX");
	else if (r1 != -1)
		len += snprintf(buf + len, bufLen - len, "%1d%1d (%1d %1d)",
			pullen, pullup, r1, r0);
	else if (rsel != -1)
		len += snprintf(buf + len, bufLen - len, "%1d%1d (rsel = %d)",
			pullen, pullup, rsel);
	else
		len += snprintf(buf + len, bufLen - len, "%1d%1d",
			pullen, pullup);

	return len;
}
EXPORT_SYMBOL_GPL(mtk_pctrl_show_one_pin);

#define PIN_DBG_BUF_SZ 96
static void mtk_pctrl_dbg_show(struct pinctrl_dev *pctldev, struct seq_file *s,
			  unsigned int gpio)
{
	struct mtk_pinctrl *hw = pinctrl_dev_get_drvdata(pctldev);
	char buf[PIN_DBG_BUF_SZ] = { 0 };

	(void)mtk_pctrl_show_one_pin(hw, gpio, buf, PIN_DBG_BUF_SZ);

	seq_printf(s, "%s", buf);
}

static const struct pinctrl_ops mtk_pctlops = {
	.dt_node_to_map		= mtk_pctrl_dt_node_to_map,
	.dt_free_map		= pinctrl_utils_free_map,
	.get_groups_count	= mtk_pctrl_get_groups_count,
	.get_group_name		= mtk_pctrl_get_group_name,
	.get_group_pins		= mtk_pctrl_get_group_pins,
	.pin_dbg_show           = mtk_pctrl_dbg_show,
};

static int mtk_pmx_get_funcs_cnt(struct pinctrl_dev *pctldev)
{
	struct mtk_pinctrl *hw = pinctrl_dev_get_drvdata(pctldev);

	if (hw->soc->nfuncs)
		return (int)hw->soc->nfuncs;

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
	struct mtk_pinctrl *hw = pinctrl_dev_get_drvdata(pctldev);

	*groups = hw->grp_names;
	*num_groups = hw->soc->ngrps;

	return 0;
}

static int mtk_pmx_set_mux(struct pinctrl_dev *pctldev,
			   unsigned function,
			   unsigned group)
{
	struct mtk_pinctrl *hw = pinctrl_dev_get_drvdata(pctldev);
	struct mtk_pinctrl_group *grp = hw->groups + group;
	const struct mtk_func_desc *desc_func;
	const struct mtk_pin_desc *desc;
	bool ret;
	int err;

	ret = mtk_pctrl_is_function_valid(hw, grp->pin, function);
	if (!ret) {
		dev_err(hw->dev, "invalid function %d on group %d .\n",
			function, group);
		return -EINVAL;
	}

	desc_func = mtk_pctrl_find_function_by_pin(hw, grp->pin, function);
	if (!desc_func)
		return -EINVAL;

	desc = (const struct mtk_pin_desc *)&hw->soc->pins[grp->pin];
	err = mtk_hw_set_value(hw, desc, PINCTRL_PIN_REG_MODE,
		desc_func->muxval);
	if (err)
		return err;

	if (hw->soc->eh_pin_pinmux) {
		err = mtk_eh_ctrl(hw, desc, desc_func->muxval);
		if (err)
			return err;
	}

	return 0;
}

static const struct pinmux_ops mtk_pmxops = {
	.get_functions_count	= mtk_pmx_get_funcs_cnt,
	.get_function_name	= mtk_pmx_get_func_name,
	.get_function_groups	= mtk_pmx_get_func_groups,
	.set_mux		= mtk_pmx_set_mux,
	.gpio_set_direction	= mtk_pinmux_gpio_set_direction,
	.gpio_request_enable	= mtk_pinmux_gpio_request_enable,
};

static int mtk_pconf_group_get(struct pinctrl_dev *pctldev, unsigned group,
			       unsigned long *config)
{
	struct mtk_pinctrl *hw = pinctrl_dev_get_drvdata(pctldev);
	struct mtk_pinctrl_group *grp = &hw->groups[group];

	 /* One pin per group only */
	return mtk_pinconf_get(pctldev, grp->pin, config);
}

static int mtk_pconf_group_set(struct pinctrl_dev *pctldev, unsigned group,
			       unsigned long *configs, unsigned num_configs)
{
	struct mtk_pinctrl *hw = pinctrl_dev_get_drvdata(pctldev);
	struct mtk_pinctrl_group *grp = &hw->groups[group];
	int i, ret;

	for (i = 0; i < num_configs; i++) {
		ret = mtk_pinconf_set(pctldev, grp->pin,
				      pinconf_to_config_param(configs[i]),
				      pinconf_to_config_argument(configs[i]));
		if (ret < 0)
			return ret;
	}

	return 0;
}

static const struct pinconf_ops mtk_confops = {
	.pin_config_get = mtk_pinconf_get,
	.pin_config_group_get	= mtk_pconf_group_get,
	.pin_config_group_set	= mtk_pconf_group_set,
	.is_generic = true,
};

static struct pinctrl_desc mtk_desc = {
	.name = MTK_PINCTRL_DEV,
	.pctlops = &mtk_pctlops,
	.pmxops = &mtk_pmxops,
	.confops = &mtk_confops,
	.owner = THIS_MODULE,
};

static int mtk_gpio_get_direction(struct gpio_chip *chip, unsigned int gpio)
{
	struct mtk_pinctrl *hw = gpiochip_get_data(chip);
	const struct mtk_pin_desc *desc;
	int value, err;

	if (gpio >= hw->soc->npins)
		return -EINVAL;

	/*
	 * "Virtual" GPIOs are always and only used for interrupts
	 * Since they are only used for interrupts, they are always inputs
	 */
	if (mtk_is_virt_gpio(hw, gpio))
		return 1;

	desc = (const struct mtk_pin_desc *)&hw->soc->pins[gpio];

	err = mtk_hw_get_value(hw, desc, PINCTRL_PIN_REG_DIR, &value);
	if (err)
		return err;

	if (value)
		return GPIO_LINE_DIRECTION_OUT;

	return GPIO_LINE_DIRECTION_IN;
}

static int mtk_gpio_get(struct gpio_chip *chip, unsigned int gpio)
{
	struct mtk_pinctrl *hw = gpiochip_get_data(chip);
	const struct mtk_pin_desc *desc;
	int value, err;

	if (gpio >= hw->soc->npins)
		return -EINVAL;

	desc = (const struct mtk_pin_desc *)&hw->soc->pins[gpio];

	err = mtk_hw_get_value(hw, desc, PINCTRL_PIN_REG_DI, &value);
	if (err)
		return err;

	return !!value;
}

static void mtk_gpio_set(struct gpio_chip *chip, unsigned int gpio, int value)
{
	struct mtk_pinctrl *hw = gpiochip_get_data(chip);
	const struct mtk_pin_desc *desc;

	if (gpio >= hw->soc->npins)
		return;

	desc = (const struct mtk_pin_desc *)&hw->soc->pins[gpio];

	(void)mtk_hw_set_value(hw, desc, PINCTRL_PIN_REG_DO, !!value);
}

static int mtk_gpio_direction_input(struct gpio_chip *chip, unsigned int gpio)
{
	struct mtk_pinctrl *hw = gpiochip_get_data(chip);

	if (gpio >= hw->soc->npins)
		return -EINVAL;

	return pinctrl_gpio_direction_input(chip->base + gpio);
}

static int mtk_gpio_direction_output(struct gpio_chip *chip, unsigned int gpio,
				     int value)
{
	struct mtk_pinctrl *hw = gpiochip_get_data(chip);

	if (gpio >= hw->soc->npins)
		return -EINVAL;

	mtk_gpio_set(chip, gpio, value);

	return pinctrl_gpio_direction_output(chip->base + gpio);
}

static int mtk_gpio_to_irq(struct gpio_chip *chip, unsigned int offset)
{
	struct mtk_pinctrl *hw = gpiochip_get_data(chip);
	const struct mtk_pin_desc *desc;

	if (!hw->eint)
		return -ENOTSUPP;

	desc = (const struct mtk_pin_desc *)&hw->soc->pins[offset];

	if (desc->eint.eint_n == EINT_NA)
		return -ENOTSUPP;

	return mtk_eint_find_irq(hw->eint, desc->eint.eint_n);
}

static int mtk_gpio_set_config(struct gpio_chip *chip, unsigned int offset,
			       unsigned long config)
{
	struct mtk_pinctrl *hw = gpiochip_get_data(chip);
	const struct mtk_pin_desc *desc;
	u32 debounce;

	desc = (const struct mtk_pin_desc *)&hw->soc->pins[offset];

	if (!hw->eint ||
	    pinconf_to_config_param(config) != PIN_CONFIG_INPUT_DEBOUNCE ||
	    desc->eint.eint_n == EINT_NA)
		return -ENOTSUPP;

	debounce = pinconf_to_config_argument(config);

	return mtk_eint_set_debounce(hw->eint, desc->eint.eint_n, debounce);
}

static int mtk_build_gpiochip(struct mtk_pinctrl *hw, struct device_node *np)
{
	struct gpio_chip *chip = &hw->chip;
	int ret;

	chip->label		= MTK_PINCTRL_DEV;
	chip->parent		= hw->dev;
	chip->request		= gpiochip_generic_request;
	chip->free		= gpiochip_generic_free;
	chip->get_direction	= mtk_gpio_get_direction;
	chip->direction_input	= mtk_gpio_direction_input;
	chip->direction_output	= mtk_gpio_direction_output;
	chip->get		= mtk_gpio_get;
	chip->set		= mtk_gpio_set;
	chip->to_irq		= mtk_gpio_to_irq,
	chip->set_config	= mtk_gpio_set_config,
	chip->base		= -1;
	chip->ngpio		= hw->soc->npins;
	chip->of_node		= np;
	chip->of_gpio_n_cells	= 2;

	ret = gpiochip_add_data(chip, hw);
	if (ret < 0)
		return ret;

	return 0;
}

static int mtk_pctrl_build_state(struct platform_device *pdev)
{
	struct mtk_pinctrl *hw = platform_get_drvdata(pdev);
	int i;

	/* Allocate groups */
	hw->groups = devm_kmalloc_array(&pdev->dev, hw->soc->ngrps,
					sizeof(*hw->groups), GFP_KERNEL);
	if (!hw->groups)
		return -ENOMEM;

	/* We assume that one pin is one group, use pin name as group name. */
	hw->grp_names = devm_kmalloc_array(&pdev->dev, hw->soc->ngrps,
					   sizeof(*hw->grp_names), GFP_KERNEL);
	if (!hw->grp_names)
		return -ENOMEM;

	for (i = 0; i < hw->soc->npins; i++) {
		const struct mtk_pin_desc *pin = hw->soc->pins + i;
		struct mtk_pinctrl_group *group = hw->groups + i;

		group->name = pin->name;
		group->pin = pin->number;

		hw->grp_names[i] = pin->name;
	}

	return 0;
}

int mtk_paris_pinctrl_probe(struct platform_device *pdev,
			    const struct mtk_pin_soc *soc)
{
	struct device_node *np = pdev->dev.of_node;
	struct pinctrl_pin_desc *pins;
	struct mtk_pinctrl *hw;
	struct property *prop;
	int err, i;

	hw = devm_kzalloc(&pdev->dev, sizeof(*hw), GFP_KERNEL);
	if (!hw)
		return -ENOMEM;

	platform_set_drvdata(pdev, hw);
	hw->soc = soc;
	hw->dev = &pdev->dev;

	prop = of_find_property(np, "reg", NULL);
	if (!prop)
		return -ENXIO;
	i = prop->length / (sizeof(unsigned int) * 4);
	if (i < 1)
		return -EINVAL;
	hw->nbase = i;
	hw->base = devm_kmalloc_array(&pdev->dev, i, sizeof(*hw->base),
		GFP_KERNEL | __GFP_ZERO);
	if (IS_ERR(hw->base))
		return PTR_ERR(hw->base);
	for (i = 0; i < hw->nbase; i++) {
		hw->base[i] = of_iomap(np, i);
		if (IS_ERR(hw->base[i]))
			return PTR_ERR(hw->base[i]);
	}

	err = mtk_pctrl_build_state(pdev);
	if (err) {
		dev_err(&pdev->dev, "build state failed: %d\n", err);
		return -EINVAL;
	}

	/* Copy from internal struct mtk_pin_desc to register to the core */
	pins = devm_kmalloc_array(&pdev->dev, hw->soc->npins, sizeof(*pins),
				  GFP_KERNEL);
	if (!pins)
		return -ENOMEM;

	for (i = 0; i < hw->soc->npins; i++) {
		pins[i].number = hw->soc->pins[i].number;
		pins[i].name = hw->soc->pins[i].name;
	}

	/* Setup pins descriptions per SoC types */
	mtk_desc.pins = (const struct pinctrl_pin_desc *)pins;
	mtk_desc.npins = hw->soc->npins;
	mtk_desc.num_custom_params = ARRAY_SIZE(mtk_custom_bindings);
	mtk_desc.custom_params = mtk_custom_bindings;
#ifdef CONFIG_DEBUG_FS
	mtk_desc.custom_conf_items = mtk_conf_items;
#endif

	err = devm_pinctrl_register_and_init(&pdev->dev, &mtk_desc, hw,
					     &hw->pctrl);
	if (err)
		return err;

	err = pinctrl_enable(hw->pctrl);
	if (err)
		return err;

	err = mtk_build_eint(hw, pdev);
	if (err)
		dev_warn(&pdev->dev,
			 "Failed to add EINT, but pinctrl still can work\n");

	/* Build gpiochip should be after pinctrl_enable is done */
	err = mtk_build_gpiochip(hw, pdev->dev.of_node);
	if (err) {
		dev_err(&pdev->dev, "Failed to add gpio_chip\n");
		return err;
	}

	platform_set_drvdata(pdev, hw);

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_paris_pinctrl_probe);

static int mtk_paris_pinctrl_suspend(struct device *device)
{
	struct mtk_pinctrl *pctl = dev_get_drvdata(device);

	return mtk_eint_do_suspend(pctl->eint);
}

static int mtk_paris_pinctrl_resume(struct device *device)
{
	struct mtk_pinctrl *pctl = dev_get_drvdata(device);

	return mtk_eint_do_resume(pctl->eint);
}

const struct dev_pm_ops mtk_paris_pinctrl_pm_ops = {
	.suspend_noirq = mtk_paris_pinctrl_suspend,
	.resume_noirq = mtk_paris_pinctrl_resume,
};
EXPORT_SYMBOL_GPL(mtk_paris_pinctrl_pm_ops);

static int mt63xx_lookup_field(struct mtk_pinctrl *hw, unsigned int pin, int field,
			struct mtk_pin_field_calc *found)
{
	const struct mtk_pin_reg_calc *rc;
	int start = 0, end, check;

	if (hw->soc->reg_cal && hw->soc->reg_cal[field].range) {
		rc = &hw->soc->reg_cal[field];
	} else {
		dev_dbg(hw->dev,
			"Not support field %d for pin %d\n", field, pin);
		return -EOPNOTSUPP;
	}

	end = rc->nranges - 1;

	while (start <= end) {
		check = (start + end) >> 1;
		if (pin == rc->range[check].s_pin) {
			found->s_addr = rc->range[check].s_addr;
			found->s_bit = rc->range[check].s_bit;
			found->x_bits = rc->range[check].x_bits;
			return 0;
		} else if (start == end)
			break;
		else if (pin < rc->range[check].s_pin)
			end = check - 1;
		else
			start = check + 1;
	}

	dev_dbg(hw->dev, "Not support field %d for pin = %d\n",
		field, pin);
	return -EOPNOTSUPP;
}

int mt63xx_hw_set_value(struct mtk_pinctrl *hw, unsigned int pin,
			int field, int value)
{
	struct mtk_pin_field_calc c = {0};
	struct regmap *pinctrl_regmap;
	u32 mask;
	int ret;

	pinctrl_regmap = (struct regmap *)hw->base[0];
	ret = mt63xx_lookup_field(hw, pin, field, &c);
	if (ret)
		return ret;

	mask = ((1 << c.x_bits) - 1) << c.s_bit;
	ret = regmap_update_bits(pinctrl_regmap, c.s_addr, mask,
			value << c.s_bit);

	return ret;
}
EXPORT_SYMBOL_GPL(mt63xx_hw_set_value);

static int mt63xx_hw_get_value(struct mtk_pinctrl *hw, unsigned int pin,
			int field, int *value)
{
	struct mtk_pin_field_calc c = {0};
	struct regmap *pinctrl_regmap;
	u32 reg_val, mask;
	int ret;

	pinctrl_regmap = (struct regmap *)hw->base[0];
	ret = mt63xx_lookup_field(hw, pin, field, &c);
	if (ret)
		return ret;

	ret = regmap_read(pinctrl_regmap, c.s_addr, &reg_val);
	if (ret)
		return ret;
	mask = ((1 << c.x_bits) - 1);
	*value = (reg_val >> c.s_bit) & mask;
	return 0;
}

static int mt63xx_pinmux_gpio_request_enable(struct pinctrl_dev *pctldev,
					  struct pinctrl_gpio_range *range,
					  unsigned int pin)
{
	struct mtk_pinctrl *hw = pinctrl_dev_get_drvdata(pctldev);

	return mt63xx_hw_set_value(hw, pin, PINCTRL_PIN_REG_MODE,
				hw->soc->gpio_m);
}

static int mt63xx_pinmux_gpio_set_direction(struct pinctrl_dev *pctldev,
					 struct pinctrl_gpio_range *range,
					 unsigned int pin, bool input)
{
	struct mtk_pinctrl *hw = pinctrl_dev_get_drvdata(pctldev);

	/* hardware take 0 as input direction */
	return mt63xx_hw_set_value(hw, pin, PINCTRL_PIN_REG_DIR, !input);
}

static int mt63xx_pmx_set_mux(struct pinctrl_dev *pctldev,
			   unsigned int function,
			   unsigned int group)
{
	struct mtk_pinctrl *hw = pinctrl_dev_get_drvdata(pctldev);
	struct mtk_pinctrl_group *grp = hw->groups + group;
	bool ret;

	ret = mtk_pctrl_is_function_valid(hw, grp->pin, function);
	if (!ret) {
		dev_notice(hw->dev, "invalid function %d on group %d .\n",
			function, group);
		return -EINVAL;
	}

	return mt63xx_hw_set_value(hw, grp->pin, PINCTRL_PIN_REG_MODE,
					function);
}

static int mt63xx_pinconf_get(struct pinctrl_dev *pctldev,
			   unsigned int pin, unsigned long *config)
{
	struct mtk_pinctrl *hw = pinctrl_dev_get_drvdata(pctldev);
	u32 param = pinconf_to_config_param(*config);
	const struct mtk_pin_desc *desc;
	int err, ret = 1, pullup;

	desc = (const struct mtk_pin_desc *)&hw->soc->pins[pin];

	switch (param) {
	case PIN_CONFIG_BIAS_DISABLE:
	case PIN_CONFIG_BIAS_PULL_UP:
	case PIN_CONFIG_BIAS_PULL_DOWN:
		err = mt63xx_hw_get_value(hw, pin, PINCTRL_PIN_REG_PULLEN,
				&ret);
		if (err)
			goto out;

		if (param != PIN_CONFIG_BIAS_DISABLE) {
			err = mt63xx_hw_get_value(hw, pin,
						PINCTRL_PIN_REG_PULLSEL,
						&pullup);
			if (err)
				goto out;
		}

		if (param == PIN_CONFIG_BIAS_PULL_UP) {
			/* When desire to get pull-up value,
			 * return error if current setting is pull-down
			 */
			if (!pullup)
				err = -EINVAL;
		}  else if (param == PIN_CONFIG_BIAS_PULL_DOWN) {
			/* When desire to get pull-down value,
			 * return error if current setting is pull-up
			 */
			if (pullup)
				err = -EINVAL;
		}
		break;
	case PIN_CONFIG_INPUT_ENABLE:
	case PIN_CONFIG_OUTPUT_ENABLE:
		err = mt63xx_hw_get_value(hw, pin, PINCTRL_PIN_REG_DIR, &ret);
		if (err)
			goto out;
		/*     CONFIG     Current direction return value
		 * -------------  ----------------- ----------------------
		 * OUTPUT_ENABLE       output       1 (= HW value)
		 *                     input        0 (= HW value)
		 * INPUT_ENABLE        output       0 (= reverse HW value)
		 *                     input        1 (= reverse HW value)
		 */
		if (param == PIN_CONFIG_INPUT_ENABLE)
			ret = !ret;

		break;
	case PIN_CONFIG_INPUT_SCHMITT_ENABLE:
		err = mt63xx_hw_get_value(hw, pin, PINCTRL_PIN_REG_DIR, &ret);
		if (err)
			goto out;
		/* return error when in output mode
		 * because schmitt trigger only work in input mode
		 */
		if (ret) {
			err = -EINVAL;
			goto out;
		}

		err = mt63xx_hw_get_value(hw, pin, PINCTRL_PIN_REG_SMT, &ret);

		break;
	case PIN_CONFIG_DRIVE_STRENGTH:
		err = mt63xx_hw_get_value(hw, pin, PINCTRL_PIN_REG_DRV, &ret);
		break;
	default:
		err = -EOPNOTSUPP;
	}

out:
	if (!err)
		*config = pinconf_to_config_packed(param, ret);

	return err;
}

static int mt63xx_pinconf_set(struct pinctrl_dev *pctldev, unsigned int pin,
			   enum pin_config_param param, u32 arg)
{
	struct mtk_pinctrl *hw = pinctrl_dev_get_drvdata(pctldev);
	const struct mtk_pin_desc *desc;
	int err = 0;

	desc = (const struct mtk_pin_desc *)&hw->soc->pins[pin];

	switch ((u32)param) {
	case PIN_CONFIG_BIAS_DISABLE:
		err = mt63xx_hw_set_value(hw, pin, PINCTRL_PIN_REG_PULLEN,
				       MTK_DISABLE);
		break;
	case PIN_CONFIG_BIAS_PULL_UP:
		err = mt63xx_hw_set_value(hw, pin, PINCTRL_PIN_REG_PULLEN,
				       MTK_ENABLE);
		if (err)
			goto err;

		err = mt63xx_hw_set_value(hw, pin, PINCTRL_PIN_REG_PULLSEL,
				       MTK_PULLUP);
		break;
	case PIN_CONFIG_BIAS_PULL_DOWN:
		err = mt63xx_hw_set_value(hw, pin, PINCTRL_PIN_REG_PULLEN,
				       MTK_ENABLE);
		if (err)
			goto err;

		err = mt63xx_hw_set_value(hw, pin, PINCTRL_PIN_REG_PULLSEL,
				       MTK_PULLDOWN);
		break;
	case PIN_CONFIG_OUTPUT_ENABLE:
		err = mt63xx_hw_set_value(hw, pin, PINCTRL_PIN_REG_SMT,
				       MTK_DISABLE);
		if (err)
			goto err;

		err = mt63xx_hw_set_value(hw, pin, PINCTRL_PIN_REG_DIR,
				       MTK_OUTPUT);
		break;
	case PIN_CONFIG_INPUT_ENABLE:
		err = mt63xx_hw_set_value(hw, pin, PINCTRL_PIN_REG_DIR,
				       MTK_INPUT);
		break;
	case PIN_CONFIG_OUTPUT:
		err = mt63xx_hw_set_value(hw, pin, PINCTRL_PIN_REG_DIR,
				       MTK_OUTPUT);
		if (err)
			goto err;

		err = mt63xx_hw_set_value(hw, pin, PINCTRL_PIN_REG_DO, arg);
		break;
	case PIN_CONFIG_INPUT_SCHMITT:
	case PIN_CONFIG_INPUT_SCHMITT_ENABLE:
		/* arg = 1: Input mode & SMT enable
		 * arg = 0: Output mode & SMT disable
		 */
		err = mt63xx_hw_set_value(hw, pin, PINCTRL_PIN_REG_DIR, !arg);
		if (err)
			goto err;

		err = mt63xx_hw_set_value(hw, pin, PINCTRL_PIN_REG_SMT, !!arg);
		break;
	case PIN_CONFIG_DRIVE_STRENGTH:
		err = mt63xx_hw_set_value(hw, pin, PINCTRL_PIN_REG_DRV, arg);
		break;
	default:
		err = -EOPNOTSUPP;
	}

err:
	return err;
}

static int mt63xx_pconf_group_set(struct pinctrl_dev *pctldev, unsigned int group,
			       unsigned long *configs, unsigned int num_configs)
{
	struct mtk_pinctrl *hw = pinctrl_dev_get_drvdata(pctldev);
	struct mtk_pinctrl_group *grp = &hw->groups[group];
	int i, ret;

	for (i = 0; i < num_configs; i++) {
		ret = mt63xx_pinconf_set(pctldev, grp->pin,
				      pinconf_to_config_param(configs[i]),
				      pinconf_to_config_argument(configs[i]));
		if (ret < 0)
			return ret;

		grp->config = configs[i];
	}

	return 0;
}

ssize_t mt63xx_pctrl_show_one_pin(struct mtk_pinctrl *hw,
	unsigned int gpio, char *buf, unsigned int bufLen)
{
	int len = 0, val = 0;

	/*
	 * Normally, we shall check >= hw->soc->npins.
	 * However, when pin number starting index is 1, instead of 0,
	 *  we shall check > hw->soc->npins.
	 */
	if (gpio >= hw->soc->npins)
		return -EINVAL;

	if (mt63xx_hw_get_value(hw, gpio, PINCTRL_PIN_REG_MODE, &val) >= 0)
		len += snprintf(buf + len, bufLen - len,
			"%02d: %1d", gpio, val);
	else
		len += snprintf(buf + len, bufLen - len,
			"%02d: X", gpio);
	if (mt63xx_hw_get_value(hw, gpio, PINCTRL_PIN_REG_DIR, &val) >= 0)
		len += snprintf(buf + len, bufLen - len, "%1d", val);
	else
		len += snprintf(buf + len, bufLen - len, "X");
	if (mt63xx_hw_get_value(hw, gpio, PINCTRL_PIN_REG_DO, &val) >= 0)
		len += snprintf(buf + len, bufLen - len, "%1d", val);
	else
		len += snprintf(buf + len, bufLen - len, "X");
	if (mt63xx_hw_get_value(hw, gpio, PINCTRL_PIN_REG_DI, &val) >= 0)
		len += snprintf(buf + len, bufLen - len, "%1d", val);
	else
		len += snprintf(buf + len, bufLen - len, "XX");
	if (mt63xx_hw_get_value(hw, gpio, PINCTRL_PIN_REG_DRV, &val) >= 0)
		len += snprintf(buf + len, bufLen - len, "%02d", val);
	else
		len += snprintf(buf + len, bufLen - len, "X");
	if (mt63xx_hw_get_value(hw, gpio, PINCTRL_PIN_REG_SMT, &val) >= 0)
		len += snprintf(buf + len, bufLen - len, "%1d", val);
	else
		len += snprintf(buf + len, bufLen - len, "X");
	if (mt63xx_hw_get_value(hw, gpio, PINCTRL_PIN_REG_PULLEN, &val) >= 0)
		len += snprintf(buf + len, bufLen - len, "%1d", val);
	else
		len += snprintf(buf + len, bufLen - len, "X");
	if (mt63xx_hw_get_value(hw, gpio, PINCTRL_PIN_REG_PULLSEL, &val) >= 0)
		len += snprintf(buf + len, bufLen - len, "%1d", val);
	else
		len += snprintf(buf + len, bufLen - len, "X");

	return len;
}

static void mt63xx_pctrl_dbg_show(struct pinctrl_dev *pctldev, struct seq_file *s,
			  unsigned int gpio)
{
	struct mtk_pinctrl *hw = pinctrl_dev_get_drvdata(pctldev);
	char buf[PIN_DBG_BUF_SZ];

	if (mt63xx_pctrl_show_one_pin(hw, gpio, buf, PIN_DBG_BUF_SZ) > 0)
		seq_printf(s, "%s", buf);
}

static const struct pinctrl_ops mt63xx_pctlops = {
	.dt_node_to_map         = mtk_pctrl_dt_node_to_map,
	.dt_free_map            = pinctrl_utils_free_map,
	.get_groups_count       = mtk_pctrl_get_groups_count,
	.get_group_name         = mtk_pctrl_get_group_name,
	.get_group_pins         = mtk_pctrl_get_group_pins,
	.pin_dbg_show           = mt63xx_pctrl_dbg_show,
};

static const struct pinconf_ops mt63xx_confops = {
	.pin_config_get = mt63xx_pinconf_get,
	.pin_config_group_get   = mtk_pconf_group_get,
	.pin_config_group_set   = mt63xx_pconf_group_set,
	.is_generic = true,
};

static const struct pinmux_ops mt63xx_pmxops = {
	.get_functions_count    = mtk_pmx_get_funcs_cnt,
	.get_function_name      = mtk_pmx_get_func_name,
	.get_function_groups    = mtk_pmx_get_func_groups,
	.set_mux                = mt63xx_pmx_set_mux,
	.gpio_set_direction     = mt63xx_pinmux_gpio_set_direction,
	.gpio_request_enable    = mt63xx_pinmux_gpio_request_enable,
};

static struct pinctrl_desc mtk_desc_mt63xx = {
	//.name = PINCTRL_PINCTRL_DEV,
	.name = "mt63xx",
	.pctlops = &mt63xx_pctlops,
	.pmxops = &mt63xx_pmxops,
	.confops = &mt63xx_confops,
	.owner = THIS_MODULE,
};

static int mt63xx_gpio_get_direction(struct gpio_chip *chip, unsigned int gpio)
{
	struct mtk_pinctrl *hw = gpiochip_get_data(chip);
	int value, err;

	if (gpio >= hw->soc->npins)
		return -EINVAL;

	err = mt63xx_hw_get_value(hw, gpio, PINCTRL_PIN_REG_DIR, &value);
	if (err)
		return err;

	return !value;
}

static int mt63xx_gpio_get(struct gpio_chip *chip, unsigned int gpio)
{
	struct mtk_pinctrl *hw = gpiochip_get_data(chip);
	int value, err;

	if (gpio >= hw->soc->npins)
		return -EINVAL;

	err = mt63xx_hw_get_value(hw, gpio, PINCTRL_PIN_REG_DI, &value);
	if (err)
		return err;

	return !!value;
}

static void mt63xx_gpio_set(struct gpio_chip *chip, unsigned int gpio, int value)
{
	struct mtk_pinctrl *hw = gpiochip_get_data(chip);

	if (gpio >= hw->soc->npins)
		return;

	(void)mt63xx_hw_set_value(hw, gpio, PINCTRL_PIN_REG_DO, !!value);
}

static int mt63xx_gpio_direction_input(struct gpio_chip *chip, unsigned int gpio)
{
	struct mtk_pinctrl *hw = gpiochip_get_data(chip);

	if (gpio >= hw->soc->npins)
		return -EINVAL;

	return pinctrl_gpio_direction_input(chip->base + gpio);
}

static int mt63xx_gpio_direction_output(struct gpio_chip *chip, unsigned int gpio,
				     int value)
{
	struct mtk_pinctrl *hw = gpiochip_get_data(chip);

	if (gpio >= hw->soc->npins)
		return -EINVAL;

	mt63xx_gpio_set(chip, gpio, value);

	return pinctrl_gpio_direction_output(chip->base + gpio);
}

static int mt6373_build_gpiochip(struct mtk_pinctrl *hw, struct device_node *np)
{
	struct gpio_chip *chip = &hw->chip;
	int ret;

	chip->label             = MTK_PINCTRL_DEV;
	chip->parent            = hw->dev;
	chip->request           = gpiochip_generic_request;
	chip->free              = gpiochip_generic_free;
	chip->get_direction     = mt63xx_gpio_get_direction;
	chip->direction_input   = mt63xx_gpio_direction_input;
	chip->direction_output  = mt63xx_gpio_direction_output;
	chip->get               = mt63xx_gpio_get;
	chip->set               = mt63xx_gpio_set;
	chip->base              = -1;
	chip->ngpio             = hw->soc->npins;
	chip->of_node           = np;
	chip->of_gpio_n_cells   = 2;

	ret = gpiochip_add_data(chip, hw);
	if (ret < 0)
		return ret;

	return 0;
}

int mt63xx_pinctrl_probe(struct platform_device *pdev,
	const struct mtk_pin_soc *soc)
{
	struct pinctrl_pin_desc *pins;
	struct mtk_pinctrl *hw;
	int err, i, npins;

	hw = devm_kzalloc(&pdev->dev, sizeof(*hw), GFP_KERNEL);
	if (!hw)
		return -ENOMEM;

	platform_set_drvdata(pdev, hw);
	hw->soc = soc;
	hw->dev = &pdev->dev;

	hw->base = devm_kmalloc_array(&pdev->dev, 1, sizeof(*hw->base),
			GFP_KERNEL | __GFP_ZERO);
	if (IS_ERR(hw->base))
		return PTR_ERR(hw->base);
	hw->base[0] = (struct regmap *)dev_get_regmap(pdev->dev.parent, NULL);

	err = mtk_pctrl_build_state(pdev);
	if (err) {
		dev_notice(&pdev->dev, "build state failed: %d\n", err);
		return -EINVAL;
	}

	/* If GPIO pin number start at 1, instead of 0,
	 *  special handle is required:
	 *  1. gpio ranges in devicetree: start from 0 and add 1 to pin count,
	 *       e.g., gpio-ranges = <&mt6373_pio 1 1 13> is changed as
	 *             gpio-ranges = <&mt6373_pio 0 0 14>;
	 *  2. hw->soc->npins and hw->soc->ngrps: added by 1, so that checking
	 *       valid pin range can always using ">= hw->soc->npins"
	 *  3. devm_pinctrl_register_and_init(): use actual pin count
	 *  4. mt6373_build_gpiochip: use hw->soc->pins to register gpiochip
	 *  5. at end of static const struct mtk_pin_desc mtk_pins_mtXXXX[],
	 *       add a dummy pin declaration:
	 *       MTK_SIMPLE_PIN(0xFFFFFFFF, "DUMMY", MTK_FUNCTION(0, NULL))
	 */
	if (hw->soc->capability_flags & FLAG_GPIO_START_IDX_1)
		npins = hw->soc->npins - 1;
	else
		npins = hw->soc->npins;

	/* Copy from internal struct mtk_pin_desc to register to the core */
	pins = devm_kmalloc_array(&pdev->dev, npins, sizeof(*pins),
				  GFP_KERNEL);
	if (!pins)
		return -ENOMEM;

	for (i = 0; i < npins; i++) {
		pins[i].number = hw->soc->pins[i].number;
		pins[i].name = hw->soc->pins[i].name;
	}

	/* Setup pins descriptions per SoC types */
	mtk_desc_mt63xx.pins = (const struct pinctrl_pin_desc *)pins;
	mtk_desc_mt63xx.npins = npins;

	err = devm_pinctrl_register_and_init(&pdev->dev, &mtk_desc_mt63xx, hw,
					     &hw->pctrl);
	if (err)
		return err;

	err = pinctrl_enable(hw->pctrl);
	if (err)
		return err;

	/* Build gpiochip should be after pinctrl_enable is done */
	err = mt6373_build_gpiochip(hw, pdev->dev.of_node);
	if (err) {
		dev_notice(&pdev->dev, "Failed to add gpio_chip\n");
		return err;
	}

	platform_set_drvdata(pdev, hw);

	return 0;
}
EXPORT_SYMBOL_GPL(mt63xx_pinctrl_probe);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek Pinctrl Common Driver V2 Paris");
