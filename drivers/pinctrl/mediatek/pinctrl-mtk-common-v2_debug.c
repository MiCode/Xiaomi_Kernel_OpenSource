/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <dt-bindings/pinctrl/mt65xx.h>
#include <linux/gpio/consumer.h>
#include <../../gpio/gpiolib.h>
#include <linux/delay.h>
#include "pinctrl-paris.h"

#define PULL_DELAY 50 /* in ms */
#define FUN_3STATE "gpio_get_value_tristate"
int gpio_get_tristate_input(unsigned int pin)
{
	struct gpio_device *gdev;
	struct gpio_chip *chip = NULL;
	struct mtk_pinctrl *hw = NULL;
	const struct mtk_pin_desc *desc;
	int val, val_up, val_down, ret, pullup, pullen;
	unsigned long flags;

	spin_lock_irqsave(&gpio_lock, flags);
	list_for_each_entry(gdev, &gpio_devices, list) {

		chip = gdev->chip;

		hw = gpiochip_get_data(chip);

		break;
	}

	spin_unlock_irqrestore(&gpio_lock, flags);

	if (!hw || !hw->soc) {
		pr_notice("invalid gpio chip\n");
		return -EINVAL;
	}

	if (pin < chip->base) {
		pr_notice(FUN_3STATE ": please use virtual pin number\n");
		return -EINVAL;
	}

	pin -= chip->base;
	if (pin >= hw->soc->npins) {
		pr_notice(FUN_3STATE ": invalid pin number: %u\n",
			pin);
		return -EINVAL;
	}

	desc = (const struct mtk_pin_desc *)&hw->soc->pins[pin];
	val = mtk_pctrl_get_pinmux(hw, pin);
	if (val != 0) {
		pr_notice(FUN_3STATE ":GPIO%d in mode %d, not GPIO mode\n",
			pin, val);
		return -EINVAL;
	}

	ret = mtk_pinconf_bias_get_combo(hw, desc, &pullup, &pullen);
	if (ret)
		return ret;
	if (pullen == 0 ||  pullen == MTK_PUPD_SET_R1R0_00) {
		pr_notice(FUN_3STATE ":GPIO%d not pullen, skip floating test\n",
			pin);
		return mtk_pctrl_get_in(hw, pin);
	}

	/* set pullsel as pull-up and get input value */
	pr_notice(FUN_3STATE ":pull up GPIO%d\n", pin);
	mtk_pinconf_bias_set_combo(hw, desc, 1, pullen);
	mdelay(PULL_DELAY);
	val_up = mtk_pctrl_get_in(hw, pin),
	pr_notice(FUN_3STATE ":GPIO%d input %d\n", pin, val_up);

	/* set pullsel as pull-down and get input value */
	pr_notice(FUN_3STATE ":pull down GPIO%d\n", pin);
	mtk_pinconf_bias_set_combo(hw, desc, 0, pullen);
	mdelay(PULL_DELAY);
	val_down = mtk_pctrl_get_in(hw, pin);
	pr_notice(FUN_3STATE ":GPIO%d input %d\n", pin, val_down);

	if (val_up && val_down)
		ret = 1;
	else if (!val_up && !val_down)
		ret = 0;
	else if (val_up && !val_down)
		ret = 2;
	else {
		pr_notice(FUN_3STATE ":GPIO%d pull HW is abnormal\n", pin);
		ret = -EINVAL;
	}

	/* restore pullsel */
	mtk_pinconf_bias_set_combo(hw, desc, pullup, pullen);

	return ret;
}

static int mtk_hw_set_value_wrap(struct mtk_pinctrl *hw, unsigned int gpio,
	int value, int field)
{
	const struct mtk_pin_desc *desc;

	if (gpio > hw->soc->npins)
		return -EINVAL;

	desc = (const struct mtk_pin_desc *)&hw->soc->pins[gpio];

	return mtk_hw_set_value(hw, desc, field, value);
}

#define mtk_pctrl_set_pinmux(hw, gpio, val)		\
	mtk_hw_set_value_wrap(hw, gpio, val, PINCTRL_PIN_REG_MODE)

/* MTK HW use 0 as input, 1 for output
 * This interface is for set direct register value,
 * so don't reverse
 */
#define mtk_pctrl_set_direction(hw, gpio, val)		\
	mtk_hw_set_value_wrap(hw, gpio, val, PINCTRL_PIN_REG_DIR)

#define mtk_pctrl_set_out(hw, gpio, val)		\
	mtk_hw_set_value_wrap(hw, gpio, val, PINCTRL_PIN_REG_DO)

#define mtk_pctrl_set_smt(hw, gpio, val)		\
	mtk_hw_set_value_wrap(hw, gpio, val, PINCTRL_PIN_REG_SMT)

#define mtk_pctrl_set_ies(hw, gpio, val)		\
	mtk_hw_set_value_wrap(hw, gpio, val, PINCTRL_PIN_REG_IES)

