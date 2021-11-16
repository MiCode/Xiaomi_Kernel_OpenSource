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
	unsigned int reg_addr;
	unsigned char bit;
	unsigned int pinmux = 0;
	unsigned int mask = (1L << GPIO_MODE_BITS) - 1;
	struct mtk_pinctrl *pctl = gpiochip_get_data(chip);

	if (pctl->devdata->pin_mode_grps)
		return mtk_pinctrl_get_gpio_mode(pctl, offset);

	reg_addr = ((offset / MAX_GPIO_MODE_PER_REG) << pctl->devdata->port_shf)
			+ pctl->devdata->pinmux_offset;

	bit = offset % MAX_GPIO_MODE_PER_REG;
	mask <<= (GPIO_MODE_BITS * bit);
	regmap_read(mtk_get_regmap(pctl, offset), reg_addr, &pinmux);
	return ((pinmux & mask) >> (GPIO_MODE_BITS * bit));
}

static int mtk_gpio_get_in(struct gpio_chip *chip, unsigned int offset)
{
	unsigned int reg_addr;
	unsigned int bit;
	unsigned int read_val = 0;
	struct mtk_pinctrl *pctl = gpiochip_get_data(chip);

	if (pctl->devdata->pin_din_grps)
		return mtk_pinctrl_get_gpio_input(pctl, offset);

	reg_addr = mtk_get_port(pctl, offset) +
		pctl->devdata->din_offset;

	bit = BIT(offset & 0xf);
	regmap_read(mtk_get_regmap(pctl, offset), reg_addr, &read_val);
	return !!(read_val & bit);
}

static int mtk_gpio_get_out(struct gpio_chip *chip, unsigned int offset)
{
	unsigned int reg_addr;
	unsigned int bit;
	unsigned int read_val = 0;
	struct mtk_pinctrl *pctl = gpiochip_get_data(chip);

	if (pctl->devdata->pin_dout_grps)
		return mtk_pinctrl_get_gpio_output(pctl, offset);

	reg_addr = mtk_get_port(pctl, offset) +
		pctl->devdata->dout_offset;

	bit = BIT(offset & pctl->devdata->port_mask);
	regmap_read(mtk_get_regmap(pctl, offset), reg_addr, &read_val);
	return !!(read_val & bit);
}

static int mtk_pullen_get(struct gpio_chip *chip, unsigned int offset)
{
	unsigned int reg_addr;
	unsigned int bit;
	unsigned int pull_en = 0;
	struct mtk_pinctrl *pctl = gpiochip_get_data(chip);
	int samereg = 0;

	/* For phone, if the pull_en have been implemented */
	if (pctl->devdata->mtk_pctl_get_pull_en)
		return pctl->devdata->mtk_pctl_get_pull_en(pctl, offset);

	if (pctl->devdata->spec_pull_get) {
		samereg =
			pctl->devdata->spec_pull_get(mtk_get_regmap(pctl,
			offset), offset);

		if (samereg != -1) {
			pull_en = (samereg >> 1) & 0x3;
			return pull_en;
		}
	}

	reg_addr = mtk_get_port(pctl, offset) + pctl->devdata->pullen_offset;

	bit = BIT(offset & pctl->devdata->port_mask);
	regmap_read(mtk_get_regmap(pctl, offset), reg_addr, &pull_en);
	return !!(pull_en & bit);
}

static int mtk_pullsel_get(struct gpio_chip *chip, unsigned int offset)
{
	unsigned int reg_addr;
	unsigned int bit;
	unsigned int pull_sel = 0;
	struct mtk_pinctrl *pctl = gpiochip_get_data(chip);

	if (pctl->devdata->mtk_pctl_get_pull_sel)
		return pctl->devdata->mtk_pctl_get_pull_sel(pctl, offset);

	if (pctl->devdata->spec_pull_get) {
		pull_sel =
			pctl->devdata->spec_pull_get(mtk_get_regmap(pctl,
			offset), offset);
		if (pull_sel != -1)
			return pull_sel;
	}

	reg_addr = mtk_get_port(pctl, offset) + pctl->devdata->pullsel_offset;

	bit = BIT(offset & pctl->devdata->port_mask);
	regmap_read(mtk_get_regmap(pctl, offset), reg_addr, &pull_sel);
	return !!(pull_sel & bit);
}

static int mtk_ies_get(struct gpio_chip *chip, unsigned int offset)
{
	unsigned int reg_addr;
	unsigned char bit;
	unsigned int ies;
	struct mtk_pinctrl *pctl = gpiochip_get_data(chip);

	if (pctl->devdata->pin_ies_grps)
		return mtk_pinctrl_get_gpio_ies(pctl, offset);

	if (!pctl->devdata->spec_ies_get ||
		pctl->devdata->ies_offset == MTK_PINCTRL_NOT_SUPPORT)
		return -1;

	/**
	 * Due to some soc are not support ies config, add this special
	 * control to handle it.
	 */
	if (pctl->devdata->spec_ies_get) {
		ies = pctl->devdata->spec_ies_get(mtk_get_regmap(pctl, offset),
			offset);
		if (ies != -1)
			return ies;
	}

	reg_addr = mtk_get_port(pctl, offset) + pctl->devdata->ies_offset;

	bit = BIT(offset & pctl->devdata->port_mask);
	regmap_read(mtk_get_regmap(pctl, offset), reg_addr, &ies);
	return !!(ies & bit);
}

