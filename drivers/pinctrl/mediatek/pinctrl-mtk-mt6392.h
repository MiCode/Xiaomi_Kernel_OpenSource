/*
* Copyright (C) 2016 MediaTek Inc.
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

#ifndef __PINCTRL_MTK_MT6392_H
#define __PINCTRL_MTK_MT6392_H

#include <linux/pinctrl/pinctrl.h>
#include "pinctrl-mtk-common.h"

static const struct mtk_desc_pin mtk_pins_mt6392[] = {
	MTK_PIN(
		PINCTRL_PIN(0, "INT"),
		NULL, "mt6392",
		MTK_EINT_FUNCTION(NO_EINT_SUPPORT, NO_EINT_SUPPORT),
		MTK_FUNCTION(0, "GPIO0"),
		MTK_FUNCTION(1, "INT"),
		MTK_FUNCTION(5, "TEST_CK2"),
		MTK_FUNCTION(6, "TEST_IN1"),
		MTK_FUNCTION(7, "TEST_OUT1")
	),
	MTK_PIN(
		PINCTRL_PIN(1, "SRCLKEN"),
		NULL, "mt6392",
		MTK_EINT_FUNCTION(NO_EINT_SUPPORT, NO_EINT_SUPPORT),
		MTK_FUNCTION(0, "GPIO1"),
		MTK_FUNCTION(1, "SRCLKEN"),
		MTK_FUNCTION(5, "TEST_CK0"),
		MTK_FUNCTION(6, "TEST_IN2"),
		MTK_FUNCTION(7, "TEST_OUT2")
	),
	MTK_PIN(
		PINCTRL_PIN(2, "RTC_32K1V8"),
		NULL, "mt6392",
		MTK_EINT_FUNCTION(NO_EINT_SUPPORT, NO_EINT_SUPPORT),
		MTK_FUNCTION(0, "GPIO2"),
		MTK_FUNCTION(1, "RTC_32K1V8"),
		MTK_FUNCTION(5, "TEST_CK1"),
		MTK_FUNCTION(6, "TEST_IN3"),
		MTK_FUNCTION(7, "TEST_OUT3")
	),
	MTK_PIN(
		PINCTRL_PIN(3, "SPI_CLK"),
		NULL, "mt6392",
		MTK_EINT_FUNCTION(NO_EINT_SUPPORT, NO_EINT_SUPPORT),
		MTK_FUNCTION(0, "GPIO3"),
		MTK_FUNCTION(1, "SPI_CLK")
	),
	MTK_PIN(
		PINCTRL_PIN(4, "SPI_CSN"),
		NULL, "mt6392",
		MTK_EINT_FUNCTION(NO_EINT_SUPPORT, NO_EINT_SUPPORT),
		MTK_FUNCTION(0, "GPIO4"),
		MTK_FUNCTION(1, "SPI_CSN")
	),
	MTK_PIN(
		PINCTRL_PIN(5, "SPI_MOSI"),
		NULL, "mt6392",
		MTK_EINT_FUNCTION(NO_EINT_SUPPORT, NO_EINT_SUPPORT),
		MTK_FUNCTION(0, "GPIO5"),
		MTK_FUNCTION(1, "SPI_MOSI")
	),
	MTK_PIN(
		PINCTRL_PIN(6, "SPI_MISO"),
		NULL, "mt6392",
		MTK_EINT_FUNCTION(NO_EINT_SUPPORT, NO_EINT_SUPPORT),
		MTK_FUNCTION(0, "GPIO6"),
		MTK_FUNCTION(1, "SPI_MISO"),
		MTK_FUNCTION(6, "TEST_IN4"),
		MTK_FUNCTION(7, "TEST_OUT4")
	),
};

#endif /* __PINCTRL_MTK_MT6392_H */
