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

#ifndef __PINCTRL_MTK_COMMON_DEBUG_H
#define __PINCTRL_MTK_COMMON_DEBUG_H

static int mtk_pinctrl_get_gpio_output(struct mtk_pinctrl *pctl, int pin);
static int mtk_pinctrl_set_gpio_output(struct mtk_pinctrl *pctl,
	int pin, int value);
static int mtk_pinctrl_get_gpio_input(struct mtk_pinctrl *pctl, int pin);
static int mtk_pinctrl_get_gpio_direction(struct mtk_pinctrl *pctl, int pin);
static int mtk_pinctrl_set_gpio_direction(struct mtk_pinctrl *pctl,
	int pin, bool input);
static int mtk_pinctrl_get_gpio_mode(struct mtk_pinctrl *pctl, int pin);
static int mtk_pinctrl_set_gpio_mode(struct mtk_pinctrl *pctl,
	int pin, unsigned long mode);
static int mtk_pinctrl_get_gpio_driving(struct mtk_pinctrl *pctl, int pin);
static int mtk_pinctrl_set_gpio_driving(struct mtk_pinctrl *pctl,
	int pin, unsigned char driving);
static int mtk_pinctrl_get_gpio_smt(struct mtk_pinctrl *pctl, int pin);
static int mtk_pinctrl_set_gpio_smt(struct mtk_pinctrl *pctl,
	int pin, bool enable);
static int mtk_pinctrl_get_gpio_ies(struct mtk_pinctrl *pctl, int pin);
static int mtk_pinctrl_set_gpio_ies(struct mtk_pinctrl *pctl,
	int pin, bool enable);
static int mtk_pinmux_get(struct gpio_chip *chip, unsigned int offset);
static int mtk_gpio_get_in(struct gpio_chip *chip, unsigned int offset);
static int mtk_gpio_get_out(struct gpio_chip *chip, unsigned int offset);
static int mtk_pullen_get(struct gpio_chip *chip, unsigned int offset);
static int mtk_pullsel_get(struct gpio_chip *chip, unsigned int offset);
static int mtk_ies_get(struct gpio_chip *chip, unsigned int offset);
static int mtk_smt_get(struct gpio_chip *chip, unsigned int offset);
static int mtk_driving_get(struct gpio_chip *chip, unsigned int offset);
static ssize_t mtk_gpio_show_pin(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t mtk_gpio_store_pin(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count);
static int mtk_gpio_create_attr(struct device *dev);

#endif