static ssize_t mtk_gpio_show_pin(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int len = 0;
	unsigned int bufLen = PAGE_SIZE;
	unsigned int i = 0;
	struct mtk_pinctrl *hw = dev_get_drvdata(dev);
	struct gpio_chip *chip;

	if (!hw || !buf) {
		pr_debug("[pinctrl] Err: NULL pointer!\n");
		return len;
	}

	chip = &hw->chip;

	len += snprintf(buf+len, bufLen-len,
		"pins base: %d, pins count: %d\n", chip->base, chip->ngpio);
	len += snprintf(buf+len, bufLen-len,
		"PIN: (MODE)(DIR)(DOUT)(DIN)(DRIVE)(SMT)(IES)(PULL_EN)(PULL_SEL)(R1 R0)\n");

	for (i = 0; i < chip->ngpio; i++) {
		if (len > (bufLen - 96)) {
			pr_debug("[pinctrl]err:%d exceed to max size %d\n",
				len, (bufLen - 96));
			break;
		}
		len += mtk_pctrl_show_one_pin(hw, i, buf + len, bufLen - len);
	}

	return len;
}

void gpio_dump_regs_range(int start, int end)
{
	struct gpio_device *gdev;
	struct mtk_pinctrl *hw;
	struct gpio_chip *chip = NULL;
	unsigned long flags;
	char buf[96];
	int i;

	spin_lock_irqsave(&gpio_lock, flags);
	list_for_each_entry(gdev, &gpio_devices, list) {

		chip = gdev->chip;

		if (start < 0) {
			start = 0;
			end = chip->ngpio - 1;
		}
		if (end > chip->ngpio - 1)
			end = chip->ngpio - 1;

		pr_notice("PIN: (MODE)(DIR)(DOUT)(DIN)(DRIVE)(SMT)(IES)(PULL_EN)(PULL_SEL)(R1 R0)\n");

		hw = gpiochip_get_data(chip);
		for (i = start; i < end; i++) {
			(void)mtk_pctrl_show_one_pin(hw, i, buf, 96);
			pr_notice("%s", buf);
		}
		break;
	}

	spin_unlock_irqrestore(&gpio_lock, flags);
}

void gpio_dump_regs(void)
{
	gpio_dump_regs_range(-1, -1);
}

