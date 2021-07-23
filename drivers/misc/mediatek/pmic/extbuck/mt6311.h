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

#ifndef __LINUX_MT6311_H
#define __LINUX_MT6311_H

extern void mt6311_clr_thr_l_int_status(void);
extern void mt6311_clr_thr_h_int_status(void);
extern void mt6311_set_rg_int_en(unsigned char val);
extern unsigned char mt6311_get_rg_thr_l_int_status(void);
extern unsigned char mt6311_get_rg_thr_h_int_status(void);
extern unsigned char mt6311_get_pmu_thr_status(void);
extern void mt6311_set_rg_strup_thr_110_clr(unsigned char val);
extern void mt6311_set_rg_strup_thr_125_clr(unsigned char val);
extern void mt6311_set_rg_strup_thr_110_irq_en(unsigned char val);
extern void mt6311_set_rg_strup_thr_125_irq_en(unsigned char val);
extern void mt6311_set_rg_thrdet_sel(unsigned char val);

#endif /* __LINUX_MT6311_H */
