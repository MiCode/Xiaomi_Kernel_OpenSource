/*
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Light Hsieh <light.hsieh@mediatek.com>
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

#if defined(CONFIG_PINCTRL_MTK_ALTERNATIVE)
static int mtk_pinctrl_get_gpio_output(struct mtk_pinctrl *pctl, int pin)
{
	return mtk_pinctrl_get_gpio_value(pctl, pin,
		pctl->devdata->n_pin_dout, pctl->devdata->pin_dout_grps);
}

static int mtk_pinctrl_set_gpio_output(struct mtk_pinctrl *pctl,
	int pin, int value)
{
#ifndef GPIO_DEBUG
	return mtk_pinctrl_update_gpio_value(pctl, pin, value,
		pctl->devdata->n_pin_dout, pctl->devdata->pin_dout_grps);
#else
	pr_info("config pin = %d, value = %d\n", pin, value);
	mtk_pinctrl_update_gpio_value(pctl, pin, value,
		pctl->devdata->n_pin_dout, pctl->devdata->pin_dout_grps);
	pr_info("set pin = %d, value = %d\n", pin,
		mtk_pinctrl_get_gpio_output(pctl, pin));
	return 0;
#endif
}

static int mtk_pinctrl_get_gpio_input(struct mtk_pinctrl *pctl, int pin)
{
	return mtk_pinctrl_get_gpio_value(pctl, pin,
		pctl->devdata->n_pin_din, pctl->devdata->pin_din_grps);
}

static int mtk_pinctrl_get_gpio_direction(struct mtk_pinctrl *pctl, int pin)
{
	return mtk_pinctrl_get_gpio_value(pctl, pin,
		pctl->devdata->n_pin_dir, pctl->devdata->pin_dir_grps);
}

static int mtk_pinctrl_set_gpio_direction(struct mtk_pinctrl *pctl,
	int pin, bool input)
{
#ifndef GPIO_DEBUG
	return mtk_pinctrl_update_gpio_value(pctl, pin, input,
		pctl->devdata->n_pin_dir, pctl->devdata->pin_dir_grps);
#else
	pr_info("config pin = %d, dir = %d\n", pin, input);
	mtk_pinctrl_update_gpio_value(pctl, pin, input,
			pctl->devdata->n_pin_dir, pctl->devdata->pin_dir_grps);
	pr_info("set pin = %d, dir = %d\n", pin,
		mtk_pinctrl_get_gpio_direction(pctl, pin));
	return 0;
#endif
}

static int mtk_pinctrl_get_gpio_mode(struct mtk_pinctrl *pctl, int pin)
{
	return mtk_pinctrl_get_gpio_value(pctl, pin,
		pctl->devdata->n_pin_mode, pctl->devdata->pin_mode_grps);
}

static int mtk_pinctrl_set_gpio_mode(struct mtk_pinctrl *pctl,
	int pin, unsigned long mode)
{
#ifndef GPIO_DEBUG
	return mtk_pinctrl_update_gpio_value(pctl, pin, mode,
		pctl->devdata->n_pin_mode, pctl->devdata->pin_mode_grps);
#else
	pr_info("config pin = %d, mode = %d\n", pin, (int)mode);
	mtk_pinctrl_update_gpio_value(pctl, pin, mode,
		pctl->devdata->n_pin_mode, pctl->devdata->pin_mode_grps);
	pr_info("set pin = %d, mode = %d\n", pin,
		mtk_pinctrl_get_gpio_mode(pctl, pin));
	return 0;
#endif
}

static int mtk_pinctrl_get_gpio_driving(struct mtk_pinctrl *pctl, int pin)
{
	return mtk_pinctrl_get_gpio_value(pctl, pin,
		pctl->devdata->n_pin_drv, pctl->devdata->pin_drv_grps);
}

static int mtk_pinctrl_set_gpio_driving(struct mtk_pinctrl *pctl,
	int pin, unsigned char driving)
{
#ifndef GPIO_DEBUG
	return mtk_pinctrl_update_gpio_value(pctl, pin, driving,
		pctl->devdata->n_pin_drv, pctl->devdata->pin_drv_grps);
#else
	pr_info("config pin = %d, driving = %d\n", pin, driving);
	mtk_pinctrl_update_gpio_value(pctl, pin, driving,
		pctl->devdata->n_pin_drv, pctl->devdata->pin_drv_grps);
	pr_info("set pin = %d, driving = %d\n", pin,
		mtk_pinctrl_get_gpio_driving(pctl, pin));
	return 0;
#endif
}

static int mtk_pinctrl_get_gpio_smt(struct mtk_pinctrl *pctl, int pin)
{
	return mtk_pinctrl_get_gpio_value(pctl, pin,
		pctl->devdata->n_pin_smt, pctl->devdata->pin_smt_grps);
}

static int mtk_pinctrl_set_gpio_smt(struct mtk_pinctrl *pctl,
	int pin, bool enable)
{
	return mtk_pinctrl_update_gpio_value(pctl, pin, enable,
		pctl->devdata->n_pin_smt, pctl->devdata->pin_smt_grps);
}

static int mtk_pinctrl_get_gpio_ies(struct mtk_pinctrl *pctl, int pin)
{
	return mtk_pinctrl_get_gpio_value(pctl, pin,
		pctl->devdata->n_pin_ies, pctl->devdata->pin_ies_grps);
}

static int mtk_pinctrl_set_gpio_ies(struct mtk_pinctrl *pctl,
	int pin, bool enable)
{
	return mtk_pinctrl_update_gpio_value(pctl, pin, enable,
		pctl->devdata->n_pin_ies, pctl->devdata->pin_ies_grps);
}

static int mtk_pinmux_get(struct gpio_chip *chip, unsigned int offset)
{
	struct mtk_pinctrl *pctl = gpiochip_get_data(chip);

	if (pctl->devdata->pin_mode_grps)
		return mtk_pinctrl_get_gpio_mode(pctl, offset);

	return -ENOENT;
}

static int mtk_gpio_get_in(struct gpio_chip *chip, unsigned int offset)
{
	struct mtk_pinctrl *pctl = gpiochip_get_data(chip);

	if (pctl->devdata->pin_din_grps)
		return mtk_pinctrl_get_gpio_input(pctl, offset);

	return -ENOENT;
}

static int mtk_gpio_get_out(struct gpio_chip *chip, unsigned int offset)
{
	struct mtk_pinctrl *pctl = gpiochip_get_data(chip);

	if (pctl->devdata->pin_dout_grps)
		return mtk_pinctrl_get_gpio_output(pctl, offset);

	return -ENOENT;
}

static int mtk_pullen_get(struct gpio_chip *chip, unsigned int offset)
{
	struct mtk_pinctrl *pctl = gpiochip_get_data(chip);

	/* For phone, if the pull_en have been implemented */
	if (pctl->devdata->mtk_pctl_get_pull_en)
		return pctl->devdata->mtk_pctl_get_pull_en(pctl, offset);

	return -ENOENT;
}

