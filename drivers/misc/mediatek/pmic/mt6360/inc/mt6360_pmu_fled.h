/*
 *  drivers/misc/mediatek/pmic/mt6360/inc/mt6360_pmu_fled.h
 *
 *  Copyright (C) 2018 Mediatek Technology Corp.
 *  cy_huang <cy_huang@richtek.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __MT6360_PMU_FLED_H
#define __MT6360_PMU_FLED_H

struct mt6360_fled_platform_data {
	u32 fled_vmid_track;
	u32 fled_strb_tout;
	u32 fled1_tor_cur;
	u32 fled1_strb_cur;
	u32 fled2_tor_cur;
	u32 fled2_strb_cur;
};

#define MT6360_FLED_CHG_VINOVP		BIT(3)

#define MT6360_ITOR_SHIFT		0
#define MT6360_ITOR_MASK		0x1F

#define MT6360_ISTRB_SHIFT		0
#define MT6360_ISTRB_MASK		0x7F

#define MT6360_UTRAL_ISTRB_MASK		BIT(7)

#define MT6360_STRB_TO_SHIFT		0
#define MT6360_STRB_TO_MASK		0x7F

#define MT6360_TCL_SHIFT		4
#define MT6360_TCL_MASK			0x70

#define MT6360_FL_TORCH_MASK		BIT(3)
#define MT6360_FL_STROBE_MASK		BIT(2)
#define MT6360_FLCS1_EN_MASK		BIT(1)
#define MT6360_FLCS2_EN_MASK		BIT(0)

#endif /* __MT6360_PMU_FLED_H */
