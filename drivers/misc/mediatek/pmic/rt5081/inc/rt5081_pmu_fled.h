 /*
 *  Copyright (C) 2016 Richtek Technology Corp.
 *  Sakya <jeff_chang@richtek.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __LINUX_RT5081_PMU_FLED_H
#define __LINUX_RT5081_PMU_FLED_H


#define RT5081_TORCH_EN_MASK	(0x08)
#define RT5081_TORCH_EN_SHIFT	(3)

#define RT5081_STROBE_EN_MASK	(0x04)
#define RT5081_STROBE_EN_SHIFT	(2)

#define RT5081_FLEDCS_EN_MASK	(0x03)
#define RT5081_FLEDCS1_MASK	(1 << 1)
#define RT5081_FLEDCS2_MASK	(1 << 0)

#define RT5081_FLED_TORCHCUR_MASK (0x1f)
#define RT5081_FLED_TORCHCUR_SHIFT (0)

#define RT5081_FLED_STROBECUR_MASK	(0x7f)
#define RT5081_FLED_STROBECUR_SHIFT	(0)

#define	RT5081_FLED_TIMEOUT_LEVEL_MASK	(0x70)
#define RT5081_TIMEOUT_LEVEL_SHIFT	(4)

#define RT5081_FLED_STROBE_TIMEOUT_MASK		(0x7f)
#define RT5081_FLED_STROBE_TIMEOUT_SHIFT	(0)

#define RT5081_FLED_FIXED_MODE_MASK	(0x40)

#endif /* __LINUX_RT5081_PMU_FLED_H */
