/*
 *  Copyright (C) 2017 MediaTek Inc.
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

#ifndef __LINUX_MT6370_PMU_FLED_H
#define __LINUX_MT6370_PMU_FLED_H


#define MT6370_TORCH_EN_MASK	(0x08)
#define MT6370_TORCH_EN_SHIFT	(3)

#define MT6370_STROBE_EN_MASK	(0x04)
#define MT6370_STROBE_EN_SHIFT	(2)

#define MT6370_FLEDCS_EN_MASK	(0x03)
#define MT6370_FLEDCS1_MASK	(1 << 1)
#define MT6370_FLEDCS2_MASK	(1 << 0)

#define MT6370_FLED_TORCHCUR_MASK (0x1f)
#define MT6370_FLED_TORCHCUR_SHIFT (0)

#define MT6370_FLED_STROBECUR_MASK	(0x7f)
#define MT6370_FLED_STROBECUR_SHIFT	(0)

#define	MT6370_FLED_TIMEOUT_LEVEL_MASK	(0x70)
#define MT6370_TIMEOUT_LEVEL_SHIFT	(4)

#define MT6370_FLED_STROBE_TIMEOUT_MASK		(0x7f)
#define MT6370_FLED_STROBE_TIMEOUT_SHIFT	(0)

#define MT6370_FLED_FIXED_MODE_MASK	(0x40)

#endif /* __LINUX_MT6370_PMU_FLED_H */