static int mtk_pullsel_get(struct gpio_chip *chip, unsigned int offset)
{
	struct mtk_pinctrl *pctl = gpiochip_get_data(chip);

	if (pctl->devdata->mtk_pctl_get_pull_sel)
		return pctl->devdata->mtk_pctl_get_pull_sel(pctl, offset);

	return -ENOENT;
}

static int mtk_ies_get(struct gpio_chip *chip, unsigned int offset)
{
	struct mtk_pinctrl *pctl = gpiochip_get_data(chip);

	if (pctl->devdata->pin_ies_grps)
		return mtk_pinctrl_get_gpio_ies(pctl, offset);

	return -ENOENT;
}

static int mtk_smt_get(struct gpio_chip *chip, unsigned int offset)
{
	struct mtk_pinctrl *pctl = gpiochip_get_data(chip);

	if (pctl->devdata->pin_smt_grps)
		return mtk_pinctrl_get_gpio_smt(pctl, offset);

	return -ENOENT;
}

static int mtk_driving_get(struct gpio_chip *chip, unsigned int offset)
{
	struct mtk_pinctrl *pctl = gpiochip_get_data(chip);

	if (pctl->devdata->pin_drv_grps)
		return mtk_pinctrl_get_gpio_driving(pctl, offset);

	return -ENOENT;
}

