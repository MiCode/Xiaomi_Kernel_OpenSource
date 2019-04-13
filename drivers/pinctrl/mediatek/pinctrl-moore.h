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

#ifndef __PINCTRL_MOORE_H
#define __PINCTRL_MOORE_H

#include <linux/io.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinconf-generic.h>

#include "../core.h"
#include "../pinconf.h"
#include "../pinmux.h"
#include "mtk-eint.h"
#include "pinctrl-mtk-common-v2.h"

#define MTK_RANGE(_a)		{ .range = (_a), .nranges = ARRAY_SIZE(_a), }

#define MTK_PIN(_number, _name, _eint_m, _eint_n, _drv_n) {	\
		.number = _number,			\
		.name = _name,				\
		.eint = {				\
			.eint_m = _eint_m,		\
			.eint_n = _eint_n,		\
		},					\
		.drv_n = _drv_n,			\
		.funcs = NULL,				\
	}

#define PINCTRL_PIN_GROUP(name, id)			\
	{						\
		name,					\
		id##_pins,				\
		ARRAY_SIZE(id##_pins),			\
		id##_funcs,				\
	}

int mtk_moore_pinctrl_probe(struct platform_device *pdev,
			    const struct mtk_pin_soc *soc);

#endif /* __PINCTRL_MOORE_H */