static int mtk_smt_get(struct gpio_chip *chip, unsigned int offset)
{
	unsigned int reg_addr;
	unsigned char bit;
	unsigned int smt;
	struct mtk_pinctrl *pctl = gpiochip_get_data(chip);

	if (pctl->devdata->pin_smt_grps)
		return mtk_pinctrl_get_gpio_smt(pctl, offset);

	if (!pctl->devdata->spec_smt_get ||
		pctl->devdata->smt_offset == MTK_PINCTRL_NOT_SUPPORT)
		return -1;

	/**
	 * Due to some soc are not support smt config, add this special
	 * control to handle it.
	 */
	if (pctl->devdata->spec_smt_get) {
		smt = pctl->devdata->spec_smt_get(mtk_get_regmap(pctl, offset),
			offset);
		if (smt != -1)
			return smt;
	}

	reg_addr = mtk_get_port(pctl, offset) + pctl->devdata->smt_offset;

	bit = BIT(offset & pctl->devdata->port_mask);
	regmap_read(mtk_get_regmap(pctl, offset), reg_addr, &smt);
	return !!(smt & bit);
}

static int mtk_driving_get(struct gpio_chip *chip, unsigned int offset)
{
	const struct mtk_pin_drv_grp *pin_drv;
	unsigned int val = 0;
	unsigned int bits, mask, shift;
	const struct mtk_drv_group_desc *drv_grp;
	struct mtk_pinctrl *pctl = gpiochip_get_data(chip);

	if (offset >= pctl->devdata->npins)
		return -1;

	if (pctl->devdata->pin_drv_grps)
		return mtk_pinctrl_get_gpio_driving(pctl, offset);

	pin_drv = mtk_find_pin_drv_grp_by_pin(pctl, offset);
	if (!pin_drv || pin_drv->grp > pctl->devdata->n_grp_cls)
		return -1;

	drv_grp = pctl->devdata->grp_desc + pin_drv->grp;
	bits = drv_grp->high_bit - drv_grp->low_bit + 1;
	mask = BIT(bits) - 1;
	shift = pin_drv->bit + drv_grp->low_bit;
	mask <<= shift;
	regmap_read(mtk_get_regmap(pctl, offset), pin_drv->offset, &val);
	return ((val & mask) >> shift);
}

static ssize_t mtk_gpio_show_pin(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int len = 0;
	int bufLen = PAGE_SIZE;
	struct mtk_pinctrl *pctl = dev_get_drvdata(dev);
	struct gpio_chip *chip = pctl->chip;
	unsigned int i;
	int pull_val;

	len += snprintf(buf+len, bufLen-len,
		"PIN: [MODE] [DIR] [DOUT] [DIN] [PULL_EN] [PULL_SEL] [IES] [SMT] [DRIVE] ( [R1] [R0] )\n");

	for (i = 0; i < chip->ngpio; i++) {
		pull_val = mtk_pullsel_get(chip, i);

		len += snprintf(buf+len, bufLen-len,
				"%4d:% d% d% d% d% d% d% d% d% d",
				i,
				mtk_pinmux_get(chip, i),
				!mtk_gpio_get_direction(chip, i),
				mtk_gpio_get_out(chip, i),
				mtk_gpio_get_in(chip, i),
				mtk_pullen_get(chip, i),
				(pull_val >= 0) ? (pull_val&1) : -1,
				mtk_ies_get(chip, i),
				mtk_smt_get(chip, i),
				mtk_driving_get(chip, i));
		if ((pull_val & 8) && (pull_val >= 0))
			len += snprintf(buf+len, bufLen-len, " %d %d",
				!!(pull_val&4), !!(pull_val&2));
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
		pr_debug("[gpio_dump_regs]error: pinctrl does not exist\n");
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
	int pin, val;
	int val_set;
	struct mtk_pinctrl *pctl = dev_get_drvdata(dev);
	struct pinctrl_dev *pctldev = pctl->pctl_dev;

	if (!strncmp(buf, "mode", 4) &&
		(sscanf(buf+4, "%d %d", &pin, &val) == 2)) {
		val_set = mtk_pmx_set_mode(pctldev, pin, val);
	} else if (!strncmp(buf, "dir", 3) &&
		(sscanf(buf+3, "%d %d", &pin, &val) == 2)) {
		val_set  = mtk_pmx_gpio_set_direction(pctldev, NULL, pin, !val);
	} else if (!strncmp(buf, "out", 3) &&
		(sscanf(buf+3, "%d %d", &pin, &val) == 2)) {
		mtk_pmx_gpio_set_direction(pctldev, NULL, pin, false);
		mtk_gpio_set(pctl->chip, pin, val);
	} else if (!strncmp(buf, "pullen", 6) &&
		(sscanf(buf+6, "%d %d", &pin, &val) == 2)) {
		val_set = mtk_pconf_set_pull_select(pctl, pin, !!val,
			false, MTK_PUPD_SET_R1R0_00 + val);
	} else if (!strncmp(buf, "pullsel", 7) &&
		(sscanf(buf+7, "%d %d", &pin, &val) == 2)) {
		val_set = mtk_pconf_set_pull_select(pctl, pin, true,
			!!val, MTK_PUPD_SET_R1R0_01);
	} else if (!strncmp(buf, "ies", 3) &&
		(sscanf(buf+3, "%d %d", &pin, &val) == 2)) {
		val_set = mtk_pconf_set_ies_smt(pctl, pin, val,
			PIN_CONFIG_INPUT_ENABLE);
	} else if (!strncmp(buf, "smt", 3) &&
		(sscanf(buf+3, "%d %d", &pin, &val) == 2)) {
		val_set = mtk_pconf_set_ies_smt(pctl, pin, val,
			PIN_CONFIG_INPUT_SCHMITT_ENABLE);
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