static ssize_t mtk_gpio_show_pin(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int len = 0;
	unsigned int bufLen = PAGE_SIZE;
	int pull_val = -1;
	unsigned int i = 0;
	struct mtk_pinctrl *pctl = dev_get_drvdata(dev);
	struct gpio_chip *chip;

	if (!pctl || !pctl->chip || !buf) {
		pr_debug("[pinctrl] Err: NULL pointer!\n");
		return len;
	}

	chip = pctl->chip;
	len += snprintf(buf+len, bufLen-len,
		"gpio base: 0x%x, pins: %03d\n",
		chip->base, pctl->chip->ngpio);

	if (pctl->dbg_start >= pctl->chip->ngpio) {
		len += snprintf(buf+len, bufLen-len,
		"wrong gpio-range: start should less than %d!\n",
		pctl->chip->ngpio);
		return len;
	}

	len += snprintf(buf+len, bufLen-len,
		"PIN: (MODE)(DIR)(DOUT)(DIN)(DRIVE)(SMT)(IES)(PULL_EN)(PULL_SEL)(R1 R0)\n");

	for (i = pctl->dbg_start; i < pctl->chip->ngpio; i++) {
		if (len > (bufLen - 96)) {
			pr_debug("[pinctrl]err:%d exceed to max size %d\n",
				len, (bufLen - 96));
			break;
		}
		len += snprintf(buf+len, bufLen-len,
				"%03d: %1d%1d%1d%1d%2d%1d%1d%1d",
				i,
				mtk_pinmux_get(chip, i),
				!mtk_gpio_get_direction(chip, i),
				mtk_gpio_get_out(chip, i),
				mtk_gpio_get_in(chip, i),
				mtk_driving_get(chip, i),
				mtk_smt_get(chip, i),
				mtk_ies_get(chip, i),
				mtk_pullen_get(chip, i));

		pull_val = mtk_pullsel_get(chip, i);
		if ((pull_val >= 0)) {
			len += snprintf(buf+len, bufLen-len, "%1d",
				(pull_val & 0x01));
			if (pull_val & 0x08)
				len += snprintf(buf+len, bufLen-len,
					"(%1d %1d)", !!(pull_val & 0x04),
					!!(pull_val & 0x02));
		} else {
			len += snprintf(buf+len, bufLen-len, "(-1)");
		}
		len += snprintf(buf+len, bufLen-len, "\n");
	}

	return len;
}

void gpio_dump_regs_range(int start, int end)
{
	struct gpio_chip *chip;
	unsigned int i;
	int pull_val;

	if (!pctl_alt) {
		pr_err("[gpio_dump_regs]error: pinctrl does not exist\n");
		return;
	}

	chip = pctl_alt->chip;

	pr_debug("PIN: [MODE] [DIR] [DOUT] [DIN][PULL_EN] [PULL_SEL] [IES] [SMT] [DRIVE] ( [R1] [R0] )\n");

	if (start < 0) {
		start = 0;
		end = chip->ngpio-1;
	}

	if (end > chip->ngpio - 1)
		end = chip->ngpio - 1;

	for (i = start; i <= end; i++) {
		pull_val = mtk_pullsel_get(chip, i);
		pr_debug("%4d: %d%d%d%d%d%d%d%d%d",
			i, mtk_pinmux_get(chip, i),
			!mtk_gpio_get_direction(chip, i),
			mtk_gpio_get_out(chip, i),
			mtk_gpio_get_in(chip, i),
			mtk_pullen_get(chip, i),
			(pull_val >= 0) ? (pull_val&1) : -1,
			mtk_ies_get(chip, i),
			mtk_smt_get(chip, i),
			mtk_driving_get(chip, i));
		if ((pull_val & MTK_PUPD_R1R0_BIT_SUPPORT) && (pull_val >= 0))
			pr_debug(" %d %d\n", !!(pull_val&4), !!(pull_val&2));
		else
			pr_debug("\n");
	}
}

