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

#ifndef MTK_POWER_GS_INTERNAL_H
#define MTK_POWER_GS_INTERNAL_H

extern void mt_power_gs_dump_suspend(void);
extern void mt_power_gs_dump_dpidle(void);
extern void mt_power_gs_dump_sodi3(void);
extern void mt_power_gs_t_dump_suspend(int count, ...);
extern void mt_power_gs_t_dump_dpidle(int count, ...);
extern void mt_power_gs_t_dump_sodi3(int count, ...);
extern void mt_power_gs_f_dump_suspend(unsigned int dump_flag);
extern void mt_power_gs_f_dump_dpidle(unsigned int dump_flag);
extern void mt_power_gs_f_dump_sodi3(unsigned int dump_flag);

enum gs_flag {
	GS_PMIC = (0x1 << 0),
	GS_PMIC_6315 = (0x1 << 1),
	GS_CG   = (0x1 << 2),
	GS_DCM  = (0x1 << 3),
	/* GS_ALL will need to be modified, if the gs_dump_flag is changed */
	GS_ALL  = (GS_PMIC | GS_PMIC_6315 | GS_CG | GS_DCM),
};

#define GS_COUNT_PARMS2(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _, ...) _
#define GS_COUNT_PARMS(...) \
	GS_COUNT_PARMS2(__VA_ARGS__, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1)
#define GS_TRANS_PARMS(...) \
	GS_COUNT_PARMS2(__VA_ARGS__, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

#define mt_power_gs_dump_suspend(...) \
	mt_power_gs_t_dump_suspend( \
		GS_TRANS_PARMS( \
			GS_COUNT_PARMS(__VA_ARGS__), ##__VA_ARGS__ \
		), ##__VA_ARGS__)

#define mt_power_gs_dump_dpidle(...) \
	mt_power_gs_t_dump_dpidle( \
		GS_TRANS_PARMS( \
			GS_COUNT_PARMS(__VA_ARGS__), ##__VA_ARGS__ \
		), ##__VA_ARGS__)

#define mt_power_gs_dump_sodi3(...) \
	mt_power_gs_t_dump_sodi3( \
		GS_TRANS_PARMS( \
			GS_COUNT_PARMS(__VA_ARGS__), ##__VA_ARGS__ \
		), ##__VA_ARGS__)

#endif
