/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
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