static ssize_t mtk_gpio_store_pin(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int i, gpio, val, val2, pullup = 0, pullen = 0;
	int vals[12];
	char attrs[12];
	struct mtk_pinctrl *hw = dev_get_drvdata(dev);
	const struct mtk_pin_desc *desc;
	struct gpio_chip *chip;
	int r1r0_en[4] = {MTK_PUPD_SET_R1R0_00, MTK_PUPD_SET_R1R0_01,
			  MTK_PUPD_SET_R1R0_10, MTK_PUPD_SET_R1R0_11};

	if (!hw) {
		pr_debug("[pinctrl] Err: NULL pointer!\n");
		return count;
	}

	chip = &hw->chip;

	if (!strncmp(buf, "mode", 4)
		&& (sscanf(buf+4, "%d %d", &gpio, &val) == 2)) {
		mtk_pctrl_set_pinmux(hw, gpio, val);
	} else if (!strncmp(buf, "dir", 3)
		&& (sscanf(buf+3, "%d %d", &gpio, &val) == 2)) {
		mtk_pctrl_set_direction(hw, gpio, val);
	} else if (!strncmp(buf, "out", 3)
		&& (sscanf(buf+3, "%d %d", &gpio, &val) == 2)) {
		mtk_pctrl_set_direction(hw, gpio, val);
		/* mtk_gpio_set(chip, gpio, val); */
		mtk_pctrl_set_out(hw, gpio, val);
	} else if (!strncmp(buf, "pullen", 6)
		&& (sscanf(buf+6, "%d %d", &gpio, &val) == 2)) {
		if (gpio < 0 || gpio > hw->soc->npins) {
			pr_notice("invalid pin number\n");
			goto out;
		}
		desc = (const struct mtk_pin_desc *)&hw->soc->pins[gpio];
		mtk_pinconf_bias_get_combo(hw, desc, &pullup, &pullen);
		if (pullen < MTK_PUPD_SET_R1R0_00) {
			pullen = !!val;
		} else {
			if (val < 0)
				val = 0;
			else if (val > 3)
				val = 3;
			pullen = r1r0_en[val];
		}
		mtk_pinconf_bias_set_combo(hw, desc, pullup, pullen);
	} else if ((!strncmp(buf, "pullsel", 7))
		&& (sscanf(buf+7, "%d %d", &gpio, &val) == 2)) {
		if (gpio < 0 || gpio > hw->soc->npins) {
			pr_notice("invalid pin number\n");
			goto out;
		}
		desc = (const struct mtk_pin_desc *)&hw->soc->pins[gpio];
		mtk_pinconf_bias_get_combo(hw, desc, &pullup, &pullen);
		mtk_pinconf_bias_set_combo(hw, desc, !!val, pullen);
	} else if ((!strncmp(buf, "ies", 3))
		&& (sscanf(buf+3, "%d %d", &gpio, &val) == 2)) {
		mtk_pctrl_set_ies(hw, gpio, val);
	} else if ((!strncmp(buf, "smt", 3))
		&& (sscanf(buf+3, "%d %d", &gpio, &val) == 2)) {
		mtk_pctrl_set_smt(hw, gpio, val);
	} else if ((!strncmp(buf, "driving", 7))
		&& (sscanf(buf+7, "%d %d", &gpio, &val) == 2)) {
		if (gpio < 0 || gpio > hw->soc->npins) {
			pr_notice("invalid pin number\n");
			goto out;
		}
		desc = (const struct mtk_pin_desc *)&hw->soc->pins[gpio];
		mtk_pinconf_drive_set_direct_val(hw, desc, val);
	} else if ((!strncmp(buf, "r1r0", 4))
		&& (sscanf(buf+4, "%d %d %d", &gpio, &val, &val2) == 3)) {
		if (gpio < 0 || gpio > hw->soc->npins) {
			pr_notice("invalid pin number\n");
			goto out;
		}
		desc = (const struct mtk_pin_desc *)&hw->soc->pins[gpio];
		mtk_pinconf_bias_get_combo(hw, desc, &pullup, &pullen);
		pullen = r1r0_en[(((!!val) << 1) + !!val2)];
		mtk_pinconf_bias_set_combo(hw, desc, pullup, pullen);
	} else if (!strncmp(buf, "set", 3)) {
		val = sscanf(buf+3, "%d %c%c%c%c%c%c%c%c%c%c %c%c", &gpio,
			&attrs[0], &attrs[1], &attrs[2], &attrs[3],
			&attrs[4], &attrs[5], &attrs[6], &attrs[7],
			&attrs[8], &attrs[9], &attrs[10], &attrs[11]);
		for (i = 0; i < ARRAY_SIZE(attrs); i++) {
			if ((attrs[i] >= '0') && (attrs[i] <= '9'))
				vals[i] = attrs[i] - '0';
			else
				vals[i] = 0;
		}
		if (gpio < 0) {
			pr_notice("invalid pin number\n");
			goto out;
		}
		desc = (const struct mtk_pin_desc *)&hw->soc->pins[gpio];
		/* MODE */
		mtk_pctrl_set_pinmux(hw, gpio, vals[0]);
		/* DIR */
		mtk_pctrl_set_direction(hw, gpio, !!vals[1]);
		/* DOUT */
		if (vals[1])
			/*mtk_gpio_set(chip, gpio, !!vals[2]); */
			mtk_pctrl_set_out(hw, gpio, !!vals[2]);
		/* DRIVING */
		desc = (const struct mtk_pin_desc *)&hw->soc->pins[gpio];
		mtk_pinconf_drive_set_direct_val(hw, desc,
			vals[4]*10 + vals[5]);
		/* SMT */
		mtk_pctrl_set_smt(hw, gpio, vals[6]);
		/* IES */
		mtk_pctrl_set_ies(hw, gpio, vals[7]);
		/* PULL */
		mtk_pinconf_bias_get_combo(hw, desc, &pullup, &pullen);
		if (pullen < MTK_PUPD_SET_R1R0_00) {
			mtk_pinconf_bias_set_combo(hw, desc, !!vals[9],
				!!vals[8]);
		} else {
			pullen = r1r0_en[(((!!vals[10]) << 1) + !!vals[11])];
			mtk_pinconf_bias_set_combo(hw, desc, !!vals[9],
				pullen);
		}
	}

out:
	return count;
}

static DEVICE_ATTR(mt_gpio, 0444, mtk_gpio_show_pin, mtk_gpio_store_pin);

static struct device_attribute *gpio_attr_list[] = {
	&dev_attr_mt_gpio,
};

static int mtk_gpio_create_attr(void)
{
	struct device *dev;
	int idx, err = 0;
	int num = ARRAY_SIZE(gpio_attr_list);
	struct gpio_device *gdev;
	struct gpio_chip *chip = NULL;
	struct mtk_pinctrl *hw = NULL;
	unsigned long flags;

	spin_lock_irqsave(&gpio_lock, flags);
	list_for_each_entry(gdev, &gpio_devices, list) {

		chip = gdev->chip;
		hw = gpiochip_get_data(chip);
		break;
	}

	spin_unlock_irqrestore(&gpio_lock, flags);

	if (!hw || !hw->soc || !hw->dev) {
		pr_notice("invalid gpio chip\n");
		return -EINVAL;
	}
	dev = hw->dev;

	for (idx = 0; idx < num; idx++) {
		err = device_create_file(dev, gpio_attr_list[idx]);
		if (err) {
			pr_notice("[pinctrl]mtk_gpio create attribute error\n");
			break;
		}
	}

	return err;
}

static int __init pinctrl_mtk_debug_v2_init(void)
{
	return mtk_gpio_create_attr();
}

late_initcall(pinctrl_mtk_debug_v2_init);
