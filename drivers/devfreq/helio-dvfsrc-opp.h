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

#ifndef __HELIO_DVFSRC_OPP_H
#define __HELIO_DVFSRC_OPP_H

#if defined(CONFIG_MACH_MT6765)
#include <helio-dvfsrc-opp-mt6765.h>
#else
#include <helio-dvfsrc-opp-mt67xx.h>
#endif

struct opp_profile {
	int vcore_uv;
	int ddr_khz;
};

extern int get_cur_vcore_dvfs_opp(void);
extern void set_opp_table(int vcore_dvfs_opp, int vcore_uv, int ddr_khz);

extern int get_vcore_opp(int opp);
extern int get_vcore_uv(int opp);
extern int get_cur_vcore_opp(void);
extern int get_cur_vcore_uv(void);
extern void set_vcore_opp(int vcore_dvfs_opp, int vcore_opp);

extern int get_ddr_opp(int opp);
extern int get_ddr_khz(int opp);
extern int get_cur_ddr_opp(void);
extern int get_cur_ddr_khz(void);
extern void set_ddr_opp(int vcore_dvfs_opp, int ddr_opp);

extern void set_vcore_uv_table(int vcore_opp, int vcore_uv);
extern int get_vcore_uv_table(int vcore_opp);

extern void set_pwrap_cmd(int vcore_opp, int pwrap_cmd);
extern int get_pwrap_cmd(int vcore_opp);
#endif /* __HELIO_DVFSRC_OPP_H */