void gpio_dump_regs(void)
{
	gpio_dump_regs_range(-1, -1);
}

static ssize_t mtk_gpio_store_pin(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int pin, val, val_set, pull_val;
	int i;
	int vals[11];
	char attrs[11];
	u32 r1r0_arg;
	struct mtk_pinctrl *pctl = dev_get_drvdata(dev);
	struct pinctrl_dev *pctldev = pctl->pctl_dev;

	if (!strncmp(buf, "mode", 4)
		&& (sscanf(buf+4, "%d %d", &pin, &val) == 2)) {
		val_set = mtk_pmx_set_mode(pctldev, pin, val);
	} else if (!strncmp(buf, "dir", 3)
		&& (sscanf(buf+3, "%d %d", &pin, &val) == 2)) {
		val_set  = mtk_pmx_gpio_set_direction(pctldev, NULL, pin, !val);
	} else if (!strncmp(buf, "out", 3)
		&& (sscanf(buf+3, "%d %d", &pin, &val) == 2)) {
		mtk_pmx_gpio_set_direction(pctldev, NULL, pin, false);
		mtk_gpio_set(pctl->chip, pin, val);
	} else if (!strncmp(buf, "pullen", 6)
		&& (sscanf(buf+6, "%d %d", &pin, &val) == 2)) {
		pull_val = mtk_pullsel_get(pctl->chip, pin);
		if (pull_val >= 0) {
			if (MTK_PUPD_R1R0_GET_SUPPORT(pull_val)) {
				if (MTK_PUPD_R1R0_GET_PUPD(pull_val))
					vals[5] = 1;
				else
					vals[5] = 0;
				val_set = mtk_pconf_set_pull_select(pctl, pin,
					!!val, vals[5],
					(MTK_PUPD_SET_R1R0_00 + val));
			} else {
				if (pull_val & GPIO_PULL_UP)
					vals[5] = 1;
				else
					vals[5] = 0;
				val_set = mtk_pconf_set_pull_select(pctl, pin,
					!!val, vals[5], val);
			}
		}
	} else if ((!strncmp(buf, "pullsel", 7))
		&& (sscanf(buf+7, "%d %d", &pin, &val) == 2)) {
		pull_val = mtk_pullsel_get(pctl->chip, pin);
		if (pull_val >= 0) {
			if (MTK_PUPD_R1R0_GET_SUPPORT(pull_val)) {
				vals[4] = MTK_PUPD_R1R0_GET_PULLEN(pull_val);
				val_set = mtk_pconf_set_pull_select(pctl, pin,
					true, !!val,
					(MTK_PUPD_SET_R1R0_00 + vals[4]));
			} else {
				val_set = mtk_pconf_set_pull_select(pctl, pin,
					true, !!val, 0 /* don't care */);
			}
		}
	} else if ((!strncmp(buf, "ies", 3))
		&& (sscanf(buf+3, "%d %d", &pin, &val) == 2)) {
		val_set = mtk_pconf_set_ies_smt(pctl, pin, val,
			PIN_CONFIG_INPUT_ENABLE);
	} else if ((!strncmp(buf, "smt", 3))
		&& (sscanf(buf+3, "%d %d", &pin, &val) == 2)) {
		val_set = mtk_pconf_set_ies_smt(pctl, pin, val,
			PIN_CONFIG_INPUT_SCHMITT_ENABLE);
	} else if ((!strncmp(buf, "start", 5))
		&& (sscanf(buf+5, "%d", &val) == 1)) {
		pctl->dbg_start = val;
	} else if ((sscanf(buf+7, "%d %d", &pin, &val) == 2)
		&& (!strncmp(buf, "driving", 7))) {
		val_set = mtk_pconf_set_driving(pctl, pin, val);
	} else if ((!strncmp(buf, "r1r0up", 6))
		&& (sscanf(buf+6, "%d %d", &pin, &val) == 2)) {
		val_set = mtk_pconf_set_pull_select(pctl, pin, true, true, val);
	} else if ((sscanf(buf+8, "%d %d", &pin, &val) == 2)
		&& (!strncmp(buf, "r1r0down", 8))) {
		val_set = mtk_pconf_set_pull_select(pctl, pin,
			true, false, val);
	} else if (!strncmp(buf, "set", 3)) {
		val = sscanf(buf+3, "%d %c%c%c%c%c%c%c%c%c %c%c", &pin,
			&attrs[0], &attrs[1], &attrs[2], &attrs[3],
			&attrs[4], &attrs[5], &attrs[6], &attrs[7],
			&attrs[8], &attrs[9], &attrs[10]);
		for (i = 0; i < val; i++) {
			if ((attrs[i] >= '0') && (attrs[i] <= '9'))
				vals[i] = attrs[i] - '0';
			else
				vals[i] = 0;
		}
		/* MODE */
		mtk_pmx_set_mode(pctldev, pin, vals[0]);
		/* DIR */
		mtk_pinctrl_set_gpio_direction(pctl, pin, !!vals[1]);
		/* DOUT */
		if (vals[1])
			mtk_gpio_set(pctl->chip, pin, !!vals[2]);
		/* PULLEN and PULLSEL */
		if (!vals[4]) {
			mtk_pconf_set_pull_select(pctl, pin, false,
				false, MTK_PUPD_SET_R1R0_00);
		} else {
			pull_val = mtk_pullsel_get(pctl->chip, pin);
			if ((pull_val & MTK_PUPD_R1R0_BIT_SUPPORT)
				&& (pull_val >= 0)) {
				if (val == 12) {
					if (vals[9] && vals[10])
						r1r0_arg =
						MTK_PUPD_SET_R1R0_11;
					else if (vals[9])
						r1r0_arg =
						MTK_PUPD_SET_R1R0_10;
					else if (vals[10])
						r1r0_arg =
						MTK_PUPD_SET_R1R0_01;
					else
						r1r0_arg =
						MTK_PUPD_SET_R1R0_00;
				} else {
					r1r0_arg = MTK_PUPD_SET_R1R0_00;
				}
				mtk_pconf_set_pull_select(pctl, pin,
					true, !!vals[5], r1r0_arg);
			} else {
				mtk_pconf_set_pull_select(pctl, pin,
					true, !!vals[5], 0 /* dont cared */);
			}
		}
		/* IES */
		mtk_pconf_set_ies_smt(pctl, pin, vals[6],
			PIN_CONFIG_INPUT_ENABLE);
		/* SMT */
		mtk_pconf_set_ies_smt(pctl, pin, vals[7],
			PIN_CONFIG_INPUT_SCHMITT_ENABLE);
		/* DRIVING */
		mtk_pconf_set_driving(pctl, pin, vals[8]);
	}

	return count;
}

static DEVICE_ATTR(mt_gpio, 0664, mtk_gpio_show_pin, mtk_gpio_store_pin);

static struct device_attribute *gpio_attr_list[] = {
	&dev_attr_mt_gpio,
};

static int mtk_gpio_create_attr(struct device *dev)
{
	int idx, err = 0;
	int num = ARRAY_SIZE(gpio_attr_list);

	if (!dev)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = device_create_file(dev, gpio_attr_list[idx]);
		if (err)
			break;
	}

	return err;
}

int mtk_pctrl_get_gpio_chip_base(void)
{
	if (pctl_alt)
		return pctl_alt->chip->base;
	pr_info("mtk_pinctrl is not initialized\n");
	return 0;
}
#endif
