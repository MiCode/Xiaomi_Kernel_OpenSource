/*
 * Copyright (C) 2017 MediaTek Inc.
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

#ifndef __HELIO_DVFSRC_OPP_MT6771_H
#define __HELIO_DVFSRC_OPP_MT6771_H

extern int vcore_opp_init(void);
extern unsigned int get_vcore_opp_volt(unsigned int opp);
extern unsigned int get_cur_vcore_opp(void);
extern unsigned int get_cur_ddr_opp(void);
extern unsigned int get_min_opp_for_vcore(int vcore_opp);
extern unsigned int get_min_opp_for_ddr(int ddr_opp);
extern unsigned int
update_vcore_opp_uv(unsigned int opp, unsigned int vcore_uv);

enum ddr_opp {
	DDR_OPP_0 = 0,			/* 3200 MHz */
	DDR_OPP_1,			/* 2400 MHz */
	DDR_OPP_2,			/* 1600 MHz */
	DDR_OPP_NUM,
	DDR_OPP_UNREQ = 15,
};

enum vcore_opp {
	VCORE_OPP_0 = 0,		/* 0.8V */
	VCORE_OPP_1,			/* 0.7V */
	VCORE_OPP_NUM,
	VCORE_OPP_UNREQ = 15,
};

enum vcore_dvfs_opp {
	VCORE_DVFS_OPP_0 = 0,		/* 3200 MHz / 0.8V */
	VCORE_DVFS_OPP_1,		/* 3200 MHz / 0.7V */
	VCORE_DVFS_OPP_2,		/* 2400 MHz / 0.7V */
	VCORE_DVFS_OPP_3,		/* 1600 MHz / 0.7V */
	VCORE_DVFS_OPP_NUM,
	VCORE_DVFS_OPP_UNREQ = 15,
};

#endif /* __HELIO_DVFSRC_MT6771_H */
