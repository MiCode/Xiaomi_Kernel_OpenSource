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

#if defined(CONFIG_MACH_MT6775)
#include <helio-dvfsrc-opp-mt6775.h>
#elif defined(CONFIG_MACH_MT6771)
#include <helio-dvfsrc-opp-mt6771.h>
#elif defined(CONFIG_MACH_MT6768)
#include <helio-dvfsrc-opp-mt6768.h>
#elif defined(CONFIG_MACH_MT6765)
#include <helio-dvfsrc-opp-mt6765.h>
#elif defined(CONFIG_MACH_MT6785)
	#if defined(CONFIG_MTK_DVFSRC_MT6781_PRETEST)
	#include <helio-dvfsrc-opp-v6781.h>
	#else
	#include <helio-dvfsrc-opp-mt6785.h>
	#endif
#elif defined(CONFIG_MACH_MT6885)
#include <helio-dvfsrc-opp-mt6885.h>
#elif defined(CONFIG_MACH_MT6873)
#include <helio-dvfsrc-opp-mt6873.h>
#elif defined(CONFIG_MACH_MT6853)
	#if defined(CONFIG_MTK_DVFSRC_MT6833_PRETEST)
	#include <helio-dvfsrc-opp-mt6833.h>
	#elif defined(CONFIG_MTK_DVFSRC_MT6877_PRETEST)
	#include <helio-dvfsrc-opp-v6877.h>
	#else
	#include <helio-dvfsrc-opp-mt6853.h>
	#endif
#elif defined(CONFIG_MACH_MT6893)
#include <helio-dvfsrc-opp-mt6893.h>
#elif defined(CONFIG_MACH_MT6833)
#include <helio-dvfsrc-opp-mt6833.h>
#elif defined(CONFIG_MACH_MT6877)
#include <helio-dvfsrc-opp-v6877.h>
#elif defined(CONFIG_MACH_MT6781)
#include <helio-dvfsrc-opp-v6781.h>
#else
#include <helio-dvfsrc-opp-mt67xx.h>
#endif

#if defined(CONFIG_MACH_MT6768) || defined(CONFIG_MACH_MT6785) \
	|| defined(CONFIG_MACH_MT6885) || defined(CONFIG_MACH_MT6765) \
	|| defined(CONFIG_MACH_MT6873) || defined(CONFIG_MACH_MT6853) \
	|| defined(CONFIG_MACH_MT6893) || defined(CONFIG_MACH_MT6833) \
	|| defined(CONFIG_MACH_MT6877) || defined(CONFIG_MACH_MT6781)
struct opp_profile {
	int vcore_uv;
	int ddr_khz;
};

extern int get_cur_vcore_dvfs_opp(void);
extern void set_opp_table(unsigned int vcore_dvfs_opp, int vcore_uv,
			  int ddr_khz);

extern int get_vcore_opp(unsigned int opp);
extern int get_vcore_uv(unsigned int opp);
extern int get_cur_vcore_opp(void);
extern int get_cur_vcore_uv(void);
extern void set_vcore_opp(unsigned int vcore_dvfs_opp, int vcore_opp);

extern int get_ddr_opp(unsigned int opp);
extern int get_ddr_khz(unsigned int opp);
extern int get_cur_ddr_opp(void);
extern int get_cur_ddr_khz(void);
extern void set_ddr_opp(unsigned int vcore_dvfs_opp, int ddr_opp);

extern void set_vcore_uv_table(unsigned int vcore_opp, int vcore_uv);
extern int get_vcore_uv_table(unsigned int vcore_opp);

extern void set_pwrap_cmd(unsigned int vcore_opp, int pwrap_cmd);
extern int get_pwrap_cmd(unsigned int vcore_opp);
extern int get_opp_ddr_freq(unsigned int ddr_opp);
extern void set_opp_ddr_freq(unsigned int ddr_opp, int ddr_freq);
#endif

#endif /* __HELIO_DVFSRC_OPP_H */

